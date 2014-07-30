// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_H_

#include <stddef.h>
#include <stdint.h>
#include "leveldb/iterator.h"

namespace leveldb
{

struct BlockContents;
class Comparator;

// 在block中，存放了一些 重启点(restart)
// 用来读block 的 k-v
//
// block的读取类，与class BlockBuilder相对应
//
class Block
{
	public:
		// Initialize the block with the specified contents.
		explicit Block(const BlockContents& contents);

		~Block();

		size_t size() const
		{
			return size_;
		}
		Iterator* NewIterator(const Comparator* comparator);

	private:
		uint32_t NumRestarts() const;

		const char* data_; //指向block的数据: 里面依次存放的是k-v | 重启点的位置 | 重启点的个数
		size_t size_; // block的大小
		// 重启点数组在data_中的偏移
		uint32_t restart_offset_; // Offset in data_ of restart array
		bool owned_; // Block owns data_[]；//true，需要自己释放

		// No copying allowed
		Block(const Block&);
		void operator=(const Block&);

		class Iter;
};

} // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_H_
