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

#ifndef SPC_USE_HUNSPELL_ENGINE

void OpSpellChecker::ReadCompressedIndexes(OpSpellCheckerBitStream &bit_stream, UINT32 *result_buf, int &result_len)
{
	result_len = 0;
	HuffmanEntry entry;
	for (;;)
	{
		entry = m_huffman_to_index[bit_stream.Read16BitNoInc() & m_huffman_to_index_mask];
		OP_ASSERT(entry.code_length);
		bit_stream.IncBits(entry.code_length);
		if (!entry.value)
			return;
		result_buf[result_len++] = entry.value;
	}
}

inline void OpSpellChecker::ReadMappingInfo(OpSpellCheckerBitStream &bit_stream, UINT32 *indexes, int len, PosToChar *pos_to_char, int &pos_to_char_len)
{
	MapMapping *mapping;
	pos_to_char_len = 0;
	int str_pos_bits = m_len_to_bit_representation[len];
	int map_pos_bits;
	while (bit_stream.ReadBit())
	{
		int str_idx = bit_stream.ReadVal16(str_pos_bits);
		mapping = m_mappings.GetElementPtr(indexes[str_idx]);
		map_pos_bits = mapping->bit_representation;
		if (pos_to_char)
		{
			int pos = map_pos_bits ? (int)bit_stream.ReadVal16(map_pos_bits) : 0;
			pos_to_char[pos_to_char_len] = PosToChar((UINT32)str_idx, mapping->chars[pos+1]);
		}
		else
			bit_stream.IncBits(map_pos_bits);
		pos_to_char_len++;
	}
}

inline UINT16 OpSpellChecker::ReadBitFlags(OpSpellCheckerBitStream &bit_stream)
{
	if (!m_has_radix_flags)
		return 0;
	HuffmanEntry entry = m_huffman_to_flags[bit_stream.Read16BitNoInc() & (1<<SPC_FLAGS_HUFFMAN_LEVELS)-1];
	OP_ASSERT(entry.code_length);
	bit_stream.IncBits(entry.code_length);
	return entry.value;
}
inline int OpSpellChecker::ReadCompoundRuleFlags(OpSpellCheckerBitStream &bit_stream, UINT16 *result_buf)
{
	if (!m_has_compound_rule_flags)
		return 0;
	int count = 0;
	while (bit_stream.ReadBit())
	{
		HuffmanEntry entry = m_huffman_to_compound_rule_flag[bit_stream.Read16BitNoInc() & m_huffman_to_compound_rule_flag_mask];
		OP_ASSERT(entry.code_length);
		bit_stream.IncBits(entry.code_length);
		if (result_buf)
			result_buf[count] = entry.value;
		count++;
	}
	return count;
}

#define GET_NEXT_WORD_UNIT(next_unit, bit_stream) do { if (bit_stream.ReadBit()) { next_unit = (UINT8*)bit_stream.ReadPointer(); if (!SPC_IS_NEXT_UNIT_POINTER(next_unit)) next_unit = NULL; } else next_unit = NULL; } while (0)

BOOL OpSpellChecker::GetIndexEntry(UINT32 *indexes, int len, OpSpellCheckerDictionaryPosition &pos, BOOL continuing)
{
	int other_len = 0, same_len;
	UINT8 *unit, *next_unit;
	OpSpellCheckerBitStream bit_stream;
	if (continuing)
	{
		bit_stream = pos.bit_stream;
		next_unit = pos.next_unit;
		same_len = pos.same_len;
		if (pos.self_entry)
		{
			while (!bit_stream.ReadBit()) // no more words in this word unit
			{
				if (!next_unit)
					return FALSE;	
				bit_stream.SetStart(next_unit, 0);
				GET_NEXT_WORD_UNIT(next_unit, bit_stream);
				continue;
			}
			pos.Set(same_len, next_unit, TRUE, bit_stream);
			return TRUE;
		}
	}
	else // pos is not initialized yet...
	{
		int level = 0;
		OpSpellCheckerRadix *dummy = NULL;
		RadixEntry *entry = m_root_radix->GetRadixEntry(indexes, len, level, dummy);
		if (!entry->IsBitPtr())
			return FALSE;
		unit = entry->GetBits();
		bit_stream = OpSpellCheckerBitStream(unit);
		GET_NEXT_WORD_UNIT(next_unit, bit_stream);
		if (level == len) // self-entry
		{
			same_len = len;
			bit_stream.IncBits(1); // ignore has-more-words-in-this-unit bit, always true for the first word unit
			pos.Set(same_len, next_unit, TRUE, bit_stream);
			return TRUE;
		}
		same_len = level+1;
	}
	UINT32 *buf_start = (UINT32*)TempBuffer();
	UINT32 *buf = buf_start + same_len;
	op_memcpy(buf_start, indexes, (continuing ? len : same_len)*sizeof(UINT32));
	indexes += same_len;
	len -= same_len;
	int buf_ofs, dummy=0;
	for (;;)
	{
		if (!bit_stream.ReadBit()) // no more words in this word unit
		{
			if (!next_unit)
			{
				ReleaseTempBuffer();
				return FALSE;
			}
			bit_stream.SetStart(next_unit, 0);
			GET_NEXT_WORD_UNIT(next_unit, bit_stream);
			continue;
		}
		if (bit_stream.ReadBit()) // we should copy from previous word, the chars to copy is already in buf...
			buf_ofs = bit_stream.ReadVal16(SPC_COPY_PREV_WORD_BITS)+1; // chars to copy is stored as to_copy-1
		else
			buf_ofs = 0;
		ReadCompressedIndexes(bit_stream, buf+buf_ofs, other_len);
		if (other_len+buf_ofs != len || !indexes_equal(indexes, buf, len)) // this is NOT the word we're searching for
		{
			ReadMappingInfo(bit_stream, buf_start, same_len+buf_ofs+other_len, NULL, dummy); // pass by the mapping info
			ReadBitFlags(bit_stream); // pass by the bit-flags
			ReadCompoundRuleFlags(bit_stream, NULL); // pass by the compound-rule-flag
		}
		else // we've found the first matching word
			break;
	} 
	ReleaseTempBuffer();
	pos.Set(same_len, next_unit, FALSE, bit_stream);
	return TRUE;
}

