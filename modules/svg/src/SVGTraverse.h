/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_TRAVERSER_H
#define SVG_TRAVERSER_H

#if defined(SVG_SUPPORT)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/SVGElementStateContext.h"

class SVGElementContext;
class SVGDocumentContext;
class SVGFilter;
class HTML_Element;
class LayoutProperties;
class SVGPaintNode;
class SVGCanvasState;
class SVGTextArguments;
struct SVGTextAreaInfo;
class SVGTextBlock;
struct SVGLayoutState;
class SvgProperties;
class SVGElementResolver;
class SVGChildIterator;
class SVGMediaBinding;
class SVGTextRootContainer;

#ifdef _DEBUG
# define PrintElmType(elm) \
	do { \
	const uni_char* theTypeString = elm->IsText() ? UNI_L("<textnode>") : HTM_Lex::GetElementString((elm)->Type(), (elm)->GetNsIdx()); \
	OP_DBG((UNI_L("Element [%p] type: %s (type: %d nsidx: %d)."), \
			elm, theTypeString ? theTypeString : UNI_L("N/A"), (elm)->Type(), (elm)->GetNsIdx())); \
	} while(0)
# define PrintElmTypeIndent(elm, prefix, indent) \
	do { \
	char indentstr[32]; /* ARRAY OK 2011-01-19 ed */ \
	for (unsigned i = 0; i < MIN(indent,31); i++) \
		indentstr[i] = ' '; \
	indentstr[MIN(indent,31)] = 0; \
	const uni_char* theTypeString = elm->IsText() ? UNI_L("<textnode>") : HTM_Lex::GetElementString((elm)->Type(), (elm)->GetNsIdx()); \
	OP_DBG(("%s%s Element [%p] type: %s (type: %d nsidx: %d).", \
			indentstr, prefix, elm, theTypeString ? make_singlebyte_in_tempbuffer(theTypeString, uni_strlen(theTypeString)) : "N/A", (elm)->Type(), (elm)->GetNsIdx())); \
	} while(0)
#else
# define PrintElmType(elm) do{}while(0)
# define PrintElmTypeIndent(elm,prefix,indent) do{}while(0)
#endif // _DEBUG

// FIXME: Should this perhaps be moved to a/the traversal object
// hierarchy, since it is only called from there, and seems to require
// traversal context (tparams, viewport, ...) ?
OP_STATUS GetElementBBox(SVGDocumentContext* doc_ctx, HTML_Element* traversed_elm,
						 const SVGNumberPair& initial_viewport, SVGBoundingBox& bbox);

//////////////////////////////////////////////////////////////////////

enum
{
	RESOURCE_CLIPPATH = 0,
	RESOURCE_MASK,
	RESOURCE_VIEWPORT,
	RESOURCE_VIEWPORT_CLIP
};

struct SVGElementInfo
{
	HTML_Element* traversed;
	HTML_Element* layouted;

	LayoutProperties* props;
	SVGElementContext* context;
	SVGPaintNode* paint_node;

	unsigned GetInvalidState() const { return state; }
	BOOL GetCSSReload() const { return css_reload; }

	BOOL IsSet(unsigned resbit) { return (has_resource & (1u << resbit)) != 0; }
	void Set(unsigned resbit) { has_resource |= (1u << resbit); }
	void Clear(unsigned resbit) { has_resource &= ~(1u << resbit); }

	// Use the accessors above when reading the following
	unsigned state:16; // Currently matches SVGElementInvalidState
	unsigned css_reload:1;
	unsigned has_transform:1;		// Does this element have a transform
	unsigned has_text_attrs:1;		// Did we push a text attribute frame
	unsigned has_canvasstate:1;		// Did we push a state for this element
	unsigned has_container_state:1;	// Did we push a container state for this element
	unsigned has_resource:4;		// Did we start a clip/mask/et.c for this element
};

struct ViewportInfo
{
	SVGMatrix transform;
	SVGMatrix user_transform;

	SVGRect viewport;
	SVGRect actual_viewport;

	short overflow;
	BOOL slice;
};

struct TransformInfo
{
	TransformInfo() : needs_reset(FALSE) {}

	SVGMatrix transform;
	SVGNumberPair offset;
	BOOL needs_reset;
};

