/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/*

  Using iType from Agfa requires a contract with Agfa, to get the license. Do not use
  this code in your project unless your project is allowed to use this library.

  Agfa iType has a number of features which makes it more suitable for embedded
  systems than Freetype.

  With this code, Agfa iType is integrated into mdf.

  To use it, define MDF_AGFA_SUPPORT, and make sure that you have turned off libfreetype.

  Also, when building, you will need to add the include directories of iType to the mdf
  build.

  It is best to build Agfa iType as a static library.
 */

#include "core/pch.h"

#ifdef MDEFONT_MODULE

#ifdef MDF_AGFA_SUPPORT

#include "modules/mdefont/mdefont.h"
#include "modules/unicode/unicode.h"

#ifdef MDF_FONT_ADVANCE_CACHE
# include "modules/mdefont/mdf_cache.h"
#endif // MDF_FONT_ADVANCE_CACHE

// inhibit compiler warnings
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif // _XOPEN_SOURCE
#ifdef _XOPEN_SOURCE_EXTENDED
#undef _XOPEN_SOURCE_EXTENDED
#endif // _XOPEN_SOURCE
#include "modules/iType/product/source/common/fs_api.h"

#ifdef MDF_AGFA_WTLE_SUPPORT
#include <tsapi.h>
#endif //MDF_AGFA_WTLE_SUPPORT

#ifdef MDF_DRAW_TO_MDE_BUFFER
#include "modules/libgogi/mde.h"
#endif // MDF_DRAW_TO_MDE_BUFFER

#ifdef MDE_MMAP_MANAGER
# include "modules/libgogi/mde_mmap.h"
#endif // MDE_MMAP_MANAGER

#if (MAJOR_VERSION >= 4) && (MINOR_VERSION >= 101)
# define GLYPHMAP_MODE
# define FS_IMAGE_MAP FS_GLYPHMAP
#else
# define FS_IMAGE_MAP FS_GRAYMAP
#endif

#include "modules/display/sfnt_base.h"

struct MDF_FontFileNameInfo
{
	MDF_FontFileNameInfo* next;
	char* sub_font_name;
	char* real_name;
	uni_char* file_name;
	int index;
	BOOL is_bold;
	BOOL is_italic;
#ifdef MDE_MMAP_MANAGER
	MDE_MMAP_File* mmap;
#endif // MDE_MMAP_MANAGER
};

struct MDF_FontInformation
{
	MDF_FontFileNameInfo* file_name_list;
	uni_char* font_name;
	MDF_SERIF has_serif;
	BOOL is_monospace;
	BOOL is_smallcaps;
	BOOL is_web_font;
	unsigned long ranges[4];
#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	UINT32 m_codepages[2];
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
};

// This is our OpWebFontRef implementation
struct MDF_WebFontImpl : public MDF_WebFontBase
{
	int						font_array_index; // for faster removal of webfonts
	MDF_FontInformation*	family;
	MDF_FontFileNameInfo*	face;
};

struct MDF_FontArray
{
	MDF_FontInformation** font_array;
	int size;
};

// To simplify the code we want to use the same structure
// as when glyph cache is on.
#ifndef MDF_FONT_GLYPH_CACHE
#define MDF_GLYPH_RENDERED 1
#define MDF_GLYPH_ID_IS_INDEX 2
struct MDF_FontGlyph
{
    MDF_GLYPH_BUFFER *buffer_data;  //  4
	short next;                     //  6
    uni_char c;                     //  8
    short bitmap_left;              // 10
    short bitmap_top;               // 12
    short advance;                  // 14
	short flags;					// 16
};
#endif

class MDF_AGFAFontEngine : public MDF_FontEngine
{
public:
	MDF_AGFAFontEngine();
	~MDF_AGFAFontEngine();
	OP_STATUS Construct();
	virtual OP_STATUS Init() { return OpStatus::OK; }

	virtual OP_STATUS AddFontFile(const uni_char* filename) { return AddFontFileInternal(filename); }
	virtual OP_STATUS AddWebFont(const uni_char* filename, MDF_WebFontBase*& webfont) { return AddFontFileInternal(filename, &webfont); }
	virtual OP_STATUS RemoveFont(MDF_WebFontBase* webfont);
	virtual OP_STATUS GetWebFontInfo(MDF_WebFontBase* webfont, MDF_FONTINFO* font_info);
	virtual OpFontInfo::FontType GetWebFontType(MDF_WebFontBase* webfont);

	virtual OP_STATUS BeginFontEnumeration();
	virtual void EndFontEnumeration();
	virtual int CountFonts();
	virtual BOOL HasComponentRendering() { return FALSE; }
	virtual OP_STATUS GetFontInfo(int font_nr, MDF_FONTINFO* font_info);
	virtual const uni_char* GetFontName(MDE_FONT* font);
	virtual const uni_char* GetFontFileName(MDE_FONT* font);
	virtual BOOL HasGlyph(MDE_FONT* font, uni_char c);
	virtual BOOL IsScalable(MDE_FONT* font);
	virtual BOOL IsBold(MDE_FONT* font);
	virtual BOOL IsItalic(MDE_FONT* font);

#ifdef MDF_KERNING
	virtual OP_STATUS ApplyKerning(MDE_FONT* font, ProcessedString& processed_string);
#endif // MDF_KERNING

#ifdef OPFONT_GLYPH_PROPS
	virtual OP_STATUS GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props);
#endif // OPFONT_GLYPH_PROPS

#ifdef OPFONT_GLYPH_PROPS
	virtual void GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props, BOOL isIndex = false);
#endif // OPFONT_GLYPH_PROPS
	virtual OP_STATUS GetLocalFont(MDF_WebFontBase*& webfont, const uni_char* facename);
	virtual MDE_FONT* GetWebFont(MDF_WebFontBase* webfont, int size);
	virtual MDE_FONT* GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size);

#ifdef MDF_AGFA_WTLE_SUPPORT
	virtual OP_STATUS ProcessString(MDE_FONT* font,
									ProcessedString& processed_string,
									const uni_char* str, const unsigned int len,
									const int extra_space = 0,
									const short word_width = -1);
#endif // MDF_AGFA_WTLE_SUPPORT

	virtual OP_STATUS GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, BOOL mustRender, BOOL isIndex);
	virtual void FreeGlyph(MDF_GLYPH& glyph);
	virtual OP_STATUS FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, const UINT32 stride, BOOL isIndex);
#ifdef MDF_FONT_GLYPH_CACHE
	virtual OP_STATUS BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, BOOL mustRender, BOOL isIndex);
	virtual OP_STATUS LoadFontGlyph(MDF_FontGlyph* glyph, MDE_FONT* font, BOOL mustRender);
	virtual void FreeFontGlyph(MDF_FontGlyph* glyph);
	virtual unsigned short GetGlyphCacheSize(MDE_FONT* font) const;
	virtual BOOL GetGlyphCacheFull(MDE_FONT* font) const;
	virtual unsigned short GetGlyphCacheFill(MDE_FONT* font) const;
#endif // MDF_FONT_GLYPH_CACHE

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(MDE_FONT* font, const UINT32 g_id, const UINT32 prev_g_id, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath* out_glyph);
#endif // SVG_SUPPORT

	virtual MDE_FONT* GetFont(int font_nr, int size, BOOL bold, BOOL italic);
	virtual void ReleaseFont(MDE_FONT* font);

	virtual OP_STATUS UCPToGID(MDE_FONT* font, ProcessedString& processed_string);

	virtual UINT32 GetCharIndex(MDE_FONT* font, const UINT32 ch);
	virtual UINT32 GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos);
	virtual void* GetFontImplementation(MDE_FONT* font);
#ifdef MDF_SFNT_TABLES
	virtual OP_STATUS GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size);
	virtual void FreeTable(BYTE* tab);
#endif // MDF_SFNT_TABLES

#ifdef MDF_FONT_FACE_CACHE
#error MDF_FONT_FACE_CACHE not implemented for AGFA engine
	virtual OP_STATUS LoadFontFace(MDF_FontFace** face, const char* font_name, int index);
#endif


private:
	OP_STATUS AddFontFileInternal(const uni_char* filename, MDF_WebFontBase** webfont = NULL);

	OP_STATUS GetFontInfoInternal(MDF_FontInformation* font, MDF_FONTINFO* font_info);

	OP_STATUS GetTableInt(unsigned long tab_tag, BYTE*& tab, UINT32& size);
	void GetBoldItalic(const char* style, BOOL& bold, BOOL& italic);
	const char* FindFontName(int font_nr, BOOL bold, BOOL italic);
	const char* FindFontNameFallback(int font_nr, BOOL bold, BOOL italic, BOOL* need_emboldment, BOOL* need_italic);
	OP_STATUS AddFontInformation(unsigned int font_number, MDF_FontInformation* font_information);
	MDF_FontFileNameInfo* CreateFontFileNameInfo(const char* font_family, const char* font_name, const char* itype_font_name, const int index, const uni_char* filename);
	BOOL FontInformationHasBoldItalic(MDF_FontInformation* font_information, MDF_FontFileNameInfo* file_name_info);
	BOOL HasNormal(MDF_FontInformation* font_information);
	BOOL HasBold(MDF_FontInformation* font_information);
	BOOL HasItalic(MDF_FontInformation* font_information);
	BOOL HasBoldItalic(MDF_FontInformation* font_information);
	MDF_FontInformation* CreateFontInformation(const FONT_METRICS &fm);

	MDF_FontInformation* FindFontInformation(char* font_name, int* font_array_index = NULL);

	void GetUnicodeRangesFromCMAP(const char* font_name, MDF_FontInformation *font_information);
	void GetFontPropertiesAndCodepageInfo(MDF_FontInformation *font_information);

	OP_STATUS SetAGFAFont(MDE_FONT* font);

	MDF_FontGlyph* GetChar(uni_char c, MDE_FONT* font, bool mustRender, BOOL isIndex);
	OP_STATUS LoadFontGlyphInt(MDF_FontGlyph* glyph, MDE_FONT* font, int mustRender, BOOL isIndex);
	void FreeFontGlyphInt(MDF_FontGlyph* glyph);
	int GlyphAdvance(MDE_FONT* font, const uni_char ch, BOOL isIndex = false);

	OpINT32HashTable<MDF_FontInformation> font_array;
	FS_STATE agfa_fs_state;
	BOOL initialized;

#ifdef MDF_AGFA_WTLE_SUPPORT
	class WTLE_LayoutEngine* layoutEngine;
	BOOL NeedsShaping(MDE_FONT* font, const uni_char* str, int len);
#endif //MDF_AGFA_WTLE_SUPPORT
};

#ifdef MDF_AGFA_WTLE_SUPPORT
class WTLE_Layout
{
public:
	WTLE_Layout();
	OP_STATUS Construct(MDE_FONT* font, const uni_char* str, int len, class WTLE_LayoutEngine* layoutEngine);
	~WTLE_Layout();

	/**
	 * Do the layout stuff.
	 */
	OP_STATUS Compose();
	/**
	 * Put the glyphs from the layout into glyphs pointer. Memory is allocated
	 * inside this functin and is deallocated when the object is destroyed.
	 *
	 * @param glyphs is the layouted glyphs. Out variable.
	 * @param len is the length of the glyphs array. Out variable.
	 */
	OP_STATUS GetGlyphs(uni_char** glyphs, int* len);

private:
	void CleanUp();

	void* m_charStr;
	WTLE_LayoutEngine* m_layoutEngine;
	uni_char* m_glyphStr;
};


struct AGFA_DATA;

class WTLE_LayoutEngine
{
public:
	WTLE_LayoutEngine();
	OP_STATUS Construct(FS_STATE& agfa_fs_state);
	~WTLE_LayoutEngine();

	/**
	 * Initialize the layout class with this layout engine and provided arguments.
	 */
	OP_STATUS InitLayout(WTLE_Layout* layout, MDE_FONT* font, const uni_char* str, int len);
	/**
	 * Attach WTLE font struct to agfa_data
	 */
	OP_STATUS AttachFont(AGFA_DATA* agfa_data, const char* font_name);
	/**
	 * Insert font into the layout engine.
	 */
	OP_STATUS InsertFont(void* addr, int size, int index);

	TsText* layoutText() { return m_source; }
	TsLayout* layout() { return m_layout; }

private:
	void CleanUp();

	TsLayoutControl* m_layoutControl;
	TsFontEngineMgr* m_fontEngineMgr;
	TsFontMgr* m_fontMgr;
	TsText* m_source;
	TsLayout* m_layout;
};
#endif //MDF_AGFA_WTLE_SUPPORT

// utility function to get bitmap in conjunction with isIndex flag
FS_IMAGE_MAP* get_fs_bitmap(FS_STATE& state, const uni_char ch, const BOOL isIndex)
{
	if (isIndex && !(FS_get_flags(&state) & FLAGS_CMAP_OFF))
	{
		FS_set_flags(&state, FLAGS_CMAP_OFF);
		FS_IMAGE_MAP* bitmap =
#ifdef GLYPHMAP_MODE
			FS_get_glyphmap(&state, ch, FS_MAP_GRAYMAP8);
#else
			FS_get_graymap(&state, ch);
#endif // GLYPHMAP_MODE
		FS_set_flags(&state, FLAGS_CMAP_ON);
		return bitmap;
	}
	else
		return
#ifdef GLYPHMAP_MODE
			FS_get_glyphmap(&state, ch, FS_MAP_GRAYMAP8);
#else
			FS_get_graymap(&state, ch);
#endif // GLYPHMAP_MODE
}


/**
   Compare if str1 and str2 are equal.
   str2 has the length len, and is in upper case.
   str1 might be upper case, but must not be upper case.
*/
static BOOL equal_strings_upr_case(const char* str1, const char* str2, int len)
{
	for (int i = 0; i < len; i++)
    {
		int c1 = str1[i];
		if (c1 == 0)
		{
			return false;
		}
		if (c1 > 'Z') // FIXME:maybe reversed.
		{
			if (c1 - ('a' - 'A') != str2[i])
			{
				return false;
			}
		}
		else
		{
			if (c1 != str2[i])
			{
				return false;
			}
		}
    }
	return true;
}

