/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/prn_dev.h"
#include "modules/display/coreview/coreview.h"
#include "modules/display/style.h"
#include "modules/debug/debug.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/bidi/characterdata.h"
#include "modules/probetools/probepoints.h"
#include "modules/style/cssmanager.h"
#include "modules/forms/piforms.h"

#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libgogi/pi_impl/mde_opview.h"
# include "modules/libgogi/pi_impl/mde_widget.h"
#endif // VEGA_OPPAINTER_SUPPORT

#include "modules/olddebug/timing.h"

#ifdef SCOPE_EXEC_SUPPORT
# include "modules/scope/scope_display_listener.h"
#endif // SCOPE_EXEC_SUPPORT

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpResizeCorner.h"
#include "modules/widgets/WidgetContainer.h"
#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif

#include "modules/forms/piforms.h"

#if defined(QUICK)
#include "adjunct/quick/Application.h"
#include "adjunct/quick/panels/NotesPanel.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#ifdef FEATURE_SCROLL_MARKER
#include "adjunct/quick/scrollmarker/OpScrollMarker.h"
#endif //FEATURE_SCROLL_MARKER
#endif

#include "modules/pi/OpWindow.h"
#include "modules/pi/OpPluginWindow.h"
#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW	
#include "modules/ns4plugins/src/proxyoppluginwindow.h"
#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW	

#ifdef WAND_SUPPORT
# include "modules/wand/wandmanager.h"
#endif // WAND_SUPPORT
#include "modules/img/image.h"
#include "modules/img/src/imagecontent.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/display/color.h"

#include "modules/display/prn_info.h"
#include "modules/dochand/viewportcontroller.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/style/css.h"
#include "modules/display/styl_man.h"
#include "modules/forms/formmanager.h"

#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"

#ifdef MSWIN
#include "platforms/windows/win_handy.h"
#ifdef SUPPORT_TEXT_DIRECTION
#include "platforms/windows/pi/GdiOpFontManager.h" // GdiOpFontManager::IsLoaded()
#endif
#endif // MSWIN

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif

#ifdef DEBUG_GFX
#include "modules/display/NullPainter.h"
#endif

#include "modules/util/handy.h"
#include "modules/doc/html_doc.h"

#ifdef HAS_ATVEF_SUPPORT
#include "modules/display/tvmanager.h"
#endif

#ifdef _MACINTOSH_
#include "platforms/mac/pi/CocoaOpWindow.h"
#include "platforms/mac/pi/MacOpView.h"
#endif

#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpPrinterController.h"

#include "modules/display/VisDevListeners.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/inputmanager/inputaction.h"

#include "modules/windowcommander/src/WindowCommander.h"

#include "modules/display/fontcache.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/libvega/src/oppainter/vegaopbitmap.h"
#include "modules/libvega/vegapath.h"
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef _SPAT_NAV_SUPPORT_
#include "modules/spatial_navigation/sn_handler.h"
#endif // _SPAT_NAV_SUPPORT_

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h" // Needed in visdev_actions.cpp
#endif // SVG_SUPPORT

#ifndef IMG_ALT_PEN_TYPE
# define IMG_ALT_PEN_TYPE CSS_VALUE_inset
#endif

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleDocument.h"
#endif

extern BOOL ScrollDocument(FramesDocument* doc, OpInputAction::Action action, int times, OpInputAction::ActionMethod method);

#include "modules/display/visdevactions.cpp"

#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
# include "modules/display/textshaper.h"
#endif // USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT

static int GetStringWidth(OpFont *font, const uni_char *str, int len, OpPainter *painter, int char_spacing_extra, VisualDevice* vis_dev)
{
	int width;

#ifdef PLATFORM_FONTSWITCHING
	// No async switching for widgets that can not do a reflow.
	OpAsyncFontListener* listener = vis_dev->GetDocumentManager() && vis_dev->GetDocumentManager()->GetCurrentDoc() ? vis_dev : 0;
#endif

	if (painter)
	    width = font->StringWidth(str, len, painter, char_spacing_extra
#ifdef PLATFORM_FONTSWITCHING
								  , listener
#endif
			);
	else
		width = font->StringWidth(str, len, char_spacing_extra
#ifdef PLATFORM_FONTSWITCHING
								  , listener
#endif
			);

	return width;
}

#ifdef PLATFORM_FONTSWITCHING
void VisualDevice::OnAsyncFontLoaded()
{
	if (GetDocumentManager())
	{
		FramesDocument* doc = GetDocumentManager()->GetCurrentDoc();
		if (doc)
		{
			HTML_Element *root = doc->GetDocRoot();
			if (root)
			{
				root->RemoveCachedTextInfo(doc);
			}
		}
	}
}
#endif

BOOL VisualDevice::SupportsAlpha()
{
	return TRUE;
}

void VisualDevice::DrawString(OpPainter *painter, const OpPoint &point, const uni_char *str, int len, int char_spacing_extra, short word_width)
{
	BeginAccurateFontSize();
	OpStatus::Ignore(CheckFont());

	if (accurate_font_size && word_width != -1)
	{
		word_width = ToPainterExtent(word_width);
		if (word_width <= 0)
		{
			EndAccurateFontSize();
			return;
		}
	}
	else
	word_width = -1;

#ifdef SVG_SUPPORT
	if(currentFont->Type() == OpFontInfo::SVG_WEBFONT)
		OpStatus::Ignore(g_svg_manager->DrawString(this, currentFont, point.x, point.y, str, len, char_spacing_extra));
	else
#endif // SVG_SUPPORT
		painter->DrawString(point, str, len, char_spacing_extra, word_width);

	EndAccurateFontSize();
}

void VisualDevice::DrawStringInternal(OpPainter *painter, const OpPoint &point, const uni_char *str, int len, int char_spacing_extra)
{
#ifdef SVG_SUPPORT
	if(currentFont->Type() == OpFontInfo::SVG_WEBFONT)
		OpStatus::Ignore(g_svg_manager->DrawString(this, currentFont, point.x, point.y, str, len, char_spacing_extra));
	else
#endif // SVG_SUPPORT
		painter->DrawString(point, str, len, char_spacing_extra);
}

void VisualDevice::BeginAccurateFontSize()
{
	if (LayoutIn100Percent() || accurate_font_size)
	{
		accurate_font_size++;
		if (accurate_font_size == 1)
			if (GetScale() != (INT32)GetLayoutScale())
				logfont.SetChanged();
	}
}

void VisualDevice::EndAccurateFontSize()
{
	if (accurate_font_size)
	{
		accurate_font_size--;
		if (accurate_font_size == 0)
			if (GetScale() != (INT32)GetLayoutScale())
				logfont.SetChanged();
	}
}

#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT

//#define CC_NOT_ORDERED			0
//#define CC_OVERLAY				1
//#define CC_NUKTA					7
//#define CC_KANA_VOICING			8
//#define CC_VIRAMA					9
//#define CC_BELOW_LEFT_ATTACHED	200
#define CC_BELOW_ATTACHED			202
//#define CC_ABOVE_ATTACHED			214
//#define CC_ABOVE_RIGHT_ATTACHED	216
#define CC_BELOW_LEFT				218
#define CC_BELOW					220
#define CC_BELOW_RIGHT				222
//#define CC_LEFT					224
//#define CC_RIGHT					226
#define CC_ABOVE_LEFT				228
#define CC_ABOVE					230
#define CC_ABOVE_RIGHT				232
#define CC_DOUBLE_BELOW				233
#define CC_DOUBLE_ABOVE				234
//#define CC_ITOA_SUBSCRIPT			240

//static
BOOL VisualDevice::IsKnownDiacritic(UnicodePoint up)
{
	// Only handle diacritics internally that we know how to handle.
	// There are many groups which we currently don't know how to draw and for
	// those it's best to just give it to the font renderer and hope for a good result.
	if (Unicode::GetCharacterClass(up) == CC_Mn)
	{
#ifdef SUPPORT_UNICODE_NORMALIZE
		unsigned char cc = Unicode::GetCombiningClass(up);
		switch (cc)
		{
		// Known types
		case CC_BELOW_ATTACHED:
		case CC_BELOW_LEFT:
		case CC_BELOW:
		case CC_BELOW_RIGHT:
		case CC_ABOVE_LEFT:
		case CC_ABOVE:
		case CC_ABOVE_RIGHT:
		case CC_DOUBLE_ABOVE:
			return TRUE;

		default:
			// Some code blocks are known anyway, because of special handling in DrawDiacritics.
			if (up >= 0x0591 && up <= 0x05F4) // Hebrew
				return TRUE;
			if (up >= 0x0600 && up <= 0x06FF) // Arabic
				return TRUE;

			break;
		}
#else
		return TRUE;
#endif
	}
	return FALSE;
}

BOOL HasKnownDiacritics(const uni_char* txt, const size_t len)
{
	int txt_len = (int)len;
	while (txt_len > 0)
	{
		int consumed;
		UnicodePoint up = Unicode::GetUnicodePoint(txt, txt_len, consumed);
		if (VisualDevice::IsKnownDiacritic(up))
			return TRUE;
		txt += consumed;
		txt_len -= consumed;
	}
	return FALSE;
}

#ifndef SUPPORT_UNICODE_NORMALIZE
static BOOL IsAbove(uni_char c)
{
	if (c < 0x0300)
		return TRUE;

	if (c <= 0x0315)
		return TRUE;
	if (c <= 0x0319)
		return FALSE;
	if (c <= 0x031b)
		return TRUE;
	if (c <= 0x0333)
		return FALSE;
	if (c <= 0x0338)
		return TRUE;
	if (c <= 0x033c)
		return FALSE;
	if (c <= 0x0344)
		return TRUE;
	if (c <= 0x0349)
		return c == 0x0346;
	if (c <= 0x034c)
		return TRUE;
	if (c <= 0x034f)
		return FALSE;
	if (c <= 0x0352)
		return TRUE;
	if (c <= 0x0356)
		return FALSE;
	if (c <= 0x0358)
		return TRUE;
	if (c <= 0x035c)
		return c == 0x035b;
	if (c <= 0x0361)
		return c != 0x035f;
	if (c == 0x0362)
		return FALSE;
	if (c <= 0x036f)
		return TRUE;

	return TRUE;
}
#endif // !SUPPORT_UNICODE_NORMALIZE

// DrawDiacritics always assumes font is scaled to target zoom, the
// font-sizes-based-on-100%-scale-when-true-zoom-is-enabled hack is done in TxtOut_Diacritics.
void VisualDevice::DrawDiacritics(OpPainter *painter, int x, int y, const uni_char *str, int len,
						   int glyph_width, int glyph_top, int glyph_bottom, int following_glyph_top, int following_glyph_width)
{
	// FIXME: this does not have a fallback for rgba support
	// store text font
	const int text_font = GetCurrentFontNumber();

	// compensate for font ascent per diacritic, since we might have different fonts
	y += ((OpFont*)GetFont())->Ascent();

	// 1px extra above and below
	++glyph_bottom;
	++glyph_top;
	++following_glyph_top;

	// Left, center and right diacritics should adjust their positions individually
	int top[3] = { glyph_top, glyph_top, glyph_top };
	int bottom[3] = { glyph_bottom, glyph_bottom, glyph_bottom };

	int i = 0;
	while (i < len)
	{
		int up_len;
		UnicodePoint up = Unicode::GetUnicodePoint(str + i, len - i, up_len);
#ifdef FONTSWITCHING
		// switch font if necessary
		OpFontInfo* current_font = styleManager->GetFontInfo(GetCurrentFontNumber());
		OpFontInfo* new_font = 0;
		int current_block, lowest_block, highest_block;
		styleManager->GetUnicodeBlockInfo(up, current_block, lowest_block, highest_block);
		if (current_font->HasBlock(current_block)
#ifdef _GLYPHTESTING_SUPPORT_
			&& (!current_font->HasGlyphTable(current_block)
				|| (styleManager->ShouldHaveGlyph(up) && current_font->HasGlyph(up)) )
#endif // _GLYPHTESTING_SUPPORT_
			)
			; // font has the diacritic
		else
			new_font = styleManager->GetRecommendedAlternativeFont(current_font, 0, OpFontInfo::OP_DEFAULT_CODEPAGE, up, FALSE);
		if (new_font)
		{
			SetFont(new_font->GetFontNumber());
			OpStatus::Ignore(CheckFont());
		}
#endif // FONTSWITCHING

		OpFont::GlyphProps dia;
		OpStatus::Ignore(GetGlyphPropsInLocalScreenCoords(up, &dia, TRUE));

		// center diacritic on glyph
		int cx = (glyph_width - dia.width + 1) >> 1;
		// character is drawn at y - character top, need to compensate for this
		int py = dia.top;

		enum H_PLACEMENT {
			H_PLACEMENT_LEFT = 0,
			H_PLACEMENT_CENTER = 1,
			H_PLACEMENT_RIGHT = 2,
		};
		H_PLACEMENT position_index = H_PLACEMENT_CENTER;
		enum PLACEMENT {
			PLACEMENT_ABOVE,	///< Position above glyph
			PLACEMENT_BELOW,	///< Position below glyph
			PLACEMENT_WITHIN,	///< No modification to the position. (Overlap glyph, f.ex Dagesh U+05BC in hebrew)
			PLACEMENT_DOUBLE	///< Position centered horizontally between the current and next glyph. Above other diacritics.
		};
#ifdef SUPPORT_UNICODE_NORMALIZE
		PLACEMENT placement = PLACEMENT_ABOVE;
		unsigned char cc = Unicode::GetCombiningClass(up);

		// Some arabic non-spacing marks are special. They should be positioned even though they have different classes.
		if (up == 0x061A || up == 0x064D || up == 0x0650)
			cc = CC_BELOW;
		else if (up == 0x0618 || up == 0x0619 || (up >= 0x064B && up <= 0x0652) || up == 0x0670)
			cc = CC_ABOVE;

		switch (cc)
		{
		// below
		case CC_BELOW_ATTACHED:
			placement = PLACEMENT_BELOW;
			--bottom[position_index]; // attached, so remove the 1px extra
			break;
		case CC_BELOW:
			placement = PLACEMENT_BELOW;
			break;
		case CC_BELOW_LEFT:
			position_index = H_PLACEMENT_LEFT;
			placement = PLACEMENT_BELOW;
			cx = -dia.width;
			break;
		case CC_BELOW_RIGHT:
			position_index = H_PLACEMENT_RIGHT;
			placement = PLACEMENT_BELOW;
			cx = glyph_width;
			break;
		// above
		case CC_ABOVE_LEFT:
			position_index = H_PLACEMENT_LEFT;
			cx = -dia.width;
			break;
		case CC_ABOVE:
			break;
		case CC_ABOVE_RIGHT:
			position_index = H_PLACEMENT_RIGHT;
			cx = glyph_width;
			break;
		// double
		case CC_DOUBLE_ABOVE:
			placement = PLACEMENT_DOUBLE;
			break;
		// within
		default:
			// If it's not any of the above classes, the mark should fall within the base letter
			// and not affect other marks position.
			placement = PLACEMENT_WITHIN;
			break;
		}
#else
		PLACEMENT placement = IsAbove(up) ? PLACEMENT_ABOVE : PLACEMENT_BELOW;
#endif // SUPPORT_UNICODE_NORMALIZE

		if (placement == PLACEMENT_WITHIN)
		{
			cx = 0;
			py = 0;
		}
		else if (placement == PLACEMENT_DOUBLE)
		{
			// If we have a following character width, center horizontally over both
			if (following_glyph_width)
			{
				cx = (glyph_width + following_glyph_width - dia.width + 1) >> 1;
				// Compensate for any horizontal fixing. This is added again in DrawString
				cx -= dia.left;
			}
			else
				cx = 0;

			// Place it above other diacritics over the highest of the 2 glyphs.
			int max_glyph_top = MAX(glyph_top, following_glyph_top);
			py += -max_glyph_top - dia.height;
			// Very simplified check of other diacritics (that only works with zero or one layer)
			if (len > 1)
				py -= dia.height+1; // 1px extra
		}
		else
		{
			// Compensate for any horizontal fixing. This is added again in DrawString
			cx -= dia.left;

			py += (placement == PLACEMENT_ABOVE) ? -top[position_index] - dia.height : bottom[position_index];

			if (placement == PLACEMENT_ABOVE)
				top[position_index] += dia.height+1; // 1px extra
			else
				bottom[position_index] += dia.height+1; // 1px extra
		}

		// compensate for font ascent per diacritic, since we might have different fonts
		py -= ((OpFont*)GetFont())->Ascent();
		DrawStringInternal(painter, ToPainterNoScale(OpPoint(x+cx,y+py)), str+i, up_len);

		i += up_len;
	}

	// restore text font
	if (GetCurrentFontNumber() != text_font)
	{
		SetFont(text_font);
		OpStatus::Ignore(CheckFont());
	}
}
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT

// == BgRegion related =============================================

void VisualDevice::PaintBg(const OpRect& r, BgInfo *bg, int debug)
{
#ifdef DEBUG_GFX
	switch(bg->type)
	{
		case BgInfo::IMAGE:
			bg_cs.flushed_img_pixels += r.width * r.height;
			OP_ASSERT(bg_cs.flushed_img_pixels <= bg_cs.total_img_pixels);
			break;
		case BgInfo::COLOR:
			bg_cs.flushed_color_pixels += r.width * r.height;
			OP_ASSERT(bg_cs.flushed_color_pixels <= bg_cs.total_color_pixels);
			break;
		// Currently does not handle BgInfo::GRADIENT
	}
#endif
	switch(bg->type)
	{
		case BgInfo::IMAGE:
			{
				OpPoint offset = bg->offset;
				offset.x += r.x - bg->rect.x;
				offset.y += r.y - bg->rect.y;

				const OpPoint bmp_offs = bg->img.GetBitmapOffset();
				const OpRect src(offset.x, offset.y, r.width * 100 / bg->imgscale_x, r.height * 100 / bg->imgscale_y);
				// drawing tiled is unnecessary, and may cause artifacts due to interpolation and wrapping
				if (bmp_offs.x == 0 && bmp_offs.y == 0 &&
				    src.x >= 0 && (unsigned)src.x + src.width  <= bg->img.Width() &&
				    src.y >= 0 && (unsigned)src.y + src.height <= bg->img.Height())
					ImageOut(bg->img, src, r, bg->image_listener, bg->image_rendering, FALSE);
				else
					ImageOutTiled(bg->img, r, offset, bg->image_listener, bg->imgscale_x, bg->imgscale_y, 0, 0, FALSE, bg->image_rendering);
			}
			break;

		case BgInfo::COLOR:
			{
				DrawBgColor(r, bg->bg_color, FALSE);
			}
			break;

#ifdef CSS_GRADIENT_SUPPORT
		case BgInfo::GRADIENT:
			DrawBgGradient(r, bg->img_rect, *bg->gradient, bg->repeat_space, bg->current_color);
			break;
#endif // CSS_GRADIENT_SUPPORT
	}

#ifdef DEBUG_GFX
	extern int display_flushrects;
	if (display_flushrects)
	{
		painter->SetColor(0, 0, 0);
		painter->DrawLine(ToPainter(OpPoint(r.x, r.y)), ToPainter(OpPoint(r.x + r.width - 1, r.y + r.height - 1)));
		painter->DrawLine(ToPainter(OpPoint(r.x + r.width - 1, r.y)), ToPainter(OpPoint(r.x, r.y + r.height - 1)));
		if (debug == 1)
			painter->SetColor(255, 0, 0);
		else
			painter->SetColor(0, 255, 0);
		painter->DrawRect(ToPainter(r));

		painter->SetColor(OP_GET_R_VALUE(this->color), OP_GET_G_VALUE(this->color), OP_GET_B_VALUE(this->color), OP_GET_A_VALUE(this->color));
	}
#endif
}

void VisualDevice::FlushBackgrounds(const OpRect& rect, BOOL only_if_intersecting_display_rect)
{
	if (!bg_cs.num)
		return;

	if (rect.width == INT_MAX || rect.height == INT_MAX)
		bg_cs.FlushAll(this);
	else
	{
		OpRect trect(rect.x + translation_x,
					 rect.y + translation_y,
					 rect.width, rect.height);
		if (!doc_display_rect.Intersecting(trect) && only_if_intersecting_display_rect)
			return;

		bg_cs.FlushBg(this, trect);
	}
}

void VisualDevice::CoverBackground(const OpRect& rect, BOOL pending_to_add_inside, BOOL keep_hole)
{
#ifdef DEBUG_GFX
	extern int avoid_overdraw;
	if (!avoid_overdraw)
		return;
#endif

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return;
#endif // CSS_TRANSFORMS

	OpRect trect(rect.x + translation_x,
				 rect.y + translation_y,
				 rect.width, rect.height);
	if (!doc_display_rect.Intersecting(trect))
		return;

	if (pending_to_add_inside && bg_cs.num == MAX_BG_INFO)
// FIX: Is the last one the best to flush? Maby the first one is better!?!
//      Maby even check which one to flush? Based on area or intersecting with the current coverarea.
		bg_cs.FlushLast(this);

	bg_cs.CoverBg(this, trect, keep_hole);
}

/** Common functionality for all the AddBackgrounds */
void VisualDevice::AddBackground(HTML_Element* element)
{
#ifdef _PRINT_SUPPORT_
	if (doc_manager && doc_manager->GetPrintPreviewVD())
		// No avoid overdraw for printpreview. It does not work for some reason.
		bg_cs.FlushBg(element, this);
#endif
#ifdef CSS_TRANSFORMS
	if (HasTransform())
		// No avoid overdraw for transforms
		bg_cs.FlushBg(element, this);
#endif // CSS_TRANSFORMS
}

void VisualDevice::AddBackground(HTML_Element* element, const COLORREF &bg_color, const OpRect& rect)
{
	if (!doc_display_rect.Intersecting(ToBBox(rect)))
		return;

	// Update this->bg_color
	SetBgColor(bg_color);

	OpRect trect(rect.x + translation_x, rect.y + translation_y,
				 rect.width, rect.height);

	bg_cs.AddBg(element, this, (use_def_bg_color) ? GetWindow()->GetDefaultBackgroundColor() : this->bg_color, trect);
	AddBackground(element);
}

void VisualDevice::AddBackground(HTML_Element* element, Image& img, const OpRect& rect, const OpPoint& offset, ImageListener* image_listener, int imgscale_x, int imgscale_y, CSSValue image_rendering)
{
	// Currently we don't scale imgscale correctly when clipping parts of backgrounds.
	OP_ASSERT(imgscale_x == 100 && imgscale_y == 100);

	if (!IsSolid(img, image_listener))
	{
		// Cover area but keep the hole! We need to flush it immediately.
		CoverBackground(rect, TRUE, TRUE);

		FlushBackgrounds(rect);

		const OpPoint bmp_offs = img.GetBitmapOffset();
		const OpRect src(offset.x, offset.y, rect.width * 100 / imgscale_x, rect.height * 100 / imgscale_y);
		// drawing tiled is unnecessary, and may cause artifacts due to interpolation and wrapping
		if (bmp_offs.x == 0 && bmp_offs.y == 0 &&
		    src.x >= 0 && (unsigned)src.x + src.width  <= img.Width() &&
		    src.y >= 0 && (unsigned)src.y + src.height <= img.Height())
			ImageOut(img, src, rect, image_listener, image_rendering);
		else
			ImageOutTiled(img, rect, offset, image_listener, imgscale_x, imgscale_y, 0, 0, TRUE, image_rendering);
		return;
	}

	// Clip to doc_display_rect

	if (!doc_display_rect.Intersecting(ToBBox(rect)))
		return;

	OpRect trect(rect.x + translation_x,
				 rect.y + translation_y,
				 rect.width, rect.height);

	bg_cs.AddBg(element, this, img, trect, offset, image_listener, imgscale_x, imgscale_y, image_rendering);

	AddBackground(element);
}

#ifdef CSS_GRADIENT_SUPPORT
void VisualDevice::AddBackground(HTML_Element* element, const CSS_Gradient& gradient, OpRect box, const OpRect& gradient_rect, COLORREF current_color, const OpRect& repeat_space, CSSValue repeat_x, CSSValue repeat_y)
{
	if (!doc_display_rect.Intersecting(ToBBox(box)))
		return;

	box.OffsetBy(translation_x, translation_y);

	bg_cs.AddBg(element, this, current_color, gradient, box, gradient_rect, repeat_space, repeat_x, repeat_y);
	AddBackground(element);
}
#endif // CSS_GRADIENT_SUPPORT

void VisualDevice::FlushBackgrounds(HTML_Element* element)
{
	bg_cs.FlushBg(element, this);
}

static BOOL IsBorderSolid(const BorderEdge &be)
{
	UINT32 col32 = HTM_Lex::GetColValByIndex(be.color);
	if (be.color == CSS_COLOR_invert || OP_GET_A_VALUE(col32) < 0xff)
		return FALSE;
#ifdef VEGA_OPPAINTER_SUPPORT
	if (be.HasRadius())
		return FALSE;
#endif
	switch (be.style)
	{
	case CSS_VALUE_solid:
		return TRUE;
	}
	return FALSE;
}

BOOL HasBorderRadius(const Border *border)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!border)
		return FALSE;
	if (border->left.HasRadius() ||
		border->top.HasRadius() ||
		border->right.HasRadius() ||
		border->bottom.HasRadius())
		return TRUE;
#endif
	return FALSE;
}

BOOL HasBorderImage(const BorderImage* border_image)
{
	return border_image && border_image->HasBorderImage();
}

void VisualDevice::PaintAnalysis(const BG_OUT_INFO *info, OpRect &rect, BOOL &rounded, BOOL &must_paint_now)
{
#ifdef CSS_TRANSFORMS
	if (HasTransform())
		must_paint_now = TRUE;
#endif // CSS_TRANSFORMS

	// May need to paint if there is a border and the backgrounds clips to it
	if (info->border)
	{
		/* Always paint if
		   - border-radius
		   - border-image
		   - scaled
		   - border is not solid
		*/
		if (HasBorderRadius(info->border))
		{
			rounded = TRUE;
			must_paint_now = TRUE;
		}
		else if (HasBorderImage(info->border_image))
			must_paint_now = TRUE;
		else if (GetScale() != 100)
			must_paint_now = TRUE;
		if (info->background->bg_clip == CSS_VALUE_border_box)
		{
			if (!must_paint_now && info->has_border_left)
			{
				if (IsBorderSolid(info->border->left))
				{
					rect.x += info->border->left.width;
					rect.width -= info->border->left.width;
				}
				else
					must_paint_now = TRUE;
			}
			if (!must_paint_now && info->has_border_right)
			{
				if (IsBorderSolid(info->border->right))
					rect.width -= info->border->right.width;
				else
					must_paint_now = TRUE;
			}
			if (!must_paint_now && info->has_border_top)
			{
				if (IsBorderSolid(info->border->top))
				{
					rect.y += info->border->top.width;
					rect.height -= info->border->top.width;
				}
				else
					must_paint_now = TRUE;
			}
			if (!must_paint_now && info->has_border_bottom)
			{
				if (IsBorderSolid(info->border->bottom))
					rect.height -= info->border->bottom.width;
				else
					must_paint_now = TRUE;
			}
		}
	}
}

void VisualDevice::BgColorOut(BG_OUT_INFO *info, const OpRect &cover_rect, const OpRect &border_box, const COLORREF &bg_color)
{
	BOOL must_paint_now = FALSE;
	BOOL rounded = FALSE;
	OpRect rect(cover_rect);

	// Cover this area on any intersecting plugin
	CoverPluginArea(rect);

	// Always paint if partly transparent.
	UINT32 bg_col32 = HTM_Lex::GetColValByIndex(bg_color);
	if (OP_GET_A_VALUE(bg_col32) < 0xff)
		must_paint_now = TRUE;

	PaintAnalysis(info, /*inout */ rect, rounded, /* inout */ must_paint_now);

	if (must_paint_now)
	{
		// Cover area but keep the hole! We need to flush it immediately.
		CoverBackground(cover_rect, TRUE, TRUE);
		FlushBackgrounds(rect);

		SetBgColor(bg_color);
#ifdef VEGA_OPPAINTER_SUPPORT
		if (rounded)
		{
			if (cover_rect.Equals(border_box))
				DrawBgColorWithRadius(rect, info->border);
			else
			{
				// Compensate the radius with the inset the cover_rect vs border_box creates, so the radius align as expected.
				short b_ofs_left = cover_rect.x - border_box.x;
				short b_ofs_top = cover_rect.y - border_box.y;
				short b_ofs_right = (border_box.x + border_box.width) - (cover_rect.x + cover_rect.width);
				short b_ofs_bottom = (border_box.y + border_box.height) - (cover_rect.y + cover_rect.height);
				Border border = *info->border;
				RadiusPathCalculator::InsetBorderRadius(&border, b_ofs_left, b_ofs_top, b_ofs_right, b_ofs_bottom);
				DrawBgColorWithRadius(rect, &border);
			}
		}
		else
#endif
			DrawBgColor(rect);
	}
	else
	{
		// Exclude cover_rect from backgroundstack because it will be fully covered.
		CoverBackground(cover_rect, TRUE);

		AddBackground(info->element, bg_color, rect);
	}
}

#ifdef CSS_GRADIENT_SUPPORT
void VisualDevice::BgGradientOut(const BG_OUT_INFO *info, COLORREF current_color, const OpRect &cover_rect, OpRect clip_rect,
                                 OpRect gradient_rect, const CSS_Gradient &gradient, const OpRect &repeat_space)
{
	BOOL rounded = FALSE,
		must_paint_now = FALSE;

	OpRect optimized_rect(cover_rect);
	PaintAnalysis(info, /* inout */ optimized_rect, rounded, /* inout */ must_paint_now);

	// Force painting if there are non-opaque stops.
	must_paint_now |= !gradient.IsOpaque();

	if (must_paint_now)
	{
		// Cover area but keep the hole. We need to flush it immediately.
		CoverBackground(cover_rect, TRUE, TRUE);
		FlushBackgrounds(cover_rect);

		clip_rect.OffsetBy(translation_x, translation_y);
		gradient_rect.OffsetBy(translation_x, translation_y);

		BOOL has_stencil = FALSE;
		if (rounded && OpStatus::IsSuccess(BeginStencil(cover_rect)))
		{
			has_stencil = TRUE;

			SetBgColor(OP_RGB(0, 0, 0));
			BeginModifyingStencil();
			DrawBgColorWithRadius(cover_rect, info->border);
			EndModifyingStencil();
		}

		DrawBgGradient(clip_rect, gradient_rect, gradient, repeat_space, current_color);

		if (has_stencil)
			EndStencil();
	}
	else
	{
		OpRect paint_rect(clip_rect);
		paint_rect.IntersectWith(optimized_rect);

		if (info->background->bg_repeat_x != CSS_VALUE_repeat ||
			 info->background->bg_repeat_y != CSS_VALUE_repeat ||
			 info->background->bg_clip != CSS_VALUE_border_box)
			// There is a chance that the background will not be drawn under any border.
		{
			CoverBackground(cover_rect, TRUE, TRUE);
			FlushBackgrounds(cover_rect);
		}
		else
			// The background will certainly be drawn under any border.
			CoverBackground(cover_rect);

		gradient_rect.OffsetBy(translation_x, translation_y);
		AddBackground(info->element, gradient, paint_rect, gradient_rect, current_color, repeat_space, (CSSValue) info->background->bg_repeat_x, (CSSValue) info->background->bg_repeat_y);
	}
}
#endif // CSS_GRADIENT_SUPPORT

void VisualDevice::BgImgOut(BG_OUT_INFO *info, const OpRect &cover_rect, Image& img, OpRect& dst_rect, OpPoint& offset, ImageListener* image_listener, double imgscale_x, double imgscale_y, int imgspace_x, int imgspace_y)
{
	// FIX: reuse same code as for colors.
	BOOL must_paint_now = FALSE;
#ifdef VEGA_OPPAINTER_SUPPORT
	BOOL rounded = FALSE;
#endif

	if (imgscale_x != 100 || imgscale_y != 100 || imgspace_x != 0 || imgspace_y != 0)
		must_paint_now = TRUE;
	if (HasBorderRadius(info->border))
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		rounded = TRUE;
#endif
		must_paint_now = TRUE;
	}
	else if (HasBorderImage(info->border_image))
		must_paint_now = TRUE;

	if (!must_paint_now && info->has_border_left)
	{
		if (IsBorderSolid(info->border->left))
		{
			int diff = MAX(dst_rect.x, cover_rect.x + info->border->left.width) - dst_rect.x;
			offset.x += diff;
			dst_rect.x += diff;
			dst_rect.width -= diff;
		}
		else
			must_paint_now = TRUE;
	}
	if (!must_paint_now && info->has_border_right)
	{
		if (IsBorderSolid(info->border->right))
		{
			int dstright = dst_rect.x + dst_rect.width;
			int diff = dstright - MIN(dstright, cover_rect.x + cover_rect.width - info->border->right.width);
			dst_rect.width -= diff;
		}
		else
			must_paint_now = TRUE;
	}
	if (!must_paint_now && info->has_border_top)
	{
		if (IsBorderSolid(info->border->top))
		{
			int diff = MAX(dst_rect.y, cover_rect.y + info->border->top.width) - dst_rect.y;
			offset.y += diff;
			dst_rect.y += diff;
			dst_rect.height -= diff;
		}
		else
			must_paint_now = TRUE;
	}
	if (!must_paint_now && info->has_border_bottom)
	{
		if (IsBorderSolid(info->border->bottom))
		{
			int dstbottom = dst_rect.y + dst_rect.height;
			int diff = dstbottom - MIN(dstbottom, cover_rect.y + cover_rect.height - info->border->bottom.width);
			dst_rect.height -= diff;
		}
		else
			must_paint_now = TRUE;
	}

	if (must_paint_now)
	{
		// Cover area but keep the hole! We need to flush it immediately.
		CoverBackground(cover_rect, TRUE, TRUE);
		FlushBackgrounds(cover_rect);

#ifdef VEGA_OPPAINTER_SUPPORT
		BOOL has_stencil = FALSE;
		if (rounded && OpStatus::IsSuccess(BeginStencil(dst_rect)))
		{
			has_stencil = TRUE;

			SetBgColor(OP_RGB(0, 0, 0));
			BeginModifyingStencil();
			DrawBgColorWithRadius(cover_rect, info->border);
			EndModifyingStencil();
		}
#endif
		ImageOutTiled(img, dst_rect, offset, image_listener, imgscale_x, imgscale_y, imgspace_x, imgspace_y, TRUE, info->image_rendering);
#ifdef VEGA_OPPAINTER_SUPPORT
		if (has_stencil)
		{
			EndStencil();
		}
#endif
	}
	else if (info->background->bg_repeat_x != CSS_VALUE_repeat ||
			 info->background->bg_repeat_y != CSS_VALUE_repeat ||
			 info->background->bg_clip != CSS_VALUE_border_box)
	{
		// There is a chance that the background will not be drawn under any border.

		// FIX: This could be improved. decrease optimisation on sdot and opera.com
		// ###################################
		// Cover area but keep the hole! We need to flush it immediately.
		if (info->has_border_left || info->has_border_top || info->has_border_right || info->has_border_bottom)
		{
			CoverBackground(cover_rect, TRUE, TRUE);
			FlushBackgrounds(cover_rect);
		}
		else
		{
			if (IsSolid(img, image_listener))
			{
				// Exclude dst_rect from backgroundstack because it will be fully covered.
				CoverBackground(dst_rect, TRUE);
			}
		}

		// Truncating imgscale_[xy] is ok since this code will never be reached when imgscale != 100
		AddBackground(info->element, img, dst_rect, offset, image_listener, (int) imgscale_x, (int) imgscale_y, (CSSValue) info->image_rendering);
	}
	else
	{
		// The background will certainly be drawn under any border.

		// FIX: Move IsSolid stuff to visualdevice
		if (IsSolid(img, image_listener))
		{
			// Exclude cover_rect from backgroundstack because it will be fully covered.
			CoverBackground(cover_rect, TRUE);
		}
		else if (info->has_border_left || info->has_border_top || info->has_border_right || info->has_border_bottom)
		{
			// Must handle flushing of borders
			BgHandleNoBackground(info, cover_rect);
		}

		// Truncating imgscale_[xy] is ok since this code will never be reached when imgscale != 100
		AddBackground(info->element, img, dst_rect, offset, image_listener, (int) imgscale_x, (int) imgscale_y, (CSSValue) info->image_rendering);
	}
}

void VisualDevice::BgHandleNoBackground(BG_OUT_INFO *info, const OpRect &cover_rect)
{
#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW
	// Cover this area on any intersecting plugin
	CoverPluginArea(cover_rect);
#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW

	// Called if there was no color background, image background or gradient background.
	// If there is borders, we need to make sure what's behind the borders are covered or flushed.
	if (info->has_border_left || info->has_border_top || info->has_border_right || info->has_border_bottom || HasBorderImage(info->border_image))
	{
		OpRect rect(cover_rect);
		if (info->has_border_left + info->has_border_top + info->has_border_right + info->has_border_bottom == 1)
		{
			// It's common with a single border, so optimize for that. (Only flush away that part)
			if (info->has_border_left)
			{
				//if (IsSolid(border->left)
				//FIX: only cover FALSE and no flush!
				rect.Set(cover_rect.x, cover_rect.y, info->border->left.width, cover_rect.height);
			}
			else if (info->has_border_top)
			{
				rect.Set(cover_rect.x, cover_rect.y, cover_rect.width, info->border->top.width);
			}
			else if (info->has_border_right)
			{
				rect.Set(cover_rect.x + cover_rect.width - info->border->right.width, cover_rect.y, info->border->right.width, cover_rect.height);
			}
			else if (info->has_border_bottom)
			{
				rect.Set(cover_rect.x, cover_rect.y + cover_rect.height - info->border->bottom.width, cover_rect.width, info->border->bottom.width);
			}
		}
		// Cover area but keep the hole! We need to flush it immediately.
		CoverBackground(rect, TRUE, TRUE);
		FlushBackgrounds(rect);
	}
}

#ifdef DEBUG_GFX

void VisualDevice::TestSpeed(int test)
{
	FramesDocument *doc = doc_manager ? doc_manager->GetCurrentDoc() : NULL;
	if (doc)
	{
		OpPainter* painter = OP_NEW(NullPainter, ());

		logfont.SetChanged();

		Reset();

		OP_ASSERT(!IsScaled());
		OpRect rect;
		if (test == 0)
			rect.Set(0, 0, doc->Width(), doc->Height());
		else if (test == 1)
			rect.Set(0, 0, VisibleWidth(), VisibleHeight());

		OnPaint(rect, painter);

		OP_DELETE(painter);
	}
}

void VisualDevice::TestSpeedScroll()
{
	SetRenderingViewPos(0, 0, TRUE);

	double tmp = g_op_time_info->GetRuntimeMS();
	while(g_op_time_info->GetRuntimeMS() == tmp) ;
	double t1 = g_op_time_info->GetRuntimeMS();

	int count = 0;
	while (TRUE)
	{
		int old_view_y = v_scroll->GetValue();

		SetRenderingViewPos(rendering_viewport.x, rendering_viewport.y + 1, TRUE);
		if (v_scroll->GetValue() == old_view_y)
			break;
		count++;
	}

	double t2 = g_op_time_info->GetRuntimeMS();

	char buf[250];	/* ARRAY OK 2007-08-28 emil */
	op_sprintf(buf, "time: %d  fps: %d\n", (int)(t2 - t1), (int)(count * 1000 / (t2 - t1)));
	//MessageBoxA(NULL, buf, "scrollspeed", MB_OK);
}

