/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_CONDITIONAL_RULE_H
#define CSS_CONDITIONAL_RULE_H

#include "modules/style/src/css_ruleset.h"

class CSS_ConditionalRule : public CSS_BlockRule
{
public:

	virtual ~CSS_ConditionalRule() { m_rules.Clear(); }

	CSS_Rule* FirstRule() { return static_cast<CSS_Rule*>(m_rules.First()); }
	CSS_Rule* LastRule() { return static_cast<CSS_Rule*>(m_rules.Last()); }

	BOOL EnterRule(FramesDocument* doc, CSS_MediaType medium);
	virtual BOOL Evaluate(FramesDocument* doc, CSS_MediaType medium) = 0;

	void AddRule(CSS_Rule* rule) { rule->Into(&m_rules); }

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);
	virtual OP_STATUS GetCssTextHeader(CSS* stylesheet, TempBuffer* buf) = 0;

	unsigned int GetRuleCount() { return m_rules.Cardinal(); }

	CSS_Rule* GetRule(unsigned int idx) { Link* l = m_rules.First(); while (l && idx-- > 0) l = l->Suc(); return (CSS_Rule*)l; }

	/** Parse and insert rule at index = idx. */
	CSS_PARSE_STATUS InsertRule(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len, unsigned int idx);

	/** Delete the rule at index = idx. Returns FALSE if index is out-of-bounds, otherwise TRUE. */
	BOOL DeleteRule(HLDocProfile* hld_prof, CSS* stylesheet, unsigned int idx);

	/** Delete all subrules. Needs to be called before this rule is removed from the stylesheet. */
	void DeleteRules(HLDocProfile* hld_prof, CSS* stylesheet);

private:

	/** List of rules inside the conditional block. */
	List<CSS_Rule> m_rules;
};

#endif // CSS_CONDITIONAL_RULE_H
