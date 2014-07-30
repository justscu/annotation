// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// WriteBatch holds a collection of updates to apply atomically to a DB.
//
// The updates are applied in the order in which they are added
// to the WriteBatch.  For example, the value of "key" will be "v3"
// after the following batch is written:
//
//    batch.Put("key", "v1");
//    batch.Delete("key");
//    batch.Put("key", "v2");
//    batch.Put("key", "v3");
//
// Multiple threads can invoke const methods on a WriteBatch without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same WriteBatch must use
// external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_
#define STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_

#include <string>
#include "leveldb/status.h"

namespace leveldb
{

class Slice;

// 批处理 写
// @rep_，用来存放处理后的数据
class WriteBatch
{
	public:
		WriteBatch();
		~WriteBatch();

		// Store the mapping "key->value" in the database.
		// 加入 k-v
		void Put(const Slice& key, const Slice& value);

		// If the database contains a mapping for "key", erase it.  Else do nothing.
		// 加入 k.
		void Delete(const Slice& key);

		// Clear all updates buffered in this batch.
		void Clear();

		// Support for iterating over the contents of a batch.
		class Handler
		{
			public:
				virtual ~Handler();
				virtual void Put(const Slice& key, const Slice& value) = 0;
				virtual void Delete(const Slice& key) = 0;
		};
		// 使用该 iterator，利用 @handler提供的接口来处理所有的key。
		Status Iterate(Handler* handler) const;

	private:
		friend class WriteBatchInternal;

		// WriteBatch 的头部为 8bytes的sequenceNum, + 4bytes 的count.
		std::string rep_; // See comment in write_batch.cc for the format of rep_

		// Intentionally copyable
};

} // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_WRITE_BATCH_H_
