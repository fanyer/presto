/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef LOGDOC_UTIL_H
#define LOGDOC_UTIL_H


/**
 * @file logdoc_util.h
 * Various document utility functions.
 * This header file defines a number of utility functions used mainly by
 * the document-related code in Opera.
 */

/** Value to indicate infinite number of loops. */
static const int LoopInfinite = INT_MAX;

/** Non-breaking space (U+00A0). */
static const uni_char NBSP_CHAR = 160;

/** Character used to force line breaks. */
static const uni_char FORCE_LINE_BREAK_CHAR = 11;

class URL;
class HLDocProfile;
class HTML_Element;
class HtmlAttrEntry;
class LoadInlineElm;


/**
 * Look up enumerated HTML attributes.
 *
 * @param str Attribute name.
 * @param len Length of attribute name.
 * @return Unique numeric value for attribute, or 0 if not found.
 */
BYTE ATTR_GetKeyword(const uni_char* str, int len);

/**
 * Helper to set the loop attribute from a value.
 *
 * @param str Attribute string.
 * @param len Length of attribute string.
 * @return Coded value for the loop attribute. Might be negative if the string contained a negative value.
 */
int SetLoop(const uni_char* str, UINT len);

/**
 * Asks StyleManager for the font number of the faces in
 * face_list, one at a time. When first a font number is found and
 * StyleManager also says that this font indeed does exist, it returns
 * this font number.
 * (rg)
 *
 * @param face_list the list of faces
 * @return the font number of the face that did exist
 */
short GetFirstFontNumber(const uni_char* face_list, int face_list_len, WritingSystem::Script script);

/**
 * Check if character string contains entirely of U+0020 SPACE or uni_iscntrl
 * control characters.
 *
 * @param txt Character string to check.
 * @param len Length of character string to check.
 * @return TRUE if only U+0020 or control characters were found in txt.
 */
BOOL WhiteSpaceOnly(const uni_char* txt, int len);
BOOL WhiteSpaceOnly(const uni_char* txt);

/**
 * Folds sequences of whitespace into one. Replaces multiple whitespaces
 * (uni_isspace) with a single whitespace character.
 *
 * @param src_txt Input text.
 * @param src_txt_len Length of input text, in uni_chars.
 * @param target_txt Output text buffer.
 * @param target_txt_len
 *    Length of output text buffer, needs to be one less than the true size
 *    to allow for a final nul character.
 * @param preserve_whitespace_type
 *    If TRUE, the first whitespace character in the run will be preserverd.
 *    If FALSE, the entire run will be replaced with U+0020 (SPACE).
 */
void ReplaceWhitespace(const uni_char *src_txt, int src_txt_len,
					   uni_char *target_txt, int target_txt_len,
					   BOOL preserve_whitespace_type = FALSE);

/**
 * Replace entity and character references
 *
 * It replaces entity references regardless of whether they are terminated by
 * a semicolon or not (as dictated by HTML and SGML) and also replaces both
 * decimal and hexadecimal character references. If require_termination is
 * true only entity references explicitly terminated with ';' are replaced.
 * This is done to avoid replacing server-side script parameters.
 *
 * @param txt Text string to replace entity references in.
 * @param txt_len Length of txt in uni_chars.
 * @param require_termination Require ';' termination for all references.
 * @param remove_tabs Remove tab references from string.
 * @param treat_nbsp_as_space Changes nbsp references to normal spaces.
 * @return The new length of the string.
 *
 * See HTML 4.0 spec "5.3 Character References".
 */
int ReplaceEscapes(uni_char *txt, int txt_len,
				   BOOL require_termination,
				   BOOL remove_tabs,
				   BOOL treat_nbsp_as_space);

/**
 * @overload
 * This version assumes a regular nul-terminated string.
 */
int ReplaceEscapes(uni_char *txt,
				   BOOL require_termination,
				   BOOL remove_tabs = FALSE,
				   BOOL treat_nbsp_as_space = FALSE);

/**
 * Strip all tab chars ('\t') from the string
 *
 * @param[in] txt The text that may contain tab chars to remove. Must not be NULL. Must be null terminated.
 */
