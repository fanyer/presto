/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LOGDOCENUM_H
#define LOGDOCENUM_H

class SpecialNs
{
public:
	enum Ns
	{
		NS_NONE = 0,
		NS_ADBROWSER,
		NS_AUTOPROXY,
		NS_CONSOLE,
		NS_DATASTREAM,
		NS_DEBUG,
		NS_DISPLAY,
		NS_DOC,
		NS_DOCHAND,
		NS_DOCEDIT,
		NS_DOCUTIL,
		NS_DOM,
		NS_ECMASCRIPT,
		NS_ECMAUTILS,
		NS_ENCODINGS,
		NS_FORMS,
		NS_HARDCORE,
		NS_IMG,
		NS_JAVA,
		NS_JSPLUGINS,
		NS_LAYOUT,
		NS_LIBJPEG,
		NS_LOCALE,
		NS_LOGDOC,
		NS_M2,
		NS_MEMTOOLS,
		NS_NS4PLUGINS,
		NS_OGP,
		NS_PREFS,
		NS_QUICK,
		NS_REGKEY,
		NS_SELFTEST,
		NS_SKIN,
		NS_SOFTCORE,
		NS_SPATNAV,
		NS_STYLE,
		NS_SVG,
		NS_MEDIA,
		NS_URL,
		NS_UTIL,
		NS_VOICE,
		NS_WAND,
		NS_WIDGETS,
		NS_WINCOM,
		NS_XML
	};
};

typedef enum
{
	ITEM_TYPE_UNDEFINED,
	ITEM_TYPE_BOOL, // value is either FALSE or TRUE
	ITEM_TYPE_NUM, // value is a INTPTR
	ITEM_TYPE_STRING, // value is a pointer to a uni_char string
	ITEM_TYPE_NUM_AND_STRING, // value is a pointer to a INTPTR and value+sizeof(INTPTR) is a pointer to a uni_char string
	ITEM_TYPE_URL,
	ITEM_TYPE_URL_AND_STRING, // value is a pointer to an URL* and value+sizeof(URL*) is a pointer to a uni_char string
	ITEM_TYPE_CSS,
	ITEM_TYPE_COORDS_ATTR,
	ITEM_TYPE_PRIVATE_ATTRS,
#ifdef XML_EVENTS_SUPPORT
	ITEM_TYPE_XML_EVENTS_REGISTRATION,
#endif // XML_EVENTS_SUPPORT
	ITEM_TYPE_COMPLEX // value is a pointer to a ComplexAttr (base class for a number of more complex attribute values)
} ItemType;

typedef enum
{
	BOUNDING_BOX,		///< Enclosing border box, outline, and overflowing content. (Relative to beginning of document)
	/** Currently identical with BOUNDING_BOX, but will be changed (see CORE-44021).
	 * Should be like BOUNDING_BOX, but inside outline
	 * (still including overflowing content).
	 */
	ENCLOSING_BOX,
	CLIENT_BOX,			///< Inside border, outside padding. (Relative to its own border)
	CONTENT_BOX,		///< Inside border and padding. (Relative to beginning of document)

	OFFSET_BOX,			///< Offset relative to containing element. Inside outline.
	SCROLL_BOX,			///<
	BORDER_BOX,			///< Border box, as defined by the CSS specification (Relative to beginning of document)
	PADDING_BOX			///< Padding box, as defined by the CSS specification (Relative to beginning of document)
} BoxRectType;

enum
{
	SCROLLING_NO = 0,
	SCROLLING_YES = 1,
	SCROLLING_AUTO = 2,
	SCROLLING_HOR_AUTO = 3
};

enum InlineResourceType
{
	INVALID_INLINE = -1,
	IMAGE_INLINE,
	EMBED_INLINE,
	SCRIPT_INLINE,
	CSS_INLINE,
	GENERIC_INLINE,
	ICON_INLINE,
	BGIMAGE_INLINE,
	EXTRA_BGIMAGE_INLINE,
	BORDER_IMAGE_INLINE,
	VIDEO_POSTER_INLINE,
	TRACK_INLINE,
	WEBFONT_INLINE
};

