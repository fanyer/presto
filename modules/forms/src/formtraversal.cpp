/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formtraversal.h"

#include "modules/forms/form.h"
#include "modules/forms/formsenum.h"
#include "modules/forms/src/formiterator.h"
#include "modules/forms/formmanager.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/doc/frm_doc.h"
#include "modules/util/str.h"
#include "modules/formats/argsplit.h"
#include "modules/upload/upload.h"

#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalueradiocheck.h"

#ifdef FORMS_KEYGEN_SUPPORT
# include "modules/forms/formvaluekeygen.h"
#endif // FORMS_KEYGEN_SUPPORT

/* static */
void FormTraversal::TraverseElementL(FormTraversalContext &context, FormTraverser *traverser, HTML_Element *he)
{
	OP_ASSERT(he);
	const char* default_name = "value";

	// To speed things up we set the form_element_check parameter to FALSE since
	// we know that the form control is associated with a form element.
	if (!FormManager::IsIncludedInSubmit(context.frames_doc, he, context.submit_elm, FALSE))
	{
		return;
	}

	BOOL verbatim_value = FALSE;

	const uni_char *force_value = context.force_value;
	if (he == context.search_field_elm)
	{
		force_value = UNI_L("%s");
		verbatim_value = TRUE;
	}

	BOOL is_default_name = TRUE;
	OpString8 name;
	ANCHOR(OpString8, name);
	const uni_char *tmp_name = context.force_name ? context.force_name : he->GetName();
	if (tmp_name)
	{
		OpString tmp_str;
		tmp_str.SetL(tmp_name);
		LEAVE_IF_ERROR(FormValue::FixupCRLF(tmp_str));
		traverser->ToFieldValueL(name, tmp_str.CStr());
		is_default_name = FALSE;
	}
	else
	{
		name.SetL(default_name);
	}

	OpString text_value;
	ANCHOR(OpString, text_value);
#ifdef _WML_SUPPORT_
	if (he->IsMatchingType(WE_GO, NS_WML) && force_value)
	{
		// These have no FormValue.
		text_value.SetL(force_value);
	}
	else
#endif // _WML_SUPPORT_
#ifdef _PLUGIN_SUPPORT_
	if (he->Type() == HE_OBJECT)
	{
		if (OpStatus::IsError(FormManager::GetPluginFormValue(he, text_value)))
		{
			// Not a forms plugin.
			return;
		}
	}
	else
#endif // _PLUGIN_SUPPORT_
	{
		FormValue* form_value = he->GetFormValue();
		OP_ASSERT(form_value);

		if (form_value->GetType() == FormValue::VALUE_RADIOCHECK)
		{
			FormValueRadioCheck* bool_val = FormValueRadioCheck::GetAs(form_value);
			if(!bool_val->IsChecked(he))
			{
				// Skip not-set radio buttons and checkboxes.
				return;
			}
		}

		if (force_value)
		{
			text_value.SetL(force_value);
		}
		else if (form_value->HasMeaningfulTextRepresentation())
		{
			if (form_value->GetType() == FormValue::VALUE_NO_OWN_VALUE)
			{
				FormValueNoOwnValue *form_value_no_own_value = FormValueNoOwnValue::GetAs(form_value);
				LEAVE_IF_ERROR(form_value_no_own_value->GetEffectiveValueAsText(he, text_value));
			}
			else
				LEAVE_IF_ERROR(form_value->GetValueAsText(he, text_value));
		}
#ifdef _DEBUG
		else
		{
			// We take care of these further down.
#ifdef FORMS_KEYGEN_SUPPORT
			OP_ASSERT(form_value->GetType() == FormValue::VALUE_LIST_SELECTION || form_value->GetType() == FormValue::VALUE_KEYGEN);
#else
			OP_ASSERT(form_value->GetType() == FormValue::VALUE_LIST_SELECTION);
#endif // FORMS_KEYGEN_SUPPORT
		}
#endif // _DEBUG
	}

	OpString8 value;
	ANCHOR(OpString8, value);
	traverser->ToFieldValueL(value, text_value.CStr());

	// We keep this as a debug check for FormValue.
#ifdef _DEBUG
	if (he->Type() == HE_TEXTAREA)
	{
		// Newlines must be submitted as CRLF, not just LF.
		char *buf = value.CStr();
		int length = value.Length();
		// Count LF:s without CR.
		int lf_count = 0;
		int i;
		for (i = length - 1; i >= 0; i--)
		{
			if (buf[i] == '\n' && (i == 0 || buf[i - 1] != '\r'))
			{
				lf_count++;
			}
		}
		OP_ASSERT(lf_count == 0);
	}
#endif // _DEBUG

#ifdef _WML_SUPPORT_
	// If the submitted data doesn't validate then we skip it.
	if (context.frames_doc->GetHLDocProfile() && context.frames_doc->GetHLDocProfile()->IsWml())
	{
		if (!FormManager::ValidateWmlInputData(he, text_value.CStr()))
		{
			return;
		}
	}
#endif // _WML_SUPPORT_

	if (he->Type() == HE_INPUT && he->GetInputType() == INPUT_IMAGE)
	{
		// Add x=323&y=121 to the submit.
		char *coord_name = static_cast<char *>(g_memory_manager->GetTempBuf());
		unsigned int coord_name_len = g_memory_manager->GetTempBufLen();

		if (coord_name_len > (unsigned)name.Length() + 2)
		{
#if UINT_MAX > 0xffffffffu
# error "Code assumes ints are at most 4 bytes"
#endif
			// Room for an int.
			char xy_str[12]; // ARRAY OK 2011-10-26 sof
			const char* name1 = (is_default_name || !name.CStr()) ? "" : name.CStr();
			const char* sep1 = *name1 ? "." : "";

			op_sprintf(coord_name, "%s%sx", name1, sep1);
			op_itoa(context.xpos, xy_str, 10); // radix 10
			traverser->AppendNameValuePairL(context, he, coord_name, xy_str);

			op_sprintf(coord_name, "%s%sy", name1, sep1);
			op_itoa(context.ypos, xy_str, 10); // radix 10
			traverser->AppendNameValuePairL(context, he, coord_name, xy_str);
		}
	}
#ifdef _WML_SUPPORT_
	else if (!is_default_name && he->IsMatchingType(WE_GO, NS_WML))
	{
		traverser->AppendNameValuePairL(context, he, name.CStr(), value.CStr());
	}
#endif // #ifdef _WML_SUPPORT_
#ifdef _PLUGIN_SUPPORT_
	// Make this a FormValue as well?
	else if (!is_default_name && he->IsMatchingType(HE_OBJECT, NS_HTML))
	{
		traverser->AppendNameValuePairL(context, he, name.CStr(), value.CStr());
	}
#endif // _PLUGIN_SUPPORT_
	// Only submit things with specified name.
	else if (!is_default_name)
	{
		FormValue* form_value = he->GetFormValue();
		if (form_value->GetType() == FormValue::VALUE_LIST_SELECTION)
		{
			OP_ASSERT(he->Type() == HE_SELECT);
			FormValueList* form_value_list = FormValueList::GetAs(form_value);
			OpAutoVector<OpString> selected_values;
			ANCHOR(OpAutoVector<OpString>, selected_values);
			LEAVE_IF_ERROR(form_value_list->GetSelectedValues(he, selected_values));

			int value_count = selected_values.GetCount();
			for (int value_index = 0; value_index < value_count; value_index++)
			{
				OpString* selected_value = selected_values.Get(value_index);
				const uni_char* opt_val = selected_value->CStr();
				if (!opt_val)
				{
					opt_val = UNI_L("");
				}

				OpString8 opt_val1;
				ANCHOR(OpString8, opt_val1);
				traverser->ToFieldValueL(opt_val1, opt_val);
				traverser->AppendNameValuePairL(context, he, name.CStr(), opt_val1.CStr());
			}
		}
#ifdef FORMS_KEYGEN_SUPPORT
		else if (form_value->GetType() == FormValue::VALUE_KEYGEN)
		{
			FormValueKeyGen* keygen_value;
			keygen_value = FormValueKeyGen::GetAs(form_value);
			unsigned int keygen_size;
			keygen_size = keygen_value->GetSelectedKeySize(he);
			if (keygen_size > 0)
			{
				const char* key = keygen_value->GetGeneratedKey();
				if (key)
				{
					traverser->AppendNameValuePairL(context, he, name.CStr(), key);
					 // Clear the stored key.
					keygen_value->SetGeneratedKey(NULL);
				}
				else
				{
					// We could for some reason not create a key, and in 99.12% of the forms
					// using <keygen> nothing else is submitted so it's basically
					// meningless to go on.
					LEAVE(OpStatus::ERR);
				}
			}
		}
#endif // FORMS_KEYGEN_SUPPORT
#ifdef _FILE_UPLOAD_SUPPORT_
		else if (he->GetInputType() == INPUT_FILE)
		{
			// The file names are in value1, need to split it.
			UniParameterList file_name_list;
			FormManager::ConfigureForFileSplit(file_name_list, text_value.CStr());
			// All files in a multiple selected file will
			// have the same index.
			UniParameters* file_name_obj = file_name_list.First();

			if (!file_name_obj)
			{
				// No filename set in the input field.
				traverser->AppendUploadFilenameL(context, he, name.CStr(), NULL, NULL);
			}
			for (;file_name_obj; file_name_obj = file_name_obj->Suc())
			{
				const uni_char* file_name = file_name_obj->Name();
				if (!file_name || !*file_name)
				{
					continue;
				}
				// First strip the path from the file name.
				const uni_char *pure_file_name = uni_strrchr(file_name, PATHSEPCHAR);
				if (!pure_file_name)
				{
					pure_file_name = text_value.CStr();
				}
				else
				{
					// Skip the path separator.
					pure_file_name++;
				}

				OpString8 encoded_pure_file_name;
				ANCHOR(OpString8, encoded_pure_file_name);
				traverser->ToFieldValueL(encoded_pure_file_name, pure_file_name);
				traverser->AppendUploadFilenameL(context, he, name.CStr(), file_name, encoded_pure_file_name.CStr());
			}
		}
#endif // _FILE_UPLOAD_SUPPORT_
		else
		{
			traverser->AppendNameValuePairL(context, he, name.CStr(), value.CStr(), verbatim_value);
		}

		const uni_char* dirname = he->GetStringAttr(ATTR_DIRNAME);
		if (dirname && *dirname)
		{
			BOOL right_type_for_dirname = he->IsMatchingType(HE_TEXTAREA, NS_HTML);
			if (!right_type_for_dirname && he->IsMatchingType(HE_INPUT, NS_HTML))
			{
				InputType inp_type = he->GetInputType();
				if (inp_type == INPUT_TEXT || inp_type == INPUT_SEARCH)
				{
					right_type_for_dirname = TRUE;
				}
			}
			if (right_type_for_dirname)
			{
				OpString8 dirname8;
				traverser->ToFieldValueL(dirname8, dirname);
				traverser->AppendNameValuePairL(context, he, dirname8, he->GetFormValue()->IsUserInputRTL(he) ? "rtl" : "ltr");
			}
		}
	}
	return;
}


