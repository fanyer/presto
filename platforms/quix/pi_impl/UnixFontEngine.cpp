/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/pi_impl/UnixFontEngine.h"

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/libvega/src/oppainter/vegamdefont.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/prefsfile/prefsfile.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/quix/commandline/StartupSettings.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/utils/XRDatabase.h"
#include "platforms/unix/base/x11/x11_globals.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include <dlfcn.h>

#define DEBUG_LOG(msg, ...) { if (g_startup_settings.debug_font) printf("opera: " msg "\n", __VA_ARGS__); }
#define RETURN_IF_ERROR_MSG(x, msg) { OP_STATUS err = x; if (OpStatus::IsError(err)) { printf("opera: " msg "\n"); return err; } }

namespace
{
	const char* AliasFonts[] =
	{
		/* This is a list of fonts that we let fontconfig try to alias
		 * to other fonts. They are known as the Microsoft 'Webfonts',
		 * and the reason to do this is to get metric-compatible replacements
		 * for those fonts if possible (since many web sites assume the
		 * presence of these fonts and their metrics).
		 */
		"Arial",
		"Times New Roman",
		"Courier New"
	};

	const UINT32 UniBlocks[][3] =
	{
		/* This table maps unicode ranges to blocks in the OS/2 truetype
		 * table. Do not change. Data was copied from
		 *    http://www.microsoft.com/typography/otspec/os2.htm#ur
		 * Data is in form { range_start, range_end (inclusive), block_nr }
		 * and is ordered by range_start.
		 */
		{ 0x000000, 0x00007F, 0 },
		{ 0x000080, 0x0000FF, 1 },
		{ 0x000100, 0x00017F, 2 },
		{ 0x000180, 0x00024F, 3 },
		{ 0x000250, 0x0002AF, 4 },
		{ 0x0002B0, 0x0002FF, 5 },
		{ 0x000300, 0x00036F, 6 },
		{ 0x000370, 0x0003FF, 7 },
		{ 0x000400, 0x0004FF, 9 },
		{ 0x000500, 0x00052F, 9 },
		{ 0x000530, 0x00058F, 10 },
		{ 0x000590, 0x0005FF, 11 },
		{ 0x000600, 0x0006FF, 13 },
		{ 0x000700, 0x00074F, 71 },
		{ 0x000750, 0x00077F, 13 },
		{ 0x000780, 0x0007BF, 72 },
		{ 0x0007C0, 0x0007FF, 14 },
		{ 0x000900, 0x00097F, 15 },
		{ 0x000980, 0x0009FF, 16 },
		{ 0x000A00, 0x000A7F, 17 },
		{ 0x000A80, 0x000AFF, 18 },
		{ 0x000B00, 0x000B7F, 19 },
		{ 0x000B80, 0x000BFF, 20 },
		{ 0x000C00, 0x000C7F, 21 },
		{ 0x000C80, 0x000CFF, 22 },
		{ 0x000D00, 0x000D7F, 23 },
		{ 0x000D80, 0x000DFF, 73 },
		{ 0x000E00, 0x000E7F, 24 },
		{ 0x000E80, 0x000EFF, 25 },
		{ 0x000F00, 0x000FFF, 70 },
		{ 0x001000, 0x00109F, 74 },
		{ 0x0010A0, 0x0010FF, 26 },
		{ 0x001100, 0x0011FF, 28 },
		{ 0x001200, 0x00137F, 75 },
		{ 0x001380, 0x00139F, 75 },
		{ 0x0013A0, 0x0013FF, 76 },
		{ 0x001400, 0x00167F, 77 },
		{ 0x001680, 0x00169F, 78 },
		{ 0x0016A0, 0x0016FF, 79 },
		{ 0x001700, 0x00171F, 84 },
		{ 0x001720, 0x00173F, 84 },
		{ 0x001740, 0x00175F, 84 },
		{ 0x001760, 0x00177F, 84 },
		{ 0x001780, 0x0017FF, 80 },
		{ 0x001800, 0x0018AF, 81 },
		{ 0x001900, 0x00194F, 93 },
		{ 0x001950, 0x00197F, 94 },
		{ 0x001980, 0x0019DF, 95 },
		{ 0x0019E0, 0x0019FF, 80 },
		{ 0x001A00, 0x001A1F, 96 },
		{ 0x001B00, 0x001B7F, 27 },
		{ 0x001B80, 0x001BBF, 112 },
		{ 0x001C00, 0x001C4F, 113 },
		{ 0x001C50, 0x001C7F, 114 },
		{ 0x001D00, 0x001D7F, 4 },
		{ 0x001D80, 0x001DBF, 4 },
		{ 0x001DC0, 0x001DFF, 6 },
		{ 0x001E00, 0x001EFF, 29 },
		{ 0x001F00, 0x001FFF, 30 },
		{ 0x002000, 0x00206F, 31 },
		{ 0x002070, 0x00209F, 32 },
		{ 0x0020A0, 0x0020CF, 33 },
		{ 0x0020D0, 0x0020FF, 34 },
		{ 0x002100, 0x00214F, 35 },
		{ 0x002150, 0x00218F, 36 },
		{ 0x002190, 0x0021FF, 37 },
		{ 0x002200, 0x0022FF, 38 },
		{ 0x002300, 0x0023FF, 39 },
		{ 0x002400, 0x00243F, 40 },
		{ 0x002440, 0x00245F, 41 },
		{ 0x002460, 0x0024FF, 42 },
		{ 0x002500, 0x00257F, 43 },
		{ 0x002580, 0x00259F, 44 },
		{ 0x0025A0, 0x0025FF, 45 },
		{ 0x002600, 0x0026FF, 46 },
		{ 0x002700, 0x0027BF, 47 },
		{ 0x0027C0, 0x0027EF, 38 },
		{ 0x0027F0, 0x0027FF, 37 },
		{ 0x002800, 0x0028FF, 82 },
		{ 0x002900, 0x00297F, 37 },
		{ 0x002980, 0x0029FF, 38 },
		{ 0x002A00, 0x002AFF, 38 },
		{ 0x002B00, 0x002BFF, 37 },
		{ 0x002C00, 0x002C5F, 97 },
		{ 0x002C60, 0x002C7F, 29 },
		{ 0x002C80, 0x002CFF, 8 },
		{ 0x002D00, 0x002D2F, 26 },
		{ 0x002D30, 0x002D7F, 98 },
		{ 0x002D80, 0x002DDF, 75 },
		{ 0x002DE0, 0x002DFF, 9 },
		{ 0x002E00, 0x002E7F, 31 },
		{ 0x002E80, 0x002EFF, 59 },
		{ 0x002F00, 0x002FDF, 59 },
		{ 0x002FF0, 0x002FFF, 59 },
		{ 0x003000, 0x00303F, 48 },
		{ 0x003040, 0x00309F, 49 },
		{ 0x0030A0, 0x0030FF, 50 },
		{ 0x003100, 0x00312F, 51 },
		{ 0x003130, 0x00318F, 52 },
		{ 0x003190, 0x00319F, 59 },
		{ 0x0031A0, 0x0031BF, 51 },
		{ 0x0031C0, 0x0031EF, 61 },
		{ 0x0031F0, 0x0031FF, 50 },
		{ 0x003200, 0x0032FF, 54 },
		{ 0x003300, 0x0033FF, 55 },
		{ 0x003400, 0x004DBF, 59 },
		{ 0x004DC0, 0x004DFF, 99 },
		{ 0x004E00, 0x009FFF, 59 },
		{ 0x00A000, 0x00A48F, 83 },
		{ 0x00A490, 0x00A4CF, 83 },
		{ 0x00A500, 0x00A63F, 12 },
		{ 0x00A640, 0x00A69F, 9 },
		{ 0x00A700, 0x00A71F, 5 },
		{ 0x00A720, 0x00A7FF, 29 },
		{ 0x00A800, 0x00A82F, 100 },
		{ 0x00A840, 0x00A87F, 53 },
		{ 0x00A880, 0x00A8DF, 115 },
		{ 0x00A900, 0x00A92F, 116 },
		{ 0x00A930, 0x00A95F, 117 },
		{ 0x00AA00, 0x00AA5F, 118 },
		{ 0x00AC00, 0x00D7AF, 56 },
		{ 0x00D800, 0x00DFFF, 57 },
		{ 0x00E000, 0x00F8FF, 60 },
		{ 0x00F900, 0x00FAFF, 61 },
		{ 0x00FB00, 0x00FB4F, 62 },
		{ 0x00FB50, 0x00FDFF, 63 },
		{ 0x00FE00, 0x00FE0F, 91 },
		{ 0x00FE10, 0x00FE1F, 65 },
		{ 0x00FE20, 0x00FE2F, 64 },
		{ 0x00FE30, 0x00FE4F, 65 },
		{ 0x00FE50, 0x00FE6F, 66 },
		{ 0x00FE70, 0x00FEFF, 67 },
		{ 0x00FF00, 0x00FFEF, 68 },
		{ 0x00FFF0, 0x00FFFF, 69 },
		{ 0x010000, 0x01007F, 101 },
		{ 0x010080, 0x0100FF, 101 },
		{ 0x010100, 0x01013F, 101 },
		{ 0x010140, 0x01018F, 102 },
		{ 0x010190, 0x0101CF, 119 },
		{ 0x0101D0, 0x0101FF, 120 },
		{ 0x010280, 0x01029F, 121 },
		{ 0x0102A0, 0x0102DF, 121 },
		{ 0x010300, 0x01032F, 85 },
		{ 0x010330, 0x01034F, 86 },
		{ 0x010380, 0x01039F, 103 },
		{ 0x0103A0, 0x0103DF, 104 },
		{ 0x010400, 0x01044F, 87 },
		{ 0x010450, 0x01047F, 105 },
		{ 0x010480, 0x0104AF, 106 },
		{ 0x010800, 0x01083F, 107 },
		{ 0x010900, 0x01091F, 58 },
		{ 0x010920, 0x01093F, 121 },
		{ 0x010A00, 0x010A5F, 108 },
		{ 0x012000, 0x0123FF, 110 },
		{ 0x012400, 0x01247F, 110 },
		{ 0x01D000, 0x01D0FF, 88 },
		{ 0x01D100, 0x01D1FF, 89 },
		{ 0x01D200, 0x01D24F, 89 },
		{ 0x01D300, 0x01D35F, 109 },
		{ 0x01D360, 0x01D37F, 111 },
		{ 0x01D400, 0x01D7FF, 89 },
		{ 0x01F000, 0x01F02F, 122 },
		{ 0x01F030, 0x01F09F, 122 },
		{ 0x020000, 0x02A6DF, 59 },
		{ 0x02F800, 0x02FA1F, 61 },
		{ 0x0E0000, 0x0E007F, 92 },
		{ 0x0E0100, 0x0E01EF, 91 },
		{ 0x0FF000, 0x0FFFFD, 90 },
		{ 0x100000, 0x10FFFD, 90 },
		{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }
	};
};

