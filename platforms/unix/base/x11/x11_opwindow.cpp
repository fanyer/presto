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

#include "x11_opwindow.h"

#include "x11_dialog.h"
#include "x11_globals.h"
#include "x11_mdebuffer.h"
#include "x11_mdescreen.h"
#include "x11_opmessageloop.h"
#include "x11_opview.h"
#include "x11_popupwidget.h"
#include "x11_viewwidget.h"
#include "x11_windowwidget.h"

#ifdef GADGET_SUPPORT
# include "x11_gadget.h"
#endif

#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/unix/product/x11quick/mdimanager.h"
#include "platforms/unix/product/x11quick/toplevelwindow.h"
#include "platforms/unix/product/x11quick/uiwindownotifier.h"

#include "adjunct/quick/Application.h"
#include "adjunct/desktop_pi/DesktopOpUiInfo.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/display/vis_dev.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/skin/OpSkinManager.h"

#include <ctype.h>

// Define if we allow a toplevel window to be opned as a child 
#define SUPPORT_EMBEDDED_POPUPS


/**
 * Helper class that will listen to clear requests from skin manager and when this happens
 * draw a background image to the backbuffer.
 */
class X11OpWindowBackgroundListener : public TransparentBackgroundListener, public SkinNotificationListener
{
public:
	X11OpWindowBackgroundListener(X11OpWindow* window)
		:m_window(window)
		,m_window_is_transparent(false)
		{
		}

	void SetWindowTransparency( bool is_transparent )
	{
		if (m_window_is_transparent != is_transparent)
		{
			m_window_is_transparent = is_transparent; // Even if test below fails
			if (m_window->GetDesktopWindow())
				m_window->GetDesktopWindow()->EnableTransparentSkins(m_window_is_transparent, FALSE, FALSE);
		}
	}

	X11OpWindowBackgroundListener* GetListener() const
	{
		// We have one widget (the menu bar) that is created as a toplevel widget
		// (it has a separate X11Widget) but still has a window parent. The window 
		// parent holds the listener (and thus bitmap) we want to use.
		X11OpWindow* parent = static_cast<X11OpWindow*>(m_window->GetParentWindow());
		return parent ? parent->GetBackgroundListener() : m_window->GetBackgroundListener();
	}

	void ClearBitmap()
	{
		m_bitmap.reset();
	}

	OpBitmap* GetBitmap() const
	{
		return m_bitmap.get();
	}

	OpPoint GetSkinOrigin() const
	{
		return m_window->GetOuterWidget()->GetSkinOrigin();
	}


	void MakeBitmap(OpSkinElement* elm)
	{
		/* The bitmap is always made on the top-level window so that a
		 * single bitmap covers everything.  Thus the bitmap should be
		 * exactly as large as the window itself, so it perfectly
		 * covers it.
		 */
		UINT32 width  = m_window->GetOuterWidget()->GetWidth();
		UINT32 height = m_window->GetOuterWidget()->GetHeight();
		OpBitmap* bitmap = m_bitmap.get();
		if (!bitmap || bitmap->Width() != width || bitmap->Height() != height)
		{
			ClearBitmap();

			if (OpStatus::IsSuccess(OpBitmap::Create(&bitmap, width, height, FALSE, TRUE, 0, 0, TRUE)))
			{
				m_bitmap.reset(bitmap);

				bitmap->SetColor(NULL, TRUE, NULL);
				OpPainter* painter = bitmap->GetPainter();
				if (!painter)
				{
					ClearBitmap();
					return;
				}

				/* If the skin origin is not (0, 0), rendering must be
				 * shifted to match.  This is unlikely to happen, so
				 * GetSkinOrigin() should probably just be removed.
				 */
				OP_ASSERT(GetSkinOrigin() == OpPoint(0, 0));
				OpRect draw_rect(0, 0, bitmap->Width(), bitmap->Height());
				VisualDevice vd;
				vd.BeginPaint(painter, draw_rect, draw_rect);
				elm->Draw(&vd, draw_rect, 0);
				vd.EndPaint();

				// Dim non active window
				if (X11WindowWidget::GetActiveWindowWidget() != m_window->GetOuterWidget())
				{
					painter->SetColor(g_desktop_op_ui_info->GetPersonaDimColor());
					painter->FillRect(draw_rect);
				}

				bitmap->ReleasePainter();
			}
		}
	}


	virtual void OnBackgroundCleared(OpPainter* op_painter, const OpRect& rect)
	{
		// Incoming painter must match what we have now. There is a chance that incoming is no longer valid
		if (m_window->GetMdeScreen()->GetVegaPainter() != op_painter)
			return;

		OpSkinElement *elm = g_skin_manager->GetPersona() ? g_skin_manager->GetPersona()->GetBackgroundImageSkinElement() : 0;
		if (!elm)
			return;

		X11OpWindowBackgroundListener* listener = GetListener();
		if (!listener)
			return;

		listener->MakeBitmap(elm);

		if (listener->GetBitmap())
		{
			SetWindowTransparency(true);

			// Code copied from WindowsBackgroundClearer
			OpBitmap * bitmap = listener->GetBitmap();
			VEGAOpPainter* painter = static_cast<VEGAOpPainter*>(op_painter);
			OpPoint skin_offset(m_window->GetOuterWidget()->GetSkinOffset());
			OpRect source_rect(skin_offset.x + painter->GetTranslationX(),
							   skin_offset.y + painter->GetTranslationY(),
							   bitmap->Width() - (skin_offset.x + painter->GetTranslationX()),
							   bitmap->Height() - (skin_offset.y + painter->GetTranslationY()));
			painter->DrawBitmapClipped(bitmap, source_rect, OpPoint(0, 0));
		}
	}

	virtual void OnSkinLoaded(OpSkin* loaded_skin) 
	{
		SetWindowTransparency(false);
	}

	/**
	 * Remove bitmap. It will be recreated in @ref OnBackgroundCleared
	 */
	virtual void OnPersonaLoaded(OpPersona* loaded_persona)
	{
		// Always reset to default behavior (= depends on skin and layout) here
		TRAPD(rc,g_pcunix->WriteIntegerL(PrefsCollectionUnix::ShowWindowBorder, 0));

		ClearBitmap();
		m_window->GetOuterWidget()->SetDecorationType( loaded_persona ? X11Widget::DECORATION_SKINNED : X11Widget::DECORATION_WM, true);
		SetWindowTransparency(loaded_persona!=0);
	}

private:
	X11OpWindow* m_window;
	OpAutoPtr<OpBitmap> m_bitmap;
	bool m_window_is_transparent;
};




