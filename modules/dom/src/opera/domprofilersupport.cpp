/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef ESUTILS_PROFILER_SUPPORT

#include "modules/dom/src/opera/domprofilersupport.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esasyncif.h"

/* == DOM_ScriptStatistics_lines == */

/* static */ OP_STATUS
DOM_ScriptStatistics_lines::Make(DOM_ScriptStatistics_lines *&lines, DOM_ScriptStatistics *parent)
{
	return DOMSetObjectRuntime(lines = OP_NEW(DOM_ScriptStatistics_lines, (parent)), parent->GetRuntime(), parent->GetRuntime()->GetArrayPrototype(), "LinesArray");
}

/* virtual */ ES_GetState
DOM_ScriptStatistics_lines::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, parent->statistics->lines.GetCount());
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ScriptStatistics_lines::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_ScriptStatistics_lines::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= parent->statistics->lines.GetCount())
		return GetNameDOMException(INDEX_SIZE_ERR, value);

	DOMSetString(value, parent->statistics->lines.Get(property_index)->CStr());
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_ScriptStatistics_lines::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= parent->statistics->lines.GetCount())
		return PutNameDOMException(INDEX_SIZE_ERR, value);
	else
		return PUT_READ_ONLY;
}

/* virtual */ void
DOM_ScriptStatistics_lines::GCTrace()
{
	GCMark(parent);
}


/* == DOM_ScriptStatistics_hits == */

/* static */ OP_STATUS
DOM_ScriptStatistics_hits::Make(DOM_ScriptStatistics_hits *&hits, DOM_ScriptStatistics *parent)
{
	return DOMSetObjectRuntime(hits = OP_NEW(DOM_ScriptStatistics_hits, (parent)), parent->GetRuntime(), parent->GetRuntime()->GetArrayPrototype(), "HitsArray");
}

/* virtual */ ES_GetState
DOM_ScriptStatistics_hits::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, parent->statistics->lines.GetCount());
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ScriptStatistics_hits::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_ScriptStatistics_hits::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= parent->statistics->hits.GetCount())
		return GetNameDOMException(INDEX_SIZE_ERR, value);

	DOMSetNumber(value, *parent->statistics->hits.Get(property_index));
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_ScriptStatistics_hits::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= parent->statistics->hits.GetCount())
		return PutNameDOMException(INDEX_SIZE_ERR, value);
	else
		return PUT_READ_ONLY;
}

/* virtual */ void
DOM_ScriptStatistics_hits::GCTrace()
{
	GCMark(parent);
}


/* == DOM_ScriptStatistics_milliseconds == */

/* static */ OP_STATUS
DOM_ScriptStatistics_milliseconds::Make(DOM_ScriptStatistics_milliseconds *&milliseconds, DOM_ScriptStatistics *parent, OpVector<double> *vector)
{
	return DOMSetObjectRuntime(milliseconds = OP_NEW(DOM_ScriptStatistics_milliseconds, (parent, vector)), parent->GetRuntime(), parent->GetRuntime()->GetArrayPrototype(), "MillisecondsArray");
}

/* virtual */ ES_GetState
DOM_ScriptStatistics_milliseconds::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, parent->statistics->lines.GetCount());
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ScriptStatistics_milliseconds::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_ScriptStatistics_milliseconds::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= milliseconds->GetCount())
		return GetNameDOMException(INDEX_SIZE_ERR, value);

	DOMSetNumber(value, *milliseconds->Get(property_index));
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_ScriptStatistics_milliseconds::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= milliseconds->GetCount())
		return PutNameDOMException(INDEX_SIZE_ERR, value);
	else
		return PUT_READ_ONLY;
}

/* virtual */ void
DOM_ScriptStatistics_milliseconds::GCTrace()
{
	GCMark(parent);
}


/* == DOM_ScriptStatistics == */

/* static */ OP_STATUS
DOM_ScriptStatistics::Make(DOM_ScriptStatistics *&script, DOM_ThreadStatistics *parent, ES_Profiler::ScriptStatistics *statistics)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(script = OP_NEW(DOM_ScriptStatistics, (parent, statistics)), parent->GetRuntime(), parent->GetRuntime()->GetObjectPrototype(), "ScriptStatistics"));

	RETURN_IF_ERROR(DOM_ScriptStatistics_lines::Make(script->lines, script));
	RETURN_IF_ERROR(DOM_ScriptStatistics_hits::Make(script->hits, script));
	RETURN_IF_ERROR(DOM_ScriptStatistics_milliseconds::Make(script->milliseconds_self, script, &statistics->milliseconds_self));
	RETURN_IF_ERROR(DOM_ScriptStatistics_milliseconds::Make(script->milliseconds_total, script, &statistics->milliseconds_total));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ScriptStatistics::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_lines:
		DOMSetObject(value, lines);
		return GET_SUCCESS;

	case OP_ATOM_hits:
		DOMSetObject(value, hits);
		return GET_SUCCESS;

	case OP_ATOM_millisecondsSelf:
		DOMSetObject(value, milliseconds_self);
		return GET_SUCCESS;

	case OP_ATOM_millisecondsTotal:
		DOMSetObject(value, milliseconds_total);
		return GET_SUCCESS;

	case OP_ATOM_description:
		DOMSetString(value, statistics->description.CStr());
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ScriptStatistics::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_lines:
	case OP_ATOM_hits:
	case OP_ATOM_millisecondsSelf:
	case OP_ATOM_millisecondsTotal:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_ScriptStatistics::GCTrace()
{
	GCMark(parent);
	GCMark(lines);
	GCMark(hits);
	GCMark(milliseconds_self);
	GCMark(milliseconds_total);
}