template<typename T, void Destroy(T*)>
class UnixFontEngine::FcHolder
{
public:
	FcHolder(T* object) : m_object(object) {}
	~FcHolder() { if (m_object) Destroy(m_object); }
	T* get() const { return m_object; }
	T* operator->() const { return m_object; }
	T* release() { T* obj = m_object; m_object = 0; return obj; }
private:
	T* m_object;
};


OP_STATUS MDF_FontEngine::Create(MDF_FontEngine** engine)
{
	OpAutoPtr<UnixFontEngine> e (OP_NEW(UnixFontEngine, ()));
	if (!e.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(e->Construct());
	RETURN_IF_ERROR(e->InitFromSystem());

	*engine = e.release();
	return OpStatus::OK;
}


UnixFontEngine::UnixFontEngine()
  : m_config(0)
{
}


UnixFontEngine::~UnixFontEngine()
{
	if (m_config)
		FcConfigDestroy(m_config);
}


OP_STATUS UnixFontEngine::InitFromSystem()
{
	m_config = FcInitLoadConfigAndFonts();
	if (!m_config)
	{
		printf("opera: can't initialize FontConfig\n");
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR_MSG(ReadSystemFonts(), "error while reading system fonts");
	RETURN_IF_ERROR_MSG(SetDefaultFonts(), "error while determining default fonts");

	return DetermineAliases();
}


OP_STATUS UnixFontEngine::ReadSystemFonts()
{
	RETURN_IF_ERROR(AddFontsWithFormat("TrueType"));
	RETURN_IF_ERROR(AddFontsWithFormat("Type 1"));
	RETURN_IF_ERROR(AddFontsWithFormat("CFF"));

	if (CountFonts() == 0)
	{
		/* We are prohibiting fonts in pcf files from being used in
		 * opera unless no fonts are found among the "preferred" file
		 * formats.  I'm not sure this is the right thing to do.
		 * Maybe we need a pref for choosing which font formats to
		 * prefer.
		 */
		printf("opera: no fonts found among preferred font formats, trying pcf fonts\n"); // always show this message when this happens
		RETURN_IF_ERROR(AddFontsWithFormat("PCF"));
	}

	if (CountFonts() == 0)
	{
		/* This happens if you're running too old version of
		 * fontconfig (see DSK-321933).  We don't actually officially
		 * support such old versions (I think), but maybe it could
		 * happen in other cases as well.  (And if it is easy to deal
		 * with, we might as well do so.)
		 */
		printf("opera: no fonts found among known formats, trying to add all fonts\n");
		RETURN_IF_ERROR(AddFontsWithFormat(0));
	};

	if (CountFonts() == 0)
	{
		printf("opera: fatal error: No fonts found\n");
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}


OP_STATUS UnixFontEngine::AddFontsWithFormat(const char* format)
{
	FcHolder<FcPattern, FcPatternDestroy> pattern (FcPatternCreate());
	FcHolder<FcObjectSet, FcObjectSetDestroy> os (FcObjectSetCreate());
	if (!pattern.get() || !os.get())
		return OpStatus::ERR_NO_MEMORY;

	if (format && !FcPatternAddString(pattern.get(), FC_FONTFORMAT, (const FcChar8*)format))
		return OpStatus::ERR_NO_MEMORY;

	if (/* list of properties we need for each font */
		!FcObjectSetAdd(os.get(), FC_FAMILY) ||
		!FcObjectSetAdd(os.get(), FC_FILE) ||
		!FcObjectSetAdd(os.get(), FC_CHARSET) ||
		!FcObjectSetAdd(os.get(), FC_STYLE) ||
		!FcObjectSetAdd(os.get(), FC_SCALABLE) ||
		!FcObjectSetAdd(os.get(), FC_SPACING) ||
		!FcObjectSetAdd(os.get(), FC_WEIGHT) ||
		!FcObjectSetAdd(os.get(), FC_SLANT) ||
		!FcObjectSetAdd(os.get(), FC_INDEX) ||
		!FcObjectSetAdd(os.get(), FC_LANG))
		return OpStatus::ERR_NO_MEMORY;

	FcHolder<FcFontSet, FcFontSetDestroy> list (FcFontList(m_config, pattern.get(), os.get()));
	if (!list.get())
		return OpStatus::ERR_NO_MEMORY;

	for (int i = 0; i < list->nfont; i++)
	{
		OpStatus::Ignore(AddFontFromPattern(list->fonts[i]));
	}

	return OpStatus::OK;
}


OP_STATUS UnixFontEngine::AddFontFromPattern(FcPattern* pattern)
{
    OpAutoVector<const char *> family_names;
	RETURN_IF_ERROR(GetFamilyNames(pattern, &family_names));

	if (family_names.GetCount() <= 0)
		return OpStatus::ERR;

	for (size_t i = 0; i < family_names.GetCount(); i++)
	{
		const char ** name = family_names.Get(i);
		OP_STATUS err = AddFontFromPatternAsName(*name, pattern);
		if (OpStatus::IsError(err))
		{
			if (*name)
				fprintf(stderr, "opera: failed to add font: %s\n", *name);
		}
	}
	return OpStatus::OK;
}

OP_STATUS UnixFontEngine::AddFontFromPatternAsName(const char* family_name, FcPattern* pattern)
{
	// Check if we already have this font family or need to create a new one
	MDF_FontInformation* font_information = 0;
	int font_array_index = -1;
	RETURN_IF_MEMORY_ERROR(FindFontInformation(family_name, font_information, &font_array_index));

	if (!font_information && CreateFontInformation(family_name, font_information) < 0)
		return OpStatus::ERR_NO_MEMORY;

	unsigned style = ParseStyle(pattern);

	UpdateFontInformation(pattern, style, font_information);

	return CreateFileInformation(pattern, style, font_information->file_name_list);
}

OP_STATUS UnixFontEngine::GetFamilyNames(FcPattern* pattern, OpAutoVector<const char*>* names)
{
	if (!names)
		return OpStatus::ERR;
	FcChar8* family_name = 0;
	size_t name_count = 0;
	while (FcPatternGetString(pattern, FC_FAMILY, name_count, &family_name) == FcResultMatch)
		name_count += 1;
	const char ** family_names = OP_NEWA(const char *, name_count + 1);
	if (!family_names)
		return OpStatus::ERR_NO_MEMORY;
	for (size_t id = 0; id < name_count; id++)
	{
		FcPatternGetString(pattern, FC_FAMILY, id, &family_name);
		family_names[id] = (const char*)family_name;
	};

	/* Eliminate names that are not suitable to be used for this font.
	 *
	 * - Any name that is a substring of another name.
	 */
	size_t cand = 0;
	while (cand < name_count)
	{
		bool discard = false;
		for (size_t alt = 0; !discard && alt < name_count; alt++)
		{
			if (alt != cand)
			{
				if (op_strstr(family_names[alt], family_names[cand]) != 0)
					discard = true;
			};
		};
		if (discard)
		{
			name_count --;
			family_names[cand] = family_names[name_count];
		}
		else
			cand ++;
	};
	family_names[name_count] = 0;

	/* Finally, add the names to the return parameter.
	 */
	for (size_t i = 0; i < name_count; i++)
	{
		const char ** wrapper = OP_NEW(const char*, ());
		if (!wrapper)
		{
			OP_DELETEA(family_names);
			return OpStatus::ERR_NO_MEMORY;
		};
		*wrapper = family_names[i];
		names->Add(wrapper);
	};
	OP_DELETEA(family_names);
	return OpStatus::OK;
}


int UnixFontEngine::CreateFontInformation(const char* family_name, MDF_FontInformation*& fi, const MDF_FontInformation* copy)
{
	DEBUG_LOG("adding font family %s", family_name);
	int font_array_index = font_array.GetCount();
	// font_array_index might already be in use, need to look for one that's not
	MDF_FontInformation* dummy;
	while (OpStatus::IsSuccess(font_array.GetData(font_array_index, &dummy)))
		++font_array_index;

	fi = (OP_NEW(MDF_FontInformation, ()));
	if (!fi)
		return -1;

	if (copy)
	{
		op_memcpy(fi, copy, sizeof(*fi));
		fi->file_name_list = 0;
	}
	else
	{
		op_memset(fi, 0, sizeof(*fi));
	}

	if (OpStatus::IsError(CreateUTF16FromUTF8(family_name, fi->font_name)) ||
		OpStatus::IsError(AddFontInformation(font_array_index, fi)))
	{
		FreeFontInformation(fi);
		return -1;
	}

	return font_array_index;
}


OP_STATUS UnixFontEngine::CreateUTF16FromUTF8(const char* utf8, uni_char*& utf16)
{
	OpString tempbuffer;
	RETURN_IF_ERROR(tempbuffer.SetFromUTF8(utf8));

	utf16 = uni_strdup(tempbuffer.CStr());

	return utf16 ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


unsigned UnixFontEngine::ParseStyle(FcPattern* pattern)
{
	unsigned flags = 0;

	// First we get the weight and slant as indicated by the font
	int weight;
	if (FcPatternGetInteger(pattern, FC_WEIGHT, 0, &weight) == FcResultMatch && weight >= FC_WEIGHT_BOLD)
		flags |= STYLE_BOLD;

	int slant;
	if (FcPatternGetInteger(pattern, FC_SLANT, 0, &slant) == FcResultMatch && slant >= FC_SLANT_ITALIC)
		flags |= STYLE_ITALIC;

	// If the style string contains weight or slant information, this overrides information above
	FcChar8* style;
	if (FcPatternGetString(pattern, FC_STYLE, 0, &style) != FcResultMatch)
		return flags;

	// Walk through style tokens, separated by a space
	for (const char* token = (char*)style; token >= (char*)style; token = op_strchr(token, ' ') + 1)
	{
		if (StringUtils::StartsWith(token, "Bold"))
			flags |= STYLE_BOLD;

		else if (StringUtils::StartsWith(token, "Italic") ||
				 StringUtils::StartsWith(token, "Oblique"))
			flags |= STYLE_ITALIC;

		else if (StringUtils::StartsWith(token, "Sans"))
			flags |= STYLE_SANS;
	}

	return flags;
}


void UnixFontEngine::UpdateFontInformation(FcPattern* pattern, unsigned style, MDF_FontInformation* fi)
{
	if (style & STYLE_SANS)
		fi->has_serif = MDF_SERIF_SANS;
	else
		fi->has_serif = MDF_SERIF_UNDEFINED;

	FcBool scalable;
	if (FcPatternGetBool(pattern, FC_SCALABLE, 0, &scalable) == FcResultMatch)
		fi->bit_field |= scalable ? MDF_FontInformation::HAS_SCALABLE : MDF_FontInformation::HAS_NONSCALABLE;

	int spacing;
	if (FcPatternGetInteger(pattern, FC_SPACING, 0, &spacing) == FcResultMatch && spacing == FC_MONO)
		fi->bit_field |= MDF_FontInformation::IS_MONOSPACE;

	for (int i = 0; i < 4; i++)
		fi->ranges[i] = 0;

	FcCharSet* charset;
	if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &charset) == FcResultMatch)
		GetCharSetInfo(charset, UniBlocks, fi);

	FcLangSet* langset;
	if (FcPatternGetLangSet(pattern, FC_LANG, 0, &langset) == FcResultMatch)
		GetLangInfo(langset, fi);


	UpdateFontInformationHeuristically(fi);
}

void UnixFontEngine::GetCharSetInfo(FcCharSet* charset, const UINT32 current_block[][3], MDF_FontInformation* fi)
{
	/* This function maps fontconfig's character set information to the OS/2
	 * unicode block table that we need in fi->ranges (see
	 * http://www.microsoft.com/typography/otspec/os2.htm#ur)
	 *
	 * Both the block range information we have (see UniBlocks) and the
	 * character set pages we get from fontconfig are in increasing order.
	 * We walk through the block ranges and character set pages, determining
	 * which blocks we have characters for, and progressing to the next block
	 * if we have determined a block is present or the next character set page if
	 * we have checked all characters in it.
	 */
	FcChar32 map[FC_CHARSET_MAP_SIZE];
	FcChar32 next;
	FcChar32 ucs4 = FcCharSetFirstPage (charset, map, &next);

	while (ucs4 != FC_CHARSET_DONE)
	{
		// align block and map
		while ((*current_block)[1] < ucs4)
			current_block++;

		const UINT32* block = *current_block;

		if (block[0] == 0xFFFFFFFF)
			break;

		if (block[0] >= ucs4 + (FC_CHARSET_MAP_SIZE * 32))
		{
			ucs4 = FcCharSetNextPage (charset, map, &next);
			continue;
		}

		// check characters in map
		size_t i;
		bool next_block = false;
		for (i = 0; i < FC_CHARSET_MAP_SIZE && !next_block; ++i)
		{
			next_block = false;

			FcChar32 current_map = map[i];
			if (!current_map) // no characters
				continue;

			FcChar32 current_base = ucs4 + i * 32;
			next_block = current_base > block[1]; // past end of block
			if (next_block)
				break;

			if (block[0] >= current_base + 32) // before start of block
				continue;

			unsigned bitindex = current_base < block[0] ? block[0] - current_base : 0;
			for (; bitindex < 32 && current_base + bitindex <= block[1]; bitindex++)
			{
				if (current_map & (1 << bitindex))
				{
					fi->ranges[block[2] / 32] |= (1 << (block[2] % 32));
					break;
				}
			}

			next_block = bitindex < 32;
		}

		if (next_block)
			current_block++;
		else
			ucs4 = FcCharSetNextPage (charset, map, &next);
	}
}


namespace {

struct lang_cp_mapping
{
	const char * lang_pattern;
	int bit;
};

/* Any language string exactly matching 'lang_pattern' should set the
 * codepage bit 'bit'.
 *
 * More interesting pattern matching may be introduced later (e.g. if
 * 'lang_pattern' ends with "-", it is a prefix match (including the
 * "-")), so it is probably a bad idea to use this list outside
 * UnixFontEngine::GetlangInfo().
 *
 * The list should be sorted on 'lang_pattern'.  (As for the pattern
 * matching, more creative rules may be imposed later.)
 *
 * (This list is constructed to match reasonably well with the mapping
 * to scripts in fontdb.cpp.)
 */
lang_cp_mapping lang_cp_map[] =
{
	{ "ar", 6 }, // arabic.  Should I check "fa" (persian) too?
	{ "bg", 2 }, // cyrillic.  I've arbitrarily chosen russian and bulgarian as representatives
	{ "cs", 1 }, // latin-2.  I've arbitrarily chosen polish and czech as representatives
	{ "el", 3 }, // greek.
	{ "en", 0 }, // latin-1.  Probably anything supporting latin-1 will have "en" set...
	{ "he", 5 }, // hebrew.
	{ "ja", 17 }, // japanese.
	{ "ko", 19 }, // korean (hopefully this is not triggered by code page 21...)
	{ "lv", 7 }, // baltic.  I've arbitrarily chosen latvian as a representative (note that estonian is covered by latin-1)
	{ "pl", 1 }, // latin-2.  I've arbitrarily chosen polish and czech as representatives
	{ "ru", 2 }, // cyrillic.  I've arbitrarily chosen russian and bulgarian as representatives
	{ "th", 16 }, // thai.
	{ "tr", 4 }, // turkish.
	{ "vi", 8 }, // vietnamese.
	// { "zh",  ?? } There is no "chinese unknown" code page
	{ "zh-cn", 18 }, // simplified chinese (china).
	{ "zh-hk", 20 }, // traditional chinese (hong kong).
	{ "zh-sg", 18 }, // simplified chinese (singapore).
	{ "zh-tw", 20 }, // traditional chinese (taiwan).
	{ 0, -1 }
};

}; // private namespace

void UnixFontEngine::GetLangInfo(FcLangSet * langset, MDF_FontInformation* fi)
{
	/* This is rather roundabout.  We get the actual languages, but
	 * 'fi' needs to have the "codepages" set.  Core will then try to
	 * deduce the writing systems from the code pages.  It would make
	 * more sense to report writing systems directly from this method,
	 * I think.
	 */
	for (lang_cp_mapping * lc = lang_cp_map; lc->lang_pattern != 0; lc += 1)
	{
		if (FcLangSetHasLang(langset, (const FcChar8*)lc->lang_pattern) == FcLangEqual)
		{
			int bit = lc->bit;
			if (bit < 0 || bit > 63)
			{ // ignore
			}
			else if (bit < 32)
				fi->m_codepages[0] |= 1 << bit;
			else
				fi->m_codepages[1] |= 1 << (bit-32);
		};
	};
}

void UnixFontEngine::UpdateFontInformationHeuristically(MDF_FontInformation* fi)
{
	if (fi->has_serif == MDF_SERIF_UNDEFINED)
	{
		/* First, look for the "universal" serifness markers.
		 */
		if (uni_stristr(fi->font_name, UNI_L("sans")))
			fi->has_serif = MDF_SERIF_SANS;
		else if (uni_stristr(fi->font_name, UNI_L("serif")) ||
				 uni_stristr(fi->font_name, UNI_L("times")))
			fi->has_serif = MDF_SERIF_SERIF;
	};

	if (fi->has_serif == MDF_SERIF_UNDEFINED)
	{
		if ((fi->m_codepages[0] & (1 << 17)) != 0)
		{
			/* Japanese fonts generally come in two variants: Mincho
			 * and Gothic.  The Mincho style is essentially equivalent
			 * to what western typography calls "serif", and Gothic is
			 * likewise "sans serif".
			 *
			 * HanaMin and Mona are specific font-families that may be
			 * in somewhat widespread use.
			 */
			if (uni_stristr(fi->font_name, UNI_L("mincho")) != 0)
				fi->has_serif = MDF_SERIF_SERIF;
			else if (uni_stristr(fi->font_name, UNI_L("gothic")) != 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("mona")) != 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("hanamin")) != 0)
				fi->has_serif = MDF_SERIF_SERIF;
		};

		if ((fi->m_codepages[0] & ((1 << 18) | (1 << 20))) != 0)
		{
			/* Likewise, Chinese has the Ming, Song and Sung
			 * meta-families that are serif-like and Gothic, Hei(Ti)
			 * and Sans that are sans-serif-like.
			 *
			 * SimSun is a specific font-family that may be in
			 * somewhat widespread use.
			 */
			if (uni_stristr(fi->font_name, UNI_L("ming"))!= 0)
				fi->has_serif = MDF_SERIF_SERIF;
			else if (uni_stristr(fi->font_name, UNI_L("song"))!= 0)
				fi->has_serif = MDF_SERIF_SERIF;
			else if (uni_stristr(fi->font_name, UNI_L("sung"))!= 0)
				fi->has_serif = MDF_SERIF_SERIF;
			else if (uni_stristr(fi->font_name, UNI_L("gothic"))!= 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("hei"))!= 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("simsun"))!= 0)
				fi->has_serif = MDF_SERIF_SERIF;
		};

		if ((fi->m_codepages[0] & (1 << 19)) != 0)
		{
			/* Likewise, Korean has the Batang meta-family that is
			 * serif-like and Gothic, Dotum, Gulim which are
			 * sans-serif-like.
			 *
			 * Shinmun is (probably) a specific font-family that may
			 * be in somewhat widespread use.
			 */
			if (uni_stristr(fi->font_name, UNI_L("batang")) != 0)
				fi->has_serif = MDF_SERIF_SERIF;
			else if (uni_stristr(fi->font_name, UNI_L("gothic")) != 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("dotum")) != 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("gulim")) != 0)
				fi->has_serif = MDF_SERIF_SANS;
			else if (uni_stristr(fi->font_name, UNI_L("shinmun")) != 0)
				fi->has_serif = MDF_SERIF_SANS;
		};
	};
};


