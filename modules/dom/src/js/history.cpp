/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/js/history.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/domutils.h"

#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/dochand/docman.h"
#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

static OP_STATUS GetHistoryState(FramesDocument* doc, ES_Value* value)
{
	DocumentManager* doc_man = doc->GetDocManager();
	DocListElm* iter = doc_man->CurrentDocListElm();
	while (iter && iter->Doc() != doc)
		iter = iter->Pred();

	if (!iter)
	{
		iter = doc_man->CurrentDocListElm();
		while (iter && iter->Doc() != doc)
			iter = iter->Suc();
	}

	if (iter && iter->GetData())
		RETURN_IF_ERROR(doc->GetESRuntime()->CloneFromPersistent(iter->GetData(), *value));
	else
		DOM_Object::DOMSetNull(value);

	return OpStatus::OK;
}

/* virtual */ ES_GetState
JS_History::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (FramesDocument* frames_doc = GetFramesDocument())
		switch (property_name)
		{
		case OP_ATOM_length:
			if (value)
				DOMSetNumber(value, frames_doc->GetWindow()->GetHistoryLen());
			return GET_SUCCESS;

		case OP_ATOM_navigationMode:
			if (value)
			{
				const uni_char *mode;

				switch (frames_doc->GetUseHistoryNavigationMode())
				{
				case HISTORY_NAV_MODE_AUTOMATIC:
					mode = UNI_L("automatic");
					break;

				case HISTORY_NAV_MODE_COMPATIBLE:
					mode = UNI_L("compatible");
					break;

				default:
					mode = UNI_L("fast");
				}
				DOMSetString(value, mode);
			}
			return GET_SUCCESS;

		case OP_ATOM_state:
			if (value)
				GET_FAILED_IF_ERROR(GetHistoryState(frames_doc, value));
			return GET_SUCCESS;

		default:
			break;
		}

	return GET_FAILED;
}

/* virtual */ ES_PutState
JS_History::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		return PUT_READ_ONLY;
	case OP_ATOM_navigationMode:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (FramesDocument *frames_doc = GetFramesDocument())
		{
			const uni_char *argument = value->value.string;
			HistoryNavigationMode mode;

			if (uni_stri_eq(argument, UNI_L("automatic")))
				mode = HISTORY_NAV_MODE_AUTOMATIC;
			else if (uni_stri_eq(argument, UNI_L("compatible")))
				mode = HISTORY_NAV_MODE_COMPATIBLE;
			else if (uni_stri_eq(argument, UNI_L("fast")))
				mode = HISTORY_NAV_MODE_FAST;
			else
				return PUT_SUCCESS;

			frames_doc->SetUseHistoryNavigationMode(mode);
		}
		return PUT_SUCCESS;

	case OP_ATOM_state:
		return PUT_READ_ONLY;
	default:
		return PUT_FAILED;
	}
}

/* virtual */ int
JS_History::walk(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int delta)
{
	DOM_THIS_OBJECT_CHECK(this_object);

	if (!this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AllowScriptToNavigateInHistory) == 0)
		return ES_FAILED; // Just abort silently

	if (this_object->IsA(DOM_TYPE_HISTORY) || this_object->IsA(DOM_TYPE_WINDOW))
		if (FramesDocument *frames_doc = this_object->GetFramesDocument())
		{
			if (delta == 0)
			{
				if (argc >= 1)
					delta = TruncateDoubleToInt(argv[0].value.number);

				if (delta == 0)
				{
					JS_Window *window = static_cast<JS_Window *>(frames_doc->GetJSWindow());

					ES_Value location;
					CALL_FAILED_IF_ERROR(window->GetPrivate(DOM_PRIVATE_location, &location));

					return JS_Location::reload(DOM_VALUE2OBJECT(location, DOM_Object), NULL, 0, return_value, origining_runtime);
				}
			}

			DocumentManager *doc_man = frames_doc->GetDocManager();
			Window *window = doc_man->GetWindow();

			int max_position = window->GetHistoryMax();
			int min_position = window->GetHistoryMin();
			int current = window->GetCurrentHistoryNumber(), position = current;

			position += delta;
			position = MAX(min_position, position);
			position = MIN(max_position, position);

			if (position != current)
			{
				int real_delta = position - current;
				doc_man->AddPendingHistoryDelta(real_delta);
				ES_Thread *thread = GetCurrentThread(origining_runtime);
				ES_SharedThreadInfo *shared = thread->GetSharedInfo();

				if (doc_man->IsCurrentDocTheSameAt(position))
				{
					// We'll stay on the same document, but still http://www.whatwg.org/specs/web-apps/current-work/#traverse-the-history-by-a-delta
					// require it to be done asynchrounously.
					ES_Thread *history_traversal = OP_NEW(ES_HistoryNavigationThread, (shared, doc_man, real_delta));
					if (!history_traversal || OpStatus::IsMemoryError(this_object->GetRuntime()->GetESScheduler()->AddRunnable(history_traversal)))
						return ES_NO_MEMORY;
				}
				else
				{
					ES_TerminatingAction *action = OP_NEW(ES_HistoryWalkAction, (doc_man, position, shared && shared->is_user_requested));
					if (!action || OpStatus::IsMemoryError(this_object->GetRuntime()->GetESScheduler()->AddTerminatingAction(action)))
						return ES_NO_MEMORY;
				}
			}

			return ES_FAILED;
		}

	return ES_FAILED;
}

