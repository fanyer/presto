/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/src/doc_tree.h"

void DocTree::Under(DocTree* tree)
{
#ifdef DO_ASSERT
	OP_ASSERT(!m_parent);
    OP_ASSERT(tree);
#endif
    /*
     *  If the previous assertion is removed, we will have to
     *  include an "Out();" here first.
     */

    m_pred = tree->m_last_child;
	if (m_pred)
		m_pred->m_suc = this;
	else
		tree->m_first_child = this;

#ifdef DO_ASSERT
	OP_ASSERT(!m_pred || m_pred != m_suc);
	OP_ASSERT(m_pred != this);
	OP_ASSERT(m_suc != this);
#endif

    tree->m_last_child = this;
	m_parent = tree;
}

void DocTree::Out()
{
#ifdef DO_ASSERT
    OP_ASSERT(m_parent);
#endif

	if (m_suc)
		// If we've a successor, unchain us
	    m_suc->SetPred(m_pred);
	else
		// If we don't, we're the last in the list
		m_parent->SetLastChild(m_pred);

	if (m_pred)
		// If we've a predecessor, unchain us
		m_pred->SetSuc(m_suc);
	else
		// If we don't, we're the first in the list
		m_parent->SetFirstChild(m_suc);

#ifdef DO_ASSERT
	OP_ASSERT(!m_pred || !m_pred->Suc() || m_pred->Suc() != m_pred->Pred());
	OP_ASSERT(!m_suc || !m_suc->Suc() || m_suc->Suc() != m_suc->Pred());
	OP_ASSERT(m_pred != this);
	OP_ASSERT(m_suc != this);
	OP_ASSERT(!m_pred || m_pred->Suc() != m_pred || m_pred->Pred() != m_pred);
	OP_ASSERT(!m_suc || m_suc->Suc() != m_suc || m_suc->Pred() != m_suc);
#endif

	m_pred = NULL;
	m_suc = NULL;
	m_parent = NULL;
}

void DocTree::DetachChildren()
{
    DocTree *tmp_child = m_first_child;
    while (tmp_child)
    {
        tmp_child->m_parent = NULL;
        tmp_child = tmp_child->m_suc;
    }
    m_first_child = NULL;
    m_last_child = NULL;
}

void DocTree::Follow(DocTree* tree)
{
#ifdef DO_ASSERT
    OP_ASSERT(tree);
    OP_ASSERT(!m_parent);
#endif

    /*
     *  If the previous assertion is removed, we will have to
     *  include an "Out();" here first.
     */
	m_parent = tree->Parent();

    m_pred = tree;
    m_suc = tree->Suc();
	if (m_suc)
		m_suc->SetPred(this);
	else
		m_parent->SetLastChild(this);

    tree->SetSuc(this);
}

void DocTree::Precede(DocTree* tree)
{
#ifdef DO_ASSERT
    OP_ASSERT(tree);
    OP_ASSERT(!m_parent);
#endif
    /*
     *  If the previous assertion is removed, we will have to
     *  include an "Out();" here first.
     */
	m_parent = tree->Parent();

    m_suc = tree;
    m_pred = tree->Pred();
	if (m_pred)
		m_pred->SetSuc(this);
	else
		m_parent->SetFirstChild(this);

    tree->SetPred(this);
}

DocTree* DocTree::LastLeaf()
{
	// optimised for compilers that can't optimise recursive methods
    DocTree *leaf = this;

    while (leaf->LastChild())
        leaf = leaf->LastChild();
    
    return leaf;
}

DocTree* DocTree::FirstLeaf()
{
	// optimised for compilers that can't optimise recursive methods
    DocTree *leaf = this;

    while (leaf->FirstChild())
        leaf = leaf->FirstChild();
    
    return leaf;
}

