/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/vis_dev.h"
#include "modules/layout/layout.h"
#include "modules/logdoc/htm_lex.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegafill.h"
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/src/vegafiltergaussian.h"
#endif

//Enable to debug some paths graphically
//#define DEBUG_VARIABLEPATH

#ifdef VEGA_OPPAINTER_SUPPORT

// == RadiusPathCalculator ================================================================================

void RadiusPathCalculator::InsetBorderRadius(Border *border, short b_ofs_left, short b_ofs_top, short b_ofs_right, short b_ofs_bottom)
{
	if (border->top.radius_start > 0 || b_ofs_left > 0)
		border->top.radius_start = MAX(0, border->top.radius_start - b_ofs_left);
	if (border->bottom.radius_start > 0 || b_ofs_left > 0)
		border->bottom.radius_start = MAX(0, border->bottom.radius_start - b_ofs_left);
	if (border->top.radius_end > 0 || b_ofs_right > 0)
		border->top.radius_end = MAX(0, border->top.radius_end - b_ofs_right);
	if (border->bottom.radius_end > 0 || b_ofs_right > 0)
		border->bottom.radius_end = MAX(0, border->bottom.radius_end - b_ofs_right);
	if (border->left.radius_start > 0 || b_ofs_top > 0)
		border->left.radius_start = MAX(0, border->left.radius_start - b_ofs_top);
	if (border->right.radius_start > 0 || b_ofs_top > 0)
		border->right.radius_start = MAX(0, border->right.radius_start - b_ofs_top);
	if (border->left.radius_end > 0 || b_ofs_bottom > 0)
		border->left.radius_end = MAX(0, border->left.radius_end - b_ofs_bottom);
	if (border->right.radius_end > 0 || b_ofs_bottom > 0)
		border->right.radius_end = MAX(0, border->right.radius_end - b_ofs_bottom);
}

RadiusPathCalculator::RadiusPathCalculator(int box_width, int box_height, const Border *b, VEGA_FIX scale)
{
	this->scale = scale;

	if (box_width <= 0 || box_height <= 0)
	{
		left_radius_start = 0;
		left_radius_stop = 0;
		top_radius_start = 0;
		top_radius_stop = 0;
		right_radius_start = 0;
		right_radius_stop = 0;
		bottom_radius_start = 0;
		bottom_radius_stop = 0;
		return;
	}

	left_radius_start = b->left.radius_start * scale;
	left_radius_stop = b->left.radius_end * scale;
	top_radius_start = b->top.radius_start * scale;
	top_radius_stop = b->top.radius_end * scale;
	right_radius_start = b->right.radius_start * scale;
	right_radius_stop = b->right.radius_end * scale;
	bottom_radius_start = b->bottom.radius_start * scale;
	bottom_radius_stop = b->bottom.radius_end * scale;

	/* "For each side of the box, compare the length of the side with the
	combined length of the two radiuses on that side.  If the combined
	radiuses are larger than the side length, divide the side length by
	the combined radius length.  Once this has been done for all four
	sides, take the smallest such value and multiply all the radiuses by
	this factor." */
#if 1
	VEGA_FIX factor = VEGA_INTTOFIX(1);
	if (VEGA_FIXTOINT(left_radius_start + left_radius_stop) > box_height)
	{
		VEGA_FIX f = VEGA_INTTOFIX(box_height) / VEGA_FIXTOINT(left_radius_start + left_radius_stop);
		factor = MIN(f, factor);
	}
	if (VEGA_FIXTOINT(right_radius_start + right_radius_stop) > box_height)
	{
		VEGA_FIX f = VEGA_INTTOFIX(box_height) / VEGA_FIXTOINT(right_radius_start + right_radius_stop);
		factor = MIN(f, factor);
	}
	if (VEGA_FIXTOINT(top_radius_start + top_radius_stop) > box_width)
	{
		VEGA_FIX f = VEGA_INTTOFIX(box_width) / VEGA_FIXTOINT(top_radius_start + top_radius_stop);
		factor = MIN(f, factor);
	}
	if (VEGA_FIXTOINT(bottom_radius_start + bottom_radius_stop) > box_width)
	{
		VEGA_FIX f = VEGA_INTTOFIX(box_width) / VEGA_FIXTOINT(bottom_radius_start + bottom_radius_stop);
		factor = MIN(f, factor);
	}
	if (factor < VEGA_INTTOFIX(1))
	{
		left_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(left_radius_start, factor));
		left_radius_stop = VEGA_TRUNCFIX(VEGA_FIXMUL(left_radius_stop, factor));
		top_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(top_radius_start, factor));
		top_radius_stop = VEGA_TRUNCFIX(VEGA_FIXMUL(top_radius_stop, factor));
		right_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(right_radius_start, factor));
		right_radius_stop = VEGA_TRUNCFIX(VEGA_FIXMUL(right_radius_stop, factor));
		bottom_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(bottom_radius_start, factor));
		bottom_radius_stop = VEGA_TRUNCFIX(VEGA_FIXMUL(bottom_radius_stop, factor));
	}
#else
	if (VEGA_FIXTOINT(left_radius_start + left_radius_stop) > box_height)
	{
		VEGA_FIX factor = VEGA_INTTOFIX(box_height) / VEGA_FIXTOINT(left_radius_start + left_radius_stop);
		left_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(left_radius_start, factor));
		left_radius_stop = VEGA_INTTOFIX(box_height) - left_radius_start;
	}
	if (VEGA_FIXTOINT(right_radius_start + right_radius_stop) > box_height)
	{
		VEGA_FIX factor = VEGA_INTTOFIX(box_height) / VEGA_FIXTOINT(right_radius_start + right_radius_stop);
		right_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(right_radius_start, factor));
		right_radius_stop = VEGA_INTTOFIX(box_height) - right_radius_start;
	}
	if (VEGA_FIXTOINT(top_radius_start + top_radius_stop) > box_width)
	{
		VEGA_FIX factor = VEGA_INTTOFIX(box_width) / VEGA_FIXTOINT(top_radius_start + top_radius_stop);
		top_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(top_radius_start, factor));
		top_radius_stop = VEGA_INTTOFIX(box_width) - top_radius_start;
	}
	if (VEGA_FIXTOINT(bottom_radius_start + bottom_radius_stop) > box_width)
	{
		VEGA_FIX factor = VEGA_INTTOFIX(box_width) / VEGA_FIXTOINT(bottom_radius_start + bottom_radius_stop);
		bottom_radius_start = VEGA_TRUNCFIX(VEGA_FIXMUL(bottom_radius_start, factor));
		bottom_radius_stop = VEGA_INTTOFIX(box_width) - bottom_radius_start;
	}
#endif
	OP_ASSERT(left_radius_start + left_radius_stop <= VEGA_INTTOFIX(box_height));
	OP_ASSERT(right_radius_start + right_radius_stop <= VEGA_INTTOFIX(box_height));
	OP_ASSERT(top_radius_start + top_radius_stop <= VEGA_INTTOFIX(box_width));
	OP_ASSERT(bottom_radius_start + bottom_radius_stop <= VEGA_INTTOFIX(box_width));
}

