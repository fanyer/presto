/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Peter Krefting
*/

#ifndef OPGENERATEDDOCUMENT_H
#define OPGENERATEDDOCUMENT_H

#include "modules/url/url2.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/locale/oplanguagemanager.h"

class TempBuffer;

/**
 * Framework for internally generated URLs, for example opera: URLs. This
 * class includes common setup code required for generated URLs, and
 * provides a base class that can be used when setting up generated
 * URLs.
 * 
 * When used as a generic interface, the user of the OpGeneratedDocument
 * object would only call the GenerateData() method to have the document
 * fully generated to the specified URL (or have an error reported on
 * failure).
 *
 * Subclasses that implement this interface should implement
 * GenerateData() to this specification, or make any diversions from it
 * well documented.
 *
 * To ease generation of documents, this class provides several
 * interfaces for its subclasses to generate common parts:
 *  - OpenDocument() (or SetupURL() for non-HTML documents)
 *  - OpenBody()
 *  - CloseDocument() (or FinishURL() for non-HTML documents)
 *  - Heading()
 *  - OpenUnorderedList(), AddListItem() and CloseList()
 *  - GeneratedByOpera()
 *  - IsXML()
 *
 * It also provides some services that are commonly used by
 * generated documents:
 *  - AppendLocaleString()
 *  - AppendHTMLified()
 *  - JSSafeLocaleString()
 */
class OpGeneratedDocument
{
public:
	/** Document types generated. */
	enum DocType
	{
		HTML5,				///< Generate HTML 5
		XHTML5,				///< Generate HTML 5 as XML
		XHTMLMobile1_2,		///< Generate XHTML Mobile 1.2
#ifdef GENERATE_XHTML_MOBILE_PAGES
		DEFAULT_GENERATED_PAGE_DOCTYPE = XHTMLMobile1_2
#else
		DEFAULT_GENERATED_PAGE_DOCTYPE = HTML5
#endif
	};
	/** Flags used with SetupURL */
	enum PageSetupOptions
	{
		NO_CACHE = 1, ///< Avoid caching of the generated page
		SKIP_BOM = 2  ///< Don't add a leading byte-order-mark to the page source
	};
	/**
	 * Standard constructor to use by inherited classes.
	 *
	 * @param url URL to write to.
	 * @param doctype
	 *     Document type to generate. Determines which doctype token to
	 *     include, and which syntax to use.
	 *     The doctype is set to HTML5 by default if not tweaked
	 *     with TWEAK_GENERATE_XHTML_MOBILE_PAGES, which will make it use
	 *     XHTML Mobile 1.2 as default instead.
	 * @param frame_options_deny
	 *     Whether this document should have X-Frame-Options: DENY header.
	 */
	OpGeneratedDocument(URL url, DocType doctype = DEFAULT_GENERATED_PAGE_DOCTYPE, BOOL frame_options_deny = FALSE)
		: m_url(url)
		, m_doctype(doctype)
		, m_frame_options_deny(frame_options_deny)
	{
#ifdef LOCALE_CONTEXTS
		g_languageManager->SetContext(m_url.GetContextId());
#endif
	}

	virtual ~OpGeneratedDocument();

	/**
	 * Generate the document. Implemented by the subclass.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData() = 0;

protected:
	// Services for subclasses.

	/**
	 * Set up a document to the given URL. The document is set up with a
	 * proper MIME type and character encoding, and a HTML header section
	 * is written to it with the provided data.
	 *
	 * The HEAD section of the document is not closed, allowing the
	 * caller to add extra data to the header. Call OpenBody() to close
	 * HEAD and open BODY.
	 *
	 * To perform the initial setup without writing start of the HTML
	 * document, call SetupURL() instead.
	 *
	 * @param title
	 *    Document title or Str::NOT_A_STRING to not include a title.
	 * @param stylesheet
	 *    Stylesheet to include. Default is not to use a stylesheet.
	 * @param allow_disk_cache
	 *    Allow caching of this document on disk. If FALSE, the "no store"
	 *    attribute will be set on the URL.
	 * @return OK on success, or any error reported by URL or string code.
	 */
	OP_STATUS OpenDocument(Str::LocaleString title
#ifdef _LOCALHOST_SUPPORT_
	    , PrefsCollectionFiles::filepref stylesheet = PrefsCollectionFiles::DummyLastFilePref
#endif
		, BOOL allow_disk_cache = TRUE
		);

	/**
	 * @overload
	 *
	 * @param title
	 *    Document title, or NULL not include a title.
	 * @param stylesheet
	 *    URL to stylesheet, or NULL not to include a stylesheet.
	 * @param allow_disk_cache
	 *    Allow caching of this document on disk. If FALSE, the "no store"
	 *    attribute will be set on the URL.
	 */
	OP_STATUS OpenDocument(const uni_char *title = NULL, const uni_char *stylesheet = NULL, BOOL allow_disk_cache = TRUE);

	/**
	 * Set up a document to the given URL. The document is set up with a
	 * proper MIME type and character encoding, and a byte order mark
	 * is written to it.
	 *
	 * @param page_options A bitwise-| of the enum PageSetupOptions.
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS SetupURL(unsigned int page_options);

	/**
	 * Close the head section and open the body for the URL. Closes the
	 * HEAD element opened by OpenDocument() and opens BODY. Optionally
	 * writes an H1 header to the document.
	 *
	 * Call CloseDocument() to close BODY and finish off the document.
	 *
	 * @param header
	 *   String to use in H1 heading, or Str::NOT_A_STRING to not produce
	 *   a H1 header.
	 * @param id
	 *   id attribute to use in the BODY tag, or NULL to not include any.
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS OpenBody(Str::LocaleString header, const uni_char *id = NULL);
	/**
	 * @overload
	 */
	OP_STATUS OpenBody();

