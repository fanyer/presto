/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

// #ifdef SVG_SUPPORT_NAVIGATION

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGNavigation.h"

#include "modules/layout/cascade.h"

/* static */
BOOL SVGNavigation::NavRefToElement(HTML_Element* elm, Markup::AttrType attr,
									HTML_Element*& preferred_next_elm)
{
	if (!AttrValueStore::HasObject(elm, attr, NS_IDX_SVG))
		return FALSE;

	SVGNavRef* navref = NULL;
	AttrValueStore::GetNavRefObject(elm, attr, navref);
	if (navref)
	{
		HTML_Element* next_elm = NULL;
		SVGNavRef::NavRefType nrt = navref->GetNavType();
		if (nrt == SVGNavRef::SELF)
		{
			next_elm = elm; // Same as current
		}
		else if (nrt == SVGNavRef::URI)
		{
			SVGURL nav_elm_url;
			if (OpStatus::IsSuccess(nav_elm_url.SetURL(navref->GetURI(), URL())))
			{
				if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm))
				{
					URL fullurl = nav_elm_url.GetURL(doc_ctx->GetURL(), elm);
					next_elm = (fullurl == doc_ctx->GetURL()) ?
						SVGUtils::FindElementById(doc_ctx->GetLogicalDocument(), fullurl.UniRelName()) :
						NULL;
				}
			}
		}
		// Fall-through for ::AUTO

		if (next_elm != NULL)
		{
			preferred_next_elm = next_elm;
			return TRUE;
		}
	}
	return FALSE;
}

/* static */
BOOL SVGNavigation::FindElementInDirection(HTML_Element* elm, int direction, int nway,
										   HTML_Element*& preferred_next_elm)
{
	OP_ASSERT(nway == 2 || nway == 4 || nway == 8); // These are what we can accommodate

	Markup::AttrType attr;
	if (nway == 2)
	{
		attr = (direction / 180) ? Markup::SVGA_NAV_NEXT : Markup::SVGA_NAV_PREV;
	}
	else
	{
		// Quantize angle
		int quant_dir = (((direction + (360/nway)/2) / (360/nway)) % nway) * (360/nway);
		attr = Markup::SVGA_NAV_DOWN;
		switch (quant_dir)
		{
		case 0:		attr = Markup::SVGA_NAV_RIGHT; break;
		case 45:	attr = Markup::SVGA_NAV_UP_RIGHT; break;
		case 90:	attr = Markup::SVGA_NAV_UP; break;
		case 135:	attr = Markup::SVGA_NAV_UP_LEFT; break;
		case 180:	attr = Markup::SVGA_NAV_LEFT; break;
		case 225:	attr = Markup::SVGA_NAV_DOWN_LEFT; break;
		case 270:	attr = Markup::SVGA_NAV_DOWN; break;
		case 315:	attr = Markup::SVGA_NAV_DOWN_RIGHT; break;
		default:
			OP_ASSERT(!"Bad direction!");
			break;
		}
	}

	return NavRefToElement(elm, attr, preferred_next_elm);
}

OP_STATUS SVGAreaIterator::Init(HTML_Element* start_elm, const OpRect* search_area)
{
	m_doc_ctx = AttrValueStore::GetSVGDocumentContext(start_elm);
	if (!m_doc_ctx)
		return OpStatus::ERR;

	VisualDevice* vd = m_doc_ctx->GetVisualDevice();
	if (!vd)
		return OpStatus::ERR;

	m_current_elm = start_elm;

	m_next_found = m_prev_found = FALSE;

	m_search_area.Empty();
	if (search_area)
	{
		m_search_area = *search_area;

		m_search_area = vd->ScaleToScreen(m_search_area);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		m_search_area = TO_DEVICE_PIXEL(vd->GetVPScale(), m_search_area);
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	}

	return OpStatus::OK;
}

OP_STATUS SVGAreaIterator::TestVisible(HTML_Element* test_elm, HTML_Element* layouted_elm)
{
	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(test_elm);
	// Enough for 'switch' handling ?
	if (!elm_ctx)
		return OpSVGStatus::SKIP_ELEMENT /* SUBTREE ? */;

	if (elm_ctx->IsFilteredByRequirements())
		return OpSVGStatus::SKIP_SUBTREE;

	const OpRect screen_extents = elm_ctx->GetScreenExtents();
	if (!m_search_area.IsEmpty() && !screen_extents.Intersecting(m_search_area))
		// Not in search area
		return OpSVGStatus::SKIP_SUBTREE;

	AutoDeleteHead prop_list;
	HLDocProfile* hld_profile = m_doc_ctx->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	LayoutProperties* elm_props = LayoutProperties::CreateCascade(layouted_elm,
		prop_list, LAYOUT_WORKPLACE(hld_profile));
	if (!elm_props)
		return OpStatus::ERR_NO_MEMORY;

	if (SVGUtils::IsDisplayNone(layouted_elm, elm_props))
		return OpSVGStatus::SKIP_SUBTREE;

	return OpStatus::OK;
}

OP_STATUS SVGAreaIterator::TestRelevantForDisplay(HTML_Element* layouted_elm)
{
	if (layouted_elm->GetNsType() != NS_SVG)
		return OpSVGStatus::SKIP_SUBTREE;

	Markup::Type type = layouted_elm->Type();
	if (SVGUtils::IsAlwaysIrrelevantForDisplay(type))
		return OpSVGStatus::SKIP_SUBTREE;

	return OpStatus::OK;
}