/**
   Try to find the string find_str inside the string str.
   The length of find_str is find_len.
   find_str is all upper case, str can be any case, and
   lower case will match upper case.
*/
static BOOL find_string_upr_case(const char* str, const char* find_str, int find_len)
{
	char* curr_str = (char*)str;
	while (*curr_str != 0)
    {
		if (equal_strings_upr_case(curr_str, find_str, find_len))
		{
			return true;
		}
		curr_str++;
    }
	return false;
}

#if !defined(MDF_FREETYPE_SUPPORT) && !defined(MDF_CUSTOM_FONTENGINE_CREATE)
OP_STATUS MDF_FontEngine::Create(MDF_FontEngine** engine)
#else
OP_STATUS CreateAGFAFontEngine(MDF_FontEngine** engine)
#endif // !MDF_FREETYPE_SUPPORT && !MDF_CUSTOM_FONTENGINE_CREATE
{
	MDF_AGFAFontEngine* e = OP_NEW(MDF_AGFAFontEngine, ());
	if (!e)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS err = e->Construct();
	if (OpStatus::IsError(err))
	{
		OP_DELETE(e);
		e = NULL;
	}
	*engine = e;
	return err;
}


MDF_AGFAFontEngine::MDF_AGFAFontEngine() : initialized(FALSE)
{
}

