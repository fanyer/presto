/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/src/opspellchecker.h"
#include "modules/spellchecker/opspellcheckerapi.h"

#ifdef SPC_DEBUG

OP_STATUS sc_error(OP_STATUS status)
{
	OP_ASSERT(FALSE);
	return status;
}

#if defined(WIN32) && !defined(_WIN64) && !defined(SPC_USE_HUNSPELL_ENGINE)

#include <crtdbg.h>

struct AllocBlockHeader
{
	AllocBlockHeader *next;
	AllocBlockHeader *prev;
	char *szFileName;
	int nLine;
	size_t nDataSize;
	int nBlockUse;
	long lRequest;
	UINT32 gap;
};

BOOL sc_mem_ok(void *mem)
{
	if (!_CrtIsValidHeapPointer(mem))
		return FALSE;
	AllocBlockHeader *header = (AllocBlockHeader*)mem - 1;
	if (header->gap != 0xFDFDFDFD || ((UINT32*)((UINT8*)mem + header->nDataSize))[0] != 0xFDFDFDFD)
		return FALSE;
	return TRUE;
}
#else
BOOL sc_mem_ok(void *mem) { return TRUE; }
#endif // defined(WIN32) && !defined(_WIN64)

#endif // SPC_DEBUG

BOOL is_ascii_new_line(UINT8 c)
{
	return c == '\n' || c == '\r';
}

BOOL is_word_separator(UINT8 c)
{
	return c == ' ' || c == '\t';
}

BOOL str_eq(UINT8 *s1, UINT8 *s2)
{
	for (;*s1 == *s2 && *s1; s1++,s2++);
	return *s1 == *s2;
}

BOOL str_equal(UINT8 *i1, UINT8 *i2, int len)
{
	if (!len)
		return TRUE;
	for (;--len && *i1 == *i2; i1++, i2++);
	return *i1 == *i2;
}

BOOL uni_fragment_is_any_of(uni_char ch, const uni_char *str, const uni_char *str_end)
{
	OP_ASSERT(str <= str_end);
	for (;str!=str_end && *str != ch; str++);
	return str!=str_end;
}

#ifndef SPC_USE_HUNSPELL_ENGINE

BOOL has_any_not(UINT8 *str, const char *_not_)
{
	int offset_of_not = op_strspn(reinterpret_cast<char*>(str), _not_); // FIXME, start using regular chars everywhere?
	return str[offset_of_not] != '\0';
}

UINT8 *bs_strip(UINT8 *word)
{
	if (!word || !*word)
		return word;
	UINT8 *iter = word;
	while (*iter)
	{
		if (*iter == '\\')
		{
			UINT8 *ptr = iter;
			do
			{
				*ptr = ptr[1];
			} while (*(ptr++));
			if (!*iter)
				return word;
		}
		iter++;
	}
	return word;
}

UINT8 *ws_strip(UINT8 *word, UINT8 *word_end)
{
	if (!word || !*word)
		return word;
	if (!word_end)
		word_end = word + op_strlen((const char*)word);
	OP_ASSERT(!*word_end);
	word_end--;
	while (word != word_end && is_word_separator(*word))
		*(word++) = '\0'; // must make zero for HunspellDictionaryFileLine::GetFlags to work
	while (word_end >= word && is_word_separator(*word_end))
		*(word_end--) = '\0';
	return word;
}

void replace_byte(UINT8 *data, int bytes, UINT8 old_byte, UINT8 new_byte)
{
	for (data += bytes; bytes--; data--)
	{
		if (*data == old_byte)
			*data = new_byte;
	}
}

int pos_num_to_int(UINT8 *str, int max_num, int &bytes_used)
{
	while (*str == '0')
		str++;
	UINT8 *ptr = str;
	while (*ptr >= '0' && *ptr <= '9')
		ptr++;
	bytes_used = (int)(ptr-str);
	if (!bytes_used)
		return -1;
	int multi = 1;
	int res = 0;
	do
	{
		ptr--;
		res += (*ptr-'0') * multi;
		if (res > max_num)
			return -1;
		multi *= 10;
	} while (ptr != str);
	return res;
}

BOOL is_number(const uni_char *str)
{
	for (;*str && *str >= '0' && *str <= '9'; str++);
	return !*str;
}
#endif //!SPC_USE_HUNSPELL_ENGINE

BOOL is_ambiguous_break(uni_char ch)
{
	return ch == '-' || ch == '\''
		|| ch >= 0x901 && ch < 0x903
		|| ch >= 0x2010 && ch <= 0x2015;
}

#ifndef SPC_USE_HUNSPELL_ENGINE

