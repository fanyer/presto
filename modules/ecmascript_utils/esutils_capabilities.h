/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESUTILS_CAPABILITIES_H
#define ES_UTILS_ESUTILS_CAPABILITIES_H

/* ES_AsyncInterface::GetSlot and ES_AsyncInterface::SetSlot are actually
   asynchronous, that is, they will never be successfully completed
   immediately, will never call the callback immediately (before the call
   to GetSlot or SetSlot returns) and will handle properties that can
   block ECMAScript execution correctly.  New in filofax_1_beta_2. */
#define ESU_CAP_ASYNCHRONOUS_SLOT_OPERATIONS

/* The class ES_Environment exists.  New in filofax_1_beta_2. */
#define ESU_CAP_ENVIRONMENT_SUPPORT

/* The class ES_EmptyThread exists.  New in filofax_1_beta_4. */
#define ESU_CAP_EMPTYTHREAD

/* The function ES_Thread::SetCallable takes two extra (optional)
   arguments (callable_argc and callable_argv). */
#define ESU_CAP_TIMEOUTARGS

/* The function ES_AsyncInterface::CallFunction is supported. */
#define ESU_CAP_CALLFUNCTION

/* Exception propagation is supported.  This includes a new ES_AsyncStatus
   code (ES_ASYNC_EXCEPTION) and a new ES_AsyncInterface function
   (ES_AsyncInterface::SetWantsExceptions). */
#define ESU_CAP_EXCEPTION_PROPAGATION

/* The last thread started by ES_AsyncInterface can be retrieved
   using the function ES_AsyncInterface::GetThread. */
#define ESU_CAP_LAST_STARTED_THREAD

/* The function ES_ThreadScheduler::RemoveThreads takes a second (optional)
   argument "final" which means the scheduler will never be reactivated
   again. */
#define ESU_CAP_REMOVETHREADS_FINAL

/* The functions ES_AsyncInterface::{Get,Set}Slot supports an extra
   (optional) argument "interrupt_thread". */
#define ESU_CAP_SLOT_INTERRUPT_THREAD

/* The function ES_Environment::Create takes an extra argument
   "ignore_prefs" that makes it possible to create an environment even
   if ECMAScript is disabled in preferences. */
#define ESU_CAP_ENVIRONMENT_IGNORE_PREFS

/* The class ES_Utils exists and has a function Initialize that needs to
   be called when core is initialized. */
#define ESU_CAP_INITIALIZE

/* The function ES_JavascriptURLThread::GetURL is supported. */
#define ESU_CAP_JAVASCRIPT_URL_GETURL

/* The function ES_ThreadScheduler::AddTerminatingAction takes and optional
   'interrupt_thread' argument. */
#define ESU_CAP_TERMINATE_INTERRUPT_THREAD

/* The function ES_JavascriptURLThread::Load exists. */
#define ESU_CAP_JAVASCRIPT_URL_LOAD

/* Threads have the flags IsUserRequested and IsShiftClick.
   ES_AsyncInterface also has functions for setting those flags on
   created threads. */
#define ESU_CAP_THREAD_FLAGS

/* ES_ThreadScheduler::CancelThread takes an extra argument that
   decides whether a timeout thread or an interval thread should be
   cancelled. */
#define ESU_CAP_CANCELTIMEOUT_INTERVAL

/* ES_Thread::Pause is supported. */
#define ESU_CAP_ESTHREAD_PAUSE

/* ES_OpenURLAction::ES_OpenURLAction supports user_auto_reload
   argument. */
#define ESU_CAP_USER_AUTO_RELOAD

/* ES_ThreadInfo::data::event contains 'is_window_event' field. */
#define ESU_CAP_IS_WINDOW_EVENT

/* ES_LoadManager::CloseDocument is fixed so it behaves sanely when called
   "from" an inline script in the same document. */
#define ESU_CAP_FIXED_CLOSEDOCUMENT

/* ES_SyncInterface operations has 'want_string_result' parameter. */
#define ESU_CAP_SYNCIF_WANT_STRING_RESULT

