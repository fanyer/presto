/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * OpFont.h -- platform implementation of fonts and font manager.
 */
#ifndef OPFONT_H
#define OPFONT_H

#include "modules/display/fontdb.h"

#ifdef PERSCRIPT_GENERIC_FONT
# include "modules/windowcommander/WritingSystem.h"
#endif // PERSCRIPT_GENERIC_FONT

class OpPainter;
#ifdef SVG_SUPPORT
class SVGPath;
# include "modules/svg/svg_number.h"
#endif // SVG_SUPPORT
#ifdef PLATFORM_FONTSWITCHING
class OpAsyncFontListener;
#endif

/** Representation of a "loaded" font.
 *
 * OpFont objects represent instantiated fonts from the system. A font has a
 * few attributes; a specific name (family), size, weight (bold?), slant
 * (italic?), and then some. An instance of this object is also able to measure
 * the width in pixels of a given string, so that a layout engine may allocate
 * appropriate amounts of space for text.
 */
class OpFont
{
public:
	virtual OpFontInfo::FontType Type() { return OpFontInfo::PLATFORM; }

#ifdef OPFONT_GLYPH_PROPS
	/** Positional and dimensional properties of a glyph */
	struct GlyphProps
	{
		/** The horizontal distance in pixels from the current pen position to the glyph's left bounding box edge. */
		INT32 left;

		/** The vertical distance in pixels from the baseline to the top of the glyph's bounding box. Upwards y-coordinates are positive. */
		INT32 top;

		/** Bounding-width in pixels of the glyph in pixels */
		INT32 width;

		/** Bounding-height in pixels of the glyph in pixels */
		INT32 height;

		/** The horizontal distance in pixels the pen position must be incremented (for left-to-right writing)
		    or decremented (for right-to-left writing) by after each glyph is rendered when processing text. */
		INT32 advance;
	};
#endif // OPFONT_GLYPH_PROPS

	virtual ~OpFont() {}

	/** Get the font's general ascent.
	 *
	 * This is the number of pixels above the baseline counting from the lower
	 * edge of the pixel line indicated by the baseline.
	 *
	 * Note that a font is likely to contain characters that have a larger or
	 * smaller ascent than the one returned from this method.
	 *
	 * Together with Descent() and InternalLeading(), Ascent() makes up the
	 * font's line-height.
	 */
	virtual UINT32 Ascent() = 0;

	/** Get the font's general descent.
	 *
	 * This is the number of pixels below the baseline, counting from the lower
	 * edge of the pixel line indicated by the baseline.
	 *
	 * Note that a font is likely to contain characters that have a larger or
	 * smaller descent than the one returned from this method.
	 *
	 * Many font systems use negative values for descent but in OpFont this is
	 * a positive number. That must be considered when reading formulas from
	 * other sources including descent.
	 *
	 * Together with Ascent() and InternalLeading(), Descent() makes up the
	 * font's line-height.
	 */
	virtual UINT32 Descent() = 0;

	/** Get the font's internal leading.
	 *
	 * Internal leading is a font's inter-line spacing - the preferred number
	 * of pixels of padding between each line of text using this font. This
	 * padding is often used by font designers to place accents, tildes and
	 * diaeresises.
	 *
	 * Together with Ascent() and Descent(), InternalLeading() makes up the
	 * font's line-height.
	 */
	virtual UINT32 InternalLeading() = 0;

	/** Get the height of the font's em-box.
	 *
	 * This is the size in pixels of the font as used in relaxed descriptions
	 * of the font. This number is a response to the size requested when the
	 * font was created, and is the closest match that the font engine could
	 * provide.
	 *
	 * In more details, this is the height of the font's em-box in pixels. When
	 * designing a font a font designer places glyphs in a an em-box which
	 * represents the font's typical size. Typically a glyph fills the em-box
	 * so that the size (height) of the em-box corresonds well to the size of
	 * the text. That is why that size is used to request and probe font sizes.
	 *
	 * This is typically quite close to Ascent()+Descent(), but may differ,
	 * depending on the font, and will (disregarding scaling, lack of certain
	 * sizes and other font selection details) be the size the user requested
	 * when creating the font.
	 *
	 * This is <b>not</b> the same as line-height. Line-height can be
	 * calculated by adding Ascent(), Descent() and InternalLeading().
	 *
	 * @see OpFontManager::CreateFont()
	 */
	virtual UINT32 Height() = 0;

