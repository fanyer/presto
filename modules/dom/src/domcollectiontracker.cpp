/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domcollectiontracker.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domhtml/htmlcoll.h"

#include "modules/logdoc/htm_elm.h"

void
DOM_CollectionTracker::Add(DOM_CollectionLink *link, HTML_Element *tree_root)
{
	DOM_NodeCollection *collection = static_cast<DOM_NodeCollection *>(link);
	DOM_Node *root = collection->GetRoot();

	if (missed_root_match[0] == root)
		missed_root_match[0] = NULL;
	if (missed_root_match[1] == root)
		missed_root_match[1] = NULL;

	DOM_CollectionCollection *collection_collection;

	if (OpStatus::IsError(collection_collection_collection.GetData(tree_root, &collection_collection)))
	{
		collection_collection = OP_NEW(DOM_CollectionCollection, ());
		if (!collection_collection || OpStatus::IsMemoryError(collection_collection_collection.Add(tree_root, collection_collection)))
		{
			collection->Into(&lost_and_found);
			return;
		}
	}

	if (link->reusable)
		link->IntoStart(collection_collection);
	else
		link->Into(collection_collection);
}

void
DOM_CollectionTracker::TreeDestroyed(HTML_Element *tree_root)
{
	DOM_CollectionCollection *collection_collection;
	if (OpStatus::IsSuccess(collection_collection_collection.Remove(tree_root, &collection_collection)))
		OP_DELETE(collection_collection);
}

BOOL
DOM_CollectionTracker::Find(DOM_NodeCollection *&collection, DOM_Node *root, BOOL include_root, DOM_CollectionFilter *filter)
{
	if (!root)
		return FALSE;

	if (root == missed_root_match[0] || root == missed_root_match[1])
		/* If we're on a missing streak, don't continue. */
		if (++cache_misses > 32)
			return FALSE;

	HTML_Element *tree_root = root->GetPlaceholderElement();

	// We might have just opened the document by destructive document.open() and the tree might not be there yet.
	if (!tree_root)
		return FALSE;

	while (HTML_Element *parent = tree_root->Parent())
		tree_root = parent;

	DOM_CollectionCollection *collection_collection;

	if (OpStatus::IsSuccess(collection_collection_collection.GetData(tree_root, &collection_collection)))
	{
		BOOL matched_root = FALSE;
		for (DOM_CollectionLink *link = collection_collection->First(); link && link->reusable; link = link->Suc())
		{
			DOM_NodeCollection *candidate = static_cast<DOM_NodeCollection *>(link);
			if (candidate->GetRoot() == root)
				if (candidate->IsSameCollection(root, include_root, filter))
				{
					collection = candidate;
					cache_misses >>= 1;
					return TRUE;
				}
				else
					matched_root = TRUE;
		}

		if (!matched_root)
			return FALSE;
	}

	if (root != missed_root_match[0])
	{
		missed_root_match[1] = missed_root_match[0];
		missed_root_match[0] = root;
	}

	return FALSE;
}

BOOL
DOM_CollectionTracker::IsEmpty()
{
	return collection_collection_collection.GetCount() == 0;
}

static void
DOM_UpdateCollections(DOM_CollectionCollection *collection_collection, HTML_Element *tree_root, HTML_Element *element, BOOL added, BOOL removed, int affected_collections)
{
	List<DOM_CollectionLink> local;

	local.Append(collection_collection);

	while (DOM_CollectionLink *link = local.First())
	{
		link->Out();
		link->Into(collection_collection);

		static_cast<DOM_NodeCollection *>(link)->ElementCollectionStatusChanged(tree_root, element, added, removed, affected_collections);
	}
}

void
DOM_CollectionTracker::SignalChange(HTML_Element *element, BOOL added, BOOL removed, int affected_collections)
{
	OP_ASSERT(!(added && removed));

	HTML_Element *tree_root = element;
	while (HTML_Element *parent = tree_root->Parent())
		tree_root = parent;

	if (added || removed)
		affected_collections = DOM_EnvironmentImpl::COLLECTION_ALL;

	DOM_CollectionCollection *collection_collection;

	if (OpStatus::IsSuccess(collection_collection_collection.GetData(tree_root, &collection_collection)))
		DOM_UpdateCollections(collection_collection, tree_root, element, added, removed, affected_collections);

	if (added && OpStatus::IsSuccess(collection_collection_collection.GetData(element, &collection_collection)))
		DOM_UpdateCollections(collection_collection, tree_root, element, added, removed, affected_collections);

	if (!lost_and_found.Empty())
		DOM_UpdateCollections(&lost_and_found, tree_root, element, added, removed, affected_collections);
}

static void
DOM_InvalidateCollections(DOM_CollectionCollection *collection_collection)
{
	for (DOM_CollectionLink *link = collection_collection->First(); link; link = link->Suc())
	{
		DOM_NodeCollection *collection = static_cast<DOM_NodeCollection *>(link);
		HTML_Element *root = collection->GetRootElement();
		if (root)
			collection->ElementCollectionStatusChanged(root, root, FALSE, FALSE, 0);
	}
}

static void
DOM_InvalidateCollections0(const void *, void *data)
{
	DOM_InvalidateCollections(static_cast<DOM_CollectionCollection *>(data));
}

void
DOM_CollectionTracker::InvalidateCollections()
{
	collection_collection_collection.ForEach(&DOM_InvalidateCollections0);

	if (!lost_and_found.Empty())
		DOM_InvalidateCollections(&lost_and_found);
}

void
DOM_CollectionTracker::RemoveAll()
{
	collection_collection_collection.DeleteAll();
	lost_and_found.RemoveAll();
}

static void
DOM_MigrateCollections(DOM_CollectionCollection *collection_collection, DOM_EnvironmentImpl *environment)
{
	for (DOM_CollectionLink *link = collection_collection->First(), *next; link; link = next)
	{
		next = link->Suc();

		DOM_NodeCollection *collection = static_cast<DOM_NodeCollection *>(link);

		if (DOM_Node *root = collection->GetRoot())
		{
			DOM_EnvironmentImpl *new_environment = root->GetEnvironment();

			if (new_environment != environment)
			{
				link->Out();
				collection->DOMChangeRuntime(new_environment->GetDOMRuntime());

				if (collection->IsElementCollection())
					new_environment->AddElementCollection(link, collection->GetTreeRoot());
				else
					new_environment->AddNodeCollection(link, collection->GetTreeRoot());
			}
		}
	}
}

class DOM_CollectionMigrationForEachListener
	: public OpHashTableForEachListener
{
public:
	DOM_EnvironmentImpl *environment;

	DOM_CollectionMigrationForEachListener(DOM_EnvironmentImpl *environment)
		: environment(environment)
	{
	}

	virtual void HandleKeyData(const void *key, void *data)
	{
		DOM_MigrateCollections(static_cast<DOM_CollectionCollection *>(data), environment);
	}
};

void
DOM_CollectionTracker::MigrateCollections(DOM_EnvironmentImpl *environment)
{
	DOM_CollectionMigrationForEachListener listener(environment);
	collection_collection_collection.ForEach(&listener);

	if (!lost_and_found.Empty())
		DOM_MigrateCollections(&lost_and_found, environment);
}
