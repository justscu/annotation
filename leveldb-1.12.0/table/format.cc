// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/format.h"

#include "leveldb/env.h"
#include "port/port.h"
#include "table/block.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb
{

// 对 @offset_ 和 @size_ 进行编码，并将编码结果放入 @dst中。
void BlockHandle::EncodeTo(std::string* dst) const
{
	// Sanity check that all fields have been set
	assert(offset_ != ~static_cast<uint64_t>(0));
	assert(size_ != ~static_cast<uint64_t>(0));
	PutVarint64(dst, offset_);
	PutVarint64(dst, size_);
}

// 从 @input中解码出 @offset_ , @size_
Status BlockHandle::DecodeFrom(Slice* input)
{
	if ( GetVarint64(input, &offset_) && GetVarint64(input, &size_) )
	{
		return Status::OK();
	}
	else
	{
		return Status::Corruption("bad block handle");
	}
}

// 编码后的总长度：20*2 + 8 = 48.
void Footer::EncodeTo(std::string* dst) const
{
#ifndef NDEBUG
	const size_t original_size = dst->size();
#endif
	metaindex_handle_.EncodeTo(dst);
	index_handle_.EncodeTo(dst);
	dst->resize(2 * BlockHandle::kMaxEncodedLength); // Padding，填充
	PutFixed32(dst, static_cast<uint32_t> (kTableMagicNumber & 0xffffffffu)); //低32bit.
	PutFixed32(dst, static_cast<uint32_t> (kTableMagicNumber >> 32)); //高32bit.
	assert(dst->size() == original_size + kEncodedLength);
}

Status Footer::DecodeFrom(Slice* input)
{
	// 验证 magic number.
	const char* magic_ptr = input->data() + kEncodedLength - 8;
	const uint32_t magic_lo = DecodeFixed32(magic_ptr);
	const uint32_t magic_hi = DecodeFixed32(magic_ptr + 4);
	const uint64_t magic = ((static_cast<uint64_t> (magic_hi) << 32)
			| (static_cast<uint64_t> (magic_lo)));
	if ( magic != kTableMagicNumber )
	{
		return Status::InvalidArgument("not an sstable (bad magic number)");
	}

	//获取 meta-index-handle.
	Status result = metaindex_handle_.DecodeFrom(input);
	if ( result.ok() )
	{
		// 获取 index-handle.
		result = index_handle_.DecodeFrom(input);
	}
	if ( result.ok() )
	{
		// We skip over any leftover data (just padding for now) in "input"
		const char* end = magic_ptr + 8;
		*input = Slice(end, input->data() + input->size() - end);
	}
	return result;
}

// @file，指向文件的指针; @options，读取参数
// 读的参数(block的大小和偏移)，根据 @handle来定
// 读完后的数据，都放入 @result中。result->heap_allocated == true时，需要自己释放内存
Status ReadBlock(RandomAccessFile* file, const ReadOptions& options,
		const BlockHandle& handle, BlockContents* result)
{
	result->data = Slice();
	result->cachable = false;
	result->heap_allocated = false;

	// Read the block contents as well as the type/crc footer.
	// See table_builder.cc for the code that built this structure.
	size_t n = static_cast<size_t> (handle.size()); // 返回block大小
	char* buf = new char[n + kBlockTrailerSize];
	Slice contents;
	// 读出来的数据，可能存在 contents 和 buf中[contents指向buf]； 也可能只存于contents中。
	Status s = file->Read(handle.offset(), n + kBlockTrailerSize, &contents,
			buf);
	if ( !s.ok() )
	{
		delete[] buf;
		return s;
	}
	if ( contents.size() != n + kBlockTrailerSize )
	{
		delete[] buf;
		return Status::Corruption("truncated block read");
	}

	// Check the crc of the type and the block contents
	const char* data = contents.data(); // Pointer to where Read put the data
	if ( options.verify_checksums )
	{
		const uint32_t crc = crc32c::Unmask(DecodeFixed32(data + n + 1)); //存储的crc
		const uint32_t actual = crc32c::Value(data, n + 1); //算出来的crc
		if ( actual != crc )
		{
			delete[] buf;
			s = Status::Corruption("block checksum mismatch");
			return s;
		}
	}

	switch (data[n])
	{
	case kNoCompression:
		if ( data != buf ) // 数据只存与contents中
		{
			// File implementation gave us pointer to some other data.
			// Use it directly under the assumption that it will be live
			// while the file is open.
			delete[] buf;
			result->data = Slice(data, n);
			result->heap_allocated = false;
			result->cachable = false; // Do not double-cache
		}
		else
		{
			result->data = Slice(buf, n);
			result->heap_allocated = true;
			result->cachable = true;
		}

		// Ok
		break;
	case kSnappyCompression:
	{
		size_t ulength = 0;
		if ( !port::Snappy_GetUncompressedLength(data, n, &ulength) )
		{
			delete[] buf;
			return Status::Corruption("corrupted compressed block contents");
		}
		char* ubuf = new char[ulength];
		if ( !port::Snappy_Uncompress(data, n, ubuf) )
		{
			delete[] buf;
			delete[] ubuf;
			return Status::Corruption("corrupted compressed block contents");
		}
		delete[] buf;
		result->data = Slice(ubuf, ulength);
		result->heap_allocated = true;
		result->cachable = true;
		break;
	}
	default:
		delete[] buf;
		return Status::Corruption("bad block type");
	}

	return Status::OK();
}

} // namespace leveldb
