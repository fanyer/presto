/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/WindowsComplexScript.h"
#include "platforms/windows/WindowsVegaPrinterListener.h"
#include "platforms/windows/user_fun.h"
#include "platforms/windows/pi/GdiOpFont.h"
#include "platforms/windows/pi/GdiOpFontManager.h"

#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_path.h"

extern HTHEME g_window_theme_handle;

// Default overhang for synthesized fonts which report 0 overhang.
#define DEFAULT_OVERHANG 1.3

#define DTT_GLOWSIZE        (1UL << 11)
#define DTT_COMPOSITED      (1UL << 13)

//
// class GdiOpFont
//
	
/*static*/ GdiOpFont *GdiOpFont::s_font_on_dc = 0;
/*static*/ HDC GdiOpFont::s_fonthdc = 0;
/*static*/ HBITMAP GdiOpFont::s_text_bitmap = NULL;
/*static*/ HBITMAP GdiOpFont::s_old_text_bitmap = NULL;
/*static*/ HDC GdiOpFont::s_text_bitmap_dc = NULL;
/*static*/ UINT8* GdiOpFont::s_text_bitmap_data;
/*static*/ LONG GdiOpFont::s_text_bitmap_width = 0;
/*static*/ LONG GdiOpFont::s_text_bitmap_height = 0;

