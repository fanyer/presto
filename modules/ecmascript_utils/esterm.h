/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESTERM_H
#define ES_UTILS_ESTERM_H

#include "modules/ecmascript_utils/esthread.h"
#include "modules/url/url2.h"
#include "modules/dochand/documentreferrer.h"

class Window;
class DocumentManager;

/* --- ES_TerminatedByParentAction ----------------------------------- */

class ES_TerminatedByParentAction : public ES_TerminatingAction, public ES_ThreadListener
{
public:
	ES_TerminatedByParentAction(ES_TerminatingThread *parent_thread);
	~ES_TerminatedByParentAction();

	OP_STATUS PerformAfterUnload(ES_TerminatingThread *);
	void TellParent(ES_Thread *thread);

	static const bool FINAL = true;
	static const bool CONDITIONAL = false;
	static const bool SEND_ONUNLOAD = true;

private:
	/* From ES_ThreadListener: */
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

	ES_TerminatingThread *parent_thread;
	ES_Thread *this_thread;
};

/* --- ES_WindowCloseAction ------------------------------------------ */

class ES_WindowCloseAction : public ES_TerminatingAction
{
public:
	ES_WindowCloseAction(Window *window_);
	OP_STATUS PerformAfterUnload(ES_TerminatingThread *);

	static const bool FINAL = true;
	static const bool CONDITIONAL = false;
	static const bool SEND_ONUNLOAD = true;

private:
	Window *window;
};

/* --- ES_OpenURLAction ---------------------------------------------- */

class ES_OpenURLAction : public ES_TerminatingAction
{
public:
	ES_OpenURLAction(DocumentManager *doc_man, const URL &url, const URL &ref_url, BOOL check_if_expired,
					 BOOL reload, BOOL replace, BOOL user_initiated, EnteredByUser entered_by_user,
					 BOOL is_walking_in_history, BOOL user_auto_reload = FALSE, BOOL from_html_attribute = FALSE,
					 int use_history_number = -1);
	~ES_OpenURLAction();

	OP_STATUS PerformBeforeUnload(ES_TerminatingThread *thread, BOOL sending_onunload, BOOL &cancel_onunload);
	OP_STATUS PerformAfterUnload(ES_TerminatingThread *);

	OP_STATUS SendPendingUnload(BOOL check_if_inline_expired_, BOOL use_plugin_);
	OP_STATUS CancelPendingUnload();

	static const bool FINAL = false;
	static const bool CONDITIONAL = true;
	static const bool SEND_ONUNLOAD = true;

	void SetReloadFlags(BOOL reload_document, BOOL conditionally_request_document, BOOL reload_inlines, BOOL conditionally_request_inlines);

private:
	DocumentManager *doc_man;
	URL url;
	DocumentReferrer ref_url;
	BOOL check_if_expired;
	BOOL reload;
	BOOL replace;
	BOOL user_initiated;
	EnteredByUser entered_by_user;
	BOOL is_walking_in_history;
	BOOL url_loaded;
	BOOL delayed_onunload;
	BOOL cancelled;
	ES_TerminatingThread *terminating_thread;

	/* Reload flags, only relevant if reload==TRUE. */
	BOOL has_set_reload_flags;
	BOOL reload_document;
	BOOL conditionally_request_document;
	BOOL reload_inlines;
	BOOL conditionally_request_inlines;

	/* Arguments sent to DocumentManager::HandleByOpera after the unload
	   event has been sent. */
	BOOL check_if_inline_expired;
	BOOL use_plugin;

	/* Used to passed back to the OpenURL() method in DocumentManager to make sure we get a
	   reload after ES has done its unload magic on the previous page
	*/
	BOOL user_auto_reload;

	BOOL from_html_attribute;

	/* Same as DocumentManager::use_history_number */
	int use_history_number;
};

/* --- ES_ReplaceDocumentAction -------------------------------------- */

class ES_ReplaceDocumentAction : public ES_TerminatingAction, public ES_ThreadListener
{
public:
	ES_ReplaceDocumentAction(FramesDocument *old_doc, FramesDocument *new_doc, ES_Thread *generating_thread, BOOL inherit_design_mode = FALSE);

	OP_STATUS PerformAfterUnload(ES_TerminatingThread *);
	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

	static const bool FINAL = true;
	static const bool CONDITIONAL = false;
	static const bool SEND_ONUNLOAD = false;

private:
	FramesDocument *old_doc;
	FramesDocument *new_doc;
	ES_Thread *generating_thread;
	BOOL generating_thread_cancelled, inherit_design_mode;
};

/* --- ES_HistoryWalkAction ------------------------------------------ */

class ES_HistoryWalkAction : public ES_TerminatingAction
{
public:
	ES_HistoryWalkAction(DocumentManager *doc_man, int history_pos, BOOL user_requested);
	OP_STATUS PerformAfterUnload(ES_TerminatingThread *);

	static const bool FINAL = true;
	static const bool CONDITIONAL = true;
	static const bool SEND_ONUNLOAD = true;

private:
	DocumentManager *doc_man;
	int history_pos;
	BOOL user_requested;
};

#endif /* ES_UTILS_ESTERM_H */
