/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGElementStateContext.h"

#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/svgpaintnode.h"

#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationtarget.h"

#include "modules/svg/src/svgfontdata.h"
#include "modules/svg/src/SVGFontImpl.h"

#include "modules/layout/cascade.h"
#include "modules/layout/box/box.h"

/* static */  SVGElementContext*
SVGElementContext::Create(HTML_Element* svg_elm)
{
	OP_ASSERT(svg_elm->IsText() || svg_elm->GetNsType() == NS_SVG);
	OP_ASSERT(AttrValueStore::GetSVGElementContext(svg_elm) == NULL);
	OP_ASSERT(!svg_elm->GetSVGContext());
	HTML_Element* real_elm = SVGUtils::GetElementToLayout(svg_elm);
	OP_ASSERT(real_elm->GetNsType() == NS_SVG || real_elm->IsText());

	SVGElementContext* ctx;

	if (SVGUtils::IsAnimationElement(real_elm))
	{
		ctx = OP_NEW(SVGAnimationElement, (real_elm));
	}
	else if (SVGUtils::IsTimedElement(real_elm) || real_elm->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG))
	{
		switch (real_elm->Type())
		{
		case Markup::SVGE_VIDEO:
		case Markup::SVGE_ANIMATION:
			ctx = OP_NEW(SVGTimedExternalContentElement, (svg_elm));
			break;

		default:
			ctx = OP_NEW(SVGInvisibleTimedElement, (svg_elm));
		}
	}
	else
	{
		switch (real_elm->Type())
		{
			// Text-related elements
		case Markup::SVGE_TSPAN:
		case Markup::SVGE_TEXTPATH:
			ctx = OP_NEW(SVGTextContainer, (svg_elm));
			break;
		case Markup::SVGE_TREF:
			ctx = OP_NEW(SVGTRefElement, (svg_elm));
			break;
		case Markup::SVGE_ALTGLYPH:
			ctx = OP_NEW(SVGAltGlyphElement, (svg_elm));
			break;
		case Markup::SVGE_TBREAK:
			ctx = OP_NEW(SVGTBreakElement, (svg_elm));
			break;
		case Markup::SVGE_TEXT:
			ctx = OP_NEW(SVGTextElement, (svg_elm));
			break;
		case Markup::SVGE_TEXTAREA:
			ctx = OP_NEW(SVGTextAreaElement, (svg_elm));
			break;
		case Markup::HTE_TEXT:
			ctx = OP_NEW(SVGTextNode, (svg_elm));
			break;
		case Markup::HTE_TEXTGROUP:
			ctx = OP_NEW(SVGTextGroupElement, (svg_elm));
			break;
			
			// Font-related elements
		case Markup::SVGE_FONT:
		case Markup::SVGE_FONT_FACE:
			ctx = OP_NEW(SVGFontElement, (svg_elm));
			break;

			// Basic shapes
		case Markup::SVGE_CIRCLE:
		case Markup::SVGE_ELLIPSE:
		case Markup::SVGE_LINE:
		case Markup::SVGE_PATH:
		case Markup::SVGE_POLYGON:
		case Markup::SVGE_POLYLINE:
		case Markup::SVGE_RECT:
			ctx = OP_NEW(SVGGraphicsElement, (svg_elm));
			break;

			// 'External' content
		case Markup::SVGE_IMAGE:
		case Markup::SVGE_FOREIGNOBJECT:
			ctx = OP_NEW(SVGExternalContentElement, (svg_elm));
			break;			

			// Container elements
		case Markup::SVGE_G:
			ctx = OP_NEW(SVGContainer, (svg_elm));
			break;
		case Markup::SVGE_USE:
			ctx = OP_NEW(SVGUseElement, (svg_elm));
			break;

			// Resource containers
		case Markup::SVGE_MARKER:
		case Markup::SVGE_PATTERN:
			ctx = OP_NEW(SVGResourceContainer, (svg_elm));
			break;
#ifdef SVG_SUPPORT_STENCIL
		case Markup::SVGE_CLIPPATH:
			ctx = OP_NEW(SVGClipPathElement, (svg_elm));
			break;
		case Markup::SVGE_MASK:
			ctx = OP_NEW(SVGMaskElement, (svg_elm));
			break;
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
		case Markup::SVGE_FILTER:
			ctx = OP_NEW(SVGFilterElement, (svg_elm));
			break;

			// Filter primitives
		case Markup::SVGE_FETILE:
		case Markup::SVGE_FEBLEND:
		case Markup::SVGE_FEFLOOD:
		case Markup::SVGE_FEIMAGE:
		case Markup::SVGE_FEMERGE:
		case Markup::SVGE_FEOFFSET:
		case Markup::SVGE_FECOMPOSITE:
		case Markup::SVGE_FEMORPHOLOGY:
		case Markup::SVGE_FETURBULENCE:
		case Markup::SVGE_FECOLORMATRIX:
		case Markup::SVGE_FEGAUSSIANBLUR:
		case Markup::SVGE_FECONVOLVEMATRIX:
		case Markup::SVGE_FEDIFFUSELIGHTING:
		case Markup::SVGE_FEDISPLACEMENTMAP:
		case Markup::SVGE_FESPECULARLIGHTING:
		case Markup::SVGE_FECOMPONENTTRANSFER:
			ctx = OP_NEW(SVGFEPrimitive, (svg_elm));
			break;
#endif // SVG_SUPPORT_FILTERS

			// Transparent containers
		case Markup::SVGE_A:
			ctx = OP_NEW(SVGAElement, (svg_elm));
			break;
		case Markup::SVGE_SWITCH:
			ctx = OP_NEW(SVGSwitchElement, (svg_elm));
			break;

		case Markup::SVGE_SVG:
			// Shadow nodes to SVG elements should have
			// ElementStateContexts, the real node should have a document
			// context which should have been created in
			// HTML_Element::Construct
			OP_ASSERT(svg_elm->GetInserted() == HE_INSERTED_BY_SVG);

			ctx = OP_NEW(SVGSVGElement, (svg_elm));
			break;

		case Markup::SVGE_SYMBOL:
			if (real_elm != svg_elm)
			{
				ctx = OP_NEW(SVGSymbolInstanceElement, (svg_elm));
				break;
			}
			/* fall-through, invisible if not an instance */

		default:
			ctx = OP_NEW(SVGInvisibleElement, (svg_elm));
		}
	}

	if (ctx)
	{
		svg_elm->SetSVGContext(ctx);
	}

	return ctx;
}

