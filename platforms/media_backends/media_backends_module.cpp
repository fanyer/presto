/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch_system_includes.h"

#ifdef MEDIA_BACKEND_GSTREAMER


#include "modules/pi/OpDLL.h"
#include "platforms/media_backends/gst/gstmediamanager.h"

MediaBackendsModule::MediaBackendsModule() :
	  thread_manager(NULL)
#ifdef MEDIA_BACKEND_GSTREAMER_USE_OPDLL
#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	, dll_LibGStreamer(NULL)
#else // !MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	, dll_LibGLib(NULL)
	, dll_LibGObject(NULL)
	, dll_LibGModule(NULL)
	, dll_LibGStreamer(NULL)
	, dll_LibGstBase(NULL)
	, dll_LibGstVideo(NULL)
	, dll_LibGstRiff(NULL)
#endif // MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
#endif // MEDIA_BACKEND_GSTREAMER_USE_OPDLL
{
}

void MediaBackendsModule::InitL(const OperaInitInfo& info)
{
}

void MediaBackendsModule::Destroy()
{
#ifdef MEDIA_BACKEND_GSTREAMER_USE_OPDLL
#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	OP_DELETE(dll_LibGStreamer);
#else // !MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	OP_DELETE(dll_LibGLib);
	OP_DELETE(dll_LibGObject);
	OP_DELETE(dll_LibGModule);
	OP_DELETE(dll_LibGStreamer);
	OP_DELETE(dll_LibGstBase);
	OP_DELETE(dll_LibGstVideo);
	OP_DELETE(dll_LibGstRiff);
#endif // MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
#endif // MEDIA_BACKEND_GSTREAMER_USE_OPDLL

	OP_DELETE(thread_manager);
}


#endif // MEDIA_BACKEND_GSTREAMER
