/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGUtils.h"

#include "modules/svg/src/animation/svganimationvalue.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/util/tempbuf.h"

#include "modules/logdoc/xmlenum.h" // For XMLA_BASE

#include "modules/logdoc/class_attribute.h"


/* SVGNumberObject ***********************
 */

SVGNumberObject::SVGNumberObject(SVGNumber value) :
		SVGObject(SVGOBJECT_NUMBER),
		value(value)
{
}

/* virtual */ BOOL
SVGNumberObject::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_NUMBER)
	{
		return (value == static_cast<const SVGNumberObject&>(obj).value);
	}
	return FALSE;
}

/* virtual */
OP_STATUS
SVGNumberObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return buffer->AppendDoubleToPrecision(value.GetFloatValue());
}

/* virtual */ BOOL
SVGNumberObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_NUMBER;
	animation_value.reference.svg_number = &value;
	return TRUE;
}

/* SVGEnum ***********************
 */

/* virtual */
BOOL
SVGEnum::IsEqual(const SVGObject& other) const
{
	if(other.IsThisType(this))
	{
		const SVGEnum& o = (const SVGEnum&)other;

		if(o.EnumType() == EnumType())
		{
			// Are they both 'inherit'?
			if (o.Flag(SVGOBJECTFLAG_INHERIT) && Flag(SVGOBJECTFLAG_INHERIT))
				return TRUE;

			// Is one 'inherit' but not the other?
			if (o.Flag(SVGOBJECTFLAG_INHERIT) != Flag(SVGOBJECTFLAG_INHERIT))
				return FALSE;

			// special case display
			if (EnumType() == SVGENUM_DISPLAY &&
				(EnumValue() != SVGDISPLAY_NONE && o.EnumValue() != SVGDISPLAY_NONE))
			{
				// the values are relatively equal, meaning
				// that they will be displayed the same
				return TRUE;
			}
			return(o.EnumValue() == EnumValue());
		}
	}

	return FALSE;
}

/* virtual */
OP_STATUS
SVGEnum::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return SVGUtils::GetEnumValueStringRepresentation(m_enum_type, m_enum_value, buffer);
}

/* virtual */ BOOL
SVGEnum::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_ENUM;
	animation_value.reference.svg_enum = this;
	return TRUE;
}


/* virtual */ SVGObject *
SVGEnum::Clone() const
{
	SVGEnum *enum_value = OP_NEW(SVGEnum, (m_enum_type));
	if (enum_value)
		enum_value->Copy(*this);
	return enum_value;
}

void
SVGEnum::Copy(const SVGEnum& other)
{
	CopyFlags(other);
	m_enum_type = other.m_enum_type; m_enum_value = other.m_enum_value;
}

/* SVGColorObject ***********************
 */

/* virtual */ BOOL
SVGColorObject::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_COLOR)
	{
		const SVGColorObject& other =
			static_cast<const SVGColorObject&>(obj);
		return m_color == other.m_color;
	}

	return FALSE;
}

/* virtual */
OP_STATUS
SVGColorObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return m_color.GetStringRepresentation(buffer);
}

/* virtual */ BOOL
SVGColorObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_COLOR;
	animation_value.reference.svg_color = &m_color;
	return TRUE;
}

/** SVGAngle **********************
 */

void
SVGAngle::SetAngle(SVGNumber angle, SVGAngleType unit)
{
	m_angle = angle;
	m_unit = unit;
}

