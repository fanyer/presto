/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_TREE_H
#define MODULES_UTIL_TREE_H

#include "modules/util/simset.h"

class Tree
  : public Link,
    public Head
{
public:

    virtual				~Tree() { Clear(); }

    Tree*				Parent() const { return (Tree*) GetList(); }

    Tree*				Suc() const { return (Tree*) Link::Suc(); }
    Tree*				Pred() const { return (Tree*) Link::Pred(); }

    void				Under(Tree* tree) { Into(tree); }

    Tree*				LastChild() const { return (Tree*) Last(); }
    Tree*				FirstChild() const { return (Tree*) First(); }

	Tree*				LastLeaf(); // MSVC can't optimise recursive calls { return LastChild() ? LastChild()->LastLeaf() : this; }
    Tree*				FirstLeaf(); // MSVC can't optimise recursive calls { return FirstChild() ? FirstChild()->FirstLeaf() : this; }

    Tree*				PrevLeaf();
    Tree*				NextLeaf();

    Tree*				Next() const;
    Tree*				Prev() const;
};

#endif // !MODULES_UTIL_TREE_H