#endif // DEBUG_GFX

// == Scale functions =================================================

int VisualDevice::GetScale(BOOL printer_scale) const
{
#ifdef _PRINT_SUPPORT_
	if (printer_scale && doc_manager && doc_manager->GetWindow()->GetPreviewMode())
		return scale_multiplier * 100 / scale_divider;
#endif
	return scale_multiplier * 100 / scale_divider;
}

INT32 VisualDevice::ScaleLineWidthToScreen(INT32 v) const
{
	if (!IsScaled() || v == 0)
		return v;

	v = ScaleToScreen(v);
	return v < 1 ? 1 : v;
}

#define SCALE_SIGNED_TO_SCREEN(x) (((x) * scale_multiplier) / scale_divider)
#define SCALE_SIGNED_TO_DOC_ROUND_DOWN(x) (((x) * scale_divider) / scale_multiplier)

INT32 VisualDevice::ScaleToScreen(INT32 v) const
{
	if (!IsScaled())
		return v;
	return SCALE_SIGNED_TO_SCREEN(v);
}

double VisualDevice::ScaleToScreenDbl(double v) const
{
	if (!IsScaled())
		return v;
	return SCALE_SIGNED_TO_SCREEN(v);
}

#ifdef VEGA_OPPAINTER_SUPPORT
OpDoublePoint VisualDevice::ScaleToScreenDbl(double x, double y) const
{
	if (!IsScaled())
		return OpDoublePoint(x, y);

	return OpDoublePoint(SCALE_SIGNED_TO_SCREEN(x), SCALE_SIGNED_TO_SCREEN(y));
}
#endif // VEGA_OPPAINTER_SUPPORT

OpPoint VisualDevice::ScaleToScreen(const INT32 &x, const INT32 &y) const
{
	if (!IsScaled())
		return OpPoint(x, y);

	return OpPoint(SCALE_SIGNED_TO_SCREEN(x), SCALE_SIGNED_TO_SCREEN(y));
}

OpRect VisualDevice::ScaleToScreen(const INT32 &x, const INT32 &y, const INT32 &w, const INT32 &h) const
{
	// This algorithm scales width and height so that there are
	// no gaps between things that should be adjacent. Because
	// of that w and h might be scaled different depending on x and y.
	// If that's bad, see ScaleDecorationToScreen
	if (!IsScaled())
		return OpRect(x, y, w, h);

	OpRect rect;
	rect.x = SCALE_SIGNED_TO_SCREEN(x);
	rect.y = SCALE_SIGNED_TO_SCREEN(y);
	int right_x = x+w;
	rect.width = SCALE_SIGNED_TO_SCREEN(right_x) - rect.x;
	if (w != 0 && rect.width == 0)
		rect.width = 1; // We don't want things to disappear completely
	int bottom_y = y + h;
	rect.height = SCALE_SIGNED_TO_SCREEN(bottom_y) - rect.y;
	if (h != 0 && rect.height == 0)
		rect.height = 1; // We don't want things to disappear completely
	return rect;
}

AffinePos VisualDevice::ScaleToScreen(const AffinePos& c) const
{
	if (!IsScaled())
		return c;

#ifdef CSS_TRANSFORMS
	if (c.IsTransform())
	{
		AffinePos scaled_pos = c;
		float vd_scale = (float)scale_multiplier / scale_divider;
		AffineTransform vd_sxfrm;
		vd_sxfrm.LoadScale(vd_scale, vd_scale);
		scaled_pos.Prepend(vd_sxfrm);
		return scaled_pos;
	}
#endif // CSS_TRANSFORMS

	OpPoint scaled_xlt = ScaleToScreen(c.GetTranslation());
	return AffinePos(scaled_xlt.x, scaled_xlt.y);
}

OpRect VisualDevice::ScaleDecorationToScreen(INT32 x, INT32 y, INT32 w, INT32 h) const
{
	// This algorithm doesn't change the scaled w and h depending on
	// x and y as ScaleToScreen does. That makes it possible for
	// things that are close to each other to get gaps so it shouldn't
	// be used for things that should touch such as layout boxes.
	if (!IsScaled())
		return OpRect(x, y, w, h);
	OpRect rect;

	rect.x = SCALE_SIGNED_TO_SCREEN(x);
	rect.y = SCALE_SIGNED_TO_SCREEN(y);
	rect.width = SCALE_SIGNED_TO_SCREEN(w);
	if (w != 0 && rect.width == 0)
		rect.width = 1; // We don't want things to disappear completely
	rect.height = SCALE_SIGNED_TO_SCREEN(h);
	if (h > 0 && rect.height == 0)
		rect.height = 1; // We don't want things to disappear completely
	return rect;
}

INT32 VisualDevice::ScaleToDoc(INT32 v) const
{
	if (scale_multiplier == scale_divider)
		return v;

	return (v * scale_divider + scale_multiplier - 1) / scale_multiplier;
}

OpRect VisualDevice::ScaleToEnclosingDoc(const OpRect& rect) const
{
	if (scale_multiplier == scale_divider)
		return rect;

//  Gives the correct result.

	OpPoint start_point(ScaleToDoc(rect.TopLeft()));
	while (ScaleToScreen(start_point.x) > rect.x)
	{
		start_point.x--;
	}
	while (ScaleToScreen(start_point.y) > rect.y)
	{
		start_point.y--;
	}
	OpRect docrect(start_point.x, start_point.y,
				   ScaleToDoc(rect.width), ScaleToDoc(rect.height));

	BOOL matches;
	do
	{
		matches = TRUE;
		OpRect resulting_screen_rect = ScaleToScreen(docrect);
		if (resulting_screen_rect.x + resulting_screen_rect.width <
			   rect.x + rect.width)
		{
			docrect.width++;
			matches = FALSE;
		}

		if (resulting_screen_rect.y + resulting_screen_rect.height <
			rect.y + rect.height)
		{
			docrect.height++;
			matches = FALSE;
		}
	}
	while (!matches);

	return docrect;

	// Gives almost the correct result.
/*	OpRect r;
	r.x = (rect.x * scale_divider) / scale_multiplier;
	r.y = (rect.y * scale_divider) / scale_multiplier;
	r.width = ((rect.x + rect.width) * scale_divider + scale_multiplier - 1) / scale_multiplier - r.x;
	r.height = ((rect.y + rect.height) * scale_divider + scale_multiplier - 1) / scale_multiplier - r.y;
	return r;*/
}

OpRect VisualDevice::ScaleToDoc(const OpRect& rect) const
{
	if (!IsScaled())
		return rect;

	return OpRect(ScaleToDoc(rect.x),
					ScaleToDoc(rect.y),
					ScaleToDoc(rect.width),
					ScaleToDoc(rect.height));
}

AffinePos VisualDevice::ScaleToDoc(const AffinePos& c) const
{
	if (!IsScaled())
		return c;

#ifdef CSS_TRANSFORMS
	if (c.IsTransform())
	{
		AffinePos scaled_pos = c;
		float vd_scale = (float)scale_divider / scale_multiplier;
		AffineTransform vd_sxfrm;
		vd_sxfrm.LoadScale(vd_scale, vd_scale);
		scaled_pos.Prepend(vd_sxfrm);
		return scaled_pos;
	}
#endif // CSS_TRANSFORMS

	OpPoint scaled_xlt = ScaleToDoc(c.GetTranslation());
	return AffinePos(scaled_xlt.x, scaled_xlt.y);
}

OpPoint	VisualDevice::ConvertToScreen(const OpPoint& doc_point) const
{
	OpPoint view_point;

	view_point.x = doc_point.x - rendering_viewport.x;
	view_point.y = doc_point.y - rendering_viewport.y;

	return view->ConvertToScreen(view_point);
}

INT32 VisualDevice::ScaleToDocRoundDown(INT32 v) const
{
	if (!IsScaled())
		return v;
	return SCALE_SIGNED_TO_DOC_ROUND_DOWN(v);
}

// ====================================================================
void VisualDevice::SetPainter(OpPainter* painter)
{
#if defined(VEGA_OPPAINTER_SUPPORT) && defined(PIXEL_SCALE_RENDERING_SUPPORT)
	OpWindow* opwin = view ? view->GetOpView()->GetRootWindow() : NULL;
	int pixelscale = opwin ? opwin->GetPixelScale() : 100;
	if (scaler.GetScale() != pixelscale)
		scaler.SetScale(pixelscale);
#endif // VEGA_OPPAINTER_SUPPORT && PIXEL_SCALE_RENDERING_SUPPORT

	if (this->painter)
		ResetImageInterpolation();

	this->painter = painter;

	if (painter)
		ResetImageInterpolation();
}

void VisualDevice::BeginPaint(OpPainter* painter)
{
	OP_ASSERT(!this->painter);
	SetPainter(painter);
	logfont.SetChanged();

	activity_paint.Begin();
}

void VisualDevice::BeginPaint(OpPainter* painter, const OpRect& rect, const OpRect& paint_rect)
{
	OP_ASSERT(!this->painter);
	SetPainter(painter);
	logfont.SetChanged();
	doc_display_rect = paint_rect;
	doc_display_rect.x += translation_x;
	doc_display_rect.y += translation_y;
	doc_display_rect_not_enlarged = paint_rect;

	win_pos.SetTranslation(rect.x, rect.y);
	win_width = rect.width;
	win_height = rect.height;

	activity_paint.Begin();
}

void VisualDevice::BeginPaint(OpPainter* painter, const OpRect& rect, const OpRect& paint_rect, const AffinePos& ctm)
{
	BeginPaint(painter, rect, paint_rect);

	ClearTranslation();

	// Setup an initial transform / translation
#ifdef CSS_TRANSFORMS
	if (ctm.IsTransform())
	{
		offset_transform = ctm.GetTransform();
		offset_transform_is_set = true;

		AffineTransform identity;
		identity.LoadIdentity();

		// Enter transformed state
		RETURN_VOID_IF_ERROR(PushTransform(identity)); // FIXME: OOM(?)
	}
	else
#endif // CSS_TRANSFORMS
	{
		OpPoint xlat = ctm.GetTranslation();
		offset_x = xlat.x;
		offset_y = xlat.y;
		offset_x += offset_x_ex;
		offset_y += offset_y_ex;
	}
}

void VisualDevice::EndPaint()
{
	logfont.SetChanged();

#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		// Leave transformed state
		PopTransform();

		// Reset offset transform
		offset_transform_is_set = false;
	}
	OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS

	SetPainter(NULL);

	activity_paint.End();
}

void VisualDevice::BeginPaintLock()
{
	if (view)
		view->GetContainer()->BeginPaintLock();
}

void VisualDevice::EndPaintLock()
{
	if (view)
		view->GetContainer()->EndPaintLock();
}

void VisualDevice::BeginHorizontalClipping(int start, int length)
{
	OpRect cr(start, rendering_viewport.y - translation_y,
			  length, rendering_viewport.height);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		// Define a line:
		//
		// L: O + D * t, O = CTM * (start + length/2, 0)
		// D = CTM * (0, 1) - CTM(0, 0)
		//
		// Then project all the points from the viewport onto this
		// line, and get the min/max.

		const AffineTransform& at = *GetCurrentTransform();
		float dx = at[1], dy = at[4];

		float d_sqr = dx * dx + dy * dy;
		if (op_fabs(d_sqr) > 1e-6)
		{
			int midx = start + length/2;
			float ox = midx * at[0] + at[2], oy = midx * at[3] + at[5];

			float t_min = 0, t_max = 0;
			for (int i = 0; i < 4; ++i)
			{
				int px = (i & 1) ? rendering_viewport.Right() : rendering_viewport.Left();
				int py = (i & 2) ? rendering_viewport.Bottom() : rendering_viewport.Top();

				float t = ((px - ox) * dx + (py - oy) * dy) / d_sqr;

				if (i != 0)
				{
					t_min = MIN(t, t_min);
					t_max = MAX(t, t_max);
				}
				else
				{
					t_min = t_max = t;
				}
			}

			cr.y = (int)op_floor(t_min);
			cr.height = (int)op_ceil(t_max - cr.y);
		}
	}
#endif // CSS_TRANSFORMS

	BeginClipping(cr);
}

void VisualDevice::BeginClipping(const OpRect& rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	OP_NEW_DBG("VisualDevice::BeginClipping", "clipping");
	OP_DBG(("Rect: %d, %d, %d, %d", rect.x, rect.y, rect.width, rect.height));

	OpRect tmprect(rect);

	tmprect.x += translation_x;
	tmprect.y += translation_y;

#ifndef CSS_TRANSFORMS
	// This code could have unwanted side effects when using transforms
	if (tmprect.height < 0)
	{
		tmprect.y = tmprect.height;
		tmprect.height = -tmprect.y;
	}
	if (tmprect.width < 0)
	{
		tmprect.x = tmprect.width;
		tmprect.width = -tmprect.x;
	}
#endif // CSS_TRANSFORMS

	// Flush intersecting backgrounds. We need to do this because
	// outside parts might be flushed from FlushLast. (Bug 247960)
	// Must flush intersecting backgrounds even if the clip doesn't
	// intersect display rect. Nested operations might intersect.
	FlushBackgrounds(rect, FALSE);

#ifdef CSS_TRANSFORMS
	BOOL should_push = !HasTransform();
#else
	const BOOL should_push = TRUE;
#endif // CSS_TRANSFORMS

	if (should_push && OpStatus::IsError(bg_cs.PushClipping(tmprect)))
		goto err;

	tmprect = ToPainter(tmprect);

	if (OpStatus::IsError(painter->SetClipRect(tmprect)))
	{
		if (should_push)
			bg_cs.PopClipping();
		goto err;
	}
	//DEBUG
	//OpRect debug_cliprect;
	//painter->GetClipRect(&debug_cliprect);
	//SetColor(OP_RGB(255, 205, 0));
	//painter->DrawRect(debug_cliprect);
	return;
err:
	if (GetWindow())
		GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	else
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

void VisualDevice::EndClipping(BOOL exclude)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	OP_NEW_DBG("VisualDevice::EndClipping", "clipping");

	if (!exclude)
	{
		painter->RemoveClipRect();
#ifdef CSS_TRANSFORMS
		if (!HasTransform())
#endif // CSS_TRANSFORMS
		bg_cs.PopClipping();
	}
}

OpRect VisualDevice::GetClipping()
{
	OpRect cliprect;
	painter->GetClipRect(&cliprect);
	cliprect.x -= offset_x;
	cliprect.y -= offset_y;
	return ScaleToDoc(cliprect);
}

BOOL VisualDevice::LeftHandScrollbar()
{
	BOOL pos = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LeftHandedUI);
#ifdef SUPPORT_TEXT_DIRECTION
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::RTLFlipsUI))
	{
		FramesDocument* frames_doc = doc_manager->GetCurrentVisibleDoc();
		if(frames_doc)
		{
			LogicalDocument *log_doc = frames_doc->GetLogicalDocument();
			if(log_doc)
			{
				LayoutWorkplace *work_place = log_doc->GetLayoutWorkplace();
				if (work_place && work_place->IsRtlDocument())
					pos = !pos;
			}
		}
	}
#endif // SUPPORT_TEXT_DIRECTION
	return pos;
}

void VisualDevice::ResizeViews()
{
	if (!view || !container)
		return;

	container->SetSize(win_width, win_height);

	int neww = VisibleWidth();
	int newh = VisibleHeight();
	BOOL adjust_view_x = v_on && LeftHandScrollbar();
	BOOL change_x_pos = m_view_pos_x_adjusted != adjust_view_x;

	int oldw, oldh;
	view->GetSize(&oldw, &oldh);
	BOOL resized = oldw != neww || oldh != newh;

	if (resized || change_x_pos)
	{
		MoveScrollbars();

		AffinePos view_pos;
		view->GetPos(&view_pos);

		if(change_x_pos)
		{
			INT32 scr_size = GetVerticalScrollbarSize();
			view_pos.AppendTranslation(adjust_view_x ? scr_size : -scr_size, 0);
			m_view_pos_x_adjusted = adjust_view_x;

			view->SetPos(view_pos);

			if (resized)
				view->SetSize(neww, newh);
		}
		else
			view->SetSize(neww, newh);

		CalculateSize();
	}
}

void VisualDevice::UpdateOffset()
{
	offset_x = 0;
	offset_y = 0;
	if (view)
		view->ConvertToContainer(offset_x, offset_y);
	offset_x += offset_x_ex;
	offset_y += offset_y_ex;
}

void VisualDevice::SetRenderingViewGeometryScreenCoords(const AffinePos& pos, int width, int height)
{
	win_pos = pos;

	win_width = width;
	win_height = height;

	if (view)
	{
		container->SetPos(win_pos);
		container->SetSize(win_width, win_height);

		int new_width = VisibleWidth();
		int new_height = VisibleHeight();
		int old_width, old_height;
		view->GetSize(&old_width, &old_height);
		BOOL resized = old_width != new_width || old_height != new_height;

		if (resized)
		{
			MoveScrollbars();
			view->SetSize(new_width, new_height);
			CalculateSize();
			UpdateAll();
		}
	}
}

OP_STATUS VisualDevice::Show(CoreView* parentview)
{
	if (!view)
		RETURN_IF_ERROR(Init(NULL, parentview));

	if (!size_ready)
	{
		size_ready = TRUE;

		SetRenderingViewGeometryScreenCoords(win_pos, win_width, win_height);
		UpdateOffset();
		ResizeViews();
	}

	if (!view)
		return OpStatus::ERR;

	m_hidden = FALSE;

	GetContainerView()->SetVisibility(!m_hidden && !m_hidden_by_lock);

	return OpStatus::OK;
}

void VisualDevice::Hide(BOOL free)
{
	m_hidden = TRUE;

	if (!view)
		return;

	GetContainerView()->SetVisibility(!m_hidden && !m_hidden_by_lock);

	size_ready = FALSE; // Make it run SetRenderingViewGeometry() when its shown again

	if (free)
		Free(FALSE);
}

BOOL VisualDevice::GetVisible()
{
	if (GetContainerView())
		return GetContainerView()->GetVisibility();
	else
		if (view)
			return TRUE; // a containerless visualdevice
		else
			return FALSE;
}

OP_STATUS VisualDevice::GetNewVisualDevice(VisualDevice *&visdev, DocumentManager* doc_man, ScrollType scroll_type, CoreView* parentview)
{
	visdev = VisualDevice::Create(doc_man, scroll_type, parentview);

	if (!visdev)
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

//rg this won't be used later, therefore no memfix.

#ifdef _PRINT_SUPPORT_

OP_STATUS VisualDevice::CreatePrintPreviewVD(VisualDevice *&preview_vd, PrintDevice* pd)
{
	if (GetWindow()->GetPreviewMode() && doc_manager->GetFrame())
		preview_vd = VisualDevice::Create(GetDocumentManager(), VD_SCROLLING_AUTO, doc_manager->GetParentDoc()->GetVisualDevice()->GetView());
	else
		preview_vd = VisualDevice::Create((OpWindow*)GetWindow()->GetOpWindow(), NULL, VD_SCROLLING_AUTO);

	if (!preview_vd)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	preview_vd->SetRenderingViewGeometryScreenCoords(win_pos, win_width, win_height);

	int percent_scale = GetWindow()->GetScale();
	percent_scale = (percent_scale * g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale)) / 100;

#ifndef MSWIN
	pd->SetScale(percent_scale, FALSE);
#endif // !MSWIN

	preview_vd->SetScale(percent_scale);
	preview_vd->SetParentInputContext(GetParentInputContext());

	return OpStatus::OK;
}

#endif // _PRINT_SUPPORT_

void VisualDevice::ScrollRect(const OpRect &rect, INT32 dx, INT32 dy)
{
	if (GetView() == NULL)
		return;

	OpRect invalid_rect(rect);
	BOOL enlarged = EnlargeWithIntersectingOutlines(invalid_rect);

	if (enlarged || IsScaled())
		GetView()->Invalidate(ScaleToScreen(invalid_rect));
	else
	{
		CheckOverlapped();
		GetView()->ScrollRect(rect, dx, dy);
	}
}

void VisualDevice::ScrollClipViews(int dx, int dy, CoreView *parent)
{
#ifdef _PLUGIN_SUPPORT_
	coreview_clipper.Scroll(dx, dy, parent);
	plugin_clipper.Scroll(dx, dy);
	plugin_clipper.Update();
#endif
}


static inline int gcd(int a, int b)
{
    int r;

    while (b)
	{
		r = a % b;
		a = b;
		b = r;
	}

    return a;
}

UINT32 VisualDevice::SetTemporaryScale(UINT32 newscale)
{
	// Must flush here since we can't flush backgrounds with old scale with new scale.
	bg_cs.FlushAll(this);

	int current_scale = GetScale();
	int common_divisor = gcd(newscale, 100);
	scale_multiplier = newscale / common_divisor;
	scale_divider = 100 / common_divisor;

	UpdateScaleOffset();

	rendering_viewport.width = ScaleToDoc(VisibleWidth());
	rendering_viewport.height = ScaleToDoc(VisibleHeight());

	return current_scale;
}

#ifdef CSS_TRANSFORMS
VDStateNoScale VisualDevice::BeginTransformedScaledPainting(const OpRect &rect, UINT32 scale)
{
	// Scale the destination rect, and apply the inverse scale factor
	// to the current transform stack
	VDStateNoScale state;
	state.dst_rect = ScaleToScreen(rect);

	float inv_vd_scale = (float)scale_divider / scale_multiplier;
	AffineTransform inv_scale;
	inv_scale.LoadScale(inv_vd_scale, inv_vd_scale);
	OpStatus::Ignore(PushTransform(inv_scale)); // FIXME: This might OOM, and then the transform will be incorrect

	// Just put the current state in the returned state object
	state.old_doc_display_rect = doc_display_rect;
	state.old_doc_display_rect_not_enlarged = doc_display_rect_not_enlarged;
	state.old_scale = GetScale();
	state.old_rendering_viewport = rendering_viewport;
	state.old_translation_x = translation_x;
	state.old_translation_y = translation_y;
	return state;
}
#endif // CSS_TRANSFORMS

VDStateNoScale VisualDevice::BeginScaledPainting(const OpRect &rect, UINT32 scale)
{
	painting_scaled++;

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return BeginTransformedScaledPainting(rect, scale);
#endif // CSS_TRANSFORMS

	VDStateNoScale state;
	state.dst_rect = rect;
	state.dst_rect.x += translation_x;
	state.dst_rect.y += translation_y;
	state.dst_rect = ScaleToScreen(state.dst_rect);
	state.dst_rect.x -= view_x_scaled;
	state.dst_rect.y -= view_y_scaled;
	state.old_doc_display_rect = doc_display_rect;
	state.old_doc_display_rect_not_enlarged = doc_display_rect_not_enlarged;
	doc_display_rect = ScaleToScreen(doc_display_rect);
	doc_display_rect.x -= view_x_scaled;
	doc_display_rect.y -= view_y_scaled;
	doc_display_rect_not_enlarged = ScaleToScreen(doc_display_rect_not_enlarged);
	doc_display_rect_not_enlarged.x -= view_x_scaled;
	doc_display_rect_not_enlarged.y -= view_y_scaled;
	state.old_scale = GetScale();
	state.old_rendering_viewport = rendering_viewport;
	state.old_translation_x = translation_x;
	state.old_translation_y = translation_y;
	SetTemporaryScale(scale);
	translation_x = 0;
	translation_y = 0;
	view_x_scaled = 0;
	view_y_scaled = 0;
	rendering_viewport.x = 0;
	rendering_viewport.y = 0;
	rendering_viewport.width = ScaleToScreen(rendering_viewport.width);
	rendering_viewport.height = ScaleToScreen(rendering_viewport.height);
	return state;
}

void VisualDevice::EndScaledPainting(const VDStateNoScale &state)
{
	SetTemporaryScale(state.old_scale);
	Translate(state.old_translation_x, state.old_translation_y);
	rendering_viewport = state.old_rendering_viewport;
	UpdateScaleOffset();
	doc_display_rect = state.old_doc_display_rect;
	doc_display_rect_not_enlarged = state.old_doc_display_rect_not_enlarged;

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		PopTransform();
#endif // CSS_TRANSFORMS

	OP_ASSERT(painting_scaled > 0);
	painting_scaled--;
}

#ifdef CSS_TRANSFORMS
// Reset scale, offset and other things that we have applied to the base-transform
VDStateTransformed VisualDevice::BeginTransformedPainting()
{
	VDStateTransformed state;
	state.old_translation_x = translation_x;
	state.old_translation_y = translation_y;

	translation_x = 0;
	translation_y = 0;

	return state;
}

void VisualDevice::EndTransformedPainting(const VDStateTransformed &state)
{
	Translate(state.old_translation_x, state.old_translation_y);
}
#endif // CSS_TRANSFORMS

VDStateNoTranslationNoOffset VisualDevice::BeginNoTranslationNoOffsetPainting()
{
	VDStateNoTranslationNoOffset state;
	state.old_translation_x = translation_x;
	state.old_translation_y = translation_y;
	state.old_offset_x = offset_x;
	state.old_offset_y = offset_y;
	state.old_view_x_scaled = view_x_scaled;
	state.old_view_y_scaled = view_y_scaled;

	translation_x = 0;
	translation_y = 0;
	offset_x = 0;
	offset_y = 0;
	view_x_scaled = 0;
	view_y_scaled = 0;

	return state;
}

void VisualDevice::EndNoTranslationNoOffsetPainting(const VDStateNoTranslationNoOffset &state)
{
	translation_x = state.old_translation_x;
	translation_y = state.old_translation_y;
	offset_x = state.old_offset_x;
	offset_y = state.old_offset_y;
	view_x_scaled = state.old_view_x_scaled;
	view_y_scaled = state.old_view_y_scaled;
}

INT32 VisualDevice::LayoutScaleToDoc(INT32 v) const
{
	return (v * m_layout_scale_divider + m_layout_scale_multiplier - 1) / m_layout_scale_multiplier;
}
INT32 VisualDevice::LayoutScaleToScreen(INT32 v) const
{
	return v * m_layout_scale_multiplier / m_layout_scale_divider;
}

void VisualDevice::SetLayoutScale(UINT32 scale)
{
	if (m_layout_scale_multiplier * 100 / m_layout_scale_divider != (INT32)scale)
	{
		int common_divisor = gcd(scale, 100);
		m_layout_scale_multiplier = scale / common_divisor;
		m_layout_scale_divider = 100 / common_divisor;
		OP_ASSERT(m_layout_scale_multiplier * 100 / m_layout_scale_divider == (INT32)scale);

		// Need to make sure we don't use old font metrics etc.
		logfont.SetChanged();
	}
}

void VisualDevice::SetScale(UINT32 scale, BOOL updatesize)
{
	step = 1;
	// Calculate scalerounding
// FIX: Have to have step for sheet position in presentationmode!
/*	int i = 0;
	do
	{
		i++;
		step = (100 * i) / scale;
	} while((100 * i) % scale);*/

	int common_divisor = gcd(scale, 100);
	scale_multiplier = scale / common_divisor;
	scale_divider = 100 / common_divisor;
	OP_ASSERT(scale_multiplier * 100 / scale_divider == (INT32)scale);

	UpdateScaleOffset();

	logfont.SetChanged();

#ifdef _PLUGIN_SUPPORT_
	// Hide clipviews. They will be showed again from the layoutboxes Paint if still inside visible area.
	coreview_clipper.Hide();
#endif // _PLUGIN_SUPPORT_

	if (doc_manager && updatesize && view)
	{
		CalculateSize();

		view->Invalidate(OpRect(0,0,win_width,win_height));
	}
}

void VisualDevice::CheckOverlapped()
{
	if (!check_overlapped || !doc_manager || !view || m_hidden)
		return;

	BOOL overlapped = FALSE;

	if (view->GetContainer()->IsPainting())
	{
		// We can't traverse the parent document now when we are already doing so.
		// Treat the views as overlapped so scroll will invalidate in this case (Reasonable thing to do anyway).
		// Let check_overlapped stay TRUE so it will be properly updated later.
		overlapped = TRUE;
	}
	else
	{
		FramesDocument* pdoc = doc_manager->GetParentDoc();
		if (pdoc)
		{
			if (!doc_manager->GetFrame()->IsInlineFrame())
				pdoc = pdoc->GetTopFramesDoc();
			if (pdoc && !pdoc->GetFrmDocRoot() && !pdoc->IsReflowing())
			{
				// This is a iframe. Check if it is overlapped.

				HTML_Element* iframe_elm = doc_manager->GetFrame()->GetHtmlElement();
				if (iframe_elm)
				{
					RECT cliprect;
					if (!pdoc->GetLogicalDocument()->GetCliprect(iframe_elm, cliprect))
						overlapped = TRUE;
				}

				check_overlapped = FALSE;
			}
		}
	}

	if (container)
		container->GetView()->SetIsOverlapped(overlapped);
	view->SetIsOverlapped(overlapped);
}

void VisualDevice::OnReflow()
{
	FramesDocument* frames_doc = doc_manager ? doc_manager->GetCurrentVisibleDoc() : 0;
	if (!frames_doc)
		return;

	// After a reflow the m_include_highlight_in_updates flag has done what it should and
	// is not needed anymore.
	m_include_highlight_in_updates = FALSE;

	// Iframes might be overlapped now so we must set the check_overlapped flag to TRUE so
	// the overlapped status is rechecked before they scroll.

	FramesDocElm *fdelm = frames_doc->GetIFrmRoot();
	fdelm = fdelm ? fdelm->FirstChild() : NULL;

	while (fdelm)
	{
		if (VisualDevice *vd = fdelm->GetVisualDevice())
			vd->check_overlapped = TRUE;
		fdelm = fdelm->Suc();
	}
}

OP_STATUS VisualDevice::Init(OpWindow* parentWindow, CoreView* parentview)
{
	size_ready = FALSE;

	color = OP_RGB(0, 0, 0);
	bg_color = USE_DEFAULT_COLOR;
	bg_color_light = USE_DEFAULT_COLOR;
	bg_color_dark = USE_DEFAULT_COLOR;
	use_def_bg_color = TRUE;

	char_spacing_extra = 0;
	current_font_number = 0;

	container = OP_NEW(WidgetContainer, ());
	if (container == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (parentWindow) // this is the regular document case
	{
		OP_STATUS err = container->Init(OpRect(0, 0, 0, 0), (OpWindow*)parentWindow);
		if (OpStatus::IsError(err))
		{
			OP_DELETE(container);
			container = NULL;
			return err;
		}
		// We can create a windowless view (CoreView)
		//RETURN_IF_ERROR(CoreView::Create(&view, container->GetView()));
		// But we can also create a CoreViewContainer in this case when we are the topdocument.
		RETURN_IF_ERROR(CoreViewContainer::Create(&view, NULL, NULL, container->GetView()));
		view->SetVisibility(TRUE);
	}
	else // this is the frame case
	{
		FramesDocument* pdoc = doc_manager->GetParentDoc();
		if (pdoc)
		{
			if (!doc_manager->GetFrame()->IsInlineFrame())
				pdoc = pdoc->GetTopFramesDoc();
		}
		if (pdoc)
		{
			OP_ASSERT(parentview); // You should have either a parentview or parentWindow !
			if (parentview == NULL)
				parentview = pdoc->GetVisualDevice()->view;

			if (parentview != NULL)
			{
				OP_STATUS err = container->Init(OpRect(0, 0, 0, 0), (OpWindow*)GetWindow()->GetOpWindow(), parentview);
				if (OpStatus::IsError(err))
				{
					OP_DELETE(container);
					container = NULL;
					return err;
				}
				if (pdoc->IsFrameDoc() && !pdoc->GetParentDoc())
				{
					// Frames in framesets that is not inside a iframe should get a container so scrolling is fastest possible.
					RETURN_IF_ERROR(CoreViewContainer::Create(&view, NULL, NULL, container->GetView()));
				}
				else
					RETURN_IF_ERROR(CoreView::Create(&view, container->GetView()));

				if (doc_manager->GetFrame()->IsInlineFrame())
					container->GetView()->SetOwningHtmlElement(doc_manager->GetFrame()->GetHtmlElement());

				if (!pdoc->GetFrmDocRoot())
				{
					// Iframes shouldn't get automatical paint since we paint them in z-index order.
					container->GetView()->SetWantPaintEvents(FALSE);
				}
				else
					view->SetVisibility(TRUE);
				// Look up initial scale (think navigation in iframes
				// which recreate their visdev or new iframes)
				int window_scale = GetDocumentManager()->GetWindow()->GetScale();
				SetScale(window_scale, FALSE); // Updates both scale and scalerounding

				SetTextScale(GetDocumentManager()->GetWindow()->GetTextScale());
			}
			else
			{
				OP_DELETE(container);
				container = NULL;
				return OpStatus::ERR;
			}
		}
	}

	// Setup the widgetcontainer and add the scrollbars to it
	container->GetView()->SetVisibility(FALSE);
	container->SetParentInputContext(this);

	RETURN_IF_ERROR(OpScrollbar::Construct(&h_scroll, TRUE));
	container->GetRoot()->AddChild(h_scroll);
	RETURN_IF_ERROR(OpScrollbar::Construct(&v_scroll, FALSE));
	container->GetRoot()->AddChild(v_scroll);
	RETURN_IF_ERROR(OpWindowResizeCorner::Construct(&corner));
	container->GetRoot()->AddChild(corner);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	h_scroll->SetAccessibleParent(this);
	v_scroll->SetAccessibleParent(this);
	corner->SetAccessibleParent(this);
#endif

	h_scroll->SetVisibility(h_on);
	v_scroll->SetVisibility(v_on);
#ifndef _MACINTOSH_
// TODO: this should better be a tweak, which is enabled by default
// and disabled for _MACINTOSH_ or the tweak could be handled inside
// the corner implementation:
	corner->SetVisibility(h_on && v_on);
#endif // _MACINTOSH_

	corner->SetTargetWindow(GetWindow());

	//Create and set the listeners...
	paint_listener = OP_NEW(PaintListener, (this));
#ifndef MOUSELESS
	mouse_listener = OP_NEW(MouseListener, (this));
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	touch_listener = OP_NEW(TouchListener, (this));
#endif // TOUCH_EVENTS_SUPPORT
	scroll_listener = OP_NEW(ScrollListener, (this));
#ifdef DRAG_SUPPORT
	drag_listener = OP_NEW(DragListener, (this));
#endif

#ifndef MOUSELESS
	OP_STATUS mouse_listener_init_status = OpStatus::ERR;

	if (mouse_listener != NULL)
	{
		mouse_listener_init_status = ((MouseListener*)mouse_listener)->Init();
	}
#endif // !MOUSELESS

	if (paint_listener == NULL ||
#ifndef MOUSELESS
		mouse_listener == NULL ||
		OpStatus::IsError(mouse_listener_init_status) ||
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
		touch_listener == NULL ||
#endif // TOUCH_EVENTS_SUPPORT
		scroll_listener == NULL
#ifdef DRAG_SUPPORT
		|| drag_listener == NULL
#endif
		)
	{
		OP_DELETE(paint_listener);
		paint_listener = NULL;
#ifndef MOUSELESS
		OP_DELETE(mouse_listener);
		mouse_listener = NULL;
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
		OP_DELETE(touch_listener);
		touch_listener = NULL;
#endif // TOUCH_EVENTS_SUPPORT
		OP_DELETE(scroll_listener);
		scroll_listener = NULL;
#ifdef DRAG_SUPPORT
		OP_DELETE(drag_listener);
		drag_listener = NULL;
#endif
		OP_DELETE(view);
		view = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	v_scroll->SetListener((ScrollListener*)scroll_listener);
	h_scroll->SetListener((ScrollListener*)scroll_listener);

	view->SetPaintListener(paint_listener);
#ifndef MOUSELESS
	view->SetMouseListener(mouse_listener);
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	view->SetTouchListener(touch_listener);
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	view->SetDragListener(drag_listener);
#endif
	view->SetParentInputContext(this);

#ifdef PLATFORM_FONTSWITCHING
	view->GetOpView()->AddAsyncFontListener(this);
#endif

	logfont.Clear();

	painter = NULL;

	if (doc_manager && doc_manager->GetParentDoc())
		SetParentInputContext(doc_manager->GetParentDoc()->GetVisualDevice());

	return OpStatus::OK;
}

void VisualDevice::Free(BOOL destructing)
{
	StopTimer();

	if (g_main_message_handler->HasCallBack(this, MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this))
		g_main_message_handler->UnsetCallBack(this, MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this);
	RemoveMsgMouseMove();

	if (!destructing)
		ReleaseFocus();

	if (view)
	{
#ifdef PLATFORM_FONTSWITCHING
		GetOpView()->RemoveAsyncFontListener(this);
#endif

		OP_DELETE(view);				view = NULL;
		OP_DELETE(paint_listener);		paint_listener = NULL;
#ifndef MOUSELESS
		OP_DELETE(mouse_listener);		mouse_listener = NULL;
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
		OP_DELETE(touch_listener);		touch_listener = NULL;
#endif // TOUCH_EVENTS_SUPPORT
		OP_DELETE(scroll_listener);		scroll_listener = NULL;
#ifdef DRAG_SUPPORT
		OP_DELETE(drag_listener);		drag_listener = NULL;
#endif
	}

	OP_DELETE(container);
	container = NULL;
	v_scroll = NULL;
	h_scroll = NULL;
	corner = NULL;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OP_DELETE(m_accessible_doc);
	m_accessible_doc = NULL;
#endif
	g_font_cache->ReleaseFont(currentFont);

	currentFont = NULL;

	OP_DELETE(m_cachedBB);
	m_cachedBB = NULL;

	box_shadow_corners.Clear();
}

BOOL VisualDevice::GetShouldAutoscrollVertically() const
{
	if (GetWindow() != 0 && GetWindow()->GetType() == WIN_TYPE_IM_VIEW)
		return m_autoscroll_vertically;
	else
		return FALSE;
}

void VisualDevice::CalculateSize()
{
	int tmpx,tmpy;

	GetContainerView()->GetSize(&tmpx, &tmpy);
	win_width = tmpx;
	win_height = tmpy;

	tmpx = VisibleWidth();
	tmpy = VisibleHeight();

	rendering_viewport.width = ScaleToDoc(tmpx);
	rendering_viewport.height = ScaleToDoc(tmpy);
}

VisualDevice::VisualDevice()
  :
    offset_x(0),
	offset_y(0),
    offset_x_ex(0),
	offset_y_ex(0),
	view(NULL),
	outlines_open_count(0),
	current_outline(NULL),
	m_outlines_enabled(FALSE),
	check_overlapped(TRUE),
#ifdef _PLUGIN_SUPPORT_
	m_clip_dev(0),
#endif // _PLUGIN_SUPPORT_
	container(NULL),
	v_scroll(NULL),
	h_scroll(NULL),
	corner(NULL),
	doc_width(0),
	negative_overflow(0),
	doc_height(0),
	v_on(FALSE),
	h_on(FALSE),
	pending_auto_v_on(FALSE),
	pending_auto_h_on(FALSE),
	step(1),
	painting_scaled(0),
	win_width(0),
	win_height(0),
	view_x_scaled(0),
	view_y_scaled(0),
	scroll_type(VD_SCROLLING_AUTO),
	//vertical_scrollbar_on(FALSE),
	doc_manager(NULL),
	current_font_number(0) // no comma
	, bg_color(USE_DEFAULT_COLOR)
	, bg_color_light(USE_DEFAULT_COLOR)
	, bg_color_dark(USE_DEFAULT_COLOR)
	, use_def_bg_color(TRUE)
	, color(OP_RGB(0, 0, 0))
	, char_spacing_extra(0)
	, accurate_font_size(0)
	, m_draw_focus_rect(DISPLAY_DEFAULT_FOCUS_RECT)
	, m_text_scale(100)
	, m_layout_scale_multiplier(1)
	, m_layout_scale_divider(1)
	, m_cachedBB(NULL)
	, m_hidden_by_lock(FALSE)
	, m_hidden(FALSE)
	, m_lock_count(0)
	, m_update_all(FALSE)
	, m_include_highlight_in_updates(FALSE)
	, m_view_pos_x_adjusted(FALSE)
	, m_posted_msg_update(FALSE)
	, m_posted_msg_mousemove(FALSE)
	, m_post_msg_update_ms(0.0)
	, m_current_update_delay(0)
	, m_autoscroll_vertically(TRUE) // Only applies for windows of type WIN_TYPE_IM_VIEW.
#ifdef FEATURE_SCROLL_MARKER
	, m_scroll_marker(NULL)
#endif //FEATURE_SCROLL_MARKER
	, scale_multiplier(1)
	, scale_divider(1)
#ifdef PAN_SUPPORT
	, isPanning(NO)
#endif // PAN_SUPPORT
	, activity_paint(ACTIVITY_PAINT)
	, painter(NULL)
	, currentFont(NULL)
	, paint_listener(NULL)
#ifndef MOUSELESS
	, mouse_listener(NULL)
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	, touch_listener(NULL)
#endif // TOUCH_EVENTS_SUPPORT
	, scroll_listener(NULL)
#ifdef DRAG_SUPPORT
	, drag_listener(NULL)
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, m_accessible_doc(NULL)
#endif
{
	actual_viewport = &rendering_viewport;
#ifdef CSS_TRANSFORMS
	offset_transform_is_set = false;
#endif // CSS_TRANSFORMS

	logfont.Clear();
	SetCharSpacingExtra(0);
}

OP_STATUS
VisualDevice::Init(OpWindow* parentWindow, DocumentManager* doc_man, ScrollType scrolltype)
{
	RETURN_IF_ERROR(Init(parentWindow));

	Show(NULL);

#ifdef FEATURE_SCROLL_MARKER
	RETURN_IF_ERROR(OpScrollMarker::Create(&m_scroll_marker, this));
	AddScrollListener(m_scroll_marker);
#endif //FEATURE_SCROLL_MARKER

	return OpStatus::OK;
}

VisualDevice*
VisualDevice::Create(OpWindow* parentWindow, DocumentManager* doc_man, ScrollType scrolltype)
{
	VisualDevice *vd = OP_NEW(VisualDevice, ());
	if (!vd)
	{
		return NULL;
	}
	vd->scroll_type = scrolltype;
	vd->doc_manager = doc_man;
	OP_STATUS status = vd->Init(parentWindow, doc_man, scrolltype);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(vd);
		return NULL;
	}
	return vd;
}

VisualDevice*
VisualDevice::Create(DocumentManager* doc_man, ScrollType scrolltype, CoreView* parentview)
{
	VisualDevice *vd = OP_NEW(VisualDevice, ());
	if (!vd)
	{
		return NULL;
	}
	vd->scroll_type = scrolltype;
	vd->doc_manager = doc_man;
	OP_STATUS status = vd->Init(doc_man->GetSubWinId() >= 0 ? NULL : doc_man->GetWindow()->GetOpWindow(), parentview);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(vd);
		return NULL;
	}
	return vd;
}

