/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_HTML_COLLECTION_
#define _DOM_HTML_COLLECTION_

#include "modules/dom/domtypes.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/namespace.h"

class HTML_Element;
class DOM_Element;
class LogicalDocument;

class DOM_CollectionFilter
{
public:
	enum Type
	{
		TYPE_SIMPLE,
		TYPE_HTML_ELEMENT,
		TYPE_NAME,
		TYPE_REPETITION_BLOCKS,
		TYPE_LABELS,
		TYPE_TAGS,
		TYPE_CLASSNAME,
		TYPE_CLASSNAME_SINGLE,
		TYPE_MICRODATA_TOPLEVEL_ITEM
	};

	DOM_CollectionFilter()
		: allocated(FALSE),
		  is_incompatible(FALSE)
	{
	}

	virtual ~DOM_CollectionFilter()
	{
	}

	/** Returns TRUE if this filter is guaranteed to admit no collection elements; FALSE otherwise. Such filters
	    can be constructed if you combine a name-filter for "A" with one for "B" as its base filter, for instance. */
	BOOL IsIncompatible() { return is_incompatible; }

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc) = 0;
	virtual DOM_CollectionFilter *Clone() const = 0;

	virtual Type GetType() = 0;
	virtual BOOL IsNameSearch(const uni_char *&name) = 0;

	/** Indicates if the filter may skip subtrees in the search for matching elements (TRUE),
	    or if every element under root is considered (FALSE). If it returns FALSE some
	    optimizations can be applied.
	    @returns TRUE if sub-trees may be skipped, FALSE if all elements are considered. */
	virtual BOOL CanSkipChildren() = 0;

	/** Indicates if the filter only contains elements that are direct children to root (FALSE), or if
	    elements deeper down in the tree may be included (TRUE). A collection that is able to return FALSE
	    will not be invalidated by irrelevant changes deep in the tree and may
	    have other optimizations applied as well.
	    @returns TRUE if the collection may include non-direct descendents, FALSE if it only includes direct children. */
	virtual BOOL CanIncludeGrandChildren() = 0;

	virtual BOOL IsMatched(unsigned collections) = 0;

	virtual BOOL IsEqual(DOM_CollectionFilter *other) = 0;

	/** Checks if this is a filter that only returns Elements, and not generic Nodes. Used to optimize scripts handling text nodes.
	    @returns TRUE if the filter will only allow Element nodes and never anything else. FALSE otherwise.*/
	inline BOOL IsElementFilter() const;

protected:
	BOOL allocated;
	BOOL is_incompatible;
};

class DOM_SimpleCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_SimpleCollectionFilter(DOM_HTMLElement_Group group)
		: group(group)
	{
	}

	void SetGroup(DOM_HTMLElement_Group new_group)
	{
		group = new_group;
	}

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_SIMPLE; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return FALSE; }
	virtual BOOL CanSkipChildren();
	virtual BOOL CanIncludeGrandChildren();
	virtual BOOL IsMatched(unsigned collections);

	virtual BOOL IsEqual(DOM_CollectionFilter *other);

	/**
	 * Not all collections has a length property.
	 */
	BOOL HasLengthProperty() { return group != DOCUMENTNODES; }

	/**
	 * Sometimes the length property hides an object
	 * named "length", but not always. See bug 310696.
	 */
	BOOL IsObjectHidingLengthProperty() { return group == SCRIPTS || group == ALL || group == FORMELEMENTS; }

	/**
	 * Use to know if this stores named elements mapped using window.<name>
	 */
	BOOL IsNameInWindow() { return group == NAME_IN_WINDOW; }

	/**
	 * Used for a small invalidation optimization regarding text nodes.
	 * CHILDNODES contains anything beneath an element, like text nodes, comments,
	 * processing instructions and doctypes, unlike all other collections
	 * which only have elements.
	 */
	BOOL ContainsNonElementNodes() const { return group == CHILDNODES; }

protected:
	DOM_HTMLElement_Group group;
};

inline BOOL DOM_CollectionFilter::IsElementFilter() const { return const_cast<DOM_CollectionFilter*>(this)->GetType() != TYPE_SIMPLE || !static_cast<const DOM_SimpleCollectionFilter*>(this)->ContainsNonElementNodes();}

