/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/GdiOpFont.h"
#include "platforms/windows/pi/GdiOpFontManager.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/WindowsVegaPrinterListener.h"
#include "platforms/windows/WindowsComplexScript.h"

#include "modules/display/check_sfnt.h"
#include "modules/display/sfnt_base.h"
#include "modules/style/css_webfont.h"
#include "modules/libgogi/mde_mmap.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/mdefont/processedstring.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

class WindowsMDE_MMapping : public MDE_MMapping
{
public:
	WindowsMDE_MMapping() : address(NULL), size(0)
	{}
	virtual ~WindowsMDE_MMapping()
	{
		UnmapViewOfFile(address);
	}

	OP_STATUS MapFile(const uni_char* file)
	{
		HANDLE fd = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL, 
								OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(fd == INVALID_HANDLE_VALUE)
		{
			return OpStatus::ERR;
		}
		size = GetFileSize(fd, NULL);

		HANDLE mapping = CreateFileMapping(fd, NULL, PAGE_READONLY, 0, 0, NULL);

		CloseHandle(fd);
 
		if (!mapping)
		{
			return OpStatus::ERR;
		}
 
		address = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
		CloseHandle(mapping);
 
		if(!address)
		{
			return OpStatus::ERR;
		}
		return OpStatus::OK;
	}
 
	virtual void* GetAddress(){return address;}
	virtual int GetSize(){return size;}
private:
	void* address;
	int size;
};

OP_STATUS MDE_MMapping::Create(MDE_MMapping** m, const uni_char* file)
{
	WindowsMDE_MMapping* wm = OP_NEW(WindowsMDE_MMapping, ());
	if (!wm)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = wm->MapFile(file);
	if (OpStatus::IsError(err))
	{
		OP_DELETE(wm);
		wm = NULL;
	}
	*m = wm;
	return err;
}


#include "platforms/windows/pi/GdiOpFont.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"

#ifdef SUPPORT_TEXT_DIRECTION
#include "platforms/windows/WindowsComplexScript.h"
#endif

#ifdef VEGA_PRINTER_LISTENER_SUPPORT
#include "platforms/windows/WindowsVegaPrinterListener.h"
#endif

#include "modules/libvega/src/oppainter/vegaoppainter.h"

#include "modules\display\check_sfnt.h"
#include "modules\style\css_webfont.h"
#include "modules\display\sfnt_base.h"

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_path.h"
# include "modules/svg/svg_number.h"
#endif // SVG_SUPPORT


#define DTT_GLOWSIZE        (1UL << 11)
#define DTT_COMPOSITED      (1UL << 13)

extern HTHEME g_window_theme_handle;

static HDC fontdb_hdc = NULL;


//#define FONTCACHETEST
#ifdef FONTCACHETEST

static Head fontcache;

class FontCacheTest : public Link
{
public:
	FontCacheTest(uni_char *face,UINT32 size,UINT8 weight,BOOL italic)
	{
		this->face = NULL;
		uni_SetStr(this->face, face);
		this->size = size;
		this->weight = weight;
		this->italic = italic;
		this->use_count = 1;
	}

	uni_char* face;
	UINT32 size;
	UINT8 weight;
	BOOL italic;
	UINT32 use_count;

	BOOL Equals(FontCacheTest &other)
	{
		return (size == other.size &&
			weight == other.weight &&
			italic == other.italic &&
			uni_strcmp(face, other.face) == 0);
	}

	void IncUseCount()
	{
		++use_count;
	}

};

static int FontCacheCompare(const void *arg1, const void *arg2)
{
	FontCacheTest* font1 = *((FontCacheTest**)arg1);
	FontCacheTest* font2 = *((FontCacheTest**)arg2);

	if (font1->use_count > font2->use_count)
		return -1;
	if (font2->use_count > font1->use_count)
		return 1;
	int res = uni_strcmp(font1->face, font2->face);
	if (res != 0)
		return res;
	if (font1->weight < font2->weight)
		return -1;
	if (font2->weight < font1->weight)
		return 1;
	if (font1->italic && !font2->italic)
		return 1;
	return -1;
}

#endif // FONTCACHETEST

struct FontAlias{
	const uni_char* face;
	const uni_char* alias;
};

const FontAlias font_alias_list[] = {
	{UNI_L("SimSun"),	UNI_L("\x5B8B\x4F53")},
	{UNI_L("NSimSun"),	UNI_L("\x65B0\x5B8B\x4F53")},
	{UNI_L("Microsoft YaHei"),	UNI_L("\x5FAE\x8F6F\x96C5\x9ED1")},
	{UNI_L("PMingLiU"),	UNI_L("\x65B0\x7D30\x660E\x9AD4")},
	{UNI_L("Microsoft JhengHei"), UNI_L("\x5FAE\x8EDF\x6B63\x9ED1\x9AD4")},
	{UNI_L("MingLiU"),	UNI_L("\x7D30\x660E\x9AD4")},
	{UNI_L("MS PMincho"),UNI_L("\xFF2D\xFF33\x0020\xFF30\x660E\x671D")},
	{UNI_L("MS PGothic"),UNI_L("\xFF2D\xFF33\x0020\xFF30\x30B4\x30B7\x30C3\x30AF")},
	{UNI_L("MS Gothic"),UNI_L("\xFF2D\xFF33\x0020\x30B4\x30B7\x30C3\x30AF")},
	{UNI_L("Batang"),	UNI_L("\xBC14\xD0D5")},
	{UNI_L("BatangChe"),UNI_L("\xBC14\xD0D5\xCCB4")},
	{UNI_L("Gulim"),	UNI_L("\xAD74\xB9BC")},
	{UNI_L("GulimChe"),	UNI_L("\xAD74\xB9BC\xCCB4")}
	// Gautami && Vrinda
};

// Use English name for these lists and add a item in the above alias list
const uni_char* Chinese_simplified_serif[]		= { UNI_L("SimSun"), NULL };
const uni_char* Chinese_simplified_sansserif[]	= { UNI_L("SimSun"), NULL };
const uni_char* Chinese_simplified_monospace[]	= { UNI_L("NSimSun"), NULL };

const uni_char* Chinese_traditional_serif[]		= { UNI_L("PMingLiU"), NULL };
const uni_char* Chinese_traditional_sansserif[] = { UNI_L("PMingLiU"), NULL };
const uni_char* Chinese_traditional_monospace[]	= { UNI_L("MingLiU"), NULL };

const uni_char* Japanese_serif[]				= { UNI_L("MS PMincho"), NULL };
const uni_char* Japanese_sansserif[]			= { UNI_L("MS PGothic"), NULL };
const uni_char* Japanese_monospace[]			= { UNI_L("MS Gothic"), NULL };
 
const uni_char* Korean_serif[]					= { UNI_L("Batang"), NULL };
const uni_char* Korean_sansserif[]				= { UNI_L("Gulim"), NULL };
const uni_char* Korean_monospace[]				= { UNI_L("BatangChe"), NULL };
 
const uni_char* IndicTelugu_serif[]				= { UNI_L("Gautami"), NULL };
const uni_char* IndicTelugu_sansserif[]			= { UNI_L("Gautami"), NULL };
const uni_char* IndicTelugu_monospace[]			= { UNI_L("Gautami"), NULL };
 
const uni_char* IndicBengali_sansserif[]		= { UNI_L("Vrinda"), NULL };

const uni_char* Arabic_monospace[]				= { UNI_L("Simplified Arabic Fixed"), NULL };

