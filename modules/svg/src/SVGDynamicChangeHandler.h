/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_DYNAMIC_CHANGE_H
#define SVG_DYNAMIC_CHANGE_H

#ifdef SVG_SUPPORT

#include "modules/logdoc/namespace.h" // For NS_Type
#include "modules/util/adt/opvector.h"

class SVGDocumentContext;
class HTML_Element;
class SVGDependencyGraph;
class NodeSet;

class SVGDynamicChangeHandler
{
public:
	/**
	 * Makes the whole SVG be repainted and recalculated. Expensive.
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 */
	static void MarkWholeSVGForRepaint(SVGDocumentContext* doc_ctx);

	/**
	 * To be called when a subtree is added or removed from the SVG.
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 */
	static OP_STATUS HandleDocumentChanged(SVGDocumentContext* doc_ctx, HTML_Element *parent, HTML_Element *child, BOOL is_addition);

	/**
	 * To be called when an element is to be discarded from treee (<discard>).
	 */
	static void HandleElementDiscard(SVGDocumentContext* doc_ctx, HTML_Element* discarded_element);

	/**
	 * To be called when an element is closed during parsing.
	 */
	static OP_STATUS HandleEndElement(SVGDocumentContext* doc_ctx, HTML_Element *element);

	/**
	 * To be called when an element should be repainted
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 */
	static OP_STATUS RepaintElement(SVGDocumentContext* doc_ctx, HTML_Element* element);

	/**
	 * To be called when an attribute value is changed, added or removed on a node in the SVG.
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 */
	static OP_STATUS HandleAttributeChange(SVGDocumentContext* doc_ctx, HTML_Element* element, Markup::AttrType attr, NS_Type attr_ns, BOOL was_removed);

	/**
	 * To be called when an element is changed. The change is
	 * described by the changes flags, where the PROPS_CHANGED_SVG_*
	 * is SVG specific.
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 * @param element The element that has changed
	 * @param changes What has happened to the element described by a bitfield.
	 * @return OpStatus::OK for success or an error code.
	 */
	static OP_STATUS HandleElementChange(SVGDocumentContext* doc_ctx, HTML_Element* element, unsigned int changes);

	/**
	 * To be called when the state of some dependent resource of the
	 * element has changed, i.e. when the
	 * image has loaded for an Markup::SVGE_IMAGE or the text for an Markup::SVGE_TREF.
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 */
	static OP_STATUS HandleInlineChanged(SVGDocumentContext* doc_ctx, HTML_Element* element, BOOL is_content_changed=FALSE);

	/**
	 * To be called when some text changes inside the SVG.
	 *
	 * @param doc_ctx The SVG:s SVGDocumentContext. Can not be NULL.
	 */
	static OP_STATUS HandleCharacterDataChanged(SVGDocumentContext* doc_ctx, HTML_Element* element);

	/**
	 * To be called when fonts arrive or disappear or is otherwise
	 * changed in the document. Causes a repaint of all text and
	 * everything depending of the text. In other word, this may be a
	 * expensive operation.
	 *
	 * This function needs to reparse all style, both inline and in
	 * style-elements. It needs to do so because font-family
	 * properties must be recomputed when the set of fonts availiable
	 * change.
	 *
	 * Beware that it also invalidates all text elements. The reason
	 * for this is that we currently have no way of knowing which text
	 * elements that needs invalidation if the set of fonts
	 * change. One reason for this is when a font is added we don't
	 * know where the font _wasn't_ selected before, but is selected
	 * then the font is added.
	 *
	 * @return OpStatus::OK for success or an error code.
	 */
	static OP_STATUS HandleFontsChanged(SVGDocumentContext* docctx);

private:
	  enum RepaintReason
	  {
		  /**
		   * To have something to compare with. Should never be sent
		   * anywhere.
		   */
		  NO_REASON = 0,

		  /**
		   * The element (and all its children) will be removed. In
		   * particular it will not grow the bounding box of any of its parents.
		   */
		  ELEMENT_REMOVED,

