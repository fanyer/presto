/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_CANVAS_STATE_H
#define SVG_CANVAS_STATE_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGInternalEnum.h"

struct SVGPaintDesc;
class SVGPaintServer;

class HTML_Element;
class HTMLayoutProperties;

enum SVGAllowPointerEvents
{
	SVGALLOWPOINTEREVENTS_FILL   = 0x1,
	SVGALLOWPOINTEREVENTS_STROKE = 0x2
};

class SVGResolvedDashArray
{
public:
	SVGResolvedDashArray() : dashes(NULL), dash_size(0), refcount(1) {}

	static void IncRef(SVGResolvedDashArray* da)
	{
		if (da)
			da->refcount++;
	}
	static void DecRef(SVGResolvedDashArray* da)
	{
		if (da && --da->refcount == 0)
			OP_DELETE(da);
	}
	static void AssignRef(SVGResolvedDashArray*& p, SVGResolvedDashArray* q)
	{
		if (p == q)
			return;

		DecRef(p);
		IncRef(p = q);
	}

	SVGNumber* dashes;
	unsigned dash_size;

private:
	~SVGResolvedDashArray() { OP_DELETEA(dashes); }

	int refcount;
};

/**
 * This class keeps track of all SVGCanvas properties.
 *
 * SaveState and RestoreState should be balanced. An unbalanced
 * call will return an error.
 */

class SVGCanvasState
{
protected:
	SVGMatrix	 	m_transform;			///< The current transform matrix
	int 			m_use_fill;				///< Fill type (paintserver)
	UINT32			m_fill_seq;				///< The sequence number of the fill
	int 			m_use_stroke;			///< Stroke type (paintserver)
	UINT32			m_stroke_seq;			///< The sequence number of the stroke
	UINT32 			m_fillcolor;			///< The fill color (RGBA)
	UINT32 			m_strokecolor;			///< The stroke color (RGBA)
	SVGNumber 		m_stroke_linewidth;		///< The linewidth in user units (affected by the CTM)
	SVGNumber 		m_stroke_flatness;		///< The flatness value that affects how jagged circles and curves look
	SVGNumber 		m_stroke_miter_limit;	///< The miterlimit value (http://www.w3.org/TR/SVG11/painting.html)
	SVGCapType 		m_stroke_captype;		///< The linecap style
	SVGJoinType 	m_stroke_jointype;		///< The linejoin style
	SVGResolvedDashArray*	m_stroke_dasharray;		///< The dasharray, defining a dashed stroke
	SVGNumber		m_stroke_dashoffset;	///< The dashoffset
	UINT32			m_dasharray_seq;		///< The sequence number of the dasharray
	SVGVectorEffect	m_vector_effect;		///< The vector effect to use
	SVGFillRule 	m_fillrule;				///< The fill rule (even-odd or non-zero)
#ifdef SVG_SUPPORT_PAINTSERVERS
	SVGPaintServer*	m_fill_pserver;			///< The fill paintserver (gradient/pattern)
	SVGPaintServer*	m_stroke_pserver;		///< The stroke paintserver (gradient/pattern)
#endif // SVG_SUPPORT_PAINTSERVERS
	SVGVisibilityType m_visibility;			///< The current visibility state
	SVGPointerEvents m_pointer_events;      ///< The current pointer event state
	FontAtt			m_logfont;				///< The last used logical font

	BOOL			m_has_decoration_paint;	///< This state can be used for text decorations
	UINT32			m_old_underline_color;	///< Saved decoration colors, used to detect changes
	UINT32			m_old_overline_color;
	UINT32			m_old_linethrough_color;
	SVGShapeRendering m_shape_rendering;	///< shape-rendering, hint for rasterization quality
	UINT8			m_fill_opacity;
	UINT8			m_stroke_opacity;

	struct IntersectionRecord
	{
		HTML_Element*	element;			///< The intersected element
		int				start_index;		///< The intersected start index inside element
		int				end_index;			///< The intersected end index inside element
	};

	IntersectionRecord m_current;			///< The current element being tested
	IntersectionRecord m_last_intersected;	///< The last intersected (actual hit)

	union
	{
		struct
		{
			unsigned int hit_end_half : 1;
		} packed;

		unsigned short packed_init;
	} m_info;

