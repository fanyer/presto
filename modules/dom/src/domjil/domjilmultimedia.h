/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILMULTIMEDIA_H
#define DOM_DOMJILMULTIMEDIA_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"

#ifdef MEDIA_JIL_PLAYER_SUPPORT
class DOM_JILAudioPlayer;
class DOM_JILVideoPlayer;
#endif // MEDIA_JIL_PLAYER_SUPPORT

class DOM_JILMultimedia : public DOM_JILObject
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_MULTIMEDIA || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILMultimedia*& new_jil_multimedia, DOM_Runtime* runtime);

#ifdef MEDIA_JIL_PLAYER_SUPPORT
	DOM_DECLARE_FUNCTION(stopAll);
	DOM_DECLARE_FUNCTION(getVolume);

	enum { FUNCTIONS_ARRAY_SIZE = 3 };
#endif // MEDIA_JIL_PLAYER_SUPPORT

protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
private:
	DOM_JILMultimedia()
#ifdef MEDIA_JIL_PLAYER_SUPPORT
		: m_audio_player(NULL)
		, m_video_player(NULL)
#endif // MEDIA_JIL_PLAYER_SUPPORT
	{}

#ifdef MEDIA_JIL_PLAYER_SUPPORT
	DOM_JILAudioPlayer* m_audio_player;
	DOM_JILVideoPlayer* m_video_player;
#endif // MEDIA_JIL_PLAYER_SUPPORT
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILMULTIMEDIA_H
