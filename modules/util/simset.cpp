/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/str.h"

#ifndef OP_ASSERT
#define OP_ASSERT(a)
#endif

#include "modules/util/simset.h"

void
Link::Out()
{
	if (suc)
		// If we've a successor, unchain us

	    suc->pred = pred;
	else
		if (parent)
			// If we don't, we're the last in the list

			parent->last = pred;

	if (pred)
		// If we've a predecessor, unchain us

		pred->suc = suc;
	else
		if (parent)
			// If we don't, we're the first in the list

			parent->first = suc;

	pred = NULL;
	suc = NULL;
	parent = NULL;
}

void
Link::Into(Head* list)
{
	OP_ASSERT(!InList());

    pred = list->last;

	if (pred)
		pred->suc = this;
	else
		list->first = this;

    list->last = this;
	parent = list;
}

BOOL
Link::Precedes(const Link* link) const
{
	// optimised for compilers that can't optimise recursive methods

	for (; link; link = link->pred)
		if (suc == link)
			return TRUE;

	return FALSE;
}

void
Link::IntoStart(Head* list)
{
	OP_ASSERT(!InList());

    suc = list->first;
	if (suc)
		suc->pred = this;
	else
		list->last = this;

    list->first = this;
	parent = list;
}

void
Link::Follow(Link* l)
{
	OP_ASSERT(!InList());

	parent = l->parent;

    pred = l;
    suc = l->suc;
	if (suc)
		suc->pred = this;
	else
		parent->last = this;

    l->suc = this;
}

void
Link::Precede(Link* l)
{
	OP_ASSERT(!InList());

	parent = l->parent;

    suc = l;
    pred = l->pred;
	if (pred)
		pred->suc = this;
	else
		parent->first = this;

    l->pred = this;
}

/**
 * Delete all the elements in the list.
 */
void
Head::Clear()
{
    while (first)
    {
        Link* temp = first;

		// Inlining temp->Out() to reduce crash risk by not accessing temp->parent
		OP_ASSERT(temp->parent == this || !"MEMORY CORRUPTION DETECTED");
		OP_ASSERT(!temp->suc || temp->suc->pred == temp || !"MEMORY CORRUPTION DETECTED");

		first = temp->suc;

		if (first)
			first->pred = NULL;
		else
			last = NULL;

		temp->pred = NULL;
		temp->suc = NULL;
		temp->parent = NULL;

        OP_DELETE(temp);
    }
}

UINT
Head::Cardinal() const
{
    unsigned int cardinal = 0;

	for (Link* l = First(); l; l = l->Suc())
		cardinal++;

    return cardinal;
}

void
Head::Append(Head *list) // Add all items in list to this list
{
	Link* first_added = list->first;

	if(!first_added)
		return;

	Link* last_added = list->last;

	list->first = NULL;
	list->last = NULL;

	if(last)
	{
		last->suc = first_added;
		first_added->pred = last;
	}
	else
		first = first_added;
	last  = last_added;

	while(first_added)
	{
		first_added->parent = this;
		first_added = first_added->Suc();
	}
}

void
Head::Sort(LinkSortFn comparator)
{
	Head temporary;
	temporary.Append(this);

	while (Link *iter = temporary.First())
	{
		iter->Out();
		Link *existing = first;
		for (; existing; existing = existing->Suc())
			if (comparator(iter, existing) < 0)
			{
				iter->Precede(existing);
				break;
			}
		if (!existing)
			iter->Into(this);
	}
}
