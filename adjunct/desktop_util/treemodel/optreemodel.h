/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ---------------------------------------------------------------------------------

#ifndef ADJUNCT_DESKTOP_UTIL_TREEMODEL_OPTREEMODEL_H
#define ADJUNCT_DESKTOP_UTIL_TREEMODEL_OPTREEMODEL_H

#include "adjunct/quick/quick_capabilities.h"

#include "optreemodelitem.h"
#include "modules/util/adt/opvector.h"

typedef int (*compare_function_t)(const void*, const void*);
/* Need to declare class before all uses of its name; gcc 4 is pedantic enough
 * to not accept the prior friend class GenericTreeModel as a declaration. */
class GenericTreeModel;

/***********************************************************************************
**
**	OpTreeModel is an abstract base class for all tree models that contains
**	OpTreeModelItem items
**
**	The base class defines a pure virtual interface that all tree models must implement.
**
**	It also implements some functionality, like adding and removing listeners.
**	Also, it contains protected helper functions for using these listeners to broadcast changes.
**
**
**	                     Model class tree:
**
**
**                         OpTypedObject
**                               |
**                               |
**                          OpTreeModel
**                               |
**                               |
**              +----------------+-----------------------------------+
**              |                                                    |
**              |                                                    |
**       GenericTreeModel                        <custom tree models that handle tree data themselves>
** (uses OpTree to store items derived from GenericTreeModelItem)
**              |
**              |
**  TreeModel<HotlistModelItem>
**  (introduces templating, with HostlistModelItem as example)
**              |
**              |
**       HotlistModel
**    (typical example of subclass use)
**
**
**
**	                   Model item class tree:
**
**
**
**                         OpTypedObject
**                               |
**                               |
**                        OpTreeModelItem
**                               |
**                               |
**              +----------------+-----------------------------------+
**              |                                                    |
**              |                                                    |
**    GenericTreeModelItem                         <other item classes for custom tree models>
** (GenericTreeModel stores items derived from this class)
**              |
**              |
**  TreeModelItem<HotlistModel>
** (introduces templating, with HotlistModel as example of model that will contain these items)
**              |
**              |
**      HotlistModelItem
**    (example subclass use)
**
**
**
**
**	You would most likely want to do the following
**
**	class MyModelItem : public TreeModelItem<MyModel>
**	{
**		// your stuff.. you need to implement at least
**		GetID()
**		GetType()
**		GetItemData()
**	}
**
**	class MyModel : public TreeModel<MyModelItem>
**	{
**		// your stuff.. you need to implement at least:
**		GetID()
**		GetType()
**		GetColumnCount()
**	    GetColumnData()
**	}
**
***********************************************************************************/

class OpTreeModel
{
	public:

		virtual	~OpTreeModel() {}

		class Listener
		{
			public:
			virtual ~Listener() {}

				// sent after item is added
				virtual	void OnItemAdded(OpTreeModel* tree_model, INT32 item) = 0;

				// sent when item has changed.. please requery columns etc.
				virtual	void OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort) = 0;

				// sent before item is being removed.. so the position is still valid
				// item is the index of an item being removed
				// IMPORTANT: This is sent for every item that is being removed

				virtual	void OnItemRemoving(OpTreeModel* tree_model, INT32 item) = 0;

				// sent before item is being removed.. so the position is still valid
				// parent is the parent of the subtree that is being removed
				// subtree_root is the index of the subtree being removed
				// subtree_size, if this is > 0, then all children are also being removed
				// IMPORTANT: This is only sent for the root of the subtree being removed, not for every item

