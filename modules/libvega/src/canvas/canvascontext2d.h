/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CANVASCONTEXT2D_H
#define CANVASCONTEXT2D_H

#ifdef CANVAS_SUPPORT

class Canvas;
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegatransform.h"
#include "modules/libvega/vegastencil.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegafilter.h"

#ifdef CANVAS_TEXT_SUPPORT
#include "modules/style/css_all_values.h"
#include "modules/style/css_property_list.h"
#endif // CANVAS_TEXT_SUPPORT

#include "modules/util/simset.h"

class DOM_Object;

/** Base class for 'fills' of different kinds (currently gradients and
 * patterns), with the primary reason being to optimize GC tracing of
 * the context. */
class CanvasFill : public ListElement<CanvasFill>
{
public:
	CanvasFill() : m_dom_object(NULL) {}

	void SetDOM_Object(DOM_Object* dom_object) { m_dom_object = dom_object; }
	DOM_Object* GetDOM_Object() const { return m_dom_object; }

private:
	DOM_Object* m_dom_object;
};

/** Representation of a linear or radial gradient used by the canvas. */
class CanvasGradient : public CanvasFill
{
public:
	CanvasGradient();
	~CanvasGradient();

	/** Add a new color stop to the gradient.
	 * @param offset the offset of this stop.
	 * @param color the color of this stop.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS addColorStop(double offset, UINT32 color);

	/** Initialize this gradient as a linear gradient.
	 * @param x1 the x coordinate of the start point.
	 * @param y1 the y coordinate of the start point.
	 * @param x2 the x coordinate of the end point.
	 * @param y2 the y coordinate of the end point.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS initLinear(double x1, double y1, double x2, double y2);
	/** Initialize this gradient as a radial gradient.
	 * @param x1 the x coordinate of the start circle.
	 * @param y1 the y coordinate of the start circle.
	 * @param r1 the radius of the start circle.
	 * @param x2 the x coordinate of the end circle.
	 * @param y2 the y coordinate of the end circle.
	 * @param r2 the radius of the end circle.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS initRadial(double x1, double y1, double r1, double x2, double y2, double r2);

	/** Get the vega representation of this gradient. Used for painting it.
	 * @param trans the transform to apply to the gradient.
	 * @returns the fill object for use in libvega. */
	VEGAFill *getFill(VEGARenderer* renderer, const VEGATransform &trans);

	/** Estimate the storage (in bytes) allocated by the gradient argument. */
	static unsigned int allocationCost(CanvasGradient *gradient);

private:
	VEGA_FIX m_values[6];
	BOOL m_radial;

	VEGA_FIX *m_stopOffsets;
	unsigned int *m_stopColors;
	unsigned int m_numStops;
	VEGAFill* m_gradfill;
};
/** Representation of a pattern. A pattern is an image used to fill a polygon. */
class CanvasPattern : public CanvasFill
{
public:
	CanvasPattern();
	~CanvasPattern();

	/** Initialize the pattern.
	 * @param bmp the bitmap to use as a fill.
	 * @param xrep the repeat rule in x direction.
	 * @param yrep the repeat rule in y direction.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS init(OpBitmap *bmp, VEGAFill::Spread xrep, VEGAFill::Spread yrep);

	/** Get the vega representation of this pattern. Used for painting it.
	 * @param trans the transform to apply to the pattern.
	 * @returns the fill object for use in libvega. */
	VEGAFill *getFill(VEGARenderer* renderer, const VEGATransform &trans);

	/** Estimate the storage (in bytes) allocated by the pattern argument. */
	static unsigned int allocationCost(CanvasPattern *pattern);

private:
	OpBitmap* m_bitmap;
	VEGAFill::Spread m_xrepeat;
	VEGAFill::Spread m_yrepeat;
	VEGAFill* m_imgfill;
};

/** The context which is used for painting 2dimentional vector graphics on a
 * canvas. */