int wide_str_len(UINT32 *str)
{
	if (!str)
		return 0;
	int len;
	for (len=0;*(str++);len++);
	return len;
}

int wide_strcmp(UINT32 *word, int word_len, UINT32 *other, int other_len)
{
	int i, len;
	int diff;
	if (other_len != word_len)
	{
		if (other_len > word_len)
		{
			len = word_len;
			diff = -1;
		}
		else
		{
			len = other_len;
			diff = 1;
		}
	}
	else
	{
		diff = 0;
		len = word_len;
	}
	for (i=0;i<len;i++,word++,other++)
		if (*word != *other)
			return (int)(*word) - (int)(*other);
	return diff;
}

BOOL indexes_equal(UINT32 *i1, UINT32 *i2, int len)
{
	if (!len)
		return TRUE;
	for (;--len && *i1 == *i2; i1++, i2++);
	return *i1 == *i2;
}

int utf8_strlen(UINT8 *str)
{
	int len = 0;
	while (*str)
	{
		if ((*str&0xC0) != 0x80)
			len++;
		str++;
	}
	return len;
}

int utf8_decode(UINT8 *str, int &bytes_used)
{
	if (!(*str&0x80))
	{
		bytes_used = 1;
		return (int)(*str);
	}
	else if (!(*str&0x40))
		return -1;
	else if (!(*str&0x20))
	{
		bytes_used = 2;
		if (!(str[0]&0x1E) || (str[1]&0xC0) != 0x80) // overlong encoding || illegal sequence
			return -1;
		return (int)(str[0]&0x1F)<<6 | (str[1]&0x3F);
	}
	else if (!(*str&0x10))
	{
		bytes_used = 3;
		if ((str[1]&0xC0) != 0x80 || (str[2]&0xC0) != 0x80) // illegal sequence
			return -1;
		int res = (int)(str[0]&0x0F)<<12 | (int)(str[1]&0x3F)<<6 | (str[2]&0x3F);
		if (!(res&0xF800) || (res&0xF800) == 0xD800 || (res&0xFFFE) == 0xFFFE) // overlong encoding || D800->DFFF || 0xFFFE->0xFFFF
			return -1;
		return res;

	}
	else if (!(*str&0x08))
	{
		bytes_used = 4;
		if ((str[1]&0xC0) != 0x80 || (str[2]&0xC0) != 0x80 || (str[3]&0xC0) != 0x80) // illegal sequence
			return -1;
		int res = (int)(str[0]&0x0F)<<18 | (int)(str[1]&0x3F)<<12 | (int)(str[2]&0x3F)<<6 | (str[3]&0x3F);
		if (((res&0xFFFF0000)-1)&0xFFF00000) // not 0x10000->0x10FFFF
			return -1;
		return res;
	}
	return -1;
}

BOOL utf8_decode_str(UINT8 *str, UINT32 *res, int &bytes_consumed, int &chars_written)
{
	int val;
	int bytes_used = 0;
	while (*str)
	{
		val = utf8_decode(str,bytes_used);
		if (val < 0)
			return FALSE;
		*(res++) = (UINT32)val;
		str += bytes_used;
		bytes_consumed += bytes_used;
		chars_written++;
	}
	bytes_consumed++;
	*res = 0;
	return TRUE;
}

int decode_flag(UINT8 *str, FlagEncoding encoding, int &bytes_used)
{
	if (!str || !*str)
		return -1;
	switch (encoding)
	{
		case FLAG_ASCII_ENC:
			bytes_used = 1;
			return (int)*str;
		case FLAG_LONG_ENC:
			if (!str[1])
			{
				bytes_used = 1;
				return (int)*str;
			}
			bytes_used = 2;
			return ((int)(*str) << 8) + str[1];
		case FLAG_NUM_ENC:
			return pos_num_to_int(str,0xFFFF,bytes_used);
	}
	OP_ASSERT(encoding == FLAG_UTF8_ENC);
	int flag = utf8_decode(str,bytes_used);
	return flag & ~0xFFFF ? -1 : flag;	// We only support UTF-8 flags < 65536
}

OP_STATUS get_wide_str_ptr(UINT8 *str, UINT32 *&result, BOOL bit8_enc, OpSpellCheckerAllocator *allocator)
{
	if (!str)
		return OpStatus::OK;
	int str_len = bit8_enc ? op_strlen((const char*)str) : utf8_strlen(str);
	if (allocator) // else, result is already the buffer we should write to
		result = (UINT32*)allocator->Allocate32((str_len+1)*sizeof(UINT32));
	int dummy1 = 0, dummy2 = 0;
	if (!result)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	if (bit8_enc)
	{
		int i;
		for (i=0;i<str_len+1;i++)
			result[i] = (UINT32)str[i];
	}
	else if (!utf8_decode_str(str,result,dummy1,dummy2))
		return SPC_ERROR(OpStatus::ERR);
	return OpStatus::OK;
}

