/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#ifndef _FOLDER_STACK_
#define _FOLDER_STACK_

/**
 * FolderStack
 *
 **/
template<class T>
class FolderStack
{
public:
	explicit FolderStack() : m_first(0), m_count(0) {}
	~FolderStack() { OP_ASSERT(IsEmpty()); }
	BOOL IsEmpty() { return !m_first;}
	UINT32 GetCount() { return m_count; }
	void Push(T* item)
	{
		if (!item) return;
		if (!m_first)
		{
			m_first = OP_NEW(Node, (item));
			m_first->SetNext(0);
		}
		else
		{
			Node* tmp = m_first;
			m_first = OP_NEW(Node, (item));
			m_first->SetNext(tmp);
			
		}
		m_count++;
	}

	T* Pop()
	{
		if (!m_first) return 0;
		Node* tmp = m_first;
		m_first = tmp->GetNext();
		tmp->SetNext(0);
		T* item = tmp->GetElem();
		tmp->SetElem(0);
		OP_DELETE(tmp);
		m_count--;
		return item;
	}

	T* Peek() { if (m_first) { return m_first->GetElem(); } else return 0; }

	void Empty()
	{
		while (Pop())
			;
	}

private:
	struct Node
	{
	private:
		T* m_elem;
		Node* m_next;
	public:
		Node(T* item):m_elem(item), m_next(0){}
		void SetNext(Node* next){ m_next = next; }
		Node* GetNext() { return m_next; }
		T* GetElem() { return m_elem; }
		void SetElem(T* elem) { m_elem = elem; }
	};

	Node* m_first;
	UINT32 m_count;
};

#endif // _FOLDER_STACK_
