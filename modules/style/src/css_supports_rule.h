/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_SUPPORTS_RULE_H
#define CSS_SUPPORTS_RULE_H

#include "modules/style/css_media.h"
#include "modules/style/src/css_conditional_rule.h"
#include "modules/style/css_condition.h"

/** An implementation of @supports, as defined by CSS3 Conditionals.

	@see CSS_Condition which actually parses and evaluates of \@supports.
*/
class CSS_SupportsRule : public CSS_ConditionalRule
{
public:

	CSS_SupportsRule(CSS_Condition* condition) : m_condition(condition) {}

	virtual BOOL Evaluate(FramesDocument* doc, CSS_MediaType medium);

	virtual Type GetType() { return SUPPORTS; }

	virtual ~CSS_SupportsRule() { OP_DELETE(m_condition); }

	virtual OP_STATUS GetCssTextHeader(CSS* stylesheet, TempBuffer* buf);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);
private:

	CSS_Condition* m_condition;
};

#endif // CSS_SUPPORTS_RULE_H