OP_STATUS MDF_AGFAFontEngine::Construct()
{
	// Initialize the font engine.
	if (FS_init(&agfa_fs_state, 500000) != SUCCESS)
		return OpStatus::ERR;

	initialized = TRUE;

#ifdef MDF_AGFA_WTLE_SUPPORT
	// Use glyph index instead of unicode chars to find glyphs in fonts.
	FS_set_flags(&agfa_fs_state, FLAGS_CMAP_OFF);

	layoutEngine = OP_NEW(WTLE_LayoutEngine, ());
	if (layoutEngine == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError(layoutEngine->Construct(agfa_fs_state)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
#endif //MDF_AGFA_WTLE_SUPPORT

	return OpStatus::OK;
}


/**
   Frees a MDF_FontFileNameInfo object.
*/
void FreeFontFileNameInfo(MDF_FontFileNameInfo* file_name_info)
{
	op_free(file_name_info->sub_font_name);
	file_name_info->sub_font_name = NULL;

	op_free(file_name_info->real_name);
	file_name_info->real_name = NULL;

#ifdef MDE_MMAP_MANAGER
	if (g_mde_mmap_manager)
	{
		g_mde_mmap_manager->UnmapFile(file_name_info->mmap);
		file_name_info->mmap = 0;
	}
#endif // MDE_MMAP_MANAGER

	op_free(file_name_info->file_name);
	file_name_info->file_name = NULL;
	OP_DELETE(file_name_info);
	file_name_info = NULL;
}
/**
   Frees the font information.
   FIXME: free all included types, too.
*/
void FreeFontInformation(MDF_FontInformation* font_information)
{
	MDF_FontFileNameInfo *file_name_info = font_information->file_name_list;
	MDF_FontFileNameInfo *next;
	while (file_name_info != NULL)
	{
		next = file_name_info->next;
		FreeFontFileNameInfo(file_name_info);
		file_name_info = next;
	}
	op_free(font_information->font_name);
	font_information->font_name = NULL;

	OP_DELETE(font_information);
	font_information = NULL;
}

static void FreeFontInformationFunc(const void *item, void *o)
{
	MDF_FontInformation* font_information = (MDF_FontInformation*)o;
	if(font_information)
		FreeFontInformation(font_information);
}

MDF_AGFAFontEngine::~MDF_AGFAFontEngine()
{
#ifdef MDF_AGFA_WTLE_SUPPORT
	OP_DELETE(layoutEngine);
#endif //MDF_AGFA_WTLE_SUPPORT

	// Shutdown the font engine. (FS_exit Always return SUCCESS)
	if (initialized)
		FS_exit(&agfa_fs_state);

	font_array.ForEach(FreeFontInformationFunc);
	font_array.RemoveAll();
}



/**
   Get information from the style about if this
   style is bold, italic, both or normal.
*/
void MDF_AGFAFontEngine::GetBoldItalic(const char* style, BOOL& bold, BOOL& italic)
{
	if (find_string_upr_case(style, "BOLD", 4))
    {
		bold = true;
    }
	else
    {
		bold = false;
    }

	if (find_string_upr_case(style, "ITALIC", 6) || find_string_upr_case(style, "OBLIQUE", 7))
    {
		italic = true;
    }
	else
    {
		italic = false;
    }
}

/**
   Finds the font name for the font_nr.
*/
const char* MDF_AGFAFontEngine::FindFontName(int font_nr, BOOL bold, BOOL italic)
{

	MDF_FontInformation* font_information = NULL;
	if (OpStatus::IsError(font_array.GetData(font_nr, &font_information)))
		return NULL;

	MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
    while (file_name_info != NULL)
    {
		if (file_name_info->is_bold == bold && file_name_info->is_italic == italic)
		{
			return file_name_info->sub_font_name; //file_name;
		}
		file_name_info = file_name_info->next;
    }
    return NULL;
}

const char* MDF_AGFAFontEngine::FindFontNameFallback(int font_nr, BOOL bold, BOOL italic, BOOL* need_emboldment, BOOL* need_italic)
{
	// Find regular font
	const char* font_name = FindFontName(font_nr, false, false);
	if (font_name)
	{
		if (bold && need_emboldment !=  NULL)
			*need_emboldment = true;
		if (italic && need_italic != NULL)
			*need_italic = true;
	}
	else
	{
		MDF_FontInformation* font_information = NULL;
		if (OpStatus::IsError(font_array.GetData(font_nr, &font_information)))
			return NULL;

		//Find any font with right font_nr
		MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
		if (file_name_info != NULL)
		{
			font_name = file_name_info->sub_font_name;
		}
	}

	return font_name;
}

/**
   Adds a MDF_FontInformation object to the font_array.
*/
OP_STATUS MDF_AGFAFontEngine::AddFontInformation(unsigned int font_number, MDF_FontInformation* font_information)
{
	return font_array.Add(font_number, font_information);
}

OP_STATUS MDF_AGFAFontEngine::RemoveFont(MDF_WebFontBase* webfont)
{
	OP_NEW_DBG("MDE_AGFAFontEngine::RemoveFont", "agfa");
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	OP_ASSERT(wfi);

	if(!wfi)
	{
		return OpStatus::ERR;
	}

	OP_ASSERT(wfi->family);
	OP_ASSERT(wfi->face);
	OP_ASSERT(wfi->font_array_index >= 0);

	OP_DBG(("wfi: %p face: %s", wfi, wfi->face->sub_font_name));

	MDF_FontInformation* font_information = wfi->family;

	// this is a local font, don't remove
	if (!(font_information->is_web_font))
	{
		OP_DELETE(wfi);
		return OpStatus::OK;
	}

	MDF_FontFileNameInfo* font_file_info = wfi->face;
	int font_array_index = wfi->font_array_index;

	OP_STATUS err = OpStatus::OK;

	// first face is a specialcase
	if(font_information->file_name_list == font_file_info)
	{
		if(font_file_info->next)
		{
			font_information->file_name_list = font_file_info->next;
			FS_delete_font(&agfa_fs_state, (FILECHAR*)wfi->face->sub_font_name);
			FreeFontFileNameInfo(font_file_info);
		}
		else
		{
			err = font_array.Remove(font_array_index, &font_information);
			if(OpStatus::IsSuccess(err))
			{
				FS_delete_font(&agfa_fs_state, (FILECHAR*)wfi->face->sub_font_name);
				FreeFontInformation(font_information);
			}
		}
	}
	else
	{
		MDF_FontFileNameInfo* prev_fi = font_information->file_name_list;
		for (MDF_FontFileNameInfo* fi = font_information->file_name_list; fi; fi = fi->next)
		{
			if(fi == font_file_info)
			{
				FS_delete_font(&agfa_fs_state, (FILECHAR*)wfi->face->sub_font_name);
				prev_fi->next = font_file_info->next;
				FreeFontFileNameInfo(font_file_info);
				break;
			}
			else
			{
				prev_fi = fi;
			}
		}
	}

	OP_DELETE(wfi);

	return err;
}

/**
   Create MDF_FontFileNameInfo for a specific face, with
   a specified file_name.
*/
MDF_FontFileNameInfo* MDF_AGFAFontEngine::CreateFontFileNameInfo(const char* font_family, const char* font_name, const char* itype_font_name, const int index, const uni_char* filename)
{
	MDF_FontFileNameInfo* file_name_info = OP_NEW(MDF_FontFileNameInfo, ());
	if (file_name_info == NULL)
    {
		return NULL;
    }

	file_name_info->next = NULL;
	file_name_info->index = index;  // Important for TTC fonts
	file_name_info->file_name = uni_strdup(filename);  // needed to track mmap
	if (file_name_info->file_name == NULL)
	{
		OP_DELETE(file_name_info);
		return NULL;
	}

	file_name_info->sub_font_name = op_strdup(itype_font_name);  //itype name

	if (file_name_info->sub_font_name == NULL)  //file_name
    {
		op_free(file_name_info->file_name);
		OP_DELETE(file_name_info);
		return NULL;
    }

	file_name_info->real_name = op_strdup(font_name);  //file_name
	if (file_name_info->real_name == NULL)
	{
		op_free(file_name_info->file_name);
		op_free(file_name_info->sub_font_name);
		OP_DELETE(file_name_info);
	}

	if (op_strlen(font_family) == op_strlen(font_name))
    {
		file_name_info->is_bold = false;
		file_name_info->is_italic = false;
    }
	else
    {
		GetBoldItalic(font_name, file_name_info->is_bold, file_name_info->is_italic);
    }

#ifdef MDE_MMAP_MANAGER
	file_name_info->mmap = 0;
#endif // MDE_MMAP_MANAGER

	return file_name_info;
}



/**
   Check if the MDF_FontInformation already has the bold and italic
   combination that we now want to add (so that we will not add two
   font styles with the same characteristics).
*/
BOOL MDF_AGFAFontEngine::FontInformationHasBoldItalic(MDF_FontInformation* font_information, MDF_FontFileNameInfo* file_name_info)
{
	BOOL is_bold = file_name_info->is_bold;
	BOOL is_italic = file_name_info->is_italic;
	MDF_FontFileNameInfo* file_info = font_information->file_name_list;
	while (file_info != NULL)
    {
		if (file_info->is_bold == is_bold && file_info->is_italic == is_italic)
		{
			return true;
		}
		file_info = file_info->next;
    }
	return false;
}




/**
   Has this font normal support?
*/
BOOL MDF_AGFAFontEngine::HasNormal(MDF_FontInformation* font_information)
{
	MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
	while (file_name_info != NULL)
    {
		if (!file_name_info->is_bold && !file_name_info->is_italic)
		{
			return true;
		}
		file_name_info = file_name_info->next;
    }

	return false;
}

/**
   Has this font bold support?
*/
BOOL MDF_AGFAFontEngine::HasBold(MDF_FontInformation* font_information)
{
    MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
    while (file_name_info != NULL)
    {
		if (file_name_info->is_bold && !file_name_info->is_italic)
		{
			return true;
		}
		file_name_info = file_name_info->next;
    }

    return false;
}

/**
   Has this font italic support?
*/
BOOL MDF_AGFAFontEngine::HasItalic(MDF_FontInformation* font_information)
{
    MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
    while (file_name_info != NULL)
    {
		if (file_name_info->is_italic && !file_name_info->is_bold)
		{
			return true;
		}
		file_name_info = file_name_info->next;
    }

    return false;
}

/**
   Has this font bold italic support?
*/
BOOL MDF_AGFAFontEngine::HasBoldItalic(MDF_FontInformation* font_information)
{
    MDF_FontFileNameInfo* file_name_info = font_information->file_name_list;
    while (file_name_info != NULL)
    {
		if (file_name_info->is_bold && file_name_info->is_italic)
		{
			return true;
		}
		file_name_info = file_name_info->next;
    }

    return false;
}



/**
   Creates font information.
   A lot of FIXME:s.
*/

MDF_FontInformation* MDF_AGFAFontEngine::CreateFontInformation(const FONT_METRICS &fm)
{
	OpString16 s;
	if (OpStatus::IsError(s.Set(fm.font_family_name)))
		return 0;
	const uni_char* font_name = s.CStr();

	MDF_FontInformation* font_info = OP_NEW(MDF_FontInformation, ());
	if (font_info == NULL)
    {
		return NULL;
    }

	font_info->font_name = uni_strdup(font_name);
	if (font_info->font_name == NULL)
    {
		OP_DELETE(font_info);
		return NULL;
    }
	// Setting default values
	font_info->file_name_list = NULL;
	font_info->has_serif = MDF_SERIF_UNDEFINED;
	font_info->is_monospace = false;
	font_info->is_smallcaps = false;


	// Default Latin-1
	font_info->ranges[0] = 1; // set to iso-latin-1-ish
	font_info->ranges[1] = 0;
	font_info->ranges[2] = 0;
	font_info->ranges[3] = 0;

	return font_info;
}


/**
   Find the font information belonging to a specific
   font name font_name, return NULL if that information
   is not yet in the gogi font system
*/
MDF_FontInformation* MDF_AGFAFontEngine::FindFontInformation(char* font_name, int* font_array_index)
{
	OpHashIterator* iter = font_array.GetIterator();
	if(!iter)
		return NULL;

	for (OP_STATUS status = iter->First();
		 OpStatus::IsSuccess(status); status = iter->Next())
	{
		MDF_FontInformation* font_information = (MDF_FontInformation*)iter->GetData();
		if (uni_strcmp(font_information->font_name, font_name) == 0)
		{


			if (font_array_index)
			{

				int* key = (int*)iter->GetKey();
				*font_array_index = *(int*)(&key);
			}

			OP_DELETE(iter);
			return font_information;
		}
    }

	OP_DELETE(iter);
	return NULL;
}



struct AGFA_DATA
{
	int font_nr;
	int size;
	BOOL bold;
	BOOL italic;
	BOOL need_emboldment;
	BOOL need_italic;
#ifdef MDF_AGFA_WTLE_SUPPORT
	TsFontStyle* wtle_font_style;
	BOOL has_substitution_table;
#endif //MDF_AGFA_WTLE_SUPPORT
#ifdef MDF_FONT_GLYPH_CACHE
 	MDF_FontGlyphCache* glyph_cache;
#endif
};
OP_STATUS MDF_AGFAFontEngine::AddFontFileInternal(const uni_char* filename, MDF_WebFontBase** webfont)
{
	OP_NEW_DBG("MDE_AGFAFontEngine::AddFontFile", "agfa");

	MDF_WebFontImpl* mwfi = NULL;
	if(webfont)
	{
		mwfi = OP_NEW(MDF_WebFontImpl, ());
		if(!mwfi)
			return OpStatus::ERR_NO_MEMORY;

		mwfi->font_array_index = -1;
		mwfi->family = NULL;
		mwfi->face = NULL;
	}
	int font_array_index = -1;
	unsigned int assigned_font_number = font_array.GetCount();

	// Add this file to the font list.
	char agfa_font_name[60]; // ARRAY OK 2009-04-24 wonko
	BOOL more_fonts_in_file = false;
	int index = 0;
	// NULL check
	if (filename == NULL) {
		OP_DELETE(mwfi);
		return OpStatus::ERR;
	}

	OpString8 utf8_filename;
	OP_STATUS status = utf8_filename.Set(filename);
	if (OpStatus::IsError(status)) {
		OP_DELETE(mwfi);
		return status;
	}

	// files should be on the form <name>.<tff|ccc|ttc|a3a|a3b|a3c>
	if (uni_strlen(filename) <= 4) {
		OP_DELETE(mwfi);
		return OpStatus::ERR;
	}

	op_memset(agfa_font_name, 0 , 60);
	// Load font from index 0, could be a TTC with more indexes
	do {
#ifdef MDE_MMAP_MANAGER
		MDE_MMAP_File *mmap;
		if (!g_mde_mmap_manager)
		{
			g_mde_mmap_manager = OP_NEW(MDE_MMapManager, ());
			if (!g_mde_mmap_manager) {
				OP_DELETE(mwfi);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		RETURN_IF_ERROR(g_mde_mmap_manager->MapFile(&mmap, filename));

		FS_LONG ret_val = FS_load_font(&agfa_fs_state, 0, (FS_BYTE*)mmap->filemap->GetAddress(), index, 60, (FILECHAR*)&agfa_font_name);
		// Check for errors
		if (ret_val != SUCCESS) {
			g_mde_mmap_manager->UnmapFile(mmap);
			mmap = 0;
			OP_DELETE(mwfi);
			return OpStatus::ERR;
		}
#else  // Not MDE_USE_MMAP
		FS_LONG ret_val = FS_load_font(&agfa_fs_state, (FILECHAR*)utf8_filename.CStr(), NULL, index, 60, (FILECHAR*)&agfa_font_name);

		// Check for errors
		if (ret_val != SUCCESS) {
			OP_DELETE(mwfi);
			return OpStatus::ERR;
		}
#endif //MDE_USE_MMAP

		// Set current font to active font
		ret_val = FS_set_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
		if (ret_val != SUCCESS)
		{
#ifdef MDE_MMAP_MANAGER
			g_mde_mmap_manager->UnmapFile(mmap);
			mmap = 0;
#endif //MDE_MMAP_MANAGER
			FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
			OP_DELETE(mwfi);
			return OpStatus::ERR;
		}

		FONT_METRICS fm;
		ret_val = FS_font_metrics(&agfa_fs_state, &fm);
		if (ret_val != SUCCESS)
		{
#ifdef MDE_MMAP_MANAGER
			g_mde_mmap_manager->UnmapFile(mmap);
			mmap = 0;
#endif //MDE_MMAP_MANAGER
			FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
			OP_DELETE(mwfi);
			return OpStatus::ERR;
		}

	    // Check if this file is a TrueType Collection
		if (fm.font_type & (0x1 << 4)) {

			// Continue until all fonts are read (indexs start from 0)
			if ((fm.indexInTTC+1) < fm.numFontsInTTC)
				more_fonts_in_file = true;
			else
				more_fonts_in_file = false;
		}


#ifdef DEBUG
		OP_DBG(("Family: %s Name: %s num_cmap_tables: %i\n",fm.font_family_name, fm.font_name, fm.num_cmap_tables));

		for (int i = 0; i < fm.num_cmap_tables; i++) {
			if (i >= MAX_MAPPINGS_LEN)
				break;
			CMAP_TAB cmap = fm.mappings[i];
			OP_DBG(("platform: %i encoding: %i offset: %li\n", cmap.platform, cmap.encoding, cmap.offset));
		}
#endif // DEBUG

		// Sanity check (broken fonts)
		BOOL sanity = false;
		for (int i = 0; i < fm.num_cmap_tables; i++) {
			if (i >= MAX_MAPPINGS_LEN) {
				sanity = false;
				break;
			}

			CMAP_TAB cmap = fm.mappings[i];
			// We only support windows unicode cmaps att the moment
			// so the font must support it
			if ((cmap.platform == 3) && (cmap.encoding == 1))
				sanity = true;

			if (cmap.platform > 3) {
				sanity = false;
				break;
			}
		}

		// We fail
		if (sanity == false)
		{
			OP_DBG(("SANITY TEST FAILED!!!\n"));
#ifdef MDE_MMAP_MANAGER
			g_mde_mmap_manager->UnmapFile(mmap);
			mmap = 0;
#endif //MDE_MMAP_MANAGER
			FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
			OP_DELETE(mwfi);
			return OpStatus::ERR;
		}

		MDF_FontInformation* font_information = NULL;

		// For webfonts we handle them as individual families
		if (!mwfi)
			font_information = FindFontInformation(fm.font_family_name, &font_array_index);

		if (font_information != NULL)
		{
			// Adding subfont to family
			MDF_FontFileNameInfo* file_name_info = CreateFontFileNameInfo(fm.font_family_name, fm.font_name, agfa_font_name, index, filename);

			if (file_name_info == NULL)
			{
				OP_DBG(("Couldn't Add subfamily. %s:%s\n",
						fm.font_family_name, fm.font_name));
#ifdef MDE_MMAP_MANAGER
				g_mde_mmap_manager->UnmapFile(mmap);
				mmap = 0;
#endif
				FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
				OP_DELETE(mwfi);
				return OpStatus::ERR_NO_MEMORY;
			}
#ifdef MDE_MMAP_MANAGER
			else
				file_name_info->mmap = mmap;
#endif // MDE_MMAP_MANAGER
			OP_DBG(("Added subfamily: %s:%s\n", fm.font_family_name, fm.font_name));

			if (FontInformationHasBoldItalic(font_information, file_name_info))
			{
				// Font allready added
				OP_DBG(("Font already added. face: %s\n", agfa_font_name));
				FreeFontFileNameInfo(file_name_info);
				FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);

				return OpStatus::OK; // Not totaly wrong to add the same font twice
			}

			file_name_info->next = font_information->file_name_list;
			font_information->file_name_list = file_name_info;
		}
		else
		{
			// New font family
			font_information = CreateFontInformation(fm);
			if (font_information == NULL)
			{
				OP_DBG(("Couldn't create a new font family.\n"));
#ifdef MDE_MMAP_MANAGER
				g_mde_mmap_manager->UnmapFile(mmap);
				mmap = 0;
#endif
				FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
				OP_DELETE(mwfi);
				return OpStatus::ERR_NO_MEMORY;
			}
			if (mwfi)
				font_information->is_web_font = 1;
			else
				font_information->is_web_font = 0;

			// Create FontFileNameInfo
			MDF_FontFileNameInfo* file_name_info = CreateFontFileNameInfo(fm.font_family_name, fm.font_name, agfa_font_name, index, filename);
			if (file_name_info == NULL)
			{
				FreeFontInformation(font_information);
#ifdef MDE_MMAP_MANAGER
				g_mde_mmap_manager->UnmapFile(mmap);
				mmap = 0;
#endif
				FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
				OP_DELETE(mwfi);
				return OpStatus::ERR_NO_MEMORY;
			}
#ifdef MDE_MMAP_MANAGER
			else
				file_name_info->mmap = mmap;
#endif // MDE_MMAP_MANAGER
			// Add FontFileNameInfo to the family
			font_information->file_name_list = file_name_info;

			// Add FontFileNameInfo to the font list
			if (OpStatus::IsError(AddFontInformation(assigned_font_number, font_information)))
			{
				FreeFontInformation(font_information);
				FS_delete_font(&agfa_fs_state, (FILECHAR*)&agfa_font_name);
				OP_DELETE(mwfi);
				return OpStatus::ERR_NO_MEMORY;
			}


			FS_ULONG tbl_size = 0;
			if (FS_get_table(&agfa_fs_state, TAG_cmap, TBL_QUERY, &tbl_size) != NULL)
			{
				GetUnicodeRangesFromCMAP(fm.font_name, font_information);
			}
			else
			{
				OP_ASSERT(!"Font is lacking CMAP");
				// fallback latin-1
				font_information->ranges[0] = 3;
				font_information->ranges[1] = 0;
				font_information->ranges[2] = 0;
				font_information->ranges[3] = 0;
			}

			// We get serif and monospace information from os2 table
			// This doesn't really work for now.
			FS_ULONG size = 0;
			if (FS_get_table(&agfa_fs_state, TAG_OS2, TBL_QUERY, &size) != NULL)
			{
				GetFontPropertiesAndCodepageInfo(font_information);
			} else {
                // Set default codepage by default
#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
				OP_ASSERT(!"Font is lacking OS/2 Table");
				font_information->m_codepages[0] = 0;
				font_information->m_codepages[1] = 0;
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
			}

		}

#ifdef MDF_AGFA_WTLE_SUPPORT
		if (OpStatus::IsError(layoutEngine->InsertFont(x->addr, x->size, index)))
		{
			return OpStatus::ERR;
		}
#endif //MDF_AGFA_WTLE_SUPPORT

		if(mwfi)
		{
			OP_ASSERT(mwfi->font_array_index == -1);
			mwfi->family = font_information;
			mwfi->face = font_information->file_name_list;
			mwfi->font_array_index = assigned_font_number;

			OP_DBG(("Added wfi: %p face: %s (font_array_index: %d)",
					mwfi, mwfi->face->sub_font_name, font_array_index));
		}

		// Index the next font entry (if any)
		if (more_fonts_in_file)
			++index;


	} while (more_fonts_in_file);

	if(mwfi)
	{
		OP_ASSERT(mwfi->font_array_index >= 0);
		OP_ASSERT(mwfi->family);
		OP_ASSERT(mwfi->face);
		*webfont = mwfi;
	}

	return OpStatus::OK;
}

OP_STATUS MDF_AGFAFontEngine::BeginFontEnumeration()
{
	// Do nothing.
	return OpStatus::OK;
}

void MDF_AGFAFontEngine::EndFontEnumeration()
{

}

int MDF_AGFAFontEngine::CountFonts()
{
	// Return the size.
	return font_array.GetCount();
}

MDE_FONT* MDF_AGFAFontEngine::GetWebFont(MDF_WebFontBase* webfont, int size)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;

	return GetFont(wfi->font_array_index, size, wfi->face->is_bold, wfi->face->is_italic);
}
MDE_FONT* MDF_AGFAFontEngine::GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;

	return GetFont(wfi->font_array_index, size, wfi->face->is_bold || weight > 5, wfi->face->is_italic || italic);
}

MDE_FONT* MDF_AGFAFontEngine::GetFont(int font_nr, int size, BOOL bold, BOOL italic)
{
	const char* font_name = FindFontName(font_nr, bold, italic);
	if (font_name == NULL)
	{
		font_name = FindFontNameFallback(font_nr, bold, italic, NULL, NULL);
	}

	if (font_name == NULL)
		return NULL;


		MDE_FONT* font = OP_NEW(MDE_FONT, ());
		if ( font == NULL )
			return NULL;

		AGFA_DATA* agfa_data = OP_NEW(AGFA_DATA, ());
		if ( agfa_data == NULL )
		{
			OP_DELETE(font);
			return NULL;
		}
#ifdef MDF_FONT_GLYPH_CACHE
		MDF_FontGlyphCache* cache = NULL;
		if ( OpStatus::IsError(MDF_FontGlyphCache::Create(&cache, this)) )
		{
			OP_DELETE(font);
			OP_DELETE(agfa_data);
			return NULL;
		}
		agfa_data->glyph_cache = cache;
#endif

		agfa_data->font_nr = font_nr;
		agfa_data->size = size;
		agfa_data->bold = bold;
		agfa_data->italic = italic;
		agfa_data->need_emboldment = false;
		agfa_data->need_italic = false;
		font->private_data = agfa_data;

		FS_LONG ret_val = FS_set_font(&agfa_fs_state, (FILECHAR*)font_name);

		if (ret_val != SUCCESS)
		{
			ReleaseFont(font);
			return NULL;
		}

#ifdef MDF_AGFA_WTLE_SUPPORT
		FS_ULONG tbl_size = 0;
		if ( FS_get_table(&agfa_fs_state, TAG_GSUB, TBL_QUERY, &tbl_size) || FS_get_table(&agfa_fs_state, TAG_GPOS, TBL_QUERY, &tbl_size) )
		{
			agfa_data->has_substitution_table = true;
		}
		else
		{
			agfa_data->has_substitution_table = false;
		}

		if (OpStatus::IsError(layoutEngine->AttachFont(agfa_data, font_name)))
		{
			ReleaseFont(font);
 			return NULL;
		}
#endif //MDF_AGFA_WTLE_SUPPORT

		// To get descent, ascent and height, we will have to set
		// the font as active.
		// We only need to get the size once for each size and font.
		FONT_METRICS fm;
		ret_val = FS_font_metrics(&agfa_fs_state, &fm);

		if (ret_val != SUCCESS)
		{
			ReleaseFont(font);
			return NULL;
		}

		font->descent = (size * abs(fm.hhea_descent) + (fm.unitsPerEm / 2)) / fm.unitsPerEm;
		font->ascent = (size * fm.hhea_ascent + (fm.unitsPerEm / 2)) / fm.unitsPerEm;
		font->height = font->ascent + font->descent;

		font->internal_leading = font->height - size;

		// FIXME: if there's a way to get this directly (i.e. without
		// fetching the table) it should probably be done that way
		// instead
		FS_ULONG tbl_size = 0;
		FS_BYTE* hhea_tab = FS_get_table(&agfa_fs_state, TAG_hhea, TBL_EXTRACT, &tbl_size);
		if (hhea_tab)
		{
			UINT16 adv = (hhea_tab[10] << 8) + hhea_tab[11];
			font->max_advance = (size * adv + (fm.unitsPerEm / 2)) / fm.unitsPerEm;
			FS_free_table(&agfa_fs_state, hhea_tab);
		}
		else
			font->max_advance = 0;
		font->extra_padding = 0;

		return font;
}

void MDF_AGFAFontEngine::ReleaseFont(MDE_FONT* font)
{
	if (font)
	{
		AGFA_DATA* ptr = (AGFA_DATA*)font->private_data;
#ifdef MDF_FONT_GLYPH_CACHE
		OP_DELETE(ptr->glyph_cache);
#endif
		OP_DELETE(ptr);
	}
	OP_DELETE(font);

}

OP_STATUS MDF_AGFAFontEngine::UCPToGID(MDE_FONT* font, ProcessedString& processed_string)
{
	OP_ASSERT(!processed_string.m_is_glyph_indices);
	RETURN_IF_ERROR(SetAGFAFont(font));
	for (size_t i = 0; i < processed_string.m_length; ++i)
		processed_string.m_processed_glyphs[i].m_id = FS_map_char(&agfa_fs_state, processed_string.m_processed_glyphs[i].m_id);
	processed_string.m_is_glyph_indices = TRUE;
	return OpStatus::OK;
}

UINT32 MDF_AGFAFontEngine::GetCharIndex(MDE_FONT* font, const UINT32 ch)
{
	if (OpStatus::IsError(SetAGFAFont(font)))
		return 0;
	return FS_map_char(&agfa_fs_state, ch);
}
UINT32 MDF_AGFAFontEngine::GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos)
{
	UINT32 ch = GetCharIndex(font, str[pos]);
	++pos;
	return ch;
}

