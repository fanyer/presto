/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_JIL_PLAYER_SUPPORT

#include "modules/dom/src/domjil/domjilmediaplayer.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/device_api/jil/JILFSMgr.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/dom/src/domhtml/htmlelem.h"

DOM_JILMediaPlayer::DOM_JILMediaPlayer()
	: m_state(PLAYER_STATE_BEGIN) // Safest default value.
	, m_on_mediaplayer_state_changed(NULL)
	, m_open_thread(NULL)
	, m_open_failed(FALSE)
	, m_element(NULL)
{
	OP_ASSERT(PLAYER_STATE_BEGIN == 0);
#ifdef SELFTEST
	m_state_strings[PLAYER_STATE_BEGIN]		= UNI_L("begin");
#else
	m_state_strings[PLAYER_STATE_BEGIN]		= NULL;
#endif // SELFTEST
	m_state_strings[PLAYER_STATE_OPENED]	= UNI_L("opened");
	m_state_strings[PLAYER_STATE_PLAYING]	= UNI_L("playing");
	m_state_strings[PLAYER_STATE_STOPPED]	= UNI_L("stopped");
	m_state_strings[PLAYER_STATE_PAUSED]	= UNI_L("paused");
	m_state_strings[PLAYER_STATE_COMPLETED] = UNI_L("completed");
}

DOM_JILMediaPlayer::~DOM_JILMediaPlayer()
{

}

void
DOM_JILMediaPlayer::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_on_mediaplayer_state_changed);
	GCMark(m_element);
}


void DOM_JILMediaPlayer::SetState(PlayerState state)
{
	m_state = state;
	if (m_on_mediaplayer_state_changed)
	{
		ES_AsyncInterface* async_iface = GetRuntime()->GetEnvironment()->GetAsyncInterface();
		ES_Value argv[1];
		OP_ASSERT(state >= PLAYER_STATE_FIRST);
		OP_ASSERT(state < PLAYER_STATE_LAST);
		OP_ASSERT(m_state_strings[m_state]);
		DOMSetString(&(argv[0]), m_state_strings[m_state]);
		async_iface->CallFunction(m_on_mediaplayer_state_changed, GetNativeObject(), sizeof(argv) / sizeof(argv[0]), argv, NULL, NULL);
	}
}

/* virtual */ ES_GetState
DOM_JILMediaPlayer::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onStateChange:
			DOMSetObject(value, m_on_mediaplayer_state_changed);
			return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILMediaPlayer::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
	case OP_ATOM_onStateChange:
		switch (value->type)
		{
		case VALUE_OBJECT:
			m_on_mediaplayer_state_changed = value->value.object;
			return PUT_SUCCESS;
		case VALUE_NULL:
			m_on_mediaplayer_state_changed = NULL;
			return PUT_SUCCESS;
		default:
			return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
		}
	}

	return PUT_FAILED;
}

/* virtual */
int DOM_JILMediaPlayer::Open(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CallState* call_state = DOM_CallState::FromReturnValue(return_value);
	OP_ASSERT(call_state); // We should always have call_state from security check earlier.
	if (call_state->GetPhase() < DOM_CallState::PHASE_EXECUTION_0)
	{
		DOM_CHECK_ARGUMENTS_JIL("s");
		if (m_state != PLAYER_STATE_BEGIN && m_state != PLAYER_STATE_STOPPED && m_state != PLAYER_STATE_COMPLETED && m_state != PLAYER_STATE_OPENED) // other entry states are not allowed
			return ES_FAILED;

		if (m_open_thread)
		{
			m_open_thread->Unblock();
			m_open_thread = NULL;
		}
		m_open_failed = FALSE;

		// Get media element
		JILMediaElement *media_element = GetJILMediaElement();

		if (!media_element)
			return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("Unable to open URL"));

		OpGadget* gadget = g_DOM_jilUtils->GetGadgetFromRuntime(origining_runtime);
		OP_ASSERT(gadget);

		const uni_char* resource_url = argv[0].value.string;

		OpString gadget_url_str;

		CALL_FAILED_IF_ERROR(gadget->GetGadgetUrl(gadget_url_str, FALSE, TRUE));
		URL gadget_url = g_url_api->GetURL(gadget_url_str.CStr(), gadget->UrlContextId());

		URL url = g_url_api->GetURL(gadget_url, resource_url);	// URL Context ID is inherited from gadget_url
		if (gadget_url.IsEmpty() || url.IsEmpty())
			return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Unable to open URL"));

		if (url.Type() == URL_FILE)
		{
			if (!g_DAPI_jil_fs_mgr->IsFileAccessAllowed(url))
				return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Could not access the file"));
		}

		OP_STATUS status = media_element->Open(url);

		if (OpStatus::IsError(status))
			return HandleJILError(status, return_value, origining_runtime);
		else if (m_open_failed)
			return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Cannot open media URL."));

		m_open_thread = DOM_Object::GetCurrentThread(origining_runtime);
		OP_ASSERT(m_open_thread);

		// Unlink from any listeners' list and link
		// to new thread.
		Out();
		m_open_thread->AddListener(this);

		m_open_thread->Block();
		call_state->SetPhase(static_cast<DOM_CallState::CallPhase>(DOM_CallState::PHASE_EXECUTION_0));
		return ES_SUSPEND | ES_RESTART;
	}
	else
	{
		OP_ASSERT(call_state->GetPhase() ==  DOM_CallState::PHASE_EXECUTION_0);
		// If we make a call to open and then before it finishes we make the call to open in another ecmascript thread,
		// then this call fails.
		// The actual check for this is done by checking if the thread of latest call to open is the same as current thread
		if (DOM_Object::GetCurrentThread(origining_runtime) != m_open_thread)
			return HandleJILError(OpStatus::ERR, return_value, origining_runtime);

		// Detach from the thread and clear the pointer, won't be needed any more.
		ES_ThreadListener::Remove();
		m_open_thread = NULL;

		if (m_open_failed) // some error occured in opening
			return HandleJILError(OpStatus::ERR, return_value, origining_runtime);
		SetState(PLAYER_STATE_OPENED);
		return ES_FAILED;
	}
}