class CanvasContext2D
{
public:
	CanvasContext2D(Canvas *can, VEGARenderer* rend);
	~CanvasContext2D();

	/** Add a reference to this context. */
	void Reference();
	/** Remove a reference to this context. Will delete the context if the
	 * reference count reaches zero. */
	void Release();

	/** Remove the reference to the owning canvas, used when the canvas is
	 * deleted. */
	void ClearCanvas();

	/** Get a list of gradients and patterns ('fills') currently being
	 * referenced by the context. */
	void GetReferencedFills(List<CanvasFill>* fills);

	/** Initialize the vega object with a buffer representing the image
	 * in the canvas.
	 * @param buffer the buffer data.
	 * @param w the width of the buffer.
	 * @param h the height of the buffer.
	 * @param stride the stride between scanlines in bytes.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS initBuffer(VEGARenderTarget* rt);

	// Functions of the actual implementation
	/** Save the current state of the canvas in a stack.
	 * @returns OpStatus::OK on success, an error otherwise. */
	OP_STATUS save();
	/** Resetore the state from the top of the stack and pop the stack.
	 * If the stack is empty the default values will be restored instead. */
	void restore();

	/** Add a scale to the current transform. Will be applied first. */
	void scale(double x, double y);
	/** Add a rotation to the current transform. Will be applied first. */
	void rotate(double angle);
	/** Add a translation to the current transform. Will be applied first. */
	void translate(double x, double y);
	/** Add a new transform to the current transform. Will be applied first. */
	void transform(double m11, double m12, double m21, double m22, double dx, double dy);
	/** Replace the current transform with a new one. */
	void setTransform(double m11, double m12, double m21, double m22, double dx, double dy);

	/** Clear a part of the canvas to transparent black. */
	void clearRect(double x, double y, double w, double h);
	/** Fill a rect with the current fill color or gradient/pattern. */
	void fillRect(double x, double y, double w, double h);
	/** Stroke the outline of a rect with current stroke color or
	 * gradient/pattern and current line width. */
	void strokeRect(double x, double y, double w, double h);

	/** Begin a new empty path. */
	OP_STATUS beginPath();
	/** Close the current sub-path. */
	OP_STATUS closePath();
	/** Move to a new point without draing a visible line. Also starts a
	 * new sub path. */
	OP_STATUS moveTo(double x, double y);
	/** Draw a visible line ot a new point. */
	OP_STATUS lineTo(double x, double y);
	/** Draw a quadratic curve to a point. */
	OP_STATUS quadraticCurveTo(double cx, double cy, double x, double y);
	/** Draw a cubic bezier curve to a new point .*/
	OP_STATUS bezierCurveTo(double c1x, double c1y, double c2x, double c2y, double x, double y);
	/** Draw an arc to a new point. */
	OP_STATUS arcTo(double x1, double y1, double x2, double y2, double radius);
	/** Add a rect in it's own subpath to the current path. */
	OP_STATUS rect(double x, double y, double w, double h);
	/** Add an arc to the current path. */
	OP_STATUS arc(double x, double y, double radius, double startang, double endang, BOOL ccw);

#ifdef CANVAS_TEXT_SUPPORT
	/** Construct a path from the glyphs of text and fill it */
	void fillText(FramesDocument* frm_doc, HTML_Element* element, const uni_char* text, double x, double y, double maxwidth);
	/** Construct a path from the glyphs of text and stroke it */
	void strokeText(FramesDocument* frm_doc, HTML_Element* element, const uni_char* text, double x, double y, double maxwidth);
	/** Measure the text */
	OP_STATUS measureText(FramesDocument* frm_doc, HTML_Element* element, const uni_char* text, double* text_extent);
#endif // CANVAS_TEXT_SUPPORT

	/** Draw an image at the specified position. */
	OP_STATUS drawImage(OpBitmap *bmp, double *src, double *dst);

	/** Draw an image at the specified position. */
	OP_STATUS drawImage(VEGAFill *fill, double *src, double *dst);

