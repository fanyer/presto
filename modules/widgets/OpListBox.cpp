/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/forms/piforms.h"

# include "modules/display/vis_dev.h"
# include "modules/display/coreview/coreview.h"

#ifdef GRAB_AND_SCROLL
# include "modules/display/VisDevListeners.h"
#endif

#ifdef WIDGETS_IMS_SUPPORT
# include "modules/widgets/OpIMSObject.h"
#endif // WIDGETS_IMS_SUPPORT

#ifdef NEARBY_ELEMENT_DETECTION
# include "modules/widgets/finger_touch/element_expander.h"
#endif // NEARBY_ELEMENT_DETECTION

// == OpItemSearch ==========================================================

#define INLINE_ITEMSEARCH_DELAY 600

OP_STATUS OpItemSearch::PressKey(const uni_char *key_value)
{
	double time = g_op_time_info->GetWallClockMS();
	if (time > m_last_keypress_time + INLINE_ITEMSEARCH_DELAY)
		string.Empty();

	OP_STATUS status = string.Append(key_value);

	m_last_keypress_time = time;
	return status;
}

BOOL OpItemSearch::IsSearching()
{
	double time = g_op_time_info->GetWallClockMS();
	if (time < m_last_keypress_time + INLINE_ITEMSEARCH_DELAY && !string.IsEmpty())
		return TRUE;
	return FALSE;
}

const uni_char* OpItemSearch::GetSearchString()
{
	return string.IsEmpty() ? UNI_L("") : string.CStr();
}

#ifdef WIDGETS_IME_ITEM_SEARCH
void OpItemSearch::Clear()
{
	string.Empty();
}
#endif // WIDGETS_IME_ITEM_SEARCH

// == OpStringItem ===========================================================

OpStringItem::OpStringItem(BOOL filtered)
	: OpFilteredVectorItem<OpStringItem>(filtered)
{
	user_data = 0;
	packed.is_enabled = TRUE;
	packed.is_selected = FALSE;
	packed.is_group_start = FALSE;
	packed.is_group_stop = FALSE;
	packed.is_seperator = FALSE;
	string2 = NULL;
	fg_col = USE_DEFAULT_COLOR;
	bg_col = USE_DEFAULT_COLOR;
#ifdef SKIN_SUPPORT
	m_indent = 0;
#endif
}

OpStringItem::~OpStringItem()
{
	OP_DELETE(string2);
}

OP_STATUS OpStringItem::SetString(const uni_char* string, OpWidget* widget)
{
	return this->string.Set(string, widget);
}

void OpStringItem::AddItemMargin(OpWidgetInfo* info, OpWidget *widget, OpRect &rect)
{
	INT32 margin_left, margin_top, margin_right, margin_bottom;
	info->GetItemMargin(margin_left, margin_top, margin_right, margin_bottom);
	margin_left = MAX(margin_left, widget->GetPaddingLeft());
	rect.x += margin_left;
	rect.y += margin_top;
	rect.width -= margin_left + margin_right;
	rect.height -= margin_top + margin_bottom;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OpAccessibleItem* OpStringItem::GetAccessibleParent()
{
	if (!m_list_item_listener)
		return NULL;
	return m_list_item_listener->GetAccessibleParentOfItem();
}

OpAccessibleItem* OpStringItem::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect r;
	AccessibilityGetAbsolutePosition(r);
	if ((x >= r.Left()) && (y >= r.Top()) && (x < r.Right()) && (y < r.Bottom()))
		return this;
	return NULL;
}

OpAccessibleItem* OpStringItem::GetAccessibleFocusedChildOrSelf()
{
	if (GetAccessibleParent()->GetAccessibleFocusedChildOrSelf() == this)
		return this;
	return NULL;
}

OpAccessibleItem* OpStringItem::GetNextAccessibleSibling()
{
	return GetAccessibleParent()->GetAccessibleChild(GetAccessibleParent()->GetAccessibleChildIndex(this) + 1);
}

OpAccessibleItem* OpStringItem::GetPreviousAccessibleSibling()
{
	return GetAccessibleParent()->GetAccessibleChild(GetAccessibleParent()->GetAccessibleChildIndex(this) - 1);
}

Accessibility::State OpStringItem::AccessibilityGetState()
{
	Accessibility::State state = 0;
	if (this->IsSelected())
		state |= Accessibility::kAccessibilityStateCheckedOrSelected;
	if (!this->IsEnabled())
		state |= Accessibility::kAccessibilityStateDisabled;
	if (GetAccessibleParent()->GetAccessibleFocusedChildOrSelf() == this)
		state |= Accessibility::kAccessibilityStateFocused;
	if(m_list_item_listener->GetAccessibleListListener()->IsListVisible())
		state |= Accessibility::kAccessibilityStateInvisible;

	return state;
}

OP_STATUS OpStringItem::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	return m_list_item_listener->GetAbsolutePositionOfItem(this, rect);
}

OP_STATUS OpStringItem::AccessibilityGetText(OpString& str)
{
	RETURN_IF_ERROR(str.Set(string.Get()));
	if (string2 != NULL)
	{
		RETURN_IF_ERROR(str.Append(UNI_L(" ")));
		RETURN_IF_ERROR(str.Append(string2->Get()));
	}
	return OpStatus::OK;
}

OP_STATUS OpStringItem::AccessibilityScrollTo(Accessibility::ScrollTo scroll_to)
{
	return m_list_item_listener->GetAccessibleListListener()->ScrollItemTo(GetAccessibleParent()->GetAccessibleChildIndex(this), scroll_to);
}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

void OpStringItem::SetEnabled(BOOL status)
{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	BOOL changed = status != packed.is_enabled;
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
	packed.is_enabled = status;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (changed)
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateDisabled));
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
}

void OpStringItem::SetSelected(BOOL status)
{
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	BOOL changed = status != packed.is_selected;
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
	packed.is_selected = status;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	if (changed)
		AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateCheckedOrSelected));
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef SKIN_SUPPORT
	m_image.SetState(status ? SKINSTATE_SELECTED : 0);
#endif // SKIN_SUPPORT
}


// == ItemHandler ===========================================================

ItemHandler::ItemHandler(BOOL multiselectable)
	: is_multiselectable(multiselectable)
	, focused_item(0)
	, widest_item(0)
	, highest_item(0)
	, in_group_nr(0)
	, free_items(TRUE)
	, has_groups(FALSE)
{
}

OP_STATUS ItemHandler::DuplicateOf(ItemHandler& ih, BOOL own_items, BOOL dup_focused_item)
{
	is_multiselectable = ih.is_multiselectable;
	if (dup_focused_item)
		focused_item = ih.focused_item;
	widest_item = ih.widest_item;
	highest_item = ih.highest_item;
	has_groups = ih.has_groups;
	if (own_items)
	{
		free_items = TRUE;
		ih.free_items = FALSE;
	}
	else
		free_items = FALSE;
	return m_items.DuplicateOf(ih.m_items);
}

#ifdef WIDGETS_CLONE_SUPPORT
OP_STATUS ItemHandler::CloneOf(ItemHandler& ih, OpWidget* widget)
{
	free_items = TRUE;
	for(int i = 0; i < ih.CountItemsAndGroups(); i++)
	{
		OpStringItem* src_item = ih.m_items.Get(i, CompleteVector);
		OpStringItem* item = MakeItem(src_item->string.Get(), -1, widget);
		if (!item)
			return OpStatus::ERR_NO_MEMORY;
		item->SetSeperator(src_item->IsSeperator());
		item->SetGroupStart(src_item->IsGroupStart());
		item->SetGroupStop(src_item->IsGroupStop());
		INT32 got_index = 0;
		OP_STATUS status = AddItem(item, widget, &got_index);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(item);
			return status;
		}
		item->SetSelected(src_item->IsSelected());
		item->SetEnabled(src_item->IsEnabled());
		if (src_item->IsGroupStart() || src_item->IsGroupStop())
			m_items.SetVisibility(got_index, FALSE);
	}
	is_multiselectable = ih.is_multiselectable;
	focused_item = ih.focused_item;
	widest_item = ih.widest_item;
	highest_item = ih.highest_item;
	has_groups = ih.has_groups;
	RecalculateWidestItem(widget);
	return OpStatus::OK;
}
#endif // WIDGETS_CLONE_SUPPORT

ItemHandler::~ItemHandler()
{
	RemoveAll();
}

void ItemHandler::RemoveAll()
{
	// Delete items
	if (free_items)
	{
		m_items.DeleteAll();
	}
	focused_item = 0;
	widest_item = 0;
	highest_item = 0;
}

OP_STATUS ItemHandler::ChangeItem(const uni_char* txt, INT32 nr, OpWidget* widget)
{
	INT32 index = FindItemIndex(nr);
	OpStringItem* item = m_items.Get(index, CompleteVector);
	OP_STATUS status = item->SetString(txt, widget);

	if (!OpStatus::IsError(status))
		UpdateMaxSizes(index, widget);

	return status;
}

OpStringItem* ItemHandler::MakeItem(const uni_char* txt, INT32 nr, OpWidget* widget, INTPTR user_data)
{
	OpStringItem* item = OP_NEW(OpStringItem, ());
	if(item == NULL)
		return 0;

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	item->SetAccessibleListItemListener(this);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	OP_STATUS status = item->SetString(txt, widget);

	if(OpStatus::IsError(status))
	{
		OP_DELETE(item);
		return 0;
	}

	item->SetUserData(user_data);

	return item;
}