#define ATTR_SRC_LIST Markup::LOGA_SRC_LIST
#define ATTR_INLINE_ONLOAD_SENT Markup::LOGA_INLINE_ONLOAD_SENT
#define ATTR_CSS Markup::LOGA_CSS
#define ATTR_FONT_NUMBER Markup::LOGA_FONT_NUMBER
#define ATTR_JS_ELM_IDX Markup::LOGA_JS_ELM_IDX
#define ATTR_JS_FORM_IDX Markup::LOGA_JS_FORM_IDX
#define ATTR_XML_NAME Markup::LOGA_XML_NAME
#define ATTR_CSS_LINK Markup::LOGA_CSS_LINK
#define ATTR_JS_SCRIPT_HANDLED Markup::LOGA_JS_SCRIPT_HANDLED
#define ATTR_JS_DELAYED_ONLOAD Markup::LOGA_JS_DELAYED_ONLOAD
#define ATTR_JS_DELAYED_ONERROR Markup::LOGA_JS_DELAYED_ONERROR
#define ATTR_XML_EVENTS_REGISTRATION Markup::LOGA_XML_EVENTS_REGISTRATION
#define ATTR_SCROLL_X Markup::LOGA_SCROLL_X
#define ATTR_SCROLL_Y Markup::LOGA_SCROLL_Y
#define ATTR_OLD_IMAGE_URL Markup::LOGA_OLD_IMAGE_URL             // functionality moved to HEListElm
#define ATTR_FORMOBJECT_BACKUP Markup::LOGA_FORMOBJECT_BACKUP
#define ATTR_JS_TEXT_IDX Markup::LOGA_JS_TEXT_IDX
#define ATTR_LOCALSRC_URL Markup::LOGA_LOCALSRC_URL
#define ATTR_HIGHEST_USED_FORM_ELM_NO Markup::LOGA_HIGHEST_USED_FORM_ELM_NO
#define ATTR_FORM_VALUE Markup::LOGA_FORM_VALUE
#define ATTR_IME_STYLING Markup::LOGA_IME_STYLING				// For setting the properties on the composestring when IME is used in editable documents.
#define ATTR_MARQUEE_STOPPED Markup::LOGA_MARQUEE_STOPPED			// A script has stopped the marquee
#define ATTR_PRIVATE Markup::LOGA_PRIVATE
#define ATTR_ALTERNATE Markup::LOGA_ALTERNATE
#define ATTR_CDATA Markup::LOGA_CDATA
#define ATTR_LOGDOC Markup::LOGA_LOGDOC
#define ATTR_RES_URL_CACHE Markup::LOGA_RES_URL_CACHE
#define ATTR_PLUGIN_ACTIVE Markup::LOGA_PLUGIN_ACTIVE
#define ATTR_PLUGIN_EXTERNAL Markup::LOGA_PLUGIN_EXTERNAL
#define ATTR_PREFOCUSED Markup::LOGA_PREFOCUSED
#define ATTR_WML_SHADOWING_TASK Markup::LOGA_WML_SHADOWING_TASK		///< WML DO element shadowing earlier DO. The element should be hidden if TRUE
#define ATTR_OP_APPLET Markup::LOGA_OP_APPLET					// Stored OpApplet object for shadow dom for print documents.
#define ATTR_TEXT_CONTENT Markup::LOGA_TEXT_CONTENT
#define ATTR_XMLDOCUMENTINFO Markup::LOGA_XMLDOCUMENTINFO
#define ATTR_STYLESHEET_DISABLED Markup::LOGA_STYLESHEET_DISABLED
#define ATTR_NOSCRIPT_ENABLED Markup::LOGA_NOSCRIPT_ENABLED          ///< Set to true if we should render the contents of this noscript tag, else we parse it as text and do not display it.
#define ATTR_PLUGIN_PLACEHOLDER Markup::LOGA_PLUGIN_PLACEHOLDER        ///< Set to true if the element represents a plugin placeholder
#define ATTR_PLUGIN_DEMAND Markup::LOGA_PLUGIN_DEMAND			    ///< Set to true when a placeholder has been demanded to be loaded.
#define ATTR_PUB_ID Markup::LOGA_PUB_ID					///< Contains the public identifier of a DOCTYPE
#define ATTR_SYS_ID Markup::LOGA_SYS_ID					///< Contains the system identifier of a DOCTYPE
#define ATTR_DSE_RESTORE_PARENT Markup::LOGA_DSE_RESTORE_PARENT		///< Stores the original parent when we need to recover with DSE
#define ATTR_SOURCE_POSITION Markup::LOGA_SOURCE_POSITION           ///< Source position (line and position in line) where the text content for script and style elements start
#define ATTR_VIEWPORT_META Markup::LOGA_VIEWPORT_META		 ///< Used to store a CSS_ViewportMeta object representing the parsed content attribute of a viewport meta element.

/* Constants used for the ATTR_JS_SCRIPT_HANDLED attribute. */
enum
{
	SCRIPT_HANDLED_LOADED   = 1,
	SCRIPT_HANDLED_EXECUTED = 2,
	SCRIPT_HANDLED_CROSS_ORIGIN = 4
};

// This must be synchronized with the ATTR_VALUE_ table in attr_val.h
enum InputType
{
	INPUT_NOT_AN_INPUT_OBJECT = 0,
	INPUT_TEXT = 1,
    INPUT_CHECKBOX = 2,
    INPUT_RADIO = 3,
    INPUT_SUBMIT = 4,
    INPUT_RESET = 5,
    INPUT_HIDDEN = 6,
    INPUT_IMAGE = 7,
    INPUT_PASSWORD = 8,
	INPUT_BUTTON = 9,

    INPUT_FILE = 11,
	INPUT_URI = 12,
	INPUT_DATE = 13,
	INPUT_WEEK = 14,
	INPUT_TIME = 15,
	INPUT_EMAIL = 16,
	INPUT_NUMBER = 17,
	INPUT_RANGE = 18,
	INPUT_MONTH = 19,
	INPUT_DATETIME = 20,
	INPUT_DATETIME_LOCAL = 21,
	INPUT_SEARCH = 22,
	INPUT_TEL = 23,
	INPUT_COLOR = 24,
	INPUT_TEXTAREA = 25
};

enum PseudoElementType
{
	ELM_NOT_PSEUDO,
	ELM_PSEUDO_BEFORE,
	ELM_PSEUDO_AFTER,
	ELM_PSEUDO_FIRST_LETTER,
	ELM_PSEUDO_FIRST_LINE,
	ELM_PSEUDO_SELECTION,
	ELM_PSEUDO_LIST_MARKER
};

#ifdef DNS_PREFETCHING
enum DNSPrefetchType
{
	DNS_PREFETCH_PARSING, ///< Link found during parsing.
	DNS_PREFETCH_DYNAMIC, ///< Link found when inserted into the document tree.
	DNS_PREFETCH_MOUSEOVER ///< When hovering the link.
};
#endif // DNS_PREFETCHING

#endif // LOGDOCENUM_H
