#ifndef MacOpFont_H
#define MacOpFont_H

#include "modules/display/pixelscaler.h"
#include "modules/pi/OpFont.h"
#include "modules/libvega/src/oppainter/vegaopfont.h"

#include "platforms/mac/model/MacFontIntern.h"

#ifdef __OBJC__
#define BOOL NSBOOL
#import <Foundation/NSDictionary.h>
#undef BOOL
#else
class NSDictionary;
#endif

#define MAX_FILTER_GLYPHS	3
#define FILTER_CHARACTERS	L"\x200B\x200C\x200D"	// ZWSP, ZWNJ, ZWJ
#define MAGIC_NUMBER		123456L*VEGA_GLYPH_CACHE_HASH_SIZE
#define OFF_SCREEN          50000

class MacOpFontManager;

class MacFontFamily
{
public:
	
	MacFontFamily(const uni_char* name) : m_name(name) {};
	~MacFontFamily() { m_fonts.DeleteAll(); }
	
	void AddFont(const uni_char* font_name) {   uni_char* font = OP_NEWA(uni_char, uni_strlen(font_name));
												uni_strcpy(font, font_name);
												m_fonts.Add(font);
											}
	
	const uni_char*	GetName() const						{ return m_name; }
	const UINT32	GetFontCount() const				{ return m_fonts.GetCount(); }
	uni_char*		GetFontName(UINT32 i) const			{ return m_fonts.Get(i); }
	
	INT32			FindFont(uni_char* fontName) const	{ return m_fonts.Find(fontName); }
	
private:
	const uni_char*		m_name;
	OpVector<uni_char>	m_fonts;
		
};

class FontBlock
{
	long c_ref;
	void* image_data;
	unsigned int bytes_per_pixel;
	OpString8 text_;
	OpRect rect;
	bool is_modified;
	UINT32 color_;
	~FontBlock();
	FontBlock();
public:
	long AddRef() { return ++c_ref; }
	long Release() {
		long c = --c_ref;
		if (0 == c)
			OP_DELETE(this);
		return c;
	}
	bool IsModified() const { return is_modified; }
	void SetModified(BOOL modif);
	UINT32 GetColor() const { return color_; }
	void SetColor(UINT32 c) { color_ = c; }
	static bool Construct(FontBlock ** result, const OpRect& r, unsigned int bytes_per_pixel);
	inline void* GetImageData() { return image_data; }
	inline const void* GetImageData() const { return const_cast<FontBlock*>(this)->GetImageData(); }
	inline OpRect GetRect() const { return rect; }
	inline unsigned int GetBPP() const { return bytes_per_pixel; }
	const char* GetText() const { return text_.CStr(); }
	void SetText(const uni_char* s, UINT32 len);
};

#if defined(VEGA_OPPAINTER_SUPPORT)
class MacOpFont : public VEGAFont
#else
class MacOpFont : public OpFont
#endif
{
	class MacAutoString8HashTable : public OpGenericString8HashTable
	{
	public:
		MacAutoString8HashTable(BOOL case_sensitive = FALSE) : OpGenericString8HashTable(case_sensitive) {}
		virtual void Delete(void* p) { static_cast<FontBlock*>(p)->Release(); }
		virtual ~MacAutoString8HashTable() { this->UnrefAndDeleteAll(); }
		
		static void DeleteFunc(const void* key, void *data, OpHashTable* table)
		{
			static_cast<FontBlock*>(data)->Release();
			free((char*)key);
		}
		
		void UnrefAndDeleteAll()
		{
			ForEach(DeleteFunc);
			RemoveAll();
		}

		
		void UnrefAll()
		{
			OpHashIterator* it = GetIterator();
			if (OpStatus::IsError(it->First()))
				return;
			do {
				FontBlock* fb = static_cast<FontBlock*>(it->GetData());
				fb->Release();
			} while (OpStatus::IsSuccess(it->Next()));
		}
	};

public:
		           MacOpFont(MacOpFontManager* manager, ATSFontRef inFont, CTFontRef fontRef, SInt16 inSize, SInt16 inFace, OpFontInfo::FontType fontType, int blur_radius);
	virtual        ~MacOpFont();

	void		   Initialize();

	/** @return the font's ascent */
	virtual UINT32 Ascent();

	/** @return the font's descent */
	virtual UINT32 Descent();

	/** @return the font's INTernal leading */
	virtual UINT32 InternalLeading();

	/** @return the font's height in pixels */
	virtual UINT32 Height();

	/** @return the average width of characters in the font */
	virtual UINT32 AveCharWidth();

	/** @return the overhang of the font
		(The width added for Italic fonts. If this is not a italic font, it returns 0)
	*/
	virtual UINT32 Overhang();

	/** Calculates the width of the specified string when drawn with the font.
		@return the width of the specified string
		@param str the string to calculate
		@param len how many characters of the string
	*/
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing = 0);

	/**
	   Calculates the width of the specified string when drawn with
	   the font, on the specified painter.

		@return the width of the specified string
		@param painter the painter, whose context to evaluate the width in.
		@param str the string to calculate
		@param len how many characters of the string */
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing = 0);
	
	virtual UINT32 MaxAdvance();