	/**
	 * Finish off a document for the given URL. Closes the BODY element
	 * opened by OpenBody() and the HTML element opened by OpenDocument(),
	 * and then calls the proper methods signalling that the document is
	 * finished.
	 *
	 * After this method has been called, no more data can be written to
	 * the URL object.
	 *
	 * To perform the shutdown without writing end of the HTML document,
	 * call FinishURL() instead.
	 *
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS CloseDocument();

	/**
	 * Finish off a document for the given URL. Calls the proper methods
	 * signalling that the document is finished.
	 *
	 * After this method has been called, no more data can be written to
	 * the URL object.
	 *
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS FinishURL();

	/**
	 * Add a subheading to the document. This inserts an H2 element with
	 * the specified language string as its contents into the generated
	 * document.
	 *
	 * @param str Text to use in the subheading.
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS Heading(Str::LocaleString str);

	/**
	 * Open an unordered list. This inserts a UL tag into the generated
	 * document.
	 *
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS OpenUnorderedList();

	/**
	 * Add a bullet-point to a list opened by OpenUnorderedList(). This
	 * inserts a LI element with the specified language string as its
	 * contents into the generated document.
	 *
	 * @param str Text to use in the bullet-point.
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS AddListItem(Str::LocaleString str);

	/**
	 * Close a list opened by OpenUnorderedList(). This inserts a UL end
	 * tag into the generated document.
	 *
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS CloseList();

#ifdef USE_ABOUT_TEMPLATES
	/**
	 * Create an internal document from a template
	 *
	 * NB: Currently, this will just copy the contents of the given HTML
	 *     document to the URL associated with this page. No substitutions
	 *     are done. You probably don't want to use this, unless you are
	 *     using webfeeds preview.
	 *
	 * TODO Make this function do more, so that it can be used for all about
	 *      pages.
	 *
	 * @param template_file
	 *    Filename of template as a file pref
	 * @param allow_disk_cache
	 *    Allow caching of this document on disk. If FALSE, the "no store"
	 *    attribute will be set on the URL.
	 * @return OK on success, or any error reported by OpFile or URL code.
	 */
	OP_STATUS GenerateDataFromTemplate(PrefsCollectionFiles::filepref template_file, BOOL allow_disk_cache = TRUE);
#endif // USE_ABOUT_TEMPLATES

	/**
	 * Add a notice saying that the document is generated by Opera. In
	 * cases where we replace a document by a generated page (for
	 * instance a readirect message, or fraud warning), we should
	 * make it clear to the user that the document was generated by
	 * Opera. This method inserts an ADDRESS element with a string
	 * saying that the document was generated by Opera.
	 *
	 * @return OK on success, or any error reported by URL code.
	 */
	OP_STATUS GeneratedByOpera();

	/**
	 * Check if the document uses XML/XHTML syntax.
	 */
	inline BOOL IsXML()
	{ return m_doctype == XHTML5 || m_doctype == XHTMLMobile1_2; }
public:
	/**
	 * Internal helper for appending a translated string to an OpString.
	 * Ideally, this should be available in OpString.
	 *
	 * @param s  String that will have the translated string appended.
	 * @param id String to append.
	 * @return OK on success, or any error reported by string code.
	 */
	static OP_STATUS AppendLocaleString(OpString *s, Str::LocaleString id);

	/**
	 * Internal helper for appending a HTMLified string to an OpString.
	 * Converts the input string to an HTML-safe version and appends it
	 * to the given string.
	 *
	 * @param s String that will have the HTMLified string appended.
	 * @param string String to append.
	 * @param string_length Length of string to append.
	 * @return OK on success, or any error reported by string code.
	 */
	static OP_STATUS AppendHTMLified(OpString *s, OpStringC string, unsigned string_length);
	static OP_STATUS AppendHTMLified(TempBuffer *s, OpStringC string, unsigned string_length);
	static OP_STATUS AppendHTMLified(OpString *s, const char *str, unsigned len);

	/**
	 * Get a JavaScript-safe translated string.
	 *
	 * @param s  String that will receive the escaped translated string.
	 * @param id String to append.
	 * @return OK on success, or any error reported by string code.
	 */
	static OP_STATUS JSSafeLocaleString(OpString *s, Str::LocaleString id);

	/**
	 * Function for stripping all html tags but links from html code.
	 * HTML A tags are left while all other tags are stripped.
	 * BR tags are replaced with single space.
	 * Event handler attributes (onclick etc.) are sanitized.
	 *
	 * @param string Buffer with input string that will be replaced with sanitized output.
	 * @param length Length of the input string.
	 */
	static void StripHTMLTags(uni_char* string, unsigned length);

protected:
	URL m_url;					///< URL object representing this document.
	enum DocType m_doctype;		///< Document type for this document.
	BOOL m_rtl;					///< Flag telling if document is right-to-left.
	const BOOL m_frame_options_deny;	///< Whether X-Frame-Options header should be set to DENY.
};

#endif
