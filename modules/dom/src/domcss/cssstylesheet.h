/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CSSSTYLESHEET_H
#define DOM_CSSSTYLESHEET_H

#include "modules/dom/src/domstylesheets/stylesheet.h"
#include "modules/style/css_dom.h"

class DOM_CSSRule;
class CSS;

class DOM_CSSStyleSheet
	: public DOM_StyleSheet
{
public:
	friend class DOM_CSSRuleList;

	virtual ~DOM_CSSStyleSheet() { OP_DELETE(m_css_sheet); }

	static OP_STATUS Make(DOM_CSSStyleSheet *&stylesheet, DOM_Node *owner_node, DOM_CSSRule *import_rule);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_STYLESHEET || DOM_StyleSheet::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(deleteRule);
	DOM_DECLARE_FUNCTION(insertRule);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

protected:
	DOM_CSSStyleSheet(DOM_Node *owner_node, DOM_CSSRule *import_rule);

private:
	CSS_DOMStyleSheet* m_css_sheet;

	/** Set to the import rule if imported, otherwise NULL. */
	DOM_CSSRule* m_import_rule;
};

#endif // DOM_STYLESHEET_H