OP_STATUS UnixFontEngine::CreateFileInformation(FcPattern* pattern, unsigned style, MDF_FontFileNameInfo*& fileinfo_head)
{
	MDF_FontFileNameInfo* temp = OP_NEW(MDF_FontFileNameInfo, ());
	if (!temp)
		return OpStatus::ERR_NO_MEMORY;
	FcHolder<MDF_FontFileNameInfo, FreeFontFileNameInfo> info (temp);

	info->next = fileinfo_head;
	info->fixed_sizes = 0;
	info->num_fixed_sizes = 0;

	FcChar8* file;
	if (FcPatternGetString(pattern, FC_FILE, 0, &file) != FcResultMatch)
		return OpStatus::ERR;

	OpString tmp_filename;
	RETURN_IF_ERROR(PosixNativeUtil::FromNative((char*)file, &tmp_filename));
	info->file_name = uni_strdup(tmp_filename.CStr());
	DEBUG_LOG("adding font file %s", (char*)file);

	if (FcPatternGetInteger(pattern, FC_INDEX, 0, &info->font_index) != FcResultMatch)
		info->font_index = 0;

	if (style & STYLE_BOLD)
		info->bit_field |= MDF_FontFileNameInfo::IS_BOLD;
	if (style & STYLE_ITALIC)
		info->bit_field |= MDF_FontFileNameInfo::IS_ITALIC;

	FcBool scalable;
	scalable = FcPatternGetBool(pattern, FC_SCALABLE, 0, &scalable) == FcResultMatch && scalable;
	if (!scalable)
		RETURN_IF_ERROR(GetNonScalableFileInfo(pattern, info.get()));

	fileinfo_head = info.release();

	return OpStatus::OK;
}


