// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// BlockBuilder generates blocks where keys are prefix-compressed:
//
// When we store a key, we drop the prefix shared with the previous
// string.  This helps reduce the space requirement significantly.
// Furthermore, once every K keys, we do not apply the prefix
// compression and store the entire key.  We call this a "restart
// point".  The tail end of the block stores the offsets of all of the
// restart points, and can be used to do a binary search when looking
// for a particular key.  Values are stored as-is (without compression)
// immediately following the corresponding key.
//
// An entry for a particular key-value pair has the form:
//     shared_bytes: varint32
//     unshared_bytes: varint32
//     value_length: varint32
//     key_delta: char[unshared_bytes]
//     value: char[value_length]
// shared_bytes == 0 for restart points.
//
// The trailer of the block has the form:
//     restarts: uint32[num_restarts]
//     num_restarts: uint32
// restarts[i] contains the offset within the block of the ith restart point.

#include "table/block_builder.h"

#include <algorithm>
#include <assert.h>
#include "leveldb/comparator.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"

namespace leveldb
{

BlockBuilder::BlockBuilder(const Options* options) :
	options_(options), restarts_(), counter_(0), finished_(false)
{
	assert(options->block_restart_interval >= 1);
	// 第一个的key的起始位置为0.
	restarts_.push_back(0); // First restart point is at offset 0
}

void BlockBuilder::Reset()
{
	buffer_.clear();
	restarts_.clear();
	restarts_.push_back(0); // First restart point is at offset 0
	counter_ = 0;
	finished_ = false;
	last_key_.clear();
}

size_t BlockBuilder::CurrentSizeEstimate() const
{
	return (buffer_.size() + // Raw data buffer
			restarts_.size() * sizeof(uint32_t) + // Restart array
			sizeof(uint32_t)); // Restart array length
}

// 将 vector<uint32_t> restart_编码后，放入到buffer_ 中
Slice BlockBuilder::Finish()
{
	// Append restart array
	for (size_t i = 0; i < restarts_.size(); i++)
	{
		PutFixed32(&buffer_, restarts_[i]); //压入每个重启点的位置
	}
	PutFixed32(&buffer_, restarts_.size()); //压入重启点的个数(最后一个int)
	finished_ = true;
	return Slice(buffer_);
}

// add k-v
// 新加入的key，要与上次加入的key比较。只存有差异的部分
void BlockBuilder::Add(const Slice& key, const Slice& value)
{
	Slice last_key_piece(last_key_);
	assert(!finished_);
	assert(counter_ <= options_->block_restart_interval);
	assert(buffer_.empty() // No values yet?
			|| options_->comparator->Compare(key, last_key_piece) > 0); //新加入的key>已加入的key
	size_t shared = 0;
	//前缀压缩存储
	if ( counter_ < options_->block_restart_interval )
	{
		// See how much sharing to do with previous string
		// 看看本次加入的key，与上次加入的key(从重启点开始计算)，有多少重复的
		const size_t min_length = std::min(last_key_piece.size(), key.size());
		// 找出相同部分
		while ((shared < min_length) && (last_key_piece[shared] == key[shared]))
		{
			shared++;
		}
	}
	//本key作为一个新的重启点，重新开始存放
	else
	{
		// Restart compression
		restarts_.push_back(buffer_.size());
		counter_ = 0;
	}
	// 不相同的部分
	const size_t non_shared = key.size() - shared;

	// Add "<shared><non_shared><value_size>" to buffer_
	PutVarint32(&buffer_, shared);     // key相同部分的长度
	PutVarint32(&buffer_, non_shared); // key不同部分的长度
	PutVarint32(&buffer_, value.size());// value的长度

	// Add string delta to buffer_ followed by value
	buffer_.append(key.data() + shared, non_shared); //往buffer_中存入不同部分
	buffer_.append(value.data(), value.size());

	// Update state
	last_key_.resize(shared);
	last_key_.append(key.data() + shared, non_shared); //将上次的key，加入到last_key_中
	assert(Slice(last_key_) == key);
	counter_++;
}

} // namespace leveldb