OP_STATUS ItemHandler::AddItem(const uni_char* txt, const uni_char* txt2, OpWidget* widget)
{
	INT32 got_index;
	return AddItem(txt, -1, &got_index, widget, 0, txt2);
}

OP_STATUS ItemHandler::AddItem(const uni_char* txt, INT32 nr, INT32 *got_index, OpWidget* widget, INTPTR user_data, const uni_char* txt2)
{
	int num_items = CountItems();

	// nr should be on a existing item or after the last one.
	OP_ASSERT(nr <= num_items);
	if (nr > num_items)
		nr = num_items;

	// Make the item :
	// ---------------
	OpStringItem* item = MakeItem(txt, nr, widget, user_data);

	if(!item)
		return OpStatus::ERR_NO_MEMORY;

	// Add the item :
	// ---------------
	OP_STATUS status = AddItem(item, widget, got_index, nr);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(item);
		return status;
	}

	if (txt2)
	{
		item->string2 = OP_NEW(OpWidgetString, ());
		if (!item->string2 || OpStatus::IsError(item->string2->Set(txt2, widget)))
		{
			OP_DELETE(item->string2);
			item->string2 = NULL;
			RemoveItem(FindItem(item));
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	item->SetUserData(user_data);

	return OpStatus::OK;
}


OP_STATUS ItemHandler::AddItem(OpStringItem * item, OpWidget* widget, INT32 *got_index, INT32 nr)
{
	int num_items = CountItems();

	// Find the right index to insert it to
	INT32 index;

	if(nr == -1 || nr == num_items)
	{
		RETURN_IF_ERROR(m_items.Add(item));
		index = CountItemsAndGroups() - 1;
	}
	else
	{
		index = FindItemIndex(nr);
		m_items.Insert(index, item, CompleteVector);
	}

	INT32 got_nr = nr;
	if (got_nr == -1)
	{
		got_nr = CountItems() - 1;
		if (got_nr < 0)
			got_nr = 0; // If group is inserted CountItems might return 0
	}

	item->SetSelected(FALSE);

	BOOL is_dropdown = widget->GetType() == OpTypedObject::WIDGET_TYPE_DROPDOWN && ((OpDropDown*)widget)->edit == NULL;

	if (!item->IsGroupStart() && !item->IsGroupStop())
	{
		if(!is_multiselectable && focused_item == got_nr && is_dropdown)
		{
			item->SetSelected(TRUE);
			if (focused_item < CountItems() - 1)
				GetItemAtNr(focused_item + 1)->SetSelected(FALSE);
		}
		if (focused_item > got_nr)
		{
			focused_item++;
		}
	}

	UpdateMaxSizes(index, widget);

	if (got_index)
	{
		*got_index = index;
	}

	return OpStatus::OK;
}

void ItemHandler::RecalculateWidestItem(OpWidget* widget)
{
	int old_in_group_nr = in_group_nr;

	in_group_nr = 0;
	widest_item = 0;
	highest_item = 0;
	int num_items = CountItemsAndGroups();
	for(int i = 0; i < num_items; i++)
	{
		UpdateMaxSizes(i, widget);

		OpStringItem* item = m_items.Get(i, CompleteVector);
		if (item->IsGroupStart())
			in_group_nr++;
		else if (item->IsGroupStop())
			in_group_nr--;
	}

	in_group_nr = old_in_group_nr;
}

void ItemHandler::UpdateMaxSizes(INT32 index, OpWidget* widget)
{
	OP_ASSERT(index >= 0 && index < (INT32) CountItemsAndGroups());
	OpStringItem* item = m_items.Get(index, CompleteVector);
	OpWidgetInfo* info = widget->GetInfo();

	// Throw away the size cache in the strings
	item->string.NeedUpdate();
	if (item->string2)
		item->string2->NeedUpdate();

	INT32 margin_left, margin_top, margin_right, margin_bottom;
	info->GetItemMargin(margin_left, margin_top, margin_right, margin_bottom);

	INT32 item_margin_w = margin_left + margin_right + in_group_nr * OPTGROUP_EXTRA_MARGIN;
	INT32 item_margin_h = margin_top + margin_bottom;
	INT32 item_height = item->string.GetHeight() + item_margin_h;

#ifdef SKIN_SUPPORT
	if (item->m_image.HasContent())
	{
		INT32 image_width;
		INT32 image_height;

		if( item->m_image.GetRestrictImageSize() )
		{
			// Assume 100 to be large enough.
			OpRect r = item->m_image.CalculateScaledRect(OpRect(0, 0, 100, 100), FALSE, FALSE);
			image_height = r.height;
			image_width = r.width;
		}
		else
		{
			item->m_image.GetSize(&image_width, &image_height);
		}

		item_margin_w += 2 + image_width;
		if (image_height + 2 > item_height)
			item_height = image_height + 2;
	}
	if (item->m_indent)
	{
		OpWidgetImage dummy;

		dummy.SetRestrictImageSize(TRUE);
		dummy.SetSkinManager(widget->GetSkinManager());

		INT32 image_width;
		INT32 image_height;

		dummy.GetRestrictedSize(&image_width, &image_height);

		item_margin_w += image_width * item->m_indent;
	}
#endif // SKIN_SUPPORT
#ifdef WIDGETS_CHECKBOX_MULTISELECT
	if (!item->IsGroupStart())
	{
		item_margin_w += 20; // FIX: Read value from skin etc.
	}
#endif // WIDGETS_CHECKBOX_MULTISELECT
	INT32 string_width = item->string.GetWidth();

	if (item->string2)
		string_width += item->string2->GetWidth() + 4;

	if (string_width + item_margin_w > widest_item)
		widest_item = string_width + item_margin_w;
	if (item_height > highest_item)
		highest_item = item_height;
}

void ItemHandler::RemoveItem(INT32 nr)
{
	BOOL last_one = (focused_item == CountItems() - 1 && focused_item != 0);
	if (nr < focused_item || last_one)
	{
		focused_item--;
	}

	INT32 index = FindItemIndex(nr);
	OpStringItem *item_ptr = m_items.Remove(index, CompleteVector);
	OP_DELETE(item_ptr);

	if (!is_multiselectable && CountItems() > 0)
		SelectItem(focused_item, TRUE);
}

OP_STATUS ItemHandler::BeginGroup(const uni_char* txt, OpWidget* widget)
{
	has_groups = TRUE;

	// Make the item :
	// ---------------
	OpStringItem* item = MakeItem(txt, -1, widget);

	if(!item)
		return OpStatus::ERR_NO_MEMORY;

	// Update the item :
	// ---------------
	item->SetGroupStart(TRUE);
	item->SetEnabled(FALSE);

	// Add the item :
	// ---------------
	INT32 got_index = 0;
	OP_STATUS status = AddItem(item, widget, &got_index);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(item);
		return status;
	}

	// Make item invisible :
	// ---------------
	m_items.SetVisibility(got_index, FALSE);

	in_group_nr++;

	return OpStatus::OK;
}

OP_STATUS ItemHandler::EndGroup(OpWidget* widget)
{
	if (!has_groups || in_group_nr == 0)
		return OpStatus::ERR; // There has been a OOM or unbalanced HTML

	// Make the item :
	// ---------------
	OpStringItem* item = MakeItem(UNI_L(""), -1, widget);

	if(!item)
		return OpStatus::ERR_NO_MEMORY;

	// Update the item :
	// ---------------
	item->SetGroupStop(TRUE);
	item->SetEnabled(FALSE);

	// Add the item :
	// ---------------
	INT32 got_index = 0;
	OP_STATUS status = AddItem(item, widget, &got_index);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(item);
		return status;
	}

	// Make item invisible :
	// ---------------
	m_items.SetVisibility(got_index, FALSE);

	in_group_nr--;

	return OpStatus::OK;
}

void ItemHandler::RemoveGroup(int group_nr, OpWidget* widget)
{
	int next_group_nr = 0;
	for(int i = 0; i < (int) CountItemsAndGroups(); i++)
	{
		OpStringItem* item = m_items.Get(i, CompleteVector);

		if (item->IsGroupStart())
		{
			if (next_group_nr == group_nr)
			{
				OpStringItem *item_ptr = m_items.Remove(i--, CompleteVector);
				OP_DELETE(item_ptr);
			}
			next_group_nr++;
		}
		else if (item->IsGroupStop())
		{
			if (next_group_nr - 1 == group_nr)
			{
				OpStringItem *item_ptr = m_items.Remove(i, CompleteVector);
				OP_DELETE(item_ptr);
				break;
			}
			next_group_nr--;
		}
	}
}

void ItemHandler::RemoveAllGroups()
{
	m_items.DeleteAll();
}

INTPTR ItemHandler::GetItemUserData(INT32 nr)
{
	if (nr < 0 || nr >= CountItems())
		return 0;

	INT32 index = FindItemIndex(nr);

	OpStringItem* itemlink = m_items.Get(index, CompleteVector);

	return itemlink->GetUserData();
}

