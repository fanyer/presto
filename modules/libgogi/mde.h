// -*- Mode: c++; tab-width: 4; c-basic-offset: 4; coding: iso-8859-1 -*-
#ifndef MDE_H
#define MDE_H

#include "modules/libgogi/mde_config.h"

#include "modules/idle/idle_detector.h"
#include "modules/hardcore/keys/opkeys.h"
#include "modules/util/simset.h"

class MDE_ScrollOperation;

// ==  Multiplatform Desktop Environment  ==

enum MDE_METHOD {
	MDE_METHOD_COPY,		///< Copy. Solid blit.
	MDE_METHOD_MASK,		///< Mask. Use the mask value to specify which index or color to be transparent.
	MDE_METHOD_ALPHA,		///< Alpha. Use the alphavalues to specify how much the colors should be visible (255 solid, 0 invisible).
							///			If the destination-buffers method is alpha, the destination and source alpha will be weighted
							///			for a correct blit. If the destination-buffers method is not alpha, the destination
							///			alpha will not be set or read.
	MDE_METHOD_COLOR,		///< Color. Use the color of the MDE_BUFFER_INFO (in the sourcebitmap) when blitting.
	MDE_METHOD_OPACITY,		///< Opacity. Use the alpha (in color) of the MDE_BUFFER_INFO (in the sourcebitmap) when blitting.

    MDE_METHOD_COMPONENT_BLEND ///< use r, g and b components from source as component-wise alpha and blend towards color in source - needed for LCD sub-pixel smoothing
};

#ifdef MDE_BILINEAR_BLITSTRETCH
enum MDE_STRETCH_METHOD {
	MDE_STRETCH_NEAREST = 0,	///< Use nearest-neighbor interpolation
	MDE_STRETCH_BILINEAR = 1	///< Use bilinear interpolation
};
#endif // MDE_BILINEAR_BLITSTRETCH

enum MDE_FORMAT {
	MDE_FORMAT_BGRA32 = 0,		///< 32bit per pixel BGRA format.
	MDE_FORMAT_BGR24  = 1,		///< 24bit per pixel BGR format.
	MDE_FORMAT_RGBA16 = 2,		///< 16bit per pixel RGBA format. (4444)
	MDE_FORMAT_RGB16  = 3,		///< 16bit per pixel RGB format.
	MDE_FORMAT_MBGR16 = 4,		///< 16bit per pixel MBGR format. (M = mask, 1555)
	MDE_FORMAT_BREW16 = 5,		///< 16bit per pixel BREW format.
	MDE_FORMAT_BREW32 = 6,		///< 32bit per pixel BREW format.
	MDE_FORMAT_ARGB32 = 7,		///< 32bit per pixel ARGB format.
	MDE_FORMAT_RGBA32 = 8,		///< 32bit per pixel RGBA format
	MDE_FORMAT_INDEX8 = 9,		///< 8bit per pixel indexed format (Uses palette in MDE_BUFFER_INFO as colorindex).
	MDE_FORMAT_SRGB16 = 10,     ///< 16bit per pixel RGB format with swaped endian.
	MDE_FORMAT_RGBA24 = 11,		///< 24bit per pixel RGBA format (5658)
	MDE_FORMAT_UNKNOWN = 255	///< Format unknown or error. Should not be used!
};

enum MDE_Z {
	MDE_Z_LOWER,			///< One level lower than the current.
	MDE_Z_HIGHER,			///< One level higher than the current.
	MDE_Z_TOP,				///< The toplevel (Visually drawn on top of everything else).
	MDE_Z_BOTTOM			///< The bottomlevel (Visually drawn behind everything else).
};

struct MDE_RECT {
	int x, y, w, h;
};

class MDE_Region
{
public:
	MDE_Region();
	~MDE_Region();

	/** Reset. Will contain nothing. */
	void Reset(bool free_mem = true);

	/** Swap the contents with another region. */
	void Swap(MDE_Region *other);

	/** Offset the contents by dx and dy*/
	void Offset(int dx, int dy);

	/** Set the region to rect only. returns true if success. */
	bool Set(MDE_RECT rect);

	/** Add a rectangle to the region. returns true if success. */
	bool AddRect(MDE_RECT rect);

	/** Remove a rectangle from the region. */
	void RemoveRect(int index);

	/** Add a rectangle to the region. If the rectangle is overlapping other rectangles in the region,
		rect will be sliced up so the region contains no overlapping rectangles. returns true if success. */
	bool IncludeRect(MDE_RECT rect);

	/** Will split up the existing rectangles into smaller rectangles to exclude remove. returns true if success.*/
	bool ExcludeRect(MDE_RECT rect, MDE_RECT remove);

	/** Will split up the existing rectangles into smaller rectangles to exclude rect. returns success status.*/
	bool ExcludeRect(const MDE_RECT &rect);

	/** Optimize the region. Note: The result is far from the most optimal in many cases. */
	void CoalesceRects();

public:
	MDE_RECT *rects;
	int num_rects;
private:
	int max_rects;
	bool GrowIfNeeded();
};

