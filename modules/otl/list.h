/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_LIST_H
#define MODULES_OTL_LIST_H

#include "modules/otl/src/const_iterator.h"
#include "modules/otl/src/reverse_iterator.h"
#include "modules/otl/range.h"

/**
 * @file
 *
 * @brief OtlList - Templated doubly-linked list container.
 */

/**
 * Collection of all methods from OtlList<T> which don't require the template.
 */
class OtlListBase
{
protected:
	/**
	 * A single node in a list. OtlList<T> extends this base by adding the item
	 * of type T to its members.
	 */
	class NodeBase
	{
	private:
		NodeBase* m_prev;
		NodeBase* m_next;

		/**
		 * An instance of this class is reference counted.
		 *
		 * A node can be referenced in an OtlList or in an iterator (Iterator or
		 * ConstIterator). A node cannot be used by distinct OtlList instances,
		 * but it may be used by several iterators. Thus we can use the
		 * reference counter to detect if the node is inside a list. On
		 * inserting an item into an OtlList, a node is created and the reference
		 * counter is increased by 1 (on calling Use(true)). On referencing a
		 * node in an iterator the counter is increased by 2 (on calling
		 * Use(false)). When the node is removed from the list, the reference
		 * counter is decreased by 1 (on calling Release(true)). When an
		 * iterator no longer references a node, the reference counter is
		 * decreased by 2 (on calling Release(false)). So if the reference
		 * counter is odd, the node is inside a list.
		 *
		 * This allows us to keep a node that is still referenced by an iterator
		 * inside a list as an "invisible" node. All operations will jump
		 * over such an "invisible" node. The node is finally removed when there
		 * is no iterator referencing the node. That allows the code to remove
		 * the current node from the list without influencing the iterator,
		 * i.e. after the node is removed, the iterator will advance to the
		 * correct previous or next element. And any other iterator will jump
		 * over the invisible node.
		 *
		 * The variable is mutable to allow the reference counter to be used
		 * by the ConstIterator on a const OtlList.
		 */
		mutable size_t m_ref_count;

		/// Copy constructor. Forbidden because of possible OOM.
		NodeBase(const NodeBase& other);

		/// Assignment operator. Forbidden because of possible OOM.
		const NodeBase& operator=(const NodeBase& other);

#ifdef DEBUG_ENABLE_OPASSERT
	private:
		/**
		 * If OP_ASSERT is enabled, we want to be able to assert that two
		 * different nodes are in the same tree. So we store the pointer to the
		 * list's head in each node on creating a new node. The method
		 * IsInSameList() is provided to test that the heads of two nodes are
		 * equal. That can be used as OP_ASSERT(node->IsInSameList(other));
		 */
		const NodeBase* const m_head;

	public:
		/**
		 * Returns true if this node is in the same list as the specified other
		 * node. This method can be used to assert that two nodes are in the
		 * same list:
		 *
		 * @code
		 * Node* node = ...;
		 * Node* other = ...;
		 * OP_ASSERT(node->IsInSameList(other));
		 * @endcode
		 */
		bool IsInSameList(const NodeBase* other) const
		{
			return other && m_head == other->m_head;
		}
#endif // DEBUG_ENABLE_OPASSERT

	protected:
		/**
		 * This method shall return a pointer to the node's item.
		 *
		 * The default implementation that is used for the list's head returns
		 * NULL. Thus we ensure a crash (instead of accessing some
		 * uninitialised or wrong data) when an iterator pointing to the head
		 * is dereferenced.
		 */
		virtual const void* ItemPtr() const
		{
			OP_ASSERT(!"Don't dereference the End() iterator"); return NULL;
		}

		virtual void* ItemPtr()
		{
			OP_ASSERT(!"Don't dereference the End() iterator"); return NULL;
		}

	public:
#ifdef DEBUG_ENABLE_OPASSERT
		NodeBase(const NodeBase* head)
			: m_prev(this)
			, m_next(this)
			, m_ref_count(0)
			, m_head(head ? head : this)
		{}
#else // !DEBUG_ENABLE_OPASSERT
		NodeBase()
			: m_prev(this)
			, m_next(this)
			, m_ref_count(0)
		{}
#endif // DEBUG_ENABLE_OPASSERT

		/// The node destructor removes the node from its list.
		virtual ~NodeBase(void)
		{
			OP_ASSERT(m_ref_count == 0);
			OP_ASSERT(m_prev && m_next);
			if (m_prev != this)
			{
				m_prev->m_next = m_next;
				m_next->m_prev = m_prev;
			}
		}

