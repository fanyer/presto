/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Øyvind Østlund
**
*/

#include "core/pch.h"

#include "platforms/windows/win_handy.h"
#include "platforms/windows/pi/DWriteOpFont.h"
#include "platforms/windows/pi/GdiOpFontManager.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/utils/com_helpers.h"
#include "platforms/vega_backends/d3d10/vegad3d10fbo.h"
#include "platforms/vega_backends/d3d10/vegad3d10window.h"

#include "modules/display/check_sfnt.h"
#include "modules/pi/OpDLL.h"
#include "modules/svg/SVGManager.h"
#include "modules/svg/svg_path.h"
#include "modules/svg/svg_number.h"

#include <d3d10_1.h>

typedef HRESULT (WINAPI *PFNDWRITECREATEFACTORYPROC)(__in DWRITE_FACTORY_TYPE factoryType, __in REFIID iid, __out IUnknown **factory);

//
// DWriteTextAnalysis
// - Used by the DWriteOpFont objects to analyze a text run.
//

/*static*/
DWRITE_MEASURING_MODE DWriteTextAnalyzer::s_measuring_mode = DWRITE_MEASURING_MODE_NATURAL;

/*static*/
void DWriteTextAnalyzer::SetMeasuringMode(int measuring_mode)
{
	if (measuring_mode == 0)
		DWriteTextAnalyzer::s_measuring_mode = DWRITE_MEASURING_MODE_NATURAL;
	else
		DWriteTextAnalyzer::s_measuring_mode = DWRITE_MEASURING_MODE_GDI_NATURAL;
}


DWriteTextAnalyzer::DWriteTextAnalyzer()
	: m_dw_text_analyzer(NULL)
	, m_str_cache(NULL)
{
	for (UINT i = 0; i < NUM_ASCII_CHARS; ++i)
		m_ascii_chars[i] = i;

	op_memset(&m_unused_runs, TRUE, FONTCACHESIZE * sizeof(bool));
	op_memset(&m_analysis, 0, sizeof(m_analysis));
	op_memset(&m_out, 0, sizeof(m_out));
}

DWriteTextAnalyzer::~DWriteTextAnalyzer()
{
	OP_DELETEA(m_analysis.analysis);
	OP_DELETEA(m_analysis.cluster_map);
	OP_DELETEA(m_analysis.text_props);
	OP_DELETEA(m_analysis.glyph_props);

	if (m_str_cache)
	{
		for (UINT i = 0; i < (FONTCACHESIZE * CACHED_GLYPH_RUNS); ++i)
		{
			OP_DELETEA(m_str_cache[i].str);
			m_str_cache[i].run.Delete();
		}

		VirtualFree(m_str_cache, 0, MEM_RELEASE);
	}
	m_out.run.Delete();

	if (m_dw_text_analyzer)
		m_dw_text_analyzer->Release();
}

