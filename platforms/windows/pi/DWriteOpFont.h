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

#ifndef DWRITEOPFONT_H
#define DWRITEOPFONT_H

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/util/opstrlst.h"
#include "platforms/vega_backends/d3d10/vegad3d10fbo.h"
#include "platforms/windows/pi/util/DirectWrite.h"

class  DWriteOpFont;
class  DWriteOpFontInfo;
class  SVGPath;

static const UINT NUM_ASCII_CHARS = 256;
static const UINT MISSING_GLYPH = NUM_ASCII_CHARS + 1;
static const UINT ASCII_SPACE_CHAR = 32;
static const UINT ASCII_QUOTE_CHAR = 34;
static const UINT ASCII_STAR_CHAR = 42;

static const UINT CACHED_GLYPH_RUNS = 1050;
static const UINT CACHED_GLYPH_RUN_GROUPS = 30;
static const UINT CACHED_GLYPH_RUN_GROUP_LEN = 35;

/*
 * Class TextAnalysis
 * - Used by the DWriteOpFont objects to analyze a text run.
 *
 */
class DWriteTextAnalyzer : public IDWriteTextAnalysisSink, public IDWriteTextAnalysisSource, public FontFlushListener
{
public:
	DWriteTextAnalyzer();
	~DWriteTextAnalyzer();
	OP_STATUS Construct(IDWriteFactory* dw_factory);

	// Set the analyzers measuring mode for text layout. All the fonts that has already been created will have to re calculate their members,
	// and the cache has to be invalidated, as they have been measured with the wrong mode.
	static void SetMeasuringMode(int measuring_mode);

	// IDWriteTextAnalysisSource
	STDMETHOD(GetLocaleName)(UINT32 text_position, UINT32* text_length, const WCHAR** locale_name);
	STDMETHOD(GetNumberSubstitution)(UINT32 text_position, UINT32* text_length, IDWriteNumberSubstitution **number_substitution);
	STDMETHOD_(DWRITE_READING_DIRECTION, GetParagraphReadingDirection)();
	STDMETHOD(GetTextAtPosition)(UINT32 text_position, const WCHAR **text_string, UINT32 *text_length);
	STDMETHOD(GetTextBeforePosition)(UINT32 text_position, const WCHAR **text_string, UINT32 *text_length);

	// IDWriteTextAnalysisSink
	STDMETHOD(SetBidiLevel)(UINT32 text_position, UINT32 text_length, UINT8 explicit_level, UINT8 resolved_level)				{ return S_OK; } // Not used 
	STDMETHOD(SetLineBreakpoints)(UINT32 text_position, UINT32 text_length, const DWRITE_LINE_BREAKPOINT* line_breakpoints)		{ return S_OK; } // Not used 
	STDMETHOD(SetNumberSubstitution)(UINT32 text_position, UINT32 text_length, IDWriteNumberSubstitution* number_substitution)	{ return S_OK; } // Not used 
	STDMETHOD(SetScriptAnalysis)(UINT32 text_position, UINT32 text_length, const DWRITE_SCRIPT_ANALYSIS* script_analysis);

	// IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) { return S_OK; }
	virtual ULONG STDMETHODCALLTYPE AddRef(void)  { return 0; }
	virtual ULONG STDMETHODCALLTYPE Release(void) { return 0; }

	// Fills in the struct with the fonts metrics with the current measuring mode. The font should ask for it every time init is called.
	void GetCurrentFontMetrics(DWriteOpFont* font, DWRITE_FONT_METRICS& font_metrics);

	// Function used to analyze a glyph run for a DWriteOpFont
	OP_STATUS AnalyzeGlyphRun(DWriteOpFont* font, DWriteString* dw_string);

	// Fill up a buffer with the glyphs and advances of the ascii chars. Every font should try to do this.
	void GetAsciiChars(DWriteOpFont* font, DWriteString* dw_string);

	// Returns CACHED_GLYPH_RUNS numbers of glyph runs for the font to cache finished runs.
	DWriteString* GetGlyphRuns();

	// The font needs to return the glyphs back when the font is deleted. So the next font created can make use of it.
	void FreeGlyphRuns(DWriteString* dw_string);

	// Needs to be called when a font is deleted. Will flush the current font batch if needed.
	void OnDeletingFont(DWriteOpFont* font);

#ifdef VEGA_BACKEND_D2D_INTEROPERABILITY
	// Sets the draw state for the text, and flush if the state changes
	void SetDrawState(DWriteOpFont* font, UINT32 color, const OpPoint& pos, const OpRect& clip, VEGA3dRenderTarget* dest, bool is_window);

	// Add the text to the outputbuffer
	void BatchText(DWriteGlyphRun* run, const OpPoint& point, FLOAT extra_spacing);

