/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#if !defined(DOM_HTML_MICRODATA)
#define DOM_HTML_MICRODATA

#include "modules/dom/domtypes.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domstringlist.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/logdoc/html.h"
#include "modules/logdoc/namespace.h"
#include "modules/ecmascript/json_serializer.h"

/**
 * This class is used to retrieve Microdata properties of a given HTML element,
 * which is a Microdata item (have itemScope attribute specified).  If it's not,
 * there are no properties.
 *
 * It implements "crawl the properties" algorithm (the spirit, not the letter)
 * from the Microdata specification.
 *
 * It traverses Microdata graph specified over HTML tree, beginning from a given
 * HTML_Element root_element and stores resulting properties in a given
 * root_properties_result.
 *
 * Instances of this class are meant to have rather short lifespan.
 */
class MicrodataTraverse
{
public:
	/**
	 *  @param[in] root_element The HTML element on which Microdata properties'
	 *  crawling should be performed.  Must not be NULL.
	 *
	 *  @param[in] root_properties_result The collection, where resulting
	 *  Microdata properties will be stored after calling
	 *  CrawlMicrodataProperties().
	 **/
	MicrodataTraverse(HTML_Element* root_element, OpVector<HTML_Element>& root_properties_result)
		: m_root_element(root_element)
		, m_root_properties_result(root_properties_result)
		, m_current_ref(0)
	{
		OP_ASSERT(m_root_element);
	}

	/**
	 * Fills m_root_properties_result collection with Microdata properties of
	 * m_root_element.
	 *
	 * @return OOM or OK.
	 **/
	OP_STATUS CrawlMicrodataProperties();

	/**
	 * Converts Microdata properties of single HTML element to JSON string.  If
	 * the element is not toplevel Microdata item, it results in "{"items":[]}"
	 * string.  Otherwise, the JSON string looks like:
	 *
	 * {"items":[{"type":"itemtype1","id":"itemid1","properties":{"propname1":["propval1"],"propname2":["propval2"]}}]}
	 *
	 * @param [in] helm The HTML element which Microdata properties are
	 *             JSON-ified.
	 *
	 * @param[in] frm_doc Utility, needed as resolving url of HTML element
	 *            attributes needs LogicalDocument and fetching string contents
	 *            of an HTML element is done by DOM_Environment, both reachable
	 *            from FramesDocument.  Moreover, JSON serialization is
	 *            performed by ES_Runtime.  Thus the FramesDocument should have
	 *            DOM_Environment created when calling this method.
	 *
	 * @param [out] result_json Resulting JSON string of Microdata properties
	 *              of given HTML element.
	 *
	 * @result OpStatus::ERR if there is NULL parameter passed or
	 *         FramesDocument has no DOM_Environment, or OpStatus::OOM, or
	 *         OpStatus::OK.
	 **/
	static OP_STATUS ExtractMicrodataToJSON(HTML_Element* helm,
			FramesDocument* frm_doc,
			TempBuffer& result_json);

	/**
	 * Converts Microdata properties of the selection interval <start_helm,
	 * end_helm) (including start_helm, excluding end_helm) to JSON string.
	 * The JSON string looks like:
	 *
	 * {"items":[{"type":"itemtype1","id":"itemid1","properties":{"propname1":["propval1"],"propname2":["propval2"]}},{"properties":{}}]}
	 *
	 * @param [in] start_helm Beginnging of the interval of HTML elements to
	 *             iterate over.  Must not be NULL.
	 *
	 * @param [in] end_helm End of the HTML elements' interval.
	 *             Must not be NULL.
	 *
	 * @param [in] frm_doc See ExtractMicrodataToJSON().
	 *
	 * @param [out] result_json Resulting JSON string of Microdata items
	 *              contained in the interval is stored in the buffer.
	 *
	 * @result See ExtractMicrodataToJSON().
	 **/
	static OP_STATUS ExtractMicrodataToJSONFromSelection(
			HTML_Element* start_helm,
			HTML_Element* end_helm,
			FramesDocument* frm_doc,
			TempBuffer& result_json);

