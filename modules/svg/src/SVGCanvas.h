/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** This is the abstract interface class that's used for drawing the
** vector graphics.
*/
#ifndef SVGCANVAS_H
#define SVGCANVAS_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGCanvasState.h"
#include "modules/svg/src/SVGCache.h"

#include "modules/libvega/vegatransform.h"
#include "modules/libvega/vegapath.h"

#include "modules/util/simset.h"

class OpFont;
class OpPainter;
class OpBitmap;
class OpBpath;

class SVGRect;
class SVGSurface;
class SVGVector;

/**
 * This is an abstract interface class to be implemented on your
 * platform.  When rendering SVG content a SVGCanvas object is created
 * for doing the actual painting. SVGCanvasState keeps track of
 * current canvas properties.
 *
 * The class hierarchy looks like:
 *  SVGCanvasState					(in SVGCanvasState.{cpp,h})
 *  +-SVGCanvas
 *  | +-SVGGeometryCanvas
 *  |   +-SVGCanvasVega
 *  |   +-SVGCanvasFakePainter
 *  |     +-SVGInvalidationCanvas
 *  |     +-SVGIntersectionCanvas
 *  +-SVGNullCanvas
 *
 * Note that stroking should be centered around the contour given,
 * half on the inside, half on the outside.
 */
class SVGCanvas : public SVGCanvasState
{
public:
	enum RenderMode
	{
		RENDERMODE_DRAW                 =      0x00,   ///< Draw to canvas
		RENDERMODE_INTERSECT_POINT      =      0x01,   ///< Intersect element with coordinate
		RENDERMODE_INTERSECT_RECT       =      0x02,   ///< Intersect element with rectangle
		RENDERMODE_ENCLOSURE            =      0x03,   ///< Elements enclosed by rectangle
		RENDERMODE_INVALID_CALC         =      0x04    ///< Don't draw anything. Just calculate dirty rect.
	};

	enum StencilMode
	{
		STENCIL_USE 					= 	   0x00,	///< Draw using current stencil
		STENCIL_CONSTRUCT				=	   0x01	///< Construct a stencil
	};

	enum ImageRenderQuality
	{
		IMAGE_QUALITY_LOW,
		IMAGE_QUALITY_NORMAL,
		IMAGE_QUALITY_HIGH
	};

	virtual ~SVGCanvas(){}

	/**
	 * Set the base scale factor for the canvas. This roughly
	 * translates to the scale factor of the target visual device.
	 */
	void SetBaseScale(SVGNumber base_scale) { m_base_scale = base_scale; }

	/**
	 * Get the base scale factor for the canvas.
	 */
	SVGNumber GetBaseScale() const { return m_base_scale; }

	/**
	 * Resets the transform to the base transform (used for
	 * ref-transform)
	 */
	void ResetTransform() { GetTransform().LoadScale(m_base_scale, m_base_scale); }

	/**
	 * Sets the anti-aliasing quality. A value of 0 disables anti-aliasing.
	 *
	 * @return OpStatus::OK if successful
	 */
	virtual OP_STATUS SetAntialiasingQuality(unsigned int quality) { return OpStatus::OK; }

	/**
	 * Get the estimated amount of memory used by the canvas.
	 * (Currently only the buffer from the root-surface).
	 * @return Size (in bytes) used.
	 */
	virtual unsigned int GetMemUsed() { return 0; }

	/**
	 * Destroy resources associated with the canvas, such as
	 * clips/masks/transparency layers.  To be used if a canvas from
	 * an aborted rendering needs to reused.
	 */
	virtual void Clean() {}

#ifdef SVG_SUPPORT_STENCIL
	/**
	 * Create a rectangular clipping region.
	 * Is equivalent to a sequence BeginClip() / DrawRect(cliprect) / EndClip().
	 *
	 * @return OpStatus::OK if successful
	 */
	virtual OP_STATUS AddClipRect(const SVGRect& cliprect) = 0;

	/**
	 * Begin defining a clipping region. All subsequent drawing
	 * operations until EndClip() will be used to define the
	 * region. The clipping will be intersected with the previous
	 * clipping region if any.  The old clipregion is saved on a
	 * stack, for restoring when RemoveClip is called.
	 *
	 * @return OpStatus::OK if successful
	 */
	virtual OP_STATUS BeginClip() = 0;

	/**
	 * All drawing commands between a BeginClip and an EndClip will be
	 * used for clipping.  EndClip sets the clipping, so after this
	 * call the clipping is active.  Needs to be balaced with
	 * BeginClip.
	 */
	virtual void EndClip() = 0;