OP_STATUS add_str_to_buffer(UINT8 *str, ExpandingBuffer<UINT32*> *buffer, BOOL bit8_enc, OpSpellCheckerAllocator *allocator)
{
	if (!str)
		return OpStatus::OK;
	UINT32 *wide_str = NULL;
	RETURN_IF_ERROR(get_wide_str_ptr(str, wide_str, bit8_enc, allocator));
	if (buffer->AddElement(wide_str) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	return OpStatus::OK;
}

int bits_to_represent(int count)
{
	if (!count)
		return 0;
	OP_ASSERT(count > 0);
	int pos = sizeof(int)*8 - 1;
	while (!(count & 1<<pos))
		pos--;
	return count & ~(1<<pos) ? pos+1 : pos;
}

#endif //!SPC_USE_HUNSPELL_ENGINE

template <class T> void quick_sort(T *unsorted, int elements, void *buffer)
{
	if (elements <= 1)
		return;
	UINT32 *buf = (UINT32*)buffer;
	buf[0] = 0;
	buf[1] = (UINT32)elements;
	int buf_idx = 0;
	while (buf_idx >= 0)
	{
		int list_ofs = (int)buf[buf_idx];
		elements = (int)buf[buf_idx+1];
		T *list = unsorted + list_ofs;
		int i=0,j=elements-1;
		int p = elements>>1;
		T pivot = list[p];
		list[p] = list[j];
		list[j--] = pivot;
		int diff;
		T tmp;
		for (;;i++,j--)
		{
			while (i != j && list[i] <= pivot)
				i++;
			while (i != j && list[j] > pivot)
				j--;
			if (i == j)
			{
				if (list[j] <= pivot)
					j++;
				break;
			}
			tmp = list[i];
			list[i] = list[j];
			list[j] = tmp;
			diff = j-i;
			if (diff == 2)
			{
				if (list[j-1] > pivot)
					j--;
				break;
			}
			if (diff == 1)
				break;
		}
		list[elements-1] = list[j];
		list[j] = pivot;
		if (j > 1)
		{
			buf[buf_idx++] = (UINT32)list_ofs;
			buf[buf_idx++] = (UINT32)j;
		}
		int right_elms = elements-j-1;
		if (right_elms > 1)
		{
			buf[buf_idx++] = (UINT32)(list_ofs + j + 1);
			buf[buf_idx++] = (UINT32)(right_elms);
		}
		buf_idx -= 2;
	}
}

#ifndef SPC_USE_HUNSPELL_ENGINE

template <class T> void rev_heap_add(T elm, T *heap, int elements)
{
	int pos;
	heap--;
	for (pos = elements+1; pos > 1 && heap[pos>>1] < elm; pos >>= 1)
		heap[pos] = heap[pos>>1];
	heap[pos] = elm;
}

template <class T> void rev_heap_replace_first(T elm, T *heap, int elements)
{
	int child, pos = 1;
	heap--;
	while ((pos<<1) <= elements)
	{
		child = (pos<<1) == elements || heap[pos<<1] > heap[(pos<<1)+1] ? pos<<1 : (pos<<1)+1;
		if (heap[child] <= elm)
			break;
		heap[pos] = heap[child];
		pos = child;
	}
	heap[pos] = elm;
}

#endif //!SPC_USE_HUNSPELL_ENGINE


OP_STATUS SCOpenFile(uni_char *path, OpFile &file, int &file_length, int mode)
{
	OP_STATUS status = file.Construct(path);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	status = file.Open(mode);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	OpFileLength _len = 0;
	status = file.GetFileLength(_len);
	if (OpStatus::IsError(status) || _len > 2000000000)
	{
		file.Close();
		return SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR);
	}
	file_length = (int)_len;
	return OpStatus::OK;
}

int get_sorted_index(const uni_char *string, const uni_char *string_end, uni_char **str_array, int size, BOOL &has_string)
{
	OP_ASSERT(string <= string_end);

	has_string = FALSE;
	if (!string || string == string_end || !str_array || !size)
		return -1;
	int low = 0, high = size-1, middle = 0, cmp;
	int string_len = string_end - string;
	while (low < high)
	{
		middle = low + ((high-low)>>1);
		cmp = uni_strncmp(string, (const uni_char*)str_array[middle], string_len);
		if (!cmp)
		{
			has_string = TRUE;
			return middle;
		}
		if (cmp > 0)
			low = middle+1;
		else 
			high = middle-1;
	}
	if (low == high)
	{
		cmp = uni_strncmp(string, (const uni_char*)str_array[high], string_len);
		has_string = !cmp;
		if (cmp < 0)
			high--;
	}
	return high;
}