SVGNumber
SVGAngle::GetAngleInUnits(SVGAngleType desiredunit) const
{
	// Identity
	if (m_unit == desiredunit ||
		(m_unit == SVGANGLE_UNSPECIFIED && desiredunit == SVGANGLE_DEG) ||
		(m_unit == SVGANGLE_DEG && desiredunit == SVGANGLE_UNSPECIFIED))
		return m_angle;

	// Degrees => ...
	if (m_unit == SVGANGLE_DEG || m_unit == SVGANGLE_UNSPECIFIED)
	{
		if (desiredunit == SVGANGLE_RAD)
			return m_angle * SVGNumber::pi() / 180;
		else if (desiredunit == SVGANGLE_GRAD)
			return (m_angle * 10) / 9;
	}

	// Radians => ...
	if (m_unit == SVGANGLE_RAD)
	{
		if (desiredunit == SVGANGLE_DEG || desiredunit == SVGANGLE_UNSPECIFIED)
			return (m_angle * 180) / SVGNumber::pi();
		else if (desiredunit == SVGANGLE_GRAD)
			return (m_angle * 200) / SVGNumber::pi();
	}

	// Grad => ...
	if (m_unit == SVGANGLE_GRAD)
	{
		if (desiredunit == SVGANGLE_DEG || desiredunit == SVGANGLE_UNSPECIFIED)
			return (m_angle * 9) / 10;
		else if (desiredunit == SVGANGLE_RAD)
			return m_angle * SVGNumber::pi() / 200;
	}

	OP_ASSERT(!"Failed to convert angle to desired unit"); // This should not happen
	return m_angle;
}

/* virtual */ OP_STATUS
SVGAngle::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(m_angle.GetFloatValue()));

	const char* unit_str = NULL;
	switch(m_unit)
	{
	case SVGANGLE_DEG: unit_str = "deg"; break;
	case SVGANGLE_RAD: unit_str = "rad"; break;
	case SVGANGLE_GRAD: unit_str = "grad"; break;
	}

	if (unit_str)
		return buffer->Append(unit_str);
	return OpStatus::OK;
}

void
SVGAngle::Copy(const SVGAngle& angle)
{
	SVGObject::CopyFlags(angle);

	m_unit = angle.m_unit;
	m_angle = angle.m_angle;
}

/* virtual */ BOOL
SVGAngle::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_ANGLE)
	{
		const SVGAngle &other_angle = static_cast<const SVGAngle &>(other);
		return (other_angle.m_unit == m_unit && other_angle.m_angle == m_angle);
	}
	else
	{
		return FALSE;
	}
}

/* static */
unsigned SVGAngle::QuantizeAngle90Deg(float angle)
{
	if (angle < 45)
		return 0;
	if (angle < 135)
		return 1;
	if (angle < 225)
		return 2;

	return 3;
}

/** SVGRotate **********************
 */

SVGRotate::SVGRotate(SVGRotateType t /* = SVGROTATE_UNKNOWN */) :
		SVGObject(SVGOBJECT_ROTATE),
		m_type(t),
		m_angle(NULL)
{
}

SVGRotate::SVGRotate(SVGAngle* a) :
		SVGObject(SVGOBJECT_ROTATE),
		m_type(SVGROTATE_ANGLE),
		m_angle(a)
{
	SVGObject::IncRef(m_angle);
}

/* virtual */
SVGRotate::~SVGRotate()
{
	SVGObject::DecRef(m_angle);
}

/* virtual */ SVGObject *
SVGRotate::Clone() const
{
	SVGRotate *rotate_object = OP_NEW(SVGRotate, (m_type));
	if(!rotate_object)
		return NULL;

	rotate_object->CopyFlags(*this);

	if(m_angle)
	{
		SVGAngle *angle_object = OP_NEW(SVGAngle, ());
		if(!angle_object)
		{
			OP_DELETE(rotate_object);
			return NULL;
		}

		angle_object->Copy(*m_angle);
		rotate_object->m_angle = angle_object;
	}

	return rotate_object;
}

/* virtual */ OP_STATUS
SVGRotate::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	switch(m_type)
	{
		case SVGROTATE_AUTO:
			return buffer->Append("auto");
		case SVGROTATE_AUTOREVERSE:
			return buffer->Append("auto-reverse");
		case SVGROTATE_ANGLE:
			if(m_angle)
			{
				return m_angle->LowLevelGetStringRepresentation(buffer);
			}
	}

	OP_ASSERT(!"Unexpected rotate type");
	return OpStatus::ERR;
}