void RadiusPathCalculator::MakeBorderPath(VEGAPath *p, const Border *border, UINT16 border_id, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height, VEGA_FIX border_width_extra)
{
	VEGA_FIX flatness = VEGA_INTTOFIX(1)/10;
	VEGA_FIX xrot = VEGA_INTTOFIX(90);
	const VEGA_FIX half_left = IS_BORDER_ON(border->left) ? (border->left.width * scale)/2 : 0;
	const VEGA_FIX half_top = IS_BORDER_ON(border->top) ? (border->top.width * scale)/2 : 0;
	const VEGA_FIX half_right = IS_BORDER_ON(border->right) ? (border->right.width * scale)/2 : 0;
	const VEGA_FIX half_bottom = IS_BORDER_ON(border->bottom) ? (border->bottom.width * scale)/2 : 0;
	VEGA_FIX border_width = 1;
	short border_style = 0;
	VEGAPath bp;

	if (border_id == 0x1111) // All
	{
		RETURN_VOID_IF_ERROR(bp.moveTo(x + half_left, y + left_radius_start));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + top_radius_start, y + half_top, left_radius_start - half_left, top_radius_start - half_top, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x + width - top_radius_stop, y + half_top));
		RETURN_VOID_IF_ERROR(bp.arcTo(x - half_right + width, y + right_radius_start, right_radius_start - half_right, top_radius_stop - half_top, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x - half_right + width, y + height - right_radius_stop));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + width - bottom_radius_stop, y - half_bottom + height, right_radius_stop - half_right, bottom_radius_stop - half_bottom, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x + bottom_radius_start, y - half_bottom + height));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + half_left, y + height - left_radius_stop, left_radius_stop - half_left, bottom_radius_start - half_bottom, xrot, false, true, flatness));

		border_style = border->left.style;
		border_width = border->left.width * scale;
	}
	else if (border_id == 0x0001) // left
	{
		OP_ASSERT(IS_BORDER_ON(border->left));

		RETURN_VOID_IF_ERROR(bp.moveTo(x + bottom_radius_start, y - half_bottom + height));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + half_left, y + height - left_radius_stop, left_radius_stop - half_bottom, bottom_radius_start - half_left, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x + half_left, y + left_radius_start));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + top_radius_start, y + half_top, left_radius_start - half_top, top_radius_start - half_left, xrot, false, true, flatness));

		border_style = border->left.style;
		border_width = border->left.width;
		if (border_style == CSS_VALUE_solid)
		{
			border_width = MAX(border_width, border->top.width);
			border_width = MAX(border_width, border->bottom.width);
		}
		border_width *= scale;
	}
	else if (border_id == 0x0010) // top
	{
		OP_ASSERT(IS_BORDER_ON(border->top));

		RETURN_VOID_IF_ERROR(bp.moveTo(x + half_left, y + left_radius_start));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + top_radius_start, y + half_top, left_radius_start - half_top, top_radius_start - half_left, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x + width - top_radius_stop, y + half_top));
		RETURN_VOID_IF_ERROR(bp.arcTo(x - half_right + width, y + right_radius_start, right_radius_start - half_top, top_radius_stop - half_right, xrot, false, true, flatness));

		border_style = border->top.style;
		border_width = border->top.width;
		if (border_style == CSS_VALUE_solid)
		{
			border_width = MAX(border_width, border->left.width);
			border_width = MAX(border_width, border->right.width);
		}
		border_width *= scale;
	}
	else if (border_id == 0x0100) // right
	{
		OP_ASSERT(IS_BORDER_ON(border->right));

		RETURN_VOID_IF_ERROR(bp.moveTo(x + width - top_radius_stop, y + half_top));
		RETURN_VOID_IF_ERROR(bp.arcTo(x - half_right + width, y + right_radius_start, right_radius_start - half_top, top_radius_stop - half_right, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x - half_right + width, y + height - right_radius_stop));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + width - bottom_radius_stop, y - half_bottom + height, right_radius_stop - half_bottom, bottom_radius_stop - half_right, xrot, false, true, flatness));

		border_style = border->right.style;
		border_width = border->right.width;
		if (border_style == CSS_VALUE_solid)
		{
			border_width = MAX(border_width, border->top.width);
			border_width = MAX(border_width, border->bottom.width);
		}
		border_width *= scale;
	}
	else if (border_id == 0x1000) // bottom
	{
		OP_ASSERT(IS_BORDER_ON(border->bottom));

		RETURN_VOID_IF_ERROR(bp.moveTo(x - half_right + width, y + height - right_radius_stop));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + width - bottom_radius_stop, y - half_bottom + height, right_radius_stop - half_bottom, bottom_radius_stop - half_right, xrot, false, true, flatness));

		RETURN_VOID_IF_ERROR(bp.lineTo(x + bottom_radius_start, y - half_bottom + height));
		RETURN_VOID_IF_ERROR(bp.arcTo(x + half_left, y + height - left_radius_stop, left_radius_stop - half_bottom, bottom_radius_start - half_left, xrot, false, true, flatness));

		border_style = border->bottom.style;
		border_width = border->bottom.width;
		if (border_style == CSS_VALUE_solid)
		{
			border_width = MAX(border_width, border->left.width);
			border_width = MAX(border_width, border->right.width);
		}
		border_width *= scale;
	}
	else
	{
		OP_ASSERT(!"Should not happen!");
	}
	RETURN_VOID_IF_ERROR(bp.close(border_id == 0x1111));

	bp.setLineWidth(border_width + border_width_extra);

	if (border_style == CSS_VALUE_solid ||
		border_style == CSS_VALUE_outset ||
		border_style == CSS_VALUE_inset ||

		// The following are temporary. We should draw them correctly!
		border_style == CSS_VALUE_double ||
		border_style == CSS_VALUE_groove ||
		border_style == CSS_VALUE_ridge)
		RETURN_VOID_IF_ERROR(bp.createOutline(p, VEGA_INTTOFIX(1))); // FIX: Use flatness instead of 1?
	else
	{
		VEGAPath dpath;
		VEGA_FIX dash_size_min = VEGA_INTTOFIX(1)/5;
		VEGA_FIX dash_size = MAX(border_width, dash_size_min);

		if (border_style == CSS_VALUE_dotted)
		{
			bp.setLineCap(VEGA_LINECAP_ROUND);
			VEGA_FIX dasharray[2] = {VEGA_INTTOFIX(1)/1000, dash_size * 2};
			RETURN_VOID_IF_ERROR(bp.createDash(&dpath, VEGA_INTTOFIX(0), 2, dasharray));
		}
		else // if (corder_style == CSS_VALUE_dashed) // FIX: other penstyles!
		{
			VEGA_FIX dasharray[2] = {dash_size * 3, dash_size * 2};
			RETURN_VOID_IF_ERROR(bp.createDash(&dpath, VEGA_INTTOFIX(0), 2, dasharray));
		}
		
		RETURN_VOID_IF_ERROR(dpath.createOutline(p, flatness));
	}
}

void RadiusPathCalculator::MakeBackgroundPath(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height)
{
	VEGA_FIX flatness = VEGA_INTTOFIX(1)/10;
	VEGA_FIX xrot = VEGA_INTTOFIX(90);

	RETURN_VOID_IF_ERROR(p->moveTo(x, y + left_radius_start));
	RETURN_VOID_IF_ERROR(p->arcTo(x + top_radius_start, y, left_radius_start, top_radius_start, xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width - top_radius_stop, y));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width, y + right_radius_start, right_radius_start, top_radius_stop, xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width, y + height - right_radius_stop));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width - bottom_radius_stop, y + height, right_radius_stop, bottom_radius_stop, xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + bottom_radius_start, y + height));
	RETURN_VOID_IF_ERROR(p->arcTo(x, y + height - left_radius_stop, left_radius_stop, bottom_radius_start, xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->close(true));
}

void RadiusPathCalculator::MakeInvertedPath(VEGAPath *p, const Border *border,
											VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height)
{
	VEGA_FIX flatness = VEGA_INTTOFIX(1)/10;
	VEGA_FIX xrot = VEGA_INTTOFIX(90);

	RETURN_VOID_IF_ERROR(p->moveTo(x + top_radius_start, y));
	RETURN_VOID_IF_ERROR(p->arcTo(x, y + left_radius_start, left_radius_start, top_radius_start, xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x, y + height - left_radius_stop));
	RETURN_VOID_IF_ERROR(p->arcTo(x + bottom_radius_start, y + height, left_radius_stop, bottom_radius_start, xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width - bottom_radius_stop, y + height));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width, y + height - right_radius_stop, right_radius_stop, bottom_radius_stop, xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width, y + right_radius_start));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width - top_radius_stop, y, right_radius_start, top_radius_stop, xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->close(true));
}

void RadiusPathCalculator::MakeInvertedPath(VEGAPath *p, const Border *border,
											VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height,
											VEGA_FIX bounding_x, VEGA_FIX bounding_y, VEGA_FIX bounding_width, VEGA_FIX bounding_height)
{
	MakeInvertedPath(p, border, x, y, width, height);

	RETURN_VOID_IF_ERROR(p->moveTo(bounding_x, bounding_y));
	RETURN_VOID_IF_ERROR(p->lineTo(bounding_x + bounding_width, bounding_y));
	RETURN_VOID_IF_ERROR(p->lineTo(bounding_x + bounding_width, bounding_y + bounding_height));
	RETURN_VOID_IF_ERROR(p->lineTo(bounding_x, bounding_y + bounding_height));
	RETURN_VOID_IF_ERROR(p->close(true));
}

void RadiusPathCalculator::MakeInvertedPathInsideBorder(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height)
{
	const VEGA_FIX left_width = IS_BORDER_ON(border->left) ? (border->left.width * scale) : 0;
	const VEGA_FIX top_width = IS_BORDER_ON(border->top) ? (border->top.width * scale) : 0;
	const VEGA_FIX right_width = IS_BORDER_ON(border->right) ? (border->right.width * scale) : 0;
	const VEGA_FIX bottom_width = IS_BORDER_ON(border->bottom) ? (border->bottom.width * scale) : 0;
	VEGA_FIX flatness = VEGA_INTTOFIX(1)/10;
	VEGA_FIX xrot = VEGA_INTTOFIX(90);

	RETURN_VOID_IF_ERROR(p->moveTo(x + MAX(left_width, top_radius_start), y + top_width));
	RETURN_VOID_IF_ERROR(p->arcTo(x + left_width, y + MAX(top_width, left_radius_start),
								  MAX(0, left_radius_start - top_width), MAX(0, top_radius_start - left_width),
								  xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + left_width, y + height - MAX(bottom_width, left_radius_stop)));
	RETURN_VOID_IF_ERROR(p->arcTo(x + MAX(left_width, bottom_radius_start), y + height - bottom_width,
								  MAX(0, left_radius_stop - bottom_width), MAX(0, bottom_radius_start - left_width),
								  xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width - MAX(right_width, bottom_radius_stop), y + height - bottom_width));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width - right_width, y + height - MAX(bottom_width, right_radius_stop),
								  MAX(0, right_radius_stop - bottom_width), MAX(0, bottom_radius_stop - right_width),
								  xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width - right_width, y + MAX(top_width, right_radius_start)));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width - MAX(left_width, top_radius_stop), y + top_width,
								  MAX(0, right_radius_start - top_width), MAX(0, top_radius_stop - right_width),
								  xrot, false, false, flatness));
	RETURN_VOID_IF_ERROR(p->close(true));
}

void RadiusPathCalculator::MakeBackgroundPathInsideBorder(VEGAPath *p, const Border *border, VEGA_FIX x, VEGA_FIX y, VEGA_FIX width, VEGA_FIX height)
{
	const VEGA_FIX left_width = IS_BORDER_ON(border->left) ? (border->left.width * scale) : 0;
	const VEGA_FIX top_width = IS_BORDER_ON(border->top) ? (border->top.width * scale) : 0;
	const VEGA_FIX right_width = IS_BORDER_ON(border->right) ? (border->right.width * scale) : 0;
	const VEGA_FIX bottom_width = IS_BORDER_ON(border->bottom) ? (border->bottom.width * scale) : 0;
	VEGA_FIX flatness = VEGA_INTTOFIX(1)/10;
	VEGA_FIX xrot = VEGA_INTTOFIX(90);

	RETURN_VOID_IF_ERROR(p->moveTo(x + left_width, y + MAX(top_width, left_radius_start)));
	RETURN_VOID_IF_ERROR(p->arcTo(x + MAX(left_width, top_radius_start), y + top_width,
								  MAX(0, left_radius_start - top_width), MAX(0, top_radius_start - left_width),
								  xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width - MAX(top_radius_stop, right_width), y + top_width));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width - right_width, y + MAX(top_width, right_radius_start),
								  MAX(0, right_radius_start - top_width), MAX(0, top_radius_stop - right_width),
								  xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + width - right_width, y + height - MAX(right_radius_stop, bottom_width)));
	RETURN_VOID_IF_ERROR(p->arcTo(x + width - MAX(bottom_radius_stop, right_width), y + height - bottom_width,
								  MAX(0, right_radius_stop - bottom_width), MAX(0, bottom_radius_stop - right_width),
								  xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->lineTo(x + MAX(left_width, bottom_radius_start), y + height - bottom_width));
	RETURN_VOID_IF_ERROR(p->arcTo(x + left_width, y + height - MAX(left_radius_stop, bottom_width),
								  MAX(0, left_radius_stop - bottom_width), MAX(0, bottom_radius_start - left_width),
								  xrot, false, true, flatness));
	RETURN_VOID_IF_ERROR(p->close(true));
}

// == VisualDevice drawing functions ====================================================================

