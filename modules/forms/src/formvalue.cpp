/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvalue.h"

#include "modules/forms/formvaluetextarea.h"
#include "modules/forms/formvaluetext.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalueradiocheck.h"
#ifdef FORMS_KEYGEN_SUPPORT
# include "modules/forms/formvaluekeygen.h"
#endif // FORMS_KEYGEN_SUPPORT
#include "modules/forms/formsenum.h"
#include "modules/forms/formmanager.h"

#include "modules/forms/piforms.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/forms/webforms2support.h"
#include "modules/forms/webforms2number.h"
#include "modules/forms/formvaluewf2date.h"
#include "modules/forms/formvaluecolor.h"
#include "modules/forms/formvalueemail.h"
#include "modules/forms/datetime.h"

 // Space for a number as string
static const int MAX_NUMBER_BUF_SIZE = 32;  // Max length of string representation of a double.

/* static */
OP_STATUS FormValue::ConstructFormValue(HTML_Element* he,
										HLDocProfile* hld_profile,
										FormValue*& out_value)
{
	OP_ASSERT(he->GetNsType() == NS_HTML);
	out_value = NULL;

	if (FormManager::IsButton(he))
	{
		// Buttons have no stored value other than the value attribute and elements
		// inserted by layout gets "dummy" FormValue objects
		return FormValueNoOwnValue::ConstructFormValueNoOwnValue(he, out_value);
	}

	switch (he->Type())
	{
	case HE_INPUT:
		switch (he->GetInputType())
		{
		// Text values
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_IMAGE:
		case INPUT_FILE:
		case INPUT_TEL:
		case INPUT_SEARCH:
		case INPUT_URI:
			return FormValueText::ConstructFormValueText(he, hld_profile, out_value);
		case INPUT_EMAIL:
			return FormValueEmail::ConstructFormValueEmail(he, hld_profile, out_value);

		// Type="hidden" have no stored value other than the value attribute
		case INPUT_HIDDEN:
			return FormValueNoOwnValue::ConstructFormValueNoOwnValue(he, out_value);
		case INPUT_RADIO:
		case INPUT_CHECKBOX:
			return FormValueRadioCheck::ConstructFormValueRadioCheck(he, out_value);
		case INPUT_NUMBER:
		case INPUT_RANGE:
			return FormValueNumber::ConstructFormValueNumber(he, out_value);
		case INPUT_DATE:
		case INPUT_WEEK:
		case INPUT_TIME:
		case INPUT_MONTH:
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
			return FormValueWF2DateTime::ConstructFormValueWF2DateTime(he, out_value);
		case INPUT_COLOR:
			return FormValueColor::ConstructFormValueColor(he, out_value);
		}
		OP_ASSERT(FALSE); // Missed case
		break;
	case HE_TEXTAREA:
		return FormValueTextArea::ConstructFormValueTextArea(he, out_value);
	case HE_SELECT:
		return FormValueList::ConstructFormValueList(he, out_value);
	case HE_OUTPUT:
		return FormValueOutput::ConstructFormValueOutput(he, out_value);
#ifdef FORMS_KEYGEN_SUPPORT
	case HE_KEYGEN:
		return FormValueKeyGen::ConstructFormValueKeyGen(he, out_value);
#endif // FORMS_KEYGEN_SUPPORT
	default:
		OP_ASSERT(FALSE); // Missed case or wrong caller
	}
	return OpStatus::ERR;
}

OP_STATUS FormValue::GetFormObjectValue(HTML_Element* he, OpString& out_value) const
{
	// This is called if we're external and in that case form_object exists
	FormObject* form_object = he->GetFormObject();
	OP_ASSERT(form_object);
	return GetFormObjectValue(form_object, out_value);
}

OP_STATUS FormValue::GetFormObjectValue(FormObject* form_object, OpString& out_value) const
{
	return form_object->GetFormWidgetValue(out_value);
}