	/**
	* Remove the current clippath from the stack and set the previous one.
	*
	* @return OpStatus::OK if successful
	*/
	virtual void RemoveClip() = 0;
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_STENCIL
    /**
     * Begin a defining mask. Create a new off-screen buffer where all
     * the drawing operations until EndMask() will be rendered. The
     * previous mask is saved on a stack, and is restored when
     * RemoveMask() is called.
     *
     * @return OpStatus::OK if successful
     */
    virtual OP_STATUS BeginMask() { return OpStatus::OK; }

    /**
     * End the definition of a mask. EndMask() sets the mask, so after
     * this call, the mask will be set. Needs to be balanced with
     * BeginMask().
     */
    virtual void EndMask() {}

    /**
     * Removes the current mask from the stack and sets the previous one.
     *
     * @return OpStatus::OK if successful
     */
    virtual void RemoveMask() {}
#endif // SVG_SUPPORT_STENCIL

	/**
	 * Return the current render mode
	 */
	RenderMode GetRenderMode() const { return m_render_mode; }

#ifdef SVG_SUPPORT_STENCIL
	/**
	 * Return the current clipping mode
	 */
	StencilMode GetClipMode() const { return m_clip_mode; }

	/**
	 * Sets the clipping mode, only for the brave.
	 */
	void SetClipMode(StencilMode mode) { m_clip_mode = mode; }

	/**
	 * Return the current mask mode
	 */
	StencilMode GetMaskMode() const { return m_mask_mode; }

	/**
	 * Sets the mask mode, only for the brave.
	 */
	void SetMaskMode(StencilMode mode) { m_mask_mode = mode; }
#endif // SVG_SUPPORT_STENCIL

	/**
	 * Tests the current render mode is one of the intersection modes
	 *
	 * @return Returns TRUE if the current render mode is an intersection mode
	 */
	BOOL IsIntersectionMode()
	{
		return
			m_render_mode == RENDERMODE_INTERSECT_POINT ||
			m_render_mode == RENDERMODE_INTERSECT_RECT ||
			m_render_mode == RENDERMODE_ENCLOSURE;
	}

	/**
	 * Get the current intersectionpoint.
	 *
	 * @return Return the intersectionpoint passed in through SetIntersectionMode.
	 */
	virtual const SVGNumberPair& GetIntersectionPoint() = 0;

	/**
	 * Get the current intersectionpoint in (truncated) integer form.
	 *
	 * @return Return the intersectionpoint passed in through SetIntersectionMode.
	 */
	virtual const OpPoint& GetIntersectionPointInt() = 0;

	/**
	 * Clears the canvas with the color specified.
	 *
	 * For this to have an effect, you'd need a canvas that actually
	 * paints.
	 *
	 * @param r The value for red (0 - 255)
	 * @param g The value for green (0 - 255)
	 * @param b The value for blue (0 - 255)
	 * @param a The value for alpha (0 - 255). 255 means opaque, 0 means transparent
	 */
	virtual void Clear(UINT8 r, UINT8 g, UINT8 b, UINT8 a, const OpRect* clear_rect = NULL) {}

