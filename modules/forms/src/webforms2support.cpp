/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/**
 * @author Daniel Bratell
 */
#include "core/pch.h"

#include "modules/forms/webforms2support.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/webforms2number.h"
#include "modules/forms/src/formiterator.h"
#include "modules/forms/datetime.h"
#include "modules/forms/piforms.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/formsenum.h"
#include "modules/forms/src/validationerrorwindow.h"

#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"

#include "modules/formats/argsplit.h"

#include "modules/util/opfile/opfile.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/ecmascript_utils/essched.h"
#include "modules/dom/domenvironment.h"

#include "modules/util/tempbuf.h"
# include "modules/dom/domutils.h"

// #define EXTREME_REGEXP_TESTING


#include "modules/forms/formvaluelist.h"
#include "modules/regexp/include/regexp_api.h"

/* static */BOOL FormValidator::MatchesRegExp(const uni_char* reg_exp_pattern, const uni_char* a_string)
{
	OP_ASSERT(reg_exp_pattern);
	OP_ASSERT(a_string);

	TempBuffer modified_pattern;
	OpRegExp* reg_exp;
	OpREFlags flags;
	flags.multi_line = FALSE;
	flags.case_insensitive = FALSE;
	flags.ignore_whitespace = FALSE;
	if (OpStatus::IsSuccess(modified_pattern.AppendFormat(UNI_L("^(?:%s)$"), reg_exp_pattern)) &&
	    OpStatus::IsSuccess(OpRegExp::CreateRegExp(&reg_exp, modified_pattern.GetStorage(), &flags)))
	{
		OpREMatchLoc match_pos;
		OP_STATUS status = reg_exp->Match(a_string, &match_pos);
		OP_DELETE(reg_exp);
		if (OpStatus::IsSuccess(status) && match_pos.matchloc == REGEXP_NO_MATCH)
		{
			// Didn't match or didn't match the full string
			return FALSE;
		}
	}

	// If anything goes wrong we assume it matches to avoid bad validation errors.
	return TRUE;
}

/* static */
BOOL FormValidator::IsValidEmailAddress(const uni_char* string)
{
	OP_ASSERT(string);

	// A valid e-mail address is a string that matches the ABNF production
	// 1*( atext / "." ) "@" ldh-str 1*( "." ldh-str ) where atext is defined
	// in RFC 5322 section 3.2.3, and ldh-str is defined in RFC 1034 section 3.5.
	//
	// RFC 5322                Internet Message Format             October 2008
	//
	//
	//    atext           =   ALPHA / DIGIT /    ; Printable US-ASCII
	//                        "!" / "#" /        ;  characters not including
	//                        "$" / "%" /        ;  specials.  Used for atoms.
	//                        "&" / "'" /
	//                        "*" / "+" /
	//                        "-" / "/" /
	//                        "=" / "?" /
	//                        "^" / "_" /
	//                        "`" / "{" /
	//                        "|" / "}" /
	//                        "~"
	//
	//    atom            =   [CFWS] 1*atext [CFWS]
	//
	//    dot-atom-text   =   1*atext *("." 1*atext)
	//
	//    dot-atom        =   [CFWS] dot-atom-text [CFWS]
	//
	//    specials        =   "(" / ")" /        ; Special characters that do
	//                        "<" / ">" /        ;  not appear in atext
	//                        "[" / "]" /
	//                        ":" / ";" /
	//                        "@" / "\" /
	//                        "," / "." /
	//                        DQUOTE
	//
	// RFC 1034             Domain Concepts and Facilities        November 1987
	//
	// The DNS specifications attempt to be as general as possible in the rules
	// for constructing domain names.  The idea is that the name of any
	// existing object can be expressed as a domain name with minimal changes.
	// However, when assigning a domain name for an object, the prudent user
	// will select a name which satisfies both the rules of the domain system
	// and any existing rules for the object, whether these rules are published
	// or implied by existing programs.
	//
	// For example, when naming a mail domain, the user should satisfy both the
	// rules of this memo and those in RFC-822.  When creating a new host name,
	// the old rules for HOSTS.TXT should be followed.  This avoids problems
	// when old software is converted to use domain names.
	//
	// The following syntax will result in fewer problems with many
	// applications that use domain names (e.g., mail, TELNET).
	//
	// <domain> ::= <subdomain> | " "
	//
	// <subdomain> ::= <label> | <subdomain> "." <label>
	//
	// <label> ::= <letter> [ [ <ldh-str> ] <let-dig> ]
	//
	// <ldh-str> ::= <let-dig-hyp> | <let-dig-hyp> <ldh-str>
	//
	// <let-dig-hyp> ::= <let-dig> | "-"
	//
	// <let-dig> ::= <letter> | <digit>
	//
	// <letter> ::= any one of the 52 alphabetic characters A through Z in
	// upper case and a through z in lower case
	//
	// <digit> ::= any one of the ten digits 0 through 9


	// A custom parser would be much quicker, but this was quick to write.
	// XXX Read standard and implement it fully

	// Store in a char built by defines to make this code readable and
	// maintainable.

	// Right now we don't support quoted emails and obsolete mail addresses?
//#define EMAIL_ATEXT "[a-zA-Z0-9_]"
#define EMAIL_ATEXT "[a-zA-Z0-9_!#$%&'*+\\-/=?\\^`{|}~]"
#define EMAIL_ADOT EMAIL_ATEXT "+(?:\\." EMAIL_ATEXT "+)*"

// The user-part accept "." anywhere.
#define EMAIL_ADOT_USER "[a-zA-Z0-9_!#$%&'*+\\-/=?\\^`{|}~\\.]+"

	const char char_pattern[] = "^" EMAIL_ADOT_USER "\\@" EMAIL_ADOT "\\.?$";

#undef EMAIL_ATEXT
#undef EMAIL_ADOT

	const uni_char* pattern = make_doublebyte_in_tempbuffer(char_pattern, sizeof(char_pattern)-1);

	return MatchesRegExp(pattern, string);
}


/* static */
BOOL FormValidator::IsInDataList(HTML_Element* form_control_element)
{
	OP_ASSERT(form_control_element);
	do
	{
		if (form_control_element->IsMatchingType(HE_DATALIST, NS_HTML))
		{
			return TRUE;
		}
		form_control_element = form_control_element->Parent();
	}
	while (form_control_element);

	return FALSE;
}

BOOL FormValidator::WillValidate(HTML_Element* form_control_element) const
{
	// Spec:
	// The willValidate attribute must return true if an element is a candidate for constraint
	// validation, and false otherwise (i.e. false if any conditions are barring it from
	// constraint validation).

	OP_ASSERT(form_control_element->GetNsType() == NS_HTML || !"Only HTML has form elements");

	BOOL correct_type = (form_control_element->Type() == HE_INPUT &&
						form_control_element->GetInputType() != INPUT_HIDDEN) ||
		(form_control_element->Type() == HE_SELECT) ||
		(form_control_element->Type() == HE_TEXTAREA) ||
		(form_control_element->Type() == HE_BUTTON);

	if (!correct_type)
	{
		return FALSE;
	}

	if (form_control_element->Type() == HE_INPUT || form_control_element->Type() == HE_BUTTON)
	{
		switch (form_control_element->GetInputType() )
		{
		case INPUT_RESET:
		case INPUT_BUTTON:
			return FALSE;
		}
	}

	if (IsInheritedDisabled(form_control_element) ||
		IsInDataList(form_control_element) ||
		form_control_element->GetReadOnly())
	{
		return FALSE;
	}

	if (form_control_element->IsMatchingType(HE_OUTPUT, NS_HTML))
	{
		return FALSE;
	}

	return TRUE; // Passed all checks
}

BOOL FormValidator::CanValidate(HTML_Element* form_control_element) const
{
	OP_ASSERT(form_control_element->GetNsType() == NS_HTML);
	BOOL correct_type = (form_control_element->Type() == HE_INPUT) ||
		(form_control_element->Type() == HE_SELECT) ||
		(form_control_element->Type() == HE_TEXTAREA) ||
		(form_control_element->Type() == HE_BUTTON);

	if (!correct_type)
	{
		return FALSE;
	}

	if (form_control_element->Type() == HE_INPUT || form_control_element->Type() == HE_BUTTON)
	{
		InputType input_type = form_control_element->GetInputType();
		if (input_type == INPUT_HIDDEN || input_type == INPUT_IMAGE ||
			input_type == INPUT_RESET || input_type == INPUT_BUTTON)
			return FALSE;
	}

	if (IsInheritedDisabled(form_control_element) ||
		IsInDataList(form_control_element))
	{
		return FALSE;
	}

	return TRUE; // Passed all checks
}