struct ScriptFontsInitializationInfo
{
	WritingSystem::Script script;
	GenericFont genericFont;
	const uni_char** fontsList;
} 
scripts_fonts_initialization_list[] = 
{
	{WritingSystem::ChineseSimplified,	GENERIC_FONT_SERIF,		Chinese_simplified_serif},
	{WritingSystem::ChineseSimplified,	GENERIC_FONT_SANSSERIF, Chinese_simplified_sansserif},
	{WritingSystem::ChineseSimplified,	GENERIC_FONT_MONOSPACE, Chinese_simplified_monospace},

	{WritingSystem::ChineseTraditional, GENERIC_FONT_SERIF,		Chinese_traditional_serif},
	{WritingSystem::ChineseTraditional, GENERIC_FONT_SANSSERIF, Chinese_traditional_sansserif},
	{WritingSystem::ChineseTraditional, GENERIC_FONT_MONOSPACE, Chinese_traditional_monospace},

	{WritingSystem::Japanese,			GENERIC_FONT_SERIF,		Japanese_serif},
	{WritingSystem::Japanese,			GENERIC_FONT_SANSSERIF, Japanese_sansserif},
	{WritingSystem::Japanese,			GENERIC_FONT_MONOSPACE, Japanese_monospace},

	{WritingSystem::Korean,				GENERIC_FONT_SERIF,		Korean_serif},
	{WritingSystem::Korean,				GENERIC_FONT_SANSSERIF, Korean_sansserif},
	{WritingSystem::Korean,				GENERIC_FONT_MONOSPACE, Korean_monospace},
	
	{WritingSystem::IndicTelugu,		GENERIC_FONT_SERIF,		IndicTelugu_serif},
	{WritingSystem::IndicTelugu,		GENERIC_FONT_SANSSERIF, IndicTelugu_sansserif},
	{WritingSystem::IndicTelugu,		GENERIC_FONT_MONOSPACE, IndicTelugu_monospace},

	{WritingSystem::IndicBengali,		GENERIC_FONT_SANSSERIF, IndicBengali_sansserif},

	{WritingSystem::Arabic,				GENERIC_FONT_MONOSPACE, Arabic_monospace}
};

const uni_char* GetEnglishNameForFont(const uni_char* font)
{
	if (!font)
		return NULL;

	for (INT32 i=0; i<ARRAY_SIZE(font_alias_list); i++)
	{
		if (uni_stricmp(font_alias_list[i].alias, font) == 0)
			return font_alias_list[i].face;
	}
	return font;
}

const uni_char* GetFirstAvailableFont(const uni_char** fonts_list, WritingSystem::Script script)
{
	const uni_char* name = NULL;
	for(int i=0;fonts_list[i];i++)
	{
		HFONT font = ::CreateFont(0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, fonts_list[i]);
		if (font)
		{
			::DeleteObject(font);
			name = fonts_list[i];
			break;
		}
	}
	
	// Convert to localized name for fonts that are used for current system
	// locale, as Core only accept one name for each font and for the system 
	// locale that is the localized one(We got that name from Windows API)
	if (name && script == WritingSystem::FromEncoding(g_op_system_info->GetSystemEncodingL()))
	{
		for(INT32 i=0; i<ARRAY_SIZE(font_alias_list); i++)
		{
			if ( uni_stricmp(font_alias_list[i].face, name) == 0)
			{
				name = font_alias_list[i].alias;
				break;
			}
		}
	}
	return name;
}





//
// class GdiOpFontManager
//

INT CALLBACK GdiOpFontManager::CountAllFacesEx(LOGFONT FAR* lf, NEWTEXTMETRICEX FAR* lpntm, INT FontType, LPARAM lpData)
{
	UINT32* num_fonts = (UINT32*)lpData;
	(*num_fonts)++;
	return 1;
}

INT CALLBACK GdiOpFontManager::EnumAllFacesEx(LOGFONT FAR* lf, NEWTEXTMETRICEX FAR* lpntm, INT FontType, LPARAM lpData)
{
	GdiOpFontManager *fm = (GdiOpFontManager *)lpData;
	UINT32 num_fonts = fm->CountFonts();

	// Add font to font database if this is the first font or if the font face is different from the last font.
	BOOL add_font = (num_fonts == 0 || uni_strcmp(fm->m_log_fonts[num_fonts - 1]->lfFaceName, lf->lfFaceName) != 0);

	if (add_font)
	{
		*fm->m_log_fonts[fm->m_num_fonts] = *lf;
		*fm->m_signatures[fm->m_num_fonts] = lpntm->ntmFontSig;

		fm->m_num_fonts++;
	}
	else
	{
		// Add the font signature to the last added font.
		FONTSIGNATURE fs = *fm->m_signatures[fm->m_num_fonts - 1];
		int i;
		for(i=0; i < 4; i++)
			fs.fsUsb[i] |= lpntm->ntmFontSig.fsUsb[i];
		for(i=0; i < 1; i++)
			fs.fsCsb[i] |= lpntm->ntmFontSig.fsCsb[i];		
	}
	
	return 1;
}

#if defined(VEGA_BACKEND_DIRECT3D10) && defined(VEGA_BACKEND_D2D_INTEROPERABILITY)
#include "DWriteOpFont.h"
#endif
OP_STATUS OpFontManager::Create(OpFontManager** new_fontmanager)
{
	OP_ASSERT(new_fontmanager != NULL);
	OP_ASSERT(g_pccore && "Not all of the prefs module has to be initialized to continue, but at least g_pccore.");

	//
	//  First we try to initialize the DirectWrite font manager. But we know for sure it will only work if:
	//    - The machine is running Windows Vista with SP2 + the Vista Platform update or newer Windows version.
	//    - The EnableHardwareAcceleration pref is not disabled.
	//    - The card and driver version can initialize a DirectX 10 or a DirectX 10 Level 9.x device.
	//    - The card or driver is not blocked by the block-list.
	//

	*new_fontmanager = NULL;

#if defined(VEGA_BACKEND_DIRECT3D10) && defined(VEGA_BACKEND_D2D_INTEROPERABILITY)

	if (IsSystemWinVista() && g_pccore->GetIntegerPref(PrefsCollectionCore::EnableHardwareAcceleration) != PrefsCollectionCore::Disable)
	{
		// If we are running vista or later, and the HW pref is not disabled, then the easiest way to test the rest of the requirements
		// is to just try to create the device, and see if it initializes without failing or getting blocked.
		VEGARenderer rend;

		if (OpStatus::IsSuccess(rend.Init(0, 0))
			&& g_vegaGlobals.rasterBackend == LibvegaModule::BACKEND_HW3D
			&& g_vegaGlobals.vega3dDevice->getDeviceType() == PrefsCollectionCore::DirectX10)
		{

			DWriteOpFontManager* dw_fontmanager = OP_NEW(DWriteOpFontManager, ());
			RETURN_OOM_IF_NULL(dw_fontmanager);

			if (OpStatus::IsError(dw_fontmanager->Construct()))
			{
				OP_DELETE(dw_fontmanager);
			}
			else
			{
				*new_fontmanager = dw_fontmanager;
				return OpStatus::OK;
			}
		}
	}

#endif // defined(VEGA_BACKEND_DIRECT3D10) && defined(VEGA_BACKEND_D2D_INTEROPERABILITY)

	//
	// If we can't get the DX10 back-end to work, or for some reason DirectWrite won't initialize. Then the back-up is to
	// create the GdiOpFontManager. It can be used in two ways:
	//   - As earlier, it can be used for native rendering with the software back-end.
	//   - Or with ProcessedStrings togheter with one of the 3D back-ends (DirectX 10 or OpenGL).
	//

	*new_fontmanager = OP_NEW(GdiOpFontManager, ());
	RETURN_OOM_IF_NULL(*new_fontmanager);

	return OpStatus::OK;
}

/*static*/
bool GdiOpFontManager::s_is_loaded = false;
GdiOpFontManager::GdiOpFontManager()
#ifdef VEGA_GDIFONT_SUPPORT
	: VEGAOpFontManager(), 
#else
	: OpFontManager(), 