/** The destructor. Calls Free() to do the job. */
VisualDevice::~VisualDevice()
{
#ifdef FEATURE_SCROLL_MARKER
	if (m_scroll_marker)
	{
		RemoveScrollListener(m_scroll_marker);
		OP_DELETE(m_scroll_marker);
	}
#endif //FEATURE_SCROLL_MARKER

	OP_ASSERT(!backbuffers.First());
	RemoveAllOutlines();
#ifdef DISPLAY_SPOTLIGHT
	RemoveAllSpotlights();
#endif
	Free(TRUE);

	// remove any trailing callback listeners
	g_main_message_handler->UnsetCallBacks(this);
}

/** @return the Window in which we live. */
Window* VisualDevice::GetWindow() const
{
	return doc_manager ? doc_manager->GetWindow() : 0;
}

OpView* VisualDevice::GetOpView()
{
	return GetView() ? GetView()->GetOpView() : NULL;
}

CoreView* VisualDevice::GetContainerView() const
{
	return container ? container->GetView() : NULL;
}

void Get3D_Colors(COLORREF col, COLORREF &color_dark, COLORREF &color_light)
{
	BOOL gray =    op_abs(OP_GET_R_VALUE(col) - OP_GET_G_VALUE(col)) < 10
				&& op_abs(OP_GET_B_VALUE(col) - OP_GET_G_VALUE(col)) < 10;

	BYTE r = OP_GET_R_VALUE(col) / 2;
	BYTE g = OP_GET_G_VALUE(col) / 2;
	BYTE b = OP_GET_B_VALUE(col) / 2;
	if (!gray)
	{
		r += OP_GET_R_VALUE(col) / 4;
		g += OP_GET_G_VALUE(col) / 4;
		b += OP_GET_B_VALUE(col) / 4;
	}
	color_dark = OP_RGBA(r, g, b, OP_GET_A_VALUE(col));

		r = 255 - (255-OP_GET_R_VALUE(col)) / 2;
		g = 255 - (255-OP_GET_G_VALUE(col)) / 2;
		b = 255 - (255-OP_GET_B_VALUE(col)) / 2;
	if (!gray)
	{
		r -= (255-OP_GET_R_VALUE(col)) / 4;
		g -= (255-OP_GET_G_VALUE(col)) / 4;
		b -= (255-OP_GET_B_VALUE(col)) / 4;
	}
	color_light = OP_RGBA(r, g, b, OP_GET_A_VALUE(col));
}

COLORREF VisualDevice::GetGrayscale(COLORREF color)
{
	BYTE r, g, b;
	r = OP_GET_R_VALUE(color);
	g = OP_GET_G_VALUE(color);
	b = OP_GET_B_VALUE(color);

    int greylevel = (int)((r * 0.30) + (g * 0.59) + (b * 0.11));

	return OP_RGB(greylevel, greylevel, greylevel);
}

void VisualDevice::SetBgColor(COLORREF col)
{
    col = HTM_Lex::GetColValByIndex(col);

    if ((!use_def_bg_color && col != bg_color) || use_def_bg_color)
	{
		BYTE r, g, b;

#ifdef _PRINT_SUPPORT_
		// Convert to gray scale if we are in preview mode and have a monochrome printer
		if (GetWindow() && GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
		{
			col = GetGrayscale(col);
		}
#endif // _PRINT_SUPPORT_

		bg_color = col;

		r = OP_GET_R_VALUE(col) / 2;
		g = OP_GET_G_VALUE(col) / 2;
		b = OP_GET_B_VALUE(col) / 2;
		bg_color_dark = OP_RGBA(r, g, b, OP_GET_A_VALUE(col));

		r = 255 - (255-OP_GET_R_VALUE(col)) / 2;
		g = 255 - (255-OP_GET_G_VALUE(col)) / 2;
		b = 255 - (255-OP_GET_B_VALUE(col)) / 2;
		bg_color_light = OP_RGBA(r, g, b, OP_GET_A_VALUE(col));

		use_def_bg_color = FALSE;
	}
}

void VisualDevice::SetDefaultBgColor()
{
	use_def_bg_color = TRUE;
}

void VisualDevice::DrawBgColor(const RECT& rect)
{
	DrawBgColor(OpRect(&rect));
}

void VisualDevice::DrawBgColor(const OpRect& rect, COLORREF override_bg_color, BOOL translate)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	COLORREF cref;
	if (use_def_bg_color)
		cref = GetWindow()->GetDefaultBackgroundColor();
	else if (override_bg_color == USE_DEFAULT_COLOR)
		cref = bg_color;
	else
		cref = override_bg_color;

	BOOL opacity_layer = FALSE;
	if (OP_GET_A_VALUE(cref) != 255 && !painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))
	{
		if (OpStatus::IsSuccess(BeginOpacity(rect, OP_GET_A_VALUE(cref))))
		{
			opacity_layer = TRUE;
		}
		painter->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref));
	}
	else
		painter->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref), OP_GET_A_VALUE(cref));

	OpRect r(rect);
	if (translate)
		r.OffsetBy(translation_x, translation_y);
	painter->FillRect(ToPainter(r));

	if (opacity_layer)
		EndOpacity();

	painter->SetColor(OP_GET_R_VALUE(this->color), OP_GET_G_VALUE(this->color), OP_GET_B_VALUE(this->color), OP_GET_A_VALUE(this->color));
}
// end of font accessors

#ifdef CSS_GRADIENT_SUPPORT
OP_STATUS VisualDevice::DrawBorderImageGradientTiled(const CSS_Gradient& gradient, const OpRect& border_box, const OpRect& src_rect, const OpRect& destination_rect, double dx, double dy, int ofs_x, int ofs_y, COLORREF current_color)
{
	OpRect local_clip_rect(rendering_viewport);

	AffinePos vd_ctm = GetCTM();
	vd_ctm.ApplyInverse(local_clip_rect);

	if (!local_clip_rect.Intersecting(destination_rect))
		return OpStatus::OK;


	// 1. Calculate where to start and end tiling output, clipping against the rendering viewport.

	int tile_end_x = destination_rect.Right();
	int tile_end_y = destination_rect.Bottom();

	if (local_clip_rect.x > destination_rect.x + ofs_x)
	{
		int idx = (int)dx;
		int number_of_skipped_tiles = (local_clip_rect.x - (destination_rect.x + ofs_x)) / idx;
		ofs_x += int(number_of_skipped_tiles * dx);
	}
	if (local_clip_rect.y > destination_rect.y + ofs_y)
	{
		int idy = (int)dy;
		int number_of_skipped_tiles = (local_clip_rect.y - (destination_rect.y + ofs_y)) / idy;
		ofs_y += int(number_of_skipped_tiles * dy);
	}

	/* Clip to the rendering viewport and make sure the last tile fits by adding an extra tile
	   to the end coordinate. This is not 100% accurate but doesnt need to be. */

	if (local_clip_rect.Right() < destination_rect.Right())
	{
		tile_end_x = local_clip_rect.Right() + (int)dx;
	}
	if (local_clip_rect.Bottom() < destination_rect.Bottom())
	{
		tile_end_y = local_clip_rect.Bottom() + (int)dy;
	}


	// 2. Paint the gradient tiles.

	if (dx > 0 && dy > 0)
	{
		double x, y;

		for (y = destination_rect.y + ofs_y; y < tile_end_y; y += dy)
			for (x = destination_rect.x + ofs_x; x < tile_end_x; x += dx)
			{
				OpRect d_rect = OpRect((INT32)x, (INT32)y, (INT32)dx, (INT32)dy);

				RETURN_IF_ERROR(BorderImageGradientPartOut(border_box, gradient, src_rect, d_rect, current_color));
			}
	}
	return OpStatus::OK;
}

void VisualDevice::DrawBgGradient(const OpRect& raw_paint_rect, const OpRect& raw_gradient_rect, const CSS_Gradient& gradient, const OpRect& raw_repeat_space, COLORREF current_color)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	OpRect gradient_rect = ToPainter(raw_gradient_rect);
	OpRect paint_rect = ToPainter(raw_paint_rect);
	OpRect repeat_space = ToPainter(raw_repeat_space);
	OpRect tile(gradient_rect);
	tile.width += repeat_space.width;
	tile.height += repeat_space.height;

	// Transpose the offset by tile dimensions so that the tile is within the image.
	OpPoint offset = paint_rect.TopLeft() - gradient_rect.TopLeft();
	TransposeOffsetInsideImage(offset, tile.width, tile.height);

	OpRect clip_rect(ToPainter(rendering_viewport));
#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		// If we render using a transform, there will be a mismatch in
		// the clipping calculations below (document space vs. local
		// space), so convert paint_rect to local space if a transform
		// is used.
		GetCTM().ApplyInverse(clip_rect);
	}
#endif // CSS_TRANSFORMS

	OpRect tile_area = VisibleTileArea(offset, paint_rect, clip_rect, tile);

	VEGAOpPainter *vpainter = static_cast<VEGAOpPainter*>(painter);
	VEGAFill *gradient_out;
	VEGATransform radial_adjust;
	radial_adjust.loadIdentity();
	gradient_out = gradient.MakeVEGAGradient(this, vpainter, raw_gradient_rect, current_color, radial_adjust);

	if (!gradient_out)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

#ifdef DISPLAY_TILE_CSS_GRADIENTS
	if (vpainter->Supports(OpPainter::SUPPORTS_TILING) && (tile.width < tile_area.width || tile.height < tile_area.height))
	{
		// Render the gradient tile to a bitmap and then use the built in tiling support if it's available.
		OpBitmap* tile_bitmap = NULL;
		if (OpStatus::IsSuccess(OpBitmap::Create(&tile_bitmap, tile.width, tile.height, FALSE, TRUE, 0, 0, TRUE)))
		{
			// Image spacing is handled by making the whole tile_bitmap transparent and then only
			// paint where the gradient should be.
			tile_bitmap->SetColor(NULL, TRUE, NULL);
			OpPainter* gradient_painter = tile_bitmap->GetPainter();
			if (gradient_painter)
			{
				VEGAOpPainter* vgradient_painter = static_cast<VEGAOpPainter*>(gradient_painter);
				vgradient_painter->SetFill(gradient_out);
				vgradient_painter->SetFillTransform(radial_adjust);
				vgradient_painter->FillRect(OpRect(0, 0, gradient_rect.width, gradient_rect.height));
				tile_bitmap->ReleasePainter();

				vpainter->DrawBitmapTiled(tile_bitmap, offset, paint_rect, 100, tile.width, tile.height);
			}
			OP_DELETE(tile_bitmap);
		}
	}
	else
#endif // DISPLAY_TILE_CSS_GRADIENTS
	{
		vpainter->SetFill(gradient_out);
		VEGATransform tx;

		for (int x = tile_area.x; x < tile_area.x + tile_area.width; x += tile.width)
			for (int y = tile_area.y; y < tile_area.y + tile_area.height; y += tile.height)
			{
				tx.loadTranslate(VEGA_INTTOFIX(x),VEGA_INTTOFIX(y));
				tx.multiply(radial_adjust);
				vpainter->SetFillTransform(tx);

				OpRect clipped_rect = OpRect(x,y, gradient_rect.width, gradient_rect.height);
				clipped_rect.IntersectWith(paint_rect);

				painter->FillRect(clipped_rect);
			}
		vpainter->SetFill(NULL);
		vpainter->ResetFillTransform();
	}

	OP_DELETE(gradient_out);
}

void VisualDevice::GradientImageOut(const OpRect& destination_rect, COLORREF current_color, const CSS_Gradient& gradient)
{
	OpRect translated_dest(destination_rect);
	translated_dest.OffsetBy(translation_x, translation_y);

	DrawBgGradient(translated_dest, translated_dest, gradient, OpRect(0,0,0,0), current_color);
}
#endif // CSS_GRADIENT_SUPPORT

#ifdef PAN_SUPPORT

BOOL VisualDevice::PanDocument(int dx, int dy)
{
	BOOL changed = FALSE;

	if (FramesDocument* doc = doc_manager ? doc_manager->GetCurrentVisibleDoc() : NULL)
		changed = doc->RequestSetVisualViewPos(doc->GetVisualViewX()+dx, doc->GetVisualViewY()+dy, VIEWPORT_CHANGE_REASON_INPUT_ACTION);

	return changed;
}

void VisualDevice::StartPotentialPanning(INT32 x, INT32 y)
{
	OP_ASSERT(isPanning == NO);
	panLastX = panNewX = x;
	panLastY = panNewY = y;
	rdx = crdx = 0;
	rdy = crdy = 0;
	isPanning = MAYBE;
}
void VisualDevice::StartPanning(OpWindow* ow)
{
	// we don't want widget to be pushed down etc. when panning
	// is started on it
	if (OpWidget::hooked_widget)
	{
		OpWidget* w = OpWidget::hooked_widget;
		w->GenerateOnMouseLeave();
		w->ReleaseFocus(FOCUS_REASON_RELEASE);

		// make sure hooked_widget is set - used as input context from
		// MouseListener::OnMouseMove if set
		OpWidget::hooked_widget = w;

		FramesDocument* frames_doc = GetDocumentManager() ? GetDocumentManager()->GetCurrentVisibleDoc() : 0;
		HTML_Document *html_doc = frames_doc ? frames_doc->GetHtmlDocument() : 0;
		if (html_doc)
			html_doc->SetCapturedWidgetElement(0);

	}

	if (ow)
	{
		panOldCursor = (int)ow->GetCursor();
		ow->SetCursor(CURSOR_MOVE);
	}
	else
		// "magic" value to avoid setting wrong cursor when StopPanning is called
		panOldCursor = CURSOR_NUM_CURSORS;
	isPanning = YES;
}

void VisualDevice::StopPanning(OpWindow* ow)
{
	// clear hooked_widget
	if (isPanning == YES)
		OpWidget::hooked_widget = 0;

	if (isPanning == YES && ow && panOldCursor != CURSOR_NUM_CURSORS)
		ow->SetCursor((CursorType)panOldCursor);
	isPanning = NO;
}

void VisualDevice::PanHookedWidget()
{
	OP_ASSERT(OpWidget::hooked_widget);

#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(isPanning != YES);
#endif // DEBUG_ENABLE_OPASSERT
	OpWidget* widget = OpWidget::hooked_widget;
	if (
		// don't pan on scroll bars, it's just stupid
		widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR ||
#ifdef DO_NOT_PAN_EDIT_FIELDS
		widget->GetType() == OpTypedObject::WIDGET_TYPE_EDIT ||
		widget->GetType() == OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT ||
#endif // DO_NOT_PAN_EDIT_FIELDS
		// don't allow panning for ui widgets. however, drop downs pan a widget window, which isn't a form object.
		(!widget->IsForm()
		&& widget->GetType() != OpTypedObject::WIDGET_TYPE_DROPDOWN
		&& widget->GetType() != OpTypedObject::WIDGET_TYPE_LISTBOX))
		isPanning = NO;
}

void VisualDevice::TouchPanMouseDown(const OpPoint& sp)
{
	StartPotentialPanning(sp.x, sp.y);
}

void VisualDevice::PanMouseDown(const OpPoint& sp, ShiftKeyState keystate)
{
	if ((keystate & PAN_KEYMASK) == PAN_KEYMASK || (GetWindow() && GetWindow()->GetScrollIsPan()))
		StartPotentialPanning(sp.x, sp.y);
}

BOOL VisualDevice::PanMouseMove(const OpPoint& sp, OpInputContext* input_context, OpWindow* ow)
{
	// start panning if delta is bigger than PAN_START_THRESHOLD
	if (isPanning == MAYBE)
	{
		// update mouse position
		SetPanMousePos(sp.x, sp.y);
		if (MAX(op_abs(GetPanMouseDeltaX()), op_abs(GetPanMouseDeltaY())) > PAN_START_THRESHOLD)
		{
#ifdef DO_NOT_PAN_ELEMENTS_WITH_MOVE_LISTENERS
			Window* win = GetWindow();
			FramesDocument* frmdoc = win ? win->GetCurrentDoc() : 0;
			HTML_Document* htmldoc = frmdoc ? frmdoc->GetHtmlDocument() : 0;
			HTML_Element* helm = htmldoc ? htmldoc->GetHoverHTMLElement() : 0;
			if (helm && helm->HasEventHandler(frmdoc, ONMOUSEMOVE, FALSE))
				isPanning = NO;
			else
#endif // DO_NOT_PAN_ELEMENTS_WITH_MOVE_LISTENERS
				StartPanning(ow);
		}
	}
	if (isPanning == YES)
	{
		// update mouse position
		SetPanMousePos(sp.x, sp.y);
		// generate panning actions

# ifdef ACTION_COMPOSITE_PAN_ENABLED
		if (GetPanMouseDeltaX() || GetPanMouseDeltaY())
		{
			int16 deltas[2];
			deltas[0] = GetPanDocDeltaX();
			deltas[1] = GetPanDocDeltaY();
			g_input_manager->InvokeAction(OpInputAction::ACTION_COMPOSITE_PAN, (INTPTR)deltas, 0,
										input_context, NULL, TRUE, OpInputAction::METHOD_MOUSE);
			// always consuming mouse deltas after panning has bubbled all the way up
			SetPanPerformedX();
			SetPanPerformedY();
		}
#else // ACTION_COMPOSITE_PAN_ENABLED
#  ifdef ACTION_PAN_X_ENABLED
		if (GetPanMouseDeltaX())
			if (g_input_manager->InvokeAction(OpInputAction::ACTION_PAN_X, GetPanDocDeltaX(), 0,
				input_context, NULL, TRUE, OpInputAction::METHOD_MOUSE))
				SetPanPerformedX();
#  endif // ACTION_PAN_X_ENABLED
#  ifdef ACTION_PAN_Y_ENABLED
		if (GetPanMouseDeltaY())
			if (g_input_manager->InvokeAction(OpInputAction::ACTION_PAN_Y, GetPanDocDeltaY(), 0,
				input_context, NULL, TRUE, OpInputAction::METHOD_MOUSE))
				SetPanPerformedY();
#  endif // ACTION_PAN_Y_ENABLED
# endif // ACTION_COMPOSITE_PAN_ENABLED
		return TRUE;
	}
	return FALSE;
}

BOOL VisualDevice::PanMouseUp(OpWindow* ow)
{
	if (isPanning == YES)
	{
		StopPanning(ow);
		g_widget_globals->is_down = FALSE;
		return TRUE;
	}
	// if we're in MAYBE state we want to continue, but still
	// need to set to NO
	StopPanning(ow);
	return FALSE;
}

void VisualDevice::SetPanMousePos(INT32 x, INT32 y)
{
	OP_ASSERT(isPanning != NO);
	panNewX = x;
	panNewY = y;
}

INT32 VisualDevice::GetPanMouseAccX()
{
	OP_ASSERT(IsPanning());
	return rdx >> FEATURE_PAN_FIX_DECBITS;
}
INT32 VisualDevice::GetPanMouseAccY()
{
	OP_ASSERT(IsPanning());
	return rdy >> FEATURE_PAN_FIX_DECBITS;
}

INT32 VisualDevice::GetPanDocDeltaX()
{
	OP_ASSERT(IsPanning());
	// difference between mouse pos and the mouse pos of last performed pan
	INT32 mdx = GetPanMouseDeltaX();
	// accumulated difference between actual mouse deltas and used deltas
	INT32 edx = GetPanMouseAccX();
	mdx += edx;
	// this value is used for panning
	INT32 dx = ScaleToDoc(mdx);
	// actual used deltas in mouse coords, fixpoint
	INT32 adx = ScaleToScreen(dx << FEATURE_PAN_FIX_DECBITS);
	// remove any used (accumulated) loss
	crdx = rdx - (edx << FEATURE_PAN_FIX_DECBITS);
	// add the loss in mouse coords, fixpoint
	crdx += (mdx << FEATURE_PAN_FIX_DECBITS) - adx;
	return dx;
}
INT32 VisualDevice::GetPanDocDeltaY()
{
	OP_ASSERT(IsPanning());
	// difference between mouse pos and the mouse pos of last performed pan
	INT32 mdy = GetPanMouseDeltaY();
	// accumulated difference between actual mouse deltas and used deltas
	INT32 edy = GetPanMouseAccY();
	mdy += edy;
	// this value is used for panning
	INT32 dy = ScaleToDoc(mdy);
	// actual used deltas in mouse coords, fixpoint
	INT32 ady = ScaleToScreen(dy << FEATURE_PAN_FIX_DECBITS);
	// remove any used (accumulated) loss
	crdy = rdy - (edy << FEATURE_PAN_FIX_DECBITS);
	// add the loss in mouse coords, fixpoint
	crdy += (mdy << FEATURE_PAN_FIX_DECBITS) - ady;
	return dy;
}

INT32 VisualDevice::GetPanMouseDeltaX()
{
	OP_ASSERT(isPanning != NO);
	return panLastX - panNewX;
}
INT32 VisualDevice::GetPanMouseDeltaY()
{
	OP_ASSERT(isPanning != NO);
	return panLastY - panNewY;
}

void VisualDevice::SetPanPerformedX()
{
	OP_ASSERT(IsPanning());
	panLastX = panNewX;
	rdx = crdx;
}
void VisualDevice::SetPanPerformedY()
{
	OP_ASSERT(IsPanning());
	panLastY = panNewY;
	rdy = crdy;
}
#endif // PAN_SUPPORT

void VisualDevice::UpdateAll()
{
	if (!GetVisible())
		return;

	m_update_all = TRUE;

	if (IsLocked())
		return;

	SyncDelayedUpdates();
}

OpRect VisualDevice::VisibleRect()
{
	OpRect rect;
	if (v_on && h_scroll)
		rect.x = h_scroll->GetRect().x; // Use h_scroll x because it is positioned according to LeftHandScrollbar() already.
	rect.y = 0;
	rect.height = VisibleHeight();
	rect.width = VisibleWidth();
	return rect;
}

OpRect VisualDevice::GetVisibleDocumentRect()
{
	FramesDocument *doc = doc_manager->GetCurrentVisibleDoc();

	if (doc->IsTopDocument())
		return doc->GetVisualViewport();
	else
	{
		// Getting which part of the screen is occupied by visual viewport
		FramesDocument *top_doc = doc->GetTopDocument();
		VisualDevice *top_device = top_doc->GetVisualDevice();
		OpRect top_visual_viewport = top_doc->GetVisualViewport();
		OpRect rect = view->GetScreenRect();
		OpPoint top_doc_screen_offset = top_device->view->ConvertToScreen(OpPoint(0, 0));

		if (rect.IsEmpty())
			return rect;

		// make it relative to the rendering viewport
		top_visual_viewport.x = top_visual_viewport.x - top_device->GetRenderingViewX();
		top_visual_viewport.y = top_visual_viewport.y - top_device->GetRenderingViewY();

		// visual viewport as the part of the screen
		top_visual_viewport = ScaleToScreen(top_visual_viewport);
		top_visual_viewport.OffsetBy(top_doc_screen_offset);

		rect.IntersectWith(top_visual_viewport);

		if(rect.IsEmpty())
			return rect;

		rect = view->ConvertFromScreen(rect);
#ifdef CSS_TRANSFORMS
		int w, h;
		view->GetSize(&w, &h);
		/** It is possible that the rect, converted from screen back again, exceeds the view rect.
			That's because when transforms are present we take bboxes which enlarge the original rect.
			Take only the part that is inside the view. */
		rect.IntersectWith(OpRect(0, 0, w, h));
		if (rect.IsEmpty())
			return rect;
#endif // CSS_TRANSFORMS
		rect = ScaleToDoc(rect);
		rect.OffsetBy(rendering_viewport.x, rendering_viewport.y); // not top doc so rendering viewport = visual viewport
		return rect;
	}
}

OpRect VisualDevice::GetDocumentInnerViewClipRect()
{
	FramesDocument* doc = doc_manager->GetCurrentVisibleDoc();
	FramesDocument* top_doc = doc->GetTopDocument();
	int frames_policy = top_doc->GetFramesPolicy();

	if (frames_policy == FRAMES_POLICY_DEFAULT)
	{
		frames_policy = GetWindow() ? GetWindow()->GetFramesPolicy() : FRAMES_POLICY_DEFAULT;

		if (frames_policy == FRAMES_POLICY_DEFAULT)
			// frames_policy_normal is more limiting, so choosing this if we still have it unknown
			frames_policy = FRAMES_POLICY_NORMAL;
	}

	/* If this condition is TRUE we expect this method to return the maximum possible rectangle, because
	   we don't need to have any clip rect at all for this doc. This happens either if this is a top document
	   or a frame in top frameset when we have one of the 'expand frames' policy - smart or stacking. */
	if (doc->IsTopDocument() ||
		(frames_policy != FRAMES_POLICY_NORMAL && top_doc == doc->GetTopFramesDoc()))
	{
		int negative_overflow = doc->NegativeOverflow();
		return OpRect(-negative_overflow, 0, doc->Width() + negative_overflow, doc->Height());
	}

	/* Get the visible rect of the view, but if the frames policy is different than normal
	   or we don't have a top frameset, we don't have to intersect with the first frame's (top doc's) view's rect,
	   because either the frames are expanded to the document size so we can't paint over the other frame
	   or it's top doc so we don't care about painting over the screen. */
	OpRect frame_rect = view->GetVisibleRect(!(frames_policy == FRAMES_POLICY_NORMAL && top_doc->IsFrameDoc()));
	AffinePos pos;

	view->GetTransformFromContainer(pos);
	pos.Apply(frame_rect);

#ifdef CSS_TRANSFORMS
	int w, h;
	view->GetSize(&w, &h);
	/** It is possible that the frame_rect, converted from screen back again, exceeds the view rect.
		That's because when transforms are present we take bboxes which enlarge the original rect.
		Take only the part that is inside the view. */
	frame_rect.IntersectWith(OpRect(0, 0, w, h));
	if (frame_rect.IsEmpty())
		return frame_rect;
#endif // CSS_TRANSFORMS

	frame_rect = ScaleToDoc(frame_rect);
	frame_rect.OffsetBy(rendering_viewport.x, rendering_viewport.y);

	return frame_rect;
}

void VisualDevice::UpdatePopupProgress()
{
#ifdef WIC_USE_VIS_RECT_CHANGE
	if (IsLocked())
		return;

	Window* window = GetWindow();
	if (window && window->GetWindowCommander() && this == window->VisualDev())
	{
		// Since it is possible to have a visual-device without
		// associated DocumentManager - and thereby also no window..
		WindowCommander* commander = window->GetWindowCommander();
		if (commander->GetDocumentListener())
			commander->GetDocumentListener()->OnVisibleRectChanged(commander, VisibleRect());
	}
#endif // WIC_USE_VIS_RECT_CHANGE
}

void VisualDevice::LockUpdate(BOOL lock)
{
	if (lock)
	{
		m_lock_count++;
	}
	else if (m_lock_count > 0)
	{
		m_lock_count--;

		if (m_lock_count == 0)
		{
			// we're being unlocked.. sync invalidates
			SyncDelayedUpdates();

			UpdatePopupProgress();

			if (doc_manager)
				doc_manager->CheckOnNewPageReady();
		}
	}

	// also lock container visual device

	if (container && container->GetVisualDevice())
	{
		container->GetVisualDevice()->LockUpdate(lock);
	}

	// let view know about lock too, maybe it does something even more lowlevel,
	// like ignoring "within-view" damage caused by child iframes views being
	// sized/moved etc.

	if (view)
	{
		view->LockUpdate(lock);
	}
}

void VisualDevice::HideIfFrame()
{
	FramesDocument* pdoc = doc_manager ? doc_manager->GetParentDoc() : NULL;
	if (pdoc && pdoc->IsFrameDoc() && !m_hidden_by_lock)
	{
		// If this is a frame in a frameset, we hide it. Since the frameset is usually loaded and unlocked
		// quite fast we would otherwise show the frameset border but old garbage in the (locked) frames.
		// Hiding the frames makes the space blank since the frameset clears background.
		m_hidden_by_lock = TRUE;
		GetContainerView()->SetVisibility(!m_hidden && !m_hidden_by_lock);
	}
}

void VisualDevice::UnhideIfFrame()
{
	FramesDocument* pdoc = doc_manager ? doc_manager->GetParentDoc() : NULL;
	if (pdoc && pdoc->IsFrameDoc() && m_hidden_by_lock)
	{
		// Show frame again if it was hidden when it was locked.
		m_hidden_by_lock = FALSE;
		GetContainerView()->SetVisibility(!m_hidden && !m_hidden_by_lock);
	}
}

void VisualDevice::Update(int x, int y, int iwidth, int iheight, BOOL timed)
{
	if (!GetVisible())
		return;

	OpRect rect(x, y, iwidth, iheight);

	if (m_include_highlight_in_updates)
		PadRectWithHighlight(rect);

	if (rect.IsEmpty())
		return;

	if (rendering_viewport.width && rendering_viewport.height)
		rect.SafeIntersectWith(rendering_viewport);

	if (rect.IsEmpty())
		return;

	if (painter && timed) // We are painting and get Update from layout engine.
	{
		// During paint something might Update new areas. That is allright, but we know that the area we are currently
		// painting is fine when we are done, so we can ignore Updates which doc_display_rect_not_enlarged contains.
		if (doc_display_rect_not_enlarged.Contains(rect))
			return;
	}

	m_update_rect.UnionWith(rect);

	if (IsLocked())
		return;

	/*uni_char tmp[300]; // ARRAY OK 2009-06-01 wonko
	uni_snprintf(tmp, 300, L"Update %d %d %d %d  timed: %d\n", rect.x, rect.y, rect.width, rect.height, (UINT32)timed);
	OutputDebugString(tmp);*/

	if (timed && doc_manager && doc_manager->GetCurrentDoc() && !doc_manager->GetCurrentDoc()->IsLoaded(FALSE) && GetWindow()->GetType() != WIN_TYPE_IM_VIEW)
		if (StartTimer(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::UpdateDelay)))
			return;

	SyncDelayedUpdates();
}

void VisualDevice::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_VISDEV_UPDATE)
	{
		m_posted_msg_update = FALSE;
		OnTimeOut();
	}
#ifndef MOUSELESS
	if (msg == MSG_VISDEV_EMULATE_MOUSEMOVE)
	{
		m_posted_msg_mousemove = FALSE;
		if (OpWidget::GetFocused() && OpWidget::GetFocused()->IsForm())
		{
			OpWidget::GetFocused()->GetFormObject()->UpdatePosition();
		}

		OP_ASSERT (g_main_message_handler->HasCallBack(this, MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this));
		g_main_message_handler->UnsetCallBack(this, MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this);

		// Emulate a new mousemove to get possible hovereffects/highlighting/menus in documents to update.
		if (view)
		{
			CoreViewContainer* c_view = view->GetContainer();
			if (!c_view->GetCapturedView())
			{
				// See if mouse are over this view or any childview.
				CoreView *tmp = c_view->GetHoverView();
				while (tmp && tmp != view)
					tmp = tmp->GetParent();
				if (tmp)
				{
					OpPoint mpos;
					c_view->GetMousePos(&mpos.x, &mpos.y);
					c_view->MouseMove(mpos, SHIFTKEY_NONE);
				}
			}
		}
	}
#endif
}

/* virtual */ void VisualDevice::OnTimeOut()
{
	StopTimer();
	if (IsLocked())
	{
		if (m_lock_count == 1)
			UnhideIfFrame();

		LockUpdate(FALSE);
	}
	else
		SyncDelayedUpdates();
}

void VisualDevice::RemoveMsgUpdate()
{
	if (m_posted_msg_update)
	{
		g_main_message_handler->RemoveFirstDelayedMessage(MSG_VISDEV_UPDATE, (MH_PARAM_1)this, 0);
		m_posted_msg_update = FALSE;
	}
}

void VisualDevice::RemoveMsgMouseMove()
{
	if (m_posted_msg_mousemove)
	{
		g_main_message_handler->RemoveFirstDelayedMessage(MSG_VISDEV_EMULATE_MOUSEMOVE, (MH_PARAM_1)this, 0);
		m_posted_msg_mousemove = FALSE;
	}
}

void VisualDevice::SyncDelayedUpdates()
{
	if (!view)
		return;

	if (m_update_all)
	{
		view->Invalidate(OpRect(0, 0, WinWidth(), WinHeight()));
	}
	else if (!m_update_rect.IsEmpty())
	{
		EnlargeWithIntersectingOutlines(m_update_rect);

		m_update_rect = ScaleToScreen(m_update_rect);
		m_update_rect.x -= view_x_scaled;
		m_update_rect.y -= view_y_scaled;

		view->Invalidate(m_update_rect);
	}

	m_update_all = FALSE;
	m_update_rect.Empty();
}

BOOL VisualDevice::IsPaintingToScreen()
{
	if (view && view->GetContainer())
		return view->GetContainer()->IsPainting();
	return FALSE;
}

void VisualDevice::SetFixedPositionSubtree(HTML_Element* fixed)
{
	if (GetContainerView())
		GetContainerView()->SetFixedPositionSubtree(fixed);
}

void VisualDevice::ForcePendingPaintRect(const OpRect& rect)
{
	Update(rect.x, rect.y, rect.width, rect.height);

	if (pending_paint_rect.IsEmpty())
		pending_paint_rect = rect;
	else
		pending_paint_rect.UnionWith(rect);
}

// Take area in (rendering) viewport coordinates, and transform
// into screen coordinates. Then convert to screen coordinates
// relative to the container of this VD.
void VisualDevice::PaintIFrame(OpRect view_rect, VisualDevice* vis_dev)
{
	OpRect screen_rect = vis_dev->ScaleToScreen(view_rect);

	// Translate the update area to the iframes position.
	AffinePos view_ctm;
	GetContainerView()->GetPos(&view_ctm);
	view_ctm.ApplyInverse(screen_rect);

	Paint(screen_rect, vis_dev);
}

