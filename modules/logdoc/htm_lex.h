/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _HTM_LEX_H_
#define _HTM_LEX_H_

class HLDocProfile;
class LogdocXmlName;
#include "modules/logdoc/namespace.h"
#include "modules/layout/layout.h" // backwards compatibility
#include "modules/logdoc/logdocenum.h" // backwards compatibility
#include "modules/logdoc/complex_attr.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/logdoc/html5namemapper.h"

#define HTM_LEX_END_OF_BUF	-1 ///< All the contents of the buffer has been parsed
#define HTM_LEX_MUST_GROW_BUF   -2 ///< The buffer must grow to be parsed correctly
#define HTM_LEX_END_TOKEN_NOT_FOUND	-3 ///< Could not find the requested end token

#define HTM_LEX_NS_PREFIX   0x0001
#define HTM_LEX_NS_ATTR     0x0002
#define HTM_LEX_NS_XMLNS    0x0004
#define HTM_LEX_NS_DEFAULT  0x0008
#define HTM_LEX_NS_CHANGED  0x0010

const int EndTokenBit	= 1024;
#define HTM_LEX_END_TOKEN(x)    ((x) & EndTokenBit) ///< Is true if the token is an end token
const int BackTokenBit	= 2048;
#define HTM_LEX_BACK_TOKEN(x)   ((x) & BackTokenBit) ///< Is true if the token is a terminating token
const int EmptyTokenBit	= 4096;
#define HTM_LEX_EMPTY_TOKEN(x)  ((x) & EmptyTokenBit) ///< Is true if token represents an empty element <elm/>
const int TerminatorBit	= 8192;
#define HTM_LEX_TERMINATOR(x)	((x) & TerminatorBit) ///< Terminates like back token but does not stop at TABLE

/**
 * The length of the static attribute array, 256 is the limit in HTML_Element
 * and with a few extra reserved for special attributes we get 246. */
const int	HtmlAttrEntriesMax	= 246;

/** Class to store names of xml- and unknown-elements as an attribute. */
class LogdocXmlName : public ComplexAttr
{
public:
	LogdocXmlName() {}
	virtual ~LogdocXmlName() {}

	XMLCompleteName&	GetName() { return m_name; }
	OP_STATUS			SetName(const XMLCompleteNameN& name);
	OP_STATUS			SetName(const uni_char *qname, unsigned int qname_len, BOOL is_xml);
	OP_STATUS			SetName(LogdocXmlName *src);

	/** Virtual methods from ComplexAttr */
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to);
	virtual OP_STATUS	ToString(TempBuffer *buffer);

private:
	XMLCompleteName		m_name; ///< The name of the element
};

#include "modules/logdoc/src/htmlattrentry.h"

/// Helper class for parsing HTML.
/// Mostly used for converting string values to numeric codes and back.
class HTM_Lex
{
private:

	HtmlAttrEntry*	m_attr_array; ///< Static table of attribute data

	LogdocXmlName	m_elm_name; ///< Static member for holding a element name entry.

public:

	/// Constants returned by GetAttrValue()
	enum
	{
		ATTR_RESULT_FOUND, ///< Found an attribute, must continue for more
		ATTR_RESULT_DONE, ///< Found no more attributes, done parsing them
		ATTR_RESULT_NEED_GROW ///< Need more data to find end of attribute
	};

			HTM_Lex() {}
			~HTM_Lex();
	void	InitL();

	/// Returns the static attribute data array used during parsing.
	HtmlAttrEntry*	GetAttrArray() { return m_attr_array; }

	/// Searches an attribute array for a specific attribute and returns the value
	///\param[in] attr Numeric code for the attribute
	///\param[in] html_attrs The array of attribute data
	///\param[out] val_start Pointer to the start of the value string
	///\param[out] val_len Length of the value returned
	static void	FindAttrValue(int attr, HtmlAttrEntry* html_attrs, uni_char* &val_start, UINT &val_len);

	/// Returns the string corresponding to the element type code tag 
	/// in the namespace given by the namespace index ns_idx. If in the NS_HTML namespace, html_uppercase
	/// controls if you want a upper-case only representation.
	static const uni_char*	GetElementString(Markup::Type tag, int ns_idx, BOOL html_uppercase = FALSE);
	/// Returns the string corresponding to the attribute type code attr
	///  in the namespace given by the namespace index ns_idx
	static const uni_char*	GetAttributeString(Markup::AttrType attr, NS_Type ns_type);

