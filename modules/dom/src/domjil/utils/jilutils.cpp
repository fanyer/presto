/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/utils/jilutils.h"

#include "modules/device_api/jil/JILFeatures.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/ecmascript/ecmascript.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/dom/src/domjil/utils/jilapplicationnames.h"
#include "modules/dom/src/domjil/domjilobject.h"

#include "modules/pi/OpSystemInfo.h"

#define JILPATHSEPCHAR '/'


JILUtils::JILUtils()
: m_jil_application_names(NULL)
{
}

JILUtils::~JILUtils()
{
	OP_DELETE(m_jil_application_names);
}

void JILUtils::ConstructL()
{
	m_jil_application_names = OP_NEW_L(JILApplicationNames, ());
	m_jil_application_names->ConstructL();
}

OpGadget* JILUtils::GetGadgetFromRuntime(DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime->GetFramesDocument());
	Window* wnd = origining_runtime->GetFramesDocument()->GetWindow();
	OP_ASSERT(wnd);
	if (wnd->GetType() != WIN_TYPE_GADGET)
		return NULL;
	OP_ASSERT(wnd->GetGadget());
	return wnd->GetGadget();
}

BOOL JILUtils::RuntimeHasFilesystemAccess(DOM_Runtime* origining_runtime)
{
	OP_ASSERT(GetGadgetFromRuntime(origining_runtime));
	OpGadgetClass* gadget_class = GetGadgetFromRuntime(origining_runtime)->GetClass();
	OP_ASSERT(gadget_class);
	return gadget_class->HasJILFilesystemAccess();
}

/* static */ BOOL
JILUtils::ValidatePhoneNumber(const uni_char* number)
{
	// Since JIL specification says we should remove all non-numeric-or-plus
	// characters, then if there is at least one digit, this is proper phone number.
	uni_char* found = uni_strpbrk(number, UNI_L("0123456789"));
	return found ? TRUE : FALSE;
}

BOOL JILUtils::IsJILApiRequested(JILFeatures::JIL_Api api_name, DOM_Runtime* runtime)
{
	return JILFeatures::IsJILApiRequested(api_name, GetGadgetFromRuntime(runtime));
}


/* virtual */ void
JILUtils::SetSystemResourceCallbackSuspendingImpl::OnFinished(OP_SYSTEM_RESOURCE_SETTER_STATUS status)
{
	if (OpStatus::IsError(status))
		DOM_SuspendCallbackBase::OnFailed(status);
	else
		DOM_SuspendCallbackBase::OnSuccess();
}

#endif // DOM_JIL_API_SUPPORT
