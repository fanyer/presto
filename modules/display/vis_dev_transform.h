/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VISUAL_DEVICE_TRANSFORM_H
#define VISUAL_DEVICE_TRANSFORM_H

#ifdef CSS_TRANSFORMS
#include "modules/libvega/vegatransform.h"

class AffineTransform
{
public:
	/** Set to identity */
	void LoadIdentity();

	/** Set the individual values of the transform in a row major order */
	void LoadValues(float m00, float m01, float m02,
					float m10, float m11, float m12);

	/** Set the individual values of the transform in a column major order */
	void LoadValuesCM(float m00, float m10, float m01,
					  float m11, float m02, float m12)
	{
		LoadValues(m00, m01, m02, m10, m11, m12);
	}

	/** Set to translation */
	void LoadTranslate(float tx, float ty);

	/** Set to scaling-transform */
	void LoadScale(float sx, float sy);

	/** Set to rotation-transform */
	void LoadRotate(float angle);

	/** Set to skew-transform in somewhat funny way */
	void LoadSkew(float anglex, float angley);

	struct Decomposition
	{
		float tx, ty;
		float sx, sy;
		float shear;
		float rotate;
	};

	/** Decompose the matrix into translation/scale/rotation/skew
	 *	using the method referenced in the transforms spec.
	 *	@returns FALSE if matrix is singular, TRUE if successful. */
	BOOL Decompose(Decomposition& d) const;

	/** Reverse the process of Decompose() */
	void Compose(const Decomposition& d);

	/** Determinant of matrix */
	float Determinant() const;

	/** Calculate the inverse of the matrix.
	 * @returns FALSE if no inverse exists, TRUE otherwise. */
	BOOL Invert();

	/** Check if the matrix is non-singular/invertible */
	BOOL IsNonSingular() const { return Determinant() != 0.0f; }

	/** Post-multiply with the translation T(tx, ty). */
	void PostTranslate(float tx, float ty);

	/** Post-multiply with the matrix <mat> */
	void PostMultiply(const AffineTransform& mat);

	/** Apply the transformation to a point */
	void TransformPoint(const OpPoint& p, float& out_x, float& out_y) const;
	OpPoint TransformPoint(const OpPoint& p) const;

	/** Get the bounding box of the rect (in_rect) after it has been
	 *	transformed with the transform */
	OpRect GetTransformedBBox(const OpRect& in_rect) const;
	RECT GetTransformedBBox(const RECT& in_rect) const;

	VEGATransform ToVEGATransform() const;

	float operator[](int index) const { return values[index]; }
	float& operator[](int index) { return values[index]; }

	BOOL operator==(const AffineTransform &trfm) const;
	BOOL operator!=(const AffineTransform &trfm) const { return !operator==(trfm); }

#ifdef SELFTEST
	BOOL IsEqual(const AffineTransform &trfm, float eps) const;
#endif // SELFTEST

	/** TRUE when the matrix consist only of scale and translation transforms. */
	BOOL IsNonSkewing() const
	{
		return values[1] == 0.0f && values[3] == 0.0f;
	}

	/** TRUE when the matrix is the identity matrix. */
	BOOL IsIdentity() const
	{
		return values[1] == 0 && values[2] == 0 && values[3] == 0 && values[5] == 0 &&
			   values[0] == 1 && values[4] == 1;
	}

private:
	// Row major 2x3 matrix
	float values[6];
};
#endif // CSS_TRANSFORMS

class AffinePos
{
	friend class VisualDeviceTransform;
public:
	AffinePos(int tx = 0, int ty = 0) { SetTranslation(tx, ty); }
#ifdef CSS_TRANSFORMS
	AffinePos(const AffineTransform& at)
	{
		transform = at;
		is_transform = TRUE;
	}
#endif // CSS_TRANSFORMS
	AffinePos(const AffinePos& ctm)
	{
#ifdef CSS_TRANSFORMS
		is_transform = ctm.is_transform;
		if (is_transform)
			transform = ctm.transform;
		else
#endif // CSS_TRANSFORMS
		{
			translate.x = ctm.translate.x;
			translate.y = ctm.translate.y;
		}
	}

	void SetTranslation(int tx, int ty)
	{
#ifdef CSS_TRANSFORMS
		is_transform = FALSE;
#endif // CSS_TRANSFORMS
		translate.x = tx;
		translate.y = ty;
	}

	void Apply(RECT& r) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			r = transform.GetTransformedBBox(r);
		}
		else
