/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/css_media.h"
#include "modules/style/css_value_types.h"
#include "modules/style/src/css_parser.h"

#include "modules/display/vis_dev.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/util/gen_math.h"
#include "modules/logdoc/logdoc_util.h" // for WhiteSpaceOnly()

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#include "modules/layout/layout_workplace.h"
#include "modules/logdoc/logdoc.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"

const int MEDIA_FEATURE_NAME_SIZE = 39;
const short MEDIA_feature_idx[] =
{
	0,	// start idx size 0
	0,	// start idx size 1
	0,	// start idx size 2
	0,	// start idx size 3
	0,	// start idx size 4
	2,	// start idx size 5
	4,	// start idx size 6
	5,	// start idx size 7
	5,	// start idx size 8
	6,	// start idx size 9
	11,	// start idx size 10
	15,	// start idx size 11
	17,	// start idx size 12
	19,	// start idx size 13
	20,	// start idx size 14
	25,	// start idx size 15
	27,	// start idx size 16
	31,	// start idx size 17
	33,	// start idx size 18
	33,	// start idx size 19
	34,	// start idx size 20
	34,	// start idx size 21
	35,	// start idx size 22
	35,	// start idx size 23
	37,	// start idx size 24
	37,	// start idx size 25
	39	// start idx size 26
};

CONST_ARRAY(g_media_feature_name, char*)
	CONST_ENTRY("GRID"),
	CONST_ENTRY("SCAN"),
	CONST_ENTRY("COLOR"),
	CONST_ENTRY("WIDTH"),
	CONST_ENTRY("HEIGHT"),
	CONST_ENTRY("-O-PAGED"),
	CONST_ENTRY("MAX-COLOR"),
	CONST_ENTRY("MAX-WIDTH"),
	CONST_ENTRY("MIN-COLOR"),
	CONST_ENTRY("MIN-WIDTH"),
	CONST_ENTRY("VIEW-MODE"),
	CONST_ENTRY("MAX-HEIGHT"),
	CONST_ENTRY("MIN-HEIGHT"),
	CONST_ENTRY("MONOCHROME"),
	CONST_ENTRY("RESOLUTION"),
	CONST_ENTRY("COLOR-INDEX"),
	CONST_ENTRY("ORIENTATION"),
	CONST_ENTRY("ASPECT-RATIO"),
	CONST_ENTRY("DEVICE-WIDTH"),
	CONST_ENTRY("DEVICE-HEIGHT"),
	CONST_ENTRY("MAX-MONOCHROME"),
	CONST_ENTRY("MAX-RESOLUTION"),
	CONST_ENTRY("MIN-MONOCHROME"),
	CONST_ENTRY("MIN-RESOLUTION"),
	CONST_ENTRY("-O-WIDGET-MODE"),
	CONST_ENTRY("MAX-COLOR-INDEX"),
	CONST_ENTRY("MIN-COLOR-INDEX"),
	CONST_ENTRY("MAX-ASPECT-RATIO"),
	CONST_ENTRY("MAX-DEVICE-WIDTH"),
	CONST_ENTRY("MIN-ASPECT-RATIO"),
	CONST_ENTRY("MIN-DEVICE-WIDTH"),
	CONST_ENTRY("MAX-DEVICE-HEIGHT"),
	CONST_ENTRY("MIN-DEVICE-HEIGHT"),
	CONST_ENTRY("DEVICE-ASPECT-RATIO"),
	CONST_ENTRY("-O-DEVICE-PIXEL-RATIO"),
	CONST_ENTRY("MAX-DEVICE-ASPECT-RATIO"),
	CONST_ENTRY("MIN-DEVICE-ASPECT-RATIO"),
	CONST_ENTRY("-O-MAX-DEVICE-PIXEL-RATIO"),
	CONST_ENTRY("-O-MIN-DEVICE-PIXEL-RATIO")
CONST_END(g_media_feature_name)

CSS_MediaFeature GetMediaFeature(const uni_char* str, int len)
{
	if (len <= MEDIA_FEATURE_NAME_MAX_SIZE)
	{
		short end_idx = MEDIA_feature_idx[len+1];
		for (short i=MEDIA_feature_idx[len]; i<end_idx; i++)
			if (uni_strni_eq(str, g_media_feature_name[i], len))
				return (CSS_MediaFeature)i;
	}
	return MEDIA_FEATURE_unknown;
}

inline const char* GetMediaFeatureString(CSS_MediaFeature feature)
{
	return g_media_feature_name[feature];
}