OP_STATUS OpSpellChecker::GetDictionaryPositionTree(UINT32 *indexes, int len, int word_index, DictionaryPositionTree *result_root)
{
	int i;
	if (word_index >= m_compoundwordmax || len < m_compoundmin)
		return OpStatus::OK;
	OpSpellCheckerDictionaryPosition dic_pos;
	DictionaryPositionTree *pos_tree = NULL;

	if (IsHashSetFor(indexes, len) && GetIndexFirstEntry(indexes, len, dic_pos)) // check if all of this sub-word is a valid word
	{
		pos_tree = (DictionaryPositionTree*)m_lookup_allocator->AllocatePtr(sizeof(DictionaryPositionTree));
		if (!pos_tree)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		pos_tree->Init(len, dic_pos, m_lookup_allocator);
		pos_tree->Into((Head*)result_root);
	}
	if (len < m_compoundmin*2)
		return OpStatus::OK;
	UINT32 hash = GetHashFor(indexes, m_compoundmin-1);
	for (i = m_compoundmin; len - i >= m_compoundmin; i++)
	{
		if (IsHashSetFor(indexes[i-1], hash) && GetIndexFirstEntry(indexes, i, dic_pos))
		{
			pos_tree = (DictionaryPositionTree*)m_lookup_allocator->AllocatePtr(sizeof(DictionaryPositionTree));
			if (!pos_tree)
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			pos_tree->Init(i, dic_pos, m_lookup_allocator);
			RETURN_IF_ERROR(GetDictionaryPositionTree(indexes+i, len-i, word_index+1, pos_tree));
			if (pos_tree->Empty())
				m_lookup_allocator->RewindBytesIfPossible(sizeof(DictionaryPositionTree));
			else
				pos_tree->Into((Head*)result_root);
		}
	}
	return OpStatus::OK;
}

/** no compound rule flag checking here... */
OP_STATUS OpSpellChecker::HasCorrectChild(DictionaryPositionTree *tree, UINT32 *indexes, int len_left, int real_pos, PosToChar *pos_to_char, int pos_len, BOOL &result)
{
	int i;
	BOOL ok;
	BOOL is_first = !real_pos;
	BOOL is_last = tree->Empty();
	BOOL is_middle = !is_first && !is_last;
	BOOL is_full = is_first && is_last;
	int dic_pos_to_char_len = 0, my_pos_to_char_len;
	UINT16 bit_flags;
	UINT16 *compound_rule_flags = NULL;
	int compound_rule_flag_count = 0;
	for (i = 0; i < pos_len && (int)pos_to_char[i].pos < real_pos + tree->len; i++);
	my_pos_to_char_len = i;
	do
	{
		PosToChar *dic_pos_to_char = (PosToChar*)TempBuffer();
		ReadMappingInfo(tree->dic_pos.bit_stream, indexes, tree->len, dic_pos_to_char, dic_pos_to_char_len);
		if (my_pos_to_char_len == dic_pos_to_char_len)
		{
			ok = TRUE;
			for (i=0;i<my_pos_to_char_len;i++)
			{
				if (pos_to_char[i].pos != real_pos + dic_pos_to_char[i].pos || 
					pos_to_char[i].char_value != dic_pos_to_char[i].char_value)
				{
					ok = FALSE;
					break;
				}
			}
		}
		else
			ok = FALSE;
		ReleaseTempBuffer();
		bit_flags = ReadBitFlags(tree->dic_pos.bit_stream);
		if (m_has_compound_rule_flags)
		{
			if (tree->dic_pos.bit_stream.ReadBit())
			{
				tree->dic_pos.bit_stream.IncBits(-1); // rewind back the bit we just read
				compound_rule_flags = (UINT16*)m_lookup_allocator->Allocate16(SPC_MAX_COMPOUND_RULE_FLAGS_PER_LINE*sizeof(UINT16));
				if (!compound_rule_flags)
					return OpStatus::ERR_NO_MEMORY;
				compound_rule_flag_count = ReadCompoundRuleFlags(tree->dic_pos.bit_stream, compound_rule_flags);
				OP_ASSERT(compound_rule_flag_count);
				m_lookup_allocator->OnlyUsed(compound_rule_flag_count*sizeof(UINT16));
			}
			else
			{
				compound_rule_flags = NULL;
				compound_rule_flag_count = 0;
			}
		}
		if (ok && !m_has_compound_rule_flags) // use the "traditional" compound flags
		{
			ok = FALSE;
			if (is_full)
				ok = !(bit_flags & 1<<AFFIX_TYPE_ONLYINCOMPOUND);
			else
			{
				ok = bit_flags & 1<<AFFIX_TYPE_COMPOUNDFLAG ||
					is_first && bit_flags & 1<<AFFIX_TYPE_COMPOUNDBEGIN ||
					is_middle && bit_flags & 1<<AFFIX_TYPE_COMPOUNDMIDDLE ||
					is_last && bit_flags & 1<<AFFIX_TYPE_COMPOUNDEND;
			}
		}
		if (ok)
		{
			if (tree->word_data.AddElement(DictionaryWordData(bit_flags,compound_rule_flags, compound_rule_flag_count)) < 0)
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		}
	} while (GetIndexNextEntry(indexes, tree->len, tree->dic_pos));
	if (!tree->word_data.GetSize())
	{
		result = FALSE;
		return OpStatus::OK;
	}
	if (is_last)
	{
		result = TRUE;
		return OpStatus::OK;
	}
	BOOL has_correct_child = FALSE;
	DictionaryPositionTree *child = (DictionaryPositionTree*)tree->First();
	while (child)
	{
		DictionaryPositionTree *next = (DictionaryPositionTree*)child->Suc();
		RETURN_IF_ERROR(HasCorrectChild(child, indexes + tree->len, len_left - tree->len, real_pos + tree->len,
			pos_to_char + my_pos_to_char_len, pos_len - my_pos_to_char_len, has_correct_child));
		if (!has_correct_child)	
			child->Out();
		child = next;
	}
	result = !tree->Empty();
	return OpStatus::OK;
}