class DOM_HTMLElementCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_HTMLElementCollectionFilter(HTML_ElementType t, const uni_char *tg, BOOL descend = TRUE)
		: tag_match(t), tag_match_children(descend), tag_name(tg)
	{
	}

	void SetMatchingTag(HTML_ElementType t, const uni_char *tg, BOOL descend = TRUE)
	{
		tag_match = t;
		tag_match_children = descend;
		tag_name = tg;
	}

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_HTML_ELEMENT; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return FALSE; }
	virtual BOOL CanSkipChildren() { return !tag_match_children; }
	virtual BOOL CanIncludeGrandChildren() { return TRUE; }
	virtual BOOL IsMatched(unsigned collections) { return FALSE; }

	virtual BOOL IsEqual(DOM_CollectionFilter *other);

protected:
	HTML_ElementType tag_match;

	BOOL tag_match_children;

	const uni_char *tag_name;
};

class DOM_NameCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_NameCollectionFilter(DOM_CollectionFilter *base, const uni_char *name, BOOL check_name = TRUE, BOOL check_id = TRUE)
		: base(base),
		  name(name),
		  check_name(check_name),
		  check_id(check_id)
	{
		is_incompatible = CheckIncompatible(base);
	}

	virtual ~DOM_NameCollectionFilter()
	{
		if (allocated)
		{
			OP_DELETE(base);
			op_free((uni_char *) name);
		}
	}

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_NAME; }
	virtual BOOL IsNameSearch(const uni_char *&name) { name = this->name; return TRUE; }
	virtual BOOL CanSkipChildren() { return base && base->CanSkipChildren(); }
	virtual BOOL CanIncludeGrandChildren() { return !base || base->CanIncludeGrandChildren(); }
	virtual BOOL IsMatched(unsigned collections);

	/**
	 * Compares this filter with the one passed and tells if they filter the exact same elements.
	 */
	virtual BOOL IsEqual(DOM_CollectionFilter *filter);

	/**
	 * Helper function.
	 * Tells if a given HE_type can be mapped to window[property] if
	 * its name attribute matches.
	 * See http://www.whatwg.org/specs/web-apps/current-work/#named-access-on-the-window-object
	 */
	static BOOL IsHETypeWithNameAllowedOnWindow(HTML_ElementType type);

	static BOOL IsNameFilterFor(DOM_CollectionFilter *filter, const uni_char *name);

protected:
	DOM_CollectionFilter *base;
	const uni_char *name;
	BOOL check_name;
	BOOL check_id;

	BOOL CheckIncompatible(DOM_CollectionFilter *base_filter);
};

class DOM_TagsCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_TagsCollectionFilter(DOM_CollectionFilter *base, const uni_char *ns_uri, const uni_char *name, BOOL is_xml);

	virtual ~DOM_TagsCollectionFilter();

	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;

	virtual Type GetType() { return TYPE_TAGS; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return base && base->IsNameSearch(name); }
	virtual BOOL CanSkipChildren() { return base && base->CanSkipChildren(); }
	virtual BOOL CanIncludeGrandChildren() { return !base || base->CanIncludeGrandChildren(); }
	virtual BOOL IsMatched(unsigned collections);

	virtual BOOL IsEqual(DOM_CollectionFilter *other);

protected:
	BOOL UseCaseSensitiveCompare(HTML_Element* elm) { return is_xml || elm->GetNs() != Markup::HTML; }
	BOOL TagNameMatches(HTML_Element* elm);

	DOM_CollectionFilter *base;
	BOOL any_ns, any_name;
	const uni_char *ns_uri, *name;
	BOOL is_xml;
};

class DOM_Collection;
class DOM_HTMLCollection;

