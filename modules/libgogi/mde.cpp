#include "core/pch.h"
#include "modules/libgogi/mde.h"

// == MDE_Region ==========================================================

MDE_Region::MDE_Region()
	: rects(NULL)
	, num_rects(0)
	, max_rects(0)
{
}

MDE_Region::~MDE_Region()
{
	Reset();
}

void MDE_Region::Swap(MDE_Region *other)
{
	int _max_rects = max_rects;
	int _num_rects = num_rects;
	MDE_RECT * _rects = rects;
	max_rects = other->max_rects;
	num_rects = other->num_rects;
	rects = other->rects;
	other->max_rects = _max_rects;
	other->num_rects = _num_rects;
	other->rects = _rects;
}

void MDE_Region::Offset(int dx, int dy)
{
	for(int i = 0; i < num_rects; i++)
	{
		rects[i].x += dx;
		rects[i].y += dy;
	}
}

void MDE_Region::Reset(bool free_mem)
{
	if (free_mem)
	{
/*#ifdef _DEBUG
		for(int i = 0; i < num_rects; i++)
			for(int j = 0; j < num_rects; j++)
				if (i != j && MDE_RectIntersects(rects[i], rects[j]))
				{
					MDE_ASSERT(false);
				}
#endif*/
		OP_DELETEA(rects);
		rects = NULL;
		max_rects = 0;
	}
	num_rects = 0;
}

bool MDE_Region::Set(MDE_RECT rect)
{
	Reset();
	return AddRect(rect);
}

bool MDE_Region::GrowIfNeeded()
{
	if(num_rects == max_rects)
	{
		int new_max_rects = (max_rects == 0 ? 1 : max_rects + 4);
		MDE_RECT *new_rects = OP_NEWA(MDE_RECT, new_max_rects);
		if (!new_rects)
			return false;

		if(rects)
			op_memmove(new_rects, rects, sizeof(MDE_RECT) * max_rects);

		OP_DELETEA(rects);
		rects = new_rects;
		max_rects = new_max_rects;
	}
	return true;
}

bool MDE_Region::AddRect(MDE_RECT rect)
{
	MDE_ASSERT(!MDE_RectIsEmpty(rect));
	if (!GrowIfNeeded())
		return false;
	rects[num_rects++] = rect;
/*#ifdef _DEBUG
	static maxcount = 0;
	if (num_rects > maxcount)
	{
		maxcount = num_rects;
		P_DEBUG_PRINTF("%d\n", maxcount);
	}
#endif*/
	return true;
}

void MDE_Region::RemoveRect(int index)
{
	MDE_ASSERT(index >= 0 && index < num_rects);
	if(index < num_rects - 1)
		for(int i = index; i < num_rects - 1; i++)
			rects[i] = rects[i + 1];
	num_rects--;
}

bool MDE_Region::IncludeRect(MDE_RECT rect)
{
	MDE_RECT r = rect;

/*	Old version. Big rectangles as result.
	for(int i = 0; i < num_rects; i++)
	{
		if (MDE_RectIntersects(r, rects[i]))
		{
			r = MDE_RectUnion(r, rects[i]);
			RemoveRect(i);
			i = -1; // 0 next loop
		}
	}*/

	for(int i = 0; i < num_rects; i++)
	{
		if (MDE_RectIntersects(r, rects[i]))
		{
			MDE_Region r_split;
			if (!r_split.ExcludeRect(r, rects[i]))
				return false;
			for(int j = 0; j < r_split.num_rects; j++)
			{
				if (!IncludeRect(r_split.rects[j]))
					return false;
			}
			return true;
		}
	}
	return AddRect(r);
}

bool MDE_Region::ExcludeRect(MDE_RECT rect, MDE_RECT remove)
{
	MDE_ASSERT(MDE_RectIntersects(rect, remove));

	remove = MDE_RectClip(remove, rect);

	// Create rectangles that surround the clipped remove rect.
	//
	// Creating vertical slices is preferred over horizontal slices to make scrolling
	// overlapped views faster vertically. This wouldn't matter if subsequent scrolls
	// exclude themselves from the invalidation caused by the other scrolls.
	//
	// Details:
	//
	// Vertical slices                           Horizontal slices
	// ####################################		 ####################################
	// #               .    .             #		 #                                  #
	// #               .top .             #		 #                top               #
	// #               .    .             #		 #                                  #
	// #               !!!!!!             #		 #!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!#
	// #               ######             #		 #...............######.............#
	// #   left        #    #     right   #		 #   left        #    #     right   #
	// #               #    #             #		 #!!!!!!!!!!!!!!!#    #!!!!!!!!!!!!!#
	// #               ######             #		 #...............######.............#
	// #               .    .             #		 #                                  #
	// #               bottom             #		 #               bottom             #
	// #               .    .             #		 #                                  #
	// #!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!#		 #!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!#
	// ####################################		 ####################################
	//
	// Showing the invalidated parts with "!" for vertical vs horizontal slices after a
	// typical scroll.
	// Since close enough invalidated areas are unioned to larger areas, the horizontal
	// slices would cause many vertical scrolls to invalidate most of the screen!

	// Top
	if (remove.y > rect.y)
		if (!AddRect(MDE_MakeRect(remove.x, rect.y, remove.w, remove.y - rect.y)))
			return false;

	// Left
	if (remove.x > rect.x)
		if (!AddRect(MDE_MakeRect(rect.x, rect.y, remove.x - rect.x, rect.h)))
			return false;

	// Right
	if (remove.x + remove.w < rect.x + rect.w)
		if (!AddRect(MDE_MakeRect(remove.x + remove.w, rect.y, rect.x + rect.w - (remove.x + remove.w), rect.h)))
			return false;

	// Bottom
	if (remove.y + remove.h < rect.y + rect.h)
		if (!AddRect(MDE_MakeRect(remove.x, remove.y + remove.h, remove.w, rect.y + rect.h - (remove.y + remove.h))))
			return false;

	return true;
}