/* static */
OP_STATUS FormValue::FixupCRLF(OpString& text)
{
	// Make sure that all newlines are representented by a CRLF pair.
	// If CR is not followed by LF, add LF.
	// If LF is not preceded by CR, add CR.

	const int orig_length = text.Length();
	if (orig_length == 0)
	{
		return OpStatus::OK;
	}

	uni_char *dest = NULL;

	enum { MEASURE, MODIFY };
	for (int pass = MEASURE; pass <= MODIFY; pass++)
	{
		int newlinecount = 0;
		BOOL lf_found = FALSE;
		int crlfcount = 0;

		OP_ASSERT(text.CStr());
		for (const uni_char* src = text.CStr() + orig_length - 1; src >= text.CStr(); src--)
		{
			BOOL add_newline = FALSE;
			uni_char ch = *src;

			switch (ch)
			{
			case '\n':
				crlfcount++;
				lf_found = TRUE;
				add_newline = TRUE;
				break;

			case '\r':
				crlfcount++;
				if (!lf_found)
				{
					add_newline = TRUE;
				}
				else
				{
					lf_found = FALSE;
				}
				break;

			default:
				lf_found = FALSE;
				if (dest)
				{
					*dest-- = ch;
				}
				break;
			}

			if (add_newline)
			{
				newlinecount++;
				if (dest)
				{
					*dest-- = '\n';
					*dest-- = '\r';
				}
			}
		}

		if (pass == MEASURE)
		{
			int new_length = orig_length - crlfcount + newlinecount * 2;
			OP_ASSERT(new_length >= orig_length);

			if (new_length == orig_length)
			{
				// Nothing to do
				break;
			}

			if (!text.Reserve(new_length))
			{
				return OpStatus::ERR_NO_MEMORY;
			}

			dest = text.CStr() + new_length;
			*dest-- = '\0';
		}
		else
		{
			OP_ASSERT(dest == text.CStr() - 1);
		}
	}

	return OpStatus::OK;
}

/* static */
void FormValue::FixupLF(OpString& text)
{
	// Normalize newlines to a single LF. That is,
	// If CR is followed by LF, remove CR.
	// If CR is not followed by LF, translate to LF.
	// If CR is followed by CR, translate (each) to LF.

	if (uni_char *src = text.CStr())
	{
		uni_char *dest = src;
		for (; *src; src++)
		{
			if (*src == '\r')
			{
				if (src[1] == '\n')
				{
					src++;
				}
				*dest++ = '\n';
			}
			else
			{
				*dest++ = *src;
			}
		}
		*dest = 0;
	}
}

/* virtual */
BOOL FormValue::Externalize(FormObject* form_object_to_seed)
{
	OP_ASSERT(form_object_to_seed);
#ifdef _DEBUG
	HTML_Element* elm = form_object_to_seed->GetHTML_Element();
	OP_ASSERT(elm);
	OP_ASSERT(elm->GetFormValue() == this); // Or this FormValue doesn't belong to the widget
#endif // _DEBUG
	if (IsValueExternally())
	{
		// This can be bad. We've either gotten two Externalize about the same form element
		// and then we're just wasting times and can (if we not break here) replace a value
		// in the widget with the default value. So we have to break here. The other alternative
		// cause is that we've missed an Unexternalize and is now going to externalize the same
		// value to a new widget. In that case we've already lost everything the person wrote
		// in the previous widget.
		// OP_ASSERT(FALSE); // Already external to something else? We must have missed an "Unexternalize" somewhere.
		return FALSE; // It wasn't right to call Externalize
	}
	members.m_is_value_externally = TRUE;
	return TRUE; // Correct to call Externalize
}

/* virtual */
void FormValue::Unexternalize(FormObject* form_object_to_replace)
{
//	OP_ASSERT(IsValueExternally()); // If this fires, then we have already the value internally. Two calls to Unexternalize for the same FormValue?
	OP_ASSERT(form_object_to_replace);
#ifdef _DEBUG
	HTML_Element* elm = form_object_to_replace->GetHTML_Element();
	OP_ASSERT(elm);
	OP_ASSERT(elm->GetFormValue() == this); // Or this FormValue doesn't belong to the widget
#endif // _DEBUG
	members.m_is_user_input_rtl = form_object_to_replace->IsInRTLMode();
	members.m_is_value_externally = FALSE;
}


void FormValue::SetMarkedPseudoClasses(int pseudo_class_flags)
{
	// Can not be in range and out of range at the same time
	OP_ASSERT(!((pseudo_class_flags & CSS_PSEUDO_CLASS_IN_RANGE) &&
		(pseudo_class_flags & CSS_PSEUDO_CLASS_OUT_OF_RANGE)));
	// Can not be enabled and disabled at the same time
	OP_ASSERT(!((pseudo_class_flags & CSS_PSEUDO_CLASS_DISABLED) &&
		(pseudo_class_flags & CSS_PSEUDO_CLASS_ENABLED)));
	// Can not be required and optional at the same time
	OP_ASSERT(!((pseudo_class_flags & CSS_PSEUDO_CLASS_REQUIRED) &&
		(pseudo_class_flags & CSS_PSEUDO_CLASS_OPTIONAL)));
	members.pseudo.m_marked_default_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_DEFAULT) ? 1 : 0;
	members.pseudo.m_marked_invalid_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_INVALID) ? 1 : 0;
	members.pseudo.m_marked_in_range_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_IN_RANGE) ? 1 : 0;
	members.pseudo.m_marked_out_of_range_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_OUT_OF_RANGE) ? 1 : 0;
	members.pseudo.m_marked_required_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_REQUIRED) ? 1 : 0;
	members.pseudo.m_marked_optional_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_OPTIONAL) ? 1 : 0;
	members.pseudo.m_marked_read_only_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_READ_ONLY) ? 1 : 0;
