/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_nativedialog.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widget.h"

#include <ctype.h>
#include <stdio.h>
#include <sys/select.h>

using X11Types::Atom;

#define BASE_COLOR                0xFFBFBFBF
#define BUTTON_DOWN_COLOR         0xFFAFAFAF
#define FOCUS_COLOR               0xFF707070
#define TEXT_COLOR                0xFF000000
#define LIGHT_BUTTON_BORDER_COLOR 0xFFE6EfE6
#define DARK_BUTTON_BORDER_COLOR  0xFF737373


static void Draw3DBorder(X11Types::Drawable drawable, GC gc, UINT32 lcolor, UINT32 dcolor, OpRect rect)
{
	int x1 = rect.x;
	int y1 = rect.y;
	int x2 = rect.x+rect.width-1;
	int y2 = rect.y+rect.height-1;

	XSetForeground(g_x11->GetDisplay(), gc, lcolor);	
	XDrawLine(g_x11->GetDisplay(), drawable, gc, x1, y1, x2, y1);
	XDrawLine(g_x11->GetDisplay(), drawable, gc, x1, y1, x1, y2);
	XSetForeground(g_x11->GetDisplay(), gc, dcolor);
	XDrawLine(g_x11->GetDisplay(), drawable, gc, x1, y2, x2, y2);
	XDrawLine(g_x11->GetDisplay(), drawable, gc, x2, y1, x2, y2);
}


class TwoButtonLabelLayout : public X11NativeLayout
{
public:
	void Resize(UINT32 width, UINT32 height)
	{
		UINT label_width, label_height;
		label->GetPreferredSize(label_width,label_height);
		UINT left_width, left_height;
		left_button->GetPreferredSize(left_width,left_height);
		UINT right_width, right_height;
		right_button->GetPreferredSize(right_width,right_height);

		UINT button_width  = MAX(left_width,right_width);
		UINT button_height = MAX(left_height,right_height);

		left_button->SetPosition(button_margin, height-normal_margin-button_height);
		left_button->SetSize(button_width, button_height);
		
		right_button->SetPosition(width-button_margin-button_width, height-normal_margin-button_height);
		right_button->SetSize(button_width, button_height);
		
		label->SetPosition(normal_margin, normal_margin);
		label->SetSize(width-normal_margin*2, height-button_height-normal_margin*3);
	}

	void GetMinimumSize(UINT32& width, UINT32& height)
	{
		UINT label_width, label_height;
		label->GetPreferredSize(label_width,label_height);
		UINT left_width, left_height;
		left_button->GetPreferredSize(left_width,left_height);
		UINT right_width, right_height;
		right_button->GetPreferredSize(right_width,right_height);

		UINT button_width  = MAX(left_width,right_width);
		UINT button_height = MAX(left_height,right_height);

		width  = 2 * button_width + 3 * button_margin;
		height = label_height + button_height + 3 * normal_margin;
	}

public:
	INT32 normal_margin;
	INT32 button_margin;
	X11NativeWindow* label;
	X11NativeWindow* left_button;
	X11NativeWindow* right_button;
};





// static 
X11NativeDialog::DialogResult X11NativeDialog::ShowTwoButtonDialog(const char* title, const char* message, const char* ok, const char* cancel)
{
	OpAutoPtr<X11NativeDialog> dialog(OP_NEW(X11NativeDialog, (0)));
	OpAutoPtr<X11NativeLabel> label(OP_NEW(X11NativeLabel, (dialog.get())));
	OpAutoPtr<X11NativeButton> yes(OP_NEW(X11NativeButton, (dialog.get())));
	OpAutoPtr<X11NativeButton> no(OP_NEW(X11NativeButton, (dialog.get())));
	OpAutoPtr<TwoButtonLabelLayout> layout(OP_NEW(TwoButtonLabelLayout,()));

	if (!dialog.get() || !label.get() || !yes.get() || !no.get() || !layout.get())
		return REP_NO;

	dialog->SetTitle(title);
	dialog->SetSize(500, 100);	
	dialog->SetLayout(layout.release()); // Takes ownership

	label->SetText(message);
	label->Show();

	yes->SetX11NativeButtonListener(dialog.get());
	yes->SetId(X11NativeDialog::REP_YES);
	yes->SetText(ok);
	yes->Show();

	no->SetX11NativeButtonListener(dialog.get());
	no->SetId(X11NativeDialog::REP_NO);
	no->SetText(cancel);
	no->Show();

	((TwoButtonLabelLayout*)dialog->GetLayout())->normal_margin = 10;
	((TwoButtonLabelLayout*)dialog->GetLayout())->button_margin = 50;
	((TwoButtonLabelLayout*)dialog->GetLayout())->label = label.get();
	((TwoButtonLabelLayout*)dialog->GetLayout())->left_button = yes.get();
	((TwoButtonLabelLayout*)dialog->GetLayout())->right_button = no.get();

	X11NativeApplication::Self()->SetFocus(no.get());

	DialogResult res = dialog->Exec();

	return res;
}