BOOL OpSpellChecker::MatchesCompoundRule(DictionaryWordData **dic_word_datas, int len)
{
	int i;
	if (len <= 1) // let's ignore compound-rule-flags for non-compound words!?
		return TRUE;
	int compound_indexes[SPC_MAX_ALLOWED_COMPOUNDWORDMAX]; // indexes into the DictionaryWordData's compound_rule_flags array
	UINT16 compound_rule_flags[SPC_MAX_ALLOWED_COMPOUNDWORDMAX]; // compound-rule-flag array for CompoundRule::Matches to work on
	op_memset(compound_indexes, 0, len*sizeof(int));
	int change_idx = len-1;
	for (i=0;i<len;i++)
		compound_rule_flags[i] = dic_word_datas[i]->compound_rule_flags ? dic_word_datas[i]->compound_rule_flags[0] : 0;

	for (;;)
	{
		for (i=0;i<m_compound_rules.GetSize();i++)
		{
			if (m_compound_rules[i]->Matches(compound_rule_flags, len))
				return TRUE;
		}
		for (i = change_idx; i >= 0 && compound_indexes[i] >= dic_word_datas[i]->compound_rule_flag_count-1; i--);
		if (i < 0)
			return FALSE;
		change_idx = i;
		compound_indexes[change_idx]++;
		compound_rule_flags[change_idx] = dic_word_datas[change_idx]->compound_rule_flags[compound_indexes[change_idx]];
		for (i=change_idx+1;i<len;i++)
			compound_rule_flags[i] = dic_word_datas[i]->compound_rule_flags ? dic_word_datas[i]->compound_rule_flags[0] : 0;
	}
}

BOOL OpSpellChecker::IsValidCompoundWordReplacement(CompoundWordReplacement *replacement)
{
	int i;
	if (replacement->count == 1)
		return !(replacement->words[0].bit_flags & (1<<AFFIX_TYPE_ONLYINCOMPOUND));

	if (!m_has_compounds || replacement->count > m_compoundwordmax)
		return FALSE;
	
	BOOL ok = FALSE;
	if (replacement->words[0].bit_flags & (1<<AFFIX_TYPE_COMPOUNDFLAG | 1<<AFFIX_TYPE_COMPOUNDBEGIN))
	{
		if (replacement->words[replacement->count-1].bit_flags & (1<<AFFIX_TYPE_COMPOUNDFLAG | 1<<AFFIX_TYPE_COMPOUNDEND))
		{
			ok = TRUE;
			for (i=1;i<replacement->count-1;i++)
			{
				if (!(replacement->words[i].bit_flags & (1<<AFFIX_TYPE_COMPOUNDFLAG | 1<<AFFIX_TYPE_COMPOUNDMIDDLE)))
				{
					ok = FALSE;
					break;
				}
			}
		}
	}
	if (ok || !m_has_compound_rule_flags)
		return ok;
	
	DictionaryWordData *word_datas = (DictionaryWordData*)TempBuffer();
	op_memset(word_datas, 0, sizeof(DictionaryWordData)*replacement->count);
	DictionaryWordData **_word_datas = (DictionaryWordData**)(word_datas + replacement->count);
	UINT16 *buf = (UINT16*)(_word_datas + replacement->count);
	for (i=0;i<replacement->count;i++)
	{
		WordReplacement *word = replacement->words + i;
		_word_datas[i] = &word_datas[i];
		if (!word->compressed_compound_rule_byte)
			continue;
		OpSpellCheckerBitStream bit_stream(word->compressed_compound_rule_byte, word->compressed_compoundrule_bit);
		int rule_count = ReadCompoundRuleFlags(bit_stream, buf);
		word_datas[i].compound_rule_flag_count = rule_count;
		word_datas[i].compound_rule_flags = buf;
		buf += rule_count;
	}
	ok = MatchesCompoundRule(_word_datas, replacement->count);
	ReleaseTempBuffer();
	return ok;
}

BOOL OpSpellChecker::HasValidCompoundRules(DictionaryPositionTree *tree, DictionaryWordData **dic_word_datas, int level)
{
	int i;
	BOOL is_last = tree->Empty();
	DictionaryPositionTree *child;
	for (i=0;i<tree->word_data.GetSize();i++)
	{
		dic_word_datas[level] = tree->word_data.GetElementPtr(i);
		if (is_last)
		{
			if (MatchesCompoundRule(dic_word_datas, level+1))
				return TRUE;
		}
		else
		{
			for (child = (DictionaryPositionTree*)tree->First(); child; child = (DictionaryPositionTree*)child->Suc())
			{
				if (HasValidCompoundRules(child, dic_word_datas, level+1))
					return TRUE;
			}
		}
	}
	return FALSE;
}

