/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _STYLE_MANAGER_
#define _STYLE_MANAGER_

#include "modules/util/simset.h"
#include "modules/display/style.h"
#include "modules/logdoc/html.h"
#include "modules/display/fontdb.h"
#include "modules/pi/OpFont.h"
#include "modules/display/webfont_manager.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/style/css_media.h"

//#define STYLE_EX_FORM_TEXT		0
#define STYLE_EX_FORM_TEXTAREA	1
#define STYLE_EX_FORM_SELECT	2
#define STYLE_EX_FORM_BUTTON	3
#define STYLE_EX_FORM_TEXTINPUT	4
#define STYLE_MAIL_BODY			5

#define STYLE_EX_SIZE	6
#define NUMBER_OF_STYLES Markup::HTE_FIRST_SPECIAL

#ifdef FONTSWITCHING
/**
 * Used by the font switching algorithm as the block number of the
 * block for characters that don't seem to belong to any existing
 * block.
 */
#define UNKNOWN_BLOCK_NUMBER		128

/**
 * Used by the font switching algorithm to select fonts that match
 * the original when switching.
 */
enum FontPitch  { UnknownPitch, FixedPitch, VariablePitch  };

#endif // FONTSWITCHING

class FontFaceElm;
class HLDocProfile;

/**
	StyleManager keeps tracks of Opera's styles and does fontswitching.

	Fonts:
	It owns an OpFontManager and an OpFontDatabase. The OpFontDatabase is initialized with the fonts from
	the OpFontManager.

	StyleManager has settings for the CSS generic fonts: serif, sans-serif, cursive, fantasy and monospace.
	The OpFontManager is queried for these.
*/

class StyleManager : public OpPrefsListener
{
public:
	struct UnicodeBlock {
		unsigned char	block_number;
		uni_char		lowest;
		uni_char		highest;
	};
	struct UnicodePointBlock {
		unsigned char	block_number;
		UnicodePoint	block_lowest;
		UnicodePoint	block_highest;
	};

	enum GenericFont { SERIF, SANS_SERIF, FANTASY, CURSIVE, MONOSPACE, UNKNOWN };
	/**
	   Returns the generic font type of base.
	 */
	static GenericFont GetGenericFontType(const OpFontInfo* base);
	/**
	   Returns the GenericFont mapping to name, which should be
	   one of "serif", "sans-serif", "fantasy", "cursive", "monospace".
	 */
	static GenericFont GetGenericFont(const uni_char* name);
	/**
	   Returns the StyleManager::GenericFont mapping to the
	   GenericFont declared in modules/pi/OpFont.h.
	 */
	static GenericFont GetGenericFont(::GenericFont font);

private:
	StyleManager();

	/** Second phase constructor. You must call this method prior to using the
		StyleManager object, unless it was created using the Create() method.

		@return OP_STATUS Status of the construction, always check this.
	*/
	OP_STATUS       Construct();

	Style*			style[NUMBER_OF_STYLES];
	Style*			style_ex[STYLE_EX_SIZE];

	void			DeleteFontFaceList();
	short			FindFallback(GenericFont id);

#ifdef FONTSWITCHING
#ifndef PLATFORM_FONTSWITCHING
	/**
	   used from font switching code: fetches number of fonts to
	   iterate over
	 */
	UINT32          GetTotalFontCount();
	/**
	   Used from font switching code: returns the next font
	   alphabetically, or NULL if font should not be a candidate for
	   font switching.
	 */
	OpFontInfo*     NextFontAlphabetical(UINT32 i);
#endif  // !PLATFORM_FONTSWITCHING

	UINT8*				block_table;
	UnicodePointBlock*	block_table_internal;
	UINT16				block_table_internal_count;
	BOOL				block_table_present;
	void				BuildOptimizedBlockTable();
	void				ReadUnicodeBlockLowestHighest(int table_index, UnicodePointBlock &block);
#endif // FONTSWITCHING

	OpFontManager* fontManager;
	OpFontDatabase* fontDatabase;

	short generic_font_numbers[WritingSystem::NumScripts][UNKNOWN];
	UINT32			fonts_in_hash;
	OpGenericStringHashTable fontface_hash;
	short* m_alphabetical_font_list;
	int		m_ahem_font_number;

	Head			preferredFonts;

	WritingSystem::Script locale_script;

public:

	~StyleManager();

	/**
	   Static method for OOM safe creation of the StyleManager.
	   @return StyleManager* The created object if successful and NULL otherwise.
	*/
	static StyleManager* Create();

	void			BuildAlphabeticalList();

