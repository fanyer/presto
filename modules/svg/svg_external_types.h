/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_EXTERNAL_TYPES_H
#define SVG_EXTERNAL_TYPES_H

#include "modules/svg/svg_number.h"

/**
 * The following enumeration types are used in CSS declarations and
 * therefore a member of the layoutprops structure.
 */

/**
 * Matches SVGENUM_POINTER_EVENTS
 */
enum SVGPointerEvents
{
	SVGPOINTEREVENTS_VISIBLEPAINTED,
	SVGPOINTEREVENTS_VISIBLEFILL,
	SVGPOINTEREVENTS_VISIBLESTROKE,
	SVGPOINTEREVENTS_VISIBLE,
	SVGPOINTEREVENTS_PAINTED,
	SVGPOINTEREVENTS_FILL,
	SVGPOINTEREVENTS_STROKE,
	SVGPOINTEREVENTS_ALL,
	SVGPOINTEREVENTS_NONE,
	SVGPOINTEREVENTS_BOUNDINGBOX,
	SVGPOINTEREVENTS_INHERIT
};

/**
 * Matches SVGENUM_FILL_RULE
 */
enum SVGFillRule
{
	SVGFILL_EVEN_ODD,
	SVGFILL_NON_ZERO,
	SVGFILL_INHERIT,
	SVGFILL_UNKNOWN
};

/**
 * Matches SVGENUM_TEXT_ANCHOR
 */
enum SVGTextAnchor
{
	SVGTEXTANCHOR_START = 0,
	SVGTEXTANCHOR_MIDDLE = 1,
	SVGTEXTANCHOR_END = 2,
	SVGTEXTANCHOR_INHERIT = 3,
	SVGTEXTANCHOR_UNKNOWN = 4
};

/**
 * Matches SVGENUM_STROKE_LINECAP
 */
enum SVGCapType
{
	SVGCAP_BUTT,
	SVGCAP_ROUND,
	SVGCAP_SQUARE,
	SVGCAP_INHERIT,
	SVGCAP_UNKNOWN
};

/**
 * Matches SVGENUM_STROKE_LINEJOIN
 */
enum SVGJoinType
{
	SVGJOIN_MITER,
	SVGJOIN_ROUND,
	SVGJOIN_BEVEL,
	SVGJOIN_INHERIT,
	SVGJOIN_UNKNOWN
};

/**
 * Matches SVGENUM_WRITING_MODE
 */
enum SVGWritingMode
{
	SVGWRITINGMODE_LR_TB = 1,
	SVGWRITINGMODE_RL_TB = 2,
	SVGWRITINGMODE_TB_RL = 3,
	SVGWRITINGMODE_LR = 4,
	SVGWRITINGMODE_RL = 5,
	SVGWRITINGMODE_TB = 6,
	SVGWRITINGMODE_INHERIT = 7,
	SVGWRITINGMODE_UNKNOWN = 0
};

/**
 * Matches SVGENUM_IMAGE_RENDERING
 */
enum SVGImageRendering
{
	SVGIMAGERENDERING_AUTO,
	SVGIMAGERENDERING_OPTIMIZESPEED,
	SVGIMAGERENDERING_OPTIMIZEQUALITY,
	SVGIMAGERENDERING_INHERIT,
	SVGIMAGERENDERING_UNKNOWN
};

/**
 * Matches SVGENUM_SHAPE_RENDERING
 */
enum SVGShapeRendering
{
	SVGSHAPERENDERING_AUTO,
	SVGSHAPERENDERING_OPTIMIZESPEED,
	SVGSHAPERENDERING_CRISPEDGES,
	SVGSHAPERENDERING_GEOMETRICPRECISION,
	SVGSHAPERENDERING_INHERIT,
	SVGSHAPERENDERING_UNKNOWN
};

/**
 * Matches SVGENUM_ALIGNMENT_BASELINE
 */
