/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVGPAINTER_H
#define SVGPAINTER_H

#ifdef SVG_SUPPORT

#include "modules/libvega/vegatransform.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#ifdef SVG_SUPPORT_STENCIL
# include "modules/libvega/vegastencil.h"
#endif // SVG_SUPPORT_STENCIL

#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGMatrix.h"

#ifdef PI_VIDEO_LAYER
#include "modules/display/vis_dev_transform.h"
#endif // PI_VIDEO_LAYER

#include "modules/util/simset.h"

class OpFont;
class OpBitmap;
class OpPainter;

class OpBpath;
class SVGMatrix;
class SVGVector;
class SVGPaintNode;
class SVGPaintServer;
class SVGResolvedDashArray;

struct SVGPaintDesc
{
	SVGPaintServer* pserver;
	UINT32 color;
	UINT8 opacity;

	void Reset()
	{
		color = 0;
		opacity = 0;
		pserver = NULL;
	}
	BOOL IsTransparent() const
	{
		return opacity == 0 ||
			(pserver == NULL && GetSVGColorAValue(color) == 0);
	}
};

struct SVGStrokeProperties
{
	SVGStrokeProperties() :
		dash_array(NULL), non_scaling(FALSE) {}

	SVGResolvedDashArray*	dash_array;		///< The dasharray, defining a dashed stroke
	SVGNumber				dash_offset;	///< Any offset for the dash
	SVGNumber 				width;			///< The width of the stroke in user units
	SVGNumber 				miter_limit;	///< The miter limit value
	unsigned 				cap:2;			///< The linecap style (SVGCapType, 3 computed values)
	unsigned 				join:2;			///< The linejoin style (SVGJoinType, 3 computed value)
	unsigned				non_scaling:1;	///< Whether the stroke is non-scaling

	SVGNumber JoinExtents(const SVGMatrix& inv_host = SVGMatrix()) const
	{
		SVGNumber join_ext = width / 2;
		switch (join)
		{
		case SVGJOIN_MITER:	/* Worst case: stroke-width / 2 * miter-limit */
			join_ext *= miter_limit;
			break;

		default:
			OP_ASSERT(!"Invalid value");
		case SVGJOIN_ROUND:	/* Worst case: stroke-width / 2 */
		case SVGJOIN_BEVEL:	/* Worst case: stroke-width / 2 */
			break;
		}

		if (!inv_host.IsIdentity())
			join_ext *= inv_host.GetExpansionFactor();

		return join_ext;
	}

	SVGNumber CapExtents(const SVGMatrix& inv_host = SVGMatrix()) const
	{
		SVGNumber cap_ext = width;
		switch (cap)
		{
		case SVGCAP_SQUARE:	/* Worst case: stroke-width / sqrt(2) */
			cap_ext /= SVGNumber(2).sqrt();
			break;

		default:
			OP_ASSERT(!"Invalid value");
		case SVGCAP_BUTT:	/* Worst case: stroke-width / 2 */
		case SVGCAP_ROUND:	/* Worst case: stroke-width / 2 */
			cap_ext /= 2;
			break;
		}

		if (!inv_host.IsIdentity())
			cap_ext *= inv_host.GetExpansionFactor();

		return cap_ext;
	}

	SVGNumber Extents(const SVGMatrix& inv_host = SVGMatrix()) const
	{
		/* This is a pessimistic approximation for how a stroke with a
		 * certain set of stroke properties will extend the extents of
		 * an object it applies to. */
		SVGNumber ext = SVGNumber::max_of(CapExtents(), JoinExtents());
		if (!inv_host.IsIdentity())
			ext *= inv_host.GetExpansionFactor();
		return ext;
	}
};

struct SVGObjectProperties
{
	unsigned aa_quality:4;	///< This goes to 8 at the moment
	unsigned fillrule:1;	///< Fill-rule (SVGFillRule, 2 computed values)
	unsigned filled:1;
	unsigned stroked:1;
};

