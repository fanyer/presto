/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SVGPAINTNODE_H
#define SVGPAINTNODE_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_number.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/svgpainter.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGFilter.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGRect.h"

// For SVGTextCache
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/src/SVGTextTraverse.h"

#include "modules/util/simset.h"

// For SVGImageNode
#include "modules/url/url2.h"

class HTML_Element;
class FramesDocument;

class OpBpath;
class SVGVector;
class SVGCanvasState;
class SVGMediaBinding;

class SVGRenderer;
class SVGRenderBuffer;

class SVGClipPathNode;
class SVGMaskNode;

class PaintPropertySet
{
public:
	PaintPropertySet() : m_bits(0) {}

	BOOL IsSet(unsigned bit) { return (m_bits & (1u << bit)) != 0; }
	void Set(unsigned bit) { m_bits |= (1u << bit); }

private:
	unsigned m_bits;
};

enum
{
	PAINTPROP_TRANSFORM,
	PAINTPROP_OPACITY,
	PAINTPROP_CLIPPATH
};

class SVGPaintNode : public ListElement<SVGPaintNode>
{
public:
	// Your average 'dynamic type determination' type. The count needs
	// to be matched up with the amount of bits for the node_type
	// field
	enum NodeType
	{
		COMPOSITE_NODE,
		FOREIGNOBJECT_NODE,
		PRIMITIVE_NODE,
		POLYGON_NODE,
		PATH_NODE,
		IMAGE_NODE,
		VIDEO_NODE,
		TEXT_NODE
	};

	SVGPaintNode(NodeType ntype) :
		parent(NULL),
#ifdef SVG_SUPPORT_STENCIL
		clippath(NULL),
		mask(NULL),
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
		filter(NULL),
#endif // SVG_SUPPORT_FILTERS
		buffer_data(0),
		opacity(255),
		node_type(ntype),
		has_valid_extents(FALSE),
		can_skip_layer(FALSE),
		visible(TRUE),
		has_clean_br_state(FALSE),
		has_markers(FALSE),
		ref_transform(FALSE) {}
	virtual ~SVGPaintNode() { Detach(); Reset(); }
	virtual void Free() { OP_DELETE(this); }

	NodeType Type() const { return static_cast<NodeType>(node_type); }

	/**
	 * Entrypoint for painting. Handles shared properties (clip, mask,
	 * opacity et.c), buffering, filters and applies the userspace
	 * transform then calls PaintContent() to paint a nodes actual
	 * content. When PaintContent() has returned, resources allocated
	 * for this node are cleaned up.
	 * Override this if something needs to be done for example before
	 * userspace for the node is setup.
	 */
	virtual OP_STATUS Paint(SVGPainter* painter);

	/**
	 * Hook for painting the actual content of the node. Needs to
	 * setup state for the painter and paint any primitives or handle
	 * children (@see SVGCompositePaintNode).
	 */
	virtual void PaintContent(SVGPainter* painter) = 0;

#ifdef SVG_SUPPORT_STENCIL
	/**
	 * Entrypoint for generation of clipping geometry
	 * (clipPath:s). Like Paint() it handles shared properties (clips)
	 * and applies the userspace transform. Calls ClipContent() to
	 * perform the actual work of emitting geometry, and cleans up
	 * afterwards.
	 */
	OP_STATUS Clip(SVGPainter* painter);

	/**
	 * Hook for emitting content for clipping. Only needs to be
	 * implemented on nodes that can take part in clipPaths.
	 */
	virtual void ClipContent(SVGPainter* painter) {}
#endif // SVG_SUPPORT_STENCIL

	/**
	 * Update the state of the paint node using the SVGCanvasState
	 */
	virtual void UpdateState(SVGCanvasState* state) {}

	/**
	 * Determine the extents of this paint node in its userspace
	 */
	virtual void GetLocalExtents(SVGBoundingBox& extents) = 0;

	/**
	 * Like GetLocalExtents, but also included any influence from a
	 * filter that is applied to the node
	 */
	void GetLocalExtentsWithFilter(SVGBoundingBox& extents);

	/**
	 * Get the bounding box (as used by paints et.c) of the paint node
	 */
	virtual SVGBoundingBox GetBBox() = 0;

	/**
	 * Get any additional transform that should be applied after the
	 * userspace transform (ustransform) but before the setting up a
	 * clipPath or mask.
	 */
	virtual void GetAdditionalTransform(SVGMatrix& addmtx) { addmtx.LoadIdentity(); }

	struct NodeContext
	{
		NodeContext() : has_filter(FALSE) {}

		SVGMatrix transform;
		OpRect clip;
		BOOL has_filter;
		SVGMatrix root_transform;
	};

