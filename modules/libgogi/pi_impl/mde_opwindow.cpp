/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/hardcore/mh/messobj.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libgogi/pi_impl/mde_widget.h"
#include "modules/libgogi/pi_impl/mde_generic_screen.h"
#include "modules/libgogi/pi_impl/mde_dsmode.h"
#include "modules/libgogi/pi_impl/mde_chrome_handler.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/pi/OpBitmap.h"

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinElement.h"

#include "modules/display/vis_dev.h"

class MDE_ChromeWindow:
	private MessageObject
{
public:
	MDE_ChromeWindow() : left_chrome_width(1), right_chrome_width(1), top_chrome_height(1), bottom_chrome_height(1), m_window(NULL), m_ch(NULL)
	{
	}
	~MDE_ChromeWindow()
	{
		g_main_message_handler->UnsetCallBacks(this);
		OP_DELETE(m_ch);
		OP_DELETE(m_window);
	}
	OP_STATUS Init(MDE_OpWindow *target_window)
	{
		m_ch = MDEChromeHandler::Create();
		if (!m_ch)
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_MDE_CHROMEWINDOW_CLOSE, MH_PARAM_1(this)));
		RETURN_IF_ERROR(OpWindow::Create(&m_window));
		RETURN_IF_ERROR(m_window->Init(OpWindow::STYLE_TRANSPARENT, OpTypedObject::WINDOW_TYPE_UNKNOWN, target_window->GetParentWindow(), target_window->GetParentView(), 0));

		RETURN_IF_ERROR(m_ch->InitChrome(m_window, target_window, left_chrome_width, top_chrome_height, right_chrome_width, bottom_chrome_height));

		// This is needed because m_window->Init will SetZ to top. We might need to fix this by avoiding that instead.
		target_window->Raise();

		return OpStatus::OK;
	}
	void SetZ(MDE_Z z)
	{
		((MDE_OpWindow*)m_window)->GetMDEWidget()->SetZ(z);
	}
	void SetVisibility(bool visible)
	{
		m_window->Show(visible);
	}
	void SetRect(const MDE_RECT &rect)
	{
		BOOL resized = m_rect.w != rect.w || m_rect.h != rect.h;

		m_rect = rect;
		m_window->SetOuterPos(m_rect.x, m_rect.y);
		m_window->SetMaximumInnerSize(m_rect.w, m_rect.h);
		m_window->SetOuterSize(m_rect.w, m_rect.h);

		if (resized)
			m_ch->OnResize(m_rect.w, m_rect.h);
	}
	void Close(bool delayed = false)
	{
		if (delayed)
			g_main_message_handler->PostMessage(MSG_MDE_CHROMEWINDOW_CLOSE, MH_PARAM_1(this), 0);
		else
		{
			OP_DELETE(m_ch);
			m_ch = NULL;
			m_window->Close(FALSE);
			OP_DELETE(this);
		}
	}
	void SetIconAndTitle(OpBitmap* icon, uni_char* title)
	{
		m_ch->SetIconAndTitle(icon, title);
	}
public:
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		switch (msg) {
		case MSG_MDE_CHROMEWINDOW_CLOSE:
			Close(false);
			break;
		}
	}
	int left_chrome_width;
	int right_chrome_width;
	int top_chrome_height;
	int bottom_chrome_height;
	MDE_RECT m_rect;
	OpWindow *m_window;
	MDEChromeHandler *m_ch;
};
#endif // MDE_OPWINDOW_CHROME_SUPPORT

MDE_OpWindow::MDE_OpWindow()
{
	mdeWidget = NULL;
	mdeWidgetShadow = NULL;
	windowListener = NULL;
	minInnerWidth = 0;
	minInnerHeight = 0;
	maxInnerWidth = 0;
	maxInnerHeight = 0;
	cursor = CURSOR_DEFAULT_ARROW;
#ifndef GOGI
	cursorListener = NULL;
#endif
#ifdef MDE_DS_MODE
	dsMode = 0;
#endif // MDE_DS_MODE
	m_state = RESTORED;
	m_state_before_minimize = RESTORED;
	m_allow_as_active = TRUE;
	cache_bitmap = NULL;

	m_title = NULL;
	m_icon = NULL;

	m_restored_rect.Set(30, 30, 300, 200);

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	chromeWin = NULL;
#endif // MDE_OPWINDOW_CHROME_SUPPORT
	m_active = FALSE;
}

/*virtual*/
MDE_OpWindow::~MDE_OpWindow()
{
#ifdef MDE_DS_MODE
	OP_DELETE(dsMode);
#endif // MDE_DS_MODE
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	OP_DELETE(chromeWin);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
	if (mdeWidget)
	{
		mdeWidget->SetOpWindow(NULL); // to prevent call to WidgetDeleted
		OP_DELETE(mdeWidget);
		mdeWidget = NULL;
	}
	OP_DELETE(mdeWidgetShadow);
	mdeWidgetShadow = NULL;
	OP_DELETE(cache_bitmap);
	op_free(m_title);
}

