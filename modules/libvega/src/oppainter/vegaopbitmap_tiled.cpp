/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_LIMIT_BITMAP_SIZE)

#include "modules/libvega/src/oppainter/vegaopbitmap_tiled.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/src/canvas/canvascontext2d.h"
#include "modules/util/opautoptr.h"

// uncomment to draw red outlines for each tile
// #define DEBUG_TILED_BITMAPS

VEGABitmapTile::~VEGABitmapTile() { OP_DELETE(bmp); }

VEGABitmapTileRow* VEGABitmapTileCollection::getRowContaining(INT32 line) const
{
	// assumes first tile row starts with line 0 and there are no gaps between tile rows
	if (line < 0)
		return NULL;
	for (VEGABitmapTileRow* row = First(); row; row = row->Suc())
	{
		VEGABitmapTile* t = row->First();
		OP_ASSERT(t);
		const OpRect& r = t->rect;
		if (line < r.y + r.height)
		{
			OP_ASSERT(line >= r.y);
			return row;
		}
	}
	return NULL;
}


// iterate functions for iterators defined in vegaopbitmap_tiled.h

OP_STATUS VEGAEachIterator::iterate(const VEGABitmapTileCollection& tiles)
{
	for (VEGABitmapTileRow* r = tiles.First(); r; r = r->Suc())
		for (VEGABitmapTile* t = r->First(); t; t = t->Suc())
			updateStatus(action(*t));
	return m_status;
}

OP_STATUS VEGALineIterator::iterate(const VEGABitmapTileCollection& tiles)
{
	VEGABitmapTileRow* row = tiles.getRowContaining(m_line);
	if (!row)
		return OpStatus::ERR;
	VEGABitmapTile* t = row->First();
	OP_ASSERT(t);
	const INT32 line = m_line - t->rect.y;
	OP_ASSERT(line >= 0 && line < t->rect.height);
	for (; t; t = t->Suc())
		updateStatus(action(*t, line));
	return m_status;
}

OP_STATUS VEGARectIterator::iterate(const VEGABitmapTileCollection& tiles)
{
	unsigned src_offs_y = 0;
	for (VEGABitmapTileRow* r = tiles.getRowContaining(m_rect.y); r; r = r->Suc())
	{
		VEGABitmapTile* t = r->First();
		if (m_rect.y + m_rect.height <= t->rect.y)
			break; // t below rect

		unsigned src_offs_x = 0;
		OpRect i;
		for (; t; t = t->Suc())
		{
			if (m_rect.x >= t->rect.x + t->rect.width)
				continue; // t left of rect

			if (m_rect.x + m_rect.width <= t->rect.x)
				break; // t right of rect

			i.Set(m_rect);
			i.IntersectWith(t->rect);
			t->toTile(i);

			updateStatus(action(*t, i, src_offs_x, src_offs_y));

			src_offs_x += i.width;
		}

		src_offs_y += i.height;
	}
	return m_status;
}

void VEGADoubleRectIterator::DRectIsectRel(double* r, const OpRect& w)
{
	const double right  = r[RX] + r[RW];
	if (r[RX] < w.x)
	{
		r[RX] = (double)w.x;
		r[RW] = right - r[RX];
	}
	if (right > w.x+w.width)
		r[RW] = (double)(w.x+w.width) - r[RX];
	r[RX] -= (double)w.x;

	const double bottom = r[RY] + r[RH];
	if (r[RY] < w.y)
	{
		r[RY] = (double)w.y;
		r[RH] = bottom - r[RY];
	}
	if (bottom > w.y+w.height)
		r[RH] = (double)(w.y+w.height) - r[RY];
	r[RY] -= (double)w.y;
}