	void			InitFontFaceList();
	OpFontInfo*		GetFontInfo(int font_number);
	OpFontManager*  GetFontManager() { return fontManager; }

#ifdef _GLYPHTESTING_SUPPORT_
	/**
	   some scripts available as language codes do not exist in the
	   OS/2 table. for these, we need to check the glyph support in
	   the fonts.
	   @param fontinfo The fontinfo to scan for glyphs to determine the writing system.
	 */
	void			SetupFontScriptsFromGlyphs(OpFontInfo* fontinfo);
#endif // _GLYPHTESTING_SUPPORT_

	Style*			GetStyle(HTML_ElementType helm) const;
	OP_STATUS		SetStyle(HTML_ElementType helm, Style *s);

	Style*			GetStyleEx(int id) const;
	void			SetStyleEx(int id, Style *s);

	int				GetFontSize(const FontAtt* font, BYTE fsize) const;
	int				GetNextFontSize(const FontAtt* font, int current_size, BOOL smaller) const;

	short			GetFontNumber(const uni_char* fontface);
	const uni_char*	GetFontFace(int font_number);
	BOOL			HasFont(int font_number);

	int				GetNextFontWeight(int weight, BOOL bolder);

	/**
		Sets up a mapping between a generic font name and a specific, e.g.
		monospace -> Courier.
		@param generic_name the generic name to define for, one of monospace,
		fantasy, serif, sans-serif, cursive
		@param font_name the actual font name
	*/

#ifdef PERSCRIPT_GENERIC_FONT
	void			InitGenericFont(GenericFont id);
	void			SetGenericFont(GenericFont id, const uni_char* font_name, WritingSystem::Script script = WritingSystem::NumScripts);
#else // PERSCRIPT_GENERIC_FONT
	void			SetGenericFont(GenericFont id, const uni_char* font_name);
#endif // PERSCRIPT_GENERIC_FONT
	void			SetGenericFont(GenericFont id, short font_number, WritingSystem::Script script);
	short			GetGenericFontNumber(const uni_char* fontface, WritingSystem::Script script) const;
	short			GetGenericFontNumber(GenericFont id, WritingSystem::Script script) const;

#ifdef FONTSWITCHING
	/**
	   finds the font stylewise closest to specified_font that claims support for script
	 */
	OpFontInfo*     GetFontSupportingScript(OpFontInfo* specified_font, WritingSystem::Script script, BOOL force = FALSE);
	OpFontInfo*		GetFontForScript(OpFontInfo* default_font, WritingSystem::Script script, BOOL force_fontswitch = FALSE);
#endif // FONTSWITCHING


	OP_STATUS		CreateFontNumber(const uni_char* family, int& fontnumber);
	void			ReleaseFontNumber(int fontnumber);

	/**
		Returns a fontnumber to use for the given font-family and media-type.
		This takes into account webfonts, and should be used instead of GetOrCreateFontNumber
		when entering values into the cascade.
		@param hld_profile The HLDocProfile corresponding to the cascade
		@param font_family The font-family name to resolve
		@param media_type The media-type corresponding to the cascade (see CSS_MediaType)
		@param set_timestamp Controls if webfonts should be timestamped. Text using a timestamped webfont will not be displayed until the webfont is loaded successfully or the timestamp expires.
		                     An expired timestamp causes the text to be displayed with a fallback font until the webfont finishes loading, at which point the text will be redrawn using the webfont.
							 A webfont will only get timestamped once to ensure that the timestamp doesn't drift as new text using the font is encountered.
							 Text using a webfont that has not been timestamped will not be suppressed. It will be shown directly with the fallback font if the webfont has not been loaded.

		@return The fontnumber to use, -1 if no matching font was found (which should mean continue to use whatever the current font is), or -2 to suppress display of text while waiting for a webfont to load or expire.
	*/
	short			LookupFontNumber(HLDocProfile* hld_profile, const uni_char* font_family, CSS_MediaType media_type, BOOL set_timestamp = FALSE);

	OP_STATUS 		AddWebFontInfo(OpFontInfo* fi);
	OP_STATUS 		RemoveWebFontInfo(OpFontInfo* fi);
	OP_STATUS 		DeleteWebFontInfo(OpFontInfo* fi, BOOL release_fontnumber = TRUE);

	/** If this font_number corresponds to one of the fonts installed on the platform */
	BOOL IsSystemFont(unsigned int font_number) { return font_number < fontDatabase->GetNumSystemFonts(); }

#ifdef FONTSWITCHING

	short			GetBestFont(const uni_char* str, int len, short font_number, const WritingSystem::Script script);

	OP_STATUS		SetPreferredFontForScript(UINT8 block_nr, BOOL monospace, const uni_char* font_name, BOOL replace = TRUE);
	OP_STATUS       SetPreferredFont(UINT8 block_nr, BOOL monospace, const uni_char* font_name, const WritingSystem::Script script, BOOL replace = TRUE);

	/**
		Get the preferred font.
		@param block_nr the block number
		@monospace monospace or not
		@return the font that was set to be preferred
	*/
	OpFontInfo*		GetPreferredFont(UINT8 block_nr, BOOL monospace, const WritingSystem::Script script);

