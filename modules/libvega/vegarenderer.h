/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGARENDERER_H
#define VEGARENDERER_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafixpoint.h"

#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegafilter.h"

class VEGAPath;
class VEGAStencil;
class VEGARenderTarget;
class VEGATransform;
#ifdef VEGA_OPPAINTER_SUPPORT
class VEGAWindow;
#endif // VEGA_OPPAINTER_SUPPORT

#include "modules/libvega/src/vegabackend.h"

struct VEGAPrimitive_Rect
{
	VEGA_FIX x, y;
	VEGA_FIX width, height;
};

struct VEGAPrimitive_RoundedRect
{
	VEGA_FIX x, y;
	VEGA_FIX width, height;

	const VEGA_FIX* corners;
};

struct VEGAPrimitive
{
	enum
	{
		/** An ordinary rectangle. */
		RECTANGLE,

		/** A rounded rectangle with uniform corners (all corners have
		 *  the same radii).
		 *  Expects VEGAPrimitive_RoundedRect::corners to point to a
		 *  VEGA_FIX[2]. */
		ROUNDED_RECT_UNIFORM
	};

	unsigned		type;		///< Type of primitive
	VEGA_FIX		flatness;	///< Flatness to use (if needed)
	VEGATransform*	transform;	///< Additional transform to apply (NULL if none)

	union
	{
		VEGAPrimitive_Rect			rect;
		VEGAPrimitive_RoundedRect	rrect;
	} data;
};

/** The main renderer class. This can be seen as the vega instance as almost
 * everything needs to pass through this class. */
class VEGARenderer
{
	friend class VEGAFilter;
public:
	VEGARenderer();
	~VEGARenderer();

	/** Initialize the renderer. It is allowed to call this function
	  * on an already initialized renderer to resize it.  When
	  * reinitializing the renderer to a different size, some
	  * render targets might require recreation or you will get an
	  * error when setting the render target.
	  * @param w the maximum width of the renderer.
	  * @param h the maximum height of the renderer.
	  * @param q the quality to use for this renderer.
	  * @param forcedBackend the rendering backend to use or BACKEND_NONE to choose automatically,
	  * the parameter will be used only during the first call and will be ignored on subsequent calls.
	  * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS Init(unsigned int w, unsigned int h, unsigned int q = VEGA_DEFAULT_QUALITY, unsigned int forcedBackend = LibvegaModule::BACKEND_NONE);

#ifdef VEGA_3DDEVICE
	/** @return the number of samples currently used by the backend */
	unsigned int getNumberOfSamples();
#endif // VEGA_3DDEVICE

#ifdef VEGA_OPPAINTER_SUPPORT
	/** Generate opera:gpu, presenting information about the active and avaliable backends.
	  * @param gpu the document
	  * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM */
	static OP_STATUS GenerateBackendInfo(class OperaGPU* gpu);
#endif // VEGA_OPPAINTER_SUPPORT

	/** Flush all rendering operations and make sure the result is presented.
	 * @param update_rects is an array of dirty rects that shall be flushed.
	 * @param num_rects is the number of items in the update_rects array. */
	void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0);

	/** Clear a rectangle in the render target to the specified color.
	 * @param x, y, w, h the rectangle to clear.
	 * @param color the color to clear to. */
	void clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform = NULL);

	/** Fill the specified predefined type of geometry.
	 * @param prim A description of the primitive to fill.
	 * @param stencil the stencil to use for clipping. */
	OP_STATUS fillPrimitive(VEGAPrimitive* prim, VEGAStencil* stencil);

	/** Fill the specified path.
	 * @param path the path to fill.
	 * @param stencil the stencil to use for clipping. */
	OP_STATUS fillPath(VEGAPath *path, VEGAStencil *stencil = NULL);

	/** Fill a rect in the render target.
	 * @param x, y, width, height the rectangle to fill.
	 * @param stencil the stencil to use for clipping.
	 * @param blend if true then the 'over' composite operation is used instead of the 'source' operation.
	 * Disabling blend is only supported if the fill is an image and no stencil is passed in. */
	OP_STATUS fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil = NULL, bool blend = true);

	/** Invert a rect in the render target.
	 * @param x, y, width, height the rectangle to invert. */
	OP_STATUS invertRect(int x, int y, unsigned int width, unsigned int height);

	/** Invert a path in the render target.
	 * @param path the path to invert. */
	OP_STATUS invertPath(VEGAPath* path);

	/** Move a rect in the render target a number of pixels in x
	 * and y direction.
	 * @param x, y, width, height the rectangle to move.
	 * @param dx, dy the offset to move the rectangle. */
	OP_STATUS moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy);

