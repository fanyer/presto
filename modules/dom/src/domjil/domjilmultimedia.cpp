/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilmultimedia.h"

#include "modules/dom/src/domjil/domjilcamera.h"
#include "modules/dom/src/domjil/domjilaudioplayer.h"
#include "modules/dom/src/domjil/domjilvideoplayer.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

#include "modules/pi/device_api/OpCamera.h"
#include "modules/pi/OpSystemInfo.h"

/* static */
OP_STATUS DOM_JILMultimedia::Make(DOM_JILMultimedia*& new_jil_multimedia, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_jil_multimedia = OP_NEW(DOM_JILMultimedia, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_jil_multimedia, runtime, runtime->GetPrototype(DOM_Runtime::JIL_MULTIMEDIA_PROTOTYPE), "Multimedia"));
#ifdef JIL_CAMERA_SUPPORT
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_CAMERA, runtime))
	{
		DOM_JILCamera* jil_camera;
		RETURN_IF_ERROR(DOM_JILCamera::Make(jil_camera, runtime));
		ES_Value camera_val;
		DOMSetObject(&camera_val, jil_camera);
		RETURN_IF_ERROR(runtime->PutName(new_jil_multimedia->GetNativeObject(), UNI_L("Camera"), camera_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}
#endif // JIL_CAMERA_SUPPORT
#ifdef MEDIA_JIL_PLAYER_SUPPORT
	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_AUDIOPLAYER, runtime))
	{
		RETURN_IF_ERROR(DOM_JILAudioPlayer::Make(new_jil_multimedia->m_audio_player, runtime));
		ES_Value audio_player_val;
		DOMSetObject(&audio_player_val, new_jil_multimedia->m_audio_player);
		RETURN_IF_ERROR(runtime->PutName(new_jil_multimedia->GetNativeObject(), UNI_L("AudioPlayer"), audio_player_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}

	if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_VIDEOPLAYER, runtime))
	{
		RETURN_IF_ERROR(DOM_JILVideoPlayer::Make(new_jil_multimedia->m_video_player, runtime));
		ES_Value video_player_val;
		DOMSetObject(&video_player_val, new_jil_multimedia->m_video_player);
		RETURN_IF_ERROR(runtime->PutName(new_jil_multimedia->GetNativeObject(), UNI_L("VideoPlayer"), video_player_val, PROP_READ_ONLY | PROP_DONT_DELETE));
	}
#endif // MEDIA_JIL_PLAYER_SUPPORT
	return OpStatus::OK;
}

/* virtual */
ES_GetState DOM_JILMultimedia::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
#ifdef MEDIA_JIL_PLAYER_SUPPORT
	case OP_ATOM_isAudioPlaying:
		DOMSetBoolean(value, m_audio_player && m_audio_player->IsPlaying());
		return GET_SUCCESS;
	case OP_ATOM_isVideoPlaying:
		DOMSetBoolean(value, m_video_player && m_video_player->IsPlaying());
		return GET_SUCCESS;
#endif // MEDIA_JIL_PLAYER_SUPPORT
	default:
		break;
	}
	return GET_FAILED;
}

/* virtual */
ES_PutState DOM_JILMultimedia::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
#ifdef MEDIA_JIL_PLAYER_SUPPORT
		case OP_ATOM_isAudioPlaying:
		case OP_ATOM_isVideoPlaying:
			return PUT_SUCCESS;
#endif // MEDIA_JIL_PLAYER_SUPPORT
		default:
			break;
	}
	return PUT_FAILED;
}

#ifdef MEDIA_JIL_PLAYER_SUPPORT
/* static */ int
DOM_JILMultimedia::stopAll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_, DOM_TYPE_JIL_MULTIMEDIA, DOM_JILMultimedia);
	this_->m_audio_player->stop(this_->m_audio_player, NULL, 0, return_value, origining_runtime);
	this_->m_video_player->stop(this_->m_video_player, NULL, 0, return_value, origining_runtime);
	return ES_FAILED;
}

/* static */ int
DOM_JILMultimedia::getVolume(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_MULTIMEDIA);

	double volume;
	TRAPD(error, volume = g_op_system_info->GetSoundVolumeL());
	CALL_FAILED_IF_ERROR_WITH_HANDLER(error, HandleJILError);

	OP_ASSERT(volume >= 0.0 && volume <= 1.0);
	double jil_volume = op_ceil(volume * 10.0);
	DOMSetNumber(return_value, jil_volume);
	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILMultimedia)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMultimedia, DOM_JILMultimedia::stopAll,    "stopAll",   "-", "Multimedia.stopAll")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMultimedia, DOM_JILMultimedia::getVolume,  "getVolume", "-", "Multimedia.getVolume")
DOM_FUNCTIONS_END(DOM_JILMultimedia)

#endif // MEDIA_JIL_PLAYER_SUPPORT

#endif // DOM_JIL_API_SUPPORT