/* virtual */ BOOL
SVGRotate::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_ROTATE)
	{
		const SVGRotate &other_rotate = static_cast<const SVGRotate &>(other);
		if (other_rotate.m_type != m_type)
			return FALSE;

		if (m_type != SVGROTATE_ANGLE)
		{
			return TRUE;
		}
		else if (other_rotate.m_angle && m_angle)
		{
			return m_angle->IsEqual(*other_rotate.m_angle);
		}
	}

	return FALSE;
}

/** SVGOrient **********************
 */

SVGOrient::SVGOrient(SVGAngle* a, SVGEnum* e) :
		SVGObject(SVGOBJECT_ORIENT),
		m_type(e),
		m_angle(a)
{
	SVGObject::IncRef(m_type);
	SVGObject::IncRef(m_angle);
}

SVGOrient::~SVGOrient()
{
	SVGObject::DecRef(m_type);
	SVGObject::DecRef(m_angle);
}

/* static */
OP_STATUS
SVGOrient::Make(SVGOrient*& orient, SVGOrientType t, SVGAngle* a /* = NULL */)
{
	SVGEnum* e_val = OP_NEW(SVGEnum, (SVGENUM_UNKNOWN, t));
	if (!e_val)
		return OpStatus::ERR_NO_MEMORY;

	if (t == SVGORIENT_ANGLE && a == NULL)
	{
		a = OP_NEW(SVGAngle, ());
		if (!a)
		{
			OP_DELETE(e_val);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	orient = OP_NEW(SVGOrient, (a, e_val));
	if (!orient)
	{
		OP_DELETE(e_val);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */ SVGObject *
SVGOrient::Clone() const
{
	SVGAngle* angle_object = NULL;
	if (m_angle)
	{
		angle_object = static_cast<SVGAngle*>(m_angle->Clone());
		if (!angle_object)
			return NULL;
	}

	SVGEnum* type_object = static_cast<SVGEnum*>(m_type->Clone());
	if (!type_object)
	{
		OP_DELETE(angle_object);
		return NULL;
	}

	SVGOrient* orient_object = OP_NEW(SVGOrient, (angle_object, type_object));
	if (!orient_object)
	{
		OP_DELETE(angle_object);
		OP_DELETE(type_object);
		return NULL;
	}

	orient_object->CopyFlags(*this);
	return orient_object;
}

/* virtual */ OP_STATUS
SVGOrient::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	switch(GetOrientType())
	{
	case SVGORIENT_AUTO:
		return buffer->Append("auto");
	case SVGORIENT_ANGLE:
		if (m_angle)
		{
			return m_angle->LowLevelGetStringRepresentation(buffer);
		}
		return buffer->Append("0");
	}

	OP_ASSERT(!"Unexpected orient type");
	return OpStatus::ERR;
}

OP_STATUS
SVGOrient::SetAngle(const SVGAngle& angle)
{
	if (m_angle)
	{
		m_angle->Copy(angle);
	}
	else
	{
		m_angle  = static_cast<SVGAngle *>(angle.Clone());
		if (!m_angle)
			return OpStatus::ERR_NO_MEMORY;
		SVGObject::IncRef(m_angle);
	}

	return OpStatus::OK;
}

OP_STATUS
SVGOrient::SetToAngleInDeg(SVGNumber deg)
{
	if (!m_angle)
	{
		m_angle = OP_NEW(SVGAngle, ());
		if (!m_angle)
			return OpStatus::ERR_NO_MEMORY;
		SVGObject::IncRef(m_angle);
	}

	m_angle->SetAngle(deg, SVGANGLE_DEG);
	SetOrientType(SVGORIENT_ANGLE);
	return OpStatus::OK;
}

/* virtual */ BOOL
SVGOrient::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_ORIENT)
	{
		const SVGOrient &other_orient = static_cast<const SVGOrient &>(other);

		if (GetOrientType() != other_orient.GetOrientType())
			return FALSE;

		if (GetOrientType() != SVGORIENT_ANGLE)
		{
			return TRUE;
		}
		else if (m_angle && other_orient.m_angle)
		{
			return m_angle->IsEqual(*other_orient.m_angle);
		}
	}

	return FALSE;
}

/* virtual */ BOOL
SVGOrient::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_ORIENT;
	animation_value.reference.svg_orient = this;
	return TRUE;
}

