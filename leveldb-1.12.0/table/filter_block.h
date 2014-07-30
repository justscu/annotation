// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A filter block is stored near the end of a Table file.  It contains
// filters (e.g., bloom filters) for all data blocks in the table combined
// into a single filter block.

#ifndef STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "leveldb/slice.h"
#include "util/hash.h"

namespace leveldb
{

class FilterPolicy;

// A FilterBlockBuilder is used to construct all of the filters for a
// particular Table.  It generates a single string which is stored as
// a special block in the Table.
//
// The sequence of calls to FilterBlockBuilder must match the regexp:
//      (StartBlock AddKey*)* Finish
// 写 filter-block.
class FilterBlockBuilder
{
	public:
		explicit FilterBlockBuilder(const FilterPolicy*);

		void StartBlock(uint64_t block_offset);
		void AddKey(const Slice& key);
		Slice Finish();

	private:
		void GenerateFilter();

		const FilterPolicy* policy_;
		std::string keys_; // Flattened key contents // 依次存放每个key，是扁平按序存放的
		// 依次存放每个key，在keys_中的起始位置.
		std::vector<size_t> start_; // Starting index in keys_ of each key
		// 依次存放每次生成的过滤器
		std::string result_; // Filter data computed so far
		std::vector<Slice> tmp_keys_; // policy_->CreateFilter() argument // 里面依次存放的是每个key
		// 依次存放每个过滤器在 @result_ 中的位置
		std::vector<uint32_t> filter_offsets_;

		// No copying allowed
		FilterBlockBuilder(const FilterBlockBuilder&);
		void operator=(const FilterBlockBuilder&);
};

// 读 filter-block.
class FilterBlockReader
{
	public:
		// REQUIRES: "contents" and *policy must stay live while *this is live.
		FilterBlockReader(const FilterPolicy* policy, const Slice& contents);
		bool KeyMayMatch(uint64_t block_offset, const Slice& key);

	private:
		const FilterPolicy* policy_;
		const char* data_; // Pointer to filter data (at block-start)
		// 第一个filter 的偏移在data_中的起始位置
		const char* offset_; // Pointer to beginning of offset array (at block-end)
		// filter的个数
		size_t num_; // Number of entries in offset array
		size_t base_lg_; // Encoding parameter (see kFilterBaseLg in .cc file)
};

}

#endif  // STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
