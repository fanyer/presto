/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 *
 * Notes: Much of the code here is based on Chapter 11 of the Xlib Programming
 * Manual (see http://www.sbin.org/doc/Xlib/chapt_11.html). This also contains
 * reasonable definitions of what the callbacks should do when they are called.
 *
 * We only support IMEs that have full callbacks. We have not encountered any
 * modern IMEs that do not support this interaction style.
 *
 * Logging can be enabled by starting Opera with the '-debugime' argument.
 *
 */

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_inputcontext.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_inputmethod.h"
#include "modules/pi/OpInputMethod.h"
#include "modules/unicode/unicode_segmenter.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/quix/commandline/StartupSettings.h"


X11InputContext::X11InputContext()
	: m_xic(0)
	, m_im(0)
	, m_listener(0)
	, m_string(0)
	, m_focus_window(0)
	, m_logger(g_startup_settings.debug_ime ?
			   OP_NEW(PosixLogToStdIO, (PosixLogger::NORMAL, true)) :
			   0)
{
}


X11InputContext::~X11InputContext()
{
	/* If we're closing the window, the focus proxy may already be
	 * deleted.
	 */
	m_focus_window = 0;
	if (m_im)
		m_im->InputContextIsDeleted(this);
	if (m_xic)
	{
		XIC xic = m_xic;
		m_xic = 0;
		XDestroyIC(xic);
	};
	// XDestroyIC should have stopped composing if necessary
	OP_ASSERT(m_string == 0);
	if (m_listener && m_string)
		m_listener->OnStopComposing(TRUE);
	OP_DELETE(m_string);
}


OP_STATUS X11InputContext::Init(X11InputMethod * im, X11Widget * window)
{
	m_im = im;
	if (window == 0)
		window = m_focus_window;
	OP_ASSERT(window != 0);
	XIM xim = m_im->GetXIM();
	XIMStyle style = m_im->GetXIMStyle();
	CallbackList preedit_list;
	RETURN_IF_ERROR(CreatePreeditList(preedit_list));

	CallbackList status_list;
	RETURN_IF_ERROR(CreateStatusList(status_list));

	X11Types::Window winhandle = window->GetWindowHandle();
	m_xic = XCreateIC(xim,
						XNInputStyle, style,
						XNClientWindow, winhandle,
						XNFocusWindow, winhandle,
						XNPreeditAttributes, preedit_list.list,
						XNStatusAttributes, status_list.list,
						NULL);

	if (!m_xic)
	{
		fprintf(stderr, "opera: XCreateIC failed\n");
		return OpStatus::ERR;
	}

	if (XSetICValues(m_xic, XNResetState, XIMPreserveState, NULL) != 0)
		m_logger.Log(PosixLogger::NORMAL, "XIC %d: Failed to set XNResetState\n", m_xic);

	m_focus_window = window;

	// Listen for events for this IM
	long im_event_mask;
	XGetICValues(m_xic, XNFilterEvents, &im_event_mask, NULL);
	XWindowAttributes attr;
	XGetWindowAttributes(XDisplayOfIM(xim), winhandle, &attr);
	if ((attr.your_event_mask | im_event_mask) != attr.your_event_mask)
		XSelectInput(XDisplayOfIM(xim), winhandle, attr.your_event_mask | im_event_mask);

	return OpStatus::OK;
}


OP_STATUS X11InputContext::CreatePreeditList(CallbackList& list)
{
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &PreeditStartCallback));
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &PreeditDoneCallback));
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &PreeditDrawCallback));
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &PreeditCaretCallback));

	list.list = XVaCreateNestedList(0,
									XNPreeditStartCallback, list.callbacks.Get(0),
									XNPreeditDoneCallback, list.callbacks.Get(1),
									XNPreeditDrawCallback, list.callbacks.Get(2),
									XNPreeditCaretCallback, list.callbacks.Get(3),
								    NULL);

	return list.list ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


OP_STATUS X11InputContext::CreateStatusList(CallbackList& list)
{
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &StatusStartCallback));
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &StatusDoneCallback));
	RETURN_IF_ERROR(AddToCallbacks(list, (XIMProc) &StatusDrawCallback));

	list.list = XVaCreateNestedList(0,
									XNStatusStartCallback, list.callbacks.Get(0),
									XNStatusDoneCallback, list.callbacks.Get(1),
									XNStatusDrawCallback, list.callbacks.Get(2),
								    NULL);

	return list.list ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