	/**
	 * Draws a line between the points (x1,y1), (x2,y2).
	 *
	 * Coordinate values are in userspace units, which means they are
	 * affected by the current transformation matrix.
	 *
	 * NOTE: DrawLine should only stroke the line, not fill it. This
	 * is what the SVG spec says: "Because 'line' elements are single
	 * lines and thus are geometrically one-dimensional, they have no
	 * interior; thus, 'line' elements are never filled (see the
	 * 'fill' property)"
	 *
	 * @param x1 Start x
	 * @param y1 Start y
	 * @param x2 End x
	 * @param y2 End y
	 */
	virtual OP_STATUS DrawLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2) = 0;

	/**
	 * Draws a rectangle with it's left upper corner in (x,y) with
	 * width and height (w,h) and optional rounded corners.
	 *
	 * Coordinate values are in userspace units, which means they are
	 * affected by the current transformation matrix.
	 *
	 * @param x Left upper corner x
	 * @param y Left upper corner y
	 * @param w Width of rectangle
	 * @param h Height of rectangle
	 * @param rx If 0 then the corners will be straight, if > 0 then that specifies the x radius
	 * @param ry If 0 then the corners will be straight, if > 0 then that specifies the y radius
	 */
	virtual OP_STATUS DrawRect(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h, SVGNumber rx, SVGNumber ry) = 0;

	/**
	 * Draws an ellipse centered in the point (x,y) with radius (rx,ry).
	 *
	 * Coordinate values are in userspace units, which means they are
	 * affected by the current transformation matrix.
	 *
	 * @param x Center x
	 * @param y Center y
	 * @param rx The x radius
	 * @param ry The y radius
	 */
	virtual OP_STATUS DrawEllipse(SVGNumber x, SVGNumber y, SVGNumber rx, SVGNumber ry) = 0;

	/**
	 * Draws a polygon shape, which means move to the first
	 * coordinate, then connect the dots with straight lines.
	 *
	 * Coordinate values are in userspace units, which means they are
	 * affected by the current transformation matrix.
	 *
	 * @param points An array of points, the first coordinate is (x,y) = (points[0],points[1]), the second is (points[2],points[3]) and so on.
	 * @param closed Flag indicating whether the polygon should be explicitly closed.
	 */
	virtual OP_STATUS DrawPolygon(const SVGVector& list, BOOL closed) = 0;

	/**
	 * Draws a path, that may consist of straight lines, 2nd or 3rd
	 * degree Bezier curves, and arcs.  If that sounds complicated,
	 * relax, OpBpath is your friend and can convert those curves to
	 * straight lines.
	 *
	 * Coordinate values are in userspace units, which means they are
	 * affected by the current transformation matrix.
	 *
	 * @param path The path
	 * @param path_length A user-specified value for how long the path is, -1 means not specified
	 */
	virtual OP_STATUS DrawPath(const OpBpath *path, SVGNumber path_length = -1) = 0;

	/**
	 * Draws a bitmap.
	 *
	 * Coordinate values are in userspace units, which means they are
	 * affected by the current transformation matrix.
	 *
	 * @param bitmap/surface The bitmap/surface
	 * @param source The source rectangle
	 * @param dest The destination rectangle
	 * @param quality The interpolation quality to use
	 */
	virtual OP_STATUS DrawImage(OpBitmap* bitmap, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality) = 0;
	virtual OP_STATUS DrawImage(SVGSurface* surface, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality) = 0;

	/**
	 * Set the rect which we expect to be painting on. This generally
	 * equals the 'screen box' of a text-root.
	 *
	 * This rect will be used by GetPainter(unsigned int).
	 */
	virtual void SetPainterRect(const OpRect& rect) = 0;

	/**
	 * Get an OpPainter for the canvas. This is used to draw systemfonts.
	 * Only one active painter is allowed at any one time for a canvas.
	 * The painter must be released with ReleasePainter().
	 *
	 * @param font_overhang Overhang compensation
	 * @return ERR if not supported, OK if success.
	 */
	virtual OP_STATUS GetPainter(unsigned int font_overhang) = 0;

	/**
	 * Get an OpPainter for the canvas.
	 * Only one active painter is allowed at any one time for a canvas.
	 * The painter must be released with ReleasePainter().
	 *
	 * This will do an implicit SetPainterRect with rect.
	 *
	 * @param rect The smallest interesting region.
	 * @param painter The painter will be returned here.
	 * @return ERR if not supported, OK if success.
	 */
	virtual OP_STATUS GetPainter(const OpRect& rect, OpPainter*& painter) = 0;

	/**
	 * Releases the painter that was returned from GetPainter().
	 *
	 * @param resultbitmap If NULL then data should be copied back, if non-NULL then the resulting
	 *					  bitmap is desired instead.
	 */
	virtual void ReleasePainter() = 0;

	virtual BOOL IsPainterActive() = 0;

	virtual void DrawStringPainter(OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing, int* caret_x = NULL) = 0;
	virtual void DrawSelectedStringPainter(const OpRect& selected, COLORREF sel_bg_color, COLORREF sel_fg_color, OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing) = 0;

	/**
	 * Returns the parts of the canvas (in fixed canvas/pixel
	 * coordinates) that has been rendered to since the last call to
	 * ResetDirtyRect. This may return a too big area (which will
	 * reduce the usefulness of other optimizations) but never a too
	 * small one.
	 *
	 * Should only be used during invalidation/layout-passes, and as
	 * such belongs in the SVGInvalidationCanvas.
	 */
	virtual OpRect GetDirtyRect() { return OpRect(); }

	/**
	 * This is to tell the canvas about things that will make an area
	 * dirty without passing through the Canvas. For example system
	 * fonts which copy a bitmap on top on the canvas.
	 *
	 * Should only be used during invalidation/layout-passes, and as
	 * such belongs in the SVGInvalidationCanvas.
	 *
	 * @param new_dirty_area The rect that should be marked dirty.
	 */
	virtual void AddToDirtyRect(const OpRect& new_dirty_area) {}

	/**
	 * Clears the "dirty rect". See the documentation of GetDirtyRect.
	 *
	 * Should only be used during invalidation/layout-passes, and as
	 * such belongs in the SVGInvalidationCanvas.
	 */
	virtual void ResetDirtyRect() {}

	/**
	 * Check if the given rect intersects the hard clip area.
	 * If no hard clip area has been set TRUE should be returned.
	 */
	virtual BOOL IsVisible(const OpRect& rect) { return TRUE; }

	/**
	 * Check if the given rect is fully contained inside the hard clip area.
	 * If no hard clip area has been set FALSE should be returned.
	 */
	virtual BOOL IsContainedBy(const OpRect& rect) { return FALSE; }