OP_STATUS OpSpellChecker::IsStringCorrectInternal(UINT32 *str, int len, BOOL &is_correct)
{
	if (!len || !*str)
	{
		is_correct = TRUE;
		return OpStatus::OK;
	}
	is_correct = FALSE;
	if (m_state != LOADING_FINISHED)
		return SPC_ERROR(OpStatus::ERR);

	UINT32 *indexes = (UINT32*)m_lookup_allocator->Allocate32(len*sizeof(UINT32));
	if (!indexes)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	if (!ConvertCharsToIndexes(str, indexes, len))
		return OpStatus::OK;
	
	PosToChar *pos_to_char = (PosToChar*)m_lookup_allocator->Allocate32(len*sizeof(PosToChar));
	if (!pos_to_char)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	int pos_len = 0, other_pos_len = 0;
	GetMappingPosToChar(str, len, pos_to_char, pos_len);
	m_lookup_allocator->OnlyUsed(pos_len*sizeof(PosToChar));

	PosToChar *other_pos_to_char = (PosToChar*)m_lookup_allocator->Allocate32(len*sizeof(PosToChar));
	if (!other_pos_to_char)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	
	UINT32 bit_flags;
	OpSpellCheckerDictionaryPosition dic_pos;
	if (IsHashSetFor(indexes, len) && GetIndexFirstEntry(indexes, len, dic_pos))
	{
		do
		{
			ReadMappingInfo(dic_pos.bit_stream, indexes, len, other_pos_to_char, other_pos_len);
			bit_flags = ReadBitFlags(dic_pos.bit_stream);
			ReadCompoundRuleFlags(dic_pos.bit_stream,NULL); // let's ignore compound-rule-flags for non-compound words!?
			if (!(bit_flags & 1<<AFFIX_TYPE_ONLYINCOMPOUND) && pos_len == other_pos_len && !op_memcmp(pos_to_char, other_pos_to_char, pos_len*sizeof(PosToChar)))
			{
				is_correct = TRUE;
				return OpStatus::OK;
			}
		} while (GetIndexNextEntry(indexes, len, dic_pos));
	}
	if (!m_has_compounds || m_compoundwordmax <= 1 || len < m_compoundmin*2)
		return OpStatus::OK;
	
	DictionaryPositionTree tree;
	DictionaryPositionTree *child;
	RETURN_IF_ERROR(GetDictionaryPositionTree(indexes, len, 0, &tree));
	BOOL has_correct_child = FALSE;
	child = (DictionaryPositionTree*)tree.First();
	while (child)
	{
		DictionaryPositionTree *next = (DictionaryPositionTree*)child->Suc();
		RETURN_IF_ERROR(HasCorrectChild(child, indexes, len, 0, pos_to_char, pos_len, has_correct_child));
		if (!has_correct_child)	
			child->Out();
		child = next;
	}
	if (tree.Empty())
		return OpStatus::OK;
	if (!m_has_compound_rule_flags)
	{
		is_correct = TRUE;
		return OpStatus::OK;
	}
	DictionaryWordData **dic_word_datas = (DictionaryWordData**)TempBuffer();
	child = (DictionaryPositionTree*)tree.First();
	while (child)
	{
		DictionaryPositionTree *next = (DictionaryPositionTree*)child->Suc();
		if (HasValidCompoundRules(child, dic_word_datas, 0))
		{
			is_correct = TRUE;
			break;
		}
		child = next;
	}
	ReleaseTempBuffer();
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::SearchReplacements(OpSpellCheckerLookupState *lookup_state, double time_out, UINT32 *chars, int len, double replacement_timeout)
{
	OpSpellCheckerReplacementState *rep_state = &lookup_state->rep_state;
	if (!rep_state->rep_iterator)
	{
		rep_state->rep_iterator = OP_NEW(ReplacementWordIterator, (this));
		if (!rep_state->rep_iterator)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		OP_STATUS status = rep_state->rep_iterator->Init();
		if (OpStatus::IsError(status))
		{
			OP_DELETE(rep_state->rep_iterator);
			rep_state->rep_iterator = NULL;
			return status;
		}
	}
	if (!rep_state->replace_start_indexes)
	{
		ConvertCharsToIndexes(chars, chars, len, TRUE);
		rep_state->replace_start_indexes = chars;
		rep_state->replace_start_indexes_len = len;
	}
	UINT32 *indexes = rep_state->replace_start_indexes;
	len = rep_state->replace_start_indexes_len;
	if (!time_out)
		time_out = replacement_timeout;
	BOOL must_finish = replacement_timeout != 0.0 && replacement_timeout <= time_out;
	if (rep_state->state == OpSpellCheckerReplacementState::NOT_STARTED)
	{
		rep_state->rep_iterator->SetStart(indexes, len);
		rep_state->state = OpSpellCheckerReplacementState::TREE_ITERATE;
	}
	BOOL finished = FALSE;
	if (rep_state->state == OpSpellCheckerReplacementState::TREE_ITERATE)
	{
		RETURN_IF_ERROR(TreeIterateSearch(lookup_state, time_out, must_finish, finished));
	}
	ExpandingBuffer<CompoundWordReplacement> *replacements = &rep_state->replacements;
	int rep_count = replacements->GetSize();
	if (finished && rep_count)
	{
		UINT8 *sort_buf = (UINT8*)lookup_state->lookup_allocator.Allocate32(8*rep_count);
		if (!sort_buf)
			return OpStatus::ERR_NO_MEMORY;
		quick_sort<CompoundWordReplacement>(replacements->Data(), rep_count, sort_buf);
		RETURN_IF_ERROR(WriteOutReplacementsUTF16(lookup_state));
	}
	lookup_state->in_progress = !finished;
	return OpStatus::OK;
}




#endif //!SPC_USE_HUNSPELL_ENGINE

BOOL OpSpellChecker::CanSpellCheck(const uni_char *word, const uni_char *word_end)
{
	OP_ASSERT(word <= word_end);

	int len = word_end - word;

	if (!word || word == word_end || len > SPC_MAX_DIC_FILE_WORD_LEN / (m_8bit_enc ? 1 : 4))
		return FALSE;

	if (uni_fragment_is_any_of('@', (uni_char*)word, word_end))
		return FALSE;

	if (*word >= '0' && *word <= '9')
	{
		while (word != word_end && *word >= '0' && *word <= '9')
			word++;
		if (word + 1 >= word_end)
			return FALSE;
	}
	return TRUE;
}

OP_STATUS OpSpellChecker::IsWordInDictionary(const uni_char *word, BOOL &has_word, BOOL find_replacements, OpSpellCheckerListener *listener, OpSpellCheckerWordIterator *word_iterator)
{
	OpSpellCheckerLookupState state;
	SingleWordIterator simple_iterator(word);
	state.word_iterator = word_iterator ? word_iterator : &simple_iterator;
	RETURN_IF_ERROR(CheckSpelling(&state, 0.0, find_replacements, SPC_REPLACEMENT_SEARCH_MAX_DEFAULT_MS));
	has_word = state.correct == YES;
	if (listener)
		RETURN_IF_ERROR(InformListenerOfLookupResult(NULL,&state, listener));
	return OpStatus::OK;
}


#ifndef SPC_USE_HUNSPELL_ENGINE


OP_STATUS OpSpellChecker::IsManyWordsInDictionary(uni_char *str, BOOL &is_correct)
{
	uni_char *ptr = str;
	is_correct = TRUE;
	while (*ptr && is_correct)
	{
		if (IsWordSeparator(*ptr))
		{
			ptr++;
			continue;
		}
		str = ptr;
		while (*ptr && !IsWordSeparator(*ptr))
			ptr++;
		uni_char old = *ptr;
		*ptr = '\0';
		RETURN_IF_ERROR(IsWordInDictionary((const uni_char*)str, is_correct));
		*ptr = old;
		str = ptr;
	}
	return OpStatus::OK;
}

#define SPC_MAX_SHARPS_CHECK 2

OP_STATUS OpSpellChecker::CheckSharpSInternal(UINT32 *buf, int len, BOOL &is_correct)
{
	int i,j;
	int doubles_ofs[SPC_MAX_SHARPS_CHECK];
	int to_replace[SPC_MAX_SHARPS_CHECK];
	int count = 0, replace_count;
	is_correct = FALSE;
	UINT32 *tmp_buf = (UINT32*)m_lookup_allocator->Allocate32(sizeof(UINT32)*(len-1));
	if (!tmp_buf)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	for (i = 0; i < len-1 && count < SPC_MAX_SHARPS_CHECK; i++)
	{
		if (buf[i] == 's' && buf[i+1] == 's')
			doubles_ofs[count++] = i;
	}
	for (i = 1; i < (1<<count) && !is_correct; i++)
	{
		replace_count = 0;
		for (j=0;j<count;j++)
		{
			if (i&(1<<j) && !(j != (count-1) && i&(1<<(j+1)) && doubles_ofs[j+1] == doubles_ofs[j]+1))
				to_replace[replace_count++] = doubles_ofs[j];
		}
		OP_ASSERT(replace_count);
		int dst_ofs = 0;
		for (j=0;j<=replace_count;j++)
		{
			int copy_ofs = j ? to_replace[j-1] + 2 : 0;
			int copy_len = (j < replace_count ? to_replace[j] : len) - copy_ofs;
			op_memcpy(tmp_buf + dst_ofs, buf + copy_ofs, sizeof(UINT32)*copy_len);
			dst_ofs += copy_len;
			if (j != replace_count)
				tmp_buf[dst_ofs++] = m_sharps_char;
		}
		RETURN_IF_ERROR(IsStringCorrectInternal(tmp_buf, dst_ofs, is_correct));
	}
	return OpStatus::OK;
}

const uni_char* OpSpellChecker::FindWordBreak(const uni_char *str, const uni_char* str_end)
{
	OP_ASSERT(str);
	OP_ASSERT(str <= str_end);

	for (; str != str_end && !is_ambiguous_break(*str) && uni_strchr(m_break_chars, *str) == NULL; str++);

	return str;
}

const uni_char* OpSpellChecker::SkipWordBreaks(const uni_char *str, const uni_char* str_end)
{
	OP_ASSERT(str);
	OP_ASSERT(str <= str_end);

	for (; str != str_end && (is_ambiguous_break(*str) || uni_strchr(m_break_chars, *str) != NULL); str++);

	return str;
}


OP_STATUS OpSpellChecker::CheckSpelling(OpSpellCheckerLookupState *lookup_state, double time_out, BOOL find_replacements, double replacement_timeout, const uni_char *override_word, const uni_char* override_word_end)
{
	int i;
	if (lookup_state->in_progress)
	{
		OP_ASSERT(find_replacements || !"should only happen when we search for replacements!");
		OP_ASSERT(lookup_state->correct == NO);
		return SearchReplacements(lookup_state, time_out, NULL, 0, replacement_timeout);
	}

	OP_ASSERT(lookup_state->correct == MAYBE);
	OP_ASSERT(override_word <= override_word_end);
	const uni_char *current_word = override_word ? override_word : lookup_state->word_iterator->GetCurrentWord();
	int str_len = override_word ? override_word_end-override_word : uni_strlen(current_word);
	const uni_char* current_end =  current_word + str_len;

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
		if ( !find_replacements )
			return OpStatus::OK;
	}

	BOOL is_correct = FALSE;

	if ( lookup_state->correct == MAYBE )
	{
		const uni_char* ptr = SkipWordBreaks(current_word, current_end);
		const uni_char* snd = FindWordBreak(ptr, current_end);

		is_correct = (ptr != current_word || snd != current_end);
		while (is_correct && ptr != current_end)
		{
			RETURN_IF_ERROR(CheckSpelling(lookup_state, 0.0, FALSE, 0.0, ptr, snd));

			if (lookup_state->correct == NO)
			{
				is_correct = FALSE;
			}
			else
			{
				ptr = SkipWordBreaks(snd, current_end);
				snd = FindWordBreak(ptr, current_end);
			}

			lookup_state->Reset();
		}
		if (is_correct)
		{
			lookup_state->correct = YES;
			return OpStatus::OK;
		}
	}

	UINT32 *buf32 = (UINT32*)lookup_state->lookup_allocator.Allocate32(sizeof(UINT32) * (str_len+1) * 2);
	if (!buf32)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	int len = 0, was_written, was_read = 0;
	if (m_8bit_enc)
	{
		UINT8 *buf8 = (UINT8*)lookup_state->lookup_allocator.Allocate8((str_len+1)*2);
		if (!buf8)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		was_written = m_output_converter->Convert(current_word, str_len*2, buf8, str_len*2, &was_read);
		if (was_written <= 0 || was_read != str_len*2)
		{
			// We can only check words where all characters could be represented with the same encoding as the dictionary...
			lookup_state->correct = NO;  //FIXME: OOM.
			return OpStatus::OK;
		}
		for (i=0; i<was_written; i++)
			buf32[i] = (UINT32)buf8[i];
		len = was_written;
		buf32[len] = 0;
		lookup_state->lookup_allocator.OnlyUsed((str_len+1)*2);
	}
	else
	{
		was_written = m_output_converter->Convert(current_word, str_len*2, buf32, sizeof(UINT32)*str_len*2, &was_read);
		if (was_written & 0x3)
			return SPC_ERROR(OpStatus::ERR);
		len = was_written/4;
		buf32[len] = 0;
	}
	
	if ( lookup_state->correct == MAYBE )
	{
		OP_STATUS status;

		m_lookup_allocator = &lookup_state->lookup_allocator;
		
		if (IsAllUpperCase(buf32, len))
		{
			do
			{
				SCToLower(buf32, len);
				if (m_checksharps)
				{
					status = CheckSharpSInternal(buf32, len, is_correct);
					if (is_correct || OpStatus::IsError(status))
						break;
				}
				status = IsStringCorrectInternal(buf32, len, is_correct);
				if (is_correct || OpStatus::IsError(status))
					break;
				SCToUpper(buf32, 1);
				if (m_checksharps)
				{
					status = CheckSharpSInternal(buf32, len, is_correct);
					if (is_correct || OpStatus::IsError(status))
						break;
				}
				status = IsStringCorrectInternal(buf32, len, is_correct);
				if (is_correct || OpStatus::IsError(status))
					break;
				SCToUpper(buf32, len);
				status = IsStringCorrectInternal(buf32, len, is_correct);
			}
			while (FALSE);
		}
		else if (IsOneUpperFollowedByLower(buf32, len))
		{
			SCToLower(buf32, 1);
			status = IsStringCorrectInternal(buf32, len, is_correct);
			if (!is_correct && OpStatus::IsSuccess(status))
			{
				SCToUpper(buf32, 1);
				status = IsStringCorrectInternal(buf32, len, is_correct);
			}
		}
		else
			status = IsStringCorrectInternal(buf32, len, is_correct);
		
		m_lookup_allocator = NULL;
		if (OpStatus::IsError(status))
			return status;

		lookup_state->correct = is_correct ? YES : NO;
	}
		
	if (lookup_state->correct == NO && find_replacements && len >= 2)
		return SearchReplacements(lookup_state, time_out, buf32, len, replacement_timeout);

	return OpStatus::OK;
}

