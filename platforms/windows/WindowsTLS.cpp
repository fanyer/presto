/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifndef PLUGIN_WRAPPER

#include "platforms/windows/WindowsTLS.h"

DWORD WindowsTLS::s_index;

bool WindowsTLS::Init()
{
	s_index = TlsAlloc();

	if (s_index == TLS_OUT_OF_INDEXES)
		return false;

	return AttachThread();
}

bool WindowsTLS::AttachThread()
{
	WindowsTLS* tls = OP_NEW(WindowsTLS, ());

	//If we are out of memory here, something is seriously wrong. We can't fail the dll load at this point, so exiting
	if(!tls)
		ExitProcess(1);

#if defined(_DEBUG) && defined(MEMTOOLS_ENABLE_CODELOC)
	OpMemDebug_Disregard(tls);
#endif
	TlsSetValue(s_index, tls);
	return true;
}

void WindowsTLS::DetachThread()
{
	OP_DELETE(Get());
	TlsSetValue(s_index, NULL);
}

void WindowsTLS::Destroy()
{
	DetachThread();
	TlsFree(s_index);
}

#endif // !PLUGIN_WRAPPER