GdiOpFont::GdiOpFont(GdiOpFontManager* font_manager, const uni_char *face, UINT32 size, UINT8 weight, BOOL italic, INT32 blur_radius)
	: vega_script_cache(NULL)
	 ,m_font_manager(font_manager)
	 ,m_blur_radius(blur_radius)
{
	//Fix for bug DSK-235152. Force some chinese fonts to use a larger size (otherwise, they're unreadable)
	if (uni_strnicmp(face, UNI_L("SimSun"), 6) == 0 ||
		uni_strnicmp(face, UNI_L("NSimSun"), 7) == 0 ||
		uni_strnicmp(face, UNI_L("\x5B8B\x4F53"), 4) == 0 ||
		uni_strnicmp(face, UNI_L("\x65B0\x5B8B\x4F53"), 6) == 0)
		if(size >= 9) // if size is less than 9 core is probably trying to paint a thumbnail! hack for DSK-250578
			size = MAX(size, 12);
	m_status = OpStatus::OK;

	INT32 nHeight = size;

	INT nWeight=INT(weight*(1000.0/10.0));

#ifdef FONTCACHETEST
	FontCacheTest* current = OP_NEW(FontCacheTest, (face, size, weight, italic));
	BOOL found = FALSE;

	FontCacheTest* font = (FontCacheTest*)fontcache.First();
	while (font)
	{
		if (current->Equals(*font))
		{
			found = TRUE;
			break;
		}
		font = (FontCacheTest*)font->Suc();
	}

	if (found)
	{
		font->IncUseCount();		
		OP_DELETE(current);
	}
	else
	{		
		current->Into(&fontcache);			
	}	
#endif // FONTCACHETEST
	BOOL acid3_workaround = FALSE;

	if(!nHeight)
	{
		// workaround for CORE-16527
		nHeight = -1;
		acid3_workaround = TRUE;
	}

	//Get some information.. (Ascent,descent,height...)
	TEXTMETRIC met;
	ZeroMemory(&met, sizeof(met));
	
	for(int i=0; i<2; i++)
	{
		m_fnt = CreateFont(
			(int)-nHeight,	// logical height of font 
			0,	// logical average character width 
			0,	// angle of escapement 
			0,	// base-line orientation angle 
			nWeight,italic,FALSE,FALSE,
			DEFAULT_CHARSET,	// character set identifier 
			OUT_DEFAULT_PRECIS	,	// output precision 
			CLIP_DEFAULT_PRECIS	,	// clipping precision 
			DEFAULT_QUALITY	,	// output quality 
			FF_DONTCARE	,	// pitch and family 
			face 	// address of typeface name string 
		   );

		if(m_fnt == NULL)
		{
			m_status = OpStatus::ERR;
			return;
		}
		
		m_oldGdiObj = SelectObject(s_fonthdc, m_fnt);

		GetTextMetrics(s_fonthdc,&met);
		
		// hack for DSK-250578
		// switch to a scalable font in case small size of bitmap font is required(which can't be fulfilled)
		if ( !(met.tmPitchAndFamily&TMPF_VECTOR) && met.tmHeight-met.tmInternalLeading > nHeight + 2  )
		{
			SelectObject(s_fonthdc, (HFONT)GetStockObject(SYSTEM_FONT));
			DeleteObject(m_fnt);
			m_fnt = NULL;
			face = GdiOpFontManager::GetLatinGenericFont(GENERIC_FONT_SERIF);
		}
		else
			break;
	}

	OP_NEW_DBG("CreateFont", "fonts");
	OP_DBG((UNI_L("face: %s"), face));

	s_font_on_dc = this;

	m_ascent = met.tmAscent;
	m_descent = met.tmDescent;
	if(acid3_workaround)
	{
		// workaround for CORE-16527
		m_ascent = m_descent = 1;
	}
	m_overhang=met.tmOverhang;
	m_internal_leading=met.tmInternalLeading;
	m_italic=italic;
	m_smallcaps=FALSE; //fix
	m_weight=weight;
	m_ave_charwidth=met.tmAveCharWidth;
	m_vectorfont=(met.tmPitchAndFamily & TMPF_VECTOR) ? TRUE : FALSE;
	m_height = met.tmHeight;

#ifdef VEGA_GDIFONT_SUPPORT
	m_max_advance = met.tmMaxCharWidth + m_overhang + 2; // 2 extra pixels for cleartype fringes

	// Metrics for some fonts like Lucida Grande does not reflect the extra char width
	// when it is synthesized, so multiply it by a factor, to make sure we draw it all.
	if (m_italic && m_overhang == 0)
		m_max_advance *= DEFAULT_OVERHANG;
#endif // VEGA_GDIFONT_SUPPORT

	//Init the widthcache...
	/*if(s1.cx==s2.cx)
		InitCache(s1.cx); //Monospace, the width is s1.cx
	else
		InitCache(0); //Not monospace, run GetWidths*/
#ifdef VEGA_GDIFONT_SUPPORT
	BITMAPINFO bi;
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = m_max_advance;
	bi.bmiHeader.biHeight = -(int)m_height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = 0;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	m_bmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&m_bmp_data, NULL, 0);
	if (!m_bmp)
	{
		m_status = OpStatus::ERR_NO_MEMORY;
		return;
	}

	HBITMAP hOldBitmap = (HBITMAP)SelectObject(s_fonthdc, m_bmp);
	SetBkMode(s_fonthdc, TRANSPARENT);
	SelectObject(s_fonthdc, hOldBitmap);
	m_status = VEGAFont::Construct();
#endif // VEGA_GDIFONT_SUPPORT
}

GdiOpFont::~GdiOpFont()
{
//	SelectObject(s_fonthdc, m_oldGdiObj);	//restore to old
	if (s_font_on_dc == this)
	{
		// Select a systemfont to the font_hdc, otherwise, the font won't be deleted
		SelectObject(s_fonthdc, (HFONT)GetStockObject(SYSTEM_FONT));
		s_font_on_dc = NULL;
	}

	if(m_fnt)
		DeleteObject(m_fnt);
#ifdef VEGA_GDIFONT_SUPPORT
	if (m_bmp)
		DeleteObject(m_bmp);
#endif // VEGA_GDIFONT_SUPPORT
	ScriptFreeCache(&vega_script_cache);
}

