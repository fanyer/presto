/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/managers/DesktopClipboardManager.h"

#include "modules/pi/OpClipboard.h"

DesktopClipboardManager::DesktopClipboardManager()
	: m_clipboard_text(NULL)
	, m_clipboard_token(0)
	, m_error(OpStatus::OK)
{
}

BOOL DesktopClipboardManager::HasText(
#ifdef _X11_SELECTION_POLICY_
		bool mouse_selection_on
#endif // _X11_SELECTION_POLICY_
		)
{
#ifdef _X11_SELECTION_POLICY_
	if (mouse_selection_on)
		g_clipboard_manager->SetMouseSelectionMode(TRUE);
#endif // _X11_SELECTION_POLICY_

	BOOL has_text = g_clipboard_manager->HasText();

#ifdef _X11_SELECTION_POLICY_
	if (mouse_selection_on)
		g_clipboard_manager->SetMouseSelectionMode(FALSE);
#endif // _X11_SELECTION_POLICY_

	return has_text;
}

OP_STATUS DesktopClipboardManager::PlaceText(const uni_char* text, UINT32 token
#ifdef _X11_SELECTION_POLICY_
		, bool mouse_selection_on
#endif // _X11_SELECTION_POLICY_
	)
{
	OP_ASSERT(!m_clipboard_text && m_clipboard_token == 0);

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;

	m_clipboard_text = OP_NEW(OpString, ());
	if (m_clipboard_text)
	{
		m_clipboard_token = token;
		status = m_clipboard_text->Set(text);
		if (OpStatus::IsSuccess(status))
		{
#ifdef _X11_SELECTION_POLICY_
			if (mouse_selection_on)
				g_clipboard_manager->SetMouseSelectionMode(TRUE);
#endif // _X11_SELECTION_POLICY_

			status = g_clipboard_manager->Copy(this, token); // calls OnCopy()

#ifdef _X11_SELECTION_POLICY_
			if (mouse_selection_on)
				g_clipboard_manager->SetMouseSelectionMode(FALSE);
#endif // _X11_SELECTION_POLICY_
		}

		OP_DELETE(m_clipboard_text);
		m_clipboard_text  = NULL;
		m_clipboard_token = 0;
	}

	OP_STATUS retval = OpStatus::IsError(status) ? status : m_error;
	m_error = OpStatus::OK;
	return retval;
}

OP_STATUS DesktopClipboardManager::GetText(OpString &text
#ifdef _X11_SELECTION_POLICY_
		, bool mouse_selection_on
#endif // _X11_SELECTION_POLICY_
	)
{
	OP_ASSERT(!m_clipboard_text);

	m_clipboard_text = &text;

#ifdef _X11_SELECTION_POLICY_
	if (mouse_selection_on)
		g_clipboard_manager->SetMouseSelectionMode(TRUE);
#endif // _X11_SELECTION_POLICY_

	m_clipboard_text = &text;
	OP_STATUS error = g_clipboard_manager->Paste(this); // calls OnPaste()
	m_clipboard_text = NULL;

#ifdef _X11_SELECTION_POLICY_
	if (mouse_selection_on)
		g_clipboard_manager->SetMouseSelectionMode(FALSE);
#endif // _X11_SELECTION_POLICY_

	OP_STATUS retval = OpStatus::IsError(error) ? error : m_error;
	m_error = OpStatus::OK;
	return retval;
}

void DesktopClipboardManager::OnPaste(OpClipboard* clipboard)
{
	m_error = clipboard->GetText(*m_clipboard_text);
}

void DesktopClipboardManager::OnCopy(OpClipboard* clipboard)
{
	m_error = clipboard->PlaceText(m_clipboard_text->CStr(), m_clipboard_token);
}
