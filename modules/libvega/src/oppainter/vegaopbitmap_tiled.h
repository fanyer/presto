/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAOPBITMAP_TILED_H
#define VEGAOPBITMAP_TILED_H

# if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_LIMIT_BITMAP_SIZE)

#  include "modules/libvega/src/oppainter/vegaopbitmap.h"

/** A tile in a tiled bitmap. Bitmaps are tiled when they exceed
  * the maximum size, determined by the backend. */
class VEGABitmapTile : public ListElement<VEGABitmapTile>
{
public:
	VEGABitmapTile() : bmp(NULL) {}
	~VEGABitmapTile();
	void toTile(OpRect& r) { r.x -= rect.x; r.y -= rect.y; }
	class VEGAOpBitmap* bmp; /// the bitmap for this tile
	OpRect rect; /// the tile rect, relative to the top of the entire bitmap
};

/// A row of bitmap tiles.
class VEGABitmapTileRow : public AutoDeleteList<VEGABitmapTile>
                        , public ListElement<VEGABitmapTileRow>
{};

/** Storage for bitmap tiles. When a bitmap exceeds the maximum
  * dimensions determined by the backend it is split into tiles. These
  * tiles are stored in rows in this helper class, that also provides
  * the means for iterating over relevant tiles. */
class VEGABitmapTileCollection : public AutoDeleteList<VEGABitmapTileRow>
{
public:
	/// Get row of tiles containing line.
	VEGABitmapTileRow* getRowContaining(INT32 line) const;
};

/** Base for iterators visiting bitmap tiles. Iterators are created
  * locally from functions in VEGAOpPainter and are used to visit the
  * relevant tiles. */
class VEGATileIterator
{
public:
	VEGATileIterator() : m_status(OpStatus::OK) {}
	/// Implementors sould call updateStatus for each visited tile, and return m_status.
	virtual OP_STATUS iterate(const VEGABitmapTileCollection& tiles) = 0;
protected:
	void updateStatus(OP_STATUS s)
	{
		if (OpStatus::IsError(s) && !OpStatus::IsMemoryError(m_status))
			m_status = s;
	}
	OP_STATUS m_status;
};

/// Base for iterators visiting all tiles.
class VEGAEachIterator : public VEGATileIterator
{
public:
	OP_STATUS iterate(const VEGABitmapTileCollection& tiles);
protected:
	virtual OP_STATUS action(VEGABitmapTile& tile) = 0;
};

/// Base for iterators visiting the row of tiles containing line.
class VEGALineIterator : public VEGATileIterator
{
public:
	VEGALineIterator(INT32 line) : m_line(line) {}
	OP_STATUS iterate(const VEGABitmapTileCollection& tiles);
protected:
	// @param line the line to do something with, relative to the tile
	virtual OP_STATUS action(VEGABitmapTile& tile, INT32 line) = 0;
private:
	const INT32 m_line;
};

/// Base for iterators visiting all tiles intersecting rect.
class VEGARectIterator : public VEGATileIterator
{
public:
	VEGARectIterator(const OpRect& rect) : m_rect(rect) {}
	OP_STATUS iterate(const VEGABitmapTileCollection& tiles);
protected:
	/** Perform some action on tile. All tiles intersecting the rect passed to the constructor will be visited.
	  * @param tile the currently visited tile
	  * @param rect the intersection between the rect passed to the constructor and tile, relative to tile
	  * @param src_offs_x accumulated x offset: 0 for first tile in row, width of intersected rect accumulates
	  * @param src_offs_y accumulated y offset: 0 for first row, height of intersected row accumulates */
	virtual OP_STATUS action(VEGABitmapTile& tile, const OpRect& rect, const unsigned src_offs_x, const unsigned src_offs_y) = 0;
	const OpRect m_rect;
};

class VEGADoubleRectIterator : public VEGATileIterator
{
public:
	VEGADoubleRectIterator(const double* rect) : m_rect(rect) {}
	OP_STATUS iterate(const VEGABitmapTileCollection& tiles);
protected:
	enum
	{
		RX = 0,
		RY = 1,
		RW = 2,
		RH = 3,
		RZ = 4
	};
	/// intersect r with w and make r relative to w
	void DRectIsectRel(double* r, const OpRect& w);

	/** Perform some action on tile. All tiles intersecting the rect passed to the constructor will be visited.
	  * @param tile the currently visited tile
	  * @param rect the intersection between the rect passed to the constructor and tile, relative to tile
	  * @param src_offs_x accumulated x offset: 0 for first tile in row, width of intersected rect accumulates
	  * @param src_offs_y accumulated y offset: 0 for first row, height of intersected row accumulates */
	virtual OP_STATUS action(VEGABitmapTile& tile, const double* rect, const double src_offs_x, const double src_offs_y) = 0;
	const double* m_rect;
};

# endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && VEGA_LIMIT_BITMAP_SIZE

#endif // !VEGAOPBITMAP_TILED_H
