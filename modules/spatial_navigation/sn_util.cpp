/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


#include "core/pch.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h" // Include win.h for core-2
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/layout.h"

#include "modules/spatial_navigation/sn_util.h"
#include "modules/spatial_navigation/navigation_api.h" // DIR_RIGHT, DIR_LEFT, etc.

#ifdef SPATIAL_NAVIGATION_ALGORITHM

void SnUtil::Transpose(INT32 direction, OpRect& rect)
{
	SnUtil::Transpose(direction, rect.x, rect.y, rect.width, rect.height);
}

void SnUtil::Transpose(INT32 direction, INT32& x, INT32& y, INT32& w, INT32& h)
{
	INT32 newX;
	INT32 newY;
	INT32 newW;
	INT32 newH;
	
	switch (direction)
	{
	case DIR_RIGHT:
		newX = -y-h;
		newY = x;
		newW = h;
		newH = w;
		break;
	case DIR_UP:
		newX = -x-w;
		newY = -y-h;
		newW = w;
		newH = h;
		break;		
	case DIR_LEFT:
		newX = y;
		newY = -x-w;
		newW = h;
		newH = w;
		break;		
	case DIR_DOWN:
	default:
		return; 
	}
	
	x = newX;
	y = newY;
	w = newW;
	h = newH;
}

/*
void SnUtil::UnTranspose(INT32 direction, OpRect& rect)
{
	SnUtil::UnTranspose(direction, rect.x, rect.y, rect.width, rect.height);
}

void SnUtil::UnTranspose(INT32 direction, INT32& x, INT32& y, INT32& w, INT32& h)
{
	switch (direction)
	{
	case DIR_RIGHT:
		SnUtil::Transpose(DIR_LEFT, x, y, w, h);
		break;
	case DIR_UP:
		SnUtil::Transpose(DIR_UP, x, y, w, h);
		break;		
	case DIR_LEFT:
		SnUtil::Transpose(DIR_RIGHT, x, y, w, h);
		break;
	case DIR_DOWN:
	default:
		return;
	}
}
*/

BOOL SnUtil::Intersect(INT32 a1, INT32 a2, INT32 b1, INT32 b2)
{
	if (a2 <= a1 || b2 <= b1)
		return FALSE;

	if ( a2 <= b1 ||
		 b2 <= a1 )
		return FALSE;  // No overlap.
	else
		return TRUE;  // Some overlap
}

BOOL SnUtil::IsPartiallyInView(const OpRect &element, const OpRect &view)
{
	return SnUtil::IsPartiallyInView(element.x, element.y,
									 element.width, element.height,
									 view.x, view.y,
									 view.width, view.height);
}

BOOL SnUtil::IsPartiallyInView(const OpRect &element, FramesDocument* doc, const RECT* iframe_rect)
{
	OpRect visible;

	GetVisibleDocumentPart(visible, doc, iframe_rect);

	return !visible.IsEmpty() &&
		IsPartiallyInView(element.x, element.y, element.width, element.height,
							 visible.x, visible.y, visible.width, visible.height);
}


BOOL SnUtil::IsPartiallyInView(const RECT &element, FramesDocument* doc, const RECT* iframe_rect)
{
	OpRect visible;

	GetVisibleDocumentPart(visible, doc, iframe_rect);

	return !visible.IsEmpty()
		&& Intersect(element.left, element.right, visible.x, visible.x + visible.width)
		&& Intersect(element.top, element.bottom, visible.y, visible.y + visible.height);
}


BOOL SnUtil::IsFullyInView(const RECT &element, FramesDocument* doc, const RECT* iframe_rect)
{
	OpRect visible;

	GetVisibleDocumentPart(visible, doc, iframe_rect);
	
	return SnUtil::IsInView(OpRect(&element), visible);
}

BOOL SnUtil::FitsComfortably(const RECT& rect, FramesDocument* doc, const RECT* iframe_rect)
{
	// An element fits comfortably if it takes less or equal to 50% of height and width
	// Does not consider if it is actually inside the area now, just if it could fit
	// Because viewport dimensions may vary depending on situation (e.g. zoom level, part of iframe is not in view),
	// we have to calculate it on the fly, basing on visible document part

	OpRect visible_rect;

	GetVisibleDocumentPart(visible_rect, doc, iframe_rect);

	if ((rect.right - rect.left) > (visible_rect.width / 2))
		return FALSE;

	if ((rect.bottom - rect.top) > (visible_rect.height / 2))
		return FALSE;

	return TRUE;
}

