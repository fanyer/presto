/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/src/opspellchecker.h"
#include "modules/spellchecker/opspellcheckerapi.h"
#include "modules/encodings/utility/opstring-encodings.h"

#ifndef SPC_USE_HUNSPELL_ENGINE
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"

/* ===============================OpSpellChecker=============================== */

#define RETURN_AND_CLEAR_IF_ERROR( expr ) \
   do { OP_STATUS RETURN_IF_ERROR_TMP = expr; \
        if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
   {	\
		ClearDictionary(); \
		return RETURN_IF_ERROR_TMP; \
   }	\
   } while (0)

#define RETURN_AND_CLEAR_IF_NULL( expr ) \
   do { \
        if ((expr) == NULL) \
   {	\
		ClearDictionary(); \
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY); \
   }	\
   } while (0)

void OpSpellChecker::Init()
{
	m_affix_file_data = NULL;
	m_affix_file_parse_state = NULL;
	m_dic_file_data = NULL;
	m_dic_parse_state = NULL;
	m_dic_file_chunks = NULL;
	m_state = INIT_STATE;
	m_dictionary_path = NULL;
	m_own_path = NULL;
	m_error_str = NULL;
	
	m_encoding = BIT8_ENC;
	m_8bit_enc = TRUE;
	m_enc_string = NULL;
	m_output_converter = NULL;
	m_input_converter = NULL;
	m_8bit_to_upper = NULL;
	m_8bit_to_lower = NULL;
	
	m_chars_in_dic_data = 0;
	m_approx_freq_len = 0;
	m_approx_freq = NULL;
	m_existing_chars.data = NULL;
	m_unusual_mapping_index = 0;

	m_affix_parsers = NULL;
	m_affix_parser_count = 0;
	m_tmp_temp_buffer_users = 0;
	m_word_chars = NULL;
	m_break_chars = NULL;
	m_char_mappings.data = NULL;
	m_root_radix = NULL;
	m_index_counters = NULL;
	m_flag_counters = NULL;
	m_compound_rule_flag_counters = NULL;
	m_index_combinations = NULL;
	op_memset(m_short_combinations,0,sizeof(UINT8*)*SPC_MAX_COMBINATION_LEN);
	m_combination_len = 0;
	m_total_word_count = 0;
	m_word_hash = NULL;
	m_huffman_to_index = NULL;
	m_index_to_huffman = NULL;
	m_huffman_to_flags = NULL;
	m_flags_to_huffman = NULL;
	m_huffman_to_compound_rule_flag = NULL;
	m_compound_rule_flag_to_huffman = NULL;
	m_huffman_to_index_mask = 0;
	m_huffman_to_compound_rule_flag_mask = 0;
	m_has_compounds = FALSE;
	m_has_radix_flags = FALSE;
	m_has_compound_rule_flags = FALSE;

	m_checksharps = FALSE;
	m_sharps_char = 0;
	m_checkcompoundcase = FALSE;
	m_compoundmin = 3;
	m_compoundwordmax = 3;

	m_lookup_allocator = NULL;

	m_stat_counters = 0;
	m_stat_count_total = 0;
	m_stat_max_counter = 0;
	m_stat_misc_dictionary_bytes = 0;
	m_stat_misc_lookup_bytes = 0;
	m_stat_lookup_bytes = 0;
	m_stat_start_time = 0.0;
}

OpSpellChecker::OpSpellChecker(OpSpellCheckerLanguage *language) : 
	m_added_words(&m_allocator),
	m_removed_words(&m_allocator),
	m_added_from_file_and_later_removed(&m_allocator)
{
	m_language = language;
	Init();
}

OpSpellChecker::~OpSpellChecker()
{
	ClearDictionary();
}

BOOL OpSpellChecker::FreeCachedData(BOOL toplevel_context)
{
	if (!m_word_hash || m_state != LOADING_FINISHED)
		return FALSE;
	OP_DELETEA(m_word_hash);
	m_word_hash = NULL;
	return TRUE;
}

