/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_CLIPBOARD_MANAGER_H
#define DESKTOP_CLIPBOARD_MANAGER_H

#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/dragdrop/clipboard_manager.h"


#define g_desktop_clipboard_manager (DesktopClipboardManager::GetInstance())

/**
 * Wrapper class for ClipboardManager functions, to make calls that are
 * guaranteed by Core to be synchronous de-facto synchronous for us
 */
class DesktopClipboardManager
	: public DesktopManager<DesktopClipboardManager>
	, private ClipboardListener
{
public:
	DesktopClipboardManager();
	virtual ~DesktopClipboardManager() { OP_ASSERT(!m_clipboard_text); }

	// Implementing GenericDesktopManager
	virtual OP_STATUS Init() { return OpStatus::OK; }

	// Wrappers for ClipboardManager functions

	/**
	 * Wrapper for ClipboardManager::HasText() with extra parameter to change the active
	 * clipboard on Unix.
	 *
	 * @see ClipboardManager::HasText
	 * @param mouse_selection_on Sets the active clipboard to the mouse-selection-mode one during the call. 
	 *                           @see OpClipboard::SetMouseSelectionMode
	 */
	BOOL            HasText(
#ifdef _X11_SELECTION_POLICY_
		bool mouse_selection_on = false
#endif // _X11_SELECTION_POLICY_
		);

	/**
	 * Synchronous wrapper around using ClipboardManager::Copy()/OnCopy(), so that not
	 * every class using Copy() needs to implement ClipboardListener.
	 * Works as long as Copy() calls are guaranteed by Core to be syncronous.
	 *
	 * @see OpClipboard::PlaceText
	 * @param mouse_selection_on Sets the active clipboard to the mouse-selection-mode one during the call. 
	 *                           @see OpClipboard::SetMouseSelectionMode
	 */
	OP_STATUS       PlaceText(const uni_char* text, UINT32 token = 0
#ifdef _X11_SELECTION_POLICY_
		, bool mouse_selection_on = false
#endif // _X11_SELECTION_POLICY_
		);

	/**
	 * Synchronous wrapper around using ClipboardManager::Paste()/OnPaste(), so that not
	 * every class using Paste() needs to implement ClipboardListener.
	 * Works as long as Paste() calls are guaranteed by Core to be syncronous.
	 * 
	 * @see OpClipboard::GetText
	 * @param mouse_selection_on Sets the active clipboard to the mouse-selection-mode one during the call. 
	 *                           @see OpClipboard::SetMouseSelectionMode
	 */
	OP_STATUS       GetText(OpString &text
#ifdef _X11_SELECTION_POLICY_
		, bool mouse_selection_on = false
#endif // _X11_SELECTION_POLICY_
		);

protected:
	// Implementing ClipboardListener
	virtual void    OnPaste(OpClipboard* clipboard);
	virtual void    OnCut(OpClipboard* clipboard) {}
	virtual void    OnCopy(OpClipboard* clipboard);

private:
	OpString*       m_clipboard_text;
	UINT32          m_clipboard_token;
	OP_STATUS       m_error;
};

#endif //DESKTOP_CLIPBOARD_MANAGER_H