		  /**
		   * This element was added.
		   */
		  ELEMENT_ADDED,

		  /**
		   * This element was subjected to a change that will effect
		   * the structure of the tree.
		   */
		  ELEMENT_STRUCTURE,

		  /**
		   * This element was added via the parser.
		   */
		  ELEMENT_PARSED,

		  /**
		   * Paint thing changed. For instance a gradient or a colour.
		   */
		  PAINT_DETAIL_CHANGED,

		  /**
		   * Attribute has changed. Is as a REMOVE+ADD+CSS reload.
		   */
		  ATTRIBUTE_CHANGED,

		  /**
		   * An inlined resource has changed
		   */
		  INLINE_CHANGED
	  };

	/** Undefined behaviour - but call it every now and then */
	static OP_STATUS HandleNewIds(SVGDocumentContext* doc_ctx, HTML_Element* subtree, BOOL* had_new_ids = NULL);
	/** Undefined behaviour - but call it every now and then */
	static OP_STATUS HandleNewId(SVGDocumentContext* doc_ctx, HTML_Element* element, const uni_char* id);
	/** Undefined behaviour - but call it every now and then */
	static OP_STATUS HandleRemovedId(SVGDocumentContext* doc_ctx, HTML_Element* element);

	/** Undefined behaviour - but call it every now and then
	 *
	 * @param elm_with_parents All parents are initially assumed to be affected.
	 * @param elm_with_children All children are initially assumed to be affected.
	 */
	static OP_STATUS MarkDependentNodesForRepaint(SVGDocumentContext* doc_ctx,
												  HTML_Element* elm_with_parents,
												  HTML_Element* elm_with_children);
	/** Undefined behaviour - but call it every now and then */
	static OP_STATUS MarkDependentNodesForRepaint(SVGDocumentContext* doc_ctx,
												  OpPointerSet<HTML_Element>& changed_nodes,
												  OpPointerSet<HTML_Element>& visited_nodes);

	static void InvalidateDependents(SVGDocumentContext* doc_ctx, HTML_Element* element);
	/** Update the dependencies that were unresolved in the document */
	static OP_STATUS UpdateUnresolvedDependencies(SVGDependencyGraph* dgraph);

	static void RemoveSubTreeFromDependencyGraph(SVGDocumentContext* doc_ctx, HTML_Element* element);

	static void MarkForRepaint(SVGDocumentContext* doc_ctx, HTML_Element* element, HTML_Element* parent, RepaintReason reason);
	static BOOL UpdatePaintNodeTransform(SVGDocumentContext* doc_ctx, HTML_Element* element);
	static void DestroyShadowTree(SVGDocumentContext* doc_ctx, HTML_Element* shadow_root,
								  HTML_Element* parent, BOOL repaint);
	static void RemoveShadowTree(SVGDocumentContext* doc_ctx, HTML_Element* use_elm);
#if 0
	static void RemoveAllShadowTrees(SVGDocumentContext* doc_ctx);
#endif
	static OP_STATUS FixupShadowTreesAfterAdd(SVGDocumentContext* doc_ctx, HTML_Element* added_child, HTML_Element* parent);
	static BOOL RemoveDependingUseSubtrees(SVGDocumentContext* doc_ctx, NodeSet& edgeset);
	static OP_STATUS FixupShadowTreesAfterRemove(SVGDocumentContext* doc_ctx, HTML_Element* removed_child, HTML_Element* parent);
	static OP_STATUS RemoveAllShadowTreesReferringToElement(SVGDocumentContext* doc_ctx, HTML_Element* element);
#ifdef _DEBUG
	static BOOL IsShadowTreeReferingTo(HTML_Element* use_element, HTML_Element* other_element);
#endif // _DEBUG
	static void GetAffectedNodes(SVGDependencyGraph& graph,
								 HTML_Element* elm,
								 OpPointerSet<HTML_Element>& affected_nodes,
								 OpPointerSet<HTML_Element>& nodes_marked_for_repaint);
};

#endif // SVG_SUPPORT
#endif // SVG_DYNAMIC_CHANGE_H
