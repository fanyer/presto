/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_rule.h"
#include "modules/style/css_dom.h"

CSS_Rule::~CSS_Rule()
{
	if (m_dom_rule)
	{
		m_dom_rule->RuleDeleted();
		m_dom_rule->Unref();
	}
}

void CSS_Rule::SetDOMRule(CSS_DOMRule* dom_rule)
{
	if (m_dom_rule)
		m_dom_rule->Unref();
	m_dom_rule = dom_rule;
	if (dom_rule)
		dom_rule->Ref();
}
