/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef M2_MERLIN_COMPATIBILITY

#include "adjunct/m2/src/engine/lexicon.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/desktop_util/adt/hashiterator.h"

#include <ctype.h> // tolower()


// version history
// 0 early alphas.. obsolete.. encountering this will cause reindex
// 1 used up to 7.5 preview 3.. encounterng this will cause rewriting to speed up reading next time
// 2 current version, added ability to keep as char or uni_chars in memory (char used if every character is <= 255)

#define		CURRENT_LEXICON_VERSION		2

/***********************************************************************************
**
**	Lexicon::Init
**
***********************************************************************************/

OP_STATUS Lexicon::Init(const uni_char* path, const uni_char* name, BOOL read_file, BOOL* existed)
{
	RETURN_IF_ERROR(CreateLexiconEntry(0, NULL, m_temp_lexicon_entry, FALSE));
	RETURN_IF_ERROR(m_path.Set(path));
	RETURN_IF_ERROR(m_name.Set(name));

	if (!read_file)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_entries_file.Init(path, name, 4, 1, FALSE, existed, TRUE));

	INT32 version = 0;

	RETURN_IF_ERROR(m_entries_file.ReadHeaderValue(0, version));

	if (version != 1 && version != CURRENT_LEXICON_VERSION)
	{
		return OpStatus::ERR;
	}

	INT32 dirty_count = 0;
	RETURN_IF_ERROR(ReadLexicon(&dirty_count));

	return OpStatus::OK;
}


/***********************************************************************************
**
**	Lexicon::ConvertTo
**
***********************************************************************************/

OP_STATUS Lexicon::ConvertTo(StringTable &string_table)
{
	int i;
	OpString word, full_path;
	OpString name;
	OpINTSortedVector id_list;
	OP_STATUS err;

	int space_char;
	for (i = 0; i < GetWordCount(); ++i)
	{
		if ((i % 2000) == 1999)
			if (OpStatus::IsError(err = string_table.Commit()))
				goto lexicon_convert_to_abort;

		if ((err = GetWord(i, word)) != OpStatus::OK)
			continue;

		if ((err = FindWord(word.CStr(), id_list)) != OpStatus::OK)
			continue;

		while ((space_char=word.FindFirstOf(' '))!=KNotFound)
			word[space_char]='_';

		for (unsigned j = 0; j < id_list.GetCount(); ++j)
		{
			OP_ASSERT(id_list.GetByIndex(j) != 0);
			if (OpStatus::IsError(err = string_table.Insert(word.CStr(), id_list.GetByIndex(j))))
				goto lexicon_convert_to_abort;
		}
	}

	if (OpStatus::IsError(err = string_table.Commit()))
		goto lexicon_convert_to_abort;

	return OpStatus::OK;

lexicon_convert_to_abort:
	OpStatus::Ignore(string_table.Clear());
	OpStatus::Ignore(string_table.Close());

	name.Set(m_name);
	name.Append(UNI_L(".act"));
	BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	browser_utils->PathDirFileCombine(full_path, m_path, name);

	OpFile tmpFile;
	BOOL exists;
	RETURN_IF_ERROR(tmpFile.Construct(full_path.CStr()));
	RETURN_IF_ERROR(tmpFile.Exists(exists));
	if (exists)
		tmpFile.Delete();

	name.Set(m_name);
	name.Append(UNI_L(".lex"));
	browser_utils->PathDirFileCombine(full_path, m_path, name);

	RETURN_IF_ERROR(tmpFile.Construct(full_path.CStr()));
	RETURN_IF_ERROR(tmpFile.Exists(exists));
	if (exists)
		tmpFile.Delete();

	return err;
}

/***********************************************************************************
**
**	Lexicon::Erase
**
***********************************************************************************/

OP_STATUS Lexicon::Erase(Lexicon *lexicon)
{
	int i;
	BOOL err;
	OpString path, name, full_path, file_name;
	uni_char suffix[32];
	OpFile delete_file;
	BOOL exists;

	path.Set(lexicon->m_path);
	name.Set(lexicon->m_name);

	OP_DELETE(lexicon);

	file_name.Set(name);
	file_name.Append(UNI_L(".dat"));

	BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	browser_utils->PathDirFileCombine(full_path, path, file_name);

	err = FALSE;

	RETURN_IF_ERROR(delete_file.Construct(full_path.CStr()));
	RETURN_IF_ERROR(delete_file.Exists(exists));
	if (exists && delete_file.Delete() != OpStatus::OK)
		err = TRUE;

	i = 1;
	while (i > 0)
	{
		OpFile file;
		uni_sprintf(suffix, UNI_L("_%i.dat"), i);
		file_name.Set(name);
		file_name.Append(suffix);

		browser_utils->PathDirFileCombine(full_path, path, file_name);

		RETURN_IF_ERROR(file.Construct(full_path.CStr()));
		RETURN_IF_ERROR(file.Exists(exists));
		if (exists && file.Delete() != OpStatus::OK)
			err = TRUE;

		i *= 2;
	}

	return err ? OpStatus::ERR_NO_ACCESS : OpStatus::OK;
}