OP_STATUS DWriteTextAnalyzer::Construct(IDWriteFactory* dw_factory)
{
	if (FAILED(dw_factory->CreateTextAnalyzer(&m_dw_text_analyzer)))
		return OpStatus::ERR;

	m_str_cache = (DWriteString*)VirtualAlloc(NULL, FONTCACHESIZE * CACHED_GLYPH_RUNS * sizeof(DWriteString), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	RETURN_OOM_IF_NULL(m_str_cache);
	op_memset(m_str_cache, 0, sizeof(DWriteString) * (FONTCACHESIZE * CACHED_GLYPH_RUNS));

	RETURN_IF_ERROR(IncreaseMaxGlyphs(8));

	const int OUTPUT_SIZE = 1024;
	return m_out.run.Resize(OUTPUT_SIZE, false);
}

// IDWriteTextAnalysisSource
HRESULT DWriteTextAnalyzer::GetLocaleName(UINT32 text_position, UINT32* text_length, const WCHAR** locale_name)
{
	// FIXME:
	*text_length = 0;
	*locale_name = NULL;
	return S_OK;
}

HRESULT DWriteTextAnalyzer::GetNumberSubstitution(UINT32 text_position, UINT32* text_length, IDWriteNumberSubstitution** number_substitution)
{
	*text_length = 0;
	*number_substitution = NULL;
	return S_OK;
}

DWRITE_READING_DIRECTION DWriteTextAnalyzer::GetParagraphReadingDirection()
{
	return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
}

HRESULT DWriteTextAnalyzer::GetTextAtPosition(UINT32 text_position, const WCHAR** text_string, UINT32* text_length)
{
	if (text_position >= m_to_analyze->len)
	{
		*text_string = NULL;
		*text_length = 0;
	}
	else
	{
		*text_string = m_to_analyze->str + text_position;
		*text_length = m_to_analyze->len - text_position;
	}
	return S_OK;
}

HRESULT DWriteTextAnalyzer::GetTextBeforePosition(UINT32 text_position, const WCHAR** text_string, UINT32* text_length)
{
	if (text_position >= m_to_analyze->len || text_position == 0)
	{
		*text_string = NULL;
		*text_length = 0;
	}
	else
	{
		*text_string = m_to_analyze->str;
		*text_length = text_position;
	}
	return S_OK;
}

HRESULT DWriteTextAnalyzer::SetScriptAnalysis(UINT32 text_position, UINT32 text_length, const DWRITE_SCRIPT_ANALYSIS* script_analysis)
{
	if (script_analysis->shapes == DWRITE_SCRIPT_SHAPES_DEFAULT)
	{
		for (UINT32 i = 0; i < text_length; ++i)
			m_analysis.analysis[text_position+i].script = script_analysis->script;
	}
	else
	{
		if (m_analysis.reset_analysis == -1)
			m_analysis.reset_analysis = text_position;

		for (UINT32 i = 0; i < text_length; ++i)
			m_analysis.analysis[text_position+i] = *script_analysis;
	}
	return S_OK;
}

OP_STATUS DWriteTextAnalyzer::IncreaseMaxGlyphs(UINT new_size)
{
	if (m_analysis.size < new_size)
	{
		DWRITE_SCRIPT_ANALYSIS* analysis = OP_NEWA(DWRITE_SCRIPT_ANALYSIS, new_size); // At least 'text len' long.
		UINT16* cluster_map = OP_NEWA(UINT16, new_size); // At least 'text len' long.
		DWRITE_SHAPING_TEXT_PROPERTIES* text_props = OP_NEWA(DWRITE_SHAPING_TEXT_PROPERTIES, new_size); // At least 'text len' long.
		DWRITE_SHAPING_GLYPH_PROPERTIES* glyph_props = OP_NEWA(DWRITE_SHAPING_GLYPH_PROPERTIES, new_size); // At least 'maxglyphs' long.

		if (!analysis || !cluster_map || !text_props || !glyph_props)
		{
			m_analysis.size = 0;
			OP_DELETEA(analysis);
			OP_DELETEA(cluster_map);
			OP_DELETEA(text_props);
			OP_DELETEA(glyph_props);
			return OpStatus::ERR_NO_MEMORY;
		}

		OP_DELETEA(m_analysis.analysis);
		OP_DELETEA(m_analysis.cluster_map);
		OP_DELETEA(m_analysis.text_props);
		OP_DELETEA(m_analysis.glyph_props);

		m_analysis.analysis = analysis;
		m_analysis.cluster_map = cluster_map;
		m_analysis.text_props = text_props;
		m_analysis.glyph_props = glyph_props;

		m_analysis.size = new_size;

		// Until otherwise proven, the analysis::shapes member should always be DWRITE_SCRIPT_SHAPES_DEFAULT
		// So lets set it to that right away, and then the analysis::script part will be set in SetScriptAnalysis for each run.
		op_memset(m_analysis.analysis, 0, new_size * sizeof(DWRITE_SCRIPT_ANALYSIS));
		m_analysis.reset_analysis = -1;
	}
	return OpStatus::OK;
}

void DWriteTextAnalyzer::GetCurrentFontMetrics(DWriteOpFont* font, DWRITE_FONT_METRICS& font_metrics)
{
	IDWriteFontFace* dw_font_face = font->GetFontFace();

	// If the pref is turned on, then get the GDI measuring mode.
	if (DWriteTextAnalyzer::s_measuring_mode != DWRITE_MEASURING_MODE_NATURAL)
	{
		if (SUCCEEDED(dw_font_face->GetGdiCompatibleMetrics(font->Size(), 96.f/96.f, NULL, &font_metrics)))
			return;
	}

	// If not (+ fallback), fetch the normal ones.
	dw_font_face->GetMetrics(&font_metrics);
}

OP_STATUS DWriteTextAnalyzer::AnalyzeGlyphRun(DWriteOpFont* font, DWriteString* dw_string)
{
	// Save a pointer to the run for the analyzer
	m_to_analyze = dw_string;
	DWriteGlyphRun* run = &dw_string->run;

	//
	// Analyze run
	//

	// If the string length is longer than the glyph run, just increase it sooner rather than later.
	if (dw_string->len > m_analysis.size)
		RETURN_IF_ERROR(IncreaseMaxGlyphs(dw_string->len));

	if (FAILED(m_dw_text_analyzer->AnalyzeScript(this, 0, dw_string->len, this)))
		return OpStatus::ERR;

	HRESULT res = S_OK;

	do
	{
		// Get indices, cluster map, text/glyph properties and length.
		res = m_dw_text_analyzer->GetGlyphs(dw_string->str,					// textString [in]
											dw_string->len,					// textLength
											font->GetFontFace(),				// fontFace
											FALSE,							// isSideways
											FALSE,							// isRightToLeft
											m_analysis.analysis,			// scriptAnalysi [in]
											NULL,							// localeName [in, optional]
											NULL,							// numberSubstitution [optional]
											NULL,							// features [in, optional]
											NULL,							// featureRangeLengths [in, optional]
											0,								// featureRanges
											run->size,						// maxGlyphCount
											m_analysis.cluster_map,			// clusterMap [out]
											m_analysis.text_props,			// textProps [out]
											run->indices,					// glyphIndices [out]
											m_analysis.glyph_props,			// glyphProps [out]
											&run->len);						// actualGlyphCount [out]

		// If the buffer is not big enough, resize, and try again.
		if (res == E_NOT_SUFFICIENT_BUFFER)
		{
			UINT new_size = run->size == 0 ? dw_string->len : run->size * 2;
			RETURN_IF_ERROR(run->Resize(new_size, false));
		
			if (new_size > m_analysis.size)
				RETURN_IF_ERROR(IncreaseMaxGlyphs(new_size));
		}

	} while (res == E_NOT_SUFFICIENT_BUFFER);

	if (FAILED(res))
		return OpStatus::ERR;


	//
	// Do glyph shaping of complex scripts. We also get the advances/offsets, that we later can use
	// for batching the glyphs togheter to one draw call by manipulating the advances. As well as add letter spacing.
	//

	if (DWriteTextAnalyzer::s_measuring_mode == DWRITE_MEASURING_MODE_NATURAL)
		GetGlyphPlacements(font);
	else
		GetGDIGlyphPlacements(font);
	
	//
	// Reset the analysis->shapes part to DWRITE_SCRIPT_SHAPES_DEFAULT since it changes so seldom, so we don't have to set it again next time.
	//

	if (m_analysis.reset_analysis > 0)
	{
		INT len = dw_string->len - m_analysis.reset_analysis;
		OP_ASSERT(len > 0);
		op_memset(m_analysis.analysis + m_analysis.reset_analysis, 0, len * sizeof(DWRITE_SCRIPT_ANALYSIS));
		m_analysis.reset_analysis = -1;
	}

	//
	// Calculate the width of the string, and cache it.
	//

	run->width = .0f;

	for (UINT i = 0; i < run->len; ++i)
		run->width += run->advances[i];

	return OpStatus::OK;
}

OP_STATUS DWriteTextAnalyzer::GetGlyphPlacements(DWriteOpFont* font)
{
	HRESULT hr;
	hr = m_dw_text_analyzer->GetGlyphPlacements(m_to_analyze->str,			// textString [in]
												m_analysis.cluster_map,		// clusterMap [in]
												m_analysis.text_props,		// textProps [in]
												m_to_analyze->len,			// textLength
												m_to_analyze->run.indices,	// glyphIndices [in]
												m_analysis.glyph_props,		// glyphProps [in]
												m_to_analyze->run.len,		// glyphCount
												font->GetFontFace(),			// fontFace
												font->Size(),				// fontEmSize
												FALSE,						// isSideways
												FALSE,						// isRightToLeft
												m_analysis.analysis,		// scriptAnalysis [in]
												NULL,						// localeName [in, optional]
												NULL,						// features [in, optional]
												NULL,						// featureRangeLengths [in, optional]
												0,							// featureRanges
												m_to_analyze->run.advances,	// glyphAdvances [out]
												m_to_analyze->run.offsets);	// glyphOffsets [out]

	if (FAILED(hr))
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS DWriteTextAnalyzer::GetGDIGlyphPlacements(DWriteOpFont* font)
{
	bool use_gdi_natural = DWriteTextAnalyzer::s_measuring_mode == DWRITE_MEASURING_MODE_GDI_NATURAL;

	HRESULT hr;
	hr = m_dw_text_analyzer->GetGdiCompatibleGlyphPlacements(m_to_analyze->str,
															 m_analysis.cluster_map,
															 m_analysis.text_props,
															 m_to_analyze->len,
															 m_to_analyze->run.indices,
															 m_analysis.glyph_props,
															 m_to_analyze->run.len,
															 font->GetFontFace(),
															 font->Size(),
															 96.f,
															 NULL,
															 use_gdi_natural,
															 FALSE,
															 FALSE,
															 m_analysis.analysis,
															 NULL,
															 NULL,
															 NULL,
															 0,
															 m_to_analyze->run.advances,
															 m_to_analyze->run.offsets);

	if (FAILED(hr))
		return OpStatus::ERR;

	return OpStatus::OK;
}


void DWriteTextAnalyzer::GetAsciiChars(DWriteOpFont* font, DWriteString* dw_string)
{
	dw_string->str = m_ascii_chars;
	dw_string->size = 256;
	dw_string->len = 256;
	dw_string->run.Resize(256, false);

	// Try to make a buffer of all the ascii characters. But only if there is a 1-to-1 mapping between characters and glyphs.
	if (OpStatus::IsError(AnalyzeGlyphRun(font, dw_string)) || dw_string->run.len != 256)
	{
		dw_string->str = NULL;
		dw_string->size = 0;
		dw_string->len = 0;
		dw_string->run.Delete();
	}
	else
	{
		// Space:
		// Since core is using UINT to measure characters and words, we get rounding errors with DirectWrite.
		// Forcing the space character to be a whole number, will remove these rounding errors if there is
		// more than one space after each other.
		dw_string->run.advances[ASCII_SPACE_CHAR] = (UINT)dw_string->run.advances[ASCII_SPACE_CHAR];

		// Quote:
		// For example Times New Roman the quote character exists, but it does not move the pen position to the right.
		// So if the character exists in the current font, but it does not have a width, set it to the width of the star or space character.
		if (dw_string->run.indices[ASCII_QUOTE_CHAR] && !dw_string->run.advances[ASCII_QUOTE_CHAR])
			dw_string->run.advances[ASCII_QUOTE_CHAR] = MAX(dw_string->run.advances[ASCII_STAR_CHAR], dw_string->run.advances[ASCII_SPACE_CHAR]);
	}
}

DWriteString* DWriteTextAnalyzer::GetGlyphRuns()
{
	UINT i;

	for (i = 0; i <= FONTCACHESIZE; ++i)
	{
		if (m_unused_runs[i])
			break;
	}

	if (i >= FONTCACHESIZE)
	{
		OP_ASSERT(!"This can't happen, there should be enough cache slots for all cached fonts.");
		return NULL;
	}

	DWriteString* dw_string = &m_str_cache[CACHED_GLYPH_RUNS * i];
	m_unused_runs[i] = false;

	for (UINT j = 0; j < CACHED_GLYPH_RUNS; j++)
		dw_string[j].len = 0;

	return dw_string;
}

void DWriteTextAnalyzer::FreeGlyphRuns(DWriteString* dw_string)
{
	// If the font cache was full, and then a webfont was created, it might not have a glyph cache.
	if (!dw_string)
		return;

	UINT cached_run_index = (dw_string - m_str_cache) / CACHED_GLYPH_RUNS;
	m_unused_runs[cached_run_index] = true;
}

void DWriteTextAnalyzer::OnDeletingFont(DWriteOpFont* font)
{
	if (m_out.font == font)
		flushFontBatch();
}

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
void DWriteTextAnalyzer::SetDrawState(DWriteOpFont* font, UINT32 color, const OpPoint& pos, const OpRect& clip, VEGA3dRenderTarget* dest, bool is_window)
{
	// If any of the states have changed, then flush the batch
	if (m_out.HasOutstandingBatch())
	{
		// We only batch text which are aligned horizontally. But if font, colour, destination changes, we have to flush even if there
		// is more text on the same line. We also have to flush if the clip rect of the currently batched text does not intersect with the new
		// text because of boxes can have no overflow set, or two buttons can be aligned vertically, but should not have text in between the buttons.

		// These tests are ordered from most frequent reason for flush, to least frequent reason. In a typical oBench run we will just batch the text without
		// flushing in about 75% of the cases, while colour change will flush in about 15% of the cases.
		if (color != m_out.color)
			flushFontBatch();
		else if (pos.y != m_out.start_pos.y)
			flushFontBatch();
		else if (font != m_out.font)
			flushFontBatch();
		else if (m_out.clip.Right() < clip.x || m_out.clip.x > clip.Right())
			flushFontBatch();
		else if (m_out.dest != dest)
			flushFontBatch();
		else
		{
			// The "is_window" parameter should not be have changed unless "dest" has changed.
			OP_ASSERT(m_out.is_window == is_window);
		}
	}

	// If there is no text already batched up, then set all states, if not, then just increase the clip rect.
	if (!m_out.HasOutstandingBatch())
	{
		// Get the render target. This will also create it, and tell the rendertarget we are using it.
		if (dest->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
			static_cast<VEGAD3d10Window*>(dest)->getD2DRenderTarget(this);
		else
			static_cast<VEGAD3d10FramebufferObject*>(dest)->getD2DRenderTarget(!is_window, this);

		m_out.font = font;
		m_out.start_pos = pos;
		m_out.clip = clip;
		m_out.color = color;
		m_out.dest = dest;
		m_out.is_window = is_window;
	}
	else
	{
		m_out.clip.UnionWith(clip);
	}
}

void DWriteTextAnalyzer::AddExtraCharSpacing(FLOAT* advances, UINT len, FLOAT extra_spacing)
{
	for (unsigned int i = 0; i < len; ++i)
	{
		advances[i] += extra_spacing;
	}
	m_out.pen_pos_x += (extra_spacing * len);
}

void DWriteTextAnalyzer::BatchText(DWriteGlyphRun* run, const OpPoint& point, FLOAT extra_spacing)
{
	UINT& out_len = m_out.run.len;

	if (m_out.run.size < (out_len + run->len))
		return;

	// In the case where there is only one word that will be written before a flush,
	// do not copy anything to the outbuffer, but just store a pointer to it.
	if (!m_out.HasOutstandingBatch() && !extra_spacing)
	{
		m_out.first_run = run;
		m_out.pen_pos_x = point.x + run->width;
		return;
	}
	else if (m_out.first_run)
	{
		// If this is the second word in the batch, then first do the copying of the first to the outbuffer 
		// that we did not do the first time, before moving on to copying the current word further down.
		m_out.run.Append(m_out.first_run);
		m_out.first_run = NULL;
	}

	// Because core is using INT32 for messuring text/glyphs and DirectWrite is using FLOAT, there will be some rounding errors.
	// Get the difference between where our text-writer is now, and where the next word/letter is supposed to be written and
	// add it to the character in front of it (usually a space) to move the text-writer to where core expects it to be.
	if (out_len > 0)
	{
		FLOAT advance_offset = point.x - m_out.pen_pos_x;
		m_out.run.advances[out_len-1] += advance_offset;
	}

	// Copy the current word to the outbuffer.
	m_out.run.Append(run);

	// Update the current text drawing position.
	m_out.pen_pos_x = point.x + run->width;

	if (extra_spacing)
		AddExtraCharSpacing(&m_out.run.advances[out_len], run->len, extra_spacing);

	// If this word ended outside on the right || left side of the clip rect. then we need to flush now so we don't draw the whole string.
	if (m_out.pen_pos_x > m_out.clip.Right() || point.x < m_out.clip.x)
		flushFontBatch();
}

void DWriteTextAnalyzer::BatchText(const uni_char* str, UINT len, DWriteGlyphRun* run, const OpPoint& point, FLOAT extra_spacing)
{
	UINT& out_len = m_out.run.len;
	FLOAT run_width = .0f;
	FLOAT advance;

	// If this is the second word in the batch, then first do the copying of the first to the outbuffer
	// that we did not do the first time, before moving on to copying the current word further down.
	if (m_out.first_run)
	{
		m_out.run.Append(m_out.first_run);
		m_out.first_run = NULL;
	}

	// Copy the indices and advances from the passed in buffer, to the outbuffer.
	for (UINT i = 0; i < len; ++i)
	{
		advance = run->advances[str[i]];
		run_width += advance;
		m_out.run.advances[out_len + i] = advance;
		m_out.run.indices[out_len + i] = run->indices[str[i]];
	}

	// FIXME: (Potential memory and speed optimization) Very few chars uses offsets, so this can be optimized away in most cases.
	op_memset(&m_out.run.offsets[out_len], 0, len * sizeof(DWRITE_GLYPH_OFFSET));

	// Because core is using INT32 for messuring text/glyphs and DirectWrite is using FLOAT, there will be some rounding errors.
	// Get the difference between where our text-writer is now, and where the next word/letter is supposed to be written and
	// add it to the character in front of it (usually a space) to move the text-writer to where core expects it to be.
	if (out_len > 0)
	{
		FLOAT advance_offset = point.x - m_out.pen_pos_x;
		m_out.run.advances[out_len-1] += advance_offset;
	}

	// Update the current text drawing position.
	m_out.pen_pos_x = point.x + run_width;

	if (extra_spacing)
		AddExtraCharSpacing(&m_out.run.advances[out_len], len, extra_spacing);

	// Update the total length of the output buffer.
	out_len += len;

	// If this word ended outside on the right || left side of the clip rect. then we need to flush now so we don't draw the whole string.
	if (m_out.pen_pos_x > m_out.clip.Right() || point.x < m_out.clip.x)
		flushFontBatch();
}

void DWriteTextAnalyzer::flushFontBatch()
{
	// If there is nothing to flush, then return.
	if (!m_out.HasOutstandingBatch())
		return;

	// Get the render target.
	ID2D1RenderTarget* rt;
	if (m_out.dest->getType() == VEGA3dRenderTarget::VEGA3D_RT_WINDOW)
		rt = static_cast<VEGAD3d10Window*>(m_out.dest)->getD2DRenderTarget(this);
	else
		rt = static_cast<VEGAD3d10FramebufferObject*>(m_out.dest)->getD2DRenderTarget(!m_out.is_window, this);

	if (!rt)
	{
		m_out.ResetBatch();
		return;
	}

	// Set the clip rect for the output string.
	const D2D1_RECT_F d2_rect = {m_out.clip.x, m_out.clip.y, m_out.clip.Right() , m_out.clip.Bottom()};
	rt->PushAxisAlignedClip(d2_rect, D2D1_ANTIALIAS_MODE_ALIASED); // D2D1_ANTIALIAS_MODE_PER_PRIMITIVE ?

	// Set a brush
	ID2D1SolidColorBrush* brush;
	rt->CreateSolidColorBrush(D2D1::ColorF(m_out.color&0xffffff, ((float)(m_out.color>>24))/255.f),  &brush);

	// Draw glyph run
	DWriteGlyphRun* run = m_out.first_run ? m_out.first_run : &m_out.run;
	DWRITE_GLYPH_RUN dw_glyph_run;
	dw_glyph_run.bidiLevel = FALSE;
	dw_glyph_run.fontEmSize = m_out.font->Size();
	dw_glyph_run.fontFace = m_out.font->GetFontFace();
	dw_glyph_run.glyphAdvances = run->advances;
	dw_glyph_run.glyphCount = run->len;
	dw_glyph_run.glyphIndices = run->indices;
	dw_glyph_run.glyphOffsets = run->offsets;
	dw_glyph_run.isSideways	= FALSE;

	D2D_POINT_2F d2_pos = {m_out.start_pos.x, m_out.start_pos.y + m_out.font->Ascent()};
	rt->DrawGlyphRun(d2_pos, &dw_glyph_run, brush, DWriteTextAnalyzer::s_measuring_mode);

	m_out.ResetBatch();
	brush->Release();
	rt->PopAxisAlignedClip();
}

void DWriteTextAnalyzer::discardBatch()
{
	m_out.ResetBatch();
}
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY


// -------------------------------------
// DWriteOutlineConverter
// -------------------------------------

DWriteOutlineConverter::DWriteOutlineConverter()
	:path(NULL)
{

}

DWriteOutlineConverter::~DWriteOutlineConverter()
{
	OP_DELETE(path);
	path = NULL;
}

OP_STATUS DWriteOutlineConverter::Construct()
{
	RETURN_IF_ERROR(g_svg_manager->CreatePath(&path));
	RETURN_OOM_IF_NULL(path);
	return OpStatus::OK;
}

void DWriteOutlineConverter::AddBeziers(const D2D1_BEZIER_SEGMENT *beziers,UINT beziersCount)
{
	for(UINT i=0; i<beziersCount; i++)
	{
		const D2D1_BEZIER_SEGMENT* segment = beziers + i;
		path->CubicCurveTo(segment->point1.x, -segment->point1.y,
			segment->point2.x, -segment->point2.y,
			segment->point3.x, -segment->point3.y,
			FALSE, FALSE);
	}
}

void DWriteOutlineConverter::AddLines(const D2D1_POINT_2F *points,UINT pointsCount)
{
	for(UINT i=0; i<pointsCount; i++)
	{
		path->LineTo(points[i].x, -points[i].y, FALSE);
	}
}

void DWriteOutlineConverter::BeginFigure(D2D1_POINT_2F startPoint,D2D1_FIGURE_BEGIN figureBegin)
{
	path->MoveTo(startPoint.x, -startPoint.y, FALSE);
}

HRESULT DWriteOutlineConverter::Close()
{
	if(OpStatus::IsSuccess(path->Close()))
		return S_OK;
	else
		return E_FAIL;
}

void DWriteOutlineConverter::EndFigure(D2D1_FIGURE_END figureEnd)
{
	OP_ASSERT(D2D1_FIGURE_END_CLOSED == figureEnd);
	if(D2D1_FIGURE_END_CLOSED == figureEnd)
		path->Close();
}

void DWriteOutlineConverter::SetFillMode(D2D1_FILL_MODE )
{

}

void DWriteOutlineConverter::SetSegmentFlags(D2D1_PATH_SEGMENT )
{
	
}

//
// DWriteOpFont
// - The DirectWrite implementation of VEGAFont.
//

DWriteOpFont::DWriteOpFont(DWriteTextAnalyzer* analyzer, IDWriteFontFace* dw_font_face, UINT32 size, UINT8 weight, BOOL italic, INT32 blur_radius)
	:  m_analyzer(analyzer)
	  ,m_dw_font_face(dw_font_face)
	  ,m_size(size)
	  ,m_weight(weight)
	  ,m_italic(italic)
	  ,m_str_cache(NULL)
																														 ,m_blur_radius(blur_radius)
{
	m_ascii_buffer.str = NULL;

	m_analyzer->GetCurrentFontMetrics(this, m_font_metrics);
	DWRITE_FONT_METRICS& met = m_font_metrics;

	m_ascent = (m_size * met.ascent + (met.designUnitsPerEm>>1)) / met.designUnitsPerEm;
	m_descent = (m_size * met.descent + (met.designUnitsPerEm>>1)) / met.designUnitsPerEm;
	m_height = (m_size * (met.ascent + met.descent + met.lineGap) + (met.designUnitsPerEm>>1)) / met.designUnitsPerEm;
	m_internal_leading = m_height - m_size;

	// FIXME: these 2 might be wrong
	m_overhang = 0;
	m_max_advance = m_height*2;

	op_memset(m_next_cache_index, 0, sizeof(UINT ) * CACHED_GLYPH_RUN_GROUPS);
	m_str_cache = m_analyzer->GetGlyphRuns();

	op_memset(&m_ascii_buffer, 0, sizeof(DWriteString));
	m_analyzer->GetAsciiChars(this, &m_ascii_buffer);

#ifdef _DEBUG
	++DWriteOpFontManager::num_dw_opfonts;
#endif //_DEBUG
}

DWriteOpFont::~DWriteOpFont()
{
	// Currently there is some code that creates fonts (outside the font cache) just to draw one or a few strings,
	// for so to delete the font, while there is still a font batch outstanding.
	m_analyzer->OnDeletingFont(this);

	// Her m_ascii_buffer.str is not supposed to be deleted.
	m_ascii_buffer.run.Delete();
	m_analyzer->FreeGlyphRuns(m_str_cache);

	if (m_dw_font_face)
		m_dw_font_face->Release();

#ifdef _DEBUG
	--DWriteOpFontManager::num_dw_opfonts;
#endif //_DEBUG
}

#ifdef SVG_SUPPORT
OP_STATUS DWriteOpFont::GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph)
{
	// If NULL this is a test to see if GetOutline is supported
	if (!out_glyph)
		return OpStatus::OK;

	if (io_str_pos >= in_len)
		return OpStatus::ERR_OUT_OF_RANGE;


	DWriteOutlineConverter converter;
	RETURN_IF_ERROR(converter.Construct());

	OP_ASSERT(io_str_pos == 0);
	DWriteGlyphRun* run = GetGlyphRun(in_str + io_str_pos, in_len - io_str_pos);
	if(run)
	{
		if(FAILED(m_dw_font_face->GetGlyphRunOutline(m_size, run->indices,
			run->advances,
			run->offsets,
			run->len, FALSE, FALSE, &converter)))
			return OpStatus::ERR;

		out_advance = run->width;
		*out_glyph = converter.ReleasePath();
		io_str_pos = in_len;
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}
#endif // SVG_SUPPORT

UINT32 DWriteOpFont::StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing)
{
	bool ascii_path = true;

	if (m_ascii_buffer.str)
	{
		FLOAT width = .5f;

		for (UINT i = 0; i < len; ++i)
		{
			if (str[i] >= NUM_ASCII_CHARS)
			{
				ascii_path = false;
				break;
			}
			width += m_ascii_buffer.run.advances[str[i]];
		}
		if (ascii_path)
			return width + (extra_char_spacing * (INT32)len);
	}

	// The width is the sum of all advances.
	DWriteGlyphRun* run = GetGlyphRun(str, len);

	if (!run)
		return 0;

	return 0.5f + run->width + (extra_char_spacing * (INT32)run->len);
}

BOOL DWriteOpFont::DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool is_window)
{
	if (!len)
		return TRUE;

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY

	m_analyzer->SetDrawState(this, color, pos, clip, dest, is_window);

	if (m_ascii_buffer.run.len)
	{
		bool use_cache = true;
		for (UINT i = 0; i < len; ++i)
		{
			if (str[i] >= NUM_ASCII_CHARS)
			{
				use_cache = false;
				break;
			}
		}

		if (use_cache)
		{
			m_analyzer->BatchText(str, len, &m_ascii_buffer.run, pos, extra_char_spacing);
			return TRUE;
		}
	}

	DWriteGlyphRun* run = GetGlyphRun(str, len);

	if (run)
		m_analyzer->BatchText(run, pos, extra_char_spacing);

	return TRUE;

#else
	OP_ASSERT(FALSE);
	return FALSE;
#endif
}

BOOL DWriteOpFont::GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride)
{
	OP_ASSERT(FALSE);
	return FALSE;
}