X11NativeDialog::X11NativeDialog(X11NativeWindow* parent, int flags, OpRect* rect )
	:X11NativeWindow(parent, flags|MODAL, rect)
	,m_result(REP_CANCEL)
{
	MotifWindowManagerHints mwm_hints;
	mwm_hints.SetModal();
	mwm_hints.Apply(g_x11->GetDisplay(), GetHandle());
	
	Atom wm_modal_atom = ATOMIZE(_NET_WM_STATE_MODAL);
	XChangeProperty(
		g_x11->GetDisplay(), GetHandle(), ATOMIZE(_NET_WM_STATE), XA_ATOM, 32, 
		PropModeReplace, reinterpret_cast<const unsigned char*>(&wm_modal_atom), 1);

	X11Types::Window client_leader = g_x11->GetAppWindow();
	XSetTransientForHint(g_x11->GetDisplay(), GetHandle(), client_leader);
}


void X11NativeDialog::OnMapped(bool before)
{
	if (before)
	{
		if (GetLayout())
		{
			UINT32 width, height;
			GetLayout()->GetMinimumSize(width, height);
			SetMinimumSize(width,height);
			GetLayout()->Resize(GetRect().width, GetRect().height);
		}

		// Some WMs (not those in KDE/Gnome) need this
		XWindowAttributes attr;
		XGetWindowAttributes(g_x11->GetDisplay(), GetHandle(), &attr);
		if (attr.x == 0 && attr.y == 0)
		{
			XGetWindowAttributes(g_x11->GetDisplay(), g_x11->GetAppWindow(), &attr);
			int x = (attr.width - GetRect().width) / 2;
			int y = (attr.height - GetRect().height) / 2;
			SetPosition(x,y);
		}		
	}
	else
	{
		SetAbove(true);
	}
}


X11NativeDialog::DialogResult X11NativeDialog::Exec()
{
	m_result = REP_CANCEL;

	Show();
	return m_result;
}


bool X11NativeDialog::HandleEvent(XEvent* event)
{
	switch (event->type)
	{
		case ConfigureNotify:
			SetRect(OpRect(event->xconfigure.x, event->xconfigure.y, event->xconfigure.width, event->xconfigure.height));
			if (GetLayout())
			{
				GetLayout()->Resize(GetRect().width, GetRect().height);
			}
			return true;
		break;

		case KeyPress:
		{
			KeySym keysym = XKeycodeToKeysym(g_x11->GetDisplay(), event->xkey.keycode, 0);
			switch(keysym)
			{
				case XK_Escape:
					Cancel();
					break;

				// This only works well because we have two buttons and nothing more 
				case XK_Tab:
				case XK_Right:
				case XK_Left:
					X11NativeApplication::Self()->StepFocus(true, event->xkey.time);
					break;
			}
		}
		break;

		case FocusIn:
			X11NativeApplication::Self()->InitFocus();
		break;


	}

	return X11NativeWindow::HandleEvent(event);
}