BOOL OpSpellChecker::IsWordSeparator(uni_char ch)
{
	if ( uni_strchr(m_word_chars, ch) != NULL )
		return FALSE;

	return g_internal_spellcheck->IsWordSeparator(ch) || uni_strchr(m_break_chars, ch) != NULL;
}

void OpSpellChecker::WordReplacementToChars(WordReplacement *replacement, ReplacementWordIterator *rep_iterator, UINT32 *res, int &len)
{
	UINT32 i;
	BOOL self_entry = FALSE;
	WordReplacement dummy;
	replacement->bit_entry->ConvertToIndexes(res, len, self_entry);
	rep_iterator->SetStart(res, len);
	for (i=0;i<replacement->in_bits_index;i++)
		rep_iterator->Next(dummy, NULL);
	OpSpellCheckerBitStream map_bits;
	rep_iterator->Next(dummy, &map_bits);
	len = rep_iterator->Length();
	op_memcpy(res, rep_iterator->Indexes(), len*sizeof(UINT32));
	PosToChar *pos_to_char = (PosToChar*)(res + len);
	int pos_to_char_len = 0;
	ReadMappingInfo(map_bits, res, len, pos_to_char, pos_to_char_len);
	int pos_idx = 0;
	for (i=0;i<(UINT32)len;i++)
	{
		if (pos_idx < pos_to_char_len && pos_to_char[pos_idx].pos == i)
			res[i] = pos_to_char[pos_idx++].char_value;
		else
			res[i] = *m_mappings.Data()[res[i]].chars;
	}
}

