/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/dochand/viewportcontroller.h"

#include "modules/debug/debug.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/logdoc/logdoc.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif // NEARBY_ELEMENT_DETECTION
#ifdef RESERVED_REGIONS
# include "modules/dom/domevents.h"
#endif // RESERVED_REGIONS

/**
 * This macro can be used to add an OpRect to the OP_DBG debug
 * statement. You need to add the debug string formatter
 * "(%d,%d)+%dx%d" at the correct place to use this macro.
 *
 * Example:
 * \code
 *   OpRect my_rect = ...;
 *   OP_DBG(UNI_L("rectangle: (%d,%d)+%dx%d"), _DBG_OP_RECT(my_rect));
 * \endcode
 * @param rect is a GogiRect instance.
 */
#define _DBG_OP_RECT(rect) rect.x, rect.y, rect.width, rect.height

static inline VisualDevice* GetScreenVisualDevice(Window* window)
{
	/* Get the print preview VisualDevice if in print preview mode; otherwise
	   get the "regular" VisualDevice. */

#ifdef _PRINT_SUPPORT_
	if (window->GetPreviewMode())
		if (VisualDevice* vis_dev = window->DocManager()->GetPrintPreviewVD())
		{
			OP_ASSERT(!vis_dev->IsPrinter()); // FIXME: We wanted the print PREVIEW VisualDevice, but take a look at the GetPrintPreviewVD() implementation... :(

			return vis_dev;
		}
#endif // _PRINT_SUPPORT_

	return window->DocManager()->GetVisualDevice();
}

void ViewportController::HandleVisualViewportChange(const OpRect& viewport)
{
	OP_NEW_DBG("ViewportController::HandleVisualViewportChange", "viewport");
	OP_DBG((UNI_L("visual viewport: (%d,%d)+%dx%d -> (%d,%d)+%dx%d"), visual_viewport_pos.x, visual_viewport_pos.y, visual_viewport_width, visual_viewport_height, _DBG_OP_RECT(viewport)));

	OpRect new_visual_viewport;
	int negative_overflow = 0;

	if (visual_viewport_size_locked)
	{
		// Visual viewport size locked. Ignore new size (if any), but honor position.

		new_visual_viewport = GetVisualViewport();
		new_visual_viewport.x = viewport.x;
		new_visual_viewport.y = viewport.y;

		OP_DBG((UNI_L("Visual viewport size locked. Ignore new size (if any), but honor position. -> viewport: (%d,%d)+%dx%d"), _DBG_OP_RECT(new_visual_viewport)));
	}
	else
		new_visual_viewport = viewport;

#ifdef NEARBY_ELEMENT_DETECTION
	if (window->GetElementExpander())
	{
		VisualDevice* vis_dev = GetScreenVisualDevice(window);
		int dx = vis_dev->ScaleToScreen(visual_viewport_pos.x - new_visual_viewport.x);
		int dy = vis_dev->ScaleToScreen(visual_viewport_pos.y - new_visual_viewport.y);

		window->GetElementExpander()->Scroll(dx, dy);
	}
#endif // NEARBY_ELEMENT_DETECTION

	FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc();
	if (doc)
	{
		OpRect layout_viewport = doc->GetLayoutViewport();
		negative_overflow = doc->NegativeOverflow();

		OP_DBG((UNI_L("layout viewport: (%d,%d)+%dx%d; negative overflow: %d"), _DBG_OP_RECT(layout_viewport), negative_overflow));

		layout_viewport.x += negative_overflow;

		int new_layout_view_x = layout_viewport.x;
		int new_layout_view_y = layout_viewport.y;

		// Keep visual viewport within layout viewport by moving layout viewport if necessary.

		if (new_visual_viewport.x > visual_viewport_pos.x)
		{
			if (new_visual_viewport.x + new_visual_viewport.width > layout_viewport.x + layout_viewport.width)
				new_layout_view_x = new_visual_viewport.x + new_visual_viewport.width - layout_viewport.width;
		}
		else if (new_visual_viewport.x < visual_viewport_pos.x)
		{
			if (layout_viewport.x > new_visual_viewport.x)
				new_layout_view_x = new_visual_viewport.x;
		}

		if (new_visual_viewport.y > visual_viewport_pos.y)
		{
			if (new_visual_viewport.y + new_visual_viewport.height > layout_viewport.y + layout_viewport.height)
				new_layout_view_y = new_visual_viewport.y + new_visual_viewport.height - layout_viewport.height;
		}
		else if (new_visual_viewport.y < visual_viewport_pos.y)
		{
			if (layout_viewport.y > new_visual_viewport.y)
				new_layout_view_y = new_visual_viewport.y;
		}

		new_layout_view_x -= negative_overflow;

		OP_DBG((UNI_L("new layout viewport pos: (%d,%d), buffered mode: %s"),
				new_layout_view_x, new_layout_view_y, buffered_mode?UNI_L("yes"):UNI_L("no")));

		if (buffered_mode)
		{
			buffered_layout_view_x = new_layout_view_x;
			buffered_layout_view_y = new_layout_view_y;
		}
		else
			doc->SetLayoutViewPos(new_layout_view_x, new_layout_view_y);
	}

#ifdef DEBUG_PAINT_VISUAL_VIEWPORT_RECT
	VisualDevice* vis_dev = GetScreenVisualDevice(window);
	OpRect visual_viewport = GetVisualViewport();
	vis_dev->Update(visual_viewport.x - negative_overflow, visual_viewport.y, visual_viewport.width, visual_viewport.height);
#endif

	BOOL viewport_pos_changed = visual_viewport_pos.x != new_visual_viewport.x || visual_viewport_pos.y != new_visual_viewport.y;

	visual_viewport_pos.x = new_visual_viewport.x;
	visual_viewport_pos.y = new_visual_viewport.y;
	visual_viewport_width = new_visual_viewport.width;
	visual_viewport_height = new_visual_viewport.height;

	if (doc && viewport_pos_changed)
		doc->HandleDocumentEvent(ONSCROLL);

#ifdef DEBUG_PAINT_VISUAL_VIEWPORT_RECT
	vis_dev->Update(new_visual_viewport.x - negative_overflow, new_visual_viewport.y, new_visual_viewport.width, new_visual_viewport.height);
#endif
}