#endif
	m_num_fonts(0), m_log_fonts(NULL), m_signatures(NULL), m_glyph_buffer(NULL), m_glyph_buffer_size(0)
{
	fontdb_hdc = ::GetDC(NULL);
	// INT hInst = 0;

	GdiOpFont::s_fonthdc = CreateCompatibleDC(NULL);

	LPLOGFONT lf = OP_NEW(LOGFONT, ());
	memset(lf, 0, sizeof(LOGFONT));
	lf->lfCharSet = DEFAULT_CHARSET;

	// Count all fonts...
	UINT32 num_fonts = 0;
	::EnumFontFamiliesEx(fontdb_hdc, lf, (FONTENUMPROC)CountAllFacesEx, (LPARAM)&num_fonts, 0);
	m_num_fonts = num_fonts;

	// Alloc memory for all the LOGFONT pointers...
	UINT32 i = 0;
	m_log_fonts = OP_NEWA(LOGFONT*, (m_num_fonts));
	for (i = 0; i < m_num_fonts; i++)
		m_log_fonts[i] = OP_NEW(LOGFONT, ());

	// Alloc memory for all the FONTSIGNATURE pointers...
	m_signatures = OP_NEWA(FONTSIGNATURE*, (m_num_fonts));
	for (i = 0; i < m_num_fonts; i++)
	{
		m_signatures[i] = OP_NEW(FONTSIGNATURE, ());
		memset(m_signatures[i], 0, sizeof(FONTSIGNATURE));
	}
	UINT32 numfonts_guess = m_num_fonts;
	m_num_fonts = 0;

	// Enum the fonts...
	memset(lf, 0, sizeof(LOGFONT));
	lf->lfCharSet = DEFAULT_CHARSET;

#ifdef _DEBUG
Debug::TimeStart("fonts", "constructor: begin enumerating fonts");
#endif
	::EnumFontFamiliesEx(fontdb_hdc, lf, (FONTENUMPROC)EnumAllFacesEx, (LPARAM)this, 0);

#ifdef _DEBUG
Debug::TimeEnd("fonts", "constructor: end enumerating fonts");
#endif

	// Make sure we delete the unused slots
	for(i = m_num_fonts; i < numfonts_guess; ++i)
	{
		OP_DELETE(m_log_fonts[i]);
		OP_DELETE(m_signatures[i]);
	}
	
	OP_DELETE(lf);
	::ReleaseDC(NULL, fontdb_hdc);

	s_is_loaded = true;
}

GdiOpFontManager::~GdiOpFontManager()
{
#ifdef FONTCACHETEST
	
	FontCacheTest* font = (FontCacheTest*)fontcache.First();
	UINT32 size = 0;
	while (font)
	{
		font = (FontCacheTest*)font->Suc();
		size++;
	}
	FontCacheTest** list = OP_NEWA(FontCacheTest*, (size));
	font = (FontCacheTest*)fontcache.First();	
	int j=0;
	while (font)
	{
		list[j] = font;
		font = (FontCacheTest*)font->Suc();		
		++j;
	}

	qsort(list, size, sizeof(FontCacheTest*), FontCacheCompare);
	uni_char buf[200];
	for(j=0; j < size; j++)
	{
		font = list[j];
		swprintf(buf, UNI_L("%d: %s (%d, %d, %s)\n"), font->use_count, font->face, font->size, font->weight, (font->italic ? UNI_L("italic") : UNI_L("non italic")));
		OutputDebugStringW(buf);		
	}


#endif // FONTCACHETEST

	UINT32 i = 0;
	for(i = 0; i < m_num_fonts; i++) //free every logfont
		OP_DELETE(m_log_fonts[i]);
	OP_DELETEA(m_log_fonts);		//free the pointervector

	for(i = 0;i < m_num_fonts; i++) //free every signature
		OP_DELETE(m_signatures[i]);
	OP_DELETEA(m_signatures);	//free the pointervector

	// NOTE: Delete the static fonthdc for here
	SelectObject(GdiOpFont::s_fonthdc, (HFONT)GetStockObject(SYSTEM_FONT));
	DeleteDC(GdiOpFont::s_fonthdc);

	OP_DELETEA(m_glyph_buffer);

	// Delete the shared static bitmap and DC used in GdiOpFont::GetAlphaMask.
	if (GdiOpFont::s_text_bitmap_dc)
	{
		SelectObject(GdiOpFont::s_text_bitmap_dc, GdiOpFont::s_old_text_bitmap);
		DeleteDC(GdiOpFont::s_text_bitmap_dc);
	}

	GdiOpFont::s_text_bitmap_dc = NULL;

	if (GdiOpFont::s_text_bitmap)
		DeleteObject(GdiOpFont::s_text_bitmap);

	GdiOpFont::s_text_bitmap = NULL;
}

#ifdef VEGA_GDIFONT_SUPPORT
VEGAFont* GdiOpFontManager::GetVegaFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius)
{
	OP_ASSERT(face);
	int fontnr = styleManager->GetFontNumber(face);
	if (fontnr < 0)
		return NULL;

	BYTE pitch = DEFAULT_PITCH;

	GdiOpFont* new_font = NULL;

	if ((UINT)fontnr < m_num_fonts)
	{
		new_font = OP_NEW(GdiOpFont, (this, m_log_fonts[fontnr]->lfFaceName, size, weight, italic, blur_radius));
		pitch = m_log_fonts[fontnr]->lfPitchAndFamily & 0x03;
	}

	// logfonts[fontnr]->lfFaceName,size,weight,italic
	if(!new_font || OpStatus::IsError(new_font->GetStatus()))
	{
		OP_DELETE(new_font);
		return NULL;
	}

	if (must_have_getoutline && !new_font->IsVectorFont())
	{
		BOOL monospace_font = (pitch==FIXED_PITCH);
				
		OP_DELETE(new_font);

		uni_char* font_name = monospace_font ? GetLatinGenericFont(GENERIC_FONT_MONOSPACE) : GetLatinGenericFont(GENERIC_FONT_SERIF);
		new_font = OP_NEW(GdiOpFont, (this, font_name, size, weight, italic, blur_radius));
		if(new_font && (OpStatus::IsError(new_font->GetStatus()) || !new_font->IsVectorFont()))
		{
			OP_ASSERT(!"Select a font capable of GetOutline, otherwise text will be missing in svg.");
			OP_DELETE(new_font);
			return NULL;
		}
	}

	return new_font;	
}
#else
OpFont* GdiOpFontManager::CreateFont(const uni_char *face,UINT32 size,UINT8 weight,BOOL italic, 
										 BOOL must_have_getoutline)
{
	GdiOpFont* new_font = OP_NEW(GdiOpFont, (face,size,weight,italic));
	if(new_font)
	{
		if(OpStatus::IsError(new_font->m_status) ||
			(must_have_getoutline && !new_font->IsVectorFont()))
		{
			BOOL monospace_font = FALSE;
			for(UINT32 i = 0; i < numfonts; i++)
			{
				if(uni_strcmp(logfonts[i]->lfFaceName, face) == 0)
				{
					BYTE pitch = logfonts[i]->lfPitchAndFamily & 0x03;
					//BYTE font_fam = (font->lfPitchAndFamily & 0xf0);
					if(pitch==FIXED_PITCH)
						monospace_font = TRUE;
					
					break;
				}
			}

			OP_DELETE(new_font);

			if(must_have_getoutline)
			{
				uni_char* font_name = monospace_font ? monospacefont.CStr() : seriffont.CStr();
				new_font = OP_NEW(GdiOpFont, (font_name, size, weight, italic));
				if(new_font && (OpStatus::IsError(new_font->m_status) || !new_font->IsVectorFont()))
				{
					OP_ASSERT(!"Select a font capable of GetOutline, otherwise text will be missing in svg.");
					OP_DELETE(new_font);
					return NULL;
				}
				return new_font;	
			}
			return NULL;
		}
	}
	return new_font;
}
#endif

#ifdef PI_WEBFONT_OPAQUE
  #ifdef VEGA_GDIFONT_SUPPORT
VEGAFont* GdiOpFontManager::GetVegaFont(OpFontManager::OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius)
{
	GdiOpFont* new_font;

	WebFontInfo* webfont_info = (WebFontInfo*)webfont;

	new_font = OP_NEW(GdiOpFont, (this, webfont_info->face,size, weight, italic, blur_radius));

	if(!new_font || OpStatus::IsError(new_font->GetStatus()))
	{
		OP_DELETE(new_font);
		new_font = NULL;
		return NULL;
	}

	return new_font;	
}

VEGAFont* GdiOpFontManager::GetVegaFont(OpFontManager::OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	GdiOpFont* new_font;

	WebFontInfo* webfont_info = (WebFontInfo*)webfont;

	new_font = OP_NEW(GdiOpFont, (this, webfont_info->face, size, 4, FALSE, blur_radius));

	if(!new_font || OpStatus::IsError(new_font->m_status))
	{
		OP_DELETE(new_font);
		new_font = NULL;
		return NULL;
	}

	return new_font;	
}

  #else

