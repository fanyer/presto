/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_PROFILER

#include "modules/scope/src/scope_profiler.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/docman.h"
#include "modules/pi/OpSystemInfo.h"

OpScopeProfiler::OpScopeProfiler()
	: OpScopeProfiler_SI()
	, current_session(NULL)
	, frame_ids(NULL)
	, next_timeline_id(1)
{
}

/* virtual */
OpScopeProfiler::~OpScopeProfiler()
{
	StopProfiling();

	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_PROCESS_TIMELINE);
	frame_ids->Release();
}

OP_STATUS
OpScopeProfiler::Construct(OpScopeIDTable<DocumentManager> *frame_ids)
{
	this->frame_ids = frame_ids->Retain();

	return g_main_message_handler->SetCallBack(this, MSG_SCOPE_PROCESS_TIMELINE, 0);
}

/* virtual */ OP_STATUS
OpScopeProfiler::OnServiceDisabled()
{
	StopProfiling();

	session_ids.Purge();

	current_session = NULL;

	next_timeline_id = 1;

	return OpStatus::OK;
}

OP_STATUS
OpScopeProfiler::OnAboutToLoadDocument(DocumentManager *docman, const OpScopeDocumentListener::AboutToLoadDocumentArgs &args)
{
	if (!IsEnabled() || !AcceptDocumentManager(docman) || !current_session->GetStartOnLoad())
		return OpStatus::OK;

	current_session->SetStartOnLoad(FALSE);

	return StartProfiling();
}

void
OpScopeProfiler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_SCOPE_PROCESS_TIMELINE)
	{
		unsigned async_tag = static_cast<unsigned>(par1);

		AsyncTask *task = async_tasks.First();

		while (task && task->GetAsyncTag() != async_tag)
			task = task->Suc();

		if (task)
			RAISE_IF_MEMORY_ERROR(PerformTask(task));
	}
}


static OpScopeProfiler::ScriptThreadType
ConvertScriptThreadType(ES_ThreadType type)
{
	switch (type)
	{
	default:
	case ES_THREAD_EMPTY:
	case ES_THREAD_TERMINATING:
		return OpScopeProfiler::SCRIPTTHREADTYPE_UNKNOWN;
	case ES_THREAD_COMMON:
		return OpScopeProfiler::SCRIPTTHREADTYPE_COMMON;
	case ES_THREAD_TIMEOUT:
		return OpScopeProfiler::SCRIPTTHREADTYPE_TIMEOUT;
	case ES_THREAD_EVENT:
		return OpScopeProfiler::SCRIPTTHREADTYPE_EVENT;
	case ES_THREAD_INLINE_SCRIPT:
		return OpScopeProfiler::SCRIPTTHREADTYPE_INLINE_SCRIPT;
	case ES_THREAD_JAVASCRIPT_URL:
		return OpScopeProfiler::SCRIPTTHREADTYPE_JAVASCRIPT_URL;
	case ES_THREAD_HISTORY_NAVIGATION:
		return OpScopeProfiler::SCRIPTTHREADTYPE_HISTORY_NAVIGATION;
	case ES_THREAD_DEBUGGER_EVAL:
		return OpScopeProfiler::SCRIPTTHREADTYPE_DEBUGGER_EVAL;
	}
}

static OpScopeProfiler::ScriptType
ConvertScriptType(ES_ScriptType type)
{
	switch (type)
	{
	default:
		OP_ASSERT(!"Please add a new case!");
	case SCRIPT_TYPE_UNKNOWN:
		return OpScopeProfiler::SCRIPTTYPE_UNKNOWN;
	case SCRIPT_TYPE_LINKED:
		return OpScopeProfiler::SCRIPTTYPE_LINKED;
	case SCRIPT_TYPE_INLINE:
		return OpScopeProfiler::SCRIPTTYPE_INLINE;
	case SCRIPT_TYPE_GENERATED:
		return OpScopeProfiler::SCRIPTTYPE_GENERATED;
	case SCRIPT_TYPE_EVENT_HANDLER:
		return OpScopeProfiler::SCRIPTTYPE_EVENT_HANDLER;
	case SCRIPT_TYPE_EVAL:
		return OpScopeProfiler::SCRIPTTYPE_EVAL;
	case SCRIPT_TYPE_TIMEOUT:
		return OpScopeProfiler::SCRIPTTYPE_TIMEOUT;
	case SCRIPT_TYPE_DEBUGGER:
		return OpScopeProfiler::SCRIPTTYPE_DEBUGGER;
	case SCRIPT_TYPE_JAVASCRIPT_URL:
		return OpScopeProfiler::SCRIPTTYPE_URI;
	case SCRIPT_TYPE_USER_JAVASCRIPT:
	case SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY:
		return OpScopeProfiler::SCRIPTTYPE_USERJS;
	case SCRIPT_TYPE_BROWSER_JAVASCRIPT:
		return OpScopeProfiler::SCRIPTTYPE_BROWSERJS;
	case SCRIPT_TYPE_EXTENSION_JAVASCRIPT:
		return OpScopeProfiler::SCRIPTTYPE_EXTENSIONJS;
	}
}

