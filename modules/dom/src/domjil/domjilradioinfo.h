/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILRADIOINFO_H
#define DOM_DOMJILRADIOINFO_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpTelephonyNetworkInfo.h"

class DOM_JILRadioInfo : public DOM_JILObject, public OpRadioSourceListener
{
public:
	virtual ~DOM_JILRadioInfo();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_RADIOINFO || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILRadioInfo*& new_jil_radio_info, DOM_Runtime* runtime);
	virtual void OnRadioSourceChanged(OpTelephonyNetworkInfo::RadioSignalSource radio_source, BOOL is_roaming);
	virtual void OnRadioDataBearerChanged(OpTelephonyNetworkInfo::RadioDataBearer radio_source) {}
	virtual void GCTrace();

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
private:
	DOM_JILRadioInfo() : m_on_radio_source_changed(NULL) {}
	OP_STATUS CreateRadioSignalSourceTypes();
	const uni_char* RadioSignalSourceValueToString(OpTelephonyNetworkInfo::RadioSignalSource radio_src);
	ES_Object* m_on_radio_source_changed;
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILRADIOINFO_H
