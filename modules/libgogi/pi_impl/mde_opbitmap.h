/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_OPBITMAP_H
#define MDE_OPBITMAP_H

#include "modules/pi/OpBitmap.h"
#include "modules/util/simset.h"

struct MDE_BUFFER;

class OpPainter;

#include "modules/libgogi/mde.h"

#ifdef MDE_SUPPORT_SPLIT_BITMAPS

/*
  Put each one of these in order, starting from
  y_pos 0, continuing with y_pos = y_pos + buffer_height.
 */
class SplitBitmapElm : public Link
{
public:
	SplitBitmapElm(MDE_BUFFER* buf, int y);
	~SplitBitmapElm();

	MDE_BUFFER* buffer;
	int y_pos;
};

#endif // MDE_SUPPORT_SPLIT_BITMAPS

class MDE_OpBitmap : public OpBitmap
{
	friend class MDE_OpPainter;
public:
	MDE_OpBitmap();
	OP_STATUS Init(UINT32 w, UINT32 h, BOOL transparent, BOOL alpha, UINT32 transcol, INT32 indexed, BOOL must_support_painter);
	virtual OP_STATUS InitStatus();
	virtual ~MDE_OpBitmap();
	virtual BOOL Supports(SUPPORTS supports) const;
	virtual UINT32 GetBytesPerLine() const;
	virtual OP_STATUS AddLine(void* data, INT32 line);
#ifdef SUPPORT_INDEXED_OPBITMAP
	virtual OP_STATUS AddIndexedLine(void* data,  INT32 line);
	virtual BOOL GetIndexedLineData(void* data, UINT32 line) const;
	virtual BOOL SetPalette(UINT8* pal,  UINT32 num_colors);
	virtual BOOL GetPalette(UINT8* pal) const;
#endif // SUPPORT_INDEXED_OPBITMAP
	virtual BOOL SetColor(UINT8*, BOOL, const OpRect *);
	virtual BOOL GetLineData(void* data, UINT32 line) const;
	virtual UINT32 Width() const;
	virtual UINT32 Height() const;
	virtual UINT8 GetBpp() const;
	virtual BOOL HasAlpha() const;
	virtual BOOL IsTransparent() const;
	virtual UINT32 GetTransparentColorIndex() const;

#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	SplitBitmapElm* GetFirstSplitBitmapElm();
	SplitBitmapElm* GetNextSplitBitmapElm();
#endif // MDE_SUPPORT_SPLIT_BITMAPS

	MDE_BUFFER *GetMDEBuffer();

	OpPainter* GetPainter();
	void ReleasePainter();

#ifdef OPBITMAP_POINTER_ACCESS
	virtual void* GetPointer(AccessType access_type = ACCESS_READWRITE);
	virtual void ReleasePointer(BOOL changed = TRUE);
#else
	virtual void* GetPointer() const;
	virtual void ReleasePointer() const;
#endif // OPBITMAP_POINTER_ACCESS

#ifdef LIBGOGI_SUPPORT_CREATE_TILE
	// Optional functions that are needed for efficient blitting of
	// skins and background images.
	virtual OpBitmap* GetTile(UINT32 horizontal_count, UINT32 vertical_count);
	virtual void ReleaseTile(OpBitmap*);
	virtual OpBitmap* CreateTile(UINT32 width, UINT32 height);
#endif // LIBGOGI_SUPPORT_CREATE_TILE

#ifdef MDE_LIMIT_IMAGE_SIZE
    UINT32 RealWidth() const {return real_width;}
    UINT32 RealHeight() const {return real_height;}
#endif // MDE_LIMIT_IMAGE_SIZE

private:

#ifdef MDE_LIMIT_IMAGE_SIZE
    UINT32 real_width, real_height;
    UINT32 scale;
#endif // MDE_LIMIT_IMAGE_SIZE

	OP_STATUS CreateInternalBuffers(MDE_FORMAT format, int extra);
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	OP_STATUS CreateSplitBuffers(MDE_FORMAT format);
	SplitBitmapElm* FindSplitBitmapElm(int line) const;
#endif // MDE_SUPPORT_SPLIT_BITMAPS

	BOOL SetColorInternal(MDE_BUFFER* buffer, UINT8* color, const OpRect& rect);

	UINT32 width, height;
	BOOL hasAlpha;
	BOOL isTransparent;
#ifdef SUPPORT_INDEXED_OPBITMAP
	BOOL isIndexed;
#endif
	UINT32 transparentColor;

	UINT32 rowstride;

	OpPainter* painter;
	BOOL must_support_painter;
#ifdef MDE_SUPPORT_SPLIT_BITMAPS
	Head split_bitmap_list;
	SplitBitmapElm* current_split_bitmap_elm;
#endif // MDE_SUPPORT_SPLIT_BITMAPS
	MDE_BUFFER *buffer;
#ifdef LIBGOGI_SUPPORT_CREATE_TILE
	MDE_OpBitmap* current_tile;
#endif // LIBGOGI_SUPPORT_CREATE_TILE
	OP_STATUS m_initStatus;
};

#endif // MDE_OPBITMAP_H