/*virtual*/
OP_STATUS
MDE_OpWindow::Init(Style style, OpTypedObject::Type type, OpWindow* parent_window,
					OpView* parent_view, void* native_handle, UINT32 effect)
{
	m_parent_window = parent_window;
	m_parent_view = parent_view;

	MDE_View* parentWidget = GetParentWidget(parent_window, parent_view);
	if (!parentWidget && native_handle)
        parentWidget = ((MDE_Screen*)native_handle)->GetOperaView();

#ifdef VEGA_OPPAINTER_SUPPORT
	// Creating the OpPainter is delayed to here since the screen can be created
	// before opera is initialized and we cannot safly use libvega before that
	if (native_handle && !((MDE_Screen*)native_handle)->GetVegaPainter())
		if (((MDE_Screen*)native_handle)->IsType("MDE_GENERICSCREEN"))
			RETURN_IF_ERROR(((MDE_GenericScreen*)native_handle)->CreateVegaPainter());
		else
			return OpStatus::ERR;
#endif // VEGA_OPPAINTER_SUPPORT
	this->style = style;
	this->effect = effect;
	this->transparency = 255;
	this->ignore_mouse_input = FALSE;

	switch (style)
	{
		case STYLE_POPUP:
			// always create popups as toplevel windows
			if (parentWidget)
			{
				parentWidget = GetScreenView(parentWidget, parent_window, parent_view);
			}
			else
			{
				OP_ASSERT(FALSE); // no possible parent for popup
				return OpStatus::ERR; // we could fall back on the first of the screens -- but we probably hide more errors that way
			}
		case STYLE_UNKNOWN:
		case STYLE_DESKTOP:
		case STYLE_TOOLTIP:
		case STYLE_NOTIFIER:
		case STYLE_MODAL_DIALOG:
		case STYLE_MODELESS_DIALOG:
		case STYLE_BLOCKING_DIALOG:
		case STYLE_CONSOLE_DIALOG:
		case STYLE_CHILD:
		case STYLE_WORKSPACE:
		case STYLE_EMBEDDED:
		case STYLE_ADOPT:
		default:
			// FIXME: the null in this case should probably be a painter to paint the window
#ifdef VEGA_OPPAINTER_SUPPORT
			mdeWidget = OP_NEW(MDE_Widget, (OpRect(0,0,0,0)));
#else
			mdeWidget = OP_NEW(MDE_Widget, (NULL, OpRect(0,0,0,0)));
#endif
			if (mdeWidget)
			{
				mdeWidget->SetVisibility(false);
				mdeWidget->SetVisibilityListener(this);
			}
			break;
	}

	if (!mdeWidget)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	UpdateTransparency();

	if (parentWidget)
	{
		parentWidget->AddChild(mdeWidget);

		maxInnerWidth = parentWidget->m_rect.w;
		maxInnerHeight = parentWidget->m_rect.h;
	}

	if (effect & OpWindow::EFFECT_DROPSHADOW)
	{
			// FIXME: the null in this case should probably be a painter to paint the window
#ifdef VEGA_OPPAINTER_SUPPORT
		mdeWidgetShadow = OP_NEW(MDE_Widget, (OpRect(0,0,0,0)));
#else // VEGA_OPPAINTER_SUPPORT
		mdeWidgetShadow = OP_NEW(MDE_Widget, (NULL, OpRect(0,0,0,0)));
#endif // !VEGA_OPPAINTER_SUPPORT
		if (mdeWidgetShadow)
		{
			mdeWidgetShadow->SetOpWindow(this);
			mdeWidgetShadow->SetIsShadow(true, false);
			mdeWidgetShadow->SetTransparent(TRUE);
			if (parentWidget)
				parentWidget->AddChild(mdeWidgetShadow);
		}
	}

	mdeWidget->SetOpWindow(this);

	// FIXME: This might not be needed if style is implemented properly
	if (mdeWidgetShadow)
		mdeWidgetShadow->SetZ(MDE_Z_TOP);
	mdeWidget->SetZ(MDE_Z_TOP);

	return OpStatus::OK;
}

OpBitmap *MDE_OpWindow::GetCacheBitmap()
{
	if (!cache_bitmap)
		return NULL;
	int w = mdeWidget->m_rect.w;
	int h = mdeWidget->m_rect.h;
	if (cache_bitmap->Width() != (UINT32)w || cache_bitmap->Height() != (UINT32)h)
	{
		OP_DELETE(cache_bitmap);
		cache_bitmap = NULL;
	}
	if (!cache_bitmap && w > 0 && h > 0)
		OpBitmap::Create(&cache_bitmap, w, h, FALSE, mdeWidget->m_is_transparent, 0, 0, TRUE);
	return cache_bitmap;
}

void MDE_OpWindow::SetCached(BOOL cached)
{
	if (cached)
	{
		if (cache_bitmap)
			return;
		OpBitmap::Create(&cache_bitmap, mdeWidget->m_rect.w, mdeWidget->m_rect.h, FALSE, mdeWidget->m_is_transparent, 0, 0, TRUE);
		mdeWidget->InvalidateCache();
	}
	else
	{
		if (!cache_bitmap)
			return;
		OP_DELETE(cache_bitmap);
		cache_bitmap = NULL;
	}
}

void MDE_OpWindow::UpdateTransparency()
{
	bool transp = false;
	if (transparency != 255)
		transp = true;
	else if (style == STYLE_GADGET || style == STYLE_OVERLAY || style == STYLE_TRANSPARENT || style == STYLE_BITMAP_WINDOW
#ifdef PI_ANIMATED_WINDOWS
		|| style == STYLE_ANIMATED
#endif // PI_ANIMATED_WINDOWS
		|| (effect & OpWindow::EFFECT_TRANSPARENT))
		transp = true;

	if (mdeWidget->m_is_transparent != transp)
	{
		OP_DELETE(cache_bitmap);
		cache_bitmap = NULL;
	}
	mdeWidget->SetTransparent(transp);
}

/*virtual*/
MDE_View* MDE_OpWindow::GetScreenView(const MDE_View *parentWidget, const OpWindow* parent_window, const OpView* parent_view) const
{
	MDE_View* root = (MDE_View*) parentWidget;
	while (root->m_parent)
		root = root->m_parent;
	return root;
}


/*virtual*/
MDE_View* MDE_OpWindow::GetParentWidget(const OpWindow* parent_window, const OpView* parent_view) const
{
	MDE_View *parentWidget = NULL;

	if (parent_window)
        parentWidget = ((MDE_OpWindow*)parent_window)->GetMDEWidget();
    else if (parent_view)
        parentWidget = ((MDE_OpView*)parent_view)->GetMDEWidget();

	return parentWidget;
}



