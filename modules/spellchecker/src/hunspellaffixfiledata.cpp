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

/* ===============================HunspellAffixFileData=============================== */

HunspellAffixFileData::HunspellAffixFileData(UINT8 *affix_data, unsigned int affix_len, OpSpellChecker *spell_checker)
{
	m_affix_data = affix_data;
	m_data_len = affix_len;
	m_lines = NULL;
	m_line_count = 0;
	m_last_line_copy = NULL;
	m_spell_checker = spell_checker;
	m_flag_encoding = FLAG_UNKNOWN_ENC;
	m_encoding = UNKNOWN_ENC;
	m_8bit_enc = FALSE;
	m_flag_mapping.data = NULL;
	m_affix_char_mapping.data = NULL;
	op_memset(m_flag_type_counters,0,sizeof(*m_flag_type_counters)*AFFIX_TYPE_COUNT);
	op_memset(m_flag_type_bits,0,sizeof(*m_flag_type_bits)*AFFIX_TYPE_COUNT);
	m_global_id_bits = 0;
}

HunspellAffixFileData::~HunspellAffixFileData() 
{ 
	int i;
	for (i=0;i<m_flag_pointers.GetSize();i++)
		OP_DELETE(m_flag_pointers.GetElement(i));
	if (GetFlagEncoding() == FLAG_UTF8_ENC)
		OP_DELETE(m_flag_mapping.utf);
	else
		OP_DELETEA(m_flag_mapping.linear);
	OP_DELETEA(m_lines);
	OP_DELETE(m_last_line_copy);
	OP_DELETEA(m_affix_data);
	if (m_8bit_enc)
		OP_DELETEA(m_affix_char_mapping.linear);
	else
		OP_DELETE(m_affix_char_mapping.utf);
}

void HunspellAffixFileData::ClearFileData()
{
	OP_DELETEA(m_affix_data);
	m_affix_data = NULL;
	OP_DELETEA(m_lines);
	m_lines = NULL;
	m_line_count = 0;
	OP_DELETEA(m_last_line_copy);
	m_last_line_copy = NULL;
}

void HunspellAffixFileData::SetTypeIdBits()
{
	int i;
	for (i=0;i<AFFIX_TYPE_COUNT;i++)
		m_flag_type_bits[i] = bits_to_represent(m_flag_type_counters[i]);
	m_global_id_bits = bits_to_represent(m_flag_pointers.GetSize());
}