#ifdef VEGA_OPPAINTER_SUPPORT
	/** Draw an (axis/grid-aligned) image at the specified position */
	OP_STATUS drawImage(VEGAOpBitmap* vbmp, const VEGADrawImageInfo& imginfo, VEGAStencil* stencil = NULL);

	/** Draw a string to the current render target at the specified position. */
	OP_STATUS drawString(class VEGAFont* font, int x, int y, const uni_char* text, UINT32 len,
						 INT32 extra_char_space = 0, short word_width = -1,
						 VEGAStencil* stencil = NULL, VEGATransform* transform = NULL);
#endif // VEGA_OPPAINTER_SUPPORT

	/** Apply a filter to the current render target.
	 * @param filter the filter to apply.
	 * @param region the region to apply the filter to. */
	OP_STATUS applyFilter(VEGAFilter* filter, VEGAFilterRegion& region);

	/** Set (update) the rasterization quality this renderer should use. Same as passed to Init().
	 * @param q the quality to use for this renderer.
	 * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */
	OP_STATUS setQuality(unsigned int q);

	/** Get the rasterization quality this renderer use. */
	unsigned int getQuality() { return backend->quality; }

	/** Set the current color to fill with. Will reset any fill set by setFill to NULL.
	  * @param col the color. */
	void setColor(unsigned long col) { backend->setColor(col); }

	/** Use a more advanced fill then solid color, such as an image or gradient.
	  * @param f the fill. */
	void setFill(VEGAFill *f) { backend->setFill(f); }

	/** Specify if xor or zero/set filling should be used. */
	void setXORFill(bool xf) { backend->setXORFill(xf); }

	/** Set the current (rectangular) clipping rect. */
	void setClipRect(int x, int y, int w, int h) { backend->setClipRect(x, y, w, h); }

	/** Get the current (rectangular) clipping rect. */
	void getClipRect(int &x, int &y, int &w, int &h) { backend->getClipRect(x, y, w, h); }

	/** Get the area affected by the render operation. This is reset for every render call. */
	unsigned int getRenderMinX() { return backend->r_minx; }
	unsigned int getRenderMinY() { return backend->r_miny; }
	unsigned int getRenderMaxX() { return backend->r_maxx; }
	unsigned int getRenderMaxY() { return backend->r_maxy; }

	/** Set the render target to be used by the renderer.
	 * @param rt the render target to use. */
	void setRenderTarget(VEGARenderTarget* rt);

	/** Create a render target bound to a OpBitmap. The OpBitmap should
	 * not be used for anything else while the render target is alive since
	 * it might not be flushed until the render target is deleted. */
	OP_STATUS createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp);

#ifdef VEGA_OPPAINTER_SUPPORT
	/** Create a render target bound to a window. The window will be created
	 * by a platform callback.
	 * FIXME: this should include some create flags. */
	OP_STATUS createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window);

	/** Create a render target bound to a pixel store.
	 * It can only be used with a software backend. */
	OP_STATUS createBufferRenderTarget(VEGARenderTarget** rt, VEGAPixelStore* ps);
