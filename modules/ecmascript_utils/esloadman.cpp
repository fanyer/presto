/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esloadman.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esutils.h"

#include "modules/doc/frm_doc.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/source_position_attr.h"
#include "modules/logdoc/html5parser.h"

/** A thread listener, used to get notifications about the generating
    thread finishing or being cancelled. */
class ES_DocumentGenerationListener
	: public ES_ThreadListener
{
public:
	ES_DocumentGenerationListener(ES_LoadManager *load_manager);
	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

private:
	ES_LoadManager *load_manager;
};

#ifdef SCRIPTS_IN_XML_HACK

/** A thread implementation only used to block execution while waiting for an
    external script to load in XML documents. */
class ES_ExternalInlineScriptThread
	: public ES_Thread
{
public:
	ES_ExternalInlineScriptThread(ES_LoadManager::ScriptElm *script_elm);

	virtual OP_STATUS EvaluateThread();

protected:
	virtual OP_STATUS Signal(ES_ThreadSignal signal);

	ES_LoadManager::ScriptElm *script_elm;
};

#endif // SCRIPTS_IN_XML_HACK

/* --- ES_DocumentGenerationListener implementation ------------------ */

ES_DocumentGenerationListener::ES_DocumentGenerationListener(ES_LoadManager *load_manager)
	: load_manager(load_manager)
{
}

/* virtual */
OP_STATUS
ES_DocumentGenerationListener::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_CANCELLED:
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
		return load_manager->FinishedInlineScript(thread);
	}

	return OpStatus::OK;
}

#ifdef SCRIPTS_IN_XML_HACK

/* --- ES_ExternalInlineScriptThread implementation ------------------ */

ES_ExternalInlineScriptThread::ES_ExternalInlineScriptThread(ES_LoadManager::ScriptElm *script_elm)
	: ES_Thread(NULL),
	  script_elm(script_elm)
{
}

/* virtual */
OP_STATUS
ES_ExternalInlineScriptThread::EvaluateThread()
{
	is_started = is_completed = TRUE;
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
ES_ExternalInlineScriptThread::Signal(ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED)
	{
		if (script_elm->thread == this)
			script_elm = NULL;
	}

	return ES_Thread::Signal(signal);
}

#endif // SCRIPTS_IN_XML_HACK

/* --- ES_LoadManager implementation --------------------------------- */

ES_LoadManager::ES_LoadManager(HLDocProfile *hld_profile)
	: hld_profile(hld_profile),
	  data_script_elm(NULL),
	  data_script_elm_was_running(FALSE),
	  is_cancelling(FALSE),
	  is_open(FALSE),
	  is_closing(FALSE),
	  is_stopped(FALSE),
	  abort_loading(FALSE),
	  first_script(TRUE),
	  last_script_type(SCRIPT_TYPE_GENERATED)
{
}

ES_LoadManager::~ES_LoadManager()
{
	while (ScriptElm *script_elm = (ScriptElm *) script_elms.First())
	{
		script_elm->Out();
		OP_DELETE(script_elm);
	}
}

BOOL
ES_LoadManager::IsBlocked()
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();

	if (script_elm && (script_elm->state == ScriptElm::INITIAL || script_elm->state == ScriptElm::RUNNING || script_elm->state == ScriptElm::BLOCKED_LOAD))
		return TRUE;
	else
		return FALSE;
}

BOOL
ES_LoadManager::IsWriting()
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();
	return script_elm && !script_elm->is_finished_writing;
}

BOOL
ES_LoadManager::IsClosing()
{
	return (!is_open && is_closing);
}

BOOL
ES_LoadManager::IsFinished()
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();
	return !is_open && (!script_elm || script_elm->state == ScriptElm::BLOCKED_CLOSE);
}

BOOL
ES_LoadManager::IsGenerationIdle()
{
	return is_open && script_elms.Empty();
}

BOOL
ES_LoadManager::IsInlineThread(ES_Thread *thread)
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();

	while (script_elm)
		if (script_elm->thread && thread->IsOrHasInterrupted(script_elm->thread))
			return TRUE;
		else
			script_elm = (ScriptElm *) script_elm->Suc();

	return FALSE;
}

BOOL
ES_LoadManager::HasData()
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();

	while (script_elm)
		if (!script_elm->is_finished_writing)
			return TRUE;
		else
			script_elm = (ScriptElm *) script_elm->Suc();

	return FALSE;
}

BOOL
ES_LoadManager::MoreData()
{
	return is_open || HasScripts();
}

void
ES_LoadManager::UpdateCurrentScriptElm(BOOL set)
{
	if (!data_script_elm && set)
	{
		SetCurrentScriptElm(static_cast<ScriptElm*>(script_elms.First()));

		while (data_script_elm->is_finished_writing)
		{
			OP_ASSERT(data_script_elm->state == ScriptElm::BLOCKED_LOAD && !data_script_elm->element->GetEndTagFound());
			SetCurrentScriptElm(static_cast<ScriptElm*>(data_script_elm->Suc()));
		}
	}
	else
		SetCurrentScriptElm(NULL);
}