struct UniCharPointer
{
	BOOL operator<=(const UniCharPointer &other) const { return uni_strcmp(string, other.string) <= 0; }
	BOOL operator>=(const UniCharPointer &other) const { return uni_strcmp(string, other.string) >= 0; }
	BOOL operator>(const UniCharPointer &other) const { return uni_strcmp(string, other.string) > 0; }
	BOOL operator<(const UniCharPointer &other) const { return uni_strcmp(string, other.string) < 0; }

	uni_char *string;
};

OP_STATUS sort_strings(uni_char **str_array, int count)
{
	if (!str_array || count <= 1)
		return OpStatus::OK;
	UINT8 *buf = OP_NEWA(UINT8, count*8);
	if (!buf)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	quick_sort<UniCharPointer>((UniCharPointer*)str_array, count, buf);
	OP_DELETEA(buf);
	return OpStatus::OK;
}

/* ===============================OpSpellCheckerFixedAllocator=============================== */

void OpSpellCheckerFixedAllocator::Init(int unit_bytes, int units_per_alloc, BOOL mem_zero)
{
	m_malloced = NULL;
	m_malloced_count = 0;
	m_unit_bytes = unit_bytes;
	m_units_per_alloc = units_per_alloc;
	m_mem_zero = mem_zero;
	m_current_local_index = m_units_per_alloc;
	m_current_global_index = -1;
}

OpSpellCheckerFixedAllocator::OpSpellCheckerFixedAllocator(int unit_bytes, int units_per_alloc, BOOL mem_zero)
{
	Init(unit_bytes,units_per_alloc,mem_zero);
}

void OpSpellCheckerFixedAllocator::Clean()
{
	int i;
	if (m_malloced)
		for (i=0;i<=m_current_global_index;i++)
			op_free(m_malloced[i]);
	OP_DELETEA(m_malloced);
	Init(m_unit_bytes,m_units_per_alloc,m_mem_zero);
}

void OpSpellCheckerFixedAllocator::Reset()
{
	int i;
	if (m_malloced)
	{
		for (i=1;i<=m_current_global_index;i++)
		{
			op_free(m_malloced[i]);
			m_malloced[i] = NULL;
		}
		m_current_global_index = 0;
		m_current_local_index = 0;
		if (m_mem_zero)
			op_memset(m_malloced[0], 0, m_unit_bytes*m_units_per_alloc);
	}
}

BOOL OpSpellCheckerFixedAllocator::MallocMore()
{
	if (!m_malloced)
	{
		int init_array_sz = 128;
		m_malloced = OP_NEWA(UINT8*, init_array_sz);
		if (!m_malloced)
			return FALSE;
		m_malloced_count = init_array_sz;
	}
	else if (m_current_global_index == m_malloced_count-1)
	{
		UINT8 **new_array = OP_NEWA(UINT8*, m_malloced_count*2);
		if (!new_array)
			return FALSE;
		op_memcpy(new_array,m_malloced,m_malloced_count*sizeof(UINT8*));
		OP_DELETEA(m_malloced);
		m_malloced = new_array;
		m_malloced_count *= 2;
	}
	UINT8 *m_new_data = (UINT8*)(m_mem_zero ? op_calloc(m_unit_bytes,m_units_per_alloc) : op_malloc(m_unit_bytes*m_units_per_alloc));
	if (!m_new_data)
		return FALSE;
	OP_ASSERT((SPC_UINT_PTR)(m_new_data) >= 4096 || !"If this can happen on this platform we're in trouble!");
	m_malloced[++m_current_global_index] = m_new_data;
	m_current_local_index = 0;
	return TRUE;
}

#ifdef SPC_DEBUG
void OpSpellCheckerFixedAllocator::VerifyMemOk()
{
	int i;
	OP_ASSERT(!m_malloced || sc_mem_ok(m_malloced));
	for (i=0;i<=m_current_global_index;i++)
		OP_ASSERT(sc_mem_ok(m_malloced[i]));
}
#endif // SPC_DEBUG

void* OpSpellCheckerFixedAllocator::Allocate()
{
	if (m_current_local_index == m_units_per_alloc)
		if (!MallocMore())
			return NULL;
	return (void*)(m_malloced[m_current_global_index] + m_current_local_index++ * m_unit_bytes);
}