	/// Returns the string corresponding to the HTML element type code tag
    static const uni_char*  GetTagString(Markup::Type tag);
	/// Returns the string corresponding to the HTML attribute type code attr
	static const uni_char*  GetAttrString(Markup::AttrType attr) { return g_html5_name_mapper->GetAttrNameFromType(attr, Markup::HTML, FALSE); }
	/// Returns the color name corresponding to the color index idx into the HTML color table.
    static const uni_char*  GetColNameByIndex(int idx);

	/// Returns the RGB value of a composite color value as a string, for instance rgb(12,12,43) or rgba(124,31,32,0.5) or #FEFEFE
	///\param[in] color Composite color value
	///\param[in] buffer The string will be copied to that buffer which must be long enough to hold it. About 30 chars seems to be the max length right now ("rgba(255,255,255,0.0038)").
	///\param[in] hash_notation Will use the form #RRGGBB if TRUE, otherwise rgb(rr, gg, bb). Will be ignored if there is a non-full alpha in the color.
    static void     GetRGBStringFromVal(COLORREF color, uni_char *buffer, BOOL hash_notation = FALSE);
	/// Returns the numeric RGB value for an indexed color value.
	///\param[in] idx a COLORREF that refers to a named/X11 color or a UI color, i. e. COLOR_is_indexed(idx).
	///\return    if idx is a CSS_COLOR_KEYWORD_TYPE_x_color or a CSS_COLOR_KEYWORD_TYPE_ui_color, returns the ABGR tuple corresponding to that index. Otherwise, returns idx unchanged.
    static COLORREF GetColValByIndex(COLORREF idx);
	/// Returns the numeric RGB values for a color by name.
	///\param[in] str Name of color, need not be nul terminated
	///\param[in] len Length of string str
	///\param[out] idx Returns index into color table for matched color
    static COLORREF GetColValByName(const uni_char* str, int len, int &idx);
	/// Returns the composite color value for a named color.
	///\param[in] str Name of color, need not be nul terminated
	///\param[in] len Length of string str
    static COLORREF GetColIdxByName(const uni_char* str, int len);

	/// Returns the first and second character of a byte value.
	///\param[in] val The value wanted in hex, only the lowest 8 bits used
	///\param[out] c1 First character of hex value
	///\param[out] c2 Second character of hex value
    static void     IntToHex(int val, uni_char &c1, uni_char &c2);

	/// Returns the numeric HTML element type code from a string.
	///\param[in] str The HTML tag name, need not be nul terminated
	///\param[in] len Number of characters in str
	///\param[in] case_sensitive TRUE if the matching should be case sensitive
	static Markup::Type	GetTag(const uni_char* str, int len, BOOL case_sensitive = FALSE);
	/// Returns the numeric HTML attribute type code from a string.
	///\param[in] str The HTML attribute name, need not be nul terminated
	///\param[in] len Number of characters in str
	///\param[in] case_sensitive TRUE if the matching should be case sensitive
	static Markup::AttrType	GetAttr(const uni_char* str, int len, BOOL case_sensitive = FALSE);
	/// Returns the numeric XML event type code from a string. Matching is case sensitive
	///\param[in] str The XML event attribute name, need not be nul terminated
	///\param[in] len Number of characters in str
    static int  GetXMLEventAttr(const uni_char* str, int len);

	/// Searches through a buffer and returns the content of an element with a given id.
	/// Returns the token for the matched element or one of these values:
	///  - HTM_LEX_END_OF_BUF: nothing found
	///  - HTM_LEX_MUST_GROW_BUF: need more data, not found at all or start found
	///\param[in] id The id string to search for, will only match 'id="something"'
	///\param[in] buf_start Start of the buffer to search
	///\param[in] buf_len Length of the buffer pointed to by buf_start
	///\param[out] content Content of the found element including the start and end tag
	///\param[out] content_len Length of the content found
	///\param[out] token_ns_idx Namespace index of the element found
	///\param[in] can_grow TRUE if more data can be put into the buffer from the stream
	static int	GetContentById2(const uni_char* id, uni_char *buf_start, int buf_len, uni_char*& content, int &content_len, int &token_ns_idx, BOOL can_grow);

