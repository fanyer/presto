/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/selftest/src/doc_st_utils.h"
#include "modules/selftest/selftest_module.h"
#include "modules/selftest/src/testutils.h"
#include "modules/dochand/win.h"
#include "modules/dochand/viewportcontroller.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/scrollable.h"

#ifdef SVG_SUPPORT
#include "modules/logdoc/helistelm.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

void DocSTUtils::SetScale(FramesDocument *doc, int scale)
{
	Window* win = g_selftest.utils->GetWindow();
	ViewportController* controller = win->GetViewportController();
	// Notify viewport controller that we're changing scale.
	controller->GetViewportRequestListener()->OnZoomLevelChangeRequest(controller, (double)scale / 100.,
																	   0, VIEWPORT_CHANGE_REASON_INPUT_ACTION);
	// Change base scale, since reference images are made with truezoom off.
	controller->SetTrueZoomBaseScale(scale);
	// Also set visual device's layout scale directly - shouldn't be necessary but apparently it is.
	doc->GetVisualDevice()->SetLayoutScale(scale);
	// Force reflow, since SetTrueZoomBaseScale doesn't.
	doc->Reflow(TRUE);
	win->SetScale(scale);
}

/*static*/
FramesDocElm* DocSTUtils::GetFrameById(FramesDocument *doc, const char* id)
{
	FramesDocElm* target_frame = NULL;
	DocumentTreeIterator iter(doc);

	while (iter.Next())
	{
		FramesDocElm* frame = iter.GetFramesDocElm();
		HTML_Element* elm = frame->GetHtmlElement();
		if (frame && elm)
		{
			const uni_char* frame_id = elm->GetId();
			if (frame_id && uni_strcmp(frame_id, id) == 0)
			{
				target_frame = frame;
				break;
			}
		}
	}

	return target_frame;
}

AsyncSTOperationObject::AsyncSTOperationObject()
{
	m_time_counter = 0;

	m_timer.SetTimerListener(this);
	m_timer.Start(10);
}

/*virtual*/
void AsyncSTOperationObject::OnTimeOut(OpTimer* timer)
{
	m_time_counter += 10;

	if (CheckPassCondition())
	{
		OP_DELETE(this);
		ST_passed();
	}
	else if (m_time_counter > 5000)
	{
		OP_DELETE(this);
		ST_failed("Timeout!");
	}
	else
		m_timer.Start(10);
}

/*static*/
FramesDocument* AsyncSTOperationObject::GetTestDocument()
{
	Window* window = g_selftest.utils->GetWindow();
	return window ? window->DocManager()->GetCurrentDoc() : NULL;
}

void AsyncSTOperationObject::Abort()
{
	m_timer.Stop();
	OP_DELETE(this);
}

/*static*/
OP_STATUS WaitUntilLoadedObj::Paint()
{
	FramesDocument *doc = GetTestDocument();
	if (!doc)
		return OpStatus::ERR;

	VisualDevice* vd = doc->GetVisualDevice();

	int width = 1000;
	int height = 1000;
	OpBitmap* bitmap_to_paint;
	RETURN_IF_ERROR(OpBitmap::Create(&bitmap_to_paint, 10, 10, FALSE, FALSE, 0, 0, TRUE));

	OpPainter* painter = bitmap_to_paint->GetPainter();
	if (!painter)
	{
		OP_DELETE(bitmap_to_paint);
		return OpStatus::ERR;
	}

	painter->SetColor(0, 0, 0);
	painter->FillRect(OpRect(0, 0, width, height));

	CoreViewContainer* cview = vd->GetView()->GetContainer();
	cview->Paint(OpRect(0, 0, width, height), painter, 0, 0, TRUE, FALSE);

	bitmap_to_paint->ReleasePainter();

	OP_DELETE(bitmap_to_paint);
	return OpStatus::OK;
}

/*virtual*/
BOOL WaitUntilLoadedObj::CheckPassCondition()
{
	FramesDocument* doc = GetTestDocument();

	if (!doc)
		return FALSE;

	// Call paint so we are sure all inlines gets loaded.
	Paint();

	return doc->IsLoaded(TRUE) && doc->ImagesDecoded() && !doc->GetVisualDevice()->IsLocked();
}

/*static*/
void WaitUntilLoadedObj::LoadAndWaitUntilLoaded(Window* win, const uni_char* url)
{
	WaitUntilLoadedObj* new_obj = OP_NEW(WaitUntilLoadedObj, ());
	if (!new_obj)
	{
		ST_failed("Out of memory");
		return;
	}

	OpWindowCommander::OpenURLOptions options;
	options.entered_by_user = TRUE;
	if (OpStatus::IsError(win->GetWindowCommander()->OpenURL(url, options)))
	{
		new_obj->Abort();
		ST_failed("Failed to open URL");
		return;
	}

	Paint();
}

/*static*/
void WaitUntilLoadedObj::ReloadAndWaitUntilLoaded(FramesDocument* top_doc)
{
	if (!(OP_NEW(WaitUntilLoadedObj, ())))
		ST_failed("Out of memory");
	else
	{
		top_doc->GetWindow()->GetWindowCommander()->Reload();
		Paint();
	}
}