class X11OpWindowListElement
{
public:
	X11OpWindowListElement(X11OpWindow * win, X11OpWindowListElement * next)
		: m_window(win),
		  m_next(next)
		{
		};

	X11OpWindow * m_window;
	X11OpWindowListElement * m_next;
};

X11OpWindowList::X11OpWindowList()
	: m_list(0),
	  m_freeze_tail_structure(0)
{
};

X11OpWindowList::~X11OpWindowList()
{
	cleanup();
	OP_ASSERT(m_list == 0);
	OP_ASSERT(m_freeze_tail_structure == 0);
};

OP_STATUS X11OpWindowList::add(X11OpWindow * win)
{
	X11OpWindowListElement * p = OP_NEW(X11OpWindowListElement, (win, m_list));
	if (p == 0)
		return OpStatus::ERR_NO_MEMORY;
	m_list = p;
	return OpStatus::OK;
};

void X11OpWindowList::remove(X11OpWindow * win)
{
	X11OpWindowListElement ** p = &m_list;
	while (*p != 0)
	{
		if ((*p)->m_window == win &&
			m_freeze_tail_structure == 0)
		{
			X11OpWindowListElement * q = *p;
			*p = q->m_next;
			OP_DELETE(q);
			/* Assuming each window only occurs once in the list, we
			 * can return here...
			 */
		}
		else
		{
			if ((*p)->m_window == win)
				(*p)->m_window = 0;
			p = &((*p)->m_next);
		};
	};
	return;
};

void X11OpWindowList::cleanup()
{
	if (m_freeze_tail_structure > 0)
		return;

	X11OpWindowListElement ** p = &m_list;
	while (*p != 0)
	{
		if ((*p)->m_window == 0)
		{
			X11OpWindowListElement * q = *p;
			*p = q->m_next;
			OP_DELETE(q);
		}
		else
			p = &((*p)->m_next);
	};
	return;
};

X11OpWindowList X11OpWindow::s_embedded_toplevel_windows;


static X11Widget *GetWidgetByViewOrWindow(X11OpWindow *window, X11OpView *view, X11Widget *wid=0)
{
	if (wid)
		return wid;
	if (view)
		return view->GetNativeWindow()->GetWidget();
	if (window)
	{
		if (window->GetWidget())
			return window->GetWidget();
		else
			return GetWidgetByViewOrWindow(static_cast<X11OpWindow *>(window->GetParentWindow()), static_cast<X11OpView *>(window->GetParentView()), 0);
	}
	return 0;
}


static X11Widget* GetWidgetFromActiveWindow()
{
	X11Widget* widget = 0;

	DesktopWindow* dw = g_application->GetActiveDesktopWindow(TRUE);
	if (dw)
	{
		X11OpWindow* opw = (X11OpWindow*)dw->GetOpWindow();
		if (opw)
		{
			widget = opw->GetWidget();
		}
	}

	return widget;
}




X11OpWindow::X11OpWindow()
	:m_widget(0)
	,m_outer_widget(0)
	,m_notifier(0)
	,m_style(STYLE_UNKNOWN)
	,m_effect(EFFECT_NONE)
	,m_type(OpTypedObject::UNKNOWN_TYPE)
	,m_parent_view(0)
	,m_parent_window(0)
	,m_window_listener(0)
	,m_cursor(CURSOR_NUM_CURSORS)
	,m_stored_menu_visible(STATE_UNKNOWN)
	,m_packed_init(0)
{
	m_packed.is_activate_on_show = TRUE;
}

X11OpWindow::~X11OpWindow()
{
	if (m_background_listener.get())
	{
		g_skin_manager->RemoveTransparentBackgroundListener(m_background_listener.get());
		g_skin_manager->RemoveSkinNotficationListener(m_background_listener.get());
	}

	if (m_packed.is_embedded_toplevel_window)
		s_embedded_toplevel_windows.remove(this);

	Close();
	OP_DELETE(m_outer_widget);
}

OP_STATUS X11OpWindow::Init(Style style,
							OpTypedObject::Type type,
							OpWindow *parent_window, OpView *parent_view,
							void *native_handle, UINT32 effect)
{
	m_style  = style;
	m_type   = type;	
	m_effect = effect;
	m_parent_view = static_cast<X11OpView *>(parent_view);
	m_parent_window = static_cast<X11OpWindow *>(parent_window);

	BOOL embed_effect_widget = FALSE;

#if defined(SUPPORT_EMBEDDED_POPUPS)
	// Test for transparency. A popup will be shown as a child if we 
	// do not run in an environment with a compositing manager when the 
	// popup requests transparency.
	if (m_style == STYLE_POPUP || m_style == STYLE_EXTENSION_POPUP || 
		m_style == STYLE_TOOLTIP || m_style == STYLE_NOTIFIER || 
		m_style == STYLE_GADGET)
	{
		if (((m_effect & EFFECT_TRANSPARENT) || m_style == STYLE_GADGET) && (parent_window || parent_view))
		{
			int screen = g_x11->GetDefaultScreen();
			X11Widget* w = GetWidgetByViewOrWindow(m_parent_window, m_parent_view, (X11Widget *)native_handle);
			if (w)
				screen = w->GetScreen();
			embed_effect_widget = !g_x11->IsCompositingManagerActive(screen) || g_startup_settings.no_argb;
		}
	}
#endif

#ifdef VEGA_OPPAINTER_SUPPORT
	if (parent_window || parent_view)
	{
		if (m_style == STYLE_DESKTOP || 
			m_style == STYLE_CHILD || 
			m_style == STYLE_EMBEDDED || 
			m_style == STYLE_WORKSPACE || 
			m_style == STYLE_TRANSPARENT ||
			embed_effect_widget)
		{
			m_packed.is_use_screen_coordinates =
				m_style == STYLE_POPUP || 
				m_style == STYLE_EXTENSION_POPUP ||
				m_style == STYLE_TOOLTIP ||
				m_style == STYLE_NOTIFIER;
			if (embed_effect_widget)
			{
				m_packed.is_embedded_toplevel_window = true;
				RETURN_IF_ERROR(s_embedded_toplevel_windows.add(this));
			};
			return MDE_OpWindow::Init(m_style, m_type, parent_window, parent_view, NULL, effect);
		}
	}
#endif // VEGA_OPPAINTER_SUPPORT

	X11Widget* parent_widget = GetWidgetByViewOrWindow(m_parent_window, m_parent_view, (X11Widget *)native_handle);
	return InitToplevelWidgets(parent_widget);
}