OP_STATUS MDF_AGFAFontEngine::GetFontInfoInternal(MDF_FontInformation* font_information, MDF_FONTINFO* font_info)
{
	font_info->font_name = font_information->font_name;
    font_info->has_serif = font_information->has_serif;
    font_info->has_normal = HasNormal(font_information);
    font_info->has_bold = HasBold(font_information);
    font_info->has_italic = HasItalic(font_information);
    font_info->has_bold_italic = HasBoldItalic(font_information);

	font_info->ranges[0] = font_information->ranges[0];
	font_info->ranges[1] = font_information->ranges[1];
	font_info->ranges[2] = font_information->ranges[2];
	font_info->ranges[3] = font_information->ranges[3];

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	font_info->m_codepages[0] = font_information->m_codepages[0];
	font_info->m_codepages[1] = font_information->m_codepages[1];
#endif // MDF_CODEPAGES_FROM_OS2_TABLE

	font_info->is_monospace = font_information->is_monospace;

    return OpStatus::OK;
}

OP_STATUS MDF_AGFAFontEngine::GetFontInfo(int font_nr, MDF_FONTINFO* font_info)
{
	MDF_FontInformation* font_information = NULL;
	RETURN_IF_ERROR(font_array.GetData(font_nr, &font_information));


	return GetFontInfoInternal(font_information, font_info);
}

OP_STATUS MDF_AGFAFontEngine::GetWebFontInfo(MDF_WebFontBase* webfont, MDF_FONTINFO* font_info)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	if(!wfi)
		return OpStatus::ERR;

	return GetFontInfoInternal(wfi->family, font_info);
}

OpFontInfo::FontType MDF_AGFAFontEngine::GetWebFontType(MDF_WebFontBase* webfont)
{
	MDF_WebFontImpl* wfi = (MDF_WebFontImpl*)webfont;
	if (!wfi)
		return OpFontInfo::PLATFORM;

	if (wfi->family->is_web_font)
		return OpFontInfo::PLATFORM_WEBFONT;
	else
		return OpFontInfo::PLATFORM_LOCAL_WEBFONT;
}

OP_STATUS MDF_AGFAFontEngine::GetLocalFont(MDF_WebFontBase*& localfont, const uni_char* facename)
{
	// convert facename to big endian
	const size_t namelen = uni_strlen(facename);
	uni_char* facenameBE = OP_NEWA(uni_char, namelen);
	if (!facenameBE)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<uni_char> _f(facenameBE);
	op_memcpy(facenameBE, facename, namelen * sizeof(*facename));
	SwitchEndian((UINT8*)facenameBE, namelen * sizeof(*facename));

	const unsigned long tag = (((unsigned long)'n'<<24)|((unsigned long)'a'<<16)|((unsigned long)'m'<<8)|'e');
	BYTE* tab;
	UINT32 size;

	OpHashIterator* it = font_array.GetIterator();
	if (!it)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<OpHashIterator> _it(it);
	for (OP_STATUS status = it->First(); OpStatus::IsSuccess(status); status = it->Next())
	{
		MDF_FontInformation* fontinfo = (MDF_FontInformation*)it->GetData();
		OP_ASSERT(fontinfo);
		if (fontinfo->is_web_font)
			continue;
		for (MDF_FontFileNameInfo* fileinfo = fontinfo->file_name_list; fileinfo; fileinfo = fileinfo->next)
		{
			FS_LONG ret_val = FS_set_font(&agfa_fs_state, (FILECHAR*)fileinfo->sub_font_name);
			if (ret_val != SUCCESS)
			{
				return OpStatus::ERR;
			}

			RETURN_IF_ERROR(GetTableInt(tag, tab, size));
			BOOL match = MatchFace((const UINT8*)tab, size, facenameBE, namelen);
			FreeTable(tab);
			if (match)
			{
				MDF_WebFontImpl* mwfi = OP_NEW(MDF_WebFontImpl, ());
				if (!mwfi)
					return OpStatus::ERR_NO_MEMORY;
				mwfi->font_array_index = (int)(INTPTR)it->GetKey();
				mwfi->family = fontinfo;
				mwfi->face = fileinfo;
				localfont = mwfi;
				return OpStatus::OK;
			}
		}
	}

	return OpStatus::OK;
}

const uni_char* MDF_AGFAFontEngine::GetFontName(MDE_FONT* font)
{
	MDF_FontInformation* font_information = NULL;
	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;
	if (OpStatus::IsError(font_array.GetData(agfa_data->font_nr, &font_information)))
	{
		OP_ASSERT(!"failed to fetch font name for font");
		return 0;
	}
	return font_information->font_name;
}

const uni_char* MDF_AGFAFontEngine::GetFontFileName(MDE_FONT* font)
{
	MDF_FontInformation* font_information = NULL;
	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;
	if (OpStatus::IsError(font_array.GetData(agfa_data->font_nr, &font_information)))
	{
		OP_ASSERT(!"failed to fetch font name for font");
		return 0;
	}
	return font_information->file_name_list->file_name;
}


OP_STATUS MDF_AGFAFontEngine::SetAGFAFont(MDE_FONT* font)
{
	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;

	const char* font_name = FindFontName(agfa_data->font_nr, agfa_data->bold, agfa_data->italic);
	if (font_name == NULL)
	{
        // Try fallback
		font_name = FindFontNameFallback(agfa_data->font_nr, agfa_data->bold, agfa_data->italic, &agfa_data->need_emboldment, &agfa_data->need_italic);

		if (!font_name)
			return OpStatus::ERR;
	}

	MDF_FontInformation* font_information = NULL;
	RETURN_IF_ERROR(font_array.GetData(agfa_data->font_nr, &font_information));

	FS_LONG ret_val = FS_set_font(&agfa_fs_state, (FILECHAR*)font_name);
	if (ret_val != SUCCESS)
	{
		return OpStatus::ERR;
	}

	// PSEUDO_BOLD
	if (agfa_data->need_emboldment) {
		FONT_METRICS fm;
		ret_val = FS_font_metrics(&agfa_fs_state, &fm);
		if (ret_val != SUCCESS)
			return OpStatus::ERR;

		if ((fm.font_type & FM_FLAGS_TTF) || // TrueType
			(fm.font_type & FM_FLAGS_CCC))   // mono
			FS_set_bold_pct(&agfa_fs_state, 3277); // 5%
		else if ((fm.font_type & 0x02) || // bit 1 = STF
				 (fm.font_type & 0x04) || // bit 2 = CCC
				 (fm.font_type & 0x08))   // bit 3 = ACT
			FS_set_stroke_pct(&agfa_fs_state, 4588); // 7%

	}
	else { // clean up
		FS_set_bold_pct(&agfa_fs_state, 0);
		FS_set_stroke_pct(&agfa_fs_state, FS_DEFAULT_STROKE_PCT);
	}

	int size = agfa_data->size;
	// iType has a lower limit set to 2 pixels
	if (size < 2)
		size = 2;

	// The 2x2 scale, skew and roation matrix
	int s00 = size;
	int s01 = 0;
	int s10 = 0;
	int s11 = size;

	if (agfa_data->need_italic) {
		// 15 degrees slant precalculated
		s01 = (int)(s11*0.267949192); // tan(15.0 * 0.01745329251944);
	}

	if (font_information->is_web_font)
		FS_set_flags(&agfa_fs_state, FLAGS_HINTS_OFF);
	else
		FS_set_flags(&agfa_fs_state, FLAGS_HINTS_ON);

	if (FS_set_scale(&agfa_fs_state,
					 (int)s00*65536,
					 (int)s01*65536,
					 (int)s10*65536,
					 (int)s11*65536) != SUCCESS)
		return OpStatus::ERR;
	if (FS_set_cmap(&agfa_fs_state, 3, 1) != SUCCESS)
		return OpStatus::ERR;

	return OpStatus::OK;
}

static inline OP_STATUS GetGlyphBuffer(MDF_GLYPH_BUFFER& buf, FS_IMAGE_MAP* bitmap, const UINT32 stride = 0)
{
	if (!bitmap || bitmap->width < 0 || bitmap->height < 0)
		return OpStatus::ERR;

	UINT8* d = 0;
	UINT32 s;
	if (stride)
	{
		d = (UINT8*)buf.data;
		if (bitmap->width <= 0)
			buf.w = 0;
		else if ((unsigned int)bitmap->width < buf.w)
			buf.w = bitmap->width;
		if (bitmap->height <= 0)
			buf.h = 0;
		else if ((unsigned int)bitmap->height < buf.h)
			buf.h = bitmap->height;
		s = stride;
	}
	else
	{
		d = OP_NEWA(UINT8, bitmap->height * bitmap->width);
		if (!d)
			return OpStatus::ERR_NO_MEMORY;
		buf.data = d;
		if (bitmap->width <= 0)
			buf.w = 0;
		else
			buf.w = bitmap->width;
		if (bitmap->height <= 0)
			buf.h = 0;
		else
			buf.h = bitmap->height;
		s = buf.w;
	}
	for (unsigned int yc = 0; yc < buf.h; yc++)
	{
#ifdef GLYPHMAP_MODE
		// 8bit per pixel
		op_memcpy(d, bitmap->bits + buf.w*yc, buf.w);
		d += s;
#else // GLYPHMAP_MODE
        // 4bit per pixel
		UINT8 *rowp = (UINT8*)(bitmap->bits + bitmap->bpl*yc);
		d = (UINT8*)(((long int)buf.data)+s*yc);
		for (unsigned int xc = 0; xc < buf.w/2; xc++)
		{
			*d = (*rowp & 0xf0) | ((*rowp & 0xf0) >> 4);
			++d;
			*d = (*rowp & 0x0f) | ((*rowp & 0x0f) << 4);
			++d;
			++rowp;
		}
		if (buf.w % 2 != 0)
			*d = (*rowp & 0xf0);
#endif // GLYPHMAP_MODE
	}
	return OpStatus::OK;
}

#ifdef MDF_AGFA_WTLE_SUPPORT
OP_STATUS MDF_AGFAFontEngine::ProcessString(MDE_FONT* font,
											ProcessedString& processed_string,
											const uni_char* str, const unsigned int len,
											const int extra_space/* = 0*/,
											const short word_width/* = -1*/)
{
	WTLE_Layout layout;

	if (!NeedsShaping(font, str, len))
		return MDF_FontEngine::ProcessString(font, processed_string, str, len, extra_space, word_width);

	RETURN_IF_ERROR(layoutEngine->InitLayout(&layout, font, str, len));
	RETURN_IF_ERROR(layout.Compose());
	return layout.GetProcessedString(processed_string, extra_space, word_width);
}
#endif // MDF_AGFA_WTLE_SUPPORT

OP_STATUS MDF_AGFAFontEngine::GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, BOOL mustRender, BOOL isIndex)
{
	RETURN_IF_ERROR( SetAGFAFont(font) );
	FS_IMAGE_MAP* bitmap = get_fs_bitmap(agfa_fs_state, ch, isIndex);
	if (!bitmap)
		return OpStatus::ERR_NO_MEMORY;

	MDF_GLYPH_BUFFER* buffer = 0;
	if (mustRender)
	{
		buffer = OP_NEW(MDF_GLYPH_BUFFER, ());
		if (!buffer)
		{
			FS_free_char(&agfa_fs_state, bitmap);
			return OpStatus::ERR_NO_MEMORY;
		}

		OP_STATUS status = GetGlyphBuffer(*buffer, bitmap);
		if (OpStatus::IsError(status))
		{
			FS_free_char(&agfa_fs_state, bitmap);
			OP_DELETE(buffer);
			return status;
		}
	}

	glyph.bitmap_left = bitmap->lo_x;
	glyph.bitmap_top = bitmap->hi_y + 1; // baseline on row 0.
	if (bitmap->i_dx != 0)
		glyph.advance = bitmap->i_dx;
	else {
		glyph.advance = bitmap->dx/65536;
		glyph.advance += ((((bitmap->dx & 0x00ff)>>15) < 5) ? 0:1);
	}
	glyph.buffer_data = buffer;

#ifdef MDF_FONT_ADVANCE_CACHE
	OP_ASSERT(font->m_advance_cache);
	if (!isIndex)
		font->m_advance_cache->Set(ch, glyph.advance);
#endif // MDF_FONT_ADVANCE_CACHE

	FS_free_char(&agfa_fs_state, bitmap);
	return OpStatus::OK;
}
void MDF_AGFAFontEngine::FreeGlyph(MDF_GLYPH& glyph)
{
	if (glyph.buffer_data)
	{
		UINT8* buf = (UINT8*)glyph.buffer_data->data;
		OP_DELETEA(buf);
		OP_DELETE(glyph.buffer_data);
	}
}
OP_STATUS MDF_AGFAFontEngine::FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, const UINT32 stride, BOOL isIndex)
{
	if (!glyph.buffer_data || !glyph.buffer_data->data)
		return OpStatus::ERR;

	RETURN_IF_ERROR( SetAGFAFont(font) );
	FS_IMAGE_MAP* bitmap = get_fs_bitmap(agfa_fs_state, ch, isIndex);
	if (!bitmap)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = GetGlyphBuffer(*glyph.buffer_data, bitmap, stride);
	if (OpStatus::IsError(status))
	{
		FS_free_char(&agfa_fs_state, bitmap);
		return status;
	}

	glyph.bitmap_left = bitmap->lo_x;
	glyph.bitmap_top = bitmap->hi_y + 1; // baseline on row 0.
	glyph.advance = bitmap->i_dx;

	FS_free_char(&agfa_fs_state, bitmap);
	return OpStatus::OK;
}

