/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/forms/formmanager.h"

#include "modules/forms/src/formiterator.h"
#include "modules/forms/webforms2support.h"
#include "modules/forms/form.h"
#include "modules/forms/piforms.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvalueemail.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/forms/formsenum.h"
#include "modules/forms/src/formsubmitter.h"

#ifdef WAND_SUPPORT
#include "modules/wand/wandmanager.h"
#endif // WAND_SUPPORT
#include "modules/logdoc/htm_elm.h"
#include "modules/locale/locale-enum.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/formats/argsplit.h"
#include "modules/idna/idna.h"

#ifdef _WML_SUPPORT_
# include "modules/spatial_navigation/handler_api.h"
# include "modules/forms/src/validationerrorwindow.h"
# include "modules/locale/oplanguagemanager.h"
#endif // _WML_SUPPORT_

#ifdef _PLUGIN_SUPPORT_
#include "modules/ns4plugins/opns4plugin.h"
#include "modules/layout/box/box.h" // For checking IsContentReplaced
#endif // _PLUGIN_SUPPORT_

/* static */
OP_STATUS FormManager::ResetForm(FramesDocument* frames_doc,
								 HTML_Element* form_element)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(form_element);
	OP_ASSERT(form_element->IsMatchingType(HE_FORM, NS_HTML) ||
		form_element->IsMatchingType(HE_ISINDEX, NS_HTML));

	FormIterator iterator(frames_doc, form_element);
	iterator.SetIncludeOutputElements();

	while (HTML_Element* elm = iterator.GetNext())
	{
		FormValue* value = elm->GetFormValue();
		if (elm->IsMatchingType(HE_OUTPUT, NS_HTML))
		{
			// The reset for output doesn't work without a FramesDocument :-(
			FormValueOutput* form_value;
			form_value = FormValueOutput::GetAs(value);
			RETURN_IF_MEMORY_ERROR(form_value->ResetOutputToDefault(elm, frames_doc));
		}
		else
		{
			RETURN_IF_MEMORY_ERROR(value->ResetToDefault(elm));
		}
		// Should ONINPUT or ONCHANGED be sent? We don't do it now, and this might
		// be called from scripts which normally won't trigger events.
		RETURN_IF_MEMORY_ERROR(FormValueListener::HandleValueChanged(frames_doc, elm, FALSE, FALSE, NULL));
		value->ResetChangedFromOriginal();
	}
	FormValidator val(frames_doc);
	val.DispatchFormChange(form_element);

	return OpStatus::OK;
}

/* static */
OP_STATUS FormManager::SubmitForm(FramesDocument* frames_doc,
								  HTML_Element* form_element,
								  HTML_Element* submit_element,
								  int offset_x, int offset_y,
								  ES_Thread* thread,
								  BOOL synthetic,
							      ShiftKeyState modifiers)
{
	// related_target = NULL -> auto submit by pressing ENTER or script submit()
	// synthetic = TRUE -> scripts were involved
	// x = -1 -> click at the left edge or script submit
	// together they form a strong case against submit(). The closest non-submit() would be a submit from ENTER in a form without any submit button
	BOOL is_script_submit = submit_element == NULL && synthetic && offset_x == -1;
	if (is_script_submit)
		offset_x = 0; // reset the signal, not that it matters since it's only used when submitting images and related_target == NULL so that wasn't the case

	FormSubmitter submitter(frames_doc, form_element,
		submit_element, offset_x, offset_y,
		thread, is_script_submit, modifiers);
	return submitter.Submit();
}


OP_STATUS FormManager::SubmitFormFromInputField(FramesDocument* frames_doc,
											   HTML_Element* input_field,
											   ShiftKeyState modifiers)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(input_field);
#ifdef _DEBUG
	if (input_field->GetInputType() == INPUT_TEXT ||
		input_field->GetInputType() == INPUT_EMAIL ||
		input_field->GetInputType() == INPUT_NUMBER ||
		input_field->GetInputType() == INPUT_SEARCH ||
		input_field->GetInputType() == INPUT_TEL ||
		input_field->GetInputType() == INPUT_URI ||
		input_field->GetInputType() == INPUT_PASSWORD)
	{
	}
	else
	{
		OP_ASSERT(!"Unexpected form field type");
	}
#endif // _DEBUG

	// Send ONCHANGE if the value has changed since the last ONCHANGE sent. See bug CORE-22914.
	if (input_field->GetFormValue()->IsChangedFromOriginalByUser(input_field))
	{
		// Hard to know if anything has happened since the last onchange event
		// but better send one event too many than too few
		RETURN_IF_ERROR(FormValueListener::HandleValueChanged(frames_doc, input_field, TRUE, TRUE, NULL));
	}

    FormObject *form_obj = input_field->GetFormObject();
    HTML_Element *default_button;
	default_button = form_obj ? form_obj->GetDefaultElement() : NULL;

    if (default_button)
    {
		if (!default_button->IsDisabled(frames_doc))
		{
			OP_STATUS status = frames_doc->HandleMouseEvent(ONCLICK, NULL, default_button, NULL, 0, 0, 0, 0, modifiers, 0);
			RETURN_IF_ERROR(status);
		}
    }
    else
    {
		// Auto submit the form. This is in MSIE and Mozilla only done
		// for single field forms. See bug 232124
		HTML_Element *form_elm;
		form_elm = FormManager::FindFormElm(frames_doc, input_field);

		if (form_elm)
		{
			// First check if it's a single element form, only type="text" counts
			FormIterator iterator(frames_doc, form_elm);
			int count = 0;
			while (HTML_Element* control = iterator.GetNext())
			{
				InputType inputtype = control->Type() == HE_INPUT ? control->GetInputType() : INPUT_NOT_AN_INPUT_OBJECT;
				if (inputtype == INPUT_TEXT ||
					inputtype == INPUT_EMAIL ||
					inputtype == INPUT_NUMBER ||
					inputtype == INPUT_SEARCH ||
					inputtype == INPUT_TEL ||
					inputtype == INPUT_URI)
				{
					if (count == 1) // More than one field
					{
						return OpStatus::OK;
					}
					count++;
				}
			}

#ifdef WAND_SUPPORT
			const BOOL submit_through_wand = !frames_doc->GetWindow()->GetPrivacyMode();

			if (submit_through_wand)
			{
				ES_Thread *thread = NULL;

				if (ES_ThreadScheduler *scheduler = frames_doc->GetESScheduler())
				{
					if (scheduler->IsActive())
					{
						thread = scheduler->GetCurrentThread();
					}
					else
					{
						thread = OP_NEW(ES_EmptyThread, ());

						OP_BOOLEAN result;
						if (!thread || OpStatus::IsMemoryError(result = scheduler->AddRunnable(thread)))
						{
							return OpStatus::ERR_NO_MEMORY;
						}

						if (result != OpBoolean::IS_TRUE)
							thread = NULL;
						else
						{
							/* Theory: this comes from an input action, and script generated
							   key events are never translated into input actions or otherwise
							   handled outside DOM, so this must be a user generated key event,
							   and thus requested. */
							thread->SetIsUserRequested();

							/* Normally, on Mac, the meaning of shift and control are reversed,
							   but this shift key state come from a translation in
							   VisualDevice::OnInputAction, where no such reversal is done. */
							if (modifiers & SHIFTKEY_SHIFT)
								thread->SetOpenInNewWindow();
							if (modifiers & SHIFTKEY_CTRL)
								thread->SetOpenInBackground();
						}
					}
				}

				RETURN_IF_MEMORY_ERROR(g_wand_manager->SubmitPage(frames_doc, NULL, form_elm, 0, 0, 0, 0, modifiers, FALSE, thread));
			}
			else
#endif//WAND_SUPPORT
			{
				RETURN_IF_MEMORY_ERROR(frames_doc->HandleEvent(ONSUBMIT, NULL, form_elm, modifiers));
			}
		}
	}

	// FIXME: Remove this code, move it to the desktop browser or make it into a tweak
