/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWS_FONT_MANAGER_H
#define WINDOWS_FONT_MANAGER_H

typedef HANDLE HTHEME;

#include "modules/libvega/src/oppainter/vegaopfont.h"
#ifdef SVG_SUPPORT
class SVGPath;
#endif // SVG_SUPPORT

#include "platforms/windows/windows_ui/hash_tables.h"
#include "modules/util/opstrlst.h"
#include <usp10.h>

#ifdef _GLYPHTESTING_SUPPORT_

#define B2LENDIAN(x) (((x & 0xff) << 8) | ((x >> 8) & 0xff)) 
#define B2LENDIAN32(x) (((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | ((x >> 24) & 0xff))

#define TT_PLATFORM_WINDOWS 3

struct ProcessedGlyph;
class GdiOpFontManager;

typedef struct
{
    uint16 version;
    uint16 number;
} CMapIndex;

typedef struct
{
    uint16 platformId;
    uint16 platformSpecId;
    uint32 offset;
} CMapSubTable;

typedef struct
{
    uint16 format;
    union
    {
        struct // format 0, 2, 6
        {
            uint16 length;
            uint16 language;
        } f0;
        struct // format 4
        {
            uint16 length;
            uint16 language;
            uint16 segCountX2;    // 2 * segCount
            uint16 searchRange;   // 2 * (2**FLOOR(log2(segCount)))
            uint16 entrySelector; // log2(searchRange/2)
            uint16 rangeShift;    // (2 * segCount) - searchRange
 
            byte   rest;

            // The rest of the subtable is dynamic
            /*
            uint16 endCode[segCount] // Ending character code for each segment, last = 0xFFFF.
            uint16 reservedPad; // This value should be zero
            uint16 startCode[segCount]; // Starting character code for each segment
            uint16 idDelta[segCount];// Delta for all character codes in segment
            uint16 idRangeOffset[segCount];// Offset in bytes to glyph indexArray, or 0
            uint16 glyphIndexArray[variable];
            */
        } f4;
        struct // format 8.0, 10.0, 12.0
        {
        } f8;
    };
} CMapGlyphTable;

#endif

// This structure is used to cache enough data for the web fonts check to work. 
// Saves having to do the open/close on every font for every web font we want to use.
struct WindowsRawFontData
{
	WindowsRawFontData(UINT32 font_nr, BYTE* raw_data, UINT32 raw_data_length) 
		: m_font_nr(font_nr), m_raw_data(raw_data), m_raw_data_length(raw_data_length) 
	{}
	~WindowsRawFontData()
	{
		OP_DELETEA(m_raw_data);
	}
	UINT32	m_font_nr;				// font number, maps into the logfonts array
	BYTE*	m_raw_data;				// the raw font header data, ownership of the data is transferred to this class now!
	UINT32	m_raw_data_length;		// length of the raw data, usually around 1-2K in size
};



class GdiOpFontManager : 
#ifdef VEGA_GDIFONT_SUPPORT
	public VEGAOpFontManager
#else
	public OpFontManager
#endif
{
public:
	GdiOpFontManager();
	
	virtual ~GdiOpFontManager();

	static bool IsLoaded() {return s_is_loaded;}
	
#ifdef VEGA_GDIFONT_SUPPORT
	virtual VEGAFont* GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius);
#else
	virtual OpFont* CreateFont(const uni_char* face, UINT32 size,
				   UINT8 weight, BOOL italic, BOOL must_have_getoutline);
#endif

#ifdef PI_WEBFONT_OPAQUE
  #ifdef VEGA_GDIFONT_SUPPORT
	virtual VEGAFont* GetVegaFont(OpFontManager::OpWebFontRef webfont, UINT32 size, INT32 blur_radius);
	virtual VEGAFont* GetVegaFont(OpFontManager::OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius);
  #else // VEGA_GDIFONT_SUPPORT
	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT32 size);
  #endif // VEGA_GDIFONT_SUPPORT
	virtual OpFontInfo::FontType GetWebFontType(OpWebFontRef webfont){return OpFontInfo::PLATFORM_WEBFONT;}
	virtual OP_STATUS AddWebFont(OpWebFontRef& webfont, const uni_char* path);
	virtual OP_STATUS RemoveWebFont(OpWebFontRef webfont);
 	virtual OP_STATUS GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo);
	//Needs to be implemented
	virtual OP_STATUS GetLocalFont(OpWebFontRef& localfont, const uni_char* facename);
	virtual BOOL SupportsFormat(int format);