void ItemHandler::SelectItem(INT32 nr, BOOL selected)
{
	if (nr < 0 || nr >= CountItems())
		return;
	INT32 index = FindItemIndex(nr);

	OpStringItem* itemlink = m_items.Get(index, CompleteVector);

	INT32 previous_focused = focused_item;
	if (nr != focused_item)
	{
		if(selected)
			focused_item = nr;		//(julienp) For the accessibility API, it makes more sense that
									//the focus has left the item before it gets unselected
		if(!is_multiselectable && selected == TRUE)
		{
			SelectItem(previous_focused, FALSE);
		}
	}
	itemlink->SetSelected(selected);
}

void ItemHandler::EnableItem(INT32 nr, BOOL enabled)
{
	INT32 index = FindItemIndex(nr);

	OpStringItem* itemlink = m_items.Get(index, CompleteVector);
	itemlink->SetEnabled(enabled);
}

void ItemHandler::SetIndent(INT32 nr, INT32 indent, OpWidget* widget)
{
#ifdef SKIN_SUPPORT
	GetItemAtNr(nr)->m_indent = indent;
	INT32 index = FindItemIndex(nr);
	UpdateMaxSizes(index, widget);
#endif
}

void ItemHandler::SetImage(INT32 nr, const char* image, OpWidget* widget)
{
#ifdef SKIN_SUPPORT
	GetItemAtNr(nr)->m_image.SetImage(image);
	INT32 index = FindItemIndex(nr);
	UpdateMaxSizes(index, widget);
#endif
}


void ItemHandler::SetImage(INT32 nr, OpWidgetImage* widget_image, OpWidget* widget)
{
#ifdef SKIN_SUPPORT
	GetItemAtNr(nr)->m_image.SetWidgetImage(widget_image);
	GetItemAtNr(nr)->m_image.SetRestrictImageSize(widget_image->GetRestrictImageSize());
	INT32 index = FindItemIndex(nr);
	UpdateMaxSizes(index, widget);
#endif
}

void ItemHandler::SetFgColor(INT32 nr, UINT32 col)
{
	GetItemAtNr(nr)->fg_col = col;
}

void ItemHandler::SetBgColor(INT32 nr, UINT32 col)
{
	GetItemAtNr(nr)->bg_col = col;
}

void ItemHandler::SetScript(INT32 nr, WritingSystem::Script script)
{
	GetItemAtNr(nr)->string.SetScript(script);
}

INT32 ItemHandler::GetTotalHeight()
{
	INT32 num = CountItemsAndGroups();

	if (has_groups)
	{
		INT32 height = 0;
		INT32 i;
		for(i = 0; i < num; i++)
		{
			OpStringItem* item = m_items.Get(i, CompleteVector);
			if (!item->IsGroupStop())
				height += highest_item;
		}
		return height;
	}
	return num * highest_item;
}

INT32 ItemHandler::GetItemYPos(INT32 nr)
{
	if (has_groups)
	{
		INT32 height = 0;
		INT32 i, num = CountItemsAndGroups(), itemnr = 0;
		for(i = 0; i < num; i++)
		{
			OpStringItem* item = m_items.Get(i, CompleteVector);
			if (! (item->IsGroupStart() || item->IsGroupStop()))
			{
				if (itemnr == nr)
					return height;
				itemnr++;
			}
			if (!item->IsGroupStop())
				height += highest_item;
		}
		return height;
	}
	return nr * highest_item;
}

BOOL ItemHandler::IsSelected(INT32 nr)
{
	INT32 index = FindItemIndex(nr);
	return m_items.Get(index, CompleteVector)->IsSelected();
}

BOOL ItemHandler::IsEnabled(INT32 nr)
{
	INT32 index = FindItemIndex(nr);
	return m_items.Get(index, CompleteVector)->IsEnabled();
}

INT32 ItemHandler::FindItemIndex(INT32 item_nr)
{
	if (has_groups)
	{
		//Translate visible to real index
		return m_items.GetIndex(item_nr, VisibleVector, CompleteVector);
	}
	return item_nr;
}

INT32 ItemHandler::FindItemNr(INT32 index)
{
	if (has_groups)
	{
		//Translate real to visible index
		return m_items.GetIndex(index, CompleteVector, VisibleVector);
	}
	return index;
}

INT32 ItemHandler::FindItemNrAtPosition(INT32 y, BOOL always_get_option/* = FALSE*/)
{
	if (has_groups)
	{
		INT32 last_item_index = -1, itemnr = 0;
		INT32 i, ypos = 0, num = CountItemsAndGroups();
		for(i = 0; i < num; i++)
		{
			OpStringItem* item = m_items.Get(i, CompleteVector);
			if (! (item->IsGroupStart() || item->IsGroupStop()))
			{
				last_item_index = itemnr;
				itemnr++;
			}
			if (!item->IsGroupStop())
			{
				if (ypos + highest_item > y && last_item_index != -1)
				{
					if (item->IsGroupStart())
					{
						if (always_get_option)
							continue;
						return -1;
					}
					else
						return last_item_index;
				}
				ypos += highest_item;
			}
		}
		INT32 num_items = CountItems();
		return num_items ? num_items - 1 : 0;
	}
	const INT32 count = CountItems();
	return (count && highest_item > 0) ? MIN(count-1, (y / highest_item)) : -1;
}

OpStringItem* ItemHandler::GetItemAtIndex(INT32 index)
{
	OP_ASSERT(index >= 0 && index < (INT32) CountItemsAndGroups());
	return m_items.Get(index, CompleteVector);
}

OpStringItem* ItemHandler::GetItemAtNr(INT32 nr)
{
    INT32 index = FindItemIndex(nr);
	if(index >= 0) return m_items.Get(index, CompleteVector);
    return NULL;
}

void ItemHandler::SetAll(BOOL selected)
{
	INT32 i, num = CountItemsAndGroups();
	for(i = 0; i < num; i++)
	{
		OpStringItem* item = m_items.Get(i, CompleteVector);
		if (item->IsEnabled() || !selected) ///< Should we, or should we not change disabled items?
											///< They should atleast be deselected // timj 2004-09-03
			item->SetSelected(selected);
	}
}

void ItemHandler::SetMultiple(BOOL multiple)
{
	if (is_multiselectable != multiple)
	{
		if (is_multiselectable)
		{
			// Going from multi selection to single selection - change selection or leave at is it and let the caller clean things up?
			is_multiselectable = FALSE;
		}
		else
		{
			// Going from single selection to multi selection, just set the flag
			is_multiselectable = TRUE;
		}
	}
}

INT32 ItemHandler::FindItem(const uni_char* string)
{
	// Find first match
	INT32 i, num_items = CountItems();
	for(i = 0; i < num_items; i++)
	{
		OpStringItem* item = GetItemAtNr(i);
		if (!item->IsEnabled())
			continue;

		const uni_char *item_string = item->string.Get();
		while (uni_isspace(*item_string))
			item_string++;

		if (uni_strnicmp(string, item_string, uni_strlen(string)) == 0)
			return i;
	}
	// No match found. If string consist of the same key, we want to loop through all items with that key.
	int len = uni_strlen(string);
	if (len)
	{
		for(i = 0; i < len - 1; i++)
			if (string[i] != string[i + 1])
				return -1;

		i = focused_item;
		for(int wrap = 0; wrap < 2; wrap++)
		{
			for(   ; i < num_items; i++)
			{
				OpStringItem* item = GetItemAtNr(i);
				if (!item->IsEnabled())
					continue;

				if (uni_toupper(string[0]) == uni_toupper(item->string.Get()[0]) && i != focused_item)
					return i;
			}
			i = 0; // Wrap to beginning
		}
	}
	return -1;
}
INT32 ItemHandler::FindItem(OpStringItem* item)
{
	return m_items.Find(item);
}

INT32 ItemHandler::FindEnabled(INT32 from_nr, BOOL forward)
{
	// Find a enabled item
	OpStringItem* item;
	INT32 num_items = CountItems();
	do
	{
		from_nr += (forward ? 1 : -1);
		if (from_nr < 0 || from_nr >= num_items)
			return -1; // No enabled item found
		item = GetItemAtNr(from_nr);
	} while (!item->IsEnabled());
	return from_nr;
}

BOOL ItemHandler::MoveFocus(INT32 from_nr, INT32 nr, BOOL select)
{
	if (CountItemsAndGroups() == 0)
		return FALSE;

	focused_item = from_nr;
	OpStringItem* item = GetItemAtNr(focused_item);

#ifdef WIDGETS_CHECKBOX_MULTISELECT
	if (!is_multiselectable)
#else
	BOOL changeSelection = !is_multiselectable || select;
	if (changeSelection)
#endif
	SetAll(FALSE);

	while (item)
	{
#ifdef WIDGETS_CHECKBOX_MULTISELECT
	if (!is_multiselectable)
#else
	if (changeSelection)
#endif
		if (item->IsEnabled() &&
			((!is_multiselectable && nr == focused_item)
			|| (is_multiselectable && select)
			|| (is_multiselectable && !select && nr == focused_item)))
			item->SetSelected(TRUE);
		if (nr == focused_item)
			break;

		if (focused_item > nr)
			focused_item--;
		else
			focused_item++;

		if (focused_item < 0 || focused_item >= (INT32) CountItemsAndGroups())
			break;
		item = GetItemAtNr(focused_item);
	}
	return TRUE;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

int ItemHandler::GetAccessibleChildrenCount()
{
	return CountItemsAndGroups();
}

OpAccessibleItem* ItemHandler::GetAccessibleParent()
{
	if (!m_accessible_listener)
		return NULL;
	return m_accessible_listener->GetAccessibleParentOfList();
}

OpAccessibleItem* ItemHandler::GetAccessibleChild(int index)
{
	return GetItemAtIndex(index);
}

int ItemHandler::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int n = CountItemsAndGroups();
	for (int i = 0; i < n; i ++)
		if (GetItemAtIndex(i) == child)
			return i;

	return Accessibility::NoSuchChild;
}

