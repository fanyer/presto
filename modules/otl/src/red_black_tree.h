/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_REDBLACKTREE_H
#define MODULES_OTL_REDBLACKTREE_H

#include "modules/otl/src/red_black_tree_node.h"
#include "modules/otl/src/red_black_tree_iterator.h"
#include "modules/otl/src/reverse_iterator.h"
#include "modules/otl/src/const_iterator.h"
#include "modules/hardcore/base/opstatus.h"

/**
 * Default ordering comparator, calls operator < on objects.
 *
 * If objects of class T don't implement <, a special comparator may be used.
 */
template<typename T>
class Less
{
public:
	bool operator()(const T& a, const T& b) const { return a < b; }
};

/**
 * A generic implementation of a Red-Black Tree.
 *
 * May be used to implement associative containers or sets. Guarantees O(log(n))
 * time complexity for insertion, removal and search operations. Doesn't allow
 * duplicates.
 *
 * @tparam T Type of stored object. T must be copy-constructible. Doesn't have to
 * be assignable.
 * @tparam Comparator Comparison functor. Takes two arguments of @a T type and returns
 * a boolean: true if the first argument is to be placed at an earlier position than
 * the second in a strict weak ordering operation, false otherwise.
 * This defaults to the Less comparator which calls @a T's < operator.
 */
template<typename T, typename Comparator = Less<T> >
class OtlRedBlackTree
{
public:
	typedef OtlRedBlackTreeIterator<T> Iterator;
	typedef OtlConstIterator<Iterator> ConstIterator;
	typedef OtlReverseIterator<Iterator> ReverseIterator;
	typedef OtlReverseIterator<ConstIterator> ReverseConstIterator;
	typedef OtlRedBlackTreeNode<T> Node;

	/**
	 * Creates an empty OtlRedBlackTree.
	 *
	 * @param comparator The comparator functor to use for ordering elements.
	 */
	OtlRedBlackTree(Comparator comparator = Comparator());

	/**
	 * Destructor.
	 *
	 * Calls Clear().
	 */
	~OtlRedBlackTree();

	/**
	 * Inserts a new item or overwrites an existing one.
	 *
	 * If an equivalent item already exists in the tree, it will be overwritten
	 * by @a item. Otherwise, a new node with the value @a item will be inserted.
	 *
	 * @note Item 'a' is equivalent to item 'b' if (!comparator(a,b) &&
	 *       !comparator(b,a)), or in other words (! a < b && ! b < a).
	 *
	 * @param item Item to insert.
	 * @param insertionPositionOut Output argument: iterator to the inserted item.
	 *                             Pass NULL if you're not interested in this.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY if allocation of
	 *         the new node failed.
	 */
	OP_STATUS Insert(const T& item, Iterator* insertionPositionOut = NULL);

	/**
	 * Erase an item from the tree.
	 *
	 * Removes an item equivalent to @a item from the tree. The removed item's
	 * destructor is called and the tree may be reorganized. Erasing an item does
	 * not invalidate existing iterators except the iterator to the erased item.
	 *
	 * @param item Item to remove.
	 * @return Number of erased elements: 1 if the item was found and removed,
	 * 0 if it wasn't.
	 */
	size_t Erase(const T& item);

	/**
	 * Erase an iterator from the tree.
	 *
	 * Remove an item referenced by the iterator. @a item is invalid after this
	 * operation but other iterators remain valid.
	 *
	 * @param item Iterator referencing an item to remove.
	 */
	void Erase(Iterator item);

	/**
	 * Find item in tree.
	 *
	 * Return an iterator to an item equivalent to @a item.
	 *
	 * @param item Item to find.
	 * @return Iterator to an item that is equivalent to the specified argument
	 *         @a item or end iterator if it was not found.
	 * @see End()
	 */
	Iterator Find(const T& item);
	ConstIterator Find(const T& item) const
	{
		return const_cast<MyType*>(this)->Find(item);
	}

	/**
	 * Iterator to the leftmost element. Iterating backwards from this iterator
	 * is undefined.
	 *
	 * @return Iterator to the leftmost element.
	 */
	Iterator Begin();
	ConstIterator Begin() const
	{
		return const_cast<MyType*>(this)->Begin();
	}

	/**
	 * Iterator to an imaginary element just beyond the rightmost element. It
	 * is illegal to dereference this iterator. Iterating from this iterator is
	 * undefined
	 *
	 * @return Iterator to an imaginary element just beyond the rightmost element.
	 */
	Iterator End();
	ConstIterator End() const
	{
		return const_cast<MyType*>(this)->End();
	}

