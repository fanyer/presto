/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUELIST_H
#define FORMVALUELIST_H

class HTML_Element;
class FormObject;
class SelectContent;
class SelectionObject;

#include "modules/forms/formvalue.h"

/* Enable if you want to maintain current selection for the FormValueList as an index/option cache.

   WARNING: This is not at all advisable right now, and should be considered an exploratory optimisation feature.
   Some important regressions have been reported with it enabled (e.g., http://t/external/jQuery/1.3.1/test/ )
   Soon adressed and fixed one would hope, but until that time..
*/
/* #define CACHE_SELECTED_FIELD */

/**
 * Represents a select.
 */
class FormValueList : public FormValue
{
public:
	FormValueList()
		: FormValue(VALUE_LIST_SELECTION)
		, m_listbox_scroll_pos(0)
#ifdef CACHE_SELECTED_FIELD
		, m_selected_index(-1)
		, m_selected_option(NULL)
#endif
		, m_update_locked(FALSE)
		, m_update_attempt_when_locked(FALSE)
		, m_rebuild_needed(FALSE)
	{};

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
	static FormValueList* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_LIST_SELECTION);
		return static_cast<FormValueList*>(val);
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
	static const FormValueList* GetAs(const FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_LIST_SELECTION);
		return static_cast<const FormValueList*>(val);
	}

	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he, OpString& out_value) const {
		OP_ASSERT(FALSE); // Not meaningful, fix caller
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value) {
		OP_ASSERT(FALSE); // Not meaningful, fix caller
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);


	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he);

	/**
	 * Get all selected values in this list.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param selected_values The array will get all selected values (excluding disabled).
	 */
	OP_STATUS GetSelectedValues(HTML_Element* he,
								OpVector<OpString>& selected_values)
	{
		return GetOptionValues(he, selected_values, FALSE);
	}

	/**
	 * Get all values in this list.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param values The array will get all values.
	 */
	OP_STATUS GetAllValues(HTML_Element* he, OpVector<OpString>& values)
	{
		return GetOptionValues(he, values, TRUE);
	}

	/**
	 * Called when we have to commit to an initial selection.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param use_selected_attributes Set to TRUE to only use 'selected' attributes to determine initial selection;
	 * ignoring previous selected index (if any.)
	 *
	 */
	OP_STATUS SetInitialSelection(HTML_Element* he, BOOL use_selected_attributes);

	/**
	 * Returns TRUE if there is any selected element in the list. This
	 * can be called before GetIndexOfFirstSelected.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	BOOL HasSelectedValue(HTML_Element* he);

	/**
	 * Get the index of the first selected item. The first element in
	 * the list has index 0.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param index Out parameter that will get the index.
	 *
	 * @returns OpStatus::ERR if nothing is selected.
	 */
	OP_STATUS GetIndexOfFirstSelected(HTML_Element* he, unsigned int& index)
	{
		HTML_Element* dummy_option;
		return GetIndexAndElementOfFirstSelected(he, dummy_option, index, FALSE);
	}

	/**
	 * The selectedness of an option has been set, hence clear the selectedness
	 * of the other options if a non-multiple FormValueList.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param he_opt The HTML element of the option element that had
	 *        its selectedness value set.
	 *
	 * @returns OpStatus::OK if the selectedness of this FormValueList was
	 *          successfully updated, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS UpdateSelectedValue(HTML_Element* he, HTML_Element* he_opt);

	/**
	 * Pick up both the index and the element of the first selected option or return an error
	 * if nothing is selected.
	 *
	 * @param[in] sel_he The select owning the FormValue.
	 * @param[out] out_option Set to the HE_OPTION element if successful.
	 * @param[out] index Set to the index (zero-based) if successful.
	 *
	 * @returns OpStatus::ERR if nothing is selected.
	 *
	 * @see GetIndexOfFirstSelected()
	 * @see GetFirstSelectedOption()
	 */
	OP_STATUS GetIndexAndElementOfFirstSelected(HTML_Element* sel_he,
	                                            HTML_Element*& out_option,
	                                            unsigned int& index)
	{
		return GetIndexAndElementOfFirstSelected(sel_he, out_option, index, FALSE);
	}


#if defined(WAND_SUPPORT) || defined(_WML_SUPPORT_)
	/**
	 * Get indexes of all selected elements in the list.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	OP_STATUS GetSelectedIndexes(HTML_Element* he,
								 OpINT32Vector& selected_indexes);
#endif // defined(WAND_SUPPORT) || defined(_WML_SUPPORT_)

	/**
	 * Select or unselect an element in the list.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param index Zero based index.
	 * @param select TRUE if it should be selected, FALSE if it should
	 * be unselected.
	 */
	OP_STATUS SelectValue(HTML_Element* he, unsigned int index, BOOL select) { return SelectValueInternal(he, index, select, FALSE); }

	/**
	 * Returns TRUE if the specified element is selected.
	 *
	 * @param he The HTML element this FormValue belongs to, i.e. the
	 * Select element.
	 *
	 * @param index The zero based index to an Option in the
	 * select. It's this object that is checked.
	 */
	BOOL IsSelected(HTML_Element* he, unsigned int index) const;

	/**
	 * Returns TRUE if the specified element is a selected option in the select.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 * @param option_elm The option. It must be a child to the select.
	 */
	BOOL IsSelectedElement(HTML_Element* he, HTML_Element* option_elm) const;

	static OP_STATUS ConstructFormValueList(HTML_Element* he,
										FormValue*& out_value);

	/**
	 * Someone has sent an ONCHANGE event and we need to update
	 * the logical view of the list.
	 *
	 * @param[in] he The element with the list (select element).
	 */
	void SyncWithFormObject(HTML_Element* he);

	/**
	 * Unselects all options. This might cause the first option to
	 * be automatically selected.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	OP_STATUS UnselectAll(HTML_Element* he);

	/**
	 * Counts the number of options.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	unsigned int GetOptionCount(HTML_Element* he);

#ifdef _WML_SUPPORT_

	/**
	 * Returns the option element for the first selected option
	 * or NULL if none was selected.
	 *
	 * @param[in] select_he The select element (rather a wml:select,
	 * though they're all HE_SELECT to us).
	 */
	HTML_Element* GetFirstSelectedOption(HTML_Element* select_he);
#endif // _WML_SUPPORT_

	/**
	 * Helper method that checks if a certain option is marked as being
	 * selected. This bypasses FormObject and any "automatic" selection
	 * caused by the list being a dropdown that always has a
	 * selected element, typically the first if nothing else is selected.
	 *
	 * This must be public since SelectContent::SetOptionContent needs it
	 * right now.
	 */
	static BOOL IsOptionSelected(HTML_Element* option);

	/**
	 *  Return the selection status, but taking list's local info about current selection into account, if possible.
	 */
	BOOL IsThisOptionSelected(HTML_Element* option) const;

	/**
	 * Gets the value string that would be used in a submit if this option was selected.
	 *
	 * @param[in] he_opt The option element.
	 *
	 * @param[inout] buffer The buffer used to allocate the string if the string doesn't already exist. Must be empty
	 * before the call. Might be non-empty after the call. Modifying it might corrupt the return value.
	 *
	 * @returns a string that might be allocated from buffer. NULL means OOM. Empty string is returned as an empty string.
	 */
	static const uni_char* GetValueOfOption(HTML_Element* he_opt, TempBuffer* buffer);

	/**
	 * Returns TRUE if HTMLOptionElement.selected should return TRUE for
	 * this option that must be outside a select.
	 *
	 * @param[in] opt_he Option element. Must not be in a select.
	 *
	 * @returns TRUE if it is "selected", FALSE otherwise
	 */
	static BOOL DOMIsFreeOptionSelected(HTML_Element* opt_he) { return IsOptionSelected(opt_he); }

	/**
	 * Sets the selected status of a free option. Normally
	 * this is handled by the select.
	 *
	 * @param[in] opt_he Option element. Must not be in a select.
	 */
	static void DOMSetFreeOptionSelected(HTML_Element* opt_he, BOOL selected) { SetOptionSelected(opt_he, selected); }