#endif // VEGA_OPPAINTER_SUPPORT

	/** Create an intermediate render target. This render target does not
	 * update a  bitmap, it can only be used internally. It can however be
	 * copied to a bitmap.
	 * Use this instead of bitmap render target if you do not require a
	 * bitmap or if you want double buffering.
	 * An intermediate render target may be smaller than the renderer,
	 * but never bigger.
	 * @param rt the output render target.
	 * @param w the width of the render target, set to 0 to use the
	 * renderers width.
	 * @param h the height of the render target, set to 0 to use the
	 * renderers height */
	OP_STATUS createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w = 0, unsigned int h = 0);

	/** Create a render target for the stencil buffer. The stencil can be
	 * used as a mask to fillPath once it has been rendered to. A stencil
	 * cannot be applied to a render target which is bigger than the
	 * stencil.
	 * @param rt the output render target.
	 * @param component use true to create component based mask.
	 * @param w the width of the render target, set to 0 to use the
	 * renderers width.
	 * @param h the height of the render target, set to 0 to use the
	 * renderers height */
	OP_STATUS createStencil(VEGAStencil** sten, bool component, unsigned int w = 0, unsigned int h = 0);

	/** Create a linear gradient.
	 * @param fill the pointer to store the resulting fill in.
	 * @param x1, y1 the start position of the gradient.
	 * @param x2, y2 the end position of the gradient.
	 * @param numStops the number of stops in this gradient.
	 * @param stopOffset an array of numStops offsets for the stops.
	 * @param stopColors an array of numStops colors for the stops.
	 * @param premultiplied true if the gradient should be
	 * interpolated using premultiplied colors. */
	OP_STATUS createLinearGradient(VEGAFill** fill, VEGA_FIX x1,
			VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
			unsigned int numStops, VEGA_FIX* stopOffsets,
			UINT32* stopColors, bool premultiplied = false);

	/** Create a radial gradient.
	 * @param fill the pointer to store the resulting fill in.
	 * @param x0, y0, r0 the inner circle of the gradient.
	 * @param x1, y1, r1 the outer circle of the gradient.
	 * @param numStops the number of stops in this gradient.
	 * @param stopOffset an array of numStops offsets for the stops.
	 * @param stopColors an array of numStops colors for the stops.
	 * @param premultiplied true if the gradient should be
	 * interpolated using premultiplied colors. */
	OP_STATUS createRadialGradient(VEGAFill** fill,
								   VEGA_FIX x0, VEGA_FIX y0, VEGA_FIX r0,
								   VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX r1,
								   unsigned int numStops, VEGA_FIX* stopOffsets,
								   UINT32* stopColors, bool premultiplied = false);

	/** Create an image fill from a bitmap.
	 * @param fill a pointer to store the fill in.
	 * @param bmp the bitmap to create a fill for.
	 * @param xspread the spread in x direction.
	 * @param yspread the spread in y direction.
	 * @param quality the quality of the fill (linear or nearest).
	 * @param invPathTransform the inverse of the transform being applied
	 * to the path rendered with this fill. */
	OP_STATUS createImage(VEGAFill** fill, OpBitmap* bmp);

#ifdef CANVAS3D_SUPPORT
	/** Create an image fill from a 3d texture. This will copy the image
	 * to software backend if needed. Only render target textures may
	 * be passed to this function. */
	OP_STATUS createImage(VEGAFill** fill, class VEGA3dTexture* tex, bool isPremultipled);
