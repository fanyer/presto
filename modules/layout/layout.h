/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _LAYOUT_H_
#define _LAYOUT_H_

#include "modules/layout/internal.h"
#include "modules/layout/frm.h"
#include "modules/display/color.h"
#include "modules/style/css.h"
#include "modules/style/css_gradient.h"
#include "modules/layout/layout_coord.h"
#include "modules/layout/layout_fixed_point.h"

struct BorderEdge
{
	short			width;
	short			style; // A CSSValue stored as a short for the sake of compactness
	COLORREF		color;

	short			radius_start; // A negative value is a percentage value relative to the border box
	short			radius_end;   // A negative value is a percentage value relative to the border box

	union
	{
		struct
		{
			unsigned
					radius_start_is_decimal:1;
			unsigned
					radius_end_is_decimal:1;
		} packed;
		unsigned int
					packed_init;
	};
	CSSValue		GetStyle() const { return CSSValue(style); }

	void			Reset() { width = BORDER_WIDTH_NOT_SET; style = BORDER_STYLE_NOT_SET; color = USE_DEFAULT_COLOR; radius_start = radius_end = 0; packed_init = 0; }
	void			SetDefault() { width = CSS_BORDER_WIDTH_MEDIUM; style = CSS_VALUE_none; color = USE_DEFAULT_COLOR; radius_start = radius_end = 0; packed_init = 0; }
	void			SetOutlineDefault() { color = COLORREF(CSS_COLOR_invert); style = CSS_VALUE_none; width = CSS_BORDER_WIDTH_MEDIUM; radius_start = radius_end = 0; packed_init = 0; }
	void			SetRadius(short radius, BOOL is_decimal = FALSE) { SetRadiusStart(radius, is_decimal); SetRadiusEnd(radius, is_decimal); }
	void			SetRadiusStart(short radius, BOOL is_decimal) { radius_start = radius; packed.radius_start_is_decimal = is_decimal; }
	void			SetRadiusEnd(short radius, BOOL is_decimal) { radius_end = radius; packed.radius_end_is_decimal = is_decimal; }
	void			ResolveRadiusPercentages(LayoutCoord percentage_of);
	void			ResetRadius() { SetRadiusStart(0, FALSE); SetRadiusEnd(0, FALSE); }
	void			Collapse(const BorderEdge& other_edge);
	BOOL			HasRadius() const { return radius_start != 0 || radius_end != 0; }
	BOOL			IsEmpty() const { return style == BORDER_STYLE_NOT_SET || style == CSS_VALUE_none || width == 0; }
};

struct Border
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

	BorderEdge		top;
	BorderEdge		left;
	BorderEdge		right;
	BorderEdge		bottom;

	void			Reset();
	void			SetDefault() { top.SetDefault(); left.SetDefault(); right.SetDefault(); bottom.SetDefault(); }
	void			ResetRadius() { top.ResetRadius(); left.ResetRadius(); right.ResetRadius(); bottom.ResetRadius(); }
	void			SetRadius(short radius) {
		top.SetRadiusStart(radius, FALSE);
		left.SetRadiusStart(radius, FALSE);
		right.SetRadiusStart(radius, FALSE);
		bottom.SetRadiusStart(radius, FALSE);
		top.SetRadiusEnd(radius, FALSE);
		left.SetRadiusEnd(radius, FALSE);
		right.SetRadiusEnd(radius, FALSE);
		bottom.SetRadiusEnd(radius, FALSE);
	}
	BOOL			HasRadius() const { return top.HasRadius() || left.HasRadius() || right.HasRadius() || bottom.HasRadius(); }
	BOOL			IsEmpty() const { return top.IsEmpty() && left.IsEmpty() && right.IsEmpty() && bottom.IsEmpty(); }
	BOOL			IsAllSolidAndSameColor() const
	{
		return
			top.style == CSS_VALUE_solid &&
			top.style == left.style &&
			top.style == right.style &&
			top.style == bottom.style &&
			top.color == left.color &&
			top.color == right.color &&
			top.color == bottom.color;
	}
	void			SetWidth(short width) { top.width = left.width = right.width = bottom.width = width; }
	void			SetStyle(CSSValue style) { top.style = left.style = right.style = bottom.style = style; }
	void			SetColor(COLORREF color) { top.color = left.color = right.color = bottom.color = color; }

	/** Convert computed border values to used border values.

		In essence, this means using the border box to convert
		percentage values to absolute values. */

	Border			GetUsedBorder(const OpRect& border_box) const {
		if (HasRadius())
		{
			Border used = *this;
			used.left.ResolveRadiusPercentages(LayoutCoord(border_box.height));
			used.right.ResolveRadiusPercentages(LayoutCoord(border_box.height));
			used.top.ResolveRadiusPercentages(LayoutCoord(border_box.width));
			used.bottom.ResolveRadiusPercentages(LayoutCoord(border_box.width));
			return used;
		}
		else
			return *this;
	}
};

