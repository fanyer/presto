/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvaluetext.h"

#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"

void FormValueText::CopyToClone(HTML_Element* he, FormValueText* clone)
{
	// If we get OOM when setting the string then we just lose the
	// contents of the text field. The alternative is to drop the
	// whole FormValue which will kill the whole HTML_Element and
	// kill any scripts that triggered the cloning. It's not
	// unreasonable, but this is less code.
	if (he && IsValueExternally())
	{
		OpStatus::Ignore(GetValueAsText(he, clone->m_text));
		FormObject *form_object = he->GetFormObject();
		form_object->GetSelection(clone->m_selection_start, clone->m_selection_end);
		clone->m_selection_highlight_type = form_object->IsSearchHitSelected()
			? (form_object->IsActiveSearchHitSelected() ? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT : VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
			: VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
		clone->m_caret_position = form_object->GetCaretOffset();
	}
	else
	{
		clone->m_selection_start = m_selection_start;
		clone->m_selection_end = m_selection_end;
		clone->m_selection_highlight_type = m_selection_highlight_type;
		clone->m_caret_position = m_caret_position;
		OpStatus::Ignore(clone->m_text.Set(m_text));
	}
	OP_ASSERT(!clone->IsValueExternally());
}

/* virtual */
FormValue* FormValueText::Clone(HTML_Element *he)
{
	FormValueText* clone = OP_NEW(FormValueText, ());
	if (clone)
	{
		CopyToClone(he, clone);
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueText::ResetToDefault(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	ResetChangedFromOriginal();

	const uni_char* default_value;
	if (he->GetInputType() == INPUT_FILE)
	{
		// *NEVER* read the value attribute for a file input
		default_value = NULL;
	}
	else
	{
		default_value = he->GetStringAttr(ATTR_VALUE);
	}

	if (IsValueExternally())
	{
		return SetValueFromText(he, default_value);
	}

	RETURN_IF_ERROR(m_text.Set(default_value));
	FilterChars(m_text);
	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueText::GetValueAsText(HTML_Element* he, OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueText*>(this));
	if(IsValueExternally())
	{
		OP_ASSERT(m_text.IsEmpty());
		RETURN_IF_ERROR(GetFormObjectValue(he, out_value));
		FilterChars(out_value);
		RETURN_IF_ERROR(ConvertDisplayValueToRealValue(out_value));
		return OpStatus::OK;
	}

	return out_value.Set(m_text);
}

/* virtual */
OP_STATUS FormValueText::SetValueFromText(HTML_Element* he, const uni_char* in_value)
{
	return SetValueFromText(he, in_value, FALSE);
}

/* virtual */
OP_STATUS FormValueText::SetValueFromText(HTML_Element* he, const uni_char* in_value, BOOL use_undo_stack)
{
	OP_ASSERT(he->GetFormValue() == this);
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // If they said we was external, then we must have a form_object
		OpString display_value;
		RETURN_IF_ERROR(ConvertRealValueToDisplayValue(in_value, display_value));
		return form_object->SetFormWidgetValue(display_value.CStr() ? display_value.CStr() : in_value, use_undo_stack);
	}
	RETURN_IF_ERROR(m_text.Set(in_value));
	FilterChars(m_text);

	return OpStatus::OK;
}

/*static*/
OP_STATUS FormValueText::ConstructFormValueText(HTML_Element* he,
												HLDocProfile* hld_profile,
												FormValue*& out_value)
{
	FormValueText* text_value = OP_NEW(FormValueText, ());
	if (!text_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// *NEVER* read the value attribute for a file input, except for
	// opera:config
	BOOL allow_value = TRUE;
	if (he->GetInputType() == INPUT_FILE)
	{
		allow_value = FALSE;

#ifdef OPERACONFIG_URL
		if (hld_profile)
		{
			URL *url = hld_profile->GetURL();
			if (url)
			{
				allow_value = url->Type() == URL_OPERA && url->GetAttribute(URL::KName).CompareI("opera:config") == 0;
			}
		}
#endif // OPERACONFIG_URL
	}

	if (allow_value)
	{
		const uni_char* initial_value = NULL;

#ifdef _WML_SUPPORT_
		if (hld_profile && hld_profile->HasWmlContent())
			initial_value = FormManager::GetWmlDefaultValue(hld_profile->GetFramesDocument(), he);
		else
#endif // _WML_SUPPORT_
			initial_value = he->GetStringAttr(ATTR_VALUE);

		if (initial_value)
		{
			OP_STATUS status = text_value->m_text.Set(initial_value);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(text_value);
				return status;
			}
			FilterChars(text_value->m_text);
		}
	}
	out_value = text_value;
	return OpStatus::OK;
}


/* virtual */
BOOL FormValueText::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}
	OpString value;
	OpStatus::Ignore(ConvertRealValueToDisplayValue(m_text.CStr(), value));
	// NULL is ok for InputObject::SetFormWidgetValue
	form_object_to_seed->SetFormWidgetValue(value.CStr() ? value.CStr() : m_text.CStr());
	if (m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_SELECTION)
		form_object_to_seed->SetSelection(m_selection_start, m_selection_end, m_selection_direction);
	else
		form_object_to_seed->SelectSearchHit(m_selection_start, m_selection_end, m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
	form_object_to_seed->SetCaretOffset(m_caret_position);
	m_text.Empty(); // Save some memory
	return TRUE;
}

/* virtual */
void FormValueText::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		// FIXME: Reuse buffers better, this doesn't work
		if (!m_text.IsEmpty())
		{
			m_text.CStr()[0] = '\0';
		}
		if (OpStatus::IsSuccess(form_object_to_replace->GetFormWidgetValue(m_text)) &&
			!m_text.IsEmpty())
		{
			FilterChars(m_text);
		}
		OpStatus::Ignore(ConvertDisplayValueToRealValue(m_text));

		form_object_to_replace->GetSelection(m_selection_start, m_selection_end);
		m_selection_highlight_type = form_object_to_replace->IsSearchHitSelected()
			? (form_object_to_replace->IsActiveSearchHitSelected() ? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT : VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
			: VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
		m_caret_position = form_object_to_replace->GetCaretOffset();
		FormValue::Unexternalize(form_object_to_replace);
	}
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
/* virtual */
int FormValueText::CreateSpellSessionId(HTML_Element* he, OpPoint *point /* = NULL */)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // If they said we was external, then we must have a form_object
		return form_object->CreateSpellSessionId(point);
	}
	else
	{
		return 0;
	}
}
#endif // INTERNAL_SPELLCHECK_SUPPORT

/* virtual */
BOOL FormValueText::IsUserEditableText(HTML_Element* he)
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

/* virtual */
void FormValueText::GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueText*>(this));

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
void FormValueText::SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetSelection(start_ofs, stop_ofs, direction);
	}
	else
	{
		int max_ofs = m_text.Length();
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
void FormValueText::SelectAllText(HTML_Element *he)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SelectText();
	}
	else
	{
		m_selection_start = 0;
		m_selection_end = m_text.Length();
		m_selection_direction = SELECTION_DIRECTION_DEFAULT;
	}
}

void FormValueText::SelectSearchHit(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit)
{
	if (IsValueExternally())
		he->GetFormObject()->SelectSearchHit(start_ofs, stop_ofs, is_active_hit);
	else
	{
		SetSelection(he, start_ofs, stop_ofs, SELECTION_DIRECTION_DEFAULT);
		m_selection_highlight_type = is_active_hit
			? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT : VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT;
	}
}

/* virtual */
void FormValueText::ClearSelection(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueText*>(this));
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		return form_object->ClearSelection();
	}

	INT32 selection_collapse_point = GetCaretOffset(he);
	FormValue::SetSelection(he, selection_collapse_point, selection_collapse_point);
}

/* virtual */
INT32 FormValueText::GetCaretOffset(HTML_Element* he) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueText*>(this));
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		return form_object->GetCaretOffset();
	}

	return m_caret_position;
}

/* virtual */
void FormValueText::SetCaretOffset(HTML_Element* he, INT32 caret_ofs)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetCaretOffset(caret_ofs);
	}
	else
	{
		int max_ofs = m_text.Length();
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

/* static */
void FormValueText::FilterChars(OpString& text)
{
	int length = text.Length();
	if (length == 0)
	{
		return;
	}
	// mode = 0: Count filtered chars
	// mode = 1: filter
	int filtered_count = 0;
	for (int mode = 0; mode < 2; mode++)
	{
		uni_char *buf = text.CStr();
		const uni_char* stop_in = buf+length+1; // Copy null char
		// We are going to remove chars so the old buffer will keep working
		uni_char* out = buf;
		while (buf != stop_in)
		{
			switch(*buf)
			{
			case '\n':
			case '\r':
				if (mode == 0)
				{
					filtered_count++;
				}
				break;
			default:
				if (mode == 1)
				{
					*out++ = *buf;
				}
			}
			buf++;
		}

		if (filtered_count == 0)
		{
			return;
		}
	}
}

uni_char* FormValueText::GetCopyOfInternalText()
{
	OP_ASSERT(!IsValueExternally());
	int len = m_text.Length();
	uni_char* res = OP_NEWA(uni_char, len+1);
	if (res)
	{
		if (len)
		{
			op_memcpy(res, m_text.CStr(), len*sizeof(uni_char));
		}
		res[len] = '\0';
	}
	return res;
}

