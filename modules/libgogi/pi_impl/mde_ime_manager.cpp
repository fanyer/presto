/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/libgogi/pi_impl/mde_ime_manager.h"

#ifdef WIDGETS_IME_SUPPORT

# ifdef MDE_CREATE_IME_MANAGER
OP_STATUS MDE_IMEManager::Create(MDE_IMEManager** new_mde_ime_manager)
{
    OP_ASSERT(new_mde_ime_manager);
	*new_mde_ime_manager = OP_NEW(MDE_IMEManager, ());
	if (*new_mde_ime_manager == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}
# endif // MDE_CREATE_IME_MANAGER

MDE_IMEManager::MDE_IMEManager()
{
}

MDE_IMEManager::~MDE_IMEManager(){}

void *MDE_IMEManager::CreateIME(MDE_OpView *opw, const OpRect &rect,
								const uni_char *text, BOOL multiline,
								BOOL passwd, int maxLength, Context context, int caretOffset,
								const FontInfo* fontinfo, const uni_char* format)
{
    return NULL;
}

void MDE_IMEManager::MoveIME(void *ime, const OpRect &rect)
{
}

void MDE_IMEManager::UpdateIMEText(void *ime, const uni_char *text)
{
}

void MDE_IMEManager::DestroyIME(void *ime)
{
}

void MDE_IMEManager::CommitIME(void *ime, const uni_char *text)
{
}

void MDE_IMEManager::UpdateIME(void *ime, const uni_char *text, int cursorpos, int highlight_start, int highlight_len)
{
}

void MDE_IMEManager::UpdateIME(void *ime, const uni_char *text)
{
}

#endif // WIDGETS_IME_SUPPORT