/*virtual*/
void
MDE_OpWindow::SetParent(OpWindow* parent_window, OpView* parent_view, BOOL behind)
{
	OP_ASSERT(mdeWidget);
	if (style == STYLE_POPUP)
	{
		return;
	}

	m_parent_window = parent_window;
	m_parent_view = parent_view;

	MDE_View *parentWidget = GetParentWidget(parent_window, parent_view);

#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
	{
		dsMode->m_parent->RemoveChild(dsMode);
		parentWidget->AddChild(dsMode);
	}
	else
#endif // MDE_DS_MODE
	{
		mdeWidget->m_parent->RemoveChild(mdeWidget);
		parentWidget->AddChild(mdeWidget);
	}
	maxInnerWidth = parentWidget->m_rect.w;
	maxInnerHeight = parentWidget->m_rect.h;

	if (m_state == MAXIMIZED)
	{
		// Maximize again to adapt to the new parents size.
		Maximize();
	}
	else if (m_state == RESTORED)
	{
		// Maximize and restore again, so the restored chrome will be recreated in the new parent.
		Maximize();
		Restore();
	}
}

/*virtual*/
void
MDE_OpWindow::SetWindowListener(OpWindowListener *windowlistener)
{
	OP_ASSERT(mdeWidget || !windowlistener);
	this->windowListener = windowlistener;
}

MDE_OpWindow * MDE_OpWindow::GetBottomMostWindow()
{
	if (!mdeWidget->m_parent)
		return 0;

	for (MDE_View* v = mdeWidget->m_parent->m_first_child; v; v = v->m_next)
	{
		if (v->IsType("MDE_Widget"))
		{
			MDE_Widget* w = static_cast<MDE_Widget*>(v);
			if (OpWindow* ow = w->GetOpWindow())
			{
				MDE_OpWindow* mw = static_cast<MDE_OpWindow*>(ow);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
				if (!mw->IsChromeWindow())
#endif // MDE_OPWINDOW_CHROME_SUPPORT
					return mw;
			}
		}
	}
	return 0;
}

MDE_OpWindow* MDE_OpWindow::NextWindow()
{
	for (MDE_View* v = mdeWidget->m_next; v; v = v->m_next)
		if (v->IsType("MDE_Widget"))
		{
			MDE_Widget* w = static_cast<MDE_Widget*>(v);
			if (OpWindow* ow = w->GetOpWindow())
			{
				MDE_OpWindow* mw = static_cast<MDE_OpWindow*>(ow);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
				if (!mw->IsChromeWindow())
#endif // MDE_OPWINDOW_CHROME_SUPPORT
					return mw;
			}
		}
	return 0;
}

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
/**
   returns TRUE if view is an MDE_Widget with an associated MDE_OpWindow that has chrome
*/
static
BOOL HasDecorations(MDE_View* view)
{
	if (view && view->IsType("MDE_Widget"))
	{
		MDE_Widget* widget = static_cast<MDE_Widget*>(view);
		if (widget->GetOpWindow())
		{
			MDE_OpWindow* window = static_cast<MDE_OpWindow*>(widget->GetOpWindow());
			return window->HasChrome();
		}
	}
	return FALSE;
}
void MDE_OpWindow::LowerWindow()
{
	LowerWindowOneStep();
	/* Now that this window has been lowered past another window,
	 * check if that other window has decorations. In that case, we
	 * need to lower this new window past the decorations too.
	 *
	 * (mdeWidget->m_next is the window above mdeWidget in the
	 * Z-order.  That's why we have to lower this window
	 * before checking what mdeWidget->m_next is.)
	 */
	if (HasDecorations(mdeWidget->m_next))
		LowerWindowOneStep();
}
void
MDE_OpWindow::LowerWindowOneStep()
{
	if (mdeWidgetShadow)
		mdeWidgetShadow->SetZ(MDE_Z_LOWER);
	if (chromeWin)
		chromeWin->SetZ(MDE_Z_LOWER);
	mdeWidget->SetZ(MDE_Z_LOWER);
}
#else // MDE_OPWINDOW_CHROME_SUPPORT
void
MDE_OpWindow::LowerWindow()
{
	if (mdeWidgetShadow)
		mdeWidgetShadow->SetZ(MDE_Z_LOWER);
	mdeWidget->SetZ(MDE_Z_LOWER);
}
#endif // MDE_OPWINDOW_CHROME_SUPPORT

void MDE_OpWindow::MoveToBottomOfWindowStack()
{
	MDE_OpWindow* bottomwindow = 0;
	MDE_OpWindow * candidate = GetBottomMostWindow();
	while (candidate && !bottomwindow)
	{
		if (candidate->m_state != MINIMIZED)
			bottomwindow = candidate;
		else
			candidate = candidate->NextWindow();
	}

	/* If this assert fails, I have misunderstood something and the
	 * test below is probably wrong.
	 */
	OP_ASSERT(mdeWidget->GetOpWindow() == this);
	// Now move the window below the current bottom-most visible window.
	if (bottomwindow && bottomwindow != this)
	{
		/* 'prev' is used to break out of the loop if we fail to find
		 * bottomwindow.
		 */
		MDE_View * prev = bottomwindow->mdeWidget;
		while (NextWindow() != bottomwindow && mdeWidget->m_next != prev)
		{
			prev = mdeWidget->m_next;
			LowerWindow();
		}
	}
}

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
BOOL MDE_OpWindow::IsChromeWindow()
{
	MDE_View * v = mdeWidget->m_next;
	if (!v)
		return FALSE;
	if (!v->IsType("MDE_Widget"))
		return FALSE;
	MDE_Widget* w = static_cast<MDE_Widget*>(v);
	OpWindow* ow = w->GetOpWindow();
	if (!ow)
		return FALSE;
	MDE_ChromeWindow * next_chrome = static_cast<MDE_OpWindow*>(ow)->chromeWin;
	if (!next_chrome)
		return FALSE;
	return next_chrome->m_window == this;
};
#endif


