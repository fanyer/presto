/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MDE_OPFONTMANAGER_H
#define MDE_OPFONTMANAGER_H

#include "modules/pi/OpFont.h"

struct MDE_GenericFonts
{
	/** The name of the font used for serif */
	const char* serif_font;
	/** The name of the font used for sans serif */
	const char* sans_serif_font;
	/** The name of the font used for cursives */
	const char* cursive_font;
	/** The name of the font used for fantasy */
	const char* fantasy_font;
	/** The name of the font used for monospace */
	const char* monospace_font;
	/** The default font */
	const char* default_font;
};

#ifdef _GLYPHTESTING_SUPPORT_
struct MDE_FONT;

template<typename T, int SIZE>
static T encodeValue(BYTE* bp)
{
	const int BYTE_BITS = 8;

	T val = 0;
	for (int i = 0; i < SIZE; ++i)
	{
		val = (val << BYTE_BITS) | bp[i];
	}
	return val;
}


#endif //_GLYPHTESTING_SUPPORT_

class MDE_OpFontManager : public OpFontManager
{
public:
	MDE_OpFontManager();
	virtual ~MDE_OpFontManager();

	void Clear();
	OP_STATUS InitFonts();

	/** Creates an OpFont object.
		@param face the name of the font
		@param size in pixels
		@param weight weight of the font (0-9). 4 is Normal, 7 is Bold.
		@param italic does the font contain italic glyphs?
		@param scale for the view where the font is used
		@return the created OpFont if it is valid or NULL. Please note that this font may
		        not have all the attributes that you specified. The font may not
		        be available in italic, for example.
	*/
	virtual OpFont* CreateFont(const uni_char* face, UINT32 size, UINT8 weight,
							   BOOL italic, BOOL hasoutline, INT32 blur_radius);

	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius);
	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius);
	virtual OP_STATUS AddWebFont(OpWebFontRef& webfont, const uni_char* full_path_of_file);
	virtual OP_STATUS RemoveWebFont(OpWebFontRef webfont);
	virtual OP_STATUS GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo);
	virtual OP_STATUS GetLocalFont(OpWebFontRef& localfont, const uni_char* facename);
	virtual BOOL SupportsFormat(int f);

	/** @return the number of fonts in the system */
	virtual UINT32 CountFonts();

	/** Get the font info for the specified font.
		@param fontnr the font you want the info for
		@param fontinfo an allocated fontinfo object
	*/
	virtual OP_STATUS GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo);

	/** Is called before first call to CountFonts or GetFontInfo.
	*/
	virtual OP_STATUS BeginEnumeration();

	/** Is called after all calls to CountFonts and GetFontInfo.
	*/
	virtual OP_STATUS EndEnumeration();

	virtual void BeforeStyleInit(class StyleManager* styl_man) {}

	/** Gets the name of the font that is default for the GenericFont.
	 */
	virtual const uni_char* GetGenericFontName(GenericFont generic_font);
#ifdef PERSCRIPT_GENERIC_FONT
	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script) { return GetGenericFontName(generic_font); }
#endif // PERSCRIPT_GENERIC_FONT

