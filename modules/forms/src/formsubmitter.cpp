/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/forms/src/formsubmitter.h"

#include "modules/forms/src/formiterator.h"
#include "modules/forms/form.h"
#include "modules/forms/formsenum.h"
#include "modules/forms/webforms2support.h"
#ifdef FORMS_KEYGEN_SUPPORT
# include "modules/forms/formvaluekeygen.h"
# include "modules/libssl/sslv3.h"
# include "modules/libssl/ssl_api.h"
# include "modules/libssl/keygen_tracker.h"
# include "modules/libssl/sslopt.h"
# include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
#endif // FORMS_KEYGEN_SUPPORT

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc_util.h" // GetCurrentBaseTarget
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dom/domutils.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esterm.h"

FormSubmitter::FormSubmitter(FramesDocument* frames_doc,
							 HTML_Element* form_element,
							 HTML_Element* submit_element,
							 int offset_x, int offset_y,
							 ES_Thread* thread,
							 BOOL triggered_by_script,
							 ShiftKeyState modifiers)
							:
		m_frames_doc(frames_doc),
		m_form_element(form_element),
		m_submit_element(submit_element),
		m_offset_x(offset_x),
		m_offset_y(offset_y),
		m_thread(thread),
		m_triggered_by_script(triggered_by_script),
		m_modifiers(modifiers),
		m_has_protected(FALSE)
#ifdef FORMS_KEYGEN_SUPPORT
		, m_has_started_key_generation(FALSE)
		, m_next(NULL)
		, m_blocking_keygenerator_count(0)
		, m_keygenerator_aborted(FALSE)
		, m_keygen_generators(NULL)
		, m_mh(NULL)
#endif // FORMS_KEYGEN_SUPPORT
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(form_element);
	OP_ASSERT(form_element->IsMatchingType(HE_FORM, NS_HTML) ||
		form_element->IsMatchingType(HE_ISINDEX, NS_HTML));
}

