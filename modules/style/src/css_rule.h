/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_RULE_H
#define CSS_RULE_H

#include "modules/util/simset.h"
#include "modules/style/css_types.h"

class CSS;
class HLDocProfile;
class CSS_DOMRule;

class CSS_Rule : public ListElement<CSS_Rule>
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L

public:

	enum Type
	{
		UNKNOWN,
		STYLE,
		CHARSET,
		IMPORT,
		SUPPORTS,
		MEDIA,
		FONT_FACE,
		PAGE,
		KEYFRAMES,
		KEYFRAME,
		NAMESPACE,
		VIEWPORT
	};

	CSS_Rule() : m_dom_rule(0) {}
	virtual ~CSS_Rule();
	virtual Type GetType() { return UNKNOWN; }

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0) { buf->Clear(); return OpStatus::OK; /* return empty string for unknown rules. */ }
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len) { return OpStatus::OK; /* Not possible to set text of unknown rules. */ }

	CSS_DOMRule* GetDOMRule() { return m_dom_rule; }
	void SetDOMRule(CSS_DOMRule* dom_rule);

private:
	/** Pointer to the dom interface object. */
	CSS_DOMRule* m_dom_rule;
};

#endif // CSS_RULE_H
