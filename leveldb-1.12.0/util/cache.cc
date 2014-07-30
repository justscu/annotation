// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "leveldb/cache.h"
#include "port/port.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb
{

Cache::~Cache()
{
}

namespace
{

// LRU cache implementation

// An entry is a variable length heap-allocated structure.  Entries
// are kept in a circular doubly linked list ordered by access time.
//
// LRU是Least Recently Used 近期最少使用算法。
// 定义一个LRUHandle结构体，代表cache中的元素
struct LRUHandle
{
		void* value; // 这个存储的是cache的数据
		void (*deleter)(const Slice&, void* value); // 数据从Cache中清除时执行的清理函数；
		// @next_hash: 指向节点在hash table链表中的下一个hash(key)相同的元素,
		// 碰撞时Leveldb采用的是链表法。最后一个节点的next_hash为NULL
		LRUHandle* next_hash;
		// @next, @prev: 节点在双向链表中的前驱 后继节点指针
		// 前面的是最新加入的，每次新加入的位置都是head->next; 每次剔除的规则就是剔除list tail
		LRUHandle* next;
		LRUHandle* prev;

		size_t charge; // TODO(opt): Only allow uint32_t?
		size_t key_length;
		uint32_t refs;
		uint32_t hash; // Hash of key(); used for fast sharding and comparisons，hash值
		char key_data[1]; // Beginning of key

		Slice key() const
		{
			// For cheaper lookups, we allow a temporary Handle object
			// to store a pointer to a key in "value".
			if (next == this)
			{
				return *(reinterpret_cast<Slice*> (value));
			} else
			{
				return Slice(key_data, key_length);
			}
		}
};

// We provide our own simple hash table since it removes a whole bunch
// of porting hacks and is also faster than some of the built-in hash
// table implementations in some of the compiler/runtime combinations
// we have tested.  E.g., readrandom speeds up by ~5% over the g++
// 4.4.3's builtin hashtable.
//
// levelDB实现的一个hashTable.
// HandleTable使用LRUHandle **list_存储所有的hash节点，
// 其实就是一个二维数组，一维是不同的hash(key)，另一维则是相同hash(key)的碰撞list。
class HandleTable
{
	public:
		HandleTable() :
			length_(0), elems_(0), list_(NULL)
		{
			Resize();
		}
		~HandleTable()
		{
			delete[] list_;
		}

		LRUHandle* Lookup(const Slice& key, uint32_t hash)
		{
			return *FindPointer(key, hash);
		}

		LRUHandle* Insert(LRUHandle* h)
		{
			LRUHandle** ptr = FindPointer(h->key(), h->hash);
			LRUHandle* old = *ptr;
			h->next_hash = (old == NULL ? NULL : old->next_hash);
			*ptr = h; //将h节点加入
			if (old == NULL) //没有找到
			{
				++elems_;
				if (elems_ > length_)
				{
					// Since each cache entry is fairly large, we aim for a small
					// average linked list length (<= 1).
					Resize();
				}
			}
			return old;
		}

		LRUHandle* Remove(const Slice& key, uint32_t hash)
		{
			LRUHandle** ptr = FindPointer(key, hash);
			LRUHandle* result = *ptr;
			if (result != NULL)
			{
				*ptr = result->next_hash;
				--elems_;
			}
			return result;
		}

	private:
		// The table consists of an array of buckets(水桶) where each bucket is
		// a linked list of cache entries that hash into the bucket.
		uint32_t length_; // 一维的长度
		uint32_t elems_;  // 整个 数组+链表 的元素个数(LRUHandle)
		LRUHandle** list_; // 指向一维数组的 首地址

		// Return a pointer to slot that points to a cache entry that
		// matches key/hash.  If there is no such cache entry, return a
		// pointer to the trailing slot in the corresponding linked list.
		//
		// 找到符合条件的第一个key (key和hash值都相等)
		// @return, 要么返回该key的LRUHandle，要么返回NULL(没有找到).
		LRUHandle** FindPointer(const Slice& key, uint32_t hash)
		{
			LRUHandle** ptr = &list_[hash & (length_ - 1)];
			while (*ptr != NULL && ((*ptr)->hash != hash || key
					!= (*ptr)->key()))
			{
				ptr = &(*ptr)->next_hash;  // -> 的优先级 比 & 高
			}
			return ptr;
		}

		// 每次当hash节点数超过当前一维数组的长度后，都会做Resize操作：
		void Resize()
		{
			uint32_t new_length = 4;
			while (new_length < elems_)
			{
				new_length *= 2;
			}
			LRUHandle** new_list = new LRUHandle*[new_length]; // 开辟一块指针数组 (LRUHandle*)
			memset(new_list, 0, sizeof(new_list[0]) * new_length);
			uint32_t count = 0;
			for (uint32_t i = 0; i < length_; i++)
			{
				LRUHandle* h = list_[i];
				while (h != NULL)
				{
					LRUHandle* next = h->next_hash; //hash结果相同的，hash链表
					uint32_t hash = h->hash;
					LRUHandle** ptr = &new_list[hash & (new_length - 1)]; // 在新的一维数组中，找到位置
					h->next_hash = *ptr;
					*ptr = h;
					h = next;
					count++;
				}
			}
			assert(elems_ == count);
			delete[] list_; //删除旧的 一维表格
			list_ = new_list;
			length_ = new_length;
		}
};

// A single shard of sharded cache.
// 封装了 LRUHandle & HandleTable.
// 基于HandleTable和LRUHandle，实现了一个标准的LRUcache，并内置了mutex保护锁，是线程安全的。
class LRUCache
{
	public:
		LRUCache();
		~LRUCache();

		// Separate from constructor so caller can easily make an array of LRUCache
		void SetCapacity(size_t capacity)
		{
			capacity_ = capacity;
		}

		// Like Cache methods, but with an extra "hash" parameter.
		Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
				size_t charge, void(*deleter)(const Slice& key, void* value));
		Cache::Handle* Lookup(const Slice& key, uint32_t hash);
		void Release(Cache::Handle* handle);
		void Erase(const Slice& key, uint32_t hash);

	private:
		void LRU_Remove(LRUHandle* e);
		void LRU_Append(LRUHandle* e);
		void Unref(LRUHandle* e);

		// Initialized before use.
		size_t capacity_;

		// mutex_ protects the following state.
		port::Mutex mutex_;
		size_t usage_;

		// Dummy head of LRU list.
		// lru.prev is newest entry, lru.next is oldest entry.
		LRUHandle lru_; //prev是最新的，next是最旧的

		HandleTable table_;
};

LRUCache::LRUCache() :
	usage_(0)
{
	// Make empty circular linked list
	lru_.next = &lru_;
	lru_.prev = &lru_;
}

LRUCache::~LRUCache()
{
	for (LRUHandle* e = lru_.next; e != &lru_;)
	{
		LRUHandle* next = e->next;
		assert(e->refs == 1); // Error if caller has an unreleased handle
		Unref(e);
		e = next;
	}
}

void LRUCache::Unref(LRUHandle* e)
{
	assert(e->refs > 0);
	e->refs--;
	if (e->refs <= 0)
	{
		usage_ -= e->charge;
		(*e->deleter)(e->key(), e->value);
		free(e);
	}
}

void LRUCache::LRU_Remove(LRUHandle* e)
{
	e->next->prev = e->prev;
	e->prev->next = e->next;
}

// 将最新的e 插入在lru_前面
void LRUCache::LRU_Append(LRUHandle* e)
{
	// Make "e" newest entry by inserting just before lru_
	e->next = &lru_;
	e->prev = lru_.prev;
	e->prev->next = e;
	e->next->prev = e;
}

// lru_.prev是最新被访问的条目，lru_.next是最老被访问的条目。
// 在访问cache中的一个数据时，会顺次执行LRU_Remove和LRU_Append函数
Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash)
{
	MutexLock l(&mutex_);
	LRUHandle* e = table_.Lookup(key, hash);
	if (e != NULL)
	{
		e->refs++;
		LRU_Remove(e);
		LRU_Append(e);
	}
	return reinterpret_cast<Cache::Handle*> (e);
}

