/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILDEVICESTATEINFO_H
#define DOM_DOMJILDEVICESTATEINFO_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/geolocation/geolocation.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/pi/device_api/OpDeviceStateInfo.h"
#include "modules/dochand/win.h"

class JILPositionInfoCallbackImpl;
struct GetSelfLocationCallbackImpl;

class DOM_JILDeviceStateInfo : public DOM_JILObject, public ScreenPropsChangedListener
{
	friend class JILPositionInfoCallbackImpl;
	friend struct GetSelfLocationCallbackImpl;
public:
	static OP_STATUS Make(DOM_JILDeviceStateInfo*& new_object, DOM_Runtime* runtime);

	virtual ~DOM_JILDeviceStateInfo(){};

	virtual void GCTrace();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_DEVICESTATEINFO || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(requestPositionInfo);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
	// from ScreenPropsChangedListener
	virtual void OnScreenPropsChanged();

	/** Makes new PositionInfo Object from OpGeolocation::Position
	 *
	 * @param new_position - set to newly created PositionInfo ES_Object
	 * @param runtime - runtime to which newly constructed object will be added
	 * @param position - geoloc info from which data will be imported to JIL PositionInfo. If NULL then
	 *  output object will be filled with undefined values.
	 * @return @see OpStatus
	 */
	static OP_STATUS MakePositionInfoObject(ES_Object** new_position, DOM_Runtime* runtime, const OpGeolocation::Position* position);

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

private:
	DOM_JILDeviceStateInfo();

	const uni_char *GetAudioPathString(OpDeviceStateInfo::AudioOutputDevice device) const;

	ES_Object* m_on_position_retrieved;
	ES_Object* m_on_screen_change_dimension;

	enum JILAudioPath
	{
		JIL_AUDIOPATH_SPEAKER = 0,
		JIL_AUDIOPATH_RECEIVER,
		JIL_AUDIOPATH_EARPHONE,
		JIL_AUDIOPATH_LAST
	};

	const uni_char* m_audio_path_strings[JIL_AUDIOPATH_LAST];
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILDEVICESTATEINFO_H