X11NativeWindow::X11NativeWindow(X11NativeWindow* parent, int flags, OpRect* rect)
	:m_parent(parent)
	,m_handle(None)
	,m_layout(0)
	,m_size_hints(0)
	,m_depth(0)
	,m_flags(flags)
	,m_has_focus(false)
{
	bool is_toplevel = (m_parent == 0);

	X11Types::Window win_parent = m_parent ? m_parent->GetHandle() : None;
	if (win_parent == None)
		win_parent = RootWindow(g_x11->GetDisplay(), g_x11->GetDefaultScreen());

	XSetWindowAttributes values;
	values.background_pixel = BASE_COLOR;
	values.border_pixel = BlackPixel(g_x11->GetDisplay(), 0);
	values.event_mask = ExposureMask |
		KeyPressMask | KeyReleaseMask |
		ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask |
		FocusChangeMask |
		EnterWindowMask |
		LeaveWindowMask |
		StructureNotifyMask |
		PropertyChangeMask;

	unsigned int mask = CWBackPixel|CWBorderPixel|CWEventMask;
	if (flags & UNMANAGED)
	{
		values.override_redirect = True;
		mask |= CWOverrideRedirect;
	}

	if (rect)
	{
		m_rect = *rect;
	}
	else
	{
		m_rect.x = m_rect.y = 0;
		m_rect.width = m_rect.height = 100;
	}

	m_handle = XCreateWindow(
		g_x11->GetDisplay(), win_parent, m_rect.x, m_rect.y, m_rect.width, m_rect.height, 0,
		CopyFromParent, InputOutput, CopyFromParent, mask, &values);

	if (is_toplevel)
	{
		m_size_hints = OP_NEW(XSizeHints, ());
		if (m_size_hints)
			m_size_hints->flags = 0;

		Atom protocols[3];
		protocols[0] = ATOMIZE(WM_DELETE_WINDOW);
		protocols[1] = ATOMIZE(WM_TAKE_FOCUS);
		protocols[2] = ATOMIZE(_NET_WM_PING); 
		XSetWMProtocols(g_x11->GetDisplay(), m_handle, protocols, 3);
	}

	XWindowAttributes attr;
	XGetWindowAttributes(g_x11->GetDisplay(), m_handle, &attr);
	m_depth = attr.depth;

	X11NativeApplication::Self()->AddWindow(this);
}


X11NativeWindow::~X11NativeWindow()
{
	OP_DELETE(m_layout);
	OP_DELETE(m_size_hints);
	X11NativeApplication::Self()->RemoveWindow(this);
}


void X11NativeWindow::Show()
{
	OnMapped(true);
	XMapWindow(g_x11->GetDisplay(), m_handle);
	OnMapped(false);
	if (m_flags & MODAL)
		X11NativeApplication::Self()->EnterLoop();
}


void X11NativeWindow::Hide()
{
	XUnmapWindow(g_x11->GetDisplay(), m_handle);
	if (m_flags & MODAL)
		X11NativeApplication::Self()->LeaveLoop();
}


void X11NativeWindow::Repaint(const OpRect&)
{
	XClearWindow(g_x11->GetDisplay(), GetHandle());
}


bool X11NativeWindow::HandleEvent(XEvent* event)
{
	switch (event->type)
	{
		case Expose:
			Repaint(OpRect(event->xexpose.x,event->xexpose.y,event->xexpose.width,event->xexpose.height));		
			break;

		case ClientMessage:
		{
			X11Types::Atom atom = event->xclient.data.l[0];
			if (atom == ATOMIZE(WM_DELETE_WINDOW))
			{
				Cancel();
				return true;
			}
			else if( atom == ATOMIZE(_NET_WM_PING) )
			{
				X11Types::Window root = RootWindowOfScreen(DefaultScreenOfDisplay(event->xclient.display));
				if (event->xclient.window != root) 
				{
					event->xclient.window = root;
					XSendEvent(event->xclient.display, event->xclient.window,False, 
							   SubstructureNotifyMask|SubstructureRedirectMask, event);
				}
				return TRUE;
			}
			break;
		}

		case FocusIn:
			m_has_focus = true;
			Repaint(GetRect());
		break;

		case FocusOut:
			m_has_focus = false;
			Repaint(GetRect());
		break;

		case KeyPress:
			// Let parent use shortcuts if any			
			if (GetParent())
				return GetParent()->HandleEvent(event);
		break;
	}

	return false;
}