OP_STATUS UnixFontEngine::GetNonScalableFileInfo(FcPattern* pattern, MDF_FontFileNameInfo* info)
{
	info->bit_field |= MDF_FontFileNameInfo::ONLY_BITMAP;

	// We need to list available pixel sizes for nonscalable font
	FcHolder<FcObjectSet, FcObjectSetDestroy> os (FcObjectSetCreate());
	if (!os.get())
		return OpStatus::ERR_NO_MEMORY;

	if (!FcObjectSetAdd(os.get(), FC_PIXEL_SIZE))
		return OpStatus::ERR_NO_MEMORY;

	FcHolder<FcFontSet, FcFontSetDestroy> list (FcFontList(m_config, pattern, os.get()));
	if (!list.get())
		return OpStatus::ERR_NO_MEMORY;

	info->num_fixed_sizes = list->nfont;
	info->fixed_sizes = OP_NEWA(int, list->nfont);
	if (!info->fixed_sizes)
		return OpStatus::ERR_NO_MEMORY;

	for (int i = 0; i < list->nfont; i++)
	{
		// add pixel sizes
		FcPattern* font = list->fonts[i];
		double pixelsize;
		if (FcPatternGetDouble(font, FC_PIXEL_SIZE, 0, &pixelsize) == FcResultMatch)
			info->fixed_sizes[i] = pixelsize;
		else
			info->fixed_sizes[i] = 0;
	}

	return OpStatus::OK;
}


