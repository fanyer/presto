/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGABACKEND_H
#define VEGABACKEND_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegafixpoint.h"

#include "modules/libvega/vegafill.h"
#include "modules/libvega/vegafilter.h"
#include "modules/libvega/vegarefcount.h"

class VEGAPath;
class VEGAImage;
class VEGAStencil;
class VEGARenderTarget;
class VEGATransform;
#ifdef VEGA_OPPAINTER_SUPPORT
class VEGAWindow;
class VEGAOpBitmap;
#endif // VEGA_OPPAINTER_SUPPORT
class VEGAFilterDisplace;
struct VEGAPixelStore;

struct VEGADrawImageInfo
{
	int srcx, srcy;
	unsigned int srcw, srch;
	int dstx, dsty;
	unsigned int dstw, dsth;

	enum Type
	{
		NORMAL = 0,
		SCALED,
		REPEAT
	};
	Type type;
	VEGAFill::Quality quality;
	UINT8 opacity;
	int tile_offset_x;
	int tile_offset_y;
	unsigned int tile_width;
	unsigned int tile_height;
};

class VEGARendererBackend
{
	friend class VEGARenderer;
public:
	VEGARendererBackend();
	virtual ~VEGARendererBackend() {}

	virtual OP_STATUS init(unsigned int w, unsigned int h, unsigned int q) = 0;

	virtual bool bind(VEGARenderTarget* rt) = 0;
	virtual void unbind() = 0;

	virtual void flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0);

	virtual void clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform) = 0;

	virtual OP_STATUS fillPrimitive(struct VEGAPrimitive* prim, VEGAStencil* stencil) { return OpStatus::ERR; }
	virtual OP_STATUS fillPath(VEGAPath *path, VEGAStencil *stencil) = 0;
	virtual OP_STATUS fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend) = 0;
	virtual OP_STATUS invertRect(int x, int y, unsigned int width, unsigned int height) = 0;
	virtual OP_STATUS invertPath(VEGAPath* path) { return OpStatus::ERR; }
	virtual OP_STATUS moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy) = 0;

#ifdef VEGA_OPPAINTER_SUPPORT
	// If you override this, make sure to set has_image_funcs to true
	virtual OP_STATUS drawImage(VEGAImage* image, const VEGADrawImageInfo& imginfo) { return OpStatus::ERR; }
	virtual OP_STATUS drawString(class VEGAFont* font, int x, int y, const struct ProcessedString* processed_string, VEGAStencil* stencil) = 0;
# if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
	virtual OP_STATUS drawString(class VEGAFont* font, int x, int y, const uni_char* text, UINT32 len, INT32 extra_char_space, short word_width, VEGAStencil* stencil) = 0;
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS
#endif // VEGA_OPPAINTER_SUPPORT

	virtual OP_STATUS applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region) = 0;

	virtual OP_STATUS updateQuality() { return OpStatus::OK; }

	void setColor(unsigned long col);
	void setFill(VEGAFill *f);

	void setXORFill(bool xf) { xorFill = xf; }

	void setClipRect(int x, int y, int w, int h);
	void getClipRect(int &x, int &y, int &w, int &h);

	virtual OP_STATUS createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp) = 0;
#ifdef VEGA_OPPAINTER_SUPPORT
	virtual OP_STATUS createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window) = 0;
	virtual OP_STATUS createBufferRenderTarget(VEGARenderTarget** rt, VEGAPixelStore* ps) { return OpStatus::ERR_NOT_SUPPORTED; };
#endif // VEGA_OPPAINTER_SUPPORT
	virtual OP_STATUS createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h) = 0;

	virtual OP_STATUS createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h) = 0;

#ifdef VEGA_LIMIT_BITMAP_SIZE
	static unsigned maxBitmapSide();
#endif // VEGA_LIMIT_BITMAP_SIZE
	static OP_STATUS createBitmapStore(VEGABackingStore** store, unsigned width, unsigned height, bool is_indexed, bool is_opaque);
	virtual bool supportsStore(VEGABackingStore* store) = 0;

#ifdef CANVAS3D_SUPPORT
	virtual OP_STATUS createImage(VEGAImage* fill, class VEGA3dTexture* tex, bool isPremultiplied) = 0;
#endif // CANVAS3D_SUPPORT

	virtual unsigned calculateArea(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy) = 0;