void VisualDevice::Paint(OpRect rect, VisualDevice* vis_dev)
{
	VisualDeviceBackBuffer* bb = (VisualDeviceBackBuffer*) vis_dev->backbuffers.First();
	if (bb && bb->GetBitmap())
	{
		if (doc_manager)
			if (FramesDocument* doc = doc_manager->GetCurrentVisibleDoc())
				if (LogicalDocument* logdoc = doc->GetLogicalDocument())
					if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
					{
						// Set bg to fixed so this view gets a full update when scrolling.

						/* If the VisualDevice painting this VisualDevice has a backbuffer, it's
						   likely it's opacity and then the whole VisualDevice has to repaint on
						   scroll instead of just move the area (since the underlaying VisualDevice
						   should be seen as fixed content). */

						workplace->SetBgFixed();
					}
	}

	int parent_offset_x, parent_offset_y;

#ifdef CSS_TRANSFORMS
	OP_ASSERT(!HasTransform());

	// Override the 'base transform' with the transform from the
	// VisualDevice that is passed in, then set the offset_{x,y} to
	// 0. This should then (hopefully) take the form of an 'offset
	// transform' which replaces the offset.
	AffinePos parent_ctm = vis_dev->GetCTM();
	if (parent_ctm.IsTransform())
	{
		parent_offset_x = parent_offset_y = 0;

		// Clear current transform on painter
		VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(vis_dev->painter);
		vpainter->ClearTransform();

		offset_transform = vis_dev->GetBaseTransform();
		offset_transform.PostMultiply(parent_ctm.GetTransform());
		offset_transform_is_set = true;

		AffineTransform identity;
		identity.LoadIdentity();

		// Enter transformed state
		RETURN_VOID_IF_ERROR(PushTransform(identity)); // FIXME: OOM(?)
	}
	else
#endif // CSS_TRANSFORMS
	{
		parent_offset_x = vis_dev->offset_x_ex;
		parent_offset_y = vis_dev->offset_y_ex;
	}

	// Translate offset with parentvisualdevices offsets. This
	// will make offset correct relative to any backbuffers for
	// opacity etc. (VisualDeviceBackBuffer)
	offset_x += parent_offset_x;
	offset_y += parent_offset_y;
	offset_x_ex += parent_offset_x;
	offset_y_ex += parent_offset_y;

	// Store old color and fontatt and restore it afterwards. Paint might change it.
	COLORREF old_col = vis_dev->GetColor();
	FontAtt old_fontatt;
	old_fontatt = vis_dev->logfont;
	int old_font_number = vis_dev->current_font_number;

	CoreView *container_view = container->GetView();

	// If this view is clipped, the flag must be set on the CoreView so it can do more
	// expensive intersection checks and scroll instead of just bounding box check.
	// Use the document based clipping rectangles instead of the painters actual cliprect
	// so we are sure this check works even when there is partial repaints. Platform forced clipping should not affect.
	BOOL is_clipped = FALSE;
	OpRect current_clip_rect;
	if (vis_dev->bg_cs.GetClipping(current_clip_rect))
	{
		// Express current clip in screen coordinates
		current_clip_rect = vis_dev->OffsetToContainerAndScroll(vis_dev->ScaleToScreen(current_clip_rect));

		// Get extents for this VD (Use GetTransformToContainer here perhaps?)
		OpRect clip_rect;
		container_view->GetSize(&clip_rect.width, &clip_rect.height);

		container_view->ConvertToContainer(clip_rect.x, clip_rect.y);
		clip_rect.OffsetBy(vis_dev->offset_x_ex, vis_dev->offset_y_ex);

		if (!current_clip_rect.Contains(clip_rect))
			is_clipped = TRUE;
	}
	container_view->SetIsClipped(is_clipped);

	// Finally, Paint
	container_view->Paint(rect, vis_dev->painter, parent_offset_x, parent_offset_y);

	vis_dev->current_font_number = old_font_number;
	vis_dev->logfont = old_fontatt;
	vis_dev->logfont.SetChanged();
	vis_dev->SetColor(old_col);

	// Translate offset back to original.
	offset_x_ex -= parent_offset_x;
	offset_y_ex -= parent_offset_y;
	offset_x -= parent_offset_x;
	offset_y -= parent_offset_y;

#ifdef CSS_TRANSFORMS
	if (parent_ctm.IsTransform())
	{
		// Leave transformed state
		PopTransform();

		// Reset offset transform
		offset_transform_is_set = false;

		// Reset transform on parent VD
		vis_dev->UpdatePainterTransform(*vis_dev->GetCurrentTransform());
	}

	OP_ASSERT(!HasTransform());
#endif // CSS_TRANSFORMS
}

void VisualDevice::OnPaint(OpRect rect, OpPainter* painter)
{
	OP_PROBE6(OP_PROBE_PAINTLISTENER_ONPAINT);

	if (!doc_manager)
		return;
	FramesDocument* doc;
#ifdef _PRINT_SUPPORT_
	if (IsPrinter() || GetWindow() && GetWindow()->GetPreviewMode())
		doc = doc_manager->GetPrintDoc();
	else
#endif // _PRINT_SUPPORT_
		doc = doc_manager->GetCurrentDoc();

	UpdateOffset();

#ifdef _PRINT_SUPPORT_
	if (!IsPrinter()) // The PrintDevice does not have a CoreView.
#endif
	{
		OP_ASSERT(view);
	}

	/*uni_char tmp[300]; // ARRAY OK 2009-06-01 wonko
	uni_snprintf(tmp, 300, L"VisualDevice::OnPaint %p    %d %d %d %d\n", this, rect.x, rect.y, rect.width, rect.height);
	OutputDebugString(tmp);*/

	if (doc)
	{
		SetPainter(painter);

		logfont.SetChanged();

		Reset();

#ifdef CSS_TRANSFORMS
		if (HasTransform())
			this->UpdatePainterTransform(*GetCurrentTransform());
#endif // CSS_TRANSFORMS

		// Calculate doc_display_rect and translate it to document coordinates
		doc_display_rect = ScaleToEnclosingDoc(rect);
		doc_display_rect.OffsetBy(rendering_viewport.x, rendering_viewport.y);

		if (doc_display_rect.Contains(rendering_viewport))
		{
			// We are doing a full update and know that the fixed-flags will be updated again, if the fixed content is still in view.
			if (LogicalDocument* logdoc = doc->GetLogicalDocument())
				if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
					workplace->ResetFixedContent();

		}

		// Keep a nonmodified doc_display_rect in document coordinates.
		doc_display_rect_not_enlarged = doc_display_rect;

		m_outlines_enabled = TRUE;

		// Remove outlines that is not in view anymore.
		RemoveIntersectingOutlines(rendering_viewport, FALSE);

		// Extend the area we need to display with intersecting outlines.
		// As long as we extend the display rect, we might intersect more outlines.
		while (EnlargeWithIntersectingOutlines(doc_display_rect))
			/* continue while TRUE */;

		// Extend display rect with areas that must be included. See ForcePendingPaintRect.
		if (!pending_paint_rect_onbeforepaint.IsEmpty())
		{
			doc_display_rect.UnionWith(pending_paint_rect_onbeforepaint);
			// Empty pending_paint_rect. pending_paint_rect_onbeforepaint will still be valid until next OnBeforePaint.
			pending_paint_rect.Empty();
		}

		/*uni_char tmp[300]; // ARRAY OK 2009-06-01 emil
		uni_snprintf(tmp, 300, L"     The enlarged rect is %d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
		OutputDebugString(tmp);*/

		// Now we can remove the outlines in the displayrect. They will be added during display if they are still there.
		RemoveIntersectingOutlines(doc_display_rect, TRUE);

		// We are only interested in paiting inside the actual viewport.
		doc_display_rect.IntersectWith(OpRect(rendering_viewport.x, rendering_viewport.y, ScaleToDoc(VisibleWidth()), ScaleToDoc(VisibleHeight())));

		// Remove scrolling for rect sent to doc->Display.
		OpRect doc_display_rect_with_scroll(doc_display_rect);
		doc_display_rect_with_scroll.OffsetBy(-rendering_viewport.x, -rendering_viewport.y);

#ifdef _DEBUG
		//OpRect debugScreenRect = ScaleToScreen(doc_display_rect_with_scroll);
		//OP_ASSERT(debugScreenRect.Contains(rect)); // If this triggers, then we are losing pixels, that won't be painted.
		// The ScaleToEnclosingDoc needs to make sure that the doc_display_rect is big enough to contain the full changed screen area.
#endif // _DEBUG

		BOOL oom = FALSE;

		if (OpStatus::IsSuccess(bg_cs.Begin(this)))
		{
			RECT winrect = doc_display_rect_with_scroll;

			// We should stop using RECT and use OpRect here instead!
			if (doc->Display(winrect, this) == OpStatus::ERR_NO_MEMORY)
				oom = TRUE;

			bg_cs.End(this);

			DrawOutlines();
		}
		else
			oom = TRUE;

		if (oom)
		{
			// Always paint something, makes sure no old garbage is left on the screen in oom
			UINT32 col = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND);
			painter->SetColor(col & 0xff, // red
				(col >> 8) & 0xff, // green
				(col >> 16) & 0xff); // blue
			painter->FillRect(rect);

			GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}

		UpdateAddedOrChangedOutlines(doc_display_rect);
		RemoveAllTemporaryOutlines();
		m_outlines_enabled = FALSE;

#ifdef GADGET_SUPPORT
		if(GetWindow()->GetType() == WIN_TYPE_GADGET)
		{
			WindowCommander* commander = GetWindow()->GetWindowCommander();
			commander->GetDocumentListener()->OnGadgetPaint(commander, this);
		}
#endif

#ifdef FEATURE_SCROLL_MARKER
	if (m_scroll_marker)
		m_scroll_marker->OnPaint(painter, rect);
#endif //FEATURE_SCROLL_MARKER

#ifdef DEBUG_PAINT_LAYOUT_VIEWPORT_RECT
		if (doc)
		{
			OpRect layout_viewport = doc->GetLayoutViewport();
			SetColor(0xff, 0xff, 0, 0x11);
			FillRect(OpRect(layout_viewport));
			SetColor(0xbb, 0, 0, 0x33);
			DrawRect(OpRect(layout_viewport));
		}
#endif // DEBUG_PAINT_VISUAL_VIEWPORT_RECT

#ifdef DEBUG_PAINT_VISUAL_VIEWPORT_RECT
		if (doc)
		{
			OpRect visual_viewport = doc->GetVisualViewport();

			SetColor(0, 0xff, 0xff, 0x33);
			FillRect(OpRect(visual_viewport));
			SetColor(0, 0xbb, 0, 0xaa);
			DrawRect(OpRect(visual_viewport));
		}
#endif // DEBUG_PAINT_VISUAL_VIEWPORT_RECT

#ifdef _PRINT_SUPPORT_
		if (!IsPrinter())
			/* We cant reset the painter here if we are printing,
			   because printing is done over several messages, clipping
			   in between. */
#endif
			SetPainter(NULL);
	}
	else
	{
		BOOL clear = TRUE;

#ifndef DISPLAY_ALWAYS_PAINT_BACKGROUND_IN_TRANSPARENT_WINDOWS
		// Don't paint anything in gadgets or transparent windows.
		if (GetWindow() && GetWindow()->IsBackgroundTransparent())
			clear = FALSE;
#endif

#ifdef GADGET_SUPPORT
		if (GetWindow() && GetWindow()->GetType() == WIN_TYPE_GADGET)
			clear = FALSE;
#endif

		if (doc_manager->GetParentDoc())
			clear = FALSE; // Don't paint background for non-top-level documents

		if (clear)
		{
			UINT32 col = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND);
			painter->SetColor(col & 0xff, // red
							  (col >> 8) & 0xff, // green
							  (col >> 16) & 0xff); // blue
			painter->FillRect(OffsetToContainer(rect));
		}
	}

#ifdef SCOPE_EXEC_SUPPORT
	//
	// The scope debug/testing functionality to immediately report
	// back when a specific content is painted needs to be told when
	// the topmost Visual device gets painted so it can inspect the
	// new contents right away.
	//
	// This functionality is used to significantly speed up automated
	// tests where a reference image is used to decide "PASS".
	//
	// The 'WindowPainted()' method is light-weight when the
	// functionality is dormant. When active, the method may post
	// a message to trigger a new paint operation into a bitmap
	// at some later time, which will then be inspected.
	//
#ifdef _PRINT_SUPPORT_
	if (!GetWindow() || !GetWindow()->GetPreviewMode())
#endif // _PRINT_SUPPORT_
	if ( doc_manager->GetParentDoc() == 0 )
	{
		// We are only interested in the root document
		if ( GetWindow() != 0 )
			OpScopeDisplayListener::OnWindowPainted(GetWindow(), rect);
	}
#endif // SCOPE_EXEC_SUPPORT
}

void VisualDevice::GetTxtExtentSplit(const uni_char *txt, unsigned int len, unsigned int max_width, int text_format, unsigned int &result_width, unsigned int &result_char_count)
{
	result_width = 0;
	result_char_count = 0;

	// Pre-check
	if (max_width == 0)
		return;
	int test_width = GetTxtExtentEx(txt, len, text_format);
	if (test_width <= int(max_width))
	{
		result_width = test_width;
		result_char_count = len;
		return;
	}

	int ave_char_w = GetFontAveCharWidth();
	if (ave_char_w <= 0)
		ave_char_w = 8; // Give it something :)

	int low_len = 0;
	int high_len = len;
	int curr_len = max_width / ave_char_w;
	int low_width = 0;
	curr_len = MIN(curr_len, high_len);

	// don't break surrogate pairs
	if (curr_len > 0 && Unicode::IsHighSurrogate(txt[curr_len-1]))
		--curr_len;

	// Instead of binary search, we will re-estimate the test-position by a diff calculated with the average char width.
	// This will in most cases give us the correct answer in very few iterations.

	while (TRUE)
	{
		int curr_width = GetTxtExtentEx(txt, curr_len, text_format);
		if (curr_width > int(max_width))
		{
			high_len = curr_len;
			// word already too wide, need to re-check
			int estimated_diff = (curr_width - max_width) / ave_char_w;
			estimated_diff = MIN(estimated_diff, curr_len - low_len - 1);
			estimated_diff = MAX(estimated_diff, 1);
			curr_len -= estimated_diff;

			// don't break surrogate pairs
			if (curr_len > 0 && Unicode::IsHighSurrogate(txt[curr_len-1]))
			{
				--curr_len;
				if (curr_len == low_len)
				{
					// make sure we break out of a run of surrogate pairs
					result_width = low_width;
					result_char_count = low_len;
					return;
				}
			}
		}
		else
		{
			low_len = curr_len;
			low_width = curr_width;
			// word too narrow, need to re-check
			int estimated_diff = (max_width - curr_width) / ave_char_w;
			estimated_diff = MIN(estimated_diff, high_len - curr_len - 1);
			estimated_diff = MAX(estimated_diff, 1);
			curr_len += estimated_diff;

			// don't break surrogate pairs
			if (curr_len < int(len) && Unicode::IsLowSurrogate(txt[curr_len]))
			{
				++curr_len;
				if (curr_len == high_len)
				{
					// make sure we break out of a run of surrogate pairs
					result_width = low_width;
					result_char_count = low_len;
					return;
				}
			}
		}
		if (high_len - low_len <= 1)
		{
			result_width = low_width;
			result_char_count = low_len;
			return;
		}
	}
}

int VisualDevice::GetTxtExtent(const uni_char* txt, int len)
{
#ifdef _DEBUG
	DebugTimer timer(DebugTimer::FONT);
#endif

#ifdef DEBUG_VIS_DEV
	VD_PrintOutT("GetTxtExtent()", txt, len);
#endif // DEBUG_VIS_DEV
	OpStatus::Ignore(CheckFont());

	if (currentFont == NULL)
		return 0;

	if (GetFontSize() > 0 && len > 0)
	{
		// Assumption: OpFont should be able to return a string width for scale 100%
		//             without having to give it a painter.

		OpPainter* tmpPainter;

		BOOL really_need_painter = !painter && IsScaled() && view;
		if (really_need_painter)
			tmpPainter = view->GetPainter(OpRect(0,0,0,0), TRUE);
		else
			tmpPainter = painter;

		int width = 0;

		int spacing = char_spacing_extra;
		if (LayoutIn100Percent())
			spacing = LayoutScaleToScreen(spacing);
		else
		spacing = ScaleToScreen(spacing);

		width = GetStringWidth(currentFont, txt, len, tmpPainter, spacing, this);

		if (LayoutIn100Percent())
			width = LayoutScaleToDoc(width);
		else
#ifdef CSS_TRANSFORMS
			if (!HasTransform())
#endif // CSS_TRANSFORMS
				width = ScaleToDoc(width);

		if (really_need_painter && tmpPainter)
			view->ReleasePainter(OpRect(0,0,0,0));

		return width;
	}
	else
		return 0;
}

void VisualDevice::TxtOut(int x, int y, const uni_char* txt, int len, short word_width)
{
// need unicode	D.Dbg("visualdevice", txt);

	OpStatus::Ignore(CheckFont());

	if (currentFont == NULL)
		return;

	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	if (OP_GET_A_VALUE(this->color) != 255 && !painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))
	{
		INT32 scaled_char_spacing = ScaleToScreen(char_spacing_extra);
		int width = GetStringWidth(currentFont, txt, len, painter, scaled_char_spacing, this);
		COLORREF oldcol = this->color;
		if (OpStatus::IsSuccess(BeginOpacity(OpRect(x, y, ScaleToDoc(width)+1, ScaleToDoc(currentFont->Height())+1), OP_GET_A_VALUE(this->color))))
		{
			SetColor(OP_GET_R_VALUE(this->color), OP_GET_G_VALUE(this->color), OP_GET_B_VALUE(this->color));
			TxtOut(x, y, txt, len, word_width);
			SetColor(oldcol);
			EndOpacity();
			return;
		}
	}

	x += translation_x;
	y += translation_y;

	INT32 scaled_char_spacing = ToPainterExtent(char_spacing_extra);

	if (GetFontSize() > 0)
	{
		DrawString(painter, ToPainter(OpPoint(x, y)), txt, len, scaled_char_spacing, word_width);
	}
}

#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT

// the font size for the font-sizes-based-on-100%-scale-when-true-zoom-is-enabled  hack is changed here, since we need
// to have the target size when calculating the glyph and diacritic rects. thus, DrawString
// is never called, but instead painter->DrawString and DrawDiacritics.
void VisualDevice::TxtOut_Diacritics(int x, int y, const uni_char* txt, int len, BOOL isRTL)
{
	// FIXME: this does not have a fallback for rgba support

	x += translation_x;
	y += translation_y;

	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	INT32 scaled_char_spacing = ToPainterExtent(char_spacing_extra);

	if (GetFontSize() <= 0)
		return;

	BeginAccurateFontSize();
	OpStatus::Ignore(CheckFont());

	const uni_char* seg = txt;
	int seg_len = 0;
	while (seg < txt+len)
	{
		// Pointers and lengths of the textruns.
		// 0 for the text part, 1 for the diacritics.
		const uni_char* segs[2];
		int lens[2];

		// for rtl text we expect to get diacritics before text content, since the text has been transformed
		// before we get here. the reverse applies for ltr text.
		BOOL diacritics = isRTL;
		// collect text and diacritics
		for (int i = 0; i < 2; ++i)
		{
			while (seg+seg_len < txt+len)
			{
				int consumed;
				UnicodePoint up = Unicode::GetUnicodePoint(seg + seg_len, (int)((txt + len) - (seg + seg_len)), consumed);
				if (diacritics != IsKnownDiacritic(up))
					break;
				seg_len += consumed;
			}

			segs[diacritics] = seg;
			lens[diacritics] = seg_len;

			seg += seg_len;
			seg_len = 0;
			diacritics = !diacritics;
		}

		if (lens[0])
			DrawStringInternal(painter, ToPainter(OpPoint(x, y)), segs[0], lens[0], scaled_char_spacing);

		const int seg_w = GetTxtExtent(segs[0], lens[0]);

		if (lens[1])
		{
			// Find the unicode point of the character we are applying the diacritics to
			UnicodePoint on_up;
			const uni_char *on;
			int on_len;
			if (lens[0])
			{
				if (isRTL)
				{
					on = &segs[0][0];
					on_up = Unicode::GetUnicodePoint(on, lens[0], on_len);
				}
				else
				{
					on = &segs[0][lens[0] - 1];
					if (lens[0] > 1 && Unicode::IsLowSurrogate(*on) && Unicode::IsHighSurrogate(*(on - 1)))
					{
						// Include the whole surrogate pair
						on--;
						on_up = Unicode::GetUnicodePoint(on, 2, on_len);
					}
					else
					{
						on_up = *on;
						on_len = 1;
					}
				}
			}
			else
			{
				// Use fake space metrics for isolated diacritic
				// Note: We should only do this if (format_option & TEXT_FORMAT_IS_START_OF_WORD)
				// so we detect isolated diacritics across element boundaries. But we must
				// also know the preceding character to do it right.
				on = UNI_L(" ");
				on_up = 32;
				on_len = 1;
			}

			OpFont::GlyphProps glyph;
			OpStatus::Ignore(GetGlyphPropsInLocalScreenCoords(on_up, &glyph, TRUE));

			int glyph_width = glyph.width;
			int glyph_top = glyph.top, glyph_bottom = glyph.height - glyph.top;
			int ex = isRTL ? 0 : seg_w - GetTxtExtent(on, on_len);

			OpPoint p = ToPainterExtent(OpPoint(x+ex, y));
			int px = p.x;
			px += glyph.left;

			if (!lens[0])
			{
				// Use fake space metrics for isolated diacritic
				px += glyph.advance;

				if (glyph.height == 0)
				{
					// Space might be all zeroed except the advance, so
					// to get correct vertical alignment, use nbsp for that.
					OpStatus::Ignore(GetGlyphPropsInLocalScreenCoords('\xA0', &glyph, TRUE));
					glyph_top = glyph.top;
					glyph_bottom = glyph.height - glyph.top;
				}
			}

			int following_glyph_top = 0;
			int following_glyph_width = 0;
			if (segs[1] + lens[1] < txt + len)
			{
				// We have at least one character following the diacritics.
				// We need its metrics if we have double diacritics.
				// Just assume we will use the same font for that.
				uni_char char_b = *(segs[1] + lens[1]);
				OpStatus::Ignore(GetGlyphPropsInLocalScreenCoords(char_b, &glyph, TRUE));
				following_glyph_top = glyph.top;
				following_glyph_width = glyph.left + glyph.advance;
			}

			DrawDiacritics(painter, px, p.y, (uni_char*)segs[1], lens[1],
							glyph_width, glyph_top, glyph_bottom, following_glyph_top, following_glyph_width);
		}
		x += seg_w;
	}

	EndAccurateFontSize();
}
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT

#ifndef FONTSWITCHING

uni_char* PrepareText(uni_char* txt, int &len, BOOL collapse_ws)
{
	uni_char* txt_out = txt;
	if ((unsigned int)len < UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()))
	{
		int out_i = 0;
		txt_out = (uni_char*)g_memory_manager->GetTempBuf2k();

		// Variables used to remember if previous char was nbsp or space
		BOOL last_was_nbsp = FALSE,
			 last_was_space = FALSE,
			 tmp_nbsp, tmp_space;

		for (int i=0; i<len; ++i)
		{
			tmp_space = uni_html_space(txt[i]);
			tmp_nbsp = uni_nocollapse_sp(txt[i]);
			if (uni_isidsp(txt[i]))
				txt_out[out_i++] = 0x3000;
			else if (tmp_nbsp)
				txt_out[out_i++] = ' ';
			else
			{
				if (tmp_space)
				{
					if (!collapse_ws || !i || last_was_nbsp || !last_was_space)
						txt_out[out_i++] = ' ';
				}
				else
					txt_out[out_i++] = txt[i];
			}
			last_was_nbsp = tmp_nbsp;
			last_was_space = tmp_space;
		}
		len = out_i;
	}
	return txt_out;
}

void VisualDevice::TxtOutEx(int x, int y, uni_char* txt, int len, BOOL ws2space, BOOL collapse_ws, short word_width)
{
	uni_char* txt_out = txt;
	if (ws2space || collapse_ws)
		txt_out = PrepareText((uni_char*)txt, len, collapse_ws);

	TxtOut(x, y, txt_out, len, word_width);
}

int VisualDevice::GetTxtExtentEx(const uni_char* txt, int len, BOOL ws2space, BOOL collapse_ws)
{
	const uni_char* txt_out = txt;
	if (ws2space || collapse_ws)
		txt_out = PrepareText((uni_char*)txt, len, collapse_ws);

	return GetTxtExtent(txt_out, len);
}

#endif // !FONTSWITCHING

//#endif

#ifdef STRIP_ZERO_WIDTH_CHARS
BOOL IsZeroWidthChar(uni_char ch)
{
	if (ch >= 0x200b && ch <= 0x200d ||
		ch == 0xfeff)
		return TRUE;
	return FALSE;
}
#endif // STRIP_ZERO_WIDTH_CHARS
uni_char*
VisualDevice::TransformText(const uni_char* txt, uni_char* txt_out, size_t &len, int format_option, INT32 spacing)
{
#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
	// we assume that we can decide if text shaping is needed on the first and last character only.
	// this is true as long as
	// * we don't support western ligatures
	// * wherever devanagari (dependent) vowel sign i occurs in a string, it's at the end of the string
	if (TextShaper::NeedsTransformation(txt, len))
	{
		uni_char *converted;
		int converted_len;
		if (OpStatus::IsSuccess(TextShaper::Prepare(txt, len, converted, converted_len))) // FIXME: OOM
		{
			OP_ASSERT(converted_len <= static_cast<int>(len));
			txt = converted;
			len = converted_len;
		}
	}
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

	unsigned int out_i = 0;

	for (unsigned int i = 0; i < len; ++i)
	{
#ifdef SUPPORT_TEXT_DIRECTION
		BidiCategory bidi_type = Unicode::GetBidiCategory(txt[i]);
		if (bidi_type == BIDI_LRE || bidi_type == BIDI_LRO || bidi_type == BIDI_RLE || bidi_type == BIDI_RLO || bidi_type == BIDI_PDF ||
			txt[i] == 0x200F || txt[i] == 0x200E)
				continue;
#endif // SUPPORT_TEXT_DIRECTION

#ifdef STRIP_ZERO_WIDTH_CHARS
		if (IsZeroWidthChar(txt[i]))
			continue;
#endif // STRIP_ZERO_WIDTH_CHARS

#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT
		if ((format_option & TEXT_FORMAT_REMOVE_DIACRITICS) && IsKnownDiacritic(txt[i]))
		{
			// Do nothing so we skip diacritics

			// Except if it's a isolated diacritic without preceding content. Normally that would be a space, so fake one to give it some space.
			if (i == 0 && len == 1)
				txt_out[out_i++] = ' ';
		}
		else
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT
		if (txt[i] == 0x00A0) // A non breaking space with a width that is the same as a normal space.
		{
			//if (format_option & TEXT_FORMAT_REPLACE_NON_BREAKING_SPACE)
			txt_out[out_i++] = ' ';
		}
		else
		{
			txt_out[out_i] = txt[i];

			if (format_option & TEXT_FORMAT_CAPITALIZE)
			{
				if ((out_i == 0 && (format_option & TEXT_FORMAT_IS_START_OF_WORD)) ||
					(out_i > 0 && uni_isspace(txt_out[out_i-1])))
					txt_out[out_i] = Unicode::ToUpper(txt_out[out_i]);

			}

			if (format_option & TEXT_FORMAT_UPPERCASE)
				txt_out[out_i] = Unicode::ToUpper(txt_out[out_i]);
			else if (format_option & TEXT_FORMAT_LOWERCASE)
				txt_out[out_i] = Unicode::ToLower(txt_out[out_i]);

			out_i++;
		}
	}
	OP_ASSERT(out_i <= len);
	len = out_i;

#ifdef SUPPORT_TEXT_DIRECTION

#if defined(MSWIN) || defined(_MACINTOSH_) || defined(WINCE)

# ifdef MSWIN
	// This is a hack to handle MSWIN because Bill wants to turn words with rtl words around himself, very annoying...

	// (julienp) In WinXP SP2, words are not turned around automatically if any character spacing is set
	if (GdiOpFontManager::IsLoaded() && !(spacing != 0 && GetWinType() == WINXP && GetServicePackMajor() == 2))
#endif // MSWIN
	{
		BidiCategory bidi_type = Unicode::GetBidiCategory(txt[0]);

		if (format_option & TEXT_FORMAT_REVERSE_WORD)
		{
			if (bidi_type == BIDI_R || bidi_type == BIDI_AL)
				 // reverse if we have a rtl word that is not rtl chars

				format_option &= ~TEXT_FORMAT_REVERSE_WORD;
		}
		else
			if (bidi_type == BIDI_R || bidi_type == BIDI_AL)
				 // reverse if we have a ltr word that is not ltr chars

				format_option |= TEXT_FORMAT_REVERSE_WORD;

	}
# endif // MSWIN, _MACINTOSH_, WINCE

	// FIXME: consider interaction with other formatting options more carefully

	if (format_option & TEXT_FORMAT_REVERSE_WORD)
	{
		if (format_option & TEXT_FORMAT_BIDI_PRESERVE_ORDER)
		{
			for (unsigned int ix = 0; ix < len; ix++)
				// mirror characters

				if (Unicode::IsMirrored(txt_out[ix]))
					txt_out[ix] = Unicode::GetMirrorChar(txt_out[ix]);
		}
		else
		{
			// replace unpaired surrogates with the replacement character
			for (size_t i = 0; i < len; ++i)
			{
				if (Unicode::IsSurrogate(txt_out[i]))
				{
					if (Unicode::IsHighSurrogate(txt_out[i]))
					{
						if (i == len - 1 || !Unicode::IsLowSurrogate(txt_out[i + 1]))
							txt_out[i] = NOT_A_CHARACTER; // unpaired high
						else
							++ i;
					}
					else
						txt_out[i] = NOT_A_CHARACTER; // unpaired low
				}
			}

			size_t half_way = len / 2;

			for (size_t ix = 0; ix < half_way; ix++)
			{
				uni_char tmp = txt_out[ix];
				size_t ix2 = len - ix - 1;

				OP_ASSERT(static_cast<int>(ix2) >= 0 && ix2 < len);
				txt_out[ix] = txt_out[ix2];
				txt_out[ix2] = tmp;

				// restore order of surrogate pairs
				if (ix && Unicode::IsHighSurrogate(txt_out[ix]))
				{
					tmp = txt_out[ix];
					txt_out[ix] = txt_out[ix-1];
					txt_out[ix-1] = tmp;
				}
				if (ix2+1 < len && Unicode::IsLowSurrogate(txt_out[ix2]))
				{
					tmp = txt_out[ix2];
					txt_out[ix2] = txt_out[ix2+1];
					txt_out[ix2+1] = tmp;
				}

				// mirror characters
				if (Unicode::IsMirrored(txt_out[ix]))
					txt_out[ix] = Unicode::GetMirrorChar(txt_out[ix]);

				if (Unicode::IsMirrored(txt_out[ix2]))
					txt_out[ix2] = Unicode::GetMirrorChar(txt_out[ix2]);
			}

			if (len > 1)
			{
				// restore order of surrogate pair
				const size_t mid = half_way + (len % 2);
				OP_ASSERT(mid < len);
				if (Unicode::IsSurrogate(txt_out[mid]))
				{
					const int dx = Unicode::IsLowSurrogate(txt_out[mid]) ? 1 : -1;
					if ((int)mid + dx >= 0 && mid + dx < len)
					{
						uni_char tmp = txt_out[mid];
						txt_out[mid] = txt_out[mid+dx];
						txt_out[mid+dx] = tmp;
					}
				}
			}

			// also mirror middle character
			if (len % 2 == 1)
				if (Unicode::IsMirrored(txt_out[half_way]))
					txt_out[half_way] = Unicode::GetMirrorChar(txt_out[half_way]);
		}
	}

#endif // SUPPORT_TEXT_DIRECTION

	return txt_out;
}

static size_t get_caps_segment_length(const uni_char* str, size_t len)
{
	size_t count = 1;
	while (len > 1 && *str && *(str+1) && uni_isupper(*str) == uni_isupper(*(str+1)))
	{
		len--;
		str++;
		count++;
	}

	return count;
}

int VisualDevice::TxtOutSmallCaps(int x, int y, uni_char* txt, size_t len, BOOL draw/* = TRUE*/, short word_width/* = -1*/)
{
	// this function isn't designed to be called when accurate font size is on
	OP_ASSERT(!accurate_font_size);

	int cap_size = GetFontSize();
	int smallcap_size = (int)(cap_size * 0.8);

	// ascent delta
	int smallcap_delta = 0;

	// get the true delta between cap and smallcap ascent, using the
	// target font - i.e. in rendering scale
	if (draw)
	{
		BeginAccurateFontSize();

		CheckFont(); // need to call before BeginNoScalePainting or wrong font might be used
		VDStateNoScale s = BeginNoScalePainting(OpRect(0,0,0,0));
		smallcap_delta = GetFontAscent();
		EndNoScalePainting(s);

		SetFontSize(smallcap_size);
		CheckFont();
		s = BeginNoScalePainting(OpRect(0,0,0,0));
		smallcap_delta -= GetFontAscent();
		EndNoScalePainting(s);

		EndAccurateFontSize();
	}

	int dx = 0;
	BOOL is_upper = uni_isupper(*txt);
	while (len)
	{
		// find next place where uppercase switches to lowercase or vice versa
		size_t segment_len = get_caps_segment_length(txt, len);
		len -= segment_len;

		int y_adjust;
		if (is_upper)
		{
			SetFontSize(cap_size);
			y_adjust = 0;
		}
		else
		{
			SetFontSize(smallcap_size);
			y_adjust = smallcap_delta;

			// convert lowercase to uppercase
			for (int j = segment_len; j; --j)
			{
				txt[j - 1] = Unicode::ToUpper(txt[j - 1]);
			}
		}
		is_upper = !is_upper;

		// measure string in document coords
		int seg_w = GetTxtExtent(txt, segment_len);

		// draw directly in rendering scale, to avoid rounding errors
		if (draw)
		{
			int actual = word_width == -1 ? -1 : ScaleToScreen(seg_w);

			BeginAccurateFontSize();

			CheckFont();
			VDStateNoScale s = BeginNoScalePainting(OpRect(x + dx, y, seg_w, currentFont->Ascent() + currentFont->Descent()));
			TxtOut(s.dst_rect.x, s.dst_rect.y + y_adjust, txt, segment_len, actual);
			EndNoScalePainting(s);

			EndAccurateFontSize();
		}

		dx += seg_w;
		txt += segment_len;
	}

	// restore font size
	SetFontSize(cap_size);
	return dx;
}

void VisualDevice::TxtOutEx(int x, int y, const uni_char* txt, size_t len, int format_option, short word_width)
{
	OpStatus::Ignore(CheckFont());

	// Handle arbitrary length strings, but use the tempbuffer if it is
	// big enough.

	uni_char *txt_out = NULL, *allocated_string = NULL;
	if (len <= UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()))
	{
		txt_out = (uni_char *) g_memory_manager->GetTempBuf2k();
	}
	else
	{
		txt_out = OP_NEWA(uni_char, len);
		allocated_string = txt_out;
		if (!txt_out)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}
	}

	txt_out = TransformText(txt, txt_out, len, format_option, char_spacing_extra);

	if (txt_out)
	{
#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
		if (len && StyleManager::NoFontswitchNoncollapsingWhitespace(txt_out[0]))
			; // only spaces, do nothing
		else
#endif
		if (format_option & TEXT_FORMAT_SMALL_CAPS && len)
			TxtOutSmallCaps(x, y, txt_out, len, TRUE, word_width);
		else
		{
#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT
			if (HasKnownDiacritics(txt_out, len))
			{
				BOOL isRTL = (format_option & TEXT_FORMAT_REVERSE_WORD) && !(format_option & TEXT_FORMAT_BIDI_PRESERVE_ORDER);
				TxtOut_Diacritics(x, y, txt_out, len, isRTL);
			}
			else
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT
				TxtOut(x, y, txt_out, len, word_width);
		}
	}
	OP_DELETEA(allocated_string);
}


int VisualDevice::MeasureNonCollapsingSpaceWord(const uni_char* word, int len, int char_spacing_extra)
{
	if (!word || len == 0)
		return 0;

	int em = GetFontSize();

	int width = len * char_spacing_extra;

#ifdef _GLYPHTESTING_SUPPORT_
	OpFontInfo* fi = styleManager->GetFontInfo(current_font_number);
#endif // _GLYPHTESTING_SUPPORT_

	for (int count = 0; count < len; count++)
	{
		uni_char c = word[count];

#ifdef _GLYPHTESTING_SUPPORT_
		// Test for the glyph using the current FontInfo.

		if (fi && fi->HasGlyph(c))
		{
			width += GetTxtExtent(word + count, 1);
			continue;
		}
#endif // _GLYPHTESTING_SUPPORT_

		switch (c)
		{
		case 0x0020: // Space
			width += em / 4;
			break;
		case 0x2000: // EN_QUAD
			width += em / 2;
			break;
		case 0x2001: // EM QUAD
			width += em;
			break;
		case 0x2002: // EN SPACE
			width += em / 2;
			break;
		case 0x2003: // EM SPACE
			width += em;
			break;
		case 0x2004: // THREE-PER-EM SPACE
			width += em / 3;
			break;
		case 0x2005: // FOUR-PER-EM SPACE
			width += em / 4;
			break;
		case 0x2006: //SIX-PER-EM SPACE
			width += em / 6;
			break;
		case 0x2007: // FIGURE SPACE
			{
#ifdef _GLYPHTESTING_SUPPORT_
				const uni_char* digit = UNI_L("1");
				if (fi && fi->HasGlyph(*digit))
				{
					width += GetTxtExtent(digit, 1);
				}
				else
#endif
					width += em;
			}
			break;
		case 0x2008: // PUNCTUATION SPACE
			{
#ifdef _GLYPHTESTING_SUPPORT_
				const uni_char* point = UNI_L(".");
				if (fi && fi->HasGlyph(*point))
				{
					width += GetTxtExtent(point, 1);
				}
				else
#endif
					width += em / 4;
			}
			break;
		case 0x2009: // THIN SPACE
			width += em / 5;
			break;
		case 0x200A: // HAIR SPACE "thinner than a thin space"
			width += em / 10;
			break;
		case 0x202F: // Narrow no breaking space "resembles a thin space"
			width += em / 5;
			break;
		default:
			OP_ASSERT(Unicode::GetCharacterClass(c) == CC_Mn);
			break;
		}
	}
	return width;

}

