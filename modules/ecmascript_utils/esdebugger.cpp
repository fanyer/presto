/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ECMASCRIPT_DEBUGGER

#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/ecmascript_utils/esenginedebugger.h"
#include "modules/ecmascript_utils/esremotedebugger.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/util/simset.h"

class ES_DebugThread;
class ES_DebugRuntime;
class ES_DebugSession;

/* ===== ES_DebugBackend ====================================== */

ES_DebugBackend::ES_DebugBackend()
	: frontend(NULL)
{
}

/* virtual */
ES_DebugBackend::~ES_DebugBackend()
{
}

/* ===== ES_DebugPosition ========================================== */

ES_DebugPosition::ES_DebugPosition()
	: scriptid(0), linenr(0)
{
}

ES_DebugPosition::ES_DebugPosition(unsigned scriptid, unsigned linenr)
	: scriptid(scriptid), linenr(linenr)
{
}

BOOL ES_DebugPosition::operator==(const ES_DebugPosition &other) const
{
	return scriptid == other.scriptid && linenr == other.linenr;
}

BOOL ES_DebugPosition::operator!=(const ES_DebugPosition &other) const
{
	return !operator==(other);
}

BOOL ES_DebugPosition::Valid() const
{
	return scriptid != 0 && linenr != 0;
}

/* ===== ES_DebugObjectInfo ============================================= */

ES_DebugObjectInfo::ES_DebugObjectInfo()
{
	prototype.info = 0;
	flags.init = 0;
	class_name = NULL;
	function_name = NULL;
}

ES_DebugObjectInfo::~ES_DebugObjectInfo()
{
	if (flags.packed.delete_function_name)
		op_free(function_name);
	if (flags.packed.delete_class_name)
		op_free(class_name);
	OP_DELETE(prototype.info);
}

/* ===== ES_DebugObjectProperties ======================================= */

ES_DebugObjectProperties::ES_DebugObjectProperties()
	: properties_count(0),
	  properties (NULL)
{
	object.info = 0;
}

ES_DebugObjectProperties::~ES_DebugObjectProperties()
{
	OP_DELETEA(properties);
	OP_DELETE(object.info);
}

/* ===== ES_DebugObjectProperties::Property ============================= */

ES_DebugObjectProperties::Property::Property()
{
	name = 0;
}

ES_DebugObjectProperties::Property::~Property()
{
	op_free(name);
}

/* ===== ES_DebugObjectChain ================================================== */

ES_DebugObjectChain::ES_DebugObjectChain()
	: prototype(NULL)
{
}

ES_DebugObjectChain::~ES_DebugObjectChain()
{
	OP_DELETE(prototype);
}

/* ===== ES_DebugValue ================================================== */

ES_DebugValue::ES_DebugValue()
	: type(VALUE_UNDEFINED),
	  is_8bit(FALSE),
	  owns_value(FALSE)
{
}

ES_DebugValue::~ES_DebugValue()
{
	if (owns_value)
		if (type == VALUE_STRING)
			if (is_8bit)
				OP_DELETEA(value.string8.value);
			else
				OP_DELETEA(value.string16.value);
		else if (type == VALUE_OBJECT)
			OP_DELETE(value.object.info);
}

OP_STATUS
ES_DebugValue::GetString(OpString& str) const
{
	if (type != VALUE_STRING && type != VALUE_STRING_WITH_LENGTH)
		return OpStatus::ERR;

	int len = KAll;

	if (type == VALUE_STRING_WITH_LENGTH)
		len = (is_8bit ? value.string8.length : value.string16.length);

	if (is_8bit)
		return str.Set(value.string8.value, len);
	else
		return str.Set(value.string16.value, len);
}

/* ===== ES_DebugReturnValue ============================================ */

ES_DebugReturnValue::ES_DebugReturnValue()
{
	function.id = 0;
	function.info = 0;
}

ES_DebugReturnValue::~ES_DebugReturnValue()
{
	if (function.id && function.info)
		OP_DELETE(function.info);

	OP_DELETE(next);
}