OP_STATUS
ES_LoadManager::Write(ES_Thread *thread, const uni_char *str, unsigned str_len, BOOL end_with_newline)
{
	OP_ASSERT(thread);
	OP_ASSERT(!thread->IsSignalled() || !"A finished script doing document.write will mess up our script scheduling and probably cause crashes.");
	ScriptElm *script_elm = FindScriptElm(thread);
	if (!script_elm)
		return OpStatus::ERR_NO_MEMORY;

	if (!script_elm->InList())
		AddScriptElm(script_elm);

	script_elm->is_finished_writing = FALSE;

	if (thread->GetSharedInfo())
		thread->GetSharedInfo()->doc_written_bytes += UNICODE_SIZE(str_len);

	BOOL was_running = script_elm->state == ScriptElm::RUNNING;
	OP_ASSERT(!data_script_elm_was_running);
	script_elm->state = ScriptElm::BLOCKED_WRITE;

	SetCurrentScriptElm(script_elm, was_running);

	/* ESFlush() might reset data_script_elm to NULL due to the potential call to
	   CancelInlineThreads() inside the many ES_LoadManager::SetScript() overloads
	   hence need to NULL check it thoroughly. */

	BOOL block_es_thread = FALSE;
	LogicalDocument *logdoc = hld_profile->GetFramesDocument()->GetLogicalDocument();
	if (BlockedByPred(data_script_elm) && was_running && data_script_elm)
	{
		block_es_thread = TRUE;
		RETURN_IF_MEMORY_ERROR(logdoc->SetPendingScriptData(str, str_len));
	}
	else if (!logdoc->ESFlush(str, str_len, end_with_newline, thread) && was_running && data_script_elm)
		block_es_thread = TRUE;
	else if (data_script_elm && BlockedByPred(data_script_elm) && was_running)
		block_es_thread = TRUE;

	if (block_es_thread)
	{
		data_script_elm->thread->Block(ES_BLOCK_DOCUMENT_WRITE);
		PostURLMessage();
	}
	else if (data_script_elm && data_script_elm_was_running)
		/* Written without requiring a block; reset ScriptElm's state. */
		data_script_elm->state = ScriptElm::RUNNING;

	SetCurrentScriptElm(NULL);

	return OpStatus::OK;
}

void
ES_LoadManager::SignalWriteFinished()
{
	ScriptElm *current_script = static_cast<ScriptElm*>(script_elms.First());
	OP_ASSERT(current_script);

	if (current_script)
	{
		OP_ASSERT(!current_script->is_finished_writing);
		current_script->is_finished_writing = TRUE;

		if (current_script->thread)
		{
			if (current_script->state != ScriptElm::RUNNING && !BlockedByPred(current_script))
			{
				current_script->state = ScriptElm::RUNNING;

				if (current_script->thread->GetBlockType() != ES_BLOCK_NONE)
					current_script->thread->Unblock(ES_BLOCK_DOCUMENT_WRITE);
			}
			/* All consumed, but before clearing the current script element that
			   was tentatively blocked-on-write, reset/sync its state back to
			   its previous running state. */
			else
				current_script->state = ScriptElm::RUNNING;
		}
	}
}

BOOL
ES_LoadManager::HasScript(HTML_Element *element)
{
	return FindScriptElm(element) != NULL;
}

ES_Thread *
ES_LoadManager::GetThread(HTML_Element *script_element)
{
	if (ScriptElm *script_elm = FindScriptElm(script_element))
		return script_elm->thread;
	return NULL;
}

ES_Thread *
ES_LoadManager::GetInterruptedThread(HTML_Element *script_element)
{
	if (ScriptElm *script_elm = FindScriptElm(script_element))
	{
#ifdef SCRIPTS_IN_XML_HACK
		if (script_elm->thread && script_elm->thread->GetBlockType() == ES_BLOCK_EXTERNAL_SCRIPT)
			return script_elm->thread;
#endif // SCRIPTS_IN_XML_HACK
		if (script_elm->interrupted)
			return script_elm->interrupted->thread;
	}
	return NULL;
}

OP_STATUS
ES_LoadManager::RegisterScript(HTML_Element *element, BOOL is_external, BOOL is_inline_script, ES_Thread *interrupt_thread)
{
	/* This assert fails if a User Javascript inserts a script element into the document in a
	   BeforeScript or BeforeExternalScript event handler.  It is difficult to know if that
	   is the case though, so for now, the assert fails.  If User Javascript is not involved
	   and this assert fails, something is wrong. */
	OP_ASSERT(!script_elms.First() || ((ScriptElm *) script_elms.First())->state != ScriptElm::INITIAL);
	OP_ASSERT(!(element->GetInserted() == HE_INSERTED_BYPASSING_PARSER && is_inline_script) || !"A dynamically inserted script can't be an inline script");

	ScriptElm *script_elm = OP_NEW(ScriptElm, ());
	if (!script_elm)
		return OpStatus::ERR_NO_MEMORY;

	script_elm->element = element;
	script_elm->is_external = !!is_external;
	script_elm->is_script_in_document = TRUE;
	script_elm->is_inline_script = !!is_inline_script;
	script_elm->state = is_external ? ScriptElm::BLOCKED_LOAD : ScriptElm::INITIAL;
	script_elm->supports_doc_write = (interrupt_thread == NULL) ? 1 : 0; // In case of interrupt_thread != NULL, this might be set to TRUE below

	if (!interrupt_thread)
		if (ScriptElm *script_elm = (ScriptElm *) script_elms.First())
		{
			if (script_elm && (script_elm->state == ScriptElm::BLOCKED_WRITE || script_elm->state == ScriptElm::BLOCKED_CLOSE))
				interrupt_thread = script_elm->thread;
		}

	ScriptElm *interrupt_elm;
	if (interrupt_thread)
	{
		if (!(interrupt_elm = FindScriptElm(interrupt_thread, TRUE)))
		{
			OP_DELETE(script_elm);
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			if (!script_elm->supports_doc_write)
				script_elm->supports_doc_write = interrupt_elm->supports_doc_write;
		}

		if (interrupt_elm->is_inline_script)
			script_elm->is_inline_script = TRUE;
	}
	else
		interrupt_elm = NULL;

	BOOL do_interrupt = FALSE;

	if (interrupt_elm)
	{
		ScriptElm *base_interrupt_elm = interrupt_elm;

		while (base_interrupt_elm->interrupted)
			base_interrupt_elm = base_interrupt_elm->interrupted;

		if (is_external && is_inline_script)
			base_interrupt_elm->has_generated_external = TRUE;

		do_interrupt = !is_external && !base_interrupt_elm->has_generated_external || interrupt_elm->state == ScriptElm::BLOCKED_CLOSE;
		if (do_interrupt && interrupt_elm->state == ScriptElm::RUNNING)
		{
			interrupt_elm->state = ScriptElm::BLOCKED_INTERRUPTED;
			interrupt_elm->thread->Block(ES_BLOCK_INLINE_SCRIPT);
		}

		if (!interrupt_elm->InList())
			AddScriptElm(interrupt_elm, TRUE, TRUE);
	}

	script_elm->interrupted = interrupt_elm;

	if (interrupt_elm && interrupt_elm->thread && (script_elm->shared_thread_info = interrupt_elm->thread->GetSharedInfo()))
		script_elm->shared_thread_info->IncRef();

	AddScriptElm(script_elm, do_interrupt);
	return OpStatus::OK;
}