	static inline UINT32 ModulateColor(UINT32 c, UINT8 m)
	{
		unsigned a = GetSVGColorAValue(c);
		a = (a * m) / 255;
		return (a << 24) | (c & 0x00FFFFFF);
	}

public:
	enum PaintType
	{
		USE_NONE		= 1,
		USE_COLOR		= 2
#ifdef SVG_SUPPORT_PAINTSERVERS
		,USE_PSERVER	= 3
#endif // SVG_SUPPORT_PAINTSERVERS
	};

	SVGCanvasState();
	virtual ~SVGCanvasState();

	/**
	 * Sets the SVGCanvasState properties to their default values as defined by the SVG specification.
	 *
	 * @param rendering_quality Different svgs can have different defaults (quality).
	 * 1 - .... 100 is low, 25 is high. 1 is ridiculously high.
	 */
	void SetDefaults(int rendering_quality);

	/**
	 * Saves all member variables so that they can be restored later.
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS 		SaveState();

	/**
	 * Restores all member variables to the previous state.
	 * @return OpStatus::OK if successful
	 */
	void			RestoreState();

	/**
	 * Creates a copy of the canvas state
	 * @param copy The canvas state to copy
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS Copy(SVGCanvasState* copy);

	/**
	 * Enable the fill properties to be used when drawing.
	 * @param paintserver_type See PaintType
	 */
	void EnableFill(int paintserver_type) { m_use_fill = paintserver_type; }

	/**
	 * Enable the stroke properties to be used when drawing.
	 * @param paintserver_type See PaintType
	 */
	void EnableStroke(int paintserver_type) { m_use_stroke = paintserver_type; }

	FontAtt& GetLogFont() { return m_logfont; }
	void SetLogFont(FontAtt& fa) { m_logfont = fa; }

	/**
	 * Set the last intersected element
	 *
	 * @param elm The last intersected element
	 */
	void SetLastIntersectedElement() { m_last_intersected = m_current; }
	void SetLastIntersectedElementHitEndHalf(BOOL end_half) { m_info.packed.hit_end_half = end_half; }

	/**
	 * Set the current element
	 *
	 * @param elm The current element
	 */
	void SetCurrentElement(HTML_Element* elm)
	{
		if (elm != m_current.element)
			m_current.start_index = m_current.end_index = -1;

		m_current.element = elm;
	}

	/**
	 * Set the current index inside the current element. Used for getting index
	 * of the intersected glyph cell.
	 */
	void SetCurrentElementIndex(int start, int end)
	{
		m_current.start_index = start;
		m_current.end_index = end;
	}

	/**
	 * Sets fill rule.
	 * @param fillrule The fill rule to use to fill a shape
	 */
	void SetFillRule(SVGFillRule fillrule) { m_fillrule = fillrule; }

	/**
	 * Sets fill color and opacity.
	 * @param color The RGBA color to fill with
	 */
	void SetFillColor(UINT32 color) { m_fillcolor = color; }

	/**
	 * Sets fill color.
	 * @param r The value for red (0 - 255)
	 * @param g The value for green (0 - 255)
	 * @param b The value for blue (0 - 255)
	 */
	void SetFillColorRGB(UINT8 r, UINT8 g, UINT8 b)
	{
		// can't use OP_RGBA because alpha is divided by 2
		m_fillcolor = 0xFF000000 | (b << 16) | (g << 8) | r;
	}

	/**
	 * Sets fill opacity.
	 * @param a The fill opacity, 0 for transparent, 255 for opaque
	 */
	void SetFillOpacity(UINT8 a) { m_fill_opacity = a; }

	/**
	 * Sets the dasharray (used for stroking if stroke is enabled).
	 * @param dasharray A dash array
	 * @param arraycount The number of items in the array
	 */
	OP_STATUS SetDashes(SVGNumber* dasharray, unsigned arraycount);

	/**
	 * Set the dash offset, only used if a dasharray has also been set.
	 * @param offset The offset (in userspace) before the first dash
	 */
	void SetDashOffset(SVGNumber offset) { m_stroke_dashoffset = offset; }

	/**
	 * Sets the linecap style.
	 * @param capType The linecap style to use
	 */
	void SetLineCap(SVGCapType capType) { m_stroke_captype = capType; }

	/**
	 * Sets the linejoin style.
	 * @param joinType The linejoin style to use
	 */
	void SetLineJoin(SVGJoinType joinType) { m_stroke_jointype = joinType; }

	/**
	 * Sets the stroke linewidth.
	 * @param lineWidth The linewidth to use (in user units)
	 */
	void SetLineWidth(SVGNumber lineWidth) { m_stroke_linewidth = lineWidth; }

