/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/tree.h"

Tree* Tree::LastLeaf()
{
	// optimised for compilers that can't optimise recursive methods

	Tree* leaf = this;

	while (leaf->LastChild())
		leaf = leaf->LastChild();

	return leaf;
}

Tree* Tree::FirstLeaf()
{
	// optimised for compilers that can't optimise recursive methods

	Tree* leaf = this;

	while (leaf->FirstChild())
		leaf = leaf->FirstChild();

	return leaf;
}

Tree* Tree::NextLeaf()
{
    Tree* leaf = this;

	while (!leaf->Suc()) {
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

Tree* Tree::PrevLeaf()
{
    Tree* leaf = this;

	while (!leaf->Pred()) {
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

Tree* Tree::Next() const
{
	if (FirstChild())
		return FirstChild();

	for (const Tree *leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Suc())
			return leaf->Suc();

	return NULL;
}

Tree* Tree::Prev() const
{
	if (Pred()) {
		Tree* leaf = Pred();

		while (leaf->LastChild())
			leaf = leaf->LastChild();

		return leaf;
	}

	return Parent();
}