OP_STATUS X11OpWindow::InitToplevelWidgets(X11Widget* parent_widget)
{
	m_packed.is_toplevel = TRUE;

	int flags = 0;
	X11Widget *wid = 0;

	if( m_effect & EFFECT_TRANSPARENT )
	{
		flags |= X11Widget::TRANSPARENT;
	}

	switch(m_style)
	{
		case STYLE_TRANSPARENT:
		{
			flags |= X11Widget::TRANSPARENT;
			break;
		}

		case STYLE_HIDDEN_GADGET:
		{
			flags |= X11Widget::OFFSCREEN;
			break;
		}

#ifdef GADGET_SUPPORT
		case STYLE_GADGET:
		case STYLE_DECORATED_GADGET:
		{
			m_packed.is_tool_window = m_type >= OpTypedObject::DIALOG_TYPE && m_type < OpTypedObject::DIALOG_TYPE_LAST;
			if (m_packed.is_tool_window)
			{
				// We want a parent window to allow proper minimizing and stacking behavior
				if (!parent_widget)
					parent_widget = GetWidgetFromActiveWindow();

				wid = OP_NEW(X11WindowWidget, (this));
				// The UTILITY flag triggers borderless windows in Compiz
				flags |= X11Widget::TRANSPARENT | X11Widget::TRANSIENT | X11Widget::UTILITY;
			}
			else
			{
				wid = OP_NEW(X11Gadget, (this, m_style));
			}
			break;
		}
#endif // GADGET_SUPPORT

		case STYLE_NOTIFIER:
		case STYLE_TOOLTIP:
		{
			m_packed.is_activate_on_show = FALSE;
			flags |= X11Widget::TRANSIENT | X11Widget::BYPASS_WM | X11Widget::TOOLTIP;
			break;
		}
		case STYLE_POPUP:
		case STYLE_EXTENSION_POPUP:
		{
			wid = OP_NEW(X11PopupWidget, (this));
			break;
		}

		case STYLE_MODAL_DIALOG:
		case STYLE_MODELESS_DIALOG:
		case STYLE_BLOCKING_DIALOG:
		{
			flags |= X11Widget::TRANSIENT;
			bool modal = m_style == STYLE_MODAL_DIALOG || m_style == STYLE_BLOCKING_DIALOG;
			if (modal)
			{
				flags |= X11Widget::MODAL;
				if (!parent_widget)
					parent_widget = GetWidgetFromActiveWindow();
			}
			wid = OP_NEW(X11Dialog, (this, modal));
			break;
		}

		case STYLE_DESKTOP:
		{
			// Toplevel window will receive FocusIn event if it should be activated when first mapped.
			// Child desktop windows may unnecessarily propagate active state
			// to their parent. Helps in fixing DSK-357081. [pobara 2012-03-08]
			m_packed.is_activate_on_show = FALSE;

			if (m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
			{
				flags |= X11Widget::SYNC_REQUEST | X11Widget::PERSONA;
				ToplevelWindow *tlw = OP_NEW(ToplevelWindow, (this));
				wid = tlw;
				m_notifier = tlw;
				if (!wid)
					return OpStatus::ERR_NO_MEMORY;
			}
			break;
		}
	}

	if (!wid)
		wid = OP_NEW(X11WindowWidget, (this));

	if (!wid)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS result = wid->Init(parent_widget, "Opera", flags);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(wid);
	}
	else
	{
		if (m_style == STYLE_DESKTOP && m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
		{
			m_widget = ((ToplevelWindow *)wid)->GetChild();
			m_outer_widget = wid;
		}
		else
		{
			m_widget = (X11WindowWidget *)wid;
			m_outer_widget = wid;
		}

		SetCursor(CURSOR_DEFAULT_ARROW);

#ifdef VEGA_OPPAINTER_SUPPORT
		RETURN_IF_ERROR(m_widget->CreateMdeScreen());
		if (m_style == STYLE_DESKTOP)
			RETURN_IF_ERROR(MakeBackgroundListener());

		// Workaround for DSK-364034 - do not set SetUseShape(TRUE) for (m_effect & EFFECT_TRANSPARENT)
		if (m_style == STYLE_GADGET /*|| (m_effect & EFFECT_TRANSPARENT)*/)
			m_widget->GetMdeBuffer()->SetUseShape(TRUE);

		m_outer_widget->SetMdeScreen(m_widget->GetMdeScreen(), m_widget->GetVegaWindow(), m_widget->GetMdeBuffer());
		RETURN_IF_ERROR(MDE_OpWindow::Init(m_style, m_type, NULL, NULL, m_widget->GetMdeScreen()));

		// FIXME: With this piece of code decorated gadgets don't produce
		// ugly graphical artifacts in application mode. This is a HACK!
		// If you want more details here's the bug number DSK-270884.
		if (m_style == STYLE_DECORATED_GADGET)
			GetMDEWidget()->SetTransparent(TRUE);

		int w, h;
		InnerOuterSizeDiff(w, h);
		ResizeWindow(m_widget->GetWidth()-w, m_widget->GetHeight()-h);
#endif // VEGA_OPPAINTER_SUPPORT

	}

	return result;
}


OP_STATUS X11OpWindow::MakeBackgroundListener()
{
	if (!m_background_listener.get())
	{
		X11OpWindowBackgroundListener* listener = OP_NEW(X11OpWindowBackgroundListener,(this));
		if (!listener)
			return OpStatus::ERR_NO_MEMORY;
		m_background_listener.reset(listener);
		g_skin_manager->AddTransparentBackgroundListener(m_background_listener.get());
		g_skin_manager->AddSkinNotficationListener(m_background_listener.get());
	}

	return OpStatus::OK;
}


void X11OpWindow::UpdateBackgroundListener()
{
	if (m_background_listener.get())
	{
		m_background_listener->ClearBitmap();
		Redraw();
	}
}


#ifdef VEGA_OPPAINTER_SUPPORT
X11MdeScreen* X11OpWindow::GetMdeScreen() const 
{ 
	return m_widget ? m_widget->GetMdeScreen() : 0;
}

X11MdeBuffer* X11OpWindow::GetMdeBuffer() const 
{ 
	return m_widget ? m_widget->GetMdeBuffer() : 0;
}


OP_STATUS X11OpWindow::ResizeWindow(int width, int height)
{
	RETURN_IF_ERROR(m_widget->ResizeMdeScreen(width,height));
	MDE_OpWindow::SetInnerSize(width, height);
	return OpStatus::OK;
}
#endif // VEGA_OPPAINTER_SUPPORT

// OpWindow pi
void X11OpWindow::SetParent(OpWindow *parent_window, OpView *parent_view, BOOL behind)
{
	X11Widget * old_parentx11widget = GetNativeWindow()->GetWidget();

	/* MDE_OpWindow will set MDE_OpWindow::m_parent_view and
	 * MDE_OpWindow::m_parent_window, and will expect
	 * GetParentWindow() and GetParentView() to return the new
	 * parents.  Currently, MDE_OpWindow::SetParent() does not call
	 * GetParentWindow() or GetParentView() before setting its parent
	 * attributes.  So setting X11OpWindow::m_parent_{view,window}
	 * before calling MDE_OpWindow::SetParent() will fulfill its
	 * expectations.
	 *
	 * The correct thing to do is probably to remove
	 * X11OpWindow::m_parent_{view,window} and rely on
	 * MDE_OpWindow::m_parent_{view,window} instead.
	 */
	m_parent_view = static_cast<X11OpView *>(parent_view);
	m_parent_window = static_cast<X11OpWindow *>(parent_window);

#ifdef VEGA_OPPAINTER_SUPPORT
	if (!IsToplevel())
	{
		MDE_OpWindow::SetParent(parent_window, parent_view, behind);
	}
#endif

	X11Widget* parent_widget = GetWidgetByViewOrWindow(m_parent_window, m_parent_view);
	if (m_outer_widget)
	{
		m_outer_widget->Reparent(parent_widget);
	}
	else
	{
		/* Since this OpWindow does not have its own X11 Window, any
		 * X11 Windows in the document (plug-ins, in particular) must
		 * be reparented manually.
		 */
		/* I'm assuming that there are no children of
		 * old_parentx11widget that do not belong to this window but
		 * has descendants that do belong to this window.  So I only
		 * need to reparent immediate children of old_parentx11widget.
		 *
		 * If that assumption should prove to be wrong, it is easy to
		 * fix by recursively checking the children of any widget that
		 * is not reparented.
		 */
		X11Widget * child = old_parentx11widget->FirstChild();
		if (old_parentx11widget != parent_widget)
		{
			while (child != 0)
			{
				X11Widget * next = child->Suc();

				OpWindow* win = child->GetOpWindow();
				while (win)
				{
					if (win == this)
					{
						child->Reparent(parent_widget);
						break;
					}
					win = win->GetParentWindow();
				}

				child = next;
			};
		};
	};

	{ // local scope
		/* When a document is reparented, all the windows belonging to
		 * the document which are children of the old parent must also
		 * be reparented.  When running without a composite manager,
		 * some windows that should be top-level are created as child
		 * windows instead.  This block deals with those windows.
		 *
		 * All these top-level windows are automatically regenerated
		 * when needed.  So instead of reparenting them, it is easier
		 * to just close them here.
		 */
		s_embedded_toplevel_windows.lock_tail();
		X11OpWindowListElement * listelm = s_embedded_toplevel_windows.get_list();
		while (listelm != 0)
		{
			X11OpWindow * win = listelm->m_window;
			if (win != 0)
				win->Close();
			listelm = listelm->m_next;
		};
		s_embedded_toplevel_windows.unlock_tail();
	};
}

// OpWindow pi
OpWindow *X11OpWindow::GetRootWindow()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		return MDE_OpWindow::GetRootWindow();
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_parent_window)
		return m_parent_window->GetRootWindow();
	if (m_parent_view)
		return m_parent_view->GetRootWindow();
	return this;
}