#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS VisualDevice::BeginRadiusClipping(const OpRect &border_box, const OpRect& clip_rect, const Border* border)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	// Do not call this method if the border doesn't have any radius.
	OP_ASSERT(HasBorderRadius(border));

	// Compensate the radius with the inset the clip_rect vs border_box creates, so the radius align as expected.
	short b_ofs_left = clip_rect.x - border_box.x;
	short b_ofs_top = clip_rect.y - border_box.y;
	short b_ofs_right = (border_box.x + border_box.width) - (clip_rect.x + clip_rect.width);
	short b_ofs_bottom = (border_box.y + border_box.height) - (clip_rect.y + clip_rect.height);
	Border adjusted_border = *border;
	RadiusPathCalculator::InsetBorderRadius(&adjusted_border, b_ofs_left, b_ofs_top, b_ofs_right, b_ofs_bottom);

	if (HasBorderRadius(&adjusted_border))
	{
		RETURN_IF_ERROR(BeginStencil(clip_rect));

		SetBgColor(OP_RGB(0, 0, 0));
		BeginModifyingStencil();
		DrawBgColorWithRadius(clip_rect, &adjusted_border);
		EndModifyingStencil();

		return OpStatus::OK;
	}
#endif
	return OpStatus::ERR;
}

void VisualDevice::EndRadiusClipping()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	EndStencil();
#endif
}

#ifdef VEGA_OPPAINTER_SUPPORT
BoxShadowCorner::BoxShadowCorner()
	: bitmap(NULL)
	, blur_radius(0)
	, inset(FALSE)
	, color(USE_DEFAULT_COLOR)
	, scale(0)
{
	border.Reset();
}

BoxShadowCorner::~BoxShadowCorner()
{
	OP_DELETE(bitmap);
}

// Returns a value greater than the maximum sampling width of the gaussian filter.
// This value should be used when calculating how big corner tile we need to
// not lose any information, i.e. get it to look like we did a filter application
// on the full rectangle.
static int CalcPaddingNeeded(int blur_radius)
{
	return (3*blur_radius + 1)/2;
}

// Draws the shadow of the top left corner into the corner bitmap.
OP_STATUS VisualDevice::DrawCorner(BoxShadowCorner *corner)
{
	OP_ASSERT(!corner->bitmap);

	int blur_radius = corner->blur_radius;
	int border_radius = corner->border.left.radius_start;
	int scaled_blur_radius = ScaleToScreen(blur_radius);
	int scaled_border_radius = ScaleToScreen(border_radius);

	// corner_side is calculated to be exactly that much space we need to draw
	// the shadowed corner. The shadow expands 'padding' pixels inside and
	// outside the border, and then we need to add border_radius as the shadow
	// extends blur_radius pixels right/down from the edge of the border radius.
	int padding = CalcPaddingNeeded(blur_radius);
	int scaled_padding = ScaleToScreen(padding);
	int corner_side = 2*scaled_padding + scaled_border_radius;

	// TODO: make blur filter support clamping to reduce the size of src.
	int src_side = corner_side + scaled_padding;
	if (corner->inset)
		src_side += scaled_padding;

	OpBitmap *src;
	RETURN_IF_ERROR(OpBitmap::Create(&src, src_side, src_side, FALSE, TRUE, 0, 0, TRUE));
	src->SetColor(NULL, TRUE, NULL);

	// Save the old painter settings and set the src bitmap as the current painter.
	OpPainter *old_painter = painter;
	painter = src->GetPainter();
	if (!painter)
	{
		OP_DELETE(src);
		painter = old_painter;
		return OpStatus::ERR;
	}
	VDStateNoTranslationNoOffset old_state = BeginNoTranslationNoOffsetPainting();

	// Draw the shape of the shadow rectangle to the src bitmap.
	int offset = padding;
	if (corner->inset)
		offset += padding;

	OpRect shadow_rect = OpRect(offset, offset, corner->shadow_rect.width, corner->shadow_rect.height);

	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	VEGAOpPainter::FillState vp_fstate = vp->SaveFillState();
	vp->SetColor(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color), OP_GET_A_VALUE(color));
	if (corner->inset)
	{
		VEGAPath p_inv;
		MakeInvertedPathWithRadius(&p_inv, shadow_rect, shadow_rect.InsetBy(-offset), &corner->border, TRUE);

		vp->FillPath(p_inv);
	}
	else
	{
		VEGAPath p;
		MakeBackgroundPathWithRadius(&p, shadow_rect, &corner->border, FALSE);

		vp->FillPath(p);
	}
	vp->RestoreFillState(vp_fstate);

	VEGARenderer *renderer = vp->GetRenderer();
	VEGARenderTarget* rt = vp->GetRenderTarget();
	VEGAFilter* filter = NULL;

	VEGA_FIX stdev = VEGA_INTTOFIX(scaled_blur_radius) / 2;
	if (OpStatus::IsSuccess(renderer->createGaussianFilter(&filter, stdev, stdev, false)))
	{
		filter->setSource(rt);

		VEGAFilterRegion region;
		region.sx = region.dx = 0;
		region.sy = region.dy = 0;
		region.width = src_side;
		region.height = src_side;

		renderer->applyFilter(filter, region);
	}

	src->ReleasePainter();
	OP_DELETE(filter);

	EndNoTranslationNoOffsetPainting(old_state);
	painter = old_painter;

	// Copy the blurred corner from the src bitmap to the corner bitmap.
	OP_STATUS status = OpBitmap::Create(&corner->bitmap, corner_side, corner_side, FALSE, TRUE, 0, 0, TRUE);
	if (OpStatus::IsSuccess(status))
	{
		int corner_offset = corner->inset ? scaled_padding : 0;
		OpRect src_rect = OpRect(corner_offset, corner_offset, corner_side, corner_side);
		status = static_cast<VEGAOpBitmap*>(corner->bitmap)->CopyRect(OpPoint(0, 0), src_rect, src);

		if (OpStatus::IsError(status))
			OP_DELETE(corner->bitmap);
	}

	OP_DELETE(src);

	return status;
}

OP_STATUS VisualDevice::GetCornerFill(VEGAFill** corner_fill, const Shadow &shadow, const OpRect &shadow_rect, const Border *border)
{
	int blur_radius = LimitBlurRadius(shadow.blur);
	BOOL inset = shadow.inset;
	COLORREF color = (use_def_bg_color) ? colorManager->GetBackgroundColor() : bg_color;
	int scale = GetScale(FALSE);

	// This is the minimum size needed to be able to draw the corner without
	// sampling below or to the right of the shadow rectangle.
	int width_needed = 3*blur_radius + 2*border->top.radius_start;
	int height_needed = 3*blur_radius + 2*border->left.radius_start;
	OpRect rect = OpRect(0, 0, MIN(shadow_rect.width, width_needed), MIN(shadow_rect.height, height_needed));

	OpBitmap *bitmap = NULL;
	BoxShadowCorner *corner = box_shadow_corners.First();
	while (corner)
	{
		if (corner->bitmap
			&& corner->shadow_rect.Equals(rect)
			&& corner->border.left.radius_start == border->left.radius_start
			&& corner->border.top.radius_start == border->top.radius_start
			&& corner->border.left.width == border->left.width
			&& corner->border.left.style == border->left.style
			&& corner->blur_radius == blur_radius
			&& corner->inset == inset
			&& corner->color == color
			&& corner->scale == scale)
		{
			bitmap = corner->bitmap;
			break;
		}
		corner = corner->Suc();
	}

	if (!bitmap)
	{
		BoxShadowCorner *new_corner = OP_NEW(BoxShadowCorner, ());

		if (!new_corner)
			return OpStatus::ERR_NO_MEMORY;

		new_corner->shadow_rect = rect;
		new_corner->border = *border;
		new_corner->blur_radius = blur_radius;
		new_corner->inset = inset;
		new_corner->color = color;
		new_corner->scale = scale;

		OP_STATUS status = DrawCorner(new_corner);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(new_corner);
			return status;
		}

		if (box_shadow_corners.Cardinal() >= DISPLAY_BOXSHADOWCACHESIZE)
		{
			BoxShadowCorner *last = box_shadow_corners.Last();
			last->Out();
			OP_DELETE(last);
		}
		new_corner->IntoStart(&box_shadow_corners);
		bitmap = new_corner->bitmap;

	}

	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	return static_cast<VEGAOpBitmap*>(bitmap)->getFill(vp->GetRenderer(), corner_fill);
}

// Check if the rectangle 'r' is contained in the path 'p' (which is assumed to be convex).
static BOOL PathContainsRect(VEGAPath& p, const OpRect& r)
{
	OpPoint r_corners[4] = { r.TopLeft(), r.TopRight(), r.BottomLeft(), r.BottomRight() };

	for (unsigned i = 0; i < ARRAY_SIZE(r_corners); i++)
		if (!p.isPointInside(VEGA_INTTOFIX(r_corners[i].x), VEGA_INTTOFIX(r_corners[i].y), false))
			return FALSE;

	return TRUE;
}

// Clip 'in' with 'clip' returning the result as 'out'.
static OP_STATUS ClipPathWithRect(VEGAPath& out, VEGAPath& in, const OpRect& clip)
{
	VEGATransform t;
	t.loadIdentity();

	VEGAPath tmp1, tmp2, tmp3;
	RETURN_IF_ERROR(in.constrain(tmp1, t, true, VEGA_INTTOFIX(clip.Left()), false));
	RETURN_IF_ERROR(tmp1.constrain(tmp2, t, true, VEGA_INTTOFIX(clip.Right()), true));
	RETURN_IF_ERROR(tmp2.constrain(tmp3, t, false, VEGA_INTTOFIX(clip.Top()), false));
	return tmp3.constrain(out, t, false, VEGA_INTTOFIX(clip.Bottom()), true);
}

// Append the rectangle 'r' to the path 'p'.
static OP_STATUS AppendRect(VEGAPath& p, const OpRect& r)
{
	VEGA_FIX r_left = VEGA_INTTOFIX(r.Left());
	VEGA_FIX r_right = VEGA_INTTOFIX(r.Right());
	VEGA_FIX r_top = VEGA_INTTOFIX(r.Top());
	VEGA_FIX r_bottom = VEGA_INTTOFIX(r.Bottom());

	RETURN_IF_ERROR(p.moveTo(r_left, r_top));
	RETURN_IF_ERROR(p.lineTo(r_right, r_top));
	RETURN_IF_ERROR(p.lineTo(r_right, r_bottom));
	RETURN_IF_ERROR(p.lineTo(r_left, r_bottom));
	return p.close(true);
}