				virtual	void OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) = 0;

				// sent after subtree has been removed
				// parent is the parent of the subtree that has been removed
				// subtree_root was the index of the subtree that was removed
				// subtree_size, if this is > 0, then all children are also removed
				// IMPORTANT: This is only sent for the root of the subtree being removed, not for every item

				virtual	void OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) = 0;

				// sent when whole tree is changing, and listener should prepare to rebuild from scratch.
				// OnItemRemoving and OnItemRemoved will not be called.
				// All items are still valid, but will soon be gone

				virtual void OnTreeChanging(OpTreeModel* tree_model) = 0;

				// sent when whole tree is changed, and listener should rebuild from scratch.
				// OnItemRemoving and OnItemRemoved will not be called.
				// All previous items must be considered deleted, so any pointers you kept are invalid.

				virtual void OnTreeChanged(OpTreeModel* tree_model) = 0;

				// sent when whole tree is being deleted, in those cases where model might be
				// deleted just before the treeview (listener)

				virtual void OnTreeDeleted(OpTreeModel* tree_model) = 0;
		};

		class SortListener
		{
			public:
			virtual ~SortListener() {}

				virtual INT32 OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1) = 0;
		};

		class ColumnData
		{
			public:

				ColumnData(INT32 col)
				{
					column = col;
					image = NULL;
					sort_by_string_first = FALSE;
					custom_sort = FALSE;
					matchable = FALSE;
					right_aligned = FALSE;
				}

				INT32				column;
				OpString			text;
				const char*			image;
				BOOL				sort_by_string_first;
				BOOL				custom_sort;
				BOOL				matchable;
				BOOL				right_aligned;
		};

		class TreeModelGrouping
		{
		public:
										TreeModelGrouping() {}
			virtual						~TreeModelGrouping() {}
			
			/**
			 * Get index of a header for given item
			 * @param item
			 * @return index of header or -1 if an item is a header
			 */
			virtual INT32				GetGroupForItem(const OpTreeModelItem* item) const = 0;

			/**
			 * Get count of group for current grouping method.
			 */
			virtual INT32				GetGroupCount() const = 0;
			/**
			 * Get a group header.
			 * @param group_idx group index
			 * @return OpTreeModelItem which is a header for current grouping method
			 */
			virtual OpTreeModelItem*	GetGroupHeader(UINT32 group_idx) const = 0;
			/**
			 * Return true if current grouping method contains a header.
			 */
			virtual BOOL				IsVisibleHeader(const OpTreeModelItem* header) const = 0;
			/**
			 * Return true if given item is a header
			 */
			virtual BOOL				IsHeader(const OpTreeModelItem* header) const { return false; }

			/**
			 * Return true if any grouping method is set
			 */
			virtual BOOL HasGrouping() const = 0;
		};

		/********************************************************************************************
		**
		** Following functions are pure virtual and must be implemented by a subclass
		**
		********************************************************************************************/

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		// Get a string that describes the type (for accessibility use only!)

		virtual OP_STATUS GetTypeString(OpString& type_string) { return OpStatus::ERR; }
#endif

		// Get number of items

		virtual INT32				GetItemCount() = 0;

		// Get OpTreeModelItem item pointer based on position or NULL if out of range

		virtual OpTreeModelItem*	GetItemByPosition(INT32 position) = 0;

		// Get parent position of item at position.

		virtual INT32				GetItemParent(INT32 position) = 0;

		// Get column count

		virtual INT32				GetColumnCount() = 0;

		// Get column data
		virtual OP_STATUS			GetColumnData(ColumnData* column_data) = 0;

		/********************************************************************************************
		**
		** Following functions are implemented by OpTreeModel since there is either no need
		** to have duplicate code in models lying around, or a default implementation
		** makes sense.
		**
		********************************************************************************************/

		// Get OpTreeModelItem item pointer based on id. Set to NULL if not found

		virtual void				GetItemByID(INT32 id, OpTreeModelItem*& item);

		// Get OpTreeModelItem item pointer based on id and type. Set to NULL if not found

		virtual void				GetItemByIDAndType(INT32 id, OpTypedObject::Type type, OpTreeModelItem*& item);

		// Compare items

		virtual INT32				CompareItems(INT32 column, OpTreeModelItem* item1, OpTreeModelItem* item2) {OP_ASSERT(0); return 0;}

		/********************************************************************************************
		**
		** Public listener functions used by those that want to listen to changes
		**
		********************************************************************************************/

		// Add listener

		virtual OP_STATUS			AddListener(Listener* listener) { return m_listeners.Add(listener); }

		// Remove listener

		virtual OP_STATUS			RemoveListener(Listener* listener);

		// Get number of listeners

		INT32						GetListenerCount() {return m_listeners.GetCount();}

		// Get listener by index

		Listener*					GetListener(INT32 index) {return m_listeners.Get(index);}

	protected:

		/********************************************************************************************
		**
		** Following functions are helpers used by subclasses to broadcast changes
		**
		********************************************************************************************/

		void						BroadcastItemAdded(INT32 index);
		void						BroadcastItemChanged(INT32 index, BOOL sort = FALSE);
		void						BroadcastItemRemoving(INT32 index) {BroadcastSubtreeRemoving(-1, index, 0);}
		void						BroadcastItemRemoved(INT32 index) {BroadcastSubtreeRemoved(-1, index, 0);}
		void						BroadcastSubtreeRemoving(INT32 parent, INT32 index, INT32 subtreesize);
		void						BroadcastSubtreeRemoved(INT32 parent, INT32 index, INT32 subtreesize);
		void						BroadcastTreeChanging();
		void						BroadcastTreeChanged();
		void						BroadcastTreeDeleted();

	private:

		OpVector<Listener>			m_listeners;
};