void ViewportController::GetVisualViewportSizeFromWindow(unsigned int& width, unsigned int& height)
{
	VisualDevice* vis_dev = GetScreenVisualDevice(window);
	UINT32 win_width, win_height;

	window->GetOpWindow()->GetInnerSize(&win_width, &win_height);

	width = vis_dev->ScaleToDoc(win_width - vis_dev->GetVerticalScrollbarSpaceOccupied());
	height = vis_dev->ScaleToDoc(win_height - vis_dev->GetHorizontalScrollbarSpaceOccupied());
}

static OpPoint GetOffsetToToplevelDoc(FramesDocument* frm_doc)
{
	VisualDevice* top_vd = frm_doc->GetWindow()->VisualDev();
	OpPoint offs(top_vd->GetRenderingViewX(), top_vd->GetRenderingViewY());

	for (FramesDocument* doc = frm_doc; doc; doc = doc->GetDocManager()->GetParentDoc())
	{
		FramesDocElm* frame = doc->GetDocManager()->GetFrame();
		VisualDevice* vd = doc->GetVisualDevice();

		if (frame)
		{
			offs.x += frame->GetAbsX();
			offs.y += frame->GetAbsY();
		}

		offs.x -= vd->GetRenderingViewX();
		offs.y -= vd->GetRenderingViewY();
	}

	return offs;
}

OpRect ViewportController::ConvertToToplevelRect(FramesDocument* frm_doc, const OpRect& local_rect)
{
	OpRect toplevel_rect = local_rect;
	OpPoint offset = GetOffsetToToplevelDoc(frm_doc);

	toplevel_rect.OffsetBy(offset.x, offset.y);

	return toplevel_rect;
}

OpRect ViewportController::ConvertToLocalRect(FramesDocument* frm_doc, const OpRect& toplevel_rect)
{
	OpRect local_rect = toplevel_rect;
	OpPoint offset = GetOffsetToToplevelDoc(frm_doc);

	local_rect.OffsetBy(-offset.x, -offset.y);

	return local_rect;
}

