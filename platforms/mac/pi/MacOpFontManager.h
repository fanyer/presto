#ifndef MacOpFontManager_H
#define MacOpFontManager_H

#include "modules/pi/OpFont.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "platforms/mac/util/MachOCompatibility.h"
#endif

#ifdef WEBFONTS_SUPPORT
#include "modules/util/opfile/opfile.h"

class WebFontContainer
{
public:
	WebFontContainer(const uni_char* path, UINT32 fontno);
	WebFontContainer(ATSFontRef font, CTFontRef ctFont, UINT32 fontno);
	~WebFontContainer();
	
	UINT32                 GetFontNumber() {return fontnumber;}
	ATSFontRef             GetFontRef() {return font;}
	CTFontRef              GetCTFontRef() {return ctFont;}
	const uint32          *GetCharacterMap();
	const FONTSIGNATURE   *GetFontSignatures() { return &signature; }
	OpFontInfo::FontSerifs GetSerifs() { return serifs; }
	Float32 ItalicAngle() { return italic; }
private:
	ATSFontContainerRef    fontcontainer;
	ATSFontRef             font;
	CTFontRef              ctFont;
	OpFile                 file;
	UINT32                 fontnumber;
	void*                  data;
	uint32                *characterMap;
	FONTSIGNATURE          signature;
	OpFontInfo::FontSerifs serifs;
	Float32 italic;
};
#endif // WEBFONTS_SUPPORT

class MacFontIntern;

#if !defined(MDEFONT_MODULE)
/**
 growable buffer used to hold a processed string
 */
class MDF_ProcessedGlyphBuffer
{
public:
	MDF_ProcessedGlyphBuffer() : m_size(0), m_buffer(0) {}
	~MDF_ProcessedGlyphBuffer();
	/**
	 grows the buffer
	 @param size the number of ProcessedGlyph elements to grow to - noop if Capacity >= size
	 @param copy_contents if TRUE, copy contents of old buffer if buffer grew
	 @return
	 OpStatus::OK if buffer growth succeeded or was not necessary - Capacity will be at least size from now on
	 OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Grow(const size_t size, BOOL copy_contents = FALSE);
	/**
	 @return the number of ProcessedGlyphs the buffer can hold
	 */
	size_t Capacity() const { return m_size; }
	/**
	 @return a pointer to the storage
	 */
	struct ProcessedGlyph* Storage() { return m_buffer; }
	
private:
	size_t m_size;
	struct ProcessedGlyph* m_buffer;
};

#endif /* !defined(MDEFONT_MODULE) */

class MacOpFontManager : public VEGAOpFontManager
{
public:
			MacOpFontManager();
	virtual ~MacOpFontManager();

	virtual VEGAFont* GetVegaFont(const uni_char* face, UINT32 size, UINT8 preferWeight, BOOL preferItalic, BOOL must_have_getoutline, INT32 blur_radius);

#ifdef WEBFONTS_SUPPORT
	virtual OP_STATUS GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo);
	virtual VEGAFont* GetVegaFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius);
	virtual VEGAFont* GetVegaFont(OpWebFontRef webfont, UINT8 preferWeight, BOOL preferItalic, UINT32 size, INT32 blur_radius);
	virtual OP_STATUS AddWebFont(OpWebFontRef &webfont, const uni_char* full_path_of_file);
	virtual OP_STATUS RemoveWebFont(OpWebFontRef webfont);
	virtual OP_STATUS GetLocalFont(OpWebFontRef& localfont, const uni_char* facename);
	virtual BOOL SupportsFormat(int format);
#endif // WEBFONTS_SUPPORT

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

	/** Called before the StyleManager is starting to use font
	 * information to set up default fonts. Can be used by platforms
	 * to correct faulty information in fonts.
	*/
	virtual void BeforeStyleInit(class StyleManager* styl_man) { }

	/** Gets the name of the font that is default for the GenericFont.
	 */
#ifdef PERSCRIPT_GENERIC_FONT
	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script = WritingSystem::Unknown);
#else
	virtual const uni_char* GetGenericFontName(GenericFont generic_font);
#endif
	ATSFontRef GetFontIdentifier(UINT32 fontnr);

#ifdef PLATFORM_FONTSWITCHING
	/**
	 * Set the preferred font for a specific unicode block
	 * @param ch a character contained by the actual unicode block. This can be the
	 * first character in the block (like number 0x0370 for Greek), for example.
	 * @param monospace is the specified font supposed to be a replacement for
	 * monospace fonts?
	 */
	virtual void SetPreferredFont(uni_char ch, BOOL monospace, const uni_char *font,
								  OpFontInfo::CodePage codepage=OpFontInfo::OP_DEFAULT_CODEPAGE);
#endif // PLATFORM_FONTSWITCHING
	
#ifdef _GLYPHTESTING_SUPPORT_
	virtual void UpdateGlyphMask(OpFontInfo *fontinfo);
#endif
#ifdef PI_UPDATE_SERIFS
	virtual void UpdateSerifness(OpFontInfo *fontinfo);
#endif
	
	int         GetAntialisingTreshhold();
#if !defined(MDEFONT_MODULE)
	MDF_ProcessedGlyphBuffer& GetGlyphBuffer() { return m_glyph_buffer; }
#endif
	
private:

#ifdef PERSCRIPT_GENERIC_FONT
	void ConsiderGenericFont(GenericFont generic_font, WritingSystem::Script script, const uni_char* name, int score);
#endif
	void ConsiderGenericFont(GenericFont generic_font, const uni_char* name, int score);

#ifdef PERSCRIPT_GENERIC_FONT
	struct GenericFontInfo {
		OpString name;
		WritingSystem::Script script;
		int score;
	};
	OpAutoVector<GenericFontInfo>mGenericSerifFonts;
	OpAutoVector<GenericFontInfo>mGenericSansSerifFonts;
	OpAutoVector<GenericFontInfo>mGenericCursiveFonts;
	OpAutoVector<GenericFontInfo>mGenericFantasyFonts;
	OpAutoVector<GenericFontInfo>mGenericMonospaceFonts;
#else
	OpString mGenericSerifName;
	OpString mGenericSansSerifName;
	OpString mGenericCursiveName;
	OpString mGenericFantasyName;
	OpString mGenericMonospaceName;

	int mGenericSerifNameScore;
	int mGenericSansSerifNameScore;
	int mGenericCursiveNameScore;
	int mGenericFantasyNameScore;
	int mGenericMonospaceNameScore;
#endif
	int             m_antialisingThreshhold;
	MacFontIntern   *m_intern;
#if !defined(MDEFONT_MODULE)
	MDF_ProcessedGlyphBuffer m_glyph_buffer;
#endif
	
	//ATSUFontID*    m_fontIDs;
	MacOpFont*      m_fonts;   // Array of all the fonts
	OpVector<MacFontFamily> m_families;
#ifdef WEBFONTS_SUPPORT
	OpVector<WebFontContainer> mInstalledWebFonts;
	virtual OpFontInfo::FontType GetWebFontType(OpWebFontRef webfont) {return OpFontInfo::PLATFORM_WEBFONT;}
#endif // WEBFONTS_SUPPORT
	SystemLanguage m_sysLanguage;
};

#endif //MacOpFontManager_H