MDE_RECT	MDE_MakeRect(int x, int y, int w, int h);
#define		MDE_RectIsEmpty(rect) (rect.w <= 0 || rect.h <= 0)
#define		MDE_RectIsInsideOut(rect) (rect.w < 0 || rect.h < 0)
#define		MDE_RectIsIdentical(rect1, rect2) (rect1.x == rect2.x && rect1.y == rect2.y && rect1.w == rect2.w && rect1.h == rect2.h)
bool		MDE_RectIntersects(const MDE_RECT &this_rect, const MDE_RECT &with_rect);
bool		MDE_RectContains(const MDE_RECT &rect, int x, int y);
/// @return true if containee is completely encompassed by container
bool		MDE_RectContains(const MDE_RECT &container, const MDE_RECT& containee);
MDE_RECT	MDE_RectClip(const MDE_RECT &this_rect, const MDE_RECT &clip_rect);
MDE_RECT	MDE_RectUnion(const MDE_RECT &this_rect, const MDE_RECT &and_this_rect);
void		MDE_RectReset(MDE_RECT &this_rect);
#define		MDE_RectUnitArea(rect) (rect.w * rect.h)

/** Modifies rect so that it won't overlap with remove_rect.
	If they intersect so that the result can't be represented by 2 rectangles,
	it will leave rect untouched and return false. Returns true if successful. */
bool		MDE_RectRemoveOverlap(MDE_RECT& rect, const MDE_RECT& remove_rect);

// == BUFFER ==================================================================

/** Storage of the data used by MDE_BUFFER. */

struct MDE_BUFFER_DATA {
	void *data;				///< datapointer
	int w, h;				///< width, height
};

/** Storage of common info about how a buffer should be blitted. */

struct MDE_BUFFER_INFO {
	unsigned char *pal;		///< palette (Order: RGB...)
	unsigned int col;		///< color
	unsigned int mask;		///< index of the color that should be invisible using MDE_METHOD_MASK.
	MDE_METHOD method;		///< method used for blitting.
	MDE_FORMAT format;		///< format
#ifdef MDE_BILINEAR_BLITSTRETCH
	MDE_STRETCH_METHOD stretch_method; ///< method used for stretch blits.
#endif // MDE_BILINEAR_BLITSTRETCH
};

/** MDE_BUFFER is the object you should use to store bitmaps. It is also the destination for the
	painting functions. What you can do on the buffer is dependent on how much the format the buffer use supports.
	If something that is not supported is called, you will get an assert where it should be implemented.

	In situations when memory of a MDE_BUFFER is to big and you need a lot of bitmaps, you can save memory by storing only
	one MDE_BUFFER_INFO and multiple MDE_BUFFER_DATA (as long as the bitmap shares the same info of course). */

struct MDE_BUFFER : public MDE_BUFFER_DATA, public MDE_BUFFER_INFO {
	int stride, ps;			///< stride (bytes per row), pixelsize (bytes per pixel)
	MDE_RECT clip;			///< clipping for paintfunctions.
	MDE_RECT outer_clip;	///< clipping for the clipping (used to avoid exceeding parents rect when dealing with SubsetBuffers)
	int ofs_x, ofs_y;		///< offset to the original buffer for subsetbuffers.
	void* user_data;		///< anything you want! Initially set to NULL. Subbuffers inherit the same user_data.
};

/** Creates a MDE_BUFFER and allocates enough memory for it. Initial content of memory is undefined. */
MDE_BUFFER *MDE_CreateBuffer(int w, int h, MDE_FORMAT format, int pal_len);

/** Deletes the data allocated by MDE_CreateBuffer and sets buf to NULL. Don't use this if the buffer was made with
	MDE_MakeSubsetBuffer or MDE_InitializeBuffer. */
void MDE_DeleteBuffer(MDE_BUFFER *&buf);

/** Makes subset_buf into a subset of parent_buf. Which means that subset_buf's pointer will point directly into parent_bufs
	memory. It will have it's own width and height. outer_clip and clip will be set so that you can't access anything you shouldn't
	(as long as you follow the cliprect). Don't try to MDE_DeleteBuffer a subsetbuffer. */
void MDE_MakeSubsetBuffer(const MDE_RECT &rect, MDE_BUFFER *subset_buf, MDE_BUFFER *parent_buf);

/** Translate the coordinates and pointer in buf. Useful if all painting should be done with a offset, instead of offsetting each paint function.
	Positive dx and dy will move the origo right and down. */
void MDE_OffsetBuffer(int dx, int dy, MDE_BUFFER *buf);

/** Will initialize buf with the given parameters. It will not allocate or copy anything. Use this only to prepare screen
	in your implementation of MDE_Screen.
	stride is the number of bytes that each horizontal row of the destination has. */
void MDE_InitializeBuffer(int w, int h, int stride, MDE_FORMAT format, void *data, unsigned char *pal, MDE_BUFFER *buf);

void* MDE_LockBuffer(MDE_BUFFER *buf, const MDE_RECT &rect, int &stride, bool readable = true);
void MDE_UnlockBuffer(MDE_BUFFER *buf, bool changed = true);

// == PAINTING ================================================================

/** Sets the color that will be used when using the Draw-functions. Use MDE_RGB or MDE_RGBA to get the correct format. */
void MDE_SetColor				(unsigned int col, MDE_BUFFER *dstbuf);

/** Sets the cliprect to rect. If rect is outside dstbuf it will be intersected so nothing will be outside. */
void MDE_SetClipRect			(const MDE_RECT &rect, MDE_BUFFER *dstbuf);