void LRUCache::Release(Cache::Handle* handle)
{
	MutexLock l(&mutex_);
	Unref(reinterpret_cast<LRUHandle*> (handle));
}

Cache::Handle* LRUCache::Insert(const Slice& key, uint32_t hash, void* value,
		size_t charge, void(*deleter)(const Slice& key, void* value))
{
	MutexLock l(&mutex_);

	LRUHandle* e = reinterpret_cast<LRUHandle*> (malloc(sizeof(LRUHandle) - 1
			+ key.size()));
	e->value = value;
	e->deleter = deleter;
	e->charge = charge;
	e->key_length = key.size();
	e->hash = hash;
	e->refs = 2; // One from LRUCache, one for the returned handle
	memcpy(e->key_data, key.data(), key.size());
	LRU_Append(e);
	usage_ += charge;

	LRUHandle* old = table_.Insert(e); //将新的数据插入
	// 若原来存在k-v，将老数据删除掉
	if (old != NULL)
	{
		LRU_Remove(old);
		Unref(old);
	}

	// 会根据 capacity_大小，将老的条目删掉
	while (usage_ > capacity_ && lru_.next != &lru_)
	{
		LRUHandle* old = lru_.next;
		LRU_Remove(old);
		table_.Remove(old->key(), old->hash);
		Unref(old);
	}

	return reinterpret_cast<Cache::Handle*> (e);
}