protected:
	// Shared support functions
	OP_STATUS fillSlicedRoundedRect(VEGAPrimitive* prim, VEGAStencil* stencil);
	OP_STATUS fillFractionalRect(VEGAPrimitive* prim, VEGAStencil* stencil);
	void clearTransformed(int x, int y, unsigned int w, unsigned int h,
						  unsigned int color, VEGATransform* transform);

	struct FractRect
	{
		int x, y;
		unsigned w, h;

		UINT8 weight;
	};

	void emitRect(FractRect* rects, unsigned& num_rects,
				  int x, int y, unsigned w, unsigned h, unsigned weight);
	void emitFractionalSpan(FractRect* rects, unsigned& num_rects,
							VEGA_FIX x, int y, VEGA_FIX w, int h, VEGA_FIX miny, VEGA_FIX maxy);
	OP_STATUS fillFractRects(FractRect* rects, unsigned num_rects, VEGAStencil* stencil);
#ifdef VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
	OP_STATUS fillFractImages(FractRect* rects, unsigned num_rects);
	VEGAImage* getDrawableImageFromFill(VEGAFill* fill, VEGA_FIX dst_x, VEGA_FIX dst_y, VEGA_FIX dst_w, VEGA_FIX dst_h);
	virtual bool useCachedImages() {return false;}
#endif // VEGA_USE_BLITTER_FOR_NONPIXELALIGNED_IMAGES
	struct FillOpacityState
	{
		UINT32 color;
		unsigned char fillopacity;
	};

	FillOpacityState saveFillOpacity();
	void restoreFillOpacity(const FillOpacityState& saved);

	class VEGARenderer* renderer;

	unsigned int width;
	unsigned int height;

	int cliprect_sx, cliprect_ex, cliprect_sy, cliprect_ey;

	struct FillState
	{
		VEGAFill* fill;
		UINT32 color;
		UINT32 ppixel;
		bool alphaBlend;
	};
	FillState fillstate;
	bool xorFill;

	unsigned int quality;

	unsigned int r_minx, r_miny, r_maxx, r_maxy;
	VEGARenderTarget* renderTarget;

	bool has_image_funcs;
	bool supports_image_tiling;
};

class VEGABackingStore : public VEGARefCount
{
public:
	enum Type
	{
		SWBUFFER,
		SURFACE,
		FRAMEBUFFEROBJECT,
		TEXTURE
	};

	virtual bool IsA(Type type) const = 0;

	virtual void SetColor(const OpRect& rect, UINT32 color) = 0;

	virtual OP_STATUS CopyRect(const OpPoint& dstp, const OpRect& srcr, VEGABackingStore* store) = 0;

	enum AccessType
	{
		ACC_READ_ONLY,
		ACC_WRITE_ONLY,
		ACC_READ_WRITE
	};

	/** Helper to access the entire backing store */
	VEGASWBuffer* BeginTransaction(AccessType acc)
	{
		return BeginTransaction(OpRect(0, 0, GetWidth(), GetHeight()), acc);
	}

	virtual VEGASWBuffer* BeginTransaction(const OpRect& rect, AccessType acc) = 0;
	virtual void EndTransaction(BOOL commit) = 0;

	virtual BOOL IsIndexed() const { return FALSE; }
	virtual VEGA_PIXEL* GetPalette() const { return NULL; }

	virtual unsigned GetWidth() const = 0;
	virtual unsigned GetHeight() const = 0;
	virtual unsigned GetBytesPerLine() const = 0;
	virtual unsigned GetBytesPerPixel() const = 0;

	virtual void Validate() {}
	virtual void Invalidate() {}
	virtual void Flush(const OpRect* update_rects = NULL, unsigned int num_rects = 0) {}

protected:
	OP_STATUS FallbackCopyRect(const OpPoint& dstp, const OpRect& srcr, VEGABackingStore* store);
};

#ifdef VEGA_USE_HW
/* Helper middle-man for backing stores that require locking.
 *
 * The help consists of ability to perform 'lazy' transaction - i.e,
 * keep the locked buffer around, expecting some 'locality of
 * reference'. */
class VEGABackingStore_Lockable : public VEGABackingStore
{
public:
	VEGABackingStore_Lockable() : m_transaction_deferred(false) {}

	virtual VEGASWBuffer* BeginTransaction(const OpRect& rect, AccessType acc);
	virtual void EndTransaction(BOOL commit);

	virtual void Validate();
	virtual void Flush(const OpRect* update_rects, unsigned int num_rects) { FlushTransaction(); }

	void FlushTransaction()
	{
		if (!m_transaction_deferred)
			return;

		OpStatus::Ignore(UnlockRect(m_acc_commit));
		m_transaction_deferred = false;
	}


protected:
	virtual OP_STATUS LockRect(const OpRect& rect, AccessType acc) = 0;
	virtual OP_STATUS UnlockRect(BOOL commit) = 0;

	VEGASWBuffer m_lock_buffer;

	// Deferred-Unlock transaction data
	OpRect m_transaction_rect;
	AccessType m_acc_type;
	bool m_acc_commit;
	bool m_transaction_deferred;
};
#endif // VEGA_USE_HW

#endif // VEGA_SUPPORT
#endif // VEGABACKEND_H