enum SVGAlignmentBaseline
{
	SVGALIGNMENTBASELINE_AUTO = 1,
	SVGALIGNMENTBASELINE_BASELINE = 2,
	SVGALIGNMENTBASELINE_BEFORE_EDGE = 3,
	SVGALIGNMENTBASELINE_TEXT_BEFORE_EDGE = 4,
	SVGALIGNMENTBASELINE_MIDDLE = 5,
	SVGALIGNMENTBASELINE_CENTRAL = 6,
	SVGALIGNMENTBASELINE_AFTER_EDGE = 7,
	SVGALIGNMENTBASELINE_TEXT_AFTER_EDGE = 8,
	SVGALIGNMENTBASELINE_IDEOGRAPHIC = 9,
	SVGALIGNMENTBASELINE_ALPHABETIC = 10,
	SVGALIGNMENTBASELINE_HANGING = 11,
	SVGALIGNMENTBASELINE_MATHEMATICAL = 12,
	SVGALIGNMENTBASELINE_INHERIT = 13,
	SVGALIGNMENTBASELINE_UNKNOWN = 0
};

/**
 * Matches SVGENUM_DOMINANT_BASELINE
 */
enum SVGDominantBaseline
{
	SVGDOMINANTBASELINE_AUTO = 1,
	SVGDOMINANTBASELINE_USE_SCRIPT = 2,
	SVGDOMINANTBASELINE_NO_CHANGE = 3,
	SVGDOMINANTBASELINE_RESET_SIZE = 4,
	SVGDOMINANTBASELINE_IDEOGRAPHIC = 5,
	SVGDOMINANTBASELINE_ALPHABETIC = 6,
	SVGDOMINANTBASELINE_HANGING = 7,
	SVGDOMINANTBASELINE_MATHEMATICAL = 8,
	SVGDOMINANTBASELINE_CENTRAL = 9,
	SVGDOMINANTBASELINE_MIDDLE = 10,
	SVGDOMINANTBASELINE_TEXT_AFTER_EDGE = 11,
	SVGDOMINANTBASELINE_TEXT_BEFORE_EDGE = 12,
	SVGDOMINANTBASELINE_INHERIT = 13,
	SVGDOMINANTBASELINE_UNKNOWN = 0
};

/**
 * Matches SVGENUM_ARABIC_FORM
 */
enum SVGArabicForm
{
	SVGARABICFORM_INITIAL = 1,
	SVGARABICFORM_MEDIAL = 2,
	SVGARABICFORM_TERMINAL = 3,
	SVGARABICFORM_ISOLATED = 4
};

/**
 * Matches SVGENUM_TEXT_RENDERING
 */
enum SVGTextRendering
{
	SVGTEXTRENDERING_AUTO,
	SVGTEXTRENDERING_OPTIMIZESPEED,
	SVGTEXTRENDERING_OPTIMIZELEGIBILITY,
	SVGTEXTRENDERING_GEOMETRICPRECISION,
	SVGTEXTRENDERING_INHERIT,
	SVGTEXTRENDERING_UNKNOWN
};

/**
 * Matches SVGENUM_COLOR_INTERPOLATION
 */
enum SVGColorInterpolation
{
	SVGCOLORINTERPOLATION_AUTO,
	SVGCOLORINTERPOLATION_SRGB,
	SVGCOLORINTERPOLATION_LINEARRGB,
	SVGCOLORINTERPOLATION_INHERIT
};

/**
 * Matches SVGENUM_VECTOR_EFFECT
 */
enum SVGVectorEffect
{
	SVGVECTOREFFECT_NONE,
	SVGVECTOREFFECT_NON_SCALING_STROKE,
	SVGVECTOREFFECT_INHERIT
};

/**
 * Matches SVGENUM_BUFFERED_RENDERING
 */
enum SVGBufferedRendering
{
	SVGBUFFEREDRENDERING_AUTO,
	SVGBUFFEREDRENDERING_STATIC,
	SVGBUFFEREDRENDERING_DYNAMIC
};


class SVGPropertyReference
{
public:
	SVGPropertyReference()
		: m_refdata_init(0) {}

	static SVGPropertyReference* IncRef(SVGPropertyReference* propref);
	static void DecRef(SVGPropertyReference* propref);
	static SVGPropertyReference* Protect(SVGPropertyReference* propref);

protected:
	virtual ~SVGPropertyReference() {}

private:
	union
	{
		struct
		{
			signed int refcount:31;
			unsigned int protect:1;
		} m_refdata;
		unsigned int m_refdata_init;
	};
};

/**
 * Two things that needs to be done with colors and paint.
 *
 * 1. Paint could inherit color? or aggregate it? Anyway, the current
 *     implementation makes for some code-duplication
 *
 * 2. Remember color names, so that we can return them in string
 *    representations. Needed for CSS matching for example.
 *
 */
