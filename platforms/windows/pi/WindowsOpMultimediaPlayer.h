/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** $Id: 
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_OPMULTIMEDIAPLAYER_H
#define WINDOWS_OPMULTIMEDIAPLAYER_H

#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"

#if defined _MSC_VER && _MSC_VER > VS6
#include "platforms/windows/multimedia-player/strmif.h"
#else
#include <amstream.h>
#endif

#include <control.h>

class WindowsOpMultimediaPlayer;

class WinMediaElement : public Link
{
public:
	WindowsOpMultimediaPlayer*	player;
	HWND						hwnd;
	UINT						device;
	int							loop;
	
	WinMediaElement(WindowsOpMultimediaPlayer* player, HWND mHwnd, UINT mDev, int nloop) :
		player(player),
		hwnd(mHwnd),
		device(mDev),
		loop(nloop) {}
	
	WinMediaElement*		Suc() { return (WinMediaElement*) Link::Suc(); }
};


class WindowsOpMultimediaPlayer : public OpMultimediaPlayer
{
public:	

	WindowsOpMultimediaPlayer();
	virtual ~WindowsOpMultimediaPlayer();

	OP_STATUS Init();

	void Stop(UINT32 id);

	void StopAll();

	HWND GetWindowHandle() { return hwnd; };

	OP_STATUS LoadMedia(const uni_char* filename);
	OP_STATUS PlayMedia();
	OP_STATUS PauseMedia();
	OP_STATUS StopMedia();

	void SetMultimediaListener(OpMultimediaListener* listener) {m_listener = listener;}

private:

	void ReleaseMedia();

	HWND CreateWindowHandle();
		
	void SetMediaStarted(HWND media_hwnd, UINT wDeviceID, BOOL loop);
		
	static HWND hwnd;
	static UINT32 hwnd_count;

	OpMultimediaListener* m_listener;
	IGraphBuilder* m_graph_builder;
	IMediaControl* m_media_control;
	IMediaEventEx* m_media_event;
};

#endif // WINDOWS_OPMULTIMEDIAPLAYER_H
