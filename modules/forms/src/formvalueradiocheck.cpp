/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvalueradiocheck.h"

#include "modules/logdoc/htm_elm.h" // HTML_Element
#include "modules/forms/piforms.h" // FormObject
#include "modules/forms/formmanager.h" // FormManager
#include "modules/forms/formvaluelistener.h" // FormValueListener
#include "modules/forms/src/formiterator.h" // FormIterator

#include "modules/doc/frm_doc.h"
#include "modules/forms/formradiogroups.h"
#include "modules/layout/layout_workplace.h"

OP_STATUS FormRadioGroup::RegisterRadioButton(HTML_Element* radio_button, BOOL might_be_registered_already, BOOL will_uncheck_others_afterwards)
{
	if (might_be_registered_already)
	{
		// When this is hooked in at a good spot this check shouldn't be necessary
		if (m_radio_buttons.Find(radio_button) != -1)
		{
			return OpStatus::OK;
		}
	}
	OP_ASSERT(m_radio_buttons.Find(radio_button) == -1);
	
	if (!will_uncheck_others_afterwards)
	{
		m_might_have_several_checked = TRUE;
	}
	return m_radio_buttons.Add(radio_button);
}

void FormRadioGroup::UnregisterRadioButton(HTML_Element* radio_button)
{
	m_radio_buttons.RemoveByItem(radio_button);
}

static void delete_FormRadioGroups(const void* key, void* data)
{
	OP_ASSERT(data);
	FormRadioGroups* groups = static_cast<FormRadioGroups*>(data);
	OP_DELETE(groups);
}

DocumentRadioGroups::~DocumentRadioGroups()
{
	m_formradiogroups.ForEach(delete_FormRadioGroups);
}

FormRadioGroups* DocumentRadioGroups::GetFormRadioGroupsForRadioButton(FramesDocument* frames_doc, HTML_Element* radio_elm, BOOL create)
{
	OP_ASSERT(radio_elm && radio_elm->IsMatchingType(HE_INPUT, NS_HTML) && radio_elm->GetInputType() == INPUT_RADIO);
	HTML_Element* form_elm = FormManager::FindFormElm(frames_doc, radio_elm);
	if (!form_elm)
	{
		while (radio_elm && radio_elm->Type() != HE_DOC_ROOT)
		{
			radio_elm = radio_elm->Parent();
		}
		if (!radio_elm)
		{
			// Not in document -> no radio groups, sorry
			return NULL;
		}
	}

	return GetFormRadioGroupsForForm(form_elm, create);
}

FormRadioGroups* DocumentRadioGroups::GetFormRadioGroupsForForm(HTML_Element* form, BOOL create)
{
	OP_ASSERT(!form || form->IsMatchingType(HE_FORM, NS_HTML));

	void* key = form; // May be NULL
	void* data;
	FormRadioGroups* groups = NULL;
	if (OpStatus::IsError(m_formradiogroups.GetData(key, &data)))
	{
		if (create)
		{
			groups = OP_NEW(FormRadioGroups, ());
			if (groups)
			{
				data = groups;
				if (OpStatus::IsError(m_formradiogroups.Add(key, data)))
				{
					OP_DELETE(groups);
					groups = NULL;
				}
			}
		}
	}
	else
	{
		groups = static_cast<FormRadioGroups*>(data);
	}

	return groups;
}

void DocumentRadioGroups::UnregisterFromRadioGroup(FramesDocument* frames_doc, HTML_Element* radio_button)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(radio_button);
	OP_ASSERT(radio_button->IsMatchingType(HE_INPUT, NS_HTML));
	OP_ASSERT(radio_button->GetInputType() == INPUT_RADIO);

	const uni_char* group_name = radio_button->GetName();
	if (group_name) // Radio buttons without a name attribute has no group (name="" has a group though)
	{
		FormRadioGroups* groups = GetFormRadioGroupsForRadioButton(frames_doc, radio_button, FALSE);
		if (groups) // Not inside the document or there was an OOM when it registered
		{
			FormRadioGroup* group = groups->Get(group_name, FALSE);
			OP_ASSERT(group);
			if (group) // Or there was an OOM when it registered
			{
				group->UnregisterRadioButton(radio_button);

				FormValueRadioCheck* value = FormValueRadioCheck::GetAs(radio_button->GetFormValue());
				if (value->IsChecked(radio_button) && !group->HasOtherChecked(radio_button))
				{
					group->SetIsInCheckedGroup(frames_doc, FALSE); // Expensive call
				}
			}
		}
	}
}

