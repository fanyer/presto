/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvaluetextarea.h"

#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/src/textdata.h"

// FormValueTextArea

/* virtual */
FormValue* FormValueTextArea::Clone(HTML_Element *he)
{
	FormValueTextArea* clone = OP_NEW(FormValueTextArea, ());
	if (clone)
	{
		// If we get OOM when setting the string then we just lose the
		// contents of the text field. The alternative is to drop the
		// whole FormValue which will kill the whole HTML_Element and
		// kill any scripts that triggered the cloning. It's not
		// unreasonable, but this is less code.
		if (he && IsValueExternally())
		{
			OpStatus::Ignore(GetFormObjectValue(he, clone->m_own_value));
			if (OpStatus::IsError(FormValue::FixupCRLF(clone->m_own_value)))
			{
				// Rather no value than a corrupt value
				clone->m_own_value.Empty();
			}
			else
			{
				clone->GetHasOwnValueBool() = TRUE;
				FormObject* form_object = he->GetFormObject();
				TextAreaObject *textarea_obj = static_cast<TextAreaObject *>(form_object);

				form_object->GetSelection(clone->m_selection_start, clone->m_selection_end);
				clone->m_selection_highlight_type = textarea_obj->IsSearchHitSelected()
					? (textarea_obj->IsActiveSearchHitSelected() ? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT : VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
					: VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
				clone->m_caret_position = textarea_obj->GetCaretOffset();
				textarea_obj->GetWidgetScrollPosition(clone->m_listbox_scroll_pos_x, clone->m_listbox_scroll_pos_y);
			}
		}
		else
		{
			clone->GetHasOwnValueBool() = GetHasOwnValueBool();
			OpStatus::Ignore(clone->m_own_value.Set(m_own_value));
			clone->m_listbox_scroll_pos_x = m_listbox_scroll_pos_x;
			clone->m_listbox_scroll_pos_y = m_listbox_scroll_pos_y;
			clone->m_caret_position = m_caret_position;
			clone->m_selection_start = m_selection_start;
			clone->m_selection_end = m_selection_end;
			clone->m_selection_highlight_type = m_selection_highlight_type;
		}
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

OP_STATUS FormValueTextArea::SetInitialValue(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	if (IsValueExternally())
	{
		OpString buf;
		RETURN_IF_ERROR(GetValueFromHTML(he, buf));
		TextAreaObject *textarea_obj = static_cast<TextAreaObject *>(he->GetFormObject());
		return textarea_obj->SetFormWidgetValue(buf.CStr());
	}

	GetHasOwnValueBool() = FALSE;
	m_own_value.Empty();

	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueTextArea::ResetToDefault(HTML_Element* he)
{
	ResetChangedFromOriginal();

	return SetInitialValue(he);
}

/* virtual */
OP_STATUS FormValueTextArea::GetValueAsText(HTML_Element* he, OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueTextArea*>(this));
	if(IsValueExternally())
	{
		OP_ASSERT(m_own_value.IsEmpty());
		RETURN_IF_ERROR(GetFormObjectValue(he, out_value));
		// The widget uses platform line endings for platform things
		// like the clipboard but we don't want to do this if not
		// required. We don't check line endings when we from
		// DOM insert directly into a widget so that's another
		// cause for bad line endings.
		return FormValue::FixupCRLF(out_value);
	}

	if (GetHasOwnValueBool())
	{
		return out_value.Set(m_own_value);
	}

	// Else use the text contents. Don't cache. This saves memory
	return GetValueFromHTML(he, out_value, TRUE);
}

/* virtual */
OP_STATUS FormValueTextArea::GetValueAsTextLF(HTML_Element* he, OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueTextArea*>(this));
	if(IsValueExternally())
	{
		OP_ASSERT(m_own_value.IsEmpty());
		RETURN_IF_ERROR(GetFormObjectValue(he, out_value));
		// The widget uses platform line endings for platform things
		// like the clipboard but we don't want to do this if not
		// required. We don't check line endings when we from
		// DOM insert directly into a widget so that's another
		// cause for bad line endings. Here we need to normalize
		// them to LF.
		FormValue::FixupLF(out_value);
		return OpStatus::OK;
	}
	if (GetHasOwnValueBool())
	{
		RETURN_IF_ERROR(out_value.Set(m_own_value));
		FormValue::FixupLF(out_value);
		return OpStatus::OK;
	}

	// Else use the text contents. Don't cache. This saves memory
	return GetValueFromHTML(he, out_value, FALSE);
}

static int GetTextAreaTextContent(HTML_Element* elm, uni_char* buf)
{
	// When buf != NULL, the buf must be at least the size of |length+1| where
	// length is what you get when calling the function with NULL as buf.
	int length = 0;
	if (elm->Type() == HE_TEXT)
	{
		TextData* textdata = elm->GetTextData();
		length = textdata->GetTextLen();
		if (buf && length)
		{
			// buf is at least length+1 long
			uni_strncpy(buf, textdata->GetText(), length);
			buf[length] = '\0';
		}
	}
	else
	{
		for (HTML_Element* child = elm->FirstChildActualStyle();
			child;
			child = child->SucActualStyle())
		{
			length += GetTextAreaTextContent(child, buf ? buf+length : NULL);
		}
	}
	return length;
}

/* static */
OP_STATUS FormValueTextArea::GetValueFromHTML(HTML_Element* he, OpString& out_value, BOOL with_crlf) const
{
	int content_len = GetTextAreaTextContent(he, NULL);
	if (content_len > 0)
	{
		uni_char* html_value = out_value.Reserve(content_len+1);
		if (!html_value)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		GetTextAreaTextContent(he, html_value);

		// Skip the first linebreak before first "real" letter.
		uni_char* first_real_char = html_value;

		unsigned int chars_to_delete = first_real_char - html_value;

		if (chars_to_delete)
		{
			out_value.Delete(0, chars_to_delete);
			content_len -= chars_to_delete;
		}

		if (with_crlf)
		{
			RETURN_IF_ERROR(FormValue::FixupCRLF(out_value));
		}
		else
		{
			FormValue::FixupLF(out_value);
		}
	}
	else
	{
		out_value.Empty();
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueTextArea::SetValueFromText(HTML_Element* he, const uni_char* in_value)
{
	return SetValueFromText(he, in_value, FALSE);
}

/* virtual */
OP_STATUS FormValueTextArea::SetValueFromText(HTML_Element* he, const uni_char* in_value, BOOL use_undo_stack)
{
	OP_ASSERT(he->GetFormValue() == this);
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		OP_ASSERT(form_object); // If they said we were external, then we must have a form_object
		// "Bad" line endings are fixed when we read the value back from the widget
		return form_object->SetFormWidgetValue(in_value, use_undo_stack);
	}

	OP_STATUS status = m_own_value.Set(in_value);
	if (OpStatus::IsSuccess(status))
	{
		GetHasOwnValueBool() = TRUE;
		// We may have gotten bad line endings from DOM
		status = FormValue::FixupCRLF(m_own_value);
	}
	return status;
	// DispatchValueChanged();
}

/*static*/
OP_STATUS FormValueTextArea::ConstructFormValueTextArea(HTML_Element* he, FormValue*& out_value)
{
	FormValueTextArea* text_area_value = OP_NEW(FormValueTextArea, ());
	if (!text_area_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	out_value = text_area_value;
	return OpStatus::OK;
}

/* virtual */
BOOL FormValueTextArea::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		OP_ASSERT(m_own_value.IsEmpty());
		return FALSE; // It was wrong to call Externalize
	}

	if (!GetHasOwnValueBool())
	{
		OpString text;
		if (OpStatus::IsSuccess(GetValueFromHTML(form_object_to_seed->GetHTML_Element(), text)))
		{
			form_object_to_seed->SetFormWidgetValue(text.CStr());
		}
	}
	else
	{
		// NULL is ok in TextAreaObject::SetFormWidgetValue
		form_object_to_seed->SetFormWidgetValue(m_own_value.CStr());
		m_own_value.Empty(); // Save memory
	}

	if (m_caret_position != -1)
	{
		if (m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_SELECTION)
			form_object_to_seed->SetSelection(m_selection_start, m_selection_end, m_selection_direction);
		else
			form_object_to_seed->SelectSearchHit(m_selection_start, m_selection_end, m_selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
		form_object_to_seed->SetCaretOffset(m_caret_position);
		TextAreaObject *textarea_obj = static_cast<TextAreaObject *>(form_object_to_seed);
		textarea_obj->SetWidgetScrollPosition(m_listbox_scroll_pos_x, m_listbox_scroll_pos_y);
	}

	return TRUE;
}

/* virtual */
void FormValueTextArea::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		OP_ASSERT(m_own_value.IsEmpty());
		OP_STATUS status = form_object_to_replace->GetFormWidgetValue(m_own_value);
#ifdef UNIX
		if (!m_own_value.IsEmpty())
		{
			// The widget uses platform line endings for platform things
			// like the clipboard but we don't want to do this if not
			// required
			FormValue::FixupCRLF(m_own_value);
		}
#endif // UNIX

		if (OpStatus::IsError(status))
		{
			// Really bad if we get oom here since that means
			// losing the contents of the textarea but what to do?
			m_selection_start = 0;
			m_selection_end = 0;
			m_caret_position = 0;
			m_selection_highlight_type = VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
		}
		else
		{
			form_object_to_replace->GetSelection(m_selection_start, m_selection_end);
			m_selection_highlight_type = form_object_to_replace->IsSearchHitSelected()
				? (form_object_to_replace->IsActiveSearchHitSelected() ? VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT : VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
				: VD_TEXT_HIGHLIGHT_TYPE_SELECTION;
			m_caret_position = form_object_to_replace->GetCaretOffset();
		}

		TextAreaObject *textarea_obj = static_cast<TextAreaObject *>(form_object_to_replace);
		textarea_obj->GetWidgetScrollPosition(m_listbox_scroll_pos_x, m_listbox_scroll_pos_y);

		GetHasOwnValueBool() = TRUE;
		FormValue::Unexternalize(form_object_to_replace);
	}
#ifdef _DEBUG
	else
	{
		// The value musn't have changed! We double check
		OpString widget_value;
		if (OpStatus::IsSuccess(form_object_to_replace->GetFormWidgetValue(widget_value)))
		{
#ifdef UNIX
			// The widget uses platform line endings for platform things
			// like the clipboard but we don't want to do this if not
			// required
			FormValue::FixupCRLF(widget_value);
#endif // UNIX
			OP_ASSERT(m_own_value.Compare(widget_value) == 0);
		}
	}
#endif // _DEBUG
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
/* virtual */
int FormValueTextArea::CreateSpellSessionId(HTML_Element* he, OpPoint *point /* = NULL */)
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
BOOL FormValueTextArea::IsUserEditableText(HTML_Element* he)
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
void FormValueTextArea::GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueTextArea*>(this));

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

INT32 FormValueTextArea::GetMaxTextOffset(HTML_Element* he) const
{
	OP_ASSERT(!IsValueExternally());
	if (GetHasOwnValueBool())
	{
		return m_own_value.Length();
	}

	OpString text;
	if (OpStatus::IsSuccess(GetValueFromHTML(he, text)))
	{
		return text.Length();
	}

	return 0;
}

/* virtual */
void FormValueTextArea::SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetSelection(start_ofs, stop_ofs, direction);
	}
	else
	{
		int max_ofs = GetMaxTextOffset(he);
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
void FormValueTextArea::SelectAllText(HTML_Element *he)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SelectText();
	}
	else
	{
		m_selection_start = 0;
		m_selection_end = GetMaxTextOffset(he);
		m_selection_direction = SELECTION_DIRECTION_DEFAULT;
	}
}

void FormValueTextArea::SelectSearchHit(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit)
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
void FormValueTextArea::ClearSelection(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueTextArea*>(this));
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		return form_object->ClearSelection();
	}

	INT32 selection_collapse_point = GetCaretOffset(he);
	FormValue::SetSelection(he, selection_collapse_point, selection_collapse_point);
}

/* virtual */
INT32 FormValueTextArea::GetCaretOffset(HTML_Element* he) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueTextArea*>(this));
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		return form_object->GetCaretOffset();
	}

	return m_caret_position == -1 ? GetMaxTextOffset(he) : m_caret_position;
}

/* virtual */
void FormValueTextArea::SetCaretOffset(HTML_Element* he, INT32 caret_ofs)
{
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetCaretOffset(caret_ofs);
	}
	else
	{
		int max_ofs = GetMaxTextOffset(he);
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