OP_STATUS OpSpellChecker::InitMemoryStructs()
{
	OP_ASSERT(m_state == INIT_STATE && m_8bit_enc && !m_existing_chars.data);
	m_existing_chars.linear = OP_NEWA(UINT32, 256);
	if (!m_existing_chars.linear)
		return OpStatus::ERR_NO_MEMORY;
	op_memset(m_existing_chars.linear, 0, sizeof(UINT32)*256);
	if (m_mappings.AddElement(MapMapping()) < 0)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

void OpSpellChecker::ClearDictionary()
{
	int i;
	OP_DELETEA(m_affix_parsers);
	for (i=0;i<m_compound_rules.GetSize();i++)
		OP_DELETE(m_compound_rules.GetElement(i));
	m_compound_rules.Reset();
	m_allocator.Clean();
	m_mappings.Reset();
	
	if (!m_8bit_enc)
	{
		OP_DELETE(m_char_mappings.utf);
		OP_DELETE(m_existing_chars.utf);
	}
	else
	{
		OP_DELETEA(m_char_mappings.linear);
		OP_DELETEA(m_existing_chars.linear);
	}
	OP_DELETEA(m_approx_freq);
	OP_DELETEA(m_enc_string);
	OP_DELETE(m_output_converter);
	OP_DELETE(m_input_converter);
	
	OP_DELETE(m_affix_file_data);
	OP_DELETE(m_affix_file_parse_state);
	OP_DELETE(m_dic_file_data);
	OP_DELETE(m_dic_parse_state);
	OP_DELETE(m_dic_file_chunks);
	OP_DELETEA(m_dictionary_path);
	OP_DELETEA(m_own_path);
	OP_DELETEA(m_index_counters);
	OP_DELETEA(m_flag_counters);
	OP_DELETEA(m_compound_rule_flag_counters);
	OP_DELETEA(m_index_combinations);
	for (i=0;i<SPC_MAX_COMBINATION_LEN;i++)
		OP_DELETEA(m_short_combinations[i]);
	OP_DELETEA(m_word_hash);
	OP_DELETEA(m_huffman_to_index);
	OP_DELETEA(m_index_to_huffman);
	OP_DELETEA(m_huffman_to_flags);
	OP_DELETEA(m_flags_to_huffman);
	OP_DELETEA(m_huffman_to_compound_rule_flag);
	OP_DELETEA(m_compound_rule_flag_to_huffman);
	
	Init();
}

OP_STATUS OpSpellChecker::AddExistingCharsAtAffix(UINT32 *chars, int len)
{
	if (m_8bit_enc)
	{
		while (len--)
			m_existing_chars.linear[chars[len]] = 100000000;
	}
	else
	{
		while (len--)
			RETURN_IF_ERROR(m_existing_chars.utf->SetVal(chars[len], 100000000));
	}
	return OpStatus::OK;
}

BOOL OpSpellChecker::IsExistingChar(UINT32 ch)
{
	return !!GetApproxExistCountFor(ch);
}

OP_STATUS OpSpellChecker::SetupApproxFreqenceOfExisting()
{
	int i;
	OP_ASSERT(!m_approx_freq);
	int count = 0;
	if (m_8bit_enc)
	{
		for (i=0;i<256;i++)
			if (m_existing_chars.linear[i])
				count++;
	}
	else
		count = m_existing_chars.utf->CountNonZero();
	m_approx_freq_len = count;
	m_approx_freq = OP_NEWA(IndexCounterMapping, count);
	if (!m_approx_freq)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	if (m_8bit_enc)
	{
		count = 0;
		for (i=0;i<256;i++)
			if (m_existing_chars.linear[i])
				m_approx_freq[count++].Set(m_existing_chars.linear[i], i);
	}
	else
	{
		OP_ASSERT(sizeof(IndexCounterMapping) == 8 && (SPC_UINT_PTR)(&m_approx_freq->index) < (SPC_UINT_PTR)(&m_approx_freq->count));
		m_existing_chars.utf->WriteOutNonZeroIndexesAndElements(m_approx_freq);
	}
	UINT8 *buf = OP_NEWA(UINT8, count*8);
	if (!buf)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	quick_sort<IndexCounterMapping>(m_approx_freq,count,buf);
	OP_DELETEA(buf);
	return OpStatus::OK;	
}

OP_STATUS OpSpellChecker::GetDifferentCaseChar(UINT32 ch, UINT32 &other_char)
{
	UINT32 other = 0;
	int was_read, was_written;
	other_char = 0;
	if (m_8bit_enc)
	{
		UINT8 src = (UINT8)ch;
		uni_char dst[2]; /* ARRAY OK 2008-06-10 danielsp */
		was_written = m_input_converter->Convert(&src, 1, dst, 2*sizeof(uni_char), &was_read);
		if (was_written != 2 || was_read != 1)
			return OpStatus::OK; // surrogate?
		ch = (UINT32)dst[0];
	}
	if (ch & 0xFFFF0000 || Unicode::IsSurrogate(ch))
		return OpStatus::OK; // we doesn't support this...
	UINT32 up = (UINT32)Unicode::ToUpper(ch);
	UINT32 low = (UINT32)Unicode::ToLower(ch);
	if (up != ch)
		other = up;
	else if (low != ch)
		other = low;
	if (!other || other & 0xFFFF0000 || Unicode::IsSurrogate(ch))
		return OpStatus::OK;
	if (m_8bit_enc)
	{
		uni_char src = (uni_char)other;
		UINT8 dst;
		was_written = m_output_converter->Convert(&src, sizeof(uni_char), &dst, 1, &was_read);
		if (was_written != 1 || was_read != 2 || dst == '?')
			return OpStatus::OK;
		other = (UINT32)dst;
	}
	other_char = other;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::AppendUnusualMapping()
{
	int i;
	int char_count = m_unusual_chars.GetSize();
	int total_count = 1 << bits_to_represent(char_count + SPC_MIN_CAPACITY_FOR_NEW_CHARS);
	MapMapping map_mapping;
	UINT32 *map = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*total_count);
	UINT32 *freq = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*char_count);
	if (!map || !freq)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);

	map_mapping.char_count = char_count;
	map_mapping.bit_representation = bits_to_represent(total_count)-1;
	map_mapping.chars = map;
	map_mapping.frequencies = freq;
	map_mapping.exists = TRUE; // set it to TRUE even if it's not, because we need an index for this mapping in order to add words with characters not in the dictionary
	for (i=0;i<char_count;i++)
		map[i] = m_unusual_chars.GetElement(i);
	m_unusual_chars.Reset();
	if (m_mappings.AddElement(map_mapping) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::MoveToUnusualMapping(UINT32 ch)
{
	int pos = m_unusual_chars.GetSize();
	if (m_unusual_chars.AddElement(ch) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	RETURN_IF_ERROR(AddToCharMappings(ch, m_unusual_mapping_index, pos));
	return OpStatus::OK;
}

int OpSpellChecker::GetApproxExistCountFor(UINT32 ch)
{
	if (m_8bit_enc)
		return m_existing_chars.linear[ch];
	else
		return m_existing_chars.utf->GetVal(ch);
}

OP_STATUS OpSpellChecker::SetupPairMapping(int ch1, int ch2)
{
	MappingPointer m1 = GetCharMapping(ch1);
	MappingPointer m2 = GetCharMapping(ch2);
	if (m1.IsPointer() && m2.IsPointer()) // both are already mapped
		return OpStatus::OK;
	if (!m1.IsPointer() && !m2.IsPointer()) // make new mapping
	{
		MapMapping map_mapping;
		int mapping_idx = m_mappings.GetSize();
		if (mapping_idx == SPC_MAX_INDEXES)
			return SPC_ERROR(OpStatus::ERR);
		UINT32 *map = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*2);
		UINT32 *freq = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*2);
		if (!map || !freq)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	
		map_mapping.char_count = 2;
		map_mapping.bit_representation = 0; // there are two elements and one are "most frequent" so indicating whether the most frequent should be used or not is enough
		map_mapping.chars = map;
		map_mapping.frequencies = freq;
		map[0] = (UINT32)ch1;
		map[1] = (UINT32)ch2;
		RETURN_IF_ERROR(AddToCharMappings(ch1,mapping_idx,0));
		RETURN_IF_ERROR(AddToCharMappings(ch2,mapping_idx,1));
		if (m_mappings.AddElement(map_mapping) < 0)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	else // add the one not mapped to the mapping of the mapped
	{
		MapMapping *map_mapping;
		int ch, mapping_idx, in_mapping_idx;
		if (m1.IsPointer())
		{
			ch = ch2;
			mapping_idx = m1.mapping_idx;
		}
		else
		{
			ch = ch1;
			mapping_idx = m2.mapping_idx;
		}
		map_mapping = m_mappings.GetElementPtr(mapping_idx);
		in_mapping_idx = map_mapping->char_count++;
		map_mapping->chars[in_mapping_idx] = (UINT32)ch;
		map_mapping->bit_representation = bits_to_represent(map_mapping->char_count-1);
		RETURN_IF_ERROR(AddToCharMappings(ch, mapping_idx, in_mapping_idx));
	}
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::SetupSingleMapping(UINT32 ch)
{
	int mapping_idx = m_mappings.GetSize();
	if (mapping_idx == SPC_MAX_INDEXES)
		return SPC_ERROR(OpStatus::ERR);
	MapMapping map_mapping;
	UINT32 *map = (UINT32*)m_allocator.Allocate32(sizeof(UINT32));
	UINT32 *freq = (UINT32*)m_allocator.Allocate32(sizeof(UINT32));
	if (!map || !freq)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);

	map_mapping.char_count = 1;
	map_mapping.bit_representation = 0;
	map_mapping.chars = map;
	map_mapping.frequencies = freq;
	map[0] = ch;
	if (m_mappings.AddElement(map_mapping) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	RETURN_IF_ERROR(AddToCharMappings(ch, mapping_idx, 0));
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::AddToCharMappings(int ch, int mapping_index, int pos)
{
	if (!m_char_mappings.data)
	{
		if (m_8bit_enc)
		{
			m_char_mappings.linear = OP_NEWA(MappingPointer, 256);
			if (m_char_mappings.linear)
				op_memset(m_char_mappings.linear, 0, 256*sizeof(MappingPointer));
		}
		else
			m_char_mappings.utf = UTFMapping<MappingPointer>::Create();
		if (!m_char_mappings.data)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	MappingPointer mapping_pointer((UINT16)mapping_index,(UINT16)pos);
	if (m_8bit_enc)
		m_char_mappings.linear[ch] = mapping_pointer;
	else
		RETURN_IF_ERROR(m_char_mappings.utf->SetVal(ch,mapping_pointer));
	return OpStatus::OK;
}

MappingPointer OpSpellChecker::GetCharMapping(int ch)
{
	if (!m_char_mappings.data)
		return MappingPointer(0,0);
	if (m_8bit_enc)
		return m_char_mappings.linear[ch];
	MappingPointer pointer(0,0);
	if (m_char_mappings.utf->GetVal(ch,pointer))
		return pointer;
	return MappingPointer(0,0);
}

OP_STATUS OpSpellChecker::SetupCharConverters()
{
	int i;
	if (!m_8bit_enc)
	{
		m_output_converter = OP_NEW(SCUTF16ToUTF32Converter, ());
		m_input_converter = OP_NEW(SCUTF32ToUTF16Converter, ());
		if (m_checksharps)
			m_sharps_char = 0xDF;
		if (!m_output_converter || !m_input_converter)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		return OpStatus::OK;
	}
	const char *char_set;
	if (m_enc_string)
		char_set = (const char*)m_enc_string;
	else
		char_set = (const char*)"ISO8859-1";
	OutputConverter* tmp_out_converter;
	OP_STATUS status = OutputConverter::CreateCharConverter(char_set, &tmp_out_converter, FALSE, TRUE);
	m_output_converter = (CharConverter*)tmp_out_converter;
	if (OpStatus::IsError(status) || !m_output_converter)
		return SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR);
	InputConverter* tmp_in_converter;
	status = InputConverter::CreateCharConverter(char_set, &tmp_in_converter);
	m_input_converter = (CharConverter*)tmp_in_converter;
	if (OpStatus::IsError(status) || !m_input_converter)
		return SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR);

	if (m_checksharps)
	{
		uni_char sharps = 0xDF;
		UINT8 res = 0;
		int was_written, was_read = 0;
		was_written = m_output_converter->Convert(&sharps, sizeof(uni_char), &res, 1, &was_read);
		if (was_written != 1 || was_read != sizeof(uni_char) || !res || res == '?')
		{
			OP_ASSERT(!"Sharp S doesn't exist in dictionary encoding!");
			m_checksharps = FALSE;
		}
		else
			m_sharps_char = res;
	}

	int was_written, was_read;
	m_8bit_to_lower = (UINT8*)m_allocator.Allocate8(256);
	m_8bit_to_upper = (UINT8*)m_allocator.Allocate8(256);
	if (!m_8bit_to_lower || !m_8bit_to_upper)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	for (i=0;i<256;i++)
	{
		m_8bit_to_lower[i] = (UINT8)i;
		m_8bit_to_upper[i] = (UINT8)i;
	}
	for (i=1;i<256;i++)
	{
		UINT8 bit8 = (UINT8)i;
		uni_char uni[2];  /* ARRAY OK 2008-06-10 danielsp */
		was_written = m_input_converter->Convert(&bit8, 1, uni, 2*sizeof(uni_char), &was_read);
		if (was_written != 2 || was_read != 1)
			continue;
		uni_char lower = Unicode::ToLower(*uni) != *uni ? Unicode::ToLower(*uni) : '\0'; 
		uni_char upper = Unicode::ToUpper(*uni) != *uni ? Unicode::ToUpper(*uni) : '\0'; 
		if (lower || upper)
		{
			*uni = lower ? lower : upper;
			was_written = m_output_converter->Convert(uni, sizeof(uni_char), &bit8, 1, &was_read);
			if (was_written != 1 || was_read != 2)
				continue;
			if (upper)
				m_8bit_to_upper[i] = bit8;
			else
				m_8bit_to_lower[i] = bit8;
		}
	}
	return OpStatus::OK;
}

BOOL OpSpellChecker::IsAllUpperCase(UINT32 *str, int len)
{
	int i;
	BOOL has_real_upper = FALSE;
	if (m_8bit_enc)
	{
		for (i = 0; i < len && m_8bit_to_upper[str[i]] == (UINT8)str[i]; i++)
		{
			if (!has_real_upper && m_8bit_to_lower[str[i]] != (UINT8)str[i])
				has_real_upper = TRUE;
		}
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			if (str[i] & 0xFFFF0000 || Unicode::IsSurrogate(str[i]))
				continue;
			if (Unicode::ToUpper((uni_char)str[i]) != (uni_char)str[i])
				break;
			if (!has_real_upper && Unicode::ToLower(str[i]) != (uni_char)str[i])
				has_real_upper = TRUE;
		}
	}
	return i == len && has_real_upper;
}

BOOL OpSpellChecker::IsOneUpperFollowedByLower(UINT32 *str, int len)
{
	int i = 0;
	if (!len)
		return FALSE;
	if (m_8bit_enc)
	{
		if (m_8bit_to_lower[*str] == *str)
			return FALSE;
		for (i = 1; i < len && m_8bit_to_lower[str[i]] == (UINT8)str[i]; i++);
	}
	else
	{
		if (Unicode::ToLower(str[i]) == str[i])
			return FALSE;
		for (i = 1; i < len; i++)
		{
			if (str[i] & 0xFFFF0000 || Unicode::IsSurrogate(str[i]))
				continue;
			if (Unicode::ToLower((uni_char)str[i]) != (uni_char)str[i])
				break;
		}
	}
	return i == len;
}

void OpSpellChecker::SCToLower(UINT32 *str, int len)
{
	int i;
	if (m_8bit_enc)
	{
		for (i=0;i<len;i++)
			str[i] = (UINT32)m_8bit_to_lower[str[i]];
	}
	else
	{
		for (i=0;i<len;i++)
		{
			if (!(str[i] & 0xFFFF0000) && !Unicode::IsSurrogate(str[i]))
				str[i] = Unicode::ToLower((uni_char)str[i]);
		}
	}
}

void OpSpellChecker::SCToUpper(UINT32 *str, int len)
{
	int i;
	if (m_8bit_enc)
	{
		for (i=0;i<len;i++)
			str[i] = (UINT32)m_8bit_to_upper[str[i]];
	}
	else
	{
		for (i=0;i<len;i++)
		{
			if (!(str[i] & 0xFFFF0000) && !Unicode::IsSurrogate(str[i]))
				str[i] = Unicode::ToUpper((uni_char)str[i]);
		}
	}
}

OP_STATUS OpSpellChecker::CreateUTF16FromBuffer(ExpandingBuffer<UINT32*> *buffer, uni_char *&dst, OpSpellCheckerAllocator *allocator)
{
	int i;
	UINT32 *tmp32 = NULL;
	UINT8 *tmp8 = NULL;
	int str_len = 0;
	for (i=0;i<buffer->GetSize();i++)
		str_len += wide_str_len(buffer->GetElement(i));
	int dst_bytes = (str_len+1)*sizeof(uni_char)*2;
	dst = (uni_char*)allocator->Allocate16(dst_bytes);
	if (!dst)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	if (!str_len)
	{
		*dst = '\0';
		return OpStatus::OK;
	}
	if (m_8bit_enc)
		tmp8 = (UINT8*)m_allocator.Allocate8(str_len+1);
	else
		tmp32 = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*(str_len+1));
	if (!tmp8 && !tmp32)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	int char_idx = 0;
	for (i=0;i<buffer->GetSize();i++)
	{
		UINT32 *in_buf = buffer->GetElement(i);
		if (in_buf)
		{
			while (*in_buf)
			{
				if (m_8bit_enc)
					tmp8[char_idx] = (UINT8)*in_buf;
				else
					tmp32[char_idx] = *in_buf;
				in_buf++;
				char_idx++;
			}
		}
	}
	OP_ASSERT(char_idx == str_len);
	int was_written, was_read = 0;
	if (m_8bit_enc)
	{
		tmp8[char_idx] = '\0';
		was_written = m_input_converter->Convert(tmp8, str_len+1, dst, dst_bytes, &was_read);
		if (was_read != str_len+1)
			return SPC_ERROR(OpStatus::ERR);
	}
	else
	{
		tmp32[char_idx] = '\0';
		was_written = m_input_converter->Convert(tmp32, (str_len+1)*sizeof(UINT32), dst, dst_bytes, &was_read);
		if (was_read != (int)((str_len+1)*sizeof(UINT32)))
			return SPC_ERROR(OpStatus::ERR);
	}
	m_allocator.OnlyUsed(0);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ConvertStringToUTF16(UINT8 *str, uni_char *buf, int buf_bytes)
{
	UTF8toUTF16Converter utf8_converter;
	CharConverter *converter = m_8bit_enc ? m_input_converter : &utf8_converter;
	int was_read = 0;
	int str_len = op_strlen((const char*)str)+1;
	int was_written = converter->Convert(str, str_len, buf, buf_bytes, &was_read);
	if (!was_written || was_written&1 || was_read != str_len)
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ConvertStringFromUTF16(const uni_char *word, UINT8 *buf, int buf_bytes)
{
	UTF16toUTF8Converter utf8_converter;
	CharConverter *converter = m_8bit_enc ? m_output_converter : &utf8_converter;
	int was_read = 0;
	int str_len = (uni_strlen(word)+1)*2;
	int was_written = converter->Convert(word, str_len, buf, buf_bytes, &was_read);
	if (!was_written || was_read != str_len)
		return OpStatus::ERR;
	return OpStatus::OK;
}

void OpSpellChecker::SetIndexCombinations(UINT32 *indexes, int len)
{
	int i, j, idx, mult;
	int index_count = m_mappings.GetSize()-1;
	if (len < m_combination_len)
	{
		for (i=0, idx=0, mult=1; i < len; i++, mult *= index_count)
			idx += (indexes[i] - 1) * mult;
		m_short_combinations[len-1][idx>>3] |= 1<<(idx&0x7);
	}
	else
	{
		int times = len - m_combination_len + 1;
		for (i=0;i<times;i++)
		{
			for (j=0, idx=0, mult=1; j < m_combination_len; j++, mult *= index_count)
				idx += (indexes[j] - 1) * mult;
			this->m_index_combinations[idx>>3] |= 1<<(idx&0x7);
			indexes++;
		}
	}
}

OP_STATUS OpSpellChecker::SetupIndexCombinations()
{
	int to_alloc;
	UINT64 indexes = m_mappings.GetSize()-1;
	UINT64 combinations = indexes;
	int comb_len = 1;
	while (combinations*indexes < SPC_MAX_COMB_BITS_BYTES*8)
	{
		to_alloc = (int)(combinations/8 + 1);
		m_short_combinations[comb_len-1] = OP_NEWA(UINT8, to_alloc);
		if (!m_short_combinations[comb_len-1])
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		m_stat_misc_lookup_bytes += to_alloc;
		combinations *= indexes;
		comb_len++;
	}
	m_combination_len = comb_len;
	to_alloc = (int)(combinations/8 + 1);
	m_index_combinations = OP_NEWA(UINT8, to_alloc);
	if (!m_index_combinations)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	m_stat_misc_lookup_bytes += to_alloc;
	op_memset(m_index_combinations,0,to_alloc);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::SetupHashBitField()
{
	int i;
	if (!m_total_word_count)
		return SPC_ERROR(OpStatus::ERR);
	for (i=sizeof(int)*8-1; !(m_total_word_count&1<<i); i--);
	int bits = 1<<i;
	while (bits/4 < SPC_MAX_HASH_LOOKUP_BYTES)
		bits *= 2;
	m_word_hash_pattern = bits-1;
	int to_alloc = m_word_hash_pattern/8 + 1;
	m_word_hash = OP_NEWA(UINT8, to_alloc);
	if (!m_word_hash)
		return SPC_ERROR(OpStatus::ERR);
	m_stat_misc_lookup_bytes += to_alloc;
	op_memset(m_word_hash,0,to_alloc);
	return OpStatus::OK;
}

static inline UINT32 huff_flip_bits(UINT32 code, UINT32 len)
{
	UINT32 i, flip_len, result;
	for (i = 0, flip_len = len-1, result = 0 ; i <= (len-1)>>1 ; i++, flip_len -= 2)
		result |= ((code & (1 << i)) << flip_len) | ((code & (1 << ((len-1) - i))) >> flip_len);
	return result;
}

OP_STATUS OpSpellChecker::SetupHuffmanCodes(int *counters, int len, HuffmanEntry *&code_to_value, HuffmanEntry *&value_to_code, int huffman_levels)
{
	int i;
	int total_count = 0;
	int count;
	OP_ASSERT(huffman_levels <= 16);
	
	IndexCounterMapping *index_to_counter = OP_NEWA(IndexCounterMapping, len);
	UINT8 *buf = OP_NEWA(UINT8, len*8);
	code_to_value = OP_NEWA(HuffmanEntry, 1<<huffman_levels);
	value_to_code = OP_NEWA(HuffmanEntry, len);
	if (!code_to_value || !value_to_code || !index_to_counter || ! buf)
	{
		OP_DELETEA(index_to_counter);
		OP_DELETEA(buf);
		OP_DELETEA(code_to_value);
		OP_DELETEA(value_to_code);
		code_to_value = NULL;
		value_to_code = NULL;
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	m_stat_misc_dictionary_bytes += ((1<<huffman_levels) + len) * sizeof(HuffmanEntry);
	for (i=0;i<len;i++)
		index_to_counter[i].Set(counters[i],i);
	quick_sort<IndexCounterMapping>(index_to_counter,len,buf);
	for (i=0;i<len;i++)
	{
		count = index_to_counter[i].count;
		total_count += count;
		if (count < 0 || total_count < 0)
		{
			OP_DELETEA(index_to_counter);
			OP_DELETEA(buf);
			return SPC_ERROR(OpStatus::ERR);
		}
	}
	int level = 1;
	int pos = 0;
	int idx = len-1;
	int left;
	while (idx >= 0)
	{
		count = index_to_counter[idx].count;
		left = (1<<level) - pos;
		if (level == huffman_levels || (INT64)count*(INT64)left >= (INT64)total_count && idx <= (left-1)<<(huffman_levels-level))
		{
			UINT32 code = huff_flip_bits((UINT32)pos, (UINT32)level);
			for (i=0; i+(int)code < 1<<huffman_levels; i += 1 << level)
			{
				OP_ASSERT(!code_to_value[i+code].code_length);
				code_to_value[i+code].Set((UINT16)(index_to_counter[idx].index), (UINT16)level);
			}
			value_to_code[index_to_counter[idx].index].Set(code, level);
			total_count -= count;
			idx--;
			pos++;
		}
		else
		{
			level++;
			pos <<= 1;
		}
	}
	OP_DELETEA(index_to_counter);
	OP_DELETEA(buf);
	return OpStatus::OK;
}

void OpSpellChecker::ConvertRepStrings()
{
	int i,j;
	ExpandingBuffer<UINT32*> new_rep(m_rep.GetSize());
	UINT32 *rep, *other_rep;
	for (i=0;i<m_rep.GetSize();i++)
	{
		rep = m_rep.GetElement(i);
		if (!ConvertCharsToIndexes(rep+1, rep+1, *rep))
			continue;
		UINT32 *to_rep = rep + 1 + *rep;
		if (*to_rep)
		{
			if (!ConvertCharsToIndexes(to_rep+1, to_rep+1, *to_rep))
				continue;
		}
		new_rep.AddElement(rep);
	}
	for (i=1;i<new_rep.GetSize();i++) // insertion sort the elements...
	{
		rep = new_rep.GetElement(i);
		for (j=i;j;j--)
		{
			other_rep = new_rep.GetElement(j-1);
			if (wide_strcmp(rep+1, *rep, other_rep+1, *other_rep) < 0)
				new_rep[j] = other_rep;
			else
				break;
		}
		new_rep[j] = rep;
	}
	m_rep.Reset();
	m_rep = new_rep;
	new_rep.Reset(FALSE);
}

BOOL OpSpellChecker::ConvertCharsToIndexes(UINT32 *src, UINT32 *dst, int len, BOOL replace_unknown)
{
	int i;
	UINT32 idx;
	if (m_8bit_enc)
	{
		for (i=0;i<len;i++)
		{
			if (src[i] & ~0xFF)
				return FALSE;
			idx = m_char_mappings.linear[src[i]].mapping_idx;
			if (!idx)
			{
				if (replace_unknown)
					idx = m_mappings.GetSize()-1;
				else
					return FALSE;
			}
			dst[i] = idx;
		}
	}
	else
	{
		for (i=0;i<len;i++)
		{
			if (src[i] >= SPC_UNICODE_REAL_CODEPOINTS)
				return FALSE;
			idx = m_char_mappings.utf->GetVal(src[i]).mapping_idx;
			if (!idx)
			{
				if (replace_unknown)
					idx = m_mappings.GetSize()-1;
				else
					return FALSE;
			}
			dst[i] = idx;
		}
	}
	return TRUE;
}

void OpSpellChecker::GetMappingPosToChar(UINT32 *chars, int len, PosToChar *pos_to_char, int &pos_count)
{
	int i;
	UINT32 ch;
	MappingPointer mp;
	pos_count = 0;
	for (i=0;i<len;i++)
	{
		ch = chars[i];
		if (m_8bit_enc)
			mp = m_char_mappings.linear[ch];
		else
			mp = m_char_mappings.utf->GetVal(ch);
		if (mp.mapping_pos)
			pos_to_char[pos_count++] = PosToChar(i,ch);
	}
}

// bit-flags + affix-flag-count + *affixes + compound-rule-flag-count + *compound-rule-flags
OP_STATUS OpSpellChecker::GetFlagPtr(UINT8 *str, UINT16 *&result, HunspellAffixFileData *fd, OpSpellCheckerAllocator *allocator)
{
	if (!result)
	{
		result = (UINT16*)allocator->Allocate16((SPC_MAX_FLAGS_PER_LINE+3)*sizeof(UINT16));
		if (!result)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	else
		allocator = NULL;
	FlagEncoding flag_encoding = fd->GetFlagEncoding();
	FlagClass *flag_class;
	int bytes_used = 0, flag_index;
	FlagAlias *alias;
	UINT16 bit_flags = 0;
	UINT16 compound_rule_indexes[SPC_MAX_COMPOUND_RULE_FLAGS_PER_LINE];
	int compound_rule_index_count = 0;
	BOOL has_aliases = !!fd->GetAliasCount();
	int i = 2,j;
	while (i < SPC_MAX_FLAGS_PER_LINE)
	{
		if (has_aliases && (alias = fd->GetAlias(decode_flag(str,FLAG_NUM_ENC,bytes_used))) != NULL)
		{
			int alias_flag_count = alias->GetFlagCount();
			if (i + alias_flag_count >= SPC_MAX_FLAGS_PER_LINE)
				break;
			for (j=0;j<alias_flag_count;j++)
			{
				flag_class = fd->GetFlagClassByGlobalId(alias->GetFlags()[j]);
				OP_ASSERT(flag_class);
				if (flag_class->GetType() == AFFIX_TYPE_AFFIX)
					result[i++] = (UINT16)flag_class->GetFlagGlobalId();
				else if (flag_class->GetType() == AFFIX_TYPE_COMPOUNDRULEFLAG)
				{
					if (compound_rule_index_count <= SPC_MAX_COMPOUND_RULE_FLAGS_PER_LINE)
						compound_rule_indexes[compound_rule_index_count++] = flag_class->GetFlagTypeId();
				}
				else
				{
					OP_ASSERT(flag_class->GetType() < AFFIX_BIT_TYPE_COUNT);
					bit_flags |= 1 << flag_class->GetType();
				}
			}
		}
		else
		{
			if ((flag_index = decode_flag(str,flag_encoding,bytes_used)) < 0)
				break;
			flag_class = fd->GetFlagClass(flag_index);
			if (flag_class)
			{
				if (flag_class->GetType() == AFFIX_TYPE_AFFIX)
					result[i++] = (UINT16)flag_class->GetFlagGlobalId();
				else if (flag_class->GetType() == AFFIX_TYPE_COMPOUNDRULEFLAG)
				{
					if (compound_rule_index_count <= SPC_MAX_COMPOUND_RULE_FLAGS_PER_LINE)
						compound_rule_indexes[compound_rule_index_count++] = flag_class->GetFlagTypeId();
				}
				else
				{
					OP_ASSERT(flag_class->GetType() < AFFIX_BIT_TYPE_COUNT);
					bit_flags |= 1 << flag_class->GetType();
				}
			}
		}
		str += bytes_used;
		while (*str == ',')
			str++;
	}
	int affix_count = i-2;
	if (affix_count)
	{
		result[1] = affix_count;
		bit_flags |= 1<<AFFIX_BIT_HAS_AFFIXES; 
	}
	int pos = affix_count ? i : 1; // count + compound-rule-flags after affixes if such exists, or immediately after bit-flags otherwise
	if (compound_rule_index_count)
	{
		bit_flags |= 1<<AFFIX_BIT_HAS_COMPOUNDRULEFLAG;
		result[pos++] = compound_rule_index_count;
		for (j=0;j<compound_rule_index_count;j++)
			result[pos++] = compound_rule_indexes[j];
	}
	result[0] = bit_flags;
	if (!bit_flags)
	{
		if (allocator)
			allocator->OnlyUsed(0);
		result = NULL;
	}
	else if (allocator)
		allocator->OnlyUsed(pos*sizeof(UINT16));
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::GetWideStrPtr(UINT8 *str, UINT32 *&result, OpSpellCheckerAllocator *allocator)
{
	return get_wide_str_ptr(str, *&result, m_8bit_enc, allocator);
}

OP_STATUS OpSpellChecker::ParseFlagGeneral(HunspellAffixFileData *fd, HunspellAffixFileParseState *state, AffixFlagType type)
{
	int dummy;
	HunspellAffixFileLine *line = state->GetCurrentLine();
	state->SetFlagsUsed();
	if (!line->GetWordCount())
		return SPC_ERROR(OpStatus::ERR);
	int flag_index = decode_flag(line->WordAt(0),fd->GetFlagEncoding(),dummy);
	if (flag_index < 0)
		return SPC_ERROR(OpStatus::ERR);
	if (fd->GetFlagClass(flag_index))
		return SPC_ERROR(OpStatus::ERR);
	
	FlagClass *flag_class = OP_NEW(FlagClass, (type,flag_index));
	if (!flag_class)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	OP_STATUS status = fd->AddFlagClass(flag_class,state); // deleted flag_class if returning error
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseHunspellAffix(HunspellAffixFileData *fd, HunspellAffixFileParseState *state, double time_out, BOOL &finished)
{
	HunspellAffixFileLine *line;
	AffixParser *parser;
	int i = 10;
	finished = FALSE;
	while (state->HasMoreLines())
	{
		while (state->HasMoreLines())
		{
			line = state->GetCurrentLine();
			parser = GetAffixParser(line->Type());
			RETURN_IF_ERROR((this->*(parser->func))(fd,state));
			if (state->GetCurrentLine() == line) // Only iterate if function hasn't iterated current line itself.
				state->NextLine();
			if (!--i)
			{
				if (time_out != 0.0 && time_out <= g_op_time_info->GetWallClockMS())
					return OpStatus::OK;
				i = 10;
			}
		}
		RETURN_IF_ERROR(PostProcessPass(fd,state));
		state->NextPass();
	}
	fd->ClearFileData();
	finished = TRUE;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseHunspellDictionary(double time_out, BOOL &finished)
{
	int current_line_index;
	finished = FALSE;
	while (m_dic_parse_state->GetPass() < 4)
	{
		int pass = m_dic_parse_state->GetPass();
		OP_STATUS (OpSpellChecker::*func)();
		if (pass == 0)
			func = &OpSpellChecker::DictionaryParsePass0;
		else if (pass == 1)
			func = &OpSpellChecker::DictionaryParsePass1;
		else if (pass == 2)
			func = &OpSpellChecker::DictionaryParsePass2;
		else
			func = &OpSpellChecker::DictionaryParsePass3;
		int last_idx_shift = 0;
		while (m_dic_parse_state->HasMoreLines())
		{
			current_line_index = m_dic_parse_state->GetCurrentLineIndex();
			RETURN_IF_ERROR((this->*(func))());
			if (current_line_index == m_dic_parse_state->GetCurrentLineIndex()) // Only iterate if function hasn't iterated current line itself.
				m_dic_parse_state->NextLine();
			if (current_line_index>>3 != last_idx_shift)
			{
				if (time_out != 0.0 && time_out <= g_op_time_info->GetWallClockMS())
					return OpStatus::OK;
				last_idx_shift = current_line_index>>3;
			}
		}
		if (pass == 0)
			RETURN_IF_ERROR(DictionaryPostProcessPass0());
		else if (pass == 1)
			RETURN_IF_ERROR(DictionaryPostProcessPass1());
		else if (pass == 2)
			RETURN_IF_ERROR(DictionaryPostProcessPass2());
		else
			RETURN_IF_ERROR(DictionaryPostProcessPass3());
		m_dic_parse_state->NextPass();
	}
	RETURN_IF_ERROR(RemoveWordList(m_dic_file_data->GetDictionaryChunks()->GetRemoveData(), m_dic_file_data->GetDictionaryChunks()->GetRemoveDataLen()));
	finished = TRUE;
	return OpStatus::OK;
}

int OpSpellChecker::GetAffixTypeFromString(UINT8 *str)
{
	int i;
	if (!*str || *str == '#' || !m_affix_parsers)
		return HS_INVALID_LINE;
	for (i=0;i<m_affix_parser_count;i++)
	{
		if (i != HS_INVALID_LINE && str_eq(m_affix_parsers[i].str,str))
			return i;
	}
	return HS_INVALID_LINE;
}

AffixParser *OpSpellChecker::GetAffixParser(int type)
{
	if (!m_affix_parsers || type == HS_INVALID_LINE || type < 0 || type >= m_affix_parser_count)
		return NULL;
	return &m_affix_parsers[type];
}

OP_STATUS OpSpellChecker::RegisterParserFunc(OP_STATUS (OpSpellChecker::*func)(HunspellAffixFileData*,HunspellAffixFileParseState*), const char *str)
{
#define SPC_MAX_AFFIX_PARSERS 100
	if (!m_affix_parsers)
	{
		m_affix_parsers = OP_NEWA(AffixParser, SPC_MAX_AFFIX_PARSERS);
		if (!m_affix_parsers)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		m_affix_parser_count = 0;
	}
	if (m_affix_parser_count == HS_INVALID_LINE)
		m_affix_parser_count++;
	if (m_affix_parser_count >= 100)
	{
		OP_ASSERT(FALSE); // too many parser functions...
		return SPC_ERROR(OpStatus::ERR);
	}
	m_affix_parsers[m_affix_parser_count++] = AffixParser(func,(UINT8*)str);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::RegisterAffixParsingFunctions()
{
#define REG_AFFIX_PARSER(x) RETURN_IF_ERROR(RegisterParserFunc(&OpSpellChecker::Parse##x,#x))
	REG_AFFIX_PARSER(SFX);
	REG_AFFIX_PARSER(PFX);
	REG_AFFIX_PARSER(REP);
	REG_AFFIX_PARSER(PHONE);
	REG_AFFIX_PARSER(KEY);
	REG_AFFIX_PARSER(AF);
	REG_AFFIX_PARSER(TRY);
	REG_AFFIX_PARSER(SET);
	REG_AFFIX_PARSER(FLAG);
	REG_AFFIX_PARSER(COMPLEXPREFIXES);
	REG_AFFIX_PARSER(COMPOUNDFLAG);
	REG_AFFIX_PARSER(COMPOUNDBEGIN);
	REG_AFFIX_PARSER(COMPOUNDMIDDLE);
	REG_AFFIX_PARSER(COMPOUNDEND);
	REG_AFFIX_PARSER(COMPOUNDWORDMAX);
	REG_AFFIX_PARSER(COMPOUNDROOT);
	REG_AFFIX_PARSER(COMPOUNDPERMITFLAG);
	REG_AFFIX_PARSER(COMPOUNDFORBIDFLAG);
	REG_AFFIX_PARSER(CHECKCOMPOUNDDUP);
	REG_AFFIX_PARSER(CHECKCOMPOUNDREP);
	REG_AFFIX_PARSER(CHECKCOMPOUNDTRIPLE);
	REG_AFFIX_PARSER(CHECKCOMPOUNDCASE);
	REG_AFFIX_PARSER(NOSUGGEST);
	REG_AFFIX_PARSER(FORBIDDENWORD);
	REG_AFFIX_PARSER(LEMMA_PRESENT);
	REG_AFFIX_PARSER(CIRCUMFIX);
	REG_AFFIX_PARSER(ONLYINCOMPOUND);
	REG_AFFIX_PARSER(PSEUDOROOT);
	REG_AFFIX_PARSER(NEEDAFFIX);
	REG_AFFIX_PARSER(COMPOUNDMIN);
	REG_AFFIX_PARSER(COMPOUNDSYLLABLE);
	REG_AFFIX_PARSER(SYLLABLENUM);
	REG_AFFIX_PARSER(CHECKNUM);
	REG_AFFIX_PARSER(WORDCHARS);
	REG_AFFIX_PARSER(IGNORE);
	REG_AFFIX_PARSER(CHECKCOMPOUNDPATTERN);
	REG_AFFIX_PARSER(COMPOUNDRULE);
	REG_AFFIX_PARSER(MAP);
	REG_AFFIX_PARSER(BREAK);
	REG_AFFIX_PARSER(LANG);
	REG_AFFIX_PARSER(VERSION);
	REG_AFFIX_PARSER(MAXNGRAMSUGS);
	REG_AFFIX_PARSER(NOSPLITSUGS);
	REG_AFFIX_PARSER(SUGSWITHDOTS);
	REG_AFFIX_PARSER(KEEPCASE);
	REG_AFFIX_PARSER(SUBSTANDARD);
	REG_AFFIX_PARSER(CHECKSHARPS);
	return OpStatus::OK;
}

void OpSpellChecker::GetAllocatedBytes(UINT32 &for_dictionary, UINT32 &for_lookup)
{
	for_dictionary = 0;
	for_lookup = 0;
	for_dictionary += sizeof(*this);
	for_dictionary += m_allocator.GetAllocatedBytes();
	for_dictionary += m_stat_misc_dictionary_bytes;
	for_dictionary += m_compound_rules.GetAllocatedBytes();
	for_dictionary += m_compound_rules.GetSize() * sizeof(CompoundRule);
	for_dictionary += m_rep.GetAllocatedBytes();
	for_dictionary += m_mappings.GetAllocatedBytes();
	if (m_8bit_enc)
	{
		for_dictionary += 256 * sizeof(*m_char_mappings.linear);
	}
	else
	{
		for_dictionary += m_char_mappings.utf->GetAllocatedBytes();
	}
	for_dictionary += m_len_to_bit_representation.GetAllocatedBytes();

	for_lookup += m_stat_misc_lookup_bytes;
}

OP_STATUS OpSpellChecker::LoadDictionary(uni_char *dic_path, uni_char *affix_path, uni_char *own_path, double time_out, BOOL &finished, uni_char **error_str)
{
	m_stat_start_time = g_op_time_info->GetWallClockMS();
	OP_ASSERT(m_state == INIT_STATE && dic_path);
	RETURN_IF_ERROR(InitMemoryStructs());
	int affix_len = 0;
	UINT8 *affix_data = NULL;
	m_error_str = error_str;
	if (affix_path)
	{
		OpFile affix_file;
		RETURN_IF_ERROR(SCOpenFile(affix_path, affix_file, affix_len));
		if (affix_len > 2)
		{
			affix_data = OP_NEWA(UINT8, affix_len);
			if (!affix_data)
			{
				affix_file.Close();
				return SPC_ERROR(OpStatus::ERR);
			}
			OpFileLength _affix_len = (OpFileLength)affix_len;
			OP_STATUS status = affix_file.Read(affix_data, _affix_len, &_affix_len);
			if (OpStatus::IsError(status) || _affix_len != (OpFileLength)affix_len)
			{
				OP_DELETEA(affix_data);
				affix_file.Close();
				return SPC_ERROR(OpStatus::IsError(status) ? status: OpStatus::ERR);
			}
		}
		affix_file.Close();
	}
	if (!affix_data)
	{
		UINT8 *dummy_data = (UINT8*)"SET ISO8859-1";
		affix_len = op_strlen((const char*)dummy_data)+1;
		affix_data = OP_NEWA(UINT8, affix_len);
		if (!affix_data)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		op_strcpy((char*)affix_data, (const char*)dummy_data);
	}
	m_affix_file_data = OP_NEW(HunspellAffixFileData, (affix_data,affix_len,this));
	if (!m_affix_file_data)
	{
		OP_DELETEA(affix_data);
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}

	OP_STATUS status = RegisterAffixParsingFunctions();
	if (OpStatus::IsError(status))
		goto failed_load_dictionary;

	status = m_affix_file_data->SetFlagEncoding(FLAG_ASCII_ENC);
	if (OpStatus::IsError(status))
		goto failed_load_dictionary;

	status = m_affix_file_data->SetDictionaryEncoding(m_encoding);
	if (OpStatus::IsError(status))
		goto failed_load_dictionary;

	status = m_affix_file_data->Tokenize();
	if (OpStatus::IsError(status))
		goto failed_load_dictionary;

	m_affix_file_parse_state = HunspellAffixFileParseState::Create(m_affix_file_data);
	if (!m_affix_file_parse_state)
	{
		status = OpStatus::ERR_NO_MEMORY;
		goto failed_load_dictionary;
	}

	status = UniSetStr(m_dictionary_path, dic_path);
	if (OpStatus::IsError(status))
		goto failed_load_dictionary;

	status = UniSetStr(m_own_path, own_path);
	if (OpStatus::IsError(status))
		goto failed_load_dictionary;

	m_state = PARSING_AFFIXES;

	return ContinueLoadingDictionary(time_out, finished, error_str);

failed_load_dictionary:
	ClearDictionary();
	return status;
}

OP_STATUS OpSpellChecker::ContinueLoadingDictionary(double time_out, BOOL &finished, uni_char **error_str)
{
	OP_ASSERT(m_state != INIT_STATE && m_state != LOADING_FINISHED);
	m_error_str = error_str;
	if (m_state == PARSING_AFFIXES)
	{
		RETURN_AND_CLEAR_IF_ERROR(ParseHunspellAffix(m_affix_file_data,m_affix_file_parse_state,0,finished));
		if (!finished)
			return OpStatus::OK;
		m_state = READING_DICTIONARY;
	}
	if (m_state == READING_DICTIONARY)
	{
		if (!m_dic_file_chunks)
			RETURN_AND_CLEAR_IF_NULL(m_dic_file_chunks = OP_NEW(DictionaryChunks, (m_dictionary_path, m_own_path)));
		RETURN_AND_CLEAR_IF_ERROR(m_dic_file_chunks->ReadDictionaryChunks(time_out, finished));
		if (!finished)
			return OpStatus::OK;
		m_state = PARSING_DICTIONARY;
	}
	if (m_state == PARSING_DICTIONARY)
	{
		if (!m_dic_file_data)
		{
			RETURN_AND_CLEAR_IF_NULL(m_dic_file_data = OP_NEW(HunspellDictionaryFileData, (m_dic_file_chunks,this,m_affix_file_data)));
			RETURN_AND_CLEAR_IF_ERROR(m_dic_file_data->Init());
			RETURN_AND_CLEAR_IF_NULL(m_dic_parse_state = OP_NEW(HunspellDictionaryParseState, (m_dic_file_data, 4)));
		}
		RETURN_AND_CLEAR_IF_ERROR(ParseHunspellDictionary(time_out,finished));
		if (!finished)
			return OpStatus::OK;
		
		OP_DELETE(m_affix_file_data);
		OP_DELETE(m_affix_file_parse_state);
		OP_DELETE(m_dic_file_data);
		OP_DELETE(m_dic_parse_state);
		OP_DELETE(m_dic_file_chunks);
		m_affix_file_data = NULL;
		m_affix_file_parse_state = NULL;
		m_dic_file_data = NULL;
		m_dic_parse_state = NULL;
		m_dic_file_chunks = NULL;

		m_state = LOADING_FINISHED;
#ifdef SPC_DEBUG
		DebugPrintStat();
#endif
	}
	return OpStatus::OK;
}

#ifdef SPC_DEBUG

void OpSpellChecker::DebugPrintStat()
{
	int i;
	UINT8 lang[200];
	const uni_char *uni_lang = m_language->GetLanguageString();
	for (i=0;uni_lang[i];i++)
		lang[i] = (UINT8)uni_lang[i];
	lang[i] = '\0';
	sc_printf("\n======================%s===========================\n", lang)
	UINT32 load_time = (UINT32)(g_op_time_info->GetWallClockMS() - m_stat_start_time);
	load_time++; // gcc warning-fix
	load_time--; // gcc warning-fix
	sc_printf("LoadTime: %d,%d seconds\n", load_time/1000, load_time%1000);
	sc_printf("Total words: %d\n", m_stat_count_total);
	UINT32 for_dictionary = 0, for_lookup = 0;
	GetAllocatedBytes(for_dictionary, for_lookup);
	sc_printf("Dictionary bytes: %d for %d words (%f bytes per word)...\n", for_dictionary, m_stat_count_total, (float)for_dictionary / (float)m_stat_count_total);
	sc_printf("Fast lookup bytes: %d\n", for_lookup);
	sc_printf("Total bytes: %d\n", for_lookup+for_dictionary);
	sc_printf("Words per radix entry: %f\n", (float)GetTotalCount() / (float)GetRadixCounters());
	sc_printf("Radix max entry: %d\n", GetRadixMaxCounter());
}

#endif

/* ===============================HunspellAffixFileParseState=============================== */

/* static */
HunspellAffixFileParseState* HunspellAffixFileParseState::Create(HunspellAffixFileData *fd)
{
	OP_ASSERT(fd);

	HunspellAffixFileParseState* state = OP_NEW(HunspellAffixFileParseState, (fd));
	if (state)
	{
		state->m_current_indexes = OP_NEWA(int, fd->LineCount());
		state->m_next_indexes = OP_NEWA(int, fd->LineCount());
		if (!state->m_current_indexes || !state->m_next_indexes)
		{
			OP_DELETE(state);
			state = NULL;
		}
		else
		{
			for (int i = 0;i < fd->LineCount();i++)
			{
				if (fd->LineAt(i)->Type() != HS_INVALID_LINE)
					state->m_current_indexes[state->m_current_count++] = i;
			}
		}
	}
	return state;
}


HunspellAffixFileParseState::HunspellAffixFileParseState(HunspellAffixFileData *fd)
{
	OP_ASSERT(fd);
	m_pass = 0;
	m_flags_used = FALSE;
	m_dic_chars_used = FALSE;
	m_aliases_detected = FALSE;
	m_map_detected = FALSE;
	m_file_data = fd;
	m_current_indexes = NULL;
	m_next_indexes = NULL;
	m_current = 0;
	m_current_count = 0;
	m_next_count = 0;
	
}
HunspellAffixFileParseState::~HunspellAffixFileParseState()
{
	OP_DELETEA(m_current_indexes);
	OP_DELETEA(m_next_indexes);
}

int HunspellAffixFileParseState::SetCurrentLine(int line)
{
	int current_index = GetCurrentLineIndex();
	OP_ASSERT(HasMoreLines() && line >= current_index);
	if (line == current_index)
		return line;
	while (m_current_indexes[m_current] < line)
		if (++m_current >= m_current_count)
			break;
	return GetCurrentLineIndex();
}

int HunspellAffixFileParseState::GetCurrentLineIndex()
{ 
	return m_current < m_current_count ? m_current_indexes[m_current] : -1; 
}

HunspellAffixFileLine* HunspellAffixFileParseState::GetCurrentLine()
{
	int i = GetCurrentLineIndex();
	return i < 0 ? NULL : m_file_data->LineAt(i); 
}

HunspellAffixFileLine *HunspellAffixFileParseState::GetLineInPass(int index)
{
	if (index >= m_current_count)
		return NULL;
	return m_file_data->LineAt(m_current_indexes[index]);
}

BOOL HunspellAffixFileParseState::NextLine()
{
	if (!HasMoreLines())
		return FALSE;
	m_current++;
	return HasMoreLines();
}

BOOL HunspellAffixFileParseState::NextPass()
{
	if (!m_next_count)
		return FALSE;
	int *tmp = m_current_indexes;
	m_current_indexes = m_next_indexes;
	m_current_count = m_next_count;
	m_current = 0;
	m_next_indexes = tmp;
	m_next_count = 0;
	m_pass++;
	return TRUE;
}

void HunspellAffixFileParseState::IterateInNextPass(int line_index)
{
	int i;
	OP_ASSERT(line_index >= 0 && line_index < m_file_data->LineCount());
	m_next_indexes[m_next_count++] = line_index;
	for (i = m_next_count - 1; i > 0 && m_next_indexes[i-1] >= line_index; i--) // place in order (might never be necessary...)
	{
		if (m_next_indexes[i-1] == line_index) // index already existed...???
		{
			for (i = i - 1; i < m_next_count; i++)
				m_next_indexes[i] = m_next_indexes[i+1];
			m_next_count--;
			return;
		}
		m_next_indexes[i] = m_next_indexes[i-1];
		m_next_indexes[i-1] = line_index;
	}
}
#else  //!SPC_USE_HUNSPELL_ENGINE

OpSpellChecker::OpSpellChecker(OpSpellCheckerLanguage *language) :
		m_language(language),
		m_hunspell(NULL),
		m_dictionary_path(NULL),
		m_own_path(NULL),
		m_tmp_temp_buffer_users(0),
		m_8bit_enc(FALSE),
		m_output_converter(NULL),
		m_input_converter(NULL),
		m_breaks(NULL),
		m_num_breaks(0)
{
}

OpSpellChecker::~OpSpellChecker()
{
    OP_DELETE(m_hunspell);
	OP_DELETEA(m_dictionary_path);
	OP_DELETEA(m_own_path);
	OP_DELETE(m_output_converter);
	OP_DELETE(m_input_converter);

	for (int i=0; i < m_num_breaks; i++)
	{
		op_free(m_breaks[i]);
	}
	op_free(m_breaks);
}

BOOL OpSpellChecker::FreeCachedData(BOOL toplevel_context)
{
	return TRUE;
}

OP_STATUS OpSpellChecker::LoadDictionary(uni_char *dic_path, uni_char *affix_path, uni_char *own_path, double time_out, BOOL &finished, uni_char **error_str)
{
	OpString8 utf8_aff_path;
	OpString8 utf8_dic_path;
	Hunspell* hunspell = NULL;
	uni_char *dicp=NULL, *ownp=NULL;
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	BOOL utf8;
	OutputConverter *oc = NULL;
	InputConverter *ic = NULL;
	const char *char_set;


	RETURN_IF_ERROR(utf8_aff_path.SetUTF8FromUTF16(affix_path));
	RETURN_IF_ERROR(utf8_dic_path.SetUTF8FromUTF16(dic_path));

	hunspell = OP_NEW(Hunspell,(utf8_aff_path.CStr(), utf8_dic_path.CStr()));
	if ( hunspell == NULL )
		goto handle_error;
	

	dicp = SetNewStr(dic_path);
	if ( dicp == NULL )
		goto handle_error;

	ownp = SetNewStr(own_path);
	if ( ownp == NULL && own_path != NULL)
		goto handle_error;


	char_set = hunspell->get_dic_encoding();
	OP_ASSERT(char_set);

	utf8 = op_strcmp(char_set, "UTF-8") == 0;

	status = OutputConverter::CreateCharConverter(char_set, &oc, FALSE, TRUE);
	if ( OpStatus::IsError(status) )
		goto handle_error;
	
	status = InputConverter::CreateCharConverter(char_set, &ic);
	if ( OpStatus::IsError(status) )
		goto handle_error;

	status = CreateWordChars(hunspell);
	if ( OpStatus::IsError(status) )
		goto handle_error;

	m_hunspell = hunspell;

	status = CreateBreaks();
	if ( OpStatus::IsError(status) )
		goto handle_error;

	m_dictionary_path = dicp;
	m_own_path = ownp;

	m_8bit_enc = !utf8;

	m_output_converter = oc;
	m_input_converter = ic;

	finished = TRUE;

	OpStatus::Ignore(LoadOwnFile());

	return OpStatus::OK;

  handle_error:
	m_hunspell = NULL;
	OP_DELETE(hunspell);
	OP_DELETEA(dicp);
	OP_DELETEA(ownp);

	OP_DELETE(oc);
	OP_DELETE(ic);

	return SPC_ERROR(status);
}

OP_STATUS OpSpellChecker::AddWordChar(const uni_char b)
{
	return m_word_chars.Append(&b, 1);
}


OP_STATUS OpSpellChecker::CreateWordChars(Hunspell* hunspell)
{
	OP_ASSERT( hunspell != NULL );

	int len = 0;
	unsigned short* word_chars_utf16 = hunspell->get_wordchars_utf16(&len);
	OpString wcs;

	if ( word_chars_utf16 )
	{
		uni_char* word_chars = wcs.Reserve(len);
		if ( !word_chars )
			return OpStatus::ERR_NO_MEMORY;
		
		for ( int i = 0; i < len; i++ )
			word_chars[i] = (uni_char) word_chars_utf16[i];
		word_chars[len] = 0;
	}
	else
	{
		const char* word_chars = hunspell->get_wordchars();
		if ( word_chars )
		{
			char* encoding = hunspell->get_dic_encoding();
			RETURN_IF_ERROR(SetFromEncoding(&wcs, encoding, word_chars, op_strlen(word_chars)));
		}
	}

	uni_char* word_chars = m_word_chars.Reserve(wcs.Length());
	if ( word_chars == NULL )
		return OpStatus::ERR_NO_MEMORY;

	uni_char* wc = wcs.CStr();
	len = wcs.Length();
	int wlen = 0;
	for (int i=0; i < len; i++)
	{
		if ( g_internal_spellcheck->IsWordSeparator(wc[i]) )
			word_chars[wlen++] = wc[i];
	}
	word_chars[wlen] = 0;

	return OpStatus::OK;
}

/** Adjusts the start and length of a break string to only include the actual break characters, and not the operators.
 *
 * After adjust break it is possible to check if the break is a start or end break.
 * If the break pointer has changed the break is a start break.
 * If the character after the end of the string is '$' instead of '\0' it is an end break
 * (i.e. b[bl]=='$', where b is the adjusted break and bl is the adjusted length).
 *
 * @param b the break string to adjust. The pointer may be changed if the break is a start break.
 * @return The length of the adjusted break (N.B. it might be smaller than the length of the adjusted string if the break is an end break).
 */
static int adjust_break(uni_char* &b)
{
	int bl = uni_strlen(b);

	if ( bl > 1 && b[0] == '^' ) // ^ means break only valid at start of word.
	{
		b++;
		bl--;
	}
	if ( bl > 1 && b[bl-1] == '$' ) // $ means break only valid at end of word.
		bl--;

	return bl;
}


static int cmp_break_len(const void *p1, const void *p2)
{
	uni_char* b1 = *(uni_char * const *)p1;
	uni_char* b2 = *(uni_char * const *)p2;

	int bl1 = b1 ? adjust_break(b1) : 0;
	int bl2 = b2 ? adjust_break(b2) : 0;

	return bl2 - bl1;
}


OP_STATUS OpSpellChecker::AddBreak(const uni_char* b)
{
	if ( m_num_breaks % 10 == 0 )
	{
		uni_char** breaks = (uni_char**) op_realloc(m_breaks, (m_num_breaks + 10)*sizeof(uni_char*));
		if ( breaks == NULL )
			return OpStatus::ERR_NO_MEMORY;
		m_breaks = breaks;
	}

	m_breaks[m_num_breaks] = uni_strdup(b);
	if ( m_breaks[m_num_breaks] == NULL )
		return OpStatus::ERR_NO_MEMORY;
	m_num_breaks++;

	return OpStatus::OK;
}


// FIXME: What if a word break contains another word break?
// FIXME: What about entities? Are those supposed to work?

OP_STATUS OpSpellChecker::CreateBreaks()
{
	OP_ASSERT(m_hunspell != NULL);

	// Add ' break rules to handle single quote quotations.
	RETURN_IF_ERROR(AddBreak(UNI_L("^'")));
	RETURN_IF_ERROR(AddBreak(UNI_L("'$")));
	RETURN_IF_ERROR(AddWordChar('\''));

	int num_breaks;
	char** breaks = m_hunspell->get_wordbreaks(&num_breaks);
	char* encoding = m_hunspell->get_dic_encoding();

	OpString bs;
	int i;
	for (i = 0; i < num_breaks; i++)
	{
		RETURN_IF_ERROR(SetFromEncoding(&bs, encoding, breaks[i], op_strlen(breaks[i])));
		RETURN_IF_ERROR(AddBreak(bs.CStr()));
	}

	// A word break must be either all word separators or all non-word separators.
	// To make it so, it is sometimes needed to add word separators as word chars.
	// It complicates things when previously added breaks contains the word separators that
	// were added as word chars. Then we might need to add more word chars to make them all non-word separators.
	// So, we need to recheck all breaks until there are no breaks containing both kind of characters.

	for (i = 0; i < m_num_breaks; i++)
	{
		uni_char* b = m_breaks[i];
		int bl = adjust_break(b);

		BOOL has_word_separator = IsWordSeparator(b[0]);
		int j;
		for (j = 1; j < bl; j++ )
			if ( has_word_separator != IsWordSeparator(b[j]) )
				break;
		
		if ( j < bl )
		{
			for (j = 0 < bl; j < bl; j++)
				if ( IsWordSeparator(b[j]) )
					AddWordChar(b[j]);
			i = 0;
		}
	}

	// Drop all breaks that (still) contain only word separators. Those will be stripped before the words reaches the spellchecker anyway.
	int drop_count = 0;
	for (i = 0; i < m_num_breaks; i++)
	{
		uni_char* b = m_breaks[i];
		adjust_break(b);

		if ( IsWordSeparator(*b) )
		{
			op_free(m_breaks[i]);
			m_breaks[i] = NULL; // No need to move stuff, the NULLs will be put at the end by the qsort below.
			drop_count++;
		}
	}

	// Sort by length. Other parts of the code assumes it to be sorted.
	op_qsort(m_breaks, m_num_breaks, sizeof(uni_char*), cmp_break_len);


	m_num_breaks -= drop_count;
	uni_char** brks = (uni_char**) op_realloc(m_breaks, sizeof(uni_char*)*m_num_breaks);
	if ( brks == NULL )
		return OpStatus::ERR_NO_MEMORY;

	m_breaks = brks;

	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::AddToHunspell(const uni_char* inword)
{
	OP_ASSERT(m_hunspell);

	if (!CanRepresentInDictionaryEncoding(inword))
		return OpStatus::ERR;

	char *encoding = m_hunspell->get_dic_encoding();
	OP_ASSERT( encoding != NULL );

	OpString8 word;

	RETURN_IF_LEAVE(SetToEncodingL(&word, encoding, inword));  //FIXME: Use char converter.

	m_hunspell->add(word.CStr()); //FIXME: Check result.
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::RemoveFromHunspell(const uni_char* inword)
{
	OP_ASSERT(m_hunspell);

	if (!CanRepresentInDictionaryEncoding(inword))
		return OpStatus::ERR;

	char *encoding = m_hunspell->get_dic_encoding();
	OP_ASSERT( encoding != NULL );

	OpString8 word;

	RETURN_IF_LEAVE(SetToEncodingL(&word, encoding, inword));  //FIXME: Use char converter.

	m_hunspell->remove(word.CStr()); //FIXME: Check result.

	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::AddWord(const uni_char *word, BOOL permanently)
{
	OP_STATUS status;
	if (!CanAddWord(word) || m_added_words.HasString(word))
		return OpStatus::OK;

	if (m_removed_words.HasString(word))
	{
		BOOL dummy = FALSE;
		m_removed_words.RemoveString(word);
		if (permanently)
			RETURN_IF_ERROR(RemoveStringsFromOwnFile(word,FALSE,dummy));
		OpStatus::Ignore(AddToHunspell(word));
		return OpStatus::OK;
	}
	else
	{
		BOOL has_word = FALSE;
		RETURN_IF_ERROR(IsWordInDictionary(word, has_word));
		if (has_word)
			return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_added_words.AddString(word));
	OpStatus::Ignore(AddToHunspell(word));
	if (!permanently)
		return OpStatus::OK;

	RETURN_IF_ERROR(CreateOwnFileIfNotExists());
	OpFile own_file;
	int file_len = 0;
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len, OPFILE_READ));
	int word_bytes = to_utf8(NULL, word, SPC_TEMP_BUFFER_BYTES+1);
	char *new_data = OP_NEWA(char, file_len+word_bytes);
	if (!new_data || word_bytes > SPC_TEMP_BUFFER_BYTES)
	{
		own_file.Close();
		OP_DELETEA(new_data);
		return new_data ? SPC_ERROR(OpStatus::ERR) : SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	to_utf8(new_data, word, word_bytes);
	new_data[word_bytes-1] = '\n';
	OpFileLength bytes_read = 0;
	status = own_file.Read(new_data+word_bytes, (OpFileLength)file_len, &bytes_read);
	if (OpStatus::IsSuccess(status))
	{
		own_file.Close();
		int dummy = 0;
		status = SCOpenFile(m_own_path, own_file, dummy, OPFILE_WRITE);
		if (OpStatus::IsSuccess(status))
			status = own_file.SetFileLength(0);
		if (OpStatus::IsSuccess(status))
			status = own_file.SetFilePos(0); // not so necessary I guess...
		if (OpStatus::IsSuccess(status))
			status = own_file.Write(new_data, (OpFileLength)(file_len+word_bytes));
	}
	own_file.Close();
	OP_DELETEA(new_data);
	return OpStatus::IsError(status) ? SPC_ERROR(status) : OpStatus::OK;
}

OP_STATUS OpSpellChecker::RemoveWord(const uni_char *word, BOOL permanently)
{
	OP_STATUS status;
	const uni_char* word_end = word + uni_strlen(word);
	if (!CanSpellCheck(word, word_end) || m_removed_words.HasString(word))
		return OpStatus::OK;

	if (m_added_words.HasString(word))
	{
		BOOL removed_from_file = FALSE;
		if (permanently)
			RETURN_IF_ERROR(RemoveStringsFromOwnFile(word,TRUE,removed_from_file));

		m_added_words.RemoveString(word);
		OpStatus::Ignore(RemoveFromHunspell(word));
		return OpStatus::OK;
	}
	else
	{
		BOOL has_word = FALSE;
		RETURN_IF_ERROR(IsWordInDictionary(word, has_word));
		if (!has_word)
			return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_removed_words.AddString(word));
	OpStatus::Ignore(RemoveFromHunspell(word));

	if (!permanently)
		return OpStatus::OK;


	RETURN_IF_ERROR(CreateOwnFileIfNotExists());
	OpFile own_file;
	int file_len = 0;
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len, OPFILE_APPEND));
	UINT8 *buf = TempBuffer();
	int word_bytes = to_utf8(NULL, word, SPC_TEMP_BUFFER_BYTES + 1);
	if (word_bytes <= SPC_TEMP_BUFFER_BYTES)
	{
		to_utf8((char*)buf, word, SPC_TEMP_BUFFER_BYTES);
		buf[word_bytes-1] = '\n';
		status = own_file.Write(buf,(OpFileLength)word_bytes);
	}
	else
	{
		status = OpStatus::ERR;
	}
	own_file.Close();
	ReleaseTempBuffer();
	return OpStatus::IsError(status) ? SPC_ERROR(status) : OpStatus::OK;
}



OP_STATUS OpSpellChecker::LoadOwnFile()
{
	if ( !m_own_path )
		return OpStatus::OK;

	OP_ASSERT(m_hunspell);

	int file_len = 0;
	OpFile own_file;
	
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len));

	OpString8 line;
	OpString word;
	OpFileLength pos;
	char value;

	// Read the added words
	while ( !own_file.Eof() )
	{
		RETURN_IF_ERROR(own_file.GetFilePos(pos));
			RETURN_IF_ERROR(own_file.ReadBinByte(value));
		if ( value == '\0' )
			break;
		RETURN_IF_ERROR(own_file.SetFilePos(pos));
		RETURN_IF_ERROR(own_file.ReadLine(line));
		if ( line.IsEmpty() )
			continue;
		RETURN_IF_ERROR(word.SetFromUTF8(line.CStr()));
		RETURN_IF_ERROR(AddWord(word.CStr(), FALSE /*permanently*/));
	}

	// Read the removed words
	while ( !own_file.Eof() )
	{
		RETURN_IF_ERROR(own_file.ReadLine(line));
		if ( line.IsEmpty() )
			continue;
		RETURN_IF_ERROR(word.SetFromUTF8(line.CStr()));
		RETURN_IF_ERROR(RemoveWord(word.CStr(), FALSE /*permanently*/));
	}
	
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ContinueLoadingDictionary(double time_out, BOOL &finished, uni_char **error_str)
{
	OP_ASSERT(!"CONTINUE LOADING DICTIONARY");
	return OpStatus::OK;
}

BOOL OpSpellChecker::IsWordSeparator(uni_char ch)
{
	OP_ASSERT( m_hunspell != NULL );
//FIXME HUNSPELL: Add support for wordbreaks
	
	return g_internal_spellcheck->IsWordSeparator(ch) && m_word_chars.FindFirstOf(ch) == KNotFound;
}


/** Keeps track of word breaks within a word. Makes it easy to partition a word at the breaks in different ways */
class WordBreakState
{
	public:
	WordBreakState(const uni_char* s, const uni_char* s_end, uni_char** breaks, int num_breaks, BOOL split) : m_breaks(breaks), m_num_breaks(num_breaks)
		{
			SkipLeadingBreaks(s, s_end, &break_start, &word_start);
			SkipTrailingBreaks(word_start, s_end, &word_end, &break_end);

			if ( split )
			{
				SkipToNextBreak(word_start, break_end, &left_word_end, &left_break_end);
				SkipLeadingBreaks(left_word_end, word_end, &right_break_start, &right_word_start);
			}
			else
			{
				left_word_end = word_end;
				left_break_end = break_end;
				right_word_start = break_end;
				right_break_start = break_end;
			}
		}
	

	/** Position the state at the next word */
	BOOL NextWord()
		{
			if ( IsEmpty() )
				return FALSE;

			break_start = right_break_start;
			word_start = right_word_start;
			SkipToNextBreak(word_start, break_end, &left_word_end, &left_break_end);
			SkipLeadingBreaks(left_word_end, word_end, &right_break_start, &right_word_start);			

			return TRUE;
		}

	/** Expands the word to include the break and the word to the right of the break */
	BOOL ExpandWord()
		{
			if ( !IsAtJoiningBreak() )
				return FALSE;

			SkipToNextBreak(right_word_start, break_end, &left_word_end, &left_break_end);
			SkipLeadingBreaks(left_word_end, word_end, &right_break_start, &right_word_start);			
			return TRUE;
		}


	BOOL IsSingleWord()
		{
			return IsLastWord();
		}


	BOOL IsEmpty()
		{
			return word_start == word_end;
		}

	BOOL IsAtJoiningBreak()
		{
			return left_word_end == right_break_start && left_break_end == right_word_start && !IsLastWord();
		}

	BOOL IsAtDividingBreak()
		{
			return (left_word_end != right_break_start || left_break_end != right_word_start) && !IsLastWord();
		}

	BOOL IsLastWord()
		{
			return left_word_end == word_end;
		}

	BOOL GetWordNoBreaks(const uni_char** w_start, const uni_char** w_end)
		{
			*w_start = word_start;
			*w_end = left_word_end;
			return word_start != left_word_end;
		}

	BOOL GetWordLeadingBreak(const uni_char** w_start, const uni_char** w_end)
		{
			*w_start = break_start;
			*w_end = left_word_end;
			return break_start != word_start;
		}

	BOOL GetWordTrailingBreak(const uni_char** w_start, const uni_char** w_end)
		{
			*w_start = word_start;
			*w_end = left_break_end;
			return left_word_end != left_break_end;
		}


	BOOL GetWordLeadingAndTrailingBreaks(const uni_char**w_start, const uni_char** w_end)
		{
			*w_start = break_start;
			*w_end = left_break_end;
			return break_start != word_start && left_word_end != left_break_end;
		}

	private:

/**
 * Searches past leading breaks until a word character is found and records the start of the word and the last preceding break.
 *
 * @param str  The string to search in.
 * @param break_start The start of a preceding (ambiguous) break or to the start of the word.
 * @param word_start  The start of the word.
 */
	
	void SkipLeadingBreaks(const uni_char* str, const uni_char* str_end, const uni_char** break_start, const uni_char** word_start)
		{
			const uni_char* s = str;
			const uni_char* nearest_break = s;
			
			while ( s < str_end )
			{
				for (int i = 0; i < m_num_breaks; i++)
				{
					uni_char* b = m_breaks[i];
					int bl = adjust_break(b);
					
					if ( b[bl] == '$' ) continue; // break only valid at end of word.
					
					const uni_char *s_start = s;
					while ( s+bl < str_end && uni_strncmp(s, b, bl) == 0 )
						s+=bl;
					
					if ( s != s_start )
					{
						nearest_break = s - bl;
						i = 0; 
					}
				}
				if ( s < str_end )
				{
					if ( !is_ambiguous_break(*s) )
						break;
					
					nearest_break = s;
					s++;
				}
			}
			*word_start = s;
			*break_start = nearest_break;
		}
	

	void SkipToNextBreak(const uni_char* str, const uni_char* str_end, const uni_char** word_end, const uni_char** break_end)
		{
			const uni_char* s = str;
			const uni_char* nearest_break = NULL;
			BOOL skip_char;

			while ( s < str_end && !nearest_break )
			{
				skip_char = FALSE;
				for (int i = 0; i < m_num_breaks; i++)
				{
					uni_char* b = m_breaks[i];
					int bl = adjust_break(b);
					
					if ( s+bl <= str_end && uni_strncmp(s, b, bl) == 0 )
					{
						if ( b > m_breaks[i] || b[bl] == '$' )
						{
							// Break only valid at start or end of word.
							// Check for another identical break definition that is valid within words
							// If no such break exists, then skip past this break unless we are at the end of a word

							uni_char* b2;
							int bl2 = bl;
							int j;
							for (j = i+1; j < m_num_breaks && bl2 == bl; j++)
							{
								b2 = m_breaks[j];
								bl2 = adjust_break(b2);
								if ( bl2 == bl && b2==m_breaks[j] && b2[bl2]!='$' && uni_strncmp(b, b2, bl) == 0 )
									break;
							}
							if ( j < m_num_breaks && bl2 == bl )
							{
								// We found a break that is valid within words, end search...
								nearest_break = s + bl;
							}
							else if ( s+bl == str_end )
							{
								// At end of string.
								nearest_break = b[bl] == '$' ? s+bl : s;
							}
							else
							{
								// We might still be at the end of a word, check for the next word.
								const uni_char* break_start;
								const uni_char* word_start;
								SkipLeadingBreaks(s, str_end, &break_start, &word_start);
								if ( break_start != s )
								{
									if ( b[bl] == '$' )
										nearest_break = s + bl;
									else
										nearest_break = s;
								}
								else
								{
									// Make sure that we don't treat it as an ambigous break. The leading/trailing breaks are special in the sense that they are not supposed to be used within words.
									// i.e They are not really breaks, but close enough to be treated as such for the most part :)
									skip_char = TRUE;
								}
							}
						}
						else
						{
							nearest_break = s + bl;
						}
						break;
					}
				}
				if ( !nearest_break )
				{
					if ( !skip_char && is_ambiguous_break(*s) )
						nearest_break = s + 1;
					else
						s++;
				}
			}
			*word_end = s;
			*break_end = nearest_break ? nearest_break : s;
		}


	void SkipTrailingBreaks(const uni_char* str, const uni_char* str_end, const uni_char** word_end, const uni_char** break_end)
		{
			const uni_char *s = str_end;
			const uni_char *nearest_break = str_end;
			
			while ( s > str )
			{
				for (int i = 0; i < m_num_breaks; i++)
				{
					uni_char* b = m_breaks[i];
					int bl = adjust_break(b);
					if ( b > m_breaks[i] ) continue; // break only valid at start of word.
					
					const uni_char *s_start = s;
					do
					{
						s-=bl;
					}
					while ( s >= str && uni_strncmp(s, b, bl) == 0 );
					
					s+=bl;
					
					if ( s != s_start )
					{
						nearest_break = s + bl;
						i = 0; 
					}
				}
				if ( s > str )
				{
					if ( !is_ambiguous_break(*(s-1)) )
						break;
					
					nearest_break = s;
					s--;
				}
			}
			
			*word_end = s;
			*break_end = nearest_break;
		}
	
	
	const uni_char* break_start;
	const uni_char* word_start;
	const uni_char* word_end;
	const uni_char* break_end;
	
	const uni_char* left_word_end;
	const uni_char* left_break_end;
	const uni_char* right_break_start;
	const uni_char* right_word_start;

	uni_char** m_breaks;
	int m_num_breaks;
};


OP_STATUS OpSpellChecker::AddContextToSuggestions(OpSpellCheckerLookupState *lookup_state, const uni_char* left_start, const uni_char* left_end, const uni_char* right_start, const uni_char* right_end)
{
	int n = lookup_state->replacements.GetSize();
	int left_len = left_end-left_start;
	int right_len = right_end-right_start;
	if ( left_len == 0 && right_len == 0 )
		return OpStatus::OK;

	for ( int i = 0; i < n; i++ )
	{
		uni_char* str = (uni_char*) lookup_state->replacements.GetElement(i);
		int len = uni_strlen(str);
		str = (uni_char*) op_realloc(str, (left_len+len+right_len+1)*2);
		if ( str == NULL )
			return OpStatus::ERR_NO_MEMORY;
		op_memmove(str+left_len, str, len*2);
		op_memcpy(str, left_start, left_len*2);
		op_memcpy(str+left_len+len, right_start, right_len*2);
		str[left_len+len+right_len]=0;
		*(lookup_state->replacements.GetElementPtr(i))=(uni_char*)str;
	}

	return OpStatus::OK;
}

//FIXME: Are there correctly spelled words that have several breaks in a row maybe -' or something?
//FIXME: Add some way of handling breaks that are parts of breaks.
//FIXME: There might be other misspellings in the rest of the string - those misspellings will be part of the suggestion!
//FIXME: Need an API that can handle only parts of a "word" as misspellings.
OP_STATUS OpSpellChecker::CheckAmbiguousBreaks(OpSpellCheckerLookupState *lookup_state, double time_out, BOOL find_replacements, double replacement_timeout, const uni_char* str, const uni_char* str_end, BOOL split)
{
	const uni_char* word_start = str;
	const uni_char* word_end = str_end;

	// Trim the original string.
	WordBreakState wbs(word_start, word_end, m_breaks, m_num_breaks, split);

	lookup_state->correct = wbs.IsEmpty() ? YES : NO;
	while ( !wbs.IsEmpty() )
	{
		if ( wbs.IsSingleWord() )
		{
			// It is a single word, but it might have surrounding word breaks.
			// The breaks might be part of the word, so we have to try different combinations.

			wbs.GetWordNoBreaks(&word_start, &word_end);
			RETURN_IF_ERROR(CheckSpelling(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end));
			if ( lookup_state->correct == YES )
				return OpStatus::OK;
			
			if ( wbs.GetWordTrailingBreak(&word_start, &word_end) )
			{
				RETURN_IF_ERROR(CheckSpelling(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end));
				if ( lookup_state->correct == YES )
					return OpStatus::OK;
			}
			

			if ( wbs.GetWordLeadingBreak(&word_start, &word_end) )
			{
				RETURN_IF_ERROR(CheckSpelling(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end));
				if ( lookup_state->correct == YES )
					return OpStatus::OK;
			}
			
			if ( wbs.GetWordLeadingAndTrailingBreaks(&word_start, &word_end) )
			{
				RETURN_IF_ERROR(CheckSpelling(lookup_state, time_out, find_replacements, FALSE /*replacement_timeout*/, word_start, word_end));
			}

			if ( find_replacements )
			{
				// Find replacements on the word without breaks. Otherwise the spellchecker might suggest removing leading/trailing breaks which is often not what we want
				// E.g. removing quotation marks at the begining or end of a word or removing hyphens within a word is usually wrong!
				// FIXME: What if hunspell suggests adding a break? Maybe we need handle that as well to avoid double breaks in some cases?
                // FIXME: Add some way to check for replacements only to avoid the checks above when we already know the word is misspelled
				wbs.GetWordNoBreaks(&word_start, &word_end);
				RETURN_IF_ERROR(CheckSpelling(lookup_state, time_out, TRUE /*find_replacements*/, replacement_timeout, word_start, word_end));
				RETURN_IF_ERROR(AddContextToSuggestions(lookup_state, str, word_start, word_end, str_end));
			}

			return OpStatus::OK;
		}
		else if ( wbs.IsAtJoiningBreak() )
		{
			// A joining break. The word might exist in the dictionary as a single word containing the break.
			// However, most of the time the words exists as separate words as well, and often only as separate words.

#define SPC_MAX_BREAK_SPLITS 10
			
			const uni_char* left_boundaries[SPC_MAX_BREAK_SPLITS];
			const uni_char* right_boundaries[SPC_MAX_BREAK_SPLITS];
			int left_split_count = 0;
			int i;
			
				
			while ( !wbs.IsEmpty() )
			{
				// Check word by word from the left until we get to a misspelled one or we find a non-joining break.
				do {
					wbs.GetWordLeadingAndTrailingBreaks(&word_start, &word_end);
					RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end, FALSE /*split*/));
					left_boundaries[left_split_count%SPC_MAX_BREAK_SPLITS]=word_start;
					left_split_count++;
				} while ( lookup_state->correct == YES && wbs.IsAtJoiningBreak() && wbs.NextWord());
				
				
				if ( lookup_state->correct == NO )
				{
					// The word is either misspelled or it needs to be part of a larger compound word containing word breaks.

					const uni_char* misspelled_start = word_start;
					const uni_char* misspelled_end = word_end;

					// First, check if the word combined with words to the right of the break is OK.
					WordBreakState tmp_wbs = wbs;
					int right_split_count = left_split_count; 
					for (i = 0; i < SPC_MAX_BREAK_SPLITS && lookup_state->correct == NO && tmp_wbs.ExpandWord(); i++ )
					{
						tmp_wbs.GetWordLeadingAndTrailingBreaks(&word_start, &word_end);
						RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end, FALSE /*split*/));
						right_boundaries[right_split_count%SPC_MAX_BREAK_SPLITS] = word_end;
						right_split_count++;
					}
					
					if ( lookup_state->correct == NO )
					{
						// Combining to the right didn't help. Lets try to combine with words on both side of the misspelled word.
						
						for (/*Empty*/; i < SPC_MAX_BREAK_SPLITS; i++)
						{
							right_boundaries[right_split_count%SPC_MAX_BREAK_SPLITS] = word_end;
							right_split_count++;
						}
						
						int i_end = left_split_count < SPC_MAX_BREAK_SPLITS ? left_split_count : SPC_MAX_BREAK_SPLITS;
						for (i = 1; i <= i_end && lookup_state->correct == NO; i++)
						{
							word_start = left_boundaries[(left_split_count-i)%SPC_MAX_BREAK_SPLITS];
							word_end = right_boundaries[(left_split_count-i)%SPC_MAX_BREAK_SPLITS];
							RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end, FALSE /*split*/));
						}
					}
					
					
					if ( lookup_state->correct == NO )
					{
						if ( find_replacements )
						{
							RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, TRUE /*find_replacements*/, replacement_timeout, misspelled_start, misspelled_end, FALSE /*split*/));
							OP_ASSERT( SPC_MAX_BREAK_SPLITS > 4 );
							if ( lookup_state->replacements.GetSize() == 0 && left_split_count < 3 && right_boundaries[left_split_count+1] == right_boundaries[left_split_count+2]  )
							{
								// No suggestions and the word seems short try to get suggestions for the full word.
								// FIXME: Refine this a bit, maybe we should always do this for short words?
								misspelled_start = left_boundaries[0];
								misspelled_end = right_boundaries[0];
								RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, TRUE /*find_replacements*/, replacement_timeout, misspelled_start, misspelled_end, FALSE /*split*/));
							}
							RETURN_IF_ERROR(AddContextToSuggestions(lookup_state, str, misspelled_start, misspelled_end, str_end));
						}
						return OpStatus::OK;
					}

					// Fast forward past the end of the word and continue from there.
					// We need to do this to keep the left_boundaries up to date in case we run in to another misspelled word near by.
					const uni_char* split_pos = word_end;
					wbs.GetWordLeadingAndTrailingBreaks(&word_start, &word_end);
					while ( word_end < split_pos )
					{
						wbs.NextWord();
						wbs.GetWordLeadingAndTrailingBreaks(&word_start, &word_end);
						left_boundaries[left_split_count%SPC_MAX_BREAK_SPLITS]=word_start;
						left_split_count++;
					}
					
				}

				if ( wbs.IsAtDividingBreak() )
				{
					wbs.NextWord();
					break;
				}
				else
				{
					wbs.NextWord();
					continue;
				}
			}
		}
		else
		{
			// The break between the left and the right word is a leading or trailing break, or there are multiple breaks ==> Split and spellcheck in parts.
			OP_ASSERT( wbs.IsAtDividingBreak() );

			lookup_state->correct = YES;
			while ( lookup_state->correct == YES && wbs.IsAtDividingBreak() )
			{
				wbs.GetWordLeadingAndTrailingBreaks(&word_start, &word_end);
				RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, FALSE /*find_replacements*/, replacement_timeout, word_start, word_end, FALSE /*split*/));
				wbs.NextWord();
			}

			if ( lookup_state->correct == NO )
			{
				if ( find_replacements )
				{
					RETURN_IF_ERROR(CheckAmbiguousBreaks(lookup_state, time_out, TRUE /*find_replacements*/, replacement_timeout, word_start, word_end, FALSE /*split*/));
					RETURN_IF_ERROR(AddContextToSuggestions(lookup_state, str, word_start, word_end, str_end));
				}
				return OpStatus::OK;
			}
		}
	}



	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::CheckSpelling(OpSpellCheckerLookupState *lookup_state, double time_out, BOOL find_replacements, double replacement_timeout, const uni_char *override_word, const uni_char* override_word_end)
{
	OP_ASSERT( m_hunspell != NULL );

	const uni_char* current_word = override_word ? override_word : lookup_state->word_iterator->GetCurrentWord();
	OP_ASSERT( current_word != NULL );

	const uni_char* current_end =  override_word ? override_word_end : current_word + uni_strlen(current_word);

	char *encoding = m_hunspell->get_dic_encoding();
	OP_ASSERT( encoding != NULL );

	OpString8 word;

	lookup_state->in_progress = FALSE;
	lookup_state->correct = NO;

	if ( override_word == NULL )
		return CheckAmbiguousBreaks(lookup_state, time_out, find_replacements, replacement_timeout, current_word, current_end);

	RETURN_IF_LEAVE(SetToEncodingL(&word, encoding, current_word, current_end-current_word));  //FIXME: Use char converter.

	if (!CanSpellCheck(current_word, current_end))
	{
		lookup_state->correct = YES;
		return OpStatus::OK;
	}

	if (m_added_words.HasString(current_word, current_end))
	{
		lookup_state->correct = YES;
		return OpStatus::OK;
	}

	if (m_removed_words.HasString(current_word, current_end))
	{
		lookup_state->correct = NO;
	}
	else
	{
		//FIXME: Probably not needed when searching for replacements
		lookup_state->correct = m_hunspell->spell(word.CStr()) == 0 ? NO : YES;
	}

	if ( find_replacements )
	{
		char** slst;
		int num, idx;
		num = m_hunspell->suggest(&slst, word.CStr());
		for ( int i = 0; i < num && lookup_state->replacements.GetSize() < SPC_MAX_REPLACEMENTS; i++ )
		{
			OpString16 suggestion;
			RETURN_IF_ERROR(SetFromEncoding(&suggestion, encoding, slst[i], op_strlen(slst[i]))); //FIXME: Use char converter.
			if ( m_removed_words.HasString(suggestion.CStr(), suggestion.CStr() + suggestion.Length()) )
				continue; // Hunspell only removes a word if it is in the dictionary file, i.e. it ignores affixed forms, so we might need to manually filter out some words.
			uni_char* s = uni_strdup(suggestion.CStr());
			if ( s == NULL )
				return OpStatus::ERR_NO_MEMORY;
			idx = lookup_state->replacements.AddElement(s);
			if (idx < 0)
			{
				op_free(s);
				m_hunspell->free_list(&slst, num);
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			}

		}
		m_hunspell->free_list(&slst, num);
	}

	return OpStatus::OK; 
}