CSS_MediaType GetMediaType(const uni_char* text, int text_len, BOOL case_sensitive)
{
	if (case_sensitive)
	{
		if (text_len == 6 && uni_strncmp(text, UNI_L("screen"), 6) == 0)
			return CSS_MEDIA_TYPE_SCREEN;
		else if (text_len == 5 && uni_strncmp(text, UNI_L("print"), 5) == 0)
			return CSS_MEDIA_TYPE_PRINT;
		else if (text_len == 3 && uni_strncmp(text, UNI_L("all"), 3) == 0)
			return CSS_MEDIA_TYPE_ALL;
		else if (text_len == 10 && uni_strncmp(text, UNI_L("projection"), 10) == 0)
			return CSS_MEDIA_TYPE_PROJECTION;
		else if (text_len == 8 && uni_strncmp(text, UNI_L("handheld"), 8) == 0)
			return CSS_MEDIA_TYPE_HANDHELD;
		else if (text_len == 2 && uni_strncmp(text, UNI_L("tv"), 2) == 0)
			return CSS_MEDIA_TYPE_TV;
		else
			return CSS_MEDIA_TYPE_UNKNOWN;
	}
	else
	{
		if (text_len == 6 && uni_strni_eq(text, "SCREEN", 6))
			return CSS_MEDIA_TYPE_SCREEN;
		else if (text_len == 5 && uni_strni_eq(text, "PRINT", 5))
			return CSS_MEDIA_TYPE_PRINT;
		else if (text_len == 3 && uni_strni_eq(text, "ALL", 3))
			return CSS_MEDIA_TYPE_ALL;
		else if (text_len == 10 && uni_strni_eq(text, "PROJECTION", 10))
			return CSS_MEDIA_TYPE_PROJECTION;
		else if (text_len == 8 && uni_strni_eq(text, "HANDHELD", 8))
			return CSS_MEDIA_TYPE_HANDHELD;
		else if (text_len == 2 && uni_strni_eq(text, "TV", 2))
			return CSS_MEDIA_TYPE_TV;
		else
			return CSS_MEDIA_TYPE_UNKNOWN;
	}
}

int CSS_ValueToWindowViewMode(short keyword)
{
	switch (keyword)
	{
	case CSS_VALUE_windowed:
		return WINDOW_VIEW_MODE_WINDOWED;
	case CSS_VALUE_floating:
		return WINDOW_VIEW_MODE_FLOATING ;
	case CSS_VALUE_minimized:
		return WINDOW_VIEW_MODE_MINIMIZED;
	case CSS_VALUE_maximized:
		return WINDOW_VIEW_MODE_MAXIMIZED;
	case CSS_VALUE_fullscreen:
		return WINDOW_VIEW_MODE_FULLSCREEN;
	}
	return WINDOW_VIEW_MODE_UNKNOWN;
}

static LayoutCoord LengthToPixels(float value, short unit)
{
	float divisor;
	switch (unit)
	{
	case CSS_PX:
		return LayoutCoord(OpRound(value));
	case CSS_EM:
	case CSS_EX:
	case CSS_REM:
		{
			FontAtt log_font;
			g_pcfontscolors->GetFont(OP_SYSTEM_FONT_DOCUMENT_NORMAL, log_font);

			return unit == CSS_EX ? LayoutCoord(OpRound(value * log_font.GetExHeight())) : LayoutCoord(OpRound(value * log_font.GetHeight()));
		}
	case CSS_CM:
		divisor = 2.54f;
		break;
	case CSS_MM:
		divisor = 25.4f;
		break;
	case CSS_IN:
		divisor = 1.0f;
		break;
	case CSS_PT:
		divisor = 72.0f;
		break;
	case CSS_PC:
		divisor = 6.0f;
		break;
	default:
		OP_ASSERT(FALSE);
		return LayoutCoord(0);
	}

	return LayoutCoord(OpRound((value * CSS_PX_PER_INCH) / divisor));
}