		/// Returns true if this Node is Use()d inside an OtlList instance.
		bool IsInList() const { return (m_ref_count % 2 == 1); }

		/**
		 * Must be called when this instance is referenced by some class.
		 *
		 * This increases the reference counter. When the node is no longer
		 * needed, call Release() with the same value for @c list.
		 *
		 * @param list Must be true if the caller is an OtlList instance and
		 *             false if it is an iterator. At most one OtlList instance
		 *             may reference any given node at any given time.
		 */
		void Use(bool list) const
		{
			OP_ASSERT(!list || !IsInList());
			m_ref_count += (list ? 1 : 2);
			OP_ASSERT(m_ref_count > 0);
		}

		/**
		 * If a node is no longer used, this method must be called.
		 *
		 * This decreases the node's reference counter and possibly deletes this
		 * instance. The caller may not dereference this instance after
		 * releasing it. The caller must have called Use() before calling this
		 * method with the same value for @c list.
		 *
		 * @param list Must be true if the caller is an OtlList instance and
		 *        false if it is an iterator. At most one OtlList instance may
		 *        reference any given node at any given time.
		 */
		void Release(bool list) const
		{
			const size_t delta = list ? 1 : 2;
			OP_ASSERT(m_ref_count >= delta);
			m_ref_count -= delta;
			if (m_ref_count == 0)
				OP_DELETE(this);
		}

		/**
		 * @param direction Go forward if true, backwards if false.
		 * @return the next node in the specified direction.
		 */
		NodeBase* Next(bool direction) const
		{
			return direction ? m_next : m_prev;
		}

		/**
		 * Returns the next node which is in the list.
		 *
		 * I.e. for which IsInList() returns true. If there is no other node,
		 * this node may be returned.
		 *
		 * @note If none of the nodes in this chain belong to a list
		 *       (i.e. IsInList() is false for all nodes), then the next node
		 *       in the chain is returned. This may be useful if two iterators
		 *       (or more) survive the destruction of a list. This ensures that
		 *       the iterator is moved on. <em>Example:</em>
		 *       @code
		 *         OtlList<int>* list = ...;
		 *         // Get some iterators from the list:
		 *         OtlList<int>::Iterator itr = list->Begin();
		 *         OtlList<int>::ConstIterator end = list->End();
		 *         // Iterate over the elements in the list:
		 *         for (; itr != end; ++itr)
		 *         {
		 *                  if (*itr == 45) // Delete the list:
		 *                 OP_DELETE(list);
		 *             else
		 *                 printf("at element %d\n", *itr);
		 *         }
		 *       @endcode
		 *       In this example we iterate over a @c list of integers. If the
		 *       @c list contains the int 45, the @c list is deleted. Then both
		 *       @c itr and @c end point to some Node which is no longer in the
		 *       list. The next step advances @c itr to the @c end and the loop
		 *       finishes.
		 *
		 * @param direction Go forward if true, backwards if false.
		 */
		NodeBase* NextInList(bool direction) const
		{
			OP_ASSERT(m_prev && m_next);
			NodeBase* next = Next(direction);
			OP_ASSERT(next->m_next && next->m_prev);
			while (next != this && !next->IsInList())
			{
				next = next->Next(direction);
				OP_ASSERT(next->m_next && next->m_prev);
			}
			if (this == next && !IsInList())
				return next->Next(direction);
			else
				return next;
		}

		/**
		 * Insert this node after the specified @c other node.
		 *
		 * @note This node must not be in any other list when calling this
		 *       method.
		 *
		 * @param other points to the node after which this node will be
		 *              inserted.
		 */
		void InsertAfter(const NodeBase* other)
		{
			OP_ASSERT(IsInSameList(other));
			OP_ASSERT((this == m_next) && (this == m_prev) ||
					  !"This node must not be in some list!");
			m_next = other->Next(true);
			OP_ASSERT(other == m_next->m_prev);
			m_prev = m_next->m_prev;
			m_prev->m_next = this;
			m_next->m_prev = this;
			OP_ASSERT(m_prev == other);
		}
	};

private:
	/// Copy constructor. Forbidden because of possible OOM.
	OtlListBase(const OtlListBase& other);

	/// Assignment operator. Forbidden because of possible OOM.
	const OtlListBase& operator=(const OtlListBase& other);

