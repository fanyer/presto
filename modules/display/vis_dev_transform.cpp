/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/vis_dev.h"

#ifdef VEGA_OPPAINTER_SUPPORT /* Implied by CSS_TRANSFORMS */
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // VEGA_OPPAINTER_SUPPORT
// AffineTransform

#ifdef CSS_TRANSFORMS
BOOL AffineTransform::operator==(const AffineTransform &trfm) const
{
	return
		values[0] == trfm[0] && values[1] == trfm[1] &&
		values[2] == trfm[2] && values[3] == trfm[3] &&
		values[4] == trfm[4] && values[5] == trfm[5];
}

#ifdef SELFTEST
BOOL AffineTransform::IsEqual(const AffineTransform &trfm, float eps) const
{
#define FLT_IS_CLOSE(a,b) (op_fabs((a)-(b)) < eps)
	return
		FLT_IS_CLOSE(values[0], trfm[0]) && FLT_IS_CLOSE(values[1], trfm[1]) &&
		FLT_IS_CLOSE(values[2], trfm[2]) && FLT_IS_CLOSE(values[3], trfm[3]) &&
		FLT_IS_CLOSE(values[4], trfm[4]) && FLT_IS_CLOSE(values[5], trfm[5]);
#undef FLT_IS_CLOSE 
}
#endif // SELFTEST

void AffineTransform::LoadIdentity()
{
	values[1] = values[2] = values[3] = values[5] = 0.0f;
	values[0] = values[4] = 1.0f;
}

// load(a, b, c, d, e, f) = [a b c; d e f]
void AffineTransform::LoadValues(float a, float b, float c, float d, float e, float f)
{
	values[0] = a;
	values[1] = b;
	values[2] = c;
	values[3] = d;
	values[4] = e;
	values[5] = f;
}

// translate(tx, ty) = [1 0 tx; 0 1 ty]
void AffineTransform::LoadTranslate(float tx, float ty)
{
	values[0] = values[4] = 1.0f;
	values[1] = values[3] = 0.0f;
	values[2] = tx;
	values[5] = ty;
}

// scale(sx, sy) = [sx 0 0; 0 sy 0]
void AffineTransform::LoadScale(float sx, float sy)
{
	values[1] = values[2] = values[3] = values[5] = 0.0f;
	values[0] = sx;
	values[4] = sy;
}

// rotate(a) = [cos(a) -sin(a) 0; sin(a) cos(a) 0]
void AffineTransform::LoadRotate(float angle)
{
	float sin_a = (float)op_sin(angle);
	float cos_a = (float)op_cos(angle);

	values[0] = cos_a;
	values[1] = -sin_a;
	values[3] = sin_a;
	values[4] = cos_a;

	values[2] = values[5] = 0.0f;
}

// skew(anglex, angley) = [1 tan(anglex) 0; tan(angley) 1 0]
//
// (the way webkit seems to do it; thus:
// skewX(anglex) = skew(anglex, 0) and
// skewY(angley) = skew(0, angley))
void AffineTransform::LoadSkew(float anglex, float angley)
{
	values[1] = (float)op_tan(anglex);
	values[3] = (float)op_tan(angley);

	values[0] = values[4] = 1.0f;
	values[2] = values[5] = 0.0f;
}