BOOL CSS_MediaFeatureExpr::Evaluate(FramesDocument* doc) const
{
// Bug #243486. We need to use the width and height of the printer device eventually.
//#ifdef _PRINT_SUPPORT_
//	Window* win = doc ? doc->GetWindow() : NULL;
//	PrinterInfo* prn_info = win ? win->GetPrinterInfo(TRUE) : NULL;
//#endif // _PRINT_SUPPORT_

	VisualDevice* vis_dev = doc->GetVisualDevice();
	OP_ASSERT(vis_dev);

	int device_width = vis_dev->GetScreenWidthCSS();
	int device_height = vis_dev->GetScreenHeightCSS();

	int width = device_width;
	int height = device_height;

	if (LogicalDocument* logdoc = doc->GetLogicalDocument())
		if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
		{
			width = workplace->GetMediaQueryWidth();
			height = workplace->GetMediaQueryHeight();
			device_width = workplace->GetDeviceMediaQueryWidth();
			device_height = workplace->GetDeviceMediaQueryHeight();
		}

	switch (m_packed.feature)
	{
	case MEDIA_FEATURE_device_width:
	case MEDIA_FEATURE_min_device_width:
	case MEDIA_FEATURE_max_device_width:
	case MEDIA_FEATURE_width:
	case MEDIA_FEATURE_min_width:
	case MEDIA_FEATURE_max_width:
	case MEDIA_FEATURE_device_height:
	case MEDIA_FEATURE_min_device_height:
	case MEDIA_FEATURE_max_device_height:
	case MEDIA_FEATURE_height:
	case MEDIA_FEATURE_min_height:
	case MEDIA_FEATURE_max_height:
		{
			int pixel_value = 0;
			if (m_packed.has_value)
			{
				// Convert length value

				OP_ASSERT(CSS_is_length_number_ext(m_packed.token));

				pixel_value = LengthToPixels(m_value.number, m_packed.token);

				OP_ASSERT(pixel_value >= 0);

				switch (m_packed.feature)
				{
				case MEDIA_FEATURE_device_width:
					return (device_width == pixel_value);
				case MEDIA_FEATURE_min_device_width:
					return (device_width >= pixel_value);
				case MEDIA_FEATURE_max_device_width:
					return (device_width <= pixel_value);
				case MEDIA_FEATURE_device_height:
					return (device_height == pixel_value);
				case MEDIA_FEATURE_min_device_height:
					return (device_height >= pixel_value);
				case MEDIA_FEATURE_max_device_height:
					return (device_height <= pixel_value);
				case MEDIA_FEATURE_width:
					return (width == pixel_value);
				case MEDIA_FEATURE_min_width:
					return (width >= pixel_value);
				case MEDIA_FEATURE_max_width:
					return (width <= pixel_value);
				case MEDIA_FEATURE_height:
					return (height == pixel_value);
				case MEDIA_FEATURE_min_height:
					return (height >= pixel_value);
				case MEDIA_FEATURE_max_height:
					return (height <= pixel_value);
				default:
					OP_ASSERT(FALSE);
					return FALSE;
				}
			}
			else
			{
				switch (m_packed.feature)
				{
				case MEDIA_FEATURE_device_width:
					return device_width > 0;
				case MEDIA_FEATURE_width:
					return width > 0;
				case MEDIA_FEATURE_device_height:
					return device_height > 0;
				case MEDIA_FEATURE_height:
					return height > 0;
				default:
					OP_ASSERT(!"Parser accepted no value for feature that requires value.");
					return FALSE;
				}
			}
		}

	case MEDIA_FEATURE_device_aspect_ratio:
		if (m_packed.has_value)
		{
			OP_ASSERT((CSSValueType)m_packed.token == CSS_RATIO);
			return device_width*m_value.ratio.denominator == device_height*m_value.ratio.numerator;
		}
		else
			return TRUE;

	case MEDIA_FEATURE_max_device_aspect_ratio:
		OP_ASSERT(m_packed.has_value && (CSSValueType)m_packed.token == CSS_RATIO);
		return device_height*m_value.ratio.numerator >= device_width*m_value.ratio.denominator;

	case MEDIA_FEATURE_min_device_aspect_ratio:
		OP_ASSERT(m_packed.has_value && (CSSValueType)m_packed.token == CSS_RATIO);
		return device_height*m_value.ratio.numerator <= device_width*m_value.ratio.denominator;

	case MEDIA_FEATURE_aspect_ratio:
		if (m_packed.has_value)
		{
			OP_ASSERT((CSSValueType)m_packed.token == CSS_RATIO);
			return width*m_value.ratio.denominator == height*m_value.ratio.numerator;
		}
		else
			return TRUE;

	case MEDIA_FEATURE_max_aspect_ratio:
		OP_ASSERT(m_packed.has_value && (CSSValueType)m_packed.token == CSS_RATIO);
		return height*m_value.ratio.numerator >= width*m_value.ratio.denominator;

	case MEDIA_FEATURE_min_aspect_ratio:
		OP_ASSERT(m_packed.has_value && (CSSValueType)m_packed.token == CSS_RATIO);
		return height*m_value.ratio.numerator <= width*m_value.ratio.denominator;

	case MEDIA_FEATURE_grid:
		// We don't do text based browsing anymore...
		OP_ASSERT(!m_packed.has_value || m_packed.token == CSS_INT_NUMBER);
		return (m_packed.has_value && m_value.integer == 0);

	case MEDIA_FEATURE_scan:
		// FIXME: TV media, progressive or interlace.
		return FALSE;

	case MEDIA_FEATURE_color:
	case MEDIA_FEATURE_max_color:
	case MEDIA_FEATURE_min_color:
		if (m_packed.has_value)
		{
			OP_ASSERT(m_packed.token == CSS_INT_NUMBER);
			int bpp = vis_dev->GetScreenPixelDepth();
			if (bpp >= 24)
				bpp = 24;
			int bpc = bpp/3;
			return (m_packed.feature == MEDIA_FEATURE_color && bpc == m_value.integer
				|| m_packed.feature == MEDIA_FEATURE_max_color && bpc <= m_value.integer
				|| m_packed.feature == MEDIA_FEATURE_min_color && bpc >= m_value.integer);
		}
		else
		{
			OP_ASSERT(m_packed.feature == MEDIA_FEATURE_color);
			return TRUE;
		}

	case MEDIA_FEATURE_monochrome:
	case MEDIA_FEATURE_max_monochrome:
	case MEDIA_FEATURE_min_monochrome:
		if (m_packed.has_value)
		{
			OP_ASSERT(m_packed.token == CSS_INT_NUMBER);
			return m_value.integer == 0 || m_packed.feature == MEDIA_FEATURE_max_monochrome;
		}
		else
		{
			OP_ASSERT(m_packed.feature == MEDIA_FEATURE_monochrome);
			return FALSE;
		}

	case MEDIA_FEATURE_resolution:
	case MEDIA_FEATURE_max_resolution:
	case MEDIA_FEATURE_min_resolution:
		if (m_packed.has_value)
		{
			INT32 scale_num;
			INT32 scale_denom;
			vis_dev->GetLayoutScale(&scale_num, &scale_denom);
			float res;
			if (m_packed.token == CSS_DPI)
				res = scale_num * 96.0f / scale_denom;
			else if (m_packed.token == CSS_DPCM)
			{
				res = scale_num * 96.0f / 2.54f / scale_denom;
			}
			else
			{
				OP_ASSERT(m_packed.token == CSS_DPPX);
				res = ((float)scale_num) / ((float)scale_denom);
			}

			return m_packed.feature == MEDIA_FEATURE_resolution && res == m_value.number
				|| m_packed.feature == MEDIA_FEATURE_max_resolution && res <= m_value.number
				|| m_packed.feature == MEDIA_FEATURE_min_resolution && res >= m_value.number;
		}
		else
		{
			OP_ASSERT(m_packed.feature == MEDIA_FEATURE_resolution);
			return TRUE;
		}

	case MEDIA_FEATURE_color_index:
	case MEDIA_FEATURE_max_color_index:
	case MEDIA_FEATURE_min_color_index:
		if (m_packed.has_value)
		{
			OP_ASSERT(m_packed.token == CSS_INT_NUMBER);
			return m_value.integer == 0 || m_packed.feature == MEDIA_FEATURE_max_color_index;
		}
		else
		{
			OP_ASSERT(m_packed.feature == MEDIA_FEATURE_color_index);
			return FALSE;
		}

	case MEDIA_FEATURE_orientation:
		if (m_packed.has_value)
		{
			OP_ASSERT(m_packed.token == CSS_IDENT);
			if (m_value.ident == CSS_VALUE_portrait)
				return height >= width;
			else if (m_value.ident == CSS_VALUE_landscape)
				return height < width;
			else
				return FALSE;
		}
		else
			return TRUE;

	case MEDIA_FEATURE__o_min_device_pixel_ratio:
	case MEDIA_FEATURE__o_max_device_pixel_ratio:
	case MEDIA_FEATURE__o_device_pixel_ratio:
		if (m_packed.has_value)
		{
			OP_ASSERT((CSSValueType)m_packed.token == CSS_RATIO);
			unsigned int val1 = m_value.ratio.denominator * doc->GetWindow()->GetTrueZoomBaseScale();
			unsigned int val2 = m_value.ratio.numerator * 100;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			val1 *= doc->GetWindow()->GetPixelScale();
			val2 *= 100;
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			if (val1 == val2)
				return TRUE;
			else
				return m_packed.feature != MEDIA_FEATURE__o_device_pixel_ratio &&
					   (val1 > val2 == (m_packed.feature == MEDIA_FEATURE__o_min_device_pixel_ratio));
		}
		else
		{
			OP_ASSERT(m_packed.feature == MEDIA_FEATURE__o_device_pixel_ratio);
			return TRUE;
		}

	case MEDIA_FEATURE__o_widget_mode:
		{
#ifdef GADGET_SUPPORT
			Window* win = doc->GetWindow();
			if (win->GetType() == WIN_TYPE_GADGET)
			{
				if (m_packed.has_value)
				{
					OP_ASSERT(m_packed.token == CSS_IDENT);
					OpGadget* gadget = win->GetGadget();
					WindowViewMode mode = gadget ? gadget->GetMode() : WINDOW_VIEW_MODE_WIDGET;
					switch (m_value.ident)
					{
					case CSS_VALUE_widget:
						return mode == WINDOW_VIEW_MODE_WIDGET;
					case CSS_VALUE_docked:
						return mode == WINDOW_VIEW_MODE_DOCKED;
					case CSS_VALUE_application:
						return mode == WINDOW_VIEW_MODE_APPLICATION;
					case CSS_VALUE_fullscreen:
						return mode == WINDOW_VIEW_MODE_FULLSCREEN;
					default:
						return FALSE;
					}
				}
				else
					return TRUE;
			}
			else
				return FALSE;
#else // GADGET_SUPPORT
			return FALSE;
#endif // GADGET_SUPPORT
		}

	case MEDIA_FEATURE_view_mode:
		if (m_packed.has_value)
		{
			OP_ASSERT(m_packed.token == CSS_IDENT);
			Window* win = doc->GetWindow();
			return CSS_ValueToWindowViewMode(m_value.ident) == win->GetViewMode();
		}
		else
			return TRUE;
	case MEDIA_FEATURE__o_paged:
#ifdef PAGED_MEDIA_SUPPORT
		return !m_packed.has_value || m_packed.token == CSS_INT_NUMBER && m_value.integer == 1;
#else
		return m_packed.has_value && m_packed.token == CSS_INT_NUMBER && m_value.integer == 0;
#endif // PAGED_MEDIA_SUPPORT
	}

	OP_ASSERT(!"Unhandled media feature.");
	return FALSE;
}