	/** Get the font's average character width.
	 *
	 * An implementation is free to choose what this means. Returning the width
	 * of the character '0' may be a good thing.
	 */
	virtual UINT32 AveCharWidth() = 0;

	/** Get the font's overhang.
	 *
	 * This is the extra width caused by the slant of italic fonts.
	 * StringWidth() doesn't include this, since that method only returns the
	 * space "taken up" by a string. The bounding box of an italic-font string
	 * will however need to be increased by the font's overhang, so that the
	 * overflow caused by the slant can be measured, to avoid clipping parts of
	 * the last character.
	 *
	 * If this is not a italic font, the result is always 0.
	 */
	virtual UINT32 Overhang() = 0;

	/** Get the width of the string, in pixels, using this font.
	 *
	 * Calculate the number of pixels taken up by the specified string when
	 * drawn with this font. This does not include overhang or other kinds of
	 * overflow.
	 *
	 * @param str the string to calculate the pixel width of
	 * @param len number of characters in the string
	 * @param extra_char_spacing How much extra space, in pixels, to put after
	 * each character. This is related to the CSS property letter-spacing. An
	 * implementation that has no means of passing this information to the
	 * system when measuring font width may simply measure the font without
	 * extra_char_spacing, and then add len multiplied with extra_char_spacing
	 * afterwards.
	 */
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing = 0
#ifdef PLATFORM_FONTSWITCHING
							   , OpAsyncFontListener* listener = 0 ///< listener to use to notify core about delayed font loading
#endif
						) = 0;

	/** Get the width of the string, in pixels, using this font and painter.
	 *
	 * Calculate the number of pixels taken up by the specified string when
	 * drawn with this font. This does not include overhang or other kinds of
	 * overflow.
	 *
	 * @param str the string to calculate the pixel width of
	 * @param len number of characters in the string
	 * @param painter the painter whose context to evaluate the width
	 * in. Font width may vary, depending on device (e.g. screen and
	 * printer)
	 * @param extra_char_spacing How much extra space, in pixels, to put after
	 * each character. This is related to the CSS property letter-spacing. An
	 * implementation that has no means of passing this information to the
	 * system when measuring font width may simply measure the font without
	 * extra_char_spacing, and then add len multiplied with extra_char_spacing
	 * afterwards.
	 */
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing = 0
#ifdef PLATFORM_FONTSWITCHING
							   , OpAsyncFontListener* listener = 0 ///< listener to use to notify core about delayed font loading
#endif
								) = 0;

#ifdef OPFONT_GLYPH_PROPS
	/** Get the positional and dimensional properties for a glyph.
	 *
	 * @param up the character for which to get the props
	 * @param props (out) A GlyphProps struct filled with the properties of the
	 * glyph
	 * @return OK if successful.
	 */
	virtual OP_STATUS GetGlyphProps(const UnicodePoint up, GlyphProps* props) = 0;
#endif // OPFONT_GLYPH_PROPS

