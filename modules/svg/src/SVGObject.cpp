/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGTimeValue.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/SVGKeySpline.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/animation/svganimationtime.h"

SVGObject::SVGObject(SVGObjectType type)
	: m_refcount(0),
	  m_object_info_packed_init(0x0)
{
	m_object_info_packed.type = type;
}

OP_STATUS
SVGObject::GetStringRepresentation(TempBuffer* buffer) const
{
	if(Flag(SVGOBJECTFLAG_INHERIT))
	{
		return buffer->Append("inherit");
	}
	else
	{
		return LowLevelGetStringRepresentation(buffer);
	}
}

/*
 * This function shouldn't be used for creating vectors. Use
 * SVGUtils::CreateSVGVector instead.
 *
 * Objects that are animatable must be creatable here, with the
 * exception for vectors that have different sub-types in different
 * attributes.
 *
 */
/* static */ OP_STATUS
SVGObject::Make(SVGObject*& obj, SVGObjectType type)
{
	switch(type)
	{
		case SVGOBJECT_NUMBER:
			obj = OP_NEW(SVGNumberObject, ());
			break;
		case SVGOBJECT_POINT:
			obj = OP_NEW(SVGPoint, ());
			break;
		case SVGOBJECT_LENGTH:
			obj = OP_NEW(SVGLengthObject, ());
			break;
		case SVGOBJECT_ENUM:
			obj = OP_NEW(SVGEnum, (SVGENUM_UNKNOWN));
			break;
		case SVGOBJECT_COLOR:
			obj = OP_NEW(SVGColorObject, ());
			break;
		case SVGOBJECT_PATH:
			{
				OpBpath* path;
				OP_STATUS err = OpBpath::Make(path);
				if(OpStatus::IsSuccess(err))
					obj = path;
				return err;
			}
			break;
		case SVGOBJECT_VECTOR:
			OP_ASSERT(!"Use SVGUtils::CreateSVGVector() with friends instead");
			obj = NULL;
			return OpStatus::ERR;
		case SVGOBJECT_RECT:
			obj = OP_NEW(SVGRectObject, ());
			break;
		case SVGOBJECT_PAINT:
			obj = OP_NEW(SVGPaintObject, (SVGPaint::RGBCOLOR));
			break;
		case SVGOBJECT_TRANSFORM:
			obj = OP_NEW(SVGTransform, (SVGTRANSFORM_TRANSLATE));
			break;
		case SVGOBJECT_MATRIX:
			obj = OP_NEW(SVGMatrixObject, ());
			break;
		case SVGOBJECT_STRING:
			obj = OP_NEW(SVGString, ());
			break;
		case SVGOBJECT_FONTSIZE:
			obj = OP_NEW(SVGFontSizeObject, ());
			break;
		case SVGOBJECT_ROTATE:
			obj = OP_NEW(SVGRotate, ()); // hmm, this gives UNKNOWN type
			break;
		case SVGOBJECT_ORIENT:
		{
			SVGOrient* orient;
			RETURN_IF_ERROR(SVGOrient::Make(orient, SVGORIENT_ANGLE));
			obj = orient;
		}
		break;
		case SVGOBJECT_ANIMATION_TIME:
			obj = OP_NEW(SVGAnimationTimeObject, (SVGAnimationTime::Unresolved()));
			break;
		case SVGOBJECT_ASPECT_RATIO:
			obj = OP_NEW(SVGAspectRatio, ());
			break;
#ifdef SVG_SUPPORT_FILTERS
		case SVGOBJECT_ENABLE_BACKGROUND:
			obj = OP_NEW(SVGEnableBackgroundObject, ());
			break;
#endif // SVG_SUPPORT_FILTERS
		case SVGOBJECT_URL:
			obj = OP_NEW(SVGURL, (UNI_L(""), 0, URL()));
			break;
		case SVGOBJECT_BASELINE_SHIFT:
			obj = OP_NEW(SVGBaselineShiftObject, ());
			break;
		case SVGOBJECT_CLASSOBJECT:
			obj = OP_NEW(SVGClassObject, ());
			break;
		// The following objects are non-animatable
		case SVGOBJECT_REPEAT_COUNT:
		case SVGOBJECT_TIME:
		case SVGOBJECT_UNKNOWN:
		case SVGOBJECT_KEYSPLINE:
		default:
			obj = NULL;
			return OpSVGStatus::TYPE_ERROR;
	}

	return obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* static */ SVGObject*
SVGObject::IncRef(SVGObject* obj)
{
	if (obj && !obj->m_object_info_packed.protect)
	{
		++obj->m_refcount;

#ifdef _DEBUG
		g_svg_manager_impl->svg_object_refcount++;
#endif // _DEBUG
	}

	return obj;
}

/* static */ void
SVGObject::DecRef(SVGObject* obj)
{
#ifdef _DEBUG
	if (obj)
	{
		// NULL if the svg module has been destroyed
		if (g_svg_manager_impl)
			g_svg_manager_impl->svg_object_refcount--;
		OP_ASSERT(obj->m_refcount >= 0);
	}
#endif // _DEBUG

	if (obj && !obj->m_object_info_packed.protect && --obj->m_refcount == 0)
		OP_DELETE(obj);
}

/* static */ SVGObject*
SVGObject::Protect(SVGObject* obj)
{
	if (obj)
		obj->m_object_info_packed.protect = 1;

	return obj;
}

/* static */ void
SVGObject::AssignRef(SVGObject *&to, SVGObject *from)
{
	if (to != from)
	{
		DecRef(to);
		IncRef(to = from);
	}
}

#endif // SVG_SUPPORT