BOOL CSS_MediaFeatureExpr::Equals(const CSS_MediaFeatureExpr& expr) const
{
	return (expr.m_packed.feature == m_packed.feature) &&
			expr.m_packed.has_value == m_packed.has_value &&
			(!m_packed.has_value ||
			 expr.m_packed.token == m_packed.token &&
			 (m_packed.token == CSS_RATIO &&
			  expr.m_value.ratio.numerator == m_value.ratio.numerator &&
			  expr.m_value.ratio.denominator == m_value.ratio.denominator ||
			  expr.m_value.number == m_value.number));
}

void CSS_MediaFeatureExpr::AppendFeatureExprStringL(TempBuffer* buf) const
{
	buf->AppendL("(");
	OpString feature;
	ANCHOR(OpString, feature);
	feature.SetL(GetMediaFeatureString(CSS_MediaFeature(m_packed.feature)));
	feature.MakeLower();
	buf->AppendL(feature.CStr());
	if (m_packed.has_value)
	{
		buf->AppendL(": ");
		if (m_packed.token == CSS_RATIO)
			CSS::FormatCssAspectRatioL(m_value.ratio.numerator, m_value.ratio.denominator, buf);
		else if (m_packed.token == CSS_IDENT)
			buf->AppendL(CSS_GetKeywordString(m_value.ident));
		else
			CSS::FormatCssNumberL(m_value.number, m_packed.token, buf, FALSE);
	}
	buf->AppendL(")");
}