BOOL SnUtil::CanScroll(INT32 direction, FramesDocument* document, const RECT* iframe_rect)
{
	OpRect visual_viewport;

	GetVisibleDocumentPart(visual_viewport, document, iframe_rect);

	switch (direction)
	{
	case DIR_LEFT:
		return visual_viewport.x > -document->NegativeOverflow();
	case DIR_UP:
		return visual_viewport.y > 0;
	case DIR_RIGHT:
		return visual_viewport.x + visual_viewport.width < document->Width();
	case DIR_DOWN:
		return visual_viewport.y + visual_viewport.height < document->Height();
	default:
		OP_ASSERT(!"Illegal scroll direction");
	}
	
	return TRUE;
}

void SnUtil::GetVisibleDocumentPart(OpRect &visible, FramesDocument* doc, const RECT* iframe_rect /*= NULL*/)
{
	if (doc->IsInlineFrame() && iframe_rect)
	{
		FramesDocument *parentDoc = doc->GetParentDoc();
		OpRect parentViewport;
		RECT iframe_rect_adjusted = *iframe_rect;
		FramesDocument *grandParentDoc = parentDoc->GetParentDoc();
		if (grandParentDoc && grandParentDoc->IsFrameDoc() && SnUtil::HasSpecialFrames(grandParentDoc))
		{
			FramesDocElm* parentDocElm = grandParentDoc->GetFrmDocElm(parentDoc->GetSubWinId());
			OP_ASSERT(parentDocElm);

			parentViewport = grandParentDoc->GetVisualViewport();

			iframe_rect_adjusted.left += parentDocElm->GetAbsX();
			iframe_rect_adjusted.right += parentDocElm->GetAbsX();
			iframe_rect_adjusted.top += parentDocElm->GetAbsY();
			iframe_rect_adjusted.bottom += parentDocElm->GetAbsY();
		}
		else
		{
			parentViewport = parentDoc->GetVisualViewport();
		}

		if (!IsPartiallyInView(iframe_rect->left,iframe_rect->top,iframe_rect->right-iframe_rect->left,iframe_rect->bottom-iframe_rect->top,
			parentViewport.x,parentViewport.y,parentViewport.width,parentViewport.height))
			//If iframe is invisible in parent view
		{
			visible.Set(0,0,0,0);
			return;
		}

		visible = doc->GetVisualViewport();

		INT32 leftCut = parentViewport.x - iframe_rect_adjusted.left
			, rightCut = iframe_rect_adjusted.right - parentViewport.Right()
			, topCut = parentViewport.y - iframe_rect_adjusted.top
			, bottomCut = iframe_rect_adjusted.bottom - parentViewport.Bottom();

		if (leftCut > 0)
		{
			visible.x += leftCut;
			visible.width -= leftCut;
		}
		if (rightCut > 0)
			visible.width -= rightCut;
		if(topCut > 0)
		{
			visible.y += topCut;
			visible.height -= topCut;
		}
		if (bottomCut)
			visible.height -= bottomCut;
	}
	else
	{
		/* If this fails, it means that we are trying to get the visible rectangle of an iframe,
		   but we don't have a valid iframe_rect. Please contact the module owner or describe your case in CORE-36894. */
		OP_ASSERT(!doc->IsInlineFrame());

		FramesDocument *parentDoc = doc->GetParentDoc();
		if (parentDoc && parentDoc->IsFrameDoc() && SnUtil::HasSpecialFrames(parentDoc))
		{
			FramesDocElm* currentElm = parentDoc->GetFrmDocElm(doc->GetSubWinId());
			OpRect frameDocRect;

			visible = parentDoc->GetVisualViewport();

			OP_ASSERT(currentElm);
		
			frameDocRect.Set(currentElm->GetAbsX(),currentElm->GetAbsY(),currentElm->GetWidth(),currentElm->GetHeight());
			visible.IntersectWith(frameDocRect);
			visible.OffsetBy(-frameDocRect.x,-frameDocRect.y);
		}
		else
		{
			visible = doc->GetVisualViewport();
		}
	}
}