	/**
	 * Converts Microdata properties of m_root_element to JSON.
	 *
	 * Calls CrawlMicrodataProperties() inside and recursively steps inside
	 * subitems.  If of Microdata items is detected, then Microdata properties
	 * of the most inner item iside the loop (a "leaf" of the loop) are
	 * replaced by "ERROR" string.
	 *
	 * @param[in, out] result_json_serializer See TraverseAndAppendJSON().
	 *
	 * @param[in] frm_doc See ExtractMicrodataToJSON().
	 *
	 * @return OOM or OK.
	 **/
	OP_STATUS ToJSON(JSONSerializer& result_json_serializer, FramesDocument& frm_doc);

private:
	/**
	 * Private twin.  It can be called recursively with operating data created
	 * by its public twin once for all.
	 *
	 * @param[in, out] result_json_serializer See TraverseAndAppendJSON().
	 *
	 * @param[in] frm_doc See ExtractMicrodataToJSON().
	 *
	 * @param[in, out] jsonified_items_in_path Collection of Microdata items
	 *                 inside which JSON-ification is currently performed.
	 *                 Used to detect loops.
	 *
	 * @param[out] item_loop_found Set to TRUE if m_root_element is found
	 *             inside jsonified_items_in_path collection.
	 *
	 * @return OOM or OK.
	 **/
	OP_STATUS ToJSON(JSONSerializer& result_json_serializer,
			FramesDocument& frm_doc,
			OpPointerSet<HTML_Element>& jsonified_items_in_path,
			BOOL& item_loop_found);

	/**
	 * Auxiliary method.  Performs MicrodataTraverse on given HTML element,
	 * JSON-ifies its properties and appends the string to given buffer.
	 *
	 * @param[in] helm_microdata_item HTML element, which must be Microdata item.
	 *
	 * @param[in] frm_doc FramesDocument in which the helm_microdata_item
	 *            exists.  See ExtractMicrodataToJSON().
	 *
	 * @param[out] result_properties See constructor of MicrodataTraverse.
	 *
	 * @param[in, out] result_json_serializer JSONSerializer operating on
	 * TempBuffer passed to ExtractMicrodataToJSON().
	 *
	 **/
	static OP_STATUS TraverseAndAppendJSON(HTML_Element* helm_microdata_item,
			FramesDocument& frm_doc,
			OpVector<HTML_Element>& result_properties,
			JSONSerializer& result_json_serializer);

	/**
	 * Fills m_refs with HTML_Elements from home subtree of m_root_element,
	 * which are referenced in ids parameter.
	 *
	 * @param[in] ids Collection of ids which should be searched for.
	 *
	 * @return OOM or OK.
	 **/
	OP_STATUS FindElementsFromHomeSubtreeWithIds(const OpAutoVector<OpString>* ids);

	/**
	 * Traverse given element and its subtree in search for Microdata
	 * properties.
	 *
	 * @param[in] ref_el Element referenced by m_root_element in itemRef attribute.
	 *
	 * @return OOM or OK.
	 **/
	OP_STATUS Traverse(HTML_Element* ref_el);

	/**
	 * Compare HTML elements in tree order. Used in qsort over collection.
	 **/
	static int CompareNodeOrder(const HTML_Element** first, const HTML_Element** second);

	HTML_Element* m_root_element;
	/**< Element (possibly Microdata item) which Microdata properties are
	 * searched for. */

	OpVector<HTML_Element>& m_root_properties_result;
	/**< Microdata properties of m_root_element. */

	OpVector<HTML_Element> m_visitedrefs;
	/**< Temporary collection of HTML elements, referenced by m_root_element,
	 * which were already traversed.  Populated during traversing. */

	OpVector<HTML_Element> m_refs;
	/**< Collection of HTML elements referenced by m_root_element with itemRef
	 * attribute.  Populated before traversing. */

	UINT32 m_current_ref;
	/**< Index in m_refs collection.  Currently traversed HTML element. */
};