	/**
	 * Sets the miter limit.
	 * @param miterLimit The miterlimit to use
	 */
	void SetMiterLimit(SVGNumber miterLimit) { m_stroke_miter_limit = miterLimit; }

	/**
	 * Sets the flatness of curves (affects how jagged they look when rendered).
	 * @param flatness The flatness value
	 */
	void SetFlatness(SVGNumber flatness) { m_stroke_flatness = flatness; }
	SVGNumber GetFlatness() { return m_stroke_flatness; }

	/**
	 * Sets the 'vector effect' to use for the stroke
	 * @param effect The vector effect to use
	 */
	void SetVectorEffect(SVGVectorEffect effect) { m_vector_effect = effect; }

	/**
	 * Sets stroke color and opacity.
	 * @param color The RGBA color to fill with
	 */
	void SetStrokeColor(UINT32 color) { m_strokecolor = color; }

	/**
	 * Sets stroke color.
	 * @param r The value for red (0 - 255)
	 * @param g The value for green (0 - 255)
	 * @param b The value for blue (0 - 255)
	 */
	void SetStrokeColorRGB(UINT8 r, UINT8 g, UINT8 b)
	{
		// can't use OP_RGBA because alpha is divided by 2
		m_strokecolor = 0xFF000000 | (b << 16) | (g << 8) | r;
	}

	/**
	 * Sets stroke opacity.
	 * @param a The stroke opacity, 0 for transparent, 255 for opaque
	 */
	void SetStrokeOpacity(UINT8 a) { m_stroke_opacity = a; }

	/**
	 * Get the last intersected element
	 *
	 * @param elm The last intersected element
	 */
	HTML_Element* GetLastIntersectedElement() { return m_last_intersected.element; }

	/**
	 * Get the last intersected index
	 *
	 * @param int The last intersected index
	 */
	void GetLastIntersectedElementIndex(int& start, int& end)
	{
		start = m_last_intersected.start_index;
		end = m_last_intersected.end_index;
	}

	BOOL GetLastIntersectedElementIndexHitEndHalf() { return m_info.packed.hit_end_half; }

#ifdef SVG_SUPPORT_PAINTSERVERS
	void SetFillPaintServer(SVGPaintServer* pserver);
	void SetStrokePaintServer(SVGPaintServer* pserver);
#endif // SVG_SUPPORT_PAINTSERVERS

	UINT32 GetFillColor() { return ModulateColor(m_fillcolor, m_fill_opacity); }
	UINT32 GetActualFillColor() const { return m_fillcolor; }
	UINT8 GetFillOpacity() { return m_fill_opacity; }

	void SetTextDecoPaint() { m_has_decoration_paint = TRUE; }
	BOOL DecorationsChanged(const HTMLayoutProperties& props);
	OP_STATUS SetDecorationPaint();
	void GetDecorationPaints(SVGPaintDesc* paints);
	void ResetDecorationPaint();

	/**
	 * Multiplies the given matrix with the current transformation
	 * matrix and sets the result as new CTM.
	 *
	 * @param trans The matrix
	 */
	void ConcatTransform(const SVGMatrix &trans) { m_transform.PostMultiply(trans); }

	/** Same as above but for translations */
	void ConcatTranslation(const SVGNumberPair &transl) { m_transform.PostMultTranslation(transl.x, transl.y); }

	/**
	 * Gets a copy of the current transformation matrix.
	 *
	 * @param outtrans A previously allocated matrix to copy to.
	 */
	void GetTransform(SVGMatrix &outtrans) { outtrans.Copy(m_transform); }
	void SetTransform(const SVGMatrix &trans) { m_transform.Copy(trans); }

	/**
	 * Is the transform a pure translation+scale? (i.e. no rotation)
	 */
	BOOL IsAlignedTransform() const { return m_transform[1].Equal(0) && m_transform[2].Equal(0); }

	void SetVisibility(SVGVisibilityType vis) { m_visibility = vis; }
	SVGVisibilityType GetVisibility() { return m_visibility; }

	void SetShapeRendering(SVGShapeRendering sr) { m_shape_rendering = sr; }

	void SetPointerEvents(SVGPointerEvents pointer_events) { m_pointer_events = pointer_events; }

	/**
	 * Allow pointer events for fill or stroke according to the fill,
	 * stroke, visibility, and pointer-events attributes.
	 *
	 * @param flags What flags of SVGAllowPointerEvents to
	 * consider. Values can be logically or'ed together.
	 */
	BOOL AllowPointerEvents(int type);

