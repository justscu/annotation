// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_

#include <vector>

#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb
{

struct Options;

/*
// block的save类，与 class Block对应
// 有个string buffer_，用来存放k-v，按照deta来存
// block的构建
 * ----------------------------------------------------------------------
 * | k-v| k-v| ...| k-v | restart-points (1-n) | sizeof(restart-points) |
 * ----------------------------------------------------------------------
****************************/
class BlockBuilder
{
	public:
		explicit BlockBuilder(const Options* options);

		// Reset the contents as if the BlockBuilder was just constructed.
		void Reset();

		// REQUIRES: Finish() has not been callled since the last call to Reset().
		// REQUIRES: key is larger than any previously added key
		void Add(const Slice& key, const Slice& value);

		// Finish building the block and return a slice that refers to the
		// block contents.  The returned slice will remain valid for the
		// lifetime of this builder or until Reset() is called.
		Slice Finish();

		// Returns an estimate of the current (uncompressed) size of the block
		// we are building.
		size_t CurrentSizeEstimate() const;

		// Return true iff no entries have been added since the last Reset()
		bool empty() const
		{
			return buffer_.empty();
		}

	private:
		const Options* options_;
		std::string buffer_; // Destination buffer，所有的k-v信息，都存入到buffer_中
		std::vector<uint32_t> restarts_; // Restart points，指出了每个"起始点"在buffer_中的起始位置
		int counter_; // Number of entries emitted since restart，指出了buffer_中key的个数
		bool finished_; // Has Finish() been called?
		std::string last_key_; //上次加入的key

		// No copying allowed
		BlockBuilder(const BlockBuilder&);
		void operator=(const BlockBuilder&);
};

} // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