void* OpSpellCheckerFixedAllocator::Allocate(int &malloc_index, int &byte_offset)
{
	if (m_current_local_index == m_units_per_alloc)
		if (!MallocMore())
			return NULL;
	malloc_index = m_current_global_index;
	byte_offset = m_current_local_index*m_unit_bytes;
	m_current_local_index++;
	return (void*)(m_malloced[m_current_global_index] + byte_offset);
}

/* ===============================OpSpellCheckerAllocator=============================== */

#define SPC_ALLOC_CHUNK_SIZE (128*1024)

void OpSpellCheckerAllocator::Init(int chunk_sz)
{
	m_mem = NULL;
	m_bytes_left = 0;
	m_last_allocated = 0;
	m_chunk_sz = chunk_sz;
}

OpSpellCheckerAllocator::OpSpellCheckerAllocator() : m_allocator(SPC_ALLOC_CHUNK_SIZE,1,TRUE)
{
	Init(SPC_ALLOC_CHUNK_SIZE);
}

OpSpellCheckerAllocator::OpSpellCheckerAllocator(int chunk_sz) : m_allocator(chunk_sz,1,TRUE)
{
	Init(chunk_sz);
}

void OpSpellCheckerAllocator::Reset()
{
	m_allocator.Reset();
	if (m_mem)
		m_mem = (UINT8*)m_allocator.GetMallocAddr(0);
	m_bytes_left = 0;
	m_last_allocated = 0;
}

void* OpSpellCheckerAllocator::AllocateAny(int bytes, int align_pattern)
{
	OP_ASSERT(bytes <= m_chunk_sz-4);
	if ((ptrdiff_t)(m_mem) & align_pattern)
	{
		ptrdiff_t to_inc = (~((ptrdiff_t)(m_mem)) & align_pattern) + 1;
		m_mem += to_inc;
		m_bytes_left -= to_inc;
	}
	if (m_bytes_left < bytes)
	{
		if (bytes > m_chunk_sz-4) // too large allocation
			return NULL;
		m_mem = (UINT8*)m_allocator.Allocate();
		if (!m_mem)
			return NULL;
		m_bytes_left = m_chunk_sz-4; // -4, we might want some "over reading" sometimes (when reading huffman-codes)
	}
	void *res = (void*)m_mem;
	m_mem += bytes;
	m_bytes_left -= bytes;
	m_last_allocated = bytes;
	return res;
}

void OpSpellCheckerAllocator::OnlyUsed(int bytes, BOOL zero_out_unused)
{
	int dec = m_last_allocated - bytes;
	OP_ASSERT(dec >= 0);
	m_mem -= dec;
	m_bytes_left += dec;
	m_last_allocated = bytes;
	if (zero_out_unused)
		op_memset(m_mem, 0, dec);
}

void OpSpellCheckerAllocator::RewindBytesIfPossible(int bytes)
{
	int can_rewind = MIN((m_chunk_sz-4) - m_bytes_left, bytes);
	m_mem -= can_rewind;
	m_bytes_left += can_rewind;
	m_last_allocated = 0;
	op_memset(m_mem, 0, can_rewind);
}

#ifndef SPC_USE_HUNSPELL_ENGINE

/* ===============================OpSpellCheckerBitStream=============================== */

void OpSpellCheckerBitStream::WriteVal32(UINT32 val, int bits)
{
	OP_ASSERT(bits <= 32 && !(val & (-1<<bits)));
	*m_byte |= (UINT8)(val << (m_bit));
	val >>= 8 - m_bit;
	m_byte[1] = val&0xFF;
	m_byte[2] = (val&0xFF00)>>8;
	m_byte[3] = (val&0xFF0000)>>16;
	m_byte[4] = (val&0xFF000000)>>24;
	m_byte += (m_bit + bits)>>3;
	m_bit = (m_bit + bits)&7;
}

void OpSpellCheckerBitStream::WriteVal16(UINT16 val, int bits)
{
	OP_ASSERT(bits <= 16 && !(val & (-1<<bits)));
	*m_byte |= (UINT8)(val << (m_bit));
	val >>= 8 - m_bit;
	m_byte[1] = val&0xFF;
	m_byte[2] = (val&0xFF00)>>8;
	m_byte += (m_bit + bits)>>3;
	m_bit = (m_bit + bits)&7;
}