	/**
	 * Head of the list.
	 *
	 * The first (possibly invisible) Node of the list is m_head->Next(true).
	 * The last (possibly invisible) Node of the list is m_head->Next(false).
	 * The first visible Node is m_head->NextInList(). If the list is empty,
	 * the first and last Node of the list point to m_head.
	 */
	NodeBase* const m_head;

protected:
	/**
	 * Constructor.
	 *
	 * @note The constructor allocates memory for its head node
	 *       (m_head). Allocating that memory may fail. But the failure to
	 *       allocate the memory is not reported back to the caller until the
	 *       caller inserts the first element into the list. Thus inserting an
	 *       element may fail even if, between constructing the list and
	 *       inserting the element, enough memory was freed.
	 *       If the constructor fails to allocate the head node, the list is a
	 *       valid empty list, i.e. all methods behave like on any other empty
	 *       list, but it will refuse to insert any elements.
	 */
	OtlListBase()
#ifdef DEBUG_ENABLE_OPASSERT
		: m_head(OP_NEW(NodeBase, (NULL)))
#else // !DEBUG_ENABLE_OPASSERT
		: m_head(OP_NEW(NodeBase, ()))
#endif // DEBUG_ENABLE_OPASSERT
	{
		if (m_head)
			m_head->Use(true);
	}

	~OtlListBase(void) { if (m_head) m_head->Release(true); }

	/// Returns the pointer to the head.
	NodeBase* Head() const { return m_head; }

	/**
	 * Returns a pointer to the first resp. last Node in the list or a pointer
	 * to the m_head if the list is empty.
	 *
	 * @param direction Returns the first Node if true and the last Node if
	 *                  false.
	 */
	const NodeBase* StartNode(bool direction) const
	{
		return m_head ? m_head->NextInList(direction) : NULL;
	}

	NodeBase* StartNode(bool direction)
	{
		return m_head ? m_head->NextInList(direction) : NULL;
	}

public:
	/// @return true if this list is empty.
	op_force_inline bool IsEmpty(void) const { return StartNode(true) == Head(); }
}; // OtlListBase

/**
 * Simple list of templated instances.
 *
 * This class is similar to the List<T> class in modules/util/simset.h with the
 * exception that OtlList maintains its own copy of each list item. Hence an
 * item can belong to several lists, and does not need to inherit from
 * ListElement<T>. (In other words, this class separates the list element from
 * the item itself).
 *
 * Since OtlList maintains a separate copy of each list element, you should think
 * twice before storing very large objects or objects whose copy-constructor is
 * inefficient (e.g. copy-constructors that perform deep-copying). Instead,
 * consider allocating such objects on the heap, and store pointers to them
 * inside OtlList. (Note that OtlList will \e not automatically deallocate such
 * pointers when they are removed from a list).
 *
 * The OtlList provides bi-directional iterators to iterate over all elements in
 * the list, either from the first element to the last:
 * @code
 *  OtlList<int> list;
 *  OtlList<int>::ConstIterator it;
 *  for (it = list.Begin(); it != list.End(); ++it) ...;
 * @endcode
 * Or from the last element to the first:
 * @code
 *  OtlList<int> list;
 *  OtlList<int>::ReverseConstIterator it;
 *  for (it = list.RBegin(); it != list.REnd(); ++it) ...;
 * @endcode
 *
 * The OtlList keeps a circular list of elements with a head node which connects
 * the front with the end of the list. The #ConstIterator returned by End()
 * points to this head and the #ReverseConstIterator returned by REnd() points
 * as well to the same head.
 *
 * Because of this design you can move an iterator that equals End() to the last
 * element in the list by using the operator--():
 * @code
 *  OtlList<int> list = ...;
 *  OtlList<int>::ConstIterator itr = list.End(); // itr points to the head
 *  --itr; // itr points now to the last element of the list.
 * @endcode
 * And you can move an iterator that equals End() to the first element in the
 * list by using the operator++():
 * @code
 *  OtlList<int> list = ...;
 *  OtlList<int>::ConstIterator itr = list.End(); // itr points to the head
 *  ++itr; // itr points now to the first element of the list.
 * @endcode
 *
 * This design has the consequences, that you can specify the End() iterator as
 * argument to the methods InsertAfter() and InsertBefore(). Inserting an
 * element before the End() inserts it at the end of the list. Inserting an
 * element after the End(), inserts it at the front of the list. So calling
 * <code>InsertBefore(item, End())</code> is equivalent to
 * <code>Append(item)</code>. And calling <code>InsertAfter(item, End())</code>
 * is equivalent to <code>Prepend(item)</code>.
 *
 * The reverse iterators #ReverseConstIterator and #ReverseIterator move in the
 * opposite direction as the normal iterators. Calling InsertAfter() or
 * InsertBefore() on a reverse iterator respects the direction of the iterator.
 */