	// Instead of batching a whole run, this function copy one and one index/advance from the input buffer (run).
	void BatchText(const uni_char* str, UINT len, DWriteGlyphRun* run, const OpPoint& point, FLOAT extra_space);

	// VEGA3dRenderTarget::FontFlushListener implementation, for flushing a font batch.
	virtual void flushFontBatch();

	// VEGA3dRenderTarget::FontFlushListener implementation, throw away the text that is already batched up.
	virtual void discardBatch();
#endif // VEGA_BACKEND_D2D_INTEROPERABILITY

private:

	// The measuring mode for text layout. Either GDI like or natural.
	static DWRITE_MEASURING_MODE s_measuring_mode;
	
	// If the text needs shaping, the analyzer will call one of these functions to get advances and offsets depending on the current measuring mode.
	OP_STATUS GetGlyphPlacements(DWriteOpFont* font);
	OP_STATUS GetGDIGlyphPlacements(DWriteOpFont* font);

	// If extra character space is wanted, then change that manually in the output buffer, so the cached version stay the same.
	void AddExtraCharSpacing(FLOAT* advances, UINT len, FLOAT extra_spacing);

	// Helper function used to increase the max number of characters/glyphs we can analyze.
	OP_STATUS IncreaseMaxGlyphs(UINT new_size);

	// Analyzes various text properties for complex script processing.
	IDWriteTextAnalyzer* m_dw_text_analyzer;

	// Temporary storage of the string and run, used by the analysis source
	DWriteString* m_to_analyze;

	// A array of cached glyph runs. Each font will be given CACHED_GLYPH_RUNS numbers of runs.
	DWriteString* m_str_cache;

	// An array which tells what cached glyph runs are free to be re-used, and which one is not.
	bool m_unused_runs[FONTCACHESIZE];

	// The ascii characters that can be used by a DWriteString if a font asks for it.
	uni_char m_ascii_chars[NUM_ASCII_CHARS];

	// Temporary storage of the run we are analyzing, so we don't have to recreate it all the time.
	struct
	{
		UINT size;										// Max length the other members can hold
		UINT16* cluster_map;							// Mapping from char ranges to glyph ranges.
		DWRITE_SHAPING_TEXT_PROPERTIES* text_props;		// Per char shaping properties.
		DWRITE_SHAPING_GLYPH_PROPERTIES* glyph_props;	// Per glyph shaping properties.
		DWRITE_SCRIPT_ANALYSIS* analysis;				// Script and visual representation.
		INT32 reset_analysis;							// Reset analysis->shapes to DWRITE_SCRIPT_SHAPES_DEFAULT in this range.
	} m_analysis;

	// Output state and run, used to batch up drawing operations.
	struct
	{
		// state
		DWriteOpFont* font;								// Current font.
		UINT32 color;									// Current colour (RGBA).
		OpPoint start_pos;								// Start position of the whole line on the screen.
		FLOAT pen_pos_x;								// Current DirectWrite pen position at the end of this run.
		OpRect clip;									// Current clipping rect for this run.
		VEGA3dRenderTarget* dest;						// The current rendertarget we render to.
		bool is_window;									// Is TRUE if we are currently rendering to a window.
		INT reset_offsets;								// Reset the offsets to 0 from that glyph index to the end of m_out.run.

		// Current run
		DWriteGlyphRun run;								// The actual glyphs, advances, offsets in the output buffer.
		DWriteGlyphRun* first_run;						// Points at the first run if there is _exactly_ one run in the batch.

		bool HasOutstandingBatch() const				// Will return true if it has/points to glyphs that are not drawn to the render target yet.
			{ return run.len > 0 || first_run; };

		void ResetBatch()								// Reset the batcher to make sure the batch is never drawn to the screen.
			{ run.len = 0; first_run = NULL; }			// Usually only needed if the render target is deleted before EndDraw() is called on it.
	} m_out;
};

/*
 * Class DWriteOutlineConverter
 * - Used by the DWriteOpFont::GetOutline to construct a SVGPath of a glyphrun.
 *
 */
class DWriteOutlineConverter : public ID2D1SimplifiedGeometrySink
{
public:
	DWriteOutlineConverter();
	~DWriteOutlineConverter();

	OP_STATUS Construct();

	STDMETHOD_(void,AddBeziers)(const D2D1_BEZIER_SEGMENT *beziers,UINT beziersCount);

	STDMETHOD_(void,AddLines)(const D2D1_POINT_2F *points,UINT pointsCount);

	STDMETHOD_(void,BeginFigure)(D2D1_POINT_2F startPoint,D2D1_FIGURE_BEGIN figureBegin);

	STDMETHOD(Close)();

	STDMETHOD_(void,EndFigure)(D2D1_FIGURE_END figureEnd);