/* private */
ValidationResult FormValidator::ValidateFullFormInternal(const HTML_Element* form_element,
														 BOOL send_invalid_event,
														 BOOL alert_as_default_action,
														 ES_Thread* thread)
{
	OP_ASSERT(m_frames_doc);
	OP_ASSERT(form_element->IsMatchingType(HE_FORM, NS_HTML));

	if (m_frames_doc == NULL)
	{
		// Possible?
		return VALIDATE_OK;
	}

	ValidationResult form_valid_res(VALIDATE_OK);

	if (alert_as_default_action)
	{
		m_frames_doc->SetNextOnInvalidCauseAlert(TRUE); // Should store this in the invalid event
	}

	// If we for every radio button check the full group if there is a
	// checked value, then we'll get an ugly O(n^2) algorithm, so
	// instead we make a pass over the form in advance and make sure
	// the IsInCheckedGroup flags are correct.
	if (OpStatus::IsError(FormValueRadioCheck::UpdateCheckedInGroupFlags(m_frames_doc, const_cast<HTML_Element*>(form_element))))
	{
		// OOM - give it up
		return VALIDATE_OK;
	}

	FormIterator form_iterator(m_frames_doc, form_element);
	while (HTML_Element* fhe = form_iterator.GetNext())
	{
		if (WillValidate(fhe))
		{
			// Disabled controls are not validated so we only check
			// controls that are not disabled. For the others we
			// always return VALIDATE_OK as our callers expect us to do.
			BOOL should_validate = CanValidate(fhe);
			if (fhe->Type() == HE_INPUT)
			{
				InputType inp_type = fhe->GetInputType();
				if (inp_type == INPUT_RADIO || inp_type == INPUT_CHECKBOX)
				{
					should_validate = TRUE; // We must always check the required flag on these
				}
			}

			if (should_validate)
			{
				// This will return VALIDATE_OK if the element isn't included in the submit
				ValidationResult valid_res = ValidateSingleControlInternal(fhe, FALSE, NULL);

				if (!valid_res.IsOk())
				{
					form_valid_res.SetErrorsFrom(valid_res);
					if (send_invalid_event)
					{
						OpStatus::Ignore(SendOnInvalidEvent(fhe, thread)); // We can live with a missing oninvalid event
					}
				}
			}
		}
	}

	return form_valid_res;

}

OP_STATUS FormValidator::SendOnInvalidEvent(HTML_Element* form_control_element, ES_Thread* thread)
{
	// This event has to be blocked by a special thread so that we know when
	// the event has finished. But we only want to do that if there is going
	// to be a event thread since it creates lots of extra complexity in the
	// submission process.

	BOOL need_to_send_to_dom = FALSE;
	if (DOM_Environment *environment = m_frames_doc->GetDOMEnvironment())
	{
		need_to_send_to_dom = environment->IsEnabled();
	}
	else
	{
		HTML_Element *currentTarget = form_control_element;
		while (currentTarget)
		{
			if (DOM_Utils::GetEventTargetElement(currentTarget)->HasEventHandlerAttribute(m_frames_doc, ONINVALID))
			{
				need_to_send_to_dom = TRUE;
				break;
			}

			currentTarget = DOM_Utils::GetEventPathParent(currentTarget, form_control_element);
		}
	}

	if (need_to_send_to_dom)
	{
		// Create a dom environment so that we have a scheduler to feed with our thread.
		RETURN_IF_ERROR(m_frames_doc->ConstructDOMEnvironment());
		if (!m_invalid_event_blocker)
		{
			ES_ThreadScheduler* scheduler = m_frames_doc->GetESScheduler();
			if (scheduler)
			{
				m_invalid_event_blocker = OP_NEW(ES_EmptyThread, ());

				OP_BOOLEAN result;
				if (!m_invalid_event_blocker ||
					OpStatus::IsMemoryError(result = scheduler->AddRunnable(m_invalid_event_blocker, thread)))
				{
					return OpStatus::ERR_NO_MEMORY;
				}

				if (result != OpBoolean::IS_TRUE)
				{
					m_invalid_event_blocker = NULL;
				}
			}
		}

		DOM_Environment::EventData data;
		data.type = ONINVALID;
		data.target = form_control_element;

		OP_BOOLEAN was_handled = m_frames_doc->GetDOMEnvironment()->HandleEvent(data, m_invalid_event_blocker);
		if (was_handled == OpBoolean::IS_TRUE)
		{
			m_invalid_events_sent_to_dom = TRUE;
			return OpStatus::OK;
		}
	}

	HTML_Element::HandleEventOptions opts;
	opts.thread = m_invalid_event_blocker;
	opts.synthetic = m_invalid_event_blocker != NULL;
	form_control_element->HandleEvent(ONINVALID, m_frames_doc, form_control_element, opts);
	return OpStatus::OK;
}

void FormValidator::SendOnFormInputEvent(HTML_Element* form_control_element, ES_Thread* thread) const
{
	// We can live with a missing event
	OpStatus::Ignore(m_frames_doc->HandleEvent(ONFORMINPUT, NULL, form_control_element, 0, 0, thread));
}

void FormValidator::SendOnFormChangeEvent(HTML_Element* form_control_element, ES_Thread* thread) const
{
	// We can live with a missing event
	OpStatus::Ignore(m_frames_doc->HandleEvent(ONFORMCHANGE, NULL, form_control_element, 0, 0, thread));
}

/* static */
ValidationResult FormValidator::CheckPattern(HTML_Element* form_control_element,
											 const uni_char* value)
{
	OP_ASSERT(value);
	const uni_char* pattern = form_control_element->GetStringAttr(ATTR_PATTERN);
	if (pattern)
	{
		if (!MatchesRegExp(pattern, value))
		{
			return VALIDATE_ERROR_PATTERN_MISMATCH;
		}
	}
	return VALIDATE_OK;
}

/* static */
ValidationResult FormValidator::CheckTextMaxLength(HTML_Element* form_control_element,
											 const uni_char* value1)
{
	ValidationResult result = VALIDATE_OK;
	int max_length = form_control_element->GetNumAttr(ATTR_MAXLENGTH);
	if (max_length > 0)
	{
		int length = uni_strlen(value1); // Do we already have the length somewhere?
		if (length > max_length)
		{
			result = VALIDATE_ERROR_TOO_LONG;
		}
	}

	return result;
}

ValidationResult FormValidator::ValidateSingleControl(HTML_Element* form_control_element) const
{
	// Disabled controls are not validated so we only check
	// controls that are not disabled. For the others we
	// always return VALIDATE_OK as our callers expect us to do.
	BOOL should_validate = CanValidate(form_control_element);

	if (!should_validate)
	{
		return VALIDATE_OK; // Next control
	}

	return ValidateSingleControlInternal(form_control_element, FALSE, NULL);
}

ValidationResult FormValidator::ValidateSingleControlAgainstValue(HTML_Element* form_control_element,
																  const uni_char* value) const
{
	// Disabled controls are not validated so we only check
	// controls that are not disabled. For the others we
	// always return VALIDATE_OK as our callers expect us to do.
	BOOL should_validate = CanValidate(form_control_element);
	if (!should_validate &&
		form_control_element->Type() == HE_INPUT &&
		(form_control_element->GetInputType() == INPUT_RADIO ||
		 form_control_element->GetInputType() == INPUT_CHECKBOX))
	{
		should_validate = TRUE; // We must check the required flag on these
	}

	if (!should_validate)
	{
		return VALIDATE_OK; // Next control
	}

	return ValidateSingleControlInternal(form_control_element, TRUE, value);
}

