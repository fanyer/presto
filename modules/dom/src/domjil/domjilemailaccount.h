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

#ifndef DOM_DOMJILEMAILACCOUNT_H
#define DOM_DOMJILEMAILACCOUNT_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"

class DOM_JILEmailAccount : public DOM_JILObject
{
public:
	DOM_JILEmailAccount() {}
	~DOM_JILEmailAccount() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_EMAILACCOUNT || DOM_JILObject::IsA(type); }
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	static OP_STATUS Make(DOM_JILEmailAccount*& new_obj, DOM_Runtime* runtime);
	OP_STATUS SetAccount(const OpMessaging::EmailAccount* account);
private:
	OpString m_id;
	OpString m_name;
	struct NullOrUndefSt
	{
		unsigned int id:2;
		unsigned int name:2;
	} m_undefnull;
};

class DOM_JILEmailAccount_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILEmailAccount_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_EMAILACCOUNT_PROTOTYPE) {}
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILEMAILACCOUNT_H
