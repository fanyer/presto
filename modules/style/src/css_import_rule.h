/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_IMPORT_RULE_H
#define CSS_IMPORT_RULE_H

#include "modules/style/src/css_rule.h"
#include "modules/style/css_media.h"

class CSS_ImportRule : public CSS_Rule
{
public:
	CSS_ImportRule(HTML_Element* elm)
		: m_element(elm)
#ifdef SCOPE_CSS_RULE_ORIGIN
		, m_line_number(0)
#endif // SCOPE_CSS_RULE_ORIGIN
	{}

	virtual Type GetType() { return IMPORT; }

	HTML_Element* GetElement() { return m_element; }
	CSS_MediaObject* GetMediaObject(BOOL create = FALSE);

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

#ifdef SCOPE_CSS_RULE_ORIGIN
	/** Set the original line number for this rule. */
	void SetLineNumber(unsigned line_number){ m_line_number = line_number; }

	/** Get the original line number for this rule. */
	unsigned GetLineNumber() const { return m_line_number; }
#endif // SCOPE_CSS_RULE_ORIGIN

private:
	HTML_Element* m_element;

#ifdef SCOPE_CSS_RULE_ORIGIN
	/** The original line number for this style rule. The line number
	    points to the start of the declaration block. */
	unsigned m_line_number;
#endif // SCOPE_CSS_RULE_ORIGIN
};

#endif // CSS_IMPORT_RULE_H
