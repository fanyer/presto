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

#ifndef DOM_DOMJILMESSAGEQUANTITIES_H
#define DOM_DOMJILMESSAGEQUANTITIES_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"

class DOM_JILMessageQuantities : public DOM_JILObject
{
public:
	DOM_JILMessageQuantities() {}
	~DOM_JILMessageQuantities() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_MESSAGEQUANTITIES || DOM_JILObject::IsA(type); }

	static OP_STATUS Make(DOM_JILMessageQuantities*& new_obj, DOM_Runtime* runtime, int total, int read, int unread);
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILMESSAGEQUANTITIES_H
