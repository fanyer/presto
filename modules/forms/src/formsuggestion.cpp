/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/forms/formsuggestion.h"
#include "modules/forms/webforms2support.h"
#include "modules/forms/form_his.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"

#include "modules/pi/OpLocale.h"

#include "modules/doc/frm_doc.h"

#include "modules/util/tempbuf.h"

#include "modules/history/OpHistoryModel.h"

#ifdef QUICK
#  include "adjunct/quick/hotlist/HotlistManager.h"
#endif // QUICK

OP_STATUS FormSuggestion::GetItems(const uni_char* in_MatchString,
								   uni_char*** out_Items,
								   int* out_ItemCount,
								   int* out_num_columns,
								   BOOL* out_contains_private_data /* = NULL */)
{
	BOOL dummy;
	if (!out_contains_private_data)
	{
		out_contains_private_data = &dummy;
	}
	*out_contains_private_data = TRUE; // Play if safe

	if (m_suggestion_type == AUTOMATIC && m_elm->HasAttr(Markup::HA_LIST))
	{
		*out_contains_private_data = FALSE;
		if (!m_elm->GetBoolAttr(Markup::HA_READONLY))
		{
			RETURN_IF_ERROR(GetDataListItems(in_MatchString, out_Items, out_ItemCount, out_num_columns));
		}
	}
	// FIX! WHen the api is better defined
#if 0 // Could be used to fool the user into giving away history information. See CORE-17590
# if defined HISTORY_SUPPORT
	else if (m_suggestion_type == AUTOMATIC && m_elm->GetInputType() == INPUT_URI && globalHistory)
	{
		RETURN_IF_ERROR(globalHistory->GetItems(in_MatchString, out_Items, out_ItemCount));
		*out_num_columns = 3;
	}
# endif // HISTORY_SUPPORT
#endif // 0
#if 0 // This returned contact names rather than email addresses. Not very useful.
# ifdef QUICK
	else if (m_suggestion_type == AUTOMATIC && m_elm->GetInputType() == INPUT_EMAIL && g_hotlist_manager)
	{
		g_hotlist_manager->GetContactNameList(in_MatchString, out_Items, out_ItemCount);
		*out_num_columns = 2;
	}
# endif // QUICK
#endif // 0
	else if (m_suggestion_type == AUTOMATIC || m_suggestion_type == PERSONAL_INFO)
	{
		*out_contains_private_data = FALSE; // Not entirely true, but
											// we want to keep the
											// personal info behaviour
											// the same as earlier
		RETURN_IF_ERROR(g_forms_history->GetItems(in_MatchString, out_Items, out_ItemCount));
		*out_num_columns = 1;
	}
	else
	{
		out_Items = NULL;
		out_ItemCount = 0;
		*out_num_columns = 1; // dummy
	}

	return OpStatus::OK;
}

static OP_STATUS CompareStringsLocaleSensitiveIgnoreCase(int& comparison_result, const uni_char *str1, const uni_char *str2, long len)
{
	TRAPD(rc, comparison_result = g_oplocale->CompareStringsL(str1, str2, len, TRUE));
	return rc;
}

OP_STATUS FormSuggestion::GetDataListItems(const uni_char* in_MatchString,
								   uni_char*** out_Items,
								   int* out_ItemCount,
								   int* out_num_columns)
{
	OpVector<uni_char> suggestions;
	OpVector<uni_char> suggestion_labels;
	BOOL has_label = FALSE;

	OP_STATUS status = OpStatus::OK;

	// Find datalist element
	const uni_char* id = m_elm->GetStringAttr(ATTR_LIST);
	HTML_Element *result = NULL;
	if (id)
	{
		LogicalDocument* logdoc = m_frames_doc->GetLogicalDocument();
		if (logdoc)
		{
			HTML_Element* docroot = NULL;
			HTML_Element* parent = m_elm;
			do
			{
				docroot = parent;
				parent = parent->Parent();
			}
			while (parent);

			NamedElementsIterator it;
			logdoc->SearchNamedElements(it, docroot, id, TRUE, FALSE);
			result = it.GetNextElement();
		}

		if (result &&
			(result->IsMatchingType(HE_DATALIST, NS_HTML) ||
			result->IsMatchingType(HE_SELECT, NS_HTML)))
		{
			HTML_Element* stop_marker = static_cast<HTML_Element*>(result->NextSibling());
			// Find options
			for (HTML_Element* option = static_cast<HTML_Element*>(result->Next());
				option != stop_marker;
				option = static_cast<HTML_Element*>(option->Next()))
			{
				if (!option->IsMatchingType(HE_OPTION, NS_HTML) ||
					option->GetDisabled())
				{
					continue;
				}

				// Wow, it _is_ a option and it's not disabled
				const uni_char* value = option->GetStringAttr(ATTR_VALUE);

				// If the attribute is missing, the text contents should be used
				TempBuffer option_text;
				if (!value)
				{
					status = option->GetOptionText(&option_text);
					if (OpStatus::IsError(status))
					{
						break;
					}

					int len = option_text.Length();
					if (len > 0)
					{
						value = option_text.GetStorage();
					}
				}

				if (!value || !*value)
				{
					// Empty strings shouldn't be showed
					continue;
				}

				FormValidator validator(m_frames_doc);
				ValidationResult validation_result =
					validator.ValidateSingleControlAgainstValue(m_elm, value);
				if (!validation_result.IsOk())
				{
					continue; // next option please
				}

				const uni_char* label_attr = option->GetStringAttr(ATTR_LABEL);

				// If a match string is given we should see if it match
				if (in_MatchString)
				{
					// Only show options with a value or label that match the matchstring.
					if (int len = uni_strlen(in_MatchString))
					{
						// Compare value
						int comp_result;
						OP_STATUS rc = CompareStringsLocaleSensitiveIgnoreCase(comp_result, value, in_MatchString, len);
						RETURN_IF_MEMORY_ERROR(rc);
						if (OpStatus::IsSuccess(rc) && comp_result)
						{
							// Not a match. Now compare label.
							if (label_attr)
							{
								rc = CompareStringsLocaleSensitiveIgnoreCase(comp_result, label_attr, in_MatchString, len);
								RETURN_IF_MEMORY_ERROR(rc);
								if (OpStatus::IsSuccess(rc) && comp_result)
								{
									// No match, skip this option.
									continue;
								}
							}
							else
							{
								continue;
							}
						}
					}
				}

				// Must make a copy since all the memory will be owned by the caller
				value = UniSetNewStr(value);
				if (!value)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}

				OP_ASSERT(value);

				suggestions.Add(const_cast<uni_char*>(value));

				// Add label if it exists
				uni_char* label;
				if (label_attr && *label_attr)
				{
					has_label = TRUE;
					label = UniSetNewStr(label_attr);
				}
				else
				{
					label = OP_NEWA(uni_char, 1);
					if (label)
					{
						*label = '\0';
					}
				}
				if (!label)
				{
					status = OpStatus::ERR_NO_MEMORY;
					break;
				}
				suggestion_labels.Add(label);
			} // end for
		}

		if (OpStatus::IsSuccess(status))
		{
			int count = suggestions.GetCount();
			OP_ASSERT((int)suggestion_labels.GetCount() == count);
			uni_char**& items = *out_Items;
			int array_length = count;
			if (has_label)
			{
				array_length *= 2; // Room for labels too
			}

			items = OP_NEWA(uni_char*, array_length);
			if (!items)
			{
				status = OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				if (has_label)
				{
					for (int i = 0; i < count; i++)
					{
						items[2*i] = suggestions.Get(i);
						items[2*i+1] = suggestion_labels.Get(i);
					}
					*out_num_columns = 2;
				}
				else // only dummy labels. We delete them and forget about them
				{
					for (int i = 0; i < count; i++)
					{
						items[i] = suggestions.Get(i);
						OP_DELETEA(suggestion_labels.Get(i));
					}
					*out_num_columns = 1;
				}

				*out_ItemCount = suggestions.GetCount();
			}
		}
	}

	// If something went wrong, we have to return the
	// memory we've put in the vectors before returning.
	// Otherwise that memory will be owned by the caller.
	if (OpStatus::IsError(status))
	{
		int count = suggestions.GetCount();
		int i;
		for (i = 0; i < count; i++)
		{
			uni_char* text = suggestions.Get(i);
			OP_DELETEA(text);
		}

		count = suggestion_labels.GetCount();
		for (i = 0; i < count; i++)
		{
			uni_char* text = suggestion_labels.Get(i);
			OP_DELETEA(text);
		}
	}

	return status;
}