//
// Decomposition as required by CSS3 transforms.
//
// Based on:
// Thomas, Spencer W., Decomposing a Matrix Into Simple Transformations, Graphics Gems II, p. 320-323
// http://tog.acm.org/GraphicsGems/gemsii/unmatrix.c
//
BOOL AffineTransform::Decompose(Decomposition& d) const
{
 	if (Determinant() == 0.0f)
 		return FALSE;

	// Get translation
	d.tx = values[2];
	d.ty = values[5];

	float r0_x = values[0];
	float r0_y = values[3];

	float r1_x = values[1];
	float r1_y = values[4];

	// Calculate X-scale
	d.sx = (float)op_sqrt(r0_x * r0_x + r0_y * r0_y);
	if (d.sx != 0.0f) // Can this actually be zero-length?
	{
		r0_x /= d.sx;
		r0_y /= d.sx;
	}

	// Calculate shear/skew
	d.shear = r0_x * r1_x + r0_y * r1_y;

	r1_x = r1_x + r0_x * (-d.shear);
	r1_y = r1_y + r0_y * (-d.shear);

	// Calculate Y-scale
	d.sy = (float)op_sqrt(r1_x * r1_x + r1_y * r1_y);
	if (d.sy != 0.0f) // Can this actually be zero-length?
	{
		r1_x /= d.sy;
		r1_y /= d.sy;
	}

	d.shear /= d.sy;

	OP_ASSERT(op_fabs(r0_x * r1_x + r0_y * r1_y) < 1e-8);

	// Need to flip?
	if (r0_x * r1_y - r0_y * r1_x < 0)
	{
		d.sx = -d.sx, d.sy = -d.sy;
		r0_x = -r0_x, r0_y = -r0_y;
		r1_x = -r1_x, r1_y = -r1_y;
	}

	// Calculate rotation
	d.rotate = (float)op_atan2(r0_y, r0_x);

 	return TRUE;
}

// Compose a matrix from a 'decomposition' (In the order: T * R * Sh * S)
void AffineTransform::Compose(const Decomposition& d)
{
	// T * R => [ 1 0 tx; 0 1 ty ] * [ c -s 0; s c 0 ] => [ c -s tx; s c ty ]
	LoadRotate(d.rotate);

	values[2] = d.tx;
	values[5] = d.ty;
 
	// (T * R) * Sh => [ a b c; d e f ] * [ 1 sh 0; 0 1 0 ] => [ a sh*a+b c; d sh*d+e f ]
	values[1] += values[0] * d.shear;
	values[4] += values[3] * d.shear;

	// (T * R * Sh) * S => [ a b c; d e f ] * [ sx 0 0; 0 sy 0 ] => [ sx*a sy*b c; sx*d sy*e f ]
	values[0] *= d.sx;
	values[3] *= d.sx;

	values[1] *= d.sy;
	values[4] *= d.sy;
}

float AffineTransform::Determinant() const
{
	return values[0] * values[4] - values[1] * values[3];
}

BOOL AffineTransform::Invert()
{
	float det = Determinant();
	if (det == 0.0f)
		return FALSE;

	AffineTransform t = *this;

	values[0] = t.values[4] / det;
	values[1] = -t.values[1] / det;
	values[2] = (t.values[1] * t.values[5] - t.values[4] * t.values[2]) / det;
	values[3] = -t.values[3] / det;
	values[4] = t.values[0] / det;
	values[5] = -(t.values[0] * t.values[5] - t.values[3] * t.values[2]) / det;

	return TRUE;
}

void AffineTransform::PostTranslate(float tx, float ty)
{
	values[2] += tx * values[0] + ty * values[1];
	values[5] += tx * values[3] + ty * values[4];
}

void AffineTransform::PostMultiply(const AffineTransform& mat)
{
	float m00 = values[0] * mat.values[0] + values[1] * mat.values[3];
	float m01 = values[0] * mat.values[1] + values[1] * mat.values[4];
	values[2] = values[0] * mat.values[2] + values[1] * mat.values[5] + values[2];

	values[0] = m00;
	values[1] = m01;

	float m10 = values[3] * mat.values[0] + values[4] * mat.values[3];
	float m11 = values[3] * mat.values[1] + values[4] * mat.values[4];
	values[5] = values[3] * mat.values[2] + values[4] * mat.values[5] + values[5];

	values[3] = m10;
	values[4] = m11;
}

void AffineTransform::TransformPoint(const OpPoint& p, float& out_x, float& out_y) const
{
	out_x = values[0] * p.x + values[1] * p.y + values[2];
	out_y = values[3] * p.x + values[4] * p.y + values[5];
}

OpPoint AffineTransform::TransformPoint(const OpPoint& p) const
{
	float vx, vy;
	TransformPoint(p, vx, vy);
	return OpPoint((int)vx, (int)vy);
}

