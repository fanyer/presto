/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(VEGA_SUPPORT) && defined(VEGA_OPPAINTER_SUPPORT) && defined(MDE_SUPPORT_HW_PAINTING)

#include "modules/libgogi/mde.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"

MDE_BUFFER* MDE_CreateBuffer(int w, int h, MDE_FORMAT format, int pal_len)
{
	return NULL;
}
void MDE_DeleteBuffer(MDE_BUFFER *&buf)
{}
void* MDE_LockBuffer(MDE_BUFFER *buf, const MDE_RECT &rect, int &stride, bool readable)
{
	return NULL;
}
void MDE_UnlockBuffer(MDE_BUFFER *buf, bool changed)
{}
void MDE_SetColor(unsigned int col, MDE_BUFFER *dstbuf)
{}
void MDE_SetClipRect(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{
	dstbuf->clip = MDE_RectClip(rect, dstbuf->outer_clip);
}
void MDE_DrawBufferData(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf)
{}
void MDE_DrawBufferDataStretch(MDE_BUFFER_DATA *srcdata, MDE_BUFFER_INFO *srcinf, int src_stride, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf)
{}
void MDE_DrawBuffer(MDE_BUFFER *srcbuf, const MDE_RECT &dst, int srcx, int srcy, MDE_BUFFER *dstbuf)
{}
void MDE_DrawBufferStretch(MDE_BUFFER *srcbuf, const MDE_RECT &dst, const MDE_RECT &src, MDE_BUFFER *dstbuf)
{}
void MDE_DrawRect(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{}
void MDE_DrawRectFill(const MDE_RECT &rect, MDE_BUFFER *dstbuf, bool blend)
{}
void MDE_DrawRectInvert(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{}
void MDE_DrawEllipse(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{}
void MDE_DrawEllipseThick(const MDE_RECT &rect, int linewidth, MDE_BUFFER *dstbuf)
{}
void MDE_DrawEllipseInvert(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{}
void MDE_DrawEllipseFill(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{}
void MDE_DrawEllipseInvertFill(const MDE_RECT &rect, MDE_BUFFER *dstbuf)
{}
void MDE_DrawLine(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf)
{}
void MDE_DrawLineThick(int x1, int y1, int x2, int y2, int linewidth, MDE_BUFFER *dstbuf)
{}
void MDE_DrawLineInvert(int x1, int y1, int x2, int y2, MDE_BUFFER *dstbuf)
{}

void MDE_MoveRect(const MDE_RECT &rect, int dx, int dy, MDE_BUFFER *dstbuf)
{
	MDE_RECT r = rect;

	int left = dstbuf->outer_clip.x;
	int top = dstbuf->outer_clip.y;
	int right = dstbuf->outer_clip.x + dstbuf->outer_clip.w;
	int bottom = dstbuf->outer_clip.y + dstbuf->outer_clip.h;

	// Make it the source rectangle
	r.x -= dx;
	r.y -= dy;

	if (r.x < left)
	{
		int diff = left - r.x;
		r.x += diff;
		r.w -= diff;
	}

	if (r.y < top)
	{
		int diff = top - r.y;
		r.y += diff;
		r.h -= diff;
	}

	if (r.x + r.w > right)
		r.w = right - r.x;
	if (r.y + r.h > bottom)
		r.h = bottom - r.y;

	if (r.w <= 0 || r.h <= 0)
		return;

	VEGAOpPainter* painter = (VEGAOpPainter*)(dstbuf->user_data);
	painter->MoveRect(r.x, r.y, r.w, r.h, dx, dy);
}
bool MDE_UseMoveRect()
{
	return true;
}

#endif // VEGA_SUPPORT && VEGA_OPPAINTER_SUPPORT && MDE_SUPPORT_HW_PAINTING

