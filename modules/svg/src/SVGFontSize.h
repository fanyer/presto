/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_FONTSIZE_H
#define SVG_FONTSIZE_H

// This values comes from a table with 15 as medium in the middle and
// a 1.2 factor between each step:
//
// 8.681
// 10.417
// 12.500
// 15.000
// 18.000
// 21.600
// 25.920
enum SVGAbsoluteFontSize
{
	SVGABSOLUTEFONTSIZE_XXSMALL = 9,
	SVGABSOLUTEFONTSIZE_XSMALL = 10,
	SVGABSOLUTEFONTSIZE_SMALL = 12,
	SVGABSOLUTEFONTSIZE_MEDIUM = 15,
	SVGABSOLUTEFONTSIZE_LARGE = 18,
	SVGABSOLUTEFONTSIZE_XLARGE = 22,
	SVGABSOLUTEFONTSIZE_XXLARGE = 26,
	SVGABSOLUTEFONTSIZE_UNKNOWN = 1
};

enum SVGRelativeFontSize
{
	SVGRELATIVEFONTSIZE_SMALLER,
	SVGRELATIVEFONTSIZE_LARGER
};

#include "modules/svg/svg_external_types.h"
#include "modules/svg/src/SVGObject.h"

class SVGFontSize
{
public:
    enum FontSizeMode {
		MODE_LENGTH,
		MODE_ABSOLUTE,
		MODE_RELATIVE,
		MODE_RESOLVED,
		MODE_UNKNOWN
    };

	SVGFontSize();
	SVGFontSize(FontSizeMode mode);

	void SetLengthMode();
	OP_STATUS SetLength(const SVGLength &len);
    void SetAbsoluteFontSize(SVGAbsoluteFontSize size);
    void SetRelativeFontSize(SVGRelativeFontSize size);
    FontSizeMode Mode() const { return m_mode; }

    SVGNumber ResolvedLength() const { OP_ASSERT(m_mode == MODE_RESOLVED); return m_resolved_length; }
	void Resolve(SVGNumber resolved_length) { m_mode = MODE_RESOLVED; m_resolved_length = resolved_length; }
    const SVGLength& Length() const { OP_ASSERT(m_mode == MODE_LENGTH); return m_length; }
    SVGLength& Length() { OP_ASSERT(m_mode == MODE_LENGTH); return m_length; }
    SVGAbsoluteFontSize AbsoluteFontSize() const { OP_ASSERT(m_mode == MODE_ABSOLUTE); return m_abs_fontsize; }
    SVGRelativeFontSize RelativeFontSize() const { OP_ASSERT(m_mode == MODE_RELATIVE); return m_rel_fontsize; }
    OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;
    BOOL operator==(const SVGFontSize& other) const;

private:
    SVGLength m_length;
    SVGAbsoluteFontSize m_abs_fontsize;
    SVGRelativeFontSize m_rel_fontsize;
    SVGNumber m_resolved_length;
    FontSizeMode m_mode;
};

class SVGFontSizeObject : public SVGObject
{
public:
    SVGFontSizeObject();
    virtual ~SVGFontSizeObject() { }

    virtual SVGObject *Clone() const;
    virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
    virtual BOOL IsEqual(const SVGObject &other) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);
    SVGFontSize font_size;
};

#endif // SVG_FONTSIZE_H