/* virtual */
int DOM_JILMediaPlayer::Play(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	double loops = argv[0].value.number;
	if (loops < 0.0 || loops > INT_MAX)
		return CallException(JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime);

	loops = op_floor(loops); // We dont need non integer loop numbers
	if (loops == 0)
		return ES_FAILED; // This is completely useless, but in a spec. TODO - try to convince JIL to change it.

	JILMediaElement *media_element = GetJILMediaElement();

	if (!media_element || (m_state != PLAYER_STATE_STOPPED && m_state != PLAYER_STATE_COMPLETED
			&& m_state != PLAYER_STATE_OPENED)) // Other entry states are not allowed.
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(media_element->Play(FALSE));

	m_loops_left = static_cast<int>(loops) - 1;

	return ES_FAILED;
}

/* virtual */
int DOM_JILMediaPlayer::Pause(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	JILMediaElement *media_element = GetJILMediaElement();

	if (!media_element || m_state != PLAYER_STATE_PLAYING) // Other entry states are not allowed.
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(media_element->Pause());
	return ES_FAILED;
}

/* virtual */
int DOM_JILMediaPlayer::Resume(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	JILMediaElement *media_element = GetJILMediaElement();

	if (!media_element || m_state != PLAYER_STATE_PAUSED) // Other entry states are not allowed.
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(media_element->Play(TRUE));

	return ES_FAILED;
}

/* virtual */
int DOM_JILMediaPlayer::Stop(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	JILMediaElement *media_element = GetJILMediaElement();

	if (!media_element || (m_state != PLAYER_STATE_PLAYING && m_state != PLAYER_STATE_PAUSED)) // Other entry states are not allowed.
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(media_element->Stop());

	return ES_FAILED;
}

/* virtual */
int DOM_JILMediaPlayer::SetWindow(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_ARGUMENT_JIL_OBJECT(element, 0, DOM_TYPE_ELEMENT, DOM_Element);

	if (element)
	{
		OP_ASSERT(element->GetThisElement());

		if (!element->GetThisElement()->IsMatchingType(HE_OBJECT, NS_HTML))
			return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Media preview requires an <object> element."));
	}

	OP_STATUS err = SetWindowInternal(element);
	if (OpStatus::IsError(err))
		return HandleJILError(err, return_value, origining_runtime);

	return ES_FAILED;
}

