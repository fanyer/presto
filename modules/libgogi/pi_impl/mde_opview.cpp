/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libgogi/pi_impl/mde_oppainter.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/libgogi/pi_impl/mde_widget.h"
#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libgogi/pi_impl/mde_generic_screen.h"
#endif
#ifdef WIDGETS_IME_SUPPORT
# include "modules/libgogi/pi_impl/mde_ime_manager.h"
# include "modules/widgets/OpWidget.h"
# include "modules/display/vis_dev.h"
# include "modules/forms/piforms.h"
# include "modules/doc/frm_doc.h"
#endif

#ifdef WIDGETS_IME_SUPPORT
#include "modules/doc/html_doc.h"
#include "modules/widgets/OpEdit.h"
#endif

class MDE_ChildViewElm : public Link
{
public:
	MDE_ChildViewElm(MDE_OpView *v) : view(v){}
	MDE_OpView *view;
};

#ifndef MDE_DONT_CREATE_VIEW
/*static*/
OP_STATUS OpView::Create(OpView** new_opview, OpWindow* parentwindow)
{
	OP_ASSERT(new_opview);
	*new_opview = OP_NEW(MDE_OpView, ());
	if (*new_opview == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = ((MDE_OpView*)(*new_opview))->Init(parentwindow);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(*new_opview);
		*new_opview = 0;
	}

	return status;
}

/*static*/
OP_STATUS OpView::Create(OpView** new_opview, OpView* parentview)
{
	OP_ASSERT(new_opview);
	*new_opview = OP_NEW(MDE_OpView, ());
	if (*new_opview == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = ((MDE_OpView*)(*new_opview))->Init(parentview);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(*new_opview);
		*new_opview = 0;
	}
	return status;
}
#endif // !MDE_DONT_CREATE_VIEW

MDE_OpView::MDE_OpView()
{
	parentView = NULL;
	numPainters = 0;
#ifdef VEGA_OPPAINTER_SUPPORT
	bitmap_painter = NULL;
#endif
	painter = NULL;
	mdeWidget = NULL;
#ifdef MDE_DOUBLEBUF_SUPPORT
	doubleBuffer = FALSE;
	backbuf = NULL;
#endif
	parentWindow = NULL;
	parentView = NULL;
#ifdef WIDGETS_IME_SUPPORT
	currentIME = NULL;
	imestr = NULL;
	imeListener = NULL;
	spawning = FALSE;
	aborting = FALSE;
#endif // WIDGETS_IME_SUPPORT
}

/*virtual*/
MDE_OpView::~MDE_OpView()
{
#ifdef UTIL_LISTENERS
	// notify destruction
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnOpViewDeleted(this);
#endif // UTIL_LISTENERS

	// set all childrens parent to NULL
	while (MDE_ChildViewElm *elm = (MDE_ChildViewElm*)childViews.First())
	{
		elm->view->parentView = NULL;
		elm->Out();
		OP_DELETE(elm);
	}
	// remove this from the parents list of children
	if (parentView)
	{
		MDE_ChildViewElm *next_elm;
		for (MDE_ChildViewElm *elm = (MDE_ChildViewElm*)parentView->childViews.First(); elm != NULL; elm = next_elm)
		{
			next_elm = (MDE_ChildViewElm*)elm->Suc();
			if (elm->view == this)
			{
				elm->Out();
				OP_DELETE(elm);
			}
		}
	}
	OP_DELETE(mdeWidget);
#ifndef VEGA_OPPAINTER_SUPPORT
	OP_DELETE(painter);
#endif
#ifdef MDE_DOUBLEBUF_SUPPORT
	MDE_DeleteBuffer(backbuf);
#endif
#ifdef WIDGETS_IME_SUPPORT
	if (currentIME)
		g_mde_ime_manager->DestroyIME(currentIME);
	OP_DELETE(imestr);
#endif // WIDGETS_IME_SUPPORT
}