OP_STATUS VisualDevice::GetCornerPaths(CornerPaths &corner_paths, const Shadow &shadow, const OpRect &shadow_rect, const OpRect &border_box, const Border *border, BOOL using_stencil)
{
	VEGAPath *paths = corner_paths.paths;

	int blur_radius = LimitBlurRadius(shadow.blur);

	OpRect outer_shadow;
	if (shadow.inset)
	{
		outer_shadow = border_box;
		outer_shadow.width -= border->left.width + border->right.width;
		outer_shadow.height -= border->top.width + border->bottom.width;
		outer_shadow.x += border->left.width;
		outer_shadow.y += border->top.width;
	}
	else
		outer_shadow = shadow_rect.InsetBy(-blur_radius);

	OpPoint middle = OpPoint((shadow_rect.Left() + shadow_rect.Right()) / 2,
							(shadow_rect.Top() + shadow_rect.Bottom()) / 2);

	OpRect tiles[NUM_CORNERS] = {
		// top-left
		OpRect(outer_shadow.Left(),
				outer_shadow.Top(),
				middle.x - outer_shadow.Left(),
				middle.y - outer_shadow.Top()),
		// top-right
		OpRect(middle.x,
				outer_shadow.Top(),
				outer_shadow.Right() - middle.x,
				middle.y - outer_shadow.Top()),
		// bottom-right
		OpRect(middle.x,
				middle.y,
				outer_shadow.Right() - middle.x,
				outer_shadow.Bottom() - middle.y),
		// bottom-left
		OpRect(outer_shadow.Left(),
				middle.y,
				middle.x - outer_shadow.Left(),
				outer_shadow.Bottom() - middle.y)
	};

	for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
	{
		tiles[i].OffsetBy(translation_x, translation_y);
		tiles[i] = ToPainter(tiles[i]);
	}

	if (using_stencil)
	{
		// If we're drawing with a stencil active it's enough to draw
		// the tile rectangle.
		for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
			RETURN_IF_ERROR(AppendRect(paths[i], tiles[i]));
	}
	else if (shadow.inset)
	{
		// Inset shadow will allow us to create convex paths for the corners.
		// TODO: we can create a simple non-convex path that will draw less
		//       pixels than the convex path created here.
		VEGAPath bg_path;
		MakeBackgroundPathWithRadius(&bg_path, border_box, border, TRUE);

		for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
			RETURN_IF_ERROR(ClipPathWithRect(paths[i], bg_path, tiles[i]));
	}
	else
	{
		OpRect rect = border_box;
		OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
		VEGA_FIX x = VEGA_INTTOFIX(trect.x);
		VEGA_FIX y = VEGA_INTTOFIX(trect.y);
		VEGA_FIX width = VEGA_INTTOFIX(trect.width);
		VEGA_FIX height = VEGA_INTTOFIX(trect.height);

		RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);
		VEGA_FIX rx = info.top_radius_start;
		VEGA_FIX ry = info.left_radius_start;

		// Check if it's possible to create a simple non-convex path.
		BOOL simple = tiles[TOP_LEFT].Contains(OpPoint(VEGA_FIXTOINT(x), VEGA_FIXTOINT(y + ry)))
				&& tiles[TOP_LEFT].Contains(OpPoint(VEGA_FIXTOINT(x + rx), VEGA_FIXTOINT(y)))
				&& tiles[TOP_RIGHT].Contains(OpPoint(VEGA_FIXTOINT(x + width - rx), VEGA_FIXTOINT(y)))
				&& tiles[TOP_RIGHT].Contains(OpPoint(VEGA_FIXTOINT(x + width), VEGA_FIXTOINT(y + ry)))
				&& tiles[BOTTOM_RIGHT].Contains(OpPoint(VEGA_FIXTOINT(x + width), VEGA_FIXTOINT(y + height - ry)))
				&& tiles[BOTTOM_RIGHT].Contains(OpPoint(VEGA_FIXTOINT(x + width - rx), VEGA_FIXTOINT(y + height)))
				&& tiles[BOTTOM_LEFT].Contains(OpPoint(VEGA_FIXTOINT(x + rx), VEGA_FIXTOINT(y + height)))
				&& tiles[BOTTOM_LEFT].Contains(OpPoint(VEGA_FIXTOINT(x), VEGA_FIXTOINT(y + height - ry)));

		if (simple)
		{
			VEGA_FIX flatness = VEGA_INTTOFIX(1)/10;
			VEGA_FIX xrot = VEGA_INTTOFIX(0);

			VEGAPath *p;
			p = &paths[TOP_LEFT];
			RETURN_IF_ERROR(p->moveTo(VEGA_INTTOFIX(tiles[TOP_LEFT].Left()), VEGA_INTTOFIX(tiles[TOP_LEFT].Top())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[TOP_LEFT].Right()), VEGA_INTTOFIX(tiles[TOP_LEFT].Top())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[TOP_LEFT].Right()), y));
			RETURN_IF_ERROR(p->lineTo(x + rx, y));
			RETURN_IF_ERROR(p->arcTo(x, y + ry, rx, ry, xrot, false, false, flatness));
			RETURN_IF_ERROR(p->lineTo(x, VEGA_INTTOFIX(tiles[TOP_LEFT].Bottom())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[TOP_LEFT].Left()), VEGA_INTTOFIX(tiles[TOP_LEFT].Bottom())));
			RETURN_IF_ERROR(p->close(true));

			p = &paths[TOP_RIGHT];
			RETURN_IF_ERROR(p->moveTo(VEGA_INTTOFIX(tiles[TOP_RIGHT].Right()), VEGA_INTTOFIX(tiles[TOP_RIGHT].Top())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[TOP_RIGHT].Right()), VEGA_INTTOFIX(tiles[TOP_RIGHT].Bottom())));
			RETURN_IF_ERROR(p->lineTo(x + width, VEGA_INTTOFIX(tiles[TOP_RIGHT].Bottom())));
			RETURN_IF_ERROR(p->lineTo(x + width, y + ry));
			RETURN_IF_ERROR(p->arcTo(x + width - rx, y, rx, ry, xrot, false, false, flatness));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[TOP_RIGHT].Left()), y));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[TOP_RIGHT].Left()), VEGA_INTTOFIX(tiles[TOP_RIGHT].Top())));
			RETURN_IF_ERROR(p->close(true));

			p = &paths[BOTTOM_RIGHT];
			RETURN_IF_ERROR(p->moveTo(VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Right()), VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Bottom())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Left()), VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Bottom())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Left()), y + height));
			RETURN_IF_ERROR(p->lineTo(x + width - rx, y + height));
			RETURN_IF_ERROR(p->arcTo(x + width, y + height - ry, rx, ry, xrot, false, false, flatness));
			RETURN_IF_ERROR(p->lineTo(x + width, VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Top())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Right()), VEGA_INTTOFIX(tiles[BOTTOM_RIGHT].Top())));
			RETURN_IF_ERROR(p->close(true));

			p = &paths[BOTTOM_LEFT];
			RETURN_IF_ERROR(p->moveTo(VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Left()), VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Bottom())));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Left()), VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Top())));
			RETURN_IF_ERROR(p->lineTo(x, VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Top())));
			RETURN_IF_ERROR(p->lineTo(x, y + height - ry));
			RETURN_IF_ERROR(p->arcTo(x + rx, y + height, rx, ry, xrot, false, false, flatness));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Right()), y + height));
			RETURN_IF_ERROR(p->lineTo(VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Right()), VEGA_INTTOFIX(tiles[BOTTOM_LEFT].Bottom())));
			RETURN_IF_ERROR(p->close(true));
		}
		else
		{
			VEGAPath bg_path;
			MakeInvertedPathWithRadius(&bg_path, border_box, border, FALSE);

			VEGA_FIX minx, miny, bbox_w, bbox_h;
			bg_path.getBoundingBox(minx, miny, bbox_w, bbox_h);

			VEGA_FIX maxx = minx + bbox_w;
			VEGA_FIX maxy = miny + bbox_h;

			for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
			{
				if (PathContainsRect(bg_path, tiles[i]))
				{
					// The tile is contained within the background shape.
					paths[i].prepare(0);
					continue;
				}

				// If the tile intersects the (bbox of the) background
				// shape then let the corner path contain the
				// exclusion of the background shape.
				if (maxx > VEGA_INTTOFIX(tiles[i].Left())  &&
					minx < VEGA_INTTOFIX(tiles[i].Right()) &&
					maxy > VEGA_INTTOFIX(tiles[i].Top())   &&
					miny < VEGA_INTTOFIX(tiles[i].Bottom()))
					RETURN_IF_ERROR(ClipPathWithRect(paths[i], bg_path, tiles[i]));

				RETURN_IF_ERROR(AppendRect(paths[i], tiles[i]));
			}
		}
	}

	return OpStatus::OK;
}

// Maximum radius of corners that we allow to be cached.
#define MAX_CORNER_RADIUS 128

