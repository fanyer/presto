/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetWindowHandler.h"
#include "modules/widgets/WidgetContainer.h"

#include "modules/doc/frm_doc.h"
#include "modules/forms/piforms.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/dochand/global_history.h"
#include "modules/dochand/win.h"
#include "modules/dochand/viewportcontroller.h"

#ifdef QUICK
# include "adjunct/quick/Application.h"
#endif

#include "modules/display/vis_dev.h"
#include "modules/pi/OpScreenInfo.h"

// == WidgetWindow ========================================

WidgetWindow::WidgetWindow(WidgetWindowHandler *handler) :
	m_window(NULL),
	m_widget_container(NULL),
	m_handler(handler),
	m_listener(NULL)
{
}

OP_STATUS WidgetWindow::Init(OpWindow::Style style, OpWindow* parent_window, OpView* parent_view, UINT32 effects, void* native_handle)
{
	// Init OpWindow
	RETURN_IF_ERROR(OpWindow::Create(&m_window));
	OP_STATUS s = m_window->Init(style, OpTypedObject::WINDOW_TYPE_UNKNOWN, parent_window, parent_view, native_handle, effects);
	if(OpStatus::IsError(s))
	{
		return s;
	}

	m_window->SetWindowListener(this);

	// Init WidgetContainer
	m_widget_container = OP_NEW(WidgetContainer, ());
	if (m_widget_container == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_widget_container->Init(OpRect(0,0,0,0), m_window));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this));

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_widget_container->GetRoot()->SetAccessibleParent(this);
#endif

	m_widget_container->SetParentInputContext(this);

	//Widget container should be ready before this
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_window->SetAccessibleItem(this);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	return OpStatus::OK;
}

WidgetWindow::~WidgetWindow()
{
	// Unreference this widgetwindow from the handler
	if (m_handler && m_handler->m_widget_window == this)
		m_handler->m_widget_window = NULL;
	else if (m_handler && m_handler->m_closed_widget_window == this)
		m_handler->m_closed_widget_window = NULL;

	g_main_message_handler->UnsetCallBacks(this);
	if (m_window)
		m_window->SetWindowListener(NULL);

	OP_DELETE(m_widget_container);
	OP_DELETE(m_window);
}

void WidgetWindow::SetListener(Listener *listener)
{
	m_listener = listener;
}

void WidgetWindow::Close(BOOL immediately, BOOL user_initiated)
{
	// We must call the listener before closing the window
	if (m_listener)
		m_listener->OnClose(this);

	if (immediately)
		m_window->Close(user_initiated);
	else
	{
		// Post a message that we should close the m_window very soon.
		g_main_message_handler->PostMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, (MH_PARAM_2)user_initiated);
	}
}

void WidgetWindow::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_CLOSE_AUTOCOMPL_POPUP);
	Close(TRUE, !!par2);
}

void WidgetWindow::Show(BOOL show, const OpRect* preferred_rect)
{
	if (show && preferred_rect)
	{
		m_window->SetOuterPos(preferred_rect->x, preferred_rect->y);
		m_window->SetOuterSize(preferred_rect->width, preferred_rect->height);
		m_widget_container->SetSize(preferred_rect->width, preferred_rect->height);
	}

	m_window->Show(show);

	// Show() rather than OnVisibilityChanged() is called when switching
	// between tabs, so we need a call to the listener here as well
	if (m_listener)
		m_listener->OnShow(this, show);
}

void WidgetWindow::OnResize(UINT32 width, UINT32 height)
{
	m_widget_container->SetSize(width, height);
}

void WidgetWindow::OnClose(BOOL user_initiated)
{
	// We must call the listener before the window closes
	if (m_listener)
		m_listener->OnClose(this);

	OP_DELETE(this);
}

void WidgetWindow::OnVisibilityChanged(BOOL vis)
{
	if (!vis)
		m_widget_container->GetRoot()->SetPaintingPending();

	if (m_listener)
		m_listener->OnShow(this, vis);
}

