/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _SPAT_NAV_SUPPORT_

#include "modules/spatial_navigation/sn_traversalobject.h"
#include "modules/spatial_navigation/sn_util.h"
#include "modules/spatial_navigation/navigation_api.h"
#include "modules/spatial_navigation/sn_algorithm.h"

#include "modules/layout/box/inline.h"
#include "modules/layout/content/content.h"
#include "modules/layout/box/tables.h"
#include "modules/doc/html_doc.h"

#ifdef SVG_SUPPORT
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_tree_iterator.h"
#endif // SVG_SUPPORT

//
// SnTraversalObject
// Public methods
//

SnTraversalObject::SnTraversalObject(FramesDocument* doc, FramesDocument* currentDoc, VisualDevice* visual_device, const RECT& area, short direction, OpSnNavFilter* nav_filter, HTML_Element* previous_element, HTML_Element* previous_scrollable, const RECT& previous_area, BOOL fourway, BOOL reconsider, const RECT* iframe_rect_ptr, OpVector<HTML_Element> *exclude)
  : VisualTraversalObject(doc, visual_device, area),
	currentElm(NULL),
	currentScrollable(previous_scrollable),
	lastElm(previous_element),
	reconsider_as_scrollable(reconsider),
	lastHandlerElm(NULL),
	lastScrollable(previous_scrollable),
	lastRect(previous_area),
	alternativeElement(NULL),
    direction(direction),
	nav_filter(nav_filter),
	fourway(fourway),
	inCurrentScrollable(FALSE),
	algorithm(NULL),
	currentDoc(currentDoc),
	navDirElement(NULL),
	navDirDocument(NULL),
	iframe_rect_ptr(iframe_rect_ptr),
	// trace ?
	bestElmDist(INT32_MAX),
	exclusions(exclude)
{
	SetTraverseType(TRAVERSE_ONE_PASS);

	SN_EMPTY_RECT(alternativeRect);
	SN_EMPTY_RECT(linkRect);

	//Fix for start navigation problem, when iframe/scrollable/paged container touches the top or left edge of the document
	//We move the starting 'stripe' or point to be one pixel out of the view
	if (!lastElm)
	{
		switch (direction)
		{
		case DIR_UP:
			lastRect.top = lastRect.bottom;
			lastRect.bottom++;
			break;
		case DIR_LEFT:
			lastRect.left = lastRect.right;
			lastRect.right++;
			break;
		case DIR_RIGHT:
			lastRect.right = lastRect.left;
			lastRect.left--;
			break;
		default:	// DIR_DOWN
			lastRect.bottom = lastRect.top;
			lastRect.top--;
			break;
		}
	}
	else if (lastScrollable)
	{
		switch (direction)
		{
		case DIR_UP:
			lastRect.top = lastRect.bottom-1;
			break;
		case DIR_LEFT:
			lastRect.left = lastRect.right-1;
			break;
		case DIR_RIGHT:
			lastRect.right = lastRect.left+1;
			break;
		default:	// DIR_DOWN
			lastRect.bottom = lastRect.top+1;
			break;
		}
	}

	if (lastElm)
	{
		// Not interested in the return value, only set lastHandlerElm
		HasEventHandler(lastElm, &lastHandlerElm);
	}
}