bool MDE_Region::ExcludeRect(const MDE_RECT &rect)
{
	// Split all intersecting rectangles
	int num_to_split = num_rects;
	for(int i = 0; i < num_to_split; i++)
	{
		if (MDE_RectIntersects(rects[i], rect))
		{
			if (!ExcludeRect(rects[i], rect))
				return false;
			RemoveRect(i);
			num_to_split--;
			i--;
		}
	}
#ifdef _DEBUG
	for(int k = 0; k < num_rects; k++)
		for(int m = 0; m < num_rects; m++)
			if (k != m && MDE_RectIntersects(rects[k], rects[m]))
			{
				MDE_ASSERT(0);
			}
#endif
	return true;
}

void MDE_Region::CoalesceRects()
{
	for(int i = 0; i < num_rects; i++)
	{
		for(int j = 0; j < num_rects; j++)
		{
			if (i == j)
				continue;
			if (i > num_rects - 1)
				break;
			if (// Vertical
				(rects[i].x == rects[j].x && rects[i].w == rects[j].w &&
				((rects[i].y + rects[i].h == rects[j].y) || (rects[j].y + rects[j].h == rects[i].y)))
				|| // Horizontal
				(rects[i].y == rects[j].y && rects[i].h == rects[j].h &&
				((rects[i].x + rects[i].w == rects[j].x) || (rects[j].x + rects[j].w == rects[i].x)))
				)
			{
				rects[i] = MDE_RectUnion(rects[i], rects[j]);
				RemoveRect(j);
				j--;
			}
		}
	}
}

// == MDE_RECT ============================================================

MDE_RECT MDE_MakeRect(int x, int y, int w, int h)
{
	MDE_RECT tmp = { x, y, w, h };
	return tmp;
}

bool MDE_RectIntersects(const MDE_RECT &this_rect, const MDE_RECT &with_rect)
{
#ifdef MDE_PEDANTIC_ASSERTS
	MDE_ASSERT(!MDE_RectIsInsideOut(this_rect));
	MDE_ASSERT(!MDE_RectIsEmpty(this_rect));
	MDE_ASSERT(!MDE_RectIsInsideOut(with_rect));
	MDE_ASSERT(!MDE_RectIsEmpty(with_rect));
#endif
	if (MDE_RectIsEmpty(this_rect) || MDE_RectIsEmpty(with_rect))
		return false;

	return !( (this_rect.x >= with_rect.x + with_rect.w) || (this_rect.x + this_rect.w <= with_rect.x) ||
			(this_rect.y >= with_rect.y + with_rect.h) || (this_rect.y + this_rect.h <= with_rect.y) );
}

bool MDE_RectContains(const MDE_RECT &rect, int x, int y)
{
	MDE_ASSERT(!MDE_RectIsInsideOut(rect));

	return !( x < rect.x || (x >= rect.x + rect.w) || y < rect.y || (y >= rect.y + rect.h) );
}

bool MDE_RectContains(const MDE_RECT& container, const MDE_RECT& containee)
{
	MDE_ASSERT(!MDE_RectIsInsideOut(container));
	MDE_ASSERT(!MDE_RectIsInsideOut(containee));

	return (containee.x >= container.x &&
	        containee.y >= container.y &&
	        containee.x + containee.w <= container.x + container.w &&
	        containee.y + containee.h <= container.y + container.h);
}


MDE_RECT MDE_RectClip(const MDE_RECT &this_rect, const MDE_RECT &clip_rect)
{
#ifdef MDE_PEDANTIC_ASSERTS
	MDE_ASSERT(!MDE_RectIsInsideOut(this_rect));
#endif
	MDE_ASSERT(!MDE_RectIsInsideOut(clip_rect));

	MDE_RECT tmp = { 0, 0, 0, 0 };
	if(!MDE_RectIntersects(this_rect, clip_rect))
		return tmp;
	int x1 = this_rect.x;
	int x2 = this_rect.x + this_rect.w;
	int y1 = this_rect.y;
	int y2 = this_rect.y + this_rect.h;
	tmp.x = MDE_MAX(x1, clip_rect.x);
	tmp.y = MDE_MAX(y1, clip_rect.y);
	tmp.w = MDE_MIN(x2, clip_rect.x + clip_rect.w) - tmp.x;
	tmp.h = MDE_MIN(y2, clip_rect.y + clip_rect.h) - tmp.y;
	return tmp;
}

MDE_RECT MDE_RectUnion(const MDE_RECT &this_rect, const MDE_RECT &and_this_rect)
{
	MDE_ASSERT(!MDE_RectIsInsideOut(this_rect));
	MDE_ASSERT(!MDE_RectIsInsideOut(and_this_rect));

	if (MDE_RectIsEmpty(this_rect))
		return and_this_rect;
	if (MDE_RectIsEmpty(and_this_rect))
		return this_rect;

	int minx = MDE_MIN(this_rect.x, and_this_rect.x);
	int miny = MDE_MIN(this_rect.y, and_this_rect.y);
	int maxx = this_rect.x + this_rect.w > and_this_rect.x + and_this_rect.w ?
				this_rect.x + this_rect.w : and_this_rect.x + and_this_rect.w;
	int maxy = this_rect.y + this_rect.h > and_this_rect.y + and_this_rect.h ?
				this_rect.y + this_rect.h : and_this_rect.y + and_this_rect.h;
	MDE_RECT tmp = { minx, miny, maxx - minx, maxy - miny };
	return tmp;
}