#ifdef SVG_SUPPORT
OP_STATUS MDF_AGFAFontEngine::GetOutline(MDE_FONT* font, const UINT32 g_id, const UINT32 prev_g_id, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath* out_glyph)
{
	FS_STATE *agfa_fs_state = (FS_STATE*)MDF_GetFontImplementation(font);
	if (!agfa_fs_state)
		return OpStatus::ERR_NOT_SUPPORTED;

	FONT_METRICS fm;
	if (FS_font_metrics(agfa_fs_state, &fm) != SUCCESS)
		return OpStatus::ERR_NOT_SUPPORTED;

	if ((fm.font_type & 0xe) > 0)
		return OpStatus::ERR_NOT_SUPPORTED;

	// Set the VERTICAL ON State
	if (in_writing_direction_horizontal == 0)
		FS_set_flags(agfa_fs_state, FLAGS_VERTICAL_ON);
	else
		FS_set_flags(agfa_fs_state, FLAGS_VERTICAL_OFF);

	FS_set_flags(agfa_fs_state, FLAGS_CMAP_OFF);
	FS_OUTLINE *outline = FS_get_outline(agfa_fs_state, g_id);
	FS_set_flags(agfa_fs_state, FLAGS_CMAP_ON);
	if (!outline)
	{
		// Reset the VERTICAL State
		if (in_writing_direction_horizontal == 0)
			FS_set_flags(agfa_fs_state, FLAGS_VERTICAL_OFF);
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Reset the VERTICAL State
	if (in_writing_direction_horizontal == 0)
		FS_set_flags(agfa_fs_state, FLAGS_VERTICAL_OFF);

	int align_x = 0;
	int align_y = 0;

	if (in_writing_direction_horizontal == 0)
	{
		// Agfa centers the glyph at origo and use negative
		// y-values for vertical text so we need to translate
		// the outline to the SVG representation which place
		// origo in the lower left corner.
		align_x = -outline->lo_x;
		align_y = -outline->lo_y;
	}

	int i_x = 0;
	int i_y = 0;
#define FIXPOINT_TO_SVG(X)    ((X) / 65536.0)

	for (int i = 0; i < outline->num; i++)
	{
		switch (outline->type[i] & 0x7f) {
		case FS_MOVETO:
			out_glyph->Close();
			out_glyph->MoveTo(FIXPOINT_TO_SVG(outline->x[i_x++]+align_x),
					   FIXPOINT_TO_SVG(outline->y[i_y++]+align_y),
					   FALSE);
			break;
		case FS_LINETO:
			out_glyph->LineTo(FIXPOINT_TO_SVG(outline->x[i_x++]+align_x),
					   FIXPOINT_TO_SVG(outline->y[i_y++]+align_y),
					   FALSE);
			break;
		case FS_QUADTO: {
			SVGNumber control_x = FIXPOINT_TO_SVG(outline->x[i_x++]+align_x);
			SVGNumber control_y = FIXPOINT_TO_SVG(outline->y[i_y++]+align_y);
			SVGNumber end_x = FIXPOINT_TO_SVG(outline->x[i_x++]+align_x);
			SVGNumber end_y = FIXPOINT_TO_SVG(outline->y[i_y++]+align_y);
			out_glyph->QuadraticCurveTo(control_x,
								 control_y,
								 end_x,
								 end_y,
								 FALSE,
								 FALSE);
			break;
		}
		case FS_CUBETO: {
			OP_ASSERT(!"FS_CUBETO!!!!"); // Never been tested (no font avail)
			SVGNumber control1_x = FIXPOINT_TO_SVG(outline->x[i_x++]+align_x);
			SVGNumber control1_y = FIXPOINT_TO_SVG(outline->y[i_y++]+align_y);
			SVGNumber control2_x = FIXPOINT_TO_SVG(outline->x[i_x++]+align_x);
			SVGNumber control2_y = FIXPOINT_TO_SVG(outline->y[i_y++]+align_y);
			SVGNumber end_x = FIXPOINT_TO_SVG(outline->x[i_x++]+align_x);
			SVGNumber end_y = FIXPOINT_TO_SVG(outline->y[i_y++]+align_y);
			out_glyph->CubicCurveTo(control1_x,
							 control1_y,
							 control2_x,
							 control2_y,
							 end_x,
							 end_y,
							 FALSE,
							 FALSE);
			break;
		}
		default:
			OP_ASSERT(!"Unknown outline type");
		}
	}

	if(in_writing_direction_horizontal)
	{
		if (outline->i_dx != 0)
			out_advance = outline->i_dx;
		else
			out_advance = FIXPOINT_TO_SVG(outline->dx);
	}
	else
	{
		if (outline->i_dy != 0)
			out_advance = outline->i_dy;
		else
			out_advance = FIXPOINT_TO_SVG(outline->dy);
	}

	// Make sure the path is closed
	out_glyph->Close();

	FS_free_char(agfa_fs_state, outline);
	return OpStatus::OK;
}
#endif // SVG_SUPPORT

int MDF_AGFAFontEngine::GlyphAdvance(MDE_FONT* font, const uni_char ch, BOOL isIndex)
{
	MDF_FontGlyph *cc = GetChar( ch, font, false, isIndex );
	OP_ASSERT(cc);
	if (cc == NULL)
		return 0;

	int advance = cc->advance;
#ifndef MDF_FONT_GLYPH_CACHE
	FreeFontGlyphInt(cc);
#endif
	return advance;
}


BOOL MDF_AGFAFontEngine::HasGlyph(MDE_FONT* font, uni_char c)
{
	if ( OpStatus::IsError( SetAGFAFont(font) ) )
		return false;

	return FS_map_char(&agfa_fs_state, c) != 0;
}

BOOL MDF_AGFAFontEngine::IsScalable(MDE_FONT* font)
{
	// all fonts are scalable
	return TRUE;
}

BOOL MDF_AGFAFontEngine::IsBold(MDE_FONT* font)
{
	MDF_FontInformation* font_information = NULL;
	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;
	if (OpStatus::IsError(font_array.GetData(agfa_data->font_nr, &font_information)))
	{
		OP_ASSERT(!"failed to fetch bold information for font");
		return 0;
	}
	return font_information->file_name_list->is_bold;
}

BOOL MDF_AGFAFontEngine::IsItalic(MDE_FONT* font)
{
	MDF_FontInformation* font_information = NULL;
	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;
	if (OpStatus::IsError(font_array.GetData(agfa_data->font_nr, &font_information)))
	{
		OP_ASSERT(!"failed to fetch italic information for font");
		return 0;
	}
	return font_information->file_name_list->is_italic;
}

#ifdef MDF_KERNING
OP_STATUS MDF_AGFAFontEngine::ApplyKerning(MDE_FONT* font, ProcessedString& processed_string)
{
	OP_ASSERT(processed_string.m_is_glyph_indices);
	RETURN_IF_ERROR( SetAGFAFont(font) );
	FS_FIXED kern_x, kern_y;
	FS_FIXED acc_x = 0;
	for (size_t i = 1; i < processed_string.m_length; ++i)
	{
		if (FS_get_kerning(&agfa_fs_state,
						   processed_string.m_processed_glyphs[i-1].m_id,
						   processed_string.m_processed_glyphs[i].m_id,
						   &kern_x, &kern_y) == SUCCESS)
			acc_x += kern_x;
		processed_string.m_processed_glyphs[i].m_pos.x += acc_x >> 16;
	}
}
#endif // MDF_KERNING

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS MDF_AGFAFontEngine::GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props)
{
	RETURN_IF_ERROR( SetAGFAFont(font) );
	FS_IMAGE_MAP* bitmap = get_fs_bitmap(agfa_fs_state, up, FALSE);
	if (!bitmap) // FIXME: OOM
		return OpStatus::ERR_NO_MEMORY;

	props->left = bitmap->lo_x;
	props->top = bitmap->hi_y + 1; // baseline on row 0.
	props->width = bitmap->width;
	props->height = bitmap->height;
	props->advance = bitmap->i_dx;

	FS_free_char(&agfa_fs_state, bitmap);
	return OpStatus::OK;
}
#endif // OPFONT_GLYPH_PROPS

#ifdef OPFONT_GLYPH_PROPS
void MDF_AGFAFontEngine::GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props, BOOL isIndex)
{
	if ( OpStatus::IsError( SetAGFAFont(font) ) )
	{
		props->top = props->left = props->width = props->height = props->advance = 0;
		return;
	}

	FS_IMAGE_MAP* bitmap = get_fs_bitmap(agfa_fs_state, up, isIndex);

	if (!bitmap) // FIXME: OOM
	{
		props->top = props->left = props->width = props->height = props->advance = 0;
		return;
	}

	props->left = bitmap->lo_x;
	props->top = bitmap->hi_y + 1; // baseline on row 0.
	props->width = bitmap->width;
	props->height = bitmap->height;
	props->advance = bitmap->i_dx;

	FS_free_char(&agfa_fs_state, bitmap);
}
#endif // OPFONT_GLYPH_PROPS

void* MDF_AGFAFontEngine::GetFontImplementation(MDE_FONT* font)
{
	// Try to set the font to active
	if (OpStatus::IsError(SetAGFAFont(font)))
		return NULL;

	return &agfa_fs_state;
}


#define GET_xWORD(a)  \
            ( ((FS_USHORT) *(FS_BYTE *)(a) << 8) |      \
               (FS_USHORT) *((FS_BYTE *)(a)+1) )

#define GET_xLONG(a)  \
            ( ((FS_ULONG) *(FS_BYTE *)(a) << 24)     |  \
              ((FS_ULONG) *((FS_BYTE *)(a)+1) << 16) |  \
              ((FS_ULONG) *((FS_BYTE *)(a)+2) << 8)  |  \
               (FS_ULONG) *((FS_BYTE *)(a)+3) )


struct CMAPTab
{
	//We currently ponly handle format 4
	uint16 format;
	uint16 length;
	uint16 language;
	uint16 seg_count_X2;    // 2 * segCount
	uint16 search_range;   // 2 * (2**FLOOR(log2(segCount)))
	uint16 entry_selector; // log2(searchRange/2)
	uint16 range_shift;    // (2 * segCount) - searchRange

	byte   *rest;

	// The rest of the subtable is dynamic
	/*
	  uint16 end_code[segCount] // Ending character code for each segment, last = 0xFFFF.
	  uint16 reserved_pad; // This value should be zero
	  uint16 start_code[segCount]; // Starting character code for each segment
	  uint16 id_delta[segCount];// Delta for all character codes in segment
	  uint16 id_range_offset[segCount];// Offset in bytes to glyph indexArray, or 0
		uint16 glyph_index_array[variable];
	*/

};

struct CMAPIdx
	{
		uint16 version;
		uint16 nr_of_subtables;
};

struct  CMAPSubtabInfo
{
	uint16 platform_id;
	uint16 platform_spec_id;
	uint32 offset;
};


class CMAPTable
{
	public:
	explicit CMAPTable(byte* raw_table)
			: m_raw_table( raw_table ),
			  m_idx( OP_NEW(CMAPIdx, ()) ),
			  m_cmap( NULL ),
			  m_start_code( NULL ),
			  m_end_code( NULL )
		{
			if (m_raw_table != NULL && m_idx != NULL)
			{
				m_idx->version = GET_xWORD(raw_table);
				m_idx->nr_of_subtables = GET_xWORD(raw_table + 2);
			}
		}


	~CMAPTable()
		{
			OP_DELETE(m_idx);
			OP_DELETE(m_cmap);
		}

	/**
	 * Selects the cmap subtable. Must be called before StartCode()/EndCode().
	 */
	BOOL SelectCMAPSubtable(int platform, int encoding)
		{
			const int SIZE_OF_CMAP_IDX = 4;
			const int SIZE_OF_CMAP_SUBTAB_INFO = 8;

			if ( NULL == m_raw_table || NULL == m_idx ) return FALSE;

			for ( int table_idx = 0; table_idx < m_idx->nr_of_subtables; ++table_idx )
			{
				int subtable_offset = SIZE_OF_CMAP_IDX + table_idx*SIZE_OF_CMAP_SUBTAB_INFO;

				CMAPSubtabInfo subtable_info;
				subtable_info.platform_id = GET_xWORD (m_raw_table + subtable_offset );
				subtable_info.platform_spec_id = GET_xWORD (m_raw_table + subtable_offset +2);
				subtable_info.offset = GET_xLONG(m_raw_table + subtable_offset + 4);

				if ( subtable_info.platform_id == platform )
				{
					byte* m_raw_cmap = (m_raw_table + subtable_info.offset);

					m_cmap = OP_NEW(CMAPTab, ());
					if ( NULL == m_cmap) return FALSE;

					m_cmap->format = GET_xWORD(m_raw_cmap);

					if ( m_cmap->format == 4 )
					{
						m_cmap->length = GET_xWORD (m_raw_cmap + 2);
						m_cmap->language = GET_xWORD (m_raw_cmap + 4);
						m_cmap->seg_count_X2 = GET_xWORD (m_raw_cmap + 6);
						m_cmap->search_range = GET_xWORD (m_raw_cmap + 8);
						m_cmap->entry_selector = GET_xWORD (m_raw_cmap + 10);
						m_cmap->range_shift = GET_xWORD (m_raw_cmap + 12);
						m_cmap->rest = m_raw_cmap + 14;

						m_end_code = (m_cmap->rest);
						m_start_code = ((m_cmap->rest) + m_cmap->seg_count_X2 + 2);
					}

					return TRUE;
				}
			}

			return FALSE;
		}