OP_STATUS
SVGOrient::Copy(const SVGOrient &other)
{
	SVGObject::CopyFlags(other);

	if(other.GetAngle())
	{
		if (!m_angle)
		{
			m_angle = OP_NEW(SVGAngle, ());
			if (!m_angle)
				return OpStatus::ERR_NO_MEMORY;
			SVGObject::IncRef(m_angle);
		}
		m_angle->Copy(*other.GetAngle());
	}
	else
	{
		SVGObject::DecRef(m_angle);
		m_angle = NULL;
	}

	m_type->SetEnumValue(other.GetOrientType());
	return OpStatus::OK;
}

/** SVGString **********************
 */

/* virtual */ BOOL
SVGString::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_STRING)
	{
		const SVGString& other = static_cast<const SVGString&>(obj);
		if (other.GetLength() == GetLength() &&
			(GetLength() == 0 ||
			 uni_strcmp(other.GetString(),GetString()) == 0))
		{
			return TRUE;
		}
	}
	return FALSE;
}

OP_STATUS
SVGString::SetString(const uni_char* str, unsigned str_length)
{
	OP_DELETEA(m_value);
	m_value = UniSetNewStrN(str, str_length);
	if (!m_value)
	{
		m_strlen = 0;
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		m_strlen = str_length;
		return OpStatus::OK;
	}
}

/* virtual */ SVGObject *
SVGString::Clone() const
{
	SVGString* string_object = OP_NEW(SVGString, ());
	if(!string_object)
		return NULL;

	string_object->CopyFlags(*this);

	if (m_value && OpStatus::IsMemoryError(string_object->SetString(m_value, m_strlen)))
	{
		OP_DELETE(string_object);
		return NULL;
	}
	return string_object;
}

/* virtual */ OP_STATUS
SVGString::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return buffer->Append(m_value, m_strlen);
}

/* virtual */ BOOL
SVGString::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_STRING;
	animation_value.reference.svg_string = this;
	return TRUE;
}

/* SVGAspectRatio **********************
 */

void
SVGAspectRatio::Copy(const SVGAspectRatio& src)
{
	SVGObject::CopyFlags(src);

	align = src.align;
	mos = src.mos;
	defer = src.defer;
}

/* virtual */ OP_STATUS
SVGAspectRatio::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	if(defer)
	{
		RETURN_IF_ERROR(buffer->Append("defer "));
	}
	const char* align_str;
	switch(align)
	{
	case SVGALIGN_NONE: align_str = "none"; break;
	case SVGALIGN_XMINYMIN: align_str = "xMinYMin"; break;
	case SVGALIGN_XMIDYMIN: align_str = "xMidYMin"; break;
	case SVGALIGN_XMAXYMIN: align_str = "xMaxYMin"; break;
	case SVGALIGN_XMINYMID: align_str = "xMinYMid"; break;
	case SVGALIGN_XMIDYMID: align_str = "xMidYMid"; break;
	case SVGALIGN_XMAXYMID: align_str = "xMaxYMid"; break;
	case SVGALIGN_XMINYMAX: align_str = "xMinYMax"; break;
	case SVGALIGN_XMIDYMAX: align_str = "xMidYMax"; break;
	case SVGALIGN_XMAXYMAX: align_str = "xMaxYMax"; break;
	default:
		OP_ASSERT(!"Unexpected type for alignment");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(buffer->Append(align_str));

	if (mos == SVGMOS_SLICE)
	{
		RETURN_IF_ERROR(buffer->Append(" slice"));
	}

	return OpStatus::OK;
}