void DocumentRadioGroups::UnregisterAllRadioButtonsInTree(FramesDocument* frames_doc, HTML_Element* tree)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(tree);

	HTML_Element* stop = static_cast<HTML_Element*>(tree->NextSibling());
	do
	{
		if (tree->IsMatchingType(HE_INPUT, NS_HTML) && tree->GetInputType() == INPUT_RADIO)
		{
			UnregisterFromRadioGroup(frames_doc, tree);
		}
		tree = tree->Next();
	}
	while (tree != stop);
}

OP_STATUS DocumentRadioGroups::AdoptOrphanedRadioButtons(FramesDocument* frames_doc, HTML_Element* form_element)
{
	BOOL oom = FALSE;
	if (MayHaveOrphans() && form_element->GetId())
	{
		HTML_Element* elm = frames_doc->GetDocRoot();
		while (elm)
		{
			if (elm->IsMatchingType(HE_INPUT, NS_HTML) &&
			    elm->GetInputType() == INPUT_RADIO &&
			    elm->HasAttr(ATTR_FORM) &&
			    FormManager::BelongsToForm(frames_doc, form_element, elm))
			{
				UnregisterFromRadioGroup(frames_doc, elm);
				if (OpStatus::IsError(RegisterRadio(form_element, elm)))
				{
					oom = TRUE;
				}
			}
			elm = elm->NextActualStyle();
		}
	}

	return oom ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

void DocumentRadioGroups::UnregisterAllRadioButtonsConnectedByName(HTML_Element* form)
{
	for (int i = 0; i < 2; i++)
	{
		FormRadioGroups* groups = GetFormRadioGroupsForForm(form, FALSE);
		if (groups)
		{
			groups->UnregisterAllRadioButtonsConnectedByName();
		}
		form = NULL; // next iteration look at the free radio buttons
	}
}

void FormRadioGroup::UnregisterThoseWithFormAttr()
{
	for (unsigned i = 0; i < m_radio_buttons.GetCount(); i++)
	{
		HTML_Element* radio_button = m_radio_buttons.Get(i);
		if (radio_button->HasAttr(ATTR_FORM))
		{
			m_radio_buttons.Remove(i);
			i--;
		}
	}
}

static void UnregisterThoseWithFormAttr(const void* key, void* data)
{
	OP_ASSERT(data);
	FormRadioGroup* group = static_cast<FormRadioGroup*>(data);
	group->UnregisterThoseWithFormAttr();
}

void FormRadioGroups::UnregisterAllRadioButtonsConnectedByName()
{
	m_radio_groups.ForEach(UnregisterThoseWithFormAttr);
}

OP_STATUS DocumentRadioGroups::RegisterRadio(HTML_Element* form_elm, HTML_Element* radio_button)
{
	OP_ASSERT(!form_elm || form_elm->IsMatchingType(HE_FORM, NS_HTML));
	OP_ASSERT(radio_button && radio_button->IsMatchingType(HE_INPUT, NS_HTML) && radio_button->GetInputType() == INPUT_RADIO);

#ifdef _DEBUG
	HTML_Element* root = radio_button;
	while (root->Parent())
		root = root->Parent();
	OP_ASSERT(root->Type() == HE_DOC_ROOT);
#endif // _DEBUG

	const uni_char* group_name = radio_button->GetName();
	if (group_name) // Radio buttons with a name attribute have no group (name="" creates a group though)
	{
		FormRadioGroups* groups = GetFormRadioGroupsForForm(form_elm, TRUE);
		FormRadioGroup* group;
		if (!groups || !(group = groups->Get(group_name, TRUE)))
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		RETURN_IF_ERROR(group->RegisterRadioButton(radio_button, FALSE, FALSE));

		FormValueRadioCheck* value = FormValueRadioCheck::GetAs(radio_button->GetFormValue());
		if (value->IsChecked(radio_button))
		{
			if (!group->HasAnyChecked())
			{
				group->SetIsInCheckedGroup(NULL, TRUE); // XXX: Invalidate them as well if the value changed?
			}
		}
		else
		{
			value->SetIsInCheckedRadioGroup(group->HasAnyChecked());
		}
	}

	return OpStatus::OK;
}

OP_STATUS DocumentRadioGroups::RegisterNewRadioButtons(FramesDocument* frames_doc, HTML_Element* form_elm)
{
	OP_ASSERT(form_elm && form_elm->IsMatchingType(HE_FORM, NS_HTML));
	HTML_Element* elm = frames_doc->GetDocRoot();
	OP_ASSERT(elm->IsAncestorOf(form_elm));

	FormRadioGroups* groups = GetFormRadioGroupsForForm(form_elm, TRUE);
	if (!groups)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	const uni_char* form_elm_id = form_elm->GetId();

	while (elm)
	{
		if (elm->IsMatchingType(HE_INPUT, NS_HTML) && elm->GetInputType() == INPUT_RADIO)
		{
			const uni_char* group_name = elm->GetName();
			if (group_name)
			{
				// This is used to connect form elements which used to be connected by 
				// name so if they have no FORM attribute they are already registered.
				const uni_char* form_attr = FormManager::GetFormIdString(elm);
				if (form_attr)
				{
					if (FormManager::BelongsToForm(frames_doc, form_elm, elm))
					{
						FormRadioGroup* group = groups->Get(group_name, TRUE);
						if (!group)
						{
							return OpStatus::ERR_NO_MEMORY;
						}
						group->RegisterRadioButton(elm, FALSE, FALSE);
						if (form_elm_id && !uni_str_eq(form_elm_id, form_attr))
						{
							SetHasOrphan();
						}
					}
					else if (FormManager::FindFormElm(frames_doc, elm) == NULL)
					{
						SetHasOrphan();
						FormRadioGroups* free_groups = GetFormRadioGroupsForForm(NULL, TRUE);
						if (!free_groups)
						{
							return OpStatus::ERR_NO_MEMORY;
						}
						FormRadioGroup* group = free_groups->Get(group_name, TRUE);
						if (!group)
						{
							return OpStatus::ERR_NO_MEMORY;
						}
						group->RegisterRadioButton(elm, TRUE, FALSE);
					}
				}
			}
		}
		elm = elm->NextActualStyle();
	}

	return OpStatus::OK; 
}

OP_STATUS DocumentRadioGroups::RegisterRadioAndUncheckOthers(FramesDocument* frames_doc, HTML_Element* form_elm, HTML_Element* radio_button)
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(!form_elm || form_elm->IsMatchingType(HE_FORM, NS_HTML));
	OP_ASSERT(radio_button && radio_button->IsMatchingType(HE_INPUT, NS_HTML) && radio_button->GetInputType() == INPUT_RADIO);
#ifdef _DEBUG
	HTML_Element* root = radio_button;
	while (root->Parent())
		root = root->Parent();
	OP_ASSERT(root->Type() == HE_DOC_ROOT);
#endif // _DEBUG

	const uni_char* form_attr = FormManager::GetFormIdString(radio_button);
	if (form_attr)
	{
		form_elm = FormManager::FindFormElm(frames_doc, radio_button);
		if (!form_elm)
		{
			SetHasOrphan();
		}
		else
		{
			const uni_char* form_elm_id = form_elm->GetId();
			if (form_elm_id && !uni_str_eq(form_attr, form_elm_id))
			{
				SetHasOrphan();
			}
		}
	}

	const uni_char* group_name = radio_button->GetName();
	if (group_name)
	{
		FormRadioGroups* groups = GetFormRadioGroupsForForm(form_elm, TRUE);
		FormRadioGroup* group;
		if (!groups || !(group = groups->Get(group_name, TRUE)))
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		RETURN_IF_ERROR(group->RegisterRadioButton(radio_button, FALSE, TRUE));

		FormValueRadioCheck* value = FormValueRadioCheck::GetAs(radio_button->GetFormValue());
		if (value->IsChecked(radio_button))
		{
			group->UncheckOthers(frames_doc, radio_button);
			if (!group->HasAnyChecked())
			{
				group->SetIsInCheckedGroup(frames_doc, TRUE);
			}
			else
			{
				value->SetIsInCheckedRadioGroup(TRUE);
			}
		}
		else
		{
			value->SetIsInCheckedRadioGroup(group->HasAnyChecked());
		}
	}

	return OpStatus::OK;
}

