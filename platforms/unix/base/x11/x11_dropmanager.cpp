/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_dropmanager.h"

#include "platforms/unix/base/common/unixutils.h"
#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_atomizer.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_opview.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/opfile/opfile.h"


#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Set this define to TRUE or FALSE
// TRUE will cause sender to ignore avoid-rect on mouse move
#define ALWAYS_SEND_POSITION TRUE

using X11Types::Atom;

X11DropManager* g_drop_manager = 0;

//#define DEBUG_DROP


// static
OP_STATUS X11DropManager::Create()
{
	if (!g_drop_manager)
	{
		g_drop_manager = OP_NEW(X11DropManager,());
		if (g_x11_op_dragmanager)
			g_x11_op_dragmanager->AddListener(g_drop_manager);
	}

	return g_drop_manager ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


// static
void X11DropManager::Destroy()
{
	if (g_drop_manager)
	{
		if (g_x11_op_dragmanager)
			g_x11_op_dragmanager->RemoveListener(g_drop_manager);
		OP_DELETE(g_drop_manager);
		g_drop_manager = 0;
	}
}


X11DropManager::X11DropManager()
	:m_drag_source(None)
	,m_target(NULL)
	,m_mde_screen(NULL)
	,m_time(X11Constants::XCurrentTime)
	,m_owns_drop_object(FALSE)
	,m_drop_object(NULL)
	,m_drop_accepted(FALSE)
	,m_url_drop_accepted(FALSE)
	,m_drag_is_outside_opera(false)
	,m_shift_state(SHIFTKEY_NONE)
{
}


X11DropManager::~X11DropManager()
{
	Reset();

	if (m_mde_screen)
		m_mde_screen->RemoveListener(this);
}

BOOL X11DropManager::HandleEvent(XEvent* event)
{
	if (event->type == ClientMessage)
	{
		if (event->xclient.message_type == ATOMIZE(XdndEnter))
		{
			m_drag_is_outside_opera = false;

#ifdef DEBUG_DROP
			printf("(XdndEnter) Received\n");
#endif
			Reset();

			const long* l = event->xclient.data.l;

			int version = (int)(((unsigned long)(l[1])) >> 24);
			if (version > X11OpDragManager::GetProtocolVersion())
			{
				printf("opera: Can not handle drop. Remote Xdnd version: %d local: %d\n",
					   version, X11OpDragManager::GetProtocolVersion());
				return TRUE;
			}

			m_drag_source = (X11Types::Window)l[0];

			X11Widget* widget = g_x11->GetWidgetList()->FindWidget(event->xclient.window);
			if (widget && widget->GetAcceptDrop())
			{
				m_target = widget;
				g_x11->GetWidgetList()->SetDropTarget(m_target);

				GetMimeTypes(&event->xclient);
				MakeDropObject();
			}

#ifdef DEBUG_DROP
			printf("(XdndEnter) Remote Xdnd version: %d\n",  version);
			printf("(XdndEnter) Remote window: 0x%08x\n",  (unsigned int)m_drag_source);
			for (UINT32 i=0; i<m_mime_types.GetCount(); i++)
				printf("(XdndEnter) %d %s\n", i, m_mime_types.Get(i)->CStr());
			if (m_drop_object)
			{
				OpString8 tmp;
				tmp.Set(m_drop_object->GetURL());
				printf("(XdndEnter) URL: %s\n", tmp.CStr());
				tmp.Set(DragDrop_Data_Utils::GetText(m_drop_object)/*m_drop_object->GetText()*/);
				printf("(XdndEnter) TXT: %s\n", tmp.CStr());
				tmp.Set(m_drop_object->GetTitle());
				printf("(XdndEnter) TITLE: %s\n", tmp.CStr());
				for( UINT32 i=0; i<m_drop_object->GetURLs().GetCount(); i++)
				{
					tmp.Set(*m_drop_object->GetURLs().Get(i));
					printf("(XdndEnter) URL %d: %s\n", i, tmp.CStr());
				}
			}
#endif
			return TRUE;
		}
		else if (event->xclient.message_type == ATOMIZE(XdndPosition))
		{
			m_drag_is_outside_opera = false;

#ifdef DEBUG_DROP
			printf("(XdndPosition) Received\n");
#endif
			if (!m_drop_object)
			{
#ifdef DEBUG_DROP
				printf("(XdndPosition) No drop object\n");
#endif

				// Happens if g_x11_op_dragmanager has aborted drag after a message has been
				// sent to target. Abortion happens when X11OpDragManager::StopDrag() is called
				// typically when a desktop window is closed. Can be observed in bug DSK-307173
				return TRUE;
			}

			const unsigned long *l = (const unsigned long *)event->xclient.data.l;

			X11Types::Window window = (X11Types::Window)l[0];
			if (window != m_drag_source)
			{
#ifdef DEBUG_DROP
				printf("(XdndPosition) Window mismatch 0x%08x vs (expected) 0x%08x\n", (unsigned int)window, (unsigned int)m_drag_source );
#endif
				return TRUE;
			}

			OpPoint p((l[2] & 0xffff0000) >> 16, l[2] & 0x0000ffff);

			m_shift_state = SHIFTKEY_NONE;
			if (l[1] & ShiftMask)
				m_shift_state |= SHIFTKEY_SHIFT;
			if (l[1] & ControlMask)
				m_shift_state |= SHIFTKEY_CTRL;
			if (l[1] & Mod1Mask)
				m_shift_state |= SHIFTKEY_ALT;

			OpRect rect;
			BOOL always_send_position = ALWAYS_SEND_POSITION;

			m_drop_accepted = m_url_drop_accepted = FALSE;

			if (m_target && m_target != g_x11->GetWidgetList()->GetDropTarget())
			{
				// There is a risk that the target widget is removed during drag-n-drop.
				// If this happens between last XdndPosition and XdndDrop we have to abort.
				printf("opera: Can not handle drop. Drop target no longer available %p\n", m_target );
				Reset();
				return TRUE;
			}

			// Never allow drops outside modal widgets
			BOOL block_drop = FALSE;
			if (g_x11->GetWidgetList()->GetModalWidget())
			{
				if (g_x11->GetWidgetList()->GetModalWidget() != m_target)
				{
					block_drop = TRUE;
				}
			}

			if (m_target && m_target->GetAcceptDrop() && !block_drop)
			{
				p -= m_target->GetDropOffset();
				rect.x = 0;
				rect.y = 0;
				rect.width = m_target->GetWidth();
				rect.height = m_target->GetHeight();
				m_target->PointToGlobal(&rect.x, &rect.y);

				long action = None;

				X11MdeScreen* mde_screen = m_target->GetMdeScreen();
				if (mde_screen)
				{
					m_drop_point = OpPoint(p.x-rect.x, p.y-rect.y);

					// We used to reset the drop type to DROP_NONE here (as a
					// safeguard should it not be set when querying the target).
					// That is no longer possible as it breaks various HTML5 dnd tests.

					if (m_mde_screen != mde_screen)
					{
						if (m_mde_screen)
						{
							m_mde_screen->TrigDragLeave(m_shift_state);
							m_mde_screen->RemoveListener(this);
						}
						m_mde_screen = mde_screen;
						m_mde_screen->AddListener(this);
						m_mde_screen->TrigDragEnter(m_drop_point.x, m_drop_point.y, m_shift_state);
					}
					m_mde_screen->TrigDragMove(m_drop_point.x, m_drop_point.y, m_shift_state);

					switch (m_drop_object->GetVisualDropType())
					{
					case DROP_COPY:
						action = ATOMIZE(XdndActionCopy);
						break;
					case DROP_MOVE:
						action = ATOMIZE(XdndActionMove);
						break;
					case DROP_LINK:
						action = ATOMIZE(XdndActionLink);
						break;
					}

					m_drop_accepted = action != (long)None;
					if (!m_drop_accepted)
					{
						// Core does not support d-n-d of files and urls to a document window. We have to examine this as a fallback
						// If we want to prevent a combination of urls and text to be droppable we need to test for
						// OpTypedObject::DRAG_TYPE_HTML_ELEMENT [DSK-367643]
						if (DragDrop_Data_Utils::HasURL(m_drop_object) || DragDrop_Data_Utils::HasFiles(m_drop_object))
						{
							DocumentDesktopWindow* dw = X11Widget::GetDocumentDesktopWindow(m_mde_screen, m_drop_point.x, m_drop_point.y);
							if (dw)
							{
								// Quick does not by default allow a link from a page to be dropped on the same page (usability issue,
								// accidental drags should not open a new url) so we test for source and target here as well to get
								// the right cursor shape while hovering.
								DocumentDesktopWindow* src = g_x11_op_dragmanager->GetSourceWindow();

								if (dw != src || (g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_SAMEWINDOW))
								{
									m_drop_accepted = TRUE;
									m_url_drop_accepted = TRUE;
									action = ATOMIZE(XdndActionCopy);
								}
							}
						}
					}
				}

				if (rect.x < 0)
				{
					rect.width += rect.x;
					rect.x = 0;
				}
				if (rect.y < 0)
				{
					rect.height += rect.y;
					rect.y = 0;
				}
				if (rect.width < 0)
					rect.width = 0;
				if (rect.height < 0)
					rect.height = 0;

				SendXdndStatus(action, always_send_position, rect);
			}
#ifdef DEBUG_DROP
			printf("(XdndPosition) Requested position %d %d\n", p.x, p.y );
			printf("(XdndPosition) Button state 0x%08x\n", l[1] );
			printf("(XdndPosition) Drop target: %p accepted: %d\n", m_target, m_drop_accepted);
			if (m_drop_accepted)
			{
				printf("(XdndPosition) Rectangle [%d %d %d %d]\n", rect.x, rect.y, rect.width, rect.height );
				printf("(XdndPosition) Always send postition: %d\n", always_send_position);
			}
			if (l[4] != 0)
			{
				char* name = XGetAtomName(g_x11->GetDisplay(), l[4]);
				if (name)
				{
					printf("(XdndPosition) action: %s\n", name);
					XFree(name);
				}
			}
			else
				printf("(XdndPosition) action: %s\n", "None");
#endif
			return TRUE;
		}
		if (event->xclient.message_type == ATOMIZE(XdndLeave))
		{
			const unsigned long *l = (const unsigned long *)event->xclient.data.l;
			X11Types::Window window = (X11Types::Window)l[0];
			if (window != m_drag_source)
			{
#ifdef DEBUG_DROP
				printf("(XdndLeave) Window mismatch 0x%08x vs (expected) 0x%08x\n",
					   (unsigned int)window, (unsigned int)m_drag_source );
#endif
				return TRUE;
			}

			if (m_mde_screen && m_drop_object)
			{
				// Core needs to know how a drag ended after it left the opera window.
				// We can only do this for drags that have started in opera. We know that
				// by testing the m_owns_drop_object. The drop manager never owns the dragged
				// data if it was started in opera.
				if (!m_owns_drop_object)
					m_drag_is_outside_opera = true;

				m_mde_screen->TrigDragLeave(m_shift_state);

				// This is a workaround for core behavior that is not fully supported by xdnd.
				// Core needs a TrigDragEnd() when we drop on an area that does not support a drop.
				// That will trigger a call to X11OpDragManager::StopDrag() which is needed in all
				// cases to fully terminate an internal drag.
				//
				// Xdnd sends an XdndLeave event when the drag ends on an undroppable area, but it also
				// happens when an active drag leaves the area. So we have to make sure our workaround
				// only happens when the mouse drag button is released (and opera started the drag which
				// is the case when m_owns_drop_object == false)
				if (!m_owns_drop_object)
				{
					MouseButton drag_button = InputUtils::X11ToOpButton(InputUtils::GetX11DragButton());
					if (!InputUtils::IsOpButtonPressed(drag_button, true))
						m_mde_screen->TrigDragEnd(-1, -1, m_shift_state);
				}
			}
			Reset();

			// Delete dragged object if the drag came from another program
			if (m_owns_drop_object)
			{
				OP_DELETE(m_drop_object);
				m_owns_drop_object = FALSE;
				m_drop_object = NULL;
			}

			return TRUE;
		}
		else if (event->xclient.message_type == ATOMIZE(XdndDrop))
		{
			m_drag_is_outside_opera = false;

			const unsigned long *l = (const unsigned long *)event->xclient.data.l;
			X11Types::Window window = (X11Types::Window)l[0];
			if (window != m_drag_source)
			{
#ifdef DEBUG_DROP
				printf("(XdndDrop) Window mismatch 0x%08x vs (expected) 0x%08x\n",
					   (unsigned int)window, (unsigned int)m_drag_source );
#endif
				return TRUE;
			}

			if (m_target && m_target != g_x11->GetWidgetList()->GetDropTarget())
			{
				// There is a risk that the target widget is removed during drag-n-drop.
				// If this happens between last XdndPosition and XdndDrop we have to abort.
				printf("opera: Can not handle drop. Drop target no longer available %p\n", m_target );
				Reset();
				return TRUE;
			}

			BOOL drop_accepted = FALSE;

			if (m_target)
			{
				if (m_mde_screen)
				{
					X11Types::Window target_handle = m_target->GetWindowHandle();
					if (m_url_drop_accepted)
					{
						// Core does not support a drop on the document window. We have to handle this ourselves
						DocumentDesktopWindow* dw = X11Widget::GetDocumentDesktopWindow(m_mde_screen, m_drop_point.x, m_drop_point.y);
						if (dw)
						{
							OpWidget* widget = dw->GetWidgetByType(OpTypedObject::WIDGET_TYPE_BROWSERVIEW);
							if(widget)
							{
								DocumentDesktopWindow* src = g_x11_op_dragmanager->GetSourceWindow();
								if (src)
									m_drop_object->SetSource(src->GetWidgetByType(OpTypedObject::WIDGET_TYPE_BROWSERVIEW));
								dw->OnDragDrop(widget, m_drop_object, 0, 0, 0);
							}
						}
						m_mde_screen->TrigDragEnd(-1, -1, InputUtils::GetOpModifierFlags());
					}
					else
					{
						// Lock current browser window so that a drop does not activate another.
						// A drop shall never do that. See DSK-366458 where we activated the target
						// BrowseDesktopWindow while the corresponding X11 window was not activated.
						// The activation occured in DesktopWindow::SetParentWorkspace() where it is
						// normally correct to activate the (parent) browser window, but not in this case.
						g_application->SetLockActiveWindow(TRUE);

						// Let core handle drop
						m_mde_screen->TrigDragDrop(m_drop_point.x, m_drop_point.y, m_shift_state);

						g_application->SetLockActiveWindow(FALSE);
					}

					// If we drag a tab the detach code will destroy the dragged object which again
					// calls OnDragObjectDeleted() which sets m_target to 0. The target is valid as
					// long as it exists in the list of known widgets so we set it once more to let
					// SendXdndFinished() do its job (DSK-322749)
					if (!m_target)
						m_target = g_x11->GetWidgetList()->FindWidget(target_handle);

					drop_accepted = TRUE;
				}
			}

#ifdef DEBUG_DROP
			printf("(XdndDrop) drop_accepted %d\n", drop_accepted );
#endif

			if (m_target)
				SendXdndFinished(drop_accepted);

			Reset();
			return TRUE;
		}
	}

	return FALSE;
}



void X11DropManager::GetMimeTypes( XClientMessageEvent* event )
{
	X11Types::Display* display = g_x11->GetDisplay();

	const long* l = event->data.l;
	if (l[1] & 0x1)
	{
		X11Types::Atom rettype = X11Constants::XNone;
		int format;
		unsigned long items;
		unsigned long bytes_remaining;
		unsigned char *data = 0;

		int rc = XGetWindowProperty(display, m_drag_source, ATOMIZE(XdndTypeList),
									0, 100, False, XA_ATOM, &rettype, &format,
									&items, &bytes_remaining, &data);
		if (rc == Success && rettype == XA_ATOM && data)
		{
			X11Types::Atom* atom_list = (X11Types::Atom *)data;
			for (unsigned long i=0; i<items; i++)
			{
				char* name = XGetAtomName( display, atom_list[i]);
				if (name)
				{
					OpString8* s = OP_NEW(OpString8,());
					s->Set(name);
					m_mime_types.Add(s);
					XFree(name);
				}
			}
		}
		if (data)
			XFree(data);
	}
	else
	{
		for (UINT32 i=0; i<3; i++)
		{
			if (l[i+2] == 0)
				break;
			char* name = XGetAtomName( display, (X11Types::Atom)l[i+2]);
			if (name)
			{
				OpString8* s = OP_NEW(OpString8,());
				s->Set(name);
				m_mime_types.Add(s);
				XFree(name);
			}
		}
	}
}


BOOL X11DropManager::HasMimeType(const OpStringC8& mime_type)
{
	if (mime_type.Compare("text/plain;charset=") == 0)
	{
		// "text/plain" may contain arguments of the format "text/plain;charset=<encoding>"
		for (UINT32 i=0; i<m_mime_types.GetCount(); i++)
		{
			if (m_mime_types.Get(i)->FindI(mime_type) == 0)
				return TRUE;
		}
	}
	else
	{
		for (UINT32 i=0; i<m_mime_types.GetCount(); i++)
		{
			if (m_mime_types.Get(i)->CompareI(mime_type) == 0)
				return TRUE;
		}
	}
	return FALSE;
}


X11DropManager::MimeDataState X11DropManager::GetMimeData(const OpStringC8& mime_type, DragByteArray& buffer, UINT32 timeout_ms)
{
	X11Types::Display* display = g_x11->GetDisplay();

	X11Types::Atom mime_atom = XInternAtom(display, mime_type.CStr(), False);
	X11Types::Atom selection_atom = ATOMIZE(XdndSelection);

	if (XGetSelectionOwner(display, selection_atom) == X11Constants::XNone)
	{
		// Should be the source window
		return MimeError;
	}

#ifdef DEBUG_DROP
	printf("XConvertSelection: window         0x%08x\n", (unsigned int)m_target->GetWindowHandle());
	char* name = XGetAtomName( g_x11->GetDisplay(), selection_atom);
	if (name)
	{
		printf("XConvertSelection: selection_atom %s\n", name );
		XFree(name);
	}
	name = XGetAtomName( g_x11->GetDisplay(), mime_atom);
	if (name)
	{
		printf("XConvertSelection: mime_atom      %s\n", name );
		XFree(name);
	}
#endif

	XConvertSelection(display, selection_atom, mime_atom, selection_atom, m_target->GetWindowHandle(), m_time);
	XFlush(display);

	X11Types::Atom property = GetProperty(m_target->GetWindowHandle(), timeout_ms);
	if (property == X11Constants::XNone)
	{
		return MimeTimeout;
	}

#ifdef DEBUG_DROP
	name = XGetAtomName( g_x11->GetDisplay(), property);
	if (name)
	{
		printf("XConvertSelection: Received property %s\n", name );
		XFree(name);
	}
#endif

	X11Types::Atom rettype = X11Constants::XNone;
	int format;
	unsigned long items;
	unsigned long bytes_remaining;
	unsigned char *data = 0;

	int rc = XGetWindowProperty(display, m_target->GetWindowHandle(), property, 0, 1000,
								False,  mime_atom, &rettype, &format, &items, &bytes_remaining, &data);

#ifdef DEBUG_DROP
	name = XGetAtomName( g_x11->GetDisplay(), rettype);
	if (name)
	{
		printf("XGetWindowProperty: rettype %s\n", name );
		XFree(name);
	}
#endif

	if (rc == Success && rettype == mime_atom)
	{
		if (data)
		{
			buffer.SetWithTerminate(data, items);
			XFree(data);
		}

		return MimeOk;
	}

	if (data)
	{
		XFree(data);
	}

	return MimeOk;
}


/*
void Dump(const DragByteArray& array)
{
	for (UINT32 i=0; i<array.GetSize(); i++)
	{
		printf("%02x ", array.GetData()[i] );
		if (i > 0 && (i%16) == 0)
			puts("");
	}
	puts("");

	printf("%s\n", array.GetData() );

	for (UINT32 i=0; i<array.GetSize(); i++)
	{
		printf("%c ", array.GetData()[i] );
		if (i > 0 && (i%16) == 0)
			puts("");
	}

	puts("");

}
*/



void X11DropManager::OnDragEnded(DesktopDragObject* dragobject, bool cancelled)
{
	if (m_mde_screen)
	{
		// The 'm_drag_is_outside_opera' is set when a drag from opera leaves the opera surface.
		// When set, the drag ended outside opera (either canceled or dropped on a target),
		// but we still have to notify core.

		if (m_drag_is_outside_opera)
		{
			if (cancelled)
			{
				m_mde_screen->TrigDragCancel(InputUtils::GetOpModifierFlags());
			}
			else
			{
				m_mde_screen->TrigDragEnd(-1, -1, InputUtils::GetOpModifierFlags());
			}
		}
	}
}


void X11DropManager::OnDragObjectDeleted(DesktopDragObject* dragobject)
{
	if (dragobject && dragobject == m_drop_object)
	{
		Reset();

		if (m_owns_drop_object)
			OP_DELETE(m_drop_object);
		m_owns_drop_object = FALSE;
		m_drop_object = NULL;
	}
}


void X11DropManager::OnDeleted(X11MdeScreen* mde_screen)
{
	if (m_mde_screen == mde_screen)
		m_mde_screen = NULL;
}


OP_STATUS X11DropManager::MakeDropObject()
{
	if (m_owns_drop_object)
	{
		OP_DELETE(m_drop_object);
		m_drop_object = 0;
	}

	if (g_x11_op_dragmanager && g_x11_op_dragmanager->IsDragging())
	{
		m_drop_object = g_x11_op_dragmanager->GetDragObject();
		m_owns_drop_object = FALSE;
		return OpStatus::OK;
	}

	m_owns_drop_object = TRUE;

	// Parse incoming object. We look for text, title and links
	OpString text, title;
	OpVector<OpString> urls;


	// Text types in preferred order
	// We also have TEXT and COMPOUND_TEXT. COMPOUND_TEXT requires quite a bit of decoding.
	const char* text_types[] = {
		"text/unicode", "UTF8_STRING", "text/plain;charset=utf-8" "STRING", "text/plain", 0 };

	MimeDataState mime_state = MimeOk;

	for (UINT32 i=0; mime_state != MimeTimeout && text_types[i] && text.IsEmpty(); i++ )
	{
		OpStringC8 mime_type = text_types[i];
		if (HasMimeType(mime_type) )
		{
			DragByteArray data; // data.GetData() is always zero terminated
			mime_state = GetMimeData(mime_type, data, 5000);
			if (mime_state != MimeOk || data.GetSize() == 0)
			{
				continue;
			}

			if (mime_type.CompareI("text/unicode") == 0 )
			{
				// Format: Full 16-bit. Not encoded
				text.Set((uni_char*)data.GetData(), data.GetSize()/2);
			}
			else if (mime_type.CompareI("UTF8_STRING") == 0)
			{
				text.SetFromUTF8((char*)data.GetData());
			}
			else if (mime_type.FindI("text/plain;charset=") == 0)
			{
				OpStringC8 encoding = &mime_type.CStr()[19];
				if (encoding.CompareI("utf-8") == 0)
					text.SetFromUTF8((char*)data.GetData());
				// We probably want more conversions here
			}
			else if (mime_type.CompareI("text/plain") == 0)
			{
				// Native encoding. FF 36 will escape it if dragging a whole
				// url as text from address field. I believe that is a bug.
				UnixUtils::FromNativeOrUTF8((char*)data.GetData(),&text);
			}
			else if (mime_type.CompareI("STRING") == 0)
			{
				// Format: 8-bit in ISO8859-1
				text.Set((char*)data.GetData());
			}
		}
	}

	// Url title types in preferred order
	const char* title_types[] = {
		"application/x-opera-title", "text/x-moz-url-desc", 0 };

	for (UINT32 i=0; mime_state != MimeTimeout && title_types[i] && title.IsEmpty(); i++ )
	{
		OpStringC8 mime_type = title_types[i];
		if (HasMimeType(mime_type) )
		{
			DragByteArray data; // data.GetData() is always zero terminated
			mime_state = GetMimeData(mime_type, data, 5000);
			if (mime_state != MimeOk || data.GetSize() == 0)
			{
				continue;
			}

			if (mime_type.CompareI("application/x-opera-title") == 0 )
			{
				// Format: Full 16-bit. Not encoded
				title.Set((uni_char*)data.GetData(), data.GetSize()/2);
			}
			else if (mime_type.CompareI("text/x-moz-url-desc") == 0 )
			{
				// Format (FF 3.6): Full 16-bit. Not encoded
				title.Set((uni_char*)data.GetData(), data.GetSize()/2);
			}
		}
	}

	// Url types in preferred order
	const char* url_types[] = {
		"text/uri-list", "text/x-moz-url", "_NETSCAPE_URL", 0 };

	for (UINT32 i=0; mime_state != MimeTimeout && url_types[i] && urls.GetCount() == 0; i++ )
	{
		OpStringC8 mime_type = url_types[i];
		if (HasMimeType(mime_type) )
		{
			DragByteArray data; // data.GetData() is always zero terminated
			mime_state = GetMimeData(mime_type, data, 5000);
			if (mime_state != MimeOk || data.GetSize() == 0)
			{
				continue;
			}

			if (mime_type.CompareI("text/uri-list") == 0 )
			{
				// Format: 8-bit UTF8 encoded
				OpString8 tmp;
				if (tmp.Reserve(data.GetSize()+1))
				{
					UriUnescape::Unescape(tmp.CStr(), (char*)data.GetData(), UriUnescape::All);

					OpString src;
					src.SetFromUTF8(tmp.CStr());

					const uni_char* sep = uni_strstr(src.CStr(), UNI_L("\r\n"));
					if (!sep)
					{
						OpString* s = OP_NEW(OpString,());
						if (!s || OpStatus::IsError(s->Set(src)))
							OP_DELETE(s);
						else
						{
							ConvertURL(*s);

							if (s->IsEmpty() || OpStatus::IsError(urls.Add(s)))
								OP_DELETE(s);
						}
					}
					else
					{
						OpVector<OpString> list;
						StringUtils::SplitString(list, src, '\r');
						for (UINT32 i=0; i<list.GetCount(); i++)
						{
							OpString* s = list.Get(i);
							ConvertURL(*s);

							if (s->IsEmpty() || OpStatus::IsError(urls.Add(s)))
								OP_DELETE(s);
						}
					}
				}
			}
			else if (mime_type.CompareI("text/x-moz-url") == 0 )
			{
				// Format (FF 3.6). Message 16-bit. URL: Escaped and UTF8. Title: not encoded
				// <url>\n<title>

				OpString src;
				if (src.Reserve(data.GetSize()+1))
				{
					UriUnescape::Unescape(src.CStr(), (uni_char*)data.GetData(), UriUnescape::All);

					OpString url, t;
					const uni_char* sep = uni_strstr(src.CStr(), UNI_L("\n"));
					if (!sep)
					{
						url.Set(src);
					}
					else
					{
						url.Set(src, sep - src);
						t.Set(sep+1);
					}

					if (url.HasContent())
					{
						OpString8 s;
						s.Set(url.CStr());
						url.SetFromUTF8(s.CStr());

						OpString* u = OP_NEW(OpString,());
						if (!u || OpStatus::IsError(u->Set(url)))
							OP_DELETE(u);
						else
						{
							ConvertURL(*u);

							if (u->IsEmpty() || OpStatus::IsError(urls.Add(u)))
								OP_DELETE(u);
							else
								title.Set(t.CStr());
						}
					}
				}
			}
			else if (mime_type.CompareI("_NETSCAPE_URL") == 0 )
			{
				// Format (FF 3.6): Message 8-bit. URL: escaped and UTF8. Title: UTF8
				// <url>\n<title>

				OpString8 tmp;
				if (tmp.Reserve(data.GetSize()+1))
				{
					UriUnescape::Unescape(tmp.CStr(), (char*)data.GetData(), UriUnescape::All);

					OpString src;
					src.SetFromUTF8((char*)tmp.CStr());

					OpString url, t;
					const uni_char* sep = uni_strstr(src.CStr(), UNI_L("\n"));
					if (!sep)
					{
						url.Set(src);
					}
					else
					{
						url.Set(src, sep-src);
						t.Set(sep+1);
					}

					if (url.HasContent())
					{
						OpString* u = OP_NEW(OpString,());
						if (!u || OpStatus::IsError(u->Set(url)))
							OP_DELETE(u);
						else
						{
							ConvertURL(*u);

							if (u->IsEmpty() || OpStatus::IsError(urls.Add(u)))
								OP_DELETE(u);
							else
								title.Set(t.CStr());
						}
					}
				}
			}
		}
	}

	OpDragObject* op_drag_object;
	if (text.HasContent() && urls.GetCount() == 0)
		RETURN_IF_ERROR(OpDragObject::Create(op_drag_object, OpTypedObject::DRAG_TYPE_TEXT));
	else
		RETURN_IF_ERROR(OpDragObject::Create(op_drag_object, OpTypedObject::DRAG_TYPE_LINK));
	m_drop_object = static_cast<DesktopDragObject*>(op_drag_object);

	if (text.HasContent())
		DragDrop_Data_Utils::SetText(m_drop_object, text.CStr());
	if (title.HasContent())
		m_drop_object->SetTitle(text.CStr());
	for (unsigned int i=0; i<urls.GetCount(); i++)
		m_drop_object->AddURL(urls.Get(i));  // Takes ownership

	// Register local files. DSK-365147: Use text/x-opera-files as type
	// They must be unescaped. DSK-367109: Drag and Drop upload of file with space in filename fails
	for (unsigned int i=0; i<m_drop_object->GetURLs().GetCount(); i++)
	{
		OpString tmp;
		OpStringC src = m_drop_object->GetURLs().Get(i)->CStr();
		if (tmp.Reserve(src.Length()+1))
		{
			UriUnescape::Unescape(tmp.CStr(), src.CStr(), UriUnescape::All);
			OpFile file;
			BOOL exists;
			if (OpStatus::IsSuccess(file.Construct(tmp.CStr())) &&
				OpStatus::IsSuccess(file.Exists(exists)) && exists)
				m_drop_object->SetData(tmp.CStr(), UNI_L("text/x-opera-files"), TRUE, FALSE);
		}
	}

	// Quite a bit of code depends on that we provide data from the single URL function
	if (!m_drop_object->GetURL() && m_drop_object->GetURLs().GetCount() > 0)
		m_drop_object->SetURL(m_drop_object->GetURLs().Get(0)->CStr());

	return OpStatus::OK;
}


X11Types::Atom X11DropManager::GetProperty(X11Types::Window window, UINT32 timeout_ms)
{
	X11Types::Display* display = g_x11->GetDisplay();

	int xsocket = XConnectionNumber(display);

	timeval start;
	gettimeofday(&start, 0);
	timeval now = start;
	timeval end = { now.tv_sec + (timeout_ms/1000), 0 };
	timeval timeout = { 0, 0 };

	XEvent event;

	int numtest = 0;
	while (1)
	{
		fd_set readset;
		fd_set exceptset;
		FD_SET(xsocket, &readset);
		FD_SET(xsocket, &exceptset);
		select(xsocket+1, &readset, 0, &exceptset, &timeout);

		if (XCheckTypedWindowEvent(display, window, SelectionNotify, &event))
		{
			break;
		}
		if (g_x11_op_dragmanager)
		{
			g_x11_op_dragmanager->HandleOneEvent(SelectionRequest);
		}

		numtest++;
		gettimeofday(&now, 0);

		if (now.tv_sec > end.tv_sec)
		{
			printf("opera: Failed to receive data from drag-n-drop source. Timeout.\n");
			return X11Constants::XNone;
		}
		timeout.tv_sec = now.tv_sec - start.tv_sec;
		timeout.tv_usec = 0;
	}
	return event.xselection.property;
}




void X11DropManager::Reset()
{
	m_drag_source = None;
	m_mime_types.DeleteAll();
	m_target = NULL;
	g_x11->GetWidgetList()->SetDropTarget(0);

	m_drop_accepted = FALSE;
	m_url_drop_accepted = FALSE;
}

void X11DropManager::SendXdndStatus(long action, BOOL always_send_position, const OpRect& rect)
{
#ifdef DEBUG_DROP
	printf("SendXdndStatus(): Target: 0x%08x\n", (unsigned int)m_drag_source );
#endif

	XClientMessageEvent message;
	message.type         = ClientMessage;
	message.window       = m_drag_source;
	message.format       = 32;
	message.message_type = ATOMIZE(XdndStatus);
	message.data.l[0]    = m_target->GetWindowHandle();
	message.data.l[1]    = (action == (long)None ? 0 : 0x1) | (always_send_position ? 0x2 : 0);
	message.data.l[2]    = (rect.x << 16) | rect.y;
	message.data.l[3]    = (rect.width << 16) | rect.height;
	message.data.l[4]    = action;
	XSendEvent(g_x11->GetDisplay(), m_drag_source, False, NoEventMask, (XEvent*)&message);
}


void X11DropManager::SendXdndFinished(BOOL drop_accepted)
{
#ifdef DEBUG_DROP
	printf("SendXdndFinished(): Target: 0x%08x\n", (unsigned int)m_drag_source );
#endif

	XClientMessageEvent message;
	message.type         = ClientMessage;
	message.window       = m_drag_source;
	message.format       = 32;
	message.message_type = ATOMIZE(XdndFinished);
	message.data.l[0]    = m_target->GetWindowHandle();
	message.data.l[1]    = drop_accepted ? 0x1 : 0;
	message.data.l[2]    = ATOMIZE(XdndActionCopy); // Might want to use what is in request
	message.data.l[3]    = 0;
	message.data.l[4]    = 0;
	XSendEvent(g_x11->GetDisplay(), m_drag_source, False, NoEventMask, (XEvent*)&message);
}


// static
OP_STATUS X11DropManager::ConvertURL(OpString& src)
{
	// Remove "file://[localhost]" Some receiver objects can not handle that
	if (src.Find("file:") == 0)
	{
		int offset = 5;
		if (src.Find("file://localhost") == 0)
			offset = 16;
		while (src.CStr()[offset] == '/' && src.CStr()[offset+1] == '/')
			offset++;
		src.Delete(0,offset);
	}

	// Use url specified in .desktop file if that is what we have received
	OpString8 url;
	if (GetURLFromDesktopFile(src, url))
	{
		OpString8 unescaped;
		RETURN_OOM_IF_NULL(unescaped.Reserve(url.Length()+1));
		UriUnescape::Unescape(unescaped.CStr(), url.CStr(), UriUnescape::All);
		RETURN_IF_ERROR(src.SetFromUTF8(unescaped.CStr()));
	}

	// Add escapes. Needed when handing string over to opera code

	src.Strip();

	int src_len = src.Length();
	int esc_len = UriEscape::GetEscapedLength(src.CStr(), src_len, UriEscape::Filename);
	if (esc_len > src_len)
	{
		OpString buf;
		RETURN_OOM_IF_NULL(buf.Reserve(esc_len+1));
		UriEscape::Escape(buf.CStr(), src.CStr(), src_len, UriEscape::Filename);
		RETURN_IF_ERROR(src.Set(buf, esc_len));
	}

	return OpStatus::OK;
}



// static
BOOL X11DropManager::GetURLFromDesktopFile(const OpStringC& filename, OpString8& url)
{
	OpFile file;
	if (filename.IsEmpty())
		return FALSE;
	if (OpStatus::IsError(file.Construct(filename.CStr())))
		return FALSE;
	if (OpStatus::IsError(file.Open(OPFILE_READ | OPFILE_TEXT)))
		return FALSE;

	OpString8 str;
	for (int line=0; line < 500; line++)
	{
		if (OpStatus::IsError(file.ReadLine(str)))
			return FALSE;

		if (str.HasContent())
		{
			if (line == 0)
			{
				if (str.Find("[Desktop Entry]") == KNotFound)
				{
					return FALSE;
				}
			}

			int pos = str.Find("URL");
			if (pos != KNotFound)
			{
				pos = str.Find("=", pos+1);
				if (pos != KNotFound)
				{
					url.Set(&str.CStr()[pos+1]);
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}