ValidationResult FormValidator::ValidateSingleControlInternal(HTML_Element* form_control_element,
															  BOOL use_custom_value, const uni_char* custom_value) const
{
	FormValue* value = form_control_element->GetFormValue();
	OP_ASSERT(value); // Otherwise it wouldn't pass IsIncludedInSubmit

#ifdef _DEBUG
	if (use_custom_value)
	{
		// We don't support this for every type
		OP_ASSERT(value->HasMeaningfulTextRepresentation());
		OP_ASSERT(value->GetType() != FormValue::VALUE_RADIOCHECK);
	}
#endif // _DEBUG

	ValidationResult valid_res(VALIDATE_OK);
	const uni_char* custom_error = form_control_element->GetSpecialStringAttr(FORMS_ATTR_USER_SET_INVALID_FLAG, SpecialNs::NS_FORMS);
	if (!use_custom_value && custom_error && *custom_error)
	{
		// Don't look at custom_error if we have use_custom_value = NULL since then it will be hard to change to another value with autocomplete that filters illegal values through this method.
		valid_res.SetError(VALIDATE_ERROR_CUSTOM);
	}

	BOOL req_attr = form_control_element->GetBoolAttr(ATTR_REQUIRED);

	// First handle all form elements that have no text value
	if (value->GetType() == FormValue::VALUE_RADIOCHECK ||
		!value->HasMeaningfulTextRepresentation())
	{
		if (req_attr && value->GetType() == FormValue::VALUE_RADIOCHECK)
		{
			// Radio and checkbox can not be handled as text so those must be
			// treated aside from the others
			FormValueRadioCheck* radiocheckvalue = FormValueRadioCheck::GetAs(value);
			if (form_control_element->GetInputType() == INPUT_CHECKBOX)
			{
				if (!radiocheckvalue->IsChecked(form_control_element))
				{
					// The only check for checkboxes
					return VALIDATE_ERROR_REQUIRED;
				}
			}
			else
			{
				OP_ASSERT(form_control_element->GetInputType() == INPUT_RADIO);
				// One (and only one) value in the radio group should be checked
#if 0
				// XXX How to check it so that we don't get an O(n^2) algorithm?
				if (!radiocheckvalue->IsGroupChecked(m_frames_doc, form_control_element))
				{
					// The only check for radio buttons
					return VALIDATE_ERROR_REQUIRED;
				}
#else
				// This is based on a cached value so we better make sure it's correct
				if (!radiocheckvalue->IsInCheckedRadioGroup())
				{
					// The only check for radio buttons
					return VALIDATE_ERROR_REQUIRED;
				}
#endif // 0
			}
		}
		else if (req_attr && form_control_element->IsMatchingType(HE_SELECT, NS_HTML))
		{
			// From the spec:
			// If a select element has a required attribute specified, does not have a multiple attribute specified,
			// and has a display size of 1; and if the value of the first option element in the select element's list
			// of options (if any) is the empty string, and that option is not disabled, and, finally, that
			// option element's parent node is the select element (and not an optgroup element), then that option is
			// the select element's placeholder label option.
			//
			// Constraint validation: If the element has its required attribute specified, and either none of the option
			// elements in the select element's list of options have their selectedness set to true, or the only option
			// element in the selected element's list of options with its selectedness set to true is
			// the placeholder label option, then the element is suffering from being missing.

			FormValueList* list_value = FormValueList::GetAs(value);
			unsigned select_index;
			HTML_Element* option_elm;
			BOOL something_selected = FALSE;
			if (OpStatus::IsSuccess(list_value->GetIndexAndElementOfFirstSelected(form_control_element, option_elm, select_index)))
			{
				something_selected = TRUE;
				if (select_index == 0 &&
				    option_elm->ParentActualStyle() == form_control_element &&
				    !form_control_element->GetBoolAttr(ATTR_MULTIPLE) &&
				    form_control_element->GetSize() <= 1)
				{
					TempBuffer buffer;
					const uni_char* opt_val = FormValueList::GetValueOfOption(option_elm, &buffer);
					if (opt_val && !*opt_val)
					{
						// Only the placeholder. Shouldn't be counted.
						something_selected = FALSE;
					}
				}
			}
			if (!something_selected)
			{
				valid_res.SetError(VALIDATE_ERROR_REQUIRED);
			}
		}

		// No text value -> nothing more to validate
		return valid_res;
	}

	InputType input_type = form_control_element->GetInputType();
	if (form_control_element->IsMatchingType(HE_TEXTAREA, NS_HTML))
	{
		// Setup input type so that we can rely solely on that one
		input_type = INPUT_TEXTAREA;
	}

	if (input_type == INPUT_TEXTAREA || input_type == INPUT_TEXT)
	{
		// These are not unlikely to have huge amounts of data
		// in them so avoid calling GetValueAsText if they have
		// no validation anyway.
		if (!req_attr &&
			!form_control_element->HasAttr(ATTR_PATTERN) &&
			form_control_element->GetNumAttr(ATTR_MAXLENGTH) <= 0)
		{
			return valid_res;
		}
	}

	// Only check non empty controls
	OpString value1;
	if (use_custom_value)
	{
		OpStatus::Ignore(value1.Set(custom_value));
	}
	else
	{
		OpStatus::Ignore(value->GetValueAsText(form_control_element, value1));
	}

	// WF2: "Fields with no value selected do not need to
	//       match the format appropriate for their type"
	if (value1.IsEmpty())
	{
		if (req_attr) // Anything that's not the empty string
		{
			valid_res.SetError(VALIDATE_ERROR_REQUIRED);
		}

		// Check against the min attribute
		if (input_type == INPUT_FILE)
		{
			valid_res.SetErrorsFrom(ValidateFile(form_control_element, UNI_L("")));
		}
	}
	else
	{
		// Check against pattern if there is one and check the maxlength.
		switch (input_type)
		{
			case INPUT_EMAIL:
			case INPUT_PASSWORD:
			case INPUT_URI:
			case INPUT_SEARCH:
			case INPUT_TEL:
			case INPUT_TEXT:
			case INPUT_TEXTAREA:
				valid_res.SetErrorsFrom(CheckPattern(form_control_element, value1.CStr()));
				valid_res.SetErrorsFrom(CheckTextMaxLength(form_control_element, value1.CStr()));
		}

		// Do custom validation
		switch (input_type)
		{
			case INPUT_CHECKBOX:
			case INPUT_RADIO:
			case INPUT_SUBMIT:
			case INPUT_RESET:
			case INPUT_HIDDEN:
			case INPUT_IMAGE:
			case INPUT_PASSWORD:
			case INPUT_BUTTON:
			case INPUT_COLOR:
				// No validation
				break;
			case INPUT_SEARCH:
			case INPUT_TEL:
			case INPUT_TEXT:
			case INPUT_TEXTAREA:
				// No validation except pattern and maxlength that are already done
				break;
			case INPUT_FILE:
				valid_res.SetErrorsFrom(ValidateFile(form_control_element, value1.CStr()));
				break;
			case INPUT_URI:
				valid_res.SetErrorsFrom(ValidateUri(form_control_element, value1.CStr()));
				break;
			case INPUT_DATE:
				valid_res.SetErrorsFrom(ValidateDate(form_control_element, value1.CStr()));
				break;
			case INPUT_WEEK:
				valid_res.SetErrorsFrom(ValidateWeek(form_control_element, value1.CStr()));
				break;
			case INPUT_TIME:
				valid_res.SetErrorsFrom(ValidateTime(form_control_element, value1.CStr()));
				break;
			case INPUT_EMAIL:
				valid_res.SetErrorsFrom(ValidateEmail(value1.CStr(), form_control_element->GetBoolAttr(ATTR_MULTIPLE)));
				break;
			case INPUT_NUMBER:
			case INPUT_RANGE:
				valid_res.SetErrorsFrom(ValidateNumber(form_control_element, value1.CStr()));
				break;
			case INPUT_MONTH:
				valid_res.SetErrorsFrom(ValidateMonth(form_control_element, value1.CStr()));
				break;
			case INPUT_DATETIME:
			case INPUT_DATETIME_LOCAL:
				valid_res.SetErrorsFrom(ValidateDateTime(form_control_element, value1.CStr()));
				break;
			default:
				// Possible?
				;
		}
	} // end has value

	return valid_res;
}


/* static */
OP_STATUS FormValidator::GetCustomValidationMessage(HTML_Element* form_control, TempBuffer* result_buffer)
{
	const uni_char* message = form_control->GetSpecialStringAttr(FORMS_ATTR_USER_CUSTOM_VALIDATION_ERROR_STR, SpecialNs::NS_FORMS);
	if (!(message && *message))
	{
		return OpStatus::ERR;
	}

	return result_buffer->Append(message);
}