UINT32 GdiOpFont::StringWidth(const uni_char *str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing)
{
#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	BOOL printer = painter?(((VEGAOpPainter*)painter)->getPrinterListener()!=NULL):FALSE;
	HFONT oldfnt = NULL;
	HDC bkup_fonthdc = NULL;
	GdiOpFont* bkup_font_on_dc = NULL;
	if (printer)
	{
		bkup_font_on_dc = s_font_on_dc;
		bkup_fonthdc = s_fonthdc;
		s_font_on_dc = this;
		s_fonthdc = ((WindowsVegaPrinterListener*)((VEGAOpPainter*)painter)->getPrinterListener())->getHDC();
		
		oldfnt = (HFONT)SelectObject(s_fonthdc, m_fnt);
	}
#endif // !VEGA_PRINTER_LISTENER_SUPPORT

	if (s_font_on_dc != this)
	{
		m_oldGdiObj = SelectObject(s_fonthdc, m_fnt);
		s_font_on_dc = this;
	}

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(s_fonthdc, extra_char_spacing);

	SIZE s = {0, 0};

#ifdef SUPPORT_TEXT_DIRECTION
	WIN32ComplexScriptSupport::GetTextExtent(s_fonthdc, str, len, &s, extra_char_spacing);
#else
	GetTextExtentPoint32(s_fonthdc, str, len, &s);
#endif

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(s_fonthdc, 0);

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
	if (printer)
	{
		SelectObject(s_fonthdc, oldfnt);
		s_fonthdc = bkup_fonthdc;
		s_font_on_dc = bkup_font_on_dc;
	}
#endif // !VEGA_PRINTER_LISTENER_SUPPORT
	return s.cx;
}

# ifdef VEGA_GDIFONT_SUPPORT

#pragma comment (lib, "usp10.lib")

#ifdef MDF_CAP_PROCESSEDSTRING
#include "modules/mdefont/processedstring.h"

SCRIPT_ANALYSIS vega_script_analysis;

OP_STATUS GdiOpFont::ProcessString(struct ProcessedString* processed_string,
									const uni_char* str, const size_t len,
									INT32 extra_char_spacing, short,
									bool use_glyph_indices)
{
	SCRIPT_ITEM items[1024];
	SCRIPT_CONTROL control;
	SCRIPT_STATE state;
	op_memset(&control, 0, sizeof(control));
	op_memset(&state, 0, sizeof(state));
	int num_items;

	HRESULT error =	ScriptItemize(str, len, 1024, &control, &state, items, &num_items);

	if (FAILED(error))
		return (error == E_OUTOFMEMORY) ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

	WORD glyphs[10*1024];
	WORD logclust[10*1024];
	SCRIPT_VISATTR visattr[10*1024];

	int num_glyphs = 0;
	int total_glyphs = 0;

	GOFFSET goffsets[10*1024];
	int advance[10*1024];

	// Don't select font into dc unless necessary
	BOOL use_dc = FALSE;

	for (int i = 0; i < num_items; ++i)
	{
		int item_len = items[i+1].iCharPos - items[i].iCharPos;
		const uni_char* str_pos = str + items[i].iCharPos;

		error = ScriptShape(use_dc ? s_fonthdc : NULL, &vega_script_cache, str_pos, item_len, 10*1024, &items[i].a, &glyphs[total_glyphs], &logclust[total_glyphs], &visattr[total_glyphs], &num_glyphs);

		if (error == E_PENDING)
		{
			if (s_font_on_dc != this)
			{
				SelectObject(s_fonthdc, m_fnt);
				s_font_on_dc = this;
			}
			use_dc = TRUE;
			error = ScriptShape(s_fonthdc, &vega_script_cache, str_pos, item_len, 10*1024, &items[i].a, &glyphs[total_glyphs], &logclust[total_glyphs], &visattr[total_glyphs], &num_glyphs);
		}

		if (FAILED(error))
			return (error == E_OUTOFMEMORY) ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

		error = ScriptPlace(use_dc ? s_fonthdc : NULL, &vega_script_cache, &glyphs[total_glyphs], num_glyphs, &visattr[total_glyphs], &items[i].a, &advance[total_glyphs], &goffsets[total_glyphs], NULL);

		if (error == E_PENDING)
		{
			if (s_font_on_dc != this)
			{
				SelectObject(s_fonthdc, m_fnt);
				s_font_on_dc = this;
			}
			use_dc = TRUE;
			error = ScriptPlace(s_fonthdc, &vega_script_cache, &glyphs[total_glyphs], num_glyphs, &visattr[total_glyphs], &items[i].a, &advance[total_glyphs], &goffsets[total_glyphs], NULL);
		}

		if (FAILED(error))
			return (error == E_OUTOFMEMORY) ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

		total_glyphs += num_glyphs;
	}

	ProcessedGlyph* glyph_buffer = m_font_manager->GetGlyphBuffer(total_glyphs);

	if (!glyph_buffer)
		return OpStatus::ERR;

	processed_string->m_length = total_glyphs;
	processed_string->m_is_glyph_indices = TRUE;
	processed_string->m_top_left_positioned = FALSE;
	processed_string->m_advance = 0;
	processed_string->m_processed_glyphs = glyph_buffer;
	for (int i = 0; i < total_glyphs; ++i)
	{
		processed_string->m_processed_glyphs[i].m_pos = OpPoint(processed_string->m_advance+goffsets[i].du, goffsets[i].dv);
		processed_string->m_advance += advance[i] + extra_char_spacing;
		processed_string->m_processed_glyphs[i].m_id = glyphs[i];
	}

	return OpStatus::OK;
}
#endif // MDF_CAP_PROCESSEDSTRING