OP_STATUS OpSpellChecker::WriteOutReplacementsUTF16(OpSpellCheckerLookupState *lookup_state)
{
	int i,j,idx;
	int rep_count = lookup_state->rep_state.replacements.GetSize();
	CompoundWordReplacement *replacements = lookup_state->rep_state.replacements.Data();
	ReplacementWordIterator *rep_iterator = lookup_state->rep_state.rep_iterator;
	UINT32 *buf = (UINT32*)TempBuffer();
	for (i = 0; i < rep_count && lookup_state->replacements.GetSize() < SPC_MAX_REPLACEMENTS; i++)
	{
		CompoundWordReplacement *rep = &replacements[i];
		int len = 0;
		for (j=0;j<rep->count;j++)
		{
			int word_len = 0;
			WordReplacementToChars(&rep->words[j], rep_iterator, buf+len, word_len);
			len += word_len;
		}
		int should_read;
		if (m_8bit_enc)
		{
			UINT8 *buf8 = (UINT8*)buf;
			for (j=0;j<len;j++)
				buf8[j] = (UINT8)buf[j];
			should_read = len;
		}
		else
			should_read = len*4;
		int to_alloc = sizeof(uni_char)*(len+1)*2;
		uni_char *utf16 = (uni_char*)lookup_state->lookup_allocator.Allocate16(to_alloc);
		if (!utf16)
		{
			ReleaseTempBuffer();
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		}
		int was_read = 0;
		int was_written = m_input_converter->Convert(buf, should_read, utf16+1, to_alloc-sizeof(uni_char)*2, &was_read);
		lookup_state->lookup_allocator.OnlyUsed(MAX(was_written+sizeof(uni_char)*2,0));
		if (was_read == should_read && was_written > 0 && !(was_written&1) && !m_removed_words.HasString(utf16+1))
		{
			utf16[0] = '0' + rep->words[0].edit_dist;
			utf16[1+was_written/2] = '\0';
			idx = lookup_state->replacements.AddElement(utf16);
			if (idx < 0)
			{
				ReleaseTempBuffer();
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
			}
			uni_char **list;
			list = lookup_state->replacements.Data();
			while (idx)
			{
				int cmp = uni_strcmp(list[idx], list[idx-1]);
				if (!cmp) // this entry is already in the list, restore the list...
				{
					for (j=idx;j<lookup_state->replacements.GetSize()-1;j++)
						list[j] = list[j+1];
					lookup_state->replacements.Resize(lookup_state->replacements.GetSize()-1);
					break;
				}
				else if (cmp < 0) // sort replacements with the same edit-distance alphabetically
				{
					uni_char *tmp = list[idx];
					list[idx] = list[idx-1];
					list[idx-1] = tmp;
				}
				else
					break;
				idx--;
			}
		}
	}
	ReleaseTempBuffer();
	for (i=0;i<lookup_state->replacements.GetSize();i++)
	{
		uni_char *str = lookup_state->replacements.GetElement(i);
		for (j=0;str[j];j++)
			str[j] = str[j+1];
	}
	const uni_char *wrong_word = lookup_state->word_iterator->GetCurrentWord();
	if (Unicode::IsUpper(*wrong_word))
	{
		BOOL is_correct = FALSE;
		BOOL all_upper;
		uni_char *c;
		for (c = (uni_char*)wrong_word; *c && Unicode::ToLower(*c) != *c; c++);
		all_upper = !*c;
		for (i=0;i<lookup_state->replacements.GetSize();i++)
		{
			uni_char *str = lookup_state->replacements.GetElement(i);
			if (!all_upper)
			{
				if (!Unicode::IsUpper(*str) && Unicode::ToUpper(*str) != *str)
				{
					uni_char old = *str;
					*str = Unicode::ToUpper(*str);
					RETURN_IF_ERROR(IsManyWordsInDictionary(str,is_correct));
					if (!is_correct)
						*str = old;
				}
			}
			else // all_upper
			{
				for (c = (uni_char*)str; *c && Unicode::ToUpper(*c) == *c; c++);
				if (*c) // str is not all uppercase
				{
					uni_char *all_upper = (uni_char*)lookup_state->lookup_allocator.Allocate16((uni_strlen(str)+1)*sizeof(uni_char));
					uni_strcpy(all_upper, str);
					for (c = all_upper; *c; c++)
						*c = Unicode::ToUpper(*c);
					RETURN_IF_ERROR(IsManyWordsInDictionary(all_upper, is_correct));
					if (is_correct)
						uni_strcpy(str, all_upper);
				}
			}
		}
	}
	return OpStatus::OK;
}