SVGElementContext::SVGElementContext(HTML_Element* element) :
		m_animation_target(NULL),
		m_paint_node(NULL),
		packed_init(0)
{
	m_element_data = reinterpret_cast<UINTPTR>(element);
	m_element_data |= element && SVGUtils::IsShadowNode(element) ? 1 : 0;

	packed.m_invalidation_state = INVALID_ADDED; // Assume the worst
}

SVGElementContext::~SVGElementContext()
{
	if (g_svg_manager_impl)
		DetachBuffer();

	OP_DELETE(m_animation_target);
	OP_DELETE(m_paint_node);
	Out();
}

OP_STATUS
SVGElementContext::RegisterListener(HTML_Element *element, SVGTimeObject* etv)
{
	if (AssertAnimationTarget(element) == NULL)
		return OpStatus::ERR_NO_MEMORY;

    return m_animation_target->RegisterListener(etv);
}

OP_STATUS
SVGElementContext::UnregisterListener(SVGTimeObject* etv)
{
	return (m_animation_target) ?
		m_animation_target->UnregisterListener(etv) :
		OpStatus::OK;
}

void
SVGElementContext::AddInvalidState(SVGElementInvalidState state)
{
	UpgradeInvalidState(state);

	SetBBoxIsValid(FALSE);
	if (state > INVALID_SUBTREE)
	{
		// FIXME: Distinguish better when this is actually necessary
		packed.need_css_reload = TRUE;
	}
}