// Fast case to draw box shadows with uniform border radius. As all the corners
// and all the sides are the same in this case we only need to draw one corner
// using the regular blur and then reuse that to draw all the other corners.
// Returns TRUE if handled, FALSE otherwise
BOOL VisualDevice::PaintBoxShadowIfSimple(const Shadow &shadow, const OpRect &shadow_rect, const Border *border_with_spread, const OpRect &border_box, const Border *border, BOOL using_stencil)
{

	// No blur is needed so let base painting handle this.
	if (shadow.blur <= 0)
		return FALSE;

	// FIXME: implement proper handling of this case.
	if (shadow.inset &&
		(border->left.width != 0
		 || border->top.width != 0
		 || border->right.width != 0
		 || border->bottom.width != 0))
		return FALSE;

	// TODO: support transforms for sw as well.
	if (HasTransform() && g_vegaGlobals.rasterBackend != LibvegaModule::BACKEND_HW3D)
		return FALSE;

	// Only support box shadows where all vertical radii are equal and all
	// horizontal radii are equal. This is required as we flip the corner
	// when we draw the shadow.
	if (border->left.radius_start != border->left.radius_end
		|| border->left.radius_start != border->right.radius_start
		|| border->left.radius_start != border->right.radius_end
		|| border->top.radius_start != border->top.radius_end
		|| border->top.radius_start != border->bottom.radius_start
		|| border->top.radius_start != border->bottom.radius_end)
		return FALSE;

	// Too large radii will make the corner bitmap very large. The
	// blur radius is already limited.
	if (border->left.radius_start > MAX_CORNER_RADIUS ||
		border->top.radius_start > MAX_CORNER_RADIUS)
		return FALSE;

	int blur_radius = LimitBlurRadius(shadow.blur);
	int padding = CalcPaddingNeeded(blur_radius);
	int corner_width = 2*padding + border_with_spread->left.radius_start;

	// The corner rectangle is where the corner fill will be placed. The tile
	// rectangle is the area where the corner fill will be used to draw,
	// with clamping.
	OpRect corners[NUM_CORNERS] = {
		// top-left
		OpRect(shadow_rect.Left() - padding,
				shadow_rect.Top() - padding,
				corner_width,
				corner_width),
		// top-right
		OpRect(shadow_rect.Right() - padding - border_with_spread->left.radius_start,
				shadow_rect.Top() - padding,
				corner_width,
				corner_width),
		// bottom-right
		OpRect(shadow_rect.Right() - padding - border_with_spread->left.radius_start,
				shadow_rect.Bottom() - padding - border_with_spread->left.radius_start,
				corner_width,
				corner_width),
		// bottom-left
		OpRect(shadow_rect.Left() - padding,
				shadow_rect.Bottom() - padding - border_with_spread->left.radius_start,
				corner_width,
				corner_width)
	};

	for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
	{
		corners[i].OffsetBy(translation_x, translation_y);
		corners[i] = ToPainter(corners[i]);
	}

	VEGAFill *corner_fill;
	if (OpStatus::IsError(GetCornerFill(&corner_fill, shadow, shadow_rect, border_with_spread)))
		return FALSE;
	corner_fill->setSpread(VEGAFill::SPREAD_CLAMP);

	CornerPaths corner_paths;
	if (OpStatus::IsError(GetCornerPaths(corner_paths, shadow, shadow_rect, border_box, border, using_stencil)))
		return FALSE;

	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	VEGAOpPainter::FillState vp_fstate = vp->SaveFillState();
	vp->SetFill(corner_fill);


	for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
	{
		OpRect corner = corners[i];

		// Calculate the translation and scaling needed to get the corner fill
		// in the right position for the tile.
		int xscale = 1;
		int yscale = 1;
		OpPoint p;
		switch (i)
		{
		case TOP_LEFT:
			p.x = corner.Left();
			p.y = corner.Top();
			break;
		case TOP_RIGHT:
			xscale = -1;
			p.x = corner.Right();
			p.y = corner.Top();
			break;
		case BOTTOM_RIGHT:
			xscale = -1;
			yscale = -1;
			p.x = corner.Right();
			p.y = corner.Bottom();
			break;
		case BOTTOM_LEFT:
			yscale = -1;
			p.x = corner.Left();
			p.y = corner.Bottom();
			break;
		default:
			OP_ASSERT(FALSE);
			break;
		}

		VEGATransform scale, trans;
		scale.loadScale(VEGA_INTTOFIX(xscale), VEGA_INTTOFIX(yscale));
		trans.loadTranslate(VEGA_INTTOFIX(p.x), VEGA_INTTOFIX(p.y));
		trans.multiply(scale);
		vp->SetFillTransform(trans);

		vp->FillPath(corner_paths.paths[i]);
	}

	/*
	// Debug code to show the position of the fill and the area drawn by the tile.
	VEGAPath tiles[NUM_CORNERS];
	GetCornerPaths(tiles, shadow, shadow_rect, border_box, border, FALSE);
	for (int i = TOP_LEFT; i < NUM_CORNERS; i++)
	{
		VEGA_FIX x, y, w, h;
		tiles[i].getBoundingBox(x, y, w, h);

		vp->SetColor(0, 0, 0xff, 0x66);
		vp->DrawRect(corners[i], 1);
		vp->SetColor(0, 0xff, 0, 0x66);
		vp->DrawRect(OpRect(x, y, w, h), 1);
		vp->SetFill(corner_fill);
	}
	*/

	vp->ResetFillTransform();
	vp->SetFill(NULL);
	vp->RestoreFillState(vp_fstate);

	return TRUE;
}

