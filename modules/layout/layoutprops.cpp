/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/frm.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/content/content.h"
#include "modules/layout/content/flexcontent.h"
#include "modules/probetools/probepoints.h"
#include "modules/logdoc/attr_val.h"
#include "modules/logdoc/wmlenum.h"
#include "modules/display/style.h"
#include "modules/display/styl_man.h"
#include "modules/dochand/win.h"
#include "modules/style/css.h"
#include "modules/style/css_collection.h"

#include "modules/logdoc/urlimgcontprov.h"

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif

#ifdef _PRINT_SUPPORT_
# include "modules/dochand/win.h"
# include "modules/prefs/prefsmanager/collections/pc_print.h"
# include "modules/display/prn_dev.h"
#endif

#include "modules/forms/webforms2support.h"

#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediaelement.h"
#endif // MEDIA_HTML_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"

#include "modules/pi/ui/OpUiInfo.h"

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#define GET_CONSTRAINED_SHORT_FONT_SIZE(x) ConstrainFontHeight(LayoutFixedToShortNonNegative(x))

/** Constrain font height */

static inline short ConstrainFontHeight(short height)
{
	short min_font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize);
	short max_font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaxFontSize);

	short new_height = height;
	if (height < min_font_size)
		new_height = min_font_size;
	else if (height > max_font_size)
		new_height = max_font_size;

	return new_height;
}

/** Constrain the font size that is in layout fixed format
	by the min/max font size prefs.

	@param font_size The font size to constrain.
	@return The constrained font size. */

LayoutFixed ConstrainFixedFontSize(LayoutFixed font_size)
{
	LayoutFixed min_font_size = IntToLayoutFixed(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize));

	if (font_size < min_font_size)
		font_size = min_font_size;
	else
	{
		LayoutFixed max_font_size = IntToLayoutFixed(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaxFontSize));
		if (font_size > max_font_size)
			font_size = max_font_size;
	}

	return font_size;
}

const short CssTextAlignValueMap[] = {
	CSS_VALUE_left,
	CSS_VALUE_right,
	CSS_VALUE_justify,
	CSS_VALUE_center,
	CSS_VALUE_inherit,
	CSS_VALUE_default
};

const short CssTextTransformValueMap[] = {
	TEXT_TRANSFORM_NONE, //CSS_VALUE_none,
	TEXT_TRANSFORM_CAPITALIZE, //CSS_VALUE_capitalize,
	TEXT_TRANSFORM_UPPERCASE, //CSS_VALUE_uppercase,
	TEXT_TRANSFORM_LOWERCASE, //CSS_VALUE_lowercase,
	CSS_VALUE_inherit
};

const short CssFloatMap[] = {
	CSS_VALUE_none,
	CSS_VALUE_left,
	CSS_VALUE_right,
	CSS_VALUE__o_top,
	CSS_VALUE__o_bottom,
	CSS_VALUE__o_top_corner,
	CSS_VALUE__o_bottom_corner,
	CSS_VALUE__o_top_next_page,
	CSS_VALUE__o_bottom_next_page,
	CSS_VALUE__o_top_corner_next_page,
	CSS_VALUE__o_bottom_corner_next_page,
	CSS_VALUE_inherit
};

const short CssDisplayMap[] = {
	CSS_VALUE_none,
	CSS_VALUE_inline,
	CSS_VALUE_block,
	CSS_VALUE_inline_block,
	CSS_VALUE_list_item,
	CSS_VALUE_run_in,
	CSS_VALUE_compact,
	0, //CSS_VALUE_marker,
	CSS_VALUE_table,
	CSS_VALUE_inline_table,
	CSS_VALUE_table_row_group,
	CSS_VALUE_table_header_group,
	CSS_VALUE_table_footer_group,
	CSS_VALUE_table_row,
	CSS_VALUE_table_column_group,
	CSS_VALUE_table_column,
	CSS_VALUE_table_cell,
	CSS_VALUE_table_caption,
	CSS_VALUE__wap_marquee,
	CSS_VALUE_flex,
	CSS_VALUE_inline_flex,
	CSS_VALUE__webkit_box,
	CSS_VALUE__webkit_inline_box,
	CSS_VALUE_inherit
};

const short CssVisibilityMap[] = {
	CSS_VALUE_visible,
	CSS_VALUE_hidden,
	CSS_VALUE_collapse,
	CSS_VALUE_inherit
};

const short CssResizeMap[] = {
	CSS_VALUE_none,
	CSS_VALUE_both,
	CSS_VALUE_horizontal,
	CSS_VALUE_vertical,
	CSS_VALUE_inherit
};

const short CssWhiteSpaceMap[] = {
	CSS_VALUE_normal,
	CSS_VALUE_pre,
	CSS_VALUE_nowrap,
	CSS_VALUE_pre_wrap,
	CSS_VALUE_inherit,
	0,
	CSS_VALUE_pre_line
};

const short CssWordWrapMap[] = {
	CSS_VALUE_normal,
	CSS_VALUE_break_word,
	CSS_VALUE_inherit,
	0
};

const short CssObjectFitMap[] = {
	CSS_VALUE_fill,
	CSS_VALUE_contain,
	CSS_VALUE_cover,
	CSS_VALUE_none,
	CSS_VALUE_auto,
	CSS_VALUE_inherit
};

const short CssClearMap[] = {
	CSS_VALUE_none,
	CSS_VALUE_left,
	CSS_VALUE_right,
	CSS_VALUE_both,
	CSS_VALUE_inherit
};

const short CssBoxSizingMap[] = {
	CSS_VALUE_content_box,
	CSS_VALUE_border_box,
	CSS_VALUE_inherit
};

const short CssOverflowMap[] = {
	CSS_VALUE_visible,
	CSS_VALUE_hidden,
	CSS_VALUE_scroll,
	CSS_VALUE_auto,
	CSS_VALUE__o_paged_x_controls,
	CSS_VALUE__o_paged_x,
	CSS_VALUE__o_paged_y_controls,
	CSS_VALUE__o_paged_y,
	CSS_VALUE_inherit
};

const short CssPositionMap[] = {
	CSS_VALUE_static,
	CSS_VALUE_relative,
	CSS_VALUE_absolute,
	CSS_VALUE_fixed,
	CSS_VALUE_inherit
};

const short CssFontWeightMap[] = {
	CSS_VALUE_inherit,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	CSS_VALUE_bolder,
	CSS_VALUE_lighter,
	7, // bold
	4 // normal
};

const short CssFontStyleMap[] = {
	FONT_STYLE_INHERIT,
	FONT_STYLE_NORMAL,
	FONT_STYLE_ITALIC,
	FONT_STYLE_ITALIC //FONT_STYLE_OBLIQUE
};

const short CssFontVariantMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_normal,
	CSS_VALUE_small_caps
};

const short CssTableLayoutMap[] = {
	CSS_VALUE_auto,
	CSS_VALUE_fixed,
	CSS_VALUE_inherit
};

const short CssCaptionSideMap[] = {
	CSS_VALUE_top,
	CSS_VALUE_bottom,
	CSS_VALUE_left,
	CSS_VALUE_right,
	CSS_VALUE_inherit
};

const short CssBorderCollapseMap[] = {
	CSS_VALUE_collapse,
	CSS_VALUE_separate,
	CSS_VALUE_inherit
};

const short CssEmptyCellsMap[] = {
	CSS_VALUE_show,
	CSS_VALUE_hide,
	CSS_VALUE_inherit
};

const short CssDirectionMap[] = {
	CSS_VALUE_ltr,
	CSS_VALUE_rtl,
	CSS_VALUE_inherit
};

const short CssUnicodeBidiMap[] = {
	CSS_VALUE_normal,
	CSS_VALUE_embed,
	CSS_VALUE_bidi_override,
	CSS_VALUE_inherit
};

const short CssListStylePosMap[] = {
	CSS_VALUE_inside,
	CSS_VALUE_outside,
	CSS_VALUE_inherit
};

const short CssListStyleTypeMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_disc,
	CSS_VALUE_circle,
	CSS_VALUE_square,
	CSS_VALUE_box,
	CSS_VALUE_decimal,
	CSS_VALUE_decimal_leading_zero,
	CSS_VALUE_lower_roman,
	CSS_VALUE_upper_roman,
	CSS_VALUE_lower_greek,
	CSS_VALUE_lower_alpha,
	CSS_VALUE_lower_latin,
	CSS_VALUE_upper_alpha,
	CSS_VALUE_upper_latin,
	CSS_VALUE_hebrew,
	CSS_VALUE_armenian,
	CSS_VALUE_georgian,
	CSS_VALUE_cjk_ideographic,
	CSS_VALUE_hiragana,
	CSS_VALUE_katakana,
	CSS_VALUE_hiragana_iroha,
	CSS_VALUE_katakana_iroha,
	CSS_VALUE_none
};

const short CssVerticalAlignMap[] = {
	CSS_VALUE_baseline,
	CSS_VALUE_top,
	CSS_VALUE_bottom,
	CSS_VALUE_sub,
	CSS_VALUE_super,
	CSS_VALUE_text_top,
	CSS_VALUE_middle,
	CSS_VALUE_text_bottom,
	CSS_VALUE_inherit
};

const short CssBgAttachMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_scroll,
	CSS_VALUE_fixed
};

const short CssBgRepeatMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_repeat,
	CSS_VALUE_repeat_x,
	CSS_VALUE_repeat_y,
	CSS_VALUE_no_repeat
};

const short CssBorderStyleMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_none,
	CSS_VALUE_hidden,
	CSS_VALUE_dotted,
	CSS_VALUE_dashed,
	CSS_VALUE_solid,
	CSS_VALUE_double,
	CSS_VALUE_groove,
	CSS_VALUE_ridge,
	CSS_VALUE_inset,
	CSS_VALUE_outset,
	CSS_VALUE__o_highlight_border
};

const short CssColumnFillMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_auto,
	CSS_VALUE_balance
};

const short CssPageBreakMap[] = {
	CSS_VALUE_inherit,
	CSS_VALUE_auto,
	CSS_VALUE_avoid,
	CSS_VALUE_avoid_page,
	CSS_VALUE_avoid_column,
	CSS_VALUE_always,
	CSS_VALUE_left,
	CSS_VALUE_right,
	CSS_VALUE_page,
	CSS_VALUE_column,
};

const short CssWapMarqueeDirMap[] = {
	ATTR_VALUE_left,
	ATTR_VALUE_right,
	ATTR_VALUE_up,
	ATTR_VALUE_down
};

const short CssWapMarqueeStyleMap[] = {
	CSS_VALUE_scroll,
	CSS_VALUE_slide,
	CSS_VALUE_alternate
};

const short CssWapMarqueeSpeedMap[] = {
	150,
	85,
	40
};

const short CssBoxDecorationBreakMap[] = {
	CSS_VALUE_slice,
	CSS_VALUE_clone,
	CSS_VALUE_inherit,
};

#ifdef CSS_MINI_EXTENSIONS
const short CssMiniFoldMap[] = {
	CSS_VALUE_none,
	CSS_VALUE_folded,
	CSS_VALUE_unfolded
};
#endif // CSS_MINI_EXTENSIONS

/* This pool has two purposes: First, it takes some load off the malloc,
   because these elements are large and large elements can be hard
   to allocate.  Second, because there are very few such elements but
   they are heavily used, it provides them with improved locality on
   systems with tiny caches and poor mallocs (Nokia/Symbian type things).

   I've never seen more than 13 elements used on real sites, but I haven't
   tested very widely. */

void*
HTMLayoutProperties::operator new(size_t nbytes) OP_NOTHROW
{
	return g_opera->layout_module.htm_layout_properties_pool.New(sizeof(HTMLayoutProperties));
}

void
HTMLayoutProperties::operator delete(void *p, size_t nbytes)
{
	g_opera->layout_module.htm_layout_properties_pool.Delete(p);
}

/* static */ PixelOrLayoutFixed
HTMLayoutProperties::GetLengthInPixelsFromProp(int value_type,
											   long value,
											   BOOL is_percentage,
											   BOOL is_decimal,
											   const CSSLengthResolver& length_resolver,
											   int min_value,
											   int max_value
#ifdef CURRENT_STYLE_SUPPORT
											   , short* use_type /* = NULL */
#endif // CURRENT_STYLE_SUPPORT
												)
{
	if (!value
#ifdef CURRENT_STYLE_SUPPORT
		&& !use_type
#endif
		)
		return 0;

	short vtype = CSS_PERCENTAGE;

	if (!is_percentage)
	{
		switch (value_type)
		{
		case CSS_LENGTH_number:
			vtype = CSS_NUMBER;
			break;

		case CSS_LENGTH_em:
			vtype = CSS_EM;
			break;

		case CSS_LENGTH_rem:
			vtype = CSS_REM;
			break;

		case CSS_LENGTH_px:
			vtype = CSS_PX;

#ifdef CURRENT_STYLE_SUPPORT
			if (!use_type)
#endif // CURRENT_STYLE_SUPPORT
			{
				if (is_decimal)
				{
					if (!length_resolver.GetResultInLayoutFixed())
						value = LayoutFixedToIntRoundDown(LayoutFixed(int(value)));
				}
				else
					if (length_resolver.GetResultInLayoutFixed())
						value = CONSTRAIN_LAYOUT_FIXED_AS_INT(value) * LAYOUT_FIXED_POINT_BASE;

				if (value < min_value)
					value = min_value;
				else
					if (value > max_value)
						value = max_value;
				return int(value);
			}
			break;

		case CSS_LENGTH_ex:
			vtype = CSS_EX;
			break;

		case CSS_LENGTH_cm:
			vtype = CSS_CM;
			break;

		case CSS_LENGTH_mm:
			vtype = CSS_MM;
			break;

		case CSS_LENGTH_in:
			vtype = CSS_IN;
			break;

		case CSS_LENGTH_pt:
			vtype = CSS_PT;
			break;

		case CSS_LENGTH_pc:
			vtype = CSS_PC;
			break;

		case CSS_LENGTH_keyword_px:
#ifdef CURRENT_STYLE_SUPPORT
			if (use_type)
				vtype = CSS_IDENT;
			else
#endif // CURRENT_STYLE_SUPPORT
				vtype = CSS_PX;
			break;
		}
	}

#ifdef CURRENT_STYLE_SUPPORT
	// if use_type is set we don't want the calculated value
	if (use_type)
	{
		*use_type = vtype;

		if (is_decimal)
		{
			if (!length_resolver.GetResultInLayoutFixed())
				return LayoutFixedToIntRoundDown(LayoutFixed(int(value)));
		}
		else
			if (length_resolver.GetResultInLayoutFixed())
				return CONSTRAIN_LAYOUT_FIXED_AS_INT(int(value)) * LAYOUT_FIXED_POINT_BASE;

		return int(value);
	}
#endif // CURRENT_STYLE_SUPPORT

	float val = float(value);
	if (is_decimal)
		val /= LAYOUT_FIXED_POINT_BASE;

	int len = length_resolver.GetLengthInPixels(val, vtype);

	if (len < min_value)
		len = min_value;
	else
		if (len > max_value)
			len = max_value;

	return len;
}

/* static */ LayoutCoord
HTMLayoutProperties::GetClipValue(CSS_number4_decl* clip_value, int index, const CSSLengthResolver& length_resolver
#ifdef CURRENT_STYLE_SUPPORT
								  , short* use_type
#endif // CURRENT_STYLE_SUPPORT
	)
{
	if (clip_value->GetValueType(index) != CSS_VALUE_auto)
	{
		short vtype = clip_value->GetValueType(index);

#ifdef CURRENT_STYLE_SUPPORT
		if (use_type)
		{
			*use_type = vtype;
			return LayoutCoord(int(clip_value->GetNumberValue(index)));
		}
#endif // CURRENT_STYLE_SUPPORT

		if (vtype != CSS_PERCENTAGE)
		{
			return length_resolver.GetLengthInPixels(clip_value->GetNumberValue(index), vtype);
		}
	}

	return CLIP_AUTO;
}

#ifdef PAGED_MEDIA_SUPPORT

/* static */ void
HTMLayoutProperties::GetPageMargins(LayoutWorkplace* workplace,
									CSS_Properties* page_props,
									LayoutCoord cb_width,
									LayoutCoord cb_height,
									BOOL allow_width_and_height,
									LayoutCoord& margin_top,
									LayoutCoord& margin_right,
									LayoutCoord& margin_bottom,
									LayoutCoord& margin_left)
{
	VisualDevice* vd = workplace->GetFramesDocument()->GetVisualDevice();
	CSSLengthResolver length_resolver(vd);
	CSS_decl* cp;
	LayoutFixed root_font_size = workplace->GetDocRootProperties().GetRootFontSize();

	length_resolver.SetPercentageBase(float(cb_width));
	length_resolver.SetFontSize(root_font_size);
	length_resolver.SetRootFontSize(root_font_size);

	cp = page_props->GetDecl(CSS_PROPERTY_margin_left);
	if (cp)
		if (cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			margin_left = LayoutCoord((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));
			margin_left = LayoutCoord(MIN(SHRT_MAX, MAX(SHRT_MIN, int(margin_left))));
		}
		else
			if (allow_width_and_height && cp->GetDeclType() == CSS_DECL_TYPE)
				if (cp->TypeValue() == CSS_VALUE_auto)
					margin_left = LAYOUT_COORD_MIN;

	cp = page_props->GetDecl(CSS_PROPERTY_margin_right);
	if (cp)
		if (cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			margin_right = LayoutCoord((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));
			margin_right = LayoutCoord(MIN(SHRT_MAX, MAX(SHRT_MIN, int(margin_right))));
		}
		else
			if (allow_width_and_height && cp->GetDeclType() == CSS_DECL_TYPE)
				if (cp->TypeValue() == CSS_VALUE_auto)
					margin_right = LAYOUT_COORD_MIN;

	length_resolver.SetPercentageBase(float(cb_height));

	cp = page_props->GetDecl(CSS_PROPERTY_margin_top);
	if (cp)
		if (cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			margin_top = LayoutCoord((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));
			margin_top = LayoutCoord(MIN(SHRT_MAX, MAX(SHRT_MIN, int(margin_top))));
		}
		else
			if (allow_width_and_height && cp->GetDeclType() == CSS_DECL_TYPE)
				if (cp->TypeValue() == CSS_VALUE_auto)
					margin_top = LAYOUT_COORD_MIN;

	cp = page_props->GetDecl(CSS_PROPERTY_margin_bottom);
	if (cp)
		if (cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			margin_bottom = LayoutCoord((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));
			margin_bottom = LayoutCoord(MIN(SHRT_MAX, MAX(SHRT_MIN, int(margin_bottom))));
		}
		else
			if (allow_width_and_height && cp->GetDeclType() == CSS_DECL_TYPE)
				if (cp->TypeValue() == CSS_VALUE_auto)
					margin_bottom = LAYOUT_COORD_MIN;

	if (allow_width_and_height)
	{
		LayoutCoord page_area_width;
		LayoutCoord page_area_height;

		length_resolver.SetPercentageBase(float(cb_width));

		cp = page_props->GetDecl(CSS_PROPERTY_width);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
			page_area_width = LayoutCoord((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));
		else
		{
			page_area_width = cb_width;

			if (margin_left != LAYOUT_COORD_MIN)
				page_area_width -= margin_left;

			if (margin_right != LAYOUT_COORD_MIN)
				page_area_width -= margin_right;
		}

		cp = page_props->GetDecl(CSS_PROPERTY_max_width);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			LayoutCoord max_width((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));

			if (page_area_width > max_width)
				page_area_width = max_width;
		}

		cp = page_props->GetDecl(CSS_PROPERTY_min_width);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			LayoutCoord min_width((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));

			if (page_area_width < min_width)
				page_area_width = min_width;
		}

		length_resolver.SetPercentageBase(float(cb_height));

		cp = page_props->GetDecl(CSS_PROPERTY_height);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
			page_area_height = LayoutCoord((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));
		else
		{
			page_area_height = cb_height;

			if (margin_top != LAYOUT_COORD_MIN)
				page_area_height -= margin_top;

			if (margin_bottom != LAYOUT_COORD_MIN)
				page_area_height -= margin_bottom;
		}

		cp = page_props->GetDecl(CSS_PROPERTY_max_height);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			LayoutCoord max_height((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));

			if (page_area_height > max_height)
				page_area_height = max_height;
		}

		cp = page_props->GetDecl(CSS_PROPERTY_min_height);
		if (cp && cp->GetDeclType() == CSS_DECL_NUMBER)
		{
			LayoutCoord min_height((int) length_resolver.GetLengthInPixels(cp->GetNumberValue(0), ((CSS_number_decl*)cp)->GetValueType(0)));

			if (page_area_height < min_height)
				page_area_height = min_height;
		}

		/* Resolve auto margins and over-constrainedness. Follow (CSS 2.1) 10.3.3
		   (Block-level, non-replaced elements in normal flow). For vertical dimensions,
		   'margin-bottom' will be the value dropped if the equation is
		   over-constrained. */

		LayoutCoord extra_space = cb_width - page_area_width;

#ifdef SUPPORT_TEXT_DIRECTION
		if (workplace->IsRtlDocument())
		{
			if (margin_right == LAYOUT_COORD_MIN)
				if (margin_left == LAYOUT_COORD_MIN)
					margin_right = extra_space / LayoutCoord(2);
				else
					margin_right = extra_space;

			margin_left = extra_space - margin_right;
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			if (margin_left == LAYOUT_COORD_MIN)
				if (margin_right == LAYOUT_COORD_MIN)
					margin_left = extra_space / LayoutCoord(2);
				else
					margin_left = extra_space;

			margin_right = extra_space - margin_left;
		}

		extra_space = cb_height - page_area_height;

		if (margin_top == LAYOUT_COORD_MIN)
			if (margin_bottom == LAYOUT_COORD_MIN)
				margin_top = extra_space / LayoutCoord(2);
			else
				margin_top = extra_space;

		margin_bottom = extra_space - margin_top;
	}
}

#endif // PAGED_MEDIA_SUPPORT

void
Border::Reset()
{
	top.Reset();
	left.Reset();
	right.Reset();
	bottom.Reset();
}

BOOL
HTMLayoutProperties::ExtractBorderImage(FramesDocument *doc, CSSLengthResolver& length_resolver,
										CSS_decl *decl, BorderImage &border_image, Border &border) const
{
    OP_ASSERT(decl != NULL);

    /* Assumes the style module has constructed a CSS_generic_value
       array of variable length matching the expression given by the
       user. */

    if (decl->GetDeclType() != CSS_DECL_GEN_ARRAY)
        /* No border image present. */

        return FALSE;

    const CSS_generic_value* arr = decl->GenArrayValue();
    int arr_len = decl->ArrayValueLen();
    int i = 0;

    OP_ASSERT(arr_len > 1);

    /* First must be a url or gradient function */
    OP_ASSERT(arr[i].GetValueType() == CSS_FUNCTION_URL || arr[i].GetValueType() == CSS_FUNCTION_LINEAR_GRADIENT ||
			  arr[i].GetValueType() == CSS_FUNCTION_REPEATING_LINEAR_GRADIENT || arr[i].GetValueType() == CSS_FUNCTION_RADIAL_GRADIENT ||
			  arr[i].GetValueType() == CSS_FUNCTION_REPEATING_RADIAL_GRADIENT ||
			  arr[i].GetValueType() == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT ||
			  arr[i].GetValueType() == CSS_FUNCTION_O_LINEAR_GRADIENT);

	if (arr[i].GetValueType() == CSS_FUNCTION_URL)
		border_image.border_img.Set(arr[i++].GetString());
#ifdef CSS_GRADIENT_SUPPORT
	else
		border_image.border_gradient = arr[i++].GetGradient();
#endif // CSS_GRADIENT_SUPPORT

    /* Second must be a cut */

    while (i < arr_len && (arr[i].GetValueType() == CSS_PERCENTAGE || arr[i].GetValueType() == CSS_NUMBER))
    {
        if (arr[i].GetValueType() == CSS_NUMBER)
            border_image.cut[i-1] = (int) arr[i].GetReal();
        else
            border_image.cut[i-1] = -(int) arr[i].GetReal();

        i++;
    }

    /* Complete cuts */
    if (i < 3) border_image.cut[1] = border_image.cut[0];
    if (i < 4) border_image.cut[2] = border_image.cut[0];
    if (i < 5) border_image.cut[3] = border_image.cut[1];

	/* FIXME: Check if the image is loaded and decoded. */

    BOOL image_can_be_displayed = doc->GetShowImages();

    if (i < arr_len && arr[i].GetValueType() == CSS_SLASH)
    {
        /* Overriding border widths present */

        const int widths_start = ++i;
        short *width[] = { &border.top.width, &border.right.width, &border.bottom.width, &border.left.width };

		BOOL horizontal = FALSE;
		int containing_size = containing_block_height;

        while (i < arr_len)
        {
			if (arr[i].GetValueType() == CSS_IDENT && (arr[i].GetType() == CSS_VALUE_stretch ||
													   arr[i].GetType() == CSS_VALUE_repeat ||
													   arr[i].GetType() == CSS_VALUE_round))
				break;

            if (image_can_be_displayed)
            {
                if (arr[i].GetValueType() == CSS_IDENT)
                {
                    switch(arr[i].GetType())
                    {
                    case CSS_VALUE_medium: *width[i-widths_start] = CSS_BORDER_WIDTH_MEDIUM; break;
                    case CSS_VALUE_thin: *width[i-widths_start] = CSS_BORDER_WIDTH_THIN; break;
                    case CSS_VALUE_thick: *width[i-widths_start] = CSS_BORDER_WIDTH_THICK; break;
                    }
                }
                else
					*width[i-widths_start] = static_cast<short>
						(length_resolver.ChangePercentageBase(float(containing_size)).GetLengthInPixels(arr[i].GetReal(), arr[i].GetValueType()));
            }

			horizontal = !horizontal;
			containing_size = horizontal ? containing_block_width : containing_block_height;
            i++;
        }

		const int width_count = i - widths_start;
		if (width_count > 0)
		{
			/* Complete widths */
			if (width_count < 2)
				*width[1] = *width[0];
			if (width_count < 3)
				*width[2] = *width[0];
			if (width_count < 4)
				*width[3] = *width[1];
		}

		border_image.has_border_widths = TRUE;
    }

    if (i < arr_len && arr[i].GetValueType() == CSS_IDENT)
    {
        border_image.scale[0] = arr[i++].GetType();

        if (i < arr_len && arr[i].GetValueType() == CSS_IDENT)
            border_image.scale[1] = arr[i++].GetType();
        else
            border_image.scale[1] = border_image.scale[0];
    }

	return TRUE;
}

int
GetBorderStylePriority(short style)
{
	switch (style)
	{
	case CSS_VALUE_double:
		return 8;
	case CSS_VALUE_solid:
		return 7;
	case CSS_VALUE_dashed:
		return 6;
	case CSS_VALUE_dotted:
		return 5;
	case CSS_VALUE_ridge:
		return 4;
	case CSS_VALUE_outset:
		return 3;
	case CSS_VALUE_groove:
		return 2;
	case CSS_VALUE_inset:
		return 1;
	default:
		return 0;
	}
}

void
BorderEdge::Collapse(const BorderEdge& other_edge)
{
	if (style == BORDER_STYLE_NOT_SET)
	{
		if (other_edge.style == BORDER_STYLE_NOT_SET)
		{
			style = CSS_VALUE_none;
			width = 0;
		}
		else
			*this = other_edge;
	}
	else
		if (other_edge.style != BORDER_STYLE_NOT_SET)
		{
			if (style == CSS_VALUE_hidden || other_edge.style == CSS_VALUE_hidden)
			{
				style = CSS_VALUE_hidden;
				width = 0;
			}
			else
				if (style == CSS_VALUE_none && other_edge.style == CSS_VALUE_none)
				{
					style = CSS_VALUE_none;
					width = 0;
				}
				else
					if (other_edge.width > width ||
						(other_edge.width == width && GetBorderStylePriority(other_edge.style) > GetBorderStylePriority(style)))
						*this = other_edge;
		}
}

void
BorderEdge::ResolveRadiusPercentages(LayoutCoord percentage_of)
{
	if (radius_start < 0)
	{
		if (packed.radius_start_is_decimal)
			radius_start = short((- radius_start * percentage_of) / 10000);
		else
			radius_start = short((- radius_start * percentage_of) / 100);
	}

	if (radius_end < 0)
	{
		if (packed.radius_end_is_decimal)
			radius_end = short((- radius_end * percentage_of) / 10000);
		else
			radius_end = short((- radius_end * percentage_of) / 100);
	}
}

#ifdef CURRENT_STYLE_SUPPORT
void
HTMLayoutPropertiesTypes::Reset(HTMLayoutPropertiesTypes *parent_types)
{
	m_top = CSS_IDENT;
	m_left = CSS_IDENT;
	m_right = CSS_IDENT;
	m_bottom = CSS_IDENT;

	m_margin_top = CSS_PX;
	m_margin_left = CSS_PX;
	m_margin_right = CSS_PX;
	m_margin_bottom = CSS_PX;

	m_padding_top = CSS_PX;
	m_padding_left = CSS_PX;
	m_padding_right = CSS_PX;
	m_padding_bottom = CSS_PX;

	m_min_width = CSS_PX;
	m_max_width = CSS_IDENT;
	m_min_height = CSS_PX;
	m_max_height = CSS_IDENT;

	m_brd_top_width = CSS_IDENT;
	m_brd_left_width = CSS_IDENT;
	m_brd_right_width = CSS_IDENT;
	m_brd_bottom_width = CSS_IDENT;

	m_outline_width = CSS_IDENT;
	m_outline_offset = CSS_IDENT;

	m_width = CSS_PX;
	m_height = CSS_PX;

	m_vertical_align = CSS_IDENT;

	m_bg_xpos = CSS_PX;
	m_bg_ypos = CSS_PX;

	m_bg_xsize = CSS_IDENT;
	m_bg_ysize = CSS_IDENT;

	m_clip_top = CSS_IDENT;
	m_clip_right = CSS_IDENT;
	m_clip_bottom = CSS_IDENT;
	m_clip_left = CSS_IDENT;

	m_column_width = CSS_PX;
	m_column_gap = CSS_IDENT;
	m_column_rule_width = CSS_IDENT;

	m_flex_basis = CSS_IDENT;

	if (parent_types)
	{
		m_line_height = parent_types->m_line_height;
		m_font_weight = parent_types->m_font_weight;
		m_font_size = parent_types->m_font_size;

		m_brd_spacing_hor = parent_types->m_brd_spacing_hor;
		m_brd_spacing_ver = parent_types->m_brd_spacing_ver;

		m_text_indent = parent_types->m_text_indent;

		m_word_spacing = parent_types->m_word_spacing;
		m_letter_spacing = parent_types->m_letter_spacing;
	}
	else
	{
		m_line_height = CSS_IDENT;
		m_font_weight = CSS_IDENT;
		m_font_size = CSS_PX;

		m_brd_spacing_hor = CSS_PX;
		m_brd_spacing_ver = CSS_PX;

		m_text_indent = CSS_PX;

		m_word_spacing = CSS_PX;
		m_letter_spacing = CSS_PX;
	}
}
#endif // CURRENT_STYLE_SUPPORT