void SVGContainer::AddInvalidState(SVGElementInvalidState state)
{
	SVGElementContext::AddInvalidState(state);
	
	if (state > INVALID_STRUCTURE)
		m_children.RemoveAll();
}

/* Return TRUE if 'element' is <svg> , and has a non-SVG parent. */
static BOOL IsLocalSVGRoot(HTML_Element* element)
{
	return
		element->IsMatchingType(Markup::SVGE_SVG, NS_SVG) &&
		element->Parent() && element->Parent()->GetNsType() != NS_SVG;
}

/* static */
SVGElementInvalidState SVGElementContext::ComputeInvalidState(HTML_Element* elm)
{
	SVGElementInvalidState state = NOT_INVALID;
	HTML_Element* iter = elm;
	NS_Type prev_ns = NS_SVG;
	BOOL crossed_namespace_boundary = FALSE;

	// Consider stopping on reaching a ns-boundary.
	while (iter && state < INVALID_ADDED)
	{
		SVGElementContext* ctx = AttrValueStore::GetSVGElementContext(iter);
		if (ctx)
		{
			// Have we gone SVG -> non-SVG -> SVG?
			if (crossed_namespace_boundary)
				return INVALID_ADDED;

			// If the element is disconnected from the rendering tree
			// (generally because display == 'none'), then set state to
			// INVALID_ADDED, since then callers of this method can't rely
			// on the state of the element. The InList() test will generally
			// only detect the transition from a detached subtree to a
			// (potentially) non-detached, but that should suffice for the
			// current users of this method. Because the topmost root of a
			// fragment will not have a parent in the rendering tree we need
			// a to do a different check for those.
			if (ctx->InList() || IsLocalSVGRoot(iter))
				state = MAX(state, ctx->GetInvalidState());
			else
				return INVALID_ADDED;

			prev_ns = NS_SVG;
		}
		else if (iter->Type() == HE_DOC_ROOT)
			break;
		else
		{
			NS_Type current_ns = iter->GetNsType();
			if (prev_ns != current_ns)
			{
				// Not the first crossing.
				if (crossed_namespace_boundary)
					return INVALID_ADDED;

				crossed_namespace_boundary = TRUE;
				prev_ns = current_ns;
			}
			else if (current_ns == NS_SVG)
			{
				// SVG node without a context.
				return INVALID_ADDED;
			}
			// else still the same namespace - no action required.
		}
		iter = iter->Parent();
	}
	return state;
}

BOOL
SVGElementContext::ListensToEvent(DOM_EventType type)
{
	if (!m_animation_target)
		return FALSE;

	return m_animation_target->ListensToEvent(type);
}

BOOL
SVGElementContext::HandleEvent(HTML_Element* elm, const SVGManager::EventData& data)
{
	if (!m_animation_target)
		return FALSE;

	return m_animation_target->HandleEvent(elm, data);
}

// The attribute info is packed into a 32-bits integer. The 16
// lowest (least-significant) bits is used for the attribute
// name. It is stored as shorts in logdoc so we should be fine.
// The ns_idx is the 8 next bits (shifted 16 bits up). It is
// stored with 8 bits in logdoc so we should be fine here also. We
// can increase this if needed in the future.  The attribute type
// (SVGAttributeType) is stored at bit 24 and is the rest of the
// integer. It has four values and should only need two bits.

/* static */ INT32
SVGElementContext::PackAttributeInfo(Markup::AttrType attribute_name, int ns_idx, SVGAttributeType attribute_type)
{
	OP_ASSERT((attribute_name & 0xffff) == attribute_name);
	OP_ASSERT((ns_idx & 0xff) == ns_idx);
	OP_ASSERT((attribute_type & 0xf) == attribute_type);
	return attribute_name & 0xffff |
		(ns_idx & 0xff) << 16 |
		(attribute_type & 0xf) << 24;
}