OpRect AffineTransform::GetTransformedBBox(const OpRect& in_r) const
{
	float minx, miny;
	TransformPoint(in_r.TopLeft(), minx, miny);
	float maxx = minx, maxy = miny, x, y;
	TransformPoint(in_r.TopRight(), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);
	TransformPoint(in_r.BottomRight(), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);
	TransformPoint(in_r.BottomLeft(), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);

	OpRect out_r;
	out_r.x = (int)op_floor(minx);
	out_r.y = (int)op_floor(miny);
	out_r.width = (int)op_ceil(maxx) - out_r.x;
	out_r.height = (int)op_ceil(maxy) - out_r.y;
	return out_r;
}

RECT AffineTransform::GetTransformedBBox(const RECT& in_r) const
{
	float minx, miny;
	TransformPoint(OpPoint(in_r.left, in_r.top), minx, miny);
	float maxx = minx, maxy = miny, x, y;
	TransformPoint(OpPoint(in_r.right, in_r.top), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);
	TransformPoint(OpPoint(in_r.right, in_r.bottom), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);
	TransformPoint(OpPoint(in_r.left, in_r.bottom), x, y);
	minx = MIN(minx, x); miny = MIN(miny, y);
	maxx = MAX(maxx, x); maxy = MAX(maxy, y);

	RECT out_r;
	out_r.left = (int)op_floor(minx);
	out_r.top = (int)op_floor(miny);
	out_r.right = (int)op_ceil(maxx);
	out_r.bottom = (int)op_ceil(maxy);
	return out_r;
}

VEGATransform AffineTransform::ToVEGATransform() const
{
	VEGATransform vtrans;
	vtrans[0] = VEGA_FLTTOFIX(values[0]);
	vtrans[1] = VEGA_FLTTOFIX(values[1]);
	vtrans[2] = VEGA_FLTTOFIX(values[2]);
	vtrans[3] = VEGA_FLTTOFIX(values[3]);
	vtrans[4] = VEGA_FLTTOFIX(values[4]);
	vtrans[5] = VEGA_FLTTOFIX(values[5]);
	return vtrans;
}

// == Transform handling ===========================================

OP_STATUS VisualDeviceTransform::PushTransform(const AffineTransform& afftrans)
{
	const AffineTransform* prev = GetCurrentTransform();

	if (!transform_stack || transform_stack->count == TransformStack::BLOCK_SIZE)
	{
		// Create new block
		TransformStack* new_block = OP_NEW(TransformStack, ());
		if (!new_block)
			return OpStatus::ERR_NO_MEMORY;

		new_block->count = 0;
		new_block->next = transform_stack;

		transform_stack = new_block;
	}

	transform_stack->count++;

	AffineTransform* curr = GetCurrentTransform();
	if (prev)
	{
		*curr = *prev;
	}
	else
	{
		curr->LoadTranslate((float)translation_x, (float)translation_y);
	}

	curr->PostMultiply(afftrans);
	return OpStatus::OK;
}

void VisualDeviceTransform::PopTransform()
{
	OP_ASSERT(transform_stack); // This would indicate unbalanced Push/Pop
	if (!transform_stack)
		return;

	OP_ASSERT(transform_stack->count > 0);
	if (--transform_stack->count == 0)
	{
		TransformStack* ts = transform_stack;
		transform_stack = transform_stack->next;
		OP_DELETE(ts);
	}
}

// Base = T(offset) * T(scaled_scroll_ofs) * S(zoom) | offset_transform * T(scroll_ofs)
AffineTransform VisualDevice::GetBaseTransform()
{
	AffineTransform base;

	if (!offset_transform_is_set)
	{
		base.LoadIdentity();

		if (IsScaled())
			base[0] = base[4] = (float)scale_multiplier / scale_divider;

		base[2] = (float)(offset_x - view_x_scaled);
		base[5] = (float)(offset_y - view_y_scaled);
	}
	else
	{
		base = offset_transform;
		base.PostTranslate((float)-rendering_viewport.x, (float)-rendering_viewport.y);
	}
	return base;
}

VDStateTransformed VisualDevice::EnterTransformMode()
{
	vd_screen_ctm = GetBaseTransform();

	// Flush backgrounds
	bg_cs.FlushAll(this);

	// Invalidate current font
	logfont.SetChanged();

	return BeginTransformedPainting();
}