OP_STATUS VEGADoubleRectIterator::iterate(const VEGABitmapTileCollection& tiles)
{
	double src_offs_y = 0;
	for (VEGABitmapTileRow* r = tiles.getRowContaining((INT32)m_rect[RY]); r; r = r->Suc())
	{
		VEGABitmapTile* t = r->First();
		if (m_rect[RY] + m_rect[RH] <= t->rect.y)
			break; // t below rect

		double src_offs_x = 0;
		double i[RZ];
		for (; t; t = t->Suc())
		{
			if (m_rect[RX] >= t->rect.x + t->rect.width)
				continue; // t left of rect

			if (m_rect[RX] + m_rect[RW] <= t->rect.x)
				break; // t right of rect

			op_memcpy(i, m_rect, RZ*sizeof(*i));
			DRectIsectRel(i, t->rect);

			updateStatus(action(*t, i, src_offs_x, src_offs_y));

			src_offs_x += i[RW];
		}

		src_offs_y += i[RH];
	}
	return m_status;
}


// custom iterators used from tile-related functions in VEGAOpBitmap

// calls VEGAOpPainter::PaintImage
class PaintImageIterator : public VEGARectIterator
{
public:
	PaintImageIterator(VEGAOpPainter* painter, const VEGADrawImageInfo& diinfo)
	: VEGARectIterator(OpRect(diinfo.srcx, diinfo.srcy, diinfo.srcw, diinfo.srch))
	, painter(painter), diinfo(diinfo)
	{
		// will divide by these later
		OP_ASSERT(diinfo.type != VEGADrawImageInfo::SCALED || (diinfo.srcw && diinfo.srch));
	}
	OP_STATUS action(VEGABitmapTile& tile, const OpRect& rect, const unsigned src_offs_x, const unsigned src_offs_y)
	{
		// figure out destination rect for this tile based on diinfo, rect and tile.rect
		VEGADrawImageInfo ditile = diinfo;
		if (diinfo.type == VEGADrawImageInfo::SCALED)
		{
			// must scale right and bottom rather than width and
			// height, or there'll be gaps between tiles
			int l =  src_offs_x                * diinfo.dstw / diinfo.srcw;
			int t =  src_offs_y                * diinfo.dsth / diinfo.srch;
			int r = (src_offs_x + rect.width)  * diinfo.dstw / diinfo.srcw;
			int b = (src_offs_y + rect.height) * diinfo.dsth / diinfo.srch;
			ditile.dstx += l;
			ditile.dsty += t;
			ditile.dstw  = r - l;
			ditile.dsth  = b - t;
		}
		else
		{
			ditile.dstx += src_offs_x;
			ditile.dsty += src_offs_y;
			ditile.dstw  = rect.width;
			ditile.dsth  = rect.height;
		}
		ditile.srcx = rect.x;
		ditile.srcy = rect.y;
		ditile.srcw = rect.width;
		ditile.srch = rect.height;

		const OP_STATUS status = painter->PaintImage(tile.bmp, ditile);
#ifdef DEBUG_TILED_BITMAPS
		// HACK: manually subtracting 40px UI offset
		OpRect r(ditile.dstx, ditile.dsty - 40, ditile.dstw, ditile.dsth);
		const UINT32 old = painter->GetColor();
		painter->SetColor(255,0,0,255);
		painter->DrawRect(r, 1);
		painter->SetColor(OP_GET_R_VALUE(old), OP_GET_G_VALUE(old), OP_GET_B_VALUE(old), OP_GET_A_VALUE(old));
#endif // DEBUG_TILED_BITMAPS
		return status;
	}
private:
	VEGAOpPainter* painter;
	const VEGADrawImageInfo& diinfo;
};