//	members.pseudo.m_marked_read_write_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_READ_WRITE) ? 1 : 0;

	members.pseudo.m_marked_disabled_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_DISABLED) ? 1 : 0;
	members.pseudo.m_marked_enabled_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_ENABLED) ? 1 : 0;
	members.pseudo.m_marked_checked_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_CHECKED) ? 1 : 0;
	members.pseudo.m_marked_indeterminate_pseudo = (pseudo_class_flags & CSS_PSEUDO_CLASS_INDETERMINATE) ? 1 : 0;
}

int FormValue::GetMarkedPseudoClasses()
{
	int pc = 0;
	if ( members.pseudo.m_marked_default_pseudo )
		pc |= CSS_PSEUDO_CLASS_DEFAULT;
	pc |= members.pseudo.m_marked_invalid_pseudo ? CSS_PSEUDO_CLASS_INVALID : CSS_PSEUDO_CLASS_VALID;
	if( members.pseudo.m_marked_in_range_pseudo )
		pc |= CSS_PSEUDO_CLASS_IN_RANGE;
	if( members.pseudo.m_marked_out_of_range_pseudo )
		pc |= CSS_PSEUDO_CLASS_OUT_OF_RANGE;
	OP_ASSERT(!((pc & CSS_PSEUDO_CLASS_IN_RANGE) && (pc & CSS_PSEUDO_CLASS_OUT_OF_RANGE)));

	if (members.pseudo.m_marked_required_pseudo)
		pc |= CSS_PSEUDO_CLASS_REQUIRED;
	if (members.pseudo.m_marked_optional_pseudo)
		pc |= CSS_PSEUDO_CLASS_OPTIONAL;
	if (members.pseudo.m_marked_read_only_pseudo)
		pc |= CSS_PSEUDO_CLASS_READ_ONLY;
	else // if (members.pseudo.m_marked_read_write_pseudo)
		pc |= CSS_PSEUDO_CLASS_READ_WRITE;
	if (members.pseudo.m_marked_disabled_pseudo)
		pc |= CSS_PSEUDO_CLASS_DISABLED;
	if (members.pseudo.m_marked_enabled_pseudo)
		pc |= CSS_PSEUDO_CLASS_ENABLED;
	if (members.pseudo.m_marked_checked_pseudo)
		pc |= CSS_PSEUDO_CLASS_CHECKED;
	if (members.pseudo.m_marked_indeterminate_pseudo)
		pc |= CSS_PSEUDO_CLASS_INDETERMINATE;

	return pc;
}

int FormValue::CalculateFormPseudoClasses(FramesDocument* frames_doc, HTML_Element* he)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(he);
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(he->GetNsType() == NS_HTML); // Only things with FormValue

	int flags = 0;

	HTML_ElementType type = he->Type();
	InputType input_type = he->GetInputType();

	if (he->IsDefaultFormElement(frames_doc))
	{
		flags |= CSS_PSEUDO_CLASS_DEFAULT;
	}
	else if (GetType() == VALUE_RADIOCHECK)
	{
		FormValueRadioCheck* radiocheck = FormValueRadioCheck::GetAs(this);
		if (radiocheck->IsCheckedByDefault(he))
		{
			flags |= CSS_PSEUDO_CLASS_DEFAULT;
		}
	}
#if 0 // not implemented support for styles on options yet
	else if this is an HE_OPTION && this->HasAttr(ATTR_SELECTED)
	{
		flags |= CSS_PSEUDO_CLASS_DEFAULT;
	}
