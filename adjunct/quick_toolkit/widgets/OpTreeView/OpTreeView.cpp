/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModelItem.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/ColumnListAccessor.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/Column.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/ItemDrawArea.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/Edit.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/quick/quick-widget-names.h"

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"

#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_scope/src/scope_widget_info.h"

#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/widgets/OpWidgetStringDrawer.h"
#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/OpLineParser.h"
#include "modules/style/css.h"
#include "modules/dragdrop/dragdrop_manager.h"

// Transparent separator line colors. Ideally we would use the "Horizontal Separator" skin.
#define SEPARATOR_DARK OP_RGBA(0, 0, 0, 30)
#define SEPARATOR_BRIGHT OP_RGBA(255, 255, 255, 90)

#define ANCHOR_RESET -10
#define ANCHOR_MOVE_OFFSET 3

class OpTreeView::ScopeWidgetInfo : public OpScopeWidgetInfo
{
public:
	ScopeWidgetInfo(OpTreeView& tree) : m_tree(tree) {}
	virtual OP_STATUS AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible);
private:
	OpTreeView& m_tree;
};


/***********************************************************************************
**
**	OpTreeView
**
***********************************************************************************/
DEFINE_CONSTRUCT(OpTreeView)

OpTreeView::OpTreeView()
	: m_show_column_headers(TRUE)
	, m_is_multiselectable(FALSE)
	, m_is_deselectable_by_keyboard(TRUE)
	, m_is_deselectable_by_mouse(TRUE)
	, m_is_drag_enabled(FALSE)
	, m_show_thread_image(FALSE)
	, m_bold_folders(FALSE)
	, m_root_opens_all(FALSE)
	, m_auto_match(FALSE)
	, m_async_match(FALSE)
	, m_user_sortable(TRUE)
	, m_allow_open_empty_folder(FALSE)
	, m_reselect_when_selected_is_removed(TRUE)
	, m_expand_on_single_click(TRUE)
	, m_forced_focus(FALSE)
	, m_select_on_hover(FALSE)
	, m_allow_select_by_letter(TRUE)
	, m_force_sort_by_string(FALSE)
	, m_force_empty_match_query(FALSE)
	, m_allow_multi_line_items(FALSE)
	, m_restrict_image_size(FALSE)
	, m_allow_multi_line_icons(FALSE)
	, m_have_weak_associated_text(FALSE)
	, m_force_background_line(FALSE)
	, m_allow_wrapping_on_scrolling(FALSE)
	, m_paint_background_line(TRUE)
	, m_close_all_headers(FALSE)
#ifdef _MACINTOSH_
	, m_column_headers_height(17)
#else
	, m_column_headers_height(20)
#endif
	, m_is_drag_selecting(FALSE)
	, m_changed(false)
	, m_edit(NULL)
	, m_edit_pos(-1)
	, m_edit_column(0)
	, m_column_filler(NULL)
	, m_horizontal_scrollbar(NULL)
	, m_vertical_scrollbar(NULL)
	, m_scrollbar_listener(NULL)
	, m_show_horizontal_scrollbar(false)
	, m_drop_pos(-1)
	, m_insert_type(DesktopDragObject::INSERT_INTO)
	, m_line_height(20)
	, m_is_centering_needed(FALSE)
	, m_custom_background_line_color(-1)
	, m_extra_line_height(0)
	, m_only_show_associated_item_on_hover(FALSE)
	, m_save_strings_that_need_refresh(FALSE)
	, m_match_column(0)
	, m_match_type(MATCH_ALL)
	, m_match_parent_id(0)
	, m_visible_column_count(0)
	, m_thread_column(0)
	, m_checkable_column(-1)
	, m_link_column(-1)
	, m_column_settings_loaded(FALSE)
	, m_selected_item(-1)
	, m_selected_item_count(0)
	, m_shift_item(-1)
	, m_hover_item(-1)
	, m_hover_associated_image_item(-1)
	, m_reselect_selected_line(-1)
	, m_reselect_selected_id(0)
	, m_timed_hover_pos(-1)
	, m_timer_counter(0)
	, m_send_onchange_later(FALSE)
	, m_selected_on_mousedown(FALSE)
	, m_sort_by_column(-1)
	, m_sort_ascending(TRUE)
	, m_sort_by_string_first(FALSE)
	, m_custom_sort(FALSE)
	, m_always_use_sortlistener(FALSE)
	, m_is_sizing_column(FALSE)
	, m_sizing_column_left(-1)
	, m_sizing_column_right(-1)
	, m_start_x(0)
	, m_thread_indentation(16)
	, m_keep_visible_on_scroll(2)
	, m_item_count(0)
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, m_column_list_accessor (NULL)
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	, m_anchor(OpPoint(ANCHOR_RESET,ANCHOR_RESET))
{
	OP_STATUS status = OpScrollbar::Construct(&m_horizontal_scrollbar, true);
	CHECK_STATUS(status);
	status = OpScrollbar::Construct(&m_vertical_scrollbar, false);
	CHECK_STATUS(status);

	m_horizontal_scrollbar->SetListener(this);
	m_vertical_scrollbar->SetListener(this);

	m_redraw_trigger.SetDelayedTriggerListener(this);
	m_redraw_trigger.SetTriggerDelay(0, 300);

	AddChild(m_horizontal_scrollbar, true);
	AddChild(m_vertical_scrollbar,  true);

	GetBorderSkin()->SetImage("Treeview Skin");
	SetSkinned(TRUE);
	SetTabStop(TRUE);

	status = m_view_model.AddListener(this);
	CHECK_STATUS(status);
	m_view_model.SetView(this);

#ifdef _MACINTOSH_
	SetSystemFont(OP_SYSTEM_FONT_UI_TREEVIEW);
#endif
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void OpTreeView::OnDeleted()
{
	for (OpListenersIterator iterator(m_treeview_listeners); m_treeview_listeners.HasNext(iterator);)
	{
		m_treeview_listeners.GetNext(iterator)->OnTreeViewDeleted(this);
	}

	m_view_model.RemoveListener(this);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OP_DELETE(m_column_list_accessor);
#endif
	OpWidget::OnDeleted();
}

/***********************************************************************************
**
**	SetTreeModel
**
***********************************************************************************/
void OpTreeView::SetTreeModel(OpTreeModel* tree_model,
							  INT32 sort_by_column,
							  BOOL sort_ascending,
							  const char* name,
							  BOOL clear_match_text,
							  BOOL always_use_sortlistener,
							  OpTreeModel::TreeModelGrouping* grouping)
{
	if (tree_model == GetTreeModel())
	{
		return;
	}

	m_always_use_sortlistener = always_use_sortlistener;
	m_is_centering_needed = !IsAllVisible();
	m_column_settings_loaded = FALSE;
	m_sort_by_column = sort_by_column;
	m_sort_ascending = sort_ascending;
	m_sort_by_string_first = FALSE;
	m_custom_sort = FALSE;
	m_columns.DeleteAll();

	if (name)
	{
		OpWidget::SetName(name);
	}

	if (clear_match_text)
	{
		m_match_text.Empty();
	}

	if (tree_model)
	{
		int column_count = tree_model->GetColumnCount();

		if (m_sort_by_column >= column_count)
		{
			m_sort_by_column = -1;
		}

		for (INT32 i = 0; i < column_count; i++)
		{
			Column* column = OP_NEW(Column, (this, i));
			if (!column) break;

			m_columns.Add(column);

			AddChild(column, TRUE);

			OpTreeModel::ColumnData column_data(i);

			tree_model->GetColumnData(&column_data);

			column->SetColumnMatchability(column_data.matchable);
			column->SetText(column_data.text.CStr());

			if(i == m_sort_by_column)
			{
				column->GetBorderSkin()->SetImage(m_sort_ascending ? "Header Button Sort Ascending Skin" : "Header Button Sort Descending Skin", "Header Button Skin");
			}

			if (column_data.right_aligned)
				column->SetJustify(GetRTL() ? JUSTIFY_LEFT : JUSTIFY_RIGHT, FALSE);

			column->GetForegroundSkin()->SetImage(column_data.image);
		}
		ReadColumnSettings();
		ReadSavedMatches();
	}

	Relayout();
	UpdateSortParameters(tree_model, sort_by_column, sort_ascending);
	m_view_model.SetModel(tree_model, grouping);

	for (OpListenersIterator iterator(m_treeview_listeners); m_treeview_listeners.HasNext(iterator);)
	{
		m_treeview_listeners.GetNext(iterator)->OnTreeViewModelChanged(this);
	}
}

/***********************************************************************************
**
**	SetFilter
**
***********************************************************************************/
void OpTreeView::SetMatch(const uni_char* match_text,
						  INT32 match_column,
						  MatchType match_type,
						  INT32 match_parent_id,
						  BOOL select,
						  BOOL force)
{
	if (!force
		&& m_match_text.Compare(match_text) == 0
		&& (match_column == m_match_column || m_match_text.IsEmpty())
		&& match_type == m_match_type
		&& match_parent_id == m_match_parent_id)
	{
		return;
	}

	if (m_match_text.CStr() != match_text)
	{
		m_match_text.Set(match_text);
	}

	m_match_column = match_column;
	m_match_type = match_type;
	m_match_parent_id = match_parent_id;

	for (OpListenersIterator iterator(m_treeview_listeners); m_treeview_listeners.HasNext(iterator);)
	{
		m_treeview_listeners.GetNext(iterator)->OnTreeViewMatchChanged(this);
	}

	if (!m_async_match)
		UpdateAfterMatch(select);
}

/***********************************************************************************
**
**	InitItem
**
***********************************************************************************/
void OpTreeView::InitItem(INT32 pos)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	OpTreeModelItem::ItemData item_data(INIT_QUERY, this);

	item->GetItemData(&item_data);
	item->m_item_type = item_data.item_type;

	if (item_data.flags & FLAG_CUSTOM_WIDGET)
	{
		item->m_flags |= FLAG_CUSTOM_WIDGET;

		// this is a custom widget and it will handle its own painting
		OpTreeModelItem::ItemData item_data(WIDGET_QUERY);

		item->GetItemData(&item_data);
		if(item_data.widget_query_data.widget)
		{
			// Add the widget, so that it has a parent and is scheduled for deletion.
			AddChild(item_data.widget_query_data.widget);

			// Add to the list of custom widgets.
			OP_ASSERT(m_custom_widgets.Find(item_data.widget_query_data.widget) == -1);
			m_custom_widgets.Add(item_data.widget_query_data.widget);
		}
	}
	if (item_data.flags & FLAG_INITIALLY_CHECKED)
	{
		item->m_flags |= FLAG_CHECKED | FLAG_INITIALLY_CHECKED;
	}

	if (item_data.flags & FLAG_INITIALLY_DISABLED)
	{
		item->m_flags |= FLAG_DISABLED | FLAG_INITIALLY_DISABLED;
	}

	if (IsItemOpen(m_view_model.GetParentIndex(pos)) && (item_data.flags & FLAG_INITIALLY_OPEN))
	{
		item->m_flags |= FLAG_OPEN | FLAG_INITIALLY_OPEN;
	}

	// If root opens all, this item should be open if its root is opened
	if (!(item->m_flags & FLAG_OPEN) && m_root_opens_all && item->GetParentItem())
	{
		TreeViewModelItem* root_item = item->GetParentItem();
		while (root_item->GetParentItem() && !root_item->GetParentItem()->IsHeader())
			root_item = root_item->GetParentItem();

		if (root_item->IsOpen() && !root_item->IsHeader())
		{
			item->m_flags |= FLAG_OPEN | FLAG_INITIALLY_OPEN;
		}
	}

	// Copy other flags
	item->m_flags |= item_data.flags & (FLAG_SEPARATOR | FLAG_TEXT_SEPARATOR | FLAG_FORMATTED_TEXT | FLAG_NO_SELECT | FLAG_ASSOCIATED_TEXT_ON_TOP);
}

/***********************************************************************************
**
**	MatchItem
**
***********************************************************************************/
BOOL OpTreeView::MatchItem(INT32 pos, BOOL check, BOOL update_line)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	// make sure this one is cleared

	if (!check)
	{
		item->m_flags &= ~(FLAG_MATCH_DISABLED|FLAG_MATCHED_CHILD);
	}

	BOOL matched = TRUE;

	// match against text and/or type

	if (m_auto_match && HasMatchText())
	{
		matched = FALSE;

		INT32 count = GetTreeModel()->GetColumnCount();

		for (INT32 column = 0; column < count; column++)
		{
			ItemDrawArea* cache = item->GetItemDrawArea(this, column, FALSE);

			if (cache && uni_stristr(cache->m_strings.m_string.Get(), m_match_text.CStr()))
			{
				matched = TRUE;
				break;
			}
		}
	}
	else if (HasMatchText() || HasMatchType())
	{
		OpTreeModelItem::ItemData item_data(MATCH_QUERY);

		item_data.match_query_data.match_text = &m_match_text;
		item_data.match_query_data.match_column = m_match_column;
		item_data.match_query_data.match_type = m_match_type;

		item->GetItemData(&item_data);

		matched = (item_data.flags & FLAG_MATCHED) != 0;
	}
	else if (!HasMatchText() && m_force_empty_match_query)
	{
		OpTreeModelItem::ItemData item_data(MATCH_QUERY);
		item_data.match_query_data.match_text = &m_match_text;
		item->GetItemData(&item_data);
	}

	// if still matching, match against parent id

	if (matched && HasMatchParentID())
	{
		OpTreeModelItem* model_item = GetItemByPosition(pos);
		OpTreeModelItem* parent_item = GetParentByPosition(pos);

		INT32 parent_id = parent_item ? parent_item->GetID() : -1;

		matched = model_item->GetID() == m_match_parent_id  || m_match_parent_id == parent_id;
	}

	if (check)
	{
		return matched != item->IsMatched();
	}

	if (matched)
	{
		item->m_flags |= FLAG_MATCHED;

		TreeViewModelItem* parent;

		for (INT32 parent_id = m_view_model.GetParentIndex(pos); parent_id >= 0 && ((parent = m_view_model.GetItemByIndex(parent_id)) != NULL);
				   parent_id = m_view_model.GetParentIndex(parent_id))
		{
			parent->m_flags |= FLAG_MATCHED_CHILD;

			if (parent->IsMatched())
				break;

			if (!HasMatchParentID())
			{
				// if parent didn't match earlier, we must set it to FLAG_MATCH_DISABLED, so that
				// it becomes visible
				parent->m_flags |= FLAG_MATCH_DISABLED;

				// Make parent visible - insert into m_lines
				InsertLines(parent_id);
			}
		}
	}
	else
	{
		if (pos == m_selected_item)
		{
			m_selected_item = -1;
		}

		item->m_flags &= ~FLAG_MATCHED;
	}

	if (update_line)
		UpdateLines(pos);

	return FALSE;
}

/***********************************************************************************
**
**	Rematch
**
***********************************************************************************/
void OpTreeView::Rematch(INT32 pos)
{
	if (pos == -1)
		return;

	while (m_view_model.GetParentIndex(pos) != -1)
	{
		pos = m_view_model.GetParentIndex(pos);
	}

	INT32 count = m_view_model.GetSubtreeSize(pos) + 1;

	for (INT32 i = 0; i < count; i++)
	{
		MatchItem(pos + i, FALSE, FALSE); // Don't update lines here, we'll do it later
	}

	UpdateLines(pos);
}

/***********************************************************************************
**
**	ToggleItem
**
***********************************************************************************/
void OpTreeView::ToggleItem(INT32 pos)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	BOOL is_checked = item->m_flags & OpTreeModelItem::FLAG_CHECKED;

	SetCheckboxValue(pos, !is_checked);
}


/***********************************************************************************
**
**	SetCheckboxValue
**
***********************************************************************************/
void OpTreeView::SetCheckboxValue(INT32 pos, BOOL checked)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	if (checked)
		item->m_flags |= OpTreeModelItem::FLAG_CHECKED;
	else
		item->m_flags &= ~OpTreeModelItem::FLAG_CHECKED;

	OpRect rect;
	if (GetItemRect(pos, rect))
	{
		Invalidate(rect);
	}

	if (listener)
	{
		listener->OnItemChanged(this, pos);
	}
}

/***********************************************************************************
**
**	UpdateSortParameters
**
***********************************************************************************/
void OpTreeView::UpdateSortParameters(OpTreeModel* model,
									  INT32 sort_by_column,
									  BOOL sort_ascending)
{
	if (!model)
		return;

	if (sort_by_column < -1 || sort_by_column >= model->GetColumnCount())
	{
		return;
	}

	Column* column = GetColumnByIndex(m_sort_by_column);

	if (column)
	{
		OpTreeModel::ColumnData column_data(m_sort_by_column);

		model->GetColumnData(&column_data);

		column->GetBorderSkin()->SetImage("Header Button Skin");
		column->GetForegroundSkin()->SetImage(column_data.image);
	}

	m_sort_by_column = sort_by_column;
	m_sort_ascending = sort_ascending;
	m_sort_by_string_first = FALSE;
	m_custom_sort = FALSE;

	column = GetColumnByIndex(m_sort_by_column);

	if (column)
	{
		column->GetBorderSkin()->SetImage(sort_ascending ? "Header Button Sort Ascending Skin" : "Header Button Sort Descending Skin", "Header Button Skin");
		const char* image = column->GetBorderSkin()->GetImage();
		if(image && op_strcmp(image, "Header Button Skin") == 0)
		{
			column->GetForegroundSkin()->SetImage(sort_ascending ? "Sort Ascending" : "Sort Descending");
		}
	}

	if (m_sort_by_column == -1)
	{
		if (m_always_use_sortlistener)
		{
			m_sort_by_string_first = FALSE;
			m_custom_sort = TRUE;
			m_view_model.SetSortListener(this);
		}
		else
		{
			m_view_model.SetSortListener(NULL);
		}
	}
	else
	{
		OpTreeModel::ColumnData column_data(m_sort_by_column);

		model->GetColumnData(&column_data);

		m_sort_by_string_first = column_data.sort_by_string_first;
		m_custom_sort = column_data.custom_sort;
		m_view_model.SetSortListener(this);
	}
}

/***********************************************************************************
**
**	Sort
**
***********************************************************************************/
void OpTreeView::Sort(INT32 sort_by_column, BOOL sort_ascending)
{
	UpdateSortParameters(GetTreeModel(), sort_by_column, sort_ascending);
	m_view_model.Resort();
}

/***********************************************************************************
**
**	Regroup
**
***********************************************************************************/
void OpTreeView::Regroup()
{
	GenericTreeModel::ModelLock lock(&m_view_model);
	m_view_model.Regroup();
	m_view_model.Resort();
}


/***********************************************************************************
**
**	OnCompareItems
**
***********************************************************************************/
INT32 OpTreeView::OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1)
{
	int result = 0;

	if (m_sort_by_column == -1 && !m_always_use_sortlistener)
	{
		OP_ASSERT(1); // should not happen
		result = 0;
	}
	else if (m_custom_sort && !m_force_sort_by_string)
	{
		result = GetTreeModel()->CompareItems(m_sort_by_column, ((TreeViewModelItem*)item0)->GetModelItem(), ((TreeViewModelItem*)item1)->GetModelItem());
	}
	else
	{
		ItemDrawArea* cache0 = ((TreeViewModelItem*)item0)->GetItemDrawArea(this, m_sort_by_column, FALSE);
		if (!cache0)
			return 0;
		ItemDrawArea* cache1 = ((TreeViewModelItem*)item1)->GetItemDrawArea(this, m_sort_by_column, FALSE);
		if (!cache1)
			return 0;

		const uni_char* string0 = cache0->m_strings.m_string.Get();
		const uni_char* string1 = cache1->m_strings.m_string.Get();

		string0 += cache0->m_string_prefix_length;
		string1 += cache1->m_string_prefix_length;

		INT32 sort_order0 = cache0->m_sort_order;
		INT32 sort_order1 = cache1->m_sort_order;

		if (!m_sort_by_string_first && !m_force_sort_by_string)
		{
			result = sort_order0 - sort_order1;
		}

		if (result == 0 && string0 && string1)
		{
			result = uni_stricmp(string0, string1);
		}

		if (m_sort_by_string_first && result == 0)
		{
			result = sort_order0 - sort_order1;
		}

		if (result == 0)
		{
			result = cache0->m_string_prefix_length - cache1->m_string_prefix_length;
		}
	}

	return m_sort_ascending ? result : -result;
}

/***********************************************************************************
**
**	GetParentByPosition
**
***********************************************************************************/
OpTreeModelItem* OpTreeView::GetParentByPosition(int pos)
{
	return m_view_model.GetParentItem(pos);
}

/***********************************************************************************
**
**	GetItemByPosition
**
***********************************************************************************/
OpTreeModelItem* OpTreeView::GetItemByPosition(int pos)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	return item ? item->GetModelItem() : NULL;
}

/***********************************************************************************
**
**	GetSelectedItems
**
***********************************************************************************/
INT32 OpTreeView::GetSelectedItems( OpINT32Vector& list, BOOL by_id)
{
	list.Clear();

	for ( INT32 i = 0; i < m_item_count; i++)
	{
		TreeViewModelItem* item = m_view_model.GetItemByIndex(i);

		if (item->IsSelected() && !item->IsHeader())
		{
			list.Add( by_id ? item->GetID() : i );
		}
	}

	return list.GetCount();
}

/***********************************************************************************
**
**	GetDragObject
**
***********************************************************************************/
DesktopDragObject* OpTreeView::GetDragObject(OpTypedObject::Type type, INT32 x, int32 y)
{
	OpINT32Vector items;

	OpDragObject* op_drag_object;
	if (OpStatus::IsError(OpDragObject::Create(op_drag_object, type)))
		return NULL;
	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);

	for (INT32 i = 0; i < m_item_count; i++)
	{
		TreeViewModelItem* item = m_view_model.GetItemByIndex(i);

		if (item->IsSelected() && !item->IsHeader())
		{
			drag_object->AddID(item->GetID());
			items.Add(i);
		}
	}

	if (drag_object->GetIDCount() == 0)
	{
		OP_DELETE(drag_object);
		return NULL;
	}

	INT32 item_count_to_paint = min(30, drag_object->GetIDCount());

	Column* column = GetColumnByIndex(m_thread_column);

	OpBitmap* bitmap = NULL;

	OpBitmap::Create(&bitmap, column->m_width, m_line_height * item_count_to_paint, FALSE, TRUE, 0, 0, TRUE);

	if (bitmap)
	{
		OpPainter* painter = bitmap->GetPainter();

		if (painter)
		{
			vis_dev->BeginPaint(painter, OpRect(0, 0, bitmap->Width(), bitmap->Height()), OpRect(0, 0, bitmap->Width(), bitmap->Height()));

			INT32 translation_x = -vis_dev->GetTranslationX();
			INT32 translation_y = -vis_dev->GetTranslationY();

			vis_dev->Translate(translation_x, translation_y);

			// paint items

			GetPainterManager()->InitPaint(vis_dev, this);

			if (font_info.font_info)
			{
				vis_dev->SetFont(font_info.font_info->GetFontNumber());
				vis_dev->SetFontSize(font_info.size);
				vis_dev->SetFontWeight(font_info.weight);
				vis_dev->SetFontStyle(font_info.italic ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL);
				vis_dev->GetFont();		// will call painter->SetFont if necessary
			}

			for (INT32 i = 0; i < item_count_to_paint; i++)
			{
				OpRect cell_rect(0, i * m_line_height, column->m_width, m_line_height);

				GetPainterManager()->DrawTreeViewRow(cell_rect, FALSE);
				PaintCell(GetPainterManager(), NULL, 0, items.Get(i), GetColumnByIndex(m_thread_column), cell_rect);
			}

			vis_dev->Translate(-translation_x, -translation_y);

			vis_dev->EndPaint();

			bitmap->ReleasePainter();

			drag_object->SetBitmap(bitmap);

			// find pos

			if (m_selected_item != -1)
			{
				OpRect cell_rect;

				if (GetCellRect(m_selected_item, m_thread_column, cell_rect))
				{
					OpRect tree_rect = GetRect(TRUE);
					OpPoint point(x - (tree_rect.x + cell_rect.x), y - (tree_rect.y + cell_rect.y));

					OpWidget* parent = OpWidget::GetParent();
					if (parent)
					{
						OpRect par_rect = parent->GetRect(TRUE);
						point.x += par_rect.x;
						point.y += par_rect.y;
					}

					drag_object->SetBitmapPoint(point);
				}
			}
		}
		else
		{
			OP_DELETE(bitmap);
		}
	}

	return drag_object;
}


