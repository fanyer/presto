/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionspeeddialcontext.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/doc/frm_doc.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dom/domutils.h"
#include "modules/windowcommander/OpExtensionUIListener.h"

/* static */ OP_STATUS
DOM_ExtensionSpeedDialContext::Make(DOM_ExtensionSpeedDialContext *&new_obj, OpGadget *gadget, DOM_Runtime *origining_runtime)
{
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionSpeedDialContext, (gadget)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_SPEEDDIAL_CONTEXT_PROTOTYPE), "SpeedDialContext");
}

/* virtual */ ES_GetState
DOM_ExtensionSpeedDialContext::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	OP_ASSERT(m_gadget->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_CAPABLE));

	switch (property_name)
	{
	case OP_ATOM_url:
		if (value)
			DOMSetString(value, m_gadget->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL));
		return GET_SUCCESS;

	case OP_ATOM_title:
		if (value)
			DOMSetString(value, m_gadget->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE));
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionSpeedDialContext::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	OP_ASSERT(m_gadget->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_CAPABLE));

	switch (property_name)
	{
	case OP_ATOM_url:
	case OP_ATOM_title:
		{
			if (value->type != VALUE_STRING)
				return PUT_NEEDS_STRING;

			if (property_name == OP_ATOM_url)
			{
				URL origin_url = GetRuntime()->GetOriginURL();
				URL url = g_url_api->GetURL(origin_url, value->value.string);

				if (url.Type() == URL_NULL_TYPE || url.Type() == URL_UNKNOWN || DOM_Utils::IsOperaIllegalURL(url))
					return PUT_SUCCESS;

				PUT_FAILED_IF_ERROR(m_gadget->SetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL, url.GetAttribute(URL::KUniName_With_Fragment).CStr(), TRUE));
			}
			else
				PUT_FAILED_IF_ERROR(m_gadget->SetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE, value->value.string, TRUE));

			if (FramesDocument *frames_doc = GetFramesDocument())
			{
				WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
				wc->GetExtensionUIListener()->OnExtensionSpeedDialInfoUpdated(wc);
			}
		}
		return PUT_SUCCESS;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}
#endif // EXTENSION_SUPPORT