OP_STATUS
SnTraversalObject::Init(BOOL activeLinkHasLineBreak)
{
#ifndef USE_4WAY_NAVIGATION
	if (!fourway)
		algorithm = OP_NEW(TwowayAlgorithm, ());
	else
#endif // USE_4WAY_NAVIGATION
		algorithm = OP_NEW(FourwayAlgorithm, ());

	if (algorithm == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return algorithm->Init(lastRect, direction, lastElm != NULL, activeLinkHasLineBreak);
}

void
SnTraversalObject::FindNextLink(HTML_Element* root)
{
	Traverse(root);
	if (currentElm == NULL && navDirElement == NULL)
	{
		currentElm = alternativeElement;
		linkRect = alternativeRect;
	}
}

FramesDocument* GetFrameWithId(FramesDocument* root_doc, const uni_char* id)
{
	DocumentTreeIterator iterator(root_doc);
	while (iterator.Next())
	{
		FramesDocElm* fde = iterator.GetFramesDocElm();
		if (fde && fde->GetName() && uni_strcmp(fde->GetName(), id) == 0)
		   return iterator.GetFramesDocument();
	}
	return NULL;
}

BOOL
SnTraversalObject::FindAndSetNavDirLinkInFrameset(FramesDocument* frmDoc, const uni_char* id)
{
	FramesDocElm* frmDocElm = frmDoc->GetFrmDocRoot();

	if (frmDocElm)
	{
		frmDocElm = frmDocElm->FirstLeaf();
		while (frmDocElm)
		{
			if (FindAndSetNavDirLinkInFrameset(frmDocElm->GetCurrentDoc(), id))
				return TRUE;
			frmDocElm = frmDocElm->Next();
		}
	}
	else
	{
		LogicalDocument* logdoc = frmDoc->GetLogicalDocument();
		if (logdoc)
		{
			NamedElementsIterator it;
			logdoc->GetNamedElements(it, id, TRUE, FALSE);
			navDirElement = it.GetNextElement();
			if (navDirElement)
			{
				navDirDocument = frmDoc;
				return TRUE;
			}
		}
	}
	return FALSE;
}

void
SnTraversalObject::FindAndSetNavDirLink(CSS_gen_array_decl* cssdecl)
{
	const CSS_generic_value* values = cssdecl->GenArrayValue();
	uni_char* id = values[0].GetString();
	if (id)
	{
		if (cssdecl->ArrayValueLen() > 1)
		{
			short type = values[1].value_type;
			if (type == CSS_IDENT)
			{
				if (values[1].value.type != CSS_VALUE_root)
				{
					navDirDocument = document;
				}
				// else check all frames below
			}
			else	// CSS_STRING_LITERAL
			{
				uni_char* frameId = values[1].GetString();
				if (frameId)
				{
					FramesDocument* tmpdoc = GetFrameWithId(currentDoc, frameId);
					if (tmpdoc)
						navDirDocument = tmpdoc;
					else
						navDirDocument = document;
				}
			}
		}
		else
		{
			navDirDocument = document;
		}

		if (navDirDocument)
		{
			LogicalDocument* logdoc = navDirDocument->GetLogicalDocument();
			if (logdoc)
			{
				NamedElementsIterator it;
				logdoc->GetNamedElements(it, id, TRUE, FALSE);
				navDirElement = it.GetNextElement();
				if (navDirElement == NULL)
					navDirDocument = NULL;
			}
		}
		else
		{
			FindAndSetNavDirLinkInFrameset(currentDoc, id);
		}
	}
}

BOOL SnTraversalObject::TestNavDirLink(LayoutProperties* layout_props, HTML_Element* element)
{
	LayoutProperties* element_props;
	BOOL matchesLastElm = FALSE;

	if (element)
	{
		matchesLastElm = lastElm && lastElm == element;
		if (matchesLastElm)
		{
			element_props = layout_props->GetChildProperties(document->GetHLDocProfile(), element);
			if (!element_props)
			{
				SetOutOfMemory();
				return FALSE;
			}
		}
		else
			return FALSE;
	}
	else
	{
		element_props = layout_props;
		matchesLastElm = lastElm && lastElm == layout_props->html_element;
	}

	if (matchesLastElm)
	{
		HTMLayoutProperties* htmlayoutprops = element_props->GetProps();
		CSS_gen_array_decl* cssdecl = NULL;

		if (direction == DIR_UP && htmlayoutprops->navup_cp != NULL && htmlayoutprops->navup_cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
			cssdecl = static_cast<CSS_gen_array_decl*>(htmlayoutprops->navup_cp);
		if (direction == DIR_DOWN && htmlayoutprops->navdown_cp != NULL && htmlayoutprops->navdown_cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
			cssdecl = static_cast<CSS_gen_array_decl*>(htmlayoutprops->navdown_cp);
		if (direction == DIR_LEFT && htmlayoutprops->navleft_cp != NULL && htmlayoutprops->navleft_cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
			cssdecl = static_cast<CSS_gen_array_decl*>(htmlayoutprops->navleft_cp);
		if (direction == DIR_RIGHT && htmlayoutprops->navright_cp != NULL && htmlayoutprops->navright_cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
			cssdecl = static_cast<CSS_gen_array_decl*>(htmlayoutprops->navright_cp);

		if (cssdecl)
		{
			FindAndSetNavDirLink(cssdecl);
			return TRUE;
		}
	}

	return FALSE;
}

SnTraversalObject::HitBoxResult
SnTraversalObject::HitBox(LayoutProperties& layout_props)
{
	OP_NEW_DBG("SnTraversalObject::HitBox", "spatnav");

	if (TestNavDirLink(&layout_props, NULL))
		return NO_HIT;

	// Don't hit hidden elements
	if (layout_props.GetProps()->visibility != CSS_VALUE_visible)
		return NO_HIT;

	HTML_Element* html_element = layout_props.html_element;

	// We can't use elements which are inserted by layout as they are volatile and might be deleted
	// by the time we want to highlight, send events etc
	if (html_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
		return NO_HIT;

	HTML_ElementType type = html_element->Type();
	if (type == HE_IFRAME || type == HE_OBJECT || type == HE_EMBED)
	{
		return HIT_ELEMENT;
	}

	if (html_element->HasAttr(ATTR_USEMAP))
	{
		return HIT_ELEMENT;
	}
	
	return HitEventHandlerElement(html_element);

	// ELEMENT_TYPE_LINK

	// ELEMENT_TYPE_OBJECT

		// H2FIX: This must be taken care of somehow
#ifdef _PLUGIN_NAVIGATION_SUPPORT_
		/* Check for plugins that aren't within object-tags */

//		if (box->IsContentEmbed() && ((EmbedContent *) ((Content_Box *) box)->GetContent())->GetOpNS4Plugin())
//			return TRUE;
#endif

	// ELEMENT_TYPE_FORM
	// Check for forms objects
/*	if (html_element->GetFormObject())
		if (!html_element->GetBoolAttr(ATTR_DISABLED))
		{
			if (search_type & ELEMENT_TYPE_FORM)
				return TRUE;

			// Get more detailed information on the type of the currently
			// active element.

			switch (html_element->GetInputType())
			{
			case INPUT_TEXT:
				if (search_type & ELEMENT_TYPE_TEXT)
					return TRUE;

				break;
			case INPUT_CHECKBOX:
				if (search_type & ELEMENT_TYPE_CHECKBOX)
					return TRUE;

				break;
			case INPUT_RADIO:
				if (search_type & ELEMENT_TYPE_RADIO)
					return TRUE;

				break;
			case INPUT_SUBMIT:
				if (search_type & ELEMENT_TYPE_SUBMIT)
					return TRUE;

				break;
			case INPUT_RESET:
				if (search_type & ELEMENT_TYPE_RESET)
					return TRUE;

				break;
			case INPUT_IMAGE:
				if (search_type & ELEMENT_TYPE_IMAGE)
					return TRUE;

				break;
			case INPUT_PASSWORD:
				if (search_type & ELEMENT_TYPE_PASSWORD)
					return TRUE;

				break;
			case INPUT_BUTTON:
				if (search_type & ELEMENT_TYPE_BUTTON)
					return TRUE;

				break;
			case INPUT_PUSHBUTTON:
				if (search_type & ELEMENT_TYPE_INPUT_BUTTON)
					return TRUE;

				break;
			case INPUT_FILE:
				if (search_type & ELEMENT_TYPE_FILE)
					return TRUE;

				break;
			case INPUT_TEXTAREA:
				if (search_type & ELEMENT_TYPE_TEXTAREA)
					return TRUE;

				break;
			}

			if (search_type & ELEMENT_TYPE_SELECT && html_element->Type() == HE_SELECT)
				return TRUE;

			if (search_type & ELEMENT_TYPE_TEXTAREA && html_element->Type() == HE_TEXTAREA)
				return TRUE;
		}

	if (html_element->GetInputType() == INPUT_IMAGE)
		if (search_type & ELEMENT_TYPE_FORM || search_type & ELEMENT_TYPE_INPUT_IMAGE)
			return TRUE;

	if (html_element->Type() == HE_BUTTON)
		if (!html_element->GetBoolAttr(ATTR_DISABLED))
			if (search_type & ELEMENT_TYPE_FORM || search_type & ELEMENT_TYPE_BUTTON)
				return TRUE;
*/

	// ELEMENT_TYPE_JS

	// ELEMENT_TYPE_IMAGE

}

SnTraversalObject::HitBoxResult
SnTraversalObject::HitEventHandlerElement(HTML_Element* html_element)
{
	if (nav_filter->AcceptElement(html_element, document))
	{
		HTML_Element* handlerElm = NULL;
		if (currentElm && html_element != currentElm && !currentElm->GetAnchor_HRef(document) && HasEventHandler(html_element, &handlerElm))
		{
			HTML_Element *iter = html_element;
			while (iter && HasEventHandler(iter, &handlerElm) && handlerElm)
			{
				if (handlerElm == currentElm)
				{
					// We are already inside another element with the same event handler, 
					// so use this element (the inner one) if it is not the same element
					// that was focused last time.
					if (lastElm == html_element)
						return NO_HIT;
					return CHANGE_ELEMENT;
				}
				iter = handlerElm->Parent();
			}
		}
		return HIT_ELEMENT;
	}
	return NO_HIT;
}

static void SetElementRect(HTML_Element* html_element, const RECT& box_area, RECT& rect, const HTMLayoutProperties* props /*=NULL*/)
{
	if (html_element->IsMatchingType(HE_IFRAME, NS_HTML) && props)
	{
		//adjust rect to be just content rect, because it is used by SnHandler to calculate
		//visible part of iframe document	
		rect.top = box_area.top + props->padding_top + props->border.top.width;
		rect.left = box_area.left + props->padding_left + props->border.left.width;
		rect.bottom = box_area.bottom - (props->padding_bottom + props->border.bottom.width);
		rect.right = box_area.right - (props->padding_right + props->border.right.width);
	}
	else
	{
		rect.top = box_area.top;
		rect.left = box_area.left;
		rect.bottom = box_area.bottom;
		rect.right = box_area.right;
	}
}

void
SnTraversalObject::SetCurrentElement(HTML_Element* html_element, const RECT& box_area, const HTMLayoutProperties* props /*=NULL*/)
{
	currentElm = html_element;
	SetElementRect(html_element, box_area, linkRect, props);
}

void
SnTraversalObject::SetAlternativeElement(HTML_Element* html_element, const RECT& box_area, const HTMLayoutProperties* props /*=NULL*/)
{
	alternativeElement = html_element;
	SetElementRect(html_element, box_area, alternativeRect, props);
}

void
SnTraversalObject::ResizeSearchArea()
{
	INT32 bestRank = algorithm->GetBestRectValue() + algorithm->GetFuzzyMargin();
	if (bestRank < 0)
		bestRank = 0;
	if (lastElm)
	{
		long boundary;

		// To make the points in lastRect relative to the rendering viewport, just as area is.
		int view_x = visual_device->GetRenderingViewX();
		int view_y = visual_device->GetRenderingViewY();

		switch (direction)
		{
		case DIR_UP:
			boundary = lastRect.top - view_y - (int)(op_sqrt((float)bestRank)) - 1;
			area.top = SnUtil::RestrictedTo(boundary, area.top, area.bottom);
			break;

		case DIR_DOWN:
			boundary = lastRect.bottom - view_y + (int)(op_sqrt((float)bestRank)) + 1;
			area.bottom = SnUtil::RestrictedTo(boundary, area.top, area.bottom);
			break;

		case DIR_LEFT:
			boundary = lastRect.left - view_x - (int)(op_sqrt((float)bestRank)) - 1;
			area.left = SnUtil::RestrictedTo(boundary, area.left, area.right);
			break;

		case DIR_RIGHT:
			boundary = lastRect.right - view_x + (int)(op_sqrt((float)bestRank)) + 1;
			area.right = SnUtil::RestrictedTo(boundary, area.left, area.right);
			break;

		default:
			break;
		}
	}
}

void
SnTraversalObject::CheckUsemapElement(HTML_Element* html_element, const RECT& rect)
{
	LogicalDocument* logDoc = document->GetLogicalDocument();
	URL* map_url = html_element->GetUrlAttr(ATTR_USEMAP, NS_IDX_HTML, logDoc);
	HTML_Element* mapElm = logDoc->GetNamedHE(map_url->UniRelName());
	if (mapElm)
	{
		HTML_Element* areaElm = mapElm->GetNextLinkElementInMap(TRUE, mapElm);

		// Check if the filter is accepting area elements
		if (areaElm && !nav_filter->AcceptElement(areaElm, document))
			return;

		while (areaElm)
		{
			HTML_Document* doc = document->GetHtmlDocument();
			HTML_Element* areaObjElm = doc->FindAREAObjectElement(mapElm, document->GetLogicalDocument()->GetRoot());
			if (areaObjElm)
			{
				RECT area_rect;
				if (SnUtil::GetImageMapAreaRect(areaElm, areaObjElm, rect, area_rect))
					IntersectBox(areaElm, area_rect);
			}

			areaElm = areaElm->GetNextLinkElementInMap(TRUE, mapElm);
		}
	}
}

/** How far we should search in the logical tree until we find a common parent. */
#define SN_SEARCH_HEIGHT 5

INT32
SnTraversalObject::DistanceToLastElm(HTML_Element* html_element)
{
	HTML_Element *elm1 = lastElm;
	
	for (int i = 0; i < SN_SEARCH_HEIGHT && elm1; i++, elm1 = elm1->Parent())
	{
		HTML_Element *elm2 = html_element;
		for (int j = 0; j < SN_SEARCH_HEIGHT && elm2; j++, elm2 = elm2->Parent())
			if (elm1 == elm2)
				return i;
	}		
	return INT32_MAX;
}

INT32
SnTraversalObject::SymDistanceToLastElm(HTML_Element* html_element)
{
	HTML_Element *elm1 = lastElm;
	HTML_Element *elm2 = html_element;
	
	for (int i = 0; i < SN_SEARCH_HEIGHT && elm1 && elm2; i++, elm1 = elm1->Parent(), elm2 = elm2->Parent())
	{
		if (elm1 == elm2)
			return i;
	}
	return INT32_MAX;
}


void
SnTraversalObject::IntersectBox(HTML_Element* html_element, const RECT& box_area, BOOL scrollable/* = FALSE*/, const HTMLayoutProperties* props /*= NULL*/)
{
	// We'll accept the element, even if it's the same as lastElm if it's a scrollable
	// if it was last considered as navigatable element (and not as a scrollable) 
	BOOL reconsider_current_as_scrollable = html_element == lastElm && scrollable && reconsider_as_scrollable;

	if ((html_element != lastElm && html_element != lastHandlerElm) 
		|| html_element->HasAttr(ATTR_USEMAP)
		|| reconsider_current_as_scrollable)
	{
		if (html_element->HasAttr(ATTR_USEMAP))
		{
			// First, check all area elements
			CheckUsemapElement(html_element, box_area);

			// If the usemap element itself is not accepted, return, otherwise it will be checked below
			if (!nav_filter->AcceptElement(html_element, document) || html_element == lastElm)
				return;
		}

		// Normal case
		RECT rect = GetVisualDeviceTransform()->ToBBox(box_area);

		/** Fix for CORE-34548. If an element, that is encountered later in traverse has the same rect
		    as the current best elm's (or the current alternative's) switch the element to the new one.
		    That does not change the best rank according to the spatnav algorithm.
		    The new one is either descendant or an element with higher z-index (ONE_PASS_TRAVERSE).
		    Make an exception for IFRAME, since it may modify the stored rect and thus the check
		    would be invalid. */
		if (!scrollable)
		{
			if (currentElm)
			{
				if (!currentElm->IsMatchingType(HE_IFRAME, NS_HTML) &&
						rect.left == linkRect.left && rect.top == linkRect.top &&
						rect.right == linkRect.right && rect.bottom == linkRect.bottom)
				{
					SetCurrentElement(html_element, rect, props);
					return;
				}
			}
			else if (alternativeElement && !alternativeElement->IsMatchingType(HE_IFRAME, NS_HTML) &&
					rect.left == alternativeRect.left && rect.top == alternativeRect.top &&
					rect.right == alternativeRect.right && rect.bottom == alternativeRect.bottom)
			{
				SetAlternativeElement(html_element, rect, props);
				return;
			}
		}

		if (rect.top >= rect.bottom || rect.left >= rect.right)
			return;

		/* If this fails, it means that we will try to get the visible rectangle of an iframe,
		   but we don't have a valid iframe rect in its parent doc coordinates, which is needed
		   to properly compute the visible rectangle.
		   Please contact the module owner or describe your case in CORE-36894. */
		OP_ASSERT(!document->IsInlineFrame() || iframe_rect_ptr);

		// Make sure the rect is visible
#ifdef SN_ONLY_CONSIDER_FULLY_IN_VIEW_ELEMENTS
		if (SnUtil::CanScroll(direction, document, iframe_rect_ptr) && SnUtil::FitsComfortably(rect, document, iframe_rect_ptr))
		{
			if (!SnUtil::IsFullyInView(rect, document, iframe_rect_ptr))
				return;
		}
		else
#endif // SN_ONLY_CONSIDER_FULLY_IN_VIEW_ELEMENTS
		if (!SnUtil::IsPartiallyInView(rect, document, iframe_rect_ptr))
			return;

		OpSnAlgorithm::EvaluationResult result;
		
		if (reconsider_current_as_scrollable)
		{
			// We know we want this element
			algorithm->ForceBestRectValue(rect);
			result = OpSnAlgorithm::START_ELEMENT;
		}
		else
			result = algorithm->EvaluateLink(rect);

#ifdef SN_DEBUG_TRACE
		trace.TraceRect(rect, algorithm->GetCurrentRectValue(), result);
#endif // SN_DEBUG_TRACE
		if (result == OpSnAlgorithm::BEST_ELEMENT || result == OpSnAlgorithm::START_ELEMENT
			 || result == OpSnAlgorithm::BEST_ELEMENT_BUT_CLOSE || result == OpSnAlgorithm::NOT_BEST_BUT_CLOSE)
		{
			INT32 distance = DistanceToLastElm(html_element);
			OP_ASSERT (result != OpSnAlgorithm::NOT_BEST_BUT_CLOSE || currentElm);

			if (result == OpSnAlgorithm::BEST_ELEMENT_BUT_CLOSE ||
				result == OpSnAlgorithm::NOT_BEST_BUT_CLOSE)
			{
				if (distance > bestElmDist)
					return;
				if (distance == bestElmDist && result == OpSnAlgorithm::NOT_BEST_BUT_CLOSE)
					return;
				if (distance > 0 && SymDistanceToLastElm(html_element) == INT32_MAX && result == OpSnAlgorithm::NOT_BEST_BUT_CLOSE)
					return;
			}
			bestElmDist = distance;
			SetCurrentElement(html_element, rect, props);
			if (scrollable)
				currentScrollable = html_element;
			else
				currentScrollable = lastScrollable;
			HTML_Element* handlerElm = NULL;
			if (!HasEventHandler(html_element, &handlerElm))
				ResizeSearchArea();
		}
		else if (result == OpSnAlgorithm::ALTERNATIVE_ELEMENT)
		{
			SetAlternativeElement(html_element, rect, props);
		}
	}
#ifdef SN_DEBUG_TRACE
	else
	{
		RECT rect = GetVisualDeviceTransform()->ToBBox(box_area);
		trace.TraceRect(rect, 0, OpSnAlgorithm::START_ELEMENT);
	}
#endif // SN_DEBUG_TRACE
}

BOOL
SnTraversalObject::TestBlock(LayoutProperties& layout_props)
{
	HitBoxResult result = HitBox(layout_props);
	if (result != NO_HIT)
	{
		HTML_Element* html_element = layout_props.html_element;
		Box* box = html_element->GetLayoutBox();
		RECT box_area;

		box_area.top = 0;
		box_area.left = 0;
		box_area.right = box->GetWidth();
		box_area.bottom = box->GetHeight();
		
		if (result == HIT_ELEMENT)
		{
			IntersectBox(html_element, box_area, FALSE, layout_props.GetProps());
		}
		else	// CHANGE_ELEMENT
		{
			RECT rect = GetVisualDeviceTransform()->ToBBox(box_area);

			SetCurrentElement(html_element, rect, layout_props.GetProps());
			algorithm->ForceBestRectValue(rect);
		}
	}

	return FALSE;
}

BOOL
SnTraversalObject::TestTableRow(HTML_Element* elm, const TableRowBox* row)
{
	HitBoxResult result = HitEventHandlerElement(elm); // Tables rows does not have LayoutProperies so we have to use this instead of HitBox.
	if (result != NO_HIT)
	{
		RECT row_rect;

		row_rect.top = 0;
		row_rect.bottom = row->GetHeight();
		row_rect.left = 0;
		// Have to get width from Table, since individual rows don't store width
		row_rect.right = row->GetTable()->GetWidth();

		if (result == HIT_ELEMENT)
		{
			IntersectBox(elm, row_rect);
		}
		else	// CHANGE_ELEMENT
		{
			row_rect = GetVisualDeviceTransform()->ToBBox(row_rect);
			SetCurrentElement(elm, row_rect);
			algorithm->ForceBestRectValue(row_rect);
		}
	}

	return FALSE;
}

BOOL
SnTraversalObject::TestInline(LayoutProperties& layout_props, const RECT& box_area)
{
	HitBoxResult result = HitBox(layout_props);
	if (result != NO_HIT)
	{
		HTML_Element* html_element = layout_props.html_element;
		
		if (result == HIT_ELEMENT)
		{
			IntersectBox(html_element, box_area, FALSE, layout_props.GetProps());
		}
		else	// CHANGE_ELEMENT
		{
			RECT rect = GetVisualDeviceTransform()->ToBBox(box_area);
			SetCurrentElement(html_element, rect, layout_props.GetProps());
			algorithm->ForceBestRectValue(rect);
		}
	}

	return FALSE;
}

BOOL
SnTraversalObject::EnterVerticalBox(LayoutProperties* parent_lprops, LayoutProperties*& layout_props, VerticalBox* box, TraverseInfo& traverse_info)
{
	if (navDirElement)
		return FALSE;

	if (!VisualTraversalObject::EnterVerticalBox(parent_lprops, layout_props, box, traverse_info))
	{
		if (!target_element)
			/* It is possible that we are now in the previous navigation element.
			   We need to check it. Then we need to check whether that element has
			   a corresponding nav dir property set. */
			TestNavDirLink(parent_lprops, box->GetHtmlElement());

		return FALSE;
	}

	if (!target_element)
	{
		if (exclusions && exclusions->Find(layout_props->html_element) != -1)
			return FALSE;

		if (layout_props->GetProps()->visibility != CSS_VALUE_visible)
			return TRUE;
		
		if (!lastScrollable || inCurrentScrollable)
			TestBlock(*layout_props);
	}

	return TRUE;
}

#ifdef SVG_SUPPORT
void SnTraversalObject::TestSVG(LayoutProperties* layout_props, const RECT& box_area)
{
	OpRect rect;

	// Check for preferred next element
	HTML_Element* next_elm = NULL;
	if ((lastElm && lastElm->GetNsType() == NS_SVG) &&
		g_svg_manager->HasNavTargetInDirection(lastElm, direction, fourway ? 4 : 2,
											   next_elm) &&
		next_elm)
	{
		// next_elm is where we are going
		g_svg_manager->GetNavigationData(next_elm, rect);

		RECT r;
		r.left = rect.x;
		r.top = rect.y;
		r.right = rect.x + rect.width;
		r.bottom = rect.y + rect.height;

		if (next_elm != lastElm)
		{
			RECT doc_rect = GetVisualDeviceTransform()->ToBBox(r);

			SetCurrentElement(next_elm, doc_rect);
			// FIXME: Need to force a new (and even 'best') rank value
			ResizeSearchArea();
		}
	}
	else
	{
		AffinePos doc_ctm = GetVisualDeviceTransform()->GetCTM();

		// Calculate search area within box
		OpRect search_area(area.left, area.top,
						   area.right - area.left,
						   area.bottom - area.top);
		search_area.OffsetBy(visual_device->GetRenderingViewport().TopLeft());
		doc_ctm.ApplyInverse(search_area);

		OpRect barea(box_area.left, box_area.top,
					 box_area.right - box_area.left,
					 box_area.bottom - box_area.top);
		search_area.IntersectWith(barea);

		if (search_area.IsEmpty())
			return;

		// Get "navigator"
		SVGTreeIterator* navigator = NULL;
		OP_STATUS status = g_svg_manager->GetNavigationIterator(layout_props->html_element,
																&search_area,
																layout_props, &navigator);
		RETURN_VOID_IF_ERROR(status);

		HTML_Element* nav_next = navigator->Next();
		while (nav_next)
		{
			if (OpStatus::IsSuccess(g_svg_manager->GetNavigationData(nav_next, rect)))
			{
				RECT r;
				r.left = rect.x;
				r.top = rect.y;
				r.right = rect.x + rect.width;
				r.bottom = rect.y + rect.height;
			
				IntersectBox(nav_next, r);
			}

			nav_next = navigator->Next();
		}
		g_svg_manager->ReleaseIterator(navigator);
	}
}
#endif // SVG_SUPPORT

BOOL
SnTraversalObject::EnterTableRow(TableRowBox* row)
{
	if (!VisualTraversalObject::EnterTableRow(row))
		return FALSE;

	if (navDirElement)
		return FALSE;

	TestTableRow(row->GetHtmlElement(), row);

	return TRUE;
}

BOOL
SnTraversalObject::EnterInlineBox(LayoutProperties* layout_props, InlineBox* box, const RECT& box_area, LineSegment& segment, BOOL start_of_box, BOOL end_of_box, LayoutCoord baseline, TraverseInfo& traverse_info)
{
	OP_NEW_DBG("SnTraversalObject::EnterInlineBox", "spatnav");

	if (navDirElement)
		return FALSE;

	if (!VisualTraversalObject::EnterInlineBox(layout_props, box, box_area, segment, start_of_box, end_of_box, baseline, traverse_info))
		return FALSE;

	// AreaTraversalObject ensures that
	OP_ASSERT(!target_element || layout_props->html_element->IsAncestorOf(target_element));

	if (target_element)
		return TRUE;

	if (exclusions && exclusions->Find(layout_props->html_element) != -1)
		return FALSE;

	OP_ASSERT(!document->IsInlineFrame() || iframe_rect_ptr);

	if (!SnUtil::IsPartiallyInView(GetVisualDeviceTransform()->ToBBox(box_area), document, iframe_rect_ptr))
	{
		/* If this inline box is not even partially in view, we don't need to test it.
		   Also we don't need to enter it at all if one of the below is TRUE:

		   - the element is not an inline block and doesn't have a non empty local
		   stacking context
		   - the element was a previous navigation element that has a nav dir
		   property in the corresponding direction. */

		if (TestNavDirLink(layout_props, NULL))
			return FALSE;

		return box->IsInlineBlockBox() || (box->GetLocalStackingContext() && !box->GetLocalStackingContext()->IsEmpty());
	}

	if (layout_props->GetProps()->visibility != CSS_VALUE_visible)
		return TRUE;

	if (!lastScrollable || inCurrentScrollable)
	{
#ifdef SVG_SUPPORT
		if (layout_props->html_element->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		{
			TestSVG(layout_props, box_area);
		}
		else
#endif // SVG_SUPPORT
			TestInline(*layout_props, box_area);
	}

	return TRUE;
}

BOOL
SnTraversalObject::EnterScrollable(const HTMLayoutProperties& props, ScrollableArea* scrollable, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (!VisualTraversalObject::EnterScrollable(props, scrollable, width, height, traverse_info))
		return FALSE;
	else
	{
		HTML_Element* helm = scrollable->GetOwningBox()->GetHtmlElement();

		if (target_element && exclusions && exclusions->Find(helm) != -1)
			return FALSE;

		if (
#ifdef SN_SCROLL_OVERFLOW_HIDDEN
			width < scrollable->GetScrollPaddingBoxWidth() ||
			height < scrollable->GetScrollPaddingBoxHeight()
#else
			// Additional requirement - we must have a scrollbar where the content overflows.
			width < scrollable->GetScrollPaddingBoxWidth() && scrollable->HasHorizontalScrollbar() ||
			height < scrollable->GetScrollPaddingBoxHeight() && scrollable->HasVerticalScrollbar()
#endif // SN_SCROLL_OVERFLOW_HIDDEN
			)
		{
			if (lastScrollable)
				if (helm->IsAncestorOf(lastScrollable))
				{
					if (helm == lastScrollable)
						inCurrentScrollable = TRUE;

					return TRUE;
				}
				else
					if (!lastScrollable->IsAncestorOf(helm))
						return FALSE;

			RECT box_area;

			box_area.top = props.border.top.width;
			box_area.left = props.border.left.width +
				(scrollable->LeftHandScrollbar() ? scrollable->GetVerticalScrollbarWidth() : 0);
			box_area.right = width;
			box_area.bottom = height;
			IntersectBox(helm, box_area, TRUE);

			return FALSE;
		}
		else
			return TRUE;
	}
}

void
SnTraversalObject::LeaveScrollable(ScrollableArea* scrollable, TraverseInfo& traverse_info)
{
	HTML_Element* helm = scrollable->GetOwningBox()->GetHtmlElement();
	if (lastScrollable && helm == lastScrollable)
		inCurrentScrollable = FALSE;
}

#ifdef PAGED_MEDIA_SUPPORT

BOOL
SnTraversalObject::EnterPagedContainer(LayoutProperties* layout_props, PagedContainer* content, LayoutCoord width, LayoutCoord height, TraverseInfo& traverse_info)
{
	if (!VisualTraversalObject::EnterPagedContainer(layout_props, content, width, height, traverse_info))
		return FALSE;

	if (content->GetTotalPageCount() > 1)
	{
		HTML_Element* helm = layout_props->html_element;

		if (target_element && exclusions && exclusions->Find(helm) != -1)
			return FALSE;

		if (lastScrollable)
			if (helm->IsAncestorOf(lastScrollable))
			{
				if (helm == lastScrollable)
					inCurrentScrollable = TRUE;

				return TRUE;
			}
			else
				if (!lastScrollable->IsAncestorOf(helm))
					return FALSE;

		const HTMLayoutProperties& props = *layout_props->GetProps();
		RECT box_area;

		box_area.top = props.border.top.width;
		box_area.left = props.border.left.width;
		box_area.right = width;
		box_area.bottom = height;
		IntersectBox(helm, box_area, TRUE);

		return FALSE;
	}
	else
		return TRUE;
}

void
SnTraversalObject::LeavePagedContainer(LayoutProperties* layout_props, PagedContainer* content, TraverseInfo& traverse_info)
{
	VisualTraversalObject::LeavePagedContainer(layout_props, content, traverse_info);

	if (lastScrollable && lastScrollable == content->GetHtmlElement())
		inCurrentScrollable = FALSE;
}

BOOL
SnTraversalObject::EnterPane(LayoutProperties* multipane_lprops, Column* pane, BOOL is_column, TraverseInfo& traverse_info)
{
	return AreaTraversalObject::EnterPane(multipane_lprops, pane, is_column, traverse_info);
}

void
SnTraversalObject::LeavePane(TraverseInfo& traverse_info)
{
	AreaTraversalObject::LeavePane(traverse_info);
}

#endif // PAGED_MEDIA_SUPPORT

BOOL
SnTraversalObject::HasEventHandler(HTML_Element* hElm, HTML_Element** html_elm)
{
	HTML_Element* handling_elm = NULL;
	BOOL has_handler = hElm->HasEventHandler(document, ONCLICK, NULL, &handling_elm) ||
	                   hElm->HasEventHandler(document, ONMOUSEOVER, NULL, &handling_elm) ||
	                   hElm->HasEventHandler(document, ONMOUSEENTER, NULL, &handling_elm);

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

#endif // _SPAT_NAV_SUPPORT_