OP_STATUS DOM_JILMediaPlayer::SetWindowInternal(DOM_Element *dom_element)
{
	HTML_Element *element = m_element ? m_element->GetThisElement() : NULL;
	HTML_Element *new_element = dom_element ? dom_element->GetThisElement() : NULL;

	if (new_element)
	{
		// Create media element.
		JILMediaElement * media_element = OP_NEW(JILMediaElement, (new_element));
		if (!media_element)
			return OpStatus::ERR_NO_MEMORY;

		if (element)
		{
			OP_STATUS err = media_element->TakeOver(GetJILMediaElement(FALSE));
			if (OpStatus::IsError(err))
			{
				OP_DELETE(media_element);
				return err;
			}
			element->SetMedia(Markup::MEDA_COMPLEX_JIL_MEDIA_ELEMENT, NULL, FALSE);
			if (m_element->GetFramesDocument())
				element->MarkExtraDirty(m_element->GetFramesDocument());
		}

		// Set listener.
		media_element->SetListener(this);
		media_element->EnableControls(TRUE);

		// Set HTML_Element attributes - from now on 'media_element' is owned by 'element'.
		if (!new_element->SetMedia(Markup::MEDA_COMPLEX_JIL_MEDIA_ELEMENT, media_element, TRUE))
		{
			OP_DELETE(media_element);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (dom_element->GetFramesDocument())
			new_element->MarkExtraDirty(dom_element->GetFramesDocument());
		m_element = dom_element;
	}
	else
	{
		if (element) // if something was already set then keep it - make a dummy element fr it
		{
			JILMediaElement* old_media_element = GetJILMediaElement(FALSE);
			OP_ASSERT(old_media_element);
			m_element = NULL;
			JILMediaElement* new_media_element = GetJILMediaElement(TRUE);
			RETURN_OOM_IF_NULL(new_media_element);
			OP_STATUS err = new_media_element->TakeOver(old_media_element);
			if (OpStatus::IsError(err))
			{
				m_element = NULL; // So that the dummy element is cleaned by GC
				return err;
			}
			element->SetMedia(Markup::MEDA_COMPLEX_JIL_MEDIA_ELEMENT, NULL, FALSE);
			if (m_element->GetFramesDocument())
				element->MarkExtraDirty(m_element->GetFramesDocument());
		}
	}
	return OpStatus::OK;
}

#ifdef SELFTEST
/* virtual */
int DOM_JILMediaPlayer::GetState(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(m_state < PLAYER_STATE_LAST);
	OP_ASSERT(m_state >= PLAYER_STATE_FIRST);

	DOMSetString(return_value, m_state_strings[static_cast<int>(m_state)]);
	return ES_VALUE;
}
#endif // SELFTEST

// MediaPlayerListener implementation.

/* virtual */
void DOM_JILMediaPlayer::OnOpen(BOOL error)
{
	if (m_open_thread)
	{
		if (m_open_thread->IsBlocked())	// If we are in a call to open?
			m_open_thread->Unblock();
	}

	m_open_failed = error;
}

/* virtual */
void DOM_JILMediaPlayer::OnPlaybackEnd()
{
	JILMediaElement *media_element = GetJILMediaElement();

	if (media_element && (m_loops_left > 0 || m_loops_left == INFINITE_LOOP))
	{
		RETURN_VOID_IF_ERROR(media_element->Play(FALSE));			// No error reporting in JIL.
		if (m_loops_left != INFINITE_LOOP)
			--m_loops_left;
	}
	else
		SetState(PLAYER_STATE_COMPLETED);
}


/* virtual */ OP_STATUS
DOM_JILMediaPlayer::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_SCHEDULER_TERMINATED:
		break; // Doesn't matter to us.
	case ES_SIGNAL_CANCELLED:
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
		if (m_open_thread == thread)
			m_open_thread = NULL;
		Remove();	// Prevent deletion of this object.
		break;
	default:
		OP_ASSERT(FALSE);
	}
	return OpStatus::OK;
}

/* virtual */
void DOM_JILMediaPlayer::OnBeforeEnvironmentDestroy()
{
	m_element = NULL;
}

JILMediaElement * DOM_JILMediaPlayer::GetJILMediaElement(BOOL construct)
{
	JILMediaElement *media_element = NULL;
	if (!m_element && construct)
	{
		DOM_HTMLElement * new_element;
		// The document node should exist here, but just to make sure...
		RETURN_VALUE_IF_ERROR(GetEnvironment()->ConstructDocumentNode(), NULL);
		RETURN_VALUE_IF_ERROR(DOM_HTMLElement::CreateElement(new_element, static_cast<DOM_Node*>(GetEnvironment()->GetDocument()), UNI_L("object")), NULL);

		// If this fails node node will be garbage collected next time garbage collector is run.
		OP_STATUS result = SetWindowInternal(static_cast<DOM_Element*>(new_element));
		if (OpStatus::IsError(result))
			return NULL;
	}
	HTML_Element *element = m_element ? m_element->GetThisElement() : NULL;
	if (element)
		media_element = static_cast<JILMediaElement*>(element->GetSpecialAttr(Markup::MEDA_COMPLEX_JIL_MEDIA_ELEMENT, ITEM_TYPE_COMPLEX, NULL, SpecialNs::NS_MEDIA));

	return media_element;
}

#endif // MEDIA_JIL_PLAYER_SUPPORT