#ifdef QUICK
	// shift -> open in other tab
	// shift+ctrl -> open in other tab in background
	if (modifiers & SHIFTKEY_SHIFT && !(modifiers & SHIFTKEY_CTRL))
	{
		// The page was opened in another tab and it's likely that the
		// user want to switch there immediately. Focus page instead
		// of text field so that pagecycling with "1" or "2" won't
		// type in the current inputfield.
		g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
	}
#endif // QUICK

	return OpStatus::OK;
}


#ifdef _WML_SUPPORT_
BOOL FormManager::IsWmlSelected(HTML_Element* elm, INT32 index)
{
	if (elm->Type() != HE_OPTION)
		return FALSE;

	// get the HE_SELECT parent
	HTML_Element* sel_elm =elm->Parent();

	while(sel_elm && sel_elm->Type() != HE_SELECT)
		sel_elm = sel_elm->Parent();

	if (!sel_elm)
		return FALSE;

	const uni_char* value = sel_elm->GetWmlValue();
	const uni_char* ivalue = sel_elm->GetAttrValue(WA_IVALUE, NS_IDX_WML);

	// check: which is the priority between value/ivalue if both specified /pw
	if (value)
	{
		const uni_char* option_value = elm->GetValue();

		if (option_value)
		{
			const uni_char* found = uni_strstr(value, option_value);
			while (found)
			{
				const uni_char* delim = uni_strchr(found, ';');
				UINT32 option_strlen = uni_strlen(option_value);

				if (option_strlen == uni_strlen(found) ||
					(delim && (option_strlen == (UINT32)(delim - found))))
				{
					return TRUE;
				}

				found = delim ? uni_strstr(delim + 1, option_value) : NULL;
			}
		}
	}
	else if (ivalue)
	{
		const uni_char *temp = ivalue;
		int i = uni_atoi(temp);

		while (i && index > i)
		{
			temp = uni_strchr(temp, ';');

			if (temp)
			{
				temp++;
				i = uni_atoi(temp);
			}
			else
				i = 0;
		}

		if (index == i)
			return TRUE;
	}

	return FALSE;
}


/** Check whether the given data char is within the set identified by the given format char.
 *
 * See WML spec (WAP-238-WML-20010626-p) section 6.5.2.1 for more information.
 *
 * @param format_code Format character identifying set of characters to match data char against. Must be one of [AaNnXxMm]
 * @param data Character to check against the above format specifier.
 * @return TRUE iff data char is within format_code's acceptable set of characters. FALSE otherwise. If format_code is not in [AaNnXxMm], FALSE is returned.
 */
static inline BOOL IsMatchingWmlFormatCode (uni_char format_code, uni_char data)
{
	CharacterClass cc = Unicode::GetCharacterClass(data);
	switch (format_code)
	{
	case 'A':
		if (cc!=CC_Lu && cc!=CC_Lm && //Uppercase
			cc!=CC_Sm && cc!=CC_Sc && cc!=CC_Sk && cc!=CC_So && //Symbol
			cc!=CC_Pc && cc!=CC_Pd && cc!=CC_Ps && cc!=CC_Pe && cc!=CC_Po) //Punctuation
			return FALSE;
		break;
	case 'a':
		if (cc!=CC_Ll && cc!=CC_Lm && //Lowercase
			cc!=CC_Sm && cc!=CC_Sc && cc!=CC_Sk && cc!=CC_So && //Symbol
			cc!=CC_Pc && cc!=CC_Pd && cc!=CC_Ps && cc!=CC_Pe && cc!=CC_Po) //Punctuation
			return FALSE;
		break;
	case 'N':
		if (cc!=CC_Nd) //Numeric
			return FALSE;
		break;
	case 'n':
		if (cc!=CC_Nd && //Numeric
			cc!=CC_Sm && cc!=CC_Sc && cc!=CC_Sk && cc!=CC_So && //Symbol
			cc!=CC_Pc && cc!=CC_Pd && cc!=CC_Ps && cc!=CC_Pe && cc!=CC_Po) //Punctuation
			return FALSE;
		break;
	case 'X':
		if (cc!=CC_Lu && cc!=CC_Lm && //Uppercase
			cc!=CC_Nd && //Numeric
			cc!=CC_Sm && cc!=CC_Sc && cc!=CC_Sk && cc!=CC_So && //Symbol
			cc!=CC_Pc && cc!=CC_Pd && cc!=CC_Ps && cc!=CC_Pe && cc!=CC_Po) //Punctuation
			return FALSE;
		break;
	case 'x':
		if (cc!=CC_Ll && cc!=CC_Lm && //Lowercase
			cc!=CC_Nd && //Numeric
			cc!=CC_Sm && cc!=CC_Sc && cc!=CC_Sk && cc!=CC_So && //Symbol
			cc!=CC_Pc && cc!=CC_Pd && cc!=CC_Ps && cc!=CC_Pe && cc!=CC_Po) //Punctuation
			return FALSE;
		break;
	case 'M':
	case 'm':
		break;
	default: // Invalid format code.
		return FALSE;
	}

	return TRUE;
}

/** Validate the given data string against the WML input data mask associated with this element.
 *
 * See WML spec (WAP-238-WML-20010626-p) section 6.5.2 for more information.
 *
 * If no input mask exists, or the input mask is invalid TRUE is returned.
 * Only if the input data mask is valid, and the given data violates the input
 * mask will FALSE be returned.
 *
 * @param data String on which to perform validation
 * @return FALSE iff data violates this element's input data mask, TRUE otherwise.
 */