#ifdef SVG_SUPPORT
	/** Get the outline of a specific glyph.
	 *
	 * This is necessary for SVG to be able to fill and stroke text.
	 *
	 * NOTE! The outline passed may be NULL, in which case it's just a test to
	 * see if the font supports getting outlines at all. Return OpStatus::OK if
	 * supported, or OpStatus::ERR_NOT_SUPPORTED if unsupported.
	 *
	 * An implementation will want to use SVGManager::CreatePath() to create
	 * the SVGPath. The caller of this method is responsible for the outline
	 * only after this method returns with a success status. In case of error,
	 * don't forget to delete the outline (if allocated) before returning.
	 *
	 * The implementation has an entire word at it's disposal, for getting
	 * shaping of Arabic letters correct in particular. Also, it's useful for
	 * kerning. The returned advance value should take kerning into
	 * consideration.
	 *
	 * An implementation should:
	 * 1) Check validity of parameters, immediately return if any of the
	 * following conditions are true:
	 *	 - if out_glyph is NULL return OpStatus::OK (must be the first test)
	 *	 - if io_str_pos >= in_len return OpStatus::ERR_OUT_OF_RANGE.
	 *	 - if in_len is zero return OpStatus::ERR_OUT_OF_RANGE
	 * 2) Map in_str[io_str_pos] to a glyph, it may consume one or more
	 * characters from in_str. Take into account the writing direction, as
	 * there may be more than one match depending on direction.
	 * 3) Increase io_str_pos with the number of consumed characters
	 * 4) Put the advance of the glyph into out_advance
	 * 5) Generate the SVGPath representation of the glyph and put it in
	 * out_glyph if successful, then return OpStatus::OK. If unsuccessful,
	 * delete any allocated memory and return OpStatus::ERR or
	 * OpStatus::ERR_NO_MEMORY.
	 *
	 * A simple but not entirely pleasing implementation ignores
	 * in_writing_direction_horizontal and maps one character in in_str to one
	 * glyph always. Furthermore kerning is ignored and the width of the glyph,
	 * (if in_writing_direction_horizontal is TRUE, otherwise the height of the
	 * glyph) is returned in advance.
	 *
	 * @param in_str The string (it's a bidi-split word) to work with
	 * @param in_len The number of characters in the string
	 * @param io_str_pos This is the current position in the string, it should
	 * be incremented with the number of characters that mapped to the returned
	 * glyph
	 * @param in_last_str_pos This is the last position in the string, to be
	 * used for kerning
	 * @param in_writing_direction_horizontal Whether the current direction is
	 * horizontal or vertical
	 * @param out_advance The advance value of the returned glyph (note that
	 * advance is in the writing direction)
	 * @param out_glyph If outline is NULL then it's only a test, don't
	 * allocate anything, just return the proper errorcode, if non-NULL then
	 * allocate and create an SVGPath and return it here
	 *
	 * @return OpStatus::ERR_NOT_SUPPORTED if font is incapable of extracting
	 * the outline, OpStatus::ERR_OUT_OF_RANGE if in parameters are out of
	 * range - see step 1 above, * OpStatus::ERR_NO_MEMORY if OOM,
	 * OpStatus::ERR if other error, OpStatus::OK if successful.
	 */
	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph) = 0;
#endif // SVG_SUPPORT

#ifdef OPFONT_FONT_DATA
	/** Get font data for this font.
	 *
	 * This is necessary for PostScript printing, to get font data for embedding
	 * in PostScript document.
	 *
	 * The data should be the full uninterpreted font data as read from font file
	 * or received by other method.
	 *
	 * @param font_data handle that should be pointed to font data.
	 * @param data_size resulting font data size.
	 * @return OpStatus::ERR if font data could not be retrieved for this font,
	 * for example if font implementation on platform does not allow this,
	 * OpStatus::OK is successful.
	 */
	virtual OP_STATUS GetFontData(UINT8*& font_data, UINT32& data_size) = 0;

	/** Release font data that was retrieved by GetFontData().
	 *
	 * This function needs to be called when font data retrieved by
	 * OpFont::GetFontData() is no longer needed.
	 *
	 * @param font_data font data that should be released.
	 * @return OpStatus::ERR if font data could not be released for this font,
	 * for example if this is not supported by font implementation,
	 * OpStatus::OK is successful.
	 */
	virtual OP_STATUS ReleaseFontData(UINT8* font_data) = 0;
#endif // OPFONT_FONT_DATA
};

/** Generic font families, as defined by CSS 2. */
enum GenericFont
{
	GENERIC_FONT_SERIF = 0, ///< CSS property value "serif"
	GENERIC_FONT_SANSSERIF, ///< CSS property value "sans-serif"
	GENERIC_FONT_CURSIVE, ///< CSS property value "cursive"
	GENERIC_FONT_FANTASY, ///< CSS property value "fantasy"
	GENERIC_FONT_MONOSPACE ///< CSS property value "monospace"
};