void
HTMLayoutProperties::Reset(const HTMLayoutProperties* parent_hlp /*=NULL*/, HLDocProfile* hld_profile /*=NULL*/
#ifdef CURRENT_STYLE_SUPPORT
						   , BOOL use_types/*=FALSE*/
#endif // CURRENT_STYLE_SUPPORT
						   )
{
#ifdef CURRENT_STYLE_SUPPORT
	if (!use_types)
		types = NULL;
	else if (types)
		types->Reset(parent_hlp ? parent_hlp->types : NULL);
#endif // CURRENT_STYLE_SUPPORT

	// First: Properties that aren't really CSS properties
	// Reset this first because some real CSS properties also use info and
	// will be overwritten below.
	info_init = 0;
	info2_init = 0;

	navup_cp = NULL;
	navdown_cp = NULL;
	navleft_cp = NULL;
	navright_cp = NULL;

	// Here come the real CSS properties

	align_block_elements	= 0;
	text_decoration			= TEXT_DECORATION_NONE;
	vertical_align			= 0;
	vertical_align_type		= CSS_VALUE_baseline;
	float_type				= CSS_VALUE_none;
	display_type			= CSS_VALUE_inline;
	clear					= CSS_VALUE_none;
	resize					= CSS_VALUE_none;

	position	= CSS_VALUE_static;
	top			= VPOSITION_VALUE_AUTO;
	left		= HPOSITION_VALUE_AUTO;
	right		= HPOSITION_VALUE_AUTO;
	bottom		= VPOSITION_VALUE_AUTO;
	z_index		= CSS_ZINDEX_auto;
	table_baseline = 1;
	computed_overflow_x = CSS_VALUE_visible;
	computed_overflow_y = CSS_VALUE_visible;
	overflow_x = CSS_VALUE_visible;
	overflow_y = CSS_VALUE_visible;

	opacity = 255;

	text_overflow = CSS_VALUE_clip;

	table_layout	= CSS_VALUE_auto;
	margin_top		= LayoutCoord(0);
	margin_left		= LayoutCoord(0);
	margin_right	= LayoutCoord(0);
	margin_bottom	= LayoutCoord(0);
	padding_top		= LayoutCoord(0);
	padding_left	= LayoutCoord(0);
	padding_right	= LayoutCoord(0);
	padding_bottom	= LayoutCoord(0);

#ifdef CSS_TRANSFORMS
	transform_origin_x = LayoutCoord(50);
	transform_origin_y = LayoutCoord(50);
	info2.transform_origin_x_is_percent = 1;
	info2.transform_origin_y_is_percent = 1;
#endif

	object_fit_position.fit = CSS_VALUE_auto;
	object_fit_position.x = LayoutFixedAsLayoutCoord(IntToLayoutFixed(OBJECT_POSITION_INITIAL));
	object_fit_position.y = LayoutFixedAsLayoutCoord(IntToLayoutFixed(OBJECT_POSITION_INITIAL));
	object_fit_position.x_percent = TRUE;
	object_fit_position.y_percent = TRUE;

	if (hld_profile)
	{
#ifdef _PRINT_SUPPORT_
		VisualDevice* vd = hld_profile->GetVisualDevice();

		if (vd->IsPrinter())
		{
			if (hld_profile->GetFramesDocument()->IsInlineFrame())
			{
				content_width = LayoutCoord(hld_profile->GetFramesDocument()->GetDocManager()->GetFrame()->GetWidth());
				content_height = LayoutCoord(hld_profile->GetFramesDocument()->GetDocManager()->GetFrame()->GetHeight());

			}
			else
			{
				PageDescription* page = hld_profile->GetFramesDocument()->GetCurrentPage();

				content_width = LayoutCoord(page->GetPageWidth());
				content_height = LayoutCoord(page->GetPageHeight());
			}
		}
		else
#endif // _PRINT_SUPPORT_
		{
			if (hld_profile->GetFramesDocument()->IsInlineFrame())
			{
				content_width = LayoutCoord(hld_profile->GetFramesDocument()->GetDocManager()->GetFrame()->GetWidth());
				content_height = LayoutCoord(hld_profile->GetFramesDocument()->GetDocManager()->GetFrame()->GetHeight());
			}
			else
			{
				FramesDocument* fdoc = hld_profile->GetFramesDocument();

				if (fdoc &&
					fdoc->GetSmartFrames() &&
					fdoc->GetDocManager()->GetFrame() &&
					!fdoc->GetDocManager()->GetFrame()->IsInlineFrame())
				{
					content_width = LayoutCoord(hld_profile->GetFramesDocument()->GetDocManager()->GetFrame()->GetNormalWidth());
					content_height = LayoutCoord(hld_profile->GetFramesDocument()->GetDocManager()->GetFrame()->GetNormalHeight());
				}
				else
				{
		 			content_width = hld_profile->GetLayoutWorkplace()->GetLayoutViewWidth();
 					content_height = hld_profile->GetLayoutWorkplace()->GetLayoutViewHeight();
				}
			}
		}

		containing_block_width	= content_width;
		containing_block_height	= content_height;
	}
	else
	{
		content_width	= CONTENT_WIDTH_AUTO;
		content_height	= CONTENT_HEIGHT_AUTO;
		containing_block_width	= LayoutCoord(0);
		containing_block_height	= LayoutCoord(0);
	}

	box_sizing	= CSS_VALUE_content_box;
	box_decoration_break = CSS_VALUE_slice;

	min_width	= CONTENT_SIZE_AUTO;
	max_width	= LayoutCoord(-1);
	normal_max_width = LayoutCoord(-1);
	min_height	= CONTENT_SIZE_AUTO;
	max_height	= LayoutCoord(-1);

	border.SetDefault();

	outline.SetOutlineDefault();
	outline_offset = 0;

	column_rule.SetDefault();
	column_width = CONTENT_WIDTH_AUTO;
	column_count = SHRT_MIN;
	column_gap = CONTENT_WIDTH_AUTO; // "normal"
	column_fill = CSS_VALUE_balance;
	column_span = 1;

	flexbox_modes.Reset();
	order = 0;
	flex_grow = 0.0;
	flex_shrink = 1.0;
	flex_basis = CONTENT_SIZE_AUTO;

	clip_left	= CLIP_NOT_SET;
	clip_right	= CLIP_NOT_SET;
	clip_top	= CLIP_NOT_SET;
	clip_bottom	= CLIP_NOT_SET;

	indent_count = 0;
	font_size_base = 16;

	border_image.Reset();

	content_cp			= NULL;
	counter_reset_cp	= NULL;
	counter_inc_cp		= NULL;

	page_props.break_before	= CSS_VALUE_auto;
	page_props.break_after	= CSS_VALUE_auto;

#ifdef SUPPORT_TEXT_DIRECTION
	unicode_bidi = CSS_VALUE_normal;
#endif // SUPPORT_TEXT_DIRECTION

#ifdef _CSS_LINK_SUPPORT_
	set_link_source_cp	= NULL;
	use_link_source		= CSS_VALUE_none;
#endif

	marquee_dir		= ATTR_VALUE_left;
	marquee_loop	= 1;
	marquee_scrolldelay	= 85;
	marquee_style	= CSS_VALUE_scroll;

#ifdef CSS_MINI_EXTENSIONS
	mini_fold = CSS_VALUE_none;
	focus_opacity = 255;
#endif // CSS_MINI_EXTENSIONS

#if defined(SVG_SUPPORT)
	if (svg != NULL)
		svg->Reset(NULL);
#endif // SVG_SUPPORT

	bg_color = USE_DEFAULT_COLOR;
	bg_images.Reset();

#ifdef CSS_TRANSFORMS
	transforms_cp = NULL;
#endif

	box_shadows.Reset();

#ifdef ACCESS_KEYS_SUPPORT
	accesskey_cp = NULL;
#endif // ACCESS_KEYS_SUPPORT

#ifdef CSS_TRANSITIONS
	transition_delay = NULL;
	transition_duration = NULL;
	transition_timing = NULL;
	transition_property = NULL;
#endif // CSS_TRANSITIONS

#ifdef CSS_ANIMATIONS
	animation_name = NULL;
	animation_duration = NULL;
	animation_delay = NULL;
	animation_direction = NULL;
	animation_iteration_count = NULL;
	animation_fill_mode = NULL;
	animation_timing_function = NULL;
	animation_play_state = NULL;
#endif // CSS_ANIMATIONS

	// Set the inheritable properties
	if (parent_hlp)
	{
		line_height_i	= parent_hlp->line_height_i;
		text_transform	= parent_hlp->text_transform;
		text_indent		= parent_hlp->text_indent;
		text_align		= parent_hlp->text_align;
		small_caps		= parent_hlp->small_caps;

		letter_spacing	= parent_hlp->letter_spacing;
		word_spacing_i	= parent_hlp->word_spacing_i;

		list_style_type	= parent_hlp->list_style_type;
		list_style_pos	= parent_hlp->list_style_pos;
		list_style_image= parent_hlp->list_style_image;

#ifdef CSS_GRADIENT_SUPPORT
		list_style_image_gradient = parent_hlp->list_style_image_gradient;
#endif // CSS_GRADIENT_SUPPORT

		visibility		= parent_hlp->visibility;
		white_space		= parent_hlp->white_space;
		overflow_wrap	= parent_hlp->overflow_wrap;

		overline_color	= parent_hlp->overline_color;
		linethrough_color	= parent_hlp->linethrough_color;
		underline_color	= parent_hlp->underline_color;
		textdecoration_baseline = parent_hlp->textdecoration_baseline;

		selection_color	= parent_hlp->selection_color;
		selection_bgcolor	= parent_hlp->selection_bgcolor;

		font_color		= parent_hlp->font_color;
		font_weight		= parent_hlp->font_weight;
		font_italic		= parent_hlp->font_italic;
		font_number		= parent_hlp->font_number;
		font_size		= parent_hlp->font_size;
		decimal_font_size_constrained = parent_hlp->decimal_font_size_constrained;
		decimal_font_size	= parent_hlp->decimal_font_size;
		unflex_font_size	= parent_hlp->unflex_font_size;

		border_collapse	= parent_hlp->border_collapse;
		border_spacing_horizontal	= parent_hlp->border_spacing_horizontal;
		border_spacing_vertical		= parent_hlp->border_spacing_vertical;
		empty_cells		= parent_hlp->empty_cells;
		table_rules		= parent_hlp->table_rules;
		caption_side	= parent_hlp->caption_side;

		indent_count	= parent_hlp->indent_count;
		font_size_base	= parent_hlp->font_size_base;

		quotes_cp		= parent_hlp->quotes_cp;
#ifdef GADGET_SUPPORT
        control_region_cp = parent_hlp->control_region_cp;
#endif // GADGET_SUPPORT
		text_shadows.Set(parent_hlp->text_shadows.Get());

		page_props.break_inside	= parent_hlp->page_props.break_inside;
		page_props.orphans		= parent_hlp->page_props.orphans;
		page_props.widows		= parent_hlp->page_props.widows;
		page_props.page			= parent_hlp->page_props.page;
#ifdef SUPPORT_TEXT_DIRECTION
		direction		= parent_hlp->direction;
#endif
		cursor = parent_hlp->cursor;

		current_bg_color = parent_hlp->current_bg_color;
		text_bg_color = parent_hlp->text_bg_color;
		era_max_width = parent_hlp->era_max_width;
		tab_size = parent_hlp->tab_size;
		current_script   = parent_hlp->current_script;
		current_font_type = parent_hlp->current_font_type;
		current_generic_font = parent_hlp->current_generic_font;
		max_paragraph_width = parent_hlp->max_paragraph_width;

		image_rendering = parent_hlp->image_rendering;

#ifdef CSS_TRANSITIONS
		UINT32 tmp = parent_hlp->transition_packed_init;
		transition_packed_init = 0;
		transition_packed.border_spacing = 1;
		transition_packed.font_color = 1;
		transition_packed.font_size = 1;
		transition_packed.font_weight = 1;
		transition_packed.letter_spacing = 1;
		transition_packed.line_height = 1;
		transition_packed.selection_bgcolor = 1;
		transition_packed.selection_color = 1;
		transition_packed.text_indent = 1;
		transition_packed.text_shadow = 1;
		transition_packed.visibility = 1;
		transition_packed.word_spacing = 1;
		transition_packed_init &= tmp;
		transition_packed2_init = 0;
		transition_packed3_init = 0;
#endif // CSS_TRANSITIONS

#if defined(SVG_SUPPORT)
		if (svg != NULL)
			svg->Reset(parent_hlp->svg);
#endif // SVG_SUPPORT

		SetSkipEmptyCellsBorder(parent_hlp->GetSkipEmptyCellsBorder());

		SetAwaitingWebfont(parent_hlp->GetAwaitingWebfont());
	}
	else
	{
		line_height_i	= LINE_HEIGHT_SQUEEZE_I;
		text_transform	= TEXT_TRANSFORM_NONE;
		text_indent		= 0;
		text_align		= CSS_VALUE_left;
		small_caps		= CSS_VALUE_normal;

		letter_spacing	= 0;
		word_spacing_i	= LayoutFixed(0);

		list_style_type		= CSS_VALUE_disc;
		list_style_pos		= CSS_VALUE_outside;
		list_style_image.Set(NULL);

#ifdef CSS_GRADIENT_SUPPORT
		list_style_image_gradient = NULL;
#endif // CSS_GRADIENT_SUPPORT

		visibility		= CSS_VALUE_visible;
		white_space		= CSS_VALUE_normal;
		overflow_wrap	= CSS_VALUE_normal;

		overline_color		= USE_DEFAULT_COLOR;
		linethrough_color	= USE_DEFAULT_COLOR;
		underline_color		= USE_DEFAULT_COLOR;
		textdecoration_baseline = SHRT_MIN;

		selection_color		= USE_DEFAULT_COLOR;
		selection_bgcolor	= USE_DEFAULT_COLOR;

		font_weight = 4;
		font_italic = FONT_STYLE_NORMAL;
		if (hld_profile)
		{
			VisualDevice* vd = hld_profile->GetVisualDevice();

			font_color = vd->GetColor();
			font_number = vd->GetCurrentFontNumber();
			// Setting the font size to RM1_Medium for handheld mode should be moved somewhere else (VisualDevice::Reset?),
			// and the font-size constant should probably not be RM1_Medium which is confusing.
			FramesDocument* doc = hld_profile->GetFramesDocument();
			if (hld_profile->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD) && doc && doc->GetMediaHandheldResponded() && doc->GetMediaType() == CSS_MEDIA_TYPE_HANDHELD)
				font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RM1_Medium);
			else
				font_size = vd->GetFontSize();

			decimal_font_size = IntToLayoutFixed(font_size);
			font_size = ConstrainFontHeight(font_size);
			decimal_font_size_constrained = IntToLayoutFixed(font_size);

			unflex_font_size = font_size;

#ifdef SVG_SUPPORT
			if (svg != NULL)
				svg->fontsize = font_size;
#endif // SVG_SUPPORT
			if (!hld_profile->IsInStrictMode())
				SetSkipEmptyCellsBorder(TRUE);

			max_paragraph_width = doc ? doc->GetMaxParagraphWidth() : 0;
		}
		else
		{
			font_color = USE_DEFAULT_COLOR;
			font_number = FONT_NUMBER_NOT_SET;
			unflex_font_size = font_size = FONT_SIZE_NOT_SET;
			decimal_font_size = decimal_font_size_constrained = LayoutFixed(FONT_SIZE_NOT_SET);
			max_paragraph_width = -1;
		}

		border_collapse = CSS_VALUE_separate;
		border_spacing_horizontal = 0;
		border_spacing_vertical = 0;
		empty_cells = CSS_VALUE_show;
		table_rules = 0;
		caption_side = CSS_VALUE_top;

		quotes_cp = NULL;
#ifdef GADGET_SUPPORT
        control_region_cp = NULL;
#endif // GADGET_SUPPORT
		text_shadows.Reset();

		image_rendering = CSS_VALUE_auto;

		page_props.break_inside	= CSS_VALUE_auto;
		page_props.orphans		= 2;
		page_props.widows		= 2;
		page_props.page			= NULL;

#ifdef SUPPORT_TEXT_DIRECTION
		direction = CSS_VALUE_ltr;
#endif
		cursor = CURSOR_AUTO;

		era_max_width = containing_block_width;
		current_bg_color = USE_DEFAULT_COLOR;
		text_bg_color = USE_DEFAULT_COLOR;
		tab_size = 8;

#ifdef CSS_TRANSITIONS
		transition_packed_init = 0;
		transition_packed2_init = 0;
		transition_packed3_init = 0;
#endif // CSS_TRANSITIONS

		current_script   =
#ifdef FONTSWITCHING
			hld_profile ? hld_profile->GetPreferredScript() :
#endif // FONTSWITCHING
			WritingSystem::Unknown;

		current_font_type = CurrentFontTypeDefault;
		current_generic_font = GENERIC_FONT_SERIF; // irrelevant at this stage.
	}
}

BOOL
SupportAttribute(Markup::Type elm_type, long attr)
{
	if (elm_type >= Markup::HTE_FIRST && elm_type < Markup::HTE_LAST)
		return (g_support_attr_array[elm_type - Markup::HTE_FIRST] & attr) != 0;

	return FALSE;
}

#ifdef _WML_SUPPORT_

short
WMLParseAlign(HTML_Element *table_elm, unsigned int column_num)
{
	const uni_char *align_str = table_elm->GetStringAttr(Markup::HA_ALIGN);

	if (align_str && uni_strlen(align_str) > column_num)
		switch (align_str[column_num])
		{
		case 'R':
			return CSS_VALUE_right;

		case 'C':
			return CSS_VALUE_center;

		case 'L':
			return CSS_VALUE_left;

		default:
			break;
		}

	return 0;
}

#endif // _WML_SUPPORT_

#ifdef M2_SUPPORT
void
HTMLayoutProperties::SetOMFCssPropertiesFromHtmlAttr(HTML_Element* element, const HTMLayoutProperties& parent_props, HLDocProfile* hld_profile)
{
	if (element->GetNsType() == NS_OMF)
	{
		Markup::Type elm_type = element->Type();

		if ((int)elm_type == OE_MIME)
		{
			FontAtt lf;
			g_pcfontscolors->GetFont(OP_SYSTEM_FONT_EMAIL_DISPLAY, lf);
			font_number = lf.GetFontNumber();
			font_size = lf.GetSize();
			decimal_font_size = IntToLayoutFixed(font_size);
			if (lf.GetItalic())
				font_italic = FONT_STYLE_ITALIC;
			if (lf.GetWeight() >= 1 && lf.GetWeight() <= 9)
					font_weight = lf.GetWeight();
			if (lf.GetUnderline())
				text_decoration |= TEXT_DECORATION_UNDERLINE;
			if (lf.GetStrikeOut())
				text_decoration |= TEXT_DECORATION_LINETHROUGH;
			if (lf.GetOverline())
				text_decoration |= TEXT_DECORATION_OVERLINE;
		}
	}
}
#endif // M2_SUPPORT

void
HTMLayoutProperties::SetListMarkerProps(FramesDocument *doc)
{
	vertical_align_type = CSS_VALUE_baseline;

	small_caps = CSS_VALUE_normal;
	text_transform = CSS_VALUE_UNSPECIFIED;

	margin_right = LayoutCoord(doc->GetHandheldEnabled() ? MARKER_INNER_OFFSET_HANDHELD : MARKER_INNER_OFFSET);
}

OP_STATUS
HTMLayoutProperties::SetCssPropertiesFromHtmlAttr(HTML_Element* element, const HTMLayoutProperties& parent_props, HLDocProfile* hld_profile)
{
	OP_PROBE4(OP_PROBE_HTMLAYOUTPROPERTIES_SETCSSPROPERTIESFROMHTML);

	Markup::Type elm_type = element->Type();
	NS_Type elm_ns_type = element->GetNsType();

	if (elm_ns_type != NS_HTML && elm_type != Markup::HTE_DOC_ROOT)
	{
		LayoutFixed old_decimal_font_size = decimal_font_size;

		switch (elm_ns_type)
		{
#ifdef M2_SUPPORT
		case NS_OMF:
			SetOMFCssPropertiesFromHtmlAttr(element, parent_props, hld_profile);
			break;
#endif // M2_SUPPORT

#ifdef SVG_SUPPORT
		case NS_SVG:
		{
			RETURN_IF_ERROR(SetSVGCssPropertiesFromHtmlAttr(element, parent_props, hld_profile));
			break;
		}
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
		case NS_CUE:
			if (elm_type == Markup::CUEE_BACKGROUND)
			{
				// Inherit background properties from cue root.
				bg_color = parent_props.bg_color;
				SetBgIsTransparent(parent_props.GetBgIsTransparent());

				bg_images.SetImages(parent_props.bg_images.GetImages());
				bg_images.SetPositions(parent_props.bg_images.GetPositions());
				bg_images.SetRepeats(parent_props.bg_images.GetRepeats());
				bg_images.SetAttachs(parent_props.bg_images.GetAttachs());
				bg_images.SetOrigins(parent_props.bg_images.GetOrigins());
				bg_images.SetClips(parent_props.bg_images.GetClips());
				bg_images.SetSizes(parent_props.bg_images.GetSizes());

#ifdef CSS_TRANSITIONS
				transition_packed.bg_pos = parent_props.transition_packed.bg_pos;
				transition_packed.bg_color = parent_props.transition_packed.bg_color;
				transition_packed.bg_size = parent_props.transition_packed.bg_size;
#endif // CSS_TRANSITIONS
			}
			break;
#endif // MEDIA_HTML_SUPPORT

		default:
			break;
		}

		if (old_decimal_font_size != decimal_font_size)
		{
			decimal_font_size_constrained = ConstrainFixedFontSize(decimal_font_size);
			font_size = LayoutFixedToShortNonNegative(decimal_font_size_constrained);
		}

        return OpStatus::OK;
	}

	BOOL inserted = (element->GetInserted() == HE_INSERTED_BY_LAYOUT);

	switch (elm_type)
	{
	case Markup::HTE_A:
		if (inserted)
		{
			URL* url = element->GetA_URL(hld_profile->GetLogicalDocument());
			if (url)
			{
				if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasFrame))
				{
					border.top.style = border.left.style = border.bottom.style = border.right.style = CSS_VALUE_outset;
					border.top.width = border.left.width = border.bottom.width = border.right.width = 2;
				}

				CSSMODE css_handling = hld_profile->GetCSSMode();

				if (FramesDocument::IsLinkVisited(hld_profile->GetFramesDocument(), *url))  // Visited Link
				{
					if (hld_profile->GetVLinkColor() == USE_DEFAULT_COLOR ||
					    !g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
					{
						if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasColor) && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
							font_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_VISITED_LINK);
					}
					else
						font_color = hld_profile->GetVLinkColor();

					if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
					{
						if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasUnderline))
							text_decoration |= TEXT_DECORATION_UNDERLINE;
						if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasStrikeThrough))
							text_decoration |= TEXT_DECORATION_LINETHROUGH;
					}
				}
				else // Link (not visited)
				{
					if (hld_profile->GetLinkColor() == USE_DEFAULT_COLOR ||
					    !g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
					{
						if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasColor) && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
							font_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_LINK);
					}
					else
						font_color = hld_profile->GetLinkColor();

					if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableUserLinkSettings)))
					{
						if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline))
							text_decoration |= TEXT_DECORATION_UNDERLINE;
						if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasStrikeThrough))
							text_decoration |= TEXT_DECORATION_LINETHROUGH;
					}
				}

				if (hld_profile->GetALinkColor() != USE_DEFAULT_COLOR)
				{
					element->SetHasDynamicPseudo(TRUE);
					FramesDocument* frm_doc = hld_profile->GetFramesDocument();
					if (frm_doc)
					{
						HTML_Document* doc = frm_doc->GetHtmlDocument();

						if (doc && doc->GetActivePseudoHTMLElement() == element &&
							g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(css_handling, PrefsCollectionDisplay::EnableAuthorFontAndColors)))
						{
							font_color = hld_profile->GetALinkColor();
						}
					}
				}
			}
		}
		break;

	case Markup::HTE_DIV:
		if (inserted)
			display_type = CSS_VALUE_block;
		break;

	case Markup::HTE_TBODY:
		if (inserted)
			display_type = CSS_VALUE_table_row_group;
		/* fall through */
	case Markup::HTE_THEAD:
	case Markup::HTE_TFOOT:
		if (table_rules == ATTR_VALUE_groups)
		{
			border.top.width = CSS_BORDER_WIDTH_THIN;
			border.top.style = CSS_VALUE_solid;
		}
		break;

	case Markup::HTE_TH:
		if (!parent_props.GetAlignSpecified()) // default TH value
		{
			text_align = CSS_VALUE_center;
			SetAlignSpecified();
			SetBidiAlignSpecified();
		}
		// fall-through
	case Markup::HTE_TD:
		{
			if (inserted)
				display_type = CSS_VALUE_table_cell;
			if (table_rules == ATTR_VALUE_cols || table_rules == ATTR_VALUE_all)
			{
				border.left.width = CSS_BORDER_WIDTH_THIN;
				border.left.style = CSS_VALUE_solid;
			}
			TableContent* table_content = NULL;
			HTML_Element* h = element->Parent();
			while (h)
			{
				if (h->Type() == Markup::HTE_TABLE && h->GetInserted() != HE_INSERTED_BY_LAYOUT)
				{
					table_content = h->GetLayoutBox() ? h->GetLayoutBox()->GetTableContent() : NULL;
					if (table_content)
					{
						if (h->HasNumAttr(Markup::HA_BORDER))
						{
							short border_width = (short)(INTPTR)h->GetAttr(Markup::HA_BORDER, ITEM_TYPE_NUM, (void*)0);
							if (border_width)
							{
								border.left.width = border.right.width = border.top.width = border.bottom.width = 1;
								border.left.style = border.right.style = border.top.style = border.bottom.style = CSS_VALUE_inset;
							}
						}

						if (element->GetLayoutBox() && element->GetLayoutBox()->IsTableCell())
						{
							short html_align = (short)(INTPTR)element->GetAttr(Markup::HA_ALIGN, ITEM_TYPE_NUM, (void*)(INTPTR)CSS_VALUE_UNSPECIFIED);
							if (!html_align)
							{
								// horizontal alignment is only needed for traversal and when box is created ...
								TableCellBox* cell_box = (TableCellBox*) element->GetLayoutBox();
								if (cell_box)
								{
#ifdef _WML_SUPPORT_
									if (hld_profile->IsWml() && h->HasAttr(Markup::HA_ALIGN))
									{
										html_align = WMLParseAlign(h, cell_box->GetColumn());
										if (html_align)
										{
											text_align = html_align;
											break;
										}
									}
#endif // _WML_SUPPORT_

									if (!h->IsDirty())
									{
										TableColGroupBox* column_box = table_content->GetColumnBox(cell_box->GetColumn());
										if (column_box)
										{
											HTML_Element* col_element = column_box->GetHtmlElement();
											html_align = (INTPTR)col_element->GetAttr(Markup::HA_ALIGN, ITEM_TYPE_NUM, (void*)0);

											if (!html_align && col_element->Parent() && col_element->Parent()->Type() == Markup::HTE_COLGROUP)
												html_align = (INTPTR)col_element->Parent()->GetAttr(Markup::HA_ALIGN, ITEM_TYPE_NUM, (void*)0);
										}
									}
								}
							}
							if (html_align)
								text_align = html_align;
						}
					}

					break;
				}
				h = h->Parent();
			}
		}
		break;

	case Markup::HTE_TR:
		if (inserted)
			display_type = CSS_VALUE_table_row;
		if (table_rules == ATTR_VALUE_rows || table_rules == ATTR_VALUE_all)
		{
			border.top.width = CSS_BORDER_WIDTH_THIN;
			border.top.style = CSS_VALUE_solid;
		}
		break;

	case Markup::HTE_COLGROUP:
		if (table_rules == ATTR_VALUE_groups)
		{
			border.left.width = CSS_BORDER_WIDTH_THIN;
			border.left.style = CSS_VALUE_solid;
		}
		break;

	case Markup::HTE_SMALL:
	case Markup::HTE_BIG:
		{
			FontAtt* font = styleManager->GetStyle(Markup::HTE_DOC_ROOT)->GetPresentationAttr().GetPresentationFont(current_script).Font;
			short normal_font_size = styleManager->GetNextFontSize(font, GET_CONSTRAINED_SHORT_FONT_SIZE(decimal_font_size), elm_type == Markup::HTE_SMALL);
			SetFontSizeSpecified();
#ifdef CSS_TRANSITIONS
			transition_packed.font_size = 0;
#endif // CSS_TRANSITIONS
			decimal_font_size = IntToLayoutFixed(normal_font_size);
			font_size = ConstrainFontHeight(normal_font_size);
			decimal_font_size_constrained = IntToLayoutFixed(font_size);
			break;
		}

	case Markup::HTE_SUB:
	case Markup::HTE_SUP:
		{
			short normal_font_size = (short)(0.83*GET_CONSTRAINED_SHORT_FONT_SIZE(decimal_font_size));
			SetFontSizeSpecified();
			decimal_font_size = IntToLayoutFixed(normal_font_size);
			font_size = ConstrainFontHeight(normal_font_size);
#ifdef CSS_TRANSITIONS
			transition_packed.font_size = 0;
#endif // CSS_TRANSITIONS
			decimal_font_size_constrained = IntToLayoutFixed(font_size);
			break;
		}

	case Markup::HTE_TABLE:
		if (inserted)
		{
			if (parent_props.display_type == CSS_VALUE_inline)
				display_type = CSS_VALUE_inline_table;
			else
				display_type = CSS_VALUE_table;
		}
        if (!hld_profile->IsInStrictMode())
		{
			line_height_i = -NORMAL_LINE_HEIGHT_FACTOR_I;
#ifdef CSS_TRANSITIONS
			transition_packed.line_height = 0;
#endif // CSS_TRANSITIONS
			overline_color = linethrough_color = underline_color = USE_DEFAULT_COLOR;
			SetHasDecorationAncestors(FALSE);
			// Need to set here, because we don't know the document font color in CSSCollection.

			font_color = hld_profile->GetLayoutWorkplace()->GetDocRootProperties().GetBodyFontColor();

			if (font_color == USE_DEFAULT_COLOR)
			{
				font_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_NORMAL);
			}
#ifdef CSS_TRANSITIONS
			transition_packed.font_color = 0;
#endif // CSS_TRANSITIONS
        }
		break;

	case Markup::HTE_MARQUEE:
		{
			int scrolldelay = (int)element->GetNumAttr(Markup::HA_SCROLLDELAY);
			if (scrolldelay)
			{
				if (scrolldelay < 40)
					// So says mozilla
					marquee_scrolldelay = 40;
				else
					if (scrolldelay > SHRT_MAX)
						marquee_scrolldelay = SHRT_MAX;
					else
						marquee_scrolldelay = (short) scrolldelay;
			}
		}
		break;

	case Markup::HTE_BUTTON:
	case Markup::HTE_SELECT:
#ifdef _SSL_SUPPORT_
	case Markup::HTE_KEYGEN:
#endif
	case Markup::HTE_TEXTAREA:
	case Markup::HTE_INPUT:
	case Markup::HTE_PROGRESS:
	case Markup::HTE_METER:
		border.left.style = border.right.style = border.top.style = border.bottom.style = BORDER_STYLE_NOT_SET;
		break;

#ifdef SVG_SUPPORT
	case Markup::HTE_IFRAME:
		if (element->GetInserted() == HE_INSERTED_BY_SVG)
			display_type = CSS_VALUE_none;
		break;
#endif // SVG_SUPPORT

	case Markup::HTE_LINK:
		if (element->GetInserted() == HE_INSERTED_BY_CSS_IMPORT)
			display_type = CSS_VALUE_none;
		break;
	}

	return OpStatus::OK;
}

/* static */ void
HTMLayoutProperties::SetFontAndSize(VisualDevice* vd, int font_number, int font_size, BOOL honor_text_scale)
{
	if (font_number >= 0)
		vd->SetFont(font_number);

	if (font_size >= 0)
	{
		int afontsize;

		if (honor_text_scale && vd->GetTextScale() != 100)
			afontsize = (font_size * vd->GetTextScale()) / 100;
		else
			afontsize = font_size;

		vd->SetFontSize(afontsize);
	}
}

void
HTMLayoutProperties::SetDisplayProperties(VisualDevice* vd) const
{
	OP_PROBE4(OP_PROBE_HTML_LAYOUT_SETDISPLAYPROPERTIES);

	SetFontAndSize(vd, font_number, font_size, TRUE);

	if (font_weight)
		vd->SetFontWeight(font_weight);

	if (font_italic >= 0)
		vd->SetFontStyle(font_italic);

	if (small_caps >= 0)
		vd->SetSmallCaps(small_caps == CSS_VALUE_small_caps);

	vd->SetCharSpacingExtra(letter_spacing);
}

LayoutFixed
HTMLayoutProperties::GetCalculatedLineHeightFixedPoint(VisualDevice* vd /* = NULL */) const
{
	if (line_height_i < LayoutFixed(0))
		return LayoutFixedMult(-line_height_i, decimal_font_size_constrained);
	else
		if (line_height_i == LINE_HEIGHT_SQUEEZE_I
# if SQUEEZE_MINIMUM_LINE_HEIGHT > 0
			|| (LayoutFixedToIntNonNegative(line_height_i) < font_ascent + font_descent && LayoutFixedToIntNonNegative(line_height_i) > SQUEEZE_MINIMUM_LINE_HEIGHT)
# endif // SQUEEZE_MINIMUM_LINE_HEIGHT > 0
			)
			return IntToLayoutFixed(font_ascent + font_descent);
		else
		{
			if (vd)
			{
				int text_scale = vd->GetTextScale();

				if (text_scale != 100)
					return MultLayoutFixedByQuotient(line_height_i, text_scale, 100);
			}
			return line_height_i;
		}
}

void
HTMLayoutProperties::SetTextMetrics(VisualDevice* vd)
{
	vd->GetFontMetrics(font_ascent, font_descent, font_internal_leading, font_average_width);
	font_underline_position = (font_ascent + font_descent - font_internal_leading) / 9;
	font_underline_width = 1 + (font_ascent + font_descent - font_internal_leading) / 24;
}

void
HTMLayoutProperties::SetERA_FontSizes(HTML_Element* element,
									  const HTMLayoutProperties& parent_props,
									  PrefsCollectionDisplay::RenderingModes rendering_mode,
									  PrefsCollectionDisplay::RenderingModes prev_rendering_mode,
									  int flexible_fonts,
									  int flex_font_scale)
{
	int min_font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize);
	int max_font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaxFontSize);

	short _new_font_size = flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED ? parent_props.unflex_font_size : -1;
	short _prev_font_size = -1;

	OP_ASSERT(flexible_fonts != FLEXIBLE_FONTS_DOCUMENT_ONLY);

	switch (element->Type())
	{
	case Markup::HTE_DOC_ROOT:
		_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::Medium));
		_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::Medium));
		break;

	case Markup::HTE_H1:
		_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::XLarge));
		_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::XLarge));
		break;

	case Markup::HTE_H2:
		_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::Large));
		_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::Large));
		break;

	case Markup::HTE_FONT:
		switch (element->GetFontSize())
		{
		case 1:
			_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::Small));
			_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::Small));
			break;

		case 4:
			_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::Large));
			_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::Large));
			break;

		case 5:
			_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::XLarge));
			_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::XLarge));
			break;

		case 6:
		case 7:
			_new_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::XXLarge));
			_prev_font_size = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(prev_rendering_mode, PrefsCollectionDisplay::XXLarge));
			break;
		}

		break;
	}

	if (_new_font_size < 0)
		font_size = parent_props.font_size;
	else
	{
		OP_ASSERT(flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED || _prev_font_size > -1);

		unflex_font_size = _new_font_size;

		if (flexible_fonts == FLEXIBLE_FONTS_PREVIOUS_TO_PREDEFINED)
			_new_font_size += short((flex_font_scale * (_prev_font_size - _new_font_size)) / 100);
		else
			if (flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED)
				_new_font_size += short((flex_font_scale * (GET_CONSTRAINED_SHORT_FONT_SIZE(decimal_font_size) - _new_font_size)) / 100);

		if (_new_font_size > max_font_size)
			font_size = max_font_size;
		else if (_new_font_size < min_font_size)
			font_size = min_font_size;
		else
			font_size = _new_font_size;
	}

	decimal_font_size_constrained = IntToLayoutFixed(font_size);
}

/* static */ BOOL
HTMLayoutProperties::IsViewportMagicElement(HTML_Element* element, HLDocProfile* hld_profile)
{
	if (element->GetNsType() != NS_HTML)
		return FALSE;

	Markup::Type elm_type = element->Type();

	return elm_type == Markup::HTE_HTML ||
		elm_type == Markup::HTE_BODY && LayoutWorkplace::IsMagicBodyElement(element, element->Parent(), hld_profile->GetBodyElm());
}

BOOL
HTMLayoutProperties::IsShrinkToFit(Markup::Type element_type) const
{
	return content_width == CONTENT_WIDTH_AUTO &&
		(display_type == CSS_VALUE_inline_block ||
		 float_type == CSS_VALUE_left || float_type == CSS_VALUE_right ||
		 (position == CSS_VALUE_absolute || position == CSS_VALUE_fixed) && (left == HPOSITION_VALUE_AUTO || right == HPOSITION_VALUE_AUTO) ||
		 element_type == Markup::HTE_LEGEND) &&
		!GetIsFlexItem(); // Flex items are sized by their containing flexbox.
}

BOOL
HTMLayoutProperties::NeedsSpaceManager(Markup::Type element_type) const
{
	return
		overflow_x != CSS_VALUE_visible ||
		display_type == CSS_VALUE__wap_marquee ||
		display_type == CSS_VALUE_flex ||
		display_type == CSS_VALUE_inline_flex ||
		column_count > 0 ||
		column_width > 0 ||
		column_span > 1 ||
		element_type == Markup::HTE_FIELDSET ||
		element_type == Markup::HTE_BUTTON ||
		element_type == Markup::HTE_VIDEO;
}

/** @return the number of text shadows. */
int
Shadows::GetCount() const
{
	if (!shadows_cp)
		return 0;

	int count = 1;
	int arr_len = shadows_cp->ArrayValueLen();

	for (int i = 0; i < arr_len; i ++)
		if (shadows_cp->GetValueType(i) == CSS_COMMA)
			count ++;

	return count;
}

Shadows::Iterator::Iterator(const Shadows& shadow) :
	arr(NULL), arr_len(0), pos(0), length_count(0)
{
	if (CSS_decl* shadows_cp = shadow.Get())
	{
		arr = shadows_cp->GenArrayValue();
		arr_len = shadows_cp->ArrayValueLen();
	}
}

void Shadows::Iterator::HandleValue(const CSS_generic_value* value,
									const CSSLengthResolver& length_resolver, Shadow& shadow)
{
	switch (value->value_type)
	{
	case CSS_DECL_LONG:
	case CSS_DECL_COLOR:
		if (value->value.color == CSS_COLOR_transparent)
			shadow.color = OP_RGBA(0,0,0,0);
		else if (value->value.color != CSS_COLOR_current_color)
			shadow.color = value->value.color;
		break;

	case CSS_IDENT:
		if (value->GetType() == CSS_VALUE_inset)
			shadow.inset = TRUE;
		break;

	default:
		{
			OP_ASSERT(CSS_is_length_number_ext(value->value_type));

			// FIXME What is percentage relative to?
			int val = length_resolver.GetLengthInPixels(value->GetReal(), value->value_type);
			short short_val = (short)MIN(MAX(val, SHRT_MIN), SHRT_MAX);

			switch (length_count++)
			{
			case 0:
				shadow.left = short_val;
				break;

			case 1:
				shadow.top = short_val;
				break;

			case 2:
				shadow.blur = short_val;
				break;

			case 3:
				shadow.spread = short_val;
				break;

			default:
				OP_ASSERT(0); // too many length values
			}
			break;
		}
	}
}

/** Get the next shadow declaration in the shadow array. */

BOOL Shadows::Iterator::GetNext(const CSSLengthResolver& length_resolver, Shadow& shadow)
{
	shadow.Reset();

	length_count = 0;

	int num_values = 0;
	while (pos < arr_len)
	{
		const CSS_generic_value* value = arr + pos++;

		if (value->value_type == CSS_COMMA)
			return TRUE;

		HandleValue(value, length_resolver, shadow);
		num_values++;
	}

	OP_ASSERT(num_values == 0 || length_count == 2 || length_count == 3 || length_count == 4); // top+left and top+left+blur + top+left+blur+spread are the only valid combinations

	return num_values != 0;
}

/** Get the previous shadow declaration in the shadow array. */

BOOL Shadows::Iterator::GetPrev(const CSSLengthResolver& length_resolver, Shadow& shadow)
{
	shadow.Reset();

	if (pos == 0)
		return FALSE;

	int next_pos = pos;
	int item_start = pos;
	while (next_pos > 0)
	{
		if (arr[next_pos - 1].value_type == CSS_COMMA)
		{
			next_pos--;
			break;
		}
		item_start--;
		next_pos--;
	}

	pos = item_start;
	GetNext(length_resolver, shadow);
	pos = next_pos;
	return TRUE;
}

/** @return TRUE if box-shadow specified and any of the shadows uses inset keyword. */

BOOL
HTMLayoutProperties::HasInsetBoxShadow(VisualDevice* vd) const
{
	if (!vd || !HasBoxShadow())
		return FALSE;

	Shadow shadow;
	CSSLengthResolver dummy(vd);
	Shadows::Iterator iter(box_shadows);

	while (iter.GetNext(dummy, shadow))
		if (shadow.inset)
			return TRUE;

	return FALSE;
}

/** @return the distance to the furthest box shadow from the box.

	Mirror of the text shadow variant.  Used for bounding box
	calculation. */

LayoutCoord
Shadows::GetMaxDistance(const CSSLengthResolver& length_resolver) const
{
	/* We may want to store the "max distance" value somewhere instead of
	   calculating it every time it is needed. There are also cheaper ways of
	   calculating it, but this code is currently optimized for footprint and
	   simplicity. */

	LayoutCoord highest(0);
	Shadow shadow;
	Shadows::Iterator iter(*this);

	while (iter.GetNext(length_resolver, shadow))
	{
		if (shadow.inset)
			continue;

		LayoutCoord shadow_width = LayoutCoord(MAX(0, shadow.blur) + MAX(0, shadow.spread));

		if (LayoutCoord(op_abs(shadow.left) + shadow_width) > highest)
			highest = LayoutCoord(op_abs(shadow.left) + shadow_width);

		if (LayoutCoord(op_abs(shadow.top) + shadow_width) > highest)
			highest = LayoutCoord(op_abs(shadow.top) + shadow_width);
	}

	return highest;
}

// WARNING: This code is duplicated in svg/src/SVGTraverse.cpp:SVGObjectFitPosition::CalculateObjectFit
//			(with SVGRects). Any modifications must be consistent.
/* static */ void
ObjectFitPosition::CalculateObjectFit(short fit_fallback, OpRect intrinsic, OpRect inner, OpRect& dst) const
{
	short object_fit;
	if(fit == CSS_VALUE_auto)
		object_fit = fit_fallback;
	else
		object_fit = fit;

	// Fall back to "fill" if an intrinsic dimension is missing.
	if (!intrinsic.width || !intrinsic.height)
		object_fit = CSS_VALUE_fill;

	if (object_fit == CSS_VALUE_contain || object_fit == CSS_VALUE_cover)
	{
		// Note: With SVG's "infinite" intrinsic size, the following multiplications overflow and result in some dimensions being 0.
		// This is not an issue as long as SVG's object-fit implementation does not call this function.

		// This is a multiplication version of inner_width/inner_height > img.Width()/img.Height(), avoiding floating point
		BOOL inner_is_wider = inner.width*intrinsic.height > intrinsic.width*inner.height;

		if (object_fit == CSS_VALUE_contain && inner_is_wider
			|| object_fit == CSS_VALUE_cover && !inner_is_wider )
		{
			// The content box has a higher width-to-height ratio than the image (i.e. if they were the same height, the content box would be wider).
			// Pillarboxing (if contain): Match image height to content box and scale width
			// 'intrinsic.height/2' is added to avoid '+ 0.5' and floating-point math
			dst.width = (inner.height*intrinsic.width + intrinsic.height/2) / intrinsic.height;
			dst.height = inner.height;
		}
		else
		{
			// The content box has a lower width-to-height ratio than the image (i.e. if they were the same width, the content box would be taller).
			// Letterboxing (if contain): Match image width to content box and scale height
			// 'intrinsic.width/2' is added to avoid '+ 0.5' and floating-point math
			dst.width = inner.width;
			dst.height = (inner.width*intrinsic.height + intrinsic.width/2) / intrinsic.width;
		} // If the sides are equal, the latter case works fine: we don't need a special one.
	}
	else if (object_fit == CSS_VALUE_none)
	{
		dst.width = intrinsic.width;
		dst.height = intrinsic.height;
	}
	else
	{
		// CSS_VALUE_fill is initial, traditional value
		// Match image height and width to content box
		dst.height = inner.height;
		dst.width = inner.width;
	}
}