#ifdef VEGA_3DDEVICE
	virtual UINT32 MaxGlyphHeight();
#endif

	BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, const OpRect& clip, UINT32 color, CGContextRef dest, size_t win_height);

	FontBlock* GetFontBlock(const OpRect& clip, unsigned int bytes_per_pixel);

#ifdef VEGA_NATIVE_FONT_SUPPORT
	virtual BOOL nativeRendering();

	/** Draw a string to the specified destination window.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGAWindow* dest);

	/** Draw a string to the specified destination buffer.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGASWBuffer* dest);
#ifdef VEGA_USE_HW
	/** Draw a string to the specified destination 3d render target.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL DrawString(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, UINT32 color, VEGA3dRenderTarget* dest, bool is_window);
#endif // VEGA_USE_HW	
	/** Draw a string to an 8 bit alpha mask owned by the font.
	 * @returns TRUE if rendering succeded, FALSE if it failed. */
	virtual BOOL GetAlphaMask(const uni_char* str, UINT32 len, INT32 extra_char_spacing, short adjust, const OpRect& clip, const UINT8** mask, OpPoint* size, unsigned int* rowStride, unsigned int* pixelStride);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	virtual int SetGlyphScale(int glyphscale)
	{
		int oldscale = mScaler.GetScale();
		if (oldscale != glyphscale)
		{
			mScaler.SetScale(glyphscale);
		}

		return oldscale;
	};
#endif // PIXEL_SCALE_RENDERING_SUPPORT

#endif // VEGA_NATIVE_FONT_SUPPORT
	
#ifdef MDF_CAP_PROCESSEDSTRING
	/**
	   process string: apply advance, kerning etc. the struct
	   ProcessedString is located in modules/mdefont/processedstring.h
	   but may be used by implementations not using the mdefont
	   backend. look there for further documentation.
	 */
	virtual OP_STATUS ProcessString(struct ProcessedString* processed_string,
									const uni_char* str, const size_t len,
									INT32 extra_char_spacing = 0, short word_width = -1,
									bool use_glyph_indices = false);
#endif // MDF_CAP_PROCESSEDSTRING
#ifdef VEGA_3DDEVICE
	// Return 0 to get the fallback implementation
	virtual int getBlurRadius() { return m_blur_radius; }
#endif

#if defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)
	static void       InitOutlineCallbacks();
	static void       DeleteOutlineCallbacks();
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
								 BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph);
	OP_STATUS		GetGlyphOutline(GlyphID glyph, SVGPath* path);
#endif // SVG_SUPPORT && SVG_CAP_HAS_SVGPATH_API

	BOOL			GetShouldSynthesizeItalic() { return ((mStyle & italic) ? true : false); }
	BOOL			GetShouldSynthesizeBold()   { return ((mStyle & bold) ? true : false); }
	short           GetGlyphAdvance(UniChar c);
	GlyphID         GetGlyphID(UniChar c);
	BOOL            GetGlyphs(const uni_char* str, unsigned int len, GlyphID *oGlyphInfo, unsigned int& ionumGlyphs, BOOL force = FALSE);
	BOOL            GetGlyphsUncached(const uni_char* str, unsigned int len, GlyphID *oGlyphInfo, unsigned int& ionumGlyphs, BOOL force = FALSE);
	CFStringRef     CopySubstitutedFontNameForGlyphRun(CTRunRef run);

// Accessors
	int				GetSize()       { return mSize; }
	CGFontRef		GetCGFontRef()  { return mCGFontRef; }
	const char*		GetFontName()   { return mFontName; }
#ifdef UNIT_TEXT_OUT
	int				GetSpaceLen()   { return mSpaceLen; }
#endif // UNIT_TEXT_OUT
	OpFontInfo::FontType Type() { return mFontType; }

#ifdef VEGA_NATIVE_FONT_SUPPORT
	// Fallback version to draw string if anything else fails.
	// It is slow and should be avoided
	void DrawStringUsingFallback(const OpPoint &pos, const uni_char* str, UINT32 len, INT32 extra_char_spacing, CGContextRef ctx, int context_height = 0);
#endif
	
	void SetupGlyphFilters();

	virtual BOOL isBold() { return mStyle & bold; }
	virtual BOOL isItalic() { return mStyle & italic; }

