/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvaluelist.h"

#include "modules/forms/formsenum.h" // FORMS_ATTR_OPTION_SELECTED
#include "modules/logdoc/htm_elm.h" // HTML_Element
#include "modules/forms/piforms.h" // SelectionObject
#include "modules/layout/box/box.h" // SelectionObject
#include "modules/layout/content/content.h" // SelectContent

/* virtual */
FormValue* FormValueList::Clone(HTML_Element *he)
{
	FormValueList* clone = OP_NEW(FormValueList, ());
	if (clone)
	{
		clone->m_listbox_scroll_pos = m_listbox_scroll_pos;
#ifdef CACHE_SELECTED_FIELD
		clone->m_selected_index  = -1;
		clone->m_selected_option = NULL;
#endif // CACHE_SELECTED_FIELD
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/**
 * Helper class, iterating over only the HE_OPTIONS (and HE_OPTGROUPS) that are included in
 * a select's option list.
 */
class OptionIterator
{
	HTML_Element* m_stop;
	HTML_Element* m_current;
	BOOL m_include_optgroups;
public:
	OptionIterator(HTML_Element* select, BOOL include_optgroups = FALSE)
		: m_current(select),
		  m_include_optgroups(include_optgroups)
	{
		OP_ASSERT(select);
		OP_ASSERT(select->IsMatchingType(HE_SELECT, NS_HTML));
		m_stop = static_cast<HTML_Element*>(select->NextSibling());
	}

	HTML_Element* GetNext()
	{
		OP_ASSERT(m_current);
		OP_ASSERT(m_current->IsMatchingType(HE_SELECT, NS_HTML) ||
		          m_current->IsMatchingType(HE_OPTION, NS_HTML) ||
		          m_current->IsMatchingType(HE_OPTGROUP, NS_HTML));
		if (m_current->IsMatchingType(HE_SELECT, NS_HTML))
		{
			// First call.
			if (!m_current->FirstChild())
			{
				m_current = NULL;
				return NULL;
			}
			m_current = m_current->FirstChild();
			if (m_current->IsMatchingType(HE_OPTION, NS_HTML) ||
			    m_include_optgroups && m_current->IsMatchingType(HE_OPTGROUP, NS_HTML))
			{
				return m_current;
			}
		}

		do
		{
			if (m_current->IsMatchingType(HE_OPTGROUP, NS_HTML))
			{
				m_current = static_cast<HTML_Element*>(m_current->Next());
			}
			else
			{
				m_current = static_cast<HTML_Element*>(m_current->NextSibling());
			}
		}
		while (m_current != m_stop &&
		       !m_current->IsMatchingType(HE_OPTION, NS_HTML) &&
		       !(m_include_optgroups && m_current->IsMatchingType(HE_OPTGROUP, NS_HTML)));

		if (m_current == m_stop)
		{
			m_current = NULL;
		}
		return m_current;
	}

	void JumpTo(HTML_Element* option)
	{
		OP_ASSERT(option);
		OP_ASSERT(option->IsMatchingType(HE_OPTION, NS_HTML));
		m_current = option;
	}
};

void FormValueList::SyncWithFormObject(HTML_Element* he)
{
	OP_ASSERT(he->Type() == HE_SELECT); // Not HE_KEYGEN
	FormObject* form_object = he->GetFormObject();
	OP_ASSERT(form_object); // Who triggered this (ONCHANGE) if there was no FormObject???
	SelectionObject* sel = static_cast<SelectionObject*>(form_object);

	int sel_count = sel->GetElementCount();
	OP_ASSERT(sel_count >= 0);
	unsigned int count = static_cast<unsigned int>(sel_count);

#ifdef CACHE_SELECTED_FIELD
	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);
#endif // CACHE_SELECTED_FIELD

	OptionIterator iterator(he);
	unsigned int current_index = 0;

	int last_selected  = -1;
#ifdef CACHE_SELECTED_FIELD
	int first_selected = -1;
	HTML_Element* first_selected_option = NULL;
	HTML_Element* last_selected_option = NULL;
#endif // CACHE_SELECTED_FIELD

	while (HTML_Element* opt_he = iterator.GetNext())
	{
		// If there are more HE_OPTION:s than elements in the SelectionObject, we set the
		// rest to be not selected
#ifdef CACHE_SELECTED_FIELD
		if (current_index >= count)
		{
			SetOptionSelected(opt_he, FALSE);
		}
		else
		{
			BOOL selected = sel->IsSelected(current_index);
			if (selected && first_selected < 0)
			{
				first_selected = current_index;
				first_selected_option = opt_he;
			}
			if (selected)
			{
				last_selected = current_index;
				last_selected_option = opt_he;
			}
			SetOptionSelected(opt_he, selected);
		}
#else
		BOOL option_selected = current_index < count && sel->IsSelected(current_index);
		if (option_selected)
		{
			last_selected = current_index;
		}
		SetOptionSelected(opt_he, option_selected);
#endif // CACHE_SELECTED_FIELD
		current_index++;
	}

#ifdef CACHE_SELECTED_FIELD
	if (is_multiple_select)
	{
		m_selected_index  = first_selected;
		m_selected_option = first_selected_option;
	}
	else
	{
		m_selected_index  = last_selected;
		m_selected_option = last_selected_option;
	}
#endif // CACHE_SELECTED_FIELD

	unsigned char &is_not_selected = GetIsExplicitNotSelected();
	if (last_selected >= 0 && is_not_selected)
	{
		is_not_selected = FALSE;
	}

	// If this fires the FormObject didn't match the HTML tree.
	// Maybe something changed the tree and we failed to update the widget?
	OP_ASSERT(count == current_index);
}

BOOL FormValueList::IsSelected(HTML_Element* he, unsigned int looked_at_index) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueList*>(this));
	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);