class SVGPainter
{
public:
	enum StencilMode
	{
		STENCIL_USE			= 0x00, ///< Draw using current stencil
		STENCIL_CONSTRUCT	= 0x01	///< Construct a stencil
	};

	enum ImageRenderQuality
	{
		IMAGE_QUALITY_LOW,
		IMAGE_QUALITY_NORMAL,
		IMAGE_QUALITY_HIGH
	};

	SVGPainter();
	~SVGPainter();

	OP_STATUS BeginPaint(VEGARenderer* renderer, VEGARenderTarget* rendertarget, const OpRect& area);
	void EndPaint();

#ifdef PI_VIDEO_LAYER
	void SetDocumentPos(const AffinePos& doc_pos) { m_doc_pos = doc_pos; }
	const AffinePos& GetDocumentPos() const { return m_doc_pos; }
#endif // PI_VIDEO_LAYER

	OP_STATUS SetAntialiasingQuality(unsigned int quality);
	void SetFlatness(SVGNumber flatness) { m_flatness = flatness; }
	SVGNumber GetFlatness() const { return m_flatness; }

	VEGARenderer* GetRenderer() const { return m_renderer; }

	OP_STATUS SetClipRect(const OpRect& cliprect);
	const OpRect& GetClipRect() { return m_clip_extents; }

	OP_STATUS PushTransform(const SVGMatrix& ctm);
	const SVGMatrix& GetTransform() const { return m_transform; }
	void SetTransform(const SVGMatrix& transform) { m_transform = transform; }
	void PopTransform();

	void SetObjectProperties(SVGObjectProperties* oprops) { m_object_props = oprops; }
	void SetStrokeProperties(SVGStrokeProperties* sprops) { m_stroke_props = sprops; }

	void SetFillPaint(const SVGPaintDesc& fillpaint) { m_paints[0] = fillpaint; }
	void SetStrokePaint(const SVGPaintDesc& strokepaint) { m_paints[1] = strokepaint; }

	void SetImageOpacity(unsigned img_opacity) { m_imageopacity = img_opacity; }

#ifdef SVG_SUPPORT_STENCIL
	OP_STATUS AddClipRect(const SVGRect& cliprect);
	OP_STATUS BeginClip();
	void EndClip();
	void RemoveClip();

	OP_STATUS BeginMask();
	void EndMask();
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_FILTERS
	OP_STATUS PushBackgroundLayer(const OpRect& extents, BOOL is_new, UINT8 opacity);
	OP_STATUS PopBackgroundLayer();
	OP_STATUS GetBackgroundLayer(VEGARenderTarget** bgi_rt, OpRect& bgi_area, const SVGRect& canvas_region);

	void EnableBackgroundLayers(BOOL enable) { m_bg_layers_enabled = enable; }
#endif // SVG_SUPPORT_FILTERS

	/**
	 * Push a new layer onto the layer stack.
	 *
	 * @param extents The visible extents of the layer
	 * @param opacity The opacity of the layer
	 *
	 * @return OpBoolean::IS_TRUE if the layer was added. OpBoolean::IS_FALSE if the area
	 * of the layer is empty.
	 */
	OP_BOOLEAN PushLayer(const SVGBoundingBox& extents, UINT8 opacity);
	OP_STATUS PopLayer();

	void Clear(UINT32 color, const OpRect* pixel_clear_rect);

	OP_STATUS DrawLine(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2);
	OP_STATUS DrawRect(SVGNumber x, SVGNumber y, SVGNumber w, SVGNumber h, SVGNumber rx, SVGNumber ry);
	OP_STATUS DrawEllipse(SVGNumber x, SVGNumber y, SVGNumber rx, SVGNumber ry);
	OP_STATUS DrawPolygon(SVGVector &list, BOOL closed);
	OP_STATUS DrawPath(const OpBpath *path, SVGNumber path_length = -1);
	OP_STATUS DrawImage(OpBitmap* bitmap, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality);
	OP_STATUS DrawImage(VEGARenderTarget* image_rt, const OpRect& source, const SVGRect& dest, ImageRenderQuality quality);

