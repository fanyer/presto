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

/* ===============================HunspellDictionaryFileData=============================== */

HunspellDictionaryFileData::HunspellDictionaryFileData(DictionaryChunks *dic_chunks, OpSpellChecker *spell_checker, HunspellAffixFileData *affix_fd)
: m_tmp_data(spell_checker->GetDictionaryEncoding(), spell_checker->GetHasCompoundRules(), affix_fd)
{
	m_spell_checker = spell_checker;
	m_dic_chunks = dic_chunks;
}

/* ===============================HunspellDictionaryFileLine=============================== */

HunspellDictionaryFileLine::HunspellDictionaryFileLine(UINT8 *prev_word, UINT8 *word, UINT8 *flags)
{
	m_word_ofs = (unsigned int)(word-prev_word);
#ifdef SIXTY_FOUR_BIT
	m_flags = flags;
#else
	m_flags = flags ? (unsigned int)(flags-word) : 0;
#endif //SIXTY_FOUR_BIT
	m_is_pointer = 0; 
}

UINT8 *HunspellDictionaryFileLine::GetFlags(UINT8 *word)
{
#ifdef SIXTY_FOUR_BIT
	return m_flags;
#else
	if (m_is_pointer)
	{
		unsigned int ptr = m_flags << 16;
		UINT8 *low_bits = word + (m_flags>>16);
		if (low_bits == word) // distance between word and low bits of flag address (the previous location for flags) was > 31, so wee must find it manually
		{
			low_bits += op_strlen((const char*)word);
			while (!*low_bits)
				low_bits++;
		}
		ptr |= (unsigned int)(low_bits[0]) << 8 | low_bits[1];
		return (UINT8*)ptr;
	}
	return m_flags ? word + m_flags : NULL;
#endif //SIXTY_FOUR_BIT
}

void HunspellDictionaryFileLine::SetNewFlagsPtr(UINT8 *word, UINT8 *flags)
{
	if (!flags)
	{
		m_flags = 0;
		m_is_pointer = 0;
		return;
	}
	OP_ASSERT(!m_is_pointer && m_flags);
	m_is_pointer = 1;
#ifdef SIXTY_FOUR_BIT
	m_flags = flags;
#else
	word[m_flags] = (UINT8)((((UINT32)flags) & 0xFF00) >> 8); // store lower bits of address at old flags location
	word[m_flags+1] = (UINT8)((UINT32)flags & 0xFF); // two operations for avoiding non-aligned memory accesses
	m_flags = ((unsigned int)flags)>>16 | (m_flags & (-1)<<5 ? 0 : m_flags<<16); // store higher bits + offset to lower bits if there are space
#endif //SIXTY_FOUR_BIT
}

/* ===============================HunspellDictionaryParseState=============================== */

void HunspellDictionaryParseState::NextPass()
{
	m_pass++;
	m_current_chunk = m_dic_file_data->GetDictionaryChunks()->GetChunkAt(0);
	m_word = m_current_chunk->FileData();
	m_current = -1;
	m_current_in_chunk_idx = -1;
	m_current_chunk_idx = 0;
	NextLine();
}

BOOL HunspellDictionaryParseState::NextLine() 
{
	while (++m_current_in_chunk_idx >= m_current_chunk->GetLinesInChunk())
	{
		DictionaryChunks *chunks = m_dic_file_data->GetDictionaryChunks();
		if (m_pass == m_pass_count-1)
			chunks->GetChunkAt(m_current_chunk_idx)->FreeChunkData();
		if (++m_current_chunk_idx >= chunks->GetChunkCount())
		{
			m_word = NULL;
			m_current_line = NULL;
			m_current++;
			return FALSE;
		}
		m_current_in_chunk_idx = -1;
		m_current_chunk = chunks->GetChunkAt(m_current_chunk_idx);
		m_word = m_current_chunk->FileData();
	}
	if (m_current_in_chunk_idx == m_current_chunk->GetLinesInChunk()-1 && m_current_chunk->PossibleLastWord())
		m_word = m_current_chunk->PossibleLastWord();
	m_current_line = m_current_chunk->GetFirstLineInChunk() + m_current_in_chunk_idx;
	m_word = m_current_line->GetWord(m_word);
	m_current++;
	return TRUE;
}

