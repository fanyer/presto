/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_systemtrayicon.h"

#include "platforms/unix/base/x11/x11_mdebuffer.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/unix/product/x11quick/iconsource.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/ui/OpUiInfo.h" // Background color
#include "platforms/quix/environment/DesktopEnvironment.h"
#include "adjunct/desktop_util/image_utils/iconutils.h"
#include "adjunct/quick/Application.h" // For what to do on mouse clicks
#include "adjunct/quick/menus/DesktopMenuHandler.h"

using X11Types::Atom;

#define SYSTEM_TRAY_CHAT_BG 0xFF3597FC // Solid orange color
UINT32 ConvertToX11Color(UINT32 color) { return color&0xFF00FF00 | ((color&0x000000FF)<<16) | ((color&0x00FF0000)>>16); }

#define SYSTEM_TRAY_REQUEST_DOCK 0
#define SYSTEM_TRAY_WIDTH  22
#define SYSTEM_TRAY_HEIGHT 22


//static 
OP_STATUS X11SystemTrayIcon::Create(X11SystemTrayIcon** icon)
{
	*icon = OP_NEW(X11SystemTrayIcon, ());
	if( !*icon )
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		OP_STATUS rc = (*icon)->Init(0, "trayicon", X11Widget::BYPASS_WM);
		if( OpStatus::IsError(rc) )
		{
			OP_DELETE(*icon);
			return rc;
		}
		(*icon)->Setup();
		
	}

	return OpStatus::OK;
}


X11SystemTrayIcon::X11SystemTrayIcon()
	:m_tray_window(None)
	,m_tray_selection(None)
	,m_unread_mail_count(0)
	,m_unattended_mail_count(0)
	,m_unattended_chat_count(0)
	,m_unite_count(0)
	,m_tray_depth(0)
	,m_tray_width(0)
	,m_tray_height(0)
	,m_width(0)
	,m_height(0)
{
	m_tray_visual.visual = 0;
}


X11SystemTrayIcon::~X11SystemTrayIcon()
{
}


void X11SystemTrayIcon::OnUnattendedMailCount( UINT32 count )
{
	UpdateIcon(UnattendedMail, count);
}

void X11SystemTrayIcon::OnUnreadMailCount( UINT32 count )
{
	UpdateIcon(UnreadMail, count);
}


void X11SystemTrayIcon::OnUnattendedChatCount( OpWindow* op_window, UINT32 count )
{
	UpdateIcon(UnattendedChat, count);
}


void X11SystemTrayIcon::OnUniteAttentionCount( UINT32 count )
{
	UpdateIcon(UniteCount, count);
}