/* static */
ValidationResult FormValidator::ValidateFile(HTML_Element* fhe, const uni_char* value)
{
	// Maxlength for files isn't checked when max length for the
	// other fields are checked so we have to check it here.
	UniParameterList filelist;
	ValidationResult result = VALIDATE_OK;

	OP_STATUS status = FormManager::ConfigureForFileSplit(filelist, value);
	if (OpStatus::IsSuccess(status))
	{
		unsigned int existing_file_count = 0;
		UniParameters* file = filelist.First();
		while (file)
		{
			const uni_char* file_name = file->Name();
			if(!file_name || !*file_name)
			{
				result.SetError(VALIDATE_ERROR_TYPE_MISMATCH);
			}
			else
			{
				OpFile file_obj;
				status = file_obj.Construct(file_name);
				if (OpStatus::IsError(status))
				{
					// Check file.Error();
					result.SetError(VALIDATE_ERROR_TYPE_MISMATCH);
				}
				else
				{
					// We have to open file the file to get to read the length
					// We also use that to signal a validation error if the file
					// wasn't readable.
					status = file_obj.Open(OPFILE_READ);
					if (OpStatus::IsError(status))
					{
						result.SetError(VALIDATE_ERROR_TYPE_MISMATCH);
					}
					else
					{
						existing_file_count++;
#if 0 // MAXLENGTH disabled because there are sites using the
		// maxlength attribute on input type=file as a way to
		// attempt to control the length of the input field.
		// This will possible be revived as "filelength" or something similar
						OpFileLength file_length;
						file_obj.GetFileLength(file_length);
						file_obj.Close();
						if (fhe->HasAttr(ATTR_MAXLENGTH))
						{
							OpFileLength max_length;

							max_length = fhe->GetNumAttr(ATTR_MAXLENGTH);
							if (file_length > max_length)
							{
								result.SetError(VALIDATE_ERROR_TOO_LONG);
							}
						}
#endif // 0 - disabled MAXLENGTH check
					}
				}
			}
			file = file->Suc();
		}

		BOOL is_multiple = fhe->GetBoolAttr(ATTR_MULTIPLE);

		if (!is_multiple && existing_file_count > 1)
		{
			result.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
		}

	}
	else // Couldn't parse the file list
	{
		result.SetError(VALIDATE_ERROR_TYPE_MISMATCH);
	}

	return result;
}

#if defined _DEBUG && defined EXTREME_REGEXP_TESTING
void TestRegExp(const char* regexp)
{
	OpString pattern_obj;
	if (OpStatus::IsError(pattern_obj.Set(regexp)))
	{
		OP_ASSERT(!"OOM?");
		return;
	}
	const uni_char* reg_exp_pattern = pattern_obj.CStr();
	OP_ASSERT(reg_exp_pattern);

	OpRegExp *reg_exp;
	OP_STATUS status = OpRegExp::CreateRegExp(&reg_exp, reg_exp_pattern,
		FALSE, FALSE); // Not multi line, not case insensitive
	OP_ASSERT(OpStatus::IsSuccess(status));
	if (OpStatus::IsSuccess(status))
	{
		OP_DELETE(reg_exp);
	}
}
#endif // _DEBUG