BOOL DocTree::Precedes(const DocTree* other_tree) const
{
    if (other_tree == this)
        return FALSE;

	const DocTree* walk = this;

	UINT this_level = Level();
	UINT other_level = other_tree->Level();
	BOOL other_is_above = this_level < other_level;

	while (walk->Parent() != other_tree->Parent())
	{
		if (this_level >= other_level)
		{
			walk = walk->Parent();
			--this_level;
		}

		if (this_level < other_level)
		{
			other_tree = other_tree->Parent();
			--other_level;
		}
	}

	if (walk == other_tree)
		// Special case; same branch

		return other_is_above;

	while ((other_tree = other_tree->Pred()) != NULL)
		if (walk == other_tree)
			return TRUE;

	return FALSE;
}

BOOL DocTree::IsAncestorOf(DocTree* tree) const
{
	// optimised for compilers that can't optimise recursive methods
    while (tree)
        if (tree == this)
            return TRUE;
        else
            tree = tree->Parent();

    return FALSE;
}

DocTree* DocTree::PrevLeaf()
{
    DocTree* leaf = this;

	while (!leaf->Pred())
    {
		// If leaf doesn't have a predecessor, go looking in parent
        leaf = leaf->Parent();

		if (!leaf)
			return NULL;
    }

    leaf = leaf->Pred();

	// Then, traverse down leaf to find last child, if it has any
	while (leaf->LastChild())
		leaf = leaf->LastChild();

	return leaf;
}

DocTree* DocTree::NextLeaf()
{
    DocTree* leaf = this;

	while (!leaf->Suc())
    {
		// If leaf doesn't have a successor, go looking in parent
        leaf = leaf->Parent();

		if (!leaf)
			return NULL;
    }

    leaf = leaf->Suc();

	// Then, traverse down leaf to find first child, if it has any
	while (leaf->FirstChild())
		leaf = leaf->FirstChild();

	return leaf;
}

DocTree* DocTree::Next() const
{
	if (FirstChild())
		return FirstChild();

	for (const DocTree *leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Suc())
			return leaf->Suc();

	return NULL;
}

DocTree* DocTree::NextSibling() const
{
	for (const DocTree *leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Suc())
			return leaf->Suc();

	return NULL;
}

DocTree* DocTree::Prev() const
{
	if (Pred())
    {
		DocTree* leaf = Pred();

		while (leaf->LastChild())
			leaf = leaf->LastChild();

		return leaf;
	}

	return Parent();
}

DocTree* DocTree::PrevSibling() const
{
	for (const DocTree *leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Pred())
			return leaf->Pred();

	return NULL;
}

void DocTree::AppendChildren(DocTree *list)
{
	DocTree* first_added = list->m_first_child;

	if (!first_added)
		return;

	DocTree* last_added = list->m_last_child;

	list->m_first_child = NULL;
	list->m_last_child = NULL;
	
	if (m_last_child)
	{
		m_last_child->m_suc = first_added;
		first_added->m_pred = m_last_child;
	}
	else
		m_first_child = first_added;

	m_last_child = last_added;

	while (first_added)
	{
		first_added->m_parent = this;
		first_added = first_added->Suc();
	}
}

void DocTree::InsertChildrenBefore(DocTree *list, DocTree *node)
{
	if (!node)
	{
		// Insert at the end is the same as append
		AppendChildren(list);
		return;
	}

#ifdef DO_ASSERT
    OP_ASSERT(node->parent == this);
#endif

	DocTree* first_added = list->m_first_child;

	// inserting an empty list does not change this.
	if (!first_added)
		return;

	DocTree* last_added = list->m_last_child;

	list->m_first_child = NULL;
	list->m_last_child = NULL;

	// set the parent of the inserted elements to this list.
	DocTree* tmp = first_added;
	while (tmp)
	{
		tmp->m_parent = this;
		tmp = tmp->Suc();
	}

	// insert the added list elements into the list.
	if (node->m_pred)
	{
		first_added->m_pred = node->m_pred;
		node->m_pred->m_suc = first_added;
	}
	else
		m_first_child = first_added;

	node->m_pred = last_added;
	last_added->m_suc = node;
}
