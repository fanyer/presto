/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILACCOUNT_H
#define DOM_DOMJILACCOUNT_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"

class DOM_JILAccountInfo : public DOM_JILObject
{
public:
	virtual ~DOM_JILAccountInfo() {}
	static OP_STATUS Make(DOM_JILAccountInfo*& new_account, DOM_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_ACCOUNTINFO || DOM_Object::IsA(type); }

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	static int HandleAccountError(OP_STATUS error_code, ES_Value* return_value, DOM_Runtime* origining_runtime);
private:
	DOM_JILAccountInfo() {}

	/** Computes a unique subscriber ID.
	 *
	 * The algorithm takes IMSI and MSISDN and hashes them
	 * with SHA-256. This is returned in base64 encoded form.
	 */
	OP_STATUS ComputeUniqueUserId(OpString& id);
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILACCOUNT_H
