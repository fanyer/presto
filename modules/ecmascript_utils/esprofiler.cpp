/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esprofiler.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/doc/frm_doc.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef ESUTILS_PROFILER_SUPPORT

class ES_ProfilerImpl
	: public ES_DebugListener
{
public:
	class Script
	{
	public:
		Script(unsigned guid) : guid(guid) {}
		unsigned guid;
		OpString description;
		OpVector<OpString> lines;
	};

	class ThreadInternal
	{
	public:
		OpINT32HashTable<ES_Profiler::ScriptStatistics> scripts;
	};

	ES_Runtime *target;
	ES_Profiler::Listener *listener;
	BOOL ignore;
	OpINT32HashTable<Script> scripts;
	OpPointerHashTable<ES_Context, ES_Profiler::ThreadStatistics> threads;
	OpPointerHashTable<ES_Context, ThreadInternal> threads_internal;

	ES_Context *current_context;
	double previous_entercontext_time, previous_event_time;
	ES_Profiler::ThreadStatistics *current_thread;
	ThreadInternal *current_thread_internal;
	double *current_self_milliseconds, *current_total_milliseconds;
	OpVector<double> current_milliseconds;

	ES_ProfilerImpl(ES_Runtime *target)
		: target(target),
		  listener(NULL),
		  ignore(FALSE),
		  current_thread(NULL),
		  current_thread_internal(NULL),
		  current_self_milliseconds(NULL),
		  current_total_milliseconds(NULL)
	{
		g_ecmaManager->SetDebugListener(this);
	}

	~ES_ProfilerImpl()
	{
		g_ecmaManager->SetDebugListener(NULL);
	}

	/* From ES_DebugListener: */

	virtual BOOL EnableDebugging(ES_Runtime *runtime)
	{
		return TRUE;
	}

	virtual void NewScript(ES_Runtime *runtime, ES_DebugScriptData *data)
	{
		if (listener)
		{
			Script *script = OP_NEW(Script, (data->script_guid));

			script->description.Set(data->type);

			if (data->url)
			{
				script->description.Append(" from ");
				script->description.Append(data->url);
			}

			const uni_char *source = data->source;
			while (*source)
			{
				const uni_char *start = source;
				while (*source && *source != 10 && *source != 13)
					++source;

				OpString *line = OP_NEW(OpString, ());
				line->Set(start, source - start);
				script->lines.Add(line);

				++source;
				if (*source && (*source == 10 && source[-1] != 10) || (*source == 13 && source[-1] != 13))
					++source;
			}

			scripts.Add(script->guid, script);
		}
	}

	virtual void ParseError(ES_Runtime * /*runtime*/, ES_ErrorInfo * /*err*/)
	{
	}

	virtual void NewContext(ES_Runtime *runtime, ES_Context *context)
	{
		if (listener && runtime == target && !ignore)
		{
			ES_Profiler::ThreadStatistics *thread_statistics = OP_NEW(ES_Profiler::ThreadStatistics, ());

			thread_statistics->total_hits = 0;
			thread_statistics->total_milliseconds = 0;

			threads.Add(context, thread_statistics);
			threads_internal.Add(context, OP_NEW(ThreadInternal, ()));
		}
	}

	virtual void EnterContext(ES_Runtime *runtime, ES_Context *context)
	{
		if (listener && runtime == target && threads.GetData(context, &current_thread) == OpStatus::OK)
		{
			current_context = context;

			if (current_thread->description.IsEmpty())
				if (FramesDocument *document = runtime->GetFramesDocument())
				{
					ES_Thread *thread = document->GetESScheduler()->GetCurrentThread();

					if (thread && thread->GetContext() == context)
						current_thread->description.Set(thread->GetInfoString());
				}

			threads_internal.GetData(context, &current_thread_internal);
			previous_entercontext_time = previous_event_time = g_op_time_info->GetRuntimeMS();
		}
	}

	virtual void LeaveContext(ES_Runtime *runtime, ES_Context *context)
	{
		if (current_thread)
		{
			double now = g_op_time_info->GetRuntimeMS();

			current_thread->total_milliseconds += now - previous_entercontext_time;
			current_thread = NULL;

			if (current_self_milliseconds)
			{
				*current_self_milliseconds += now - previous_event_time;
				*current_total_milliseconds += now - previous_event_time;
			}

			for (unsigned index = 0; index < current_milliseconds.GetCount(); ++index)
				*current_milliseconds.Get(index) += now - previous_event_time;

			current_context = NULL;
		}
	}

	virtual void DestroyContext(ES_Runtime *runtime, ES_Context *context)
	{
		ES_Profiler::ThreadStatistics *thread_statistics;
		if (listener && runtime == target && threads.Remove(context, &thread_statistics) == OpStatus::OK)
		{
			ignore = TRUE;
			listener->OnThreadExecuted(thread_statistics);
			ignore = FALSE;
		}
	}

	virtual void DestroyRuntime(ES_Runtime *runtime)
	{
		if (runtime == target)
		{
			if (listener)
				listener->OnTargetDestroyed();

			g_ecmaManager->SetDebugListener(NULL);
		}
	}

	virtual void DestroyObject(ES_Object *object)
	{
	}

	virtual EventAction HandleEvent(EventType type, unsigned int script_guid, unsigned int line_no)
	{
		if (!current_thread)
			return ESACT_CONTINUE;

		--line_no;

		/* Ignore these. */
		switch (type)
		{
		case ESEV_ERROR:
		case ESEV_EXCEPTION:
		case ESEV_ABORT:
		case ESEV_BREAKHERE:
			return ESACT_CONTINUE;
		}

		double now = g_op_time_info->GetRuntimeMS();

		if (type == ESEV_CALLSTARTING && current_total_milliseconds)
			current_milliseconds.Add(current_total_milliseconds);

		if (script_guid == 0 || line_no == 0)
		{
			if (current_self_milliseconds && current_total_milliseconds)
			{
				*current_self_milliseconds += now - previous_event_time;
				*current_total_milliseconds += now - previous_event_time;
				current_self_milliseconds = NULL;
				current_total_milliseconds = NULL;
			}

			for (unsigned index = 0; index < current_milliseconds.GetCount(); ++index)
				*current_milliseconds.Get(index) += now - previous_event_time;

			previous_event_time = now;
			return ESACT_CONTINUE;
		}

		if (type == ESEV_CALLCOMPLETED)
			*current_milliseconds.Remove(current_milliseconds.GetCount() - 1) += now - previous_event_time;

		Script *script;

		if (scripts.GetData(script_guid, &script) == OpStatus::OK && line_no < script->lines.GetCount())
		{
			ES_Profiler::ScriptStatistics *script_statistics;

			if (current_thread_internal->scripts.GetData(script_guid, &script_statistics) != OpStatus::OK)
			{
				script_statistics = OP_NEW(ES_Profiler::ScriptStatistics, ());
				script_statistics->description.Set(script->description.CStr());
				script_statistics->lines.SetAllocationStepSize(script->lines.GetCount());
				script_statistics->hits.SetAllocationStepSize(script->lines.GetCount());
				script_statistics->milliseconds_self.SetAllocationStepSize(script->lines.GetCount());
				script_statistics->milliseconds_total.SetAllocationStepSize(script->lines.GetCount());

				for (unsigned index = 0; index < script->lines.GetCount(); ++index)
				{
					script_statistics->lines.Add(script->lines.Get(index));

					unsigned *hits = OP_NEW(unsigned, ());
					*hits = 0;
					script_statistics->hits.Add(hits);

					double *milliseconds_self = OP_NEW(double, ());
					*milliseconds_self = 0;
					script_statistics->milliseconds_self.Add(milliseconds_self);

					double *milliseconds_total = OP_NEW(double, ());
					*milliseconds_total = 0;
					script_statistics->milliseconds_total.Add(milliseconds_total);
				}

				current_thread_internal->scripts.Add(script_guid, script_statistics);
				current_thread->scripts.Add(script_statistics);
			}

			if (current_self_milliseconds && current_total_milliseconds)
			{
				*current_self_milliseconds += now - previous_event_time;
				*current_total_milliseconds += now - previous_event_time;
			}

			for (unsigned index = 0; index < current_milliseconds.GetCount(); ++index)
				*current_milliseconds.Get(index) += now - previous_event_time;

			if (type != ESEV_CALLSTARTING)
			{
				current_self_milliseconds = script_statistics->milliseconds_self.Get(line_no);
				current_total_milliseconds = script_statistics->milliseconds_total.Get(line_no);
			}
			else
				current_self_milliseconds = current_total_milliseconds = NULL;

			if (type == ESEV_STATEMENT)
			{
				++current_thread->total_hits;
				++*script_statistics->hits.Get(line_no);
			}
		}

		previous_event_time = now;
		return ESACT_CONTINUE;
	}

	virtual void RuntimeCreated(ES_Runtime *runtime)
	{
	}
};

/* static */ OP_STATUS
ES_Profiler::Make(ES_Profiler *&profiler, ES_Runtime *target)
{
	ES_ProfilerImpl *impl = OP_NEW(ES_ProfilerImpl, (target));
	profiler = OP_NEW(ES_Profiler, (impl));
	return OpStatus::OK;
}

ES_Profiler::~ES_Profiler()
{
	OP_DELETE(impl);
}

void
ES_Profiler::SetListener(Listener *listener)
{
	impl->listener = listener;
}

ES_Profiler::ES_Profiler(ES_ProfilerImpl *impl)
	: impl(impl)
{
}

#endif // ESUTILS_PROFILER_SUPPORT