/*virtual*/
void
MDE_OpWindow::SetDesktopPlacement(const OpRect& rect, State state, BOOL inner, BOOL behind, BOOL center)
{
	State old_state = m_state;
	if (state == MINIMIZED && m_state != MINIMIZED)
		m_state_before_minimize = m_state;

	m_state = state;
	if (state == MAXIMIZED || state == MINIMIZED)
	{
		if (mdeWidgetShadow && mdeWidgetShadow->IsFullscreenShadow())
		{
			mdeWidgetShadow->m_parent->RemoveChild(mdeWidgetShadow);
			OP_DELETE(mdeWidgetShadow);
			mdeWidgetShadow = NULL;
		}
		if (state == MAXIMIZED)
			SetWidgetRect(MDE_MakeRect(0,0,mdeWidget->m_parent->m_rect.w, mdeWidget->m_parent->m_rect.h));
		else
			SetWidgetRect(MDE_MakeRect(rect.x, rect.y, rect.width, rect.height));
	}
	else
	{
		MDE_RECT r = MDE_MakeRect(rect.x, rect.y, rect.width, rect.height);
		/*if (!mdeWidgetShadow &&
			((effect & OpWindow::EFFECT_DROPSHADOW) || (style == STYLE_DESKTOP)))
		{
			// FIXME: the null in this case should probably be a painter to paint the window
#ifdef VEGA_OPPAINTER_SUPPORT
			mdeWidgetShadow = OP_NEW(MDE_Widget, (OpRect(0,0,0,0)));
#else // VEGA_OPPAINTER_SUPPORT
			mdeWidgetShadow = OP_NEW(MDE_Widget, (NULL, OpRect(0,0,0,0)));
#endif // !VEGA_OPPAINTER_SUPPORT
			if (mdeWidgetShadow)
			{
				mdeWidgetShadow->SetOpWindow(this);
				mdeWidgetShadow->SetIsShadow(true, true);
				mdeWidgetShadow->SetTransparent(TRUE);
				mdeWidget->m_parent->AddChild(mdeWidgetShadow);
			}
		}*/
		if (center)
		{
			r.x = (mdeWidget->m_parent->m_rect.w-rect.width+1)/2;
			r.y = (mdeWidget->m_parent->m_rect.y-rect.height+1)/2;
		}
		SetWidgetRect(r);
	}
	if (state == MINIMIZED)
	{
		// Make invisible and deactivate
		SetWidgetVisibility(false);

		if (state != old_state)
			DeactivateAndActivateTopmost(TRUE);

		// Put window at bottom (so futher minimizing will not pick this one to activate next)
		if (mdeWidgetShadow)
			mdeWidgetShadow->SetZ(MDE_Z_BOTTOM);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
		if (chromeWin)
			chromeWin->SetZ(MDE_Z_BOTTOM);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
		mdeWidget->SetZ(MDE_Z_BOTTOM);
	}
	else
	{
		SetWidgetVisibility(true);
		if (windowListener)
		{
			windowListener->OnResize(mdeWidget->m_rect.w, mdeWidget->m_rect.h);
			windowListener->OnShow(TRUE);
		}
	}
	if (behind)
	{
		MoveToBottomOfWindowStack();
	}
	if (!mdeWidget->m_next && state != MINIMIZED)
		Activate();
}

/*virtual*/
void
MDE_OpWindow::GetDesktopPlacement(OpRect& rect, State& state)
{
	rect.x = mdeWidget->m_rect.x;
	rect.y = mdeWidget->m_rect.y;
	rect.width = mdeWidget->m_rect.w;
	rect.height = mdeWidget->m_rect.h;
	state = m_state;
}

/*virtual*/
void
MDE_OpWindow::Restore()
{
	SetDesktopPlacement(m_restored_rect, RESTORED);
}

/*virtual*/
void
MDE_OpWindow::Minimize()
{
	SetDesktopPlacement(OpRect(mdeWidget->m_rect.x,mdeWidget->m_rect.y,mdeWidget->m_rect.w,mdeWidget->m_rect.h), MINIMIZED);
}

/*virtual*/
void
MDE_OpWindow::Maximize()
{
	SetDesktopPlacement(OpRect(0,0,mdeWidget->m_rect.w,mdeWidget->m_rect.h), MAXIMIZED);
}

/*virtual*/
void
MDE_OpWindow::Fullscreen()
{
	OP_ASSERT(0);
}

/*virtual*/
void
MDE_OpWindow::LockUpdate(BOOL lock)
{
	OP_ASSERT(0);
}

/*virtual*/
void
MDE_OpWindow::Show(BOOL show)
{
	if (mdeWidget->m_is_visible == (show ? true : false))
		return;
#ifdef MDE_DS_MODE
	if (dsMode)
		dsMode->SetVisibility(show ? true : false);
#endif // MDE_DS_MODE
	SetWidgetVisibility(show ? true : false);
	if (windowListener)
		windowListener->OnShow(show);

#if defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_OPPAINTER_ANIMATIONS)
	/* Disabled for now. Should be done in a different position since we don't want *all* windows to animate.
	if (mdeWidget && mdeWidget->m_screen && !MDE_RectIsEmpty(mdeWidget->m_rect))
	{
		VEGAOpPainter* painter = ((MDE_GenericScreen*)mdeWidget->m_screen)->GetVegaPainter();
		painter->prepareAnimation(OpRect(mdeWidget->m_rect.x, mdeWidget->m_rect.y, mdeWidget->m_rect.w, mdeWidget->m_rect.h), show?VEGAOpPainter::ANIM_SHOW_WINDOW:VEGAOpPainter::ANIM_HIDE_WINDOW);
		painter->startAnimation(OpRect(mdeWidget->m_rect.x, mdeWidget->m_rect.y, mdeWidget->m_rect.w, mdeWidget->m_rect.h));
	}*/
#endif // VEGA_OPPAINTER_SUPPORT && VEGA_OPPAINTER_ANIMATIONS
	if (!mdeWidget->m_next && show)
		Activate();
}

