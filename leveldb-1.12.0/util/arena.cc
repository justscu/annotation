// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"
#include <assert.h>

namespace leveldb
{

static const int kBlockSize = 4096;

Arena::Arena()
{
	blocks_memory_ = 0;
	alloc_ptr_ = NULL; // First allocation will allocate a block
	alloc_bytes_remaining_ = 0;
}

Arena::~Arena()
{
	for (size_t i = 0; i < blocks_.size(); i++)
	{
		delete[] blocks_[i];
	}
}

char* Arena::AllocateFallback(size_t bytes)
{
	//要分配的内存比较大（>1K），直接分配
	if ( bytes > kBlockSize / 4 )
	{
		// Object is more than a quarter of our block size.  Allocate it separately
		// to avoid wasting too much space in leftover bytes.
		char* result = AllocateNewBlock(bytes);
		return result;
	}

	// We waste the remaining space in the current block.
	// 分配一块更大的内存出来。去掉bytes这块后，交给本类
	alloc_ptr_ = AllocateNewBlock(kBlockSize); //直接分配4K
	alloc_bytes_remaining_ = kBlockSize;

	char* result = alloc_ptr_;
	alloc_ptr_ += bytes;
	alloc_bytes_remaining_ -= bytes;
	return result;
}

// 分配的内存，需要内存对齐
char* Arena::AllocateAligned(size_t bytes)
{
	//内存对齐，根据指针大小来对齐
	const int align = sizeof(void*); // We'll align to pointer size
	assert((align & (align-1)) == 0); // Pointer size should be a power of 2
	size_t current_mod = reinterpret_cast<uintptr_t> (alloc_ptr_) & (align - 1);
	size_t slop = (current_mod == 0 ? 0 : align - current_mod);
	size_t needed = bytes + slop;
	char* result;
	if ( needed <= alloc_bytes_remaining_ )
	{
		result = alloc_ptr_ + slop;
		alloc_ptr_ += needed;
		alloc_bytes_remaining_ -= needed;
	}
	else
	{
		// AllocateFallback always returned aligned memory
		result = AllocateFallback(bytes);
	}
	assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);
	return result;
}

//分配一块新内存.
char* Arena::AllocateNewBlock(size_t block_bytes)
{
	char* result = new char[block_bytes];
	blocks_memory_ += block_bytes;
	blocks_.push_back(result);
	return result;
}

} // namespace leveldb