DWriteGlyphRun* DWriteOpFont::GetGlyphRun(const uni_char* str, UINT32 len)
{

	//
	// Look in the cache to see if we have already saved the whole run.
	//

	UINT start_index = str[0] % CACHED_GLYPH_RUN_GROUPS;
	UINT end_index = (start_index * CACHED_GLYPH_RUN_GROUPS) + CACHED_GLYPH_RUN_GROUP_LEN;

	// Go through the cached glyph runs, and see if we already have stored a similar one.
	for (UINT i = start_index * CACHED_GLYPH_RUN_GROUPS; i < end_index; ++i)
	{
		// If this cache position is not filled yet. No reason to go further.
		// If the length is 0, the rest of the data is either 0 or rests from the previous font using the cache.
		if (m_str_cache[i].len == 0)
			break;

		// Check if this run has the exact same text as what we are looking for.
		if (len == m_str_cache[i].len && uni_strncmp(str, m_str_cache[i].str, len) == 0)
			return &m_str_cache[i].run;
	}

	//
	// So there was no cache hit. Reuse one of those we have.
	//

	DWriteString* dw_string = &m_str_cache[(start_index * CACHED_GLYPH_RUN_GROUPS) + m_next_cache_index[start_index]];

	if (OpStatus::IsError(dw_string->SetString(str, len)))
		return NULL;

	// Fill out the indices/advances/offsets for the glyphs corresponding to the string.
	if (OpStatus::IsError(m_analyzer->AnalyzeGlyphRun(this, dw_string)))
		return NULL;

	// This ensures we move the index from 0->(CACHED_GLYPH_RUNS-1) in a circular way.
	m_next_cache_index[start_index] = (++m_next_cache_index[start_index]) % CACHED_GLYPH_RUN_GROUP_LEN;

	return &dw_string->run;
}