void RemoveTabs(uni_char* txt);

/**
 * Encode the forms data part of a URL to the document character encoding.
 * The text will be converted to the target encoding, with any non-ASCII
 * character URL encoded (%xx).
 *
 * @param charset target encoding.
 * @param url URL string, will be in-place converted.
 * @param startidx_arg Character to start encoding at.
 * @param len Length of the URL string, will be updated to the new length on
 *            exit.
 * @param maxlen Maximum length of the buffer pointed to by url.
 */
void EncodeFormsData(const char *charset,
					 uni_char *url,
					 size_t startidx_arg,
					 unsigned int &len,
					 unsigned int maxlen);

/**
 * Removes leading and trailing white space.
 * Removes newline characters.
 * Replaces linefeed and tab characters with space.
 *
 * @param[in,out] url_name the string to clean. The buffer must be |len|+1 long since the returned string will be null terminated.
 * @param[in,out] len the string length in and when returning, will contain the final length of the data
 */
void CleanCDATA(uni_char* url_name, unsigned int &len);

/**
 * Removes leading and trailing white space and collapses any
 * internal whitespace to a single space character. Used to
 * convert html/xml data to something viewable. For instance page titles.
 *
 * @param[in,out] string the null-terminated string to clean.
 */
void CleanStringForDisplay(uni_char* string);


/**
 * Convert a hexidecimal string to an integer. The input string does not need
 * to be nul-terminated.
 *
 * @param str Pointer to a hexidecimal number.
 * @param len Length of the hexadecimal number, in uni_chars.
 * @param strict
 *    Exit on error if TRUE.
 *    Treat non-hexadecimal characters as zeroes if FALSE.
 * @return -1 on error (only if strict is TRUE), or the integer corresponding
 *         to the hexadecimal string in str.
 */
int HexToInt(const uni_char* str, int len, BOOL strict = FALSE);

/**
 * Convert a color value into a numeric value. Used to parse color attributes so it will be able to
 * handle leading and trailing space.
 *
 * @param coltxt Pointer to a color string.
 * @param collen Length of the color string.
 * @param check_color_name Allow named colors.
 * @return The color represented by coltxt, or USE_DEFAULT_COLOR if shorter than 3 characters.
 */
long GetColorVal(const uni_char* coltxt, int collen, BOOL check_color_name);

/**
 * Replace HTML entities in hae->value.
 *
 * @param[in] hae The attribute data to fix.
 *
 * @returns OpBoolean::IS_TRUE if the hae->value and hae->value_len was
 * replaced and in that case hae->value is owned by the caller. If it returns
 * OpBoolean::IS_FALSE nothing had to be done. If something needed to be done
 * but there isn't enough memory OpStatus::ERR_NO_MEMORY is returned.
 */
OP_BOOLEAN	ReplaceAttributeEntities(HtmlAttrEntry* hae);

/**
 * Create an URL from an attribute value.  Character and entity
 * references are replaced, unless hld_profile->IsXML() is TRUE, in
 * which case the XML parser is assumed to have replaced them already
 * (and replacing again would be plain wrong).
 *
 * @see CreateUrlFromString().
 *
 * @param value Pointer to the URL string attribute.
 * @param value_len Length of value.
 * @param base_url Base URL for this link.
 * @param hld_profile Parent document.
 * @param check_for_internal_img
 *   Expand Opera-internal image references into full URLs.
 * @param accept_empty Allow empty relative URLs.
 * @return The URL corresponding to the string value.
 */
URL* SetUrlAttr(const uni_char* value, UINT value_len, URL* base_url, HLDocProfile *hld_profile, BOOL check_for_internal_img, BOOL accept_empty);

/**
 * Create an URL from a string that is assumed not to contain
 * character or entity references (that is, & is not special in any
 * way).
 *
 * @see SetUrlAttr().
 *
 * @param value Pointer to the URL string attribute.
 * @param value_len Length of value.
 * @param base_url Base URL for this link.
 * @param hld_profile Parent document.
 * @param check_for_internal_img
 *   Expand Opera-internal image references into full URLs.
 * @param accept_empty Allow empty relative URLs.
 * @param set_context_id If TRUE logdocs current url context will be used
 * @return The URL corresponding to the string value.
 */