/**
 * This class represents live collection of HTML elements which are Microdata
 * properties of Microdata item.
 *
 * It contains cache of collection elements populated and updated lazily, for
 * example when length property is requested or its subcollection (got with
 * namedItem() or with names) is accessed.
 *
 * Cache is marked dirty when HTML tree is altered - element is added or removed
 * or some attributes (such as id, itemprop, itemscope, itemref) are added or
 * removed anywhere in the document tree (or home subtree in case of an item which
 * is detached from document tree).
 *
 * It is used as base_properties collection by the
 * DOM_NodeCollection_PropertyNodeList (subcollection) and as internals of the
 * DOM_HTMLPropertiesCollection (which represents HTMLPropertiesCollection in
 * DOM).
 *
 * DOM_NodeCollectionMicrodataProperties::RegisterCollection() can be called
 * only with argument of type DOM_HTMLPropertiesCollection.
*/
class DOM_NodeCollectionMicrodataProperties
	: public DOM_NodeCollection
{
protected:
	DOM_NodeCollectionMicrodataProperties()
		: DOM_NodeCollection(FALSE)
	{
	}

public:
	static OP_STATUS Make(DOM_NodeCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root);

	// Implements DOM_NodeCollection.
	virtual int Length();
	virtual HTML_Element *Item(int index); // Returns index-th Microdata property in tree order.
	virtual ES_GetState GetLength(int &length, BOOL allowed);
	virtual ES_GetState GetCollectionProperty(DOM_Collection *collection, BOOL allowed, const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual void ElementCollectionStatusChangedSpecific(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections);

	/**
	 * There can be number of PropertyNodeLists connected with given
	 * HTMLPropertiesCollection.
	 *
	 * This method fits general DOM collection design, because property_name is
	 * key here, used in GetCachedSubcollection().
	 *
	 * @param[in] property_name Filter for Microdata properties from
	 * HTMLPropertiesCollection.namedItem(property_name).
	 *
	 * @param[out] subcollection Resulting live collection with property_name
	 * filter memorized.
	 *
	 * @return If subcollection with given filter wasn't yet created,
	 * allocates, so can return ERR_NO_MEMORY.
	 **/
	OP_STATUS GetPropertyNodeList(const uni_char *property_name, DOM_Collection *&subcollection);

	/**
	 * Make sure collection is properly cached.
	 *
	 * If needed, crawls all the properties of Microdata item.
	 *
	 * @return TRUE is update was needed, FALSE otherwise.
	 **/
	BOOL Update();

	/**
	 * Check if Microdata item has given Microdata property.
	 *
	 * @param[in] property_name To search for.
	 *
	 * @return TRUE if this collection contains property_name Microdata
	 * property.
	 **/
	BOOL ContainsMicrodataProperty(const uni_char* property_name);

	/**
	 * Getter for cache.  To be used only if collection is up to date.
	 *
	 * @return i-th Microdata property in tree order.
	 **/
	HTML_Element* Get(int i) { return m_root_properties_cache.Get(i); }

protected:
	OpVector<HTML_Element> m_root_properties_cache;
	/**< Cache of Microdata properties of root, stored in tree order. */
};

/**
 * This class represents live view (or subcollection) of Microdata properties
 * of Microdata item.
 *
 * It uses DOM_NodeCollectionMicrodataProperties as a base_properties for
 * fetching its live content before filtering it.  Filtering is done with
 * "property_name_filter" member, which comes at creation time (in js: var
 * subcollection = html_element.properties.namedItem(name)).
 *
 * It is used by DOM_PropertyNodeList_Collection which represents
 * PropertyNodeList in DOM.
 **/
class DOM_NodeCollection_PropertyNodeList
	: public DOM_NodeCollection
{
public:
	static OP_STATUS Make(DOM_NodeCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, DOM_NodeCollectionMicrodataProperties* base_properties, const uni_char* property_name_filter);
	DOM_NodeCollection_PropertyNodeList(DOM_NodeCollectionMicrodataProperties* base_properties, const uni_char* property_name_filter)
		: DOM_NodeCollection(FALSE),
		  m_base_properties(base_properties),
		  m_property_name_filter(property_name_filter)
	{
	}

	// Implements DOM_NodeCollection.
	virtual int Length();
	virtual HTML_Element *Item(int index);
	virtual ES_GetState GetLength(int &length, BOOL allowed);
	virtual ES_GetState GetCollectionProperty(DOM_Collection *collection, BOOL allowed, const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual void ElementCollectionStatusChangedSpecific(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections);

protected:
	DOM_NodeCollectionMicrodataProperties *m_base_properties;
	/**< Base collection to be filtered. */

	const uni_char *m_property_name_filter;
	/**< Filter for base collection. */
};

/**
 * This class represents HTMLPropertiesCollection in DOM.
 *
 * It contains *all* HTML elements which are Microdata properties of Microdata
 * item.
 *
 * Inherits HTMLCollection, overrides inherited namedItem() - it returns
 * PropertyNodeList - and has names attribute which is a live collection of
 * names of all Microdata properties.
 **/
class DOM_HTMLPropertiesCollection
	: public DOM_Collection
	, public DOM_DOMStringList::Generator
{
public:
	DOM_HTMLPropertiesCollection()
		: m_is_call(FALSE)
		, m_cached_names_length(-1)
		, m_cached_names_index(-1)
	{}
	static OP_STATUS Make(DOM_HTMLPropertiesCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Element *root);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual int Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_PROPERTIES_COLLECTION || DOM_Collection::IsA(type); }

	/* Implements DOM_StringList::Generator for "names" property. */
	virtual unsigned StringList_length();
	virtual OP_STATUS StringList_item(int index, const uni_char *&name);
	virtual BOOL StringList_contains(const uni_char *string);

	/* Used when underlying collection changes. */
	void CacheInvalid() { m_cached_names_length = -1; m_cached_names_index = -1; }

	DOM_DECLARE_FUNCTION(item);
	DOM_DECLARE_FUNCTION(namedItem);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

protected:
	/**
	 * Needed to disallow creation of empty PropertyNodeList when referencing
	 * helm.properties.nonExistantMicrodataProperty.  And in the same time
	 * allow creation of empty PropertyNodeList when calling "caller
	 * namedItem()", that is helm.properties('nonExistantMicrodataProperty').
	 **/
	BOOL m_is_call;

	// Cache for DOM_StringList "names" attribute.
	int m_cached_names_length;
	int m_cached_names_index;
	OpString m_cached_names_index_value;
};

/**
 * This class represents PropertyNodeList in DOM.
 *
 * It contains subset of HTML elements which are Microdata properties of
 * Microdata item.
 *
 * Inherits NodeList, and has getValues() methods which references itemValue
 * DOM properties of all HTML elements in this collection.
 *
 **/
class DOM_PropertyNodeList_Collection
	: public DOM_Collection
{
public:
	static OP_STATUS Make(DOM_PropertyNodeList_Collection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, DOM_NodeCollectionMicrodataProperties *base_properties, const uni_char* property_name);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_PROPERTY_NODE_LIST_COLLECTION || DOM_Collection::IsA(type); }

	DOM_DECLARE_FUNCTION(item);
	DOM_DECLARE_FUNCTION(getValues);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };
};

/**
 * Filter for document.getItems() for Microdata toplevel items live collection.
 **/
class DOM_ToplevelItemCollectionFilter
	: public DOM_CollectionFilter
{
public:
	DOM_ToplevelItemCollectionFilter() {}
	virtual ~DOM_ToplevelItemCollectionFilter();

	// Implements DOM_CollectionFilter.
	virtual void Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc);
	virtual DOM_CollectionFilter *Clone() const;
	virtual Type GetType() { return TYPE_MICRODATA_TOPLEVEL_ITEM; }
	virtual BOOL IsNameSearch(const uni_char *&name) { return FALSE; }
	virtual BOOL CanSkipChildren() { return FALSE; }
	virtual BOOL CanIncludeGrandChildren() { return TRUE; }
	virtual BOOL IsMatched(unsigned collections);
	virtual BOOL IsEqual(DOM_CollectionFilter *other);

	OP_STATUS AddType(const uni_char* to_add, int len = -1);
private:
	/**
	 * Contains tokens from "typeNames" attribute split on whitespaces from
	 * document.getItems(typeNames) call.
	 **/
	OpVector<OpString> m_itemtypes;
};

#endif // DOM_HTML_MICRODATA

