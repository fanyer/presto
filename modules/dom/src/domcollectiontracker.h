/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DOM_COLLECTIONTRACKER_H
#define DOM_COLLECTIONTRACKER_H

#include "modules/util/OpHashTable.h"

class DOM_CollectionLink
	: public ListElement<DOM_CollectionLink>
{
public:
	virtual ~DOM_CollectionLink() { Out(); }

protected:
	friend class DOM_CollectionTracker;

	DOM_CollectionLink(BOOL reusable)
		: reusable(reusable)
	{
	}

	BOOL reusable;
};

class DOM_CollectionCollection
	: public List<DOM_CollectionLink>
{
public:
	~DOM_CollectionCollection() { RemoveAll(); }
};

class DOM_Node;
class DOM_Collection;
class DOM_NodeCollection;
class DOM_CollectionFilter;
class DOM_EnvironmentImpl;

class DOM_CollectionTracker
{
public:
	DOM_CollectionTracker()
		: cache_misses(0)
	{
		missed_root_match[0] = NULL;
		missed_root_match[1] = NULL;
	}

	~DOM_CollectionTracker()
	{
		RemoveAll();
	}

	void Add(DOM_CollectionLink* collection, HTML_Element* tree_root);
	void TreeDestroyed(HTML_Element* root);
	BOOL Find(DOM_NodeCollection *&collection, DOM_Node* root, BOOL include_root, DOM_CollectionFilter* filter);

	BOOL IsEmpty();

	void SignalChange(HTML_Element* element, BOOL added, BOOL removed, int collections);
	void InvalidateCollections();
	void RemoveAll();

	/**
	 * A node that used to belong to the environment that owns this object has
	 * migrated into a new environment, so we need to check every collection
	 * we're tracking to make sure it really belongs to us, or move it to the
	 * environment it ought to be tracked by.
	 */
	void MigrateCollections(DOM_EnvironmentImpl *environment);

private:
	OpAutoPointerHashTable<HTML_Element, DOM_CollectionCollection> collection_collection_collection;
	DOM_CollectionCollection lost_and_found;

	void *missed_root_match[2];
	unsigned cache_misses;
};

#endif // DOM_COLLECTIONTRACKER_H