/* ===============================SingleDictionaryChunk=============================== */

OP_STATUS SingleDictionaryChunk::Tokenize(ExpandingBuffer<HunspellDictionaryFileLine> *line_dst)
{
#define NOT_END(x) ((x) < end && *(x))
#define WORD_FLAG_SEPARATOR(x) (*(x) == '/' && (x)[-1] != '\\')
#define IS_ASCII_NEW_LINE(x) ((x) == '\n' || (x) == '\r')
#define IS_WORD_SEPARATOR(x) ((x) == ' ' || (x) == '\t')
	UINT8 *str = m_dic_data;
	UINT8 *end = m_dic_data + m_dic_len;
	if (m_dic_len <= 0)
		return line_dst->GetSize() ? OpStatus::OK : SPC_ERROR(OpStatus::ERR);
	
	int org_lines = line_dst->GetSize();
	UINT8 *prev_word = str;
	UINT8 *word1 = NULL, *word2 = NULL, *word1_end = NULL, *word2_end = NULL;
	BOOL has_backslash, is_first_line = !line_dst->GetSize();

	if (is_first_line && m_dic_len >= 3 && str[0] == 0xEF && str[1] == 0xBB && str[2] == 0xBF) // ignore Byte Order Mark
		str += 3;
	
	while (NOT_END(str))
	{
		has_backslash = FALSE;
		if (*str == '\t' || *str == '#' || *str == '/')
		{
			// Tab or / before the first non-whitespace character seems to make hunspell treat the rest of the line as a comment.
			while (NOT_END(str) && !IS_ASCII_NEW_LINE(*str))
				str++;
			continue;
		}
		if (*str == ' ' || IS_ASCII_NEW_LINE(*str))
		{
			//NOTE: This is different from Hunspell. Hunspell does not seem to ignore leading spaces. Odd...
			str++;
			continue;
		}
		word1 = str;
		word2 = NULL;
		while (NOT_END(str) && !(WORD_FLAG_SEPARATOR(str)) && !IS_ASCII_NEW_LINE(*str))
		{
			has_backslash = has_backslash || *str == '\\';
			str++;
		}
		word1_end = str;
		if ((SPC_UINT_PTR)(word1_end-word1) > SPC_MAX_DIC_FILE_WORD_LEN)
			return SPC_ERROR(OpStatus::ERR); // not exectly correct when dictionary is UTF-8...
		BOOL separator = NOT_END(str) && WORD_FLAG_SEPARATOR(str);
		if (NOT_END(str))
			*(str++) = '\0';
		if (separator && NOT_END(str))
		{
			word2 = str;
			while (NOT_END(str) && !IS_ASCII_NEW_LINE(*str))
				str++;
			word2_end = str;
			if (NOT_END(str))
				*(str++) = '\0';
		}
		if (!NOT_END(str))
		{
			if (word2)
			{
				UINT8 *old_word1 = word1;
				RETURN_IF_ERROR(CopyLastDictionaryWord(word1,word2_end));
				SPC_UINT_PTR diff = (SPC_UINT_PTR)word1 - (SPC_UINT_PTR)old_word1;
				word1_end = (UINT8*)((SPC_UINT_PTR)word1_end + diff);
				word2 = (UINT8*)((SPC_UINT_PTR)word2 + diff);
			}
			else
				RETURN_IF_ERROR(CopyLastDictionaryWord(word1,word1_end));
			prev_word = word1;
		}
		if (IS_WORD_SEPARATOR(*word1) || IS_WORD_SEPARATOR(word1_end[-1]))
			word1 = ws_strip(word1,word1_end);
		if (*word1 && has_backslash)
			word1 = bs_strip(word1);
		if (word2 && (IS_WORD_SEPARATOR(*word2) || IS_WORD_SEPARATOR(word2_end[-1])))
			word2 = ws_strip(word2,word2_end);
		if (!*word1)
			continue;
		if (is_first_line)
		{
			int dummy = 0;
			int parsed_line_count = pos_num_to_int(word1,100000000,dummy);
			if (parsed_line_count <= 0) // perhaps this file doesn't contain any line count?
			{
				if (line_dst->AddElement(HunspellDictionaryFileLine(prev_word, word1, word2 && *word2 ? word2 : NULL)) < 0)
					return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			}
			is_first_line = FALSE;
		}
		else
		{
			if (line_dst->AddElement(HunspellDictionaryFileLine(prev_word, word1, word2 && *word2 ? word2 : NULL)) < 0)
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		}
		prev_word = word1;
	}
	m_line_count = line_dst->GetSize() - org_lines;
	return line_dst->GetSize() ? OpStatus::OK : SPC_ERROR(OpStatus::ERR);
}