#endif // 0

	FormValidator form_validator(frames_doc);
	ValidationResult val_res = form_validator.ValidateSingleControl(he);
	flags |= val_res.IsOk() ? CSS_PSEUDO_CLASS_VALID : CSS_PSEUDO_CLASS_INVALID;

	// We only set in-range/out-of-range for elements
	// that can be out-of-range.
	switch (GetType())
	{
	case VALUE_NUMBER:
	case VALUE_DATETIMERELATED:
		{
			BOOL out_of_range =
				val_res.HasError(VALIDATE_ERROR_RANGE_OVERFLOW) ||
				val_res.HasError(VALIDATE_ERROR_RANGE_UNDERFLOW);
			flags |= out_of_range ? CSS_PSEUDO_CLASS_OUT_OF_RANGE :
				CSS_PSEUDO_CLASS_IN_RANGE;
		}
	}

	// set :optional and :required on those that support those pseudo classes
	BOOL supports_required_optional = FALSE;
	switch (type)
	{
	case HE_INPUT:
		supports_required_optional = !(FormManager::IsButton(input_type) || input_type == INPUT_IMAGE);
		break;
	case HE_TEXTAREA:
		supports_required_optional = TRUE;
		break;
	}

	if (supports_required_optional)
	{
			BOOL required = he->HasAttr(ATTR_REQUIRED);
			flags |= required ? CSS_PSEUDO_CLASS_REQUIRED : CSS_PSEUDO_CLASS_OPTIONAL;
	}

		// 4.1.6 in the CSS3UI specification:
		//
		// An element whose contents are not user-alterable is
		// :read-only. However, elements whose contents are user-alterable
		// (such as text input fields) are considered to be in a
		// :read-write state. In typical documents, most elements are
		// :read-only. However it may be possible (e.g. in the context of
		// an editor) for any element to become :read-write.

		// Should we limit ourselves to forms? It's too much
		// to set the pseudo class on the whole document
	BOOL supports_readonly = type == HE_TEXTAREA;
	if (type == HE_INPUT)
	{
		switch (input_type)
		{
		case INPUT_TEXT:
		case INPUT_PASSWORD:
		case INPUT_EMAIL:
		case INPUT_URI:
		case INPUT_NUMBER:
		case INPUT_WEEK:
		case INPUT_DATETIME:
		case INPUT_DATETIME_LOCAL:
		case INPUT_MONTH:
		case INPUT_TIME:
		case INPUT_COLOR:
		case INPUT_TEL:
		case INPUT_SEARCH:
			supports_readonly = TRUE;
		}
	}
	BOOL readonly = supports_readonly && he->HasAttr(ATTR_READONLY);
	flags |= readonly ? CSS_PSEUDO_CLASS_READ_ONLY : CSS_PSEUDO_CLASS_READ_WRITE;

	if (type != HE_OUTPUT)
	{
		BOOL disabled = he->IsDisabled(frames_doc);
		flags |= disabled ? CSS_PSEUDO_CLASS_DISABLED : CSS_PSEUDO_CLASS_ENABLED;
	}

	if (GetType() == VALUE_RADIOCHECK)
	{
		FormValueRadioCheck* radio_check = FormValueRadioCheck::GetAs(this);
		if (radio_check->IsChecked(he))
		{
			flags |= CSS_PSEUDO_CLASS_CHECKED;
		}
		if (he->GetInputType() == INPUT_RADIO &&
			!radio_check->IsInCheckedRadioGroup())
		{
			flags |= CSS_PSEUDO_CLASS_INDETERMINATE;
		}
		else if (he->GetInputType() == INPUT_CHECKBOX)
		{
			if (he->GetIndeterminate())
			{
				flags |= CSS_PSEUDO_CLASS_INDETERMINATE;
			}
		}
	}

	return flags;
}

void FormValue::PrepareToReplace(const FormValue& old_other, HTML_Element* elm)
{
	// Copy stuff that isn't depending on the actual value and its type
	members.m_is_value_externally = old_other.members.m_is_value_externally;
#ifdef WAND_SUPPORT
	members.m_exists_in_wand = old_other.members.m_exists_in_wand;
#endif // WAND_SUPPORT

	if (GetType() == VALUE_TEXT && old_other.GetType() == VALUE_TEXT)
	{
		static_cast<FormValueText*>(this)->CopyValueInternal(static_cast<const FormValueText&>(old_other));
		INT32 selection_start, selection_end;
		old_other.GetSelection(elm, selection_start, selection_end);
		SetSelection(elm, selection_start, selection_end);
		SetCaretOffset(elm, old_other.GetCaretOffset(elm));
	}
}


const uni_char* FormValue::GetFormValueTypeString(HTML_Element* he) const
{
	// This method is not reliable.
	OP_ASSERT(he->Type() == HE_INPUT);
	switch (GetType())
	{
	case VALUE_TEXT:
		return UNI_L("text"); // Could be password or file
//		VALUE_TEXTAREA, not a HE_INPUT
// 		VALUE_LIST_SELECTION, not a HE_INPUT
	case VALUE_RADIOCHECK:
		return UNI_L("checkbox"); // Could be a radio button too
	case VALUE_DATETIMERELATED:
		return FormValueWF2DateTime::GetAs(this)->GetFormValueTypeString();
	case VALUE_NUMBER:
		return UNI_L("number"); // Could be range too
	case VALUE_COLOR:
		return UNI_L("color");
//		 VALUE_OUTPUT, not a HE_INPUT
	case VALUE_NO_OWN_VALUE:
		return UNI_L("button"); // Could add, remove, move-up or move-down too
	}
	OP_ASSERT(FALSE);
	return UNI_L("");
}