OP_STATUS UnixFontEngine::Init()
{
	RETURN_IF_ERROR(ReadSystemSettings());
	return SetInternationalFonts();
}


OP_STATUS UnixFontEngine::ReadSystemSettings()
{
	// First, we get the Xft settings - they are global and form a baseline
	GetXftSettings();
	if (g_startup_settings.debug_font)
		PrintDebugSettings(m_xft_settings, "Xft baseline settings");

	// Now check if the toolkit would like to set any font settings
	if (!g_pcunix->GetIntegerPref(PrefsCollectionUnix::PreferFontconfigSettings))
		g_toolkit_library->GetUiSettings()->GetFontRenderSettings(m_toolkit_settings);
	if (g_startup_settings.debug_font)
		PrintDebugSettings(m_toolkit_settings, "Toolkit font render settings");

	// Now check for settings from fontconfig with an empty pattern
	FcHolder<FcPattern, FcPatternDestroy> pattern (FcPatternCreate());
	if (!pattern.get())
		return OpStatus::ERR_NO_MEMORY;

	// Execute substitutions
	if (!FcConfigSubstitute(m_config, pattern.get(), FcMatchFont))
		return OpStatus::ERR_NO_MEMORY;
	FcDefaultSubstitute(pattern.get());

	// Set all settings that weren't set by the toolkit
	ToolkitUiSettings::FontRenderSettings settings = GetRenderSettings(pattern.get());
	if (g_startup_settings.debug_font)
		PrintDebugSettings(settings, "Final settings adjusted by fontconfig");

	// Use settings to set smoothing and lcd filter (these settings can't be set per-font)
	smoothing = NONE;

	if (settings.antialias)
	{
		switch (settings.rgba)
		{
			case FC_RGBA_RGB:
				smoothing = RGB;
				break;
			case FC_RGBA_BGR:
				smoothing = BGR;
				break;
			case FC_RGBA_VRGB:
				smoothing = VRGB;
				break;
			case FC_RGBA_VBGR:
				smoothing = VBGR;
				break;
		}
	}

	// We load this function via dlsym because it's not available on old Linux distributions (eg. CentOS 5)
	typedef FT_Error (*SetLcdFilterFunction) (FT_Library library, FT_LcdFilter filter);
	SetLcdFilterFunction SetLcdFilter = (SetLcdFilterFunction) dlsym(RTLD_DEFAULT, "FT_Library_SetLcdFilter");

	if (!SetLcdFilter)
		return OpStatus::OK;

	switch (settings.lcd_filter)
	{
		case FC_LCD_NONE:
			SetLcdFilter(ft_library, FT_LCD_FILTER_NONE);
			break;
		case FC_LCD_DEFAULT:
			SetLcdFilter(ft_library, FT_LCD_FILTER_DEFAULT);
			break;
		case FC_LCD_LIGHT:
			SetLcdFilter(ft_library, FT_LCD_FILTER_LIGHT);
			break;
		case FC_LCD_LEGACY:
			SetLcdFilter(ft_library, FT_LCD_FILTER_LEGACY);
			break;
	}

	return OpStatus::OK;
}


void UnixFontEngine::PrintDebugSettings(const ToolkitUiSettings::FontRenderSettings& settings, const char* description)
{
	printf("opera: %s:\n", description);

	printf("opera: antialias: %s\n", settings.antialias < 0 ? "not specified" : (settings.antialias ? "yes" : "no"));
	printf("opera: hinting: %s\n", settings.hinting < 0 ? "not specified" : (settings.hinting ? "yes" : "no"));
	printf("opera: autohint: %s\n", settings.autohint < 0 ? "not specified" : (settings.autohint ? "yes" : "no"));

	const char* hintstyle = "not specified";
	switch (settings.hintstyle)
	{
		case FC_HINT_NONE:
			hintstyle = "none";
			break;
		case FC_HINT_SLIGHT:
			hintstyle = "slight";
			break;
		case FC_HINT_MEDIUM:
			hintstyle = "medium";
			break;
		case FC_HINT_FULL:
			hintstyle = "full";
			break;
	}
	printf("opera: hintstyle: %s\n", hintstyle);

	const char* rgba = "not specified";
	switch (settings.rgba)
	{
		case FC_RGBA_RGB:
			rgba = "rgb";
			break;
		case FC_RGBA_BGR:
			rgba = "bgr";
			break;
		case FC_RGBA_VRGB:
			rgba = "vrgb";
			break;
		case FC_RGBA_VBGR:
			rgba = "vbgr";
			break;
	}
	printf("opera: rgba: %s\n", rgba);

	const char* lcd_filter = "not specified";
	switch (settings.lcd_filter)
	{
		case FC_LCD_NONE:
			lcd_filter = "none";
			break;
		case FC_LCD_DEFAULT:
			lcd_filter = "default";
			break;
		case FC_LCD_LIGHT:
			lcd_filter = "light";
			break;
		case FC_LCD_LEGACY:
			lcd_filter = "legacy";
			break;
	}
	printf("opera: lcdfilter: %s\n", lcd_filter);
	printf("\n");
}