OP_STATUS
ES_LoadManager::ReportScriptError(HTML_Element *element, const uni_char *error_message)
{
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();
	ES_Runtime *runtime = frames_doc->GetESRuntime();

	if (!runtime || abort_loading)
		return OpStatus::OK;

	URL *script_url = element->GetScriptURL(*hld_profile->GetURL(), frames_doc->GetLogicalDocument());
	if (!script_url)
		script_url = &frames_doc->GetURL();

	ScriptElm *script_elm = FindScriptElm(element);
	ES_Thread *interrupt_thread = script_elm && script_elm->interrupted ? script_elm->interrupted->thread : NULL;

	if (interrupt_thread && interrupt_thread->GetScheduler() == runtime->GetESScheduler())
		runtime->GetESScheduler()->SetErrorHandlerInterruptThread(interrupt_thread);

	SourcePositionAttr *pos_attr = static_cast<SourcePositionAttr *>(element->GetSpecialAttr(ATTR_SOURCE_POSITION, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC));
	OP_STATUS status = runtime->ReportScriptError(error_message, script_url, pos_attr ? pos_attr->GetLineNumber() : 0, TRUE);

	if (interrupt_thread && interrupt_thread->GetScheduler() == runtime->GetESScheduler())
		runtime->GetESScheduler()->ResetErrorHandlerInterruptThread();

	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(OpStatus::ERR_SOFT_NO_MEMORY);

	return status;
}

OP_BOOLEAN
ES_LoadManager::SetScript(HTML_Element *element, ES_ProgramText *program_array, int program_array_length, BOOL allow_cross_origin_errors)
{
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();
	ES_Runtime *runtime = frames_doc->GetESRuntime();
	ES_Program *program = NULL;

	if (!runtime || abort_loading)
	{
		RETURN_IF_ERROR(CancelInlineScript(element));
		return OpBoolean::IS_FALSE;
	}

	URL *script_url = element->GetScriptURL(*hld_profile->GetURL(), frames_doc->GetLogicalDocument());
	BOOL linked_script = FALSE;
	if (script_url)
		linked_script = TRUE;
	else
		script_url = &frames_doc->GetURL();

	OP_STATUS compile_status;

	ScriptElm *script_elm = FindScriptElm(element);
	ES_Thread *interrupt_thread = script_elm && script_elm->interrupted ? script_elm->interrupted->thread : NULL;

	ES_Runtime::CompileProgramOptions options;
	options.global_scope = TRUE;
	options.is_external = linked_script;
	options.script_url = script_url;
	options.script_type = linked_script ? SCRIPT_TYPE_LINKED : last_script_type;
	options.when = UNI_L("while loading");
	options.context = options.script_type == SCRIPT_TYPE_LINKED ? UNI_L("Linked script compilation") : UNI_L("Inline script compilation");
	options.allow_cross_origin_error_reporting = allow_cross_origin_errors;
#ifdef ECMASCRIPT_DEBUGGER
	options.reformat_source = g_ecmaManager->GetWantReformatScript(runtime, program_array[0].program_text, program_array[0].program_text_length);
#endif // ECMASCRIPT_DEBUGGER
	SourcePositionAttr* pos_attr = static_cast<SourcePositionAttr *>(element->GetSpecialAttr(ATTR_SOURCE_POSITION, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_LOGDOC));
	if (pos_attr)
	{
		options.start_line = pos_attr->GetLineNumber();
		options.start_line_position = pos_attr->GetLinePosition();
	}

	if (interrupt_thread && interrupt_thread->GetScheduler() == runtime->GetESScheduler())
		runtime->GetESScheduler()->SetErrorHandlerInterruptThread(interrupt_thread);

	compile_status = runtime->CompileProgram(program_array, program_array_length, &program, options);

	if (interrupt_thread && interrupt_thread->GetScheduler() == runtime->GetESScheduler())
		runtime->GetESScheduler()->ResetErrorHandlerInterruptThread();

	OP_ASSERT(!OpStatus::IsSuccess(compile_status) == !program);

	if (compile_status == OpStatus::ERR_NO_MEMORY)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_SOFT_NO_MEMORY);
		RETURN_IF_ERROR(CancelInlineScript(element));
		return compile_status;
	}
	if (compile_status == OpStatus::ERR || !program)
	{
		RETURN_IF_ERROR(CancelInlineScript(element));
		return OpBoolean::IS_FALSE;
	}

	return SetScript(element, program, allow_cross_origin_errors);
}