/* virtual */ BOOL
SVGAspectRatio::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_ASPECT_RATIO)
	{
		const SVGAspectRatio &other_aspect_ratio = static_cast<const SVGAspectRatio &>(other);
		return
			other_aspect_ratio.align == align &&
			other_aspect_ratio.mos == mos &&
			other_aspect_ratio.defer == defer;
	}
	else
	{
		return FALSE;
	}
}

/* virtual */ BOOL
SVGAspectRatio::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_ASPECT_RATIO;
	animation_value.reference.svg_aspect_ratio = this;
	return TRUE;
}

/* SVGURL **********************
 */

/* virtual */ BOOL
SVGURL::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_URL)
	{
		const SVGURL& other = static_cast<const SVGURL&>(obj);
		return m_attr_string.Compare(other.m_attr_string) == 0;
	}
	return FALSE;
}

URL
SVGURL::GetURL(URL root_url, HTML_Element* elm)
{
	URL real_root_url = root_url.GetAttribute(URL::KMovedToURL, TRUE);
	URL base_url = SVGUtils::ResolveBaseURL(!real_root_url.IsEmpty() ? real_root_url : root_url, elm);
	OP_ASSERT(!base_url.IsEmpty());
	URLType base_url_type = base_url.Type();

	// This code inspired by SetURLAttr in the docutil module. When that
	// code understands the xml:base attribute, we can switch over to it completely.
	uni_char* uni_tmp = m_attr_string.CStr();
	uni_char* rel_start = NULL;
	uni_char *url_start = uni_tmp;
	if (uni_tmp && base_url_type != URL_JAVASCRIPT)
	{
		rel_start = uni_strchr(uni_tmp, '#');
		if(rel_start)
		{
			// Must reset the '#' before returning from this method
			*(rel_start++) = '\0';
		}
	}

	if (base_url_type == URL_OPERA || base_url_type == URL_JAVASCRIPT)
	{
		// The g_url_api->GetURL API doesn't work for this kind of urls. Yngve knows why.
	}

	if (!base_url.IsEmpty())
	{
		if (base_url_type != URL_JAVASCRIPT)
		{
			if (url_start && *url_start)
			{
#ifdef SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF
				if (SVGUtils::IsAllWhitespace(url_start, uni_strlen(url_start)))
				{
					m_last_resolved_url = URL();
				}
				else
#endif // SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF
					m_last_resolved_url = g_url_api->GetURL(base_url, url_start, rel_start);
			}
			else if (rel_start)
			{
				m_last_resolved_url = URL(base_url, rel_start);
			}
			else
			{
				m_last_resolved_url = URL();
			}
		}
		else
		{
			OP_ASSERT(rel_start == NULL); // No rel part on javascript urls
			m_last_resolved_url = g_url_api->GetURL(base_url, url_start, rel_start, TRUE);
		}
	}
	else
	{
		m_last_resolved_url = g_url_api->GetURL(url_start, rel_start, FALSE);
	}

	if(rel_start)
	{
		// Must reset the '#' before returning from this method
		*(rel_start-1) = '#';
	}

	return m_last_resolved_url;
}

/* virtual */ OP_STATUS
SVGURL::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	if (m_attr_string.IsEmpty())
	{
		return OpStatus::OK;
	}
	return buffer->Append(m_attr_string.CStr(), m_attr_string.Length());
}

OP_STATUS
SVGURL::SetURL(const uni_char* attr_string, URL url)
{
	m_last_resolved_url = url;
	return m_attr_string.Set(attr_string);
}