#else // PI_WEBFONT_OPAQUE
  #ifdef VEGA_GDIFONT_SUPPORT
	virtual VEGAFont* GetVegaFont(UINT32 fontnr, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline);
  #else
	virtual OpFont* CreateFont(UINT32 fontnr, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline) ;
  #endif
	virtual OP_STATUS AddWebFont(UINT32 fontnr, const uni_char* full_path_of_file);
	virtual OP_STATUS RemoveWebFont(UINT32 fontnr);
#endif // PI_WEBFONT_OPAQUE

	virtual UINT32 CountFonts() { return m_num_fonts; }

	virtual OP_STATUS GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo);

	//Common implementation to GetFontInfo and GetWebFontInfo
	OP_STATUS GetFontInfo(LOGFONT* font, FONTSIGNATURE* signature, OpFontInfo *fontinfo);

	BOOL HasWeight(UINT32 fontnr, UINT8 weight);

	virtual OP_STATUS BeginEnumeration();

	virtual OP_STATUS EndEnumeration();

	virtual void BeforeStyleInit(class StyleManager* styl_man) { }

	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script);

	static OP_STATUS InitGenericFont(OpString_list& serif, 
									OpString_list& sansserif, 
									OpString_list& cursive, 
									OpString_list& fantasy, 
									OpString_list& monospace);
	static uni_char* GetLatinGenericFont(GenericFont generic_font);

#ifdef _GLYPHTESTING_SUPPORT_
    void UpdateGlyphMask(OpFontInfo *fontinfo);
#endif

	ProcessedGlyph* GetGlyphBuffer(size_t size);

	struct WebFontInfo
	{
		WebFontInfo() : logfont(NULL), signature(NULL) {}

		~WebFontInfo()
		{
			if (memory_font)
			{
				delete logfont;
				delete signature;
			}
		}

		OpString face;
		HANDLE memory_font;
		LOGFONT* logfont;
		FONTSIGNATURE* signature;
	};

	static INT CALLBACK CountAllFacesEx(LOGFONT FAR* lf, NEWTEXTMETRICEX FAR* lpntm, INT FontType, LPARAM lpData);
	static INT CALLBACK EnumAllFacesEx(LOGFONT FAR* lf, NEWTEXTMETRICEX FAR* lpntm, INT FontType, LPARAM lpData);

private:

	FONTSIGNATURE** m_signatures; //We are storing info about unicodeblocks here.
	UINT32 m_num_fonts;
	LOGFONT** m_log_fonts; //We are storing all fonts here, to be able to directly return
						   //info about a font (in GetFontInfo).

	ULONG_PTR m_gdiplus_token;
#ifndef PI_CAP_PLATFORM_WEBFONT_HANDLES
	OpWindowsPointerHashTable<UINT32, WebFontInfo *> m_webfont_faces;
#endif

	OpString_list m_serif_fonts;
	OpString_list m_sansserif_fonts;
	OpString_list m_cursive_fonts;
	OpString_list m_fantasy_fonts;
	OpString_list m_monospace_fonts;

	bool GetRawFontData(UINT32 font_nr, BYTE *& raw_data, UINT32& data_length);
	OP_STATUS AddRawFontData(UINT32 font_nr, BYTE* raw_data, UINT32 data_length);
	bool HasRawFontData() { return m_fonts_raw_data.GetCount() > 0; }
	void BuildRawFontDataCache(HDC& fonthdc);

	OpAutoVector<WindowsRawFontData>	m_fonts_raw_data;		// Maps a webfont name to an offset into the logfonts array

	ProcessedGlyph* m_glyph_buffer;
	size_t m_glyph_buffer_size;

	static bool s_is_loaded;
};
#endif // WINDOWS_FONT_MANAGER_H