// WARNING: This code is duplicated in svg/src/SVGTraverse.cpp:SVGObjectFitPosition::CalculateObjectPosition
//			(with SVGNumbers). Any modifications must be consistent.
/* static */ void
ObjectFitPosition::CalculateObjectPosition(const OpRect& pos_rect, OpRect& content) const
{
	// Calculate initial position of content
	content.x = pos_rect.x;
	content.y = pos_rect.y;

	if (x_percent)
		content.x += LayoutFixedPercentageOfInt(LayoutFixed(x), pos_rect.width - content.width);
	else
		content.x += x;

	if (y_percent)
		content.y += LayoutFixedPercentageOfInt(LayoutFixed(y), pos_rect.height - content.height);
	else
		content.y += y;
}


#define ASSIGN_BORDER_RADIUS(EDGE, RADIUS, IS_INHERIT, ABSOLUTE_RADIUS, ABSOLUTE_RADIUS_IS_DECIMAL) \
	if ((IS_INHERIT))													\
	{																	\
		border.EDGE.RADIUS = (parent_props.border.EDGE.RADIUS);			\
		border.EDGE.packed.RADIUS ## _is_decimal =						\
			(parent_props.border.EDGE.packed.RADIUS ## _is_decimal);	\
	}																	\
	else																\
	{																	\
		border.EDGE.RADIUS = (ABSOLUTE_RADIUS);							\
		border.EDGE.packed.RADIUS ## _is_decimal =						\
			(ABSOLUTE_RADIUS_IS_DECIMAL);								\
	}

/* FIXME: currentStyle support? */

void
HTMLayoutProperties::SetBorderRadius(CssPropertyItem* pi, const HTMLayoutProperties& parent_props, const CSSLengthResolver& length_resolver)
{
	short horiz_radius = 0;
	short vert_radius = 0;
	BOOL horiz_radius_is_decimal = FALSE;
	BOOL vert_radius_is_decimal = FALSE;
	BOOL horiz_radius_is_inherit = FALSE;
	BOOL vert_radius_is_inherit = FALSE;

	OP_ASSERT(pi->info.type == CSSPROPS_BORDER_TOP_RIGHT_RADIUS ||
			  pi->info.type == CSSPROPS_BORDER_BOTTOM_RIGHT_RADIUS ||
			  pi->info.type == CSSPROPS_BORDER_BOTTOM_LEFT_RADIUS ||
			  pi->info.type == CSSPROPS_BORDER_TOP_LEFT_RADIUS);

	if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_inherit)
	{
		if (pi->info.info1.val1_percentage)
		{
			horiz_radius = short(-pi->value.length2_val.info.val1);
			horiz_radius_is_decimal = pi->info.info1.val1_decimal;
		}
		else
		{
			horiz_radius = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
													 pi->value.length2_val.info.val1,
													 FALSE,
													 pi->info.info1.val1_decimal,
													 length_resolver,
													 SHRT_MIN,
													 SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
													 , NULL
#endif // CURRENT_STYLE_SUPPORT
				);
		}
	}
	else
		horiz_radius_is_inherit = TRUE;

	if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_inherit)
	{
		if (pi->info.info1.val2_percentage)
		{
			vert_radius = short(-pi->value.length2_val.info.val2);
			vert_radius_is_decimal = pi->info.info1.val2_decimal;
		}
		else
		{
			vert_radius = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
													pi->value.length2_val.info.val2,
													FALSE,
													pi->info.info1.val2_decimal,
													length_resolver,
													SHRT_MIN,
													SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
													, NULL
#endif
				);
		}
	}
	else
		vert_radius_is_inherit = TRUE;

	switch (pi->info.type)
	{
	case CSSPROPS_BORDER_TOP_RIGHT_RADIUS:
		ASSIGN_BORDER_RADIUS(top, radius_end, horiz_radius_is_inherit, horiz_radius, horiz_radius_is_decimal);
		ASSIGN_BORDER_RADIUS(right, radius_start, vert_radius_is_inherit, vert_radius, vert_radius_is_decimal);
		break;
	case CSSPROPS_BORDER_BOTTOM_RIGHT_RADIUS:
		ASSIGN_BORDER_RADIUS(bottom, radius_end, horiz_radius_is_inherit, horiz_radius, horiz_radius_is_decimal);
		ASSIGN_BORDER_RADIUS(right, radius_end, vert_radius_is_inherit, vert_radius, vert_radius_is_decimal);
		break;
	case CSSPROPS_BORDER_BOTTOM_LEFT_RADIUS:
		ASSIGN_BORDER_RADIUS(bottom, radius_start, horiz_radius_is_inherit, horiz_radius, horiz_radius_is_decimal);
		ASSIGN_BORDER_RADIUS(left, radius_end, vert_radius_is_inherit, vert_radius, vert_radius_is_decimal);
		break;
	case CSSPROPS_BORDER_TOP_LEFT_RADIUS:
		ASSIGN_BORDER_RADIUS(top, radius_start, horiz_radius_is_inherit, horiz_radius, horiz_radius_is_decimal);
		ASSIGN_BORDER_RADIUS(left, radius_start, vert_radius_is_inherit, vert_radius, vert_radius_is_decimal);
		break;
	default:
		OP_ASSERT(!"Should not happen");
		break;
	}
}

#undef ASSIGN_BORDER_RADIUS

void
HTMLayoutProperties::SetCellVAlignFromTableColumn(HTML_Element* element, LayoutProperties* parent_cascade)
{
	OP_ASSERT(element->Type() == Markup::HTE_TD || element->Type() == Markup::HTE_TH);

	/* Special handling for TD and TH elements. If neither the TR nor the TBODY
	   / THEAD / TFOOT that this cell belongs to specifies vertical-align, see
	   if its COL or COLGROUP has a VALIGN attribute, and, if so, use it as
	   vertical-align for this cell. */

	HTML_Element* row_elm = parent_cascade->html_element;
	const HTMLayoutProperties& row_props = parent_cascade->GetCascadingProperties();

	if (row_elm->Type() != Markup::HTE_TR ||
		row_props.GetVerticalAlignSpecified() || row_elm->HasNumAttr(Markup::HA_VALIGN))
		return;

	LayoutProperties* rowgroup_cascade = parent_cascade->Pred();
	if (!rowgroup_cascade || !rowgroup_cascade->html_element)
		return;

	HTML_Element* rowgroup_elm = rowgroup_cascade->html_element;
	Markup::Type rowgroup_elm_type = rowgroup_elm->Type();
	const HTMLayoutProperties& rowgroup_props = rowgroup_cascade->GetCascadingProperties();

	if (rowgroup_elm_type != Markup::HTE_TBODY && rowgroup_elm_type != Markup::HTE_THEAD && rowgroup_elm_type != Markup::HTE_TFOOT ||
		rowgroup_props.GetVerticalAlignSpecified() || rowgroup_elm->HasNumAttr(Markup::HA_VALIGN))
		return;

	LayoutProperties* table_cascade = rowgroup_cascade->Pred();
	if (!table_cascade ||
		!table_cascade->html_element ||
		table_cascade->html_element->Type() != Markup::HTE_TABLE ||
		table_cascade->html_element->IsDirty())
		return;

	Box* table_box = table_cascade->html_element->GetLayoutBox();
	if (!table_box)
		return;

	TableContent* table_content = table_box->GetTableContent();
	if (!table_content)
		return;

	// vertical alignment may not be correct until box is created ..., is that sufficient?
	// All we need is the column that this cell belongs to and that is known by the table.

	if (element->GetLayoutBox() && element->GetLayoutBox()->IsTableCell())
	{
		TableCellBox* cell_box = (TableCellBox*) element->GetLayoutBox();

		TableColGroupBox* column_box = table_content->GetColumnBox(cell_box->GetColumn());
		if (column_box)
		{
			HTML_Element* col_element = column_box->GetHtmlElement();

			if (col_element->Type() == Markup::HTE_COL || col_element->Type() == Markup::HTE_COLGROUP)
			{
				CSSValue valign = (CSSValue)(INTPTR)col_element->GetAttr(Markup::HA_VALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_UNSPECIFIED);

				if (valign == CSS_VALUE_UNSPECIFIED && col_element->Parent() && col_element->Parent()->Type() == Markup::HTE_COLGROUP)
					valign = (CSSValue)(INTPTR)col_element->Parent()->GetAttr(Markup::HA_VALIGN, ITEM_TYPE_NUM, (void*)CSS_VALUE_UNSPECIFIED);

				if (valign != CSS_VALUE_UNSPECIFIED)
					vertical_align_type = valign;
			}
		}
	}
}

short
CheckRealSizeDependentCSS(HTML_Element* element, FramesDocument* doc)
{
	LayoutMode layout_mode = doc->GetLayoutMode();

	if (layout_mode != LAYOUT_NORMAL && !doc->GetMediaHandheldResponded())
	{
		PrefsCollectionDisplay::RenderingModes rendering_mode;

		switch (layout_mode)
		{
		case LAYOUT_SSR:
			rendering_mode = PrefsCollectionDisplay::SSR;
			break;

		case LAYOUT_CSSR:
			rendering_mode = PrefsCollectionDisplay::CSSR;
			break;

		case LAYOUT_AMSR:
			rendering_mode = PrefsCollectionDisplay::AMSR;
			break;

#ifdef TV_RENDERING
		case LAYOUT_TVR:
			rendering_mode = PrefsCollectionDisplay::TVR;
			break;
#endif // TV_RENDERING

		default:
			rendering_mode = PrefsCollectionDisplay::MSR;
			break;
		}

		int remove_ornamental_images = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RemoveOrnamentalImages));
		BOOL remove_large_images = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::RemoveLargeImages));
		BOOL use_alt_for_certain_images = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::UseAltForCertainImages));

		if (remove_ornamental_images || remove_large_images || use_alt_for_certain_images)
		{
			int real_width = 0;
			int real_height = 0;
			int img_width = (int)element->GetNumAttr(Markup::HA_WIDTH);
			int img_height = (int)element->GetNumAttr(Markup::HA_HEIGHT);

			BOOL is_link = FALSE;

			if (element->GetA_Tag())
				is_link = TRUE;

			URL img_url = element->GetImageURL(TRUE, doc->GetLogicalDocument());

			if (!img_url.IsEmpty())
			{
				Image img = UrlImageContentProvider::GetImageFromUrl(img_url);

				real_width = img.Width();
				real_height = img.Height();
			}

			BOOL rescued_ornamental_image = FALSE;

			if (remove_ornamental_images)
				if ((img_width > 0 && img_width <= 28) ||
					(img_height > 0 && img_height <= 15) ||
					(real_width > 0 && real_width <= 28) ||
					(real_height > 0 && real_height <= 15))
				{
					// We remove small images if PrefsManager::RemoveOrnamentalImages is >0 but keep them if
					// PrefsManager::RemoveOrnamentalImages is 2 and there is an alt text. This is to avoid
					// replacing small "inline" images with long texts.

					if (remove_ornamental_images == 2 && element->HasAttr(Markup::HA_ALT))
						rescued_ornamental_image = TRUE;
					else
						return is_link ? 0 : CSS_VALUE_none;
				}

			if (remove_large_images)
				if ((img_width > 600 && img_height > 600) || (real_width > 600 && real_height > 600))
					return CSS_VALUE_none;

			const uni_char* alt_txt = NULL;

			if (element->HasAttr(Markup::HA_ALT))
				alt_txt = element->GetStringAttr(Markup::HA_ALT);

			OP_ASSERT(!element->HasAttr(Markup::HA_ALT) || alt_txt);

			if (use_alt_for_certain_images && !rescued_ornamental_image && element->HasAttr(Markup::HA_ALT) && alt_txt && *alt_txt)
				if ((img_width > 0 && img_width <= 35) || (img_height > 0 && img_height <= 34) ||
					(real_width > 0 && real_width <= 35) || (real_height > 0 && real_height <= 34))
				{
					return CSS_VALUE_inline;
				}

			// Remove really small images or very thin images i.e. table borders and similar.
			if (remove_ornamental_images)
				if ((img_width > 0 && img_height > 0 && (img_width < 5 || img_height < 5 || (img_width < 10 && img_height < 10))) ||
					(real_width > 0 && real_height > 0 && (real_width < 5 || real_height < 5 || (real_width < 10 && real_height < 10))))
					return is_link ? 0 : CSS_VALUE_none;
		}
	}

	return 0;
}

/** Returns TRUE if the element will be rendered as replaced content.
	Expects element_type to be the resolved element_type for Markup::HTE_OBJECT.
	So if the OBJECT is rendered as an APPLET, send in Markup::HTE_APPLET. */
static BOOL
IsReplacedElement(NS_Type elm_ns, Markup::Type element_type, HLDocProfile* hld_prof)
{
	if (elm_ns == NS_HTML)
		switch (element_type)
		{
		case Markup::HTE_IFRAME:
			return g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::IFramesEnabled, *hld_prof->GetURL());

		case Markup::HTE_IMG:
#ifdef MEDIA_HTML_SUPPORT
		case Markup::HTE_VIDEO:
#endif // MEDIA_HTML_SUPPORT
		case Markup::HTE_EMBED:
		case Markup::HTE_APPLET:
		case Markup::HTE_INPUT:
		case Markup::HTE_SELECT:
		case Markup::HTE_TEXTAREA:
#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_
		case Markup::HTE_KEYGEN:
#endif // _SSL_SUPPORT_ && !_EXTERNAL_SSL_SUPPORT_
#ifdef CANVAS_SUPPORT
		case Markup::HTE_CANVAS:
#endif // CANVAS_SUPPORT
		case Markup::HTE_METER:
		case Markup::HTE_PROGRESS:
			return TRUE;
		default:
			return FALSE;
		}
#ifdef SVG_SUPPORT
	else if (elm_ns == NS_SVG)
		return element_type == Markup::SVGE_SVG;
#endif // SVG_SUPPORT
	else
		return FALSE;
}

static FontAtt* GetFontForElm(const HTML_Element* elm, const WritingSystem::Script script)
{
	Style* style;
    const Markup::Type elm_type = elm->Type();
    if (elm_type == Markup::HTE_BUTTON ||
        elm_type == Markup::HTE_SELECT ||
#ifdef _SSL_SUPPORT_
        elm_type == Markup::HTE_KEYGEN ||
#endif // _SSL_SUPPORT_
        elm_type == Markup::HTE_TEXTAREA ||
        elm_type == Markup::HTE_INPUT)
    {
        int form_style = GetFormStyle(elm_type, elm->GetInputType());
        style = styleManager->GetStyleEx(form_style);
    }
    else
        style = styleManager->GetStyle(elm_type);
    OP_ASSERT(style);
	FontAtt* font = style->GetPresentationAttr().GetPresentationFont(script).Font;
    if (!font)
	{
		style = styleManager->GetStyle(Markup::HTE_DOC_ROOT);
		font = style->GetPresentationAttr().GetPresentationFont(script).Font;
	}
	OP_ASSERT(font);
	return font;
}

/** Return TRUE if this a display type that participates in a table formatting context.

	That is, any 'table*' display type except 'table' itself and 'inline-table'. */

static BOOL IsInternalTableDisplayType(short display_type)
{
	return display_type == CSS_VALUE_table_row_group ||
		display_type == CSS_VALUE_table_header_group ||
		display_type == CSS_VALUE_table_footer_group ||
		display_type == CSS_VALUE_table_row ||
		display_type == CSS_VALUE_table_cell ||
		display_type == CSS_VALUE_table_caption ||
		display_type == CSS_VALUE_table_column_group ||
		display_type == CSS_VALUE_table_column;
}

/** Return TRUE if this is an inline-level display type. */

static inline BOOL IsInlineDisplayType(short display_type)
{
	return display_type == CSS_VALUE_inline ||
		display_type == CSS_VALUE_inline_block ||
		display_type == CSS_VALUE_inline_table ||
		display_type == CSS_VALUE_inline_flex;
}

/** Convert inline-level display type to block-level.

	Inline-level display types cannot be used on floats and absolutely
	positioned boxes. Nor can they become flex items. */

static short ToBlockDisplayType(short display_type)
{
	switch (display_type)
	{
	case CSS_VALUE_none:
	case CSS_VALUE_flex:
	case CSS_VALUE_list_item:
	case CSS_VALUE_table:
	case CSS_VALUE__wap_marquee:
		return display_type;

	case CSS_VALUE_inline_flex:
		return CSS_VALUE_flex;

	case CSS_VALUE_inline_table:
		return CSS_VALUE_table;

	default:
		return CSS_VALUE_block;
	}
}

/** Is 'parent' an auto height element? */

static BOOL
IsParentAutoHeight(HLDocProfile* hld_profile, Container* container, FlexContent* flexbox, LayoutProperties* parent_cascade)
{
	if (!hld_profile->IsInStrictMode())
		return FALSE;

	OP_ASSERT(!container || !flexbox);

	BOOL parent_is_auto_height = FALSE;
	HTML_Element* cont_element = container ? container->GetHtmlElement() : flexbox ? flexbox->GetHtmlElement() : NULL;

	if (cont_element)
	{
		Box* containing_box = cont_element->GetLayoutBox();

		if (cont_element->Type() != Markup::HTE_DOC_ROOT)
		{
			LayoutProperties *container_cascade = parent_cascade;

			while (container_cascade && container_cascade->html_element != cont_element)
				container_cascade = container_cascade->Pred();

			if (container_cascade)
			{
				const HTMLayoutProperties& props = *container_cascade->GetProps();

				parent_is_auto_height =
					props.content_height == CONTENT_HEIGHT_AUTO &&
					(!containing_box->IsAbsolutePositionedBox() || props.top == VPOSITION_VALUE_AUTO || props.bottom == VPOSITION_VALUE_AUTO);

				if (parent_is_auto_height && containing_box->IsTableCell())
				{
					OP_ASSERT(parent_cascade->table);

					long row_height = container_cascade->Pred()->GetProps()->content_height;

					if (row_height != CONTENT_HEIGHT_AUTO)
					{
						LayoutProperties *table_cascade = container_cascade;
						HTML_Element *table_element = parent_cascade->table->GetPlaceholder()->GetHtmlElement();

						while (table_cascade && table_cascade->html_element != table_element)
							table_cascade = table_cascade->Pred();

						if (table_cascade && (row_height >= 0 || table_cascade->GetProps()->content_height != CONTENT_HEIGHT_AUTO))
							parent_is_auto_height = FALSE;
					}
				}
			}
		}
	}

	return parent_is_auto_height;
}

/* static */ CSS_decl*
HTMLayoutProperties::GetDeclFromPropertyItem(const CssPropertyItem &pi, CSS_decl *inherit_decl)
{
	if (pi.info.is_inherit)
		return inherit_decl;
	else
	{
		CSS_decl* cd = pi.value.css_decl;

		if (cd->GetDeclType() == CSS_DECL_TYPE)
		{
			OP_ASSERT(cd->TypeValue() == CSS_VALUE_none);
			return NULL;
		}
		else
			return cd;
	}
}

#ifdef CSS_TRANSFORMS

