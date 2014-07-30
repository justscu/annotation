// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <cstddef>
#include <vector>
#include <assert.h>
#include <stdint.h>

namespace leveldb
{

/*
 * levelDB的简单内存池；给memtable使用。
 * 一个memtable使用一个Arena，当memtable被释放时，由Arena统一释放其内存
 * */
class Arena
{
	public:
		Arena();
		~Arena();

		// Return a pointer to a newly allocated memory block of "bytes" bytes.
		char* Allocate(size_t bytes);

		// Allocate memory with the normal alignment guarantees provided by malloc
		char* AllocateAligned(size_t bytes);

		// Returns an estimate of the total memory usage of data allocated
		// by the arena (including space allocated but not yet used for user
		// allocations).
		size_t MemoryUsage() const
		{
			return blocks_memory_ + blocks_.capacity() * sizeof(char*);
		}

	private:
		char* AllocateFallback(size_t bytes); //重新分配一块内存.
		char* AllocateNewBlock(size_t block_bytes);

		// Allocation state
		char* alloc_ptr_; //剩下的，内存池中没有分配的 起始地址
		size_t alloc_bytes_remaining_; //剩下的，内存池中没有分配的大小

		// Array of new[] allocated memory blocks
		std::vector<char*> blocks_; //里面放的是 指向已经分配的内存的指针

		// Bytes of memory in blocks allocated so far
		size_t blocks_memory_; //到目前为止，分配的内存的大小

		// No copying allowed
		Arena(const Arena&);
		void operator=(const Arena&);
};

inline char* Arena::Allocate(size_t bytes)
{
	// The semantics of what to return are a bit messy if we allow
	// 0-byte allocations, so we disallow them here (we don't need
	// them for our internal use).
	assert(bytes > 0);

	//需要分配的内存，小于内存池中还没有分配出去的内存。
	if ( bytes <= alloc_bytes_remaining_ )
	{
		char* result = alloc_ptr_;
		alloc_ptr_ += bytes;
		alloc_bytes_remaining_ -= bytes;
		return result;
	}
	return AllocateFallback(bytes);
}

} // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_