void MDE_DrawRect				(const MDE_RECT &rect, MDE_BUFFER *dstbuf);
void MDE_DrawRectFill			(const MDE_RECT &rect, MDE_BUFFER *dstbuf, bool blend = true);
void MDE_DrawRectInvert			(const MDE_RECT &rect, MDE_BUFFER *dstbuf);
void MDE_DrawEllipse			(const MDE_RECT &rect, MDE_BUFFER *dstbuf);
void MDE_DrawEllipseThick		(const MDE_RECT &rect, int linewidth, MDE_BUFFER *dstbuf);
void MDE_DrawEllipseInvert		(const MDE_RECT &rect, MDE_BUFFER *dstbuf);
void MDE_DrawEllipseFill		(const MDE_RECT &rect, MDE_BUFFER *dstbuf);
void MDE_DrawEllipseInvertFill	(const MDE_RECT &rect, MDE_BUFFER *dstbuf);
void MDE_DrawLine				(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf);
void MDE_DrawLineThick			(int x1, int y1, int x2, int y2, int linewidth, MDE_BUFFER *dstbuf);
void MDE_DrawLineInvert			(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf);

/** Same as MDE_DrawBuffer but takes bufferdata and bufferinfo separately. */
void MDE_DrawBufferData			(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf);

/** Same as MDE_DrawBufferStretch but takes bufferdata and bufferinfo separately. */
void MDE_DrawBufferDataStretch	(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf);

/** Draws srcbuf at dst in dstbuf.
	srcx and srcy is the origo in srcbuf which should be drawn (0,0 to draw start with the upper left corner). */
void MDE_DrawBuffer(MDE_BUFFER *srcbuf, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf);

/** Draws srcbuf at dst in dstbuf.
	src is the rectangle in srcbuf which should be drawn. The result will stretch to fit in dst. */
void MDE_DrawBufferStretch(MDE_BUFFER *srcbuf, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf);

#ifdef MDE_SUPPORT_HW_PAINTING
bool MDE_UseMoveRect();
#else
#define MDE_UseMoveRect() true
#endif // MDE_SUPPORT_HW_PAINTING
/** Moves the content in rect by delta dx and dy pixels. Positive values are right or down.
	Note: rect is the destinationrect. some area should be moved *into* rect. No pixels outside rect should be written to.
	There is no need for clipping/intersecting since rect is guaranteed to be inside dstbuf.
	No invalidation of the area should be done!
	You should use MDE_View::ScrollRect when scrolling instead of calling this directly! */
void MDE_MoveRect(const MDE_RECT &rect, int dx, int dy, MDE_BUFFER *dstbuf);

#ifdef MDE_SUPPORT_ROTATE

bool MDE_RotateBuffer(MDE_BUFFER *buf, double rad, double scale, unsigned int bg_col, bool antialias);

#endif

// == BLITTING/FORMATS ========================================================

int MDE_GetBytesPerPixel(MDE_FORMAT format);

/** Returns true if the given method is supported for blitting nonstretched with the given formats. */
bool MDE_GetBlitMethodSupported(MDE_METHOD method, MDE_FORMAT src_format, MDE_FORMAT dst_format);

/** The callback to set a color. len pixels should be set to col. */
typedef void (*MDE_SCANLINE_SETCOLOR)(void *dst, int len, unsigned int col);

/** The callback to blit pixels. len pixels should be blitted from src to dst. srcinf contains information which blitmethod
	that should be performed. */
typedef bool (*MDE_SCANLINE_BLITNORMAL)(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf);

/** The callback to blit pixels stretched. dstlen is the number of resulting pixels in dst. sx is the fixedpoint startoffset in pixels
	in src and dx is the fixedpoint step-value in pixels for src. srcinf contains information which blitmethod that should be performed. */
typedef void (*MDE_SCANLINE_BLITSTRETCH)(void *dst, void *src, int dstlen, MDE_F1616 sx, MDE_F1616 dx, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf);

#ifdef MDE_BILINEAR_BLITSTRETCH
/** The callback to do bilinear scaling of an image. */
typedef void (*MDE_BILINEAR_INTERPOLATION_X)(void* dst, void* src, int dstlen, int srclen, MDE_F1616 sx, MDE_F1616 dx);
/** Do a bilinear interpolation of two scanlines. */
typedef void (*MDE_BILINEAR_INTERPOLATION_Y)(void* dst, void* src1, void* src2, int len, MDE_F1616 sy);
#endif // MDE_BILINEAR_BLITSTRETCH

MDE_SCANLINE_SETCOLOR		MDE_GetScanline_InvertColor(MDE_BUFFER *dstbuf, bool blend=true);
MDE_SCANLINE_SETCOLOR		MDE_GetScanline_SetColor(MDE_BUFFER *dstbuf, bool blend=true);
MDE_SCANLINE_BLITNORMAL		MDE_GetScanline_BlitNormal(MDE_BUFFER *dstbuf, MDE_FORMAT src_format);
MDE_SCANLINE_BLITSTRETCH	MDE_GetScanline_BlitStretch(MDE_BUFFER *dstbuf, MDE_FORMAT src_format);
#ifdef MDE_BILINEAR_BLITSTRETCH
MDE_BILINEAR_INTERPOLATION_X	MDE_GetBilinearInterpolationX(MDE_FORMAT fmt);
MDE_BILINEAR_INTERPOLATION_Y	MDE_GetBilinearInterpolationY(MDE_FORMAT fmt);
#endif // MDE_BILINEAR_BLITSTRETCH

// == VIEWSYSTEM/CLIPPING/UPDATING =======================================================

#ifdef MDE_SUPPORT_VIEWSYSTEM

/** MDE_View is the object that deals with invalidating/clipping/updating. The container that
	can be used to create windows & widgets. */

class MDE_View
{
public:
	MDE_View();
	virtual ~MDE_View();

	/** Add the child to this view. The childview will automatically be deleted when
		this view is deleted. (If the child isn't removed again with RemoveChild.) */
	void AddChild(MDE_View *child, MDE_View *after = 0);

	/** Remove child from this view without deleting it. */
	void RemoveChild(MDE_View *child);