	/** Fill the current path with the current fill style. */
	void fill();
	/** Stroke the current path with the current stroke style. */
	void stroke();
	/** Use the current path as clipping for the following operations. */
	void clip();

	/** Check if a point is inside the current path.
	 * @returns TRUE if the path contains the point, FALSE otherwise. */
	BOOL isPointInPath(double x, double y);

	/** Put a block of raster data as denoted by (x, y, w, h) */
	void putImageData(int dx, int dy, int sx, int sy, int sw, int sh, UINT8 *pixels, unsigned datastride);
	/** Get a block of raster data as denoted by (x, y, w, h) */
	void getImageData(int x, int y, int w, int h, UINT8* pixels);

	// Get and set functions
	enum CanvasCompositeOperation {CANVAS_COMP_SRC_ATOP = 0, CANVAS_COMP_SRC_IN,
			CANVAS_COMP_SRC_OUT, CANVAS_COMP_SRC_OVER, CANVAS_COMP_DST_ATOP,
			CANVAS_COMP_DST_IN, CANVAS_COMP_DST_OUT, CANVAS_COMP_DST_OVER,
			CANVAS_COMP_DARKER, CANVAS_COMP_LIGHTER, CANVAS_COMP_COPY,
			CANVAS_COMP_XOR};
	enum CanvasLineCap {CANVAS_LINECAP_ROUND, CANVAS_LINECAP_SQUARE, CANVAS_LINECAP_BUTT};
	enum CanvasLineJoin {CANVAS_LINEJOIN_ROUND, CANVAS_LINEJOIN_BEVEL, CANVAS_LINEJOIN_MITER};
#ifdef CANVAS_TEXT_SUPPORT
	enum CanvasTextAlign
	{
		CANVAS_TEXTALIGN_START,
		CANVAS_TEXTALIGN_END,
		CANVAS_TEXTALIGN_LEFT,
		CANVAS_TEXTALIGN_RIGHT,
		CANVAS_TEXTALIGN_CENTER
	};
	enum CanvasTextBaseline
	{
		CANVAS_TEXTBASELINE_TOP,
		CANVAS_TEXTBASELINE_HANGING,
		CANVAS_TEXTBASELINE_MIDDLE,
		CANVAS_TEXTBASELINE_ALPHABETIC,
		CANVAS_TEXTBASELINE_IDEOGRAPHIC,
		CANVAS_TEXTBASELINE_BOTTOM
	};
#endif // CANVAS_TEXT_SUPPORT
	/** Set the global alpha which is applied to all drawing. */
	void setAlpha(double alpha);
	/** Set the composite operation, which specifies how the draw operations
	 * blend with previous results. */
	void setGlobalCompositeOperation(CanvasCompositeOperation comp);
	/** Set the current stroke color. Will disable gradients and patterns. */
	void setStrokeColor(UINT32 color);
	/** Set currect stroke style to a gradient. */
	void setStrokeGradient(CanvasGradient *gradient);
	/** Set current stroke style to a pattern. */
	void setStrokePattern(CanvasPattern *pattern);
	/** Set the current fill color. Will disable gradients and patterns. */
	void setFillColor(UINT32 color);
	/** Set current fill style to a gradient. */
	void setFillGradient(CanvasGradient *gradient);
	/** Set current fill style to a pattern. */
	void setFillPattern(CanvasPattern *pattern);
	/** Set the current line width to use for stroking a path. */
	void setLineWidth(double width);
	/** Set the current line cap to use for stroking a path. */
	void setLineCap(CanvasLineCap cap);
	/** Set the current line join to use for stroking a path. */
	void setLineJoin(CanvasLineJoin join);
	/** Set the current mieter limit to use if using mieter joins. */
	void setMiterLimit(double miter);
	/** Set the x offset of shadows. */
	void setShadowOffsetX(double x);
	/** Set the y offset of shadows. */
	void setShadowOffsetY(double y);
	/** Set the blur radius of shadows. */
	void setShadowBlur(double blur);
	/** Set the color of shadows. */
	void setShadowColor(UINT32 color);
#ifdef CANVAS_TEXT_SUPPORT
	/** Set font to use for text */
	void setFont(CSS_property_list* font, FramesDocument* frm_doc, HTML_Element* element);
	/** Set the (dominant?) baseline of text */
	void setTextBaseline(CanvasTextBaseline baseline);
	/** Set the alignment of text */
	void setTextAlign(CanvasTextAlign align);
#endif // CANVAS_TEXT_SUPPORT

