/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/dom/src/domasynccallback.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"

OP_STATUS
DOM_AutoProtectPtr::Protect()
{
	OP_ASSERT(!m_was_protected);
	if (!m_obj)
		return OpStatus::OK;
	if (!m_runtime->Protect(m_obj))
		return OpStatus::ERR_NO_MEMORY;
	m_was_protected = TRUE;
	return OpStatus::OK;
}

void
DOM_AutoProtectPtr::Reset(ES_Object* obj)
{
	if (m_was_protected)
		m_runtime->Unprotect(m_obj);
	m_was_protected = FALSE;
	m_obj = obj;	// TODO: this method should either protect the new object or not accept any arguments and just be called Release.
}

DOM_AutoProtectPtr::~DOM_AutoProtectPtr()
{
	Reset(NULL);
}

DOM_AutoProtectPtr::DOM_AutoProtectPtr(ES_Object* obj, ES_Runtime* runtime)
	: m_obj(obj)
	, m_runtime(runtime)
	, m_was_protected(FALSE)
{
	OP_ASSERT(m_runtime);
}


DOM_AsyncCallback::DOM_AsyncCallback(DOM_Runtime* runtime, ES_Object* callback, ES_Object* this_object)
	: m_runtime(runtime) // maybe we should get runtime from callback ?
	, m_callback_protector(callback, runtime)
	, m_this_object_protector(this_object, runtime)
	, m_aborted(FALSE)
{
	OP_ASSERT(m_runtime);
	OP_ASSERT(m_this_object_protector.Get());
}

DOM_AsyncCallback::~DOM_AsyncCallback()
{
	Out();
}

/* virtual */ OP_STATUS
DOM_AsyncCallback::Construct()
{
	RETURN_IF_ERROR(m_callback_protector.Protect());
	RETURN_IF_ERROR(m_this_object_protector.Protect());
	m_runtime->GetEnvironment()->AddAsyncCallback(this);
	return OpStatus::OK;
}

void
DOM_AsyncCallback::Abort()
{
	m_aborted = TRUE;
	m_callback_protector.Reset(NULL);
	m_this_object_protector.Reset(NULL);
}

void
DOM_AsyncCallback::OnBeforeDestroy()
{
	Abort();
	m_runtime = NULL;
}

OP_STATUS
DOM_AsyncCallback::CallCallback(ES_Value* argv, unsigned int argc)
{
	if (m_callback_protector.Get() && !IsAborted())
	{
		ES_AsyncInterface* async_iface = m_runtime->GetEnvironment()->GetAsyncInterface();
		OP_ASSERT(async_iface);
		return async_iface->CallFunction(m_callback_protector.Get(), m_this_object_protector.Get(), argc, argv, NULL, NULL);
	}
	else
		return OpStatus::ERR;
}