/* ES_LoadManager::IsGenerationIdle is supported. */
#define ESU_CAP_ESLOADMAN_ISGENERATIONIDLE

/* ES_OpenURLAction::SetReloadFlags is supported. */
#define ESU_CAP_OPENURLACTION_SETRELOADFLAGS

/* ES_ThreadScheduler::ResumeIfNeeded is supported. */
#define ESU_CAP_ESTHREADSCHEDULER_RESUMEIFNEEDED

/* Debugger stop types are enumerated, and can be accessed by
   ES_EngineDebugBackend::SetStopAt and ES_EngineDebugBackend::GetStopAt. */
#define ESU_CAP_HAS_STOP_TYPE_ENUM

/* ES_TimeoutThread::{Get,Set}IntervalCount are supported. */
#define ESU_CAP_ESTIMEOUTTHREAD_INTERVALCOUNT

/* Rename ES_DebugStackFrame::script_no to script_guid */
#define ESU_CAP_DISAMBIGUATE_SCRIPT_ID

/* ES_EngineDebugBackend::Get{Object,Runtime}ById are supported. */
#define ESU_CAP_RUNTIME_AND_OBJECT_ID_INTERFACE

/* ES_(Engine)?DebugFrontend::GetObjectId is supported. */
#define ESU_CAP_OBJECT_LOOKUP_ID

/* ES_DebugStackFrame::this_obj is supported. */
#define ESU_CAP_THIS_OBJECT_IN_STACK_FRAME

/* ES_ThreadScheduler::MigrateThread is supported. */
#define ESU_CAP_MIGRATE_THREAD

/* ES_Thread and ES_ThreadInfo keep track of if the thread has tried
   to load a url or not. */
#define ESU_CAP_HAS_LOADED_URL

/* ES_SyncInterface supports the "direct_execution" flag in the various *Data structures. */
#define ESU_CAP_SYNCIF_DIRECT_EXECUTION

/* ES_LoadManager takes care of firing load events to external scripts after they finish executing. */
#define ESU_CAP_FIRES_ONLOAD_EVENT_AFTER_SCRIPT

/* ES_EngineDebugBackend::RuntimeCreated implemented */
#define ESU_CAP_IMPLEMENTS_RUNTIMECREATED

/* ES_LoadManager takes care of firing readystatechange events to external scripts after they finish executing. */
#define ESU_CAP_FIRES_ONREADYSTATECHANGE_AFTER_SCRIPT

/* ES_ReplaceDocumentAction can let the replacing document inherit design mode from the replaced document. */
#define ESU_CAP_INHERIT_DESIGN_MODE

/* ES_Thread::HasDebugInfo allows for checking if a thread has debugging information. */
#define ESU_CAP_THREAD_DEBUG_INFO

/* ES_DebugFrontend::ConstructEngineBackend and ES_EngineDebugBackend::Construct takes and extra parameter
   of the type ES_DebugWindowAccepter */
#define ESU_CAP_DEBUGGER_WINDOW_FILTERING

/* ES_DebugFrontend::GetRuntimeId exists */
#define ESU_CAP_GET_RUNTIME_ID

/* ecmascript/ecmascript_types.h/ES_ErrorInfo::error_length is not referenced anymore; Instead error_length() function is used. */
#define ESU_CAP_NOMORE_ERRORINFO_ERRORLENGTH

/* Sets interrupt pointers between threads in different documents and has an ES_LoadManager::SupportsWrite method */
#define ESU_CAP_CROSSDOCUMENT_INTERRUPTS

/* ES_DebugFrontend::FreeObjects exists */
#define ESU_CAP_FREE_DEBUG_OBJECTS

/* Reporting of parser errors for internal Evals calls. Adds ES_DebugFrontend::EvalError and ES_AsyncCallback::HandleError. Used by scope */
#define ESU_CAP_EVAL_ERROR_CALLBACK

#endif // ES_UTILS_ESUTILS_CAPABILITIES_H