Accessibility::State ItemHandler::AccessibilityGetState()
{
	Accessibility::State state = 0;
	if (m_accessible_listener->IsListFocused() && ((ItemHandler*)this)->CountItems() == 0)
		state |= Accessibility::kAccessibilityStateFocused;
	if (!m_accessible_listener->IsListVisible())
		state |= Accessibility::kAccessibilityStateInvisible;
	return state;
}

OpAccessibleItem* ItemHandler::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpRect myRect;
	AccessibilityGetAbsolutePosition(myRect);

	if (x < myRect.Left() || x > myRect.Right())
		return NULL;
	int index = FindItemNrAtPosition(y - myRect.y);
	if (index >= 0 && index < CountItems())
		return GetItemAtIndex(index);

	return NULL;
}

OpAccessibleItem* ItemHandler::GetAccessibleFocusedChildOrSelf()
{
	if (!m_accessible_listener->IsListFocused())
		return NULL;

	if (CountItems() == 0 || focused_item == 0)
		return this;

	return GetItemAtIndex(focused_item);
}

OP_STATUS ItemHandler::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	return m_accessible_listener->GetAbsolutePositionOfList(rect);
}

OP_STATUS ItemHandler::GetAbsolutePositionOfItem(OpStringItem* string_item, OpRect &rect)
{
	RETURN_IF_ERROR(AccessibilityGetAbsolutePosition(rect));
	rect.y+= GetItemYPos(FindItem(string_item));
	rect.height = highest_item;

	return OpStatus::OK;
}

OpAccessibleItem* ItemHandler::GetAccessibleLabelForControl()
{
	return GetAccessibleParent()->GetAccessibleLabelForControl();
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT


// == ListboxListener =======================================================

void ListboxListener::OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	if (FormObject* form_obj = widget->GetFormObject(TRUE))
		form_obj->UpdatePosition();

	OpRect rect = listbox->GetDocumentRect(TRUE);
	listbox->GetInfo()->AddBorder(listbox, &rect);
	rect.width -= listbox->GetInfo()->GetVerticalScrollbarWidth();
	if(listbox->LeftHandedUI())
		rect.x += listbox->GetInfo()->GetVerticalScrollbarWidth();
	VisualDevice* vis_dev = listbox->GetVisualDevice();
	if (vis_dev->view == NULL)
		return; // No view so we can't scroll (Probably printpreview which we shouldn't scroll anyway)

	if (!listbox->IsForm())
	{
		vis_dev->ScrollRect(rect, 0, old_val - new_val);
		return;
	}

	OP_ASSERT(listbox->IsForm());
	if(OpWidgetListener* listener = listbox->GetListener())
		listener->OnScroll(listbox, old_val, new_val, caused_by_input);

	if (caused_by_input && !listbox->HasCssBackgroundImage())
	{
		OpRect clip_rect;
		if (listbox->GetIntersectingCliprect(clip_rect))
		{
			rect.OffsetBy(-vis_dev->GetRenderingViewX(), -vis_dev->GetRenderingViewY());
			rect.IntersectWith(clip_rect);

			vis_dev->ScrollRect(rect, 0, old_val - new_val);
			return;
		}
	}

	rect = listbox->GetDocumentRect(TRUE);
	vis_dev->Update(rect.x, rect.y, rect.width, rect.height);
}

// == OpListBox

OP_STATUS OpListBox::Construct(OpListBox** obj, BOOL multiselectable, BorderStyle border_style)
{
	*obj = OP_NEW(OpListBox, (multiselectable, border_style));
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpListBox::OpListBox(BOOL multiselectable, BorderStyle border_style)
	:
	  border_style(border_style)
	, shiftdown_item(-1)
#ifndef MOUSELESS
	, mousedown_item(-1)
	, mouseover_item(-1)
#endif // !MOUSELESS
	, scrollbar(NULL)
	, scrollbar_listener(this)
	, x_scroll(0)
	, ih(multiselectable)
	, hot_track(FALSE)
	, grab_n_scroll(FALSE)
	, is_grab_n_scrolling(0)
	, anchor_index(-1)
	, is_dragging(FALSE)
{
	SetTabStop(TRUE);
	OP_STATUS status = OpScrollbar::Construct(&scrollbar, FALSE);
	CHECK_STATUS(status);

	scrollbar->SetVisibility(FALSE);
	scrollbar->SetListener(&scrollbar_listener);
	AddChild(scrollbar, TRUE);

	// The BORDER_STYLE_POPUP border is currently drawn manually and not via a
	// skin (we might want to change this in the future)
	if (border_style == BORDER_STYLE_FORM)
	{
#ifdef SKIN_SUPPORT
		GetBorderSkin()->SetImage("Listbox Skin");
		SetSkinned(TRUE);
#endif
	}

	TRAP(status, g_pcdisplay->RegisterListenerL(this));

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	ih.SetAccessibleListListener(this);
#endif
}

OpListBox::~OpListBox()
{
	g_pcdisplay->UnregisterListener(this);


	if (g_widget_globals->is_down)
		g_widget_globals->is_down = FALSE;
}

#ifdef WIDGETS_CLONE_SUPPORT

OP_STATUS OpListBox::CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded)
{
	*obj = NULL;
	OpListBox *widget;

	RETURN_IF_ERROR(OpListBox::Construct(&widget));
	parent->AddChild(widget);

	if (OpStatus::IsError(widget->CloneProperties(this, font_size)) ||
		OpStatus::IsError(widget->ih.CloneOf(ih, widget)))
	{
		widget->Remove();
		OP_DELETE(widget);
		return OpStatus::ERR;
	}

	widget->SetHasCssBorder(TRUE);
	widget->grab_n_scroll = TRUE;
	widget->UpdateScrollbar();
	*obj = widget;
	return OpStatus::OK;
}

#endif // WIDGETS_CLONE_SUPPORT

void OpListBox::AddMargin(OpRect *rect)
{
	OpStringItem::AddItemMargin(GetInfo(), this, *rect);
}

void OpListBox::UpdateScrollbar()
{
	// Change the scrollbar stepvalue
	OpRect tmprect = GetBounds();
	if (tmprect.width > 0 && tmprect.height > 0) // If this is called before the size is set, we want to avoid
												 // updating stuff since it will result in negative values which
												 // might mess display up.
	{
		GetInfo()->AddBorder(this, &tmprect);
		int height = tmprect.height;
		scrollbar->SetSteps(ih.highest_item, height - ih.highest_item);

		// Update the scrollbars limit
		INT32 total_height = ih.GetTotalHeight();
		INT32 max = total_height - height;
		if (total_height > 0)
			scrollbar->SetLimit(0, max, height);

		if (total_height == 0 || max <= 0) // Make scrollbar invisible
			scrollbar->SetVisibility(FALSE);
		else // Make scrollbar visible
			scrollbar->SetVisibility(TRUE);

		ScrollIfNeeded();
	}
}

void OpListBox::UpdateScrollbarPosition()
{
	OpRect border_rect(0, 0, rect.width, rect.height);
	GetInfo()->AddBorder(this, &border_rect);

	if (!LeftHandedUI())
		border_rect.x += border_rect.width - GetInfo()->GetVerticalScrollbarWidth();

	scrollbar->SetRect(OpRect(border_rect.x, border_rect.y, GetInfo()->GetVerticalScrollbarWidth(),
							  border_rect.height));
}

void OpListBox::PrefChanged (enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
	OP_ASSERT(OpPrefsCollection::Display == id);

	if (pref == PrefsCollectionDisplay::LeftHandedUI
#ifdef SUPPORT_TEXT_DIRECTION
		|| pref == PrefsCollectionDisplay::RTLFlipsUI
#endif
		)
	{
		UpdateScrollbarPosition();
		InvalidateAll();
	}
}

void OpListBox::ScrollIfNeeded(BOOL include_document, BOOL smooth_scroll)
{
	OpRect inner_rect = GetBounds();
	GetInfo()->AddBorder(this, &inner_rect);
	int y = ih.GetItemYPos(ih.focused_item) - scrollbar->GetValue();
	if (y < 0) // upper edge above top border
		scrollbar->SetValue(scrollbar->GetValue() + y);
	else if (y + ih.highest_item > inner_rect.height) // lower edge below bottom border
		scrollbar->SetValue(scrollbar->GetValue() + (y - inner_rect.height) + ih.highest_item);

	// Start scroll of item in X if wider than the listbox

	x_scroll = 0;
	if (IsTimerRunning() && timer_event == TIMER_EVENT_SCROLL_X)
		StopTimer();

	if (ih.focused_item >= 0 && ih.focused_item < CountItems())
	{
		OpStringItem* focused_item = ih.GetItemAtNr(ih.focused_item);
		if (!IsTimerRunning() && focused_item && focused_item->string.GetWidth() > GetBounds().width
			// Focused or hot_track (dropdowns listbox has hot_track but is never focused)
			&& (IsFocused() || hot_track))
		{
			timer_event = TIMER_EVENT_SCROLL_X;
			StartTimer(1000);
		}
	}
}