FramesDocument* ViewportController::FindDocumentAtPosAndTranslate(OpPoint& p, BOOL allow_svg /* = TRUE*/)
{
	FramesDocument* d = window->DocManager()->GetCurrentVisibleDoc();
	if (!d)
		return NULL;

	VisualDevice* vd = d->GetVisualDevice();
	OpPoint screen_pos = vd->ScaleToScreen(p - vd->GetRenderingViewport().TopLeft()) + vd->GetPosOnScreen();
	FindElementAtScreenPosAndTranslate(screen_pos, d, p);

#ifdef SVG_SUPPORT
	if (!allow_svg)
		while (d && d->GetLogicalDocument() && d->GetLogicalDocument()->GetDocRoot() &&
		       d->GetLogicalDocument()->GetDocRoot()->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		{
			d = d->GetParentDoc();
			p = vd->ScaleToDoc(screen_pos - vd->GetPosOnScreen()) + vd->GetRenderingViewport().TopLeft();
		}
#endif

	return d;
}

HTML_Element* ViewportController::FindElementAtScreenPosAndTranslate(const OpPoint& screen_pos, FramesDocument* &target_doc, OpPoint &document_pos)
{
	HTML_Element* target = NULL;

	target_doc = window->DocManager()->GetCurrentVisibleDoc();
	if (!target_doc)
		return NULL;

	// Loop over the document hierarchy to try to find the target element. An IntersectionObject is used to find an element within a document.
	// The IntersectionObject won't enter subdocuments. For iframes it returns the HTML_Element of the iframe, and for framesets it does nothing.
    // Thus iframes and framesets need different code paths. Either way, when a subdocument is encountered, the loop is restarted with
	// the target_doc set to the subdocument. That way, the document position is always recalculated to match the current target_doc and
	// additional subdocuments will be entered when needed.
	while (TRUE)
	{
		VisualDevice* vd = target_doc->GetVisualDevice();
		document_pos = vd->ScaleToDoc(screen_pos - vd->GetPosOnScreen()) + vd->GetRenderingViewport().TopLeft();

		if (target_doc->IsFrameDoc())
		{
			// The document contains a frameset. Intersection objects won't work with framesets, so we need to manually locate which subdocument the position is in.
			FramesDocElm* fdelm = target_doc->GetFrmDocRoot();
			while (fdelm)
			{
				if (fdelm->GetCurrentDoc())
				{
					OpRect frame_rect(fdelm->GetAbsX(), fdelm->GetAbsY(), fdelm->GetWidth(), fdelm->GetHeight());
					if (frame_rect.Contains(document_pos))
						break;
				}
				fdelm = fdelm->Next();
			}

			if (fdelm)
			{
				// Found a suitable subdocument. Set target_doc and restart the loop to enter the subdocument.
				target_doc = fdelm->GetCurrentDoc();
				continue;
			}
		}

		// It is not a frameset, or we didn't find a matching frame -> try an IntersectionObject.

		HLDocProfile* hld_profile = target_doc->GetHLDocProfile();
		if (!hld_profile || !hld_profile->GetRoot())
			break;

		IntersectionObject intersection(target_doc, LayoutCoord(document_pos.x), LayoutCoord(document_pos.y), TRUE);
		intersection.Traverse(hld_profile->GetRoot());

		Box* inner_box = intersection.GetInnerBox();
		if (!inner_box)
			break;

		target = inner_box->GetHtmlElement();

		FramesDocElm* fdelm = FramesDocElm::GetFrmDocElmByHTML(target);
		if (!fdelm || !fdelm->GetCurrentDoc())
			break; // The loop normally ends here. We found a target element and it is not an iframe!

		// The position is at an iframe with a document. Set target_doc and restart the loop to enter the subdocument.
		target_doc = fdelm->GetCurrentDoc();
		target = NULL;
	}

	return target;
}

#ifdef PAGED_MEDIA_SUPPORT
void ViewportController::SignalPageChange(int current, int total)
{
	if (current_page_number != current || total_page_count != total)
	{
		current_page_number = current;
		total_page_count = total;

		viewport_info_listener->OnPageChanged(current, total);
	}
}
#endif // PAGED_MEDIA_SUPPORT

void ViewportController::SetViewportRequestListener(OpViewportRequestListener* listener)
{
	viewport_request_listener = listener ? listener : &default_viewport_request_listener;
}

void ViewportController::SetViewportInfoListener(OpViewportInfoListener* listener)
{
	viewport_info_listener = listener ? listener : &null_viewport_info_listener;
}

void ViewportController::LockVisualViewportSize(BOOL lock)
{
	OP_NEW_DBG("ViewportController::LockVisualViewportSize", "viewport");
	OP_DBG((UNI_L("lock: %s"), (lock?UNI_L("yes"):UNI_L("no"))));

	if (!visual_viewport_size_locked != !lock)
	{
		if (!lock)
			// Set visual viewport size to current window size when unlocking it.

			GetVisualViewportSizeFromWindow(visual_viewport_width, visual_viewport_height);

		visual_viewport_size_locked = lock;
	}

#ifdef DEBUG_PAINT_VISUAL_VIEWPORT_RECT
	GetScreenVisualDevice(window)->UpdateAll();
#endif // DEBUG_PAINT_VISUAL_VIEWPORT_RECT
}

void ViewportController::SetBufferedMode(BOOL buffered_mode)
{
	if (this->buffered_mode != buffered_mode)
	{
		if (!buffered_mode)
			FlushBuffer();

		this->buffered_mode = buffered_mode;
	}
}

void ViewportController::FlushBuffer()
{
	OP_NEW_DBG("ViewportController::FlushBuffer", "viewport");

	if (buffered_mode)
	{
		OpRect updated_area;

		if (buffered_layout_view_x != INT_MIN)
		{
			if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
			{
				OP_DBG((UNI_L("flush layout viewport position: %d,%d"),
						buffered_layout_view_x, buffered_layout_view_y));
				updated_area = doc->SetLayoutViewPos(buffered_layout_view_x, buffered_layout_view_y);
			}

			buffered_layout_view_x = INT_MIN;
			buffered_layout_view_y = INT_MIN;
		}

		if (buffered_rendering_view_x != INT_MIN)
		{
			OP_DBG((UNI_L("flush rendering viewport position: %d,%d"),
					buffered_rendering_view_x, buffered_rendering_view_y));

			GetScreenVisualDevice(window)->SetRenderingViewPos(buffered_rendering_view_x, buffered_rendering_view_y, FALSE, &updated_area);

			buffered_rendering_view_x = INT_MIN;
			buffered_rendering_view_y = INT_MIN;
		}
	}
}

void ViewportController::SetDesktopLayoutViewportSize(unsigned int width, unsigned int height)
{
	OP_NEW_DBG("ViewportController::SetDesktopLayoutViewportSize", "viewport");
	OP_DBG((UNI_L("size: %dx%d"), width, height));

	if (desktop_layout_width != width || desktop_layout_height != height)
	{
		desktop_layout_width = width;
		desktop_layout_height = height;

		if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
			doc->RecalculateLayoutViewSize(FALSE);
	}
}

OpRect ViewportController::GetVisualViewport()
{
	OpRect visual_viewport;

	visual_viewport.x = visual_viewport_pos.x;
	visual_viewport.y = visual_viewport_pos.y;

	if (visual_viewport_size_locked)
		// Let visual viewport size follow window size

		GetVisualViewportSizeFromWindow(visual_viewport_width, visual_viewport_height);

	visual_viewport.width = visual_viewport_width;
	visual_viewport.height = visual_viewport_height;

	return visual_viewport;
}

void ViewportController::SetVisualViewport(const OpRect& viewport)
{
	OP_NEW_DBG("ViewportController::SetVisualViewport", "viewport");
	OP_DBG((UNI_L("viewport: (%d,%d)+%dx%d"), _DBG_OP_RECT(viewport)));

	HandleVisualViewportChange(viewport);
}

void ViewportController::SetVisualViewportPos(const OpPoint& pos)
{
	OP_NEW_DBG("ViewportController::SetVisualViewportPos", "viewport");
	OP_DBG((UNI_L("viewport: (%d,%d)"), pos.x, pos.y));

	OpRect new_viewport = GetVisualViewport();

	new_viewport.x = pos.x;
	new_viewport.y = pos.y;

	HandleVisualViewportChange(new_viewport);
}

#ifdef PAGED_MEDIA_SUPPORT
void ViewportController::SetCurrentPageNumber(int page_number)
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
		{
			LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace();
			int page_count = workplace->GetTotalPageCount();

			if (page_number >= page_count)
				page_number = page_count - 1;

			if (page_number < 0)
				page_number = 0;

			workplace->SetCurrentPageNumber(page_number, VIEWPORT_CHANGE_REASON_INPUT_ACTION);
		}
}

