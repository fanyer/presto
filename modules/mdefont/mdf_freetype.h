/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef MDF_FREETYPE_H
#define MDF_FREETYPE_H

#include "modules/mdefont/mdefont.h"

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
# define COMMIT_LOCALIZED_FONT_NAMES
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_LibraryRec_* FT_Library;
typedef struct  FT_Bitmap_ FT_Bitmap;

class MDF_FTFontEngine : public MDF_FontEngine
{
public:

#ifdef SELFTEST
	friend class ST_mdefontmdf_freetype;
#endif // SELFTEST

	MDF_FTFontEngine();
	virtual ~MDF_FTFontEngine();
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

#ifdef MDF_FONT_SMOOTHING
	virtual BOOL HasComponentRendering() { return smoothing != NONE; }
#endif // MDF_FONT_SMOOTHING

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

	virtual MDE_FONT* GetFont(int font_nr, int size, BOOL bold, BOOL italic);
	virtual OP_STATUS GetLocalFont(MDF_WebFontBase*& webfont, const uni_char* facename);
	virtual MDE_FONT* GetWebFont(MDF_WebFontBase* webfont, int size);
	virtual MDE_FONT* GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size);

	virtual void ReleaseFont(MDE_FONT* font);
	virtual OP_STATUS UCPToGID(MDE_FONT* font, ProcessedString& processed_string);
	virtual UINT32 GetCharIndex(MDE_FONT* font, const UnicodePoint ch);
	virtual UINT32 GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos);
	virtual void* GetFontImplementation(MDE_FONT* font);
#ifdef MDF_SFNT_TABLES
	virtual OP_STATUS GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size);
	virtual void FreeTable(BYTE* tab);
#endif // MDF_SFNT_TABLES

	virtual OP_STATUS LoadFontFace(MDF_FontFace** face, const uni_char* font_name, int index);
#ifdef MDF_FONT_GLYPH_CACHE
	virtual OP_STATUS LoadFontGlyph(MDF_FontGlyph* glyph, MDE_FONT* font, BOOL mustRender);
	virtual void FreeFontGlyph(MDF_FontGlyph* glyph);
	virtual OP_STATUS ResizeGlyphCache(MDE_FONT* font, int new_size);
	virtual unsigned short GetGlyphCacheSize(MDE_FONT* font) const;
	virtual BOOL GlyphCacheFull(MDE_FONT* font) const;
	virtual unsigned short GetGlyphCacheFill(const MDE_FONT* font) const;
#endif // MDF_FONT_GLYPH_CACHE
	OP_STATUS LoadFontGlyph(MDF_GLYPH& glyph, const UnicodePoint c, MDE_FONT* font, BOOL& render, BOOL isIndex = FALSE, const UINT32 stride = 0);

	OP_STATUS GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender, BOOL isIndex = FALSE);
	void FreeGlyph(MDF_GLYPH& glyph);
	OP_STATUS FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, const UINT32 stride, BOOL isIndex = FALSE);
#ifdef MDF_FONT_GLYPH_CACHE
	OP_STATUS BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender, BOOL isIndex = FALSE);
#endif // MDF_FONT_GLYPH_CACHE

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(MDE_FONT* font, const UINT32 g_id, const UINT32 prev_g_id, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath* out_glyph);
#endif // SVG_SUPPORT

protected:
	struct MDF_FontFileNameInfo
	{
		MDF_FontFileNameInfo()
			: next(0),
			  file_name(0),
			  fixed_sizes(0),
			  num_fixed_sizes(0),
			  font_index(0),
			  bit_field(0)
			{
			};
		enum {
			IS_BOLD			= 0x01,
			IS_ITALIC		= 0x02,
			BOLDIFY         = 0x04,
			APPLY_ITALICS	= 0x08,
			ONLY_BITMAP     = 0x10
		};
		MDF_FontFileNameInfo* next;
		uni_char* file_name;
		int* fixed_sizes;
		int num_fixed_sizes;
		int font_index;
		UINT8 bit_field;

#ifdef PI_WEBFONT_OPAQUE
		AutoFaceNameHashTable local_names;
#endif // PI_WEBFONT_OPAQUE
	};

	struct MDF_FontInformation;
	friend struct MDF_FTFontEngine::MDF_FontInformation;
	struct MDF_FontInformation
	{
		enum {
			IS_MONOSPACE	= 0x01,
			IS_SMALLCAPS	= 0x02,
			HAS_SCALABLE	= 0x04,
			HAS_NONSCALABLE	= 0x08,
			IS_WEBFONT		= 0x10
		};
		MDF_FontFileNameInfo* file_name_list;
		uni_char* font_name;
		MDF_SERIF has_serif;
		UINT32 ranges[4];
		UINT8 bit_field;
	
#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
		UINT32 m_codepages[2];
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
	};

	// This is our OpWebFontRef implementation
	struct MDF_WebFontImpl;
	friend struct MDF_FTFontEngine::MDF_WebFontImpl;
	struct MDF_WebFontImpl : public MDF_WebFontBase
	{
		int						font_array_index; // for faster removal of webfonts
		MDF_FontInformation*	family;
		MDF_FontFileNameInfo*	face;
	};

	struct MDF_FT_FONT;
	friend struct MDF_FTFontEngine::MDF_FT_FONT;
	struct MDF_FT_FONT
	{
		MDF_FT_FONT()
#ifdef MDF_FONT_GLYPH_CACHE
			: glyph_cache(NULL)
#endif // MDF_FONT_GLYPH_CACHE
		{}
		~MDF_FT_FONT()
		{
#ifdef MDF_FONT_GLYPH_CACHE
			OP_DELETE(glyph_cache);
#endif // MDF_FONT_GLYPH_CACHE
		}
		int font_size;
#ifdef MDF_FONT_GLYPH_CACHE
		MDF_FontGlyphCache* glyph_cache;
#endif // MDF_FONT_GLYPH_CACHE
		MDF_FontFileNameInfo* file_name_info;
		const uni_char* font_name;
		int load_flags;
	};

	MDF_FontFileNameInfo* GetSynthesizedFileInfo(MDF_FontInformation* font_information, MDF_FontFileNameInfo* base_file_info, BOOL boldify, BOOL italicize);

