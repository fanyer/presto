/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_VALUE_H
#define SVG_ANIMATION_VALUE_H

class SVGLength;
class SVGFontSize;
class SVGBaselineShift;

#include "modules/svg/src/animation/svganimationinterval.h"
#include "modules/svg/src/animation/svganimationvaluecontext.h"

class SVGPoint;
class SVGRect;
class SVGEnum;
class SVGTransform;
class SVGVector;
class SVGMatrix;
class OpBpath;
class SVGString;
class SVGURL;
class SVGNavRef;
class SVGOrient;
class SVGAspectRatio;
class SVGClassObject;
class SVGDocumentContext;
class HTMLayoutProperties;
class HTML_Element;

class SVGAnimationValue
{
public:
    enum ExtraOperation {
		EXTRA_OPERATION_NOOP,
		EXTRA_OPERATION_TREAT_TO_AS_BY
    };

	/**
	 * This enum is used to index an array:
	 * s_reference_to_object_type. These two has to stay in sync.
	 */
    enum ReferenceType
    {
		REFERENCE_SVG_NUMBER = 0,
		REFERENCE_SVG_LENGTH,
		REFERENCE_SVG_STRING,
		REFERENCE_SVG_BASELINE_SHIFT,
		REFERENCE_SVG_COLOR,
		REFERENCE_SVG_PAINT,
		REFERENCE_SVG_FONT_SIZE,
		REFERENCE_SVG_POINT,
		REFERENCE_SVG_RECT,
		REFERENCE_SVG_VECTOR,
		REFERENCE_SVG_OPBPATH,
		REFERENCE_SVG_MATRIX,
		REFERENCE_SVG_TRANSFORM,
		REFERENCE_SVG_ENUM,
		REFERENCE_SVG_URL,
		REFERENCE_SVG_ASPECT_RATIO,
		REFERENCE_SVG_NAVREF,
		REFERENCE_SVG_ORIENT,
		REFERENCE_SVG_CLASSOBJECT,

		REFERENCE_EMPTY
    };

    /* These are the types we can reference. That type of value we
	 * reference depend on (SVGObject vs non-SVGObject):
	 *
	 * Does the value exist in the layout props? If yes, we have the
	 * same type here.
	 */
    union
    {
		SVGNumber *svg_number;
		SVGLength *svg_length;
		SVGString *svg_string;
		SVGBaselineShift *svg_baseline_shift;
		SVGColor *svg_color;
		SVGPaint *svg_paint;
		SVGFontSize *svg_font_size;
		SVGPoint *svg_point;
		SVGRect *svg_rect;
		SVGVector *svg_vector;
		OpBpath *svg_path;
		SVGMatrix *svg_matrix;
		SVGTransform *svg_transform;
		SVGEnum *svg_enum;
		SVGURL *svg_url;
		SVGAspectRatio *svg_aspect_ratio;
		SVGNavRef *svg_nav_ref;
		SVGOrient *svg_orient;
		SVGClassObject *svg_classobject;
    } reference;

    ReferenceType reference_type;

    enum ValueType
    {
		VALUE_NUMBER,
		VALUE_PERCENTAGE,
		VALUE_COLOR,
		VALUE_EMPTY
    };

    /* These are the types we can manipulate internally. These are
	 * transfered back to the reference type using the Transfer()
	 * function.*/
    union
    {
		float number;
		UINT32 color;
    } value;

    ValueType value_type;

	static SVGAnimationValue Empty();
	static BOOL IsEmpty(SVGAnimationValue &animation_value);

    static OP_STATUS Interpolate(SVGAnimationValueContext &context,
								 SVG_ANIMATION_INTERVAL_POSITION interval_position,
								 const SVGAnimationValue &from_value,
								 const SVGAnimationValue &to_value,
								 ExtraOperation extra_operation,
								 SVGAnimationValue &animation_value);

    static float CalculateDistance(SVGAnimationValueContext &context,
								   const SVGAnimationValue &from_value,
								   const SVGAnimationValue &to_value);

    static OP_STATUS Assign(SVGAnimationValueContext &context,
							const SVGAnimationValue &rvalue, SVGAnimationValue &lvalue);

    static SVGObjectType TranslateToSVGObjectType(ReferenceType reference_type);