#endif // CANVAS3D_SUPPORT

	/** Create a merge filter which does not need any special
	 * parameters. Merge filters can not be applied to windows.
	 * @param filter the filter to create.
	 * @param type the type of merge filter to create. */
	OP_STATUS createMergeFilter(VEGAFilter** filter, VEGAMergeType type);

	/** Create an opacity merge filter.
	 * @param filter the filter to create.
	 * @param opacity the opacity to use for the merging. */
	OP_STATUS createOpacityMergeFilter(VEGAFilter** filter, unsigned char opacity);

	/** Create an arithmetic merge filter.
	 * @param filter the filter to create.
	 * @param k1, k2, k3, k4 the arithmetic merge coefficients. */
	OP_STATUS createArithmeticMergeFilter(VEGAFilter** filter, VEGA_FIX k1, VEGA_FIX k2, VEGA_FIX k3, VEGA_FIX k4);

	/** Create a convolve filter.
	 * @param filter the filter to create.
	 * @param kernel the convolve kernel data.
	 * @param kernelWidth, kernelHeight the size of the convolve kernel.
	 * @param kernelCenterX, kernelCenterY the center position of the kernel.
	 * @param divisor the divisor to use in the convolve.
	 * @param bias the bias to use in the convolve.
	 * @param edgeMode the wrapping mode when reading outside the image.
	 * @param preserveAlpha if true, the alpha channel will not be affected by the filtering. */
	OP_STATUS createConvolveFilter(VEGAFilter** filter, VEGA_FIX* kernel, unsigned int kernelWidth, unsigned int kernelHeight,
		unsigned int kernelCenterX, unsigned int kernelCenterY, VEGA_FIX divisor, VEGA_FIX bias, VEGAFilterEdgeMode edgeMode,
		bool preserveAlpha);

	/** Create a morphology filter.
	 * @param filter the filter to create.
	 * @param type the type of filter to create.
	 * @param radiusX, radiusY the morphology filter radius.
	 * @param wrap the edge mode to use for morphology filter. */
	OP_STATUS createMorphologyFilter(VEGAFilter** filter, VEGAMorphologyType type, int radiusX, int radiusY, bool wrap);

	/** Create a gaussian blur filter, this is an optimized special case of a convolve filter.
	 * @param filter the filter to create.
	 * @param deviationX, deviationY the blur deviation.
	 * @param wrap the edge mode to use for blurring. */
	OP_STATUS createGaussianFilter(VEGAFilter** filter, VEGA_FIX deviationX, VEGA_FIX deviationY, bool wrap);

	/** Create a filter for a distant light.
	 * @param filter the filter to create.
	 * @param type the light and parameters.
	 * @param xdir, ydir, zdir the direction of the light. */
	OP_STATUS createDistantLightFilter(VEGAFilter** filter, const VEGALightParameter& type,
		VEGA_FIX xdir, VEGA_FIX ydir, VEGA_FIX zdir);

	/** Create a filter for a point light.
	 * @param filter the filter to create.
	 * @param type the light and parameters.
	 * @param xpos, ypos, zpos the position of the light. */
	OP_STATUS createPointLightFilter(VEGAFilter** filter, const VEGALightParameter& type,
		VEGA_FIX xpos, VEGA_FIX ypos, VEGA_FIX zpos);

	/** Create a filter for a spot light.
	 * @param filter the filter to create.
	 * @param type the light and parameters.
	 * @param xpos, ypos, zpos the position of the light.
	 * @param xtgt, ytgt, ztgt the target of the light.
	 * @param spotSpecExp the specular exponent of the spot light.
	 * @param angle the limiting angle of the light in degrees. Set to >= 360 to not use limiting angle. */
	OP_STATUS createSpotLightFilter(VEGAFilter** filter, const VEGALightParameter& type,
		VEGA_FIX xpos, VEGA_FIX ypos, VEGA_FIX zpos,
		VEGA_FIX xtgt, VEGA_FIX ytgt, VEGA_FIX ztgt,
		VEGA_FIX spotSpecExp, VEGA_FIX angle);

	/** Create a luminance to alpha filter.
	 * @param filter the filter to create. */
	OP_STATUS createLuminanceToAlphaFilter(VEGAFilter** filter);

	/** Create a matrix based color transform filter.
	 * If the matrix is not null it will be set in the filter.
	 * If matrix is null it will on return contain a pointer
	 * to the matrix used in the filter which can be modified.
	 * @param filter the filter to create.
	 * @param matrix the matrix to use in the filter, or null
	 * to get a pointer to the matrix used in the filter. */
	OP_STATUS createMatrixColorTransformFilter(VEGAFilter** filter, VEGA_FIX* &matrix);

	/** Create a component transfer color transform filter.
	 * When creating a component color transform filter the table must be set up correctly.
	 * When using linear it should contain 2 elements - slope and intercept. When using
	 * gamma correction it should contains 3 elements - amplitude, exponent and offset.
	 * @param filter the filter to create. */
	OP_STATUS createComponentColorTransformFilter(VEGAFilter** filter,
		VEGACompFuncType red_type, VEGA_FIX* red_table, unsigned int red_table_size,
		VEGACompFuncType green_type, VEGA_FIX* green_table, unsigned int green_table_size,
		VEGACompFuncType blue_type, VEGA_FIX* blue_table, unsigned int blue_table_size,
		VEGACompFuncType alpha_type, VEGA_FIX* alpha_table, unsigned int alpha_table_size);

	/** Create a filter for colorspace conversion.
	 * @param filter the filter to create.
	 * @param from source color space.
	 * @param to destination color space. */
	OP_STATUS createColorSpaceConversionFilter(VEGAFilter** filter, VEGAColorSpace from, VEGAColorSpace to);

	// FIXME: might need to support image too
	/** Create a displacement mapping filter.
	 * @param filter the filter to create.
	 * @param displaceMap the displacement map to use in the filter.
	 * @param scale the scale factor for the displacement map.
	 * @param xComp, yComp which color components to use for x and y displacement. */
	OP_STATUS createDisplaceFilter(VEGAFilter** filter, VEGARenderTarget* displaceMap, VEGA_FIX scale,
		VEGAComponent xComp, VEGAComponent yComp);

private:
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS drawImageFallback(VEGAImage* img, const VEGADrawImageInfo& imginfo,
								VEGAStencil* stencil);

# ifdef SVG_PATH_TO_VEGAPATH
	OP_STATUS drawTransformedString(VEGAFont* font, int x, int y, const uni_char* _text, UINT32 len,
									INT32 extra_char_space, short adjust,
									VEGATransform* transform, VEGAStencil* stencil);
# endif // SVG_PATH_TO_VEGAPATH
#endif // VEGA_OPPAINTER_SUPPORT

	bool fillIfSimple(VEGAPath* path, VEGAStencil *stencil);
	void validateStencil(VEGAStencil*& stencil);

	VEGARendererBackend* backend;
};

#endif // VEGA_SUPPORT
#endif // VEGARENDERER_H