#ifdef CACHE_SELECTED_FIELD
	// If for cached selection _or_ there is another one for dropdowns, go with it. Otherwise, fall through to scanning.
	if (m_selected_index == looked_at_index || (is_dropdown && m_selected_index >= 0))
	{
		return (m_selected_index == looked_at_index);
	}
#endif // CACHE_SELECTED_FIELD

	OptionIterator iterator(he);
	unsigned int current_index = 0;
	while (HTML_Element* option_he = iterator.GetNext())
	{
		if (current_index == looked_at_index)
		{
			if (IsThisOptionSelected(option_he))
			{
				return TRUE;
			}

			// Not selected, but can option at matching index be defaulted as the selected option?
			// Definitely not iff:
			//  - a multi-selection.
			//  - index > 0 for a dropdown
			//  - we know that we aren't selected.
			if (!is_dropdown || looked_at_index > 0 || GetIsExplicitNotSelected())
			{
				return FALSE;
			}
		}
		if (looked_at_index == 0)
		{
			// This is selected -> The first option cannot be automatically selected
			if (IsThisOptionSelected(option_he))
			{
				return FALSE;
			}
		}
		current_index++;
	}

	if (current_index > looked_at_index)
	{
		OP_ASSERT(is_dropdown);
		OP_ASSERT(looked_at_index == 0);
		// We looked for a later selection but didn't find one -> The first must be selected automatically
		return TRUE;
	}

	// The index was too large
	return FALSE; // OpStatus::ERR?
}

#ifdef WAND_SUPPORT
BOOL FormValueList::IsListChangedFromOriginal(HTML_Element* he) const
{
	OptionIterator iterator(he);
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		BOOL selected_in_html = he_opt->GetSelected();
		BOOL selected_for_user = IsThisOptionSelected(he_opt);
		if (selected_in_html != selected_for_user)
		{
			return TRUE;
		}
	}
	return FALSE;
}
#endif // WAND_SUPPORT

static SelectContent* GetSelectContent(HTML_Element* select)
{
	Box* sel_box = select->GetLayoutBox();
	Content* content = sel_box ? sel_box->GetContent() : NULL;
	SelectContent* sel_content = content ? content->GetSelectContent() : NULL;
	if (!sel_content)
	{
		// Not a real select in the layout - generated content? Since we
		// need the "real" SelectContent we have to find the element it's stored at.
		// As soon as we skip past the layout engine when updating the widget all this
		// can be removed.
		HTML_Element* child = select->FirstChild();
		while (child && !sel_content)
		{
			if (child->GetInserted() == HE_INSERTED_BY_LAYOUT && child->Type() == select->Type())
			{
				sel_content = GetSelectContent(child);
			}
			child = child->Suc();
		}
	}

	return sel_content;
}
/* private */
void FormValueList::EmptyWidget(HTML_Element* he)
{
	OP_ASSERT(IsValueExternally());

	SelectContent* sel_content = GetSelectContent(he);
	if (!sel_content)
	{
		// Not a real select in the layout
		return;
	}

	SelectionObject* sel_obj = static_cast<SelectionObject*>(he->GetFormObject());
	sel_content->RemoveOptions();
	int options_count = sel_obj->GetFullElementCount();
	OP_ASSERT(options_count >= 0);
	if (options_count > 0)
	{
		// If we end up here, we probably have thrown away the old SelectContent
		// and need to remove the elements from SelectionObject here instead
		sel_obj->RemoveAll();
	}

	OP_ASSERT(sel_obj->GetElementCount() == 0);
	OP_ASSERT(sel_obj->GetFullElementCount() == 0);
}