void FormValue::UpdateSerialNr(FramesDocument *doc)
{
	m_serial_nr = doc->GetNewFormValueSerialNr();
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
/* virtual */
int FormValue::CreateSpellSessionId(HTML_Element* he, OpPoint *point /* = NULL */)
{
	return 0;
}
#endif // INTERNAL_SPELLCHECK_SUPPORT

/* virtual */
BOOL FormValue::IsUserEditableText(HTML_Element* he)
{
	return FALSE;
}

BOOL FormValue::IsUserInputRTL(HTML_Element* he)
{
	if (IsValueExternally())
	{
		return he->GetFormObject()->IsInRTLMode();
	}
	return members.m_is_user_input_rtl;
}

// FormValueOutput

/* virtual */
FormValue* FormValueOutput::Clone(HTML_Element *he)
{
	OP_ASSERT(!IsValueExternally());

	FormValueOutput* clone = OP_NEW(FormValueOutput, ());
	if (clone)
	{
		clone->GetHasDefaultValueBool() = GetHasDefaultValueBool();
		OpStatus::Ignore(clone->m_default_value.Set(m_default_value));
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueOutput::ResetToDefault(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(!IsValueExternally()); // There's no "externally" for output values

	OP_ASSERT(FALSE); // ResetToDefault doesn't work for output elements because we need a FramesDocument. Call FormValueOutput::ResetOutputToDefault instead.
	return OpStatus::ERR;
}

OP_STATUS FormValueOutput::ResetOutputToDefault(HTML_Element* he, FramesDocument* frames_doc)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(!IsValueExternally()); // There's no "externally" for output values

	OpString default_value;
	RETURN_IF_MEMORY_ERROR(GetDefaultValueAsText(he, default_value));
	GetHasDefaultValueBool() = FALSE;
	m_default_value.Empty();

	// Remove previous content
	return ReplaceTextContents(he, frames_doc, default_value.CStr());
}

/* virtual */
OP_STATUS FormValueOutput::GetValueAsText(HTML_Element* he, OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueOutput*>(this));
	OP_ASSERT(!IsValueExternally());

	int text_len = he->GetTextContentLength();
	if (text_len > 0)
	{
		uni_char* out_value_p = out_value.Reserve(text_len+1); // add one for the NULL
		if (!out_value_p)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		he->GetTextContent(out_value_p, text_len+1);
	}
	else
	{
		out_value.Empty();
	}

	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueOutput::SetValueFromText(HTML_Element* he, const uni_char* in_value)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(!IsValueExternally());

	OP_ASSERT(FALSE); // Call FormValueOutput::SetOutputValueFromText instead

	return OpStatus::OK;
}

OP_STATUS FormValueOutput::SetOutputValueFromText(HTML_Element* he, FramesDocument* frames_doc,
											const uni_char* in_value)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(!IsValueExternally());

	if (!GetHasDefaultValueBool())
	{
		RETURN_IF_ERROR(GetValueAsText(he, m_default_value));
		GetHasDefaultValueBool() = TRUE;
	}

	ReplaceTextContents(he, frames_doc, in_value);

	return OpStatus::OK;
}