/* static */
ValidationResult FormValidator::ValidateUri(HTML_Element* fhe, const uni_char* value)
{
	// A URI, as defined by [RFC2396] (the absoluteURI token, defined in
	// RFC2396 section 3). UAs could, for example, offer the user URIs
	// from his bookmarks.

	// URI-reference = [ absoluteURI | relativeURI ] [ "#" fragment ]
	// absoluteURI   = scheme ":" ( hier_part | opaque_part )
	// relativeURI   = ( net_path | abs_path | rel_path ) [ "?" query ]

	// hier_part     = ( net_path | abs_path ) [ "?" query ]
	// opaque_part   = uric_no_slash *uric

	// uric_no_slash = unreserved | escaped | ";" | "?" | ":" | "@" |
	//                 "&" | "=" | "+" | "$" | ","

	// net_path      = "//" authority [ abs_path ]
	// abs_path      = "/"  path_segments
	// rel_path      = rel_segment [ abs_path ]

	// rel_segment   = 1*( unreserved | escaped |
	//                     ";" | "@" | "&" | "=" | "+" | "$" | "," )

	// scheme        = alpha *( alpha | digit | "+" | "-" | "." )

	// authority     = server | reg_name

	// reg_name      = 1*( unreserved | escaped | "$" | "," |
	//                     ";" | ":" | "@" | "&" | "=" | "+" )

	// server        = [ [ userinfo "@" ] hostport ]
	// userinfo      = *( unreserved | escaped |
	//                    ";" | ":" | "&" | "=" | "+" | "$" | "," )

	// hostport      = host [ ":" port ]
	// host          = hostname | IPv4address
	// hostname      = *( domainlabel "." ) toplabel [ "." ]
	// domainlabel   = alphanum | alphanum *( alphanum | "-" ) alphanum
	// toplabel      = alpha | alpha *( alphanum | "-" ) alphanum
	// IPv4address   = 1*digit "." 1*digit "." 1*digit "." 1*digit
	// port          = *digit

	// path          = [ abs_path | opaque_part ]
	// path_segments = segment *( "/" segment )
	// segment       = *pchar *( ";" param )
	// param         = *pchar
	// pchar         = unreserved | escaped |
	//                 ":" | "@" | "&" | "=" | "+" | "$" | ","

	// query         = *uric

	// fragment      = *uric

	// uric          = reserved | unreserved | escaped
	// reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
	//                 "$" | ","
	// unreserved    = alphanum | mark
	// mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" |
	//                 "(" | ")"

	// escaped       = "%" hex hex
	// hex           = digit | "A" | "B" | "C" | "D" | "E" | "F" |
	//                         "a" | "b" | "c" | "d" | "e" | "f"

	// alphanum      = alpha | digit
	// alpha         = lowalpha | upalpha

	// lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" |
	//            "j" | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" |
	//            "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z"
	// upalpha  = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" |
	//            "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" |
	//            "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
	// digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
	//            "8" | "9"


	// ucschar = %xA0-D7FF / %xF900-FDCF / %xFDF0-FFEF
	//                   / %x10000-1FFFD / %x20000-2FFFD / %x30000-3FFFD
	//                  / %x40000-4FFFD / %x50000-5FFFD / %x60000-6FFFD
	//                  / %x70000-7FFFD / %x80000-8FFFD / %x90000-9FFFD
	//                  / %xA0000-AFFFD / %xB0000-BFFFD / %xC0000-CFFFD
	//                  / %xD0000-DFFFD / %xE1000-EFFFD


	//    IPv6address    =                            6( h16 ":" ) ls32
	//                   /                       "::" 5( h16 ":" ) ls32
	//                   / [               h16 ] "::" 4( h16 ":" ) ls32
	//                   / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
	//                   / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
	//                   / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
	//                   / [ *4( h16 ":" ) h16 ] "::"              ls32
	//                   / [ *5( h16 ":" ) h16 ] "::"              h16
	//                   / [ *6( h16 ":" ) h16 ] "::"

#define IRI_DEC_OCTET "(?:\\d|(?:[1-9]\\d)|(?:1\\d\\d)|(?:2[0-4]\\d)|(?:25[0-5]))"
#define IRI_IPV4ADDRESS "(?:" IRI_DEC_OCTET "\\." IRI_DEC_OCTET "\\." IRI_DEC_OCTET "\\." IRI_DEC_OCTET ")"
//#define URI_IPV4ADDRESS "(?:\\d+\\.\\d+\\.\\d+\\.\\d+)"
#define IRI_H16 "(?:" URI_HEX URI_HEX URI_HEX URI_HEX ")"
#define IRI_LS32 "(?:(?:" IRI_H16 ":" IRI_H16 ")|" IRI_IPV4ADDRESS ")"
#define IRI_IPV6_ADDRESS_ALT1 "(?:(?:"IRI_H16 ":){6}" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT2 "(?:::(?:" IRI_H16 ":){5}" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT3 "(?:(?:" IRI_H16 ")?::(?:" IRI_H16 ":){4}" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT4 "(?:(?:" IRI_H16 ":" IRI_H16 ")?::(?:" IRI_H16 ":){3}" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT5 "(?:(?:(?:" IRI_H16 ":){2}" IRI_H16 ")?::(?:" IRI_H16 ":){2}" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT6 "(?:(?:(?:" IRI_H16 ":){3}" IRI_H16 ")?::" IRI_H16 ":" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT7 "(?:(?:(?:" IRI_H16 ":){4}" IRI_H16 ")?::" IRI_LS32 ")"
#define IRI_IPV6_ADDRESS_ALT8 "(?:(?:(?:" IRI_H16 ":){5}" IRI_H16 ")?::" IRI_H16 ")"
#define IRI_IPV6_ADDRESS_ALT9 "(?:(?:(?:" IRI_H16 ":){5}" IRI_H16 ")?::)"
#define IRI_IPV6_ADDRESS "(?:" IRI_IPV6_ADDRESS_ALT1 "|" IRI_IPV6_ADDRESS_ALT2 "|" IRI_IPV6_ADDRESS_ALT3 "|" IRI_IPV6_ADDRESS_ALT4 "|" IRI_IPV6_ADDRESS_ALT5 "|" IRI_IPV6_ADDRESS_ALT6 "|" IRI_IPV6_ADDRESS_ALT7 "|" IRI_IPV6_ADDRESS_ALT8 "|" IRI_IPV6_ADDRESS_ALT9 ")"
#define IRI_UCSCHARS_INTERVALS "\\xa0-\\ud7ff\\uf900-\\ufdcf\\ufdf0-\\uffef"
#define IRI_PRIVATE_INTERVALS "\\ue000-\\uf8ff"
// sub-delims     = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
#define IRI_SUB_DELIMS "[!$'()*+,;=]"
#define URI_ALPHA "[a-zA-Z]"
#define URI_HEX "[0-9A-Fa-f]"
#define URI_ESCAPED "(?:%" URI_HEX "{2})"
// ipchar         = iunreserved / pct-encoded / sub-delims / ":" / "@"
#define IRI_PCHAR "(?:" "[a-zA-Z0-9\\-_.~:@&=+$," IRI_UCSCHARS_INTERVALS "]|" URI_ESCAPED "|" IRI_SUB_DELIMS ")"
#define IRI_QUERY "(?:(?:" IRI_PCHAR "|[" IRI_PRIVATE_INTERVALS "/\\?])*)"
#define IRI_SEGMENT "(?:(?:" IRI_PCHAR ")*)"
#define IRI_SEGMENT_NZ "(?:(?:" IRI_PCHAR ")+)"
#define IRI_PATH_SEGMENTS "(?:" IRI_SEGMENT "(?:/" IRI_SEGMENT ")*)"
#define URI_PORT "\\d*"
#define IRI_IP_LITERAL "(?:\\[" IRI_IPV6_ADDRESS "\\])"
#define IRI_IREG_NAME "(?:(?:" IRI_UNRESERVED "|" URI_ESCAPED "|" IRI_SUB_DELIMS ")*)"
#define IRI_HOST "(?:" IRI_IP_LITERAL "|" IRI_IPV4ADDRESS "|" IRI_IREG_NAME ")"
#define IRI_UNRESERVED "[a-zA-Z0-9\\-._~" IRI_UCSCHARS_INTERVALS "]"
#define IRI_USERINFO "(?:" URI_ESCAPED "|" IRI_UNRESERVED "|" IRI_SUB_DELIMS "|:)*"
#define IRI_AUTHORITY "(?:(?:" IRI_USERINFO "@)?" IRI_HOST "(?::" URI_PORT ")?)"
#define URI_SCHEME "(?:" URI_ALPHA "(?:[a-zA-Z0-9+-.])*)"
#define IRI_ABS_PATH "(?:/" IRI_PATH_SEGMENTS ")"
#define IRI_NET_PATH "(?://" URI_AUTHORITY IRI_ABS_PATH "?)"
#define IRI_PATH_ABEMPTY "(?:(?:/" IRI_SEGMENT ")*)"
#define IRI_PATH_ROOTLESS "(?:(?:" IRI_SEGMENT_NZ IRI_PATH_ABEMPTY ")?)"
#define IRI_PATH_ABSOLUTE "(?:/(?:" IRI_PATH_ROOTLESS ")?)"
#define IRI_PATH_EMPTY "(?:)"
#define IRI_HIER_PART "(?:(?://" IRI_AUTHORITY IRI_PATH_ABEMPTY ")|" IRI_PATH_ABSOLUTE "|" IRI_PATH_ROOTLESS "|" IRI_PATH_EMPTY ")"
#define IRI_FRAGMENT "(?:(?:" IRI_PCHAR "|[/\\?])*)"
#define IRI_IRI "(?:" URI_SCHEME ":" IRI_HIER_PART "(?:\\?" IRI_QUERY ")?(?:#" IRI_FRAGMENT ")?)"

#if defined _DEBUG && defined EXTREME_REGEXP_TESTING
TestRegExp(URI_ESCAPED);
TestRegExp("[a-zA-Z0-9\\-._~]");
TestRegExp("[\\ud7ff]");
TestRegExp("[\\xa0]");
TestRegExp("[\\xa0-\\ud7ff]");
TestRegExp("[\\xa0-\\ud7ff\\uf900-\\ufdcf\\ufdf0-\\uffef]");
TestRegExp("[" IRI_UCSCHARS_INTERVALS "]");
TestRegExp(IRI_UNRESERVED);
TestRegExp(IRI_SUB_DELIMS);
TestRegExp(IRI_USERINFO);
TestRegExp(IRI_IREG_NAME);
TestRegExp(IRI_DEC_OCTET);
TestRegExp(IRI_IPV4ADDRESS);
TestRegExp(IRI_IP_LITERAL);
TestRegExp(IRI_HOST);
TestRegExp(URI_PORT);
TestRegExp(IRI_AUTHORITY);
TestRegExp(IRI_PATH_ABEMPTY);
TestRegExp(IRI_PATH_ABSOLUTE);
TestRegExp(IRI_PATH_ROOTLESS);
TestRegExp(IRI_PATH_EMPTY);
TestRegExp(URI_SCHEME);
TestRegExp(IRI_HIER_PART);
TestRegExp(IRI_QUERY);
TestRegExp(IRI_FRAGMENT);
TestRegExp("(?:" URI_SCHEME ":)");
TestRegExp("(?:" URI_SCHEME ":" IRI_HIER_PART ")");
TestRegExp("(?:"  "\\:" IRI_HIER_PART "(?:\\?"  ")?)");
TestRegExp("(?:"  "\\:"  "(?:\\?" IRI_QUERY ")?)");
TestRegExp("(?:"  "\\:" IRI_HIER_PART "(?:\\?" IRI_QUERY ")?)");
TestRegExp("(?:" URI_SCHEME "\\:" IRI_HIER_PART "(?:\\?" IRI_QUERY ")?)");
TestRegExp("(?:" URI_SCHEME ":" IRI_HIER_PART "(?:\\?" IRI_QUERY ")?)");
TestRegExp("(?:" URI_SCHEME ":" IRI_HIER_PART "(?:\\#" IRI_FRAGMENT ")?)");
TestRegExp(IRI_IRI);
const char char_pattern_query[] = "^" IRI_QUERY "$";
OpString pattern_query;
if (OpStatus::IsSuccess(pattern_query.Set(char_pattern_query, sizeof(char_pattern_query)-1)))
{
	OP_ASSERT(MatchesRegExp(pattern_query.CStr(), UNI_L("f=g")));
}
const char char_pattern_query2[] = "^(?:\\?" IRI_QUERY ")?$";
OpString pattern_query2;
if (OpStatus::IsSuccess(pattern_query2.Set(char_pattern_query2, sizeof(char_pattern_query2)-1)))
{
	OP_ASSERT(MatchesRegExp(pattern_query2.CStr(), UNI_L("?f=h")));
}
const char char_pattern_query3[] = "^" IRI_FRAGMENT "$";
OpString pattern_query3;
if (OpStatus::IsSuccess(pattern_query3.Set(char_pattern_query3, sizeof(char_pattern_query3)-1)))
{
	OP_ASSERT(MatchesRegExp(pattern_query3.CStr(), UNI_L("foo")));
}
const char char_pattern_query4[] = "^(?:\\#" IRI_FRAGMENT ")?$";
OpString pattern_query4;
if (OpStatus::IsSuccess(pattern_query4.Set(char_pattern_query4, sizeof(char_pattern_query4)-1)))
{
	OP_ASSERT(MatchesRegExp(pattern_query4.CStr(), UNI_L("#foo")));
}
#endif // _DEBUG && EXTREME_REGEXP_TESTING
	const char* char_pattern_iri = "^" IRI_IRI "$";
	const size_t char_pattern_irl_len = sizeof("^" IRI_IRI "$") - 1;

	OP_ASSERT(op_strlen(char_pattern_iri) == char_pattern_irl_len);

	OpString pattern_obj; // Copy the 4KB+ reg.exp. into this to get it as uni_char.
	if (OpStatus::IsSuccess(pattern_obj.Set(char_pattern_iri, char_pattern_irl_len)))
	{
		const uni_char* pattern_iri = pattern_obj.CStr();
		OP_ASSERT(pattern_iri);
#if defined _DEBUG && defined EXTREME_REGEXP_TESTING
		OP_ASSERT(MatchesRegExp(pattern_iri, UNI_L("http://www.opera.com/?hej=hipp")));
		OP_ASSERT(MatchesRegExp(pattern_iri, UNI_L("http://www.opera.com/?hej=hipp#foo")));
#endif // defined _DEBUG && defined EXTREME_REGEXP_TESTING
		if (!MatchesRegExp(pattern_iri, value))
		{
			return VALIDATE_ERROR_TYPE_MISMATCH;
		}
	}

	// Matched or we got OOM.
	return VALIDATE_OK;
}