/* static */
void FormTraversal::TraverseL(FormTraversalContext &context, FormTraverser *traverser, HTML_Element *he, BOOL *have_password)
{
	// Iterate over everything in the form, adding to the submit data when
	// something that should be added to the submit data is found.

	HTML_Element* image_submit_elm = NULL;
	FormIterator form_iterator(context.frames_doc, he);
#ifdef _PLUGIN_SUPPORT_
	form_iterator.SetIncludeObjectElements();
#endif // _PLUGIN_SUPPORT_
	HTML_Element* fhe = form_iterator.GetNext();

	while (fhe)
	{
		if (fhe->Type() == HE_INPUT && fhe->GetInputType() == INPUT_IMAGE)
		{
			if (fhe == context.submit_elm)
			{
				image_submit_elm = fhe;
			}
		}
		else
		{
			OP_ASSERT(context.force_name == NULL);
			OP_ASSERT(context.force_value == NULL);
			TraverseElementL(context, traverser, fhe);
			if (have_password && fhe->Type() == HE_INPUT && fhe->GetInputType() == INPUT_PASSWORD)
			{
				*have_password = TRUE;
			}
		}

		fhe = form_iterator.GetNext();
	}
	// We add the image last (why?)
	if (image_submit_elm)
	{
		TraverseElementL(context, traverser, image_submit_elm);
	}
}