	double getAlpha();
	CanvasCompositeOperation getGlobalCompositeOperation();
	UINT32 getStrokeColor();
	CanvasGradient *getStrokeGradient(int stackDepth);
	CanvasPattern *getStrokePattern(int stackDepth);
	UINT32 getFillColor();
	CanvasGradient *getFillGradient(int stackDepth);
	CanvasPattern *getFillPattern(int stackDepth);
	double getLineWidth();
	CanvasLineCap getLineCap();
	CanvasLineJoin getLineJoin();
	double getMiterLimit();
	double getShadowOffsetX();
	double getShadowOffsetY();
	double getShadowBlur();
	UINT32 getShadowColor();
#ifdef CANVAS_TEXT_SUPPORT
	CSS_property_list* getFont();
	CanvasTextBaseline getTextBaseline();
	CanvasTextAlign getTextAlign();
#endif // CANVAS_TEXT_SUPPORT

	/** Get the canvas which this context is painting on. */
	Canvas *getCanvas();
	/** Get the depth of the current state stack. */
	int getStateStackDepth();
	/** Restore the defaults for all values. */
	void setDefaults();

	/** Estimate the storage (in bytes) allocated by the canvas context argument. */
	static unsigned int allocationCost(CanvasContext2D *context);

private:
	OP_STATUS partialArc(VEGA_FIX vx, VEGA_FIX vy, VEGA_FIX vradius,
						 VEGA_FIX vstartang, VEGA_FIX vdeltaang, BOOL skipMove);

	/** Clear the area specified by [dx, dy, dw, dh] on the render
	 * target dst limiting it by clip (can be NULL). */
	void clearClipHelper(VEGARenderTarget* dst, int dx, int dy, int dw, int dh,
						 VEGAStencil* clip);
	/** Clear the area occupied by the intersection of the path passed
	 * as an argument and the current clip (if any). */
	void clearRectHelper(VEGAPath& path);

	OP_STATUS createShadow(VEGARenderTarget* src_rt,
						   unsigned int src_width, unsigned int src_height,
						   VEGARenderTarget** out_shadow);
	void calcShadowArea(OpRect& srcrect);
	bool shouldDrawShadow() const;

	OP_STATUS cloneSurface(VEGARenderTarget*& clone, VEGARenderTarget* src,
						   int sx, int sy, int sw, int sh);
	OP_STATUS copySurface(VEGARenderTarget* dst, VEGARenderTarget* src,
						  int sx, int sy, int sw, int sh, int dx, int dy);
	OP_STATUS compositeGeneric(VEGARenderTarget* dst, VEGARenderTarget* src,
							   int sx, int sy, int sw, int sh, int dx, int dy);
	OP_STATUS compositeZPSrc(VEGARenderTarget* dst, VEGARenderTarget* src,
							 int sx, int sy, int sw, int sh, int dx, int dy);