void FormValueList::HandleOptionListChanged(FramesDocument* frames_doc, HTML_Element* select_he, HTML_Element* option_he, BOOL added)
{
	if (IsValueExternally()) // else it will be fixed when we are externalized
	{
		BOOL need_full_rebuild = FALSE;
		if (option_he->Type() == HE_OPTION)
		{
			// These are not handled by the layout engine:
			// - inserting first into an optgroup or
			// - inserting right before an optgroup.
			// (ignoring text nodes in both cases.)
			if (added)
			{
				HTML_Element *pred = option_he->PredActual();
				while (pred && pred->IsText())
				{
					pred = pred->PredActual();
				}

				if (option_he->ParentActual() && option_he->ParentActual()->IsMatchingType(HE_OPTGROUP, NS_HTML))
				{
					HTML_Element *iter = NULL;
					if (pred)
					{
						iter = option_he->ParentActual()->SucActual();
						while (iter && iter->IsText())
						{
							iter = iter->SucActual();
						}
					}
					if (!pred || iter && iter->IsMatchingType(HE_OPTGROUP, NS_HTML))
					{
						need_full_rebuild = TRUE;
					}
				}
				else if (HTML_Element *iter = option_he->SucActual())
				{
					while (iter && iter->IsText())
					{
						iter = iter->SucActual();
					}
					if (iter && iter->IsMatchingType(HE_OPTGROUP, NS_HTML))
					{
						need_full_rebuild = TRUE;
					}
				}
			}
		}
		else
		{
			OP_ASSERT(option_he->Type() == HE_OPTGROUP);
			if (added)
			{
				need_full_rebuild = TRUE;
			}
			else
			{
				// This is somehow handled by the layout engine yet
			}
		}

		if (need_full_rebuild)
		{
			OP_ASSERT(select_he->GetFormObject()); // Otherwise we're not external
			// Rebuild the widget from the beginning. Yes, this sucks.
			select_he->RemoveLayoutBox(frames_doc, TRUE);
			select_he->MarkExtraDirty(frames_doc);
		}
	}

	HTML_Element *selected_option = NULL;
	unsigned int selected_index = 0;
	BOOL has_selection = FALSE;

#ifdef CACHE_SELECTED_FIELD
	has_selection = m_selected_index >= 0;
	selected_index = m_selected_index;
	selected_option = m_selected_option;
#endif // CACHE_SELECTED_FIELD

	has_selection = has_selection || OpStatus::IsSuccess(GetIndexAndElementOfFirstSelected(select_he, selected_option, selected_index, TRUE));

	/* Adjust the selection as add/remove had an impact on the selected index
	 * (e.g. if selected element was removed a default one should be selected,
	 * or if selected option was added to a single selection <select> - make sure it's the only one selected)
	 */
	if (!has_selection || IsThisOptionSelected(option_he))
	{
		if (!select_he->GetBoolAttr(ATTR_MULTIPLE))
		{
			SetInitialSelection(select_he, FALSE);
		}
	}
	// If we added/removed an element before the current selection, adjust selected index.
	else if (has_selection)
	{
		int index = static_cast<int>(selected_index);
		HTML_Element *iter = selected_option;

		// Avoid doing a full linear scan by approximating: if near the start, see if before.
		if (index <= 6)
		{
			while (iter && index-- >= 0)
			{
				if (option_he == iter)
				{
					// Before; adjust selected index (and cached info, if any.)
					OpStatus::Ignore(SetInitialSelection(select_he, FALSE));
					return;
				}
				iter = iter->Pred();
			}
			// Known not to occur before current selection; update unnecessary.
		}
		else
		{
			// Look beyond current selection..a bit; if found, selection and index is still valid. Otherwise, assume it occurred before.
			index = 6;
			while (iter && index-- >= 0)
			{
				if (option_he == iter)
				{
					return;
				}
				iter = iter->Suc();
			}

			OpStatus::Ignore(SetInitialSelection(select_he, FALSE));
		}
	}
}

void FormValueList::LockUpdate(HTML_Element* helm, BOOL is_externalizing, BOOL lock)
{
	m_update_locked = lock;
	if (!m_update_locked)
	{
		if (m_update_attempt_when_locked)
			UpdateWidget(helm, is_externalizing);
		m_update_attempt_when_locked = FALSE;
	}
}

/* private */
BOOL FormValueList::UpdateWidget(HTML_Element* he, BOOL is_externalizing)
{
	if (m_update_locked)
	{
		m_update_attempt_when_locked = TRUE;
		return FALSE;
	}

	OP_ASSERT(he->Type() == HE_SELECT);

	SelectionObject *sel = (SelectionObject*)he->GetFormObject();
	OP_ASSERT(sel); // Otherwise we're not external

	SelectContent* sel_content = GetSelectContent(he);
	if (!sel_content)
	{
		// Not a real select
		return FALSE;
	}

	BOOL was_impossible;
	// First try an simple fast sync
	BOOL changes_done = SynchronizeWidgetWithTree(he, sel, sel_content, is_externalizing, was_impossible);
	if (was_impossible)
	{
		m_rebuild_needed = TRUE;
		sel->ScheduleFormValueRebuild();
	}

	return changes_done;
}