class SVGTraversalObject
{
public:
	SVGTraversalObject(SVGChildIterator* child_iterator,
					   BOOL ignore_pointer_events = TRUE,
					   BOOL apply_paint = TRUE,
					   BOOL calculate_bbox = FALSE) :
		m_saved_viewports(NULL),
		m_owns_resolver(FALSE),
		m_child_iterator(child_iterator),
		m_textinfo(NULL), m_textarea_info(NULL),
		m_initial_invalid_state(NOT_INVALID),
		m_ignore_pointer_events(ignore_pointer_events), m_allow_raster_fonts(TRUE),
		m_apply_paint(apply_paint), m_calculate_bbox(calculate_bbox), m_use_traversal_filter(TRUE),
		m_detached_operation(FALSE) {}
	virtual ~SVGTraversalObject();

	OP_STATUS PushState(SVGElementInfo& info)
	{
		RETURN_IF_ERROR(m_canvas->SaveState());
		info.has_canvasstate = 1;
		return OpStatus::OK;
	}
	OP_STATUS PopState(SVGElementInfo& info)
	{
		if (!info.has_canvasstate)
			return OpStatus::ERR_NO_MEMORY;

		m_canvas->RestoreState();
		return OpStatus::OK;
	}

	static BOOL FetchTransform(SVGElementInfo& info, TransformInfo& tinfo);
	static BOOL FetchUseOffset(SVGElementInfo& info, const SVGValueContext& vcxt, TransformInfo& tinfo);
	void ApplyTransform(SVGElementInfo& info, const TransformInfo& tinfo);
	void ApplyUserTransform(SVGElementInfo& info, const ViewportInfo& vpinfo);

	OP_STATUS CalculateSVGViewport(SVGElementInfo& info, ViewportInfo& vpinfo)
	{
		return CalculateSVGViewport(m_doc_ctx, info, GetValueContext(), m_root_transform, vpinfo);
	}
	static OP_STATUS CalculateSVGViewport(SVGDocumentContext* doc_ctx, SVGElementInfo& info,
										  const SVGValueContext& vcxt,
										  SVGMatrix& root_transform, ViewportInfo& vpinfo);
	OP_STATUS CalculateSymbolViewport(SVGElementInfo& info, ViewportInfo& vpinfo);

	void UpdateValueContext(SVGElementInfo& info);
	void UpdateValueContext(const SVGNumber& root_font_size) { m_value_context.root_font_size = root_font_size; }
	const SVGValueContext& GetValueContext() const { return m_value_context; }

	BOOL UseTraversalFilter() const { return m_use_traversal_filter; }
	void SetTraversalFilter(BOOL filter_elements) { m_use_traversal_filter = filter_elements; }
	BOOL IgnorePointerEvents() const { return m_ignore_pointer_events; }
	BOOL AllowRasterFonts() const { return m_allow_raster_fonts; }
	void SetAllowRasterFonts(BOOL allow_raster_fonts) { m_allow_raster_fonts = allow_raster_fonts; }
	BOOL ShouldApplyPaint() const { return m_apply_paint; }
	BOOL CalculateBBox() const { return m_calculate_bbox; }
	void SetDetachedOperation(BOOL detached_op) { m_detached_operation = detached_op; }
	BOOL GetDetachedOperation() const { return m_detached_operation; }
	void SetInitialInvalidState(SVGElementInvalidState invalid_state) { m_initial_invalid_state = invalid_state; }
	SVGElementInvalidState GetInitialInvalidState() const { return m_initial_invalid_state; }

	/**
	 * Determine if the element denoted by info should be entered.
	 * @return FALSE to skip the element, TRUE otherwise.
	 */
	virtual BOOL AllowElementTraverse(SVGElementInfo& info) { return TRUE; }