URL*
SVGURL::GetURLPtr(URL root_url, HTML_Element* elm)
{
#ifdef DEBUG_ENABLE_OPASSERT
	URL url =
#endif // DEBUG_ENABLE_OPASSERT
		GetURL(root_url, elm);
	OP_ASSERT(url == m_last_resolved_url);
	return &m_last_resolved_url;
} // needed to extract url from attribute

URL*
SVGURL::GetUnreliableCachedURLPtr()
{
	URL* url = &m_last_resolved_url;
	return url;
}

OP_STATUS
SVGURL::Copy(const SVGURL& other)
{
	SVGObject::CopyFlags(other);

	m_last_resolved_url = other.m_last_resolved_url;
	return m_attr_string.Set(other.m_attr_string);
}

/* virtual */ BOOL
SVGURL::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_URL;
	animation_value.reference.svg_url = this;
	return TRUE;
}

#ifdef SVG_SUPPORT_FILTERS

/* SVGEnableBackgroundObject **********************
 */

SVGEnableBackgroundObject::SVGEnableBackgroundObject(SVGEnableBackground::EnableBackgroundType t) :
		SVGObject(SVGOBJECT_ENABLE_BACKGROUND)
{
	m_enable_bg.SetType(t);
}

SVGEnableBackgroundObject::SVGEnableBackgroundObject(SVGEnableBackground::EnableBackgroundType t,
													 SVGNumber x, SVGNumber y, SVGNumber width, SVGNumber height) :
		SVGObject(SVGOBJECT_ENABLE_BACKGROUND)
{
	m_enable_bg.Set(t, x, y, width, height);
}

SVGEnableBackgroundObject::SVGEnableBackgroundObject(SVGEnableBackground& other) :
		SVGObject(SVGOBJECT_ENABLE_BACKGROUND)
{
	m_enable_bg.Copy(other);
}

/* virtual */ SVGObject *
SVGEnableBackgroundObject::Clone() const
{
	SVGEnableBackgroundObject* enable_background_object = OP_NEW(SVGEnableBackgroundObject, ());
	if (enable_background_object)
	{
		enable_background_object->CopyFlags(*this);
		enable_background_object->m_enable_bg.Copy(m_enable_bg);
	}
	return enable_background_object;
}

/* virtual */ OP_STATUS
SVGEnableBackgroundObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return m_enable_bg.GetStringRepresentation(buffer);
}

/* virtual */ BOOL
SVGEnableBackgroundObject::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_ENABLE_BACKGROUND)
	{
		const SVGEnableBackgroundObject &other_eb = static_cast<const SVGEnableBackgroundObject &>(other);
		return other_eb.m_enable_bg == m_enable_bg;
	}
	else
	{
		return FALSE;
	}
}

#endif // SVG_SUPPORT_FILTERS

/* SVGBaselineShiftObject ************************
 */

/* virtual */ OP_STATUS
SVGBaselineShiftObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return m_shift.GetStringRepresentation(buffer);
}

/* virtual */ BOOL
SVGBaselineShiftObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_BASELINE_SHIFT;
	animation_value.reference.svg_baseline_shift = &m_shift;
	return TRUE;
}

/* virtual */ BOOL
SVGBaselineShiftObject::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_BASELINE_SHIFT)
		return m_shift == static_cast<const SVGBaselineShiftObject &>(other).m_shift;
	else
		return FALSE;
}

/* SVGNavRef ******************************************
 */

/* virtual */ SVGObject *
SVGNavRef::Clone() const
{
	SVGNavRef * navref_object = OP_NEW(SVGNavRef, (m_navtype));
	if (navref_object && OpStatus::IsMemoryError(navref_object->Copy(*this)))
	{
		OP_DELETE(navref_object);
		return NULL;
	}

	return navref_object;
}

/* virtual */ BOOL
SVGNavRef::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_NAVREF)
	{
		const SVGNavRef &other_navref = static_cast<const SVGNavRef &>(other);

		if (other_navref.m_navtype != m_navtype)
			return FALSE;

		if (m_navtype != URI)
		{
			return TRUE;
		}
		else if (m_uri && other_navref.m_uri)
		{
			return uni_str_eq(other_navref.m_uri, m_uri);
		}
	}

	return FALSE;
}