	/** Reuse or create a new layer to paint on. If use_stencil is
	  * false the returned layer may be bigger than requested size.
	  * @param width the needed width of the layer
	  * @param height the needed height of the layer
	  * @param use_stencil true if painting will use stencil, makes
	  * sure the returned layer is exactly 'width' x 'height' px big
	 */
	VEGARenderTarget* getLayer(int width, int height, bool use_stencil);
	void putLayer(VEGARenderTarget* layer);

#ifdef CANVAS_TEXT_SUPPORT
	OP_STATUS getTextOutline(const struct CanvasTextContext& ctx, const uni_char* text, double x, double y, double maxwidth,
							 VEGAPath& voutline, VEGATransform& outline_transform);
	VEGA_FIX calculateBaseline(OpFont* font);

#ifdef VEGA_OPPAINTER_SUPPORT
	void fillRasterText(const CanvasTextContext& ctx, const uni_char* text,
						int px, int py);
	bool isFillColor() const
	{
		return
			m_current_state.fill.gradient == NULL &&
			m_current_state.fill.pattern == NULL;
	}
#endif // VEGA_OPPAINTER_SUPPORT
#endif // CANVAS_TEXT_SUPPORT

	Canvas *m_canvas;
	VEGARenderer* m_vrenderer;

public:
	// The struct PaintAttribute needs to be public to be able to use
	// it inside the struct FillParams and CanvasContext2D:
	struct PaintAttribute
	{
		CanvasGradient *gradient;
		CanvasPattern *pattern;
		unsigned int color;
	};

private:
	void setPaintAttribute(PaintAttribute* pa, const VEGATransform &trans);

	struct FillParams
	{
		VEGATransform image_transform;
		VEGAFill* image;

		PaintAttribute* paint;
	};

	void setFillParameters(FillParams& parms, const VEGATransform& ctm);
	OP_STATUS fillPath(FillParams& parms, VEGAPath& p);

	// variables used to store internal state
	struct CanvasContext2DState
	{
		VEGATransform transform;

		VEGAStencil* clip;

		unsigned char alpha;
		CanvasCompositeOperation compOperation;

		PaintAttribute stroke;
		PaintAttribute fill;

		VEGA_FIX lineWidth;
		VEGALineCap lineCap;
		VEGALineJoin lineJoin;
		VEGA_FIX miterLimit;
		VEGA_FIX shadowOffsetX;
		VEGA_FIX shadowOffsetY;
		VEGA_FIX shadowBlur;
		unsigned int shadowColor;

#ifdef CANVAS_TEXT_SUPPORT
		CSS_property_list* font;
		CanvasTextBaseline textBaseline;
		CanvasTextAlign textAlign;
#endif // CANVAS_TEXT_SUPPORT

		CanvasContext2DState *next;
	};

	CanvasContext2DState m_current_state;
	CanvasContext2DState *m_state_stack;

	CanvasContext2DState *getStateAtDepth(int stackDepth);

	VEGAPath m_current_path;
	bool m_current_path_started;

	VEGAFilter* m_composite_filter;
	VEGAFilter* m_copy_filter;

	VEGARenderTarget* m_buffer;
	VEGARenderTarget* m_offscreen_buffer;

	VEGARenderTarget* m_shadow_rt;
	VEGARenderTarget* m_shadow_color_rt;
	unsigned int m_current_shadow_color;
	VEGAFilter* m_shadow_filter_merge;
	VEGAFilter* m_shadow_filter_blur;
	VEGA_FIX m_shadow_filter_blur_radius;
	VEGAFilter* m_shadow_filter_modulate;
	VEGARenderTarget* m_layer;

	int m_ref_count;
};

#ifdef VEGA_USE_FLOATS
#define DOUBLE_TO_VEGA(x) ((VEGA_FIX)(x))
#define VEGA_TO_DOUBLE(x) (double(x))
#else
#define DOUBLE_TO_VEGA(x) ((VEGA_FIX)((x)*(double)(1<<VEGA_FIX_DECBITS) + .5))
#define VEGA_TO_DOUBLE(x) ((double)((double)(x) / (double)(1<<VEGA_FIX_DECBITS)))
#endif

#define CANVAS_FLATNESS DOUBLE_TO_VEGA(0.25)

#endif // CANVAS_SUPPORT

#endif // CANVASCONTEXT2D