void UnixFontEngine::GetXftSettings()
{
	// defaults
	m_xft_settings.rgba = FC_RGBA_UNKNOWN;
	m_xft_settings.hintstyle = FC_HINT_FULL;
	m_xft_settings.lcd_filter = FC_LCD_LEGACY;
	m_xft_settings.antialias = true;
	m_xft_settings.hinting = true;
	m_xft_settings.autohint = false;

	XRDatabase database;

	if (OpStatus::IsError(database.Init(XResourceManagerString(g_x11->GetDisplay()))))
		return;

	FcNameConstant((FcChar8*)database.GetStringValue("Xft.rgba", ""), &m_xft_settings.rgba);
	FcNameConstant((FcChar8*)database.GetStringValue("Xft.hintstyle", ""), &m_xft_settings.hintstyle);
	FcNameConstant((FcChar8*)database.GetStringValue("Xft.lcdfilter", ""), &m_xft_settings.lcd_filter);

	m_xft_settings.antialias = database.GetBooleanValue("Xft.antialias", m_xft_settings.antialias);
	m_xft_settings.hinting = database.GetBooleanValue("Xft.hinting", m_xft_settings.hinting);
	m_xft_settings.autohint = database.GetBooleanValue("Xft.autohint", m_xft_settings.autohint);
}


ToolkitUiSettings::FontRenderSettings UnixFontEngine::GetRenderSettings(FcPattern* match)
{
	ToolkitUiSettings::FontRenderSettings settings = m_toolkit_settings;

	// Read settings not overridden by toolkit
	if (settings.rgba == ToolkitUiSettings::USE_FONTCONFIG)
		settings.rgba = FcPatternGetInteger(match, FC_RGBA, 0, &settings.rgba) == FcResultMatch ? settings.rgba : m_xft_settings.rgba;

	if (settings.hintstyle == ToolkitUiSettings::USE_FONTCONFIG)
		settings.hintstyle = FcPatternGetInteger(match, FC_HINT_STYLE, 0, &settings.hintstyle) == FcResultMatch ? settings.hintstyle : m_xft_settings.hintstyle;

	if (settings.lcd_filter == ToolkitUiSettings::USE_FONTCONFIG)
		settings.lcd_filter = FcPatternGetInteger(match, FC_LCD_FILTER, 0, &settings.lcd_filter) == FcResultMatch ? settings.lcd_filter : m_xft_settings.lcd_filter;

	if (settings.antialias == ToolkitUiSettings::USE_FONTCONFIG)
		settings.antialias = FcPatternGetBool(match, FC_ANTIALIAS, 0, &settings.antialias) == FcResultMatch ? settings.antialias : m_xft_settings.antialias;

	if (settings.hinting == ToolkitUiSettings::USE_FONTCONFIG)
		settings.hinting = FcPatternGetBool(match, FC_HINTING, 0, &settings.hinting) == FcResultMatch ? settings.hinting : m_xft_settings.hinting;

	if (settings.autohint == ToolkitUiSettings::USE_FONTCONFIG)
		settings.autohint = FcPatternGetBool(match, FC_AUTOHINT, 0, &settings.autohint) == FcResultMatch ? settings.autohint : m_xft_settings.autohint;

	return settings;
}


MDE_FONT* UnixFontEngine::GetFont(int font_nr, int size, BOOL bold, BOOL italic)
{
	int* alias_nr;
	if (OpStatus::IsSuccess(m_aliases.GetData(font_nr, &alias_nr)))
		return GetFont(reinterpret_cast<INTPTR>(alias_nr), size, bold, italic);

	MDE_FONT* font = MDF_FTFontEngine::GetFont(font_nr, size, bold, italic);
	if (!font)
		 return 0;

	MDF_FT_FONT* font_data = (MDF_FT_FONT*)font->private_data;

	OpString8 family_name;
	RETURN_VALUE_IF_ERROR(CreateUTF8FromUTF16(font_data->font_name, family_name), font);

	FcHolder<FcPattern, FcPatternDestroy> pattern (FcPatternCreate());
	if (!pattern.get() ||
		!FcPatternAddString(pattern.get(), FC_FAMILY, (const FcChar8*)family_name.CStr()) ||
		!FcPatternAddDouble(pattern.get(), FC_PIXEL_SIZE, font_data->font_size) ||
		!FcPatternAddInteger(pattern.get(), FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR) ||
		!FcPatternAddInteger(pattern.get(), FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN))
		return font;

	// Execute substitutions
	if (!FcConfigSubstitute(m_config, pattern.get(), FcMatchFont))
		return font;
	FcDefaultSubstitute(pattern.get());

	// Override load flags with settings for this pattern
	font_data->load_flags = GetLoadFlags(pattern.get());

	return font;
}


int UnixFontEngine::GetLoadFlags(FcPattern* match)
{
	int load_flags = FT_LOAD_DEFAULT | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;
	int load_target = FT_LOAD_TARGET_NORMAL;

	ToolkitUiSettings::FontRenderSettings settings = GetRenderSettings(match);

	if (settings.antialias)
	{
		switch (smoothing)
		{
			case RGB: /* fallthrough */
			case BGR:
				load_target = FT_LOAD_TARGET_LCD;
				break;
			case VRGB: /* fallthrough */
			case VBGR:
				load_target = FT_LOAD_TARGET_LCD_V;
				break;
		}

		if (settings.hinting)
		{
			switch (settings.hintstyle)
			{
				case FC_HINT_NONE:
					settings.hinting = FALSE;
					break;
				case FC_HINT_SLIGHT:
					load_target = FT_LOAD_TARGET_LIGHT;
					break;
				case FC_HINT_MEDIUM:
					load_target = FT_LOAD_TARGET_NORMAL;
					break;
			}
		}

		if (settings.hinting)
		{
			if (settings.autohint)
				load_flags |= FT_LOAD_FORCE_AUTOHINT;
		}
		else
		{
			load_flags |= FT_LOAD_NO_HINTING;
		}
	}
	else
	{
		load_target = FT_LOAD_TARGET_MONO;
	}

	return load_flags | load_target;
}


OP_STATUS UnixFontEngine::CreateUTF8FromUTF16(const uni_char* utf16, OpString8& utf8)
{
	UTF16toUTF8Converter encoder;

	int utf16_size = UNICODE_SIZE(uni_strlen(utf16));

	char* buffer = utf8.Reserve(encoder.BytesNeeded(utf16, utf16_size));
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	int read;
	int written = encoder.Convert(utf16, utf16_size, buffer, utf8.Capacity(), &read);
	if (written == -1)
		return OpStatus::ERR_NO_MEMORY;

	buffer[written] = '\0';

	return OpStatus::OK;
}


OP_STATUS UnixFontEngine::SetDefaultFonts()
{
	RETURN_IF_ERROR(GetDefaultFontFamily(m_default_fonts[SERIF], NULL, "Serif", NULL));
	g_opera->libvega_module.default_serif_font = m_default_fonts[SERIF].CStr();
	g_opera->libvega_module.default_font = m_default_fonts[SERIF].CStr();

	RETURN_IF_ERROR(GetDefaultFontFamily(m_default_fonts[SANS], NULL, "Sans", NULL));
	g_opera->libvega_module.default_sans_serif_font = m_default_fonts[SANS].CStr();

	RETURN_IF_ERROR(GetDefaultFontFamily(m_default_fonts[CURSIVE], NULL, "Cursive", NULL));
	g_opera->libvega_module.default_cursive_font = m_default_fonts[CURSIVE].CStr();

	RETURN_IF_ERROR(GetDefaultFontFamily(m_default_fonts[FANTASY], NULL, "Fantasy", NULL));
	g_opera->libvega_module.default_fantasy_font = m_default_fonts[FANTASY].CStr();

	RETURN_IF_ERROR(GetDefaultFontFamily(m_default_fonts[MONOSPACE], NULL, "Monospace", NULL));
	g_opera->libvega_module.default_monospace_font = m_default_fonts[MONOSPACE].CStr();

	if (g_startup_settings.debug_font)
		PrintFontChoices(m_default_fonts, UNI_L("default fonts"));

	return OpStatus::OK;
}


