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

/* ===============================CompoundRule=============================== */

OP_STATUS CompoundRule::Initialize(int cond_count, int total_cond_flags, OpSpellCheckerAllocator *allocator)
{
	m_added_conditions = 0;
	m_total_cond_flags = total_cond_flags;
	m_last_single_pos = -1;
	m_cond_count = cond_count;
	if (!cond_count)
	{
		m_jump_table = NULL;
		m_retry_table = NULL;
		m_multiple = NULL;
		return OpStatus::OK;
	}
	m_jump_table = (UINT8*)allocator->Allocate8(total_cond_flags*(cond_count+1));
	m_retry_table = (UINT8*)allocator->Allocate8(total_cond_flags*cond_count);
	m_multiple = OP_NEWA(BOOL, cond_count);
	if (!m_jump_table || !m_retry_table || !m_multiple)
	{
		OP_DELETEA(m_multiple);
		m_multiple = NULL;
		return SPC_ERROR(OpStatus::ERR);
	}
	return OpStatus::OK;
}

#define SPC_JUMP_TABLE(x) (m_jump_table + (x)*m_total_cond_flags)
#define SPC_RETRY_TABLE(x) (m_retry_table + (x)*m_total_cond_flags)
#define SPC_NO_JUMP 255
#define SPC_EXISTS 254

void CompoundRule::AddCondition(UINT16 *cond_ids, int cond_ids_count, BOOL is_negative, BOOL multiple)
{
	int i,j,k;
	OP_ASSERT(m_added_conditions < m_cond_count);
	UINT8 *jump = SPC_JUMP_TABLE(m_added_conditions), *retry;
	op_memset(jump, is_negative ? SPC_EXISTS : SPC_NO_JUMP, m_total_cond_flags);
	for (i=0;i<cond_ids_count;i++)
		jump[cond_ids[i]] = is_negative ? SPC_NO_JUMP : SPC_EXISTS;
	m_multiple[m_added_conditions++] = multiple;
	if (m_added_conditions != m_cond_count)
		return;
	op_memset(m_jump_table+m_total_cond_flags*m_cond_count,0xFF,m_total_cond_flags); // set the "after last" condition to match nothing
	for (i=m_cond_count-1;i>=0;i--)
	{
		multiple = m_multiple[i];
		if (!multiple && m_last_single_pos < 0)
			m_last_single_pos = i;
		for (j=0;j<m_total_cond_flags;j++)
		{
			jump = SPC_JUMP_TABLE(i);
			if (jump[j] != SPC_EXISTS)
				continue;
			jump[j] = multiple ? 0 : 1;
			BOOL other_accept_passed = FALSE;
			for (k=i-1;k>=0 && m_multiple[k];k--)
			{
				jump = SPC_JUMP_TABLE(k);
				retry = SPC_RETRY_TABLE(k);
				if (jump[j] == SPC_NO_JUMP)
				{
					if (!other_accept_passed)
						jump[j] = (UINT8)(i-k + (multiple ? 0 : 1));
					else // jump[j] will be set later to previous accept
						retry[j] = (UINT8)i;
				}
				else // SPC_EXISTS
				{
					OP_ASSERT(jump[j] == SPC_EXISTS);
					if (other_accept_passed)
						break;
					other_accept_passed = TRUE;
					retry[j] = (UINT8)i;
				}
			}
		}
	}
	OP_DELETEA(m_multiple);
	m_multiple = NULL;
}

BOOL CompoundRule::Matches(UINT16 *indexes, int count)
{
	UINT8 *jumps = m_jump_table;
	UINT8 *retry = m_retry_table;
	int cond_pos = 0, index_pos = 0, retry_cond = 0, retry_index = 0;
	int idx,jump;
	for (;;)
	{
		idx = indexes[index_pos];
		jump = jumps[idx];
		cond_pos += jump;
		if (jump==SPC_NO_JUMP) // doesn't match condition!
		{
do_retry:
			if (!retry_cond) // There is no previous point to retry from
				return FALSE;
			cond_pos = retry_cond;
			index_pos = retry_index;
			retry_cond = 0;
			retry = SPC_RETRY_TABLE(cond_pos);
			jumps = SPC_JUMP_TABLE(cond_pos);
			continue;
		}
		if (!retry_cond && retry[idx])
		{
			retry_cond = retry[idx];
			retry_index = index_pos;
		}
		if (++index_pos == count)
		{
			if (cond_pos > m_last_single_pos)
				return TRUE;
			goto do_retry;			
		}
		if (jump)
		{
			if (jump == 1)
			{
				retry += m_total_cond_flags;
				jumps += m_total_cond_flags;
			}
			else
			{
				retry = SPC_RETRY_TABLE(cond_pos);
				jumps = SPC_JUMP_TABLE(cond_pos);
			}
		}
	}
	// unreachable...
}

#endif //!USE_HUNSPELL_ENGINE

#endif // INTERNAL_SPELLCHECK_SUPPORT
