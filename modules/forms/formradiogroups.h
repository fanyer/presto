/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMRADIOGROUPS_H
#define FORMRADIOGROUPS_H

#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

class HTML_Element;
class FramesDocument;

/**
 * A radio button must be able to find all its friends. It does so by asking the Form for a named group.
 */
class FormRadioGroup
{
	OpString m_name; // Used as key in the hash table in FormRadioGroups
	OpVector<HTML_Element> m_radio_buttons;
	BOOL m_might_have_several_checked;
	BOOL m_has_any_checked;
	/**
	 * Until some radio button in a group has been explicitly changed,
	 * setting the checked attribute will affect checked state. So
	 * to know when to stop, we have to keep track of whether
	 * something was explicitly checked or not.
	 *
	 * Explicitly set means set by the user of by the checked property in
	 * scripts.
	 */
	BOOL m_has_been_explicitly_checked;

public:
	FormRadioGroup() : m_might_have_several_checked(FALSE), m_has_any_checked(FALSE), m_has_been_explicitly_checked(FALSE) {}
	OP_STATUS Init(const uni_char* name) { OP_ASSERT(name); return m_name.Set(name); }
	const uni_char* GetName() { OP_ASSERT(m_name.CStr()); return m_name.CStr(); }

	/**
	 * Uncheck other buttons in the group and sets the "is_in_checked_group" flag to TRUE.
	 */
	void UncheckOthers(FramesDocument* frames_doc, HTML_Element *radio_button);

	/**
	 * Returns TRUE if any of the buttons beside |radio_button| is checked.
	 */
	BOOL HasOtherChecked(HTML_Element* radio_button) const;

	/**
	 * Returns TRUE if any of the buttons are checked (uses cached value).
	 */
	BOOL HasAnyChecked() const { return m_has_any_checked; }

	/**
	 * Set or clear the flag that says that this is in an checked
	 * group. Will/might change the :indeterminate class so this is
	 * potentially a very expensive method that should be 
	 * avoided if possible.
	 *
	 * @param frames_doc the document. Use NULL if pseudo classes shouldn't be recalculated.
	 * @param checked What the flag should be set to.
	 */
	void SetIsInCheckedGroup(FramesDocument* frames_doc, BOOL checked);

	/**
	 * Returns the group of the button. NULL if out of memory.
	 */
	OP_STATUS RegisterRadioButton(HTML_Element* radio_button, BOOL might_be_registered_already, BOOL will_uncheck_afterwards);
	void UnregisterRadioButton(HTML_Element* radio_button);
	void UnregisterThoseWithFormAttr();

	/**
	 * At some time or other a radio button in the control
	 * was implicitly checked if this returns TRUE.
	 */
	BOOL HasBeenExplicitlyChecked() { return m_has_been_explicitly_checked; }

	/**
	 * Until some radio button in a group has been explicitly changed,
	 * setting the checked attribute will affect checked state. So
	 * to know when to stop, we have to keep track of whether
	 * something was explicitly checked or not.
	 *
	 * Explicitly set means set by the user of by the checked property in
	 * scripts.
	 */
	void SetHasBeenExplicitlyChecked() { m_has_been_explicitly_checked = TRUE; }
};

class FormRadioGroups
{
	OpAutoStringHashTable<FormRadioGroup> m_radio_groups;

public:
	/**
	 * Get the group with the given name. The name will be normalized.
	 * @param create TRUE if the group should be created if it doesn't exist.
	 * @return NULL if OOM when create is set.
	 */
	FormRadioGroup* Get(const uni_char* name, BOOL create);
	/**
	 * To be called when an attribute changes.
	 */
	void UnregisterAllRadioButtonsConnectedByName();
};

class DocumentRadioGroups
{
private:
	OpHashTable m_formradiogroups;
	/** If there are radio buttons with non-functional form attributes. */
	BOOL m_may_have_orphans;
public:
	DocumentRadioGroups() : m_may_have_orphans(TRUE) {}
	~DocumentRadioGroups();
	/**
	 * If it returns NULL and create was TRUE, then it is an OOM.
	 */
	FormRadioGroups* GetFormRadioGroupsForRadioButton(FramesDocument* frames_doc, HTML_Element* radio_elm, BOOL create);
	/**
	 * Returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS RegisterRadioAndUncheckOthers(FramesDocument* frames_doc, HTML_Element* form_elm, HTML_Element* radio_button);
	OP_STATUS RegisterRadio(HTML_Element* form_elm, HTML_Element* radio_button);
	OP_STATUS RegisterNewRadioButtons(FramesDocument* frames_doc, HTML_Element* form_element);
	void UnregisterAllRadioButtonsConnectedByName(HTML_Element* form);
	void UnregisterAllRadioButtonsInTree(FramesDocument* frames_doc, HTML_Element* tree);
	void UnregisterFromRadioGroup(FramesDocument* frames_doc, HTML_Element* radio_button);

	/**
	 * A late form element might take ownership of existing elements if those existing
	 * elements have the form attribute pointing to the late form element.
	 *
	 * Call this for a form element to move all necessary radio buttons to their right
	 * place.
	 *
	 * @param[in] frames_doc The document owning the form element.
	 * @param[in] form_element The HE_FORM element.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AdoptOrphanedRadioButtons(FramesDocument* frames_doc, HTML_Element* form_element);

private:
	void operator=(const DocumentRadioGroups& other);
	DocumentRadioGroups(const DocumentRadioGroups& other);
	/**
	 * If it returns NULL and create was TRUE, then it is an OOM.
	 */
	FormRadioGroups* GetFormRadioGroupsForForm(HTML_Element* form, BOOL create);

	BOOL MayHaveOrphans() { return m_may_have_orphans; }
	void SetHasOrphan() { m_may_have_orphans = TRUE; }
};


#endif // FORMRADIOGROUPS_H