/** Font manager. Knows about all fonts in the system, and loads them upon request.
 *
 * OpFontManager handles instantiation of objects for the fonts that are in the
 * system (OpFonts). Some interaction with OpFontDatabase is needed in order to
 * know which font to instantiate. See modules/doc/style/fontdb.h for more info
 * about the font database and the OpFontInfo structure.
 *
 * OpFontManager is capable of enumerating the fonts on the system, categorize
 * them (detect generic font family), and load and create a new instance of a
 * font.
 *
 * During startup, Opera core will call BeginEnumeration(), then get the number
 * of enumerable fonts by calling CountFonts(). Then it will allocate an
 * OpFontInfo for each font, then call GetFontInfo() for each font number,
 * starting on 0, continuing to CountFonts()-1. After this, EndEnumeration()
 * will be called. At this point, core knows what it needs to know about
 * available fonts. Core will later call CreateFont() each time it needs to
 * load (= prepare for size measurements and rendering) a new font.
 */
class OpFontManager
{
public:
	/** Create and return a new OpFontManager.
	 *
	 * @param new_fontmanager (out) Set to the font manager created
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors so
	 * critical that Opera cannot run (if the system has no fonts, for
	 * example).
	 */
	static OP_STATUS Create(OpFontManager** new_fontmanager);

	virtual ~OpFontManager() {}