#ifdef SVG_SUPPORT_STENCIL
	/**
	 * Clips the given rect against the current stencil.
	 */
	virtual void ApplyClipToRegion(OpRect& iorect) {}
#endif // SVG_SUPPORT_STENCIL

#if defined SVG_SUPPORT_MEDIA && defined PI_VIDEO_LAYER
	/** Check if it is safe to use Video Layer "overlays". */
	virtual BOOL UseVideoOverlay() { return FALSE; }
#endif // defined SVG_SUPPORT_MEDIA && defined PI_VIDEO_LAYER

protected:
	SVGCanvas() :
		m_base_scale(1),
		m_render_mode(RENDERMODE_DRAW),
		m_clip_mode(STENCIL_USE),
		m_mask_mode(STENCIL_USE) {}

	SVGNumber	m_base_scale;
	RenderMode	m_render_mode;
	StencilMode	m_clip_mode;
	StencilMode	m_mask_mode;
};

/**
 * Intermediary helper class which converts primitive calls into
 * actual rendering geometry (using VEGAPath/VEGAPrimitive) and pass
 * that along to a ProcessPrimitive-hook which subclasses need to
 * implement.
 * Also handles stroke and pointer-events (if required by concrete class).
 */
class SVGGeometryCanvas : public SVGCanvas
{
public:
	SVGGeometryCanvas(BOOL use_pointer_events) : m_use_pointer_events(use_pointer_events) {}

#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS AddClipRect(const SVGRect& cliprect);
	virtual OP_STATUS BeginClip() = 0;
	virtual void EndClip() = 0;
	virtual void RemoveClip() = 0;
#endif // SVG_SUPPORT_STENCIL

	virtual OP_STATUS DrawLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2);
	virtual OP_STATUS DrawRect(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h, SVGNumber rx, SVGNumber ry);
	virtual OP_STATUS DrawEllipse(SVGNumber x, SVGNumber y, SVGNumber rx, SVGNumber ry);
	virtual OP_STATUS DrawPolygon(const SVGVector& list, BOOL closed);
	virtual OP_STATUS DrawPath(const OpBpath *path, SVGNumber path_length = -1);
	virtual OP_STATUS DrawImage(OpBitmap* bitmap, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality);
	virtual OP_STATUS DrawImage(SVGSurface* bitmap_surface, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality);

	BOOL IsPointerEventsSensitive(int type)
	{
		return m_use_pointer_events && AllowPointerEvents(type);
	}

protected:
	struct VPrimitive
	{
		VEGAPath* vpath;
		VEGAPrimitive* vprim;

		enum GeometryType
		{
			GEOM_FILL,
			GEOM_STROKE,
			GEOM_IMAGE
		};

		GeometryType geom_type;
		BOOL do_intersection_test;
	};

	virtual OP_STATUS ProcessPrimitive(VPrimitive& prim) = 0;

	OP_STATUS CreateStrokePath(VEGAPath& vpath, VEGAPath& vstrokepath, VEGA_FIX precompPathLength = VEGA_INTTOFIX(-1));

	OP_STATUS ProcessImage(const SVGRect& dest);

	OP_STATUS FillStrokePath(VEGAPath& vpath, VEGA_FIX pathLength);
	OP_STATUS FillStrokePrimitive(VEGAPrimitive& vprimitive);

	void SetVegaTransform();
	VEGATransform m_vtransform;

	BOOL m_use_pointer_events;
};