static OpScopeProfiler::EventType
ConvertEventType(OpProbeEventType type)
{
	switch (type)
	{
	case PROBE_EVENT_STYLE_RECALCULATION:
		return OpScopeProfiler::EVENTTYPE_STYLE_RECALCULATION;
	case PROBE_EVENT_CSS_SELECTOR_MATCHING:
		return OpScopeProfiler::EVENTTYPE_CSS_SELECTOR_MATCHING;
	case PROBE_EVENT_SCRIPT_THREAD_EVALUATION:
		return OpScopeProfiler::EVENTTYPE_SCRIPT_THREAD_EVALUATION;
	case PROBE_EVENT_REFLOW:
		return OpScopeProfiler::EVENTTYPE_REFLOW;
	case PROBE_EVENT_PAINT:
		return OpScopeProfiler::EVENTTYPE_PAINT;
	case PROBE_EVENT_DOCUMENT_PARSING:
		return OpScopeProfiler::EVENTTYPE_DOCUMENT_PARSING;
	case PROBE_EVENT_CSS_PARSING:
		return OpScopeProfiler::EVENTTYPE_CSS_PARSING;
	case PROBE_EVENT_SCRIPT_COMPILATION:
		return OpScopeProfiler::EVENTTYPE_SCRIPT_COMPILATION;
	case PROBE_EVENT_LAYOUT:
		return OpScopeProfiler::EVENTTYPE_LAYOUT;
	default:
		return OpScopeProfiler::EVENTTYPE_GENERIC;
	}
}

static OpProbeEventType
ConvertEventType(OpScopeProfiler::EventType type)
{
	switch (type)
	{
	case OpScopeProfiler::EVENTTYPE_STYLE_RECALCULATION:
		return PROBE_EVENT_STYLE_RECALCULATION;
	case OpScopeProfiler::EVENTTYPE_CSS_SELECTOR_MATCHING:
		return PROBE_EVENT_CSS_SELECTOR_MATCHING;
	case OpScopeProfiler::EVENTTYPE_SCRIPT_THREAD_EVALUATION:
		return PROBE_EVENT_SCRIPT_THREAD_EVALUATION;
	case OpScopeProfiler::EVENTTYPE_REFLOW:
		return PROBE_EVENT_REFLOW;
	case OpScopeProfiler::EVENTTYPE_PAINT:
		return PROBE_EVENT_PAINT;
	case OpScopeProfiler::EVENTTYPE_DOCUMENT_PARSING:
		return PROBE_EVENT_DOCUMENT_PARSING;
	case OpScopeProfiler::EVENTTYPE_CSS_PARSING:
		return PROBE_EVENT_CSS_PARSING;
	case OpScopeProfiler::EVENTTYPE_SCRIPT_COMPILATION:
		return PROBE_EVENT_SCRIPT_COMPILATION;
	case OpScopeProfiler::EVENTTYPE_LAYOUT:
		return PROBE_EVENT_LAYOUT;
	default:
		return PROBE_EVENT_GENERIC;
	}
}


static OP_STATUS
ExportCssSelectorMatching(OpScopeProfiler::CssSelectorMatchingEvent &out, const OpProbeTimeline::Event &event)
{
	const OpProbeTimeline::Property *p = event.GetProperty("selector");

	return p ? out.SetSelector(p->GetString()) : OpStatus::OK;
}

static OP_STATUS
ExportScriptThreadEvaluation(OpScopeProfiler::ScriptThreadEvaluationEvent &out, const OpProbeTimeline::Event &event)
{
	const OpProbeTimeline::Property *type_prop = event.GetProperty("thread-type");
	const OpProbeTimeline::Property *event_prop = event.GetProperty("event");

	ES_ThreadType thread_type = type_prop ? static_cast<ES_ThreadType>(type_prop->GetInteger()) : ES_THREAD_COMMON;

	if (type_prop)
		out.SetScriptThreadType(ConvertScriptThreadType(thread_type));

	if (thread_type == ES_THREAD_EVENT && event_prop)
	{
		const uni_char *event_name = event_prop->GetString();

		if (event_name)
			RETURN_IF_ERROR(out.GetEventNameRef().Set(event_name));
	}

	return OpStatus::OK;
}

static OP_STATUS
ExportDocumentParsing(OpScopeProfiler::DocumentParsingEvent &out, const OpProbeTimeline::Event &event)
{
	const OpProbeTimeline::Property *p = event.GetProperty("url");

	return p ? out.SetUrl(p->GetString()) : OpStatus::OK;
}

static OP_STATUS
ExportCssParsing(OpScopeProfiler::CssParsingEvent &out, const OpProbeTimeline::Event &event)
{
	const OpProbeTimeline::Property *p = event.GetProperty("url");

	return p ? out.SetUrl(p->GetString()) : OpStatus::OK;
}

static OP_STATUS
ExportScriptCompilation(OpScopeProfiler::ScriptCompilationEvent &out, const OpProbeTimeline::Event &event)
{
	const OpProbeTimeline::Property *type_prop = event.GetProperty("type");
	const OpProbeTimeline::Property *url_prop = event.GetProperty("url");

	ES_ScriptType type = SCRIPT_TYPE_UNKNOWN;

	if (type_prop)
		type = static_cast<ES_ScriptType>(type_prop->GetUnsigned());

	out.SetScriptType(ConvertScriptType(type));

	if (url_prop)
		return out.SetUrl(url_prop->GetString());

	return OpStatus::OK;
}