void VisualDevice::LeaveTransformMode()
{
	if (painter)
	{
		VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(painter);
		vpainter->ClearTransform();
	}

	// Invalidate current font
	logfont.SetChanged();

	EndTransformedPainting(untransformed_state);
}

void VisualDevice::UpdatePainterTransform(const AffineTransform& at)
{
	OP_ASSERT(painter);

	AffineTransform screen_ctm = vd_screen_ctm;
	screen_ctm.PostMultiply(at);

	VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(painter);
	vpainter->SetTransform(screen_ctm.ToVEGATransform());
}

OP_STATUS VisualDevice::PushTransform(const AffineTransform& afftrans)
{
	BOOL had_transform = HasTransform();

	RETURN_IF_ERROR(VisualDeviceTransform::PushTransform(afftrans));

	if (!had_transform)
		untransformed_state = EnterTransformMode();

	if (painter)
		UpdatePainterTransform(*GetCurrentTransform());

	return OpStatus::OK;
}

void VisualDevice::AppendTranslation(int tx, int ty)
{
	VisualDeviceTransform::AppendTranslation(tx, ty);

	if (painter)
		UpdatePainterTransform(*GetCurrentTransform());
}

void VisualDevice::PopTransform()
{
	VisualDeviceTransform::PopTransform();

	if (!HasTransform())
		LeaveTransformMode();
	else if (painter)
		UpdatePainterTransform(*GetCurrentTransform());
}
#endif // CSS_TRANSFORMS

VDCTMState VisualDeviceTransform::SaveCTM()
{
	VDCTMState saved;

	saved.translation_x = translation_x;
	saved.translation_y = translation_y;

#ifdef CSS_TRANSFORMS
	saved.transform_stack = transform_stack;
	transform_stack = NULL;
#endif // CSS_TRANSFORMS

	translation_x = 0;
	translation_y = 0;
	return saved;
}

void VisualDeviceTransform::RestoreCTM(const VDCTMState& state)
{
#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		PopTransform();
		// This assumes that you did what you should, and popped
		// everything you pushed.
		OP_ASSERT(!HasTransform());
	}

	transform_stack = state.transform_stack;
#endif // CSS_TRANSFORMS

	translation_x = state.translation_x;
	translation_y = state.translation_y;
}

VDCTMState VisualDevice::SaveCTM()
{
	VDCTMState saved;

	saved.translation_x = translation_x;
	saved.translation_y = translation_y;

#ifdef CSS_TRANSFORMS
	saved.transform_stack = transform_stack;
	transform_stack = NULL;

	if (saved.transform_stack)
		LeaveTransformMode();
#endif // CSS_TRANSFORMS

	translation_x = 0;
	translation_y = 0;
	return saved;
}

void VisualDevice::RestoreCTM(const VDCTMState& state)
{
#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		PopTransform();
		// This assumes that you did what you should, and popped
		// everything you pushed.
		OP_ASSERT(!HasTransform());
	}

	if (state.transform_stack)
	{
		EnterTransformMode();

		UpdatePainterTransform(state.transform_stack->Top());
	}

	transform_stack = state.transform_stack;
#endif // CSS_TRANSFORMS

	translation_x = state.translation_x;
	translation_y = state.translation_y;
}

BOOL VisualDeviceTransform::TestIntersection(const RECT& lrect, const OpPoint& p) const
{
	OpPoint doc_p = p;
	if (actual_viewport)
	{
		doc_p.x += actual_viewport->x;
		doc_p.y += actual_viewport->y;
	}

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return TestIntersectionTransformed(lrect, doc_p);
#endif // CSS_TRANSFORMS

	if (lrect.left != LONG_MIN && doc_p.x - translation_x < lrect.left)
		return FALSE;

	if (lrect.top != LONG_MIN && doc_p.y - translation_y < lrect.top)
		return FALSE;

	if (lrect.right != LONG_MAX && doc_p.x - translation_x >= lrect.right)
		return FALSE;

	if (lrect.bottom != LONG_MAX && doc_p.y - translation_y >= lrect.bottom)
		return FALSE;

	return TRUE;
}

