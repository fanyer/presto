/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_ADT_OPTREE_H
#define MODULES_UTIL_ADT_OPTREE_H

#include "modules/util/adt/opvector.h"

/***********************************************************************************
**
**	OpGenericTree
**
***********************************************************************************/

typedef int (*compare_function_t)(const void*, const void*);

class OpGenericTree
{
	protected:

						OpGenericTree();
						~OpGenericTree();

		void			SetAllocationStepSize(UINT32 step) {m_tree_nodes.SetAllocationStepSize(step);}

		OP_STATUS		AddFirst(INT32 parent_index, void* item, INT32* got_index = NULL);
		OP_STATUS		AddLast(INT32 parent_index, void* item, INT32* got_index = NULL);

		OP_STATUS		InsertBefore(INT32 sibling_index, void* item, INT32* got_index = NULL);
		OP_STATUS		InsertAfter(INT32 sibling_index, void* item, INT32* got_index = NULL);

		OP_STATUS		Sort(compare_function_t compare_function);

		void*			Remove(INT32 index, BOOL all_children = TRUE);
		void			Clear();

		INT32			Find(void* item) const;

		void*			Get(INT32 index) const;
		INT32			GetParent(INT32 index) const;
		INT32			GetChild(INT32 index) const;
		INT32			GetSibling(INT32 index) const;

		INT32			GetCount() const { return m_tree_nodes.GetCount(); }
		INT32			GetSubtreeSize(INT32 index) const;

	private:

		struct TreeNode
		{
			void*			m_item;
			INT32			m_parent_index;
			INT32			m_sub_tree_size;
		};

		OP_STATUS		Insert(INT32 parent_index, INT32 index, void* item);
		void			UpdateNodes(INT32 index_changed_from, INT32 index, INT32 parent_index, INT32 count);
		void			SortChildren(compare_function_t compare_function, INT32 old_parent_index, INT32 new_parent_index, INT32* quicksort_array, INT32* parent_array, TreeNode** result_array, INT32& current_quicksort_array_pos, INT32& current_result_array_pos);

		OpVector<TreeNode>	m_tree_nodes;
		INT32				m_total_sub_tree_sizes;		// used to detect and optimize for flat tree

		OpGenericTree(const OpGenericTree&);
		OpGenericTree& operator=(const OpGenericTree&);
};

/***********************************************************************************
**
**	OpTree
**
***********************************************************************************/

template<class T> class OpTree : private OpGenericTree
{
	public:

		/**
		 * Creates a tree
		 */
		OpTree() {};

		/**
		 * Sets the allocation step size for the internal OpVector used
		 */
		void SetAllocationStepSize(UINT32 step) { OpGenericTree::SetAllocationStepSize(step); }

		/**
		 * Adds an item of type T as first child of parent_index, where -1 means root level
		 */
		OP_STATUS AddFirst(INT32 parent_index, T* item, INT32* got_index = NULL) { return OpGenericTree::AddFirst(parent_index, item, got_index); }

		/**
		 * Adds an item of type T as last child of parent_index, where -1 means root level
		 */
		OP_STATUS AddLast(INT32 parent_index, T* item, INT32* got_index = NULL) { return OpGenericTree::AddLast(parent_index, item, got_index); }

		/**
		 * Insert an item of type T before sibling_index
		 */
		OP_STATUS InsertBefore(INT32 sibling_index, T* item, INT32* got_index = NULL) { return OpGenericTree::InsertBefore(sibling_index, item, got_index); }

		/**
		 * Insert an item of type T after sibling_index
		 */
		OP_STATUS InsertAfter(INT32 sibling_index, T* item, INT32* got_index = NULL) { return OpGenericTree::InsertAfter(sibling_index, item, got_index); }

		/**
		 * Quick sort items with given compare function
		 */

		void Sort(compare_function_t compare_function) { OpGenericTree::Sort(compare_function); }

		/**
		 * Removes the item at index and returns the pointer to the item.
		 */
		T* Remove(INT32 index, BOOL all_children = TRUE) { return static_cast<T*>(OpGenericTree::Remove(index, all_children)); }

		/**
		 * Clear tree, returning the tree to an empty state
		 */
		void Clear() { OpGenericTree::Clear(); }

		/**
		 * Remove AND Delete (call actual delete operator) index item (and all subitems). If index = -1, delete whole tree
		 */

		void Delete(INT32 index = -1, BOOL all_children = TRUE)
		{
			INT32 count;
			INT32 start;

			if (index == -1)
			{
				count = GetCount();
				start = 0;
			}
			else
			{
				count = all_children ? GetSubtreeSize(index) + 1 : 1;
				start = index;
			}
	
			// delete items

			for (INT32 i = 0; i < count; i++)
			{
				OP_DELETE(Get(start + i));
			}

			// clear tree nodes

			if (index == -1)
			{
				Clear();
			}
			else
			{
				Remove(index, all_children);
			}
		}

		/**
		 * Finds the index of a given item
		 */
		INT32 Find(T* item) const { return OpGenericTree::Find(item); }

		/**
		 * Returns the item at index.
		 */
		T* Get(INT32 index) const { return static_cast<T*>(OpGenericTree::Get(index)); }

		/**
		 * Returns the index of the parent of index item, or -1 if at root level.
		 */
		INT32 GetParent(INT32 index) const { return OpGenericTree::GetParent(index); }

		/**
		 * Returns the index of the first child of index item, or -1 if no children.
		 */
		INT32 GetChild(INT32 index) const { return OpGenericTree::GetChild(index); }

		/**
		 * Returns the index of the next sibling of index item, or -1 if no more siblings.
		 */
		INT32 GetSibling(INT32 index) const { return OpGenericTree::GetSibling(index); }

		/**
		 * Returns the number of items in the tree.
		 */
		INT32 GetCount() const { return OpGenericTree::GetCount(); }

		/**
		 * Returns the number of items one or more levels below an item. (ie. sum of children and grandchildren)
		 */
		INT32 GetSubtreeSize(INT32 index) const { return OpGenericTree::GetSubtreeSize(index); }

	private:

		OpTree(const OpTree&);
		OpTree& operator=(const OpTree&);
};

// ---------------------------------------------------------------------------------

#endif // !MODULES_UTIL_ADT_OPTREE_H

// ---------------------------------------------------------------------------------