	OP_STATUS GetParentContext(NodeContext& context, BOOL invalidate_extents);
	virtual void GetNodeContext(NodeContext& context, BOOL for_child = FALSE);
	OpRect GetTransformedExtents(const SVGMatrix& parent_transform);

	virtual BOOL HasNonscalingStroke() { return FALSE; }

	OpRect GetPixelExtentsWithFilter(const NodeContext& context);
	OpRect GetPixelExtents(const NodeContext& context);
	OpRect GetPixelExtents();

	BOOL Update(SVGRenderer* renderer);
	OP_STATUS UpdateTransform(SVGRenderer* renderer, const SVGMatrix* transform, OpRect& update_extents);

	virtual void SetTransform(const SVGMatrix& ctm) { ustransform = ctm; }
	void SetRefTransform(BOOL is_ref_transform) { ref_transform = !!is_ref_transform; }
	BOOL HasRefTransform() const { return !!ref_transform; }
	void ResetTransform() { ustransform.LoadIdentity(); ref_transform = FALSE; }
	const SVGMatrix& GetTransform() const { return ustransform; }
	SVGMatrix GetRefTransformToParent();
	SVGMatrix GetHostInverseTransform();

	/** Indicate that any cached extents should no longer be considered valid */
	void MarkExtentsDirty() { has_valid_extents = FALSE; }

	void SetOpacity(UINT8 gopacity) { opacity = gopacity; }
	void SetVisible(BOOL in_visible) { visible = !!in_visible; }
	void SetParent(SVGPaintNode* p) { parent = p; } // Maybe SVGCompositePaintNode instead ?

#ifdef SVG_SUPPORT_STENCIL
	void SetClipPath(SVGClipPathNode* clipnode) { clippath = clipnode; }
	BOOL HasClipPath() const { return !!clippath; }
	void SetMask(SVGMaskNode* masknode) { mask = masknode; }
	SVGMaskNode* GetMask() { return mask; }
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_FILTERS
	void SetFilter(SVGFilter* in_filter) { filter = in_filter; }
	BOOL AffectsFilter() const;
#endif // SVG_SUPPORT_FILTERS

	void SetBufferKey(void* key)
	{
		OP_ASSERT((reinterpret_cast<UINTPTR>(key) & BUFFER_FLAGS_MASK) == 0);
		buffer_data &= BUFFER_FLAGS_MASK;
		buffer_data |= reinterpret_cast<UINTPTR>(key);
	}
	void ValidateBuffered()
	{
		SVGCache* scache = g_svg_manager_impl->GetCache();
		if (scache->Get(SVGCache::SURFACE, GetBufferKey()))
			SetBufferPresent(TRUE);
	}
	BOOL IsBuffered() const
	{
		return buffer_data > BUFFER_FLAGS_MASK && !(buffer_data & BUFFER_DISABLED);
	}
	void SetBufferDirty() { has_clean_br_state = 0; }
	void ResetBuffering() { buffer_data = 0; }
	void CopyBufferData(SVGPaintNode* other_node)
	{
		buffer_data = other_node->buffer_data;
	}

	SVGPaintNode* GetParent() const { return parent; }
	UINT8 GetOpacity() const { return opacity; }
	BOOL IsVisible() const { return !!visible; }

	void Reset();

	void Detach()
	{
		Out();
		parent = NULL;
	}

protected:
	enum
	{
		BUFFER_DISABLED		= 0x1,
		BUFFER_PRESENT		= 0x2,
		BUFFER_FLAGS_MASK	= 0x3
	};

	void* GetBufferKey() const
	{
		return reinterpret_cast<void*>(buffer_data & ~(UINTPTR)BUFFER_FLAGS_MASK);
	}
	void SetBufferPresent(BOOL present)
	{
		if (present)
			buffer_data |= BUFFER_PRESENT;
		else
			buffer_data &= ~(UINTPTR)BUFFER_PRESENT;
	}
	BOOL IsBufferPresent() const { return (buffer_data & BUFFER_PRESENT) != 0; }
	void DisableBuffering() { buffer_data |= BUFFER_DISABLED; }
	OP_STATUS UpdateBuffer(SVGPainter* painter, SVGRenderBuffer* render_buffer);
	BOOL PaintBuffered(SVGPainter* painter);

	OP_STATUS PaintContentWithFilter(SVGPainter* painter);

	OP_BOOLEAN ApplyProperties(SVGPainter* painter, PaintPropertySet& propset);
	void UnapplyProperties(SVGPainter* painter, PaintPropertySet& propset);

#ifdef SVG_SUPPORT_STENCIL
	OP_STATUS ApplyClipProperties(SVGPainter* painter, PaintPropertySet& propset);
	void UnapplyClipProperties(SVGPainter* painter, PaintPropertySet& propset);
#endif // SVG_SUPPORT_STENCIL

