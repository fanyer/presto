/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_NAMESPACE_RULE_H
#define CSS_NAMESPACE_RULE_H

#include "modules/style/src/css_rule.h"
#include "modules/logdoc/namespace.h"

class CSS_NamespaceRule : public CSS_Rule
{
public:
	CSS_NamespaceRule(int ns_idx) : m_ns_idx(ns_idx) { g_ns_manager->GetElementAt(m_ns_idx)->IncRefCount(); }
	virtual ~CSS_NamespaceRule() { g_ns_manager->GetElementAt(m_ns_idx)->DecRefCount(); }
	virtual Type GetType() { return NAMESPACE; }

	const uni_char* GetPrefix() { return g_ns_manager->GetElementAt(m_ns_idx)->GetPrefix(); }
	const uni_char* GetURI() { return g_ns_manager->GetElementAt(m_ns_idx)->GetUri(); }

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);

private:
	int m_ns_idx;
};

#endif // CSS_NAMESPACE_RULE_H