OP_BOOLEAN
ES_LoadManager::SetScript(HTML_Element *element, ES_Program *program, BOOL allow_cross_origin_errors)
{
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();
	ES_Runtime *runtime = frames_doc->GetESRuntime();

	ScriptElm *script_elm = FindScriptElm(element);
	if (!script_elm)
	{
		/* This happens if page loading is aborted while a User JS BeforeScript
		   event is being processed.  User JS isn't told about it, so when it
		   finishes, it will try to start an inline script that has already been
		   cancelled.  However, if the call to this function does not come
		   through DOM_UserJSManager, something is wrong.  If that happens,
		   tell someone about it, or investigate yourself. */
		OP_ASSERT(!"might be nothing, read the comment above the assert");

		ES_Runtime::DeleteProgram(program);
		return OpBoolean::IS_FALSE;
	}

	DOM_Object *node;
	if (OpStatus::IsMemoryError(hld_profile->GetFramesDocument()->GetDOMEnvironment()->ConstructNode(node, element)) ||
	    !runtime->Protect(DOM_Utils::GetES_Object(node)))
	{
		ES_Runtime::DeleteProgram(program);
		return OpStatus::ERR_NO_MEMORY;
	}

	script_elm->node_protected = TRUE;

	ES_Thread *interrupt_thread;

	if (script_elm->interrupted)
		interrupt_thread = script_elm->interrupted->thread;
	else
		interrupt_thread = NULL;

#ifdef SCRIPTS_IN_XML_HACK
# ifdef USER_JAVASCRIPT
	if (hld_profile->IsXml() && script_elm->thread)
# else // USER_JAVASCRIPT
	if (hld_profile->IsXml() && script_elm->is_external && script_elm->thread)
# endif // USER_JAVASCRIPT
	{
		/* We've previously added a blocking thread, interrupt that thread
		   and unblock it so that execution can continue. */

		interrupt_thread = script_elm->thread;
		interrupt_thread->Unblock(ES_BLOCK_EXTERNAL_SCRIPT);

		script_elm->listener->Remove();
		OP_DELETE(script_elm->listener);
		script_elm->listener = NULL;
	}
#endif // SCRIPTS_IN_XML_HACK

	if (first_script)
	{
		first_script = FALSE;
	}

	ES_Context *context;

	if ((context = runtime->CreateContext(NULL)) != NULL)
	{
		if ((script_elm->thread = OP_NEW(ES_InlineScriptThread, (context, program, script_elm->shared_thread_info))) != NULL &&
		    (script_elm->listener = OP_NEW(ES_DocumentGenerationListener, (this))) != NULL)
		{
			script_elm->thread->SetIsExternalScript(script_elm->is_external);
			script_elm->thread->AddListener(script_elm->listener);
			script_elm->state = ScriptElm::RUNNING;
			if (allow_cross_origin_errors)
				script_elm->thread->SetAllowCrossOriginErrors();

			if (!script_elm->Pred())
			{
				if (script_elm->thread->GetSharedInfo() &&
					++script_elm->thread->GetSharedInfo()->inserted_by_parser_count >= ESUTILS_MAXIMUM_RECURSIVE_INLINE_THREADS &&
					script_elm->thread->GetSharedInfo()->doc_written_bytes >= ESUTILS_MAXIMUM_RECURSIVE_DOC_WRITE_BYTES)
				{
					/* This if block handles the case where a script element writes itself
					   recursively. After the number of handled scripts and amount of written
					   data overflow a limit, all queued scripts are cancelled and data is
					   dropped, else logdoc and the load manager would be stuck reparsing
					   and requeueing the same scripts still in the parser queue.
					   Inline threads created with doc.write() share the sharedinfo
					   object with the scripts that wrote them. */
#ifdef OPERA_CONSOLE
					if (!script_elm->thread->GetSharedInfo()->has_reported_recursion_error)
					{
						script_elm->thread->GetSharedInfo()->has_reported_recursion_error = TRUE;
						ES_ErrorInfo error(NULL);
						uni_strlcpy(error.error_text, UNI_L("Maximum inline script generation limit exceeded"), ARRAY_SIZE(error.error_text));
						OpStatus::Ignore(ES_Utils::PostError(frames_doc, error, UNI_L("Executing document.write()"), frames_doc->GetURL()));
					}
#endif // OPERA_CONSOLE
					OP_DELETE(script_elm->thread);

					script_elm->thread = NULL;
					script_elm->listener = NULL;

					RETURN_IF_ERROR(CancelInlineScript(element));
					RETURN_IF_ERROR(CancelInlineThreads());

					return OpStatus::ERR_OUT_OF_RANGE;
				}

				OP_BOOLEAN result = frames_doc->GetESScheduler()->AddRunnable(script_elm->thread, interrupt_thread);

				if (result != OpBoolean::IS_TRUE)
				{
					script_elm->thread = NULL;
					script_elm->listener = NULL;

					RETURN_IF_ERROR(CancelInlineScript(element));
				}

				return result;
			}
			else
				return OpBoolean::IS_TRUE;
		}

		script_elm->Out();

		if (!script_elm->thread)
			ES_Runtime::DeleteContext(context);
		else
		{
			OP_DELETE(script_elm->thread);
			script_elm->thread = NULL;
			program = NULL;
		}

		OP_DELETE(script_elm);
	}

	ES_Runtime::DeleteProgram(program);
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
ES_LoadManager::FinishedInlineScript(ES_Thread *thread)
{
	if (ScriptElm *script_elm = FindScriptElm(thread, FALSE))
	{
		/* So ScriptElm::~ScriptElm won't touch it. */
		script_elm->listener = NULL;
		return FinishedInlineScript(script_elm);
	}
	else
		return OpStatus::OK;
}

OP_STATUS
ES_LoadManager::FinishedInlineScript(ScriptElm *script_elm)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	ES_Thread *thread = script_elm->thread;
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();

#ifdef USER_JAVASCRIPT
	if (script_elm->element && frames_doc->GetDocRoot()->IsAncestorOf(script_elm->element))
		status = frames_doc->GetDOMEnvironment()->HandleScriptElementFinished(script_elm->element, thread);
#endif // USER_JAVASCRIPT

	if (script_elm->element)
		script_elm->element->CleanupScriptSource();

	if (OpStatus::IsSuccess(status) && script_elm->is_external)
		status = frames_doc->HandleEvent(ONLOAD, NULL, script_elm->element, SHIFTKEY_NONE, 0, script_elm->thread);

	ScriptElm *next_script_elm = (ScriptElm *) script_elm->Suc();
	ScriptElm *interrupted = script_elm->interrupted;

	script_elm->Out();

#ifdef DELAYED_SCRIPT_EXECUTION
	BOOL started_by_parser = FALSE;
	if (script_elm->is_inline_script)
		started_by_parser = frames_doc->GetLogicalDocument()->SignalScriptFinished(script_elm->element, this);
#else // DELAYED_SCRIPT_EXECUTION
	if (script_elm->is_inline_script)
		frames_doc->GetLogicalDocument()->SignalScriptFinished(script_elm->element, this);
#endif // DELAYED_SCRIPT_EXECUTION

	OP_ASSERT(script_elm->interrupted_count == 0);

	if (interrupted)
		if (interrupted->interrupted_count == 0 && interrupted->state == ScriptElm::BLOCKED_INTERRUPTED)
		{
			interrupted->thread->Unblock(ES_BLOCK_INLINE_SCRIPT);

			if (!interrupted->element)
			{
				if (next_script_elm == interrupted)
					next_script_elm = (ScriptElm *) next_script_elm->Suc();

				interrupted->Out();
				OP_DELETE(interrupted);
			}
			else
				interrupted->state = ScriptElm::RUNNING;
		}

	if (data_script_elm == script_elm)
		SetCurrentScriptElm(NULL);

	OP_DELETE(script_elm);

	if (next_script_elm && next_script_elm->thread && !next_script_elm->thread->GetScheduler())
	{
		/* We haven't started the thread yet. */

		ES_Thread *interrupt_thread;

		if (next_script_elm->interrupted)
		{
			interrupt_thread = next_script_elm->interrupted->thread;
#ifdef _DEBUG
			// Trying to check that we're interrupting something that actually is in the script elms list
			ScriptElm* debug_se = static_cast<ScriptElm*>(script_elms.First());
			while (debug_se)
			{
				if (debug_se->thread && debug_se->thread->GetScheduler())
				{
					OP_ASSERT(debug_se->thread == interrupt_thread);
				}
				debug_se = static_cast<ScriptElm*>(debug_se->Suc());
			}
#endif // _DEBUG
		}
		else
		{
			// Make sure the new thread runs before any threads waiting to close or we'll deadlock
			interrupt_thread = NULL;
			ScriptElm* se = static_cast<ScriptElm*>(script_elms.First());
			while (se)
			{
				if (se->thread && thread && se->thread->GetScheduler() == thread->GetScheduler())
				{
					interrupt_thread = se->thread;
					break;
				}
				se = static_cast<ScriptElm*>(se->Suc());
			}
		}

		OP_BOOLEAN result = frames_doc->GetESScheduler()->AddRunnable(next_script_elm->thread, interrupt_thread);

		if (result != OpBoolean::IS_TRUE)
		{
			next_script_elm->thread = NULL;
			next_script_elm->listener = NULL;

			return FinishedInlineScript(next_script_elm);
		}
	}

	if (next_script_elm && next_script_elm->state == ScriptElm::BLOCKED_WRITE && next_script_elm->is_finished_writing)
	{
		next_script_elm->state = ScriptElm::RUNNING;
		if (OpStatus::IsMemoryError(next_script_elm->thread->Unblock(ES_BLOCK_DOCUMENT_WRITE)))
			status = OpStatus::ERR_NO_MEMORY;
	}
	else if (!is_cancelling)
		if (!next_script_elm && abort_loading)
			AbortLoading();
#ifdef DELAYED_SCRIPT_EXECUTION
		else if (hld_profile->ESIsExecutingDelayedScript())
		{
			LogicalDocument *logdoc = hld_profile->GetLogicalDocument();
			if (started_by_parser && logdoc->GetParser()->HasUnfinishedWriteData())
				logdoc->ParseHtml(TRUE);

			if (script_elms.Empty())
				return hld_profile->ESFinishedDelayedScript();
		}
#endif // DELAYED_SCRIPT_EXECUTION
		else if (!is_stopped)
		{
			PostURLMessage();

			if (GetScriptGeneratingDoc() && IsGenerationIdle())
			{
				frames_doc->OnESGenerationIdle();
				frames_doc->GetTopDocument()->GetDocManager()->EndProgressDisplay(FALSE);
			}
		}

	return status;
}