OpFont* GdiOpFontManager::CreateFont(OpWebFontRef webfont, UINT32 size)
{
	GdiOpFont* new_font;

	WebFontInfo* webfont_info = (WebFontInfo*)webfont;

	new_font = OP_NEW(GdiOpFont, (webfont_info->face,size,0,0));

	if(!new_font || OpStatus::IsError(new_font->status))
	{
		OP_DELETE(new_font);
		new_font = NULL;
		return NULL;
	}

	return new_font;	

}
  #endif
OP_STATUS GdiOpFontManager::AddWebFont(OpWebFontRef& webfont, const uni_char* path)
{
	WebFontInfo* webfont_info = OP_NEW(WebFontInfo, ());

	OpString face;
	//This can survive up to the times of 1024-bits architectures...
	if (!face.Reserve(320))
	{
		OP_DELETE(webfont_info);
		return OpStatus::ERR_NO_MEMORY;
	}
	uni_itoa(INTPTR(webfont_info), face.CStr(), 10);
	face.Insert(0, "font_");

	const UINT8* renamed_font;
	size_t renamed_font_size;
	if (OpStatus::IsError(RenameFont(path, face.CStr(), renamed_font, renamed_font_size, NULL)))
	{
		OP_DELETE(webfont_info);
		return OpStatus::ERR;
	}

	HANDLE memory_font = 0;
	DWORD pcfonts = 0;
	if ((memory_font = AddFontMemResourceEx((PVOID)renamed_font, renamed_font_size, 0, &pcfonts)) == 0)
	{
		OP_DELETEA(renamed_font);
		return OpStatus::ERR;
	}
	else
		OP_DELETEA(renamed_font);

	webfont_info->face.Set(face);
	webfont_info->memory_font = memory_font;
	webfont_info->logfont = NULL;
	webfont_info->signature = NULL;

	fontdb_hdc = ::GetDC(NULL);
	if(fontdb_hdc)
	{
		HFONT font = ::CreateFont(0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
		if(font)
		{
			HGDIOBJ old_font = SelectObject(fontdb_hdc, font);
			FONTSIGNATURE fntsig;
			GetTextCharsetInfo(fontdb_hdc, &fntsig, 0);
			webfont_info->signature = OP_NEW(FONTSIGNATURE, (fntsig));
			SelectObject(fontdb_hdc, old_font);
			::DeleteObject(font);
		}
		::ReleaseDC(NULL, fontdb_hdc);
	}

	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	uni_strncpy(lf.lfFaceName, face, LF_FACESIZE);
	webfont_info->logfont = OP_NEW(LOGFONT, (lf));

	//::EnumFontFamiliesEx(fontdb_hdc, &lf, (FONTENUMPROC)GetWebFontInfo, (LPARAM)webfont_info, 0);

	if (!webfont_info->logfont || !webfont_info->signature)
	{
		// enumeration failed somehow, or OOM
		OP_DELETE(webfont_info);
		return OpStatus::ERR;
	}

	webfont = (INTPTR)webfont_info;

	return OpStatus::OK;
}

OP_STATUS GdiOpFontManager::RemoveWebFont(OpWebFontRef webfont)
{
	if (!webfont)
		return OpStatus::ERR;

	WebFontInfo* webfont_info = (WebFontInfo*)webfont;
	
	BOOL res = TRUE;
	if (webfont_info->memory_font)
		res = RemoveFontMemResourceEx(webfont_info->memory_font);

	OP_DELETE(webfont_info);

	if (!res)
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}
OP_STATUS GdiOpFontManager::GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo)
{
	LOGFONT* font;
	FONTSIGNATURE* signature;

	WebFontInfo* webfont_info = (WebFontInfo*)webfont;
	font = webfont_info->logfont;
	signature = webfont_info->signature;

	fontinfo->SetWebFont(webfont);

	return GetFontInfo(font, signature, fontinfo);
}

void GdiOpFontManager::BuildRawFontDataCache(HDC& fonthdc)
{
	// build the raw font data cache
	for (UINT32 i = 0; i < m_num_fonts; i++)
	{
		HFONT fnt = ::CreateFont(
			0,	// logical height of font 
			0,	// logical average character width 
			0,	// angle of escapement 
			0,	// base-line orientation angle 
			0,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,	// character set identifier 
			OUT_DEFAULT_PRECIS	,	// output precision 
			CLIP_DEFAULT_PRECIS	,	// clipping precision 
			DEFAULT_QUALITY	,	// output quality 
			FF_DONTCARE	,	// pitch and family 
			m_log_fonts[i]->lfFaceName 	// address of typeface name string 
			);

		if (fnt)
		{
			HGDIOBJ oldGdiObj = SelectObject(fonthdc, fnt);
			const char *tabname = "name";

			int sizReq = GetFontData(fonthdc,*(unsigned long *)tabname,0,NULL,0);
			if(sizReq != GDI_ERROR)
			{
				BYTE *raw_table = OP_NEWA(BYTE, sizReq);
				if(raw_table)
				{
					sizReq = GetFontData(fonthdc, *(unsigned long *)tabname, 0, raw_table, sizReq);
					if(sizReq != GDI_ERROR)
					{
						// raw_table now belongs to the table
						AddRawFontData(i, raw_table, sizReq);
					}
					else
					{
						// add dummy
						AddRawFontData(i, NULL, 0);
						OP_DELETEA(raw_table);		// delete the data again, we're not keeping it
					}
				}
			}
			else
			{
				// add dummy to maintain a 1-to-1 mapping
				AddRawFontData(i, NULL, 0);
			}
			SelectObject(fonthdc, oldGdiObj);
			DeleteObject(fnt);
		}
		else
		{
			// add dummy to maintain a 1-to-1 mapping
			AddRawFontData(i, NULL, 0);
		}
	}
}

OP_STATUS GdiOpFontManager::GetLocalFont(OpWebFontRef& localfont, const uni_char* facename)
{
	localfont = NULL;
	HDC fonthdc = CreateCompatibleDC(NULL);
	if (!fonthdc)
		return OpStatus::ERR_NO_MEMORY;

	if(!HasRawFontData())
	{
		// errors in this is not critical
		BuildRawFontDataCache(fonthdc);
	}
	const size_t namelen = uni_strlen(facename);
	uni_char* facenameBE = OP_NEWA(uni_char, namelen);
	if (!facenameBE)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<uni_char> _f(facenameBE);
	op_memcpy(facenameBE, facename, namelen * sizeof(uni_char));
	SwitchEndian((UINT8*)facenameBE, namelen * sizeof(uni_char));

	for (UINT32 i = 0; i < m_num_fonts; i++)
	{
		BOOL is_match = FALSE;
		UINT32 raw_length = 0;
		BYTE *raw_table = NULL;

		if(GetRawFontData(i, raw_table, raw_length))
		{
			is_match = raw_table != NULL && MatchFace(raw_table, raw_length, facenameBE, namelen);
		}
		else
		{
			HFONT fnt = ::CreateFont(
				0,	// logical height of font 
				0,	// logical average character width 
				0,	// angle of escapement 
				0,	// base-line orientation angle 
				0,FALSE,FALSE,FALSE,
				DEFAULT_CHARSET,	// character set identifier 
				OUT_DEFAULT_PRECIS	,	// output precision 
				CLIP_DEFAULT_PRECIS	,	// clipping precision 
				DEFAULT_QUALITY	,	// output quality 
				FF_DONTCARE	,	// pitch and family 
				m_log_fonts[i]->lfFaceName 	// address of typeface name string
				);

			if (fnt)
			{
				HGDIOBJ oldGdiObj = SelectObject(fonthdc, fnt);
				const char *tabname = "name";

				int sizReq = GetFontData(fonthdc,*(unsigned long *)tabname,0,NULL,0);

				if(sizReq != GDI_ERROR)
				{
					OP_ASSERT(sizReq < 1024*64 && "Table is too large, we should find another method");

					raw_table = OP_NEWA(BYTE, sizReq);

					if(raw_table)
					{
						//					OutputDebugStr(fontinfo->GetFace());
						//					OutputDebugStr(UNI_L(" (UpdateGlyphMask called)\n"));

						sizReq = GetFontData(fonthdc,*(unsigned long *)tabname,0,raw_table,sizReq);
						is_match = sizReq != GDI_ERROR && MatchFace(raw_table, sizReq, facenameBE, namelen);

						OP_DELETEA(raw_table);
					}
				}

				SelectObject(fonthdc, oldGdiObj);
				DeleteObject(fnt);
			}
		}
		if(is_match)
		{
			WebFontInfo* webfont_info = OP_NEW(WebFontInfo, ());

			if (webfont_info == NULL)
			{
				DeleteDC(fonthdc);
				return OpStatus::ERR_NO_MEMORY;
			}
			webfont_info->face.Set(m_log_fonts[i]->lfFaceName);
			webfont_info->memory_font = 0;
			webfont_info->logfont = m_log_fonts[i];
			webfont_info->signature = m_signatures[i];

			localfont = (INTPTR)webfont_info;

			break;
		}
	}
	DeleteDC(fonthdc);

	return OpStatus::OK;
}