BOOL FormManager::ValidateWmlInputData(HTML_Element* elm, const uni_char* data, BOOL is_typing/*=FALSE*/)
{
	OP_NEW_DBG("FormManager::ValidateWmlInputData()", "wml");

	// Retrieve input mask, if any, on this element.
	const uni_char* mask = elm->GetAttrValue(WA_FORMAT, NS_IDX_WML);

	if (!mask)
	{
		mask = elm->GetAttrValue(UNI_L("FORMAT"), NS_IDX_HTML, HE_UNKNOWN, NULL);
	}

	OP_DBG((UNI_L("FormManager::ValidateWmlInputData(): Validating data = \"%s\"."), data));
	OP_DBG((UNI_L("FormManager::ValidateWmlInputData():    against mask = \"%s\"."), mask));

	if (!(data && *data))
	{ // No data given; check emptyok attribute, if present.
		if (elm->HasAttr(WA_EMPTYOK, NS_IDX_WML))
		{
			BOOL emptyok = elm->GetBoolAttr(WA_EMPTYOK, NS_IDX_WML);
			OP_DBG(("FormManager::ValidateWmlInputData(): Returning because of empty data and emptyok == %s.", emptyok ? "TRUE" : "FALSE"));
			return emptyok;
		}
		data = UNI_L(""); // Otherwise, continue, but make sure that data is a valid pointer, even if no data.
	}

	if (!mask || !*mask)
	{
		return TRUE; // No mask -> no format limitations.
	}

	// Ready to start parsing.
	OP_ASSERT(mask);
	OP_ASSERT(data);

#define RETURN_INVALID_MASK(why) do { \
	OP_DBG(("FormManager::ValidateWmlInputData(): Invalid mask (%s). Returning success.", why)); \
	return TRUE; \
} while (0)
#define RETURN_FAIL_PARSING(why) do { \
	OP_DBG(("FormManager::ValidateWmlInputData(): Parsing failed! (%s)", why)); \
	return FALSE; \
} while (0)

	// Start parsing
	unsigned int mask_index = 0;
	unsigned int data_index = 0;
	while (uni_char m = mask[mask_index]) // Retrieve (start of) next atom in mask.
	{
		mask_index++;

		if (uni_strchr(UNI_L("AaNnXxMm"), m))
		{ // Format code.
			if (!data[data_index])
			{
				RETURN_FAIL_PARSING("Premature end of data.");
			}
			if (!IsMatchingWmlFormatCode(m, data[data_index++]))
			{
				RETURN_FAIL_PARSING("Data char fails to match format code.");
			}
		}
		else if (m == '<')
		{ // <lang:class>. Unsupported. Treat as format code 'M'.
			const uni_char *skip_to = uni_strchr(mask + mask_index, '>'); // Find end of <lang:class> code.
			if (!skip_to)
			{
				RETURN_INVALID_MASK("Failed to find end of <lang:class> code.");
			}

			mask_index = (skip_to - mask) + 1; // Skip to char after <lang:class> code.

			if (!data[data_index])
			{
				RETURN_FAIL_PARSING("Premature end of data.");
			}

			if (!IsMatchingWmlFormatCode('M', data[data_index++]))
			{
				RETURN_FAIL_PARSING("Data char fails to match <lang:class> code.");
			}
		}
		else if (m == '*')
		{ // Entry of any number of chars. Format code follows '*'.
			m = mask[mask_index++]; // Format code.

			if (!m)
			{
				RETURN_INVALID_MASK("*-sequence ends prematurely.");
			}
			if (!uni_strchr(UNI_L("AaNnXxMm"), m))
			{
				RETURN_INVALID_MASK("Invalid format code in *-sequence.");
			}
			if (mask[mask_index] != '\0')
			{
				RETURN_INVALID_MASK("*-sequence is not at end of input mask.");
			}

			while (data[data_index])
			{ // For each remaining char in data.
				if (!IsMatchingWmlFormatCode(m, data[data_index]))
				{
					RETURN_FAIL_PARSING("Data char fails to match *-sequence format code.");
				}
				data_index++; // Go to next data char.
			}
		}
		else if (uni_isdigit(m))
		{ // Entry of up to n chars. Format code follows digit(s).
			uni_char *skip_to;
			unsigned long num = uni_strtoul(mask + mask_index - 1, &skip_to, 10); // Parse number
			OP_ASSERT(skip_to >= mask + mask_index); // Verify that at least one digit was parsed.
			mask_index = skip_to - mask; // Skip to char after parsed digits.
			m = mask[mask_index++]; // Format code.

			if (!m)
			{
				RETURN_INVALID_MASK("n-sequence ends prematurely.");
			}

			if (!uni_strchr(UNI_L("AaNnXxMm"), m))
			{
				RETURN_INVALID_MASK("Invalid format code in n-sequence.");
			}

			if (mask[mask_index] != '\0')
			{
				RETURN_INVALID_MASK("n-sequence is not at end of input mask.");
			}

			while (data[data_index] && num-- > 0)
			{ // For each remaining char in data, limited by num.
				if (!IsMatchingWmlFormatCode(m, data[data_index]))
				{
					RETURN_FAIL_PARSING("Data char fails to match n-sequence format code.");
				}
				data_index++; // Go to next data char.
			}
		}
		else if (m == '\\')
		{ // Escape sequence. Next mask character must be matched literally.
			m = mask[mask_index++];
			if (!m)
			{
				RETURN_INVALID_MASK("Escape sequence ends prematurely.");
			}

			if (!data[data_index])
			{
				RETURN_FAIL_PARSING("Premature end of data.");
			}

			if (m != data[data_index++])
			{
				RETURN_FAIL_PARSING("Failed to match exact character.");
			}
		}
		else
		{ // Invalid mask character.
			RETURN_INVALID_MASK("Invalid mask character.");
		}
	}

	OP_ASSERT(mask[mask_index] == '\0'); // We're at end of mask.
	if (data[data_index] != '\0')
	{
		RETURN_FAIL_PARSING("Redundant non-matched chars at end of data.");
	}

	OP_DBG(("FormManager::ValidateWmlInputData(): Parsing successful."));
	return TRUE;

#undef RETURN_INVALID_MASK
#undef RETURN_FAIL_PARSING
}