void X11NativeWindow::SetPosition(int x, int y)
{
	m_rect.x = x;
	m_rect.y = y;
	XMoveWindow(g_x11->GetDisplay(), GetHandle(), m_rect.x, m_rect.y);
	if (m_size_hints)
	{
		m_size_hints->x = m_rect.x;
		m_size_hints->y = m_rect.y;
		m_size_hints->flags |= PPosition | USPosition;
		XSetWMNormalHints(g_x11->GetDisplay(), m_handle, m_size_hints);
	}
}


void X11NativeWindow::SetSize(UINT32 width, UINT32 height)
{
	m_rect.width = width;
	m_rect.height = height;
	XResizeWindow(g_x11->GetDisplay(), GetHandle(), m_rect.width, m_rect.height);
	if (m_size_hints)
	{
		m_size_hints->width  = m_rect.width;
		m_size_hints->height = m_rect.height;
		m_size_hints->flags |= PSize | USSize;
		XSetWMNormalHints(g_x11->GetDisplay(), GetHandle(), m_size_hints);
	}
}


void X11NativeWindow::SetMinimumSize(UINT32 width, UINT32 height)
{	
	if (m_size_hints)
	{
		m_size_hints->flags |= PMinSize;
		m_size_hints->min_width = width;
		m_size_hints->min_height = height;
		XSetWMNormalHints(g_x11->GetDisplay(), GetHandle(), m_size_hints);
	}
}


void X11NativeWindow::SetTitle(const OpStringC8& title)
{
	if (title.IsEmpty())
		return;
	XChangeProperty(g_x11->GetDisplay(), GetHandle(), ATOMIZE(WM_NAME), XA_STRING, 8, PropModeReplace,
					reinterpret_cast<const unsigned char*>(title.CStr()), title.Length());
}


void X11NativeWindow::SetText(const OpStringC8& text)
{
	m_text.Set(text);
}


void X11NativeWindow::SetAbove(bool state)
{	
	if (IS_NET_SUPPORTED(_NET_WM_STATE))
	{
		XEvent event;

		event.xclient.type = ClientMessage;
		event.xclient.display = g_x11->GetDisplay();
		event.xclient.window = GetHandle();
		event.xclient.message_type = ATOMIZE(_NET_WM_STATE);
		event.xclient.format = 32;
		event.xclient.data.l[0] = state ? 1 : 0;
		event.xclient.data.l[2] = 0;
		event.xclient.data.l[3] = 0;
		event.xclient.data.l[4] = 0;
		
		if (IS_NET_SUPPORTED(_NET_WM_STATE_ABOVE))
			event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_ABOVE);
		else if (IS_NET_SUPPORTED(_NET_WM_STATE_STAYS_ON_TOP))
			event.xclient.data.l[1] = ATOMIZE(_NET_WM_STATE_STAYS_ON_TOP);
		else
		{
			XRaiseWindow(g_x11->GetDisplay(), GetHandle());
			return;
		}
		
		X11Types::Window root = RootWindowOfScreen(DefaultScreenOfDisplay(g_x11->GetDisplay()));
		XSendEvent(g_x11->GetDisplay(), root, False, SubstructureNotifyMask, &event);
	}
	else
	{
		XRaiseWindow(g_x11->GetDisplay(), GetHandle());
	}
}



X11NativeButton::X11NativeButton(X11NativeWindow* parent, int flags, OpRect* rect)
	:X11NativeWindow(parent,flags,rect)
	,m_listener(0)
	,m_id(-1)
	,m_down(false)
	,m_inside(false)
	,m_pressed(false)
{
}