void MDE_RectReset(MDE_RECT &this_rect)
{
	this_rect.x = this_rect.y = this_rect.w = this_rect.h = 0;
}

bool MDE_RectRemoveOverlap(MDE_RECT& rect, const MDE_RECT& remove_rect)
{
	if (!MDE_RectIntersects(rect, remove_rect))
		return true;

	MDE_RECT new_rect = { 0, 0, 0, 0 };
	MDE_RECT remove = MDE_RectClip(remove_rect, rect);
	int piece_count = 0;

	// Top
	if (remove.y > rect.y)
	{
		new_rect = MDE_MakeRect(rect.x, rect.y, rect.w, remove.y - rect.y);
		piece_count++;
	}

	// Left
	if (remove.x > rect.x)
	{
		new_rect = MDE_MakeRect(rect.x, remove.y, remove.x - rect.x, remove.h);
		piece_count++;
	}

	// Right
	if (remove.x + remove.w < rect.x + rect.w)
	{
		new_rect = MDE_MakeRect(remove.x + remove.w, remove.y, rect.x + rect.w - (remove.x + remove.w), remove.h);
		piece_count++;
	}

	// Bottom
	if (remove.y + remove.h < rect.y + rect.h)
	{
		new_rect = MDE_MakeRect(rect.x, remove.y + remove.h, rect.w, rect.y + rect.h - (remove.y + remove.h));
		piece_count++;
	}

	// Only succeed if there was 1 new slice. Otherwise we have more than 2 rectangles totally.
	if (piece_count == 1)
	{
		rect = new_rect;
		return true;
	}
	return false;
}

// == MDE_BUFFER ============================================================

#if !defined(MDE_SUPPORT_HW_PAINTING) || defined(MDE_HW_SOFTWARE_FALLBACK)

#ifndef MDE_SUPPORT_HW_PAINTING
#define MDE_SOFTWARE_FUNCTION(func) func
#else
#define MDE_SOFTWARE_FUNCTION(func) func ## _soft
#endif

MDE_BUFFER* MDE_SOFTWARE_FUNCTION(MDE_CreateBuffer)(int w, int h, MDE_FORMAT format, int pal_len)
{
	MDE_BUFFER *buf = OP_NEW(MDE_BUFFER, ());
	if (!buf)
		return NULL;

	MDE_InitializeBuffer(w, h, MDE_GetBytesPerPixel(format) * w, format, NULL, NULL, buf);

	buf->data = OP_NEWA(char, buf->stride * h);

	if (pal_len)
		buf->pal = OP_NEWA(unsigned char, pal_len * 3);

	if (!buf->data || (pal_len && !buf->pal))
	{
		MDE_DeleteBuffer(buf);
		return NULL;
	}

	return buf;
}

void MDE_SOFTWARE_FUNCTION(MDE_DeleteBuffer)(MDE_BUFFER *&buf)
{
	if (buf)
	{
		char* data_ptr = (char*) buf->data;
		OP_DELETEA(data_ptr);
		OP_DELETEA(buf->pal);
		OP_DELETE(buf);
		buf = NULL;
	}
}

void* MDE_SOFTWARE_FUNCTION(MDE_LockBuffer)(MDE_BUFFER *buf, const MDE_RECT &rect, int &stride, bool readable)
{
	stride = buf->stride;
	return ((char*)buf->data) + stride*rect.y + MDE_GetBytesPerPixel(buf->format)*rect.x;
}

void MDE_SOFTWARE_FUNCTION(MDE_UnlockBuffer)(MDE_BUFFER *buf, bool changed)
{}

#endif // !MDE_SUPPORT_HW_PAINTING || MDE_HW_SOFTWARE_FALLBACK

void MDE_MakeSubsetBuffer(const MDE_RECT &rect, MDE_BUFFER *subset_buf, MDE_BUFFER *parent_buf)
{
	MDE_RECT outer_clip = MDE_RectClip(rect, parent_buf->outer_clip);
	outer_clip.x -= rect.x;
	outer_clip.y -= rect.y;

	*subset_buf = *parent_buf;
	if (parent_buf->data)
		subset_buf->data = ((char *)parent_buf->data) + parent_buf->stride * rect.y + rect.x * parent_buf->ps;
	subset_buf->outer_clip = outer_clip;
	subset_buf->w = rect.w;
	subset_buf->h = rect.h;
	subset_buf->ofs_x = parent_buf->ofs_x + rect.x;
	subset_buf->ofs_y = parent_buf->ofs_y + rect.y;
	subset_buf->user_data = parent_buf->user_data;
	MDE_SetClipRect(MDE_MakeRect(0, 0, rect.w, rect.h), subset_buf);
}

void MDE_OffsetBuffer(int dx, int dy, MDE_BUFFER *buf)
{
	if (buf->data)
		buf->data = ((char *)buf->data) + buf->stride * (-dy) - dx * buf->ps;
	buf->outer_clip.x += dx;
	buf->outer_clip.y += dy;
	buf->clip.x += dx;
	buf->clip.y += dy;
	buf->ofs_x += dx;
	buf->ofs_y += dy;
}

void MDE_InitializeBuffer(int w, int h, int stride, MDE_FORMAT format, void *data, unsigned char *pal, MDE_BUFFER *buf)
{
	buf->data = data;
	buf->pal = pal;
	buf->w = w;
	buf->h = h;
	buf->stride = stride;
	buf->ps = MDE_GetBytesPerPixel(format);
	buf->format = format;
	buf->clip = MDE_MakeRect(0, 0, w, h);
	buf->outer_clip = buf->clip;
	buf->ofs_x = 0;
	buf->ofs_y = 0;
	buf->user_data = NULL;
	buf->col = 0;
	buf->mask = 0;
	buf->method = MDE_METHOD_COPY;
#ifdef MDE_BILINEAR_BLITSTRETCH
	buf->stretch_method = MDE_DEFAULT_STRETCH_METHOD;
#endif
}