	/** Create a font object.
	 *
	 * This will load a font, meaning that a font is prepared for size
	 * measurement and rendering. A font is uniquely identified by name, size,
	 * weight and slant.
	 *
	 * @param face the name of the font. This is a canonical font name. It has
	 * to be exactly the same as one of the fonts provided by GetFontInfo().
	 * @param size Requested font size, in pixels. Call Height() on the
	 * returned font to get the actual size of the new font. An implementation
	 * is not required to support scalable fonts, so the caller must bear in
	 * mind that it may be impossible to provide a font whose size matches the
	 * requested size exactly. An implementation not supporting scalable fonts
	 * will choose the nearest available size. A bigger font is preferred over
	 * a smaller one when no exact match is available.
	 * @param weight weight of the font (0-9). This value is a
	 * hundredth of the weight values used by CSS 2. E.g. 4 is normal,
	 * 7 is bold. If there are no fonts between normal and bold then 5
	 * should be normal and 6 should be bold. See
	 * http://www.w3.org/TR/CSS21/fonts.html#font-boldness
	 * @param italic If TRUE, request an italic font instead of an unslanted
	 * one.
	 * @param must_have_getoutline If TRUE the returned font MUST support
	 * GetOutline(). If this requirement is impossible to satisfy, NULL is
	 * returned.
	 * @param blur_radius the amount of blur in pixels to apply to glyphs when
	 * rendering them. Rendering a set of blurred glyphs is the same as
	 * rendering a text shadow.
	 * If the platform cannot implement this in a good way, use
	 * TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT to set the blur radius limit to 0
	 * and core will use workarounds to never give the font a non-zero blur
	 * radius. The platform implementation is then at liberty to ignore this
	 * parameter.
	 *
	 * @return the OpFont created, or NULL (out of memory, or no outline
	 * support while required). As already mentioned, the attributes of the
	 * returned font may not be identical to what was requested. For example, the
	 * font may not be available in italic, or perhaps no font by the specified
	 * name could be found. In any case, the implementation will find as good a
	 * substitute as possible.
	 */
	virtual OpFont* CreateFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius
#ifdef PI_CREATEFONT_WITH_CONTEXT
		, void* context = NULL
#endif
		) = 0;

	/** Opaque type for references to registered webfonts.
	 */
 	typedef UINTPTR OpWebFontRef;

	/** Create a font object based on a webfont.
	 *
	 * This method is similar to the CreateFont() method that takes a
	 * string as the first argument. The webfont handle specified
	 * refers to a font previously added with AddWebFont().
	 *
	 * @param webfont The webfont
	 * @param size Requested font size, in pixels.
	 * @param blur_radius the amount of blur in pixels to apply to glyphs when
	 * rendering them. Rendering a set of blurred glyphs is the same as
	 * rendering a text shadow.
	 * If the platform cannot implement this in a good way, use
	 * TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT to set the blur radius limit to 0
	 * and core will use workarounds to never give the font a non-zero blur
	 * radius. The platform implementation is then at liberty to ignore this
	 * parameter.
	 *
	 * @return the OpFont created, or NULL (out of memory).
	 */
	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius) = 0;

	/** As above, but used when synthesizing is needed. If the
	 * platform cannot perform synthesizing of weight and style it may
	 * return CreateFont(webfont, size).
	 *
	 * @param webfont The webfont
	 * @param weight the requested weight
	 * @param italic TRUE if an italic font is requested
	 * @param size Requested font size, in pixels.
	 * @param blur_radius the amount of blur in pixels to apply to glyphs when
	 * rendering them. Rendering a set of blurred glyphs is the same as
	 * rendering a text shadow.
	 * If the platform cannot implement this in a good way, use
	 * TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT to set the blur radius limit to 0
	 * and core will use workarounds to never give the font a non-zero blur
	 * radius. The platform implementation is then at liberty to ignore this
	 * parameter.
	 *
	 * @return the OpFont created, or NULL (out of memory).
	 */
	virtual OpFont* CreateFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius) = 0;

	/** Add a new webfont.
	 *
	 * This method adds a new webfont to the font manager. The font is
	 * loaded from the given path.
	 *
	 * The font is only to be applied to this instance of Opera, and
	 * should not be visible to other applications. Moreover, the font
	 * should not appear in the enumeration or participate in any
	 * automatic font selection logic.
	 *
	 * The font should be removed by calling RemoveWebFont().
	 *
	 * @param webfont (out) The unique reference to the newly added font is
	 * returned here.
	 * @param path This path points to the font file
	 *
	 * @return OK if font successfully added, ERR_NOT_SUPPORTED if the font
	 * format is unsupported, ERR_NO_MEMORY on OOM, ERR for other errors.
	 */
	virtual OP_STATUS AddWebFont(OpWebFontRef& webfont, const uni_char* path) = 0;

	/** Remove a webfont.
	 *
	 * Removes the webfont previously added with AddWebFont().
	 *
	 * @param webfont The reference to the webfont.
	 *
	 * @return OK if font successfully removed, ERR for errors.
	 */
	virtual OP_STATUS RemoveWebFont(OpWebFontRef webfont) = 0;

	/** Populate a font info structure.
	 *
	 * This method can be called at any time, unlike the GetFontInfo method.
	 *
	 * @param webfont The reference to the webfont.
	 * @param fontinfo (out) Filled with data for a font entry.
	 *
	 * @return OK if the fontinfo was successfully updated.
	 */
	virtual OP_STATUS GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo) = 0;

	/**
	 * Look for facename in name tables of all font faces - it's
	 * recommended to use MatchFace (in modules/display/sfnt.h) to
	 * match name to face
	 *
	 * @param localfont resulting font - set to 0 if no match is found
	 * @param facename the face name to look for
	 * @return OpStatus::OK on success (does not mean face is found), OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS GetLocalFont(OpWebFontRef& localfont, const uni_char* facename) = 0;

	/**
	 * Queries whether the platform supports a webfont format.
	 * @param format the format to query support for. A CSS_WebFont::Format value disguised as an int.
	 * @return TRUE if the format is supported, FALSE otherwise.
	 */
	virtual BOOL SupportsFormat(int format) = 0;

	/** Get the total number of fonts to be enumerated.
	 *
	 * It is not legal to call this method before BeginEnumeration() has been
	 * called, and also not legal to call it after EndEnumeration() has been
	 * called.
	 *
	 * @return the number of fonts found on the system. A font here means font
	 * family name, possibly including foundry as well on some systems. In
	 * other words, while a loaded font in an OpFont object is uniquely
	 * identified by name, size, weight and slant, what counts as a font here,
	 * is just a unique name.
	 */
	virtual UINT32 CountFonts() = 0;

	/** Populate a font info structure with a font.
	 *
	 * It is not legal to call this method before BeginEnumeration() has been
	 * called, and also not legal to call it after EndEnumeration() has been
	 * called.
	 *
	 * The font name to pass to fontinfo is the canonical font name. On many
	 * systems this simply means family (e.g. "Helvetica"), while on others it
	 * may include foundry name as well (e.g. "Helvetica [Adobe]"). Calls to
	 * CreateFont() later will use these canonical font names in the argument.
	 *
	 * @param fontnr The font number, a number between 0 and
	 * CountFonts()-1. The number refers to a font entry in the internal font
	 * list of OpFontManager. See BeginEnumeration().
	 * @param fontinfo (out) Filled with data for a font entry.
	 */
	virtual OP_STATUS GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo) = 0;

	/** Begin font enumeration.
	 *
	 * Allows calls to CountFonts() or GetFontInfo(). Cancelled by
	 * EndEnumeration().
	 *
	 * If the platform implementation requires special preparation prior to
	 * calling CountFonts() or GetFontInfo() (and that it is not feasible to do
	 * that kind of preparation when the OpFontManager is created), it will be
	 * done here.
	 *
	 * After this call, OpFontManager has an internal list of fonts, each with
	 * a unique number (which GetFontInfo() refers to), starting at font number
	 * 0.
	 *
	 * Most implementations will probably do nothing and just return
	 * OpStatus::OK here, as long as initialization and scanning of fonts can
	 * be completed during OpFontManager initialization.
	 */
	virtual OP_STATUS BeginEnumeration() = 0;

	/** End font enumeration.
	 *
	 * Cancel the effect of a previous call to BeginEnumeration(). After
	 * calling EndEnumeration(), it is no longer legal to call CountFonts() or
	 * GetFontInfo().
	 *
	 * Most implementations will probably do nothing and just return
	 * OpStatus::OK here, as long as initialization and scanning of fonts can
	 * be completed during OpFontManager initialization, so that
	 * BeginEnumeration did nothing().
	 */
	virtual OP_STATUS EndEnumeration() = 0;

	/**
	 * Called before the StyleManager is starting to use font
	 * information to set up default fonts. Can be used by platforms
	 * to correct faulty information in fonts.
	 * @param styl_man the style manager that is being created. At the
	 * time of calling StyleManager::GetFontNumber() and
	 * StyleManager::GetFontInfo() can be used to fetch and override
	 * font information.
	 */
	virtual void BeforeStyleInit(class StyleManager* styl_man) = 0;

	/** Get the name of a font that represents a generic CSS font family.
	 *
	 * An implementation should generate a list of such generic fonts when
	 * OpFontManager is created. It is obviously entirely system specific which
	 * fonts are suitable as a generic CSS font family candidate. It is however
	 * required that passing the returned name to CreateFont() result in a
	 * valid OpFont object.
	 *
	 * If the system has no means of detecting characteristics to use to
	 * determine which generic CSS font family a font may serve as, here are
	 * some hints: Fonts called something with "times" or "serif" may be
	 * suitable GENERIC_FONT_SERIF candidates. Fonts called "helvetica",
	 * "arial" or "sans-serif" may be suitable GENERIC_FONT_SANSSERIF
	 * candidates. Fonts called "courier", "fixed" or "monospace" may be
	 * suitable GENERIC_FONT_MONOSPACE candidates.
	 *
	 * @param generic_font The generic CSS font family name for which to get an
	 * actual font name (that exists on the system)
	 *
	 * @return The name of the font that represents the specified generic CSS
	 * font family. NULL is not allowed. The string is valid as long as this
	 * OpFontManager object is alive.
	 */