int VisualDevice::GetTxtExtentEx(const uni_char* txt, size_t len, int format_option)
{
	// Handle arbitrary length strings, but use the tempbuffer if it is
	// big enough.

	uni_char *txt_out = NULL, *allocated_string = NULL;
	if (len <= UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()))
	{
		txt_out = (uni_char *) g_memory_manager->GetTempBuf2k();
	}
	else
	{
		txt_out = OP_NEWA(uni_char, len);
		allocated_string = txt_out;
		if (!txt_out)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return 0;
		}
	}

	txt_out = TransformText(txt, txt_out, len, format_option | TEXT_FORMAT_REMOVE_DIACRITICS, char_spacing_extra);

	int extent = 0;

	if (txt_out)
	{
#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
		if (len && StyleManager::NoFontswitchNoncollapsingWhitespace(txt_out[0]))
			extent = MeasureNonCollapsingSpaceWord(txt_out, len, char_spacing_extra);
		else
#endif
			if (format_option & TEXT_FORMAT_SMALL_CAPS && len)
			extent = TxtOutSmallCaps(0, 0, txt_out, len, FALSE);
		else
			extent = (int)len ? GetTxtExtent(txt_out, len) : 0;
	}

	OP_DELETEA(allocated_string);
	return extent;
}
int VisualDevice::GetTxtExtentToEx(const uni_char* txt, size_t len, size_t to, int format_option)
{
	// special path for text shaper - assumes platform won't do
	// anything special when text shaper does, which is probably the
	// case
#if defined(USE_TEXTSHAPER_INTERNALLY) && defined(TEXTSHAPER_SUPPORT)
	if (to < len)
	{
	if (TextShaper::NeedsTransformation(txt, len))
	{
		// Handle arbitrary length strings, but use the tempbuffer if it is
		// big enough.

		uni_char *txt_out = NULL, *allocated_string = NULL;
		if (len <= UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()))
		{
			txt_out = (uni_char *) g_memory_manager->GetTempBuf2k();
		}
		else
		{
			txt_out = OP_NEWA(uni_char, len);
			allocated_string = txt_out;
			if (!txt_out)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return 0;
			}
		}

		// FIXME: this doesn't work for text that contains characters
		// that are stripped in TransformText, since then the boundary
		// detection is made on a different string than that which is
		// used for measuring

		// find first boundary before to
		TextShaper::ResetState();
		size_t p = 0;
		size_t out_to = 0;
		UnicodePoint c;
		int clen;
		while (1)
		{
			c = TextShaper::GetShapedChar(txt + p, len - p, clen);
			if (p + (unsigned int)clen > to)
				break;
			p += clen;
			++ out_to;
		}
		// p is offset to first output character boundary before to in logical string
		// c is first output unicode point after p
		// clen is number of input unicode points that form c
		OP_ASSERT(p <= to);
		OP_ASSERT(p + clen > to);

		int extent = 0;
		size_t out_len = len;
		txt_out = TransformText(txt, txt_out, out_len, format_option, char_spacing_extra);
		if (txt_out && out_len)
		{
			OP_ASSERT(out_len >= out_to);
			// string is reversed
			const size_t start_offs = out_len - out_to;
			if (out_to)
			{
				OP_ASSERT(start_offs < out_len);
				extent = GetTxtExtent(txt_out + start_offs, out_to);
			}

			// if inside ligature, place caret accordingly
			const int frac = to - p;
			if (frac > 0)
			{
				OP_ASSERT(frac < clen);
				// FIXME: doesn't work for non-BMP, which is ok as
				// long as text shaper only works on BMP.
				OP_ASSERT(c < 0x10000);
				uni_char u16c = c;
				extent += GetTxtExtent(&u16c, 1) * frac / clen;
			}
		}
		OP_DELETEA(allocated_string);
		return extent;
	}
	}
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

	// FIXME: add a porting interface for this, and measure through
	// that.
	// caveats:
	// * TransformText must keep track of to and update it whenever
	//   characters are stripped and/or string is reversed
	// * probably, TransformText should also tell whether string is
	//   reversed, since measurnig on a reversed string should be done
	//   from the end. maybe it'd be better if added interface worked
	//   on string in logical order ...


	// old way: measure to chars - doesn't work for text that requires
	// shaping
	return GetTxtExtentEx(txt, to, format_option);
}

void VisualDevice::DisplayCenteredString(int x, int y, int extent, int width, const uni_char* txt, int len)
{
	// Skip spaces at the end of the line
	int chars_to_skip = 0;
	while ( (chars_to_skip != len) && (uni_html_space( *(txt + len - chars_to_skip - 1))) )
		chars_to_skip++;

	if (chars_to_skip)
	{
		// Calculate extent for the removed chars and set new length
		extent -= GetTxtExtent(txt + len - chars_to_skip, chars_to_skip);
		len -= chars_to_skip;
	}

	int start_x;
	if (extent > width)
		start_x = x;
	else
		start_x = x + (width - extent) / 2;

	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	DrawString(painter, ToPainter(OpPoint(start_x, y)), txt, len);
}

#ifndef FONTSWITCHING

int NextWord(const uni_char* txt, int len)
{

	int i = 0;
	for (; i<len; i++)
	{
		if (txt[i] == ' ' && (i == len - 1 || txt[i+1] != ' '))
		{
			return i + 1;
		}
	}
	return i;
}

int VisualDevice::TxtOut_WordBreakAndCenter(int x, int y, int iwidth, uni_char* txt, int len, BOOL draw)
{
	OpStatus::Ignore(CheckFont());

	int y_inc_value = (int)(ScaleToDoc(currentFont->Height())*1.2);
	int txt_pos = 0;
	int txt_start = 0;
	int y_pos = y;

	while( txt_pos < len )
	{
		int extent = 0;

		while ( TRUE )
		{
			int nw = NextWord(txt + txt_pos, len - txt_pos);

			int word_extent = GetTxtExtent(txt + txt_pos, nw);

			// Check if word fits in box together with the rest of the words OR
			// if this is a long word which has to be clipped
			if ( (extent + word_extent <= iwidth) || ((txt_start - txt_pos == 0)  && (txt_pos + nw == len))
				 || ((extent == 0) && (word_extent >= iwidth)) )
			{
				extent += word_extent;
				txt_pos += nw;
				if (txt_pos == len)
				{
					if (draw)
						DisplayCenteredString(x, y_pos, extent, iwidth, txt + txt_start, txt_pos - txt_start);
					break;
				}
			} else
			{
				if (draw)
					DisplayCenteredString(x, y_pos, extent, iwidth, txt + txt_start, txt_pos - txt_start);
				break;
			}
		}

		txt_start = txt_pos;
		y_pos += y_inc_value;
	}
	return y_pos - y;
}

#else // FONTSWITCHING

#include "modules/layout/box/box.h"

void VisualDevice::TxtOut_FontSwitch(int x, int y, int iwidth, uni_char* _txt, int _len, const WritingSystem::Script script, BOOL draw /* = TRUE */)
{
	if (_len < 0)
		return;

	size_t len = _len;
	// Handle arbitrary length strings, but use the tempbuffer if it is
	// big enough.
	uni_char *txt = NULL, *allocated_string = NULL;
	if (len <= UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()))
	{
		txt = (uni_char *) g_memory_manager->GetTempBuf2k();
	}
	else
	{
		txt = OP_NEWA(uni_char, len);
		allocated_string = txt;
		if (!txt)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}
	}
	txt = TransformText(_txt, txt, len, 0, 0);

	int currfont = GetCurrentFontNumber();
	int original_font_ascent = (int)GetFontAscent();

	const uni_char* tmp_txt = txt;
	OpPoint current_pos(x, y);

	for(;;)
	{
		WordInfo wi;
		wi.Reset();

		int this_word_start_index = tmp_txt - txt;
		int len_left = len - this_word_start_index;

		SetFont(currfont);
		FontSupportInfo fsi(currfont);
		wi.SetFontNumber(currfont);

		if (!GetNextTextFragment(tmp_txt, len_left, wi, CSS_VALUE_normal, TRUE, TRUE, fsi, doc_manager->GetCurrentVisibleDoc(), script))
			break;

		if (wi.GetFontNumber() != -1)
			SetFont(wi.GetFontNumber());

		OpStatus::Ignore(CheckFont());

		if (currentFont)
		{
			OpPoint baseline_adjusted_pos(current_pos);
			if (currfont != GetCurrentFontNumber())
				baseline_adjusted_pos.y -= (int)GetFontAscent() - original_font_ascent;

			DrawString(painter, ToPainter(baseline_adjusted_pos), txt + this_word_start_index, wi.GetLength());
		}

		current_pos.x += GetTxtExtent(txt + this_word_start_index, wi.GetLength());

		if (wi.HasTrailingWhitespace() || wi.HasEndingNewline())
			current_pos.x += GetTxtExtent(UNI_L(" "), 1);
	}

	SetFont(currfont);

	OP_DELETEA(allocated_string);
}
#endif // FONTSWITCHING

void VisualDevice::ImageAltOut(int x, int y, int iwidth, int iheight, const uni_char* txt, int len, WritingSystem::Script script)
{
	if (iwidth < 2 || iheight < 2)
		return;

	if (OP_GET_A_VALUE(this->color) != 255 && !painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))
	{
		COLORREF oldcol = this->color;
		if (OpStatus::IsSuccess(BeginOpacity(OpRect(x, y, iwidth, iheight), OP_GET_A_VALUE(this->color))))
		{
			SetColor(OP_GET_R_VALUE(this->color), OP_GET_G_VALUE(this->color), OP_GET_B_VALUE(this->color));
			ImageAltOut(x, y, iwidth, iheight, txt, len, script);
			SetColor(oldcol);
			EndOpacity();
			return;
		}
	}

	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	if (txt && len)
	{
		int xpos = x + translation_x;
		int ypos = y + translation_y;

		OpStatus::Ignore(CheckFont());

		OP_STATUS err = painter->SetClipRect(ToPainter(OpRect(xpos, ypos, iwidth, iheight)));
		if (OpStatus::IsError(err))
		{
			g_memory_manager->RaiseCondition(err);
			return;
		}
#ifdef FONTSWITCHING
		TxtOut_FontSwitch(xpos + 1, ypos, iwidth, (uni_char*)txt, len, script);
#else // FONTSWITCHING
		TxtOut_WordBreakAndCenter(xpos, ypos, iwidth, (uni_char*)txt, len);
#endif // FONTSWITCHING

		painter->RemoveClipRect();
	}

	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AltImageBorderEnabled))
	{
		LineOut(x, y, iwidth, 1, IMG_ALT_PEN_TYPE, bg_color, TRUE, TRUE, 0, 0);
		LineOut(x, y, iheight, 1, IMG_ALT_PEN_TYPE, bg_color, FALSE, TRUE, 0, 0);
		LineOut(x + iwidth - 1, y, iheight, 1, IMG_ALT_PEN_TYPE, bg_color, FALSE, FALSE, 0, 0);
		LineOut(x, y + iheight - 1, iwidth, 1, IMG_ALT_PEN_TYPE, bg_color, TRUE, FALSE, 0, 0);
	}
}
// DEPRECATED
void VisualDevice::ImageAltOut(int x, int y, int width, int height, const uni_char* txt, int len, OpFontInfo::CodePage preferred_codepage)
{
	ImageAltOut(x, y, width, height, txt, len, WritingSystem::Unknown);
}

#ifdef _PLUGIN_SUPPORT_
OP_STATUS VisualDevice::GetNewPluginWindow(OpPluginWindow *&new_win, int x, int y, int w, int h, CoreView* parentview
# ifdef USE_PLUGIN_EVENT_API
										   , BOOL windowless
#  ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW										   
										   , Plugin *plugin
#  endif // NS4P_USE_PLUGIN_NATIVE_WINDOW										   
# else
										   , BOOL transparent
# endif // USE_PLUGIN_EVENT_API
										   , HTML_Element* element /* = NULL */
	)
{
	if (!view)
		return OpStatus::ERR;

# if defined USE_PLUGIN_EVENT_API || !defined ALL_PLUGINS_ARE_WINDOWLESS
	ClipViewEntry* clipview = NULL;
# endif
	OpPluginWindow* plugin_window = NULL;

	if (element && !parentview)
		parentview = LayoutWorkplace::FindParentCoreView(element);
	if (!parentview)
		parentview = view;

# ifdef USE_PLUGIN_EVENT_API
	if (windowless)
	{
		x += offset_x;
		y += offset_y;
	}
	else
	{
		clipview = OP_NEW(ClipViewEntry, ());
		if (!clipview ||OpStatus::IsError(clipview->Construct(OpRect(x, y, w, h), parentview)))
		{
			OP_DELETE(clipview);
			return OpStatus::ERR_NO_MEMORY;
		}

		clipview->GetOpView()->SetParentInputContext(this);
		// We must receive paint on the area behind the plugin in order
		// to be able to calculate eventual overlapping layoutboxes.
		clipview->GetOpView()->SetAffectLowerRegions(FALSE);
		clipview->GetClipView()->SetOwningHtmlElement(element);
		// We can't call clipview->Update() before ->SetPluginWindow()
		// but we still need to update the clipview position before
		// OpPluginWindow::Create(), so we'll update it manually.
		clipview->GetOpView()->SetPos(x, y);
		// and since we move the clipview container already, the
		// pluginwindow coords should now be relative to the clipview.
		x = y = 0;
#ifdef DRAG_SUPPORT
		clipview->GetClipView()->SetDragListener(drag_listener);
#endif // DRAG_SUPPORT
	}

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW	
	OP_STATUS status = ProxyOpPluginWindow::Create(&plugin_window, OpRect(x, y, w, h), GetScale(), clipview ? clipview->GetOpView() : GetOpView(), windowless, GetWindow()->GetOpWindow(), plugin);
#else
	OP_STATUS status = OpPluginWindow::Create(&plugin_window, OpRect(x, y, w, h), GetScale(), clipview ? clipview->GetOpView() : GetOpView(), windowless, GetWindow()->GetOpWindow());
#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW	
	if (OpStatus::IsSuccess(status))
	{
		if (clipview)
		{
			coreview_clipper.Add(clipview);
			clipview->GetClipView()->SetPluginWindow(plugin_window);
			clipview->Update();

			if (!m_clip_dev)
				m_clip_dev = GetWindow()->DocManager()->GetCurrentVisibleDoc()->GetVisualDevice();
			OP_ASSERT(m_clip_dev);
			m_clip_dev->plugin_clipper.Add(clipview);
		}

		new_win = plugin_window;

		return OpStatus::OK;
	}
	else
	{
		OP_DELETE(clipview);
		OP_DELETE(plugin_window);
		return status;
	}
# else // USE_PLUGIN_EVENT_API
#  ifdef ALL_PLUGINS_ARE_WINDOWLESS
	// Create no ClipView. All plugins are windowless.
#  else
	if (transparent)
	{
		// if there is a window created anyway, it should not be visible.
		w = h = 0;
	}
	else
	{
		clipview = OP_NEW(ClipViewEntry, ());
		if (!clipview ||OpStatus::IsError(clipview->Construct(OpRect(x, y, w, h), parentview)))
		{
			OP_DELETE(clipview);
			return OpStatus::ERR_NO_MEMORY;
		}
		clipview->GetOpView()->SetParentInputContext(this);

		/** We must receive paint on the area behind the plugin in order to be able to calculate
			eventual overlapping layoutboxes. */
		clipview->GetOpView()->SetAffectLowerRegions(FALSE);

		clipview->GetClipView()->SetOwningHtmlElement(element);

		// the pluginwindow should be relative to the clipview
		x = y = 0;
	}
#  endif

	OP_STATUS status = OpPluginWindow::Create(&plugin_window);
	if (OpStatus::IsError(status))
	{
# if defined USE_PLUGIN_EVENT_API || !defined ALL_PLUGINS_ARE_WINDOWLESS
		OP_DELETE(clipview);
# endif
		return status;
	}

# if defined USE_PLUGIN_EVENT_API || !defined ALL_PLUGINS_ARE_WINDOWLESS
	if (OpStatus::IsError(plugin_window->Construct(OpRect(x, y, w, h), GetScale(), clipview ? clipview->GetOpView() : GetOpView(), transparent, GetWindow()->GetOpWindow())))
		goto err;

	if (clipview)
	{
		coreview_clipper.Add(clipview);
		clipview->GetClipView()->SetPluginWindow(plugin_window);
		clipview->Update();
	}
# else
	if (OpStatus::IsError(plugin_window->Construct(OpRect(x, y, w, h), GetScale(), GetOpView(), transparent)))
		goto err;
# endif

	new_win = plugin_window;
	return OpStatus::OK;

err:
# if defined USE_PLUGIN_EVENT_API || !defined ALL_PLUGINS_ARE_WINDOWLESS
	OP_DELETE(clipview);
# endif
	OP_DELETE(plugin_window);
	return OpStatus::ERR_NO_MEMORY;
# endif // USE_PLUGIN_EVENT_API
}
#endif // _PLUGIN_SUPPORT_

void VisualDevice::CoverPluginArea(const OpRect& rect)
{
#ifdef _PLUGIN_SUPPORT_
	CoreViewContainer *container = GetView() ? GetView()->GetContainer() : NULL;
	if (!container || !container->HasPluginArea())
		return;

	OpRect trect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height);

	// Clip cover area to current clipping
	OpRect clip_rect;
	if (bg_cs.GetClipping(clip_rect))
		trect.IntersectWith(clip_rect);

	trect = OffsetToContainerAndScroll(ScaleToScreen(trect));

	// Clip cover area to visible rect of the view
	OpRect container_clip = GetView()->GetVisibleRect();
	trect.IntersectWith(container_clip);

	container->CoverPluginArea(trect);
#endif
}

OP_STATUS VisualDevice::AddPluginArea(const OpRect& rect, OpPluginWindow* plugin_window)
{
#ifdef _PLUGIN_SUPPORT_
	if (GetWindow() && GetWindow()->IsThumbnailWindow())
		return OpStatus::OK;

	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (!entry)
		return OpStatus::OK;

	OpRect trect(rect.x + translation_x, rect.y + translation_y, rect.width, rect.height);
	return GetView()->GetContainer()->AddPluginArea(OffsetToContainerAndScroll(ScaleToScreen(trect)), entry);
#else
	return OpStatus::OK;
#endif
}

#ifdef _PLUGIN_SUPPORT_

void VisualDevice::ShowPluginWindow(OpPluginWindow* plugin_window, BOOL show)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
		entry->GetClipView()->SetVisibility(show);

	if (show)
		plugin_window->Show();
	else
		plugin_window->Hide();
}

void VisualDevice::SetPluginFixedPositionSubtree(OpPluginWindow* plugin_window, HTML_Element* fixed)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
		entry->GetClipView()->SetFixedPositionSubtree(fixed);
}

#ifdef USE_PLUGIN_EVENT_API

void VisualDevice::DetachPluginWindow(OpPluginWindow* plugin_window)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
	{
		if (CoreView* view = GetView())
			if (CoreViewContainer* container = view->GetContainer())
				container->RemovePluginArea(entry);

		coreview_clipper.Remove(entry);

		OP_ASSERT(m_clip_dev);
		m_clip_dev->plugin_clipper.Remove(entry);
	}

	plugin_window->Detach();
	OP_DELETE(entry);
}

#endif // USE_PLUGIN_EVENT_API

void VisualDevice::AttachPluginCoreView(OpPluginWindow* plugin_window, CoreView *parentview)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
		entry->GetClipView()->IntoHierarchy(parentview);
}

void VisualDevice::DetachPluginCoreView(OpPluginWindow* plugin_window)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
		entry->GetClipView()->OutFromHierarchy();
}

void VisualDevice::DeletePluginWindow(OpPluginWindow* plugin_window)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
	{
		coreview_clipper.Remove(entry);

#ifdef USE_PLUGIN_EVENT_API
		// Since there was a clip view entry we can be sure that
		// m_clip_dev has been set in VisualDevice::GetNewPluginWindow()
		OP_ASSERT(m_clip_dev);
		m_clip_dev->plugin_clipper.Remove(entry);
#endif // USE_PLUGIN_EVENT_API
	}

	OP_DELETE(plugin_window);
	OP_DELETE(entry);
}

void VisualDevice::ResizePluginWindow(OpPluginWindow* plugin_window, int x, int y, int w, int h, BOOL set_pos, BOOL update_pluginwindow)
{
	ClipViewEntry* entry = coreview_clipper.Get(plugin_window);
	if (entry)
	{
		OpRect rect = entry->GetVirtualRect();

		if (set_pos)
		{
			rect.x = x;
			rect.y = y;
		}
		rect.width = w;
		rect.height = h;

		entry->SetVirtualRect(rect);
		if (update_pluginwindow)
			entry->Update();
	}
	else if (update_pluginwindow)
	{
		if (set_pos)
		{
#ifdef VEGA_OPPAINTER_SUPPORT
			MDE_OpView* mdeview = (MDE_OpView*)GetOpView();
			mdeview->GetMDEWidget()->ConvertToScreen(x,y);
#endif // VEGA_OPPAINTER_SUPPORT
			plugin_window->SetPos(x, y);
		}
		plugin_window->SetSize(w, h);
	}
}
#endif // _PLUGIN_SUPPORT_

#if defined DISPLAY_IMAGEOUTEFFECT || defined SKIN_SUPPORT

OP_STATUS VisualDevice::ImageOutEffect(Image& img, const OpRect &src, const OpRect &dst, INT32 effect, INT32 effect_value, ImageListener* image_listener)
{
	if (dst.IsEmpty())
		return OpStatus::OK;

	if (!(effect & ~(Image::EFFECT_DISABLED | Image::EFFECT_BLEND)))
	{
		unsigned int preAlpha = painter->GetPreAlpha();
		unsigned int alpha = preAlpha;
		if (effect & Image::EFFECT_DISABLED)
			alpha /= 2;
		if (effect & Image::EFFECT_BLEND)
		{
			alpha = (alpha*effect_value)/100;
		}
		painter->SetPreAlpha(alpha);
		OP_STATUS err = ImageOut(img, src, dst, image_listener);
		painter->SetPreAlpha(preAlpha);
		return err;
	}

	OpRect dsttranslated = dst;
	dsttranslated.x += translation_x;
	dsttranslated.y += translation_y;

	OP_STATUS status = OpStatus::OK;
	OpBitmap* blit_bitmap = img.GetEffectBitmap(effect, effect_value, image_listener);
	if (blit_bitmap)
	{
		OpPoint bitmapoffset = img.GetBitmapOffset();
		OpRect bitmap_src = src;

		dsttranslated.y += bitmapoffset.y * src.height / dst.height;
		if (bitmap_src.y + bitmap_src.height > (INT32) blit_bitmap->Height())
		{
			int diff = bitmap_src.y + bitmap_src.height - blit_bitmap->Height();
			bitmap_src.height -= diff;
			dsttranslated.height -= diff * src.height / dst.height;
		}

		dsttranslated.x += bitmapoffset.x * src.width / dst.width;
		if (bitmap_src.x + bitmap_src.width > (INT32) blit_bitmap->Width())
		{
			int diff = bitmap_src.x + bitmap_src.width - blit_bitmap->Width();
			bitmap_src.width -= diff;
			dsttranslated.width -= diff * src.width / dst.width;
		}

		OP_ASSERT(bitmap_src.width <= (INT32) blit_bitmap->Width());
		OP_ASSERT(bitmap_src.height <= (INT32) blit_bitmap->Height());

		status = BlitImage(blit_bitmap, bitmap_src, ToPainterExtent(dsttranslated), TRUE);

		img.ReleaseEffectBitmap();
	}

	return status;
}

#endif // DISPLAY_IMAGEOUTEFFECT || SKIN_SUPPORT

void VisualDevice::BitmapOut(OpBitmap* bitmap, const OpRect& src, const OpRect& dst, short image_rendering)
{
	OpRect rect = ToPainter(OpRect(dst.x + translation_x, dst.y + translation_y,
									   dst.width, dst.height));

	SetImageInterpolation(image_rendering);

	if (rect.width != src.width || rect.height != src.height)
		painter->DrawBitmapScaled(bitmap, src, rect);
	else
		painter->DrawBitmapClipped(bitmap, src, OpPoint(rect.x, rect.y));

	ResetImageInterpolation();
}

OP_STATUS VisualDevice::BlitImage(OpBitmap* bitmap, const OpRect& src, const OpRect& const_dst, BOOL add_scrolling)
{
	// Looks what the system supports and makes the best out of it (hopefully)...

	BOOL use_alpha = bitmap->HasAlpha();
	BOOL use_transparent = bitmap->IsTransparent();
	BOOL opaque = !(use_alpha || use_transparent);

	OpRect dst = add_scrolling ? ToPainterNoScale(const_dst) : ToPainterNoScaleNoScroll(const_dst);
	BOOL needscale = TRUE;
	if (dst.width == src.width && dst.height == src.height)
	{
		needscale = FALSE;
	}

	if (!opaque) // Alpha or Transparent bitmap
	{
		// CASE 1: First check if the painter have alphasupport or transparent support
		if (use_alpha && painter->Supports(OpPainter::SUPPORTS_ALPHA))
		{
			if (needscale)
				painter->DrawBitmapScaledAlpha(bitmap, src, dst);
			else
				painter->DrawBitmapClippedAlpha(bitmap, src, OpPoint(dst.x, dst.y));
		}
		else if (use_transparent && !use_alpha && painter->Supports(OpPainter::SUPPORTS_TRANSPARENT))
		{
			if (needscale)
				painter->DrawBitmapScaledTransparent(bitmap, src, dst);
			else
				painter->DrawBitmapClippedTransparent(bitmap, src, OpPoint(dst.x, dst.y));
		}
#ifdef DISPLAY_FALLBACK_PAINTING
		// CASE 2: We can get a bitmap from screen and manupilate it and blit it back
		else if (painter->Supports(OpPainter::SUPPORTS_GETSCREEN))
		{
			OP_STATUS BlitImage_UsingBackgroundAndPointer(OpPainter* painter, OpBitmap* bitmap,
														  const OpRect &source, const OpRect &dest,
											  BOOL is_transparent, BOOL has_alpha);
			OP_STATUS err = BlitImage_UsingBackgroundAndPointer(painter, bitmap, src, dst, use_transparent, use_alpha);
			if (OpStatus::IsError(err))
			{
				g_memory_manager->RaiseCondition(err);
				return err;
			}
 		}
#endif // DISPLAY_FALLBACK_PAINTING
		// CASE 3: We are helpless. No alpha or transparency :(
		else
		{
#ifndef DISPLAY_FALLBACK_PAINTING
			// If this assert trig, you need to enable TWEAK_DISPLAY_FALLBACK_PAINTING. Otherwise transparent or alphatransparent
			// images might not display correctly in all situations because your OpPainter doesn't have the necessary support.
			OP_ASSERT(!painter->Supports(OpPainter::SUPPORTS_GETSCREEN));
#endif
			// Well, anyway, let's draw SOMETHING, at least...
			if (use_alpha && painter->Supports(OpPainter::SUPPORTS_TRANSPARENT))
			{
				// This bitmap has alpha information, but the painter reported that
				// it's not supported. However, the painter does support transparency,
				// so let's draw it transparently instead.
				if (needscale)
					painter->DrawBitmapScaledTransparent(bitmap, src, dst);
				else
					painter->DrawBitmapClippedTransparent(bitmap, src, OpPoint(dst.x, dst.y));
			}
			else
			{
				if (needscale)
					painter->DrawBitmapScaled(bitmap, src, dst);
				else
					painter->DrawBitmapClipped(bitmap, src, OpPoint(dst.x, dst.y));
			};
		}
	}
	else // Normal opaque bitmap
	{
		if (needscale)
			painter->DrawBitmapScaled(bitmap, src, dst);
		else
			painter->DrawBitmapClipped(bitmap, src, OpPoint(dst.x, dst.y));
	}

	return OpStatus::OK;
}

/* static */
OpRect VisualDevice::VisibleTileArea(const OpPoint& offset, OpRect paint, const OpRect& viewport, const OpRect& tile)
{
	// Expand the paint area upwards and to the left by the tile offset
	paint.OffsetBy(-offset);
	paint.width += offset.x;
	paint.height += offset.y;

	if (paint.x < viewport.x - tile.width)
		paint.x = viewport.x - (viewport.x - paint.x) % tile.width;
	if (paint.y < viewport.y - tile.height)
		paint.y = viewport.y - (viewport.y - paint.y) % tile.height;

	if (viewport.x + viewport.width > 0)
		paint.width = MIN(viewport.x + viewport.width - paint.x, paint.width);
	if (viewport.y + viewport.height > 0)
		paint.height = MIN(viewport.y + viewport.height - paint.y, paint.height);
	return paint;
}

/* static */
void VisualDevice::TransposeOffsetInsideImage(OpPoint& offset, int width, int height)
{
	/*
		"+ 1" is needed in the following calculations because integer division floors the 'offset/dimension'
		calculation. This is fine in the positive case, because when the offset is incorrect it is always
		at least one image-length away from the origin, so that at least one image-length will be subtracted.
		In the negative case, however, an incorrect offset can be just under 0, thus less than one image-length
		from the origin. So one image-length must be added.
	*/

	OP_ASSERT(width != 0 && height != 0);

	if (offset.x < 0)
		offset.x += int((-offset.x) / width + 1) * width;
	else if (offset.x >= width)
		offset.x -= int(offset.x / width) * width;

	if (offset.y < 0)
		offset.y += int((-offset.y) / height + 1) * height;
	else if (offset.y >= height)
		offset.y -= int(offset.y / height) * height;
}

OP_STATUS VisualDevice::BlitImageTiled(OpBitmap* bitmap, const OpRect& cdst, const OpPoint& coffset, double imgscale_x, double imgscale_y, int imgspace_x, int imgspace_y)
{
	OpRect dst = cdst;
	OpPoint offset = coffset;

	int img_w = bitmap->Width();
	int img_h = bitmap->Height();
    int scaled_img_w;
    int scaled_img_h;

	if (imgscale_x == 100)
	{
		scaled_img_w = img_w;
	}
	else
	{
		scaled_img_w = (int) (0.5 + (img_w * imgscale_x) / 100);
	}
	if (imgscale_y == 100)
	{
		scaled_img_h = img_h;
	}
	else
	{
		scaled_img_h = (int) (0.5 + (img_h * imgscale_y) / 100);
	}

	if (scaled_img_w == 0 || scaled_img_h == 0)
		return OpStatus::OK;

	int total_width = scaled_img_w + imgspace_x;
	int total_height = scaled_img_h + imgspace_y;

	// Make sure the offset is inside the limit of image width and height.
	TransposeOffsetInsideImage(offset, total_width, total_height);

	/* The API for OpPainter::DrawBitmapTiled has to be
		updated. You will render incorrect scaled tiles if
		imgspace_[x,y] is != 0 or imgscale_x != imgscale_y. */
#if defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_LIMIT_BITMAP_SIZE)
	if (!static_cast<VEGAOpBitmap*>(bitmap)->isTiled())
#endif // VEGA_OPPAINTER_SUPPORT && VEGA_LIMIT_BITMAP_SIZE
	if (painter->Supports(OpPainter::SUPPORTS_TILING) && imgspace_x == 0 && imgspace_y == 0 && imgscale_x == imgscale_y)
	{
		OpRect bmpRect = ToPainterExtent(OpRect(dst.x, dst.y, bitmap->Width(), bitmap->Height()));
		INT32 scale = ToPainterExtent(int(0.5 + imgscale_x));

		const OP_STATUS s = painter->DrawBitmapTiled(bitmap, ToPainterExtent(offset), ToPainter(dst), scale,
													 (int)(0.5 + bmpRect.width * imgscale_x / 100),
													 (int)(0.5 + bmpRect.height * imgscale_y / 100));
		return s;
	}

	// Calculate visible rectangle of this document in the parent document.

	OpRect clip_rect(rendering_viewport);

	/*
	 * NOTE: We may want to clip against the visible portion of the
	 * parent document (if this VisualDevice belongs to a/an
	 * frame/iframe). Maybe the CoreView hierarchy could be used to
	 * achieve this?
	 *
	 * There is trickyness involved though - related bugs include
	 * CORE-15420 and ITVSDK-2036 (related to BeginNoScalePainting).
	 */

#ifdef CSS_TRANSFORMS
	if (HasTransform())
	{
		// If we render using a transform, there will be a mismatch in
		// the clipping calculations below (document space vs. local
		// space), so convert clip_rect to local space if a transform
		// is used.
		AffinePos vd_ctm = GetCTM();
		vd_ctm.ApplyInverse(clip_rect);
	}
#endif // CSS_TRANSFORMS

	// Clip the start and stop position. (We only want to iterate through the visible range)
	OpRect visible = VisibleTileArea(offset, dst, clip_rect, OpRect(0,0, total_width, total_height));

	int dx, dy;
	int dw = scaled_img_w;
	int dh = scaled_img_h;

	for(dy = visible.y; dy < visible.y + visible.height; dy += dh + imgspace_y)
	{
		int dy_clipped = dy;
		int dh_clipped = dh;
		int sy_clipped = 0;
		int sh_clipped = img_h;

		// Clip Upper edge
		if (dy_clipped < dst.y)
		{
			int diff = dst.y - dy_clipped;
			dy_clipped += diff;
			dh_clipped -= diff;
			sy_clipped += int(0.5 + (diff * 100) / imgscale_y);
			sh_clipped -= int(0.5 + (diff * 100) / imgscale_y);
		}

		// Clip lower edge
		if (dy_clipped + dh_clipped > dst.y + dst.height)
		{
			int diff = (dy_clipped + dh_clipped) - (dst.y + dst.height);
			dh_clipped -= diff;
			sh_clipped -= int(0.5 + (diff * 100) / imgscale_y);
		}

		if (sh_clipped == 0 || dh_clipped == 0)
			continue;
		if (sh_clipped < 0 || dh_clipped < 0)
			if (imgspace_y == 0)
				break; // We are done.
			else
				continue;

		for(dx = visible.x; dx < visible.x + visible.width; dx += dw + imgspace_x)
		{
			int dx_clipped = dx;
			int dw_clipped = dw;
			int sx_clipped = 0;
			int sw_clipped = img_w;

			// Clip left edge
			if (dx_clipped < dst.x)
			{
				int diff = dst.x - dx_clipped;
				dx_clipped += diff;
				dw_clipped -= diff;
				sx_clipped += int(0.5 + (diff * 100) / imgscale_x);
				sw_clipped -= int(0.5 + (diff * 100) / imgscale_x);
			}

			// Clip right edge
			if (dx_clipped + dw_clipped > dst.x + dst.width)
			{
				int diff = (dx_clipped + dw_clipped) - (dst.x + dst.width);
				dw_clipped -= diff;
				sw_clipped -= int(0.5 + (diff * 100) / imgscale_x);
			}

			if (sw_clipped == 0 || dw_clipped == 0)
				continue;
			if (sw_clipped < 0 || dw_clipped < 0)
				if (imgspace_x == 0)
					break; // We are done.
				else
					continue;

			OpRect dest = ToPainterExtent(OpRect(dx_clipped, dy_clipped, dw_clipped, dh_clipped));
			RETURN_IF_ERROR(BlitImage(bitmap, OpRect(sx_clipped, sy_clipped, sw_clipped, sh_clipped), dest, TRUE));
		}
	}
	return OpStatus::OK;
}

BOOL VisualDevice::ImageNeedsInterpolation(const Image& img, int img_doc_width, int img_doc_height) const
{
#ifdef PREFS_HAVE_INTERPOLATE_IMAGES
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InterpolateImages))
		return FALSE;
#endif // PREFS_HAVE_INTERPOLATE_IMAGES

	return (int)img.Width() != ScaleToScreen(img_doc_width) ||
		(int)img.Height() != ScaleToScreen(img_doc_height);
}

void VisualDevice::SetImageInterpolation(const Image& img, const OpRect& dest, short image_rendering)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!(image_rendering == CSS_VALUE_optimizeSpeed ||
		  image_rendering == CSS_VALUE__o_crisp_edges) &&
		!img.ImageDecoded() && ImageNeedsInterpolation(img, dest.width, dest.height))
		image_rendering = CSS_VALUE_optimizeSpeed;
#endif // VEGA_OPPAINTER_SUPPORT

	SetImageInterpolation(image_rendering);
}

void VisualDevice::SetImageInterpolation(short image_rendering)
{
	OP_ASSERT(painter != NULL);

#ifdef VEGA_OPPAINTER_SUPPORT
	VEGAFill::Quality image_quality = VEGAFill::QUALITY_NEAREST;

#ifdef PREFS_HAVE_INTERPOLATE_IMAGES
	if (!(image_rendering == CSS_VALUE_optimizeSpeed ||
		  image_rendering == CSS_VALUE__o_crisp_edges) &&
		g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InterpolateImages))
		image_quality = VEGAFill::QUALITY_LINEAR;
#endif // PREFS_HAVE_INTERPOLATE_IMAGES

	VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(painter);
	vpainter->SetImageQuality(image_quality);
#endif // VEGA_OPPAINTER_SUPPORT
}

void VisualDevice::ResetImageInterpolation()
{
	OP_ASSERT(painter != NULL);

#ifdef VEGA_OPPAINTER_SUPPORT
	VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(painter);
	VEGAFill::Quality image_quality = VEGAFill::QUALITY_NEAREST;

#ifdef PREFS_HAVE_INTERPOLATE_IMAGES
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InterpolateImages))
		image_quality = VEGAFill::QUALITY_LINEAR;
#endif // PREFS_HAVE_INTERPOLATE_IMAGES

	vpainter->SetImageQuality(image_quality);
#endif // VEGA_OPPAINTER_SUPPORT
}

OP_STATUS VisualDevice::ImageOut(Image& img, const OpRect& src, const OpRect& dst, ImageListener* image_listener, short image_rendering)
{
	return ImageOut(img, src, dst, image_listener, image_rendering, TRUE);
}

OP_STATUS VisualDevice::ImageOut(Image& img, const OpRect& src, const OpRect& dst, ImageListener* image_listener, short image_rendering, BOOL translate)
{
	if (dst.IsEmpty())
		return OpStatus::OK;

#ifdef HAS_ATVEF_SUPPORT
	if (img.IsAtvefImage())
	{
		OP_ASSERT(g_tvManager);
		if (!g_tvManager)
			return OpStatus::OK;

		UINT8 red,green,blue,alpha;
		g_tvManager->GetChromakeyColor(&red, &green, &blue, &alpha);

		COLORREF previous_color = GetColor();
		painter->SetColor(red,green,blue,alpha);

		OpRect r(dst);
		if (translate)
		{
			r.x += translation_x;
			r.y += translation_y;
		}

		painter->ClearRect(ToPainter(r));
		painter->SetColor(previous_color);
		return OpStatus::OK;
	}
#endif

	OpRect dsttranslated = dst;
	if (translate)
	{
		dsttranslated.x += translation_x;
		dsttranslated.y += translation_y;
	}

	OpBitmap* bitmap = img.GetBitmap(image_listener);
	if (bitmap)
	{
		OpPoint bitmapoffset = img.GetBitmapOffset();
		OpRect bitmap_src = src;

		dsttranslated.y += bitmapoffset.y * dst.height / src.height;
		if (bitmap_src.y + bitmap_src.height > (INT32)bitmap->Height())
		{
			int diff = bitmap_src.y + bitmap_src.height - bitmap->Height();
			bitmap_src.height -= diff;
			dsttranslated.height -= diff * dst.height / src.height;
		}

		dsttranslated.x += bitmapoffset.x * dst.width / src.width;
		if (bitmap_src.x + bitmap_src.width > (INT32)bitmap->Width())
		{
			int diff = bitmap_src.x + bitmap_src.width - bitmap->Width();
			bitmap_src.width -= diff;
			dsttranslated.width -= diff * dst.width / src.width;
		}

		OP_ASSERT(bitmap_src.width <= (INT32)bitmap->Width());
		OP_ASSERT(bitmap_src.height <= (INT32)bitmap->Height());

		SetImageInterpolation(img, dst, image_rendering);

		OP_STATUS status = BlitImage(bitmap, bitmap_src, ToPainterExtent(dsttranslated), TRUE);

		ResetImageInterpolation();

		img.ReleaseBitmap();
		return status;
	}
#ifdef OOM_SAFE_API
 	if (!bitmap && img.IsOOM())
		return OpStatus::ERR_NO_MEMORY;
#endif

	return OpStatus::OK;
}

BOOL VisualDevice::IsSolid(Image& img, ImageListener* image_listener)
{
#ifdef HAS_ATVEF_SUPPORT
	if (img.IsAtvefImage())
		return TRUE;
#endif
	// OPTO: Check if 1x1 image like below in ImageOutTiled
	return !img.IsTransparent();
}

