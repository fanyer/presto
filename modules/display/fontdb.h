/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FONTDB_H
#define FONTDB_H

#include "modules/util/str.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/WritingSystem.h"

class OpFontManager;

/**
   Holds information about a font in the system. Given this
   information, you can get a clue to what font you should
   instantiate. An instantiated font is represented by an OpFont
   object.

   //rg move part of the implementation of this to "platform layer".
*/

class OpFontInfo
{
private:
	OpFontInfo(const OpFontInfo& other); // Not implemented, to prevent accidential copying
	OpFontInfo& operator=(const OpFontInfo& other); // Not implemented, to prevent accidential copying
public:

	enum FontType
	{
		PLATFORM = 0,
		PLATFORM_WEBFONT,
		PLATFORM_LOCAL_WEBFONT,
		SVG_WEBFONT
	};

#define NUM_CODEPAGES_DEFINED 6


	/** Constructs a new OpFontInfo object. */
	OpFontInfo()
		: face(NULL)
		, has_weight(0)
		, packed_init(0)
		, font_number(0)
		, m_webfont(0)
	{
		parsed_font_size = -1;

		blocks_available[0] = 0;
		blocks_available[1] = 0;
		blocks_available[2] = 0;
		blocks_available[3] = 0;

		int i;
		for (i = 0; i < (int)WritingSystem::NumScripts; i++)
		{
			m_scripts[i] = FALSE;
            m_script_preference[i] = 0;
		}
#ifdef _GLYPHTESTING_SUPPORT_
        m_glyphtab = NULL;
#endif
	};

	/** Frees all memory used by this object. */
	~OpFontInfo()
	{
		OP_DELETEA(face);
#ifdef _GLYPHTESTING_SUPPORT_
		if(m_glyphtab) OP_DELETEA(m_glyphtab);
#endif
	}

	enum CodePage { OP_DEFAULT_CODEPAGE = 0, OP_SIMPLIFIED_CHINESE_CODEPAGE, OP_TRADITIONAL_CHINESE_CODEPAGE, OP_JAPANESE_CODEPAGE, OP_KOREAN_CODEPAGE, OP_CYRILLIC_CODEPAGE };

    /** @return the name of the font */
	inline const uni_char* GetFace() const { return face; };

	/** @return a unique number wich can be used to create the font */
	inline UINT32 GetFontNumber() const { return font_number; };

	/**
		@param weight the weight (0-9), where 4 is Normal, and 7 Bold.
		@return wheter the font supports the specified weight or not (0-9)
	*/
	BOOL HasWeight(UINT8 weight);

#ifdef _GLYPHTESTING_SUPPORT_
	/**
		@return whether the font supports testing specified character glyph mappings
	*/
    inline BOOL HasGlyphTable(int block) { UpdateGlyphTableIfNeeded(); return (m_glyphtab != NULL) && (block != 57);};
	/** Check if the font has glyph support for a unicode character.
        NB! Currently we will only check for single 16-bits unicode characters.
        At a later stage we may have to check for longer characters, for sequences and ligaturs.
		@param uc unicode character
		@return whether the font supports the specified characters
	*/
    BOOL HasGlyph(const UnicodePoint uc);
	/**
		@param uc unicode character supported by the font
		@return OK if the setting worked
	*/
    OP_STATUS SetGlyph(const UnicodePoint uc);
	/**
		@param uc unicode character not supported by the font
		@return OK if the clearing worked
	*/
    OP_STATUS ClearGlyph(const UnicodePoint uc);
#endif // _GLYPHTESTING_SUPPORT_

	/**	Merge the glyphtable and blocks_available from another font
		info into the current one. Used for SVG fonts when an
		font-face reference another external font-face for additional
		glyphs, and that external font-face is loaded later.

		@param other The font info that should be merged
		@return OK if the merging worked
	*/
	OP_STATUS MergeFontInfo(const OpFontInfo& other);

	/**
	   @param blocknr the unicode block we wonder about
	   @return whether the font supports the specified block
	*/
	inline BOOL HasBlock(INT blocknr)
	{
#ifdef _GLYPHTESTING_SUPPORT_
		UpdateGlyphTableIfNeeded();
#endif // _GLYPHTESTING_SUPPORT_
		return 0 != ((blocks_available[(blocknr & 0x60) >> 5] >> (blocknr & 0x1F)) & 0x1);
	};

	/** @return whether the font is monospace */
	inline BOOL Monospace() const { return packed.monospace; };

	/** @return whether the font contains italic glyphs */
	inline BOOL HasItalic() const { return packed.italic; };

	/** @return whether the font contains/can create oblique (slanted) glyphs */
	inline BOOL HasOblique() const { return packed.oblique; };

	/** @return whether the font contains normal glyphs */
	inline BOOL HasNormal() const { return packed.normal; };

	/** @return whether the font contains small caps glyphs */
	inline BOOL Smallcaps() const { return packed.smallcaps; };

	enum FontSerifs { UnknownSerifs=0, WithSerifs, WithoutSerifs };