#ifdef PERSCRIPT_GENERIC_FONT
	virtual const uni_char* GetGenericFontName(GenericFont generic_font, WritingSystem::Script script = WritingSystem::Unknown) = 0;
#else // PERSCRIPT_GENERIC_FONT
	virtual const uni_char* GetGenericFontName(GenericFont generic_font) = 0;
#endif // PERSCRIPT_GENERIC_FONT


#ifdef HAS_FONT_FOUNDRY
	/** Find a canonical font name (one recognizable by CreateFont()).
	 *
	 * Search for a font on any given format ("helvetica", "adobe-helvetica",
	 * "adobe-helvetica-iso8859-1" etc.). This method should support any format
	 * that is likely to be used (which is implementation specific). It should
	 * return (through full_name) the full, real font name, as it is (or would
	 * have been) stored in OpFontDatabase. The method may choose not to verify
	 * that the font name exists, as long as it's on the valid format, e.g. if
	 * 'name' is "foo-bar-oste-pop", and the implementation of
	 * FindBestFontName() says that this is the right format (even though the
	 * font doesn't necessarily exist), it may report success. This method is
	 * here to help core find the right font name based on a font specified on
	 * a webpage. On e.g. X11, part of the font name (as provided by
	 * GetFontInfo()) is the foundry name. Foundry names are usually not
	 * specified on a webpage. A webpage may specify "Arial" as the font name,
	 * while the actual font name on the system may be "Arial [monotype]".
	 *
	 * The amount of black magic to apply here is up to the implementation.
	 *
	 * @param name the font name to search for (like "helvetica",
	 * "adobe-helvetica", "adobe-helvetica-iso8859-1", "Helvetica [Adobe]",
	 * etc.)
	 * @full_name the fully qualified (canonical) name. This name should be on
	 * the format as stored in OpFontDatabase, including foundry if
	 * OpFontDatabase uses that.
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on allocation
	 * failure, OpStatus::ERR on invalid font format.
	 */
	virtual OP_STATUS FindBestFontName(const uni_char *name, OpString &full_name) = 0;

	/** Get the foundry name part of a canonical font name.
	 *
	 * Extract the foundry name from a font string (like "Helvetica [Adobe]",
	 * or whatever the font format looks like).
	 *
	 * @param font the font string
	 * @param foundry where to put the result. This will be a string containing
	 * only the foundry name (like "Adobe").
	 */
	virtual OP_STATUS GetFoundry(const uni_char *font, OpString &foundry) = 0;

	/** Get the family name part of a canonical font name.
	 *
	 * Extract the family name from a font string (like "Helvetica [Adobe]"),
	 * i.e. strip away any foundry name etc.
	 *
	 * @param font the font string
	 * @param family where to put the result. This will be a string containing
	 * only the family name (like "Helvetica").
	 */
	virtual OP_STATUS GetFamily(const uni_char *font, OpString &family) = 0;

	/** Set the preferred foundry to use when there are multiple choices.
	 *
	 * Set the preferred foundry to use when converting to canonical font
	 * names. When a font name is specified without the foundry (i.e. only the
	 * family name) as, for example, the argument to FindBestFontName(), and
	 * this family is available from more than one foundry, choose the
	 * preferred foundry if possible.
	 *
	 * @param foundry The new preferred foundry.
	 */
	virtual OP_STATUS SetPreferredFoundry(const uni_char *foundry) = 0;