BOOL GdiOpFont::StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_space, UINT32* strlen)
{
	// Only used for native rendering
	OP_ASSERT(FALSE);

	*strlen = 0;
	if (s_font_on_dc != this)
	{
		SelectObject(s_fonthdc, m_fnt);
		s_font_on_dc = this;
	}
	SIZE s = {0, 0};
	GetTextExtentPoint32(s_fonthdc, str, len, &s);
	*strlen = s.cx;
	return TRUE;
}

OP_STATUS GdiOpFont::loadGlyph(VEGAGlyph& glyph, UINT8* _data, unsigned int stride, BOOL isIndex, BOOL blackOnWhite)
{
	UINT8* data;
	glyph.left = -1; // 1 pixel reserved for cleartype fringe
	glyph.top = m_ascent;
	glyph.width = m_max_advance;
	glyph.height = m_height;

	if (_data)
	// rasterize glyph into buffer provided in data
	{
		data = _data;
		glyph.m_handle = NULL;
	}
	else
	{
#ifdef VEGA_SUBPIXEL_FONT_BLENDING
		data = OP_NEWA(UINT8, glyph.width*glyph.height*4);
#else
		data = OP_NEWA(UINT8, glyph.width*glyph.height);
#endif
		if (!data)
			return OpStatus::ERR_NO_MEMORY;
		glyph.m_handle = data;
		stride = glyph.width;
	}
	if (s_font_on_dc != this)
	{
		SelectObject(s_fonthdc, m_fnt);
		s_font_on_dc = this;
	}
#ifndef MDF_CAP_PROCESSEDSTRING
	SIZE s = {0, 0};
	uni_char glyphcode = glyph.glyph;
	GetTextExtentPoint32(s_fonthdc, &glyphcode, 1, &s);
	glyph.advance = s.cx;
#endif // !MDF_CAP_PROCESSEDSTRING

	HBITMAP hOldBitmap = (HBITMAP)SelectObject(s_fonthdc, m_bmp);

	RECT r = {0,0,glyph.width, glyph.height};

	if (blackOnWhite)
	{
		FillRect(s_fonthdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
		SetTextColor(s_fonthdc, RGB(0,0,0));
	}
	else
	{
		FillRect(s_fonthdc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
		SetTextColor(s_fonthdc, RGB(255,255,255));
	}

#ifdef MDF_CAP_PROCESSEDSTRING
	int adv = 0;
	GOFFSET gof;
	gof.du = 0;
	gof.dv = 0;
	WORD codepoint = glyph.glyph;
	ScriptTextOut(s_fonthdc, &vega_script_cache, -glyph.left, 0, 0, NULL, &vega_script_analysis, NULL, 0, &codepoint, 1, &adv, NULL, &gof);
	glyph.top = 0;
#else // !MDF_CAP_PROCESSEDSTRING
	TextOut(s_fonthdc, -glyph.left, 0, &glyphcode, 1);
#endif // !MDF_CAP_PROCESSEDSTRING

	for (int yp = 0; yp < glyph.height; ++yp)
	{
		for (int xp = 0; xp < glyph.width; ++xp)
		{
			UINT8 r,g,b;
			if (blackOnWhite)
			{
				r = 255-((m_bmp_data[yp*m_max_advance+xp]&0xff0000)>>16);
				g = 255-((m_bmp_data[yp*m_max_advance+xp]&0xff00)>>8);
				b = 255-((m_bmp_data[yp*m_max_advance+xp]&0xff));
			}
			else
			{
				r = ((m_bmp_data[yp*m_max_advance+xp]&0xff0000)>>16);
				g = ((m_bmp_data[yp*m_max_advance+xp]&0xff00)>>8);
				b = ((m_bmp_data[yp*m_max_advance+xp]&0xff));
			}

#ifdef VEGA_SUBPIXEL_FONT_BLENDING
			data[yp*stride+xp*4] = b;
			data[yp*stride+xp*4+1] = g;
			data[yp*stride+xp*4+2] = r;
			data[yp*stride+xp*4+3] = (b+g+r)/3;
#else
			data[yp*stride+xp] = (b+g+r)/3;
#endif
		}
	}

	SelectObject(s_fonthdc, hOldBitmap);
	return OpStatus::OK;
}

void GdiOpFont::unloadGlyph(VEGAGlyph& glyph)
{
	OP_DELETEA(((UINT8*)glyph.m_handle));
}

void GdiOpFont::getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride)
{
	buffer = (UINT8*)glyph.m_handle;
	stride = glyph.width;
}
# endif

#include "platforms/windows/pi/WindowsOpWindow.h"
BOOL GdiOpFont::nativeRendering()
{
	return g_vegaGlobals.rasterBackend != LibvegaModule::BACKEND_HW3D;
}

BOOL GdiOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGAWindow* dest)
{
	WindowsVegaWindow* dest_window = static_cast<WindowsVegaWindow*>(dest);

	if ((color>>24) != 255 || !dest_window->GetBitmapDC())
		return FALSE;

	OpRect dest_rect(0,0,dest_window->getWidth(), dest_window->getHeight());

	int trans_top, trans_bottom, trans_left, trans_right;
	dest_window->getTransparentMargins(trans_top, trans_bottom, trans_left, trans_right);
	trans_right = dest_rect.width - trans_right;
	trans_bottom = dest_rect.height - trans_bottom;
	BOOL restore_alpha = FALSE;
	unsigned int strw = 0;
	BOOL in_glass = pos.x < trans_left || pos.x > trans_right || pos.y < trans_top || pos.y > trans_bottom;
	if (in_glass || dest_window->GetIsLayered())
	{
		restore_alpha = TRUE;
		strw = StringWidth(str, len, NULL, extra_char_spacing);
		OpRect rect(pos.x, pos.y, strw, m_height);
		// Clip
		rect.IntersectWith(clip);
		rect.IntersectWith(dest_rect);

		if (rect.width == 0 || rect.height == 0)
			return TRUE;

		UINT32 sample1, sample2, sample3;
		VEGAPixelStore* ps = dest_window->getPixelStore();
		sample1 = ((UINT32*)ps->buffer)[(ps->stride/4)*rect.y+rect.x];
		sample2 = ((UINT32*)ps->buffer)[(ps->stride/4)*(rect.y+rect.height/2)+rect.x+rect.width/2];
		sample3 = ((UINT32*)ps->buffer)[(ps->stride/4)*(rect.y+rect.height-1)+rect.x+rect.width-1];
		if ((sample2>>24) == 0 && in_glass)
		{
			dest_window->ChangeDCFont(m_fnt, color);
			HDC hdc = dest_window->GetBitmapDC();

			if (extra_char_spacing != 0)
				SetTextCharacterExtra(hdc, extra_char_spacing);

			IntersectClipRect(hdc, clip.x, clip.y, clip.x + clip.width, clip.y + clip.height);

			DTTOPTS dto = { sizeof(DTTOPTS) };
		    const UINT uFormat = DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX;
		 	RECT r;
			r.top = pos.y;
			r.left = pos.x;
			r.bottom = pos.y+m_height;
			r.right = pos.x+strw;
		 
		    dto.dwFlags = DTT_COMPOSITED|DTT_GLOWSIZE;
		    dto.iGlowSize = 10;

			HRESULT hr = S_OK;
			if (!g_window_theme_handle)
				g_window_theme_handle = OpenThemeData(g_main_hwnd, UNI_L("window"));
			if (g_window_theme_handle)
				hr = OPDrawThemeTextEx(g_window_theme_handle, hdc, 0, 0, str, len, uFormat, &r, &dto);

			SelectClipRgn(hdc, NULL);

			if (extra_char_spacing != 0)
				SetTextCharacterExtra(hdc, 0);
			if (g_window_theme_handle && SUCCEEDED(hr))
				return TRUE;
			else
				return FALSE;
		}
		else if ((sample1>>24) != 255 || (sample2>>24) != 255 || (sample3>>24) != 255)
			return FALSE;
	}

	dest_window->ChangeDCFont(m_fnt, color);
	HDC hdc = dest_window->GetBitmapDC();

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(hdc, extra_char_spacing);

	IntersectClipRect(hdc, clip.x, clip.y, clip.x + clip.width, clip.y + clip.height);
#ifdef SUPPORT_TEXT_DIRECTION
	WIN32ComplexScriptSupport::TextOut(hdc, pos.x, pos.y, str, len);
#else
	TextOut(hdc, pos.x, pos.y, str, len);
#endif
	if (restore_alpha)
	{
		if (!strw)
			strw = StringWidth(str, len, NULL, extra_char_spacing);

		OpRect rect(pos.x, pos.y, strw, m_height);

		// Cleartype can touch 2 pixels to the left of the starting position, so we have to include those.
		rect.x -= 2;
		rect.width += 3; // Also need 1 extra pixel to the right

		// Clip
		rect.IntersectWith(clip);
		rect.IntersectWith(dest_rect);
		VEGAPixelStore* ps = dest->getPixelStore();
		for (int yp = rect.y; yp < rect.Bottom(); ++yp)
		{
			UINT32* data = ((UINT32*)ps->buffer)+yp*ps->width;
			for (int xp = rect.x; xp < rect.Right(); ++xp)
			{
				data[xp] |= (255<<24);
			}
		}
	}
	SelectClipRgn(hdc, NULL);

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(hdc, 0);
	return TRUE;
}
BOOL GdiOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGASWBuffer* dest)
{
	return FALSE;
}

