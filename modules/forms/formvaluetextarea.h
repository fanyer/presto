/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUETEXTAREA_H
#define FORMVALUETEXTAREA_H

#include "modules/forms/formvalue.h"
#include "modules/display/vis_dev.h" // VD_TEXT_HIGHLIGHT_TYPE

/**
 * Represents an input of type TEXTAREA.
 *
 * @author Daniel Bratell
 */
class FormValueTextArea : public FormValue
{
public:
	FormValueTextArea() :
	  FormValue(VALUE_TEXTAREA),
	  m_selection_start(0),
	  m_selection_end(0),
	  m_selection_direction(SELECTION_DIRECTION_DEFAULT),
	  m_selection_highlight_type(VD_TEXT_HIGHLIGHT_TYPE_SELECTION),
	  m_caret_position(-1),
	  m_listbox_scroll_pos_x(0),
	  m_listbox_scroll_pos_y(0) {};

	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueTextArea* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_TEXTAREA);
		return static_cast<FormValueTextArea*>(val);
	}
	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he);
	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he,
                                         OpString& out_value) const;
	static OP_STATUS ConstructFormValueTextArea(HTML_Element* he,
											FormValue*& out_value);
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
                                           const uni_char* in_value);
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
	                                           const uni_char* in_value,
	                                           BOOL use_undo_stack);

	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);

	/**
	 * Read value from HTML. Called when the end textarea tag is reached.
	 */
	OP_STATUS SetInitialValue(HTML_Element* he);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	// Use base class documentation
	virtual int CreateSpellSessionId(HTML_Element* he, OpPoint *point = NULL);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	// Use base class documentation
	virtual BOOL IsUserEditableText(HTML_Element* he);

	// Use base class documentation
	virtual void GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& selection) const;

	// Use base class documentation
	virtual void SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION selection);

	// Use base class documentation
	virtual void SelectAllText(HTML_Element* he);

	// Use base class documentation
	virtual void SelectSearchHit(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit);

	// Use base class documentation
	virtual void ClearSelection(HTML_Element* he);

	// Use base class documentation
	virtual INT32 GetCaretOffset(HTML_Element* he) const;
	// Use base class documentation
	virtual void SetCaretOffset(HTML_Element* he, INT32 caret_ofs);

	/**
	 * Same as GetValueAsText(), but using LF as line terminator
	 * rather than CRLF, see its documentation.
	 *
	 * The different line ending normalization is needed when
	 * returning the textarea's value back to scripts.
	 */
	virtual OP_STATUS GetValueAsTextLF(HTML_Element* he,
                                       OpString& out_value) const;
private:
	unsigned char& GetHasOwnValueBool() { return GetReservedChar1(); }
	const unsigned char& GetHasOwnValueBool() const { return GetReservedChar1(); }
	OP_STATUS GetValueFromHTML(HTML_Element* he, OpString& out_value, BOOL with_crlf = TRUE) const;
	/**
	 * To be used when the value isn't external.
	 */
	INT32 GetMaxTextOffset(HTML_Element* he) const;

	/**
	 * The form control value if the value isn't in a widget.
	 */
	OpString m_own_value;

	/**
	 * The selection start if there is no widget to ask.
	 */
	INT32 m_selection_start;
	/**
	 * The selection end if there is no widget to ask.
	 */
	INT32 m_selection_end;
	/**
	 * The direction of the selection if there is no widget to ask.
	 */
	SELECTION_DIRECTION m_selection_direction;
	/**
	 * Selection highlight type.
	 */
	VD_TEXT_HIGHLIGHT_TYPE m_selection_highlight_type;
	/**
	 * caret pos if there is no widget to ask. -1 means "at the end" and is the initial value.
	 */
	INT32 m_caret_position;
	/**
	 * Used for form restoring, so that listboxes have the right scroll
	 * position when walking in history.
	 */
	int m_listbox_scroll_pos_x;
	/**
	 * Used for form restoring, so that listboxes have the right scroll
	 * position when walking in history.
	 */
	int m_listbox_scroll_pos_y;

};

#endif // FORMVALUETEXTAREA_H