/***********************************************************************************
**
**	GenericTreeModel is an abstract base class that uses OpTree as data container to
**  implement some of the missing pure virtual functions from OpTreeModel
**
**	GenericTreeModel store items derived from the GenericTreeModelItem class
**	and introduces editing functions like AddItem, RemoveItem, DeleteItem etc.
**
**	To speed up a batch of changes, wrap the editing with BeginChange and EndChange
**  calls. This will cause listeners to not be notified of changes.
**  Instead, OnTreeChanged will be called at EndChange time.
**
***********************************************************************************/

class GenericTreeModelItem;

class GenericTreeModel : public OpTreeModel
{
	// GenericTreeModelItem must be friend class to let it call private Set/Get functions

	friend class GenericTreeModelItem;

	public:

									GenericTreeModel() : m_sort_listener(NULL), m_change_counter(0), m_change_type(NO_CHANGES), m_is_flat(TRUE), m_tree_model_grouping(NULL) {}
		virtual						~GenericTreeModel();

		/********************************************************************************************
		**
		**	A sort listener must be set if Sort and AddSorted will be used
		**
		********************************************************************************************/

		void						SetSortListener(SortListener* sort_listener) {m_sort_listener = sort_listener;}
		SortListener*				GetSortListener() {return m_sort_listener;}

		/********************************************************************************************
		**
		**	These functions are just to fulfill the OpTreeModel interface (as used by OpTreeView)
		**	The proper API to be used by the model owners are below
		**
		********************************************************************************************/

		INT32						GetItemCount() {return GetCount();}
		OpTreeModelItem*			GetItemByPosition(INT32 position) {return (OpTreeModelItem*)GetGenericItemByIndex(position);}
		INT32						GetItemParent(INT32 position) {return GetParentIndex(position);}

		/********************************************************************************************
		**
		**	The public API of a tree model follows
		**
		********************************************************************************************/

		BOOL						IsItem(INT32 index) {return index >= 0 && index < GetCount();}

		// Get number of items

		INT32						GetCount() const {return m_items.GetCount();}

		// Get index of item

		INT32						GetIndexByItem(GenericTreeModelItem* item);

		// Get parent of item

		INT32						GetParentIndex(INT32 index) { return GetIndexByItem(GetGenericParentItem(index)); }

		// Get first sibling of item

		INT32						GetSiblingIndex(INT32 index);


		// Get previous of item

		INT32						GetPreviousIndex(INT32 index);


		// Get first child of item

		INT32						GetChildIndex(INT32 index) { return GetSubtreeSize(index) == 0 ? -1 : index + 1; }

		// Get last child of item

		INT32						GetLastChildIndex(INT32 index);

		// Get closest leaf index (depth-first search), or -1 if no more leaves from here

		INT32						GetPreviousLeafIndex(INT32 index);

