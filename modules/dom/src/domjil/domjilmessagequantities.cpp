/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilmessagequantities.h"

/* static */ OP_STATUS
DOM_JILMessageQuantities::Make(DOM_JILMessageQuantities*& new_obj, DOM_Runtime* runtime, int total, int read, int unread)
{
	new_obj = OP_NEW(DOM_JILMessageQuantities, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::JIL_MESSAGEQUANTITIES_PROTOTYPE), "MessageQuantities"));
	TRAPD(status,
		DOM_Object::PutNumericConstantL(new_obj->GetNativeObject(), "totalMessageCnt", total, runtime);
		DOM_Object::PutNumericConstantL(new_obj->GetNativeObject(), "totalMessageReadCnt", read, runtime);
		DOM_Object::PutNumericConstantL(new_obj->GetNativeObject(), "totalMessageUnreadCnt", unread, runtime);
	);

	return status;
}

#endif // DOM_JIL_API_SUPPORT