/* virtual */ int
JS_History::putState(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime, int data)
{
	DOM_THIS_OBJECT(history, DOM_TYPE_HISTORY, JS_History);
	DOM_CHECK_ARGUMENTS("-s|S");

	if (FramesDocument *frames_doc = history->GetFramesDocument())
	{
		ES_PersistentValue *state = NULL;

		OP_STATUS cloning_status = origining_runtime->CloneToPersistent(argv[0], state);
		if (OpStatus::IsError(cloning_status))
			if (OpStatus::IsMemoryError(cloning_status))
				return ES_NO_MEMORY;
			else
				return DOM_CALL_DOMEXCEPTION(DATA_CLONE_ERR);

		OpAutoPtr<ES_PersistentValue> ap_state(state);
		const uni_char *title = argv[1].value.string;
		const uni_char *url = (argc > 2 && argv[2].type == VALUE_STRING) ? argv[2].value.string : NULL;

		URL resolved_url;
		if (url)
		{
			resolved_url = GetEncodedURL(origining_runtime->GetFramesDocument(), frames_doc, url);
			if (!resolved_url.IsValid() || DOM_Utils::IsOperaIllegalURL(resolved_url))
				return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

			if (!resolved_url.SameServer(frames_doc->GetURL(), URL::KCheckPort) ||
				frames_doc->GetURL().Type() != resolved_url.Type())
				return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

			OpString8 origin_path_and_query;
			OpString8 resolved_path_and_query;
			CALL_FAILED_IF_ERROR(origining_runtime->GetOriginURL().GetAttribute(URL::KPathAndQuery_L, origin_path_and_query));
			CALL_FAILED_IF_ERROR(resolved_url.GetAttribute(URL::KPathAndQuery_L, resolved_path_and_query));
			if (!history->OriginCheck(origining_runtime->GetOriginURL(), resolved_url) &&
				(origin_path_and_query.Compare(resolved_path_and_query) != 0))
				return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);
		}
		else
			resolved_url = frames_doc->GetDocManager()->CurrentDocListElm()->GetUrl();

		if (data == 0) // push
		{
			ES_ThreadScheduler *scheduler = history->GetRuntime()->GetESScheduler();
			ES_TerminatingThread *terminator = static_cast<ES_TerminatingThread *>(scheduler->GetTerminatingThread());
			if (terminator)
			{
				terminator->RemoveAction(ES_HISTORY_WALK_ACTION);
				/* The action would clear the pending delta but since it has been removed
				 the delta must be cleared here. */
				frames_doc->GetDocManager()->ClearPendingHistoryDelta();
			}

			scheduler->CancelThreads(ES_THREAD_HISTORY_NAVIGATION);

			OP_ASSERT(frames_doc->GetDocManager()->GetPendingHistoryDelta() == 0);
		}

		CALL_FAILED_IF_ERROR(frames_doc->GetDocManager()->PutHistoryState(ap_state.get(), title, resolved_url, !data));
		ap_state.release();
	}

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(JS_History)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_History, JS_History::walk, -1, "back", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_History, JS_History::walk, 1, "forward", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_History, JS_History::walk, 0, "go", "n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_History, JS_History::putState, 0, "pushState", "-sS-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_History, JS_History::putState, 1, "replaceState", "-sS-")
DOM_FUNCTIONS_WITH_DATA_END(JS_History)