/* ===== ES_DebugStackFrame ============================================ */

ES_DebugStackFrame::ES_DebugStackFrame()
	: scope_chain(NULL)
	, scope_chain_length(0)
{
	function.info = 0;
	arguments.info = 0;
	this_obj.info = 0;
}

ES_DebugStackFrame::~ES_DebugStackFrame()
{
	OP_DELETE(function.info);
	OP_DELETE(arguments.info);
	OP_DELETE(this_obj.info);
	OP_DELETEA(scope_chain);
}

/* ===== ES_DebugFrontend =============================================== */

ES_DebugFrontend::ES_DebugFrontend()
	: dbg_backend(NULL)
{
}

ES_DebugFrontend::~ES_DebugFrontend()
{
}

OP_STATUS
ES_DebugFrontend::ConstructEngineBackend(ES_DebugWindowAccepter* accepter)
{
#ifdef ECMASCRIPT_ENGINE_DEBUGGER
	dbg_backend = OP_NEW(ES_EngineDebugBackend, ());

	if (!dbg_backend)
		return OpStatus::ERR_NO_MEMORY;

	return static_cast<ES_EngineDebugBackend *>(dbg_backend)->Construct(this, accepter);
#else // ECMASCRIPT_ENGINE_DEBUGGER
	return OpStatus::ERR;
#endif // ECMASCRIPT_ENGINE_DEBUGGER
}

OP_STATUS
ES_DebugFrontend::ConstructRemoteBackend(BOOL active, const OpStringC &address, int port)
{
#ifdef ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	dbg_backend = OP_NEW(ES_RemoteDebugBackend, ());

	if (!dbg_backend)
		return OpStatus::ERR_NO_MEMORY;

	return ((ES_RemoteDebugBackend *) dbg_backend)->Construct(this, active, address, port);
#else // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
	return OpStatus::ERR;
#endif // ECMASCRIPT_REMOTE_DEBUGGER_BACKEND
}

void
ES_DebugFrontend::ExportObject(ES_Runtime *rt, ES_Object *internal, ES_DebugObject &external, BOOL chain_info)
{
	dbg_backend->ExportObject(rt, internal, external, chain_info);
}

void
ES_DebugFrontend::ExportValue(ES_Runtime *rt, const ES_Value &internal, ES_DebugValue &external, BOOL chain_info)
{
	dbg_backend->ExportValue(rt, internal, external, chain_info);
}

OP_STATUS
ES_DebugFrontend::Detach()
{
	OP_STATUS status;

	if (dbg_backend)
	{
		status = dbg_backend->Detach();
		OP_DELETE(dbg_backend);
		dbg_backend = NULL;
	}
	else
		status = OpStatus::OK;

	return status;
}

OP_STATUS
ES_DebugFrontend::Runtimes(unsigned tag, OpUINTPTRVector &runtime_ids, BOOL force_create_all)
{
	return dbg_backend->Runtimes(tag, runtime_ids, force_create_all);
}

OP_STATUS
ES_DebugFrontend::Continue(unsigned dbg_runtime_id, ES_DebugFrontend::Mode mode)
{
	return dbg_backend->Continue(dbg_runtime_id, mode);
}

OP_STATUS
ES_DebugFrontend::Backtrace(ES_Runtime *runtime, ES_Context *context, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames)
{
	return dbg_backend->Backtrace(runtime, context, max_frames, length, frames);
}

OP_STATUS
ES_DebugFrontend::Backtrace(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames)
{
	return dbg_backend->Backtrace(dbg_runtime_id, dbg_thread_id, max_frames, length, frames);
}

OP_STATUS
ES_DebugFrontend::Eval(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count, BOOL want_debugging)
{
	return dbg_backend->Eval(tag, dbg_runtime_id, dbg_thread_id, frame_index, string, string_length, variables, variables_count, want_debugging);
}