	uint16 StartCode(int segment) const
		{
			if (NULL != m_cmap)
			{
				return GET_xWORD( &(m_start_code[ segment*2 ]) );
			}
			return 0xffff;
		}

	uint16 EndCode(int segment) const
		{
			if (NULL != m_cmap)
			{
				return GET_xWORD( &(m_end_code[ segment*2 ]) );
			}
			return 0xffff;
		}

	private:
	//Prevent copying
	CMAPTable(const CMAPTable&);
	CMAPTable& operator=(const CMAPTable&);

	BOOL m_attached; /**<If attached free the raw cmap table. */
	byte* m_raw_table;
	CMAPIdx* m_idx;
	CMAPTab* m_cmap;
	byte* m_start_code; /**< Array of segment start codes. */
	byte* m_end_code; /**< Array of segment end codes. */
};


const uint32_t unicode_blocks[][2] = {
	{0x0000, 0x007F},  //  Basic Latin
	{0x0080, 0x00FF},  //  Latin-1 Supplement
	{0x0100, 0x017F},  //  Latin Extended-A
	{0x0180, 0x024F},  //  Latin Extended-B
	{0x0250, 0x02AF},  //  IPA Extensions
	{0x02B0, 0x02FF},  //  Spacing Modifier Letters
	{0x0300, 0x036F},  //  Combining Diacritical Marks
	{0x0370, 0x03FF},  //  Greek and Coptic
	{0x0400, 0x04FF},  //  Cyrillic // 8
	{0x0500, 0x052F},  //  Cyrillic Supplement
	{0x0530, 0x058F},  //  Armenian
	{0x0590, 0x05FF},  //  Hebrew
	{0x0600, 0x06FF},  //  Arabic  // 12
	{0x0700, 0x074F},  //  Syriac
	{0x0750, 0x077F},  //  Arabic Supplement
	{0x0780, 0x07BF},  //  Thaana
	{0x0900, 0x097F},  //  Devanagari
	{0x0980, 0x09FF},  //  Bengali
	{0x0A00, 0x0A7F},  //  Gurmukhi
	{0x0A80, 0x0AFF},  //  Gujarati
	{0x0B00, 0x0B7F},  //  Oriya  // 20
	{0x0B80, 0x0BFF},  //  Tamil
	{0x0C00, 0x0C7F},  //  Telugu
	{0x0C80, 0x0CFF},  //  Kannada
	{0x0D00, 0x0D7F},  //  Malayalam  //24
	{0x0D80, 0x0DFF},  //  Sinhala
	{0x0E00, 0x0E7F},  //  Thai  // 26
	{0x0E80, 0x0EFF},  //  Lao
	{0x0F00, 0x0FFF},  //  Tibetan
	{0x1000, 0x109F},  //  Myanmar
	{0x10A0, 0x10FF},  //  Georgian // 30
	{0x1100, 0x11FF},  //  Hangul Jamo
	{0x1200, 0x137F},  //  Ethiopic
	{0x1380, 0x139F},  //  Ethiopic Supplement
	{0x13A0, 0x13FF},  //  Cherokee
	{0x1400, 0x167F},  //  Unified Canadian Aboriginal Syllabics
	{0x1680, 0x169F},  //  Ogham
	{0x16A0, 0x16FF},  //  Runic
	{0x1700, 0x171F},  //  Tagalog // 38
	{0x1720, 0x173F},  //  Hanunoo
	{0x1740, 0x175F},  //  Buhid
	{0x1760, 0x177F},  //  Tagbanwa // 41
	{0x1780, 0x17FF},  //  Khmer
	{0x1800, 0x18AF},  //  Mongolian
	{0x1900, 0x194F},  //  Limbu
	{0x1950, 0x197F},  //  Tai Le  // 45
	{0x1980, 0x19DF},  //  New Tai Lue
	{0x19E0, 0x19FF},  //  Khmer Symbols
	{0x1A00, 0x1A1F},  //  Buginese
	{0x1D00, 0x1D7F},  //  Phonetic Extensions  // 49
	{0x1D80, 0x1DBF},  //  Phonetic Extensions Supplement
	{0x1DC0, 0x1DFF},  //  Combining Diacritical Marks Supplement
	{0x1E00, 0x1EFF},  //  Latin Extended Additional // 52
	{0x1F00, 0x1FFF},  //  Greek Extended
	{0x2000, 0x206F},  //  General Punctuation
	{0x2070, 0x209F},  //  Superscripts and Subscripts // 55
	{0x20A0, 0x20CF},  //  Currency Symbols
	{0x20D0, 0x20FF},  //  Combining Diacritical Marks for Symbols
	{0x2100, 0x214F},  //  Letterlike Symbols
	{0x2150, 0x218F},  //  Number Forms  // 59
	{0x2190, 0x21FF},  //  Arrows
	{0x2200, 0x22FF},  //  Mathematical Operators
	{0x2300, 0x23FF},  //  Miscellaneous Technical
	{0x2400, 0x243F},  //  Control Pictures // 63
	{0x2440, 0x245F},  //  Optical Character Recognition
	{0x2460, 0x24FF},  //  Enclosed Alphanumerics
	{0x2500, 0x257F},  //  Box Drawing
	{0x2580, 0x259F},  //  Block Elements  // 67
	{0x25A0, 0x25FF},  //  Geometric Shapes
	{0x2600, 0x26FF},  //  Miscellaneous Symbols
	{0x2700, 0x27BF},  //  Dingbats
	{0x27C0, 0x27EF},  //  Miscellaneous Mathematical Symbols-A  // 71
	{0x27F0, 0x27FF},  //  Supplemental Arrows-A
	{0x2800, 0x28FF},  //  Braille Patterns
	{0x2900, 0x297F},  //  Supplemental Arrows-B
	{0x2980, 0x29FF},  //  Miscellaneous Mathematical Symbols-B // 75
	{0x2A00, 0x2AFF},  //  Supplemental Mathematical Operators
	{0x2B00, 0x2BFF},  //  Miscellaneous Symbols and Arrows
	{0x2C00, 0x2C5F},  //  Glagolitic
	{0x2C80, 0x2CFF},  //  Coptic  // 79
	{0x2D00, 0x2D2F},  //  Georgian Supplement
	{0x2D30, 0x2D7F},  //  Tifinagh
	{0x2D80, 0x2DDF},  //  Ethiopic Extended
	{0x2E00, 0x2E7F},  //  Supplemental Punctuation  // 83
	{0x2E80, 0x2EFF},  //  CJK Radicals Supplement
	{0x2F00, 0x2FDF},  //  Kangxi Radicals
	{0x2FF0, 0x2FFF},  //  Ideographic Description Characters
	{0x3000, 0x303F},  //  CJK Symbols and Punctuation  // 87
	{0x3040, 0x309F},  //  Hiragana
	{0x30A0, 0x30FF},  //  Katakana
	{0x3100, 0x312F},  //  Bopomofo
	{0x3130, 0x318F},  //  Hangul Compatibility Jamo  // 91
	{0x3190, 0x319F},  //  Kanbun
	{0x31A0, 0x31BF},  //  Bopomofo Extended
	{0x31C0, 0x31EF},  //  CJK Strokes
	{0x31F0, 0x31FF},  //  Katakana Phonetic Extensions // 95
	{0x3200, 0x32FF},  //  Enclosed CJK Letters and Months
	{0x3300, 0x33FF},  //  CJK Compatibility
	{0x3400, 0x4DBF},  //  CJK Unified Ideographs Extension A
	{0x4DC0, 0x4DFF},  //  Yijing Hexagram Symbols // 99
	{0x4E00, 0x9FFF},  //  CJK Unified Ideographs
	{0xA000, 0xA48F},  //  Yi Syllables
	{0xA490, 0xA4CF},  //  Yi Radicals
	{0xA700, 0xA71F},  //  Modifier Tone Letters  // 103
	{0xA800, 0xA82F},  //  Syloti Nagri
	{0xAC00, 0xD7AF},  //  Hangul Syllables
	{0xD800, 0xDB7F},  //  High Surrogates
	{0xDB80, 0xDBFF},  //  High Private Use Surrogates // 107
	{0xDC00, 0xDFFF},  //  Low Surrogates
	{0xE000, 0xF8FF},  //  Private Use Area
	{0xF900, 0xFAFF},  //  CJK Compatibility Ideographs
	{0xFB00, 0xFB4F},  //  Alphabetic Presentation Forms  // 111
	{0xFB50, 0xFDFF},  //  Arabic Presentation Forms-A
	{0xFE00, 0xFE0F},  //  Variation Selectors
	{0xFE10, 0xFE1F},  //  Vertical Forms
	{0xFE20, 0xFE2F},  //  Combining Half Marks  // 115
	{0xFE30, 0xFE4F},  //  CJK Compatibility Forms
	{0xFE50, 0xFE6F},  //  Small Form Variants
	{0xFE70, 0xFEFF},  //  Arabic Presentation Forms-B
	{0xFF00, 0xFFEF},  //  Halfwidth and Fullwidth Forms  // 119
	{0xFFF0, 0xFFFF},  //  Specials
	{0x10000, 0x1007F},  //  Linear B Syllabary
	{0x10080, 0x100FF},  //  Linear B Ideograms
	{0x10100, 0x1013F},  //  Aegean Numbers  // 123
	{0x10140, 0x1018F},  //  Ancient Greek Numbers
	{0x10300, 0x1032F},  //  Old Italic
	{0x10330, 0x1034F},  //  Gothic
	{0x10380, 0x1039F},  //  Ugaritic
	{0x103A0, 0x103DF},  //  Old Persian
	{0x10400, 0x1044F},  //  Deseret  // 129
	{0x10450, 0x1047F},  //  Shavian
	{0x10480, 0x104AF},  //  Osmanya
	{0x10800, 0x1083F},  //  Cypriot Syllabary
	{0x10A00, 0x10A5F},  //  Kharoshthi
	{0x1D000, 0x1D0FF},  //  Byzantine Musical Symbols
	{0x1D100, 0x1D1FF},  //  Musical Symbols  // 135
	{0x1D200, 0x1D24F},  //  Ancient Greek Musical Notation
	{0x1D300, 0x1D35F},  //  Tai Xuan Jing Symbols
	{0x1D400, 0x1D7FF},  //  Mathematical Alphanumeric Symbols
	{0x20000, 0x2A6DF},  //  CJK Unified Ideographs Extension B  // 139
	{0x2F800, 0x2FA1F},  //  CJK Compatibility Ideographs Supplement
	{0xE0000, 0xE007F},  //  Tags
	{0xE0100, 0xE01EF},  //  Variation Selectors Supplement
	{0xF0000, 0xFFFFF},  //  Supplementary Private Use Area-A  // 143
	{0x100000, 0x10FFFF},  //  Supplementary Private Use Area-B

	{0xFFFFFF, 0xFFFFFF} //End
};


const int rangemapping[][8] = {
	{0, -1}, //	Basic Latin
	{1, -1}, //	Latin-1 Supplement
	{2, -1}, //	Latin Extended-A
	{3, -1}, //	Latin Extended-B
	{4, -1}, //	IPA Extensions
	{5, -1}, //	Spacing Modifier Letters
	{6, -1}, //	Combining Diacritical Marks
	{7, -1}, //	Greek and Coptic
	{-1}, //	Reserved for Unicode SubRanges
	{8,  //	Cyrillic
	 9, -1}, // Cyrillic Supplementary
	{10, -1}, //	Armenian
	{11, -1}, //	Hebrew
	{-1}, //	Reserved for Unicode SubRanges
	{12, -1}, //	Arabic
	{-1}, //	Reserved for Unicode SubRanges
	{16, -1}, //	Devanagari
	{17, -1}, // Bengali
	{18, -1}, //	Gurmukhi
	{19, -1}, //	Gujarati
	{20, -1}, //	Oriya
	{21, -1}, //	Tamil
	{22, -1}, //	Telugu
	{23, -1}, //	Kannada
	{24, -1}, //	Malayalam
	{26, -1}, //	Thai
	{27, -1}, //	Lao
	{30, -1}, //	Georgian
	{-1}, //	Reserved for Unicode SubRanges
	{31, -1}, //	Hangul Jamo
	{52, -1}, //	Latin Extended Additional
	{53, -1}, //	Greek Extended
	{54, -1}, //	General Punctuation
	{55, -1}, //	Superscripts And Subscripts
	{56, -1}, //	Currency Symbols
	{57, -1}, //	Combining Diacritical Marks For Symbols
	{58, -1}, //	Letterlike Symbols
	{59, -1}, //	Number Forms
	{60,  //	Arrows
	 72,  //	Supplemental Arrows-A
	 74, -1}, //	Supplemental Arrows-B
	{61,  //	Mathematical Operators
	 76,  //	Supplemental Mathematical Operators
	 71,  //	Miscellaneous Mathematical Symbols-A
	 75, -1}, // 	Miscellaneous Mathematical Symbols-B
	{62, -1}, //	Miscellaneous Technical
	{63, -1}, //	Control Pictures
	{64, -1}, //	Optical Character Recognition
	{65, -1}, //	Enclosed Alphanumerics
	{66, -1}, //	Box Drawing
	{67, -1}, //	Block Elements
	{68, -1}, //	Geometric Shapes
	{69, -1}, //	Miscellaneous Symbols
	{70, -1}, //	Dingbats
	{87, -1}, //	CJK Symbols And Punctuation
	{88, -1}, //	Hiragana
	{89,  //	Katakana
	 95, -1}, //	Katakana Phonetic Extensions
	{90,  //	Bopomofo
	 93, -1}, //	Bopomofo Extended
	{91, -1}, //	Hangul Compatibility Jamo
	{-1}, //	Reserved for Unicode SubRanges
	{96, -1}, //	Enclosed CJK Letters And Months
	{97, -1}, //	CJK Compatibility
	{105, -1},//	Hangul Syllables
	{-1}, //	Non-Plane 0 * (subrange)
	{-1}, //	Reserved for Unicode SubRanges
	{100, //	CJK Unified Ideographs
	 84,  //	CJK Radicals Supplement
	 85,  //	Kangxi Radicals
	 86,  //	Ideographic Description Characters
	 98,  //	CJK Unified Ideograph Extension A
	 139, //	CJK Unified Ideographs Extension B
	 92, -1}, //	Kanbun
	{109, -1}, //	Private Use Area
	{110,  //	CJK Compatibility Ideographs
	 140, -1}, //	CJK Compatibility Ideographs Supplement
	{111, -1}, //	Alphabetic Presentation Forms
	{112, -1}, //	Arabic Presentation Forms-A
	{115, -1}, //	Combining Half Marks
	{116, -1}, //	CJK Compatibility Forms
	{117, -1}, //	Small Form Variants
	{118, -1}, //	Arabic Presentation Forms-B
	{119, -1}, //	Halfwidth And Fullwidth Forms
	{120, -1}, //	Specials
	{28, -1},  //	Tibetan
	{13, -1},  //	Syriac
	{15, -1},  //	Thaana
	{25, -1},  //	Sinhala
	{29, -1},  //	Myanmar
	{32, -1},  //	Ethiopic
	{34, -1},  //	Cherokee
	{35, -1},  //	Unified Canadian Aboriginal Syllabics
	{36, -1},  //	Ogham
	{37, -1},  //	Runic
	{42, -1},  //	Khmer
	{43, -1},  //	Mongolian
	{73, -1},  //	Braille Patterns
	{101,  //	Yi Syllables
	 102, -1}, //	Yi Radicals
	{38, -1},  //	Tagalog
	{39,   //	Hanunoo
	 40,   //	Buhid
	 41, -1},  //	Tagbanwa
	{125, -1}, //	Old Italic
	{126, -1}, //	Gothic
	{129, -1}, //	Deseret
	{134,  //	Byzantine Musical Symbols
	 135, -1}, //	Musical Symbols
	{138, -1}, //	Mathematical Alphanumeric Symbols
	{-1, //	Private Use (plane 15)
	 -1},//	Private Use (plane 16)
	{113, -1}, //	Variation Selectors
	{141, -1}, //	Tags
	{-1},//	93-127	Reserved for Unicode SubRanges

	{-1000}  // End
};