OP_STATUS CSS_MediaFeatureExpr::AddQueryLengths(CSS* stylesheet) const
{
	OP_STATUS stat = OpStatus::OK;
	BOOL horizontal = FALSE;
	BOOL device = FALSE;
	char query_type(0);
	switch (m_packed.feature)
	{
	case MEDIA_FEATURE_max_device_width:
		query_type++;
	case MEDIA_FEATURE_min_device_width:
		query_type++;
	case MEDIA_FEATURE_device_width:
		query_type++;
	case MEDIA_FEATURE_max_width:
		query_type++;
	case MEDIA_FEATURE_min_width:
		query_type++;
	case MEDIA_FEATURE_width:
		query_type++;
		horizontal = TRUE;
	case MEDIA_FEATURE_max_device_height:
		query_type++;
	case MEDIA_FEATURE_min_device_height:
		query_type++;
	case MEDIA_FEATURE_device_height:
		query_type++;
	case MEDIA_FEATURE_max_height:
		query_type++;
	case MEDIA_FEATURE_min_height:
		query_type++;
	case MEDIA_FEATURE_height:
		{
			LayoutCoord pixel_value(0);
			if (m_packed.has_value)
			{
				OP_ASSERT(CSS_is_length_number_ext(m_packed.token));
				pixel_value = LengthToPixels(m_value.number, m_packed.token);
			}
			query_type = (1 << (query_type%3));
			switch (m_packed.feature)
			{
			case MEDIA_FEATURE_max_device_width:
			case MEDIA_FEATURE_min_device_width:
			case MEDIA_FEATURE_device_width:
			case MEDIA_FEATURE_max_device_height:
			case MEDIA_FEATURE_min_device_height:
			case MEDIA_FEATURE_device_height:
				if (horizontal)
					stat = stylesheet->AddDeviceMediaQueryWidth(pixel_value, (CSS::MediaQueryType)query_type);
				else
					stat = stylesheet->AddDeviceMediaQueryHeight(pixel_value, (CSS::MediaQueryType)query_type);
				break;
			default:
				if (horizontal)
					stat = stylesheet->AddMediaQueryWidth(pixel_value, (CSS::MediaQueryType)query_type);
				else
					stat = stylesheet->AddMediaQueryHeight(pixel_value, (CSS::MediaQueryType)query_type);
			}
		}
		break;

	case MEDIA_FEATURE_orientation:
		if (m_packed.has_value)
		{
			OP_ASSERT((CSSValueType)m_packed.token == CSS_IDENT);
			stat = stylesheet->AddMediaQueryRatio(LayoutCoord(1), LayoutCoord(1), CSS::QUERY_MAX);
		}
		break;

	case MEDIA_FEATURE_max_device_aspect_ratio:
		query_type++;
	case MEDIA_FEATURE_min_device_aspect_ratio:
		query_type++;
	case MEDIA_FEATURE_device_aspect_ratio:
		query_type++;
		device = TRUE;
	case MEDIA_FEATURE_max_aspect_ratio:
		query_type++;
	case MEDIA_FEATURE_min_aspect_ratio:
		query_type++;
	case MEDIA_FEATURE_aspect_ratio:
		if (m_packed.has_value)
		{
			OP_ASSERT((CSSValueType)m_packed.token == CSS_RATIO);
			query_type = (1 << (query_type%3));
			if (device)
				stat = stylesheet->AddDeviceMediaQueryRatio(LayoutCoord(m_value.ratio.numerator), LayoutCoord(m_value.ratio.denominator), (CSS::MediaQueryType)query_type);
			else
				stat = stylesheet->AddMediaQueryRatio(LayoutCoord(m_value.ratio.numerator), LayoutCoord(m_value.ratio.denominator), (CSS::MediaQueryType)query_type);
		}
		break;
	}
	return stat;
}

