/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilaccelerometerinfo.h"
#include "modules/pi/device_api/OpAccelerometer.h"

/* static */ OP_STATUS
DOM_JILAccelerometerInfo::Make(DOM_JILAccelerometerInfo*& new_accelerometer_info, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_accelerometer_info = OP_NEW(DOM_JILAccelerometerInfo, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_accelerometer_info, runtime, runtime->GetPrototype(DOM_Runtime::JIL_ACCELEROMETERINFO_PROTOTYPE), "AccelerometerInfo"));
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILAccelerometerInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	// JIL 1.2.1 specification says that reading the AccelerometerInfo.xAxis triggers a read of all values from
	// the underlying platform, AccelerometerInfo.yAxis and zAxis merely return the previously obtained values.
	switch (property_atom)
	{
	case OP_ATOM_xAxis:
		if (value)
		{
			double x;
			GET_FAILED_IF_ERROR(g_op_accelerometer->GetCurrentData(&x, &m_y, &m_z));
			m_data_fetched = TRUE;
			SetNumberIfNotNaN(value, x);
		}
		return GET_SUCCESS;
	case OP_ATOM_yAxis:
	case OP_ATOM_zAxis:
		if (value)
		{
			if (m_data_fetched)
				SetNumberIfNotNaN(value, property_atom == OP_ATOM_yAxis ? m_y : m_z);
		}
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILAccelerometerInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_xAxis:
	case OP_ATOM_yAxis:
	case OP_ATOM_zAxis:
		return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

#endif // DOM_JIL_API_SUPPORT