	/**
	 * Allow pointer events in the current state.
	 */
	BOOL AllowPointerEvents()
	{
		return(AllowPointerEvents(SVGALLOWPOINTEREVENTS_FILL | SVGALLOWPOINTEREVENTS_STROKE));
	}

	/**
	 * Call this function to find out whether elements should be drawn
	 * to the canvas. Parameters affecting this is the visibility
	 * property and, depending on 'ignore_pointer_events' parameter,
	 * the pointer-events property.
	 */
	BOOL AllowDraw(BOOL ignore_pointer_events);

	void SetFillSeq(UINT32 seqnum) { m_fill_seq = seqnum; }
	void SetStrokeSeq(UINT32 seqnum) { m_stroke_seq = seqnum; }
	void SetDashArraySeq(UINT32 seqnum) { m_dasharray_seq = seqnum; }

	//=============== Getters ==================//

	BOOL UseFill() { return m_use_fill != USE_NONE; }
	UINT32 FillSeq() { return m_fill_seq; }
	BOOL UseStroke() { return m_use_stroke != USE_NONE; }
	UINT32 StrokeSeq() { return m_stroke_seq; }
	int GetFill() { return m_use_fill; }
	int GetStroke() { return m_use_stroke; }
	SVGMatrix& GetTransform() { return m_transform; }
	SVGNumber GetStrokeFlatness() { return m_stroke_flatness; }
	SVGVectorEffect GetVectorEffect() { return m_vector_effect; }
	SVGFillRule GetFillRule() { return m_fillrule; }
	UINT32 GetStrokeColor() { return ModulateColor(m_strokecolor, m_stroke_opacity); }
	UINT32 GetActualStrokeColor() const { return m_strokecolor; }
	UINT8 GetStrokeOpacity() { return m_stroke_opacity; }
	SVGNumber GetStrokeLineWidth() { return m_stroke_linewidth; }
	SVGNumber GetStrokeMiterLimit() { return m_stroke_miter_limit; }
	SVGCapType GetStrokeCapType() { return m_stroke_captype; }
	SVGJoinType GetStrokeJoinType() { return m_stroke_jointype; }
	SVGResolvedDashArray* GetStrokeDashArray() { return m_stroke_dasharray; }
	SVGNumber GetStrokeDashOffset() const { return m_stroke_dashoffset; }
	UINT32 DashArraySeq() { return m_dasharray_seq; }
#ifdef SVG_SUPPORT_PAINTSERVERS
	SVGPaintServer* GetFillPaintServer() { return m_use_fill == USE_PSERVER ? m_fill_pserver : NULL; }
	SVGPaintServer* GetStrokePaintServer() { return m_use_stroke == USE_PSERVER ? m_stroke_pserver : NULL; }
#endif // SVG_SUPPORT_PAINTSERVERS
	SVGPointerEvents GetPointerEvents() { return m_pointer_events; }
	SVGShapeRendering GetShapeRendering() { return m_shape_rendering; }
	unsigned int GetShapeRenderingVegaQuality();

#if defined(SVG_INTERRUPTABLE_RENDERING) || defined(DEBUG_ENABLE_OPASSERT)
	int GetStackDepth()
	{
		SVGCanvasState* state = m_prev;
		int depth = 0;
		while (state)
		{
			depth++;
			state=state->m_prev;
		}
		return depth;
	}
#endif // SVG_INTERRUPTABLE_RENDERING || DEBUG_ENABLE_OPASSERT

private:
	SVGCanvasState* GetDecorationState();

	SVGCanvasState* 		m_prev;			///< The previous state
};

class SVGCanvasStateStack
{
private:
	SVGCanvasState* m_canvasstate;
public:
	SVGCanvasStateStack() : m_canvasstate(NULL) {}
	OP_STATUS Save(SVGCanvasState* canvasstate)
	{
		if (!canvasstate)
			return OpStatus::ERR;

		m_canvasstate = canvasstate;
		return canvasstate->SaveState();
	}
	void TakeOver(SVGCanvasState* state) { m_canvasstate = state; }
	void Release() { m_canvasstate = NULL; }
	OP_STATUS Restore()
	{
		if (!m_canvasstate)
			return OpStatus::ERR;

		m_canvasstate->RestoreState();
		m_canvasstate = NULL;
		return OpStatus::OK;
	}
	~SVGCanvasStateStack()
	{
		if (m_canvasstate)
			m_canvasstate->RestoreState();
	}
};

#endif // SVG_SUPPORT
#endif // SVG_CANVAS_STATE_H