void FormValueList::Rebuild(HTML_Element* he, BOOL is_externalizing)
{
	if (!m_rebuild_needed || !IsValueExternally())
		return;

	m_rebuild_needed = FALSE;

	OP_ASSERT(he->Type() == HE_SELECT);

	SelectionObject *sel = static_cast<SelectionObject*>(he->GetFormObject());

	SelectContent* sel_content = GetSelectContent(he);
	if (!sel_content)
	{
		// Not a real select
		return;
	}

	sel->LockWidgetUpdate(TRUE);
	// Rebuild the widget from the beginning. Yes, this sucks.
	EmptyWidget(he);

#ifdef _DEBUG
	OP_ASSERT(sel->GetElementCount() == 0);
	OP_ASSERT(sel->GetFullElementCount() == 0);
#endif // _DEBUG

	BOOL was_impossible;
	SynchronizeWidgetWithTree(he, sel, sel_content, is_externalizing, was_impossible);
	OP_ASSERT(!was_impossible || !"Still out of sync. Caused by a bug or OOM");

	sel->LockWidgetUpdate(FALSE);
	sel->UpdateWidget();

	if (!is_externalizing)
	{
		// Since we had to redo the whole widget, we must also ensure that we layout every option
		// This must not be done if we're inside the layout engine already (thus the check for
		// is_externalizing). See bug 273923.
		HTML_Element* stop_marker = static_cast<HTML_Element*>(he->NextSibling());
		HTML_Element* opt_he = static_cast<HTML_Element*>(he->Next());
		while (opt_he != stop_marker)
		{
			opt_he->MarkExtraDirty(sel->GetDocument());
			opt_he = opt_he->Next();
		}
	}
}

/* private */
BOOL FormValueList::SynchronizeWidgetWithTree(HTML_Element* he, SelectionObject* sel, SelectContent* sel_content, BOOL is_externalizing, BOOL& out_was_impossible)
{
	OP_ASSERT(IsValueExternally());

	out_was_impossible = FALSE;

	// Do we know a better way than looping over the options
	// and update the widget one by one?

	unsigned int widget_option_count;
	unsigned int widget_element_count; // Includes optgroup starts and ends

	{
		int count = sel->GetElementCount();
		OP_ASSERT(count >= 0);
		widget_option_count = static_cast<unsigned int>(count);
		widget_element_count = sel->GetFullElementCount();
	}

	OP_ASSERT(he->Type() == HE_SELECT);

	BOOL need_full_rebuild = FALSE;
	BOOL changed_widget_structure = FALSE;
	unsigned int current_option_index = 0;
	unsigned int current_element_index = 0; // includes optgroup starts and ends
	unsigned int selected_count = 0;
	int first_enabled_option = -1;
	OptionIterator iterator(he, TRUE); // TRUE = Include HE_OPTGROUP
	OpVector<HTML_Element> opt_groups;
	while (HTML_Element* opt_he = iterator.GetNext())
	{
		// Close any optgroups we have left
		while (opt_groups.GetCount() > 0)
		{
			HTML_Element* innermost_opt_group = opt_groups.Get(opt_groups.GetCount() - 1);
			if (innermost_opt_group->IsAncestorOf(opt_he))
			{
				// We are still inside the optgroup
				break;
			}

			BOOL need_to_insert_optgroup_stop = current_element_index >= widget_element_count ||
				!sel->ElementAtRealIndexIsOptGroupStop(current_element_index);
			if (need_to_insert_optgroup_stop)
			{
				if (current_element_index < widget_element_count)
				{
					// Can only insert groups at the end of the widget
					need_full_rebuild = TRUE;
					break;
				}
				changed_widget_structure = TRUE;
#if 0
				sel->EndGroup();
#else
				// Need to talk through layout right now or it will get confused
				sel_content->EndOptionGroup(opt_he);
#endif // 0
				widget_element_count++;
			}

			current_element_index++;

			opt_groups.Remove(opt_groups.GetCount() - 1);
		}

		if (opt_he->Type() == HE_OPTGROUP)
		{
			// Entering a optgroup
			if (OpStatus::IsSuccess(opt_groups.Add(opt_he)))
			{
				BOOL need_to_insert_optgroup_start = current_element_index >= widget_element_count ||
					!sel->ElementAtRealIndexIsOptGroupStart(current_element_index);
				if (need_to_insert_optgroup_start)
				{
					if (current_element_index < widget_element_count)
					{
						// Can only insert groups at the end of the widget
						need_full_rebuild = TRUE;
						break;
					}
					changed_widget_structure = TRUE;
#if 0
					sel->BeginGroup(opt_he);
#else
					// Need to talk through layout right now or it will get confused
					sel_content->BeginOptionGroup(opt_he);
#endif // 0
					widget_element_count++;
				}
			}
			// else (OOM) we will miss the optgroup. Not too serious effect of the oom.

			current_element_index++;
		}
		else
		{
			OP_ASSERT(opt_he->IsMatchingType(HE_OPTION, NS_HTML));
			BOOL selected = IsThisOptionSelected(opt_he);
			if (selected)
			{
				selected_count++;
			}
			if (current_option_index < widget_option_count)
			{
				if (sel->IsSelected(current_option_index) != selected)
				{
					sel->SetElement(current_option_index, selected);
				}
			}
			else
			{
				// This option is not added to the listbox yet. Normally
				// done by the layout engine, but then we do it here instead.
				// Hopefully the actual string will be set by the layout engine when the
				// options is processed there. It should be since it's always been that
				// way.
				changed_widget_structure = TRUE;

				// Need to add through the layout box since it has own
				// datastructures and would get confused if we bypassed it.
				// (see bug 217347)
				unsigned index;
				if (OpStatus::IsError(sel_content->AddOption(opt_he, index)))
				{
					// OOM or other error (exhausted capacity to hold more options);
					// empty and rebuild.
					need_full_rebuild = TRUE;
					break;
				}

				widget_option_count++;
				widget_element_count++;
				if (static_cast<unsigned>(sel->GetElementCount()) != widget_option_count)
				{
					// the new option wasn't inserted, probably because
					// it had already been used at a different position earlier or
					// that the widget was locked (should the lock be
					// removed now with FormValueList?)

					// So now we have to do a full rebuild.
					need_full_rebuild = TRUE;
					break;
				}
			}

			if (first_enabled_option == -1 && !opt_he->GetDisabled())
			{
				first_enabled_option = current_option_index;
			}

			// Verify option text? A change to the text should cause a reflow which should solve
			// all unsyncness
			current_option_index++;
			current_element_index++;
		}
	}

	if (need_full_rebuild)
	{
		out_was_impossible = TRUE;
		return TRUE;
	}

	// Remove options in the widget that are not in the tree
	while (current_option_index < widget_option_count)
	{
		sel->RemoveElement(--widget_option_count);
	}

	// If it's a dropdown and nothing else was selected,
	// then the first enabled value is
	// automatically selected
	if (GetIsExplicitNotSelected())
	{
		// Setting the option "-1" will make the widget understand that it
		// is completely unselected as well. Otherwise it will during
		// history walks reset itself to "first option".
		sel->SetElement(-1, TRUE);
	}
	else if (selected_count == 0 &&
		widget_option_count > 0 &&
		first_enabled_option != -1)
	{
		BOOL is_multiple_select, is_dropdown;
		IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);

		if (is_dropdown)
		{
			if (he->GetEndTagFound())
			{
				// Must have a real selection
				SelectValueInternal(he, first_enabled_option, TRUE, is_externalizing);
			}
			sel->SetElement(first_enabled_option, TRUE);
		}
	}

	sel->ScrollIfNeeded();

	return changed_widget_structure;
}