	/**
	 * Iterator to the rightmost element. Iterating forwards from this iterator
	 * is undefined.
	 *
	 * @return Iterator to the rightmost element.
	 */
	ReverseIterator RBegin();
	ReverseConstIterator RBegin() const
	{
		return const_cast<MyType*>(this)->RBegin();
	}

	/**
	 * Iterator to an imaginary element just beyond the leftmost element. It
	 * is illegal to dereference this iterator. Iterating from this iterator is
	 * undefined.
	 *
	 * @return Iterator to an imaginary element just beyond the leftmost element.
	 */
	ReverseIterator REnd();
	ReverseConstIterator REnd() const
	{
		return const_cast<MyType*>(this)->REnd();
	}

	/**
	 * Current number of elements.
	 *
	 * Returns the current number of elements in the tree. This is an O(1)
	 * operation as the running count is kept and updated during Insert, Erase
	 * and Clear.
	 *
	 * @return Size of the tree.
	 */
	size_t Size() const;

	/// @return true if there are no items in the tree, false otherwise.
	bool Empty() const;

	/**
	 * Erase all elements.
	 *
	 * Removes all elements in order, calling their destructors.
	 */
	void Clear();

	/**
	 * Count elements with a specific value.
	 *
	 * Since we don't allow duplicates, this will either return 0 if @a item is
	 * not in the tree or 1 if it is in the tree.
	 *
	 * @param item Item to search for.
	 * @return 1 if @a item is found, 0 otherwise.
	 */
	size_t Count(const T& item) const;

#if defined(RBTREE_DEBUG) || defined(SELFTEST)
	/**
	 * Checks if all red-black tree properties are met. These properties are:
	 * - Root node is black
	 * - Black depth is identical for all leaf nodes
	 * - No red node has a red parent or child
	 * - Iterator iterates over all nodes in correct order.
	 * If any of these properties is not met, an assertion will be triggered and
	 * the method will return false.
	 *
	 * @return true only if the tree is a valid red-black tree
	 */
	bool IsValidRedBlackTree();
	bool IsCorrectParent(Node* n);
#endif
private:
	typedef OtlRedBlackTree<T, Comparator> MyType;
	Node* InsertRecursive(Node* root,
	                      const T& item,
	                      Iterator& insertionPosition,
	                      OP_STATUS& status,
	                      bool& nodeAdded);
	void SetRoot(Node* newRoot);
	void UpdateShortcutsAfterInsert(Node* newNode);
	void UpdateShortcutsBeforeErase(Node* nodeToDelete);
	Node* SingleRotate(Node* n, int dir);
	Node* DoubleRotate(Node* n, int dir);

	/// Swap positions of node a and b in the tree.
	void Swap(Node* a, Node* b);

	/**
	 * Overwrite existing node's item with a new one.
	 *
	 * This will not change ordering. An entirely new node will be created
	 * and it will be swapped with @a n.
	 * @param n    Node to overwrite.
	 * @param item New item, equivalent to the old one.
	 * @return OpStatus::OK if everything went smoothly,
	 *         OpStatus::ERR_OUT_OF_MEMORY if the new node could not be
	 *         allocated.
	 */
	OP_STATUS Overwrite(Node*& n, const T& item);

	/// Root node in binary tree. NULL if tree is empty.
	Node* root;
	Node* leftmost;
	Node* rightmost;

	/// Number of nodes in binary tree.
	size_t count;

	Comparator comparator;
};

// Implementation
#define RBT OtlRedBlackTree<T, Comparator>


template<typename T, typename Comparator>
RBT::OtlRedBlackTree(Comparator comparator)
	: root(NULL)
	, leftmost(NULL)
	, rightmost(NULL)
	, count(0)
	, comparator(comparator)
{
}

template<typename T, typename Comparator>
RBT::~OtlRedBlackTree()
{
	Clear();
}

template<typename T, typename Comparator>
typename RBT::Iterator RBT::Find(const T& item)
{
	Node* n = root;
	while (n)
	{
		if (comparator(item, n->item))
			n = n->Child(0);
		else if (comparator(n->item, item))
			n = n->Child(1);
		else
			return Iterator(n); // Found.
	}

	// Not found.
	return End();
}

template<typename T, typename Comparator>
void RBT::SetRoot(Node* newRoot)
{
	root = newRoot;
	if (root)
	{
		root->SetBlack();
		root->SetParent(NULL);
	}
}