/* virtual */ OP_STATUS
SVGNavRef::Copy(const SVGNavRef &navref_object)
{
	SVGObject::CopyFlags(navref_object);

	m_navtype = navref_object.m_navtype;
	const uni_char* other_uri = navref_object.GetURI();
	if (other_uri)
		return SetURI(other_uri, uni_strlen(other_uri));
	else
		return SetURI(NULL, 0);
}

/* virtual */ OP_STATUS
SVGNavRef::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	if (m_navtype == URI && m_uri)
	{
		const char* url_func_str = "url(";
		RETURN_IF_ERROR(buffer->Append(url_func_str, op_strlen(url_func_str)));
		RETURN_IF_ERROR(buffer->Append(m_uri, uni_strlen(m_uri)));
		return buffer->Append(')');
	}

	const char* nav_type_str = "auto";
	if (m_navtype == SELF)
		nav_type_str = "self";

	return buffer->Append(nav_type_str, op_strlen(nav_type_str));
}

OP_STATUS
SVGNavRef::SetURI(const uni_char* uri, int strlen, BOOL add_hashsep /* = FALSE */)
{
	OP_DELETEA(m_uri);
	m_uri = NULL;

	if (uri != NULL && strlen > 0)
	{
		if (add_hashsep && uri[0] != '#')
		{
			m_uri = OP_NEWA(uni_char, strlen + 2);
			if (!m_uri)
				return OpStatus::ERR_NO_MEMORY;
			m_uri[0] = '#';
			uni_strncpy(m_uri+1, uri, strlen);
			m_uri[strlen+1] = '\0';
			return OpStatus::OK;
		}
		return UniSetStrN(m_uri, uri, strlen);
	}
	return OpStatus::OK;
}

/** SVGClassObject **********************
 */

void
SVGClassObject::SetClassAttribute(ClassAttribute* class_attr)
{
	OP_DELETE(m_class_attr);
	m_class_attr = class_attr;
}

OP_STATUS
SVGClassObject::Copy(const SVGClassObject& other)
{
	CopyFlags(other);
	if (other.m_class_attr)
	{
		ComplexAttr* new_attr = NULL;
		RETURN_IF_MEMORY_ERROR(other.m_class_attr->CreateCopy(&new_attr));
		SetClassAttribute(static_cast<ClassAttribute*>(new_attr));
	}
	return OpStatus::OK;
}

/* virtual */ SVGObject*
SVGClassObject::Clone() const
{
	SVGClassObject* clone = OP_NEW(SVGClassObject, ());
	if (!clone || OpStatus::IsMemoryError(clone->Copy(*this)))
	{
		OP_DELETE(clone);
		return NULL;
	}
	else
		return clone;
}

/* virtual */ OP_STATUS
SVGClassObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	if (m_class_attr)
		return m_class_attr->ToString(buffer);
	else
		return OpStatus::OK;
}

/* virtual */ BOOL
SVGClassObject::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_CLASSOBJECT)
	{
		const SVGClassObject& obj = static_cast<const SVGClassObject&>(other);
		const ClassAttribute* attr_this = GetClassAttribute();
		const ClassAttribute* attr_other = obj.GetClassAttribute();
		if (!attr_this && !attr_other)
			return TRUE;
		if (attr_this && attr_other)
		{
			const uni_char* str_this = attr_this->GetAttributeString();
			const uni_char* str_other = attr_other->GetAttributeString();
			if (str_this && str_other && uni_strcmp(str_this, str_other) == 0)
				return TRUE;
		}
	}
	return FALSE;
}

/* virtual */ BOOL
SVGClassObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_CLASSOBJECT;
	animation_value.reference.svg_classobject = this;
	return TRUE;
}

#endif // SVG_SUPPORT