BOOL FormManager::ValidateWmlInputData(FramesDocument* frames_doc,
									   HTML_Element* elm,
									   BOOL is_typing/*=FALSE*/)
{
	OP_ASSERT(frames_doc);
	if (frames_doc &&
		frames_doc->GetHLDocProfile() &&
		(frames_doc->GetHLDocProfile()->IsWml()
			|| elm->HasAttr(WA_FORMAT, NS_IDX_WML)
			|| elm->HasAttr(WA_EMPTYOK, NS_IDX_WML)))
	{
		WML_Context *wc =	frames_doc->GetDocManager()->WMLGetContext();
		OP_ASSERT(elm->IsFormElement());

		FormValue* form_value = elm->GetFormValue();
		OpString value;
		if (form_value->HasMeaningfulTextRepresentation())
		{
			OP_STATUS status = form_value->GetValueAsText(elm, value);
			if (OpStatus::IsError(status))
			{
				// Assume it's correct
				return TRUE;
			}
		}

		const uni_char* value_p = UNI_L("");
		if (!value.IsEmpty())
		{
			value_p = value.CStr();
		}

		// validate data according to given 'format' and 'emptyok'
		if (ValidateWmlInputData(elm, value_p, is_typing))
		{
			const uni_char* name = elm->GetName();
			if (name)
				wc->SetVariable(name, value_p);
		}
		else
		{
			FormObject *form =	elm->GetFormObject();
			if (form)
			{
				// Here we used to reset the value back to its default but that will be
				// a real pita in case the user has spent 2 minutes writing something
				// with a lousy phone keyboard

				return FALSE;
			}
		}
	}

	return TRUE;
}

const uni_char* FormManager::GetWmlDefaultValue(FramesDocument* frames_doc,
												HTML_Element* elm)
{
	const uni_char* val;
	const uni_char* name = elm->GetName();
	WML_Context* context = frames_doc->GetDocManager()->WMLGetContext();
	if (name && context)
	{
		val = context->GetVariable(name);
	}
	else
	{
		val = NULL;
	}

	if (!val || !ValidateWmlInputData(elm, val))
	{
		val = elm->GetValue();

		if (val && !ValidateWmlInputData(elm, val))
			val = NULL;
	}

	return val;
}

/* static */
BOOL FormManager::ValidateWMLForm(FramesDocument* frames_doc)
{
	OP_ASSERT(frames_doc->GetDocManager()->WMLGetContext());

	HTML_Element* root = NULL;
	if (frames_doc->GetHLDocProfile())
	{
		root = frames_doc->GetHLDocProfile()->WMLGetCurrentCard();
		if (!root)
		{
			root = frames_doc->GetHLDocProfile()->GetRoot();
		}
	}
	HTML_Element* invalid_elm = NULL;
	HTML_Element* elm = root;
	HTML_Element* stop_elm = elm ? elm->NextSiblingActual() : NULL;
	while (elm != stop_elm && !invalid_elm)
	{
		if (elm->Type() == HE_INPUT)
		{
			if (!ValidateWmlInputData(frames_doc, elm))
			{
				HTML_Document* doc = frames_doc->GetHtmlDocument();
#ifdef _SPAT_NAV_SUPPORT_
				// Remove this code if setting focus in the bad text
				// field causes problems
				OpSnHandler* sn_handler = doc->GetWindow()->GetSnHandler();
				if (sn_handler)
				{
					doc->ClearFocusAndHighlight();
					sn_handler->Highlight(doc, elm, TRUE);
				}
#else
				// Uncomment this code if the bad text field should be
				// automatically opened
				doc->HighlightElement(elm,
									  FALSE /* send_event */,
									  TRUE /* focus form element */);
#endif // _SPAT_NAV_SUPPORT_
				invalid_elm = elm;
			}
		}
		elm = elm->NextActual();
	}

	if (invalid_elm && invalid_elm->GetFormObject()) // only display an error message in case the invalid form element is visible (has a FormObject)
	{
		OpString error_message;
		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_WML_INPUT_FORMAT_ERROR, error_message)) &&
			!error_message.IsEmpty())
		{
			ValidationErrorWindow::Display(invalid_elm, error_message.CStr());
		}
	}

	return invalid_elm == NULL;
}

#endif // _WML_SUPPORT_

#ifdef _PLUGIN_SUPPORT_
/* static */
OP_STATUS FormManager::GetPluginFormValue(HTML_Element* elm, OpString& output)
{
	if (elm->Type() == HE_OBJECT) // HE_EMBED/HE_APPLET?
	{
		OpNS4Plugin* plugin = elm->GetNS4Plugin();
		if (plugin)
		{
			uni_char* plugin_form_value = NULL;
			if (OpStatus::IsSuccess(plugin->GetFormsValue(plugin_form_value)))
			{
				OP_STATUS status = output.Set(plugin_form_value);
				OP_DELETEA(plugin_form_value);
				return status;
			}
		}
	}
	return OpStatus::ERR;
}
#endif // _PLUGIN_SUPPORT_


/* static */
HTML_Element* FormManager::NextFormElm(FramesDocument* frames_doc,
									   HTML_Element* form_element,
									   HTML_Element* current_form_elm,
									   BOOL include_option,
									   BOOL include_images)
{
	OP_ASSERT(form_element);
	OP_ASSERT(form_element->IsMatchingType(HE_FORM, NS_HTML));
	OP_ASSERT(current_form_elm);

	HTML_Element* he;
	// In Web Forms 2, elements can be anywhere in the document,
	// not only after/in the <form> element. If we're starting at the <form>
	// tag we move to the beginning of the document and scan from there. We
	// will never again be at the form tag because we don't stop at form tags.
	if (form_element == current_form_elm)
	{
		he = current_form_elm;
		while (he->Parent())
		{
			he = he->Parent();
		}
	}
	else
	{
		he = (HTML_Element*) current_form_elm->Next();
	}

	while (he)
	{
		HTML_ElementType he_type = he->Type();
		if (he_type == HE_TEXTAREA ||
			he_type == HE_SELECT ||
			he_type == HE_INPUT ||
			he_type == HE_BUTTON ||
			he_type == HE_FIELDSET ||
#ifdef FORMS_KEYGEN_SUPPORT
			he_type == HE_KEYGEN ||
#endif // FORMS_KEYGEN_SUPPORT
			(include_images && he_type == HE_IMG) ||
			(include_option && he_type == HE_OPTION))
		{
			if (FormManager::BelongsToForm(frames_doc, form_element, he))
				return he;
		}

		he = (HTML_Element*) he->Next();
	}
	return NULL;
}