//returns false if no blockinfo was found
static BOOL GetUnicodeBlockInfo(UnicodePoint ch, int &block_no, UnicodePoint &block_lowest, UnicodePoint &block_highest)
{
	int range_idx = 0;
	while (TRUE)
	{
		for (int sub=0; ;++sub)
		{
			int block_idx = rangemapping[range_idx][sub];
			if (block_idx == -1)
				break;

			if (block_idx == -1000)
			{
				return FALSE;
			}

			uint32_t block_start = unicode_blocks[ block_idx ][0];
			uint32_t block_end = unicode_blocks[ block_idx ][1];

			if ( block_start <= ch && ch <= block_end )
			{
				block_no = range_idx;
				block_lowest = block_start;
				block_highest = block_end;
				return TRUE;
			}
		}

		++range_idx;
	}

}

#ifdef MDF_AGFA_WTLE_SUPPORT
// This method checks if the font has a substitution table and the string contains shapeable glyphs
// glyphs in the ranges below. The ranges are preliminary and might need to be revised.
BOOL MDF_AGFAFontEngine::NeedsShaping(MDE_FONT* font, const uni_char* str, int len)
{
	//Arabic is shaped by the Opera internal text shaper.
	unsigned int shaped_blocks[2][2] = {
		{0x0300, 0x036F}, // Combining Diacritical Mark
		{0x0780, 0x13FF}  // Thaana, Devanagari,
                          // Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, Malayalam,
                          // Sinhala, Thai, Tibetan, Myanmar, Georgian, Hangul Jamo, Ethiopic, Cherokee
	};

	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;
	if ( agfa_data->has_substitution_table )
	{
		for (int i = 0; i < len; ++i)
		{
			if (shaped_blocks[0][0] <= str[i]  && str[i] <= shaped_blocks[0][1] ||
				shaped_blocks[1][0] <= str[i]  && str[i] <= shaped_blocks[1][1])
			{
				return true;
			}
		}

	}

	return false;
}
#endif // MDF_AGFA_WTLE_SUPPORT

void MDF_AGFAFontEngine::GetUnicodeRangesFromCMAP(const char* font_name, MDF_FontInformation *font_information)
{
	const int TT_PLATFORM_WINDOWS = 3;

	FS_ULONG tbl_size = 0;
	BYTE* raw_table = FS_get_table(&agfa_fs_state, TAG_cmap, TBL_EXTRACT, &tbl_size);
	CMAPTable cmap(raw_table);
	BOOL cmap_success = cmap.SelectCMAPSubtable(TT_PLATFORM_WINDOWS, 1);
	if (cmap_success)
	{
		unsigned int ranges[4] = {0, 0, 0, 0};

        //Search segments for blocks.
		int block_no = 0;
		UnicodePoint block_lowest = 0;
		UnicodePoint block_highest = 0;
		int segment = 0;
		while (TRUE) //Last segment is always 0xFFFF
		{
			UnicodePoint segment_start = cmap.StartCode( segment );
			UnicodePoint segment_end = cmap.EndCode( segment );

			//Part of this block has not been registered before
			if (segment_start > block_highest || segment_end > block_highest) {

				BOOL block_found = FALSE;
				do
				{
					block_found = GetUnicodeBlockInfo(segment_start, block_no, block_lowest, block_highest);

					if (block_no < 32)
						ranges[0] |= (1 << block_no);
					else if (block_no>=32 && block_no < 64)
						ranges[1] |= 1 << (block_no-32);
					else if (block_no>=64 && block_no < 96)
						ranges[2] |= 1 << (block_no-64);
					else if (block_no>=96 && block_no < 128)
						ranges[3] |= 1 << (block_no-96);

					if (block_highest == 0xffff)
						break;

					segment_start = block_highest + 1;
				} while (segment_start < segment_end //Segment spans several blocks
						 && block_found
						 );
			}

			if(segment_end == 0xFFFF) break;
			++segment;
		}

		font_information->ranges[0] |= ranges[0];
		font_information->ranges[1] |= ranges[1];
		font_information->ranges[2] |= ranges[2];
		font_information->ranges[3] |= ranges[3];
	}
	FS_free_table(&agfa_fs_state, raw_table);
}

MDF_FontGlyph* MDF_AGFAFontEngine::GetChar(uni_char c, MDE_FONT* font, bool mustRender, BOOL isIndex)
{
#ifdef MDF_FONT_GLYPH_CACHE
	MDF_FontGlyphCache *cache = ((AGFA_DATA*)font->private_data)->glyph_cache;
	MDF_FontGlyph* font_glyph;
	OP_STATUS status =	cache->GetChar(font_glyph,  c, font, mustRender, isIndex );
	if (status != OpStatus::OK)
		return NULL;
	else
		return font_glyph;
#else
	MDF_FontGlyph* glyph = OP_NEW(MDF_FontGlyph, ());
	if (!glyph)
		return NULL;
	glyph->c = c;
	if (LoadFontGlyphInt(glyph, font, mustRender, isIndex) != OpStatus::OK)
	{
		OP_DELETE(glyph);
		return NULL;
	}

	if (mustRender)
	{
		glyph->flags |= MDF_GLYPH_RENDERED;
	}

	return glyph;
#endif

}

#ifdef MDF_FONT_GLYPH_CACHE
OP_STATUS MDF_AGFAFontEngine::LoadFontGlyph(MDF_FontGlyph* glyph, MDE_FONT* font, int mustRender)
{
	return LoadFontGlyphInt(glyph, font, mustRender, glyph->flags & MDF_GLYPH_ID_IS_INDEX);
}

OP_STATUS MDF_AGFAFontEngine::BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, BOOL mustRender, BOOL isIndex)
{
	MDF_FontGlyph* cglyph;
	MDF_FontGlyphCache *cache = ((AGFA_DATA*)font->private_data)->glyph_cache;
	RETURN_IF_ERROR(cache->GetChar(cglyph, ch, font, mustRender, isIndex));
	glyph.buffer_data = cglyph->buffer_data;
	glyph.bitmap_left = cglyph->bitmap_left;
	glyph.bitmap_top = cglyph->bitmap_top;
	glyph.advance = cglyph->advance;

	return OpStatus::OK;
}

void MDF_AGFAFontEngine::FreeFontGlyph(MDF_FontGlyph* glyph)
{
	FreeFontGlyphInt(glyph);
}

unsigned short MDF_AGFAFontEngine::GetGlyphCacheSize(MDE_FONT* font)  const
{
	return ((AGFA_DATA*)font->private_data)->glyph_cache->GetSize();
}

BOOL MDF_AGFAFontEngine::GetGlyphCacheFull(MDE_FONT* font) const
{
	return ((AGFA_DATA*)font->private_data)->glyph_cache->IsFull();
}

unsigned short MDF_AGFAFontEngine::GetGlyphCacheFill(MDE_FONT* font) const
{
	return ((AGFA_DATA*)font->private_data)->glyph_cache->GetFill();
}

#endif // MDF_FONT_GLYPH_CACHE


OP_STATUS MDF_AGFAFontEngine::LoadFontGlyphInt(MDF_FontGlyph* glyph, MDE_FONT* font, int mustRender, BOOL isIndex)
{
	if (OpStatus::IsError(SetAGFAFont(font)))
		return OpStatus::ERR;

	if (!mustRender)
	{
		if (isIndex)
			FS_set_flags(&agfa_fs_state, FLAGS_CMAP_OFF);

#if (MAJOR_VERSION >= 4) && (MINOR_VERSION >= 101)
		FS_SHORT i_dx, i_dy;
		FS_FIXED dx, dy;
		FS_LONG stat = FS_get_advance(&agfa_fs_state,
									  glyph->c,
									  FS_MAP_ANY_GRAYMAP,
									  &i_dx,
									  &i_dy,
									  &dx,
									  &dy);
		if (stat != SUCCESS)
			return OpStatus::ERR;

		if (i_dx != 0)
			glyph->advance = i_dx;
		else
			glyph->advance = dx/65536;
#else // Old way to before iType 4.1.1
		FS_OUTLINE *outline = FS_get_outline(&agfa_fs_state, glyph->c);
		if (!outline)
		{
			return OpStatus::ERR;
		}
		if (outline->i_dx != 0)
			glyph->advance = outline->i_dx;
		else {

			glyph->advance = outline->dx/65536;
			glyph->advance += ((((outline->dx & 0x00ff)>>15) < 5) ? 0:1);
		}
		FS_free_char(&agfa_fs_state, outline);
#endif
		if (isIndex)
			FS_set_flags(&agfa_fs_state, FLAGS_CMAP_ON);

		return OpStatus::OK;
	}

	MDF_GLYPH mdfglyph;
	OP_STATUS status = GetGlyph(mdfglyph, font, glyph->c, mustRender, isIndex);
	if (OpStatus::IsError(status))
	{
		return status;
	}

	glyph->buffer_data = mdfglyph.buffer_data;
	glyph->bitmap_left = mdfglyph.bitmap_left;
	glyph->bitmap_top = mdfglyph.bitmap_top;
	glyph->advance = mdfglyph.advance;

	return OpStatus::OK;
}

void MDF_AGFAFontEngine::FreeFontGlyphInt(MDF_FontGlyph* glyph)
{
#ifdef MDF_FONT_GLYPH_CACHE
	if (glyph->flags & MDF_FontGlyph::GlyphRendered)
#else
	if (glyph->flags & MDF_GLYPH_RENDERED)
#endif // MDF_FONT_GLYPH_CACHE
	{
		OP_DELETEA(static_cast<UINT8*>(glyph->buffer_data->data));
		OP_DELETE(glyph->buffer_data);
		glyph->buffer_data = NULL;
	}
#ifndef MDF_FONT_GLYPH_CACHE
	OP_DELETE(glyph);
#endif
}


void MDF_AGFAFontEngine::GetFontPropertiesAndCodepageInfo(MDF_FontInformation *font_information)
{
	OP_NEW_DBG("MDF_AGFAFontEngine::GetFontPropertiesAndCodepageInfo", "agfa");
	TTF_OS2 os2;
	if (FS_get_table_structure(&agfa_fs_state, TAG_OS2, &os2) != SUCCESS)
		return;

	FS_BYTE classID = (os2.sFamilyClass & 0xff00) >> 8;

    // OTSPEC: IBM Font Class Parameters
	OP_DBG(("sFamilyClass: %x\n", os2.sFamilyClass));
	switch (classID) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 7:
			font_information->has_serif = MDF_SERIF_SERIF;
			break;
		case 8:
			font_information->has_serif = MDF_SERIF_SANS;
			break;
		default:
			font_information->has_serif = MDF_SERIF_UNDEFINED;
			break;
	}

#ifdef DEBUG
    // http://www.microsoft.com/typography/otspec/os2.htm#fss
	OP_DBG(("fsSelection: %x\n", os2.fsSelection));
	OP_ASSERT(os2.fsSelection);
	if (os2.fsSelection & 0x1)
		OP_DBG(("ITALIC\n"));
	if (os2.fsSelection & 0x2)
		OP_DBG(("UNDERSCORE\n"));
	if (os2.fsSelection & 0x4)
		OP_DBG(("NEGATIVE\n"));
	if (os2.fsSelection & 0x8)
		OP_DBG(("OUTLINE\n"));
	if (os2.fsSelection & 0x10)
		OP_DBG(("STRIKEOUT\n"));
	if (os2.fsSelection & 0x20)
		OP_DBG(("BOLD\n"));
	if (os2.fsSelection & 0x40)
		OP_DBG(("REGULAR\n"));
	if (os2.fsSelection & 0x80)
		OP_DBG(("USE_TYPO_METRICS\n"));
	if (os2.fsSelection & 0x100)
		OP_DBG(("WWS\n"));
	if (os2.fsSelection & 0x200)
		OP_DBG(("OBLIQUE\n"));
#endif //DEBUG