/**
 * Intermediary helper class which keeps a stateful (but dummy)
 * implementation for the different *Painter* methods.
 */
class SVGCanvasFakePainter : public SVGGeometryCanvas
{
public:
	SVGCanvasFakePainter() : SVGGeometryCanvas(TRUE), m_painter_active(FALSE) {}

	virtual void SetPainterRect(const OpRect& rect) {}
	virtual OP_STATUS GetPainter(unsigned int font_overhang)
	{
		m_painter_active = TRUE;
		return OpStatus::OK;
	}
	virtual OP_STATUS GetPainter(const OpRect& rect, OpPainter*& painter) { return OpStatus::ERR; }
	virtual void ReleasePainter() { m_painter_active = FALSE; }
	virtual BOOL IsPainterActive() { return m_painter_active; }

protected:
	BOOL m_painter_active;
};

/**
 * SVGCanvas implementation used during intersection-testing. Tracks
 * resources in the form of geometric shapes and performs
 * intersection-testing ("hit-testing") on said geometry.
 * (previously available as RENDERMODE_INTERSECT* in SVGCanvasVega)
 */
class SVGIntersectionCanvas : public SVGCanvasFakePainter
{
public:
	virtual ~SVGIntersectionCanvas();

	void SetIntersectionMode(const SVGNumberPair& intersection_point);
	void SetIntersectionMode(const SVGRect& intersection_rect);
	void SetEnclosureMode(const SVGRect& intersection_rect);

	/**
	 * Set a list to add intersected elements to
	 *
	 * @param lst List to which intersected elements will be added
	 */
	void SetSelectionList(OpVector<HTML_Element>* lst) { m_selected_list = lst; }

	/**
	 * Adds the current element to the list of selected items
	 */
	void SelectCurrentElement()
	{
		if (m_selected_list)
			m_selected_list->Add(m_current.element);
	}

	virtual void Clean();

	virtual void DrawStringPainter(OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing, int* caret_x = NULL);
	virtual void DrawSelectedStringPainter(const OpRect& selected, COLORREF sel_bgcolor, COLORREF sel_fgcolor, OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing);

	virtual const SVGNumberPair& GetIntersectionPoint() { return m_intersection_point; }
	virtual const OpPoint& GetIntersectionPointInt() { return m_intersection_point_i; }

#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS BeginClip();
	virtual void EndClip();
	virtual void RemoveClip();

	// SVGClipPathInfo needs to be declared public to be able to use
	// this struct inside SVGClipPathSet Used for intersection testing
	struct SVGClipPathInfo : public ListElement<SVGClipPathInfo>
	{
		SVGClipPathInfo(SVGFillRule rule) : clip_rule(rule) {}

		VEGAPath path;
		SVGFillRule clip_rule;
	};
#endif // SVG_SUPPORT_STENCIL

protected:
	virtual OP_STATUS ProcessPrimitive(VPrimitive& prim);

#ifdef SVG_SUPPORT_STENCIL
	struct SVGClipPathSet : public ListElement<SVGClipPathSet>
	{
		SVGClipPathSet(StencilMode in_prev_mode) : prev_mode(in_prev_mode) {}

		AutoDeleteList<SVGClipPathInfo> paths;
		StencilMode prev_mode;
	};

	AutoDeleteList<SVGClipPathSet> m_active_clipsets;
	AutoDeleteList<SVGClipPathSet> m_partial_clipsets;

	BOOL IntersectClip();
#endif // SVG_SUPPORT_STENCIL

	void IntersectPath(VEGAPath* path, SVGFillRule fillrule);

	SVGNumberPair	m_intersection_point;
	OpPoint			m_intersection_point_i;
	SVGRect			m_intersection_rect; // Used for RENDERMODE_INTERSECT_RECT and RENDERMODE_ENCLOSURE

	OpVector<HTML_Element>* m_selected_list; ///< List of selected elements
};

/**
 * SVGCanvas implementation used during invalidation/layout. Does
 * little except track resources using during invalidation, and
 * calculate extents for primitives drawn.
 * (previously available as RENDERMODE_INVALIDATION_CALC in SVGCanvasVega).
 */
class SVGInvalidationCanvas : public SVGCanvasFakePainter
{
public:
	SVGInvalidationCanvas() : SVGCanvasFakePainter() { m_render_mode = RENDERMODE_INVALID_CALC; }

	virtual void Clean();

