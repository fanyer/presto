/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAOPBITMAP_H
#define VEGAOPBITMAP_H

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT)

#include "modules/pi/OpBitmap.h"

#ifdef EMBEDDED_ICC_SUPPORT
class ImageColorTransform;
#endif // EMBEDDED_ICC_SUPPORT

class VEGAFill;
class VEGARenderer;
class VEGARenderTarget;
class VEGABackingStore;

class VEGAOpBitmap : public OpBitmap
{
public:
	VEGAOpBitmap(UINT32 width, UINT32 height, BOOL transparent,
				 BOOL alpha, UINT32 transpcolor, INT32 indexed);
	virtual ~VEGAOpBitmap();

	OP_STATUS Construct(BOOL support_painter, BOOL is_indexed);
#ifdef VEGA_USE_HW
	OP_STATUS Migrate();
#endif // VEGA_USE_HW

	virtual BOOL Supports(SUPPORTS /*supports*/) const;

	virtual void* GetPointer(AccessType access_type);
	virtual void ReleasePointer(BOOL changed);

	virtual OpPainter* GetPainter();
	virtual void ReleasePainter();

	/*virtual */OP_STATUS CopyRect(const OpPoint& dest_point, const OpRect& src_rect, OpBitmap* src);

	virtual OP_STATUS AddLine(void* data, INT32 line);
#ifdef USE_PREMULTIPLIED_ALPHA
	OP_STATUS AddLineAndPremultiply(void* data, INT32 line);
#endif // USE_PREMULTIPLIED_ALPHA
#ifdef EMBEDDED_ICC_SUPPORT
	OP_STATUS AddLineWithColorTransform(void* data, INT32 line, ImageColorTransform* color_transform);
#endif // EMBEDDED_ICC_SUPPORT
	virtual BOOL GetLineData(void* data, UINT32 line) const;

#ifdef SUPPORT_INDEXED_OPBITMAP
	virtual OP_STATUS AddIndexedLine(void* data,  INT32 line);
	virtual BOOL GetIndexedLineData(void* data, UINT32 line) const;
	virtual BOOL SetPalette(UINT8* /*pal*/,  UINT32 /*num_colors*/);
	virtual BOOL GetPalette(UINT8* pal) const;
#endif // SUPPORT_INDEXED_OPBITMAP

	virtual BOOL SetColor(UINT8* color, BOOL all_transparent, const OpRect* rect);

	virtual UINT32 Width() const;
	virtual UINT32 Height() const;
	virtual UINT8 GetBpp() const;
	virtual UINT32 GetBytesPerLine() const;
	virtual BOOL HasAlpha() const;
	virtual BOOL IsTransparent() const;
	virtual UINT32 GetTransparentColorIndex() const;

	VEGABackingStore* GetBackingStore()
	{
#ifdef VEGA_LIMIT_BITMAP_SIZE
		OP_ASSERT(!isTiled());
#endif // VEGA_LIMIT_BITMAP_SIZE
		return backingstore;
	}
	OP_STATUS getFill(VEGARenderer* rend, VEGAFill** outfill);
	OpBitmap* GetSubBitmap(const OpRect& rect);
	void ReleaseSubBitmap();

	static UINT32 GetAllocationSize(UINT32 width, UINT32 height, BOOL transparent, BOOL alpha, INT32 indexed);
#ifdef VEGA_LIMIT_BITMAP_SIZE
	static bool needsTiling(UINT32 width, UINT32 height);
	bool needsTiling() const { return needsTiling(width, height); }
	/** Construct tiles for this bitmap.
	  * @param is_indexed see Construct()
	  * @return OpStatus::OK if all went well, OpStatus::ERR_NO_MEMORY on OOM */
	OP_STATUS ConstructTiles(const unsigned tile_side, BOOL is_indexed);
	/// @return true if this bitmap is tiled
	bool isTiled() const { return bitmap_tiles != NULL; }
	/** Paint a tiled bitmap according to information in diinfo. Will
	  * generate calls to VEGAOpPainter::PaintImage.
	  * @param painter the painter to use
	  * @param diinfo source and destination rect etc
	  * @return the accumulated status from the calls to
	  * PaintImage. OOM takes priority over other errors.
	 */
	OP_STATUS paintTiled(class VEGAOpPainter* painter, struct VEGADrawImageInfo& diinfo);
	/** Copy parts of a tiled bitmap to a render target. Will
	  * generate calls to VEGARenderTarget::copyToBitmap.
	  * @param rt the render target to copy to
	  * @param width the width of the area to be copied
	  * @param height the height of the area to be copied
	  * @param srcx,srcy the top-left corner of the area to be copied,
	  * relative to the top-left corner of the entire bitmap
	  * @param dstx,dsty the top-left corner of the target area,
	  * relative to the top-left corner of the render target
	  * @return the accumulated status from the calls to
	  * copyToBitmap. OOM takes priority over other errors.
	 */
	OP_STATUS copyTiled(class VEGARenderTarget* rt,
	                    unsigned int width, unsigned int height,
	                    unsigned int srcx, unsigned int srcy,
	                    unsigned int dstx, unsigned int dsty);
	/** Copy parts of srcbmp to this tiled bitmap. Will
	  * generate calls to VEGAOpBitmap::CopyRect.
	  * @param dest_point the top-left corner of the area to be copied,
	  * relative to the top-left corner of this entire bitmap
	  * @param src_rect the area in srcbmp to copy
	  * @return the accumulated status from the calls to
	  * CopyRect. OOM takes priority over other errors.
	 */
	OP_STATUS copyTiled(const OpPoint& dest_point, const OpRect& src_rect, VEGAOpBitmap* src_bmp);
	/** Copy parts of this tiled bitmap to target_bmp. Will
	  * generate calls to VEGAOpBitmap::CopyRect.
	  * @param src_rect the area in this tiled bitmap to copy
	  * @param dest_point the top-left corner of the area to be copied,
	  * relative to the top-left corner of target_bmp
	  * @return the accumulated status from the calls to
	  * CopyRect. OOM takes priority over other errors.
	 */
	OP_STATUS copyFromTiled(const OpRect& src_rect, const OpPoint& dest_point, VEGAOpBitmap* target_bmp);
	/** Draw parts of this tiled bitmap to ctx. Will
	  * generate calls to VEGACanvasContext2D::drawImage.
	  * @param ctx the target canvas context
	  * @param src the area in this tiled bitmap to copy
	  * @param dst the target area
	  * @return the accumulated status from the calls to
	  * CopyRect. OOM takes priority over other errors.
	 */
	OP_STATUS drawCanvasTiled(class CanvasContext2D* ctx, const double* src, const double* dst);
#endif // VEGA_LIMIT_BITMAP_SIZE

private:
	VEGABackingStore* backingstore;

	UINT32 width;
	UINT32 height;

	bool alpha;
	bool transparent;
	UINT32 transColor;

	UINT32* palette;
	unsigned int paletteSize;

	class VEGAOpPainter* painter;

	VEGAFill* fill;

	struct CachedSubBitmap
	{
		OpBitmap* bmp;
		OpRect rect;
	};
	CachedSubBitmap subBitmapCache[9];

#ifdef VEGA_LIMIT_BITMAP_SIZE
	class VEGABitmapTileCollection* bitmap_tiles;
#endif // VEGA_LIMIT_BITMAP_SIZE
};

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT

#endif // !VEGAOPBITMAP_H