OP_STATUS
ES_LoadManager::CancelInlineScript(HTML_Element *element)
{
	OP_STATUS status = OpStatus::OK;
	element->CleanupScriptSource();
	ScriptElm *script_elm = FindScriptElm(element);

	if (script_elm && !script_elm->thread) // Don't kill running (or to-be-run) scripts
	{
		ScriptElm *next_script_elm = (ScriptElm *) script_elm->Suc();
		ScriptElm *interrupted_script_elm = script_elm->interrupted;

		BOOL script_has_data = !script_elm->is_finished_writing;

		if (!script_has_data)
		{
			script_elm->Out();

			// we need to signal that the script is done processing, even though
			// it was empty, so that the parser can continue after waiting for
			// an external script.
			if (script_elm->is_inline_script)
				hld_profile->GetLogicalDocument()->SignalScriptFinished(script_elm->element, this);

#ifdef SCRIPTS_IN_XML_HACK
			if (script_elm->thread && script_elm->thread->GetBlockType() != ES_BLOCK_NONE)
				status = script_elm->thread->Unblock(ES_BLOCK_EXTERNAL_SCRIPT);
#endif // SCRIPTS_IN_XML_HACK

			if (data_script_elm == script_elm)
				SetCurrentScriptElm(NULL);

			OP_DELETE(script_elm);
		}
		else
		{
#ifdef DELAYED_SCRIPT_EXECUTION
			if (hld_profile->ESIsExecutingDelayedScript())
			{
				SetCurrentScriptElm(script_elm);

				if (OpStatus::IsSuccess(status))
				{
					hld_profile->ESFlushDelayed();
					hld_profile->GetLogicalDocument()->ParseHtml(TRUE);
				}

				SetCurrentScriptElm(NULL);
			}

			if (!hld_profile->ESIsExecutingDelayedScript())
#endif // DELAYED_SCRIPT_EXECUTION
				script_elm->state = ScriptElm::BLOCKED_WRITE;
		}

		if (next_script_elm && !BlockedByPred(next_script_elm) && next_script_elm->thread && !next_script_elm->thread->GetScheduler())
		{
			/* We haven't started the thread yet. */

			OP_BOOLEAN result = hld_profile->GetFramesDocument()->GetESScheduler()->AddRunnable(next_script_elm->thread);

			if (result != OpBoolean::IS_TRUE)
			{
				next_script_elm->thread = NULL;
				next_script_elm->listener = NULL;

				return FinishedInlineScript(next_script_elm);
			}
		}
		else if (next_script_elm && (next_script_elm->state == ScriptElm::BLOCKED_WRITE && next_script_elm->is_finished_writing ||
			interrupted_script_elm == next_script_elm && next_script_elm->state == ScriptElm::BLOCKED_INTERRUPTED && !interrupted_script_elm->interrupted_count))
		{
			status = next_script_elm->thread->Unblock(next_script_elm->state == ScriptElm::BLOCKED_WRITE ? ES_BLOCK_DOCUMENT_WRITE : ES_BLOCK_INLINE_SCRIPT);
			next_script_elm->state = ScriptElm::RUNNING;
		}
		else
		{
#ifdef DELAYED_SCRIPT_EXECUTION
			if (hld_profile->ESIsExecutingDelayedScript())
				switch (hld_profile->ESCancelScriptElement(element))
				{
				case OpStatus::OK:
					return OpStatus::OK;
				case OpStatus::ERR:
					if (!next_script_elm)
					{
						LogicalDocument *logdoc = hld_profile->GetLogicalDocument();
						if (logdoc->GetParser()->HasUnfinishedWriteData())
							logdoc->ParseHtml(TRUE);

						if (script_elms.Empty())
							return hld_profile->ESFinishedDelayedScript();
					}
					break;
				case OpStatus::ERR_NO_MEMORY:
					return OpStatus::ERR_NO_MEMORY;
				}
#endif // DELAYED_SCRIPT_EXECUTION

			PostURLMessage();
		}
	}

	return status;
}