/*static */ OpRect WidgetWindow::GetBestDropdownPosition(OpWidget* widget, INT32 width, INT32 height, BOOL force_over_if_needed, const OpRect* limits)
{
	VisualDevice* vd = widget->GetVisualDevice();
	INT32 scale = vd->GetScale();

	// Calculate position

	OpRect rect = widget->GetBounds();
#ifdef QUICK
	OpRect screen_rect = widget->GetScreenRect();
#endif
	OpPoint pos;

	// different pos for form widgets

	if (widget->GetFormObject())
	{
		widget->GetFormObject()->UpdatePosition();

		FramesDocument* doc = widget->GetFormObject()->GetDocument();
		Window* win = doc->GetWindow();
		OpWindow* opwin = win->GetOpWindow();
		ViewportController* vpc = win->GetViewportController();
		OpRect visual_viewport = vpc->GetVisualViewport();
		OpRect rendering_viewport = vpc->GetRenderingViewport();
		UINT32 winwidth, winheight;
		UINT32 rbwidth, rbheight;

		opwin->GetInnerSize(&winwidth, &winheight);
		opwin->GetRenderingBufferSize(&rbwidth, &rbheight);

		if (winwidth != rbwidth || winheight != rbheight)
		{
			/* Crappy workaround (especially the above test). However, with visual
			   viewport size different from the window size, the values from OpScreenInfo
			   can no longer be used as it is the code after this block. Furthermore, the
			   way OpWindow deals with position, size, and, uh, windows, is a mess. I
			   think that class needs to go away and be replaced with something that
			   works. */

			// Start with a rectangle relative to document origo.
			AffinePos doc_pos = widget->GetPosInDocument();
			OpRect widget_rect(0, widget->GetHeight(),
							   (width * 100) / scale,
							   (height * 100) / scale);
			doc_pos.Apply(widget_rect); // Widget extents in document coords
			widget_rect = vpc->ConvertToToplevelRect(doc, widget_rect);

			// Make fit within visual viewport.

			if (widget_rect.Right() > visual_viewport.Right())
				widget_rect.x = visual_viewport.Right() - widget_rect.width;

			if (widget_rect.x < visual_viewport.x)
				widget_rect.x = visual_viewport.x;

			if (widget_rect.Bottom() > visual_viewport.Bottom())
				widget_rect.y -= widget->GetHeight() + widget_rect.height;

			if (widget_rect.y < visual_viewport.y)
				widget_rect.y = visual_viewport.y;

			// Make relative to rendering viewport.

			widget_rect.x -= rendering_viewport.x;
			widget_rect.y -= rendering_viewport.y;

			/* Make relative to rendering buffer. These values will only be correct if
			   the OpWindow implementation thinks that the parent window is at 0,0. */

			return widget->GetVisualDevice()->ScaleToScreen(widget_rect);
		}
	}

	pos = widget->GetVisualDevice()->view->ConvertToScreen(OpPoint(0, 0));

	int viewx = widget->GetVisualDevice()->GetRenderingViewX();
	int viewy = widget->GetVisualDevice()->GetRenderingViewY();

	AffinePos doc_pos = widget->GetPosInDocument();
	doc_pos.AppendTranslation(0, rect.height);

	OpPoint top_doc_pt = doc_pos.GetTranslation();
	pos.x += ((top_doc_pt.x - viewx) * scale) / 100;
	pos.y += ((top_doc_pt.y - viewy) * scale) / 100;

#if defined(_MACINTOSH_)
	pos.x += 1; // align properly
#endif

	OpRect screen;

	OpScreenProperties sp;

	//Either we get window from widget, OR from visual device
	if(widget->GetWidgetContainer()){
		//In widget case, the dropdown is a widget
		g_op_screen_info->GetProperties(&sp, widget->GetWidgetContainer()->GetWindow());
	} else {
		//In visual device case, the dropdown is part of a document
		g_op_screen_info->GetProperties(&sp, widget->GetVisualDevice()->GetWindow()->GetOpWindow());
	}

	g_op_screen_info->GetProperties(&sp, (widget->GetWidgetContainer() ? widget->GetWidgetContainer()->GetWindow() : widget->GetVisualDevice()->GetWindow()->GetOpWindow()));

	screen = sp.workspace_rect;
	if (limits)
		screen.IntersectWith(*limits);

#ifdef WIDGETS_LIMIT_DROPDOWN_TO_WINDOW
	{
		OpWindow *window = (OpWindow *) (vd->GetWindow() ? vd->GetWindow()->GetOpWindow() : NULL);
		if (window)
		{
			INT32 x, y;
			UINT32 width, height;
			window->GetInnerPos(&x, &y);
			window->GetInnerSize(&width, &height);
			screen.IntersectWith(OpRect(x, y, width, height));
		}
	}
#endif

#ifdef QUICK
	if (force_over_if_needed)
	{
		force_over_if_needed = FALSE;

		OpWidget* parent = widget->GetParent();

		while (parent && !force_over_if_needed)
		{
			if (parent && parent->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR)
			{
				OpWidget* sibling = widget->GetNextSibling();

				while (sibling)
				{
					OpRect sibling_rect = sibling->GetRect(TRUE);

					if (sibling_rect.y > screen_rect.y + screen_rect.height)
					{
						force_over_if_needed = TRUE;
						break;
					}

					sibling = sibling->GetNextSibling();
				}
			}

			widget = parent;
			parent = parent->GetParent();
		}
	}
#endif

	if (force_over_if_needed || pos.y + height > screen.y + screen.height)
	{
		int rect_height_scaled = (int)(rect.height * scale) / 100;

		int avail_under = screen.y + screen.height - pos.y;
		int avail_over = pos.y - rect_height_scaled - screen.y;

		if (avail_over > avail_under)
		{
			pos.y = pos.y - height - rect_height_scaled; // Show it over the widget instead of under.

			if (pos.y < screen.y)
			{
				height -= screen.y - pos.y;
				if (height < 40)
					height = 40;
				pos.y = screen.y;

				if (height > screen.height)
				{
					height = screen.height;
				}
			}
		}
		else
		{
			height = avail_under;
		}
	}

	if (pos.x + width > screen.x + screen.width)
	{
		if (pos.x + width > screen.x + screen.width)
		{
			pos.x = screen.x + screen.width - width;
		}
	}

	if (pos.x < screen.x)
	{
		pos.x = screen.x;
	}

	return OpRect(pos.x, pos.y, width, height);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**  Acessibility methods
**
***********************************************************************************/

int WidgetWindow::GetAccessibleChildrenCount()
{
	return m_widget_container->GetRoot()->GetAccessibleChildrenCount();
}

OpAccessibleItem* WidgetWindow::GetAccessibleChild(int child)
{
	return m_widget_container->GetRoot()->GetAccessibleChild(child);
}

int WidgetWindow::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	return m_widget_container->GetRoot()->GetAccessibleChildIndex(child);
}