BOOL VisualDeviceTransform::TestIntersection(const RECT& lrect, const RECT& rect) const
{
	RECT doc_rect = rect;
	if (actual_viewport)
	{
		doc_rect.left += actual_viewport->x;
		doc_rect.right += actual_viewport->x;
		doc_rect.top += actual_viewport->y;
		doc_rect.bottom += actual_viewport->y;
	}

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return TestIntersectionTransformed(lrect, doc_rect);
#endif // CSS_TRANSFORMS

	// Horizontal extents
	if (lrect.left != LONG_MIN &&
		doc_rect.right <= lrect.left + translation_x)
		return FALSE;
	if (lrect.right != LONG_MAX &&
		doc_rect.left >= lrect.right + translation_x)
		return FALSE;

	// Vertical extents
	if (lrect.top != LONG_MIN &&
		doc_rect.bottom <= lrect.top + translation_y)
		return FALSE;
	if (lrect.bottom != LONG_MAX &&
		doc_rect.top >= lrect.bottom + translation_y)
		return FALSE;

	return TRUE;
}

BOOL VisualDeviceTransform::TestInclusion(const OpRect& lrect, const AffinePos* transform, const OpRect& rect, OpRect* translated_lrect_ptr /*= NULL*/) const
{
	//Compute the total translation or transformation matrix from lrect to rect
	AffinePos total_transformation = GetCTM();
	if (transform)
	{
		AffinePos transform_inv = *transform;
		if (!transform_inv.Invert())
			return FALSE;
		total_transformation.Prepend(transform_inv);
	}

#ifdef CSS_TRANSFORMS
	if (total_transformation.IsTransform() && !total_transformation.GetTransform().IsNonSkewing())
		return TestInclusionTransformed(lrect, total_transformation.GetTransform(), rect);
#endif // CSS_TRANSFORMS

	// Simpler way, when we know, that transformed lrect is still a rectangle with the edges parallel to the axes
	OpRect translated_lrect = lrect;
	total_transformation.Apply(translated_lrect);

	if (translated_lrect.Contains(rect))
		return TRUE;

	if (translated_lrect_ptr)
		*translated_lrect_ptr = translated_lrect;

	return FALSE;
}

BOOL
VisualDeviceTransform::TestInclusionOfLocal(const OpRect& lrect, const AffinePos* transform, const OpRect& rect) const
{
	//Compute the total translation or transformation matrix from rect to lrect
	AffinePos total_transformation = GetCTM();

	if (!total_transformation.Invert())
		return FALSE;

	if (transform)
		total_transformation.Append(*transform);

#ifdef CSS_TRANSFORMS
	if (total_transformation.IsTransform() && !total_transformation.GetTransform().IsNonSkewing())
		return TestInclusionTransformed(rect, total_transformation.GetTransform(), lrect);
#endif // CSS_TRANSFORMS

	// Simpler way, when we know, that transformed rect is still a rectangle with the edges parallel to the axes
	OpRect translated_rect = rect;
	total_transformation.Apply(translated_rect);

	return translated_rect.Contains(lrect);
}

#ifdef CSS_TRANSFORMS
static inline BOOL VectorProductPosZTest(float ox, float oy, float dx, float dy, BOOL include_parallel)
{
	// o x d
	float vector_prod_z = ox * dy - oy * dx;
	return include_parallel ? vector_prod_z >= 0 : vector_prod_z > 0;
}

static BOOL IsRectInPosHalfSpace(float ox, float oy, float dx, float dy, const RECT& r, BOOL include_parallel)
{
	float oy_top = r.top != LONG_MIN ? r.top - oy : r.top;
	float ox_left = r.left != LONG_MIN ? r.left - ox : r.left;

	if (!VectorProductPosZTest(ox_left, oy_top, dx, dy, include_parallel))
	{
		float ox_right = r.right != LONG_MAX ? r.right - 1 - ox : r.right;

		if (!VectorProductPosZTest(ox_right, oy_top, dx, dy, include_parallel))
		{
			float oy_bottom = r.bottom != LONG_MAX ? r.bottom - 1 - oy : r.bottom;

			return VectorProductPosZTest(ox_right, oy_bottom, dx, dy, include_parallel) ||
				VectorProductPosZTest(ox_left, oy_bottom, dx, dy, include_parallel);
		}
	}

	return TRUE;
}