#ifdef HAS_FONT_FOUNDRY
	/**
	 * Search for a font on any given format ("helvetica", "adobe-helvetica",
	 * "adobe-helvetica-iso8859-1" etc.). This method should support any format
	 * that is likely to be used (which is implementation specific). It should
	 * return (through full_name) the full, real font name, as it is (or would
	 * have been) stored in OpFontDatabase. The method may choose not to verify
	 * that the font name exists, as long as it's on the valid format, e.g. if
	 * 'name' is "foo-bar-oste-pop", and the implementation of
	 * FindBestFontName() says that this is the right format (even though the
	 * font doesn't necessarily exist), it may report success. This method is
	 * here to help core find the right font name based on a font specified
	 * on a webpage. On X11, a part of the font name is the foundry name.
	 * Foundry names are never specified on a webpage. A webpage may specify
	 * "Arial" as the font name, while the actual font name on the system is
	 * "Arial [Xft]".
	 * @param name the font name to search for (like "helvetica",
	 * "adobe-helvetica", "adobe-helvetica-iso8859-1", "Helvetica [Adobe]",
	 * etc.)
	 * @full_name the fully qualified name. This name should be on the format
	 * as stored in OpFontDatabase.
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on allocation
	 * failure, OpStatus::ERR on invalid font format.
	 */
	virtual OP_STATUS FindBestFontName(const uni_char *name, OpString &full_name);

	/**
	 * Extract the foundry name from a font string (like "adobe-helvetica-22",
	 * or whatever the font format looks like)
	 * @param font the font string
	 * @param foundry where to put the result. This will be a string
	 * containing only the  foundry name (like "adobe").
	 */
	virtual OP_STATUS GetFoundry(const uni_char *font, OpString &foundry);

	/**
	 * Set the preferred foundry. When a font name is specified without the
	 * foundry (i.e. only the family name is specified) as, for example, the
	 * argument to FindBestFontName(), and this family is available from
	 * several foundries, choose the preferred foundry if possible.
	 * @param foundry The new preferred foundry.
	 */
	virtual OP_STATUS SetPreferredFoundry(const uni_char *foundry);
#endif // HAS_FONT_FOUNDRY

#ifdef PLATFORM_FONTSWITCHING
	/**
	 * Set the preferred font for a specific unicode block
	 * @param ch a character contained by the actual unicode block. This can be the
	 * first character in the block (like number 0x0370 for Greek), for example.
	 * @param monospace is the specified font supposed to be a replacement for
	 * monospace fonts?
	 */
	virtual void SetPreferredFont(uni_char ch, bool monospace, const uni_char *font,
								  OpFontInfo::CodePage codepage=OpFontInfo::OP_DEFAULT_CODEPAGE);
#endif // PLATFORM_FONTSWITCHING

	// additional functions not in OpFontManager interface:
	virtual OP_STATUS SetDefaultFonts(const MDE_GenericFonts* fonts);

	/**
	 * Called from EndEnumeration to get the names of the default fonts
	 * MDE_MI_OpFontManager has a default implementation.
	 */
    virtual OP_STATUS FindDefaultFonts(MDE_GenericFonts* fonts);

#ifdef _GLYPHTESTING_SUPPORT_
	virtual void UpdateGlyphMask(OpFontInfo *fontinfo);
#endif // _GLYPHTESTING_SUPPORT_

private:
    OpFont* CreateFont(UINT32 fontnr, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline);

#ifdef MDEFONT_MODULE
	OP_STATUS GetFontInfoInternal(const struct MDF_FONTINFO& info, OpFontInfo* fontinfo);
#endif // MDEFONT_MODULE

	class DefaultFonts
	{
	public:
		DefaultFonts() :
			serif_font(NULL),
			sans_serif_font(NULL),
			cursive_font(NULL),
			fantasy_font(NULL),
			monospace_font(NULL),
			default_font(NULL) {}

		~DefaultFonts()
		{
			Clear();
		}

		void Clear()
		{
			op_free(serif_font);
			serif_font = NULL;

			op_free(sans_serif_font);
			sans_serif_font = NULL;

			op_free(cursive_font);
			cursive_font = NULL;

			op_free(fantasy_font);
			fantasy_font = NULL;

			op_free(monospace_font);
			monospace_font = NULL;

			op_free(default_font);
			default_font = NULL;
		}

		/* set the css fonts that are to be used */
		uni_char* serif_font;
		uni_char* sans_serif_font;
		uni_char* cursive_font;
		uni_char* fantasy_font;
		uni_char* monospace_font;
		/* also set a default font */
		uni_char* default_font;
	} m_default_fonts;

	UINT32 m_fontcount;
	uni_char** m_fontNames;
	BOOL m_isInEnumeration;
};

#endif // MDE_OPFONTMANAGER_H
