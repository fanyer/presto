/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#include "modules/ecmascript_utils/esterm.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/hardcore/mh/messages.h"

/* --- ES_TerminatedByParentAction ----------------------------------- */

ES_TerminatedByParentAction::ES_TerminatedByParentAction(ES_TerminatingThread *parent_thread)
	: ES_TerminatingAction(FINAL, CONDITIONAL, SEND_ONUNLOAD, ES_TERMINATED_BY_PARENT_ACTION, TRUE),
	  parent_thread(parent_thread),
	  this_thread(NULL)
{
}

ES_TerminatedByParentAction::~ES_TerminatedByParentAction()
{
	if (parent_thread && this_thread)
	{
		ES_ThreadListener::Remove();
		OpStatus::Ignore(parent_thread->ChildTerminated(this_thread));
	}
}

/* virtual */
OP_STATUS
ES_TerminatedByParentAction::PerformAfterUnload(ES_TerminatingThread *)
{
	if (parent_thread)
	{
		RETURN_IF_ERROR(parent_thread->ChildTerminated(this_thread));
		ES_ThreadListener::Remove();
		parent_thread = NULL;
	}
	return OpStatus::OK;
}

void
ES_TerminatedByParentAction::TellParent(ES_Thread *thread)
{
	this_thread = thread;
	parent_thread->ChildNoticed(this_thread);
	parent_thread->AddListener(this);
}

/* virtual */
OP_STATUS
ES_TerminatedByParentAction::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	OP_ASSERT(thread == parent_thread);

	if (signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FINISHED || signal == ES_SIGNAL_FAILED)
	{
		ES_ThreadListener::Remove();
		parent_thread = NULL;
	}

	return OpStatus::OK;
}

/* --- ES_WindowCloseAction ------------------------------------------ */

ES_WindowCloseAction::ES_WindowCloseAction(Window *window_)
	: ES_TerminatingAction(FINAL, CONDITIONAL, SEND_ONUNLOAD, ES_WINDOW_CLOSE_ACTION),
	  window(window_)
{
}

/* virtual */
OP_STATUS
ES_WindowCloseAction::PerformAfterUnload(ES_TerminatingThread *)
{
	if (!window->GetMessageHandler()->PostMessage(MSG_ES_CLOSE_WINDOW, 0, 0))
		return OpStatus::ERR_NO_MEMORY;
	else
		return OpStatus::OK;
}

/* --- ES_OpenURLAction ---------------------------------------------- */

ES_OpenURLAction::ES_OpenURLAction(DocumentManager *doc_man, const URL &url, const URL &ref_url, BOOL check_if_expired,
								   BOOL reload, BOOL replace, BOOL user_initiated, EnteredByUser entered_by_user,
								   BOOL is_walking_in_history, BOOL user_auto_reload, BOOL from_html_attribute,
								   int use_history_number)
	: ES_TerminatingAction(FINAL, CONDITIONAL, SEND_ONUNLOAD, ES_OPEN_URL_ACTION),
	  doc_man(doc_man),
	  url(url),
	  ref_url(ref_url),
	  check_if_expired(check_if_expired),
	  reload(reload),
	  replace(replace),
	  user_initiated(user_initiated),
	  entered_by_user(entered_by_user),
	  is_walking_in_history(is_walking_in_history),
	  url_loaded(FALSE),
	  delayed_onunload(FALSE),
	  cancelled(FALSE),
	  terminating_thread(NULL),
	  has_set_reload_flags(FALSE),
	  reload_document(FALSE),
	  conditionally_request_document(FALSE),
	  reload_inlines(FALSE),
	  conditionally_request_inlines(FALSE),
	  check_if_inline_expired(FALSE),
	  use_plugin(FALSE),
	  user_auto_reload(user_auto_reload),
	  from_html_attribute(from_html_attribute),
	  use_history_number(use_history_number)
{
	/* Don't send unload event on reload. */
	if (reload)
		send_onunload = FALSE;
}

ES_OpenURLAction::~ES_OpenURLAction()
{
	if (delayed_onunload)
	{
		if (terminating_thread->GetBlockType() == ES_BLOCK_WAITING_FOR_URL)
			terminating_thread->Unblock(ES_BLOCK_WAITING_FOR_URL);

		doc_man->ESSetPendingUnload(NULL);
	}
}

