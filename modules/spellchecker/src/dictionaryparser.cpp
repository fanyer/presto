/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#ifndef SPC_USE_HUNSPELL_ENGINE

#include "modules/spellchecker/src/opspellchecker.h"

inline void OpSpellCheckerBitStream::WriteVal16NoOverByte(UINT16 val, int bits)
{
	OP_ASSERT(bits <= 16 && !(val & (-1<<bits)));
	*m_byte |= (UINT8)(val << (m_bit));
	val >>= 8 - m_bit;
	int bits_left_after_first = bits - (8 - m_bit);
	if (bits_left_after_first > 0)
	{
		m_byte[1] = val&0xFF;
		if (bits_left_after_first > 8)
			m_byte[2] = (val&0xFF00)>>8;
	}
	m_byte += (m_bit + bits)>>3;
	m_bit = (m_bit + bits)&7;
}

inline void OpSpellCheckerBitStream::WriteBit(BOOL set)
{
	if (set)
		*m_byte |= 1<<m_bit;
	else
		*m_byte &= ~(1<<m_bit);
	m_byte += ++m_bit >> 3;
	m_bit &= 7;
}

inline void OpSpellChecker::CountExtraInfo(TempWordInfo *word_info)
{
	UINT32 radix_flags = word_info->info.bit_flags & ((1<<SPC_RADIX_FLAGS)-1);
	m_flag_counters[radix_flags]++;
	if (m_compound_rule_flag_counters && word_info->info.compound_rule_flag_count)
	{
		int i;
		for (i=0;i<word_info->info.compound_rule_flag_count;i++)
			m_compound_rule_flag_counters[word_info->info.compound_rule_flags[i]]++;
	}
}

inline void OpSpellChecker::CountMappedChars(UINT32 *str, int str_len)
{
	int i = str_len;
	MappingPointer mp;
	if (m_8bit_enc)
	{
		while (i--)
		{
			mp = m_char_mappings.linear[str[i]];
			m_mappings.Data()[mp.mapping_idx].frequencies[mp.mapping_pos]++;
		}
	}
	else
	{
		while (i--)
		{
			mp = m_char_mappings.utf->GetExistingVal(str[i]);
			m_mappings.Data()[mp.mapping_idx].frequencies[mp.mapping_pos]++;
		}
	}
}

inline RadixEntry* OpSpellChecker::GetRadixEntry(UINT32 *idx_str, int len, int &level, OpSpellCheckerRadix *&parent)
{
	int i;
	if (m_radix_cache.valid && len > m_radix_cache.level)
	{
		i = m_radix_cache.level+1;
		while (i--)
		{
			if (idx_str[i] != m_radix_cache.indexes[i])
				break;
		}
		if (i == -1)
		{
			level = m_radix_cache.level;
			parent = m_radix_cache.parent;
			return m_radix_cache.entry;
		}
	}
	{
		m_radix_cache.entry = m_root_radix->GetRadixEntry(idx_str, len, level, parent);
		if (level < SPC_RADIX_CACHE_INDEXES && level != len)
		{
			m_radix_cache.valid = TRUE;
			op_memcpy(m_radix_cache.indexes, idx_str, sizeof(UINT32)*(level+1));
			m_radix_cache.level = level;
			m_radix_cache.parent = parent;
		}
		else
			m_radix_cache.valid = FALSE;
	}
	return m_radix_cache.entry;
}

inline void OpSpellChecker::ConvertCharsToIndexesAndMappings(UINT32 *chars, int len, IndexAndMapping *char_mapping, int &char_mapping_count)
{
	int i;
	MappingPointer mp;
	char_mapping_count = 0;
	for (i=0;i<len;i++)
	{
		mp = m_8bit_enc ? m_char_mappings.linear[chars[i]] : m_char_mappings.utf->GetExistingVal(chars[i]);
		chars[i] = mp.mapping_idx;
		if (mp.mapping_pos)
			char_mapping[char_mapping_count++] = IndexAndMapping((UINT16)i, mp.mapping_pos-1);
	}
}

inline void OpSpellChecker::ConvertCharsToIndexesFast(UINT32 *src, UINT32 *dst, int len)
{
	if (m_8bit_enc)
	{
		while (len--)
			dst[len] = (UINT32)m_char_mappings.linear[src[len]].mapping_idx;
	}
	else
	{
		while (len--)
			dst[len] = (UINT32)m_char_mappings.utf->GetVal(src[len]).mapping_idx;
	}
}

