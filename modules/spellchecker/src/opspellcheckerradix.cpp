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

/* ===============================OpSpellCheckerRadix=============================== */

RadixEntry* OpSpellCheckerRadix::GetRadixEntry(UINT32 *idx_str, int len, int &level, OpSpellCheckerRadix *&radix)
{
	int i;
	radix = this;
	RadixEntry *entry;
	unsigned int index;
	for (i=0;i<len;i++)
	{
		index = *(idx_str++);
		entry = radix->AtIndex(index);
		if (entry->IsRadixPtr())
			radix = entry->GetRadix();
		else
		{
			level = i;
			return entry;
		}
	}
	level = len;
	return radix->AtIndex(0);
}

OP_STATUS OpSpellCheckerRadix::ClearCounters(int entries)
{
	int level = 0;
	int *pos = OP_NEWA(int, SPC_MAX_DIC_FILE_WORD_LEN+1);
	OpSpellCheckerRadix **radixes = OP_NEWA(OpSpellCheckerRadix*, SPC_MAX_DIC_FILE_WORD_LEN+1);
	if (!pos || !radixes)
	{
		OP_DELETEA(pos);
		OP_DELETEA(radixes);
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	pos[0] = 0;
	radixes[0] = this;
	OpSpellCheckerRadix *radix = this;
	RadixEntry *entry;
	while (level >= 0)
	{
		if (pos[level] == entries)
		{
			level--;
			continue;
		}
		radix = radixes[level];
		entry = radix->AtIndex(pos[level]++);
		if (entry->IsCounter())
			entry->SetCount(0);
		else // radix...
		{
			level++;
			radixes[level] = entry->GetRadix();
			pos[level] = 0;
		}
	}
	OP_DELETEA(pos);
	OP_DELETEA(radixes);
	return OpStatus::OK;
}

int OpSpellCheckerRadix::GetIndexOf(RadixEntry *entry)
{
	int i;
	for (i = 0; AtIndex(i)->Entry() != entry->Entry(); i++);
	return i;
}

int OpSpellCheckerRadix::GetIndexOf(OpSpellCheckerRadix *radix)
{
	RadixEntry entry(radix);
	return GetIndexOf(&entry);
}

/* ===============================RadixEntry=============================== */

OpSpellCheckerRadix* RadixEntry::GetParent()
{
	OP_ASSERT(sizeof(RadixEntry) == sizeof(SPC_UINT_PTR));

	SPC_UINT_PTR *entry = &m_entry;
	while (*(--entry) != SPC_RADIX_START_MARKER);
	return reinterpret_cast<OpSpellCheckerRadix*>(entry+1);
}

int RadixEntry::GetIndex()
{
	OP_ASSERT(sizeof(RadixEntry) == sizeof(SPC_UINT_PTR));

	SPC_UINT_PTR *entry = &m_entry;
	int idx = 0;
	while (*(--entry) != SPC_RADIX_START_MARKER)
		idx++;
	return idx;	
}

void RadixEntry::ConvertToIndexes(UINT32 *buf, int &len, BOOL &self_entry)
{
	len = 0;
	int idx = GetIndex();
	self_entry = !idx;
	if (!self_entry)
		buf[len++] = idx;
	
	OpSpellCheckerRadix *radix = GetParent();
	OpSpellCheckerRadix *parent = radix->Parent();
	while (parent)
	{
		buf[len++] = parent->GetIndexOf(radix);
		radix = parent;
		parent = radix->Parent();
	}
	UINT32 *fst = buf, *snd = buf+len-1;
	int flip_count;
	for (flip_count = len/2; flip_count--; fst++, snd--)
	{
		UINT32 tmp = *fst;
		*fst = *snd;
		*snd = tmp;
	}
}

#endif //!SPC_USE_HUNSPELL_ENGINE
#endif // INTERNAL_SPELLCHECK_SUPPORT