// == PAINTING ================================================================

#if !defined(MDE_SUPPORT_HW_PAINTING) || defined(MDE_HW_SOFTWARE_FALLBACK)

void MDE_SOFTWARE_FUNCTION(MDE_SetColor)(unsigned int col, MDE_BUFFER *dstbuf)
{
	dstbuf->col = col;
}

void MDE_SOFTWARE_FUNCTION(MDE_SetClipRect)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	dstbuf->clip = MDE_RectClip(rect, dstbuf->outer_clip);
}


#ifdef USE_PREMULTIPLIED_ALPHA
void SetColorMap(unsigned int* col_map, unsigned int color);
#endif // USE_PREMULTIPLIED_ALPHA

void MDE_SOFTWARE_FUNCTION(MDE_DrawBufferData)(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf)
{
	if (!MDE_RectIntersects(dst, dstbuf->clip))
		return;

        if (srcinf->method == MDE_METHOD_COLOR)
        {
            if (!MDE_COL_A(srcinf->col))
                return;
#ifdef USE_PREMULTIPLIED_ALPHA
            if (srcinf->col == MDE_RGBA(0,0,0,0xff))
                g_opera->libgogi_module.m_color_lookup = g_opera->libgogi_module.m_black_lookup;
            else
            {
                g_opera->libgogi_module.m_color_lookup = g_opera->libgogi_module.m_generic_color_lookup;
                if (g_opera->libgogi_module.m_color_lookup[256] != srcinf->col)
                {
                    SetColorMap(g_opera->libgogi_module.m_color_lookup, srcinf->col);
                    g_opera->libgogi_module.m_color_lookup[256] = srcinf->col;
                }
            }
#endif // USE_PREMULTIPLIED_ALPHA
        }

	MDE_RECT r = dst;
	MDE_RECT clip = dstbuf->clip;

	// Destclipping
	if(r.x < clip.x)
	{
		int diff = clip.x - r.x;
		srcx += diff;
		r.x += diff;
		r.w -= diff;
	}
	if(r.y < clip.y)
	{
		int diff = clip.y - r.y;
		srcy += diff;
		r.y += diff;
		r.h -= diff;
	}
	if(r.x + r.w > clip.x + clip.w)
	{
		int diff = (r.x + r.w) - (clip.x + clip.w);
		r.w -= diff;
	}
	if(r.y + r.h > clip.y + clip.h)
	{
		int diff = (r.y + r.h) - (clip.y + clip.h);
		r.h -= diff;
	}

	// Extreme sourceclipping
	if(srcx < - r.w)
		return;
	if(srcy < - r.h)
		return;
	if(srcx > srcdata->w)
		return;
	if(srcy > srcdata->h)
		return;

	// Sourceclipping
	if(srcx < 0)
	{
		int diff = - srcx;
		srcx += diff;
		r.x += diff;
		r.w -= diff;
	}
	if(srcy < 0)
	{
		int diff = - srcy;
		srcy += diff;
		r.y += diff;
		r.h -= diff;
	}
	if(srcx + r.w > srcdata->w)
	{
		int diff = (srcx + r.w) - srcdata->w;
		r.w -= diff;
	}
	if(srcy + r.h > srcdata->h)
	{
		int diff = (srcy + r.h) - srcdata->h;
		r.h -= diff;
	}

	// Check if there is something left
	if (MDE_RectIsEmpty(r))
		return;

	MDE_SCANLINE_BLITNORMAL scanline = MDE_GetScanline_BlitNormal(dstbuf, srcinf->format);
	bool MDE_Scanline_BlitNormal_unsupported(void *dst, void *src, int len, MDE_BUFFER_INFO *srcinf, MDE_BUFFER_INFO *dstinf);
	MDE_ASSERT(scanline != MDE_Scanline_BlitNormal_unsupported);

	int src_ps = MDE_GetBytesPerPixel(srcinf->format);

	for(int j = 0; j < r.h; j++)
	{
		void *dstrow = ((char*)dstbuf->data) + r.x * dstbuf->ps + (j + r.y) * dstbuf->stride;
		void *srcrow = ((char*)srcdata->data) + srcx * src_ps + (j + srcy) * src_stride;

		scanline(dstrow, srcrow, r.w, srcinf, dstbuf);
	}
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawBufferDataStretch)(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf)
{
	if (MDE_RectIsEmpty(dst))
		return;
	if (!MDE_RectIntersects(dst, dstbuf->clip))
		return;
	if (src.w <= 0 || src.h <= 0)
		return;

	MDE_RECT r = dst;
	MDE_RECT clip = dstbuf->clip;

	MDE_F1616 dx = ((MDE_F1616)src.w << 16) / dst.w;
	MDE_F1616 dy = ((MDE_F1616)src.h << 16) / dst.h;
	MDE_F1616 sx;
	MDE_F1616 sy;

	// Pre clipping of source start - since we can't allow sx/sy to be negative. We need 16.16 unsigned fixedpoint.
	if (src.x < 0)
	{
		int diff = ((MDE_F1616)(-src.x << 16) - 1) / dx + 1;
		// Assert we're adjusting by the precise amount needed.
		MDE_ASSERT((int)((diff - 1) * dx >> 16) < -src.x);
		MDE_ASSERT((int)(diff * dx >> 16) >= -src.x);
		sx = diff * dx - (-src.x << 16);
		r.x += diff;
		r.w -= diff;
		if (r.w <= 0) return;
	}
	else
	{
		sx = ((MDE_F1616)src.x << 16);
	}
	if (src.y < 0)
	{
		int diff = ((MDE_F1616)(-src.y << 16) - 1) / dy + 1;
		// Assert we're adjusting by the precise amount needed.
		MDE_ASSERT((int)((diff - 1) * dy >> 16) < -src.y);
		MDE_ASSERT((int)(diff * dy >> 16) >= -src.y);
		sy = diff * dy - (-src.y << 16);
		r.y += diff;
		r.h -= diff;
		if (r.h <= 0) return;
	}
	else
	{
		sy = ((MDE_F1616)src.y << 16);
	}

	// Destclipping
	if(r.x < clip.x)
	{
		int diff = clip.x - r.x;
		sx += diff * dx;
		r.x += diff;
		r.w -= diff;
		if (r.w <= 0) return;
	}
	if(r.y < clip.y)
	{
		int diff = clip.y - r.y;
		sy += diff * dy;
		r.y += diff;
		r.h -= diff;
		if (r.h <= 0) return;
	}
	if(r.x + r.w > clip.x + clip.w)
	{
		int diff = (r.x + r.w) - (clip.x + clip.w);
		r.w -= diff;
		if (r.w <= 0) return;
	}
	if(r.y + r.h > clip.y + clip.h)
	{
		int diff = (r.y + r.h) - (clip.y + clip.h);
		r.h -= diff;
		if (r.h <= 0) return;
	}

	// Sourceclipping
	if (((sx + (MDE_F1616)(r.w - 1) * dx) >> 16) >= (unsigned long)srcdata->w)
	{
		if ((sx >> 16) >= (unsigned long)srcdata->w)
			return; // completely outside
		int diff = ((sx + (MDE_F1616)(r.w - 1) * dx) - ((unsigned long)srcdata->w << 16)) / dx + 1;
		// Assert we're adjusting by the precise amount needed.
		MDE_ASSERT(((sx + (MDE_F1616)(r.w - diff - 1) * dx) >> 16) < (unsigned long)srcdata->w);
		MDE_ASSERT(((sx + (MDE_F1616)(r.w - diff) * dx) >> 16) >= (unsigned long)srcdata->w);
		r.w -= diff;
		if (r.w <= 0) return;
	}
	if (((sy + (MDE_F1616)(r.h - 1) * dy) >> 16) >= (unsigned long)srcdata->h)
	{
		if ((sy >> 16) >= (unsigned long)srcdata->h)
			return; // completely outside
		int diff = ((sy + (MDE_F1616)(r.h - 1) * dy) - ((unsigned long)srcdata->h << 16)) / dy + 1;
		// Assert we're adjusting by the precise amount needed.
		MDE_ASSERT(((sy + (MDE_F1616)(r.h - diff - 1) * dy) >> 16) < (unsigned long)srcdata->h);
		MDE_ASSERT(((sy + (MDE_F1616)(r.h - diff) * dy) >> 16) >= (unsigned long)srcdata->h);
		r.h -= diff;
		if (r.h <= 0) return;
	}

	MDE_ASSERT(((sx + (r.w-1) * dx) >> 16) < (unsigned long)srcdata->w);
	MDE_ASSERT(((sy + (r.h-1) * dy) >> 16) < (unsigned long)srcdata->h);

	// Check if there is something left

#ifdef MDE_BILINEAR_BLITSTRETCH
        // if this triggers a buffer not initialized with
        // MDE_InitializeBuffer doesn't set stretch_method - please
        // fix
        OP_ASSERT(srcinf->stretch_method == MDE_STRETCH_BILINEAR || srcinf->stretch_method == MDE_STRETCH_NEAREST);

	MDE_BILINEAR_INTERPOLATION_X interpolX = MDE_GetBilinearInterpolationX(srcinf->format);
	MDE_BILINEAR_INTERPOLATION_Y interpolY = MDE_GetBilinearInterpolationY(srcinf->format);
	if (interpolX && interpolY && srcinf->stretch_method == MDE_STRETCH_BILINEAR)
	{
		MDE_SCANLINE_BLITNORMAL scanline = MDE_GetScanline_BlitNormal(dstbuf, srcinf->format);
		int currentScanline[2] = {-1, -1};
		// allocate 3 scanlines
		int tmpstride = r.w*MDE_GetBytesPerPixel(srcinf->format);
		void* tmpbuf = op_malloc(3*tmpstride);
		void* scanlineBuf[2] = {tmpbuf, ((unsigned char*)tmpbuf)+tmpstride};
		if (tmpbuf)
		{
			// for all y values,
			for (int y = 0; y < r.h; ++y)
			{
				// calculate the two scanlines to use (or one), reuse the existing ones
				// merge the two scanlines
				// do a regular, non-stretched blit
				int sl = sy>>16;
				void *dstrow = ((char*)dstbuf->data) + r.x * dstbuf->ps + (y + r.y) * dstbuf->stride;
				void *srcrow = ((char*)srcdata->data) + sl * src_stride;
				if (currentScanline[0] != sl)
				{
					if (currentScanline[1] == sl)
					{
						// swap 0 and 1
						void* tmp = scanlineBuf[0];
						scanlineBuf[0] = scanlineBuf[1];
						scanlineBuf[1] = tmp;
						currentScanline[1] = -1;
					}
					else
						interpolX(scanlineBuf[0], srcrow, r.w, srcdata->w, sx, dx);
					currentScanline[0] = sl;

				}
				if (sy&0xffff && sl+1 < srcdata->h)
				{
					if (currentScanline[1] != sl+1)
					{
						interpolX(scanlineBuf[1], ((char*)srcrow) + src_stride, r.w, srcdata->w, sx, dx);
						currentScanline[1] = sl+1;
					}
					interpolY(((unsigned char*)tmpbuf)+2*tmpstride, scanlineBuf[0], scanlineBuf[1], r.w, sy&0xffff);
					scanline(dstrow, ((unsigned char*)tmpbuf)+2*tmpstride, r.w, srcinf, dstbuf);
				}
				else
					scanline(dstrow, scanlineBuf[0], r.w, srcinf, dstbuf);
				sy += dy;
			}
			op_free(tmpbuf);
			return;
		}
	}
#endif // MDE_BILINEAR_BLITSTRETCH

	MDE_SCANLINE_BLITSTRETCH scanline = MDE_GetScanline_BlitStretch(dstbuf, srcinf->format);
	for(int y = 0; y < r.h; y++)
	{
		void *dstrow = ((char*)dstbuf->data) + r.x * dstbuf->ps + (y + r.y) * dstbuf->stride;
		void *srcrow = ((char*)srcdata->data) + (sy >> 16) * src_stride;
		scanline(dstrow, srcrow, r.w, sx, dx, srcinf, dstbuf);
		sy += dy;
	}
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawBuffer)(MDE_BUFFER *srcbuf, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf)
{
	MDE_SOFTWARE_FUNCTION(MDE_DrawBufferData)(srcbuf, srcbuf, srcbuf->stride, dst, srcx, srcy, dstbuf);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawBufferStretch)(MDE_BUFFER *srcbuf, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf)
{
	MDE_SOFTWARE_FUNCTION(MDE_DrawBufferDataStretch)(srcbuf, srcbuf, srcbuf->stride, dst, src, dstbuf);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawRect)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	if (MDE_RectIsEmpty(rect))
		return;
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf);

	int x = MDE_MAX(dstbuf->clip.x, rect.x);
	int w = MDE_MIN(dstbuf->clip.x + dstbuf->clip.w, rect.x + rect.w) - x;
	if (MDE_RectContains(dstbuf->clip, dstbuf->clip.x, rect.y))
		scanline(((char*)dstbuf->data) + x * dstbuf->ps + (rect.y) * dstbuf->stride, w, dstbuf->col);
	if (MDE_RectContains(dstbuf->clip, dstbuf->clip.x, rect.y + rect.h - 1))
		scanline(((char*)dstbuf->data) + x * dstbuf->ps + (rect.y + rect.h - 1) * dstbuf->stride, w, dstbuf->col);
	int y1 = MDE_MAX(rect.y + 1, dstbuf->clip.y);
	int y2 = MDE_MIN(rect.y + rect.h - 1, dstbuf->clip.y + dstbuf->clip.h);
	for(int y = y1; y < y2; y++)
	{
		if (MDE_RectContains(dstbuf->clip, rect.x, y))
			scanline(((char*)dstbuf->data) + rect.x * dstbuf->ps + y * dstbuf->stride, 1, dstbuf->col);
		if (MDE_RectContains(dstbuf->clip, rect.x + rect.w - 1, y))
			scanline(((char*)dstbuf->data) + (rect.x + rect.w - 1) * dstbuf->ps + y * dstbuf->stride, 1, dstbuf->col);
	}
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawRectFill)(const MDE_RECT &rect, MDE_BUFFER *dstbuf, bool blend)
{
	MDE_RECT r = MDE_RectClip(rect, dstbuf->clip);
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf, blend);
	for(int y = 0; y < r.h; y++)
		scanline(((char*)dstbuf->data) + r.x * dstbuf->ps + (r.y + y) * dstbuf->stride, r.w, dstbuf->col);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawRectInvert)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	MDE_RECT r = MDE_RectClip(rect, dstbuf->clip);
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_InvertColor(dstbuf);
	for(int y = 0; y < r.h; y++)
		scanline(((char*)dstbuf->data) + r.x * dstbuf->ps + (r.y + y) * dstbuf->stride, r.w, 0);
}