void UnixFontEngine::PrintFontChoices(const OpString fonts[], const uni_char* type)
{
	OpString description;
	description.AppendFormat(UNI_L("opera: font choices for %s:\n"), type);
	description.AppendFormat(UNI_L("serif:      %s\n"), fonts[SERIF].CStr() ? fonts[SERIF].CStr() : UNI_L("[none]"));
	description.AppendFormat(UNI_L("sans-serif: %s\n"), fonts[SANS].CStr() ? fonts[SANS].CStr() : UNI_L("[none]"));
	description.AppendFormat(UNI_L("cursive:    %s\n"), fonts[CURSIVE].CStr() ? fonts[CURSIVE].CStr() : UNI_L("[none]"));
	description.AppendFormat(UNI_L("fantasy:    %s\n"), fonts[FANTASY].CStr() ? fonts[FANTASY].CStr() : UNI_L("[none]"));
	description.AppendFormat(UNI_L("monospace:  %s\n\n"), fonts[MONOSPACE].CStr() ? fonts[MONOSPACE].CStr() : UNI_L("[none]"));
	if (description.IsEmpty())
		return;

	OpString8 desc_utf8;
	RETURN_VOID_IF_ERROR(CreateUTF8FromUTF16(description.CStr(), desc_utf8));
	printf("%s", desc_utf8.CStr());
}


OP_STATUS UnixFontEngine::DetermineAliases()
{
	for (size_t i = 0; i < ARRAY_SIZE(AliasFonts); i++)
	{
		FcHolder<FcPattern, FcPatternDestroy> pattern (FcPatternCreate());
		if (!pattern.get() ||
			!FcPatternAddString(pattern.get(), FC_FAMILY, (const FcChar8*)AliasFonts[i]))
			return OpStatus::ERR_NO_MEMORY;

		// Execute substitutions
		if (!FcConfigSubstitute(m_config, pattern.get(), FcMatchPattern))
			return OpStatus::ERR_NO_MEMORY;
		FcDefaultSubstitute(pattern.get());

		// Get font from fontconfig
		FcResult result;
		FcHolder<FcPattern, FcPatternDestroy> match (FcFontMatch(m_config, pattern.get(), &result));
		if (!match.get())
			continue;

		FcChar8* fc_family = 0;
		if (FcPatternGetString(match.get(), FC_FAMILY, 0, &fc_family) != FcResultMatch ||
			!FcStrCmp((const FcChar8*)AliasFonts[i], fc_family))
			continue;

		// Check if we have the font found by fontconfig
		MDF_FontInformation* font_info = 0;
		int font_index = 0;
		RETURN_IF_MEMORY_ERROR(FindFontInformation((const char*)fc_family, font_info, &font_index));
		if (!font_info)
			continue;

		// Create alias
		MDF_FontInformation* alias_font_info = 0;
		int alias_index = CreateFontInformation(AliasFonts[i], alias_font_info, font_info);
		if (alias_index < 0)
			return OpStatus::ERR_NO_MEMORY;

		// Casting the integer int* because our hash table only stores pointers
		RETURN_IF_ERROR(m_aliases.Add(alias_index, reinterpret_cast<int*>((INTPTR)font_index)));
	}

	return OpStatus::OK;
}


struct UnixFontEngine::DefaultGenericFonts
{
	WritingSystem::Script script;
	const uni_char* script_name;
	OpString font[_DEFAULT_FONTS_COUNT];

	DefaultGenericFonts(WritingSystem::Script s, const uni_char* name)
		: script(s), script_name(name)
	{}

	OP_STATUS RegisterFonts()
	{
		VEGAOpFontManager::DefaultFonts fonts;
		fonts.default_font = 0;
		fonts.serif_font = font[SERIF].CStr();
		fonts.sans_serif_font = font[SANS].CStr();
		fonts.cursive_font = font[CURSIVE].CStr();
		fonts.fantasy_font = font[FANTASY].CStr();
		fonts.monospace_font = font[MONOSPACE].CStr();

		if (g_startup_settings.debug_font)
			UnixFontEngine::PrintFontChoices(font, script_name);

		return ((VEGAMDFOpFontManager*)styleManager->GetFontManager())->SetGenericFonts(fonts, script);
	}
};