#endif // HAS_FONT_FOUNDRY

#ifdef PLATFORM_FONTSWITCHING
	/** Set the preferred font for a unicode block.
	 *
	 * @param ch a character contained by the actual unicode block. This can be
	 * the first character in the block (like number 0x0370 for Greek), for
	 * example.
	 * @param monospace is the specified font supposed to be a replacement for
	 * monospace fonts or proportional fonts?
	 */
	virtual void SetPreferredFont(uni_char ch, bool monospace, const uni_char *font, OpFontInfo::CodePage codepage=OpFontInfo::OP_DEFAULT_CODEPAGE) = 0;
#endif // PLATFORM_FONTSWITCHING

#ifdef _GLYPHTESTING_SUPPORT_
	/** Update the glyph mask for a font.
	 *
	 * OpFontInfo has a bitmask to tell which glyphs a font supports. This
	 * method will update this mask.
	 *
	 * This makes it possible for the font manager to delay glyph mask scanning
	 * until the first call to OpFontInfo::HasGlyph(). In other words, it is
	 * not necessary for the platform implementation to set this mask during
	 * OpFontManager initialization or enumeration, if that is expensive (it
	 * probably is).
	 */
	virtual void UpdateGlyphMask(OpFontInfo *fontinfo) = 0;
#endif // _GLYPHTESTING_SUPPORT_

#ifdef FONTMAN_HASWEIGHT
	/** Checks if a font supports a certain weight.
	 *
	 * @param fontnr The font number, a number between 0 and
	 * CountFonts()-1. The number refers to a font entry in the internal font
	 * list of OpFontManager. See BeginEnumeration().
	 * @param weight weight of the font (0-9). This value is a
	 * hundredth of the weight values used by CSS 2. E.g. 4 is normal,
	 * 7 is bold.
	 */
	virtual BOOL HasWeight(UINT32 fontnr, UINT8 weight) = 0;
#endif // FONTMAN_HASWEIGHT
};

#endif // OPFONT_H