BOOL CSS_MediaQuery::Equals(const CSS_MediaQuery& query) const
{
	if (query.m_packed.type == m_packed.type &&
		query.m_packed.invert == m_packed.invert)
	{
		CSS_MediaFeatureExpr* expr1 = static_cast<CSS_MediaFeatureExpr*>(m_features.First());
		CSS_MediaFeatureExpr* expr2 = static_cast<CSS_MediaFeatureExpr*>(query.m_features.First());
		while (expr1 && expr2)
		{
			if (!expr1->Equals(*expr2))
				return FALSE;

			expr1 = static_cast<CSS_MediaFeatureExpr*>(expr1->Suc());
			expr2 = static_cast<CSS_MediaFeatureExpr*>(expr2->Suc());
		}
		if (expr1 == expr2) // Both NULL.
			return TRUE;
	}
	return FALSE;
}

short CSS_MediaQuery::Evaluate(FramesDocument* doc) const
{
	BOOL failed = m_packed.invert;

	CSS_MediaFeatureExpr* expr = static_cast<CSS_MediaFeatureExpr*>(m_features.First());

	while (expr)
	{
		if (!expr->Evaluate(doc))
		{
			failed = !failed;
			break;
		}
		expr = static_cast<CSS_MediaFeatureExpr*>(expr->Suc());
	}

	short xor_mask = m_packed.invert && m_packed.type != CSS_MEDIA_TYPE_ALL ? CSS_MEDIA_TYPE_MASK : CSS_MEDIA_TYPE_NONE;

	if (!failed)
		return m_packed.type ^ xor_mask;
	else
		return xor_mask & ~m_packed.type & ~CSS_MEDIA_TYPE_ALL;
}