#ifdef WAND_SUPPORT
	/**
	 * The value has been changed by the user (or by a script). This
	 * basically checks that the current value differs from the value
	 * in the original HTML.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 *
	 * @returns TRUE is the value has changed, FALSE otherwise.
	 */
	BOOL IsListChangedFromOriginal(HTML_Element* he) const;
#endif // WAND_SUPPORT

	/**
	 * Should be called when the tree below the select changes
	 * so that FormValueList might update the widget.
	 *
	 * @param frames_doc The document containing the select.
	 *
	 * @param select_he The select.
	 *
	 * @param option_he The HE_OPTION/HE_OPTGROUP that was removed/added.
	 *
	 * @param added TRUE if option_he was added, FALSE if it was removed.
	 */
	void HandleOptionListChanged(FramesDocument* frames_doc,
								 HTML_Element* select_he,
								 HTML_Element* option_he, BOOL added);

	/**
	 * Locks/unlocks update of the underlying structures (SelectionObject, SelectionContent,
	 * OpWidget) i.e. they are not synchronized with the tree until unlocked.
	 *
	 * @param[in] helm The element with the list.
	 * @param[in] is_externalizing TRUE if called from Externalize
	 * (i.e. from a reflow). We can't
	 * do MarkDirty and such if we're inside the layout engine already.
	 * @param[in] lock If TRUE the updates are locked. Unlocked otherwise.
	 *
	 * @note If any updates were attempted while locked one update is performed
	 * when unlocking.
	 */
	void LockUpdate(HTML_Element* helm, BOOL is_externalizing, BOOL lock);

	/**
	 * Rebuilds all underlying structures (SelectionObject, SelectionContent, OpWidget)
	 * by empting them and filling again based on the tree.
	 * Note that this is no-op if the internal 'needs rebuild' flag is not set.
	 *
	 * @param[in] he The element with the list.
	 *
	 * @param[in] is_externalizing TRUE if called from Externalize
	 * (i.e. from a reflow). We can't
	 * do MarkDirty and such if we're inside the layout engine already.
	 */
	void Rebuild(HTML_Element* he, BOOL is_externalizing = FALSE);