	/// Returns the numeric element type code for the given namespace from a string with given length.
	///\param[in] tok The tag name, need not be null terminated
	///\param[in] tok_len Number of characters in str
	///\param[in] ns_type Namespace type the tag will be matched in
	///\param[in] case_sensitive TRUE if the matching should be case sensitive
	///\returns The type matching an entry in the element name tables, otherwise HTE_UNKNOWN
    static Markup::Type	GetElementType(const uni_char *tok, int tok_len, NS_Type ns_type, BOOL case_sensitive = FALSE);
	/// Returns the numeric element type code for the given namespace from a null terminated string.
	///\param[in] tok The tag name, must be null terminated
	///\param[in] ns_type Namespace type the tag will be matched in
	///\param[in] case_sensitive TRUE if the matching should be case sensitive
	///\returns The type matching an entry in the element name tables, otherwise HTE_UNKNOWN
	static Markup::Type	GetElementType(const uni_char *tok, NS_Type ns_type, BOOL case_sensitive = FALSE);
	/// Returns the numeric attribute type code for the given namespace from a string with given length.
	///\param[in] tok The attribute name, need not be null terminated
	///\param[in] tok_len Number of characters in str
	///\param[in] ns_type Namespace type the attribute will be matched in
	///\param[in] case_sensitive TRUE if the matching should be case sensitive
    static Markup::AttrType	GetAttrType(const uni_char *tok, int tok_len, NS_Type ns_type, BOOL case_sensitive = FALSE);
	/// Returns the numeric attribute type code for the given namespace from a null terminated string.
	///\param[in] tok The attribute name, must be null terminated
	///\param[in] tok_len Number of characters in str
	///\param[in] ns_type Namespace type the attribute will be matched in
	///\param[in] case_sensitive TRUE if the matching should be case sensitive
	static Markup::AttrType	GetAttrType(const uni_char *tok, NS_Type ns_type, BOOL case_sensitive = FALSE);

	/// Returns TRUE if the last characters, starting from buf_end, of the buffer buf_start
	/// contains references to entities.
	///\param[in] buf_start Start of the buffer to scan.
	///\param[in,out] buf_end. Points to the end of the buffer (the character after the last in the string) and will point to before the reference after
	///\param[in] allowed_to_read_next_char If you are able to look one char ahead, better decisions might be made. This tells the function if that is possible.
	static BOOL	ScanBackIfEscapeChar(const uni_char* buf_start, uni_char** buf_end, BOOL allowed_to_read_next_char);

	/// Internal: Used to find the next attribute in a start tag.
	///\param[out] buf_start The start of the buffer to parse, will be moved to after last match, need not be nul terminated
	///\param[in] buf_end The first character after the last character in the buffer buf_start
	///\param[out] ha_list Attribute data structure to be filled with the result of the parsing
	///\param[in] strict_html Will stop at first '>' even in quoted strings if FALSE
	///\param[in] hld_profile Yeah, you know
	///\param[in] can_grow Is there more data in the stream that didn't go into the buffer
	///\param[in] elm_ns The namespace of the element used to derive default namespace for attrs
	///\param[in] level The depth in the logical tree for the element you parse attributes for
	///\param[in] resolve_ns TRUE if we should look for xml namespace declarations.
	int		GetAttrValue(uni_char** buf_start, uni_char* buf_end, HtmlAttrEntry* ha_list, BOOL strict_html,
				HLDocProfile *hld_profile, BOOL can_grow, NS_Type elm_ns, unsigned level, BOOL resolve_ns);
};

/**
 * HTML5 has a simplified view of what a space is. Probably to not
 * have to handle changes to the Unicode spec and to keep a parser
 * simpler and more backwards compatible. Spaces are SPACE, TAB, CR,
 * LF and FormFeed.
 * See From http://www.w3.org/html/wg/html5/#space
 *
 * @returns TRUE if the char is a space according to HTML5.
 */
inline BOOL IsHTML5Space(const uni_char c)
{
	// From http://www.w3.org/html/wg/html5/#space

	// The space characters, for the purposes of this specification,
	// are U+0020 SPACE, U+0009 CHARACTER TABULATION (tab),
	// U+000A LINE FEED (LF), U+000C FORM FEED (FF), and
	// U+000D CARRIAGE RETURN (CR).

	return c == 0x20 ||c == 0x09 || c == 0x0a || c == 0x0c || c == 0x0d /* 0x0d is normalized to 0x0a in the spec but not in Opera so we have to handle it as well*/;
}

inline BOOL IsHTML5ASCIILetter(const uni_char c)
{
	// TODO: Check which implementation is the fastest
	// return uni_isascii(c) && uni_isalpha(c);
	return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z';
}

#endif /* _HTM_LEX_H_ */