/***********************************************************************************
**
**	Lexicon::Exists
**
***********************************************************************************/

BOOL Lexicon::Exists(const uni_char* path, const uni_char* name)
{
	OpString full_path;

	MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->PathDirFileCombine(full_path, path, name);

	full_path.Append(".dat");

	BOOL exists;
	OpFile tmpFile;
	if (tmpFile.Construct(full_path.CStr()) != OpStatus::OK)
		return FALSE;
	if (tmpFile.Exists(exists) != OpStatus::OK)
		return FALSE;
	return exists;
}


/***********************************************************************************
**
**	Lexicon::GetWord
**
***********************************************************************************/

OP_STATUS Lexicon::GetWord(INT32 index, OpString& word)
{
	LexiconEntry* entry = m_entries_list.GetByIndex(index);

	if (!entry)
		return OpStatus::ERR;

	if (entry->IsChar())
	{
		return word.Set(entry->GetChar());
	}
	else
	{
		return word.Set(entry->GetUniChar());
	}
}


/***********************************************************************************
**
**	Lexicon::FindWord
**
***********************************************************************************/

OP_STATUS Lexicon::FindWord(const uni_char* word, OpINTSortedVector& id_list, BOOL partial_match_allowed)
{
	id_list.Clear();

	INT32 index;
	INT32 count;

	if (!word)
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(GetMatch(word, index, count, partial_match_allowed));

	OpSortedVector<LexiconEntry, CompareLexiconDiskOrder> entries;

	INT32 i;

	for (i = 0; i < count; i++)
		RETURN_IF_ERROR(entries.Insert(m_entries_list.GetByIndex(index + i)));

	OpString8 temp_buffer;
	OpINT32Set total_id_set;

	for (i = 0; i < count; i++)
	{
		OpINT32Set id_set;

		LexiconEntry* entry = entries.GetByIndex(i);

		BlockFile* file;

		RETURN_IF_ERROR(GetIndexFile(entry->GetBlockSize(), file));

		INT32 id_count;

		RETURN_IF_ERROR(file->ReadValue(entry->GetBlockIndex(), 0, id_count));

		if (!id_count)
			continue;

		if (!temp_buffer.Reserve(id_count * 4))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(file->Read(temp_buffer.CStr(), id_count * 4));

		unsigned char*	c = (unsigned char*) temp_buffer.CStr();

		OpINT32Set& id_set_to_use = i == 0 ? total_id_set : id_set;

		id_set_to_use.SetMinimumCount(id_count);

		for (INT32 j = 0; j < id_count; j++, c += 4)
		{
			INT32 id = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];

//			OP_ASSERT(id != 0);

			if (id > 0)
			{
				id_set_to_use.Add(id);
			}
			else
			{
				id_set_to_use.Remove(-id);
			}
		}

		if (id_set.GetCount() > 0)
		{
			total_id_set.SetMinimumCount(total_id_set.GetCount() + id_set.GetCount());
			id_set.CopyAllToHashTable((OpHashTable&) total_id_set);
		}
	}

	for (INT32SetIterator it(total_id_set); it; it++)
		RETURN_IF_ERROR(id_list.Insert(it.GetData()));

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Lexicon::GetMatch
**
***********************************************************************************/

OP_STATUS Lexicon::GetMatch(const uni_char* start_of_word, INT32& index, INT32& count, BOOL partial_match_allowed)
{
	index = 0;
	count = 0;

	RETURN_IF_ERROR(m_temp_lexicon_entry->Init(0, start_of_word));

	INT32 closest;
	if (partial_match_allowed)
		closest = m_entries_list.FindInsertPosition(m_temp_lexicon_entry);
	else
		closest = m_entries_list.Find(m_temp_lexicon_entry);

	if (!partial_match_allowed)
	{
		if (closest != -1)
		{
			index = closest;
			count = 1;
		}

		return OpStatus::OK;
	}

	INT32 first = closest;
	INT32 last = closest + 1;

	INT32 len = uni_strlen(start_of_word);

	for (;;)
	{
		LexiconEntry* entry = m_entries_list.GetByIndex(first);

		if (!entry || m_temp_lexicon_entry->CompareWord(entry, len))
			break;

		first--;
	}

	for (;;)
	{
		LexiconEntry* entry = m_entries_list.GetByIndex(last);

		if (!entry || m_temp_lexicon_entry->CompareWord(entry, len))
			break;

		last++;
	}

	index = first + 1;
	count = last - first - 1;

	return OpStatus::OK;
}