		// Get closest leaf index (depth-first search), or -1 if no more leaves from here

		INT32						GetNextLeafIndex(INT32 index);

		// Get child count for this item

		INT32						GetChildCount(INT32 index);

		// Get subtree size

		INT32						GetSubtreeSize(INT32 index);

		// Returns true if the tree is flat (if no item has children)

		BOOL						IsFlat() {return m_is_flat;}

		// Add item as first child of parent (defaults to parent = -1 which means under root)

		INT32						AddFirst(GenericTreeModelItem* item, INT32 parent_index = -1) { return Insert(item, GetGenericItemByIndex(parent_index), parent_index + 1); }

		// Add item as last child of parent (defaults to parent = -1 which means under root)

		INT32						AddLast(GenericTreeModelItem* item, INT32 parent_index = -1) { return Insert(item, GetGenericItemByIndex(parent_index), parent_index + GetSubtreeSize(parent_index) + 1); }

		// Add item into sorted position under parent. It will only work correctly
		// if the items in the tree are all in sorted order, ie that all previous items
		// were added with AddItemSorted() or Sort() was used.
		// The sorting listener set with SetSortListener will be called for each comparison

		INT32						AddSorted(GenericTreeModelItem* item, INT32 parent = -1) { return ResortItem(AddLast(item, parent)); }

		// Resort one item in the tree (to use when sorting property of this item has changed)

		INT32						ResortItem(INT32 index);

		// Insert item before sibling

		INT32						InsertBefore(GenericTreeModelItem* item, INT32 sibling_index) { return Insert(item, GetGenericParentItem(sibling_index), sibling_index); }

		// Insert item after sibling

		INT32						InsertAfter(GenericTreeModelItem* item, INT32 sibling_index) { return Insert(item, GetGenericParentItem(sibling_index), sibling_index + GetSubtreeSize(sibling_index) + 1); }

		// Sort items. The sorting listener set with SetSortListener will be called for each comparison

		OP_STATUS					Sort();

		// Inform model that item has changed.. it will broadcast it to listeners

		void						Change(INT32 index, BOOL sorting_might_have_changed = FALSE);

		// Inform model that all items have changed.. ie, cause a redraw

		void						ChangeAll();

		// Remove item, and normally that means removing all children too. If all_children is FALSE,
		// the effect is that all children are reparented to the parent of the removing item

		GenericTreeModelItem*		Remove(INT32 index, BOOL all_children = TRUE);

		// Remove all items, without deleting them

		void						RemoveAll();

		// Delete item, and normally that means deleting all children too. If all_children is FALSE,
		// the effect is that all children are reparented to the parent of the deleting item

		void						Delete(INT32 index, BOOL all_children = TRUE) { if (IsItem(index))return Remove(index, all_children, TRUE); }

		// Delete all items.

		void						DeleteAll();

		void						Move(INT32 index, INT32 parent_index, INT32 previous_index);

		void						SetTreeModelGrouping(TreeModelGrouping* groups) { m_tree_model_grouping = groups; }
		TreeModelGrouping*			GetTreeModelGrouping() const { return m_tree_model_grouping; }
		OP_STATUS					RegroupItems();
		virtual void				GetItemByID(INT32 id, OpTreeModelItem*& item) { m_id_to_item.GetData(id, &item);}

		/********************************************************************************************
		**
		**	Use BeginChange/EndChange to avoid broadcasting listeners for every change
		**  and instead broadcast what is needed at EndChange time
		**
		**	Best thing to do is to use a ModelLock object on the stack which handles it for you
		**  In other words, you don't have to remember to call EndChange since it is called in the destructor
		**	when leaving the function context
		**
		********************************************************************************************/

		void						BeginChange() {m_change_counter++;}
		void						EndChange();

		class ModelLock
		{
			public:
					ModelLock(GenericTreeModel* model) : m_model(model) { m_model->BeginChange(); }
					~ModelLock() { m_model->EndChange(); }
			private:
					GenericTreeModel*	m_model;
		};

	protected:

		// protected generic get'ers that will be used by the proper public typed get'ers in templated subclass

