// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"

namespace leveldb
{

// See doc/table_format.txt for an explanation of the filter block format.

// Generate new filter every 2KB of data
static const size_t kFilterBaseLg = 11;
static const size_t kFilterBase = 1 << kFilterBaseLg;

FilterBlockBuilder::FilterBlockBuilder(const FilterPolicy* policy) :
	policy_(policy)
{
}

void FilterBlockBuilder::StartBlock(uint64_t block_offset)
{
	uint64_t filter_index = (block_offset / kFilterBase);
	assert(filter_index >= filter_offsets_.size());
	// TODO： 死循环 ？
	while (filter_index > filter_offsets_.size())
	{
		GenerateFilter();
	}
}

// 将 key 加入到 start_ , keys_ 中去
void FilterBlockBuilder::AddKey(const Slice& key)
{
	Slice k = key;
	start_.push_back(keys_.size());
	keys_.append(k.data(), k.size()); //记录的是key的结束位置
}

// 依次存放的为：
// | filter1 | filter2 | ... | filter1 的起始位置 | filter 2 的起始位置 | ...
// | 过滤器总数(4字节)|  kFilterBaseLg(1字节) |
Slice FilterBlockBuilder::Finish()
{
	// 还有key，没有生成过滤器
	if ( !start_.empty() )
	{
		GenerateFilter();
	}

	// Append array of per-filter offsets
	const uint32_t array_offset = result_.size(); //所有过滤器的长度
	for (size_t i = 0; i < filter_offsets_.size(); i++)
	{
		PutFixed32(&result_, filter_offsets_[i]); // 存放每个过滤器在 @result_中的位置
	}

	PutFixed32(&result_, array_offset); //过滤器总数
	result_.push_back(kFilterBaseLg); // Save encoding parameter in result
	return Slice(result_);
}

// 根据现有的key，将生成的过滤器放在 @result_ 中.
// 每个过滤器的起始位置，存放在 @filter_offsets_ 中
// 做完之后，清空 @keys_, @start_, @tmp_keys_

void FilterBlockBuilder::GenerateFilter()
{
	const size_t num_keys = start_.size(); // key 的个数
	if ( num_keys == 0 )
	{
		// Fast path if there are no keys for this filter
		filter_offsets_.push_back(result_.size());
		return;
	}

	// Make list of keys from flattened key structure
	start_.push_back(keys_.size()); // Simplify length computation
	tmp_keys_.resize(num_keys);
	// 临时计算出所有的key.
	for (size_t i = 0; i < num_keys; i++)
	{
		const char* base = keys_.data() + start_[i];
		size_t length = start_[i + 1] - start_[i];
		tmp_keys_[i] = Slice(base, length);
	}

	// Generate filter for current set of keys and append to result_.
	filter_offsets_.push_back(result_.size());
	policy_->CreateFilter(&tmp_keys_[0], num_keys, &result_); //生成过滤器

	tmp_keys_.clear();
	keys_.clear();
	start_.clear();
}

FilterBlockReader::FilterBlockReader(const FilterPolicy* policy,
		const Slice& contents) :
	policy_(policy), data_(NULL), offset_(NULL), num_(0), base_lg_(0)
{
	size_t n = contents.size();
	if ( n < 5 )
		return; // 1 byte for base_lg_ and 4 for start of offset array
	base_lg_ = contents[n - 1];
	uint32_t last_word = DecodeFixed32(contents.data() + n - 5); //最后一个filter的结束位置
	if ( last_word > n - 5 )
		return;
	data_ = contents.data();
	offset_ = data_ + last_word;
	num_ = (n - 5 - last_word) / 4;
}

bool FilterBlockReader::KeyMayMatch(uint64_t block_offset, const Slice& key)
{
	uint64_t index = block_offset >> base_lg_;
	if ( index < num_ )
	{
		uint32_t start = DecodeFixed32(offset_ + index * 4);
		uint32_t limit = DecodeFixed32(offset_ + index * 4 + 4);
		if ( start <= limit && limit <= (offset_ - data_) )
		{
			Slice filter = Slice(data_ + start, limit - start); //得到过滤器
			return policy_->KeyMayMatch(key, filter);
		}
		else if ( start == limit )
		{
			// Empty filters do not match any keys
			return false;
		}
	}
	return true; // Errors are treated as potential matches
}

}