	void DrawRectWithPaint(const SVGRect& rect, const SVGPaintDesc& fill);

#ifdef VEGA_OPPAINTER_SUPPORT
	void SetPainterRect(const OpRect& rect) {}

	OP_STATUS GetPainter(unsigned int font_overhang) { return OpStatus::OK; }
	void ReleasePainter(OpBitmap** resultbitmap = NULL) {}
	BOOL IsPainterActive() { return TRUE; }

	void DrawStringPainter(OpFont* font, const OpPoint& pos, const uni_char* text, unsigned len, int extra_char_spacing, int* caret_x = NULL);
	void DrawSelectedStringPainter(const OpRect& selected, COLORREF sel_bgcolor, COLORREF sel_fgcolor, OpFont* font, const OpPoint& pos, const uni_char* text, unsigned len, int extra_char_spacing);
#else
	void SetPainterRect(const OpRect& rect) { m_painter_rect = rect; }

	OP_STATUS GetPainter(unsigned int font_overhang);
	void ReleasePainter(OpBitmap** resultbitmap = NULL);
	BOOL IsPainterActive() { return m_painter != NULL; }

	void DrawStringPainter(OpFont* font, const OpPoint& pos, const uni_char* text, unsigned len, int extra_char_spacing, int* caret_x = NULL);
	void DrawSelectedStringPainter(const OpRect& selected, COLORREF sel_bgcolor, COLORREF sel_fgcolor, OpFont* font, const OpPoint& pos, const uni_char* text, unsigned len, int extra_char_spacing);

	OP_STATUS GetPainter(const OpRect& rect, OpPainter*& painter);
#endif // VEGA_OPPAINTER_SUPPORT

	BOOL IsVisible(const OpRect& pixel_extents) const
	{
		return m_clip_extents.Intersecting(pixel_extents);
	}

#ifdef PI_VIDEO_LAYER
	void ClearRect(const OpRect& rect, UINT32 color);
	void ApplyClipping(OpRect& rect);

	BOOL AllowOverlay() const;
	void SetAllowOverlay(BOOL allow) { m_allow_overlay = allow; }
#endif // PI_VIDEO_LAYER

#ifdef SVG_SUPPORT_STENCIL
	StencilMode GetClipMode() { return m_clip_mode; }
	void SetClipMode(StencilMode mode) { m_clip_mode = mode; }
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_STENCIL
	void ApplyClipToRegion(OpRect& region);
#endif // SVG_SUPPORT_STENCIL

	OP_STATUS BeginDrawing(SVGPaintNode* context_node);
	OP_STATUS EndDrawing();

	void Reset();

private:
	/** Helper for SetRootSurface */
	void MoveArea(const OpRect& common_area, const OpPoint& old_area_offset);
	void GetCurrentOffset(OpPoint& curr_ofs);

	void SetupPaint(unsigned paint_index);

#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS SyncPainter() { return OpStatus::OK; }
#else
	OP_STATUS GetPainterInternal(const OpRect& rect, OpPainter*& painter);
	OP_STATUS SyncPainter();

	OpRect m_current_painter_rect;	// This is the clipped and otherwise adapted version
	OpRect m_painter_rect;			// This is the unmodified version
	OpBitmap* m_painter_bitmap;		// The OpBitmap to which the OpPainter draws
	OpPainter* m_painter;			// An OpPainter used to render text
	BOOL m_painter_bitmap_is_dirty;	// Flag indicating whether the painter bitmap has been painted upon

	OP_STATUS LowGetPainter(const OpRect& rect, OpPainter*& painter);
	void ReleasePainter(const OpRect& damagedRect, OpBitmap** resultbitmap);

#ifdef SVG_BROKEN_PAINTER_ALPHA
	void FixupPainterAlpha(OpBitmap* painterbm, const OpRect& damagedRect);
	void FillRect(const OpRect& rect, UINT32 color);
	UINT32 m_last_painter_fillcolor;
#endif // SVG_BROKEN_PAINTER_ALPHA
#endif // VEGA_OPPAINTER_SUPPORT