	/** Set the rect if this view. */
	void SetRect(const MDE_RECT &rect, bool invalidate = true);

	/** Sets the visible status of this view. */
	void SetVisibility(bool visible);

	/** Sets the z-order of this view related to its siblings. When a view is added with AddChild, it will be
		placed at the bottom in the parent (Behind previosly added views). SetZ can be used to change the order. */
	void SetZ(MDE_Z z);

	/** Invalidates rect. The rect will be painted during the next validate.
		If include_children is true, the childviews in this view will also be included.
		If ignore_if_hidden is true, this operation will be cheaper if the view is completly hidden behind other views.
		If bypass_lock is true, the view will be painted next Validate even if locked. Should only be used to force painting of overlapping transparent views. */
	void Invalidate(const MDE_RECT &rect, bool include_children = false, bool ignore_if_hidden = false, bool recursing_internal = false, bool bypass_lock = false);

	/** If the view has invalid content it will paint it. */
	void Validate(bool include_children);
#ifdef MDE_NO_MEM_PAINT
	/** If the view has invalid content it will be repainted (including 
	 * children) in a bottom to top fashion, requireing no memory. */
	void ValidateOOM(bool include_children);
	void MaxInvalidRect(MDE_RECT& irect);
#endif // MDE_NO_MEM_PAINT
	
	/** Make x and y (relative to this view) relative to the upper left corner of the screen. */
	void ConvertToScreen(int &x, int &y);

	/** Make x and y (relative to the upper left corner of the screen) relative to this view. */
	void ConvertFromScreen(int &x, int &y);

	/** Lock the painting for this view and children until it is unlocked. Invalidate will work as usual but
		the OnPaint won't be called as long it is locked. When it gets unlocked, the invalidated area will be updated in the
		next validation-process. Call Validate to force update immediately. You can make nestled calls to LockUpdate.
		Note:	Calls to Scroll when it is locked will invalidate the rectangle that should be scrolled so it gets painted when unlocked.
				Calls to Validate when it is locked will have no effect at all.
				If there is a overlapping transparent view, this view will be painted when that view needs to be painted even when locked! */
	void LockUpdate(bool lock);

	/** Returns true if this view and its parents is visible.
		Note, if the view isn't added to a parent it may still return true. */
	bool IsVisible();

	/** Scrolls the area within rect. dx and dy is the offset. Negative values moves the area left and up.
		If move_children is true, all childviews will be moved to. */
	void ScrollRect(const MDE_RECT &rect, int dx, int dy, bool move_children);

	/** Replaced by OnBeforePaintEx. This function is called from OnBeforePaintEx defaultimplementation to be backwardscompatible. */
	virtual void OnBeforePaint() {}

	/** Called before a view is going to receive one or several OnPaint to paint invalid areas. In this function,
		there is still a chance to Invalidate more areas that should be included in the paint.
		It's also possible to abort the paint completly if the view is not yet ready, by returning false.
		Note that that will also abort painting of transparent view that intersect this view and child views. */
	virtual bool OnBeforePaintEx() { OnBeforePaint(); return true; }

	/** Replaced by OnPaintEx. This function is called from OnPaintEx defaultimplementation to be backwardscompatible. */
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);

	/** Called when drawing should be done. Clipping is set to the minimal required updaterect.
		Never call OnPaintEx directly! Use Invalidate then Validate if you want to trig a repaint immediately.
		The default implementation fills with black color.
		Return true if content was painted and the area should be flushed to screen (if there is a backbuffer).
		Return false if for some reason this area should not be flushed to screen. */
	virtual bool OnPaintEx(const MDE_RECT &rect, MDE_BUFFER *screen) { OnPaint(rect, screen); return true; }

	/** Called when the view has been resized. old_w and old_h is the old size. m_rect is updated with the new. */
	virtual void OnResized(int old_w, int old_h) {}

	/** Called when the view has been resized and/or moved. old_rect is the old rect. m_rect is updated to the new. */
	virtual void OnRectChanged(const MDE_RECT &old_rect) {}

	/** Called when this view is about to be validated (Validate is called on it).
		This differs from OnBeforePaintEx a little:
		It will only be called on the view that Validate is called on (and not on any children to it).
		It will be called even if this view itself has no visible or invalidated parts, as long as there are children to it that are going to be validated. */
	virtual void OnValidate() {}

	/** Happens if the view becomes invalid (first time Invalid is called since OnPaint). */
	virtual void OnInvalid() {}

	/** Happens if the view becomes invalidated from itself or any of its children. Invalidations made on it or its children
		as a result of a sibling or parent isn't included. If all invalidations is interesting, use OnInvalid!
		Note: It is currently called each time Invalidate is called (in above mentioned cases), but that might change in the future. */
	virtual void OnInvalidSelf() {}

	/** Called from ScrollRect to check if the scroll may be done by moving the area or if it must be invalidated instead.
		Return true if the area may be moved (this is faster) */
	virtual bool GetScrollMoveAllowed() { return true; }

	/** Happens after the view has been added to a parent. */
	virtual void OnAdded() {}

	/** Happens after the view has been removed from its direct parent. */
	virtual void OnRemoved() {}

	/** Called when the visible region for this view including it's children
	    changes to be empty or non empty.
	    This is not always immediate, it can be delayed until next update. 
	    visible is the new visibility status, true for visible false for hidden.
		A view will only receive this callback if it or any of its
		ancestors returns true in GetOnVisibilityChangeWanted(). */
	virtual void OnVisibilityChanged(bool visible) {}

	/** return true if this view and the subtree for which it is the
		root should get the OnVisibilityChanged callback.  That
		enables the extra calculation required for that callback. */
	virtual bool GetOnVisibilityChangeWanted() { return false; }

	/** Called when the visible region for this view is marked as invalid. */
	virtual void OnRegionInvalid() {}

	/** If MDE_SUPPORT_TRANSPARENT_VIEWS is defined, this can make the view transparent. If transparent, it will
        not affect underlying views updateregion. Everytime some part of a underlying view is painted, the intersecting
        part of the transparent view is painted directly after. Underlying views will Invalidate instead of
        MDE_MoveRect when ScrollRect is called, so performance of scrolling is lower. */
	void SetTransparent(bool transparent);

	/** Set if this view should affect the visible region of lower views (parent or children to parents with lower z).
		Note: Only set this to false on solid views! Setting it to false on transparent views has undefined behaviour.
		The default is true. */
	void SetAffectLowerRegions(bool affect_lower_regions);

	/** Set a custom overlap region. Any rect in the overlap region will
		cause the visible region to exclude that area on both this view and child views.
		If combined with SetAffectLowerRegions(false), the lower views will be visible through the overlapping area.
		Note: The region sent to this function will be swapped with the old custom region for this view (they will swap internal representations).
		Note: Only set this on solid views! Setting it on transparent views has undefined behaviour. */
	void SetCustomOverlapRegion(MDE_Region *rgn);

	/** Set to true if the view is transparent and doesn't paint anything at all.
		This is useful if you want a view just to receive mouse input, and its presence should not affect
		painting or scrolling performance on underlying views. */
	void SetFullyTransparent(bool fully_transparent);

	/** Set to true if the view is transparent but scroll along with views behind it.
		This is useful for scroll performance reasons.
		If it's true, the scroll of a background view don't need to do full update on the area because of this view.
		Note: To enable this, you must also set SetFullyTransparent(true)! */
	void SetScrollingTransparent(bool scrolling_transparent);

	/** Set how much extra pixels that should be invalidated when SetScrollingTransparent is set to true.
		The default is 0. */
	void SetScrollInvalidationExtra(int extra) { m_scroll_transp_invalidate_extra = extra; }

	/** Returns closest parent that is transparent or NULL. Will return this, if this view is transparent itself. */
	MDE_View *GetTransparentParent();

