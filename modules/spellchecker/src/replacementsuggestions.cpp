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

int GetEditDist(OpSpellCheckerReplacementState *rep_state, UINT32 *indexes, int len)
{
	int i,j;
	int max_dst = 15;
	if (rep_state->word_count == SPC_MAX_REPLACEMENTS)
		max_dst = rep_state->word_heap->edit_dist - 1;
	int wrong_len = rep_state->replace_start_indexes_len;
	UINT32 *wrong_indexes = rep_state->replace_start_indexes;
	if (op_abs(len-wrong_len) > max_dst)
		return 16;
	UINT8 **rows = rep_state->lev_rows;
	UINT8 *current = rows[0], *prev = rows[2], *prev_prev = rows[1], *tmp;
	for (i=len;i>=0;i--)
		current[i] = i;
	UINT8 cost,dist;
	for (i = 0; i < wrong_len; i++)
	{
		tmp = prev;
		prev = current;
		current = prev_prev;
		prev_prev = tmp;
		current[0] = i+1;
		for (j=0;j<len;j++)
		{
			cost = wrong_indexes[i] != indexes[j];
			dist = MIN(prev[j+1], current[j])+1;
			dist = MIN(dist, prev[j]+cost);
			if (i && j && wrong_indexes[i] == indexes[j-1] && wrong_indexes[i-1] == indexes[j])
				dist = MIN(dist, prev_prev[j-1]+cost);
			current[j+1] = dist;
		}
	}
	return (int)current[len];
}

OP_STATUS OpSpellChecker::TreeIterateSearch(OpSpellCheckerLookupState *lookup_state, double timeout, BOOL must_finish, BOOL &finished)
{
	int i, edit_dist;
	OpSpellCheckerReplacementState *rep_state = &lookup_state->rep_state;
	ReplacementWordIterator *iterator = rep_state->rep_iterator;
	if (!rep_state->word_heap)
	{
		rep_state->word_heap = (WordReplacement*)lookup_state->lookup_allocator.AllocatePtr(SPC_MAX_REPLACEMENTS*sizeof(WordReplacement));
		for (i=0;i<3;i++)
		{
			rep_state->lev_rows[i] = (UINT8*)lookup_state->lookup_allocator.AllocatePtr(SPC_MAX_DIC_FILE_WORD_LEN+1);
			if (!rep_state->lev_rows[i])
				break;
		}
		if (!rep_state->word_heap || i != 3)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	WordReplacement *word_heap = rep_state->word_heap;
	int countdown = 1000;
	BOOL aborted = FALSE;
	WordReplacement word;
	while (iterator->Next(word))
	{
		if (!(word.bit_flags & (1<<AFFIX_TYPE_ONLYINCOMPOUND | 1<<AFFIX_TYPE_NOSUGGEST)))
		{ 
			edit_dist = GetEditDist(rep_state, iterator->Indexes(), iterator->Length());
			if (edit_dist < 16)
			{
				word.edit_dist = edit_dist;
				if (rep_state->word_count < SPC_MAX_REPLACEMENTS)
					rev_heap_add(word, word_heap, rep_state->word_count++);
				else if (word_heap->edit_dist > (unsigned int)edit_dist)
					rev_heap_replace_first(word, word_heap, rep_state->word_count);
			}
		}
		if (!--countdown)
		{
			if (timeout != 0.0 && g_op_time_info->GetWallClockMS() >= timeout)
			{
				aborted = TRUE;
				break;
			}
			countdown = 1000;
		}
	}
	finished = !aborted || must_finish;
	if (finished && rep_state->word_count)
	{
		for (i=0;i<rep_state->word_count;i++)
		{
			CompoundWordReplacement comp_rep(word_heap+i);
			if (rep_state->replacements.AddElement(comp_rep) < 0)
				return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
		}
	}
	return OpStatus::OK;
}

#endif //!USE_HUNSPELL_ENGINE
#endif // INTERNAL_SPELLCHECK_SUPPORT