BOOL GdiOpFontManager::SupportsFormat(int format)
{
	int supported = CSS_WebFont::FORMAT_TRUETYPE | CSS_WebFont::FORMAT_OPENTYPE;

	return (format &~ supported) == 0;
}

#else

  #ifdef VEGA_GDIFONT_SUPPORT
VEGAFont* GdiOpFontManager::GetVegaFont(UINT32 fontnr, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline)
{
	BYTE pitch;
	GdiOpFont* new_font;

	if (fontnr < numfonts)
	{
		new_font = OP_NEW(GdiOpFont, (logfonts[fontnr]->lfFaceName, size, weight, italic));
		pitch = logfonts[fontnr]->lfPitchAndFamily & 0x03;
	}
	else
	{
		WebFontInfo* webfont_info;
		if (OpStatus::IsError(m_webfont_faces.GetData(fontnr, &webfont_info)))
			return NULL;

		new_font = OP_NEW(GdiOpFont, (webfont_info->face,size,weight,italic));
		pitch = webfont_info->logfont->lfPitchAndFamily & 0x03;
	}

	// logfonts[fontnr]->lfFaceName,size,weight,italic
	if(!new_font || OpStatus::IsError(new_font->m_status))
	{
		OP_DELETE(new_font);
		return NULL;
	}

	if (must_have_getoutline && !new_font->IsVectorFont())
	{
		BOOL monospace_font = (pitch==FIXED_PITCH);
				
		OP_DELETE(ew_font);

		uni_char* font_name = monospace_font ? monospacefont.CStr() : seriffont.CStr();
		new_font = OP_NEW(GdiOpFont, (font_name, size, weight, italic));
		if(new_font && (OpStatus::IsError(new_font->m_status) || !new_font->IsVectorFont()))
		{
			OP_ASSERT(!"Select a font capable of GetOutline, otherwise text will be missing in svg.");
			
			OP_DELETE(new_font);
			return NULL;
		}
	}

	return new_font;	
}
  #else
OpFont* GdiOpFontManager::CreateFont(UINT32 fontnr, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline) 
{
	BYTE pitch;
	GdiOpFont* new_font;

	if (fontnr < numfonts)
	{
		new_font = OP_NEW(GdiOpFont, (logfonts[fontnr]->lfFaceName,size,weight,italic));
		pitch = logfonts[fontnr]->lfPitchAndFamily & 0x03;
	}
	else
	{
		WebFontInfo* webfont_info;
		if (OpStatus::IsError(m_webfont_faces.GetData(fontnr, &webfont_info)))
			return NULL;

		new_font = OP_NEW(GdiOpFont, (webfont_info->face,size,weight,italic));
		pitch = webfont_info->logfont->lfPitchAndFamily & 0x03;
	}

	if(!new_font || OpStatus::IsError(new_font->m_status))
	{
		OP_DELETE(new_font);
		new_font = NULL;
		return NULL;
	}

	if (must_have_getoutline && !new_font->IsVectorFont())
	{
		BOOL monospace_font = (pitch==FIXED_PITCH);
				
		OP_DELETE(new_font);

		uni_char* font_name = monospace_font ? monospacefont.CStr() : seriffont.CStr();
		new_font = OP_NEW(GdiOpFont, (font_name, size, weight, italic));
		if(new_font && (OpStatus::IsError(new_font->m_status) || !new_font->IsVectorFont()))
		{
			OP_ASSERT(!"Select a font capable of GetOutline, otherwise text will be missing in svg.");
			
			OP_DELETE(new_font);
			return NULL;
		}
	}

	return new_font;	
}
  #endif