OP_STATUS FormSubmitter::ProtectHTMLElements(BOOL protect)
{
	OP_ASSERT(!protect || !m_has_protected);
	OP_ASSERT(protect || m_has_protected);
	DOM_Object* form_node;
	DOM_Object* submit_node = NULL;
	DOM_Environment* env = m_frames_doc->GetDOMEnvironment();
#ifdef FORMS_KEYGEN_SUPPORT
	if (!env)
	{
		// Create an environment to protect our elements. This is very expensive
		// but compared to creating a key, extremely cheap.
		if (OpStatus::IsError(m_frames_doc->ConstructDOMEnvironment()))
		{
			// Couldn't create a dom environment so nothing protects the elements
			// from being deleted, except that if we have no dom environment
			// and thus no scripts, it isn't that easy for someone to delete
			// them.
			// FIXME: Create a permanent protection of the elements, not based
			// on DOM
			return OpStatus::OK; // Will keep going with the keygen without protection
		}
		env = m_frames_doc->GetDOMEnvironment();
	}
#else
	// Can only come here to handle ONINVALID events in DOM and therefore we know that
	// we have an environment.
#endif // FORMS_KEYGEN_SUPPORT
	OP_ASSERT(env); // This isn't called unless DOM is involved
	if (OpStatus::IsError(env->ConstructNode(form_node, m_form_element)) ||
		m_submit_element && OpStatus::IsError(env->ConstructNode(submit_node, m_submit_element)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	ES_Object* form_obj = DOM_Utils::GetES_Object(form_node);
	ES_Object* submit_obj = submit_node ? DOM_Utils::GetES_Object(submit_node) : NULL;
	if (protect)
	{
		if (!env->GetRuntime()->Protect(form_obj))
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (submit_obj && !env->GetRuntime()->Protect(submit_obj))
		{
			env->GetRuntime()->Unprotect(form_obj);
			return OpStatus::ERR_NO_MEMORY;
		}
		m_has_protected = TRUE;
	}
	else
	{
		env->GetRuntime()->Unprotect(form_obj);
		if (submit_obj)
		{
			env->GetRuntime()->Unprotect(submit_obj);
		}
	}

	return OpStatus::OK;
}

OP_STATUS FormSubmitter::Submit()
{
	ES_ThreadScheduler* scheduler = m_frames_doc->GetESScheduler();
	/* Pretty ugly hack to not submit a form if the current document
	   has been replaced using document.write. (jl@opera.com) */
	if (scheduler &&
		!scheduler->TestTerminatingAction(ES_OpenURLAction::FINAL,
										  ES_OpenURLAction::CONDITIONAL))
	{
		return OpStatus::ERR;
	}

	if (m_frames_doc->IsRestoringFormState(m_thread))
	{
		/* We really do not want this to submit the form as a side-effect. */
		return OpStatus::ERR;
	}

	m_frames_doc->SignalFormChangeSideEffect(m_thread);

#ifdef _WML_SUPPORT_
	if (m_frames_doc->GetHLDocProfile()
		&& m_frames_doc->GetHLDocProfile()->HasWmlContent()
		&& !FormManager::ValidateWMLForm(m_frames_doc))
	{
		return OpStatus::OK; // Abort submit
	}
#endif // _WML_SUPPORT_

	return SubmitFormStage2();
}

#ifdef FORMS_KEYGEN_SUPPORT
BOOL FormSubmitter::StartKeyGeneration()
{
	FormIterator iterator(m_frames_doc, m_form_element);
	BOOL need_to_block = FALSE;
	while (HTML_Element* control = iterator.GetNext())
	{
		if (control->IsMatchingType(HE_KEYGEN, NS_HTML))
		{
			FormValueKeyGen* keygen_value;
			keygen_value = FormValueKeyGen::GetAs(control->GetFormValue());
			unsigned int keygen_size;
			keygen_size = keygen_value->GetSelectedKeySize(control);
			if (keygen_size > 0)
			{
				const uni_char* request_type = control->GetStringAttr(ATTR_TYPE);
				SSL_Certificate_Request_Format format = SSL_Netscape_SPKAC; // default

				if(request_type && uni_stri_eq(request_type, "PKCS10"))
				{
					format = SSL_PKCS10;
				}

				OpString8 challenge8; // truncate challenge to ascii
				if (OpStatus::IsSuccess(challenge8.Set(control->GetStringAttr(ATTR_CHALLENGE))))
				{
					if (!m_mh)
					{
						m_mh = OP_NEW(FormSubmitterMessageHandler, (this, m_frames_doc->GetWindow()));
						if (!m_mh || OpStatus::IsError(m_mh->SetCallBack(this, MSG_FORMS_KEYGEN_FINISHED, 0)))
						{
							OP_DELETE(m_mh);
							m_mh = NULL;
							// return OpStatus::ERR_NO_MEMORY
							return FALSE;
						}
					}
					if (!m_keygen_generators)
					{
						m_keygen_generators = OP_NEW(OpVector<SSL_Private_Key_Generator>, ());
						if (!m_keygen_generators)
						{
							// return OpStatus::ERR_NO_MEMORY
							return FALSE;
						}
					}
					OpWindow* op_win = const_cast<OpWindow*>(m_frames_doc->GetWindow()->GetOpWindow());
					OP_ASSERT(m_mh);
					OP_ASSERT(m_keygen_generators);
					URL empty_url;
					SSL_dialog_config dialog_config(op_win, m_mh, MSG_FORMS_KEYGEN_FINISHED, 0, empty_url);
					URL* action_url_p = m_form_element->GetUrlAttr(ATTR_ACTION, NS_IDX_HTML, m_frames_doc->GetLogicalDocument());
					URL action_url;
					if (action_url_p)
					{
						action_url = *action_url_p;
					}

					SSL_Options* ssl_options = NULL;
					SSL_BulkCipherType bulk_cipher_type = SSL_RSA; // Not certain
					SSL_Private_Key_Generator* keygen_generator = g_ssl_api->CreatePrivateKeyGenerator(dialog_config, action_url, format, bulk_cipher_type, challenge8, keygen_size, ssl_options);
					if (!keygen_generator || OpStatus::IsError(m_keygen_generators->Add(keygen_generator)))
					{
						OP_DELETE(keygen_generator);
						// return OpStatus::ERR_NO_MEMORY;
						return FALSE;
					}
					OP_STATUS status = keygen_generator->StartKeyGeneration();
					if (status == InstallerStatus::KEYGEN_FINISHED)
					{
						// Take the value and replace the generator with NULL to indicate that we
						// have already taken the value
						keygen_value->SetGeneratedKey(keygen_generator->GetSPKIString().CStr());
						m_keygen_generators->Replace(m_keygen_generators->GetCount()-1, NULL);
						OP_DELETE(keygen_generator);
					}
					else
					{
						m_blocking_keygenerator_count++;
						need_to_block = TRUE;
					}
				}
			}
		}
	}

	if (need_to_block)
	{
		/**
		 * Fix for bug CORE-15735.
		 *
		 * We're not really blocking for keygen in any case, but it would crash
		 * when keygen finally finished a long time later and the thread had
		 * terminated.
		 * So set thread to NULL.  It makes Opera think the submit was not
		 * script-initiated, so some side effects may be experienced.
		 */
		m_thread = NULL;

		FormSubmitter* stored_submitter = OP_NEW(FormSubmitter, (*this));
		if (!stored_submitter)
		{
			// return OpStatus::ERR_NO_MEMORY;
			return FALSE;
		}

#ifdef _DEBUG
		int debug_submit_count_before = 0;
		FormSubmitter* debug_it = g_opera->forms_module.m_submits_in_progress;
		while (debug_it)
		{
			debug_submit_count_before++;
			debug_it = debug_it->m_next;
		}
#endif // _DEBUG

		FormSubmitter** last = &g_opera->forms_module.m_submits_in_progress;
		while (*last)
		{
			last = &(*last)->m_next;
		}
		OP_ASSERT(!*last);
		*last = stored_submitter;
#ifdef _DEBUG
		OP_ASSERT(g_opera->forms_module.m_submits_in_progress);
		int debug_submit_count_after = 0;
		debug_it = g_opera->forms_module.m_submits_in_progress;
		while (debug_it)
		{
			debug_submit_count_after++;
			debug_it = debug_it->m_next;
		}
		OP_ASSERT(debug_submit_count_after = debug_submit_count_before+1);
#endif // _DEBUG

		if (!m_has_protected)
		{
			OP_STATUS status = stored_submitter->ProtectHTMLElements(TRUE);
			if (OpStatus::IsError(status))
			{
				// Couldn't protect elements, so we have to give up
				OP_DELETE(stored_submitter);
				return FALSE;
			}
		}
	}
	return need_to_block;
}
#endif // FORMS_KEYGEN_SUPPORT

OP_STATUS FormSubmitter::SubmitFormStage2()
{
#ifdef WAND_SUPPORT
	// Hack to avoid wand appear 2 times when several things submits
	// the form. (Which is a bug itself but almost impossible to
	// avoid)
	m_frames_doc->SetWandSubmitting(TRUE);
#endif // WAND_SUPPORT

#ifdef FORMS_KEYGEN_SUPPORT
	// Now check if we have to generate any keys
	if (!m_has_started_key_generation)
	{
		m_has_started_key_generation = TRUE;
		if (StartKeyGeneration())
		{
			// There were keygen tags and now we have to wait for them to be
			// processed. A FormSubmitter object is registered and protected
			// and we just need to wait for the callback.
			return OpStatus::OK;
		}
	}
#endif // FORMS_KEYGEN_SUPPORT

	const URL& url = m_frames_doc->GetURL();

	Form form(url,
			  m_form_element, m_submit_element, m_offset_x, m_offset_y);

	OP_STATUS status = OpStatus::OK;
	URL form_url = form.GetURL(m_frames_doc, status);
	RETURN_IF_MEMORY_ERROR(status);

	BOOL shift_pressed = (m_modifiers & SHIFTKEY_SHIFT) != 0;
	BOOL control_pressed = (m_modifiers & SHIFTKEY_CTRL) != 0;
	if (!m_triggered_by_script)
	{
		m_thread = NULL;
	}

	const uni_char* win_name;
	// First look at the submit button target, fallback on the form
	// |target| is the event target, not the submit target
	win_name = m_submit_element ? m_submit_element->GetStringAttr(ATTR_FORMTARGET) : NULL;
	if (!win_name || !*win_name)
	{
		win_name = m_form_element->GetTarget(); // form target
		if (!win_name || !*win_name)
		{
			// Global method in doc_util.cpp
			win_name = GetCurrentBaseTarget(m_form_element);
		}
	}

	return status = m_frames_doc->MouseOverURL(form_url, win_name, ONSUBMIT,
		shift_pressed, control_pressed, m_thread, NULL);
}

// Used by FormManager::Submit when we decide that we need a heap allocated FormSubmitter
FormSubmitter::FormSubmitter(FormSubmitter& other) :
#ifdef FORMS_KEYGEN_SUPPORT
	MessageObject(),
#endif // FORMS_KEYGEN_SUPPORT
	m_frames_doc(other.m_frames_doc),
	m_form_element(other.m_form_element),
	m_submit_element(other.m_submit_element),
	m_offset_x(other.m_offset_x),
	m_offset_y(other.m_offset_y),
	m_thread(other.m_thread),
	m_triggered_by_script(other.m_triggered_by_script),
	m_modifiers(other.m_modifiers),
	m_has_protected(FALSE)
#ifdef FORMS_KEYGEN_SUPPORT
	, m_has_started_key_generation(other.m_has_started_key_generation)
	, m_next(NULL)
	, m_blocking_keygenerator_count(other.m_blocking_keygenerator_count)
	, m_keygenerator_aborted(other.m_keygenerator_aborted)
	, m_keygen_generators(other.m_keygen_generators)
	, m_mh(other.m_mh)
#endif // FORMS_KEYGEN_SUPPORT
{
#ifdef FORMS_KEYGEN_SUPPORT
	OP_ASSERT(!other.m_next); // Can only steal free FormSubmitters and this is a simple check
	// Steal the other messagehandler
	other.m_keygen_generators = NULL;
	if (m_mh)
	{
		// move callback
		m_mh->RemoveCallBacks(&other, 0);
		// This shouldn't be able to fail since we just replace one pointer with another
		// but we should still write this more robust in case the MessageHandler implementation
		// changes and for instance deallocates stuff in the RemoveCallbacks call.
		OP_STATUS status = m_mh->SetCallBack(this, MSG_FORMS_KEYGEN_FINISHED, 0);
		OP_ASSERT(OpStatus::IsSuccess(status));
		OpStatus::Ignore(status);
		other.m_mh = NULL;
	}
	other.m_blocking_keygenerator_count = 0;
#endif // FORMS_KEYGEN_SUPPORT
}

void FormSubmitter::Abort()
{
	if (m_has_protected)
	{
		OpStatus::Ignore(ProtectHTMLElements(FALSE));
		m_has_protected = FALSE;
	}

	m_frames_doc = NULL;
}

#ifdef FORMS_KEYGEN_SUPPORT
/* virtual */
void FormSubmitter::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_FORMS_KEYGEN_FINISHED);
	OP_ASSERT(par1 == 0);
	OP_ASSERT(par2 == 0 || par2 == 1);
	OP_ASSERT(m_blocking_keygenerator_count > 0);

	BOOL completed_successfully = !!par2;
	if (!completed_successfully || !m_frames_doc /* FormSubmitter::Abort() called */)
	{
		m_keygenerator_aborted = TRUE;
	}

	m_blocking_keygenerator_count--;
	if (m_blocking_keygenerator_count == 0)
	{
		// Let it go on.

		// Unhook from the global list
#ifdef _DEBUG
		FormSubmitter* check_list = g_opera->forms_module.m_submits_in_progress;
		int debug_submit_count_before = 0;
		BOOL found_it = FALSE;
		while (check_list)
		{
			if (check_list == this)
			{
				OP_ASSERT(!found_it);
				found_it = TRUE;
			}
			debug_submit_count_before++;
			check_list = check_list->m_next;
		}
		OP_ASSERT(found_it);
#endif // _DEBUG

		FormSubmitter** submitter_ptr = &g_opera->forms_module.m_submits_in_progress;
		while (*submitter_ptr != this)
		{
			submitter_ptr = &(*submitter_ptr)->m_next;
		}

		// Unhook
		*submitter_ptr = (*submitter_ptr)->m_next;

#ifdef _DEBUG
		check_list = g_opera->forms_module.m_submits_in_progress;
		int debug_submit_count_after = 0;
		while (check_list)
		{
			OP_ASSERT(check_list != this);
			debug_submit_count_after++;
			check_list = check_list->m_next;
		}
		OP_ASSERT(debug_submit_count_after+1 == debug_submit_count_before);
#endif // _DEBUG

		// Unhooked, so lets insert all the generated values into the FormValueKeygen objects

		if (!m_keygenerator_aborted)
		{
			FormIterator iterator(m_frames_doc, m_form_element);

			unsigned keygen_index = 0;
			while (HTML_Element* control = iterator.GetNext())
			{
				if (control->IsMatchingType(HE_KEYGEN, NS_HTML))
				{
					FormValueKeyGen* keygen_value;
					keygen_value = FormValueKeyGen::GetAs(control->GetFormValue());
					unsigned int keygen_size;
					keygen_size = keygen_value->GetSelectedKeySize(control);
					if (keygen_size > 0)
					{
						keygen_index++;
						if (!m_keygen_generators || m_keygen_generators->GetCount() < keygen_index)
						{
							// ouch, we're missing parts. OOM? Someone tampered with the key size choices?
						}
						else
						{
							SSL_Private_Key_Generator* key_generator = m_keygen_generators->Get(keygen_index-1);
							if (key_generator) // NULL key generator means that we got the key directly for this HE_KEYGEN
							{
								keygen_value->SetGeneratedKey(key_generator->GetSPKIString().CStr());
							}
						}
					}
				}
			}

			// Let the submit proceed
			SubmitFormStage2();
		}
		OP_DELETE(this);
	}
	// Don't add code here. This object is likely deleted.
}
#endif // FORMS_KEYGEN_SUPPORT



