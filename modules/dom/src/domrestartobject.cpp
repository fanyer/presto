/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domrestartobject.h"

OP_STATUS DOM_RestartObject::KeepAlive()
{
	return keep_alive.Protect(GetRuntime(), GetNativeObject());
}

void
DOM_RestartObject::Block(ES_Value* restart_value, ES_Runtime* origining_runtime)
{
	OP_ASSERT(keep_alive.GetObject() == GetNativeObject() || !"The restart object has't been protected yet - possible crash."); 
	(thread = GetCurrentThread(origining_runtime))->Block();
	thread->AddListener(this);
	DOMSetObject(restart_value, GetNativeObject());
}

void
DOM_RestartObject::Resume()
{
	keep_alive.Unprotect();
	if (thread)
	{
		ES_ThreadListener::Remove();
		OpStatus::Ignore(thread->Unblock());
		thread = NULL;
	}
}

/* virtual */ OP_STATUS
DOM_RestartObject::Signal(ES_Thread* , ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
		OP_ASSERT(!"Should not happen while thread is blocked.");

	case ES_SIGNAL_CANCELLED:
		ES_ThreadListener::Remove();
		thread = NULL;
		OnAbort();
	}
	return OpStatus::OK;
}