/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/


#ifndef _X11_SYSTEM_TRAY_ICON_H_
#define _X11_SYSTEM_TRAY_ICON_H_

#include "x11_widget.h"
#include "platforms/unix/base/x11/x11_optaskbar.h"

class X11SystemTrayIcon : public X11Widget, public X11OpTaskbarListener
{
public:
	enum MonitorType
	{
		UnreadMail,
		UnattendedMail,
		UnattendedChat,
		UniteCount,
		InitIcon // Only for internal use
	};

public:
	static OP_STATUS Create(X11SystemTrayIcon** icon);

public:
	X11SystemTrayIcon();
	virtual ~X11SystemTrayIcon();

	void UpdateIcon(MonitorType type, int count);
	void Repaint(const OpRect& rect);

	bool HandleSystemEvent(XEvent* event);
	bool HandleEvent(XEvent* event);

	UINT32 GetBackgroundColor();

	// X11OpTaskbarListener
	void OnUnattendedMailCount( UINT32 count );
	void OnUnreadMailCount( UINT32 count );
	void OnUnattendedChatCount( OpWindow* op_window, UINT32 count );
	void OnUniteAttentionCount( UINT32 count );

private:
	void UpdateSize(int width, int height);
	void Setup();
	void EnterTray();
	void DetectTrayWindow();
	void DetectTrayVisual();

private:
	X11Types::Window m_tray_window;
	X11Types::Atom   m_tray_selection;
	XVisualInfo m_tray_visual;
	INT32 m_unread_mail_count;
	INT32 m_unattended_mail_count;
	INT32 m_unattended_chat_count;
	INT32 m_unite_count;
	INT32 m_tray_depth;
	INT32 m_tray_width;
	INT32 m_tray_height;
	UINT32 m_width;
	UINT32 m_height;
	BOOL m_has_painted;
};


#endif
