/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef POSTSCRIPT_PROLOG_H
#define POSTSCRIPT_PROLOG_H

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/postscriptcommandbuffer.h"

class OpFileDescriptor;
class TrueTypeConverter;

/** @brief Helper class to abstract creation of a PostScript document prolog part (upper part with meta info and resources).
  */
class PostScriptProlog : public PostScriptCommandBuffer
{
public:
	PostScriptProlog();

	/** Destructor */
	~PostScriptProlog();

	/**
	  * @param file A closed, constructed file object - will be opened for writing
	  */
	OP_STATUS Init(OpFileDescriptor& file, PostScriptOutputMetrics* screen_metrics, PostScriptOutputMetrics* paper_metrics);

	/** Add a font to be included in the document. This involves converting the
	  * font to PostScript Type42 and writing the results in hexcode.
	  * 
	  * A hash table of currently included fonts makes sure that only new ones
	  * are added. This makes it safe to call this multiple times for the same font.
	  *
	  * @param font font to add to document
	  * @param ps_fontname upon return this will point to the
	    generated PostScript name for this font - pointer is owned by
	    current_handled_font
	  * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other error.
	  */
	OP_STATUS addFont(OpFont* font, const char*& ps_fontname);

	/** Finish generation of the prolog. This includes ending the setup part of the prolog */
	OP_STATUS finish();

	/** Return TrueTypeConverter for the currently used font
	  * @return pointer to TrueTypeConverter for currently used font. Null if no font is currently used. */
	TrueTypeConverter* getCurrentFontConverter();

	/** Get current font data and its size
	  * @param font_data (out) pointer to current font data
	  * @param font_data_size (out) current font data size */
	void getCurrentFontData(UINT8*& font_data, UINT32& font_data_size);

private:
	class HandledFont
	{
	public:
		HandledFont(UINT32 checksum, TrueTypeConverter* font_converter, OpString8& ps_fontname)
				: checksum(checksum), has_slanted_copy(false)
			{ this->ps_fontname.TakeOver(ps_fontname); this->font_converter = font_converter; }
		~HandledFont();

		UINT32 checksum;
		OpString8 ps_fontname;

		TrueTypeConverter* font_converter;

		bool has_slanted_copy;
		OpString8 ps_fontname_slanted;
	};

	/**
	   adds a truetype font

	   @param font should be a font in g_font_cache - on success
	   ownership will be transformed to this, font will be referenced
	   via g_font_cache->ReferenceFont(...) and later released via
	   g_font_cache->ReleaseFont(...)

	   @param font_data should be fetched using
	   OpFont::GetFontData(...) - on success ownership will be
	   trasferred to this and later released via
	   OpFont::ReleaseFontData

	   @param font_data_size the size of the font data
	   @param ps_fontname the internal name of the font (OperaFontX) - this pointer is owned by the HandledFont
	 */
	OP_STATUS addTrueTypeFont(OpFont* font, UINT8* font_data, UINT32 font_data_size, const char*& ps_fontname);
	/**
	   creates a new HandledFont and stores it in handled_fonts

	   @param font_converter will be deleted on failure, on success ownership is transferred to handled_font
	   @param checksum GLYF checksum for the font
	   @param font_name the internal name of the font
	   @param handled_font (out) handle to the created HandledFont
	   @return OpStatus::OK on success, OpStatus::ERR on failure, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS addHandledFont(TrueTypeConverter* font_converter, UINT32 checksum, OpString8& font_name, HandledFont*& handled_font);
	bool checkNeedsManualSlant(OpFont* font, bool glyphs_are_italic);
	/**
	   @param slanted_fontname - owned by handled_font
	 */
	OP_STATUS getSlantedFontCopy(HandledFont* handled_font, const char*& slanted_fontname);

	OP_STATUS generateFontResource(TrueTypeConverter* converter, OpString8& ps_fontname);
	OP_STATUS addSuppliedResource(OpString8& resource_name);

	PostScriptOutputMetrics* screen_metrics;
	PostScriptOutputMetrics* paper_metrics;
	UINT32 font_count;

 	HandledFont* current_handled_font;
	OpFont* current_opfont;
	UINT8* current_font_data;
	UINT32 current_font_data_size;

	OpString8 document_supplied_resources;
	OpString8 document_slanted_fonts;

	OpAutoINT32HashTable<HandledFont> handled_fonts;
};


#endif // VEGA_POSTSCRIPT_PRINTING

#endif // POSTSCRIPT_P_H