/* static */
ValidationResult FormValidator::ValidateDate(HTML_Element* fhe,
											 const uni_char* value)
{
	// A date (year, month, day) encoded according to [ISO8601], e.g.:
	// 1995-12-31. User agents are expected to show an appropriate widget,
	// such as a datepicker.

	DaySpec day;
	if (!day.SetFromISO8601String(value))
	{
		return VALIDATE_ERROR_TYPE_MISMATCH;
	}

	ValidationResult val_res = VALIDATE_OK;

	// min, max, step
	double step_base = 0.0; // XXX "0 as a date"
	const uni_char* max = fhe->GetStringAttr(ATTR_MAX);
	if (max)
	{
		DaySpec max_date;
		if (max_date.SetFromISO8601String(max))
		{
			step_base = max_date.AsDouble();
			if (day.IsAfter(max_date))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
			}
		}
	}

	const uni_char* min = fhe->GetStringAttr(ATTR_MIN);
	if (min)
	{
		DaySpec min_date;
		if (min_date.SetFromISO8601String(min))
		{
			step_base = min_date.AsDouble(); // Overwrites max. Min goes first
			if (day.IsBefore(min_date))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_UNDERFLOW);
			}
		}
	}

	if (!FormValidator::CheckStep(fhe, step_base, day.AsDouble(),
								  TRUE, TRUE, 1.0))
	{
		val_res.SetError(VALIDATE_ERROR_STEP_MISMATCH);
	}

	return val_res;
}

/** Determine if 'x' is an integral-valued double, with an 'epsilon'
    margin of error on either side. */
static BOOL
op_isintegral_fuzzy(double x, double epsilon)
{
	double intpart;
	double fraction = op_modf(op_fabs(x), &intpart);
	return fraction > 0.5 ? (fraction > (1.0 - epsilon)) : (fraction <= epsilon);
}

/* static */
BOOL FormValidator::CheckStep(HTML_Element* elm, double step_base,
							  double value,
							  BOOL require_integer_step,
							  BOOL require_positive_step,
							  double default_step)
{
	const uni_char* step_str = elm->GetStringAttr(ATTR_STEP);
	double step;
	BOOL use_default = FALSE;
	if (!step_str)
	{
		use_default = TRUE;
		step = 0.0;  // Initialize to silence stupid compiler
	}
	else
	{
		// step is not NULL
		if (uni_str_eq(step_str, "any"))
		{
			return TRUE; // Step is ok, since we shouldn't check it
		}

		if (!WebForms2Number::StringToDouble(step_str, step) ||
			(require_positive_step && step < 0.0) ||
			step == 0.0)
		{
			use_default = TRUE;
		}
	}

	if (use_default)
	{
		if (default_step == 0.0)
		{
			// No default, no correct step specified
			return TRUE;
		}
		step = default_step; // The default value
	}

	if (op_fabs(step) < 0.0009)
	{
		// We can treat too small steps as "any" per the specification
		return TRUE;
	}

	if (require_integer_step)
	{
		if (!op_isintegral_fuzzy(step, 0.00001))
		{
			if (default_step == 0.0)
			{
				// No default, no correct step specified
				return TRUE;
			}
			step = default_step; // The default value
		}
	}

	OP_ASSERT(step > 0.0);

	double diff = value - step_base;
	double step_count = diff / step;

	return op_isintegral_fuzzy(step_count, 0.00000001);
}

/* static */
ValidationResult FormValidator::ValidateWeek(HTML_Element* fhe,
											 const uni_char* value)
{
	// A date consisting of a year and a week number encoded according
	// to [ISO8601], e.g.: 1996-W52. This type is used most frequently
	// for dates in European industry....

	WeekSpec value_week;
	if (!value_week.SetFromISO8601String(value))
	{
		return VALIDATE_ERROR_TYPE_MISMATCH;
	}

	ValidationResult val_res = VALIDATE_OK;

	// min, max, step
	double step_base = 0.0;

	const uni_char* max = fhe->GetStringAttr(ATTR_MAX);
	if (max)
	{
		WeekSpec max_week;
		if (max_week.SetFromISO8601String(max))
		{
			step_base = max_week.AsDouble();
			if (value_week.IsAfter(max_week))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
			}
		}
	}

	const uni_char* min = fhe->GetStringAttr(ATTR_MIN);
	if (min)
	{
		WeekSpec min_week;
		if (min_week.SetFromISO8601String(min))
		{
			step_base = min_week.AsDouble();

			if (value_week.IsBefore(min_week))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_UNDERFLOW);
			}
		}
	}

	if (!FormValidator::CheckStep(fhe, step_base,
								  value_week.AsDouble(),
								  TRUE, TRUE, 1.0))
	{
		val_res.SetError(VALIDATE_ERROR_STEP_MISMATCH);
	}

	return val_res;
}

/* static */
ValidationResult FormValidator::ValidateTime(HTML_Element* fhe,
											 const uni_char* value)
{
	// A time (hour, minute) encoded according to [ISO8601] with no
	// time zone, e.g.: 23:59. User agents are expected to show an
	// appropriate widget, such as a clock. UAs should make it clear
	// to the user that the time does not carry any time zone information.

	TimeSpec time;
	if (!time.SetFromISO8601String(value))
	{
		return VALIDATE_ERROR_TYPE_MISMATCH;
	}

	// min, max, step
	double step_base = 0.0;
	ValidationResult val_res = VALIDATE_OK;
	const uni_char* max = fhe->GetStringAttr(ATTR_MAX);
	if (max)
	{
		TimeSpec max_time;
		if (max_time.SetFromISO8601String(max))
		{
			step_base = max_time.AsDouble();
			if (time.IsAfter(max_time))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
			}
		}
	}

	const uni_char* min = fhe->GetStringAttr(ATTR_MIN);
	if (min)
	{
		TimeSpec min_time;
		if (min_time.SetFromISO8601String(min))
		{
			step_base = min_time.AsDouble();
			if (time.IsBefore(min_time))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_UNDERFLOW);
			}
		}
	}

	if (!FormValidator::CheckStep(fhe, step_base, time.AsDouble(),
								  FALSE, TRUE, 60.0))
	{
		val_res.SetError(VALIDATE_ERROR_STEP_MISMATCH);
	}

	return val_res;
}

/* static */
ValidationResult FormValidator::ValidateEmail(const uni_char* value, BOOL multiple)
{
	if (multiple)
	{
		// Check each string between commas for validity separately
		OpString address;
		while (TRUE)
		{
			const uni_char *comma = uni_strstr(value, ",");
			if (comma)
				address.Set(value, comma - value);
			else
				address.Set(value);

			// The spec indicates that spaces should work in combination with the commas
			address.Strip();

#ifdef FORMS_VALIDATE_EMAIL
			if (!IsValidEmailAddress(address))
				return VALIDATE_ERROR_TYPE_MISMATCH;
#endif // FORMS_VALIDATE_EMAIL
			if (comma)
				value = comma + 1;
			else
				break;
		};
	}
#ifdef FORMS_VALIDATE_EMAIL
	else if (!IsValidEmailAddress(value))
	{
		return VALIDATE_ERROR_TYPE_MISMATCH;
	}
#endif // FORMS_VALIDATE_EMAIL
	return VALIDATE_OK;
}