BOOL
ES_LoadManager::GetScriptGeneratingDoc() const
{
	return is_open;
}

BOOL
ES_LoadManager::HasScripts()
{
	if (script_elms.Empty())
		return FALSE;
	else
	{
		ScriptElm *script_elm = (ScriptElm *) script_elms.First();
		return script_elm->thread || (script_elm->state == ScriptElm::BLOCKED_LOAD && script_elm->element->GetEndTagFound());
	}
}

void
ES_LoadManager::AddScriptElm(ScriptElm *new_script_elm, BOOL do_interrupt, BOOL insert_first)
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();

	if (new_script_elm->interrupted)
		++new_script_elm->interrupted->interrupted_count;

	if (new_script_elm->thread)
	{
		while (script_elm)
		{
			if ((script_elm->thread && new_script_elm->thread->IsOrHasInterrupted(script_elm->thread)) ||
			    (!script_elm->is_script_in_document && new_script_elm->is_script_in_document) || // inline scripts before scripts from other documents
			    (script_elm->is_script_in_document && new_script_elm->is_script_in_document &&
			     !script_elm->thread && script_elm->state == ScriptElm::BLOCKED_WRITE) ||
			    (new_script_elm->state != ScriptElm::BLOCKED_CLOSE && script_elm->state == ScriptElm::BLOCKED_CLOSE))
			{
				new_script_elm->Precede(script_elm);
				return;
			}

			script_elm = (ScriptElm *) script_elm->Suc();
		}

		if (insert_first)
			new_script_elm->IntoStart(&script_elms);
		else
			new_script_elm->Into(&script_elms);
	}
	else if (new_script_elm->interrupted)
		if (!do_interrupt)
		{
			ScriptElm *interrupted_elm = new_script_elm->interrupted;
			while (interrupted_elm->interrupted)
				interrupted_elm = interrupted_elm->interrupted;
			while (ScriptElm *following = (ScriptElm *) interrupted_elm->Suc())
				if (following->is_script_in_document && (following->state == ScriptElm::BLOCKED_LOAD || following->state == ScriptElm::RUNNING))
					interrupted_elm = following;
				else
					break;
			new_script_elm->Follow(interrupted_elm);
			--new_script_elm->interrupted->interrupted_count;
			new_script_elm->interrupted = NULL;
		}
		else
			new_script_elm->Precede(new_script_elm->interrupted);
	else
		new_script_elm->IntoStart(&script_elms);
}

ES_LoadManager::ScriptElm *
ES_LoadManager::FindScriptElm(HTML_Element *element)
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();

	while (script_elm)
	{
		if (script_elm->element == element)
			return script_elm;

		script_elm = (ScriptElm *) script_elm->Suc();
	}

	return NULL;
}