OpAccessibleItem* WidgetWindow::GetAccessibleChildOrSelfAt(int x, int y)
{
	return m_widget_container->GetRoot()->GetAccessibleChildOrSelfAt(x,y);
}

OpAccessibleItem* WidgetWindow::GetAccessibleFocusedChildOrSelf()
{
	return m_widget_container->GetRoot()->GetAccessibleFocusedChildOrSelf();
}

OpAccessibleItem* WidgetWindow::GetNextAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem* WidgetWindow::GetPreviousAccessibleSibling()
{
	return NULL;
}

OP_STATUS WidgetWindow::AccessibilityGetAbsolutePosition(OpRect & rect)
{
	m_widget_container->GetRoot()->AccessibilityGetAbsolutePosition(rect);
	return OpStatus::OK;
}

OP_STATUS WidgetWindow::AccessibilityClicked()
{
	return OpStatus::OK;
}

OP_STATUS WidgetWindow::AccessibilitySetText(const uni_char* text)
{
	return OpStatus::OK;
}

OP_STATUS WidgetWindow::AccessibilitySetValue(int value)
{
	return OpStatus::OK;
}

OP_STATUS WidgetWindow::AccessibilityGetValue(int &value)
{
	return OpStatus::ERR;
}

OP_STATUS WidgetWindow::AccessibilityGetText(OpString& str)
{
	str.Empty();

	return OpStatus::OK;
}

OP_STATUS WidgetWindow::AccessibilityGetDescription(OpString& str)
{
	str.Empty();

	return OpStatus::OK;
}

OP_STATUS WidgetWindow::AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut)
{
	return OpStatus::ERR;
}

Accessibility::State WidgetWindow::AccessibilityGetState()
{
	Accessibility::State state = m_widget_container->GetRoot()->AccessibilityGetState();

	return state;
}

Accessibility::ElementKind WidgetWindow::AccessibilityGetRole() const
{
	return Accessibility::kElementKindWindow;
}

OpAccessibleItem* WidgetWindow::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* WidgetWindow::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* WidgetWindow::GetUpAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* WidgetWindow::GetDownAccessibleObject()
{
	return NULL;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

// == WidgetWindowHandler =============================================

WidgetWindowHandler::WidgetWindowHandler()
	: m_widget_window(NULL)
	, m_closed_widget_window(NULL)
{
}

WidgetWindowHandler::~WidgetWindowHandler()
{
	OnOwnerDeleted();
}

void WidgetWindowHandler::OnOwnerDeleted()
{
	if (m_widget_window)
		m_widget_window->Close(TRUE);
	m_widget_window = NULL;

	// If we have a pending closed window (the async message still hasn't arrived), we must
	// ensure it is closed immediately as we might be shutting down. (Or leak checks might trig because it's would get deleted in time).
	if (m_closed_widget_window)
		m_closed_widget_window->Close(TRUE);
	m_closed_widget_window = NULL;
}

void WidgetWindowHandler::SetWindow(WidgetWindow *window)
{
	if (m_widget_window)
	{
		// Trig a asynchronous close of the window and store the pointer in m_closed_widget_window so we can
		// close it immediately if we happen to be deallocated ourselfs.

		// Any pending closed window can be immediately closed now (and should, since we'll use the pointer for the next one)
		if (m_closed_widget_window)
			m_closed_widget_window->Close(TRUE);

		m_closed_widget_window = m_widget_window;
		m_widget_window->Show(FALSE);
		m_widget_window->Close();
	}
	m_widget_window = window;
}

void WidgetWindowHandler::Show()
{
	if (m_widget_window)
		m_widget_window->Show(TRUE/*, &translated_rect*/);
}

void WidgetWindowHandler::Close()
{
	// Just remove the current bubble for now (never reuse it)
	SetWindow(NULL);
}
