/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_VIEWPORT_RULE_H
#define CSS_VIEWPORT_RULE_H

#include "modules/style/src/css_ruleset.h"
#include "modules/style/css_viewport.h"

class CSS_ViewportRule : public CSS_DeclarationBlockRule
#ifdef CSS_VIEWPORT_SUPPORT
	, public CSS_ViewportDefinition
#endif // CSS_VIEWPORT_SUPPORT
{
public:
	CSS_ViewportRule(CSS_property_list* props) : CSS_DeclarationBlockRule(props) {}

	virtual Type GetType() { return VIEWPORT; }
	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

#ifdef CSS_VIEWPORT_SUPPORT
	virtual void AddViewportProperties(CSS_Viewport* viewport);

private:
	CSS_ViewportLength GetViewportLengthFromDecl(CSS_decl* decl);
	double GetZoomFactorFromDecl(CSS_decl* decl);
#endif // CSS_VIEWPORT_SUPPORT
};

#endif // CSS_VIEWPORT_RULE_H