OP_STATUS SingleDictionaryChunk::CopyLastDictionaryWord(UINT8 *&word_start, UINT8 *&word_end)
{
	OP_ASSERT(!m_last_word);
	int len = (int)(word_end-word_start);
	m_last_word = OP_NEWA(UINT8, len+1);
	if (!m_last_word)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	op_memcpy(m_last_word,word_start,len);
	m_last_word[len] = '\0';
	word_start = m_last_word;
	word_end = m_last_word + len;
	return OpStatus::OK;
}

DictionaryChunks::DictionaryChunks(uni_char *dictionary_path, uni_char *own_path)
{
	m_dictionary_path = dictionary_path;
	m_own_path = own_path;
	m_dic_pos = 0;
	m_insert_data = NULL;
	m_remove_data = NULL;
	m_remove_data_len = 0;
	m_insert_data_len = 0;
	m_approx_insert_count = 0;
	m_own_data_finished = FALSE;
}

DictionaryChunks::~DictionaryChunks()
{ 
	int i;
	for (i=0;i<m_chunks.GetSize();i++)
		m_chunks.GetElementPtr(i)->FreeChunkData();
	OP_DELETEA(m_insert_data);
	OP_DELETEA(m_remove_data);
}

OP_STATUS DictionaryChunks::ReadOwnData()
{
	OP_ASSERT(!m_insert_data);
	OP_ASSERT(!m_remove_data);

	if (!m_own_path)
		return OpStatus::OK;
	int file_len = 0;
	OpFile own_file;
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len));
	if (!file_len)
	{
		own_file.Close();
		return OpStatus::OK;
	}
	UINT8 *data = OP_NEWA(UINT8, file_len+1);
	if (!data)
	{
		own_file.Close();
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	data[file_len] = '\0';
	OpFileLength was_read = 0;
	OP_STATUS status = own_file.Read(data, file_len, &was_read);
	own_file.Close();
	// FIXME: Read the file in a loop instead of this. Check the PI API documentation what is guaranteed.
	if (OpStatus::IsError(status) || (OpFileLength)file_len != was_read)
	{
		OP_DELETEA(data);
		return SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR);
	}
	int i;
	for (i=0;data[i];i++)
	{
		if (is_ascii_new_line(data[i]))
			m_approx_insert_count++;
	}

	if (i) // Did we have a section of data before the first NULL char?
	{
		m_approx_insert_count++;
		m_insert_data = data;
		m_insert_data_len = i;
	}

	i++; // Skip the NULL

	if (i < file_len && data[i])
	{
		int remove_data_len = file_len - i;
		m_remove_data = OP_NEWA(UINT8, remove_data_len+1);
		if (m_remove_data)
		{
			m_remove_data[remove_data_len] = '\0';
			m_remove_data_len = remove_data_len;
			op_memcpy(m_remove_data, data+i, remove_data_len);
		}
		else
			status = SPC_ERROR(OpStatus::ERR);
	}

	if (!m_insert_data)
		OP_DELETEA(data);
	else
		OP_ASSERT(data == m_insert_data);

	return status;
}