// OpWindow pi
void X11OpWindow::SetWindowListener(OpWindowListener* window_listener)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetWindowListener(window_listener);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_window_listener = window_listener;
}

// OpWindow pi
void X11OpWindow::SetDesktopPlacement(const OpRect &rect, State state, BOOL inner, BOOL behind, BOOL center)
{
	OpRect local_rect = rect;

	if( center )
	{
		OpScreenProperties screen_properties;
		g_op_screen_info->GetProperties(&screen_properties, 0);

		OpRect top_rect;
		X11Widget* toplevel = m_parent_window && m_parent_window->m_outer_widget ? m_parent_window->m_outer_widget->GetNearestTopLevel() : 0;
		if( toplevel )
		{
			toplevel->GetPos(&top_rect.x, &top_rect.y);
			toplevel->GetSize(&top_rect.width, &top_rect.height);
		}
		else
		{
			top_rect.width  = screen_properties.width;
			top_rect.height = screen_properties.height;
		}
		local_rect.x = top_rect.x + (top_rect.width - local_rect.width) / 2;
		local_rect.y = top_rect.y + (top_rect.height - local_rect.height) / 2;

		if( local_rect.x + local_rect.width > (INT32)screen_properties.width )
		{
			local_rect.x = screen_properties.width - local_rect.width;
		}
		if( local_rect.y + local_rect.height > (INT32)screen_properties.height )
		{
			local_rect.y = screen_properties.height - local_rect.height;
		}
		if( local_rect.x < 0 )
		{
			local_rect.x = 0;
		}
		if( local_rect.y < 0 )
		{
			local_rect.y = 0;
		}
	}


#ifdef VEGA_OPPAINTER_SUPPORT
	if (!IsToplevel())
	{
		MDE_OpWindow::SetDesktopPlacement(local_rect, state, inner, behind, center);

		if (m_style == STYLE_POPUP || m_style == STYLE_EXTENSION_POPUP)
		{
			// As we do in Show() we must grab the window this window is drawn in to get the
			// desired popup behavior.
			X11OpWindow* old_popup = GetNativeWindow()->GetWidget()->GetActivePopup();
			if (old_popup)
			{
				old_popup->Show(FALSE);
				old_popup->Close(FALSE);
			}
			GetNativeWindow()->GetWidget()->SetActivePopup(this);
			GetNativeWindow()->GetWidget()->Grab(); 
		}
		return;
	}

	int w, h;
	InnerOuterSizeDiff(w, h);
	ResizeWindow(local_rect.width-w, local_rect.height-h);
#endif // VEGA_OPPAINTER_SUPPORT

	if(inner)
	{
		SetInnerSize(local_rect.width, local_rect.height);
		SetInnerPos(local_rect.x, local_rect.y);
	}
	else
	{
		SetOuterSize(local_rect.width, local_rect.height);
		SetOuterPos(local_rect.x, local_rect.y);
	}

	if (!behind)
		Activate();

	switch (state)
	{
	case RESTORED:
		m_outer_widget->SetWindowState(X11Widget::NORMAL);
		break;
	case MAXIMIZED:
		m_outer_widget->SetWindowState(X11Widget::MAXIMIZED);
		break;
	case MINIMIZED:
		m_outer_widget->SetWindowState(X11Widget::ICONIFIED);
		break;
	case FULLSCREEN:
		m_outer_widget->SetWindowState(X11Widget::FULLSCREEN);
		break;
	default:
		OP_ASSERT(FALSE);
		break;
	}

	if (behind)
		m_packed.is_move_behind_on_show = TRUE;

	Show(TRUE);
}

