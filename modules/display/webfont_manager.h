/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WEBFONT_MANAGER_H
#define WEBFONT_MANAGER_H

#include "modules/pi/OpFont.h"
#include "modules/url/url2.h"
#include "modules/util/simset.h"
#include "modules/util/adt/opvector.h"

class HTML_Element;
class FramesDocument;

/**
 * The WebFontManager class keeps track of all usable webfonts that core knows about in all documents.
 * It sits between the fontcache and the platform so that the correct OpFont instances get created.
 *
 * About OpFontInfo
 * ================
 * The OpFontInfo class is used for passing around information about font faces internally in core,
 * but is really meant for describing a font family, which is what the porting interfaces expect.
 *
 * The OpFontInfo instance that describes an entire family is owned by the OpFontDatabase owned by
 * the StyleManager. Any other instance is just used as a transport mechanism, and for interfacing
 * with the platform layer.
 *
 * Generally when an OpFontInfo is passed in to the webfont manager it is just for describing what
 * was specified in an @font-face rule, not what was specified in the font itself. The only thing
 * that the webfont manager needs to collect from the actual information in the font itself is the
 * supported script(s), unicode-range(s) and glyphs. Those are needed for the fontswitcher to work
 * correctly.
 */
class WebFontManager
{
  private:
  	/**
  	 * The FontValue class represents a webfont family (which may be based on a local system font family),
  	 * only the non-system parts are kept here though. Note that the FontValue font family can contain
  	 * faces that are from different actual font families (e.g times.ttf can be used as bold face and arial.ttf
  	 * as italic face).
  	 *
  	 * A FontValue can contain any number of font faces (a.k.a font variants).
  	 */
	class FontValue
	{
	public:
		uni_char* family_name;		// The family name, since the OpFontInfo's have different lifespan
		unsigned int font_number;	// For quicker lookups we keep the font_number around
		OpFontInfo* original_platform_fontinfo; // Stored copy of the original fontinfo (for rebuilding fontinfo when faces are removed)

		/**
		 * The FontVariant class represents a font face (or a "font variant") of a font family.
		 */
		struct FontVariant
		{
			FontVariant(int size, int weight, BOOL smallcaps, BOOL italic, FramesDocument* frm_doc, HTML_Element* fontelm, OpFontManager::OpWebFontRef webfontref, URL url) :
				font_size(size), font_weight(weight), is_smallcaps(smallcaps), is_italic(italic), frm_doc(frm_doc), element(fontelm), webfontref(webfontref), url(url) {}

			BOOL HasWeight(UINT8 weight) const { return (this->font_weight & (1 << weight)) != 0; }

			int font_size; 								// -1 if not set
			int font_weight;
			BOOL is_smallcaps;
			BOOL is_italic;
			FramesDocument* frm_doc;					// The document that included this font face
			HTML_Element* element;						// The <font> or <font-face> element describing an SVG font face, always NULL for non-SVG fonts
			OpFontManager::OpWebFontRef webfontref;		// Owned by this class
			URL url;									// The url used for loading this font face (empty in case of svg fonts defined in markup only)
		};

		FontValue(unsigned int font_number) : family_name(NULL), font_number(font_number), original_platform_fontinfo(NULL) {}
		~FontValue();

		OP_STATUS SetOriginalFontInfo(OpFontInfo* original_platform_fontinfo);

		/** Adds a new font face to the font family */
		OP_STATUS AddFontVariant(int size, int font_weight, BOOL smallcaps, BOOL italic, FramesDocument* frm_doc, HTML_Element* fontelm, OpFontManager::OpWebFontRef webfontref, URL url);

		/**
		* Get the best font face match for the given criteria.
		* @see http://www.w3.org/TR/REC-CSS2/fonts.html#font-selection
		*
		* @param size The font size in pixels
		* @param weight The font weight ([0..9] where 4 is normal)
		* @param smallcaps The font style should be smallcaps
		* @param italic The font style should be italic
		* @param frm_doc The document that wants to use the font (used for security checking)
		*/
		FontVariant* GetFontVariant(int size, int weight, BOOL smallcaps, BOOL italic, FramesDocument* frm_doc);