/* static */
BOOL FormManager::BelongsToForm(FramesDocument* frames_doc,
								 HTML_Element* form,
								 HTML_Element* form_control)
{
	OP_ASSERT(form);
	OP_ASSERT(form->GetNsType() == NS_HTML);
	OP_ASSERT(form->Type() == HE_FORM || form->Type() == HE_ISINDEX);
	OP_ASSERT(form_control);
	// frames_doc can be NULL if we're called from a Forms Collection
	// from a document that has been Freed (because that document
	// is too far away in history or because the document cache is full
	// for instance).

	const uni_char* form_specified;
	form_specified = FormManager::GetFormIdString(form_control);
	if (form_specified)
	{
		// "getElementById"
		do
		{
			// Skip spaces
			while (uni_isspace(*form_specified))
				form_specified++;
			const uni_char* end = form_specified;
			while (*end && !uni_isspace(*end))
				end++;

			if (form_specified != end)
			{
				// We have an id that is supposed to be to this form
				// element's form - let's find it
				OpString id;
				// FIXME Need API that take string pointer and length
				if (OpStatus::IsError(id.Set(form_specified, end-form_specified)))
				{
					return FALSE;
				}
				HTML_Element* found_elm = NULL;
				if (frames_doc)
				{
					// Do this fast and simple and correct
					LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
					if (logdoc)
					{
						NamedElementsIterator it;
						logdoc->GetNamedElements(it, id.CStr(), TRUE, FALSE);
						found_elm = it.GetNextElement();
					}
				}
				else
				{
					// Without frames doc this will be slow and
					// painful.  All callers that use the non frames
					// doc version should be fixed
					HTML_Element* root = form_control;
					while (root->Parent())
					{
						root = root->Parent();
					}
					found_elm = root->GetElmById(id.CStr());
				}

				if (found_elm == form)
				{
					return TRUE;
				}
			}
			form_specified = end;
		} while (*form_specified);

		return FALSE;
	}

	// For XHTML the document structure is more "correct" than the
	// form number.
	if (frames_doc)
	{
		HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
		if (hld_profile && hld_profile->IsXml())
		{
			HTML_Element* parent = form_control;
			do
			{
				if (parent->IsMatchingType(HE_FORM, NS_HTML))
				{
					return parent == form;
				}
				parent = parent->Parent();
			}
			while (parent);
		}
	}

	int form_nr = form->GetFormNr(frames_doc);
	int control_form_nr = form_control->GetFormNr(frames_doc);

	if (form_nr != control_form_nr)
	{
		if (control_form_nr == -1 &&
			form_control->GetInserted() == HE_INSERTED_BY_DOM)
			/* Form control inserted by a script into a parsed FORM. */
			return form->IsAncestorOf(form_control);

		return FALSE;
	}

	if (form_nr == -1)
	{
		/* This is either a FORM created from a start tag we ignored, in
		   which case no form controls belong to it, or it is a FORM
		   inserted by a script, in which case we simply check if it is
		   an ancestor of the form control (which then inevitably also
		   was inserted by a script.) */
		if (form->GetInserted() == HE_INSERTED_BY_DOM)
			return form->IsAncestorOf(form_control);

		return FALSE;
	}

	return TRUE;
}

/*static */
BOOL FormManager::IsSelectedOption(HTML_Element* option)
{
	OP_ASSERT(option->Type() == HE_OPTION);
	// Find select
	HTML_Element* sel_elm = option->Parent();
	while (sel_elm && sel_elm->Type() != HE_SELECT)
	{
		sel_elm = sel_elm->Parent();
	}
	if (sel_elm)
	{
		FormValueList* value = FormValueList::GetAs(sel_elm->GetFormValue());
		return value->IsSelectedElement(sel_elm, option);
	}

	return FALSE;
}

/* static */
const uni_char* FormManager::GetFormIdString(HTML_Element* form_control)
{
	// The form id string if not directly specified,
	// is inherited from parent fieldsets
	// unless we're inside a form of our own inside that fieldset
	const uni_char* form_specified = form_control->GetStringAttr(ATTR_FORM);
	HTML_Element* parent = form_control->Parent();
	while (parent &&
		   !form_specified &&
		   !parent->IsMatchingType(HE_FORM, NS_HTML))
	{
		if (parent->IsMatchingType(HE_FIELDSET, NS_HTML))
		{
			form_specified = parent->GetStringAttr(ATTR_FORM);
		}
		parent = parent->Parent();
	}
	return form_specified;
}

/* static */
HTML_Element*
FormManager::FindElementFromIdString(FramesDocument* frames_doc,
										  HTML_Element* form_control,
										  const uni_char* form_id)
{
	OP_ASSERT(form_control);
	OP_ASSERT(frames_doc);
	OP_ASSERT(form_id);
	// "getElementById"

	if (*form_id)
	{
		HTML_Element* found_elm = NULL;
		if (frames_doc)
		{
			// Do this fast and simple and correct
			LogicalDocument* logdoc = frames_doc->GetLogicalDocument();
			if (logdoc)
			{
				NamedElementsIterator it;
				logdoc->GetNamedElements(it, form_id, TRUE, FALSE);
				found_elm = it.GetNextElement();
			}
		}
		else
		{
			// Without frames doc this will be slow and painful.
			// All callers that use the non frames doc version
			// should be fixed
			HTML_Element* root = form_control;
			while (root->Parent())
			{
				root = root->Parent();
			}
			found_elm = root->GetElmById(form_id);
		}

		if (found_elm)
		{
			return found_elm;
		}
	}
	return NULL;
}

static HTML_Element* FindFormElementFromNumber(FramesDocument *frames_doc, HTML_Element *form_control, int form_nr, BOOL forward)
{
	HTML_Element *form_iter = forward ? form_control->Next() : form_control;
	while (form_iter)
	{
		if (form_iter->GetNsType() == NS_HTML &&
			(form_iter->Type() == HE_FORM ||
			form_iter->Type() == HE_ISINDEX))
		{
			// -1 == -1 means that a dom inserted form element
			// is matched to a dom inserted form
			if (form_iter->GetFormNr(frames_doc) == form_nr)
			{
				OP_ASSERT(!frames_doc || FormManager::BelongsToForm(frames_doc, form_iter, form_control));
				return form_iter;
			}
			else
			{
				OP_ASSERT(!frames_doc || !FormManager::BelongsToForm(frames_doc, form_iter, form_control));
			}
		}

		form_iter = forward ? form_iter->Next() : form_iter->Prev();
	}

	return NULL;
}