void VisualDevice::PaintBoxShadow(const Shadow &shadow, const OpRect &border_box, const OpRect &padding_box, COLORREF bg_color, const Border *border)
{
	int spread_half = (shadow.inset ? -shadow.spread : shadow.spread);
	if (shadow.inset)
	{
		// If spread value is more than will fit inside the box, we have to apply a limit.
		spread_half = -MIN(-spread_half, padding_box.width / 2);
		spread_half = -MIN(-spread_half, padding_box.height / 2);
	}
	int spread = spread_half * 2;
	OpRect shadow_rect(border_box.x + shadow.left - spread_half,
					   border_box.y + shadow.top - spread_half,
					   border_box.width + spread,
					   border_box.height + spread);

	if (shadow.inset && padding_box.IsEmpty() || !shadow.inset && shadow_rect.IsEmpty())
		return;

	OpRect shadow_padding_rect(padding_box.x + shadow.left - spread_half,
							   padding_box.y + shadow.top - spread_half,
							   padding_box.width + spread,
							   padding_box.height + spread);
	int blur_radius = LimitBlurRadius(shadow.blur);
	DisplayEffect effect(DisplayEffect::EFFECT_TYPE_BLUR, blur_radius);
	BOOL has_border_radius = HasBorderRadius(border);
	COLORREF color = shadow.color;

	if (shadow.color == USE_DEFAULT_COLOR)
	{
		// UA-chosen color (according to spec)
		if (bg_color == USE_DEFAULT_COLOR)
			color = OP_RGB(0, 0, 0);
		else
			color = bg_color;
	}
	SetColor(color);
	SetBgColor(color);

	// Normal shadow:	An outer box-shadow casts a shadow as if the border-box of the element were opaque.
	//					The shadow is drawn outside the border edge only: it is not drawn inside the border-box of the element.
	// Inset shadow:	An inner box-shadow casts a shadow as if everything outside the padding edge were opaque.
	//					The inner shadow is drawn inside the padding edge only: it is not drawn outside the padding box of the element.

	if (!has_border_radius && !shadow.blur)
	{
		// No blur and no roundness makes it really easy and faster to do with a simple region

		BgRegion region;
		if (shadow.inset)
		{
			if (OpStatus::IsError(region.Set(padding_box)) ||
				OpStatus::IsError(region.ExcludeRect(shadow_padding_rect, FALSE)))
				return;
		}
		else
		{
			if (OpStatus::IsError(region.Set(shadow_rect)) ||
				OpStatus::IsError(region.ExcludeRect(border_box, FALSE)))
					return;
		}

		FlushBackgrounds(shadow.inset ? padding_box : shadow_rect);

		for(int i = 0; i < region.num_rects; i++)
			FillRect(region.rects[i]);
	}
	else
	{
		// For blurred or rounded shadow, we will do this:
		// - Try to draw the shadow with the fast path without using a stencil.
		// - If that doesn't work; create a stencil which includes only the
		//   shape in which the shadow should be painted (Outside for regular
		//   shadow and inside for inset).
		// - Paint the shadow using that stencil.

		int expand = shadow.blur ? blur_radius : 0;
		OpRect bounding_rect = border_box;
		bounding_rect.UnionWith(shadow_rect);
		bounding_rect = bounding_rect.InsetBy(-expand);

		Border border_with_spread = *border;
		RadiusPathCalculator::InsetBorderRadius(&border_with_spread, -spread_half, -spread_half, -spread_half, -spread_half);

		// Try do draw with the fast path without using a stencil.
		BeginClipping(bounding_rect);
		BOOL drawn_with_fast_path = PaintBoxShadowIfSimple(shadow, shadow_rect, &border_with_spread, border_box, border, FALSE);
		EndClipping();

		if (drawn_with_fast_path)
			return;

		OpRect blur_rect = shadow.inset ? border_box : shadow_rect;

		BgRegion region;
		region.Set(bounding_rect);

		BOOL needs_split = shadow.blur ? TRUE : FALSE;
#ifdef VEGA_LIMIT_BITMAP_SIZE
		if (needs_split)
			needs_split = g_vegaGlobals.rasterBackend != LibvegaModule::BACKEND_HW3D;
#endif // VEGA_LIMIT_BITMAP_SIZE
		if (needs_split)
		{
			// FIX: Maybe we should do this even when we don't have blur? Clearing the stencil is also quite expensive.

			// Blurring is expensive, so we will split the area into small pieces so we don't have to
			// blur the area that is covered by the box or doesn't have any visible shadow anyway.

			OpRect trect = ToPainter(OpRect(border_box.x + translation_x, border_box.y + translation_y, border_box.width, border_box.height));
			RadiusPathCalculator info(trect.width, trect.height, &border_with_spread, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

			int left_radius_max = ScaleToDoc(VEGA_FIXTOINT(MAX(info.top_radius_start, info.bottom_radius_start)));
			int top_radius_max = ScaleToDoc(VEGA_FIXTOINT(MAX(info.left_radius_start, info.right_radius_start)));
			int right_radius_max = ScaleToDoc(VEGA_FIXTOINT(MAX(info.top_radius_stop, info.bottom_radius_stop)));
			int bottom_radius_max = ScaleToDoc(VEGA_FIXTOINT(MAX(info.left_radius_stop, info.right_radius_stop)));

#if 1
			if (shadow.inset)
			{
				if (OpStatus::IsSuccess(region.Set(padding_box)))
				{
					OpRect shadow_rect_inset = shadow_padding_rect.InsetBy(expand);
					OpRect hslice(shadow_rect_inset.x, shadow_rect_inset.y + top_radius_max, shadow_rect_inset.width, shadow_rect_inset.height - top_radius_max - bottom_radius_max);
					OpRect vslice(shadow_rect_inset.x + left_radius_max, shadow_rect_inset.y, shadow_rect_inset.width - left_radius_max - right_radius_max, shadow_rect_inset.height);

					/* Only split into smaller rectangles if there is
					 * significant savings in area to paint.  The
					 * overhead for each painted rectangle is
					 * significant, particularly with hwaccel.
					 */
					if (hslice.width * hslice.height > 5000 ||
						vslice.width * vslice.height > 5000)
					{
						if (!hslice.IsEmpty())
							OpStatus::Ignore(region.ExcludeRect(hslice, FALSE));
						if (!vslice.IsEmpty())
							OpStatus::Ignore(region.ExcludeRect(vslice, FALSE));
					}
				}
			}
			else
			{
				OpRect blur_rect_inset = blur_rect.InsetBy(-expand);
				if (OpStatus::IsSuccess(region.Set(blur_rect_inset)))
				{
					OpRect hslice(border_box.x, border_box.y + top_radius_max, border_box.width, border_box.height - top_radius_max - bottom_radius_max);
					OpRect vslice(border_box.x + left_radius_max, border_box.y, border_box.width - left_radius_max - right_radius_max, border_box.height);

					/* Only split into smaller rectangles if there is
					 * significant savings in area to paint.  The
					 * overhead for each painted rectangle is
					 * significant, particularly with hwaccel.
					 */
					if (hslice.width * hslice.height > 5000 ||
						vslice.width * vslice.height > 5000)
					{
						if (!hslice.IsEmpty())
							OpStatus::Ignore(region.ExcludeRect(hslice, FALSE));
						if (!vslice.IsEmpty())
							OpStatus::Ignore(region.ExcludeRect(vslice, FALSE));
					}

					// Remove parts that doesn't produce visible shadow (intersects fully with the border_box).
					// FIX: Have to remove this optimization for now. Doesn't detect cases when corners are needed because of extremly long rounding.
					/*for(int i = 0; i < region.num_rects; i++)
					{
						if (border_box.Contains(region.rects[i]))
						{
							region.RemoveRect(i);
							i--;
						}
					}*/
				}
			}
#endif

			// FIX: Optimize the region (coalesce too small pieces)

			// Flush intersectig backgrounds. We need to do this because outside parts might be flushed from FlushLast when we have a clipping set.
			FlushBackgrounds(blur_rect);
		}


#ifdef VEGA_LIMIT_BITMAP_SIZE
		int max_tile_side = (int)VEGARendererBackend::maxBitmapSide();
		if (max_tile_side > 0)
		{
			// actual bitmaps will be created in screen space
			max_tile_side = ScaleToDocRoundDown(max_tile_side) - 2*expand;
			if (max_tile_side <= 0) // radius exceeds max tile size
				return;
			region.Constrain(max_tile_side);
		}
#endif // VEGA_LIMIT_BITMAP_SIZE

		OpRect *cliprects = region.rects;
		int num_cliprects = region.num_rects;

		int i;
		for(i = 0; i < num_cliprects; i++)
		{
			OP_ASSERT(!cliprects[i].IsEmpty());
			BeginClipping(cliprects[i]);

			OP_STATUS stencil_status = BeginStencil(bounding_rect);
			if (OpStatus::IsSuccess(stencil_status))
			{
				BeginModifyingStencil();

				if (shadow.inset)
				{
					VEGAOpPainter *vp = (VEGAOpPainter *) painter;

					VEGAPath p;
					MakeBackgroundPathWithRadius(&p, border_box, border, TRUE);

					// Color ignored when painting on stencils
					vp->FillPath(p);
				}
				else
				{
					VEGAOpPainter *vp = (VEGAOpPainter *) painter;

					VEGAPath p_inv;
					MakeInvertedPathWithRadius(&p_inv, border_box, bounding_rect, border, FALSE);

					// Color ignored when painting on stencils
					vp->FillPath(p_inv);
				}

				EndModifyingStencil();

				if (!PaintBoxShadowIfSimple(shadow, shadow_rect, &border_with_spread, border_box, border))
				{
					BOOL has_blur = (shadow.blur > 0 && OpStatus::IsSuccess(BeginEffect(blur_rect, effect)));

					if (shadow.inset)
					{
						VEGAOpPainter *vp = (VEGAOpPainter *) painter;

						VEGAPath p_inv;
						MakeInvertedPathWithRadius(&p_inv, shadow_rect, bounding_rect, &border_with_spread, TRUE);

						vp->FillPath(p_inv);
					}
					else
					{
						DrawBgColorWithRadius(shadow_rect, &border_with_spread, FALSE);
					}

					if (has_blur)
						EndEffect();
				}

				EndStencil();
			}
			else if (OpStatus::IsMemoryError(stencil_status))
			{
				DrawBgColorWithRadius(shadow_rect, border, FALSE);
			}

			EndClipping();
		}
		// Debug to show where we really end up painting blurry shadows after all region clipping.
		/*SetColor(255, 0, 255);
		for(i = 0; i < region.num_rects; i++)
		{
			OpRect r = region.rects[i];
			if (!r.IsEmpty())
				DrawRect(r);
		}*/
	}
}

void VisualDevice::MakeBackgroundPathWithRadius(VEGAPath *p, const OpRect &rect, const Border *border, BOOL inside_border)
{
	OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
	VEGA_FIX x = VEGA_INTTOFIX(trect.x);
	VEGA_FIX y = VEGA_INTTOFIX(trect.y);
	VEGA_FIX width = VEGA_INTTOFIX(trect.width);
	VEGA_FIX height = VEGA_INTTOFIX(trect.height);

	RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

	if (inside_border)
		info.MakeBackgroundPathInsideBorder(p, border, x, y, width, height);
	else
		info.MakeBackgroundPath(p, border, x, y, width, height);
}

void VisualDevice::MakeBorderPathWithRadius(VEGAPath *p, const OpRect &rect, const Border *border, UINT16 border_id)
{
	OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
	VEGA_FIX x = VEGA_INTTOFIX(trect.x);
	VEGA_FIX y = VEGA_INTTOFIX(trect.y);
	VEGA_FIX width = VEGA_INTTOFIX(trect.width);
	VEGA_FIX height = VEGA_INTTOFIX(trect.height);

	RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

	info.MakeBorderPath(p, border, border_id, x, y, width, height);
}

void VisualDevice::MakeInvertedPathWithRadius(VEGAPath *p, const OpRect &rect, const Border *border, BOOL inside_border)
{
	OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
	VEGA_FIX x = VEGA_INTTOFIX(trect.x);
	VEGA_FIX y = VEGA_INTTOFIX(trect.y);
	VEGA_FIX width = VEGA_INTTOFIX(trect.width);
	VEGA_FIX height = VEGA_INTTOFIX(trect.height);

	RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

	if (inside_border)
		info.MakeInvertedPathInsideBorder(p, border, x, y, width, height);
	else
		info.MakeInvertedPath(p, border, x, y, width, height);
}

void VisualDevice::MakeInvertedPathWithRadius(VEGAPath *p, const OpRect &rect, const OpRect &bounding_rect, const Border *border, BOOL inside_border)
{
	OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
	OpRect tbounding_rect = ToPainter(OpRect(bounding_rect.x + translation_x, bounding_rect.y + translation_y, bounding_rect.width, bounding_rect.height));
	VEGA_FIX x = VEGA_INTTOFIX(trect.x);
	VEGA_FIX y = VEGA_INTTOFIX(trect.y);
	VEGA_FIX width = VEGA_INTTOFIX(trect.width);
	VEGA_FIX height = VEGA_INTTOFIX(trect.height);
	VEGA_FIX bounding_x = VEGA_INTTOFIX(tbounding_rect.x);
	VEGA_FIX bounding_y = VEGA_INTTOFIX(tbounding_rect.y);
	VEGA_FIX bounding_width = VEGA_INTTOFIX(tbounding_rect.width);
	VEGA_FIX bounding_height = VEGA_INTTOFIX(tbounding_rect.height);

	RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

	if (inside_border)
	{
		info.MakeInvertedPathInsideBorder(p, border, x, y, width, height);

		RETURN_VOID_IF_ERROR(p->moveTo(bounding_x, bounding_y));
		RETURN_VOID_IF_ERROR(p->lineTo(bounding_x + bounding_width, bounding_y));
		RETURN_VOID_IF_ERROR(p->lineTo(bounding_x + bounding_width, bounding_y + bounding_height));
		RETURN_VOID_IF_ERROR(p->lineTo(bounding_x, bounding_y + bounding_height));
		RETURN_VOID_IF_ERROR(p->close(true));
	}
	else
		info.MakeInvertedPath(p, border, x, y, width, height, bounding_x, bounding_y, bounding_width, bounding_height);
}

void FillPathCorner(VEGAOpPainter *vp, VEGAPath *path, COLORREF cref, BOOL use_gradient, VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2, const OpRect &clip_rect)
{
	cref = HTM_Lex::GetColValByIndex(cref);
	UINT8 col_a = OP_GET_A_VALUE(cref);
	UINT8 col_r = OP_GET_R_VALUE(cref);
	UINT8 col_g = OP_GET_G_VALUE(cref);
	UINT8 col_b = OP_GET_B_VALUE(cref);
	vp->SetColor(col_r, col_g, col_b, col_a);

	VEGAFill *gradient = NULL;
	if (use_gradient)
	{
		VEGA_FIX offsets[2] = { VEGA_INTTOFIX(1)/2, VEGA_INTTOFIX(1)/2 + VEGA_INTTOFIX(1)/100 }; // 0.5 0.51
		UINT32 colors[2] = { OP_COLORREF2VEGA(cref), OP_COLORREF2VEGA(cref) & 0x00FFFFFF };

		gradient = vp->CreateLinearGradient(x1, y1, x2, y2, 2, offsets, colors);

		if (gradient)
		{
			gradient->setSpread(VEGAFill::SPREAD_CLAMP);
			vp->SetFill(gradient);
		}
	}
	if (OpStatus::IsSuccess(vp->SetClipRect(clip_rect)))
	{
		// The use of this function (FillPathCorner) seem to indicate
		// that <path> should not be modified.
		vp->FillPathImmutable(*path);
		vp->RemoveClipRect();
	}
	if (gradient)
	{
		vp->SetFill(NULL);
		OP_DELETE(gradient);
	}
}

void VisualDevice::DrawBgColorWithRadius(const OpRect& rect, const Border *border, BOOL clip /* = TRUE */)
{
	if (clip && !doc_display_rect.Intersecting(ToBBox(rect)))
		return;

	COLORREF cref = (use_def_bg_color) ? colorManager->GetBackgroundColor() : bg_color;

	VEGAPath p;
	MakeBackgroundPathWithRadius(&p, rect, border, FALSE);

	VEGAOpPainter *vp = (VEGAOpPainter *) painter;
	VEGAOpPainter::FillState vp_fstate = vp->SaveFillState();
	vp->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref), OP_GET_A_VALUE(cref));
	vp->FillPath(p);
	vp->RestoreFillState(vp_fstate);
}