/* static */
ValidationResult FormValidator::ValidateNumber(HTML_Element* fhe, const uni_char* value)
{
	// A number. The allowed precision of the number decides what UI user
	// agents may show, and is dicussed below, under the precision attribute.
	//
	// Numbers must be submitted as follows: an optional minus sign ("-"),
	// one or more decimal integers, optionally a decimal point (".")
	// and a decimal fractional part, together forming a number
	// representing the base, followed optionally by the lowercase
	// literal letter "e", another optional minus sign, and a decimal
	// integer exponent representing the index of a power of ten with
	// which to multiply the base to get the resulting number.
	//
	// For example, negative-root-two, to 32 significant figures,
	// would be -1.4142135623730950488016887242097e0, the radius of the
	// earth given in furlongs would be 3.17053408e4, and the answer to
	// the life, the universe and everything could be any of
	// (amongst others) 42, 0042.000, 42e0, 4.2e1, or 420e-1.
	//
	// This format is designed to be compatible with scanf(3)'s %f format
	// and similar parsers while being easier to parse than required by
	// some other floating point syntaxes. If the exponent part is omitted
	// it is assumed to be zero.
	//
	// Note that +0, 0e+0, +0e0 are invalid numbers (the minus sign
	// cannot be replaced by a plus sign for positive numbers, it must
	// simply be dropped). UAs must not submit numbers in invalid formats.
	//
	// The submission format is not intended to be the format seen and used
	// by users. UAs may use whatever format and UI is appropriate for
	// user interaction; the description above is simply the submission format.

	ValidationResult val_res = VALIDATE_OK;
	double value_number;
	if (!WebForms2Number::StringToDouble(value, value_number))
	{
		val_res.SetError(VALIDATE_ERROR_TYPE_MISMATCH);
		// If it's no number, no need to check anything else. The custom error
		// is handled outside of this
		return val_res;
	}

	val_res.SetErrorsFrom(ValidateNumberForMinMaxStep(fhe, value_number));

	return val_res;
}

/* static */
ValidationResult FormValidator::ValidateNumberForMinMaxStep(HTML_Element* fhe,
											 double value_number)
{
	ValidationResult val_res = VALIDATE_OK;

	double min_value;
	double max_value;
	double step_base;
	double step_value;
	WebForms2Number::GetNumberRestrictions(fhe, min_value, max_value,
										   step_base, step_value);

	if (value_number > max_value)
	{
		val_res.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
	}

	if (value_number < min_value)
	{
		val_res.SetError(VALIDATE_ERROR_RANGE_UNDERFLOW);
	}

	if (!CheckStep(fhe, step_base, value_number, FALSE, FALSE, 1.0))
	{
		val_res.SetError(VALIDATE_ERROR_STEP_MISMATCH);
	}

	return val_res;
}

/* static */
ValidationResult FormValidator::ValidateMonth(HTML_Element* fhe, const uni_char* value)
{
	// A date consisting of a year and a month encoded according to [ISO8601],
	// e.g.: 1995-12. This type is used most frequently for credit
	// card expiry dates.

	MonthSpec value_month;
	if (!value_month.SetFromISO8601String(value))
	{
		return VALIDATE_ERROR_TYPE_MISMATCH;
	}

	// min, max, step
	double step_base = 0.0;
	ValidationResult val_res = VALIDATE_OK;

	const uni_char* max = fhe->GetStringAttr(ATTR_MAX);
	if (max)
	{
		MonthSpec max_month;
		if (max_month.SetFromISO8601String(max))
		{
			step_base = max_month.AsDouble();
			if (value_month.IsAfter(max_month))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
			}
		}
	}

	const uni_char* min = fhe->GetStringAttr(ATTR_MIN);
	if (min)
	{
		MonthSpec min_month;
		if (min_month.SetFromISO8601String(min))
		{
			step_base = min_month.AsDouble();

			if (value_month.IsBefore(min_month))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_UNDERFLOW);
			}
		}
	}

	if (!FormValidator::CheckStep(fhe, step_base, value_month.AsDouble(), TRUE, TRUE, 1.0))
	{
		val_res.SetError(VALIDATE_ERROR_STEP_MISMATCH);
	}

	return val_res;
}

/* static */
ValidationResult FormValidator::ValidateDateTime(HTML_Element* fhe, const uni_char* value)
{
	// A date and time (year, month, day, hour, minute, second) encoded
	// according to [ISO8601] with the time zone set to UTC,
	// e.g.: 1995-12-31T23:59Z. User agents are expected to show an
	// appropriate widget.

	BOOL include_time_zone = (fhe->GetInputType() == INPUT_DATETIME);
	OP_ASSERT(include_time_zone || fhe->GetInputType() == INPUT_DATETIME_LOCAL);

	DateTimeSpec date_time;
	if (!date_time.SetFromISO8601String(value, include_time_zone))
	{
		return VALIDATE_ERROR_TYPE_MISMATCH;
	}

	// min, max
	double step_base = 0.0;

	ValidationResult val_res = VALIDATE_OK;

	const uni_char* max = fhe->GetStringAttr(ATTR_MAX);
	if (max)
	{
		DateTimeSpec max_date_time;
		if (max_date_time.SetFromISO8601String(max, include_time_zone))
		{
			step_base = max_date_time.AsDouble();
			if (date_time.IsAfter(max_date_time))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_OVERFLOW);
			}
		}
	}
	const uni_char* min = fhe->GetStringAttr(ATTR_MIN);
	if (min)
	{
		DateTimeSpec min_date_time;
		if (min_date_time.SetFromISO8601String(min, include_time_zone))
		{
			step_base = min_date_time.AsDouble(); // Rather min than max
			if (date_time.IsBefore(min_date_time))
			{
				val_res.SetError(VALIDATE_ERROR_RANGE_UNDERFLOW);
			}
		}
	}

	// We have rounding problems here. Since we both have very long range and a high precision.
	if (!CheckStep(fhe, step_base, date_time.AsDouble(), FALSE, TRUE, 60000.0))
	{
		val_res.SetError(VALIDATE_ERROR_STEP_MISMATCH);
	}

	return val_res;
}