OP_STATUS DictionaryChunks::ReadDictionaryChunks(double time_out, BOOL &finished)
{
	int i;
	finished = FALSE;
	OpFile dic_file;
	OP_STATUS status = OpStatus::OK;
	int file_len = 0;
	if (!m_own_data_finished)
	{
		RETURN_IF_ERROR(ReadOwnData());
		m_own_data_finished = TRUE;
		if (time_out != 0.0 && g_op_time_info->GetWallClockMS() > time_out)
			return OpStatus::OK;
	}
	RETURN_IF_ERROR(SCOpenFile(m_dictionary_path, dic_file, file_len));
	BOOL is_last = FALSE;
	do
	{
		if (m_dic_pos)
		{
			status = dic_file.SetFilePos(m_dic_pos);
			if (OpStatus::IsError(status))
			{
				status = SPC_ERROR(status);
				break;
			}
		}
		int to_read = MIN(file_len-m_dic_pos, SPC_DICTIONARY_CHUNK_SIZE);
		is_last = to_read != SPC_DICTIONARY_CHUNK_SIZE;
		UINT8 *data = OP_NEWA(UINT8, to_read+1);
		if (!data)
		{
			status = SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			break;
		}
		data[to_read] = '\0';
		OpFileLength was_read = 0;
		status = dic_file.Read(data, (OpFileLength)to_read, &was_read);
		if (OpStatus::IsError(status) || (OpFileLength)to_read != was_read)
		{
			OP_DELETEA(data);
			status = SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR);
			break;
		}
		//replace_byte(data, to_read, '\0','\n'); // Fix for Math_es_ES
		int chunk_sz = to_read;
		if (!is_last) // we don't want the chunk to be splitted in a word...
		{
			int i;
			for (i = to_read; i >= 0 && !is_ascii_new_line(data[i]); i--);
			chunk_sz = i+1;
		}
		if (chunk_sz <= 0) // 128 kb of text at the same line!?
		{
			OP_DELETEA(data);
			status = SPC_ERROR(OpStatus::ERR);
			break;
		}
		int idx = m_chunks.AddElement(SingleDictionaryChunk(data, chunk_sz));
		if (idx < 0)
		{
			OP_DELETEA(data);
			status = SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			break;
		}
		if (!idx) // first chunk...
		{
			int dummy = 0;
			int dic_words = MAX(pos_num_to_int(data, file_len, dummy), 0);
			m_lines.SetInitCapacity(dic_words+m_approx_insert_count+1);
		}
		SingleDictionaryChunk *chunk = m_chunks.GetElementPtr(idx);
		if (OpStatus::IsError(status = chunk->Tokenize(&m_lines)))
			break;
		if (is_last)
		{
			if (m_insert_data)
			{
				idx = m_chunks.AddElement(SingleDictionaryChunk(m_insert_data, m_insert_data_len));
				if (idx < 0)
				{
					status = SPC_ERROR(OpStatus::ERR_NO_MEMORY);
					break;
				}
				m_insert_data = NULL; // will be deleted when corresponding SingleDictionaryChunk::FreeChunkData() is called
				m_insert_data_len = 0;
				chunk = m_chunks.GetElementPtr(idx);
				if (OpStatus::IsError(status = chunk->Tokenize(&m_lines)))
					break;
			}
			finished = TRUE;
		}
		m_dic_pos += chunk_sz;
	} while (!is_last && (time_out == 0.0 || g_op_time_info->GetWallClockMS() < time_out));
	
	dic_file.Close();
	if (OpStatus::IsError(status))
		return status;
	int line_pos = 0;
	for (i=0;i<m_chunks.GetSize();i++)
	{
		if (m_chunks[i].GetLinesInChunk())
			m_chunks[i].SetFirstLineInChunk(m_lines.GetElementPtr(line_pos));
		line_pos += m_chunks[i].GetLinesInChunk();
	}
	return OpStatus::OK;
}
#endif //!USE_HUNSPELL_ENGINE

#endif // INTERNAL_SPELLCHECK_SUPPORT