// calls VEGARenderTarget::copyToBitmap, or VEGAOpBitmap::CopyRect (when target is tiled)
class CopyRectIterator : public VEGARectIterator
{
public:
	CopyRectIterator(const OpRect& target_rect, VEGARenderTarget* src_rt, unsigned int srcx, unsigned int srcy)
	: VEGARectIterator(target_rect)
	, src_rt(src_rt), src_bmp(NULL), srcx(srcx), srcy(srcy)
	{}
	CopyRectIterator(const OpRect& target_rect, VEGAOpBitmap* src_bmp, unsigned int srcx, unsigned int srcy)
	: VEGARectIterator(target_rect)
	, src_rt(NULL), src_bmp(src_bmp), srcx(srcx), srcy(srcy)
	{}
	OP_STATUS action(VEGABitmapTile& tile, const OpRect& target_rect, const unsigned src_offs_x, const unsigned src_offs_y)
	{
		if (src_rt)
			return src_rt->copyToBitmap(tile.bmp,
			                            target_rect.width, target_rect.height,
			                            srcx + src_offs_x, srcy + src_offs_y,
			                            target_rect.x, target_rect.y);
		if (src_bmp)
			return tile.bmp->CopyRect(OpPoint(target_rect.x, target_rect.y),
			                          OpRect(srcx + src_offs_x, srcy + src_offs_y, target_rect.width, target_rect.height),
			                          src_bmp);
		OP_ASSERT(!"should never be reached");
		return OpStatus::ERR;
	}
private:
	VEGARenderTarget* src_rt;
	VEGAOpBitmap* src_bmp;
	const unsigned srcx, srcy;
};

// calls VEGAOpBitmap::CopyRect 'in reverse', ie when source bitmap is tiled
class CopyFromRectIterator : public VEGARectIterator
{
public:
	CopyFromRectIterator(const OpRect& src_rect, VEGAOpBitmap* target_bmp, unsigned int dstx, unsigned int dsty)
	: VEGARectIterator(src_rect)
	, target_bmp(target_bmp), dstx(dstx), dsty(dsty)
	{}
	OP_STATUS action(VEGABitmapTile& tile, const OpRect& src_rect, const unsigned dst_offs_x, const unsigned dst_offs_y)
	{
		return target_bmp->CopyRect(OpPoint(dstx + dst_offs_x, dsty + dst_offs_y), src_rect, tile.bmp);
	}
private:
	VEGAOpBitmap* target_bmp;
	const unsigned dstx, dsty;
};

// calls CanvasContext2D::drawImage
class CanvasPaintImageIterator : public VEGADoubleRectIterator
{
public:
	CanvasPaintImageIterator(CanvasContext2D* ctx, const double* src, const double* dst)
		: VEGADoubleRectIterator(src), ctx(ctx), dst(dst)
	{
		// will divide by these later
		OP_ASSERT(dst[RW]);
		OP_ASSERT(dst[RH]);
	}
	OP_STATUS action(VEGABitmapTile& tile, const double* src, const double src_offs_x, const double src_offs_y)
	{
		const double fact_x = dst[RW] / m_rect[RW];
		const double fact_y = dst[RH] / m_rect[RH];
		double src_tile[] =
		{
			src[RX],
			src[RY],
			src[RW],
			src[RH]
		};

		// except for outer edges, dest rect must be integral or there'll be blending issues

		double l =  dst[RX];
		if (src_offs_x) // not first in row
			l = (int)(0.5 + l + src_offs_x * fact_x);

		double t =  dst[RY];
		if (src_offs_y) // not first row
			t = (int)(0.5 + t + src_offs_y * fact_y);

		double r = dst[RX] + dst[RW];
		if (src_offs_x + src[RW] < m_rect[RX] + m_rect[RW]) // not last in row
			r = (int)(0.5 + l + src[RW] * fact_x);

		double b = dst[RY] + dst[RH];
		if (src_offs_y + src[RH] < m_rect[RY] + m_rect[RH]) // not last row
			b = (int)(0.5 + t + src[RH] * fact_y);

		double dst_tile[] =
		{
			l,
			t,
			r - l,
			b - t
		};

		return ctx->drawImage(tile.bmp, src_tile, dst_tile);
	}
private:
	CanvasContext2D* ctx;
	const double* dst;
};


// tile-related functions in VEGAOpBitmap

// paint this bitmap according to diinfo
OP_STATUS VEGAOpBitmap::paintTiled(VEGAOpPainter* painter, VEGADrawImageInfo& diinfo)
{
	OP_ASSERT(isTiled());
	PaintImageIterator it(painter, diinfo);
	return it.iterate(*bitmap_tiles);
}