	OP_STATUS SelectStencil(const OpRect& dstrect, VEGAStencil*& stencil);
	BOOL SetRenderTarget(VEGARenderTarget* rt, const OpRect& area);

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
	};

#ifdef SVG_SUPPORT_FILTERS
	OP_STATUS RenderToBackgroundImage(VPrimitive& prim);
#endif // SVG_SUPPORT_FILTERS
	OP_STATUS RenderPrimitive(VPrimitive& prim);

	OP_STATUS CreateStrokePath(VEGAPath& vpath, VEGAPath& vstrokepath, VEGA_FIX precompPathLength = VEGA_INTTOFIX(-1));
	void GetSurfaceTransform(VEGATransform& trans);
#ifdef SVG_SUPPORT_PAINTSERVERS
	OP_STATUS SetupPaintServer(SVGPaintServer* pserver, SVGPaintNode* context_node, VEGAFill* &vfill);
#endif // SVG_SUPPORT_PAINTSERVERS

	struct ImageInfo
	{
		ImageInfo() : img(NULL), need_free(FALSE) {}

		void Reset();

		OP_STATUS Setup(VEGARenderer* rend, OpBitmap* bitmap);
		OP_STATUS Setup(VEGARenderer* rend, VEGARenderTarget* rt);

		SVGRect dest;
		OpRect source;

		VEGAFill* img;

		SVGPainter::ImageRenderQuality quality;
		BOOL need_free;
		BOOL is_premultiplied;
	};

	ImageInfo m_imageinfo;

	void SetupImageSource(const OpPoint& base_ofs);
	OP_STATUS ProcessImage(const SVGRect& dest);

	OP_STATUS FillStrokePath(VEGAPath& vpath, VEGA_FIX pathLength);
	OP_STATUS FillStrokePrimitive(VEGAPrimitive& vprimitive);

	VEGARenderTarget* GetLayer();
	const OpRect& GetLayerArea();

	void SetVegaTransform();

	VEGARenderer* m_renderer;
	VEGARenderTarget* m_rendertarget;
	VEGATransform m_vtransform;
	OpRect m_area;
	OpRect m_clip_extents;
	OpPoint m_curr_layer_pos;
#ifdef PI_VIDEO_LAYER
	AffinePos m_doc_pos;
#endif // PI_VIDEO_LAYER

	enum
	{
		FILL_PAINT = 0,
		STROKE_PAINT = 1
	};

#ifdef SVG_SUPPORT_PAINTSERVERS
	VEGAFill* m_fills[2];
#endif // SVG_SUPPORT_PAINTSERVERS

	BOOL IsLayer() const { return !m_layers.Empty(); }

	struct Layer : public ListElement<Layer>
	{
		Layer() :
			target(NULL),
#ifdef SVG_SUPPORT_STENCIL
			mask(NULL),
#endif // SVG_SUPPORT_STENCIL
			opacity(255),
			owns_target(TRUE) {}
		~Layer()
		{
			if (owns_target)
				VEGARenderTarget::Destroy(target);
#ifdef SVG_SUPPORT_STENCIL
			VEGARenderTarget::Destroy(mask);
#endif // SVG_SUPPORT_STENCIL
		}

		OpRect area;
		VEGARenderTarget* target;
#ifdef SVG_SUPPORT_STENCIL
		VEGAStencil* mask;
#endif // SVG_SUPPORT_STENCIL
		unsigned opacity:8;
		unsigned owns_target:1;
	};

	AutoDeleteList<Layer> m_layers;
