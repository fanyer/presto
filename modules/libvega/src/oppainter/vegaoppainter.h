/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAOPPAINTER_H
#define VEGAOPPAINTER_H

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/pi/OpPainter.h"

#include "modules/libvega/vegatransform.h"
#include "modules/libvega/vegarenderer.h"

class VEGARenderTarget;
class VEGAWindow;

class VEGASprite;
class VEGAStencil;
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
class VEGAPrinterListener;
#endif // VEGA_PRINTER_LISTENER_SUPPORT

class VEGAOpPainter : public OpPainter
#ifdef VEGA_OPPAINTER_ANIMATIONS
	, public MessageObject
#endif // VEGA_OPPAINTER_ANIMATIONS
{
public:
	VEGAOpPainter();
	~VEGAOpPainter();

	OP_STATUS Construct(unsigned int width, unsigned int height, VEGAWindow* window = NULL);
	virtual OP_STATUS Resize(unsigned int width, unsigned int height);

	virtual void present(const OpRect* update_rects = NULL, unsigned int num_rects = 0);

	BOOL Supports(SUPPORTS supports);

	class FillState
	{
		friend class VEGAOpPainter;

		class VEGAFill* fill;
		UINT32 color;
	};

	void SetColor(UINT8 red, UINT8 green, UINT8 blue, UINT8 alpha);
	void SetFill(VEGAFill* fill) { fillstate.fill = fill; }

	virtual void SetPreAlpha(UINT8 alpha) { m_pre_alpha = alpha; }
	virtual UINT8 GetPreAlpha() { return m_pre_alpha; }

	void SetFillTransform(const VEGATransform& filltrans);
	void ResetFillTransform();

	UINT32 GetColor();

	const FillState& SaveFillState() const { return fillstate; }
	void RestoreFillState(const FillState& fstate) { fillstate = fstate; }

	VEGAFill* CreateLinearGradient(VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
								   unsigned int numStops, VEGA_FIX* stopOffsets, UINT32* stopColors, BOOL premultiplied = FALSE);
	VEGAFill* CreateRadialGradient(VEGA_FIX fx, VEGA_FIX fy, VEGA_FIX cx, VEGA_FIX cy, VEGA_FIX radius,
								   unsigned int numStops, VEGA_FIX* stopOffsets, UINT32* stopColors, BOOL premultiplied = FALSE);

	void SetFont(OpFont* font);

	OP_STATUS SetClipRect(const OpRect& rect);
	void RemoveClipRect();
	void GetClipRect(OpRect* rect);

	virtual OP_STATUS BeginStencil(const OpRect& rect);
	void BeginModifyingStencil();
	void EndModifyingStencil();
	virtual void EndStencil();

	void DrawRect(const OpRect& rect, UINT32 width);
	void FillRect(const OpRect& rect);
	void ClearRect(const OpRect& rect);
	void DrawEllipse(const OpRect& rect, UINT32 width);
	void FillEllipse(const OpRect& rect);
	void DrawPolygon(const OpPoint* points, int count, UINT32 width);

	void DrawLine(const OpPoint& from, UINT32 length, BOOL horizontal, UINT32 width);
	void DrawLine(const OpPoint& from, const OpPoint& to, UINT32 width);

	void DrawString(const OpPoint& pos, const uni_char* text, UINT32 len, INT32 extra_char_spacing, short word_width = -1);

	void InvertRect(const OpRect& rect);
	void InvertBorderRect(const OpRect& rect, int border);
	void InvertBorderEllipse(const OpRect& rect, int border);
	void InvertBorderPolygon(const OpPoint* point_array, int points, int border);

	void DrawBitmapClipped(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	void DrawBitmapClippedTransparent(const OpBitmap* bitmap, const OpRect& source, OpPoint p);
	void DrawBitmapClippedAlpha(const OpBitmap* bitmap, const OpRect& source, OpPoint p);

	virtual void DrawBitmapScaled(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual void DrawBitmapScaledTransparent(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);
	virtual void DrawBitmapScaledAlpha(const OpBitmap* bitmap, const OpRect& source, const OpRect& dest);

	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale);
	virtual OP_STATUS DrawBitmapTiled(const OpBitmap* bitmap, const OpPoint& offset, const OpRect& dest, INT32 scale, UINT32 bitmapWidth, UINT32 bitmapHeight);

	OpBitmap* CreateBitmapFromBackground(const OpRect& rect);
	OP_STATUS BeginOpacity(const OpRect& rect, UINT8 opacity);
	void EndOpacity();
	/**
	   gets the accumulated opacity by traversing the opacity stack
	   @return the accumulated opacity of all frames in the opacity stack
	 */
	UINT8 GetAccumulatedOpacity();

	// Fill a path. It is assumed that the path is not reused, since
	// this call most likely will modify it.
	OP_STATUS FillPath(class VEGAPath& path);

	// Fill a path. Makes a copy of the path passed in, so use the
	// above if you can.
	OP_STATUS FillPathImmutable(const class VEGAPath& path);

#ifdef CSS_TRANSFORMS
	void SetTransform(const VEGATransform& ctm)
	{
		current_transform = ctm;
		current_transform_is_set = true;
	}
	void ClearTransform() { current_transform_is_set = false; }
#endif // CSS_TRANSFORMS

	virtual void MoveRect(int x, int y, unsigned int w, unsigned int h, int dx, int dy);

	virtual void SetVegaTranslation(int x, int y);

	/** Value from 0-255 that will affect DrawBitmap functions. */
	void SetImageOpacity(int image_opacity) { m_image_opacity = image_opacity; }
	int GetImageOpacity() { return m_image_opacity; }

	void SetImageQuality(VEGAFill::Quality q) { m_image_quality = q; }
	VEGAFill::Quality GetImageQuality() const { return m_image_quality; }

	VEGARenderer* GetRenderer(){return m_renderer;}
	VEGARenderTarget* GetRenderTarget(){return m_renderTarget;}
	int GetTranslationX(){return GetLayerOffset().x;}
	int GetTranslationY(){return GetLayerOffset().y;}
	UINT32 GetCurrentColor() { return fillstate.color; }
	VEGAStencil *GetCurrentStencil();
	
	VEGASprite* CreateSprite(unsigned int w, unsigned int h);
	void DestroySprite(VEGASprite* spr);
	void AddSprite(VEGASprite* spr, int x, int y);
	void RemoveSprite(VEGASprite* spr);
	void SetSpriteData(VEGASprite* spr, UINT32* data, unsigned int stride);
	OP_STATUS CopyBackgroundToBitmap(OpBitmap* bmp, const OpRect& rect);

#ifdef VEGA_OPPAINTER_ANIMATIONS
	enum VegaAnimationEvent {
		ANIM_SHOW_WINDOW,
		ANIM_HIDE_WINDOW,
		ANIM_SWITCH_TAB,
		ANIM_LOAD_PAGE,
		ANIM_LOAD_PAGE_BACK,
		ANIM_LOAD_PAGE_FWD
	};

	OP_STATUS prepareAnimation(const OpRect& rect, VegaAnimationEvent evt);
	OP_STATUS startAnimation(const OpRect& rect);
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif // VEGA_OPPAINTER_ANIMATIONS

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	void setPrinterListener(VEGAPrinterListener* pl){m_printerListener = pl;}
	VEGAPrinterListener* getPrinterListener(){return m_printerListener;}
#endif // VEGA_PRINTER_LISTENER_SUPPORT

	virtual OP_STATUS PaintImage(class VEGAOpBitmap* vbmp, struct VEGADrawImageInfo& diinfo);

	virtual void SetRenderingHint(RenderingHint);
	virtual RenderingHint GetRenderingHint();

protected:
#ifdef CSS_TRANSFORMS
	bool HasTransform() const { return current_transform_is_set; }

	/** Get the transform to calculate the cliprect. Unlike GetCTM, layer offset
	 *  will not affect the result. */
	virtual void GetClipRectTransform(VEGATransform& trans) { trans = current_transform; }

	VEGATransform current_transform;
#endif // CSS_TRANSFORMS

	OpPoint GetLayerOffset() const
	{
		OpPoint lofs(translateX, translateY);

		if (opacityStack && m_modifying_stencil == 0)
		{
			OpRect lext = GetLayerExtent();
			lofs.x -= lext.x;
			lofs.y -= lext.y;
		}
		return lofs;
	}

	virtual void GetCTM(VEGATransform& trans);
	virtual OP_STATUS PaintRect(OpRect& r);
	OP_STATUS InvertShape(class VEGAPath& shape);

private:
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	bool PrinterNeedsFallback() const;
	void EmitPrinterFallback(const OpRect& r);
#endif // VEGA_PRINTER_LISTENER_SUPPORT

	VEGARenderer* m_renderer;
	VEGARenderTarget* m_renderTarget;
#ifdef VEGA_OPPAINTER_OPACITY_CACHE
	VEGARenderTarget* m_opacityRenderTargetCache;
#endif // VEGA_OPPAINTER_OPACITY_CACHE
	VEGAStencil* m_stencilCache;
	VEGAFilter* m_invertFilter;

	VEGAStencil* GetLayerStencil();

	OpRect GetLayerExtent() const;

	bool IsLayerCached(VEGARenderTarget* layer) const
	{
#ifdef VEGA_OPPAINTER_OPACITY_CACHE
		return layer == m_opacityRenderTargetCache;
#else
		return false;
#endif // VEGA_OPPAINTER_OPACITY_CACHE
	}

	OP_STATUS GetLayer(VEGARenderTarget*& layer, const OpRect& rect);
	void ClearLayer(VEGARenderTarget* layer, const OpRect& rect);
	void PutLayer(VEGARenderTarget* layer);

	void UpdateClipRect();
	OpRect GetCurrentClipRect() const;

	struct ClipStack
	{
		OpRect clip;
#ifdef CSS_TRANSFORMS
		bool has_stencil;
#endif // CSS_TRANSFORMS
		ClipStack* next;
	};
	ClipStack* clipStack;

	struct OpacityStack
	{
		VEGARenderTarget* renderTarget;
		OpRect rect;
		UINT8 opacity;
		OpacityStack* next;
	};
	OpacityStack* opacityStack;

	struct StencilStack
	{
		VEGARenderTarget* renderTarget;
		OpRect rect;
		VEGAStencil *stencil;
		StencilStack* next;
	};
	StencilStack* stencilStack;
	int m_modifying_stencil;

	FillState fillstate;
	int translateX, translateY;
	int m_image_opacity;
	VEGAFill::Quality m_image_quality;

	VEGATransform filltransform;
	bool filltransform_is_set;

	OP_STATUS PaintPath(class VEGAPath& path);

	void SetupFill()
	{
		if (fillstate.fill)
		{
			SetupComplexFill();
			return;
		}

		m_renderer->setColor(fillstate.color);
	}
	void SetupComplexFill();

#ifdef CSS_TRANSFORMS
	bool current_transform_is_set;
#endif // CSS_TRANSFORMS

#ifdef VEGA_OPPAINTER_ANIMATIONS
	VEGARenderTarget* m_animationRenderTarget;
	VEGARenderTarget* m_animationPreviousRenderTarget;

	BOOL m_isAnimating;
	double m_animationStart;
	OpRect animatedArea;
	enum {
		VEGA_ANIMATION_FADE,
		VEGA_ANIMATION_SLIDE_RIGHT,
		VEGA_ANIMATION_SLIDE_LEFT,
		VEGA_ANIMATION_SLIDE_UP
	} animationType;
	VegaAnimationEvent m_currentEvent;
#endif // VEGA_OPPAINTER_ANIMATIONS
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	VEGAPrinterListener* m_printerListener;
#endif // VEGA_PRINTER_LISTENER_SUPPORT
	UINT8 m_pre_alpha;
};

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT

#endif // !VEGAOPPAINTER_H