class SVGPaint : public SVGPropertyReference
{
public:
	// These numeric values has been lifted from SVG DOM and must
	// match them
	enum PaintType
	{
		UNKNOWN = 0,
		RGBCOLOR = 1,
		RGBCOLOR_ICCCOLOR = 2,
		NONE = 101,
		CURRENT_COLOR = 102,
		URI_NONE = 103,
		URI_RGBCOLOR = 104,
		URI_RGBCOLOR_ICCCOLOR = 105,
		URI_CURRENT_COLOR = 106,
		URI = 107,
		INHERIT = 200 // Should use SVGObjects inherit mechanism instead
	};

	SVGPaint(PaintType t = UNKNOWN); // Initialize members to default values
	virtual ~SVGPaint();

	/**
	 * Helper for setting a rgb color. The paint type isn't touched by
	 * this because the rgbcolor could be used in many different paint
	 * types
	 */
	void SetRGBColor(UINT8 r, UINT8 g, UINT8 b);

	/**
	 * Set the color from a color ref
	 */
	void SetColorRef(COLORREF cref) { m_color = cref; }

	/**
	 * Get RGB color
	 *
	 * This involves looking up system colors and masking away
	 * extended color information (as stored in the upper bits of
	 * COLORREF).
	 */
	UINT32 GetRGBColor() const;

	/**
	 * Get the color as as parsed (keeps the extra bits of
	 * CSSColorKeyword)
	 */
	COLORREF GetColorRef() const { return m_color; }

	/**
	 * Helper for setting a icc color. The paint type isn't touched by
	 * this because the icccolor could be used in many different paint
	 * types
	 */
	void SetICCColor(UINT32 icccolor) { m_icccolor = icccolor; }

	/**
	 * Get ICC color
	 */
	UINT32 GetICCColor() const { return m_icccolor; }

	/**
	 * Get URI
	 */
	const uni_char* GetURI() const { return m_uri; }

	/**
	 * If SVG_DOM is defined the numeric value of SVGPaintType is
	 * guaranteed to be matching the SVG DOM paint types.
	 *
	 * Setting the paint type does not change the internal state
	 * except setting the type. You need to remove uri if the paint
	 * should stop pointing at some thing. Use SetPaint if you want to
	 * change the state in one go
	 */
	void SetPaintType(PaintType t) { m_paint_type = t; }

	/**
	 * If SVG_DOM is defined the numeric value of SVGPaintType is
	 * guaranteed to be matching the SVG DOM paint types.
	 */
	PaintType GetPaintType() const { return m_paint_type; }

#ifdef SVG_DOM
	/**
	 * Set paint in one go
	 * @return OpBoolean::IS_TRUE if OK OpBoolean::IS_FALSE if one or
	 * more values are invalid, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_BOOLEAN SetPaint(PaintType type, const uni_char* uri, const uni_char* rgb_color, const uni_char* icc_color);
#endif // SVG_DOM

	/**
	 * Used from style to set a paint from a style declaration and a
	 * parent paint
	 */
	static OP_STATUS CloneFromCSSDecl(CSS_decl* cp, SVGPropertyReference*& paintref,
									  const SVGPaint* parent);

	/**
	 * Gets the color as a CSS_decl. Note that it's only supported for
	 * computed values in the css cascade.
	 */
	OP_STATUS GetCSSDecl(CSS_decl** out_cd, HTML_Element* element, short property, 
						 BOOL is_current_style) const;

	/**
	 * Set the URI of the paint.
	 *
	 * @param uri A pointer to a string to copy the URI from. If the
	 * pointer is NULL the URI of the paint is also set to NULL.
	 * @param strlen The length of the string. Set to zero if uri is NULL.
	 * @param add_hashsep
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS SetURI(const uni_char* uri, int strlen, BOOL add_hashsep = FALSE);

	/**
	 * Get string representation. Cannot yet remember color names, so
	 * what starts in 'blue' will return the string '#0000FF'.
	 */
	OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;

	/**
	 * Copy another paint.
	 */
	OP_STATUS Copy(const SVGPaint &other);

	/**
	 * Compare to another paint.
	 */
	BOOL IsEqual(const SVGPaint& other) const;

protected:
#ifdef SVG_DOM
	OP_BOOLEAN SetPaintURIRGBColor(PaintType type, const uni_char* uri, const uni_char* rgb_color);
	OP_BOOLEAN SetPaintRGBColor(PaintType type, const uni_char* rgb_color);
	OP_BOOLEAN SetPaintURI(PaintType type, const uni_char* uri);
#endif // SVG_DOM

private:
	SVGPaint(const SVGPaint& X);				// Don't use this
	void operator=(const SVGPaint& X);			// Don't use this

