/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionuiitemevent.h"

/* static */ OP_STATUS
DOM_ExtensionUIItemEvent::Make(DOM_ExtensionUIItemEvent *&new_obj, DOM_Object *target_object, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionUIItemEvent, ()), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_UIITEM_EVENT_PROTOTYPE), "UIItemEvent"));

	ES_Value value;
	DOMSetObject(&value, target_object);
	RETURN_IF_ERROR(origining_runtime->PutName(*new_obj, UNI_L("item"), value));
	return OpStatus::OK;
}

/* static */ int
DOM_ExtensionUIItemEvent::initUIItemEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_EXTENSION_UIITEM_EVENT);
	DOM_CHECK_ARGUMENTS("sbb-");

	int result = initEvent(this_object, argv, 3, return_value, origining_runtime);

	if (result != ES_FAILED)
		return result;

	if (argv[3].type != VALUE_OBJECT)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	CALL_FAILED_IF_ERROR(this_object->GetRuntime()->PutName(*this_object, UNI_L("item"), argv[3], PROP_READ_ONLY));
	return result;
}


#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionUIItemEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionUIItemEvent, DOM_ExtensionUIItemEvent::initUIItemEvent, "initUIItemEvent", "sbb-")
DOM_FUNCTIONS_END(DOM_ExtensionUIItemEvent)

#endif // EXTENSION_SUPPORT