OP_STATUS VisualDevice::ImageOutTiled(Image& img, const OpRect& cdst, const OpPoint& offset, ImageListener* image_listener, double imgscale_x, double imgscale_y, int imgspace_x, int imgspace_y, BOOL translate, short image_rendering)
{
	if (cdst.IsEmpty())
		return OpStatus::OK;

	OpRect dst = cdst;

	if (translate)
	{
		dst.x += translation_x;
		dst.y += translation_y;
	}

#ifdef HAS_ATVEF_SUPPORT
	if (img.IsAtvefImage())
	{
		OP_ASSERT(g_tvManager);

		if (!g_tvManager)
			return OpStatus::OK; // :) matter of definition...

		UINT8 red,green,blue,alpha;
		g_tvManager->GetChromakeyColor(&red, &green, &blue, &alpha);

		COLORREF previous_color = GetColor();
		painter->SetColor(red,green,blue,alpha);
		painter->ClearRect(ToPainter(dst));
		painter->SetColor(previous_color);
		return OpStatus::OK;
	}
#endif

	// If it is a singlepixelbitmap, we can draw with FillRect which is much faster.
	OpBitmap* bitmap = img.GetBitmap(image_listener);
	OpPoint bitmap_offset = img.GetBitmapOffset();
	BOOL tileable = (bitmap_offset.x == 0 && bitmap_offset.y == 0);
	const BOOL has_spacing = imgspace_x != 0 || imgspace_y != 0;
	if (bitmap)
	{
		if (img.Width() != bitmap->Width() || img.Height() != bitmap->Height())
			tileable = FALSE;
		if (bitmap->Width() == 1 && bitmap->Height() == 1 && !has_spacing)
		{
			UINT32 data[1]; // enough data for one 32bit pixel
			bitmap->GetLineData(data, 0);
#ifdef OPERA_BIG_ENDIAN
			unsigned char alpha = ((unsigned char*)data)[0];
#else
			unsigned char alpha = ((unsigned char*)data)[3];
#endif
			if (alpha == 0)
			{
				img.ReleaseBitmap();
				return OpStatus::OK;
			}
			else if (alpha == 255)
			{
#ifdef OPERA_BIG_ENDIAN
				unsigned char red = ((unsigned char*)data)[1];
				unsigned char green = ((unsigned char*)data)[2];
				unsigned char blue = ((unsigned char*)data)[3];
#else
				unsigned char red = ((unsigned char*)data)[2];
				unsigned char green = ((unsigned char*)data)[1];
				unsigned char blue = ((unsigned char*)data)[0];
#endif

				painter->SetColor(red, green, blue);
				painter->FillRect(ToPainter(dst));
				painter->SetColor(OP_GET_R_VALUE(this->color), OP_GET_G_VALUE(this->color), OP_GET_B_VALUE(this->color), OP_GET_A_VALUE(this->color));

				img.ReleaseBitmap();
				return OpStatus::OK;
			}
		}
		img.ReleaseBitmap();
		bitmap = NULL;
	}

	OP_STATUS status = OpStatus::OK;

	const BOOL disable_tiled_bitmap = (painter->Supports(OpPainter::SUPPORTS_TILING) && tileable) || has_spacing;

	if (disable_tiled_bitmap)
		bitmap = img.GetBitmap(image_listener);
	else
		bitmap = img.GetTileBitmap(image_listener, dst.width, dst.height);

	if (bitmap)
	{
		SetImageInterpolation(image_rendering);

		status = BlitImageTiled(bitmap, dst, offset, imgscale_x, imgscale_y, imgspace_x, imgspace_y);

		ResetImageInterpolation();

		if (disable_tiled_bitmap)
			img.ReleaseBitmap();
		else
			img.ReleaseTileBitmap();
	}
#ifdef OOM_SAFE_API
	else if (img.IsOOM())
		status = OpStatus::ERR_NO_MEMORY;
#endif

	return status;
}

#ifdef SKIN_SUPPORT
OP_STATUS VisualDevice::ImageOutTiledEffect(Image& img, const OpRect& cdst, const OpPoint& offset,
											INT32 effect, INT32 effect_value,
											INT32 imgscale_x, INT32 imgscale_y)
{
	if (!(effect & ~(Image::EFFECT_DISABLED | Image::EFFECT_BLEND)))
	{
		unsigned int preAlpha = painter->GetPreAlpha();
		unsigned int alpha = preAlpha;
		if (effect & Image::EFFECT_DISABLED)
			alpha /= 2;
		if (effect & Image::EFFECT_BLEND)
		{
			alpha = (alpha*effect_value)/100;
		}
		painter->SetPreAlpha(alpha);
		OP_STATUS err = ImageOutTiled(img, cdst, offset, NULL, imgscale_x, imgscale_y, 0, 0);
		painter->SetPreAlpha(preAlpha);
		return err;
	}

	OP_STATUS status = OpStatus::OK;
	OpRect dst = cdst;

	dst.x += translation_x;
	dst.y += translation_y;

#if defined(VEGA_OPPAINTER_SUPPORT) && defined(VEGA_LIMIT_BITMAP_SIZE)
	// GetEffectBitmap is expensive, and should not be called if the
	// painter cannot use it anyways
	if (!VEGAOpBitmap::needsTiling(img.Width(), img.Height()))
#endif // VEGA_OPPAINTER_SUPPORT && VEGA_LIMIT_BITMAP_SIZE
	if (painter->Supports(OpPainter::SUPPORTS_TILING))
	{
		OpBitmap* effectbitmap = img.GetEffectBitmap(effect, effect_value);

		if (effectbitmap)
		{
			INT32 scale = ToPainterExtent(int(0.5 + imgscale_x));
			OpRect bmpRect = ToPainterExtent(OpRect(dst.x, dst.y, effectbitmap->Width(), effectbitmap->Height()));
			status = painter->DrawBitmapTiled(effectbitmap, ToPainter(offset), ToPainter(dst), scale,
											  bmpRect.width * imgscale_x / 100, bmpRect.height * imgscale_y / 100);

			img.ReleaseEffectBitmap();
		}
		return status;
	}

	int tilewidth = img.Width() * imgscale_x / 100;
	int tileheight = img.Height() * imgscale_y / 100;
	int horizontal_count = (dst.width + tilewidth - 1) / tilewidth;
	int vertical_count = (dst.height + tileheight - 1) / tileheight;

	// Make sure we don't ask GetTileEffectBitmap for too much. It would hang and allocate too much memory.
	// We simply set a limit of maximum 64 pixels in both directions.
	int horizontal_max = MAX(64 / tilewidth, 1);
	int vertical_max = MAX(64 / tileheight, 1);
	horizontal_count = MIN(horizontal_count, horizontal_max);
	vertical_count = MIN(vertical_count, vertical_max);

	OpBitmap* bitmap = img.GetTileEffectBitmap(effect, effect_value, horizontal_count, vertical_count);

	if (bitmap)
	{
		status = BlitImageTiled(bitmap, dst, offset, imgscale_x, imgscale_y, 0, 0);

		img.ReleaseTileEffectBitmap();
	}

	return status;
}
#endif // SKIN_SUPPORT


/* Calculate the width of each tile and the start offset for tiling, in either vertical or horizontal direction.
 *
 * @param[in/out]	part_len			As in variable this is the unscaled size of one tile. If the scale type is CSS_VALUE_round, it returns the scaled size of the tile.
 * @param[out]		start_ofs			The start offset of the first tile.
 * @param[in]		available_length	The space where to put the tiles.
 * @param[in]		scale_type			The CSS value for scaling.
 *
 */
void CalculateTileCount(double &part_len, int &start_ofs, int available_length, short scale_type)
{
	if (part_len > 0)
	{
		if (scale_type == CSS_VALUE_round)
		{
			// Round is actually ceil (according to the spec)
			int count = (int)op_ceil((double)available_length / (double)part_len);
			part_len = (double)available_length / (double)count;
		}
		else
		{
			// Center result by changing start_ofs
			int overflowed_len = (int)(op_ceil(available_length / part_len) * part_len);
			start_ofs = -(overflowed_len - available_length) / 2;
		}
	}
}

#ifdef VEGA_OPPAINTER_SUPPORT

void TileBitmapPart(VisualDevice *vd, OpBitmap *bitmap, OpRect src_rect, int start_x, int stop_x, int start_y, int stop_y, double dx, double dy, double start_ofs_x, double start_ofs_y);

OpBitmap *CreateSlicedBitmapIfNeeded(OpBitmap *bitmap, int dst_width, int dst_height, const OpRect &src_rect, BOOL force)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (dst_width > src_rect.width || dst_height > src_rect.height || force)
	{
		if (!force && !g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::InterpolateImages))
			return NULL;
		// Upscaling with filtering creates artifacts since filtering use pixels from outside src_rect. Must slice into smaller bitmap.
		return static_cast<VEGAOpBitmap*>(bitmap)->GetSubBitmap(src_rect);
	}
#endif // VEGA_OPPAINTER_SUPPORT
	return NULL;
}

void TileBitmapPart(VisualDevice *vd, OpBitmap *bitmap, OpRect src_rect, int start_x, int stop_x, int start_y, int stop_y, double dx, double dy, double start_ofs_x, double start_ofs_y)
{
	if (src_rect.IsEmpty() || !dx || !dy)
		return;

	// FIX: Clip to parent viewports too. Use CoreView::GetVisibleRect
	OpRect clip_rect(vd->ScaleToScreen(vd->GetRenderingViewX()), vd->ScaleToScreen(vd->GetRenderingViewY()), vd->VisibleWidth(), vd->VisibleHeight());
	if (start_x > clip_rect.x + clip_rect.width ||
		start_y > clip_rect.y + clip_rect.height ||
		stop_x < clip_rect.x || stop_y < clip_rect.y)
		return;

	int dst_w = stop_x - start_x;
	int dst_h = stop_y - start_y;
	BOOL need_tile = ((dst_w > 64 || dst_h > 64) && (src_rect.width < 32 && src_rect.height < 32));
	if (dst_w / dx < 4 && dst_h / dy < 4)
		// If we won't get many iterations, we will not need the tile either
		need_tile = FALSE;

	// FIX: Create scaled image directly so we don't end up with a too small tile (that is stretched down)
	// FIX: Could do the same thing as in VisualDevice::ImageOutTiled and ignore fully transparent 1x1 bitmaps, or draw the color.

	OpBitmap* old_bitmap = bitmap;
	OpBitmap *sliced_bitmap = CreateSlicedBitmapIfNeeded(bitmap, stop_x - start_x, stop_y - start_y, src_rect, need_tile);
	if (sliced_bitmap)
	{
		bitmap = sliced_bitmap;
		src_rect.x = 0;
		src_rect.y = 0;
	}

	// Calculate visible rectangle of this document in the parent document.

	OpBitmap *tile_bitmap = NULL;
	OpAutoPtr<OpBitmap> tile_bitmap_;
	if (need_tile && sliced_bitmap)
	{
		BOOL tile_horizontally = (start_x + start_ofs_x + dx < stop_x);
		BOOL tile_vertically = (start_y + start_ofs_y + dy < stop_y);
		//StaticImageContent::UpdateTileBitmapIfNeeded(bitmap, tile_bitmap, dst_w, dst_h);
		// Some code copied from StaticImageContent::UpdateTileBitmapIfNeeded. Ideally, the API would be improved instead.
		const int TILE_SQUARE_LIMIT = 64;
		if(bitmap->Width() * bitmap->Height() < TILE_SQUARE_LIMIT * TILE_SQUARE_LIMIT)
		{
			int desired_width = MIN(dst_w, TILE_SQUARE_LIMIT);
			int desired_height = MIN(dst_h, TILE_SQUARE_LIMIT);
			UINT32 tile_width = tile_horizontally ? ((desired_width + bitmap->Width() - 1) / bitmap->Width()) * bitmap->Width() : bitmap->Width();
			UINT32 tile_height = tile_vertically ? ((desired_height + bitmap->Height() - 1) / bitmap->Height()) * bitmap->Height() : bitmap->Height();

			OpBitmap* CreateTileOptimizedBitmap(OpBitmap* srcbitmap, INT32 new_width, INT32 new_height);
			tile_bitmap = CreateTileOptimizedBitmap(bitmap, tile_width, tile_height);
			if (tile_bitmap == bitmap)
			{
				// CreateTileOptimizedBitmap returns the source bitmap on error.
				// It is a very dangerous practice since it easily leads to double frees.
				// Setting bitmap_tile to NULL here solves the problem.
				tile_bitmap = NULL;
			}
		}
		if (tile_bitmap)
		{
			tile_bitmap_.reset(tile_bitmap);
			bitmap = tile_bitmap;
			double factor_x = (double)tile_bitmap->Width() / (double)src_rect.width;
			double factor_y = (double)tile_bitmap->Height() / (double)src_rect.height;
			src_rect.width = tile_bitmap->Width();
			src_rect.height = tile_bitmap->Height();
			dx *= factor_x;
			dy *= factor_y;
		}
	}

	if (!dx || !dy)
		return;

	// Clip the start and stop position. (We only want to iterate through the visible range)
	if (start_x + start_ofs_x + dx < clip_rect.x)
	{
		double slack = (int)((clip_rect.x - (start_x + start_ofs_x + dx)) / dx);
		start_ofs_x += slack * dx;
	}
	if (start_y + start_ofs_y + dy < clip_rect.y)
	{
		double slack = (int)((clip_rect.y - (start_y + start_ofs_y + dy)) / dy);
		start_ofs_y += slack * dy;
	}
	stop_x = MIN(stop_x, clip_rect.x + clip_rect.width + (int)dx);
	stop_y = MIN(stop_y, clip_rect.y + clip_rect.height + (int)dy);

	// Draw tiled along the edge
	double x, y;
	for(y = start_y + start_ofs_y; y < stop_y; y += dy)
	for(x = start_x + start_ofs_x; x < stop_x; x += dx)
	{
		// FIX: Use DrawTiled and implement in VegaOpPainter instead! Would be hwd acceleratable and faster!
		// FIX: Must have subpixel accuracy in DrawBitmap. Otherwise we get src rounding errors and rendering artifacts when clipping!

		OpRect src = src_rect;
		OpRect dst((int)x, (int)y, (int)(x + dx) - (int)x, (int)(y + dy) - (int)y);
		if (dst.x < start_x)
		{
			int diff = (int)-start_ofs_x;
			int sdiff = (src.width * diff) / dst.width;
			src.x += sdiff;
			src.width -= sdiff;
			dst.x += diff;
			dst.width -= diff;
		}
		if (dst.x + dst.width > stop_x)
		{
			int diff = dst.x + dst.width - stop_x;
			int sdiff = (src.width * diff) / dst.width;
			src.width -= sdiff;
			dst.width -= diff;
		}
		if (dst.y < start_y)
		{
			int diff = (int)-start_ofs_y;
			int sdiff = (src.height * diff) / dst.height;
			src.y += sdiff;
			src.height -= sdiff;
			dst.y += diff;
			dst.height -= diff;
		}
		if (dst.y + dst.height > stop_y)
		{
			int diff = dst.y + dst.height - stop_y;
			int sdiff = (src.height * diff) / dst.height;
			src.height -= sdiff;
			dst.height -= diff;
		}
		vd->BlitImage(bitmap, src, dst, TRUE);
		// Debug
		/*vd->SetColor(tile_bitmap ? 255 : 0,0,0, 45);
		vd->painter->DrawRect(vd->OffsetToContainerAndScroll(dst));*/
	}
#ifdef VEGA_OPPAINTER_SUPPORT
	if (sliced_bitmap)
		static_cast<VEGAOpBitmap*>(old_bitmap)->ReleaseSubBitmap();
#endif // VEGA_OPPAINTER_SUPPORT
}
void BlitImageIfNotEmpty(VisualDevice *vd, OpBitmap *bitmap, OpRect src_rect, const OpRect &dst_rect)
{
	if (!src_rect.IsEmpty())
	{
		OpBitmap* old_bitmap = bitmap;
		OpBitmap *sliced_bitmap = CreateSlicedBitmapIfNeeded(bitmap, dst_rect.width, dst_rect.height, src_rect, FALSE);
		if (sliced_bitmap)
		{
			bitmap = sliced_bitmap;
			src_rect.x = 0;
			src_rect.y = 0;
		}
		vd->BlitImage(bitmap, src_rect, dst_rect, TRUE);
#ifdef VEGA_OPPAINTER_SUPPORT
		if (sliced_bitmap)
			static_cast<VEGAOpBitmap*>(old_bitmap)->ReleaseSubBitmap();
#endif // VEGA_OPPAINTER_SUPPORT
	}
}
#endif // VEGA_OPPAINTER_SUPPORT

void VisualDevice::PrepareBorderImageRects(BorderImageRects& border_image_rects, const BorderImage* border_image, const Border* border, int imgw, int imgh, int border_box_width, int border_box_height)
{
	// 1. Preprocess some variables.

	int border_top = border->top.width;
	int border_right = border->right.width;
	int border_bottom = border->bottom.width;
	int border_left = border->left.width;

	if (border_top + border_bottom > border_box_height)
	{
		// Avoid overlap (the skin code may trigger this)
		border_top = (border_box_height * border_top) / (border_top + border_bottom);
		border_bottom = border_box_height - border_top;
	}
	if (border_left + border_right > border_box_width)
	{
		// Avoid overlap (the skin code may trigger this)
		border_left = (border_box_width * border_left) / (border_left + border_right);
		border_right = border_box_width - border_left;
	}

	int cut_top = border_image->cut[0] < 0 ? (imgh * -border_image->cut[0]) / 100 : border_image->cut[0];
	int cut_right = border_image->cut[1] < 0 ? (imgw * -border_image->cut[1]) / 100 : border_image->cut[1];
	int cut_bottom = border_image->cut[2] < 0 ? (imgh * -border_image->cut[2]) / 100 : border_image->cut[2];
	int cut_left = border_image->cut[3] < 0 ? (imgw * -border_image->cut[3]) / 100 : border_image->cut[3];

	cut_top = MAX(0, cut_top); cut_top = MIN(imgh, cut_top);
	cut_left = MAX(0, cut_left); cut_left = MIN(imgw, cut_left);
	cut_right = MAX(0, cut_right); cut_right = MIN(imgw, cut_right);
	cut_bottom = MAX(0, cut_bottom); cut_bottom = MIN(imgh, cut_bottom);

	int cut_middle_w = imgw - cut_left - cut_right;
	int cut_middle_h = imgh - cut_top - cut_bottom;

	// 2. Calulate corners

	border_image_rects.corner_source[0] = OpRect(0, 0, cut_left, cut_top);
	border_image_rects.corner_destination[0] = OpRect(0, 0, border_left, border_top);

	border_image_rects.corner_source[1] = OpRect(imgw - cut_right, 0, cut_right, cut_top);
	border_image_rects.corner_destination[1] = OpRect(border_box_width - border_right, 0, border_right, border_top);

	border_image_rects.corner_source[2] = OpRect(imgw - cut_right, imgh - cut_bottom, cut_right, cut_bottom);
	border_image_rects.corner_destination[2] = OpRect(border_box_width - border_right, border_box_height - border_bottom, border_right, border_bottom);

	border_image_rects.corner_source[3] = OpRect(0, imgh - cut_bottom, cut_left, cut_bottom);
	border_image_rects.corner_destination[3] = OpRect(0, border_box_height - border_bottom, border_left, border_bottom);

	// 3. Calculate edges

	// The used_part and used_ofs variables are used as input when calculating metrics for the fill rectangle.
	double used_part_width_top = 0;
	double used_part_width_bottom = 0;
	double used_part_height_left = 0;
	double used_part_height_right = 0;
	int used_ofs_x_top = 0;
	int used_ofs_x_bottom = 0;
	int used_ofs_y_left = 0;
	int used_ofs_y_right = 0;

	// Edges are stored in the BorderImageRects structure as top-right-bottom-left

	/* BorderImage::scale[0] represents the CSS values for the horizontal scaling of the top and bottom edges as well as the center rectangle.
	   BorderImage::scale[1] represents the CSS values for the vertical scaling of the left and right edges as well as the center rectangle. */
	border_image_rects.edge_scale[0] = border_image->scale[0];
	border_image_rects.edge_scale[1] = border_image->scale[1];
	border_image_rects.edge_scale[2] = border_image->scale[0];
	border_image_rects.edge_scale[3] = border_image->scale[1];

	border_image_rects.edge_source[0].Empty();
	border_image_rects.edge_source[1].Empty();
	border_image_rects.edge_source[2].Empty();
	border_image_rects.edge_source[3].Empty();

	if (cut_middle_w > 0)
	{
		border_image_rects.edge_source[0] = OpRect(cut_left, 0, cut_middle_w, cut_top);
		border_image_rects.edge_destination[0] = OpRect(border_left, 0, border_box_width - border_left - border_right, border_top);

		border_image_rects.edge_source[2] = OpRect(cut_left, imgh - cut_bottom, cut_middle_w, cut_bottom);
		border_image_rects.edge_destination[2] = OpRect(border_left, border_box_height - border_bottom, border_box_width - border_left - border_right, border_bottom);

		if (border_image->scale[0] == CSS_VALUE_stretch)
			used_part_width_top = used_part_width_bottom = border_box_width - border_left - border_right;
		else
		{
			OP_ASSERT(border_image->scale[0] == CSS_VALUE_round || border_image->scale[0] == CSS_VALUE_repeat);

			if (!border_image_rects.edge_source[0].IsEmpty())
			{
				used_part_width_top = cut_middle_w;
				double factor = (double)border_top / cut_top;
				used_part_width_top *= factor;
				CalculateTileCount(used_part_width_top, used_ofs_x_top, border_box_width - border_left - border_right, border_image->scale[0]);
				border_image_rects.edge_used_part_width[0] = used_part_width_top;
				border_image_rects.edge_used_part_height[0] = border_top;
				border_image_rects.edge_start_ofs[0] = used_ofs_x_top;
			}

			if (!border_image_rects.edge_source[2].IsEmpty())
			{
				used_part_width_bottom = cut_middle_w;
				double factor = (double)border_bottom / cut_bottom;
				used_part_width_bottom *= factor;
				CalculateTileCount(used_part_width_bottom, used_ofs_x_bottom, border_box_width - border_left - border_right, border_image->scale[0]);
				border_image_rects.edge_used_part_width[2] = used_part_width_bottom;
				border_image_rects.edge_used_part_height[2] = border_bottom;
				border_image_rects.edge_start_ofs[2] = used_ofs_x_bottom;
			}
		}
	}

	if (cut_middle_h > 0)
	{
		border_image_rects.edge_source[1] = OpRect(imgw - cut_right, cut_top, cut_right, cut_middle_h);
		border_image_rects.edge_destination[1] = OpRect(border_box_width - border_right, border_top, border_right, border_box_height - border_top - border_bottom);

		border_image_rects.edge_source[3] = OpRect(0, cut_top, cut_left, cut_middle_h);
		border_image_rects.edge_destination[3] = OpRect(0, border_top, border_left, border_box_height - border_top - border_bottom);

		if (border_image->scale[1] == CSS_VALUE_stretch)
			used_part_height_left = used_part_height_right = border_box_height - border_top - border_bottom;
		else
		{
			OP_ASSERT(border_image->scale[1] == CSS_VALUE_round || border_image->scale[1] == CSS_VALUE_repeat);

			if (!border_image_rects.edge_source[1].IsEmpty())
			{
				used_part_height_right = cut_middle_h;
				double factor = (double)border_right / cut_right;
				used_part_height_right *= factor;
				CalculateTileCount(used_part_height_right, used_ofs_y_right, border_box_height - border_top - border_bottom, border_image->scale[1]);
				border_image_rects.edge_used_part_width[1] = border_right;
				border_image_rects.edge_used_part_height[1] = used_part_height_right;
				border_image_rects.edge_start_ofs[1] = used_ofs_y_right;
			}

			if (!border_image_rects.edge_source[3].IsEmpty())
			{
				used_part_height_left = cut_middle_h;
				double factor = (double)border_left / cut_left;
				used_part_height_left *= factor;
				CalculateTileCount(used_part_height_left, used_ofs_y_left, border_box_height - border_top - border_bottom, border_image->scale[1]);
				border_image_rects.edge_used_part_width[3] = border_left;
				border_image_rects.edge_used_part_height[3] = used_part_height_left;
				border_image_rects.edge_start_ofs[3] = used_ofs_y_left;
			}
		}
	}

	// 4. Calculate the fill rectangle.

	border_image_rects.fill.Empty();

	if (cut_middle_w > 0 && cut_middle_h > 0)
	{
		double center_part_width = imgw;
		double center_part_height = imgh;
		int start_ofs_x = 0;
		int start_ofs_y = 0;

		// Base tile sizes and offsets on the values calculated for edges if available.

		if (used_part_width_top)
		{
			center_part_width = used_part_width_top;
			start_ofs_x = (int)used_ofs_x_top;
		}
		else if (used_part_width_bottom)
		{
			center_part_width = used_part_width_bottom;
			start_ofs_x = (int)used_ofs_x_bottom;
		}
		else
		{
			CalculateTileCount(center_part_width, start_ofs_x, border_box_width - border_left - border_right, border_image->scale[0]);
		}

		if (used_part_height_left)
		{
			center_part_height = used_part_height_left;
			start_ofs_y = (int)used_ofs_y_left;
		}
		else if (used_part_height_right)
		{
			center_part_height = used_part_height_right;
			start_ofs_y = (int)used_ofs_y_right;
		}
		else
		{
			CalculateTileCount(center_part_height, start_ofs_y, border_box_height - border_top - border_bottom, border_image->scale[1]);
		}

		if (center_part_width > 0 && center_part_height > 0)
		{
			border_image_rects.fill = OpRect(cut_left, cut_top, cut_middle_w, cut_middle_h);
			border_image_rects.fill_destination = OpRect(border_left, border_top, border_box_width - border_left - border_right, border_box_height - border_top - border_bottom);
			border_image_rects.fill_start_ofs_x = start_ofs_x;
			border_image_rects.fill_start_ofs_y = start_ofs_y;
			border_image_rects.fill_part_width = center_part_width;
			border_image_rects.fill_part_height = center_part_height;
		}
	}
}

#ifdef CSS_GRADIENT_SUPPORT
OP_STATUS VisualDevice::BorderImageGradientPartOut(const OpRect& border_box_rect, const CSS_Gradient& gradient, const OpRect& src_rect, const OpRect& destination_rect, COLORREF current_color)
{
	OP_ASSERT(!src_rect.IsEmpty());

	double horizontal_scale = (double)destination_rect.width / (double)src_rect.width;
	double vertical_scale = (double)destination_rect.height / (double)src_rect.height;

	OpRect screen_destination(destination_rect);
	screen_destination.OffsetBy(translation_x, translation_y);
	screen_destination = ToPainter(screen_destination);

	VEGAOpPainter *vpainter = static_cast<VEGAOpPainter*>(painter);
	VEGAFill *gradient_out;
	VEGATransform radial_adjust;
	radial_adjust.loadIdentity();
	gradient_out = gradient.MakeVEGAGradient(this, vpainter, border_box_rect, current_color, radial_adjust);

	if (!gradient_out)
		return OpStatus::ERR_NO_MEMORY;

	VEGATransform tx;

	tx.loadTranslate(VEGA_INTTOFIX(screen_destination.x), VEGA_INTTOFIX(screen_destination.y));

	VEGATransform scale;
	scale.loadScale(VEGA_FLTTOFIX(horizontal_scale), VEGA_FLTTOFIX(vertical_scale));
	tx.multiply(scale);

	VEGATransform tx2;
	tx2.loadTranslate(VEGA_INTTOFIX(ToPainterExtent(-src_rect.x)), VEGA_INTTOFIX(ToPainterExtent(-src_rect.y)));
	tx.multiply(tx2);

	tx.multiply(radial_adjust);

	vpainter->SetFillTransform(tx);
	vpainter->SetFill(gradient_out);

	painter->FillRect(screen_destination);

	vpainter->SetFill(NULL);
	vpainter->ResetFillTransform();
	OP_DELETE(gradient_out);

	return OpStatus::OK;
}

void VisualDevice::PaintBorderImageGradient(const OpRect& border_box_rect, const Border* border, const BorderImage* border_image, COLORREF current_color)
{
	if (!doc_display_rect.Intersecting(ToBBox(border_box_rect)))
		return;

	BorderImageRects border_image_rects;
	PrepareBorderImageRects(border_image_rects, border_image, border, border_box_rect.width, border_box_rect.height, border_box_rect.width, border_box_rect.height);

	// paint corners

	for (int i = 0; i < 4; i++)
	{
		if (!border_image_rects.corner_source[i].IsEmpty())
			RAISE_AND_RETURN_VOID_IF_ERROR(BorderImageGradientPartOut(border_box_rect, *border_image->border_gradient, border_image_rects.corner_source[i], border_image_rects.corner_destination[i], current_color));
	}

	// paint edges

	for (int i = 0; i < 4; i++)
	{
		if (!border_image_rects.edge_source[i].IsEmpty())
		{
			if (border_image_rects.edge_scale[i] == CSS_VALUE_stretch)
				RAISE_AND_RETURN_VOID_IF_ERROR(BorderImageGradientPartOut(border_box_rect, *border_image->border_gradient,
														   border_image_rects.edge_source[i], border_image_rects.edge_destination[i],
														   current_color));
			else
			{
				int horizontal_offset = (i % 2) ? 0 : border_image_rects.edge_start_ofs[i];
				int vertical_offset = (i % 2) ? border_image_rects.edge_start_ofs[i] : 0;

				OpRect doc_dest_rect = border_image_rects.edge_destination[i];
				doc_dest_rect.OffsetBy(translation_x, translation_y);

				RETURN_VOID_IF_ERROR(painter->SetClipRect(ToPainter(doc_dest_rect)));

				OP_STATUS err = DrawBorderImageGradientTiled(*border_image->border_gradient, border_box_rect, border_image_rects.edge_source[i],
															 border_image_rects.edge_destination[i],
															 border_image_rects.edge_used_part_width[i], border_image_rects.edge_used_part_height[i],
															 horizontal_offset, vertical_offset, current_color);

				painter->RemoveClipRect();
				RAISE_AND_RETURN_VOID_IF_ERROR(err);
			}
		}

	}

	// Paint fill (center) rectangle

	if (!border_image_rects.fill.IsEmpty())
	{
		OpRect doc_dest_rect = border_image_rects.fill_destination;
		doc_dest_rect.OffsetBy(translation_x, translation_y);

		RETURN_VOID_IF_ERROR(painter->SetClipRect(ToPainter(doc_dest_rect)));

		OP_STATUS err = DrawBorderImageGradientTiled(*border_image->border_gradient, border_box_rect, border_image_rects.fill,
													 border_image_rects.fill_destination,
													 border_image_rects.fill_part_width, border_image_rects.fill_part_height,
													 border_image_rects.fill_start_ofs_x, border_image_rects.fill_start_ofs_y,
													 current_color);

		painter->RemoveClipRect();
		RAISE_AND_RETURN_VOID_IF_ERROR(err);
	}
}

#endif // CSS_GRADIENT_SUPPORT

OP_STATUS VisualDevice::PaintBorderImage(Image& img, ImageListener* image_listener, const OpRect &doc_rect, const Border *border, const BorderImage *border_image, const ImageEffect *effect, short image_rendering)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!doc_display_rect.Intersecting(ToBBox(doc_rect)))
		return OpStatus::OK;

	BorderImageRects border_image_rects;
	PrepareBorderImageRects(border_image_rects, border_image, border, img.Width(), img.Height(), doc_rect.width, doc_rect.height);

	OpBitmap* bitmap;
	unsigned int preAlpha = painter->GetPreAlpha();
	if (effect)
	{
		if (!(effect->effect & ~(Image::EFFECT_DISABLED | Image::EFFECT_BLEND)))
		{
			unsigned int alpha = preAlpha;
			if (effect->effect & Image::EFFECT_DISABLED)
				alpha /= 2;
			if (effect->effect & Image::EFFECT_BLEND)
			{
				alpha = (alpha*effect->effect_value)/100;
			}
			bitmap = img.GetBitmap(image_listener);
			if (bitmap)
				painter->SetPreAlpha(alpha);
		}
		else
			bitmap = img.GetEffectBitmap(effect->effect, effect->effect_value, effect->image_listener);
	}
	else
		bitmap = img.GetBitmap(image_listener);

	if (!bitmap)
		return OpStatus::ERR;

	SetImageInterpolation(image_rendering);

	// Paint corners

	for (int i = 0; i < 4; i++)
	{
		if (!border_image_rects.corner_source[i].IsEmpty())
		{
			OpRect screen_dest_rect = border_image_rects.corner_destination[i];
			screen_dest_rect.OffsetBy(doc_rect.x, doc_rect.y);
			screen_dest_rect.OffsetBy(translation_x, translation_y);
			screen_dest_rect = ToPainterExtent(screen_dest_rect);

			BlitImageIfNotEmpty(this, bitmap, border_image_rects.corner_source[i], screen_dest_rect);
		}
	}

	// Paint edges

	for (int i = 0; i < 4; i++)
	{
		if (!border_image_rects.edge_source[i].IsEmpty())
		{
			OpRect screen_dest_rect = border_image_rects.edge_destination[i];
			screen_dest_rect.OffsetBy(doc_rect.x, doc_rect.y);
			screen_dest_rect.OffsetBy(translation_x, translation_y);
			screen_dest_rect = ToPainterExtent(screen_dest_rect);

			if (border_image_rects.edge_scale[i] == CSS_VALUE_stretch)
				BlitImageIfNotEmpty(this, bitmap, border_image_rects.edge_source[i], screen_dest_rect);
			else
			{
				int horizontal_offset = (i % 2) ? 0 : border_image_rects.edge_start_ofs[i];
				int vertical_offset = (i % 2) ? border_image_rects.edge_start_ofs[i] : 0;

				TileBitmapPart(this, bitmap, border_image_rects.edge_source[i],
							   screen_dest_rect.x, screen_dest_rect.Right(), screen_dest_rect.y, screen_dest_rect.Bottom(),
							   ToPainterExtentDbl(border_image_rects.edge_used_part_width[i]), ToPainterExtentDbl(border_image_rects.edge_used_part_height[i]),
							   ToPainterExtentDbl(horizontal_offset), ToPainterExtentDbl(vertical_offset));
			}
		}
	}

	// Paint middle

	if (!border_image_rects.fill.IsEmpty())
	{
		OpRect fill_screen_dest = border_image_rects.fill_destination;
		fill_screen_dest.OffsetBy(doc_rect.x, doc_rect.y);
		fill_screen_dest.OffsetBy(translation_x, translation_y);
		fill_screen_dest = ToPainterExtent(fill_screen_dest);

		double fill_tile_width = ToPainterExtentDbl(border_image_rects.fill_part_width);

		if (border_image_rects.edge_scale[0] == CSS_VALUE_stretch)
		{
			// Avoid rounding errors if we are not tiling in this direction.
			fill_tile_width = fill_screen_dest.width;
		}

		double fill_tile_height = ToPainterExtentDbl(border_image_rects.fill_part_height);

		if (border_image_rects.edge_scale[1] == CSS_VALUE_stretch)
		{
			// Avoid rounding errors if we are not tiling in this direction.
			fill_tile_height = fill_screen_dest.height;
		}

		TileBitmapPart(this, bitmap, border_image_rects.fill,
					   fill_screen_dest.x,fill_screen_dest.Right(), fill_screen_dest.y, fill_screen_dest.Bottom(),
					   fill_tile_width, fill_tile_height,
					   ToPainterExtentDbl(border_image_rects.fill_start_ofs_x), ToPainterExtentDbl(border_image_rects.fill_start_ofs_y));
	}

	ResetImageInterpolation();

	if (effect)
	{
		if (!(effect->effect & ~(Image::EFFECT_DISABLED | Image::EFFECT_BLEND)))
		{
			painter->SetPreAlpha(preAlpha);
			img.ReleaseBitmap();
		}
		else
			img.ReleaseEffectBitmap();
	}
	else
		img.ReleaseBitmap();

	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif
}

int VisualDevice::GetDoubleBorderSingleWidth(int border_w)
{
	if (border_w >= 3)
		border_w = border_w/3 + (border_w % 3) / 2;
	return border_w;
}