BOOL FormValueList::IsSelectedElement(HTML_Element* he, HTML_Element* option_elm) const
{
	OP_ASSERT(he->GetFormValue() == const_cast<FormValueList*>(this));
	OP_ASSERT(he->IsAncestorOf(option_elm));

#ifdef CACHE_SELECTED_FIELD
	if (option_elm == m_selected_option)
		return TRUE;
#endif // CACHE_SELECTED_FIELD

	BOOL selected = IsThisOptionSelected(option_elm);
	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);

	// If it's a dropdown and nothing else is selected, we select the first so
	// we must see if we're selected in that way.
	if (!selected && is_dropdown && !option_elm->GetDisabled())
	{
#ifdef CACHE_SELECTED_FIELD
		// element not selected, but another is known to be.
		if (m_selected_index >= 0)
			return FALSE;
#endif // CACHE_SELECTED_FIELD

		// Look if we're first
		HTML_Element* earlier_he = static_cast<HTML_Element*>(option_elm->Prev());
		while (earlier_he != he)	// We must pass the select on our way backwards
		{
			if (earlier_he->IsMatchingType(HE_OPTION, NS_HTML))
			{
				HTML_Element* parent = earlier_he->Parent();
				while (parent != he && parent->IsMatchingType(HE_OPTGROUP, NS_HTML))
				{
					parent = parent->Parent();
				}
				if (parent == he)
				{
					// We weren't the first option -> Not selected
					return FALSE;
				}
			}
			earlier_he = static_cast<HTML_Element*>(earlier_he->Prev());
		}

		// We were the first OPTION. See if any other is selected.
		OptionIterator iterator(he);
		iterator.JumpTo(option_elm);
		while (HTML_Element* later_he = iterator.GetNext())
		{
			if (IsThisOptionSelected(later_he))
			{
				// Later selected -> We're not selected
				return FALSE;
			}
		}

		// Found no later selected, and we're the first option in a dropdown
		// so we must be automatically selected
		selected = TRUE;
	}

	return selected;
}

/* virtual */
OP_STATUS FormValueList::ResetToDefault(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	ResetChangedFromOriginal();

	return SetInitialSelection(he, TRUE);
}

OP_STATUS FormValueList::SetInitialSelection(HTML_Element* he, BOOL use_selected_attribute)
{
	OP_ASSERT(he->GetFormValue() == this);
//	OP_ASSERT(he->GetEndTagFound()); // This is called right before the flag is set

	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);
	BOOL must_have_single_selection = is_dropdown;

	// If we have to fallback on something ourself, we will use options in this order:
	// 1. first_enabled_option
	HTML_Element* first_enabled_option = NULL;
	unsigned int selection_count = 0;

#ifdef CACHE_SELECTED_FIELD
	int option_count         = 0;
	int first_selected_index = -1;