OP_STATUS MDE_OpView::Init(OpWindow *parentWindow)
{
	OP_ASSERT(parentWindow);
	MDE_Widget *parentWidget = ((MDE_OpWindow*)parentWindow)->GetMDEWidget();
	OP_ASSERT(parentWidget); // parentWindow doesn't have any mdeWidget. Probably parentWindow isn't initialized.
	if (!parentWidget) // if there's some less drastic solution, fix attempted dereferences below !
		return OpStatus::ERR;

	this->parentWindow = (MDE_OpWindow*)parentWindow;
#ifdef VEGA_OPPAINTER_SUPPORT
	mdeWidget = OP_NEW(MDE_Widget, (OpRect(0,0,0,0)));
#else
#ifdef MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	extern OpPainter* get_op_painter();
	painter = get_op_painter();
#else // MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	painter = OP_NEW(MDE_OpPainter, ());
#endif // MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	if (!painter)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	mdeWidget = OP_NEW(MDE_Widget, (painter, OpRect(0,0,0,0)));
#endif // VEGA_OPPAINTER_SUPPORT
	if (!mdeWidget)
	{
#ifndef VEGA_OPPAINTER_SUPPORT
		OP_DELETE(painter);
		painter = NULL;
#endif
		return OpStatus::ERR_NO_MEMORY;
	}
	parentWidget->AddChild(mdeWidget);
	mdeWidget->SetOpView(this);
	return OpStatus::OK;
}

OP_STATUS MDE_OpView::Init(OpView *parentView){
	MDE_ChildViewElm *elm;
	OP_ASSERT(parentView);
	// Set the parent view and add this as a child view to the parent
	elm = OP_NEW(MDE_ChildViewElm, (this));
	if (!elm)
		return OpStatus::ERR_NO_MEMORY;
	elm->Into(&((MDE_OpView*)parentView)->childViews);
	this->parentView = (MDE_OpView*)parentView;
	// make sure the number of locks match so we don't get negative lock-count

	MDE_Widget *parentWidget = ((MDE_OpView*)parentView)->GetMDEWidget();
#ifdef VEGA_OPPAINTER_SUPPORT
	mdeWidget = OP_NEW(MDE_Widget, (OpRect(0,0,0,0)));
#else

#ifdef MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	extern OpPainter* get_op_painter();
	painter = get_op_painter();
#else // MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	painter = OP_NEW(MDE_OpPainter, ());
#endif // MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	if (!painter)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	mdeWidget = OP_NEW(MDE_Widget, (painter, OpRect(0,0,0,0)));
#endif // MDE_PLATFORM_IMPLEMENTS_OPPAINTER
	if (!mdeWidget)
	{
#ifndef VEGA_OPPAINTER_SUPPORT
		OP_DELETE(painter);
		painter = NULL;
#endif
		return OpStatus::ERR_NO_MEMORY;
	}
	parentWidget->AddChild(mdeWidget);
	mdeWidget->SetOpView(this);
	return OpStatus::OK;
}

/*virtual*/
void MDE_OpView::UseDoublebuffer(BOOL doublebuffer)
{
#ifdef MDE_DOUBLEBUF_SUPPORT
	doubleBuffer = doublebuffer;
#endif
}

/*virtual*/
void MDE_OpView::ScrollRect(const OpRect &rect, INT32 dx, INT32 dy)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->ScrollRect(rect, dx, dy, false);
}

/*virtual*/
void MDE_OpView::Scroll(INT32 dx, INT32 dy, BOOL full_update)
{
	OP_ASSERT(mdeWidget);

	if (full_update)
	{
		UINT32 w, h;
		// Move all children..
		//for (MDE_View *c=mdeWidget->m_first_child; c!=NULL; c=c->m_next)
		//	c->SetRect(MDE_MakeRect(c->m_rect.x+dx, c->m_rect.y+dy,c->m_rect.w, c->m_rect.h));
		for (MDE_ChildViewElm *elm = (MDE_ChildViewElm*)childViews.First();
				elm != NULL; elm = (MDE_ChildViewElm*)elm->Suc())
		{
			INT32 x, y;
			elm->view->GetPos(&x,&y);
			x += dx;
			y += dy;
			elm->view->SetPos(x,y);
		}
		GetSize(&w,&h);
		Invalidate(OpRect(0,0,w,h));
	}
	else
	{
		mdeWidget->ScrollRect(OpRect(0,0,
							  mdeWidget->m_rect.w, mdeWidget->m_rect.h), dx, dy, true);
	}
}