struct BorderWidths
{
	unsigned short top;
	unsigned short left;
	unsigned short bottom;
	unsigned short right;
};

class CssURL
{
public:
	void Set(const uni_char* url) { m_url = url; }
	void Set(const uni_char* url, BOOL user_defined) { m_url = reinterpret_cast<const uni_char*>(reinterpret_cast<UINTPTR>(url) | (user_defined ? 1 : 0)); }
	operator const uni_char*() const { return reinterpret_cast<const uni_char*>(reinterpret_cast<UINTPTR>(m_url) & ~1); }
	BOOL IsUserDefined() const { return (reinterpret_cast<UINTPTR>(m_url) & 1) != 0; }
	BOOL IsSkinURL() const { const uni_char* url = *this; return url && url[0] == 's' && url[1] == ':'; }
private:
	const uni_char* m_url;
};

struct BorderImage
{
    CssURL          border_img;

#ifdef CSS_GRADIENT_SUPPORT
    CSS_Gradient*   border_gradient;
#endif // CSS_GRADIENT_SUPPORT

    int             cut[4];   // Negative values are percentages
    short           scale[2]; // css values
	BOOL			has_border_widths;

    void            Reset() {
        border_img.Set(0);
#ifdef CSS_GRADIENT_SUPPORT
        border_gradient = NULL;
#endif
        cut[0] = cut[1] = cut[2] = cut[3] = 0;
        scale[0] = scale[1] = CSS_VALUE_stretch;
        has_border_widths = FALSE;
    }

	/**
	 * Does this instance have an actual border image
	 *
	 * @return TRUE if it has a border image
	 */
	BOOL			HasBorderImage() const {
		if (border_img)
			return TRUE;
#ifdef CSS_GRADIENT_SUPPORT
		if (border_gradient)
			return TRUE;
#endif // CSS_GRADIENT_SUPPORT
		return FALSE;
	}
};

#define BG_SIZE_X_AUTO				LAYOUT_COORD_MIN
#define BG_SIZE_X_MIN_VALUE			(LAYOUT_COORD_MIN+LayoutCoord(1))
#define BG_SIZE_X_MAX_VALUE			LAYOUT_COORD_MAX
#define BG_SIZE_Y_AUTO				LAYOUT_COORD_MIN
#define BG_SIZE_Y_MIN_VALUE			(LAYOUT_COORD_MIN+LayoutCoord(1))
#define BG_SIZE_Y_MAX_VALUE			LAYOUT_COORD_MAX

struct BackgroundProperties
{
	/* background-image */

	CssURL			bg_img;
	int				bg_idx;
#ifdef CSS_GRADIENT_SUPPORT
	CSS_Gradient	*bg_gradient;
#endif // CSS_GRADIENT_SUPPORT

	/** @name background position and size.

		In layout fixed point format when stored as percentage values
		(indicated by flags). */

	//@{
	LayoutCoord		bg_ypos;
	LayoutCoord		bg_xpos;
	LayoutCoord		bg_ysize;
	LayoutCoord		bg_xsize;
	//@}

	/* Reference point for background-position.  The default value is
	   'left top' */

	CSSValue		bg_pos_xref;
	CSSValue		bg_pos_yref;

	/* background-repeat */

	short			bg_repeat_x;
	short			bg_repeat_y;

	/* background-attachment */

	short			bg_attach;

	/* background-origin */

	short			bg_origin;

	/* background-clip */

	short			bg_clip;