BOOL OpListBox::HasScrollbar()
{
	return scrollbar->IsVisible();
}

OP_STATUS OpListBox::AddItem(const uni_char* txt, INT32 index, INT32 *got_index, INTPTR user_data, BOOL selected, const char* image)
{
	shiftdown_item = -1;
	OP_STATUS status = ih.AddItem(txt, index, got_index, this, user_data);

	if (index == -1)
	{
		index = ih.CountItems() - 1;
	}

	if (got_index)
	{
		*got_index = index;
	}

	if (image)
	{
		ih.SetImage(index, image, this);
	}

	if (selected)
	{
		SelectItem(index, TRUE);
	}

	Invalidate(GetBounds());
	UpdateScrollbar();
#ifdef WIDGETS_IMS_SUPPORT
	UpdateIMS();
#endif // WIDGETS_IMS_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventReorder));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	return status;
}

#ifdef WIDGET_ADD_MULTIPLE_ITEMS

OP_STATUS OpListBox::AddItems(const OpVector<OpString> *txt, const OpVector<INT32> *index, const OpVector<void> *user_data, INT32 selected, const OpVector<char> *image)
{
	shiftdown_item = -1;

	UINT32 count = txt->GetCount();

	if (index && index->GetCount() < count || user_data && user_data->GetCount() < count || image && image->GetCount() < count)
		return OpStatus::ERR;

	for (UINT32 i = 0; i < count; i++)
	{
		INT32 idx = index?*(index->Get(i)):-1;
		OP_STATUS status = ih.AddItem(txt->Get(i)->CStr(), idx, index ? index->Get(i) : NULL, this, reinterpret_cast<INTPTR>(user_data ? user_data->Get(i) : 0));

		if (OpStatus::IsError(status))
			return status;

		if (idx == -1)
		{
			idx = ih.CountItems() - 1;
		}

		if (index)
		{
			*(index->Get(i)) = idx;
		}

		if (image)
		{
			ih.SetImage(idx, image->Get(i), this);
		}

		if (int(i) == selected)
		{
			SelectItem(idx, TRUE);
		}
	}

	Invalidate(GetBounds());
	UpdateScrollbar();
#ifdef WIDGETS_IMS_SUPPORT
	UpdateIMS();
#endif // WIDGETS_IMS_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventReorder));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	return OpStatus::OK;
}

#endif // WIDGET_ADD_MULTIPLE_ITEMS

void OpListBox::RemoveItem(INT32 index)
{
	anchor_index = -1;
	shiftdown_item = -1;
	ih.RemoveItem(index);

	UpdateScrollbar();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventReorder));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
#ifdef WIDGETS_IMS_SUPPORT
	UpdateIMS();
#endif // WIDGETS_IMS_SUPPORT
}

void OpListBox::RemoveAll()
{
	anchor_index = -1;
	shiftdown_item = -1;
	ih.RemoveAll();
#ifdef WIDGETS_IMS_SUPPORT
	UpdateIMS();
#endif // WIDGETS_IMS_SUPPORT

	UpdateScrollbar();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventReorder));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

OP_STATUS OpListBox::ChangeItem(const uni_char* txt, INT32 index)
{
	return ih.ChangeItem(txt, index, this);
}

OP_STATUS OpListBox::BeginGroup(const uni_char* txt)
{
	OP_STATUS status = ih.BeginGroup(txt, this);

	UpdateScrollbar();
	return status;
}

void OpListBox::EndGroup()
{
	ih.EndGroup(this);
}

void OpListBox::RemoveGroup(int group_nr)
{
	ih.RemoveGroup(group_nr, this);

	UpdateScrollbar();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventReorder));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

void OpListBox::RemoveAllGroups()
{
	ih.RemoveAllGroups();

	UpdateScrollbar();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventReorder));
#endif
}

void OpListBox::SelectItem(INT32 nr, BOOL selected)
{
#ifndef MOUSELESS
	if (mousedown_item != -1)
		return; // The user is currently selecting things so don't let scripts/reflows interrupt her.
#endif // !MOUSELESS

	if (nr == -1)
	{
		ih.focused_item = 0;
		ih.SelectItem(ih.focused_item, FALSE);
		ScrollIfNeeded();
	}
	else
	{
		ih.SelectItem(nr, selected);
		if (!ih.is_multiselectable)
			ScrollIfNeeded();
	}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
}

void OpListBox::EnableItem(INT32 nr, BOOL enabled)
{
	ih.EnableItem(nr, enabled);
}

INT32 OpListBox::GetSelectedItem()
{
	if (CountItems() == 0) // Has no items
		return -1;
	if (IsSelected(ih.focused_item) == FALSE) // Item -1 is selected
		return -1;
	return ih.focused_item;
}

void OpListBox::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, GetType(), w, h, cols, rows);
}

void OpListBox::OnResize(INT32* new_w, INT32* new_h)
{
	UpdateScrollbarPosition();
	UpdateScrollbar();
}

void OpListBox::OnDirectionChanged()
{
	UpdateScrollbarPosition();
}

void OpListBox::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpRect r(GetBounds());

	widget_painter->DrawListbox(r);

	if(!HasCssBorder() && border_style == BORDER_STYLE_POPUP)
	{
		// Listboxes with thin popup-style borders draw the border manually.  A
		// better way to do this might be to instead introduce a new skin
		// element.

		vis_dev->SetColor32(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND));
		vis_dev->FillRect(r);

		vis_dev->SetColor32(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT));

		INT32 left, top, right, bottom;
		GetInfo()->GetBorders(this, left, top, right, bottom);

		OpRect border(r);

		// top
		border.height = top;
		vis_dev->FillRect(border);
		// bottom
		border.y += r.height - bottom;
		border.height = bottom;
		vis_dev->FillRect(border);

		// remove top and bottom borders from height of rect
		border.y = r.y + top;
		border.height = r.height - top - bottom;

		// left
		border.width = left;
		vis_dev->FillRect(border);
		// right
		border.x += r.width - right;
		border.width = right;
		vis_dev->FillRect(border);
	}

	if (ih.CountItems() == 0 || ih.highest_item == 0) // Has no items
		return;

	GetInfo()->AddBorder(this, &r);

	INT32 inner_width = r.width;
	if (scrollbar->IsVisible())
		inner_width -= GetInfo()->GetVerticalScrollbarWidth();

	if(LeftHandedUI() && HasScrollbar())
		r.x += GetInfo()->GetVerticalScrollbarWidth();

	SetClipRect(OpRect(r.x, r.y, inner_width, r.height));

	r.y -= scrollbar->GetValue();
	r.width = inner_width;
	int visible_height = r.height + ih.highest_item;
	r.height = ih.highest_item;

	// Start at the first visible item and draw to the last visible item
	INT32 index = 0;
	INT32 extra_margin = 0;
	INT32 focused_index = 0;
	if (ih.CountItems() > 0)
		focused_index = ih.FindItemIndex(ih.focused_item);
	INT32 extra_margin_step = GetRTL() ? -OPTGROUP_EXTRA_MARGIN : OPTGROUP_EXTRA_MARGIN;

	if (!ih.HasGroups())
	{
		// Skip the items above visible range
		index = scrollbar->GetValue() / ih.highest_item;
		r.y += ih.GetItemYPos(index);
	}

	while (index < ih.CountItemsAndGroups())
	{
		OpStringItem* item = ih.GetItemAtIndex(index);

		if (!item->IsGroupStop())
		{
			BOOL is_focused_item = (index == focused_index);
			r.x += extra_margin;

			if (r.y >= - ih.highest_item)
			{
				if (item->IsSeperator())
				{
					vis_dev->SetColor32(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED));
					vis_dev->DrawLine(OpPoint(r.x, r.y + r.height / 2), OpPoint(r.x + r.width, r.y + r.height / 2));
				}
				else
				{
					widget_painter->DrawItem(r, item, TRUE, is_focused_item, extra_margin);
				}
			}
			r.x -= extra_margin;

			r.y += ih.highest_item;
		}

		// Break if we are below the visible range
		if (r.y > visible_height)
			break;

		if (item->IsGroupStart())
			extra_margin += extra_margin_step;
		if (item->IsGroupStop())
			extra_margin -= extra_margin_step;

		index++;
	}

	RemoveClipRect();
}

#ifdef WIDGETS_IMS_SUPPORT
OP_STATUS OpListBox::SetIMSInfo(OpIMSObject& ims_object)
{
	OpWidget::SetIMSInfo(ims_object);
	ims_object.SetIMSAttribute(OpIMSObject::HAS_SCROLLBAR, HasScrollbar());
	ims_object.SetItemHandler(&ih);
	return OpStatus::OK;
}
#endif // WIDGETS_IMS_SUPPORT

void OpListBox::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		if (reason != FOCUS_REASON_MOUSE)
			ScrollIfNeeded();
	}
	else
	{
#ifdef WIDGETS_IMS_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
	if (!ElementExpander::IsEnabled())
#endif //NEARBY_ELEMENT_DETECTION
			DestroyIMS();
#endif // WIDGETS_IMS_SUPPORT

		x_scroll = 0;
		StopTimer();
	}

	Invalidate(GetBounds());
}