HTML_Element* FormManager::FindFormElm(FramesDocument* frames_doc,
									   HTML_Element* form_control)
{
	if (form_control->GetNsType() != NS_HTML)
	{
		return NULL;
	}

	// All types with a form property in DOM2HTML
	HTML_ElementType type = form_control->Type();
	BOOL is_form_control_type = type == HE_INPUT ||
		type == HE_BUTTON || type == HE_TEXTAREA ||
		type == HE_OPTGROUP || type == HE_OPTION ||
		type == HE_SELECT || type == HE_OUTPUT ||
		type == HE_FIELDSET || type == HE_LEGEND ||
		type == HE_LABEL ||
		type == HE_PROGRESS || type == HE_METER;
#ifdef FORMS_KEYGEN_SUPPORT
	is_form_control_type = is_form_control_type || type == HE_KEYGEN;
#endif // FORMS_KEYGEN_SUPPORT
#ifdef _PLUGIN_SUPPORT_
	is_form_control_type = is_form_control_type || type == HE_OBJECT;
#endif // _PLUGIN_SUPPORT
	if (!is_form_control_type)
	{
		return NULL;
	}

	const uni_char* form_ids = FormManager::GetFormIdString(form_control);
	if (form_ids)
	{
		HTML_Element* found_elm;
		found_elm = FormManager::FindElementFromIdString(frames_doc,
															  form_control,
															  form_ids);
		if (found_elm)
		{
			if (!found_elm->IsMatchingType(HE_FORM, NS_HTML))
			{
				// If the user has pointed to an illegal element, then
				// the form element isn't associated with any form.
				return NULL;
			}
			return found_elm;
		}

		// No matching id -> the form
		// element isn't associated with any form.
		return NULL;
	}

	int form_nr = form_control->GetFormNr(frames_doc);
	// form_nr == -1 means that the form isn't connected to a form
	// that existed when the document was initially parsed. It might
	// be an orphan or belong to an unnumbered <form> (inserted by
	// DOM). We therefore search for a form with form_nr == -1

	// For XHTML the document structure is more "correct" than the
	// form number.

	// In both cases we therefore look at the parents first of all
	BOOL is_xhtml = FALSE;
	if (frames_doc)
	{
		HLDocProfile *hld_profile = frames_doc->GetHLDocProfile();
		is_xhtml = hld_profile && hld_profile->IsXml();
	}

	if (form_nr == -1 && form_control->GetInserted() == HE_INSERTED_BY_DOM || is_xhtml)
	{
		HTML_Element* parent = form_control;
		do
		{
			if (parent->IsMatchingType(HE_FORM, NS_HTML))
			{
				OP_ASSERT(!frames_doc || BelongsToForm(frames_doc, parent, form_control));
				return parent;
			}
			parent = parent->Parent();
		}
		while (parent);
	}

	if (form_nr != -1)
	{
		// use the form number
		HTML_Element* form_element = FindFormElementFromNumber(frames_doc, form_control, form_nr, FALSE);
		if (!form_element)
			form_element = FindFormElementFromNumber(frames_doc, form_control, form_nr, TRUE);

		return form_element;
	}

	return NULL;
}

BOOL FormManager::IsSelectedRadioInGroup(HTML_Document *html_doc, HTML_Element* radio_button)
{
	OP_ASSERT(radio_button->GetInputType() == INPUT_RADIO);
	OP_ASSERT(html_doc);
	FormValue* form_value = radio_button->GetFormValue();
	OP_ASSERT(form_value);

	FormValueRadioCheck* value_radio = FormValueRadioCheck::GetAs(form_value);
	if (value_radio->IsChecked(radio_button))
		return TRUE;

	// If there are no other checked radio button in this group, we
	// assume that this is checked. That is needed because we only
	// navigate to checked radio buttons and we need to be able to
	// navigate to radio groups where nothing is checked (yet)
	const uni_char* this_name = radio_button->GetName();

	const uni_char* EmptyString = UNI_L("");
	if (!this_name)
		this_name = EmptyString; // Empty name -> no name. This isn't consistent with other places that groups radio buttons

	FramesDocument* frames_doc = html_doc->GetFramesDocument();
	HTML_Element *form_elm = FormManager::FindFormElm(frames_doc, radio_button);
	if (form_elm)
	{
		HTML_Element* he = form_elm;
		while ((he = FormManager::NextFormElm(frames_doc, form_elm, he)) != NULL)
		{
			if (he != radio_button && he->Type() == HE_INPUT && he->GetInputType() == INPUT_RADIO)
			{
				const uni_char* other_name = he->GetName();
				if (!other_name)
					other_name = EmptyString; // Empty name -> no name. This isn't consistent with other places that groups radio buttons
				if (uni_str_eq(this_name, other_name))
				{
					FormValueRadioCheck* other_value_radio;
					other_value_radio = FormValueRadioCheck::GetAs(he->GetFormValue());
					// Old code checks also Disabled() and !Disabled(), but the only caller
					// (HTML_Document::NextTabElm) checks that anyway
					if (other_value_radio->IsChecked(he))
						return FALSE;
				}
			}
		}
	}

	// Found no other checked radio button in the same group. Assume
	// that this is checked.
	return TRUE;
}

BOOL FormManager::IsFormElement(HTML_Element* elm)
{
	if (elm->GetNsType() == NS_HTML)
	{
		// This must only return things that have a FormValue since it
		// is used to check if the FormValue exist
		switch(elm->Type())
		{
		case HE_INPUT:
		case HE_SELECT:
		case HE_BUTTON:
		case HE_TEXTAREA:
#ifdef FORMS_KEYGEN_SUPPORT
		case HE_KEYGEN:
#endif // FORMS_KEYGEN_SUPPORT
		case HE_OUTPUT:
			return TRUE;
		}
	}
	return FALSE;
}

/* static */
OP_STATUS FormManager::ConfigureForFileSplit(UniParameterList& file_list,
											 const uni_char* file_value_str)
{
	return file_list.SetValue(file_value_str,
								PARAM_ONLY_SEP | PARAM_SEP_COMMA |
								PARAM_SEP_SEMICOLON | PARAM_NO_ASSIGN |
								PARAM_STRIP_ARG_QUOTES);
}

/* static */
OP_STATUS FormManager::HandleSubmitButtonClick(FramesDocument* frames_doc,
											   HTML_Element* form_element,
											   HTML_Element* submit_element,
											   int offset_x, long offset_y,
											   int document_x, long document_y,
											   int button_or_key_or_delta,
											   ShiftKeyState modifiers,
											   ES_Thread *origining_thread)
{
	if (!(form_element->GetBoolAttr(ATTR_NOVALIDATE)
		  || submit_element && submit_element->GetBoolAttr(ATTR_FORMNOVALIDATE)))
	{
		FormValidator validator(frames_doc);
		ValidationResult result = validator.ValidateForm(form_element, TRUE, origining_thread);
		if (!result.IsOk()
#ifdef ALLOW_TOO_LONG_FORM_VALUES
			// Compatibility fix. See bug 174846
			&& !FormSubmitter::HandleTooLongValue(frames_doc, result)
#endif // ALLOW_TOO_LONG_FORM_VALUES
			)
		{
			/* Validation failed for non-submit() use; "oninvalid" event fires, but
			   nothing to wait on as the submit has been aborted. */
			return OpStatus::OK;
		}
	}

	OP_STATUS status = OpStatus::OK;
#ifdef WAND_SUPPORT
	const BOOL submit_through_wand = !frames_doc->GetWindow()->GetPrivacyMode();

	if (submit_through_wand)
	{
		if (ES_ThreadScheduler *scheduler = frames_doc->GetESScheduler())
		{
			/* Pretty ugly hack to not submit a form if the current document
			has been replaced using document.write. (jl@opera.com) */
			if (!scheduler->TestTerminatingAction(ES_OpenURLAction::FINAL, ES_OpenURLAction::CONDITIONAL))
			{
				return OpStatus::OK;
			}
		}

		status = g_wand_manager->SubmitPage(frames_doc, submit_element, form_element,
											offset_x, offset_y,
											document_x, document_y,
											modifiers, FALSE, origining_thread,
											FALSE); /* not synthetic */
	}
	else
#endif //WAND_SUPPORT
	{
		// If we have no wand enabled, it's just converted to a submit event.
		status = frames_doc->HandleMouseEvent(ONSUBMIT, submit_element, form_element,
											  NULL,
											  offset_x, offset_y,
											  document_x, document_y,
											  modifiers,
											  button_or_key_or_delta,
											  origining_thread);
	}
	return status;
}