OP_STATUS SVGAreaIterator::TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm)
{
	if (layouted_elm->IsText())
		return OpSVGStatus::SKIP_ELEMENT;

	RETURN_IF_ERROR(TestRelevantForDisplay(layouted_elm));
	return TestVisible(test_elm, layouted_elm);
}

BOOL SVGAreaIterator::Step(BOOL forward)
{
	HTML_Element* next_elm = m_current_elm;
	
	while (next_elm)
	{
		HTML_Element* layouted_elm = SVGUtils::GetElementToLayout(next_elm);
		OP_STATUS status = TestElement(next_elm, layouted_elm);

		switch (status)
		{
		default: // In case of error
		case OpSVGStatus::SKIP_ELEMENT:
			next_elm = forward ? next_elm->Next() : next_elm->Prev();
			break;
		case OpSVGStatus::SKIP_SUBTREE:
			next_elm = forward ?
				static_cast<HTML_Element*>(next_elm->NextSibling()) :
				static_cast<HTML_Element*>(next_elm->PrevSibling());
			break;
		case OpStatus::OK:
			// Yay! Found one
			m_current_elm = next_elm;
			return TRUE;
		}
	}

	// Nothing found
	m_current_elm = next_elm;
	return FALSE;
}

HTML_Element* SVGAreaIterator::Next()
{
	if (!m_current_elm && !m_prev_found)
		m_current_elm = m_doc_ctx->GetSVGRoot();
	else if (m_current_elm)
		m_current_elm = m_current_elm->Next();

	m_next_found = Step(TRUE /* forward */);

	return m_next_found ? m_current_elm : NULL;
}

HTML_Element* SVGAreaIterator::Prev()
{
	if (!m_current_elm && !m_next_found)
		m_current_elm = m_doc_ctx->GetSVGRoot()->LastChild();
	else if (m_current_elm)
		m_current_elm = m_current_elm->Prev();

	m_prev_found = Step(FALSE /* backward */);

	return m_prev_found ? m_current_elm : NULL;
}

OP_STATUS SVGFocusIterator::TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm)
{
	if (layouted_elm->IsText())
		return OpSVGStatus::SKIP_ELEMENT;

	RETURN_IF_ERROR(TestRelevantForDisplay(layouted_elm));

	if (!g_svg_manager_impl->IsFocusableElement(m_doc_ctx->GetDocument(), layouted_elm))
		return OpSVGStatus::SKIP_ELEMENT;

	return TestVisible(test_elm, layouted_elm);
}

#ifdef SEARCH_MATCHES_ALL
OP_STATUS SVGHighlightUpdateIterator::Init(HTML_Element* start_elm, SelectionElm* first_hit)
{
	m_doc_ctx = AttrValueStore::GetSVGDocumentContext(start_elm);
	if (!m_doc_ctx)
		return OpStatus::ERR;

	m_current_elm = start_elm;

	m_next_found = m_prev_found = FALSE;

	m_current_hit = first_hit;

	return OpStatus::OK;
}

OP_STATUS SVGHighlightUpdateIterator::TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm)
{
	if(!m_doc_ctx->GetSVGImage()->IsInteractive())
		return OpSVGStatus::SKIP_SUBTREE;

	if(!m_current_hit)
		return OpSVGStatus::SKIP_ELEMENT;

	if (layouted_elm->IsText())
	{
		if(layouted_elm == m_current_hit->GetSelection()->GetStartElement())
		{
			m_current_hit = (SelectionElm*)m_current_hit->Suc();
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(TestRelevantForDisplay(layouted_elm));

	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(test_elm);
	if (!elm_ctx)
		return OpSVGStatus::SKIP_ELEMENT /* SUBTREE ? */;

	if (!SVGUtils::IsTextClassType(layouted_elm->Type()) &&
		!SVGUtils::IsContainerElement(test_elm))
		return OpSVGStatus::SKIP_SUBTREE;

	RETURN_IF_ERROR(TestVisible(test_elm, layouted_elm));

	return OpSVGStatus::SKIP_ELEMENT;
}
#endif // SEARCH_MATCHES_ALL

#ifdef RESERVED_REGIONS
OP_STATUS SVGReservedRegionIterator::TestElement(HTML_Element* test_elm, HTML_Element* layouted_elm)
{
	if (layouted_elm->IsText())
		return OpSVGStatus::SKIP_ELEMENT;

	RETURN_IF_ERROR(TestRelevantForDisplay(layouted_elm));

	HLDocProfile* hld_profile = m_doc_ctx->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	BOOL has_reserved_handler = FALSE;
	for (int i = 0; g_reserved_region_types[i] != DOM_EVENT_NONE && !has_reserved_handler; i++)
		if (layouted_elm->HasEventHandler(hld_profile->GetFramesDocument(), g_reserved_region_types[i], FALSE))
			has_reserved_handler = TRUE;

	if (!has_reserved_handler)
		return OpSVGStatus::SKIP_ELEMENT;

	return TestVisible(test_elm, layouted_elm);
}
#endif // RESERVED_REGIONS

// #endif // SVG_SUPPORT_NAVIGATION

#endif // SVG_SUPPORT
