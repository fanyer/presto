/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef TREE_VIEW_MODEL_H
#define TREE_VIEW_MODEL_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
#include "modules/accessibility/AccessibleTextObject.h"
#endif

/**
 * @brief
 * @author Peter Karlsson
 *
 *
 */

class TreeViewModelItem;
class OpTreeView;

class TreeViewModel :
	public TreeModel<TreeViewModelItem>,
	public OpTreeModel::Listener
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public OpAccessibleItem
#endif
{
	friend class TreeViewModelItem;

public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 *
	 *
	 */
	TreeViewModel() :
		m_model(NULL),
		m_view(NULL),
		m_resorting(FALSE)
		{}

	/**
	 *
	 *
	 */
	virtual ~TreeViewModel();

	/**
	 *
	 * @param model
	 */
	void SetModel(OpTreeModel* model, OpTreeModel::TreeModelGrouping* grouping = NULL);

	/**
	 *
	 * @return
	 */
	OpTreeModel * GetModel()
		{return m_model;}

	/**
	 *
	 *
	 */
	void Resort();

	/**
	 *
	 *
	 */
	void Regroup();

	/**
	 *
	 * @param model_index
	 *
	 * @return
	 */
	INT32 GetIndexByModelIndex(INT32 model_index);

	/**
	 *
	 * @param item
	 *
	 * @return
	 */
	INT32 GetIndexByModelItem(OpTreeModelItem* item);

	/**
	 *
	 * @param item
	 *
	 * @return
	 */
	virtual TreeViewModelItem *	GetItemByModelItem(OpTreeModelItem* item);

	/**
	 *
	 * @param index
	 *
	 * @return
	 */
	INT32 GetModelIndexByIndex(INT32 index);

	/**
	 *
	 *
	 * @return
	 */
	INT32 GetColumnCount()
		{OP_ASSERT(0); return 0;}

	/**
	 *
	 * @param column_data
	 *
	 * @return
	 */
	OP_STATUS GetColumnData(ColumnData* column_data)
		{OP_ASSERT(0); return OpStatus::ERR;}

	// == OpTreeModel::Listener ======================

	void OnItemAdded(OpTreeModel* tree_model, INT32 index);
	void OnItemChanged(OpTreeModel* tree_model, INT32 index, BOOL sort);
	void OnItemRemoving(OpTreeModel* tree_model, INT32 index) {}
	void OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size);
	void OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent_index, INT32 index, INT32 subtree_size) {}
	void OnTreeChanging(OpTreeModel* tree_model);
	void OnTreeChanged(OpTreeModel* tree_model);
	void OnTreeDeleted(OpTreeModel* tree_model);

	void				SetView(OpTreeView* view);
	OpTreeView*			GetView() { return m_view; }
	void RegroupItem(INT32 pos);
	BOOL IsEmptyGroup(TreeViewModelItem* item);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS 	GetTypeString(OpString& type_string) { return OpStatus::ERR; }
	void				SetFocusToItem(int item);
	Accessibility::State GetUnfilteredState() const;


	virtual OP_STATUS	AccessibilityClicked();
	virtual BOOL		AccessibilityIsReady() const {return TRUE;}
	virtual BOOL		AccessibilityChangeSelection(Accessibility::SelectionSet flags, OpAccessibleItem* child);
	virtual BOOL		AccessibilitySetFocus();
	virtual OP_STATUS	AccessibilityGetText(OpString& str);
	virtual OP_STATUS	AccessibilityGetDescription(OpString& str);
	virtual OP_STATUS	AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual Accessibility::ElementKind AccessibilityGetRole() const;
	virtual Accessibility::State AccessibilityGetState();
	virtual int	GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleParent();
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

#endif //ACCESSIBILITY_EXTENSION_SUPPORT
protected:
	virtual void Init();

	OpHashTable			m_hash_table;
	OpTreeModel*		m_model;


private:
//  -------------------------
//  Private member functions:
//  -------------------------

	void Clear();

//  -------------------------
//  Private member varables:
//  -------------------------

	OpTreeView*			m_view;
	BOOL				m_resorting;
};

#endif // TREE_VIEW_MODEL_H