OP_STATUS DWriteOpFont::getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex)
{
	// Get the glyph index of the UnicodePoint.
	UINT16 glyph_index;
	if (FAILED(m_dw_font_face->GetGlyphIndices(&c, 1, &glyph_index)))
		return OpStatus::ERR;

	// Get the glyph metrics.
	DWRITE_GLYPH_METRICS glyph_metrics = {0};
	if (FAILED(m_dw_font_face->GetDesignGlyphMetrics(&glyph_index, 1, &glyph_metrics)))
		return OpStatus::ERR;

	// Get the font metrics.
	UINT16 duPerEm = m_font_metrics.designUnitsPerEm;

	// Calculate the glyph metrics depending on the font size and metrics.
	glyph.left = 0;
	glyph.advance = (m_size * (glyph_metrics.advanceWidth) + (duPerEm>>1)) / duPerEm;
	glyph.height = (m_size * (glyph_metrics.advanceHeight) + (duPerEm>>1)) / duPerEm;
	glyph.width = (m_size * (glyph_metrics.advanceWidth) + (duPerEm>>1)) / duPerEm;
	glyph.top =  (m_size * (glyph_metrics.advanceHeight - glyph_metrics.bottomSideBearing - glyph_metrics.topSideBearing) + (duPerEm>>1)) / duPerEm;

	return OpStatus::OK;
}

