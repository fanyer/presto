/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUETEXT_H
#define FORMVALUETEXT_H

#include "modules/forms/formvalue.h"
#include "modules/display/vis_dev.h" // VD_TEXT_HIGHLIGHT_TYPE

/**
 * This is used by stuff that has a normal text value. That is
 * INPUT_TEXT, INPUT_PASSWORD, INPUT_URI and similar
 * types.
 *
 * @author Daniel Bratell
 */
class FormValueText : public FormValue
{
public:
	FormValueText() :
	  FormValue(VALUE_TEXT),
	  m_selection_start(0),
	  m_selection_end(0),
	  m_selection_direction(SELECTION_DIRECTION_DEFAULT),
	m_selection_highlight_type(VD_TEXT_HIGHLIGHT_TYPE_SELECTION),
	  m_caret_position(0) {};

	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);
	static OP_STATUS ConstructFormValueText(HTML_Element* he,
											HLDocProfile* hld_profile,
											FormValue*& out_value);

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueText* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_TEXT);
		return static_cast<FormValueText*>(val);
	}
	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he);
	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he, OpString& out_value) const;
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
	                                   const uni_char* in_value,
	                                   BOOL use_undo_stack);
	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	// Use base class documentation
	virtual int CreateSpellSessionId(HTML_Element* he, OpPoint *point = NULL);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	// Use base class documentation
	virtual BOOL IsUserEditableText(HTML_Element* he);

	// Use base class documentation
	virtual void GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction) const;

	// Use base class documentation
	virtual void SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction);

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
	 * Copies the value from the other FormValue without any safety
	 * nets at all. This is only to be used internally
	 * in the FormValue system.
	 *
	 * <p>If the operation fails because of OOM it will just not copy the string since the caller doesn't care.
	 *
	 * @param[in] other The other FormValue to copy the text from.
	 */
	void CopyValueInternal(const FormValueText& other)
	{
		OP_ASSERT(!IsValueExternally());
		OP_ASSERT(!other.IsValueExternally());
		OpStatus::Ignore(m_text.Set(other.m_text));
	}

	/**
	 * To get a raw, free text buffer with the value, to be used
	 * when converting from input=text to input=hidden, so that
	 * we have something to send to SetAttr. Allocated with new[].
	 */
	uni_char* GetCopyOfInternalText();

	/**
	 * Removes chars (like linebreaks) that are not allowed in the string
	 */
	static void FilterChars(OpString& val_to_fix);

protected:
	void CopyToClone(HTML_Element* he, FormValueText* clone);

	/**
	 * Use real_value as "Display Value" if display_value.CStr() is
	 * NULL after a successful call. That is so that the fast-path
	 * (doing nothing at all) requires no allocations or string
	 * copies.
	 */
	virtual OP_STATUS ConvertRealValueToDisplayValue(const uni_char* real_value,
	                                                 OpString& display_value) const { return OpStatus::OK; }
	virtual OP_STATUS ConvertDisplayValueToRealValue(OpString& value) const { return OpStatus::OK; }

	OpString m_text;

private:
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
	 * caret pos if there is no widget to ask. -1 means "at the end" and is the initial value.
	 */
	/**
	 * Selection highlight type.
	 */
	VD_TEXT_HIGHLIGHT_TYPE m_selection_highlight_type;

	INT32 m_caret_position;
};

#endif // FORMVALUETEXT_H