	PaintType 		m_paint_type;
	uni_char*		m_uri;
	COLORREF 		m_color;
	UINT32			m_icccolor;
};


class SVGLength
{
public:
	SVGLength() :
			m_scalar(0),
			m_unit(CSS_NUMBER)
		{}
	SVGLength(SVGNumber scalar, int unit) :
			m_scalar(scalar),
			m_unit(unit)
		{}

	enum LengthOrientation
	{
		SVGLENGTH_X,
		SVGLENGTH_Y,
		SVGLENGTH_OTHER
	};

	/**
	 * Get scalar value (The stored length without unit applied)
	 */
	SVGNumber GetScalar() const { return m_scalar; }

	/**
	 * Set the stored length in the current unit.
	 */
	void SetScalar(SVGNumber s) { m_scalar = s; }

	/**
	 * Get current unit
	 */
	int GetUnit() const { return m_unit; }

	/**
	 * Set unit without changing the stored length
	 */
	void SetUnit(int u) { m_unit = u; }

	BOOL operator==(const SVGLength& other) const;
	BOOL operator!=(const SVGLength& other) const { return !operator==(other); }

	OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;

private:
	SVGNumber m_scalar;
	int m_unit;
};

class SVGBaselineShift
{
public:
	enum BaselineShiftType
	{
		SVGBASELINESHIFT_BASELINE,
		SVGBASELINESHIFT_SUB,
		SVGBASELINESHIFT_SUPER,
		SVGBASELINESHIFT_VALUE
	};

	SVGBaselineShift(BaselineShiftType bls_type = SVGBASELINESHIFT_BASELINE) :
			m_shift_type(bls_type) {}
	SVGBaselineShift(BaselineShiftType bls_type, SVGLength value) :
			m_value(value),
			m_shift_type(bls_type) {}

	OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;

	SVGLength GetValue() const { return m_value; }
	SVGLength& GetValueRef() { return m_value; }
	const SVGLength& GetValueRef() const { return m_value; }

	BaselineShiftType GetShiftType() const { return m_shift_type; }
	void SetShiftType(BaselineShiftType bls_type) { m_shift_type = bls_type; }

	BOOL operator==(const SVGBaselineShift &other) const;

private:
	SVGLength m_value;
	BaselineShiftType m_shift_type;
};

class SVGColor
{
public:

	// This is in sync with the SVG DOM SVGColor types
	enum ColorType
	{
		SVGCOLOR_UNKNOWN = 0,
		SVGCOLOR_RGBCOLOR = 1,
		SVGCOLOR_RGBCOLOR_ICCCOLOR = 2,
		SVGCOLOR_CURRENT_COLOR = 3,
		SVGCOLOR_NONE = 4
	};

	SVGColor() :
			m_color_type(SVGCOLOR_UNKNOWN),
			m_color(0),
			m_icccolor(0)
		{}
	SVGColor(ColorType type, COLORREF color, UINT32 icccolor);

	/**
	 * Get ICC Color
	 */
    UINT32 GetICCColor() const { return m_icccolor; }

	/**
	 * Get RGB color.
	 *
	 * This involves looking up system colors and masking away
	 * extended color information (as stored in the upper bits of
	 * COLORREF).
	 */
	UINT32 GetRGBColor() const;

	/**
	 * Get the color as as parsed (keeps the extra bits of
	 * CSSColorKeyword)
	 */
	COLORREF GetColorRef() const { return m_color; }

	/**
	 * Get Color type
	 */
	ColorType GetColorType() const { return m_color_type; }

	/**
	 * Set RGB Color from a composed RGBColor value
	 */
	void SetRGBColor(UINT32 col);

	/**
	 * Set RGB color from a color ref
	 */
	void SetColorRef(COLORREF cref) { m_color = cref; }

	/**
	 * Set RGB Color from the channels red, green and blue.
	 */
	void SetRGBColor(UINT8 r, UINT8 g, UINT8 b);

	/**
	 * Set ICC Color
	 */
	void SetICCColor(UINT32 icccolor) { m_icccolor = icccolor; }

	/**
	 * Set color type
	 */
	void SetColorType(ColorType type) { m_color_type = type; }