// OpWindow pi
void X11OpWindow::GetDesktopPlacement(OpRect &rect, State &state)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::GetDesktopPlacement(rect, state);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int x, y, w, h;
	m_outer_widget->GetNormalPos(&x, &y);
	m_outer_widget->GetNormalSize(&w, &h);
	rect.x = x;
	rect.y = y;
	rect.width = w;
	rect.height = h;
	X11Widget::WindowState widstate = m_outer_widget->GetWindowState();
	switch (widstate)
	{
	case X11Widget::NORMAL:
		state = RESTORED;
		break;
	case X11Widget::ICONIFIED:
		state = MINIMIZED;
		break;
	case X11Widget::MAXIMIZED:
		state = MAXIMIZED;
		break;
	case X11Widget::FULLSCREEN:
		state = FULLSCREEN;
		break;
	default:
		OP_ASSERT(FALSE);
		state = RESTORED;
		break;
	}
}

// OpWindow pi
X11OpWindow::State X11OpWindow::GetRestoreState() const
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		return MDE_OpWindow::GetRestoreState();
	}
#endif // VEGA_OPPAINTER_SUPPORT
	return RESTORED;
}

// OpWindow pi
void X11OpWindow::SetRestoreState(State state)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetRestoreState(state);
	}
#endif // VEGA_OPPAINTER_SUPPORT
}

// OpWindow pi
void X11OpWindow::Restore()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Restore();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_stored_menu_visible != STATE_UNKNOWN)
		SetMenuBarVisibility(m_stored_menu_visible == STATE_ON);

	m_outer_widget->SetWindowState(X11Widget::NORMAL);
}

// OpWindow pi
void X11OpWindow::Minimize()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Minimize();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->SetWindowState(X11Widget::ICONIFIED);
}

// OpWindow pi
void X11OpWindow::Maximize()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Maximize();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_stored_menu_visible != STATE_UNKNOWN)
		SetMenuBarVisibility(m_stored_menu_visible == STATE_ON);
	m_outer_widget->SetWindowState(X11Widget::MAXIMIZED);
}


// OpWindow pi
void X11OpWindow::Fullscreen()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
		return;
#endif // VEGA_OPPAINTER_SUPPORT

	m_stored_menu_visible = GetMenuBarVisibility() ? STATE_ON : STATE_OFF;
	SetMenuBarVisibility(FALSE);

	m_outer_widget->SetWindowState(X11Widget::FULLSCREEN);
}

// OpWindow pi
void X11OpWindow::LockUpdate(BOOL lock)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
		return;
#endif // VEGA_OPPAINTER_SUPPORT

//	m_outer_widget->LockUpdate(lock);
}

// OpWindow pi
void X11OpWindow::Show(BOOL show)
{
	if (m_style == STYLE_HIDDEN_GADGET)
		return;

	// Do not apply the same state that we already use. Waste of CPU. 
	// Also: Bug DSK-300910 was trigged because of showing the dropdown
	// list while already visible. The call to Activate() below is the 
	// reason (so we could have done the check only there), but then why 
	// waste CPU.

	if(IsVisible() == show)
		return;

#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		if (m_style == STYLE_POPUP || m_style == STYLE_EXTENSION_POPUP)
		{
			if (show)
			{
				X11OpWindow* old_popup = GetNativeWindow()->GetWidget()->GetActivePopup();
				if (old_popup)
				{
					old_popup->Show(FALSE);
					old_popup->Close(FALSE);
				}
				GetNativeWindow()->GetWidget()->SetActivePopup(this);
				// So that it behaves like a normal popup (try zoom popup window without)
				GetNativeWindow()->GetWidget()->Grab(); 
			}
			else
			{
				if (GetNativeWindow()->GetWidget()->GetActivePopup() == this)
				{
					GetNativeWindow()->GetWidget()->SetActivePopup(NULL);
					GetNativeWindow()->GetWidget()->Ungrab();
				}
			}
		}

		BOOL invalidate_and_bypass_lock = show && !IsVisible();

		MDE_OpWindow::Show(show);

		// When we show it, core will lock updating when loading is started before we have painted it the first time.
		// Send true for the bypass_lock parameter so the first display will paint anyway. The page will become cleared
		// since core will most probably not have got any content to paint yet.
		//only if changed!
		if (invalidate_and_bypass_lock)
			GetMDEWidget()->Invalidate(MDE_MakeRect(0, 0, GetMDEWidget()->m_rect.w, GetMDEWidget()->m_rect.h), true, false, false, true);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	SetWidgetVisibility(show);

	if (show)
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		GetMdeScreen()->ValidateMDEScreen();
#endif // VEGA_OPPAINTER_SUPPORT

		if (m_style == STYLE_MODAL_DIALOG)
		{
			m_outer_widget->PostCallback(X11WindowWidget::DELAYED_SHOW);
		}
		else
		{
			BOOL is_modal = m_style == STYLE_BLOCKING_DIALOG || (m_outer_widget->GetWindowFlags() & X11Widget::MODAL);
			if (m_packed.is_move_behind_on_show)
				m_outer_widget->ShowBehind();
			else
				m_outer_widget->Show();
			if (is_modal)
				return;

			if (m_packed.is_activate_on_show)
				Activate();

			if (m_packed.is_set_floating_on_show)
			{
				m_packed.is_set_floating_on_show = FALSE;
				SetFloating(TRUE);
			}
		}
	}
	else
	{
		m_outer_widget->Hide();
	}
}