void OpSpellCheckerBitStream::WriteDataNoOverWrite(UINT8 *data, int bits)
{
	if (!((m_bit+bits) & ~7)) // we're only writing to one byte
	{
		*m_byte &= ~(((1<<bits)-1) << m_bit);
		*m_byte |= (((1<<bits)-1) & *data) << m_bit;
		m_byte += (m_bit + bits)>>3;
		m_bit = (m_bit + bits)&7;
	}
	else
	{
		*m_byte &= ~(-1 << m_bit);
		*m_byte |= *data << m_bit;
		int bit = 8-m_bit;
		bits -= bit;
		if (bit == 8)
		{
			bit = 0;
			data++;
		}
		m_byte++;
		while (bits & ~7)
		{
			if (bit)
				*(m_byte++) = data[0] >> bit | data[1] << (8-bit);
			else
				*(m_byte++) = *data;
			data++;
			bits -= 8;
		}
		m_bit = bits;
		if (m_bit)
		{
			*m_byte &= ~((1<<m_bit)-1);
			if (bit + bits > 8)
				*m_byte |= (data[0] >> bit | data[1] << (8-bit)) & ((1<<bits)-1);
			else
				*m_byte |= (*data >> bit) & ((1<<bits)-1);
		}
	}
}	

UINT32 OpSpellCheckerBitStream::ReadVal32(int bits)
{
	OP_ASSERT(bits <= 32);
	UINT32 res = *m_byte >> m_bit;
	UINT32 val = m_byte[1] | m_byte[2]<<8 | m_byte[3]<<16 | m_byte[4]<<24;
	res |= val << (8-m_bit);
	if (bits != 32)
		res &= ~(-1 << bits);
	m_byte += (m_bit + bits)>>3;
	m_bit = (m_bit + bits)&7;
	return res;
}

UINT16 OpSpellCheckerBitStream::Read16BitNoInc()
{
	UINT16 res = *m_byte >> m_bit;
	UINT16 val = m_byte[1] | m_byte[2]<<8;
	res |= val << (8-m_bit);
	return res;
}

void* OpSpellCheckerBitStream::ReadPointer()
{
#ifdef SIXTY_FOUR_BIT
	UINT64 res = *m_byte >> m_bit;
	UINT64 val = m_byte[1] | m_byte[2]<<8 | m_byte[3]<<16 | m_byte[4]<<24;
	val |= (UINT64)(m_byte[5] | m_byte[6]<<8 | m_byte[7]<<16 | m_byte[8]<<24) << 32;
	res |= val << (8-m_bit);
	m_byte += (m_bit + 64)>>3;
	m_bit = (m_bit + 64)&7;
	return (void*)res;
#else // SIXTY_FOUR_BIT
	return (void*)ReadVal32(32);
#endif // !SIXTY_FOUR_BIT
}

/* ===============================UTFMapping=============================== */

template <class T> UTFMapping<T>* UTFMapping<T>::Create()
{
	UTFMapping<T> *obj = OP_NEW(UTFMapping<T>, ());
	if (!obj)
		return NULL;
	if (obj->Init() != OpStatus::OK)
	{
		OP_DELETE(obj);
		return NULL;
	}
	return obj;
}

template <class T> UTFMapping<T>::UTFMapping()
{
	op_memset(m_leafs,0xFF,SPC_UTF_LEAF_ARRAY_SIZE*sizeof(UINT16));
	op_memset(&m_empty,0,sizeof(T));
	m_status = OpStatus::OK;
	m_leaf_allocator = NULL;
	m_leaf_count = 0;
}
	
template <class T> OP_STATUS UTFMapping<T>::Init()
{
	m_leaf_allocator = OP_NEW(OpSpellCheckerFixedAllocator, (SPC_UTF_CODEPOINTS_PER_LEAF*sizeof(T), SPC_UTF_LEAFS_PER_MALLOC, TRUE));
	if (!m_leaf_allocator)
	{
		m_status = SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		return m_status;
	}
	return OpStatus::OK;
}

template <class T> BOOL UTFMapping<T>::GetVal(int cp, T &res)
{
	OP_ASSERT((unsigned int)cp < SPC_UTF_MAX_CODEPOINTS);
	int leaf_idx = cp>>SPC_UTF_LEAF_BITS;
	if (m_leafs[leaf_idx] == 0xFFFF)
		return FALSE;
	res = SPC_UTF_VALUE_AT_ALLOC_IDX(m_leafs[leaf_idx],cp);
	return TRUE;
}

template <class T> T UTFMapping<T>::GetVal(int cp)
{
	OP_ASSERT((unsigned int)cp < SPC_UTF_MAX_CODEPOINTS);
	int leaf_idx = cp>>SPC_UTF_LEAF_BITS;
	return m_leafs[leaf_idx] == 0xFFFF ? m_empty : SPC_UTF_VALUE_AT_ALLOC_IDX(m_leafs[leaf_idx],cp);
}