FormRadioGroup* FormRadioGroups::Get(const uni_char* name, BOOL create)
{
	OP_ASSERT(name); // Radio buttons with no name have no button group
	FormRadioGroup* data = NULL;
	if (OpStatus::IsError(m_radio_groups.GetData(name, &data)) && create)
	{
		data = OP_NEW(FormRadioGroup, ());
		if (data)
		{
			// Give it a name to store so that the key in the hashtable isn't
			// destroyed under us.
			if (OpStatus::IsError(data->Init(name)) ||
				OpStatus::IsError(m_radio_groups.Add(data->GetName(), data)))
			{
				OP_DELETE(data);
				data = NULL;
			}
		}
	}
	return data;
}

BOOL FormRadioGroup::HasOtherChecked(HTML_Element* radio_button) const
{
	for (unsigned i = 0; i < m_radio_buttons.GetCount(); i++)
	{
		HTML_Element* elm = m_radio_buttons.Get(i);
		if (elm != radio_button)
		{
			FormValueRadioCheck* value = FormValueRadioCheck::GetAs(elm->GetFormValue());
			if (value->IsChecked(elm))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

void FormRadioGroup::SetIsInCheckedGroup(FramesDocument* frames_doc, BOOL checked)
{
	for (unsigned i = 0; i < m_radio_buttons.GetCount(); i++)
	{
		HTML_Element* elm = m_radio_buttons.Get(i);
		FormValueRadioCheck* value = FormValueRadioCheck::GetAs(elm->GetFormValue());
		value->SetIsInCheckedRadioGroup(checked);
		if (frames_doc)
		{
			// Changing the :indeterminate class
			frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(elm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
		}
	}
	m_has_any_checked = checked;
}

// Lowest bit is "is checked". 
// Next bit is "there is a selected field in the radio group" (used by the :indeterminate pseudo style).
// Next bit is "was checked before onclick".
// Next bit is if this was changed from the .checked script property.
// The next 4 bits are a counter on
// how deep the restore stack is so that we restore at the right place.
#define SET_IS_CHECKED(set) do { GetIsCheckedBool() = (unsigned char)((GetIsCheckedBool() & 0xfe) | int(set)); } while(0)
#define GET_IS_CHECKED() ((GetIsCheckedBool() & 0x1) != 0)

#define SET_IS_IN_CHECKED_GROUP(set) do { GetIsCheckedBool() = (unsigned char)((GetIsCheckedBool() & 0xfd) | (int(set) << 1)); } while(0)
#define GET_IS_IN_CHECKED_GROUP() (((GetIsCheckedBool() >> 1) & 0x1) != 0)

#define SET_IS_CHECKED_BEFORE_ONCLICK(set) do { GetIsCheckedBool() = (unsigned char)((GetIsCheckedBool() & 0xfb) | (int(set) << 2)); } while(0)
#define GET_IS_CHECKED_BEFORE_ONCLICK() (((GetIsCheckedBool() >> 2) & 0x1) != 0)

#define SET_IS_CHANGED_FROM_CHECKED_PROPERTY(set) do { GetIsCheckedBool() = (unsigned char)((GetIsCheckedBool() & 0xf7) | (int(set) << 3)); } while(0)
#define GET_IS_CHANGED_FROM_CHECKED_PROPERTY() (((GetIsCheckedBool() >> 3) & 0x1) != 0)

#ifdef SUPPORT_NESTED_CLICKS // Don't enable this before bug 211433 and 211435 is fixed
#error "Need bug 211433 and bug 211435 fixed before enabling this"
#define SET_ONCLICK_RESTORE_COUNT(set) do { GetIsCheckedBool() = (unsigned char)((GetIsCheckedBool() & 0x0f) | ((set) << 4)); } while(0)
#define GET_ONCLICK_RESTORE_COUNT() ((GetIsCheckedBool() >> 4) & 0x0f)
#define IS_ONCLICK_RESTORE_COUNT_MAX(val) ((val) == 15)
#endif // SUPPORT_NESTED_CLICKS

/* virtual */
OP_STATUS FormValueRadioCheck::GetValueAsText(HTML_Element* he, OpString& out_value) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueRadioCheck*>(this));

	const uni_char* attr_value = he->GetStringAttr(ATTR_VALUE);
	if (!attr_value)
	{
		// This is illegal HTML but MSIE defaults to the value "on" so we do the same.
		attr_value = UNI_L("on");
	}
	return out_value.Set(attr_value);
}

/* virtual */
OP_STATUS FormValueRadioCheck::SetValueFromText(HTML_Element* he, const uni_char* in_value)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(FALSE); // Can the caller use SetIsChecked instead of playing with strings?
	return OpStatus::ERR; // not implemented. Use SetIsChecked instead.
}


/*static*/
OP_STATUS FormValueRadioCheck::ConstructFormValueRadioCheck(HTML_Element* he,
															FormValue*& out_value)
{
	FormValueRadioCheck* check_value = OP_NEW(FormValueRadioCheck, ());
	if (!check_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_ASSERT(/* he->Type() == HE_OPTION || */
		(he->Type() == HE_INPUT &&
			  (he->GetInputType() == INPUT_CHECKBOX ||
			   he->GetInputType() == INPUT_RADIO)));
	BOOL is_checked = he->GetBoolAttr(ATTR_CHECKED);
	if (is_checked)
	{
		check_value->GetIsCheckedBool() = 0x3;
	}

	out_value = check_value;

	return OpStatus::OK;
}

BOOL FormValueRadioCheck::IsInCheckedRadioGroup()
{
	return GET_IS_IN_CHECKED_GROUP();
}

void FormRadioGroup::UncheckOthers(FramesDocument* frames_doc, HTML_Element *radio_button)
{
	OP_ASSERT(radio_button);
	BOOL saw_radio_button = FALSE;
	for (unsigned int i = 0; i < m_radio_buttons.GetCount(); i++)
	{
		HTML_Element* other_radio_elm = m_radio_buttons.Get(i);
//		OP_ASSERT(IsRadioButtonInSameGroup(frames_doc, other_radio_elm, m_is_global_group, radio_button->GetName()));
		BOOL new_pseudo_classes = FALSE;
		if (radio_button != other_radio_elm)
		{
			FormValueRadioCheck* other_radio =
				FormValueRadioCheck::GetAs(other_radio_elm->GetFormValue());
			// Shouldn't be here unless there were a selected radio button in the group
			if (!other_radio->IsInCheckedRadioGroup())
			{
				other_radio->SetIsInCheckedRadioGroup(TRUE);
				new_pseudo_classes = TRUE;
			}

			if (other_radio->IsChecked(other_radio_elm))
			{
				// It can not also be checked
				other_radio->SetIsChecked(other_radio_elm, FALSE, NULL, FALSE);
				FormValueListener::HandleValueChanged(frames_doc, other_radio_elm, FALSE, FALSE, NULL);
				new_pseudo_classes = TRUE;
				if (!m_might_have_several_checked)
				{
					while (++i < m_radio_buttons.GetCount() && !saw_radio_button)
					{
						saw_radio_button = radio_button == m_radio_buttons.Get(i);
					}
				}
			}
		}
		else
		{
			saw_radio_button = TRUE;
		}

		if (new_pseudo_classes)
		{
			// XXX: Use FormValueListener instead?
			frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(other_radio_elm, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
		}
	}

	m_might_have_several_checked = FALSE;

	if (!saw_radio_button)
	{
		OP_ASSERT(!"Not reachable?");
		m_radio_buttons.Add(radio_button);
	}
}

void FormValueRadioCheck::UncheckPrevRadioButtons(FramesDocument* frames_doc, HTML_Element* radio_elm)
{
	OP_ASSERT(radio_elm->GetInputType() == INPUT_RADIO);

	const uni_char* group_name = radio_elm->GetName();

	if (group_name) // radio buttons without a name attribute has no group (but name="" creates a group)
	{
		// Clear previous set radio buttons with the same name because several
		// radio buttons may be checked by the HTML.
		FormRadioGroups* radio_groups = frames_doc->GetLogicalDocument()->GetRadioGroups().GetFormRadioGroupsForRadioButton(frames_doc, radio_elm, FALSE);
		if (!radio_groups) // OOM earlier or outside the document
		{
			return;
		}

		FormRadioGroup* radio_group = radio_groups->Get(group_name, FALSE);
		OP_ASSERT(radio_group);

		if (!radio_group) // Hmm, unnecessary check?
		{
			return;
		}

		radio_group->UncheckOthers(frames_doc, radio_elm); // Will also make sure this radio_elm is registered
	}
}

void FormValueRadioCheck::SetIsInCheckedRadioGroup(BOOL value)
{
	OP_ASSERT(value == FALSE || value == TRUE || !"value isn't a true BOOL");
	SET_IS_IN_CHECKED_GROUP(value);
}

void FormValueRadioCheck::UncheckRestOfRadioGroup(HTML_Element* radio_elm, FramesDocument* doc, ES_Thread* thread) const
{
	const uni_char* group_name = radio_elm->GetName();
	if (group_name) // radio buttons without a name attribute has no group (but name="" creates a group)
	{
		DocumentRadioGroups& docgroups = doc->GetLogicalDocument()->GetRadioGroups();
		FormRadioGroups* groups = docgroups.GetFormRadioGroupsForRadioButton(doc, radio_elm, FALSE);
		if (groups) // else either not inserted into the document yet or we got OOM while registering all radio buttons
		{
			FormRadioGroup* group = groups->Get(group_name, FALSE);
			if (group)  // else we got OOM while registering all radio buttons
			{
				group->UncheckOthers(doc, radio_elm);
			}
		}
	}
}

void FormValueRadioCheck::SetIsChecked(HTML_Element* he, BOOL is_checked, FramesDocument* doc /*= NULL*/, BOOL process_full_radiogroup /*= FALSE */, ES_Thread* interrupt_thread /*= NULL */)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(is_checked == FALSE || is_checked == TRUE);
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		form_object->SetFormWidgetValue(is_checked ? UNI_L("true") : UNI_L("false"));
		if (is_checked && he->GetInputType() == INPUT_RADIO)
		{
			SetIsInCheckedRadioGroup(TRUE);
			UncheckRestOfRadioGroup(he, form_object->GetDocument(), NULL);
			FormValueListener::HandleValueChanged(form_object->GetDocument(), he,
				FALSE, /* don't send onchange */
				FALSE, /* User origin... we don't know :-/ */
				NULL); // Thread
		}
		return;
	}

	// The code below is only for "hidden" form controls

	if (he->GetInputType() == INPUT_RADIO)
	{
		if (is_checked)
		{
			SET_IS_IN_CHECKED_GROUP(TRUE);
			if (process_full_radiogroup && doc)
			{
				UncheckRestOfRadioGroup(he, doc, interrupt_thread);
			}
		}

		const uni_char* group_name = he->GetName();
		if (group_name && // radio buttons without a name attribute has no group (but name="" creates a group)
			process_full_radiogroup && 
			!is_checked &&
			GET_IS_CHECKED())
		{
			FormRadioGroups* groups = doc->GetLogicalDocument()->GetRadioGroups().GetFormRadioGroupsForRadioButton(doc, he, FALSE);
			if (groups)
			{
				FormRadioGroup* group = groups->Get(group_name, FALSE);
				if (group)
				{
					if (!group->HasOtherChecked(he))
					{
						group->SetIsInCheckedGroup(doc, FALSE);
					}
				}
			}
		}
	}

	if (GET_IS_CHECKED() != is_checked)
	{
		SET_IS_CHECKED(is_checked);
		if (process_full_radiogroup && doc) // Otherwise someone else does it, and we don't have any doc anyway
		{
			FormValueListener::HandleValueChanged(doc, he, FALSE, FALSE, interrupt_thread);
		}
	}
}

BOOL FormValueRadioCheck::IsChecked(HTML_Element* he) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueRadioCheck*>(this));
	if (IsValueExternally())
	{
		// Ugly!
		OpString val_as_text;
		if (OpStatus::IsSuccess(he->GetFormObject()->GetFormWidgetValue(val_as_text)))
		{
			return val_as_text.CStr() != NULL;
		}
	}

	return GET_IS_CHECKED();
}