int ViewportController::GetCurrentPageNumber()
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			return logdoc->GetLayoutWorkplace()->GetCurrentPageNumber();

	return -1;
}

int ViewportController::GetTotalPageCount()
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			return logdoc->GetLayoutWorkplace()->GetTotalPageCount();

	return -1;
}
#endif // PAGED_MEDIA_SUPPORT

void ViewportController::SetTextParagraphWidthLimit(unsigned int max_paragraph_width, const OpPoint* pos)
{
	OP_NEW_DBG("ViewportController::SetTextParagraphWidthLimit", "viewport");
	OP_DBG((UNI_L("max paragraph width: %d; position: (%d,%d)"), max_paragraph_width, (pos ? pos->x : 0), (pos ? pos->y : 0)));

	if (window->GetLimitParagraphWidth() && (unsigned int) this->max_paragraph_width == max_paragraph_width)
		return;

	if (pos)
	{
		// unconstify
		OpPoint tmp_point = *pos;

		FramesDocument* fdoc = FindDocumentAtPosAndTranslate(tmp_point);

		if (fdoc)
		{
			HTML_Document* hdoc = fdoc->GetHtmlDocument();

			OP_ASSERT(hdoc);

			if (hdoc)
				hdoc->SaveScrollToElement(tmp_point);
		}
	}

	this->max_paragraph_width = max_paragraph_width;

	window->SetLimitParagraphWidth(TRUE);
}

