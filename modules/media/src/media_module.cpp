/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/formats/argsplit.h"
#include "modules/media/camerarecorder.h"
#include "modules/media/src/controls/mediavolume.h"
#include "modules/media/src/coremediaplayer.h"
#include "modules/media/src/mediasource.h"
#include "modules/pi/OpMediaPlayer.h"
#include "modules/skin/OpSkinManager.h"

MediaModule::MediaModule()
	: m_media_source_manager(NULL)
	, m_op_media_manager(NULL)
	, m_volume_skin_count(0)
#ifdef MEDIA_CAMERA_SUPPORT
	, m_camera_recorder(NULL)
#endif // MEDIA_CAMERA_SUPPORT
{
}

void MediaModule::InitL(const OperaInitInfo& info)
{
	m_media_source_manager = OP_NEW_L(MediaSourceManagerImpl, ());
}

void MediaModule::Destroy()
{
#ifdef MEDIA_CAMERA_SUPPORT
	OP_DELETE(m_camera_recorder);
	m_camera_recorder = NULL;
#endif // MEDIA_CAMERA_SUPPORT
	OP_DELETE(m_media_source_manager);
	m_media_source_manager = NULL;
	OP_DELETE(m_op_media_manager);
	m_op_media_manager = NULL;
}

OP_STATUS
MediaModule::CanPlayType(BOOL3* result, const char* type_and_codecs)
{
	OP_ASSERT(result);
	OP_STATUS status = OpStatus::OK;

	*result = NO;

	// to be passed to OpMediaManager::CanPlayType
	OpStringC8 type;
	OpVector<OpStringC8> codecs;

	ParameterList params;

	RETURN_IF_ERROR(params.SetValue(type_and_codecs,
		PARAM_SEP_SEMICOLON | PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES | PARAM_BORROW_CONTENT));

	// first "parameter" should be the media type, e.g. "video/ogg"
	Parameters* param = params.First();
	if (!param)
		return status; // NO

	if (param->AssignedValue())
		return status; // NO (something like "video/ogg=foo")

	// verify that this is at least potentially a valid media type
	// (Firefox/Safari/Chrome don't accept uppercase, so neither do we)
	{
		const char* media_type = param->Name();

		BOOL slash = FALSE; // found slash?
		for (int i = 0; media_type[i] != '\0'; i++)
		{
			const char c = media_type[i];
			if (c == '/')
			{
				// just a single slash, not first or last
				if (slash || i == 0 || media_type[i+1] == '\0')
					return status; // NO
				slash = TRUE;
			}
			else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
					 c == '+' || c == '-' || c == '.'))
			{
				return status; // NO
			}
		}
		if (!slash)
			return status; // NO

		// validated OK
		type = param->GetName();
	}

	// build codecs list
	param = params.GetParameter("codecs", param);
	if (param && param->AssignedValue())
	{
		ParameterList* codec_list = param->GetParameters(PARAM_SEP_COMMA | PARAM_ONLY_SEP | PARAM_NO_ASSIGN | PARAM_BORROW_CONTENT);
		Parameters* codec = codec_list->First();
		while (codec)
		{
			OpStringC8* codec_str = OP_NEW(OpStringC8, (codec->Name()));
			status = codec_str ? codecs.Add(codec_str) : OpStatus::ERR_NO_MEMORY;
			if (OpStatus::IsError(status))
				break;
			codec = codec->Suc();
		}
	}

	// call OpMediaManager to actually get the answer
	if (OpStatus::IsSuccess(status))
	{
		status = g_media_module.EnsureOpMediaManager();
		if (OpStatus::IsSuccess(status))
			*result  = g_op_media_manager->CanPlayType(type, codecs);
	}

	codecs.DeleteAll();

	return status;
}

OP_STATUS
MediaModule::CanPlayType(BOOL3* result, const uni_char* type_and_codecs)
{
	OpString8 type_and_codecs_utf8;
	RETURN_IF_ERROR(type_and_codecs_utf8.SetUTF8FromUTF16(type_and_codecs));
	return CanPlayType(result, type_and_codecs_utf8);
}

OP_STATUS
MediaModule::EnsureOpMediaManager()
{
	if (!m_op_media_manager)
	{
		RETURN_IF_ERROR(OpMediaManager::Create(&m_op_media_manager));
		OP_ASSERT(m_op_media_manager);
	}
	return OpStatus::OK;
}

BOOL MediaModule::IsMediaPlayableUrl(const URL& url)
{
	const OpStringC8& type = url.GetAttribute(URL::KMIME_Type);
	switch (url.ContentType())
	{
	case URL_AVI_CONTENT:
	case URL_MEDIA_CONTENT:
	case URL_MIDI_CONTENT:
	case URL_MP4_CONTENT:
	case URL_MPG_CONTENT:
	case URL_WAV_CONTENT:
		break;
	case URL_UNKNOWN_CONTENT:
	case URL_UNDETERMINED_CONTENT:
		// Only consider unknown audio/* and video/* types.
		if (type.Compare("audio/", 6) == 0 || type.Compare("video/", 6) == 0)
			break;
	default:
		return FALSE;
	}

	BOOL3 playable = NO;
	RAISE_IF_MEMORY_ERROR(CanPlayType(&playable, type.CStr()));
	return playable != NO;
}

BOOL
MediaModule::IsAssociatedWithVideo(OpMediaHandle handle)
{
	// Theoretically OpMediaHandle could also be a different type of
	// MediaPlayer but it is only CoreMediaPlayer instances for which
	// using this method makes sense. Having a different type of player
	// here would indicate a serious bug.
	CoreMediaPlayer* coremediaplayer = reinterpret_cast<CoreMediaPlayer*>(handle);
	return coremediaplayer->IsAssociatedWithVideo();
}

unsigned
MediaModule::GetVolumeSkinCount()
{
	if (m_volume_skin_count == 0)
	{
		VolumeString volstr;
		while (m_volume_skin_count <= 100)
		{
			if (g_skin_manager->GetSkinElement(volstr.Get(m_volume_skin_count)) != NULL)
				m_volume_skin_count++;
			else
				break;
		}
		// Expect to find between 2 and 101 parts.
		OP_ASSERT(m_volume_skin_count >= 2 && m_volume_skin_count <= 101);
	}
	return  m_volume_skin_count;
}

#ifdef MEDIA_CAMERA_SUPPORT
OP_STATUS
MediaModule::GetCameraRecorder(CameraRecorder*& camera_recorder)
{
	if (!m_camera_recorder)
	{
		RETURN_IF_ERROR(CameraRecorder::Create(m_camera_recorder));
		OP_ASSERT(m_camera_recorder);
	}
	camera_recorder = m_camera_recorder;
	return OpStatus::OK;
}
#endif // MEDIA_CAMERA_SUPPORT

#endif // MEDIA_PLAYER_SUPPORT