template<typename T, typename Comparator>
void RBT::UpdateShortcutsAfterInsert(Node* newNode)
{
	if (leftmost && rightmost)
	{
		if (comparator(newNode->item, leftmost->item))
			leftmost = newNode;
		if (comparator(rightmost->item, newNode->item))
			rightmost = newNode;
	}
	else
		leftmost = rightmost = newNode;
}

template<typename T, typename Comparator>
void RBT::UpdateShortcutsBeforeErase(Node* nodeToDelete)
{
	if (!nodeToDelete) return;
	if (leftmost == nodeToDelete)
		leftmost = nodeToDelete->Next();
	if (rightmost == nodeToDelete)
		rightmost = nodeToDelete->Previous();
}

template<typename T, typename Comparator>
typename RBT::Node* RBT::SingleRotate(Node* n, int dir)
{
	OP_ASSERT(dir == 0 || dir == 1);
	Node* x = n->Child(!dir);
	OP_ASSERT(x);

	if (n->parent)
		n->parent->SetChild(x, n->parent->Child(1) == n);
	else
		x->parent = 0;

	n->SetChild(x->Child(dir), !dir);
	x->SetChild(n, dir);

	n->SetRed();
	x->SetBlack();
	return x;
}

template<typename T, typename Comparator>
typename RBT::Node* RBT::DoubleRotate(Node* n, int dir)
{
	n->SetChild(SingleRotate(n->Child(!dir), !dir), !dir);
	return SingleRotate(n, dir);
}

template<typename T, typename Comparator>
void RBT::Swap(Node* a, Node* b)
{
	if (a == b) return;

	// Swap parents.
	Node* temp = a->parent;
	a->parent = b->parent;
	b->parent = temp;

	// Update parent's child pointers.
	if (a->parent)
		a->parent->SetChild(a, a->parent->Child(0) != b);

	if (b->parent)
		b->parent->SetChild(b, b->parent->Child(0) != a);

	// Swap children.
	for (int i = 0; i < 2; i++)
	{
		temp = a->Child(i);
		a->SetChild(b->Child(i), i);
		b->SetChild(temp, i);
	}

	bool red = a->IsRed();
	if (b->IsRed()) a->SetRed();
	else a->SetBlack();
	if (red) b->SetRed();
	else b->SetBlack();
}

template<typename T, typename Comparator>
OP_STATUS RBT::Overwrite(Node*& node, const T& item)
{
	// Assert this won't change ordering.
	OP_ASSERT(!comparator(item, node->item) && !comparator(node->item, item));

	/* Note, if type T was always assignable, we could write an easier and
	more effective implementation that boils down to node->item = item.
	This, however, precludes us from supporting const T or T's with const
	members, because you can't have assignment to a const variable.
	Instead, we create a new node (T's copy constructor is called) and
	replace the existing one. */
	Node* replacement = OP_NEW(Node, (item));
	RETURN_OOM_IF_NULL(replacement);
	replacement->color = node->color;
	Swap(node, replacement);
	if (node == leftmost) leftmost = replacement;
	if (node == rightmost) rightmost = replacement;
	if (node == root) root = replacement;
	OP_DELETE(node);
	node = replacement;
	return OpStatus::OK;
}

template<typename T, typename Comparator>
typename RBT::Node* RBT::InsertRecursive(Node* node,
										 const T& item,
										 Iterator& insertionPosition,
										 OP_STATUS& status,
										 bool& nodeAdded)
{
	if (!node)
	{
		// Reached a leaf in the tree. insert new node here.
		node = OP_NEW(Node, (item));
		if (!node)
		{
			status = OpStatus::ERR_NO_MEMORY;
			return node;
		}
		insertionPosition = Iterator(node);
		nodeAdded = true; // Rebalance on the way up.
		count++;
		UpdateShortcutsAfterInsert(node);
	}
	else
	{
		if (!comparator(node->item, item) &&
			!comparator(item, node->item))
		{
			/* Equivalent item already exists here, overwrite it.
			Overwriting "equal" items may seem pointless but it makes sense
			when using custom comparators and it's a must-have for implementing
			maps over RBT.*/
			status = Overwrite(node, item);
			insertionPosition = Iterator(node);
			nodeAdded = false; // No need to recolor/rebalance.
			return node;
		}

		int dir = comparator(node->item, item);
		node->SetChild(InsertRecursive(node->Child(dir), item, insertionPosition, status, nodeAdded), dir);

		/* When no node is added, just an existing one is updated, there's no need
		to rebalance. Return immediately.*/
		if (!nodeAdded)
			return node;

		// Rebalancing/recoloring.
		if (Node::IsRed(node->Child(dir)))
		{
			if (Node::IsRed(node->Child(!dir)))
			{
				// This is known as "Case 1" in literature.
				node->SetRed();
				node->Child(dir)->SetBlack();
				node->Child(!dir)->SetBlack();
			}
			else
			{
				if (Node::IsRed(node->Child(dir)->Child(dir))) // "Case 2".
					node = SingleRotate(node, !dir);
				else if (Node::IsRed(node->Child(dir)->Child(!dir))) // "Case 3".
					node = DoubleRotate(node, !dir);
			}
		}
	}
	return node;
}