// Panose table (http://www.panose.com/ProductsServices/pan1.aspx)
	OP_DBG(("Panose0: %i\n", os2.panose[0]));
	OP_DBG(("Panose1: %i\n", os2.panose[1]));
	OP_DBG(("Panose2: %i\n", os2.panose[2]));
	OP_DBG(("Panose3: %i\n", os2.panose[3]));
	OP_DBG(("Panose4: %i\n", os2.panose[4]));
	OP_DBG(("Panose5: %i\n", os2.panose[5]));
	OP_DBG(("Panose6: %i\n", os2.panose[6]));
	OP_DBG(("Panose7: %i\n", os2.panose[7]));
	OP_DBG(("Panose8: %i\n", os2.panose[8]));
	OP_DBG(("Panose9: %i\n", os2.panose[9]));
	// Latin Text
	if (os2.panose[0] == 2) {

#ifdef DEBUG
		int serif;
		switch (os2.panose[1]) {
		case 0:  // 0-Any
		case 1:  // 1-No Fit
			serif = -1;
			break;
		case 2:  // 2-Cove
		case 3:  // 3-Obtuse Cove
		case 4:  // 4-Square Cove
		case 5:  // 5-Obtuse Square Cove
		case 6:  // 6-Square
		case 7:  // 7-Thin
		case 8:  // 8-Oval
		case 9:  // 9-Exaggerated
		case 10: // 10-Triangle
			serif = 1;
			break;
		case 11: // 11-Normal Sans
		case 12: // 12-Obtuse Sans
		case 13: // 13-Perpendicular Sans
		case 14: // 14-Flared
			serif = 0;
			break;
		case 15: // 15-Rounded
			serif = 1;
			break;
		default:
			OP_ASSERT(!"No Panose serif info");
		}
		if (((serif == 1) && (font_information->has_serif != MDF_SERIF_SERIF))
			|| ((serif == 0) && (font_information->has_serif != MDF_SERIF_SANS))
			|| ((serif == -1) && (font_information->has_serif != MDF_SERIF_UNDEFINED)))
			OP_DBG(("Serif detection problem: %i %i\n", serif, font_information->has_serif));
#endif // DEBUG

		// Properties (monospaced)
		if (os2.panose[3] == 9)
			font_information->is_monospace = true;

	}

	// Latin Hand written
	if (os2.panose[0] == 3) {
		// Properties (monospaced)
		if (os2.panose[3] == 3)
			font_information->is_monospace = true;
	}

	// Latin Decoratives
	if (os2.panose[0] == 4) {
		// Properties (monospaced)
		if (os2.panose[3] == 9)
			font_information->is_monospace = true;
	}

	// Latin Symbol
	if (os2.panose[0] == 5) {
		// Properties (monospaced)
		if (os2.panose[3] == 3)
			font_information->is_monospace = true;
	}

/* // Use the CMAP unicode ranges
  // These unicode ranges only contain a subset of all glyphs actually
  // in the font.
	font_information->ranges[0] = os2.ulUnicodeRange1;
	font_information->ranges[1] = os2.ulUnicodeRange2;
	font_information->ranges[2] = os2.ulUnicodeRange3;
	font_information->ranges[3] = os2.ulUnicodeRange4;

*/
#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	font_information->m_codepages[0] = os2.ulCodePageRange1;
	font_information->m_codepages[1] = os2.ulCodePageRange2;

	OP_DBG(("codepage1: %x\n", font_information->m_codepages[0]));
	OP_DBG(("codepage2: %x\n", font_information->m_codepages[1]));
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
}


#ifdef MDF_SFNT_TABLES

OP_STATUS MDF_AGFAFontEngine::GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size)
{
    //Activate font
	RETURN_IF_ERROR(SetAGFAFont(font));

	return GetTableInt(tab_tag, tab, size);
}

OP_STATUS MDF_AGFAFontEngine::GetTableInt(unsigned long tab_tag, BYTE*& tab, UINT32& size)
{
	tab = 0;
	size = 0;

    //Extract the cmap table from AGFA engine
	FS_ULONG tbl_size = 0;
	FS_BYTE* raw_table = FS_get_table(&agfa_fs_state, tab_tag, TBL_EXTRACT, &tbl_size);
	if (!raw_table)
	{
		const int WINDOWS_PLATFORM = 3;
		//Reset the agfa_fs_state.
		FS_set_cmap(&agfa_fs_state, WINDOWS_PLATFORM, 1);
		return OpStatus::ERR;
	}

	tab = OP_NEWA(FS_BYTE, tbl_size);
	if (tab)
	{
		size = (UINT32)tbl_size;
		op_memcpy(tab, raw_table, size);
	}
	int res = FS_free_table(&agfa_fs_state, raw_table);
	OP_ASSERT(res == SUCCESS);
	return tab ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

void MDF_AGFAFontEngine::FreeTable(byte* tab)
{
	OP_DELETEA(tab);
}
#endif // MDF_SFNT_TABLES

#ifdef MDF_FONT_FACE_CACHE
OP_STATUS MDF_AGFAFontEngine::LoadFontFace(MDF_FontFace** face, const char* font_name, int index)
{
	OP_ASSERT(!"FIXME: not implemented");
	return OpStatus::ERR;
}
#endif // MDF_FONT_FACE_CACHE

#ifdef MDF_AGFA_WTLE_SUPPORT

WTLE_Layout::WTLE_Layout()
		: m_charStr(NULL),
		  m_layoutEngine(NULL),
		  m_glyphStr(NULL)
{

}

OP_STATUS WTLE_Layout::Construct(MDE_FONT* font, const uni_char* str, int len, WTLE_LayoutEngine* layoutEngine)
{
	m_layoutEngine = layoutEngine;
	m_charStr = static_cast<void*>(const_cast<uni_char*>(str));

	TsResult ok = TsText_insertRun(	m_layoutEngine->layoutText(),
									m_charStr,
									len,
									TS_UTF16,
									NULL );
	if (TS_OK != ok)
	{
		m_charStr = NULL;
 		return OpStatus::ERR;
	}

	AGFA_DATA* agfa_data = (AGFA_DATA*)font->private_data;
	ok = TsText_setFontStyle( layoutEngine->layoutText(),
							  agfa_data->wtle_font_style,
							  TS_START_OF_TEXT,
							  TS_END_OF_TEXT );

	if (TS_OK != ok)
	{
 		return OpStatus::ERR;
	}

	return OpStatus::OK;

}

WTLE_Layout::~WTLE_Layout()
{
	CleanUp();
}

void WTLE_Layout::CleanUp()
{
	if (m_charStr != NULL)
	{
		TsText_removeRun(m_layoutEngine->layoutText(), m_charStr);
		m_charStr = NULL;
	}

	OP_DELETEA(m_glyphStr);
	m_glyphStr = NULL;
}


OP_STATUS  WTLE_Layout::Compose()
{
	OP_NEW_DBG("WTLE_Layout::Compose", "agfa");
	//We are only using the text shaping functionality of WTLE so the layout is never
	//composed.
	TsResult ok = TsLayout_composeLineInit(m_layoutEngine->layout());
	if (TS_OK != ok )
	{
		OP_DBG(("TsLayout_composeLineInit falied!\n"));
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS WTLE_Layout::GetProcessedString(ProcessedString& processed_string,
										  const int extra_space/* = 0*/, const short word_width/* = -1*/)
{
	const int len = TsLayout_getNrOfGlyphs(m_layoutEngine->layout());
	OP_ASSERT(len >= 0); // can this ever happen?
	if (len == 0)
	{
		processed_string.m_length = 0;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(font->engine->glyph_buffer.Grow(len));
	ProcessedGlyph* glyphs = font->engine->glyph_buffer.Storage();

	size_t out_len = 0;
	for (size_t i = 0; i < len; ++i)
	{
		MDF_FontGlyph *cc = GetChar( ch, font, false, isIndex );
		if (cc)
		{
			// FIXME: this is stupid, we should use the data in the
			// layout struct for glyph placement instead (and set
			// proceesed_string.m_top_left_positioned = TRUE)
			glyphs[out_len].m_pos.Set(px, font->ascent);
			glyphs[out_len].m_id = TsLayout_getGlyph(m_layoutEngine->layout(), i);
			px += cc.advance + extra_space;
			++ out_len;
		}
	}

	processed_string.m_top_left_positioned = FALSE;
	processed_string.m_is_glyph_indices = TRUE;
	processed_string.m_length = out_len;
	processed_string.m_processed_glyphs = glyphs;
	processed_string.m_advance = px;

#ifdef MDF_KERNING
	RETURN_IF_ERROR(ApplyKerning(font, processed_string));
#endif // MDF_KERNING

	if (word_width != -1 && word_width != processed_string.m_advance)
	{
		AdjustToWidth(processed_string, word_width);
		OP_ASSERT(word_width == processed_string.m_advance);
	}

	return OpStatus::OK;
}

OP_STATUS WTLE_Layout::GetGlyphs(uni_char** glyphs, int* len)
{
	int numGlyphs = TsLayout_getNrOfGlyphs(m_layoutEngine->layout());
	if (numGlyphs == 0)
	{
		*len = 0;
		*glyphs = NULL;
		return OpStatus::OK;
	}

	OP_DELETEA(m_glyphStr);
	m_glyphStr = OP_NEWA(uni_char,  numGlyphs );
	if (m_glyphStr == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	//Populate the glyphStr with glyphs
	int strIndex = 0;
	for (int glyphIndex = 0; glyphIndex < numGlyphs; ++glyphIndex)
	{
		m_glyphStr[strIndex] = TsLayout_getGlyph(m_layoutEngine->layout(), glyphIndex);
		++strIndex;
	}

	*glyphs = m_glyphStr;
	*len = numGlyphs;

	return OpStatus::OK;
}


WTLE_LayoutEngine::WTLE_LayoutEngine()
		: m_layoutControl(NULL),
		  m_fontEngineMgr(NULL),
		  m_fontMgr(NULL),
		  m_source(NULL),
		  m_layout(NULL)
{

}

WTLE_LayoutEngine::~WTLE_LayoutEngine()
{
	CleanUp();
}


OP_STATUS WTLE_LayoutEngine::Construct(FS_STATE& agfa_fs_state)
{
	OP_NEW_DBG("WTLE_LayoutEngine::Construct", "agfa");

	m_layoutControl = TsLayoutControl_new();

	if (m_layoutControl == NULL)
	{
		OP_DBG(("WTLE: Error creating layout control!\n"));
		return OpStatus::ERR_NO_MEMORY;
	}

	m_fontEngineMgr = TsFontEngineMgr_new();
	if (m_fontEngineMgr == NULL)
	{
		OP_DBG(("WTLE: Error creating font engine!\n"));
		CleanUp();
		return OpStatus::ERR_NO_MEMORY;
	}

	TsResult ok = TsFontEngineMgr_addAttached_iType(m_fontEngineMgr, static_cast<void*>(&agfa_fs_state), 0);
	if (ok != 0)
	{
		OP_DBG(("WTLE: Error attaching font engine!\n"));
		CleanUp();
		return OpStatus::ERR_NO_MEMORY;
	}

	m_fontMgr = TsFontMgr_new(m_fontEngineMgr);
	if (m_fontMgr == NULL)
	{
		OP_DBG(("WTLE: Error creating font manager!\n"));
		CleanUp();
		return OpStatus::ERR_NO_MEMORY;
	}

	m_source = TsText_new();
	if (m_source == NULL)
	{
		OP_DBG(("WTLE: Error creating text object!\n"));
		CleanUp();
		return OpStatus::ERR_NO_MEMORY;
	}


	TsLayoutOptions* layoutOptions = TsLayoutOptions_new();
	if (layoutOptions == NULL)
	{
		OP_DBG(("WTLE: Error creating layout options!\n"));
		CleanUp();
		return OpStatus::ERR_NO_MEMORY;
	}

	// We don't want BIDI since the opera core is doing this.
	TsLayoutOptions_set(layoutOptions, TS_DISABLE_BIDI, true);

	m_layout = 	TsLayout_new(m_source, m_layoutControl, layoutOptions);
	if (m_layout == NULL)
	{
		TsLayoutOptions_delete(layoutOptions);
		CleanUp();
		return OpStatus::ERR_NO_MEMORY;
	}

	TsLayoutOptions_delete(layoutOptions);

	return OpStatus::OK;
}

void WTLE_LayoutEngine::CleanUp()
{
	if (m_fontEngineMgr != NULL)
	{
		TsFontEngineMgr_delete(m_fontEngineMgr);
	}

	if (m_layoutControl != NULL)
	{
		TsLayoutControl_delete(m_layoutControl);
	}

	if (m_fontMgr != NULL)
	{
		TsFontMgr_delete(m_fontMgr);
	}

	if (m_source != NULL)
	{
		TsText_delete(m_source);
	}

	if (m_layout != NULL)
	{
		TsLayout_delete(m_layout);
	}
}

OP_STATUS WTLE_LayoutEngine::InitLayout(WTLE_Layout* layout, MDE_FONT* font, const uni_char* str, int len)
{
	return layout->Construct(font, str, len, this);
}

OP_STATUS WTLE_LayoutEngine::AttachFont(AGFA_DATA* agfa_data, const char* font_name)
{
   	TsFont *wtle_font = TsFontMgr_getFontByName(m_fontMgr, (FILECHAR*)font_name);

	TsFontParam param;
	TsFontParam_defaults(&param);
	param.size = TsFixed_init(agfa_data->size);
	TsFontStyle* fontStyle = TsFont_createFontStyle(wtle_font, &param);
	if (fontStyle == NULL)
	{
		return OpStatus::ERR;
	}

	agfa_data->wtle_font_style = fontStyle;

	return OpStatus::OK;
}


OP_STATUS WTLE_LayoutEngine::InsertFont(void* addr, int size, int index)
{
	OP_NEW_DBG("WTLE_LayoutEngine::InsertFont", "agfa");
	TsFontSpec font_spec;
	TsResult ok = TsResource_initMem(&font_spec.resource, addr, size);
	if (TS_OK != ok)
	{
			return OpStatus::ERR;
	}
	font_spec.index = index;

	TsFont *font = TsFontMgr_getFont(m_fontMgr, font_spec);
	if (font == NULL)
	{
		OP_DBG(("Error inserting font into layoutengine\n"));
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

#endif 	//MDF_AGFA_WTLE_SUPPORT

#endif // MDF_AGFA_SUPPORT

#endif // MDEFONT_MODULE