	union
	{
		struct
		{
			unsigned int bg_xpos_per:1;
			unsigned int bg_ypos_per:1;
			unsigned int bg_xsize_per:1;
			unsigned int bg_ysize_per:1;
			unsigned int bg_size_contain:1; // bg_xsize and bg_ysize ignored if set
			unsigned int bg_size_cover:1; // bg_size_contain, bg_xsize and bg_ysize ignored if set
		} packed; // 32 bits
		unsigned int packed_init;
	};

	void			Reset() {
		bg_img.Set(0);
#ifdef CSS_GRADIENT_SUPPORT
		bg_gradient = NULL;
#endif // CSS_GRADIENT_SUPPORT

		bg_ypos = LayoutCoord(0);
		bg_xpos = LayoutCoord(0);
		bg_pos_xref = CSS_VALUE_left;
		bg_pos_yref = CSS_VALUE_top;

		bg_ysize = BG_SIZE_Y_AUTO;
		bg_xsize = BG_SIZE_X_AUTO;

		bg_repeat_x = CSS_VALUE_repeat;
		bg_repeat_y = CSS_VALUE_repeat;
		bg_attach = CSS_VALUE_scroll;
		bg_origin = CSS_VALUE_padding_box;
		bg_clip = CSS_VALUE_border_box;
		packed_init = 0;
	}
};

struct Shadow
{
	COLORREF		color;
	short			left;
	short			top;
	short			blur;
	short			spread;
	BOOL			inset;

	void Reset()
	{
		color = USE_DEFAULT_COLOR;
		left = top = blur = spread = 0;
		inset = FALSE;
	}
};

/** Collection of keyword-based flexbox properties.

	Some apply to flexboxes, others apply to flexbox items. */

struct FlexboxModes
{
private:
	struct
	{
		unsigned int justify_content:4; ///< Of type JustifyContent
		unsigned int align_content:4; ///< Of type AlignContent
		unsigned int align_items:4; ///< Of type AlignItems
		unsigned int align_self:4; ///< Of type AlignSelf
		unsigned int direction:4; ///< Of type Direction
		unsigned int wrap:4; ///< Of type Wrap
	} packed;
	UINT32 packed_init;

public:

	enum JustifyContent ///< justify-content
	{
		JCONTENT_INHERIT,
		JCONTENT_FLEX_START,
		JCONTENT_FLEX_END,
		JCONTENT_CENTER,
		JCONTENT_SPACE_BETWEEN,
		JCONTENT_SPACE_AROUND
	};

	enum AlignContent ///< align-content
	{
		ACONTENT_INHERIT,
		ACONTENT_FLEX_START,
		ACONTENT_FLEX_END,
		ACONTENT_CENTER,
		ACONTENT_SPACE_BETWEEN,
		ACONTENT_SPACE_AROUND,
		ACONTENT_STRETCH
	};

	enum AlignItems ///< align-items
	{
		AITEMS_INHERIT,
		AITEMS_FLEX_START,
		AITEMS_FLEX_END,
		AITEMS_CENTER,
		AITEMS_BASELINE,
		AITEMS_STRETCH
	};

	enum AlignSelf ///< align-self
	{
		ASELF_INHERIT,
		ASELF_AUTO,
		ASELF_FLEX_START,
		ASELF_FLEX_END,
		ASELF_CENTER,
		ASELF_BASELINE,
		ASELF_STRETCH
	};

	enum Direction ///< flex-direction
	{
		DIR_INHERIT,
		DIR_ROW,
		DIR_ROW_REVERSE,
		DIR_COLUMN,
		DIR_COLUMN_REVERSE
	};

	enum Wrap ///< flex-wrap
	{
		WRAP_INHERIT,
		WRAP_NOWRAP,
		WRAP_WRAP,
		WRAP_WRAP_REVERSE,
	};

	/** Reset properties to their initial value. */
	void Reset()
	{
		packed_init = 0;

		SetJustifyContent(JCONTENT_FLEX_START);
		SetAlignContent(ACONTENT_STRETCH);
		SetAlignItems(AITEMS_STRETCH);
		SetAlignSelf(ASELF_AUTO);
		SetDirection(DIR_ROW);
		SetWrap(WRAP_NOWRAP);
	}