BOOL GdiOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool isWindow)
{
	OP_ASSERT(FALSE);
	return FALSE;
}

BOOL GdiOpFont::GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride)
{
	if (s_font_on_dc != this)
	{
		m_oldGdiObj = SelectObject(s_fonthdc, m_fnt);
		s_font_on_dc = this;
	}

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(s_fonthdc, extra_char_spacing);

	SIZE s = {0, 0};

#ifdef SUPPORT_TEXT_DIRECTION
	WIN32ComplexScriptSupport::GetTextExtent(s_fonthdc, str, len, &s, extra_char_spacing);
#else
	GetTextExtentPoint32(s_fonthdc, str, len, &s);
#endif

	if (extra_char_spacing != 0)
		SetTextCharacterExtra(s_fonthdc, 0);

	OpRect dst(0,0,s.cx, s.cy);
	dst.IntersectWith(clip);
	if (dst.IsEmpty())
	{
		size->x = 0;
		size->y = 0;
		return TRUE;
	}

	if (!s_text_bitmap || s_text_bitmap_width < s.cx || s_text_bitmap_height < s.cy)
	{
		if (s_text_bitmap_dc)
		{
			SelectObject(s_text_bitmap_dc, s_old_text_bitmap);
			DeleteDC(s_text_bitmap_dc);
		}
		s_text_bitmap_dc = NULL;
		if (s_text_bitmap)
			DeleteObject(s_text_bitmap);
		s_text_bitmap = NULL;

		BITMAPINFO bi;
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = s.cx;
		bi.bmiHeader.biHeight = -(int)s.cy;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		bi.bmiHeader.biSizeImage = 0;
		bi.bmiHeader.biXPelsPerMeter = 0;
		bi.bmiHeader.biYPelsPerMeter = 0;
		bi.bmiHeader.biClrUsed = 0;
		bi.bmiHeader.biClrImportant = 0;

		s_text_bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&s_text_bitmap_data, NULL, 0);
		if (!s_text_bitmap)
			return FALSE;

		s_text_bitmap_dc = CreateCompatibleDC(s_fonthdc);
		if (!s_text_bitmap_dc)
			return FALSE;

		s_old_text_bitmap = (HBITMAP)SelectObject(s_text_bitmap_dc, s_text_bitmap);
		s_text_bitmap_width = s.cx;
		s_text_bitmap_height = s.cy;
		SetBkMode(s_text_bitmap_dc, TRANSPARENT);
		//SetTextColor(s_text_bitmap_dc, RGB(255,255,255));
		SetTextColor(s_text_bitmap_dc, 0);
	}
	
	IntersectClipRect(s_text_bitmap_dc, clip.x, clip.y, clip.x + clip.width, clip.y + clip.height);

	RECT tmprect = { 0, 0, s.cx, s.cy };
	//::FillRect(s_text_bitmap_dc, &tmprect, (HBRUSH) GetStockObject(BLACK_BRUSH));
	::FillRect(s_text_bitmap_dc, &tmprect, (HBRUSH) GetStockObject(WHITE_BRUSH));

	HFONT old_font = (HFONT) SelectObject(s_text_bitmap_dc, m_fnt);

	SetTextCharacterExtra(s_text_bitmap_dc, extra_char_spacing);
	WIN32ComplexScriptSupport::TextOut(s_text_bitmap_dc, 0, 0, str, len);
	
	SelectClipRgn(s_text_bitmap_dc, NULL);

	SelectObject(s_text_bitmap_dc, old_font);

	*mask = s_text_bitmap_data+1;
	size->x = s.cx;
	size->y = s.cy;
	*rowStride = s_text_bitmap_width*4;
	*pixelStride = 4;

	// DSK-281697: Text now rendered black on white, so invert the mask
	UINT8* line = (UINT8*)*mask;
	for (int y = 0; y < size->y; ++y)
	{
		UINT8* pix = line;
		for (int x = 0; x < size->x*4; x+=4)
		{
			*pix = 255 - *pix;
			pix+=4;
		}
		line += *rowStride;
	}

	return TRUE;
}

