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

/* ===============================OpSpellCheckerAffix=============================== */

OpSpellCheckerAffix::OpSpellCheckerAffix(int flag_index, BOOL is_suffix, BOOL can_combine, int rule_count)
	: FlagClass(AFFIX_TYPE_AFFIX,flag_index), m_rules(rule_count), m_unconditional_rules(8)
{
	m_can_combine = can_combine;
	m_is_suffix = is_suffix;
	m_char_count = 0;
	m_max_cond_len = 0;
	m_bit_units = 0;
	m_bits = NULL;
	m_accept_mask = NULL;
	m_terminate_mask = NULL;
	m_rule_count_bits = 0;
}

OpSpellCheckerAffix::~OpSpellCheckerAffix()
{
	OP_DELETEA(m_bits);
}

OP_STATUS OpSpellCheckerAffix::AddRule(OpSpellCheckerAffixRule rule)
{
	rule.SetIndex(m_rules.GetSize());
	int res = m_rules.AddElement(rule);
	if (res >= 0 && !rule.GetConditionString())
		res = m_unconditional_rules.AddElement(m_rules.GetElementPtr(res));
	return res < 0 ? SPC_ERROR(OpStatus::ERR_NO_MEMORY) : OpStatus::OK;
}

#define ACCEPT_BITS(pos,ch) (m_accept_mask + ((pos)*m_char_count + (ch)) * m_bit_units)
#define TERM_BITS(pos) (m_terminate_mask + (pos) * m_bit_units)
#define SET_BIT32(bits,x) bits[x>>5] |= 1 << (x&0x1F)
#define ZERO_BIT32(bits,x) bits[x>>5] &= ~(1 << (x&0x1F))

OP_STATUS OpSpellCheckerAffix::SetConditionProperties(int max_cond_len, int char_count)
{
	int i,j,k;
	m_char_count = char_count;
	m_max_cond_len = max_cond_len;
	int rule_count = GetRuleCount();
	m_rule_count_bits = MAX(bits_to_represent(rule_count),1);
	if (!char_count) // only unconditional rules
	{
		OP_ASSERT(!max_cond_len && m_unconditional_rules.GetSize() == m_rules.GetSize());
		return OpStatus::OK;
	}
	OP_ASSERT(rule_count);
	m_bit_units = (rule_count-1)/(sizeof(*m_bits)*8) + 1;
	int alloc_units = (max_cond_len*char_count + max_cond_len) * m_bit_units;
	m_bits = OP_NEWA(UINT32, alloc_units);
	if (!m_bits)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	op_memset(m_bits,0,alloc_units*sizeof(*m_bits));
	m_accept_mask = m_bits;
	m_terminate_mask = m_bits + max_cond_len*char_count*m_bit_units;
	for (i=0;i<rule_count;i++)
	{
		OpSpellCheckerAffixRule *rule = GetRulePtr(i);
		int cond_len = rule->GetConditionLength();
		for (j=MAX(cond_len-1,0);j<max_cond_len;j++) // set termination bits for >= cond_len
			SET_BIT32(TERM_BITS(j),i);
		for (j=cond_len;j<max_cond_len;j++) // set accept bits for > cond_len
			for (k=0;k<char_count;k++)
				SET_BIT32(ACCEPT_BITS(j,k),i);
	}
	return OpStatus::OK;
}

void OpSpellCheckerAffix::SetRuleConditions(OpSpellCheckerAffixRule *rule, int *cond, int count, int pos, BOOL negative)
{
	int i;
	int rule_idx = rule->GetIndex();
	OP_ASSERT(pos < rule->GetConditionLength());
	if (negative)
	{
		for (i=0;i<m_char_count;i++)
			SET_BIT32(ACCEPT_BITS(pos,i),rule_idx);
	}
	for (i=0;i<count;i++)
	{
		OP_ASSERT(cond[i] < m_char_count);
		if (negative)
			ZERO_BIT32(ACCEPT_BITS(pos,cond[i]),rule_idx);
		else
			SET_BIT32(ACCEPT_BITS(pos,cond[i]),rule_idx);
	}
}

void OpSpellCheckerAffix::GetMatchingRules(UINT32 *chars, UINT32 *bit_buffer, int *result, int &result_count)
{
	int i,j,pos;
	result_count = 0;
	if (!m_max_cond_len) // only unconditional rules
	{
		for (i=0;i<m_rules.GetSize();i++)
			result[i] = i;
		result_count = i;
		return;
	}
	op_memset(bit_buffer,0xFF,m_bit_units*sizeof(UINT32));
	UINT32 *accept_bits, *term_bits;
	UINT32 has_more = -1, terminators;
	for (pos=0;has_more;pos++)
	{
		OP_ASSERT(pos < m_max_cond_len);
		accept_bits = ACCEPT_BITS(pos,chars[pos]);
		term_bits = TERM_BITS(pos);
		has_more = 0;
		for (i=0;i<m_bit_units;i++)
		{
			bit_buffer[i] &= accept_bits[i];
			terminators = bit_buffer[i] & term_bits[i];
			if (terminators)
			{
				for (j=0;j<32;j++)
					if (terminators & (1<<j) && result_count != SPC_MAX_MATCHING_RULES_PER_WORD)
						result[result_count++] = (i<<5) + j;
				bit_buffer[i] &= ~terminators;
			}
			has_more |= bit_buffer[i];
		}
	}
}

#endif // !USE_HUNSPELL_ENGINE
#endif // INTERNAL_SPELLCHECK_SUPPORT
