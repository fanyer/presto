/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** $Id$
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "WindowsOpScreenInfo.h"
#include "WindowsOpWindow.h"

OP_STATUS OpScreenInfo::Create(OpScreenInfo** new_opscreeninfo)
{
	OP_ASSERT(new_opscreeninfo != NULL);
	*new_opscreeninfo = new WindowsOpScreenInfo();
	if(*new_opscreeninfo == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = ((WindowsOpScreenInfo*)*new_opscreeninfo)->Init();
	if(OpStatus::IsError(status))
	{
		delete *new_opscreeninfo;
		*new_opscreeninfo = NULL;
	}
	return status;
}

WindowsOpScreenInfo::WindowsOpScreenInfo() 
{
	m_screenDC = CreateCompatibleDC(NULL);
}

OP_STATUS WindowsOpScreenInfo::Init()
{
	return OpStatus::OK;
}

WindowsOpScreenInfo::~WindowsOpScreenInfo()
{
	if (m_screenDC)
	{
		DeleteDC(m_screenDC);
	}
}

OP_STATUS WindowsOpScreenInfo::GetProperties(OpScreenProperties* properties, OpWindow* window, const OpPoint* point)
{
	if (window)
	{
		HMONITOR hMon = NULL;
		if (window->GetStyle() == OpWindow::STYLE_BITMAP_WINDOW)
		{
			POINT pt;
			pt.x = 0;
			pt.y = 0;
			hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
		}
		else
		{
			if (point)
			{
				POINT pt;
				pt.x = point->x;
				pt.y = point->y;
				hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
			}
			else
			{
				HWND wnd = static_cast<WindowsOpWindow*>(window)->GetParentHWND();
				hMon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
			}
		}

		if( hMon )
		{
			MONITORINFOEX m;
			m.cbSize = sizeof( m );
			if( GetMonitorInfo( hMon, &m ) )
			{
				properties->workspace_rect.Set(m.rcWork.left, m.rcWork.top, m.rcWork.right-m.rcWork.left, m.rcWork.bottom-m.rcWork.top);
				properties->screen_rect.Set(m.rcMonitor.left, m.rcMonitor.top, m.rcMonitor.right-m.rcMonitor.left, m.rcMonitor.bottom-m.rcMonitor.top);
				properties->width = m.rcWork.right-m.rcWork.left;
				properties->height = m.rcWork.bottom-m.rcWork.top;

				HDC monitorDC = CreateDC(m.szDevice, NULL, NULL, NULL);

				//TODO: find monitor specific settings if possible
				properties->vertical_dpi = GetDeviceCaps(monitorDC, LOGPIXELSY);
				properties->horizontal_dpi = GetDeviceCaps(monitorDC, LOGPIXELSX);
				properties->bits_per_pixel = GetDeviceCaps(monitorDC, BITSPIXEL);
				properties->number_of_bitplanes = GetDeviceCaps(monitorDC, PLANES);

				DeleteDC(monitorDC);
			}
		}
	}
	else
	{
		properties->width = GetDeviceCaps(m_screenDC, HORZRES);
		properties->height = GetDeviceCaps(m_screenDC, VERTRES);
		properties->vertical_dpi = GetDeviceCaps(m_screenDC, LOGPIXELSY);
		properties->horizontal_dpi = GetDeviceCaps(m_screenDC, LOGPIXELSX);
		properties->bits_per_pixel = GetDeviceCaps(m_screenDC, BITSPIXEL);
		properties->number_of_bitplanes = GetDeviceCaps(m_screenDC, PLANES);
		properties->screen_rect.Set(0, 0, properties->width, properties->height);
		properties->workspace_rect.Set(0, 0, properties->width, properties->height);
	}
	return OpStatus::OK;
}

/** Get the DPI properties only (superfast)*/ 
OP_STATUS WindowsOpScreenInfo::GetDPI(UINT32* horizontal, UINT32* vertical, OpWindow* window, const OpPoint* point)
{
	*horizontal = GetDeviceCaps(m_screenDC, LOGPIXELSX);
	*vertical = GetDeviceCaps(m_screenDC, LOGPIXELSY);
	return OpStatus::OK;
}

OP_STATUS WindowsOpScreenInfo::RegisterMainScreenProperties(OpScreenProperties& properties){ 
    // not needed on windows
	return OpStatus::OK;
} 

UINT32 WindowsOpScreenInfo::GetBitmapAllocationSize(UINT32 width, UINT32 height,
										   BOOL transparent,
										   BOOL alpha,
										   INT32 indexed)
{
	return width*height*4;
}

void WindowsOpScreenInfo::OnSettingsChanged()
{
	if (m_screenDC)
	{
		DeleteDC(m_screenDC);
	}

	m_screenDC = CreateCompatibleDC(NULL);
}