		/**
		 * Check if this font family (aka FontValue) has a face that is allowed for use in the given document.
		 *
		 * If the FontValue didn't contain a face that was allowed in the document, but the
		 * font-family name matches a system font then that will also return TRUE.
		 * This is because the webfontmanager contains all fonts in all documents, and we
		 * want to allow normal systemfonts to be used in all documents that don't override
		 * the systemfont by using a webfont with the same familyname.
		 *
		 * @return TRUE if this font family is allowed in the given document, FALSE if not
		 */
		BOOL AllowedInDocument(FramesDocument* frm_doc);

		/**
		 * Remove a font face (font variant) from the font family. There may be several
		 * matching faces for the given input, so more than one face can be removed.
		 *
		 * @param frm_doc Remove variants only in this FramesDocument
		 * @param fontelm The element (for svgfonts), always NULL for non-svgfonts
		 * @param url The url (for @font-face fonts), always an empty URL() object for svgfonts
		 * @return Returns TRUE if all variants have been removed (signifies that the family can be removed too)
		 */
		BOOL RemoveFontVariants(FramesDocument* frm_doc, HTML_Element* fontelm, URL url);

		/**
		 * Check if there are font faces in this font family that are in the process of being deleted.
		 */
		BOOL IsBeingDeleted() {	return m_trailing_webfont_refs.GetCount() > 0; }

		/**
		* Rebuild the font family information. To be called when a webfont is removed.
		* @param removed_face The removed font face
		* @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM, otherwise OpStatus::ERR
		*/
		OP_STATUS RebuildFontInfo(FontVariant* removed_face);

	private:
		OpAutoVector<FontVariant>	fontvariants;	// The faces of this font family

		/**
		 * Removes a trailing web font from font manager and vector.
		 */
		OP_STATUS RemoveTrailingWebFontRef(int trailing_idx);
		OpVector<OpFontManager::OpWebFontRef>	m_trailing_webfont_refs;	// List of platform fontrefs that are being deleted
	};

	OpAutoVector<FontValue> m_font_list;			// The list of all known webfonts

#if 0 // this may be used later on
	class WebFontChangeListener : public Link
	{
	public:
		virtual OP_STATUS OnWebFontAdded(UINT32 font_number) = 0;
		virtual void OnWebFontRemoved(UINT32 font_number) = 0;
	};

	Head m_listeners; 							// The change listeners (to be informed of changes to the webfontmanager)

	//void AddListener(WebFontChangeListener* listener) { listener->Into(&m_listeners); }
#endif

	/**
	 * Get a webfont family corresponding to the given font number.
	 *
	 * @param fontnr The font number (as handed out by styleManager::LookupFontNumber)
	 * @param frm_doc The document that wants to use the font
	 * @return FontValue* The webfont family if a match was found, or NULL
	 */
	FontValue* GetFontValue(UINT32 fontnr, FramesDocument* frm_doc = NULL);

	/**
	 * Adds a new webfont to the manager.
	 *
	 * @param frm_doc The document that loaded the webfont
	 * @param font_elm The SVG <font> or <font-face> element if this was an svg webfont, otherwise NULL
	 * @param resolved_url The url that was used to load the font face (may be empty if this was an svg font defined only in markup)
	 * @param webfontref An opaque platform reference to the webfont (if the face was handled by the platform OpFontManager), otherwise NULL
	 * @param fi The font face descriptor containing information from the webfont (from the @font-face rule, or from svgfont markup)
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM, otherwise OpStatus::ERR
	 */
	OP_STATUS AddWebFontInternal(FramesDocument* frm_doc, HTML_Element* font_elm, URL resolved_url,
								 OpFontManager::OpWebFontRef webfontref,
								 OpFontInfo* fi, BOOL added_to_fontdb);

  public:
    WebFontManager();
    virtual ~WebFontManager();

	/**
	 * Checks if the given font family is allowed in the given FramesDocument.
	 *
	 * If the webfont manager doesn't have a webfont family (aka FontValue) for
	 * the given fontnumber then TRUE is returned.
	 * Otherwise we check that we have an allowed webfont face for the given
	 * document, and if so TRUE is returned.
	 * Otherwise we do a final check to see if the given fontnumber is a
	 * systemfont and if so then TRUE is returned.
	 * For all other cases FALSE is returned.
	 *
	 * @param fontnr The fontnumber of the font family
	 * @param frm_doc The FramesDocument to check against
	 * @return TRUE if the font family is allowed in the given document, FALSE if not
	 */
	BOOL AllowedInDocument(UINT32 fontnr, FramesDocument* frm_doc);

