/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef BACKGROUND_CLIPPING_H
#define BACKGROUND_CLIPPING_H

#include "modules/style/css_gradient.h"
#include "modules/layout/cssprops.h"

class VisualDevice;
class HTML_Element;

OpRect OpRectClip(const OpRect &this_rect, const OpRect &clip_rect);

/** BgRegion. Region class for AvoidOverdraw-optimization.
*/

class BgRegion
{
public:
	BgRegion();
	~BgRegion();

	/** Reset. The region will contain nothing after this call. If free_mem is TRUE it will also deallocate the rect array. */
	void Reset(BOOL free_mem = TRUE);

	/** Set the region to rect only. returns success status. */
	OP_STATUS Set(OpRect rect);

	/** Set the region to be the same as another region. returns success status.
		Note: If it fails, the region will be empty! */
	OP_STATUS Set(const BgRegion& rgn);

	/** Add a rectangle to the region. returns success status. */
	OP_STATUS AddRect(const OpRect& rect);

	/** Add a rectangle to the region if not contained in any other rects in the region. returns success status. */
	OP_STATUS AddRectIfNotContained(const OpRect& rect);

	/** Make sure no rect exceeds tile_side. */
	OP_STATUS Constrain(unsigned int tile_side);

	/** Remove a rectangle from the region. */
	void RemoveRect(int index);

	/** Add a rectangle to the region. If the rectangle is overlapping other rectangles in the region,
		rect will be sliced up so the region contains no overlapping rectangles. returns success status. */
	OP_STATUS IncludeRect(OpRect rect);

	/** Will split up the existing rectangles into smaller rectangles to exclude rect. returns success status.*/
	OP_STATUS ExcludeRect(OpRect rect, BOOL keep_hole);

	/** Extend bounding box. Make sure the bounding box in the region includes rect. */
	void ExtendBoundingRect(const OpRect& rect);

	/** Optimize the region.
		Note: The result is far from the most optimal in many cases. */
	void CoalesceRects();

	/** Return TRUE if the other region equals this one.
		Note: If the other region covers exactly the same area, it might still return FALSE if the internal representation
		of that area is different. */
	BOOL Equals(BgRegion& other);
public:
	OpRect *rects;
	int num_rects;
	OpRect bounding_rect;
private:
	int max_rects;
	OP_STATUS ExcludeRectInternal(OpRect rect, OpRect remove);
	OP_STATUS GrowIfNeeded();
};

#define MAX_BG_INFO 10

/**	BgInfo contains the necessary data to draw backgroundcolor or backgroundimages delayed. Used by BgClipStack. */

class BgInfo
{
public:
	typedef enum {
		COLOR,
		IMAGE,
#ifdef CSS_GRADIENT_SUPPORT
		GRADIENT
#endif // CSS_GRADIENT_SUPPORT
	} BgType;

	// Basics
	HTML_Element* element;
	OpRect rect;
	BgRegion region;
	BgType type;

	// Backgrounds
	COLORREF bg_color;
	Image img;
#ifdef CSS_GRADIENT_SUPPORT
	const CSS_Gradient* gradient;
#endif // CSS_GRADIENT_SUPPORT

	// Helpers
	OpPoint offset;
	ImageListener* image_listener;
	int imgscale_x;
	int imgscale_y;
	COLORREF current_color;
#ifdef CSS_GRADIENT_SUPPORT
	OpRect img_rect;
	OpRect repeat_space;
	CSSValue repeat_x,
		repeat_y;
#endif // CSS_GRADIENT_SUPPORT
	CSSValue image_rendering;
};

/** BgClipStack. Used to calculate covered backgrounds when painting a document. (AvoidOverdraw-optimization) */

class BgClipStack
{
public:
	BgClipStack();
	~BgClipStack();

	OP_STATUS Begin(VisualDevice *vd);
	void End(VisualDevice *vd);

	BgInfo *Add(VisualDevice *vd);
	void AddBg(HTML_Element* element, VisualDevice *vd, const COLORREF &bg_color, OpRect rect);
	void AddBg(HTML_Element* element, VisualDevice *vd, Image& img, OpRect rect, OpPoint offset, ImageListener* image_listener, int imgscale_x, int imgscale_y, CSSValue image_rendering);
#ifdef CSS_GRADIENT_SUPPORT
	void AddBg(HTML_Element* element, VisualDevice *vd, COLORREF current_color, const CSS_Gradient &gradient, OpRect rect, const OpRect &gradient_rect, const OpRect &repeat_space, CSSValue repeat_x, CSSValue repeat_y);
#endif // CSS_GRADIENT_SUPPORT

	void CoverBg(VisualDevice *vd, const OpRect& rect, BOOL keep_hole);

	void FlushBg(VisualDevice *vd, OpRect rect);
	void FlushBg(HTML_Element* element, VisualDevice *vd);

	void FlushAll(VisualDevice *vd);
	void FlushBg(VisualDevice *vd, int index);
	void FlushLast(VisualDevice *vd);

	OP_STATUS PushClipping(OpRect rect);
	void PopClipping();

	/** Get the current clipping set by PushClipping/PopClipping.
		If there is no clipping at the moment, it returns FALSE and clip_rect is not changed. */
	BOOL GetClipping(OpRect &clip_rect);
public:
	BgInfo *info[MAX_BG_INFO];
	int num;
	Head cliplist;
	void RemoveBg(int index);
#ifdef DEBUG_GFX
	unsigned int flushed_color_pixels;
	unsigned int total_color_pixels;
	unsigned int flushed_img_pixels;
	unsigned int total_img_pixels;
#endif
};

#endif // BACKGROUND_CLIPPING_H