void LRUCache::Erase(const Slice& key, uint32_t hash)
{
	MutexLock l(&mutex_);
	LRUHandle* e = table_.Remove(key, hash);
	if (e != NULL)
	{
		LRU_Remove(e);
		Unref(e);
	}
}

static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;

/*
 * 为了多线程访问，尽可能快速，减少锁开销，ShardedLRUCache内部有16个LRUCache，
 * 查找Key时首先计算key属于哪一个分片，分片的计算方法是取32位hash值的高4位，
 * 然后在相应的LRUCache中进行查找，这样就大大减少了多线程的访问锁的开销
 *
 * */
class ShardedLRUCache: public Cache
{
	private:
		LRUCache shard_[kNumShards]; // shard_[16];
		port::Mutex id_mutex_;
		uint64_t last_id_;

		static inline uint32_t HashSlice(const Slice& s)
		{
			return Hash(s.data(), s.size(), 0);
		}
		// 取hash值的高4位
		static uint32_t Shard(uint32_t hash)
		{
			return hash >> (32 - kNumShardBits);
		}

	public:
		explicit ShardedLRUCache(size_t capacity) :
			last_id_(0)
		{
			const size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
			for (int s = 0; s < kNumShards; s++)
			{
				shard_[s].SetCapacity(per_shard);
			}
		}
		virtual ~ShardedLRUCache()
		{
		}
		virtual Handle* Insert(const Slice& key, void* value, size_t charge,
				void(*deleter)(const Slice& key, void* value))
		{
			const uint32_t hash = HashSlice(key);
			return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
		}
		virtual Handle* Lookup(const Slice& key)
		{
			const uint32_t hash = HashSlice(key);
			return shard_[Shard(hash)].Lookup(key, hash);
		}
		virtual void Release(Handle* handle)
		{
			LRUHandle* h = reinterpret_cast<LRUHandle*> (handle);
			shard_[Shard(h->hash)].Release(handle);
		}
		virtual void Erase(const Slice& key)
		{
			const uint32_t hash = HashSlice(key);
			shard_[Shard(hash)].Erase(key, hash);
		}
		virtual void* Value(Handle* handle)
		{
			return reinterpret_cast<LRUHandle*> (handle)->value;
		}
		virtual uint64_t NewId()
		{
			MutexLock l(&id_mutex_);
			return ++(last_id_);
		}
};

} // end anonymous namespace

Cache* NewLRUCache(size_t capacity)
{
	return new ShardedLRUCache(capacity);
}

} // namespace leveldb