#ifdef MDE_SUPPORT_MOUSE
	/** Returns the childview that contains the point x,y or NULL if no one did. If include_children
		is true, the search will recurse into the childrens children. */
	MDE_View *GetViewAt(int x, int y, bool include_children);

	/** Return true if x and y is inside the view and on a spot that should be hit by the mouse.
		Transparent views would probably want to return true only if x and y is on a solid spot. */
	virtual bool GetHitStatus(int x, int y);

	/** Will trig a OnMouseDown on the view that was hit. See OnMouseDown for details. */
	void TrigMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate);
	void TrigMouseUp(int x, int y, int button, ShiftKeyState keystate);
	void TrigMouseMove(int x, int y, ShiftKeyState keystate);
	void TrigMouseWheel(int x, int y, int delta, bool vertical, ShiftKeyState keystate);

	/** Called when a mousebutton is pressed on this view. Movement and additional pressed buttons will be also be sent
		to this view until the button that was first pressed has been released.
		button should have these values 1,3,2 (left, middle, right) */
	virtual void OnMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate);
	virtual void OnMouseUp(int x, int y, int button, ShiftKeyState keystate);
	virtual void OnMouseMove(int x, int y, ShiftKeyState keystate);

	/** Called when the mouse moves out from this view.
		If the mouse moves out from a captured view, OnMouseLeave won't be called until the capture is released. */
	virtual void OnMouseLeave();

	/** Positive is a turn against the user. return true if you implement it. If it return false, the parent will get it. */
	virtual bool OnMouseWheel(int delta, bool vertical, ShiftKeyState keystate);

	/** Called when the mouse capture for this view is being released */
	virtual void OnMouseCaptureRelease();
#endif // MDE_SUPPORT_MOUSE