#ifndef VEGA_GDIFONT_SUPPORT
void GdiOpFont::GetWidths(UINT32 from, UINT32 to, INT *cached_widths)
{
	GetCharWidth(s_fonthdc, from, to, cached_widths);
}
#endif // VEGA_GDIFONT_SUPPORT

#ifdef SVG_SUPPORT
double fixed_to_double(FIXED in)
{
	return (double)in.value + (double)in.fract/USHRT_MAX;
}

OP_STATUS GdiOpFont::GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
									 BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph)
{
	// If NULL this is a test to see if GetOutline is supported
	if (!out_glyph)
		return OpStatus::OK;

	if (io_str_pos >= in_len)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (s_font_on_dc != this)
	{
		SelectObject(s_fonthdc, m_fnt);
		s_font_on_dc = this;
	}

	GLYPHMETRICS metrics;
	MAT2 matrix;
	
	memset(&matrix, 0, sizeof(matrix));

	// glyphs are upside down, which is what svg expects, so set the matrix to identity
	matrix.eM11.value = 1;
	matrix.eM22.value = 1;

	uni_char outl_char = in_str[io_str_pos];

	DWORD buffer_size = ::GetGlyphOutline(s_fonthdc, outl_char, GGO_NATIVE, &metrics, 0, 0, &matrix);
	if (buffer_size == GDI_ERROR)
		return OpStatus::ERR;

	BYTE *buffer = OP_NEWA(BYTE, buffer_size);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoArray<BYTE> buffer_ap(buffer);

	DWORD err = ::GetGlyphOutline(s_fonthdc, outl_char, GGO_NATIVE, &metrics, buffer_size, buffer, &matrix);
	if (err == GDI_ERROR)
		return OpStatus::ERR;

	SVGPath* pathTmp;
	RETURN_IF_ERROR(g_svg_manager->CreatePath(&pathTmp));

	OpAutoPtr<SVGPath> path(pathTmp);

	BYTE *buf_end = buffer + buffer_size;

	while (buffer < buf_end)
	{
		TTPOLYGONHEADER* ph = (TTPOLYGONHEADER*)buffer;
		RETURN_IF_ERROR(path->MoveTo(fixed_to_double(ph->pfxStart.x), fixed_to_double(ph->pfxStart.y), FALSE));

		BYTE *curves_pos = buffer + sizeof(TTPOLYGONHEADER);
		BYTE *curves_end = buffer + ph->cb;

		while(curves_pos < curves_end)
		{
			TTPOLYCURVE* pc = (TTPOLYCURVE*)curves_pos;
			if (pc->wType == TT_PRIM_LINE)
			{
				// straight line
				for(int i = 0; i < pc->cpfx; i++)
				{
					RETURN_IF_ERROR(path->LineTo(fixed_to_double(pc->apfx[i].x), fixed_to_double(pc->apfx[i].y), FALSE));
				}
			}
			else if (pc->wType == TT_PRIM_QSPLINE)
			{
				// quadratic spline
				for(int i = 0; i < pc->cpfx-1; i++)
				{
					POINTFX* b = &pc->apfx[i];
					POINTFX* c = &pc->apfx[i+1];
					SVGNumber epx;
					SVGNumber epy;

					if (i < pc->cpfx - 2)
					{
						epx = (fixed_to_double(b->x) + fixed_to_double(c->x)) / 2;
						epy = (fixed_to_double(b->y) + fixed_to_double(c->y)) / 2;
					}
					else
					{
						epx = fixed_to_double(c->x);
						epy = fixed_to_double(c->y);
					}

					RETURN_IF_ERROR(path->QuadraticCurveTo(fixed_to_double(b->x), fixed_to_double(b->y),
											epx, epy, 
											FALSE, FALSE));
				}
			}
			else if (pc->wType == TT_PRIM_CSPLINE)
			{
				// cubic spline
				for(int i = 0; i < pc->cpfx-2; i++)
				{
					POINTFX* cp1 = &pc->apfx[i];
					POINTFX* cp2 = &pc->apfx[i+1];
					POINTFX* endp = &pc->apfx[i+2];
					RETURN_IF_ERROR(path->CubicCurveTo(fixed_to_double(cp1->x), fixed_to_double(cp1->y), 
											fixed_to_double(cp2->x), fixed_to_double(cp2->y),
											fixed_to_double(endp->x), fixed_to_double(endp->y),
											FALSE, FALSE));
				}
			}
			
			curves_pos += 2 * sizeof(WORD) + sizeof(POINTFX) * pc->cpfx;
		}

		RETURN_IF_ERROR(path->Close());

		buffer = curves_end;
	}

	*out_glyph = path.release();

	if (in_writing_direction_horizontal)
	{
		out_advance = metrics.gmCellIncX;
	}
	else
	{
		// Use Ascent()+Descent() as fallback since most fonts
		// have no y advance. Actually, the advance.y value might
		// be completely wrong anyway since it's probably based on
		// writing horizontally.

		if (metrics.gmCellIncY != 0)
		{
			out_advance = metrics.gmCellIncY;
		}
		else if (metrics.gmCellIncX > 0)
		{
			out_advance = Ascent() + Descent();
		}
	}

	io_str_pos++;

	return OpStatus::OK;
}
#endif // SVG_SUPPORT