	/** Set justify-content. */
	void SetJustifyContent(JustifyContent val) { packed.justify_content = val; }

	/** Set align-content. */
	void SetAlignContent(AlignContent val) { packed.align_content = val; }

	/** Set align-items. */
	void SetAlignItems(AlignItems val) { packed.align_items = val; }

	/** Set align-self. */
	void SetAlignSelf(AlignSelf val) { packed.align_self = val; }

	/** Set flex-direction. */
	void SetDirection(Direction val) { packed.direction = val; }

	/** Set flex-wrap. */
	void SetWrap(Wrap val) { packed.wrap = val; }

	/** Get justify-content. */
	JustifyContent GetJustifyContent() const { return JustifyContent(packed.justify_content); }

	/** Get align-content. */
	AlignContent GetAlignContent() const { return AlignContent(packed.align_content); }

	/** Get align-items. */
	AlignItems GetAlignItems() const { return AlignItems(packed.align_items); }

	/** Get align-self. */
	AlignSelf GetAlignSelf() const { return AlignSelf(packed.align_self); }

	/** Get flex-direction. */
	Direction GetDirection() const { return Direction(packed.direction); }

	/** Get flex-wrap. */
	Wrap GetWrap() const { return Wrap(packed.wrap); }

	/** Set justify-content as a CSSValue. */
	void SetJustifyContentCSSValue(CSSValue val)
	{
		switch (val)
		{
		case CSS_VALUE_inherit:
			SetJustifyContent(JCONTENT_INHERIT); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_start:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_start:
			SetJustifyContent(JCONTENT_FLEX_START); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_end:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_end:
			SetJustifyContent(JCONTENT_FLEX_END); break;
		case CSS_VALUE_center:
			SetJustifyContent(JCONTENT_CENTER); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_justify:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_space_between:
			SetJustifyContent(JCONTENT_SPACE_BETWEEN); break;
		case CSS_VALUE_space_around:
			SetJustifyContent(JCONTENT_SPACE_AROUND); break;
		default: OP_ASSERT(!"Unexpected value");
		}
	}

	/** Set align-content as a CSSValue. */
	void SetAlignContentCSSValue(CSSValue val)
	{
		switch (val)
		{
		case CSS_VALUE_inherit:
			SetAlignContent(ACONTENT_INHERIT); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_start:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_start:
			SetAlignContent(ACONTENT_FLEX_START); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_end:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_end:
			SetAlignContent(ACONTENT_FLEX_END); break;
		case CSS_VALUE_center:
			SetAlignContent(ACONTENT_CENTER); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_justify:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_space_between:
			SetAlignContent(ACONTENT_SPACE_BETWEEN); break;
		case CSS_VALUE_space_around:
			SetAlignContent(ACONTENT_SPACE_AROUND); break;
		case CSS_VALUE_stretch:
			SetAlignContent(ACONTENT_STRETCH); break;
		default: OP_ASSERT(!"Unexpected value");
		}
	}

	/** Set align-items as a CSSValue. */
	void SetAlignItemsCSSValue(CSSValue val)
	{
		switch (val)
		{
		case CSS_VALUE_inherit:
			SetAlignItems(AITEMS_INHERIT); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_start:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_start:
			SetAlignItems(AITEMS_FLEX_START); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_end:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_end:
			SetAlignItems(AITEMS_FLEX_END); break;
		case CSS_VALUE_center:
			SetAlignItems(AITEMS_CENTER); break;
		case CSS_VALUE_baseline:
			SetAlignItems(AITEMS_BASELINE); break;
		case CSS_VALUE_stretch:
			SetAlignItems(AITEMS_STRETCH); break;
		default: OP_ASSERT(!"Unexpected value");
		}
	}

	/** Set align-self as a CSSValue. */
	void SetAlignSelfCSSValue(CSSValue val)
	{
		switch (val)
		{
		case CSS_VALUE_inherit:
			SetAlignSelf(ASELF_INHERIT); break;
		case CSS_VALUE_auto:
			SetAlignSelf(ASELF_AUTO); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_start:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_start:
			SetAlignSelf(ASELF_FLEX_START); break;
#ifdef WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_end:
#endif // WEBKIT_OLD_FLEXBOX
		case CSS_VALUE_flex_end:
			SetAlignSelf(ASELF_FLEX_END); break;
		case CSS_VALUE_center:
			SetAlignSelf(ASELF_CENTER); break;
		case CSS_VALUE_baseline:
			SetAlignSelf(ASELF_BASELINE); break;
		case CSS_VALUE_stretch:
			SetAlignSelf(ASELF_STRETCH); break;
		default: OP_ASSERT(!"Unexpected value");
		}
	}