private:
	/**
	 * Helper method that implements GetAllValues and GetSelectedValues.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	OP_STATUS GetOptionValues(HTML_Element* he,
							  OpVector<OpString>& selected_values,
							  BOOL include_not_selected);

	/**
	 * Helper method to get the zero based index or the count of options
	 * in a select element.
	 *
	 * @param he The select element.
	 * @param option_he The option to get the index of. If this is NULL,
	 * then the total count is returned.
	 */
	static unsigned int GetOptionIndexOrCount(HTML_Element* he,
											 HTML_Element* option_he);


	/**
	 * Mark this option as selected or unselected. This works directly
	 * on the HTML tree.
	 */
	static void SetOptionSelected(HTML_Element* he_opt, BOOL selected);

	/**
	 * Calculates if this is a "dropdown" i.e. a list that must always have
	 * something selected or if it's a multiple select.
	 *
	 * If it is not a multiple select, then author errors must
	 * be corrected for instance, so that if the author has
	 * selected multiple elements, only one of them should be
	 * selected, the last one.
	 *
	 * @param[in] he The HTML element this FormValue belongs to.
	 *
	 * @param[out] is_multiple Set to TRUE is this allows multiple (or no) selection.
	 * @param[out] is_dropdown Set to TRUE is this is a dropdown which forces an object
	 * to be auto selected, unless explicitly unselected.
	 */
	static void IsMultipleOrDropdown(HTML_Element* he, BOOL& is_multiple, BOOL& is_dropdown);

	/**
	 * Someone has done a FormValue change. Must inform the widget too since
	 * we never send changes directly to the FormObject.
	 *
	 * @param[in] he The element with the list.
	 *
	 * @param[in] is_externalizing TRUE if called from Externalize
	 * (i.e. from a reflow). We can't
	 * do MarkDirty and such if we're inside the layout engine already.
	 *
	 * @returns TRUE if options were added/removed as part of the sync.
	 */
	BOOL UpdateWidget(HTML_Element* he, BOOL is_externalizing);

	/**
	 * helper function for UpdateWidget. Does one pass over the widget/tree.
	 *
	 * @param[in] he The element with the list.
	 *
	 * @param[in] sel The SelectionObject for the element.
	 *
	 * @param[in] sel_content The sel_content for the element.
	 *
	 * @param[in] is_externalizing TRUE if called from Externalize
	 * (i.e. from a reflow). We can't
	 * do MarkDirty and such if we're inside the layout engine already.
	 *
	 * @param[out] out_was_impossible Set to FALSE if the operation was
	 * successful, TRUE if we failed completely and have to restart with an empty widget.
	 *
	 * @returns TRUE if options were added/removed as part of the sync.
	 */
	BOOL SynchronizeWidgetWithTree(HTML_Element* he, SelectionObject* sel, SelectContent* sel_content, BOOL is_externalizing, BOOL& out_was_impossible);

	/**
	 * Removes everything in the widget. Used to
	 * handle option groups that can't be added incrementally.
	 */
	void EmptyWidget(HTML_Element* he);

	/**
	 * Helper function.
	 */
	OP_STATUS GetIndexAndElementOfFirstSelected(HTML_Element* sel_he,
	                                            HTML_Element*& out_option,
	                                            unsigned int& index,
	                                            BOOL use_first_selected);

	/**
	 * Helper function for SelectValue
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param index Zero based index.
	 * @param select TRUE if it should be selected, FALSE if it should
	 * be unselected.
	 * @param is_externalizing TRUE if we're in the middle of a FormValue::Externalize call (i.e. deep inside the layout engine).
	 */
	OP_STATUS SelectValueInternal(HTML_Element* he, unsigned int index, BOOL select, BOOL is_externalizing);

	/**
	 * Helper method that adds the value in an option to an OpVector
	 * or if it fails empties the OpVector of all values to prevent
	 * leaks.
	 */
	static OP_STATUS AddOptionValueOrClear(OpVector<OpString>& selected_values,
										   HTML_Element* he_opt,
										   HTML_Element* he_sel);

	/**
	 * Accesses the base class storage as our "IsExplicitNotSelected" data.
	 */
	unsigned char& GetIsExplicitNotSelected() { return GetReservedChar1(); }

	/**
	 * Accesses the base class storage as our "IsExplicitNotSelected" data.
	 */
	const unsigned char& GetIsExplicitNotSelected() const { return GetReservedChar1(); }

	/**
	 * Used for form restoring, so that listboxes have the right scroll
	 * position when walking in history.
	 */
	int m_listbox_scroll_pos;

#ifdef CACHE_SELECTED_FIELD
	/**
	 * What is the current selected index; (-1) if none.
	 */
	int m_selected_index;

	/**
	 * If available, the option that the selected index is referring to.
	 */
	HTML_Element *m_selected_option;
#endif // CACHE_SELECTED_FIELD

	/**
	 * TRUE if updates of the widget are locked.
	 */
	BOOL m_update_locked;

	/**
	 * TRUE if any update of the widget was attempted when updates were locked.
	 */
	BOOL m_update_attempt_when_locked;

	/**
	 * TRUE if a full rebuild of the widget is needed i.e. a simple synchonization
	 * with the tree failed.
	 * @see Rebuild().
	 */
	BOOL m_rebuild_needed;
};

#endif // FORMVALUELIST_H