// OpWindow pi
void X11OpWindow::SetFocus(BOOL focus)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetFocus(focus);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (focus)
		m_widget->GiveFocus();
	else
		m_widget->RemoveFocus();
}

// OpWindow pi
void X11OpWindow::Close(BOOL user_initiated)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		if ((m_style == STYLE_POPUP || m_style == STYLE_EXTENSION_POPUP) && IsVisible())
		{
			if (GetNativeWindow()->GetWidget()->GetActivePopup() == this)
			{
				GetNativeWindow()->GetWidget()->SetActivePopup(NULL);
				GetNativeWindow()->GetWidget()->Ungrab();
			}
		}
		if (mdeWidget)
			MDE_OpWindow::Close(user_initiated);
		return;
	}
	if (mdeWidget && mdeWidget->m_parent)
		mdeWidget->m_parent->RemoveChild(mdeWidget);
	if (GetWidget() && GetWidget()->GetActivePopup())
	{
		GetWidget()->GetActivePopup()->Close(FALSE);
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_packed.is_closed)
		return;

	m_packed.is_closed = TRUE;

	if (m_outer_widget)
	{
		m_outer_widget->Close();
	}

	if (m_window_listener)
		m_window_listener->OnClose(user_initiated);
}

// OpWindow pi
void X11OpWindow::GetOuterSize(UINT32 *width, UINT32 *height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::GetOuterSize(width, height);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int w, h;
	m_outer_widget->GetSize(&w, &h);
	*width = w;
	*height = h;
}

// OpWindow pi
void X11OpWindow::SetOuterSize(UINT32 width, UINT32 height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetOuterSize(width, height);
		return;
	}
#endif

	int t=0, b=0, l=0, r=0;
	m_outer_widget->GetContentMargins(t, b, l, r);
	m_outer_widget->Resize(width, height);
	SetInnerSize(width - l - r, height - t - b);
}

// OpWindow pi
void X11OpWindow::GetInnerSize(UINT32 *width, UINT32 *height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::GetInnerSize(width, height);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int w, h;
	m_widget->GetSize(&w, &h);
	*width = w;
	*height = h;
}

// OpWindow pi
void X11OpWindow::SetInnerSize(UINT32 width, UINT32 height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetInnerSize(width, height);
		return;
	}

	ResizeWindow(width, height);
#endif

	// In case widget is only allowed to use a part of m_outer_widget's area
	if (m_outer_widget != m_widget)
		m_outer_widget->AdjustAvailableSize(width, height);

	m_widget->Resize(width, height);

	if (m_window_listener)
		m_window_listener->OnResize(width, height);
}

// OpWindow pi
void X11OpWindow::GetOuterPos(INT32 *x, INT32 *y)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::GetOuterPos(x, y);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->GetPos(x, y);
}

// OpWindow pi
void X11OpWindow::SetOuterPos(INT32 x, INT32 y)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetOuterPos(x, y);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->Move(x, y);

	if (m_window_listener)
		m_window_listener->OnMove();
}

#ifdef VEGA_OPPAINTER_SUPPORT
void X11OpWindow::ConvertFromScreen(int& x, int& y)
{
	X11OpWindow* tw = GetNativeWindow();
	tw->GetWidget()->PointFromGlobal(&x, &y);
	// MDE always create popups as toplevel windows
	if (m_style != STYLE_POPUP && m_style != STYLE_EXTENSION_POPUP)
	{
		if (m_parent_window && m_parent_window->GetMDEWidget())
			m_parent_window->GetMDEWidget()->ConvertFromScreen(x, y);
		if (m_parent_view && m_parent_view->GetMDEWidget())
			m_parent_view->GetMDEWidget()->ConvertFromScreen(x, y);
	}
}
void X11OpWindow::ConvertToScreen(int& x, int& y)
{
	X11OpWindow* tw = GetNativeWindow();
	tw->GetWidget()->PointToGlobal(&x, &y);
	// MDE always create popups as toplevel windows
	if (m_style != STYLE_POPUP && m_style != STYLE_EXTENSION_POPUP)
	{
		if (m_parent_window && m_parent_window->GetMDEWidget())
			m_parent_window->GetMDEWidget()->ConvertToScreen(x, y);
		if (m_parent_view && m_parent_view->GetMDEWidget())
			m_parent_view->GetMDEWidget()->ConvertToScreen(x, y);
	}
}
#endif // VEGA_OPPAINTER_SUPPORT



// OpWindow pi
void X11OpWindow::GetInnerPos(INT32 *x, INT32 *y)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::GetInnerPos(x, y);

		if (m_packed.is_use_screen_coordinates)
			ConvertToScreen(*x, *y); // Coordinates are relative coordinates, but we want screen coordinates
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT


	// The menu bar and window decoration widgets have a separate X11 widget (and is thus a toplevel window,
	// but they still live inside the main browser window
	if (GetParentWindow())
		GetParentWindow()->GetInnerPos(x,y);
	else
	{
		m_outer_widget->GetPos(x, y);
		int dx, dy;
		InnerLeftTopOffset(dx, dy);
		*x += dx;
		*y += dy;
	}
}

