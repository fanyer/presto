/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_BACKENDS_MODULE_H
#define MEDIA_BACKENDS_MODULE_H

#ifdef MEDIA_BACKEND_GSTREAMER

#include "modules/hardcore/opera/module.h"

class OpDLL;
class GstThreadManager;
class GstPluginManager;

class MediaBackendsModule : public OperaModule
{
public:
    MediaBackendsModule();

    void InitL(const OperaInitInfo& info);
    void Destroy();

	GstThreadManager *thread_manager;

#ifdef MEDIA_BACKEND_GSTREAMER_USE_OPDLL

	// created by GstMediaManager, held here for cleanup
#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	// Single GStreamer DLL
	OpDLL *dll_LibGStreamer;
#else // ! MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	// Use system GStreamer (UNIX)
	OpDLL *dll_LibGLib;
	OpDLL *dll_LibGObject;
	OpDLL *dll_LibGModule;
	OpDLL *dll_LibGStreamer;
	OpDLL *dll_LibGstBase;
	OpDLL *dll_LibGstVideo;
	OpDLL *dll_LibGstRiff;
#endif //  MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
#endif // MEDIA_BACKEND_GSTREAMER_USE_OPDLL
};

#define g_gst_thread_manager (g_opera->media_backends_module.thread_manager)

#define MEDIA_BACKENDS_MODULE_REQUIRED

#endif // MEDIA_BACKEND_GSTREAMER

#endif // MEDIA_BACKENDS_MODULE_H