/***********************************************************************************
**
**	Lexicon::ReadLexicon
**
***********************************************************************************/

OP_STATUS Lexicon::ReadLexicon(INT32* dirty_count)
{
	OpString word;

	// seek to first block

	RETURN_IF_ERROR(m_entries_file.Seek(0));

	INT32 number_of_entries_using_only_chars = 0;

	for (;;)
	{
		INT32 index = m_entries_file.GetCurrentBlock();

		if (index >= m_entries_file.GetBlockCount())
			break;

		INT32 is_char_and_block_size_and_index;

		RETURN_IF_ERROR(m_entries_file.ReadValue(is_char_and_block_size_and_index));
		RETURN_IF_ERROR(m_entries_file.ReadString(word, (is_char_and_block_size_and_index & 0x80000000) != 0));

		// check if is_char_and_block_size_and_index is zero, which means dirty item, to be removed

		if (!is_char_and_block_size_and_index)
		{
			if (dirty_count)
			{
				(*dirty_count)++;
			}
			continue;
		}

		LexiconEntry* entry = NULL;

		RETURN_IF_ERROR(CreateLexiconEntry(index, word.CStr(), entry, FALSE));

		entry->SetBlockSizeAndIndexPacked(is_char_and_block_size_and_index);

		if (entry->IsChar())
			number_of_entries_using_only_chars++;

		RETURN_IF_ERROR(m_entries_list.Insert(entry));
		RETURN_IF_ERROR(m_entries_list_disk_order.Insert(entry));
	}

	return OpStatus::OK;
}


/***********************************************************************************
**
**	Lexicon::GetIndexFile
**
***********************************************************************************/

OP_STATUS Lexicon::GetIndexFile(INT32 block_size, BlockFile*& block_file)
{
	OP_ASSERT(block_size > 0);

	INT32 count = m_index_files.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		block_file = m_index_files.Get(i);

		if (block_file->GetBlockSize() == (OpFileLength)block_size)
		{
			return OpStatus::OK;
		}
	}

	block_file = OP_NEW(BlockFile, ());
	if (!block_file)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(block_file->Init(m_path.CStr(), m_name.CStr(), 0, block_size, TRUE, NULL, TRUE));

	return m_index_files.Add(block_file);
}


/***********************************************************************************
**
**	Lexicon::CreateLexiconEntry
**
***********************************************************************************/

OP_STATUS Lexicon::CreateLexiconEntry(INT32 entry_index, const uni_char* word, LexiconEntry*& entry, BOOL to_lower)
{
	LexiconBlock* block = m_lexicon_blocks.Get(m_lexicon_blocks.GetCount() - 1);
	entry = NULL;

	if (block)
		block->CreateLexiconEntry(entry_index, word, entry, to_lower);

	if (entry)
		return OpStatus::OK;

	block = OP_NEW(LexiconBlock, ());

	if (!block || block->BufferOOM())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_lexicon_blocks.Add(block));

	return block->CreateLexiconEntry(entry_index, word, entry, to_lower);
}

/***********************************************************************************
**
**	Lexicon::LexiconBlock::CreateLexiconEntry
**
***********************************************************************************/

OP_STATUS Lexicon::LexiconBlock::CreateLexiconEntry(INT32 entry_index, const uni_char* word, LexiconEntry*& entry, BOOL to_lower)
{
	INT32 bytes_per_character = 2;

	if (word)
	{
		bytes_per_character = 1;

		const uni_char* temp = word;

		while (*temp)
		{
			if (*temp > 255)
			{
				bytes_per_character = 2;
				break;
			}
			temp++;
		}
	}

	entry = NULL;

	INT32 len = word ? uni_strlen(word) : MAX_WORD_LENGTH;

	if (len > MAX_WORD_LENGTH)
		len = MAX_WORD_LENGTH;

	INT32 bytes_needed = 8 + (len + 1) * bytes_per_character;

	// align

	bytes_needed += 3;
	bytes_needed &= ~3;

	if (m_bytes_used + bytes_needed > LEXICON_MEMORY_BLOCK_SIZE)
		return OpStatus::ERR;

	entry = (LexiconEntry*) (m_buffer + m_bytes_used);

	m_bytes_used += bytes_needed;

	return entry->Init(entry_index, word, len, to_lower, bytes_per_character);
}