OP_STATUS
ES_DebugFrontend::Examine(unsigned dbg_runtime_id, unsigned objects_count, unsigned *object_ids, BOOL examine_prototypes, BOOL skip_non_enum, ES_DebugPropertyFilters *filters, /* out */ ES_DebugObjectChain *&chains, unsigned int async_tag)
{
	return dbg_backend->Examine(dbg_runtime_id, objects_count, object_ids, examine_prototypes, skip_non_enum, filters, chains, async_tag);
}

BOOL
ES_DebugFrontend::HasBreakpoint(unsigned id)
{
	return dbg_backend->HasBreakpoint(id);
}

OP_STATUS
ES_DebugFrontend::AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data)
{
	return dbg_backend->AddBreakpoint(id, data);
}

OP_STATUS
ES_DebugFrontend::RemoveBreakpoint(unsigned id)
{
	return dbg_backend->RemoveBreakpoint(id);
}

OP_STATUS
ES_DebugFrontend::SetStopAt(const StopType stop_type, const BOOL value)
{
	return dbg_backend->SetStopAt(stop_type, value);
}

OP_STATUS
ES_DebugFrontend::GetStopAt(StopType stop_type, BOOL &return_value)
{
	return dbg_backend->GetStopAt(stop_type, return_value);
}

OP_STATUS
ES_DebugFrontend::Break(unsigned dbg_runtime_id, unsigned dbg_thread_id)
{
	return dbg_backend->Break(dbg_runtime_id, dbg_thread_id);
}

void
ES_DebugFrontend::Prune()
{
	if (dbg_backend)
		dbg_backend->Prune();
}

void
ES_DebugFrontend::FreeObject(unsigned object_id)
{
	if (dbg_backend)
		dbg_backend->FreeObject(object_id);
}

void
ES_DebugFrontend::FreeObjects()
{
	if (dbg_backend)
		dbg_backend->FreeObjects();
}

ES_Object *
ES_DebugFrontend::GetObjectById(ES_Runtime *runtime, unsigned object_id)
{
	return dbg_backend->GetObjectById(runtime, object_id);
}

ES_Object *
ES_DebugFrontend::GetObjectById(unsigned object_id)
{
	return dbg_backend->GetObjectById(object_id);
}

ES_Runtime *
ES_DebugFrontend::GetRuntimeById(unsigned runtime_id)
{
	return dbg_backend->GetRuntimeById(runtime_id);
}

unsigned
ES_DebugFrontend::GetObjectId(ES_Runtime *rt, ES_Object *object)
{
	return dbg_backend->GetObjectId(rt, object);
}

unsigned
ES_DebugFrontend::GetRuntimeId(ES_Runtime *runtime)
{
	return dbg_backend->GetRuntimeId(runtime);
}

unsigned
ES_DebugFrontend::GetThreadId(ES_Thread *thread)
{
	return dbg_backend->GetThreadId(thread);
}

ES_DebugReturnValue *
ES_DebugFrontend::GetReturnValue(unsigned dbg_runtime_id, unsigned dbg_thread_id)
{
	return dbg_backend->GetReturnValue(dbg_runtime_id, dbg_thread_id);
}

OP_STATUS
ES_DebugFrontend::GetFunctionPosition(unsigned dbg_runtime_id, unsigned function_id, unsigned &script_guid, unsigned &line_no)
{
	return dbg_backend->GetFunctionPosition(dbg_runtime_id, function_id, script_guid, line_no);
}

void
ES_DebugFrontend::ResetFunctionFilter()
{
	dbg_backend->ResetFunctionFilter();
}

OP_STATUS
ES_DebugFrontend::AddFunctionClassToFilter(const char* classname)
{
	return dbg_backend->AddFunctionClassToFilter(classname);
}

void
ES_DebugFrontend::SetReformatFlag(BOOL enable)
{
	dbg_backend->SetReformatFlag(enable);
}

OP_STATUS
ES_DebugFrontend::SetReformatConditionFunction(const uni_char *source)
{
	return dbg_backend->SetReformatConditionFunction(source);
}

#endif // ECMASCRIPT_DEBUGGER