void X11SystemTrayIcon::UpdateIcon(MonitorType type, int count)
{
	OpString text;

	if (type == UnreadMail)
	{
		if (m_unread_mail_count == count)
			return;
		m_unread_mail_count = count;
	}
	else if (type == UnattendedMail)
	{
		if (m_unattended_mail_count == count)
			return;
		m_unattended_mail_count = count;
		if (m_unattended_mail_count > 0)
		{
			text.Set("Opera");
			if (m_unattended_mail_count > 1)
			{
				OpString tmp;
				TRAPD(err, g_languageManager->GetStringL(Str::S_UNREAD_MESSAGES, tmp));
				if (tmp.CStr())
				{
					// "%d unread messages"
					text.Append("\n");
					text.AppendFormat(tmp.CStr(), m_unattended_mail_count );
				}
			}
			else if (m_unattended_mail_count == 1)
			{
				OpString tmp;
				TRAPD(err, g_languageManager->GetStringL(Str::S_ONE_UNREAD_MESSAGE, tmp));
				if (tmp.CStr())
				{
					// "1 unread message"
					text.Append("\n");
					text.Append(tmp);
				}
			}
		}
	}
	else if (type == UnattendedChat)
	{
		if (m_unattended_chat_count == count)
			return;
		m_unattended_chat_count = count;
	}
	else if (type == UniteCount)
	{
		if (m_unite_count == count)
			return;
		m_unite_count = count;
	}
	else if (type == InitIcon)
	{
		// No test
	}
	else
	{
		return;
	}

	UINT8* buffer;
	UINT32 size;

	UINT32 unit;
	if (m_height < 32)
		unit = 16;
	else if(m_height < 48)
		unit = 32;
	else
		unit = 48;

	if (GetMdeBuffer() && IconSource::GetSystemTrayOperaIcon(unit, buffer, size))
	{
		OpBitmap* bitmap = 0;
		if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, unit, unit, TRUE, TRUE, 0, 0, TRUE)))
		{
			OpPoint p(0,0);
			OpRect r(0,0,bitmap->Width(),bitmap->Height());

			// Init background. Needed for 24-bit visuals
			UINT32* data = (UINT32*)bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
			if (data)
			{
				UINT32 c = GetBackgroundColor();
				UINT32 col = (OP_GET_A_VALUE(c) << 24) | (OP_GET_R_VALUE(c) << 16) | (OP_GET_G_VALUE(c) << 8) | OP_GET_B_VALUE(c);
				UINT32 size = r.width*r.height;
				for (UINT32 i=0; i<size; i++ )
					data[i] = col;
				bitmap->ReleasePointer(TRUE);
			}

			OpPainter* painter = bitmap->GetPainter();
			if (painter)
			{
				// Opera icon
				OpBitmap* bm = IconUtils::GetBitmap(buffer, size, -1, -1);
				if (bm)
				{
					painter->DrawBitmapClipped(bm, r, p);
					OP_DELETE(bm);
				}
				// Mail overlay
				if (m_unread_mail_count > 0 && IconSource::GetSystemTrayMailIcon(unit, buffer, size))
				{
					bm = IconUtils::GetBitmap(buffer, size, -1, -1);
					if (bm)
					{
						painter->DrawBitmapClipped(bm, r, p);
						OP_DELETE(bm);
					}
				}
				// Chat overlay
				if (m_unattended_chat_count > 0 && IconSource::GetSystemTrayChatIcon(unit, buffer, size))
				{
					bm = IconUtils::GetBitmap(buffer, size, -1, -1);
					if (bm)
					{
						painter->DrawBitmapClipped(bm, r, p);
						OP_DELETE(bm);
					}
				}
				// Unite overlay.
				// Note. We can not show chat and unite status at the same time (status gfx. occupy the same area)
				else if (m_unite_count > 0 && IconSource::GetSystemTrayUniteIcon(unit, buffer, size))
				{
					bm = IconUtils::GetBitmap(buffer, size, -1, -1);
					if (bm)
					{
						painter->DrawBitmapClipped(bm, r, p);
						OP_DELETE(bm);
					}
				}
			}
			GetMdeBuffer()->SetBitmap(*bitmap);
			OP_DELETE(bitmap);
		}
	}

	UpdateAll();
}


UINT32 X11SystemTrayIcon::GetBackgroundColor()
{
	UINT32 color;

	if (m_tray_visual.visual && m_tray_visual.depth == 32)
		color = 0x00000000;
	else
		color = 0xFF000000 | g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON);

	// Special test for chat mode.
	if (m_unattended_chat_count > 0)
	{
		const char* env = op_getenv("OPERA_CHAT_NOTIFICATION_HIGHLIGHT");
		if (env)
			color = SYSTEM_TRAY_CHAT_BG;
	}

	return color;
}


void X11SystemTrayIcon::Repaint(const OpRect& rect)
{
	if (GetMdeBuffer())
	{
		X11Types::Display* display = GetDisplay();
		GC gc = XCreateGC(display, GetWindowHandle(), 0, 0);
		XSetForeground(display, gc, ConvertToX11Color(GetBackgroundColor()));
		XFillRectangle(display, GetWindowHandle(), gc, rect.x, rect.y, rect.width, rect.height);
		XFreeGC(display, gc);

		// The buffer can be smaller than the window. Center it
		int x = (int)(((float)(m_width-GetMdeBuffer()->GetWidth())/2.0)+0.5);
		int y = (int)(((float)(m_height-GetMdeBuffer()->GetHeight())/2.0)+0.5);

		GetMdeBuffer()->Repaint(rect, x, y);
	}
}