/*virtual*/
void MDE_OpView::Sync()
{
#ifndef VEGA_OPPAINTER_SUPPORT
	if (numPainters == 0)
		mdeWidget->Validate(false);
#endif // !VEGA_OPPAINTER_SUPPORT
}


/*virtual*/
void MDE_OpView::LockUpdate(BOOL lock)
{
	mdeWidget->LockUpdate(lock==TRUE);
}

/*virtual*/
void MDE_OpView::SetPos(INT32 x, INT32 y)
{
	MDE_RECT mr;
	OP_ASSERT(mdeWidget);
	mr = mdeWidget->m_rect;
	mr.x = x;
	mr.y = y;
	mdeWidget->SetRect(mr);
}

/*virtual*/
void MDE_OpView::GetPos(INT32* x, INT32* y)
{
	OP_ASSERT(mdeWidget);
	*x = mdeWidget->m_rect.x;
	*y = mdeWidget->m_rect.y;
}

/*virtual*/
void MDE_OpView::SetSize(UINT32 w, UINT32 h)
{
	MDE_RECT mr;
	OP_ASSERT(mdeWidget);
	mr = mdeWidget->m_rect;
	mr.w = w;
	mr.h = h;

	mdeWidget->SetRect(mr);
}

/*virtual*/
void MDE_OpView::GetSize(UINT32* w, UINT32* h)
{
	OP_ASSERT(mdeWidget);
	*w = mdeWidget->m_rect.w;
	*h = mdeWidget->m_rect.h;
}

/*virtual*/
void MDE_OpView::SetVisibility(BOOL visible)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->SetVisibility(visible ? true : false);
}

/*virtual*/
BOOL MDE_OpView::GetVisibility()
{
	OP_ASSERT(mdeWidget);
	return mdeWidget->IsVisible();
}

/*virtual*/
OP_STATUS MDE_OpView::SetCustomOverlapRegion(OpRect *rects, INT32 num_rects)
{
	OP_ASSERT(mdeWidget);

	// Convert OpRect list into a MDE_Region
	MDE_Region rgn;
	for (int i = 0; i < num_rects; i++)
	{
		if (!rgn.AddRect(MDE_MakeRect(rects[i].x, rects[i].y, rects[i].width, rects[i].height)))
			return OpStatus::ERR_NO_MEMORY;
	}
	mdeWidget->SetCustomOverlapRegion(&rgn);
	return OpStatus::OK;
}

/*virtual*/
void MDE_OpView::SetAffectLowerRegions(BOOL affect_lower_regions)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->SetAffectLowerRegions(affect_lower_regions ? true : false);
}

#ifdef WIDGETS_IME_SUPPORT
/*virtual*/
void MDE_OpView::SetInputMethodListener(OpInputMethodListener* imlistener)
{
	imeListener = imlistener;
}
#endif // WIDGETS_IME_SUPPORT

/*virtual*/
void MDE_OpView::SetPaintListener(OpPaintListener* paintlistener)
{
	mdeWidget->SetPaintListener(paintlistener, this);
}

#ifndef MOUSELESS
/*virtual*/
void MDE_OpView::SetMouseListener(OpMouseListener* mouselistener)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->SetMouseListener(mouselistener, this);
}
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
/*virtual*/
void MDE_OpView::SetTouchListener(OpTouchListener* touchlistener)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->SetTouchListener(touchlistener, this);
}
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
/*virtual*/
void MDE_OpView::SetDragListener(OpDragListener* draglistener)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->SetDragListener(draglistener, this);
}

/*virtual*/
OpDragListener* MDE_OpView::GetDragListener() const
{
	OP_ASSERT(mdeWidget);
	return mdeWidget->GetDragListener();
}
#endif // DRAG_SUPPORT

/*virtual*/
ShiftKeyState MDE_OpView::GetShiftKeys()
{
#ifdef GOGI_MODULE_REQUIRED
	extern unsigned char gogi_get_shiftkeys();
	return gogi_get_shiftkeys();
#else
	return SHIFTKEY_NONE;
#endif
}

