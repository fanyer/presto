/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef __X11_OPBITMAP_H__
#define __X11_OPBITMAP_H__

#include "platforms/unix/base/common/common_opbitmap.h"

#include "modules/util/simset.h"

#ifndef VEGA_OPPAINTER_SUPPORT
class X11OpBitmapInternal;
class X11OpPainter;


class X11OpBitmap : public Link, public OpBitmap
{
public:
	X11OpBitmap(bool transparent=false, bool alpha=true, UINT32 transparent_index=0);

	/**
	 * Create a new X11OpBitmap in the "normal" way. This is the second-stage
	 * constructor that should be invoked from the OpFactory.
	 */
	OP_STATUS Construct(int width, int height);

	/**
	 * Create a new X11OpBitmap from an already initialized
	 * X11OpBitmapInternal. The X11OpBitmapInternal object must be valid. No
	 * error checking is done by the constructor. X11OpBitmap takes ownership
	 * over the X11OpBitmapInternal object.
	 */
	OP_STATUS Construct(X11OpBitmapInternal *bi);

	/**
	 * Create a scaled version of an X11OpBitmapInternal.
	 * @param width The width of the scaled bitmap
	 * @param height The height of the scaled bitmap
	 * @param area excerpt of the image to scale (coordinates and dimensions
	 * are specified as after scaling) - NULL-pointer is OK
	 * @param bi Internal bitmap object to crop and scale
	 */
	OP_STATUS Construct(int width, int height, const OpRect *area, X11OpBitmapInternal *bi);

	~X11OpBitmap();

	X11OpBitmap *Suc() { return (X11OpBitmap *)Link::Suc(); }
	X11OpBitmap *Pred() { return (X11OpBitmap *)Link::Pred(); }

	BOOL Supports(SUPPORTS supports) const;

	OpBitmap *CreateTile(UINT32 width, UINT32 height);

#ifdef OPBITMAP_POINTER_ACCESS
	virtual void* GetPointer(AccessType access_type = ACCESS_READWRITE);
	virtual void ReleasePointer(BOOL changed = TRUE);
#else
	void *GetPointer() const;
	void ReleasePointer() const;
#endif
	OpPainter *GetPainter();
	void ReleasePainter();

	OP_STATUS AddLine(void* data, INT32 line);
#ifdef SUPPORT_INDEXED_OPBITMAP
    OP_STATUS AddIndexedLine(void* data,  INT32 line);
	BOOL SetPalette(UINT8* pal,  UINT32 num_colors);
#endif // SUPPORT_INDEXED_OPBITMAP
	BOOL SetColor(UINT8* color, BOOL all_transparent, const OpRect* rect);

	UINT32 GetBytesPerLine() const;
	BOOL GetLineData(void* data, UINT32 line) const;

	UINT32 Width() const;
	UINT32 Height() const;
	UINT8 GetBpp() const;
	BOOL HasAlpha() const;
	BOOL IsTransparent() const;
	UINT32 GetTransparentColorIndex() const { return 0; }
	BOOL GetIndexedLineData(void* data, UINT32 line) const;
	BOOL GetPalette(UINT8* pal) const;
	INT32 IsIndexed() const;

	void ForceAlpha();
	BOOL SetOpacity(UINT8 opacity);

	// Implementation specific methods:
	void Draw(class OpPainter *painter, const OpRect &source, int dx, int dy,
			  int use_w, int use_h);

	void PrepareAndDrawFragments(OpPainter *painter, const OpRect &source,
								 int dx, int dy, int use_w, int use_h);

	void ColorcellsChanged();

	void PixmapChanged();


	BOOL HasSharedMemory() const;

	/**
	 * Return an estimate of the number of bytes needed for a buffer to hold
	 * a bitmap with the characteristics specified by the parameters.
	 * See OpBitmap::GetBitmapAllocationSize() for more info.
	 */
	static int GetBitmapAllocationSize(int width, int height, bool transparent,
									   bool alpha, int indexed=0);

	/**
	 * Disable all alpha blending.
	 */
	static void DisableAlphaBlending(bool disable);

	/**
	 * I hope you know what you're doing...
	 */
	X11OpBitmapInternal *GetInternal() const { return d; }


	void SetIsShaped(bool shaped);

private:
	void CopyToPointer();
	void CopyFromPointer();
	void SetAlphaFromImageData(unsigned int line, const UINT32* image_data);
	void SetAlphaToImageData(unsigned int line, UINT32* image_data) const;

private:
	X11OpBitmapInternal* d;
	X11OpPainter* m_painter;
	X11OpBitmapInternal* m_cached_effect_image;
	UINT32* m_pointer_buffer; // Copy of internal image data. Always ARGB 32 bit.
	AccessType m_pointer_access_type;
#ifdef SUPPORT_INDEXED_OPBITMAP
	UINT8* m_palette;
	int m_palette_size;
	UINT8 m_transparent_index;
#endif // SUPPORT_INDEXED_OPBITMAP
	bool m_transparent;
	bool m_alpha;
	bool m_invalidate_cache;
	bool m_is_shaped;
};
#endif // !VEGA_OPPAINTER_SUPPORT
#endif // __X11_OPBITMAP_H__