#endif // CSS_TRANSFORMS
		{
			r.left += translate.x;
			r.right += translate.x;
			r.top += translate.y;
			r.bottom += translate.y;
		}
	}

	void Apply(OpRect& r) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			r = transform.GetTransformedBBox(r);
		}
		else
#endif // CSS_TRANSFORMS
		{
			r.x += translate.x;
			r.y += translate.y;
		}
	}

	void ApplyInverse(OpRect& r) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			AffineTransform inv = transform;
			if (inv.Invert())
			{
				r = inv.GetTransformedBBox(r);
			}
			else
			{
				r.Empty();
			}
		}
		else
#endif // CSS_TRANSFORMS
		{
			r.x -= translate.x;
			r.y -= translate.y;
		}
	}

	void Apply(OpPoint& p) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			float doc_x, doc_y;
			transform.TransformPoint(p, doc_x, doc_y);
			p.x = (int)doc_x;
			p.y = (int)doc_y;
		}
		else
#endif // CSS_TRANSFORMS
		{
			p.x += translate.x;
			p.y += translate.y;
		}
	}

	void ApplyInverse(OpPoint& p) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			AffineTransform inv = transform;
			if (inv.Invert())
			{
				float doc_x, doc_y;
				inv.TransformPoint(p, doc_x, doc_y);
				p.x = (int)doc_x;
				p.y = (int)doc_y;
			}
			else
			{
				p.x = p.y = 0;
			}
		}
		else
#endif // CSS_TRANSFORMS
		{
			p.x -= translate.x;
			p.y -= translate.y;
		}
	}

	void AppendTranslation(int tx, int ty)
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			transform.PostTranslate((float)tx, (float)ty);
			return;
		}
#endif // CSS_TRANSFORMS

		translate.x += tx;
		translate.y += ty;
	}

	void PrependTranslation(int tx, int ty)
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
		{
			AffineTransform tmp;
			tmp.LoadTranslate((float)tx, (float)ty);
			tmp.PostMultiply(transform);
			transform = tmp;
			return;
		}
#endif // CSS_TRANSFORMS

		translate.x += tx;
		translate.y += ty;
	}

#ifdef CSS_TRANSFORMS
	void AppendTransform(const AffineTransform& at)
	{
		if (!is_transform)
		{
			transform.LoadTranslate((float)translate.x, (float)translate.y);
			is_transform = TRUE;
		}

		transform.PostMultiply(at);
	}

	void PrependTransform(const AffineTransform& at)
	{
		if (!is_transform)
		{
			transform.LoadTranslate((float)translate.x, (float)translate.y);
			is_transform = TRUE;
		}

		AffineTransform tmp = at;
		tmp.PostMultiply(transform);
		transform = tmp;
	}
#endif // CSS_TRANSFORMS

	void Append(const AffinePos& ctm)
	{
#ifdef CSS_TRANSFORMS
		if (ctm.is_transform)
			AppendTransform(ctm.transform);
		else
#endif // CSS_TRANSFORMS
			AppendTranslation(ctm.translate.x, ctm.translate.y);
	}

	void Prepend(const AffinePos& ctm)
	{
#ifdef CSS_TRANSFORMS
		if (ctm.is_transform)
			PrependTransform(ctm.transform);
		else
#endif // CSS_TRANSFORMS
			PrependTranslation(ctm.translate.x, ctm.translate.y);
	}

	OpPoint GetTranslation() const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
			return OpPoint((int)transform[2], (int)transform[5]);
#endif // CSS_TRANSFORMS

		return OpPoint(translate.x, translate.y);
	}

#ifdef CSS_TRANSFORMS
	BOOL IsTransform() const { return is_transform; }

	const AffineTransform& GetTransform() const
	{
		OP_ASSERT(is_transform);
		return transform;
	}
#endif // CSS_TRANSFORMS

	BOOL Invert()
	{
#ifdef CSS_TRANSFORMS
		if (is_transform)
			return transform.Invert();
#endif // CSS_TRANSFORMS

		translate.x = -translate.x;
		translate.y = -translate.y;
		return TRUE;
	}

	BOOL operator==(const AffinePos& other) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform != other.is_transform)
			return FALSE;

		if (is_transform)
			return transform == other.transform;
#endif // CSS_TRANSFORMS

		return translate.x == other.translate.x && translate.y == other.translate.y;
	}

	BOOL operator!=(const AffinePos& other) const
	{
		return !this->operator==(other);
	}