	BOOL HasFilter() const
	{
#ifdef SVG_SUPPORT_FILTERS
		return !!filter;
#else
		return FALSE;
#endif // SVG_SUPPORT_FILTERS
	}

	BOOL AddPixelExtents(const NodeContext& parent_context, SVGRenderer* renderer,
						 OpRect* update_extents = NULL);

	void DrawImagePlaceholder(SVGPainter* painter, const SVGRect& img_rect);

	SVGPaintNode* parent;
	SVGMatrix ustransform;
#ifdef SVG_SUPPORT_STENCIL
	SVGClipPathNode* clippath;
	SVGMaskNode* mask;
#endif // SVG_SUPPORT_STENCIL
#ifdef SVG_SUPPORT_FILTERS
	SVGFilter* filter;
#endif // SVG_SUPPORT_FILTERS
	UINTPTR buffer_data;
	unsigned opacity:8;
	// Flags
	unsigned node_type:3;
	unsigned has_valid_extents:1;
	unsigned can_skip_layer:1;
	unsigned visible:1;
	unsigned has_clean_br_state:1;
	unsigned has_markers:1;
	unsigned ref_transform:1;
};

/** Node for collection of nodes (groups such as <g>) */
class SVGCompositePaintNode : public SVGPaintNode
{
public:
	SVGCompositePaintNode() :
		SVGPaintNode(COMPOSITE_NODE), has_bglayer(FALSE), new_bglayer(FALSE), has_valid_nsstroke(FALSE) {}
	virtual ~SVGCompositePaintNode() { Clear(); }
	virtual void Free();

#ifdef SVG_SUPPORT_FILTERS
	/* Overload to handle enable-background */
	virtual OP_STATUS Paint(SVGPainter* painter);
#endif // SVG_SUPPORT_FILTERS

	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	virtual BOOL HasNonscalingStroke();

	void Insert(SVGPaintNode* node, SVGPaintNode* pred);
	void Clear();

	void FreeChildren();

	void InvalidateHasNonscalingStrokeCache() { has_valid_nsstroke = FALSE; }

	void SetHasBackgroundLayer(BOOL has_bg, BOOL new_bg)
	{
		has_bglayer = !!has_bg;
		new_bglayer = !!new_bg;
	}

protected:
	SVGBoundingBox cached_extents;
	List<SVGPaintNode> children;
	unsigned has_bglayer:1;
	unsigned new_bglayer:1;
	unsigned cached_nsstroke:1;
	unsigned has_valid_nsstroke:1;
};

/** Node for collection of nodes which has an additional offset (<use>) */
class SVGOffsetCompositePaintNode : public SVGCompositePaintNode
{
public:
	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	virtual void GetAdditionalTransform(SVGMatrix& addmtx);

	virtual void GetNodeContext(NodeContext& context, BOOL for_child);

	void SetOffset(const SVGNumberPair& ofs) { offset = ofs; }

protected:
	SVGNumberPair offset;
};

/** Node for collection of nodes which has a rectangular clip (<svg>, used <symbol>) */
class SVGViewportCompositePaintNode : public SVGCompositePaintNode
{
public:
	SVGViewportCompositePaintNode()
		: scale(1), cliprect_enabled(FALSE) { fillpaint.Reset(); }

	virtual OP_STATUS Paint(SVGPainter* painter);

	virtual void GetNodeContext(NodeContext& context, BOOL for_child);

	void SetClipRect(const SVGRect& clip) { cliprect = clip; }
	void UseClipRect(BOOL use_cliprect) { cliprect_enabled = use_cliprect; }

	void SetFillColor(UINT32 color) { fillpaint.color = color; }
	void SetFillOpacity(UINT8 opacity) { fillpaint.opacity = opacity; }

	void SetScale(SVGNumber vpscale) { scale = vpscale; }
	void SetUserTransform(const SVGMatrix& user_mtx) { user_transform = user_mtx; }

protected:
	SVGMatrix user_transform;
	SVGRect cliprect;
	SVGPaintDesc fillpaint;
	SVGNumber scale;
	BOOL cliprect_enabled;
};

#ifdef SVG_SUPPORT_STENCIL
/** Node for clipPaths */
class SVGClipPathNode : public SVGCompositePaintNode
{
public:
	SVGClipPathNode() : is_rectangle(FALSE), is_bbox_relative(FALSE) {}

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	OP_STATUS Apply(SVGPainter* painter, SVGPaintNode* context_node);