	/** @return wich type of serifs the font has */
	inline FontSerifs GetSerifs() const { return (FontSerifs)packed.serifs; };

	/**
	   @return if the font is vertical or not (most common in the western world)
	*/
	inline BOOL Vertical() const { return packed.vertical; };

	/**
	   @return if the font supports the specified codepage or not
	*/
	inline BOOL HasScript(const WritingSystem::Script script) const { return m_scripts[script]; };


	///////// setters come here. nothing to see here, folks, unless you're hacking the fontmanager. /////////

	/** Sets the name of the font. This method is for OpFontManager implementations ONLY! */
    OP_STATUS SetFace(const OpStringC& aFace) { return UniSetStr(face,aFace.CStr()); }
    void SetFaceL(const OpStringC& aFace) { LEAVE_IF_ERROR(UniSetStr(face,aFace.CStr())); }

	/** Sets the (unique!) number of the font. This method is for OpFontManager implementations ONLY! */
	inline void SetFontNumber(UINT32 font_number) { this->font_number=font_number; };

	/**
		Sets that the font supports a specific weight or not.
		This method is for OpFontManager implementations ONLY!
		@param weight the weight (0-9), where 4 is Normal, and 7 Bold.
		@param val whether we supports the weight or not
	*/
	inline void SetWeight(UINT8 weight, UINT32 val)
	{
		OP_ASSERT(weight <= 9);
		has_weight += ((val << weight) - (has_weight & (0x1 << weight)));
		has_weight += ((1 << (weight + 10)) - (has_weight & (0x1 << (weight + 10))));
	};

	/** Sets that the font supports the specified page or not.
		This method is for OpFontManager implementations ONLY!
		@param pagennr the page
		@param val whether we supports the page or not
	*/
	inline void SetBlock(INT blocknr, UINT32 val)
	{
		// The block info has been optimized and is using 4 UINT32's => 128 bits.
		// This means 1/32 as much memory is used for the block info.
		// With 50 fonts the gain is about 24 kb.
		int array_index = (blocknr & 0x60) >> 5;
		int bit = (blocknr & 0x1F);
		int newvalue = val << bit;
		int oldvalue = blocks_available[array_index] & (0x1 << bit);
		blocks_available[array_index] += (newvalue - oldvalue);
	};

	inline void SetDirectBlock( UINT32 lower, UINT32 upper )
	{
		blocks_available[0] = lower;
		blocks_available[1] = upper;
	}

	/** Setter for the monospace attribute of the font.
		This method is for OpFontManager implementations ONLY!
		@param monospace monospace or not
	*/
	inline void SetMonospace(BOOL monospace) { this->packed.monospace=monospace; };

	/** Sets whether the font supports italic.
		This method is for OpFontManager implementations ONLY!
		@param italic supports italic or not
	*/
	inline void SetItalic(BOOL italic) { this->packed.italic=italic; };

	/** Sets whether the font supports oblique or not.
		This method is for OpFontManager implementations ONLY!
		@param oblique oblique or not
	*/
	inline void SetOblique(BOOL oblique) { this->packed.oblique=oblique; };

	/** Sets whether the font supports normal glyphs or not.
		This method is for OpFontManager implementations ONLY!
		@param normal normal or not
	*/
	inline void SetNormal(BOOL normal) { this->packed.normal=normal; };

	/** Sets whether the font supports small caps or not.
		This method is for OpFontManager implementations ONLY!
		@param smallcaps has smallcaps or not
	*/
	inline void SetSmallcaps(BOOL smallcaps) { this->packed.smallcaps=smallcaps; };

	/** Sets wich type of serifs the font has.
		This method is for OpFontManager implementations ONLY!
	*/
	inline void SetSerifs(FontSerifs serifs) { this->packed.serifs=serifs; };

	/** Sets if the font is vertical or not.
		This method is for OpFontManager implementations ONLY!
	*/
	inline void SetVertical(BOOL vertical) { this->packed.vertical = vertical; };

	/** Sets that the font supports the given script.
		This method is for OpFontManager implementations ONLY!
	*/
	inline void SetScript(const WritingSystem::Script script, BOOL state = TRUE) { m_scripts[script] = state; }

	/** Sets that the font should be preferred for the script.
		@param script the prefered script
		@param point the preference of the font for this script
	*/
    inline void SetScriptPreference(const WritingSystem::Script script, char point) { m_script_preference[script] = point; }
	/**
		@param script  the prefered script
		@return the preference of the font for this script
	*/
    inline char GetScriptPreference(const WritingSystem::Script script) const { return m_script_preference[script]; }

	/** Convert a script system to which preferred script this corresponds to
		@param script the script
	*/
	static CodePage CodePageFromScript(WritingSystem::Script script);

	// temporarily introduced while not all modules are multistyle-aware - don't use!
	static WritingSystem::Script DEPRECATED(ScriptFromCodePage(const OpFontInfo::CodePage codepage));