void
HTMLayoutProperties::SetTransformOrigin(const CssPropertyItem &pi, const HTMLayoutProperties &parent_props, const CSSLengthResolver& length_resolver)
{
	if (pi.value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
	{
		transform_origin_x = parent_props.transform_origin_x;
		info2.transform_origin_x_is_decimal = parent_props.info2.transform_origin_x_is_decimal;
		info2.transform_origin_x_is_percent = parent_props.info2.transform_origin_x_is_percent;
	}
	else
	{
		if (pi.info.info1.val1_percentage)
		{
			transform_origin_x = LayoutCoord(pi.value.length2_val.info.val1);
			info2.transform_origin_x_is_decimal = pi.info.info1.val1_decimal;
			info2.transform_origin_x_is_percent = 1;
		}
		else
		{
			transform_origin_x = GetLengthInPixelsFromProp(pi.value.length2_val.info.val1_type,
													pi.value.length2_val.info.val1,
													FALSE,
													pi.info.info1.val1_decimal,
													length_resolver,
													LAYOUT_COORD_MIN,
													LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
													, NULL
#endif // CURRENT_STYLE_SUPPORT
				);
			info2.transform_origin_x_is_decimal = 0;
			info2.transform_origin_x_is_percent = 0;
		}
	}

	if (pi.value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
	{
		transform_origin_y = parent_props.transform_origin_y;
		info2.transform_origin_y_is_decimal = parent_props.info2.transform_origin_y_is_decimal;
		info2.transform_origin_y_is_percent = parent_props.info2.transform_origin_y_is_percent;
	}
	else
	{
		if (pi.info.info1.val2_percentage)
		{
			transform_origin_y = LayoutCoord(pi.value.length2_val.info.val2);
			info2.transform_origin_y_is_decimal = pi.info.info1.val2_decimal;
			info2.transform_origin_y_is_percent = 1;
		}
		else
		{
			transform_origin_y = GetLengthInPixelsFromProp(pi.value.length2_val.info.val2_type,
													pi.value.length2_val.info.val2,
													FALSE,
													pi.info.info1.val2_decimal,
													length_resolver,
													LAYOUT_COORD_MIN,
													LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
													, NULL
#endif // CURRENT_STYLE_SUPPORT
				);
			info2.transform_origin_y_is_decimal = 0;
			info2.transform_origin_y_is_percent = 0;
		}
	}
}

#endif // CSS_TRANSFORMS

OP_STATUS
HTMLayoutProperties::GetCssProperties(HTML_Element* element, LayoutProperties* parent_cascade, HLDocProfile* hld_profile, Container* container, BOOL ignore_transitions)
{
	OP_PROBE6(OP_PROBE_HTMLAYOUTPROPERTIES_GETCSSPROPERTIES);

	LayoutProperties* orig_parent_cascade = parent_cascade;

	if (parent_cascade->html_element)
		while (parent_cascade->html_element->GetInserted() == HE_INSERTED_BY_LAYOUT && !parent_cascade->html_element->GetIsPseudoElement())
			parent_cascade = parent_cascade->Pred();

	const HTMLayoutProperties& parent_props = parent_cascade->GetCascadingProperties();
	Markup::Type elm_type = element->Type();
	NS_Type elm_ns = element->GetNsType();

	VisualDevice* vd = hld_profile->GetVisualDevice();

	CssPropertyItem* css_properties = element->GetCssProperties();

	BOOL use_era_setting = FALSE;
	LayoutMode layout_mode = LAYOUT_NORMAL;
	FramesDocument* doc = hld_profile->GetFramesDocument();

	LayoutWorkplace* layout_workplace = NULL;

	if (doc)
	{
		layout_mode = doc->GetLayoutMode();
		use_era_setting = layout_mode != LAYOUT_NORMAL && (!doc->GetMediaHandheldResponded() || doc->GetOverrideMediaStyle());
		if (doc->GetLogicalDocument())
			layout_workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();
	}

	OP_ASSERT(layout_workplace);

	BOOL is_ssr_or_cssr = layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR;
	DocRootProperties& root_props = layout_workplace->GetDocRootProperties();
	BOOL updating_root_font_size = elm_type == Markup::HTE_DOC_ROOT || element->Parent()->Type() == Markup::HTE_DOC_ROOT;
#ifdef SVG_SUPPORT
	if (updating_root_font_size)
	{
		FramesDocElm* frame = doc->GetDocManager()->GetFrame();
		if (frame && frame->IsSVGResourceDocument())
		{
			OP_ASSERT(frame->GetParentFramesDoc()->GetLogicalDocument());
			/* Elements inside svg resource document, should take the root font
			   size from the top svg document (parent doc in this step, which if
			   also svg resource, should have parent's root font size already set). */
			root_props.SetRootFontSize(frame->GetParentFramesDoc()->GetLogicalDocument()->GetLayoutWorkplace()->GetDocRootProperties().GetRootFontSize());
			updating_root_font_size = FALSE;
		}
	}
#endif // SVG_SUPPORT

#ifdef CURRENT_STYLE_SUPPORT
	Reset(&parent_props, (HLDocProfile*) NULL, parent_props.types != NULL);
#else // CURRENT_STYLE_SUPPORT
	Reset(&parent_props);
#endif // CURRENT_STYLE_SUPPORT

	PrefsCollectionDisplay::RenderingModes rendering_mode = PrefsCollectionDisplay::MSR;
	PrefsCollectionDisplay::RenderingModes prev_rendering_mode = PrefsCollectionDisplay::MSR;

	int flexible_fonts = FLEXIBLE_FONTS_DOCUMENT_ONLY;
	int apply_doc_styles = APPLY_DOC_STYLES_YES;
	int avoid_flicker = 0;

#ifdef _DEBUG
	OP_ASSERT(!element->IsPropsDirty() || !Markup::IsRealElement(element->Type()) || layout_workplace->AllowDirtyChildPropsDebug());
#endif

	if (use_era_setting)
	{
		switch (layout_mode)
		{
		case LAYOUT_SSR:
			rendering_mode = PrefsCollectionDisplay::SSR;
			prev_rendering_mode = PrefsCollectionDisplay::CSSR;
			break;

		case LAYOUT_CSSR:
			rendering_mode = PrefsCollectionDisplay::CSSR;
			prev_rendering_mode = PrefsCollectionDisplay::AMSR;
			break;

		case LAYOUT_AMSR:
			rendering_mode = PrefsCollectionDisplay::AMSR;
			prev_rendering_mode = PrefsCollectionDisplay::AMSR;
			break;

#ifdef TV_RENDERING
		case LAYOUT_TVR:
			rendering_mode = PrefsCollectionDisplay::TVR;
			prev_rendering_mode = PrefsCollectionDisplay::TVR;
			break;
#endif // TV_RENDERING

		default:
			break;
		}

		flexible_fonts = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FlexibleFonts));
		apply_doc_styles = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::DownloadAndApplyDocumentStyleSheets));
		avoid_flicker = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::AvoidInterlaceFlicker));

		if (is_ssr_or_cssr)
		{
			if (flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_ONLY)
				flexible_fonts = FLEXIBLE_FONTS_PREDEFINED_ONLY;
			else
				if (flexible_fonts == FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED)
					flexible_fonts = FLEXIBLE_FONTS_PREVIOUS_TO_PREDEFINED;
		}
		else
			if (layout_mode == LAYOUT_MSR && flexible_fonts == FLEXIBLE_FONTS_PREVIOUS_TO_PREDEFINED)
				flexible_fonts = FLEXIBLE_FONTS_DOCUMENT_TO_PREDEFINED;
	}

	Markup::Type resolved_element_type = elm_type;

	if (elm_ns == NS_HTML)
		switch (elm_type)
		{
		case Markup::HTE_TEXTAREA:
			// Textarea does not inherit whitespace in IE. Mozilla doesn't support whitespace in textarea.
			// Do as IE:
			white_space = CSS_VALUE_normal;

			/* Default box-sizing for form elements roughly goes like this:

			   - button-like elements: always border-box
			   - common text edit fields: border-box in quirks mode, content-box in strict mode
			   - other elements: always content-box

			   This is based on observations in IE and Mozilla. */

			box_sizing = hld_profile->IsInStrictMode() ? CSS_VALUE_content_box : CSS_VALUE_border_box;
			break;

		case Markup::HTE_INPUT:
			switch (element->GetInputType())
			{
			case INPUT_CHECKBOX:
			case INPUT_RADIO:
			case INPUT_SUBMIT:
			case INPUT_RESET:
			case INPUT_BUTTON:
			case INPUT_FILE:
				box_sizing = CSS_VALUE_border_box;
				break;

			case INPUT_TEXT:
			case INPUT_PASSWORD:
				box_sizing = hld_profile->IsInStrictMode() ? CSS_VALUE_content_box : CSS_VALUE_border_box;
				break;

			default:
				box_sizing = CSS_VALUE_content_box;
				break;
			}
			break;

		case Markup::HTE_TABLE:
		case Markup::HTE_BUTTON:
		case Markup::HTE_SELECT:
			box_sizing = CSS_VALUE_border_box;
			break;
		case Markup::HTE_OBJECT:
			{
				URL tmp_url = element->GetImageURL(TRUE, hld_profile->GetLogicalDocument());
				RETURN_IF_ERROR(element->GetResolvedObjectType(&tmp_url, resolved_element_type, hld_profile->GetLogicalDocument()));
			}
			break;
		}

	FlexContent* flexbox = NULL;

	if (!container)
		if (HTML_Element* elm = parent_cascade->html_element)
			if (Box* box = elm->GetLayoutBox())
				if ((flexbox = box->GetFlexContent()) != NULL)
				{
					/* This is either a flexbox item, or a child that needs an
					   anonymous flexbox item parent. */

					SetIsFlexItem();

#ifdef WEBKIT_OLD_FLEXBOX
					if (flexbox->IsOldspecFlexbox())
					{
						// In the old spec, items were not initially flexible.

						flex_grow = 0.0;
						flex_shrink = 0.0;
						flex_basis = CONTENT_SIZE_AUTO;

						// And there was also no min-width/height:auto magic.

						min_width = LayoutCoord(0);
						min_height = LayoutCoord(0);
					}
#endif // WEBKIT_OLD_FLEXBOX
				}

	HTML_Element* parent = element->Parent();

	if (container)
	{
		containing_block_width = container->CalculateContentWidth(parent_props);
		containing_block_height = container->GetHeight() -
			LayoutCoord(parent_props.border.top.width) - LayoutCoord(parent_props.border.bottom.width) -
			parent_props.padding_top - parent_props.padding_bottom;
	}
	else
		if (flexbox)
		{
			// This is a flex item.

			containing_block_width = flexbox->GetWidth() -
				(LayoutCoord(parent_props.border.left.width) +
				 LayoutCoord(parent_props.border.right.width) +
				 parent_props.padding_left +
				 parent_props.padding_right);

			if (parent_props.content_height >= 0)
				containing_block_height = parent_props.content_height;
			else
				if (parent_props.GetHeightIsPercent())
					containing_block_height = parent_props.ResolvePercentageHeight(parent_props.containing_block_height);
				else
					containing_block_height = parent_props.containing_block_height;

			if (parent_props.box_sizing == CSS_VALUE_border_box)
			{
				containing_block_height -=
					LayoutCoord(parent_props.border.top.width) +
					LayoutCoord(parent_props.border.bottom.width) +
					parent_props.padding_top +
					parent_props.padding_bottom;

				if (containing_block_height < 0)
					containing_block_height = LayoutCoord(0);
			}
		}
		else
		{
			containing_block_width = parent_props.containing_block_width;
			containing_block_height = parent_props.containing_block_height;
		}

	SetHasDecorationAncestors(parent_props.GetHasDecorationAncestors() || parent_props.text_decoration & (TEXT_DECORATION_OVERLINE | TEXT_DECORATION_LINETHROUGH | TEXT_DECORATION_UNDERLINE));

	BOOL is_replaced_elm = IsReplacedElement(elm_ns, resolved_element_type, hld_profile);

	BOOL is_form_elm = elm_ns == NS_HTML && (elm_type == Markup::HTE_INPUT || elm_type == Markup::HTE_SELECT
#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_
											 || elm_type == Markup::HTE_KEYGEN
#endif
											 || elm_type == Markup::HTE_TEXTAREA);

	if (!element->GetIsPseudoElement())
	{
		// set values given by html attributes
		RETURN_IF_ERROR(SetCssPropertiesFromHtmlAttr(element, parent_props, hld_profile));
	}
	else if (elm_type == Markup::HTE_Q && elm_ns == NS_HTML)
	{
		OP_ASSERT(element->GetIsPseudoElement());

		g_quotes[0].value_type = CSS_IDENT;
		g_quotes[0].value.type = element->GetIsBeforePseudoElement() ? CSS_VALUE_open_quote : CSS_VALUE_close_quote;
		g_array_decl.Set(g_quotes, 1);
		content_cp = &g_array_decl;
	}

	if (updating_root_font_size)
		root_props.SetRootFontSize(decimal_font_size);

	// Elements inside replaced content dont have a lot of freedom.
	if (parent_props.IsInsideReplacedContent() ||
		(parent_cascade->html_element &&
		 parent_cascade->html_element->GetLayoutBox() &&
		 parent_cascade->html_element->GetLayoutBox()->IsContentReplaced()
#ifdef SVG_SUPPORT
		 && parent_cascade->html_element->GetLayoutBox()->GetSVGContent() == NULL
#endif // SVG_SUPPORT
		 ))
		SetIsInsideReplacedContent(TRUE);

	int css_prop_len = element->GetCssPropLen();

	if (flexible_fonts != FLEXIBLE_FONTS_DOCUMENT_ONLY &&
		(!css_prop_len ||
		 (css_properties[0].info.type != CSSPROPS_FONT ||
		  (css_prop_len == 1 || css_properties[1].info.type != CSSPROPS_FONT))))
		SetERA_FontSizes(element,
						parent_props,
						rendering_mode,
						prev_rendering_mode,
						flexible_fonts,
						doc->GetFlexFontScale());

	/* Used for every prop except font size.
	   Since percentage base varies, it is set before every call of
	   CSSLengthResolver::GetLengthInPixelsFromProp. */
	CSSLengthResolver length_resolver(vd, FALSE, 0.0f, decimal_font_size_constrained, font_number, root_props.GetRootFontSize());

	for (int i = 0; i < css_prop_len; i++)
	{
		CssPropertyItem* pi = &css_properties[i];

		switch (pi->info.type)
		{
		case CSSPROPS_COMMON:
			OP_ASSERT(i == 0);

			// float
			if (pi->value.common_props.info.float_type != CSS_FLOAT_value_not_set)
			{
				float_type = CssFloatMap[pi->value.common_props.info.float_type];
				if (float_type == CSS_VALUE_inherit)
					float_type = parent_props.float_type;
			}

			// text-align
			if (pi->value.common_props.info.text_align != CSS_TEXT_ALIGN_value_not_set)
			{
				text_align = CssTextAlignValueMap[pi->value.common_props.info.text_align];
				align_block_elements = CSS_VALUE_none;
				if (text_align == CSS_VALUE_inherit)
					text_align = parent_props.text_align;
				else if (text_align != CSS_VALUE_default)
				{
					SetBidiAlignSpecified();
					SetAlignSpecified();
					if (pi->info.info2.extra)
					{
						switch (elm_type)
						{
						case Markup::HTE_P:
						case Markup::HTE_DIV:
						case Markup::HTE_CENTER:
						case Markup::HTE_TD:
						case Markup::HTE_TH:
						case Markup::HTE_TR:
						case Markup::HTE_TBODY:
						case Markup::HTE_THEAD:
						case Markup::HTE_TFOOT:
							align_block_elements = text_align;
							break;
						}
					}
				}
				else
					text_align = CSS_VALUE_left;
			}

			// text-decoration
			if (pi->value.common_props.info.inherit_text_decoration)
				text_decoration = parent_props.text_decoration;
			else if (pi->value.common_props.info.text_decoration_set)
				text_decoration = pi->value.common_props.info.text_decoration;

			// display
			if (pi->value.common_props.info.display_type != CSS_DISPLAY_value_not_set)
			{
				short tmp_display_type = CssDisplayMap[pi->value.common_props.info.display_type];

				if (tmp_display_type == CSS_VALUE_inherit)
					tmp_display_type = parent_props.display_type;

				display_type = tmp_display_type;

#ifdef WEBKIT_OLD_FLEXBOX
				if (display_type == CSS_VALUE__webkit_box)
				{
					display_type = CSS_VALUE_flex;
					SetIsOldspecFlexbox();
				}
				else
					if (display_type == CSS_VALUE__webkit_inline_box)
					{
						display_type = CSS_VALUE_inline_flex;
						SetIsOldspecFlexbox();
					}
#endif // WEBKIT_OLD_FLEXBOX
			}

			// visibility
			if (pi->value.common_props.info.visibility != CSS_VISIBILITY_value_not_set &&
				pi->value.common_props.info.visibility != CSS_VISIBILITY_inherit)
			{
				visibility = CssVisibilityMap[pi->value.common_props.info.visibility];
#ifdef CSS_TRANSITIONS
				transition_packed.visibility = 0;
#endif // CSS_TRANSITIONS
			}

			// white-space
			if (pi->value.common_props.info.white_space != CSS_WHITE_SPACE_value_not_set)
			{
				white_space = CssWhiteSpaceMap[pi->value.common_props.info.white_space];
				if (white_space == CSS_VALUE_inherit)
					white_space = parent_props.white_space;
			}

			// clear
			if (pi->value.common_props.info.clear != CSS_CLEAR_value_not_set)
			{
				clear = CssClearMap[pi->value.common_props.info.clear];
				if (clear == CSS_VALUE_inherit)
					clear = parent_props.clear;
			}

			// position
			if (pi->value.common_props.info.position != CSS_POSITION_value_not_set)
			{
				position = CssPositionMap[pi->value.common_props.info.position];
				if (position == CSS_VALUE_inherit)
					position = parent_props.position;
			}

			if ((position == CSS_VALUE_absolute || position == CSS_VALUE_fixed))
			{
				// recalculate containing_block_width and containing_block_height

				// FIXME: This isn't entirely correct for paged media, is it?

				containing_block_width = hld_profile->GetLayoutWorkplace()->GetLayoutViewWidth();
				containing_block_height = hld_profile->GetLayoutWorkplace()->GetLayoutViewHeight();

				HTML_Element* containing_element = Box::GetContainingElement(element, TRUE, position == CSS_VALUE_fixed);

				if (containing_element)
				{
					Box* containing_box = containing_element->GetLayoutBox();

					for (LayoutProperties* parent = parent_cascade; parent; parent = parent->Pred())
						if (parent->html_element == containing_element)
						{
							const HTMLayoutProperties &props = *parent->GetProps();
							containing_block_width = containing_box->GetContainingBlockWidth() - LayoutCoord(props.border.left.width + props.border.right.width);
							containing_block_height = containing_box->GetContainingBlockHeight() - LayoutCoord(props.border.top.width + props.border.bottom.width);

							if (containing_box->IsInlineBox() && !containing_box->IsInlineBlockBox())
							{
								containing_block_width -= props.padding_left + props.padding_right;
								containing_block_height -= props.padding_top + props.padding_bottom;
							}
							break;
						}
				}
			}

			break;

		case CSSPROPS_WRITING_SYSTEM:
			OP_ASSERT(i == 0 || (i == 1 && css_properties[0].info.type == CSSPROPS_COMMON));
			if (current_script != pi->value.writing_system)
			{
				current_script = pi->value.writing_system;

				if (current_font_type == CurrentFontTypeDefault)
				{
					FontAtt* font = GetFontForElm(element, current_script);
					font_number = styleManager->GetFontNumber(font->GetFaceName());
				}
				else if (current_font_type == CurrentFontTypeGeneric)
					font_number = styleManager->GetGenericFontNumber((StyleManager::GenericFont)current_generic_font, current_script);
			}

			break;

		case CSSPROPS_FONT:
			{
				OP_ASSERT(i == 0 ||
					(i == 1 && (css_properties[0].info.type == CSSPROPS_COMMON || css_properties[0].info.type == CSSPROPS_WRITING_SYSTEM)) ||
					(i == 2 && css_properties[0].info.type == CSSPROPS_COMMON && css_properties[1].info.type == CSSPROPS_WRITING_SYSTEM));

				// font-weight
				if (pi->value.font_props.info.weight != CSS_FONT_WEIGHT_inherit &&
					pi->value.font_props.info.weight != CSS_FONT_WEIGHT_value_not_set)
				{
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
					{
						if (pi->value.font_props.info.weight < 10)
							types->m_font_weight = CSS_NUMBER;
						else
							types->m_font_weight = CSS_IDENT;
					}
#endif // CURRENT_STYLE_SUPPORT

					font_weight = CssFontWeightMap[pi->value.font_props.info.weight];
#ifdef CSS_TRANSITIONS
					transition_packed.font_weight = 0;
#endif // CSS_TRANSITIONS

					if (font_weight == CSS_VALUE_bolder || font_weight == CSS_VALUE_lighter)
					{
						font_weight = styleManager->GetNextFontWeight(parent_props.font_weight, font_weight == CSS_VALUE_bolder);
					}
				}

				// font-style
				if (pi->value.font_props.info.style != CSS_FONT_STYLE_value_not_set)
				{
					font_italic = CssFontStyleMap[pi->value.font_props.info.style];

					if (font_italic == FONT_STYLE_INHERIT)
						font_italic = parent_props.font_italic;
				}

				// font-variant
				if (pi->value.font_props.info.variant != CSS_FONT_VARIANT_value_not_set)
				{
					small_caps = CssFontVariantMap[pi->value.font_props.info.variant];
					if (small_caps == CSS_VALUE_inherit)
						small_caps = parent_props.small_caps;
				}

				// font-size
				if (pi->value.font_props.info.size != CSS_FONT_SIZE_value_not_set)
				{
					if (pi->value.font_props.info.size == CSS_FONT_SIZE_inherit)
					{
#ifdef CURRENT_STYLE_SUPPORT
						if (types)
						{
							font_size = parent_props.font_size;
							decimal_font_size = parent_props.decimal_font_size;
							types->m_font_size = parent_props.types->m_font_size;
							break; // jump out of the case
						}
						else
#endif // CURRENT_STYLE_SUPPORT
							decimal_font_size = parent_props.decimal_font_size;
					}
					else if (pi->value.font_props.info.size == CSS_FONT_SIZE_larger)
					{
#ifdef CURRENT_STYLE_SUPPORT
						if (types)
						{
							font_size = CSS_VALUE_larger;
							types->m_font_size = CSS_IDENT;
							break; // jump out of the case
						}
						else
#endif // CURRENT_STYLE_SUPPORT
						{
							LayoutFixed base_decimal_font_size;
#ifdef SVG_SUPPORT
							if (svg)
								base_decimal_font_size = parent_props.decimal_font_size;
							else
#endif // SVG_SUPPORT
								base_decimal_font_size = ConstrainFixedFontSize(parent_props.decimal_font_size);

							decimal_font_size = MultLayoutFixedByQuotient(base_decimal_font_size, 6, 5);
						}
					}
					else if (pi->value.font_props.info.size == CSS_FONT_SIZE_smaller)
					{
#ifdef CURRENT_STYLE_SUPPORT
						if (types)
						{
							font_size = CSS_VALUE_smaller;
							types->m_font_size = CSS_IDENT;
							break; // jump out of the case
						}
						else
#endif // CURRENT_STYLE_SUPPORT
						{
							LayoutFixed base_decimal_font_size;
#ifdef SVG_SUPPORT
							if (svg)
								base_decimal_font_size = parent_props.decimal_font_size;
							else
#endif // SVG_SUPPORT
								base_decimal_font_size = ConstrainFixedFontSize(parent_props.decimal_font_size);

							decimal_font_size = MultLayoutFixedByQuotient(base_decimal_font_size, 4, 5);
						}
					}
					else
					{
						BOOL use_font_size = TRUE;
						if ((pi->info.info2.extra & 0x80) != 0)
						{
							switch (elm_type)
							{
							case Markup::HTE_PRE:
							case Markup::HTE_LISTING:
							case Markup::HTE_PLAINTEXT:
							case Markup::HTE_XMP:
							case Markup::HTE_CODE:
							case Markup::HTE_SAMP:
							case Markup::HTE_TT:
							case Markup::HTE_KBD:
								use_font_size = !parent_props.GetFontSizeSpecified();
								break;
							}
						}

						if (use_font_size)
						{
							float ffont_size = float(pi->value.font_props.info.size);
							if (pi->value.font_props.info.size == CSS_FONT_SIZE_use_lang_def)
							{
								FontAtt* font = GetFontForElm(element, current_script);
								int size = element->GetFontSizeDefined() ? element->GetFontSize() : 3;
								ffont_size = float(styleManager->GetFontSize(font, size));
							}

							if (pi->info.info2.extra)
							{
								ffont_size *= LAYOUT_FIXED_POINT_BASE;
								ffont_size += (float)(pi->info.info2.extra & LAYOUT_FIXED_POINT_BITS);
							}

							SetFontSizeSpecified();
#ifdef CSS_TRANSITIONS
							transition_packed.font_size = 0;
#endif // CSS_TRANSITIONS

							/* Use decimal font size (fixed point value) for more precise computations.
							   Also because decimal_font_size is not affected by MIN/MAX settings. */
							CSSLengthResolver font_resolver(vd,
															/* Result in layout fixed */ TRUE,
															LayoutFixedToFloat(parent_props.decimal_font_size),
															parent_props.decimal_font_size,
															parent_props.font_number,
															root_props.GetRootFontSize(),
															/* Do not constrain the root font size */ FALSE);
							decimal_font_size = GetLengthInPixelsFromProp(pi->value.font_props.info.size_type,
																		  int(ffont_size),
																		  pi->info.info2.val_percentage,
																		  pi->info.info2.extra != 0,
																		  font_resolver,
																		  SHORT_AS_LAYOUT_FIXED_POINT_MIN,
																		  SHORT_AS_LAYOUT_FIXED_POINT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																		  , types ? &types->m_font_size : NULL
#endif // CURRENT_STYLE_SUPPORT
								);

#ifdef CURRENT_STYLE_SUPPORT
							if (types)
							{
								font_size = LayoutFixedToShortNonNegative(decimal_font_size);
								break;
							}
#endif // CURRENT_STYLE_SUPPORT
						}
					}

#ifdef SVG_SUPPORT
					if (svg)
						svg->fontsize = LayoutFixedToSVGNumber(decimal_font_size);
#endif // SVG_SUPPORT
				}

				if (flexible_fonts != FLEXIBLE_FONTS_DOCUMENT_ONLY)
					SetERA_FontSizes(element, parent_props,
									 rendering_mode, prev_rendering_mode,
									 flexible_fonts, doc->GetFlexFontScale());
				else
				{
					decimal_font_size_constrained = ConstrainFixedFontSize(decimal_font_size);
					font_size = LayoutFixedToShortNonNegative(decimal_font_size_constrained);
				}

				// New font size, new em/ex base for other props.
				length_resolver.SetFontSize(decimal_font_size_constrained);
				if (updating_root_font_size)
				{
					// <html> or :root or <svg> can change the rem unit base.
					root_props.SetRootFontSize(decimal_font_size);
					length_resolver.SetRootFontSize(decimal_font_size);
				}
			}
			break;

		case CSSPROPS_OTHER2:
			// opacity
			if (pi->value.other_props2.info.inherit_opacity)
			{
				opacity = parent_props.opacity;
#ifdef CSS_TRANSITIONS
				transition_packed2.opacity = parent_props.transition_packed2.opacity;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.other_props2.info.opacity_set)
				opacity = pi->value.other_props2.info.opacity;

			// overflow-wrap
			if (pi->value.other_props2.info.overflow_wrap != CSS_OVERFLOW_WRAP_value_not_set)
			{
				overflow_wrap = CssWordWrapMap[pi->value.other_props2.info.overflow_wrap];
				if (overflow_wrap == CSS_VALUE_inherit)
					overflow_wrap = parent_props.overflow_wrap;
			}

			// object-fit
			if (pi->value.other_props2.info.object_fit != CSS_OBJECT_FIT_value_not_set)
			{
				object_fit_position.fit = CssObjectFitMap[pi->value.other_props2.info.object_fit];
				if (object_fit_position.fit == CSS_VALUE_inherit)
					object_fit_position.fit = parent_props.object_fit_position.fit;
			}

			// box-decoration-break
			if (pi->value.other_props2.info.box_decoration_break != CSS_BOX_DECORATION_BREAK_value_not_set)
			{
				box_decoration_break = CssBoxDecorationBreakMap[pi->value.other_props2.info.box_decoration_break];
				if (box_decoration_break == CSS_VALUE_inherit)
					box_decoration_break = parent_props.box_decoration_break;
			}

			if (pi->value.other_props2.info.overflowx != CSS_OVERFLOW_value_not_set ||
				pi->value.other_props2.info.overflowy != CSS_OVERFLOW_value_not_set)
			{
				// overflow-x
				if (pi->value.other_props2.info.overflowx != CSS_OVERFLOW_value_not_set)
				{
					computed_overflow_x = CssOverflowMap[pi->value.other_props2.info.overflowx];
					if (computed_overflow_x == CSS_VALUE_inherit)
						computed_overflow_x = parent_props.computed_overflow_x;
				}

				// overflow-y
				if (pi->value.other_props2.info.overflowy != CSS_OVERFLOW_value_not_set)
				{
					computed_overflow_y = CssOverflowMap[pi->value.other_props2.info.overflowy];
					if (computed_overflow_y == CSS_VALUE_inherit)
						computed_overflow_y = parent_props.computed_overflow_y;
				}

				// Resolve invalid combinations of overflow-x and overflow-y

				if (computed_overflow_x != computed_overflow_y)
#ifdef PAGED_MEDIA_SUPPORT
					// If one of the properties specifies -o-paged*, they must both be equal:

					if (IsPagedOverflowValue(computed_overflow_x) || IsPagedOverflowValue(computed_overflow_y))
					{
						// This is how howcome suggested that conflicts be resolved:

						if (computed_overflow_x == CSS_VALUE__o_paged_x_controls || computed_overflow_y == CSS_VALUE__o_paged_x_controls)
							computed_overflow_x = computed_overflow_y = CSS_VALUE__o_paged_x_controls;
						else
							if (computed_overflow_x == CSS_VALUE__o_paged_x || computed_overflow_y == CSS_VALUE__o_paged_x)
								computed_overflow_x = computed_overflow_y = CSS_VALUE__o_paged_x;
							else
								if (computed_overflow_x == CSS_VALUE__o_paged_y_controls || computed_overflow_y == CSS_VALUE__o_paged_y_controls)
									computed_overflow_x = computed_overflow_y = CSS_VALUE__o_paged_y_controls;
								else
									computed_overflow_x = computed_overflow_y = CSS_VALUE__o_paged_y;
					}
					else
#endif // PAGED_MEDIA_SUPPORT
						// If one of the properties specifies non-visible overflow, so must the other one:

						if (computed_overflow_x == CSS_VALUE_visible)
							computed_overflow_x = CSS_VALUE_auto;
						else
							if (computed_overflow_y == CSS_VALUE_visible)
								computed_overflow_y = CSS_VALUE_auto;

				overflow_x = computed_overflow_x;
				overflow_y = computed_overflow_y;

				if (IsViewportMagicElement(element, hld_profile))
				{
					/* Overflow on HTML applies to the viewport, not the element
					   itself. If overflow on HTML is visible, overflow on BODY applies
					   to the viewport. In such cases, leave the computed values as
					   specified, but set used values to 'visible'. */

					if (elm_type == Markup::HTE_HTML ||
						parent_props.computed_overflow_x == CSS_VALUE_visible &&
						parent_props.computed_overflow_y == CSS_VALUE_visible)
						overflow_x = overflow_y = CSS_VALUE_visible;
				}
			}

			break;

		case CSSPROPS_FONT_NUMBER:
			// font-family
			if (pi->value.font_family_props.info.awaiting_webfont)
				SetAwaitingWebfont(TRUE);

			if (pi->value.font_family_props.info.type != CSS_FONT_FAMILY_value_not_set)
			{
				if (pi->value.font_family_props.info.type == CSS_FONT_FAMILY_inherit)
				{
					font_number = parent_props.font_number;
					SetAwaitingWebfont(parent_props.GetAwaitingWebfont());
				}
				else if (pi->value.font_family_props.info.type == CSS_FONT_FAMILY_use_lang_def)
				{
					current_font_type = CurrentFontTypeDefault;
					FontAtt* font = GetFontForElm(element, current_script);
					font_number = styleManager->GetFontNumber(font->GetFaceName());
				}
				else if (pi->value.font_family_props.info.type == CSS_FONT_FAMILY_generic)
				{
					current_font_type = CurrentFontTypeGeneric;
					current_generic_font = (GenericFont)pi->value.font_family_props.info.font_number;
					font_number = styleManager->GetGenericFontNumber((StyleManager::GenericFont)current_generic_font, current_script);
				}
				else
				{
					current_font_type = CurrentFontTypeDefined;
					font_number = short(pi->value.font_family_props.info.font_number);
				}

				length_resolver.SetFontNumber(font_number);
			}
			break;

		case CSSPROPS_COLOR:
			font_color = pi->value.color_val;
			if (font_color == CSS_COLOR_inherit)
				font_color = parent_props.font_color;
#ifdef CSS_TRANSITIONS
			else
				transition_packed.font_color = 0;
#endif // CSS_TRANSITIONS
			if (pi->info.info2.extra == 2)
			{
				/* This is a quirk. If a FONT element specifies a COLOR attribute, that cancels any
				   text-decoration set on parents that should normally be drawn under text in this
				   element, and instead sets text-decoration directly on this element. */

				if (overline_color != USE_DEFAULT_COLOR)
					text_decoration |= TEXT_DECORATION_OVERLINE;

				if (linethrough_color != USE_DEFAULT_COLOR)
					text_decoration |= TEXT_DECORATION_LINETHROUGH;

				if (underline_color != USE_DEFAULT_COLOR)
					text_decoration |= TEXT_DECORATION_UNDERLINE;

				SetHasDecorationAncestors(FALSE);
			}
			break;

		case CSSPROPS_TOP_BOTTOM:
			// top
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				top = parent_props.top;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_top = parent_props.types->m_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.top = parent_props.transition_packed2.top;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_auto)
				top = VPOSITION_VALUE_AUTO;
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				top = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
												pi->value.length2_val.info.val1,
												pi->info.info1.val1_percentage,
												pi->info.info1.val1_decimal,
												length_resolver.ChangePercentageBase(float(containing_block_height)),
												VPOSITION_VALUE_MIN,
												VPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												, types ? &types->m_top : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
					SetHasVerticalPropsWithPercent();
			}

			// bottom
			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				bottom = parent_props.bottom;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_bottom = parent_props.types->m_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.bottom = parent_props.transition_packed.bottom;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_auto)
				bottom = VPOSITION_VALUE_AUTO;
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				bottom = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
												   pi->value.length2_val.info.val2,
												   pi->info.info1.val2_percentage,
												   pi->info.info1.val2_decimal,
												   length_resolver.ChangePercentageBase(float(containing_block_height)),
												   VPOSITION_VALUE_MIN,
												   VPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												   , types ? &types->m_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val2_percentage)
					SetHasVerticalPropsWithPercent();
			}
			break;

		case CSSPROPS_LEFT_RIGHT:
			// left
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				left = parent_props.left;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_left = parent_props.types->m_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.left = parent_props.transition_packed2.left;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_auto)
				left = HPOSITION_VALUE_AUTO;
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				left = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
												 pi->value.length2_val.info.val1,
												 pi->info.info1.val1_percentage,
												 pi->info.info1.val1_decimal,
												 length_resolver.ChangePercentageBase(float(containing_block_width)),
												 HPOSITION_VALUE_MIN,
												 HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												 , types ? &types->m_left : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			// right
			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				right = parent_props.right;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_right = parent_props.types->m_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.right = parent_props.transition_packed2.right;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_auto)
				right = HPOSITION_VALUE_AUTO;
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				right = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
												  pi->value.length2_val.info.val2,
												  pi->info.info1.val2_percentage,
												  pi->info.info1.val2_decimal,
												  length_resolver.ChangePercentageBase(float(containing_block_width)),
												  HPOSITION_VALUE_MIN,
												  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												  , types ? &types->m_right : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val2_percentage)
					SetHasHorizontalPropsWithPercent();
			}
			break;

		case CSSPROPS_TOP:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				top = parent_props.top;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_top = parent_props.types->m_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.top = parent_props.transition_packed2.top;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				top = VPOSITION_VALUE_AUTO;
			else
			{
				top = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
												pi->value.length_val.info.val,
												pi->info.info1.val1_percentage,
												pi->info.info1.val1_decimal,
												length_resolver.ChangePercentageBase(float(containing_block_height)),
												VPOSITION_VALUE_MIN,
												VPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												, types ? &types->m_top : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
					SetHasVerticalPropsWithPercent();
			}
			break;

		case CSSPROPS_BOTTOM:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				bottom = parent_props.bottom;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_bottom = parent_props.types->m_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.bottom = parent_props.transition_packed.bottom;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				bottom = VPOSITION_VALUE_AUTO;
			else
			{
				bottom = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
												   pi->value.length_val.info.val,
												   pi->info.info1.val1_percentage,
												   pi->info.info1.val1_decimal,
												   length_resolver.ChangePercentageBase(float(containing_block_height)),
												   VPOSITION_VALUE_MIN,
												   VPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												   , types ? &types->m_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
					SetHasVerticalPropsWithPercent();
			}
			break;

		case CSSPROPS_LEFT:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				left = parent_props.left;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_left = parent_props.types->m_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.left = parent_props.transition_packed2.left;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				left = HPOSITION_VALUE_AUTO;
			else
			{
				left = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
												 pi->value.length_val.info.val,
												 pi->info.info1.val1_percentage,
												 pi->info.info1.val1_decimal,
												 length_resolver.ChangePercentageBase(float(containing_block_width)),
												 HPOSITION_VALUE_MIN,
												 HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												 , types ? &types->m_left : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_RIGHT:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				right = parent_props.right;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_right = parent_props.types->m_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.right = parent_props.transition_packed2.right;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				right = HPOSITION_VALUE_AUTO;
			else
			{
				right = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
												  pi->value.length_val.info.val,
												  pi->info.info1.val1_percentage,
												  pi->info.info1.val1_decimal,
												  length_resolver.ChangePercentageBase(float(containing_block_width)),
												  HPOSITION_VALUE_MIN,
												  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
												  , types ? &types->m_right : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
					SetHasHorizontalPropsWithPercent();
			}
			break;

		case CSSPROPS_ZINDEX:
			if ((signed long)pi->value.long_val == CSS_ZINDEX_inherit)
			{
				z_index = parent_props.z_index;
# ifdef CSS_TRANSITIONS
				transition_packed2.z_index = parent_props.transition_packed2.z_index;
# endif // CSS_TRANSITIONS
			}
			else
				z_index = (long) pi->value.long_val;
			break;

		case CSSPROPS_LINE_HEIGHT_VALIGN:
			{
				// line-height
				if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
				{
					line_height_i = parent_props.line_height_i;
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_line_height = parent_props.types->m_line_height;
#endif // CURRENT_STYLE_SUPPORT
				}
				else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_normal)
					line_height_i = LINE_HEIGHT_SQUEEZE_I;
				else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
				{
					if (pi->value.length2_val.info.val1_type == CSS_LENGTH_number && !pi->info.info1.val1_percentage)
					{
#ifdef CSS_TRANSITIONS
						transition_packed.line_height = 0;
#endif // CSS_TRANSITIONS

						if (pi->info.info1.val1_decimal)
							line_height_i = LayoutFixed(int(pi->value.length2_val.info.val1));
						else
							line_height_i = IntToLayoutFixed(pi->value.length2_val.info.val1);

						// negative value means <number>
						line_height_i = -line_height_i;
					}
					else
					{
						length_resolver.SetPercentageBase(LayoutFixedToFloat(decimal_font_size_constrained));
						length_resolver.SetResultInLayoutFixed(TRUE);
						line_height_i = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
																  pi->value.length2_val.info.val1,
																  pi->info.info1.val1_percentage,
																  pi->info.info1.val1_decimal,
																  length_resolver,
																  SHORT_AS_LAYOUT_FIXED_POINT_MIN,
																  SHORT_AS_LAYOUT_FIXED_POINT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																  , types ? &types->m_line_height : NULL
#endif // CURRENT_STYLE_SUPPORT
							);
						length_resolver.SetResultInLayoutFixed(FALSE);
					}
				}

				// vertical-align

				BOOL vertical_align_inherit = FALSE;

				if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
					vertical_align_inherit = TRUE;
				else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
				{
					if (pi->value.length2_val.info.val2_type == CSS_LENGTH_number && !pi->info.info1.val2_percentage)
					{
						// vertical-align type is stored by its enum value coded as unit CSS_LENGTH_number
						vertical_align_type = CssVerticalAlignMap[pi->value.length2_val.info.val2];

						if (vertical_align_type == CSS_VALUE_inherit)
							vertical_align_inherit = TRUE;

					}
					else
					{
						vertical_align_type = 0;
						length_resolver.SetPercentageBase(line_height_i == LINE_HEIGHT_SQUEEZE_I ?
														  float(font_ascent + font_descent) : LayoutFixedToFloat(GetCalculatedLineHeightFixedPoint(vd)));
						vertical_align = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
																   pi->value.length2_val.info.val2,
																   pi->info.info1.val2_percentage,
																   pi->info.info1.val2_decimal,
																   length_resolver,
																   SHRT_MIN,
																   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																   , types ? &types->m_vertical_align : NULL
#endif // CURRENT_STYLE_SUPPORT
							);
					}
				}

				if (!pi->info.info1.val2_default)
					SetVerticalAlignSpecified();

				if (vertical_align_inherit)
				{
					vertical_align = parent_props.vertical_align;
					vertical_align_type = parent_props.vertical_align_type;
# ifdef CSS_TRANSITIONS
					transition_packed2.valign = parent_props.transition_packed2.valign;
# endif // CSS_TRANSITIONS

					if (elm_ns == NS_HTML && (elm_type == Markup::HTE_TD || elm_type == Markup::HTE_TH))
						SetCellVAlignFromTableColumn(element, parent_cascade);
				}
			}
			break;

		case CSSPROPS_PADDING:
			// padding-left
			if (pi->value.length4_val.info.val1 == LENGTH_4_VALUE_inherit)
			{
				padding_left = parent_props.padding_left;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_left = parent_props.types->m_padding_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_left = parent_props.transition_packed2.padding_left;
#endif // CSS_TRANSITIONS

				if (parent_props.GetPaddingLeftIsPercent())
				{
					SetPaddingLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			else if (pi->value.length4_val.info.val1 != LENGTH_4_VALUE_not_set)
			{
				if (!pi->info.info1.val1_default)
					SetPaddingLeftSpecified();

				padding_left = GetLengthInPixelsFromProp(pi->value.length4_val.info.val1_type,
														 pi->value.length4_val.info.val1,
														 pi->info.info1.val1_percentage,
														 pi->info.info1.val1_decimal,
														 length_resolver.ChangePercentageBase(float(containing_block_width)),
														 MARGIN_PADDING_VALUE_MIN,
														 MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														 , types ? &types->m_padding_left : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			// padding-top
			if (pi->value.length4_val.info.val2 == LENGTH_4_VALUE_inherit)
			{
				padding_top = parent_props.padding_top;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_top = parent_props.types->m_padding_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_top = parent_props.transition_packed2.padding_top;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length4_val.info.val2 != LENGTH_4_VALUE_not_set)
			{
				padding_top = GetLengthInPixelsFromProp(pi->value.length4_val.info.val2_type,
														pi->value.length4_val.info.val2,
														pi->info.info1.val2_percentage,
														pi->info.info1.val2_decimal,
														length_resolver.ChangePercentageBase(float(containing_block_width)),
														MARGIN_PADDING_VALUE_MIN,
														MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														, types ? &types->m_padding_top : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val2_percentage)
				{
					SetPaddingTopIsPercent();
					SetHasHorizontalPropsWithPercent();
				}

			}

			// padding-right
			if (pi->value.length4_val.info.val3 == LENGTH_4_VALUE_inherit)
			{
				padding_right = parent_props.padding_right;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_right = parent_props.types->m_padding_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_right = parent_props.transition_packed2.padding_right;
#endif // CSS_TRANSITIONS

				if (parent_props.GetPaddingRightIsPercent())
				{
					SetPaddingRightIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			else if (pi->value.length4_val.info.val3 != LENGTH_4_VALUE_not_set)
			{
				if (!pi->info.info1.val4_default)
					SetPaddingRightSpecified();

				padding_right = GetLengthInPixelsFromProp(pi->value.length4_val.info.val3_type,
														  pi->value.length4_val.info.val3,
														  pi->info.info1.val3_percentage,
														  pi->info.info1.val3_decimal,
														  length_resolver.ChangePercentageBase(float(containing_block_width)),
														  MARGIN_PADDING_VALUE_MIN,
														  MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														  , types ? &types->m_padding_right : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val3_percentage)
				{
					SetPaddingRightIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			// padding-bottom
			if (pi->value.length4_val.info.val4 == LENGTH_4_VALUE_inherit)
			{
				padding_bottom = parent_props.padding_bottom;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_bottom = parent_props.types->m_padding_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_bottom = parent_props.transition_packed2.padding_bottom;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length4_val.info.val4 != LENGTH_4_VALUE_not_set)
			{
				padding_bottom = GetLengthInPixelsFromProp(pi->value.length4_val.info.val4_type,
														   pi->value.length4_val.info.val4,
														   pi->info.info1.val4_percentage,
														   pi->info.info1.val4_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   MARGIN_PADDING_VALUE_MIN,
														   MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_padding_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val4_percentage)
				{
					SetPaddingBottomIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_MARGIN:
			// margin-left
			if (pi->value.length4_val.info.val1 != LENGTH_4_VALUE_not_set)
			{
				if (!pi->info.info1.val1_default)
				{
					SetMarginLeftSpecified();
					SetMarginLeftExplicitlySet();
				}

				if (pi->value.length4_val.info.val1 == LENGTH_4_VALUE_inherit)
				{
					margin_left = parent_props.margin_left;

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_left = parent_props.types->m_margin_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_left = parent_props.transition_packed2.margin_left;
#endif // CSS_TRANSITIONS

					if (parent_props.GetMarginLeftIsPercent())
					{
						SetMarginLeftIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
				else if (pi->value.length4_val.info.val1 == LENGTH_4_VALUE_auto)
				{
					SetMarginLeftAuto(TRUE);
					margin_left = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_left = GetLengthInPixelsFromProp(pi->value.length4_val.info.val1_type,
															pi->value.length4_val.info.val1,
															pi->info.info1.val1_percentage,
															pi->info.info1.val1_decimal,
															length_resolver.ChangePercentageBase(float(containing_block_width)),
															MARGIN_PADDING_VALUE_MIN,
															MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															, types ? &types->m_margin_left : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginLeftIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			// margin-top
			if (pi->value.length4_val.info.val2 != LENGTH_4_VALUE_not_set)
			{
				if (!pi->info.info1.val2_default)
					SetMarginTopExplicitlySet();
				if (!pi->info.info1.val2_default || !pi->info.info1.val2_unspecified)
					SetMarginTopSpecified();

				if (pi->value.length4_val.info.val2 == LENGTH_4_VALUE_inherit)
				{
					margin_top = parent_props.margin_top;
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_top = parent_props.types->m_margin_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_top = parent_props.transition_packed2.margin_top;
#endif // CSS_TRANSITIONS

					if (parent_props.GetHasHorizontalPropsWithPercent())
						SetHasHorizontalPropsWithPercent();
				}
				else if (pi->value.length4_val.info.val2 == LENGTH_4_VALUE_auto)
				{
					SetMarginTopAuto(TRUE);
					margin_top = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_top = GetLengthInPixelsFromProp(pi->value.length4_val.info.val2_type,
														   pi->value.length4_val.info.val2,
														   pi->info.info1.val2_percentage,
														   pi->info.info1.val2_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   MARGIN_PADDING_VALUE_MIN,
														   MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_margin_top : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val2_percentage)
					{
						SetMarginTopIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			// margin-right
			if (pi->value.length4_val.info.val3 != LENGTH_4_VALUE_not_set)
			{
				if (!pi->info.info1.val3_default)
				{
					SetMarginRightSpecified();
					SetMarginRightExplicitlySet();
				}

				if (pi->value.length4_val.info.val3 == LENGTH_4_VALUE_inherit)
				{
					margin_right = parent_props.margin_right;

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_right = parent_props.types->m_margin_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_right = parent_props.transition_packed2.margin_right;
#endif // CSS_TRANSITIONS

					if (parent_props.GetMarginRightIsPercent())
					{
						SetMarginRightIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
				else if (pi->value.length4_val.info.val3 == LENGTH_4_VALUE_auto)
				{
					SetMarginRightAuto(TRUE);
					margin_right = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_right = GetLengthInPixelsFromProp(pi->value.length4_val.info.val3_type,
															 pi->value.length4_val.info.val3,
															 pi->info.info1.val3_percentage,
															 pi->info.info1.val3_decimal,
															 length_resolver.ChangePercentageBase(float(containing_block_width)),
															 MARGIN_PADDING_VALUE_MIN,
															 MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															 , types ? &types->m_margin_right : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val3_percentage)
					{
						SetMarginRightIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			// margin-bottom
			if (pi->value.length4_val.info.val4 != LENGTH_4_VALUE_not_set)
			{
				if (!pi->info.info1.val4_default)
					SetMarginBottomExplicitlySet();
				if (!pi->info.info1.val4_default || !pi->info.info1.val4_unspecified)
					SetMarginBottomSpecified();

				if (pi->value.length4_val.info.val4 == LENGTH_4_VALUE_inherit)
				{
					margin_bottom = parent_props.margin_bottom;
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_bottom = parent_props.types->m_margin_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_bottom = parent_props.transition_packed2.margin_bottom;
#endif // CSS_TRANSITIONS

					if (parent_props.GetHasHorizontalPropsWithPercent())
						SetHasHorizontalPropsWithPercent();
				}
				else if (pi->value.length4_val.info.val4 == LENGTH_4_VALUE_auto)
				{
					SetMarginBottomAuto(TRUE);
					margin_bottom = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_bottom = GetLengthInPixelsFromProp(pi->value.length4_val.info.val4_type,
															  pi->value.length4_val.info.val4,
															  pi->info.info1.val4_percentage,
															  pi->info.info1.val4_decimal,
															  length_resolver.ChangePercentageBase(float(containing_block_width)),
															  MARGIN_PADDING_VALUE_MIN,
															  MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_margin_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val4_percentage)
					{
						SetMarginBottomIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			break;

		case CSSPROPS_PADDING_TOP_BOTTOM:

			// padding-top
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				padding_top = parent_props.padding_top;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_top = parent_props.types->m_padding_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_top = parent_props.transition_packed2.padding_top;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				padding_top = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
														pi->value.length2_val.info.val1,
														pi->info.info1.val1_percentage,
														pi->info.info1.val1_decimal,
														length_resolver.ChangePercentageBase(float(containing_block_width)),
														MARGIN_PADDING_VALUE_MIN,
														MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														, types ? &types->m_padding_top : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingTopIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			// padding-bottom
			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				padding_bottom = parent_props.padding_bottom;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_bottom = parent_props.types->m_padding_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_bottom = parent_props.transition_packed2.padding_bottom;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				padding_bottom = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
														   pi->value.length2_val.info.val2,
														   pi->info.info1.val2_percentage,
														   pi->info.info1.val2_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   MARGIN_PADDING_VALUE_MIN,
														   MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_padding_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val2_percentage)
				{
					SetPaddingBottomIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_PADDING_LEFT_RIGHT:
			// padding-left
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				padding_left = parent_props.padding_left;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_left = parent_props.types->m_padding_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_left = parent_props.transition_packed2.padding_left;
#endif // CSS_TRANSITIONS

				if (parent_props.GetPaddingLeftIsPercent())
				{
					SetPaddingLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				if (!pi->info.info1.val1_default)
					SetPaddingLeftSpecified();

				padding_left = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
														 pi->value.length2_val.info.val1,
														 pi->info.info1.val1_percentage,
														 pi->info.info1.val1_decimal,
														 length_resolver.ChangePercentageBase(float(containing_block_width)),
														 MARGIN_PADDING_VALUE_MIN,
														 MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														 , types ? &types->m_padding_left : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			// padding-right
			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				padding_right = parent_props.padding_right;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_right = parent_props.types->m_padding_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_right = parent_props.transition_packed2.padding_right;
#endif // CSS_TRANSITIONS

				if (parent_props.GetPaddingRightIsPercent())
				{
					SetPaddingRightIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				if (!pi->info.info1.val2_default)
					SetPaddingRightSpecified();

				padding_right = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
														  pi->value.length2_val.info.val2,
														  pi->info.info1.val2_percentage,
														  pi->info.info1.val2_decimal,
														  length_resolver.ChangePercentageBase(float(containing_block_width)),
														  MARGIN_PADDING_VALUE_MIN,
														  MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														  , types ? &types->m_padding_right : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val2_percentage)
				{
					SetPaddingRightIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_PADDING_LEFT:
			// padding-left
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				padding_left = parent_props.padding_left;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_left = parent_props.types->m_padding_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_left = parent_props.transition_packed2.padding_left;
#endif // CSS_TRANSITIONS

				if (parent_props.GetPaddingLeftIsPercent())
				{
					SetPaddingLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			else
			{
				if (!pi->info.info1.val1_default)
					SetPaddingLeftSpecified();

				padding_left = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														 pi->value.length_val.info.val,
														 pi->info.info1.val1_percentage,
														 pi->info.info1.val1_decimal,
														 length_resolver.ChangePercentageBase(float(containing_block_width)),
														 MARGIN_PADDING_VALUE_MIN,
														 MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														 , types ? &types->m_padding_left : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingLeftIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_PADDING_RIGHT:
			// padding-right
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				padding_right = parent_props.padding_right;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_right = parent_props.types->m_padding_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_right = parent_props.transition_packed2.padding_right;
#endif // CSS_TRANSITIONS

				if (parent_props.GetPaddingRightIsPercent())
				{
					SetPaddingRightIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			else
			{
				if (!pi->info.info1.val1_default)
					SetPaddingRightSpecified();

				padding_right = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														  pi->value.length_val.info.val,
														  pi->info.info1.val1_percentage,
														  pi->info.info1.val1_decimal,
														  length_resolver.ChangePercentageBase(float(containing_block_width)),
														  MARGIN_PADDING_VALUE_MIN,
														  MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														  , types ? &types->m_padding_right : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingRightIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_PADDING_TOP:
			// padding-top
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				padding_top = parent_props.padding_top;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_top = parent_props.types->m_padding_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_top = parent_props.transition_packed2.padding_top;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else
			{
				padding_top = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														pi->value.length_val.info.val,
														pi->info.info1.val1_percentage,
														pi->info.info1.val1_decimal,
														length_resolver.ChangePercentageBase(float(containing_block_width)),
														MARGIN_PADDING_VALUE_MIN,
														MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														, types ? &types->m_padding_top : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingTopIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_PADDING_BOTTOM:
			// padding-bottom
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				padding_bottom = parent_props.padding_bottom;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_padding_bottom = parent_props.types->m_padding_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.padding_bottom = parent_props.transition_packed2.padding_bottom;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else
			{
				padding_bottom = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														   pi->value.length_val.info.val,
														   pi->info.info1.val1_percentage,
														   pi->info.info1.val1_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   MARGIN_PADDING_VALUE_MIN,
														   MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_padding_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetPaddingBottomIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_MARGIN_TOP_BOTTOM:
			// margin-top
			if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				if (!pi->info.info1.val1_default)
					SetMarginTopExplicitlySet();
				if (!pi->info.info1.val1_default || !pi->info.info1.val1_unspecified)
					SetMarginTopSpecified();

				if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
				{
					if (parent_props.GetMarginTopAuto())
						SetMarginTopAuto(TRUE);
					else
						if (parent_props.GetHasHorizontalPropsWithPercent())
							SetHasHorizontalPropsWithPercent();

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_top = parent_props.types->m_margin_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_top = parent_props.transition_packed2.margin_top;
#endif // CSS_TRANSITIONS

					margin_top = parent_props.margin_top;
				}
				else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_auto)
				{
					SetMarginTopAuto(TRUE);
					margin_top = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_top = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
														   pi->value.length2_val.info.val1,
														   pi->info.info1.val1_percentage,
														   pi->info.info1.val1_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   MARGIN_PADDING_VALUE_MIN,
														   MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_margin_top : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginTopIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			// margin-bottom
			if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				if (!pi->info.info1.val2_default)
					SetMarginBottomExplicitlySet();
				if (!pi->info.info1.val2_default || !pi->info.info1.val2_unspecified)
					SetMarginBottomSpecified();

				if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
				{
					if (parent_props.GetMarginBottomAuto())
						SetMarginBottomAuto(TRUE);
					else
						if (parent_props.GetHasHorizontalPropsWithPercent())
							SetHasHorizontalPropsWithPercent();

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_bottom = parent_props.types->m_margin_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_bottom = parent_props.transition_packed2.margin_bottom;
#endif // CSS_TRANSITIONS

					margin_bottom = parent_props.margin_bottom;
				}
				else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_auto)
				{
					SetMarginBottomAuto(TRUE);
					margin_bottom = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_bottom = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
															  pi->value.length2_val.info.val2,
															  pi->info.info1.val2_percentage,
															  pi->info.info1.val2_decimal,
															  length_resolver.ChangePercentageBase(float(containing_block_width)),
															  MARGIN_PADDING_VALUE_MIN,
															  MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_margin_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val2_percentage)
					{
						SetMarginBottomIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			break;

		case CSSPROPS_MARGIN_LEFT_RIGHT:
			// margin-left
			if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				if (!pi->info.info1.val1_default)
				{
					SetMarginLeftSpecified();
					SetMarginLeftExplicitlySet();
				}

				if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
				{
					if (parent_props.GetMarginLeftAuto())
						SetMarginLeftAuto(TRUE);

					margin_left = parent_props.margin_left;

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_left = parent_props.types->m_margin_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_left = parent_props.transition_packed2.margin_left;
#endif // CSS_TRANSITIONS

					if (parent_props.GetMarginLeftIsPercent())
					{
						SetMarginLeftIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
				else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_auto)
				{
					SetMarginLeftAuto(TRUE);
					margin_left = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_left = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
															pi->value.length2_val.info.val1,
															pi->info.info1.val1_percentage,
															pi->info.info1.val1_decimal,
															length_resolver.ChangePercentageBase(float(containing_block_width)),
															MARGIN_PADDING_VALUE_MIN,
															MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															, types ? &types->m_margin_left : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginLeftIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			// margin-right
			if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				if (!pi->info.info1.val2_default)
				{
					SetMarginRightSpecified();
					SetMarginRightExplicitlySet();
				}

				if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
				{
					if (parent_props.GetMarginRightAuto())
						SetMarginRightAuto(TRUE);

					margin_right = parent_props.margin_right;

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_right = parent_props.types->m_margin_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_right = parent_props.transition_packed2.margin_right;
#endif // CSS_TRANSITIONS

					if (parent_props.GetMarginRightIsPercent())
					{
						SetMarginRightIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
				else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_auto)
				{
					SetMarginRightAuto(TRUE);
					margin_right = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_right = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
															 pi->value.length2_val.info.val2,
															 pi->info.info1.val2_percentage,
															 pi->info.info1.val2_decimal,
															 length_resolver.ChangePercentageBase(float(containing_block_width)),
															 MARGIN_PADDING_VALUE_MIN,
															 MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															 , types ? &types->m_margin_right : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val2_percentage)
					{
						SetMarginRightIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}

			break;

		case CSSPROPS_MARGIN_TOP:
			// margin-top
			{
				if (!pi->info.info1.val1_default)
					SetMarginTopExplicitlySet();
				if (!pi->info.info1.val1_default || !pi->info.info1.val1_unspecified)
					SetMarginTopSpecified();

				if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
				{
					if (parent_props.GetMarginTopAuto())
						SetMarginTopAuto(TRUE);
					else
						if (parent_props.GetHasHorizontalPropsWithPercent())
							SetHasHorizontalPropsWithPercent();

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_top = parent_props.types->m_margin_top;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_top = parent_props.transition_packed2.margin_top;
#endif // CSS_TRANSITIONS

					margin_top = parent_props.margin_top;
				}
				else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				{
					SetMarginTopAuto(TRUE);
					margin_top = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_top = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														   pi->value.length_val.info.val,
														   pi->info.info1.val1_percentage,
														   pi->info.info1.val1_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   MARGIN_PADDING_VALUE_MIN,
														   MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_margin_top : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginTopIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}
			break;

		case CSSPROPS_MARGIN_BOTTOM:
			// margin-bottom
			{
				if (!pi->info.info1.val1_default)
					SetMarginBottomExplicitlySet();
				if (!pi->info.info1.val1_default || !pi->info.info1.val1_unspecified)
					SetMarginBottomSpecified();

				if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
				{
					if (parent_props.GetMarginBottomAuto())
						SetMarginBottomAuto(TRUE);
					else
						if (parent_props.GetHasHorizontalPropsWithPercent())
							SetHasHorizontalPropsWithPercent();

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_bottom = parent_props.types->m_margin_bottom;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_bottom = parent_props.transition_packed2.margin_bottom;
#endif // CSS_TRANSITIONS

					margin_bottom = parent_props.margin_bottom;
				}
				else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				{
					SetMarginBottomAuto(TRUE);
					margin_bottom = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_bottom = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															  pi->value.length_val.info.val,
															  pi->info.info1.val1_percentage,
															  pi->info.info1.val1_decimal,
															  length_resolver.ChangePercentageBase(float(containing_block_width)),
															  MARGIN_PADDING_VALUE_MIN,
															  MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_margin_bottom : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginBottomIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}
			break;

		case CSSPROPS_MARGIN_LEFT:
			// margin-left
			{
				if (!pi->info.info1.val1_default)
				{
					SetMarginLeftSpecified();
					SetMarginLeftExplicitlySet();
				}

				if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
				{
					if (parent_props.GetMarginLeftAuto())
						SetMarginLeftAuto(TRUE);

					margin_left = parent_props.margin_left;

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_left = parent_props.types->m_margin_left;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_left = parent_props.transition_packed2.margin_left;
#endif // CSS_TRANSITIONS

					if (parent_props.GetMarginLeftIsPercent())
					{
						SetMarginLeftIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
				else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				{
					SetMarginLeftAuto(TRUE);
					margin_left = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_left = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															pi->value.length_val.info.val,
															pi->info.info1.val1_percentage,
															pi->info.info1.val1_decimal,
															length_resolver.ChangePercentageBase(float(containing_block_width)),
															MARGIN_PADDING_VALUE_MIN,
															MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															, types ? &types->m_margin_left : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginLeftIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}
			break;

		case CSSPROPS_MARGIN_RIGHT:
			// margin-right
			{
				if (!pi->info.info1.val1_default)
				{
					SetMarginRightSpecified();
					SetMarginRightExplicitlySet();
				}

				if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
				{
					if (parent_props.GetMarginRightAuto())
						SetMarginRightAuto(TRUE);

					margin_right = parent_props.margin_right;

#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_margin_right = parent_props.types->m_margin_right;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
					transition_packed2.margin_right = parent_props.transition_packed2.margin_right;
#endif // CSS_TRANSITIONS

					if (parent_props.GetMarginRightIsPercent())
					{
						SetMarginRightIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
				else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				{
					SetMarginRightAuto(TRUE);
					margin_right = LayoutCoord(0); // cancel default value
				}
				else
				{
					margin_right = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															 pi->value.length_val.info.val,
															 pi->info.info1.val1_percentage,
															 pi->info.info1.val1_decimal,
															 length_resolver.ChangePercentageBase(float(containing_block_width)),
															 MARGIN_PADDING_VALUE_MIN,
															 MARGIN_PADDING_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
															 , types ? &types->m_margin_right : NULL
#endif // CURRENT_STYLE_SUPPORT
						);

					if (pi->info.info1.val1_percentage)
					{
						SetMarginRightIsPercent();
						SetHasHorizontalPropsWithPercent();
					}
				}
			}
			break;

		case CSSPROPS_BORDER_WIDTH:
			// border-left
			if (pi->value.length4_val.info.val1 == LENGTH_4_VALUE_inherit)
			{
				border.left.width = parent_props.border.left.width;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_left_width = parent_props.types->m_brd_left_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_left_width = parent_props.transition_packed.border_left_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length4_val.info.val1 != LENGTH_4_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.left.width = GetLengthInPixelsFromProp(pi->value.length4_val.info.val1_type,
															  pi->value.length4_val.info.val1,
															  FALSE,
															  pi->info.info1.val1_decimal,
															  length_resolver,
															  0,
															  SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_brd_left_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}

			// border-top
			if (pi->value.length4_val.info.val2 == LENGTH_4_VALUE_inherit)
			{
				border.top.width = parent_props.border.top.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_top_width = parent_props.types->m_brd_top_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_top_width = parent_props.transition_packed.border_top_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length4_val.info.val2 != LENGTH_4_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val2_percentage);
				border.top.width = GetLengthInPixelsFromProp(pi->value.length4_val.info.val2_type,
															 pi->value.length4_val.info.val2,
															 FALSE,
															 pi->info.info1.val2_decimal,
															 length_resolver,
															 0,
															 SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															 , types ? &types->m_brd_top_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}

			// border-right
			if (pi->value.length4_val.info.val3 == LENGTH_4_VALUE_inherit)
			{
				border.right.width = parent_props.border.right.width;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_right_width = parent_props.types->m_brd_right_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_right_width = parent_props.transition_packed.border_right_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length4_val.info.val3 != LENGTH_4_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val3_percentage);
				border.right.width = GetLengthInPixelsFromProp(pi->value.length4_val.info.val3_type,
															   pi->value.length4_val.info.val3,
															   FALSE,
															   pi->info.info1.val3_decimal,
															   length_resolver,
															   0,
															   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															   , types ? &types->m_brd_right_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}

			// border-bottom
			if (pi->value.length4_val.info.val4 == LENGTH_4_VALUE_inherit)
			{
				border.bottom.width = parent_props.border.bottom.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_bottom_width = parent_props.types->m_brd_bottom_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_bottom_width = parent_props.transition_packed.border_bottom_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length4_val.info.val4 != LENGTH_4_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val4_percentage);
				border.bottom.width = GetLengthInPixelsFromProp(pi->value.length4_val.info.val4_type,
																pi->value.length4_val.info.val4,
																FALSE,
																pi->info.info1.val4_decimal,
																length_resolver,
																0,
																SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																, types ? &types->m_brd_bottom_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_LEFT_RIGHT_WIDTH:
			// border-left
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				border.left.width = parent_props.border.left.width;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_left_width = parent_props.types->m_brd_left_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_left_width = parent_props.transition_packed.border_left_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.left.width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
															  pi->value.length2_val.info.val1,
															  FALSE,
															  pi->info.info1.val1_decimal,
															  length_resolver,
															  0,
															  SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_brd_left_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}

			// border-right
			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				border.right.width = parent_props.border.right.width;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_right_width = parent_props.types->m_brd_right_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_right_width = parent_props.transition_packed.border_right_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val2_percentage);
				border.right.width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
															   pi->value.length2_val.info.val2,
															   FALSE,
															   pi->info.info1.val2_decimal,
															   length_resolver,
															   0,
															   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															   , types ? &types->m_brd_right_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_TOP_BOTTOM_WIDTH:
			// border-top
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				border.top.width = parent_props.border.top.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_top_width = parent_props.types->m_brd_top_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_top_width = parent_props.transition_packed.border_top_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.top.width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
															 pi->value.length2_val.info.val1,
															 FALSE,
															 pi->info.info1.val1_decimal,
															 length_resolver,
															 0,
															 SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															 , types ? &types->m_brd_top_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}

			// border-bottom
			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				border.bottom.width = parent_props.border.bottom.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_bottom_width = parent_props.types->m_brd_bottom_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.border_bottom_width = parent_props.transition_packed.border_bottom_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				OP_ASSERT(!pi->info.info1.val2_percentage);
				border.bottom.width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
																pi->value.length2_val.info.val2,
																FALSE,
																pi->info.info1.val2_decimal,
																length_resolver,
																0,
																SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																, types ? &types->m_brd_bottom_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_TOP_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_2_VALUE_inherit)
			{
				border.top.width = parent_props.border.top.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_top_width = parent_props.types->m_brd_top_width;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.top.width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															 pi->value.length_val.info.val,
															 FALSE,
															 pi->info.info1.val1_decimal,
															 length_resolver,
															 0,
															 SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															 , types ? &types->m_brd_top_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_BOTTOM_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_2_VALUE_inherit)
			{
				border.bottom.width = parent_props.border.bottom.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_bottom_width = parent_props.types->m_brd_bottom_width;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.bottom.width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
																pi->value.length_val.info.val,
																FALSE,
																pi->info.info1.val1_decimal,
																length_resolver,
																0,
																SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																 , types ? &types->m_brd_bottom_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_LEFT_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_2_VALUE_inherit)
			{
				border.left.width = parent_props.border.left.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_left_width = parent_props.types->m_brd_left_width;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.left.width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															  pi->value.length_val.info.val,
															  FALSE,
															  pi->info.info1.val1_decimal,
															  length_resolver,
															  0,
															  SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_brd_left_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_RIGHT_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_2_VALUE_inherit)
			{
				border.right.width = parent_props.border.right.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_brd_right_width = parent_props.types->m_brd_right_width;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
			{
				OP_ASSERT(!pi->info.info1.val1_percentage);
				border.right.width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															   pi->value.length_val.info.val,
															   FALSE,
															   pi->info.info1.val1_decimal,
															   length_resolver,
															   0,
															   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															   , types ? &types->m_brd_right_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}
			break;

		case CSSPROPS_BORDER_COLOR:
			{
				COLORREF color = pi->value.color_val;
				if (color == CSS_COLOR_inherit)
				{
#ifdef CSS_TRANSITIONS
					transition_packed.border_top_color = parent_props.transition_packed.border_top_color;
					transition_packed.border_left_color = parent_props.transition_packed.border_left_color;
					transition_packed.border_right_color = parent_props.transition_packed.border_right_color;
					transition_packed.border_bottom_color = parent_props.transition_packed.border_bottom_color;
#endif // CSS_TRANSITIONS
					border.top.color = parent_props.border.top.color;
					border.left.color = parent_props.border.left.color;
					border.right.color = parent_props.border.right.color;
					border.bottom.color = parent_props.border.bottom.color;
				}
				else if (color != USE_DEFAULT_COLOR && color != CSS_COLOR_current_color)
				{
					border.top.color = color;
					border.left.color = color;
					border.right.color = color;
					border.bottom.color = color;
				}
			}
			break;

		case CSSPROPS_BORDER_STYLE:
			{
				if (pi->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
				{
					CSSValue style = CSSValue(CssBorderStyleMap[pi->value.border_props.info.style]);
					if (style == CSS_VALUE_inherit)
					{
						border.top.style = parent_props.border.top.style;
						border.left.style = parent_props.border.left.style;
						border.right.style = parent_props.border.right.style;
						border.bottom.style = parent_props.border.bottom.style;
					}
					else
					{
						border.top.style = style;
						border.left.style = style;
						border.right.style = style;
						border.bottom.style = style;
					}
				}
			}
			break;

		case CSSPROPS_BORDER_LEFT_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.border_left_color = parent_props.transition_packed.border_left_color;
#endif // CSS_TRANSITIONS
				border.left.color = parent_props.border.left.color;
			}
			else if (pi->value.color_val != USE_DEFAULT_COLOR &&
					 pi->value.color_val != CSS_COLOR_current_color)
				border.left.color = pi->value.color_val;
			break;

		case CSSPROPS_BORDER_LEFT_STYLE:
			if (pi->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
			{
				border.left.style = CssBorderStyleMap[pi->value.border_props.info.style];
				if (border.left.style == CSS_VALUE_inherit)
					border.left.style = parent_props.border.left.style;
			}
			break;

		case CSSPROPS_BORDER_RIGHT_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.border_right_color = parent_props.transition_packed.border_right_color;
#endif // CSS_TRANSITIONS
				border.right.color = parent_props.border.right.color;
			}
			else if (pi->value.color_val != USE_DEFAULT_COLOR &&
					 pi->value.color_val != CSS_COLOR_current_color)
				border.right.color = pi->value.color_val;
			break;

		case CSSPROPS_BORDER_RIGHT_STYLE:
			if (pi->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
			{
				border.right.style = CssBorderStyleMap[pi->value.border_props.info.style];
				if (border.right.style == CSS_VALUE_inherit)
					border.right.style = parent_props.border.right.style;
			}
			break;

		case CSSPROPS_BORDER_TOP_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.border_top_color = parent_props.transition_packed.border_top_color;
#endif // CSS_TRANSITIONS
				border.top.color = parent_props.border.top.color;
			}
			else if (pi->value.color_val != USE_DEFAULT_COLOR &&
					 pi->value.color_val != CSS_COLOR_current_color)
				border.top.color = pi->value.color_val;
			break;

		case CSSPROPS_BORDER_TOP_STYLE:
			if (pi->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
			{
				border.top.style = CssBorderStyleMap[pi->value.border_props.info.style];
				if (border.top.style == CSS_VALUE_inherit)
					border.top.style = parent_props.border.top.style;
			}
			break;

		case CSSPROPS_BORDER_BOTTOM_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.border_bottom_color = parent_props.transition_packed.border_bottom_color;
#endif // CSS_TRANSITIONS
				border.bottom.color = parent_props.border.bottom.color;
			}
			else if (pi->value.color_val != USE_DEFAULT_COLOR &&
					 pi->value.color_val != CSS_COLOR_current_color)
				border.bottom.color = pi->value.color_val;
			break;

		case CSSPROPS_BORDER_BOTTOM_STYLE:
			if (pi->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
			{
				border.bottom.style = CssBorderStyleMap[pi->value.border_props.info.style];
				if (border.bottom.style == CSS_VALUE_inherit)
					border.bottom.style = parent_props.border.bottom.style;
			}
			break;

		case CSSPROPS_BORDER_TOP_RIGHT_RADIUS:
		case CSSPROPS_BORDER_BOTTOM_RIGHT_RADIUS:
		case CSSPROPS_BORDER_BOTTOM_LEFT_RADIUS:
		case CSSPROPS_BORDER_TOP_LEFT_RADIUS:
			SetBorderRadius(pi, parent_props, length_resolver);
			break;

		case CSSPROPS_BORDER_IMAGE:
			if (pi->info.is_inherit)
			{
				border_image = parent_props.border_image;
				if (border_image.has_border_widths)
				{
					border.top.width = parent_props.border.top.width;
					border.left.width = parent_props.border.left.width;
					border.right.width = parent_props.border.right.width;
					border.bottom.width = parent_props.border.bottom.width;
				}
			}
			else
				ExtractBorderImage(doc, length_resolver, pi->value.css_decl, border_image, border);
			break;

#ifdef CSS_TRANSFORMS
		case CSSPROPS_TRANSFORM:
			{
				transforms_cp = GetDeclFromPropertyItem(*pi, parent_props.transforms_cp);
# ifdef CSS_TRANSITIONS
				if (transforms_cp == parent_props.transforms_cp)
					transition_packed2.transform = parent_props.transition_packed2.transform;
# endif // CSS_TRANSITIONS

			}
			break;

		case CSSPROPS_TRANSFORM_ORIGIN:
			SetTransformOrigin(*pi, parent_props, length_resolver);
			break;
#endif // CSS_TRANSFORMS

		case CSSPROPS_OUTLINE_WIDTH_OFFSET:
			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)

			{
				outline.width = parent_props.outline.width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_outline_width = parent_props.types->m_outline_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.outline_width = parent_props.transition_packed2.outline_width;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
				outline.width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
														  pi->value.length2_val.info.val1,
														  pi->info.info1.val1_percentage,
														  pi->info.info1.val1_decimal,
														  length_resolver.ChangePercentageBase(float(containing_block_width)),
														  SHRT_MIN,
														  SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
														  , types ? &types->m_outline_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				outline_offset = parent_props.outline_offset;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_outline_offset = parent_props.types->m_outline_offset;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.outline_offset = parent_props.transition_packed2.outline_offset;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
				outline_offset = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
														   pi->value.length2_val.info.val2,
														   pi->info.info1.val2_percentage,
														   pi->info.info1.val2_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_width)),
														   SHRT_MIN,
														   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_outline_offset : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;
		case CSSPROPS_OUTLINE_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
				outline.color = parent_props.outline.color;
#ifdef CSS_TRANSITIONS
				transition_packed2.outline_color = parent_props.transition_packed2.outline_color;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.color_val != USE_DEFAULT_COLOR)
				outline.color = pi->value.color_val;
			break;

		case CSSPROPS_OUTLINE_STYLE:
			if (pi->value.border_props.info.style != CSS_BORDER_STYLE_value_not_set)
			{
				outline.style = CssBorderStyleMap[pi->value.border_props.info.style];
				if (outline.style == CSS_VALUE_inherit)
					outline.style = parent_props.outline.style;

				if (outline.style == CSS_VALUE_none)
					SetOutlineNoneSpecified();
				else if (IsOutlineVisible())
					SetIsInOutline(TRUE);
			}
			break;

		case CSSPROPS_CURSOR:
			if (pi->info.is_inherit)
				cursor = parent_props.cursor;
			else if (pi->value.css_decl)
				; // URI cursor value not supported yet.
			else
				cursor = (CursorType) pi->info.info2.extra;
			break;

		case CSSPROPS_COLUMN_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				column_width = parent_props.column_width;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_column_width = parent_props.types->m_column_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.column_width = parent_props.transition_packed2.column_rule_width;
#endif // CSS_TRANSITIONS
			}
			else
				column_width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														 pi->value.length_val.info.val,
														 FALSE,
														 pi->info.info1.val1_decimal,
														 length_resolver.ChangePercentageBase(float(containing_block_width)),
														 0,
														 LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
														 , types ? &types->m_column_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;

		case CSSPROPS_COLUMN_COUNT:
			if (pi->info.is_inherit)
			{
				column_count = parent_props.column_count;
#ifdef CSS_TRANSITIONS
				transition_packed.column_count = parent_props.transition_packed.column_count;
#endif // CSS_TRANSITIONS
			}
			else
				column_count = pi->value.length_val.info.val;
			break;

		case CSSPROPS_COLUMN_RULE_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				column_rule.width = parent_props.column_rule.width;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_column_rule_width = parent_props.types->m_column_rule_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.column_rule_width = parent_props.transition_packed2.column_rule_width;
#endif // CSS_TRANSITIONS
			}
			else
				column_rule.width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
															  pi->value.length_val.info.val,
															  FALSE,
															  pi->info.info1.val1_decimal,
															  length_resolver.ChangePercentageBase(float(containing_block_width)),
															  0,
															  SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															  , types ? &types->m_column_rule_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;

		case CSSPROPS_COLUMN_GAP:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				column_gap = parent_props.column_gap;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_column_gap = parent_props.types->m_column_gap;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.column_gap = parent_props.transition_packed.column_gap;
#endif // CSS_TRANSITIONS
			}
			else
				column_gap = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
													   pi->value.length_val.info.val,
													   FALSE,
													   pi->info.info1.val1_decimal,
													   length_resolver.ChangePercentageBase(float(containing_block_width)),
													   0,
													   LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
													   , types ? &types->m_column_gap : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;

		case CSSPROPS_COLUMN_RULE_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
				column_rule.color = parent_props.column_rule.color;
#ifdef CSS_TRANSITIONS
				transition_packed2.column_rule_color = parent_props.transition_packed2.column_rule_color;
#endif // CSS_TRANSITIONS
			}
			else if (pi->value.color_val != USE_DEFAULT_COLOR &&
					 pi->value.color_val != CSS_COLOR_current_color)
				column_rule.color = pi->value.color_val;
			break;

		case CSSPROPS_COLUMN_RULE_STYLE:
			column_rule.style = CssBorderStyleMap[pi->value.border_props.info.style];
			if (column_rule.style == CSS_VALUE_inherit)
				column_rule.style = parent_props.column_rule.style;
			break;

		case CSSPROPS_MULTI_PANE_PROPS:
			if (pi->value.multi_pane_props.info.column_fill != CSS_COLUMN_FILL_value_not_set)
			{
				column_fill = CssColumnFillMap[pi->value.multi_pane_props.info.column_fill];

				if (column_fill == CSS_VALUE_inherit)
					column_fill = parent_props.column_fill;
			}

			if (pi->value.multi_pane_props.info.column_span != CSS_COLUMN_SPAN_value_not_set)
				switch (pi->value.multi_pane_props.info.column_span)
				{
				case CSS_COLUMN_SPAN_inherit:
					column_span = parent_props.column_span;
					break;
				case CSS_COLUMN_SPAN_none:
					column_span = 1;
					break;
				case CSS_COLUMN_SPAN_all:
					column_span = COLUMN_SPAN_ALL;
					break;
				default:
					column_span = pi->value.multi_pane_props.info.column_span;
					break;
				}

			if (column_span > 1 && container)
			{
				/* column-span affects width available for percentage resolution. Try to
				   figure out if it applies, and modify containing block width if it
				   does. */

				MultiColumnContainer* multicol_container = container->GetMultiColumnContainer();

				if (!multicol_container && parent_cascade->multipane_container)
					multicol_container = parent_cascade->multipane_container->GetMultiColumnContainer();

				if (multicol_container)
				{
					if (column_span == COLUMN_SPAN_ALL)
					{
						if ((display_type == CSS_VALUE_block || display_type == CSS_VALUE_list_item) &&
							position != CSS_VALUE_absolute && position != CSS_VALUE_fixed &&
							(CSS_is_gcpm_float_val(float_type) || float_type == CSS_VALUE_none))
							containing_block_width = multicol_container->CalculateContentWidth(parent_props, OUTER_CONTENT_WIDTH);
					}
					else
						if (CSS_is_gcpm_float_val(float_type))
							containing_block_width = containing_block_width * LayoutCoord(column_span) +
								multicol_container->GetColumnGap() * LayoutCoord(column_span - 1);
				}
			}

			break;

		case CSSPROPS_FLEX_GROW:
			// flex-grow

			if (pi->info.is_inherit)
				flex_grow = parent_props.flex_grow;
			else
				flex_grow = pi->value.float_val;

			break;

		case CSSPROPS_FLEX_SHRINK:
			// flex-shrink

			if (pi->info.is_inherit)
				flex_shrink = parent_props.flex_shrink;
			else
				flex_shrink = pi->value.float_val;

			break;

		case CSSPROPS_FLEX_BASIS:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				flex_basis = parent_props.flex_basis;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_flex_basis = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
			{
				flex_basis = CONTENT_SIZE_AUTO;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_flex_basis = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->info.info1.val1_percentage)
			{
				flex_basis = LayoutCoord(-pi->value.length_val.info.val);

				if (!pi->info.info1.val1_decimal)
				{
					if (flex_basis < -LAYOUT_FIXED_AS_INT_SAFE_MAX)
						flex_basis = LayoutCoord(-LAYOUT_FIXED_AS_INT_SAFE_MAX);

					flex_basis *= LAYOUT_FIXED_POINT_BASE;
				}

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_flex_basis = CSS_PERCENTAGE;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
				flex_basis = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
													   pi->value.length_val.info.val,
													   pi->info.info1.val1_percentage,
													   pi->info.info1.val1_decimal,
													   length_resolver,
													   LAYOUT_COORD_MIN,
													   LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
													   , types ? &types->m_flex_basis : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;

		case CSSPROPS_ORDER:
			// order

			if (pi->info.is_inherit)
			{
				order = parent_props.order;
#ifdef CSS_TRANSITIONS
				transition_packed3.order = parent_props.transition_packed3.order;
#endif // CSS_TRANSITIONS
			}
			else
				order = MIN(INT_MAX, MAX(INT_MIN, pi->value.long_val));

			break;

		case CSSPROPS_FLEX_MODES:
			// justify-content, align-content, align-items, align-self, flex-direction and flex-wrap

			flexbox_modes = pi->value.flexbox_modes;

			if (flexbox_modes.GetJustifyContent() == FlexboxModes::JCONTENT_INHERIT)
				flexbox_modes.SetJustifyContent(parent_props.flexbox_modes.GetJustifyContent());

			if (flexbox_modes.GetAlignContent() == FlexboxModes::ACONTENT_INHERIT)
				flexbox_modes.SetAlignContent(parent_props.flexbox_modes.GetAlignContent());

			if (flexbox_modes.GetAlignItems() == FlexboxModes::AITEMS_INHERIT)
				flexbox_modes.SetAlignItems(parent_props.flexbox_modes.GetAlignItems());

			if (flexbox_modes.GetAlignSelf() == FlexboxModes::ASELF_INHERIT)
				flexbox_modes.SetAlignSelf(parent_props.flexbox_modes.GetAlignSelf());

			if (flexbox_modes.GetDirection() == FlexboxModes::DIR_INHERIT)
				flexbox_modes.SetDirection(parent_props.flexbox_modes.GetDirection());

			if (flexbox_modes.GetWrap() == FlexboxModes::WRAP_INHERIT)
				flexbox_modes.SetWrap(parent_props.flexbox_modes.GetWrap());

			break;

		case CSSPROPS_BORDER_SPACING:
			// inheritance is done in Reset() for this property
			if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_inherit)
			{
				OP_ASSERT(pi->value.length2_val.info.val2 != LENGTH_2_VALUE_inherit);

#ifdef CSS_TRANSITIONS
				transition_packed.border_spacing = 0;
#endif // CSS_TRANSITIONS

				border_spacing_horizontal = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
																	  pi->value.length2_val.info.val1,
																	  pi->info.info1.val1_percentage,
																	  pi->info.info1.val1_decimal,
																	  length_resolver.ChangePercentageBase(float(containing_block_width)),
																	  SHRT_MIN,
																	  SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																	  , types ? &types->m_brd_spacing_hor : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				border_spacing_vertical = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
																	pi->value.length2_val.info.val2,
																	pi->info.info1.val2_percentage,
																	pi->info.info1.val2_decimal,
																	length_resolver.ChangePercentageBase(float(containing_block_height)),
																	SHRT_MIN,
																	SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
																	, types ? &types->m_brd_spacing_ver : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			}

			break;

		case CSSPROPS_TEXT_INDENT:
#ifdef CSS_TRANSITIONS
			transition_packed.text_indent = 0;
#endif // CSS_TRANSITIONS
			text_indent = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
													pi->value.length_val.info.val,
													pi->info.info1.val1_percentage,
													pi->info.info1.val1_decimal,
													length_resolver.ChangePercentageBase(float(containing_block_width)),
													SHRT_MIN,
													SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
													, types ? &types->m_text_indent : NULL
#endif // CURRENT_STYLE_SUPPORT
				);

			if (pi->info.info1.val1_percentage)
				SetTextIndentIsPercent();

			break;

		case CSSPROPS_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				content_width = parent_props.content_width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.width = parent_props.transition_packed2.width;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
			{
				content_width = CONTENT_WIDTH_AUTO;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_o_content_size)
			{
				// we only need to apply this to the elements with document content
				if ((elm_type == Markup::HTE_OBJECT || elm_type == Markup::HTE_IFRAME) && elm_ns == NS_HTML)
					content_width = CONTENT_WIDTH_O_SIZE;
				else
					content_width = CONTENT_WIDTH_AUTO;
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_o_skin)
			{
				content_width = CONTENT_WIDTH_O_SKIN;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->info.info1.val1_percentage)
			{
				content_width = LayoutCoord(-pi->value.length_val.info.val);

				if (!pi->info.info1.val1_decimal)
				{
					if (content_width < -LAYOUT_FIXED_AS_INT_SAFE_MAX)
						content_width = LayoutCoord(-LAYOUT_FIXED_AS_INT_SAFE_MAX);

					content_width *= LAYOUT_FIXED_POINT_BASE;
				}

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_PERCENTAGE;
#endif // CURRENT_STYLE_SUPPORT

				SetHasHorizontalPropsWithPercent();
			}
			else
				content_width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														  pi->value.length_val.info.val,
														  pi->info.info1.val1_percentage,
														  pi->info.info1.val1_decimal,
														  length_resolver.ChangePercentageBase(float(containing_block_width)),
														  LAYOUT_COORD_MIN,
														  LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
														  , types ? &types->m_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;

		case CSSPROPS_HEIGHT:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				content_height = parent_props.content_height;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_height = parent_props.types->m_height;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.height = parent_props.transition_packed2.height;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
				if (parent_props.GetHeightIsPercent())
					SetHeightIsPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
			{
				content_height = CONTENT_HEIGHT_AUTO;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_o_content_size)
			{
				// we only need to apply this to the elements with document content
				if ((elm_type == Markup::HTE_OBJECT || elm_type == Markup::HTE_IFRAME) && elm_ns == NS_HTML)
					content_height = CONTENT_HEIGHT_O_SIZE;
				else
					content_height = CONTENT_HEIGHT_AUTO;
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_o_skin)
			{
				content_height = CONTENT_HEIGHT_O_SKIN;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->info.info1.val1_percentage)
			{
				if (position != CSS_VALUE_absolute && position != CSS_VALUE_fixed && IsParentAutoHeight(hld_profile, container, flexbox, orig_parent_cascade))
					content_height = CONTENT_HEIGHT_AUTO;
 	 	 	 	else
				{
					content_height = LayoutCoord(-pi->value.length_val.info.val);
					if (!pi->info.info1.val1_decimal)
					{
						if (content_height < -LAYOUT_FIXED_AS_INT_SAFE_MAX)
							content_height = LayoutCoord(-LAYOUT_FIXED_AS_INT_SAFE_MAX);

						content_height *= LAYOUT_FIXED_POINT_BASE;
					}
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_height = CSS_PERCENTAGE;
#endif // CURRENT_STYLE_SUPPORT
				}

				SetHasVerticalPropsWithPercent();
				SetHeightIsPercent();
			}
			else
				content_height = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														   pi->value.length_val.info.val,
														   pi->info.info1.val1_percentage,
														   pi->info.info1.val1_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_height)),
														   0,
														   VPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_height : NULL
#endif // CURRENT_STYLE_SUPPORT
					);
			break;

		case CSSPROPS_WIDTH_HEIGHT:
			// width

			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				content_width = parent_props.content_width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.width = parent_props.transition_packed2.width;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_auto)
			{
				content_width = CONTENT_WIDTH_AUTO;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_o_content_size)
			{
				// we only need to apply this to the elements with document content
				if ((elm_type == Markup::HTE_OBJECT || elm_type == Markup::HTE_IFRAME) && elm_ns == NS_HTML)
					content_width = CONTENT_WIDTH_O_SIZE;
				else
					content_width = CONTENT_WIDTH_AUTO;
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_o_skin)
			{
				content_width = CONTENT_WIDTH_O_SKIN;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->info.info1.val1_percentage)
			{
				content_width = LayoutCoord(-pi->value.length2_val.info.val1);

				if (!pi->info.info1.val1_decimal)
				{
					OP_ASSERT(content_width >= -LAYOUT_FIXED_AS_INT_SAFE_MAX);
					content_width *= LAYOUT_FIXED_POINT_BASE;
				}

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_width = CSS_PERCENTAGE;
#endif // CURRENT_STYLE_SUPPORT
				SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
				content_width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
														  pi->value.length2_val.info.val1,
														  pi->info.info1.val1_percentage,
														  pi->info.info1.val1_decimal,
														  length_resolver.ChangePercentageBase(float(containing_block_width)),
														  0,
														  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														  , types ? &types->m_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

			// height

			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				content_height = parent_props.content_height;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.height = parent_props.transition_packed2.height;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
				if (parent_props.GetHeightIsPercent())
					SetHeightIsPercent();
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_auto)
			{
				content_height = CONTENT_HEIGHT_AUTO;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_o_content_size)
			{
				// we only need to apply this to the elements with document content
				if ((elm_type == Markup::HTE_OBJECT || elm_type == Markup::HTE_IFRAME) && elm_ns == NS_HTML)
					content_height = CONTENT_HEIGHT_O_SIZE;
				else
					content_height = CONTENT_HEIGHT_AUTO;
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_o_skin)
			{
				content_height = CONTENT_HEIGHT_O_SKIN;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else if (pi->info.info1.val2_percentage)
			{
				if (position != CSS_VALUE_absolute && position != CSS_VALUE_fixed && IsParentAutoHeight(hld_profile, container, flexbox, orig_parent_cascade))
					content_height = CONTENT_HEIGHT_AUTO;
 	 	 	 	else
				{
					content_height = LayoutCoord(-pi->value.length2_val.info.val2);
					if (!pi->info.info1.val2_decimal)
					{
						OP_ASSERT(content_height >= -LAYOUT_FIXED_AS_INT_SAFE_MAX);
						content_height *= LAYOUT_FIXED_POINT_BASE;
					}
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_height = CSS_PERCENTAGE;
#endif // CURRENT_STYLE_SUPPORT
				}

				SetHasVerticalPropsWithPercent();
				SetHeightIsPercent();
			}
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
				content_height = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
														   pi->value.length2_val.info.val2,
														   pi->info.info1.val2_percentage,
														   pi->info.info1.val2_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_height)),
														   0,
														   VPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_height : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

			break;

		case CSSPROPS_MIN_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				min_width = parent_props.min_width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_min_width = parent_props.types->m_min_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.min_width = parent_props.transition_packed2.min_width;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				min_width = CONTENT_SIZE_AUTO;
			else
			{
				min_width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
													  pi->value.length_val.info.val,
													  pi->info.info1.val1_percentage,
													  pi->info.info1.val1_decimal,
													  length_resolver.ChangePercentageBase(float(containing_block_width)),
													  0,
													  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
													  , types ? &types->m_min_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetMinWidthIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_MIN_HEIGHT:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				min_height = parent_props.min_height;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_min_height = parent_props.types->m_min_height;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.min_height = parent_props.transition_packed2.min_height;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_auto)
				min_height = CONTENT_SIZE_AUTO;
			else
			{
				length_resolver.SetPercentageBase(float(!pi->info.info1.val1_percentage || parent_props.content_height != CONTENT_HEIGHT_AUTO || position == CSS_VALUE_absolute || position == CSS_VALUE_fixed ? containing_block_height : 0));
				min_height = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
													   pi->value.length_val.info.val,
													   pi->info.info1.val1_percentage,
													   pi->info.info1.val1_decimal,
													   length_resolver,
													   0,
													   LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
													   , types ? &types->m_min_height : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetMinHeightIsPercent();
					SetHasVerticalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_MIN_WIDTH_HEIGHT:
			// min-width

			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				min_width = parent_props.min_width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_min_width = parent_props.types->m_min_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.min_width = parent_props.transition_packed2.min_width;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_auto)
				min_width = CONTENT_SIZE_AUTO;
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				min_width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
													  pi->value.length2_val.info.val1,
													  pi->info.info1.val1_percentage,
													  pi->info.info1.val1_decimal,
													  length_resolver.ChangePercentageBase(float(containing_block_width)),
													  0,
													  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
													  , types ? &types->m_min_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetMinWidthIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			// min-height

			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				min_height = parent_props.min_height;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_min_height = parent_props.types->m_min_height;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.min_height = parent_props.transition_packed2.min_height;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_auto)
				min_height = CONTENT_SIZE_AUTO;
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				length_resolver.SetPercentageBase(float(!pi->info.info1.val1_percentage || parent_props.content_height != CONTENT_HEIGHT_AUTO || position == CSS_VALUE_absolute || position == CSS_VALUE_fixed ? containing_block_height : 0));
				min_height = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
													   pi->value.length2_val.info.val2,
													   pi->info.info1.val2_percentage,
													   pi->info.info1.val2_decimal,
													   length_resolver,
													   0,
													   LAYOUT_COORD_MAX
#ifdef CURRENT_STYLE_SUPPORT
													   , types ? &types->m_min_height : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetMinHeightIsPercent();
					SetHasVerticalPropsWithPercent();
				}
			}
			break;

		case CSSPROPS_MAX_WIDTH:
			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				max_width = parent_props.max_width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_max_width = parent_props.types->m_max_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.max_width = parent_props.transition_packed2.max_width;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_none)
			{
				max_width = LayoutCoord(-1);
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_max_width = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
			{
				max_width = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
													  pi->value.length_val.info.val,
													  pi->info.info1.val1_percentage,
													  pi->info.info1.val1_decimal,
													  length_resolver.ChangePercentageBase(float(containing_block_width)),
													  0,
													  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
													  , types ? &types->m_max_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
					SetHasHorizontalPropsWithPercent();
			}
			normal_max_width = max_width;
			break;

		case CSSPROPS_MAX_HEIGHT:
			// All ERA modes ignore max-height
			if (layout_mode != LAYOUT_NORMAL)
				break;

			if (pi->value.length_val.info.val == LENGTH_VALUE_inherit)
			{
				max_height = parent_props.max_height;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_max_height = parent_props.types->m_max_height;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.max_height = parent_props.transition_packed2.max_height;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length_val.info.val == LENGTH_VALUE_none)
			{
				max_height = LayoutCoord(-1);
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_max_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
			}
			else
			{
				BOOL calculate = TRUE;

				if (pi->info.info1.val1_percentage)
				{
					if (parent_props.content_height == CONTENT_HEIGHT_AUTO && position != CSS_VALUE_absolute && position != CSS_VALUE_fixed)
					{
						calculate = FALSE;
						max_height = LayoutCoord(-1);
#ifdef CURRENT_STYLE_SUPPORT
						if (types)
							types->m_max_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
					}
					SetMaxHeightIsPercent();
					SetHasVerticalPropsWithPercent();
				}

				if (calculate)
					max_height = GetLengthInPixelsFromProp(pi->value.length_val.info.val_type,
														   pi->value.length_val.info.val,
														   pi->info.info1.val1_percentage,
														   pi->info.info1.val1_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_height)),
														   0,
														   LAYOUT_COORD_HALF_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_max_height : NULL
#endif // CURRENT_STYLE_SUPPORT
						);
			}
			break;

		case CSSPROPS_MAX_WIDTH_HEIGHT:
			// max-width

			if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_inherit)
			{
				max_width = parent_props.max_width;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_max_width = parent_props.types->m_max_width;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.max_width = parent_props.transition_packed2.max_width;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasHorizontalPropsWithPercent())
					SetHasHorizontalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_none)
			{
				max_width = LayoutCoord(-1);
			}
			else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
				max_width = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
													  pi->value.length2_val.info.val1,
													  pi->info.info1.val1_percentage,
													  pi->info.info1.val1_decimal,
													  length_resolver.ChangePercentageBase(float(containing_block_width)),
													  0,
													  HPOSITION_VALUE_MAX
#ifdef CURRENT_STYLE_SUPPORT
													  , types ? &types->m_max_width : NULL
#endif // CURRENT_STYLE_SUPPORT
					);

				if (pi->info.info1.val1_percentage)
				{
					SetMaxWidthIsPercent();
					SetHasHorizontalPropsWithPercent();
				}
			}

			normal_max_width = max_width;

			// max-height

			// All ERA modes ignore max-height
			if (layout_mode != LAYOUT_NORMAL)
				break;

			if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_inherit)
			{
				max_height = parent_props.max_height;
#ifdef CURRENT_STYLE_SUPPORT
				if (types)
					types->m_max_height = parent_props.types->m_max_height;
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed2.max_height = parent_props.transition_packed2.max_height;
#endif // CSS_TRANSITIONS

				if (parent_props.GetHasVerticalPropsWithPercent())
					SetHasVerticalPropsWithPercent();
			}
			else if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_none)
				max_height = LayoutCoord(-1);
			else if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
				BOOL calculate = TRUE;

				if (pi->info.info1.val2_percentage)
				{
					if (parent_props.content_height == CONTENT_HEIGHT_AUTO && position != CSS_VALUE_absolute && position != CSS_VALUE_fixed)
					{
						calculate = FALSE;
						max_height = LayoutCoord(-1);
#ifdef CURRENT_STYLE_SUPPORT
						if (types)
							types->m_max_height = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
					}
					SetMaxHeightIsPercent();
					SetHasVerticalPropsWithPercent();
				}

				if (calculate)
					max_height = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
														   pi->value.length2_val.info.val2,
														   pi->info.info1.val2_percentage,
														   pi->info.info1.val2_decimal,
														   length_resolver.ChangePercentageBase(float(containing_block_height)),
														   0,
														   LAYOUT_COORD_HALF_MAX
#ifdef CURRENT_STYLE_SUPPORT
														   , types ? &types->m_max_height : NULL
#endif // CURRENT_STYLE_SUPPORT
						);
			}

			break;

		case CSSPROPS_LIST_STYLE_IMAGE:
#ifdef CSS_GRADIENT_SUPPORT
			if (pi->info.info2.image_is_gradient)
				list_style_image_gradient = pi->value.gradient;
			else
#endif // CSS_GRADIENT_SUPPORT
				list_style_image = pi->value.url;
			break;

		case CSSPROPS_LETTER_WORD_SPACING:
			if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_inherit &&
				pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.word_spacing = 0;
#endif // CSS_TRANSITIONS
				if (pi->value.length2_val.info.val1 == LENGTH_2_VALUE_normal)
				{
					word_spacing_i = LayoutFixed(0);
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_word_spacing = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
				}
				else if (pi->value.length2_val.info.val1 != LENGTH_2_VALUE_not_set)
				{
					length_resolver.SetResultInLayoutFixed(TRUE);
					word_spacing_i = GetLengthInPixelsFromProp(pi->value.length2_val.info.val1_type,
															   pi->value.length2_val.info.val1,
															   pi->info.info1.val1_percentage,
															   pi->info.info1.val1_decimal,
															   length_resolver.ChangePercentageBase(float(containing_block_width)),
															   SHRT_MIN,
															   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															   , types ? &types->m_word_spacing : NULL
#endif // CURRENT_STYLE_SUPPORT
						);
					length_resolver.SetResultInLayoutFixed(FALSE);
				}
			}

			if (pi->value.length2_val.info.val2 != LENGTH_2_VALUE_inherit &&
				pi->value.length2_val.info.val2 != LENGTH_2_VALUE_not_set)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.letter_spacing = 0;
#endif // CSS_TRANSITIONS
				if (pi->value.length2_val.info.val2 == LENGTH_2_VALUE_normal)
				{
					letter_spacing = 0;
#ifdef CURRENT_STYLE_SUPPORT
					if (types)
						types->m_letter_spacing = CSS_IDENT;
#endif // CURRENT_STYLE_SUPPORT
				}
				else
					letter_spacing = GetLengthInPixelsFromProp(pi->value.length2_val.info.val2_type,
															   pi->value.length2_val.info.val2,
															   pi->info.info1.val2_percentage,
															   pi->info.info1.val2_decimal,
															   length_resolver.ChangePercentageBase(float(containing_block_width)),
															   SHRT_MIN,
															   SHRT_MAX
#ifdef CURRENT_STYLE_SUPPORT
															   , types ? &types->m_letter_spacing : NULL
#endif // CURRENT_STYLE_SUPPORT
						);
			}
			break;

		case CSSPROPS_OTHER:
#ifdef SUPPORT_TEXT_DIRECTION
			// direction
			if (pi->value.other_props.info.direction != CSS_DIRECTION_value_not_set)
			{
				direction = CssDirectionMap[pi->value.other_props.info.direction];
				if (direction == CSS_VALUE_inherit)
					direction = parent_props.direction;
			}

			// unicode-bidi
			if (pi->value.other_props.info.unicode_bidi != CSS_UNICODE_BIDI_value_not_set)
			{
				unicode_bidi = CssUnicodeBidiMap[pi->value.other_props.info.unicode_bidi];
				if (unicode_bidi == CSS_VALUE_inherit)
					unicode_bidi = parent_props.unicode_bidi;
			}
#endif

			// text-transform
			if (pi->value.other_props.info.text_transform != CSS_TEXT_TRANSFORM_value_not_set)
			{
				text_transform = CssTextTransformValueMap[pi->value.other_props.info.text_transform];
				if (text_transform == CSS_VALUE_inherit)
					text_transform = parent_props.text_transform;
			}

			// list-style-type
			if (pi->value.other_props.info.list_style_type != CSS_LIST_STYLE_TYPE_value_not_set)
			{
				list_style_type = CssListStyleTypeMap[pi->value.other_props.info.list_style_type];
				if (list_style_type == CSS_VALUE_inherit)
					list_style_type = parent_props.list_style_type;
			}

			// table-layout
			table_layout = CssTableLayoutMap[pi->value.other_props.info.table_layout];
			if (table_layout == CSS_VALUE_inherit)
				table_layout = parent_props.table_layout;

			// caption-side
			if (pi->value.other_props.info.caption_side != CSS_CAPTION_SIDE_value_not_set)
			{
				caption_side = CssCaptionSideMap[pi->value.other_props.info.caption_side];
				if (caption_side == CSS_VALUE_inherit)
					caption_side = parent_props.caption_side;
				else if (caption_side != CSS_VALUE_bottom)
					caption_side = CSS_VALUE_top;
			}

			// border-collapse
			if (pi->value.other_props.info.border_collapse != CSS_BORDER_COLLAPSE_value_not_set)
			{
				border_collapse = CssBorderCollapseMap[pi->value.other_props.info.border_collapse];
				if (border_collapse == CSS_VALUE_inherit)
					border_collapse = parent_props.border_collapse;
			}

			// empty-cells
			empty_cells = CssEmptyCellsMap[pi->value.other_props.info.empty_cells];
			if (empty_cells == CSS_VALUE_inherit)
				empty_cells = parent_props.empty_cells;
			else
				// Cancel the "show-empty-cells-but-only-background" quirk.

				SetSkipEmptyCellsBorder(FALSE);

			// list-style-position
			if (pi->value.other_props.info.list_style_pos != CSS_LIST_STYLE_POS_value_not_set)
			{
				list_style_pos = CssListStylePosMap[pi->value.other_props.info.list_style_pos];
				if (list_style_pos == CSS_VALUE_inherit)
					list_style_pos = parent_props.list_style_pos;
			}

			// text-overflow
			switch (pi->value.other_props.info.text_overflow)
			{
			case CSS_TEXT_OVERFLOW_clip:
				text_overflow = CSS_VALUE_clip;
				break;
			case CSS_TEXT_OVERFLOW_ellipsis:
				text_overflow = CSS_VALUE_ellipsis;
				break;
			case CSS_TEXT_OVERFLOW_ellipsis_lastline:
				text_overflow = CSS_VALUE__o_ellipsis_lastline;
				break;
			case CSS_TEXT_OVERFLOW_inherit:
				text_overflow = parent_props.text_overflow;
				break;
			}

			// resize
			if (pi->value.other_props.info.resize != CSS_RESIZE_value_not_set)
			{
				resize = CssResizeMap[pi->value.other_props.info.resize];
				if (resize == CSS_VALUE_inherit)
					resize = parent_props.resize;
			}

			// box-sizing
			if (pi->value.other_props.info.box_sizing != CSS_BOX_SIZING_value_not_set)
			{
				box_sizing = CssBoxSizingMap[pi->value.other_props.info.box_sizing];
				if (box_sizing == CSS_VALUE_inherit)
					box_sizing = parent_props.box_sizing;
			}

			break;

		case CSSPROPS_BG_IMAGE:
			if (pi->info.is_inherit)
				bg_images.SetImages(parent_props.bg_images.GetImages());
			else
				bg_images.SetImages(pi->value.css_decl);

			break;

		case CSSPROPS_BG_POSITION:
			if (pi->info.is_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.bg_pos = parent_props.transition_packed.bg_pos;
#endif // CSS_TRANSITIONS
				bg_images.SetPositions(parent_props.bg_images.GetPositions());
			}
			else
				bg_images.SetPositions(pi->value.css_decl);
			break;

		case CSSPROPS_BG_COLOR:
			if (pi->value.color_val == CSS_COLOR_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.bg_color = parent_props.transition_packed.bg_color;
#endif // CSS_TRANSITIONS
				bg_color = parent_props.bg_color;
			}
			else if (pi->value.color_val == CSS_COLOR_transparent)
			{
				bg_color = USE_DEFAULT_COLOR;
				SetBgIsTransparent(TRUE);
			}
			else
				bg_color = pi->value.color_val;
			break;

		case CSSPROPS_BG_REPEAT:
			if (pi->info.is_inherit)
				bg_images.SetRepeats(parent_props.bg_images.GetRepeats());
			else
				bg_images.SetRepeats(pi->value.css_decl);

			break;

		case CSSPROPS_BG_ATTACH:
			if (pi->info.is_inherit)
				bg_images.SetAttachs(parent_props.bg_images.GetAttachs());
			else
				bg_images.SetAttachs(pi->value.css_decl);

			break;

		case CSSPROPS_BG_ORIGIN:
			if (pi->info.is_inherit)
				bg_images.SetOrigins(parent_props.bg_images.GetOrigins());
			else
				bg_images.SetOrigins(pi->value.css_decl);

			break;

		case CSSPROPS_BG_CLIP:
			if (pi->info.is_inherit)
				bg_images.SetClips(parent_props.bg_images.GetClips());
			else
				bg_images.SetClips(pi->value.css_decl);

			break;

		case CSSPROPS_BG_SIZE:
			if (pi->info.is_inherit)
			{
#ifdef CSS_TRANSITIONS
				transition_packed.bg_size = parent_props.transition_packed.bg_size;
#endif // CSS_TRANSITIONS
				bg_images.SetSizes(parent_props.bg_images.GetSizes());
			}
			else
				bg_images.SetSizes(pi->value.css_decl);

			break;

		case CSSPROPS_OBJECT_POSITION:
			if (pi->info.is_inherit)
			{
				object_fit_position.x = parent_props.object_fit_position.x;
				object_fit_position.y = parent_props.object_fit_position.y;
				object_fit_position.x_percent = parent_props.object_fit_position.x_percent;
				object_fit_position.y_percent = parent_props.object_fit_position.y_percent;
#ifdef CSS_TRANSITIONS
				transition_packed2.object_position = parent_props.transition_packed2.object_position;
#endif // CSS_TRANSITIONS
			}
			else
			{
				CSS_decl* object_position_cp = pi->value.css_decl;
				if (object_position_cp && object_position_cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
				{
					OP_ASSERT(object_position_cp->ArrayValueLen() == 2);
					float pos[2];
					int pos_type[2];

					pos[0] = object_position_cp->GenArrayValue()[0].GetReal();
					pos_type[0] = object_position_cp->GenArrayValue()[0].GetValueType();

					if (pos_type[0] == CSS_PERCENTAGE)
					{
						object_fit_position.x = LayoutFixedAsLayoutCoord(FloatToLayoutFixed(pos[0]));
						object_fit_position.x_percent = TRUE;
					}
					else
					{
						object_fit_position.x = length_resolver.GetLengthInPixels(pos[0], pos_type[0]);
						object_fit_position.x_percent = FALSE;
					}

					pos[1] = object_position_cp->GenArrayValue()[1].GetReal();
					pos_type[1] = object_position_cp->GenArrayValue()[1].GetValueType();

					if (pos_type[1] == CSS_PERCENTAGE)
					{
						object_fit_position.y = LayoutFixedAsLayoutCoord(FloatToLayoutFixed(pos[1]));
						object_fit_position.y_percent = TRUE;
					}
					else
					{
						object_fit_position.y = length_resolver.GetLengthInPixels(pos[1], pos_type[1]);
						object_fit_position.y_percent = FALSE;
					}
				}
			}
			break;

		case CSSPROPS_CLIP:
			if (pi->info.is_inherit)
			{
				clip_left = parent_props.clip_left;
				clip_right = parent_props.clip_right;
				clip_top = parent_props.clip_top;
				clip_bottom = parent_props.clip_bottom;

#ifdef CURRENT_STYLE_SUPPORT
				if (types)
				{
					types->m_clip_top = parent_props.types->m_clip_top;
					types->m_clip_right = parent_props.types->m_clip_right;
					types->m_clip_bottom = parent_props.types->m_clip_bottom;
					types->m_clip_left = parent_props.types->m_clip_left;
				}
#endif // CURRENT_STYLE_SUPPORT
#ifdef CSS_TRANSITIONS
				transition_packed.clip = parent_props.transition_packed.clip;
#endif // CSS_TRANSITIONS
			}
			else
				if (pi->value.css_decl->GetDeclType() == CSS_DECL_NUMBER4)
				{
					CSS_number4_decl* clip_value = (CSS_number4_decl*) pi->value.css_decl;
					length_resolver.SetFontSize(parent_props.decimal_font_size_constrained);

#ifdef CURRENT_STYLE_SUPPORT
					clip_top = GetClipValue(clip_value, 0, length_resolver, types ? &types->m_clip_top : NULL);
					clip_right = GetClipValue(clip_value, 1, length_resolver, types ? &types->m_clip_right : NULL);
					clip_bottom = GetClipValue(clip_value, 2, length_resolver, types ? &types->m_clip_bottom : NULL);
					clip_left = GetClipValue(clip_value, 3, length_resolver, types ? &types->m_clip_left : NULL);
#else // CURRENT_STYLE_SUPPORT
					clip_top = GetClipValue(clip_value, 0, length_resolver);
					clip_right = GetClipValue(clip_value, 1, length_resolver);
					clip_bottom = GetClipValue(clip_value, 2, length_resolver);
					clip_left = GetClipValue(clip_value, 3, length_resolver);
#endif // CURRENT_STYLE_SUPPORT

					/* Restore that to current font_size, since clip is an exception,
					   taking parent's font size as an em or ex base. */
					length_resolver.SetFontSize(decimal_font_size_constrained);
				}
			break;

		case CSSPROPS_CONTENT:
			if (pi->info.is_inherit)
				content_cp = parent_props.content_cp;
			else
				content_cp = pi->value.css_decl;

			if (content_cp && content_cp->GetDeclType() == CSS_DECL_TYPE && content_cp->TypeValue() == CSS_VALUE_normal)
				content_cp = NULL;

			break;

		case CSSPROPS_COUNTER_RESET:
			if (pi->info.is_inherit)
				counter_reset_cp = parent_props.counter_reset_cp;
			else
				counter_reset_cp = pi->value.css_decl;
			break;

		case CSSPROPS_COUNTER_INCREMENT:
			if (pi->info.is_inherit)
				counter_inc_cp = parent_props.counter_inc_cp;
			else
				counter_inc_cp = pi->value.css_decl;
			break;

		case CSSPROPS_PAGE:
			// page-break-after
			page_props.break_after = CssPageBreakMap[pi->value.page_props.info.break_after];
			if (page_props.break_after == CSS_VALUE_inherit)
				page_props.break_after = parent_props.page_props.break_after;

			// page-break-before
			page_props.break_before = CssPageBreakMap[pi->value.page_props.info.break_before];
			if (page_props.break_before == CSS_VALUE_inherit)
				page_props.break_before = parent_props.page_props.break_before;

			// page-break-inside
			page_props.break_inside = CssPageBreakMap[pi->value.page_props.info.break_inside];
			if (page_props.break_inside == CSS_VALUE_inherit)
				page_props.break_inside = parent_props.page_props.break_inside;

			// orphans
			page_props.orphans = pi->value.page_props.info.orphans;
			if (page_props.orphans == CSS_ORPHANS_WIDOWS_inherit)
				page_props.orphans = parent_props.page_props.orphans;

			// widows
			page_props.widows = pi->value.page_props.info.widows;
			if (page_props.widows == CSS_ORPHANS_WIDOWS_inherit)
				page_props.widows = parent_props.page_props.widows;

			// page (inherited)
			if (pi->value.page_props.info.page == CSS_PAGE_auto)
				page_props.page = NULL;
			/*else if (pi->value.page_props.info.page == CSS_PAGE_identifier)
				; // find the value in CSSPROPS_PAGE_IDENTIFIER */

			break;

		case CSSPROPS_PAGE_IDENTIFIER:
			// page
			page_props.page = pi->value.string;
			break;

		case CSSPROPS_QUOTES:
			if (!pi->info.is_inherit)
				quotes_cp = pi->value.css_decl;
			break;

#ifdef GADGET_SUPPORT
        case CSSPROPS_CONTROL_REGION:
            if (!pi->info.is_inherit)
                control_region_cp = pi->value.css_decl;
            break;
#endif // GADGET_SUPPORT

		case CSSPROPS_SET_LINK_SOURCE:
		case CSSPROPS_USE_LINK_SOURCE:
			break;

		case CSSPROPS_WAP_MARQUEE:
			marquee_dir = CssWapMarqueeDirMap[pi->value.wap_marquee_props.info.dir];
			marquee_style = CssWapMarqueeStyleMap[pi->value.wap_marquee_props.info.style];
			if (pi->value.wap_marquee_props.info.speed != CSS_WAP_MARQUEE_SPEED_value_not_set)
				marquee_scrolldelay = CssWapMarqueeSpeedMap[pi->value.wap_marquee_props.info.speed];
			marquee_loop = pi->value.wap_marquee_props.info.loop;
			break;

#if defined(SVG_SUPPORT)
		case CSSPROPS_FILL:
		case CSSPROPS_STROKE:
		case CSSPROPS_FILL_OPACITY:
		case CSSPROPS_STROKE_OPACITY:
		case CSSPROPS_FILL_RULE:
		case CSSPROPS_STROKE_DASHOFFSET:
		case CSSPROPS_STROKE_DASHARRAY:
		case CSSPROPS_STROKE_MITERLIMIT:
		case CSSPROPS_STROKE_WIDTH:
		case CSSPROPS_STROKE_LINECAP:
		case CSSPROPS_STROKE_LINEJOIN:
		case CSSPROPS_TEXT_ANCHOR:
		case CSSPROPS_STOP_COLOR:
		case CSSPROPS_STOP_OPACITY:
		case CSSPROPS_FLOOD_COLOR:
		case CSSPROPS_FLOOD_OPACITY:
		case CSSPROPS_CLIP_RULE:
		case CSSPROPS_CLIP_PATH:
		case CSSPROPS_LIGHTING_COLOR:
		case CSSPROPS_MASK:
		case CSSPROPS_ENABLE_BACKGROUND:
		case CSSPROPS_FILTER:
		case CSSPROPS_POINTER_EVENTS:
		case CSSPROPS_ALIGNMENT_BASELINE:
		case CSSPROPS_BASELINE_SHIFT:
		case CSSPROPS_DOMINANT_BASELINE:
		case CSSPROPS_WRITING_MODE:
		case CSSPROPS_COLOR_INTERPOLATION:
		case CSSPROPS_COLOR_INTERPOLATION_FILTERS:
		case CSSPROPS_COLOR_PROFILE:
		case CSSPROPS_COLOR_RENDERING:
		case CSSPROPS_MARKER:
		case CSSPROPS_MARKER_END:
		case CSSPROPS_MARKER_MID:
		case CSSPROPS_MARKER_START:
		case CSSPROPS_SHAPE_RENDERING:
		case CSSPROPS_TEXT_RENDERING:
		case CSSPROPS_GLYPH_ORIENTATION_VERTICAL:
		case CSSPROPS_GLYPH_ORIENTATION_HORIZONTAL:
		case CSSPROPS_KERNING:
		case CSSPROPS_VECTOR_EFFECT:
		case CSSPROPS_AUDIO_LEVEL:
		case CSSPROPS_SOLID_COLOR:
		case CSSPROPS_DISPLAY_ALIGN:
		case CSSPROPS_SOLID_OPACITY:
		case CSSPROPS_VIEWPORT_FILL:
		case CSSPROPS_LINE_INCREMENT:
		case CSSPROPS_VIEWPORT_FILL_OPACITY:
		case CSSPROPS_BUFFERED_RENDERING:
			if (pi->value.css_decl)
			{
				if (!svg && !AllocateSVGProps(svg, parent_props.svg))
					return OpStatus::ERR_NO_MEMORY;
				svg->SetFromCSSDecl(pi->value.css_decl, parent_cascade, element, hld_profile);
			}
			break;
#endif // SVG_SUPPORT

		case CSSPROPS_IMAGE_RENDERING:
#ifdef SVG_SUPPORT
			// SVG animations can override this property
			if (svg && svg->HasOverriddenImageRendering())
				break;
#endif // SVG_SUPPORT

			if (!pi->info.is_inherit)
			{
				switch (pi->value.long_val)
				{
				case CSS_VALUE_auto:
				case CSS_VALUE_optimizeSpeed:
				case CSS_VALUE__o_crisp_edges:
				case CSS_VALUE_optimizeQuality:
					image_rendering = (short)pi->value.long_val;
					break;
				}
			}
			break;

		case CSSPROPS_TEXT_SHADOW:
			text_shadows.Set(GetDeclFromPropertyItem(*pi, parent_props.text_shadows.Get()));
#ifdef CSS_TRANSITIONS
			if (text_shadows.Get() != parent_props.text_shadows.Get())
				transition_packed.text_shadow = 0;
#endif // CSS_TRANSITIONS
			break;

		case CSSPROPS_BOX_SHADOW:
			box_shadows.Set(GetDeclFromPropertyItem(*pi, parent_props.box_shadows.Get()));
			break;

		case CSSPROPS_NAVUP:
			if (pi->info.is_inherit)
				navup_cp = parent_props.navup_cp;
			else
				navup_cp = pi->value.css_decl;
			break;
		case CSSPROPS_NAVDOWN:
			if (pi->info.is_inherit)
				navdown_cp = parent_props.navdown_cp;
			else
				navdown_cp = pi->value.css_decl;
			break;
		case CSSPROPS_NAVLEFT:
			if (pi->info.is_inherit)
				navleft_cp = parent_props.navleft_cp;
			else
				navleft_cp = pi->value.css_decl;
			break;
		case CSSPROPS_NAVRIGHT:
			if (pi->info.is_inherit)
				navright_cp = parent_props.navright_cp;
			else
				navright_cp = pi->value.css_decl;
			break;

#ifdef ACCESS_KEYS_SUPPORT
		case CSSPROPS_ACCESSKEY:
			if (pi->info.is_inherit)
				accesskey_cp = parent_props.accesskey_cp;
			else
				accesskey_cp = pi->value.css_decl;
			break;
#endif // ACCESS_KEYS_SUPPORT

#ifdef CSS_TRANSITIONS
		case CSSPROPS_TRANS_DELAY:
			if (pi->info.is_inherit)
				transition_delay = parent_props.transition_delay;
			else
				transition_delay = pi->value.css_decl;
			break;

		case CSSPROPS_TRANS_DURATION:
			if (pi->info.is_inherit)
				transition_duration = parent_props.transition_duration;
			else
				transition_duration = pi->value.css_decl;
			break;

		case CSSPROPS_TRANS_TIMING:
			if (pi->info.is_inherit)
				transition_timing = parent_props.transition_timing;
			else
				transition_timing = pi->value.css_decl;
			break;

		case CSSPROPS_TRANS_PROPERTY:
			if (pi->info.is_inherit)
				transition_property = parent_props.transition_property;
			else
				transition_property = pi->value.css_decl;
			break;
#endif // CSS_TRANSITIONS

#ifdef CSS_ANIMATIONS
		case CSSPROPS_ANIMATION_NAME:
			if (pi->info.is_inherit)
				animation_name = parent_props.animation_name;
			else
				animation_name = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_DURATION:
			if (pi->info.is_inherit)
				animation_duration = parent_props.animation_duration;
			else
				animation_duration = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_TIMING_FUNCTION:
			if (pi->info.is_inherit)
				animation_timing_function = parent_props.animation_timing_function;
			else
				animation_timing_function = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_ITERATION_COUNT:
			if (pi->info.is_inherit)
				animation_iteration_count = parent_props.animation_iteration_count;
			else
				animation_iteration_count = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_DIRECTION:
			if (pi->info.is_inherit)
				animation_direction = parent_props.animation_direction;
			else
				animation_direction = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_PLAY_STATE:
			if (pi->info.is_inherit)
				animation_play_state = parent_props.animation_play_state;
			else
				animation_play_state = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_DELAY:
			if (pi->info.is_inherit)
				animation_delay = parent_props.animation_delay;
			else
				animation_delay = pi->value.css_decl;
			break;

		case CSSPROPS_ANIMATION_FILL_MODE:
			if (pi->info.is_inherit)
				animation_fill_mode = parent_props.animation_fill_mode;
			else
				animation_fill_mode = pi->value.css_decl;
			break;
#endif // CSS_ANIMATIONS

		case CSSPROPS_TABLE:
			if ((signed long)pi->value.long_val != CSS_TABLE_BASELINE_not_set)
			{
				table_baseline = (long)pi->value.long_val;
				if (table_baseline == CSS_TABLE_BASELINE_inherit)
					table_baseline = parent_props.table_baseline;
			}
			if (pi->info.info2.extra)
				table_rules = pi->info.info2.extra;
			break;

		case CSSPROPS_TAB_SIZE:
			if (pi->value.long_val != CSS_TAB_SIZE_inherit)
				tab_size = short(pi->value.long_val);
			break;

		case CSSPROPS_SEL_COLOR:
			{
#ifdef CSS_TRANSITIONS
				transition_packed.selection_color = 0;
#endif // CSS_TRANSITIONS
				selection_color = pi->value.long_val;
			}
			break;

		case CSSPROPS_SEL_BGCOLOR:
			{
#ifdef CSS_TRANSITIONS
				transition_packed.selection_bgcolor = 0;
#endif // CSS_TRANSITIONS
				selection_bgcolor = pi->value.long_val;
			}
			break;

#ifdef CSS_MINI_EXTENSIONS
		case CSSPROPS_MINI:
			// opacity
			if (pi->value.mini_props.info.focus_opacity_set)
				focus_opacity = pi->value.mini_props.info.focus_opacity;
			// mini_fold
			if (pi->value.mini_props.info.fold != CSS_MINI_FOLD_value_not_set)
				mini_fold = CssMiniFoldMap[pi->value.mini_props.info.fold];
#endif // CSS_MINI_EXTENSIONS

		default:
			break;
		}
	}

	// This must be done after both background.bg_color and font_color is set.
	if (bg_color == CSS_COLOR_current_color)
		bg_color = font_color;

	if (selection_bgcolor == CSS_COLOR_current_color)
	{
#ifdef CSS_TRANSITIONS
		transition_packed.selection_bgcolor = transition_packed.font_color;
#endif // CSS_TRANSITIONS
		selection_bgcolor = font_color;
	}

	if (outline.color == CSS_COLOR_current_color)
	{
#ifdef CSS_TRANSITIONS
		transition_packed2.outline_color = transition_packed.font_color;
#endif // CSS_TRANSITIONS
		outline.color = font_color;
	}

	// ::selected pseudo element inherits from same element.
	if (selection_color == CSS_COLOR_inherit)
	{
#ifdef CSS_TRANSITIONS
		transition_packed.selection_color = transition_packed.font_color;
#endif // CSS_TRANSITIONS
		selection_color = font_color;
	}

	if (selection_bgcolor == CSS_COLOR_inherit)
	{
#ifdef CSS_TRANSITIONS
		transition_packed.selection_bgcolor = transition_packed.bg_color;
#endif // CSS_TRANSITIONS
		selection_bgcolor = bg_color;
	}

	if (column_gap == CONTENT_WIDTH_AUTO) // "normal"
		column_gap = LayoutCoord(font_size);

#ifdef CURRENT_STYLE_SUPPORT
	// don't do the rest of the stuff for current style
	if (types)
		return OpStatus::OK;
#endif // CURRENT_STYLE_SUPPORT

#ifdef SVG_SUPPORT
	// Make sure that if the parent had svg properties, the child must
	// inherit them.
	if (parent_props.svg != NULL && svg == NULL)
		if (!HTMLayoutProperties::AllocateSVGProps(svg, parent_props.svg))
			return OpStatus::ERR_NO_MEMORY;

	// This function must be called after all css properties that
	// concern font_color has been executed, since font_color is used
	// to resolve all the svg properties that depend the current
	// color, e.g. currentColor.
	if (svg != NULL)
		svg->Resolve(*this);
#endif // SVG_SUPPORT

	// ---------------------------------------------------------------------------------------------

	if (!GetFontSizeSpecified() && parent_props.GetFontSizeSpecified())
		SetFontSizeSpecified();

	if (parent_props.GetIsInOutline())
		SetIsInOutline(TRUE);

#if defined(SKIN_OUTLINE_SUPPORT)
	if (outline.style == CSS_VALUE__o_highlight_border)
	{
		OpSkinElement* skin_elm_inside = g_skin_manager->GetSkinElement("Active Element Inside");
		if (skin_elm_inside)
		{
			INT32 border_width;
			skin_elm_inside->GetBorderWidth(&border_width, &border_width, &border_width, &border_width, 0);
			outline.width = border_width;
		}
	}
#endif // SKIN_OUTLINE_SUPPORT

	if (!element->GetIsPseudoElement())
	{
		if (elm_type == Markup::HTE_DOC_ROOT)
		{
			position = CSS_VALUE_absolute;

			if (layout_workplace->UsingFlexRoot())
			{
				min_width = layout_workplace->GetLayoutViewMinWidth();
				max_width = layout_workplace->GetLayoutViewMaxWidth();
				normal_max_width = max_width;

				/* Min-width and max-width may not be fixed values here, so
				   they cannot be used to constrain min/max width propagation: */

				SetMinWidthIsPercent();
				SetMaxWidthIsPercent();
			}

#ifdef PAGED_MEDIA_SUPPORT
			if (IsPagedOverflowValue(layout_workplace->GetViewportOverflowX()))
			{
				/* Set @page margins as padding on the root paged container to achieve a
				   page margins effect. Note that this may give us negative padding, but
				   the layout engine seems to play along well with that. */

				CSS_Properties page_props;

				hld_profile->GetCSSCollection()->GetPageProperties(&page_props, FALSE, FALSE, doc->GetMediaType());
				GetPageMargins(layout_workplace, &page_props, containing_block_width, containing_block_height, TRUE, padding_top, padding_right, padding_bottom, padding_left);
			}
#endif // PAGED_MEDIA_SUPPORT
		}

		if (elm_ns == NS_HTML)
		{
			switch (elm_type)
			{
			case Markup::HTE_BODY:
				{
					root_props.SetBodyFontData(decimal_font_size_constrained, font_number, font_color);

#ifdef SUPPORT_TEXT_DIRECTION
					layout_workplace->SetRtlDocument(direction == CSS_VALUE_rtl);
#endif

				}
				break;

			case Markup::HTE_HR:
				if (text_align == CSS_VALUE_right || text_align == CSS_VALUE_center)
					SetMarginLeftAuto(TRUE);

				if (text_align == CSS_VALUE_center)
					SetMarginRightAuto(TRUE);

				overflow_x = overflow_y = CSS_VALUE_hidden;
				break;

			case Markup::HTE_SELECT:
				if (display_type == CSS_VALUE_table_cell)
					// Force inline instead of tablecell (bug 138395). Should we always do this?  /emil
					display_type = CSS_VALUE_inline;
				/* fall through */

			case Markup::HTE_INPUT:
			case Markup::HTE_TEXTAREA:
			case Markup::HTE_BUTTON:
				if (elm_type == Markup::HTE_BUTTON)
					switch (display_type)
					{
					case CSS_VALUE_inline_block:
					case CSS_VALUE_block:
					case CSS_VALUE_table_cell:
					case CSS_VALUE_none:
						break;

					default:
						// Anything else is illegal for BUTTON
						display_type = CSS_VALUE_inline_block;
						break;
					}

				if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableStylingOnForms, doc->GetHostName()))
				{
					border.top.width = 2;
					border.bottom.width = 2;
					border.left.width = 2;
					border.right.width = 2;
					border.top.style = BORDER_STYLE_NOT_SET;
					border.bottom.style = BORDER_STYLE_NOT_SET;
					border.left.style = BORDER_STYLE_NOT_SET;
					border.right.style = BORDER_STYLE_NOT_SET;
				}
				break;

			case Markup::HTE_FIELDSET:
				if (display_type == CSS_VALUE_inline)
				{
					/* inline fieldsets behave like inline blocks, but must not
					   honour width! */

					display_type = CSS_VALUE_inline_block;
					content_width = CONTENT_WIDTH_AUTO;
				}

				// Set our defaultcolor if we still have a groove border we set in SetCssPropertiesFromHtmlAttr.
				if (border.top.color == USE_DEFAULT_COLOR &&
					border.top.style == CSS_VALUE_groove && border.bottom.style == CSS_VALUE_groove &&
					border.left.style == CSS_VALUE_groove && border.right.style == CSS_VALUE_groove)
				{
#if defined (SKIN_SUPPORT)
					UINT32 col = g_skin_manager->GetFieldsetBorderColor();
#else
					UINT32 col = OP_RGB(222, 222, 222);
#endif
					border.top.color = col;
					border.bottom.color = col;
					border.left.color = col;
					border.right.color = col;
				}
				break;

			case Markup::HTE_LEGEND:
				switch (display_type)
				{
				case CSS_VALUE_inline_block:
				case CSS_VALUE_block:
				case CSS_VALUE_table_cell:
				case CSS_VALUE_none:
					break;

				default:
					// Anything else is illegal for LEGEND

					display_type = CSS_VALUE_block;
					break;
				}

				break;

#if defined(DOCUMENT_EDIT_SUPPORT) && defined(WIDGETS_IME_SUPPORT)
			case Markup::HTE_SPAN:
				if (doc->GetDocumentEdit())
					if (element->GetIMEStyling() == 1)
					{
						border.bottom.color = OP_RGB(255, 0, 0);
						border.bottom.width = 1;
						border.bottom.style= CSS_VALUE_solid;
						margin_bottom = LayoutCoord(-1);
					}
					else
						if (element->GetIMEStyling() == 2)
							bg_color = OP_RGB(128, 128, 128);

				break;
#endif
#ifdef MEDIA_HTML_SUPPORT
			case Markup::MEDE_VIDEO_DISPLAY:
				/* If the parent (<video>) has an absolute dimension
				   propagate it to the video display child.
				   If the parent has percentage dimension set 100% for
				   the child. */

				if (parent_props.content_width >= 0)
					content_width = parent_props.content_width;
				else
					if (parent_props.content_width >= CONTENT_WIDTH_MIN)
						content_width = IntToFixedPointPercentageAsLayoutCoord(100);

				if (parent_props.content_height >= 0)
					content_height = parent_props.content_height;
				else
					if (parent_props.content_height >= CONTENT_HEIGHT_MIN)
					{
						content_height = IntToFixedPointPercentageAsLayoutCoord(100);
						SetHeightIsPercent();
					}

				max_width = parent_props.max_width;
				min_width = parent_props.min_width;

				max_height = parent_props.max_height;
				min_height = parent_props.min_height;

				if (parent_props.box_sizing == CSS_VALUE_border_box)
				{
					LayoutCoord parent_horiz_border_padding =
						LayoutCoord(parent_props.border.left.width) +
						LayoutCoord(parent_props.border.right.width) +
						parent_props.padding_left +
						parent_props.padding_right;

					if (content_width >= 0)
						content_width = MAX(LayoutCoord(0), content_width - parent_horiz_border_padding);

					if (max_width >= 0)
						max_width = MAX(LayoutCoord(0), max_width - parent_horiz_border_padding);
					if (min_width >= 0)
						min_width = MAX(LayoutCoord(0), min_width - parent_horiz_border_padding);

					LayoutCoord parent_vert_border_padding =
						LayoutCoord(parent_props.border.top.width) +
						LayoutCoord(parent_props.border.bottom.width) +
						parent_props.padding_top +
						parent_props.padding_bottom;

					if (content_height >= 0)
						content_height = MAX(LayoutCoord(0), content_height - parent_vert_border_padding);

					if (max_height >= 0)
						max_height = MAX(LayoutCoord(0), max_height - parent_vert_border_padding);
					if (min_height >= 0)
						min_height = MAX(LayoutCoord(0), min_height - parent_vert_border_padding);
				}

				normal_max_width = max_width;

				object_fit_position = parent_props.object_fit_position;

				/* fall through */

			case Markup::MEDE_VIDEO_TRACKS:
			case Markup::MEDE_VIDEO_CONTROLS:
				display_type = CSS_VALUE_block;
				break;
#endif // MEDIA_HTML_SUPPORT
			}
		}
#ifdef _WML_SUPPORT_
		else
			if (elm_ns == NS_WML)
				if ((int)elm_type == WE_CARD)
					root_props.SetBodyFontData(decimal_font_size_constrained, font_number, font_color);
#endif // _WML_SUPPORT_
	}

	// ---------------------------------------------------------------------------------------------
	// resolve conflicting property values

	if (position == CSS_VALUE_absolute || position == CSS_VALUE_fixed)
	{
		if (elm_type != Markup::HTE_DOC_ROOT && IsInlineDisplayType(display_type))
			SetWasInline(TRUE);

		display_type = ToBlockDisplayType(display_type);
		float_type = CSS_VALUE_none;

		if (!GetMarginTopSpecified())
			margin_top = LayoutCoord(0);
		if (!GetMarginBottomSpecified())
			margin_bottom = LayoutCoord(0);
	}

	if (use_era_setting && is_ssr_or_cssr)
	{
		int ifl = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::Float));

		if (ifl == 0 || ifl == 1 && !(elm_ns == NS_HTML &&
									  (elm_type == Markup::HTE_IMG ||
									   elm_type == Markup::HTE_LI ||
									   elm_type == Markup::HTE_DD ||
									   elm_type == Markup::HTE_DT)))
			float_type = CSS_VALUE_none;
	}

	BOOL parent_has_content = (parent_props.content_cp != NULL);
	if (!parent_has_content && parent_cascade->use_first_line_props)
	{
		/* Normally we set the display type to 'none' for children of
		   elements with the content property set.  The children
		   generate no boxes and no boxes are generated under elements
		   without boxes (except for in some broken html cases).

		   For :first-line, we check one extra level up the cascade,
		   represented by GetProps(), to see if that one has the
		   content property, since there is no element on the way to
		   _not_ get a box. */
		parent_has_content = (parent_cascade->GetProps()->content_cp != NULL);
	}

	if (parent_has_content && !element->GetIsGeneratedContent() && !element->GetIsPseudoElement() && element->GetInserted() != HE_INSERTED_BY_LAYOUT)
		display_type = CSS_VALUE_none;
	else
		if (display_type == CSS_VALUE_table_caption &&
			parent_props.display_type != CSS_VALUE_table &&
			parent_props.display_type != CSS_VALUE_table_row_group &&
			parent_props.display_type != CSS_VALUE_inline_table &&
			parent_props.display_type != CSS_VALUE_table_row)
			display_type = CSS_VALUE_block;

	if (display_type != CSS_VALUE_none)
		if (!parent)
			display_type = CSS_VALUE_block;
		else
			if (float_type != CSS_VALUE_none || flexbox && !IsInternalTableDisplayType(display_type))
				display_type = ToBlockDisplayType(display_type);

	if (!hld_profile->IsInStrictMode() && display_type == CSS_VALUE_table_row)
	{
		BOOL has_background = (bg_color == USE_DEFAULT_COLOR || bg_images.IsEmpty());

		if (has_background && element->GetLayoutBox())
		{
			OP_ASSERT(element->GetLayoutBox()->IsTableRow() || element->GetLayoutBox()->IsContentReplaced() || element->IsExtraDirty());

			if (parent_cascade->html_element->GetLayoutBox() && parent_cascade->html_element->GetLayoutBox()->IsTableRowGroup())
			{
				if (bg_color == USE_DEFAULT_COLOR)
					bg_color = parent_props.bg_color;

				if (bg_images.IsEmpty())
					bg_images.SetImages(parent_props.bg_images.GetImages());
			}
		}
	}

	if (display_type == CSS_VALUE__wap_marquee)
		overflow_x = overflow_y = CSS_VALUE_hidden;

	if (!border_image.HasBorderImage())
	{
		/* Border style 'none' or 'hidden' means no border. Color and
		   width are ignored (i.e., the border has width 0, unless the
		   border is an image). */

		if (border.top.style == CSS_VALUE_none || border.top.style == CSS_VALUE_hidden)
			border.top.width = 0;
		if (border.left.style == CSS_VALUE_none || border.left.style == CSS_VALUE_hidden)
			border.left.width = 0;
		if (border.right.style == CSS_VALUE_none || border.right.style == CSS_VALUE_hidden)
			border.right.width = 0;
		if (border.bottom.style == CSS_VALUE_none || border.bottom.style == CSS_VALUE_hidden)
			border.bottom.width = 0;
	}

	if (border.top.color == USE_DEFAULT_COLOR)
		border.top.color = font_color;
	if (border.left.color == USE_DEFAULT_COLOR)
		border.left.color = font_color;
	if (border.right.color == USE_DEFAULT_COLOR)
		border.right.color = font_color;
	if (border.bottom.color == USE_DEFAULT_COLOR)
		border.bottom.color = font_color;

	if (column_rule.style == CSS_VALUE_none || column_rule.style == CSS_VALUE_hidden)
		column_rule.width = 0;
	if (column_rule.color == USE_DEFAULT_COLOR)
		column_rule.color = font_color;

	if (flexbox_modes.GetAlignSelf() == FlexboxModes::ASELF_AUTO)
		// 'align-self:auto' computes to parent's computed value of 'align-items'.

		flexbox_modes.SetAlignSelf(FlexboxModes::ToAlignSelf(parent_props.flexbox_modes.GetAlignItems()));

	/* Auto min-width / min-height computes to 0 unless it's specified on a
	   flex item's main axis. */

	if (flexbox)
	{
		if (flexbox->IsVertical())
		{
			if (min_width == CONTENT_SIZE_AUTO)
				min_width = LayoutCoord(0);
		}
		else
			if (min_height == CONTENT_SIZE_AUTO)
				min_height = LayoutCoord(0);
	}
	else
	{
		if (min_width == CONTENT_SIZE_AUTO)
			min_width = LayoutCoord(0);

		if (min_height == CONTENT_SIZE_AUTO)
			min_height = LayoutCoord(0);
	}

	// ---------------------------------------------------------------------------------------------
	// special handling of aligned block elements inside div and center

	if (parent_props.align_block_elements &&
		position != CSS_VALUE_absolute &&
		position != CSS_VALUE_fixed &&
		parent_props.display_type != CSS_VALUE_table &&
		parent_props.display_type != CSS_VALUE_inline_table && // do not inherit block alignment from table
		element->Type() != Markup::HTE_LEGEND) // block alignment should not affect LEGEND
	{
		switch (parent_props.align_block_elements)
		{
		case CSS_VALUE_center:
			SetMarginRightAuto(TRUE);
			// no break
		case CSS_VALUE_right:
			SetMarginLeftAuto(TRUE);
			break;
		}

		if (!align_block_elements)
			align_block_elements = parent_props.align_block_elements;
	}

#ifdef SUPPORT_TEXT_DIRECTION

	if (unicode_bidi == CSS_VALUE_bidi_override && layout_workplace->GetBidiStatus() == LOGICAL_MAYBE_RTL)
		layout_workplace->SetBidiStatus(LOGICAL_HAS_RTL);

	// --------------------------------------------------------------------------------------------
	// set text alignment corresponding to the current direction

	// in svg display:table has no other meaning than that the element is rendered
	if (parent_props.GetBidiAlignSpecified() &&
		(elm_ns == NS_SVG ||
		(display_type != CSS_VALUE_table && (elm_ns == NS_HTML && (elm_type != Markup::HTE_SELECT && elm_type != Markup::HTE_TEXTAREA && elm_type != Markup::HTE_INPUT)))))
		SetBidiAlignSpecified();

	if (parent_props.GetAlignSpecified() && elm_ns == NS_HTML && (elm_type == Markup::HTE_TR || elm_type == Markup::HTE_TBODY || elm_type == Markup::HTE_THEAD || elm_type == Markup::HTE_TFOOT))
		SetAlignSpecified();

	if (direction == CSS_VALUE_rtl)
	{
		/* this is to tell the document that is might have bidi codes.
		   Some test pages test bidi without any RTL chars. We want to
		   pass these tests aswell... */

		if (layout_workplace && layout_workplace->GetBidiStatus() == LOGICAL_MAYBE_RTL)
			layout_workplace->SetBidiStatus(LOGICAL_HAS_RTL);

		if (!GetBidiAlignSpecified())
			text_align = CSS_VALUE_right;

		/* Swap right and left values of margin and padding if they come from
		   the default style sheet. A default HTML stylesheet isn't expressible
		   in CSS 2.1 for a BIDI capable browser. Think of margin-left,
		   margin-right, padding-left and padding-right in our default HTML
		   stylesheet as XSL space-start, space-end, padding-start and
		   padding-end. */

		if (!GetMarginLeftSpecified() && !GetMarginRightSpecified())
		{
			LayoutCoord tmp = margin_left;
			margin_left = margin_right;
			margin_right = tmp;
		}

		if (!GetPaddingLeftSpecified() && !GetPaddingRightSpecified())
		{
			LayoutCoord tmp = padding_left;
			padding_left = padding_right;
			padding_right = tmp;
		}

		// FIXME mg should we also swap borders, etc ???
	}
	else
		if (direction == CSS_VALUE_ltr && parent_props.direction == CSS_VALUE_rtl)
		{
			/* This element has specified direction, so alignment
			   should not be inherited from its parent. */

			if (!GetBidiAlignSpecified())
				text_align = CSS_VALUE_left;
		}
#endif // SUPPORT_TEXT_DIRECTION

	if (content_width == CONTENT_WIDTH_O_SKIN || content_height == CONTENT_HEIGHT_O_SKIN)
	{
#ifdef SKIN_SUPPORT
		const uni_char *skin_name = NULL;
		if ((skin_name = bg_images.GetSingleSkinName()))
		{
			INT32 skin_width, skin_height;

			char name8[120]; /* ARRAY OK 2008-02-05 mstensho */
			uni_cstrlcpy(name8, skin_name, ARRAY_SIZE(name8));
			g_skin_manager->GetSize(name8, &skin_width, &skin_height);

			if (content_width == CONTENT_WIDTH_O_SKIN)
				content_width = LayoutCoord(skin_width);
			if (content_height == CONTENT_HEIGHT_O_SKIN)
				content_height = LayoutCoord(skin_height);
		}
		else
#endif
		{
			if (content_width == CONTENT_WIDTH_O_SKIN)
				content_width = CONTENT_WIDTH_AUTO;
			if (content_height == CONTENT_HEIGHT_O_SKIN)
				content_height = CONTENT_HEIGHT_AUTO;
		}
	}

	switch (elm_ns)
	{
#ifdef _WML_CARD_SUPPORT_
	case NS_WML:
		if (hld_profile->HasWmlContent() && (WML_ElementType)elm_type == WE_CARD && !element->GetIsPseudoElement())
		{
			if (vd->GetDocumentManager()->GetCurrentDoc())
			{
				if (hld_profile->WMLGetCurrentCard() == element)
					visibility = CSS_VALUE_visible;
				else
					display_type = CSS_VALUE_none;
			}
		}

		break;
#endif //_WML_CARD_SUPPORT_

	case NS_HTML:
#ifdef HAS_ATVEF_SUPPORT
		if ((elm_type == Markup::HTE_OBJECT || elm_type == Markup::HTE_IMG) && element->GetImageURL(TRUE, hld_profile->GetLogicalDocument()).Type() == URL_TV)
		{
			// check if sizes are set, if not set them to 100%

			if (content_width == CONTENT_WIDTH_AUTO)
				content_width = IntToFixedPointPercentageAsLayoutCoord(100);

			if (content_height == CONTENT_HEIGHT_AUTO)
				content_height = IntToFixedPointPercentageAsLayoutCoord(100);
		}
#endif // HAS_ATVEF_SUPPORT

		if (elm_type == Markup::HTE_VIDEO && computed_overflow_x == CSS_VALUE_visible)
			// Support for overflow: visible requires some refactoring, so we don't support it for now.
			overflow_x = overflow_y = CSS_VALUE_hidden;

		break;
	}

	if (is_replaced_elm)
	{
		/* Replaced content doesn't work with certain table box types, so we need to override
		   the display type. */

		if (!content_cp)
			switch (display_type)
			{
			case CSS_VALUE_inline_table:
				display_type = CSS_VALUE_inline;
				break;

			case CSS_VALUE_table:
			case CSS_VALUE_table_row:
			case CSS_VALUE_table_row_group:
			case CSS_VALUE_table_header_group:
			case CSS_VALUE_table_footer_group:
			case CSS_VALUE_table_column:
			case CSS_VALUE_table_column_group:
			case CSS_VALUE_table_caption:
			case CSS_VALUE_table_cell:
				display_type = CSS_VALUE_block;
				break;
			}
	}

	if (element->GetIsListMarkerPseudoElement())
		SetListMarkerProps(doc);
	else if (use_era_setting)
	{
		BOOL highlight_blocks = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::HighlightBlocks)) == 1;
		BOOL show_background_images = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ShowBackgroundImages)) != 0;

		// expand iframe heights

		if (resolved_element_type == Markup::HTE_IFRAME)
		{
			int scale_height = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FlexibleImageSizes));

			if (scale_height == FLEXIBLE_REPLACED_WIDTH_SCALE || scale_height == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN && element->GetFrameScrolling() == SCROLLING_NO)
			{
				if ((content_height < 0 || content_height >= 4) && (content_width < 0 || content_width >= 4))
				{
					/* Expand the height unless the iframe is very small.
					   They probably don't contain anything visually interesting anyway. */

					if (content_height > 0 && min_height < content_height)
						min_height = content_height;

					content_height = CONTENT_HEIGHT_O_SIZE;
				}
			}
			else
				if (apply_doc_styles != APPLY_DOC_STYLES_YES)
					content_height = CONTENT_HEIGHT_AUTO;
		}
		else
			if (apply_doc_styles != APPLY_DOC_STYLES_YES)
			{
				BOOL is_svg_document = FALSE;

#ifdef SVG_SUPPORT
				if (elm_ns == NS_SVG && elm_type == Markup::HTE_SVG && element->Parent()->Type() == Markup::HTE_DOC_ROOT)
					is_svg_document = TRUE;
#endif // SVG_SUPPORT

				/* Unless an SVG document, ignore height if not replaced
				   content, or if there is no absolute width to retain
				   aspect ratio against. */

				if (!is_svg_document && (!is_replaced_elm || content_height <= 0 || content_width <= 0))
					content_height = CONTENT_HEIGHT_AUTO;
			}

		if (apply_doc_styles != APPLY_DOC_STYLES_YES)
		{
			position = CSS_VALUE_static;

			if (is_form_elm || (elm_type == Markup::HTE_BUTTON && elm_ns == NS_HTML))
			{
				// Pretend that no borders were set (setting the width to zero
				// would remove borders from forms elements)
				border.top.style = BORDER_STYLE_NOT_SET;
				border.left.style = BORDER_STYLE_NOT_SET;
				border.right.style = BORDER_STYLE_NOT_SET;
				border.bottom.style = BORDER_STYLE_NOT_SET;
				padding_top = MIN(padding_top, LayoutCoord(1));
				padding_left = MIN(padding_left, LayoutCoord(3));
				padding_right = MIN(padding_right, LayoutCoord(3));
				padding_bottom = MIN(padding_bottom, LayoutCoord(1));
			}
			else
			{
				border.top.width = 0;
				border.left.width = 0;
				border.right.width = 0;
				border.bottom.width = 0;
				padding_top = LayoutCoord(0);
				padding_left = LayoutCoord(0);
				padding_right = LayoutCoord(0);
				padding_bottom = LayoutCoord(0);
			}

			margin_top = LayoutCoord(0);
			margin_left = LayoutCoord(0);
			margin_right = LayoutCoord(0);
			margin_bottom = LayoutCoord(0);

			if (!(elm_ns == NS_HTML && (elm_type == Markup::HTE_BR || elm_type == Markup::HTE_LI || elm_type == Markup::HTE_DD || elm_type == Markup::HTE_DT)))
				clear = CSS_VALUE_both;

			if (!(elm_type == Markup::HTE_FONT && elm_ns == NS_HTML))
			{
				font_number = parent_props.font_number;
				SetAwaitingWebfont(parent_props.GetAwaitingWebfont());
			}

			if (elm_type == Markup::HTE_DOC_ROOT)
			{
				position = CSS_VALUE_absolute;
				{
					short font_number = styleManager->GetFontNumber(UNI_L("Arial"));
					if (font_number >= 0)
						this->font_number = font_number;
				}
			}

			if (elm_ns == NS_HTML)
			{
				switch (elm_type)
				{
				case Markup::HTE_BODY:
					padding_bottom = padding_right = padding_left = padding_top = LayoutCoord(font_size >> 2);
					font_size_base = font_size;
					SetMarginLeftAuto(TRUE);
					SetMarginRightAuto(TRUE);

#ifdef QUICK
					if (bg_color == USE_DEFAULT_COLOR)
						bg_color = OP_RGB(255, 255, 255);
#endif

#if defined (LAYOUT_USE_SSR_MAX_WIDTH_PREF)
					if (!doc->GetERA_Mode() && is_ssr_or_cssr)
						max_width = era_max_width = LayoutCoord(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SSRMaxWidth));
#endif // LAYOUT_USE_SSR_MAX_WIDTH_PREF


					break;

				case Markup::HTE_H1:
					margin_top = LayoutCoord(6);
					font_weight = 7;
					break;
				case Markup::HTE_H2:
					margin_top = LayoutCoord(5);
					font_weight = 7;
					break;
				case Markup::HTE_H3:
					margin_top = LayoutCoord(4);
					font_weight = 7;
					break;
				case Markup::HTE_H4:
					margin_top = LayoutCoord(3);
					font_weight = 7;
					break;
				case Markup::HTE_H5:
				case Markup::HTE_H6:
					margin_top = LayoutCoord(3);
					break;
				case Markup::HTE_FONT:
					switch (element->GetFontSize())
					{
					case 4:
						margin_top = LayoutCoord(3);
						display_type = CSS_VALUE_block;
						break;
					case 5:
						margin_top = LayoutCoord(4);
						display_type = CSS_VALUE_block;
						break;
					case 6:
					case 7:
						margin_top = LayoutCoord(5);
						display_type = CSS_VALUE_block;
						break;
					}
					break;
				case Markup::HTE_P:
					margin_top = LayoutCoord(font_size / 2);
					margin_bottom = LayoutCoord(1);
					break;
				case Markup::HTE_B:
				case Markup::HTE_STRONG:
				case Markup::HTE_DT:
					font_weight = 7;
					break;
				case Markup::HTE_BLOCKQUOTE:
					margin_top = margin_bottom = LayoutCoord(2);
					++indent_count;

					if (indent_count < 3)
						margin_left = margin_right = LayoutCoord(font_size);
					else if (indent_count < 5)
						margin_left = margin_right = LayoutCoord(font_size / 2);
					/* fall through */
				case Markup::HTE_I:
				case Markup::HTE_EM:
					font_italic = FONT_STYLE_ITALIC;
					break;
				case Markup::HTE_APPLET:
				case Markup::HTE_EMBED:
				case Markup::HTE_OBJECT:
#ifdef CANVAS_SUPPORT
				case Markup::HTE_CANVAS:
#endif // CANVAS_SUPPORT
					if (apply_doc_styles == APPLY_DOC_STYLES_NO)
						display_type = CSS_VALUE_none;
					break;
				case Markup::HTE_INPUT:
					if (element->GetInputType() == INPUT_TEXT || element->HasAttr(Markup::HA_SIZE))
						max_width = LayoutCoord((containing_block_width * 98) / 100);
					if (element->HasAttr(Markup::HA_ALT))
						text_decoration = TEXT_DECORATION_UNDERLINE;
					break;
				case Markup::HTE_TEXTAREA:
					max_width = LayoutCoord((containing_block_width * 98) / 100);
					break;
				case Markup::HTE_DD:
					margin_left = LayoutCoord(font_size);
					break;
				case Markup::HTE_LI:
					switch (list_style_type)
					{
					case CSS_VALUE_disc:
					case CSS_VALUE_circle:
					case CSS_VALUE_square:
					case CSS_VALUE_box:
					case CSS_VALUE_none:
						++indent_count;

						if (indent_count < 3)
							margin_left = LayoutCoord(int(1.3f * font_size));
						else if (indent_count < 5)
							margin_left = LayoutCoord(font_size / 2);

						break;
					default:
						margin_left = LayoutCoord(font_average_width * 4 + 8); // Make room for 4 characters (3 digits/letters and a '.') + the hard-coded marker offset in ListItemBlockBox::Layout()
						break;
					};
					break;

				case Markup::HTE_TABLE:
				case Markup::HTE_TR:
				case Markup::HTE_TD:
				case Markup::HTE_TH:
				case Markup::HTE_DIV:
					if (apply_doc_styles == APPLY_DOC_STYLES_NO)
					{
#ifdef SUPPORT_TEXT_DIRECTION
						if (direction == CSS_VALUE_rtl)
							text_align = CSS_VALUE_right;
						else
#endif
							text_align = CSS_VALUE_left;
					}
					break;
				case Markup::HTE_AREA:
					if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline))
						text_decoration = TEXT_DECORATION_UNDERLINE;
					break;
				case Markup::HTE_A:
					if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline) && element->GetA_HRef(doc))
						text_decoration = TEXT_DECORATION_UNDERLINE;
					break;
				case Markup::HTE_HR:
					border.top.width = 1;
					border.top.color = OP_RGB(0, 0, 0);
					border.top.style = CSS_VALUE_solid;
					margin_top = margin_bottom = LayoutCoord(3);
					break;
				case Markup::HTE_IFRAME:
					if (content_width < 0 && content_width != CONTENT_WIDTH_AUTO)
						content_width = LayoutCoord(-100);
					break;
				case Markup::HTE_IMG:
					padding_top = LayoutCoord(2);
					padding_bottom = LayoutCoord(2);
					break;
				}
			}

			if (float_type != CSS_VALUE_none)
			{
				padding_top = LayoutCoord(3);
				padding_left = LayoutCoord(3);
				padding_right = LayoutCoord(3);
				padding_bottom = LayoutCoord(3);
			}
		}
		else
		{
			if (doc && doc->GetAbsPositionedStrategy() == ABS_POSITION_STATIC && elm_type != Markup::HTE_DOC_ROOT)
				position = CSS_VALUE_static;

			if (content_width >= 0)
			{
				/* If things are non-auto and non-percent, constrain to min-width and
				   max-width now. This helps width propagation (and float scaling). In
				   ERA mode the layout engine cannot trust min-width and max-width
				   properties, because they may depend on the containing block width. So
				   check the real min-width and max-width values while we still have
				   them. They may be tampered with further below in this method. */

				if (max_width >= 0 && content_width > max_width && !GetMaxWidthIsPercent())
					content_width = max_width;

				if (content_width < min_width && !GetMinWidthIsPercent())
					content_width = min_width;
			}
		}

		if (resolved_element_type == Markup::HTE_IMG && elm_ns == NS_HTML && (!element->FirstChild() || element->Type() == Markup::HTE_OBJECT))
		{
			if (layout_mode == LAYOUT_SSR && display_type != CSS_VALUE_none)
			{
				display_type = CSS_VALUE_block;
				SetMarginLeftAuto(TRUE);
				SetMarginRightAuto(TRUE);
				margin_top = margin_bottom = LayoutCoord(2);
			}

			if (element->HasRealSizeDependentCss())
				switch (CheckRealSizeDependentCSS(element, doc))
				{
				case 0:
					break;

				case CSS_VALUE_inline:
					content_cp = g_opera->layout_module.m_img_alt_content;
					display_type = CSS_VALUE_inline;
					padding_right = LayoutCoord(3);
					break;

				case CSS_VALUE_none:
					// Replace image with a whitespace
					content_cp = g_opera->layout_module.m_space_content;
					display_type = CSS_VALUE_inline;
					break;
				}
		}

		if (display_type == CSS_VALUE_table)
		{
			if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ColumnStretch)) == COLUMN_STRETCH_CENTER)
			{
				SetMarginLeftAuto(TRUE);
				SetMarginRightAuto(TRUE);
			}

			if (layout_mode == LAYOUT_SSR || layout_mode == LAYOUT_CSSR)
			{
				if (border_spacing_horizontal > 2)
					border_spacing_horizontal = 2;
				if (border_spacing_vertical > 2)
					border_spacing_vertical = 2;
			}
		}

		if (highlight_blocks && elm_ns == NS_HTML
			&& (elm_type == Markup::HTE_DIV || elm_type == Markup::HTE_H1 || elm_type == Markup::HTE_H2 || elm_type == Markup::HTE_H3
				|| (elm_type == Markup::HTE_FONT && display_type == CSS_VALUE_block)))
		{
#ifndef SHB_NO_SHADING
			HTML_Element* parent_div = NULL;
			if (elm_type == Markup::HTE_DIV)
			{
				parent_div = element->Parent();
				while (parent_div && parent_div->Type() != Markup::HTE_DIV)
					parent_div = parent_div->Parent();
			}

			if (parent_div)
				bg_color = OP_RGB(0xdd, 0xdd, 0xdd);
			else
				bg_color = OP_RGB(0xee, 0xee, 0xee);
#endif

			margin_top = margin_bottom = LayoutCoord(2);
			padding_top = padding_left = padding_right = padding_bottom = LayoutCoord(2);
		}

		if (!show_background_images)
			bg_images.Reset();

		table_layout = CSS_VALUE_auto;

		switch (white_space)
		{
		case CSS_VALUE_pre:
		case CSS_VALUE_nowrap:
		case CSS_VALUE_pre_line:
			if (g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::HonorNowrap)) != 2)
			{
				switch (white_space)
				{
				case CSS_VALUE_pre:
					white_space = CSS_VALUE_pre_wrap;
					break;
				case CSS_VALUE_nowrap:
				case CSS_VALUE_pre_line:
					white_space = CSS_VALUE_normal;
					break;
				}
			}
			break;
		}

		LayoutCoord new_max_width = LAYOUT_COORD_MAX;

		if (layout_workplace && layout_workplace->GetScaleAbsoluteWidthAndPos())
		{
			LayoutCoord view_width = layout_workplace->GetAbsScaleViewWidth();

			if (is_replaced_elm || display_type == CSS_VALUE_block)
			{
				if (content_width > 0)
					new_max_width = (content_width * view_width) / layout_workplace->GetNormalEraWidth();

				if (margin_left > 0)
					margin_left = (margin_left * view_width) / layout_workplace->GetNormalEraWidth();

				if (padding_left > 0)
					padding_left = (padding_left * view_width) / layout_workplace->GetNormalEraWidth();
			}

			if ((position == CSS_VALUE_absolute || position == CSS_VALUE_fixed) && left != HPOSITION_VALUE_AUTO)
				left = (left * view_width) / layout_workplace->GetNormalEraWidth();
		}

		era_max_width = containing_block_width - margin_left;

		/* min-width and max-width MAY depend on the containing block in
		   ERA. Assume (tell the layout engine) that this is always the case,
		   to avoid inconsistent width propagation (and potential infinite
		   reflow loops). */

		SetMinWidthIsPercent();
		SetMaxWidthIsPercent();
		SetHasHorizontalPropsWithPercent();

		if (box_sizing == CSS_VALUE_content_box)
			era_max_width -= padding_left + LayoutCoord(border.left.width);

		if (position != CSS_VALUE_absolute && position != CSS_VALUE_fixed && column_span == 1)
		{
			if (new_max_width == LAYOUT_COORD_MAX)
				new_max_width = era_max_width;

			if (is_ssr_or_cssr &&
				(content_width == CONTENT_WIDTH_AUTO || (elm_type == Markup::HTE_BODY && elm_ns == NS_HTML))) // body should never overflow
			{
				new_max_width -= margin_right;

				if (box_sizing == CSS_VALUE_content_box)
					new_max_width -= padding_right + LayoutCoord(border.right.width);
			}
		}
		else
			if (left != HPOSITION_VALUE_AUTO)
				era_max_width -= left;

		if (new_max_width < 0)
			new_max_width = LayoutCoord(0);

		if (era_max_width < 0)
			era_max_width = LayoutCoord(0);

		if (new_max_width != LAYOUT_COORD_MAX)
		{
			if (min_width > new_max_width)
				min_width = new_max_width;

			if (max_width < 0 || max_width > new_max_width)
				max_width = new_max_width;
		}

		if (is_ssr_or_cssr)
		{
			if (elm_ns == NS_HTML && elm_type != Markup::HTE_LI)
			{
				// translate left/right margin into padding to keep the background color
				if (margin_left > 0)
				{
					padding_left += margin_left;
					margin_left = LayoutCoord(0);
				}
				if (margin_right > 0)
				{
					padding_right += margin_right;
					margin_right = LayoutCoord(0);
				}
			}
			if (display_type == CSS_VALUE_block && !element->IsMatchingType(Markup::HTE_BODY, NS_HTML) && !element->IsMatchingType(Markup::HTE_HTML, NS_HTML) && element->Type() != Markup::HTE_DOC_ROOT)
			{
				// ensure that block background stretch the entire screen
				margin_left -= LayoutCoord(font_size_base >> 2);
				padding_left += LayoutCoord(font_size_base >> 2);
				margin_right -= LayoutCoord(font_size_base >> 2);
				padding_right += LayoutCoord(font_size_base >> 2);
			}
		}
	}