//
// DWriteOpWebFont
// - Class encapsulating PLATFORM_WEBFONT and PLATFORM_LOCAL_WEBFONT.
//

DWriteOpWebFont::DWriteOpWebFont() : m_dw_font_face(NULL)
{
#ifdef _DEBUG
	++DWriteOpFontManager::num_dw_webfonts;
#endif //_DEBUG
};

DWriteOpWebFont::~DWriteOpWebFont() 
{
	for (UINT i = 0; i < m_synthesized_types.GetCount(); ++i)
	{
		OP_DELETE(m_synthesized_types.Get(i));
	}

	if (m_dw_font_face)
		m_dw_font_face->Release();

#ifdef _DEBUG
	--DWriteOpFontManager::num_dw_webfonts;
#endif // _DEBUG
};


OP_STATUS DWriteOpWebFont::CreateWebFont(IDWriteFactory* dwrite_factory, const uni_char* path, BOOL synthesize_bold, BOOL synthesize_italic)
{
	OP_ASSERT(!path || "You can't create a web font, if you don't have the path to it");

	RETURN_IF_ERROR(m_path.Set(path));
	m_web_font_type = OpFontInfo::PLATFORM_WEBFONT;

	// Add simulations if nessesary
	DWRITE_FONT_SIMULATIONS simulations = DWRITE_FONT_SIMULATIONS_NONE;

	if (synthesize_bold)
		simulations = DWRITE_FONT_SIMULATIONS_BOLD;

	if (synthesize_italic)
		simulations |= DWRITE_FONT_SIMULATIONS_OBLIQUE;

	// Create the font file
	IDWriteFontFile* dw_font_file;
	if (FAILED(dwrite_factory->CreateFontFileReference(path, NULL, &dw_font_file)))
		return OpStatus::ERR;

	OpComPtr<IDWriteFontFile> dw_font_file_ptr(dw_font_file);

	// Check if font type is supported, and if so, what type it is.
	BOOL is_supported_font_type;
	DWRITE_FONT_FILE_TYPE font_file_type;
	DWRITE_FONT_FACE_TYPE font_face_type;
	UINT32 num_faces;
	if (FAILED(dw_font_file->Analyze(&is_supported_font_type, &font_file_type, &font_face_type, &num_faces)))
		return OpStatus::ERR;

	if (!is_supported_font_type)
		return OpStatus::ERR_NOT_SUPPORTED;

	// Create the face font object.
	IDWriteFontFace* dw_font_face;
	if (FAILED(dwrite_factory->CreateFontFace(font_face_type, 1, &dw_font_file, 0, simulations, &dw_font_face)))
		return OpStatus::ERR;

	m_dw_font_face = dw_font_face;

	// Rename the font face because of problems with name collisions
	OpString face;
	const OpStringC prefix_name(UNI_L("font_"));
	if (!face.Reserve(320) || OpStatus::IsError(face.Insert(0, prefix_name)))
		return OpStatus::ERR_NO_MEMORY;

	uni_itoa(INTPTR(this), face.CStr() + prefix_name.Length(), 10);

	const UINT8* renamed_font;
	size_t renamed_font_size;
	uni_char* orig_name;
	if (OpStatus::IsError(RenameFont(path, face.CStr(), renamed_font, renamed_font_size, &orig_name)))
		return OpStatus::ERR;

	RETURN_IF_ERROR(SetFaceName(face.CStr(), orig_name));

	return OpStatus::OK;
}