void CSS_MediaQuery::AppendQueryStringL(TempBuffer* buf) const
{
	if (m_packed.invert)
	{
		buf->AppendL("not ");
	}

	const char* media_str = NULL;

	switch (m_packed.type)
	{
	case CSS_MEDIA_TYPE_PRINT:
		media_str = "print";
		break;
	case CSS_MEDIA_TYPE_SCREEN:
		media_str = "screen";
		break;
	case CSS_MEDIA_TYPE_PROJECTION:
		media_str = "projection";
		break;
	case CSS_MEDIA_TYPE_HANDHELD:
		media_str = "handheld";
		break;
	case CSS_MEDIA_TYPE_SPEECH:
		media_str = "speech";
		break;
	case CSS_MEDIA_TYPE_TV:
		media_str = "tv";
		break;
	case CSS_MEDIA_TYPE_ALL:
		media_str = "all";
		break;
	default:
		OP_ASSERT(FALSE);
		// fall through
	case CSS_MEDIA_TYPE_UNKNOWN:
		media_str = "unknown";
		break;
	}

	// Add feature expressions
	CSS_MediaFeatureExpr* expr = static_cast<CSS_MediaFeatureExpr*>(m_features.First());

	BOOL skip_and = expr && m_packed.type == CSS_MEDIA_TYPE_ALL;
	if (!skip_and)
		buf->AppendL(media_str);

	while (expr)
	{
		if (skip_and)
			skip_and = FALSE;
		else
			buf->AppendL(" and ");

		expr->AppendFeatureExprStringL(buf);
		expr = static_cast<CSS_MediaFeatureExpr*>(expr->Suc());
	}
}

OP_STATUS CSS_MediaQuery::AddQueryLengths(CSS* stylesheet) const
{
	CSS_MediaFeatureExpr* expr = static_cast<CSS_MediaFeatureExpr*>(m_features.First());
	while (expr)
	{
		RETURN_IF_ERROR(expr->AddQueryLengths(stylesheet));
		expr = static_cast<CSS_MediaFeatureExpr*>(expr->Suc());
	}
	return OpStatus::OK;
}

BOOL CSS_MediaObject::RemoveMediaQuery(const CSS_MediaQuery& query)
{
	CSS_MediaQuery* tmp = static_cast<CSS_MediaQuery*>(m_queries.First());
	while (tmp)
	{
		if (tmp->Equals(query))
		{
			tmp->Out();
			OP_DELETE(tmp);
			return TRUE;
		}
		tmp = static_cast<CSS_MediaQuery*>(tmp->Suc());
	}
	return FALSE;
}

short CSS_MediaObject::EvaluateMediaTypes(FramesDocument* doc) const
{
	OP_ASSERT(doc);

	short ret_type = CSS_MEDIA_TYPE_NONE;

	CSS_MediaQuery* tmp = static_cast<CSS_MediaQuery*>((void*)m_queries.First());
	if (!tmp)
		return CSS_MEDIA_TYPE_ALL;

	while (tmp)
	{
		ret_type |= tmp->Evaluate(doc);
		tmp = static_cast<CSS_MediaQuery*>(tmp->Suc());
	}
	return ret_type;
}

short CSS_MediaObject::GetMediaTypes() const
{
	short ret_type = CSS_MEDIA_TYPE_NONE;

	CSS_MediaQuery* tmp = static_cast<CSS_MediaQuery*>((void*)m_queries.First());
	while (tmp)
	{
		ret_type |= tmp->GetMediaType();
		tmp = static_cast<CSS_MediaQuery*>(tmp->Suc());
	}
	return ret_type;
}

void CSS_MediaObject::GetMediaStringL(TempBuffer* buf) const
{
	CSS_MediaQuery* tmp = static_cast<CSS_MediaQuery*>((void*)m_queries.First());
	while (tmp)
	{
		tmp->AppendQueryStringL(buf);
		tmp = static_cast<CSS_MediaQuery*>(tmp->Suc());
		if (tmp)
			buf->AppendL(", ");
	}
}

CSS_PARSE_STATUS CSS_MediaObject::SetMediaText(const uni_char* text, int len, BOOL html_attribute)
{
	BOOL delete_only = FALSE;
	CSS_PARSE_STATUS stat = ParseMedia(text, len, CSS_TOK_DOM_MEDIA_LIST, delete_only);
	if (html_attribute && stat == CSSParseStatus::SYNTAX_ERROR)
		return OpStatus::OK;
	else
		return stat;
}