void VisualDevice::LineOut(int x, int y, int length, int iwidth, int css_pen_style, COLORREF color, BOOL horizontal, BOOL top_or_left, int top_or_left_w, int right_or_bottom_w)
{
	if (color == CSS_COLOR_transparent)
		return;

	OpRect orig_rect(x, y, horizontal?length:iwidth, horizontal?iwidth:length);
	if (horizontal)
	{
		if (!top_or_left)
			orig_rect.y -= iwidth-1;
		if (top_or_left_w < 0)
		{
			orig_rect.x += top_or_left_w;
			orig_rect.width -= top_or_left_w;
		}
		if (right_or_bottom_w < 0)
			orig_rect.width -= right_or_bottom_w;
	}
	else
	{
		if (!top_or_left)
			orig_rect.x -= iwidth-1;
		if (top_or_left_w < 0)
		{
			orig_rect.y += top_or_left_w;
			orig_rect.height -= top_or_left_w;
		}
		if (right_or_bottom_w < 0)
			orig_rect.height -= right_or_bottom_w;
	}

	x += translation_x;
	y += translation_y;

	int dot_dash_w = 0;
	int dot_dash_s = iwidth; //5;
	if (dot_dash_s < 2)
		dot_dash_s = 2;

	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	COLORREF old_cref = this->color;
	COLORREF cref = this->color;
	if (color != USE_DEFAULT_COLOR && color != CSS_COLOR_invert)
	    cref = HTM_Lex::GetColValByIndex(color);

	COLORREF change_cref = USE_DEFAULT_COLOR;
	int change_color_i = INT_MAX;

	switch (css_pen_style)
	{
	case CSS_VALUE_hidden:
		return;

	case CSS_VALUE_dotted:
		dot_dash_w = iwidth; //5;
		break;

	case CSS_VALUE_dashed:
		dot_dash_w = iwidth * 3; //20;
		break;

	case CSS_VALUE_solid:
	case CSS_VALUE_double:
		break;

	case CSS_VALUE_groove:
	case CSS_VALUE_ridge:
	case CSS_VALUE_inset:
	case CSS_VALUE_outset:
		if (color != CSS_COLOR_invert)
		{
			COLORREF bg_color_dark;
			COLORREF bg_color_light;
			Get3D_Colors(cref, bg_color_dark, bg_color_light);

			if (css_pen_style == CSS_VALUE_inset || css_pen_style == CSS_VALUE_groove)
				cref = (top_or_left) ? bg_color_dark : bg_color_light;
			else
				if (css_pen_style == CSS_VALUE_outset || css_pen_style == CSS_VALUE_ridge)
					cref = (top_or_left) ? bg_color_light : bg_color_dark;

			if (css_pen_style == CSS_VALUE_ridge || css_pen_style == CSS_VALUE_groove)
			{
				change_cref = (cref == bg_color_light) ? bg_color_dark : bg_color_light;
				change_color_i = top_or_left ? iwidth / 2 : (iwidth + 1) / 2;
			}
		}
		break;

	default:
		return;
	}

#ifdef _PRINT_SUPPORT_
	if (GetWindow() && GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
	{
		cref = GetGrayscale(cref);
		//color = cref;
	}
	else
#endif // _PRINT_SUPPORT_
		if (color == CSS_COLOR_invert)
			cref = color;

	if (cref != USE_DEFAULT_COLOR && cref != CSS_COLOR_invert)
	{
		painter->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref), OP_GET_A_VALUE(cref));
	}

	int last_ex_w = 0;
	if (dot_dash_w)
	{
		// calculate best spacing
		int rem = length % (dot_dash_w + dot_dash_s);
		int best_i = 0;

		if (rem != dot_dash_w)
		{
			for (int i=1; i<3 && i < dot_dash_w + dot_dash_s; i++)
			{
				int s = length % (dot_dash_w + dot_dash_s + i);
				if (op_abs(s - dot_dash_w) < op_abs(rem - dot_dash_w))
				{
					rem = s;
					best_i = i;
				}
				if (dot_dash_s - i >= 2)
				{
					s = length % (dot_dash_w + dot_dash_s - i);
					if (op_abs(s - dot_dash_w) < op_abs(rem - dot_dash_w))
					{
						rem = s;
						best_i = -i;
					}
				}
			}
		}

		dot_dash_s += best_i;

		if (rem > dot_dash_w)
			last_ex_w = rem - dot_dash_w;
	}

	OpRect destrect;

	// Have some pointers to the destrect. Then we can swap axis and share the same code for horizontal and vertical line.
	INT32 *dest_u = &destrect.x;
	INT32 *dest_v = &destrect.y;
	INT32 *dest_w = &destrect.width;
	INT32 *dest_h = &destrect.height;
	if (!horizontal)
	{
		// Swap axis
		dest_u = &destrect.y;
		dest_v = &destrect.x;
		dest_w = &destrect.height;
		dest_h = &destrect.width;
		int tmp = x;
		x = y;
		y = tmp;
	}

	// don't draw more than necessary
	{
		OpRect clip_rect(GetRenderingViewX(), GetRenderingViewY(), GetRenderingViewWidth(), GetRenderingViewHeight());
#ifdef CSS_TRANSFORMS
		if (HasTransform())
		{
			GetCTM().ApplyInverse(clip_rect);
		}
#endif // CSS_TRANSFORMS

		// start and end document coordinates of rendering viewport
		int clip_start, clip_end;
		if (horizontal)
		{
			clip_start = clip_rect.x;
			clip_end = clip_start + clip_rect.width;
		}
		else
		{
			clip_start = clip_rect.y;
			clip_end = clip_start + clip_rect.height;
		}

		if (x < clip_start)
		{
			int delta;
			if (dot_dash_w)
			{
				// remove only whole segments, so that dots start where they should
				const int seg_len = dot_dash_w + dot_dash_s; // length of dash + space
				const int n = (clip_start - x) / seg_len; // number of whole segments outside viewport
				delta = n * seg_len; // length of n segments
			}
			else
			{
				delta = MAX(0, clip_start - x - top_or_left_w);
			}
			x += delta;
			length -= delta;
		}
		if (x + length > clip_end)
			length = MIN(length, clip_end - x + right_or_bottom_w);
	}

	BOOL opacity_layer = FALSE;
	if (cref != CSS_COLOR_invert && OP_GET_A_VALUE(cref) != 255 &&
		!painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))
	{
		if (OpStatus::IsSuccess(BeginOpacity(orig_rect, OP_GET_A_VALUE(cref))))
		{
			opacity_layer = TRUE;
		}
		painter->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref));
	}

	int dx1, dx2;
	for (int i=0; i<iwidth; i++)
	{
		if (css_pen_style == CSS_VALUE_double && iwidth >= 3)
 		{
	 		int dw = iwidth/3 + (iwidth%3)/2;
 			if (i == dw)
 				i += iwidth - 2*dw;
 		}
		int dy = i;
		if (!top_or_left)
			dy = -dy;

		BOOL opaque = OP_GET_A_VALUE(cref) == 255;
		BOOL opaque_or_horizontal = opaque || horizontal;
		int add_dx = (top_or_left_w > 0 && opaque_or_horizontal) ? top_or_left_w - 1 : 0;
		dx1 = (int)((i*top_or_left_w + add_dx) / iwidth);
		add_dx = (right_or_bottom_w > 0 && opaque_or_horizontal) ? right_or_bottom_w - 1 : 0;
		dx2 = (int)((i*right_or_bottom_w + add_dx) / iwidth);

		if (horizontal && !opaque)
		{
			if (top_or_left_w)
				++dx1;

			if (right_or_bottom_w)
				++dx2;
		}

		int from_x = x+dx1;
		int to_x = x+length-dx2;
		if (dot_dash_w)
		{
			int ddx = (dx1 % (dot_dash_w + dot_dash_s));
			int xx = from_x;
			int end_xx = from_x - ddx + dot_dash_w;
			if (ddx >= dot_dash_w)
			{
				xx += dot_dash_w + dot_dash_s - ddx;
				end_xx = xx + dot_dash_w;
			}
			while (xx < to_x)
			{
				if (end_xx >= to_x - last_ex_w)
					end_xx = to_x;

				destrect.x = xx;
				destrect.y = y + dy;
				destrect.width = end_xx - destrect.x;
				destrect.height = 1;

				OpRect paint_rect = ToPainter(OpRect(*dest_u, *dest_v, *dest_w, *dest_h));
				if (cref == CSS_COLOR_invert)
					painter->InvertRect(paint_rect);
				else
					painter->FillRect(paint_rect);

				xx = end_xx + dot_dash_s;
				end_xx += dot_dash_w + dot_dash_s;
			}
		}
		else
		{
			if (change_cref != USE_DEFAULT_COLOR && i >= change_color_i)
			{
#ifdef _PRINT_SUPPORT_
				if (GetWindow() && GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
				{
					change_cref = GetGrayscale(change_cref);
				}
#endif // _PRINT_SUPPORT_
				if (opacity_layer)
					EndOpacity();
				opacity_layer = FALSE;
				if (OP_GET_A_VALUE(change_cref) != 255 &&
					!painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))
				{
					if (OpStatus::IsSuccess(BeginOpacity(orig_rect, OP_GET_A_VALUE(change_cref))))
					{
						opacity_layer = TRUE;
					}
					painter->SetColor(OP_GET_R_VALUE(change_cref), OP_GET_G_VALUE(change_cref), OP_GET_B_VALUE(change_cref));
				}
				else
					painter->SetColor(OP_GET_R_VALUE(change_cref), OP_GET_G_VALUE(change_cref), OP_GET_B_VALUE(change_cref), OP_GET_A_VALUE(change_cref));

				change_cref = USE_DEFAULT_COLOR;
			}

			destrect.Set(from_x, y + dy, to_x - from_x, 1);

			OpRect paint_rect = ToPainter(OpRect(*dest_u, *dest_v, *dest_w, *dest_h));
			if (cref == CSS_COLOR_invert)
				painter->InvertRect(paint_rect);
			else
				painter->FillRect(paint_rect);
		}
	}

	if (opacity_layer)
		EndOpacity();

#ifdef _PRINT_SUPPORT_
	if (GetWindow() && GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
	{
		old_cref = GetGrayscale(old_cref);
	}
#endif // _PRINT_SUPPORT_

	painter->SetColor(OP_GET_R_VALUE(old_cref), OP_GET_G_VALUE(old_cref), OP_GET_B_VALUE(old_cref), OP_GET_A_VALUE(old_cref));
}

/**
 * A horizontal line that will be drawn with style CSS_VALUE_solid. Its width will be the
 * same regardless of where on the screen it is. This makes this line different from LineOut
 * which scales the width to be sure that there are no gaps between lines that should touch
 * each other.
 */
void VisualDevice::DecorationLineOut(int x, int y, int length, int iwidth, COLORREF color)
{
	if (color == CSS_COLOR_transparent)
		return;

	x += translation_x;
	y += translation_y;

	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	COLORREF old_cref = this->color;
	COLORREF cref = this->color;
	if (color != USE_DEFAULT_COLOR && color != CSS_COLOR_invert)
	    cref = HTM_Lex::GetColValByIndex(color);

#ifdef _PRINT_SUPPORT_
	if (GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
	{
		cref = GetGrayscale(cref);
		//color = cref;
	}
	else
#endif // _PRINT_SUPPORT_
	if (color == CSS_COLOR_invert)
		cref = color;

	if (cref != USE_DEFAULT_COLOR && cref != CSS_COLOR_invert)
	{
		painter->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref), OP_GET_A_VALUE(cref));
	}

	OpRect line_rect(x, y, length, iwidth);
	if (cref == CSS_COLOR_invert)
		painter->InvertRect(ToPainterDecoration(line_rect));
	else
	{
		if (cref == USE_DEFAULT_COLOR)
			cref = color;
		BOOL opacity_layer = FALSE;
		if (OP_GET_A_VALUE(cref) != 255 && !painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))
		{
			line_rect.OffsetBy(-translation_x, -translation_y);
			if (OpStatus::IsSuccess(BeginOpacity(line_rect, OP_GET_A_VALUE(this->color))))
			{
				painter->SetColor(OP_GET_R_VALUE(cref), OP_GET_G_VALUE(cref), OP_GET_B_VALUE(cref));
				opacity_layer = TRUE;
			}
			line_rect.OffsetBy(translation_x, translation_y);
		}

		painter->FillRect(ToPainterDecoration(line_rect));

		if (opacity_layer)
			EndOpacity();
	}
#ifdef _PRINT_SUPPORT_
	if (GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
	{
		old_cref = GetGrayscale(old_cref);
	}
#endif // _PRINT_SUPPORT_

	painter->SetColor(OP_GET_R_VALUE(old_cref), OP_GET_G_VALUE(old_cref), OP_GET_B_VALUE(old_cref), OP_GET_A_VALUE(old_cref));
}

void VisualDevice::RectangleOut(int x, int y, int iwidth, int iheight, int css_pen_style)
{
	LineOut(x, y, iwidth, 1, css_pen_style, color, TRUE, TRUE, 1, 1);
	LineOut(x, y, iheight, 1, css_pen_style, color, FALSE, TRUE, 1, 1);
	LineOut(x, y + iheight - 1, iwidth, 1, css_pen_style, color, TRUE, FALSE, 1, 1);
	LineOut(x + iwidth - 1, y, iheight, 1, css_pen_style, color, FALSE, FALSE, 1, 1);
}

#define VD_AlphaFallback(CALL, RECT)	do {							\
	if (OP_GET_A_VALUE(this->color) != 255 &&							\
		!painter->Supports(OpPainter::SUPPORTS_ALPHA_COLOR))			\
	{																	\
		COLORREF oldcol = this->color;									\
		if (OpStatus::IsSuccess(BeginOpacity(RECT, OP_GET_A_VALUE(this->color)))) \
		{																\
			SetColor(OP_GET_R_VALUE(this->color), OP_GET_G_VALUE(this->color), OP_GET_B_VALUE(this->color)); \
			CALL;														\
			SetColor(oldcol);											\
			EndOpacity();												\
			return;														\
		}																\
	}} while(0)

void VisualDevice::BulletOut(int x, int y, int iwidth, int iheight)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	VD_AlphaFallback(BulletOut(x, y, iwidth, iheight), OpRect(x, y, iwidth, iheight));

	painter->FillEllipse(ToPainterDecoration(OpRect(x + translation_x, y + translation_y,
													iwidth, iheight)));
}

void VisualDevice::DrawLine(const OpPoint &from, const OpPoint &to, UINT32 width)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	VD_AlphaFallback(DrawLine(from, to, width), OpRect(MIN(from.x, to.x)-width,
													   MIN(from.y, to.y)-width,
													   MAX(from.x,to.x)-MIN(from.x,to.x)+2*width,
													   MAX(from.y,to.y)-MIN(from.y,to.y)+2*width));

	OpPoint p1(from.x + translation_x, from.y + translation_y);
	OpPoint p2(to.x + translation_x, to.y + translation_y);
	painter->DrawLine(ToPainter(p1), ToPainter(p2), ToPainterExtentLineWidth(width));
}

void VisualDevice::DrawLine(const OpPoint &from, UINT32 length, BOOL horizontal, UINT32 width)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	VD_AlphaFallback(DrawLine(from, length, horizontal, width),
					 OpRect(from.x-width, from.y-width,
							(horizontal?length:width)+2*width, (horizontal?width:length)+2*width));

	OpPoint p1(from.x + translation_x, from.y + translation_y);
	painter->DrawLine(ToPainter(p1), ToPainterExtent(length), horizontal, ToPainterExtentLineWidth(width));
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT

void VisualDevice::DrawMisspelling(const OpPoint &from, UINT32 length, UINT32 wave_height, COLORREF color, UINT32 width)
{
	COLORREF old_col = GetColor();
	SetColor(color);

	OpPoint p = ToPainter(OpPoint(from.x + translation_x, from.y + translation_y));
	width = ToPainterExtent(MIN(width, 2));
	width = MAX(width, 1);

	int size = width;
	int step = size * 2;
	int x = p.x, y = p.y + 1;
	int end = x + ToPainterExtent(length);
	//Alignment x: x = ((x + step - 1) / step) * step;

	for(; x < end; x += step)
		painter->FillRect(OpRect(x, y, size, size));

	SetColor(old_col);
}

#endif // INTERNAL_SPELLCHECK_SUPPORT

void VisualDevice::DrawCaret(const OpRect& rect)
{
#ifdef DISPLAY_INVERT_CARET

#ifdef GADGET_SUPPORT
	// ugly kludge to make sure the cursor is always shown in widgets on Win32
	if(GetWindow() && GetWindow()->GetOpWindow() &&
		((OpWindow *)GetWindow()->GetOpWindow())->GetStyle() == OpWindow::STYLE_GADGET)
	{
		FillRect(rect);
	}
	else
#endif // GADGET_SUPPORT
		InvertRect(rect);

#else
	FillRect(rect);
#endif
}

void VisualDevice::FillRect(const OpRect &rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	VD_AlphaFallback(FillRect(rect), rect);

	painter->FillRect(ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y,
									   rect.width, rect.height)));
}

void VisualDevice::DrawRect(const OpRect &rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	if (GetScale() > 100)
	{
		// The thickness should be scaled too, so we'll use 4 fillrects if scale is > 100
		FillRect(OpRect(rect.x, rect.y, rect.width-1, 1));
		FillRect(OpRect(rect.x, rect.y, 1, rect.height-1));
		FillRect(OpRect(rect.x, rect.y + rect.height - 1, rect.width, 1));
		FillRect(OpRect(rect.x + rect.width - 1, rect.y, 1, rect.height));
		return;
	}

	VD_AlphaFallback(DrawRect(rect), rect);

	painter->DrawRect(ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y,
									   rect.width, rect.height)));
}

void	VisualDevice::ClearRect(const OpRect &rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	// FIXME: solve the non VegaOpPainter case
#ifdef VEGA_OPPAINTER_SUPPORT
	// Use ToBBox in order to get the bounding box of the rect in case
	// there's a transform applied.
	((VEGAOpPainter*)painter)->ClearRect(OffsetToContainerAndScroll(ScaleToScreen(ToBBox(rect))));
#endif // VEGA_OPPAINTER_SUPPORT
}

void	VisualDevice::DrawFocusRect(const OpRect &rect)
{
	OpRect clipped = doc_display_rect;
	clipped.OffsetBy(-translation_x, -translation_y);
	clipped.IntersectWith(rect);

	BOOL top_clipped = rect.Top() != clipped.Top();
	BOOL bottom_clipped = rect.Bottom() != clipped.Bottom();
	BOOL left_clipped = rect.Left() != clipped.Left();
	BOOL right_clipped = rect.Right() != clipped.Right();

	if (!(top_clipped && bottom_clipped))
	{
		// Start at the right offset to avoid scrolling glitches
		INT32 start = !((clipped.x - rect.x) & 1);

		INT32 end = clipped.width;

		// Don't draw the last (corner) pixel when not clipped
		if (!right_clipped)
			end -= 1;

		for(INT32 i = start; i < end; i += 2)
		{
			// top
			if (!top_clipped)
				InvertRect(OpRect(i + clipped.x, clipped.y, 1, 1));
			// bottom
			if (!bottom_clipped)
				InvertRect(OpRect(i + clipped.x, clipped.y + clipped.height - 1, 1, 1));
		}
	}
	if (!(left_clipped && right_clipped))
	{
		// Start at the right offset to avoid scrolling glitches
		INT32 start = !((clipped.y - rect.y) & 1);

		INT32 end = clipped.height;

		// Don't draw the last (corner) pixel when not clipped
		if (!bottom_clipped)
			end -= 1;

		for(INT32 i = start; i < end; i += 2)
		{
			// left
			if (!left_clipped)
				InvertRect(OpRect(clipped.x, i + clipped.y, 1, 1));
			// right
			if (!right_clipped)
				InvertRect(OpRect(clipped.x + clipped.width - 1, i + clipped.y, 1, 1));
		}
	}
}

void	VisualDevice::InvertRect(const OpRect &rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	painter->InvertRect(ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y,
										 rect.width, rect.height)));
}

void VisualDevice::FillEllipse(const OpRect &rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	VD_AlphaFallback(FillEllipse(rect), rect);

	painter->FillEllipse(ToPainter(OpRect(rect.x + translation_x, rect.y + translation_y,
										  rect.width, rect.height)));
}

void VisualDevice::EllipseOut(int x, int y, int iwidth, int iheight, UINT32 line_width)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	VD_AlphaFallback(EllipseOut(x, y, iwidth, iheight, line_width), OpRect(x, y, iwidth, iheight));

	painter->DrawEllipse(ToPainter(OpRect(x + translation_x, y + translation_y,
										  iwidth, iheight)), line_width);
}

void VisualDevice::InvertBorderRect(const OpRect &rect, int border)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	painter->InvertBorderRect(ToPainterNoScroll(rect), border);
}

void VisualDevice::DrawSelection(const OpRect &rect)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.

	UINT32 selection_color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
	unsigned r = OP_GET_R_VALUE(selection_color);
	unsigned g = OP_GET_G_VALUE(selection_color);
	unsigned b = OP_GET_B_VALUE(selection_color);
	unsigned a = OP_GET_A_VALUE(selection_color);
	UINT32 transparent_color = OP_RGBA(r, g, b, a/2);
	SetColor32(transparent_color);

	this->FillRect(rect);
}

BOOL VisualDevice::GetSearchMatchRectangles(OpVector<OpRect> &all_rects, OpVector<OpRect> &active_rects)
{
#if !defined(HAS_NO_SEARCHTEXT) && defined(SEARCH_MATCHES_ALL)
	FramesDocument* frames_doc = doc_manager ? doc_manager->GetCurrentVisibleDoc() : NULL;
	if (!frames_doc || !frames_doc->GetHtmlDocument() || !frames_doc->GetHtmlDocument()->GetFirstSelectionElm())
		return FALSE;
#endif

#ifdef WIC_SEARCH_MATCHES
	BgRegion active_region;
	INT32 skin_inset = 5;
#ifdef SKIN_SUPPORT
	if (OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Search Hit Active Skin"))
		skin_elm->GetMargin(&skin_inset, &skin_inset, &skin_inset, &skin_inset, 0);
#endif

	OpPoint container_point = GetOpView()->ConvertToScreen(OpPoint(0, 0));
	OpPoint top_container_point = GetWindow()->VisualDev()->GetOpView()->ConvertToScreen(OpPoint(0, 0));

	VisualDeviceOutline *o = (VisualDeviceOutline *) outlines.First();
	while (o)
	{
		if (VDTextHighlight *th = o->GetTextHighlight())
		{
			// Clip to visible rect
			OpRect rect = o->GetBoundingRect().InsetBy(skin_inset);
			rect = OffsetToContainerAndScroll(ScaleToScreen(rect));

			OpRect visible_rect = VisibleRect();
			visible_rect = OffsetToContainer(visible_rect);
			rect.IntersectWith(visible_rect);

			// Add containers diff to the top container (the API should return coordinates relative to the Window).
			rect.x += container_point.x - top_container_point.x;
			rect.y += container_point.y - top_container_point.y;

			if (!rect.IsEmpty())
			{
				if (th->m_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT)
					active_region.IncludeRect(rect);

				OpRect *r = OP_NEW(OpRect, (rect));
				if (!r)
					GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				else
					all_rects.Add(r);
			}
		}

		o = (VisualDeviceOutline *) o->Suc();
	}

	if (active_region.num_rects)
	{
		active_region.CoalesceRects();
		for(int i = 0; i < active_region.num_rects; i++)
		{
			OpRect *r = OP_NEW(OpRect, (active_region.rects[i]));
			if (!r)
				GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			else
				active_rects.Add(r);
		}
	}
#endif // WIC_SEARCH_MATCHES
	return TRUE;
}

void VisualDevice::DrawTextBgHighlight(const OpRect& rect, COLORREF col, VD_TEXT_HIGHLIGHT_TYPE type)
{
	SetBgColor(col);
	DrawBgColor(rect);
#ifdef WIC_SEARCH_MATCHES
	if (type != VD_TEXT_HIGHLIGHT_TYPE_SELECTION)
	{
		OpRect visible_rect = rect;
		OpRect clip_rect;
		if (bg_cs.GetClipping(clip_rect))
		{
			clip_rect.x -= translation_x;
			clip_rect.y -= translation_y;
			visible_rect.IntersectWith(clip_rect);
		}
		if (visible_rect.IsEmpty())
			return;

		int skin_inset = 5;
#ifdef SKIN_SUPPORT
		if (OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Search Hit Active Skin"))
			skin_elm->GetMargin(&skin_inset, &skin_inset, &skin_inset, &skin_inset, 0);
#endif

		// Create outline and set VDTextHighlight type. Then the repainting including the skin_inset will
		// be guaranteed, and we can fetch all highlights later in the outline list.
		if (OpStatus::IsSuccess(BeginOutline(0, OP_RGB(0,0,0), CSS_VALUE_solid, NULL, 0)))
		{
			PushOutlineRect(visible_rect.InsetBy(-skin_inset));
			current_outline->SetTextHighlight(type);
			current_outline->SetIgnore();
			EndOutline();

#ifndef HAS_NO_SEARCHTEXT
			Window* window = GetWindow();
			WindowCommander* commander = window ? window->GetWindowCommander() : NULL;
			OpDocumentListener* doc_listener = commander ? commander->GetDocumentListener() : NULL;
			if (doc_listener)
				doc_listener->OnSearchHit(commander);
#endif // !HAS_NO_SEARCHTEXT
		}
	}
#endif // WIC_SEARCH_MATCHES
}

BOOL VisualDevice::DrawHighlightRects(OpRect *rect, int num_rects, BOOL img, HTML_Element *highlight_elm)
{
	if (!GetWindow()->DrawHighlightRects())
		return FALSE;

#ifdef SKIN_HIGHLIGHTED_ELEMENT
	if (num_rects == 0)
		return FALSE;

	// We don't really need this code as long as highlight_elm is sent. Keep it here to ensure we work with old users of DrawHighlightRects.
	// (Otherwise we might end upp adding new outlines all the time when they're not intersecting with the actual paint area)
	int i;
	for(i = 0; i < num_rects; i++)
	{
		if (doc_display_rect.Intersecting(rect[i]))
			break;
	}
	if (i == num_rects)
		return TRUE;

	// FIX: Should get thickness from the skin.
	if (OpStatus::IsSuccess(BeginOutline(4, OP_RGB(0, 0, 0), CSS_VALUE__o_highlight_border, highlight_elm)))
	{
		for(i = 0; i < num_rects; i++)
			PushOutlineRect(rect[i]);

		if (img)
			SetContentFound(DISPLAY_CONTENT_IMAGE);
		EndOutline();
	}
	return TRUE;
#else
	return FALSE;
#endif
}

#define ADD_HIGHLIGHT_PADDING(vd, rect) do { \
		int padding = MAX(1, vd->ScaleToDoc(3)); \
		rect = rect.InsetBy(-padding, -padding); \
	} while (0)

void VisualDevice::DrawHighlightRect(OpRect rect, BOOL img, HTML_Element *highlight_elm)
{
	if (!GetWindow()->DrawHighlightRects())
		return;

	if (!DrawHighlightRects(&rect, 1, img, highlight_elm))
	{
		ADD_HIGHLIGHT_PADDING(this, rect);

		InvertBorderRect(rect, INVERT_BORDER_SIZE);
	}
}

#ifdef SVG_SUPPORT
void VisualDevice::UpdateHighlightRect(OpRect rect)
{
	PadRectWithHighlight(rect);
	Update(rect.x, rect.y, rect.width, rect.height);
}
#endif // SVG_SUPPORT

void VisualDevice::PadRectWithHighlight(OpRect& rect)
{
	ADD_HIGHLIGHT_PADDING(this, rect);

#ifdef SKIN_HIGHLIGHTED_ELEMENT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Active Element Inside");
	if (skin_elm)
	{
		INT32 left, top, right, bottom;
		skin_elm->GetPadding(&left, &top, &right, &bottom, 0);
		rect.x -= left;
		rect.y -= top;
		rect.width += left + right;
		rect.height += top + bottom;
	}
#endif
}

void VisualDevice::DrawWindowBorder(BOOL invert)
{
	OP_ASSERT(painter != NULL); // Calls to painting functions should be enclosed in BeginPaint/EndPaint.
	BOOL is_frame = doc_manager && doc_manager->GetSubWinId() >= 0;
	if (is_frame)
	{
		painter->InvertBorderRect(OpRect(offset_x, offset_y, VisibleWidth(), VisibleHeight()), 1);
	}
}

/**
   Creates a font according to specifications set in for example
   SetTxtItalic and friends, and sets it on the visual device. Then
   updates the TextMetric structure in the VisualDevice so that it
   contains correct data. Only updates if the FontAtt member has been
   changed.

   @return OpStatus::OK if everything went fine and the font was changed.
   Otherwise the font is not changed.
*/
OP_STATUS VisualDevice::CheckFont()
{
	if (logfont.GetChanged() || currentFont == NULL)
	{
		logfont.ClearChanged();

		OpFont* tmpFont = NULL;

		int scale;
		if (LayoutIn100Percent())
			scale = GetLayoutScale();
		else
#ifdef CSS_TRANSFORMS
		if (HasTransform())
			scale = 100; // Always use 100% if transformed
		else
#endif // CSS_TRANSFORMS
		scale = GetScale();

		FramesDocument* doc = doc_manager ? doc_manager->GetCurrentDoc() : NULL;
		tmpFont = g_font_cache->GetFont(logfont, scale, doc);

		// GetFont will raise OOM when it knows that's why fetching failed
		if(!tmpFont)
			return OpStatus::ERR;

		if (painter && tmpFont->Type() != OpFontInfo::SVG_WEBFONT)
		{
			painter->SetFont(tmpFont);
		}

		if (currentFont)
		{
			g_font_cache->ReleaseFont(currentFont);
		}

		currentFont = tmpFont;
	}
	return OpStatus::OK;
}

void VisualDevice::SetColor(int r, int g, int b, int a)
{
	color = OP_RGBA(r,g,b,a);
	if (painter)
	{
	   painter->SetColor(r, g, b, a);
	}
}

#ifdef _PRINT_SUPPORT_
BOOL VisualDevice::HaveMonochromePrinter()
{
	PrinterInfo* printer_info = GetWindow()->GetPrinterInfo(TRUE);

	if (printer_info)
	{
		PrintDevice* printer_dev = printer_info->GetPrintDevice();

		if (printer_dev)
		{
			OpPrinterController* printer_controller = (OpPrinterController*) printer_dev->GetPrinterController();

			if (printer_controller && printer_controller->IsMonochrome())
				return TRUE;
		}
	}

	return FALSE;
}

COLORREF VisualDevice::GetVisibleColorOnWhiteBg(COLORREF color)
{
	color = HTM_Lex::GetColValByIndex(color);

	//markus fixme: use better algorithm
	BYTE r = OP_GET_R_VALUE(color);
	BYTE g = OP_GET_G_VALUE(color);
	BYTE b = OP_GET_B_VALUE(color);

	if ((r + g + b) < 440)
		return color;
	else
		return OP_RGB(0, 0, 0); // black
}
#endif // _PRINT_SUPPORT_

COLORREF VisualDevice::GetVisibleColorOnBgColor(COLORREF color, COLORREF bg_color)
{
	color = HTM_Lex::GetColValByIndex(color);
	bg_color = HTM_Lex::GetColValByIndex(bg_color);
	return GetVisibleColorOnBgColor(color, bg_color, 60);
}
UINT32 VisualDevice::GetVisibleColorOnBgColor(UINT32 color, UINT32 bg_color, UINT8 threshold)
{
	// FIX: Could make a better algorithm
	int r = OP_GET_R_VALUE(color);
	int g = OP_GET_G_VALUE(color);
	int b = OP_GET_B_VALUE(color);
	int bg_r = OP_GET_R_VALUE(bg_color);
	int bg_g = OP_GET_G_VALUE(bg_color);
	int bg_b = OP_GET_B_VALUE(bg_color);
	if (op_abs(r - bg_r) < threshold &&
		op_abs(g - bg_g) < threshold &&
		op_abs(b - bg_b) < threshold)
	{
		int bg_brightness = bg_r + bg_g + bg_b;
		if (bg_brightness > 128 * 3)
			r = g = b = 0;
		else
			r = g = b = 255;
	}
	return OP_RGB(r, g, b);
}

void VisualDevice::SetColor(COLORREF col)
{
    col = HTM_Lex::GetColValByIndex(col);

    if (!painter)
		return;

#ifdef _PRINT_SUPPORT_
	// Convert to gray scale if we are in preview mode and have a monochrome printer
	if (GetWindow() && GetWindow()->GetPreviewMode() && HaveMonochromePrinter())
	{
		col = GetGrayscale(col);
	}
#endif // _PRINT_SUPPORT_
	color = col;

	painter->SetColor(OP_GET_R_VALUE(col), OP_GET_G_VALUE(col), OP_GET_B_VALUE(col), OP_GET_A_VALUE(col));
}

void VisualDevice::SetFontStyle(int style)
{
	if (style == FONT_STYLE_ITALIC)
		logfont.SetItalic(TRUE);
	else
		if (style == FONT_STYLE_NORMAL)
			logfont.SetItalic(FALSE);
}

void VisualDevice::SetSmallCaps(BOOL small_caps)
{
	logfont.SetSmallCaps(small_caps);
}

void VisualDevice::SetFontWeight(int weight)
{
//	OP_ASSERT(weight > 0 && weight < 10);
	logfont.SetWeight(weight);
}

void VisualDevice::SetFontBlurRadius(int radius)
{
	OP_ASSERT(radius >= 0 && radius <= GLYPH_SHADOW_MAX_RADIUS);
	logfont.SetBlurRadius(radius);
}

BOOL VisualDevice::IsCurrentFontBlurCapable(int radius)
{
	OpStatus::Ignore(CheckFont());
	if (!currentFont || currentFont->Type() == OpFontInfo::SVG_WEBFONT
#ifdef CSS_TRANSFORMS
		|| HasTransform()
#endif // CSS_TRANSFORMS
		)
		return FALSE;
	return (radius <= GLYPH_SHADOW_MAX_RADIUS);
}


/** Sets the current font size, in pixels */
void VisualDevice::SetFontSize(int size)
{
#ifdef DEBUG_VIS_DEV
	VD_PrintOutI("SetFontSize()", size);
#endif
	logfont.SetSize(size);
}

void VisualDevice::SetCharSpacingExtra(int extra)
{
	char_spacing_extra = extra;
}

// To optimize theese calls, we can cache the scaled values in fontcache. But maby it isn't critical anyway.

UINT32 VisualDevice::GetFontAscent()
{
	OpStatus::Ignore(CheckFont());
	UINT32 r = currentFont ? currentFont->Ascent() : 0;
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return r;
#endif // CSS_TRANSFORMS

	return ScaleToDoc(r);
}

UINT32 VisualDevice::GetFontDescent()
{
	OpStatus::Ignore(CheckFont());
	UINT32 r = currentFont ? currentFont->Descent() : 0;
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return r;
#endif // CSS_TRANSFORMS

	return ScaleToDoc(r);
}

UINT32 VisualDevice::GetFontInternalLeading()
{
	OpStatus::Ignore(CheckFont());
	UINT32 r = currentFont ? currentFont->InternalLeading() : 0;
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);
	return ScaleToDoc(r);
}

UINT32 VisualDevice::GetFontHeight()
{
	OpStatus::Ignore(CheckFont());
	UINT32 r = currentFont ? currentFont->Height() : 0;
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return r;
#endif // CSS_TRANSFORMS

	return ScaleToDoc(r);
}

UINT32 VisualDevice::GetFontAveCharWidth()
{
	OpStatus::Ignore(CheckFont());
	UINT32 r = currentFont ? currentFont->AveCharWidth() : 0;
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return r;
#endif // CSS_TRANSFORMS

	return ScaleToDoc(r);
}

UINT32 VisualDevice::GetFontOverhang()
{
	OpStatus::Ignore(CheckFont());
	UINT32 r = currentFont ? currentFont->Overhang() : 0;
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return r;
#endif // CSS_TRANSFORMS

	return ScaleToDoc(r);
}

void VisualDevice::GetFontMetrics(short &font_ascent, short &font_descent, short &font_internal_leading, short &font_average_width)
{
	OpStatus::Ignore(CheckFont());
	if (currentFont)
	{
		font_ascent = currentFont->Ascent();
		font_descent = currentFont->Descent();
		font_internal_leading = currentFont->InternalLeading();
		font_average_width = currentFont->AveCharWidth();
		if (LayoutIn100Percent())
		{
			font_ascent = LayoutScaleToDoc(font_ascent);
			font_descent = LayoutScaleToDoc(font_descent);
			font_internal_leading = LayoutScaleToDoc(font_internal_leading);
			font_average_width = LayoutScaleToDoc(font_average_width);
			return;
		}

#ifdef CSS_TRANSFORMS
		if (HasTransform())
			return;
#endif // CSS_TRANSFORMS

		if (!IsScaled())
			return;
		font_ascent = ScaleToDoc(font_ascent);
		font_descent = ScaleToDoc(font_descent);
		font_internal_leading = ScaleToDoc(font_internal_leading);
		font_average_width = ScaleToDoc(font_average_width);
	}
	else
	{
		font_ascent = font_descent = font_internal_leading = font_average_width = 0;
	}
}

UINT32 VisualDevice::GetFontStringWidth(const uni_char* str, UINT32 len)
{
	OpStatus::Ignore(CheckFont());
	if (currentFont == NULL)
		return 0;
	UINT32 r = GetStringWidth(currentFont, str, len, painter, ScaleToScreen(char_spacing_extra), this);
	if (LayoutIn100Percent())
		return LayoutScaleToDoc(r);

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return r;
#endif // CSS_TRANSFORMS

	return ScaleToDoc(r);
}

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS VisualDevice::GetGlyphProps(const UnicodePoint up, OpFont::GlyphProps* props)
{
	RETURN_IF_ERROR(GetGlyphPropsInLocalScreenCoords(up, props));

	if (logfont.GetHeight() == 0)
	{
		/* Workaround for the fact that font engines seem to fill props with non
		   zero values in case of zero font size. Layout's ex height depends on that. */
		props->left = props->top = props->width = props->height = props->advance = 0;
		return OpStatus::OK;
	}

	if (LayoutIn100Percent())
	{
		props->left = LayoutScaleToDoc(props->left);
		props->top = LayoutScaleToDoc(props->top);
		props->width = LayoutScaleToDoc(props->width);
		props->height = LayoutScaleToDoc(props->height);
		props->advance = LayoutScaleToDoc(props->advance);
		return OpStatus::OK;
	}

#ifdef CSS_TRANSFORMS
	if (HasTransform())
		return OpStatus::OK; // props are already in doc coords.
#endif // CSS_TRANSFORMS

	props->left = ScaleToDoc(props->left);
	props->top = ScaleToDoc(props->top);
	props->width = ScaleToDoc(props->width);
	props->height = ScaleToDoc(props->height);
	props->advance = ScaleToDoc(props->advance);

	return OpStatus::OK;
}

OP_STATUS VisualDevice::GetGlyphPropsInLocalScreenCoords(const UnicodePoint up, OpFont::GlyphProps* props, BOOL fill_zeros_if_error /*= FALSE*/)
{
	OpFont* font = const_cast<OpFont*>(GetFont());
	OP_STATUS status = font ? font->GetGlyphProps(up, props) : OpStatus::ERR;

	if (OpStatus::IsError(status) && fill_zeros_if_error)
		props->left = props->top = props->width = props->height = props->advance = 0;

	return status;
}
#endif // OPFONT_GLYPH_PROPS

const UINT32 FontAtt::FA_FACESIZE = 256;

const OpFont* VisualDevice::GetFont()
{
	if (CheckFont() != OpStatus::OK)
		return NULL;

	return currentFont;
}
const FontAtt& VisualDevice::GetFontAtt() const
{
	return logfont;
}

void VisualDevice::SetFont(int font_number)
{
#ifdef DEBUG_VIS_DEV
	VD_PrintOutI("SetFont(int font_number)", font_number);
#endif
	if (current_font_number != font_number)
	{
		logfont.SetFontNumber(font_number);
		current_font_number = font_number;
	}
}

void VisualDevice::SetFont(const FontAtt& font)
{
	logfont = font;
	current_font_number = logfont.GetFontNumber();
}

OpFont* VisualDevice::CreateFont()
{
	INT32 scale;
	if (LayoutIn100Percent())
		scale = GetLayoutScale();
	else
	scale = GetScale();
	FramesDocument* doc = doc_manager ? doc_manager->GetCurrentDoc() : NULL;
	return g_font_cache->GetFont(logfont, scale, doc);
}

void VisualDevice::Reset()
{
#ifdef _SUPPORT_SMOOTH_DISPLAY_
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SmoothDisplay))
		logfont.Clear();
#endif // _SUPPORT_SMOOTH_DISPLAY_

	WritingSystem::Script script = WritingSystem::Unknown;
#ifdef FONTSWITCHING
	if (doc_manager)
	{
		FramesDocument* frm_doc = doc_manager->GetCurrentVisibleDoc();
		if (frm_doc && frm_doc->GetHLDocProfile())
			script = frm_doc->GetHLDocProfile()->GetPreferredScript();
	}
#endif // FONTSWITCHING
	Style* style = styleManager->GetStyle(HE_DOC_ROOT);
	const PresentationAttr& p_attr = style->GetPresentationAttr();
	SetFont(*(p_attr.GetPresentationFont(script).Font));

	SetColor(0, 0, 0); // initialize to black
	char_spacing_extra = 0;

	translation_x = 0;
	translation_y = 0;
}

VDState VisualDevice::PushState()
{
	VDState state;
	state.translation_x = translation_x;
	state.translation_y = translation_y;
	state.color = color;
	state.char_spacing_extra = char_spacing_extra;
	state.logfont = logfont;
	return state;
}

void VisualDevice::PopState(const VDState& state)
{
	translation_x = state.translation_x;
	translation_y = state.translation_y;
	SetColor(state.color);
	char_spacing_extra = state.char_spacing_extra;
	SetFont(state.logfont);
	logfont.SetChanged();
}

void VisualDevice::ClearTranslation()
{
#ifdef CSS_TRANSFORMS
	OP_ASSERT(!HasTransform()); // ClearTranslation() doesn't have a meaning inside transforms
#endif // CSS_TRANSFORMS

	translation_x = 0;
	translation_y = 0;
}

void VisualDevice::TranslateView(int x, int y)
{
	rendering_viewport.x += x;
	rendering_viewport.y += y;
	UpdateScaleOffset();
}

void VisualDevice::ClearViewTranslation()
{
	rendering_viewport.x = 0;
	rendering_viewport.y = 0;
	view_x_scaled = 0;
	view_y_scaled = 0;
}