template<class T>
class OtlList : public OtlListBase
{
private:

	/// A single node in an OtlList.
	class Node : public OtlListBase::NodeBase
	{
		/**
		 * OtlList is declared as friend to be able to access the Node's
		 * members m_prev and m_next.
		 */
		friend class OtlList;
	private:
		T m_item;

	protected:
		virtual const void* ItemPtr() const { return &m_item; }
		virtual void* ItemPtr() { return &m_item; }

	public:
#ifdef DEBUG_ENABLE_OPASSERT
		Node(const NodeBase* head, const T& i)
			: NodeBase(head)
			, m_item(i)
		{}
#else // !DEBUG_ENABLE_OPASSERT
		explicit Node(const T& i) : m_item(i) {}
#endif // DEBUG_ENABLE_OPASSERT
		virtual ~Node() {}

		Node* NextInList(bool direction) const
			{ return static_cast<Node*>(NodeBase::NextInList(direction)); }

		/** @return the item of the Node. */
		op_force_inline const T& Item() const
		{
			return *(reinterpret_cast<const T*>(ItemPtr()));
		}

		op_force_inline T& Item()
		{
			return *(reinterpret_cast<T*>(ItemPtr()));
		}
	};

public:
	class Iterator;

	/**
	 * Declare Iterator and ConstIterator as friend to allow them access to the
	 * private class Node.
	 * @{
	 */
	friend class Iterator;
	/* @} */

protected:
	/// Returns the pointer to the head.
	Node* Head() const { return static_cast<Node*>(OtlListBase::Head()); }

	const Node* StartNode(bool direction) const
	{
		return static_cast<const Node*>(OtlListBase::StartNode(direction));
	}

	Node* StartNode(bool direction)
	{
		return static_cast<Node*>(OtlListBase::StartNode(direction));
	}

#ifdef DEBUG_ENABLE_OPASSERT
	/**
	 * Returns true if the specified @c node is in this list.
	 *
	 * This method can be used in an OP_ASSERT():
	 * @code
	 * Node* node = ...;
	 * OpListBase list = ...;
	 * OP_ASSERT(list.ContainsNode(other));
	 * @endcode
	 */
	bool ContainsNode(const NodeBase* node) const
	{
		return node && node->IsInSameList(Head());
	}

#endif // DEBUG_ENABLE_OPASSERT

public:
	/**
	 * The Iterator can be used to iterate over all elements in a list.
	 *
	 * An iterator either points to a single element in the list or past the
	 * last element in the list. To test if the iterator is past the last
	 * element in the list, compare it with OtlList::End() or use
	 * OtlList::IsAtEnd().
	 *
	 * An iterator that is not the end provides access to the methods and
	 * members of the element as defined by the template type @c T and can thus
	 * change the element.
	 *
	 * Example:
	 *
	 * @code
	 * OtlList<int> list;
	 * ...
	 * for (OtlList<int>::Iterator itr = list.Begin();
	 *      itr != list.End(); ++itr)
	 *     std::cout << "Value: " << *itr << std::endl;
	 * @endcode
	 */
	class Iterator
	{
	private:
		/// The current list node.
		Node* m_node;

		/**
		 * The ConstIterator is declared as friend to allow its comparison
		 * operator to access the m_node attribute of this class.
		 */
		friend class ConstIterator;

		/**
		 * The OtlList is declared as friend to allow the InsertAfter() method
		 * to access its node via GetNode().
		 */
		friend class OtlList;

		/// Returns the current list node.
		Node* GetNode() const { return m_node; }

		/**
		 * Sets the current node pointer to the specified value.
		 *
		 * The old current node is released (see Node::Release()) and the new
		 * current node is used (see Node::Use()).
		 */
		void SetNode(Node* node)
		{
			if (m_node == node) return;
			if (m_node) m_node->Release(false);
			m_node = node;
			if (m_node) m_node->Use(false);
		}

		/** Sets this iterator to the next node in the specified direction.
		 *
		 * @param direction Go forward if true, backwards if false.
		 */
		void Next(bool direction)
		{
			if (m_node) SetNode(m_node->NextInList(direction));
		}

