/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** OpClipboard.h -- platform-specific implementation of clipboard.
*/

#ifndef OPCLIPBOARD_H
#define OPCLIPBOARD_H

/** @short Clipboard class
 *
 * This class provides means of copying and pasting text between Opera
 * and other applications, and also internally in Opera.
 */
class OpClipboard
{
public:
	/** Create an OpClipboard object */
	static OP_STATUS Create(OpClipboard** new_opclipboard);

	virtual ~OpClipboard() {};

	/** Return TRUE if there is text on the clipboard */
	virtual BOOL HasText() = 0;

	/** Place text on the clipboard.
	 *
	 * @param text a NUL-terminated string to place on the clipboard
 	 * @param token used to identify the content so that 
	 * it could be used later to empty the clipboard content associated with it.
	 * @return OpStatus::OK on success, error otherwise.
	 */
	virtual OP_STATUS PlaceText(const uni_char* text, UINT32 token = 0) = 0;

	/** Copy the text currently on the clipboard to the string.
	 *
	 * @param text (output) Set to the current contents of the
	 * clipboard
	 */
	virtual OP_STATUS GetText(OpString &text) = 0;

#ifdef CLIPBOARD_HTML_SUPPORT
	/** Is the current text on the clipboard HTML formatted?
	 *
	 * @return TRUE if there is HTML text on the clipboard. See
	 * FEATURE_CLIPBOARD_HTML in features.txt for more info about how
	 * this is ment to work.
	 */
	virtual BOOL HasTextHTML() = 0;

	/** Place HTML text on the clipboard.
	 *
	 * @param htmltext a NUL-terminated string of HTML to place on the
	 * clipboard
	 * @param text a NUL-terminated plaintext version of htmltext to
	 * place on the clipboard
	 * @param token used to identify the content so that 
	 * it could be used later to empty the clipboard content associated with it.
	 */
	virtual OP_STATUS PlaceTextHTML(const uni_char* htmltext, const uni_char* text, UINT32 token = 0) = 0;

	/** Copy the text currently on the clipboard to the string.
	 *
	 * @param text (output) Set to the current HTML contents of the
	 * clipboard
	 */
	virtual OP_STATUS GetTextHTML(OpString &text) = 0;
#endif // CLIPBOARD_HTML_SUPPORT

	/** Place a bitmap on the clipboard.
	 *
	 * @param bitmap a bitmap to place on the clipboard
	 * @param token used to identify the content so that
	 * it could be used later to empty the clipboard content associated with it.
	 */
	virtual OP_STATUS PlaceBitmap(const class OpBitmap* bitmap, UINT32 token = 0) { return OpStatus::ERR;};

	/** Empty the clipboard if the current content is associated with the token.
	 *
	 * When clipboard has a different token don't clear
	 * the clipboard but return OpStatus::OK.
	 *
	 * @param token of clipboard content which should be cleared
	 */
	virtual OP_STATUS EmptyClipboard(UINT32 token) = 0;

#ifdef _X11_SELECTION_POLICY_
	/** Enable or disable mouse selection mode (set active clipboard).
	 *
	 * Some systems (e.g. X11 environments) sort of use two different
	 * clipboards; one used for current mouse selection (which
	 * automatically puts text on the clipboard when text is selected
	 * with the mouse), and another for other/regular selection
	 * mechanisms (the user selects text, and manually copies it to
	 * the clipboard by pressing Ctrl+C, selecting "Copy" from the
	 * menu, etc.).
	 *
	 * This method is used to specify which clipboard is to be the
	 * active one, i.e. which of the clipboards to manipulate when
	 * calling PlaceText(), GetText(), etc.
	 *
	 * Initially, mouse selection mode is disabled.
	 *
	 * @param mode If TRUE, select mouse selection clipboard. If FALSE
	 */
	virtual void SetMouseSelectionMode(BOOL mode) = 0;

	/** Is mouse selection enabled?
	 *
	 * @return The active clipboard, i.e. what has been set by
	 * SetMouseSelectionMode(). Return TRUE if mouse selection is
	 * enabled. Initially, mouse selection mode is disabled.
	 */
	virtual BOOL GetMouseSelectionMode() = 0;
#endif // _X11_SELECTION_POLICY_
};

#endif // !OPCLIPBOARD_H