	void Optimize();

	void SetBBoxRelative(BOOL is_relative) { is_bbox_relative = is_relative; }

protected:
	void SetIsRectangle(BOOL is_rect) { is_rectangle = is_rect; }
	void SetRectangle(const SVGRect& cr) { cliprect = cr; }

	SVGRect cliprect;
	BOOL is_rectangle;
	BOOL is_bbox_relative;
};

/** Node for masks */
class SVGMaskNode : public SVGCompositePaintNode
{
public:
	SVGMaskNode() : is_bbox_relative(FALSE) {}

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	OP_STATUS Apply(SVGPainter* painter, SVGPaintNode* context_node);

	void SetBBoxRelative(BOOL is_relative) { is_bbox_relative = is_relative; }
	void SetClipRect(const SVGRect& cr) { cliprect = cr; }

protected:
	SVGRect cliprect;
	BOOL is_bbox_relative;
};
#endif // SVG_SUPPORT_STENCIL

class SVGMarkerNode;
class SVGMarkerPosIterator;

/** Intermediate node type carrying paint properties */
class SVGGeometryNode : public SVGPaintNode
{
public:
	SVGGeometryNode(NodeType ntype) : SVGPaintNode(ntype)
	{
		paints[0].Reset();
		paints[1].Reset();
#ifdef SVG_SUPPORT_MARKERS
		markers[0] = NULL;
		markers[1] = NULL;
		markers[2] = NULL;
#endif // SVG_SUPPORT_MARKERS
	}
	virtual ~SVGGeometryNode();

	virtual void UpdateState(SVGCanvasState* state);
	virtual BOOL IsStrokePaintServerAllowed() { return TRUE; }
	virtual BOOL HasNonscalingStroke() { return stroke_props.non_scaling; }

#ifdef SVG_SUPPORT_MARKERS
	void PaintMarkers(SVGPainter* painter, SVGMarkerPosIterator *iter);
	void SetMarkers(SVGMarkerNode* start, SVGMarkerNode* middle, SVGMarkerNode* end);
	void ResetMarkers();
#endif // SVG_SUPPORT_MARKERS

	void SetStrokeProperties(const SVGStrokeProperties& sprops) { stroke_props = sprops; }
	void SetObjectProperties(const SVGObjectProperties& oprops) { obj_props = oprops; }
	void SetPaint(unsigned pindex, const SVGPaintDesc& pdesc) { paints[pindex] = pdesc; }

	void TransferObjectProperties(SVGPainter* painter);
protected:
	SVGStrokeProperties stroke_props;
	SVGObjectProperties obj_props;
	SVGPaintDesc paints[2];
#ifdef SVG_SUPPORT_MARKERS
	SVGMarkerNode* markers[3];
#endif // SVG_SUPPORT_MARKERS
};

/** Node for primitives (geometry easily defined) */
class SVGPrimitiveNode : public SVGGeometryNode
{
public:
	SVGPrimitiveNode() : SVGGeometryNode(PRIMITIVE_NODE), type(INVALID) {}

	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual void UpdateState(SVGCanvasState* state);

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	virtual BOOL IsStrokePaintServerAllowed()
	{
		if (type != LINE)
		{
			return TRUE;
		}
		else
		{
			SVGBoundingBox bbox = GetBBox();
			return (bbox.Width() > 0 && bbox.Height() > 0);
		}
	}

	enum Type
	{
		INVALID,
		RECTANGLE,
		ELLIPSE,
		LINE
	};

	void SetRectangle(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h, SVGNumber rx, SVGNumber ry)
	{
		data[0] = x, data[1] = y, data[2] = w, data[3] = h, data[4] = rx, data[5] = ry;
		type = RECTANGLE;
	}
	void SetEllipse(SVGNumber cx, SVGNumber cy, SVGNumber rx, SVGNumber ry)
	{
		data[0] = cx, data[1] = cy, data[2] = rx, data[3] = ry;
		type = ELLIPSE;
	}
	void SetLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2)
	{
		data[0] = x1, data[1] = y1, data[2] = x2, data[3] = y2;
		type = LINE;
	}

	BOOL IsRectangle() const { return type == RECTANGLE && (data[4] <= 0 && data[5] <= 0); }
	SVGRect GetRectangle() const { return SVGRect(data[0], data[1], data[2], data[3]); }

protected:
	OP_STATUS PaintPrimitive(SVGPainter* painter);

	SVGNumber data[6];	/* rectangle		{ x, y, w, h, rx, ry } */
						/* circle / ellipse	{ cx, cy, rx, ry } */
						/* line				{ x1, y1, x2, y2 } */
	Type type;
	// SVGMarker...* markers; /* for 'line' */
};