OP_STATUS DWriteOpWebFont::CreateLocalFont(IDWriteFontCollection* font_collection, const uni_char* face_name)
{
	m_web_font_type = OpFontInfo::PLATFORM_LOCAL_WEBFONT;

	OpString font_face_name;
	RETURN_IF_ERROR(font_face_name.Set(face_name));
	font_face_name.Strip();
	
	UINT32 index;
	BOOL exists;
	if (FAILED(font_collection->FindFamilyName(font_face_name.CStr(), &index, &exists)) || !exists)
		return OpStatus::ERR;

	IDWriteFontFamily* dw_font_family;
	if (FAILED(font_collection->GetFontFamily(index, &dw_font_family)))
		return OpStatus::ERR;

	OpComPtr<IDWriteFontFamily> font_family_ptr(dw_font_family);

	IDWriteFont* dw_font;
	if (FAILED(dw_font_family->GetFont(0, &dw_font)))
		return OpStatus::ERR;

	OpComPtr<IDWriteFont> dw_font_ptr(dw_font);

	IDWriteFontFace* dw_font_face;
	if (FAILED(dw_font->CreateFontFace(&dw_font_face)))
		return OpStatus::ERR;

	RETURN_IF_ERROR(SetFaceName(font_face_name.CStr()));

	m_dw_font_face = dw_font_face;
	
	return OpStatus::OK;
}

//
// DWriteOpFontManager
// - Enumerates and manages system fonts, and adds removes webfonts and local fonts.
//

#ifdef _DEBUG
int DWriteOpFontManager::num_dw_opfonts = 0;
int DWriteOpFontManager::num_dw_webfonts = 0;
#endif // _DEBUG

DWriteOpFontManager::DWriteOpFontManager() : m_dwrite_factory(NULL),
											 m_font_collection(NULL),
											 m_analyzer(NULL),
											 m_dwriteDLL(NULL),
											 m_num_fonts(0),
											 m_font_info_list(NULL)
{}

DWriteOpFontManager::~DWriteOpFontManager()
{
	OP_DELETEA(m_font_info_list);
	m_font_info_list = NULL;

	if (m_font_collection)
		m_font_collection->Release();

	if (m_dwrite_factory)
		m_dwrite_factory->Release();

	OP_DELETE(m_analyzer);

	// Make sure to be done with any dwrite cleanup before unloading the dll
	OP_DELETE(m_dwriteDLL);

	OP_ASSERT(m_web_fonts.GetCount() == 0);
	OP_ASSERT(DWriteOpFontManager::num_dw_opfonts == 0);
	OP_ASSERT(DWriteOpFontManager::num_dw_webfonts == 0);
}

OP_STATUS DWriteOpFontManager::Construct()
{
	RETURN_IF_ERROR(OpDLL::Create(&m_dwriteDLL));
	RETURN_IF_ERROR(m_dwriteDLL->Load(UNI_L("dwrite.dll")));

	PFNDWRITECREATEFACTORYPROC create_factory = (PFNDWRITECREATEFACTORYPROC)m_dwriteDLL->GetSymbolAddress("DWriteCreateFactory");
	RETURN_VALUE_IF_NULL(create_factory, OpStatus::ERR);

	if (FAILED(create_factory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_dwrite_factory))))
		return OpStatus::ERR;

	if (FAILED(m_dwrite_factory->GetSystemFontCollection(&m_font_collection)))
		return OpStatus::ERR;

	m_analyzer = OP_NEW(DWriteTextAnalyzer, ());
	RETURN_OOM_IF_NULL(m_analyzer);
	RETURN_IF_ERROR(m_analyzer->Construct(m_dwrite_factory));

	return EnumerateFontInfo();
}

VEGAFont* DWriteOpFontManager::GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius)
{
	DWRITE_FONT_WEIGHT dw_weight = ToDWriteWeight(weight);

	UINT32 idx = 0;
	BOOL exists = FALSE;
	if (FAILED(m_font_collection->FindFamilyName(face, &idx, &exists)) || !exists)
		return NULL;

	IDWriteFontFamily* dw_font_family = NULL;
	if (FAILED(m_font_collection->GetFontFamily(idx, &dw_font_family)))
		return NULL;

	IDWriteFont* dw_font = NULL;
	HRESULT hr = dw_font_family->GetFirstMatchingFont(dw_weight, DWRITE_FONT_STRETCH_NORMAL, italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, &dw_font);
	dw_font_family->Release();
	if (FAILED(hr))
		return NULL;

	IDWriteFontFace* dw_font_face;
	hr = dw_font->CreateFontFace(&dw_font_face);
	dw_font->Release();
	if (FAILED(hr))
		return NULL;

	DWriteOpFont* font = OP_NEW(DWriteOpFont, (m_analyzer, dw_font_face, size, weight, italic, blur_radius));
	if (!font)
		dw_font_face->Release();

#ifdef _DEBUG
	if (font)
		font->SetFontName(face);
#endif

	return font;
}

#ifdef PI_WEBFONT_OPAQUE
VEGAFont* DWriteOpFontManager::GetVegaFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	return GetVegaFont(webfont, 4, FALSE, size, blur_radius);
}

VEGAFont* DWriteOpFontManager::GetVegaFont(OpWebFontRef web_font, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius)
{
	DWriteOpWebFont* dwrite_web_font = (DWriteOpWebFont*)web_font;
	IDWriteFontFace* dw_font_face = NULL;

	if (dwrite_web_font->GetWebFontType() == OpFontInfo::PLATFORM_WEBFONT)
	{
		OpAutoPtr<DWriteOpWebFont> web_font_synthesized(OP_NEW(DWriteOpWebFont, ()));

		if (!web_font_synthesized.get())
			return NULL;

		if (OpStatus::IsError(web_font_synthesized->CreateWebFont(m_dwrite_factory, dwrite_web_font->GetPath(), weight > 4, italic)))
			return NULL;

		dw_font_face = web_font_synthesized->GetFontFace();

		if (OpStatus::IsError(dwrite_web_font->AddSynthesizedFont(web_font_synthesized.get())))
			return NULL;

		web_font_synthesized.release();
	}
	else
	{
		dw_font_face = dwrite_web_font->GetFontFace();
	}

	dw_font_face->AddRef();

	// We make a VegaFont, which core will take the responsible to delete them selves. See FontCache::PurgeWebFont.
	DWriteOpFont* font = OP_NEW(DWriteOpFont, (m_analyzer, dw_font_face, size, weight, italic, blur_radius));

#ifdef _DEBUG
	if (font)
		font->SetFontName(dwrite_web_font->GetFaceName());
#endif

	return font;
}

OP_STATUS DWriteOpFontManager::GetLocalFont(OpWebFontRef& local_font, const uni_char* face_name)
{
	// NB: This function should return OpStatus::OK even if we don't find the web font. Only exception is on OOM.

	if (!m_font_collection || !face_name)
		return OpStatus::ERR_NULL_POINTER;

	local_font = NULL;
	
	OpAutoPtr<DWriteOpWebFont> dw_web_font(OP_NEW(DWriteOpWebFont, ()));
	RETURN_OOM_IF_NULL(dw_web_font.get());

	if (OpStatus::IsError(dw_web_font->CreateLocalFont(m_font_collection, face_name)))
		return OpStatus::OK;

	RETURN_IF_ERROR(m_web_fonts.Add(dw_web_font.get()));

	local_font = (OpWebFontRef)dw_web_font.release();

	return OpStatus::OK;
}

OpFontInfo::FontType DWriteOpFontManager::GetWebFontType(OpWebFontRef web_font)
{
	DWriteOpWebFont* dwrite_web_font = (DWriteOpWebFont*)web_font;
	return dwrite_web_font->GetWebFontType();
}

OP_STATUS DWriteOpFontManager::AddWebFont(OpWebFontRef& web_font, const uni_char* path)
{	
	if (!path || !*path)
		return OpStatus::ERR;

	web_font = NULL;

	OpAutoPtr<DWriteOpWebFont> dwrite_web_font(OP_NEW(DWriteOpWebFont, ()));

	RETURN_OOM_IF_NULL(dwrite_web_font.get());
	RETURN_IF_ERROR(dwrite_web_font->CreateWebFont(m_dwrite_factory, path, false, false));

	web_font = (OpWebFontRef)dwrite_web_font.get();

	RETURN_IF_ERROR(m_web_fonts.Add(dwrite_web_font.get()));

	dwrite_web_font.release();

	return OpStatus::OK;
}

