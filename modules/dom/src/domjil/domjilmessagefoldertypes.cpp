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

#include "modules/dom/src/domjil/domjilmessagefoldertypes.h"

/* static */ OP_STATUS
DOM_JILMessageFolderTypes::Make(DOM_JILMessageFolderTypes*& new_obj, DOM_Runtime* runtime)
{
	new_obj = OP_NEW(DOM_JILMessageFolderTypes, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::JIL_MESSAGEFOLDERTYPES_PROTOTYPE), "MessageFolderTypes"));
	RETURN_IF_ERROR(new_obj->PutStringConstant(UNI_L("DRAFTS"),  MESSAGE_FOLDER_TYPE_DRAFTS));
	RETURN_IF_ERROR(new_obj->PutStringConstant(UNI_L("INBOX"),   MESSAGE_FOLDER_TYPE_INBOX));
	RETURN_IF_ERROR(new_obj->PutStringConstant(UNI_L("OUTBOX"),  MESSAGE_FOLDER_TYPE_OUTBOX));
	return new_obj->PutStringConstant(UNI_L("SENTBOX"), MESSAGE_FOLDER_TYPE_SENTBOX);
}

#endif // DOM_JIL_API_SUPPORT