static OP_STATUS
ExportPaint(OpScopeProfiler::PaintEvent &out, const OpProbeTimeline::Event &event)
{
	OpScopeProfiler::Area *area = OP_NEW(OpScopeProfiler::Area, ());
	RETURN_OOM_IF_NULL(area);

	out.SetArea(area);

	const OpProbeTimeline::Property *x = event.GetProperty("x");
	const OpProbeTimeline::Property *y = event.GetProperty("y");
	const OpProbeTimeline::Property *w = event.GetProperty("w");
	const OpProbeTimeline::Property *h = event.GetProperty("h");
	const OpProbeTimeline::Property *ox = event.GetProperty("ox");
	const OpProbeTimeline::Property *oy = event.GetProperty("oy");

	area->SetX(x ? x->GetInteger() : 0);
	area->SetY(y ? y->GetInteger() : 0);
	area->SetW(w ? w->GetInteger() : 0);
	area->SetH(h ? h->GetInteger() : 0);
	area->SetOx(ox ? ox->GetInteger() : 0);
	area->SetOy(oy ? oy->GetInteger() : 0);

	return OpStatus::OK;
}

OP_STATUS
OpScopeProfiler::ExportSession(Session &out, unsigned session_id) const
{
	InternalSession *session;
	RETURN_IF_ERROR(GetSession(session_id, session));

	out.SetSessionID(session_id);

	if (session->GetWindowID() != 0)
		out.SetWindowID(session->GetWindowID());

	FrameTimeline *timeline = session->FirstTimeline();

	while (timeline)
	{
		Timeline *t = out.GetTimelineListRef().AddNew();
		RETURN_OOM_IF_NULL(t);

		t->SetTimelineID(timeline->GetTimelineID());
		t->SetFrameID(timeline->GetFrameID());

		timeline = timeline->Suc();
	}

	return OpStatus::OK;
}

static OP_STATUS
UpdateInterval(OpScopeProfiler::Events &out, const OpProbeTimeline *timeline, const OpProbeTimeline::Event &event)
{
	OpScopeProfiler::Interval *interval = out.GetInterval();

	double s = timeline->GetRelativeTime(event.GetStart());
	double e = timeline->GetRelativeTime(event.GetEnd());

	// If this is the first Event, set the start and end of the interval.
	if (!interval)
	{
		OpScopeProfiler::Interval *interval = OP_NEW(OpScopeProfiler::Interval, ());
		RETURN_OOM_IF_NULL(interval);

		out.SetInterval(interval);

		interval->SetStart(s);
		interval->SetEnd(e);
	}
	else
	{
		if (s < interval->GetStart())
			interval->SetStart(s);
		if (e > interval->GetEnd())
			interval->SetEnd(e);
	}

	return OpStatus::OK;
}

static OP_STATUS
ExportEventTypeField(OpScopeProfiler::Event &out, const OpProbeTimeline::Event &event)
{
	switch (event.GetType())
	{
	case PROBE_EVENT_CSS_SELECTOR_MATCHING:
		out.SetCssSelectorMatching(OP_NEW(OpScopeProfiler::CssSelectorMatchingEvent, ()));
		RETURN_OOM_IF_NULL(out.GetCssSelectorMatching());
		RETURN_IF_ERROR(ExportCssSelectorMatching(*out.GetCssSelectorMatching(), event));
		break;
	case PROBE_EVENT_SCRIPT_THREAD_EVALUATION:
		out.SetScriptThreadEvaluation(OP_NEW(OpScopeProfiler::ScriptThreadEvaluationEvent, ()));
		RETURN_OOM_IF_NULL(out.GetScriptThreadEvaluation());
		RETURN_IF_ERROR(ExportScriptThreadEvaluation(*out.GetScriptThreadEvaluation(), event));
		break;
	case PROBE_EVENT_PAINT:
		out.SetPaint(OP_NEW(OpScopeProfiler::PaintEvent, ()));
		RETURN_OOM_IF_NULL(out.GetPaint());
		RETURN_IF_ERROR(ExportPaint(*out.GetPaint(), event));
		break;
	case PROBE_EVENT_DOCUMENT_PARSING:
		out.SetDocumentParsing(OP_NEW(OpScopeProfiler::DocumentParsingEvent, ()));
		RETURN_OOM_IF_NULL(out.GetDocumentParsing());
		RETURN_IF_ERROR(ExportDocumentParsing(*out.GetDocumentParsing(), event));
		break;
	case PROBE_EVENT_CSS_PARSING:
		out.SetCssParsing(OP_NEW(OpScopeProfiler::CssParsingEvent, ()));
		RETURN_OOM_IF_NULL(out.GetCssParsing());
		RETURN_IF_ERROR(ExportCssParsing(*out.GetCssParsing(), event));
		break;
	case PROBE_EVENT_SCRIPT_COMPILATION:
		out.SetScriptCompilation(OP_NEW(OpScopeProfiler::ScriptCompilationEvent, ()));
		RETURN_OOM_IF_NULL(out.GetScriptCompilation());
		RETURN_IF_ERROR(ExportScriptCompilation(*out.GetScriptCompilation(), event));
		break;
	}

	return OpStatus::OK;
}