template<typename T, typename Comparator>
OP_STATUS RBT::Insert(const T& item, Iterator* insertionPositionOut)
{
	Iterator insertionPosition = End();
	OP_STATUS status = OpStatus::OK;
	bool nodeAdded = false;
	SetRoot(InsertRecursive(root, item, insertionPosition, status, nodeAdded));
	if(insertionPositionOut)
		*insertionPositionOut = insertionPosition;
	return status;
}

/*
 * This is a top-down removal approach, by basically rebalancing the tree
 * as we are searching our way down to find the node we want to remove.
 *
 * This algorithm ensures the node we are deleting is a red leaf node.
 */
template<typename T, typename Comparator>
size_t RBT::Erase(const T& item)
{
	Node* iterator = root;
	Node* needle = NULL;
	int dir = 1; // direction (0 = left, 1 = right)
	int last = -1;

	/*
	Walk down the tree to find the element with the specified key, if found
	assign it to needle. After it was found continue to walk the tree until we
	reach the leaf node which is the closest child of the needle, i.e. either
	the smallest child in the right sub-tree or the largest child in the left
	sub-tree. On walking down the tree we rotate the tree and change the node's
	colours such that the path from the root to the leaf has no two consecutive
	black elements, the leaf is red and at most the 2nd properties of the
	black-tree (root being black) may be violated. Thus we may swap the needle
	with the leaf and remove the leaf (which is red) without violating the
	black-tree properties.
	*/
	while (iterator)
	{
		// Find direction for walking down the tree.
		if (comparator(iterator->item, item))
			dir = 1;
		else if (comparator(item, iterator->item))
			dir = 0;
		else
			needle = iterator; // Found it! But we'll continue deeper towards a leaf.

		if (!Node::IsRed(iterator) && !Node::IsRed(iterator->Child(dir)))
		{
			if (Node::IsRed(iterator->Child(!dir)))
			{
				Node* topOfRotatedSubtree = SingleRotate(iterator, dir);
				if (topOfRotatedSubtree->IsRoot())
					// Rotation has invalidated the root pointer.
					SetRoot(topOfRotatedSubtree);
			}
			else
			{
				Node* parent = iterator->Parent();
				Node* sibling = iterator->Sibling();
				if (sibling)
				{
					if (sibling->HasNoRedChildren())
					{
						parent->SetBlack();
						sibling->SetRed();
						iterator->SetRed();
					}
					else
					{
						Node* topOfRotatedSubtree = NULL;
						if (Node::IsRed(sibling->Child(last)))
							parent->SetChild(SingleRotate(parent->Child(!last), !last), !last);
						topOfRotatedSubtree = SingleRotate(parent, last);

						topOfRotatedSubtree->SetRed();
						topOfRotatedSubtree->Child(0)->SetBlack();
						topOfRotatedSubtree->Child(1)->SetBlack();

						if (topOfRotatedSubtree->IsRoot())
							// Rotation has invalidated the root pointer.
							SetRoot(topOfRotatedSubtree);
						iterator->SetRed();

					}
				}
			}
		}

		if (!iterator->IsLeaf())
		{
			// Go deeper.
			last = dir;
			iterator = iterator->Child(dir);
		}
		else break;
	}

	if (!needle)
		return 0; // Item wasn't found!

	OP_ASSERT(iterator->IsLeaf());

	UpdateShortcutsBeforeErase(needle);

	/* Swap the found node (needle) with a leaf that will replace it (iterator).
	This is not needed when the found node IS that leaf. */
	if (needle != iterator)
	{
		iterator->color = needle->color;
		Swap(needle, iterator);
		if (root == needle)
			root = iterator;
	}

	OP_DELETE(needle);

	count--;

	if (!count)
		root = NULL;

#ifdef RBTREE_DEBUG
	OP_ASSERT(IsValidRedBlackTree());
#endif
	return 1;
}