OP_STATUS X11InputContext::AddToCallbacks(CallbackList& list, XIMProc callback_func)
{
	OpAutoPtr<XIMCallback> callback (OP_NEW(XIMCallback, ()));
	if (!callback.get())
		return OpStatus::ERR_NO_MEMORY;

	callback->client_data = (XPointer) this;
	callback->callback = callback_func;

	RETURN_IF_ERROR(list.callbacks.Add(callback.get()));
	callback.release();

	return OpStatus::OK;
}


void X11InputContext::Activate(OpInputMethodListener* listener)
{
	if (m_listener == listener)
		return;

#ifdef DEBUG_ENABLE_OPASSERT
	if (m_xic != 0)
	{
		X11Types::Window current_x_focus_window;
		int current_x_revert_to = 0;
		XIM xim = XIMOfIC(m_xic);
		XGetInputFocus(XDisplayOfIM(xim), &current_x_focus_window, &current_x_revert_to);
		/* This assert is not entirely accurate.  m_focus_window must
		 * be the same window that key events will be sent to.  In
		 * other words, the topmost window underneath the pointer
		 * which is a child of the current focus window.  But as long
		 * as we use a focus proxy, where focus is set to a window
		 * without children, all key events will go to the focus
		 * proxy.
		 */
		OP_ASSERT(current_x_focus_window == m_focus_window->GetWindowHandle());
	};
#endif

	m_listener = listener;
	/* Arjan says:
	 *
	 * When we didn't do the XSetICValues(XNFocusWindow) calls, we
	 * would very quickly end up in a state where the IME would eat
	 * all input events without doing anything. The details were
	 * different from IME to IME (and even it seemed from installation
	 * to installation - we once got it working on my setup with SCIM
	 * but not on Espen's setup with SCIM, which seemed identical in X
	 * version, SCIM version etc).
	 */
	/* On the other hand, unless we are sure that SetFocusWindow() is
	 * always called when we switch focus between X windows, it
	 * probably makes sense to make this call here.
	 */
	if (m_xic == 0)
		return;
	XSetICValues(m_xic, XNFocusWindow, m_focus_window->GetWindowHandle(), NULL);
	XSetICFocus(m_xic);
	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Input method listener registered\n", m_xic);
}


void X11InputContext::Deactivate(OpInputMethodListener* listener)
{
	if (m_listener != listener)
		return;

	m_listener = 0;
	if (m_xic == 0)
		return;
	XUnsetICFocus(m_xic);
	/* Arjan says:
	 *
	 * When we didn't do the XSetICValues(XNFocusWindow) calls, we
	 * would very quickly end up in a state where the IME would eat
	 * all input events without doing anything. The details were
	 * different from IME to IME (and even it seemed from installation
	 * to installation - we once got it working on my setup with SCIM
	 * but not on Espen's setup with SCIM, which seemed identical in X
	 * version, SCIM version etc).
	 */
	/* Apart from the comment above, this call to
	 * XSetICValues(XNFocusWindow) seems wrong to me...
	 */
	XSetICValues(m_xic, XNFocusWindow, 0, NULL);
	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Input method listener deregistered\n", m_xic);
}


void X11InputContext::Abort()
{
	if (m_xic && m_string && m_string->Get() && *m_string->Get())
	{
		char* string = XmbResetIC(m_xic);
		if (string)
			XFree(string);
		m_logger.Log(PosixLogger::NORMAL, "XIC %d: Aborted context\n", m_xic);
	};


	if (m_listener && m_string)
	{

		m_listener->OnStopComposing(TRUE);
		ResetTextArea();
		m_logger.Log(PosixLogger::NORMAL, "XIC %d: Aborted, IME reset\n", m_xic);
	}

	OP_DELETE(m_string);
	m_string = 0;
}


void X11InputContext::SetFocusWindow(X11Widget * window)
{
	OP_ASSERT(window != 0);
	m_focus_window = window;
	if (m_xic)
		XSetICValues(m_xic, XNFocusWindow, window->GetWindowHandle(), NULL);
}

void X11InputContext::SetFocusIfActive()
{
	if (m_listener && m_xic)
		XSetICFocus(m_xic);
};


