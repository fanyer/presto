/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_DESKTOP_GLOBAL_APPLICATION_H
#define WINDOWS_DESKTOP_GLOBAL_APPLICATION_H

#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "platforms/windows/pi/WindowsOpTaskbar.h"

// Implementation of platform hooks for events that affect the Opera application
class WindowsDesktopGlobalApplication : public DesktopGlobalApplication
{

    public:

        WindowsDesktopGlobalApplication();
        ~WindowsDesktopGlobalApplication();

		//implementing DesktopGlobalApplication
		virtual void OnStart();
		virtual void Exit();
		virtual void ExitStarted();
		virtual BOOL OnConfirmExit(BOOL /*exit*/) {return FALSE;}

		virtual void OnHide();
		virtual void OnShow();

		WindowsOpTaskbar *GetTaskbar() { return m_taskbar; }
		
	private:
	
		WindowsOpTaskbar* m_taskbar;
};

#endif //WINDOWS_DESKTOP_GLOBAL_APPLICATION_H