#ifdef OPFONT_GLYPH_PROPS
	virtual OP_STATUS getGlyph(UnicodePoint c, VEGAGlyph& glyph, BOOL isIndex = FALSE);
#endif // OPFONT_GLYPH_PROPS

	virtual bool UseGlyphInterpolation();

protected:
	/**
	   loads a glyph. if data and stride are both 0, implementation
	   should allocate buffer storage for the glyph.
	   VEGAGlyph::m_handle can be used as a handle. cleanup should be
	   done in unloadGlyph.
	   NOTE: implementations that need to do per-glyph cleanup are
	   responsible for calling clearCaches from destructor. since
	   clearCache calls unloadGlyph, which is virtual, this cannot be
	   done from base destructor.
	 */
	virtual OP_STATUS loadGlyph(VEGAGlyph& glyph, UINT8* data, unsigned int stride, BOOL isIndex = FALSE, BOOL blackOnWhite = TRUE);
	/**
	   called when a glyph is no longer used. implemenation should
	   free any memory allocated when loading glyph here.
	 */
	virtual void unloadGlyph(VEGAGlyph& glyph);
	/**
	   used by loadGlyph, buffer and stride should be set up as
	   documented in loadGlyph.
	 */
	virtual void getGlyphBuffer(VEGAGlyph& glyph, const UINT8*& buffer, unsigned int& stride);
	
	
private:
	
#ifdef VEGA_NATIVE_FONT_SUPPORT

	OP_STATUS   SetClipRect(CGContextRef ctx, const OpRect &rect);
	CTFontRef	TryToFindNewFont(CTFontSymbolicTraits trait_in);
	BOOL		UpdateCTFontRefAndATSUId(CTFontRef newFont, CTFontSymbolicTraits trait);
	void		CreateCTFont();

	// This is for Nafees Nastaleeq and similar, the insanity constant "2" is an emprirical value
	BOOL		HasInsanelyLargeBoundingBox() {return (mBoundingBox.size.height > 2*(mAscent+mDescent));}
	
	MacOpFontManager*       mFontManager;    // Pointer to the manager (there is only one of them, the font does not own it)
	ATSUFontID              mATSUid;         // specifies a font family and instance
	OpFontInfo::FontType	mFontType;
	float			        mWinHeight;
	float			        mPenColorRed;
	float			        mPenColorGreen;
	float			        mPenColorBlue;
	float			        mPenColorAlpha;
	unsigned short	        mCGFontChanged;
	int						m_blur_radius;

#endif // VEGA_NATIVE_FONT_SUPPORT
	
	SInt16		            mSize;
	SInt16		            mStyle;
	SInt16		            mAscent;
	SInt16		            mDescent;
	SInt16		            mAvgCharWidth;
	SInt16		            mOverhang;
	CGRect					mBoundingBox;
	SInt16					mExHeight;
	static CGrafPtr			sOffscreen;
	static CGContextRef		sCGContext;        // Used for font measuring. Not for rendering.
	static MacOpFont*		sContextFont;
	CGFontRef				mCGFontRef;
	CTFontRef				mCTFontRef;
	NSDictionary*			mDictionary;
	UINT32*					mTextAlphaMask;
#ifdef UNIT_TEXT_OUT
	int						mSpaceLen;
#endif
	char*					mFontName;
	BOOL					mIsStronglyContextualFont;
	BOOL					mIsFallbackOnlyFont;
	GlyphID					mFilterGlyphs[MAX_FILTER_GLYPHS];
	OpHashTable				mFallbackFontNames;
	GlyphRange				*mGlyphPages[256];
	AdvanceRange			*mGlyphAdvances[256];
	static GlyphID			*mGlyphBuffer;
	static CGSize			*mAdvanceBuffer;
	static Boolean          s_advanceBufferInitialized;
	MacAutoString8HashTable image_data_map;
	float					mVegaScale;

#if defined(SVG_SUPPORT) && defined(SVG_CAP_HAS_SVGPATH_API)
	static BOOL                     mInitializedOutlinecallbacks;
	static ATSCubicMoveToUPP 		mMoveToUPP;
	static ATSCubicLineToUPP 		mLineToUPP;
	static ATSCubicCurveToUPP 		mCurveToUPP;
	static ATSCubicClosePathUPP 	mClosePathUPP;

	static ATSQuadraticNewPathUPP 	mQuadNewPathUPP;
	static ATSQuadraticLineUPP		mQuadLineUPP;
	static ATSQuadraticCurveUPP 	mQuadCurveUPP;
	static ATSQuadraticClosePathUPP mQuadClosePathUPP;
#endif // SVG_SUPPORT && SVG_CAP_HAS_SVGPATH_API
	
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	PixelScaler mScaler;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
};

#endif //MacOpFont_H