BOOL SnUtil::IsPartiallyInView(INT32 x, INT32 y, INT32 w, INT32 h,
									  INT32 viewX, INT32 viewY, 
									  INT32 viewWidth, INT32 viewHeight)
{
	OP_NEW_DBG("SnUtil::IsPartiallyInView", "spatnav-view");
  	OP_DBG(("Is (%d-%d, %d-%d) inside (%d-%d, %d-%d)", 
			x, x+w, y, y+h, viewX, viewX+viewWidth, viewY, viewY+viewHeight));
	
	BOOL ret = (SnUtil::Intersect(x, x+w, viewX, viewX+viewWidth) &&
			SnUtil::Intersect(y, y+h, viewY, viewY+viewHeight));
	if (ret)
		OP_DBG(("yes"));
	else
		OP_DBG(("no"));

	return ret;
}

BOOL SnUtil::IsInView(const OpRect &element, const OpRect &view)
{
	return SnUtil::IsInView(element.x, element.y,
							element.width, element.height,
							view.x, view.y,
							view.width, view.height);
}

BOOL SnUtil::IsInView(INT32 x, INT32 y, INT32 w, INT32 h,
					  INT32 viewX, INT32 viewY, 
					  INT32 viewWidth, INT32 viewHeight)
{
	OP_NEW_DBG("SnUtil::IsInView", "spatnav-view");
  	OP_DBG(("Is (%d-%d, %d-%d) inside (%d-%d, %d-%d)", 
			x, x+w, y, y+h, viewX, viewX+viewWidth, viewY, viewY+viewHeight));
	if ((x < viewX)  || (y < viewY)  || 
		(x+w > viewX+viewWidth) || (y+h > viewY+viewHeight) || 
		(x==viewX && y==viewY && w==viewWidth && h==viewHeight))
	{
		OP_DBG(("no"));
		return FALSE;
	}
	OP_DBG(("yes"));
	return TRUE;
}

/*
INT32 SnUtil::DistancePointToRect(INT32 x, INT32 y, INT32 w, INT32 h, 
								  INT32 xRef, INT32 yRef)
{
	if (x <= xRef && x+w >= xRef)
	{
		if (y <= yRef && y+h >= yRef)
			// Point is within rect
			return 0;
		
		// Distance from point to rect is in y direction
		if (y < yRef)
			return yRef-y-h;
		else
			return y-yRef;
	}

	if (y <= yRef && y+h >= yRef)
	{
		// Distance from point to rect is in x direction
		if (x < xRef)
			return xRef-x-w;
		else
			return x-xRef;
	}
	
	// One of the corners of the rect are closest
	INT32 xDistance = 0;
	INT32 yDistance = 0;

	if (x < xRef)
		xDistance = xRef-x-w;
	else
		xDistance = x-xRef;

	if (y < yRef)
		yDistance = yRef-y-h;
	else
		yDistance = y-yRef;

	return static_cast<INT32>(op_sqrt(double(xDistance*xDistance+yDistance*yDistance)));
}
*/

BOOL SnUtil::HasSpecialFrames(FramesDocument* frmDoc)
{
	return frmDoc->GetFramesStacked() || frmDoc->GetSmartFrames();
}

FramesDocument* SnUtil::GetDocToScroll(FramesDocument* frmDoc)
{
	if (!HasSpecialFrames(frmDoc))
		return frmDoc;

	if (frmDoc->IsFrameDoc() || frmDoc->GetParentDoc())
		return frmDoc->GetTopFramesDoc();
	else
		return frmDoc->GetWindow()->DocManager()->GetCurrentDoc();
}

BOOL
SnUtil::HasEventHandler(FramesDocument* doc, HTML_Element* hElm, HTML_Element** html_elm)
{
	HTML_Element* handling_elm = hElm;
	BOOL has_handler = hElm->HasEventHandler(doc, ONCLICK, TRUE) ||
	                   hElm->HasEventHandler(doc, ONMOUSEOVER, TRUE) ||
	                   hElm->HasEventHandler(doc, ONMOUSEENTER, TRUE);

	if (has_handler)
	{
		// Do something clever, like look at handling_elm's Frame and see if 
		// it is much larger than hElm's, in that case, the event handler might 
		// not be just for hElm.
		// Perhaps check if handling_elm has more leaves than this?

		if (html_elm)
			*html_elm = handling_elm;

		return TRUE;
	}
	else
		return FALSE;
}