/*virtual*/
void
MDE_OpWindow::SetFocus(BOOL focus)
{
	// FIXME: is this correct? (focus stuff for mde)
	OP_ASSERT(mdeWidget);

	if (focus)
	{
		Raise();
	}

	if (m_allow_as_active)
	{
		NotifyOnActivate(focus, this);
	}
#if defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_OPPAINTER_ANIMATIONS)
	if (mdeWidget && mdeWidget->m_screen)
	{
		VEGAOpPainter* painter = ((MDE_GenericScreen*)mdeWidget->m_screen)->GetVegaPainter();
		painter->prepareAnimation(OpRect(mdeWidget->m_rect.x, mdeWidget->m_rect.y, mdeWidget->m_rect.w, mdeWidget->m_rect.h), VEGAOpPainter::ANIM_SWITCH_TAB);
		painter->startAnimation(OpRect(mdeWidget->m_rect.x, mdeWidget->m_rect.y, mdeWidget->m_rect.w, mdeWidget->m_rect.h));
	}
#endif // VEGA_OPPAINTER_SUPPORT && VEGA_OPPAINTER_ANIMATIONS
}

/*virtual*/
void
MDE_OpWindow::CloseInternal()
{
	if (mdeWidget->m_parent)
		mdeWidget->m_parent->RemoveChild(mdeWidget);
	if (mdeWidgetShadow && mdeWidgetShadow->m_parent)
		mdeWidgetShadow->m_parent->RemoveChild(mdeWidgetShadow);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
	{
		chromeWin->Close();
		chromeWin = NULL;
	}
#endif // MDE_OPWINDOW_CHROME_SUPPORT

#ifdef MDE_DS_MODE
	if (dsMode)
		dsMode->SetVisibility(FALSE);
#endif // MDE_DS_MODE
}

void
MDE_OpWindow::Close(BOOL user_initiated)
{
	CloseInternal();

	if (windowListener)
	{
		windowListener->OnClose(user_initiated);
	}
}

void
MDE_OpWindow::WidgetDeleted()
{
	CloseInternal();

	mdeWidget = NULL;

	if (windowListener)
	{
		windowListener->OnClose(FALSE);
	}
}

/*virtual*/
void
MDE_OpWindow::GetOuterSize(UINT32* width, UINT32* height)
{
	GetInnerSize(width, height);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
	{
		*width += chromeWin->left_chrome_width+chromeWin->right_chrome_width;
		*height += chromeWin->top_chrome_height+chromeWin->bottom_chrome_height;
	}
#endif // MDE_OPWINDOW_CHROME_SUPPORT
}

/*virtual*/
void
MDE_OpWindow::SetOuterSize(UINT32 width, UINT32 height)
{
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
	{
		width -= chromeWin->left_chrome_width+chromeWin->right_chrome_width;
		height -= chromeWin->top_chrome_height+chromeWin->bottom_chrome_height;
	}
#endif // MDE_OPWINDOW_CHROME_SUPPORT
	SetInnerSize(width, height);
}

/*virtual*/
void
MDE_OpWindow::GetInnerSize(UINT32* width, UINT32* height)
{
	OP_ASSERT(mdeWidget);
#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
	{
		*width = dsMode->m_rect.w;
		*height = dsMode->m_rect.h;
	}
	else
#endif // MDE_DS_MODE
	{
		*width = MAX(mdeWidget->m_rect.w,0);
		*height = MAX(mdeWidget->m_rect.h,0);
	}
}

/*virtual*/
BOOL
MDE_OpWindow::SetWidgetRect(const MDE_RECT& mr)
{
	BOOL changed = !MDE_RectIsIdentical(mr, mdeWidget->m_rect);
#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
	{
		dsMode->SetRect(mr);
		dsMode->Resize(mr.w,mr.h);
		changed = TRUE;
	}
	else
#endif // MDE_DS_MODE
		mdeWidget->SetRect(mr);
	MDE_RECT view_rect = mr;

	if (m_state == RESTORED)
		m_restored_rect.Set(mr.x, mr.y, mr.w, mr.h);

#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	BOOL need_chrome = (m_state != MAXIMIZED);
	if (!mdeWidget->m_parent ||
			(mr.x == 0 && mr.y == 0 &&
			mr.w == mdeWidget->m_parent->m_rect.w &&
			mr.h == mdeWidget->m_parent->m_rect.h))
		need_chrome = FALSE;

	// Delete chrome if we maximize
	if (chromeWin && !need_chrome)
	{
		chromeWin->Close(true);
		chromeWin = NULL;
	}
	// Add chrome if we restore
	else if (!chromeWin && style == STYLE_DESKTOP && need_chrome)
	{
		chromeWin = OP_NEW(MDE_ChromeWindow, ());
		if (chromeWin && OpStatus::IsSuccess(chromeWin->Init(this)))
			chromeWin->SetIconAndTitle(m_icon, m_title);
		else
		{
			OP_DELETE(chromeWin);
			chromeWin = NULL;
		}
	}
	if (chromeWin)
	{
		view_rect.x -= chromeWin->left_chrome_width;
		view_rect.y -= chromeWin->top_chrome_height;
		view_rect.w += chromeWin->left_chrome_width+chromeWin->right_chrome_width;
		view_rect.h += chromeWin->top_chrome_height+chromeWin->bottom_chrome_height;
		chromeWin->SetRect(view_rect);
	}
#endif // MDE_OPWINDOW_CHROME_SUPPORT
	if (mdeWidgetShadow)
	{
		MDE_RECT shadow_rect = view_rect;
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
		if (chromeWin)
		{
			shadow_rect.y += chromeWin->top_chrome_height - 1;
			shadow_rect.h -= chromeWin->top_chrome_height - 1;
		}
#endif
		if (mdeWidgetShadow->IsFullscreenShadow() && mdeWidgetShadow->m_parent)
		{
			shadow_rect.x = 0;
			shadow_rect.y = 0;
			shadow_rect.w = mdeWidgetShadow->m_parent->m_rect.w;
			shadow_rect.h = mdeWidgetShadow->m_parent->m_rect.h;
		}
		else
		{
			shadow_rect.x += 5;
			shadow_rect.y += 5;
		}
		mdeWidgetShadow->SetRect(shadow_rect);
	}
	return changed;
}

