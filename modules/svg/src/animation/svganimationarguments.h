/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_ARGUMENTS_H
#define SVG_ANIMATION_ARGUMENTS_H

#include "modules/svg/src/SVGAttribute.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/animation/svganimationtime.h"
#include "modules/svg/src/animation/svganimationattributelocation.h"
#include "modules/svg/src/animation/svganimationvaluecontext.h"
#include "modules/svg/src/SVGMotionPath.h"

class SVGAnimationTarget;
class SVGAnimationInterface;
class SVGAnimationWorkplace;
class SVGMotionPath;
class SVGRotate;

/**
 * Contains parameters that controls animation elements.
 */
class SVGAnimationArguments
{
public:
	SVGAnimationArguments() :
		from(NULL),
		to(NULL),
		by(NULL),
		values(NULL),
		key_times(NULL),
		key_splines(NULL) {
		extra.motion.path = NULL;
		extra.motion.key_points = NULL;
		extra.motion.rotate = NULL;
	}

	~SVGAnimationArguments() {
		SVGObject::DecRef(from);
		SVGObject::DecRef(to);
		SVGObject::DecRef(by);
		SVGObject::DecRef(values);
		SVGObject::DecRef(key_times);
		SVGObject::DecRef(key_splines);

		if (animation_element_type == Markup::SVGE_ANIMATEMOTION)
		{
			OP_DELETE(extra.motion.path);
			SVGObject::DecRef(extra.motion.key_points);
			SVGObject::DecRef(extra.motion.rotate);
		}
	}

	Markup::Type animation_element_type;

	SVGAnimationAttributeLocation attribute;

	SVGAnimateFillType fill_mode;
	SVGCalcMode calc_mode;
	SVGAdditiveType additive_type;
	SVGAccumulateType accumulate_type;

	void SetKeyTimes(SVGVector *kt) { SVGVector::AssignRef(key_times, kt); }
	void SetKeySplines(SVGVector *ks) { SVGVector::AssignRef(key_splines, ks); }

	void SetFrom(SVGObject *f) { SVGObject::AssignRef(from, f); }
	void SetTo(SVGObject *t) { SVGObject::AssignRef(to, t); }
	void SetBy(SVGObject *b) { SVGObject::AssignRef(by, b); }
	void SetValues(SVGVector *v) { SVGVector::AssignRef(values, v); }

	SVGVector* GetKeyTimes() const { return key_times; }
	SVGVector* GetKeySplines() const { return key_splines; }

	SVGObject* GetFrom() const { return from; }
	SVGObject* GetTo() const { return to; }
	SVGObject* GetBy() const { return by; }
	SVGVector* GetValues() const { return values; }

	void ResetMotionPath() {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATEMOTION);

		OP_DELETE(extra.motion.path);
		SVGObject::DecRef(extra.motion.key_points); extra.motion.key_points = NULL;
		SVGObject::DecRef(extra.motion.rotate); extra.motion.rotate = NULL;
		extra.motion.serial = (UINT32)-1;
	}

	UINT32 GetMotionPathSerial() {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATEMOTION);
		return extra.motion.serial;
	}

	void SetMotionPath(SVGMotionPath *motion_path) {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATEMOTION);
		OP_DELETE(extra.motion.path);
		extra.motion.path = motion_path;
	}

	SVGMotionPath* GetMotionPath() {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATEMOTION);
		return extra.motion.path;
	}

	void SetMotionPathKeyPoints(SVGVector* key_points) {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATEMOTION);
		SVGObject::DecRef(extra.motion.key_points);
		SVGObject::IncRef(extra.motion.key_points = key_points);
	}

	SVGVector* GetMotionPathKeyPoints() { return extra.motion.key_points; }

	void SetMotionPathRotate(SVGRotate* rotate) {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATEMOTION);
		SVGObject::DecRef(extra.motion.rotate);
		SVGObject::IncRef(extra.motion.rotate = rotate);
	}

	SVGRotate *GetMotionPathRotate() { return extra.motion.rotate; }

	void SetTransformType(SVGTransformType transform_type) {
		OP_ASSERT(animation_element_type == Markup::SVGE_ANIMATETRANSFORM);
		extra.transform.transform_type = transform_type;
	}

	SVGTransformType GetTransformType() { return extra.transform.transform_type; }

private:
	SVGAnimationArguments(const SVGAnimationArguments &cpy);
	SVGAnimationArguments& operator=(const SVGAnimationArguments& other);

	union {
		struct {
			SVGMotionPath* path;
			SVGVector* key_points;
			SVGRotate* rotate;
			UINT32 serial;
		} motion; /* animation_element_type == Markup::SVGE_ANIMATEMOTION */
		struct {
			SVGTransformType transform_type;
		} transform;  /* animation_element_type == Markup::SVGE_ANIMATETRANSFORM */
	} extra;

	SVGObject *from;
	SVGObject *to;
	SVGObject *by;
	SVGVector *values;

	SVGVector *key_times;
	SVGVector *key_splines;
};

#endif // !SVG_ANIMATION_ARGUMENTS_H