static OP_STATUS
ExportInterval(OpScopeProfiler::Event &out, const OpProbeTimeline *timeline, OpProbeTimeline::Event &event)
{
	OpAutoPtr<OpScopeProfiler::Interval> interval(OP_NEW(OpScopeProfiler::Interval, ()));
	RETURN_OOM_IF_NULL(interval.get());

	interval->SetStart(timeline->GetRelativeTime(event.GetStart()));
	interval->SetEnd(timeline->GetRelativeTime(event.GetEnd()));

	out.SetInterval(interval.release());

	return OpStatus::OK;
}

static OP_STATUS
ExportEvent(OpScopeProfiler::Event &out, const OpProbeTimeline *timeline, const OpProbeTimeline::Iterator &iterator)
{
	OpProbeTimeline::Event *event = iterator.GetEvent();

	out.SetType(ConvertEventType(event->GetType()));
	out.SetOverhead(event->GetOverhead());
	out.SetTime(event->GetTime());
	out.SetHits(event->GetHits());

	RETURN_IF_ERROR(ExportInterval(out, timeline, *event));

	if (timeline->IsAggregationFinished())
	{
		out.SetAggregatedTime(event->GetAggregatedTime());
		out.SetAggregatedOverhead(event->GetAggregatedOverhead());
	}

	out.SetEventID(event->GetId());

	if (iterator.GetParent() && !timeline->IsRoot(iterator.GetParent()))
		out.SetParentEventID(iterator.GetParent()->GetId());

	out.SetChildCount(event->CountChildren());

	return ExportEventTypeField(out, *event);
}

static void
ZeroEvent(OpScopeProfiler::Event &out)
{
	out.SetTime(0);
	out.SetOverhead(0);
	out.SetHits(0);
}

/* virtual */ OP_STATUS
OpScopeProfiler::DoStartProfiler(const StartProfilerArg &in, SessionID &out)
{
	SCOPE_BADREQUEST_IF_FALSE(current_session == NULL, this,
		UNI_L("Profiling already in progress."));

	Window *window;
	RETURN_IF_ERROR(GetWindow(in.GetWindowID(), window));

	BOOL start_on_load = (in.GetStartMode() == StartProfilerArg::STARTMODE_URL);

	OpAutoPtr<InternalSession> session(OP_NEW(InternalSession, (this, in.GetWindowID(), start_on_load)));
	RETURN_OOM_IF_NULL(session.get());

	unsigned session_id;
	RETURN_IF_ERROR(GetSessionID(session.get(), session_id));
	out.SetSessionID(session_id);

	current_session = session.release();

	if (start_on_load)
		return OpStatus::OK; // Starting takes place later.

	// Start immediately.
	return StartProfiling();
}

/* virtual */ OP_STATUS
OpScopeProfiler::DoReleaseSession(const ReleaseSessionArg &in)
{
	// If no session ID is specified, the client wants to remove all sessions.
	if (!in.HasSessionID())
	{
		session_ids.DeleteAll();
		return OpStatus::OK;
	}

	// Session ID was specified. Delete it, if present.

	InternalSession *session;

	SCOPE_BADREQUEST_IF_ERROR(GetSession(in.GetSessionID(), session), this,
		UNI_L("Session ID does not exist."));

	SCOPE_BADREQUEST_IF_FALSE(session != current_session, this,
		UNI_L("Session must be stopped first."));

	return session_ids.DeleteID(in.GetSessionID());
}

