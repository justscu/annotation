// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/coding.h"

namespace leveldb
{

// 将 @value 进行编码，并放入到 @buf 中。
void EncodeFixed32(char* buf, uint32_t value)
{
	if ( port::kLittleEndian )
	{
		memcpy(buf, &value, sizeof(value));
	}
	else
	{
		buf[0] = value & 0xff;
		buf[1] = (value >> 8) & 0xff;
		buf[2] = (value >> 16) & 0xff;
		buf[3] = (value >> 24) & 0xff;
	}
}

void EncodeFixed64(char* buf, uint64_t value)
{
	if ( port::kLittleEndian )
	{
		memcpy(buf, &value, sizeof(value));
	}
	else
	{
		buf[0] = value & 0xff;
		buf[1] = (value >> 8) & 0xff;
		buf[2] = (value >> 16) & 0xff;
		buf[3] = (value >> 24) & 0xff;
		buf[4] = (value >> 32) & 0xff;
		buf[5] = (value >> 40) & 0xff;
		buf[6] = (value >> 48) & 0xff;
		buf[7] = (value >> 56) & 0xff;
	}
}

//存放固定长度（32位）的数据
void PutFixed32(std::string* dst, uint32_t value)
{
	char buf[sizeof(value)];
	EncodeFixed32(buf, value);
	dst->append(buf, sizeof(buf));
}

void PutFixed64(std::string* dst, uint64_t value)
{
	char buf[sizeof(value)];
	EncodeFixed64(buf, value);
	dst->append(buf, sizeof(buf));
}

// 变长编码(32bit)，每7bit进行一次编码.
char* EncodeVarint32(char* dst, uint32_t v)
{
	// Operate on characters as unsigneds
	unsigned char* ptr = reinterpret_cast<unsigned char*> (dst);
	static const int B = 128; // 1000 0000 B
	if ( v < (1 << 7) )
	{
		*(ptr++) = v;
	}
	else if ( v < (1 << 14) )
	{
		*(ptr++) = v | B; //取低7bit，并把第8bit置1.
		*(ptr++) = v >> 7;//取 8bit - 14bit.
	}
	else if ( v < (1 << 21) )
	{
		*(ptr++) = v | B;
		*(ptr++) = (v >> 7) | B;
		*(ptr++) = v >> 14;
	}
	else if ( v < (1 << 28) )
	{
		*(ptr++) = v | B;
		*(ptr++) = (v >> 7) | B;
		*(ptr++) = (v >> 14) | B;
		*(ptr++) = v >> 21;
	}
	else
	{
		*(ptr++) = v | B;
		*(ptr++) = (v >> 7) | B;
		*(ptr++) = (v >> 14) | B;
		*(ptr++) = (v >> 21) | B;
		*(ptr++) = v >> 28;
	}
	return reinterpret_cast<char*> (ptr);
}

void PutVarint32(std::string* dst, uint32_t v)
{
	char buf[5];
	char* ptr = EncodeVarint32(buf, v);
	dst->append(buf, ptr - buf);
}

char* EncodeVarint64(char* dst, uint64_t v)
{
	static const int B = 128;
	unsigned char* ptr = reinterpret_cast<unsigned char*> (dst);
	while (v >= B)
	{
		*(ptr++) = (v & (B - 1)) | B;  // v & (B-1)，取低7bit； 后面 |b，将高位置1.
		v >>= 7;
	}
	*(ptr++) = static_cast<unsigned char> (v);
	return reinterpret_cast<char*> (ptr);
}


//每7bit进行一次编码，最多需要64/7 = 9.1 = 10字节
void PutVarint64(std::string* dst, uint64_t v)
{
	char buf[10];
	char* ptr = EncodeVarint64(buf, v);
	dst->append(buf, ptr - buf);
}

//先存放大小(变长编码)，再存放具体字符串
void PutLengthPrefixedSlice(std::string* dst, const Slice& value)
{
	PutVarint32(dst, value.size());
	dst->append(value.data(), value.size());
}

//求字符长度
int VarintLength(uint64_t v)
{
	int len = 1;
	while (v >= 128)
	{
		v >>= 7;
		len++;
	}
	return len;
}

//将变长编码的字符串，转回32位int
/*
 * 要解析[@p, @limit]间的数据，并将结果返回到 @value中。
 * @return，返回@p未解析的位置。
 *
 */
const char* GetVarint32PtrFallback(const char* p, const char* limit,
		uint32_t* value)
{
	uint32_t result = 0;
	for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7)
	{
		uint32_t byte = *(reinterpret_cast<const unsigned char*> (p));
		p++;
		if ( byte & 128 ) //最高位为1，说明还有数据
		{
			// More bytes are present
			result |= ((byte & 127) << shift);
		}
		else
		{
			result |= (byte << shift);
			*value = result;
			return reinterpret_cast<const char*> (p);
		}
	}
	return NULL;
}

bool GetVarint32(Slice* input, uint32_t* value)
{
	const char* p = input->data();
	const char* limit = p + input->size();
	const char* q = GetVarint32Ptr(p, limit, value);
	if ( q == NULL )
	{
		return false;
	}
	else
	{
		*input = Slice(q, limit - q);
		return true;
	}
}

const char* GetVarint64Ptr(const char* p, const char* limit, uint64_t* value)
{
	uint64_t result = 0;
	for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7)
	{
		uint64_t byte = *(reinterpret_cast<const unsigned char*> (p));
		p++;
		if ( byte & 128 )
		{
			// More bytes are present
			result |= ((byte & 127) << shift);
		}
		else
		{
			result |= (byte << shift);
			*value = result;
			return reinterpret_cast<const char*> (p);
		}
	}
	return NULL;
}

bool GetVarint64(Slice* input, uint64_t* value)
{
	const char* p = input->data();
	const char* limit = p + input->size();
	const char* q = GetVarint64Ptr(p, limit, value);
	if ( q == NULL )
	{
		return false;
	}
	else
	{
		*input = Slice(q, limit - q);
		return true;
	}
}

/*
 * [@p, @limit]，前面放长度，后面放具体的字符串.
 * 将[@p, @limit] 间的字符串解析出来，并转成Slice格式。
 *
 */
const char* GetLengthPrefixedSlice(const char* p, const char* limit,
		Slice* result)
{
	uint32_t len;
	p = GetVarint32Ptr(p, limit, &len);
	if ( p == NULL )
		return NULL;
	if ( p + len > limit )
		return NULL;
	*result = Slice(p, len);
	return p + len;
}

bool GetLengthPrefixedSlice(Slice* input, Slice* result)
{
	uint32_t len;
	if ( GetVarint32(input, &len) && input->size() >= len )
	{
		*result = Slice(input->data(), len);
		input->remove_prefix(len);
		return true;
	}
	else
	{
		return false;
	}
}

} // namespace leveldb
