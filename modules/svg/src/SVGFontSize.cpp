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
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/animation/svganimationvalue.h"

SVGFontSize::SVGFontSize() :
	m_mode(MODE_UNKNOWN)
{
}

SVGFontSize::SVGFontSize(FontSizeMode mode) :
	m_mode(mode)
{
}

OP_STATUS
SVGFontSize::SetLength(const SVGLength& len)
{
    if(len.GetScalar() < 0)
    {
		return OpSVGStatus::INVALID_ARGUMENT;
    }

    m_mode = MODE_LENGTH;
    m_length = len;
    return OpStatus::OK;
}

void
SVGFontSize::SetLengthMode()
{
    m_mode = MODE_LENGTH;
}

void
SVGFontSize::SetAbsoluteFontSize(SVGAbsoluteFontSize size)
{
    m_mode = MODE_ABSOLUTE;
    m_abs_fontsize = size;
}

void
SVGFontSize::SetRelativeFontSize(SVGRelativeFontSize size)
{
    m_mode = MODE_RELATIVE;
    m_rel_fontsize = size;
}

BOOL
SVGFontSize::operator==(const SVGFontSize& other) const
{
	if (m_mode != other.m_mode)
		return FALSE;

	if (m_mode == MODE_ABSOLUTE)
	{
		if (m_abs_fontsize != other.m_abs_fontsize)
			return FALSE;
	}
	else if (m_mode == MODE_RELATIVE)
	{
		if (m_rel_fontsize != other.m_rel_fontsize)
			return FALSE;
	}
	else if (m_mode == MODE_LENGTH)
	{
		if (m_length != other.m_length)
			return FALSE;
	}
	else if (m_mode == MODE_RESOLVED)
	{
		if (!m_resolved_length.Close(other.m_resolved_length))
			return FALSE;
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

OP_STATUS
SVGFontSize::GetStringRepresentation(TempBuffer* buffer) const
{
	switch(m_mode)
	{
	case MODE_LENGTH:
		return m_length.GetStringRepresentation(buffer);
		break;
	case MODE_ABSOLUTE:
		switch(m_abs_fontsize)
		{
		case SVGABSOLUTEFONTSIZE_XXSMALL:
			return buffer->Append("xx-small");
		case SVGABSOLUTEFONTSIZE_XSMALL:
			return buffer->Append("x-small");
		case SVGABSOLUTEFONTSIZE_SMALL:
			return buffer->Append("small");
		case SVGABSOLUTEFONTSIZE_MEDIUM:
			return buffer->Append("medium");
		case SVGABSOLUTEFONTSIZE_LARGE:
			return buffer->Append("large");
		case SVGABSOLUTEFONTSIZE_XLARGE:
			return buffer->Append("x-large");
		case SVGABSOLUTEFONTSIZE_XXLARGE:
			return buffer->Append("xx-large");
		}
		break;
	case MODE_RELATIVE:
		switch(m_rel_fontsize)
		{
		case SVGRELATIVEFONTSIZE_SMALLER:
			return buffer->Append("smaller");
		case SVGRELATIVEFONTSIZE_LARGER:
			return buffer->Append("larger");
		}
	}
	return OpStatus::ERR;
}

SVGFontSizeObject::SVGFontSizeObject() :
    SVGObject(SVGOBJECT_FONTSIZE)
{
}

/* virtual */ SVGObject *
SVGFontSizeObject::Clone() const
{
    SVGFontSizeObject* fontsize_object = OP_NEW(SVGFontSizeObject, ());
    if (fontsize_object)
    {
		fontsize_object->CopyFlags(*this);
		fontsize_object->font_size = font_size;
    }
    return fontsize_object;
}

/* virtual */ OP_STATUS
SVGFontSizeObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return font_size.GetStringRepresentation(buffer);
}

/* virtual */ BOOL
SVGFontSizeObject::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_FONTSIZE)
	    return font_size == static_cast<const SVGFontSizeObject &>(other).font_size;
	return FALSE;
}

/* virtual */ BOOL
SVGFontSizeObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_FONT_SIZE;
	animation_value.reference.svg_font_size = &font_size;
	return TRUE;
}

#endif // SVG_SUPPORT