/* == DOM_ThreadStatistics_scripts == */

/* static */ OP_STATUS
DOM_ThreadStatistics_scripts::Make(DOM_ThreadStatistics_scripts *&scripts, DOM_ThreadStatistics *parent)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(scripts = OP_NEW(DOM_ThreadStatistics_scripts, (parent)), parent->GetRuntime(), parent->GetRuntime()->GetArrayPrototype(), "MillisecondsArray"));

	unsigned count = parent->statistics->scripts.GetCount();

	scripts->items = OP_NEWA(DOM_ScriptStatistics *, count);

	for (unsigned index = 0; index < count; ++index)
		RETURN_IF_ERROR(DOM_ScriptStatistics::Make(scripts->items[index], parent, parent->statistics->scripts.Get(index)));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ThreadStatistics_scripts::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		DOMSetNumber(value, parent->statistics->scripts.GetCount());
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ThreadStatistics_scripts::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_ThreadStatistics_scripts::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= parent->statistics->scripts.GetCount())
		return GetNameDOMException(INDEX_SIZE_ERR, value);

	DOMSetObject(value, items[property_index]);
	return GET_SUCCESS;
}

/* virtual */ ES_PutState
DOM_ThreadStatistics_scripts::PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index < 0 || (unsigned) property_index >= parent->statistics->scripts.GetCount())
		return PutNameDOMException(INDEX_SIZE_ERR, value);
	else
		return PUT_READ_ONLY;
}

/* virtual */ void
DOM_ThreadStatistics_scripts::GCTrace()
{
	GCMark(parent);

	unsigned count = parent->statistics->scripts.GetCount();

	for (unsigned index = 0; index < count; ++index)
		GCMark(items[index]);
}


/* == DOM_ThreadStatistics == */

/* static */ OP_STATUS
DOM_ThreadStatistics::Make(DOM_ThreadStatistics *&thread, DOM_Profiler *parent, ES_Profiler::ThreadStatistics *statistics)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(thread = OP_NEW(DOM_ThreadStatistics, (statistics)), parent->GetRuntime(), parent->GetRuntime()->GetObjectPrototype(), "ThreadStatistics"));

	RETURN_IF_ERROR(DOM_ThreadStatistics_scripts::Make(thread->scripts, thread));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ThreadStatistics::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_scripts:
		DOMSetObject(value, scripts);
		return GET_SUCCESS;

	case OP_ATOM_description:
		DOMSetString(value, statistics->description.CStr());
		return GET_SUCCESS;

	case OP_ATOM_hits:
		DOMSetNumber(value, statistics->total_hits);
		return GET_SUCCESS;

	case OP_ATOM_millisecondsTotal:
		DOMSetNumber(value, statistics->total_milliseconds);
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ThreadStatistics::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_scripts:
	case OP_ATOM_description:
	case OP_ATOM_hits:
	case OP_ATOM_millisecondsTotal:
		return PUT_READ_ONLY;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_ThreadStatistics::GCTrace()
{
	GCMark(scripts);
}

/* static */ OP_STATUS
DOM_Profiler::Make(DOM_Profiler *&profiler, DOM_Object *reference)
{
	return DOMSetObjectRuntime(profiler = OP_NEW(DOM_Profiler, ()), reference->GetRuntime(), reference->GetRuntime()->GetObjectPrototype(), "Profiler");
}

/* virtual */ void
DOM_Profiler::Initialise(ES_Runtime *runtime)
{
	AddFunctionL(start, "start");
	AddFunctionL(start, "stop");
}

/* virtual */ ES_GetState
DOM_Profiler::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_onthread:
		DOMSetObject(value, onthread);
		return GET_SUCCESS;

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_Profiler::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_onthread:
		if (value->type == VALUE_OBJECT)
			onthread = value->value.object;
		return PUT_SUCCESS;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_Profiler::GCTrace()
{
	GCMark(onthread);
}

/* virtual */ void
DOM_Profiler::OnThreadExecuted(ES_Profiler::ThreadStatistics *statistics)
{
	if (onthread)
	{
		DOM_ThreadStatistics *thread;
		if (OpStatus::IsSuccess(DOM_ThreadStatistics::Make(thread, this, statistics)))
		{
			ES_Value argv[1];
			DOMSetObject(&argv[0], thread);
			OpStatus::Ignore(GetEnvironment()->GetAsyncInterface()->CallFunction(onthread, GetNativeObject(), 1, argv));
		}
	}
}

/* virtual */ void
DOM_Profiler::OnTargetDestroyed()
{
}

/* static */ int
DOM_Profiler::start(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	/* Note: this is dangerous. */
	DOM_THIS_OBJECT(profiler, DOM_TYPE_OBJECT, DOM_Profiler);

	if (!profiler->profiler)
	{
		CALL_FAILED_IF_ERROR(ES_Profiler::Make(profiler->profiler, profiler->GetRuntime()));
		profiler->profiler->SetListener(profiler);
	}

	return ES_FAILED;
}

/* static */ int
DOM_Profiler::stop(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	/* Note: this is dangerous. */
	DOM_THIS_OBJECT(profiler, DOM_TYPE_OBJECT, DOM_Profiler);

	OP_DELETE(profiler->profiler);
	profiler->profiler = NULL;

	return ES_FAILED;
}

#endif // ESUTILS_PROFILER_SUPPORT