	public:
		typedef T& Reference;
		typedef const T& ConstReference;
		typedef T* Pointer;
		typedef const T* ConstPointer;

		/**
		 * The default constructor creates an Iterator which does not point to
		 * anything (not even to the end of a list).
		 */
		Iterator() : m_node(NULL) {}
		~Iterator() { SetNode(0); }

		/**
		 * This constructor creates an Iterator that points to the specified
		 * @c node.
		 */
		explicit Iterator(Node* node)
			: m_node(node)
		{
			if (m_node)
				m_node->Use(false);
		}

		/**
		 * The copy constructor creates an Iterator that points to the same
		 * node as the specified @c other Iterator.
		 */
		Iterator(const Iterator& other)
			: m_node(other.GetNode())
		{
			if (m_node)
				m_node->Use(false);
		}

		/**
		 * The assignment operator sets this Iterator to the same node as the
		 * specified @c other Iterator.
		 */
		const Iterator& operator=(const Iterator& other)
		{
			SetNode(other.GetNode()); return *this;
		}

		/**
		 * The comparison operators compare this Iterator with the specified
		 * other Iterator. Two iterators are equal iff they point to the same
		 * node.
		 * @{
		 */
		bool operator==(const Iterator& other) const
		{
			OP_ASSERT(!m_node || m_node->IsInSameList(other.GetNode()));
			return m_node == other.GetNode();
		}

		bool operator!=(const Iterator& other) const
		{
			return !(*this == other);
		}

		template<typename CompatibleIterator>
		bool operator==(const CompatibleIterator& other) const
		{
			return other == *this;
		}

		template<typename CompatibleIterator>
		bool operator!=(const CompatibleIterator& other) const
		{
			return other != *this;
		}

		/** @} */

		/**
		 * The pre-increment operator moves this Iterator to the next Node in
		 * the list and returns a reference to this Iterator.
		 */
		Iterator& operator++() { Next(true); return *this; }

		/**
		 * The post-increment operator moves this Iterator to the next node
		 * in the list and returns an new Iterator instance that points to the
		 * Node this instance pointed to before it was incremented.
		 */
		Iterator operator++(int)
		{
			Iterator previous = *this;
			Next(true);
			return previous;
		}

		/**
		 * The pre-decrement operator moves this Iterator to the previous Node
		 * in the list and returns a reference to this Iterator.
		 */
		Iterator& operator--() { Next(false); return *this; }

		/**
		 * The post-decrement operator moves this Iterator to the previous node
		 * in the list and returns a new Iterator instance that points to the
		 * Node this instance pointed to before it was decremented.
		 */
		Iterator operator--(int)
		{
			Iterator previous = *this;
			Next(false);
			return previous;
		}

		/**
		 * The dereference operator returns a reference of the item of the
		 * associated Node.
		 */
		Reference operator*() const { return m_node->Item(); }

		/**
		 * The pointer operator returns a pointer to the item of the associated
		 * Node.
		 */
		ConstPointer operator->() const { return &(m_node->Item()); }
		Pointer operator->() { return &(m_node->Item()); }
	};

	typedef OtlConstIterator<Iterator> ConstIterator;

	/**
	 * The ReverseIterator or ReverseConstIterator can be used to iterate over
	 * the elements in the list in the reverse order, i.e. from the last element
	 * in the list (as returned by RBegin()) to the first element until the
	 * iterator hits REnd().
	 *
	 * The reverse iterators are based on the normal Iterator and ConstIterator.
	 * The operator++() of the reverse iterator calls operator--() on the
	 * underlying base iterator. And the operator--() of the reverse iterator
	 * calls operator++() on the underlying base iterator.
	 *
	 * Example:
	 * @code
	 * OtlList<int> list = ...;
	 * OtlList<int>::ReverseConstIterator itr;
	 * for (itr = list.RBegin(); itr != list.REnd(); ++itr)
	 *    printf("Item: %d\n", *itr);
	 * @endcode
	 *
	 * @note Calling InsertAfter() and InsertBefore() with a reverse iterator
	 *       respects the direction of the iterator, i.e. the new item will be
	 *       inserted in the other direction relative to the underlying base
	 *       iterator.
	 *
	 * @{
	 */
	typedef OtlReverseIterator<Iterator> ReverseIterator;
	typedef OtlReverseIterator<ConstIterator> ReverseConstIterator;
	/* @} */