// OpWindow pi
void X11OpWindow::SetInnerPos(INT32 x, INT32 y)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		if (m_packed.is_use_screen_coordinates)			
			ConvertFromScreen(x, y); // Coordinates are screen coordinates, but we want relative coordinates

		MDE_OpWindow::SetInnerPos(x, y);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int dx, dy;
	InnerLeftTopOffset(dx, dy);
	x -= dx;
	y -= dy;

	SetOuterPos(x, y);
}

// OpWindow pi
void X11OpWindow::GetRenderingBufferSize(UINT32* width, UINT32* height)
{
	// MDE_OpWindow does not implement GetRenderingBufferSize

	GetInnerSize(width, height);
}

// OpWindow pi
void X11OpWindow::SetMinimumInnerSize(UINT32 width, UINT32 height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetMinimumInnerSize(width, height);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int w, h;
	InnerOuterSizeDiff(w, h);

	m_outer_widget->SetMinSize(MAX((int)width-w, 0), MAX((int)height-h, 0));
	// FIXME: OOM
}

// OpWindow pi
void X11OpWindow::SetMaximumInnerSize(UINT32 width, UINT32 height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetMaximumInnerSize(width, height);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	int max_width = min(width, (unsigned)MAXIMUM_SIZE);
	int max_height = min(height, (unsigned)MAXIMUM_SIZE);

	int w_diff, h_diff;
	InnerOuterSizeDiff(w_diff, h_diff);

	m_outer_widget->SetMaxSize(max(max_width - w_diff, 0), max(max_height - h_diff, 0));
	// FIXME: OOM
}

// OpWindow pi
void X11OpWindow::GetMargin(INT32 *left_margin, INT32 *top_margin,
							INT32 *right_margin, INT32 *bottom_margin)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::GetMargin(left_margin, top_margin, right_margin, bottom_margin);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_notifier)
		m_notifier->GetMargin(left_margin, top_margin, right_margin, bottom_margin);
	else
		*left_margin = *right_margin = *top_margin = *bottom_margin = 0;
}

// OpWindow pi
void X11OpWindow::SetTransparency(INT32 transparency)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetTransparency(transparency);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT
}

// OpWindow pi
void X11OpWindow::SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility)
{
	// Something useful might be done here - MDE_OpWindow::SetWorkspaceMinimizedVisibility
	// is not implemented
}

// OpWindow pi
void X11OpWindow::SetFloating(BOOL floating)
{
	if (!m_outer_widget || !IsVisible())
	{
		m_packed.is_set_floating_on_show = TRUE;
		return;
	}

	m_packed.is_floating = floating;
	m_outer_widget->SetAbove(floating);
}

// OpWindow pi
void X11OpWindow::SetAlwaysBelow(BOOL below)
{
	if (m_outer_widget)
	{
		m_outer_widget->SetBelow(below);
	}
}

// OpWindow pi
void X11OpWindow::SetShowInWindowList(BOOL in_window_list)
{
	if (m_outer_widget)
	{
		m_outer_widget->SetWMSkipTaskbar(!in_window_list);
	}
}

void X11OpWindow::UpdateFont()
{
}

void X11OpWindow::UpdateLanguage()
{
}

void X11OpWindow::UpdateMenuBar()
{
	if (m_style == STYLE_DESKTOP && m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		ToplevelWindow* window = static_cast<ToplevelWindow*>(m_outer_widget);
		if (m_packed.is_menubar_visible)
			RETURN_VOID_IF_ERROR(window->SetupMenuBar());
		window->ShowMenuBar(m_packed.is_menubar_visible);
	}
}


// OpWindow pi
void X11OpWindow::SetSkin(const char *skin)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
		return;
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_notifier)
		m_notifier->SetSkinElement(skin);
	if (m_style == STYLE_DESKTOP)
		g_x11->SetWindowBackgroundSkinElement(skin);
}

// OpWindow pi
void X11OpWindow::Redraw()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Redraw();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->UpdateAll();
}

// OpWindow pi
OpWindow::Style X11OpWindow::GetStyle()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		return MDE_OpWindow::GetStyle();
	}
#endif // VEGA_OPPAINTER_SUPPORT

	return m_style;
}

// OpWindow pi
void X11OpWindow::Raise()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Raise();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->Raise();
}

// OpWindow pi
void X11OpWindow::Lower()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Lower();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->Lower();
}

// OpWindow pi
void X11OpWindow::Activate()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::Activate();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->Raise();

	// Give focus to topmost widget. If we set focus to an internal widget (window)
	// the top window may receive an focus out (DSK-308495). 
	// TODO: I am not convinced we should set focus at all. The WM will do it as well. 
	//
	// Disabled for now (2010-08-20) Setting focus to top_widget makes dropdowns fail 
	// in dialogs and setting it to m_widget leads to bug DSK-308495
	//X11Widget* top_widget = widget->GetTopLevel();
	//if (top_widget)
	//	top_widget->GiveFocus();

	if (m_window_listener)
		m_window_listener->OnActivate(TRUE);
}

// OpWindow pi
void X11OpWindow::Deactivate()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!IsToplevel())
	{
		MDE_OpWindow::Deactivate();
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->Lower();
	if (m_window_listener)
		m_window_listener->OnActivate(FALSE);
}

// OpWindow pi
void X11OpWindow::Attention()
{
}


X11OpWindow* X11OpWindow::GetNativeWindow()
{
	MDE_OpView* v = m_parent_view;
	X11OpWindow* w =  m_parent_window;
	while (v || (w && !w->IsToplevel()))
	{
		if (v)
		{
			if (v->GetParentView())
				v = (MDE_OpView*)v->GetParentView();
			else
			{
				w = (X11OpWindow*)v->GetParentWindow();
				v = NULL;
			}
		}
		else
		{
			if (w->GetParentView())
			{
				v = (MDE_OpView*)w->GetParentView();
				w = NULL;
			}
			else
				w = (X11OpWindow*)w->GetParentWindow();
		}
	}
	return w;
}