	STDMETHOD_(void,SetFillMode)(D2D1_FILL_MODE fillMode);

	STDMETHOD_(void,SetSegmentFlags)(D2D1_PATH_SEGMENT vertexFlags);

	// IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(/* [in] */ REFIID riid, /* [iid_is][out] */ __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject) { return S_OK; }
	virtual ULONG STDMETHODCALLTYPE AddRef(void)  { return 0; }
	virtual ULONG STDMETHODCALLTYPE Release(void) { return 0; }

	SVGPath* ReleasePath(){SVGPath* ret = path; path = 0; return ret;}
private:
	SVGPath* path;
};


class DWriteOpFont : public VEGAFont
{
public:
	DWriteOpFont(DWriteTextAnalyzer* analyzer, IDWriteFontFace* dw_font_face, UINT32 size, UINT8 weight, BOOL italic, INT32 blur_radius);
	~DWriteOpFont();

	virtual UINT32 Ascent()		{ return m_ascent;  }
	virtual UINT32 Descent()	{ return m_descent; }
	virtual UINT32 InternalLeading(){return m_internal_leading;}
	virtual UINT32 Height()		{ return m_height;  }
	virtual UINT32 Overhang()	{ return m_overhang;}
	virtual UINT32 MaxAdvance() { return m_max_advance;}

	virtual BOOL isBold()		{ return m_weight >= 6; }
	virtual BOOL isItalic()		{ return m_italic; }
	virtual int getBlurRadius() { return 0; }

	UINT32 Size() const			{ return m_size; }
	IDWriteFontFace* GetFontFace() const { return m_dw_font_face; }

#ifdef _DEBUG
	// Only used to verify what font we actually draw with during debugging.
	OP_STATUS SetFontName(const uni_char* font_name) { return m_font_name.Set(font_name); }
	const uni_char* GetFontname() const { return m_font_name.CStr(); }
#endif

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph);
#endif // SVG_SUPPORT

#ifdef MDF_CAP_PROCESSEDSTRING
	// Not used for native fonts
	virtual OP_STATUS ProcessString(struct ProcessedString* processed_string, const uni_char* str, const size_t len, INT32 extra_char_spacing = 0, short word_width = -1, bool use_glyph_indices = false)
		{ return OpStatus::ERR; }
#endif // MDF_CAP_PROCESSEDSTRING

	virtual BOOL	nativeRendering() { return TRUE; }
	virtual UINT32	StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing);
	virtual BOOL	DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGAWindow* dest) { OP_ASSERT(FALSE);	return TRUE; }
	virtual BOOL	DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGASWBuffer* dest) { OP_ASSERT(FALSE); return TRUE; }
	virtual BOOL	DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool isWindow);
	virtual BOOL	GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride);

protected:
	virtual OP_STATUS loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex = FALSE, BOOL blackOnWhite = TRUE) { OP_ASSERT(FALSE); return OpStatus::ERR; }
	virtual void	unloadGlyph(VEGAGlyph& glyph) { OP_ASSERT(FALSE); }
	virtual void	getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride) { OP_ASSERT(FALSE); }
	virtual OP_STATUS getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex);

private:

	DWriteGlyphRun* GetGlyphRun(const uni_char* str, UINT32 len);

#ifdef _DEBUG
	OpString m_font_name;			// Used to verify what font we actually draw with.
#endif
	
	IDWriteFontFace* m_dw_font_face;
	DWriteTextAnalyzer* m_analyzer;
	DWRITE_FONT_METRICS m_font_metrics;

	UINT32 m_ascent;
	UINT32 m_descent;
	UINT32 m_internal_leading;
	UINT32 m_height;
	UINT32 m_overhang;
	UINT32 m_max_advance;
	UINT8  m_weight;
	BOOL   m_italic;
	INT32  m_blur_radius;

	UINT32 m_size;

	// A cache of the extended ASCII.
	// .str should not be deleted. It points to a str in the analyzser.
	// .run part should be deleted
	DWriteString m_ascii_buffer;

	DWriteString* m_str_cache;								// A cache of CACHED_GLYPH_RUNS runs for this font.
	UINT m_next_cache_index[CACHED_GLYPH_RUN_GROUPS];		// The next cache run to be overwritten.
};

/*
 *
 * Class DWriteWebOpFont
 *	- Class encapsulating a DirectWrite web font
 *
 */

class DWriteOpWebFont
{
public:
	DWriteOpWebFont();
	~DWriteOpWebFont();

	OP_STATUS CreateWebFont(IDWriteFactory* dwrite_factory, const uni_char* path, BOOL synthesize_bold, BOOL synthesize_italic);
	OP_STATUS CreateLocalFont(IDWriteFontCollection* font_collection, const uni_char* face_name);

