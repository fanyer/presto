/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_SPEEDDIALCONTEXT_H
#define DOM_EXTENSIONS_SPEEDDIALCONTEXT_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domobj.h"

class OpGadget;

/** This class implements opera.contexts.speeddial object.
 */
class DOM_ExtensionSpeedDialContext
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_ExtensionSpeedDialContext *&new_obj, OpGadget *gadget, DOM_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_SPEEDDIAL_CONTEXT || DOM_Object::IsA(type); }

protected:

	DOM_ExtensionSpeedDialContext(OpGadget *gadget)
		: m_gadget(gadget)
	{
	}

	OpGadget *m_gadget;
};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_SPEEDDIALCONTEXT_H