BOOL OpSpellChecker::HasDictionary()
{
	return m_hunspell != NULL;
}

OpSpellCheckerLookupState::OpSpellCheckerLookupState() : replacements(SPC_MAX_REPLACEMENTS+1)
{
	correct = MAYBE;
	in_progress = FALSE;
	word_iterator = NULL;
}
	
void OpSpellCheckerLookupState::Reset()
{ 
	int num = replacements.GetSize();
	for (int i = 0; i < num; i++)
	{
		op_free(replacements.GetElement(i));
	}

	replacements.Reset();
	correct = MAYBE;
	in_progress = FALSE;
}


OP_STATUS OpSpellChecker::ConvertStringToUTF16(UINT8 *str, uni_char *buf, int buf_bytes)
{
	int was_read = 0;
	int str_len = op_strlen((const char*)str)+1;
	int was_written = m_input_converter->Convert(str, str_len, buf, buf_bytes, &was_read);
	if (!was_written || was_written&1 || was_read != str_len)
		return OpStatus::ERR;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ConvertStringFromUTF16(const uni_char *word, UINT8 *buf, int buf_bytes)
{
	int was_read = 0;
	int str_len = (uni_strlen(word)+1)*2;
	int was_written = m_output_converter->Convert(word, str_len, buf, buf_bytes, &was_read);
	if (!was_written || was_read != str_len)
		return OpStatus::ERR;
	return OpStatus::OK;
}

char *hunspell_null()
{
	OP_ASSERT(FALSE);
	return NULL;
}

#define HFNULL (HUNSPELL_FILE*)hunspell_null()
#define HNULL hunspell_null()

HUNSPELL_FILE* hunspell_fopen(const char *path, const char *mode)
{
	OpFile* f;
	OpString p;

	
	OP_ASSERT(*mode == 'r');

	RETURN_VALUE_IF_ERROR(p.SetFromUTF8(path), HFNULL);

	f = OP_NEW(OpFile,());
	if ( f == NULL )
		return HFNULL;

	if ( OpStatus::IsError(f->Construct(p.CStr())) )
	{
		OP_DELETE(f);
		return HFNULL;
	}
	
	if ( OpStatus::IsError(f->Open(OPFILE_READ)) )
	{
		OP_DELETE(f);
		return HFNULL;
	}

	return (HUNSPELL_FILE*)f;
}

int hunspell_fclose(HUNSPELL_FILE* fp)
{
	OP_ASSERT(fp);

	OP_DELETE((OpFile*)fp);
	return 0;
}

size_t hunspell_fread(void *ptr, size_t size, size_t nmemb, HUNSPELL_FILE *stream)
{
	OpFile* f = (OpFile*)stream;

	OpFileLength bytes_read = 0;

	OP_ASSERT(f);

	if ( f->Eof() )
		return 0;

	f->Read(ptr, size*nmemb, &bytes_read);

	return (size_t) bytes_read/size;
}

char* hunspell_fgets(char *s, int size, HUNSPELL_FILE *stream)
{
	OpFile* f = (OpFile*)stream;

	OP_ASSERT(f);
	OP_ASSERT(size > 1);

	if ( f->Eof() )
	{
		s[0]='\0';
		return NULL;
	}

	OpString8 line;
	RETURN_VALUE_IF_ERROR(f->ReadLine(line), HNULL);

	OP_ASSERT( line.Length() < size - 1 );

	size = line.Length();
	
	op_memcpy(s, line.CStr(), size);
	s[size]='\n';
	s[size+1]='\0';

	return s;
}

char* hunspell_strncpy(char *dest, const char *src, size_t n)
{
	OP_ASSERT( dest != NULL && src != NULL );

	size_t i;
	for (i = 0 ; i < n && src[i] != '\0' ; i++)
		dest[i] = src[i];

	for ( ; i < n ; i++)
		dest[i] = '\0';
	
	return dest;
}

#endif //!SPC_USE_HUNSPELL_ENGINE




#endif // INTERNAL_SPELLCHECK_SUPPORT