ES_LoadManager::ScriptElm *
ES_LoadManager::FindScriptElm(ES_Thread *thread, BOOL create, BOOL find_related)
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.Last();

	ScriptElm *related_elm = NULL;
	while (script_elm)
	{
		if (thread->IsOrHasInterrupted(script_elm->thread))
			if (script_elm->thread == thread || find_related)
				return script_elm;
			else if (create && !related_elm)
			{
				related_elm = script_elm;
			}

			script_elm = (ScriptElm *) script_elm->Pred();
	}

	if (create)
	{
		script_elm = OP_NEW(ScriptElm, ());
		if (!script_elm)
			return NULL;

		script_elm->listener = OP_NEW(ES_DocumentGenerationListener, (this));
		if (!script_elm->listener)
		{
			OP_DELETE(script_elm);
			return NULL;
		}

		if (related_elm)
		{
			script_elm->is_finished_writing = related_elm->is_finished_writing;
			script_elm->thread = thread;
			script_elm->thread->AddListener(script_elm->listener);
			script_elm->is_script_in_document = related_elm->is_script_in_document;
			script_elm->is_inline_script = related_elm->is_inline_script;
			script_elm->state = ScriptElm::RUNNING;
			script_elm->shared_thread_info = thread->GetSharedInfo();
			if (script_elm->shared_thread_info)
				script_elm->shared_thread_info->IncRef();

			return script_elm;
		}
		script_elm->is_finished_writing = TRUE;

		script_elm->thread = thread;
		script_elm->thread->AddListener(script_elm->listener);
		script_elm->is_script_in_document = FALSE;
		script_elm->is_inline_script = FALSE;  // All inline scripts are created through RegisterScript so this is not inline
		script_elm->state = ScriptElm::RUNNING;
		script_elm->shared_thread_info = thread->GetSharedInfo();
		if (script_elm->shared_thread_info)
			script_elm->shared_thread_info->IncRef();
	}

	return script_elm;
}

BOOL
ES_LoadManager::BlockedByPred(ScriptElm *script_elm)
{
	ScriptElm *pred_script_elm = (ScriptElm *) script_elm->Pred();

	// Don't block if we just wrote just the starttag of an
	// external script, since we must write the endtag before
	// the script can actually start.
	if (!pred_script_elm || (pred_script_elm->state == ScriptElm::BLOCKED_LOAD && !pred_script_elm->element->GetEndTagFound()))
		return FALSE;
	else
		return TRUE;
}

OP_STATUS
ES_LoadManager::CancelInlineThreads()
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	is_cancelling = TRUE;
	while (ScriptElm *script_elm = (ScriptElm *) script_elms.First())
	{
		if (script_elm->thread)
		{
			OP_STATUS status2 = OpStatus::OK;
			OpStatus::Ignore(status2);

			if (!script_elm->is_script_in_document)
			{
				if (script_elm->state != ScriptElm::RUNNING)
					status2 = script_elm->thread->Unblock(script_elm->state == ScriptElm::BLOCKED_INTERRUPTED ? ES_BLOCK_INLINE_SCRIPT : ES_BLOCK_DOCUMENT_WRITE);

				if (data_script_elm == script_elm)
					SetCurrentScriptElm(NULL);

				script_elm->Out();
				OP_DELETE(script_elm);
			}
			else if (script_elm->is_inline_script && script_elm->thread->GetScheduler())
				status2 = script_elm->thread->GetScheduler()->CancelThread(script_elm->thread);
			else
			{
				if (data_script_elm == script_elm)
					SetCurrentScriptElm(NULL);

				script_elm->Out();
				OP_DELETE(script_elm);
			}

			if (status2 == OpStatus::ERR_NO_MEMORY)
				status = OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			if (data_script_elm == script_elm)
				SetCurrentScriptElm(NULL);

			if (script_elm->is_inline_script)
				hld_profile->GetFramesDocument()->GetLogicalDocument()->SignalScriptFinished(script_elm->element, this);

			script_elm->Out();
			OP_DELETE(script_elm);
		}
	}
	is_cancelling = FALSE;

	return status;
}

OP_STATUS
ES_LoadManager::OpenDocument(ES_Thread *thread)
{
	is_open = TRUE;
	return OpStatus::OK;
}

OP_STATUS
ES_LoadManager::CloseDocument(ES_Thread *thread)
{
	if (is_open && !is_stopped || is_closing)
	{
		ScriptElm *script_elm = FindScriptElm(thread);
		if (!script_elm)
			return OpStatus::ERR_NO_MEMORY;

		if (!script_elm->is_script_in_document)
		{
			if (thread->IsCompleted() || thread->IsFailed())
			{
				script_elm->Out();
				OP_DELETE(script_elm);
			}
			else
			{
				script_elm->state = ScriptElm::BLOCKED_CLOSE;
				ScriptElm* following_script_elm = static_cast<ScriptElm*>(script_elm->Suc());
				// When we change order of the script_elm we might move a script past ScripElms
				// that script_elm was responsible for creating. Those should then be explicitly
				// linked to script_elm. We can identify those by being in the list but not added
				// to a scheduler.
				script_elm->Out();
				while (following_script_elm && !(following_script_elm->thread && following_script_elm->thread->GetScheduler()))
				{
					if (!following_script_elm->interrupted)
					{
						following_script_elm->interrupted = script_elm;
						script_elm->interrupted_count++;
					}

					following_script_elm = static_cast<ScriptElm*>(following_script_elm->Suc());
				}
				AddScriptElm(script_elm);

				ScriptElm* next_script_elm = (ScriptElm*)script_elms.First();
				if (next_script_elm != script_elm)
				{
					if (next_script_elm && next_script_elm->thread && !next_script_elm->thread->GetScheduler())
					{
						/* We haven't started the thread yet. */

						// Make sure the new thread runs before any threads waiting to close or we'll deadlock
						ES_Thread *interrupt_thread = NULL;
						ScriptElm* se = static_cast<ScriptElm*>(script_elms.First());
						while (se)
						{
							if (se->thread && se->thread->GetScheduler() == thread->GetScheduler())
							{
								interrupt_thread = se->thread;
								break;
							}
							se = static_cast<ScriptElm*>(se->Suc());
						}

						FramesDocument* frames_doc = hld_profile->GetFramesDocument();
						OP_BOOLEAN result = frames_doc->GetESScheduler()->AddRunnable(next_script_elm->thread, interrupt_thread);

						if (result != OpBoolean::IS_TRUE)
						{
							next_script_elm->thread = NULL;
							next_script_elm->listener = NULL;

							return FinishedInlineScript(next_script_elm);
						}
					}
				}

				if (script_elm->Suc() && ((ScriptElm *) (script_elm->Suc()))->state != ScriptElm::BLOCKED_CLOSE)
				{
					/* A script is trying to do something fairly odd, and blocking
					   this thread waiting for the document to finish loading will
					   probably just cause a deadlock. */

					script_elm->Out();
					OP_DELETE(script_elm);
				}
				else
					thread->Block(ES_BLOCK_DOCUMENT_WRITE);
			}
		}
		else if (!script_elm->InList())
			OP_DELETE(script_elm);

		is_open = FALSE;
		is_closing = TRUE;

		FramesDocument *frames_doc = hld_profile->GetFramesDocument();
		frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetDocManager()->GetCurrentURL().Id(TRUE), 0);
	}

	return OpStatus::OK;
}