	/**
		Check if codepage is relevant (and should play a role in fontswitching) for the given block.
		@return whether we have to look at the codepage to get the best font possible
	*/
	BOOL DEPRECATED(IsCodepageRelevant(UINT8 block_nr));

#ifndef PLATFORM_FONTSWITCHING
	/** Use the call below. */
	inline OpFontInfo DEPRECATED(*GetRecommendedAlternativeFont(const OpFontInfo* specified_font, int block,
												OpFontInfo::CodePage preferred_codepage = OpFontInfo::OP_DEFAULT_CODEPAGE,
												BOOL use_preferred = TRUE));

	/** This function can be called if specified_font didn't have the block or character you need.
		It will look through the available fonts and return the font that is as similar as possible to specified_font.
		If a font has the same properties (monospace/serifs/etc.) as the specified_font it will score a higher point and the probability is higher
		that it is the choosen font. If a font is preferred for the given codepage, it will also get a higher point.
		If FEATURE_GLYPHTESTING is supported and uc is not 0, the font will also be tested if it really contains the character and then get a much higher point.
		Can return NULL if no good font where found.
	*/
	OpFontInfo DEPRECATED(*GetRecommendedAlternativeFont(const OpFontInfo* specified_font, int block,
													  OpFontInfo::CodePage preferred_codepage = OpFontInfo::OP_DEFAULT_CODEPAGE,
													  const uni_char uc = (uni_char)0,
															 BOOL use_preferred = TRUE));
	OpFontInfo*		GetRecommendedAlternativeFont(const OpFontInfo* specified_font, int block,
												  WritingSystem::Script script = WritingSystem::Unknown,
												  const UnicodePoint uc = (UnicodePoint)0, BOOL use_preferred = TRUE);
#endif // !PLATFORM_FONTSWITCHING

	/**
		This method can be used to find out which Unicode block a  character belongs to, and also what its boundaries are.
	*/
	void			GetUnicodeBlockInfo(UnicodePoint ch, int &block_no, UnicodePoint &block_lowest, UnicodePoint &block_highest);
	void DEPRECATED(GetUnicodeBlockInfo(UnicodePoint ch, int &block_no, int &block_lowest, int &block_highest));

#ifdef _GLYPHTESTING_SUPPORT_
	/** Return FALSE for characters that should never ever be displayed. F.ex newline, tab, NULL... */
	BOOL            ShouldHaveGlyph(UnicodePoint uc);
#endif // _GLYPHTESTING_SUPPORT_

	/**
	 * Determine if this is a whitespace character that we should try to fontswitch, or if we should just
	 * determine the width of the space ourselves.
	 *
	 * @param ch The unicode point
	 * @return TRUE if there is no reason to try to fontswitch this whitespace
	 */

	static inline BOOL NoFontswitchNoncollapsingWhitespace(UnicodePoint ch) { return ch >= 0x2000 && ch <= 0x200a || ch == 0x202f; }

#else // FONTSWITCHING
	short			GetBestFont(const uni_char* str, int length, short font_number, const WritingSystem::Script)
	{ return font_number; }

#endif // FONTSWITCHING

	void OnPrefsInitL();

	// OpPrefsListener interface:
	/** Signals a change in an integer preference. */
	void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);

	/** Signals a change in a string preference. */
	void PrefChanged(enum OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue);

#ifdef PREFS_HOSTOVERRIDE
	/** Signals the addition, change or removal of a host override. */
	void HostOverrideChanged(enum OpPrefsCollection::Collections id, const uni_char *hostname);
#endif

	/** Get the script the block belongs, but doesn't cover Han blocks
	*/
	static WritingSystem::Script ScriptForBlock(UINT8 block_nr);
};

/** Representation of a font that is preferred for a certain unicode block (see http://www.unicode.org/charts/). */
class PreferredFont : public Link
{
public:
	PreferredFont(UINT8 block_nr, const WritingSystem::Script script) :
	  block_nr(block_nr),
      font_info(NULL),
      monospace_font_info(NULL),
      script(script)
	{
	}

	UINT8 block_nr;
	OpFontInfo* font_info;
	OpFontInfo* monospace_font_info;
	WritingSystem::Script script;
};

#ifdef FONTSWITCHING
inline void StyleManager::GetUnicodeBlockInfo(UnicodePoint ch, int &block_no, int &block_lowest, int &block_highest)
{
	UnicodePoint bl, bh;
	GetUnicodeBlockInfo(ch, block_no, bl, bh);
	block_lowest = bl;
	block_highest = bh;
};
#endif // FONTSWITCHING


#ifndef _NO_GLOBALS_
extern StyleManager* styleManager;
#endif

#endif /* _STYLE_MANAGER_ */