/* virtual */
OP_STATUS
ES_OpenURLAction::PerformBeforeUnload(ES_TerminatingThread *thread, BOOL sending_onunload, BOOL &cancel_onunload)
{
	if (IsLast())
	{
		/* Javascript URLs should be handled by a special thread, they should not
		   be opened this way. */
		OP_ASSERT(url.Type() != URL_JAVASCRIPT);

		BOOL old_redirect = doc_man->GetRedirect();

		URL current_url = doc_man->GetCurrentDoc()->GetURL();

		if (reload || !(url == current_url) || current_url.Status(TRUE) == URL_LOADING_ABORTED || !url.RelName())
			doc_man->ResetStateFlags();

		doc_man->SetReload(reload);
		if (reload && has_set_reload_flags)
			doc_man->SetReloadFlags(reload_document, conditionally_request_document, reload_inlines, conditionally_request_inlines);
		doc_man->SetRedirect(old_redirect);
		doc_man->SetUserAutoReload(user_auto_reload);
		doc_man->SetUseHistoryNumber(use_history_number);
		doc_man->SetReplace(replace);

		DocumentManager::OpenURLOptions options;
		options.user_initiated = user_initiated;
		options.create_doc_now = FALSE;
		options.entered_by_user = entered_by_user;
		options.is_walking_in_history = is_walking_in_history;
		options.es_terminated = TRUE;
		options.from_html_attribute = from_html_attribute;
		doc_man->OpenURL(url, ref_url, check_if_expired, reload, options);

		if (doc_man->GetLoadStatus() == NOT_LOADING)
			cancel_onunload = TRUE;
		else if (thread && sending_onunload)
		{
			/* Block this thread and tell the DocumentManager to unblock it when the
			   URL has started loading. */
			thread->Block(ES_BLOCK_WAITING_FOR_URL);
			doc_man->ESSetPendingUnload(this);

			delayed_onunload = TRUE;
			terminating_thread = thread;
		}

		url_loaded = TRUE;
	}

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
ES_OpenURLAction::PerformAfterUnload(ES_TerminatingThread *)
{
	if (!cancelled)
	{
		if (url_loaded)
		{
			if (delayed_onunload && IsLast())
			{
				url_loaded = FALSE;
				long user_data = (check_if_inline_expired ? 1 : 0) | (use_plugin ? 2 : 0) | (user_initiated ? 4 : 0);

				doc_man->GetMessageHandler()->PostMessage(MSG_HANDLE_BY_OPERA, doc_man->GetSubWinId(), user_data);
			}
		}
		else
		{
			BOOL cancel_onunload;
			return PerformBeforeUnload(NULL, FALSE, cancel_onunload);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
ES_OpenURLAction::SendPendingUnload(BOOL check_if_inline_expired_, BOOL use_plugin_)
{
	check_if_inline_expired = check_if_inline_expired_;
	use_plugin = use_plugin_;

	return terminating_thread->Unblock(ES_BLOCK_WAITING_FOR_URL);
}

OP_STATUS
ES_OpenURLAction::CancelPendingUnload()
{
	cancelled = TRUE;

	if (IsLast())
		terminating_thread->CancelUnload();

	return terminating_thread->Unblock(ES_BLOCK_WAITING_FOR_URL);
}

void
ES_OpenURLAction::SetReloadFlags(BOOL reload_document0, BOOL conditionally_request_document0, BOOL reload_inlines0, BOOL conditionally_request_inlines0)
{
	has_set_reload_flags = TRUE;
	reload_document = reload_document0;
	conditionally_request_document = conditionally_request_document0;
	reload_inlines = reload_inlines0;
	conditionally_request_inlines = conditionally_request_inlines0;
}

/* --- ES_ReplaceDocumentAction -------------------------------------- */

ES_ReplaceDocumentAction::ES_ReplaceDocumentAction(FramesDocument *old_doc, FramesDocument *new_doc, ES_Thread *generating_thread, BOOL inherit_design_mode)
	: ES_TerminatingAction(FINAL, CONDITIONAL, SEND_ONUNLOAD, ES_REPLACE_DOCUMENT_ACTION),
	  old_doc(old_doc),
	  new_doc(new_doc),
	  generating_thread(generating_thread),
	  generating_thread_cancelled(FALSE),
	  inherit_design_mode(inherit_design_mode)
{
	if (generating_thread)
		generating_thread->AddListener(this);
}

/* virtual */
OP_STATUS
ES_ReplaceDocumentAction::PerformAfterUnload(ES_TerminatingThread *)
{
	if (generating_thread)
	{
		if (!generating_thread_cancelled)
		{
			RETURN_IF_ERROR(generating_thread->Unblock(ES_BLOCK_DOCUMENT_REPLACED));
			ES_ThreadListener::Remove();
		}

		if (old_doc->SetAsCurrentDoc(FALSE) == OpStatus::ERR_NO_MEMORY ||
			new_doc->SetAsCurrentDoc(TRUE) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;

		new_doc->GetDocManager()->SetLoadStatus(DOC_CREATED);

#ifdef DOCUMENT_EDIT_SUPPORT
		if (inherit_design_mode)
			RETURN_IF_ERROR(new_doc->SetEditable(FramesDocument::DESIGNMODE));
#endif // DOCUMENT_EDIT_SUPPORT
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ReplaceDocumentAction::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
		/* This should never happen, the thread is supposed to be blocked. */
		OP_ASSERT(FALSE);
		/* fall through */

	case ES_SIGNAL_CANCELLED:
		generating_thread_cancelled = TRUE;
		ES_ThreadListener::Remove();
	}

	return OpStatus::OK;
}

/* --- ES_HistoryWalkAction ------------------------------------------ */

ES_HistoryWalkAction::ES_HistoryWalkAction(DocumentManager *doc_man, int history_pos, BOOL user_requested)
	: ES_TerminatingAction(FINAL, CONDITIONAL, SEND_ONUNLOAD, ES_HISTORY_WALK_ACTION),
	  doc_man(doc_man),
	  history_pos(history_pos),
	  user_requested(user_requested)
{
}

/* virtual */
OP_STATUS
ES_HistoryWalkAction::PerformAfterUnload(ES_TerminatingThread *)
{
	if (doc_man->GetWindow())
		doc_man->GetWindow()->SetCurrentHistoryPos(history_pos, TRUE, user_requested);
	// Termination action cancels any further traversing. Reset the delta.
	doc_man->ClearPendingHistoryDelta();
	return OpStatus::OK;
}