void WaitUntilLoadedObj::MoveInHistoryAndWaitUntilLoaded(Window* win, BOOL forward)
{
	if (!(OP_NEW(WaitUntilLoadedObj, ())))
		ST_failed("Out of memory");
	else
	{
		if (forward)
			win->SetHistoryNext();
		else
			win->SetHistoryPrev();
		Paint();
	}
}

MoveVisualViewportObj::MoveVisualViewportObj(FramesDocument* doc, INT32 x, INT32 y)
	: AsyncSTOperationObject()
{
	doc->RequestSetVisualViewPos(x, y, VIEWPORT_CHANGE_REASON_SELFTEST);

	// Calculate what is the expected final position
	int negative_overflow = doc->NegativeOverflow();
	OpRect visual_viewport = doc->GetVisualViewport(),
		wholeDocRect(-negative_overflow, 0, doc->Width() + negative_overflow, doc->Height());

	this->x = visual_viewport.x = x;
	this->y = visual_viewport.y = y;

	if (visual_viewport.Right() > wholeDocRect.Right())
		this->x = wholeDocRect.Right() - visual_viewport.width;

	if (visual_viewport.x < wholeDocRect.x)
		this->x = wholeDocRect.x;

	if (visual_viewport.Bottom() > wholeDocRect.Bottom())
		this->y = wholeDocRect.Bottom() - visual_viewport.height;

	if (visual_viewport.y < wholeDocRect.y)
		this->y = wholeDocRect.y;
}

/*static*/
void MoveVisualViewportObj::MoveVisualViewport(INT32 x, INT32 y)
{
	FramesDocument *doc = GetTestDocument();
	if (!doc)
		ST_failed("No document to move visual viewport of.");
	else if (!(OP_NEW(MoveVisualViewportObj, (doc, x, y))))
		ST_failed("Out of memory");
}

/*virtual*/
BOOL MoveVisualViewportObj::CheckPassCondition()
{
	FramesDocument *doc = GetTestDocument();

	if (!doc)
		return FALSE;

	OpRect vis_viewport = doc->GetVisualViewport();

	return vis_viewport.x == x && vis_viewport.y == y;
}


/*static*/ void
ScrollableScrolledObj::WaitUntilScrollableScrolled(HTML_Element* scrollable)
{
	ScrollableArea* sc = scrollable->GetLayoutBox() ? scrollable->GetLayoutBox()->GetScrollable() : NULL;
	if (!sc)
		ST_failed("The element has no ScrollableArea.");
	else if(!(OP_NEW(ScrollableScrolledObj, (scrollable))))
		ST_failed("Out of memory");
}

/*virtual*/BOOL
ScrollableScrolledObj::CheckPassCondition()
{
	ScrollableArea* sc = scrollable->GetLayoutBox() ? scrollable->GetLayoutBox()->GetScrollable() : NULL;
	if (!sc)
		return FALSE;

	return sc->GetViewY() > 0;
}

/*static*/
void CheckJSFunctionExecutedObj::WaitUntilJSFunctionExecuted(FramesDocument* doc)
{
	HTML_Element *root = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetRoot() : NULL;
	HTML_Element *elm = NULL;

	if (!root)
	{
		ST_failed("No root element, the test can't pass.");
		return;
	}

	elm = root->GetElmById(UNI_L("function_executed"));

	if (elm)
		ST_passed();
	else
	{
		if (!(OP_NEW(CheckJSFunctionExecutedObj, (doc))))
			ST_failed("Out of memory");
	}
}

/*virtual*/
BOOL CheckJSFunctionExecutedObj::CheckPassCondition()
{
	HTML_Element *root = scripted_doc->GetLogicalDocument() ? scripted_doc->GetLogicalDocument()->GetRoot() : NULL;

	if (!root)
		return FALSE;

	return !!root->GetElmById(UNI_L("function_executed"));
}

#ifdef SVG_SUPPORT

/*static*/ void
WaitUntilSVGReadyObj::WaitUntilSVGReady(const char* id)
{
	if (!(OP_NEW(WaitUntilSVGReadyObj, (id))))
		ST_failed("Out of memory");
}

/*virtual*/ BOOL
WaitUntilSVGReadyObj::CheckPassCondition()
{
	FramesDocument *doc = GetTestDocument();
	if (!doc)
		return FALSE;

	HTML_Element *root = doc->GetLogicalDocument() ? doc->GetLogicalDocument()->GetRoot() : NULL;
	if (!root)
		return FALSE;

	OpString temp;
	temp.Set(id);
	HTML_Element* html_element = root->GetElmById(temp);
	HEListElm* helm = html_element ? html_element->GetHEListElm(FALSE) : NULL;

	if (helm)
	{
		UrlImageContentProvider* content_provider = helm->GetUrlContentProvider();
		if (content_provider)
			return content_provider->GetSVGImageRef() && content_provider->GetSVGImageRef()->GetSVGImage();
	}

	return FALSE;
}
#endif // SVG_SUPPORT

#endif // SELFTEST
