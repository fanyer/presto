/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VALIDITYSTATE_H
#define VALIDITYSTATE_H

#include "modules/dom/src/domobj.h"

class DOM_HTMLElement;

class DOM_ValidityState : public DOM_Object
{
public:
	static OP_STATUS Make(DOM_ValidityState*& validity_state, DOM_HTMLElement* html_node, DOM_EnvironmentImpl *environment);
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual void GCTrace();

private:
	DOM_ValidityState(DOM_HTMLElement* html_node) : m_html_elm_node(html_node) {}

private:
	DOM_HTMLElement* m_html_elm_node;
};

#endif // VALIDITYSTATE_H