BOOL OpListBox::MoveFocusedItem(BOOL forward, BOOL is_rangeaction)
{
	int index = ih.FindEnabled(ih.focused_item, forward);
	if (index != -1)
	{
		if (!ih.is_multiselectable && GetSelectedItem() == -1)
		{
			// If the focused item wasn't selected we won't step to the next, only select it.
			ih.SelectItem(ih.focused_item, TRUE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
			return TRUE;
		}
		else
		{
			return MoveFocus(shiftdown_item, index, is_rangeaction);
		}
	}
	return FALSE;
}

void OpListBox::HighlightFocusedItem()
{
	if (ih.CountItems())
	{
		int y = ih.GetItemYPos(ih.focused_item) - scrollbar->GetValue();
		GenerateHighlightRectChanged(OpRect(0, y, rect.width, ih.highest_item + 2));
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpListBox::OnInputAction(OpInputAction* action)
{
	BOOL update = FALSE;
	BOOL handled = TRUE;

	if (shiftdown_item == -1)
		shiftdown_item = ih.focused_item;

	int old_focused_item = ih.focused_item;

	switch (action->GetAction())
	{
#ifdef PAN_SUPPORT
#ifdef ACTION_COMPOSITE_PAN_ENABLED
		case OpInputAction::ACTION_COMPOSITE_PAN:
#endif // ACTION_COMPOSITE_PAN_ENABLED
		case OpInputAction::ACTION_PAN_X:
		case OpInputAction::ACTION_PAN_Y:
			return OpWidget::OnInputAction(action);
			break;
#endif // PAN_SUPPORT

		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			OpKey::Code key = static_cast<OpKey::Code>(action->GetActionData());
			const uni_char *key_value = action->GetKeyValue();
#ifdef OP_KEY_SPACE_ENABLED
			if (key == OP_KEY_SPACE && !g_widget_globals->itemsearch->IsSearching())
				// Support space in an active inline search (but not as first letter.)
				key_value = UNI_L(" ");
#endif // OP_KEY_SPACE_ENABLED

			ShiftKeyState modifiers = action->GetShiftKeys();
			if ((modifiers == SHIFTKEY_NONE || modifiers == SHIFTKEY_SHIFT) && key >= OP_KEY_SPACE && key_value != NULL)
			{
				g_widget_globals->itemsearch->PressKey(key_value);
				INT32 found_item = ih.FindItem(g_widget_globals->itemsearch->GetSearchString());

				if (found_item != -1)
				{
					ih.SetAll(FALSE);
					ih.SelectItem(found_item, TRUE);
					update = TRUE;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
					AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
				}
				break;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_PREVIOUS_ITEM:
#ifdef ESCAPE_FOCUS_AT_TOP_BOTTOM
			if (ih.focused_item == 0 || ih.FindEnabled(old_focused_item, FALSE) == -1)
			{
				handled = FALSE;
				break;
			}
#endif
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PREVIOUS_ITEM:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_PREVIOUS_LINE:
			handled = update = MoveFocusedItem(FALSE, action->IsRangeAction());
			HighlightFocusedItem();
			break;

		case OpInputAction::ACTION_NEXT_ITEM:
#ifdef ESCAPE_FOCUS_AT_TOP_BOTTOM
			if (ih.focused_item == ih.CountItems() - 1 || ih.FindEnabled(old_focused_item, TRUE) == -1)
			{
				handled = FALSE;
				break;
			}
#endif
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_NEXT_ITEM:
#endif // !HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_NEXT_LINE:
			handled = update = MoveFocusedItem(TRUE, action->IsRangeAction());
			HighlightFocusedItem();
			break;

		case OpInputAction::ACTION_GO_TO_START:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_START:
#endif // !HAS_NOTEXTSELECTION
		{
			update = MoveFocus(shiftdown_item, 0, action->IsRangeAction());
			break;
		}

		case OpInputAction::ACTION_GO_TO_END:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_GO_TO_END:
#endif // !HAS_NOTEXTSELECTION
		{
			update = MoveFocus(shiftdown_item, CountItems() - 1, action->IsRangeAction());
			break;
		}

		case OpInputAction::ACTION_PAGE_UP:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PAGE_UP:
#endif // !HAS_NOTEXTSELECTION
		{
			if (ih.CountItems())
			{
				int moveto = ih.focused_item - GetBounds().height / ih.highest_item;
				if (moveto < 0)
					moveto = 0;
				update = MoveFocus(shiftdown_item, moveto, action->IsRangeAction());
			}
			break;
		}

		case OpInputAction::ACTION_PAGE_DOWN:
#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_RANGE_PAGE_DOWN:
#endif // !HAS_NOTEXTSELECTION
		{
			if (ih.CountItems())
			{
				int moveto = ih.focused_item + GetBounds().height / ih.highest_item;
				INT32 num_items = CountItems() - 1;
				if (moveto > num_items)
					moveto = num_items;
				update = MoveFocus(shiftdown_item, moveto, action->IsRangeAction());
			}
			break;
		}

#ifndef HAS_NOTEXTSELECTION
		case OpInputAction::ACTION_SELECT_ALL:
		{
			if (ih.is_multiselectable)
			{
				ih.SetAll(TRUE);
				update = TRUE;
				Invalidate(GetBounds());
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
			}
			break;
		}
#endif // !HAS_NOTEXTSELECTION

		case OpInputAction::ACTION_CHECK_ITEM:
		case OpInputAction::ACTION_UNCHECK_ITEM:
#ifdef WIDGETS_CHECKBOX_MULTISELECT
		if (ih.CountItems())
		{
			if (ih.is_multiselectable && ih.focused_item != -1)
			{
				BOOL select = (action->GetAction() == OpInputAction::ACTION_CHECK_ITEM);
				if (select == ih.IsSelected(ih.focused_item))
				{
					handled = FALSE;
					break;
				}

				ih.SelectItem(ih.focused_item, select);
				// FIXME: The entire widget doesn't have to be redrawn
				update = TRUE;
				Invalidate(GetBounds());
			}
			break;
		}
#else
		{
			BOOL select = action->GetAction() == OpInputAction::ACTION_CHECK_ITEM;
			OpStringItem* item = ih.GetItemAtNr(ih.focused_item);
			if (ih.is_multiselectable && item && item->IsSelected() != select)
			{
				ih.SelectItem(ih.focused_item, select);
				update = TRUE;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
			}
			else
				handled = FALSE;
			break;
		}
#endif // WIDGETS_CHECKBOX_MULTISELECT

#ifdef _SPAT_NAV_SUPPORT_
		case OpInputAction::ACTION_UNFOCUS_FORM:
			return g_input_manager->InvokeAction(action, GetParentInputContext());
#endif // _SPAT_NAV_SUPPORT_

		default:
		{
			handled = FALSE;
			break;
		}
	}

	// Check if the focused item was not changed and we tried to select previous or next item, we will
	// try to scroll the listbox. Otherwise the user wouldn't be able to scroll to disabled items or groups at the top or bottom.
	BOOL scrolled = FALSE;
	if (ih.focused_item == old_focused_item)
	{
		int old_scroll_val = scrollbar->GetValue();
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_PREVIOUS_ITEM:
			case OpInputAction::ACTION_PREVIOUS_LINE:
#ifdef ESCAPE_FOCUS_AT_TOP_BOTTOM
				if (!scrollbar->CanScroll(ARROW_UP))
				    break;
#endif // ESCAPE_FOCUS_AT_TOP_BOTTOM
				scrollbar->SetValue(old_scroll_val - scrollbar->small_step, TRUE);
				handled = TRUE; // We don't want to bubble up to the next action
				break;
			case OpInputAction::ACTION_NEXT_ITEM:
			case OpInputAction::ACTION_NEXT_LINE:
#ifdef ESCAPE_FOCUS_AT_TOP_BOTTOM
				if (!scrollbar->CanScroll(ARROW_DOWN))
				    break;
#endif // ESCAPE_FOCUS_AT_TOP_BOTTOM
				scrollbar->SetValue(old_scroll_val + scrollbar->small_step, TRUE);
				handled = TRUE; // We don't want to bubble up to the next action
				break;
		};
		if (scrollbar->GetValue() != old_scroll_val)
			scrolled = TRUE;
	}

	if (handled)
	{
		if (!scrolled)
			ScrollIfNeeded();

		// Invoke & Update screen
		if (update)
		{
			if (listener)
			{
#ifdef MOUSELESS
				if (GetFormObject())
				{
					// Emulate a mouseclick to send onclick, since some webpages has functionality on onclick.
					listener->OnMouseEvent(this, GetSelectedItem(), 0, 0, MOUSE_BUTTON_1, TRUE, 1);
					listener->OnMouseEvent(this, GetSelectedItem(), 0, 0, MOUSE_BUTTON_1, FALSE, 1);
				}
#endif
				listener->OnChange(this); // Invoke listener!
			}

			if (ih.is_multiselectable == FALSE && old_focused_item != ih.focused_item)
			{
				// Invalidate the old and the new focused item
				int y = ih.GetItemYPos(old_focused_item) - scrollbar->GetValue();
				Invalidate(OpRect(0, y, 10000, ih.highest_item + 2));
				y = ih.GetItemYPos(ih.focused_item) - scrollbar->GetValue();
				Invalidate(OpRect(0, y, 10000, ih.highest_item + 2));
			}
			else
				Invalidate(GetBounds());
		}
	}

	//if (!action->IsRangeAction()) // Doesn't work anymore.. something bad in inputmanager
	if (!(vis_dev->view && vis_dev->view->GetShiftKeys() & SHIFTKEY_SHIFT))
		shiftdown_item = -1;

	return handled;
}

#ifndef MOUSELESS

void OpListBox::OnMouseMove(const OpPoint &point)
{
	if (IsDead())
		return;

	if (is_grab_n_scrolling)
	{
		int threshold = 7;
		if (op_abs(g_widget_globals->start_point.x - point.x) > threshold || op_abs(g_widget_globals->start_point.y - point.y) > threshold)
		{
			is_grab_n_scrolling = 2;

			int dy = point.y - g_widget_globals->last_mousemove_point.y;
			scrollbar->SetValue(scrollbar->GetValue() - dy);
			scrollbar->PrepareAutoScroll(-dy);
		}
		return;
	}

	if (mousedown_item != -1 ||
#ifdef PAN_SUPPORT
		(hot_track && !g_widget_globals->is_down))
#else
		hot_track) // We are selecting items
#endif // PAN_SUPPORT
	{
		OpRect border_rect;
		GetInfo()->AddBorder(this, &border_rect);
		// See if we need to scroll
		BOOL mousedown = vis_dev->view->GetMouseButton(MOUSE_BUTTON_1);
		if (mousedown && point.y < 0)
		{
			timer_event = TIMER_EVENT_SCROLL_UP;
			OnTimer();
			StartTimer(GetInfo()->GetScrollDelay(FALSE, FALSE));
		}
		else if (mousedown && point.y > GetBounds().height)
		{
			timer_event = TIMER_EVENT_SCROLL_DOWN;
			OnTimer();
			StartTimer(GetInfo()->GetScrollDelay(FALSE, FALSE));
		}
		else
			StopTimer();

		// Calculate index
		int nr = ih.FindItemNrAtPosition(point.y - border_rect.y + scrollbar->GetValue());
		if (nr < 0 || nr >= CountItems())
			return;

		if (mouseover_item == nr) // Avoid do stuff if we haven't moved to another item.
			return;

		INT32 old_mouseover_item = mouseover_item;
		INT32 old_focused_item = ih.focused_item;
		mouseover_item = nr;
		is_dragging = TRUE;

		OpStringItem* item = ih.GetItemAtNr(nr);
		if (ih.is_multiselectable && nr != mousedown_item)
		{
			MoveFocus(mousedown_item, nr, TRUE);
		}
		else if (item && item->IsEnabled()) // Select one item
		{
			if(!ih.is_multiselectable)
				ih.SelectItem(nr, TRUE);
			else
			{
				ih.SetAll(FALSE);
				ih.SelectItem(nr, TRUE);
			}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		}
		// Invalidate the areas for the changed items
		BOOL big_change = ih.is_multiselectable && op_abs(nr - mousedown_item) > 1; // changed many items at once
		if (big_change)
			Invalidate(GetBounds());
		else
		{
			int yofs = border_rect.y;
			yofs -= scrollbar->GetValue();
			OpRect r1(0, yofs + ih.GetItemYPos(old_mouseover_item), GetBounds().width, ih.highest_item);
			OpRect r2(0, yofs +     ih.GetItemYPos(mouseover_item), GetBounds().width, ih.highest_item);
			OpRect r3(0, yofs +   ih.GetItemYPos(old_focused_item), GetBounds().width, ih.highest_item);
			if (old_mouseover_item != -1)
				Invalidate(r1);
			if (mouseover_item != -1)
				Invalidate(r2);
			Invalidate(r3);
		}
	}
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

void OpListBox::AccessibilitySendEvent(Accessibility::Event evt)
{
	if (evt.is_state_event)
		evt.event_info.state_event &= ~Accessibility::kAccessibilityStateFocused;

	OpWidget::AccessibilitySendEvent(evt);
}

void OpListBox::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OpWidget::OnKeyboardInputGained(new_input_context, old_input_context, reason);
	AccessibilityManager::GetInstance()->SendEvent(ih.GetAccessibleFocusedChildOrSelf(), Accessibility::Event(Accessibility::kAccessibilityStateFocused));
}

void OpListBox::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	OpAccessibleItem* item;
	if (ih.CountItems() == 0 || ih.focused_item == 0)
		item = &ih;
	else
		item = ih.GetItemAtIndex(ih.focused_item);

	AccessibilityManager::GetInstance()->SendEvent(item, Accessibility::Event(Accessibility::kAccessibilityStateFocused));
	OpWidget::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

void OpListBox::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OnMouseDown(point, button, nclicks, TRUE);
}

void OpListBox::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, BOOL send_event)
{
	if (button != MOUSE_BUTTON_1 || IsDead())
		return;

#ifdef WIDGETS_IMS_SUPPORT
#ifdef NEARBY_ELEMENT_DETECTION
	if (!ElementExpander::IsEnabled())
#endif //NEARBY_ELEMENT_DETECTION
	{
		// Only use old code if ERR_NOT_SUPPORTED is returned, otherwise
		// platform might have failed to open IMS but we still don't want
		// core to try
		if (StartIMS() != OpStatus::ERR_NOT_SUPPORTED)
			return;
	}
#endif //WIDGETS_IMS_SUPPORT

	g_widget_globals->is_down = TRUE;
	is_dragging = FALSE;

	if (!GetFormObject())
		SetFocus(FOCUS_REASON_MOUSE);

	g_widget_globals->had_selected_items = FALSE;

	INT32 num_items = CountItems();
	if (num_items == 0) // Has no items
		return;

	if (IsSelected(ih.focused_item))
		g_widget_globals->had_selected_items = TRUE;

	mouseover_item = -1;
	shiftdown_item = -1;

	if (grab_n_scroll)
	{
		scrollbar->PrepareAutoScroll(0);
		is_grab_n_scrolling = 1;
		return;
	}

	int nr = FindItemAtPosition(point.y);
	if (nr < 0 || nr >= num_items)
		return;

	BOOL shift = (vis_dev->view->GetShiftKeys() & SHIFTKEY_SHIFT) ? TRUE : FALSE;
	BOOL control = (vis_dev->view->GetShiftKeys() & SHIFTKEY_CTRL) ? TRUE : FALSE;

	OpStringItem* item = ih.GetItemAtNr(nr);
	if (control)
	{
		anchor_index = nr;

		if (item->IsEnabled())
		{
			if (ih.is_multiselectable)
				ih.SelectItem(nr, !item->IsSelected());
			else
				ih.SelectItem(nr, TRUE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		}
		Invalidate(GetBounds());
	}
	else
	{
		if (ih.is_multiselectable)
		{
			if( shift)
			{
				INT32 num_items = CountItems();
				INT32 start = MAX(0,MIN(anchor_index,nr));
				INT32 stop  = MIN(num_items,MAX(anchor_index,nr));

				for( INT32 i=0; i<num_items; i++ )
				{
					if (IsEnabled(i))
						SelectItem(i, i >= start && i <= stop);
				}
				ih.focused_item = nr;
			}
			else
			{
				anchor_index = nr;
				ih.focused_item = nr;
#ifdef WIDGETS_CHECKBOX_MULTISELECT
				if (item->IsEnabled())
					ih.SelectItem(nr, !item->IsSelected());
#else
				mousedown_item = ih.focused_item;
				OnMouseMove(point);
#endif
			}
		}
		else
		{
			anchor_index = nr;
			mousedown_item = ih.focused_item;
			OnMouseMove(point);
		}

		Invalidate(GetBounds());
	}

	if (listener && send_event)
		listener->OnMouseEvent(this, nr, point.x, point.y, button, TRUE, nclicks); // Invoke listener!

#ifdef PAN_SUPPORT
	if (vis_dev->GetPanState() != NO)
	{
		// hijack mouseover_item to hold "real" down item when potentially panning
		mouseover_item = mousedown_item;
		// wonko: reset so we don't trigger select on mouse move
		mousedown_item = -1;
		StopTimer();
		// g_widget_globals->is_down will be reset by panning code
	}
#elif defined GRAB_AND_SCROLL
	if (MouseListener::GetGrabScrollState() != NO)
		OnMouseUp(point, button, nclicks);
#endif
}

void OpListBox::OnMouseLeave()
{
	g_widget_globals->is_down = FALSE;
}

void OpListBox::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1 || !g_widget_globals->is_down || IsDead())
		return;

	g_widget_globals->is_down = FALSE;