OP_STATUS UnixFontEngine::SetInternationalFonts()
{
	const char* encoding;
	TRAPD(rc, encoding = g_op_system_info->GetSystemEncodingL());
	if (OpStatus::IsError(rc))
		encoding = 0;
	WritingSystem::Script script = encoding ? WritingSystem::FromEncoding(encoding) : WritingSystem::LatinWestern;

	// Entries taken from https://wiki.oslo.opera.com/developerwiki/Desktop/Specifications/Linux/Fonts/International#Font_Samples
	// TODO 1: Make it possible to read candidates from file
	// TODO 2: Make it possible to use names with locale encoding

	{
		// Chinese Simplified (keep in sync with Chinese Unknown)
		DefaultGenericFonts zhcn(WritingSystem::ChineseSimplified, UNI_L("ChineseSimplified"));

		GetDefaultFontFamily(zhcn.font[SERIF], "zh-cn", "AR PL UMing CN", "AR PL Ukai CN", "Serif", NULL);
		GetDefaultFontFamily(zhcn.font[SANS], "zh-cn", "WenQuanYi Zen Hei", "WenQuanYi Micro Hei", "Sans", NULL);
		GetDefaultFontFamily(zhcn.font[MONOSPACE], "zh-cn", "WenQuanYi Zen Hei Mono", "WenQuanYi Micro Hei Mono", "Monospace", NULL);
		GetDefaultFontFamily(zhcn.font[CURSIVE], "zh-cn", "WenQuanYi Zen Hei", "WenQuanYi Micro Hei", "Cursive", NULL);
		GetDefaultFontFamily(zhcn.font[FANTASY], "zh-cn", "WenQuanYi Zen Hei", "WenQuanYi Micro Hei", "Fantasy", NULL);

		RETURN_IF_ERROR(zhcn.RegisterFonts());

		// Chinese Traditional (keep in sync with Chinese Unknown)
		DefaultGenericFonts zhtw(WritingSystem::ChineseTraditional, UNI_L("ChineseTraditional"));

		GetDefaultFontFamily(zhtw.font[SERIF], "zh-tw", "AR PL UMing TW", "Serif", NULL);
		GetDefaultFontFamily(zhtw.font[SANS], "zh-tw", "AR PL UMing TW", "Sans", NULL);
		GetDefaultFontFamily(zhtw.font[MONOSPACE], "zh-tw", "AR PL UMing TW", "Monospace", NULL);
		GetDefaultFontFamily(zhtw.font[CURSIVE], "zh-tw", "AR PL UMing TW", "Cursive", NULL);
		GetDefaultFontFamily(zhtw.font[FANTASY], "zh-tw", "AR PL UMing TW", "Fantasy", NULL);

		RETURN_IF_ERROR(zhtw.RegisterFonts());

		// Chinese Unknown
		// In case we can not determine simplified vs. traditional. Use traditional only if system encoding says so
		DefaultGenericFonts& chunknown = (script == WritingSystem::ChineseTraditional) ? zhtw : zhcn;
		chunknown.script = WritingSystem::ChineseUnknown;
		chunknown.script_name = UNI_L("ChineseUnknown");

		RETURN_IF_ERROR(chunknown.RegisterFonts());
	}

	// Japanese
	{
		DefaultGenericFonts f(WritingSystem::Japanese, UNI_L("Japanese"));

		GetDefaultFontFamily(f.font[SERIF], "ja", "TakaoPMincho", "IPAPMincho", "IPAMonaPMincho", "Serif", NULL);
		GetDefaultFontFamily(f.font[SANS], "ja", "TakaoPGothic", "IPAPGothic", "IPAMonaPGothic", "VL PGothic", "Sans", NULL);
		GetDefaultFontFamily(f.font[MONOSPACE], "ja", "TakaoGothic", "IPAGothic", "IPAMonaGothic", "VL Gothic", "Monospace", NULL);
		GetDefaultFontFamily(f.font[CURSIVE], "ja", "TakaoPGothic", "IPAPGothic", "IPAMonaPGothic", "VL PGothic", "Cursive", NULL);
		GetDefaultFontFamily(f.font[FANTASY], "ja", "TakaoPGothic", "IPAPGothic", "IPAMonaPGothic", "VL PGothic", "Fantasy", NULL);

		RETURN_IF_ERROR(f.RegisterFonts());
	}

	// Korean
	{
		DefaultGenericFonts f(WritingSystem::Korean, UNI_L("Korean"));

		GetDefaultFontFamily(f.font[SERIF], "ko", "NanumMyeongjo", "NanumMyeongjoOTF", "Baekmuk Gulim", "UnBatang", "Serif", NULL);
		GetDefaultFontFamily(f.font[SANS], "ko", "NanumGothic", "NanumGothicOTF", "Baekmuk Batang", "UnDotum", "Sans", NULL);
		GetDefaultFontFamily(f.font[MONOSPACE], "ko", "NanumGothic", "NanumGothicOTF", "Baekmuk Gulim", "UnDotum", "Monospace", NULL);
		GetDefaultFontFamily(f.font[CURSIVE], "ko", "NanumGothic", "NanumGothicOTF", "Baekmuk Gulim", "UnDotum", "Cursive", NULL);
		GetDefaultFontFamily(f.font[FANTASY], "ko", "NanumGothic", "NanumGothicOTF", "Baekmuk Gulim", "UnDotum", "Fantasy", NULL);

		RETURN_IF_ERROR(f.RegisterFonts());
	}

	// Unknown
	// Should be defined as the last. Use system encoding for unknown scripts
	{
		DefaultGenericFonts f(WritingSystem::Unknown, UNI_L("Unknown"));
		const uni_char* name;

		name = ((VEGAMDFOpFontManager*)styleManager->GetFontManager())->GetGenericFontName(GENERIC_FONT_SERIF, script);
		f.font[SERIF].Set(name);
		name = ((VEGAMDFOpFontManager*)styleManager->GetFontManager())->GetGenericFontName(GENERIC_FONT_SANSSERIF, script);
		f.font[SANS].Set(name);
		name = ((VEGAMDFOpFontManager*)styleManager->GetFontManager())->GetGenericFontName(GENERIC_FONT_CURSIVE, script);
		f.font[CURSIVE].Set(name);
		name = ((VEGAMDFOpFontManager*)styleManager->GetFontManager())->GetGenericFontName(GENERIC_FONT_FANTASY, script);
		f.font[FANTASY].Set(name);
		name = ((VEGAMDFOpFontManager*)styleManager->GetFontManager())->GetGenericFontName(GENERIC_FONT_MONOSPACE, script);
		f.font[MONOSPACE].Set(name);

		RETURN_IF_ERROR(f.RegisterFonts());
	}

	/* Since we have modified the default fonts, we need to tell the
	 * style manager to update its settings accordingly.
	 */
	styleManager->InitGenericFont(StyleManager::SERIF);
	styleManager->InitGenericFont(StyleManager::SANS_SERIF);
	styleManager->InitGenericFont(StyleManager::CURSIVE);
	styleManager->InitGenericFont(StyleManager::FANTASY);
	styleManager->InitGenericFont(StyleManager::MONOSPACE);

	return OpStatus::OK;
}


OP_STATUS UnixFontEngine::GetDefaultFontFamily(OpString& font_family, const char* lang, const char* to_match, ...)
{
	FcHolder<FcPattern, FcPatternDestroy> pattern (FcPatternCreate());
	if (!pattern.get())
		return OpStatus::ERR_NO_MEMORY;

	if (lang)
	{
		FcHolder<FcLangSet, FcLangSetDestroy> langset (FcLangSetCreate());
		if (!langset.get() ||
		    !FcLangSetAdd(langset.get(), (const FcChar8*)lang) ||
			!FcPatternAddLangSet(pattern.get(), FC_LANG, langset.get()))
			return OpStatus::ERR_NO_MEMORY;
	}

	va_list ap;
	va_start(ap, to_match);

	while (to_match)
	{
		if (!FcPatternAddString(pattern.get(), FC_FAMILY, (const FcChar8*)(to_match)))
		{
			va_end(ap);
			return OpStatus::ERR_NO_MEMORY;
		}

		to_match = va_arg(ap, const char*);
	}
	va_end(ap);

	// Execute substitutions
	if (!FcConfigSubstitute(m_config, pattern.get(), FcMatchPattern))
		return OpStatus::ERR_NO_MEMORY;
	FcDefaultSubstitute(pattern.get());

	// Get font from fontconfig
	FcResult result;
	FcHolder<FcPattern, FcPatternDestroy> match (FcFontMatch(m_config, pattern.get(), &result));
	if (!match.get())
	{
		DEBUG_LOG("can't find font for %s", lang ? lang : "default");
		return OpStatus::ERR;
	}

	FcChar8* filename = 0;
	FcChar8* style = 0;
	FcChar8* family = 0;
	if (FcPatternGetString(match.get(), FC_FILE, 0, &filename) != FcResultMatch ||
		FcPatternGetString(match.get(), FC_STYLE, 0, &style) != FcResultMatch ||
		FcPatternGetString(match.get(), FC_FAMILY, 0, &family) != FcResultMatch)
		return OpStatus::ERR;

	// Get list for match, so that we can get at the original font information
	// which we need to get a proper family name (the same as the one used in
	// AddFontsFromPattern)
	FcHolder<FcPattern, FcPatternDestroy> listpattern (FcPatternCreate());
	FcHolder<FcObjectSet, FcObjectSetDestroy> os (FcObjectSetCreate());
	if (!listpattern.get() || !os.get() ||
		!FcPatternAddString(listpattern.get(), FC_FILE, filename) ||
		!FcPatternAddString(listpattern.get(), FC_STYLE, style) ||
		!FcPatternAddString(listpattern.get(), FC_FAMILY, family) ||
		!FcObjectSetAdd(os.get(), FC_FAMILY))
		return OpStatus::ERR_NO_MEMORY;

	FcHolder<FcFontSet, FcFontSetDestroy> list (FcFontList(m_config, listpattern.get(), os.get()));
	if (!list.get())
		return OpStatus::ERR_NO_MEMORY;

	FcPattern* family_pattern = list->nfont ? list->fonts[0] : match.get();
	OpAutoVector<const char *> matched_families;
	RETURN_IF_ERROR(GetFamilyNames(family_pattern, &matched_families));
	if (matched_families.GetCount() <= 0)
		return OpStatus::ERR;
	const char * matched_family = *matched_families.Get(0);

	return font_family.SetFromUTF8(matched_family);
}

#undef DEBUG_LOG
#undef RETURN_IF_ERROR_MSG
