/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef _DOCUMENT_TYPES_
#define _DOCUMENT_TYPES_

#include "modules/logdoc/logdocenum.h" // for ATTR_FIRST_ALLOWED
#include "modules/debug/debug.h"

typedef OP_STATUS OP_DOC_STATUS;

/**
 * Return values used by some doc and dochand methods.
 */
class DocStatus : public OpStatus
{
public:
    enum
    {
		/**
		 * Everything is alright.
		 */
        DOC_OK = USER_SUCCESS + 0,

		/**
		 * The document was painted.
		 */
        DOC_DISPLAYED = USER_SUCCESS + 1,
		//        DOC_DISPLAYING = USER_SUCCESS + 2,

		/**
		 * The document failed to paint.
		 */
        DOC_CANNOT_DISPLAY = USER_ERROR + 0,

        // DOC_PRINTING = USER_SUCCESS + 3,

		/**
		 * The page was printed.
		 */
        DOC_PRINTED = USER_SUCCESS + 4,

		/**
		 * The page failed to paint.
		 */
        DOC_CANNOT_PRINT = USER_ERROR + 1,

		/**
		 * There was no such page in the document.
		 */
        DOC_PAGE_OUT_OF_RANGE = USER_ERROR + 2,
        // DOC_PRINTER_ERROR = USER_ERROR + 3,
        // DOC_OUT_OF_PAPER = USER_ERROR + 4,
        // DOC_PAPER_LOW = USER_ERROR + 5,

		/**
		 * Frames have been setup successfully.
		 */
        DOC_FORMATTED = USER_SUCCESS + 5,

		/**
		 * Also a success code, but I'm not really sure how it's used.
		 */
        DOC_FORMATTING = USER_SUCCESS + 6,

		/**
		 * General failure in the frame buildup.
		 */
        DOC_CANNOT_FORMAT = USER_ERROR + 6,

		/**
		 * To be removed. Never set.
		 */
        DOC_FORMATTING_NO_DATA = USER_ERROR + 7,

        // DOC_NEED_IMAGE_RELOAD = USER_SUCCESS + 7,

		// DOC_FORMATTING_FINISHED_NONCONFLICTED = USER_SUCCESS + 8	// conflicts with core/constant.h

    };
};

/**
 * See Document::Type() though this is going away. You don't
 * need an enum to figure out if you have a FramesDocument
* or a HTML_Document.
 */
enum DocType
{
	DOC_PLUGIN, // Unused but referenced in docman.cpp (to be removed)
	DOC_HTML, // HTML_Document
	DOC_FRAMES // FramesDocument.
};

enum DocAction
{
	DOC_ACTION_INVALID,
	DOC_ACTION_NONE,
	DOC_ACTION_HANDLED,
	DOC_ACTION_FORM_SUBMIT,
	DOC_ACTION_FORM_RESET,
	DOC_ACTION_OPEN_ELM,
	DOC_ACTION_CLOSE_ELM,
	DOC_ACTION_DRAG_SEP,
	DOC_ACTION_TOGGLE,
	DOC_ACTION_BUTTON,
	DOC_ACTION_YIELD
};

enum HLDStatus
{
    HLD_UNLOADED = 0,
    HLD_LOADING,
    HLD_LOADED,
    HLD_FORMATTED,
    HLD_WAITING_FOR_INLINE_URL,
    HLD_WAITING_FOR_LOADING,
    HLD_FORMATTING_SUSPENDED,
    HLD_WAITING_FOR_CSS,
    HLD_WAITING_FOR_GENERIC_INLINE,
    HLD_WAITING_FOR_IFRAME_SIZE
};

/**
 * Different ways to scroll in ScrollToElement / ScrollToRect in
 * FramesDocument and HTML_Document.
 */
enum SCROLL_ALIGN
{
	// These constants exist in FramesDocument as well. Don't reorder them, unless
	// FramesDocument and HTML_Document are also fixed.
	SCROLL_ALIGN_CENTER,	///< Places the rect in the center (If rect was not already visible).

#if defined DOCUMENT_EDIT_SUPPORT || defined KEYBOARD_SELECTION_SUPPORT
	SCROLL_ALIGN_NEAREST,	///< Scrolls as little as possible (If rect was not already visible).
#endif // DOCUMENT_EDIT_SUPPORT || KEYBOARD_SELECTION_SUPPORT

	SCROLL_ALIGN_LAZY_TOP,	///< Don't scroll if the rectangle is at least partially visible.  Otherwise scroll to top and beginning (left for LTR, right for RTL).
	SCROLL_ALIGN_TOP,		///< Places the rect at the top.
	SCROLL_ALIGN_BOTTOM,		///< Places the rect at the bottom.
	SCROLL_ALIGN_SPOTLIGHT	///< Places the rect at the top but offset a few pixels to show the outline
};

#ifdef _DEBUG
inline Debug& operator<<(Debug& d, enum SCROLL_ALIGN align) {
	switch (align) {
	case SCROLL_ALIGN_CENTER:    return d << UNI_L("SCROLL_ALIGN_CENTER");
#ifdef DOCUMENT_EDIT_SUPPORT
	case SCROLL_ALIGN_NEAREST:   return d << UNI_L("SCROLL_ALIGN_NEAREST");
#endif
	case SCROLL_ALIGN_LAZY_TOP:  return d << UNI_L("SCROLL_ALIGN_LAZY_TOP");
	case SCROLL_ALIGN_TOP:       return d << UNI_L("SCROLL_ALIGN_TOP");
	case SCROLL_ALIGN_BOTTOM:    return d << UNI_L("SCROLL_ALIGN_BOTTOM");
	case SCROLL_ALIGN_SPOTLIGHT: return d << UNI_L("SCROLL_ALIGN_SPOTLIGHT");
	default:
		return d.AddDbg(UNI_L("SCROLL_ALLIGN(unknown:%d)"), static_cast<int>(align));
	}
};
#endif // _DEBUG

enum HtmlDocType
{
    HTML_DOC_UNKNOWN = 0,
    HTML_DOC_PLAIN,
    HTML_DOC_FRAMES
};

enum TextSelectionType
{
	TEXT_SELECTION_CHARACTERS,
	TEXT_SELECTION_WORD,
	TEXT_SELECTION_SENTENCE,
	TEXT_SELECTION_PARAGRAPH
};


#endif // _DOCUMENT_TYPES_