	/**
	   sets all scripts represented in codepages as supported (doesn't set not represented as unsupported).
	   if match_score is non-zero, sets score for all represented scripts.
	 */
	void SetScriptsFromOS2CodePageRanges(const UINT32* codepages, const UINT8 match_score = 0);
	static WritingSystem::Script ScriptFromOS2CodePageBit(const UINT8 bit);

	inline void SetWebFont(UINTPTR webfont) { m_webfont = webfont; }
	UINTPTR GetWebFont() const { return m_webfont; }

	void SetFontType(OpFontInfo::FontType type) { packed.font_type = type; }
	OpFontInfo::FontType Type() { return (OpFontInfo::FontType)packed.font_type; }

	void SetParsedFontSize(int size) { parsed_font_size = size; }
	int ParsedFontSize() { return parsed_font_size; }

	void SetHasLigatures(BOOL has_ligatures) { packed.has_ligatures = has_ligatures? 1 : 0; }
	BOOL HasLigatures() { return packed.has_ligatures; }

#ifdef _GLYPHTESTING_SUPPORT_
	/** Semi-private method for updating the glyph table, used by the font switching code (and the WebFontManager) */
	void UpdateGlyphTableIfNeeded();
	void InvalidateGlyphTable();
#endif // _GLYPHTESTING_SUPPORT_

private:
	uni_char* face;			   ///< holds the font face (the name of the font)
	UINT32 blocks_available[4];///< What unicode blocks that the font contains (128 bits)
	/** Which weights the font supports. It is using 20 bits.
		Bit 0-9 to tell if a weight is available.
		But 10-19 to tell if it is set or if it has to ask OpFontManager. */
	UINT32 has_weight;

	union
	{
		struct
		{
			unsigned int monospace:1;				///< whether the font is monospace or not
			unsigned int italic:1;					///< whether the font contains italic glyphs
			unsigned int oblique:1;					///< whether the font supports oblique glyphs
			unsigned int normal:1;					///< whether the font contains normal glyphs
			unsigned int smallcaps:1;				///< whether the font contains small caps
			unsigned int serifs:2;					///< whether the font has serifs or not (or unknown)
			unsigned int vertical:1;				///< if the font is vertical or not.
			unsigned int has_scanned_for_glyphs:1;
			unsigned int font_type:2;				///< the type of font
			unsigned int has_ligatures: 1;			///< whether the font contains ligatures (needed for acid3 and svgfonts at the moment, to workaround fontswitching issues)
		} packed; /** 12 bits used **/
		unsigned int packed_init;
	};

	int parsed_font_size;		///< used for svg fonts only, contains the parsed font-size
	UINT32 font_number;			///< a unique number wich can be used to create the font.
	BOOL m_scripts[WritingSystem::NumScripts];           ///< used to store what scripts the font supports
    char m_script_preference[WritingSystem::NumScripts]; ///< used to store the preference of a font for a certain script
#ifdef _GLYPHTESTING_SUPPORT_
	OP_STATUS CreateGlyphTable();
	void ClearGlyphTable();
	// binary table indexed by UnicodePoint (BMP, SMP and SIP only) represented as UINT32 array indexed by 13 bits
    UINT32 * m_glyphtab;
#endif // _GLYPHTESTING_SUPPORT_
	UINTPTR m_webfont;             ///< the font handle issued by the fontmanager. Note: it is volatile (there can be more than one webfont face per OpFontInfo), only to be used for UpdateGlyphMask
};


/**
   OpFontDatabase represents a database over all of the fonts that are
   available in the system. After it is instantiated, you can ask for
   information on all the fonts. Such information is held by
   OpFontInfo. This is needed to be able to pass the correct name to
   OpFontManager::CreateFont(...).
*/
class OpFontDatabase
{
public:
	OpFontDatabase();
	~OpFontDatabase();

	/** Construct the object based on the fontmanager parameter.
		Code moved from the constructor since it allocates memory.
	*/
	OP_STATUS Construct(OpFontManager* fontmanager);

	/** Get info about a certain font
		@param fontnr the font that you want info about
		@return info about the requested font
	*/
	OpFontInfo* GetFontInfo(UINT32 fontnr);

	UINT32 GetNumFonts();
	UINT32 GetNumSystemFonts();

	UINT32 AllocateFontNumber();
	OP_STATUS AddFontInfo(OpFontInfo* fi);
	OP_STATUS RemoveFontInfo(OpFontInfo* fi);
	OP_STATUS DeleteFontInfo(OpFontInfo* fi, BOOL release_fontnumber = TRUE);
	OpHashIterator* GetIterator(){return m_font_list.GetIterator();}
	void ReleaseFontNumber(UINT32 fontnumber);

private:
	OpAutoINT32HashTable<OpFontInfo> m_font_list;
	OpINT32Vector m_webfont_list;
	UINT32 numSystemFonts; // The number of system fonts in the system, not counting svg and web fonts

#ifdef DISPLAY_FONT_NAME_OVERRIDE
	BOOL LoadFontOverrideFile(PrefsFile* prefs_obj);
#endif
};


#endif // FONTDB_H
