/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef TREE_VIEW_MODEL_ITEM_H
#define TREE_VIEW_MODEL_ITEM_H

#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleTextObject.h"
#endif

/**
 * @brief
 * @author Peter Karlsson
 *
 *
 */

class ItemDrawArea;
class OpWidgetImage;

class TreeViewModelItem :
	public TreeModelItem<TreeViewModelItem, TreeViewModel>
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public OpAccessibleItem
	, AccessibleTextObjectListener
#endif
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 *
	 * @param model_item
	 */
	TreeViewModelItem(OpTreeModelItem* model_item)
		: m_flags(0)
		, m_widget_image(NULL)
		, m_item_type(TREE_MODEL_ITEM_TYPE_NORMAL)
		, m_column_cache(NULL)
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		, m_view_model(NULL)
		, m_focused_col(0)
		, m_text_objects(NULL)
#endif
		, m_model_item(model_item)
		, m_associated_text(NULL)
		, m_associated_image(NULL)
	{
		OP_ASSERT(model_item);
	}

	/**
	 *
	 *
	 */
	virtual	~TreeViewModelItem();

	/**
	 *
	 *
	 */
	void Clear();

	/**
	 * Gets the associated text to be drawn beneath the column items
	 *
	 */
	ItemDrawArea* GetAssociatedText(OpTreeView* view);

	/**
	 * Gets the associated image to be drawn beneath the column items
	 *
	 */
	ItemDrawArea* GetAssociatedImage(OpTreeView* view);

	/**
	 * Gets the skin to be drawn beneath the item
	 *
	 */
	const char *GetSkin(OpTreeView* view);

	/**
	 *
	 * @param view
	 * @param column
	 * @param paint
	 *
	 * @return
	 */
	ItemDrawArea * GetItemDrawArea(OpTreeView* view,
								   INT32 column,
								   BOOL paint);

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsFocused() 
		{return (m_flags & OpTreeModelItem::FLAG_FOCUSED) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsSelected()
		{return (m_flags & OpTreeModelItem::FLAG_SELECTED) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsOpen()
		{return (m_flags & OpTreeModelItem::FLAG_OPEN) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsMatched()
		{return (m_flags & OpTreeModelItem::FLAG_MATCHED) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsChecked()
		{return (m_flags & OpTreeModelItem::FLAG_CHECKED) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsDisabled()
		{return (m_flags & (OpTreeModelItem::FLAG_DISABLED |
							OpTreeModelItem::FLAG_MATCH_DISABLED |
							OpTreeModelItem::FLAG_TEXT_SEPARATOR)) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsTextSeparator()
		{return (m_flags & OpTreeModelItem::FLAG_TEXT_SEPARATOR) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsFormattedText()
		{return (m_flags & OpTreeModelItem::FLAG_FORMATTED_TEXT) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsChanged()
		{return ((m_flags & OpTreeModelItem::FLAG_CHECKED) &&
				 !(m_flags & OpTreeModelItem::FLAG_INITIALLY_CHECKED)) ||
			 (!(m_flags & OpTreeModelItem::FLAG_CHECKED) &&
			  (m_flags & OpTreeModelItem::FLAG_INITIALLY_CHECKED));}

	/**
	 *
	 *
	 * @return
	 */
	BOOL CanOpen()
		{return (m_flags & OpTreeModelItem::FLAG_MATCHED_CHILD) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	BOOL IsSeparator()
		{return (m_flags & OpTreeModelItem::FLAG_SEPARATOR) != 0;}

	/**
	*
	*
	* @return
	*/
	BOOL IsNoSelect()
		{return (m_flags & OpTreeModelItem::FLAG_NO_SELECT) != 0;}

	/**
	*
	*
	* @return TRUE if the item is selectable. That is: Not a disabled item and doesn't have flag FLAG_NO_SELECT.
	*/
	BOOL IsSelectable()
		{return !IsDisabled() && !IsNoSelect(); }

	/**
	*
	*
	* @return TRUE if the whole item is a custom widget
	*/
	BOOL IsCustomWidget()
		{return (m_flags & OpTreeModelItem::FLAG_CUSTOM_WIDGET) != 0;}

	/**
	*
	*
	* @return TRUE if the item is a group header
	*/
	BOOL IsHeader() const
		{return m_item_type == TREE_MODEL_ITEM_TYPE_HEADER;}
	
	/**
	*
	*
	* @return TRUE if one of the cells has a custom widget
	*/
	BOOL HasCustomCellWidget()
	{return (m_flags & OpTreeModelItem::FLAG_CUSTOM_CELL_WIDGET) != 0;}

	/**
	*
	*
	* @return TRUE if the item wants the associated text on top
	*/
	BOOL HasAssociatedTextOnTop()
	{return (m_flags & OpTreeModelItem::FLAG_ASSOCIATED_TEXT_ON_TOP) != 0;}

	/**
	 *
	 *
	 * @return
	 */
	void SetModelItem(OpTreeModelItem* model_item)
		{ m_model_item = model_item; }

	/**
	 *
	 *
	 * @return
	 */
	OpTreeModelItem * GetModelItem() const
		{return m_model_item;}

	/**
	 *
	 *
	 * @return
	 */
	INT32 GetID()
		{return m_model_item->GetID();}

	/**
	 *
	 *
	 * @return
	 */
	Type GetType()
		{return m_model_item->GetType();}

	/**
	 *
	 *
	 * @return
	 */
	OP_STATUS GetItemData(ItemData* item_data)
		{return m_model_item->GetItemData(item_data);}

	virtual int GetNumLines() { return m_model_item->GetNumLines(); }

	/**
	 *
	 *
	 */
	void OnAdded();

	/**
	 *
	 *
	 */
	void OnRemoving();

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/**
	 *
	 * @param view_model
	 */
	void SetViewModel(TreeViewModel* view_model);

	/**
	 *
	 * @return
	 */
	TreeViewModel * GetViewModel() { return m_view_model; }

	/**
	 *
	 *
	 */
	OP_STATUS CreateTextObjectsIfNeeded();

	Accessibility::State GetUnfilteredState();

	virtual OP_STATUS	AccessibilityClicked();
	virtual BOOL		AccessibilityIsReady() const {return TRUE;}
	virtual BOOL		AccessibilitySetFocus();
	virtual OP_STATUS	AccessibilityGetText(OpString& str);
	virtual OP_STATUS	AccessibilityGetDescription(OpString& str);
	virtual OP_STATUS	AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS	AccessibilityGetValue(int &value);
	virtual BOOL		AccessibilitySetExpanded(BOOL expanded);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual Accessibility::State AccessibilityGetState();
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent() {return m_view_model;}
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetNextAccessibleSibling() { return NULL; }
	virtual OpAccessibleItem* GetPreviousAccessibleSibling() { return NULL; }
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual OpAccessibleItem* GetLeftAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetRightAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetDownAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetUpAccessibleObject() { return NULL; }

	virtual OP_STATUS GetAbsolutePositionOfObject(int i, OpRect& rect);
	virtual OP_STATUS SetFocusToObject(int i);
	virtual OP_STATUS GetTextOfObject(int i, OpString& str);
	virtual OP_STATUS GetDecriptionOfObject(int i, OpString& str);
	virtual Accessibility::State GetStateOfObject(int i);
	virtual OP_STATUS ObjectClicked(int i);
	virtual OpAccessibleItem* GetAccessibleParentOfObject();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	static OP_STATUS ParseFormattedText(OpTreeView* view,
										ItemDrawArea* cache,
										const OpStringC& formatted_text);

//  -------------------------
//  Public member varables:
//  -------------------------

	UINT32			m_flags;
	OpWidgetImage*	m_widget_image;
	INT32			m_item_type;

private:

	ItemDrawArea*	m_column_cache;

//  -------------------------
//  Private member functions:
//  -------------------------

	OP_STATUS SetDrawAreaText(OpTreeView* view,
							  OpString & string,
							  ItemDrawArea* cache);

	void	  RemoveCustomCellWidget();

//  -------------------------
//  Private member variables:
//  -------------------------

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	TreeViewModel*				m_view_model;
	int							m_focused_col;
	OpAutoVector<AccessibleTextObject>* m_text_objects;
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

	OpTreeModelItem*	m_model_item;

	ItemDrawArea * m_associated_text;

	ItemDrawArea * m_associated_image;
	OpString8      m_associated_image_skin_elm;
};

#endif // TREE_VIEW_MODEL_ITEM_H