void MDE_OpWindow::SetWidgetVisibility(bool vis)
{
	mdeWidget->SetVisibility(vis);
	if (mdeWidgetShadow)
		mdeWidgetShadow->SetVisibility(vis);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
		chromeWin->SetVisibility(vis);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
}

/*virtual*/
void
MDE_OpWindow::SetInnerSize(UINT32 width, UINT32 height)
{
	if (m_state != RESTORED && !(m_state == MAXIMIZED && width == (UINT32)mdeWidget->m_parent->m_rect.w && height == (UINT32)mdeWidget->m_parent->m_rect.h))
		return;

	MDE_RECT mr;
	OP_ASSERT(mdeWidget);

	/* Treat maxInnerWidth == 0 as maxInnerWidth == infinite */
	if (maxInnerWidth && width > maxInnerWidth)
	{
		width = maxInnerWidth;
	}

	/* Treat maxInnerHeight == 0 as maxInnerHeight == infinite */
	if (maxInnerHeight && height > maxInnerHeight)
	{
		height = maxInnerHeight;
	}

	if (width < minInnerWidth)
	{
		width = minInnerWidth;
	}

	if (height < minInnerHeight)
	{
		height = minInnerHeight;
	}
#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
		mr = dsMode->m_rect;
	else
#endif // MDE_DS_MODE
		mr = mdeWidget->m_rect;
	mr.w = width;
	mr.h = height;

	if (!SetWidgetRect(mr))
		return;

	for (MDE_View* w = mdeWidget->m_first_child; w; w = w->m_next)
	{
		MDE_OpWindow *child_window = NULL;

		if (w->IsType("MDE_Widget"))
			child_window = (MDE_OpWindow*)((MDE_Widget*)w)->GetOpWindow();

		if (child_window)
			child_window->SetMaximumInnerSize(width, height);

		if (child_window && child_window->m_state == MAXIMIZED)
		{
			child_window->SetInnerSize(width, height);
			child_window->SetInnerPos(0, 0);
		}
		else if (child_window)
		{
			if (child_window->mdeWidgetShadow && child_window->mdeWidgetShadow->IsFullscreenShadow())
			{
				MDE_RECT shadow_rect;
				shadow_rect.x = 0;
				shadow_rect.y = 0;
				shadow_rect.w = mdeWidget->m_rect.w;
				shadow_rect.h = mdeWidget->m_rect.h;
				child_window->mdeWidgetShadow->SetRect(shadow_rect);
			}
		}
	}

	if (windowListener)
		windowListener->OnResize(mdeWidget->m_rect.w, mdeWidget->m_rect.h);
}

/*virtual*/
void
MDE_OpWindow::GetOuterPos(INT32* x, INT32* y)
{
	GetInnerPos(x, y);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
	{
		*x -= chromeWin->left_chrome_width;
		*y -= chromeWin->top_chrome_height;
	}
#endif // MDE_OPWINDOW_CHROME_SUPPORT
}

/*virtual*/
void
MDE_OpWindow::SetOuterPos(INT32 x, INT32 y)
{
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
	{
		x += chromeWin->left_chrome_width;
		y += chromeWin->top_chrome_height;
	}
#endif // MDE_OPWINDOW_CHROME_SUPPORT
	SetInnerPos(x, y);
}

/*virtual*/
void
MDE_OpWindow::GetInnerPos(INT32* x, INT32* y)
{
	OP_ASSERT(mdeWidget);
#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
	{
		*x = dsMode->m_rect.x;
		*y = dsMode->m_rect.y;
	}
	else
#endif // MDE_DS_MODE
	{
		*x = mdeWidget->m_rect.x;
		*y = mdeWidget->m_rect.y;
	}
}

/*virtual*/
void
MDE_OpWindow::SetInnerPos(INT32 x, INT32 y)
{
	if (m_state != RESTORED && !(m_state == MAXIMIZED && x == 0 && y == 0))
		return;

	MDE_RECT mr;
	OP_ASSERT(mdeWidget);
#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
		mr = dsMode->m_rect;
	else
#endif // MDE_DS_MODE
		mr = mdeWidget->m_rect;
	mr.x = x;
	mr.y = y;
	SetWidgetRect(mr);
}

/*virtual*/
void
MDE_OpWindow::SetMinimumInnerSize(UINT32 width, UINT32 height)
{
	minInnerWidth = width;
	minInnerHeight = height;
}

/*virtual*/
void
MDE_OpWindow::SetMaximumInnerSize(UINT32 width, UINT32 height)
{
	maxInnerWidth = width;
	maxInnerHeight = height;
}

