/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GSTMEDIAMANAGER_H
#define GSTMEDIAMANAGER_H

#ifdef MEDIA_BACKEND_GSTREAMER

#include "modules/pi/OpMediaPlayer.h"
#include "platforms/media_backends/gst/gstlibs.h"
#include "platforms/media_backends/gst/gstmediaplayer.h"

/** Map a Content-Type to a GstCaps type.
 * @param type A Content-Type with no parameters, e.g. "video/webm".
 * @return A GStreamer caps type, or NULL if type is unknown. */
const char* GstMapType(const char* type);

class GstMediaManager :
	public OpMediaManager
{
public:
	virtual ~GstMediaManager() {}
	virtual BOOL3 CanPlayType(const OpStringC8& type, const OpVector<OpStringC8>& codecs);
	virtual BOOL CanPlayURL(const uni_char* url);
	virtual OP_STATUS CreatePlayer(OpMediaPlayer** player, OpMediaHandle handle);
	virtual OP_STATUS CreatePlayer(OpMediaPlayer** player, OpMediaHandle handle, const uni_char* url);
	virtual void DestroyPlayer(OpMediaPlayer* player);
};

class GstThreadManager :
	private MessageObject
{
public:
	static OP_STATUS Init();

	virtual ~GstThreadManager();

	/** Queue a thread to be run as soon as possible. */
	void Queue(GstMediaPlayer* player);
	/** Stop a running thread. */
	void Stop(GstMediaPlayer* player);

private:
	/** Start as many pending threads as possible and keep the rest in
	 *  queue for later. */
	void StartPending();
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	CountedList<GstMediaPlayer> m_running;
	CountedList<GstMediaPlayer> m_pending;
};

#endif // MEDIA_BACKEND_GSTREAMER

#endif // GSTMEDIAMANAGER_H