#ifndef NEARBY_ELEMENT_DETECTION
	if (use_era_setting)
#endif
	{
		if (!bg_images.IsEmpty())
		{
			// If current element has an background image, no color contrasting should be done.

			current_bg_color = USE_DEFAULT_COLOR;
			text_bg_color = USE_DEFAULT_COLOR;
		}
		else
			if (bg_color != USE_DEFAULT_COLOR)
			{
				current_bg_color = bg_color;
				text_bg_color = USE_DEFAULT_COLOR;
			}
			else
				if (display_type == CSS_VALUE_table_caption)
				{
					// Table background is never painted behind captions. Do color contrasting against grandparent.

					const HTMLayoutProperties& grandparent_props = *parent_cascade->Pred()->GetProps();

					current_bg_color = grandparent_props.current_bg_color;
					text_bg_color = grandparent_props.text_bg_color;
				}
				else
					if (float_type != CSS_VALUE_none || position != CSS_VALUE_static && elm_type != Markup::HTE_DOC_ROOT)
						/* Floats and positioned elements are not necessarily contained within their
						   parent, and as such it is impossible (well... at least quite hard) to
						   tell what color(s) will be behind them. Therefore, paint text content in
						   such elements with a background color. */

						text_bg_color = current_bg_color;
	}

	// Give hints to limit paragraph width regarding marquee.
	if ((display_type == CSS_VALUE__wap_marquee && (marquee_dir == ATTR_VALUE_right || marquee_dir == ATTR_VALUE_left)) || parent_props.IsInsideHorizontalMarquee())
		SetIsInsideHorizontalMarquee();

	/* Inhibit limit paragraph width on blocks with specified height < 40px.
	   But relative or absolute will reset this state. */

	if (display_type == CSS_VALUE_block && content_height >= 0 && content_height < 40)
		max_paragraph_width = -1;
	else if (position == CSS_VALUE_absolute || position == CSS_VALUE_relative)
		max_paragraph_width = doc->GetMaxParagraphWidth();

	parent_font_ascent = parent_props.font_ascent;
	parent_font_descent = parent_props.font_descent;
	parent_font_internal_leading = parent_props.font_internal_leading;