/* virtual */ OP_STATUS
OpScopeProfiler::DoStopProfiler(const StopProfilerArg &in, unsigned int async_tag)
{
	SCOPE_BADREQUEST_IF_NULL(current_session, this,
		UNI_L("Profiling not in progress."));

	InternalSession *session = current_session;

	unsigned session_id;
	RETURN_IF_ERROR(GetSessionID(session, session_id));

	SCOPE_BADREQUEST_IF_FALSE(session_id == in.GetSessionID(), this,
		UNI_L("No Session with that ID"));

	StopProfiling();

	// The stop response is sent when aggregation is done.
	AggregationTask *task = OP_NEW(AggregationTask, (session_id, async_tag));
	RETURN_OOM_IF_NULL(task);

	// TODO: Send async error. Not possible at time of writing. CORE-37340
	// adds support for this.
	RETURN_IF_MEMORY_ERROR(PerformTask(task));

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
OpScopeProfiler::DoGetEvents(const GetEventsArg &in, unsigned async_tag)
{
	unsigned sid = in.GetSessionID();
	unsigned tid = in.GetTimelineID();

	FrameTimeline *timeline;
	RETURN_IF_ERROR(GetTimeline(sid, tid, timeline));

	IterationOptions opt(timeline, in);

	OpAutoPtr<GetEventsTask> task;

	switch (in.GetMode())
	{
	case GetEventsArg::MODE_ALL:
		task.reset(OP_NEW(GetAllEventsTask, (tid, sid, async_tag, opt)));
		break;
	case GetEventsArg::MODE_REDUCE_UNIQUE_TYPES:
		task.reset(OP_NEW(ReduceUniqueTypesTask, (tid, sid, async_tag, opt)));
		break;
	case GetEventsArg::MODE_REDUCE_UNIQUE_EVENTS:
		task.reset(OP_NEW(ReduceUniqueEventsTask, (tid, sid, async_tag, opt)));
		break;
	case GetEventsArg::MODE_REDUCE_ALL:
		task.reset(OP_NEW(ReduceAllEventsTask, (tid, sid, async_tag, opt)));
		break;
	default:
		OP_ASSERT(!"Please add a new case handler.");
		return OpStatus::ERR;
	}

	RETURN_OOM_IF_NULL(task.get());
	RETURN_IF_ERROR(task->Construct(this));

	return PerformTask(task.release());
}

BOOL
OpScopeProfiler::AcceptDocumentManager(DocumentManager *docman)
{
	if (!current_session || !docman)
		return FALSE;

	return (current_session->GetWindowID() == docman->GetWindow()->Id());
}

OP_STATUS
OpScopeProfiler::StartProfiling()
{
	if (!current_session)
		return OpStatus::ERR;

	unsigned wid = current_session->GetWindowID();

	Window *window;
	RETURN_IF_ERROR(GetWindow(wid, window));

	OP_STATUS status = window->StartProfiling(current_session);

	if (OpStatus::IsError(status))
	{
		// Clean up objects added earlier.
		OpStatus::Ignore(session_ids.DeleteObject(current_session));
		current_session = NULL;
	}
	else
		current_session->OnStart();

	return status;
}

void
OpScopeProfiler::StopProfiling()
{
	InternalSession *session = current_session;
	current_session = NULL;

	if (session)
	{
		session->OnStop();

		Window *window = g_windowManager->GetWindow(session->GetWindowID());

		if (window)
			window->StopProfiling();

		// If the Window has closed since we started, profiling was implicitly
		// stopped, so no need to stop explicitly.
	}
}

OP_STATUS
OpScopeProfiler::GetSession(unsigned id, InternalSession *&session) const
{
	return session_ids.GetObject(id, session);
}

OP_STATUS
OpScopeProfiler::GetSessionID(InternalSession *session, unsigned &id)
{
	return session_ids.GetID(session, id);
}

OP_STATUS
OpScopeProfiler::GetTimeline(unsigned session_id, unsigned timeline_id, FrameTimeline *&timeline)
{
	InternalSession *session;
	RETURN_IF_ERROR(GetSession(session_id, session));

	timeline = session->GetTimeline(timeline_id);

	SCOPE_BADREQUEST_IF_NULL1(timeline, this,
		UNI_L("Invalid timeline ID: %i"), timeline_id);

	return OpStatus::OK;
}

OP_STATUS
OpScopeProfiler::GetFrameID(DocumentManager *docman, unsigned &id)
{
	return frame_ids->GetID(docman, id);
}

OP_STATUS
OpScopeProfiler::GetWindow(unsigned id, Window *&window)
{
	window = g_windowManager->GetWindow(id);

	SCOPE_BADREQUEST_IF_NULL1(window, this, UNI_L("No Window with id: %i"), id);

	return OpStatus::OK;
}

OP_STATUS
OpScopeProfiler::PerformTask(AsyncTask *task)
{
	BOOL finished;
	OP_STATUS status = task->Perform(this, finished);

	if (finished)
		OP_DELETE(task);
	else if (!async_tasks.HasLink(task))
		task->Into(&async_tasks);

	return status;
}


OpScopeProfiler::InternalSession::InternalSession(OpScopeProfiler *profiler, unsigned window_id, BOOL start_on_load)
	: profiler(profiler)
	, start_on_load(start_on_load)
	, window_id(window_id)
	, current_timeline(NULL)
	, os_process_time_start(0)
	, os_process_time_stop(0)
	, os_process_status(OpStatus::OK)
{
}

/* virtual */ OpProbeTimeline *
OpScopeProfiler::InternalSession::AddTimeline(DocumentManager *docman)
{
	unsigned frame_id;

	if (OpStatus::IsError(profiler->GetFrameID(docman, frame_id)))
		return NULL;

	unsigned timeline_id = profiler->NextTimelineID();

	OpAutoPtr<FrameTimeline> timeline(OP_NEW(FrameTimeline, (timeline_id, frame_id)));

	if (timeline.get() == NULL)
		return NULL;

	if (OpStatus::IsMemoryError(timeline->Construct()))
		return NULL;

	AddTimeline(timeline.get());

	return timeline.release();
}

BOOL
OpScopeProfiler::InternalSession::GetStartOnLoad() const
{
	return start_on_load;
}

void
OpScopeProfiler::InternalSession::SetStartOnLoad(BOOL start_on_load)
{
	this->start_on_load = start_on_load;
}

unsigned
OpScopeProfiler::InternalSession::GetWindowID() const
{
	return window_id;
}

OP_STATUS
OpScopeProfiler::InternalSession::Aggregate(int iterations)
{
	if (IsAggregationFinished())
		return OpStatus::OK;

	// If 'current_timeline' is NULL, then Aggregate has never been called
	// before. Set to first element.
	if (current_timeline == NULL)
		current_timeline = frame_timelines.First();

	// If there is nothing more to aggregate on the current timeline, then move
	// to next. But do not move to 'NULL' if that's the value of Suc(). That
	// would restart the aggregation.
	if (current_timeline->IsAggregationFinished() && current_timeline->Suc())
		current_timeline = current_timeline->Suc();

	return current_timeline->Aggregate(iterations);
}

BOOL
OpScopeProfiler::InternalSession::IsAggregationFinished()
{
	if (frame_timelines.First() == NULL)
		return TRUE;

	if (current_timeline == frame_timelines.Last())
		return current_timeline->IsAggregationFinished();

	return FALSE;
}

void
OpScopeProfiler::InternalSession::AddTimeline(FrameTimeline *timeline)
{
	timeline->Into(&frame_timelines);
}

OpScopeProfiler::FrameTimeline *
OpScopeProfiler::InternalSession::GetTimeline(unsigned timeline_id)
{
	FrameTimeline *timeline = frame_timelines.First();

	while (timeline)
	{
		if (timeline->GetTimelineID() == timeline_id)
			return timeline;

		timeline = timeline->Suc();
	}

	return NULL;
}

OpScopeProfiler::FrameTimeline *
OpScopeProfiler::InternalSession::FirstTimeline()
{
	return frame_timelines.First();
}

void
OpScopeProfiler::InternalSession::OnStart()
{
	os_process_status = g_op_system_info->GetProcessTime(&os_process_time_start);
}

void
OpScopeProfiler::InternalSession::OnStop()
{
	g_op_system_info->GetProcessTime(&os_process_time_stop);
}

OpScopeProfiler::AsyncTask::AsyncTask(unsigned session_id, unsigned async_tag)
	: session_id(session_id)
	, async_tag(async_tag)
{
}

OpScopeProfiler::AsyncTask::~AsyncTask()
{
	if (InList())
		Out();
}

BOOL
OpScopeProfiler::AsyncTask::PostMessage()
{
	MH_PARAM_1 par1 = static_cast<MH_PARAM_1>(GetAsyncTag());

	return g_main_message_handler->PostMessage(MSG_SCOPE_PROCESS_TIMELINE, par1, 0);
}

OpScopeProfiler::AggregationTask::AggregationTask(unsigned session_id, unsigned async_tag)
	: AsyncTask(session_id, async_tag)
{
}

/* virtual */ OP_STATUS
OpScopeProfiler::AggregationTask::Perform(OpScopeProfiler *profiler, BOOL &finished)
{
	finished = TRUE;

	InternalSession *session;
	OP_STATUS status = profiler->GetSession(GetSessionID(), session);

	if (OpStatus::IsSuccess(status))
	{
		status = session->Aggregate(SCOPE_TIMELINE_PROCESSING_ITERATIONS);

		if (OpStatus::IsSuccess(status))
			finished = session->IsAggregationFinished();
	}

	// If not finished, post a message to resume later.
	if (!finished && !PostMessage())
	{
		finished = TRUE;
		status = OpStatus::ERR_NO_MEMORY;
	}

	if (finished)
	{
		Session out;
		RETURN_IF_ERROR(profiler->ExportSession(out, GetSessionID()));
		RETURN_IF_ERROR(profiler->SendStopProfiler(out, GetAsyncTag()));
	}

	return status;
}

OpScopeProfiler::ProbeEventFilter::ProbeEventFilter()
{
	SetAll(TRUE);
}

void
OpScopeProfiler::ProbeEventFilter::SetAll(BOOL value)
{
	for (unsigned i = 0; i < PROBE_EVENT_COUNT; ++i)
		filter[i] = value;
}
void
OpScopeProfiler::ProbeEventFilter::SetInclude(OpProbeEventType type, BOOL value)
{
	filter[type] = value;
}

BOOL
OpScopeProfiler::ProbeEventFilter::Includes(const OpProbeTimeline::Event &event) const
{
	return filter[event.GetType()];
}

OpScopeProfiler::IterationOptions::IterationOptions(FrameTimeline *timeline, const GetEventsArg &arg)
	: root(timeline->GetRoot())
	, max_depth(-1)
{
	// Set 'root'.
	if (arg.HasEventID())
	{
		OpProbeTimeline::Event *e;

		if (OpStatus::IsSuccess(timeline->FindEvent(arg.GetEventID(), e)))
			root = e;
	}

	// Set 'max_depth'.
	if (arg.HasMaxDepth())
		max_depth = arg.GetMaxDepth();

	// Set 'filter'.
	if (arg.GetEventTypeListCount() > 0)
	{
		filter.SetAll(FALSE);

		for (unsigned i = 0; i < arg.GetEventTypeListCount(); ++i)
		{
			OpProbeEventType type = ConvertEventType(arg.GetEventTypeListItem(i));

			filter.SetInclude(type, TRUE);
		}
	}

	// Set 'interval'.
	if (arg.HasInterval())
		interval = *arg.GetInterval();
}

OpScopeProfiler::GetEventsTask::GetEventsTask(unsigned timeline_id, unsigned session_id, unsigned async_tag, const IterationOptions &options)
	: AsyncTask(session_id, async_tag)
	, timeline_id(timeline_id)
	, iterator(options.GetRoot(), options.GetMaxDepth())
	, options(options)
{
}

/* virtual */ OP_STATUS
OpScopeProfiler::GetEventsTask::Perform(OpScopeProfiler *profiler, BOOL &finished)
{
	finished = TRUE;

	FrameTimeline *timeline;
	OP_STATUS status = profiler->GetTimeline(GetSessionID(), timeline_id, timeline);

	unsigned i = 0;

	while (OpStatus::IsSuccess(status) && i++ < SCOPE_TIMELINE_PROCESSING_ITERATIONS)
	{
		finished = !iterator.Next();

		if (finished)
			break;

		if (IsExcluded(timeline, *iterator.GetEvent()))
			continue;

		status = OnMatchingEvent(timeline, iterator);
	}

	if (OpStatus::IsError(status))
		finished = TRUE;

	// If not finished, post a message to resume later.
	if (!finished && !PostMessage())
	{
		finished = TRUE;
		status = OpStatus::ERR_NO_MEMORY;
	}

	if (finished)
		return profiler->SendGetEvents(events, GetAsyncTag());

	return status;
}

BOOL
OpScopeProfiler::GetEventsTask::IsExcluded(const OpProbeTimeline *timeline, const OpProbeTimeline::Event &event) const
{
	// Check type filter.
	if (!options.GetFilter().Includes(event))
		return TRUE;

	// Check interval.
	const Interval &i = options.GetInterval();

	double s = timeline->GetRelativeTime(event.GetStart());
	double e = timeline->GetRelativeTime(event.GetEnd());

	BOOL left = (!i.HasStart() || s >= i.GetStart() && s <= i.GetEnd());
	BOOL right = (!i.HasEnd() || e >= i.GetStart() && e <= i.GetEnd());
	BOOL interval = (left || right);

	return !interval;
}

OP_STATUS
OpScopeProfiler::GetEventsTask::AccumulateEventData(const OpProbeTimeline *timeline, const OpProbeTimeline::Event &event, Event &out)
{
	out.SetOverhead(out.GetOverhead() + event.GetOverhead());
	out.SetTime(out.GetTime() + event.GetTime());
	out.SetHits(out.GetHits() + event.GetHits());

	return UpdateInterval(events, timeline, event);
}

OpScopeProfiler::GetAllEventsTask::GetAllEventsTask(unsigned timeline_id, unsigned session_id, unsigned async_tag, const IterationOptions &options)
	: GetEventsTask(timeline_id, session_id, async_tag, options)
{
}

/* virtual */ OP_STATUS
OpScopeProfiler::GetAllEventsTask::OnMatchingEvent(OpProbeTimeline *timeline, const OpProbeTimeline::Iterator &iterator)
{
	Event *e = events.GetEventListRef().AddNew();
	RETURN_OOM_IF_NULL(e);

	RETURN_IF_ERROR(ExportEvent(*e, timeline, iterator));
	RETURN_IF_ERROR(UpdateInterval(events, timeline, *iterator.GetEvent()));

	return OpStatus::OK;
}

OpScopeProfiler::ReduceUniqueTypesTask::ReduceUniqueTypesTask(unsigned timeline_id, unsigned session_id, unsigned async_tag, const IterationOptions &options)
	: GetEventsTask(timeline_id, session_id, async_tag, options)
{
	for (unsigned i = 0; i < PROBE_EVENT_COUNT; ++i)
		uniqueTypes[i] = NULL;
}

/* virtual */ OP_STATUS
OpScopeProfiler::ReduceUniqueTypesTask::Construct(OpScopeProfiler *profiler)
{
	// Initialize types array.
	for (unsigned i = 0; i < PROBE_EVENT_COUNT; ++i)
	{
		OpProbeEventType type = GetValidType(static_cast<OpProbeEventType>(i));

		if (uniqueTypes[type])
			continue; // Already added.

		uniqueTypes[type] = events.GetEventListRef().AddNew();
		RETURN_OOM_IF_NULL(uniqueTypes[type]);

		uniqueTypes[type]->SetType(ConvertEventType(type));
		ZeroEvent(*uniqueTypes[type]);
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
OpScopeProfiler::ReduceUniqueTypesTask::OnMatchingEvent(OpProbeTimeline *timeline, const OpProbeTimeline::Iterator &iterator)
{
	OpProbeTimeline::Event *event = iterator.GetEvent();

	OpProbeEventType type = GetValidType(event->GetType());

	return AccumulateEventData(timeline, *event, *uniqueTypes[type]);
}

/* static */ OpProbeEventType
OpScopeProfiler::ReduceUniqueTypesTask::GetValidType(OpProbeEventType type)
{
	// At first, it may look like we will always just return 'type' here,
	// but this is not the case. If the incoming 'type' does not have an
	// external representation, 'GENERIC' will be returned (even if the
	// incoming type was not 'GENERIC').
	return ConvertEventType(ConvertEventType(type));
}

OpScopeProfiler::ReduceUniqueEventsTask::ReduceUniqueEventsTask(unsigned timeline_id, unsigned session_id, unsigned async_tag, const IterationOptions &options)
	: GetEventsTask(timeline_id, session_id, async_tag, options)
{
}

/* virtual */ OP_STATUS
OpScopeProfiler::ReduceUniqueEventsTask::OnMatchingEvent(OpProbeTimeline *timeline, const OpProbeTimeline::Iterator &iterator)
{
	OpProbeTimeline::Event *event = iterator.GetEvent();

	const uni_char *str = ToString(timeline, *event);

	if (!str)
		str = UNI_L("");

	OpProbeEventType type = event->GetType();

	Event *out = NULL;

	if (OpStatus::IsError(uniqueEvents[type].GetData(str, &out)))
	{
		out = events.GetEventListRef().AddNew();
		RETURN_OOM_IF_NULL(out);
		RETURN_IF_ERROR(uniqueEvents[type].Add(str, out));
		ZeroEvent(*out);

		out->SetType(ConvertEventType(event->GetType()));
		RETURN_IF_MEMORY_ERROR(ExportEventTypeField(*out, *event));
	}

	OP_ASSERT(out);
	return AccumulateEventData(timeline, *event, *out);
}

/* static */ const uni_char *
OpScopeProfiler::ReduceUniqueEventsTask::ToString(OpProbeTimeline *timeline, const OpProbeTimeline::Event &event)
{
	switch (event.GetType())
	{
	default:
		OP_ASSERT(!"Please add a new case.");
	case PROBE_EVENT_GENERIC:
	case PROBE_EVENT_ROOT:
	case PROBE_EVENT_STYLE_RECALCULATION:
	case PROBE_EVENT_REFLOW:
	case PROBE_EVENT_LAYOUT:
		return NULL;
	case PROBE_EVENT_PAINT:
		return JoinProperties(timeline, event);
	case PROBE_EVENT_CSS_SELECTOR_MATCHING:
		return GetStringProperty("selector", event);
	case PROBE_EVENT_SCRIPT_THREAD_EVALUATION:
		{
			const OpProbeTimeline::Property *event_prop = event.GetProperty("event");

			// If this is an event thread, use the event name as the string.
			// Otherwise, use a string representation of the thread type.
			if (event_prop)
				return event_prop->GetString();
			else
				return JoinProperties(timeline, event);
		}
	case PROBE_EVENT_DOCUMENT_PARSING:
	case PROBE_EVENT_CSS_PARSING:
		return GetStringProperty("url", event);
	case PROBE_EVENT_SCRIPT_COMPILATION:
		{
			const OpProbeTimeline::Property *type_prop = event.GetProperty("type");
			const OpProbeTimeline::Property *url_prop = event.GetProperty("url");

			if (url_prop)
				return url_prop->GetString();
			else if (type_prop)
				return JoinProperties(timeline, event);
			else
				return NULL;
		}
	}
}

/* static */ const uni_char *
OpScopeProfiler::ReduceUniqueEventsTask::JoinProperties(OpProbeTimeline *timeline, const OpProbeTimeline::Event &event)
{
	unsigned count = event.GetPropertyCount();

	if (count == 0)
		return NULL;

	TempBuffer buf;

	for (unsigned i = 0; i < count; ++i)
	{
		const OpProbeTimeline::Property *p = event.GetProperty(i);

		switch (p->GetType())
		{
		default:
		case OpProbeTimeline::PROPERTY_UNDEFINED:
			return NULL;
		case OpProbeTimeline::PROPERTY_INTEGER:
			RETURN_VALUE_IF_ERROR(buf.AppendLong(p->GetInteger()), NULL);
			break;
		case OpProbeTimeline::PROPERTY_STRING:
			RETURN_VALUE_IF_ERROR(buf.Append(p->GetString()), NULL);
			break;
		}

		// Append a separator between elements, but not after the last element.
		if (i < count - 1)
			RETURN_VALUE_IF_ERROR(buf.Append(","), NULL);
	}

	const uni_char *ret;
	RETURN_VALUE_IF_ERROR(timeline->GetString(buf.GetStorage(), ret), NULL);

	return ret;
}

/* static */ const uni_char *
OpScopeProfiler::ReduceUniqueEventsTask::GetStringProperty(const char *name, const OpProbeTimeline::Event &event)
{
	const OpProbeTimeline::Property *p = event.GetProperty(name);

	if (!p || p->GetType() != OpProbeTimeline::PROPERTY_STRING)
		return NULL;

	return p->GetString();
}

OpScopeProfiler::ReduceAllEventsTask::ReduceAllEventsTask(unsigned timeline_id, unsigned session_id, unsigned async_tag, const IterationOptions &options)
	: GetEventsTask(timeline_id, session_id, async_tag, options)
	, all(NULL)
{
}

/* virtual */ OP_STATUS
OpScopeProfiler::ReduceAllEventsTask::Construct(OpScopeProfiler *profiler)
{
	all = events.GetEventListRef().AddNew();
	RETURN_OOM_IF_NULL(all);

	ZeroEvent(*all);
	all->SetType(EVENTTYPE_GENERIC);

	InternalSession *session;
	RETURN_IF_ERROR(profiler->GetSession(GetSessionID(), session));

#ifdef SELFTEST
	// Add the second event (total execution time for process) if supported.
	if (session->IsProcessTimeSupported() == OpStatus::OK)
	{
		Event *process = events.GetEventListRef().AddNew();
		RETURN_OOM_IF_NULL(process);
		ZeroEvent(*process);
		process->SetType(EVENTTYPE_PROCESS);
		process->SetTime(session->GetProcessTime());
	}
#endif // SELFTEST

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
OpScopeProfiler::ReduceAllEventsTask::OnMatchingEvent(OpProbeTimeline *timeline, const OpProbeTimeline::Iterator &iterator)
{
	return AccumulateEventData(timeline, *iterator.GetEvent(), *all);
}

#endif // SCOPE_PROFILER
