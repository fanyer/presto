/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPINPUTMETHODS_H
#define OPINPUTMETHODS_H

#ifdef WIDGETS_IME_SUPPORT

class OpFont;
class OpInputMethodString;

/** Flags specifying the style of font used in a widget. The values
 *  can be combined with bitwise OR. */
typedef enum
{
	WIDGET_FONT_STYLE_ITALIC    = 0x01, ///< Set if the font used is italic
	WIDGET_FONT_STYLE_BOLD      = 0x02, ///< Set if the font used is bold
	WIDGET_FONT_STYLE_MONOSPACE = 0x04  ///< Set if the font used is monospace
} WIDGET_FONT_STYLE;

struct IM_WIDGETINFO {
	OpRect rect;	///< Rectangle of the caret - relative to the upper left corner of the screen.
	OpRect widget_rect;///< Rectangle of the widget - relative to the upper left corner of the screen.
	OpFont* font;	///< The font that the widget uses (with exception for eventual fontswitching).
	BOOL has_font_info; ///< TRUE if information about the font is supplied through fields below, which can be the case if 'font' is NULL.
	INT16 font_height; ///< Height of the font
	int font_style; ///< A bitwise OR:ed combination of WIDGET_FONT_STYLE flags
	BOOL is_multiline;///< TRUE if it is a multiline textfield
	IM_WIDGETINFO() : has_font_info(FALSE), font_height(0), font_style(0), is_multiline(FALSE) {}
};

enum IM_COMPOSE {
	IM_COMPOSE_NEW,		///< The composition is a new
	IM_COMPOSE_WORD,
	IM_COMPOSE_ALL
};

/**
	== Input Method Listener ==================

	Some systems supports ways to type Asian language strings in a special "composemode" in which the users
	keystrokes is translated to f.ex. a japanese words/characters.
	Most systems supply a "IME unaware" input methodsystem where the actual editing of the text is done in a popupwindow
	own by the IME and when it is done, it's sent as a serie of keystrokes to the destinationapplication.

	This interface exists to make opera "Full IME-aware" (IM-editing is done inside the target textfield)

	Comments/questions: emil@opera.com

	== How it works ===========================

	When the user starts to type, OnStartComposing should be called. This starts the "composemode" in a textfield.
	The OpInputMethodString should be filled with information about the current composestring.
	It should should be updated and OnCompose should be called when the user changes it or selects a candidate.
	If the user comfirms the current composestring (or a part of it), update the imstring and call OnCommitResult
	and the current text in the imstring will be permanently inserted in the textfield.
	If the user aborts the composemode or is done, call OnStopComposing.

	Note: During the composemode, no keyevents should be passed into the OpViews keyboardlistener.

	We render the composestring and candidates by ourselfs. The entire imstring is drawn underlined so the user
	can see the difference from it and the rest of the text. If there is a candidate, it will render as selected text.
	The hooks OnStartComposing and OnCompose returns back the rectangle where the text is rendered relative to the
	OpView (There is no OpView or other native platformwidget in the formswidgets itself). The implementation can use
	this rect to update the position of the candidatewindow so it appear near the text.

	== Input method stuff in OpView ===========

	There is also 2 functions in OpView which should be implemented.

	void AbortInputMethodComposing();
		Read more in OpView.h

	void SetInputMethodMode(IME_MODE mode);
		Read more in OpView.h

	== Input method Undo =====================

	In windows some IME's let the user to undo previosly written phrases which is already commited to the text.
	It is a bit tricky implemented. The IME itself caches previosly written strings. When the user presses
	ctrl+backspace (which means undo when the IME is active), it sends a emulated backspace for each character in
	the previous string to erase the string from the textfield. Then it starts the composemode as usual,
	with the previous string as composestring.
	To avoid strange effects if the caret is moved to another line or something, it clears that cached list when a
	mousebutton or arrowkey is invoked.


	== The windows implementation ============

	The windowsimplementation overrides theese messages:
	WM_IME_STARTCOMPOSITION		Calls OnStartComposition
	WM_IME_ENDCOMPOSITION		Calls OnStopComposing
	WM_IME_COMPOSITION			Calls OnCommitResult and/or OnCompose depending on the current inputstate.
	WM_IME_CHAR					Does nothing. Must be overridden to prevent the automatical popup-composewindow to showup.
	WM_IME_CONTROL				-||-

	A tutorial for IME programming in windows: http://plaza27.mbn.or.jp/~satomii/design/imm/index.html
*/