OP_STATUS OpSpellChecker::ProcessStemAndInflectionsPass1()
{
	int i;
	TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	TempWordInfo *word;
	OpSpellCheckerRadix *radix = NULL;
	RadixEntry *entry;
	int level = 0;
	m_total_word_count += tmp_data->valid_word_count;
	if (m_total_word_count > SPC_MAX_TOTAL_WORD_COUNT)
		return SPC_ERROR(OpStatus::ERR);

	TempWordInfo **valid_words = tmp_data->valid_words;
	i = tmp_data->valid_word_count;
	while (i--)
	{
		word = *(valid_words++);
		ConvertCharsToIndexesFast(word->word, word->word, word->word_len);
		entry = GetRadixEntry(word->word, word->word_len, level, radix);
		OP_ASSERT(!entry->IsRadixPtr());
		if (level != word->word_len) // not a "self counter"
		{
			entry->IncCount();
			if (entry->GetCount() == SPC_RADIX_ALLOCATION_COUNT)
			{
				radix = CreateRadix(radix);
				if (!radix)
					return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
				if (level != word->word_len-1) // not "self counter" for new radix
					radix->AtIndex(word->word[level])->SetCount(1);
				entry->SetRadix(radix);
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ProcessStemAndInflectionsPass2()
{
	int i,j,len;
	OpSpellCheckerRadix *dummy = NULL;
	TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	TempWordInfo *word;
	RadixEntry *entry;
	int level = 0;
	UINT32 *str;
	TempWordInfo **valid_words = tmp_data->valid_words;
	i = tmp_data->valid_word_count;
	while (i--)
	{
		word = *(valid_words++);
		str = word->word;
		len = word->word_len;
		if (!(i&0xF))
		{
			CountMappedChars(str, len);
			CountExtraInfo(word);
		}
		ConvertCharsToIndexesFast(str, str, len);
#ifdef SPC_SETUP_CHAR_COMBINATIONS
		SetIndexCombinations(str, len);
#endif // SPC_SETUP_CHAR_COMBINATIONS
		SetHashBit(GetHashBitFor(str, len));
		entry = GetRadixEntry(str, len, level, dummy);
		OP_ASSERT(!entry->IsRadixPtr());
		if (entry->GetCount() == SPC_MAX_RADIX_COUNTER-1) // we can't handle so large count...
			return SPC_ERROR(OpStatus::ERR);
		entry->IncCount();
		if (!(i&0xF) && level != len)
		{
			j = len-level;
			str += level;
			while (j--)
				m_index_counters[*(str++)]++;
			(*m_index_counters)++; // count termination char
		}
	}
	return OpStatus::OK;
}

OpSpellCheckerRadix *OpSpellChecker::CreateRadix(OpSpellCheckerRadix *parent)
{ 
	SPC_UINT_PTR *radix = (SPC_UINT_PTR*)m_allocator.AllocatePtr(m_mappings.GetSize()*sizeof(RadixEntry)+(2*sizeof(SPC_UINT_PTR)));
	if (radix)
	{
		radix[0] = (SPC_UINT_PTR)parent;
		radix[1] = SPC_RADIX_START_MARKER;
		radix = &radix[2];
	}
	m_radix_cache.valid = FALSE;
	return (OpSpellCheckerRadix*)radix;
}

#ifdef OPERA_BIG_ENDIAN
#define FLIP_BYTES_IF_BIG_ENDIAN(x) \
{ \
	int f1,f2; \
	UINT8 *flip_ptr = (UINT8*)(&x); \
	UINT8 f_tmp; \
	for (f1=0,f2=sizeof(x)-1;f1<f2;f1++,f2--) \
	{ \
		f_tmp = flip_ptr[f1]; \
		flip_ptr[f1] = flip_ptr[f2]; \
		flip_ptr[f2] = f_tmp; \
	} \
}
#else // OPERA_BIG_ENDIAN
#define FLIP_BYTES_IF_BIG_ENDIAN(x)
#endif // !OPERA_BIG_ENDIAN

#define WRITE_HUFFMAN_BITS(_entry) \
{ \
	entry = _entry; \
	bits_left -= entry.code_length; \
	if (bits_left <= 0) return FALSE; \
	OP_ASSERT(entry.code_length); \
	bit_stream.WriteVal16NoOverByte(entry.value, entry.code_length); \
}

// No need of rewinding bit_stream upon failure.
inline BOOL OpSpellChecker::WriteExtraWordInfo(TempWordInfo *word_info, OpSpellCheckerBitStream &bit_stream, UINT8 *end_write)
{
	int bits_left = -bit_stream.GetBitCountFrom(end_write) - 1; // make sure one extra bit is saved for has-more-words-in-unit bit
	HuffmanEntry entry;
	if (m_has_radix_flags)
		WRITE_HUFFMAN_BITS(m_flags_to_huffman[word_info->info.bit_flags & ((1<<SPC_RADIX_FLAGS)-1)]);
	if (m_has_compound_rule_flags)
	{
		int i;
		for (i=0;i<word_info->info.compound_rule_flag_count;i++)
		{
			if (!bits_left)
				return FALSE;
			bit_stream.WriteBit(TRUE); // here comes another compound-rule-flag
			WRITE_HUFFMAN_BITS(m_compound_rule_flag_to_huffman[word_info->info.compound_rule_flags[i]]);
		}
		if (!bits_left)
			return FALSE;
		bit_stream.WriteBit(FALSE); // no more compound-rule-flags
		
	}
	return TRUE;
}

// No need of rewinding bit_stream upon failure.
inline BOOL OpSpellChecker::WriteMappingInfo(TempWordInfo *word_info, OpSpellCheckerBitStream &bit_stream, UINT8 *end_write)
{
	int bits_left = -bit_stream.GetBitCountFrom(end_write) - 1; // make sure one extra bit is saved for has-more-words-in-unit bit
	if (word_info->mapped_char_count)
	{
		int i, map_pos_bits;
		IndexAndMapping idx_map;
		int str_pos_bits = m_len_to_bit_representation[word_info->word_len];
		UINT32 *word = word_info->word;
		for (i=0;i<word_info->mapped_char_count;i++)
		{
			idx_map = word_info->mapped_chars[i];
			map_pos_bits = m_mappings.Data()[word[idx_map.index]].bit_representation;
			bits_left -= 1 + str_pos_bits + map_pos_bits;
			if (bits_left <= 0)
				return FALSE;
			bit_stream.WriteBit(TRUE); // there are more mappings
			bit_stream.WriteVal16NoOverByte(idx_map.index, str_pos_bits); // write position in "index string"
			if (map_pos_bits) // will be 0 for "pair-mappings", e.g. (A,a)
				bit_stream.WriteVal16NoOverByte(idx_map.mapping, map_pos_bits); // write position inside mapping (excluding the most frequent char)
		}
	}
	if (!bits_left)
		return FALSE;
	bit_stream.WriteBit(FALSE); // there are no more mappings
	return TRUE;
}

// No need of rewinding bit_stream upon failure.
inline BOOL OpSpellChecker::WriteCompressedIndexes(UINT32 *indexes, int len, OpSpellCheckerBitStream &bit_stream, UINT8 *end_write)
{
	int i;
	int bits_left = -bit_stream.GetBitCountFrom(end_write) - 1; // make sure one extra bit is saved for has-more-words-in-unit bit
	HuffmanEntry entry;
	for (i=0;i<len;i++)
		WRITE_HUFFMAN_BITS(m_index_to_huffman[indexes[i]]);
	WRITE_HUFFMAN_BITS(m_index_to_huffman[0]);
	return TRUE;
}

inline BOOL OpSpellChecker::WriteUsingPrevWord(TempWordInfo *word_info, OpSpellCheckerBitStream &bit_stream, 
	UINT8 *end_write, BOOL self_entry, int same_len, UINT32 *prev_end, int prev_end_len)
{
	OpSpellCheckerBitStream stream = bit_stream;
	if (!self_entry)
	{
		int copy;
		UINT32 *word_end = word_info->word + same_len;
		int word_end_len = word_info->word_len - same_len;
		int compare_len = MIN(MIN(word_end_len, prev_end_len), 1<<SPC_COPY_PREV_WORD_BITS);
		for (copy = 0; copy < compare_len && word_end[copy] == prev_end[copy]; copy++);
		if (stream.Byte() == end_write)
			return FALSE;
		stream.WriteBit(!!copy); // write if we should copy from previous word or not
		if (copy)
		{
			if (stream.GetBitCountFrom(end_write) > -(SPC_COPY_PREV_WORD_BITS+1)) // make sure one extra bit is saved for has-more-words-in-unit bit
				return FALSE;
			stream.WriteVal16NoOverByte(copy-1, SPC_COPY_PREV_WORD_BITS); // write number of indexes to copy -1
		}
		if (!WriteCompressedIndexes(word_end+copy, word_end_len-copy, stream, end_write)) // write the remaining of the string
			return FALSE;
	}
	if (!WriteMappingInfo(word_info, stream, end_write))
		return FALSE;
	if (!WriteExtraWordInfo(word_info, stream, end_write))
		return FALSE;
	bit_stream = stream;
	return TRUE;
}

inline BOOL OpSpellChecker::WriteAllOfWord(TempWordInfo *word_info, OpSpellCheckerBitStream &bit_stream, BOOL self_entry, int ignore_len, UINT8 *end_write)
{
	OpSpellCheckerBitStream stream = bit_stream;
	if (!self_entry)
	{
		if (stream.Byte() == end_write)
			return FALSE;
		stream.WriteBit(FALSE); // we should NOT copy anything from previous word 
		if (!WriteCompressedIndexes(word_info->word + ignore_len, word_info->word_len - ignore_len, stream, end_write))
			return FALSE;
	}
	if (!WriteMappingInfo(word_info, stream, end_write))
		return FALSE;
	if (!WriteExtraWordInfo(word_info, stream, end_write))
		return FALSE;
	bit_stream = stream;
	return TRUE;
}

inline BOOL OpSpellChecker::WriteNextInfoBits(UINT8* data, OpSpellCheckerBitStream &bit_stream, 
	int left_to_write, UINT8 *end_write, UINT8 **new_end_write, UINT32 *prev_word_end, int prev_word_end_len)
{
	BOOL use_first_word_for_continue = prev_word_end == NULL;
	if (bit_stream.Byte() == end_write)
		return FALSE;
	bit_stream.WriteBit(use_first_word_for_continue);
	int bits_back = bit_stream.Bit() ? 8 - bit_stream.Bit() : 0;
	bits_back += bit_stream.GetByteCountFrom(data) & 1 ? 8 : 0;
	bit_stream.IncBits(bits_back);
	OP_ASSERT(!(bit_stream.GetByteCountFrom(data) & 1) && !bit_stream.Bit()); // the offset from data should now be 2 byte aligned
	if (bit_stream.Byte() + 4 >= end_write)
		return FALSE;
	OpSpellCheckerBitStream pointer(data, 1);
	void *ofs_as_pointer = (void*)(bit_stream.GetByteCountFrom(data)>>1);
	FLIP_BYTES_IF_BIG_ENDIAN(ofs_as_pointer);
	pointer.WriteDataNoOverWrite((UINT8*)(&ofs_as_pointer), sizeof(void*)*8); // write offset from data / 2;
	UINT8 *info_start = bit_stream.Byte();
	bit_stream.WriteVal16NoOverByte((UINT16)bits_back,4); // write free bits before this info
	bit_stream.WriteVal16NoOverByte((UINT16)left_to_write,12); // write words left
	pointer = bit_stream;
	bit_stream.IncBits(13); // write bytes left later, when we're sure...
	if (!use_first_word_for_continue) // only write end of last word if there are > 1 words here
	{
		if (!WriteCompressedIndexes(prev_word_end, prev_word_end_len, bit_stream, end_write))
			return FALSE;
	}
	int extra_bytes;
	if (new_end_write) // the caller want this function to decide the end of the word unit
	{
		extra_bytes = left_to_write * SPC_ALLOCATE_BYTES_PER_UNKNOWN_WORD;
		extra_bytes = MAX(extra_bytes, (int)((bit_stream.Byte() + (bit_stream.Bit() ? 1 : 0)) - info_start));
		extra_bytes = MIN(extra_bytes, (int)(end_write-data));
		*new_end_write = info_start + extra_bytes;
	}
	else // the allocation for this word unit is already fixed, and it ends at end_write
		extra_bytes = (int)(end_write-info_start);
	FLIP_BYTES_IF_BIG_ENDIAN(extra_bytes);
	pointer.WriteDataNoOverWrite((UINT8*)(&extra_bytes),13);
	return TRUE;
}

OP_STATUS OpSpellChecker::ConstructWordUnit(TempWordInfo **related_words, BOOL self_entry, int in_flow, int count, int same_len, UINT8 *&data, BOOL is_continuing, UINT32 *prev_end, int prev_end_len)
{
	int i;
	BOOL result;
	TempWordInfo *word_info = related_words[0];
	UINT32 *word = word_info->word;
	int len = word_info->word_len;
	if (is_continuing)
		data = (UINT8*)m_allocator.Allocate8(SPC_COMPRESSED_WORDS_ALLOCATE);
	else
		data = (UINT8*)m_allocator.Allocate16(SPC_COMPRESSED_WORDS_ALLOCATE); // address to data in radix must NOT have it's lowest bit set, for avoiding confusion with radix-addresses
	if (!data)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	OpSpellCheckerBitStream bit_stream = OpSpellCheckerBitStream(data);
	UINT8 *end_write = data + SPC_COMPRESSED_WORDS_ALLOCATE;
	if (in_flow == count) // all words are here...
	{
		bit_stream.WriteBit(FALSE); // no next-pointer / offset
		bit_stream.WriteBit(TRUE); // has-more-words bit, necessary also before first word...
		if (is_continuing)
			result = WriteUsingPrevWord(word_info, bit_stream, end_write, self_entry, same_len, prev_end, prev_end_len); // should NOT overflow
		else
			result = WriteAllOfWord(word_info,bit_stream, self_entry,same_len, end_write); // should NOT overflow
		if (!result)
		{
			OP_ASSERT(!"Should not be possible!");
			return SPC_ERROR(OpStatus::ERR);
		}
		prev_end = word + same_len;
		prev_end_len = len - same_len;
		for (i=1;i<in_flow;i++)
		{
			bit_stream.WriteBit(TRUE); // YES, there are more words
			if (!WriteUsingPrevWord(related_words[i], bit_stream, end_write, self_entry,same_len, prev_end, prev_end_len))
				return SPC_ERROR(OpStatus::ERR); // we need increase SPC_COMPRESSED_WORD_ALLOCATE for this dictionary
			prev_end = related_words[i]->word + same_len;
			prev_end_len = related_words[i]->word_len - same_len;
		}
		bit_stream.WriteBit(FALSE); // no more words...
		m_allocator.OnlyUsed(bit_stream.GetCurrentBytes(), FALSE);
	}
	else // not all words here, we need add offset to info + info
	{
		bit_stream.WriteBit(TRUE); // there is a next-pointer / offset
		bit_stream.IncBits(sizeof(void*)*8); // save space for next-pointer / temporary info-offset 
		bit_stream.WriteBit(TRUE); // has-more-words bit, necessary also before first word...
		
		if (is_continuing)
			result = WriteUsingPrevWord(word_info, bit_stream, end_write, self_entry, same_len, prev_end, prev_end_len); // should NOT overflow
		else
			result = WriteAllOfWord(word_info, bit_stream, self_entry, same_len, end_write); // should NOT overflow
		if (!result)
		{
			OP_ASSERT(!"Should not be possible!");
			return SPC_ERROR(OpStatus::ERR);
		}

		prev_end = word + same_len;
		prev_end_len = len - same_len;
		for (i=1;i<in_flow;i++)
		{
			bit_stream.WriteBit(TRUE); // YES, there are more words
			if (!WriteUsingPrevWord(related_words[i], bit_stream, end_write, self_entry, same_len, prev_end, prev_end_len))
				return SPC_ERROR(OpStatus::ERR); // we need increase SPC_COMPRESSED_WORD_ALLOCATE for this dictionary
			prev_end = related_words[i]->word + same_len;
			prev_end_len = related_words[i]->word_len - same_len;
		}
		bit_stream.IncBits(1); // we don't know for sure yet if there will be more words in this unit, so leave it for later...
		UINT8 *new_end_write = NULL;
		if (!is_continuing && in_flow == 1 || self_entry) // TRUE if the first word should be used when continuing adding words in this unit
		{
			prev_end = NULL;
			prev_end_len = 0;
		}
		if (!WriteNextInfoBits(data, bit_stream, count-in_flow, end_write, &new_end_write, prev_end, prev_end_len))
			return SPC_ERROR(OpStatus::ERR); // we need increase SPC_COMPRESSED_WORD_ALLOCATE for this dictionary
		m_allocator.OnlyUsed((int)(new_end_write-data), FALSE);
	}
	return OpStatus::OK;
}

static inline BOOL word_infos_equal(TempWordInfo *a, TempWordInfo *b)
{
	if (a->word_len != b->word_len)
		return FALSE;
	if (!indexes_equal(a->word, b->word, a->word_len))
		return FALSE;
	if ((a->info.bit_flags ^ b->info.bit_flags) & (1<<SPC_RADIX_FLAGS)-1)
		return FALSE;
	if (a->info.compound_rule_flag_count != b->info.compound_rule_flag_count)
		return FALSE;
	if (a->info.compound_rule_flag_count && op_memcmp(a->info.compound_rule_flags, b->info.compound_rule_flags, a->info.compound_rule_flag_count*sizeof(UINT16)))
		return FALSE;
	if (a->mapped_char_count != b->mapped_char_count)
		return FALSE;
	if (a->mapped_char_count && op_memcmp(a->mapped_chars, b->mapped_chars, sizeof(IndexAndMapping)*a->mapped_char_count))
		return FALSE;
	return TRUE;
}

OP_STATUS OpSpellChecker::ProcessStemAndInflectionsPass3()
{
	int i, j, len, level, prev_end_len = 0, mapping_count = 0;
	UINT32 *word, *prev_end = NULL;
	OpSpellCheckerRadix *dummy = NULL;
	UINT32 *word_buf = (UINT32*)TempBuffer();
	TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	int word_count = tmp_data->valid_word_count;
	TempWordInfo **word_pointers = tmp_data->valid_words;
	TempWordInfo **related_words;
	TempWordInfo *word_info;
	IndexAndMapping *mapping;
	RadixEntry *entry;
	UINT8 *data = NULL, *next_data = NULL, *info_start;
	for (i=0;i<word_count;i++)
	{
		word_info = word_pointers[i];
		if (tmp_data->index_and_mapping_ofs + word_info->word_len > SPC_TEMP_INDEX_AND_MAPPING_BUF_SZ)
			return SPC_ERROR(OpStatus::ERR);
		mapping = tmp_data->index_and_mapping_buf + tmp_data->index_and_mapping_ofs;
		ConvertCharsToIndexesAndMappings(word_info->word, word_info->word_len, mapping, mapping_count);
		if (mapping_count)
		{
			word_info->mapped_chars = mapping;
			word_info->mapped_char_count = mapping_count;
			tmp_data->index_and_mapping_ofs += mapping_count;
		}
	}

#ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	quick_sort<TempWordPointer>((TempWordPointer*)word_pointers, word_count, tmp_data->sort_buf);
	for (i=1,j=0;i<word_count;i++) // remove clones...
		if (!word_infos_equal(word_pointers[i], word_pointers[j]))
			word_pointers[++j] = word_pointers[i];
	word_count = j + 1;
#endif // SPC_SORT_PARTIALLY_FOR_COMPRESSION
	
	m_stat_count_total += word_count;

	int count, in_flow, same_len;
	BOOL self_entry;
	UINT8 *end_write;
	OpSpellCheckerBitStream bit_stream;
	for (i = 0; i < word_count; i += in_flow)
	{
		word_info = word_pointers[i];
		word = word_info->word;
		len = word_info->word_len;
		entry = m_root_radix->GetRadixEntry(word, len, level, dummy);
		self_entry = len == level;
		if (self_entry) // count "identical" entrys
		{
			same_len = level;
			for (j=i+1;j<word_count;j++)
				if (word_pointers[j]->word_len != same_len || !indexes_equal(word_pointers[j]->word, word, same_len))
					break;
		}
		else // count entry with same beginning
		{
			same_len = level+1;
			for (j=i+1;j<word_count;j++)
				if (word_pointers[j]->word_len < same_len || !indexes_equal(word_pointers[j]->word, word, same_len))
					break;
		}
		in_flow = MIN(j-i, SPC_MAX_WORDS_PER_COMPRESSED_UNIT);
		related_words = word_pointers + i;
		if (entry->IsCounter()) // first time we're here
		{
			count = entry->GetCount();
			OP_ASSERT(count);
			
			m_stat_counters++;
			m_stat_max_counter = MAX(m_stat_max_counter, (UINT32)count);
			
			RETURN_IF_ERROR(ConstructWordUnit(related_words, self_entry, in_flow, count, same_len, data));
			entry->SetBits(data);
			continue;
		}
		OP_ASSERT(!entry->IsRadixPtr());
		data = entry->GetBits();
		for (;;)
		{
			OP_ASSERT(*data & 1); // this unit must have a next-pointer / offset
			bit_stream.SetStart(data,1);
			void *ptr = bit_stream.ReadPointer();
			if (!SPC_IS_NEXT_UNIT_POINTER(ptr))
			{
				info_start = data + (((SPC_UINT_PTR)ptr)<<1);
				break;
			}
			data = (UINT8*)ptr;
		}
		bit_stream = OpSpellCheckerBitStream(info_start);
		int bits_back = bit_stream.ReadVal16(4); // empty bits before info_start and after more-words-in-unit bit for last previous word + use-first-word-when-continuing bit
		count = bit_stream.ReadVal16(12); // number of words left totally for radix entry
		OP_ASSERT(in_flow <= count);
		int bytes_left = bit_stream.ReadVal16(13);
		BOOL use_first_word = TRUE;
		OpSpellCheckerBitStream pointer = bit_stream;
		if (!self_entry) // set previous word-end that we should continue from
		{
			bit_stream.SetStart(info_start,-(bits_back+1)); // "rewind" back to use-first-word-when-continuing bit
			use_first_word = bit_stream.ReadBit();
			prev_end = word_buf;
			if (use_first_word)
				bit_stream.SetStart(data, 1+sizeof(void*)*8+1+1); // data + next-pointer-/address bit + next-pointer-/address bit + more-words-in-unit bit + copy-from-previous bit
			else
				bit_stream.SetStart(info_start, 4+12+13);
			ReadCompressedIndexes(bit_stream, prev_end, prev_end_len);
			if (!use_first_word)
				pointer = bit_stream;
		}
		op_memset(info_start, 0, pointer.GetByteCountFrom(info_start)); // clear memory for the cached info
		bit_stream.SetStart(info_start, -(bits_back+2)); // we will start by outputting more-words-in-this-unit bit TRUE for the previous word in this unit
		
		// Somewhat convoluted way to zero out the bits before info_start
		pointer = bit_stream;
		UINT32 zero = 0;
		pointer.WriteDataNoOverWrite((UINT8*)&zero, bits_back+2);
		
		end_write = info_start + bytes_left;
		for (j=0;j<in_flow;j++)
		{
			bit_stream.WriteBit(TRUE); // YES, there are more words
			if (!WriteUsingPrevWord(related_words[j], bit_stream, end_write, self_entry, same_len, prev_end, prev_end_len))
			{ // no more space, let's put everything left in a new unit
				bit_stream.IncBits(-1);
				bit_stream.WriteBit(FALSE); // we changed our mind, there are NO more words in this unit
				RETURN_IF_ERROR(ConstructWordUnit(&related_words[j], self_entry, in_flow-j, count-j, same_len, next_data, TRUE, prev_end, prev_end_len));
				FLIP_BYTES_IF_BIG_ENDIAN(next_data);
				bit_stream.SetStart(data,1);
				bit_stream.WriteDataNoOverWrite((UINT8*)(&next_data),sizeof(void*)*8);
				break;
			}
			prev_end = related_words[j]->word + same_len;
			prev_end_len = related_words[j]->word_len - same_len;
		}
		if (j != in_flow) // all words didn't fit in and we constructed a new unit above -> there should be no next-unit info
			continue;
		if (in_flow == count) // we're finished with this radix-entry...
		{
			bit_stream.WriteBit(FALSE); // ...so no more words
			continue;
		}
		pointer = bit_stream;
		bit_stream.IncBits(1); // we don't know for sure yet if there will be more words in this unit, so leave it for later...
		if (self_entry)
		{
			prev_end = NULL;
			prev_end_len = 0;
		}
		if (WriteNextInfoBits(data, bit_stream, count-in_flow, end_write, NULL, prev_end, prev_end_len))
			continue; // we succeeded writing the info and can continue...
		pointer.WriteBit(FALSE); // now we know there won't be any more words in this unit
		next_data = (UINT8*)m_allocator.Allocate8(SPC_COMPRESSED_WORDS_ALLOCATE); // allocate next word unit
		if (!next_data)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		UINT8 *temp_data = next_data;
		FLIP_BYTES_IF_BIG_ENDIAN(temp_data);
		bit_stream.SetStart(data,1);
		bit_stream.WriteDataNoOverWrite((UINT8*)(&temp_data),sizeof(void*)*8); // set pointer to next unit
		bit_stream.SetStart(next_data,0);
		bit_stream.WriteBit(TRUE); // there is a next-pointer / offset (will be offset)
		bit_stream.IncBits(sizeof(void*)*8); // save space for next-pointer / temporary info-offset 
		bit_stream.IncBits(1); // we're not 100% sure there will acutally be any word in this new unit
		if (!WriteNextInfoBits(next_data, bit_stream, count-in_flow, next_data+SPC_COMPRESSED_WORDS_ALLOCATE, &end_write, prev_end, prev_end_len))
			return SPC_ERROR(OpStatus::ERR);
		m_allocator.OnlyUsed((int)(end_write-next_data), FALSE);
	}
	ReleaseTempBuffer();
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryParsePass0()
{
	UINT8 *word = m_dic_parse_state->GetCurrentWord();
	if (m_8bit_enc)
	{
		while (*word)
		{
			m_existing_chars.linear[*(word++)]++;
			m_chars_in_dic_data++;
		}			
	}
	else
	{
		UINT32 ch;
		int bytes_used = 0;
		int old_count;
		while (*word)
		{
			ch = utf8_decode(word,bytes_used);
			if ((INT32)ch < 0)
				return SPC_ERROR(OpStatus::ERR);
			old_count = m_existing_chars.utf->GetVal(ch);
			RETURN_IF_ERROR(m_existing_chars.utf->SetVal(ch, old_count+1));
			m_chars_in_dic_data++;
			word += bytes_used;
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryParsePass1()
{
	int i,j;
	UINT8 *flag_buf = NULL;
	UINT8 *word = m_dic_parse_state->GetCurrentWord();
	UINT8 *flags = m_dic_parse_state->GetCurrentFlags();
	UINT16 *flag_ptr = (UINT16*)TempBuffer();
	TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	RETURN_IF_ERROR(GetFlagPtr(flags,flag_ptr,m_affix_file_data,NULL));
	FlagPtrInfo flag_info;
	flag_info.SetValuesFrom(flag_ptr);
	RETURN_IF_ERROR(tmp_data->SetStem(word,&flag_info));
	if (flag_ptr)
	{
		OpSpellCheckerAllocator *allocator = m_dic_file_data->Allocator();
		flag_buf = (UINT8*)allocator->Allocate8(SPC_MAX_FLAG_BUF_SIZE);
		if (!flag_buf)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		OpSpellCheckerBitStream stream(flag_buf);
		stream.WriteVal16(flag_info.bit_flags, sizeof(UINT16)*8); // write bit flags
		
		if (m_has_compound_rule_flags)
		{
			if (flag_info.compound_rule_flag_count) // write compound rule flags
			{
				for (i=0;i<flag_info.compound_rule_flag_count;i++)
				{
					stream.WriteBit(TRUE); // another compound rule flag...
					stream.WriteVal16(flag_info.compound_rule_flags[i], m_affix_file_data->GetFlagTypeBits(AFFIX_TYPE_COMPOUNDRULEFLAG));
				}
			}
			stream.WriteBit(FALSE); // no more compound rule flags
		}

		OpSpellCheckerAffix *affix;
		int rule_count = 0;
		for (i=0;i<flag_info.affix_count;i++)
		{
			UINT16 id = flag_info.affixes[i];
			affix = (OpSpellCheckerAffix*)m_affix_file_data->GetFlagClassByGlobalId(id);
			m_affix_file_data->ConvertStrToAffixChars(tmp_data->stem, tmp_data->stem_len, tmp_data->affix_char_buf, affix);
			int *rule_indexes = tmp_data->rule_match_buf;
			affix->GetMatchingRules(tmp_data->affix_char_buf, tmp_data->affix_match_bit_buf , rule_indexes, rule_count);
			if (rule_count)
			{
				stream.WriteBit(TRUE); // more affixes...
				stream.WriteVal16(id, m_affix_file_data->GetGlobalIdBits()); // write affix global id
				for (j=0;j<rule_count;j++)
					RETURN_IF_ERROR(tmp_data->AddStemInflection(affix,affix->GetRulePtr(rule_indexes[j])));
				if (rule_count <= SPC_MAX_RULE_PER_AFFIX_CACHE)
				{
					for (j=0;j<rule_count;j++)
					{
						stream.WriteBit(TRUE); // here comes a cached rule...
						stream.WriteVal32((UINT32)rule_indexes[j],affix->GetRuleCountBits()); // write the rule index
					}
				}
				stream.WriteBit(FALSE); // no more cached rules
			}
		}
		if (flag_info.affix_count)
			stream.WriteBit(FALSE); // no more affixes
		int flag_string_bytes = op_strlen((const char*)flags) + 1;
		int new_flag_bytes = stream.GetCurrentBytes();
		if (new_flag_bytes <= flag_string_bytes) // we can store the parsed flag at the location for the unparsed ones
		{
			op_memcpy(flags,flag_buf,new_flag_bytes);
			allocator->OnlyUsed(0);
		}
		else
		{
			allocator->OnlyUsed(new_flag_bytes);
			m_dic_parse_state->SetNewCurrentFlags(flag_buf);
		}
	}
	else
		m_dic_parse_state->SetNewCurrentFlags(NULL);
	ReleaseTempBuffer();
	RETURN_IF_ERROR(tmp_data->CreateCombines());
	RETURN_IF_ERROR(ProcessStemAndInflectionsPass1());
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryParsePass2()
{
	//UINT8 *word = m_dic_parse_state->GetCurrentWord();
	//UINT8 *flags = m_dic_parse_state->GetCurrentFlags();
	//TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	//RETURN_IF_ERROR(tmp_data->SetStem(word,flags,TRUE));
	//RETURN_IF_ERROR(tmp_data->CreateCombines());
	//RETURN_IF_ERROR(ProcessStemAndInflectionsPass2());
	//return OpStatus::OK;
	UINT8 *word = m_dic_parse_state->GetCurrentWord();
	UINT8 *flags = m_dic_parse_state->GetCurrentFlags();
	TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	RETURN_IF_ERROR(tmp_data->SetStem(word,flags,TRUE));
	RETURN_IF_ERROR(tmp_data->CreateCombines());
	m_dic_parse_state->NextLine();
	while (m_dic_parse_state->HasMoreLines() && tmp_data->ReadyForAnotherDictionaryWord())
	{
		word = m_dic_parse_state->GetCurrentWord();
		flags = m_dic_parse_state->GetCurrentFlags();
		RETURN_IF_ERROR(tmp_data->SetStem(word,flags,FALSE));
		RETURN_IF_ERROR(tmp_data->CreateCombines());
		m_dic_parse_state->NextLine();
	}
	RETURN_IF_ERROR(ProcessStemAndInflectionsPass2());
	return OpStatus::OK;

}

OP_STATUS OpSpellChecker::DictionaryParsePass3()
{
	UINT8 *word = m_dic_parse_state->GetCurrentWord();
	UINT8 *flags = m_dic_parse_state->GetCurrentFlags();
	TempDictionaryParseData *tmp_data = m_dic_file_data->GetTempData();
	RETURN_IF_ERROR(tmp_data->SetStem(word,flags,TRUE));
	RETURN_IF_ERROR(tmp_data->CreateCombines());
	m_dic_parse_state->NextLine();
	while (m_dic_parse_state->HasMoreLines() && tmp_data->ReadyForAnotherDictionaryWord())
	{
		word = m_dic_parse_state->GetCurrentWord();
		flags = m_dic_parse_state->GetCurrentFlags();
		RETURN_IF_ERROR(tmp_data->SetStem(word,flags,FALSE));
		RETURN_IF_ERROR(tmp_data->CreateCombines());
		m_dic_parse_state->NextLine();
	}
	RETURN_IF_ERROR(ProcessStemAndInflectionsPass3());
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryPostProcessPass0()
{
	int i,j;
	RETURN_IF_ERROR(SetupApproxFreqenceOfExisting());
	if (m_approx_freq_len < 2)
		return SPC_ERROR(OpStatus::ERR);
	int min_usual = m_chars_in_dic_data/SPC_MIN_USUAL_DIVIDE;
	int counted = 0;
	for (i=0;i<m_approx_freq_len;i++)
	{
		counted += m_approx_freq[i].count;
		if (m_chars_in_dic_data/counted < SPC_MIN_USUAL_FACTOR)
			min_usual = MIN((UINT32)min_usual, m_approx_freq[i].count);
		UINT32 ch = m_approx_freq[i].index, other_ch = 0;
		RETURN_IF_ERROR(GetDifferentCaseChar(ch, other_ch));
		if (other_ch)
			RETURN_IF_ERROR(SetupPairMapping(ch, other_ch));
	}
	m_unusual_mapping_index = m_mappings.GetSize();
	for (i=0;i<m_approx_freq_len;i++)
	{
		if (!GetCharMapping(m_approx_freq[i].index).IsPointer())
		{
			if (m_approx_freq[i].count >= (UINT32)min_usual)
				RETURN_IF_ERROR(SetupSingleMapping(m_approx_freq[i].index));
			else
				RETURN_IF_ERROR(MoveToUnusualMapping(m_approx_freq[i].index));
		}
	}
	for (i=1;i<m_mappings.GetSize();i++)
	{
		MapMapping *mapping = m_mappings.GetElementPtr(i);
		int total = 0;
		for (j=0;j<mapping->char_count;j++)
		{
			total += GetApproxExistCountFor(mapping->chars[j]);
			if (total >= min_usual)
				break;
		}
		if (j==mapping->char_count)
		{
			for (j=0;j<mapping->char_count;j++)
				RETURN_IF_ERROR(MoveToUnusualMapping(mapping->chars[j]));
			mapping->exists = FALSE;
		}
		else
			mapping->exists = TRUE;
	}
	RETURN_IF_ERROR(AppendUnusualMapping());
	int exist_count = 1;
	for (i=1;i<m_mappings.GetSize();i++)
	{
		MapMapping *mapping = m_mappings.GetElementPtr(i);
		if (mapping->exists)
		{
			for (j=0;j<mapping->char_count;j++)
			{
				if (IsExistingChar(mapping->chars[j]))
				{
					if (j) // make so that the first char is an existing one...
					{
						UINT32 non_existing = mapping->chars[0];
						UINT32 existing = mapping->chars[j];
						mapping->chars[0] = existing;
						mapping->chars[j] = non_existing;
					}
					break;
				}
			}
			m_mappings.Data()[exist_count++] = m_mappings.Data()[i];
		}
	}
	m_mappings.Resize(exist_count);
	m_unusual_mapping_index = exist_count-1;
	if (m_mappings.GetSize() > SPC_MAX_INDEXES || m_mappings.GetSize() < 3)
		return SPC_ERROR(OpStatus::ERR);

	for (i=1;i<m_mappings.GetSize();i++) // re-map all characters
	{
		MapMapping *mapping = m_mappings.GetElementPtr(i);
		for (j=0;j<mapping->char_count;j++)
			RETURN_IF_ERROR(AddToCharMappings(mapping->chars[j], i, j));
	}

	// Setup length -> bit representation bits
	m_len_to_bit_representation.SetInitCapacity(SPC_MAX_DIC_FILE_WORD_LEN);
	if (m_len_to_bit_representation.AddElement(0) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	for (i=1;i<SPC_MAX_DIC_FILE_WORD_LEN;i++)
		m_len_to_bit_representation.AddElement(bits_to_represent(i));
	
	ConvertRepStrings();
	if ((m_root_radix = CreateRadix(NULL)) == NULL)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);

#ifdef SPC_SETUP_CHAR_COMBINATIONS
	RETURN_IF_ERROR(SetupIndexCombinations());
#endif // SPC_SETUP_CHAR_COMBINATIONS

	if (m_8bit_enc)
		OP_DELETEA(m_existing_chars.linear);
	else
		OP_DELETE(m_existing_chars.utf);
	m_existing_chars.data = NULL;
	m_approx_freq_len = 0;
	OP_DELETEA(m_approx_freq);
	m_approx_freq = NULL;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryPostProcessPass1()
{
	int compound_rule_flags = m_affix_file_data->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG);
	if (compound_rule_flags)
	{
		m_compound_rule_flag_counters = OP_NEWA(int, compound_rule_flags);
		if (!m_compound_rule_flag_counters)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		op_memset(m_compound_rule_flag_counters,0,sizeof(int)*(compound_rule_flags));
	}

	m_index_counters = OP_NEWA(int, m_mappings.GetSize());
	m_flag_counters = OP_NEWA(int, 1<<SPC_RADIX_FLAGS);
	if (!m_index_counters || !m_flag_counters)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	op_memset(m_index_counters,0,sizeof(int)*m_mappings.GetSize());
	op_memset(m_flag_counters,0,sizeof(int)*(1<<SPC_RADIX_FLAGS));
	RETURN_IF_ERROR(m_root_radix->ClearCounters(m_mappings.GetSize()));
	RETURN_IF_ERROR(SetupHashBitField());
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryPostProcessPass2()
{
	int i,j;
	for (i=1;i<m_mappings.GetSize();i++)
	{
		MapMapping *mapping = m_mappings.GetElementPtr(i);
		int max_index = 0, max_freq = mapping->frequencies[0];
		for (j=1;j<mapping->char_count;j++)
		{
			if (mapping->frequencies[j] > (UINT32)max_freq)
			{
				max_index = j;
				max_freq = mapping->frequencies[j];
			}
		}
		RETURN_IF_ERROR(AddToCharMappings(mapping->chars[0], i, max_index));
		RETURN_IF_ERROR(AddToCharMappings(mapping->chars[max_index], i, 0));
		UINT32 tmp = mapping->chars[0];
		mapping->chars[0] = mapping->chars[max_index];
		mapping->chars[max_index] = tmp;
	}
	int huffman_levels = MIN(bits_to_represent(m_mappings.GetSize()) + 4, SPC_HUFFMAN_MAX_LEVELS);
	RETURN_IF_ERROR(SetupHuffmanCodes(m_index_counters, m_mappings.GetSize(), m_huffman_to_index, m_index_to_huffman, huffman_levels));
	m_huffman_to_index_mask = (1<<huffman_levels)-1;
	if (m_has_radix_flags)
		RETURN_IF_ERROR(SetupHuffmanCodes(m_flag_counters, 1<<SPC_RADIX_FLAGS, m_huffman_to_flags, m_flags_to_huffman, SPC_FLAGS_HUFFMAN_LEVELS));
	if (m_has_compound_rule_flags)
	{
		int compound_rule_flags = m_affix_file_data->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG);
		huffman_levels = MIN(bits_to_represent(compound_rule_flags) + 4, SPC_HUFFMAN_MAX_LEVELS);
		RETURN_IF_ERROR(SetupHuffmanCodes(m_compound_rule_flag_counters, compound_rule_flags, m_huffman_to_compound_rule_flag, m_compound_rule_flag_to_huffman, huffman_levels));
		m_huffman_to_compound_rule_flag_mask = (1<<huffman_levels)-1;
	}
	OP_DELETEA(m_index_counters);
	OP_DELETEA(m_flag_counters);
	OP_DELETEA(m_compound_rule_flag_counters);
	m_index_counters = NULL;
	m_flag_counters = NULL;
	m_compound_rule_flag_counters = NULL;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::DictionaryPostProcessPass3()
{
	return OpStatus::OK;
}

/* ===============================TempDictionaryParseData=============================== */

TempDictionaryParseData::TempDictionaryParseData(DictionaryEncoding encoding, BOOL has_compound_rule_flags, HunspellAffixFileData *fd)
{
	m_encoding = encoding;
	m_8bit_enc = IS_8BIT_ENCODING(encoding);
	m_has_compound_rule_flags = has_compound_rule_flags;
	m_fd = fd;
	m_next_to_alloc = NULL;
	rule_match_buf = NULL;
	affix_char_buf = NULL;
	affix_match_bit_buf = NULL;
	stem_word_info = NULL;
	stem = NULL;
	stem_len = 0;
	tmp_words = NULL;
	tmp_word_count = 0;
	valid_words = NULL;
	valid_word_count = 0;
	m_char_buf = NULL;
	m_char_buf_ofs = 0;
	m_stem_prefix_combine_count = 0;
	m_stem_suffix_combine_count = 0;
	m_generated_inflection_count = 0;
	m_stem_prefixes_with_affixes_count = 0;
	m_stem_suffixes_with_affixes_count = 0;
	index_and_mapping_buf = NULL;
	index_and_mapping_ofs = 0;
	m_stem_prefix_combines = NULL;
	m_stem_suffix_combines = NULL;
	m_generated_inflections = NULL;
	m_stem_prefixes_with_affixes = NULL;
	m_stem_suffixes_with_affixes = NULL;
#ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	sort_buf = NULL;
#endif // #ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	m_compound_rule_flag_buf = NULL;
	m_compound_rule_flag_buf_ofs = 0;
	m_compound_rule_lookup = NULL;
	m_compound_lookup_times = 0;
	m_alloced = sizeof(*this);
}

TempDictionaryParseData::~TempDictionaryParseData()
{
	OP_DELETEA(rule_match_buf);
	OP_DELETEA(affix_char_buf);
	OP_DELETEA(affix_match_bit_buf);
	OP_DELETEA(tmp_words);
	OP_DELETEA(valid_words);
	OP_DELETEA(m_char_buf);
	OP_DELETEA(m_stem_prefix_combines);
	OP_DELETEA(m_stem_suffix_combines);
	OP_DELETEA(m_generated_inflections);
	OP_DELETEA(m_stem_prefixes_with_affixes);
	OP_DELETEA(m_stem_suffixes_with_affixes);
#ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	OP_DELETEA(sort_buf);
#endif // #ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	OP_DELETEA(index_and_mapping_buf);
	OP_DELETEA(m_compound_rule_flag_buf);
	OP_DELETEA(m_compound_rule_lookup);
}

OP_STATUS TempDictionaryParseData::Init()
{
	rule_match_buf = OP_NEWA(int, SPC_MAX_MATCHING_RULES_PER_WORD);
	m_alloced += sizeof(*rule_match_buf) * SPC_MAX_MATCHING_RULES_PER_WORD;
	
	affix_char_buf = OP_NEWA(UINT32, SPC_MAX_AFFIX_RULE_CONDITION_LENGTH);
	m_alloced += sizeof(*affix_char_buf) * SPC_MAX_AFFIX_RULE_CONDITION_LENGTH;
	
	affix_match_bit_buf = OP_NEWA(UINT32, SPC_MAX_RULES_PER_AFFIX/32 + 1);
	m_alloced += sizeof(*affix_match_bit_buf) * (SPC_MAX_RULES_PER_AFFIX/32 + 1);
	
	tmp_words = OP_NEWA(TempWordInfo, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*tmp_words) * SPC_MAX_TEMP_WORDS;
	
	valid_words = OP_NEWA(TempWordInfo*, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*valid_words) * SPC_MAX_TEMP_WORDS;
	
	m_char_buf = OP_NEWA(UINT32, SPC_TEMP_WORD_CHAR_BUF_SZ);
	m_alloced += sizeof(*m_char_buf) * SPC_TEMP_WORD_CHAR_BUF_SZ;
	
	m_stem_prefix_combines = OP_NEWA(TempWordInfo*, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*m_stem_prefix_combines) * SPC_MAX_TEMP_WORDS;
	
	m_stem_suffix_combines = OP_NEWA(TempWordInfo*, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*m_stem_suffix_combines) * SPC_MAX_TEMP_WORDS;
	
	m_generated_inflections = OP_NEWA(TempWordInfo*, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*m_generated_inflections) * SPC_MAX_TEMP_WORDS;
	
	m_stem_prefixes_with_affixes = OP_NEWA(TempWordInfo*, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*m_stem_prefixes_with_affixes) * SPC_MAX_TEMP_WORDS;
	
	m_stem_suffixes_with_affixes = OP_NEWA(TempWordInfo*, SPC_MAX_TEMP_WORDS);
	m_alloced += sizeof(*m_stem_suffixes_with_affixes) * SPC_MAX_TEMP_WORDS;
	
#ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	sort_buf = OP_NEWA(UINT8, SPC_MAX_TEMP_WORDS*8);
	m_alloced += sizeof(*sort_buf) * SPC_MAX_TEMP_WORDS*8;
#endif // #ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
	
	index_and_mapping_buf = OP_NEWA(IndexAndMapping, SPC_TEMP_INDEX_AND_MAPPING_BUF_SZ);
	m_alloced += sizeof(*index_and_mapping_buf) * SPC_TEMP_INDEX_AND_MAPPING_BUF_SZ;
	
	m_compound_rule_flag_buf = OP_NEWA(UINT16, SPC_TEMP_COMPOUND_RULE_FLAG_BUF_SZ);
	m_alloced += sizeof(*m_compound_rule_flag_buf) * SPC_TEMP_COMPOUND_RULE_FLAG_BUF_SZ;
	
	m_compound_rule_lookup = OP_NEWA(int, MAX(m_fd->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG),1));
	m_alloced += sizeof(*m_compound_rule_lookup) * MAX(m_fd->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG),1);

	if (!rule_match_buf || !affix_char_buf || !affix_match_bit_buf || !tmp_words || !valid_words || !m_char_buf || 
		!m_stem_prefix_combines || !m_stem_suffix_combines || !m_generated_inflections || 
		!m_stem_prefixes_with_affixes || !m_stem_suffixes_with_affixes || 
		!m_compound_rule_flag_buf || !m_compound_rule_lookup
#ifdef SPC_SORT_PARTIALLY_FOR_COMPRESSION
		|| !sort_buf
#endif // SPC_SORT_PARTIALLY_FOR_COMPRESSION
		)
	{
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	op_memset(m_compound_rule_lookup, 0, m_fd->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG)*sizeof(*m_compound_rule_lookup));
	return OpStatus::OK;
}

OP_STATUS TempDictionaryParseData::SetStem(UINT8 *word, UINT8 *flags, BOOL reset_lists)
{
	int i;
	OpSpellCheckerBitStream stream(flags);
	UINT16 bit_flags = 0;
	UINT16 compound_rule_flags[SPC_MAX_COMPOUND_RULE_FLAGS_PER_LINE];
	int compound_rule_flag_count = 0;
	if (flags)
	{
		bit_flags = stream.ReadVal16(sizeof(UINT16)*8); // read bit flags
		if (m_has_compound_rule_flags)
		{
			while (stream.ReadBit()) // there are more compound rule flags
				compound_rule_flags[compound_rule_flag_count++] = stream.ReadVal16(m_fd->GetFlagTypeBits(AFFIX_TYPE_COMPOUNDRULEFLAG));
		}
	}
	RETURN_IF_ERROR(SetStemInternal(word,bit_flags,compound_rule_flags,compound_rule_flag_count,reset_lists));
	if (!(bit_flags & 1<<AFFIX_BIT_HAS_AFFIXES)) // there are no flags or there are no affixes
		return OpStatus::OK;
	OpSpellCheckerAffix *affix;
	OpSpellCheckerAffixRule *rule;
	int rule_count;
	while (stream.ReadBit()) // has another affix
	{
		rule_count = 0;
		affix = (OpSpellCheckerAffix*)m_fd->GetFlagClassByGlobalId((int)stream.ReadVal16(m_fd->GetGlobalIdBits()));
		if (stream.ReadBit()) // the rules are cached
		{
			do
			{
				rule_match_buf[rule_count++] = stream.ReadVal16(affix->GetRuleCountBits()); // read rule index
			} while (stream.ReadBit()); // while there is another rule
		}
		else
		{
			m_fd->ConvertStrToAffixChars(stem, stem_len, affix_char_buf, affix);				
			affix->GetMatchingRules(affix_char_buf, affix_match_bit_buf, rule_match_buf, rule_count);
		}
		for (i=0;i<rule_count;i++)
		{
			rule = affix->GetRulePtr(rule_match_buf[i]);
			RETURN_IF_ERROR(AddStemInflection(affix,rule));
		}
	}
	return OpStatus::OK;
}

OP_STATUS TempDictionaryParseData::CreateCombines()
{
	int i,j,k,l;
	TempWordInfo *info1,*info2,*info3;
	OpSpellCheckerAffix *affix;
	if (m_stem_prefix_combine_count && m_stem_suffix_combine_count)
	{
		for (i=0;i<m_stem_prefix_combine_count;i++)
		{
			info1 = m_stem_prefix_combines[i];
			for (j=0;j<m_stem_suffix_combine_count;j++)
			{
				info2 = m_stem_suffix_combines[j];
				RETURN_IF_ERROR(AddStemInflectionCombineInternal(info1,info2));
			}
		}
	}
	for (i=0;i<m_stem_prefixes_with_affixes_count;i++)
	{
		info1 = m_stem_prefixes_with_affixes[i]; // prefix-rule0 + stem
		for (j=0;j<info1->info.affix_count;j++)
		{
			affix = (OpSpellCheckerAffix*)m_fd->GetFlagClassByGlobalId((int)info1->info.affixes[j]);
			RETURN_IF_ERROR(GenerateAffixInflectionsInternal(affix,info1)); // generate prefix-rule0 + stem + suffix-rule1 OR prefix-rule1 + prefix-rule0 + stem
			if (!affix->IsSuffix() && affix->CanCombine() && m_stem_suffix_combine_count)
			{
				for (k=0;k<m_generated_inflection_count;k++)
				{
					info2 = m_generated_inflections[k]; // prefix-rule1 + prefix-rule0 + stem
					for (l=0;l<m_stem_suffix_combine_count;l++)
					{
						info3 = m_stem_suffix_combines[l];
						RETURN_IF_ERROR(AddCombineSuffixInflectionCombineInternal(info2,info3)); // prefix-rule1 + prifix-rule0 + stem + suffix-rule0
					}
				}
			}
		}
	}
	for (i=0;i<m_stem_suffixes_with_affixes_count;i++)
	{
		info1 = m_stem_suffixes_with_affixes[i]; // stem + suffix-rule0
		for (j=0;j<info1->info.affix_count;j++)
		{
			affix = (OpSpellCheckerAffix*)m_fd->GetFlagClassByGlobalId((int)info1->info.affixes[j]);
			RETURN_IF_ERROR(GenerateAffixInflectionsInternal(affix,info1)); // generate prefix-rule1 + stem + suffix-rule0 OR stem + suffix-rule0 + suffix-rule1
			if (affix->IsSuffix() && affix->CanCombine() && m_stem_prefix_combine_count)
			{
				for (k=0;k<m_generated_inflection_count;k++)
				{
					info2 = m_generated_inflections[k]; // stem + suffix-rule0 + suffix-rule1
					for (l=0;l<m_stem_prefix_combine_count;l++)
					{
						info3 = m_stem_prefix_combines[l];
						RETURN_IF_ERROR(AddCombinePrefixInflectionCombineInternal(info2,info3)); // prefix-rule0 + stem + suffix-rule0 + suffix-rule1
					}
				}
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS TempDictionaryParseData::SetStemInternal(UINT8 *word, UINT16 bit_flags, UINT16 *compound_rule_flags, 
	int compound_rule_flag_count, BOOL reset_lists)
{
	int dummy = 0;
	m_stem_prefix_combine_count = 0;
	m_stem_suffix_combine_count = 0;
	m_stem_prefixes_with_affixes_count = 0;
	m_stem_suffixes_with_affixes_count = 0;
	m_generated_inflection_count = 0;
	if (reset_lists)
	{
		m_next_to_alloc = tmp_words;
		tmp_word_count = 0;
		valid_word_count = 0;
		m_char_buf_ofs = 0;
		index_and_mapping_ofs = 0;
		m_compound_rule_flag_buf_ofs = 0;
	}
	int len = 0;
	if (m_8bit_enc)
	{
		while (*word)
			m_char_buf[m_char_buf_ofs + len++] = (int)(*(word++));
	}
	else
	{
		if (!utf8_decode_str(word,m_char_buf+m_char_buf_ofs,dummy,len))
			return SPC_ERROR(OpStatus::ERR);
	}
	TempWordInfo *word_info = m_next_to_alloc++;
	tmp_word_count++;
	word_info->Init();
	word_info->info.bit_flags = bit_flags;
	word_info->info.compound_rule_flag_count = compound_rule_flag_count;
	if (compound_rule_flag_count)
	{
		word_info->info.compound_rule_flags = m_compound_rule_flag_buf + m_compound_rule_flag_buf_ofs;
		op_memcpy(word_info->info.compound_rule_flags, compound_rule_flags, sizeof(UINT16)*compound_rule_flag_count);
		m_compound_rule_flag_buf_ofs += compound_rule_flag_count;
	}
	word_info->word = m_char_buf + m_char_buf_ofs;
	word_info->word_len = len;
	if (!(bit_flags & 1<<AFFIX_TYPE_NEEDAFFIX))
		valid_words[valid_word_count++] = word_info;
	stem_word_info = word_info;
	stem = word_info->word;
	stem_len = len;
	m_char_buf_ofs += len;
	return OpStatus::OK;
}

inline OP_STATUS TempDictionaryParseData::AddStemInflection(OpSpellCheckerAffix *affix, OpSpellCheckerAffixRule *rule)
{
	TempWordInfo *word = AllocateTempWordAndCombine(stem_word_info, affix, rule, FALSE);
	if (!word)
		return SPC_ERROR(OpStatus::ERR);
	if (affix->IsSuffix())
	{
		if (word->info.affix_count)
			m_stem_suffixes_with_affixes[m_stem_suffixes_with_affixes_count++] = word;
		if (affix->CanCombine())
			m_stem_suffix_combines[m_stem_suffix_combine_count++] = word;
	}
	else
	{
		if (word->info.affix_count)
			m_stem_prefixes_with_affixes[m_stem_prefixes_with_affixes_count++] = word;
		if (affix->CanCombine())
			m_stem_prefix_combines[m_stem_prefix_combine_count++] = word;
	}
	return OpStatus::OK;
}

inline OP_STATUS TempDictionaryParseData::CombineCompoundRuleFlags(TempWordInfo *left, TempWordInfo *right, TempWordInfo *dst)
{
	if (!left->info.compound_rule_flag_count && !right->info.compound_rule_flag_count)
		return OpStatus::OK;
	if (left->info.compound_rule_flag_count && !right->info.compound_rule_flag_count)
	{
		dst->info.compound_rule_flags = left->info.compound_rule_flags;
		dst->info.compound_rule_flag_count = left->info.compound_rule_flag_count;
	}
	else if (!left->info.compound_rule_flag_count && right->info.compound_rule_flag_count)
	{
		dst->info.compound_rule_flags = right->info.compound_rule_flags;
		dst->info.compound_rule_flag_count = right->info.compound_rule_flag_count;
	}
	else // combine compound-rule-flags from left and right
	{
		int i,j;
		if (left->info.compound_rule_flag_count + right->info.compound_rule_flag_count + m_compound_rule_flag_buf_ofs > SPC_MAX_COMPOUND_RULE_FLAGS_PER_LINE)
			return SPC_ERROR(OpStatus::ERR);
		UINT16 *new_flags = m_compound_rule_flag_buf + m_compound_rule_flag_buf_ofs;
		m_compound_lookup_times++;
		UINT16 flag;
		for (i=0;i<left->info.compound_rule_flag_count;i++)
		{
			flag = left->info.compound_rule_flags[i];
			OP_ASSERT(flag && flag < m_fd->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG));
			m_compound_rule_lookup[flag] = m_compound_lookup_times;
			new_flags[i] = flag;
		}
		for (j=0;j<right->info.compound_rule_flag_count;j++)
		{
			flag = right->info.compound_rule_flags[j];
			if (m_compound_rule_lookup[flag] != m_compound_lookup_times)
				new_flags[i++] = flag;
		}
		dst->info.compound_rule_flag_count = i;
		if (i == left->info.compound_rule_flag_count) // right didn't add any more compound-rule-flags, so just use left
			dst->info.compound_rule_flags = left->info.compound_rule_flags;
		else
		{
			dst->info.compound_rule_flags = new_flags;
			m_compound_rule_flag_buf_ofs += i;
		}
	}
	return OpStatus::OK;
}

inline TempWordInfo* TempDictionaryParseData::AllocateTempWordAndCombine(TempWordInfo *stem, OpSpellCheckerAffix *affix, OpSpellCheckerAffixRule *rule, BOOL affix_at_opposite)
{
#define TMP_MEMCPY32(_dst, _src, len) \
	src = _src; \
	dst = _dst; \
	i = len; \
	while (i--) \
		*(dst++) = *(src++);

	if (tmp_word_count == SPC_MAX_TEMP_WORDS)
		return NULL;
	
	TempWordInfo *word = m_next_to_alloc++;
	tmp_word_count++;

	word->mapped_char_count = 0;
	//word->Init();
	UINT32 *affix_chars = rule->GetAffixChars();
	int affix_char_len = rule->GetAffixCharLength();
	if (m_char_buf_ofs + affix_char_len > SPC_TEMP_WORD_CHAR_BUF_SZ)
		return NULL;
	
	int strip_max = MIN(rule->GetStripCharLength(),stem->word_len);
	UINT32 *word_ptr, *strip_ptr, *src, *dst;
	int to_strip = 0;
	int copy_len,i;
	word->word = m_char_buf + m_char_buf_ofs;
	word->info.SetValuesFrom(rule->GetFlags());
	if (affix->IsSuffix())
	{
		if (m_has_compound_rule_flags && OpStatus::IsError(CombineCompoundRuleFlags(stem, word, word)))
			return NULL;
		word_ptr = stem->word + stem->word_len - 1;
		strip_ptr = rule->GetStripChars() + rule->GetStripCharLength() - 1;
		while (strip_max-- && *(word_ptr--) == *(strip_ptr--))
			to_strip++;
		copy_len = stem->word_len - to_strip;
		TMP_MEMCPY32(word->word, stem->word, copy_len);
		TMP_MEMCPY32(word->word+copy_len, affix_chars, affix_char_len);
	}
	else
	{
		if (m_has_compound_rule_flags && OpStatus::IsError(CombineCompoundRuleFlags(word, stem, word)))
			return NULL;
		word_ptr = stem->word;
		strip_ptr = rule->GetStripChars();
		while (strip_max-- && *(word_ptr++) == *(strip_ptr++))
			to_strip++;
		copy_len = stem->word_len - to_strip;
		TMP_MEMCPY32(word->word, affix_chars, affix_char_len);
		TMP_MEMCPY32(word->word+affix_char_len, stem->word+to_strip, copy_len);
	}
	word->word_len = copy_len + affix_char_len;
	word->inserted_chars = affix_char_len;
	word->removed_chars = to_strip;
	if (affix_at_opposite) // no need to set inserted_chars + removed_chars as they will NOT be used later
	{
		if (!(stem->info.bit_flags & 1<<AFFIX_TYPE_CIRCUMFIX ^ word->info.bit_flags & 1<<AFFIX_TYPE_CIRCUMFIX))
			valid_words[valid_word_count++] = word;
	}
	else
	{
		if (!(word->info.bit_flags & (1<<AFFIX_TYPE_CIRCUMFIX | 1<<AFFIX_TYPE_NEEDAFFIX)))
			valid_words[valid_word_count++] = word;
	}
	word->info.bit_flags |= stem->info.bit_flags & (1<<SPC_RADIX_FLAGS)-1;
	m_char_buf_ofs += word->word_len;
	return word;
}

inline TempWordInfo* TempDictionaryParseData::AllocateTempWordAndCombine(TempWordInfo *pfx_stem, TempWordInfo *sfx_stem)
{
	if (tmp_word_count == SPC_MAX_TEMP_WORDS)
		return NULL;
	
	TempWordInfo *word = m_next_to_alloc++;
	tmp_word_count++;
	
	word->Init();
	if (m_char_buf_ofs + pfx_stem->word_len + sfx_stem->inserted_chars > SPC_TEMP_WORD_CHAR_BUF_SZ)
		return NULL;

	BOOL valid = !(pfx_stem->info.bit_flags & 1<<AFFIX_TYPE_CIRCUMFIX ^ sfx_stem->info.bit_flags & 1<<AFFIX_TYPE_CIRCUMFIX);
	int from_pfx = MAX(pfx_stem->word_len - sfx_stem->removed_chars, 0);
	word->word = m_char_buf + m_char_buf_ofs;
	op_memcpy(word->word, pfx_stem->word, from_pfx*sizeof(UINT32));
	op_memcpy(word->word+from_pfx, sfx_stem->word+sfx_stem->word_len-sfx_stem->inserted_chars, sfx_stem->inserted_chars*sizeof(UINT32));
	word->word_len = from_pfx + sfx_stem->inserted_chars;
	
	if (m_has_compound_rule_flags)
	{
		if (OpStatus::IsError(CombineCompoundRuleFlags(pfx_stem, sfx_stem, word)))
			return NULL;
	}
	word->info.bit_flags = pfx_stem->info.bit_flags | pfx_stem->info.bit_flags;
	
	if (valid)
		valid_words[valid_word_count++] = word;
	m_char_buf_ofs += word->word_len;
	return word;
}

OP_STATUS TempDictionaryParseData::GenerateAffixInflectionsInternal(OpSpellCheckerAffix *affix, TempWordInfo *afx_rule_stem)
{
	int i,rule_count = 0;
	TempWordInfo *word;
	OpSpellCheckerAffixRule *rule;
	m_fd->ConvertStrToAffixChars(afx_rule_stem->word, afx_rule_stem->word_len, affix_char_buf, affix);
	affix->GetMatchingRules(affix_char_buf, affix_match_bit_buf, rule_match_buf, rule_count);
	m_generated_inflection_count = rule_count;
	for (i=0;i<rule_count;i++)
	{
		rule = affix->GetRulePtr(rule_match_buf[i]);
		word = AllocateTempWordAndCombine(afx_rule_stem, affix, rule, TRUE);
		if (!word)
			return SPC_ERROR(OpStatus::ERR);
		m_generated_inflections[i] = word;
	}
	return OpStatus::OK;
}

#endif // !USE_HUNSPELL_ENGINE

#endif // INTERNAL_SPELLCHECK_SUPPORT