#ifndef MOUSELESS
/*virtual*/
BOOL MDE_OpView::GetMouseButton(MouseButton button)
{
	OP_ASSERT(mdeWidget);
	return mdeWidget->GetMouseButton(button);
}

#ifdef TOUCH_EVENTS_SUPPORT
/* virtual */
void MDE_OpView::SetMouseButton(MouseButton button, bool set)
{
	OP_ASSERT(mdeWidget);
	OP_ASSERT(button >= MOUSE_BUTTON_1 && button <= MOUSE_BUTTON_3);
	mdeWidget->SetMouseButton(button, set);
}
#endif // TOUCH_EVENTS_SUPPORT

/*virtual*/
void MDE_OpView::GetMousePos(INT32* xpos, INT32* ypos)
{
	OP_ASSERT(mdeWidget);
	mdeWidget->GetMousePos(xpos, ypos);
}

#endif // !MOUSELESS

/*virtual*/
OpPoint MDE_OpView::ConvertToScreen(const OpPoint &point)
{
	int ix, iy;
	OP_ASSERT(mdeWidget);
	ix = point.x;
	iy = point.y;
	mdeWidget->ConvertToScreen(ix, iy);
	return OpPoint(ix, iy);
}

/*virtual*/
OpPoint MDE_OpView::ConvertFromScreen(const OpPoint &point)
{
	int ix, iy;
	OP_ASSERT(mdeWidget);
	ix = point.x;
	iy = point.y;
	mdeWidget->ConvertFromScreen(ix, iy);
	return OpPoint(ix, iy);
}

#ifdef WIDGETS_IME_SUPPORT
void MDE_OpView::AbortInputMethodComposing()
{
	// send the destroyIMD event to the app
	OP_ASSERT(imeListener);

	if (spawning)
		return;

#ifdef LIBGOGI_IME_COMMIT_ON_ABORT_COMPOSING
	commiting = true;
#else
	commiting = false;
#endif // LIBGOGI_IME_COMMIT_ON_ABORT_COMPOSING

	if (currentIME)
	{
		aborting = true;
		g_mde_ime_manager->DestroyIME(currentIME);
		aborting = false;
		if (imeListener)
			imeListener->OnStopComposing(!commiting);

	}
	currentIME = NULL;
	OP_DELETE(imestr);
	imestr = NULL;
}

void MDE_OpView::SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle)
{
	OP_ASSERT(imeListener);

	IM_WIDGETINFO iw;
	// make sure a IME is not already running.
	if (!imeListener || spawning)
		return;
	// SetInputMethodMode will be called with mode = IME_MODE_UNKNOWN when commiting a change
	if (mode != IME_MODE_TEXT && mode != IME_MODE_PASSWORD)
	{
		return;
	}
	if (currentIME)
		AbortInputMethodComposing();
	imestr = OP_NEW(OpInputMethodString, ());
	if (!imestr) // FIXME OOM
		return;

	spawning = TRUE;

	imeListener->OnStartComposing(imestr, IM_COMPOSE_ALL);
	iw = imeListener->GetWidgetInfo();

	int maxlen = -1;
	if (OpWidget::GetFocused() && OpWidget::GetFocused()->GetType() == OpTypedObject::WIDGET_TYPE_EDIT)
	{
		maxlen = ((OpEdit*)OpWidget::GetFocused())->string.GetMaxLength();
	}

	MDE_IMEManager::FontInfo imeFontInfo;
	if (iw.has_font_info)
	{
		imeFontInfo.height = iw.font_height;
		imeFontInfo.style = 0;
		if (iw.font_style & WIDGET_FONT_STYLE_ITALIC)    imeFontInfo.style |= MDE_IMEManager::ITALIC;
		if (iw.font_style & WIDGET_FONT_STYLE_BOLD)      imeFontInfo.style |= MDE_IMEManager::BOLD;
		if (iw.font_style & WIDGET_FONT_STYLE_MONOSPACE) imeFontInfo.style |= MDE_IMEManager::MONOSPACE;
	}

	// Should be safe as both enums should have identical content.
	MDE_IMEManager::Context mde_context = static_cast<MDE_IMEManager::Context>(context);

	currentIME = g_mde_ime_manager->CreateIME(this, iw.widget_rect, imestr->Get(), iw.is_multiline, mode==IME_MODE_PASSWORD, maxlen, mde_context, imestr->GetUniPointCaretPos(), iw.has_font_info ? &imeFontInfo : NULL, istyle);

	spawning = FALSE;

	if (!currentIME) // OOM or other error
	{
		imeListener->OnStopComposing(TRUE);
		OP_DELETE(imestr);
		imestr = NULL;
		return;
	}
}