#ifdef CSS_TRANSITIONS
	if (!ignore_transitions)
		layout_workplace->GetTransitionValues(element, *this);
#endif // CSS_TRANSITIONS

	SetDisplayProperties(vd);

	SetTextMetrics(vd);

	if (display_type == CSS_VALUE_inline_block || display_type == CSS_VALUE_inline_table ||
		float_type != CSS_VALUE_none ||
		position == CSS_VALUE_absolute || position == CSS_VALUE_fixed ||
		elm_ns == NS_CUE && elm_type == Markup::CUEE_ROOT)
	{
		/* Don't propagate text-decoration to floating and absolutely
		   positioned descendants, the contents of 'inline-table' and
		   'inline-block' nor <track> cue roots. */

		overline_color = linethrough_color = underline_color = USE_DEFAULT_COLOR;
		SetHasDecorationAncestors(FALSE);
	}

	if (text_decoration & TEXT_DECORATION_OVERLINE)
		overline_color = font_color;
	if (text_decoration & TEXT_DECORATION_LINETHROUGH)
		linethrough_color = font_color;
	if (text_decoration & TEXT_DECORATION_UNDERLINE)
		underline_color = font_color;

	if (border_collapse == CSS_VALUE_collapse && CSS_is_display_table_value(display_type))
		border.ResetRadius();

	if (elm_type == Markup::HTE_LEGEND && elm_ns == NS_HTML && position == CSS_VALUE_static && float_type == CSS_VALUE_none && parent_cascade->html_element->Type() == Markup::HTE_FIELDSET && parent_cascade->html_element->GetNsType() == NS_HTML)
	{
		HTML_Element *elm = element->Pred();
		while (elm && !elm->GetLayoutBox())
			elm = elm->Pred();

		if (!elm) // no preceding sibling with layout box
		{
			/* Override the vertical margin of a LEGEND element if it is the first child of a
			   FIELDSET. Set a top margin equal to the negative value of parent top padding, so
			   that we move the LEGEND vertically from the content edge and up to the padding edge.
			   Then set a positive bottom margin equal to the parent top padding, so that the
			   specified parent top padding ends up between the LEGEND and the successive
			   elements. */

			margin_top = -parent_props.padding_top;
			margin_bottom = parent_props.padding_top;
			SetMarginTopSpecified();
			SetMarginBottomSpecified();
		}
	}