	/**
	 * Create a font object.
	 *
	 * This will load a font, meaning that a font is prepared for size
	 * measurement and rendering. A font is uniquely identified by name, size,
	 * weight and slant.
	 *
	 * This method will first check if there's a matching webfont for the
	 * given criteria, and if there is then that will be used, otherwise
	 * the method will return what platform OpFontManager::CreateFont
	 * returns (called with the same arguments).
	 * @see OpFontManager::CreateFont
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
	 * @param blur_radius the blur radius to use for bluring the glyphs
	 * before rendering them.
	 * @param frm_doc The document that wants to use the font
	 *
	 * @return the OpFont created, or NULL (out of memory, or no outline
	 * support while required). As already mentioned, the attributes of the
	 * returned font may not be identical to what was requested. For example, the
	 * font may not be available in italic, or perhaps no font by the specified
	 * name could be found. In any case, the implementation will find as good a
	 * substitute as possible.
	 */
	OpFont* CreateFont(const uni_char* face, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline, INT32 blur_radius, FramesDocument* frm_doc);

	/**
	 * Queries whether this fontnr is handled by the webfont manager.
	 * @return TRUE if the fontnumber is in the webfontmanager registry of webfonts, FALSE otherwise
	 */
	BOOL HasFont(UINT32 fontnr);

	/**
	 * Check if there is a webfont family corresponding to the given fontnumber
	 * that is currently waiting to delete one or more of its faces.
	 *
	 * @param fontnr The font number (as handed out by styleManager::LookupFontNumber)
	 * @param frm_doc The document that wants to know
	 */
	BOOL IsBeingDeleted(UINT32 fontnr, FramesDocument* frm_doc);

	/**
	 * Queries whether the platform supports a webfont format.
	 *
	 * @param format the format to query support for. A CSS_WebFont::Format disguised as an int.
	 * @return TRUE if the format is supported, FALSE otherwise.
	 */
	BOOL SupportsFormat(int format);

	/**
	 * fetches the path to the cache entry corresponding to the font
	 * file referred to by resolved_url. if the font file is a wOFF
	 * file an sfnt version will be created as needed, and fontfile
	 * will point to this version.
	 * @param resolved_url The url that contains the font itself must be loaded (URL_LOADED).
	 * @param fontfile (out)
	 * @return OpStatus::OK on success, OpStatus::ERR on error and OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS GetWebFontFile(URL& resolved_url, OpString& fontfile);

    /**
     * Adds a webfont face to the WebFontManager.
     *
     * @param resolved_url The url that contains the font itself must be loaded (URL_LOADED).
	 * @param elm The element pointed to in an @font-face src rule
	 * @param fontinfo The OpFontInfo that describes the font in the eyes of the style module (should contain face and fontnumber).
     * @return OpStatus::OK if successful, OpStatus::ERR on error and OpStatus::ERR_NO_MEMORY on OOM.
     */
	OP_STATUS AddCSSWebFont(URL resolved_url, FramesDocument* frm_doc, HTML_Element* elm, OpFontInfo* fontinfo);

	/**
	 * Adds a local font-face to the WebFontManager, a local font-face is a single face from a platform font family.
	 *
	 * @param frm_doc The FramesDocument that this local font-face belongs to
	 * @param fontinfo The OpFontInfo that describes the font in the eyes of the style module (should contain face and fontnumber).
	 * @param localfont The opaque platform reference to the local font
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AddLocalFont(FramesDocument* frm_doc, OpFontInfo* fontinfo, OpFontManager::OpWebFontRef localfont);

	/**
	 * Removes a font face from the webfont manager. When the last face in a family is removed the
	 * family is also removed from the font database owned by the styleManager.
	 *
	 * @param frm_doc The document that loaded the webfont
	 * @param font_elm The <font> or <font-face> element identifying an SVG font, or NULL if not an svg font
	 * @param resolved_url The url that was used to load the font face (may be an empty url for svg fonts defined in markup only)
	 */
	void RemoveCSSWebFont(FramesDocument* frm_doc, HTML_Element* font_elm, URL resolved_url);
};

#endif // WEBFONT_MANAGER_H