/* static */
OP_STATUS FormManager::DOMSubmit(FramesDocument* frames_doc, HTML_Element* form,
								 ES_Thread* origining_thread
#ifdef WAND_SUPPORT
								 , BOOL is_from_submit_event
#endif// WAND_SUPPORT
								 )
{
#if 0 // We have disabled the validation as part of bug 306647: throwing submit() breaks sites
	FormValidator validator(frames_doc);
	ValidationResult val_res = validator.ValidateFormWithoutEvent(form);
	BOOL validates = val_res.IsOk();

	if (!validates
#ifdef ALLOW_TOO_LONG_FORM_VALUES
		&& 	// Compatibility fix. See bug 174846
		!FormSubmitter::HandleTooLongValue(frames_doc, val_res)
#endif // ALLOW_TOO_LONG_FORM_VALUES
		)
	{
		// WF2 spec:
		// Otherwise, if the form submission was initiated via the submit()
		// method, then instead of firing invalid events, a SYNTAX_ERR
		// exception shall be raised (and submission is aborted) if any
		// of the controls are invalid. [DOM3CORE]
		return OpStatus::ERR_OUT_OF_RANGE;
	}
#endif // 0 // We have disabled the validation as part of bug 306647: throwing submit() breaks sites

	int offset_x_signal = -1; // This is used in HTML_Element::HandleEvent to be able to see the difference between a call to submit() and a submit generated by pressing ENTER in a field in a form without a submit button

#ifdef WAND_SUPPORT
		const BOOL submit_through_wand = !is_from_submit_event && !frames_doc->GetWindow()->GetPrivacyMode();

	if (submit_through_wand)
	{
		/* If we're processing a submit event, there's a good chance
			the wand has already looked at it, so we don't want to go
			through the wand manager again. Otherwise we first go to
			wand and then let it do the actual submit. */
		g_wand_manager->SubmitPage(frames_doc, NULL, form, offset_x_signal, 0, 0, 0,
		                           SHIFTKEY_NONE, TRUE, origining_thread, TRUE);
	}
	else
#endif //WAND_SUPPORT
	{
		HTML_Element::HandleEventOptions opts;
		opts.offset_x = offset_x_signal;
		opts.sequence_count_and_button_or_key_or_delta = MOUSE_BUTTON_1;
		opts.synthetic = TRUE;
		form->HandleEvent(ONSUBMIT, frames_doc, form, opts);
	}

	return OpStatus::OK;
}

/* static */
BOOL FormManager::IsButton(HTML_Element* elm)
{
	OP_ASSERT(elm);
	if (elm->GetNsType() != NS_HTML)
		return FALSE;
	if (elm->Type() == HE_BUTTON)
		return TRUE;
	if (elm->Type() == HE_INPUT)
		return IsButton(elm->GetInputType());
	return FALSE;
}

/* static */
BOOL FormManager::IsButton(InputType input_type)
{
	switch (input_type)
	{
		// INPUT_IMAGE is sometimes treated as
		// a button, but not here.
	case INPUT_BUTTON:
	case INPUT_SUBMIT:
	case INPUT_RESET:
		return TRUE;
	}
	return FALSE;
}

/* static */
BOOL FormManager::IsIncludedInSubmit(FramesDocument* frames_doc,
									 HTML_Element* form_control_element,
									 HTML_Element* submit_element,
									 BOOL form_element_check)
{
	// The willValidate attribute of a form control element must return true if all of the following conditions are met:
	// The control is associated with a form (or several forms).
	// The control does not have a repetition template as an ancestor.
	// The control does not have a datalist element as an ancestor.
	// The control has a name.
	// The control is not disabled.
	// The control is not a button of type button, reset, add, remove, move-up, or move-down.
	// The control is not an output element.
	//
	// It must return false if any of these conditions are not met.

#ifdef _WML_SUPPORT_
	if (HLDocProfile *hld_profile = frames_doc->GetHLDocProfile())
	{
		if (hld_profile->IsWml()
			|| (hld_profile->HasWmlContent()
				&& form_control_element->GetNsType() == NS_WML))
			return TRUE;
	}
#endif // _WML_SUPPORT_

	// Allow the caller to optimize by skipping the following check.
	if (form_element_check && !FormManager::FindFormElm(frames_doc, form_control_element))
	{
		return FALSE;
	}

	BOOL correct_type = (form_control_element->Type() == HE_INPUT) ||
		(form_control_element->Type() == HE_SELECT) ||
		(form_control_element->Type() == HE_TEXTAREA) ||
		(form_control_element->Type() == HE_BUTTON);

	// For SSL we also support the keygen tag
#ifdef FORMS_KEYGEN_SUPPORT
	correct_type = correct_type || form_control_element->Type() == HE_KEYGEN;
#endif // FORMS_KEYGEN_SUPPORT

#ifdef _PLUGIN_SUPPORT_
	if (form_control_element->Type() == HE_OBJECT) // HE_EMBED/HE_APPLET?
	{
		OpString dummy;
		if (OpStatus::IsSuccess(FormManager::GetPluginFormValue(form_control_element, dummy)))
		{
			correct_type = TRUE;
		}
	}
#endif // _PLUGIN_SUPPORT

	if (!correct_type)
	{
		return FALSE;
	}

	const BOOL is_button = FormManager::IsButton(form_control_element);
	BOOL is_unchecked_radiocheck = FALSE;
	BOOL is_image_submit = FALSE;
	if (form_control_element->Type() == HE_INPUT)
	{
		switch (form_control_element->GetInputType())
		{
		case INPUT_IMAGE:
			is_image_submit = (form_control_element == submit_element);
			break;
		case INPUT_CHECKBOX:
		case INPUT_RADIO:
			is_unchecked_radiocheck = !FormValueRadioCheck::GetAs(form_control_element->GetFormValue())->IsChecked(form_control_element);
			break;
		}
	}

	const uni_char* name = form_control_element->GetName();
	if (!(name && *name) && !is_image_submit)
	{
		// The only thing sent despite missing name are
		// images which have the click coordinates sent as x and y
		return FALSE;
	}

	if (is_button && form_control_element != submit_element ||
		is_unchecked_radiocheck)
	{
		return FALSE; // Not submitted button or unchecked thing
	}

	if (form_control_element->IsDisabled(frames_doc))
	{
		return FALSE;
	}

	HTML_Element* parent = form_control_element->ParentActual();
	while (parent)
	{
		if (parent->IsMatchingType(HE_DATALIST, NS_HTML))
		{
			// Nothing in datalist is submitted
			return FALSE;
		}

#ifdef _PLUGIN_SUPPORT_
		// Don't submit alternate content under an HE_OBJECT/Plugin that was included
		if (parent->IsMatchingType(HE_OBJECT, NS_HTML))
		{
			Box* box = parent->GetLayoutBox();
			if (box && box->IsContentReplaced())
			{
				// This was just alternative content for a plugin
				return FALSE;
			}
		}
#endif // _PLUGIN_SUPPORT_

		if (parent->IsMatchingType(HE_NOSCRIPT, NS_HTML))
		{
			HLDocProfile* hld_profile = frames_doc->GetHLDocProfile();
			if (hld_profile && hld_profile->GetESEnabled())
			{
				// This was in a noscript tag and since we had script activated,
				// this content should be ignored. Normally (in HTML) there are no
				// form controls under a NOSCRIPT, but in XHTML there is, and
				// scripts can probably insert them too.
				return FALSE;
			}
		}
		parent = parent->ParentActual();
	}

	return TRUE; // Passed all checks
}