void Symmetry(int x, int y, int a, int b, MDE_SCANLINE_SETCOLOR scanline, MDE_BUFFER *dstbuf, bool fill, int linewidth, const MDE_RECT &clip, int fix_x, int fix_y)
{
	if (fill)
	{
		int x1 = MDE_MAX(clip.x, x - a);
		int x2 = MDE_MIN(clip.x + clip.w, x + a - fix_x + 1);
		if (x1 <= x2)
		{
			int top = y - b;
			int bottom = y + b - fix_y;
			if (top >= clip.y && top < clip.y + clip.h)
				scanline(((char*)dstbuf->data) + x1 * dstbuf->ps + top * dstbuf->stride, x2-x1, dstbuf->col);
			if (top != bottom && bottom >= clip.y && bottom < clip.y + clip.h)
				scanline(((char*)dstbuf->data) + x1 * dstbuf->ps + bottom * dstbuf->stride, x2-x1, dstbuf->col);
		}
	}
	else
	{
		int px[4] = {	x - a,
						x - a,
						x + a - fix_x,
						x + a - fix_x };
		int py[4] = {	y - b,
						y + b - fix_y,
						y + b - fix_y,
						y - b };
		int on[4] = { 1, 1, 1, 1 };
		if (x - a == x + a - fix_x)
		{
			on[2] = 0;
			on[3] = 0;
		}
		else if (y - b == y + b - fix_y)
		{
			on[1] = 0;
			on[2] = 0;
		}
		if (linewidth > 1)
		{
			// FIX: Can be optimized
			// FIX: Always align even thickness inside or outside
			int half_linewidth = (linewidth >> 1);
			for(int i = 0; i < 4; i++)
				if (on[i])
					MDE_DrawRectFill(MDE_MakeRect(px[i] - half_linewidth, py[i] - half_linewidth, linewidth, linewidth), dstbuf);
			return;
		}
		for(int i = 0; i < 4; i++)
			if (on[i] && MDE_RectContains(clip, px[i], py[i]))
				scanline(((char*)dstbuf->data) + px[i] * dstbuf->ps + py[i] * dstbuf->stride, 1, dstbuf->col);
	}
}

