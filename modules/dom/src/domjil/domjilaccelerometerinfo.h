/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILACCELEROMETERINFO_H
#define DOM_DOMJILACCELEROMETERINFO_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"

class DOM_JILAccelerometerInfo : public DOM_JILObject
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_ACCELEROMETERINFO || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILAccelerometerInfo*& new_accelerometer_info, DOM_Runtime* runtime);

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	inline void SetNumberIfNotNaN(ES_Value* value, double number);
private:
	DOM_JILAccelerometerInfo() : m_data_fetched(FALSE) {} // m_y and m_z dont need initialization

	// Cached values for y and z components
	double m_y;
	double m_z;
	BOOL m_data_fetched;
};
#endif // DOM_JIL_API_SUPPORT

inline void DOM_JILAccelerometerInfo::SetNumberIfNotNaN(ES_Value* value, double number)
{
	if (!op_isnan(number))
		DOMSetNumber(value, number);
}
#endif // DOM_DOMJILACCELEROMETERINFO_H