/*
short SnUtil::RestrictedTo(short value, short lower_limit, short upper_limit)
{
	OP_ASSERT(lower_limit <= upper_limit);
	if (value < lower_limit)
		return lower_limit;
	else if (value > upper_limit)
		return upper_limit;
	return value;	
}

int SnUtil::RestrictedTo(int value, int lower_limit, int upper_limit)
{
	OP_ASSERT(lower_limit <= upper_limit);
	if (value < lower_limit)
		return lower_limit;
	else if (value > upper_limit)
		return upper_limit;
	return value;	
}
*/
long SnUtil::RestrictedTo(long value, long lower_limit, long upper_limit)
{
	OP_ASSERT(lower_limit <= upper_limit);
	if (value < lower_limit)
		return lower_limit;
	else if (value > upper_limit)
		return upper_limit;
	return value;	
}

BOOL SnUtil::GetImageMapAreaRect(HTML_Element* areaElm, HTML_Element* mapElm, const RECT& mapRect, RECT& rect)
{
	if (areaElm->GetAREA_Shape() == AREA_SHAPE_DEFAULT)
	{
		rect.left = mapRect.left;
		rect.right = mapRect.right;
		rect.top = mapRect.top;
		rect.bottom = mapRect.bottom;
	}
	else
	{
		CoordsAttr* ca = (CoordsAttr*)areaElm->GetAttr(ATTR_COORDS, ITEM_TYPE_COORDS_ATTR, (void*)0);
		if (!ca)
			return FALSE;

		int coords_len = ca->GetLength();
		int* coords = ca->GetCoords();
		int width_scale = (int)mapElm->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_WIDTH_SCALE, SpecialNs::NS_LAYOUT, 1000);
		int height_scale = (int)mapElm->GetSpecialNumAttr(Markup::LAYOUTA_IMAGEMAP_HEIGHT_SCALE, SpecialNs::NS_LAYOUT, 1000);
		switch (areaElm->GetAREA_Shape())
		{
		case AREA_SHAPE_CIRCLE:
			{
				if (coords_len < 3)
					return FALSE;
				int radius_scale = MIN(width_scale, height_scale);
				int radius = coords[2]*radius_scale/1000;
				rect.left = mapRect.left + coords[0]*width_scale/1000-radius;
				rect.right = mapRect.left + coords[0]*width_scale/1000+radius;
				rect.top = mapRect.top + coords[1]*height_scale/1000-radius;
				rect.bottom = mapRect.top + coords[1]*height_scale/1000+radius;
				break;
			}
		case AREA_SHAPE_POLYGON:
			{
				int counter = 0;
				rect.left = LONG_MAX;
				rect.right = LONG_MIN;
				rect.top = LONG_MAX;
				rect.bottom = LONG_MIN;
				while (counter < coords_len)
				{
					if (coords[counter] < rect.left)
						rect.left = coords[counter];
					if (coords[counter] > rect.right)
						rect.right = coords[counter];
					if (coords[counter+1] < rect.top)
						rect.top = coords[counter+1];
					if (coords[counter+1] > rect.bottom)
						rect.bottom = coords[counter+1];
					counter = counter + 2;
				}
				rect.left *= width_scale;
				rect.left /= 1000;
				rect.right *= width_scale;
				rect.right /= 1000;
				rect.top *= height_scale;
				rect.top /= 1000;
				rect.bottom *= height_scale;
				rect.bottom /= 1000;
				rect.left += mapRect.left;
				rect.right += mapRect.left;
				rect.top += mapRect.top;
				rect.bottom += mapRect.top;
				break;
			}
		case AREA_SHAPE_RECT:
			{
				if (coords_len < 4)
					return FALSE;

				if (coords[2] >= coords[0])
				{
					// Normal case
					rect.left = mapRect.left + coords[0]*width_scale/1000;
					rect.right = mapRect.left + coords[2]*width_scale/1000;
				}
				else
				{
					// Coords are swapped
					rect.left = mapRect.left + coords[2]*width_scale/1000;
					rect.right = mapRect.left + coords[0]*width_scale/1000;
				}
				if (coords[3] >= coords[1])
				{
					// Normal case
					rect.top = mapRect.top + coords[1]*height_scale/1000;
					rect.bottom = mapRect.top + coords[3]*height_scale/1000;
				}
				else
				{
					// Coords are swapped
					rect.top = mapRect.top + coords[3]*height_scale/1000;
					rect.bottom = mapRect.top + coords[1]*height_scale/1000;
				}
				break;
			}
		default:
			OP_ASSERT(!"Unknown image map area type!");
			return FALSE;
		}
	}
	
	return TRUE;
}

