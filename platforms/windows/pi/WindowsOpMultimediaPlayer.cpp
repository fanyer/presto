/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** $Id: 
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#include "WindowsOpMultimediaPlayer.h"

#include "modules/logdoc/logdoc_util.h"

#include "platforms/windows/pi/WindowsOpView.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/CustomWindowMessages.h"
#include "platforms/windows/windows_ui/menubar.h"

#include <digitalv.h>
#include <uuids.h>

// Static members
HWND WindowsOpMultimediaPlayer::hwnd = NULL;
UINT32 WindowsOpMultimediaPlayer::hwnd_count = 0;
static Head media_list;
static uni_char VideoTypeAvi[] = UNI_L("AVIVideo");
static uni_char VideoTypeMpg[] = UNI_L("MpegVideo");

OP_STATUS OpMultimediaPlayer::Create(OpMultimediaPlayer** new_opmultimediaplayer)
{
	OP_ASSERT(new_opmultimediaplayer != NULL);
	*new_opmultimediaplayer = new WindowsOpMultimediaPlayer();
	if(*new_opmultimediaplayer == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = ((WindowsOpMultimediaPlayer*)*new_opmultimediaplayer)->Init();
	if(OpStatus::IsError(status))
	{
		delete *new_opmultimediaplayer;
		*new_opmultimediaplayer = NULL;
	}
	return status;
}

static long FAR PASCAL
MultimediaPlayerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT wDeviceID	= LOWORD(lParam);
	UINT param		= wParam;
	
	// Find media element beloning to the received message
	WinMediaElement* media = (WinMediaElement*) media_list.First();
    while (media && media->device != wDeviceID)
    	media = media->Suc();
	
	// Media element found?
    if (media)
    {
		switch (param)
		{
		case MCI_NOTIFY_SUCCESSFUL:

			if (media->loop != LoopInfinite && media->loop > 0)
				media->loop--;				
			
			if (media->loop > 0)
			{				
				MCI_PLAY_PARMS mciPlayParms;

				mciPlayParms.dwCallback = (DWORD_PTR) media->player->GetWindowHandle();
				mciPlayParms.dwFrom = 0;

				if (mciSendCommand(wDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM,
								  (DWORD_PTR)(LPVOID) &mciPlayParms))
				{
					// don't send error message from within loop - just terminate
				}
				else
					break;
			}
			// else pass through and close ...

		case MCI_NOTIFY_SUPERSEDED:
		case MCI_NOTIFY_FAILURE:
		case MCI_NOTIFY_ABORTED:

			mciSendCommand(wDeviceID, MCI_CLOSE, 0, NULL);
			if (media->hwnd)
				DestroyWindow(media->hwnd);

			media->Out();
			delete media;

			// FIXME: doc_manager
			/*			{
						Document* doc = doc_manager->GetCurrentDoc();
						if (doc && doc->Type() == DOC_VIDEO)
						doc_manager->GetWindow()->SetHistoryPrev(FALSE);
						}
			*/
			break;
		default:
			break;
		}
	} 

	return DefWindowProc(hWnd, message,  wParam, lParam);
}

// WindowsOpMultiMediaPlayer

WindowsOpMultimediaPlayer::WindowsOpMultimediaPlayer() :
	m_listener(NULL),
	m_graph_builder(NULL),
	m_media_control(NULL),
	m_media_event(NULL)
{
}

WindowsOpMultimediaPlayer::~WindowsOpMultimediaPlayer()
{
	ReleaseMedia();
	StopAll();

	if (hwnd)
	{
		hwnd_count--;
		if (hwnd_count == 0)
		{
			UnregisterClass(UNI_L("MultimediaPlayer"), NULL);
			DestroyWindow(hwnd);
			hwnd = NULL;
		}		
	}
}

OP_STATUS WindowsOpMultimediaPlayer::Init()
{
	if (!hwnd)
	{
		hwnd = CreateWindowHandle();
		if (!hwnd)
			return OpStatus::ERR;
	}
	
	hwnd_count++;
	return OpStatus::OK;
}

void WindowsOpMultimediaPlayer::Stop(UINT32 id)
{
	WinMediaElement* media = (WinMediaElement*) media_list.First();
	while (media)
	{
		if (media->player == this && media->device == id)
		{
			mciSendCommand(media->device, MCI_CLOSE, 0, NULL);

			if (media->hwnd)
				DestroyWindow(media->hwnd);

			media->Out();
			delete media;

			return;
		}

		media = (WinMediaElement*) media->Suc();
	}
}