OP_STATUS DWriteOpFontManager::RemoveWebFont(OpWebFontRef web_font)
{
	DWriteOpWebFont* dwrite_web_font = (DWriteOpWebFont*) web_font;
	return m_web_fonts.Delete(dwrite_web_font);
}

OP_STATUS DWriteOpFontManager::GetWebFontInfo(OpWebFontRef web_font, OpFontInfo* font_info)
{
	DWriteOpWebFont* dwrite_web_font = reinterpret_cast<DWriteOpWebFont*>(web_font);
	IDWriteFontFace* dw_font_face =  dwrite_web_font->GetFontFace();

	DWriteOpFontInfo dw_font_info;
	RETURN_IF_ERROR(dw_font_info.InitFontFaceInfo(dw_font_face));
	RETURN_IF_ERROR(dw_font_info.CreateCopy(font_info));

	RETURN_IF_ERROR(font_info->SetFace(dwrite_web_font->GetOriginalFaceName()));
	font_info->SetFontType(OpFontInfo::PLATFORM_WEBFONT);
	font_info->SetWebFont(web_font);

	return OpStatus::OK;
}

BOOL DWriteOpFontManager::SupportsFormat(int format)
{
	return TRUE;
}
#endif // PI_WEBFONT_OPAQUE

OP_STATUS DWriteOpFontManager::EnumerateFontInfo()
{
	OP_NEW_DBG("DWriteOpFontManager::", "Font.Enumeration");

	UINT num_system_fonts = m_font_collection->GetFontFamilyCount();
	m_font_info_list = OP_NEWA(DWriteOpFontInfo, num_system_fonts);
	RETURN_OOM_IF_NULL(m_font_info_list);

	for (UINT font_nr = 0; font_nr < num_system_fonts; ++font_nr)
	{
		IDWriteFontFamily* dw_font_family = NULL;
		if (FAILED(m_font_collection->GetFontFamily(font_nr, &dw_font_family)))
			continue;

		OpComPtr<IDWriteFontFamily> font_family_ptr(dw_font_family);

		// Font list
		IDWriteFontList* dw_font_list = NULL;
		if (FAILED(dw_font_family->GetMatchingFonts(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &dw_font_list)))
			continue;

		OpComPtr<IDWriteFontList> font_list_ptr(dw_font_list);

		if (dw_font_list->GetFontCount() == 0)
			continue;

		// Font
		IDWriteFont* dw_font = NULL;
		if (FAILED(dw_font_list->GetFont(0, &dw_font)))
			continue;

		OpComPtr<IDWriteFont> font_ptr(dw_font);

		// Font face
		IDWriteFontFace* dw_font_face = NULL;
		if (FAILED(dw_font->CreateFontFace(&dw_font_face)))
			continue;

		OpComPtr<IDWriteFontFace> font_face_ptr(dw_font_face);

		DWriteOpFontInfo& font_info = m_font_info_list[m_num_fonts];

		if (OpStatus::IsError(font_info.InitFontFamilyInfo(dw_font_family, dw_font)))
			continue;

		if (OpStatus::IsError(font_info.InitFontFaceInfo(dw_font_face)))
			continue;

		OP_DBG((UNI_L("(%d): %s"), font_nr, font_info.GetFontName()));

		++m_num_fonts;
	}

	return m_num_fonts > 0 ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS DWriteOpFontManager::GetFontInfo(UINT32 font_nr, OpFontInfo* font_info)
{
	font_info->SetFontNumber(font_nr);
	return m_font_info_list[font_nr].CreateCopy(font_info);
}

UINT32 DWriteOpFontManager::CountFonts()
{
	return m_num_fonts;
}

OP_STATUS DWriteOpFontManager::BeginEnumeration()
{
	return OpStatus::OK;
}

OP_STATUS DWriteOpFontManager::EndEnumeration()
{
	return OpStatus::OK;
}

const uni_char* DWriteOpFontManager::GetGenericFontName(GenericFont generic_font, WritingSystem::Script script)
{
	if (OpStatus::IsError(GdiOpFontManager::InitGenericFont(m_serif_fonts, m_sansserif_fonts, m_cursive_fonts, m_fantasy_fonts, m_monospace_fonts)))
		return NULL;
	
	switch (generic_font)
	{
		case GENERIC_FONT_SERIF:
		{
			return m_serif_fonts.Item(script).CStr();
		}
		case GENERIC_FONT_SANSSERIF:
		{
			return m_sansserif_fonts.Item(script).CStr();
		}
		case GENERIC_FONT_CURSIVE:
		{
			return m_cursive_fonts.Item(script).CStr();
		}
		case GENERIC_FONT_FANTASY:
		{
			return m_fantasy_fonts.Item(script).CStr();
		}
		case GENERIC_FONT_MONOSPACE:
		{
			return m_monospace_fonts.Item(script).CStr();
		}
		default:
		{
			OP_ASSERT(FALSE);
			return NULL;
		}
	}
}

#ifdef _GLYPHTESTING_SUPPORT_
// Copied from mdefont.cpp
template<typename T, int SIZE>
static T encodeValue(const BYTE* bp)
{
	const int BYTE_BITS = 8;

	T val = 0;
	for (int i = 0; i < SIZE; ++i)
	{
		val = (val << BYTE_BITS) | bp[i];
	}
	return val;
}
// format 12 microsoft (platform ID 3) UCS-4 (encoding ID 10) sfnt subtable
static OP_STATUS ParseFormat12Table(const BYTE* cmap, const UINT32 offs, const UINT32 size, OpFontInfo* fontinfo)
{
	if (offs + 16 > size) // subtable (way) too small
		return OpStatus::ERR;

	const BYTE* tab = cmap + offs;

	const  UINT16 format = encodeValue<UINT16, 2>(tab);
	if (format != 12)
	{
		OP_ASSERT(!"unsupported format");
		return OpStatus::ERR;
	}

#ifdef DEBUG
	const UINT16 reserved = encodeValue<UINT16, 2>(tab+2);
	const UINT32 language = encodeValue<UINT32, 4>(tab + 8);
	OP_ASSERT(reserved == 0); // reserved - set to 0
	OP_ASSERT(language == 0); // language - should be 0
#endif // DEBUG

	const UINT32 length = encodeValue<UINT32, 4>(tab + 4);
	const UINT32 ngroups = encodeValue<UINT32, 4>(tab + 12);

	if (offs + length > size // size of subtable exceeds that of table
		|| 16 + 12*ngroups > length) // header + entries exceed recorded length
		return OpStatus::ERR;

	const UINT8* group = tab + 16;
	for (UINT32 i = 0; i < ngroups; ++i)
	{
		const UINT32 start = encodeValue<UINT32, 4>(group + 0);
		UINT32 end = encodeValue<UINT32, 4>(group + 4);
		// const UINT32 glyph = encodeValue<UINT32, 4>(group + 8);

		// currently glyph testing is only supported for BMP SMP and SIP
		const UnicodePoint highest = 0x2ffff;
		if (start > highest)
			break;
		if (end > highest)
			end = highest;

		OP_ASSERT(end >= start);

		for (UnicodePoint uc = start; uc <= end; ++uc)
			RETURN_IF_ERROR(fontinfo->SetGlyph(uc));

		group += 12;
	}
	OP_ASSERT(group <= tab + length);

	return OpStatus::OK;
}
// format 4 microsoft (platform ID 3) unicode (encoding ID 1) sfnt subtable
static OP_STATUS ParseFormat4Table(const BYTE* cmap, const UINT32 offs, const UINT32 size, OpFontInfo* fontinfo)
{
	if (offs + 14 > size) // subtable (way) too small
		return OpStatus::ERR;

	const BYTE* tab = cmap + offs;

	const  UINT16 format = encodeValue<UINT16, 2>(tab);
	if (format != 4)
	{
		OP_ASSERT(!"unsupported format");
		return OpStatus::ERR;
	}

	const UINT16 length = encodeValue<UINT16, 2>(tab + 2);
	if (offs + length > size) // size of subtable exceeds that of table
		return OpStatus::ERR;
	const BYTE* tabEnd = tab + length;

	const UINT16 segCount2 = encodeValue<UINT16, 2>(tab + 6);
	if (length < 16 + 4*segCount2) // subtable not big enough to hold data
		return OpStatus::ERR;

#ifdef DEBUG
	const UINT16 language = encodeValue<UINT16, 2>(tab + 4);
	OP_ASSERT(language == 0); // language field - must be 0
	const UINT16 reservePad = encodeValue<UINT16, 2>(tab + 14 + segCount2);
	OP_ASSERT(reservePad == 0); // reservedPad - should be 0
#endif // DEBUG

	const BYTE* endCount    = tab + 14;
	const BYTE* startCount  = endCount + 2 + segCount2;
	const BYTE* idDelta     = startCount + segCount2;
	const BYTE* rangeOffset = idDelta + segCount2;

	// loop over segments
	for (unsigned int seg2 = 0; seg2 < segCount2; seg2 += 2)
	{
		const UINT16 end    = encodeValue<UINT16, 2>(endCount    + seg2);
		const UINT16 start  = encodeValue<UINT16, 2>(startCount  + seg2);
		const INT16  delta  = encodeValue<INT16,  2>(idDelta     + seg2);
		const UINT16 offset = encodeValue<UINT16, 2>(rangeOffset + seg2);
		for (unsigned int u = start; u <= end; ++u)
		{
			// determine glyph id
			UINT16 g_id;
			if (!offset	||
				offset == 0xffff) // (at least) AkrutiMal1 uses this to mean missing glyph
				g_id = delta + u;
			else
			{
				const BYTE* glyphId = rangeOffset + seg2 + /*mod 65536*/(UINT16)(offset + 2*(u - start));
				g_id = /* bounds check */(glyphId + 2 > tabEnd) ? 0 : encodeValue<UINT16, 2>(glyphId);
				if (g_id)
					g_id += delta;
			}
			if (g_id)
				RETURN_IF_ERROR(fontinfo->SetGlyph(u));
			if (u == 0xffff)
				break;
		}
		if (end == 0xffff)
		{
			RETURN_IF_ERROR(fontinfo->ClearGlyph(end));
			break;
		}
	}
	return OpStatus::OK;
}

void DWriteOpFontManager::UpdateGlyphMask(OpFontInfo *fontinfo)
{
	const uni_char* face_name;
	IDWriteFontFace* dwrite_face;

	if (fontinfo->GetWebFont() != 0)
	{
		DWriteOpWebFont* webfont_info = (DWriteOpWebFont*)(fontinfo->GetWebFont());
		face_name = webfont_info->GetFaceName();
		dwrite_face = webfont_info->GetFontFace();
	}
	else
	{
		face_name = fontinfo->GetFace();

		IDWriteFontFamily* family = NULL;
		HRESULT hr = m_font_collection->GetFontFamily(fontinfo->GetFontNumber(), &family);
		if (FAILED(hr))
			return;

		IDWriteFont* dwfnt = NULL;
		hr = family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &dwfnt);
		family->Release();
		if (FAILED(hr))
			return;

		hr = dwfnt->CreateFontFace(&dwrite_face);
		dwfnt->Release();
		if (FAILED(hr))
			return;
	}

	BYTE* cmap;
	UINT32 size;
	void* cmapCtx;
	BOOL hasCMap;
	HRESULT hr = dwrite_face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('c', 'm', 'a', 'p'), (const void**)&cmap, &size, &cmapCtx, &hasCMap);
	if (FAILED(hr) || !hasCMap || size < 4)
	{
		dwrite_face->ReleaseFontTable(cmapCtx);
		dwrite_face->Release();
		return;
	}

