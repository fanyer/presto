/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILPOWERINFO_H
#define DOM_DOMJILPOWERINFO_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/pi/device_api/OpPowerStatus.h"

class DOM_JILAddressBookItem;

class DOM_JILPowerInfo : public DOM_JILObject, public OpPowerStatusListener
{
public:
	virtual ~DOM_JILPowerInfo();
	virtual void GCTrace();
	static OP_STATUS Make(DOM_JILPowerInfo*& new_powerinfo, DOM_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_POWERINFO || DOM_Object::IsA(type); }

	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	// from OpPowerStatusListener
	virtual void OnWakeUp(OpPowerStatus* power_status);
	virtual void OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level);
	virtual void OnPowerSourceChange(OpPowerStatus* power_status, PowerSupplyType power_source);
	virtual void OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low);
private:
	/** Converts charge byte (as is in OpPowerStatus) to percent value */
	double ConvertChargeByteToPercent(BYTE charge_byte){ return op_ceil(charge_byte / 2.55); }
	DOM_JILPowerInfo();

	ES_Object* m_onChargeLevelChange;
	ES_Object* m_onChargeStateChange;
	ES_Object* m_onLowBattery;
};

#endif // DOM_JIL_API_SUPPORT
#endif //!DOM_DOMJILPOWERINFO_H