void ViewportController::DisableTextParagraphWidthLimit(const OpPoint* pos)
{
	OP_NEW_DBG("ViewportController::DisableTextParagraphWidthLimit", "viewport");
	OP_DBG((UNI_L("position: (%d,%d)"), (pos ? pos->x : 0), (pos ? pos->y : 0)));

	if (!window->GetLimitParagraphWidth())
		return;

	if (pos)
	{
		// unconstify
		OpPoint tmp_point = *pos;

		FramesDocument* fdoc = FindDocumentAtPosAndTranslate(tmp_point);

		OP_ASSERT(fdoc);

		if (fdoc)
		{
			HTML_Document* hdoc = fdoc->GetHtmlDocument();

			OP_ASSERT(hdoc);

			if (hdoc)
				hdoc->SaveScrollToElement(tmp_point);
		}
	}

	window->SetLimitParagraphWidth(FALSE);
}

OpRect ViewportController::GetRenderingViewport()
{
	VisualDevice* vis_dev = GetScreenVisualDevice(window);

	OpRect viewport = vis_dev->GetRenderingViewport();

	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
		viewport.x += doc->NegativeOverflow();

	return viewport;
}

void ViewportController::SetRenderingViewportPos(const OpPoint& pos)
{
	OP_NEW_DBG("ViewportController::SetRenderingViewportPos", "viewport");
	OP_DBG((UNI_L("position: (%d,%d)"), pos.x, pos.y));

	VisualDevice* vis_dev = GetScreenVisualDevice(window);
	int negative_overflow = 0;

	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
		negative_overflow = doc->NegativeOverflow();

	OpPoint new_pos(pos.x - negative_overflow, pos.y);

	if (buffered_mode)
	{
		buffered_rendering_view_x = new_pos.x;
		buffered_rendering_view_y = new_pos.y;
	}
	else
		vis_dev->SetRenderingViewPos(new_pos.x, new_pos.y);
}

void ViewportController::SetRenderingScale(unsigned int scale_percentage)
{
	OP_NEW_DBG("ViewportController::SetRenderingScale", "viewport");
	OP_DBG((UNI_L("scale: %d%%"), scale_percentage));

	window->SetTrueZoom(TRUE);
	window->SetScale(scale_percentage);
}

void ViewportController::SetTrueZoomBaseScale(unsigned int scale_percentage)
{
	window->SetTrueZoomBaseScale(scale_percentage);
}

OpRect ViewportController::GetLayoutViewport()
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
	{
		OpRect viewport = doc->GetLayoutViewport();

		viewport.x += doc->NegativeOverflow();

		return viewport;
	}
	else
		return OpRect();
}