	/**
	 * The Range and ConstRange can be used to iterate over a range of elements
	 * in the list. The ReverseRange and ReverseConstRange can be used to
	 * iterate over a range of element in the list in the reverse order.
	 *
	 * Example:
	 * @code
	 * OtlList<int> list = ...;
	 * OtlList<int>::ConstRange range;
	 * for (range = list.All(); range; ++range)
	 *    printf("Item: %d\n", *range);
	 * @endcode
	 *
	 * @see All(), ConstAll(), RAll(), RConstAll()
	 */
	typedef OtlRange<Iterator, ConstIterator> Range;
	typedef OtlRange<ConstIterator> ConstRange;
	typedef OtlRange<ReverseIterator, ReverseConstIterator> ReverseRange;
	typedef OtlRange<ReverseConstIterator> ReverseConstRange;

	/**
	 * Constructor.
	 *
	 * @note The constructor allocates memory for its head Node
	 *       (m_head). Allocating that memory may fail. But the failure to
	 *       allocate the memory is not reported back to the caller until the
	 *       caller inserts the first element into the list. Thus inserting an
	 *       element may fail even if, between constructing the list and
	 *       inserting the element, enough memory was freed.
	 *       If the constructor fails to allocate the head Node, the list is a
	 *       valid empty list, i.e. all methods behave like on any other empty
	 *       list, but it will refuse to insert any elements.
	 */
	OtlList(void) : m_end(Iterator(Head())) {}

	/// Destructor
	virtual ~OtlList(void) { Clear(); }

	/**
	 * Count the number of items in this list.
	 *
	 * Takes O(n) time.
	 */
	virtual size_t Count(void) const
	{
		size_t ret = 0;
		for (ConstIterator itr = Begin(); !IsAtEnd(itr); ++itr)
			ret++;
		return ret;
	}

	/// Return the first item in the list.
	op_force_inline const T& First(void) const
	{
		OP_ASSERT(!IsEmpty()); return StartNode(true)->Item();
	}

	op_force_inline T& First(void)
	{
		OP_ASSERT(!IsEmpty()); return StartNode(true)->Item();
	}

	/// Return the last item in the list.
	op_force_inline const T& Last(void) const
	{
		OP_ASSERT(!IsEmpty()); return StartNode(false)->Item();
	}

	op_force_inline T& Last(void)
	{
		OP_ASSERT(!IsEmpty()); return StartNode(false)->Item();
	}

	/**
	 * Return the Iterator to first list element.
	 *
	 * Returns an iterator that equals End() if the list is empty.
	 */
	op_force_inline Iterator Begin(void)
	{
		return Iterator(StartNode(true));
	}

	op_force_inline ConstIterator Begin(void) const
	{
		return const_cast<OtlList*>(this)->Begin();
	}

	op_force_inline ConstIterator ConstBegin(void) const
	{
		return Begin();
	}

	/// Return the ConstIterator that points past the last list element.
	op_force_inline Iterator End(void)
	{
		return Iterator(Head());
	}

	op_force_inline ConstIterator End(void) const
	{
		return const_cast<OtlList*>(this)->End();
	}

	/**
	 * Return the ReverseIterator to last list element.
	 *
	 * Returns an iterator that equals REnd() if the list is empty.
	 */
	ReverseIterator RBegin(void)
	{
		return ReverseIterator(Iterator(StartNode(false)));
	}

	/**
	 * Return the ReverseConstIterator to last list element.
	 *
	 * Returns an iterator that equals REnd() if the list is empty.
	 */
	ReverseConstIterator RBegin(void) const
	{
		return const_cast<OtlList*>(this)->RBegin();
	}

	ReverseConstIterator RConstBegin(void) const
	{
		return RBegin();
	}

	/**
	 * Return the ReverseConstIterator that points before the first list
	 * element.
	 */
	ReverseIterator REnd(void)
	{
		return ReverseIterator(End());
	}

	ReverseConstIterator REnd(void) const
	{
		return const_cast<OtlList*>(this)->REnd();
	}

	/// Returns the range of all elements in the list.
	Range All(void) { return Range(Begin(), End()); }
	ConstRange All(void) const { return ConstRange(ConstBegin(), End()); }
	ConstRange ConstAll(void) const { return ConstRange(ConstBegin(), End()); }

	/// Returns the range of all elements in the list in reverse order.
	ReverseRange RAll(void)
	{
		return ReverseRange(RBegin(), REnd());
	}

	ReverseConstRange RAll(void) const
	{
		return ReverseConstRange(RConstBegin(), REnd());
	}