/*virtual*/
void
MDE_OpWindow::GetMargin(INT32* left_margin, INT32* top_margin, INT32* right_margin, INT32* bottom_margin)
{
	OP_ASSERT(left_margin && top_margin && right_margin && bottom_margin);
	(*left_margin) = 0;
	(*top_margin) = 0;
	(*right_margin) = 0;
	(*bottom_margin) = 0;
}

/*virtual*/
void
MDE_OpWindow::SetTransparency(INT32 transparency)
{
	if (this->transparency != transparency)
	{
		this->transparency = transparency;
		UpdateTransparency();
		Redraw();
	}
}

/*virtual*/
void
MDE_OpWindow::SetWorkspaceMinimizedVisibility(BOOL workspace_minimized_visibility)
{
	OP_ASSERT(0);
}

/*virtual*/
void MDE_OpWindow::SetSkin(const char* skin)
{
	OP_ASSERT(0);
}

/*virtual*/
void
MDE_OpWindow::Redraw()
{
	// I should probably update all children too
	mdeWidget->Invalidate(MDE_MakeRect(0,0,mdeWidget->m_rect.w,mdeWidget->m_rect.h), true);
	if (mdeWidgetShadow)
		mdeWidgetShadow->Invalidate(MDE_MakeRect(0,0,mdeWidgetShadow->m_rect.w,mdeWidgetShadow->m_rect.h));
}

/*virtual*/
OpWindow::Style
MDE_OpWindow::GetStyle()
{
	return style;
}

/*virtual*/
void
MDE_OpWindow::Raise()
{
	OP_ASSERT(mdeWidget);

#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
		dsMode->SetZ(MDE_Z_TOP);
	else
#endif // MDE_DS_MODE
	{
		if (mdeWidgetShadow)
			mdeWidgetShadow->SetZ(MDE_Z_TOP);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
		if (chromeWin)
			chromeWin->SetZ(MDE_Z_TOP);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
		mdeWidget->SetZ(MDE_Z_TOP);
	}
}

/*virtual*/
void
MDE_OpWindow::Lower()
{
	OP_ASSERT(mdeWidget);

#ifdef MDE_DS_MODE
	if (dsMode && dsMode->IsDSEnabled())
		dsMode->SetZ(MDE_Z_BOTTOM);
	else
#endif // MDE_DS_MODE
	{
		mdeWidget->SetZ(MDE_Z_BOTTOM);
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
		if (chromeWin)
			chromeWin->SetZ(MDE_Z_BOTTOM);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
		if (mdeWidgetShadow)
			mdeWidgetShadow->SetZ(MDE_Z_BOTTOM);
	}

	DeactivateAndActivateTopmost();
}

void MDE_OpWindow::DeactivateAndActivateTopmost(BOOL minimizing)
{
	MDE_OpWindow* ow = NULL;
	if (mdeWidget->m_parent)
	{
		for (MDE_View* v = mdeWidget->m_parent->m_first_child; v; v = v->m_next)
		{
			if (v->IsType("MDE_Widget") && ((MDE_Widget*)v)->GetOpWindow() &&
					((MDE_OpWindow*)((MDE_Widget*)v)->GetOpWindow())->windowListener)
			{
				MDE_OpWindow* tw = (MDE_OpWindow*)((MDE_Widget*)v)->GetOpWindow();
				if (tw != this && tw->m_state != MINIMIZED)
				{
					ow = tw;
				}
			}
		}
	}
	if (ow)
		ow->Activate();
	else if (minimizing)
		NotifyOnActivate(FALSE, this);
}

/*virtual*/
void
MDE_OpWindow::Activate()
{
	if (IsActive())
		return;

	OP_ASSERT(mdeWidget);
	if (!m_allow_as_active)
		return;
	State old_state = m_state;
	if (m_state == MINIMIZED)
	{
		m_state = m_state_before_minimize;
		if (m_state == MAXIMIZED && mdeWidget->m_parent)
			SetWidgetRect(MDE_MakeRect(0,0,mdeWidget->m_parent->m_rect.w, mdeWidget->m_parent->m_rect.h));
		SetWidgetVisibility(true);
	}
	if (style != STYLE_POPUP)
	{
		// Find what we probably called OnActivate(TRUE) on before.
		// This isn't really a reliable way to do it.
		MDE_OpWindow* ow = NULL;
		for (MDE_View* v = mdeWidget->m_parent->m_first_child; v; v = v->m_next)
		{
			if (v->IsType("MDE_Widget") && ((MDE_Widget*)v)->GetOpWindow() && ((MDE_Widget*)v)->GetOpWindow() != this &&
				((MDE_OpWindow*)((MDE_Widget*)v)->GetOpWindow())->windowListener && 
				((MDE_OpWindow*)((MDE_Widget*)v)->GetOpWindow())->IsActive())
			{
				ow = (MDE_OpWindow*)((MDE_Widget*)v)->GetOpWindow();
			}
		}
		if (ow)
			NotifyOnActivate(FALSE, ow);
	}
	Raise();
	if (windowListener)
	{
		if (old_state == MINIMIZED)
		{
			windowListener->OnResize(mdeWidget->m_rect.w, mdeWidget->m_rect.h);
			windowListener->OnShow(TRUE);
		}
		NotifyOnActivate(TRUE, this);
	}
}

void
MDE_OpWindow::Deactivate()
{
	if (m_allow_as_active && IsActive())
	{
		NotifyOnActivate(FALSE, this);
	}
}

void
MDE_OpWindow::NotifyOnActivate(BOOL activate, MDE_OpWindow* ow)
{
	OP_ASSERT(ow);
	if (ow && ow->windowListener && activate != ow->IsActive())
	{
		ow->windowListener->OnActivate(activate);
		ow->SetActive(activate);
	}
}