		GenericTreeModelItem*		GetGenericItemByIndex(INT32 index);
		GenericTreeModelItem*		GetGenericItemByID(INT32 id);
		GenericTreeModelItem*		GetGenericItemByIDAndType(INT32 id, OpTypedObject::Type type);
		GenericTreeModelItem*		GetGenericParentItem(INT32 index);
		GenericTreeModelItem*		GetGenericSiblingItem(INT32 index) { return GetGenericItemByIndex(GetSiblingIndex(index)); }
		GenericTreeModelItem*		GetGenericPreviousItem(INT32 index) { return GetGenericItemByIndex(GetPreviousIndex(index)); }
		GenericTreeModelItem*		GetGenericChildItem(INT32 index) { return GetGenericItemByIndex(GetChildIndex(index)); }
		GenericTreeModelItem*		GetGenericLastChildItem(INT32 index) { return GetGenericItemByIndex(GetLastChildIndex(index)); }
		GenericTreeModelItem*		GetGenericPreviousLeafItem(INT32 index) { return GetGenericItemByIndex(GetPreviousLeafIndex(index)); }
		GenericTreeModelItem*		GetGenericNextLeafItem(INT32 index) { return GetGenericItemByIndex(GetNextLeafIndex(index)); }

	private:

		enum ChangeType
		{
			NO_CHANGES,
			ITEMS_CHANGED,
			TREE_CHANGED
		};

		BOOL								SetChangeType(ChangeType type);
		INT32								Insert(GenericTreeModelItem* item, GenericTreeModelItem* parent, INT32 index);
		void								Remove(INT32 index, BOOL all_children, BOOL delete_item);
		void								Removing(INT32 index, INT32 count);
		void								UpdateSubtreeSizes(GenericTreeModelItem* parent, INT32 count);
		void								SortChildren(INT32 parent_index, INT32* sort_array, INT32* temp_array, GenericTreeModelItem** result_array, INT32& current_quicksort_array_pos, INT32& current_result_array_pos);
		void								MergeSort(INT32* array, INT32* temp_array, INT32 left, INT32 right);
		void								Merge(INT32* array, INT32* temp_array, INT32 left, INT32 right, INT32 right_end);
		void								QuickSort(INT32* array, INT32 count);
		void								HeapSort(INT32* array, INT32 count);
		void								HeapTraverse(INT32* array, INT32 index, INT32 count);
		INT32								Compare(INT32 index1, INT32 index2);
		OP_STATUS							RemoveGroups();
		void								ReleaseMemory(OpVector<GenericTreeModelItem>** tab);

		SortListener*						m_sort_listener;
		INT32								m_change_counter;
		ChangeType							m_change_type;
		OpVector<GenericTreeModelItem>		m_items;
		OpINT32HashTable<OpTreeModelItem> m_id_to_item;
		INT32								m_comparison_counter;
		BOOL								m_is_flat;
		TreeModelGrouping*					m_tree_model_grouping;

		static int							StaticCompareFunction(const void* p0, const void* p1);
		static GenericTreeModel*			s_sort_model;
};

/***********************************************************************************
**
**	GenericTreeModelItem
**
**	Base class for items that are added to GenericTreeModel
**
**	GenericTreeModel and GenericTreeModelItem work together to make it easy
**  to use template versions of models and items with no code duplication
**
**	While an OpTreeModelItem might theoretically belong to multiple models at the same
**	time, this not possible with GenericTreeModelItem and its subclasses.
**
**	GenericTreeModel stores pointer to itself in each item in addition to
**	last_index cache to make mapping item pointer to an index faster
**
***********************************************************************************/

class GenericTreeModelItem : public OpTreeModelItem
{
	// OpVector must be friend class to have access to protected destructor

	friend class OpVector<GenericTreeModelItem>;

	// GenericTreeModel must be friend class to let it call private Set/Get functions

	friend class GenericTreeModel;

	public:

								GenericTreeModelItem() :
									m_model(NULL),
									m_parent(NULL),
									m_last_index(0),
									m_sub_tree_size(0) {};