#endif
	OptionIterator iterator(he);
	HTML_Element* last_selected_option = NULL; // Used to handle multiple ATTR_SELECTED
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		BOOL option_disabled = he_opt->GetDisabled(); // We don't care about inherited disabling here.
		if (!first_enabled_option && !option_disabled)
		{
			first_enabled_option = he_opt;
#ifdef CACHE_SELECTED_FIELD
			first_selected_index = option_count;
#endif // CACHE_SELECTED_FIELD
		}

		BOOL select_option;
		if (use_selected_attribute)
		{
			// Only look at the selected attribute
			select_option = he_opt->GetSelected();
		}
		else
		{
			select_option = IsThisOptionSelected(he_opt);
		}

		if (select_option)
		{
			selection_count++;
			GetIsExplicitNotSelected() = FALSE;
		}
		SetOptionSelected(he_opt, select_option);
		if (select_option && !is_multiple_select)
		{
			if (last_selected_option)
			{
				// Only the last of the elements marked with
				// ATTR_SELECTED should be select
				SetOptionSelected(last_selected_option, FALSE);
			}
			last_selected_option = he_opt;
#ifdef CACHE_SELECTED_FIELD
			m_selected_index  = option_count;
			m_selected_option = he_opt;
		}
		option_count++;
#else
		}
#endif // CACHE_SELECTED_FIELD
	}

	// Fallback to a calculated selection
	if (selection_count == 0 && must_have_single_selection)
	{
		if (first_enabled_option)
		{
			GetIsExplicitNotSelected() = FALSE;
			SetOptionSelected(first_enabled_option, TRUE);
		}
#ifdef CACHE_SELECTED_FIELD
		m_selected_index  = first_selected_index;
		m_selected_option = first_enabled_option;
	}
	else if (selection_count == 0)
	{
		// If the sole selection in a multi-select was removed, drop its cached selection.
		m_selected_index  = -1;
		m_selected_option = NULL;
	}
#else
	}
#endif // CACHE_SELECTED_FIELD

	if (IsValueExternally())
	{
		// The widget needs to update too
		BOOL changed_structure = UpdateWidget(he, FALSE);
		if (changed_structure)
		{
			// Need to layout the options (color, text...)
			he->MarkDirty(he->GetFormObject()->GetDocument());
		}
	}

	return OpStatus::OK;
}

OP_STATUS FormValueList::SelectValueInternal(HTML_Element* he, unsigned int index, BOOL select, BOOL is_externalizing)
{
	OP_ASSERT(he->GetFormValue() == this);

	// Can we improve? A linear scan to count them followed by the same below.
	if (index >= GetOptionCount(he))
	{
		OP_ASSERT(FALSE); // XXX Count the options in advance!
		return OpStatus::ERR;
	}

#ifdef CACHE_SELECTED_FIELD
	HTML_Element* selected_element = NULL;
#endif // CACHE_SELECTED_FIELD

	OptionIterator iterator(he);
	BOOL is_single_selection = !he->GetMultiple();
	unsigned int option_number = 0;
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		if (select && is_single_selection || index == option_number)
		{
			BOOL select_option = (index == option_number) && select;
			SetOptionSelected(he_opt, select_option);
#ifdef CACHE_SELECTED_FIELD
			if (index == option_number)
				selected_element = he_opt;
#endif // CACHE_SELECTED_FIELD
		}
		option_number++;
	}
#ifdef CACHE_SELECTED_FIELD
	// Can we improve caching? Default to first-enabled on the fly or get at the selection set for multi-select.
	if (!select)
	{
		m_selected_index  = -1;
		m_selected_option = NULL;
	}
	else
	{
		m_selected_index  = index;
		m_selected_option = selected_element;
	}
#endif // CACHE_SELECTED_FIELD

	GetIsExplicitNotSelected() = FALSE;

	if (IsValueExternally() && !is_externalizing)
	{
		// The widget needs to update too
		UpdateWidget(he, FALSE);
	}

	return OpStatus::OK;
}

unsigned int FormValueList::GetOptionCount(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(he->Type() == HE_SELECT);
	return GetOptionIndexOrCount(he, NULL); // NULL -> Get Count
}

unsigned int FormValueList::GetOptionIndexOrCount(HTML_Element* he, HTML_Element* option_he)
{
	// option_he == NULL -> Count
	unsigned int option_count = 0;
	OptionIterator iterator(he);
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		if (he_opt == option_he)
		{
			break;
		}
		option_count++;
	}

	return option_count;
}