/* static */ void
SVGElementContext::UnpackAttributeInfo(INT32 packed, Markup::AttrType &attribute_name, int &ns_idx, SVGAttributeType &attribute_type)
{
	attribute_name = static_cast<Markup::AttrType>(packed & 0xffff);
	ns_idx = (packed >> 16) & 0xff;
	attribute_type = (SVGAttributeType)((packed >> 24) & 0xf);
}

BOOL
SVGElementContext::ShouldBuffer(const SvgProperties* svg_props)
{
	if (!(packed.should_buffer & 0x01))
	{
		SVGBufferedRendering bufren = svg_props->GetBufferedRendering();
		packed.should_buffer = bufren == SVGBUFFEREDRENDERING_STATIC ? 1 : 3;
	}

	return packed.should_buffer == 1;
}

void SVGElementContext::DetachBuffer()
{
	if (!m_paint_node)
		return;

	if (m_paint_node->IsBuffered())
	{
		SVGCache* scache = g_svg_manager_impl->GetCache();
		scache->Remove(SVGCache::SURFACE, this);
	}

	m_paint_node->ResetBuffering();

	RevalidateBuffering();
}

SVGAnimationTarget *
SVGElementContext::AssertAnimationTarget(HTML_Element *target_element)
{
	return m_animation_target ? m_animation_target :
		(m_animation_target = OP_NEW(SVGAnimationTarget, (target_element)));
}

BOOL SVGElementContext::HasAttachedPaintNode() const
{
	return m_paint_node && m_paint_node->InList();
}

OpRect SVGElementContext::GetScreenExtents() const
{
	OpRect scr_extents;
	if (m_paint_node)
		scr_extents = m_paint_node->GetPixelExtents();

	return scr_extents;
}

SVGFontElement::~SVGFontElement()
{
	if (m_fontdata)
		m_fontdata->DetachFontElement();

	SVGFontData::DecRef(m_fontdata);

	m_font_instances.RemoveAll();
}

SVGFontImpl* SVGFontElement::CreateFontInstance(UINT32 size)
{
	RETURN_VALUE_IF_ERROR(CreateFontData(), NULL);

	SVGFontImpl* font_instance = OP_NEW(SVGFontImpl, (m_fontdata, size));
	if (!font_instance)
		return NULL;

	font_instance->Into(&m_font_instances);
	return font_instance;
}

void SVGFontElement::NotifyFontDataChange()
{
	OP_ASSERT(m_fontdata);

	Link* l = m_font_instances.First();
	while (l)
	{
		Link* next = l->Suc();
		SVGFontImpl* svg_font = static_cast<SVGFontImpl*>(l);

		// Flush potentially cached glyphs from the old font
		if (g_svg_manager_impl)
			g_svg_manager_impl->GetGlyphCache().HandleFontDeletion(svg_font);

		svg_font->UpdateFontData(m_fontdata);

		l = next;
	}
}

OP_STATUS SVGFontElement::MarkFontDataDirty()
{
	SVGFontData* old_fontdata = m_fontdata;
	m_fontdata = NULL;

	RETURN_IF_ERROR(CreateFontData());

	NotifyFontDataChange();

	SVGFontData::DecRef(old_fontdata);
	return OpStatus::OK;
}

OP_STATUS SVGFontElement::CreateFontData()
{
	if (!m_fontdata)
	{
		m_fontdata = OP_NEW(SVGXMLFontData, ());
		if (!m_fontdata || OpStatus::IsError(m_fontdata->Construct(GetHtmlElement())))
		{
			OP_DELETE(m_fontdata);
			m_fontdata = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		SVGFontData::IncRef(m_fontdata);
	}
	return OpStatus::OK;
}

#endif // SVG_SUPPORT