#ifdef FORMS_VALIDATE_EMAIL // exposed API, see module.export

/* static */
BOOL FormManager::IsValidEmailAddress(const uni_char* string)
{
	OP_ASSERT(string);

	OpString idna_encoded_string;
	OpString modifiable_string;

	int string_len = uni_strlen(string);
	// The expansion in unicode -> idna is unknown, but assume it's huge to not hit the limit for anything realistic.
	int storage_len = string_len * 15 + 1;
	if (idna_encoded_string.Reserve(storage_len) &&
	    OpStatus::IsSuccess(modifiable_string.Set(string)) &&
	    OpStatus::IsSuccess(FormValueEmail::ConvertToIDNA(modifiable_string.CStr(), idna_encoded_string.CStr(), storage_len, TRUE)))
	{
		string = idna_encoded_string.CStr();
	}

	return FormValidator::IsValidEmailAddress(string);
}
#endif // FORMS_VALIDATE_EMAIL

/**
 * We must take fieldsets into considerations
 */
/* static */
BOOL FormManager::IsInheritedDisabled(const HTML_Element* form_control_element)
{
	OP_ASSERT(form_control_element);
	HTML_ElementType type = form_control_element->Type();
	BOOL is_form_type = type == HE_INPUT || type == HE_FIELDSET ||
		type == HE_SELECT || type == HE_TEXTAREA || type == HE_BUTTON;
#ifdef FORMS_KEYGEN_SUPPORT
	is_form_type = is_form_type || form_control_element->Type() == HE_KEYGEN;
#endif // FORMS_KEYGEN_SUPPORT

	if (is_form_type)
	{
		if (form_control_element->GetDisabled())
		{
			return TRUE;
		}

		const HTML_Element* parent = form_control_element;
		while ((parent = parent->Parent()) != NULL)
		{
			if (parent->Type() == HE_FIELDSET)
			{
				if (parent->GetDisabled())
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/* static */
OP_STATUS FormManager::HandleChangedRadioGroups(FramesDocument* doc,
												const OpVector<HTML_Element>& radio_buttons,
												int added_removed_or_name_change)
{
	if (added_removed_or_name_change == 1)
	{
		for (unsigned int i = 0; i < radio_buttons.GetCount(); i++)
		{
			HTML_Element* radio = radio_buttons.Get(i);
			OP_ASSERT(radio->IsMatchingType(HE_INPUT, NS_HTML));
			OP_ASSERT(radio->GetInputType() == INPUT_RADIO);
			HTML_Element* form_elm = FormManager::FindFormElm(doc, radio);
			RETURN_IF_ERROR(doc->GetLogicalDocument()->GetRadioGroups().RegisterRadioAndUncheckOthers(doc, form_elm, radio));
		}
	}

	return OpStatus::OK;
}

/* static*/
void FormManager::AbortSubmitsForDocument(FramesDocument* doc)
{
#ifdef FORMS_KEYGEN_SUPPORT
	FormSubmitter* submit = g_opera->forms_module.m_submits_in_progress;
	while (submit)
	{
		if (submit->BelongsToDocument(doc))
		{
			submit->Abort();
		}
		submit = submit->GetNextInList();
	}
#endif // FORMS_KEYGEN_SUPPORT
}

/* static */
HTML_Element* FormManager::FindDefaultSubmitButton(FramesDocument* doc, HTML_Element* form)
{
	/* We could import the body of FindDefButtonInternal() here, but
	   that function needs to stay either way, for now, because
	   against all logic, it's called from adjunct/quick... */
	extern HTML_Element* FindDefButtonInternal(FramesDocument* frames_doc, HTML_Element* helm, HTML_Element* form_elm);
	return FindDefButtonInternal(doc, form, form);
}

/**
 * Stores the information needed for a submit.
 */
struct FormSubmitInfo
{
	FormSubmitInfo(FramesDocument* doc, HTML_Element* he_submit, HTML_Element* he_form,
	         int offset_x, long offset_y,
	         int document_x, long document_y,
	         ShiftKeyState modifiers,
	         ES_Thread *interrupt_thread)
			 :
	m_doc(doc),
		m_submit_elm(he_submit),
		m_form_elm(he_form),
		m_offset_x(offset_x),
		m_offset_y(offset_y),
		m_document_x(document_x),
		m_document_y(document_y),
		m_key_modifiers(modifiers),
		m_interrupt_thread(interrupt_thread)
	{}

	FramesDocument* m_doc;
	HTML_Element* m_submit_elm;
	HTML_Element* m_form_elm;
	int m_offset_x;
	int m_offset_y;
	int m_document_x;
	int m_document_y;
	ShiftKeyState m_key_modifiers;
	ES_Thread *m_interrupt_thread;
};