/* static */
OP_STATUS FormValueList::ConstructFormValueList(HTML_Element* he, FormValue*& out_value)
{
	OP_ASSERT(he->Type() == HE_SELECT);
	FormValueList* list_value = OP_NEW(FormValueList, ());
	if (!list_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// No children exists yet so this value's internals are built lazily.

	out_value = list_value;
	return OpStatus::OK;
}

OP_STATUS FormValueList::GetOptionValues(HTML_Element* he, OpVector<OpString>& selected_values, BOOL include_not_selected)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(he->Type() == HE_SELECT);

	OptionIterator iterator(he);
	BOOL only_selected = !include_not_selected;
	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);
	BOOL must_have_one_selected = is_dropdown && !GetIsExplicitNotSelected();
	HTML_Element* first_option = NULL;
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		if (must_have_one_selected && !first_option)
		{
			first_option = he_opt;
		}

		if (include_not_selected || (IsThisOptionSelected(he_opt) && !he_opt->GetDisabled()))
		{
			RETURN_IF_ERROR(AddOptionValueOrClear(selected_values, he_opt, he));
		}
	}

	if (only_selected && must_have_one_selected && selected_values.GetCount() == 0 && first_option)
	{
		RETURN_IF_ERROR(AddOptionValueOrClear(selected_values, first_option, he));
	}

	return OpStatus::OK;
}

/* static */
void FormValueList::IsMultipleOrDropdown(HTML_Element* he, BOOL& is_multiple, BOOL& is_dropdown)
{
	OP_ASSERT(he->Type() == HE_SELECT);
	is_multiple = he->GetBoolAttr(ATTR_MULTIPLE);
	is_dropdown = !is_multiple && he->GetNumAttr(ATTR_SIZE) <= 1;
}

/* private static */
OP_STATUS FormValueList::AddOptionValueOrClear(OpVector<OpString>& selected_values, HTML_Element* he_opt, HTML_Element* he_sel)
{
	TempBuffer buffer;
	const uni_char* opt_val = GetValueOfOption(he_opt, &buffer);
	if (!opt_val)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	// String will be owned by caller
	OpString* selected_value = OP_NEW(OpString, ());
	if (!selected_value ||
		OpStatus::IsError(selected_value->Set(opt_val)))
	{
		OP_DELETE(selected_value);
		selected_values.DeleteAll();
		return OpStatus::ERR_NO_MEMORY;
	}

	selected_values.Add(selected_value);
	return OpStatus::OK;
}

BOOL FormValueList::HasSelectedValue(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);
	unsigned int dummy_index;

#ifdef CACHE_SELECTED_FIELD
	if (m_selected_index >= 0)
	{
		return TRUE;
	}
#endif // CACHE_SELECTED_FIELD
	return GetIndexOfFirstSelected(he, dummy_index) == OpStatus::OK;
}

#ifdef _WML_SUPPORT_
HTML_Element* FormValueList::GetFirstSelectedOption(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	HTML_Element* he_opt;
	unsigned int dummy_index;
	if (OpStatus::IsSuccess(GetIndexAndElementOfFirstSelected(he, he_opt, dummy_index, FALSE)))
	{
		OP_ASSERT(he_opt);
		return he_opt;
	}

	return NULL;
}
#endif // _WML_SUPPORT_

/* private */
OP_STATUS FormValueList::GetIndexAndElementOfFirstSelected(HTML_Element* sel_he, HTML_Element*& out_option, unsigned int& out_index, BOOL use_first_selected)
{
	OP_ASSERT(sel_he->GetFormValue() == this);

	out_option = sel_he->FirstChild();
	out_index = 0;
	HTML_Element* first_enabled_option = NULL;
	unsigned first_enabled_option_index = 0;
	HTML_Element* last_selected = NULL;
	unsigned int last_selected_index = 0;
	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(sel_he, is_multiple_select, is_dropdown);

	OptionIterator iterator(sel_he);
	while (HTML_Element* it_option = iterator.GetNext())
	{
		if (IsThisOptionSelected(it_option))
		{
			if (use_first_selected || is_multiple_select)
			{
				// First selected
				return OpStatus::OK;
			}
			// This is selected, but if there are more than one selected, then
			// the last one is the one really selected.
			last_selected = it_option;
			last_selected_index = out_index;
		}

		if (!first_enabled_option && !it_option->GetDisabled())
		{
			first_enabled_option = it_option;
			first_enabled_option_index = out_index;
		}
		out_index++;
	}

	if (last_selected)
	{
		out_option = last_selected;
		out_index = last_selected_index;
		return OpStatus::OK;
	}

	if (!GetIsExplicitNotSelected() && is_dropdown && first_enabled_option)
	{
		// If it's a dropdown and nothing else is selected
		// but it is non empty, we select the first
		out_index = first_enabled_option_index;
		out_option = first_enabled_option;
		return OpStatus::OK;
	}

	return OpStatus::ERR; // Nothing selected
}

/* static */
BOOL FormValueList::IsOptionSelected(HTML_Element* option)
{
	BOOL has_option_selected_attr = option->HasSpecialAttr(FORMS_ATTR_OPTION_SELECTED, SpecialNs::NS_FORMS);
	return has_option_selected_attr ? option->GetSpecialBoolAttr(FORMS_ATTR_OPTION_SELECTED, SpecialNs::NS_FORMS) : option->GetBoolAttr(ATTR_SELECTED);
}

BOOL FormValueList::IsThisOptionSelected(HTML_Element* option) const
{
#ifdef CACHE_SELECTED_FIELD
	if (m_selected_option == option)
	{
		return TRUE;
	}
	else
#endif // CACHE_SELECTED_FIELD
	{
		return IsOptionSelected(option);
	}
}