/** The widget has been respositioned, so notify the application layer. */
void MDE_OpView::SetInputMoved()
{
	IM_WIDGETINFO iw;
	if (!currentIME || !imeListener)
		return;
	iw = imeListener->GetWidgetInfo();
	g_mde_ime_manager->MoveIME(currentIME, iw.widget_rect);
}

/** The widget has changed its text, so notify the application layer. */
void MDE_OpView::SetInputTextChanged()
{
	if(!imestr)
		return;

	g_mde_ime_manager->UpdateIMEText( this, imestr->Get() );
}

/** Commit the ime, will set currentIME to NULL and set the text supplied if it is != NULL.
  * Call with text = NULL for cancel. */
void MDE_OpView::CommitIME(const uni_char *text)
{
	OP_ASSERT(imeListener);
	OP_ASSERT(currentIME);
	if (imeListener)
	{
		if (text)
		{
			BOOL is_change = !imestr->Get() || (uni_strcmp(text, imestr->Get()) != 0);
			INT32 text_compose_len = uni_strlen(text);
			if (is_change)
			{
				imestr->Set(text, text_compose_len); // FIXME OOM
				imeListener->OnCompose();
			}
			imeListener->OnStopComposing(FALSE);
		}
		else
			imeListener->OnStopComposing(TRUE);
	}
	OP_DELETE(imestr);
	imestr = NULL;
	currentIME = NULL;

	commiting = true;

	//We are aborting another ime and the stuff below messes up the input context.
	if (aborting)
		return;

	// Cannot call DeactivateElement() for
	// FEATURE_IME_SEND_KEY_EVENTS, since the key events sent by
	// SendKey are handled asynchronously, and if input field is not
	// focused, the key events won't reach the input field.
	// The deactivation is handled later, when the keypress comes through
#ifndef IME_SEND_KEY_EVENTS
	if (OpWidget::GetFocused() && OpWidget::GetFocused()->GetVisualDevice() &&
		OpWidget::GetFocused()->GetVisualDevice()->GetDocumentManager())
	{
		FramesDocument* doc = OpWidget::GetFocused()->GetVisualDevice()->GetDocumentManager()->GetCurrentDoc();
		if (doc && doc->GetHtmlDocument())
		{
 			doc->GetHtmlDocument()->DeactivateElement();
#ifndef IME_NO_HIGHLIGHT_ON_COMMIT
 			doc->GetHtmlDocument()->InvertFormBorderRect(doc->GetHtmlDocument()->GetNavigationElement());
#endif // !IME_NO_HIGHLIGHT_ON_COMMIT
		}
	}
#endif // !IME_SEND_KEY_EVENTS
}

void MDE_OpView::UpdateIME(const uni_char *text, int cursorpos, int highlight_start, int highlight_len)
{
	OP_ASSERT(imeListener);
	OP_ASSERT(currentIME);
	if (imeListener)
	{
		if(text != NULL)
		{
			INT32 text_compose_len = uni_strlen(text);
			imestr->Set(text, text_compose_len); // FIXME OOM
		}
		imestr->SetCaretPos(cursorpos);
		imestr->SetCandidate(highlight_start, highlight_len);
		imeListener->OnCompose();
	}
}

void MDE_OpView::UpdateIME(const uni_char *text)
{
	int caret_pos = imestr->GetCaretPos();
	const int text_len = uni_strlen(text);

	if(caret_pos > text_len)
	{
		caret_pos = text_len;
	}

	UpdateIME(text, caret_pos, caret_pos, 0);
}