OP_STATUS HunspellAffixFileData::SetFlagEncoding(FlagEncoding encoding)
{
	if (m_flag_encoding == encoding)
		return OpStatus::OK;
	if (m_flag_encoding == FLAG_UTF8_ENC)
		OP_DELETE(m_flag_mapping.utf);
	else
		OP_DELETEA(m_flag_mapping.linear);
	if (encoding == FLAG_UNKNOWN_ENC)
		return OpStatus::OK; //???
	if (encoding == FLAG_UTF8_ENC)
		m_flag_mapping.utf = UTFMapping<UINT16>::Create();
	else
	{
		int sz = encoding == FLAG_ASCII_ENC ? 256 : 65536;
		m_flag_mapping.linear = OP_NEWA(UINT16, sz);
		if (m_flag_mapping.linear)
			op_memset(m_flag_mapping.linear,0,sz*sizeof(UINT16));
	}
	if (!m_flag_mapping.data)
	{
		m_flag_encoding = FLAG_UNKNOWN_ENC;
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	m_flag_encoding = encoding;
	return OpStatus::OK;
}

OP_STATUS HunspellAffixFileData::SetDictionaryEncoding(DictionaryEncoding encoding)
{
	if (m_encoding == encoding)
		return OpStatus::OK;
	if (m_encoding == UTF8_ENC)
		OP_DELETE(m_affix_char_mapping.utf);
	else
		OP_DELETEA(m_affix_char_mapping.linear);
	if (encoding == UNKNOWN_ENC)
		return OpStatus::OK; //???
	if (encoding == UTF8_ENC)
		m_affix_char_mapping.utf = UTFMapping<UINT16*>::Create();
	else
	{
		m_affix_char_mapping.linear = OP_NEWA(UINT16*, 256);
		if (m_affix_char_mapping.linear)
			op_memset(m_affix_char_mapping.linear,0,256*sizeof(UINT16*));
	}
	m_encoding = encoding;
	m_8bit_enc = IS_8BIT_ENCODING(encoding);
	if (!m_affix_char_mapping.data)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	return OpStatus::OK;
}

OP_STATUS HunspellAffixFileData::Tokenize()
{
	unsigned int i;
	if (m_lines || !m_affix_data || !m_data_len)
		return SPC_ERROR(OpStatus::ERR);
	
	unsigned int alloc_count;
	for (alloc_count=1,i=0;i<m_data_len;i++)
		if (is_ascii_new_line(m_affix_data[i]))
			alloc_count++;
	m_lines = OP_NEWA(HunspellAffixFileLine, alloc_count);
	if (!m_lines)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	
	m_line_count = 0;
	UINT8 *pos = m_affix_data;
	if (m_data_len >= 3 && pos[0] == 0xEF && pos[1] == 0xBB && pos[2] == 0xBF) // ignore Byte Order Mark
		pos += 3;
	UINT8 *line_start = NULL;
	UINT8 *end = m_affix_data + m_data_len;
	int org_lines = 0;
	for (;;)
	{
		while (pos < end && *pos && is_ascii_new_line(*pos))
		{
			pos++;
			org_lines++;
		}
		if (pos >= end || !*pos)
			break;
		line_start = pos;
		while (pos < end && *pos && !is_ascii_new_line(*pos))
			pos++;
		if (pos >= end) // file doesn't end with '\0', so let's make a copy of the last line;
		{
			int str_len = (int)(pos-line_start);
			m_last_line_copy = OP_NEWA(UINT8, str_len+1);
			if (!m_last_line_copy)
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			op_memcpy(m_last_line_copy,line_start,str_len);
			m_last_line_copy[str_len] = '\0';
			line_start = m_last_line_copy;
		}
		else if (is_ascii_new_line(*pos))
			*pos = '\0';
		else // *pos == '\0';
			end = pos; // to break loop next interation...
		HunspellAffixFileLine *line = &m_lines[m_line_count];
		RETURN_IF_ERROR(line->Tokenize(line_start,m_spell_checker));
		if (line->Type() != HS_INVALID_LINE)
		{
			line->SetLineIndex(m_line_count++);
			line->SetOriginalLineNr(org_lines);
		}
		org_lines++;
		pos++;
	}
	return OpStatus::OK;
}

FlagClass *HunspellAffixFileData::GetFlagClass(int flag_index)
{
	if (!m_flag_pointers.GetSize())
		return NULL;
	if (GetFlagEncoding() == FLAG_UTF8_ENC)
	{
		UINT16 res = 0;
		if (!m_flag_mapping.utf->GetVal(flag_index,res))
			return NULL;
		return m_flag_pointers.GetElement(res);
	}
	OP_ASSERT(flag_index < 65536);
	return m_flag_pointers.GetElement(m_flag_mapping.linear[flag_index]);
}

OP_STATUS HunspellAffixFileData::AddFlagClass(FlagClass *flag_class, HunspellAffixFileParseState *state)
{
	int i, index;
	OP_STATUS status;
	FlagClass *old_flag = GetFlagClass(flag_class->GetFlagIndex());
	if (old_flag)
	{
		if (flag_class->GetType() != AFFIX_TYPE_AFFIX || old_flag->GetType() != AFFIX_TYPE_AFFIX || !state->GetCurrentLine())
		{
			OP_DELETE(flag_class);
			return SPC_ERROR(OpStatus::ERR);
		}
		//The same flag is created twice! Error in dictionary, but let's go on and replace the flag in order to support the en_GB dictionary which has both a PFX and and a SFX named 'O'
		flag_class->SetFlagTypeId(old_flag->GetFlagTypeId());
		index = old_flag->GetFlagGlobalId();
		flag_class->SetFlagGlobalId(index);
		*m_flag_pointers.GetElementPtr(index) = flag_class;
		HunspellAffixFileLine *current = state->GetCurrentLine();
		for (i=0;;i++)
		{
			HunspellAffixFileLine *line = state->GetLineInPass(i);
			if (!line || line == current)
			{
				OP_ASSERT(FALSE);
				return SPC_ERROR(OpStatus::ERR);
			}
			if (line->GetParserData() == (void*)old_flag)
			{
				line->SetParserData(NULL);
				break;
			}
		}
		OP_DELETE(old_flag);
		return OpStatus::OK;
	}
	if (!m_flag_pointers.GetSize())
	{
		if (m_flag_pointers.AddElement(NULL) < 0)
		{
			OP_DELETE(flag_class);
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		}
	}
	index = m_flag_pointers.AddElement(flag_class);
	if (index < 0)
	{
		OP_DELETE(flag_class);
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	
	int id = m_flag_type_counters[flag_class->GetType()]++;
	flag_class->SetFlagTypeId(id);
	flag_class->SetFlagGlobalId(index);

	if (GetFlagEncoding() == FLAG_UTF8_ENC)
	{
		status = m_flag_mapping.utf->SetVal(flag_class->GetFlagIndex(),(UINT16)index);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(flag_class);
			return SPC_ERROR(status);
		}
		return OpStatus::OK;
	}
	
	m_flag_mapping.linear[flag_class->GetFlagIndex()] = index;
	return OpStatus::OK;
}

UINT16* HunspellAffixFileData::GetCharIndexPtrForAffix(int ch, OpSpellCheckerAffix *affix)
{
	UINT16 *ptr = NULL;
	if (!m_8bit_enc)
	{
		BOOL exists = m_affix_char_mapping.utf->GetVal(ch,ptr);
		if (!exists || !ptr)
		{
			if ((ptr = (UINT16*)m_allocator.Allocate16(GetFlagTypeCount(AFFIX_TYPE_AFFIX)*sizeof(UINT16))) == NULL)
				return NULL;
			if (OpStatus::IsError(m_affix_char_mapping.utf->SetVal(ch,ptr)))
				return NULL;
		}
	}
	else
	{
		OP_ASSERT(ch < 256);
		ptr = m_affix_char_mapping.linear[ch];
		if (!ptr)
		{
			if ((ptr = (UINT16*)m_allocator.Allocate16(GetFlagTypeCount(AFFIX_TYPE_AFFIX)*sizeof(UINT16))) == NULL)
				return NULL;
			m_affix_char_mapping.linear[ch] = ptr;
		}
	}
	return ptr + affix->GetFlagTypeId();
}

UINT16 HunspellAffixFileData::GetCharIndexForAffix(int ch, OpSpellCheckerAffix *affix)
{
	UINT16 *ptr = NULL;
	if (!m_8bit_enc)
	{
		BOOL exists = m_affix_char_mapping.utf->GetVal(ch,ptr);
		if (!exists || !ptr)
			return 0;
	}
	else if ((ptr = m_affix_char_mapping.linear[ch]) == NULL)
		return 0;
	
	return ptr[affix->GetFlagTypeId()];
}

void HunspellAffixFileData::ConvertStrToAffixChars(UINT32 *str, int str_len, UINT32 *&result, OpSpellCheckerAffix *affix)
{
	int i;
	int id = affix->GetFlagTypeId();
	int max_cond_len = affix->GetMaxConditionLength();
	int convert_count = MIN(max_cond_len,str_len);
	int str_idx = 0, to_add = 1;
	UINT16 *ptr = NULL;
	if (affix->IsSuffix())
	{
		str_idx = str_len-1;
		to_add = -1;
	}
	if (!m_8bit_enc)
	{
		BOOL exists;
		for (i=0;i<convert_count;i++)
		{
			exists = m_affix_char_mapping.utf->GetVal((int)str[str_idx],ptr);
			result[i] = exists && ptr ? (UINT32)ptr[id] : 0;
			str_idx += to_add;
		}
	}
	else
	{
		for (i=0;i<convert_count;i++)
		{
			ptr = m_affix_char_mapping.linear[str[str_idx]];
			result[i] = ptr ? (UINT32)ptr[id] : 0;
			str_idx += to_add;
		}
	}
	for (i=convert_count;i<max_cond_len;i++)
		result[i] = 0;
}

OP_STATUS HunspellAffixFileData::ConvertAliasIndexes()
{
	int i,j;
	for (i=0;i<m_aliases.GetSize();i++)
	{
		FlagAlias *alias = m_aliases.GetElementPtr(i);
		int dst = 0;
		for (j=0;j<alias->GetFlagCount();j++)
		{
			FlagClass *flag_class = GetFlagClass(alias->GetFlags()[j]);
			if (flag_class)
				alias->GetFlags()[dst++] = flag_class->GetFlagGlobalId();
		}
		alias->SetFlags(alias->GetFlags(),dst);
	}
	return OpStatus::OK;
}

OP_STATUS HunspellAffixFileData::PostProcessCircumfixes(HunspellAffixFileParseState *state)
{
	int i,j, flag_index;
	FlagClass *flag_class;
	for (i=0;i<m_circumfixes.GetSize();i++)
	{
		flag_index = m_circumfixes.GetElement(i);
		flag_class = GetFlagClass(flag_index);
		OP_ASSERT(state->GetPass() == 0 || flag_class);
		if (state->GetPass() == 0 && !flag_class) // just add circumfix flag classes
		{
			flag_class = OP_NEW(FlagClass, (AFFIX_TYPE_CIRCUMFIX, flag_index));
			if (!flag_class)
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			RETURN_IF_ERROR(AddFlagClass(flag_class, state));
		}
		else if (state->GetPass() == 1 && flag_class->GetType() == AFFIX_TYPE_AFFIX) // set circumfix-flags on affix rules
		{
			OpSpellCheckerAffix *affix = (OpSpellCheckerAffix*)flag_class;
			for (j=0;j<affix->GetRuleCount();j++)
			{
				OpSpellCheckerAffixRule *rule = affix->GetRulePtr(j);
				if (rule->GetFlags())
					*rule->GetFlags() |= 1<<AFFIX_TYPE_CIRCUMFIX;
				else
				{
					UINT16 *new_flags = (UINT16*)m_allocator.Allocate16(sizeof(UINT16));
					if (!new_flags)
						return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
					*new_flags = 1<<AFFIX_TYPE_CIRCUMFIX;
					rule->SetFlags(new_flags);
				}
			}
		}
	}
	return OpStatus::OK;
}

/* ===============================HunspellAffixFileLine=============================== */

HunspellAffixFileLine::HunspellAffixFileLine() 
{
	m_words = m_tmp_words;
	m_word_count = 0;
	m_type = HS_INVALID_LINE;
	m_line_index = -1;
	m_org_line_nr = -1;
	m_parser_data = NULL;
}
HunspellAffixFileLine::~HunspellAffixFileLine()
{
	ResetData();
}

void HunspellAffixFileLine::ResetData()
{
	if (m_words != m_tmp_words)
		OP_DELETEA(m_words);
	m_words = m_tmp_words;
	m_word_count = 0;
}

OP_STATUS HunspellAffixFileLine::Tokenize(UINT8 *line_data, OpSpellChecker *spell_checker)
{
	UINT8 *pos = line_data;
	ResetData();
	m_type = HS_INVALID_LINE;
	UINT8 *word_start;
	BOOL end_of_line = FALSE;
	int array_size = AFFIX_LINE_STACK_WORDS;
	while (!end_of_line)
	{
		while (*pos && is_word_separator(*pos))
			pos++;
		if (!*pos)
			break;
		word_start = pos;
		while (*pos && !is_word_separator(*pos))
			pos++;
		if (!*pos)
			end_of_line = TRUE;
		*pos = '\0';
		if (m_type == HS_INVALID_LINE)
		{
			m_type = spell_checker->GetAffixTypeFromString(word_start);
			if (m_type == HS_INVALID_LINE)
				break;
		}
		else
		{
			if (m_word_count == array_size)
			{
				array_size *= 2;
				UINT8 **new_words = OP_NEWA(UINT8*, array_size);
				if (!new_words)
					return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
				op_memcpy(new_words,m_words,m_word_count*sizeof(UINT8*));
				if (m_words != m_tmp_words)
					OP_DELETEA(m_words);
				m_words = new_words;
			}
			m_words[m_word_count++] = word_start;
		}
		pos++;
	}
	return OpStatus::OK;
}

#endif // !SPC_USE_HUNSPELL_ENGINE

#endif // INTERNAL_SPELLCHECK_SUPPORT