OP_STATUS GdiOpFontManager::AddWebFont(UINT32 fontnr, const uni_char* full_path_of_file)
{
	OpString face;
	if (!face.Reserve(15))
		return OpStatus::ERR_NO_MEMORY;
	uni_itoa(fontnr, face.CStr(), 10);
	face.Insert(0, "font_");

	const UINT8* renamed_font;
	size_t renamed_font_size;
	RETURN_IF_ERROR(RenameFont(full_path_of_file, face.CStr(), renamed_font, renamed_font_size, NULL));

	HANDLE memory_font = 0;
	DWORD pcfonts = 0;
	if ((memory_font = AddFontMemResourceEx((PVOID)renamed_font, renamed_font_size, 0, &pcfonts)) == 0)
	{
		OP_DELETEA(renamed_font);

		return OpStatus::ERR;
	}
	else
	{
		OP_DELETEA(renamed_font);
	}

	WebFontInfo* webfont_info = new WebFontInfo;
	webfont_info->face.Set(face);
	webfont_info->memory_font = memory_font;
	webfont_info->logfont = NULL;
	webfont_info->signature = NULL;

	fontdb_hdc = ::GetDC(NULL);
	HFONT font = ::CreateFont(0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
	HGDIOBJ old_font = SelectObject(fontdb_hdc, font);
	FONTSIGNATURE fntsig;
	GetTextCharsetInfo(fontdb_hdc, &fntsig, 0);
	webfont_info->signature = OP_NEW(FONTSIGNATURE, (fntsig));
	SelectObject(fontdb_hdc, old_font);
	::ReleaseDC(NULL, fontdb_hdc);

	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	uni_strncpy(lf.lfFaceName, face, LF_FACESIZE);
	webfont_info->logfont = OP_NEW(LOGFONT, (lf));

	if (!webfont_info->logfont || !webfont_info->signature)
	{
		// enumeration failed somehow, or OOM
		OP_DELETE(webfont_info);
		return OpStatus::ERR;
	}

	OP_STATUS err = m_webfont_faces.Add(fontnr, webfont_info);

	if (OpStatus::IsError(err))
		OP_DELETE(webfont_info);

	return err;
}

OP_STATUS GdiOpFontManager::RemoveWebFont(UINT32 fontnr)
{
	WebFontInfo* webfont_info;
	RETURN_IF_ERROR(m_webfont_faces.Remove(fontnr, &webfont_info));
	
	BOOL res = RemoveFontMemResourceEx(webfont_info->memory_font);

	OP_DELETE(webfont_info);

	if (!res)
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

#endif

#ifdef _GLYPHTESTING_SUPPORT_
void GdiOpFontManager::UpdateGlyphMask(OpFontInfo *fontinfo)
{
	HDC   fonthdc = CreateCompatibleDC(NULL);
	const uni_char* face;

#ifdef PI_WEBFONT_OPAQUE
	if (fontinfo->GetWebFont() != 0)
	{
		WebFontInfo* webfont_info = (WebFontInfo*)(fontinfo->GetWebFont());

		face = webfont_info->face.CStr();
	}
	else
		face = fontinfo->GetFace();
#else
	if (fontinfo->GetFontNumber() < numfonts)
	{
		face = fontinfo->GetFace();
	}
	else
	{
		WebFontInfo* webfont_info;
		if (OpStatus::IsError(m_webfont_faces.GetData(fontinfo->GetFontNumber(), &webfont_info)))
			return;

		face = webfont_info->face.CStr();

	}
#endif

	HFONT fnt = ::CreateFont(
			0,	// logical height of font 
			0,	// logical average character width 
			0,	// angle of escapement 
			0,	// base-line orientation angle 
			0,FALSE,FALSE,FALSE,
			DEFAULT_CHARSET,	// character set identifier 
			OUT_DEFAULT_PRECIS	,	// output precision 
			CLIP_DEFAULT_PRECIS	,	// clipping precision 
			DEFAULT_QUALITY	,	// output quality 
			FF_DONTCARE	,	// pitch and family 
			face 	// address of typeface name string 
		   );

	void * oldGdiObj = SelectObject(fonthdc, fnt);
	const char *tabname = "cmap";

    int sizReq = GetFontData(fonthdc,*(unsigned long *)tabname,0,NULL,0);

    if(sizReq != GDI_ERROR)
    {
		byte *raw_table = (byte *)malloc(sizReq);

		if(raw_table)
		{
//			OutputDebugStr(fontinfo->GetFace());
//			OutputDebugStr(UNI_L(" (UpdateGlyphMask called)\n"));

	        sizReq = GetFontData(fonthdc,*(unsigned long *)tabname,0,raw_table,sizReq);
			if(sizReq != GDI_ERROR)
			{
				byte *raw_table_end = raw_table + sizReq;

				CMapIndex * ci = (CMapIndex *)raw_table;

				int n = B2LENDIAN(ci->number);
				int i;

				CMapSubTable * st = (CMapSubTable *)(raw_table + 4);
				CMapGlyphTable * sub_table = NULL;

				for(i=0;i<n;i++)
				{
					if ((byte *)(st+1) > raw_table_end)
						break;

					if(B2LENDIAN(st->platformId) == TT_PLATFORM_WINDOWS)
					{
						sub_table = (CMapGlyphTable *)(raw_table + B2LENDIAN32(st->offset));
						break;
					}
					st++;
				}

				if(sub_table && (byte *)(sub_table+1) <= raw_table_end)
				{
					if(B2LENDIAN(sub_table->format) == 4)
					{
						uint16 segCountX2 = B2LENDIAN(sub_table->f4.segCountX2);

						uint16 * endCode         = (uint16 *)&(sub_table->f4.rest);
						uint16 * startCode       = (uint16 *)(&(sub_table->f4.rest) + segCountX2 +2);
						//uint16 * idDelta         = (uint16 *)(&(sub_table->f4.rest) + segCountX2*2 +2);
						//uint16 * idRangeOffset   = (uint16 *)(&(sub_table->f4.rest) + segCountX2*3 +2);
						//uint16 * glyphIndexArray = (uint16 *)(&(sub_table->f4.rest) + segCountX2*4 +2);
						//uint16 * glyphIndex      = NULL;
 
						int segment=0;

						while(1)
						{
							if ( (byte *)&startCode[segment+1] > raw_table_end)
								break;

							uint16 endc = B2LENDIAN(endCode[segment]), k;
							if(endc == 0xFFFF)
								endc--;

							for(k=B2LENDIAN(startCode[segment]); k<=endc; k++)  // Set a bit for all characters in segments
							{
								if (OpStatus::IsError(fontinfo->SetGlyph((uni_char)k)))
									break;
							}

							if(endc == 0xFFFE)
								break;

							segment++;
						}
					}
				}
			}
			free(raw_table);
		}
    }

	SelectObject(fonthdc, oldGdiObj);
	DeleteObject(fnt);
	DeleteDC(fonthdc);
}
#endif

ProcessedGlyph* GdiOpFontManager::GetGlyphBuffer(size_t size)
{
	if (size > m_glyph_buffer_size)
	{
		OP_DELETEA(m_glyph_buffer);
		m_glyph_buffer = OP_NEWA(ProcessedGlyph, size);
		m_glyph_buffer_size = m_glyph_buffer ? size : 0;
	}

	return m_glyph_buffer;
}

OP_STATUS GdiOpFontManager::GetFontInfo(UINT32 fontnr, OpFontInfo *fontinfo)
{
	LOGFONT* font;
	FONTSIGNATURE* signature;

	if (fontnr < (UINT32)m_num_fonts)
	{
		font = m_log_fonts[fontnr];
		signature = m_signatures[fontnr];
	}
	else
#ifndef PI_WEBFONT_OPAQUE
	{
		WebFontInfo* webfont_info;
		RETURN_IF_ERROR(OpStatus::IsError(m_webfont_faces.GetData(fontnr, &webfont_info)));

		font = webfont_info->logfont;
		signature = webfont_info->signature;
	}
#else // !PI_WEBFONT_OPAQUE
	{
		return OpStatus::ERR;
	}
#endif // !PI_WEBFONT_OPAQUE

	fontinfo->SetFontNumber(fontnr);

	return GetFontInfo(font, signature, fontinfo);
}

OP_STATUS GdiOpFontManager::GetFontInfo(LOGFONT* font, FONTSIGNATURE* signature, OpFontInfo *fontinfo)
{
	OP_NEW_DBG("GdiOpFontManager::", "Font.Enumeration");

	if (GdiOpFont::s_fonthdc == NULL) 
	{	
		GdiOpFont::s_fonthdc = CreateCompatibleDC(NULL);
		//RESOURCETRACKER_INC(RES_HDC);
	}

	OP_ASSERT(fontinfo != NULL);
	
	//
	fontinfo->SetFace(font->lfFaceName);

	OP_DBG((UNI_L("(%d): %s"), fontinfo->GetFontNumber() , font->lfFaceName));

	BYTE pitch = font->lfPitchAndFamily & 0x03;
	//BYTE font_fam = (font->lfPitchAndFamily & 0xf0);
	fontinfo->SetMonospace(pitch == FIXED_PITCH);
	fontinfo->SetItalic(TRUE);
	fontinfo->SetOblique(FALSE); //fix?
	fontinfo->SetNormal(TRUE);
	fontinfo->SetSmallcaps(FALSE); //fix?

	fontinfo->SetVertical(font->lfFaceName[0] == '@');

	switch(font->lfPitchAndFamily & 0xF0) 
	{
		case FF_ROMAN:
			fontinfo->SetSerifs(OpFontInfo::WithSerifs);
			break;
		case FF_MODERN:
			if (uni_stricmp(fontinfo->GetFace(), UNI_L("ms mincho")) == 0 ||
				uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\x660E\x671D")) == 0)
				fontinfo->SetSerifs(OpFontInfo::WithSerifs);
			else if (uni_stricmp(fontinfo->GetFace(), UNI_L("ms gothic")) == 0 ||
				uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\x30B4\x30B7\x30C3\x30AF")) == 0)
				fontinfo->SetSerifs(OpFontInfo::WithoutSerifs);
			else
				fontinfo->SetSerifs(OpFontInfo::UnknownSerifs);
			break;
		case FF_SWISS:
			fontinfo->SetSerifs(OpFontInfo::WithoutSerifs);
			break;
		default:
			fontinfo->SetSerifs(OpFontInfo::UnknownSerifs);
			break;
	};
	
	// We get the weight info for the other 9 weights when the information is needed.
	fontinfo->SetWeight(0, FALSE);

	//Find out wich unicode blocks the font has...
	INT i;
	for(i = 0; i < 128; i++)
	{
		fontinfo->SetBlock(i, FALSE);
	}
	for(i = 0; i < 126; i++)
	{
		if(signature->fsUsb[i / 32] & (1 << (i % 32)))
		{
			fontinfo->SetBlock(i, TRUE);
		}
	}
	// unless the font is encoded in OEM_CHARSET, if the font has no info at all,
	// ?? assume two first blocks (ms sans serif, for example):
	BOOL has_latin_western = FALSE;

	if (font->lfCharSet == OEM_CHARSET)
	{
		fontinfo->SetBlock(0,TRUE);
	}
	else if (!signature->fsUsb[0] &&
		!signature->fsUsb[1] &&
		!signature->fsUsb[2] &&
		!signature->fsUsb[3] &&
		!signature->fsCsb[0])
	{
		fontinfo->SetBlock(0,TRUE);
		fontinfo->SetBlock(1,TRUE);
		has_latin_western = TRUE;
	}


	// most fonts that have the most common codepages also support stuff that really are in the general punctuation unicode block.
    // is this called "heuristic overriding"? ;-)
	// "you don't know what you know, so I'll tell you what you know."
	// AKA
	// "if I want your opinion I'll give it to you"

	/*
	  adding these for now:
	  0 1252 Latin 1 
	  1 1250 Latin 2: Eastern Europe
	  17 932 Japanese, Shift-JIS 
	  18 936 Chinese: Simplified chars PRC and Singapore 
	  20 950 Chinese: Traditional chars Taiwan Region and Hong Kong SAR, PRC 

	  also consider these:
	  2 1251 Cyrillic 
	  3 1253 Greek 
	  4 1254 Turkish 
	  5 1255 Hebrew 
	  6 1256 Arabic 
	  7 1257 Baltic 
	*/

	BOOL has_punctuation = FALSE;
	BOOL has_chinese_punctuation = FALSE;
	BOOL has_currency = FALSE;
	BOOL has_geometric_shapes = FALSE;
	BOOL has_mathematical_operators = FALSE;
	
	for (i = 0; i < 22; i++)
	{
		if (signature->fsCsb[0] & (1 << i))
		{
			switch (i)
			{
				case 0:
				case 1:
					has_punctuation = TRUE;
					has_currency = TRUE;
					break;
				case 17: // jis
				case 18: // chinese-simplified
				case 19: // Korean Unified Hangeul Code
				case 20: // chinese-traditional
				case 21: // Korean (Johab)
					has_punctuation = TRUE;
					has_chinese_punctuation = TRUE;
					has_geometric_shapes = TRUE;
					has_mathematical_operators = TRUE;
					break;
			}
		}
	}

	if (has_punctuation)
		fontinfo->SetBlock(31, TRUE); // 31 is the "general punctuation" block

	if (has_chinese_punctuation)
	{
		fontinfo->SetBlock(48, TRUE); // 48 is the "cjk symbols and punctuation" block
		fontinfo->SetBlock(68, TRUE); // 68 is the "Halfwidth and Fullwidth Forms" block
	}

	if (has_currency)
		fontinfo->SetBlock(33, TRUE); // 33 is the "currency symbols" block

	if (has_geometric_shapes)
	{
		fontinfo->SetBlock(45, TRUE); // 45 is the "Geometric Shapes" block
	}

	if (has_mathematical_operators)
	{
		fontinfo->SetBlock(38, TRUE); // 38 is the "Mathematical Operators" block
	}

    /* rg comment in to debug with only certain fonts */
/*	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("batang")) != 0 &&
//		uni_stricmp(fontinfo->GetFace(), UNI_L("gulim")) != 0 &&
		uni_stricmp(fontinfo->GetFace(), UNI_L("simsun")) != 0 &&
//		uni_stricmp(fontinfo->GetFace(), UNI_L("simhei")) != 0 &&
//		uni_stricmp(fontinfo->GetFace(), UNI_L("ms mincho")) != 0 &&
		uni_stricmp(fontinfo->GetFace(), UNI_L("mingliu")) != 0 &&
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms gothic")) != 0 &&
		uni_stricmp(fontinfo->GetFace(), UNI_L("arial")) != 0 &&
		uni_stricmp(fontinfo->GetFace(), UNI_L("verdana")) != 0
		)
	{
		fontinfo->SetVertical(TRUE);
		fontinfo->SetFace(UNI_L("miffo"));
	}
	else
	{
		int debug = 1974;
	}
*/

    // set the codepage info...

	fontinfo->SetScript(WritingSystem::Unknown);

	if (signature->fsCsb[0] & 3 || has_latin_western)
		fontinfo->SetScript(WritingSystem::LatinWestern);

	if (signature->fsCsb[0] & (1 << 17))
		fontinfo->SetScript(WritingSystem::Japanese);

	if (signature->fsCsb[0] & (1 << 18))
		fontinfo->SetScript(WritingSystem::ChineseSimplified);

	if (signature->fsCsb[0] & (1 << 20))
		fontinfo->SetScript(WritingSystem::ChineseTraditional);

#ifdef _SORTED_CODEPAGE_PREFERENCE_
	// preferred fonts for certain codepages:
	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("songti")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("simsun")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x5B8B\x4F53")) == 0 ||
        uni_stricmp(fontinfo->GetFace(), UNI_L("\x65B0\x5B8B\x4F53")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 4);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("simfang")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x4EFF\x5B8B\x4F53")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 3);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("simkai")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x6977\x4F53")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 3);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("simhei")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x9ED1\x4F53")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 3);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("simli")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x96B6\x4E66")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 3);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("simyou")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x5E7C\x5706")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 3);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("stcaiyun")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x534E\x6587\x5F69\x4E91")) == 0 )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 2);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("stzhongs")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x534E\x4E2D\x5B8B")) == 0  )
   	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 2);
	}
    if(
		uni_stricmp(fontinfo->GetFace(), UNI_L("mingliu")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x7EC6\x660E")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), 1);
	}

	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("mingliu")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x7EC6\x660E")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE), 4);
	}

	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms mincho")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\x660E\x671D")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms pmincho")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\xFF30\x660E\x671D")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_JAPANESE_CODEPAGE), 3);
	}

	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms gothic")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\x30B4\x30B7\x30C3\x30AF")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms pgothic")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\xFF30\x30B4\x30B7\x30C3\x30AF")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_JAPANESE_CODEPAGE), 4);
	}

	// Dont use this font for CJK unless we really need to (bug 185062)
	if (uni_stricmp(fontinfo->GetFace(), UNI_L("Arial Unicode MS")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_JAPANESE_CODEPAGE), -2);
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE), -2);
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE), -2);
	}