void
ES_LoadManager::HandleOOM()
{
}

void
ES_LoadManager::StopLoading()
{
	if (is_closing)
	{
		ScriptElm *script_elm = (ScriptElm *) script_elms.First();

		while (script_elm)
		{
			ScriptElm *next_script_elm = (ScriptElm *) script_elm->Suc();

			if (script_elm->state == ScriptElm::BLOCKED_CLOSE)
			{
				script_elm->thread->Unblock(ES_BLOCK_DOCUMENT_WRITE);

				script_elm->Out();
				OP_DELETE(script_elm);
			}

			script_elm = next_script_elm;
		}

		is_closing = FALSE;
	}

	FramesDocument *frames_doc = hld_profile->GetFramesDocument();
	frames_doc->ESStoppedGeneratingDocument();

	is_stopped = TRUE;
}

void
ES_LoadManager::StopLoading(ES_Thread *thread)
{
	ScriptElm *script_elm = FindScriptElm(thread, FALSE, TRUE);

	if (script_elm)
		abort_loading = TRUE;
	else
	{
		DocumentManager *docman = hld_profile->GetFramesDocument()->GetDocManager();
		if (docman->GetLoadStatus() < WAIT_FOR_ECMASCRIPT)
			AbortLoading();
	}
}

BOOL
ES_LoadManager::IsStopped()
{
	return is_stopped;
}

BOOL
ES_LoadManager::SupportsWrite(ES_Thread* thread)
{
	ScriptElm* script_elm = FindScriptElm(thread, FALSE, TRUE);
	if (script_elm)
	{
		if (!script_elm->supports_doc_write)
			return FALSE;
	}

	return TRUE;
}

BOOL
ES_LoadManager::HasParserBlockingScript()
{
	ScriptElm *script_elm = (ScriptElm *) script_elms.First();
	while (script_elm && (script_elm->state != ScriptElm::BLOCKED_LOAD || script_elm->element->GetInserted() != HE_NOT_INSERTED))
		script_elm = static_cast<ScriptElm*>(script_elm->Suc());

	return script_elm != NULL;
}

BOOL
ES_LoadManager::IsHandlingExternalScript(ES_Thread *thread)
{
	if (thread)
		return ES_Runtime::GetIsExternal(thread->GetContext());

	if (data_script_elm)
		return data_script_elm->is_external;

	return FALSE;
}

ES_ScriptType
ES_LoadManager::GetLastScriptType() const
{
	return last_script_type;
}

void
ES_LoadManager::SetLastScriptType(ES_ScriptType type)
{
	last_script_type = type;
}

void
ES_LoadManager::AbortLoading()
{
	hld_profile->GetFramesDocument()->GetDocManager()->StopLoading(TRUE);
	abort_loading = FALSE;
}

void
ES_LoadManager::PostURLMessage()
{
	FramesDocument *frames_doc = hld_profile->GetFramesDocument();
	frames_doc->GetMessageHandler()->PostMessage(MSG_URL_DATA_LOADED, frames_doc->GetURL().Id(TRUE), 0);
}

void
ES_LoadManager::SetCurrentScriptElm(ScriptElm *new_data_script_elm, BOOL from_run_state)
{
	data_script_elm = new_data_script_elm;
	data_script_elm_was_running = from_run_state;
}

ES_LoadManager::ScriptElm::ScriptElm()
	: thread(0),
	  listener(0),
	  is_finished_writing(TRUE),
	  element(0),
	  interrupted(NULL),
	  shared_thread_info(NULL),
	  interrupted_count(0),
	  is_external(FALSE),
	  is_script_in_document(TRUE),
	  is_inline_script(TRUE),
	  node_protected(FALSE),
	  supports_doc_write(FALSE),
	  has_generated_external(FALSE)
{
}

ES_LoadManager::ScriptElm::~ScriptElm()
{
	if (listener)
	{
		listener->Remove();
		OP_DELETE(listener);
	}

	if (thread && !thread->GetScheduler())
		OP_DELETE(thread);

	if (shared_thread_info)
		shared_thread_info->DecRef();

	if (node_protected)
		DOM_Utils::GetDOM_Environment(element->GetESElement())->GetRuntime()->Unprotect(DOM_Utils::GetES_Object(element->GetESElement()));
}

void
ES_LoadManager::ScriptElm::Out()
{
	ScriptElm *script_elm = (ScriptElm *) Pred();
	while (script_elm)
	{
		if (script_elm->interrupted == this)
		{
			script_elm->interrupted = NULL;
			if (--interrupted_count == 0)
				break;
		}
		script_elm = (ScriptElm *) script_elm->Pred();
	}
	Link::Out();
	if (interrupted)
	{
		--interrupted->interrupted_count;
		interrupted = NULL;
	}
}