void X11InputContext::SetFocus()
{
	if (m_xic)
		XSetICFocus(m_xic);
}

void X11InputContext::UnsetFocus()
{
	if (m_xic)
		XUnsetICFocus(m_xic);
};


bool X11InputContext::GetKeyPress(XEvent* event, KeySym& keysym, OpString& text)
{
	if (!m_keybuf.Reserve(100))
		return false;

	int length;

	if (!m_xic)
	{
		/* Input context is dead, stop all composing and let the
		 * keypress be handled normally.
		 */
		if (IsComposing())
		{
			m_listener->OnCommitResult();
			Abort();
		};
		return false;
	};

	while(1)
	{
		X11Types::Status status;
		length = Xutf8LookupString(m_xic, (XKeyEvent*)event, m_keybuf.CStr(), m_keybuf.Capacity(), &keysym, &status);
		if (status == XBufferOverflow)
		{
			if(!m_keybuf.Reserve(length+1))
				return false;
		}
		else
		{
			if (m_keybuf.Capacity() > length)
				m_keybuf.CStr()[length] = '\0';

			text.SetFromUTF8(m_keybuf, length);
			break;
		}
	}

	if (IsComposing())
	{
		if (keysym == 0)
		{
			// This should be interpreted as committing the IM string
			const int utf16_length = text.Length();
			if (utf16_length > 0)
			{
				m_string->Set(text.CStr(), utf16_length);
				m_string->SetCaretPos(0);
				m_string->SetCandidate(0, 0);
				m_listener->OnCommitResult();
			};
			if (m_string)
			{
				/* Crash reports from CAT (see DSK-341683) indicate
				 * that OnCommitResult() may somehow cause m_string to
				 * be set to NULL.
				 */
				m_string->Set(0, 0);
			}

			m_logger.Log(PosixLogger::NORMAL, "XIC %d: Committed string (UTF-8) with length %d (converted to UTF-16 string with length %d): %s\n",
						m_xic, length, utf16_length, m_keybuf.CStr() ? m_keybuf.CStr() : "[Empty string]");
			keysym = NoSymbol;
			text.Empty();
			return true;
		}
		else if ((keysym == XK_Shift_L || keysym == XK_Shift_R) &&
				 m_string->Get() && *m_string->Get())
		{
			/* TEMPORARY UGLY BAND-AID HACK:
			 *
			 * Block the shift key during composition if there is a
			 * pre-edit string.  Hopefully this will make it possible
			 * to type Hangul (Korean) without breaking ATOK.
			 *
			 * An unfortunate side effect is that we will now pass the
			 * key release of the shift key to core without having
			 * sent a key press first...
			 */
			m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received keypress of shift key in compose mode.  Blocking key event.\n", m_xic);
			return true;
		}
		else
		{
			// Action will fall through, need to stop composing
			m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received keypress in compose mode, commit and abort compose mode\n", m_xic);
			/* If we are going to abort composing because we get a key
			 * event while composing, it is probably better to commit
			 * the current pre-edit string than to discard it.  But
			 * this obviously depends a lot on the expectations of the
			 * input method engine itself.
			 *
			 * An alternative solution would be to clear the pre-edit
			 * and then call StopComposing().  But that could
			 * potentially leave our display and the input method out
			 * of sync until the next pre-edit callback from the input
			 * method.
			 */
			m_listener->OnCommitResult();
			Abort();
		}
	}

	return true;
}

void X11InputContext::InputMethodIsDeleted()
{
	m_im = 0;
};

void X11InputContext::XIMHasBeenDestroyed()
{
	m_xic = 0;
};

void X11InputContext::PreeditStart()
{
	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received PreeditStart, starting compose mode\n", m_xic);
	StartComposing();
}


void X11InputContext::PreeditDone()
{
	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received PreeditDone, stopping compose mode\n", m_xic);
	StopComposing();
}


bool X11InputContext::StartComposing()
{
	if (!m_listener)
		return false;

	if (m_string)
		return true;

	m_string = OP_NEW(OpInputMethodString, ());
	if (!m_string)
		return false;

	IM_WIDGETINFO wi = m_listener->OnStartComposing(m_string);
	SetTextArea(wi);

	return true;
}