#ifdef PAN_SUPPORT
	// WONKO: the panning code sets mousedown_item to -1 on mouse down
	// so as not to trigger selection on mouse move. this will cause
	// OnChange when the selected element is clicked. we don't want
	// that - see bug #321053.
	if (g_widget_globals->had_selected_items && mousedown_item == -1)
		mousedown_item = mouseover_item;
#endif // PAN_SUPPORT

	INT32 tmp = mousedown_item;
	mousedown_item = -1;
	mouseover_item = -1;
	StopTimer();

	INT32 num_items = CountItems();
	if (num_items == 0) // Has no items
		return;

	OpRect border_rect;
	GetInfo()->AddBorder(this, &border_rect);

	OpRect rect = GetBounds();
	if( !(is_grab_n_scrolling || is_dragging) && !rect.Contains(point) )
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_DROPDOWN);
		return;
	}

	int nr;
	if (hot_track)
		nr = ih.focused_item;
	else
		nr = ih.FindItemNrAtPosition(point.y - border_rect.y + scrollbar->GetValue());

	if (is_grab_n_scrolling)
	{
		scrollbar->AutoScroll(scrollbar->GetValue());
		OpStringItem* item = ih.GetItemAtNr(nr);
		if (is_grab_n_scrolling == 1 && item && item->IsEnabled())
		{
			// Event for mousedown wasn't done in OnMouseDown if we started grab'n'scroll, so trig it here.
			if (ih.is_multiselectable)
				ih.SelectItem(nr, !item->IsSelected());
			else
				ih.SelectItem(nr, TRUE);
			Invalidate(GetBounds());

			is_grab_n_scrolling = 0;
			if (nr >= 0 && nr < num_items && listener)
				listener->OnMouseEvent(this, nr, point.x, point.y, button, TRUE, nclicks); // Invoke listener!
		}
		else
		{
			is_grab_n_scrolling = 0;
			return;
		}
	}

	if (listener)
	{
		if (nr >= 0 && nr < num_items)
			listener->OnMouseEvent(this, nr, point.x, point.y, button, FALSE, nclicks); // Invoke listener!

		if ((nr != tmp || !g_widget_globals->had_selected_items) || ih.is_multiselectable)
			listener->OnChange(this); // Invoke the listener
	}
}

#endif // !MOUSELESS

void OpListBox::OnTimer()
{
	if (timer_event == TIMER_EVENT_SCROLL_X)
	{
		x_scroll += 2;

		if (ih.focused_item >= 0 && ih.focused_item < CountItems())
		{
			OpStringItem* focused_item = ih.GetItemAtNr(ih.focused_item);
			if (focused_item && x_scroll > focused_item->string.GetWidth() - GetBounds().width + 30) // 30 to stop at end for a while.
				x_scroll = 0;

			int y = ih.GetItemYPos(ih.focused_item) - scrollbar->GetValue();
			Invalidate(OpRect(0, y, 10000, ih.highest_item + 2));

			StartTimer(GetInfo()->GetScrollDelay(FALSE, FALSE));
		}
		return;
	}

	int step = ((OpScrollbar*)scrollbar)->small_step;
	if (timer_event == TIMER_EVENT_SCROLL_UP)
		step = -step;
	scrollbar->SetValue(scrollbar->GetValue() + step);
}

BOOL OpListBox::IsScrollable(BOOL vertical)
{
	if(vertical)
		return scrollbar->CanScroll(ARROW_UP) || scrollbar->CanScroll(ARROW_DOWN);
	return FALSE;
}

BOOL OpListBox::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	return vertical && scrollbar->OnScroll(delta, vertical, smooth);
}

#ifndef MOUSELESS

BOOL OpListBox::OnMouseWheel(INT32 delta, BOOL vertical)
{
	if (!vertical)
		return FALSE;

	return scrollbar->OnMouseWheel(delta, vertical);
}

#endif // !MOUSELESS

INT32 OpListBox::GetVisibleItems()
{
	OpRect rect = GetRect(TRUE);
	GetInfo()->AddBorder(this, &rect);
	if (ih.highest_item)
		return rect.height / ih.highest_item;
	return 0;
}

void OpListBox::SetScrollbarColors(const ScrollbarColors &colors)
{
	scrollbar->scrollbar_colors = colors;
}

int OpListBox::FindItemAtPosition(int y, BOOL always_get_option/* = FALSE*/)
{
	OpRect border_rect;
	GetInfo()->AddBorder(this, &border_rect);
	y = y - border_rect.y + scrollbar->GetValue();
	return ih.FindItemNrAtPosition(y, always_get_option);
}

BOOL OpListBox::MoveFocus(INT32 from_nr, INT32 nr, BOOL select)
{
			BOOL result = ih.MoveFocus(from_nr, nr, select);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			if (result && (!ih.is_multiselectable
#ifndef WIDGETS_CHECKBOX_MULTISELECT
				|| select
#endif
				))
				AccessibilityManager::GetInstance()->SendEvent(&ih, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
			return result;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OP_STATUS OpListBox::GetAbsolutePositionOfList(OpRect &rect)
{
	OpRect myRect;
	RETURN_IF_ERROR(AccessibilityGetAbsolutePosition(myRect));
	rect.x = myRect.x;
	rect.y = myRect.y - scrollbar->GetValue();
	rect.width = myRect.width - GetInfo()->GetVerticalScrollbarWidth();
	rect.height = ih.GetTotalHeight();
	return OpStatus::OK;
}

OP_STATUS OpListBox::ScrollItemTo(INT32 item, Accessibility::ScrollTo scroll_to)
{
	OpRect inner_rect = GetBounds();
	GetInfo()->AddBorder(this, &inner_rect);
	int y = ih.GetItemYPos(item) - scrollbar->GetValue();
	if (scroll_to == Accessibility::kScrollToTop || scroll_to == Accessibility::kScrollToAnywhere && y < 0) // upper edge above top border
		scrollbar->SetValue(scrollbar->GetValue() + y);
	else if (scroll_to == Accessibility::kScrollToBottom || scroll_to == Accessibility::kScrollToAnywhere && y + ih.highest_item > inner_rect.height) // lower edge below bottom border
		scrollbar->SetValue(scrollbar->GetValue() + (y - inner_rect.height) + ih.highest_item);

	return OpStatus::OK;
}

BOOL OpListBox::IsListFocused() const
{
	return this == g_input_manager->GetKeyboardInputContext();
}

BOOL OpListBox::IsListVisible() const
{
	return this->IsVisible();
}

int OpListBox::GetAccessibleChildrenCount()
{
	return (HasScrollbar()?2:1);
}

OpAccessibleItem* OpListBox::GetAccessibleChild(int child_nr)
{
	if (child_nr == 0)
		return &ih;
	if (child_nr == 1 && HasScrollbar())
		return scrollbar;

	return NULL;
}

int OpListBox::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	if (child == &ih)
		return 0;
	if (HasScrollbar() && child == scrollbar)
		return 1;

	return Accessibility::NoSuchChild;
}

OpAccessibleItem* OpListBox::GetAccessibleChildOrSelfAt(int x, int y)
{
	if (scrollbar->GetAccessibleChildOrSelfAt(x,y))
		return scrollbar;
	if (ih.GetAccessibleChildOrSelfAt(x,y))
		return &ih;

	return NULL;
}

OpAccessibleItem* OpListBox::GetAccessibleFocusedChildOrSelf()
{
	if (IsListFocused())
		return &ih;

	return NULL;
}

Accessibility::State OpListBox::AccessibilityGetState()
{
	return OpWidget::AccessibilityGetState() & ~Accessibility::kAccessibilityStateFocused;
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

void OpListBox::GetRelevantOptionRange(const OpRect& area, UINT32& start, UINT32& stop)
{
	OpRect r = GetDocumentRect();
	GetInfo()->AddBorder(this, &r);

	// area doesn't intersects us at all
	if (!r.Intersecting(area))
	{
		start = stop = 0;
		return;
	}

	// no items in widget
	INT32 count = ih.CountItems();
	if (!count || ih.highest_item <= 0)
	{
		start = stop = 0;
		return;
	}

	// get items in rect, clipping to widget rect so as not to get too many
	INT32 f = FindItemAtPosition(MAX(0, area.y - r.y), TRUE);
	INT32 l = FindItemAtPosition(MIN(r.height, area.y + area.height - r.y + 1), TRUE);

	// sanity checks
	OP_ASSERT(f >= 0);
	OP_ASSERT(l >= f);
	OP_ASSERT(l < count);

	start = (UINT32)f;
	stop = (UINT32)l+1; // range is [start, stop[
}

#ifdef WIDGETS_IMS_SUPPORT
void OpListBox::DoCommitIMS(OpIMSUpdateList *updates)
{
	int idx;
	int nr = -1;
	int first_item = -1;
	BOOL was_updated = FALSE;

	Invalidate(GetBounds());

	for(idx = updates->GetFirstIndex(); idx >= 0; idx = updates->GetNextIndex(idx))
	{
		OpStringItem* item = ih.GetItemAtIndex(idx);

		if (!item->IsEnabled())
			continue;

		int value = updates->GetValueAt(idx);
		nr = ih.FindItemNr(idx);
		was_updated = was_updated || ih.IsSelected(nr) ^ (value == 1);
		ih.SelectItem(nr, value != 0);

		if (listener)
		{
			if (GetFormObject())
			{
				// Emulate a mouseclick to send onclick, since some webpages has functionality on onclick.
				listener->OnMouseEvent(this, GetSelectedItem(), 0, 0, MOUSE_BUTTON_1, TRUE, 1);
				listener->OnMouseEvent(this, GetSelectedItem(), 0, 0, MOUSE_BUTTON_1, FALSE, 1);
			}
			if (was_updated)
			{
				listener->OnChange(this); // Invoke listener!
			}
		}

		if (first_item == -1)
			first_item = nr;
	}

	if (nr == -1)
		return;

	ih.focused_item = first_item;
	ScrollIfNeeded();
}

/* virtual */
void OpListBox::DoCancelIMS()
{
	// Do nothing right now.
}

#endif // WIDGETS_IMS_SUPPORT