	ReverseConstRange RConstAll(void) const
	{
		return ReverseConstRange(RConstBegin(), REnd());
	}

	/// Returns true if the specified iterator is at the end of the list.
	bool IsAtEnd(const ConstIterator& itr) const
	{
		return itr == m_end;
	}

	bool IsAtEnd(const Iterator& itr) const
	{
		OP_ASSERT(ContainsNode(itr.GetNode()));
		return itr.GetNode() == Head();
	}

	bool IsAtEnd(const ReverseConstIterator& itr) const
	{
		return IsAtEnd(itr.Base());
	}

	bool IsAtEnd(const ReverseIterator& itr) const
	{
		return IsAtEnd(itr.Base());
	}

	/// Insert the given @c item at the start of this list.
	OP_STATUS Prepend(const T& item)
	{
		return InsertAfter(item, Iterator(Head()));
	}

	/// Insert the given @c item at the end of this list.
	OP_STATUS Append(const T& item)
	{
		return InsertAfter(item, Iterator(StartNode(false)));
	}

	/**
	 * Insert the given @c item before the given list iterator. If insertion is
	 * successful, then you can decrease the iterator to point it to the
	 * inserted item.
	 *
	 * @param item will be inserted.
	 * @param itr points to the item to insert before.
	 */
	OP_STATUS InsertBefore(const T& item, const Iterator& itr)
	{
		Iterator previous = itr; --previous;
		return InsertAfter(item, previous);
	}

	/**
	 * Insert the given @c item after the given list iterator. If insertion is
	 * successful, then you can increase the iterator to point it to the
	 * inserted item.
	 *
	 * @param item will be inserted.
	 * @param itr points to the item to insert after.
	 */
	virtual OP_STATUS InsertAfter(const T& item, const Iterator& itr)
	{
		RETURN_OOM_IF_NULL(Head());
#ifdef DEBUG_ENABLE_OPASSERT
		Node* node = OP_NEW(Node, (Head(), item));
#else // !DEBUG_ENABLE_OPASSERT
		Node* node = OP_NEW(Node, (item));
#endif // DEBUG_ENABLE_OPASSERT
		RETURN_OOM_IF_NULL(node);
		node->Use(true);
		node->InsertAfter(itr.GetNode());
		return OpStatus::OK;
	}

	/**
	 * Insert the given @c item before the given reverse iterator. If inserting
	 * is successful, then you can decrease the iterator to point it to the
	 * inserted item.
	 *
	 * @note Inserting an item before the specified reverse iterator means to
	 *       insert it after the underlying normal Iterator. The direction
	 *       "before" refers to the direction of the iterator.
	 *
	 * @param item will be inserted.
	 * @param itr points to the item to insert before.
	 */
	OP_STATUS InsertBefore(const T& item, const ReverseIterator& itr)
	{
		return InsertAfter(item, itr.Base());
	}

	/**
	 * Insert the given @c item after the given reverse iterator. If inserting
	 * is successful, then you can increase the iterator to point it to the
	 * inserted item.
	 *
	 * @note Inserting an item after the specified reverse iterator means to
	 *       insert it before the underlying normal Iterator. The direction
	 *       "after" refers to the direction of the iterator.
	 *
	 * @param item will be inserted.
	 * @param itr points to the item to insert after.
	 */
	OP_STATUS InsertAfter(const T& item, const ReverseIterator& itr)
	{
		return InsertBefore(item, itr.Base());
	}

	/// Remove and return the first item.
	T PopFirst(void) { OP_ASSERT(!IsEmpty()); return Remove(Begin()); }

	/// Remove and return the last item.
	T PopLast(void) { OP_ASSERT(!IsEmpty()); return Remove(RBegin()); }

	/**
	 * Remove the Node from the list that is pointed to by the specified
	 * Iterator, and return its item.
	 *
	 * This method is useful when you're iterating through a list using
	 * the Iterator returned from Begin() and End(), and you want to remove the
	 * item pointed at by the specified @c Iterator.
	 *
	 * This method causes the Node of the specified Iterator to be removed from
	 * the list; it will subsequently be destroyed once all Iterators have moved
	 * off it.
	 * @{
	 */
	virtual T Remove(const Iterator& itr)
	{
		if (!IsAtEnd(itr))
			itr.GetNode()->Release(true);
		return *itr;
	}

	T Remove(const ReverseIterator& itr) { return Remove(itr.Base()); }
	/** @} */

