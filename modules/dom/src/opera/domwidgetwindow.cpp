/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/dom/src/opera/domwidgetwindow.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"

#include "modules/dom/domeventtypes.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/dominternaltypes.h"

#include "modules/util/opfile/opfile.h"

#include "modules/doc/frm_doc.h"

#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/pi/OpWindow.h"

/* static */ OP_STATUS
DOM_WidgetWindow::Make(DOM_WidgetWindow *&widget_window, DOM_Runtime *runtime, OpGadget *opgadget, OpWindowCommander *window_commander)
{
	widget_window = OP_NEW(DOM_WidgetWindow, ());

	if (!widget_window)
	{
		g_gadget_manager->DestroyGadget(opgadget);
		g_windowCommanderManager->GetUiWindowListener()->CloseUiWindow(window_commander);

		return OpStatus::ERR_NO_MEMORY;
	}

	widget_window->m_gadget = opgadget;
	widget_window->m_commander = window_commander;

	return DOMSetObjectRuntime(widget_window, runtime, runtime->GetPrototype(DOM_Runtime::WIDGETWINDOW_PROTOTYPE), "WidgetWindow");
}

/* virtual */
DOM_WidgetWindow::~DOM_WidgetWindow()
{
	Destroy();
}

/* virtual */ ES_GetState
DOM_WidgetWindow::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_x:
	case OP_ATOM_y:
		if (value)
			if (m_commander)
			{
				INT32 x, y;

				m_commander->GetOpWindow()->GetOuterPos(&x, &y);

				if (property_name == OP_ATOM_x)
					DOMSetNumber(value, x);
				else
					DOMSetNumber(value, y);
			}
			else
				DOMSetNumber(value, -1);
		return GET_SUCCESS;

	case OP_ATOM_width:
	case OP_ATOM_height:
		if (value)
			if (m_commander)
			{
				UINT32 w, h;

				m_commander->GetOpWindow()->GetInnerSize(&w, &h);

				if (property_name == OP_ATOM_width)
					DOMSetNumber(value, w);
				else
					DOMSetNumber(value, h);
			}
			else
				DOMSetNumber(value, -1);
		return GET_SUCCESS;

	case OP_ATOM_closed:
		DOMSetBoolean(value, m_commander ? FALSE : TRUE);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_WidgetWindow::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_x:
	case OP_ATOM_y:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (m_commander)
		{
			INT32 x, y;

			m_commander->GetOpWindow()->GetOuterPos(&x, &y);

			if (property_name == OP_ATOM_x)
				x = static_cast<UINT32>(value->value.number + 0.5);
			else
				y = static_cast<UINT32>(value->value.number + 0.5);

			m_commander->GetOpWindow()->SetOuterPos(x, y);
		}
		return PUT_SUCCESS;

	default:
		return PUT_FAILED;
	}
}

void
DOM_WidgetWindow::Destroy()
{
	if (m_commander)
	{
		g_windowCommanderManager->GetUiWindowListener()->CloseUiWindow(m_commander);
		m_commander = NULL;
	}

	if (m_gadget)
	{
		g_gadget_manager->DestroyGadget(m_gadget);
		m_gadget = NULL;
	}
}

int DOM_WidgetWindow::destroy(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(gadget, DOM_TYPE_WIDGETWINDOW, DOM_WidgetWindow);
	DOM_CHECK_ARGUMENTS("");

	gadget->Destroy();

	return ES_FAILED;
}

int DOM_WidgetWindow::hideOrShow(DOM_Object *this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(gadget, DOM_TYPE_WIDGETWINDOW, DOM_WidgetWindow);
	DOM_CHECK_ARGUMENTS("");

	if (gadget->m_commander)
		gadget->m_commander->GetOpWindow()->Show(data == 1);

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_WidgetWindow)
	DOM_FUNCTIONS_FUNCTION(DOM_WidgetWindow, DOM_WidgetWindow::destroy, "destroy", NULL)
DOM_FUNCTIONS_END(DOM_WidgetWindow)

DOM_FUNCTIONS_WITH_DATA_START(DOM_WidgetWindow)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WidgetWindow, DOM_WidgetWindow::hideOrShow, 0, "hide", NULL)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WidgetWindow, DOM_WidgetWindow::hideOrShow, 1, "show", NULL)
DOM_FUNCTIONS_WITH_DATA_END(DOM_WidgetWindow)

#endif // GADGET_SUPPORT