	virtual void DrawStringPainter(OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing, int* caret_x = NULL);
	virtual void DrawSelectedStringPainter(const OpRect& selected, COLORREF sel_bgcolor, COLORREF sel_fgcolor, OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing);

	virtual const SVGNumberPair& GetIntersectionPoint() { return m_dummy_numpair; }
	virtual const OpPoint& GetIntersectionPointInt() { return m_dummy_point; }

#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS BeginClip();
	virtual void EndClip();
	virtual void RemoveClip();

	virtual OP_STATUS BeginMask();
	virtual void EndMask();
	virtual void RemoveMask();

	virtual void ApplyClipToRegion(OpRect& region);
#endif // SVG_SUPPORT_STENCIL

	virtual OpRect GetDirtyRect();
	virtual void AddToDirtyRect(const OpRect& new_dirty_area);
	virtual void ResetDirtyRect();

protected:
	virtual OP_STATUS ProcessPrimitive(VPrimitive& prim);

#ifdef SVG_SUPPORT_STENCIL
public:
	// Stencil needs to be declared public to be able to use
	// this struct inside StencilState:
	struct Stencil : public ListElement<Stencil>
	{
		Stencil(StencilMode in_prev_mode) : prev_mode(in_prev_mode) {}

		OpRect area;
		StencilMode prev_mode;
	};

protected:
	AutoDeleteList<Stencil> m_active_clips;
	AutoDeleteList<Stencil> m_partial_clips;

	AutoDeleteList<Stencil> m_active_masks;
	AutoDeleteList<Stencil> m_partial_masks;
#endif // SVG_SUPPORT_STENCIL

	OpRect			m_dirty_area;

	SVGNumberPair	m_dummy_numpair;
	OpPoint			m_dummy_point;
};

/**
 * This is an implementation of the SVGCanvas interface that is
 * supposed to do nothing in most cases. This can be used in all cases
 * where a canvas is needed (traversing), but the actual contents, or
 * what ends up on the canvas is not of interest (since it still
 * inherits from SVGCanvasState, state-handling will still work as
 * expected though).
 */
class SVGNullCanvas : public SVGCanvas
{
public:
#ifdef SVG_SUPPORT_STENCIL
	virtual OP_STATUS AddClipRect(const SVGRect& cliprect) { return OpStatus::OK; }

	virtual OP_STATUS BeginClip() { return OpStatus::OK; }
	virtual void EndClip() {}
	virtual void RemoveClip() {}
#endif // SVG_SUPPORT_STENCIL

	virtual const SVGNumberPair& GetIntersectionPoint() { return m_dummy_isect_pt; }
	virtual const OpPoint& GetIntersectionPointInt() { return m_dummy_isect_pti; }

	virtual OP_STATUS DrawLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2) { return OpStatus::OK; }
	virtual OP_STATUS DrawRect(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h, SVGNumber rx, SVGNumber ry) { return OpStatus::OK; }
	virtual OP_STATUS DrawEllipse(SVGNumber x, SVGNumber y, SVGNumber rx, SVGNumber ry) { return OpStatus::OK; }
	virtual OP_STATUS DrawPolygon(const SVGVector &list, BOOL closed) { return OpStatus::OK; }
	virtual OP_STATUS DrawPath(const OpBpath *path, SVGNumber path_length = -1) { return OpStatus::OK; }

	virtual OP_STATUS DrawImage(OpBitmap* bitmap, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality) { return OpStatus::OK; }
	virtual OP_STATUS DrawImage(SVGSurface* bitmap_surface, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality) { return OpStatus::OK; }

	virtual void SetPainterRect(const OpRect& rect) {}
	virtual OP_STATUS GetPainter(unsigned int font_overhang) { return OpStatus::OK; }
	virtual OP_STATUS GetPainter(const OpRect& rect, OpPainter*& painter) { return OpStatus::OK; }
	virtual void ReleasePainter() {}

	virtual BOOL IsPainterActive() { return FALSE; } // FIXME: Need to track this?

	virtual void DrawStringPainter(OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing, int* caret_x = NULL) {}
	virtual void DrawSelectedStringPainter(const OpRect& selected, COLORREF sel_bg_color, COLORREF sel_fg_color, OpFont* font, const OpPoint& pos, uni_char* text, UINT32 len, INT32 extra_char_spacing) {}

private:
	SVGNumberPair	m_dummy_isect_pt;
	OpPoint			m_dummy_isect_pti;
};

#endif // SVG_SUPPORT
#endif // SVGCANVAS_H