		// Get index of this item in the model, or -1 if doesn't exist in a model

		INT32					GetIndex() { return m_model ? m_model->GetIndexByItem(this) : -1; }

		// Get parent

		INT32					GetParentIndex() { return m_model ? m_model->GetParentIndex(GetIndex()) : -1; }

		// Get first sibling of this item, or -1 if no siblings

		INT32					GetSiblingIndex() { return m_model ? m_model->GetSiblingIndex(GetIndex()) : -1; }

	    // Get previous of this item, or -1 if no such item

		INT32                   GetPreviousIndex() { return m_model ? m_model->GetPreviousIndex(GetIndex()) : -1; }

		// Get first child of this item, or -1 if no children

		INT32					GetChildIndex() { return m_model ? m_model->GetChildIndex(GetIndex()) : -1; }

		// Get last child of this item, or -1 if no children

		INT32					GetLastChildIndex() { return m_model ? m_model->GetLastChildIndex(GetIndex()) : -1; }

		// Get closest leaf index (depth-first search), or -1 if no more leaves from here

		INT32					GetPreviousLeafIndex() { return m_model ? m_model->GetPreviousLeafIndex(GetIndex()) : -1; }

		// Get closest leaf index (depth-first search), or -1 if no more leaves from here

		INT32					GetNextLeafIndex() { return m_model ? m_model->GetNextLeafIndex(GetIndex()) : -1; }

		// Get child count for this item

		INT32					GetChildCount() { return m_model ? m_model->GetChildCount(GetIndex()) : 0; }

		// Get subtree size

		INT32					GetSubtreeSize() {return m_sub_tree_size;}

		// Add an item as first child of this, return pos of child or -1 if error

		INT32					AddChildFirst(GenericTreeModelItem* item) { return m_model ? m_model->AddFirst(item, GetIndex()) : -1; }

		// Add an item as last child of this, return pos of child or -1 if error

		INT32					AddChildLast(GenericTreeModelItem* item) { return m_model ? m_model->AddLast(item, GetIndex()) : -1; }

		// Add an item as sibling before this, return pos of sibling or -1 if error

		INT32					InsertSiblingBefore(GenericTreeModelItem* item) { return m_model ? m_model->InsertBefore(item, GetIndex()) : -1; }

		// Add an item as sibling after this, return pos of sibling or -1 if error

		INT32					InsertSiblingAfter(GenericTreeModelItem* item) { return m_model ? m_model->InsertAfter(item, GetIndex()) : -1; }

		// ChangeItem. This means that the item has been edited, and you want to inform
		// any listeners of the model that this item might be in about the changes

		void					Change(BOOL sorting_might_have_changed = FALSE) { if (m_model) m_model->Change(GetIndex(), sorting_might_have_changed); }

		// RemoveItem. Removes the item from any model it might be in. Listeners are informed properly.
		// if all_children is FALSE, they are reparented to parent of removing item

		void					Remove(BOOL all_children = TRUE) { if (m_model) m_model->Remove(GetIndex(), all_children); }

		// DeleteItem. Remove item from any model and delete it. Listeners are informed of removal.
		// if all_children is FALSE, children are reparented to have parent of the item being deleted.

		void					Delete(BOOL all_children = TRUE) { if (m_model) m_model->Delete(GetIndex(), all_children); else OP_DELETE(this); }

		// Is item in a model

		BOOL					IsInModel() {return m_model != NULL;}

		// Is item in a specific model

		BOOL					IsInModel(GenericTreeModel* model) {return model == m_model;}

		// destructor must be public in order to be accessible from the OpAutoVector template.

		virtual					~GenericTreeModelItem() {}

		// These are hooks that items can overload to know when they are added or removed to a model

		virtual void			OnAdded() {}
		virtual void			OnRemoving() {}
		virtual void			OnRemoved() {}

	protected:


		// protected generic get'ers that will be used by the proper public typed get'ers in templated subclass