void Bresenham_Ellipse(const MDE_RECT &rect, MDE_BUFFER *dstbuf, bool fill, int linewidth, MDE_SCANLINE_SETCOLOR scanline)
{
	if (rect.w <= 2 || rect.h <= 2)
	{
		// The ellipsedrawing code doesn't handle 2px width or height well (displays nothing)
		// Use a filled rect, since that will give the same result anyway.
		MDE_DrawRectFill(rect, dstbuf);
		return;
	}

	// Bresenham algorithm. http://xarch.tu-graz.ac.at/home/rurban/news/comp.graphics.algorithms/ellipse_bresenham.html
	// Modified to handle both odd/even widths/height and be filled.
	int a = rect.w >> 1, b = rect.h >> 1;
	int px = rect.x + a;
	int py = rect.y + b;
	int x, y, a2,b2, S, T, oldy;
	int fix_x = a << 1 == rect.w;
	int fix_y = b << 1 == rect.h;

	if (MDE_RectIsEmpty(rect))
		return;

	a2 = b * b;
	b2 = a * a;
	x = a;
	y = 0;
	oldy = y - 1;
	S = a2 * (1 - (a << 1))  +  (b2 << 1);
	T = b2 - (a2 << 1) * ((a << 1) - 1);
	b2 = b2 << 1;
	a2 = a2 << 1;
	do
	{
		if (S < 0)
		{
			S += b2 * ((y << 1) + 3);
			T += (b2 << 1) * (y + 1);
			y++;
		}
		else if (T < 0)
		{
			S += b2 * ((y << 1) + 3) - (a2 << 1) * (x - 1);
			T += (b2 << 1) * (y + 1) - a2 * ((x << 1) - 3);
			y++;
			x--;
		}
		else
		{
			S -= (a2 << 1) * (x - 1);
			T -= a2 * ((x << 1) - 3);
			x--;
		}
		if (!fill || y != oldy)
		{
			oldy = y;
			Symmetry(px, py, x, y, scanline, dstbuf, fill, linewidth, dstbuf->clip, fix_x, fix_y);
		}
	}
	while (x > 0);
	if (!fix_y)
		Symmetry(px, py, a, 0, scanline, dstbuf, fill, linewidth, dstbuf->clip, fix_x, fix_y);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawEllipse)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf);
	Bresenham_Ellipse(rect, dstbuf, false, 1, scanline);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawEllipseThick)(const MDE_RECT &rect, int linewidth, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf);
	Bresenham_Ellipse(rect, dstbuf, false, linewidth, scanline);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawEllipseInvert)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_InvertColor(dstbuf);
	Bresenham_Ellipse(rect, dstbuf, false, 1, scanline);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawEllipseFill)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf);
	Bresenham_Ellipse(rect, dstbuf, true, 1, scanline);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawEllipseInvertFill)(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_InvertColor(dstbuf);
	Bresenham_Ellipse(rect, dstbuf, true, 1, scanline);
}