/***********************************************************************************
**
**	Lexicon::LexiconEntry::Init
**
***********************************************************************************/

OP_STATUS Lexicon::LexiconEntry::Init(INT32 entry_index, const uni_char* word, INT32 max_len, BOOL to_lower, INT32 bytes_per_character)
{
	m_entry_index = entry_index;

	if (bytes_per_character == 2)
	{
		m_is_char_and_block_size_and_index = 0;

		uni_char* dst = (uni_char*) &m_char;
		*dst = 0;

		if (word)
		{
			uni_strlcpy(dst, word, max_len + 1);

			if (to_lower)
			{
				while (*dst)
				{
					*dst = uni_tolower(*dst);
					dst++;
				}
			}
		}
	}
	else
	{
		m_is_char_and_block_size_and_index = 0x80000000;

		char* dst = &m_char;
		*dst = 0;

		if (word)
		{
			for (INT32 i = 0; i < max_len && *word; i++)
			{
				dst[i] = (char) word[i];
			}

			dst[max_len] = 0;

			if (to_lower)
			{
				while (*dst)
				{
					*dst = tolower(BYTE(*dst));
					dst++;
				}
			}
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**	Lexicon::LexiconEntry::SetBlockSizeAndIndex
**
***********************************************************************************/

void Lexicon::LexiconEntry::SetBlockSizeAndIndex(INT32 block_size, INT32 block_index)
{
#ifdef DEBUG_ENABLE_OPASSERT
	INT32 real_block_size = block_size;
#endif
	INT32 i = 0;

	while (block_size)
	{
		block_size >>= 1;
		i++;
	}

	SetBlockSizeAndIndexPacked((i << 26) | block_index);

	OP_ASSERT(real_block_size == GetBlockSize());
	OP_ASSERT(block_index == GetBlockIndex());
}

/***********************************************************************************
**
**	Lexicon::LexiconEntry::GetBlockSize
**
***********************************************************************************/

INT32 Lexicon::LexiconEntry::GetBlockSize() const
{
	INT32 bit_pos = ((m_is_char_and_block_size_and_index & 0x7fffffff) >> 26);
	return bit_pos ? (1 << (bit_pos - 1)) : 0;
}

/***********************************************************************************
**
**	Lexicon::LexiconEntry::CompareWord
**
***********************************************************************************/

inline int uni_nuni_strcmp(const uni_char *p, const char *q)
{
	while (*p && *p == (uni_char)((unsigned char)*q))
	{
		++p;
		++q;
	}
	return (int)*p - (int)(uni_char)((unsigned char)*q);
}

inline int uni_nuni_strncmp(const uni_char *p, const char *q, INT32 number_of_characters_to_compare)
{
	while (*p && *p == (uni_char)((unsigned char)*q))
	{
		if (!number_of_characters_to_compare)
			return 0;

		--number_of_characters_to_compare;
		++p;
		++q;
	}

	if (!number_of_characters_to_compare)
		return 0;

	return (int)*p - (int)(uni_char)((unsigned char)*q);
}

INT32 Lexicon::LexiconEntry::CompareWord(const LexiconEntry* other_entry) const
{
	if (IsChar())
	{
		if (other_entry->IsChar())
		{
			return strcmp(GetChar(), other_entry->GetChar());
		}
		else
		{
			return -uni_nuni_strcmp(other_entry->GetUniChar(), GetChar());
		}
	}
	else
	{
		if (other_entry->IsChar())
		{
			return uni_nuni_strcmp(GetUniChar(), other_entry->GetChar());
		}
		else
		{
			return uni_strcmp(GetUniChar(), other_entry->GetUniChar());
		}
	}
}

INT32 Lexicon::LexiconEntry::CompareWord(const LexiconEntry* other_entry, INT32 number_of_characters_to_compare) const
{
	if (IsChar())
	{
		if (other_entry->IsChar())
		{
			return strncmp(GetChar(), other_entry->GetChar(), number_of_characters_to_compare);
		}
		else
		{
			return -uni_nuni_strncmp(other_entry->GetUniChar(), GetChar(), number_of_characters_to_compare);
		}
	}
	else
	{
		if (other_entry->IsChar())
		{
			return uni_nuni_strncmp(GetUniChar(), other_entry->GetChar(), number_of_characters_to_compare);
		}
		else
		{
			return uni_strncmp(GetUniChar(), other_entry->GetUniChar(), number_of_characters_to_compare);
		}
	}
}

#endif // M2_MERLIN_COMPATIBILITY

#endif //M2_SUPPORT
