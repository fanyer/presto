/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUERADIOCHECK_H
#define FORMVALUERADIOCHECK_H

class HTML_Element;
class FormObject;
class FramesDocument;
class ES_Thread;

#include "modules/forms/formvalue.h"

/**
 * Represents a radio button or a checkbox.
 */
class FormValueRadioCheck : public FormValue
{
public:
	FormValueRadioCheck() : FormValue(VALUE_RADIOCHECK) {};

	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);

	static OP_STATUS ConstructFormValueRadioCheck(HTML_Element* he,
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
	static FormValueRadioCheck* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_RADIOCHECK);
		return static_cast<FormValueRadioCheck*>(val);
	}

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static const FormValueRadioCheck* GetAs(const FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_RADIOCHECK);
		return static_cast<const FormValueRadioCheck*>(val);
	}

	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he);
	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he, OpString& out_value) const;
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);
	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);

	/**
	 * Set this radio button or checkbox checked or unchecked.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param is_checked If TRUE the element will be marked checked,
	 * if FALSE it will be marked unchecked.
	 *
	 * @param doc The element's document, needed if
	 * "process_full_radiogroup" is set. Otherwise it can be NULL. The
	 * presence of this parameter is controlled by the capability
	 *
	 * @param process_full_radiogroup TRUE if any side effects (such
	 * as unchecking other radio buttons) should be performed. If TRUE
	 * is used, doc mustn't be NULL.
	 *
	 * @param interrupt_thread The thread causing the change. Needed
	 * to get events correct.
	 */
	void SetIsChecked(HTML_Element* he, BOOL is_checked,
					  FramesDocument* doc = NULL,
					  BOOL process_full_radiogroup = FALSE,
					  ES_Thread* interrupt_thread = NULL);

	/**
	 * If this is a radio button in a radio group, then this method will make
	 * every other radio button in that group unchecked without sending any
	 * events.
	 *
	 * @param radio_elm The HTML element this FormValue belongs to.
	 * @param doc The document
	 * @param thread The thread initiating this action or NULL.
	 */
	void UncheckRestOfRadioGroup(HTML_Element* radio_elm, FramesDocument* doc, ES_Thread* thread) const;

	/**
	 * Returns TRUE if this is checked.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	BOOL IsChecked(HTML_Element* he) const;

	/**
	 * Returns TRUE if this is checked by default.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	BOOL IsCheckedByDefault(HTML_Element* he) const;

	/**
	 * Checks if the "is in checked radio group" flag is set, and 
	 * returns TRUE in that case. This is O(1) but possibly not 100% reliable.
	 */
	BOOL IsInCheckedRadioGroup();

	/**
	 * Marks this radio button as being in a group of radio buttons where
	 * at least one is selected. This is used for the :indeterminate
	 * pseudo class. ONLY FOR FORMS INTERNAL USE.
	 */
	void SetIsInCheckedRadioGroup(BOOL value);

	/**
	 * When a click on a checkbox happens the value should be changed
	 * before event handlers. Then if the click event was cancelled,
	 * then the original value should be restored. otherwise a change
	 * event should be sent. So this has to be called before the onclick
	 * event.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	void SaveStateBeforeOnClick(HTML_Element* he);

	/**
	 * See the SaveStateBeforeOnClick documentation.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param onclick_cancelled TRUE if the onclick event was
	 * cancelled.
	 *
	 * @returns TRUE if the value was changed by the onclick. If
	 * onclick_cancelled is TRUE, then it will always return FALSE.
	 */
	BOOL RestoreStateAfterOnClick(HTML_Element* he, BOOL onclick_cancelled);

	// Used in FormManager. Should not be used anywhere else. Especially not from outside the forms module
	/**
	 * Iterates through the form and makes sure the IsInCheckedGroup flag is
	 * correct on all radio buttons that doesn't belong to a form.
	 * Method internal to the forms module.
	 */
	static OP_STATUS UpdateFreeButtonsCheckedInGroupFlags(FramesDocument* frames_doc);

	// Used in FormManager. Should not be used anywhere else. Especially not from outside the forms module
	/**
	 * Iterates through the form and makes sure the IsInCheckedGroup flag is
	 * correct on all radio buttons. Method internal to the forms module.
	 *
	 * @param frames_doc The document with the radio button.
	 * @param form_element The form to iterate through.
	 */
	static OP_STATUS UpdateCheckedInGroupFlags(FramesDocument* frames_doc,
											   HTML_Element* form_element);

	/**
	 * Tells the FormValue that a recent change was made
	 * from a script setting the .checked property or
	 * from a click by the user.
	 * That will block future changes from the changed attribute */
	void SetWasChangedExplicitly(FramesDocument* frames_doc, HTML_Element* form_element);

	/**
	 * Returns whether setting the checked property also will check
	 * the checkbox/radio button.
	 */
	BOOL IsCheckedAttrChangingState(FramesDocument* frames_doc, HTML_Element* form_element) const;
private:
	/**
	 * Unchecks and previous set radio buttons in the radio group.
	 */
	void UncheckPrevRadioButtons(FramesDocument* frames_doc,
								 HTML_Element* radio_elm);

	/**
	 * @param names_with_check Really a OpStringSet, but that didn't
	 * exist so we use a dummy hash table with only NULL as data.
	 */
	static OP_STATUS UpdateCheckedInGroupFlagsHelper(FramesDocument* frames_doc, HTML_Element* fhe,
													OpStringHashTable<uni_char>& names_with_check,
													int mode);

	/**
	 * Accesses the base class storage as our "IsChecked" data.
	 */
	unsigned char& GetIsCheckedBool() { return GetReservedChar1(); }
	const unsigned char& GetIsCheckedBool() const { return GetReservedChar1(); }
};

#endif // FORMVALUERADIOCHECK_H