/* private */
OP_STATUS FormValueOutput::ReplaceTextContents(HTML_Element* he, FramesDocument* frames_doc,
											const uni_char* new_value)
{
	OP_ASSERT(he);
	OP_ASSERT(frames_doc);

	// Remove previous content
	HTML_Element* child = he->FirstChild();
	while (child)
	{
		HTML_Element* current = child;
		child = child->Suc();
		current->Remove(frames_doc);
		if (current->Clean(frames_doc))
		{
			current->Free(frames_doc);
		}
	}

	if (new_value)
	{
		// Insert new text node
		HTML_Element* text_node = HTML_Element::CreateText(new_value, uni_strlen(new_value), FALSE, FALSE, FALSE);
		if (!text_node)
		{
			DELETE_HTML_Element(text_node);
			return OpStatus::ERR_NO_MEMORY;
		}
		text_node->UnderSafe(frames_doc, he);
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS FormValueOutput::ConstructFormValueOutput(HTML_Element* he, FormValue*& out_value)
{
	FormValueOutput* output_value = OP_NEW(FormValueOutput, ());
	if (!output_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	out_value = output_value;
	return OpStatus::OK;
}

/* virtual */
BOOL FormValueOutput::Externalize(FormObject* form_object_to_seed)
{
	OP_ASSERT(FALSE); // We have no external counterpart. Don't even try!
	return FALSE;
}

/* virtual */
void FormValueOutput::Unexternalize(FormObject* form_object_to_replace)
{
	OP_ASSERT(FALSE); // We have no external counterpart. Don't even try!
}


OP_STATUS FormValueOutput::GetDefaultValueAsText(HTML_Element* he, OpString& out_value)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(!IsValueExternally());

	// As long as we have not found the end tag, the defaultValue is the empty string.
	if (he->GetEndTagFound())
	{
		if (GetHasDefaultValueBool())
		{
			return out_value.Set(m_default_value);
		}

		return GetValueAsText(he, out_value);
	}

	out_value.Empty();
	return OpStatus::OK;
}

OP_STATUS FormValueOutput::SetDefaultValueFromText(HTML_Element* he, const uni_char* default_value)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(!IsValueExternally());

	GetHasDefaultValueBool() = TRUE;
	return m_default_value.Set(default_value);
}


// FormValueNumber

/* virtual */
FormValue* FormValueNumber::Clone(HTML_Element *he)
{
	FormValueNumber* clone = OP_NEW(FormValueNumber, ());
	if (clone)
	{
		if (he && IsValueExternally())
		{
			if (OpStatus::IsSuccess(GetFormObjectValue(he, clone->m_value)))
			{
				// Rather lose the value, than kill the whole FormValue/HTML_Element
				if (!ValueFromText(he, clone->m_value.CStr(), clone->m_number))
				{
					clone->ClearValue();
				}
			}
		}
		else
		{
			clone->m_number = m_number;
			clone->m_value.Set(m_value);
		}
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

double FormValueNumber::SnapToStepMinMax(double number, double min_val, double max_val,
										 double step_base, double step_val)
{
	// Check with step, we might need to align it to make sure it's valid.
	// This code is a direct copy of OpSlider::SnapToStepMinMax so make sure
	// anything fixed here is also fixed there.
	if (step_val != 0)
	{
		double steps = (number - step_base) / step_val;
		// round it
		if (steps < 0)
		{
			steps -= 0.5;
		}
		else
		{
			steps += 0.5;
		}

		if (steps > INT_MAX || steps < INT_MIN) // this is an integer but too large for an int32
		{
			number = steps * step_val + step_base;
		}
		else
		{
			int whole_steps = static_cast<int>(steps);
			number = whole_steps * step_val + step_base;
		}
	}

	// Put inside [min_val, max_val]
	number = MAX(min_val, MIN(max_val, number));
	return number;
}

double FormValueNumber::CalculateRangeDefaultValue(HTML_Element* he)
{
	OP_ASSERT(he->GetInputType() == INPUT_RANGE);

	double min_val;
	double max_val;
	double step_base;
	double step_val;
	WebForms2Number::GetNumberRestrictions(he, min_val, max_val,
										   step_base, step_val);

	// Calculate the middle between min and max
	double ret_value = max_val > min_val ? (max_val+min_val)/2.0 : min_val;

	return SnapToStepMinMax(ret_value, min_val, max_val, step_base, step_val);
}

/* virtual */
OP_STATUS FormValueNumber::ResetToDefault(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	ResetChangedFromOriginal();

	return SetValueFromTextInternal(he, he->GetStringAttr(ATTR_VALUE));
}

/* virtual */
OP_STATUS FormValueNumber::GetValueAsText(HTML_Element* he, OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueNumber*>(this));

	if(IsValueExternally())
	{
		return GetFormObjectValue(he, out_value);
	}
	else
	{
		return out_value.Set(m_value);
	}
}

/* private */
BOOL FormValueNumber::ValueFromText(HTML_Element* he, const uni_char* in_value, double& val)
{
	if (!in_value || !*in_value)
		return FALSE;

	if (!WebForms2Number::StringToDouble(in_value, val))
		return FALSE;

	// Check for NaN and Infinity
	if (!op_isfinite(val))
		return FALSE;

	if (he->GetInputType() == INPUT_RANGE)
	{
		double min_val;
		double max_val;
		double step_base;
		double step_val;
		WebForms2Number::GetNumberRestrictions(he, min_val, max_val,
											   step_base, step_val);

		// Fix potential step mismatch
		if (step_val != 0.0)
			val = SnapToStepMinMax(val, min_val, max_val, step_base, step_val);
	}

	return TRUE;
}

OP_STATUS FormValueNumber::SetValueFromTextInternal(HTML_Element* he, const uni_char* in_value)
{
	double new_number;
	if (!ValueFromText(he, in_value, new_number))
	{
		// Default value for type=number is an empty string if we couldn't convert.
		if (he->GetInputType() == INPUT_NUMBER)
		{
			if (IsValueExternally())
			{
				FormObject* form_object = he->GetFormObject();
				OP_ASSERT(form_object); // It is external after all!
				return form_object->SetFormWidgetValue(NULL);
			}
			else
			{
				ClearValue();
				return OpStatus::OK;
			}
		}
		else
		{
			new_number = CalculateRangeDefaultValue(he);
		}
	}

	uni_char* value_buf = m_value.Reserve(MAX_NUMBER_BUF_SIZE);
	if (!value_buf || OpStatus::IsError(WebForms2Number::DoubleToString(new_number, value_buf)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // It is external after all!
		RETURN_IF_ERROR(form_object->SetFormWidgetValue(value_buf));
		ClearValue();
	}
	else
	{
		m_number = new_number;
		ResetInternalSelection();
	}

	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueNumber::SetValueFromText(HTML_Element* he, const uni_char* in_value)
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueNumber*>(this));

	return SetValueFromTextInternal(he, in_value);
}

/* static */
OP_STATUS FormValueNumber::ConstructFormValueNumber(HTML_Element* he, FormValue*& out_value)
{
	FormValueNumber* number_value = OP_NEW(FormValueNumber, ());
	if (!number_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	const uni_char* html_value = he->GetStringAttr(ATTR_VALUE);

	OP_STATUS status = number_value->SetValueFromTextInternal(he, html_value);
	if (OpStatus::IsMemoryError(status))
	{
		OP_DELETE(number_value);
		return status;
	}

	out_value = number_value;
	return OpStatus::OK;
}

/* virtual */
BOOL FormValueNumber::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}
	if (HasValue())
	{
		form_object_to_seed->SetFormWidgetValue(m_value.CStr());
		form_object_to_seed->SetSelection(m_selection_start, m_selection_end, m_selection_direction);
		form_object_to_seed->SetCaretOffset(m_caret_position);

		ClearValue();
	}
	else
	{
		form_object_to_seed->SetFormWidgetValue(NULL);
	}

	return TRUE;
}

/* virtual */
void FormValueNumber::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		ClearValue();
		ResetInternalSelection();

		if (OpStatus::IsSuccess(form_object_to_replace->GetFormWidgetValue(m_value)) &&
			!m_value.IsEmpty())
		{
			const uni_char* buf_p = m_value.CStr();
			double val;
			if (WebForms2Number::StringToDouble(buf_p, val) && op_isfinite(val))
			{
				m_number = val;

				form_object_to_replace->GetSelection(m_selection_start, m_selection_end, m_selection_direction);
				m_caret_position = form_object_to_replace->GetCaretOffset();
			}
			else
			{
				ClearValue();
			}
		}

		FormValue::Unexternalize(form_object_to_replace);
	}
}

