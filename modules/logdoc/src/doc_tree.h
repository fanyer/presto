/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _DOC_TREE_H_
#define _DOC_TREE_H_

class DocTree
{
private:
	DocTree*    m_parent;

	DocTree*    m_suc;
	DocTree*    m_pred;

	DocTree*    m_first_child;
	DocTree*    m_last_child;

public:
				DocTree() : m_parent(NULL), m_suc(NULL), m_pred(NULL),
							m_first_child(NULL), m_last_child(NULL) {};
#ifdef _DEBUG
		virtual
#endif // DEBUG
				~DocTree() {};

	void		Reset() { m_parent = m_suc = m_pred = m_first_child = m_last_child = NULL; }

	DocTree*	Parent() const { return m_parent; }
	void		SetParent(DocTree *new_parent) { m_parent = new_parent; }
	DocTree*	Suc() const { return m_suc; }
	void		SetSuc(DocTree *new_suc) { m_suc = new_suc; }
	DocTree*	Pred() const { return m_pred; }
	void		SetPred(DocTree *new_pred) { m_pred = new_pred; }
	DocTree*	LastChild() const { return m_last_child; }
	void		SetLastChild(DocTree *new_child) { m_last_child = new_child; }
	DocTree*	FirstChild() const { return m_first_child; }
	void		SetFirstChild(DocTree *new_child) { m_first_child = new_child; }

	void		Under(DocTree* tree);
	void		Out();
	void		DetachChildren();
	void		Follow(DocTree* tree);
	void		Precede(DocTree* tree);

	DocTree*	LastLeaf();
	DocTree*	FirstLeaf();

	DocTree*	PrevLeaf();
	DocTree*	NextLeaf();
    
	/** Calculates the level of a node. The root node has level 1. */
	UINT		Level() const
	{
		UINT level = 1;
		for (const DocTree* parent = this->Parent(); parent; parent = parent->Parent())
			level++;
		return level;
	}

	BOOL		Precedes(const DocTree* other_tree) const;
	BOOL		IsAncestorOf(DocTree* tree) const;

	DocTree*	Next() const;
	DocTree*	NextSibling() const;
	DocTree*	Prev() const;
	DocTree*	PrevSibling() const;

	DocTree*	Root()
	{
		DocTree *root = this;
		while (root->Parent())
			root = root->Parent();
		return root;
	}

	/**	Append the children of list to the children of this. */
	void		AppendChildren(DocTree *list);

	/**	Insert the children of list into the list of children of this before node.
		Same as AppendChildren, but the list can be	put in the middle of another
		list instead of at the end only. node *must* be a child of this.

		InsertChildrenBefore(list, NULL); == AppendChildren(list);
	*/
	void		InsertChildrenBefore(DocTree *list, DocTree *node);
};

#endif // _DOC_TREE_H_