#ifdef CSS_TRANSFORMS
	if (parent_props.IsInsideTransform() || parent_props.transforms_cp != NULL)
		SetIsInsideTransform(TRUE);
#endif

	if (parent_props.IsInsidePositionFixed() || parent_props.position == CSS_VALUE_fixed)
		SetIsInsidePositionFixed(TRUE);

	/* Horizontal lines with width=1 flicker on TVs. */
	if (avoid_flicker)
	{
		if (margin_top == 1)
			margin_top++;
		if (margin_bottom == 1)
			margin_bottom++;
		if (padding_top == 1)
			padding_top++;
		if (padding_bottom == 1)
			padding_bottom++;
		if (border.top.width == 1)
			border.top.width++;
		if (border.bottom.width == 1)
			border.bottom.width++;
		if (outline.width == 1)
			outline.width++;
		if (border_spacing_vertical == 1)
			border_spacing_vertical++;
		if (content_height == 1)
			content_height++;
	}

	return OpStatus::OK;
}

TableRowGroupBox* GetTableRowGroup(HTML_Element* elm)
{
	for (HTML_Element* parent = Box::GetContainingElement(elm); parent; parent = Box::GetContainingElement(parent))
		if (parent->GetLayoutBox()->IsTableRowGroup())
			return (TableRowGroupBox*) parent->GetLayoutBox();

	return NULL;
}