/* virtual */
OP_STATUS FormValueNumber::StepUpDown(HTML_Element* he, int step_count)
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueNumber*>(this));

	double current_number;

	if (IsValueExternally())
	{
		OpString current_value;
		RETURN_IF_ERROR(GetFormObjectValue(he, current_value));

		if (!ValueFromText(he, current_value.CStr(), current_number))
			return OpStatus::ERR_NOT_SUPPORTED;
	}
	else
	{
		if (!HasValue())
			return OpStatus::ERR_NOT_SUPPORTED;
		current_number = m_number;
	}

	double min_value;
	double max_value;
	double step_base;
	double step_value;
	WebForms2Number::GetNumberRestrictions(he, min_value, max_value, step_base, step_value);
	if (step_value == 0)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Number inputs may have a value outside the steps.
	if (he->GetInputType() == INPUT_NUMBER)
		current_number = WebForms2Number::SnapToStep(current_number, step_base, step_value, step_count < 0 ? -1 : 1);

	double out_value_num;
	RETURN_IF_ERROR(WebForms2Number::StepNumber(current_number, min_value, max_value,
	                                            step_base, step_value, step_count, FALSE /* wrap_around */,
	                                            TRUE /* fuzzy_snapping */, out_value_num));


	ValidationResult val_res = VALIDATE_OK;
	val_res.SetErrorsFrom(FormValidator::ValidateNumberForMinMaxStep(he, out_value_num));
	if (!val_res.IsOk())
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	uni_char* value_buf = m_value.Reserve(MAX_NUMBER_BUF_SIZE);
	if (!value_buf || OpStatus::IsError(WebForms2Number::DoubleToString(out_value_num, value_buf)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // It is external after all!
		RETURN_IF_ERROR(form_object->SetFormWidgetValue(m_value.CStr()));
		ClearValue();
	}
	else
	{
		m_number = out_value_num;
		ResetInternalSelection();
	}

	return OpStatus::OK;
}

/* virtual */
BOOL FormValueNumber::IsUserEditableText(HTML_Element* he)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // If they said we was external, then we must have a form_object
		return form_object->IsUserEditableText();
	}
	else
	{
		return FALSE;
	}
}

/* private */
void FormValueNumber::ResetInternalSelection()
{
	m_selection_start = m_selection_end = 0;
	m_selection_direction = SELECTION_DIRECTION_DEFAULT;
	m_caret_position = m_value.Length();
}

/* virtual */
void FormValueNumber::GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueNumber*>(this));

	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->GetSelection(start_ofs, stop_ofs, direction);
	}
	else
	{
		start_ofs = m_selection_start;
		stop_ofs = m_selection_end;
		direction = m_selection_direction;
	}
}