	/** Set flex-direction as a CSSValue. */
	void SetDirectionCSSValue(CSSValue val)
	{
		switch (val)
		{
		case CSS_VALUE_inherit: SetDirection(DIR_INHERIT); break;
		case CSS_VALUE_row: SetDirection(DIR_ROW); break;
		case CSS_VALUE_row_reverse: SetDirection(DIR_ROW_REVERSE); break;
		case CSS_VALUE_column: SetDirection(DIR_COLUMN); break;
		case CSS_VALUE_column_reverse: SetDirection(DIR_COLUMN_REVERSE); break;
		default: OP_ASSERT(!"Unexpected value");
		}
	}

	/** Set flex-wrap as a CSSValue. */
	void SetWrapCSSValue(CSSValue val)
	{
		switch (val)
		{
		case CSS_VALUE_inherit: SetWrap(WRAP_INHERIT); break;
		case CSS_VALUE_nowrap: SetWrap(WRAP_NOWRAP); break;
		case CSS_VALUE_wrap: SetWrap(WRAP_WRAP); break;
		case CSS_VALUE_wrap_reverse: SetWrap(WRAP_WRAP_REVERSE); break;
		default: OP_ASSERT(!"Unexpected value");
		}
	}

	/** Get justify-content as a CSSValue. */
	CSSValue GetJustifyContentCSSValue() const
	{
		switch (GetJustifyContent())
		{
		default: return CSS_VALUE_inherit;
		case JCONTENT_FLEX_START: return CSS_VALUE_flex_start;
		case JCONTENT_FLEX_END: return CSS_VALUE_flex_end;
		case JCONTENT_CENTER: return CSS_VALUE_center;
		case JCONTENT_SPACE_BETWEEN: return CSS_VALUE_space_between;
		case JCONTENT_SPACE_AROUND: return CSS_VALUE_space_around;
		}
	}

	/** Get align-content as a CSSValue. */
	CSSValue GetAlignContentCSSValue() const
	{
		switch (GetAlignContent())
		{
		default: return CSS_VALUE_inherit;
		case ACONTENT_FLEX_START: return CSS_VALUE_flex_start;
		case ACONTENT_FLEX_END: return CSS_VALUE_flex_end;
		case ACONTENT_CENTER: return CSS_VALUE_center;
		case ACONTENT_SPACE_BETWEEN: return CSS_VALUE_space_between;
		case ACONTENT_SPACE_AROUND: return CSS_VALUE_space_around;
		case ACONTENT_STRETCH: return CSS_VALUE_stretch;
		}
	}

	/** Get align-items as a CSSValue. */
	CSSValue GetAlignItemsCSSValue() const
	{
		switch (GetAlignItems())
		{
		default: return CSS_VALUE_inherit;
		case AITEMS_FLEX_START: return CSS_VALUE_flex_start;
		case AITEMS_FLEX_END: return CSS_VALUE_flex_end;
		case AITEMS_CENTER: return CSS_VALUE_center;
		case AITEMS_BASELINE: return CSS_VALUE_baseline;
		case AITEMS_STRETCH: return CSS_VALUE_stretch;
		}
	}

	/** Get align-self as a CSSValue. */
	CSSValue GetAlignSelfCSSValue() const
	{
		switch (GetAlignSelf())
		{
		default: return CSS_VALUE_inherit;
		case ASELF_AUTO: return CSS_VALUE_auto;
		case ASELF_FLEX_START: return CSS_VALUE_flex_start;
		case ASELF_FLEX_END: return CSS_VALUE_flex_end;
		case ASELF_CENTER: return CSS_VALUE_center;
		case ASELF_BASELINE: return CSS_VALUE_baseline;
		case ASELF_STRETCH: return CSS_VALUE_stretch;
		}
	}

