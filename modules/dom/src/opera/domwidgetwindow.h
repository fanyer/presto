/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DOM_DOMWIDGETWINDOW_H
#define DOM_DOMWIDGETWINDOW_H

#ifdef GADGET_SUPPORT

#include "modules/dom/src/domobj.h"

class OpGadget;
class OpWindowCommander;

class DOM_WidgetWindow
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_WidgetWindow *&gadget, DOM_Runtime *runtime, OpGadget *opgadget, OpWindowCommander *window_commander);

	virtual ~DOM_WidgetWindow();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WIDGETWINDOW || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(destroy);

	enum {
		FUNCTIONS_destroy = 1,
		FUNCTIONS_ARRAY_SIZE
	};

	DOM_DECLARE_FUNCTION_WITH_DATA(hideOrShow);

	enum {
		FUNCTIONS_WITH_DATA_hide = 1,
		FUNCTIONS_WITH_DATA_show,
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

private:
	void Destroy();

	OpWindowCommander *m_commander;
	OpGadget* m_gadget;
};

#endif // GADGET_SUPPORT
#endif // DOM_DOMWIDGETWINDOW_H
