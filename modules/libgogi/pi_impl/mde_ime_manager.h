/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef MDE_IME_MANAGER_H
#define MDE_IME_MANAGER_H

class MDE_OpView;
class OpRect;

#ifdef WIDGETS_IME_SUPPORT

/**
 * Currently the mde ime manager do not provide any implementation
 */
class MDE_IMEManager
{
public:
	/** Flags specifying the style of font used in input method. The
	 * values can be combined with bitwise OR. */
	typedef enum
	{
		ITALIC    = 0x01, ///< Set if the input widgets font is in italics.
		BOLD      = 0x02, ///< Set if the input widgets font is in bold.
		MONOSPACE = 0x04  ///< Set if the input widgets font is monospaced.
	} FontStyle;

	typedef struct
	{
		int height;
		int style;  ///< A bitwise OR:ed combination of FontStyle flags.
	} FontInfo;

	/** Specifies the context to which the input method belongs. */
	typedef enum
	{
		CONTEXT_DEFAULT,	///< No specific context, the default context.
		CONTEXT_FORM,		///< The IME belongs to a submittable form.
		CONTEXT_SEARCH		///< The IME belongs to a submittable form that's used for search.
	} Context;

	static OP_STATUS Create(MDE_IMEManager** new_mde_ime_manager);

	MDE_IMEManager();
    virtual ~MDE_IMEManager();
    
	/**
	 * Create an IME
	 *
	 * @param opw The view for the ime
	 * @param rect The rectangle of the widget the ime is operating on
	 *   relative to the upper left corner of the screen.
	 * @param text The initial text
	 * @param multiline TRUE if more than one line can be used
	 * @param passwd Hint that this IME operates on a password
	 * @param maxLength Maximum string length
	 * @param context Context to which the IME belongs.
	 * @param caretOffset number of characters into the string to put caret at
	 * @param fontinfo describes the font style for this IME
	 * @param format The im style which is a string given by the webpage
	 *   from the 'inputmode' or 'ISTYLE' attribute.
	 */
	virtual void *CreateIME(MDE_OpView *opw, const OpRect &rect,
                            const uni_char *text, BOOL multiline,
                            BOOL passwd, int maxLength, Context context, int caretOffset,
                            const FontInfo* fontinfo = NULL,
                            const uni_char* format = 0);

	/**
	 * Notify ui about changed position of the IMEs widget
	 *
	 * @param ime The ime id to move
	 * @param rect The rectangle of the widget the ime is operating on
	 *   relative to the upper left corner of the screen.
	 */
	virtual void MoveIME(void *ime, const OpRect &rect);

	/**
	 * update the text contents of the IME
	 *
	 * @param ime The ime id for which to update the text contents
	 * @param text the new text contents of the ime
	 */
	virtual void UpdateIMEText(void *ime, const uni_char *text);

	/**
	 * Bring down an IME
	 *
	 * @param ime The ime id to destroy
	 */
	virtual void DestroyIME(void *ime);

	/**
	 * Should be called when text input has been finished
	 *
	 * @param ime The ime id to commit
	 * @param text The text to commit
	 */
	virtual void CommitIME(void *ime, const uni_char *text);

	/**
	 * Change the current text of the IME
	 *
	 * @param ime The ime id to update
	 * @param text The new text
	 * @param cursorpos Position of cursor in new text [0-strlen]
	 * @param highlight_start start of highlighted part of text
	 * @param highlight_len length of highlighted part of text
	 */
	virtual void UpdateIME(void *ime, const uni_char *text, int cursorpos, int highlight_start, int highlight_len);

	/**
	 * Change the current text of the IME while keeping last known caret
	 * position, possibly capping it to the length of the new text.
	 *
	 * @param ime The ime id to update
	 * @param text The new text
	 */
	virtual void UpdateIME(void *ime, const uni_char *text);
};

#endif // WIDGETS_IME_SUPPORT
#endif // MDE_IME_MANAGER_H