/** Node for paths */
class SVGPathNode : public SVGGeometryNode
{
public:
	SVGPathNode() : SVGGeometryNode(PATH_NODE), path(NULL), pathlength(-1) {}
	virtual ~SVGPathNode() { SVGObject::DecRef(path); }

	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual BOOL IsStrokePaintServerAllowed()
	{
		SVGBoundingBox bbox = GetBBox();
		return (bbox.Width() > 0 && bbox.Height() > 0);
	}

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	void SetPath(OpBpath* p)
	{
		SVGObject::IncRef(p);
		SVGObject::DecRef(path);
		path = p;
		has_valid_extents = FALSE;
	}
	void SetPathLength(SVGNumber plen)
	{
		pathlength = plen;
	}

protected:
	SVGBoundingBox cached_bbox;
	OpBpath* path;
	SVGNumber pathlength;
	// SVGMarker...* markers;
};

/** Node for polygons */
class SVGPolygonNode : public SVGGeometryNode
{
public:
	SVGPolygonNode() : SVGGeometryNode(POLYGON_NODE), pointlist(NULL), is_closed(FALSE) {}
	virtual ~SVGPolygonNode() { SVGObject::DecRef(pointlist); }

	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	void SetPointList(SVGVector* ptlist)
	{
		SVGObject::IncRef(ptlist);
		SVGObject::DecRef(pointlist);
		pointlist = ptlist;
		has_valid_extents = FALSE;
	}
	void SetIsClosed(BOOL closed)
	{
		is_closed = closed;
	}

protected:
	SVGBoundingBox cached_bbox;
	SVGVector* pointlist;
	BOOL is_closed;
	// SVGMarker...* markers;
};

/** Node for carrying text attributes */
class SVGTextAttributePaintNode : public SVGCompositePaintNode
{
public:
	TextAttributes* GetAttributes() { return &m_attributes; }
	virtual TextPathAttributes* GetTextPathAttributes() { return NULL; }

	void ForceExtents(const SVGBoundingBox& forced_extents)
	{
		cached_extents = forced_extents;
		has_valid_extents = TRUE;
	}

protected:
	TextAttributes m_attributes;
};

class SVGTextPainter;

/** Structure representing a run of text */
struct TextRun
{
	TextRun() { Reset(); }

	void Reset()
	{
		next = NULL;
		start = length = 0;
		start_offset = end_offset = 0;
		is_visible = 1;
		has_ellipsis = 0;
	}
	TextRun* Suc() { return next; }

	TextRun* next;

	// The starting CTP
	SVGNumberPair position;

	// The run stretches from 'start' to 'start+length-1' in the
	// fragments array associated with the content paint node
	UINT16 start;
	UINT16 length;

	// The run starts at offset 'start_offset' in the 'start'
	// fragment, and ends at offset 'end_offset' in the runs last
	// ('start+length-1') fragment. If 'end_offset' is 0, then the
	// entire (possibly remainder of) ending fragment is used
	UINT16 start_offset;
	UINT16 end_offset;

	// Run flags
	unsigned is_visible:1;
	unsigned has_ellipsis:1;
};

class TextRunList
{
public:
	TextRunList() : first(NULL), last(NULL) {}

	TextRun* First() { return first; }
	TextRun* Last() { return last; }
	void Append(TextRun* run);
	void Clear();

private:
	TextRun* first;
	TextRun* last;
};

class SVGTextRotate {
protected:
	SVGTextRotate() : m_rotation_list(NULL) {}

	GlyphRotationList* m_rotation_list;
public:
	virtual ~SVGTextRotate() { OP_DELETE(m_rotation_list); }
	void SetRotationList(GlyphRotationList* rot_list) {
		OP_DELETE(m_rotation_list);
		m_rotation_list = rot_list;
	}
	GlyphRotationList* GetRotationList() { return m_rotation_list; }

	virtual unsigned GetEstimatedSize() = 0;
};

/** Node for carrying actual text content */
class SVGTextContentPaintNode : public SVGGeometryNode, public SVGTextRotate
{
public:
	SVGTextContentPaintNode() :
		SVGGeometryNode(TEXT_NODE),
		SVGTextRotate(),
		m_attributes(NULL),
		m_textpath_attributes(NULL),
		m_textselection_doc_ctx(NULL),
		m_textselection_element(NULL),
		m_text_cache(NULL),
		m_editable(NULL),
		m_node_text_format(0) {}
	virtual ~SVGTextContentPaintNode() { m_runs.Clear(); }

	virtual void UpdateState(SVGCanvasState* state);

	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	void AppendRun(TextRun* textrun) { m_runs.Append(textrun); }