void X11NativeButton::GetPreferredSize(UINT& width, UINT& height)
{
	width = 100;
	height = 30;

	XGCValues gc_value;
	GC gc = XCreateGC(g_x11->GetDisplay(), GetHandle(), 0, &gc_value);

	XFontStruct* fs = XQueryFont(g_x11->GetDisplay(), XGContextFromGC(gc));
	if (fs)
	{
		height = fs->ascent + fs->descent + 12;
		width  = XTextWidth(fs, GetText(), GetText().Length()) + 12;
		
		if (height < 30)
			height = 30;
		if (width < 100)
			width = 100;

		XFreeFontInfo(0, fs, 0);
	}

	XFreeGC(g_x11->GetDisplay(), gc);
}



void X11NativeButton::Repaint(const OpRect& rect)
{
	OpRect r(GetRect());
	r.x = 0;
	r.y = 0;

	X11NativeWindow::Repaint(r);
	
	XGCValues gc_value;
	GC gc = XCreateGC(g_x11->GetDisplay(), GetHandle(), 0, &gc_value);

	if(m_down && m_inside)
	{
		XSetForeground(g_x11->GetDisplay(), gc, BUTTON_DOWN_COLOR);
		XFillRectangle(g_x11->GetDisplay(), GetHandle(), gc, r.x, r.y, r.width, r.height);
	}

	UINT32 c1 = LIGHT_BUTTON_BORDER_COLOR;
	UINT32 c2 = DARK_BUTTON_BORDER_COLOR;
	if (m_down || m_pressed)
	{
		Draw3DBorder(GetHandle(), gc, c2, c1, r);
		Draw3DBorder(GetHandle(), gc, c2, c1, r.InsetBy(1,1));
	}
	else
	{
		Draw3DBorder(GetHandle(), gc, c1, c2, r);
		Draw3DBorder(GetHandle(), gc, c1, c2, r.InsetBy(1,1));
	}

	if (HasFocus())
	{
		UINT32 c = FOCUS_COLOR;
		Draw3DBorder(GetHandle(), gc, c, c, r.InsetBy(4,4));
	}
	

	if (GetText())
	{
		int a = 0;
		int d = 0;
		int w = 0;
		int l = GetText().Length();

		XFontStruct* fs = XQueryFont(g_x11->GetDisplay(), XGContextFromGC(gc));
		if (fs)
		{
			a = fs->ascent;
			d = fs->descent;
			w = XTextWidth(fs, GetText(), l);

			XSetForeground(g_x11->GetDisplay(), gc, TEXT_COLOR);
			XDrawString( g_x11->GetDisplay(), GetHandle(), gc, (r.width-w)/2, (r.height-(a+d))/2+a, GetText(), l);

			XFreeFontInfo(0, fs, 0);
		}
	}


	XFreeGC(g_x11->GetDisplay(), gc);
}


bool X11NativeButton::HandleEvent(XEvent* event)
{
	switch (event->type)
	{
		case LeaveNotify:
			if (m_down)
			{
				m_inside = false;
				Repaint(GetRect());
			}
		break;

		case EnterNotify:
			if (m_down)
			{
				
				m_inside = true;
				Repaint(GetRect());
			}
		break;

		case ButtonPress:
			m_down = true;
			m_inside = true;
			Repaint(GetRect());
			return true;
		break;

		case ButtonRelease:
			m_down = false;
			m_inside = false;
			Repaint(GetRect());

			if (m_listener)
			{
				OpRect rect = GetRect();
				int x = rect.x + event->xbutton.x;
				int y = rect.y + event->xbutton.y;
				if (rect.Contains(OpPoint(x, y)))
				{
					m_listener->OnButtonActivated(m_id);
				}
			}

			return true;
		break;

		case KeyPress:
		{
			KeySym keysym = XKeycodeToKeysym(g_x11->GetDisplay(), event->xkey.keycode, 0);
			if (keysym == XK_KP_Enter || keysym == XK_Return || keysym == XK_space)
			{
				m_pressed = true;
				Repaint(GetRect());
				return true;
			}
		}
		break;

		case KeyRelease:
			if (m_pressed)
			{
				KeySym keysym = XKeycodeToKeysym(g_x11->GetDisplay(), event->xkey.keycode, 0);
				if (keysym == XK_KP_Enter || keysym == XK_Return || keysym == XK_space)
				{
					m_pressed = false;
					Repaint(GetRect());
			
					if (m_listener)
					{
						m_listener->OnButtonActivated(m_id);
					}
					return true;
				}
			}
		break;

	}

	return X11NativeWindow::HandleEvent(event);
}





