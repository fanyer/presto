/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @mainpage Media API
 *
 * This is the auto-generated API documentation for the Media module.
 * For more information about the module, see the module's <a
 * href="https://wiki.oslo.opera.com/developerwiki/Modules/media">wiki
 * page</a>.
 */

#ifndef MEDIA_MEDIA_MODULE_H
#define MEDIA_MEDIA_MODULE_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/hardcore/opera/module.h"
#include "modules/dom/domeventtypes.h"
#include "modules/pi/OpMediaPlayer.h"
#include "modules/util/OpHashTable.h"

class OpMediaManager;
class MediaSourceManager;
class CameraRecorder;

class MediaModule : public OperaModule
{
public:
	MediaModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	OP_STATUS CanPlayType(BOOL3* result, const char* type_and_codecs);
	OP_STATUS CanPlayType(BOOL3* result, const uni_char* type_and_codecs);

	OP_STATUS EnsureOpMediaManager();

	BOOL IsMediaPlayableUrl(const URL& url);

	/** Is this OpMediaHandle associated with a video context?
	 *
	 * @param[in] handle Handle to the core media resource.
	 * @return TRUE if the handle is associated with a video context
	 * (e.g. HTML/SVG <video>), FALSE otherwise.
	 */
	BOOL IsAssociatedWithVideo(OpMediaHandle handle);

	/** Find the number of available volume skin parts.
	 *
	 * @return The number of "Media Volume n" skin parts available,
	 *         at most 101 parts: 0-100. The result is cached.
	 */
	unsigned GetVolumeSkinCount();

#ifdef MEDIA_CAMERA_SUPPORT
	OP_STATUS GetCameraRecorder(CameraRecorder*& camera_recorder);
#endif // MEDIA_CAMERA_SUPPORT

	MediaSourceManager* m_media_source_manager;
	OpMediaManager* m_op_media_manager;

private:
	unsigned m_volume_skin_count;
#ifdef MEDIA_CAMERA_SUPPORT
	CameraRecorder* m_camera_recorder;
#endif // MEDIA_CAMERA_SUPPORT
};

#define g_media_module (g_opera->media_module)
#define g_media_source_manager (g_media_module.m_media_source_manager)
#define g_op_media_manager (g_media_module.m_op_media_manager)

#define MEDIA_MODULE_REQUIRED

#endif // MEDIA_PLAYER_SUPPORT
#endif // MEDIA_MEDIA_MODULE_H
