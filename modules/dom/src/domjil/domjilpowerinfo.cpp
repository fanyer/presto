/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilpowerinfo.h"


DOM_JILPowerInfo::DOM_JILPowerInfo()
	: m_onChargeLevelChange(NULL)
	, m_onChargeStateChange(NULL)
	, m_onLowBattery(NULL)
{
}

DOM_JILPowerInfo::~DOM_JILPowerInfo()
{
	OpStatus::Ignore(g_op_power_status_monitor->RemoveListener(this));
}

/* virtual */ void
DOM_JILPowerInfo::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_onChargeLevelChange);
	GCMark(m_onChargeStateChange);
	GCMark(m_onLowBattery);
}

/* static */ OP_STATUS
DOM_JILPowerInfo::Make(DOM_JILPowerInfo*& new_powerinfo, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_powerinfo = OP_NEW(DOM_JILPowerInfo, ()), runtime, runtime->GetPrototype(DOM_Runtime::JIL_POWERINFO_PROTOTYPE), "PowerInfo"));
	return g_op_power_status_monitor->AddListener(new_powerinfo);
}

/* virtual */ ES_GetState
DOM_JILPowerInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onChargeLevelChange:
		DOMSetObject(value, m_onChargeLevelChange);
		return GET_SUCCESS;
	case OP_ATOM_onChargeStateChange:
		DOMSetObject(value, m_onChargeStateChange);
		return GET_SUCCESS;
	case OP_ATOM_onLowBattery:
		DOMSetObject(value, m_onLowBattery);
		return GET_SUCCESS;
	case OP_ATOM_percentRemaining:
		if (value)
		{
			BYTE charge_level;
			GET_FAILED_IF_ERROR(g_op_power_status_monitor->GetPowerStatus()->GetBatteryCharge(&charge_level));
			DOMSetNumber(value, ConvertChargeByteToPercent(charge_level));
		}
		return GET_SUCCESS;
	case OP_ATOM_isCharging:
		if (value)
		{
			BOOL is_charging;
			GET_FAILED_IF_ERROR(g_op_power_status_monitor->GetPowerStatus()->IsExternalPowerConnected(&is_charging));
			DOMSetBoolean(value, is_charging);
		}
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILPowerInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onChargeLevelChange:
		return PutCallback(value, m_onChargeLevelChange);
	case OP_ATOM_onChargeStateChange:
		return PutCallback(value, m_onChargeStateChange);
	case OP_ATOM_onLowBattery:
		return PutCallback(value, m_onLowBattery);
	case OP_ATOM_percentRemaining:
	case OP_ATOM_isCharging:
		return PUT_SUCCESS; // read-only
	}
	return PUT_FAILED;
}

/* virtual */ void
DOM_JILPowerInfo::OnWakeUp(OpPowerStatus* power_status)
{
	// ignore it
}
/* virtual */ void
DOM_JILPowerInfo::OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level)
{
	ES_Value argv[1];
	DOMSetNumber(&argv[0], ConvertChargeByteToPercent(new_charge_level));
	OpStatus::Ignore(CallCallback(m_onChargeLevelChange, argv, ARRAY_SIZE(argv)));
	if (new_charge_level == 255)
	{
		DOMSetString(&argv[0], UNI_L("full"));
		OpStatus::Ignore(CallCallback(m_onChargeStateChange, argv, ARRAY_SIZE(argv)));
	}
}
/* virtual */ void
DOM_JILPowerInfo::OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low)
{
	if (is_low && m_onLowBattery)
	{
		BYTE charge;
		RETURN_VOID_IF_ERROR(g_op_power_status_monitor->GetPowerStatus()->GetBatteryCharge(&charge));
		ES_Value argv[1];
		DOMSetNumber(&argv[0], ConvertChargeByteToPercent(charge));
		OpStatus::Ignore(CallCallback(m_onLowBattery, argv, ARRAY_SIZE(argv)));
	}
}
/* virtual */ void
DOM_JILPowerInfo::OnPowerSourceChange(OpPowerStatus* power_status, PowerSupplyType new_power_source)
{
	ES_Value argv[1];
	switch (new_power_source)
	{
	case OpPowerStatusListener::POWER_SOURCE_SOCKET:
		DOMSetString(&argv[0], UNI_L("charging"));
		break;
	default:
		OP_ASSERT(!"Unknown power source");
		// fallthrough
	case OpPowerStatusListener::POWER_SOURCE_BATTERY:
		DOMSetString(&argv[0], UNI_L("discharging"));
	}
	OpStatus::Ignore(CallCallback(m_onChargeStateChange, argv, ARRAY_SIZE(argv)));
}


#endif // DOM_JIL_API_SUPPORT