/* static */
const uni_char* FormValueList::GetValueOfOption(HTML_Element* he_opt, TempBuffer* buffer)
{
	OP_ASSERT(he_opt && he_opt->IsMatchingType(HE_OPTION, NS_HTML));
	OP_ASSERT(buffer && buffer->Length() == 0);

	const uni_char* opt_val = he_opt->GetValue();
	if (!opt_val)
	{
		if (OpStatus::IsSuccess(he_opt->GetOptionText(buffer)))
		{
			opt_val = buffer->GetStorage();
			OP_ASSERT(opt_val);
		}
	}
	return opt_val;
}

OP_STATUS FormValueList::UnselectAll(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(he->Type() == HE_SELECT);

	OptionIterator iterator(he);
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		SetOptionSelected(he_opt, FALSE);
	}

	GetIsExplicitNotSelected() = TRUE;

#ifdef CACHE_SELECTED_FIELD
	m_selected_index  = -1;
	m_selected_option = NULL;
#endif // CACHE_SELECTED_FIELD

	if (IsValueExternally())
	{
		// The widget needs to update too
		UpdateWidget(he, FALSE);
	}

	return OpStatus::OK;
}

OP_STATUS FormValueList::UpdateSelectedValue(HTML_Element* he, HTML_Element* he_opt)
{
	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);
	if (is_multiple_select)
	{
		SetOptionSelected(he_opt, TRUE);
		return SetInitialSelection(he, FALSE);
	}
	else
	{
		unsigned index;
		HTML_Element* he_opt_selected;
		OP_STATUS status = GetIndexAndElementOfFirstSelected(he, he_opt_selected, index);
		if (OpStatus::IsMemoryError(status))
		{
			return status;
		}
		else if (OpStatus::IsError(status))
		{
			SetOptionSelected(he_opt, TRUE);
			return SetInitialSelection(he, FALSE);
		}
		else if (he_opt != he_opt_selected)
		{
			SetOptionSelected(he_opt_selected, FALSE);
			SetOptionSelected(he_opt, TRUE);
		}

		return OpStatus::OK;
	}
}

/* static */
void FormValueList::SetOptionSelected(HTML_Element* he_opt, BOOL selected)
{
	// We save memory by not setting the attribute unless it's
	// already there or if it must be set to TRUE

	// Update: It was not possible to save that memory because if we don't mark things
	// explicitly we may when unsetting things default back to checked values instead
	// of getting nothing selected at all.
//	if (IsOptionSelected(he_opt) != selected)
//	{
		void* attr_val = reinterpret_cast<void*>(static_cast<INTPTR>(selected));
		he_opt->SetSpecialAttr(FORMS_ATTR_OPTION_SELECTED, ITEM_TYPE_BOOL, attr_val, FALSE, SpecialNs::NS_FORMS);
//	}
}

#if defined(WAND_SUPPORT) || defined(_WML_SUPPORT_)
OP_STATUS FormValueList::GetSelectedIndexes(HTML_Element* he, OpINT32Vector& selected_indexes)
{
	OP_ASSERT(he->GetFormValue() == this);
	OP_ASSERT(he->Type() == HE_SELECT);

	BOOL is_multiple_select, is_dropdown;
	IsMultipleOrDropdown(he, is_multiple_select, is_dropdown);
	if (is_multiple_select)
	{
		HTML_Element* dummy;
		unsigned int selected_index;
		if (OpStatus::IsSuccess(GetIndexAndElementOfFirstSelected(he, dummy, selected_index, FALSE)))
		{
			return selected_indexes.Add(static_cast<INT32>(selected_index));
		}

		return OpStatus::OK;
	}

	OptionIterator iterator(he);
	int option_index = 0;
	while (HTML_Element* he_opt = iterator.GetNext())
	{
		if (IsOptionSelected(he_opt))
		{
			selected_indexes.Add(option_index);
		}
		option_index++;
	}

	if (!GetIsExplicitNotSelected() && is_dropdown && selected_indexes.GetCount() == 0 && option_index > 0)
	{
		// First value is automatically selected
		selected_indexes.Add(0);
	}

	return OpStatus::OK;
}
#endif // defined(WAND_SUPPORT) || defined(_WML_SUPPORT_)

/* virtual */
BOOL FormValueList::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}

	OP_ASSERT(m_listbox_scroll_pos >= 0);
	SelectionObject* sel = static_cast<SelectionObject*>(form_object_to_seed);
	sel->SetWidgetScrollPosition(m_listbox_scroll_pos);
	if (m_rebuild_needed)
		Rebuild(form_object_to_seed->GetHTML_Element(), TRUE);
	else
		UpdateWidget(form_object_to_seed->GetHTML_Element(), TRUE);

	return TRUE;
}

/* virtual */
void FormValueList::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		m_listbox_scroll_pos = static_cast<SelectionObject*>(form_object_to_replace)->GetWidgetScrollPosition();
		OP_ASSERT(m_listbox_scroll_pos >= 0);
		FormValue::Unexternalize(form_object_to_replace);
	}
}