bool X11SystemTrayIcon::HandleSystemEvent(XEvent* event)
{
	// Shall only contain code that deals with events NOT sent to the widget window

	switch (event->type)
	{
		case DestroyNotify:
		{
			if (event->xany.window == m_tray_window)
			{
				// Tray dock window has been destroyed. Try to detect a new one and
				// enter it if possible, owtherwise just hide our window

				EnterTray();

				if (m_tray_window == None)
					Hide();

				return true;
			}
		}
		break;

		case ClientMessage:
		{
			if (m_tray_window == None)
			{
				X11Types::Atom manager_atom = XInternAtom(GetDisplay(), "MANAGER", False);
				if (event->xclient.message_type == manager_atom && (Atom)event->xclient.data.l[1] == m_tray_selection)
				{
					EnterTray();
					return true;
				}
			}
		}
		break;

		case PropertyNotify:
		{
			if (event->xproperty.window == m_tray_window)
			{
				if (event->xproperty.atom == XInternAtom(GetDisplay(), "_NET_SYSTEM_TRAY_VISUAL", False))
				{
					m_tray_visual.visual = 0;
					EnterTray();
					return true;
				}
			}
		}
		break;
	}

	return false;
}

bool X11SystemTrayIcon::HandleEvent( XEvent* event )
{
	switch (event->type)
	{
		case ButtonPress:
		{
			UpdateAll();
			if (event->xbutton.button == 1)
			{
				if (!X11Widget::GetPopupWidget())
				{
					g_application->ShowOpera( !g_application->IsShowingOpera() );
					return true;
				}
			}
			break;
		}

		case ButtonRelease:
		{
			if (event->xbutton.button == 3) // FIXME: Should be configurable (left/right)
			{
				g_application->GetMenuHandler()->ShowPopupMenu("Tray Popup Menu", PopupPlacement::AnchorAtCursor());
				return true;
			}
			break;
		}

		case Expose:
		{
			if (IsVisible())
			{
				// Workaround for KDE 3x. This DE fails to resize its trayicons properly
				// in ConfigureNotify on startup. The size is typically way too large making
				// our centering fail in Repaint(). We do this for all versions of KDE
				// but I have only seen the issue in KDE 3.5.8 and later (not KDE 4). The
				// first paint always spans the entire widget.
				if (!m_has_painted)
				{
					m_has_painted = TRUE;
					if (DesktopEnvironment::GetInstance().GetToolkitEnvironment() == DesktopEnvironment::ENVIRONMENT_KDE)
					{
						UpdateSize(event->xexpose.width, event->xexpose.height);
						UpdateIcon(InitIcon, 0);
					}
				}

				OpRect rect;
				rect.x = event->xexpose.x;
				rect.y = event->xexpose.y;
				rect.width  = event->xexpose.width;
				rect.height = event->xexpose.height;
				Repaint(rect);
				return true;
			}
			break;
		}

		case ReparentNotify:
		{
			XWindowAttributes attr;
			if (XGetWindowAttributes(GetDisplay(), GetWindowHandle(), &attr))
				UpdateSize(attr.width, attr.height);
			// Set background pixmap if tray window area does not provide a 32-bit visual 
			// (in case reparenting removed the attribute set in EnterTray)
			if (!(m_tray_visual.visual && m_tray_visual.depth == 32))
				XSetWindowBackgroundPixmap(GetDisplay(), GetWindowHandle(), ParentRelative);

			Show();
			return true;
		}

		case ConfigureNotify:
		{
			// Save new size. If the window is visible we update the icon as well
			UpdateSize(event->xconfigure.width, event->xconfigure.height);
			if (IsVisible())
				UpdateIcon(InitIcon, 0);
			return X11Widget::HandleEvent(event);
		}

		case MapNotify:
		{
			UpdateIcon(InitIcon, 0);
			return X11Widget::HandleEvent(event);
		}

		case DestroyNotify:
		{
			bool state = X11Widget::HandleEvent(event);
			RecreateWindow();
			return state;
		}

		default:
			return X11Widget::HandleEvent(event);
	}

	return false;
}


void X11SystemTrayIcon::UpdateSize(int width, int height)
{
	// Use height only. Some systems has given "weird" values for width 
	m_width = m_height = height;
	Resize(m_width, m_height);
	SetMinSize(m_width, m_height); // Gnome (metacity) likes this
}