void Plot(int x, int y, int linewidth, MDE_SCANLINE_SETCOLOR scanline, MDE_BUFFER *dstbuf)
{
	if (linewidth > 1)
	{
		// FIX: Can be optimized
		int half_linewidth = (linewidth >> 1);
		MDE_DrawRectFill(MDE_MakeRect(x - half_linewidth, y - half_linewidth, linewidth, linewidth), dstbuf);
	}
	else if (MDE_RectContains(dstbuf->clip, x, y))
		scanline(((char*)dstbuf->data) + x * dstbuf->ps + y * dstbuf->stride, 1, dstbuf->col);
}

void Bresenham_DrawLine(int x1, int y1, int x2, int y2, int linewidth, MDE_BUFFER *dstbuf, MDE_SCANLINE_SETCOLOR scanline)
{
	int minx = MDE_MIN(x1, x2);
	int miny = MDE_MIN(y1, y2);
	int maxx = MDE_MAX(x1, x2);
	int maxy = MDE_MAX(y1, y2);
	if (!MDE_RectIntersects(MDE_MakeRect(minx, miny, maxx - minx + 1, maxy - miny + 1), dstbuf->clip))
		return;

	// Bresenham algorithm
	// http://www.ezresult.com/article/Bresenham's_line_algorithm_C_code

	int i;
	int steep = 1;
	int sx, sy; // step positive or negative (1 or -1)
	int dx, dy; // delta (difference in X and Y between points)
	int e;
	int tmpswap;

	Plot(x2, y2, linewidth, scanline, dstbuf);

	dx = MDE_ABS(x2 - x1);
	sx = ((x2 - x1) > 0) ? 1 : -1;
	dy = MDE_ABS(y2 - y1);
	sy = ((y2 - y1) > 0) ? 1 : -1;
	if (dy > dx)
	{
		steep = 0;
		MDE_SWAP(tmpswap, x1, y1);
		MDE_SWAP(tmpswap, dx, dy);
		MDE_SWAP(tmpswap, sx, sy);
	}
	e = (dy << 1) - dx;
	for (i = 0; i < dx; i++)
	{
		if (steep)
			Plot(x1, y1, linewidth, scanline, dstbuf);
		else
			Plot(y1, x1, linewidth, scanline, dstbuf);
		while (e >= 0)
		{
			y1 += sy;
			e -= (dx << 1);
		}

		x1 += sx;
		e += (dy << 1);
	}
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawLine)(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf);
	Bresenham_DrawLine(x1, y1, x2, y2, 1, dstbuf, scanline);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawLineThick)(int x1, int y1, int x2, int y2, int linewidth, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_SetColor(dstbuf);
	Bresenham_DrawLine(x1, y1, x2, y2, linewidth, dstbuf, scanline);
}

