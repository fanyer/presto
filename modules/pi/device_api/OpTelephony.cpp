/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef PI_TELEPHONY

#include "modules/pi/device_api/OpTelephony.h"

OP_STATUS
OpTelephony::CallRecord::CopyTo(OpTelephony::CallRecord** new_call_record)
{
	OP_ASSERT(new_call_record);
	*new_call_record = OP_NEW(OpTelephony::CallRecord, ());
	RETURN_OOM_IF_NULL(new_call_record);
	OpAutoPtr<OpTelephony::CallRecord> ap_call_record(*new_call_record);
	RETURN_IF_ERROR((*new_call_record)->m_id.Set(m_id));
	RETURN_IF_ERROR((*new_call_record)->m_name.Set(m_name));
	RETURN_IF_ERROR((*new_call_record)->m_phone_number.Set(m_phone_number));
	(*new_call_record)->m_duration = m_duration;
	(*new_call_record)->m_start = m_start;
	(*new_call_record)->m_type = m_type;
	ap_call_record.release();
	return OpStatus::OK;
}

#endif  // PI_TELEPHONY