/* static */ HTML_Element*
HTMLayoutProperties::FindInlineRunInContainingElm(const HTML_Element* elm)
{
	// For inline run-ins we have to walk the sibling tree

	for (HTML_Element* scan = elm->Suc(); scan; scan = scan->Suc())
		if (scan->GetLayoutBox() && scan->GetLayoutBox()->IsBlockBox())
			return scan;

	OP_ASSERT(0);

	return NULL;
}

/* static */ HTML_Element*
HTMLayoutProperties::FindLayoutParent(const HTML_Element* elm)
{
	if (elm->GetLayoutBox() && elm->GetLayoutBox()->IsInlineRunInBox())
		return FindInlineRunInContainingElm(elm);

	return elm->Parent();
}

/* static */ BOOL
HTMLayoutProperties::IsLayoutAncestorOf(const HTML_Element* parent, const HTML_Element* element)
{
    while (element)
        if (element == parent)
            return TRUE;
        else
			element = FindLayoutParent(element);

    return FALSE;
}

BOOL
HTMLayoutProperties::Copy(const HTMLayoutProperties &other)
{
#ifdef SVG_SUPPORT
	delete svg;
#endif

#ifdef CURRENT_STYLE_SUPPORT
	OP_DELETE(types);
#endif

	*this = other;

#ifdef SVG_SUPPORT
	if (other.svg != NULL)
	{
		this->svg = new SvgProperties(*other.svg);
		if (!this->svg)
			return FALSE;
	}
#endif

#ifdef CURRENT_STYLE_SUPPORT
	if (other.types != NULL)
	{
		this->types = OP_NEW(HTMLayoutPropertiesTypes, (*other.types));
		if (!this->types)
			return FALSE;
	}
#endif // CURRENT_STYLE_SUPPORT

	return TRUE;
}

#ifdef CURRENT_STYLE_SUPPORT
HTMLayoutPropertiesTypes*
HTMLayoutProperties::CreatePropTypes()
{
	types = OP_NEW(HTMLayoutPropertiesTypes, ());
	return types;
}
#endif // CURRENT_STYLE_SUPPORT

LayoutCoord
HTMLayoutProperties::CheckWidthBounds(LayoutCoord test_width, BOOL ignore_percent) const
{
	LayoutCoord local_min_width = MAX(min_width, LayoutCoord(0)); // ignore auto min-width here
	LayoutCoord local_max_width = max_width;

	if (ignore_percent)
	{
		if (GetMinWidthIsPercent())
			local_min_width = LayoutCoord(0);

		if (GetMaxWidthIsPercent())
			local_max_width = LayoutCoord(-1);
	}

	if (local_max_width >= 0 && test_width > local_max_width)
		test_width = local_max_width;

	return MAX(test_width, local_min_width);
}

LayoutCoord
HTMLayoutProperties::CheckHeightBounds(LayoutCoord test_height, BOOL ignore_percent) const
{
	LayoutCoord local_min_height = MAX(min_height, LayoutCoord(0)); // ignore auto min-height here.
	LayoutCoord local_max_height = max_height;

	if (ignore_percent)
	{
		if (GetMinHeightIsPercent())
			local_min_height = LayoutCoord(0);

		if (GetMaxHeightIsPercent())
			local_max_height = LayoutCoord(-1);
	}

	if (local_max_height >= 0 && test_height > local_max_height)
		test_height = local_max_height;

	return MAX(test_height, local_min_height);
}

BOOL
HTMLayoutProperties::HasBorderRadius() const
{
	return border.top.HasRadius() || border.left.HasRadius() || border.right.HasRadius() || border.bottom.HasRadius();
}

BOOL
HTMLayoutProperties::HasBorderImage() const
{
	return border_image.HasBorderImage();
}

LayoutCoord
HTMLayoutProperties::DecorationExtent(FramesDocument *doc) const
{
	CSSLengthResolver length_resolver(doc->GetVisualDevice(), FALSE, 0.0f, decimal_font_size_constrained, font_number, doc->RootFontSize());

	LayoutCoord shadow_extent(box_shadows.GetMaxDistance(length_resolver));
	LayoutCoord outline_extent(outline.style != CSS_VALUE_none ? outline.width + outline_offset : 0);
	return MAX(shadow_extent, outline_extent); // Must not return negative values. Shadows will always be positive, so we are safe.
}

BOOL
HTMLayoutProperties::HasListItemMarker() const
{
	return (list_style_type != CSS_VALUE_none || list_style_image
#ifdef CSS_GRADIENT_SUPPORT
		|| list_style_image_gradient
#endif // CSS_GRADIENT_SUPPORT
	);
}

void
HTMLayoutProperties::GetLeftAndTopOffset(LayoutCoord& left_offset, LayoutCoord& top_offset) const
{
	if (position == CSS_VALUE_static)
	{
		/* Normally GetLeftAndTopOffset is only called for positioned
		   boxes, but transformed boxes is one exception. They are
		   handled like positioned boxes in many ways but they
		   shouldn't respect the left and top offsets unless they
		   really are positioned. */

		left_offset = LayoutCoord(0);
		top_offset = LayoutCoord(0);

		return;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	if (direction == CSS_VALUE_rtl)
	{
		if (right != HPOSITION_VALUE_AUTO)
			left_offset = -right;
		else
			if (left != HPOSITION_VALUE_AUTO)
				left_offset = left;
			else
				left_offset = LayoutCoord(0);
	}
	else
#endif // SUPPORT_TEXT_DIRECTION
		if (left != HPOSITION_VALUE_AUTO)
			left_offset = left;
		else
			if (right != HPOSITION_VALUE_AUTO) //ignore right if left is set according to CSS2 errata section 9.4.3
				left_offset = -right;
			else
				left_offset = LayoutCoord(0);

	if (top != VPOSITION_VALUE_AUTO)
		top_offset = top;
	else
		if (bottom != VPOSITION_VALUE_AUTO)
			top_offset = -bottom;
		else
			top_offset = LayoutCoord(0);
}

PixelOrLayoutFixed
CSSLengthResolver::GetLengthInPixels(float val, short vtype) const
{
	PixelOrLayoutFixed length = INT_MAX;

	/* There are two rounding rules in case of the result desired to be in pixels

	   First one concerns px, number, percent, em and rem. In this case we
	   round towards zero, unless the argument is a mathematical integer or it is
	   closer (or equally far) to its ceil then to the first layout fixed point
	   represented number that is smaller than the argument. In such case we round
	   towards +infinity (positive number case). In case of negative argument,
	   the reasoning is flipped (ceil -> floor, smaller -> greater,
	   +infinity -> -infinity).

	   Example for LAYOUT_FIXED_POINT_BASE being 100:
	   1.49 -> 1
	   1.50 -> 1
	   1.51 -> 1
	   1.99 -> 1
	   1.994 -> 1
	   1.995 -> 2
	   1.999 -> 2
	   (1.99 is the greatest number represented by layout fixed point type that is
	   smaller than 2)

	   Second one concerns all other units - this is a standard round to closer
	   integer using OpRound function. */

	switch (vtype)
	{
	case CSS_PX:
		/* Ensured by the LENGTH_PROPS_VAL_MAX < LAYOUT_INT_SAFE_MAX fact.
		   So no one should pass the val that would exceed LAYOUT_FLOAT_SAFE_MAX. */

		OP_ASSERT(op_fabs(val) <= LAYOUT_FLOAT_SAFE_MAX);

		length = result_in_layout_fixed ? PixelOrLayoutFixed(FloatToLayoutFixed(CONSTRAIN_LAYOUT_FIXED_AS_FLOAT(val))) : PixelOrLayoutFixed(int(val));
		break;

	// relative units
	case CSS_PERCENTAGE:
		if (result_in_layout_fixed)
		{
			float temp = val / 100.0f * percentage_base;
			length = FloatToLayoutFixed(CONSTRAIN_LAYOUT_FIXED_AS_FLOAT(temp));
		}
		else
		{
			double temp = double(val) * (LAYOUT_FIXED_POINT_BASE / 100) * percentage_base;
			temp = Round(temp, 0, ROUND_NORMAL) / LAYOUT_FIXED_POINT_BASE;
			length = int(CONSTRAIN_LAYOUT_COORD(temp));
		}
		break;

	case CSS_NUMBER:
	case CSS_EM:
	case CSS_REM:
		{
			double temp = (vtype == CSS_REM ? root_font_size : font_size) * val;
			if (result_in_layout_fixed)
				length = OpRound(CONSTRAIN_LAYOUT_FIXED(temp));
			else
			{
				temp = Round(temp, 0, ROUND_NORMAL) / LAYOUT_FIXED_POINT_BASE;
				length = int(CONSTRAIN_LAYOUT_COORD(temp));
			}
		}
		break;

	case CSS_EX:
		if (result_in_layout_fixed)
		{
			float temp = val * float(GetExUnitLength());
			length = FloatToLayoutFixed(CONSTRAIN_LAYOUT_FIXED_AS_FLOAT(temp));
		}
		else
		{
			double temp = val * GetExUnitLength();
			length = OpRound(CONSTRAIN_LAYOUT_COORD(temp));
		}
		break;

	case CSS_CM:
	case CSS_MM:
	case CSS_IN:
	case CSS_PT:
	case CSS_PC:
	{
		float flength = 0;

		switch (vtype)
		{
		case CSS_IN:
			// inches = 2.54cm
			flength = val * CSS_PX_PER_INCH;
			break;

		case CSS_CM:
			flength = (val * CSS_PX_PER_INCH) / 2.54f;
			break;

		case CSS_MM:
			flength = (val * CSS_PX_PER_INCH) / 25.4f;
			break;

		case CSS_PT:
			// points = 1/72in
			flength = (val * CSS_PX_PER_INCH) / 72;
			break;

		case CSS_PC:
			// picas = 12pt
			flength = (val * CSS_PX_PER_INCH) / 6;
			break;
		}

		length = result_in_layout_fixed ? PixelOrLayoutFixed(FloatToLayoutFixed(CONSTRAIN_LAYOUT_FIXED_AS_FLOAT(flength)))
			: PixelOrLayoutFixed(OpRound(CONSTRAIN_LAYOUT_COORD(flength)));
	}
	break;

	default:
		break;
	}

	return length;
}

int
CSSLengthResolver::GetExUnitLength() const
{
	if (ex_length == -1)
	{
#ifdef OPFONT_GLYPH_PROPS
		if (font_number >= 0)
		{
			/* Even if we use decimal font size, we have to take pixel size.
			   Maybe can be improved? */
			HTMLayoutProperties::SetFontAndSize(vis_dev, font_number, LayoutFixedToIntNonNegative(font_size), FALSE);
			OpFont::GlyphProps glyph_props;
			if (OpStatus::IsSuccess(vis_dev->GetGlyphProps('x', &glyph_props)))
				ex_length = glyph_props.top;
			else
				// Fallback to font_size (em unit base) / 2
				ex_length = LayoutFixedToIntNonNegative(font_size / 2);
		}
		else
#endif // OPFONT_GLYPH_PROPS
			// Fallback to font_size (em unit base) / 2
			ex_length = LayoutFixedToIntNonNegative(font_size / 2);
	}

	OP_ASSERT(ex_length >= 0);

	return ex_length;
}

void
CSSLengthResolver::FillFromVisualDevice()
{
	DocumentManager* doc_man = vis_dev->GetDocumentManager();
	FramesDocument* frm_doc = doc_man ? doc_man->GetCurrentDoc() : NULL;
	font_size = IntToLayoutFixed(vis_dev->GetFontSize());
	font_number = vis_dev->GetCurrentFontNumber();
	root_font_size = frm_doc ? frm_doc->RootFontSize() : LayoutFixed(0);
	if (constrain_root_font_size)
		root_font_size = ConstrainFixedFontSize(root_font_size);
}

#ifdef SKIN_SUPPORT

const uni_char *
BgImages::GetSingleSkinName()
{
	if (bg_images_cp && bg_images_cp->ArrayValueLen() == 1)
	{
		uni_char *bg_str = bg_images_cp->GenArrayValue()[0].GetString();
		if (bg_str[0] == 's' && bg_str[1] == ':')
			return bg_str + 2;
	}
	return NULL;
}

#endif

int
BgImages::GetLayerCount() const
{
	if (bg_images_cp)
		return bg_images_cp->GetLayerCount();
	else
		return 1;
}

void
BgImages::SetBackgroundSize(const CSSLengthResolver& length_resolver, int num, BackgroundProperties &background) const
{
	if (!bg_sizes_cp)
		return;

	int idx = num % bg_sizes_cp->GetLayerCount();
	int arr_len = bg_sizes_cp->ArrayValueLen();
	const CSS_generic_value* arr = bg_sizes_cp->GenArrayValue();
	int size_count = 0;

	for (int i = 0; i < arr_len; i++)
	{
		short type = arr[i].value_type;

		if (type == CSS_COMMA)
		{
			if (idx == 0)
				break;

			idx--;
		}
		else
			if (idx == 0)
			{
				switch (type)
				{
				case CSS_IDENT:

					if (arr[i].GetType() == CSS_VALUE_contain)
						background.packed.bg_size_contain = TRUE;
					else if (arr[i].GetType() == CSS_VALUE_cover)
						background.packed.bg_size_cover = TRUE;

					break;

				case CSS_PERCENTAGE:
				{
					float val = arr[i].GetReal();
					if (size_count == 0)
					{
						background.packed.bg_xsize_per = TRUE;
						background.bg_xsize = LayoutFixedAsLayoutCoord(FloatToLayoutFixed(val));
					}
					else
					{
						background.packed.bg_ysize_per = TRUE;
						background.bg_ysize = LayoutFixedAsLayoutCoord(FloatToLayoutFixed(val));
					}
				}
				break;

				default:
				{
					float val = arr[i].GetReal();
					if (size_count == 0)
						background.bg_xsize = length_resolver.GetLengthInPixels(val, type);
					else
						background.bg_ysize = length_resolver.GetLengthInPixels(val, type);
				}
				}

				size_count++;
			}
	}
}

void
BgImages::SetBackgroundPosition(const CSSLengthResolver& length_resolver, int num, BackgroundProperties &background) const
{
	if (!bg_positions_cp)
		return;

	int idx = num % bg_positions_cp->GetLayerCount();
	int arr_len = bg_positions_cp->ArrayValueLen();
	const CSS_generic_value* arr = bg_positions_cp->GenArrayValue();

	for (int i = 0; i < arr_len; i++)
	{
		short type = arr[i].value_type;

		if (type == CSS_COMMA)
		{
			if (idx == 0)
				break;

			idx--;
		}
		else
			if (idx == 0)
			{
				/* bg-position is stored as either '<offset-x> <offset-y> COMMA' or <ref-point-x>
				   <offset-x> <ref-point-y> <offset-y> COMMA'. */

				OP_ASSERT(i + 2 <= arr_len);

				float pos;
				int pos_type;

				if (arr[i].GetValueType() == CSS_IDENT)
					background.bg_pos_xref = (CSSValue)arr[i++].GetType();

				pos = arr[i].GetReal();
				pos_type = arr[i].GetValueType();

				if (pos_type == CSS_PERCENTAGE)
				{
					background.bg_xpos = LayoutFixedAsLayoutCoord(FloatToLayoutFixed(pos));
					background.packed.bg_xpos_per = TRUE;
				}
				else
					background.bg_xpos = length_resolver.GetLengthInPixels(pos, pos_type);

				i++;

				if (arr[i].GetValueType() == CSS_IDENT)
					background.bg_pos_yref = (CSSValue)arr[i++].GetType();

				pos = arr[i].GetReal();
				pos_type = arr[i].GetValueType();

				if (pos_type == CSS_PERCENTAGE)
				{
					background.bg_ypos = LayoutFixedAsLayoutCoord(FloatToLayoutFixed(pos));
					background.packed.bg_ypos_per = TRUE;
				}
				else
					background.bg_ypos = length_resolver.GetLengthInPixels(pos, pos_type);

				break;
			}
	}
}

void
BgImages::GetLayer(const CSSLengthResolver& length_resolver, int num, BackgroundProperties &background) const
{
	background.Reset();

	/* background-image */

	{
		if (bg_images_cp)
		{
			background.bg_idx = num % bg_images_cp->GetLayerCount();
			const CSS_generic_value& image = bg_images_cp->GenArrayValue()[background.bg_idx];
			if (image.GetValueType() == CSS_FUNCTION_URL ||
				image.GetValueType() == CSS_FUNCTION_SKIN)
			{
				background.bg_img.Set(image.GetString(), bg_images_cp->GetUserDefined());
			}
#ifdef CSS_GRADIENT_SUPPORT
			else if (image.GetValueType() == CSS_FUNCTION_LINEAR_GRADIENT ||
			         image.GetValueType() == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT ||
			         image.GetValueType() == CSS_FUNCTION_O_LINEAR_GRADIENT ||
			         image.GetValueType() == CSS_FUNCTION_REPEATING_LINEAR_GRADIENT ||
			         image.GetValueType() == CSS_FUNCTION_RADIAL_GRADIENT ||
			         image.GetValueType() == CSS_FUNCTION_REPEATING_RADIAL_GRADIENT)
			{
				background.bg_gradient = image.GetGradient();
			}
#endif // CSS_GRADIENT_SUPPORT
		}
	}

	/* background-repeat */

	{
		int bg_repeats_idx = -1;

		if (bg_repeats_cp)
			bg_repeats_idx = num % bg_repeats_cp->GetLayerCount();

		if (bg_repeats_idx >= 0)
		{
			int arr_len = bg_repeats_cp->ArrayValueLen();
			const CSS_generic_value* arr = bg_repeats_cp->GenArrayValue();

			short repeats[2];
			int repeat_count = 0;

			for (int i = 0; i < arr_len && repeat_count < 2; i++)
			{
				short type = arr[i].value_type;

				if (type == CSS_COMMA)
				{
					if (bg_repeats_idx == 0)
						break;

					bg_repeats_idx--;
				}
				else if (bg_repeats_idx == 0 && type == CSS_IDENT)
					repeats[repeat_count++] = arr[i].GetType();
			}

			if (repeat_count == 1)
			{
				switch (repeats[0])
				{
				case CSS_VALUE_repeat_x:
					background.bg_repeat_x = CSS_VALUE_repeat;
					background.bg_repeat_y = CSS_VALUE_no_repeat;
					break;
				case CSS_VALUE_repeat_y:
					background.bg_repeat_x = CSS_VALUE_no_repeat;
					background.bg_repeat_y = CSS_VALUE_repeat;
					break;

				case CSS_VALUE_no_repeat:
				case CSS_VALUE_repeat:
				case CSS_VALUE_space:
				case CSS_VALUE_round:
				case CSS_VALUE__o_extend:
					background.bg_repeat_y = background.bg_repeat_x = repeats[0];
					break;

				default:
					OP_ASSERT(!"Unknown repeat value");
					break;
				}
			}
			else if (repeat_count > 1)
			{
				background.bg_repeat_x = repeats[0];
				background.bg_repeat_y = repeats[1];
			}
		}
	}

	/* background-attachment */

	{
		if (bg_attachs_cp)
		{
			int idx = num % bg_attachs_cp->GetLayerCount();
			background.bg_attach = bg_attachs_cp->GenArrayValue()[idx].GetType();
		}
	}

	/* background-position */

	SetBackgroundPosition(length_resolver, num, background);

	/* background-origin */

	{
		if (bg_origins_cp)
		{
			int idx = num % bg_origins_cp->GetLayerCount();
			background.bg_origin = bg_origins_cp->GenArrayValue()[idx].GetType();
		}
	}

	/* background-clip */

	{
		if (bg_clips_cp)
		{
			int idx = num % bg_clips_cp->GetLayerCount();
			background.bg_clip = bg_clips_cp->GenArrayValue()[idx].GetType();
		}
	}

	/* background-size */

	SetBackgroundSize(length_resolver, num, background);
}

OP_STATUS
BgImages::LoadImages(int media_type, FramesDocument *doc, HTML_Element *element, BOOL load_only_extra /* = FALSE */) const
{
	OP_STATUS load_inline_stat = OpStatus::OK;

	if (bg_images_cp)
	{
		int images_len = bg_images_cp->ArrayValueLen();
		const CSS_generic_value* images_arr = bg_images_cp->GenArrayValue();

		element->ResetExtraBackgroundList();

		int i = 0;

		if (load_only_extra)
			i++;

		/* Each CssURL is resolved during style parsing and should be
		   absolute URLs. Therefore we should ideally use the empty
		   URL as base. Some information, like context id, doesn't
		   survive stringification so we use the document base url for
		   the extra information. There is also the exception for
		   bgimage attributes. Those CssURLs are not resolved to
		   absolute URLs during parsing for performance reasons. */

		URL base_url;

		if (doc->GetHLDocProfile()->BaseURL())
			base_url = *doc->GetHLDocProfile()->BaseURL();
		else
			base_url = doc->GetURL();

		for (; i<images_len; i++)
			if (images_arr[i].GetValueType() == CSS_FUNCTION_URL)
			{
				URL bg_url = g_url_api->GetURL(base_url, images_arr[i].GetString());

				const InlineResourceType inline_type = ((i == 0) ? BGIMAGE_INLINE : EXTRA_BGIMAGE_INLINE);

				if (!bg_url.IsEmpty())
#ifdef _PRINT_SUPPORT_
					if (media_type == CSS_MEDIA_TYPE_PRINT)
					{
						if (g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground))
							load_inline_stat = doc->GetHLDocProfile()->AddPrintImageElement(element, inline_type, &bg_url);
					}
					else
#endif
						load_inline_stat = doc->LoadInline(&bg_url, element, inline_type, LoadInlineOptions().FromUserCSS(bg_images_cp->GetUserDefined()));
			}

		element->CommitExtraBackgroundList();
	}

	return load_inline_stat;
}

void
BgImages::HasPercentagePositions(BOOL &x, BOOL &y) const
{
	x = FALSE; y = FALSE;

	if (!bg_positions_cp)
		return;

	int len = bg_positions_cp->ArrayValueLen();
	const CSS_generic_value* arr = bg_positions_cp->GenArrayValue();

	for (int i=0; i<len; i++)
	{
		if (arr[i].GetValueType() == CSS_PERCENTAGE)
			if (i % 2)
				y = TRUE;
			else
				x = TRUE;
	}
}

BOOL
BgImages::HasPercentagePositions() const
{
	BOOL x = FALSE, y = FALSE;
	HasPercentagePositions(x, y);
	return x || y;
}

CSS_decl *
BgImages::CloneProperty(short property) const
{
	CSS_decl *ret_decl = NULL;

	CSS_generic_value default_position[2];
	default_position[0].SetValueType(CSS_PERCENTAGE);
	default_position[0].SetReal(0);
	default_position[1].SetValueType(CSS_PERCENTAGE);
	default_position[1].SetReal(0);

	switch(property)
	{
	case CSS_PROPERTY_background_image:
		if (bg_images_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_images_cp->GenArrayValue(), bg_images_cp->ArrayValueLen());
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		break;
	case CSS_PROPERTY_background_repeat:
		if (bg_repeats_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_repeats_cp->GenArrayValue(), bg_repeats_cp->ArrayValueLen());
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_repeat));
		break;
	case CSS_PROPERTY_background_attachment:
		if (bg_attachs_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_attachs_cp->GenArrayValue(), bg_attachs_cp->ArrayValueLen());
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_scroll));
		break;
	case CSS_PROPERTY_background_position:
		if (bg_positions_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_positions_cp->GenArrayValue(), bg_positions_cp->ArrayValueLen());
		else
			ret_decl = CSS_copy_gen_array_decl::Create(property, default_position, 2);
		break;
	case CSS_PROPERTY_background_origin:
		if (bg_origins_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_origins_cp->GenArrayValue(), bg_origins_cp->ArrayValueLen());
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_padding_box));
		break;
	case CSS_PROPERTY_background_clip:
		if (bg_clips_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_clips_cp->GenArrayValue(), bg_clips_cp->ArrayValueLen());
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_border_box));
		break;
	case CSS_PROPERTY_background_size:
		if (bg_sizes_cp)
			ret_decl = CSS_copy_gen_array_decl::Create(property, bg_sizes_cp->GenArrayValue(), bg_sizes_cp->ArrayValueLen());
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		break;
	default:
		OP_ASSERT(!"Unknown property");
	}

	return ret_decl;
}

BOOL
BgImages::IsAnyImageLoaded(URL base_url) const
{
	if (bg_images_cp)
	{
		int images_len = bg_images_cp->ArrayValueLen();
		const CSS_generic_value* images_arr = bg_images_cp->GenArrayValue();

		for (int i=0; i<images_len; i++)
			if (images_arr[i].GetValueType() == CSS_FUNCTION_URL)
			{
				URL bg_url = g_url_api->GetURL(base_url, images_arr[i].GetString());

				if (!bg_url.IsEmpty())
				{
					if (bg_url.GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADED)
						return TRUE;

					Image img = UrlImageContentProvider::GetImageFromUrl(bg_url);
					if (!img.IsEmpty() && img.ImageDecoded())
						return TRUE;
				}
			}
			else if (images_arr[i].GetValueType() == CSS_FUNCTION_LINEAR_GRADIENT ||
			         images_arr[i].GetValueType() == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT ||
			         images_arr[i].GetValueType() == CSS_FUNCTION_O_LINEAR_GRADIENT ||
			         images_arr[i].GetValueType() == CSS_FUNCTION_REPEATING_LINEAR_GRADIENT)
				return TRUE;
	}

	return FALSE;
}

#ifdef CSS_TRANSFORMS

void
CSSTransforms::SetTransformFromProps(const HTMLayoutProperties &props, FramesDocument *doc, short box_width, long box_height)
{
	transform.LoadIdentity();

	CSS_decl *css_decl = props.transforms_cp;

	if (!css_decl || css_decl->GetDeclType() != CSS_DECL_TRANSFORM_LIST)
		return;

	const CSS_transform_list::CSS_transform_item *iter = static_cast<const CSS_transform_list *>(css_decl)->First();

	while (iter)
	{
		AffineTransform tmp;

		switch(iter->type)
		{
		case CSS_VALUE_rotate:
			packed.has_non_translate = TRUE;
			tmp.LoadRotate(iter->value[0]);
			break;
		case CSS_VALUE_scaleY:
			packed.has_non_translate = TRUE;
			tmp.LoadScale(1, iter->value[0]);
			break;
		case CSS_VALUE_scaleX:
			packed.has_non_translate = TRUE;
			tmp.LoadScale(iter->value[0], 1);
			break;
		case CSS_VALUE_scale:
			packed.has_non_translate = TRUE;
			if (iter->n_values == 2)
				tmp.LoadScale(iter->value[0], iter->value[1]);
			else
				tmp.LoadScale(iter->value[0], iter->value[0]);
			break;
		case CSS_VALUE_translate:
		case CSS_VALUE_translateX:
		case CSS_VALUE_translateY:
		{
			float tx = 0;
			short txunit = CSS_PX;

			float ty = 0;
			short tyunit = CSS_PX;

			if (iter->type == CSS_VALUE_translateY)
			{
				ty = iter->value[0];
				tyunit = iter->value_type[0];
			}
			else
			{
				tx = iter->value[0];
				txunit = iter->value_type[0];

				if (iter->type == CSS_VALUE_translate && iter->n_values > 1)
				{
					ty = iter->value[1];
					tyunit = iter->value_type[1];
				}
			}

			CSSLengthResolver length_resolver(doc->GetVisualDevice(), TRUE, float(box_width), props.decimal_font_size_constrained, props.font_number, doc->RootFontSize());

			LayoutFixed resolved_tx = length_resolver.GetLengthInPixels(tx, txunit);
			LayoutFixed resolved_ty = length_resolver.ChangePercentageBase(float(box_height)).GetLengthInPixels(ty, tyunit);

			tmp.LoadTranslate(LayoutFixedToFloat(resolved_tx), LayoutFixedToFloat(resolved_ty));
		}
		break;
		case CSS_VALUE_skew:
		case CSS_VALUE_skewX:
		case CSS_VALUE_skewY:
		{
			packed.has_non_translate = TRUE;
			float sx = 0.0, sy = 0.0;

			if (iter->type == CSS_VALUE_skewY)
				sy = iter->value[0];
			else
			{
				sx = iter->value[0];

				if (iter->type == CSS_VALUE_skew && iter->n_values  > 1)
					sy = iter->value[1];
			}

			tmp.LoadSkew(sx, sy);
		}
		break;
		case CSS_VALUE_matrix:
			packed.has_non_translate = TRUE;
			tmp.LoadValuesCM(iter->value[0], iter->value[1],
							 iter->value[2], iter->value[3],
							 iter->value[4], iter->value[5]);
			break;
		default:
			OP_ASSERT(!"Not reached");
		}

		transform.PostMultiply(tmp);
		iter = iter->Suc();
	}
}

BOOL
CSSTransforms::ComputeTransform(FramesDocument* doc,
								const HTMLayoutProperties &props,
								LayoutCoord box_width,
								LayoutCoord box_height)
{
	AffineTransform old_transform = transform;

	Reset();

	if (props.transforms_cp)
	{
		SetTransformFromProps(props, doc, box_width, box_height);

		SetOriginX(props.transform_origin_x, props.GetTransformOriginXIsPercent(), props.GetTransformOriginXIsDecimal());
		SetOriginY(props.transform_origin_y, props.GetTransformOriginYIsPercent(), props.GetTransformOriginYIsDecimal());

		if (packed.has_non_translate)
		{
			AffineTransform t;

			t.LoadIdentity();

			float origin_x = 0, origin_y = 0;
			ComputeTransformOrigin(box_width, box_height, origin_x, origin_y);

			t.LoadTranslate(origin_x, origin_y);
			t.PostMultiply(transform);
			t.PostTranslate(- origin_x, - origin_y);

			transform = t;
		}
	}

	return old_transform != transform;
}

void
CSSTransforms::ComputeTransformOrigin(LayoutCoord box_width, LayoutCoord box_height, float &origin_x, float &origin_y)
{
	origin_x = float(int(transform_origin_x));
	if (packed.transform_origin_x_is_percent)
	{
		if (packed.transform_origin_x_is_decimal)
			origin_x = box_width * origin_x / LayoutFixedAsInt(LAYOUT_FIXED_HUNDRED_PERCENT);
		else
			origin_x = box_width * origin_x / 100;
	}
#ifdef DEBUG
	else
		OP_ASSERT(!packed.transform_origin_x_is_decimal);
#endif // DEBUG

	origin_y = float(int(transform_origin_y));
	if (packed.transform_origin_y_is_percent)
	{
		if (packed.transform_origin_y_is_decimal)
			origin_y = box_height * origin_y / LayoutFixedAsInt(LAYOUT_FIXED_HUNDRED_PERCENT);
		else
			origin_y = box_height * origin_y / 100;
	}
#ifdef DEBUG
	else
		OP_ASSERT(!packed.transform_origin_y_is_decimal);
#endif // DEBUG
}

#endif // CSS_TRANSFORMS

#ifdef CSS_TRANSITIONS
BOOL
HTMLayoutProperties::IsTransitioning(CSSProperty prop) const
{
	switch (prop)
	{
	case CSS_PROPERTY_border_spacing:
		return !!transition_packed.border_spacing;
	case CSS_PROPERTY_color:
		return !!transition_packed.font_color;
	case CSS_PROPERTY_font_size:
		return !!transition_packed.font_size;
	case CSS_PROPERTY_font_weight:
		return !!transition_packed.font_weight;
	case CSS_PROPERTY_letter_spacing:
		return !!transition_packed.letter_spacing;
	case CSS_PROPERTY_line_height:
		return !!transition_packed.line_height;
	case CSS_PROPERTY_selection_background_color:
		return !!transition_packed.selection_bgcolor;
	case CSS_PROPERTY_selection_color:
		return !!transition_packed.selection_color;
	case CSS_PROPERTY_text_indent:
		return !!transition_packed.text_indent;
	case CSS_PROPERTY_text_shadow:
		return !!transition_packed.text_shadow;
	case CSS_PROPERTY_visibility:
		return !!transition_packed.visibility;
	case CSS_PROPERTY_word_spacing:
		return !!transition_packed.word_spacing;
	case CSS_PROPERTY_background_color:
		return !!transition_packed.bg_color;
	case CSS_PROPERTY_background_position:
		return !!transition_packed.bg_pos;
	case CSS_PROPERTY_background_size:
		return !!transition_packed.bg_size;
	case CSS_PROPERTY_border_bottom_color:
		return !!transition_packed.border_bottom_color;
	case CSS_PROPERTY_border_bottom_left_radius:
		return !!transition_packed.border_bottom_left_radius;
	case CSS_PROPERTY_border_bottom_right_radius:
		return !!transition_packed.border_bottom_right_radius;
	case CSS_PROPERTY_border_bottom_width:
		return !!transition_packed.border_bottom_width;
	case CSS_PROPERTY_border_left_color:
		return !!transition_packed.border_left_color;
	case CSS_PROPERTY_border_left_width:
		return !!transition_packed.border_left_width;
	case CSS_PROPERTY_border_right_color:
		return !!transition_packed.border_left_color;
	case CSS_PROPERTY_border_right_width:
		return !!transition_packed.border_left_width;
	case CSS_PROPERTY_border_top_color:
		return !!transition_packed.border_top_color;
	case CSS_PROPERTY_border_top_left_radius:
		return !!transition_packed.border_top_left_radius;
	case CSS_PROPERTY_border_top_right_radius:
		return !!transition_packed.border_top_right_radius;
	case CSS_PROPERTY_border_top_width:
		return !!transition_packed.border_top_width;
	case CSS_PROPERTY_bottom:
		return !!transition_packed.bottom;
	case CSS_PROPERTY_box_shadow:
		return !!transition_packed.box_shadow;
	case CSS_PROPERTY_column_count:
		return !!transition_packed.column_count;
	case CSS_PROPERTY_column_gap:
		return !!transition_packed.column_gap;

	case CSS_PROPERTY_column_rule_color:
		return !!transition_packed2.column_rule_color;
	case CSS_PROPERTY_column_rule_width:
		return !!transition_packed2.column_rule_width;
	case CSS_PROPERTY_column_width:
		return !!transition_packed2.column_width;
	case CSS_PROPERTY_height:
		return !!transition_packed2.height;
	case CSS_PROPERTY_left:
		return !!transition_packed2.left;
	case CSS_PROPERTY_margin_bottom:
		return !!transition_packed2.margin_bottom;
	case CSS_PROPERTY_margin_left:
		return !!transition_packed2.margin_left;
	case CSS_PROPERTY_margin_right:
		return !!transition_packed2.margin_right;
	case CSS_PROPERTY_margin_top:
		return !!transition_packed2.margin_top;
	case CSS_PROPERTY_max_height:
		return !!transition_packed2.max_height;
	case CSS_PROPERTY_max_width:
		return !!transition_packed2.max_width;
	case CSS_PROPERTY_min_height:
		return !!transition_packed2.min_height;
	case CSS_PROPERTY_min_width:
		return !!transition_packed2.min_width;
	case CSS_PROPERTY__o_object_position:
		return !!transition_packed2.object_position;
	case CSS_PROPERTY_opacity:
		return !!transition_packed2.opacity;
	case CSS_PROPERTY_outline_color:
		return !!transition_packed2.outline_color;
	case CSS_PROPERTY_outline_offset:
		return !!transition_packed2.outline_offset;
	case CSS_PROPERTY_outline_width:
		return !!transition_packed2.outline_width;
	case CSS_PROPERTY_padding_bottom:
		return !!transition_packed2.padding_bottom;
	case CSS_PROPERTY_padding_left:
		return !!transition_packed2.padding_left;
	case CSS_PROPERTY_padding_right:
		return !!transition_packed2.padding_right;
	case CSS_PROPERTY_padding_top:
		return !!transition_packed2.padding_top;
	case CSS_PROPERTY_right:
		return !!transition_packed2.right;
	case CSS_PROPERTY_top:
		return !!transition_packed2.top;
	case CSS_PROPERTY_transform:
		return !!transition_packed2.transform;
	case CSS_PROPERTY_transform_origin:
		return !!transition_packed2.transform_origin;
	case CSS_PROPERTY_vertical_align:
		return !!transition_packed2.valign;
	case CSS_PROPERTY_width:
		return !!transition_packed2.width;
	case CSS_PROPERTY_z_index:
		return !!transition_packed2.z_index;
	case CSS_PROPERTY_order:
		return !!transition_packed3.order;
	case CSS_PROPERTY_flex_grow:
		return !!transition_packed3.flex_grow;
	case CSS_PROPERTY_flex_shrink:
		return !!transition_packed3.flex_shrink;
	case CSS_PROPERTY_flex_basis:
		return !!transition_packed3.flex_basis;

	default:
		return FALSE;
	}
}
#endif // CSS_TRANSITIONS