OP_STATUS FormValidator::MaybeDisplayErrorMessage(HTML_Element* form_control_element) const
{
	if (m_frames_doc->GetNextOnInvalidCauseAlert())
	{
		// Look at the field again to see what was wrong
		ValidationResult val_res = ValidateSingleControl(form_control_element);
		if (!val_res.IsOk())
		{
			HTML_Document* html_doc = m_frames_doc->GetHtmlDocument();
			LogicalDocument* logdoc = m_frames_doc->GetLogicalDocument();
			if (!html_doc || !logdoc || !logdoc->GetRoot() || !logdoc->GetRoot()->IsAncestorOf(form_control_element))
			{
				// The element is no longer in the tree so an error message is useless.
				return OpStatus::ERR;
			}

			// If this invalid event was caused by a submit, then only the first
			// invalid control should have an error message. Else all should
			// have it, but right now we rather have too few than too many.
			m_frames_doc->SetNextOnInvalidCauseAlert(FALSE);
			// Focus the form element (control) that was bad
			html_doc->FocusElement(form_control_element, HTML_Document::FOCUS_ORIGIN_UNKNOWN);

			// Sorted by importance. Several errors can be set at the same time

			BOOL include_title_attr = FALSE;
			BOOL custom_error_message = FALSE;
			BOOL use_value_length_as_value = FALSE;
			Str::LocaleString error_message_id = Str::NOT_A_STRING;
			const uni_char* extra_error_info_data = NULL;
			OpString extra_error_info_data_buf;
			if (val_res.HasError(VALIDATE_ERROR_CUSTOM))
			{
				custom_error_message = TRUE;
			}
			else if (val_res.HasError(VALIDATE_ERROR_TYPE_MISMATCH))
			{
				if (form_control_element->Type() == HE_INPUT)
				{
					switch (form_control_element->GetInputType())
					{
					case INPUT_EMAIL:
						error_message_id = Str::D_VALIDATE_ERROR_TYPE_MISMATCH_EMAIL;
						break;
					case INPUT_URI:
						error_message_id = Str::D_VALIDATE_ERROR_TYPE_MISMATCH_URL;
						break;
					case INPUT_NUMBER:
					case INPUT_RANGE:
						error_message_id = Str::D_VALIDATE_ERROR_TYPE_MISMATCH_NUMBER;
						break;
					case INPUT_FILE:
						error_message_id = Str::D_VALIDATE_ERROR_TYPE_MISMATCH_FILE;
						break;
					default:
						error_message_id = Str::D_VALIDATE_ERROR_TYPE_MISMATCH_STR;
						break;
					}
				}
				else
				{
					error_message_id = Str::D_VALIDATE_ERROR_TYPE_MISMATCH_STR;
				}
			}
			else if (val_res.HasError(VALIDATE_ERROR_RANGE_UNDERFLOW))
			{
				error_message_id = Str::D_VALIDATE_ERROR_RANGE_UNDERFLOW_STR;
				extra_error_info_data = form_control_element->GetStringAttr(ATTR_MIN);
			}
			else if (val_res.HasError(VALIDATE_ERROR_RANGE_OVERFLOW))
			{
#if LANGUAGE_DATABASE_VERSION <= 842
				OP_ASSERT(!"Using too old string database that's missing required strings");
#else
				if (form_control_element->GetInputType() == INPUT_FILE)
				{
					error_message_id = Str::D_VALIDATE_ERROR_TOO_MANY_FILES;
					OP_ASSERT(!form_control_element->GetBoolAttr(ATTR_MULTIPLE)); ///< Why did we get here otherwise?
					extra_error_info_data = UNI_L("1");
				}
				else
#endif // LANGUAGE_DATABASE_VERSION
				{
					error_message_id = Str::D_VALIDATE_ERROR_RANGE_OVERFLOW_STR;
					extra_error_info_data = form_control_element->GetStringAttr(ATTR_MAX);
				}
			}
			else if (val_res.HasError(VALIDATE_ERROR_STEP_MISMATCH))
			{
				switch (form_control_element->GetInputType())
				{
				case INPUT_RANGE:
				case INPUT_NUMBER:
					error_message_id = Str::S_VALIDATE_ERROR_STEP_MISMATCH_NUMBER;
					break;
				default:
					error_message_id = Str::S_VALIDATE_ERROR_STEP_MISMATCH_DATE;
					break;
				}
				extra_error_info_data = form_control_element->GetStringAttr(ATTR_STEP);
			}
			else if (val_res.HasError(VALIDATE_ERROR_TOO_LONG))
			{
				if (form_control_element->Type() == HE_INPUT &&
					form_control_element->GetInputType() == INPUT_FILE)
				{
					error_message_id = Str::D_VALIDATE_ERROR_FILE_TOO_LONG;
				}
				else
				{
					error_message_id = Str::D_VALIDATE_ERROR_TOO_LONG_STR;
				}
				int max_length = form_control_element->GetNumAttr(ATTR_MAXLENGTH);
				RETURN_IF_ERROR(extra_error_info_data_buf.AppendFormat(UNI_L("%d"), max_length));
				extra_error_info_data = extra_error_info_data_buf.CStr();
				// The "value" for this error is the number of used characters.
				use_value_length_as_value = TRUE;
			}
			else if (val_res.HasError(VALIDATE_ERROR_PATTERN_MISMATCH))
			{
				error_message_id = Str::D_VALIDATE_ERROR_PATTERN_MISMATCH_STR;
				include_title_attr = TRUE;
			}
			else // if (val_res.HasError(VALIDATE_ERROR_REQUIRED))
			{
				OP_ASSERT(val_res.HasError(VALIDATE_ERROR_REQUIRED));
				error_message_id = Str::D_VALIDATE_ERROR_REQUIRED_STR;
			}

			OpString error_message;
			if (!custom_error_message)
			{
				RETURN_IF_ERROR(g_languageManager->GetString(error_message_id, error_message));
				OP_ASSERT(!error_message.IsEmpty()); // Bad locale data!
			}

			if (!error_message.IsEmpty() && uni_strstr(error_message.CStr(), UNI_L("%")))
			{
				// Has placeholder for the entered value
				OpString value;
				FormValue* form_value = form_control_element->GetFormValue();
				if (form_value->HasMeaningfulTextRepresentation())
				{
					RETURN_IF_MEMORY_ERROR(form_value->GetValueAsText(form_control_element, value));
				}
				if (use_value_length_as_value)
				{
					int length = value.Length();
					value.Empty();
					RETURN_IF_ERROR(value.AppendFormat(UNI_L("%d"), length));
				}
				const uni_char* val_p = value.IsEmpty() ? UNI_L("") : value.CStr();
				UINT32 value_len = value.Length() + 1;

				OpString pattern;
				RETURN_IF_ERROR(pattern.Set(error_message));

				int expected_len = pattern.Length() - 2 + value_len + 1; // sure to include room for the NULL
				if (extra_error_info_data)
				{
					expected_len += uni_strlen(extra_error_info_data);
				}

				uni_char* err_mess_p = error_message.Reserve(expected_len);
				if (!err_mess_p)
				{
					return OpStatus::ERR_NO_MEMORY;
				}

				if (uni_strstr(error_message.CStr(), UNI_L("%2")) && !extra_error_info_data)
				{
					// Crash fix
					extra_error_info_data = UNI_L("");
				}

				OP_ASSERT(!uni_strstr(error_message.CStr(), UNI_L("%3")));
				OP_ASSERT(!uni_strstr(error_message.CStr(), UNI_L("%4")));

				uni_snprintf_ss(err_mess_p, expected_len, pattern.CStr(), val_p, extra_error_info_data);
			}

			if (custom_error_message)
			{
				RETURN_IF_ERROR(error_message.Append(form_control_element->GetSpecialStringAttr(FORMS_ATTR_USER_SET_INVALID_FLAG, SpecialNs::NS_FORMS)));
			}

			if (include_title_attr)
			{
				const uni_char* title = form_control_element->GetStringAttr(ATTR_TITLE);
				if (title && *title)
				{
					RETURN_IF_ERROR(error_message.Append("\n"));
					// Sanitize?
					RETURN_IF_ERROR(error_message.Append(title));
				}
			}

			if (error_message.IsEmpty())
			{
				return OpStatus::ERR;
			}
			return DisplayValidationErrorMessage(form_control_element, error_message.CStr());
		}
	}

	return OpStatus::OK;
}


/* private */
OP_STATUS FormValidator::DisplayValidationErrorMessage(HTML_Element* form_control_element, const uni_char* message) const
{
	OP_ASSERT(message);

	// Don't show any error message window for background tabs.
	if (!m_frames_doc->GetWindow()->IsVisibleOnScreen())
		return OpStatus::OK;

#if 1
	ValidationErrorWindow::Display(form_control_element, message);
	return OpStatus::OK;
#else

	MessageHandler* mh = m_frames_doc->GetMessageHandler();
	OpMessage finished_message = MSG_NO_MESSAGE;
	OpString dialog_title;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_VALIDATE_ERROR_DIALOG_TITLE, dialog_title));

	// Add the server name to the message to make it harder to fool the user into doing something wrong
	OpString message_with_server_name;
	ServerName *sn = m_frames_doc->GetURL().GetServerName();
	if (sn)
	{
		const uni_char *snname = sn->UniName();
		if (snname && *snname)
		{
			RETURN_IF_ERROR(message_with_server_name.Set("<"));
			RETURN_IF_ERROR(message_with_server_name.Append(snname));
			RETURN_IF_ERROR(message_with_server_name.Append(">:\n\n"));
		}
	}
	RETURN_IF_ERROR(message_with_server_name.Append(message));

	(void)mh->PostMessageBox(MBT_ERROR, dialog_title.CStr(), message_with_server_name.CStr(), Str::NOT_A_STRING, finished_message);

	return OpStatus::OK;
#endif // 0
}

void FormValidator::DispatchFormChange(HTML_Element* form_element) const
{
	OP_ASSERT(form_element->Type() == HE_FORM); // This is not for HE_ISINDEX, nor for any other tag

	ES_Thread* thread = NULL;
	ES_ThreadScheduler *scheduler = m_frames_doc->GetESScheduler();
	if (scheduler && scheduler->IsActive())
	{
		thread = scheduler->GetCurrentThread();
	}

	FormIterator form_iterator(m_frames_doc, form_element);
	form_iterator.SetIncludeOutputElements();
	while(HTML_Element* fhe = form_iterator.GetNext())
	{
		SendOnFormChangeEvent(fhe, thread);
	}
	SendOnFormChangeEvent(form_element, thread);
}

void FormValidator::DispatchFormInput(HTML_Element* form_element) const
{
	OP_ASSERT(form_element->Type() == HE_FORM); // This is not for HE_ISINDEX, nor for any other tag

	ES_Thread* thread = NULL;
	ES_ThreadScheduler *scheduler = m_frames_doc->GetESScheduler();
	if (scheduler && scheduler->IsActive())
{
		thread = scheduler->GetCurrentThread();
	}

	FormIterator form_iterator(m_frames_doc, form_element);
	form_iterator.SetIncludeOutputElements();
	while(HTML_Element* fhe = form_iterator.GetNext())
	{
		SendOnFormInputEvent(fhe, thread);
	}
	SendOnFormInputEvent(form_element, thread);
}