BOOL FormValueRadioCheck::IsCheckedByDefault(HTML_Element* he) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueRadioCheck*>(this));
	return he->GetBoolAttr(ATTR_CHECKED);
}

/* virtual */
BOOL FormValueRadioCheck::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}

	const uni_char* text_val = GET_IS_CHECKED() ? UNI_L("true") : UNI_L("false");

	form_object_to_seed->SetFormWidgetValue(text_val);

	return FALSE;
}

/* virtual */
void FormValueRadioCheck::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		BOOL checked = FALSE;
		// Ugly!
		OpString val_as_text;
		if (OpStatus::IsSuccess(form_object_to_replace->GetFormWidgetValue(val_as_text)))
		{
			// XXX Ugly!
			checked = val_as_text.CStr() != NULL;
		}

		SET_IS_CHECKED(checked);
		FormValue::Unexternalize(form_object_to_replace);
	}
	else
	{
		// We've been called twice for the same form_object. Just double check in debug
		// code that the value hasn't changed. It mustn't.
#ifdef _DEBUG
		// Ugly!
		OpString val_as_text;
		if (OpStatus::IsSuccess(form_object_to_replace->GetFormWidgetValue(val_as_text)))
		{
			// XXX Ugly!
			BOOL checked = val_as_text.CStr() != NULL;
			OP_ASSERT(GET_IS_CHECKED() ==checked);
		}
#endif // _DEBUG
	}
}