#ifdef MDF_SFNT_TABLES
	OP_STATUS GetTableInt(FT_Face face, unsigned long tab_tag, BYTE*& tab, UINT32& size);
#endif // MDF_SFNT_TABLES

	OP_STATUS AddFontFileInternal(const uni_char* filename, MDF_WebFontBase** webfont = NULL);

#ifdef MDF_FONT_SMOOTHING
	void LCDToRGB(const unsigned char* source, UINT32* dest, UINT32 component_stride);
	/** Same as BlitGlyph(), but only handles the cases where no
	 * subpixel rendering is involved.
	 */
	OP_STATUS BlitGlyphNoSubpixels(const FT_Bitmap & source_bitmap, const MDF_GLYPH_BUFFER* dest_buffer, UINT32 dest_stride, BOOL scalable);
#endif // MDF_FONT_SMOOTHING
	/** Copy glyph data from 'source_bitmap' to 'dest_buffer',
	 * potentially doing some transformations on the way.
	 *
	 * The precise behaviour is mostly magical, but some of it is
	 * documented at the top of the implementation of the method.
	 */
	OP_STATUS BlitGlyph(const FT_Bitmap & source_bitmap, const MDF_GLYPH_BUFFER* dest_buffer, UINT32 dest_stride, BOOL scalable);

	MDE_FONT* GetFontInternal(MDF_FontFileNameInfo* file_name_info, int font_nr, int size);
	MDE_FONT* GetFontInternal(MDF_FontFileNameInfo* file_name_info, MDF_FontInformation* font_information, int size);
	OP_STATUS GetFontInfoInternal(MDF_FontInformation* font, MDF_FONTINFO* font_info);

	OP_STATUS FindFontInformation(const char* name, MDF_FontInformation*& fi, int* font_array_index = NULL);
	OP_STATUS AddFontInformation(unsigned int font_number, MDF_FontInformation* font_information);
	MDF_FontFileNameInfo* FindFileNameInfo(int font_nr, int size, BOOL bold, BOOL italic);
	static MDF_FontFileNameInfo* GetBestNonscalableFontFileNameInfo(MDF_FontInformation* font_information, int wanted_size, BOOL bold, BOOL italic, int& actual_size);
	static MDF_FontFileNameInfo* CreateFontFileNameInfo(FT_Face face, const uni_char* file_name, int font_index);
	static void GetBoldItalic(const char* style, UINT8& bit_field);
	static void FreeFontInformationFunc(const void *item, void *o);
	static void FreeFontInformation(MDF_FontInformation* font_information);
	static void FreeFontFileNameInfo(MDF_FontFileNameInfo* file_name_info);
	static BOOL FontInformationHasBoldItalic(MDF_FontInformation* font_information, MDF_FontFileNameInfo* file_name_info);
	static void GetFontInfo(FT_Face face, MDF_FontInformation& fi);
	static MDF_FontInformation* CreateFontInformation(FT_Face face, const char* name, int name_len);
	static BOOL HasMask(MDF_FontInformation* font_information, const UINT8 mask, const UINT8 val);
	static BOOL HasNormal(MDF_FontInformation* font_information);
	static BOOL HasBold(MDF_FontInformation* font_information);
	static BOOL HasItalic(MDF_FontInformation* font_information);
	static BOOL HasBoldItalic(MDF_FontInformation* font_information);

	/** Internal function to request a font face from the cache, and if found set the requested font size. */
	OP_STATUS GetFontFace(FT_Face& face, const uni_char* font_name, int index);
	OP_STATUS GetFontFace(FT_Face& face, const MDF_FontFileNameInfo* info, int wanted_size);

	MDF_FontFaceCache* face_cache;
	OpINT32HashTable<MDF_FontInformation> font_array;
	FT_Library ft_library;

	int system_font_count;
	int default_load_flags;

#ifdef MDF_FONT_SMOOTHING
	enum LcdSmoothing
	{
		NONE,
		RGB,
		BGR,
		VRGB,
		VBGR
	};
	LcdSmoothing smoothing;
#endif // MDF_FONT_SMOOTHING

#ifdef COMMIT_LOCALIZED_FONT_NAMES
	// scans the FT_Face for localized font names and adds them to the hash in DisplayModule
	OP_STATUS CommitLocalizedNames(FT_Face face, const uni_char* western_name);
#endif // COMMIT_LOCALIZED_FONT_NAMES

#ifdef _DEBUG
private:
	virtual const uni_char* DebugGetFaceName(MDE_FONT* font);
	virtual const uni_char* DebugGetFileName(MDE_FONT* font);
#endif // _DEBUG
};

#endif // MDF_FREETYPE_H