#ifdef SVG_SUPPORT_FILTERS
	BOOL HasBackgroundLayer() const { return !m_bg_layers.Empty(); }

	struct BILayer : public ListElement<BILayer>
	{
		BILayer() : target(NULL), is_new(FALSE), opacity(255) {}
		~BILayer() { VEGARenderTarget::Destroy(target); }

		OpRect area;
		VEGARenderTarget* target;
		BOOL is_new;
		UINT8 opacity;
	};

	AutoDeleteList<BILayer> m_bg_layers;
	BOOL m_bg_layers_enabled;
#endif // SVG_SUPPORT_FILTERS
#ifdef SVG_SUPPORT_STENCIL
public:
	struct Stencil : public ListElement<Stencil>
	{
		Stencil(VEGAStencil* in_stencil, StencilMode in_prev_mode) :
			stencil(in_stencil), prev_mode(in_prev_mode) {}
		~Stencil() { VEGARenderTarget::Destroy(stencil); }

		OpRect area;
		VEGAStencil* stencil;
		StencilMode prev_mode;
	};

private:
	/**
	 * Pop a stencil off a stencil stack.
	 *
	 * @param stencil_stack The stack.
	 *
	 * @return OpStatus::OK if successful.
	 */
	Stencil* PopStencil(AutoDeleteList<Stencil>& stencil_stack);

	void ReleaseStencil(Stencil* stencil);

	/**
	 * Get the topmost (VEGA) stencil in the stack
	 *
	 * @param stencil_stack The stack/vector
	 *
	 * @return A pointer to the stencil, or NULL if no stencils.
	 */
	VEGAStencil* GetStencil(AutoDeleteList<Stencil>& stencil_stack) const
	{
		if (Stencil* vega_stencil = stencil_stack.Last())
			return vega_stencil->stencil;

		return NULL;
	}
	/**
	 * Get the topmost clip stencil that is actually a stencil (stencil != NULL)
	 *
	 * @return A pointer to the stencil, or NULL if no stencils.
	 */
	VEGAStencil* GetActualClipStencil() const
	{
		Stencil* vega_stencil = m_active_clips.Last();
		while (vega_stencil)
		{
			if (vega_stencil->stencil)
				return vega_stencil->stencil;

			vega_stencil = vega_stencil->Pred();
		}
		return NULL;
	}

	BOOL GetClipRect(OpRect& cr);

	VEGAStencil* m_cached_stencil;

	VEGAStencil* GetStencil();
	BOOL PutStencil(VEGAStencil* stencil);
	void ClearStencilCache();
#endif // SVG_SUPPORT_STENCIL

	StencilMode m_clip_mode;

#ifdef SVG_SUPPORT_STENCIL
	AutoDeleteList<Stencil> m_active_clips;
	AutoDeleteList<Stencil> m_partial_clips;
#endif // SVG_SUPPORT_STENCIL

    StencilMode m_mask_mode;

#ifdef SVG_SUPPORT_STENCIL
    AutoDeleteList<Stencil> m_masks;
#endif // SVG_SUPPORT_STENCIL

	VEGA_FIX CalculateFlatness();

	OP_STATUS PushTransformBlock();
	void PopTransformBlock();
	void CleanTransformStack();

	struct TransformBlock
	{
		TransformBlock(TransformBlock* parent) : next(parent), count(0) {}

		TransformBlock* next;
		unsigned count;

		enum { MAX_XFRM_PER_BLOCK = 8 };

		SVGMatrix transform[MAX_XFRM_PER_BLOCK];
	};
	TransformBlock* m_transform_stack;
	SVGMatrix m_transform;

	SVGPaintDesc m_paints[2];
	SVGStrokeProperties* m_stroke_props;
	SVGObjectProperties* m_object_props;
	SVGNumber m_flatness;

	unsigned m_current_quality;
	unsigned m_imageopacity;

#ifdef PI_VIDEO_LAYER
	BOOL m_allow_overlay;
#endif // PI_VIDEO_LAYER
};

#endif // SVG_SUPPORT
#endif // SVGPAINTER_H