#else
	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("songti")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x5B8B\x4F53")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("simhei")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x9ED1\x4F53")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("simkai")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x6977\x4F53")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("simli")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x96B6\x4E66")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("simyou")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x5E7C\x5706")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("stcaiyun")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x534E\x6587\x5F69\x4E91")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("stzhongs")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x534E\x4E2D\x5B8B")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("simfang")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x4EFF\x5B8B\x4F53")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x65B0\x5B8B\x4F53")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("mingliu")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x7EC6\x660E")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE));
	}

	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("mingliu")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\x7EC6\x660E")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE));
	}

	if (
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms mincho")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\x660E\x671D")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("ms gothic")) == 0 ||
		uni_stricmp(fontinfo->GetFace(), UNI_L("\xFF2D\xFF33\x0020\x30B4\x30B7\x30C3\x30AF")) == 0)
	{
		fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_JAPANESE_CODEPAGE));
	}
#endif //  _SORTED_CODEPAGE_PREFERENCE_

// <Traditional Chinese>
//mingliu: \x7EC6\x660E

/* <Korean>
batang
gulim
*/

//<Japanese>
//msmincho:\xFF2D\xFF33\x0020\x660E\x671D
//msgothic:\xFF2D\xFF33\x0020\x30B4\x30B7\x30C3\x30AF

/*<Simplified Chinese>
songti:\x5B8B\x4F53
simhei:\x9ED1\x4F53
simkai:\x6977\x4F53
simli:\x96B6\x4E66
simyou:\x5E7C\x5706
stcaiyun:\x534E\x6587\x5F69\x4E91
stzhongs:\x534E\x4E2D\x5B8B
simfang:\x4EFF\x5B8B\x4F53
\x65B0\x5B8B\x4F53
mingliu: \x7EC6\x660E
*/

	return OpStatus::OK;
}

// This function exist for one reason and that is to get weight info first when
// it is needed. It decreases the time it takes to start Opera, but the solution might
// not be as intuitive as one might want. Feel free to implement something more elegant.

BOOL GdiOpFontManager::HasWeight(UINT32 fontnr, UINT8 weight)
{
	if (fontnr >= (UINT32)m_num_fonts)
		return OpStatus::ERR;

	float nWeight = weight*100.0f;
	HFONT fnt=::CreateFont((int)(32*(100.0/72.0)),0,0,0,
				(int)nWeight,
				FALSE,
				FALSE,
				FALSE,
				DEFAULT_CHARSET,	// character set identifier 
				OUT_DEFAULT_PRECIS	,	// output precision 
				CLIP_DEFAULT_PRECIS	,	// clipping precision 
				DEFAULT_QUALITY	,	// output quality 
				FF_DONTCARE	,	// pitch and family 
				m_log_fonts[fontnr]->lfFaceName);				


	if (fnt == NULL) // the interface doesn't let us return a status (yet), but at least we do something defined.
		return FALSE;
	
	HGDIOBJ oldGdiObj = SelectObject(GdiOpFont::s_fonthdc, fnt);		
	
	TEXTMETRIC met;
	GetTextMetrics(GdiOpFont::s_fonthdc, &met);

	BOOL result = (met.tmWeight == nWeight);

	SelectObject(GdiOpFont::s_fonthdc,oldGdiObj);	//restore the old object, Win9x will fail dramatically freeing its own usage if not
	DeleteObject(fnt);

	return result;
}