		GenericTreeModel*		GetGenericModel() const {return m_model;}
		GenericTreeModelItem*	GetGenericParentItem() const {return m_parent;}
		GenericTreeModelItem*	GetGenericSiblingItem() { return m_model ? m_model->GetGenericSiblingItem(GetIndex()) : NULL; }
		GenericTreeModelItem*	GetGenericPreviousItem() { return m_model ? m_model->GetGenericPreviousItem(GetIndex()) : NULL; }
		GenericTreeModelItem*	GetGenericChildItem() { return m_model ? m_model->GetGenericChildItem(GetIndex()) : NULL; }
		GenericTreeModelItem*	GetGenericLastChildItem() { return m_model ? m_model->GetGenericLastChildItem(GetIndex()) : NULL; }
		GenericTreeModelItem*	GetGenericPreviousLeafItem() { return m_model ? m_model->GetGenericPreviousLeafItem(GetIndex()) : NULL; }
		GenericTreeModelItem*	GetGenericNextLeafItem() { return m_model ? m_model->GetGenericNextLeafItem(GetIndex()) : NULL; }

	private:

		// Used by GenericTreeModel to manage tree

		void					SetGenericModel(GenericTreeModel* model) {m_model = model;}
		void					SetGenericParentItem(GenericTreeModelItem* parent) {m_parent = parent;}
		void					SetSubtreeSize(INT32 sub_tree_size) {m_sub_tree_size = sub_tree_size;}
		void					AddSubtreeSize(INT32 value) {m_sub_tree_size += value;}

		// Used by GenericTreeModel to speed up item to index mapping

		void					SetLastIndex(INT32 index) {m_last_index = index;}
		INT32					GetLastIndex() {return m_last_index;}

		GenericTreeModel*		m_model;
		GenericTreeModelItem*	m_parent;
		INT32					m_last_index;
		INT32					m_sub_tree_size;
};


/***********************************************************************************
**
**	TreeModelItem is an abstract template class from which you would want
**	to derive your own model item classes.
**
***********************************************************************************/

template<class T, class U> class TreeModelItem : public GenericTreeModelItem
{
	public:

		U*							GetModel() const { return static_cast<U*>(GetGenericModel()); }

		T*							GetParentItem() const { return static_cast<T*>(GetGenericParentItem()); }

		T*							GetSiblingItem() { return static_cast<T*>(GetGenericSiblingItem()); }

		T*							GetPreviousItem() { return static_cast<T*>(GetGenericPreviousItem()); }

		T*							GetChildItem() { return static_cast<T*>(GetGenericChildItem()); }

		T*							GetLastChildItem() { return static_cast<T*>(GetGenericLastChildItem()); }

		T*							GetPreviousLeafItem() { return static_cast<T*>(GetGenericPreviousLeafItem()); }

		T*							GetNextLeafItem() { return static_cast<T*>(GetGenericNextLeafItem()); }

	protected:

		// destructor is protected, use Delete

		virtual						~TreeModelItem() {}
};

/***********************************************************************************
**
**	TreeModel is an abstract template class from which you would want
**	to derive your own model classes
**
***********************************************************************************/

template<class T> class TreeModel : public GenericTreeModel
{
	public:

		virtual						~TreeModel() {}

		T*							GetItemByIndex(INT32 index){ return static_cast<T*>(GetGenericItemByIndex(index)); }

		T*							GetItemByID(INT32 id) { return static_cast<T*>(GetGenericItemByID(id)); }

		T*							GetItemByIDAndType(INT32 id, OpTypedObject::Type type) { return static_cast<T*>(GetGenericItemByIDAndType(id, type)); }

		T*							GetParentItem(INT32 index) { return static_cast<T*>(GetGenericParentItem(index)); }

		T*							GetSiblingItem(INT32 index) { return static_cast<T*>(GetGenericSiblingItem(index)); }

		T*							GetPreviousItem(INT32 index) { return static_cast<T*>(GetGenericPreviousItem(index)); }

		T*							GetChildItem(INT32 index) { return static_cast<T*>(GetGenericChildItem(index)); }
};

#endif // !ADJUNCT_DESKTOP_UTIL_TREEMODEL_OPTREEMODEL_H