	void UpdateRunBBox(const SVGBoundingBox& run_bbox);

	void SetTextCache(SVGTextCache* text_cache) { m_text_cache = text_cache; }
	void SetAttributes(TextAttributes* text_attributes) { m_attributes = text_attributes; }
	void SetTextPathAttributes(TextPathAttributes* text_path_attributes) { m_textpath_attributes = text_path_attributes; }
	void SetEditable(SVGEditable* editable) { m_editable = editable; }
	void SetTextSelectionDocument(SVGDocumentContext* textselection_doc_ctx) { m_textselection_doc_ctx = textselection_doc_ctx; }
	void SetTextSelectionElement(HTML_Element* textselection_element) { m_textselection_element = textselection_element; }
	void SetFontDocument(SVGDocumentContext* font_doc_ctx) { m_font_doc_ctx = font_doc_ctx; }
	void SetTextFormat(int text_format) { m_node_text_format = text_format; }

	void ResetRuns() { m_stored_bbox.Clear(); m_runs.Clear(); }

	/** SVGTextRotate methods */
	virtual unsigned GetEstimatedSize();

protected:
	void PaintRuns(SVGTextPainter& text_painter);
	OP_STATUS PaintFragment(SVGTextPainter* text_painter, const uni_char* text,
							const WordInfo* fragment, int format);

	SVGBoundingBox m_stored_bbox;
	TextRunList m_runs;
	TextAttributes* m_attributes;
	TextPathAttributes* m_textpath_attributes;
	SVGDocumentContext* m_textselection_doc_ctx;
	HTML_Element* m_textselection_element;
	SVGDocumentContext* m_font_doc_ctx; // Put this in TextAttributes?
	SVGTextCache* m_text_cache;
	SVGEditable* m_editable;
	int m_node_text_format;
};

class SVGTextPathNode : public SVGTextAttributePaintNode
{
public:
	virtual TextPathAttributes* GetTextPathAttributes() { return &m_textpath_attributes; }

protected:
	TextPathAttributes m_textpath_attributes;
};

class SVGTextReferencePaintNode : public SVGTextAttributePaintNode
{
public:
	virtual void Free();
	virtual void UpdateState(SVGCanvasState* state) { m_content_node.UpdateState(state); }

	SVGTextContentPaintNode* GetContentNode() { return &m_content_node; }

protected:
	SVGTextContentPaintNode m_content_node;
};

class SVGAltGlyphNode : public SVGTextAttributePaintNode, public SVGTextRotate
{
public:
	SVGAltGlyphNode() :
		SVGTextRotate(),
		m_textpath_attributes(NULL),
		m_glyphs(NULL),
		m_font_doc_ctx(NULL),
		m_textselection_doc_ctx(NULL),
		m_textselection_element(NULL),
		m_editable(NULL)
	{
		paints[0].Reset();
		paints[1].Reset();
	}
	virtual ~SVGAltGlyphNode();

	virtual void UpdateState(SVGCanvasState* state);

	virtual void PaintContent(SVGPainter* painter);
#ifdef SVG_SUPPORT_STENCIL
	virtual void ClipContent(SVGPainter* painter);
#endif // SVG_SUPPORT_STENCIL

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();
	virtual BOOL HasNonscalingStroke() { return stroke_props.non_scaling; }

	/** SVGTextRotate methods */
	virtual unsigned GetEstimatedSize();

	void SetTextPathAttributes(TextPathAttributes* text_path_attributes) { m_textpath_attributes = text_path_attributes; }
	void SetFontDocument(SVGDocumentContext* font_doc_ctx) { m_font_doc_ctx = font_doc_ctx; }
	void SetGlyphVector(OpVector<GlyphInfo>* glyphs) { m_glyphs = glyphs; }
	void SetPosition(const SVGNumberPair& pos) { m_position = pos; }
	void SetBBox(const SVGBoundingBox& bbox) { m_stored_bbox = bbox; }
	void SetEditable(SVGEditable* editable) { m_editable = editable; }
	void SetTextSelectionDocument(SVGDocumentContext* textselection_doc_ctx) { m_textselection_doc_ctx = textselection_doc_ctx; }
	void SetTextSelectionElement(HTML_Element* textselection_element) { m_textselection_element = textselection_element; }

protected:
	void PaintGlyphs(SVGPainter* painter);

	SVGBoundingBox m_stored_bbox;
	SVGNumberPair m_position;
	TextPathAttributes* m_textpath_attributes;
	OpVector<GlyphInfo>* m_glyphs;
	SVGDocumentContext* m_font_doc_ctx;
	SVGDocumentContext* m_textselection_doc_ctx;
	HTML_Element* m_textselection_element;
	SVGEditable* m_editable;