static BOOL IsRectFullyInPosHalfSpace(float ox, float oy, float dx, float dy, const OpRect& r, BOOL include_parallel)
{
	float oy_top = r.Top() - oy;
	float ox_left = r.Left() - ox;

	if (VectorProductPosZTest(ox_left, oy_top, dx, dy, include_parallel))
	{
		float ox_right = r.Right() - 1 - ox;

		if (VectorProductPosZTest(ox_right, oy_top, dx, dy, include_parallel))
		{
			float oy_bottom = r.Bottom() - 1 - oy;

			return VectorProductPosZTest(ox_right, oy_bottom, dx, dy, include_parallel) &&
				VectorProductPosZTest(ox_left, oy_bottom, dx, dy, include_parallel);
		}
	}

	return FALSE;
}

BOOL VisualDeviceTransform::TestIntersectionTransformed(const RECT& lrect, const OpPoint& doc_p) const
{
	const AffineTransform& aff = *GetCurrentTransform();

	float det = aff.Determinant();
	if (det == 0.0f)
		return FALSE;

	float c0_x = aff[0], c0_y = aff[3];
	float c1_x = aff[1], c1_y = aff[4];

	if (det < 0)
	{
		// Flip
		c1_x = -c1_x, c1_y = -c1_y;
		c0_x = -c0_x, c0_y = -c0_y;
	}

	if (lrect.left != LONG_MIN && !VectorProductPosZTest(doc_p.x - (lrect.left * aff[0] + aff[2]),
														 doc_p.y - (lrect.left * aff[3] + aff[5]),
														 c1_x, c1_y, TRUE))
		return FALSE;

	if (lrect.top != LONG_MIN && !VectorProductPosZTest(doc_p.x - (lrect.top * aff[1] + aff[2]),
														doc_p.y - (lrect.top * aff[4] + aff[5]),
														-c0_x, -c0_y, TRUE))
		return FALSE;

	if (lrect.right != LONG_MAX && !VectorProductPosZTest(doc_p.x - (lrect.right * aff[0] + aff[2]),
														  doc_p.y - (lrect.right * aff[3] + aff[5]),
														  -c1_x, -c1_y, FALSE))
		return FALSE;

	if (lrect.bottom != LONG_MAX && !VectorProductPosZTest(doc_p.x - (lrect.bottom * aff[1] + aff[2]),
														   doc_p.y - (lrect.bottom * aff[4] + aff[5]),
														   c0_x, c0_y, FALSE))
		return FALSE;

	return TRUE;
}