OP_STATUS GdiOpFontManager::BeginEnumeration()
{
	return OpStatus::OK;
}

OP_STATUS GdiOpFontManager::EndEnumeration()
{
	return OpStatus::OK;
	

}

uni_char* GdiOpFontManager::GetLatinGenericFont(GenericFont generic_font)
{
	switch(generic_font)
	{
		case GENERIC_FONT_SERIF:
			return UNI_L("Times New Roman");
		case GENERIC_FONT_SANSSERIF:
			return UNI_L("Arial");
		case GENERIC_FONT_CURSIVE:
			return UNI_L("Comic Sans MS");
		case GENERIC_FONT_FANTASY:
			return UNI_L("Arial");
		case GENERIC_FONT_MONOSPACE:
			return IsSystemWinVista() ? UNI_L("Consolas") : UNI_L("Courier New");
		default:
			return UNI_L("Times New Roman");
	}
}

OP_STATUS GdiOpFontManager::InitGenericFont(OpString_list& serif, 
												OpString_list& sansserif, 
												OpString_list& cursive, 
												OpString_list& fantasy, 
												OpString_list& monospace)
{
	if (serif.Count())
		return OpStatus::OK;

	OP_STATUS init = OpStatus::ERR;
	
	do 
	{
		if (OpStatus::IsError(serif.Resize(WritingSystem::NumScripts))) break;
		if (OpStatus::IsError(sansserif.Resize(WritingSystem::NumScripts))) break;
		if (OpStatus::IsError(cursive.Resize(WritingSystem::NumScripts))) break;
		if (OpStatus::IsError(fantasy.Resize(WritingSystem::NumScripts))) break;
		if (OpStatus::IsError(monospace.Resize(WritingSystem::NumScripts))) break;
		init = OpStatus::OK;
	} while(FALSE);

	if (OpStatus::IsError(init))
	{
		OpStatus::Ignore(serif.Resize(0));
		OpStatus::Ignore(sansserif.Resize(0));
		OpStatus::Ignore(cursive.Resize(0));
		OpStatus::Ignore(fantasy.Resize(0));
		OpStatus::Ignore(monospace.Resize(0));
		return OpStatus::ERR;
	}

	for (int j=0; j< ARRAY_SIZE(scripts_fonts_initialization_list); j++)
	{
		OpString_list *list = &serif;
		switch(scripts_fonts_initialization_list[j].genericFont)
		{
			case GENERIC_FONT_SERIF:
				list = &serif;
				break;
			case GENERIC_FONT_SANSSERIF:
				list = &sansserif;
				break;
			case GENERIC_FONT_CURSIVE:
				list = &cursive;
				break;
			case GENERIC_FONT_FANTASY:
				list = &fantasy;
				break;
			case GENERIC_FONT_MONOSPACE:
				list = &monospace;
				break;
			default:;
		}

		list->Item(scripts_fonts_initialization_list[j].script).Set(
			GetFirstAvailableFont(scripts_fonts_initialization_list[j].fontsList, scripts_fonts_initialization_list[j].script));
	}

	// In case language detection failed to determine whether the content is simplified or traditional Chinese
	// Favor Simplified Chinese font for ChineseUnknown
	if (WritingSystem::ChineseTraditional == WritingSystem::FromEncoding(g_op_system_info->GetSystemEncodingL()))
	{
		serif.Item(WritingSystem::ChineseUnknown).Set(serif.Item(WritingSystem::ChineseTraditional));
		sansserif.Item(WritingSystem::ChineseUnknown).Set(sansserif.Item(WritingSystem::ChineseTraditional));
		monospace.Item(WritingSystem::ChineseUnknown).Set(monospace.Item(WritingSystem::ChineseTraditional));
	}
	else
	{
		serif.Item(WritingSystem::ChineseUnknown).Set(serif.Item(WritingSystem::ChineseSimplified));
		sansserif.Item(WritingSystem::ChineseUnknown).Set(sansserif.Item(WritingSystem::ChineseSimplified));
		monospace.Item(WritingSystem::ChineseUnknown).Set(monospace.Item(WritingSystem::ChineseSimplified));
	}

	// Use system writing system fonts for unkonwn script
	WritingSystem::Script script = WritingSystem::FromEncoding(g_op_system_info->GetSystemEncodingL());

	serif.Item(WritingSystem::Unknown).Set(serif.Item(script));
	sansserif.Item(WritingSystem::Unknown).Set(sansserif.Item(script));
	cursive.Item(WritingSystem::Unknown).Set(cursive.Item(script));
	fantasy.Item(WritingSystem::Unknown).Set(fantasy.Item(script));
	monospace.Item(WritingSystem::Unknown).Set(monospace.Item(script));

	for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		if (!serif.Item(i).HasContent())
			serif.Item(i).Set(GetLatinGenericFont(GENERIC_FONT_SERIF));
		if (!sansserif.Item(i).HasContent())
			sansserif.Item(i).Set(GetLatinGenericFont(GENERIC_FONT_SANSSERIF));
		if (!cursive.Item(i).HasContent())
			cursive.Item(i).Set(GetLatinGenericFont(GENERIC_FONT_CURSIVE));
		if (!fantasy.Item(i).HasContent())
			fantasy.Item(i).Set(GetLatinGenericFont(GENERIC_FONT_FANTASY));
		if (!monospace.Item(i).HasContent())
			monospace.Item(i).Set(GetLatinGenericFont(GENERIC_FONT_MONOSPACE));
	}

	return OpStatus::OK;
}

const uni_char* GdiOpFontManager::GetGenericFontName(GenericFont generic_font, WritingSystem::Script script)
{
	if (OpStatus::IsError(InitGenericFont(m_serif_fonts, m_sansserif_fonts, m_cursive_fonts, m_fantasy_fonts, m_monospace_fonts)))
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

bool GdiOpFontManager::GetRawFontData(UINT32 font_nr, BYTE *& raw_data, UINT32& data_length)
{
	WindowsRawFontData* data = m_fonts_raw_data.Get(font_nr);
	// in normal cases, this is a 1-to-1 mapping
	if(data && data->m_font_nr == font_nr)
	{
		raw_data = data->m_raw_data;
		data_length = data->m_raw_data_length;
		return true;
	}
	// otherwise, search with the slower path
	for(UINT32 x = 0; x < m_fonts_raw_data.GetCount(); x++)
	{
		data = m_fonts_raw_data.Get(x);
		if(font_nr == data->m_font_nr)
		{
			raw_data = data->m_raw_data;
			data_length = data->m_raw_data_length;
			return true;
		}
	}
	return false;
}
OP_STATUS GdiOpFontManager::AddRawFontData(UINT32 font_nr, BYTE* raw_data, UINT32 data_length)
{
	OpAutoPtr<WindowsRawFontData> data(OP_NEW(WindowsRawFontData, (font_nr, raw_data, data_length)));
	RETURN_OOM_IF_NULL(data.get());

	RETURN_IF_ERROR(m_fonts_raw_data.Add(data.get()));
	data.release();

	return OpStatus::OK;
}



#ifdef VEGA_GDIFONT_SUPPORT
# ifdef OPFONT_GLYPH_PROPS
OP_STATUS GdiOpFont::getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex)
{
	GLYPHMETRICS metrics;
	MAT2 identity = { {0,1},{0,0},{0,0},{0,1} };

	HFONT old_font = (HFONT) SelectObject(s_fonthdc, m_fnt);
	DWORD retval = GetGlyphOutline(s_fonthdc, c, GGO_METRICS, &metrics, 0, NULL, &identity);
	SelectObject(s_fonthdc, old_font);

	if (retval == GDI_ERROR)
		return OpStatus::ERR;

	glyph.top     = metrics.gmptGlyphOrigin.y;
	glyph.left    = metrics.gmptGlyphOrigin.x;
	glyph.advance = metrics.gmCellIncX;
	glyph.width   = metrics.gmBlackBoxX;
	glyph.height  = metrics.gmBlackBoxY;

	return OpStatus::OK;
}
# endif // OPFONT_GLYPH_PROPS
#endif // VEGA_GDIFONT_SUPPORT