/* virtual */
FormValue* FormValueRadioCheck::Clone(HTML_Element *he)
{
	FormValueRadioCheck* clone = OP_NEW(FormValueRadioCheck, ());
	if (clone)
	{
		if (he && IsValueExternally())
		{
			BOOL checked = FALSE;
			// Ugly!
			OpString val_as_text;
			if (OpStatus::IsSuccess(he->GetFormObject()->GetFormWidgetValue(val_as_text)))
			{
				// XXX Ugly!
				checked = val_as_text.CStr() != NULL;
			}
			SET_IS_CHECKED(checked);
		}
		clone->GetIsCheckedBool() = GetIsCheckedBool();
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueRadioCheck::ResetToDefault(HTML_Element* he)
{
	ResetChangedFromOriginal();

	SetIsChecked(he, IsCheckedByDefault(he), NULL, FALSE); // Will have to fix this after the reset is processed
	return OpStatus::OK;
}

void FormValueRadioCheck::SaveStateBeforeOnClick(HTML_Element* he)
{
#ifdef SUPPORT_NESTED_CLICKS // The support for nested click handlers broke when people double clicked. See bugs 204317 and 211452.
	int saved_states = GET_ONCLICK_RESTORE_COUNT();
	if (saved_states == 0)
#endif // SUPPORT_NESTED_CLICKS
	{
		SET_IS_CHECKED_BEFORE_ONCLICK(IsChecked(he));
	}
#ifdef SUPPORT_NESTED_CLICKS
	else if (IS_ONCLICK_RESTORE_COUNT_MAX(saved_states))
	{
		OP_ASSERT(!"Something here might break because of a deeply nested onclick event");
		return;
	}

	SET_ONCLICK_RESTORE_COUNT(saved_states+1);
#endif // SUPPORT_NESTED_CLICKS
}

BOOL FormValueRadioCheck::RestoreStateAfterOnClick(HTML_Element* he, BOOL onclick_cancelled)
{
#ifdef SUPPORT_NESTED_CLICKS // Don't enable this before bug 211433 and 211435 is fixed
	int saved_states = GET_ONCLICK_RESTORE_COUNT();
	if (saved_states == 0)
	{
		OP_ASSERT(FALSE); // Why wasn't the value saved?
		return !onclick_cancelled; // assumption
	}

	saved_states--;
	SET_ONCLICK_RESTORE_COUNT(saved_states);
#endif // SUPPORT_NESTED_CLICKS

	BOOL saved_value = GET_IS_CHECKED_BEFORE_ONCLICK();
	if (
#ifdef SUPPORT_NESTED_CLICKS // Don't enable this before bug 211433 and 211435 is fixed
		saved_states == 0 && 
#endif // SUPPORT_NESTED_CLICKS
		onclick_cancelled)
	{
		SetIsChecked(he, saved_value, NULL, FALSE);
		return FALSE;
	}

	return IsChecked(he) != saved_value;
}

/* static */
OP_STATUS FormValueRadioCheck::UpdateFreeButtonsCheckedInGroupFlags(FramesDocument* frames_doc)
{
	OP_ASSERT(frames_doc);

	// Two passes. One to check which groups are checked, and one to set the flags
	OpStringHashTable<uni_char> names_with_check;
	for (int mode = 0; mode < 2; mode++)
	{
		HTML_Element* elm = frames_doc->GetDocRoot();
		while (elm)
		{
			if (elm->IsMatchingType(HE_INPUT, NS_HTML) &&
				elm->GetInputType() == INPUT_RADIO &&
				FormManager::FindFormElm(frames_doc, elm) == NULL)
			{
				RETURN_IF_ERROR(UpdateCheckedInGroupFlagsHelper(frames_doc, elm, names_with_check, mode));
			}
			elm = static_cast<HTML_Element*>(elm->NextActual());
		}
	}

	return OpStatus::OK;
}

/* static */
OP_STATUS FormValueRadioCheck::UpdateCheckedInGroupFlags(FramesDocument* frames_doc, HTML_Element* form_element)
{
	OP_ASSERT(frames_doc);

	// Two passes. One to check which groups are checked, and one to set the flags
	OpStringHashTable<uni_char> names_with_check;
	for (int mode = 0; mode < 2; mode++)
	{
		FormIterator form_iterator(frames_doc, form_element);
		while (HTML_Element* fhe = form_iterator.GetNext())
		{
			RETURN_IF_ERROR(UpdateCheckedInGroupFlagsHelper(frames_doc, fhe, names_with_check, mode));
		}
	}

	return OpStatus::OK;
}

/* private static */
OP_STATUS FormValueRadioCheck::UpdateCheckedInGroupFlagsHelper(FramesDocument* frames_doc, HTML_Element* fhe,
														 OpStringHashTable<uni_char>& names_with_check,
														 int mode)
{
	if (fhe->IsMatchingType(HE_INPUT, NS_HTML) &&
		fhe->GetInputType() == INPUT_RADIO)
	{
		const uni_char* name = fhe->GetName();
		if (name)
		{
			FormValueRadioCheck* radio_value =
				FormValueRadioCheck::GetAs(fhe->GetFormValue());
			if (mode == 0)
			{
				if (radio_value->IsChecked(fhe))
				{
					RETURN_IF_MEMORY_ERROR(names_with_check.Add(name, NULL));
				}
			}
			else
			{
				OP_ASSERT(mode == 1);
				uni_char* dummy;
				BOOL is_in_checked_group = OpStatus::IsSuccess(names_with_check.GetData(name, &dummy));
				if (radio_value->IsInCheckedRadioGroup() != is_in_checked_group)
				{
					radio_value->SetIsInCheckedRadioGroup(is_in_checked_group);
					frames_doc->GetLogicalDocument()->GetLayoutWorkplace()->ApplyPropertyChanges(fhe, CSS_PSEUDO_CLASS_GROUP_FORM, TRUE);
				}
			}
		}
	}

	return OpStatus::OK;
}

BOOL FormValueRadioCheck::IsCheckedAttrChangingState(FramesDocument* frames_doc, HTML_Element* form_element) const
{
	OP_ASSERT(form_element);
	OP_ASSERT(form_element->GetFormValue() == const_cast<FormValueRadioCheck*>(this));
	OP_ASSERT(frames_doc);

	const uni_char* name = form_element->GetName();
	if (name && form_element->GetInputType() == INPUT_RADIO)
	{
		DocumentRadioGroups& docgroups = frames_doc->GetLogicalDocument()->GetRadioGroups();
		if (FormRadioGroups* groups = docgroups.GetFormRadioGroupsForRadioButton(frames_doc, form_element, FALSE))
		{
			if (FormRadioGroup* group = groups->Get(name, FALSE))
			{
				return !group->HasBeenExplicitlyChecked();
			}
		}
	}

	return !IsChangedFromOriginalByUser(form_element) &&
		!GET_IS_CHANGED_FROM_CHECKED_PROPERTY();
}

void FormValueRadioCheck::SetWasChangedExplicitly(FramesDocument* frames_doc, HTML_Element* form_element)
{
	OP_ASSERT(form_element);
	OP_ASSERT(form_element->GetFormValue() == const_cast<FormValueRadioCheck*>(this));
	OP_ASSERT(frames_doc);

	SET_IS_CHANGED_FROM_CHECKED_PROPERTY(TRUE);

	const uni_char* name = form_element->GetName();
	if (!name || form_element->GetInputType() == INPUT_CHECKBOX)
	{
		return;
	}

	DocumentRadioGroups& docgroups = frames_doc->GetLogicalDocument()->GetRadioGroups();
	if (FormRadioGroups* groups = docgroups.GetFormRadioGroupsForRadioButton(frames_doc, form_element, FALSE))
	{
		if (FormRadioGroup* group = groups->Get(name, FALSE))
		{
			group->SetHasBeenExplicitlyChecked();
		}
	}
}