class DOM_NodeCollection
	: public DOM_Object,
	  public DOM_CollectionLink
{
protected:
	DOM_NodeCollection(BOOL reusable);

	OP_STATUS SetFilter(const DOM_CollectionFilter &filter_prototype);
	/**< Sets the collection's filter by cloning 'filter_prototype'.

	     @param filter_prototype Filter prototype.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM.
	*/

public:
	static OP_STATUS Make(DOM_NodeCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, BOOL include_root, BOOL has_named_properties, const DOM_CollectionFilter &filter_prototype, BOOL reusable);
	/**< Creates a node collection. The collection represents all descendants of the root element (and optionally the root element itself)
	     filtered by the predicate represented via the 'filter_prototype'. The root of an existing collection may later on be adjusted using SetRoot().

	     @param collection Set to the created collection.
	     @param environment DOM environment.
	     @param class_name The collection object's class name.
	     @param root New root.
	     @param include_root If TRUE, the root element is included in the
	                         collection; if FALSE, only the root element's
	                         children are included.
	     @param has_named_properties Set to TRUE if it should be possible to
	                                 lookup elements by element name attributes
	                                 through this collection.
	     @param reusable Set to TRUE if this collection might be reused through
	                     the collection cache. Use FALSE for singleton
	                     collections to avoid polluting the collection cache.
	     @param filter_prototype Filter prototype.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM.
	*/

	void SetRoot(DOM_Node *root, BOOL include_root);
	/**< Sets the collection's root.

	     @param root New root.
	     @param include_root If TRUE, the root element is included in the
	                         collection; if FALSE, only the root element's
	                         children are included.
	*/

	void SetCreateSubcollections();
	/**< Signal that this collection should create subcollections when
	     a lookup by name finds more than one element instead of just
	     returning the first match.
	*/

	void SetOwner(DOM_Object *owner);

	void ElementCollectionStatusChanged(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections);
	/**< Called when elements within the tree rooted in 'affected_tree_root' has
	     changed in ways that might invalidate caches in this collection.  There
	     are three distinct cases in which this function is called: an element
	     is inserted into another element, an element is removed from its
	     parent, and key attributes on an element (such as ID or NAME) has been
	     added, remove or modified.

	     The 'affected tree root' parameter is the root of the tree in which the
	     change occured.  In the case of an inserted (added) element, it is the
	     root of the tree into which the element was inserted.  In the case of a
	     removed element, it is the root of the tree from which the element was
	     removed.

	     The 'element' parameter is the element that was inserted, removed or
	     had its attributes changed.  In the case of insertion and removal, all
	     the children of this element are of course indirectly affected.

	     Calls ElementCollectionStatusChangedSpecific from inheriting classes.
	*/

	void ResetRoot(HTML_Element *affected_tree_root);

	void AddToIndexCache(unsigned int index, HTML_Element *new_element);
	/**< Explicitly seed the collection's cache with an element. The caller is assumed to have correct (but validatable by the collection) association
	     mapping from index to element. */

	void RefreshIndexCache(HTML_Element *new_element);
	/**< Supply new candidate to the collection's cache. If the cache has it marked as being invalid, revert that state and consider it valid again. */

	void SetUseFormNr() { use_form_nr = TRUE; }
	void SetNamePolicy(BOOL new_check_name, BOOL new_check_id) { check_name = new_check_name; check_id = new_check_id; }
	void SetPreferWindowObjects() { prefer_window_objects = TRUE; }

	virtual ~DOM_NodeCollection();
	virtual void GCTrace();

	virtual ES_GetState GetCollectionProperty(DOM_Collection *collection, BOOL allowed, const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetLength(int &length, BOOL allowed);
	virtual int Length();

	virtual HTML_Element *Item(int index);
	HTML_Element *NamedItem(OpAtom atom, const uni_char *name, BOOL *found_many = NULL);

	DOM_Node *GetRoot() { return root; }
	HTML_Element *GetRootElement();

	DOM_CollectionFilter* GetFilter() { return filter; }
	/**< Returns a pointer the filter, which is a
	     clone of the filter sent into the Make method. */

	BOOL IsElementCollection() { return !filter || filter->IsElementFilter(); }
	/**< Returns whether this is an Element Collection or not. If it's
	     an Element Collection it might not be informed about changes to
	     non-Element Nodes. */

	BOOL IsSameCollection(DOM_Node *root, BOOL include_root, DOM_CollectionFilter *filter);
	/**< Returns TRUE if collection represents a collection over the same (root,filter) pair. */

	HTML_Element *GetTreeRoot() { return tree_root; }

#ifdef _DEBUG
	HTML_Element* DebugGetTreeRoot() { return tree_root; }
#endif // _DEBUG

	void UnregisterCollection(DOM_Collection *collection);
	OP_STATUS RegisterCollection(DOM_Collection *collection);

protected:
	friend class DOM_Collection;
	friend class DOM_HTMLCollection;
	friend class DOM_HTMLOptionsCollection;

	void RecalculateTreeRoot();

	OP_STATUS GetCachedSubcollection(DOM_Collection *&subcollection, const uni_char *name);
	OP_STATUS SetCachedSubcollection(const uni_char *name, DOM_Collection *subcollection);

	virtual void ElementCollectionStatusChangedSpecific(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections);
	/**< Called by ElementCollectionStatusChanged() when cache invalidation
	     could be needed. */

	ES_GetState SetElement(ES_Value *value, HTML_Element *element, DOM_Runtime *origining_runtime);

	int GetFormNr();
	HTML_Element *GetForm();

	int GetTags(const uni_char *name, ES_Value *return_value);

	DOM_Node *root;
	DOM_CollectionFilter *filter;
	DOM_Object *subcollections;
	HTML_Element *tree_root;
	DOM_Object *owner;

	unsigned include_root:1;
	unsigned create_subcollections:1;
	unsigned check_name:1;
	unsigned check_id:1;
	unsigned has_named_element_properties:1;
	unsigned use_form_nr:1;
	unsigned prefer_window_objects:1;
	unsigned needs_invalidation:1;

	OpAtom missing_names[3];

	void UpdateSerialNr();
	int serial_nr;
	BOOL cached_valid;
	int cached_index, cached_length;
	HTML_Element *cached_index_match;

	List<DOM_Collection> collection_listeners;
};

/* The DOM object representation of a collection; a NodeList, HTMLCollection or HTMLOptionsCollection being some examples.
 * It wraps an underlying DOM_NodeCollection, which might be used and shared by multiple collection objects.
 * Mutations of the collection set object has local effects.
 */
class DOM_Collection
	: public DOM_Object
	, public ListElement<DOM_Collection>
{
public:
	static OP_STATUS Make(DOM_Collection *&collection, DOM_EnvironmentImpl *environment, const char *class_name, DOM_Node *root, BOOL include_root, BOOL try_sharing, const DOM_CollectionFilter &filter_prototype);
	/**< Creates a collection object for the given (root, filter) pairing.

	     @param [out] collection Set to the created collection.
	     @param environment DOM environment.
	     @param class_name The collection object's class name.
	     @param root New root.
	     @param include_root If TRUE, the root element is included in the
	                         collection; if FALSE, only the root element's
	                         children are included.
	     @param try_sharing set to TRUE to attempt sharing with a previously created node collection.
	     @param filter_prototype Filter prototype.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM.
	*/
	static OP_STATUS MakeNodeList(DOM_Collection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, BOOL include_root, BOOL try_sharing, const DOM_CollectionFilter &filter_prototype);
	/**< Convenience method for creating NodeList collections (the common case.) Same interpretation of arguments and result as for DOM_Collection::Make(). */

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState	GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual int Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_COLLECTION || DOM_Object::IsA(type); }
	virtual void GCTrace();
	virtual BOOL TypeofYieldsObject() { return TRUE; }

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexedPropertiesLength(unsigned &index, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(item);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

	DOM_NodeCollection *GetNodeCollection() { return node_collection; }
	void SetCreateSubcollections();

	int Length()
	{
		return node_collection->Length();
	}

	HTML_Element *Item(int index)
	{
		return node_collection->Item(index);
	}

	DOM_Node *GetRoot()
	{
		return node_collection->GetRoot();
	}

	void SetRoot(DOM_Node *root, BOOL include_root)
	{
		node_collection->SetRoot(root, include_root);
	}

	void SetPreferWindowObjects()
	{
		node_collection->SetPreferWindowObjects();
	}

	DOM_CollectionFilter* GetFilter()
	{
		return node_collection->filter;
	}

	void SetOwner(DOM_Object *owner)
	{
		node_collection->SetOwner(owner);
	}

protected:
	friend class DOM_NodeCollection;
	friend class DOM_PropertyNodeListCollection;

	DOM_NodeCollection *node_collection;

	virtual ~DOM_Collection();

	static OP_STATUS Make(DOM_Collection *&collection, DOM_EnvironmentImpl *environment, const uni_char *uni_class_name, const char *class_name, DOM_Node *root, BOOL include_root, BOOL try_sharing, const DOM_CollectionFilter &filter_prototype, DOM_Runtime::Prototype &prototype);
};

/** A DOM_Collection but with namedItem() (and tags()) added. */
class DOM_HTMLCollection
	: public DOM_Collection
{
public:
	DOM_DECLARE_FUNCTION(namedItem);
	DOM_DECLARE_FUNCTION(tags);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

class DOM_HTMLOptionsCollection
	: public DOM_Collection
{
public:
	static OP_STATUS Make(DOM_HTMLOptionsCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Element *select_element);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState	PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object* restart_object);
	virtual ES_PutState	PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState	PutIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_OPTIONS_COLLECTION || DOM_Collection::IsA(type); }

	DOM_DECLARE_FUNCTION_WITH_DATA(addOrRemove);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };

	void AddToIndexCache(unsigned int index, HTML_Element *new_element) { node_collection->AddToIndexCache(index, new_element); }
	void RefreshIndexCache(HTML_Element *new_element) { node_collection->RefreshIndexCache(new_element); }

protected:
	OP_STATUS SetLength(int length);
	int AddOption(HTML_Element *option, int index);
	void RemoveOption(int index);
};

#endif /* _DOM_HTML_COLLECTION_ */