X11NativeLabel::X11NativeLabel(X11NativeWindow* parent, int flags, OpRect* rect)
	:X11NativeWindow(parent,flags,rect)
{
}


void X11NativeLabel::GetPreferredSize(UINT& width, UINT& height)
{	
	width = 500;

	FormatText(width);

   	height = m_text_collection.line_height * (m_text_collection.list.GetCount() + 1);

	if (height < 50)
		height = 50;

}

void X11NativeLabel::FormatText(INT32 width)
{
	XGCValues gc_value;
	GC gc = XCreateGC(g_x11->GetDisplay(), GetHandle(), 0, &gc_value);

	m_text_collection.Reset();

	XFontStruct* fs = XQueryFont(g_x11->GetDisplay(), XGContextFromGC(gc));
	if (fs)
	{
		const char* text = GetText();
		const char* base = text;
		const char* candidate = text;
		
		while (1)
		{
			const char* p = strpbrk(base, " \n");
			if (!p)
			{
				OpString8* s = OP_NEW(OpString8,());
				s->Set(text);
				m_text_collection.list.Add(s);
				break;
			}
				
			int w1 = XTextWidth(fs, text, p-text);
			if (w1 < width || *p == '\n')
			{
				candidate = p;
				base = p+1;
				if( *p == ' ') 
					continue;
			}

			int w2 = XTextWidth(fs, text, candidate-text);
			if (w2 <= width)
			{
				OpString8* s = OP_NEW(OpString8,());
				s->Set(text, candidate-text);
				m_text_collection.list.Add(s);
			}
			else
			{
				for (p=text; p<candidate; p++)
				{
					int w3 = XTextWidth(fs, text, p-text);
					if ((p > text+1 && w3 > width) || p+1 == candidate)
					{
						OpString8* s = OP_NEW(OpString8,());
						s->Set(text, w3 > width ? p-text-1 : p-text+1);
						m_text_collection.list.Add(s);
						text = p-1;
					}
				}
			}
			base = text = candidate = candidate+1;
		}
		
		m_text_collection.line_height = fs->ascent + fs->descent;
		m_text_collection.ascent = fs->ascent;
		m_text_collection.descent = fs->descent;
		
		for (UINT32 i=0; i<m_text_collection.list.GetCount(); i++)
		{
			INT32 sw = XTextWidth(fs, m_text_collection.list.Get(i)->CStr(), m_text_collection.list.Get(i)->Length());
			if (m_text_collection.width < sw)
				m_text_collection.width = sw;
		}

		XFreeFontInfo(0, fs, 0);
	}

	XFreeGC(g_x11->GetDisplay(), gc);
}


void X11NativeLabel::Repaint(const OpRect& rect)
{
	OpRect r(GetRect());
	r.x = 0;
	r.y = 0;

	// We prefer double buffering. Less flicker
	X11Types::Drawable drawable = GetHandle();
	X11Types::Pixmap pixmap = XCreatePixmap(g_x11->GetDisplay(), GetHandle(), r.width, r.height, GetDepth());
	if (pixmap != None)
	{
		drawable = pixmap;		
	}

	XGCValues gc_value;
	GC gc = XCreateGC(g_x11->GetDisplay(), drawable, 0, &gc_value);
	
	XSetForeground(g_x11->GetDisplay(), gc, BASE_COLOR);
	XFillRectangle(g_x11->GetDisplay(), drawable, gc, 0, 0, r.width, r.height);
	
	if (GetText().HasContent())
	{
		FormatText(r.width);

		XSetForeground(g_x11->GetDisplay(), gc, TEXT_COLOR);

		if (m_text_collection.list.GetCount()>0)
		{
			int height = m_text_collection.line_height * m_text_collection.list.GetCount();
			int x = (r.width-m_text_collection.width)/2;
			int y = (r.height-(height))/2+m_text_collection.ascent;

			if( y < 0 )
				y = 0;
			if( x < 0 )
				x = 0;

			for (UINT32 i=0; i<m_text_collection.list.GetCount(); i++)
			{
				XDrawString(g_x11->GetDisplay(), drawable, gc, x, y, 
							m_text_collection.list.Get(i)->CStr(), m_text_collection.list.Get(i)->Length());
				y += m_text_collection.line_height;
			}
		}
	}

	if (drawable == pixmap)
	{
		XCopyArea(g_x11->GetDisplay(), pixmap, GetHandle(), gc, 0, 0, r.width, r.height, 0, 0);
		XFreePixmap(g_x11->GetDisplay(), pixmap);
	}

	XFreeGC(g_x11->GetDisplay(), gc);
}



