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

#ifndef DOM_DOMJILMESSAGEFOLDERTYPES_H
#define DOM_DOMJILMESSAGEFOLDERTYPES_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"

#define MESSAGE_FOLDER_TYPE_DRAFTS UNI_L("drafts")
#define MESSAGE_FOLDER_TYPE_INBOX UNI_L("inbox")
#define MESSAGE_FOLDER_TYPE_OUTBOX UNI_L("outbox")
#define MESSAGE_FOLDER_TYPE_SENTBOX UNI_L("sentbox")
#define MESSAGE_FOLDER_TYPE_UNKNOWN UNI_L("unknown")

class DOM_JILMessageFolderTypes : public DOM_JILObject
{
public:
	DOM_JILMessageFolderTypes() {}
	~DOM_JILMessageFolderTypes() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_MESSAGEFOLDERTYPES || DOM_JILObject::IsA(type); }

	static OP_STATUS Make(DOM_JILMessageFolderTypes*& new_obj, DOM_Runtime* runtime);

protected:
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILMESSAGEFOLDERTYPES_H
