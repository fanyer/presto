/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/ItemDrawArea.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModelItem.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/Column.h"

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TreeViewModelItem::Clear()
{
    if (m_column_cache)
    {
		RemoveCustomCellWidget();

		OP_DELETEA(m_column_cache);
		m_column_cache = NULL;
		OP_DELETE(m_associated_text);
		m_associated_text = NULL;
		OP_DELETE(m_associated_image);
		m_associated_image = NULL;
		OP_DELETE(m_widget_image);
		m_widget_image = NULL;
    }
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
ItemDrawArea* TreeViewModelItem::GetAssociatedText(OpTreeView* view)
{
	ItemData item_data(ASSOCIATED_TEXT_QUERY);

	OpString string;
	item_data.associated_text_query_data.text = &string;
	item_data.column_query_data.column_text_color = OP_RGB(0, 0, 0);

	if(!m_associated_text)
		GetItemData(&item_data);

	if(item_data.associated_text_query_data.text->HasContent())
	{
		if(!m_associated_text)
			m_associated_text = OP_NEW(ItemDrawArea, ());

		if(m_associated_text)
		{
			// Maybe this shouldn't be ignored?
			OpStatus::Ignore(SetDrawAreaText(view, string, m_associated_text));
		
			m_associated_text->m_color = item_data.associated_text_query_data.text_color;
			m_associated_text->m_indentation = item_data.associated_text_query_data.associated_text_indentation;
			m_associated_text->m_flags = item_data.flags;
		}
	}
	return m_associated_text;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
ItemDrawArea* TreeViewModelItem::GetAssociatedImage(OpTreeView* view)
{
	ItemData item_data(ASSOCIATED_IMAGE_QUERY, view);

	OpString8 string;
	item_data.associated_image_query_data.image = &string;

	GetItemData(&item_data);

	if(item_data.associated_image_query_data.image->HasContent())
	{
		if(!m_associated_image)
			m_associated_image = OP_NEW(ItemDrawArea, ());

		if(m_associated_image)
		{
			if(OpStatus::IsSuccess(m_associated_image_skin_elm.Set(item_data.associated_image_query_data.image->CStr())))
			{
				m_associated_image->m_image = m_associated_image_skin_elm.CStr();
			}
			m_associated_image->m_image_state = item_data.associated_image_query_data.skin_state;
		}
	}
	else
	{
		OP_DELETE(m_associated_image);
		m_associated_image = NULL;
	}

	return m_associated_image;
}

const char *TreeViewModelItem::GetSkin(OpTreeView* view)
{
	ItemData item_data(SKIN_QUERY, view);

	item_data.flags = m_flags;

	GetItemData(&item_data);

	return item_data.skin_query_data.skin;
}

/***********************************************************************************
 **
 **	GetItemDrawArea
 **
 ***********************************************************************************/
ItemDrawArea* TreeViewModelItem::GetItemDrawArea(OpTreeView* view,
												 INT32 column,
												 BOOL paint)
{
    INT32 count = view->GetTreeModel()->GetColumnCount();

    if (m_column_cache == NULL)
    {
		m_column_cache = OP_NEWA(ItemDrawArea, count);

		if (!m_column_cache)
		{
			return NULL;
		}
    }

    ItemDrawArea* cache = &m_column_cache[column];

    if (!(cache->m_flags & FLAG_FETCHED) || (paint && cache->m_flags & FLAG_NO_PAINT))
    {
		ItemData item_data(COLUMN_QUERY,view);
		OpString string;

		item_data.column_query_data.column = column;
		item_data.column_query_data.column_text = &string;
		item_data.column_query_data.column_text_color = OP_RGB(0, 0, 0);
		item_data.flags = m_flags;
		item_data.column_query_data.custom_cell_widget = cache->m_custom_cell_widget;

		if (!paint)
		{
			item_data.flags |= FLAG_NO_PAINT;
		}

		GetItemData(&item_data);

		// The disabled state needs to be propagated to the treeviewmodelitem, since it
		// influences the behavior of the item (not just drawing behavior)
		if (item_data.flags & FLAG_DISABLED)
			m_flags |= FLAG_DISABLED;
		else
			m_flags &= ~FLAG_DISABLED;

		if (item_data.flags & FLAG_CUSTOM_CELL_WIDGET)
			m_flags |= FLAG_CUSTOM_CELL_WIDGET;
		else
			m_flags &= ~FLAG_CUSTOM_CELL_WIDGET;

		// Formatting can be changed in an extisting item
		if (item_data.flags & FLAG_FORMATTED_TEXT)
			m_flags |= FLAG_FORMATTED_TEXT;
		else
			m_flags &= ~FLAG_FORMATTED_TEXT;

		cache->m_image = item_data.column_query_data.column_image;
		cache->m_image_2 = item_data.column_query_data.column_image_2;
		cache->m_color = item_data.column_query_data.column_text_color;
		cache->m_sort_order = item_data.column_query_data.column_sort_order;
		cache->m_flags = item_data.flags;
		cache->m_progress = item_data.column_query_data.column_progress;
		cache->m_indentation = item_data.column_query_data.column_indentation;
		cache->m_column_span = item_data.column_query_data.column_span;
		cache->m_custom_cell_widget = item_data.column_query_data.custom_cell_widget;

		OpStatus::Ignore(SetDrawAreaText(view, string, cache));

		cache->m_string_prefix_length = item_data.column_query_data.column_text_prefix;
		cache->m_flags |= FLAG_FETCHED;
		cache->m_bitmap_image = item_data.column_bitmap;
    }

    return cache;
}

/***********************************************************************************
 **
 **	SetDrawAreaText
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::SetDrawAreaText(OpTreeView* view,
											 OpString & string,
											 ItemDrawArea* cache)
{
	// break at newline, but not until we have something to display (so
	// we want to skip leading newlines).
	uni_char* cur_pos = string.CStr();
	uni_char* start_pos = cur_pos;

	while (cur_pos && *cur_pos)
	{
		const BOOL is_newline = (*cur_pos == '\n' || *cur_pos == '\r');

		if (cur_pos > start_pos && is_newline)
		{
			*cur_pos = 0;
			break;
		}
		else if (cur_pos == start_pos && is_newline)
			start_pos = cur_pos + 1;

		cur_pos++;
	}

	if (IsFormattedText())
		return ParseFormattedText(view, cache, start_pos);
	else
		return cache->m_strings.m_string.Set(start_pos, view);

}

/***********************************************************************************
 **
 **	ParseFormattedText
 **
 ** Checks the string for some predefined HTML tags:
 ** <i></i> Italic
 ** <b></b> Bold
 ** <o></o> Overstrike
 ** <c=353253></c> Colored text
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::ParseFormattedText(OpTreeView* view,
												ItemDrawArea* cache,
												const OpStringC& formatted_text)
{
    if (formatted_text.IsEmpty())
		return OpStatus::OK;

    ItemDrawArea::StringList* current_string = &cache->m_strings;
    ItemDrawArea::StringList* prev_string    = current_string;
    const uni_char*   text_ptr       = formatted_text.CStr();
    const uni_char*   next_ptr       = uni_strchr(text_ptr, '<');
    OpString		  text_part;
    int				  color = 0;

	// Cleanup previous strings
    OP_DELETE(current_string->m_next);
    current_string->m_next = NULL;

    if (!text_part.Reserve(formatted_text.Length()))
		return OpStatus::ERR_NO_MEMORY;

	int count = 0;
	int string_flags = 0;
	bool use_line_through = false;
	bool use_new = true;
    // See if we found a tag
    while (text_ptr && *text_ptr)
    {
		use_new = true;
		// Make new string if necessary
		if (!current_string)
		{
			current_string = OP_NEW(ItemDrawArea::StringList, ());
			if (!current_string)
				return OpStatus::ERR_NO_MEMORY;

			prev_string->m_next = current_string;
			prev_string = current_string;
		}
		// FIXME: The current highlithing approach is incompatible with BiDi.
		// Force LTR for now, fix DSK-359759 better later.
		current_string->m_string.SetForceLTR(TRUE);

		if (next_ptr != text_ptr)
		{
			// get the text
			if (next_ptr)
			{
				RETURN_IF_ERROR(text_part.Set(text_ptr, next_ptr - text_ptr));
				RETURN_IF_ERROR(current_string->m_string.Set(text_part.CStr(), view));
			}
			else
			{
				RETURN_IF_ERROR(current_string->m_string.Set(text_ptr, view));
			}

			text_ptr = next_ptr;
		}
		else if (*text_ptr == UNI_L('<'))
		{
			// get the tag
			if (*(text_ptr + 1) != '\0' && *(text_ptr + 2) == UNI_L('>'))
			{
				switch(*(text_ptr + 1))
				{
					case UNI_L('b'):
						string_flags |= ItemDrawArea::FLAG_BOLD;
						break;
					case UNI_L('i'):
						string_flags |= ItemDrawArea::FLAG_ITALIC;
						break;
					case UNI_L('o'):
						use_line_through = true; 
						break;
					default:
						OP_ASSERT(!"Unknown tag");
				}
				text_ptr = text_ptr + 3;
				next_ptr = text_ptr;
			}
			else if (*(text_ptr + 1) == UNI_L('/') && *(text_ptr + 2) != '\0' && *(text_ptr + 3) == UNI_L('>'))
			{
				switch(*(text_ptr + 2))
				{
					case 'b':
						string_flags &= ~ItemDrawArea::FLAG_BOLD;
						break;
					case 'i':
						string_flags &= ~ItemDrawArea::FLAG_ITALIC;
						break;
					case 'c':
						string_flags &= ~ItemDrawArea::FLAG_COLOR;
						break;
					case 'o':
						use_line_through = false; 
						break;
					default:
						OP_ASSERT(!"Unknown tag");
				}
				string_flags &= ~ItemDrawArea::FLAG_BOLD;
				text_ptr = text_ptr + 4;
				next_ptr = text_ptr;
			}
			else if (*(text_ptr + 1) == UNI_L('c') && *(text_ptr + 2) == UNI_L('=') &&
					uni_sscanf(text_ptr+3, UNI_L("%d>%n"), &color, &count) == 1 )
			{
				string_flags |= ItemDrawArea::FLAG_COLOR;

				text_ptr += 3 + count;
				next_ptr =  text_ptr;
			}
			else
			{
				next_ptr++;
			}

			use_new = false;
		}

		if (current_string && use_new)
		{
			current_string->m_string_flags = string_flags;

			if (use_line_through)
				current_string->m_string.SetTextDecoration(WIDGET_LINE_THROUGH, TRUE);

			if (string_flags & ItemDrawArea::FLAG_COLOR)
				current_string->m_string_color = color;
		}

		if (use_new)
			current_string = NULL;

		if (next_ptr)
			next_ptr = uni_strchr(next_ptr, '<');
    }

    return OpStatus::OK;
}

/***********************************************************************************
 **
 **	TreeViewModelItem::OnAdded
 **
 ***********************************************************************************/
void TreeViewModelItem::OnAdded()
{
    TreeViewModel* model = GetModel();

    if (model->m_resorting)
		return;

    model->m_hash_table.Add(m_model_item, this);
}

/***********************************************************************************
 **
 **	TreeViewModelItem::OnRemoving
 **
 ***********************************************************************************/
void TreeViewModelItem::OnRemoving()
{
    TreeViewModel* model = GetModel();

    if (model->m_resorting)
		return;

    void* data = NULL;

    model->m_hash_table.Remove(m_model_item, &data);


	if (IsCustomWidget() && model->GetView())
	{
		model->GetView()->OnCustomWidgetItemRemoving(this);
	}

	RemoveCustomCellWidget();
}

/***********************************************************************************
 **
 ** RemoveCustomCellWidget
 **
 ***********************************************************************************/

void TreeViewModelItem::RemoveCustomCellWidget()
{
	TreeViewModel* model = GetModel();

	if (HasCustomCellWidget() && model && model->GetView() && m_column_cache)
	{
		INT32 count = model->GetView()->GetColumnCount();
		for (INT32 i = 0; i < count; i++)
		{
			if (m_column_cache[i].m_custom_cell_widget)
			{
				model->GetView()->OnCustomWidgetItemRemoving(this, m_column_cache[i].m_custom_cell_widget);
			}
		}
	}
}
/***********************************************************************************
 **
 **	TreeViewModelItem::~TreeViewModelItem
 **
 ***********************************************************************************/
TreeViewModelItem::~TreeViewModelItem()
{
    Clear();
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
    OP_DELETE(m_text_objects);
#endif
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TreeViewModelItem::SetViewModel(TreeViewModel* view_model)
{
    m_view_model = view_model;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::CreateTextObjectsIfNeeded()
{
	if (!m_text_objects)
	{
		m_text_objects = OP_NEW(OpAutoVector<AccessibleTextObject>, ());
		if (!m_text_objects)
			return OpStatus::ERR;
	}

	int count = m_view_model->GetModel()->GetColumnCount();
	for (INT32 i = 0; i < count; i++)
	{
		AccessibleTextObject* obj = OP_NEW(AccessibleTextObject, (this, i));
		if (obj)
			m_text_objects->Add(obj);
	}
	return OpStatus::OK;
}

OP_STATUS TreeViewModelItem::AccessibilityClicked()
{
//	m_view_model->SetFocusToItem(GetIndex());
	OpTreeView* view = m_view_model->GetView();
	INT32 index = GetIndex();
	if (index >= 0 && view)
	{
		view->SetFocus(FOCUS_REASON_OTHER);
		if (CanOpen())
		{
			view->OpenItem(index, !IsOpen());
		}
		view->SelectItem(index, FALSE, FALSE, FALSE, TRUE);
		if (view->GetListener())
		{
			view->GetListener()->OnMouseEvent(view, index, -1, -1, MOUSE_BUTTON_1, TRUE, 1);
			view->GetListener()->OnMouseEvent(view, index, -1, -1, MOUSE_BUTTON_1, FALSE, 1);
		}
	}
    return OpStatus::OK;
}
/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL TreeViewModelItem::AccessibilitySetFocus()
{
    m_view_model->SetFocusToItem(GetIndex());
    return TRUE;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::AccessibilityGetText(OpString& str)
{
    // implement: find important column
    return GetTextOfObject(0, str);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::AccessibilityGetDescription(OpString& str)
{
//	switch (GetType())
//	{
//		case FOLDER_TYPE:
//			return g_languageManager->GetString(Str::S_FOLDER_TYPE, str);
//		default:
    str.Empty();
    return OpStatus::ERR;
//	}
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::AccessibilityGetAbsolutePosition(OpRect &rect)
{
    OpRect off_rect;
    int index = GetIndex();
    m_view_model->GetView()->GetItemRect(index, rect, FALSE);
    m_view_model->GetView()->AccessibilityGetAbsolutePosition(off_rect);
    rect.x += off_rect.x;
    rect.y += off_rect.y;
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::AccessibilityGetValue(int &value)
{
    int index = GetIndex();
    value = -1;
    do {
		value++;
		index = m_view_model->GetView()->GetParent(index);
    } while (index >= 0);
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL TreeViewModelItem::AccessibilitySetExpanded(BOOL expanded)
{
    if (CanOpen())
    {
		int index = GetIndex();
		m_view_model->GetView()->OpenItem(index, expanded);
		return TRUE;
    }
    return FALSE;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
Accessibility::ElementKind TreeViewModelItem::AccessibilityGetRole() const
{
    return Accessibility::kElementKindListRow;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
Accessibility::State TreeViewModelItem::GetUnfilteredState()
{
	Accessibility::State state = m_view_model->GetUnfilteredState();

    if (IsSelected())
    {
		state |= Accessibility::kAccessibilityStateCheckedOrSelected;
    }
    if (!IsFocused())
    {
		state &= ~Accessibility::kAccessibilityStateFocused;
    }
    if (CanOpen())
    {
		state |= Accessibility::kAccessibilityStateExpandable;
		if (IsOpen())
		{
			state |= Accessibility::kAccessibilityStateExpanded;
		}
    }
    if (!m_view_model->GetView()->IsItemVisible(GetIndex()))
    {
		state |= Accessibility::kAccessibilityStateInvisible;
    }
    return state;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
Accessibility::State TreeViewModelItem::AccessibilityGetState()
{
    return GetUnfilteredState();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
int TreeViewModelItem::GetAccessibleChildrenCount()
{
    int n = m_view_model->GetView()->GetColumnCount();
    int children = 0;
    for (int i = 0; i < n; i++)
    {
		OpTreeView::Column* col = m_view_model->GetView()->GetColumnByIndex(i);
		if (col->IsColumnVisible())
			children++;
    }

    return children;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* TreeViewModelItem::GetAccessibleChild(int child_nr)
{
    int n = m_view_model->GetView()->GetColumnCount();
    int children = -1;
    int i;
    for (i = 0; i < n && children < child_nr; i++)
    {
		OpTreeView::Column* col = m_view_model->GetView()->GetColumnByIndex(i);
		if (col->IsColumnVisible())
			children++;
    }

    if (children < child_nr)
		return NULL;

	if (OpStatus::IsError(this->CreateTextObjectsIfNeeded()))
		return NULL;
    AccessibleTextObject* obj = m_text_objects ? m_text_objects->Get(i-1) : NULL;
    if (obj)
		return obj;
    return NULL;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/

int TreeViewModelItem::GetAccessibleChildIndex(OpAccessibleItem* child)
{
    int n = m_view_model->GetView()->GetColumnCount();
    int children = -1;
    int i;
    for (i = 0; i < n; i++)
    {
		OpTreeView::Column* col = m_view_model->GetView()->GetColumnByIndex(i);
		if (col->IsColumnVisible())
		{
			if (child == m_text_objects->Get(i-1))
				return children;
			children++;
		}
    }

	return Accessibility::NoSuchChild;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* TreeViewModelItem::GetAccessibleChildOrSelfAt(int x, int y)
{
    if (m_view_model)
    {
		int i = m_view_model->GetView()->GetColumnAt(x, y);
		if (OpStatus::IsError(this->CreateTextObjectsIfNeeded()))
			return NULL;
		AccessibleTextObject* obj = m_text_objects ? m_text_objects->Get(i) : NULL;
		if (obj)
			return obj;
    }

    return this;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* TreeViewModelItem::GetAccessibleFocusedChildOrSelf()
{
    if (IsFocused())
    {
		if (OpStatus::IsError(this->CreateTextObjectsIfNeeded()))
			return NULL;
		AccessibleTextObject* obj = m_text_objects ? m_text_objects->Get(m_focused_col) : NULL;
		if (obj)
			return obj;
		return this;
    }
    return NULL;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::GetAbsolutePositionOfObject(int i, OpRect& rect)
{
    OpRect parent_rect;
    OpRect column_rect;
    AccessibilityGetAbsolutePosition(parent_rect);
    OpTreeView::Column* column = m_view_model->GetView()->GetColumnByIndex(i);
    column->AccessibilityGetAbsolutePosition(column_rect);

    rect.y = parent_rect.y;
    rect.height= parent_rect.height;
    if (column_rect.IsEmpty())
    {
		rect.x = parent_rect.x;
		rect.width = column->m_width;
    }
    else
    {
		rect.x = column_rect.x;
		rect.width = column_rect.width;
    }
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::SetFocusToObject(int i)
{
    AccessibilitySetFocus();
    m_focused_col = i;
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::GetTextOfObject(int i, OpString& str)
{
    ItemDrawArea* item = GetItemDrawArea(m_view_model->GetView(), i, FALSE);
    if (item)
    {
		for (ItemDrawArea::StringList* string = &item->m_strings; string; string = string->m_next)
		{
			RETURN_IF_ERROR(str.Append(string->m_string.Get()));
		}
		return OpStatus::OK;
    }
    return OpStatus::ERR;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::GetDecriptionOfObject(int i, OpString& str)
{
    return AccessibilityGetDescription(str);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
Accessibility::State TreeViewModelItem::GetStateOfObject(int i)
{
	Accessibility::State state = GetUnfilteredState();
    if (m_focused_col != i)
		state &= ~Accessibility::kAccessibilityStateFocused;
    return state;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TreeViewModelItem::ObjectClicked(int i)
{
	// FIXME, if click in checkbox column, toggle checkbox.
	return AccessibilityClicked();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpAccessibleItem* TreeViewModelItem::GetAccessibleParentOfObject()
{
    return this;
}
    
#endif // ACCESSIBILITY_EXTENSION_SUPPORT