/* ===============================OpSpellCheckerLookupState=============================== */

OpSpellCheckerLookupState::OpSpellCheckerLookupState() : lookup_allocator(16384), 
	replacements(SPC_MAX_REPLACEMENTS+1, &lookup_allocator), rep_state(&lookup_allocator)
{
	correct = MAYBE;
	in_progress = FALSE;
	word_iterator = NULL;
}
	
void OpSpellCheckerLookupState::Reset()
{ 
	lookup_allocator.Reset();
	replacements.Reset();
	rep_state.Reset();
	correct = MAYBE;
	in_progress = FALSE;
}

/* ===============================ReplacementWordIterator=============================== */

OP_STATUS ReplacementWordIterator::Init()
{
	m_finished_indexes = OP_NEWA(UINT32, SPC_MAX_DIC_FILE_WORD_LEN);
	m_radix_indexes = OP_NEWA(UINT32, SPC_MAX_DIC_FILE_WORD_LEN);
	m_radixes = OP_NEWA(OpSpellCheckerRadix*, SPC_MAX_DIC_FILE_WORD_LEN);
	if (!m_finished_indexes || !m_radix_indexes || !m_radixes)
	{
		OP_DELETEA(m_finished_indexes);
		OP_DELETEA(m_radix_indexes);
		OP_DELETEA(m_radixes);
		m_finished_indexes = NULL;
		m_radix_indexes = NULL;
		m_radixes = NULL;
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	return OpStatus::OK;
}

void ReplacementWordIterator::SetStart(UINT32 *start_indexes, int len)
{
	int i, idx;
	OP_ASSERT(start_indexes && len);
	op_memset(m_finished_indexes,0xFF,sizeof(UINT32)*SPC_MAX_DIC_FILE_WORD_LEN);
	m_finished = FALSE;

	RadixEntry *entry = NULL;
	OpSpellCheckerRadix *radix = m_spell_checker->GetRootRadix();
	for (i=0;i<len;i++)
	{
		idx = start_indexes[i];
		entry = radix->AtIndex(idx);
		m_radixes[i] = radix;
		if (entry->IsNoPointer())
		{
			m_radix_indexes[i] = 0;
			break;
		}
		else
		{
			m_finished_indexes[i] = idx;
			m_radix_indexes[i] = idx;
			if (entry->IsRadixPtr())
				radix = entry->GetRadix();
			else if (entry->IsBitPtr())
			{
				m_bit_entry = entry;
				m_in_bits_index = 0;
				m_bit_pos.SetStart(entry->GetBits(),0);
				GET_NEXT_WORD_UNIT(m_next, m_bit_pos);
				break;
			}
		}
	}
	m_level = i;
	if (entry->IsRadixPtr())
	{
		radix = entry->GetRadix();
		m_radix_indexes[m_level] = 0; // m_level has already been increased by for (...;i++)
		m_radixes[m_level] = radix;
	}
	if (!entry->IsBitPtr())
	{
		if (!GotoNextBits(TRUE))
		{
			m_finished = TRUE;
			OP_ASSERT(!"Nothing to iterate!");
		}
	}
}

BOOL ReplacementWordIterator::GotoNextBits(BOOL include_current)
{
	int radix_entries = m_spell_checker->GetRadixEntries();
	int i = m_radix_indexes[m_level];
	if (!include_current)
	{
		if (i == (int)m_finished_indexes[m_level])
			i = 0;
		else
			i++;
	}
	OpSpellCheckerRadix *radix;
	RadixEntry *entry;
	for (;;)
	{
		if (i == radix_entries)
		{
			if (!m_level)
				return FALSE;
			m_finished_indexes[m_level--] = -1;
			i = m_radix_indexes[m_level];
			if (i == (int)m_finished_indexes[m_level])
				i = 0;
			else
				i++;
			continue;
		}
		entry = m_radixes[m_level]->AtIndex(i);
		if (entry->IsNoPointer() || i == (int)m_finished_indexes[m_level])
			i++;
		else
		{
			m_radix_indexes[m_level] = i;
			if (entry->IsRadixPtr())
			{
				m_level++;
				radix = entry->GetRadix();
				m_radix_indexes[m_level] = 0;
				i = 0;
				m_radixes[m_level] = radix;
				continue;
			}
			else // bit pointer
			{
				m_bit_entry = entry;
				m_in_bits_index = 0;
				m_bit_pos.SetStart(entry->GetBits(),0);
				GET_NEXT_WORD_UNIT(m_next, m_bit_pos);
				break;
			}
		}
	}
	return TRUE;
}

BOOL ReplacementWordIterator::Next(WordReplacement &result_pos, OpSpellCheckerBitStream *mapping_pos)
{
	if (m_finished)
		return FALSE;
	while (!m_bit_pos.ReadBit()) // no more words in this word unit
	{
		if (!m_next)
		{
			if (!GotoNextBits(FALSE))
			{
				m_finished = TRUE;
				return FALSE;
			}
			continue;
		}
		m_bit_pos.SetStart(m_next,0);
		GET_NEXT_WORD_UNIT(m_next, m_bit_pos);
	}

	if (m_radix_indexes[m_level]) // NOT self entry
	{
		int ofs = m_level+1;
		if (m_bit_pos.ReadBit()) // copy from last word
			ofs += m_bit_pos.ReadVal16(SPC_COPY_PREV_WORD_BITS)+1; // chars to copy is stored as to_copy-1
		int was_read = 0;
		m_spell_checker->ReadCompressedIndexes(m_bit_pos, m_radix_indexes+ofs, was_read);
		m_word_len = ofs+was_read;
	}
	else
		m_word_len = m_level;
	
	result_pos.bit_entry = m_bit_entry;
	result_pos.in_bits_index = (UINT16)(m_in_bits_index++);
	if (mapping_pos)
		*mapping_pos = m_bit_pos;
	
	int dummy = 0;
	m_spell_checker->ReadMappingInfo(m_bit_pos, m_radix_indexes, m_word_len, NULL, dummy); // pass by the mapping info
	result_pos.bit_flags = m_spell_checker->ReadBitFlags(m_bit_pos) & ((1<<SPC_RADIX_FLAGS)-1);
	
	UINT8 *byte = m_bit_pos.Byte();
	int bit = m_bit_pos.Bit();
	if (m_spell_checker->ReadCompoundRuleFlags(m_bit_pos, NULL))
	{
		result_pos.compressed_compound_rule_byte = byte;
		result_pos.compressed_compoundrule_bit = bit;
	}
	else
	{
		result_pos.compressed_compound_rule_byte = NULL;
		result_pos.compressed_compoundrule_bit = 0;
	}
	result_pos.edit_dist = 0; // "initialization"
	return TRUE;
}

#endif //!SPC_USE_HUNSPELL_ENGINE

#endif // INTERNAL_SPELLCHECK_SUPPORT