	SVGStrokeProperties stroke_props;
	SVGObjectProperties obj_props;
	SVGPaintDesc paints[2];
};

/** Node for raster images */
class SVGRasterImageNode : public SVGPaintNode
{
public:
	SVGRasterImageNode() : SVGPaintNode(IMAGE_NODE), aspect(NULL) { background.Reset(); }
	virtual ~SVGRasterImageNode() { SVGObject::DecRef(aspect); }

	virtual void PaintContent(SVGPainter* painter);

	virtual void UpdateState(SVGCanvasState* state);

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	void SetImageViewport(const SVGRect& vprect)
	{
		viewport = vprect;
	}
	void SetImageAspect(SVGAspectRatio* ar);
	void SetImageQuality(SVGPainter::ImageRenderQuality img_quality)
	{
		quality = img_quality;
	}
	void SetImageContext(const URL& img_url, HTML_Element* img_elm, FramesDocument* doc)
	{
		image_url = img_url;
		image_element = img_elm;
		frames_doc = doc;
	}
	void SetBackgroundColor(UINT32 color) { background.color = color; }
	void SetBackgroundOpacity(UINT8 opacity) { background.opacity = opacity; }

protected:
	OP_STATUS DrawImageFromURL(SVGPainter* painter);

	SVGRect viewport;
	SVGPaintDesc background;
	URL image_url;
	HTML_Element* image_element;
	FramesDocument* frames_doc;
	SVGAspectRatio* aspect;
	SVGPainter::ImageRenderQuality quality;
};

/** Node for scalable images */
class SVGVectorImageNode : public SVGCompositePaintNode
{
public:
	SVGVectorImageNode() : aspect(NULL) { background.Reset(); }
	virtual ~SVGVectorImageNode();

	virtual void SetTransform(const SVGMatrix& ctm)
	{
		ustransform = ctm;
		ustransform.PostMultTranslation(viewport.x, viewport.y);
	}

	virtual void PaintContent(SVGPainter* painter);

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	void SetImageViewport(const SVGRect& vprect) { viewport = vprect; }
	void SetImageAspect(SVGAspectRatio* ar);
	void SetImageQuality(SVGPainter::ImageRenderQuality img_quality)
	{
		quality = img_quality;
	}
	void SetImageContext(SVGDocumentContext* external_doc_ctx)
	{
		this->external_doc_ctx = external_doc_ctx;
	}
	void SetBackgroundColor(UINT32 color) { background.color = color; }
	void SetBackgroundOpacity(UINT8 opacity) { background.opacity = opacity; }

protected:
	SVGRect viewport;
	SVGPaintDesc background;
	SVGDocumentContext* external_doc_ctx;
	SVGAspectRatio* aspect;
	SVGPainter::ImageRenderQuality quality;
};

#ifdef SVG_SUPPORT_FOREIGNOBJECT
/** Node for foreignObject without xlink:href */
class SVGForeignObjectNode : public SVGPaintNode
{
public:
	SVGForeignObjectNode() :
		SVGPaintNode(FOREIGNOBJECT_NODE),
		foreign_object_element(NULL),
		frames_doc(NULL),
		vis_dev(NULL),
		quality(SVGPainter::IMAGE_QUALITY_NORMAL)
	{
		background.Reset();
	}

	virtual void PaintContent(SVGPainter* painter);

	virtual void GetLocalExtents(SVGBoundingBox& extents);
	virtual void UpdateState(SVGCanvasState* state);

	virtual SVGBoundingBox GetBBox();

	void SetContext(HTML_Element* foreign_object_element, FramesDocument* frames_doc, VisualDevice* vis_dev)
	{
		this->foreign_object_element = foreign_object_element;
		this->frames_doc = frames_doc;
		this->vis_dev = vis_dev;
	}
	void SetViewport(const SVGRect& vprect)
	{
		viewport = vprect;
	}
	void SetQuality(SVGPainter::ImageRenderQuality img_quality)
	{
		quality = img_quality;
	}
	void SetBackgroundColor(UINT32 color) { background.color = color; }
	void SetBackgroundOpacity(UINT8 opacity) { background.opacity = opacity; }

protected:
	SVGRect 		viewport;
	SVGPaintDesc 	background;
	HTML_Element*	foreign_object_element;
	FramesDocument*	frames_doc;
	VisualDevice* 	vis_dev;
	SVGPainter::ImageRenderQuality quality;
};

/** Node for foreignObject with xlink:href */
class SVGForeignObjectHrefNode : public SVGForeignObjectNode
{
public:
	SVGForeignObjectHrefNode() : frame(NULL) {}