/***********************************************************************************
**
**  HasChanged
**  - Returns TRUE if new items have been checked or unchecked since it was
**    populated. Otherwise return FALSE.
***********************************************************************************/
BOOL OpTreeView::HasChanged()
{
	INT32 i;
	for (i = 0; i < m_item_count; i++)
		if (m_view_model.GetItemByIndex(i)->IsChanged())
			return TRUE;

	return FALSE;
}
 
/***********************************************************************************
**
**  HasItemChanged
**  - Returns TRUE if new items have been checked or unchecked since it was
**    populated. Otherwise return FALSE.
***********************************************************************************/
 
BOOL OpTreeView::HasItemChanged(INT32 pos)
{
	if (pos == -1)
	{
		return FALSE;
	}

	return m_view_model.GetItemByIndex(pos)->IsChanged();
}
 
/***********************************************************************************
**
**	GetChangedItems
**
***********************************************************************************/
INT32 OpTreeView::GetChangedItems(INT32*& items)
{
	items = NULL;

	INT32 changed_count = 0;
	INT32 i;

	for (i = 0; i < m_item_count; i++)
	{
		if (m_view_model.GetItemByIndex(i)->IsChanged())
		{
			changed_count++;
		}
	}

	if (changed_count > 0)
	{
		items = OP_NEWA(INT32, changed_count);
		if (!items) return 0;
		changed_count = 0;

		for (i = 0; i < m_item_count; i++)
		{
			if (m_view_model.GetItemByIndex(i)->IsChanged())
			{
				items[changed_count] = i;
				changed_count++;
			}
		}
	}

	return changed_count;
}

/***********************************************************************************
**
**	TreeChanged
**
***********************************************************************************/
void OpTreeView::TreeChanged()
{
	InvalidateAll();
	if (UpdateScrollbars())
	{
		Relayout();
	}
}

/***********************************************************************************
**
**	UpdateLines
**
***********************************************************************************/
void OpTreeView::UpdateLines(INT32 pos)
{
	// Update m_lines

	if(pos < 0)
	{
		// Update the entire tree :

		m_line_map.Clear();

		for (INT32 i = 0; i < m_item_count; i++)
		{
			if (IsItemVisible(i))
			{
				InsertLines(i);
			}
		}
	}
	else
	{
		// Update a subtree :

		INT32 subtree_size = m_view_model.GetSubtreeSize(pos);
		for (INT32 i = pos; i <= pos + subtree_size; i++)
		{
			if (IsItemVisible(i))
				InsertLines(i);
			else
				RemoveLines(i);
		}
	}
}

/***********************************************************************************
**
**	SetShowColumnHeaders
**
***********************************************************************************/
void OpTreeView::SetShowColumnHeaders(BOOL show_column_headers)
{
	if (m_show_column_headers != show_column_headers)
	{
		m_show_column_headers = show_column_headers;
		Relayout();
	}
}

/***********************************************************************************
**
**	SetSelected
**
***********************************************************************************/
void OpTreeView::SetSelected(INT32 item_index, BOOL selected)
{
	if (!IsDead())
	{
		UINT32& flags = m_view_model.GetItemByIndex(item_index)->m_flags;
		if(selected)
		{
			if(!(flags & OpTreeModelItem::FLAG_SELECTED))
			{
				flags |= OpTreeModelItem::FLAG_SELECTED;
				m_selected_item_count++;
			}
		}
		else
		{
			if(flags & OpTreeModelItem::FLAG_SELECTED)
			{
				flags &= ~OpTreeModelItem::FLAG_SELECTED;
				OP_ASSERT(m_selected_item_count > 0);
				m_selected_item_count--;
			}
		}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilityManager::GetInstance()->SendEvent(m_view_model.GetItemByIndex(item_index), Accessibility::Event(Accessibility::kAccessibilityStateCheckedOrSelected));
		AccessibilityManager::GetInstance()->SendEvent(&m_view_model, Accessibility::Event(Accessibility::kAccessibilityEventSelectionChanged));
#endif
	}
}

/***********************************************************************************
**
**	GetColumnByIndex
**
***********************************************************************************/
OpTreeView::Column* OpTreeView::GetColumnByIndex(INT32 column_index)
{
	for (UINT32 i = 0; i < m_columns.GetCount(); i++)
	{
		if (m_columns.Get(i)->m_column_index == column_index)
		{
			return m_columns.Get(i);
		}
	}

	return NULL;
}

/***********************************************************************************
**
**	SetColumnWeight
**
***********************************************************************************/
void OpTreeView::SetColumnWeight(INT32 column_index, INT32 weight)
{
	if (m_column_settings_loaded)
		return;

	Column* column = GetColumnByIndex(column_index);

	if (column)
	{
		column->m_weight = weight << 8;
		Relayout();
	}
}

/***********************************************************************************
**
**	SetColumnFixedWidth
**
***********************************************************************************/
void OpTreeView::SetColumnFixedWidth(INT32 column_index, INT32 fixed_width)
{
	Column* column = GetColumnByIndex(column_index);

	if (column)
	{
		column->m_fixed_width = fixed_width;
		Relayout();
	}
}

/***********************************************************************************
**
**	SetColumnFixedCharacterWidth
**
***********************************************************************************/
void OpTreeView::SetColumnFixedCharacterWidth(INT32 column_index, INT32 character_count)
{
	Column* column = GetColumnByIndex(column_index);

	if (column)
	{
		column->m_fixed_width = WidgetUtils::GetFontWidth(font_info)*character_count;
		Relayout();
	}
}

/***********************************************************************************
**
**	SetColumnImageFixedDrawArea
**
**  @param column_index
**  @param fixed_width of icon - the icon will be centered within this area
**
***********************************************************************************/
void OpTreeView::SetColumnImageFixedDrawArea(INT32 column_index, INT32 fixed_width)
{
	Column* column = GetColumnByIndex(column_index);

	if (column && fixed_width > 0)
	{
		column->m_image_fixed_width     = fixed_width;
		column->m_fixed_image_drawarea  = TRUE;
		Relayout();
	}
}

/***********************************************************************************
**
**	SetColumnHasDropdown
**
***********************************************************************************/
void OpTreeView::SetColumnHasDropdown(INT32 column_index, BOOL has_dropdown)
{
	Column* column = GetColumnByIndex(column_index);

	if (column)
	{
		column->m_has_dropdown = has_dropdown;
	}
}

/***********************************************************************************
**
**	SetColumnVisibility
**
***********************************************************************************/
BOOL OpTreeView::SetColumnVisibility(INT32 column_index, BOOL is_visible)
{
	Column* column = GetColumnByIndex(column_index);

	if (!column || !column->SetColumnVisibility(is_visible))
		return FALSE;

	Relayout();
	InvalidateAll();

	return TRUE;
}

/***********************************************************************************
**
**	SetColumnUserVisibility
**
***********************************************************************************/
BOOL OpTreeView::SetColumnUserVisibility(INT32 column_index, BOOL is_visible)
{
	Column* column = GetColumnByIndex(column_index);

	if (!is_visible)
	{
		// sanity check: at least 1 must be visible

		INT32 visible_count = 0;

		for (UINT32 i = 0; i < m_columns.GetCount(); i++)
		{
			if (m_columns.Get(i)->IsColumnReallyVisible())
				visible_count++;
		}

		if (visible_count <= 1)
			return FALSE;
	}

	if (!column || !column->SetColumnUserVisibility(is_visible))
		return FALSE;

	Relayout();
	InvalidateAll();
	WriteColumnSettings();

	return TRUE;
}

/***********************************************************************************
**
**	GetColumnVisibility
**
***********************************************************************************/
BOOL OpTreeView::GetColumnVisibility(INT32 column_index)
{
	Column* column = GetColumnByIndex(column_index);

	if (!column)
		return FALSE;

	return column->IsColumnVisible();
}

/***********************************************************************************
**
**	GetColumnUserVisibility
**
***********************************************************************************/
BOOL OpTreeView::GetColumnUserVisibility(INT32 column_index)
{
	Column* column = GetColumnByIndex(column_index);

	if (!column)
		return FALSE;

	return column->IsColumnUserVisible();
}

/***********************************************************************************
**
**	GetColumnName
**
***********************************************************************************/
BOOL OpTreeView::GetColumnName(INT32 column_index, OpString& name)
{
	Column* column = GetColumnByIndex(column_index);

	if (!column)
		return FALSE;

	column->GetText(name);

	return TRUE;
}

/***********************************************************************************
**
**	GetColumnMatchability
**
***********************************************************************************/
BOOL OpTreeView::GetColumnMatchability(INT32 column_index)
{
	Column* column = GetColumnByIndex(column_index);

	if (!column)
		return FALSE;

	return column->IsColumnMatchable();
}

/***********************************************************************************
**
**	IsFlat
**
***********************************************************************************/
BOOL OpTreeView::IsFlat()
{
	return m_view_model.IsFlat();
}

/***********************************************************************************
**
**	GetSavedMatch
**
***********************************************************************************/
BOOL OpTreeView::GetSavedMatch(INT32 match_index, OpString& match_text)
{
	OpString* match = m_saved_matches.Get(match_index);

	if (!match)
		return FALSE;

	match_text.Set(match->CStr());
	return TRUE;
}

/***********************************************************************************
**
**	AddSavedMatch
**
***********************************************************************************/
void OpTreeView::AddSavedMatch(OpString& match_text)
{
	if (match_text.IsEmpty())
		return;

	INT32 count = m_saved_matches.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		OpString* match = m_saved_matches.Get(i);

		if (match->CompareI(match_text.CStr()) == 0)
			return;
	}

	OpString* match = OP_NEW(OpString, ());

	if (!match)
		return;

	match->Set(match_text.CStr());
	m_saved_matches.Insert(0, match);

	if (count == 10)
		m_saved_matches.Delete(10);

	WriteSavedMatches();
}

/***********************************************************************************
**
**	UpdateAfterMatch
**
***********************************************************************************/
void OpTreeView::UpdateAfterMatch(BOOL select)
{
	if (!GetTreeModel())
		return;

	m_hover_item = -1;

	for (INT32 i = 0; i < m_item_count; i++)
	{
		MatchItem(i, FALSE, FALSE);
	}

	UpdateLines();
	TreeChanged();

	if (select)
	{
		if (m_selected_item == -1)
		{
			SelectNext();
		}
		else if (!m_async_match)
		{
			ScrollToItem(m_selected_item);
		}
	}

	for (OpListenersIterator iterator(m_treeview_listeners); m_treeview_listeners.HasNext(iterator);)
		m_treeview_listeners.GetNext(iterator)->OnTreeViewMatchFinished(this);
}

/***********************************************************************************
**
**	SetSelectedItemByID
**
***********************************************************************************/
void OpTreeView::SetSelectedItemByID(INT32 id,
									 BOOL changed_by_mouse,
									 BOOL send_onchange,
									 BOOL allow_parent_fallback)
{
	INT32 pos = GetItemByID(id);

	if (pos != -1)
	{
		SetSelectedItem(pos, changed_by_mouse, send_onchange, allow_parent_fallback);
	}
}

/***********************************************************************************
**
**	SetSelectedItem
**
***********************************************************************************/
void OpTreeView::SetSelectedItem(INT32 pos,
								 BOOL changed_by_mouse,
								 BOOL send_onchange,
								 BOOL allow_parent_fallback)
{
	if (pos < m_item_count && pos >= -1)
	{
		if (pos != -1 && m_view_model.GetItemByIndex(pos)->IsHeader())
		{
			SelectItem(pos, TRUE, FALSE, FALSE, FALSE, FALSE);
			if (SelectNext())
				return;
			pos = -1;
		}
		if (pos != -1)
		{
			INT32 parent = m_view_model.GetParentIndex(pos);
			while (m_view_model.GetParentIndex(parent) != -1) parent = m_view_model.GetParentIndex(parent);
			TreeViewModelItem* parent_item = m_view_model.GetItemByIndex(parent);
			if (parent_item && !parent_item->IsHeader() && !parent_item->IsOpen())
				OpenItem(m_view_model.GetParentIndex(pos), TRUE);
		}

		if (allow_parent_fallback)
		{
			while (pos != -1 && !IsItemVisible(pos))
			{
				pos = m_view_model.GetParentIndex(pos);
			}
		}

		SelectItem(pos, FALSE, FALSE, FALSE, changed_by_mouse, send_onchange);
	}
}

/***********************************************************************************
**
**	SelectAllItems
**
***********************************************************************************/
void OpTreeView::SelectAllItems(BOOL update)
{
	if (m_is_multiselectable)
	{
		if (m_root_opens_all)
		{
			OpenAllItems(TRUE);
		}

		for (INT32 i = 0; i < m_item_count; i++)
		{
			if (IsItemVisible(i))
			{
				SetSelected(i, TRUE);
			}
		}

		if (update)
		{
			InvalidateAll();
		}

		if (listener)
		{
			listener->OnChange(this, FALSE);
		}
	}
}

/***********************************************************************************
**
**	SelectNext
**
***********************************************************************************/
BOOL OpTreeView::SelectNext(BOOL forward, BOOL unread_only, BOOL skip_group_headers)
{
	if (m_item_count == 0)
		return FALSE;

	INT32 step = forward ? 1 : -1;

	INT32 start = m_selected_item == -1 ? 0 : m_selected_item + step;

	if (start < 0)
	{
		start = m_item_count - 1;
	}
	else if (start >= m_item_count)
	{
		start = 0;
	}

	INT32 next = start;

	do
	{
		BOOL can_be_selected = m_view_model.GetItemByIndex(next)->IsSelectable() && !(skip_group_headers && m_view_model.GetItemByIndex(next)->IsHeader());

		if (can_be_selected && unread_only)
		{
			OpTreeModelItem::ItemData item_data(UNREAD_QUERY);

			GetItemByPosition(next)->GetItemData(&item_data);

			can_be_selected = (item_data.flags & FLAG_UNREAD) != 0;
		}

		if (can_be_selected)
		{
			OpenItem(m_view_model.GetParentIndex(next), TRUE);

			if (IsItemVisible(next))
			{
				SelectItem(next);
				return TRUE;
			}
		}

		next += step;

		if (next < 0)
		{
			next = m_item_count - 1;
		}
		else if (next >= m_item_count)
		{
			next = 0;
		}

	} while (next != start);

	return FALSE;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL OpTreeView::SelectByLetter( INT32 letter, BOOL forward )
{
	if (IS_OP_KEY(letter))
		return FALSE;

	if( GetLineCount() <= 0 )
	{
		return FALSE;
	}

	INT32 step  = forward ? 1 : -1;
	INT32 line = GetLineByItem(m_selected_item) + step;

	if (line < 0)
	{
		line = GetLineCount() - 1;
	}
	else if (line >= GetLineCount())
	{
		line = 0;
	}

	letter = uni_toupper(letter);

	INT32 start_line = line;

	do
	{
		INT32 pos = GetItemByLine(line);
		if( pos == -1 )
		{
			break;
		}

		if( m_view_model.GetItemByIndex(pos)->IsSelectable() )
		{
			TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);
			if( item )
			{
				OpString text;
				OpTreeModelItem::ItemData item_data(COLUMN_QUERY);
				item_data.column_query_data.column_text = &text;
				item->GetItemData(&item_data);

				if( text.CStr() && uni_toupper(text.CStr()[0]) == (uni_char)letter )
				{
					SelectItem(pos);
					return TRUE;
				}
			}
		}

		line ++;
		if( line >= GetLineCount())
		{
			line = 0;
		}
		else if (line < 0)
		{
			line = GetLineCount() - 1;
		}
	} while (line != start_line );


	return FALSE;
}


/***********************************************************************************
**
**	SelectItem
**
***********************************************************************************/
void OpTreeView::SelectItem(INT32 pos,
							BOOL force_change,
							BOOL control,
							BOOL shift,
							BOOL changed_by_mouse,
							BOOL send_onchange)
{
	if (!m_is_multiselectable)
	{
		BOOL is_deselectable = changed_by_mouse ? m_is_deselectable_by_mouse : m_is_deselectable_by_keyboard;

		if (pos == -1 ||
		    !is_deselectable ||
		    !m_view_model.GetItemByIndex(pos)->IsSelected()
		    )
			control = FALSE;

		shift = FALSE;

		if (!is_deselectable && pos == -1 && m_item_count > 0)
		{
			return;
		}
	}

	OP_ASSERT(pos < m_item_count);

	if (pos != -1 && (!m_view_model.GetItemByIndex(pos)->IsSelectable() || !IsItemVisible(pos)))
	{
		m_selected_item = -1;
		return;
	}

	if (!shift)
	{
		m_shift_item = -1;
	}
	else if (m_shift_item == -1)
	{
		m_shift_item = m_selected_item;
	}

	BOOL update = FALSE;

	// if not control pressed and shift wasn't held while missing something, clear all

	if (!control && (!shift || pos != -1 || m_shift_item != -1))
	{
		for (INT32 i = 0; i < m_item_count; i++)
		{
			if ((i != m_selected_item || shift) && m_view_model.GetItemByIndex(i)->IsSelected())
			{
				SetSelected(i, FALSE);
				update = TRUE;
			}
		}
	}

	// if shift is pressed and we hit something, mark range

	if (shift && pos != -1 && m_shift_item != -1)
	{
		INT32 from = pos < m_shift_item ? pos : m_shift_item;
		INT32 to = pos > m_shift_item ? pos : m_shift_item;

		while (from <= to)
		{
			if (IsItemVisible(from))
			{
				SetSelected(from, TRUE);
			}
			from++;
		}

		update = TRUE;
	}

	// if any control or shift operations caused damage, invalidate all

	if (update)
	{
		InvalidateAll();
	}

	// update the main selected item

	if (pos != m_selected_item || force_change || control || shift)
	{
		if (m_edit)
		{
			CancelEdit();
		}

		OpRect rect;

		if (m_selected_item != -1 && m_selected_item < m_item_count)
		{
			UpdateItem(m_selected_item, TRUE);

			TreeViewModelItem* item = m_view_model.GetItemByIndex(m_selected_item);

			if (item)
			{
				item->m_flags &= ~OpTreeModelItem::FLAG_FOCUSED;

				if (!shift && !control)
				{
					SetSelected(m_selected_item, FALSE);
				}
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				else
				{
					AccessibilityManager::GetInstance()->SendEvent(item, Accessibility::Event(Accessibility::kAccessibilityStateCheckedOrSelected));
				}
#endif
			}
		}

		if (pos != -1)
		{
			UpdateItem(pos, TRUE);

			TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

			if (item)
			{
				if (control && !shift && item->IsSelected())
				{
					item->m_flags &= ~OpTreeModelItem::FLAG_FOCUSED;
					SetSelected(pos, FALSE);
					pos = -1;
				}
				else
				{
					item->m_flags |= OpTreeModelItem::FLAG_FOCUSED;
					SetSelected(pos, TRUE);
					ScrollToItem(pos, m_selected_item == -1 && !changed_by_mouse);
				}
			}
		}
	}

	if (pos != m_selected_item || force_change || update)
	{
		m_selected_item = pos;

		if (send_onchange && listener)
		{
			if (force_change)
			{
				m_send_onchange_later = TRUE;

				if (!IsTimerRunning())
				{
					StartTimer(1);
				}
			}
			else
			{
				listener->OnChange(this, changed_by_mouse);
				g_input_manager->UpdateAllInputStates();
			}
		}
	}
}

/***********************************************************************************
**
**	ScrollToLine
**
***********************************************************************************/
void OpTreeView::ScrollToLine(INT32 line, BOOL force_to_center)
{
	const OpRect rect = GetVisibleRectLTR();
	if (rect.height <= 0)
	{
		OnLayout();
	}
	if (line == -1)
	{
		line = GetLineCount() - 1;
	}

	if (force_to_center)
	{
		line -= (rect.height / m_line_height) / 2;

		if (line < 0)
		{
			line = 0;
		}

		m_vertical_scrollbar->SetValue(line * m_line_height);
		return;
	}

	int pos_of_line = line * m_line_height;

	int pos = m_vertical_scrollbar->GetValue();

	if (pos == pos_of_line)
	{
		return;
	}

	if (pos > pos_of_line)
	{
		m_vertical_scrollbar->SetValue(pos_of_line);
	}
	else if ((pos + rect.height - m_line_height) < pos_of_line)
	{
		m_vertical_scrollbar->SetValue(pos_of_line - rect.height + m_line_height);
	}
}

/***********************************************************************************
**
**	ScrollToItem
**
***********************************************************************************/
void OpTreeView::ScrollToItem(INT32 item, BOOL force_to_center)
{
	if (!IsItemVisible(item))
	{
		OpenItem(m_view_model.GetParentIndex(item), TRUE);
	}
	INT32 line = GetLineByItem(item);
	if ((m_vertical_scrollbar->GetValue() / m_line_height) < line && GetItemByLine(line+1) == item)
		line++;
	ScrollToLine(line, force_to_center);
}

/***********************************************************************************
**
**	Redraw
**
***********************************************************************************/
void OpTreeView::Redraw()
{
	for (INT32 i = 0; i < m_item_count; i++)
	{
		m_view_model.GetItemByIndex(i)->Clear();
	}

	InvalidateAll();
}

/***********************************************************************************
**
**	GetItemByID
**
***********************************************************************************/
INT32 OpTreeView::GetItemByID(INT32 id)
{
	TreeViewModelItem* item = m_view_model.GetItemByID(id);
	return item ? item->GetIndex() : -1;
}

/***********************************************************************************
**
**	GetItemByModelItem
**
***********************************************************************************/
INT32 OpTreeView::GetItemByModelItem(OpTreeModelItem* model_item)
{
	TreeViewModelItem* item = m_view_model.GetItemByModelItem(model_item);
	return item ? item->GetIndex() : -1;
}


/***********************************************************************************
**
**  GetViewModelIndex
**
***********************************************************************************/
INT32 OpTreeView::GetModelIndexByIndex(INT32 index)
{
	return m_view_model.GetModelIndexByIndex(index);
}

/***********************************************************************************
**
**	IsItemVisible
**
***********************************************************************************/
BOOL OpTreeView::IsItemVisible(INT32 pos)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	if (m_view_model.GetTreeModelGrouping() && item->IsHeader() && m_view_model.IsEmptyGroup(item))
	{
		return FALSE;
	}

	if (item->m_flags & FLAG_MATCH_DISABLED)
		return TRUE;

	if (!item->IsMatched())
		return FALSE;

	if (HasMatchText() || HasMatchParentID())
		return TRUE;

	INT32 parent = m_view_model.GetParentIndex(pos);

	if (parent != -1 && m_view_model.GetItemByIndex(parent)->m_flags & FLAG_MATCH_DISABLED)
		return TRUE;

	return IsItemOpen(parent);
}

/***********************************************************************************
**
**	IsItemOpen
**
***********************************************************************************/
BOOL OpTreeView::IsItemOpen(INT32 pos)
{
	if (pos == -1)
	{
		return TRUE;
	}

	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	if(item)
		return item->IsOpen();

	return FALSE;
}


/***********************************************************************************
 **
 ** IsLastLineSelected
 **
 ** Returns TRUE if the selected item is the bottom-most enabled item.
 **
 ***********************************************************************************/
BOOL OpTreeView::IsLastLineSelected()
{
	int position  = GetSelectedItemPos();
	int line      = GetLineByItem(position);
	int last_line = GetLineCount() - 1;

	int i = 0;

	for( i = last_line; i > line && IsItemDisabled(GetItemByLine(i)); i--)
	{
		// Do nothing
	}

	return position == GetItemByLine(i);
}


/***********************************************************************************
 **
 ** IsFirstLineSelected
 **
 ** Returns TRUE if the selected item is the topmost enabled item.
 **
 ***********************************************************************************/
BOOL OpTreeView::IsFirstLineSelected()
{
	int position = GetSelectedItemPos();
	int line     = GetLineByItem(position);

	int i = 0;

	for( i = 0; i < line && IsItemDisabled(GetItemByLine(i)); i++)
	{
		// Do nothing
	}

	return position == GetItemByLine(i);
}


/***********************************************************************************
**
**	CanItemOpen
**
***********************************************************************************/
BOOL OpTreeView::CanItemOpen(INT32 pos)
{
	if (IsItemOpen(pos))
	{
		return FALSE;
	}

	if( m_allow_open_empty_folder )
	{
		return TRUE;
	}
	else
	{
		return m_view_model.GetItemByIndex(pos)->CanOpen();
	}
}

/***********************************************************************************
**
**	IsItemChecked
**
***********************************************************************************/
BOOL OpTreeView::IsItemChecked(INT32 pos)
{
	if (pos == -1)
	{
		return FALSE;
	}

	return m_view_model.GetItemByIndex(pos)->IsChecked();
}

/***********************************************************************************
**
**	IsItemDisabled
**
***********************************************************************************/
BOOL OpTreeView::IsItemDisabled(INT32 pos)
{
	if (pos == -1)
	{
		return FALSE;
	}

	return m_view_model.GetItemByIndex(pos)->IsDisabled();
}

/***********************************************************************************
**
**	GetItemByLine
**
***********************************************************************************/
int OpTreeView::GetItemByLine(int line) const
{
	if(line >= int(m_line_map.GetCount()) || line < 0)
		return -1;

	return m_line_map.GetByIndex(line);
}

/***********************************************************************************
**
**	GetItemByPoint
**
***********************************************************************************/
INT32 OpTreeView::GetItemByPoint(OpPoint point, BOOL truncate_to_visible_items)
{
	return GetItemByLine(GetLineByPoint(point, truncate_to_visible_items));
}

/***********************************************************************************
**
**	GetLineByItem
**
***********************************************************************************/
int OpTreeView::GetLineByItem(int pos)
{
	return m_line_map.Find(pos);
}

/***********************************************************************************
**
**	GetLineByPoint
**
***********************************************************************************/
INT32 OpTreeView::GetLineByPoint(OpPoint point, BOOL truncate_to_visible_lines)
{
	if (!GetTreeModel())
	{
		return -1;
	}

	OpRect rect = GetVisibleRect();

	if (!truncate_to_visible_lines && !rect.Contains(point))
	{
		return -1;
	}

	point.y -= rect.y;
	point.y += m_vertical_scrollbar->GetValue();

	INT32 line = point.y / m_line_height;

	if (truncate_to_visible_lines)
	{
		INT32 line_at_top = m_vertical_scrollbar->GetValue() / m_line_height;
		INT32 line_at_bottom = (m_vertical_scrollbar->GetValue() + rect.height - 1) / m_line_height;

		if (line < line_at_top)
		{
			line = line_at_top;
		}
		else if (line > line_at_bottom)
		{
			line = line_at_bottom;
		}

		INT32 line_count = GetLineCount();

		if (line >= line_count)
		{
			line = line_count - 1;
		}
	}

	return line;
}

/***********************************************************************************
**
**	IsIcon
**
***********************************************************************************/
BOOL OpTreeView::IsThreadImage(OpPoint point, INT32 pos)
{
	if (!GetTreeModel() || !m_show_thread_image)
		return FALSE;

	if (pos != -1 && m_view_model.GetItemByIndex(pos)->IsHeader())
		return TRUE;

	// If root opens all, children don't have thread images
	if (m_root_opens_all && m_view_model.GetParentIndex(pos) != -1 && !m_view_model.GetParentItem(pos)->IsHeader())
		return FALSE;

	OpRect rect = GetVisibleRectLTR();
	if (GetRTL())
		point.x = GetRect().width - point.x - 1;

	if (!rect.Contains(point))
		return FALSE;

	const char* image = "Thread";

	INT32 image_width;
	INT32 image_height;

	GetSkinManager()->GetSize(image, &image_width, &image_height);
	image_width += 2; // We use 2 pixels as padding for the thread image

	point.x -= rect.x;
	point.y -= rect.y - (HasGrouping() ? m_line_height : 0);

	// special handling of associated text on top, if it has a thread image it's always at the start
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);
	if (item && item->HasAssociatedTextOnTop())
	{
		OpRect item_rect;
		GetItemRectLTR(pos, item_rect, FALSE);
		INT32 padding_top = (GetLineHeight() - image_height) / 2;
		if (point.x < (image_width+2) && (item_rect.y + padding_top) < point.y && (item_rect.y + padding_top + image_height) > point.y)
			return TRUE;
		else
			return FALSE;
	}

	if (m_show_horizontal_scrollbar)
		point.x += GetRTL() ? -m_horizontal_scrollbar->GetValue() : m_horizontal_scrollbar->GetValue();
	point.y += m_vertical_scrollbar->GetValue();

	INT32 column_width = 0;
	Column* column = NULL;
	UINT32 column_index;

	// Check which column we're in; keep adding widths until we get to the point we're looking for
	for (column_index = 0; column_index < m_columns.GetCount() && column_width <= point.x; column_index++)
	{
		column = m_columns.Get(column_index);

		if (column->IsColumnReallyVisible())
			column_width += column->m_width;
	}

	// Check if it's in the column where the thread image is in
	if (!column || column->m_column_index != m_thread_column)
		return FALSE;

	unsigned depth = 0;
	while ((pos = m_view_model.GetParentIndex(pos)) != -1 && !m_view_model.GetItemByIndex(pos)->IsHeader())
		depth++;

	int thread_image_x = column_width - (column->m_width - (depth * m_thread_indentation));

	return thread_image_x <= point.x && point.x < thread_image_x + image_width;
}

/***********************************************************************************
**
**	GetItemRect
**
***********************************************************************************/
BOOL OpTreeView::GetItemRect(INT32 pos, OpRect& rect, BOOL only_visible)
{
	const BOOL found = GetItemRectLTR(pos, rect, only_visible);
	rect = AdjustForDirection(rect);
	return found;
}

BOOL OpTreeView::GetItemRectLTR(INT32 pos, OpRect& rect, BOOL only_visible)
{
	INT32 line = GetLineByItem(pos);

	if (line == -1)
	{
		return FALSE;
	}

	const OpRect visible_rect = GetVisibleRectLTR();

	rect = visible_rect;
	if (m_show_horizontal_scrollbar)
		rect.x -= GetRTL() ? -m_horizontal_scrollbar->GetValue() : m_horizontal_scrollbar->GetValue();
	rect.y -= m_vertical_scrollbar->GetValue();
	rect.y += line * m_line_height;
	rect.height = m_line_height * m_view_model.GetItemByIndex(pos)->GetNumLines(); // TODO cleanup

	if (rect.Intersecting(visible_rect))
	{
		if (only_visible)
		{
			rect.IntersectWith(visible_rect);
		}
		return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
**
**	GetCellRect
**
***********************************************************************************/
BOOL OpTreeView::GetCellRect(INT32 pos, INT32 column_index, OpRect& rect, BOOL text_rect)
{
	const BOOL found = GetCellRectLTR(pos, column_index, rect, text_rect);
	rect = AdjustForDirection(rect);
	return found;
}

BOOL OpTreeView::GetCellRectLTR(INT32 pos, INT32 column_index, OpRect& rect, BOOL text_rect)
{
	GetItemRectLTR(pos, rect, FALSE);

	for (UINT32 i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);

		if (column->IsColumnReallyVisible())
		{
			if (column->m_column_index == column_index)
			{
				rect.width = column->m_width;

				if (text_rect)
				{
					PaintCell(NULL, NULL, 0, pos, column, rect);
				}

				return TRUE;
			}

			rect.x += column->m_width;
		}
	}

	return FALSE;
}

/***********************************************************************************
**
**	GetVisibleRectLTR
**
***********************************************************************************/
OpRect OpTreeView::GetVisibleRectLTR(bool include_column_headers, bool include_scrollbars, bool include_group_header)
{
	OpRect rect = GetBounds();
	AddPadding(rect);

	if (!include_scrollbars)
	{
		if (m_vertical_scrollbar->IsVisible())
			rect.width -= GetInfo()->GetVerticalScrollbarWidth();

		if (m_horizontal_scrollbar->IsVisible())
			rect.height -= GetInfo()->GetHorizontalScrollbarHeight();
	}

	if (GetTreeModel() && m_show_column_headers && !include_column_headers)
	{
		rect.y += m_column_headers_height;
		rect.height -= m_column_headers_height;
	}

	if (!include_group_header && HasGrouping())
	{
		rect.y += m_line_height;
		rect.height -= m_line_height;
	}

	rect.width = MAX(rect.width, 0);
	rect.height = MAX(rect.height, 0);

	return rect;
}

/***********************************************************************************
**
**	UpdateScrollbars
**
***********************************************************************************/
bool OpTreeView::UpdateScrollbars()
{
	INT32 total_width, total_height, cols = 0, rows = 0;

	BOOL was_horizontal_visible = m_horizontal_scrollbar->IsVisible();
	BOOL was_vertical_visible   = m_vertical_scrollbar->IsVisible();

	GetPreferedSize(&total_width, &total_height, cols, rows);
	total_height -= 4; // extra padding added for correct scrollbars in OpAccordion. See GetPreferedSize()

	OpRect rect = GetVisibleRectLTR(false, true);

	m_horizontal_scrollbar->SetVisibility(total_width > rect.width);
	m_vertical_scrollbar->SetVisibility(total_height > rect.height);

	// a second pass will turn on other scrollbar if it's needed because first pass turned on one

	rect = GetVisibleRectLTR(false, true);

	m_horizontal_scrollbar->SetVisibility(total_width > rect.width);
	m_vertical_scrollbar->SetVisibility(total_height > rect.height);

	// now we have the resulting rect and can set correct values.

	rect = GetVisibleRectLTR();

	m_horizontal_scrollbar->SetSteps(m_line_height, rect.width);
	m_vertical_scrollbar->SetSteps(m_line_height, rect.height);

	if (GetRTL())
	{
		m_horizontal_scrollbar->SetLimit(rect.width - total_width, 0, rect.width);
	}
	else
	{
		m_horizontal_scrollbar->SetLimit(0, total_width - rect.width, rect.width);
	}
	m_vertical_scrollbar->SetLimit(HasGrouping() ? m_line_height : 0, total_height - rect.height, rect.height);

	return was_horizontal_visible != m_horizontal_scrollbar->IsVisible()
			|| was_vertical_visible != m_vertical_scrollbar->IsVisible();
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/
void OpTreeView::OnLayout()
{
	CalculateLineHeight();
	UpdateScrollbars();

	// * To layout scrollbars: get rect without headers, with scrollbars and with group header
	OpRect rect = GetVisibleRectLTR(false, true, true);

	int scrollbar_width  = GetInfo()->GetVerticalScrollbarWidth();
	int scrollbar_height = GetInfo()->GetHorizontalScrollbarHeight();

	// Vertical scrollbar
	OpRect scrollbar_rect(rect.Right() - scrollbar_width, rect.y, scrollbar_width, rect.height);
	if (m_horizontal_scrollbar->IsVisible())
		scrollbar_rect.height -= scrollbar_height;
	SetChildRect(m_vertical_scrollbar, scrollbar_rect);

	// Horizontal scrollbar
	scrollbar_rect = OpRect(rect.x, rect.Bottom() - scrollbar_height, rect.width, scrollbar_height);
	if (m_vertical_scrollbar->IsVisible())
		scrollbar_rect.width -= scrollbar_width;
	SetChildRect(m_horizontal_scrollbar, scrollbar_rect);

	// * To layout content: get rect with headers, without scrollbars and with group header
	rect = GetVisibleRectLTR(true, false, true);

	int total_column_weight = 0;
	int available_column_width = rect.width;

	// If showing the horizontal scrollbar we need to calculate the full width (without scrollbars)
	if (m_show_horizontal_scrollbar)
		available_column_width = max(rect.width, GetFixedWidth());

	// 1) Add up column count, weights (relative), and sizes (absolute)
	m_visible_column_count = 0;

	UINT32 i;

	for (i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);

		if (column->IsColumnReallyVisible())
		{
			m_visible_column_count++;

			if (column->m_fixed_width > 0)
			{
				available_column_width -= column->m_fixed_width;
			}
			else
			{
				total_column_weight += column->m_weight;
			}
		}
	}

	// 2) Set column widths
	OpRect column_rect = rect;
	column_rect.height = m_column_headers_height;

	int rest = available_column_width;
	int sizeable_count = 0;
	const int min_width = 4;

	for (i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);

		if (column->IsColumnReallyVisible())
		{
			if (column->m_fixed_width > 0)
			{
				column->m_width = column->m_fixed_width;
			}
			else if (available_column_width > 0)
			{
				sizeable_count++;
				column->m_width = total_column_weight ? available_column_width * column->m_weight / total_column_weight : min_width;
				if (column->m_width < min_width)
					column->m_width = min_width;
				rest -= column->m_width;
			}
			else
			{
				column->m_width = 0;
			}
		}
	}

	// 3) spread the remaining width amongst the colums that don't have fixed with
	if (sizeable_count > 0 && rest > 0)
	{
		int add_width = rest / sizeable_count;
		int resized = 0;
		rest = rest % sizeable_count; // the rest that cannot be spread evenly: add 1px of it to each column until it is used up

		for (i = 0; i < m_columns.GetCount() && resized < sizeable_count; i++)
		{
			Column* column = m_columns.Get(i);

			if (column->IsColumnReallyVisible() && column->m_fixed_width <= 0) // column is sizable
			{
				column->m_width += add_width;
				if (resized < rest)
					column->m_width++;

				resized++;
			}
		}
	}

	// 4) Set the column rects (update x and width) and visibility
	for (i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);

		if (column->IsColumnReallyVisible() && m_show_column_headers)
		{
			column_rect.width = column->m_width;
			SetChildRect(column, column_rect);
			column_rect.x += column_rect.width;
		}

		column->SetVisibility(column->IsColumnReallyVisible() && m_show_column_headers);
	}

	// 5) If needed, (un)set filler widget (i.e. the corner between the scrollbars).
	if (m_vertical_scrollbar->IsVisible() && m_show_column_headers)
	{
		if (!m_column_filler)
		{
			// Create dummy button
			RETURN_VOID_IF_ERROR(OpButton::Construct(&m_column_filler, OpButton::TYPE_HEADER));
			AddChild(m_column_filler, true);
			m_column_filler->SetDead(true);
		}

		// Set position
		OpRect filler_rect;
		filler_rect.height = m_column_headers_height;
		filler_rect.width  = scrollbar_width;
		filler_rect.x = rect.x + rect.width;
		filler_rect.y = rect.y;
		SetChildRect(m_column_filler, filler_rect);
	}
	else if (!m_vertical_scrollbar->IsVisible() && m_column_filler)
	{
		m_column_filler->Delete();
		m_column_filler = NULL;
	}

	m_group_headers_to_draw.DeleteAll();
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/
BOOL OpTreeView::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_GET_TYPED_OBJECT)
	{
		if (action->GetActionData() == WIDGET_TYPE_TREEVIEW)
		{
			action->SetActionObject(this);
			return TRUE;
		}
		return FALSE;
	}

	if (!GetTreeModel())
	{
		return FALSE;
	}

	const OpRect rect = GetVisibleRectLTR();

	int pos = m_vertical_scrollbar->GetValue();
	int lines_per_page = rect.height / m_line_height;
	int line_at_top = (pos + m_line_height - 1) / m_line_height;
	int line_at_bottom = ((pos + rect.height - m_line_height) / m_line_height);
	if (GetItemByLine(line_at_bottom-1) == GetItemByLine(line_at_bottom))
		line_at_bottom--;

	int current_selected_line = GetLineByItem(m_selected_item);
	int new_selected_line = current_selected_line;
	BOOL forward_scan = TRUE;
	BOOL allow_selecting_headers = TRUE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SORT_BY_COLUMN:
				{
					child_action->SetSelected(child_action->GetActionData() == m_sort_by_column);
					return TRUE;
				}

				case OpInputAction::ACTION_SORT_DIRECTION:
				{
					child_action->SetSelected(child_action->GetActionData() == m_sort_ascending);
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_COLUMN:
				case OpInputAction::ACTION_HIDE_COLUMN:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_COLUMN, GetColumnUserVisibility(child_action->GetActionData()));
					return TRUE;
				}
				case OpInputAction::ACTION_OPEN_ITEM:
				{
					child_action->SetEnabled(CanItemOpen(m_selected_item));
					return TRUE;
				}
			}

			return FALSE;
		}

		case OpInputAction::ACTION_SHOW_COLUMN:
		{
			return SetColumnUserVisibility(action->GetActionData(), TRUE);
		}

		case OpInputAction::ACTION_HIDE_COLUMN:
		{
			return SetColumnUserVisibility(action->GetActionData(), FALSE);
		}

		case OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED:
		{
			if (action->GetActionData() == OP_KEY_ENTER && GetAction())
			{
				return g_input_manager->InvokeAction(GetAction(), this);
			}
			else if( m_allow_select_by_letter && !g_op_system_info->GetShiftKeyState() )
			{
				return SelectByLetter( action->GetActionData(), TRUE );
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SORT_BY_COLUMN:
		{
			Sort(action->GetActionData(), m_sort_ascending);
			if (listener)
			{
				listener->OnSortingChanged(this);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SORT_DIRECTION:
		{
			Sort(m_sort_by_column, action->GetActionData());
			if (listener)
			{
				listener->OnSortingChanged(this);
			}
			return TRUE;
		}

#ifdef M2_SUPPORT
		case OpInputAction::ACTION_SELECT_NEXT_UNREAD:
		case OpInputAction::ACTION_SELECT_PREVIOUS_UNREAD:
		{
			if (SelectNext(action->GetAction() == OpInputAction::ACTION_SELECT_NEXT_UNREAD, TRUE))
			{
				new_selected_line = GetLineByItem(m_selected_item);
				pos = m_vertical_scrollbar->GetValue();
				line_at_top = (pos + m_line_height - 1) / m_line_height;
				line_at_bottom = ((pos + rect.height - m_line_height) / m_line_height);
				if (new_selected_line < line_at_top + m_keep_visible_on_scroll)
					ScrollToLine(MAX(new_selected_line - m_keep_visible_on_scroll, 0));
				if (new_selected_line > line_at_bottom - m_keep_visible_on_scroll)
					ScrollToLine(new_selected_line + m_keep_visible_on_scroll);
				return TRUE;
			}
			return FALSE;
		}
#endif // M2_SUPPORT
		case OpInputAction::ACTION_EDIT_ITEM:
		{	
			if (m_edit)
			{
				if( AutoCompletePopup::IsAutoCompletionVisible() )
				{
					// We have to close the popup window before the edit field can be removed
					// AutoCompletePopup::CloseAnyVisiblePopup() does not help [espen]
					return TRUE;
				}

				OpString text;

				m_edit->GetText(text);

				if (listener)
				{
					// Allow calling code to reject change before it gets added into the tree
					if (!listener->OnItemEditVerification(this, m_edit_pos, m_edit_column, text))
						return TRUE;
				}

				CancelEdit();

				if (listener)
				{
					listener->OnItemEdited(this, m_edit_pos, m_edit_column, text);
				}

				UpdateItem(m_edit_pos, TRUE);

				if (m_sort_by_column != -1 || m_always_use_sortlistener)
				{
					Sort(m_sort_by_column, m_sort_ascending);
				}
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SELECT_ALL:
		{
			if (m_is_multiselectable)
			{
				if (m_root_opens_all)
				{
					OpenAllItems(TRUE);
				}

				for (INT32 i = 0; i < m_item_count; i++)
				{
					if (IsItemVisible(i))
					{
						SetSelected(i, TRUE);
					}
				}

				InvalidateAll();
			}
			return TRUE;
		}

		case OpInputAction::ACTION_CHECK_ITEM:
		{
			if (m_selected_item != -1 && m_checkable_column != -1 && !IsItemChecked(m_selected_item))
			{
				ToggleItem(m_selected_item);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_UNCHECK_ITEM:
		{
			if (m_selected_item != -1 && m_checkable_column != -1 && IsItemChecked(m_selected_item))
			{
				ToggleItem(m_selected_item);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_PREVIOUS_LINE:
			allow_selecting_headers = FALSE;
			// fallthrough
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_RANGE_PREVIOUS_ITEM:

			// If there is no selected item then pick the last one
			// as the previous. (Part of fix for Bug 255338, psmaas)

			if(new_selected_line < 0)
				new_selected_line = GetLineCount() - 1;
			else
				new_selected_line--;

			forward_scan = FALSE;

			/*if (new_selected_line < line_at_top + m_keep_visible_on_scroll)
				ScrollToLine(MAX(new_selected_line - m_keep_visible_on_scroll, 0));*/
			break;

		case OpInputAction::ACTION_NEXT_LINE:
			allow_selecting_headers = FALSE;
			// fallthrough
		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_RANGE_NEXT_ITEM:
		{
			int num_lines = m_view_model.GetItemByIndex(m_selected_item) ? m_view_model.GetItemByIndex(m_selected_item)->GetNumLines() : 0;

			new_selected_line+= num_lines;

			if(new_selected_line < 0)
				new_selected_line = 0;

			if (new_selected_line > line_at_bottom - m_keep_visible_on_scroll)
				ScrollToLine(new_selected_line + m_keep_visible_on_scroll);

			if (action->GetActionData() && m_selected_item != -1)
			{
				OpenItem(m_selected_item,  TRUE);
			}

			break;
		}

		case OpInputAction::ACTION_GO_TO_START:
		case OpInputAction::ACTION_RANGE_GO_TO_START:
			new_selected_line = 0;
			allow_selecting_headers = FALSE;
			break;

		case OpInputAction::ACTION_GO_TO_END:
		case OpInputAction::ACTION_RANGE_GO_TO_END:
			new_selected_line = GetLineCount() - 1;
			forward_scan = FALSE;
			allow_selecting_headers = FALSE;
			break;

		case OpInputAction::ACTION_PAGE_UP:
		case OpInputAction::ACTION_RANGE_PAGE_UP:
		{
			if (action->GetActionData())
				return FALSE;

			if (new_selected_line == line_at_top)
			{
				new_selected_line -= lines_per_page;
			}
			else
			{
				new_selected_line = line_at_top;
			}
			forward_scan = FALSE;
			break;
		}

		case OpInputAction::ACTION_PAGE_DOWN:
		case OpInputAction::ACTION_RANGE_PAGE_DOWN:
		{
			if (action->GetActionData())
				return FALSE;

			if (new_selected_line == line_at_bottom)
			{
				new_selected_line += lines_per_page;
			}
			else
			{
				new_selected_line = line_at_bottom;
			}
			break;
		}

		case OpInputAction::ACTION_OPEN_ALL_ITEMS:
		{
			OpenAllItems(TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_CLOSE_ALL_ITEMS:
		{
			OpenAllItems(FALSE);
			return TRUE;
		}

		case OpInputAction::ACTION_OPEN_ITEM:
		{
			if (CanItemOpen(m_selected_item))
			{
				OpenItem(m_selected_item, TRUE);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_CLOSE_ITEM:
		{
			INT32 item_to_close = m_selected_item;

			if (item_to_close == -1)
				return FALSE;

			if (m_root_opens_all)
			{
				while (m_view_model.GetParentIndex(item_to_close) != -1 && !m_view_model.GetParentItem(item_to_close)->IsHeader())
				{
					item_to_close = m_view_model.GetParentIndex(item_to_close);
				}
			}
			else if (!IsItemOpen(item_to_close) && m_view_model.GetParentIndex(item_to_close) != -1)
			{
				item_to_close = m_view_model.GetParentIndex(item_to_close);
			}

			if (IsItemOpen(item_to_close))
			{
				OpenItem(item_to_close, FALSE);
				SelectItem(item_to_close);
				return TRUE;
			}
			return FALSE;
		}
		default:
			return FALSE;
	}

	if (!allow_selecting_headers)
	{
		if (m_view_model.GetTreeModelGrouping() && m_view_model.GetTreeModelGrouping()->HasGrouping())
			{
				TreeViewModelItem* item = m_view_model.GetItemByIndex(GetItemByLine(new_selected_line));
				if (item && item->IsHeader())
				{
					if (!item->IsOpen())
						OpenItem(item->GetIndex(),TRUE);
					switch (action->GetAction())
					{
					case OpInputAction::ACTION_PREVIOUS_LINE:
					{
						TreeViewModelItem* current_item =  m_view_model.GetItemByIndex(GetItemByLine(new_selected_line));
						while (current_item && current_item->IsHeader())
						{
							if (current_item->IsOpen())
								new_selected_line = (new_selected_line == 0) ? GetLineCount() -1 : new_selected_line - current_item->GetNumLines();
							else
							{
								int count = GetLineCount();
								OpenItem(current_item->GetIndex(),TRUE);
								new_selected_line += GetLineCount() - count - item->GetNumLines();
							}
							current_item =  m_view_model.GetItemByIndex(GetItemByLine(new_selected_line));
						}
					}
					break;
					case OpInputAction::ACTION_NEXT_LINE:
						new_selected_line += item->GetNumLines();
						break;
					case OpInputAction::ACTION_GO_TO_START:
					case OpInputAction::ACTION_RANGE_GO_TO_START:
						new_selected_line += item->GetNumLines();
						break;
					case OpInputAction::ACTION_GO_TO_END:
					case OpInputAction::ACTION_RANGE_GO_TO_END:
						new_selected_line =  GetLineCount() - item->GetNumLines();
						break;
					}
					
				}
			}
	}

	if (new_selected_line < 0)
	{
		if(m_allow_wrapping_on_scrolling)
		{
			new_selected_line = GetLineCount() - 1;
		}
		else
		{
			new_selected_line = 0;
			if (m_selected_item == new_selected_line)
				return FALSE;
		}
	}

	if (new_selected_line >= GetLineCount() && GetLineCount())
	{
		if(m_allow_wrapping_on_scrolling)
		{
			new_selected_line = 0;
		}
		else
		{
			new_selected_line = GetLineCount() - 1;
			if (m_selected_item == GetItemByLine(new_selected_line))
				return FALSE;
		}
	}

	INT32 new_selected_item = GetItemByLine(new_selected_line);

	if (new_selected_item != m_selected_item)
	{
		/* new selected item might be disabled. scan for other one to select */

		while (new_selected_item != -1 && 
			new_selected_item != m_selected_item && 
			(!m_view_model.GetItemByIndex(new_selected_item)->IsSelectable()))
		{
			if (forward_scan)
			{
				if (new_selected_line == GetLineCount() - 1)
				{
					// we're at the end, wrap back to the top and continue trying
					if(m_allow_wrapping_on_scrolling)
					{
						new_selected_line = 0;
					}
					else
					{
						// We arrive here if last item in list is not selectable. Restore
						// to current line to avoid an endless loop.
						new_selected_line = current_selected_line;
					}
				}
				else
				{
					new_selected_line++;
				}
			}
			else
			{
				if (new_selected_line == 0)
				{
					// we're at the top, wrap back to the bottom and continue trying
					if(m_allow_wrapping_on_scrolling)
					{
						new_selected_line = GetLineCount() - 1;
					}
					else
					{
						return TRUE;
					}
				}
				else
				{
					new_selected_line--;
				}
			}
			new_selected_item = GetItemByLine(new_selected_line);
		}

		m_anchor = GetMousePos();

		SelectItem(new_selected_item, FALSE, FALSE, action->IsRangeAction());
		Sync();
	}

	return TRUE;
}

/***********************************************************************************
**
** GetVisibleItemPos
**
** @param line - a line owned by the item you want to find the visible index of
**
** TODO - do not do this - think of a better way (this is a fix for bug 280353)
**
** This is probably very slow - which might be noticable especially on scroll.
** It counts the number of items up until this item, to get what the visible
** index of the item that owns this line.
**
***********************************************************************************/
int OpTreeView::GetVisibleItemPos(int line)
{
	if(line >= INT32(m_line_map.GetCount()) || line < 0)
		return -1;

	int visible_pos = 0;
	int last_item   = 0;

	for(int i = 0; i < line; i++)
	{
		int item = GetItemByLine(i);

		if(item != last_item)
		{
			visible_pos++;
			last_item = item;
		}
	}

	return visible_pos;
}

/***********************************************************************************
**
**	GetFixedWidth
**
**
***********************************************************************************/

int OpTreeView::GetFixedWidth()
{
	int total_width = 0;

	for (UINT32 i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);
		if (column->IsColumnReallyVisible())
			total_width += column->m_fixed_width;
	}

	return total_width;
}

/***********************************************************************************
**
**	OnPaint
**
**  @param widget_painter
**  @param paint_rect
**
***********************************************************************************/
void OpTreeView::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	PaintTree(widget_painter, paint_rect);
}
	
void OpTreeView::PaintTree(OpWidgetPainter* widget_painter, const OpRect &paint_rect, OpScopeDesktopWindowManager_SI::QuickWidgetInfoList *list, BOOL include_invisible)
{
	if (!GetTreeModel())
		return;
	PaintGroupHeader(widget_painter, paint_rect, FALSE);
	const OpRect visible_rect = GetVisibleRect(); // without headers and scrollbars

	if (!paint_rect.Intersecting(visible_rect))
	{
		return;
	}

	int top_line = m_vertical_scrollbar->GetValue() / m_line_height;

	// Get the real first line of the top item:
	int line = GetLineByItem(GetItemByLine(top_line));

	int visible_pos = line;

	if(m_allow_multi_line_items)
	{
		// Get the visible index of the item owning this line:
		visible_pos = GetVisibleItemPos(line);
	}

	// Get the number of lines to paint
	// + 2 to cover partially scrolled lines
	// + (top_line - line) to cover the entire first item
	int lines_to_paint = visible_rect.height / m_line_height + 2 + (top_line - line);

	// Add non-painted widgets to the UI automation list
	if (!widget_painter && list && include_invisible)
		OpStatus::Ignore(AddQuickWidgetInfoInvisibleItems(*list, 0, visible_pos - 1));
	
	OpVector<OpWidget> painted_custom_widgets;

	// install clipping
	if (widget_painter)
		SetClipRect(visible_rect);

	while (lines_to_paint > 0 && line < GetLineCount())
	{
		int pos = GetItemByLine(line);

		if (pos == -1)
			break;

		// first do a prepaint query. this is a dummy query to let model change itself before
		// painting occurs. If something happens to model while in this query, m_changed will be
		// set and we will detect this when call returns and bail out and wait for a whole new
		// redraw. (this is really panic functionality.. so that model can wait until
		// the very latest before detecting that something it thought existed doesn't exist.
		// The model is NOT allowed to change itself inside any other GetItemData call

		m_changed = false;

		OpTreeModelItem::ItemData item_data(PREPAINT_QUERY);
		GetItemByPosition(pos)->GetItemData(&item_data);

		if (m_changed)
		{
			TreeChanged();
			break;
		}

		//  ----------------------------------------------------------------------
		//  Get the rectangle to draw the item in :
		//  ----------------------------------------------------------------------
		OpRect rect;
		if (!GetItemRectLTR(pos, rect, FALSE))
			break;

		int lines_painted = PaintItem(widget_painter, pos, paint_rect, rect, visible_pos & 1, list);

		OP_ASSERT(lines_painted >= 1);

		// Add any hidden items to the list
		if (!widget_painter && list && include_invisible)
		{
			INT32 next_pos = GetItemByLine(line + 1);

			OpStatus::Ignore(AddQuickWidgetInfoInvisibleItems(*list, pos + 1, next_pos));
		}

		TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

		if (item->IsCustomWidget())
		{
			// If a custom widget item is painted we will add it to the list
			// of painted custom widgets so we can detect custom widgets that
			// should no be visible anymore.

			OpTreeModelItem::ItemData item_data(WIDGET_QUERY);
			item->GetItemData(&item_data);

			if (item_data.widget_query_data.widget)
				m_custom_painted_widgets.Add(item_data.widget_query_data.widget);
		}

		line += lines_painted;
		lines_to_paint -= lines_painted;
		visible_pos++;
	}

	if (widget_painter)
		RemoveClipRect();

	// Add non-painted widgets to the UI automation list
	if (!widget_painter && list && include_invisible)
		OpStatus::Ignore(AddQuickWidgetInfoInvisibleItems(*list, GetItemByLine(visible_pos), INT32_MAX));
	
	// Hide custom widget that shouldn't be visible.

	for (unsigned i = 0; i < m_custom_widgets.GetCount(); i++)
	{
		OpWidget *child = m_custom_widgets.Get(i);
		if (m_custom_painted_widgets.Find(child) == -1)
		{
			// This is a custom widget that wasn't painted. If the paint_rect
			// intersects with where this widget is currently positioned,
			// we know that the widget should be hidden. Happens f.ex when adding
			// more items at the beginning so a custom widget is pushed out of view.

			OpRect child_rect = child->GetRect();
			if (child_rect.Intersecting(paint_rect))
				child->SetRect(OpRect(-10000, child_rect.y, child_rect.width, child_rect.height));
		}
	}
	m_custom_painted_widgets.Clear();
}

void OpTreeView::PaintGroupHeader(OpWidgetPainter* widget_painter,
		             const OpRect &paint_rect,
		             BOOL background_line)
{
	if (!HasGrouping())
		return;

	const OpRect visible_rect = GetVisibleRect(false, false, true);
	if (!paint_rect.Intersecting(visible_rect))
		return;

	if (!m_group_headers_to_draw.GetCount())
	{
		SetTopGroupHeader();
	}
	if (widget_painter)
		SetClipRect(visible_rect);
	for (UINT32 i = 0; i < m_group_headers_to_draw.GetCount(); i++)
	{
		GroupHeaderInfo* header = m_group_headers_to_draw.Get(i);
		header->rect.width = visible_rect.width;
		PaintItem(widget_painter, header->pos, paint_rect, header->rect, background_line);
	}
	if (widget_painter)
		RemoveClipRect();

	return;
}


void OpTreeView::CalculateAssociatedImageSize(ItemDrawArea* associated_image, const OpRect& item_rect, OpRect& associated_image_rect)
{
	if(associated_image && associated_image->m_image != NULL)
	{
		OpWidgetImage associated_img;
		associated_img.SetImage(associated_image->m_image);

		associated_image_rect = associated_img.CalculateScaledRect(item_rect, FALSE, TRUE);
	}
}
/***********************************************************************************
**
**	PaintItem
**
**  @param widget_painter  - to be used when painting
**  @param pos             - of the item to be painted
**  @param paint_rect      - the rectangle to be painted in
**  @param rect			   - the rectangle of item to be painted
**  @param background_line - whether the item should have a background color
**  @param list			   - list to add QuickWidgetInfo items too for UI automation
**
**  @return the number of lines painted
***********************************************************************************/
int OpTreeView::PaintItem(OpWidgetPainter* widget_painter,
						  INT32 pos,
						  OpRect paint_rect,
						  OpRect rect,
						  BOOL background_line,
						  OpScopeDesktopWindowManager_SI::QuickWidgetInfoList *list)
{
#ifdef WIDGET_RUNTIME_SUPPORT
	if (!m_paint_background_line)
	{
		background_line = FALSE;
	}
#endif // WIDGET_RUNTIME_SUPPORT

	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	INT32 state = 0;
	OpSkinElement *item_skin_elm = NULL;
	if (const char *skin = item->GetSkin(this))
	{
		if (IsFocused())
			state |= SKINSTATE_FOCUSED;
		if (item->IsSelected() || pos == m_selected_item)
			state |= SKINSTATE_SELECTED;
		if (item->IsDisabled())
			state |= SKINSTATE_DISABLED;
		item_skin_elm = GetSkinManager()->GetSkinElement(skin);
	}

	//  ----------------------------------------------------------------------
	//  Paint the background :
	//  ----------------------------------------------------------------------

	// never paint any background on custom widgets
	if (!item->IsCustomWidget() && widget_painter)
		PaintItemBackground(pos, item_skin_elm, state, background_line, rect);

	//  ----------------------------------------------------------------------
	//  Adjust the rectangles :
	//  ----------------------------------------------------------------------

	if (item_skin_elm)
	{
		// Add skin padding to the item content rect
		INT32 left, top, right, bottom;
		if (OpStatus::IsSuccess(item_skin_elm->GetPadding(&left, &top, &right, &bottom, state)))
		{
			rect.x += left;
			rect.y += top;
			rect.width  -= left + right;
			rect.height -= top  + bottom;
		}
	}

	const OpRect full_item_rect = rect;

	if(!m_allow_multi_line_icons)
		rect.height = m_line_height;

	OpRect item_rect = rect;

	OpRect associated_text_rect = rect;
	OpRect associated_image_rect;
	int associated_image_left_margin = 0;
	// m_extra_line_height affects items with associated text twice! Moving the associated text up with m_extra_line_height
	// looks odd, but better than not doing it. Ideally we would have support for items with different height so it could be added only once.
	if (item->HasAssociatedTextOnTop())
	{
		item_rect.y += m_line_height - m_extra_line_height;
	}
	else
	{
		associated_text_rect.y     += m_line_height - m_extra_line_height;
		associated_text_rect.height = m_line_height;
	}

	ItemDrawArea* associated_image = NULL;

	if (!m_only_show_associated_item_on_hover || pos == m_selected_item)
		associated_image = item->GetAssociatedImage(this);

	if(associated_image)
	{
		CalculateAssociatedImageSize(associated_image, rect, associated_image_rect);
		associated_image_left_margin = associated_image_rect.x;
		associated_image_rect.x = item_rect.x + item_rect.width - associated_image_rect.width;
	}
	//  ----------------------------------------------------------------------
	//  Draw the column fields :
	//  ----------------------------------------------------------------------

	int line_count = 1;
	INT32 visible_col_num = 0;

	// This lets us avoid item text to jump to the right upon selection when in
	// RTL.  FIXME: Remove when DSK-359759 is solved better.
	const bool must_erase_image_background = GetRTL() && item->IsSelected() && m_forced_focus;

	if(item->IsCustomWidget() && widget_painter)
	{
		// this is a custom widget and it will handle its own painting
		OpTreeModelItem::ItemData item_data(WIDGET_QUERY);

		item->GetItemData(&item_data);

		if(item_data.widget_query_data.widget)
		{
			item_data.widget_query_data.widget->SetRect(AdjustForDirection(item_rect));
		}
	}
	else
	for (UINT32 i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);

		if (!column->IsColumnReallyVisible())
		{
			continue;
		}
		// we need to get the column data for the item to check if the span of the column should change
		ItemDrawArea* cache = item->GetItemDrawArea(this, i, FALSE);
		if(!cache)
		{
			break;
		}
		if (cache->m_custom_cell_widget && !cache->m_custom_cell_widget->GetParent())
		{
			// we should always add the widget, so that it has a parent and is scheduled for deletion
			// so never return before this point
			AddChild(cache->m_custom_cell_widget);
			m_custom_widgets.Add(cache->m_custom_cell_widget);
			cache->m_custom_cell_widget->SetListener(this);
		}

		OpRect cell_rect;
		if (cache->m_column_span > 1)
		{
			// item spans over multiple columns
			INT32 span = cache->m_column_span - 1;
			OpRect span_cell_rect = item_rect;
			INT32 max_cnt = min(span + i, m_columns.GetCount());
			item_rect.width = rect.width - item_rect.x;
			while((INT32)i < max_cnt)
			{
				Column* next_column = m_columns.Get(++i);
				OP_ASSERT(next_column);

				span_cell_rect.width += next_column->m_width;
			}
			span_cell_rect.width = span_cell_rect.width - associated_image_rect.width - associated_image_left_margin;

			if (!m_show_horizontal_scrollbar)
				span_cell_rect.width = MIN(span_cell_rect.width, item_rect.x + item_rect.width - span_cell_rect.x);

			cell_rect = span_cell_rect;
			item_rect.x += span_cell_rect.width;
		}
		else
		{
			item_rect.width = column->m_width;
			if (!must_erase_image_background && item_rect.Contains(associated_image_rect))
				item_rect.width -= associated_image_rect.width + associated_image_left_margin;

			cell_rect = item_rect;
			item_rect.x += column->m_width;
		}

		if (paint_rect.Intersecting(AdjustForDirection(cell_rect)))
		{
			if (widget_painter)
				PaintCell(widget_painter, item_skin_elm, state, pos, column, cell_rect, list);
			else
				OpStatus::Ignore(AddQuickWidgetInfoToList(*list, item, i, cell_rect, pos, visible_col_num));
		}

		visible_col_num++;
	}

	if(!item->IsCustomWidget() && widget_painter)
	{
		//  ----------------------------------------------------------------------
		//  Draw the associated text :
		//  ----------------------------------------------------------------------

		ItemDrawArea* associated_text = item->GetAssociatedText(this);

		if(associated_text && item->GetNumLines() > 1)
		{
			// Add some padding to make it pretty
			associated_text_rect.x += 5;
			associated_text_rect.width -= 5;

			if (!HasMatchParentID())
			{
				INT32 depth = 0;
				INT32 parent = m_view_model.GetParentIndex(pos);

				while (parent != -1 && !m_view_model.GetItemByIndex(parent)->IsHeader())
				{
					depth++;
					parent = m_view_model.GetParentIndex(parent);
				}

				if (associated_text->m_indentation != -1)
				{
					// we have a custom indentation for this column, so use that instead
					associated_text_rect.x += associated_text->m_indentation;
					associated_text_rect.width -= associated_text->m_indentation;
				}
				else
				{
					associated_text_rect.x += m_thread_indentation * depth;
					associated_text_rect.width -= m_thread_indentation * depth;
				}

				if (m_show_thread_image && !HasMatchText() && !item->IsSeparator())
				{
					const OpRect thread_rect = PaintThreadImage(!widget_painter || !item->HasAssociatedTextOnTop(), item, depth, associated_text_rect);
					associated_text_rect.x += thread_rect.width + 7;
					associated_text_rect.width -= thread_rect.width + 7;
				}
			}

			if(associated_image && associated_image->m_image != NULL)
			{
				OpWidgetImage associated_img;
				associated_img.SetImage(associated_image->m_image);
				if (associated_image->m_image_state != -1)
				{
					associated_img.SetState(associated_image->m_image_state);
				}

				OpRect image_rect = associated_img.CalculateScaledRect(full_item_rect, FALSE, TRUE);

				INT32 w;
				w = image_rect.width;

				INT32 left, top, right, bottom;
				GetPadding(&left, &top, &right, &bottom);

				if(w + right < associated_text_rect.width)
				{
					image_rect.x = associated_text_rect.x + associated_text_rect.width - w - right;
					if(m_hover_associated_image_item == pos && associated_img.GetState() != SKINSTATE_DISABLED)
					{
						associated_img.SetState(SKINSTATE_HOVER, -1, 100);
						associated_img.Draw(vis_dev, AdjustForDirection(image_rect), NULL, SKINSTATE_HOVER);
					}
					else
					{
						associated_img.Draw(vis_dev, AdjustForDirection(image_rect));
					}
					associated_text_rect.width -= w + right;
				}
			}

			if (widget_painter)
				PaintText(widget_painter, item_skin_elm, state, associated_text_rect, item, associated_text, m_have_weak_associated_text, FALSE, associated_text->m_flags & OpTreeModelItem::FLAG_BOLD);

			line_count++;
		}
		else if(associated_image && associated_image->m_image != NULL)
		{
			// we have an image but no text

			if (must_erase_image_background)
			{
				OpRect background_rect;
				GetItemRectLTR(pos, background_rect, FALSE);
				OpRect erase_rect = background_rect;
				erase_rect.x = associated_image_rect.x - associated_image_left_margin;
				erase_rect.width = background_rect.Right() - erase_rect.Left();
				PaintItemBackground(pos, item_skin_elm, state, background_line, erase_rect);
			}

			OpWidgetImage associated_img;
			associated_img.SetImage(associated_image->m_image);
			if (associated_image->m_image_state != -1)
			{
				associated_img.SetState(associated_image->m_image_state);
			}
			if(m_hover_associated_image_item == pos && associated_img.GetState() != SKINSTATE_DISABLED)
			{
				associated_img.SetState(SKINSTATE_HOVER, -1, 100);
				associated_img.Draw(vis_dev, AdjustForDirection(associated_image_rect), NULL, SKINSTATE_HOVER);
			}
			else
			{
				associated_img.Draw(vis_dev, AdjustForDirection(associated_image_rect));
			}
		}
	}
	return line_count;
}

void OpTreeView::PaintItemBackground(INT32 pos, OpSkinElement* item_skin_element, INT32 skin_state, BOOL background_line, const OpRect& rect)
{
	if (item_skin_element)
	{
		item_skin_element->Draw(GetVisualDevice(), AdjustForDirection(rect), skin_state, 0, NULL);
	}
	else
	{
		const INT32 row_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::ColorListRowMode);
		TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

		if ((item->IsSelected() && !m_forced_focus) || (pos == m_drop_pos && m_insert_type == DesktopDragObject::INSERT_INTO))
		{
			PaintSelectedItemBackground(rect, pos == m_selected_item);
		}
		else if ( (item->IsSelected() && m_forced_focus) ||
			(background_line && (m_visible_column_count > 2 || m_force_background_line) && row_mode != 2 ))
		{
			INT32 color = 0;

			if (item->IsSelected())
			{
				color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
			}
			else if(m_custom_background_line_color < 0)
			{
				double bg_h, bg_s, bg_v;

				OpSkinUtils::ConvertRGBToHSV(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND), bg_h, bg_s, bg_v);

				if (bg_s == 0)
				{
					BOOL is_dark_background = bg_v < 0.5;
					if (is_dark_background && row_mode == 1)
					{
						// Assumes bright text on dark background
						bg_v += 0.1;
					}
					else
					{
						// Assumes dark text on bright background
						OpSkinUtils::ConvertRGBToHSV(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED), bg_h, bg_s, bg_v);
						bg_v = 0.98;
						bg_s = 0.02;
					}
				}
				else
				{
					bg_v -= 0.03;

					if (bg_v < 0)
						bg_v = 0;
				}
				color = OP_RGBA(0, 0, 0, 255) | OpSkinUtils::ConvertHSVToRGB(bg_h, bg_s, bg_v);
			}
			else
			{
				// m_custom_background_line_color is an INT32 which causes the alpha to be destroyed, so remove it here.
				color = (m_custom_background_line_color & OP_RGBA(0, 0, 0, 255)) | m_custom_background_line_color;
			}

			GetVisualDevice()->SetColor32(color);
			GetVisualDevice()->FillRect(AdjustForDirection(rect));
		}
	}
}

/***********************************************************************************
**
**	PaintSelectedItemBackground
**
**  This function is used instead of IndpWidgetPainter::DrawTreeViewRow in order to
** allow colors taken from the skin. System colors are still used as fallback.
**
**  @param paint_rect      - the rectangle to be painted in
**
**  @return success or no success
***********************************************************************************/
void OpTreeView::PaintSelectedItemBackground(const OpRect &paint_rect, BOOL is_selected_item)
{
	UINT32 color;

	if (IsFocused())
	{
		if (OpStatus::IsError(GetSkinManager()->GetBackgroundColor(GetBorderSkin()->GetImage(), &color, SKINSTATE_FOCUSED)))
		{
			color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
		}
	}
	else
	{
		if (OpStatus::IsError(GetSkinManager()->GetBackgroundColor(GetBorderSkin()->GetImage(), &color, SKINSTATE_SELECTED)))
		{
			BOOL fix_color = TRUE;

			BOOL native_nofocus_bg = GetSkinManager()->GetCurrentSkin()->GetOptionValue("Native bgcolor nofocus") == 1;
			if (native_nofocus_bg)
			{
				// get system color
				color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS);
				fix_color = FALSE;  // use it blindly
			}
			else
			{
				// calculate desaturated version of the color used for selected item
				color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
			}

			if (fix_color)
			{
				double bg_h, bg_s, bg_v;
				OpSkinUtils::ConvertBGRToHSV(color, bg_h, bg_s, bg_v);

				BOOL is_dark_background = bg_v < 0.6;
				if (is_dark_background)
				{
					// Assumes dark text on dark background
					bg_s = 0.10;
					bg_v = 0.70;
				}
				else
				{
					bg_v = MAX(0, bg_v - 0.03);
					/* don't go to 0 which would give completely desaturated
					   color - black, white or some shade in between */
					bg_s = MAX(0.08, bg_s - 0.40);
				}

				color = OP_RGBA(0, 0, 0, 255) | OpSkinUtils::ConvertHSVToBGR(bg_h, bg_s, bg_v);
			}
			else
			{
				color = OP_RGBA(0, 0, 0, 255) | color;
			}
		}
	}
	GetVisualDevice()->SetColor32(color);
	GetVisualDevice()->FillRect(AdjustForDirection(paint_rect));

	if (IsFocused() && is_selected_item && GetSkinManager()->GetDrawFocusRect())
	{
		GetVisualDevice()->DrawFocusRect(AdjustForDirection(paint_rect));
	}
}


/***********************************************************************************
**
**	PaintCell
**
**  @param widget_painter - to be used when painting
**  @param pos            - of the item to be painted
**  @param column         - that indicates this cell
**  @param cell_rect      - the rectangle that should be used
**
***********************************************************************************/
void OpTreeView::PaintCell(OpWidgetPainter* widget_painter,
						   OpSkinElement *item_skin_elm,
						   INT32 state,
						   INT32 pos,
						   Column* column,
						   OpRect& cell_rect,
						   OpScopeDesktopWindowManager_SI::QuickWidgetInfoList *list)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	if(item->IsCustomWidget())
	{
		return;
	}

	int start_x = cell_rect.x;

	cell_rect.x += 2;
	cell_rect.width -= 2;

	//  ----------------------------------------------------------------------
	//  Get the column information :
	//  ----------------------------------------------------------------------

	ItemDrawArea* cache = item->GetItemDrawArea(this, column->m_column_index, TRUE);
	if (!cache)
		return;

	//  ----------------------------------------------------------------------
	//  Threading :
	//  ----------------------------------------------------------------------

	if (column->m_column_index == m_thread_column && !HasMatchParentID() && !item->IsHeader())
	{
		INT32 depth = 0;
		INT32 parent = m_view_model.GetParentIndex(pos);

		while (parent != -1 && !m_view_model.GetItemByIndex(parent)->IsHeader())
		{
			depth++;
			if (m_show_thread_image && cache->m_indentation == -1 && m_view_model.GetItemByIndex(parent)->HasAssociatedTextOnTop())
			{
				// not needed since the [+] thread expander will be on the associated_text line
				cell_rect.x -= m_thread_indentation;
				cell_rect.width += m_thread_indentation;
			}
			parent = m_view_model.GetParentIndex(parent);
		}
		
		if(cache->m_indentation != -1)
		{
			// we have a custom indentation for this column, so use that instead
			cell_rect.x += cache->m_indentation;
			cell_rect.width -= cache->m_indentation;
		}
		else
		{
			INT32 indentation, half_width_depth = column->m_width / (m_thread_indentation * 2) + 1;

			// it needs to do depth*m_thread_indentation up to a certain point, then width/(depth*2)
			if (depth > half_width_depth)
			{
				int ind = column->m_width / (depth*2);
				indentation = column->m_width / 2 + ind*(depth - half_width_depth);
			}
			else
			{
				indentation = m_thread_indentation * depth;
			}

			cell_rect.x += indentation;
			cell_rect.width -= indentation;
		}

		if (m_show_thread_image && !HasMatchText() && !item->IsSeparator() && !item->HasAssociatedTextOnTop())
		{
			const OpRect thread_rect = PaintThreadImage(!widget_painter, item, depth, cell_rect);
			cell_rect.x += thread_rect.width + 2;
			cell_rect.width -= thread_rect.width + 2;
		}
	}

	//  ----------------------------------------------------------------------
	//  Drag and drop :
	//  ----------------------------------------------------------------------

	if (widget_painter && pos == m_drop_pos && (m_insert_type == DesktopDragObject::INSERT_BEFORE || m_insert_type == DesktopDragObject::INSERT_AFTER))
	{
		OpRect drop_line;
		INT32 width, height;
		const char* element = "Horizontal Drop Insert";
		
		GetSkinManager()->GetSize(element, &width, &height);
		drop_line.y = cell_rect.y - height/2;

		if (m_insert_type == DesktopDragObject::INSERT_AFTER)
		{
			drop_line.y += cell_rect.height;
		}
		
		drop_line.height = height;
		drop_line.x = cell_rect.x - 2;
		drop_line.width = cell_rect.width + 2;

		GetSkinManager()->DrawElement(vis_dev, element, AdjustForDirection(drop_line));
	}

	//  ----------------------------------------------------------------------
	//  Seperator :
	//  ----------------------------------------------------------------------

	if (item->IsSeparator() && widget_painter)
	{
		ItemDrawArea* cache = item->GetItemDrawArea(this, column->m_column_index, TRUE);

		ItemDrawArea::StringList* string = 0;

		if ( cache )
		{
			string = &cache->m_strings;

			if ( string ) // Split the line, draw it on each side of the string
			{
				INT32 str_width            = string->m_string.GetWidth();
				INT32 length_first_line    = (cell_rect.width + 2 - str_width ) / 2;
				INT32 start_of_second_line = cell_rect.x - 2 + length_first_line + str_width;

				if (str_width > 0)
				{
					OpPoint p1(cell_rect.x - 2, cell_rect.y + cell_rect.height / 2);
					OpPoint p2(start_of_second_line + 4, cell_rect.y + cell_rect.height / 2);
					vis_dev->SetColor32(SEPARATOR_DARK);
					vis_dev->DrawLine(p1, length_first_line - 4, TRUE); p1.y++;
					vis_dev->DrawLine(p2, length_first_line,     TRUE); p2.y++;
					vis_dev->SetColor32(SEPARATOR_BRIGHT);
					vis_dev->DrawLine(p1, length_first_line - 4, TRUE);
					vis_dev->DrawLine(p2, length_first_line,     TRUE);
				}
			}
		}

		if (!string || string->m_string.GetWidth() == 0 )
		{
			vis_dev->SetColor32(SEPARATOR_DARK);
			vis_dev->DrawLine(OpPoint(cell_rect.x - 2, cell_rect.y + cell_rect.height / 2), cell_rect.width + 2, TRUE);
			vis_dev->SetColor32(SEPARATOR_BRIGHT);
			vis_dev->DrawLine(OpPoint(cell_rect.x - 2, cell_rect.y + cell_rect.height / 2 + 1), cell_rect.width + 2, TRUE);
			if (!string) // If the separator has text this must also be drawn
				return;
		}
	}

	//  ----------------------------------------------------------------------
	//  Checkboxes :
	//  ----------------------------------------------------------------------

	if (column->m_column_index == m_checkable_column)
	{
		INT32 size = cell_rect.height - 4;

		if (widget_painter)
		{
			OpRect box_rect(cell_rect.x, cell_rect.y + m_line_height / 2 - size / 2, size, size);
			widget_painter->DrawCheckbox(AdjustForDirection(box_rect), item->m_flags & OpTreeModelItem::FLAG_CHECKED);
		}

		cell_rect.x		+= size + 3;
		cell_rect.width	-= size + 3;
	}

	//  ----------------------------------------------------------------------
	//  Image :
	//  ----------------------------------------------------------------------

	if (cache->m_image || (!cache->m_bitmap_image.IsEmpty()))
	{
		if(!item->m_widget_image)
		{
			item->m_widget_image = OP_NEW(OpWidgetImage, ());
			if(!item->m_widget_image)
				return;
		}
		OpWidgetImage* cell_image = item->m_widget_image;

		cell_image->SetListener(this);

		cell_image->SetRestrictImageSize(m_restrict_image_size);

		if (HasMatchParentID() && GetItemByPosition(pos)->GetID() == m_match_parent_id)
		{
			cell_image->SetImage("Parent Folder");
			cell_image->SetState(0);
		}
		else
		{
			BOOL open = !HasMatchText() && !HasMatchParentID() && item->IsOpen();

			INT32 state = 0;

			if (open)
				state |= SKINSTATE_OPEN;

			if (item->IsDisabled())
				state |= SKINSTATE_DISABLED;

			if (GetRTL())
				state |= SKINSTATE_RTL;

			if (item->IsFocused())
			{
				state |= SKINSTATE_FOCUSED;
			}

			cell_image->SetImage(cache->m_image);
			cell_image->SetBitmapImage(cache->m_bitmap_image, FALSE);
			cell_image->SetState(state);
		}

		if (cell_image->HasContent())
		{
			OpRect rect = cell_image->CalculateScaledRect(cell_rect, FALSE, TRUE);

			BOOL do_adjust = FALSE;

			if (column->m_fixed_image_drawarea) // Images that are smaller is centerd within the area
			{
				// 1) rect.width = image width
				// 2) width      = fixed image width
				// 3) center the image width inside the broad image width
				// 4) from that get spacing on left and right side

				INT32 width = m_columns.Get(column->m_column_index)->m_image_fixed_width;

				if (width > rect.width)
				{
					INT32 left_space = (width - rect.width) / 2;
					rect.x          += left_space;
 					do_adjust        = TRUE;
				}
			}

			if (widget_painter)
			{
				cell_image->Draw(vis_dev, AdjustForDirection(rect));

				if (cache->m_image_2)
				{
					cell_image->SetImage(cache->m_image_2);
					cell_image->Draw(vis_dev, AdjustForDirection(rect));
				}
			}

 			if (column->m_fixed_image_drawarea && do_adjust)
 			{
				cell_rect.x += m_columns.Get(column->m_column_index)->m_image_fixed_width + 3;
 				cell_rect.width -= m_columns.Get(column->m_column_index)->m_image_fixed_width + 3;

 			}
			else
			{
				cell_rect.x += rect.width + 3;
				cell_rect.width -= rect.width + 3;
			}
		}
	}

	//  ----------------------------------------------------------------------
	//  Dropdown :
	//  ----------------------------------------------------------------------

	const char* dropdown_image = "Dropdown";

	INT32 dropdown_width = 0;
	INT32 dropdown_height = 0;

	if (column->m_has_dropdown && item->IsFocused())
	{
		GetSkinManager()->GetSize(dropdown_image, &dropdown_width, &dropdown_height);
	}

	
	//  ----------------------------------------------------------------------
	//  Custom Cell Widget:
	//  ----------------------------------------------------------------------

	if (cache->m_custom_cell_widget != NULL)
	{
		INT32 w, h;
		cache->m_custom_cell_widget->SetVisualDevice(vis_dev);
		cache->m_custom_cell_widget->GetRequiredSize(w, h);
		if (w+10 >= GetVisibleRectLTR(false, true).width)
			return;

		if (w > cell_rect.width)
		{
			column->m_fixed_width = w+2;
			Relayout();
			return;
		}
		INT32 w_to_use = min(w, cell_rect.width), x_to_use;

		x_to_use = cell_rect.x;
		if (cache->m_flags & FLAG_RIGHT_ALIGNED)
			x_to_use += (cell_rect.width - w_to_use);
		if (cache->m_flags & FLAG_CENTER_ALIGNED)
			x_to_use += (cell_rect.width - w_to_use) / 2;

		OpRect custom_widget_rect(x_to_use, cell_rect.y, w_to_use, cell_rect.height);

		SetChildRect(cache->m_custom_cell_widget, custom_widget_rect, FALSE);

		RETURN_VOID_IF_ERROR(m_custom_painted_widgets.Add(cache->m_custom_cell_widget));
		cache->m_custom_cell_widget->GenerateOnPaint(AdjustForDirection(cell_rect));
		return;
	}
	else if (cache->m_progress)
	{
		//  ----------------------------------------------------------------------
		//  Progressbar :
		//  ----------------------------------------------------------------------
	
		int inset = 1;

#ifdef _MACINTOSH_
		// Because of the fact mac progress bars can't change size they are only big or small
		OpSkinElement* prog_element = g_skin_manager->GetSkinElement("Progress General Skin");
		if (prog_element && prog_element->IsNative())
			inset = -1;
#endif // _MACINTOSH_

		cell_rect = cell_rect.InsetBy(inset, inset);
		const JUSTIFY old_justify = font_info.justify;
		font_info.justify = JUSTIFY_CENTER;
		widget_painter->DrawProgressbar(AdjustForDirection(cell_rect), cache->m_progress, 0, &cache->m_strings.m_string);
		font_info.justify = old_justify;
	}
	else
	{
		//  ----------------------------------------------------------------------
		//  Paint Text :
		//  ----------------------------------------------------------------------

		int cell_width = cell_rect.x - start_x;

		BOOL link = column->m_column_index == m_link_column && pos == m_hover_item;

		BOOL bold = (cache->m_flags & OpTreeModelItem::FLAG_BOLD) || item->IsTextSeparator();

		if (m_bold_folders && !bold)
		{
			INT32 subtree_size = m_view_model.GetSubtreeSize(pos);
			for (INT32 i = pos + 1; i < pos + 1 + subtree_size && !bold; i++)
			{
				TreeViewModelItem* child_view_item = m_view_model.GetItemByIndex(i);
				OpTreeModelItem*   child_item      = child_view_item ? child_view_item->GetModelItem() : 0;

				if (child_item)
				{
					OpTreeModelItem::ItemData item_data(BOLD_QUERY);
					child_item->GetItemData(&item_data);
					bold = item_data.flags & OpTreeModelItem::FLAG_BOLD;
				}
			}
		}

		if(cell_rect.height > m_line_height)
			cell_rect.height = m_line_height;

		if (cache->m_flags & OpTreeModelItem::FLAG_VERTICAL_ALIGNED)
		{
			cell_rect.height *= item->GetNumLines();
		}
		if(item_skin_elm)
		{
			INT32 spacing = 0;

			// skin might have custom spacing defined, let's use that if defined
			if(OpStatus::IsSuccess(item_skin_elm->GetSpacing(&spacing, state)))
			{
				cell_rect.width -= spacing;
				cell_rect.x += spacing;

				cell_width = cell_rect.width;
			}
		}
		PaintText(widget_painter, item_skin_elm, state, cell_rect, item, cache, FALSE, link, bold, dropdown_width, &cell_width);

		// If the horizontal scrollbar is on we need to force the fixed width so the columns don't resize for the
		// visible rect of the control.
		if (m_show_horizontal_scrollbar && column->m_fixed_width < cell_width)
		{
			column->m_fixed_width = cell_width;
			Relayout();
		}
	}

	cell_rect.x += 3;
	cell_rect.width -= 3;

	if (dropdown_width && cell_rect.width > 0)
	{
		OpRect image_rect;
		image_rect.x = cell_rect.x;
		image_rect.y = cell_rect.y + (m_line_height - dropdown_height) / 2;
		GetSkinManager()->GetSize(dropdown_image, &image_rect.width, &image_rect.height);
		GetSkinManager()->DrawElement(vis_dev, dropdown_image, AdjustForDirection(image_rect));
	}
}

OpRect OpTreeView::PaintThreadImage(bool get_size_only, TreeViewModelItem* item,
		int depth, const OpRect& parent_rect)
{
	const char image[] = "Thread";

	OpRect image_rect;
	GetSkinManager()->GetSize(image, &image_rect.width, &image_rect.height);

	if (!get_size_only && (item->m_flags & FLAG_MATCHED_CHILD) && (!m_root_opens_all || depth == 0))
	{
		INT32 state = item->IsOpen() ? SKINSTATE_OPEN : 0;
		state |= GetRTL() ? SKINSTATE_RTL : 0;

		image_rect.x = parent_rect.x;
		image_rect.y = parent_rect.y + m_line_height / 2 - image_rect.height / 2;
		GetSkinManager()->DrawElement(vis_dev, image, AdjustForDirection(image_rect), state);
	}

	return image_rect;
}

/***********************************************************************************
**
**	PaintText
**
**  @param widget_painter     - to be used when painting
**  @param cell_rect          - the rectangle that should be used
**  @param item               - the item that is being painted
**  @param cache              - the drawarea that should be drawn
**  @param use_weak_style     - if the text should be drawn in "weak style" (grayish)
**  @param use_link_style     - if the text should be drawn like a link
**  @param use_bold_style     - if the text should be drawn bold
**  @param right_offset       - how much of the right that should be ignored
**  @param cell_width         - Adds the width of the text to the width passed in
**
***********************************************************************************/
void OpTreeView::PaintText(OpWidgetPainter* widget_painter,
						   OpSkinElement *item_skin_elm,
						   INT32 state,
						   OpRect& cell_rect,
						   TreeViewModelItem* item,
						   ItemDrawArea* cache,
						   BOOL use_weak_style,
						   BOOL use_link_style,
						   BOOL use_bold_style,
						   int right_offset,
						   int *cell_width)
{
	if(!widget_painter || !item || !cache)
	{
		// this always asserts when EditItem() is called on an item because PaintCell() is called with a NULL widget painter
//		OP_ASSERT(FALSE);
		return;
	}

	if (m_save_strings_that_need_refresh)
		m_item_ids_that_need_string_refresh.Insert(item->GetID());

	//  ----------------------------------------------------------------------
	//  Text color :
	//  ----------------------------------------------------------------------

	UINT32 color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT);

	if (item_skin_elm)
	{
		// Hacky, but some columns will fetch colors from skin and put it in column_text_color. Those should override the item skin.
		if (cache->m_color != 0xffffffff && !item->IsSelected())
			color = cache->m_color;
		else
			item_skin_elm->GetTextColor(&color, state);
	}
	else if (item->IsDisabled())
	{
		// returns OpStatus::ERR if no color is defined in the skin
		if(OpStatus::IsError(g_skin_manager->GetTextColor(GetBorderSkin()->GetImage(), &color, GetBorderSkin()->GetState() | SKINSTATE_DISABLED)))
		{
			color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
		}
	}
	else if (item->IsTextSeparator())
	{
		color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS);
	}
	else if (use_link_style)
	{
		color = OP_RGB(0x28, 0x50, 0x78);
	}
	else if (item->IsSelected() && !cache->m_progress)
	{
		if (IsFocused() || m_forced_focus)
			color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
		else
			color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
//			color = GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT);
	}
	else if(use_weak_style)
	{
		color = OP_RGB(0x74, 0x74, 0x74);
	}
	else if (cache->m_color != 0xffffffff)
	{
		color = cache->m_color;
	}

	//  ----------------------------------------------------------------------
	//  Text boldness :
	//  ----------------------------------------------------------------------

	// We must restore boldness after drawing bold element
	int font_weight = font_info.weight;

	if (use_bold_style)
	{
		font_info.weight = QuickOpWidgetBase::WEIGHT_BOLD;
	}

	//  ----------------------------------------------------------------------
	//  Text italics :
	//  ----------------------------------------------------------------------

	if (cache->m_flags & OpTreeModelItem::FLAG_ITALIC)
	{
		font_info.italic = TRUE;
	}

	//  ----------------------------------------------------------------------
	//  Draw text :
	//  ----------------------------------------------------------------------

	// Force right-alignment when in RTL.
	// FIXME: Solve DSK-359759 better.

	cell_rect = AdjustForDirection(cell_rect);

	if (GetRTL() || (cache->m_flags & OpTreeModelItem::FLAG_RIGHT_ALIGNED) || (cache->m_flags & OpTreeModelItem::FLAG_CENTER_ALIGNED))
	{
		INT32 string_width = 3;

		for (ItemDrawArea::StringList* string = &cache->m_strings; string; string = string->m_next)
		{
			int prev_weight = font_info.weight;
			BOOL prev_italic = font_info.italic;

			if (string->m_string_flags & ItemDrawArea::FLAG_BOLD)
				font_info.weight = QuickOpWidgetBase::WEIGHT_BOLD;
			if (string->m_string_flags & ItemDrawArea::FLAG_ITALIC)
				font_info.italic = TRUE;
			string_width += string->m_string.GetWidth();

			font_info.weight = prev_weight;
			font_info.italic = prev_italic;
		}

		if (cell_rect.width >= string_width + right_offset)
		{
			INT32 offset = cell_rect.width - string_width + right_offset;
			if (cache->m_flags & OpTreeModelItem::FLAG_CENTER_ALIGNED)
				offset = (cell_rect.width - string_width) / 2 + right_offset;
			cell_rect.x += offset - 3;
 			cell_rect.width -= offset + 3;
		}
	}

	const JUSTIFY prev_justify = font_info.justify;
	font_info.justify = JUSTIFY_LEFT;

	for (ItemDrawArea::StringList* string = &cache->m_strings; string; string = string->m_next)
	{
		int prev_weight = font_info.weight;
		BOOL prev_italic = font_info.italic;
		UINT32 prev_color = color;

		if (string->m_string_flags & ItemDrawArea::FLAG_BOLD)
			font_info.weight = QuickOpWidgetBase::WEIGHT_BOLD;
		if (string->m_string_flags & ItemDrawArea::FLAG_ITALIC)
			font_info.italic = TRUE;
		if (string->m_string_flags & ItemDrawArea::FLAG_COLOR)
			color = string->m_string_color;

		int tmp_string_width = string->m_string.GetWidth();
		// Add the width of the actual text to the cell width
		if (cell_width)
			*cell_width += tmp_string_width + right_offset;

		if (item->m_flags & FLAG_SEPARATOR)
		{
			/* Separator text is centered within the cell_rect */
			INT32 str_width  = string->m_string.GetWidth();
			INT32 difference = cell_rect.width - str_width;
			cell_rect.x     += difference / 2;
		}

		OpWidgetStringDrawer string_drawer;
		string_drawer.SetEllipsisPosition(ELLIPSIS_END);
		string_drawer.SetUnderline(use_link_style);
		string_drawer.Draw(&string->m_string, cell_rect, GetVisualDevice(), color);

		font_info.weight = prev_weight;
		font_info.italic = prev_italic;
		color = prev_color;
		cell_rect.x += tmp_string_width;
		cell_rect.width -= tmp_string_width;
	}

	// Restore font_info.
	font_info.weight = font_weight;
	font_info.italic = FALSE;
	font_info.justify = prev_justify;
}

/***********************************************************************************
 **
 **	UpdateItem
 **
 ***********************************************************************************/
void OpTreeView::UpdateItem(INT32 pos, BOOL clear, BOOL delay)
{
	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	if (!item)
		return;

	if (clear)
	{
		item->Clear();
	}

	// optimize that work even if tree is dirty.. if item is outside, don't query rect

	const OpRect rect = GetVisibleRectLTR();

	INT32 line_at_top		= m_vertical_scrollbar->GetValue() / m_line_height;
	INT32 line_at_bottom	= (m_vertical_scrollbar->GetValue() + rect.height) / m_line_height + 1;

	INT32 item_at_top		= GetItemByLine(line_at_top);
	INT32 item_at_bottom	= GetItemByLine(line_at_bottom);

	if (item_at_top != -1 && item_at_bottom != -1 && (item_at_top > pos || item_at_bottom < pos))
	{
		return;
	}

	// if updates start to arrive frequently, delay redraws a bit

	INT32 count = m_items_that_need_redraw.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		TreeViewModelItem* compare_item = m_items_that_need_redraw.Get(i);

		if (item == compare_item)
			return;
	}

	m_items_that_need_redraw.Add(item);
	m_redraw_trigger.InvokeTrigger(delay ? OpDelayedTrigger::INVOKE_DELAY : OpDelayedTrigger::INVOKE_NOW);
}

/***********************************************************************************
**
**	UpdateDelayedItems
**
***********************************************************************************/
void OpTreeView::UpdateDelayedItems()
{
	INT32 count = m_items_that_need_redraw.GetCount();

	if (!count)
		return;

	for (INT32 i = 0; i < count; i++)
	{
		TreeViewModelItem* item = m_items_that_need_redraw.Get(i);

		OpRect rect;

		// make sure the item is still valid - fixes bug DSK-223018
		if(m_view_model.GetIndexByItem(item) > -1)  
		{
			if (GetItemRect(item->GetIndex(), rect))
				Invalidate(rect);
		}
	}

	m_items_that_need_redraw.Clear();
}

/***********************************************************************************
**
**	EditItem
**
***********************************************************************************/
void OpTreeView::EditItem(INT32 pos,
						  INT32 column_index,
						  BOOL always_use_full_column_width,
						  AUTOCOMPL_TYPE autocomplete_type)
{
	SetSelectedItem(pos);

	INT32 line = GetLineByItem(pos);

	if (line == -1)
	{
		return;
	}

	if (m_edit)
	{
		FinishEdit();
	}

	OpRect rect;

	if (GetCellRectLTR(pos, column_index, rect, TRUE))
	{
		ScrollToLine(line);

		m_edit = OP_NEW(Edit, (this));
		if (!m_edit) return;
		m_edit_pos = pos;
		m_edit_column = column_index;

		m_edit->SetName(WIDGET_NAME_TREEITEM_EDITITEM);
		AddChild(m_edit, TRUE);

		m_edit->autocomp.SetType(autocomplete_type);

		ItemDrawArea* cache = m_view_model.GetItemByIndex(pos)->GetItemDrawArea(this, column_index, TRUE);

		if (cache)
		{
			if (!always_use_full_column_width)
			{
				if (cache->m_strings.m_string.GetWidth() < rect.width)
				{
					rect.width = cache->m_strings.m_string.GetWidth();

					if (rect.width < 100)
					{
						rect.width = 100;
					}
				}
			}

			m_edit->SetText(cache->m_strings.m_string.Get());
		}
		SetChildRect(m_edit, rect);

		OpRect text_rect = m_edit->GetTextRect();
		rect.x -= text_rect.x;
		rect.width += text_rect.x;

		SetChildRect(m_edit, rect);

		m_edit->SetFocus(FOCUS_REASON_OTHER);
	}
}

OpRect OpTreeView::GetEditRect()
{
	if(m_edit)
		return m_edit->GetRect(FALSE);
	else
		return OpRect();
}



/***********************************************************************************
**
**
**
***********************************************************************************/
void OpTreeView::CancelEdit(BOOL broadcast)
{
	if(listener && broadcast)
	{
		OpString text;

		m_edit->GetText(text);

		listener->OnItemEditAborted(this, m_edit_pos, m_edit_column, text);
	}
	if (m_edit)
	{
		Edit* edit = m_edit;
		m_edit = NULL;
		edit->Delete();
	}
	SetFocus(FOCUS_REASON_OTHER);
}

/***********************************************************************************
**
**	OpenItem
**
***********************************************************************************/
void OpTreeView::OpenItem(INT32 pos, BOOL open, BOOL scroll_to_fit)
{
	if (HasMatchText() || HasMatchParentID() || pos < 0)
	{
		return;
	}

	if (IsItemOpen(pos) != open)
	{
		// not able to close subtrees when m_root_opens_all is set

		if (!open && m_root_opens_all && m_view_model.GetParentIndex(pos) != -1 &&  !m_view_model.GetParentItem(pos)->IsHeader())
		{
			return;
		}

		if (!OpenItemRecursively(pos, open, m_root_opens_all && !m_view_model.GetItemByIndex(pos)->IsHeader()))
		{
			return;
		}

		Relayout();
		TreeChanged();
		if (open && scroll_to_fit)
		{
			// if opening, scroll so that parent and as many children as possible is visible

			INT32 lines_to_show = 1;
			INT32 subtree_size = m_view_model.GetSubtreeSize(pos);

			for (INT32 i = 0; i < subtree_size; i++)
			{
				if (IsItemVisible(pos + i + 1))
				{
					lines_to_show+=m_view_model.GetItemByIndex(pos+i+1)->GetNumLines();
				}
			}

			const OpRect rect = GetVisibleRectLTR();

			INT32 lines_per_page = rect.height / m_line_height;

			if (lines_per_page < lines_to_show)
			{
				lines_to_show = lines_per_page;
			}

			INT32 line = GetLineByItem(pos);

			ScrollToLine(line);

			if (lines_to_show > 1)
			{
				ScrollToLine(line + lines_to_show - 1);
			}	
		}
		if (m_view_model.GetItemByIndex(pos)->IsHeader() && (GetSelectedItemPos() == -1 || GetSelectedItemPos() >= pos && GetSelectedItemPos() <= pos + m_view_model.GetItemByIndex(pos)->GetSubtreeSize()))
		{
			TreeViewModelItem* item;
			int count = m_view_model.GetCount();
			if (pos == -1)
				pos = 0;
			
			while (pos < count && (item = m_view_model.GetItemByIndex(pos)) && item->IsHeader())
			{
			  if (!item->IsOpen())
				pos += item->GetSubtreeSize();
			  pos++;
			}

			SelectItem(pos < count ? pos : -1);
		}
	}
}

/***********************************************************************************
**
**	OpenAllItems
**
***********************************************************************************/
void OpTreeView::OpenAllItems(BOOL open)
{
	if (HasMatchText() || HasMatchParentID())
	{
		return;
	}

	for (INT32 i = 0; i < m_item_count; i++)
	{
		TreeViewModelItem* item = m_view_model.GetItemByIndex(i);
		if(open || m_close_all_headers || !item->IsHeader() || !open && (!item->IsOpen() || m_view_model.IsEmptyGroup(item)))
		{
				OpenItemRecursively(i, open, open, FALSE);
		}
	}
	UpdateLines();

	Relayout();

	if (m_selected_item != -1)
	{
		INT32 new_selected_item = m_selected_item;

		if (!open)
		{
			while (m_view_model.GetParentIndex(new_selected_item) != -1 && (m_close_all_headers || !m_view_model.GetParentItem(new_selected_item)->IsHeader()))
			{
				new_selected_item = m_view_model.GetParentIndex(new_selected_item);
			}
		}

		if (m_view_model.GetItemByIndex(new_selected_item)->IsHeader())
		{
			new_selected_item = -1;
		}

		if (new_selected_item != m_selected_item)
		{
			SelectItem(new_selected_item);
		}
		else
		{
			ScrollToItem(m_selected_item);
		}
	}
	TreeChanged();
}

/***********************************************************************************
**
** OpenItemRecursively
**
***********************************************************************************/
BOOL OpTreeView::OpenItemRecursively(INT32 pos, BOOL open, BOOL recursively, BOOL update_lines)
{
	// open parent if needed

	BOOL handled = FALSE;
	INT32 parent_pos = m_view_model.GetParentIndex(pos);

	if (open && !IsItemOpen(parent_pos))
	{
		if (OpenItemRecursively(parent_pos, TRUE, FALSE, update_lines))
			handled = TRUE;
	}

	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);
	INT32 child = m_view_model.GetChildIndex(pos);

	if (open != item->IsOpen())
	{
		ChangeItemOpenState(item, open);
		handled = TRUE;
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		AccessibilityManager::GetInstance()->SendEvent(item, Accessibility::Event(Accessibility::kAccessibilityStateExpanded));
#endif
	}

	if (update_lines && item->IsHeader() && !open)
	{
		INT32 subtree_size = m_view_model.GetSubtreeSize(pos);
		m_line_map.RemoveLines(child, pos + subtree_size);
		for (INT32 i = child; recursively && i <= pos + subtree_size; i++)
		{
			TreeViewModelItem* item = m_view_model.GetItemByIndex(i);
			ChangeItemOpenState(item, open);
		}
		return handled;
	}
	else if (update_lines && item->IsHeader() && open && child != -1)
	{
		INT32 subtree_size = m_view_model.GetSubtreeSize(pos);
		m_line_map.BeginInsertion();
		for (INT32 i = child; i <= pos + subtree_size; i++)
		{
			if (IsItemVisible(i))
			{
				TreeViewModelItem* item = m_view_model.GetItemByIndex(i);
				m_line_map.InsertItem(i, item->GetNumLines());
				if (recursively)
					ChangeItemOpenState(item, open);
			}
		}
		m_line_map.EndInsertion();
	}
	else if (update_lines)
		UpdateLines(pos);

	if (open && !recursively)
		return handled;

	while (child != -1)
	{
		if (OpenItemRecursively(child, open, TRUE, update_lines))
			handled =TRUE;

		child = m_view_model.GetSiblingIndex(child);
	}

	return handled;
}

/***********************************************************************************
**
**	ChangeItemOpenState
**
***********************************************************************************/
void OpTreeView::ChangeItemOpenState(TreeViewModelItem* item, BOOL open)
{
	if (open)
		item->m_flags |= OpTreeModelItem::FLAG_OPEN;
	else
		item->m_flags &= ~OpTreeModelItem::FLAG_OPEN;

	item->Clear();


#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityManager::GetInstance()->SendEvent(item, Accessibility::Event(Accessibility::kAccessibilityStateCheckedOrSelected));
#endif
	for (OpListenersIterator iterator(m_treeview_listeners); m_treeview_listeners.HasNext(iterator);)
		m_treeview_listeners.GetNext(iterator)->OnTreeViewOpenStateChanged(this, item->GetID(), open);
}

/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/
void OpTreeView::OnClick(OpWidget *widget, UINT32 id)
{
	for (UINT32 i = 0; i < m_columns.GetCount(); i++)
	{
		Column* column = m_columns.Get(i);

		if (column == widget)
		{
			if( m_user_sortable )
			{
				if (column->m_column_index != m_sort_by_column)
				{
					Sort(column->m_column_index, TRUE);
				}
				else if (m_sort_ascending)
				{
					Sort(column->m_column_index, FALSE);
				}
				else
				{
					Sort(-1, TRUE);
				}

				if (listener)
				{
					listener->OnSortingChanged(this);
				}
			}
			break;
		}
	}
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/
void OpTreeView::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus && reason == FOCUS_REASON_KEYBOARD && m_selected_item == -1)
	{
		SetSelectedItem(GetItemByLine(0));
	}
}