#ifdef SELFTEST
	BOOL IsEqual(const AffinePos &other, float eps) const
	{
#ifdef CSS_TRANSFORMS
		if (is_transform != other.is_transform)
			return FALSE;

		if (is_transform)
			return transform.IsEqual(other.transform, eps);
#endif // CSS_TRANSFORMS

		return translate.x == other.translate.x && translate.y == other.translate.y;
	}
#endif // SELFTEST

private:
	union
	{
#ifdef CSS_TRANSFORMS
		AffineTransform transform;
#endif // CSS_TRANSFORMS
		struct { int x, y; } translate;
	};
#ifdef CSS_TRANSFORMS
	BOOL is_transform;
#endif // CSS_TRANSFORMS
};

#ifdef CSS_TRANSFORMS
struct TransformStack
{
	AffineTransform& Top() { return items[count - 1]; }

	enum { BLOCK_SIZE = 8 };

	AffineTransform	items[BLOCK_SIZE];
	unsigned int	count;
	TransformStack*	next;
};
#endif // CSS_TRANSFORMS

struct VDCTMState
{
#ifdef CSS_TRANSFORMS
	TransformStack* transform_stack;
#endif // CSS_TRANSFORMS

	int translation_x;
	int translation_y;
};

/**
 * Visual Device Transform handling
 *
 * This class provides the means for transform handling (be it simple
 * translations, or more complex transformations such as rotations and
 * skews).
 *
 * The intent is to provide transparent handling of the current
 * transformation matrix (CTM), during both painting (using the
 * VisualDevice) or, for instance, for hit-testing.
 */
class VisualDeviceTransform
{
public:
	VisualDeviceTransform() :
#ifdef CSS_TRANSFORMS
		transform_stack(NULL),
#endif // CSS_TRANSFORMS
		actual_viewport(NULL),
		translation_x(0), translation_y(0) {}
#ifdef CSS_TRANSFORMS
	virtual ~VisualDeviceTransform()
	{
		while (HasTransform())
			PopTransform();
	}

	virtual OP_STATUS PushTransform(const AffineTransform& afftrans);
	virtual void PopTransform();
#endif // CSS_TRANSFORMS

	AffinePos GetCTM() const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
			return AffinePos(*at);
#endif // CSS_TRANSFORMS

		return AffinePos(translation_x, translation_y);
	}

	virtual VDCTMState SaveCTM();
	virtual void RestoreCTM(const VDCTMState& state);

	OP_STATUS SetCTM(const AffinePos& ctm)
	{
#ifdef CSS_TRANSFORMS
		OP_ASSERT(!HasTransform()); // Don't do this without calling SaveCTM

		if (ctm.is_transform)
			return PushTransform(ctm.transform);
#endif // CSS_TRANSFORMS

		translation_x = ctm.translate.x;
		translation_y = ctm.translate.y;
		return OpStatus::OK;
	}

	OP_STATUS PushCTM(const AffinePos& ctm)
	{
#ifdef CSS_TRANSFORMS
		if (ctm.is_transform)
			return PushTransform(ctm.transform);
#endif // CSS_TRANSFORMS

		Translate(ctm.translate.x, ctm.translate.y);
		return OpStatus::OK;
	}

	void PopCTM(const AffinePos& ctm)
	{
#ifdef CSS_TRANSFORMS
		if (ctm.IsTransform())
		{
			PopTransform();
			return;
		}
#endif // CSS_TRANSFORMS

		Translate(-ctm.translate.x, -ctm.translate.y);
	}

	void Translate(int tx, int ty)
	{
#ifdef CSS_TRANSFORMS
		if (HasTransform())
		{
			AppendTranslation(tx, ty);
			return;
		}
#endif // CSS_TRANSFORMS

		translation_x += tx;
		translation_y += ty;
	}

	/**
	 * @deprecated Avoid using this. It is a relic from pre-transform days.
	 */
	int GetTranslationX() const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
			return (int)(*at)[2];
#endif // CSS_TRANSFORMS

		return translation_x;
	}
	/**
	 * @deprecated Avoid using this. It is a relic from pre-transform days.
	 */
	int GetTranslationY() const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
			return (int)(*at)[5];
#endif // CSS_TRANSFORMS

		return translation_y;
	}

	OpPoint ToLocal(const OpPoint& p) const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
		{
			AffineTransform inv = *at;
			if (inv.Invert())
				return inv.TransformPoint(p);

			return OpPoint();
		}