template<typename T, typename Comparator>
void RBT::Erase(Iterator iter)
{
	if (iter == End() || !root)
		return;

	Erase(*iter);
}

template<typename T, typename Comparator>
void RBT::Clear()
{
	if (root)
	{
		root->DeleteChildren();
		OP_DELETE(root);
		root = NULL;
	}
	leftmost = NULL;
	rightmost = NULL;
	count = 0;
}

template<typename T, typename Comparator>
size_t RBT::Size() const
{
	return count;
}

template<typename T, typename Comparator>
bool RBT::Empty() const
{
	return root == NULL;
}

template<typename T, typename Comparator>
typename RBT::Iterator RBT::Begin()
{
	return Iterator(leftmost);
}

template<typename T, typename Comparator>
typename RBT::Iterator RBT::End()
{
	return Iterator(NULL);
}

template<typename T, typename Comparator>
typename RBT::ReverseIterator RBT::RBegin()
{
	return ReverseIterator(Iterator(rightmost));
}

template<typename T, typename Comparator>
typename RBT::ReverseIterator RBT::REnd()
{
	return ReverseIterator(Iterator(NULL));
}

template<typename T, typename Comparator>
size_t RBT::Count(const T& item) const
{
	return Find(item) != End();
}

#if defined(RBTREE_DEBUG) || defined(SELFTEST)

template<typename T, typename Comparator>
bool RBT::IsCorrectParent(Node* n)
{
	bool ok = true;
	for(int dir = 0; dir <= 1; dir++)
	{
		if (n->Child(dir))
		{
			bool thisParentOk = n->Child(dir)->parent == n;
			OP_ASSERT(thisParentOk);
			if (!thisParentOk) return false;
			ok = ok && IsCorrectParent(n->Child(dir));
		}
	}

	return ok;
}

template<class T, class Comparator>
bool RBT::IsValidRedBlackTree()
{
	bool ok = true;
	if (root)
	{
		if (!root->IsBlack())
		{
			OP_ASSERT(!"Root node is not black.");
			ok = false; // Not returning just yet, maybe we'll find more problems.
		}

		if (!root->IsRoot())
		{
			OP_ASSERT(!"Root node has a parent.");
			ok = false;
		}

		if (!IsCorrectParent(root))
		{
			OP_ASSERT(!"A child has a different parent than its real parent.");
			ok = false;
		}

		if (leftmost != root->Furthest(0))
		{
			OP_ASSERT(!"Leftmost shortcut is not correct.");
			ok = false;
		}

		if (rightmost != root->Furthest(1))
		{
			OP_ASSERT(!"Rightmost shortcut is not correct.");
			ok = false;
		}
		size_t nodes = 0;
		size_t black_depth = 0;
		size_t leaves = 0;

		// Ensure all leaf nodes have the same "black depth".
		Node* p = NULL;
		for (Iterator it = Begin(); it != End() && nodes < count; ++it)
		{
			Node* n = it.nodePtr();
			if (!n)
			{
				OP_ASSERT(!"Node is null.");
				ok = false;
			}

			if (nodes > 0)
			{
				if (!(comparator(p->item, n->item)))
				{
					OP_ASSERT(!"Nodes are not sorted correctly.");
					ok = false;
				}
			}

			if (n->IsLeaf())
			{
				if (n->FindDoubleRed())
				{
					OP_ASSERT(!"A red node has a red parent/child.");
					ok = false;
				}

				size_t depth = n->BlackDepth();
				if (leaves > 0)
				{
					if (depth != black_depth)
					{
						OP_ASSERT(!"Black depth property is not consistent.");
						ok = false;
					}
				}
				else
				{
					black_depth = depth;
				}
				leaves++;
			}
			p = n;
			nodes++;
		}

		if (nodes != count)
		{
			OP_ASSERT(!"The number of nodes in tree is different from number of nodes reached by iterator.");
			ok = false;
		}
	}
	else
	{
		if (leftmost || rightmost || count)
		{
			OP_ASSERT(!"Tree is not empty, despite having no root.");
			ok = false;
		}
	}
	return ok;
}
#endif // defined(RBTREE_DEBUG) || defined(SELFTEST)


#endif // MODULES_OTL_REDBLACKTREE_H