// OpWindow pi
void X11OpWindow::SetCursor(CursorType cursor)
{
	m_cursor = cursor;

	// Never set an invisible cursor when a document asks for it.
	// It is a security risk as a document may fool the user where
	// a click will take place by hiding the real cursor and moving
	// a faked pointer around.
	//
	// Note that 'm_cursor' can be set to CURSOR_NONE. X11OpWindow::GetCursor()
	// will return CURSOR_NONE to core if that is what core has requested.

	if (cursor == CURSOR_NONE)
		cursor = CURSOR_DEFAULT_ARROW;

	if (IsToplevel())
	{
		if (m_widget)
		{
			m_widget->SetCursor(cursor);
		}
	}
	else
	{
		X11OpWindow* native_window = GetNativeWindow();
		if (native_window && native_window->GetWidget())
		{
			native_window->GetWidget()->SetCursor(cursor);
		}
	}
}

// OpWindow pi
CursorType X11OpWindow::GetCursor()
{
	// X11 widgets may set platform specific cursor shapes. Core
	// shall not know about that so return the shape core has specified
	return m_cursor;
}

// OpWindow pi
const uni_char *X11OpWindow::GetTitle() const
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		return MDE_OpWindow::GetTitle();
	}
#endif // VEGA_OPPAINTER_SUPPORT

	return m_outer_widget->GetTitle();
}

// OpWindow pi
void X11OpWindow::SetTitle(const uni_char *title)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!IsToplevel())
	{
		MDE_OpWindow::SetTitle(title);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	m_outer_widget->SetTitle(title); // FIXME: OOM
}

// OpWindow pi
void X11OpWindow::SetIcon(OpWidgetImage *image)
{
	// MDE_OpWindow uses virtual void SetIcon(class OpBitmap* bitmap)
	if (IsToplevel())
	{
		m_widget->SetIcon(image);
	}
}

// OpWindow pi
void X11OpWindow::SetResourceName(const char *name)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!IsToplevel())
	{
		MDE_OpWindow::SetResourceName(name);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_outer_widget)
		m_outer_widget->SetResourceName(name);
}

// OpWindow pi
BOOL X11OpWindow::IsActiveTopmostWindow()
{
	// MDE_OpWindow does not implement IsActiveTopmostWindow

	return FALSE;
}

void X11OpWindow::SetMenuBarVisibility(BOOL visible)
{
	if (m_style == STYLE_DESKTOP && m_type == OpTypedObject::WINDOW_TYPE_BROWSER && visible != m_packed.is_menubar_visible)
	{
		ToplevelWindow* window = static_cast<ToplevelWindow*>(m_outer_widget);
		window->ShowMenuBar(visible);
		m_packed.is_menubar_visible = visible;
	}
}

BOOL X11OpWindow::GetMenuBarVisibility()
{
	return m_packed.is_menubar_visible;
}

BOOL X11OpWindow::GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	if (GetMenuBarVisibility())
	{
		if (m_style == STYLE_DESKTOP && m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
			return ((ToplevelWindow*)m_outer_widget)->GetMenubarQuickMenuInfoItems(out);
	}
	return FALSE;
}


BOOL X11OpWindow::SupportsExternalCompositing()
{
	return m_outer_widget && m_outer_widget->SupportsExternalCompositing();
}

void X11OpWindow::OnVisibilityChanged(BOOL visibility)
{
	if (m_window_listener)
		m_window_listener->OnVisibilityChanged(visibility);
}

void X11OpWindow::SetLockedByUser(BOOL locked)
{
}


void X11OpWindow::SetShowWMDecoration(BOOL show_wm_decoration)
{
	if (m_style == STYLE_DESKTOP && m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		TRAPD(rc,g_pcunix->WriteIntegerL(PrefsCollectionUnix::ShowWindowBorder, show_wm_decoration ? 1 : 2));
		m_outer_widget->SetDecorationType(show_wm_decoration ? X11Widget::DECORATION_WM : X11Widget::DECORATION_SKINNED, false);
	}
}


BOOL X11OpWindow::HasWMDecoration()
{
	if (m_style == STYLE_DESKTOP && m_type == OpTypedObject::WINDOW_TYPE_BROWSER)
	{
		return m_outer_widget->GetDecorationType() == X11Widget::DECORATION_WM;
	}
	return TRUE;
}



/*
  Implementation specific methods
*/


void X11OpWindow::RegisterAsMenuBar()
{
	if (GetOuterWidget())
	{
		m_packed.is_menubar = TRUE;
		SetActivateOnShow(FALSE);
		OpStatus::Ignore(MakeBackgroundListener());
		GetOuterWidget()->SetAcceptDrop(FALSE);
		GetOuterWidget()->SetPropagateLeaveEvents(TRUE);
	}
}


// The only diffrence from SetInnerSize() is that we do not resize
// the 'm_outer_widget' object. Can cause endless loops.
void X11OpWindow::SetInnerSizeFromPlatform(UINT32 width, UINT32 height)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if(!IsToplevel())
	{
		MDE_OpWindow::SetInnerSize(width, height);
		return;
	}

	ResizeWindow(width, height);
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_window_listener)
		m_window_listener->OnResize(width, height);
}

void X11OpWindow::SetContentSize(UINT32 width, UINT32 height)
{
	ResizeWindow(width, height);
	if (m_window_listener)
		m_window_listener->OnResize(width, height);
}


BOOL X11OpWindow::UiClosing()
{
	if (m_window_listener && !m_window_listener->OnIsCloseAllowed())
		return FALSE;

	Close(TRUE);
	// Can delete the window. Do not access object variables here 
	return TRUE;
}

MdiManager *X11OpWindow::GetMdiManager()
{
	return m_style == STYLE_WORKSPACE ? (MdiManager *)m_widget : 0;
}

void X11OpWindow::InnerOuterSizeDiff(int &w, int &h)
{
	if (m_outer_widget == m_widget)
	{
		w = 0;
		h = 0;
	}
	else
	{
		int ow, oh;
		m_outer_widget->GetSize(&ow, &oh);
		int iw, ih;
		m_widget->GetSize(&iw, &ih);

		w = ow - iw;
		h = oh - ih;
	}
}

void X11OpWindow::InnerLeftTopOffset(int &dx, int &dy)
{
	if (m_outer_widget == m_widget)
	{
		dx = 0;
		dy = 0;
	}
	else
	{
		m_outer_widget->GetDecorationOffset(&dx, &dy);
	}
}

