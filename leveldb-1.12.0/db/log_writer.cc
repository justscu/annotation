// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/log_writer.h"

#include <stdint.h>
#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb
{
namespace log
{

Writer::Writer(WritableFile* dest) :
	dest_(dest), block_offset_(0)
{
	for (int i = 0; i <= kMaxRecordType; i++)
	{
		char t = static_cast<char> (i);
		type_crc_[i] = crc32c::Value(&t, 1);
	}
}

Writer::~Writer()
{
}

// 增加一条 记录(record).
// 写到block里面，每个block 32K.
Status Writer::AddRecord(const Slice& slice)
{
	const char* ptr = slice.data();
	size_t left = slice.size(); //剩下没有写的长度

	// Fragment(分片) the record if necessary and emit it.  Note that if slice
	// is empty, we still want to iterate once to emit a single
	// zero-length record
	Status s;
	bool begin = true;
	do
	{
		// 该block剩下的大小
		const int leftover = kBlockSize - block_offset_;
		assert(leftover >= 0);
		//剩下的长度小于头部长度，用0来填充.
		if ( leftover < kHeaderSize )
		{
			// Switch to a new block
			if ( leftover > 0 )
			{
				// Fill the trailer (literal below relies on kHeaderSize being 7)
				assert(kHeaderSize == 7);
				dest_->Append(Slice("\x00\x00\x00\x00\x00\x00", leftover));
			}
			block_offset_ = 0;
		}

		// Invariant: we never leave < kHeaderSize bytes in a block.
		assert(kBlockSize - block_offset_ - kHeaderSize >= 0);

		const size_t avail = kBlockSize - block_offset_ - kHeaderSize; //该block剩下可用长度
		const size_t fragment_length = (left < avail) ? left : avail; //可以填充的长度.

		RecordType type;
		const bool end = (left == fragment_length);
		if ( begin && end )
		{
			type = kFullType;
		}
		else if ( begin )
		{
			type = kFirstType;
		}
		else if ( end )
		{
			type = kLastType;
		}
		else
		{
			type = kMiddleType;
		}

		s = EmitPhysicalRecord(type, ptr, fragment_length);
		ptr += fragment_length;
		left -= fragment_length;
		begin = false;
	} while (s.ok() && left > 0);
	return s;
}

Status Writer::EmitPhysicalRecord(RecordType t, const char* ptr, size_t n)
{
	assert(n <= 0xffff); // Must fit in two bytes
	assert(block_offset_ + kHeaderSize + n <= kBlockSize);

	// Format the header
	char buf[kHeaderSize]; //32bit, CRC; 16bit, len; 8bit, type.
	// little-endian
	buf[4] = static_cast<char> (n & 0xff); //先存小的
	buf[5] = static_cast<char> (n >> 8); //后存大的
	buf[6] = static_cast<char> (t);

	// Compute the crc of the record type and the payload.
	uint32_t crc = crc32c::Extend(type_crc_[t], ptr, n);
	crc = crc32c::Mask(crc); // Adjust for storage
	EncodeFixed32(buf, crc);

	// Write the header and the payload
	Status s = dest_->Append(Slice(buf, kHeaderSize)); //写头部信息
	if ( s.ok() )
	{
		s = dest_->Append(Slice(ptr, n));
		if ( s.ok() )
		{
			s = dest_->Flush();
		}
	}
	block_offset_ += kHeaderSize + n;
	return s;
}

} // namespace log
} // namespace leveldb
