/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILCONFIG_H
#define DOM_DOMJILCONFIG_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

class DOM_JILConfig : public DOM_JILObject
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_CONFIG || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILConfig*& new_jil_config, DOM_Runtime* runtime);
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(setAsWallpaper);
	DOM_DECLARE_FUNCTION(setDefaultRingtone);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };
protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
private:
	DOM_JILConfig() {}
	friend class DOM_JILDevice;
	static int HandleSetSystemResourceError(OP_SYSTEM_RESOURCE_SETTER_STATUS error_code, ES_Value* return_value, DOM_Runtime* origining_runtime);

};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILCONFIG_H