COLORREF VisualDevice::GetBorderColor(const BorderEdge *border_edge, BOOL top_or_left)
{
	COLORREF col = HTM_Lex::GetColValByIndex(border_edge->color);
	if (border_edge->style == CSS_VALUE_outset ||
		border_edge->style == CSS_VALUE_inset)
	{
		COLORREF col_dark;
		COLORREF col_light;
		Get3D_Colors(col, col_dark, col_light);
		if (border_edge->style == CSS_VALUE_inset)
			top_or_left = !top_or_left;
		col = (top_or_left ? col_light : col_dark);
	}
	return col;
}

static void SwitchBorderStyle(Border *border_excluding, Border *border_including, short replace_style, CSSValue with_style)
{
	if (border_including->left.style == replace_style)
	{
		border_excluding->left.color = COLORREF(CSS_COLOR_transparent);
		border_including->left.style = with_style;
	}
	else
		border_including->left.color = COLORREF(CSS_COLOR_transparent);

	if (border_including->top.style == replace_style)
	{
		border_excluding->top.color = COLORREF(CSS_COLOR_transparent);
		border_including->top.style = with_style;
	}
	else
		border_including->top.color = COLORREF(CSS_COLOR_transparent);

	if (border_including->right.style == replace_style)
	{
		border_excluding->right.color = COLORREF(CSS_COLOR_transparent);
		border_including->right.style = with_style;
	}
	else
		border_including->right.color = COLORREF(CSS_COLOR_transparent);

	if (border_including->bottom.style == replace_style)
	{
		border_excluding->bottom.color = COLORREF(CSS_COLOR_transparent);
		border_including->bottom.style = with_style;
	}
	else
		border_including->bottom.color = COLORREF(CSS_COLOR_transparent);
}

void VisualDevice::ModifyAndDrawBorderOfSpecificStyle(int width, int height, const Border *border, short replace_style, CSSValue with_style,
														short b_left_width, short b_top_width, short b_right_width, short b_bottom_width,
														int top_gap_x, int top_gap_width)
{
	Border b_excluding = *border;
	Border b_including = *border;

	SwitchBorderStyle(&b_excluding, &b_including, replace_style, with_style);
	DrawBorderWithRadius(width, height, &b_excluding, top_gap_x, top_gap_width);

	b_including.left.width = b_left_width;
	b_including.top.width = b_top_width;
	b_including.right.width = b_right_width;
	b_including.bottom.width = b_bottom_width;

	DrawBorderWithRadius(width, height, &b_including, top_gap_x, top_gap_width);

	int b_ofs_left = border->left.width - b_left_width;
	int b_ofs_top = border->top.width - b_top_width;
	int b_ofs_right = border->right.width - b_right_width;
	int b_ofs_bottom = border->bottom.width - b_bottom_width;

	RadiusPathCalculator::InsetBorderRadius(&b_including, b_ofs_left, b_ofs_top, b_ofs_right, b_ofs_bottom);

	// Don't draw overlapping the previous border
	if (border->left.width == 1)
		b_including.left.color = COLORREF(CSS_COLOR_transparent);
	if (border->top.width == 1)
		b_including.top.color = COLORREF(CSS_COLOR_transparent);
	if (border->right.width == 1)
		b_including.right.color = COLORREF(CSS_COLOR_transparent);
	if (border->bottom.width == 1)
		b_including.bottom.color = COLORREF(CSS_COLOR_transparent);

	if (with_style == CSS_VALUE_inset)
		SwitchBorderStyle(&b_excluding, &b_including, CSS_VALUE_inset, CSS_VALUE_outset);
	else if (with_style == CSS_VALUE_outset)
		SwitchBorderStyle(&b_excluding, &b_including, CSS_VALUE_outset, CSS_VALUE_inset);

	translation_x += b_ofs_left;
	translation_y += b_ofs_top;

	DrawBorderWithRadius(width - b_ofs_left - b_ofs_right, height - b_ofs_top - b_ofs_bottom, &b_including, top_gap_x, top_gap_width);

	translation_x -= b_ofs_left;
	translation_y -= b_ofs_top;
}

void VisualDevice::DrawBorderWithRadius(int width, int height, const Border *border, int top_gap_x, int top_gap_width)
{
	OpRect rect(0, 0, width, height);
	// Debugging
	/*SetColor(0, 0, 0, 128);
	DrawRect(rect);*/

	if (!doc_display_rect.Intersecting(ToBBox(rect)))
		return;

	VEGAOpPainter *vp = (VEGAOpPainter *) painter;

	BOOL has_variable_width = !(border->left.width == border->top.width &&
								border->top.width == border->right.width &&
								border->right.width == border->bottom.width);
	// The width is only really "variable" if there is radius where it changes. Otherwise we don't need any smooth transition.
	// This didn't work in some cases.
	//BOOL has_variable_width = ((border->left.radius_start > 0 && border->top.radius_start > 0) && border->left.width != border->top.width) ||
	//						  ((border->top.radius_end > 0 && border->right.radius_start > 0) && border->top.width != border->right.width) ||
	//						  ((border->right.radius_end > 0 && border->bottom.radius_end > 0) && border->right.width != border->bottom.width) ||
	//						  ((border->left.radius_end > 0 && border->bottom.radius_start > 0) && border->left.width != border->bottom.width);

	BOOL has_visible_border_left = IS_BORDER_VISIBLE(border->left);
	BOOL has_visible_border_top = IS_BORDER_VISIBLE(border->top);
	BOOL has_visible_border_right = IS_BORDER_VISIBLE(border->right);
	BOOL has_visible_border_bottom = IS_BORDER_VISIBLE(border->bottom);

	if (!has_visible_border_left &&
		!has_visible_border_top &&
		!has_visible_border_right &&
		!has_visible_border_bottom)
		return;

	// Filter out double, ridge and groove border styles.
	// In those cases, we will construct new borders. One excluding those which is drawn normally, and one
	// which is drawn twice to represent the outer and inner part of the style.
	// Currently this will cause glitches in non even zooms, and some overlap in some bordersizes which isn't good :(
	if ((has_visible_border_left && border->left.style == CSS_VALUE_double && border->left.width > 2) ||
		(has_visible_border_top && border->top.style == CSS_VALUE_double && border->top.width > 2) ||
		(has_visible_border_right && border->right.style == CSS_VALUE_double && border->right.width > 2) ||
		(has_visible_border_bottom && border->bottom.style == CSS_VALUE_double && border->bottom.width > 2))
	{
		ModifyAndDrawBorderOfSpecificStyle(width, height, border, CSS_VALUE_double, CSS_VALUE_solid,
											GetDoubleBorderSingleWidth(border->left.width),
											GetDoubleBorderSingleWidth(border->top.width),
											GetDoubleBorderSingleWidth(border->right.width),
											GetDoubleBorderSingleWidth(border->bottom.width),
											top_gap_x, top_gap_width);
		return;
	}
	if ((has_visible_border_left && border->left.style == CSS_VALUE_groove) ||
		(has_visible_border_top && border->top.style == CSS_VALUE_groove) ||
		(has_visible_border_right && border->right.style == CSS_VALUE_groove) ||
		(has_visible_border_bottom && border->bottom.style == CSS_VALUE_groove))
	{
		ModifyAndDrawBorderOfSpecificStyle(width, height, border, CSS_VALUE_groove, CSS_VALUE_inset,
											(border->left.width + 1) / 2,
											(border->top.width + 1) / 2,
											(border->right.width + 1) / 2,
											(border->bottom.width + 1) / 2,
											top_gap_x, top_gap_width);
		return;
	}
	if ((has_visible_border_left && border->left.style == CSS_VALUE_ridge) ||
		(has_visible_border_top && border->top.style == CSS_VALUE_ridge) ||
		(has_visible_border_right && border->right.style == CSS_VALUE_ridge) ||
		(has_visible_border_bottom && border->bottom.style == CSS_VALUE_ridge))
	{
		ModifyAndDrawBorderOfSpecificStyle(width, height, border, CSS_VALUE_ridge, CSS_VALUE_outset,
											(border->left.width + 1) / 2,
											(border->top.width + 1) / 2,
											(border->right.width + 1) / 2,
											(border->bottom.width + 1) / 2,
											top_gap_x, top_gap_width);
		return;
	}

	COLORREF left_col = GetBorderColor(&border->left, TRUE);
	COLORREF top_col = GetBorderColor(&border->top, TRUE);
	COLORREF right_col = GetBorderColor(&border->right, FALSE);
	COLORREF bottom_col = GetBorderColor(&border->bottom, FALSE);

	BOOL is_continous = (has_visible_border_left == has_visible_border_top &&
						 has_visible_border_top == has_visible_border_right &&
						 has_visible_border_right == has_visible_border_bottom &&
						 left_col == top_col &&
						 top_col == right_col &&
						 right_col == bottom_col &&
						 !has_variable_width &&
						 !top_gap_width);

	// Get the color of any borderside that is on (not using has_visible_border_* since those are FALSE for transparent)
	COLORREF some_border_col = 0;
	if (IS_BORDER_ON(border->left))
		some_border_col = border->left.color;
	else if (IS_BORDER_ON(border->top))
		some_border_col = border->top.color;
	else if (IS_BORDER_ON(border->right))
		some_border_col = border->right.color;
	else if (IS_BORDER_ON(border->bottom))
		some_border_col = border->bottom.color;
	BOOL is_all_visible_borders_solid_same_color = !top_gap_width &&
													(!IS_BORDER_ON(border->left) || (border->left.style == CSS_VALUE_solid && border->left.color == some_border_col)) &&
													(!IS_BORDER_ON(border->top) || (border->top.style == CSS_VALUE_solid && border->top.color == some_border_col)) &&
													(!IS_BORDER_ON(border->right) || (border->right.style == CSS_VALUE_solid && border->right.color == some_border_col)) &&
													(!IS_BORDER_ON(border->bottom) || (border->bottom.style == CSS_VALUE_solid && border->bottom.color == some_border_col));

	if (is_all_visible_borders_solid_same_color)
	{
		// If all visible borders are solid and using the same color, we can
		// create a path without using createOutline. This works better with thick borders and small radius.
		// It also supports variable widths without stencil and no pixel-glitches because there's no overlap.

		OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
		VEGA_FIX x = VEGA_INTTOFIX(trect.x);
		VEGA_FIX y = VEGA_INTTOFIX(trect.y);
		VEGA_FIX width = VEGA_INTTOFIX(trect.width);
		VEGA_FIX height = VEGA_INTTOFIX(trect.height);

		RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

		VEGAPath p;
		info.MakeBackgroundPath(&p, border, x, y, width, height);
		info.MakeInvertedPathInsideBorder(&p, border, x, y, width, height);

		SetColor(some_border_col);
		vp->FillPath(p);
	}
	else if (is_continous)
	{
		// If the border is the same around the whole rectangle, we use 1 path in 1 pass (For correctness and speed).
		VEGAPath p;
		MakeBorderPathWithRadius(&p, rect, border, 0x1111);

		SetColor(left_col);
		vp->FillPath(p);
	}
	else
	{
		// Some edge was different. Handle each edge separately.

		OpRect trect = ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height));
		VEGA_FIX x = VEGA_INTTOFIX(trect.x);
		VEGA_FIX y = VEGA_INTTOFIX(trect.y);
		VEGA_FIX width = VEGA_INTTOFIX(trect.width);
		VEGA_FIX height = VEGA_INTTOFIX(trect.height);

		int half_w = trect.width / 2;
		int half_h = trect.height / 2;
		OpRect half_top_rect(trect.x, trect.y, trect.width, half_h);
		OpRect half_bottom_rect(trect.x, trect.y + half_h, trect.width, trect.height - half_h);
		OpRect half_left_rect(trect.x, trect.y, half_w, trect.height);
		OpRect half_right_rect(trect.x + half_w, trect.y, trect.width - half_w, trect.height);

		RadiusPathCalculator info(trect.width, trect.height, border, VEGA_INTTOFIX(scale_multiplier)/scale_divider);

		const VEGA_FIX border_left_width = VEGA_INTTOFIX(IS_BORDER_ON(border->left) ? (border->left.width) : 0);
		const VEGA_FIX border_top_width = VEGA_INTTOFIX(IS_BORDER_ON(border->top) ? (border->top.width) : 0);
		const VEGA_FIX border_right_width = VEGA_INTTOFIX(IS_BORDER_ON(border->right) ? (border->right.width) : 0);
		const VEGA_FIX border_bottom_width = VEGA_INTTOFIX(IS_BORDER_ON(border->bottom) ? (border->bottom.width) : 0);
		VEGA_FIX posx[8] = { x - border_top_width,
							 x + border_top_width,
							 x + width - border_top_width,
							 x + width + border_top_width,
							 x + width + border_bottom_width,
							 x + width - border_bottom_width,
							 x + border_bottom_width,
							 x - border_bottom_width };
		VEGA_FIX posy[8] = { y + border_left_width,
							 y - border_left_width,
							 y - border_right_width,
							 y + border_right_width,
							 y + height - border_right_width,
							 y + height + border_right_width,
							 y + height + border_left_width,
							 y + height - border_left_width };
		VEGA_FIX border_width_extra = 0;
