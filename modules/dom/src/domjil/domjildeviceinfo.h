/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILDEVICEINFO_H
#define DOM_DOMJILDEVICEINFO_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"

class DOM_JILDeviceInfo : public DOM_JILObject
{
public:
	static OP_STATUS Make(DOM_JILDeviceInfo*& new_object, DOM_Runtime* runtime);

	virtual ~DOM_JILDeviceInfo() {}

	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_DEVICEINFO || DOM_Object::IsA(type); }

protected:

	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

private:
	DOM_JILDeviceInfo() {}
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILDEVICEINFO_H