	virtual void PaintContent(SVGPainter* painter);

	void SetFrame(FramesDocElm* frame)
	{
		this->frame = frame;
	}

protected:
	FramesDocElm* frame;
};
#endif // SVG_SUPPORT_FOREIGNOBJECT

#ifdef SVG_SUPPORT_MEDIA
/** Node for video */
class SVGVideoNode : public SVGPaintNode
{
public:
	SVGVideoNode() :
		SVGPaintNode(VIDEO_NODE),
		aspect(NULL),
#ifdef PI_VIDEO_LAYER
		allow_overlay(TRUE),
#endif // PI_VIDEO_LAYER
		is_pinned(FALSE),
		pinned_rotation(0)
	{
		background.Reset();
	}
	virtual ~SVGVideoNode() { SVGObject::DecRef(aspect); }

	virtual void PaintContent(SVGPainter* painter);

	virtual void GetLocalExtents(SVGBoundingBox& extents);

	virtual SVGBoundingBox GetBBox();

	void SetVideoViewport(const SVGRect& vprect)
	{
		viewport = vprect;
	}
	void SetVideoAspect(SVGAspectRatio* ar);
	void SetVideoQuality(SVGPainter::ImageRenderQuality img_quality)
	{
		quality = img_quality;
	}
	void SetBinding(SVGMediaBinding* mb) { mbind = mb; }
	void SetIsPinned(BOOL video_is_pinned) { is_pinned = !!video_is_pinned; }
	void SetPinnedExtents(const SVGBoundingBox& pinned_exts) { pinned_extents = pinned_exts; }
	/**
	 * Set the rotation for a 'pinned' <video> as a multiple of 90
	 * (n*90) degrees - 0-3 is the valid range.
	 */
	void SetPinnedRotation(unsigned rotation90) { OP_ASSERT(rotation90 <= 3); pinned_rotation = rotation90; }
#ifdef PI_VIDEO_LAYER
	void SetAllowOverlay(BOOL can_use_overlay) { allow_overlay = !!can_use_overlay; }
#endif // PI_VIDEO_LAYER
	void SetBackgroundColor(UINT32 color) { background.color = color; }
	void SetBackgroundOpacity(UINT8 opacity) { background.opacity = opacity; }

protected:
#ifdef PI_VIDEO_LAYER
	BOOL CheckOverlay(SVGPainter* painter, const SVGRect& video_rect);
#else
	BOOL CheckOverlay(SVGPainter* painter, const SVGRect& video_rect) { return FALSE; }
#endif // PI_VIDEO_LAYER

	SVGRect viewport;
	SVGBoundingBox pinned_extents;
	SVGPaintDesc background;
	SVGMediaBinding* mbind;
	SVGAspectRatio* aspect;
#ifdef PI_VIDEO_LAYER
	unsigned allow_overlay:1;
#endif // PI_VIDEO_LAYER
	unsigned is_pinned:1;
	unsigned pinned_rotation:2; ///< Rotation in 90 degree units
	SVGPainter::ImageRenderQuality quality;
};
#endif // SVG_SUPPORT_MEDIA

#ifdef SVG_SUPPORT_MARKERS
class SVGMarkerNode : public SVGCompositePaintNode
{
public:
	SVGMarkerNode() : is_orient_auto(FALSE), cliprect_enabled(FALSE) {}

	void GetLocalMarkerExtents(SVGNumber current_slope, const SVGNumberPair& marker_pos, SVGBoundingBox& extents);
	OP_STATUS Apply(SVGPainter* painter, SVGNumber current_slope, const SVGNumberPair& pos);

	BOOL NeedsSlope() { return is_orient_auto; }
	void SetBaseTransform(const SVGMatrix& base_xfrm) { m_marker_base_xfrm.Copy(base_xfrm); }
	void SetOrientIsAuto(BOOL orient) { is_orient_auto = orient; }
	void SetAngle(const SVGNumber& angle) { m_angle = angle; }
	void SetClipRect(const SVGRect& clip) { cliprect = clip; }
	void UseClipRect(BOOL use_cliprect) { cliprect_enabled = use_cliprect; }

protected:
	void PositionAndOrient(SVGMatrix& xfrm, SVGNumber current_slope, const SVGNumberPair& marker_pos);

	SVGRect cliprect;
	SVGMatrix m_marker_base_xfrm;
	SVGNumber m_angle;
	unsigned is_orient_auto : 1;
	unsigned cliprect_enabled : 1;
};
#endif // SVG_SUPPORT_MARKERS

#endif // SVG_SUPPORT
#endif // SVGPAINTNODE_H