OP_STATUS CSS_MediaObject::GetItemString(TempBuffer* buf, unsigned int index) const
{
	if (index >= GetLength())
	{
		OP_ASSERT(!"Caller should check index against GetLength() first...");
		return OpStatus::ERR;
	}

	Link* OP_MEMORY_VAR l = m_queries.First();
	while (index-- > 0)
		l = l->Suc();

	CSS_MediaQuery* query = static_cast<CSS_MediaQuery*>(l);
	TRAPD(stat, query->AppendQueryStringL(buf));
	return stat;
}

CSS_PARSE_STATUS CSS_MediaObject::AppendMedium(const uni_char* text, int len, BOOL& medium_deleted)
{
	medium_deleted = FALSE;
	return ParseMedia(text, len, CSS_TOK_DOM_MEDIUM, medium_deleted);
}

CSS_PARSE_STATUS CSS_MediaObject::DeleteMedium(const uni_char* text, int len, BOOL& medium_deleted)
{
	medium_deleted = TRUE;
	return ParseMedia(text, len, CSS_TOK_DOM_MEDIUM, medium_deleted);
}

CSS_PARSE_STATUS CSS_MediaObject::ParseMedia(const uni_char* text, int len, int start_token, BOOL& delete_only)
{
	if (WhiteSpaceOnly(text, len))
	{
		if (CSS_TOK_DOM_MEDIA_LIST)
			m_queries.Clear();
		return OpStatus::OK;
	}

	CSS_PARSE_STATUS stat = OpStatus::OK;

	CSS_Buffer* css_buf = OP_NEW(CSS_Buffer, ());

	if (css_buf && css_buf->AllocBufferArrays(1))
	{
		css_buf->AddBuffer(text, len);

		URL base_url;
		CSS_Parser* parser = OP_NEW(CSS_Parser, (NULL, css_buf, base_url, NULL, 1));
		if (parser)
		{
			parser->SetNextToken(start_token);
			TRAP(stat, parser->ParseL());
			if (stat != OpStatus::ERR_NO_MEMORY)
			{
				if (start_token == CSS_TOK_DOM_MEDIA_LIST)
				{
					CSS_MediaObject* new_media = parser->GetMediaObject();
					if (new_media && !new_media->IsEmpty())
					{
						SetFrom(new_media);
						OP_DELETE(new_media);
					}
					else
					{
						m_queries.Clear();
						CSS_MediaQuery* new_query = OP_NEW(CSS_MediaQuery, (TRUE, CSS_MEDIA_TYPE_ALL));
						if (new_query)
							AddMediaQuery(new_query);
						else
							stat = OpStatus::ERR_NO_MEMORY;
					}
				}
				else if (start_token == CSS_TOK_DOM_MEDIUM)
				{
					CSS_MediaQuery* new_query = parser->GetMediaQuery();
					if (new_query)
					{
						BOOL removed = RemoveMediaQuery(*new_query);
						if (delete_only)
						{
							OP_DELETE(new_query);
						}
						else
						{
							AddMediaQuery(new_query);
						}
						delete_only = removed;
					}
				}
			}
		}
		else
			stat = OpStatus::ERR_NO_MEMORY;

		OP_DELETE(parser);
	}
	else
		stat = OpStatus::ERR_NO_MEMORY;

	OP_DELETE(css_buf);
	return stat;
}

void CSS_MediaObject::SetFrom(CSS_MediaObject* copy_from)
{
	m_queries.Clear();
	m_queries.Append(&copy_from->m_queries);
}

void CSS_MediaObject::EndMediaQueryListL()
{
	// If the media object is empty after parsing a media query list, it means that we have only
	// syntax errors in the media query list. Add a "not all" query to make the media list never match.
	if (IsEmpty())
	{
		CSS_MediaQuery* new_query = OP_NEW(CSS_MediaQuery, (TRUE, CSS_MEDIA_TYPE_ALL));
		if (!new_query)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		AddMediaQuery(new_query);
	}
}

OP_STATUS CSS_MediaObject::AddQueryLengths(CSS* stylesheet) const
{
	CSS_MediaQuery* tmp = static_cast<CSS_MediaQuery*>((void*)m_queries.First());
	while (tmp)
	{
		RETURN_IF_ERROR(tmp->AddQueryLengths(stylesheet));
		tmp = static_cast<CSS_MediaQuery*>(tmp->Suc());
	}
	return OpStatus::OK;
}

void CSS_MediaQueryList::Evaluate(FramesDocument* doc)
{
	BOOL matches = (m_media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType() | CSS_MEDIA_TYPE_ALL)) != 0;
	if (matches != m_matches)
	{
		m_matches = matches;
		if (m_listener)
			m_listener->OnChanged(this);
	}
}