template <class T> T UTFMapping<T>::GetExistingVal(int cp)
{
	OP_ASSERT((unsigned int)cp < SPC_UTF_MAX_CODEPOINTS);
	int leaf_idx = cp>>SPC_UTF_LEAF_BITS;
	OP_ASSERT(m_leafs[leaf_idx] != 0xFFFF);
	return SPC_UTF_VALUE_AT_ALLOC_IDX(m_leafs[leaf_idx],cp);
}

template <class T> OP_STATUS UTFMapping<T>::AllocateLeafAt(int leaf_idx)
{
	if (m_leaf_count == 0xFFFE)
		m_status = SPC_ERROR(OpStatus::ERR);
	else if (!m_leaf_allocator->Allocate())
		m_status = SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	if (OpStatus::IsError(m_status))
		return m_status;
	m_leafs[leaf_idx] = m_leaf_count++;
	return OpStatus::OK;
}

template <class T> OP_STATUS UTFMapping<T>::SetVal(int cp, T val)
{
	OP_ASSERT((unsigned int)cp < SPC_UTF_MAX_CODEPOINTS);
	int leaf_idx = cp>>SPC_UTF_LEAF_BITS;
	if (m_leafs[leaf_idx] == 0xFFFF)
		RETURN_IF_ERROR(AllocateLeafAt(leaf_idx));
	SPC_UTF_VALUE_AT_ALLOC_IDX(m_leafs[leaf_idx],cp) = val;
	return OpStatus::OK;
}

template <class T> int UTFMapping<T>::CountNonZero()
{
	int i,j;
	int count = 0;
	for (i=0;i<SPC_UTF_LEAF_ARRAY_SIZE;i++)
	{
		if (m_leafs[i] == 0xFFFF)
			continue;
		T *leaf = SPC_UTF_LEAF_ADDR(m_leafs[i]);
		for (j=0;j<SPC_UTF_CODEPOINTS_PER_LEAF;j++)
		{
			if (leaf[j] != m_empty)
				count++;
		}
	}
	return count;
}

template <class T> void UTFMapping<T>::WriteOutNonZeroIndexesAndElements(void *dst)
{
	int i,j;
	UINT32 *_dst = (UINT32*)dst;
	for (i=0;i<SPC_UTF_LEAF_ARRAY_SIZE;i++)
	{
		if (m_leafs[i] == 0xFFFF)
			continue;
		T *leaf = SPC_UTF_LEAF_ADDR(m_leafs[i]);
		for (j=0;j<SPC_UTF_CODEPOINTS_PER_LEAF;j++)
		{
			if (leaf[j] != m_empty)
			{
				*(_dst++) = i*SPC_UTF_CODEPOINTS_PER_LEAF + j;
				T *t_dst = (T*)_dst;
				*t_dst = leaf[j];
				_dst = (UINT32*)((SPC_UINT_PTR)(_dst) + sizeof(T));
			}
		}
	}
}
#endif //!SPC_USE_HUNSPELL_ENGINE

/* ===============================SortedStringCollection=============================== */

SortedStringCollection::~SortedStringCollection()
{
	int i;
	if (!m_string_allocator)
		for (i=0;i<m_strings.GetSize();i++)
			OP_DELETEA(m_strings.GetElement(i));
	OP_ASSERT(m_ref == 0);
}

