/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_CHARSET_RULE_H
#define CSS_CHARSET_RULE_H

#include "modules/style/src/css_rule.h"

class CSS_CharsetRule : public CSS_Rule
{
public:
	CSS_CharsetRule() : m_charset(0) {}
	virtual ~CSS_CharsetRule() { OP_DELETEA(m_charset); }
	virtual Type GetType() { return CHARSET; }

	void SetCharset(uni_char* charset) { OP_DELETEA(m_charset); m_charset = charset; }
	const uni_char* GetCharset() { return m_charset; }

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

private:
	uni_char* m_charset;
};

#endif // CSS_CHARSET_RULE_H