void ViewportController::GetDocumentSize(unsigned int* width, unsigned int* height)
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
	{
		*width = doc->Width() + doc->NegativeOverflow();
		*height = doc->Height();
	}
	else
	{
		*width = 0;
		*height = 0;
	}
}

void ViewportController::GetParagraphRects(const OpRect& rect, Head* paragraph_rects)
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
		if (LogicalDocument* logdoc = doc->GetLogicalDocument())
			if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
			{
				RAISE_IF_MEMORY_ERROR(workplace->GetParagraphList(rect, *paragraph_rects));

				int negative_overflow = doc->NegativeOverflow();

				for (OpRectListItem* item = (OpRectListItem*) paragraph_rects->First(); item; item = item->Suc())
					item->rect.x += negative_overflow;
			}
}

#ifdef RESERVED_REGIONS
/**
  * ReservedRectFinder traverses a document hierarchy and gathers a list of all reserved rectangles found.
  */
class ReservedRectFinder
{
public:
	/**
	  * Retrieve list of all reserved rectangles in the document hierarchy.
	  *
	  * @param[in] root_document Document from which to initiate search.
	  * @param[in] search_rect Rectangle defining region in which to look for reserved rectangles.
	  * @param[out] reserved_region Region structure where found reserved rectangles will be stored.
	  *
	  * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS FindReservedRects(FramesDocument* root_document, const OpRect& search_rect, OpRegion& reserved_region);

protected:
	OP_STATUS GetReservedRects(FramesDocument* document, const OpRect& search_rect, OpRegion& region);
};

OP_STATUS ReservedRectFinder::FindReservedRects(FramesDocument* root_document, const OpRect& search_rect, OpRegion& reserved_region)
{
	RETURN_IF_ERROR(GetReservedRects(root_document, search_rect, reserved_region));

	DocumentTreeIterator it(root_document);
	BOOL skip_children = FALSE;

	while (it.Next(skip_children))
	{
		FramesDocElm* frame = it.GetFramesDocElm();
		skip_children = FALSE;

		if (FramesDocument* doc = frame->GetCurrentDoc())
		{
			/* Obtain the local document's position in the top level document space. */
			OpPoint translation = frame->GetAbsPos().GetTranslation();
			for (FramesDocument* fd = doc->GetParentDoc(); fd && fd != root_document; fd = fd->GetParentDoc())
				if (FramesDocElm* fde = fd->GetDocManager()->GetFrame())
				{
					VisualDevice* vd = fde->GetVisualDevice();
					translation.x += fde->GetAbsX() - vd->GetRenderingViewX();
					translation.y += fde->GetAbsY() - vd->GetRenderingViewY();
				}

			/* Clip against search rect. */
			VisualDevice* vis_dev = doc->GetVisualDevice();
			OpRect rect(translation.x, translation.y, vis_dev->GetRenderingViewWidth(), vis_dev->GetRenderingViewHeight());
			rect.IntersectWith(search_rect);

			if (!rect.IsEmpty())
			{
				/* Translate into local document space. */
				rect.x += vis_dev->GetRenderingViewX() - translation.x;
				rect.y += vis_dev->GetRenderingViewY() - translation.y;

				OpRegion region;
				RETURN_IF_ERROR(GetReservedRects(doc, rect, region));

				/* Clip and translate found regions back to top level document space. */
				region.Translate(doc->NegativeOverflow(), 0);
				region.IntersectWith(rect);
				region.Translate(translation.x - vis_dev->GetRenderingViewX(), translation.y - vis_dev->GetRenderingViewY());

				reserved_region.IncludeRegion(region);
			}
			else
				skip_children = TRUE;
		}
	}

	return OpStatus::OK;
}

OP_STATUS ReservedRectFinder::GetReservedRects(FramesDocument* document, const OpRect& search_rect, OpRegion& region)
{
	return document->GetReservedRegion(search_rect, region);
}

OP_STATUS ViewportController::GetReservedRegion(const OpRect& rect, OpRegion& reserved_region)
{
	if (FramesDocument* doc = window->DocManager()->GetCurrentVisibleDoc())
	{
		ReservedRectFinder finder;
		return finder.FindReservedRects(doc, rect, reserved_region);
	}

	return OpStatus::OK;
}
#endif // RESERVED_REGIONS

#undef _DBG_OP_RECT
