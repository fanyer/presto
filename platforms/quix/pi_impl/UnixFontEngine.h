/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UNIX_FONT_ENGINE_H
#define UNIX_FONT_ENGINE_H

#include "modules/mdefont/mdf_freetype.h"
#include "platforms/quix/toolkits/ToolkitUiSettings.h"

class XRDatabase;
typedef struct _FcPattern FcPattern;
typedef struct _FcCharSet FcCharSet;
typedef struct _FcConfig FcConfig;
typedef struct _FcLangSet FcLangSet;

class UnixFontEngine : public MDF_FTFontEngine
{
public:
	UnixFontEngine();
	virtual ~UnixFontEngine();

	/** Initializer that gets run before anything else happens
	  */
	OP_STATUS InitFromSystem();

	// Overriding MDF_FTFontEngine
	virtual OP_STATUS Init();
	virtual MDE_FONT* GetFont(int font_nr, int size, BOOL bold, BOOL italic);

private:
	template<typename T, void Destroy(T*)> class FcHolder;
	struct DefaultGenericFonts;

	OP_STATUS ReadSystemFonts();
	OP_STATUS SetDefaultFonts();
	static void PrintFontChoices(const OpString fonts[], const uni_char* type);
	OP_STATUS DetermineAliases();
	OP_STATUS SetInternationalFonts();
	OP_STATUS GetDefaultFontFamily(OpString& font_family, const char* lang, const char* to_match, ...);
	/** Add all fonts having an FC_FONTFORMAT matching 'format' to
	 * opera's list of fonts.
	 *
	 * If 'format' is 0, all fonts are added, regardless of
	 * FC_FONTFORMAT.
	 */
	OP_STATUS AddFontsWithFormat(const char* format);
	OP_STATUS AddFontFromPattern(FcPattern* pattern);
	OP_STATUS AddFontFromPatternAsName(const char* family_name, FcPattern* pattern);
	/** Add all the names that this font should be known by to
	 * 'names'.  The name that is added first is the "canonical" name
	 * of this font.
	 *
	 * Note that the names are added with one layer of indirection, so
	 * to access the name itself, you need one layer of dereferencing.
	 * For example:
	 *    OpAutoVector<const char*> names;
	 *    RETURN_IF_ERROR(GetFamilyNames(pattern, &names));
	 *    const char * canonical_name = *names.Get(0);
	 *
	 * The "const char **" items added to 'names' will be deleted
	 * automatically when 'names' is deleted (since it is an
	 * OpAutoVector).  The names themselves are static pointers into
	 * the data of 'pattern' and so should not be freed by the caller.
	 */
	static OP_STATUS GetFamilyNames(FcPattern* pattern, OpAutoVector<const char*>* names);
	OP_STATUS CreateFontInformation(const char* family_name, MDF_FontInformation*& fi, const MDF_FontInformation* copy = 0);
	static OP_STATUS CreateUTF16FromUTF8(const char* utf8, uni_char*& utf16);
	static unsigned ParseStyle(FcPattern* pattern);
	static void UpdateFontInformation(FcPattern* pattern, unsigned style, MDF_FontInformation* fi);
	static void GetCharSetInfo(FcCharSet* charset, const UINT32 current_block[][3], MDF_FontInformation* fi);
	static void GetLangInfo(FcLangSet* langset, MDF_FontInformation* fi);
	static void UpdateFontInformationHeuristically(MDF_FontInformation* fi);
	OP_STATUS CreateFileInformation(FcPattern* pattern, unsigned style, MDF_FontFileNameInfo*& fileinfo_head);
	OP_STATUS GetNonScalableFileInfo(FcPattern* pattern, MDF_FontFileNameInfo* info);
	static OP_STATUS CreateUTF8FromUTF16(const uni_char* utf16, OpString8& utf8);
	OP_STATUS ReadSystemSettings();
	static void PrintDebugSettings(const ToolkitUiSettings::FontRenderSettings& settings, const char* description);
	void GetXftSettings();
	ToolkitUiSettings::FontRenderSettings GetRenderSettings(FcPattern* match);
	int GetLoadFlags(FcPattern* match);

	enum DefaultFonts
	{
		SERIF,
		SANS,
		CURSIVE,
		FANTASY,
		MONOSPACE,
		_DEFAULT_FONTS_COUNT
	};

	enum FontStyle
	{
		STYLE_BOLD		= 1 << 0,
		STYLE_ITALIC	= 1 << 1,
		STYLE_SANS		= 1 << 2
	};

	FcConfig* m_config;
	OpString m_default_fonts[_DEFAULT_FONTS_COUNT];
	ToolkitUiSettings::FontRenderSettings m_toolkit_settings;
	ToolkitUiSettings::FontRenderSettings m_xft_settings;
	OpINT32HashTable<int> m_aliases; ///< Mapping from alias's font number to real font number, both integers.
		///< @note OpHashTables do not support storing int values, only pointer values. This hash table stores
		///< the integer values in the pointers themselves and ignores the pointed-to integers.
};

#endif // UNIX_FONT_ENGINE_H
