/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_conditional_rule.h"
#include "modules/style/src/css_parser.h"

BOOL
CSS_ConditionalRule::EnterRule(FramesDocument* doc, CSS_MediaType medium)
{
	CSS_ConditionalRule* parent = GetConditionalRule();
	if (!parent || parent->EnterRule(doc,medium))
		return Evaluate(doc, medium);
	else
		return FALSE;
}

/* virtual */ OP_STATUS
CSS_ConditionalRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(GetCssTextHeader(stylesheet, buf));
	RETURN_IF_ERROR(buf->Append(" {\n"));
	CSS_Rule* rule = static_cast<CSS_Rule*>(m_rules.First());
	while (rule)
	{
		for (unsigned int i = 0; i < indent_level + 1; i++)
			RETURN_IF_ERROR(buf->Append("  "));
		RETURN_IF_ERROR(rule->GetCssText(stylesheet, buf, indent_level + 1));
		RETURN_IF_ERROR(buf->Append("\n"));
		rule = static_cast<CSS_Rule*>(rule->Suc());
	}
	for (unsigned int i = 0; i < indent_level; i++)
		RETURN_IF_ERROR(buf->Append("  "));
	return buf->Append("}");
}

CSS_PARSE_STATUS
CSS_ConditionalRule::InsertRule(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len, unsigned int idx)
{
	OP_ASSERT(idx <= GetRuleCount());

	CSS_Rule* before_rule = GetRule(idx);
	return stylesheet->ParseAndInsertRule(hld_prof, before_rule, this, NULL, FALSE, CSS_TOK_DOM_RULE, text, len);
}

BOOL
CSS_ConditionalRule::DeleteRule(HLDocProfile* hld_prof, CSS* stylesheet, unsigned int idx)
{
	Link* l = m_rules.First();
	while (l && idx-- > 0) l = l->Suc();
	if (l)
	{
		stylesheet->RuleRemoved(hld_prof, static_cast<CSS_Rule*>(l));
		l->Out();
		OP_DELETE(l);
		return TRUE;
	}
	else
		return FALSE;
}

void CSS_ConditionalRule::DeleteRules(HLDocProfile* hld_prof, CSS* stylesheet)
{
	Link* l;
	while ((l = m_rules.First()))
	{
		stylesheet->RuleRemoved(hld_prof, static_cast<CSS_Rule*>(l));
		l->Out();
		OP_DELETE(l);
	}
}