void X11InputContext::StopComposing(bool cancel)
{
	ResetTextArea();
	if (IsComposing())
		m_listener->OnStopComposing(cancel ? TRUE : FALSE);

	OP_DELETE(m_string);
	m_string = 0;
}


void X11InputContext::PreeditDraw(XIMPreeditDrawCallbackStruct* data)
{
	if (!StartComposing())
		return;

	if (data->chg_length > 0)
		DeleteFromPreedit(data->chg_first, data->chg_length);

	if (data->text)
	{
		if (data->text->string.wide_char)
		{
			if (data->text->encoding_is_wchar)
				InsertIntoPreedit(data->chg_first, data->text->string.wide_char, data->text->length);
			else
				InsertIntoPreedit(data->chg_first, data->text->string.multi_byte, data->text->length);
		}

		if (data->text->feedback)
			DeterminePreeditUnderline(data->text->feedback, data->text->length);
	}

	int caret_pos = MIN(OpStringC(m_string->Get()).Length(), data->caret);
	m_string->SetCaretPos(caret_pos);
	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Setting caret position to %d\n", m_xic, caret_pos);

	if (!m_string->Get() || !*m_string->Get())
	{
		// Core dislikes OnCompose() with empty pre-edit strings
		m_logger.Log(PosixLogger::NORMAL, "XIC %d: Update with empty pre-edit string\n", m_xic);
		StopComposing(true);
		return;
	};
	IM_WIDGETINFO wi = m_listener->OnCompose();
	SetTextArea(wi);
}


void X11InputContext::DeleteFromPreedit(int pos, int length)
{
	if (OpStringC(m_string->Get()).Length() < pos + length)
		return;

	OpString string_copy;
	string_copy.Set(m_string->Get());
	string_copy.Delete(pos, length);
	m_string->Set(string_copy.CStr(), string_copy.Length());

	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Deleting %d characters at pos %d\n", m_xic, length, pos);
}


void X11InputContext::InsertIntoPreedit(int pos, const uni_char* string, int length)
{
	OpString string_copy;
	string_copy.Set(m_string->Get());
	string_copy.Insert(pos, string, length);
	m_string->Set(string_copy.CStr(), string_copy.Length());

	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Inserting string of length %d (wide) at pos %d\n", m_xic, length, pos);
}


void X11InputContext::InsertIntoPreedit(int pos, const char* string, int length)
{
	OpString string_copy;
	string_copy.Set(m_string->Get());

	/* According to the documentation, 'string' is not guaranteed to
	 * be zero-terminated.  However, both Qt and Gtk assumes it is, so
	 * it is probably safe for us to do the same.
	 */
	OpString operastring;
	OP_STATUS err = PosixNativeUtil::FromNative(string, &operastring);
	if (OpStatus::IsSuccess(err))
		string_copy.Insert(pos, operastring, length);
	else
		m_logger.Log(PosixLogger::TERSE, "XIC %d: Failed to convert multibyte pre-edit string\n", m_xic);

	m_string->Set(string_copy.CStr(), string_copy.Length());

	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Inserting string %s (multibyte) at pos %d\n", m_xic, string, pos);
}


void X11InputContext::DeterminePreeditUnderline(XIMFeedback* feedback, unsigned length)
{
	for (unsigned startpos = 0; startpos < length; startpos++)
	{
		unsigned endpos = startpos;
		while (endpos < length && (feedback[endpos] & XIMReverse))
			endpos++;

		if (endpos > startpos)
		{
			m_string->SetCandidate(startpos, endpos - startpos);
			break;
		}
	}

	if (g_startup_settings.debug_ime)
	{
		OpString8 feedback_string;
		for (unsigned i = 0; i < length; i++)
			feedback_string.AppendFormat("%d", feedback[i]);

		if (feedback_string.CStr())
			m_logger.Log(PosixLogger::NORMAL, "XIC %d: Feedback on compose string is %s\n", m_xic, feedback_string.CStr());
	}
}