void X11SystemTrayIcon::Setup()
{
	X11Types::Display* display = GetDisplay();
	X11Types::Window root = RootWindow(display, GetScreen());

	// Be informed when a tray window appears later even if we find none now.
	XWindowAttributes attr;
	XGetWindowAttributes(display, root, &attr);
	if ((attr.your_event_mask & StructureNotifyMask) != StructureNotifyMask)
		XSelectInput(display, root, attr.your_event_mask | StructureNotifyMask);

	EnterTray();
}


void X11SystemTrayIcon::EnterTray()
{
	DetectTrayWindow();
	DetectTrayVisual();

	UpdateSize(MAX(m_tray_width,SYSTEM_TRAY_WIDTH), MAX(m_tray_height, SYSTEM_TRAY_HEIGHT));

	CreateMdeBuffer();

	UpdateIcon(InitIcon, 0);

	// Set background pixmap if tray window area does not provide a 32-bit visual
	if (!(m_tray_visual.visual && m_tray_visual.depth == 32))
		XSetWindowBackgroundPixmap(GetDisplay(), GetWindowHandle(), ParentRelative);

	if (m_tray_window != None)
	{
		XSelectInput(GetDisplay(), m_tray_window, StructureNotifyMask);

		XEvent event;
		memset(&event, 0, sizeof(event));
		event.xclient.type         = ClientMessage;
		event.xclient.window       = m_tray_window;
		event.xclient.message_type = XInternAtom(GetDisplay(), "_NET_SYSTEM_TRAY_OPCODE", False);
		event.xclient.format       = 32;
		event.xclient.data.l[0]    = CurrentTime;
		event.xclient.data.l[1]    = SYSTEM_TRAY_REQUEST_DOCK;
		event.xclient.data.l[2]    = GetWindowHandle();
		event.xclient.data.l[3]    = 0;
		event.xclient.data.l[4]    = 0;
		XSendEvent(GetDisplay(), m_tray_window, False, NoEventMask, &event);
		SetMinSize(SYSTEM_TRAY_WIDTH, SYSTEM_TRAY_HEIGHT); // Gnome (metacity) likes this
		XSync(GetDisplay(), False);
	}
}


void X11SystemTrayIcon::DetectTrayWindow()
{
	int screen = DefaultScreen(GetDisplay());

	m_tray_window = None;
	m_tray_depth  = 0;

	if (m_tray_selection == None)
	{
		OpString8 buf;
		buf.AppendFormat("_NET_SYSTEM_TRAY_S%d", screen);
		if (buf.HasContent())
			m_tray_selection = XInternAtom(GetDisplay(), buf.CStr(), False);
	}

	if (m_tray_selection != None)
	{
		XGrabServer(GetDisplay());
		m_tray_window = XGetSelectionOwner(GetDisplay(), m_tray_selection);
		XUngrabServer(GetDisplay());
	}

	if (m_tray_window != None)
	{
		XWindowAttributes attr;
		if (XGetWindowAttributes(GetDisplay(), m_tray_window, &attr ))
		{
			m_tray_depth = attr.depth;
			m_tray_width = attr.width;
			m_tray_height = attr.height;
		}
	}

}


void X11SystemTrayIcon::DetectTrayVisual()
{
	if (!m_tray_visual.visual)
	{
		if (m_tray_window == None)
		{
			DetectTrayWindow();
		}
		if (m_tray_window != None)
		{
			X11Types::Atom type;
            int  format;
            unsigned long  nitems, bytes_remaining;
            unsigned char *data = 0;

            int rc = XGetWindowProperty(
				GetDisplay(), m_tray_window, XInternAtom(GetDisplay(), "_NET_SYSTEM_TRAY_VISUAL", False), 0, 1,
				False, XA_VISUALID, &type, &format, &nitems, &bytes_remaining, &data);

			X11Types::VisualID visual_id = 0;
			if (rc == Success && type == XA_VISUALID && format == 32 && bytes_remaining == 0 && nitems == 1 )
			{
				visual_id = *(X11Types::VisualID*)data;
			}
			if (data)
			{
				XFree(data);
			}
			if (visual_id != 0)
			{
				int num_match;
				XVisualInfo visual_template;
				visual_template.visualid = visual_id;
				XVisualInfo* info = XGetVisualInfo(GetDisplay(), VisualIDMask, &visual_template, &num_match);
				if (info)
				{
				 	m_tray_visual = info[0];
					ChangeVisual(info);
					XFree((char*)info);
				}
			}
		}
	}
}