void VisualDevice::GetDPI(UINT32* horizontal, UINT32* vertical)
{
	OP_ASSERT(horizontal);
	OP_ASSERT(vertical);
	OpWindow* opWindow = (GetWindow() ? GetWindow()->GetOpWindow() : NULL);
	*horizontal = m_screen_properties_cache.getHorizontalDpi(opWindow);
	*vertical = m_screen_properties_cache.getVerticalDpi(opWindow);
}

BOOL VisualDevice::InEllipse(int* coords, int x, int y)
{
	int cx = *coords++;
    int cy = *coords++;
    int radius = *coords;
    int max = radius - 1;

    x = op_abs(x - cx);
    y = op_abs(y - cy);

    if (x <= max && y <= max && x * y <= max * max >> 1)
		return TRUE;
	else
		return FALSE;
}

BOOL VisualDevice::InPolygon(int* point_array, int points, int x, int y)
{
    BOOL inside = FALSE;

    unsigned int coords = points * 2;

	if (point_array[0] == point_array[coords - 2] && point_array[1] == point_array[coords - 1])
		coords -= 2;

    if (coords < 6)
		return FALSE;

    unsigned int xold = point_array[coords - 2];
    unsigned int yold = point_array[coords - 1];

    for (unsigned int i = 0; i < coords; i += 2)
    {
		unsigned int xnew = point_array[i];
		unsigned int ynew = point_array[i + 1];
		unsigned int x1 = 0;
		unsigned int x2 = 0;
		unsigned int y1 = 0;
		unsigned int y2 = 0;

		if (xnew > xold)
		{
			x1 = xold;
			x2 = xnew;
			y1 = yold;
			y2 = ynew;
		}
		else
		{
			x1 = xnew;
			x2 = xold;
			y1 = ynew;
			y2 = yold;
		}

		if (((int)x1 < x) == (x <= (int)x2) &&         /* edge "open" at one end */
			((long) y - (long) y1) * (long) (x2 - x1) <
			((long) y2 - (long) y1) * (long) (x - x1))
			inside = !inside;

		xold = xnew;
		yold = ynew;
    }

    return inside;
}

void VisualDevice::SetScrollType(ScrollType scroll_type)
{
	this->scroll_type = scroll_type;
}

void VisualDevice::RequestScrollbars(BOOL vertical_on, BOOL horizontal_on)
{
	pending_auto_v_on = vertical_on;
	pending_auto_h_on = horizontal_on;

	UpdateScrollbars();
}

BOOL VisualDevice::StartTimer(INT32 time, BOOL restart)
{
	if (m_posted_msg_update && !restart)
		return TRUE;

	if (!g_main_message_handler->HasCallBack(this, MSG_VISDEV_UPDATE, (INTPTR) this))
		if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_VISDEV_UPDATE, (INTPTR) this)))
			return FALSE;

	RemoveMsgUpdate();
	m_posted_msg_update =
		g_main_message_handler->PostDelayedMessage(MSG_VISDEV_UPDATE, (INTPTR) this, 0, time);
	if (m_posted_msg_update)
	{
		m_post_msg_update_ms = g_op_time_info->GetRuntimeMS();
		m_current_update_delay = time;
	}
	return m_posted_msg_update;
}

void VisualDevice::StopTimer()
{
	if (!m_posted_msg_update)
		return;
	RemoveMsgUpdate();
	OP_ASSERT(g_main_message_handler->HasCallBack(this, MSG_VISDEV_UPDATE, (INTPTR) this));
	g_main_message_handler->UnsetCallBack(this, MSG_VISDEV_UPDATE, (INTPTR) this);
}

void VisualDevice::StartLoading()
{
}

void VisualDevice::DocCreated()
{
	int first_update_delay = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FirstUpdateDelay);
	TryLockForNewPage(first_update_delay);
}

void VisualDevice::StylesApplied()
{
	if (m_posted_msg_update)
	{
		int first_styled_update_delay = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::StyledFirstUpdateTimeout);
		double now = g_op_time_info->GetRuntimeMS();
		double until_styled_display = first_styled_update_delay - now + m_post_msg_update_ms;
		if (until_styled_display <= 0)
			OnTimeOut();
		else
		{
			double left_of_current_timer = m_current_update_delay - now + m_post_msg_update_ms;
			if (until_styled_display < left_of_current_timer)
				StartTimer(static_cast<INT32>(until_styled_display), TRUE);
			else
			{
				// Wait for the current timer to fire.
			}
		}
	}
	else
		OnTimeOut();
}


BOOL VisualDevice::TryLockForNewPage(int delay)
{
	if (delay == 0 || IsLocked() || GetWindow()->GetType() == WIN_TYPE_IM_VIEW)
		return FALSE;

	LockUpdate(TRUE);

	StopTimer();

	if (m_lock_count == 1)
	{
		if (doc_manager)
		{
			// Try to avoid clearing the space where a frame is if we just
			// clicked a link or something that replaces document in one
			// frame.
			FramesDocument* current_frm_doc = doc_manager->GetCurrentDoc();
			if (!current_frm_doc ||
				!current_frm_doc->IsLoaded() && !current_frm_doc->GetDocRoot())
				HideIfFrame();
		}
	}

	if (delay != INT_MAX)
		StartTimer(delay);

	return TRUE;
}

void VisualDevice::StopLoading()
{
	// We could call OnTimeOut directly, but we prefer doing it by a message. This will allow
	// onload handlers to be called first and possibly include more changes before we get the paint.
	StartTimer(0, TRUE);
}

void VisualDevice::LoadingFinished()
{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (m_accessible_doc)
		m_accessible_doc->DocumentLoaded(doc_manager->GetCurrentDoc());
#endif
}

void VisualDevice::UpdateWindowBorderPart(BOOL left, BOOL top, BOOL right, BOOL bottom)
{
	FramesDocument* doc = doc_manager->GetCurrentVisibleDoc();

	if (!doc)
		return;

	BOOL draw_frameborder = FALSE;
	DocumentManager* top_doc_man = doc_manager->GetWindow()->DocManager();
	if (
#ifdef _PRINT_SUPPORT_
		!doc_manager->GetWindow()->GetPreviewMode() &&
#endif // _PRINT_SUPPORT_
		g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ShowActiveFrame) &&
		top_doc_man)
	{
		FramesDocument* fdoc = top_doc_man->GetCurrentVisibleDoc();
		if (fdoc)
		{
			fdoc = fdoc->GetActiveSubDoc();
			if (fdoc == doc) // If this document is the active
				draw_frameborder = TRUE;
		}
	}

	INT32 edge = 1;

#ifdef SKIN_HIGHLIGHTED_ELEMENT
	// Outlines that needs update on scroll, or has skin overlapping the edge needs to be updated. (Skin is painted differently at the edge)
	VisualDeviceOutline *outline = (VisualDeviceOutline *) outlines.First();
	while (outline)
	{
		// FIX: The second case only needs to pass if the skin has "Active Element Outside"!
		if (outline->NeedUpdateOnScroll() || (outline->GetPenStyle() == CSS_VALUE__o_highlight_border && outline->IsOverlappingEdge(this)))
		{
			OpRect o_rect(outline->GetBoundingRect());
			Update(o_rect.x, o_rect.y, o_rect.width, o_rect.height);
		}
		outline = (VisualDeviceOutline *) outline->Suc();
	}
#endif // SKIN_HIGHLIGHTED_ELEMENT

	if (!draw_frameborder)
		return;

	INT32 vis_width = VisibleWidth();
	INT32 vis_height = VisibleHeight();
	OpRect rect(0, 0, vis_width, vis_height);

#ifdef DISPLAY_ONTHEFLY_DRAWING
	BOOL draw_immediately = TRUE;
	OpPainter* temp_painter = NULL;

	temp_painter = view->GetPainter(rect, TRUE);
	if (temp_painter == NULL)
		draw_immediately = FALSE;
#endif
	int left_edge = left ? edge : 0;
	int top_edge = top ? edge : 0;
	int right_edge = right ? edge : 0;
	int bottom_edge = bottom ? edge : 0;

	OpRect r[4];
	r[0].Set(vis_width - edge, top_edge, edge, vis_height - top_edge - bottom_edge);	// Right
	r[1].Set(0, top_edge, edge, vis_height - top_edge - bottom_edge);					// Left
	r[2].Set(left_edge, vis_height - edge, vis_width - left_edge - right_edge, edge);	// Bottom
	r[3].Set(left_edge, 0, vis_width - left_edge - right_edge, edge);					// Top
	BOOL *on[4] = { &right, &left, &bottom, &top };

#ifdef DISPLAY_ONTHEFLY_DRAWING
	if (draw_immediately)
	{
		for(int i = 0; i < 4; i++)
			if (*on[i])
				temp_painter->InvertRect(OffsetToContainer(r[i]));
	}
	else
#endif
	{
		for(int i = 0; i < 4; i++)
			if (*on[i])
				view->Invalidate(r[i]);
	}

#ifdef DISPLAY_ONTHEFLY_DRAWING
	if (draw_immediately && temp_painter)
		view->ReleasePainter(rect);
#endif
}

void VisualDevice::ApplyScaleRounding(INT32* val)
{
	*val = (*val / step) * step;
}

INT32 VisualDevice::ApplyScaleRoundingNearestUp(INT32 val)
{
	return ((val + step - 1) / step) * step;
}

void VisualDevice::UpdateScaleOffset()
{
	view_x_scaled = (int) (rendering_viewport.x * scale_multiplier / (double)scale_divider);
	view_y_scaled = (int) (rendering_viewport.y * scale_multiplier / (double)scale_divider);
}

void VisualDevice::SetRenderingViewPos(INT32 x, INT32 y, BOOL allow_sync, const OpRect* updated_area)
{
	if (x == rendering_viewport.x && y == rendering_viewport.y)
		return;

	// We must calculate the difference from the last actual position with precision (double because the values might be huge too) to
	// detect the 'extra' pixels we have to scroll in f.ex scale 110%. (In that case we would normally scroll 1px but sometimes 2px)
	OpRect old_rendering_viewport = rendering_viewport;
	double old_pixel_x = rendering_viewport.x * scale_multiplier / (double)scale_divider;
	double new_pixel_x = x * scale_multiplier / (double)scale_divider;
	double old_pixel_y = rendering_viewport.y * scale_multiplier / (double)scale_divider;
	double new_pixel_y = y * scale_multiplier / (double)scale_divider;
	int dx = (int)old_pixel_x - (int)new_pixel_x;
	int dy = (int)old_pixel_y - (int)new_pixel_y;

	rendering_viewport.x = x;
	rendering_viewport.y = y;

	BOOL changed = dx || dy;

	// update scrollbar positions
	if (h_scroll)
		h_scroll->SetValue(x);

	if (v_scroll)
		v_scroll->SetValue(y);

	if (!changed)
		return;

	UpdateScaleOffset();

	if (!doc_manager)
		return;

	FramesDocument* doc = doc_manager->GetCurrentVisibleDoc();

	if (!doc)
		return;

	doc->OnRenderingViewportChanged(rendering_viewport);

	if (doc->IsFrameDoc())
		if (!doc->GetSmartFrames() && !doc->GetFramesStacked())
			return;

	if (view && (doc_width || doc_height))
	{
		UpdateOffset();

#ifdef _PLUGIN_SUPPORT_
		coreview_clipper.Scroll(dx, dy);
#endif // _PLUGIN_SUPPORT_

		UpdateWindowBorderPart(dx > 0, dy > 0, dx < 0, dy < 0);

		if (updated_area && !updated_area->IsEmpty())
		{
			// FIX: Scrolling with fixed content in zoomed mode currently update everything.
			//      This code, code in layout (regarding updated_area) and
			//      VisualDevice::ScrollRect should be fixed.

			OpRect scroll_area(0, 0, rendering_viewport.width, rendering_viewport.height);
			OpRect update_rect(*updated_area);
			update_rect.OffsetBy(-old_rendering_viewport.x, -old_rendering_viewport.y);

			// Create vertical "scroll-slices" around the area we update.
			//
			// Optimization idea: If the total scrolled area is almost the entire view, and we know that the backend scrolls the
			// backbuffer (isn't using hwd scroll "on-screen"), we could optimize by just scrolling all in one go. That would
			// probably leave less invalidated areas scattered around due to the scroll operations.
			//
			if (update_rect.x > scroll_area.x || update_rect.Right() < scroll_area.Right())
			{
				// Doing scrolls of multiple vertical slices is also costly and if they are very thin, we're
				// probably not gaining much by scrolling it.
				// If the horizontally extended area isn't very big, grow update_rect to it so the side-slices
				// won't be needed.
				update_rect.IntersectWith(scroll_area);
				OpRect tmp_area(scroll_area.x, update_rect.y, scroll_area.width, update_rect.height);
				if (tmp_area.width * tmp_area.height < scroll_area.width * scroll_area.height / 10)
					update_rect = tmp_area;
			}
			OpRect top(update_rect.x, scroll_area.y, update_rect.width, update_rect.y - scroll_area.y);
			OpRect bottom(update_rect.x, update_rect.Bottom(), update_rect.width, scroll_area.Bottom() - update_rect.Bottom());
			OpRect left(scroll_area.x, scroll_area.y, update_rect.x - scroll_area.x, scroll_area.height);
			OpRect right(update_rect.Right(), scroll_area.y, scroll_area.Right() - update_rect.Right(), scroll_area.height);

			// Update the fixed area since we're not going to scroll it.
			update_rect.OffsetBy(rendering_viewport.x, rendering_viewport.y);
			Update(update_rect.x, update_rect.y, update_rect.width, update_rect.height);

			view->MoveChildren(dx, dy, TRUE);

			if (!top.IsEmpty())
				ScrollRect(top, dx, dy);

			if (!bottom.IsEmpty())
				ScrollRect(bottom, dx, dy);

			if (!left.IsEmpty())
				ScrollRect(left, dx, dy);

			if (!right.IsEmpty())
				ScrollRect(right, dx, dy);
		}
		else
		{
			CheckOverlapped();
			view->Scroll(dx, dy);
		}

		UpdateWindowBorderPart(dx < 0, dy < 0, dx > 0, dy > 0);
	}

	BOOL is_reflowing = doc->IsReflowing();

	// Check if we are painting already. Then we should not call sync because we could get nestled OnPaint.
	// FIX: Theese checks can probably be removed now when CoreViewContainer has BeginPaintLock and EndPaintLock
	//      but keeping them for a while. /emil
	if (!painter && !is_reflowing && allow_sync && view)
	{
		if (GetContainerView() && GetContainerView()->GetOpView() != view->GetOpView())
			GetContainerView()->Sync();

		view->Sync();
	}

	Window* window = GetWindow();

#ifdef _SPAT_NAV_SUPPORT_
	if (window->GetSnHandler())
		window->GetSnHandler()->OnScroll();
#endif // _SPAT_NAV_SUPPORT_

	if (window->GetWindowCommander() && window->GetWindowCommander()->GetDocumentListener())
		window->GetWindowCommander()->GetDocumentListener()->OnDocumentScroll(window->GetWindowCommander());

	if (view)
		view->NotifyScrollListeners(dx, dy, allow_sync ? CoreViewScrollListener::SCROLL_REASON_USER_INPUT : CoreViewScrollListener::SCROLL_REASON_UNKNOWN, FALSE);

#ifdef WIDGETS_IME_SUPPORT
	if (g_opera->widgets_module.im_listener->IsIMEActive())
		// Will make the widget update its position and trig OnMove so the system is informed about the new position.
		g_opera->widgets_module.im_listener->GetWidgetInfo();
#endif

	// When reflowing, visual device can get smaller then doc_height temporarily (when sizing down document) thus
	// windows like IRC will stop scrolling automatically. To fix that problem, we will always trigger autoscroll
	// when reflowing regardless of document and viewport height.
	m_autoscroll_vertically = is_reflowing || (rendering_viewport.y + rendering_viewport.height >= doc_height);

#ifndef MOUSELESS
	if (view && !painter && !is_reflowing && allow_sync)
	{
		// Post message to emulate a mousemove on the new scrollposition.
		// Should not be done immediately for both performance and stability reasons.
		// If already posted, we delay it until later.
		RemoveMsgMouseMove();

		if (!g_main_message_handler->HasCallBack(this, MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this))
			if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this)))
				return;

		m_posted_msg_update =
			g_main_message_handler->PostDelayedMessage(MSG_VISDEV_EMULATE_MOUSEMOVE, (INTPTR) this, 0, 200);
	}
#endif
}

void VisualDevice::SetDocumentSize(UINT32 w, UINT32 h, int negative_overflow)
{
	BOOL changed = (doc_width != int(w) || doc_height != long(h));
	doc_width = w;
	doc_height = h;
	this->negative_overflow = negative_overflow;
	INT32 scaled_client_width = ScaleToDoc(VisibleWidth());
	INT32 scaled_client_height = ScaleToDoc(VisibleHeight());
	if (v_scroll)
		v_scroll->SetLimit(0, doc_height - scaled_client_height, scaled_client_height);
	if (h_scroll)
		h_scroll->SetLimit(-negative_overflow, doc_width - scaled_client_width, scaled_client_width);

	if (changed)
	{
		if (h_scroll)
			h_scroll->SetValue(rendering_viewport.x);
		if (v_scroll)
			v_scroll->SetValue(rendering_viewport.y);
	}
}

void VisualDevice::MoveScrollbars()
{
	if (v_scroll == NULL || !doc_manager || !corner) // Not initiated.
		return;

	// Update colors... Fix: move
	BOOL use_rtl = LeftHandScrollbar();

#ifdef CSS_SCROLLBARS_SUPPORT
	FramesDocument* frames_doc = doc_manager->GetCurrentVisibleDoc();
	if (frames_doc && !frames_doc->IsFrameDoc() &&
	    g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableScrollbarColors, frames_doc->GetHostName()))
	{
		if (frames_doc->GetHLDocProfile())
		{
			ScrollbarColors* color = frames_doc->GetHLDocProfile()->GetScrollbarColors();
			v_scroll->SetScrollbarColors(color);
			h_scroll->SetScrollbarColors(color);
			corner->SetScrollbarColors(color);
		}
	}
#endif // CSS_SCROLLBARS_SUPPORT

	// FIXME: Why are we assuming that the width of vertical and horizontal scrollbar is the same?

	int scr_size = v_scroll ? v_scroll->GetInfo()->GetVerticalScrollbarWidth() : g_op_ui_info->GetVerticalScrollbarWidth();
	int cx = use_rtl ? 0 : WinWidth() - scr_size;
	int cy = WinHeight() - scr_size;

	int window_width, window_height;
	GetWindow()->GetClientSize(window_width, window_height);

	if (doc_manager->GetWindow()->GetType() == WIN_TYPE_JS_CONSOLE ||
		doc_manager->GetWindow()->GetType() == WIN_TYPE_BRAND_VIEW || use_rtl)
	{
		corner->SetActive(FALSE);
	}
	else
	{
		OpRect win_rect(0, 0, win_width, win_height);
		win_pos.Apply(win_rect);
		BOOL lower_right = (win_rect.Right() >= window_width && win_rect.Bottom() >= window_height);
		corner->SetActive(lower_right); // FIXME: This happens for iframes too.
	}

	int hsize = h_on ? scr_size : 0;
	int vsize = v_on ? scr_size : 0;

#ifdef _MACINTOSH_
	// Mac always has the resizecorner visible since there's no other way of resizing windows.
	corner->SetRect(OpRect(cx, cy, scr_size, scr_size), TRUE);

	OpPoint deltas(0,0);
	if (GetWindow()->GetOpWindow()->GetRootWindow()->GetStyle() != OpWindow::STYLE_BITMAP_WINDOW)
		deltas = ((CocoaOpWindow*)GetWindow()->GetOpWindow()->GetRootWindow())->IntersectsGrowRegion(corner->GetScreenRect());

	v_scroll->SetRect(OpRect(cx, 0, scr_size, WinHeight() - (deltas.y > 0 ? deltas.y : hsize)), TRUE);
	h_scroll->SetRect(OpRect(use_rtl ? scr_size : 0, cy, WinWidth() - (deltas.x > 0 ? deltas.x : vsize), scr_size), TRUE);

	corner->SetVisibility((h_on && v_on) || ((h_on || v_on) && (deltas.x || deltas.y)));

#else
	v_scroll->SetRect(OpRect(cx, 0, scr_size, WinHeight() - hsize), TRUE);
	h_scroll->SetRect(OpRect(use_rtl ? vsize : 0, cy, WinWidth() - vsize, scr_size), TRUE);
	corner->SetRect(OpRect(cx, cy, scr_size, scr_size), TRUE);
#endif // _MACINTOSH_
}

void VisualDevice::InvalidateScrollbars()
{
	if (view && container && corner)
	{
		v_scroll->Invalidate(v_scroll->GetBounds());
		h_scroll->Invalidate(h_scroll->GetBounds());
		corner->Invalidate(corner->GetBounds());
	}
}

void VisualDevice::UpdateScrollbars()
{
	if (v_scroll == NULL || !corner) // Not initiated.
		return;

	BOOL old_h_on = h_on, old_v_on = v_on;
	int client_width = VisibleWidth();
	int client_height = VisibleHeight();

	if (scroll_type == VD_SCROLLING_AUTO)
	{
		h_on = pending_auto_h_on;
		v_on = pending_auto_v_on;
	}
	else
		h_on = v_on = scroll_type == VD_SCROLLING_YES;

#ifndef _MACINTOSH_
// TODO: this should better be a tweak, which is enabled by default
// and disabled for _MACINTOSH_ or the tweak could be handled inside
// the corner implementation:
	corner->SetVisibility(h_on && v_on);
#endif // _MACINTOSH_
	v_scroll->SetVisibility(v_on);
	h_scroll->SetVisibility(h_on);

	// Update scrollbars limits
	INT32 scaled_client_width = ScaleToDoc(client_width);
	INT32 scaled_client_height = ScaleToDoc(client_height);

	int max_v = doc_height - scaled_client_height;
	int max_h = doc_width - scaled_client_width;
	v_scroll->SetLimit(0, max_v, scaled_client_height);
	h_scroll->SetLimit(-negative_overflow, max_h, scaled_client_width);
	v_scroll->SetSteps(DISPLAY_SCROLL_STEPSIZE, GetPageScrollAmount(scaled_client_height));
	h_scroll->SetSteps(DISPLAY_SCROLL_STEPSIZE, GetPageScrollAmount(scaled_client_width));

	// Resize the view
	ResizeViews();

	if (!view->GetVisibility())
		view->SetVisibility(TRUE);

	if (v_on != old_v_on || h_on != old_h_on)
	{
		UpdatePopupProgress();
	}

	if (GetShouldAutoscrollVertically() && doc_manager != 0)
		if (FramesDocument* doc = doc_manager->GetCurrentVisibleDoc())
			ScrollDocument(doc, OpInputAction::ACTION_GO_TO_END, 1, OpInputAction::METHOD_OTHER);
}

OpPoint VisualDevice::GetInnerPosition()
{
	AffinePos pos;
	view->GetPos(&pos);
#ifdef CSS_TRANSFORMS
	// Document's view and its scrollbars are in the same coordinate system.
	OP_ASSERT(!pos.IsTransform());
#endif // CSS_TRANSFORMS
	return pos.GetTranslation();
}

int VisualDevice::GetScreenAvailHeight()
{
	return m_screen_properties_cache.getProperties((GetWindow() ? GetWindow()->GetOpWindow() : NULL))->workspace_rect.height;
}

int VisualDevice::GetScreenAvailWidth()
{
	return m_screen_properties_cache.getProperties((GetWindow() ? GetWindow()->GetOpWindow() : NULL))->workspace_rect.width;
}

int VisualDevice::GetScreenColorDepth()
{
	OpScreenProperties *sp = m_screen_properties_cache.getProperties((GetWindow() ? GetWindow()->GetOpWindow() : NULL));
	return sp->bits_per_pixel * sp->number_of_bitplanes;
}

int VisualDevice::GetScreenPixelDepth()
{
	return m_screen_properties_cache.getProperties((GetWindow() ? GetWindow()->GetOpWindow() : NULL))->bits_per_pixel;
}

int VisualDevice::GetScreenHeight()
{
	return m_screen_properties_cache.getProperties((GetWindow() ? GetWindow()->GetOpWindow() : NULL))->screen_rect.height;
}

int VisualDevice::GetScreenHeightCSS()
{
	int tmp_height = GetScreenHeight();
	if (GetWindow()->GetTrueZoom())
	{
		tmp_height= LayoutScaleToDoc(tmp_height);
	}
	return tmp_height;
}

int VisualDevice::GetScreenWidth()
{
	return m_screen_properties_cache.getProperties((GetWindow() ? GetWindow()->GetOpWindow() : NULL))->screen_rect.width;
}

int VisualDevice::GetScreenWidthCSS()
{
	int tmp_width = GetScreenWidth();
	if (GetWindow()->GetTrueZoom())
	{
		tmp_width = LayoutScaleToDoc(tmp_width);
	}
	return tmp_width;
}

OpPoint VisualDevice::GetPosOnScreen()
{
	OpPoint point;
	if (view)
		point = view->ConvertToScreen(point);
	return point;
}

int VisualDevice::GetRenderingViewWidth() const
{
	return rendering_viewport.width;
}

int VisualDevice::GetRenderingViewHeight() const
{
	return rendering_viewport.height;
}

int VisualDevice::VisibleWidth()
{
	int w = WinWidth() - GetVerticalScrollbarSpaceOccupied();

	if (w < 0)
		return 0;
	else
		return w;
}

int VisualDevice::VisibleHeight()
{
	int h = WinHeight() - GetHorizontalScrollbarSpaceOccupied();

	if (h < 0)
		return 0;
	else
		return h;
}

int VisualDevice::GetVerticalScrollbarSize() const
{
	return v_scroll ? v_scroll->GetInfo()->GetVerticalScrollbarWidth() : g_op_ui_info->GetVerticalScrollbarWidth();
}

int VisualDevice::GetHorizontalScrollbarSize() const
{
	return h_scroll ? h_scroll->GetInfo()->GetHorizontalScrollbarHeight() : g_op_ui_info->GetHorizontalScrollbarHeight();
}

void VisualDevice::SetFocus(FOCUS_REASON reason)
{
#ifdef DOCUMENT_EDIT_SUPPORT
	if (doc_manager)
	{
		if (FramesDocument* frames_doc = doc_manager->GetCurrentVisibleDoc())
		{
			BOOL focus = frames_doc->GetDesignMode();
			if (!focus)
			{
				if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
				{
					HTML_Element *elm = logdoc->GetBodyElm() ? logdoc->GetBodyElm() : logdoc->GetDocRoot();
					focus = elm && elm->IsContentEditable(TRUE);
				}

			}
			if (focus && frames_doc->GetDocumentEdit())
			{
				frames_doc->GetDocumentEdit()->SetFocus(reason);
				return;
			}
		}
	}
#endif
	OpInputContext::SetFocus(reason);
}

void VisualDevice::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	DocumentManager* doc_man = GetDocumentManager();
	FramesDocument* doc = doc_man->GetCurrentVisibleDoc();

	// Get old_input_context from inputmanager since it may have been deleted in other call to OnKeyboardInputGained.
	old_input_context = g_input_manager->GetOldKeyboardInputContext();

	if (doc && !IsParentInputContextOf(old_input_context))
		doc->GotKeyFocus(reason);
		// Force hoover recalculation only when document is being activated. Hoover position can
		// go out of sync with actual mouse pointer position only when document is inactive.

	if (this != new_input_context)
	{
		if (new_input_context->GetInputContextName() == GetInputContextName())
			/* New input context is a VisualDevice and we are in its ancestor
			   VisualDevice. */
			return;
		else
		{
			OpInputContext* parent_of_new_input_ctx = new_input_context->GetParentInputContext();
			OP_ASSERT(parent_of_new_input_ctx);

			if (this != parent_of_new_input_ctx && parent_of_new_input_ctx->GetInputContextName() == GetInputContextName())
				/* If the parent of a new input ctx is a VisualDevice (e.g. DocumentEdit case)
				   and we are in the ancestor VisualDevice of the one being parent of the
				   new input ctx, return now also. */
				return;
		}
	}

	// Active frame must be set even if it was a child-inputcontext that gained focus.
	if (doc_man->GetFrame())
	{
		if (doc)
		{
			// Keyboardfocus must be locked when SetActiveFrame is called so it won't steal back focus from children.
			g_input_manager->LockKeyboardInputContext(TRUE);

			doc->GetTopDocument()->SetActiveFrame(doc_man->GetFrame(), this == new_input_context);

			g_input_manager->LockKeyboardInputContext(FALSE);

			// Wand usability may change when we focus another frame so we must update of toolbar.
			g_input_manager->UpdateAllInputStates();
		}
	}

	if (this != new_input_context)
		return;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

void VisualDevice::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
    FramesDocument* doc = doc_manager ? doc_manager->GetCurrentVisibleDoc() : NULL;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (GetAccessibleDocument()) {
		GetAccessibleDocument()->HighlightElement(NULL);
	}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	if (doc && !(this == new_input_context || this->IsParentInputContextOf(new_input_context)))
		doc->LostKeyFocus();
}

BOOL VisualDevice::IsInputContextAvailable(FOCUS_REASON reason)
{
	return view && doc_manager && doc_manager->GetWindow()->HasFeature(WIN_FEATURE_FOCUSABLE) && ((doc_manager->GetCurrentDoc() || doc_manager->GetWindow()->IsLoading()) || (reason != FOCUS_REASON_MOUSE && reason != FOCUS_REASON_KEYBOARD));
}

void VisualDevice::OnVisibilityChanged(BOOL vis)
{
	if (!vis)
	{
		OP_DELETE(m_cachedBB);
		m_cachedBB = NULL;

		box_shadow_corners.Clear();
	}
}

static void InternalHide(const void* el, void* cv)
{
	((CoreView*)cv)->SetVisibility(FALSE);
}

static OP_STATUS FindVisibleCoreViews(CoreView *cv, OpPointerHashTable<HTML_Element,CoreView> &core_views, OpPointerSet<HTML_Element> &fixed_positioned_subtrees)
{
	while(cv)
	{
		OpRect r = cv->GetVisibleRect();

		HTML_Element* elm = NULL;

		if (cv->GetVisibility() && !r.IsEmpty())  // view is visible inside this frame/screen
		{
			elm = cv->GetOwningHtmlElement();

			if (elm && core_views.Add(elm,cv) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;
			if (cv->GetFixedPositionSubtree() && fixed_positioned_subtrees.Add(cv->GetFixedPositionSubtree()) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;
		}

		Box *box = elm ? elm->GetLayoutBox() : NULL;

		if (cv->GetFirstChild() && box && box->GetScrollable())
			if (FindVisibleCoreViews(cv->GetFirstChild(), core_views, fixed_positioned_subtrees) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;

		cv = (CoreView*)cv->Suc();
	}

	return OpStatus::OK;
}

OP_STATUS VisualDevice::CheckCoreViewVisibility()
{
	FramesDocument* doc = GetDocumentManager()->GetCurrentVisibleDoc();
	if (!doc || !doc->GetDocRoot() || !GetView())
		return OpStatus::OK;

	/* May only be run directly after reflow */
	OP_ASSERT(!doc->GetDocRoot()->IsDirty());

	OpPointerHashTable<HTML_Element,CoreView> core_views;
	OpPointerSet<HTML_Element> fixed_position_subtrees;

	CoreView* cv = GetView()->GetFirstChild();

	if (cv && FindVisibleCoreViews(cv, core_views, fixed_position_subtrees) == OpStatus::ERR_NO_MEMORY)
	{
		core_views.RemoveAll();
		fixed_position_subtrees.RemoveAll();
		return OpStatus::ERR_NO_MEMORY;
	}

	if (core_views.GetCount()) // We have coreviews inside the view...
	{
		if (GetWindow()->IsVisibleOnScreen())
		{
			OpRect rect = GetVisibleDocumentRect();
			rect.OffsetBy(-rendering_viewport.x, -rendering_viewport.y);

			if (CoreViewFinder::TraverseWithFixedPositioned(doc, rect, core_views, fixed_position_subtrees) == OpStatus::ERR_NO_MEMORY)
			{
				core_views.RemoveAll();
				fixed_position_subtrees.RemoveAll();
				return OpStatus::ERR_NO_MEMORY;
			}

			/* Successful CoreViewFinder traverse implies reaching all the fixed positioned
			   subtrees. */
			OP_ASSERT(fixed_position_subtrees.GetCount() == 0);
		}
		else
			fixed_position_subtrees.RemoveAll();

		/* Hide all CoreViews that are in view but should not be visible anymore. */
		core_views.ForEach(&InternalHide);
		core_views.RemoveAll();
	}
	return OpStatus::OK;
}

void VisualDevice::ScreenPropertiesHaveChanged()
{
	m_screen_properties_cache.markPropertiesAsDirty();
}

void VisualDevice::OnMoved()
{
	if (h_scroll)
	  h_scroll->OnMove();
	if (v_scroll)
	  v_scroll->OnMove();
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

AccessibleDocument* VisualDevice::GetAccessibleDocument()
{
	if (!m_accessible_doc)
	{
		if (doc_manager && doc_manager->GetCurrentVisibleDoc())
		{
			m_accessible_doc = OP_NEW(AccessibleDocument, (this, doc_manager));
		}
	}
	return m_accessible_doc;
}

BOOL VisualDevice::AccessibilitySetFocus()
{
	SetFocus(FOCUS_REASON_OTHER);
	return FALSE;
}

OP_STATUS VisualDevice::AccessibilityGetText(OpString& str)
{
	str.Empty();
	return OpStatus::OK;
}

OP_STATUS VisualDevice::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	rect.Set(0, 0, win_width, win_height);

	OpPoint point = GetPosOnScreen();
	rect.OffsetBy(point);

	return OpStatus::OK;
}

Accessibility::ElementKind VisualDevice::AccessibilityGetRole() const
{
	return Accessibility::kElementKindScrollview;
}

Accessibility::State VisualDevice::AccessibilityGetState()
{
	Accessibility::State state=Accessibility::kAccessibilityStateNone;
	if (!GetVisible())
		state |= Accessibility::kAccessibilityStateInvisible;
	if (this == g_input_manager->GetKeyboardInputContext())
		state |= Accessibility::kAccessibilityStateFocused;
	return state;
}

int VisualDevice::GetAccessibleChildrenCount()
{
	int count = 0;
	if (v_on && v_scroll)
		count++;
	if (h_on && h_scroll)
		count++;
	if (doc_manager->GetCurrentVisibleDoc())
		count++;
	return count;
}

OpAccessibleItem* VisualDevice::GetAccessibleParent()
{
	OpInputContext* parent_input_context = GetParentInputContext();
	if (!parent_input_context)
		return NULL;
	//If the parent input context is a widget, it is also an accessible item -> our parent.
	if (parent_input_context->GetType() >= OpTypedObject::WIDGET_TYPE && parent_input_context->GetType() < OpTypedObject::WIDGET_TYPE_LAST)
	{
		OpWidget* parent = (OpWidget*)parent_input_context;
		if (parent->AccessibilitySkips())
			return parent->GetAccessibleParent();
	}
	if (parent_input_context->GetType() == OpTypedObject::VISUALDEVICE_TYPE)
		return ((VisualDevice*)parent_input_context)->GetAccessibleDocument();
	else
		return NULL;
}

OpAccessibleItem* VisualDevice::GetAccessibleChild(int index)
{
	if (v_on && v_scroll) {
		if (index == 0)
			return v_scroll;
		index--;
	}
	if (h_on && h_scroll) {
		if (index == 0)
			return h_scroll;
		index--;
	}
	if (doc_manager && doc_manager->GetCurrentVisibleDoc()) {
		if (index == 0 && GetAccessibleDocument())
			return GetAccessibleDocument();
		index--;
	}
	return NULL;
}

int VisualDevice::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = 0;
	if (v_on && v_scroll)
	{
		if (child == v_scroll)
			return index;
		index ++;
	}
	if (h_on && h_scroll)
	{
		if (child == h_scroll)
			return index;
		index ++;
	}
	if (doc_manager && doc_manager->GetCurrentDoc()) {
		if (child == GetAccessibleDocument())
			return index;
	}
	return Accessibility::NoSuchChild;
}

OpAccessibleItem* VisualDevice::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpPoint point = GetPosOnScreen();

	point.x = x - point.x;
	point.y = y - point.y;

	OpRect rect;
	if (v_on && v_scroll)
	{
		rect = v_scroll->GetRect();
		if (rect.Contains(point))
			return v_scroll;
	}
	if (h_on && h_scroll)
	{
		rect = h_scroll->GetRect();
		if (rect.Contains(point))
			return h_scroll;
	}

	if (point.x >= 0 && point.x < win_width &&
		point.y >= 0 && point.y < win_height)
	{
		if (GetAccessibleDocument())
			return GetAccessibleDocument();
	}

	return NULL;
}

OpAccessibleItem* VisualDevice::GetNextAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem* VisualDevice::GetPreviousAccessibleSibling()
{
	return NULL;
}

OpAccessibleItem* VisualDevice::GetAccessibleFocusedChildOrSelf()
{
	OpInputContext* focused = g_input_manager->GetKeyboardInputContext();
	while (focused)
	{
		if (focused == this) {
			if (GetAccessibleDocument())
				return GetAccessibleDocument();
			return this;
		}
		focused = focused->GetParentInputContext();
	}
	return NULL;
}

OpAccessibleItem* VisualDevice::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* VisualDevice::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* VisualDevice::GetDownAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* VisualDevice::GetUpAccessibleObject()
{
	return NULL;
}

OP_STATUS VisualDevice::GetAbsoluteViewBounds(OpRect& bounds)
{
	bounds.Set(0, 0, VisibleWidth(), VisibleHeight());

	OpPoint point = GetPosOnScreen();
	bounds.OffsetBy(point);

	return OpStatus::OK;
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

UINT8 VisualDevice::PreMulBBAlpha()
{
	UINT8 alpha = 255;
	for (VisualDeviceBackBuffer* bb = static_cast<VisualDeviceBackBuffer*>(backbuffers.Last());
		 bb && bb->oom_fallback;
		 bb = static_cast<VisualDeviceBackBuffer*>(bb->Pred()))
	{
		OP_ASSERT(!bb->bitmap);
		OP_ASSERT(bb->opacity != 255);
		alpha = static_cast<UINT8>((static_cast<UINT32>(alpha+1) * static_cast<UINT32>(bb->opacity)) >> 8);
	}
	return alpha;
}

#ifdef EXTENSION_SUPPORT
void
VisualDevice::PaintWithExclusion(OpPainter* painter, OpRect& rect, OpVector<VisualDevice>& exclude_list)
{
	for (UINT32 i = 0; i < exclude_list.GetCount(); ++i)
	{
		if (exclude_list.Get(i)->GetView()->GetWantPaintEvents())
			exclude_list.Get(i)->GetView()->SetWantPaintEvents(FALSE);
		else
			exclude_list.Replace(i, NULL);
	}

	GetContainerView()->Paint(rect, painter, 0, 0, TRUE, FALSE);

	for (UINT32 i = 0; i < exclude_list.GetCount(); ++i)
	{
		if (exclude_list.Get(i))
			exclude_list.Get(i)->GetView()->SetWantPaintEvents(TRUE);
	}
}
#endif // EXTENSION_SUPPORT