	/**
	 * Copy another color
	 */
	void Copy(const SVGColor& v1);

	/**
	 * Get string representation. Cannot yet remember color names, so
	 * what starts in 'blue' will return the string '#0000FF'.
	 */
	OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;

	/**
	 * Set the color from a CSS declaration and the parent property.
	 */
	OP_STATUS SetFromCSSDecl(CSS_decl* cp, SVGColor* parent_color);

	/**
	 * Gets the color as a CSS_decl. Note that it's only supported for
	 * computed values in the css cascade.
	 */
	OP_STATUS GetCSSDecl(CSS_decl** out_cd, HTML_Element* element, short property, 
						 BOOL is_current_style) const;

	BOOL operator==(const SVGColor& other) const;

private:
	ColorType		m_color_type;
	COLORREF		m_color;
	UINT32			m_icccolor;
};

struct SVGDashArray : public SVGPropertyReference
{
	SVGDashArray() : dash_array(NULL), dash_array_size(0), is_none(FALSE) {}
	~SVGDashArray() { OP_DELETEA(dash_array); }

	SVGLength*					dash_array;		 ///< An array of pointers to dash lengths.
	UINT32						dash_array_size; ///< Size of array of dash lengths.
	BOOL						is_none;	///< A flag to tell if the value 'none' have been specified
};

#ifdef SVG_SUPPORT_FILTERS
class SVGEnableBackground
{
public:
	enum EnableBackgroundType
	{
		SVGENABLEBG_NEW,
		SVGENABLEBG_ACCUMULATE
	};

	SVGEnableBackground(EnableBackgroundType = SVGENABLEBG_ACCUMULATE);
	SVGEnableBackground(EnableBackgroundType t,
						SVGNumber x, SVGNumber y,
						SVGNumber width, SVGNumber height);
	void Copy(const SVGEnableBackground& other);
	OP_STATUS GetStringRepresentation(TempBuffer* buffer) const;

	void Set(EnableBackgroundType type, SVGNumber x, SVGNumber y, SVGNumber width, SVGNumber height);

	void SetType(EnableBackgroundType type) { info.desc = (int)type; }
	EnableBackgroundType GetType() const { return (EnableBackgroundType)info.desc; }
	BOOL IsRectValid() const { return info.bgrect_valid ? TRUE : FALSE; }
	SVGNumber GetX() { return m_x; }
	SVGNumber GetY() { return m_y; }
	SVGNumber GetWidth() { return m_width; }
	SVGNumber GetHeight() { return m_height; }

	BOOL operator==(const SVGEnableBackground& other) const;

private:
	SVGNumber m_x;
	SVGNumber m_y;
	SVGNumber m_width;
	SVGNumber m_height;

	union
	{
		struct
		{
			unsigned int desc:7;
			unsigned int bgrect_valid:1;
			/* 8 bits */
		} info;
		unsigned int m_eb_info_init;
	};
};
#endif // SVG_SUPPORT_FILTERS

struct SVGURLReference
{
	const uni_char* url_str;
	union
	{
		struct
		{
			unsigned int url_str_len:31;
			unsigned int is_none:1;
		} info;
		UINT32 urlref_info_init;
	};
};

class SVGRect
{
public:
	SVGNumber x;
	SVGNumber y;
	SVGNumber width;
	SVGNumber height;

	SVGRect() : x(0), y(0), width(0), height(0) {}
	SVGRect(SVGNumber _x, SVGNumber _y, SVGNumber _width, SVGNumber _height) : x(_x), y(_y), width(_width), height(_height) {}

	OpRect GetEnclosingRect() const;

	void Set(SVGNumber x, SVGNumber y, SVGNumber width, SVGNumber height);
	void Set(const SVGRect&);

	void UnionWith(const SVGRect& rect);

	void Interpolate(const SVGRect &from_rect, const SVGRect& to_rect,
					 SVG_ANIMATION_INTERVAL_POSITION interval_position);

	OpRect GetSimilarRect() const
	{
		return OpRect((INT32)x.GetIntegerValue(),
					  (INT32)y.GetIntegerValue(),
					  (INT32)width.GetIntegerValue(),
					  (INT32)height.GetIntegerValue());
	}

	BOOL IsEmpty() const { return width <= 0 || height <= 0; }
	BOOL IsEqual(const SVGRect& other) const;
};

#endif // SVG_EXTERNAL_TYPES_H