/*virtual*/
void
MDE_OpWindow::Attention()
{
	OP_ASSERT(0);
}

#ifndef MOUSELESS
/*virtual*/
void
MDE_OpWindow::SetCursor(CursorType cursor)
{
#ifndef GOGI
	if (this->cursor == cursor)
		return;
#endif
	this->cursor = cursor;

	if (!mdeWidget->m_screen)
		return;

	// Check if we are currently hovering this view with the mouse. Otherwise we shouldn't change the cursor!
	MDE_View *hovered_view = mdeWidget->m_screen->GetViewAt(mdeWidget->m_screen->m_mouse_x, mdeWidget->m_screen->m_mouse_y, true);
	while (hovered_view)
	{
		if (hovered_view == mdeWidget)
			break;
		hovered_view = hovered_view->m_parent;
	}

#ifdef MDE_SUPPORT_DND
	if (!hovered_view && g_drag_manager->IsDragging())
	{
		// Check if we are currently hovering this view. Otherwise we shouldn't change the cursor!
		hovered_view = mdeWidget->m_screen->GetViewAt(mdeWidget->m_screen->m_drag_x, mdeWidget->m_screen->m_drag_y, true);
		while (hovered_view)
		{
			if (hovered_view == mdeWidget)
				break;
			hovered_view = hovered_view->m_parent;
		}
	}
#endif // MDE_SUPPORT_DND
	if (!hovered_view)
		return;

#ifdef GOGI
	((MDE_GenericScreen*)mdeWidget->m_screen)->SetCursor(this, cursor);
#else // !GOGI
	if (cursorListener)
		cursorListener->OnSetCursor(cursor, (MDE_Screen*)mdeWidget->m_screen);
#endif // GOGI
}

/*virtual*/
CursorType
MDE_OpWindow::GetCursor()
{
	return cursor;
}
#endif // !MOUSELESS

/*virtual*/
const uni_char*
MDE_OpWindow::GetTitle() const
{
	return m_title;
}

/*virtual*/
void
MDE_OpWindow::SetTitle(const uni_char* title)
{
	op_free(m_title);
	if (title)
		m_title = uni_strdup(title);
	else
		m_title = NULL;
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
		if (chromeWin)
			chromeWin->SetIconAndTitle(m_icon, m_title);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
}

/*virtual*/
void
MDE_OpWindow::SetIcon(OpBitmap* icon)
{
	m_icon = icon;
#ifdef MDE_OPWINDOW_CHROME_SUPPORT
	if (chromeWin)
		chromeWin->SetIconAndTitle(m_icon, m_title);
#endif // MDE_OPWINDOW_CHROME_SUPPORT
}

#ifndef GOGI
void
MDE_OpWindow::SetCursorListener(OpCursorListener *cl)
{
	cursorListener = cl;
}
#endif
#ifdef MDE_DS_MODE
/*virtual*/
OP_STATUS
MDE_OpWindow::SetDSMode(BOOL enable)
{
	if (enable && !dsMode)
	{
		dsMode = OP_NEW(MDE_DSMode, ());
		if (!dsMode)
			return OpStatus::ERR_NO_MEMORY;
	}
	if (dsMode)
	{
		RETURN_IF_ERROR(dsMode->EnableDS(enable, mdeWidget));
		if (windowListener)
			windowListener->OnResize(mdeWidget->m_rect.w, mdeWidget->m_rect.h);
	}
	return OpStatus::OK;
}
BOOL MDE_OpWindow::GetDSMode()
{
	return dsMode && dsMode->IsDSEnabled();
}
#endif // MDE_DS_MODE

void MDE_OpWindow::OnVisibilityChanged(bool vis)
{
	if (windowListener)
		windowListener->OnVisibilityChanged(vis);
}

void MDE_OpWindow::OnRectChanged(const MDE_RECT &old_rect)
{
	if (!windowListener)
		return;

	OP_ASSERT(mdeWidget->m_rect.x != old_rect.x || mdeWidget->m_rect.y != old_rect.y ||
			mdeWidget->m_rect.w != old_rect.w || mdeWidget->m_rect.h != old_rect.h);

	if (mdeWidget->m_rect.x != old_rect.x || mdeWidget->m_rect.y != old_rect.y)
		windowListener->OnMove();
	// FIX: Should probably move the OnResize handling here too! But maybe we should only send theese callbacks when the window has been made visible?
	//if (mdeWidget->m_rect.w != old_rect.w || mdeWidget->m_rect.h != old_rect.h)
	//	windowListener->OnResize(mdeWidget->m_rect.w, mdeWidget->m_rect.h);
}

bool MDE_OpWindow::PaintOnBitmap(OpBitmap *bitmap)
{
	if (OpPainter *painter = bitmap->GetPainter())
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		((VEGAOpPainter*)painter)->GetRenderer()->clear(0, 0, bitmap->Width(), bitmap->Height(), 0x00000000);
#endif
		bool ret = false;
		MDE_View *tmp = mdeWidget->m_first_child;
		while (tmp)
		{
			if (tmp->IsType("MDE_Widget"))
				ret |= ((MDE_Widget*)tmp)->Paint(painter);
			tmp = tmp->m_next;
		}
		bitmap->ReleasePainter();
		return ret;
	}
	return false;
}

OpBitmap *MDE_OpWindow::CreateBitmapFromWindow()
{
	int w = mdeWidget->m_rect.w, h = mdeWidget->m_rect.h;
	OpBitmap *bitmap = NULL;
	if (OpStatus::IsError(OpBitmap::Create(&bitmap, w, h, FALSE, TRUE, 0, 0, TRUE)))
		return NULL;

	if (PaintOnBitmap(bitmap))
		return bitmap;

	OP_DELETE(bitmap);
	return NULL;
}