HTML_Element* SnUtil::GetScrollableAncestor(HTML_Element* helm, FramesDocument *doc, OpRect* scrollableRect)
{
	HTML_Element* scrollable_element = NULL;

	while ((helm = helm->Parent()) != NULL)
		if (Box* box = helm->GetLayoutBox())
			if (Content* content = box->GetContent())
				if (ScrollableArea *scrollable = content->GetScrollable())
				{
					BOOL overflowed_x = scrollable->IsOverflowedX();
					BOOL overflowed_y = scrollable->IsOverflowedY();

					if (overflowed_x || overflowed_y)
#ifndef SN_SCROLL_OVERFLOW_HIDDEN
						// Additional requirement - we must have a scrollbar where the content overflows.
						if (overflowed_x && scrollable->HasHorizontalScrollbar() ||
							overflowed_y && scrollable->HasVerticalScrollbar())
#endif // !SN_SCROLL_OVERFLOW_HIDDEN
						{
							scrollable_element = helm;
							break;
						}
				}
#ifdef PAGED_MEDIA_SUPPORT
				else
					if (PagedContainer* paged = content->GetPagedContainer())
						if (paged->GetTotalPageCount() > 1)
						{
							scrollable_element = helm;
							break;
						}
#endif // PAGED_MEDIA_SUPPORT

	if (scrollable_element && scrollableRect)
		GetScrollableElementRect(scrollable_element->GetLayoutBox()->GetContent(), doc, *scrollableRect);

	return scrollable_element;
}

void SnUtil::GetScrollableElementRect(Content* content, FramesDocument *doc, OpRect& scrollableRect)
{
	RECT rect;

	content->GetPlaceholder()->GetRect(doc, PADDING_BOX, rect);
	scrollableRect.Set(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);

	if (ScrollableArea* scrollable = content->GetScrollable())
	{
		if (scrollable->HasVerticalScrollbar())
		{
			UINT32 scrollbarWidth = scrollable->GetVerticalScrollbarWidth();

			if (scrollable->LeftHandScrollbar())
				scrollableRect.x += scrollbarWidth;

			scrollableRect.width -= scrollbarWidth;
		}

		if (scrollable->HasHorizontalScrollbar())
			scrollableRect.height -= scrollable->GetHorizontalScrollbarWidth();
	}
#ifdef PAGED_MEDIA_SUPPORT
	else
		if (PagedContainer* paged = content->GetPagedContainer())
			scrollableRect.height -= paged->GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT
		else
			OP_ASSERT(!"Unknown container type");
}


BOOL ElementTypeCheck::IsFormObject(HTML_Element* element, FramesDocument* doc)
{
	if (element->Type() == HE_SELECT)  // only focus select boxes with at least one enabeled option
	{
		HTML_Element* stop = element->LastLeaf();
		if (stop)
			stop = stop->Next();

		for (HTML_Element* he = element->FirstChild(); he && he != stop; he = he->Next())
			if (he->Type() == HE_OPTION && !he->GetBoolAttr(ATTR_DISABLED))
				return TRUE;  // found an enabeled option

		return FALSE;  // a select box with all options disabeled
	}

	if (element->GetFormObject() && !element->GetBoolAttr(ATTR_DISABLED))
		return TRUE;

	if (element->GetInputType() == INPUT_IMAGE)
		return TRUE;

	if (!element->IsDisabled(doc) && FormManager::IsButton(element) && element->GetLayoutBox())
		return TRUE;

	return FALSE;
}

BOOL3 ElementTypeCheck::IsTabbable(HTML_Element* element)
{
	INT tabindex = element->GetTabIndex();

	if (tabindex == -1)
		return MAYBE;

	if (tabindex < 0)
		return NO;

	return YES;
}

BOOL ElementTypeCheck::IsClickable(HTML_Element* element, FramesDocument* doc)
{
	if (element->IsDisabled(doc))
		return FALSE;

	return (IsLink(element, doc) || IsFormObject(element, doc) || (SnUtil::HasEventHandler(doc, element) && element->Type() != HE_BODY && element->Type() != HE_DOC_ROOT)) || IsTabbable(element) == YES;
}

