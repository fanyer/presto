/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGDependencyGraph.h"
#include "modules/svg/src/SVGWorkplaceImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/svgpaintnode.h"
#ifdef SVG_SUPPORT_EDITABLE
#include "modules/svg/src/SVGEditable.h"
#endif // SVG_SUPPORT_EDITABLE

#include "modules/svg/src/animation/svganimationworkplace.h"

#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/box/box.h" // For LayoutInfo
#include "modules/style/css_property_list.h" // For font-family reparsing
#include "modules/style/css_style_attribute.h"

#include "modules/dochand/fdelm.h"

/* static */ OP_STATUS
SVGDynamicChangeHandler::HandleFontsChanged(SVGDocumentContext* doc_ctx)
{
	HTML_Element* svg_root = doc_ctx->GetSVGRoot();
	HTML_Element* elm = svg_root;
	HTML_Element* stop = static_cast<HTML_Element*>(elm->NextSibling());

	while (elm != stop)
	{
		// For all elements, reparse style for inline style with font
		// numbers in them
		if (CssPropertyItem::GetCssPropertyItem(elm, CSSPROPS_FONT_NUMBER))
		{
			StyleAttribute* style_attr =
				static_cast<StyleAttribute*>(elm->GetAttr(Markup::SVGA_STYLE,
														  ITEM_TYPE_COMPLEX,
														  (void*)0, NS_IDX_SVG));
			if (style_attr)
			{
				URL doc_url = doc_ctx->GetURL();
				URL base_url = SVGUtils::ResolveBaseURL(doc_url, elm);
				style_attr->ReparseStyleString(doc_ctx->GetHLDocProfile(), base_url);
				elm->MarkPropsDirty(doc_ctx->GetDocument());
			}
		}

		// Reparse style elements
		if (elm->IsMatchingType(Markup::SVGE_STYLE, NS_SVG))
		{
			HTML_Element::DocumentContext context(doc_ctx->GetDocument());
			RETURN_IF_ERROR(elm->LoadStyle(context, FALSE));

			SVGElementContext* root_ctx = AttrValueStore::GetSVGElementContext(svg_root);
			if (root_ctx && !root_ctx->IsCSSReloadNeeded())
				root_ctx->AddInvalidState(INVALID_ADDED);
		}

		/* Invalidate all text elements, since we don't know which
		   text elements really wanted to use an eventual new font
		   instead of the current font it is using. */
		if (SVGUtils::IsTextRootElement(elm))
		{
			// If the text-root is part of a font, don't flag a change since that
			// will cause infinite recursion between this function and MarkForRepaint.
			// Hopefully there should be no need to do this either - primarily because
			// we don't support full SVG fonts.
			BOOL has_font_parent = FALSE;
			HTML_Element* parent = elm->Parent();
			while (parent)
			{
				if (parent->IsMatchingType(Markup::SVGE_FONT, NS_SVG) ||
					parent->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
				{
					has_font_parent = TRUE;
					break;
				}

				parent = parent->Parent();
			}

			if (!has_font_parent)
				HandleAttributeChange(doc_ctx, elm, Markup::SVGA_FONT_FAMILY, NS_SVG, FALSE);
		}

		elm = elm->Next();
	}

	if(SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext(svg_root))
	{
	  elem_ctx->AddInvalidState(INVALID_FONT_METRICS);
	}

	return OpStatus::OK;
}

class RepaintTraverser : public SVGGraphTraverser
{
public:
	RepaintTraverser(SVGDependencyGraph* dgraph) :
		SVGGraphTraverser(dgraph, FALSE /* don't care about indirect dependencies */) {}

	virtual OP_STATUS Visit(HTML_Element* node);

	OpRegion* GetRegion() { return &m_region; }

	static OpRect GetRepaintArea(HTML_Element* node);

private:
	OpRegion m_region;
};

/* static */
OpRect RepaintTraverser::GetRepaintArea(HTML_Element* node)
{
	SVGElementContext *elm_ctx = AttrValueStore::GetSVGElementContext(node);
	return elm_ctx ? elm_ctx->GetScreenExtents() : OpRect();
}

OP_STATUS RepaintTraverser::Visit(HTML_Element* node)
{
	OpRect repaint_area = GetRepaintArea(node);
	if (!repaint_area.IsEmpty()) // This being empty would be an error in some cases
	{
		if (!m_region.IncludeRect(repaint_area))
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* static */
OP_STATUS SVGDynamicChangeHandler::RepaintElement(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	SVGRenderer* renderer = doc_ctx->GetRenderingState();
	SVGElementContext *elm_ctx = AttrValueStore::GetSVGElementContext(element);

	if (renderer && elm_ctx)
	{
		OpRect repaint_area;

		if (SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph())
		{
			RepaintTraverser rtrav(dgraph);

			rtrav.AddRootPath(element);
			rtrav.Visit(element);
			rtrav.Mark(element);

			rtrav.Traverse();

			OpRegion* region = rtrav.GetRegion();

			if (!region->IsEmpty())
			{
				OpRegionIterator iter = region->GetIterator();
				BOOL ok = iter.First();
				while (ok)
				{
					renderer->GetInvalidState()->Invalidate(iter.GetRect());
					repaint_area.UnionWith(iter.GetRect());

					ok = iter.Next();
				}
			}
		}
		else
		{
			// No dependencygraph - No dependencies
			repaint_area = RepaintTraverser::GetRepaintArea(element);
			if (!repaint_area.IsEmpty())
				renderer->GetInvalidState()->Invalidate(repaint_area);
		}

		if (!repaint_area.IsEmpty())
		{
			doc_ctx->GetSVGImage()->QueueInvalidate(repaint_area);
			return OpStatus::OK;
		}
	}
	// Fallback (and case when locked)
	HTML_Element* parent = element->Parent();
	MarkForRepaint(doc_ctx, element, parent, PAINT_DETAIL_CHANGED);
	return OpStatus::OK;
}

static BOOL ElementHasDependents(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	if (!dgraph)
		return FALSE;

	for (HTML_Element* parent = element; parent; parent = parent->Parent())
	{
		NodeSet* edgeset;
		OP_STATUS status = dgraph->GetDependencies(parent, &edgeset);

		if (OpStatus::IsSuccess(status) && !edgeset->IsEmpty())
			return TRUE;
	}
	return FALSE;
}

/* static */
OP_STATUS SVGDynamicChangeHandler::HandleElementChange(SVGDocumentContext* doc_ctx, HTML_Element* element, unsigned int changes)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	doc_ctx->GetSVGImage()->SuspendScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING

	HTML_Element* parent = element->Parent();

	/* Attempt at mapping the change flags to repaint reasons */

	RepaintReason reason = NO_REASON;

	if (changes & PROPS_CHANGED_SVG_REPAINT)
		reason = PAINT_DETAIL_CHANGED;

	if (changes & PROPS_CHANGED_SVG_RELAYOUT)
		reason = ATTRIBUTE_CHANGED;

	if (changes & PROPS_CHANGED_SVG_DISPLAY)
		reason = ELEMENT_STRUCTURE;

	if (changes & PROPS_CHANGED_SVG_AUDIO_LEVEL)
		if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
			animation_workplace->MarkPropsDirty(element);

	if (reason != NO_REASON)
	{
		MarkForRepaint(doc_ctx, element, parent, reason);

		if (SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph())
		{
			RETURN_IF_ERROR(MarkDependentNodesForRepaint(doc_ctx, element, NULL));
			dgraph->RemoveTargetNode(element);
		}
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS SVGDynamicChangeHandler::HandleAttributeChange(SVGDocumentContext* doc_ctx, HTML_Element* element, Markup::AttrType attr, NS_Type attr_ns, BOOL was_removed)
{
	OP_ASSERT(element && element->GetNsType() == NS_SVG);
	Markup::Type type = element->Type();

#ifdef SVG_INTERRUPTABLE_RENDERING
	doc_ctx->GetSVGImage()->SuspendScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING

	RepaintReason reason = ATTRIBUTE_CHANGED;

	// Smarter handling of attributes. Not finished
	if (attr_ns == NS_SVG)
	{
		// Changing the current view means that we have to recalculate the whole SVG
		if(type == Markup::SVGE_VIEW)
		{
			const uni_char* rel = doc_ctx->GetURL().UniRelName();
			if (rel && element == SVGUtils::FindDocumentRelNode(doc_ctx->GetLogicalDocument(), rel))
			{
				MarkWholeSVGForRepaint(doc_ctx);
				return OpStatus::OK;
			}
		}
		else if (SVGUtils::IsViewportElement(type))
		{
			switch (attr)
			{
			  case Markup::SVGA_WIDTH:
			  case Markup::SVGA_HEIGHT:
			  case Markup::SVGA_VIEWBOX:
				{
					/* Root elements have layout boxes that are
					   affected by width and height.

					   Since width and height are mapped to properties
					   (at least for inline SVG) we need their
					   properties reloaded.

					   viewBox may contribute to intrinsic ratio and
					   may influence the size as well. */

					if (type == Markup::SVGE_SVG &&
						element == doc_ctx->GetElement())
					{
						element->MarkPropsDirty(doc_ctx->GetDocument());

						if (FramesDocument* frm_doc = doc_ctx->GetDocument())
						{
							DocumentManager* doc_man = frm_doc->GetDocManager();
							if (FramesDocElm* frame = doc_man->GetFrame())
								frame->GetHtmlElement()->MarkDirty(doc_man->GetParentDoc(), TRUE, TRUE);
						}
					}

					if (SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext_Unsafe(element))
					{
						elem_ctx->AddInvalidState(INVALID_FONT_METRICS);
					}
				}
				break;
			  case Markup::SVGA_OPACITY:
				if (type == Markup::SVGE_SVG && element == doc_ctx->GetElement())
				{
					// May change the type of layout box required.
					element->MarkExtraDirty(LOGDOC_DOC(doc_ctx->GetDocument()));
				}
				break;
			}
		}

		if (SVGUtils::IsPresentationAttribute(attr, type))
		{
			if (attr == Markup::SVGA_COLOR || attr == Markup::SVGA_STOP_COLOR || attr == Markup::SVGA_SOLID_COLOR ||
				attr == Markup::SVGA_STOP_OPACITY)
			{
				if(doc_ctx->GetDependencyGraph())
				{
					MarkDependentNodesForRepaint(doc_ctx, NULL, element);
				}
			}
			else if (attr == Markup::SVGA_AUDIO_LEVEL)
			{
				if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
					animation_workplace->MarkPropsDirty(element);
			}
			else if (attr == Markup::SVGA_DISPLAY)
				reason = ELEMENT_STRUCTURE;

			if(SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext_Unsafe(element))
			{
				elem_ctx->SetHasTrustworthyPresAttrs(FALSE);

				if (attr == Markup::SVGA_BUFFERED_RENDERING)
				  elem_ctx->DetachBuffer();
				else if (SVGUtils::AttributeAffectsFontMetrics(attr, type))
				  elem_ctx->AddInvalidState(INVALID_FONT_METRICS);
			}
		}
		else if (attr == Markup::SVGA_CLASS)
		{
			// Notify potential collections that they have changed
			if (DOM_Environment* environment = doc_ctx->GetDOMEnvironment())
				environment->ElementCollectionStatusChanged(element, DOM_Environment::COLLECTION_CLASSNAME);
		}
		else if (SVGUtils::AttributeAffectsFontMetrics(attr, type))
		{
			if(SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext_Unsafe(element))
			{
			  elem_ctx->AddInvalidState(INVALID_FONT_METRICS);
			}
		}
		else if (attr == Markup::SVGA_TRANSFORM || attr == Markup::SVGA_ANIMATE_TRANSFORM || attr == Markup::SVGA_MOTION_TRANSFORM)
		{
			// The 'transform' changed, attempt to do an update via
			// the paint tree if we're not going to do a layout-pass
			// anyway.
			SVGElementInvalidState invalid_state = SVGElementContext::ComputeInvalidState(element);
			if (invalid_state < INVALID_ADDED && !ElementHasDependents(doc_ctx, element))
				if (UpdatePaintNodeTransform(doc_ctx, element))
					return OpStatus::OK;
		}
	}
	else if (attr_ns == NS_XLINK)
	{
		Markup::AttrType xlink_attr = attr;
		if (xlink_attr == Markup::XLINKA_HREF)
		{
			if (type == Markup::SVGE_USE)
			{
				// The shadow tree just changed.
				RemoveShadowTree(doc_ctx, element);
				// Also remove all other shadow trees that indirectly referred to this one
				RemoveAllShadowTreesReferringToElement(doc_ctx, element);

				// FIXME This might leak dependencies since we don't remove the
				// dependencies between this node and the node it used to refer to.
				// The problem is that we don't know which nodes we used to refer
				// to via the xlink:href attribute

				// Maybe we should consider doing a:
				//    dgraph->RemoveNode(element);
				// then?
			}
			else if (type == Markup::SVGE_IMAGE ||
					 type == Markup::SVGE_FEIMAGE ||
					 type == Markup::SVGE_FOREIGNOBJECT ||
					 type == Markup::SVGE_ANIMATION)
			{
				SVGUtils::LoadExternalReferences(doc_ctx, element);
			}

#ifdef SVG_TINY_12
			// Might be necessary, needs testing and perhaps a better way of updating the media player
			if (type == Markup::SVGE_VIDEO ||
				type == Markup::SVGE_AUDIO ||
				type == Markup::SVGE_ANIMATION)
			{
				if (SVGTimingInterface* timed_elm_ctx = AttrValueStore::GetSVGTimingInterface(element))
				{
					timed_elm_ctx->OnTimelineRestart();
				}
			}
#endif // SVG_TINY_12
		}
	}
	else
	{
		// FIXME: Handle change in xml-namespace here. We will
		// eventually get xml:base change notifications here.
		return OpStatus::OK;
	}

	HTML_Element* parent = element->Parent();
	MarkForRepaint(doc_ctx, element, parent, reason);

	// Update the dependency graph, and if we have a dependency graph,
	// use it to mark all related nodes.
	// If we don't have any dependency graph, then we have not yet
	// layouted and painted the SVG so no need to keep track of
	// depending nodes.
	if (SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph())
	{
		if (attr == Markup::SVGA_ID)
		{
			HandleRemovedId(doc_ctx, element);
			if (!was_removed)
			{
				// The removed id was replaced by a new one
				const uni_char* id = element->GetId();
				if (id)
				{
					HandleNewId(doc_ctx, element, id);
				}
			}
		}
		RETURN_IF_ERROR(MarkDependentNodesForRepaint(doc_ctx, element, NULL));

		// Dependencies are rebuilt lazily so we just remove existing ones here
		dgraph->RemoveTargetNode(element);

#ifdef SVG_DEBUG_DEPENDENCY_GRAPH
		SVGDocumentContext* top_doc_ctx = doc_ctx->GetParentDocumentContext() ?
			doc_ctx->GetParentDocumentContext() : doc_ctx;
		dgraph->CheckConsistency(top_doc_ctx->GetSVGRoot());
#endif // SVG_DEBUG_DEPENDENCY_GRAPH
	}
	return OpStatus::OK;

}

/* static */
void SVGDynamicChangeHandler::InvalidateDependents(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	OP_ASSERT(dgraph); // Don't call this unless there is a dependency graph

	NodeSet* edgeset;
	RETURN_VOID_IF_ERROR(dgraph->GetDependencies(element, &edgeset));

	OP_ASSERT(edgeset);

	NodeSetIterator ei = edgeset->GetIterator();

	for (OP_STATUS status = ei.First();
		 OpStatus::IsSuccess(status);
		 status = ei.Next())
	{
		// Depending node
		HTML_Element* dependent_node = ei.GetEdge();
		MarkForRepaint(doc_ctx, dependent_node, dependent_node->Parent(), ATTRIBUTE_CHANGED);
		MarkDependentNodesForRepaint(doc_ctx, dependent_node, NULL);
	}
}

OP_STATUS SVGDynamicChangeHandler::HandleRemovedId(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	InvalidateDependents(doc_ctx, element);

	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	dgraph->RemoveTargetNode(element);

	return OpStatus::OK;
}

OP_STATUS SVGDynamicChangeHandler::HandleNewId(SVGDocumentContext* doc_ctx, HTML_Element* element, const uni_char* id)
{
	// This element might have triggered a broken dependency becoming whole, or
	// something that earlier resolved to another node now resolving to this
	LogicalDocument* logdoc = doc_ctx->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::OK; // Without a logical document, who cares?

	NamedElementsIterator matches;
	logdoc->GetNamedElements(matches, id, TRUE, FALSE);

	HTML_Element* match = matches.GetNextElement();
	if (match == element)
	{
		match = matches.GetNextElement();

		if (match)
		{
			// 'match' now points to what used to be the first
			// matching element

			InvalidateDependents(doc_ctx, match);

			SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
			dgraph->RemoveTargetNode(match);
		}
	}

	return OpStatus::OK;
}

static void InvalidateParentBBox(HTML_Element* element)
{
	HTML_Element* parent = element->Parent();
	while (parent)
	{
		SVGElementContext* parent_context = AttrValueStore::GetSVGElementContext(parent);
		if (!parent_context)
			break;

		parent_context->SetBBoxIsValid(FALSE);

		parent = parent->Parent();
	}
}

BOOL SVGDynamicChangeHandler::UpdatePaintNodeTransform(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	OP_ASSERT(element->GetNsType() == NS_SVG);

	if (doc_ctx->IsExternalAnimation())
	{
		doc_ctx = doc_ctx->GetParentDocumentContext();
		if (!doc_ctx)
			return FALSE;

		return UpdatePaintNodeTransform(doc_ctx, element);
	}

	SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext_Unsafe(element);
	if (!elem_ctx)
		return FALSE;

	// FIXME: Only if in tree
	if (!elem_ctx->HasAttachedPaintNode())
		return FALSE;

	SVGRenderer* renderer = doc_ctx->GetRenderingState();
	if (!renderer)
		return FALSE;

	SVGPaintNode* paint_node = elem_ctx->GetPaintNode();

#ifdef SVG_SUPPORT_FILTERS
	if (paint_node->AffectsFilter())
		return FALSE;
#endif // SVG_SUPPORT_FILTERS

	// Get new local transform
	SVGElementInfo info;
	info.layouted = elem_ctx->GetLayoutedElement();

	TransformInfo tinfo;
	BOOL set_transform = SVGTraversalObject::FetchTransform(info, tinfo);
	OpRect update_extents;

	RETURN_VALUE_IF_ERROR(paint_node->UpdateTransform(renderer,
													  set_transform ? &tinfo.transform : NULL,
													  update_extents), FALSE);

	if (!update_extents.IsEmpty() && !doc_ctx->IsInvalidationPending())
		doc_ctx->GetSVGImage()->QueueInvalidate(update_extents);

	InvalidateParentBBox(element);
	return TRUE;
}

void SVGDynamicChangeHandler::MarkForRepaint(SVGDocumentContext* doc_ctx, HTML_Element* element, HTML_Element* parent, RepaintReason reason)
{
	OP_ASSERT(element);
	OP_ASSERT(doc_ctx);
//	OP_ASSERT(!g_svg_debug_is_in_layout_pass);

	SVGElementContext *elm_ctx = AttrValueStore::GetSVGElementContext(element);

	//
	// Actions taken on different reasons:
	//
	// ATTRIBUTE_CHANGED:
	// * Invalidate parents
	// * Create elementcontext if none exist (!Timed element)
	// * Recalculate animations (Timed element)
	// * If context exists:
	//   * Add extra invalidation
	// * Schedule invalidation
	//
	// ELEMENT_ADDED:
	// * Invalidate parents
	// * Mark parent container as dirty
	// * Load CSS for subtree
	// * If context exists
	//   * Invalidate element
	// * Schedule invalidation
	//
	// ELEMENT_REMOVED:
	// * Invalidate parents
	// * Mark parent container as dirty
	// * If context exists:
	//   * Add extra invalidation
	//   * Schedule invalidation
	//
	// ELEMENT_STRUCTURE
	// * Invalidate parents
	// * Mark parent container as dirty
	// * Create elementcontext if none exist (!Timed element)
	// * Recalculate animations (Timed element)
	// * If context exists:
	//   * Add extra invalidation
	// * Schedule invalidation
	//
	// ELEMENT_PARSED:
	// * Invalidate parents
	// * Mark parent container as dirty
	// * Load CSS for subtree
	// * If context exists
	//   * Invalidate element
	// * Schedule invalidation
	// * Only rebuild fonts on <font> or <font-face>
	//
	// PAINT_DETAIL_CHANGED:
	// * Invalidate parents
	// * If context exists:
	//   * Invalidate element by setting subtree-changed
	//   * Add extra invalidation
	//   * Schedule invalidation
	//
	// INLINE_CHANGED:
	// * Invalidate parents
	// * If context exists:
	//   * Add extra invalidation
	//   * Schedule invalidation
	//   * Invalidate element
	//

	if (reason == ATTRIBUTE_CHANGED || reason == ELEMENT_STRUCTURE)
	{
		if (SVGUtils::IsTimedElement(element))
		{
			if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
				animation_workplace->InvalidateTimingElement(element);
		}
		else if (!elm_ctx)
		{
			// We need this already because we need to set the "need_css_reload" flag set by
			// elem_ctx->AddInvalidState(SVGElementContext::INVALID_ADDED);
			// below
			elm_ctx = SVGElementContext::Create(element);
			if(!elm_ctx)
			{
				OP_ASSERT(!"Take care of OOM");
				return;
			}
		}
	}

	if (reason == ELEMENT_ADDED || reason == ELEMENT_PARSED)
	{
		if (!elm_ctx &&
			// Just to be really sure that the added element isn't
			// part of a shadowtree
			!SVGUtils::IsShadowNode(element))
		{
			LayoutInfo info(doc_ctx->GetHLDocProfile()->GetLayoutWorkplace());
			HTML_Element* stop_iter = static_cast<HTML_Element*>(element->NextSiblingActualStyle());
			HTML_Element* elm = element;
			while (elm != stop_iter)
			{
				OP_STATUS err = info.workplace->UnsafeLoadProperties(elm);
				if (OpStatus::IsMemoryError(err))
				{
					// How about g_memory_manager->RaiseCondition(err); ?
					OP_ASSERT(!"Take care of OOM");
				}

				elm = elm->NextActualStyle();
			}
		}
	}

	if (reason == ELEMENT_ADDED || reason == ELEMENT_REMOVED ||
		reason == ELEMENT_PARSED || reason == ELEMENT_STRUCTURE)
	{
		if (SVGElementContext* parent_context = AttrValueStore::GetSVGElementContext(parent))
			parent_context->AddInvalidState(INVALID_STRUCTURE);
	}

	BOOL schedule_invalidation = FALSE;
	BOOL add_extra_invalidation = FALSE;

	if (reason == ELEMENT_REMOVED || reason == ATTRIBUTE_CHANGED ||
		reason == PAINT_DETAIL_CHANGED || reason == INLINE_CHANGED ||
		reason == ELEMENT_STRUCTURE)
	{
		// Set if 'element' has a context
		add_extra_invalidation = !!elm_ctx;
		// Notice the overlap with this statement and the one further down
		schedule_invalidation = !!elm_ctx;
	}

	if (reason == ELEMENT_ADDED || reason == ATTRIBUTE_CHANGED ||
		reason == ELEMENT_PARSED || reason == ELEMENT_STRUCTURE)
	{
		schedule_invalidation = TRUE;
	}

	if (elm_ctx)
	{
		if (reason == ELEMENT_ADDED || reason == ATTRIBUTE_CHANGED ||
			reason == INLINE_CHANGED || reason == ELEMENT_PARSED ||
			reason == ELEMENT_STRUCTURE)
		{
			// For ELEMENT_ADDED (and possibly ATTRIBUTE_CHANGED):
			// This can happen if an element has been added without being created.
			// Moved in the tree for example?
			//
			// For INLINE_CHANGED:
			// We need to re-layout this element so that the layout
			// can propagate into the inline document
			elm_ctx->AddInvalidState(INVALID_ADDED);
		}
		else if (reason == PAINT_DETAIL_CHANGED)
		{
			// We need to reset properties on this elements paint
			// node, so we have to at least reach it during the
			// traversal.
			elm_ctx->AddInvalidState(INVALID_SUBTREE);
		}
	}
	// else invalid state will be set to INVALID_ADDED when the
	// SVGElementContext is created, but without the "need_css_reload"
	// flag set.

	if (add_extra_invalidation)
	{
		OP_ASSERT(elm_ctx); // Should have been tested already

		if (SVGRenderer* renderer = doc_ctx->GetRenderingState())
			if (SVGPaintNode* paint_node = elm_ctx->GetPaintNode())
			{
				paint_node->Update(renderer);
				schedule_invalidation = TRUE;
			}
	}

	BOOL is_font_element =
		element->IsMatchingType(Markup::SVGE_FONT, NS_SVG) ||
		element->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG);
	HTML_Element *font_element = is_font_element ? element : NULL;

	BOOL invalidate_text_subtree = SVGUtils::IsTextRootElement(element);

	// Invalidate parents
	HTML_Element* parent_iter = parent;
	while(parent_iter != NULL)
	{
		SVGElementContext *paren_elem_ctx = AttrValueStore::GetSVGElementContext(parent_iter);
		if (paren_elem_ctx)
			paren_elem_ctx->AddInvalidState(INVALID_SUBTREE);

		if (parent_iter->IsMatchingType(Markup::SVGE_FONT, NS_SVG) ||
			parent_iter->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
		{
			font_element = parent_iter;
		}

		if (SVGUtils::IsTextRootElement(parent_iter))
		{
			MarkForRepaint(doc_ctx, parent_iter, parent_iter->Parent(), ATTRIBUTE_CHANGED /*reason*/);
			// We are inside a 'text' element, text layout can depend
			// on (atleast) those elements that precedes the interesting
			// element (i.e. 'element')
			invalidate_text_subtree = TRUE;
		}

		parent_iter = parent_iter->Parent();
	}

	if (invalidate_text_subtree)
	{
		HTML_Element* start_element = element;
		// Attempt to move upwards, looking for 'text'
		while (start_element && !SVGUtils::IsTextRootElement(start_element))
		{
			start_element = start_element->Parent();
		}

		if (!start_element)
			start_element = element;

		// For text - mark the entire subtree, since the text layout can depend
		// on the children
		HTML_Element* stop_iter = static_cast<HTML_Element*>(start_element->NextSibling());
		HTML_Element* iter = start_element;
		while (iter != stop_iter)
		{
			SVGElementContext *child_elem_ctx = AttrValueStore::GetSVGElementContext(iter);
			if (child_elem_ctx)
				child_elem_ctx->AddInvalidState(INVALID_ADDED);

			iter = iter->Next();
		}
	}

	// FIXME: Don't rebuild fonts while parsing (it's done in SVGManagerImpl::EndElement)
	if (font_element != NULL)
	{
		SVGFontElement* font_elm_ctx = AttrValueStore::GetSVGFontElement(font_element, FALSE);
		if (font_elm_ctx)
			font_elm_ctx->MarkFontDataDirty();

		if (is_font_element || reason != ELEMENT_PARSED)
		{
			if (doc_ctx->GetLogicalDocument() && doc_ctx->GetLogicalDocument()->GetRoot()->IsAncestorOf(font_element))
				OpStatus::Ignore(SVGUtils::BuildSVGFontInfo(doc_ctx, font_element)); // FIXME: OOM
			OpStatus::Ignore(HandleFontsChanged(doc_ctx));
		}
	}

	if (schedule_invalidation)
	{
		// If we are an external document, we mark the element that
		// uses us, so that the whole chain (both in parent document
		// and sub-document) can be re-layouted.
		if (doc_ctx->IsExternalAnimation())
		{
			HTML_Element* frame_element = doc_ctx->GetReferencingElement();
			if (frame_element)
			{
				SVGDocumentContext* frame_doc_ctx = AttrValueStore::GetSVGDocumentContext(frame_element);
				if (frame_doc_ctx)
					MarkForRepaint(frame_doc_ctx, frame_element,
								   frame_element->Parent(),
								   INLINE_CHANGED);
			}
		}
		else
		{
			if (doc_ctx->GetSVGImage()->IsVisible())
			{
				doc_ctx->GetSVGImage()->ScheduleUpdate();
			}

			doc_ctx->SetInvalidationPending(TRUE);
		}
	}
}

OP_STATUS SVGDynamicChangeHandler::HandleNewIds(SVGDocumentContext* doc_ctx, HTML_Element* subtree, BOOL* had_new_ids)
{
	HTML_Element* stop = static_cast<HTML_Element*>(subtree->NextSibling());
	HTML_Element* it = subtree;
	while (it != stop)
	{
		const uni_char* id = it->GetId();
		if (id)
		{
			if (had_new_ids)
				*had_new_ids = TRUE;

			RETURN_IF_ERROR(HandleNewId(doc_ctx, it, id));
		}
		it = it->Next();
	}

	return OpStatus::OK;
}

/*static */
void SVGDynamicChangeHandler::GetAffectedNodes(SVGDependencyGraph& graph,
											   HTML_Element* elm,
											   OpPointerSet<HTML_Element>& affected_nodes,
											   OpPointerSet<HTML_Element>& visited_nodes)
{
	NodeSet* edgeset = NULL;
	OpStatus::Ignore(graph.GetDependencies(elm, &edgeset));

	if (!edgeset)
		return;

	NodeSetIterator ei = edgeset->GetIterator();

	for (OP_STATUS iter_status = ei.First();
		 OpStatus::IsSuccess(iter_status);
		 iter_status = ei.Next())
	{
		HTML_Element* depelm = ei.GetEdge(); OP_ASSERT(depelm);
		if (!depelm)
			continue;

		// If this node has already been marked we can ignore it
		OP_STATUS status = visited_nodes.Add(depelm);
		if (OpStatus::IsError(status))
			continue;

		// Mark this node for repaint
		RepaintReason repaint_reason = ATTRIBUTE_CHANGED;
		if (depelm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
			repaint_reason = ELEMENT_STRUCTURE;

		SVGDocumentContext *element_doc_ctx = AttrValueStore::GetSVGDocumentContext(depelm);
		if (!element_doc_ctx)
			continue; // The element is detached

		MarkForRepaint(element_doc_ctx, depelm, depelm->Parent(), repaint_reason);

		// This node and the path to it has been "affected"
		status = affected_nodes.Add(depelm);
		if (OpStatus::IsError(status))
			continue;

		// Mark path to root
		HTML_Element* parent = depelm->Parent();
		while (parent && OpStatus::IsSuccess(status))
		{
			status = affected_nodes.Add(parent);
			parent = parent->Parent();
		}
	}
}

// FIXME: Make this more efficient in case we have changes
// with overlapping "depending" sets. Which is likely if people
// via DOM changes several atributes on the same node for instance.
OP_STATUS SVGDynamicChangeHandler::MarkDependentNodesForRepaint(SVGDocumentContext* doc_ctx,
																HTML_Element* elm_with_parents,
																HTML_Element* elm_with_children)
{
	// Don't call this if there is no dependency graph
	OP_ASSERT(doc_ctx->GetDependencyGraph());
	OP_ASSERT(elm_with_parents != elm_with_children); // Duplicates not allowed

	OpPointerSet<HTML_Element> changed_nodes;
	// Add parents to nodes_that_have_changed
	while (elm_with_parents)
	{
		RETURN_IF_ERROR(changed_nodes.Add(elm_with_parents));
		elm_with_parents = elm_with_parents->Parent();
	}

	if (elm_with_children)
	{
		HTML_Element* stop = static_cast<HTML_Element*>(elm_with_children->NextSibling());
		HTML_Element* it = elm_with_children;
		// Add all children to nodes_that_have_changed
		while (it != stop)
		{
			// Duplicates not possible here
			RETURN_IF_ERROR(changed_nodes.Add(it));
			it = it->Next();
		}
	}

	OpPointerSet<HTML_Element> visited_nodes; // no nodes fully processed yet, leave empty
	return MarkDependentNodesForRepaint(doc_ctx, changed_nodes, visited_nodes);
}

// FIXME: Make this more efficient in case we have changes
// with overlapping "depending" sets. Which is likely if people
// via DOM changes several atributes on the same node for instance.
OP_STATUS SVGDynamicChangeHandler::MarkDependentNodesForRepaint(SVGDocumentContext* doc_ctx,
																OpPointerSet<HTML_Element>& changed_nodes,
																OpPointerSet<HTML_Element>& visited_nodes)
{
	OpPointerSet<HTML_Element> affected_nodes;

	OpPointerSet<HTML_Element>* modified = &changed_nodes;
	OpPointerSet<HTML_Element>* affected = &affected_nodes;

	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	if (!dgraph)
	{
		return OpStatus::OK;
	}
	SVGDependencyGraph& graph = *dgraph;
	// Mark dependent nodes
	while (modified->GetCount())
	{
		OpHashIterator* iter = modified->GetIterator();
		if (!iter)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = iter->First();
		while (OpStatus::IsSuccess(status))
		{
			HTML_Element* elm =	(HTML_Element*)iter->GetKey();

			// Get affected nodes (and mark them for repaint)
			// (nodes depending on modified nodes, and their parents)
			GetAffectedNodes(graph, elm, *affected, visited_nodes);

			status = iter->Next();
		}

		OP_DELETE(iter);

		OpPointerSet<HTML_Element>* tmp = affected;
		affected = modified;
		modified = tmp;

		affected->RemoveAll();
	}

	return OpStatus::OK;
}

/* static */
void SVGDynamicChangeHandler::RemoveSubTreeFromDependencyGraph(SVGDocumentContext* doc_ctx,
															   HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(!element->Parent());

	SVGDependencyGraph *depgraph = doc_ctx->GetDependencyGraph();
	if (depgraph != NULL)
	{
		while (element != NULL)
		{
			depgraph->RemoveNode(element);
#ifdef _DEBUG
			OP_ASSERT(!depgraph->IsDependent(element));
#endif // _DEBUG
			element = element->Next();
		}
	}
}

/* static */
void SVGDynamicChangeHandler::DestroyShadowTree(SVGDocumentContext* doc_ctx, HTML_Element* shadow_root,
												HTML_Element* parent, BOOL repaint)
{
	OP_ASSERT(SVGUtils::IsShadowNode(shadow_root));

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection& sel = doc_ctx->GetTextSelection();
	if (shadow_root->IsAncestorOf(sel.GetElement()))
		sel.ClearSelection(FALSE);
#endif // SVG_SUPPORT_TEXTSELECTION

	// Let's hope that there is nothing on the stack that refers to these
	// elements. There shouldn't be since they are "invisible", but that's
	// probably a too simplistic view of the problem.
	SVG_DOCUMENT_CLASS* doc = doc_ctx->GetDocument();
	shadow_root->Remove(LOGDOC_DOC(doc)); // Maybe just shadow_root->Out() ?

	if (repaint)
		MarkForRepaint(doc_ctx, shadow_root, parent, ELEMENT_REMOVED);

	RemoveSubTreeFromDependencyGraph(doc_ctx, shadow_root);

	if (shadow_root->Clean(LOGDOC_DOC(doc)))
	{
		shadow_root->Free(LOGDOC_DOC(doc));
	}
}

/* static */
void SVGDynamicChangeHandler::RemoveShadowTree(SVGDocumentContext* doc_ctx, HTML_Element* use_elm)
{
	OP_NEW_DBG("SVGManagerImpl::RemoveShadowTree", "svg_shadowtrees");
	OP_ASSERT(doc_ctx);
	OP_ASSERT(use_elm);
	OP_ASSERT(use_elm->IsMatchingType(Markup::SVGE_USE, NS_SVG));

	OP_DBG(("Cutting down shadowtree under use-element: %p", use_elm));

	HTML_Element* child = use_elm->FirstChild();
	while (child)
	{
		HTML_Element* next_child = child->Suc();
		if (SVGUtils::IsShadowNode(child))
			DestroyShadowTree(doc_ctx, child, use_elm, FALSE);

		child = next_child;
	}
	use_elm->SetSpecialAttr(Markup::SVGA_SHADOW_TREE_BUILT, ITEM_TYPE_NUM, NUM_AS_ATTR_VAL(0), FALSE,  SpecialNs::NS_SVG);

	HTML_Element* parent_iter = use_elm;
	while (parent_iter)
	{
		SVGElementContext *paren_elem_ctx = AttrValueStore::GetSVGElementContext(parent_iter);
		if (paren_elem_ctx)
			paren_elem_ctx->AddInvalidState(INVALID_SUBTREE);

		parent_iter = parent_iter->Parent();
	}
}

#if 0
/* static */
void SVGDynamicChangeHandler::RemoveAllShadowTrees(SVGDocumentContext* doc_ctx)
{
	OP_NEW_DBG("SVGManagerImpl::RemoveAllShadowTrees", "svg_shadowtrees");
	OP_DBG(("Removing all shadowtrees."));
	OP_ASSERT(doc_ctx);

	// Start from the root
	HTML_Element* elm = doc_ctx->GetSVGRoot();
	while (elm->Parent())
	{
		elm = elm->Parent();
	}

	// Must kill all shadow trees because one of the shadow nodes might point
	// to a node that is being removed or if this is a new node,
	// then it's possible that it should be added to one or more shadow
	// trees and the only way to modify
	// a shadow tree is to remove it and then rebuild it.
	while ((elm = elm->Next()) != NULL)
	{
		if (elm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
		{
			RemoveShadowTree(doc_ctx, elm);
		}
	}
}
#endif

BOOL SVGDynamicChangeHandler::RemoveDependingUseSubtrees(SVGDocumentContext* doc_ctx,
														 NodeSet& edgeset)
{
	// Go through the dependencies in the edge set, and remove those
	// referring to 'use'-elements
	BOOL use_ref_removed = FALSE;
	NodeSetIterator ei = edgeset.GetIterator();

	for (OP_STATUS status = ei.First();
		 OpStatus::IsSuccess(status);
		 status = ei.Next())
	{
		HTML_Element* depelm = ei.GetEdge();
		if (!depelm)
			continue;

		if (!depelm->IsMatchingType(Markup::SVGE_USE, NS_SVG) &&
			(!SVGUtils::IsShadowNode(depelm) ||
			 !SVGUtils::GetElementToLayout(depelm)->IsMatchingType(Markup::SVGE_USE, NS_SVG)))
			continue;

		use_ref_removed = TRUE;
		HTML_Element* depchild = depelm->FirstChild();

		if (depchild)
			DestroyShadowTree(doc_ctx, depchild, depelm, TRUE);

		SVGUtils::MarkShadowTreeAsBuilt(depelm, FALSE /* not built */, TRUE /* animated */);
	}
	return use_ref_removed;
}

#ifdef _DEBUG
/* static */
BOOL SVGDynamicChangeHandler::IsShadowTreeReferingTo(HTML_Element* use_element, HTML_Element* other_element)
{
	HTML_Element* stop = static_cast<HTML_Element*>(use_element->NextSibling());
	HTML_Element* elm = use_element;
	do
	{
		if (SVGUtils::IsShadowNode(elm))
		{
			if (SVGUtils::GetElementToLayout(elm) == other_element)
			{
				return TRUE;
			}
		}
		elm = static_cast<HTML_Element*>(elm->Next());
	}
	while (elm != stop);
	return FALSE;
}
#endif // _DEBUG

OP_STATUS SVGDynamicChangeHandler::RemoveAllShadowTreesReferringToElement(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
	OP_ASSERT(element);
	OP_ASSERT(doc_ctx);

	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	if (!dgraph)
		return OpStatus::OK;

	for (HTML_Element* parent = element; parent; parent = parent->Parent())
	{
		NodeSet* edgeset = NULL;
		OpStatus::Ignore(dgraph->GetDependencies(parent, &edgeset));
		if (!edgeset || edgeset->IsEmpty())
			continue;

		NodeSetIterator ei = edgeset->GetIterator();

		for (OP_STATUS status = ei.First();
			 OpStatus::IsSuccess(status);
			 status = ei.Next())
		{
			HTML_Element* depelm = ei.GetEdge();
			if (!depelm || !depelm->IsMatchingType(Markup::SVGE_USE, NS_SVG))
				continue;

			RemoveShadowTree(doc_ctx, depelm);
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGDynamicChangeHandler::FixupShadowTreesAfterAdd(SVGDocumentContext* doc_ctx, HTML_Element* added_child, HTML_Element* parent)
{
	OP_ASSERT(added_child);
	OP_ASSERT(doc_ctx);

 	return RemoveAllShadowTreesReferringToElement(doc_ctx, parent);
}

OP_STATUS SVGDynamicChangeHandler::FixupShadowTreesAfterRemove(SVGDocumentContext* doc_ctx, HTML_Element* removed_child, HTML_Element* parent)
{
	OP_ASSERT(removed_child);
	OP_ASSERT(doc_ctx);

	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();
	if (!dgraph)
		return OpStatus::OK;

	HTML_Element* sub_elm = removed_child;
	HTML_Element* stop = sub_elm->NextSiblingActual();
	while (sub_elm != stop)
	{
		NodeSet* edgeset = NULL;
		OpStatus::Ignore(dgraph->GetDependencies(sub_elm, &edgeset));
		if (!edgeset || !RemoveDependingUseSubtrees(doc_ctx, *edgeset))
		{
			// No dependencies or no references to 'use' tree
			sub_elm = sub_elm->NextActual();
		}
		else
		{
			// Skip this subtree
			sub_elm = sub_elm->NextSiblingActual();
		}
	}

	for (; parent; parent = parent->Parent())
	{
		NodeSet* edgeset = NULL;
		OpStatus::Ignore(dgraph->GetDependencies(parent, &edgeset));
		if (!edgeset)
			continue;

		RemoveDependingUseSubtrees(doc_ctx, *edgeset);
	}
	return OpStatus::OK;
}

void SVGDynamicChangeHandler::MarkWholeSVGForRepaint(SVGDocumentContext* doc_ctx)
{
	// A simple way to make it happen
	MarkForRepaint(doc_ctx, doc_ctx->GetSVGRoot(), NULL, ATTRIBUTE_CHANGED);
}

/* virtual */
OP_STATUS SVGDynamicChangeHandler::HandleDocumentChanged(SVGDocumentContext* doc_ctx, HTML_Element *parent, HTML_Element *child, BOOL is_addition)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	doc_ctx->GetSVGImage()->SuspendScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING

	if (is_addition)
	{
		// We load external resources for added elements
		HTML_Element *stop = static_cast<HTML_Element *>(child->NextSibling());
		HTML_Element *element = child;
		while(element != stop)
		{
			if (element->GetNsType() == NS_SVG)
				SVGUtils::LoadExternalReferences(doc_ctx, element);

			element = element->Next();
		}

		MarkForRepaint(doc_ctx, child, parent, ELEMENT_ADDED);
		RETURN_IF_ERROR(FixupShadowTreesAfterAdd(doc_ctx, child, parent));

		// Changing the current view means that we have to recalculate the whole SVG
		if (child->IsMatchingType(Markup::SVGE_VIEW, NS_SVG))
		{
			const uni_char* rel = doc_ctx->GetURL().UniRelName();
			if (rel && child->IsAncestorOf(SVGUtils::FindDocumentRelNode(doc_ctx->GetLogicalDocument(), rel)))
			{
				doc_ctx->GetSVGImage()->InvalidateAll();
				return OpStatus::OK;
			}
		}

		RETURN_IF_ERROR(SVGAnimationWorkplace::Prepare(doc_ctx, child));
	}
	else
	{
		if(child)
		{
			doc_ctx->SubtreeRemoved(child);
		}

#ifdef SVG_SUPPORT_EDITABLE
		HTML_Element* text_parent = parent;
		while (text_parent && !SVGUtils::IsTextRootElement(text_parent))
			text_parent = text_parent->Parent();

		// If this text element is contained in text that is editable,
		// signal the editing context
		if (SVGElementContext* e_ctx = AttrValueStore::GetSVGElementContext(text_parent))
		{
			SVGTextRootContainer* tr_cont = e_ctx->GetAsTextRootContainer();
			if (tr_cont && tr_cont->IsEditing())
				tr_cont->GetEditable()->OnElementRemoved(child);
		}
#endif // SVG_SUPPORT_EDITABLE

		MarkForRepaint(doc_ctx, child, parent, ELEMENT_REMOVED);

		// Detach from paint tree
		if (SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(child))
			if (elm_ctx->HasAttachedPaintNode())
				if (SVGPaintNode* paint_node = elm_ctx->GetPaintNode())
					paint_node->Detach();

		OP_ASSERT(!child->Parent());
		RETURN_IF_ERROR(FixupShadowTreesAfterRemove(doc_ctx, child, parent));
		// Since part of the tree is removed,
		// MarkDependentNodesInTreeForRepaint won't have the chance to
		// discover all dependencies so we have to prepare a little
		// for it here.
		HTML_Element* elm = child;
		const uni_char* rel = doc_ctx->GetURL().UniRelName();
		while (elm)
		{
			const uni_char* id = elm->GetId();
			if (id && *id)
			{
				//doc_ctx->AddChangedId(id);
				if (elm->IsMatchingType(Markup::SVGE_VIEW, NS_SVG) && rel && uni_strstr(rel, id))
				{
					// Removing the current view so we have to recalculate the whole SVG
					doc_ctx->GetSVGImage()->InvalidateAll();
					return OpStatus::OK; // No need for more finetuned invalidation
				}
			}
			elm = elm == child ? parent : elm->Parent();
		}
	}

	// Update the dependency graph, and if we have a dependency graph, use it to mark all related nodes.
	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();

	// If we don't have any dependency graph, then we have not yet
	// layouted and painted the SVG so no need to keep track of
	// depending nodes.
	if (dgraph)
	{
		if (is_addition)
		{
			// Addition.
			BOOL had_new_ids = FALSE;
			RETURN_IF_ERROR(HandleNewIds(doc_ctx, child, &had_new_ids));
			RETURN_IF_ERROR(MarkDependentNodesForRepaint(doc_ctx, parent, NULL));

			// Update the dependency graph. How? Lazy during layout/paint?

			// Dependencies might have appeared if any of the new children have an id
			// and something exists in the tree referring to that id. If the id already
			// exists in the tree we also might have to remove existing dependencies in case
			// "getElementById" now returns the new node.

			if (had_new_ids)
			{
				RETURN_IF_ERROR(UpdateUnresolvedDependencies(dgraph));
			}
		}
		else
		{
			RETURN_IF_ERROR(MarkDependentNodesForRepaint(doc_ctx, parent, child));
			dgraph->RemoveSubTree(child);
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGDynamicChangeHandler::UpdateUnresolvedDependencies(SVGDependencyGraph* dgraph)
{
	OP_ASSERT(dgraph);

	OpHashIterator* iter = dgraph->GetUnresolvedDependenciesIterator();
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = iter->First();
	while (OpStatus::IsSuccess(status))
	{
		HTML_Element* elm =	(HTML_Element*)iter->GetKey();

		if (SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(elm))
		{
			elm_ctx->AddInvalidState(INVALID_ADDED);

			// Invalidate parents
			HTML_Element* parent_iter = elm->Parent();
			while(parent_iter != NULL)
			{
				SVGElementContext *paren_elem_ctx = AttrValueStore::GetSVGElementContext(parent_iter);
				// Skip if already sufficiently dirty
				if (!paren_elem_ctx || paren_elem_ctx->GetInvalidState() >= INVALID_SUBTREE)
					break;

				paren_elem_ctx->AddInvalidState(INVALID_SUBTREE);

				parent_iter = parent_iter->Parent();
			}
		}

		status = iter->Next();
	}

	OP_DELETE(iter);

	dgraph->ClearUnresolvedDependencies();
	return OpStatus::OK;
}

/* static */
void SVGDynamicChangeHandler::HandleElementDiscard(SVGDocumentContext* doc_ctx, HTML_Element* discarded_element)
{
	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(discarded_element);
	if (elm_ctx)
	{
		elm_ctx->SetDiscarded();

		// This supposedly is a spec. requirement, and we do check it in calling code.
		OP_ASSERT(doc_ctx->GetSVGRoot()->IsAncestorOf(discarded_element));

		// Add the screen box from the element here, since we might
		// lose it in the layoutpass between this and the
		// ProcessDiscard call (yes, hacky)
		if (SVGRenderer* renderer = doc_ctx->GetRenderingState())
			if (SVGPaintNode* paint_node = elm_ctx->GetPaintNode())
				paint_node->Update(renderer);
	}

	LogicalDocument* logdoc = doc_ctx->GetLogicalDocument();
	SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(logdoc->GetSVGWorkplace());
	workplace->AddPendingDiscard(discarded_element);
}

/* static */
OP_STATUS SVGDynamicChangeHandler::HandleEndElement(SVGDocumentContext* doc_ctx, HTML_Element *element)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	doc_ctx->GetSVGImage()->SuspendScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING

	// Animation elements should be handled when their parent gets the
	// EndElement callback, otherwise we can crash on e.g discard
	// during parsing.
	if (SVGUtils::IsAnimationElement(element) || element->IsMatchingType(Markup::SVGE_DISCARD, NS_SVG))
		return OpStatus::OK;

	HTML_Element *parent = element->Parent();

	MarkForRepaint(doc_ctx, element, parent, ELEMENT_PARSED);
	RETURN_IF_ERROR(FixupShadowTreesAfterAdd(doc_ctx, element, parent));

	// Changing the current view means that we have to recalculate the whole SVG
	if (element->IsMatchingType(Markup::SVGE_VIEW, NS_SVG))
	{
		const uni_char* rel = doc_ctx->GetURL().UniRelName();
		if (rel && element->IsAncestorOf(SVGUtils::FindDocumentRelNode(doc_ctx->GetLogicalDocument(), rel)))
		{
			doc_ctx->GetSVGImage()->InvalidateAll();
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(SVGAnimationWorkplace::Prepare(doc_ctx, element));

	// Update the dependency graph, and if we have a dependency graph, use it to mark all related nodes.
	SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph();

	// If we don't have any dependency graph, then we have not yet
	// layouted and painted the SVG so no need to keep track of
	// depending nodes.
	if (dgraph)
	{
		// Addition.
		RETURN_IF_ERROR(HandleNewIds(doc_ctx, element));
		RETURN_IF_ERROR(MarkDependentNodesForRepaint(doc_ctx, parent, NULL));

		// Update the dependency graph. How? Lazy during layout/paint?

		// Dependencies might have appeared if any of the new children have an id
		// and something exists in the tree referring to that id. If the id already
		// exists in the tree we also might have to remove existing dependencies in case
		// "getElementById" now returns the new node.
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS SVGDynamicChangeHandler::HandleInlineChanged(SVGDocumentContext* doc_ctx, HTML_Element* element, BOOL is_content_changed)
{
	// Warning for unexpected element types:
	OP_ASSERT(element->GetNsType() == NS_SVG && (SVGUtils::CanHaveExternalReference(element->Type()) || doc_ctx->GetSVGRoot() == element /* root may have css background */ ));

	if (element->IsMatchingType(Markup::SVGE_IMAGE, NS_SVG) || element->IsMatchingType(Markup::SVGE_FEIMAGE, NS_SVG))
	{
		HEListElm* hle = element->GetHEListElm(FALSE); // Always loading as IMAGE_INLINE
		if(hle)
		{
			LoadInlineElm* lie = hle->GetLoadInlineElm();
			URL* url = lie ? lie->GetUrl() : NULL;
			if(url && url->ContentType() == URL_SVG_CONTENT)
			{
				HTML_Element *frame_element = doc_ctx->GetExternalFrameElement(*url, element);
				FramesDocElm* frame = frame_element ? FramesDocElm::GetFrmDocElmByHTML(frame_element) : NULL;
				if(!frame)
				{
					if (frame_element)
					{
						SVGUtils::LoadExternalDocument(*url, doc_ctx->GetDocument(), frame_element);
						if (element != frame_element)
							doc_ctx->RegisterDependency(element, frame_element);
					}

					return OpStatus::OK;
				}
				else if(!(*url == frame->GetCurrentURL()))
				{
					DocumentManager* docman = frame->GetDocManager();
					if(docman)
					{
						DocumentReferrer ref_url;
						if (doc_ctx->GetDocument())
							ref_url = DocumentReferrer(doc_ctx->GetDocument());
						docman->SetReplace(TRUE); // we don't want a history entry for this
						docman->OpenURL(*url, ref_url, TRUE, FALSE);
					}
				}

			}
		}
	}

	// We call prepare again to check for new animation element in
	// loaded frames
	if (SVGUtils::IsExternalProxyElement(element))
	{
		RETURN_IF_ERROR(SVGAnimationWorkplace::Prepare(doc_ctx, element));
	}

	SVGRenderer* r = doc_ctx->GetRenderingState();
	if(r && r->IsActive())
	{
		r->Stop();
	}

	HTML_Element* parent = element->Parent();
	MarkForRepaint(doc_ctx, element, parent, ATTRIBUTE_CHANGED);

	// If we don't have any dependency graph, then we have not yet
	// layouted and painted the SVG so no need to keep track of
	// depending nodes.
	if (SVGDependencyGraph* dgraph = doc_ctx->GetDependencyGraph())
	{
		MarkDependentNodesForRepaint(doc_ctx, element, NULL);
		// No dependencies are changed

		if(!is_content_changed)
		{
			NodeSet* edgeset = NULL;
			OP_STATUS status = dgraph->GetDependencies(element, &edgeset);
			if (OpStatus::IsSuccess(status))
			{
				OP_ASSERT(edgeset);
				NodeSetIterator ei = edgeset->GetIterator();

				for (OP_STATUS status = ei.First();
					 OpStatus::IsSuccess(status);
					 status = ei.Next())
				{
					// Depending node
					HTML_Element* dependent_node = ei.GetEdge();
					if (dependent_node->IsMatchingType(Markup::SVGE_USE, NS_SVG))
					{
						RemoveShadowTree(doc_ctx, dependent_node);
						// Also remove all other shadow trees that indirectly referred to this one
						RemoveAllShadowTreesReferringToElement(doc_ctx, dependent_node);
					}
				}
			}
		}
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS SVGDynamicChangeHandler::HandleCharacterDataChanged(SVGDocumentContext* doc_ctx, HTML_Element* element)
{
#ifdef SVG_INTERRUPTABLE_RENDERING
	doc_ctx->GetSVGImage()->SuspendScriptExecution();
#endif // SVG_INTERRUPTABLE_RENDERING

	BOOL has_tref = doc_ctx->ExistsTRefInSVG();
	// We only need to bother if the text enters a Markup::SVGE_TEXT or if
	// it enters something referred to by a Markup::SVGE_TREF. Since we
	// don't know what is referred to we include all elements with an
	// id in that group.
	HTML_Element* parent = element;
	BOOL is_meaningful_text_change = FALSE;
	HTML_Element* text_parent = NULL;
	HTML_Element* text_root_element = NULL;
	while (parent && !is_meaningful_text_change)
	{
		if (SVGUtils::IsTextRootElement(parent))
		{
			text_root_element = parent;
		}

		if (text_root_element != NULL ||
			(has_tref && parent->GetId()))
		{
			is_meaningful_text_change = TRUE;
		}

		parent = parent->Parent();
		if (!text_parent)
		{
			text_parent = parent;
		}
	}

	if (is_meaningful_text_change && text_parent)
	{
#ifdef SVG_SUPPORT_EDITABLE
		if(text_root_element)
		{
			// If this text element is editable, signal the editing context
			if (SVGElementContext* e_ctx = AttrValueStore::GetSVGElementContext(text_root_element))
			{
				SVGTextRootContainer* tr_cont = e_ctx->GetAsTextRootContainer();
				if (tr_cont && tr_cont->IsEditing())
					tr_cont->GetEditable()->OnElementRemoved(element);
			}
		}
#endif // SVG_SUPPORT_EDITABLE

		MarkForRepaint(doc_ctx, element, text_parent, ATTRIBUTE_CHANGED);

		// If we don't have any dependency graph, then we have not yet
		// layouted and painted the SVG so no need to keep track of
		// depending nodes.
		if (doc_ctx->GetDependencyGraph())
		{
			RETURN_IF_ERROR(MarkDependentNodesForRepaint(doc_ctx, element, NULL));

			// No need to update dependency graph, nothing can have changed
		}
	}

	return OpStatus::OK;
}

#endif // SVG_SUPPORT