	/**
	 * Remove an element by item.
	 *
	 * Starts searching for the item to remove at the specified @c begin. The
	 * first element that matches the @c item is removed.
	 *
	 * @param item  element to be removed.
	 * @param begin points to the element where the search shall start. (Note
	 *              that @c begin is included in the search, i.e.
	 *              if *begin == item, then @c begin is removed from the list.)
	 *
	 * @return true if the element was removed, or false if it was not found.
	 */
	template<typename _Iterator>
	bool RemoveItem(const T& item, const _Iterator& begin)
	{
		_Iterator itr = FindItem(item, begin);
		if (!IsAtEnd(itr))
		{
			Remove(itr);
			return true;
		}
		return false;
	}

	bool RemoveItem(const T& item) { return RemoveItem(item, Begin()); }

	/**
	 * Searches for the specified @c item in the list starting at the specified
	 * @c begin. If the @c item is found, an iterator pointing to the item is
	 * returned. If the @c item is not found, an iterator that equals End()
	 * resp. REnd() is returned.
	 *
	 * @note The specified iterator @c begin determines the direction of the
	 *       search. If a ConstIterator or an Iterator is used, the method
	 *       searches the list in forward direction. If a reverse iterator is
	 *       used, the method searches the list backwards.
	 *
	 * Example: find all occurrences of one element:
	 * @code
	 *  OtlList<int> list = ...;
	 *  OtlList<int>::ConstIterator i;
	 *  for (i = list.Begin(); (i = list.FindItem(17, i)) != list.End(); ++i)
	 *      printf("Found another 17\n");
	 * @endcode
	 *
	 * @param item  is the element to find.
	 * @param begin points to the element where the search shall start. (Note
	 *              that @c begin is included in the search, i.e.
	 *              if *begin == item, then @c begin is returned.)
	 *
	 * @return An iterator that points to the found element or an iterator that
	 *         equals End() resp. REnd() if the element is not found. The
	 *         returned iterator has the same type as the @c begin iterator.
	 */
	template<typename _Iterator>
	_Iterator FindItem(const T& item, const _Iterator& begin) const
	{
		_Iterator itr = begin;
		while (!IsAtEnd(itr) && *itr != item)
			++itr;
		return itr;
	}

	Iterator FindItem(const T& item) { return FindItem(item, Begin()); }

	ConstIterator FindItem(const T& item) const { return FindItem(item, Begin()); }

	/**
	 * Consume the given number of items from the front of the list.
	 *
	 * Returns the number of items consumed (will be less than @c count if
	 * there are not enough items in this list).
	 */
	size_t Consume(size_t count)
	{
		size_t ret = 0;
		for (Iterator itr = Begin(); !IsAtEnd(itr) && ret < count; ++itr, ++ret)
			Remove(itr);
		return ret;
	}

	/// Remove all items from this list.
	op_force_inline void Clear(void)
	{
		for (Iterator itr = Begin(); !IsAtEnd(itr); ++itr)
			Remove(itr);
		OP_ASSERT(IsEmpty());
	}
protected:
	const ConstIterator m_end;
}; // OtlList

/**
 * An OtlList which caches the number of items held.
 *
 * See OtlList for missing documentation on virtual methods.
 */
template<class T>
class OtlCountedList : public OtlList<T>
{
public:
	typedef typename OtlList<T>::Iterator Iterator;
	typedef typename OtlList<T>::ConstIterator ConstIterator;

	/// Constructor
	OtlCountedList(void) : m_count(0) {}

	/**
	 * Count the number of items in this list.
	 *
	 * Takes O(1) time.
	 */
	virtual op_force_inline size_t Count(void) const { return m_count; }
	virtual op_force_inline size_t Length(void) const { return m_count; }

	virtual OP_STATUS InsertAfter(const T& item, const Iterator& itr)
	{
		RETURN_IF_ERROR(OtlList<T>::InsertAfter(item, itr));
		m_count++;
		return OpStatus::OK;
	}

	virtual T Remove(const Iterator& itr)
	{
		OP_ASSERT(m_count);
		m_count--;
		return OtlList<T>::Remove(itr);
	}

private:
	/// Copy constructor. Forbidden because of possible OOM.
	OtlCountedList(const OtlCountedList& other);

	/// Assignment operator. Forbidden because of possible OOM.
	const OtlCountedList& operator=(const OtlCountedList& other);

	/// Cached item count.
	size_t m_count;
}; // OtlCountedList

#endif // MODULES_OTL_LIST_H