URL* CreateUrlFromString(const uni_char *value, UINT value_len,
						 URL* base_url, HLDocProfile *hld_profile,
						 BOOL check_for_internal_img, BOOL accept_empty
#ifdef WEB_TURBO_MODE
						 , BOOL set_context_id = FALSE
#endif // WEB_TURBO_MODE
						 );

/**
 * Creates a tabindex attribute value.
 *
 * @param value Pointer to the URL string attribute.
 * @param value_len Length of value.
 * @param[out] return_value The numberic value
 * @return TRUE if there was a numeric value, FALSE otherwise
 */
BOOL	SetTabIndexAttr(const uni_char *value, int value_len, HTML_Element *elm, long& return_value);

/**
 * Allocates a new nul-terminated string and copies the string in the value
 * parameter to it, and then performs the requested whitespace and character
 * entity transformations.
 *
 * @see SetStringAttrUTF8Escaped().
 *
 * @param value Attribute value string.
 * @param value_len Length of attribute value string.
 * @param replace_ws Fold whitespaces in value into U+0020 (SPACE).
 * @return The new attribute string, or NULL on OOM.
 */
uni_char* SetStringAttr(const uni_char* value, int value_len, BOOL replace_ws);

/**
 * @deprecated Only has three arguments now
 */
DEPRECATED(uni_char* SetStringAttr(const uni_char* value, int value_len, BOOL replace_ws, BOOL unused1, BOOL unused2));

/**
 * Allocates a new nul-terminated string and copies the string in the value
 * parameter to it, and convert non-ASCII characters to URL escapes. Any
 * non-ASCII characters are converted to URL escapes according to the UTF-8
 * URL settings, with forms data using the local character encoding for the
 * parent document referenced by hld_profile.
 *
 * @see SetStringAttr().
 *
 * @param value Attribute value string.
 * @param value_len Length of attribute value string.
 * @param hld_profile HLDocProfile of the parent document
 * @return The new attribute string, or NULL on OOM.
 */
uni_char *SetStringAttrUTF8Escaped(const uni_char *value, int value_len,
								   HLDocProfile *hld_profile);

/**
 * Retrieve the base target for this element. If there is no base target
 * specified on the element itself, it searches backward in the document
 * to where one can be located.
 *
 * @param from_he Element to search.
 * @return Base target for the element, or NULL if none was found.
 */
const uni_char* GetCurrentBaseTarget(HTML_Element* from_he);

/**
 * Returns TRUE if the HTML element type type is an inline element
 *
 * @param type Type of the HTML element
 * @param hld_profile The HLDocProfile of the document the element belongs to
 * @return TRUE if the element is inline
 */
BOOL IsHtmlInlineElm(HTML_ElementType type, HLDocProfile *hld_profile);

class AutoTempBuffer
{
private:
	TempBuffer*		m_buf;

public:
	AutoTempBuffer();
	~AutoTempBuffer();

	TempBuffer* operator->() { return m_buf; }
	TempBuffer* GetBuf() { return m_buf; }
};

/**
 * Get suggested encoding for a sub-resource. If inheritance is allowed,
 * the sub-resource should inherit the encoding 1) the CHARSET attribute
 * of the html_element or 2) the encoding of hld_profile.
 *
 * If you need to retain the numeric encoding ID beyond the call to this
 * function, you need to call CharsetManager::IncrementCharsetIDReference()
 * on the value before calling any other methods that might modify
 * CharsetManager.
 *
 * @param html_element The HTML element linking to the sub-resource.
 * @param hld_profile The document holding the link.
 * @param elm The LoadInlineElm object keeping track of the sub-resource
 *   we want to load.
 * @return ID of the suggested encoding as returned by CharsetManager.
 */
unsigned short int GetSuggestedCharsetId(HTML_Element *html_element, HLDocProfile *hld_profile, LoadInlineElm *elm);

#endif // LOGDOC_UTIL_H