void X11InputContext::PreeditCaret(XIMPreeditCaretCallbackStruct* data)
{
	if (!StartComposing())
		return;

	switch (data->direction)
	{
		case XIMForwardChar:
			m_string->SetCaretPos(MIN(m_string->GetCaretPos() + 1, OpStringC(m_string->Get()).Length()));
			break;

		case XIMBackwardChar:
			m_string->SetCaretPos(MAX(m_string->GetCaretPos() - 1, 0));
			break;

		case XIMForwardWord:
		{
			const uni_char* string = m_string->Get() + m_string->GetCaretPos();
			UnicodeSegmenter segmenter(UnicodeSegmenter::Word);
			int newpos = segmenter.FindBoundary(string, OpStringC(string).Length());
			m_string->SetCaretPos(m_string->GetCaretPos() + newpos);
			break;
		}

		case XIMBackwardWord:
		{
			const uni_char* string = m_string->Get();
			int length = OpStringC(string).Length();
			UnicodeSegmenter segmenter(UnicodeSegmenter::Word);
			int newpos = 0;
			int suggestion = segmenter.FindBoundary(string, length);
			while (suggestion < m_string->GetCaretPos())
			{
				newpos = suggestion;
				string += suggestion;
				length -= suggestion;
				suggestion = segmenter.FindBoundary(string, length);
			}
			m_string->SetCaretPos(newpos);
			break;
		}

		case XIMCaretUp: /* fallthrough */
		case XIMPreviousLine:
		{
			const uni_char* string = m_string->Get() + m_string->GetCaretPos() - 1;
			bool found_line = false;
			while (string > m_string->Get())
			{
				if (*string == '\n')
				{
					if (found_line)
					{
						string++;
						break;
					}
					else
						found_line = true;
				}
				string--;
			}
			m_string->SetCaretPos(string - m_string->Get());
			break;
		}

		case XIMCaretDown: /* fallthrough */
		case XIMNextLine:
		{
			const uni_char* string = m_string->Get() + m_string->GetCaretPos();
			while (*string && *string != '\n')
				string++;
			if (*string == '\n')
				string++;
			m_string->SetCaretPos(string - m_string->Get());
			break;
		}

		case XIMLineStart:
		{
			const uni_char* string = m_string->Get() + m_string->GetCaretPos();
			if (string == m_string->Get())
				break;
			while (string > m_string->Get() && *string != '\n')
				string--;
			if (*string == '\n')
				string++;
			m_string->SetCaretPos(string - m_string->Get());
			break;
		}

		case XIMLineEnd:
		{
			const uni_char* string = m_string->Get() + m_string->GetCaretPos();
			while (*string && *string != '\n')
				string++;
			m_string->SetCaretPos(string - m_string->Get());
			break;
		}

		case XIMAbsolutePosition:
			m_string->SetCaretPos(MIN(data->position, (int)OpStringC(m_string->Get()).Length()));
			break;

		case XIMDontChange:
			break;
	}

	data->position = m_string->GetCaretPos();

	m_string->SetShowCaret(data->style != XIMIsInvisible);

	if (!m_string->Get() || !*m_string->Get())
	{
		// Core dislikes OnCompose() with empty pre-edit string
		m_logger.Log(PosixLogger::NORMAL, "XIC %d: Update with empty pre-edit string\n", m_xic);
		StopComposing(true);
		return;
	};
	IM_WIDGETINFO wi = m_listener->OnCompose();
	SetTextArea(wi);

	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Move caret in direction %d, style %d, end position %d\n", m_xic, data->direction, data->style, data->position);
}


void X11InputContext::StatusStart()
{
	if (!StartComposing())
		return;

	m_listener->OnCandidateShow(TRUE);

	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received StatusStart\n", m_xic);
}


void X11InputContext::StatusDone()
{
	if (!StartComposing())
		return;

	m_listener->OnCandidateShow(FALSE);

	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received StatusDone\n", m_xic);
}


void X11InputContext::StatusDraw(XIMStatusDrawCallbackStruct* data)
{
	// We currently don't show status text
	m_logger.Log(PosixLogger::NORMAL, "XIC %d: Received StatusDraw\n", m_xic);
}


void X11InputContext::SetTextArea(const IM_WIDGETINFO & wi)
{
	if (!m_focus_window)
		return;
	int x = wi.rect.x;
	int y = wi.rect.y;
	int width = wi.rect.width;
	int height = wi.rect.height;
	if (m_focus_window->Parent())
		m_focus_window->Parent()->PointFromGlobal(&x, &y);
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;
	m_focus_window->SetGeometry(x, y, width, height);
};

void X11InputContext::ResetTextArea()
{
	if (m_focus_window)
		m_focus_window->SetGeometry(-1, -1, 1, 1);
};