#ifdef _DEBUG
	const UINT16 version = encodeValue<UINT16, 2>(cmap + 0);
	OP_ASSERT(version == 0); // format says this should be 0
#endif

	const UINT16 count = encodeValue<UINT16, 2>(cmap + 2);

	if (size < 8u * count) // not enough room for records
	{
		dwrite_face->ReleaseFontTable(cmapCtx);
		dwrite_face->Release();
		return;
	}

	// parse subtables
	// currently only support for Microsoft
	// * format 4 - Unicode 'Segment mapping to delta values'. "All
	//   Microsoft Unicode encodings (Platform ID = 3, Encoding ID = 1)
	//   must provide at least a Format 4 'cmap' subtable". for BMP
	//   unicode points.
	// * format 12 - UCS-4 'Segmented coverage'. for non-BMP unicode
	//   points.

	const UINT16 platformMS   = 3; // microsoft
	const UINT16 encodingUni  = 1; // unicode
	const UINT16 encodingUCS4 = 10; // UCS-4
	const UINT16 format4  =  4; // segment mapping to delta values
	const UINT16 format12 = 12; // segmented coverage
	BOOL read_format4  = FALSE;
	BOOL read_format12 = FALSE;

	const BYTE* p = cmap + 4;
	OP_STATUS status = OpStatus::OK;
	for (UINT32 i = 0; i < count; ++i, p += 8)
	{
		if (encodeValue<UINT16, 2>(p + 0) == platformMS)
		{
			const UINT16 encoding = encodeValue<UINT16, 2>(p + 2);
			const UINT32 offset = encodeValue<UINT32, 4>(p + 4);

			if (size < offset + 2) // this subtable falls outside cmap table
				continue;

			const UINT16 format = encodeValue<UINT16, 2>(cmap + offset);
			if (!read_format4 && encoding == encodingUni && format == format4)
			{
				status = ParseFormat4Table(cmap, offset, size, fontinfo);
				if (OpStatus::IsError(status))
					break;
				read_format4 = TRUE;
				// both formats read - no need to take this further
				if (read_format12)
					break;
			}
			else if (!read_format12 && encoding == encodingUCS4 && format == format12)
			{
				status = ParseFormat12Table(cmap, offset, size, fontinfo);
				if (OpStatus::IsError(status))
					break;
				read_format12 = TRUE;
				// both formats read - no need to take this further
				if (read_format4)
					break;
			}
		}
	}

	if (!read_format4 && !read_format12) // no appropriate table found
		status = OpStatus::ERR;

	dwrite_face->ReleaseFontTable(cmapCtx);

	if (fontinfo->GetWebFont() == 0)
		dwrite_face->Release();

	return;
}
#endif // _GLYPHTESTING_SUPPORT_

BOOL DWriteOpFontManager::HasWeight(UINT32 font_nr, UINT8 weight)
{
	if (font_nr >= CountFonts())
		return FALSE;

	IDWriteFontFamily* dw_font_family = NULL;
	if (FAILED(m_font_collection->GetFontFamily(font_nr, &dw_font_family)))
		return FALSE;

	OpComPtr<IDWriteFontFamily> font_family_ptr(dw_font_family);
	DWRITE_FONT_WEIGHT dw_weight = ToDWriteWeight(weight);

	IDWriteFontList* dw_font_list;
	if (FAILED(dw_font_family->GetMatchingFonts(dw_weight, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &dw_font_list)))
		return FALSE;

	OpComPtr<IDWriteFontList> font_list(dw_font_list);

	IDWriteFont* dw_font;
	if (FAILED(dw_font_list->GetFont(0, &dw_font)))
		return FALSE;

	OpComPtr<IDWriteFont> font(dw_font);

	if (dw_font->GetWeight() != dw_weight)
		return FALSE;

	return TRUE;
}