OP_STATUS SortedStringCollection::AddString(const uni_char *string, BOOL sort)
{
	int len = uni_strlen(string);
	uni_char *new_string = m_string_allocator ? (uni_char*)m_string_allocator->Allocate16((len+1)*sizeof(uni_char)) : OP_NEWA(uni_char, len+1);
	if (!new_string)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	uni_strcpy(new_string, string);
	if (m_strings.AddElement(new_string) < 0)
	{
		if (!m_string_allocator)
			OP_DELETEA(new_string);
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	if (!sort)
	{
		m_sorted = FALSE;
		return OpStatus::OK;
	}
	if (!m_sorted)
		return Sort();
	uni_char **str_array = m_strings.GetElementPtr(0);
	int old_count = m_strings.GetSize() - 1;
	BOOL has_string = FALSE;
	int idx = get_sorted_index(new_string, new_string + len, str_array, old_count, has_string);
	int i;
	for (i=old_count;i-1>idx;i--)
		str_array[i] = str_array[i-1];
	str_array[idx+1] = new_string;
	return OpStatus::OK;
}

BOOL SortedStringCollection::RemoveString(const uni_char *string)
{
	uni_char **str_array = m_strings.GetElementPtr(0);
	int count = m_strings.GetSize();
	BOOL has_string = FALSE;
	int idx = get_sorted_index(string, string + uni_strlen(string), str_array, count, has_string);
	if (!has_string)
		return FALSE;
	int start_remove = idx, end_remove = idx;
	while (start_remove > 0 && !uni_strcmp(string,str_array[start_remove-1]))
		start_remove--;
	while (end_remove < count-1 && !uni_strcmp(string,str_array[end_remove+1]))
		end_remove++;
	int remove_count = end_remove-start_remove + 1;
	int copy_count = count-end_remove - 1;
	int i;
	for (i=0;i<copy_count;i++)
	{
		if (!m_string_allocator)
			OP_DELETEA(str_array[start_remove+i]);
		str_array[start_remove+i] = str_array[start_remove+i+remove_count];
	}

	if (!m_string_allocator)
		for (i=start_remove+copy_count; i<=end_remove; i++)
		{
			OP_DELETEA(str_array[i]);
		}

	m_strings.Resize(count-remove_count);
	return TRUE;
}

BOOL SortedStringCollection::HasString(const uni_char *string, const uni_char *string_end)
{
	BOOL has_string = FALSE;

	if ( string == NULL )
		return FALSE;

	if ( string_end == NULL )
		string_end = string + uni_strlen(string);

	if (string == string_end || !m_strings.GetSize())
		return FALSE;

	if (!m_sorted)
	{
		if (OpStatus::IsError(Sort()))
			return FALSE;		
	}
	get_sorted_index(string, string_end, m_strings.GetElementPtr(0), m_strings.GetSize(), has_string);
	return has_string;
}

OP_STATUS SortedStringCollection::Sort()
{
	if (!m_strings.GetSize() || m_sorted)
	{
		m_sorted = TRUE;
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(sort_strings(m_strings.GetElementPtr(0), m_strings.GetSize()));
	m_sorted = TRUE;
	return OpStatus::OK;
}

/* ===============================SCUTF16ToUTF32Converter=============================== */

int SCUTF16ToUTF32Converter::Convert(const void *src, int len, void *dest, int maxlen, int *read)
{
	uni_char *data = (uni_char*)src;
	if (!len || len&1 || !maxlen || maxlen&3)
	{
		OP_ASSERT(FALSE);
		return 0;
	}
	uni_char *stop = data + len/2;
	UINT32 *result = (UINT32*)dest;
	int count = 0;
	while (data < stop)
	{
		UINT32 ch;
		if (Unicode::IsHighSurrogate(*data))
		{
			if (data+1 == stop || !Unicode::IsLowSurrogate(data[1]))
			{
				OP_ASSERT(FALSE);
				return 0;
			}
			ch = Unicode::DecodeSurrogate(*data, data[1]);
			data += 2;
		}
		else
			ch = (UINT32)*(data++);
		result[count++] = ch;
	}
	if (read)
		*read = (SPC_UINT_PTR)data - (SPC_UINT_PTR)src;
	return count * 4;
}

/* ===============================SCUTF32ToUTF16Converter=============================== */

int SCUTF32ToUTF16Converter::Convert(const void *src, int len, void *dest, int maxlen, int *read)
{
	int i;
	UINT32 *data = (UINT32*)src;
	if (!len || len&3 || !maxlen || maxlen&1)
	{
		OP_ASSERT(FALSE);
		return -1;
	}
	int count = len / 4;
	int out_pos;
	uni_char *result = (uni_char*)dest;
	for (i = 0, out_pos = 0; i < count && out_pos < maxlen;i++)
	{
		UINT32 ch = data[i];
		if (ch < 0x10000)
		{
			if (Unicode::IsSurrogate(ch))
			{
				OP_ASSERT(FALSE);
				return -1;
			}
			result[out_pos++] = (uni_char)ch;
		}
		else
		{
			if (out_pos == maxlen-1)
			{
				OP_ASSERT(FALSE);
				return -1;
			}
			uni_char high = 0, low = 0;
			Unicode::MakeSurrogate(ch, high, low);
			result[out_pos++] = high;
			result[out_pos++] = low;
		}
	}
	if (read)
		*read = len;
	return out_pos * 2;
}

#ifndef SPC_USE_HUNSPELL_ENGINE

template class UTFMapping<UINT16>;
template class UTFMapping<UINT32>;
template class UTFMapping<UINT16*>;
template class UTFMapping<MappingPointer>;

template void quick_sort(TempWordPointer *unsorted, int elements, void *buffer);
template void quick_sort(IndexCounterMapping *unsorted, int elements, void *buffer);
template void quick_sort(CompoundWordReplacement *unsorted, int elements, void *buffer);

template void rev_heap_add(WordReplacement elm, WordReplacement *heap, int elements);
template void rev_heap_replace_first(WordReplacement elm, WordReplacement *heap, int elements);


#endif //!SPC_USE_HUNSPELL_ENGINE
#endif // INTERNAL_SPELLCHECK_SUPPORT