/* virtual */
void FormValueNumber::SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetSelection(start_ofs, stop_ofs, direction);
	}
	else
	{
		int max_ofs = m_value.Length();
		if (start_ofs < 0)
		{
			start_ofs = 0;
		}
		else if (start_ofs > max_ofs)
		{
			start_ofs = max_ofs;
		}

		if (stop_ofs < start_ofs)
		{
			stop_ofs = start_ofs;
		}

		if (stop_ofs > max_ofs)
		{
			stop_ofs = max_ofs;
		}

		if (stop_ofs == start_ofs)
		{
			start_ofs = 0;
			stop_ofs = 0;
			direction = SELECTION_DIRECTION_DEFAULT;
		}

		m_selection_start = start_ofs;
		m_selection_end = stop_ofs;
		m_selection_direction = direction;
	}
}

/* virtual */
void FormValueNumber::SelectAllText(HTML_Element *he)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SelectText();
	}
	else
	{
		m_selection_start = 0;
		m_selection_end = m_value.Length();
		m_selection_direction = SELECTION_DIRECTION_DEFAULT;
	}
}

/* virtual */
void FormValueNumber::ClearSelection(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueNumber*>(this));
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		return form_object->ClearSelection();
	}

	INT32 selection_collapse_point = GetCaretOffset(he);
	FormValue::SetSelection(he, selection_collapse_point, selection_collapse_point);
}

/* virtual */
INT32 FormValueNumber::GetCaretOffset(HTML_Element* he) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueNumber*>(this));
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		return form_object->GetCaretOffset();
	}

	return m_caret_position;
}

/* virtual */
void FormValueNumber::SetCaretOffset(HTML_Element* he, INT32 caret_ofs)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetCaretOffset(caret_ofs);
	}
	else
	{
		int max_ofs = m_value.Length();
		if (caret_ofs < 0)
		{
			caret_ofs = 0;
		}
		else if (caret_ofs > max_ofs)
		{
			caret_ofs = max_ofs;
		}

		m_caret_position = caret_ofs;
	}
}

/* virtual */
FormValue* FormValueNoOwnValue::Clone(HTML_Element *he)
{
	FormValueNoOwnValue* clone = OP_NEW(FormValueNoOwnValue, ());
	if (clone)
	{
		OpStatus::Ignore(clone->m_default_value.Set(m_default_value));
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueNoOwnValue::GetValueAsText(HTML_Element* he, OpString& out_value) const
{
	if (he->IsMatchingType(HE_INPUT, NS_HTML) && he->GetInputType() == INPUT_HIDDEN)
	{
		RETURN_IF_MEMORY_ERROR(out_value.Set(he->GetValue()));
		return FormValue::FixupCRLF(out_value);
	}
	else
	{
		return out_value.Set(he->GetValue());
	}
}

/* virtual */
OP_STATUS FormValueNoOwnValue::SetValueFromText(HTML_Element* he, const uni_char* in_value)
{
	if (in_value)
	{
		// XXX Used to set default texts for buttons in InputFieldContent::ShowForm
		// XXX That can probably be done in a better way. Default value for the attribute?
		uni_char* new_str = UniSetNewStr_NotEmpty(in_value);
		if (!new_str)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (he->SetAttr(ATTR_VALUE, ITEM_TYPE_STRING, new_str, TRUE) == -1)
		{
			OP_DELETEA(new_str);
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			if (IsValueExternally())
			{
				he->GetFormObject()->SetFormWidgetValue(new_str);
			}
		}
	}
	else
	{
		he->RemoveAttribute(ATTR_VALUE);
	}

	return OpStatus::OK;
}

void FormValueNoOwnValue::SetValueAttributeFromText(HTML_Element* he, const uni_char* in_value)
{
	if (IsValueExternally())
	{
		he->GetFormObject()->SetFormWidgetValue(in_value ? in_value : UNI_L(""));
	}
}

/* virtual */
BOOL FormValueNoOwnValue::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}
	HTML_Element* he = form_object_to_seed->GetHTML_Element();
	const uni_char *value = he->GetValue();
	if (!value)
	{
		value = m_default_value.CStr();
	}
	form_object_to_seed->SetFormWidgetValue(value);
	return FALSE;
}

OP_STATUS FormValueNoOwnValue::GetEffectiveValueAsText(HTML_Element *he, OpString &out_value)
{
	RETURN_IF_ERROR(GetValueAsText(he, out_value));
	if (!out_value.CStr())
	{
		if (!m_default_value.CStr())
		{
			InputType input_type = he->GetInputType();
			Str::LocaleString language_string_id(Str::NOT_A_STRING);

			switch (input_type)
			{
			case INPUT_SUBMIT:
				language_string_id = Str::SI_IDFORM_SUBMIT;
				break;

			case INPUT_RESET:
				language_string_id = Str::SI_IDFORM_RESET;
				break;

			default:
				return OpStatus::OK;
			}
			RETURN_IF_ERROR(g_languageManager->GetString(language_string_id, m_default_value));
		}
		RETURN_IF_ERROR(out_value.Set(m_default_value.CStr()));
	}
	return OpStatus::OK;
}