BOOL VisualDeviceTransform::TestIntersectionTransformed(const RECT& lrect, const RECT& doc_rect) const
{
	const AffineTransform& aff = *GetCurrentTransform();
	BOOL inverse_halfplanes_check;

	if (lrect.left != LONG_MIN && lrect.top != LONG_MIN &&
		lrect.right != LONG_MAX && lrect.bottom != LONG_MAX)
	{
		RECT lrect_in_doc = aff.GetTransformedBBox(lrect);

		/** First check the intersection of doc_rect against the BBox of lrect.
		If it fails we're done. If not, go to the halfplanes method (instead of checking
		the intersection of lrect against the BBox of doc_rect, which would require
		inverse calculation. */

		if (lrect_in_doc.left >= doc_rect.right || lrect_in_doc.right <= doc_rect.left ||
			lrect_in_doc.top >= doc_rect.bottom || lrect_in_doc.bottom <= doc_rect.top)
			return FALSE;

		inverse_halfplanes_check = FALSE;
	}
	else
		/** First check the doc_rect against the halfplanes derived from lrect.
			If that does not eliminate the possibility of an intersection,
			then we need to calculate the inverse and check lrect against the halfplanes derived from doc_rect. */
		inverse_halfplanes_check = TRUE;

	float det = aff.Determinant();
	if (det == 0.0f)
		return FALSE;

	float c0_x = aff[0], c0_y = aff[3];
	float c1_x = aff[1], c1_y = aff[4];

	if (det < 0)
	{
		// Flip
		c1_x = -c1_x, c1_y = -c1_y;
		c0_x = -c0_x, c0_y = -c0_y;
	}

	if (lrect.left != LONG_MIN && !IsRectInPosHalfSpace(lrect.left * aff[0] + aff[2],
														lrect.left * aff[3] + aff[5],
														c1_x, c1_y, doc_rect, TRUE))
		return FALSE;

	if (lrect.top != LONG_MIN && !IsRectInPosHalfSpace(lrect.top * aff[1] + aff[2],
													   lrect.top * aff[4] + aff[5],
													   -c0_x, -c0_y, doc_rect, TRUE))
		return FALSE;

	if (lrect.right != LONG_MAX && !IsRectInPosHalfSpace(lrect.right * aff[0] + aff[2],
														 lrect.right * aff[3] + aff[5],
														 -c1_x, -c1_y, doc_rect, FALSE))
		return FALSE;

	if (lrect.bottom != LONG_MAX && !IsRectInPosHalfSpace(lrect.bottom * aff[1] + aff[2],
														  lrect.bottom * aff[4] + aff[5],
														  c0_x, c0_y, doc_rect, FALSE))
		return FALSE;

	if (inverse_halfplanes_check)
	{
		AffineTransform inverted = aff;
		inverted.Invert();

		float c0_x = inverted[0], c0_y = inverted[3];
		float c1_x = inverted[1], c1_y = inverted[4];

		if (det < 0) //det_inv = 1 / det, keeps the sign
		{
			// Flip
			c1_x = -c1_x, c1_y = -c1_y;
			c0_x = -c0_x, c0_y = -c0_y;
		}

		if (!IsRectInPosHalfSpace(doc_rect.left * inverted[0] + inverted[2],
								  doc_rect.left * inverted[3] + inverted[5],
								  c1_x, c1_y, lrect, TRUE))
			return FALSE;

		if (!IsRectInPosHalfSpace(doc_rect.top * inverted[1] + inverted[2],
								  doc_rect.top * inverted[4] + inverted[5],
								  -c0_x, -c0_y, lrect, TRUE))
			return FALSE;

		if (!IsRectInPosHalfSpace(doc_rect.right * inverted[0] + inverted[2],
								  doc_rect.right * inverted[3] + inverted[5],
								  -c1_x, -c1_y, lrect, FALSE))
			return FALSE;

		if (!IsRectInPosHalfSpace(doc_rect.bottom * inverted[1] + inverted[2],
								  doc_rect.bottom * inverted[4] + inverted[5],
								  c0_x, c0_y, lrect, FALSE))
			return FALSE;
	}

	return TRUE;
}

/*static*/
BOOL VisualDeviceTransform::TestInclusionTransformed(const OpRect& rect1, const AffineTransform& transform, const OpRect& rect2)
{
	float det = transform.Determinant();
	if (det == 0.0f)
		return FALSE;

	float c0_x = transform[0], c0_y = transform[3];
	float c1_x = transform[1], c1_y = transform[4];

	if (det < 0)
	{
		// Flip
		c1_x = -c1_x, c1_y = -c1_y;
		c0_x = -c0_x, c0_y = -c0_y;
	}

	if (!IsRectFullyInPosHalfSpace(rect1.Left() * transform[0] + transform[2],
								   rect1.Left() * transform[3] + transform[5],
								   c1_x, c1_y, rect2, TRUE))
		return FALSE;

	if (!IsRectFullyInPosHalfSpace(rect1.Top() * transform[1] + transform[2],
								   rect1.Top() * transform[4] + transform[5],
								   -c0_x, -c0_y, rect2, TRUE))
		return FALSE;

	if (!IsRectFullyInPosHalfSpace(rect1.Right() * transform[0] + transform[2],
								   rect1.Right() * transform[3] + transform[5],
								   -c1_x, -c1_y, rect2, FALSE))
		return FALSE;

	if (!IsRectFullyInPosHalfSpace(rect1.Bottom() * transform[1] + transform[2],
								   rect1.Bottom() * transform[4] + transform[5],
								   c0_x, c0_y, rect2, FALSE))
		return FALSE;

	return TRUE;
}

#endif // CSS_TRANSFORMS