// copy rect [srcx, srcy, width, height] from render target to point [dstx, dsty] in this bitmap
OP_STATUS VEGAOpBitmap::copyTiled(VEGARenderTarget* src_rt,
                                  unsigned int width, unsigned int height,
                                  unsigned int srcx, unsigned int srcy,
                                  unsigned int dstx, unsigned int dsty)
{
	OpRect tgt(dstx, dsty, (width ? width : Width()), (height ? height : Height()));
	CopyRectIterator it(tgt, src_rt, srcx, srcy);
	return it.iterate(*bitmap_tiles);
}

// copy rect src_rect from src_bmp to point dest_point in this bitmap
OP_STATUS VEGAOpBitmap::copyTiled(const OpPoint& dest_point, const OpRect& src_rect, VEGAOpBitmap* src_bmp)
{
	OP_ASSERT(isTiled());

	OpRect tgt(dest_point.x, dest_point.y, src_rect.width, src_rect.height);
	CopyRectIterator it(tgt, src_bmp, src_rect.x, src_rect.y);
	return it.iterate(*bitmap_tiles);
}

// copy rect src_rect from this bitmap to point dst_point in target_bmp
OP_STATUS VEGAOpBitmap::copyFromTiled(const OpRect& src_rect, const OpPoint& dst_point, VEGAOpBitmap* target_bmp)
{
	OP_ASSERT(isTiled());

	CopyFromRectIterator it(src_rect, target_bmp, dst_point.x, dst_point.y);
	return it.iterate(*bitmap_tiles);
}

// draw part of this bitmap defined by rect src to rect dst in ctx
OP_STATUS VEGAOpBitmap::drawCanvasTiled(CanvasContext2D* ctx, const double* src, const double* dst)
{
	CanvasPaintImageIterator it(ctx, src, dst);
	return it.iterate(*bitmap_tiles);
}

// static
bool VEGAOpBitmap::needsTiling(UINT32 width, UINT32 height)
{
	const unsigned max_side = VEGARendererBackend::maxBitmapSide();
	return max_side && (width > max_side || height > max_side);
}

OP_STATUS VEGAOpBitmap::ConstructTiles(const unsigned tile_side, BOOL is_indexed)
{
	OP_ASSERT(tile_side);
	OpAutoPtr<VEGABitmapTileCollection> tiles(OP_NEW(VEGABitmapTileCollection, ()));
	RETURN_OOM_IF_NULL(tiles.get());

	const unsigned tiles_x = (width  + tile_side-1) / tile_side;
	const unsigned tiles_y = (height + tile_side-1) / tile_side;

	unsigned y_pos = 0;
	for (unsigned y = 0; y < tiles_y; ++y)
	{
		const unsigned tile_h = MIN(tile_side, height - y_pos);
		unsigned x_pos = 0;

		OpAutoPtr<VEGABitmapTileRow> row(OP_NEW(VEGABitmapTileRow, ()));
		RETURN_OOM_IF_NULL(row.get());

		for (unsigned x = 0; x < tiles_x; ++x)
		{
			const unsigned tile_w = MIN(tile_side, width - x_pos);

			OpAutoPtr<VEGABitmapTile> tile(OP_NEW(VEGABitmapTile, ()));
			RETURN_OOM_IF_NULL(tile.get());

			tile->bmp = OP_NEW(VEGAOpBitmap, (tile_w, tile_h, transparent, alpha, transColor, 0/*unused*/));
			RETURN_OOM_IF_NULL(tile->bmp);
			RETURN_IF_ERROR(tile->bmp->Construct(FALSE, is_indexed));
			tile->rect.Set(x_pos, y_pos, tile_w, tile_h);

			tile.release()->Into(row.get());
			x_pos += tile_w;
		}
		OP_ASSERT(x_pos == width);

		row.release()->Into(tiles.get());
		y_pos += tile_h;
	}
	OP_ASSERT(y_pos == height);

	bitmap_tiles = tiles.release();
	return OpStatus::OK;
}

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && VEGA_LIMIT_BITMAP_SIZE