	OP_STATUS AddSynthesizedFont(DWriteOpWebFont* web_font)
		{
			return m_synthesized_types.Add(web_font);
		}

	OP_STATUS SetFaceName(const uni_char* renamed_face_name, const uni_char* original_name = NULL)
		{
			RETURN_IF_ERROR(m_face_name.Set(renamed_face_name));
			return m_original_face_name.Set(original_name);
		}

	const uni_char*		 GetFaceName() const		{ return m_face_name.CStr(); };
	const uni_char*		 GetOriginalFaceName() const{ return m_original_face_name.CStr(); };
	IDWriteFontFace*	 GetFontFace() const		{ return m_dw_font_face; };
	OpFontInfo::FontType GetWebFontType() const		{ return m_web_font_type;	 };
	const uni_char*		 GetPath() const			{ return m_path.CStr(); };

private:
	OpFontInfo::FontType m_web_font_type;
	IDWriteFontFace*	 m_dw_font_face;
	OpString			 m_face_name;
	OpString			 m_original_face_name;
	OpString			 m_path;					// Path to font file (LocalWebFonts do not have a file).
	OpVector<DWriteOpWebFont> m_synthesized_types;	// If we have made bold or italic types of the same web font, they should be added here.
};


/*
 *
 * Class DWriteOpFontManager
 *	- Is a DirectWrite VEGAOpFontManager implementation.
 *
 */

class DWriteOpFontManager : public VEGAOpFontManager
{
public:
	DWriteOpFontManager();
	~DWriteOpFontManager();
	OP_STATUS Construct();

	virtual VEGAFont* GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius);
#ifdef PI_WEBFONT_OPAQUE
	virtual VEGAFont* GetVegaFont(OpWebFontRef web_font, UINT32 size, INT32 blur_radius);
	virtual VEGAFont* GetVegaFont(OpWebFontRef web_font, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius);
	virtual OP_STATUS GetLocalFont(OpWebFontRef& local_font, const uni_char* face_name);
	virtual OpFontInfo::FontType GetWebFontType(OpWebFontRef web_font);
	virtual OP_STATUS AddWebFont(OpWebFontRef& webfont, const uni_char* path);
	virtual OP_STATUS RemoveWebFont(OpWebFontRef web_font);
 	virtual OP_STATUS GetWebFontInfo(OpWebFontRef web_font, OpFontInfo* font_info);
	virtual BOOL SupportsFormat(int format);
#endif // PI_WEBFONT_OPAQUE
	virtual UINT32 CountFonts();
	virtual OP_STATUS GetFontInfo(UINT32 font_nr, OpFontInfo* font_info);

	virtual OP_STATUS BeginEnumeration();
	virtual OP_STATUS EndEnumeration();
	virtual void BeforeStyleInit(class StyleManager* styl_man){}

	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script);

#ifdef _GLYPHTESTING_SUPPORT_
	virtual void UpdateGlyphMask(OpFontInfo *fontinfo);
#endif // _GLYPHTESTING_SUPPORT_

	// This function comes from TWEAK_PI_FONTMAN_HASWEIGHT to speed up start up.
	BOOL HasWeight(UINT32 font_nr, UINT8 weight);

#ifdef _DEBUG
	// Simple memory leak testing, to see if any fonts have leaked
	static int num_dw_opfonts;
	static int num_dw_webfonts;
#endif // _DEBUG

private:

	// Will create m_font_info_list array and fill inn the font info for all the fonts that does not fail to initialize.
	OP_STATUS EnumerateFontInfo();

	// Tries to find the localized font name, then falls back to using en-us if not found, and last it tries just the first name it finds.
	OP_STATUS GetFontFamilyName(IDWriteFontFamily* dw_font_family, OpString& font_name) const;

	// Is a helper function used to to fill out the common part of a OpFontInfo for system fonts, and web fonts.
	void		GetCommonFontInfo(IDWriteFontFace* dw_font_face, OpFontInfo*& font_info);

	IDWriteFactory* m_dwrite_factory;
	IDWriteFontCollection* m_font_collection;
	DWriteTextAnalyzer* m_analyzer;

	OpString_list m_serif_fonts;
	OpString_list m_sansserif_fonts;
	OpString_list m_cursive_fonts;
	OpString_list m_fantasy_fonts;
	OpString_list m_monospace_fonts;

	OpDLL* m_dwriteDLL;

	OpVector<DWriteOpWebFont>	m_web_fonts;

	// Number of fonts that was enumerated successfully.
	UINT m_num_fonts;

	// Will hold information about all the fonts that was enumerated during start-up (will not change during runtime at all).
	DWriteOpFontInfo* m_font_info_list;
};

#endif // DWRITEOPFONT_H