#ifdef DEBUG_VARIABLEPATH
		VEGAPath debugpath;
#endif

		BOOL has_stencil = FALSE;
		if (has_variable_width && OpStatus::IsSuccess(BeginStencil(rect)))
		{
			// The stencil adds antialias exactly where the border adds antialias which creates too weak pixels (ugly corners).
			// Apply workaround (thicker line if it's solid) to the line width (Could probably just draw a big box instead since it will follow stencil).
			border_width_extra = VEGA_INTTOFIX(1)/1;
			has_stencil = TRUE;
			VEGAPath p;
			info.MakeBackgroundPath(&p, border, x, y, width, height);
			info.MakeInvertedPathInsideBorder(&p, border, x, y, width, height);

			BeginModifyingStencil();

			SetColor(OP_RGB(0, 0, 0));
			// The SetColor above seems redundant, because this paints
			// on a stencil, where colors don't matter
			vp->FillPath(p);

			EndModifyingStencil();

#ifdef DEBUG_VARIABLEPATH
			p.setLineWidth(1);
			RETURN_VOID_IF_ERROR(p.createOutline(&debugpath, VEGA_INTTOFIX(1)/10));
#endif
		}

		VEGAOpPainter::FillState vp_fstate = vp->SaveFillState();
		if (has_visible_border_left)
		{
			VEGAPath p;
			info.MakeBorderPath(&p, border, 0x0001, x, y, width, height, border->left.style == CSS_VALUE_solid ? border_width_extra : 0);
			FillPathCorner(vp, &p, left_col, IS_BORDER_ON(border->top), posx[0], posy[0], posx[1], posy[1], half_top_rect);
			FillPathCorner(vp, &p, left_col, IS_BORDER_ON(border->bottom), posx[7], posy[7], posx[6], posy[6], half_bottom_rect);
		}
		if (has_visible_border_top)
		{
			VEGAPath p;
			info.MakeBorderPath(&p, border, 0x0010, x, y, width, height, border->top.style == CSS_VALUE_solid ? border_width_extra : 0);
			if (top_gap_width)
			{
				int x1 = trect.x + ScaleToScreen(top_gap_x);
				int x2 = x1 + ScaleToScreen(top_gap_width);

				// First part (to the left of the gap)
				OpRect r1(half_left_rect);
				OpRect r2(half_right_rect);
				r1.width = MIN(r1.width, x1 - r1.x);
				r2.width = MIN(r2.width, x1 - r2.x);
				if (!r1.IsEmpty())
					FillPathCorner(vp, &p, top_col, IS_BORDER_ON(border->left), posx[1], posy[1], posx[0], posy[0], r1);
				if (!r2.IsEmpty())
					FillPathCorner(vp, &p, top_col, IS_BORDER_ON(border->right), posx[2], posy[2], posx[3], posy[3], r2);

				// Second part (to the right of the gap)
				r1.x = MAX(r1.x, x2);
				r2.x = MAX(r2.x, x2);
				r1.width = half_left_rect.x + half_left_rect.width - r1.x;
				r2.width = half_right_rect.x + half_right_rect.width - r2.x;
				if (!r1.IsEmpty())
					FillPathCorner(vp, &p, top_col, IS_BORDER_ON(border->left), posx[1], posy[1], posx[0], posy[0], r1);
				if (!r2.IsEmpty())
					FillPathCorner(vp, &p, top_col, IS_BORDER_ON(border->right), posx[2], posy[2], posx[3], posy[3], r2);
			}
			else
			{
				FillPathCorner(vp, &p, top_col, IS_BORDER_ON(border->left), posx[1], posy[1], posx[0], posy[0], half_left_rect);
				FillPathCorner(vp, &p, top_col, IS_BORDER_ON(border->right), posx[2], posy[2], posx[3], posy[3], half_right_rect);
			}
		}
		if (has_visible_border_right)
		{
			VEGAPath p;
			info.MakeBorderPath(&p, border, 0x0100, x, y, width, height, border->right.style == CSS_VALUE_solid ? border_width_extra : 0);
			FillPathCorner(vp, &p, right_col, IS_BORDER_ON(border->top), posx[3], posy[3], posx[2], posy[2], half_top_rect);
			FillPathCorner(vp, &p, right_col, IS_BORDER_ON(border->bottom), posx[4], posy[4], posx[5], posy[5], half_bottom_rect);
		}
		if (has_visible_border_bottom)
		{
			VEGAPath p;
			info.MakeBorderPath(&p, border, 0x1000, x, y, width, height, border->bottom.style == CSS_VALUE_solid ? border_width_extra : 0);
			FillPathCorner(vp, &p, bottom_col, IS_BORDER_ON(border->left), posx[6], posy[6], posx[7], posy[7], half_left_rect);
			FillPathCorner(vp, &p, bottom_col, IS_BORDER_ON(border->right), posx[5], posy[5], posx[4], posy[4], half_right_rect);
		}
		vp->RestoreFillState(vp_fstate);

		if (has_stencil)
			EndStencil();

#ifdef DEBUG_VARIABLEPATH
		SetColor(OP_RGB(0, 0, 0));
		renderer->setColor(vp->GetCurrentColor());
		renderer->fillPath(&debugpath);

		SetColor(OP_RGBA(0, 0, 0, 128));
		FillRect(OpRect(0, 0, 1, 1));
		FillRect(OpRect(width-1, 0, 1, 1));
		FillRect(OpRect(0, height-1, 1, 1));
		FillRect(OpRect(width-1, height-1, 1, 1));
		SetColor(OP_RGBA(255, 0, 128, 200));
		FillRect(OpRect(info.top_radius_start, border_top_width, 1, 1));
		FillRect(OpRect(border_left_width, info.left_radius_start, 1, 1));
		DrawLine(OpPoint(info.top_radius_start, border_top_width), OpPoint(border_left_width, info.left_radius_start));

		FillRect(OpRect(border_left_width, height - info.left_radius_stop, 1, 1));
		FillRect(OpRect(info.bottom_radius_start, height - border_bottom_width, 1, 1));
		DrawLine(OpPoint(border_left_width, height - info.left_radius_stop), OpPoint(info.bottom_radius_start, height - border_bottom_width));

		FillRect(OpRect(width - info.bottom_radius_stop, height - border_bottom_width, 1, 1));
		FillRect(OpRect(width - border_right_width, height - info.right_radius_stop, 1, 1));
		DrawLine(OpPoint(width - info.bottom_radius_stop, height - border_bottom_width), OpPoint(width - border_right_width, height - info.right_radius_stop));

		FillRect(OpRect(width - border_right_width, info.right_radius_start, 1, 1));
		FillRect(OpRect(width - info.top_radius_stop, border_top_width, 1, 1));
		DrawLine(OpPoint(width - border_right_width, info.right_radius_start), OpPoint(width - info.top_radius_stop, border_top_width));

		SetColor(OP_RGBA(0, 128, 0, 200));
		DrawLine(OpPoint(posx[0] - x, posy[0] - y), OpPoint(posx[1] - x, posy[1] - y));
		DrawLine(OpPoint(posx[2] - x, posy[2] - y), OpPoint(posx[3] - x, posy[3] - y));
		DrawLine(OpPoint(posx[4] - x, posy[4] - y), OpPoint(posx[5] - x, posy[5] - y));
		DrawLine(OpPoint(posx[6] - x, posy[6] - y), OpPoint(posx[7] - x, posy[7] - y));
#endif
	}
}

#endif // VEGA_OPPAINTER_SUPPORT