#ifdef MDE_SUPPORT_TOUCH
	/** Will trigger OnTouch{Down, Up, Move} on the view that was hit. */
	void TrigTouchDown(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
	void TrigTouchUp(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
	void TrigTouchMove(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);

	/** Called when a touch event occurs on this view. */
	virtual void OnTouchDown(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
	virtual void OnTouchUp(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);
	virtual void OnTouchMove(int id, int x, int y, int radius, ShiftKeyState modifiers, void *user_data);

	/** Called when a view is removed. Uncaptures this view from all touch traps. */
	virtual void ReleaseFromTouchCapture();
#endif // MDE_SUPPORT_TOUCH
#ifdef MDE_SUPPORT_DND
	virtual void TrigDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y);
	virtual void TrigDragEnter(int x, int y, ShiftKeyState modifiers);
	virtual void TrigDragDrop(int x, int y, ShiftKeyState modifiers);
	virtual void TrigDragEnd(int x, int y, ShiftKeyState modifiers);
	virtual void TrigDragMove(int x, int y, ShiftKeyState modifiers);
	virtual void TrigDragCancel(ShiftKeyState modifiers);
	virtual void TrigDragLeave(ShiftKeyState modifiers);
	virtual void TrigDragUpdate();
	virtual void OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y);
	virtual void OnDragDrop(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragEnd(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragMove(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragLeave(ShiftKeyState modifiers);
	virtual void OnDragCancel(ShiftKeyState modifiers);
	virtual void OnDragEnter(int x, int y, ShiftKeyState modifiers);
	virtual void OnDragUpdate(int x, int y);
#endif // MDE_SUPPORT_DND

	/** Check if the MDE_View is of a specific type. In order to be able to check for the type the class must check for its class name and propage up if the types mismatch */
	virtual bool IsType(const char* type){return false;}
public:
	MDE_RECT m_rect;			///< The rectangle relative to the parent.
	MDE_RECT m_invalid_rect;	///< The invalid rectangle (the rectangle that will be repainted during next call to Validate)
	MDE_Region m_invalid_region;///< The invalid region. Used in addition to m_invalid_rect under some conditions to reduce areas to repaint.
	MDE_Region m_region;		///< The region that is visible on the screen in screencoordinates.
	MDE_Region m_custom_overlap_region; ///< List of rectangles (may overlap) that should be excluded from the visible region of this view.
	bool m_is_visible;			///< The visibility status.
	bool m_is_region_invalid;	///< If the region needs to be recalculated before use.
	bool m_region_invalid_first_check;
	bool m_is_validating;		///< true if this view or any child or overlapping transparent view is being painted.
	bool m_is_transparent;		///< If the view is transparent or not.
	bool m_is_fully_transparent;///< If the view is transparent and is not painting anything at all.
	bool m_is_scrolling_transparent;///< If the view is transparent but scroll along with views behind it (so it won't require full update on scroll).
	bool m_affect_lower_regions;///< If the view should affect the visible region of lower views (parent or children to parents with lower z).
	bool m_bypass_lock;			///< If the lock should be bypassed (paint view even if it's locked).
	int m_updatelock_counter;	///< Stack like counter of LockUpdate calls.
	int m_num_overlapping_transparentviews;
	int m_num_overlapping_scrolling_transparentviews;
	int m_scroll_transp_invalidate_extra; ///< Set with SetScrollInvalidationExtra.
	int m_scroll_invalidate_extra; ///< How much extra pixels that should be invalidated on scroll.
	bool m_exclude_invalidation;
	bool m_visibility_check_needed;

#ifdef MDE_DEBUG_INFO
	int m_color_toggle_debug;
	int m_on_stack;
#endif

	MDE_View *m_parent;
	MDE_View *m_next;
	MDE_View *m_first_child;
	class MDE_Screen *m_screen;

    bool GetInvalidFlag() const { return m_is_invalid; }
    void SetInvalidFlag(bool invalid) { m_is_invalid = invalid; }

private:
	// for UpdateRegion
	friend class MDE_ScrollOperation;
	friend class MDE_Screen;
	void MakeSubsetOfScreen(MDE_BUFFER *buf, MDE_BUFFER *screen);
	bool ShouldNotBePainted();
	void BeforePaintInternal(bool include_children);
	bool PaintInternal(MDE_BUFFER sub_buffer, int pos_on_screen_x, int pos_on_screen_y);
	void PaintAllChildrenInternal(MDE_BUFFER sub_buffer, int pos_on_screen_x, int pos_on_screen_y);
	void ValidateInternal(MDE_BUFFER *buf, MDE_BUFFER *screen, MDE_Region *update_region, bool include_children);
	void ScrollRectInternal(const MDE_RECT &rect, int dx, int dy, bool move_children,
	                        MDE_BUFFER* const passed_screen_buf = NULL, MDE_Region* passed_update_region = NULL,
							bool is_deferred = false);
	bool AddToOverlapRegion(MDE_Region &overlapping_region, int parent_screen_x, int parent_screen_y);
#ifdef MDE_NO_MEM_PAINT
	void ValidateOOMInternal(MDE_BUFFER *buf, const MDE_RECT &invalid, bool include_children);
#endif // MDE_NO_MEM_PAINT
	void InvalidateInternal(const MDE_RECT &rect);
	void SetInvalidState();
	void ValidateRegionDone();
	void SetScreenRecursive(MDE_Screen* screen);
	void RemoveChildInternal(MDE_View *child, bool temporary);
	bool IsLockedAndNoBypass();

	bool ThisOrParentWantsVisibilityChange();
	void CheckVisibilityChange(bool force_check);
	bool m_region_or_child_visible;
	bool m_visibility_status_first_check;
	bool m_onbeforepaint_return;


	bool m_is_invalid;			///< Is this view or any child invalid. If m_is_invalid is true because a child is invalid this view will still not get a OnPaint.

	OpActivity m_activity_invalidation;

protected:
	bool ValidateRegion(bool include_children = true);
	void UpdateRegion(bool include_children = true);

	/**
	 * Setting the MDE_View as busy will prevent the Core-wide idle detection
	 * from reporting that Core is idle.
	 *
	 * Idle detection is used to figure out when Core is "not doing something",
	 * initially implemented to let test systems to know when proceeding to the
	 * next step in a test case is "safe". Core is not idle if there are
	 * invalid areas, and the purpose of SetBusy and SetIdle is track this
	 * state.
	 *
	 * The MDE_View will remain in the busy state until SetIdle is called.
	 *
	 * Using this function (or SetIdle) is not necessary (nor advisable) for
	 * anything other than the top-level MDE_Views, as invalid children will
	 * cause parent invalidation as well.
	 */
	void SetBusy();

	/**
	 * Puts the MDE_View in the idle state, thus allowing Core as a whole to
	 * go into the idle state (if other conditions allow it).
	 *
	 * @see SetBusy
	 */
	void SetIdle();
};

/** MDE_Screen is the backend for the platform. It should basically only maintain a MDE_BUFFER which datapointer
	points directly to the framebuffer or into a backbuffer.
	When MDE prepares for painting, it will call LockBuffer. When the painting is done it will call UnlockBuffer.

	If the backend also has another viewsystem (If the target f.ex. is a window among other windows) you
	will have to decide what to do when another window is erasing this windows data.
	This is 2 suggestions:
	1. If the target was a backbuffer, you could just blit it to the window immediately, not involving MDE.
	2. You can call Invalidate(rect, true) so everything will be updated by MDE during the next validate.

	You must frequently call Validate(true) to repaint all views that has been invalidated. On a slow device
	it might be a good idea to call it from a timer, not very often. That will prevent your app from spending
	to much time painting.
*/

class MDE_Screen : public MDE_View
{
public:
	virtual ~MDE_Screen() { m_deferred_scroll_operations.Clear(); }

	void Init(int width, int height);

	virtual bool IsType(const char* type) { return op_strcmp(type, "MDE_SCREEN") == 0 ? true : MDE_View::IsType(type); }

	int GetWidth()		{ return m_rect.w; }
	int GetHeight()		{ return m_rect.h; }

	/** Called when there is not enough memory to complete something. There will probably be some
		invalid area of the screen which remains unupdated. If this happens and there is something to
		free you can then call Invalidate(m_rect, true); to repaint everything. But be cautious so
		you don't end up getting a eternal loop of invalidating and getting out of memory. */
	virtual void OutOfMemory() = 0;

	/** Return the format of the screen. */
	virtual MDE_FORMAT GetFormat() = 0;

	/** Should prepare a MDE_BUFFER pointing to the destination where all
		drawing should be done. MDE_InitializeBuffer can make it easier for you. */
	virtual MDE_BUFFER *LockBuffer() = 0;

	/** Is called when drawing is done. If the destionationbuffer was a backbuffer, now it is time
		to blit the changed region of it to screen. update_region is the region that has changed. */
	virtual void UnlockBuffer(MDE_Region *update_region) = 0;

	/** Will be called immediately before any view in this screen enters its OnPaint. */
	virtual void OnBeforeRectPaint(const MDE_RECT &rect) {}

	/** Will be called immediately after any view in this screen has exited its OnPaint.
		May f.ex. be used for postprocessing the pixels before they are flushed to screen. */
	virtual void OnRectPainted(const MDE_RECT &rect) {};

	/** Should do the same as MDE_MoveRect directly on the framebuffer/texture (ignoring the backbuffer).
		This will save us the work of scrolling the backbuffer and then blit the result to the framebuffer.
		There is no point implementing it if the screen is a frontbuffer.

		IMPORTANT: The requirement for this is that the backbuffer is never blit to frontbuffer from somewhere else
		than UnlockBuffer with the given region, since it may contain pixels that are not up-to-date with the actual content.
		If a systempaint/expose happens, call MDE_Screen::Invalidate instead of blitting the backbuffer directly since the invalidate will make sure backbuffer is repainted before blitting it to frontbuffer.

		This is NOT COMPATIBLE with sprites!

		See documentation for MDE_MoveRect about parameter documentation. */
	virtual void ScrollPixels(const MDE_RECT &rect, int dx, int dy) {};

	/** Return true if ScrollPixels is supported. If false, scroll will be done normally by MDE_MoveRect. */
	virtual bool ScrollPixelsSupported() { return false; }

	/** To be implemented by foreground/background aware subclasses that need to be notified when the background should move. */
	virtual void ScrollBackground(const MDE_RECT &rect, int dx, int dy) {}

	/** Add a deferred scroll operation. The rect, deltas and flag
		will be stored in a list, and applied during
		validation. Scroll operations are batched whenever possible.
		@param view the view to scroll
		@param rect the part of the view to scroll
		@param dx horizontal movement
		@param dy vertical movement
		@param move_children if true, also move any children to view
		@return
		OpStatus::OK on success (scroll operation was added to list)
		OpStatus::ERR_NO_MEMORY on OOM (all successfully queued scroll
		operations will have been applied, but caller must deal with
		the failed one) */
	OP_STATUS AddDeferredScroll(class MDE_View* view, const MDE_RECT& rect, int dx, int dy, bool move_children);
	/** Any invalidations made after a deferred scrolling operation
		has been added must be adjusted, since VisualDevice believes
		the scrolling has already been performed.
		@param view the view for which to adjust the rect (eg the view being invalidated)
		@param rect the rect to adjust
	 */
	void AdjustRectForDeferredScroll(class MDE_View* view, MDE_RECT& rect);
	/** Called from MDE_View::Validate after locking the buffer, to
		apply all queued scroll operations.
		@param screen_buf the scrren containing the deferred scroll	operations. */
	void ApplyDeferredScroll(MDE_BUFFER* screen_buf, MDE_Region& region);
	/** Called from MDE_View::RemoveChildInternal, to make sure
		removed views are purged from the list of deferred scroll
		operations.
		@param child the child being removed */
	void OnChildRemoved(MDE_View* child);

#ifdef MDE_SUPPORT_DND
	/** Set the view as the view that will be used by drag and drop functions
	    when data is dragged
	    @param view The view to use */
	void SetDragCapture(class MDE_View* view) { m_dnd_capture_view = view; }

	/** Reset the view that has been set by @ref SetDragCapture */
	void ReleaseDragCapture() { m_dnd_capture_view = NULL; }
#endif

	/**
		Return the view which should act as parent for all opera views/windows in this screen. 
		If no special view for this is wanted, leave it as it is and the screen itself will be the direct parent of opera views.*/	
	virtual MDE_View *GetOperaView() { return this; }
#ifdef MDE_SUPPORT_SPRITES

	void AddSprite(class MDE_Sprite *spr);
	void RemoveSprite(class MDE_Sprite *spr);

	void PaintSpriteInViewInternal(MDE_BUFFER *screen, MDE_Sprite *spr, MDE_View *view, bool paint_background);
	void DisplaySpritesInternal(MDE_BUFFER *screen, MDE_Region *update_region);
	void UndisplaySpritesInternal(MDE_BUFFER *screen, MDE_Region *update_region, const MDE_RECT &rect);
	void UndisplaySpritesRecursiveInternal(MDE_BUFFER *screen, MDE_Region *update_region, const MDE_RECT &rect, MDE_Sprite *spr);

	MDE_Sprite *m_first_sprite;
	MDE_Sprite *m_last_sprite;

	virtual void PaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr);
	virtual void UnpaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr);
#endif // MDE_SUPPORT_SPRITES

#ifdef MDE_SUPPORT_MOUSE
	MDE_View*	m_captured_input;
	int			m_captured_button;
	int			m_mouse_x;
	int			m_mouse_y;

	/** Release the mouse capture. */
	void		ReleaseMouseCapture();
#endif // MDE_SUPPORT_MOUSE

#ifdef MDE_SUPPORT_TOUCH
	struct TouchState
	{
		MDE_View* m_captured_input;
		int m_x;
		int m_y;
	} m_touch[LIBGOGI_MULTI_TOUCH_LIMIT];
#endif // MDE_SUPPORT_TOUCH

#ifdef MDE_SUPPORT_DND
	int			m_drag_x;
	int			m_drag_y;
	MDE_View*	m_last_dnd_view;
	MDE_View*	m_dnd_capture_view;
#endif // MDE_SUPPORT_DND

	List<MDE_ScrollOperation> m_deferred_scroll_operations;

#ifdef MDE_DEBUG_INFO
	int			m_debug_flags;

	/** Set a combination of flags for which debug information that should be active. See mde_config.h for details. */
	void		SetDebugInfo(int flags);

	/** Should draw the debug rectangle with the given color on dstbuf. The default implementation use MDE_DrawRect. */
	virtual void DrawDebugRect(const MDE_RECT &rect, unsigned int col, MDE_BUFFER *dstbuf);
#endif
#ifdef VEGA_OPPAINTER_SUPPORT
	virtual class VEGAOpPainter* GetVegaPainter() = 0;
#endif // VEGA_OPPAINTER_SUPPORT

	/** Is the screen being resized, vega may reduce drawing quality to have better performance when resizing. */
	virtual bool IsResizing() { return false; }

	/** Should return true if the screen is transparent and no skin background clearer is present. Otherwise when
	    repainting the transparent content will be blended with leftover on background.

	    A typical example is the platform popup window which has transparent border shadows.
	*/
	virtual bool UseTransparentBackground() { return false; }
};

#ifdef MDE_SUPPORT_SPRITES

/** MDE_Sprite is a object that is drawn on top of all views in a MDE_Screen. The benifits compared
	to using another on-top MDE_View, is performance. Views covered by a sprite will not be repainted
	when a sprite is moved or repainted. The regions of covered views is not affected by the sprites.
	Sprites allocates a buffer for the background.

	Note: Without doublebuffering, sprites may flicker when something behind is repainted!

	== Implementation details ==

	The idea is quite simple:
	 -Sprites are removed (replaced with the cached background) in MDE_View::UndisplaySpritesInternal.
	 -Sprites are painted (after caching the background) in MDE_View::DisplaySpritesInternal.

	Any change to the screen (which is either MDE_View::Validate or MDE_View::ScrollRect) first call UndisplaySpritesInternal to remove intersecting sprites.
	Any change (same functions) also calls DisplaySpritesInternal when it's done.

	It is important that the blit from the screen to the "sprite-background" is done with METHOD_COPY since any other method would make the background "fade" or smear the page.
	The sprites can be alphatransparent or do anything they want in OnPaint since the whole intersecting background is cached and put back when needed.
*/

class MDE_Sprite
{
public:
	MDE_Sprite();
	virtual ~MDE_Sprite();

	/** Initialize the sprite with the give width and height and colorformat of the given screen.
		If it returns false, it ran out of memory and the sprite should not be used.
		You also have to call MDE_Screen::AddSprite on your screen to add it. */
	bool Init(int w, int h, MDE_Screen *screen);

	/** Normally, sprites are always on top of all existing MDE_View's. If a view is set for the sprite, it will
		only be visible over the visible parts of the view or its children.
		Note, that the position of the sprite is still in screen coordinates.
		The view must exist as long as it is set on a sprite. It can be unset by calling SetView(NULL) */
	void SetView(MDE_View *view);

	/** Set the position of the sprite (In pixels relative to the upper left of the screen) */
	void SetPos(int x, int y);

	/** Set the hotspot (origo) of the sprite, that it will be centered around. */
	void SetHotspot(int x, int y);

	void Invalidate();

	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen) = 0;
public:
	MDE_BUFFER *m_buf;		///< Cached background of the screen (before the sprite was painted)
	MDE_Screen *m_screen;
	MDE_View *m_view;		///< NULL or the view to clip the sprite within.
	MDE_RECT m_rect;
	MDE_RECT m_displayed_rect;
	bool m_displayed;
	bool m_invalid;
	MDE_Sprite *m_prev;
	MDE_Sprite *m_next;
	int m_hx, m_hy; ///< Hotspot
};

#endif // MDE_SUPPORT_SPRITES

#endif // MDE_SUPPORT_VIEWSYSTEM

#endif // MDE_H