    static OP_STATUS AddBasevalue(const SVGAnimationValue &base_value,
								  SVGAnimationValue &animation_value);

    static OP_STATUS AddAccumulationValue(const SVGAnimationValue &accumulation_value_1,
										  const SVGAnimationValue &accumulation_value_2,
										  SVG_ANIMATION_INTERVAL_REPETITION repetition,
										  SVGAnimationValue &animation_value);

    static BOOL Initialize(SVGAnimationValue &animation_value, SVGObject *svg_object, SVGAnimationValueContext &context);

    static float ResolveLength(float value, int css_unit, SVGAnimationValueContext& context);

	static void SetCSSProperty(SVGAnimationAttributeLocation &location,
							   SVGAnimationValue &animation_value,
							   SVGAnimationValueContext &context);

protected:

    static OP_STATUS AssignSVGPaints(const SVGPaint &rvalue,
									 SVGPaint& lvalue) { return lvalue.Copy(rvalue); }

    static void AssignSVGColors(const SVGColor &rvalue,
								SVGColor& lvalue) { lvalue.Copy(rvalue); }

    static void AssignSVGBaselineShifts(const SVGBaselineShift &rvalue,
										SVGBaselineShift& lvalue) { lvalue = rvalue; }

    static UINT32 InterpolateColors(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									UINT32 from, UINT32 to, ExtraOperation extra_operation);

    static UINT32 AddBasevalueColor(UINT32 base, UINT32 anim);

    static UINT32 AddAccumulationValueColor(UINT32 accum,
											UINT32 base,
											SVG_ANIMATION_INTERVAL_REPETITION interval_repetition,
											UINT32 anim);

    static float CalculateDistanceColors(UINT32 from, UINT32 to);

    static float CalculateDistanceSVGVector(SVGAnimationValueContext &context, const SVGVector &from, const SVGVector &to);
    static float CalculateDistanceSVGPoint(SVGAnimationValueContext &context, const SVGPoint &from, const SVGPoint &to);
	static float CalculateDistanceSVGTransform(const SVGTransform &from,
											   const SVGTransform &to);
	static float CalculateDistanceSVGPath(const OpBpath &from,
										  const OpBpath &to);

    static void Setup(SVGAnimationValue &animation_value, SVGAnimationValueContext &context);
    static OP_STATUS Transfer(SVGAnimationValue &value);

	static void InterpolateSVGPoints(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									 SVGPoint *from_point,
									 SVGPoint *to_point,
									 ExtraOperation extra_operation,
									 SVGPoint *target_point);

	static void InterpolateSVGRects(SVG_ANIMATION_INTERVAL_POSITION interval_position,
									SVGRect *from_rect,
									SVGRect *to_rect,
									ExtraOperation extra_operation,
									SVGRect *target_rect);

	static OP_STATUS InterpolateSVGVectors(SVGAnimationValueContext &context,
										   SVG_ANIMATION_INTERVAL_POSITION interval_position,
										   SVGVector *from_vector,
										   SVGVector *to_vector,
										   ExtraOperation extra_operation,
										   SVGVector *target_vector);

	static OP_STATUS InterpolateSVGOpBpaths(SVG_ANIMATION_INTERVAL_POSITION interval_position,
											OpBpath *from_path,
											OpBpath *to_path,
											ExtraOperation extra_operation,
											OpBpath *target_path);

	static OP_STATUS InterpolateSVGTransforms(SVG_ANIMATION_INTERVAL_POSITION interval_position,
											  const SVGTransform &from_transform,
											  const SVGTransform &to_transform,
											  ExtraOperation extra_operation,
											  SVGTransform &target_transform);

	static void SetAnimationValueFromLength(SVGAnimationValue &animation_value,
											SVGAnimationValueContext &context,
											SVGLength &length);

};

class SVGObjectContainer
{
public:
    SVGObjectContainer() : object(NULL) {}
    ~SVGObjectContainer() { SVGObject::DecRef(object); }

	void Set(SVGObject *o) { SVGObject::DecRef(object); object = o; SVGObject::IncRef(object); }

private:
    SVGObject *object;
};

#endif // !SVG_ANIMATION_VALUE_H