OP_STATUS MDE_OpView::SubmitIME()
{
	if (OpWidget::GetFocused() && OpWidget::GetFocused()->GetFormObject())
	{
		FormObject* form_obj = OpWidget::GetFocused()->GetFormObject();

		return FormManager::SubmitFormFromInputField(form_obj->GetDocument(), form_obj->GetHTML_Element(), SHIFTKEY_NONE);
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

#endif // WIDGETS_IME_SUPPORT

/*virtual*/
void MDE_OpView::OnHighlightRectChanged(const OpRect &rect)
{
	// NOP for now
}

/*virtual*/
OpPainter* MDE_OpView::GetPainter(const OpRect &rect, BOOL nobackbuffer)
{
#ifndef VEGA_OPPAINTER_SUPPORT
	OP_ASSERT(painter);
#endif
#ifdef MDE_DOUBLEBUF_SUPPORT
	if (doubleBuffer && !nobackbuffer)
	{
		// Screen is set on the painter already (from MDE_Widget::OnPaint)
		// Create the backbuffer and redirect the painter to it, translated. If OOM, no backbuffer will be used this time.
		MDE_BUFFER *screen = ((MDE_OpPainter*)painter)->GetScreen();
		backbuf = MDE_CreateBuffer(rect.width, rect.height, screen->format, 0);
		if (backbuf)
		{
			frontbuf = *screen;
			backbuf->method = screen->method;
			backbuf->col = screen->col;
			backbuf->mask = screen->mask;
			MDE_MakeSubsetBuffer(MDE_MakeRect(0, 0, rect.width, rect.height), screen, backbuf);
			MDE_OffsetBuffer(rect.x, rect.y, screen);
		}
	}
#endif
	// There is really no limit to the number of painters, as long as they are not used between OnPaint calls
	OP_ASSERT(numPainters == 0);
	++numPainters;

#ifdef VEGA_OPPAINTER_SUPPORT
	if (bitmap_painter)
		return bitmap_painter;
	if (!mdeWidget || !mdeWidget->m_screen)
		return NULL;

	MDE_OpWindow *mdewin = (MDE_OpWindow*) (mdeWidget ? mdeWidget->GetParentWindow() : NULL);
	if (mdewin && mdewin->GetCacheBitmap())
	{
		painter = (VEGAOpPainter*) mdewin->GetCacheBitmap()->GetPainter();
		painter->SetClipRect(rect);
		painter->SetVegaTranslation(0, 0);
		return painter;
	}

	painter = mdeWidget->m_screen->GetVegaPainter();
	OP_ASSERT(painter);
	int x = 0;
	int y = 0;
	mdeWidget->ConvertToScreen(x, y);
	painter->SetVegaTranslation(x, y);
	return painter;
#else
	return painter;
#endif
}

/*virtual*/
void MDE_OpView::ReleasePainter(const OpRect &rect)
{
	--numPainters;
	OP_ASSERT(numPainters == 0);

#ifdef VEGA_OPPAINTER_SUPPORT
	MDE_OpWindow *mdewin = (MDE_OpWindow*) (mdeWidget ? mdeWidget->GetParentWindow() : NULL);
	if (mdewin && mdewin->GetCacheBitmap() && painter)
	{
		painter->RemoveClipRect();
		mdewin->GetCacheBitmap()->ReleasePainter();
	}
	painter = NULL;
#endif

#ifdef MDE_DOUBLEBUF_SUPPORT
	// always end the backbuffer usage to avoid storing if we are using backbuffer here
	if (backbuf)
	{
		backbuf->method = MDE_METHOD_COPY;
		MDE_DrawBuffer(backbuf, MDE_MakeRect(rect.x, rect.y, backbuf->w, backbuf->h), 0, 0, &frontbuf);
		MDE_DeleteBuffer(backbuf);
	}
#endif
}

/*virtual*/
void MDE_OpView::Invalidate(const OpRect& rect)
{
	OP_ASSERT(mdeWidget);
	MDE_RECT mrect;
	mrect.x = rect.x;
	mrect.y = rect.y;
	mrect.w = rect.width;
	mrect.h = rect.height;
	mdeWidget->Invalidate(mrect, FALSE, TRUE);
}

#ifdef PLATFORM_FONTSWITCHING
/*virtual*/
void MDE_OpView::SetPreferredCodePage(OpFontInfo::CodePage cp)
{
	OP_ASSERT(FALSE);
}
#endif // PLATFORM_FONTSWITCHING