BOOL ElementTypeCheck::IsHeading(HTML_Element* element, FramesDocument* doc)
{
	if (element->Type() >= HE_H1 && element->Type() <= HE_H6)
		return TRUE;

	return FALSE;
}

BOOL ElementTypeCheck::IsParagraph(HTML_Element* element, FramesDocument* doc)
{
	return element->Type() == HE_P;
}

BOOL ElementTypeCheck::IsImage(HTML_Element* element, FramesDocument* doc)
{
	return element->Type() == HE_IMG;
}

BOOL ElementTypeCheck::IsLink(HTML_Element* element, FramesDocument* doc)
{
	if (element->Type() == HE_AREA)
		return TRUE;

	if (element->GetA_HRef(doc))
		return TRUE;
#ifdef _WML_SUPPORT_
	else if (IsWMLLink(element, doc))
		return TRUE;
#endif // _WML_SUPPORT_

	// Elements which have a width of 0 are not considered for navigation.
	// But such elements could still be links, so if this element is not
	// a link, check if any of its parents are.
	for (HTML_Element* elem = element->Parent(); elem; elem = elem->Parent())
	{
		Box* box = elem->GetLayoutBox();
		if (box)
		{
			// only need to search until finding first element with a width
			// (and a layout box)
			if (box->GetWidth() != 0)
				break;

			if (elem->GetA_HRef(doc))
				return TRUE;
#ifdef _WML_SUPPORT_
			else if (IsWMLLink(elem, doc))
				return TRUE;
#endif // _WML_SUPPORT_
		}
	}

	return FALSE;
}

#ifdef _WML_SUPPORT_
BOOL ElementTypeCheck::IsWMLLink(HTML_Element* element, FramesDocument* doc)
{
	if (element->GetNsType() == NS_WML)
	{
		switch ((WML_ElementType) element->Type())
		{
		case WE_DO:
		case WE_ANCHOR:
			return TRUE;
		}
	}

	return FALSE;
}
#endif // _WML_SUPPORT_

BOOL ElementTypeCheck::IsSpecificLink(HTML_Element* element, const uni_char* protocol_prefix)
{
	if (!element->HasAttr(ATTR_HREF))
		return FALSE;

	const uni_char* href = element->GetA_HRef(NULL);
    if (!href)
	    return FALSE;
        
    return uni_strni_eq(href, protocol_prefix, uni_strlen(protocol_prefix));
}

BOOL ElementTypeCheck::IsMailtoLink(HTML_Element* element, FramesDocument* )
{
	return IsSpecificLink(element, UNI_L("mailto:"));
}

BOOL ElementTypeCheck::IsTelLink(HTML_Element* element, FramesDocument* doc)
{
	return IsSpecificLink(element, UNI_L("tel:"));
}

BOOL ElementTypeCheck::IsWtaiLink(HTML_Element* element, FramesDocument* doc)
{
	return IsSpecificLink(element, UNI_L("wtai:"));
}

BOOL ElementTypeCheck::IsIFrameElement(HTML_Element* elm, LogicalDocument* logdoc)
{
	if (!elm)
		return FALSE;

	if (elm->Type() == HE_IFRAME)
		return TRUE;

	if (logdoc && elm->Type() == HE_OBJECT)
	{
		URL* inline_url = NULL;
        HTML_ElementType element_type = elm->Type();
        inline_url = elm->GetUrlAttr(ATTR_DATA, NS_IDX_HTML, logdoc);

        OP_BOOLEAN resolve_status = elm->GetResolvedObjectType(inline_url, element_type, logdoc);
        if (resolve_status == OpBoolean::IS_TRUE && element_type == HE_IFRAME)
            return TRUE;
	}

	return FALSE;
}

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
BOOL ElementTypeCheck::IsNavigatePlugin(HTML_Element* helm)
{
        Box* box = helm ? helm->GetLayoutBox() : NULL;

        if (box &&
                box->IsContentEmbed() &&
                ((EmbedContent *) ((Content_Box *) box)->GetContent())->GetOpNS4Plugin())
                return TRUE;

        return FALSE;
}
#endif // _PLUGIN_NAVIGATION_SUPPORT_

#endif // SPATIAL_NAVIGATION_ALGORITHM