FormSubmitter::~FormSubmitter()
{
	if (m_has_protected)
	{
		OpStatus::Ignore(ProtectHTMLElements(FALSE));
	}
#ifdef FORMS_KEYGEN_SUPPORT
	if (m_keygen_generators)
	{
		m_keygen_generators->DeleteAll();
		OP_DELETE(m_keygen_generators);
	}
	OP_DELETE(m_mh);
#endif // defined FORMS_KEYGEN_SUPPORT
}

#ifdef ALLOW_TOO_LONG_FORM_VALUES
/* static */
BOOL FormSubmitter::HandleTooLongValue(FramesDocument* frames_doc, ValidationResult val_res)
{
	val_res.ClearError(VALIDATE_ERROR_TOO_LONG);
	if (val_res.IsOk())
	{
#ifdef OPERA_CONSOLE
		// Since we break against the specification to keep
		// bad pages working, we'll log an error so that
		// the authors can be made aware that their
		// pages aren't completely ok. This shouldn't
		// catch user errors since maxlength is enforced
		// by the widget by not letting the user insert more
		// text than maxlength.
		frames_doc->EmitError(UNI_L("Form had field with violated maxlength property. Submitted anyway for compatibility reasons."), NULL);
#endif // OPERA_CONSOLE
		return TRUE;
	}
	return FALSE;
}
#endif // ALLOW_TOO_LONG_FORM_VALUES