void MDE_SOFTWARE_FUNCTION(MDE_DrawLineInvert)(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf)
{
	MDE_SCANLINE_SETCOLOR scanline = MDE_GetScanline_InvertColor(dstbuf);
	Bresenham_DrawLine(x1, y1, x2, y2, 1, dstbuf, scanline);
}

#ifdef MDE_INTERNAL_MEMCPY_SCROLL

void MDE_MemCpy32(unsigned int *dst, unsigned int *src, int len)
{
	switch(len & 7)
	{
		while(len > 0)
		{
		case 0: *dst++ = *src++;
		case 7: *dst++ = *src++;
		case 6: *dst++ = *src++;
		case 5: *dst++ = *src++;
		case 4: *dst++ = *src++;
		case 3: *dst++ = *src++;
		case 2: *dst++ = *src++;
		case 1: *dst++ = *src++;
			len -= 8;
		}
	}
}

void MDE_MemCpy16(unsigned short *dst, unsigned short *src, int len)
{
	switch(len & 7)
	{
		while(len > 0)
		{
		case 0: *dst++ = *src++;
		case 7: *dst++ = *src++;
		case 6: *dst++ = *src++;
		case 5: *dst++ = *src++;
		case 4: *dst++ = *src++;
		case 3: *dst++ = *src++;
		case 2: *dst++ = *src++;
		case 1: *dst++ = *src++;
			len -= 8;
		}
	}
}

#endif // MDE_INTERNAL_MEMCPY_SCROLL

void MDE_SOFTWARE_FUNCTION(MDE_MoveRect)(const MDE_RECT &rect, int dx, int dy, MDE_BUFFER *dstbuf)
{
	MDE_RECT r = rect;
    
    r.x -= dx;
    r.y -= dy;
    r = MDE_RectClip(r, dstbuf->outer_clip);
    r.x += dx;
    r.y += dy;
    r = MDE_RectClip(r, dstbuf->outer_clip);

	if (r.w <= 0 || r.h <= 0)
		return;

	char* dstrow = ((char*)dstbuf->data) + r.x * dstbuf->ps + r.y * dstbuf->stride;
	char* srcrow = dstrow - dx * dstbuf->ps - dy * dstbuf->stride;

#ifdef MDE_USE_MEMCPY_SCROLL
	if (dx == 0)
	{
#ifdef MDE_INTERNAL_MEMCPY_SCROLL
		if (dstbuf->ps == 4)
		{
			if (dy > 0)
			{
				for(int y = r.h - 1; y >= 0; y--)
					MDE_MemCpy32((unsigned int *)(dstrow + y * dstbuf->stride), (unsigned int *)(srcrow + y * dstbuf->stride), r.w);
			}
			else
			{
				for(int y = 0; y < r.h; y++)
					MDE_MemCpy32((unsigned int *)(dstrow + y * dstbuf->stride), (unsigned int *)(srcrow + y * dstbuf->stride), r.w);
			}
		}
		else if (dstbuf->ps == 2)
		{
			if (dy > 0)
			{
				for(int y = r.h - 1; y >= 0; y--)
					MDE_MemCpy16((unsigned short *)(dstrow + y * dstbuf->stride), (unsigned short *)(srcrow + y * dstbuf->stride), r.w);
			}
			else
			{
				for(int y = 0; y < r.h; y++)
					MDE_MemCpy16((unsigned short *)(dstrow + y * dstbuf->stride), (unsigned short *)(srcrow + y * dstbuf->stride), r.w);
			}
		}
		else
#endif // MDE_INTERNAL_MEMCPY_SCROLL
		{
			if (dy > 0)
			{
				for(int y = r.h - 1; y >= 0; y--)
					op_memcpy(dstrow + y * dstbuf->stride, srcrow + y * dstbuf->stride, r.w * dstbuf->ps);
			}
			else
			{
				for(int y = 0; y < r.h; y++)
					op_memcpy(dstrow + y * dstbuf->stride, srcrow + y * dstbuf->stride, r.w * dstbuf->ps);
			}
		}
	}
	else
#endif // MDE_USE_MEMCPY_SCROLL
	{
		if ((r.w + MDE_ABS(dx)) * dstbuf->ps == dstbuf->stride)
		{
			const size_t n = r.h * dstbuf->stride - MDE_ABS(dx) * dstbuf->ps;
			MDE_ASSERT(dstrow >= (char*)dstbuf->data && dstrow + n <= (char*)dstbuf->data + dstbuf->stride * dstbuf->h);
			MDE_ASSERT(srcrow >= (char*)dstbuf->data && srcrow + n <= (char*)dstbuf->data + dstbuf->stride * dstbuf->h);
			// It should be faster to do only one call than several.
			MDE_GfxMove(dstrow, srcrow, n);
		}
		else
		{
			if (dy > 0)
			{
				for(int y = r.h - 1; y >= 0; y--)
					MDE_GfxMove(dstrow + y * dstbuf->stride, srcrow + y * dstbuf->stride, r.w * dstbuf->ps);
			}
			else
			{
				for(int y = 0; y < r.h; y++)
					MDE_GfxMove(dstrow + y * dstbuf->stride, srcrow + y * dstbuf->stride, r.w * dstbuf->ps);
			}
		}
	}
}

#endif // !MDE_SUPPORT_HW_PAINTING