/***********************************************************************************
**
**	StartScrollTimer
**
***********************************************************************************/
void OpTreeView::StartScrollTimer()
{
	if (!IsTimerRunning())
	{
		StartTimer(100);
		m_timed_hover_pos = -1;
		m_timer_counter = 0;
	}
}

/***********************************************************************************
**
**	StopScrollTimer
**
***********************************************************************************/
void OpTreeView::StopScrollTimer()
{
	StopTimer();
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/
void OpTreeView::OnDragStart(const OpPoint& point)
{
//	INT32 pos = GetItemByPoint(point);
	INT32 pos = GetSelectedItemPos();

	if (pos != -1 && listener && m_is_drag_enabled)
	{
		listener->OnDragStart(this, pos, point.x, point.y);
	}
}

/***********************************************************************************
**
**	OnDragLeave
**
***********************************************************************************/
void OpTreeView::OnDragLeave(OpDragObject* drag_object)
{
	m_drop_pos = -1;
	m_insert_type = DesktopDragObject::INSERT_INTO;

	InvalidateAll();
}


/***********************************************************************************
**
**	OnDragCancel
**
***********************************************************************************/
void OpTreeView::OnDragCancel(OpDragObject* drag_object)
{
	m_drop_pos = -1;
	m_insert_type = DesktopDragObject::INSERT_INTO;

	InvalidateAll();
}


/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/
void OpTreeView::OnDragMove(OpDragObject* op_drag_object, const OpPoint& point)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	StartScrollTimer();

	INT32 pos = GetItemByPoint(point);
//	BOOL insert = FALSE;

	if (listener)
	{
		drag_object->SetSuggestedInsertType(DesktopDragObject::INSERT_INTO);

		OpRect rect;

		GetItemRect(pos, rect, FALSE);

		if (point.y < rect.y + (rect.height / 4))
		{
			drag_object->SetSuggestedInsertType(DesktopDragObject::INSERT_BEFORE);
		}
		else if (point.y >= rect.y + (rect.height * 3 / 4))
		{
			drag_object->SetSuggestedInsertType(DesktopDragObject::INSERT_AFTER);
		}

		listener->OnDragMove(this, op_drag_object, pos, point.x, point.y);
	}

	if (!drag_object->IsDropAvailable())
	{
		pos = -1;
	}
	else if (pos == -1)
	{
		pos = GetItemByLine(GetLineCount() - 1);
	}

	if (pos != m_drop_pos || (pos != -1 && drag_object->GetSuggestedInsertType() != m_insert_type))
	{
		InvalidateAll();
	}

	m_drop_pos = pos;
	m_insert_type = drag_object->GetSuggestedInsertType();
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/
void OpTreeView::OnDragDrop(OpDragObject* drag_object, const OpPoint& point)
{
	if (listener)
	{
		listener->OnDragDrop(this, drag_object, m_drop_pos, point.x, point.y);
	}

	OnDragLeave(drag_object);
}

/***********************************************************************************
**
**	OnTimer
**
***********************************************************************************/
void OpTreeView::OnTimer()
{
	UpdateDelayedItems();

	if (m_send_onchange_later)
	{
		// In m_select_on_hover mode we call OnChange with changed_by_mouse set to TRUE.
		// Ideally the mode parameter should have been, KEYBOARD, MOUSE or OTHER
		// as we need to know when the treeview has been cleared in the address
		// dropdown. Fixes bug #261810 [espen 2007-04-27]

		m_send_onchange_later = FALSE;
		if (listener)
			listener->OnChange(this, m_select_on_hover ? TRUE : FALSE);
		g_input_manager->UpdateAllInputStates();
	}

	if (!g_drag_manager->IsDragging() && !m_is_drag_selecting)
	{
		StopTimer();
		return;
	}

	const OpRect rect = GetVisibleRectLTR();
	OpPoint point = GetMousePos();

	INT32 step = 0;

	if (g_drag_manager->IsDragging())
	{
		if (rect.Contains(point))
		{
			if (point.y < rect.y + (rect.height / 10))
			{
				step = -m_line_height;
			}
			else if (point.y > rect.y + (rect.height * 9 / 10))
			{
				step = m_line_height;
			}
		}
	}
	else
	{
		if (point.y < rect.y)
		{
			step = -m_line_height;
		}
		else if (point.y > rect.y + rect.height)
		{
			step = m_line_height;
		}
	}

	if (step)
	{
		m_vertical_scrollbar->SetValue(m_vertical_scrollbar->GetValue() + step);
		Sync();
	}

	INT32 pos = GetItemByPoint(point);

	if (pos != -1 && m_timed_hover_pos == pos)
	{
		m_timer_counter++;

		if (m_timer_counter == 10 && m_insert_type == DesktopDragObject::INSERT_INTO)
		{
			OpenItem(pos, TRUE, FALSE);

			// Test code. Allow autoexpand even in flat mode
			// [espen 2003-06-20]
			if (HasMatchText() || HasMatchParentID())
			{
				if (listener )
				{
					SetSelectedItem(pos, TRUE, FALSE, FALSE);
					listener->OnMouseEvent(this, pos, 0, 0, MOUSE_BUTTON_1, TRUE, 2);
				}
			}
		}
	}
	else
	{
		m_timer_counter = 0;
	}

	m_timed_hover_pos = pos;

	UpdateDragSelecting();
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/
void OpTreeView::OnShow(BOOL show)
{
	if (show && m_is_centering_needed)
	{
		m_is_centering_needed = FALSE;

		if (m_selected_item != -1)
		{
			ScrollToItem(m_selected_item, TRUE);
		}
	}
}

/***********************************************************************************
**
**	OnMouseDown
**
***********************************************************************************/
void OpTreeView::OnMouseDown(const OpPoint &point,
							 MouseButton button,
							 UINT8 nclicks)
{
	if(m_edit && listener)
	{
		OpString text;
		m_edit->GetText(text);
		if (!listener->OnItemEditVerification(this, m_edit_pos, m_edit_column, text))
			return;
	}

	SetFocus(FOCUS_REASON_MOUSE);

	if (!m_is_drag_enabled)
	{
		m_is_drag_selecting = TRUE;
	}

	BOOL control = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_CTRL;
	BOOL shift = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;

	INT32 pos = GetItemByPoint(point);

	if (nclicks > 1 && pos != m_selected_item)
	{
		nclicks = 1;
	}

	m_selected_on_mousedown = TRUE;

	if (pos != -1)
	{
		if (button == MOUSE_BUTTON_1)
		{
			if (m_checkable_column != -1)
			{
				// toggle only if clicking directly on checkbox, or if item was already selected

				OpRect cell_rect;
				GetCellRectLTR(pos, m_checkable_column, cell_rect);
				OpRect box_rect(cell_rect);
				box_rect.width = cell_rect.height;

				if (pos == m_selected_item || AdjustForDirection(box_rect).Contains(point))
					ToggleItem(pos);
			}

			if (!control && !shift && nclicks >= 1)
			{
				// expand/collapse tree if we clicked on expand icon and do nothing else
				if (IsThreadImage(point, pos))
				{
					if (CanItemOpen(pos))
					{
						OpenItem(pos, TRUE);
						return;
					}
					else
					{
						OpenItem(pos, FALSE);
						return;
					}
				}

				if (m_expand_on_single_click || nclicks == 2)
				{
					// expand if possible when m_expand_on_single_click is enabled
					if (CanItemOpen(pos))
					{
						OpenItem(pos, TRUE);
					}
					// otherwise collapse item if already selected and m_expand_on_single_click is enabled
					else if (m_selected_item == pos &&
						m_view_model.GetItemByIndex(pos)->IsSelected())
					{
						OpenItem(pos, FALSE);
					}
					// and pass on the event
				}
			}
		}

		m_selected_on_mousedown = !m_view_model.GetItemByIndex(pos)->IsSelected();
	}

	if (HandleGroupHeaderMouseClick(point, button))
	{
		SetTopGroupHeader();
	}
	else
	{
		if (m_selected_on_mousedown)
		{
			SelectItem(pos, FALSE, control, shift, TRUE);
			Sync();
		}
		else if (pos != m_selected_item)
		{
			m_selected_item = pos;
			if (pos != -1)
				SetSelected(pos, TRUE);
			InvalidateAll();

			if (listener)
				listener->OnChange(this, TRUE);

			g_input_manager->UpdateAllInputStates();
		}

		if (button == MOUSE_BUTTON_1 && nclicks == 2 && GetAction() && pos != -1)
		{
			// Everytime something is about to open stop the dragging
			m_is_drag_selecting = FALSE;

			if (g_input_manager->InvokeAction(GetAction(), this))
				return;
		}
	}

	// Only propagate event if it wasn't on the thread image, since that is
	// handled by the treeview
	if (listener && !IsThreadImage(point, pos))
	{
		listener->OnMouseEvent(this, pos, point.x, point.y, button, TRUE, nclicks);
	}
}

/***********************************************************************************
**
**	OnMouseUp
**
***********************************************************************************/
void OpTreeView::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	m_is_drag_selecting = FALSE;

	BOOL control = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_CTRL;
	BOOL shift = vis_dev->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;

	INT32 pos = GetItemByPoint(point);

	// lose event if it didn't happen on same item as mouse down to avoid confusion
	// or if the event was on the thread image (handled only by treeview)
	if (pos != m_selected_item || IsThreadImage(point, pos))
		return;

	if (pos != -1)
	{
		if (button == MOUSE_BUTTON_1)
		{
			if (!m_selected_on_mousedown)
			{
				SelectItem(pos, FALSE, control, shift, TRUE);
			}
		}
	}

	if (listener)
	{
		listener->OnMouseEvent(this, pos, point.x, point.y, button, FALSE, 1);
	}
}

/***********************************************************************************
**
**	OnMouseLeave
**
***********************************************************************************/
void OpTreeView::OnMouseLeave()
{
	m_is_drag_selecting = FALSE;

	if (m_hover_item != -1)
	{
		OpString empty;
		GetParentDesktopWindow()->SetStatusText(empty);
		UpdateItem(m_hover_item, FALSE);
		m_hover_associated_image_item = -1;
		m_hover_item = -1;
	}
	
	if (m_item_count == 0)
	{
		// for now the hover skin state is only needed when the treeview is empty
		GetBorderSkin()->SetState(0);
	}
}

/***********************************************************************************
**
**	OnMouseMove
**
***********************************************************************************/
void OpTreeView::OnMouseMove(const OpPoint &point)
{
	// Reduce mouse sensitvity when move happens in-between keyboard 
	// inputs. A slight vibration in the table causing the mouse to 
	// move a pixel will override keyboard navigation
	// anchor(ANCHOR_RESET,ANCHOR_RESET) - Set after mouse move has been accpected

	if (m_anchor.x != ANCHOR_RESET && m_anchor.y != ANCHOR_RESET )
	{
		if( op_abs(m_anchor.x - point.x)+op_abs(m_anchor.y - point.y) < ANCHOR_MOVE_OFFSET )
		{
			return; // Mouse has not moved enough
		}
		m_anchor = OpPoint(ANCHOR_RESET,ANCHOR_RESET);
	}

	INT32 pos = GetItemByPoint(point);

	if (m_hover_item != pos)
	{
		if (m_hover_item != -1)
		{
			if (m_link_column != -1 || m_hover_associated_image_item == m_hover_item)
			{
				m_hover_associated_image_item = -1;
				UpdateItem(m_hover_item, FALSE);
			}
		}

		m_hover_item = pos;

		if (m_hover_item != -1)
		{
			if (m_link_column != -1)
			{
				UpdateItem(m_hover_item, FALSE);
			}

			OpInfoText text;

			GetInfoText(pos, text);

			if (text.HasStatusText())
			{
				GetParentDesktopWindow()->SetStatusText(text.GetStatusText());
			}
		}
		else
		{
			OpString empty;
			GetParentDesktopWindow()->SetStatusText(empty);
		}
	}

	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);

	if(item)
	{
		ItemDrawArea* associated_image = item->GetAssociatedImage(this);

		if(associated_image && associated_image->m_image != NULL)
		{
			OpRect cell_rect;
			GetCellRectLTR(pos, m_columns.GetCount() - 1, cell_rect);

			INT32 left, top, right, bottom;
			GetPadding(&left, &top, &right, &bottom);

			OpWidgetImage associated_img;
			associated_img.SetImage(associated_image->m_image);
			if (associated_image->m_image_state != -1)
			{
				associated_img.SetState(associated_image->m_image_state);
			}

			OpRect image_rect = associated_img.CalculateScaledRect(cell_rect, FALSE, TRUE);
			image_rect.x = cell_rect.Right() - image_rect.width - right;

			if (AdjustForDirection(image_rect).Contains(point))
			{
				if(m_hover_associated_image_item != m_hover_item)
				{
					UpdateItem(m_hover_item, FALSE);
					m_hover_associated_image_item = m_hover_item;
				}
			}
			else
			{
				if(m_hover_associated_image_item == m_hover_item)
				{
					UpdateItem(m_hover_item, FALSE);
					m_hover_associated_image_item = -1;
				}
			}
		}
	}

	UpdateDragSelecting();
	
	if (m_item_count == 0)
	{
		// the hover skin state is much more complex if there are items, but we only need when it's empty
		GetBorderSkin()->SetState(SKINSTATE_HOVER, -1, 100);
	}
}

/***********************************************************************************
**
**	OnMouseWheel
**
***********************************************************************************/
BOOL OpTreeView::OnMouseWheel(INT32 delta,BOOL vertical)
{
	if (vertical)
		return m_vertical_scrollbar->OnMouseWheel(delta,vertical);
	else
		return m_horizontal_scrollbar->OnMouseWheel(delta,vertical);
}

/***********************************************************************************
**
**	OnSetCursor
**
***********************************************************************************/
void OpTreeView::OnSetCursor(const OpPoint &point)
{
	INT32 pos = GetItemByPoint(point);

	if (pos != -1 && m_link_column != -1)
	{
		SetCursor(CURSOR_CUR_POINTER);
		return;
	}

	OpWidget::OnSetCursor(point);
}

/***********************************************************************************
**
**	GetPreferedSize
**
***********************************************************************************/
void OpTreeView::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*h = GetLineCount() * m_line_height + 4; // +4 needed for OpAccordion. See commit 5d5bf5ef782a.
	*w = 0;

	// If showing the horizontal scrollbar we need to calculate the full width
	if (m_show_horizontal_scrollbar)
	{
		OpRect rect = GetVisibleRectLTR(false, true);
		*w = max(rect.width, GetFixedWidth());
	}
}

/***********************************************************************************
**
**	OnScaleChanged
**
***********************************************************************************/
void OpTreeView::OnScaleChanged()
{
	m_save_strings_that_need_refresh = ! m_save_strings_that_need_refresh;

	if (m_save_strings_that_need_refresh || !GetTreeModel())
		return;

	INT32 count = GetTreeModel()->GetColumnCount();
	
	for (UINT32 item_no = 0; item_no < m_item_ids_that_need_string_refresh.GetCount(); item_no++)
	{
		for (INT32 column = 0; column < count; column++)
		{
			TreeViewModelItem* item = m_view_model.GetItemByID(m_item_ids_that_need_string_refresh.GetByIndex(item_no));
			if (!item)
				continue;

			ItemDrawArea* cache = item->GetItemDrawArea(this, column, FALSE);
			if (!cache)
				continue;

			for (ItemDrawArea::StringList* string = &cache->m_strings; string; string = string->m_next)
			{
				string->m_string.NeedUpdate();
			}

			cache = item->GetAssociatedText(this);
			if (!cache)
				continue;

			cache->m_strings.m_string.NeedUpdate();
		}
	}
	m_item_ids_that_need_string_refresh.Clear();
}

/***********************************************************************************
**
**	OnImageChanged
**
***********************************************************************************/
void OpTreeView::OnImageChanged(OpWidgetImage* widget_image)
{
	if (widget_image == GetBorderSkin())
	{
		Invalidate(rect);
	}
}

/***********************************************************************************
**
**	OnScrollAction
**
***********************************************************************************/
BOOL OpTreeView::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	if(vertical)
		return m_vertical_scrollbar->OnScroll(delta, vertical, smooth);
	else
		return m_horizontal_scrollbar->OnScroll(delta, vertical, smooth);
}

/***********************************************************************************
**
**	GetInfoText
**
***********************************************************************************/
void OpTreeView::UpdateDragSelecting()
{
	if (m_select_on_hover || m_is_drag_selecting)
	{
		INT32 pos = GetItemByPoint(GetMousePos(), TRUE);

		SelectItem(pos, FALSE, FALSE, FALSE, TRUE);
		Sync();

		StartScrollTimer();
	}
}

/***********************************************************************************
**
**	GetInfoText
**
***********************************************************************************/
void OpTreeView::GetInfoText(INT32 pos, OpInfoText& text)
{
	OpTreeModelItem* item = GetItemByPosition(pos);

	if (item)
	{
		OpTreeModelItem::ItemData item_data(INFO_QUERY);
		item_data.info_query_data.info_text = &text;
		item->GetItemData(&item_data);
	}
}

/***********************************************************************************
**
**  GetToolTipText
**
***********************************************************************************/
void OpTreeView::GetToolTipText(OpToolTip* tooltip, OpInfoText& text) 
{
	// needed for the debugging tooltip first
	if (IsDebugToolTipActive())
		QuickOpWidgetBase::GetToolTipText(tooltip, text);
	else
		GetInfoText(tooltip->GetItem(), text);
}

/***********************************************************************************
**
**  GetToolTipThumbnailText
**
***********************************************************************************/
void OpTreeView::GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url)
{
	INT32 pos = tooltip->GetItem();
	OpTreeModelItem* item = GetItemByPosition(pos);

	if (item)
	{
		OpThumbnailText text;
		OpTreeModelItem::ItemData item_data(URL_QUERY);
		item_data.url_query_data.thumbnail_text = &text;
		item->GetItemData(&item_data);
		if(!text.thumbnail.IsEmpty())
		{
			tooltip->SetImage(item_data.url_query_data.thumbnail_text->thumbnail, item_data.url_query_data.thumbnail_text->fixed_image);
		}
		title.Set(text.title.CStr());
		url_string.Set(text.url.CStr());
	}
}

/***********************************************************************************
**
**  GetToolTipType
**
***********************************************************************************/
OpToolTipListener::TOOLTIP_TYPE OpTreeView::GetToolTipType(OpToolTip* tooltip)
{
	OpToolTipListener::TOOLTIP_TYPE type = TOOLTIP_TYPE_NORMAL;

	// let's check if we can supply a thumbnail
	INT32 pos = tooltip->GetItem();
	OpTreeModelItem* item = GetItemByPosition(pos);

	if (item)
	{
		OpThumbnailText text;
		OpTreeModelItem::ItemData item_data(URL_QUERY);
		item_data.url_query_data.thumbnail_text = &text;
		item->GetItemData(&item_data);
		if(!text.thumbnail.IsEmpty())
		{
			// we do have a thumbnail, set the type accordingly
			type = TOOLTIP_TYPE_THUMBNAIL;
		}
	}
	return type;
}

/***********************************************************************************
**
**	GetToolTipItem
**
***********************************************************************************/
INT32 OpTreeView::GetToolTipItem(OpToolTip* tooltip)
{
	OpPoint point = GetMousePos();

	return GetItemByPoint(point);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL OpTreeView::IsScrollable(BOOL vertical)
{
	if( vertical )
		return m_vertical_scrollbar->CanScroll();
	else
		return m_horizontal_scrollbar->CanScroll();
}

/***********************************************************************************
**
**	OnScroll
**
***********************************************************************************/
void OpTreeView::OnScroll(OpWidget* source_widget,
						  INT32 old_val,
						  INT32 new_val,
						  BOOL caused_by_input)
{
	if (!IsAllVisible())
		return;

	INT32 dx = 0, dy = 0;

	if (static_cast<OpScrollbar*>(source_widget)->IsHorizontal())
		dx = new_val - old_val;
	else
		dy = new_val - old_val;

	OpRect rect = GetVisibleRect(false, false);
	// Translate to view coordinates.
	rect.OffsetBy(GetRect(TRUE).TopLeft());

	if (GetVisualDevice())
		GetVisualDevice()->GetView()->ScrollRect(rect, -dx, -dy);

	if (SetTopGroupHeader())
	{
		rect = GetVisibleRect(false, false, true);
		rect.height = m_line_height;
		Invalidate(rect);
	}

	if (m_edit)
		CancelEdit(TRUE);

	// Move custom widget children
	for (UINT32 i = 0; i < m_custom_widgets.GetCount(); i++)
	{
		OpWidget *child = m_custom_widgets.Get(i);
		OpRect r = child->GetRect();
		r.OffsetBy(-dx, -dy);
		child->SetRect(r);
	}
}

/***********************************************************************************
**
**	OnItemAdded
**
***********************************************************************************/
void OpTreeView::OnItemAdded(OpTreeModel* tree_model, INT32 pos)
{
	m_changed = true;

	m_item_count++;

	if (m_selected_item >= pos)
	{
		m_selected_item++;
	}

	InitItem(pos);

	TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);
	m_line_map.AddItem(pos, item->GetNumLines());

	UpdateSearch(pos, FALSE, FALSE);

	if (!IsItemVisible(pos))
	{
		m_line_map.RemoveLines(pos);
	}

	if (item->IsSelected())
		m_selected_item_count++;

	// next level of optimization occurs when item is visible, but not inside
	// the viewable area.

	const OpRect rect = GetVisibleRectLTR();

	INT32 line_at_top		= m_vertical_scrollbar->GetValue() / m_line_height;
	INT32 line_at_bottom	= (m_vertical_scrollbar->GetValue() + rect.height) / m_line_height + 1;

	INT32 item_at_top		= GetItemByLine(line_at_top);
	INT32 item_at_bottom	= GetItemByLine(line_at_bottom);

	if (item_at_top != -1 && item_at_bottom != -1 && (item_at_top > pos || item_at_bottom < pos))
	{
		if (item_at_top > pos)
		{
			// item appears before start of list, so we must bump scrollbar by hand
			// since the viewable area should be unaffected.

			m_vertical_scrollbar->SetValue(m_vertical_scrollbar->GetValue() + m_line_height, FALSE, FALSE);
		}

		if (UpdateScrollbars())
		{
			InvalidateAll();
			Relayout();
		}
	}
	else
	{
		//  not possible to optimize.. invalidate all.
		InvalidateAll();

		if (UpdateScrollbars())
		{
			Relayout();
		}

		// special case: if item was added just below what is visible at bottom, then try to scroll it into view,
		// but not if it causes selected item to scroll higher than center of view

		INT32 selected_line = GetLineByItem(m_selected_item);
		INT32 line = GetLineByItem(pos);
		INT32 line_at_center = line_at_top + (rect.height / m_line_height) / 2;

		if ((line == line_at_bottom || line == line_at_bottom - 1) && selected_line > line_at_center && selected_line < line_at_bottom)
		{
			ScrollToLine(line);
		}
	}
}


/***********************************************************************************
**
**	UpdateSearch
**
***********************************************************************************/
BOOL OpTreeView::UpdateSearch(INT32 pos,
							  BOOL check,
							  BOOL update_line)
{
	if(m_async_match && m_match_text.HasContent())
	{
		// Since the tree has now changed we need to redo the search in case
		// this item also needs to be matched, and since for async search
		// MatchItem is not sufficient, the whole search will have to be done
		// again
		SetMatch(m_match_text.CStr(),
				 m_match_column,
				 m_match_type,
				 m_match_parent_id,
				 TRUE,  // select
				 TRUE); // force

		return FALSE;
	}
	else
	{
		return MatchItem(pos, check, update_line);
	}
}

/***********************************************************************************
**
**	OnSubtreeRemoving
**
***********************************************************************************/
void OpTreeView::OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size)
{
	m_changed = true;
	m_hover_item = -1;
	m_reselect_selected_line = m_reselect_when_selected_is_removed ? GetLineByItem(m_selected_item) : -1;
	TreeViewModelItem* item = m_view_model.GetItemByIndex(parent_index);
	if(item && item->IsHeader() && item->GetSubtreeSize() == 1)
		m_reselect_selected_line--;

	// Add the children to m_lines

	for(INT32 child = GetChild(index); child >= 0; child = GetSibling(child))
	{
		InsertLines(child);
	}

	UpdateDelayedItems();
}

/***********************************************************************************
 **
 **	OnSubtreeRemoved
 **
 ** @param index - index of subtree that was removed
 **
 ***********************************************************************************/
void OpTreeView::OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size)
{
	INT32 old_line_count = GetLineCount();
	INT32 removed_count = subtree_size + 1;

	m_item_count -= removed_count;

	m_line_map.RemoveItems(index, index + subtree_size);

	// if removing this causes parent to have no children, then close parent, unless m_root_opens_all == TRUE
	if (!m_root_opens_all)
	{
		TreeViewModelItem* parent_item = m_view_model.GetItemByIndex(parent_index);

		if (parent_item && parent_item->GetSubtreeSize() == 0)
		{
			parent_item->m_flags &= ~FLAG_OPEN;
		}
	}

	BOOL update_selected = FALSE;

	INT32 count = subtree_size + 1;

	if (m_selected_item >= index + count)
	{
		m_selected_item -= count;
	}
	else if (m_selected_item >= index)
	{
		update_selected = TRUE;
	}

	Rematch(parent_index);

	if (update_selected)
	{
		if (m_reselect_selected_line >= GetLineCount())
		{
			m_reselect_selected_line = GetLineCount() - 1;
		}
		
		TreeViewModelItem* item = m_view_model.GetItemByIndex(GetItemByLine(m_reselect_selected_line));
		if (item && item->IsHeader())
		{
			if (!item->IsOpen())
			{
				OpenItem(item->GetIndex(), TRUE);
			}
			m_reselect_selected_line++;
		}

		SelectItem(GetItemByLine(m_reselect_selected_line), TRUE);
	}

	if (old_line_count != GetLineCount())
	{
		TreeChanged();
	}

	OpPoint point = GetMousePos();
	INT32 pos = GetItemByPoint(point);
	if (pos == index)
	{
		OnMouseMove(point);
	}
	else if (m_item_count == 0 && GetParentDesktopWindow())
	{
		OpString empty;
		GetParentDesktopWindow()->SetStatusText(empty);
		UpdateItem(m_hover_item, FALSE); 
		m_hover_associated_image_item = -1;
		m_hover_item = -1;

	}
}

/***********************************************************************************
**
**	OnItemChanged
**
***********************************************************************************/
void OpTreeView::OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort)
{
	m_changed = true;

	if (pos == -1)
	{
		Redraw();
	}
	else
	{
		TreeViewModelItem* item = m_view_model.GetItemByIndex(pos);
		if (!item)
			return;
		item->Clear();
		bool is_selected = m_view_model.GetItemByModelItem(GetSelectedItem()) == item;
		BOOL match_changed = UpdateSearch(pos, TRUE);
		
		m_view_model.RegroupItem(pos);

		if (match_changed)
		{
			Rematch(pos);
		}

		if (sort && (m_sort_by_column != -1 || m_always_use_sortlistener) && !m_edit)
		{
			UpdateSortParameters(GetTreeModel(), m_sort_by_column, m_sort_ascending);
			m_view_model.ResortItem(pos);
		}
		else if (match_changed)
		{
			UpdateLines(pos);
			TreeChanged();
		}
		else
		{
			UpdateItem(pos, FALSE, TRUE);
		}

		if (is_selected)
		{
			SetSelected(item->GetIndex(), TRUE);
		}
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void OpTreeView::OnItemRemoving(OpTreeModel* tree_model, INT32 index)
{
	SetSelected(index,FALSE);
	m_item_ids_that_need_string_refresh.Remove(m_view_model.GetItemByIndex(index)->GetID());
}

/***********************************************************************************
**
**	OnTreeChanging
**
***********************************************************************************/
void OpTreeView::OnTreeChanging(OpTreeModel* tree_model)
{
	OpINT32Vector items;
	GetSelectedItems(items, FALSE);
	INT32 item = items.GetCount() ? items.Get(items.GetCount() - 1) : m_selected_item;
	m_reselect_selected_line = m_reselect_when_selected_is_removed ? GetLineByItem(item) : -1;
	m_reselect_selected_id = item == -1 ? 0 : GetItemByPosition(item)->GetID();
	m_items_that_need_redraw.Clear();
}

/***********************************************************************************
**
**	OnTreeChanged
**
***********************************************************************************/
void OpTreeView::OnTreeChanged(OpTreeModel* tree_model)
{
	INT32 old_line_count = GetLineCount();

	m_changed = true;
	m_item_count = m_view_model.GetCount();
	m_selected_item = -1;
	m_shift_item = -1;
	m_hover_item = -1;
	m_selected_item_count = 0;
	OpINT32Vector selected_items;

	for (INT32 i = 0; i < m_item_count; i++)
	{
		InitItem(i);
		MatchItem(i, FALSE, FALSE);
		TreeViewModelItem* item = m_view_model.GetItemByIndex(i);
		if (item->IsSelected())
		{
			RETURN_VOID_IF_ERROR(selected_items.Add(i));
			m_selected_item_count++;
		}
	}

	UpdateLines();
	UpdateScrollbars();
	Relayout();

	INT32 item_to_select = -1;

	if (m_reselect_selected_id)
	{
		item_to_select = GetItemByID(m_reselect_selected_id);
	}

	if (item_to_select == -1)
	{
		if (old_line_count > GetLineCount())
		{
			m_reselect_selected_line -= (old_line_count - GetLineCount() - 1);
		}

		if (m_reselect_selected_line >= GetLineCount())
		{
			m_reselect_selected_line = GetLineCount() - 1;
		}

		if (m_reselect_selected_line >= 0)
		{
			item_to_select = GetItemByLine(m_reselect_selected_line);
		}
	}
	
	if (item_to_select != -1)
		SelectItem(item_to_select, TRUE);

	for (UINT32 i = 0; i < selected_items.GetCount(); i++)
	{
		SetSelected(selected_items.Get(i), TRUE);
	}
}

/***********************************************************************************
**
**	OnCustomWidgetItemRemoving
**
***********************************************************************************/

void OpTreeView::OnCustomWidgetItemRemoving(TreeViewModelItem *item, OpWidget* widget)
{
	// Remove custom widgets from the m_custom_widgets vector.
	if (widget == NULL)
	{
		OP_ASSERT(item->IsCustomWidget());

		OpTreeModelItem::ItemData item_data(WIDGET_QUERY);
		item->GetItemData(&item_data);
		widget = item_data.widget_query_data.widget;
	}
	OP_STATUS status = m_custom_widgets.RemoveByItem(widget);

	OP_ASSERT(OpStatus::IsSuccess(status));
}

/***********************************************************************************
**
**	ReadColumnSettings
**
***********************************************************************************/
void OpTreeView::ReadColumnSettings()
{
	if (m_show_column_headers && HasName() && m_columns.GetCount() > 1)
	{
		OpString name;
		name.Set(GetName());

		OpLineParser line(g_pcui->GetColumnSettings(name));

		INT32 column_position = 0;

		while (line.HasContent())
		{
			m_column_settings_loaded = TRUE;

			INT32 column_index = -1;
			INT32 column_weight = 100 << 8;
			int column_visible = 1;

			line.GetNextValue(column_index);
			line.GetNextValue(column_weight);
			line.GetNextValue(column_visible);

			for (UINT32 i = 0; i < m_columns.GetCount(); i++)
			{
				Column* column = m_columns.Get(i);

				if (column->m_column_index == column_index)
				{
					column->m_weight = column_weight;
					column->SetColumnUserVisibility(column_visible);

					m_columns.Remove(i);
					m_columns.Insert(column_position, column);
					column_position++;
					break;
				}
			}
		}
	}
}

/***********************************************************************************
**
**	WriteColumnSettings
**
***********************************************************************************/
void OpTreeView::WriteColumnSettings()
{
	if (m_show_column_headers && HasName() && m_columns.GetCount() > 1)
	{
		OpString name;
		name.Set(GetName());

		OpString line;

		for (UINT32 i = 0; i < m_columns.GetCount(); i++)
		{
			uni_char buffer[32];
			Column* column = m_columns.Get(i);

			if (line.HasContent())
			{
				line.Append(UNI_L(", "));
			}

			uni_itoa(column->m_column_index, buffer, 10);
			line.Append(buffer);

			line.Append(UNI_L(", "));
			uni_itoa(column->m_weight, buffer, 10);
			line.Append(buffer);

			line.Append(UNI_L(", "));
			uni_itoa(column->IsColumnUserVisible(), buffer, 10);
			line.Append(buffer);
		}

		g_pcui->WriteColumnSettingsL(name, line.CStr());
	}
}



/***********************************************************************************
**
**	ReadSavedMatches
**
***********************************************************************************/
void OpTreeView::ReadSavedMatches()
{
	m_saved_matches.DeleteAll();

	if (HasName())
	{
		OpString name;

		name.Set(GetName());

		OpLineParser line(g_pcui->GetMatchSettings(name));

		while (line.HasContent())
		{
			OpString* match = OP_NEW(OpString, ());
			if (!match)
				return;

			line.GetNextString(*match);

			m_saved_matches.Add(match);
		}
	}
}



/***********************************************************************************
**
**	WriteSavedMatches
**
***********************************************************************************/
void OpTreeView::WriteSavedMatches()
{
	if (HasName())
	{
		OpString name;
		name.Set(GetName());

		OpLineString line;

		for (UINT32 i = 0; i < m_saved_matches.GetCount(); i++)
		{
			OpString* match = m_saved_matches.Get(i);

			line.WriteString(match->CStr());
		}

		g_pcui->WriteMatchSettingsL(name, line.GetString());

	}
}

/***********************************************************************************
**
**	CalculateLineHeight
**
***********************************************************************************/
void OpTreeView::CalculateLineHeight()
{
	INT32 image_width;
	INT32 image_height;

	// Get the image size
	GetSkinManager()->GetSize("Folder", &image_width, &image_height);

	// Get the height of the font
	int font_height = WidgetUtils::GetFontHeight(font_info);

	// Get the padding; generic line padding
	int padding = GetSkinManager()->GetLinePadding();

	// Adjust the line height to maximum of the image and font, with one pixel padding
	m_line_height = max(image_height, font_height) + padding + m_extra_line_height;
	OP_ASSERT(m_line_height > 0);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
/***********************************************************************************
**
**
**
***********************************************************************************/
void OpTreeView::CreateColumnListAccessorIfNeeded()
{
		if (!m_column_list_accessor)
			m_column_list_accessor = OP_NEW(ColumnListAccessor, (this));
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int	OpTreeView::GetAccessibleChildrenCount()
{
	return OpWidget::GetAccessibleChildrenCount()+1;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* OpTreeView::GetAccessibleChild(int i)
{
	if (i-- == 0)
		return &m_view_model;

	return OpWidget::GetAccessibleChild(i);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int OpTreeView::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	if (child == &m_view_model)
		return 0;
	int index = OpWidget::GetAccessibleChildIndex(child);
	if (index != Accessibility::NoSuchChild)
		return index +1;
	return Accessibility::NoSuchChild;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* OpTreeView::GetAccessibleChildOrSelfAt(int x, int y)
{
	if (m_view_model.GetAccessibleChildOrSelfAt(x, y))
		return &m_view_model;
	
	return OpWidget::GetAccessibleChildOrSelfAt(x, y);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpAccessibleItem* OpTreeView::GetAccessibleFocusedChildOrSelf()
{
	if (m_view_model.GetAccessibleFocusedChildOrSelf())
		return &m_view_model;
	
	return OpWidget::GetAccessibleFocusedChildOrSelf();
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int OpTreeView::GetItemAt(int x, int y)
{
	OpPoint point(0,0);
	OpRect rect;
	AccessibilityGetAbsolutePosition(rect);
	point.x = x - rect.x;
	point.y = y - rect.y;

	return GetItemByPoint(point);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
int OpTreeView::GetColumnAt(int x, int y)
{
	int n=m_columns.GetCount();
	for (int i=0; i<n; i++)
	{
		OpRect rect;
		m_columns.Get(i)->AccessibilityGetAbsolutePosition(rect);
		if (rect.Contains(OpPoint(x,rect.y)))
			return i;
	}

	return 0;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Accessibility::State OpTreeView::GetUnfilteredState()
{
	return OpWidget::AccessibilityGetState();
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Accessibility::State OpTreeView::AccessibilityGetState()
{
	return GetUnfilteredState() & ~Accessibility::kAccessibilityStateFocused;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void OpTreeView::OnKeyboardInputGained(OpInputContext* new_input_context,
									   OpInputContext* old_input_context,
									   FOCUS_REASON reason)
{
	if (m_item_count == 0)
	{
		// if there are items, they would gain the focus state
		GetBorderSkin()->SetState(SKINSTATE_FOCUSED, -1, 100);
	}
	OpWidget::OnKeyboardInputGained(new_input_context, old_input_context, reason);
	AccessibilityManager::GetInstance()->SendEvent(&m_view_model, Accessibility::Event(Accessibility::kAccessibilityStateFocused));
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void OpTreeView::OnKeyboardInputLost(OpInputContext* new_input_context,
									 OpInputContext* old_input_context,
									 FOCUS_REASON reason)
{
	if (m_item_count == 0)
	{
		// if there are items, they would lose the focus state at this point
		GetBorderSkin()->SetState(0);
	}
	OpWidget::OnKeyboardInputLost(new_input_context, old_input_context, reason);
	AccessibilityManager::GetInstance()->SendEvent(&m_view_model, Accessibility::Event(Accessibility::kAccessibilityStateFocused));
}

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

/***********************************************************************************
**
**	StartColumnSizing
**
***********************************************************************************/
BOOL OpTreeView::StartColumnSizing(Column* column, const OpPoint& point, BOOL check_only)
{
	m_is_sizing_column = FALSE;

	INT32 index = m_columns.Find(column);

	OpPoint use_point = point;
	if (GetRTL())
		use_point.x = column->GetWidth() - point.x - 1;

	if (use_point.x < 4)
	{
		if (index == 0)
			return FALSE;

		index--;
	}
	else if (use_point.x < column->GetWidth() - 4)
	{
		return FALSE;
	}

	const INT32 count = m_columns.GetCount();

	// there must at least be one sizeable column on the left and right side

	m_sizing_column_left = -1;
	m_sizing_column_right = -1;
	m_sizing_columns.reset(OP_NEWA(OpRect, count));
	RETURN_VALUE_IF_NULL(m_sizing_columns.get(), FALSE);

	for (INT32 i = 0; i < count; i++)
	{
		Column* c = m_columns.Get(i);

		m_sizing_columns[i] = c->GetRect();

		if (c->IsColumnReallyVisible() && c->m_fixed_width == 0)
		{
			if (i <= index)
				m_sizing_column_left = i;

			if (i > index && m_sizing_column_right == -1)
				m_sizing_column_right = i;
		}
	}

	if (m_sizing_column_left == -1 || m_sizing_column_right == -1)
		return FALSE;

	if (check_only)
		return TRUE;

	m_is_sizing_column = TRUE;
	m_start_x = point.x + column->GetRect().x;

	return TRUE;
}

/***********************************************************************************
**
**	DoColumnSizing
**
***********************************************************************************/
void OpTreeView::DoColumnSizing(Column* column, const OpPoint& point)
{
	INT32 delta = point.x + column->GetRect().x - m_start_x;
	if (delta == 0)
		return;
	if (GetRTL())
		delta = -delta;

	const INT32 count = m_columns.GetCount();

	// reset column pos

	OpAutoArray<OpRect> rects(OP_NEWA(OpRect, count));
	if (rects.get() == NULL)
		return;
	op_memcpy(rects.get(), m_sizing_columns.get(), sizeof(OpRect) * count);

	if (delta < 0)
	{
		INT32 delta_left_to_lose = -delta;

		// m_sizing_column_left loses the delta, unless at minimum, then the next one loses etc
		// m_sizing_column_right gains what the other loses

		for (INT32 i = m_sizing_column_left; i >= 0 && delta_left_to_lose > 0; i--)
		{
			Column* c = m_columns.Get(i);

			if (c->IsColumnReallyVisible() && c->m_fixed_width == 0)
			{
				const OpRect& original_rect = m_sizing_columns[i];

				INT32 delta_this_can_lose = delta_left_to_lose;

				if (delta_this_can_lose > original_rect.width - 20)
				{
					delta_this_can_lose = original_rect.width - 20;

					if (delta_this_can_lose <= 0)
						continue;
				}

				rects[i].width -= delta_this_can_lose;
				rects[m_sizing_column_right].width += delta_this_can_lose;

				if (GetRTL())
				{
					for (INT32 j = i; j < m_sizing_column_right; j++)
						rects[j].x += delta_this_can_lose;
				}
				else
				{
					for (INT32 j = i + 1; j <= m_sizing_column_right; j++)
						rects[j].x -= delta_this_can_lose;
				}

				delta_left_to_lose -= delta_this_can_lose;
			}
		}
	}
	else
	{
		INT32 delta_left_to_lose = delta;

		// m_sizing_column_right loses the delta, unless at minimum, then the next one loses etc
		// m_sizing_column_left gains what the other loses

		for (INT32 i = m_sizing_column_right; i < count && delta_left_to_lose > 0; i++)
		{
			Column* c = m_columns.Get(i);

			if (c->IsColumnReallyVisible() && c->m_fixed_width == 0)
			{
				const OpRect& original_rect = m_sizing_columns[i];

				INT32 delta_this_can_lose = delta_left_to_lose;

				if (delta_this_can_lose > original_rect.width - 20)
				{
					delta_this_can_lose = original_rect.width - 20;

					if (delta_this_can_lose <= 0)
						continue;
				}

				rects[i].width -= delta_this_can_lose;
				rects[m_sizing_column_left].width += delta_this_can_lose;

				if (GetRTL())
				{
					for (INT32 j = i - 1; j >= m_sizing_column_left; j--)
						rects[j].x -= delta_this_can_lose;
				}
				else
				{
					for (INT32 j = i; j > m_sizing_column_left; j--)
						rects[j].x += delta_this_can_lose;
				}

				delta_left_to_lose -= delta_this_can_lose;
			}
		}
	}

	Column* last_visible_column = NULL;

	for (INT32 i = 0; i < count; i++)
	{
		Column* c = m_columns.Get(i);

		if (c->IsColumnReallyVisible())
		{
			last_visible_column = c;
		}

		c->SetRect(rects[i]);
		c->m_width = rects[i].width;
	}

	if (last_visible_column && m_vertical_scrollbar->IsVisible())
	{
		last_visible_column->m_width -= GetInfo()->GetVerticalScrollbarWidth();
	}

	InvalidateAll();
}

/***********************************************************************************
**
**	EndColumnSizing
**
***********************************************************************************/
void OpTreeView::EndColumnSizing()
{
	// compute new weights from the current layout

	const OpRect rect = GetVisibleRectLTR(true, true);

	INT32 available_column_width = rect.width;

	INT32 i;
	INT32 count = m_columns.GetCount();

	for (i = 0; i < count; i++)
	{
		Column* column = m_columns.Get(i);

		if (column->IsColumnReallyVisible() && column->m_fixed_width)
		{
			available_column_width -= column->m_fixed_width;
		}
	}

	if (available_column_width > 0)
	{
		INT32 total_weight = (count * 100) << 8;

		for (i = 0; i < count; i++)
		{
			Column* column = m_columns.Get(i);

			if (column->IsColumnReallyVisible() && !column->m_fixed_width)
			{
				column->m_weight = column->GetWidth() * total_weight / available_column_width;
			}
		}
	}

	m_is_sizing_column = FALSE;
	m_sizing_columns.reset();
	WriteColumnSettings();
}

/***********************************************************************************
**
**	InsertLines
**
***********************************************************************************/
OP_STATUS OpTreeView::InsertLines(TreeViewModelItem * item)
{
	if(!item)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	INT32 index = item->GetIndex();

	int num_lines = item->GetNumLines();

	return InsertLines(index, num_lines);
}

/***********************************************************************************
**
**	InsertLines
**
***********************************************************************************/
OP_STATUS OpTreeView::InsertLines(int index, int num_lines /* = -1 */)
{
	if(num_lines < 0)
	{
		TreeViewModelItem * item = m_view_model.GetItemByIndex(index);

		return InsertLines(item);
	}

	return m_line_map.InsertLines(index, num_lines);
}

/***********************************************************************************
**
**	RemoveLines
**
***********************************************************************************/
OP_STATUS OpTreeView::RemoveLines(int index)
{
	m_line_map.RemoveLines(index);
	return OpStatus::OK;
}

#ifdef ANIMATED_SKIN_SUPPORT
void OpTreeView::OnAnimatedImageChanged(OpWidgetImage* widget_image)
{
	for(INT32 i=0; i<m_item_count; i++)
	{
		TreeViewModelItem* item = m_view_model.GetItemByIndex(i);

		if(item && item->m_widget_image == widget_image)
		{
			OpRect rect;
			if(GetItemRect(item->GetIndex(), rect))
			{
				Invalidate(rect);
			}
		}
	}
}
#endif // ANIMATED_SKIN_SUPPORT

OpScopeWidgetInfo* OpTreeView::CreateScopeWidgetInfo()
{
	return OP_NEW(ScopeWidgetInfo, (*this));
}

OP_STATUS OpTreeView::ScopeWidgetInfo::AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible)
{
	OpRect paint_rect(m_tree.GetVisibleRectLTR()); // Make the paint rect everything for now
	
	m_tree.PaintTree(NULL, paint_rect, &list, include_invisible);

	return OpStatus::OK;
}

OP_STATUS OpTreeView::AddQuickWidgetInfoToList(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, TreeViewModelItem* item, INT32 real_col, const OpRect cell_rect, INT32 row, INT32 col)
{
	OP_ASSERT(item);

	// If there is a list this is the automated UI collecting information about each of the cells
	OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickWidgetInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickWidgetInfo, ()));
	if (info.get() == NULL)
		return OpStatus::ERR;

	OpString name;
	name.AppendFormat(UNI_L("%d"), item->GetID());
	RETURN_IF_ERROR(info.get()->SetName(name));
	
	info.get()->SetType(OpScopeDesktopWindowManager_SI::QuickWidgetInfo::QUICKWIDGETTYPE_TREEITEM);
	ItemDrawArea* associated_text = NULL;
	ItemDrawArea* cache = NULL;
	if(item->GetType() == OpTypedObject::WIDGET_TYPE_PAGE_BASED_AUTOCOMPLETE_ITEM ||
	   item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM)
	{
		HistoryAutocompleteModelItem* history_item = static_cast<HistoryAutocompleteModelItem*>(item->GetModelItem());
		OP_ASSERT(history_item);

		OpString address, display_address, additional_text;
		RETURN_IF_ERROR(history_item->GetAddress(address));
		RETURN_IF_ERROR(history_item->GetDisplayAddress(display_address));
		RETURN_IF_ERROR(info.get()->SetVisible_text(display_address));
		RETURN_IF_ERROR(info.get()->SetText(address));
		if (item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM)
		{
			OpStatus::Ignore(additional_text.Set((static_cast<SearchSuggestionAutocompleteItem*>(item->GetModelItem()))->GetSearchSuggestion()));
		}
		else
		{
			OpStatus::Ignore(history_item->GetTitle(additional_text));
		}
		RETURN_IF_ERROR(info.get()->SetAdditional_text(additional_text));
	}
	else
	{
		cache = item->GetItemDrawArea(this, real_col, FALSE);
		if (cache)
		{
			const uni_char *str = cache->m_strings.m_string.Get();
			if (str && *str)
			{
				RETURN_IF_ERROR(info.get()->SetText(str));
			}
		}

		associated_text = item->GetAssociatedText(this);
		if (associated_text)
		{
			// Set the text
			const uni_char *str = associated_text->m_strings.m_string.Get();
			if (str && *str)
			{
				RETURN_IF_ERROR(info.get()->SetAdditional_text(str));
			}
		}
	}
	// Set the visibility based on the visibility of the tree view itself
	BOOL visible = IsVisible();
	
	// If this widget is visible check if all it's parents are also visible
	if (visible)
	{
		OpWidget *parent_widget = this;
		
		while ((parent_widget = parent_widget->GetParent()) != NULL)
		{
			visible = parent_widget->IsVisible();
			
			if (!visible)
				break;
		}
	}

	
	// If there is no cell rect then the item is invisible
	if (cell_rect.width == 0 && cell_rect.height == 0)
		visible = FALSE;

	// Should only get here for the visible items
	info.get()->SetVisible(visible);
	
	// Set the enabled flag based on the main control
	info.get()->SetEnabled(IsEnabled());

	// Set value based on Selected value 
	info.get()->SetValue(item->IsSelected());

	// Set the tree view as the parent
	if (GetName().HasContent())
	{
		OpString name;
		name.Set(GetName());
		info.get()->SetParent(name);
	}

	/*
	   Set the rect information.
	   Due to DSK-349574 we need to send back the _visible_ rect information, when
	   the driver tries to click a widget, it looks for the center of the widget
	   rect it got. If the item is not fully visible, the center is out of the
	   tree view and the item never gets clicked.
	*/
	
	// tree_rect is the screen rect of the whole tree view
	OpRect tree_rect = GetScreenRect();

	// visible_rect is the visible rect of the whole tree view
	OpRect visible_rect = GetVisibleRect();

	// visible_cell_rect is the visible rect of the tree view item
	OpRect visible_cell_rect = cell_rect;
	visible_cell_rect.IntersectWith(visible_rect);

	// cell_rect was relative to the tree_rect and we want the screen rect
	visible_cell_rect.x += tree_rect.x;
	visible_cell_rect.y += tree_rect.y;
	
	// If there is a text, take it into the account
	if (associated_text)
	{
		if (cache && cache->m_flags & FLAG_ASSOCIATED_TEXT_ON_TOP)
			visible_cell_rect.y -= GetLineHeight();

		visible_cell_rect.height += GetLineHeight();
	}

	// Set the final rect info
	info.get()->GetRectRef().SetX(visible_cell_rect.x);
	info.get()->GetRectRef().SetY(visible_cell_rect.y);
	info.get()->GetRectRef().SetWidth(visible_cell_rect.width);
	info.get()->GetRectRef().SetHeight(visible_cell_rect.height);

	// Set the row and column
	info.get()->SetRow(row);
	info.get()->SetCol(col);
	
	RETURN_IF_ERROR(list.GetQuickwidgetListRef().Add(info.get()));
	info.release();
	
	return OpStatus::OK;
}

OP_STATUS OpTreeView::AddQuickWidgetInfoInvisibleItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, INT32 start_pos, INT32 end_pos)
{
	// First check if there are items to get at all
	if (start_pos >= 0 && start_pos < end_pos)
	{
		TreeViewModelItem	*item = m_view_model.GetItemByIndex(start_pos);
		INT32				curr_pos = start_pos;
		OpRect				cell_rect;
		
		while (item)
		{
			INT32 visible_col_num = 0;
			
			// Loop all the columns
			for (UINT32 i = 0; i < m_columns.GetCount(); i++)
			{
				Column* column = m_columns.Get(i);
				
				if (!column->IsColumnReallyVisible())
				{
					continue;
				}
				
				OpStatus::Ignore(AddQuickWidgetInfoToList(list, item, i, cell_rect, curr_pos, visible_col_num));

				visible_col_num++;
			}

			// Get the next item
			item = m_view_model.GetItemByIndex(++curr_pos);

			// If we have reached the last item then just stop
			if (curr_pos >= end_pos)
				break;
		}
	}
	return OpStatus::OK;
}

bool OpTreeView::SetTopGroupHeader()
{
	if (!HasGrouping())
		return false;
	UINT32 old_count = m_group_headers_to_draw.GetCount();
	INT32 old_pos = old_count ? m_group_headers_to_draw.Get(0)->pos : -1;
	m_group_headers_to_draw.DeleteAll();

	const OpRect visible_rect = GetVisibleRect(false, false, true);
	int top_line = m_vertical_scrollbar->GetValue() / m_line_height;
	int line = GetLineByItem(GetItemByLine(top_line));
	line = line < 0 ? 0 : line;

	TreeViewModelItem* previous_item = m_view_model.GetItemByIndex(GetItemByLine(line - 1));
	TreeViewModelItem* item = m_view_model.GetItemByIndex(GetItemByLine(line));

	while (previous_item && !previous_item->IsHeader())
		previous_item = previous_item->GetParentItem();
	while (item && !item->IsHeader())
		item = item->GetParentItem();

	OpRect previous_item_rect;
	bool previous_item_exist = previous_item && previous_item != item && !GetItemRectLTR(GetItemByLine(line - 1), previous_item_rect, FALSE);
	TreeViewModelItem * item_covered_by_group_header = m_view_model.GetItemByIndex(GetItemByLine(line - 1));
	if (previous_item_exist)
	{
		OpAutoPtr<GroupHeaderInfo> header(OP_NEW(GroupHeaderInfo,()));
		RETURN_VALUE_IF_NULL(header.get(), false);
		header->rect = previous_item_rect;
		header->rect.y = previous_item_rect.y + (item_covered_by_group_header->GetNumLines() -1) * m_line_height;
		header->rect.height = m_line_height;
		header->pos = previous_item->GetIndex();
		header->item = previous_item;
		RETURN_VALUE_IF_ERROR(m_group_headers_to_draw.Add(header.get()), false);
		header.release();
	}

	if (item)
	{
		if (previous_item_exist && previous_item_rect.y <= visible_rect.y)
		{
			previous_item_rect.height = m_line_height;
			previous_item_rect.y = (previous_item_rect.y + item_covered_by_group_header->GetNumLines() * m_line_height);
		}
		else
		{
			previous_item_rect = visible_rect;
			previous_item_rect.height = m_line_height;
		}

		OpAutoPtr<GroupHeaderInfo> header(OP_NEW(GroupHeaderInfo,()));
		RETURN_VALUE_IF_NULL(header.get(), false);
		header->pos = item->GetIndex();
		header->rect = previous_item_rect;
		header->item = item;
		RETURN_VALUE_IF_ERROR(m_group_headers_to_draw.Add(header.get()), false);
		header.release();
	}

	return (old_count != m_group_headers_to_draw.GetCount()) || (m_group_headers_to_draw.GetCount() == 2) || m_group_headers_to_draw.GetCount() && m_group_headers_to_draw.Get(0)->pos != old_pos;
}

BOOL OpTreeView::HandleGroupHeaderMouseClick(const OpPoint& point, MouseButton button)
{
	OpRect header_rect = GetVisibleRect(false, false, true);
	header_rect.height = m_line_height;
	if (!header_rect.Contains(point))
		return FALSE;

	if (!m_group_headers_to_draw.GetCount() && !SetTopGroupHeader())
		return FALSE;

	int top_line = m_vertical_scrollbar->GetValue() / m_line_height;

	for (UINT32 i = 0; i < m_group_headers_to_draw.GetCount(); i++)
	{
		GroupHeaderInfo* header = m_group_headers_to_draw.Get(i);
		if (header && header->rect.Contains(point))
		{
			TreeViewModelItem * tmp = m_view_model.GetItemByIndex(GetItemByLine(top_line - 1));
			if (button == MOUSE_BUTTON_1)
			{
				if (i > 0 || tmp->GetID() == header->item->GetID())
				{
					OpenItem(tmp->GetIndex(), !tmp->IsOpen());
				}
				else
				{
					ScrollToLine(GetLineByItem(header->item->GetIndex()) + 1);
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}
