/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_parser.h"
#include "modules/style/css_condition.h"

/* virtual */
CSS_Condition::~CSS_Condition()
{
	OP_DELETE(next);
}

void CSS_Condition::Negate()
{
	negated = !negated;
}

OP_STATUS CSS_Condition::GetCssTextNegatedStart(TempBuffer* buf)
{
	if (negated)
		return buf->Append("(not ");
	return OpStatus::OK;

}
OP_STATUS CSS_Condition::GetCssTextNegatedEnd(TempBuffer* buf)
{
	if (negated)
		return buf->Append(")");
	return OpStatus::OK;
}

OP_STATUS CSS_SimpleCondition::SetDecl(const uni_char *property, unsigned property_length,
                                       const uni_char *value,    unsigned value_length)
{
	uni_char *decl = OP_NEWA(uni_char, property_length + value_length + 2); // 1 for the null terminator, one for ':'.
	if (!decl) return OpStatus::ERR_NO_MEMORY;

	uni_sprintf(decl, UNI_L("%s:%s"), property, value);
	return SetDecl(decl);
}

OP_STATUS CSS_SimpleCondition::SetDecl(uni_char *decl)
{
	if (!decl) return OpStatus::ERR;
	m_decl = decl;

	CSS_Buffer css_buf;
	if (css_buf.AllocBufferArrays(1))
	{
		css_buf.AddBuffer(m_decl, uni_strlen(m_decl));

		URL base_url; // Dummy, since we only care whether a URL parses
		CSS_Parser* parser = OP_NEW(CSS_Parser, (NULL, &css_buf, base_url, NULL, 1));
		if (!parser)
			return OpStatus::ERR_NO_MEMORY;

		CSS_property_list *prop_list = OP_NEW(CSS_property_list, ());
		if (!prop_list)
		{
			OP_DELETE(parser);
			return OpStatus::ERR_NO_MEMORY;
		}
		parser->SetCurrentPropList(prop_list); // Refcounts prop_list
		parser->SetNextToken( CSS_TOK_DECLARATION );

		OP_STATUS stat;
		TRAP(stat, parser->ParseL());
		if (OpStatus::IsMemoryError(stat))
		{
			OP_DELETE(parser);
			return OpStatus::ERR_NO_MEMORY;
		}
		supported = !prop_list->IsEmpty();
		OP_DELETE(parser);

		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/* virtual */
CSS_SimpleCondition::~CSS_SimpleCondition()
{
	OP_DELETEA(m_decl);
}

/* virtual */
BOOL CSS_SimpleCondition::Evaluate()
{
	return negated ? !supported : supported;
}

/* virtual */
OP_STATUS CSS_SimpleCondition::GetCssText(TempBuffer* buf)
{
	RETURN_IF_ERROR(GetCssTextNegatedStart(buf));
	RETURN_IF_ERROR(buf->Append("("));
	RETURN_IF_ERROR(buf->Append(m_decl));
	RETURN_IF_ERROR(buf->Append(")"));
	return GetCssTextNegatedEnd(buf);
}

/* virtual */
CSS_CombinedCondition::~CSS_CombinedCondition()
{
	OP_DELETE(members);
}

void CSS_CombinedCondition::AddFirst(CSS_Condition *c)
{
	c->next = members;
	members = c;
}

void CSS_CombinedCondition::AddLast(CSS_Condition *c)
{
	if (!members)
	{
		members = c;
		return;
	}
	CSS_Condition* last = members;
	while (last->next)
		last = last->next;
	last->next = c;
	OP_ASSERT(c->next == NULL);
}

/* virtual */
OP_STATUS CSS_CombinedCondition::GetCssText(TempBuffer* buf)
{
	RETURN_IF_ERROR(buf->Append("("));
	RETURN_IF_ERROR(GetCssTextNegatedStart(buf));
	RETURN_IF_ERROR(members->GetCssText(buf));
	for (CSS_Condition *c = members->next; c; c = c->next)
	{
		RETURN_IF_ERROR(AppendCombiningWord(buf));
		RETURN_IF_ERROR(c->GetCssText(buf));
	}
	RETURN_IF_ERROR(GetCssTextNegatedEnd(buf));
	return buf->Append(")");
}

/* virtual */
BOOL CSS_ConditionConjunction::Evaluate()
{
	for (CSS_Condition *c = members; c; c = c->next)
	{
		if (!c->Evaluate())
			return negated;
	}
	return !negated;
}

/* virtual */
OP_STATUS CSS_ConditionConjunction::AppendCombiningWord(TempBuffer* buf)
{
	return buf->Append(" and ");
}

/* virtual */
BOOL CSS_ConditionDisjunction::Evaluate()
{
	for (CSS_Condition *c = members; c; c = c->next)
	{
		if (c->Evaluate())
			return !negated;
	}
	return negated;
}

/* virtual */
OP_STATUS CSS_ConditionDisjunction::AppendCombiningWord(TempBuffer* buf)
{
	return buf->Append(" or ");
}