	/**
	 * This is hook called just before an element is entered (after
	 * having been 'allowed' by the method above). The primary usage
	 * is to create a paint node for the element and put it in info.
	 */
	virtual OP_STATUS PrepareElement(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo) { return OpStatus::OK; }
	virtual OP_STATUS LeaveContainer(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo) { return OpStatus::OK; }
	virtual OP_STATUS LeaveElement(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterUseElement(SVGElementInfo& info, const TransformInfo& tinfo);

#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS EnterClipPath(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveClipPath(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterMask(SVGElementInfo& info, const TransformInfo& tinfo) { return OpStatus::OK; }
	virtual OP_STATUS LeaveMask(SVGElementInfo& info) { return OpStatus::OK; }
#endif // SVG_SUPPORT_STENCIL

	virtual OP_STATUS HandleGraphicsElement(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS HandleExternalContent(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS PushSVGViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);
	OP_STATUS PopSVGViewport(SVGElementInfo& info);

	virtual OP_STATUS PushSymbolViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);
	OP_STATUS PopSymbolViewport(SVGElementInfo& info);

	/** Hook used to validate that a use shadow tree is built and is valid */
	virtual OP_STATUS ValidateUse(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterAnchor(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveAnchor(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterTextGroup(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveTextGroup(SVGElementInfo& info) { return OpStatus::OK; }

	/** Hooks for entering/leaving elements that are allowed to/can
	 *  have textnodes as their children (deriving from
	 *  SVGTextContainer:s generally). */
	virtual OP_STATUS EnterTextContainer(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveTextContainer(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterTextElement(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveTextElement(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterTextArea(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveTextArea(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS EnterTextNode(SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS LeaveTextNode(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS HandleTextElement(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS HandleFilterPrimitive(SVGElementInfo& info) { return OpStatus::OK; }

	void SetCurrentViewport(const SVGNumberPair& vp) { m_current_viewport = vp; }
	const SVGNumberPair& GetCurrentViewport() const { return m_current_viewport; }

	OP_STATUS PushViewport(const SVGNumberPair& vp);
	void PopViewport();

	OP_STATUS SetupResolver(SVGElementResolver* external_resolver = NULL);
	SVGElementResolver* GetResolver() const { return m_resolver; }

	void SetDocumentContext(SVGDocumentContext* doc_ctx)
	{
		m_doc_ctx = doc_ctx;
	}
	SVGDocumentContext* GetDocumentContext() const { return m_doc_ctx; }

	void SetCanvas(SVGCanvas* canvas) { m_canvas = canvas; }
	SVGCanvas* GetCanvas() const { return m_canvas; }

	OP_STATUS CreateTextInfo(SVGTextRootContainer* container, SVGTextData* textdata = NULL);

	SVGChildIterator* GetChildPolicy() { return m_child_iterator; }

#ifndef FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE
	/**
	 * This calculates if the first whitespace in
	 * traversed_elm (which must be/point to a HE_TEXT) 
	 * should be kept if xml:space="default", and if a 
	 * space should be kept at the end.
	 *
	 * @param traversed_elm
	 *
	 */
	static void CalculateSurroundingWhiteSpace(HTML_Element* traversed_elm, SVGDocumentContext* doc_ctx,
											   BOOL& keep_trailing_whitespace);
#endif // !FOLLOW_THE_SVG_1_1_SPEC_EVEN_WHEN_ITS_UNUSABLE

protected:
	virtual OP_STATUS InvisibleElement(SVGElementInfo& info);

	SVGMatrix m_root_transform;			///< The CTM of the root SVG element, used for ref-transforms
	SVGValueContext m_value_context;	///< The context use to resolve relative values

	TempBuffer m_buffer;				///< Buffer used for text processing / normalization

	SVGNumberPair m_current_viewport;	///< The current viewport
	struct SavedViewport
	{
		SVGNumberPair viewport;
		SavedViewport* next;
	};
	SavedViewport* m_saved_viewports;

	struct
	{
		SVGNumberPair viewport;
		SVGDocumentContext* doc_ctx;
		HTML_Element* element;
	} m_stencil_target;					///< Context structure for stencil subtraversals

	SVGDocumentContext* m_doc_ctx;		///< The context of the currently traversed fragment
	SVGElementResolver* m_resolver;		///< Used for breaking circular traversal loops
	BOOL m_owns_resolver;

	SVGChildIterator* m_child_iterator;	///< Policy object used when iterating children

	SVGCanvas* m_canvas;				///< The canvas to use during the traverse

	OP_STATUS PrepareTextBlock(SVGTextBlock& block, SVGElementInfo& info, HTML_Element* text_content_elm);
	void SetupTextRootProperties(SVGElementInfo& info);
	void DestroyTextInfo();

	SVGTextArguments* m_textinfo;
	SVGTextAreaInfo* m_textarea_info;

	SVGElementInvalidState m_initial_invalid_state;
	BOOL m_ignore_pointer_events;
	BOOL m_allow_raster_fonts;
	BOOL m_apply_paint;
	BOOL m_calculate_bbox;
	BOOL m_use_traversal_filter;
	BOOL m_detached_operation;
};

//
// SVGVisualTraversalObject - traversal object, common ancestor of a
// lot of the traversal objects that deal mostly in 'geometry' (maybe
// it should be named SVGGeometryTraversalObject ?)
//
class SVGVisualTraversalObject : public SVGTraversalObject
{
public:
	SVGVisualTraversalObject(SVGChildIterator* child_iterator,
							 BOOL ignore_pointer_events = TRUE,
							 BOOL apply_paint = TRUE,
							 BOOL calculate_bbox = FALSE) :
		SVGTraversalObject(child_iterator, ignore_pointer_events, apply_paint, calculate_bbox)
#ifdef SVG_SUPPORT_MARKERS
		, m_in_marker(FALSE)
#endif // SVG_SUPPORT_MARKERS
	{}

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo) = 0;
	virtual OP_STATUS LeaveElement(SVGElementInfo& info) = 0;

	// virtual OP_STATUS EnterContainer(SVGElementInfo& info);
	virtual OP_STATUS LeaveContainer(SVGElementInfo& info);

	virtual OP_STATUS HandleGraphicsElement(SVGElementInfo& info);
	virtual OP_STATUS HandleExternalContent(SVGElementInfo& info);

	virtual OP_STATUS PushSVGViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);
	virtual OP_STATUS PushSymbolViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);

#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS EnterClipPath(SVGElementInfo& info);
	virtual OP_STATUS LeaveClipPath(SVGElementInfo& info);
#endif // SVG_SUPPORT_STENCIL

	virtual OP_STATUS EnterAnchor(SVGElementInfo& info);
	virtual OP_STATUS LeaveAnchor(SVGElementInfo& info);

	virtual OP_STATUS EnterTextGroup(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextGroup(SVGElementInfo& info);

	virtual OP_STATUS EnterTextContainer(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextContainer(SVGElementInfo& info);

	virtual OP_STATUS EnterTextElement(SVGElementInfo& info);
	virtual OP_STATUS HandleTextElement(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextElement(SVGElementInfo& info);

	virtual OP_STATUS EnterTextArea(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextArea(SVGElementInfo& info);

	virtual OP_STATUS EnterTextRoot(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextRoot(SVGElementInfo& info);

	virtual OP_STATUS EnterTextNode(SVGElementInfo& info);

	virtual OP_STATUS ValidateUse(SVGElementInfo& info);

protected:
	OP_STATUS SetupGeometricProperties(SVGElementInfo& info);
	OP_STATUS SetupPaintProperties(SVGElementInfo& info);
	OP_STATUS SetupPaint(SVGElementInfo& info, const SVGPaint* paint, BOOL isFill);

	OP_STATUS PushTextProperties(SVGElementInfo& info);
	void PopTextProperties(SVGElementInfo& info);

	virtual OP_STATUS SetupTextAreaElement(SVGElementInfo& info);
	virtual OP_STATUS PrepareTextAreaElement(SVGElementInfo& info);
	virtual void HandleTextAreaBBox(const SVGBoundingBox& bbox) {}
#ifdef SVG_SUPPORT_EDITABLE
	virtual void FlushCaret(SVGElementInfo& info);
#endif // SVG_SUPPORT_EDITABLE

	OP_BOOLEAN FetchAltGlyphs(SVGElementInfo& info);
	virtual OP_STATUS HandleAltGlyph(SVGElementInfo& info, BOOL have_glyphs);

	virtual OP_STATUS HandleTextNode(SVGElementInfo& info, HTML_Element* text_content_elm,
									 SVGBoundingBox* bbox);

	virtual OP_STATUS SetupExtents(SVGElementInfo& info);
	OP_STATUS SetupTextOverflow(SVGElementInfo& info);

	OP_STATUS CalculateTextAreaExtents(SVGElementInfo& info);

	virtual OP_STATUS HandlePaintserver(SVGElementInfo& info, HTML_Element* paintserver, BOOL is_fill,
										BOOL& ignore_fallback) = 0;

	virtual OP_STATUS HandleRasterImage(SVGElementInfo& info, URL* imageURL, const SVGRect& img_vp, int quality) { return OpStatus::OK; }
#ifdef SVG_SUPPORT_SVG_IN_IMAGE
	virtual OP_STATUS HandleVectorImage(SVGElementInfo& info, const SVGRect& img_vp) { return OpStatus::OK; }
#endif // SVG_SUPPORT_SVG_IN_IMAGE
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	virtual OP_STATUS HandleForeignObject(SVGElementInfo& info, const SVGRect& img_vp, int quality) { return OpStatus::OK; }
#endif // SVG_SUPPORT_FOREIGNOBJECT
#ifdef SVG_SUPPORT_MEDIA
	/**
	 * Handle the video. Virtual to allow special handling
	 * of layout traversals for Video Layer (see PI_VIDEO_LAYER).
	 */
	virtual OP_STATUS HandleVideo(SVGElementInfo& info, BOOL draw, SVGBoundingBox* bbox);
	/**
	 * Draw the video. Virtual to allow some subclasses to
	 * avoid expensive operations with video bitmaps.
	 * Draw a dummy rectangle by default.
	 */
	virtual OP_STATUS DrawVideo(SVGElementInfo& info, SVGMediaBinding* mbind, const SVGRect& dst_rect);
#endif // SVG_SUPPORT_MEDIA

	virtual OP_STATUS HandleForcedLineBreak(SVGElementInfo& info);
#ifdef SVG_SUPPORT_EDITABLE
	virtual void CheckCaret(SVGElementInfo& info);
#endif // SVG_SUPPORT_EDITABLE

#ifdef SVG_SUPPORT_MARKERS
	void ResolveMarker(SVGElementInfo& info, const SVGURLReference& urlref, HTML_Element*& out_elm);

	void SetInMarker(BOOL in_marker) { m_in_marker = in_marker; }
	BOOL IsInMarker() const { return m_in_marker; }

	BOOL m_in_marker;
#else
	BOOL IsInMarker() const { return FALSE; }
#endif // SVG_SUPPORT_MARKERS

	OP_STATUS FillViewport(SVGElementInfo& info, const SVGRect& viewport);

#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);

	OP_STATUS SetupClipOrMask(SVGElementInfo& info, const SVGURLReference& urlref,
							  Markup::Type match_type, unsigned resource_tag);
	OP_STATUS GenerateClipOrMask(HTML_Element* element);
	void CleanStencils(SVGElementInfo& info);
#endif // SVG_SUPPORT_STENCIL

	virtual void SavePendingNodes(List<SVGPaintNode>& nodes) {}
	virtual void RestorePendingNodes(List<SVGPaintNode>& nodes) {}

	virtual OP_STATUS PushReferringNode(SVGElementInfo& info) { return OpStatus::OK; }
	virtual void PopReferringNode() {}
};

class SVGCompositePaintNode;
#ifdef SVG_SUPPORT_MARKERS
class SVGMarkerNode;
#endif // SVG_SUPPORT_MARKERS

//
// SVGLayoutObject - traversal object for the layout pass
//
class SVGLayoutObject : public SVGVisualTraversalObject
{
public:
	SVGLayoutObject(SVGChildIterator* child_iterator) :
		SVGVisualTraversalObject(child_iterator,
								 TRUE /* ignore pevents */,
								 TRUE /* apply paint */,
								 TRUE /* calculate bbox */)
#ifdef SVG_SUPPORT_FILTERS
		, m_background_count(0)
#endif // SVG_SUPPORT_FILTERS
		, m_container_stack(NULL)
		, m_current_buffer_element(NULL)
		, m_referring_paint_node(NULL)
		, m_current_container(NULL)
		, m_current_pred(NULL)
	{}
	virtual ~SVGLayoutObject();

	virtual BOOL AllowElementTraverse(SVGElementInfo& info);

	virtual OP_STATUS PrepareElement(SVGElementInfo& info);

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveElement(SVGElementInfo& info);

	virtual OP_STATUS EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveContainer(SVGElementInfo& info);

	virtual OP_STATUS EnterUseElement(SVGElementInfo& info, const TransformInfo& tinfo);

#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS EnterClipPath(SVGElementInfo& info);
	virtual OP_STATUS LeaveClipPath(SVGElementInfo& info);

	virtual OP_STATUS EnterMask(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveMask(SVGElementInfo& info);
#endif // SVG_SUPPORT_STENCIL

	virtual OP_STATUS HandleGraphicsElement(SVGElementInfo& info);
	virtual OP_STATUS HandleTextElement(SVGElementInfo& info);
	virtual OP_STATUS HandleExternalContent(SVGElementInfo& info);

	virtual OP_STATUS PushSVGViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);

	virtual OP_STATUS EnterTextGroup(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextGroup(SVGElementInfo& info);

	virtual OP_STATUS EnterTextRoot(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextRoot(SVGElementInfo& info);

	virtual OP_STATUS EnterTextContainer(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextContainer(SVGElementInfo& info);

	virtual OP_STATUS EnterTextNode(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextNode(SVGElementInfo& info);

	virtual OP_STATUS LeaveTextArea(SVGElementInfo& info);

	void SetLayoutState(SVGLayoutState* layout_state) { m_layout_state = layout_state; }

protected:
#ifdef SVG_SUPPORT_MARKERS
	OP_STATUS LayoutMarker(HTML_Element* marker_elm, SVGMarkerNode*& out_marker_node);
	OP_STATUS HandleMarkers(SVGElementInfo& info);
#endif // SVG_SUPPORT_MARKERS
	virtual OP_STATUS HandlePaintserver(SVGElementInfo& info, HTML_Element* paintserver, BOOL is_fill,
										BOOL& ignore_fallback);

	virtual OP_STATUS SetupTextAreaElement(SVGElementInfo& info);
	virtual OP_STATUS SetupExtents(SVGElementInfo& info);

	virtual void HandleTextAreaBBox(const SVGBoundingBox& bbox);
	virtual OP_STATUS HandleTextNode(SVGElementInfo& info, HTML_Element* text_content_elm,
									 SVGBoundingBox* bbox);
	virtual OP_STATUS HandleAltGlyph(SVGElementInfo& info, BOOL have_glyphs);

	virtual OP_STATUS HandleRasterImage(SVGElementInfo& info, URL* imageURL, const SVGRect& img_vp, int quality);
#ifdef SVG_SUPPORT_SVG_IN_IMAGE
	virtual OP_STATUS HandleVectorImage(SVGElementInfo& info, const SVGRect& img_vp);
#endif // SVG_SUPPORT_SVG_IN_IMAGE
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	virtual OP_STATUS HandleForeignObject(SVGElementInfo& info, const SVGRect& img_vp, int quality);
#endif // SVG_SUPPORT_FOREIGNOBJECT

#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipViewport(SVGElementInfo& info, const ViewportInfo& vpinfo);
#endif // SVG_SUPPORT_STENCIL

	virtual OP_STATUS InvisibleElement(SVGElementInfo& info);

	void UpdateElement(SVGElementInfo& info);
	void SetTextAttributes(SVGElementInfo& info);

#ifdef SVG_SUPPORT_FILTERS
	OP_STATUS SetupFilter(SVGElementInfo& info);

	unsigned m_background_count;
#endif // SVG_SUPPORT_FILTERS

#ifdef SVG_SUPPORT_MEDIA
	virtual OP_STATUS HandleVideo(SVGElementInfo& info, BOOL draw, SVGBoundingBox* bbox);
#endif // SVG_SUPPORT_MEDIA

	SVGLayoutState* m_layout_state;

	OP_STATUS CreatePaintNode(SVGElementInfo& info);

	void AttachPaintNode(SVGElementInfo& info);
	void DetachPaintNode(SVGElementInfo& info);

	List<SVGPaintNode> m_pending_nodes;

	struct ContainerState
	{
		ContainerState(SVGCompositePaintNode* c, SVGPaintNode* p, ContainerState* cs)
			: container(c), pred(p), next(cs) {}

		SVGCompositePaintNode* container;
		SVGPaintNode* pred;
		ContainerState* next;
	};
	ContainerState* m_container_stack;

	OP_STATUS PushContainerState(SVGElementInfo& info);
	void PopContainerState(SVGElementInfo& info);
	void CleanContainerStack()
	{
		while (m_container_stack)
		{
			ContainerState* next = m_container_stack->next;
			OP_DELETE(m_container_stack);
			m_container_stack = next;
		}
	}

	HTML_Element* m_current_buffer_element;

	SVGPaintNode* m_referring_paint_node;
	SVGStack<SVGPaintNode> m_referrer_stack;

	virtual void SavePendingNodes(List<SVGPaintNode>& nodes)
	{
		OP_ASSERT(nodes.Empty());
		nodes.Append(&m_pending_nodes);
	}
	virtual void RestorePendingNodes(List<SVGPaintNode>& nodes)
	{
		OP_ASSERT(m_pending_nodes.Empty());
		m_pending_nodes.Append(&nodes);
	}

	virtual OP_STATUS PushReferringNode(SVGElementInfo& info);
	virtual void PopReferringNode();

	SVGCompositePaintNode* m_current_container;
	SVGPaintNode* m_current_pred;
};

//
// SVGPaintTreeBuilder - traversal object for building instance paint trees
//
class SVGPaintTreeBuilder : public SVGLayoutObject
{
public:
	SVGPaintTreeBuilder(SVGChildIterator* child_iterator,
						SVGCompositePaintNode* tree_container) :
		SVGLayoutObject(child_iterator)
	{
		m_current_container = tree_container;
		m_use_traversal_filter = FALSE;
		m_detached_operation = TRUE;
	}
};

//
// SVGIntersectionObject - traversal object for intersection calculations
//
class SVGIntersectionObject : public SVGVisualTraversalObject
{
public:
	SVGIntersectionObject(SVGChildIterator* child_iterator) :
		SVGVisualTraversalObject(child_iterator,
								 FALSE /* ignore pevents */,
								 FALSE /* apply paint */) {}

	virtual BOOL AllowElementTraverse(SVGElementInfo& info);

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveElement(SVGElementInfo& info);

	virtual OP_STATUS EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo);

	virtual OP_STATUS HandleGraphicsElement(SVGElementInfo& info);
	virtual OP_STATUS HandleTextElement(SVGElementInfo& info);
	virtual OP_STATUS HandleExternalContent(SVGElementInfo& info);

	virtual OP_STATUS ValidateUse(SVGElementInfo& info) { return OpStatus::OK; }

protected:
	virtual OP_STATUS HandlePaintserver(SVGElementInfo& info, HTML_Element* paintserver, BOOL is_fill,
										BOOL& ignore_fallback) { return OpStatus::OK; }

	virtual OP_STATUS HandleRasterImage(SVGElementInfo& info, URL* imageURL, const SVGRect& img_vp, int quality);
#ifdef SVG_SUPPORT_SVG_IN_IMAGE
	virtual OP_STATUS HandleVectorImage(SVGElementInfo& info, const SVGRect& img_vp);
#endif // SVG_SUPPORT_SVG_IN_IMAGE
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	virtual OP_STATUS HandleForeignObject(SVGElementInfo& info, const SVGRect& img_vp, int quality);
#endif // SVG_SUPPORT_FOREIGNOBJECT
};

//
// SVGRectSelectionObject - traversal object for rectangular
// intersections (used by the DOM intersection APIs)
//
class SVGRectSelectionObject : public SVGIntersectionObject
{
public:
	SVGRectSelectionObject(SVGChildIterator* child_iterator) :
		SVGIntersectionObject(child_iterator) {}

	virtual BOOL AllowElementTraverse(SVGElementInfo& info);
};

//
// SVGBBoxUpdateObject
//
class SVGBBoxUpdateObject : public SVGVisualTraversalObject
{
public:
	SVGBBoxUpdateObject(SVGChildIterator* child_iterator) :
		SVGVisualTraversalObject(child_iterator,
								 TRUE /* ignore pevents */,
								 FALSE /* apply paint */,
								 TRUE /* calculate bbox */) {}

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveElement(SVGElementInfo& info);

	virtual OP_STATUS EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveContainer(SVGElementInfo& info);

	virtual OP_STATUS EnterTextGroup(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextGroup(SVGElementInfo& info);

	virtual OP_STATUS EnterTextNode(SVGElementInfo& info);
	virtual OP_STATUS LeaveTextNode(SVGElementInfo& info);

protected:
	virtual OP_STATUS HandlePaintserver(SVGElementInfo& info, HTML_Element* paintserver, BOOL is_fill,
										BOOL& ignore_fallback) { return OpStatus::OK; }

	virtual OP_STATUS SetupTextAreaElement(SVGElementInfo& info);

	SVGElementContext* GetParentContext(SVGElementInfo& info);
	void Propagate(SVGElementInfo& info);
};

//
// SVGTransformTraversalObject
//
class SVGTransformTraversalObject : public SVGTraversalObject
{
public:
	SVGTransformTraversalObject(SVGChildIterator* child_iterator,
								OpVector<HTML_Element>* element_filter)
		: SVGTraversalObject(child_iterator), m_element_filter(element_filter) {}

	virtual BOOL AllowElementTraverse(SVGElementInfo& info);

	virtual OP_STATUS EnterElement(SVGElementInfo& info, const TransformInfo& tinfo);

	virtual OP_STATUS EnterContainer(SVGElementInfo& info, const TransformInfo& tinfo);
	virtual OP_STATUS LeaveContainer(SVGElementInfo& info) { return OpStatus::OK; }

	virtual OP_STATUS HandleGraphicsElement(SVGElementInfo& info) { return OpStatus::OK; }

	const SVGMatrix& GetCalculatedCTM() const { return m_calculated_ctm; }

protected:
	SVGMatrix m_calculated_ctm;
	OpVector<HTML_Element>* m_element_filter;
};

class TraverseStack;

class SVGTraversalState
{
public:
	SVGTraversalState(SVGTraversalObject* travobj) :
		m_traverse_stack(NULL), m_traversal_object(travobj),
		m_allow_timeout(TRUE), m_continue(FALSE) {}
	~SVGTraversalState();

	OP_STATUS	Init(HTML_Element* root);
	OP_STATUS	RunSlice(LayoutProperties* root_props);

	void		SetAllowTimeout(BOOL allow_timeout) { m_allow_timeout = allow_timeout; }

private:
#ifdef SVG_INTERRUPTABLE_RENDERING
	OP_STATUS	UpdateStackProps();
	void		UnhookStackProps();
#endif // SVG_INTERRUPTABLE_RENDERING

	TraverseStack*		m_traverse_stack;
	SVGTraversalObject* m_traversal_object;
	BOOL				m_allow_timeout;
	BOOL				m_continue;
};

class SVGTraverser
{
	friend class SVGTraversalState;
public:
	static OP_STATUS Traverse(SVGTraversalObject* traversal_object,
							  HTML_Element* traversal_root, LayoutProperties* props);

private:
	SVGTraverser() {}

	static OP_STATUS PushTraversalRoot(TraverseStack& traverse_stack, HTML_Element* traversal_root);
	static OP_STATUS CreateCascade(SVGElementInfo& element_info, LayoutProperties*& props,
								   Head& prop_list, HLDocProfile*& hld_profile);
	static OP_STATUS TraverseElement(SVGTraversalObject* traversal_object,
									 TraverseStack* traverse_stack, BOOL allow_timeout);
};

class SVGSimpleTraversalObject
{
public:
	SVGSimpleTraversalObject(SVGDocumentContext* doc_ctx,
							 SVGElementResolver* parent_resolver = NULL)
		: m_doc_ctx(doc_ctx),
		  m_resolver(parent_resolver),
		  m_owns_resolver(FALSE) {}
	virtual ~SVGSimpleTraversalObject()
	{
		if (m_owns_resolver)
			OP_DELETE(m_resolver);
	}

	OP_STATUS CreateResolver();

	/**
	 * If the children of args.elm should be traversed return TRUE,
	 * otherwise FALSE.  If you wish to restrict what element types
	 * should be entered when traversing the children, return a
	 * nullterminated array of allowed Markup::Type:s.
	 */
	virtual BOOL		AllowChildTraverse(HTML_Element* element) = 0;

	/**
	 * This is called before Enter and can be used to skip subtrees,
	 * but is mainly used to control the loading of CSS. The CSS is
	 * often the most expensive part of the traversal and in case it's
	 * not really needed for this element traversel_needs_css should
	 * be set to FALSE or an error code returned. If an error code is
	 * returned then Leave is the next function to be called and no
	 * children will be visited.
	 */
	virtual OP_STATUS	CSSAndEnterCheck(HTML_Element* element, BOOL& traversal_needs_css) = 0;

	/**
	 * Called when an element is entered. If an error code is returned
	 * then Leave is the next function to be called and no children
	 * will be visited.
	 */
	virtual OP_STATUS	Enter(HTML_Element* element) = 0;

	/**
	 * Called after successful enter of element, before any children
	 * are visited.
	 */
	virtual OP_STATUS	DoContent(HTML_Element* element) = 0;

	/**
	 * Called when an element is left, regardless of the status from  
	 * CSSAndEnterCheck, Enter or DoContent.
	 */
	virtual OP_STATUS	Leave(HTML_Element* element) = 0;

protected:
	SVGDocumentContext*			m_doc_ctx;
	SVGElementResolver*			m_resolver;
	BOOL						m_owns_resolver;
};

class SVGSimpleTraverser
{
public:
	static OP_STATUS TraverseElement(SVGSimpleTraversalObject* traversal_object, HTML_Element* element);

private:
	SVGSimpleTraverser() {}
};

#endif // SVG_SUPPORT
#endif // SVG_TRAVERSER_H