void WindowsOpMultimediaPlayer::StopAll()
{
	BOOL media_closed = FALSE;
	WinMediaElement* media = (WinMediaElement*) media_list.First();
	WinMediaElement* delMedia;
	while (media)
	{
		delMedia = NULL;
		if (media->player == this)
		{			
			media_closed = TRUE;
			mciSendCommand(media->device, MCI_CLOSE, 0, NULL);

			if (media->hwnd)
				DestroyWindow(media->hwnd);

			delMedia = media;
		}

		media = (WinMediaElement*) media->Suc();
		
		// Did we find something to delete ?
		if (delMedia)
		{
			delMedia->Out();
			delete delMedia;
		}
	}

	// FIXME: doc_manager
	/*
	if (video_closed && doc_manager)
	{
		Document* doc = doc_manager->GetCurrentDoc();
		if (doc && doc->Type() == DOC_VIDEO)
			doc_manager->GetWindow()->SetHistoryPrev(FALSE);
	}
	*/
}

HWND WindowsOpMultimediaPlayer::CreateWindowHandle()
{
	OP_NEW_DBG("WindowsOpMultimediaPlayer::CreateWindowHandle", "media");

	WNDCLASS WndClass;
	WndClass.style         = 0;
	WndClass.lpfnWndProc   = (WNDPROC)MultimediaPlayerWndProc;
	WndClass.cbClsExtra    = 0;
	WndClass.cbWndExtra    = 0;
	WndClass.hInstance     = NULL;
	WndClass.hIcon         = 0;
	WndClass.hCursor       = 0;
	WndClass.hbrBackground = 0;
	WndClass.lpszMenuName  = NULL;
	WndClass.lpszClassName = UNI_L("MultimediaPlayer");

	UnregisterClass(UNI_L("MultimediaPlayer"), NULL);
	if (!RegisterClass(&WndClass))
		return NULL;
		
	HWND new_hwnd = ::CreateWindow(UNI_L("MultimediaPlayer"),
								   NULL,            //  window caption
								   NULL,
								   0,        		//  x position
								   0,        		//  y position
								   0,        		//  width
								   0,        		//  iheight
								   NULL ,           //  parent handle
								   0,               //  menu or child ID
								   NULL,            //  instance
								   NULL);           //  additional info

	return new_hwnd;	
}

void WindowsOpMultimediaPlayer::SetMediaStarted(HWND media_hwnd, UINT wDeviceID, int loop)
{
    WinMediaElement* media = new WinMediaElement(this, media_hwnd, wDeviceID, loop);
	if (media == NULL)
	{
		mciSendCommand(wDeviceID, MCI_CLOSE, 0, NULL);

		if (media_hwnd)
			DestroyWindow(media_hwnd);
		return;
	}

    media->Into(&media_list);
}

OP_STATUS WindowsOpMultimediaPlayer::LoadMedia(const uni_char* filename)
{
	ReleaseMedia();

	HRESULT hresult;

	hresult = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&m_graph_builder);

	if (FAILED(hresult))
	{
		ReleaseMedia();
		return OpStatus::ERR;
	}

	m_graph_builder->QueryInterface(IID_IMediaControl, (void **)&m_media_control);

	if (!m_media_control)
	{
		ReleaseMedia();
		return OpStatus::ERR;
	}

	m_graph_builder->QueryInterface(IID_IMediaEventEx, (void **)&m_media_event);

	if (!m_media_event)
	{
		ReleaseMedia();
		return OpStatus::ERR;
	}

	hresult = m_graph_builder->RenderFile(filename, NULL);

	if (FAILED(hresult))
	{
		ReleaseMedia();
		return OpStatus::ERR;
	}

	m_media_event->SetNotifyWindow((OAHWND)hwnd, WM_OPERA_MEDIA_EVENT, 0);

	return OpStatus::OK;
}

OP_STATUS WindowsOpMultimediaPlayer::PlayMedia()
{
	if (!m_media_control)
		return OpStatus::ERR;

	HRESULT hresult = m_media_control->Run();

	if (FAILED(hresult))
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS WindowsOpMultimediaPlayer::PauseMedia()
{
	if (!m_media_control)
		return OpStatus::ERR;

	HRESULT hresult = m_media_control->Pause();

	if (FAILED(hresult))
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS WindowsOpMultimediaPlayer::StopMedia()
{
	if (!m_media_control)
		return OpStatus::ERR;

	HRESULT hresult = m_media_control->Stop();

	if (FAILED(hresult))
		return OpStatus::ERR;

	return OpStatus::OK;
}

void WindowsOpMultimediaPlayer::ReleaseMedia()
{
	if (m_media_control)
		m_media_control->Release();

	if (m_media_event)
		m_media_event->Release();

	if (m_media_event)
		m_media_event->Release();

	m_graph_builder = NULL;
	m_media_control = NULL;
	m_media_event = NULL;
}