#endif // CSS_TRANSFORMS

		return p - OpPoint(translation_x, translation_y);
	}

	OpPoint ToDocument(const OpPoint& p) const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
		{
			float doc_x, doc_y;
			at->TransformPoint(p, doc_x, doc_y);
			return OpPoint((int)doc_x, (int)doc_y);
		}
#endif // CSS_TRANSFORMS

		return OpPoint(p.x + translation_x, p.y + translation_y);
	}

	OpRect ToBBox(const OpRect& r) const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
			return at->GetTransformedBBox(r);
#endif // CSS_TRANSFORMS

		return OpRect(r.x + translation_x, r.y + translation_y, r.width, r.height);
	}

	RECT ToBBox(const RECT& r) const
	{
#ifdef CSS_TRANSFORMS
		if (AffineTransform* at = GetCurrentTransform())
			return at->GetTransformedBBox(r);
#endif // CSS_TRANSFORMS

		RECT out_r;
		out_r.left = r.left + translation_x;
		out_r.right = r.right + translation_x;
		out_r.top = r.top + translation_y;
		out_r.bottom = r.bottom + translation_y;
		return out_r;
	}

	/** Test intersection between a rect in the local coordinate space
		against a point in the 'global' space.

		If a viewport is available (pointed to by actual_viewport), p
		is assumed to be in viewport coordinates (this is currently
		only the case for VisualDevice). */
	BOOL TestIntersection(const RECT& local_rect, const OpPoint& p) const;

	/** Test intersection between a rect in the local coordinate space
		against a rect in the 'global' space. Both rects are assumed to be not empty.

		If a viewport is available (pointed to by actual_viewport), rect
		is assumed to be in viewport coordinates (this is currently
		only the case for VisualDevice).
	 *  @param local_rect May have infinite extents. */
	BOOL TestIntersection(const RECT& local_rect, const RECT& rect) const;

	/** Test whether the rect in local coordinate space fully contains the rect, that can be transformed to document root coordinates
	 *	by transform. In order to transform the lrect into rect, lrect has to be transformed
	 *  by the current translation/transformation state and then by the inverse of transform.
	 *  @param transform the transform or translation, that describes the rect in document coordinates.
	 *		   May be NULL, then rect is assumed to be in document coordinates.
	 *	@param[out] translated_lrect_prt an optional pointer. When the AffinePos that translates/transforms from local
	 *				coordinate system to the coordinate system of the rect is a non skewing transform (the translated rect is still
	 *				a rectangle with the edges parallel to the axes) and the method returns FALSE,
	 *				the translated_lrect_ptr is used to assign the translated lrect to the rect's coordinates. */
	BOOL TestInclusion(const OpRect& lrect, const AffinePos* transform, const OpRect& rect, OpRect* translated_lrect_ptr = NULL) const;

	/** Similar to the above, but tests whether the rect contains lrect. */
	BOOL TestInclusionOfLocal(const OpRect& lrect, const AffinePos* transform, const OpRect& rect) const;

#ifdef CSS_TRANSFORMS
	BOOL HasTransform() const { return !!transform_stack; }
#endif // CSS_TRANSFORMS

protected:
#ifdef CSS_TRANSFORMS

	/** Below methods test the intersection between a rect in the local coordinate space
		against a rect or a point in the 'global' space. The rects are assumed to be not empty. */

	BOOL TestIntersectionTransformed(const RECT& lrect, const OpPoint& p) const;

	/**
	 * @param lrect May have infinite extents. */
	BOOL TestIntersectionTransformed(const RECT& lrect, const RECT& rect) const;

	/** Checks whether rect2 is fully inside rect1.
	 * @param transform a transformation matrix that describes rect1 in rect2 coords. */
	static BOOL TestInclusionTransformed(const OpRect& rect1, const AffineTransform& transform, const OpRect& rect2);

	virtual void AppendTranslation(int tx, int ty)
	{
		transform_stack->Top().PostTranslate((float)tx, (float)ty);
	}

	AffineTransform* GetCurrentTransform() const
	{
		return transform_stack ? &transform_stack->Top() : NULL;
	}

	TransformStack* transform_stack;
#endif // CSS_TRANSFORMS

	const OpRect*	actual_viewport;

	/** Offset for painting in doc coordinates. */
	int				translation_x;
	int				translation_y;
};

#endif // VISUAL_DEVICE_TRANSFORM_H