	/** Get flex-direction as a CSSValue. */
	CSSValue GetDirectionCSSValue() const
	{
		switch (GetDirection())
		{
		default: return CSS_VALUE_inherit;
		case DIR_ROW: return CSS_VALUE_row;
		case DIR_ROW_REVERSE: return CSS_VALUE_row_reverse;
		case DIR_COLUMN: return CSS_VALUE_column;
		case DIR_COLUMN_REVERSE: return CSS_VALUE_column_reverse;
		}
	}

	/** Get flex-wrap as a CSSValue. */
	CSSValue GetWrapCSSValue() const
	{
		switch (GetWrap())
		{
		default: return CSS_VALUE_inherit;
		case WRAP_NOWRAP: return CSS_VALUE_nowrap;
		case WRAP_WRAP: return CSS_VALUE_wrap;
		case WRAP_WRAP_REVERSE: return CSS_VALUE_wrap_reverse;
		}
	}

	CSSValue GetValue(CSSProperty property) const
	{
		switch (property)
		{
		case CSS_PROPERTY_justify_content:
			return GetJustifyContentCSSValue();
		case CSS_PROPERTY_align_content:
			return GetAlignContentCSSValue();
		case CSS_PROPERTY_align_items:
			return GetAlignItemsCSSValue();
		case CSS_PROPERTY_align_self:
			return GetAlignSelfCSSValue();
		case CSS_PROPERTY_flex_direction:
			return GetDirectionCSSValue();
		case CSS_PROPERTY_flex_wrap:
			return GetWrapCSSValue();
		default:
			OP_ASSERT(!"Unexpected property");
		}

		return CSS_VALUE_none;
	}

	void SetValue(CSSProperty property, CSSValue value)
	{
		switch (property)
		{
		case CSS_PROPERTY_justify_content:
			SetJustifyContentCSSValue(value); break;
		case CSS_PROPERTY_align_content:
			SetAlignContentCSSValue(value); break;
		case CSS_PROPERTY_align_items:
			SetAlignItemsCSSValue(value); break;
		case CSS_PROPERTY_align_self:
			SetAlignSelfCSSValue(value); break;
		case CSS_PROPERTY_flex_direction:
			SetDirectionCSSValue(value); break;
		case CSS_PROPERTY_flex_wrap:
			SetWrapCSSValue(value); break;
		default:
			OP_ASSERT(!"Unexpected property");
		}
	}

	/** Convert AlignItems to AlignSelf. */
	static AlignSelf ToAlignSelf(AlignItems align)
	{
		switch (align)
		{
		case AITEMS_FLEX_START:
			return ASELF_FLEX_START;
		case AITEMS_FLEX_END:
			return ASELF_FLEX_END;
		case AITEMS_CENTER:
			return ASELF_CENTER;
		case AITEMS_BASELINE:
			return ASELF_BASELINE;
		default:
			OP_ASSERT(!"Unexpected AlignItems value");
		case AITEMS_STRETCH:
			return ASELF_STRETCH;
		}
	}
};

extern const short CssFloatMap[];
extern const short CssDisplayMap[];
extern const short CssPositionMap[];

/** Type of break inside multicol containers. */

enum BREAK_TYPE
{
	COLUMN_BREAK,
	PAGE_BREAK,
	SPANNED_ELEMENT
};

/** Column or page breaking policy. */

enum BREAK_POLICY
{
	BREAK_ALLOW, ///< page/column break allowed if necessary
	BREAK_AVOID, ///< page/column break forbidden
	BREAK_ALWAYS, ///< page/column break mandatory
	BREAK_LEFT, ///< page break to the next left page (not used as a column breaking policy)
	BREAK_RIGHT ///< page break to the next right page (not used as a column breaking policy)
};

/** SpaceManager space vertical locking options. */

enum VerticalLock
{
	VLOCK_OFF = 0,			///< no lock - we may be pushed down below floats to get enough space
	VLOCK_KEEP_RIGHT_FIXED,	///< space locked verticaly; right aligned floats will not trim available space
	VLOCK_KEEP_LEFT_FIXED	///< space locked verticaly; left aligned floats will not trim available space
};

#endif //_LAYOUT_H_
