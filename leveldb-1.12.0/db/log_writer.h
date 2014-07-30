// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include <stdint.h>
#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb
{

class WritableFile;

namespace log
{

// 写的时候，是一个 block一个block的写[每个block占32K]
class Writer
{
	public:
		// Create a writer that will append data to "*dest".
		// "*dest" must be initially empty.
		// "*dest" must remain live while this Writer is in use.
		explicit Writer(WritableFile* dest);
		~Writer();

		Status AddRecord(const Slice& slice);

	private:
		WritableFile* dest_;
		int block_offset_; // Current offset in block，当前指针在block中的偏移

		// crc32c values for all supported record types.  These are
		// pre-computed to reduce the overhead of computing the crc of the
		// record type stored in the header.
		uint32_t type_crc_[kMaxRecordType + 1];

		// 写物理磁盘
		Status EmitPhysicalRecord(RecordType type, const char* ptr,
				size_t length);

		// No copying allowed
		Writer(const Writer&);
		void operator=(const Writer&);
};

} // namespace log
} // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_WRITER_H_
