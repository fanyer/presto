/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGABACKEND_SW_H
#define VEGABACKEND_SW_H

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegabackend.h"
#include "modules/libvega/src/vegarasterizer.h"

class VEGABackend_SW :
	public VEGARendererBackend,
	public VEGASpanConsumer
{
public:
	VEGABackend_SW() { has_image_funcs = true; }

	OP_STATUS init(unsigned int w, unsigned int h, unsigned int q);

	bool bind(VEGARenderTarget* rt);
	void unbind();

	void clear(int x, int y, unsigned int w, unsigned int h, unsigned int color, VEGATransform* transform);
	OP_STATUS fillPrimitive(struct VEGAPrimitive* prim, VEGAStencil* stencil);
	OP_STATUS fillPath(VEGAPath *path, VEGAStencil *stencil);
	OP_STATUS fillRect(int x, int y, unsigned int width, unsigned int height, VEGAStencil* stencil, bool blend);
	OP_STATUS invertRect(int x, int y, unsigned int width, unsigned int height);
	OP_STATUS moveRect(int x, int y, unsigned int width, unsigned int height, int dx, int dy);

#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS drawImage(VEGAImage* image, const VEGADrawImageInfo& imginfo);
	virtual OP_STATUS drawString(class VEGAFont* font, int x, int y, const struct ProcessedString* processed_string, VEGAStencil* stencil);
# if defined(VEGA_NATIVE_FONT_SUPPORT) || defined(CSS_TRANSFORMS)
	virtual OP_STATUS drawString(class VEGAFont* font, int x, int y, const uni_char* text, UINT32 len, INT32 extra_char_space, short word_width, VEGAStencil* stencil);
# endif // VEGA_NATIVE_FONT_SUPPORT || CSS_TRANSFORMS
#endif // VEGA_OPPAINTER_SUPPORT

	OP_STATUS applyFilter(VEGAFilter* filter, const VEGAFilterRegion& region);

	OP_STATUS updateQuality();

	OP_STATUS createBitmapRenderTarget(VEGARenderTarget** rt, OpBitmap* bmp);
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_STATUS createWindowRenderTarget(VEGARenderTarget** rt, VEGAWindow* window);
	OP_STATUS createBufferRenderTarget(VEGARenderTarget** rt, VEGAPixelStore* ps);
#endif // VEGA_OPPAINTER_SUPPORT
	OP_STATUS createIntermediateRenderTarget(VEGARenderTarget** rt, unsigned int w, unsigned int h);

	OP_STATUS createStencil(VEGAStencil** sten, bool component, unsigned int w, unsigned int h);

#ifdef VEGA_LIMIT_BITMAP_SIZE
	static unsigned maxBitmapSide() { return 0; }
#endif // VEGA_LIMIT_BITMAP_SIZE
	static OP_STATUS createBitmapStore(VEGABackingStore** store, unsigned w, unsigned h, bool is_indexed, bool is_opaque);
	virtual bool supportsStore(VEGABackingStore* store);

#ifdef CANVAS3D_SUPPORT
	OP_STATUS createImage(VEGAImage* img, VEGA3dTexture* tex, bool isPremultiplied);
#endif // CANVAS3D_SUPPORT

	unsigned calculateArea(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy);

protected:
	VEGARasterizer rasterizer;
	VEGASWBuffer* buffer;

	void drawSpans(VEGASpanInfo* raster_spans, unsigned int span_count);

	UINT8* maskscratch;

	void maskOpaqueSpan(VEGASpanInfo& span);
	void maskNonOpaqueSpan(VEGASpanInfo& span);

	void prepareStencil(VEGAStencil* stencil);
	void finishStencil(VEGAStencil* stencil);

	struct Context
	{
		VEGASWBuffer* stencil_buffer;
		int stencil_ofs_x;
		int stencil_ofs_y;
		bool stencil_is_a8;
	} context;
};

/* Backing by a VEGASWBuffer - rw */
class VEGABackingStore_SWBuffer : public VEGABackingStore
{
public:
	VEGABackingStore_SWBuffer() { m_buffer.Reset(); }
	virtual ~VEGABackingStore_SWBuffer();

	OP_STATUS Construct(unsigned width, unsigned height, VEGASWBufferType type, bool opaque);

	virtual bool IsA(Type type) const { return type == SWBUFFER; }

	virtual void SetColor(const OpRect& rect, UINT32 color);

	virtual OP_STATUS CopyRect(const OpPoint& dstp, const OpRect& srcr, VEGABackingStore* store);

	virtual VEGASWBuffer* BeginTransaction(const OpRect& rect, AccessType acc)
	{
		m_transaction_buffer = m_buffer.CreateSubset(rect.x, rect.y, rect.width, rect.height);
		return &m_transaction_buffer;
	}
	virtual void EndTransaction(BOOL commit) {}

	virtual BOOL IsIndexed() const { return m_buffer.IsIndexed(); }
	virtual VEGA_PIXEL* GetPalette() const { return m_buffer.GetPalette(); }

	virtual unsigned GetWidth() const { return m_buffer.width; }
	virtual unsigned GetHeight() const { return m_buffer.height; }
	virtual unsigned GetBytesPerLine() const { return m_buffer.pix_stride * m_buffer.bpp; }
	virtual unsigned GetBytesPerPixel() const { return m_buffer.bpp; }

	VEGASWBuffer* GetBuffer() { return &m_buffer; }

protected:
	VEGASWBuffer m_buffer;
	VEGASWBuffer m_transaction_buffer;
};

#ifdef VEGA_OPPAINTER_SUPPORT
/* Backing by window pixel store */
class VEGABackingStore_WindowBuffer : public VEGABackingStore_SWBuffer
{
public:
	VEGABackingStore_WindowBuffer() : m_window(NULL), m_ps_buf(NULL) {}
	virtual ~VEGABackingStore_WindowBuffer();

	OP_STATUS Construct(VEGAWindow* window);

	virtual unsigned GetWidth() const;
	virtual unsigned GetHeight() const;

	virtual void Validate();
	virtual void Flush(const OpRect* update_rects, unsigned int num_rects);

protected:
	void UpdateBuffer();

	VEGAWindow* m_window;
	void* m_ps_buf;
};

/* Backing by pixel store */
class VEGABackingStore_PixelStore : public VEGABackingStore_SWBuffer
{
public:
	VEGABackingStore_PixelStore(VEGAPixelStore* pixelStore) : m_pixelStore(pixelStore) {}
	virtual ~VEGABackingStore_PixelStore();

	/**
	 * This backing store will be bound to the
	 * pixel store for its entire lifespan.
	 */
	OP_STATUS Construct();

	virtual void Validate();
	virtual void Flush(const OpRect* update_rects, unsigned int num_rects);
	virtual void Invalidate();

protected:
	VEGAPixelStore* m_pixelStore;
};

#endif // VEGA_OPPAINTER_SUPPORT

#ifndef VEGA_OPPAINTER_SUPPORT
/* Backing by OpBitmap (the non-VEGA kind) */
class VEGABackingStore_OpBitmap : public VEGABackingStore_SWBuffer
{
public:
	VEGABackingStore_OpBitmap() : m_bitmap(NULL), m_using_pointer(false) {}
	virtual ~VEGABackingStore_OpBitmap();

	OP_STATUS Construct(OpBitmap* bitmap);

	virtual void Flush(const OpRect* update_rects, unsigned int num_rects);

protected:
	OpBitmap* m_bitmap;
	bool m_using_pointer;
};
#endif // !VEGA_OPPAINTER_SUPPORT

#endif // VEGA_SUPPORT
#endif // VEGABACKEND_SW_H