static X11NativeApplication NativeApplication;

// static 
X11NativeApplication* X11NativeApplication::Self()
{
	return &NativeApplication;
}


X11NativeApplication::X11NativeApplication()
	:m_focused_window(0)
	,m_num_window(0)
	,m_nest_count(0)
	,m_stop_count(0)
#if defined(SELFTEST)
	,m_listener(0)
	,m_id(0)
#endif
{
}


void X11NativeApplication::AddWindow(X11NativeWindow* window)
{
	if (!window)
		return;

	for (UINT32 i=0; i<m_num_window; i++)
		if (m_window_list[i] == window)
			return;

	m_window_list[m_num_window++] = window;
}



void X11NativeApplication::RemoveWindow(X11NativeWindow* window)
{
	if (!window)
		return;

	for (UINT32 i=0; i<m_num_window; i++)
		if (m_window_list[i] == window )
		{
			m_num_window --;
			for( ; i<m_num_window; i++)
				m_window_list[i] = m_window_list[i+1];
			break;
		}

	if (m_focused_window == window)
		m_focused_window = 0;
}


void X11NativeApplication::StepFocus(bool next, X11Types::Time time)
{
	X11Types::Window focused_window = None;
	int mode;
	
	XGetInputFocus(g_x11->GetDisplay(), &focused_window, &mode);
	if (focused_window == None)
		focused_window = m_window_list[0]->GetHandle();

	for (UINT32 i=0; i<m_num_window; i++)
	{
		if (m_window_list[i]->GetHandle() == focused_window)
		{
			for (UINT32 j=1; j<m_num_window; j++)
			{
				X11NativeWindow* w = m_window_list[ next ? (i+j)%m_num_window : (i+m_num_window-j)%m_num_window];
				if (w->AcceptFocus())
				{
					XSetInputFocus(g_x11->GetDisplay(), w->GetHandle(), RevertToParent, time);
					m_focused_window = w;
					break;
				}
			}
			break;
		}
	}
}


void X11NativeApplication::InitFocus()
{
	if (!m_focused_window)
	{
		for (UINT32 i=0; i<m_num_window; i++)
		{
			if (m_window_list[i]->AcceptFocus())
			{
				m_focused_window = m_window_list[i];
				break;
			}
		}
	}
	
	if (m_focused_window)
		XSetInputFocus(g_x11->GetDisplay(), m_focused_window->GetHandle(), RevertToParent, CurrentTime);
}


X11NativeWindow* X11NativeApplication::GetWindow(X11Types::Window handle)
{
	for (UINT32 i=0; i<m_num_window; i++)
		if (m_window_list[i]->GetHandle() == handle)
			return m_window_list[i];
	
	return 0;
}




void X11NativeApplication::LeaveLoop()
{
	if (m_stop_count < m_nest_count)
		m_stop_count ++;
}


void X11NativeApplication::EnterLoop()
{
	m_nest_count ++;

	XEvent event;
	while (!m_stop_count)
	{
		XNextEvent(g_x11->GetDisplay(), &event);

		X11NativeWindow* window = GetWindow(event.xany.window);
		if (window)
		{
			window->HandleEvent(&event);
		}
#if defined(SELFTEST)
		if (m_listener)
			m_listener->OnButtonActivated(m_id);
#endif
	}

	m_stop_count --;
	m_nest_count --;
}