class OpInputMethodListener
{
public:
	virtual ~OpInputMethodListener() {}

	/**
	  Notifies that composemode is started.
	  The immstring should be valid until AFTER OnStopEvent.
	  Returns back information which may be needed to setup candidate-windows etc.
	  If compose is IM_COMPOSE_WORD or IM_COMPOSE_ALL, the word or all text will be removed from the textfield
	  and set to imstring.
	*/
	virtual IM_WIDGETINFO OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose = IM_COMPOSE_NEW) = 0;

	/**
	  Should be called when the user changes the composestring or moves the caret in it. The OpInputMethodString
	  sent in OnStartComposing should be updated before this call.
	*/
	virtual IM_WIDGETINFO OnCompose() = 0;

	/**
	  Commits the current content of InputMethodString to the text. After commit, the composing may stop or continue.
	*/
	virtual void OnCommitResult() = 0;

	/**
	  Should be called when the user is ready or aborts composing. The composestring should contain the result.
	  If canceled is FALSE it will be inserted, otherwise not.
	*/
	virtual void OnStopComposing(BOOL cancel) = 0;

	/**
	  Should be called when the canditate selection windows is shown/hidden. This is to allow hiding anything that
	  would appear in front of it
	*/

	virtual void OnCandidateShow(BOOL visible) = 0;

#ifdef IME_RECONVERT_SUPPORT
	/**
	Should be called when the IME request to Re-convert.If Re-conversion is intended,the compose string should be set
	in this function. str to the whole string,sel_start and sel_stop to the start and stop of selection.The whole
	string is needed since the IME may adjust the selection.
	e.g assume str="1234" ,sel_start = 2,sel_stop = 3. IME will still set compose string to "1234" instead of "3".
	The adjusted start and stop of compose string will be sent to app via OnReconvertRange.
	*/
	virtual void OnPrepareReconvert(const uni_char*& str, int& sel_start, int& sel_stop) = 0;

	/**
	Should be called after the IME determined compose string,sel_start and sel_stop are offsets in str.Generally,
	[sel_start,sel_stop) should be set as selection which will be removed in next OnCompse message.
	*/
	virtual void OnReconvertRange(int sel_start, int sel_stop) = 0;
#endif

	/**
	  Returns the widgetinfo of the current focused widget.
	*/
	virtual IM_WIDGETINFO GetWidgetInfo() = 0;
};

/**
  String which carries the composestring and info about the caretposition and eventual candidate position.
  If not in candidatemode, set candidatepos and length to 0.
*/

class OpInputMethodString
{
public:
	OpInputMethodString();
	~OpInputMethodString();

	/** Sets which characterposition the caret should be rendered at */
	void SetCaretPos(INT32 pos);

	/** Sets the current candidate position and length. If there is no candidate, just use length 0 */
	void SetCandidate(INT32 pos, INT32 len);

	/** Sets the current composestring */
	OP_STATUS Set(const uni_char* str, INT32 len);

	void SetShowCaret(BOOL show) { show_caret = show; }
	void SetShowUnderline(BOOL show) { show_underline = show; }
	void SetUnderlineColor(UINT32 col) { underline_color = col; }

	INT32 GetUniPointCaretPos() const;
	INT32 GetCaretPos() const;
	INT32 GetCandidatePos() const;
	INT32 GetCandidateLength() const;
	const uni_char* Get() const;

	BOOL GetShowCaret() const { return show_caret; }
	BOOL GetShowUnderline() const { return show_underline; }
	UINT32 GetUnderlineColor() const { return underline_color; }

private:
	INT32 caret_pos;
	INT32 candidate_pos, candidate_len;
	uni_char* string;

	BOOL show_caret;		///< Show caret or not during composition. Default is TRUE.
	BOOL show_underline;	///< Show underline under the composition string. Default is TRUE.
	UINT32 underline_color;	///< Default is red.
};

#endif // WIDGETS_IME_SUPPORT

#endif // OPINPUTMETHODS_H
