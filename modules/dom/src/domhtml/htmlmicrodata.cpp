/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"


#include "modules/dom/src/domhtml/htmlmicrodata.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/opatom.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/stringtokenlist_attribute.h"
#include "modules/logdoc/link.h"

//---
// JSON-ification.
//---

/* static */ OP_STATUS
MicrodataTraverse::ExtractMicrodataToJSON(HTML_Element* helm,
		FramesDocument* frm_doc,
		TempBuffer& result_json)
{
	if (!frm_doc || !frm_doc->GetDOMEnvironment() || !frm_doc->GetESRuntime() || !helm)
		return OpStatus::ERR;

	JSONSerializer serializer(result_json);
	RETURN_IF_ERROR(serializer.EnterObject());
	RETURN_IF_ERROR(serializer.AttributeName(UNI_L("items")));
	RETURN_IF_ERROR(serializer.EnterArray());
	OpVector<HTML_Element> iter_result_properties;

	if (helm->IsToplevelMicrodataItem())
		RETURN_IF_ERROR(TraverseAndAppendJSON(helm, *frm_doc, iter_result_properties, serializer));

	RETURN_IF_ERROR(serializer.LeaveArray());
	RETURN_IF_ERROR(serializer.LeaveObject());
	return OpStatus::OK;
}

/* static */ OP_STATUS
MicrodataTraverse::ExtractMicrodataToJSONFromSelection(HTML_Element* start_helm,
		HTML_Element* end_helm,
		FramesDocument* frm_doc,
		TempBuffer& result_json)
{
	if (!frm_doc || !frm_doc->GetDOMEnvironment() || !frm_doc->GetESRuntime() || !start_helm || !end_helm)
		return OpStatus::ERR;

	JSONSerializer serializer(result_json);
	RETURN_IF_ERROR(serializer.EnterObject());
	RETURN_IF_ERROR(serializer.AttributeName(UNI_L("items")));
	RETURN_IF_ERROR(serializer.EnterArray());

	OpVector<HTML_Element> iter_result_properties;

	// If an interval (selection) is processed, check whether start element is
	// inside a toplevel Microdata item (TMI).  If it is, then add Microdata
	// properties of such parent TMI to results.  (But beware:  get all
	// Microdata properties of the parent TMI, but don't transgress selection
	// limits, and don't get Microdata properties of TMIs, which are children
	// of the parent in HTML tree.  In other words, when adding TMI, which is
	// parent of beginning of selection, don't add Microdata properties of
	// TMIs, which are siblings in the HTML tree of the beginning of selection,
	// to results).
	//
	// To make things more complex, search for TMIs all the way up from
	// beginning of selection to to the document root.  The order of Microdata
	// properties of parent TMIs in resulting JSON must be from top of the
	// document, to bottom.
	{
		OpVector<HTML_Element> parents;
		HTML_Element* iter_parent = start_helm;
		while (iter_parent)
		{
			if (iter_parent->IsToplevelMicrodataItem())
				RETURN_IF_ERROR(parents.Add(iter_parent));
			iter_parent = iter_parent->ParentActual();
		}
		for (int i = parents.GetCount() - 1; i >= 0; i--)
			RETURN_IF_ERROR(TraverseAndAppendJSON(parents.Get(i), *frm_doc, iter_result_properties, serializer));
	}

	HTML_Element* iter_subtree = start_helm;
	do
	{
		if (iter_subtree->IsToplevelMicrodataItem())
			RETURN_IF_ERROR(TraverseAndAppendJSON(iter_subtree, *frm_doc, iter_result_properties, serializer));
		iter_subtree = iter_subtree->NextActual();
	} while (iter_subtree && iter_subtree != end_helm);

	RETURN_IF_ERROR(serializer.LeaveArray());
	RETURN_IF_ERROR(serializer.LeaveObject());
	return OpStatus::OK;
}

struct MicrodataNameValueJSON
{
	// Name of Microdata property.
	const uni_char* name;

	// JSON-ified value of Microdata property, including quotation marks,
	// separated by commas, sorted in order of appearance.
	TempBuffer value_json;
};

/**
 * Holds collection of pairs (Microdata property name, JSON-ified Microdata
 * property value).  Uses MicrodataNameValueJSON.
 **/
class MicrodataPropertiesJSONData
{
public:
	~MicrodataPropertiesJSONData() { properties_by_name.DeleteAll(); }

	/**
	 * Add the value to property named name.
	 *
	 * @param[in] name Name of the property.
	 *
	 * @param[in] value New value of the property.  One property can have many
	 *                  values.
	 *
	 * @param[in] plain_text If TRUE, insert value as is.  Otherwise, escape
	 *            special characters found in value.
	 *
	 * @return OK or OOM.
	 **/
	OP_STATUS Add(const uni_char* name, const uni_char* value, BOOL plain_text);

	UINT32 GetCount() { return properties_by_name.GetCount(); }

	MicrodataNameValueJSON* Get(int i) { return properties_by_name.Get(i); }
private:
	OpVector<MicrodataNameValueJSON> properties_by_name;
	/**< Preserve properties' order by names. */
};

OP_STATUS
MicrodataPropertiesJSONData::Add(const uni_char* name, const uni_char* value_to_add, BOOL plain_text)
{
	for (UINT32 i = 0; i < properties_by_name.GetCount(); i++)
	{
		MicrodataNameValueJSON* ith_data = properties_by_name.Get(i);
		if (uni_str_eq(ith_data->name, name))
		{
			RETURN_IF_ERROR(ith_data->value_json.Append(UNI_L(",")));
			if (plain_text)
				return ith_data->value_json.Append(value_to_add);
			else
			{
				JSONSerializer serializer(ith_data->value_json);
				return serializer.String(value_to_add);
			}
		}
	}

	OpAutoPtr<MicrodataNameValueJSON> new_data(OP_NEW(MicrodataNameValueJSON, ()));
	if (!new_data.get())
		return OpStatus::ERR_NO_MEMORY;
	new_data->name = name;
	if (plain_text)
		RETURN_IF_ERROR(new_data->value_json.Append(value_to_add));
	else
	{
		JSONSerializer serializer(new_data->value_json);
		RETURN_IF_ERROR(serializer.String(value_to_add));
	}
	return properties_by_name.Add(new_data.release());
}

OP_STATUS
MicrodataTraverse::ToJSON(JSONSerializer& result_json_serializer, FramesDocument& frm_doc)
{
	OpPointerSet<HTML_Element> jsonified_items_in_path;
	BOOL item_loop_found = FALSE;
	return ToJSON(result_json_serializer, frm_doc, jsonified_items_in_path, item_loop_found);
}

OP_STATUS
MicrodataTraverse::ToJSON(
	JSONSerializer& serializer,
	FramesDocument& frm_doc,
	OpPointerSet<HTML_Element>& jsonified_items_in_path,
	BOOL& item_loop_found)
{
	// Exclude loops of Microdata items.
	if (jsonified_items_in_path.Contains(m_root_element))
	{
		item_loop_found = TRUE;
		return OpStatus::OK;
	}
	item_loop_found = FALSE;
	RETURN_IF_ERROR(jsonified_items_in_path.Add(m_root_element));

	RETURN_IF_ERROR(serializer.EnterObject());

	// "typename" : "itemtype_attr"
	const uni_char* itemtype = m_root_element->GetStringAttr(ATTR_ITEMTYPE);
	if (itemtype)
	{
		RETURN_IF_ERROR(serializer.AttributeName(UNI_L("type")));
		RETURN_IF_ERROR(serializer.String(itemtype));
	}
	// "idname" : "itemid_attr"
	const uni_char* itemid = m_root_element->GetStringAttr(ATTR_ITEMID);
	if (itemid)
	{
		RETURN_IF_ERROR(serializer.AttributeName(UNI_L("id")));
		RETURN_IF_ERROR(serializer.String(itemid));
	}

	// Retrieve Microdata properties and group them by properties' names.
	MicrodataPropertiesJSONData properties_json;
	RETURN_IF_ERROR(CrawlMicrodataProperties());
	TempBuffer local_buffer; // Used as subitem_json or operating buffer.
	for (UINT32 i_property = 0; i_property < m_root_properties_result.GetCount(); i_property++)
	{
		HTML_Element* ith_property_helm = m_root_properties_result.Get(i_property);
		const StringTokenListAttribute* ith_property_names = ith_property_helm->GetItemPropAttribute();
		if (!ith_property_names->GetTokenCount())
			continue;
		const uni_char* property_value_string;
		const uni_char* is_subitem = ith_property_helm->GetStringAttr(ATTR_ITEMSCOPE);
		if (is_subitem)
		{
			// Recursively get JSON-ified properties which are subitems.
			OpVector<HTML_Element> ith_properties;
			BOOL item_loop_found = FALSE;
			local_buffer.Clear();
			JSONSerializer local_json_serializer(local_buffer);
			MicrodataTraverse subitem_mdt(ith_property_helm, ith_properties);
			RETURN_IF_ERROR(subitem_mdt.ToJSON(local_json_serializer, frm_doc, jsonified_items_in_path, item_loop_found));
			if (item_loop_found)
				property_value_string = UNI_L("\"ERROR\"");
			else
				property_value_string = local_buffer.GetStorage();
		}
		else
		{
			local_buffer.Clear();
			property_value_string = ith_property_helm->GetMicrodataItemValue(local_buffer, &frm_doc);
		}
		const OpAutoVector<OpString>& names = ith_property_names->GetTokens();
		for (UINT32 jth_name = 0; jth_name < names.GetCount(); jth_name++)
			properties_json.Add(names.Get(jth_name)->CStr(), property_value_string, is_subitem ? TRUE : FALSE);
	}

	// JSON-ify fetched properties.
	// "properties":{"property_name" : ["value1", "value12", ...], "property_name2" : ["value12", "value2", ...], ...}}
	RETURN_IF_ERROR(serializer.AttributeName(UNI_L("properties")));
	RETURN_IF_ERROR(serializer.EnterObject());
	for (UINT32 i_property_name = 0; i_property_name < properties_json.GetCount(); i_property_name++)
	{
		MicrodataNameValueJSON* ith_property = properties_json.Get(i_property_name);
		RETURN_IF_ERROR(serializer.AttributeName(ith_property->name));
		RETURN_IF_ERROR(serializer.EnterArray());
		RETURN_IF_ERROR(serializer.PlainString(ith_property->value_json.GetStorage()));
		RETURN_IF_ERROR(serializer.LeaveArray());
	}
	RETURN_IF_ERROR(serializer.LeaveObject());
	RETURN_IF_ERROR(serializer.LeaveObject());

	jsonified_items_in_path.Remove(m_root_element);
	return OpStatus::OK;
}

/* static */ OP_STATUS
MicrodataTraverse::TraverseAndAppendJSON(HTML_Element* helm_microdata_item,
		FramesDocument& frm_doc, OpVector<HTML_Element>& result_properties,
		JSONSerializer& result_json_serializer)
{
	result_properties.Clear();

	MicrodataTraverse mdt(helm_microdata_item, result_properties);
	RETURN_IF_ERROR(mdt.ToJSON(result_json_serializer, frm_doc));
	return OpStatus::OK;
}

//---
// Collections.
//---

/* static */ OP_STATUS
DOM_NodeCollectionMicrodataProperties::Make(DOM_NodeCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	if (OpStatus::IsMemoryError(DOMSetObjectRuntime(collection = OP_NEW(DOM_NodeCollectionMicrodataProperties, ()), runtime)))
	{
		collection = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	collection->SetRoot(root, TRUE);
	return OpStatus::OK;
}

/* virtual */ void
DOM_NodeCollectionMicrodataProperties::ElementCollectionStatusChangedSpecific(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections)
{
	// This method is called when an element is changed, inserted into or deleted
	// from affected_tree_root. Collection needs to be nofified if changed
	// element has one of attributes: id, itemprop, itemscope or itemref.

	if (collections & DOM_Environment::COLLECTION_MICRODATA_PROPERTIES)
	{
		cached_valid = FALSE;
		cached_length = -1;
		for (DOM_Collection *collection = collection_listeners.First(); collection; collection = collection->Suc())
		{
			DOM_HTMLPropertiesCollection* typed_collection = static_cast<DOM_HTMLPropertiesCollection*>(collection);
			typed_collection->CacheInvalid();
		}
	}
}

/* static */ int
MicrodataTraverse::CompareNodeOrder(const HTML_Element** first, const HTML_Element** second)
{
	if (*first == *second)
		return 0;
	if ((*first)->Precedes(*second))
		return -1;
	return 1;
}

OP_STATUS
MicrodataTraverse::FindElementsFromHomeSubtreeWithIds(const OpAutoVector<OpString>* ids)
{
	OP_ASSERT(ids);
	LogicalDocument *logdoc = m_root_element->GetLogicalDocument();
	if (logdoc && logdoc->GetRoot()->IsAncestorOf(m_root_element))
		// Home subtree = whole document tree.
		for (UINT32 i_ref = 0; i_ref < ids->GetCount(); i_ref++)
		{
			HTML_Element *named_element = NULL;
			NamedElementsIterator iterator;
			int found = logdoc->SearchNamedElements(iterator, NULL, ids->Get(i_ref)->CStr(), TRUE, FALSE);
			if (found == -1)
				return OpStatus::ERR_NO_MEMORY;
			else if (found > 0)
			{
				named_element = iterator.GetNextElement();
				RETURN_IF_ERROR(m_refs.Add(named_element));
			}
		}
	else
	{
		// Home subtree = tree detached from document tree.
		// In this case there's no nice collection of named elements.
		HTML_Element* local_subtree_root = m_root_element;
		while (local_subtree_root->ParentActual())
			local_subtree_root = local_subtree_root->ParentActual();

		// O(m*n) = O(#refids * #elements in local subtree).
		for (UINT32 i_ref = 0; i_ref < ids->GetCount(); i_ref++)
			for (HTML_Element* iter_home_subtree = local_subtree_root; iter_home_subtree; iter_home_subtree = iter_home_subtree->NextActual())
				if (iter_home_subtree->GetId() && uni_str_eq(iter_home_subtree->GetId(), ids->Get(i_ref)->CStr()))
				{
					RETURN_IF_ERROR(m_refs.Add(iter_home_subtree));
					break;  // Match only first element with given id in home subtree.
				}
	}
	return OpStatus::OK;
}

OP_STATUS
MicrodataTraverse::Traverse(HTML_Element* ref_el)
{
	// 1. if this element is property (has itemProp attribute), add it to properties.
	// 2. if this element is subitem (has itemScope attribute), don't traverse inside.
	// 3. if this element is not subitem, traverse subtree.

	// 0. Don't do double visit:  store all elements from root's itemRef
	// attribute (plus root element itself) which were traversed into
	// visitedrefs collection.
	for (UINT32 i_ref = m_current_ref; i_ref < m_refs.GetCount(); i_ref++)
		if (m_refs.Get(i_ref) == ref_el)
			RETURN_IF_ERROR(m_visitedrefs.Add(ref_el));

	if (ref_el != m_root_element)
	{
		// 1. if this element is property (has itemProp attribute), add it to properties.
		if (ref_el->GetItemPropAttribute() && ref_el->GetItemPropAttribute()->GetTokenCount() != 0)
			RETURN_IF_ERROR(m_root_properties_result.Add(ref_el));

		// 2. If this element is subitem (has itemScope attribute), don't traverse inside.
		if (ref_el->GetBoolAttr(ATTR_ITEMSCOPE) && ref_el != m_root_element)
			return OpStatus::OK;
	}

	// 3. If this element is not subitem, traverse subtree.
	for (HTML_Element* children = ref_el->FirstChildActual(); children; children = children->SucActual())
		RETURN_IF_ERROR(Traverse(children));

	return OpStatus::OK;
}

OP_STATUS
MicrodataTraverse::CrawlMicrodataProperties()
{
	// 1. sort refids in tree order.
	// 2. traverse all refids' subtrees before root
	// 3. traverse root subtree
	// 4. traverse all refids' subtrees after root

	// 0. init
	m_root_properties_result.Empty();
	m_visitedrefs.Empty();
	m_refs.Empty();

	if (!m_root_element->GetBoolAttr(ATTR_ITEMSCOPE))
		// Properties and all dependant subcollections are empty for elements
		// which are not Microdata items.
		return OpStatus::OK;

	if (m_root_element->GetItemRefAttribute())
	{
		const OpAutoVector<OpString>& itemRefIds = m_root_element->GetItemRefAttribute()->GetTokens();

		// Find elements from home subtree with ids given in itemRef attribute.
		RETURN_IF_ERROR(FindElementsFromHomeSubtreeWithIds(&itemRefIds));

		// If I add root element into refs list and then sort set of {itemRef, root}, I have next loop of traversing easier.
		RETURN_IF_ERROR(m_refs.Add(m_root_element));

		// 1. sort refids in tree order
		m_refs.QSort(&CompareNodeOrder); // O(#refids ^ 2).

		// 2., 3., 4. traverse all subtrees of refids before root, subtree of root and subtrees of refids after root.
		// O(n * #siblings of ith refid).
		for (m_current_ref = 0; m_current_ref < m_refs.GetCount(); m_current_ref++)
		{
			HTML_Element* ref_el = m_refs.Get(m_current_ref);
			if (m_visitedrefs.Find(ref_el) != -1)
				// HTML element from itemRef has been traversed, so there's no need to
				// double properties from its subtree.
				continue;
			RETURN_IF_ERROR(Traverse(ref_el));
		}
	}
	else
		// There's no risk of getting duplicate properties.
		// 2. - empty, 3. - traverse subtree of root, 4. - empty.
		RETURN_IF_ERROR(Traverse(m_root_element));

	return OpStatus::OK;
}

BOOL DOM_NodeCollectionMicrodataProperties::Update()
{
	if (cached_valid)
		return FALSE;
	Length(); // As a side effect of Length() method, collection becomes up to date.
	return TRUE;
}

BOOL
DOM_NodeCollectionMicrodataProperties::ContainsMicrodataProperty(const uni_char* property_name)
{
	OP_ASSERT(property_name);
	int properties_length = Length(); // Length makes collection up to date if needed.
	for (int i_props = 0; i_props < properties_length; i_props++)
	{
		HTML_Element* helm_ith_property = Item(i_props);
		OP_ASSERT(helm_ith_property || !"Length() should return number of real elements.");
		const OpAutoVector<OpString>& v_props = helm_ith_property->GetItemPropAttribute()->GetTokens();
		for (UINT32 j = 0; j < v_props.GetCount(); j++)
		{
			OP_ASSERT(v_props.Get(j)->CStr() || !"itemProp should be split on whitespace to non-null tokens.");
			if (uni_str_eq(v_props.Get(j)->CStr(), property_name))
				return TRUE;
		}
	}
	return FALSE;
}

/*virtual */ ES_GetState
DOM_NodeCollectionMicrodataProperties::GetCollectionProperty(DOM_Collection *collection, BOOL allowed, const uni_char *property_name, int property_code, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	OpAtom atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(atom >= OP_ATOM_UNASSIGNED && atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);

	if (atom == OP_ATOM_length || atom == OP_ATOM_names)
		return collection->GetName(atom, return_value, origining_runtime);

	DOM_Collection* subcollection;
	GET_FAILED_IF_ERROR(GetPropertyNodeList(property_name, subcollection));
	DOMSetObject(return_value, subcollection);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_NodeCollectionMicrodataProperties::GetLength(int &length, BOOL allowed)
{
	needs_invalidation = TRUE;

	if (!allowed)
		return GET_SECURITY_VIOLATION;

	length = Length();
	return GET_SUCCESS;
}

/* virtual */ int
DOM_NodeCollectionMicrodataProperties::Length()
{
	if (!root)
		return 0;

	UpdateSerialNr();

	if (cached_length != -1)
		return cached_length;

	HTML_Element *root_element = GetRootElement();
	if (!root_element)
		return 0;

	MicrodataTraverse mdt(root_element, m_root_properties_cache);
	if (OpStatus::IsError(mdt.CrawlMicrodataProperties()))
	{
		cached_length = 0;
		return cached_length;
	}
	cached_length = m_root_properties_cache.GetCount();
	cached_valid = FALSE;
	cached_index_match = NULL;
	cached_index = -1;

	return cached_length;
}

/* virtual */ HTML_Element *
DOM_NodeCollectionMicrodataProperties::Item(int index)
{
	if (!root)
		return NULL;

	UpdateSerialNr();

	if (cached_length != -1 && index >= cached_length)
		return NULL;

	if (cached_valid && cached_index_match && cached_index == index)
		return cached_index_match;

	/* Force the computation of length to populate cache and to determine if given index is out of bounds. */
	if (cached_length == -1)
	{
		cached_length = Length();
		if (index >= cached_length)
			return NULL;
	}

	HTML_Element *root_element = GetRootElement();
	if (!root_element)
		return NULL;

	HTML_Element* found_element = m_root_properties_cache.Get(index);
	if (found_element)
	{
		cached_valid = TRUE;
		cached_index = index;
		cached_index_match = found_element;
		return found_element;
	}
	else
		return NULL;
}

OP_STATUS
DOM_NodeCollectionMicrodataProperties::GetPropertyNodeList(const uni_char *property_name, DOM_Collection *&subcollection)
{
	DOM_PropertyNodeList_Collection *typed_collection;
	RETURN_IF_ERROR(GetCachedSubcollection(subcollection, property_name));
	if (!subcollection)
	{
		RETURN_IF_ERROR(DOM_PropertyNodeList_Collection::Make(typed_collection, GetEnvironment(), root, this, property_name));
		subcollection = typed_collection;
		RETURN_IF_ERROR(SetCachedSubcollection(property_name, subcollection));
	}
	return OpStatus::OK;
}

//---

/* static */ OP_STATUS
DOM_NodeCollection_PropertyNodeList::Make(DOM_NodeCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, DOM_NodeCollectionMicrodataProperties* base_properties, const uni_char* property_name_filter)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	if (OpStatus::IsMemoryError(DOMSetObjectRuntime(collection = OP_NEW(DOM_NodeCollection_PropertyNodeList, (base_properties, property_name_filter)), runtime)))
	{
		collection = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	collection->SetRoot(root, TRUE);
	return OpStatus::OK;
}

/* virtual */ int
DOM_NodeCollection_PropertyNodeList::Length()
{
	if (!m_base_properties->Update() && cached_valid && cached_length != -1)
		return cached_length;
	int base_length = m_base_properties->Length();
	int filtered_hit = 0;
	for (int i_props = 0; i_props < base_length; i_props++)
	{
		OP_ASSERT(m_base_properties->Get(i_props)->GetItemPropAttribute() || !"m_base_properties->Length() should return number of HTML elements with itemProp attribute.");
		const OpAutoVector<OpString>& ith_prop_tokens = m_base_properties->Get(i_props)->GetItemPropAttribute()->GetTokens();
		for (UINT32 j_prop_token = 0; j_prop_token < ith_prop_tokens.GetCount(); j_prop_token++)
			if (uni_str_eq(ith_prop_tokens.Get(j_prop_token)->CStr(), m_property_name_filter))
			{
				filtered_hit++;
				break; // Single HTML element fits only once, even if it has duplicated "m_property_name_filter" tokens in itemProp.
			}
	}
	cached_length = filtered_hit;
	return filtered_hit;
}

/* virtual */ HTML_Element *
DOM_NodeCollection_PropertyNodeList::Item(int index)
{
	if (!m_base_properties->Update() && cached_valid && cached_index == index)
		return cached_index_match;
	int base_length = m_base_properties->Length();
	int filtered_hit = 0;
	for (int i_prop = 0; i_prop < base_length; i_prop++)
	{
		HTML_Element* helm_ith_property = m_base_properties->Get(i_prop);
		OP_ASSERT(m_base_properties->Get(i_prop)->GetItemPropAttribute() || !"m_base_properties->Length() should return number of HTML elements with itemProp attribute.");
		const OpAutoVector<OpString>& ith_prop_tokens = helm_ith_property->GetItemPropAttribute()->GetTokens();
		for (UINT32 j_prop_token = 0; j_prop_token < ith_prop_tokens.GetCount(); j_prop_token++)
			if (uni_str_eq(ith_prop_tokens.Get(j_prop_token)->CStr(), m_property_name_filter))
			{
				if (filtered_hit == index)
				{
					cached_valid = TRUE;
					cached_index_match = helm_ith_property;
					cached_index = index;
					return cached_index_match;
				}
				filtered_hit++;
				break; // Single HTML element fits only once.
			}
	}
	cached_valid = TRUE;
	cached_index_match = NULL;
	cached_index = index;
	return cached_index_match;
}

/* virtual */ ES_GetState
DOM_NodeCollection_PropertyNodeList::GetLength(int &length, BOOL allowed)
{
	needs_invalidation = TRUE;
	if (!allowed)
		return GET_SECURITY_VIOLATION;
	length = Length();
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_NodeCollection_PropertyNodeList::GetCollectionProperty(DOM_Collection *collection, BOOL allowed, const uni_char *property_name, int property_code, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	OpAtom atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(atom >= OP_ATOM_UNASSIGNED && atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);
	if (atom == OP_ATOM_length)
	{
		DOMSetNumber(return_value, Length());
		return GET_SUCCESS;
	}
	return collection->GetName(atom, return_value, origining_runtime);
}

/* virtual */ void
DOM_NodeCollection_PropertyNodeList::ElementCollectionStatusChangedSpecific(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections)
{
	if (collections & DOM_Environment::COLLECTION_MICRODATA_PROPERTIES)
	{
		cached_valid = FALSE;
		cached_length = -1;
	}
}

//---
/** This class represents Microdata property of Microdata item.
 *
 * It is used when fetching helm.properties.names, which is list of unique
 * tree-ordered Microdata property names.
 **/
struct PropertyNameTreeOrdered
{
	PropertyNameTreeOrdered(const uni_char* name, int tree_order_index)
		: name(name)
		, name_hash(OpGenericStringHashTable::HashString(name, TRUE))
		, tree_order_index(tree_order_index)
	{}
	const uni_char* name;
	int name_hash;
	int tree_order_index;
	enum {
		IS_DUPLICATE = (1 << 30)
	};

	static int compare_name_hash(const PropertyNameTreeOrdered** a, const PropertyNameTreeOrdered** b)
	{
		if ((*a)->name_hash == (*b)->name_hash)
			return 0;
		return (*a)->name_hash < (*b)->name_hash ? -1 : 1;
	}
	static int compare_indices(const PropertyNameTreeOrdered** a, const PropertyNameTreeOrdered** b)
	{
		if ((*a)->tree_order_index == (*b)->tree_order_index)
			return 0;
		return (*a)->tree_order_index < (*b)->tree_order_index ? -1 : 1;
	}
};
//---

/* static */ OP_STATUS
DOM_HTMLPropertiesCollection::Make(DOM_HTMLPropertiesCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Element *root)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	DOM_NodeCollection *node_collection;
	RETURN_IF_ERROR(DOM_NodeCollectionMicrodataProperties::Make(node_collection, environment, root));

	if (!(collection = OP_NEW(DOM_HTMLPropertiesCollection, ())) ||
		OpStatus::IsMemoryError(collection->SetFunctionRuntime(runtime, runtime->GetPrototype(DOM_Runtime::HTMLPROPERTIESCOLLECTION_PROTOTYPE), UNI_L("HTMLPropertiesCollection"),  "HTMLPropertiesCollection", "s")))
	{
		OP_DELETE(collection);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(node_collection->RegisterCollection(collection));
	collection->node_collection = node_collection;
	node_collection->SetRoot(root, TRUE);

	return OpStatus::OK;
}

static OP_STATUS
PopulateNamesWithDuplicatesInTreeOrder(OpAutoVector<PropertyNameTreeOrdered>& names, DOM_NodeCollectionMicrodataProperties* nc)
{
	int properties_len = nc->Length();
	for (int i_props = 0; i_props < properties_len; i_props++)
	{
		OP_ASSERT(nc->Item(i_props)->GetItemPropAttribute() || !"nc->Length() should return number of HTML elements with itemProp attribute.");
		const OpAutoVector<OpString>& ith_prop_tokens = nc->Item(i_props)->GetItemPropAttribute()->GetTokens();
		for (UINT32 j_prop_token = 0; j_prop_token < ith_prop_tokens.GetCount(); j_prop_token++)
		{
			PropertyNameTreeOrdered* next_name = OP_NEW(PropertyNameTreeOrdered, (ith_prop_tokens.Get(j_prop_token)->CStr(), names.GetCount()));
			if (!next_name)
				return OpStatus::ERR_NO_MEMORY;
			RETURN_IF_ERROR(names.Add(next_name));
		}
	}
	return OpStatus::OK;
}

/* virtual */ unsigned
DOM_HTMLPropertiesCollection::StringList_length()
{
	DOM_NodeCollectionMicrodataProperties* nc = static_cast<DOM_NodeCollectionMicrodataProperties*>(GetNodeCollection());
	if (!nc->Update() && m_cached_names_length != -1)
		return m_cached_names_length;

	OpAutoVector<PropertyNameTreeOrdered> names;
	if (OpStatus::IsError(PopulateNamesWithDuplicatesInTreeOrder(names, nc)))
		return 0;

	// In order to spot duplicates, sort the collection by string name_hash.
	names.QSort(&PropertyNameTreeOrdered::compare_name_hash);
	PropertyNameTreeOrdered* previous = NULL;

	int i_unique = 0;
	for (UINT32 i_with_dupes = 0; i_with_dupes < names.GetCount(); i_with_dupes++)
	{
		PropertyNameTreeOrdered* current = names.Get(i_with_dupes);
		if (previous && previous->name_hash == current->name_hash && uni_str_eq(previous->name, current->name))
			continue; // It is duplicate, don't count.
		i_unique++;
		previous = current;
	}
	m_cached_names_length = i_unique;
	return m_cached_names_length;
}

// Place for future performance improvement: cached OpAutoVector<PropertyNameTreeOrdered> names.
/* virtual */ OP_STATUS
DOM_HTMLPropertiesCollection::StringList_item(int index, const uni_char *&property_name)
{
	DOM_NodeCollectionMicrodataProperties* nc = static_cast<DOM_NodeCollectionMicrodataProperties*>(GetNodeCollection());
	if (!nc->Update() && m_cached_names_index != -1 && m_cached_names_index == index)
	{
		property_name = m_cached_names_index_value.CStr();
		return OpStatus::OK;
	}

	OpAutoVector<PropertyNameTreeOrdered> names;
	RETURN_IF_ERROR(PopulateNamesWithDuplicatesInTreeOrder(names, nc));

	// In order to spot duplicates, sort the collection by string name_hash.
	names.QSort(&PropertyNameTreeOrdered::compare_name_hash);
	PropertyNameTreeOrdered* previous = NULL;

	int i_unique = 0;
	for (UINT32 i_with_dupes = 0; i_with_dupes < names.GetCount(); i_with_dupes++)
	{
		PropertyNameTreeOrdered* current = names.Get(i_with_dupes);
		if (previous && previous->name_hash == current->name_hash && uni_str_eq(previous->name, current->name))
		{
			// In order to preserve tree order between different properties,
			// check tree order of properties with same name.  If there's a
			// duplicate, it is given very high number, so after sorting, it
			// ends up at the end of the list.
			if (current->tree_order_index >= previous->tree_order_index)
				current->tree_order_index = PropertyNameTreeOrdered::IS_DUPLICATE;
			else
			{
				previous->tree_order_index = PropertyNameTreeOrdered::IS_DUPLICATE;
				previous = current;
			}
			continue;
		}
		i_unique++;
		previous = current;
	}

	// Sort back to tree order.
	names.QSort(&PropertyNameTreeOrdered::compare_indices);

	PropertyNameTreeOrdered* indexth_item = names.Get(index);
	OP_ASSERT(static_cast<UINT32>(index) < names.GetCount());
	OP_ASSERT(indexth_item || !"Only non-null names should have been sorted.");
	if (indexth_item->tree_order_index != PropertyNameTreeOrdered::IS_DUPLICATE)
		RETURN_IF_ERROR(m_cached_names_index_value.Set(indexth_item->name));
	else
		m_cached_names_index_value.Empty();
	m_cached_names_index = index;
	property_name = m_cached_names_index_value.CStr();
	return OpStatus::OK;
}

/* virtual */ BOOL
DOM_HTMLPropertiesCollection::StringList_contains(const uni_char *string)
{
	DOM_NodeCollectionMicrodataProperties* nc = static_cast<DOM_NodeCollectionMicrodataProperties*>(GetNodeCollection());
	return nc->ContainsMicrodataProperty(string);
}

/* static */ int
DOM_HTMLPropertiesCollection::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(collection, DOM_TYPE_HTML_PROPERTIES_COLLECTION, DOM_HTMLPropertiesCollection);
	DOM_CHECK_ARGUMENTS("N");
	int index = TruncateDoubleToInt(argv[0].value.number);
	ES_GetState state = collection->GetIndex(index, return_value, origining_runtime);
	if (state == GET_FAILED)
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	else
		return ConvertGetNameToCall(state, return_value);
}

/* static */ int
DOM_HTMLPropertiesCollection::namedItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(collection, DOM_TYPE_HTML_PROPERTIES_COLLECTION, DOM_HTMLPropertiesCollection);
	DOM_CHECK_ARGUMENTS("s");
	const uni_char* property_name = argv[0].value.string;

	DOM_Collection* subcollection;
	DOM_NodeCollectionMicrodataProperties* nc = static_cast<DOM_NodeCollectionMicrodataProperties*>(collection->GetNodeCollection());
	CALL_FAILED_IF_ERROR(nc->GetPropertyNodeList(property_name, subcollection));
	DOMSetObject(return_value, subcollection);

	return ES_VALUE;
}

/* virtual */ int
DOM_HTMLPropertiesCollection::Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");
	if (!OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	m_is_call = TRUE;
	int state = CallAsGetNameOrGetIndex(argv[0].value.string, return_value, static_cast<DOM_Runtime *>(origining_runtime));
	m_is_call = FALSE;
	return state;
}

/* virtual */ ES_GetState
DOM_HTMLPropertiesCollection::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	BOOL allowed = OriginCheck(origining_runtime);
	DOM_NodeCollectionMicrodataProperties* collection = static_cast<DOM_NodeCollectionMicrodataProperties*>(GetNodeCollection());

	// [OverrideBuiltins] is not declared for any of the properties, hence no overriding is allowed
	if (!m_is_call && (property_code == OP_ATOM_item || property_code == OP_ATOM_namedItem))
		return GET_FAILED;
	if (m_is_call
		|| (collection->ContainsMicrodataProperty(property_name))
		|| property_code == OP_ATOM_length
		|| property_code == OP_ATOM_names)
		return collection->GetCollectionProperty(this, allowed, property_name, property_code, value, origining_runtime);
	return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_HTMLPropertiesCollection::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
		if (value)
			return DOM_Collection::GetName(property_name, value, origining_runtime);
		else
			return GET_SUCCESS;
	case OP_ATOM_names:
		if (value)
		{
			ES_GetState state = DOMSetPrivate(value, DOM_PRIVATE_names);
			if (state == GET_FAILED)
			{
				DOM_DOMStringList *stringlist;
				GET_FAILED_IF_ERROR(DOM_DOMStringList::Make(stringlist, this, this, static_cast<DOM_Runtime *>(origining_runtime)));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_names, *stringlist));
				DOMSetObject(value, stringlist);
				return GET_SUCCESS;
			}
			else
				return state;
		}
		return GET_SUCCESS;
	default:
		return DOM_Collection::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLPropertiesCollection::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_names:
		return PUT_SUCCESS;
	default:
		return DOM_Collection::PutName(property_name, value, origining_runtime);
	}
}

/* static */ OP_STATUS
DOM_PropertyNodeList_Collection::Make(DOM_PropertyNodeList_Collection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, DOM_NodeCollectionMicrodataProperties* base_properties, const uni_char* property_name_filter)
{
	OP_ASSERT(base_properties);
	OP_ASSERT(property_name_filter);

	DOM_NodeCollection *node_collection;
	RETURN_IF_ERROR(DOM_NodeCollection_PropertyNodeList::Make(node_collection, environment, root, base_properties, property_name_filter));

	DOM_Runtime *runtime = environment->GetDOMRuntime();
	if (!(collection = OP_NEW(DOM_PropertyNodeList_Collection, ()))
		|| OpStatus::IsMemoryError(collection->SetFunctionRuntime(runtime, runtime->GetPrototype(DOM_Runtime::HTMLPROPERTYNODELISTCOLLECTION_PROTOTYPE), UNI_L("PropertyNodeList"), "PropertyNodeList", "s-")))
	{
		OP_DELETE(collection);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(node_collection->RegisterCollection(collection));
	collection->node_collection = node_collection;
	collection->SetRoot(root, TRUE);
	return OpStatus::OK;
}

/* static */ int
DOM_PropertyNodeList_Collection::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(collection, DOM_TYPE_PROPERTY_NODE_LIST_COLLECTION, DOM_PropertyNodeList_Collection);
	DOM_CHECK_ARGUMENTS("N");
	int index = TruncateDoubleToInt(argv[0].value.number);
	ES_GetState state = collection->GetIndex(index, return_value, origining_runtime);
	if (state == GET_FAILED)
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	else
		return ConvertGetNameToCall(state, return_value);
}

/* static */ int
DOM_PropertyNodeList_Collection::getValues(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(collection, DOM_TYPE_PROPERTY_NODE_LIST_COLLECTION, DOM_PropertyNodeList_Collection);

	ES_Object* values_array;
	CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&values_array));
	int props_len = collection->Length();
	for (int i_prop_el = 0; i_prop_el < props_len; i_prop_el++)
	{
		ES_Value ith_itemValue;
		HTML_Element* helm_ith_property = collection->Item(i_prop_el);
		DOM_Object* ith_property_node;
		collection->GetEnvironment()->ConstructNode(ith_property_node, helm_ith_property);
		const uni_char* is_subitem = static_cast<const uni_char*>(helm_ith_property->GetAttr(ATTR_ITEMSCOPE, ITEM_TYPE_STRING, NULL));
		if (is_subitem)
		{
			DOM_Object::DOMSetObject(&ith_itemValue, ith_property_node);
			CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(values_array, i_prop_el, ith_itemValue));
			continue;
		}
		OpAtom to_get = DOM_HTMLElement::GetItemValueProperty(helm_ith_property);
		ES_GetState state = ith_property_node->GetName(to_get, &ith_itemValue, origining_runtime);
		if (state != GET_SUCCESS)
			return ConvertGetNameToCall(state, return_value);
		CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(values_array, i_prop_el, ith_itemValue));
	}
	DOMSetObject(return_value, values_array);
	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_PropertyNodeList_Collection::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	// Microdata property names shadow PropertyNodeList members (for now only "length" member).
	DOM_NodeCollectionMicrodataProperties* collection = static_cast<DOM_NodeCollectionMicrodataProperties*>(GetNodeCollection());
	if (collection->ContainsMicrodataProperty(property_name))
		return collection->GetCollectionProperty(this, TRUE, property_name, property_code, value, origining_runtime);

	if (property_code == OP_ATOM_length)
	{
		if (value)
			DOMSetNumber(value, collection->Length());
		return GET_SUCCESS;
	}
	return GET_FAILED;
}

//---
OP_STATUS
DOM_ToplevelItemCollectionFilter::AddType(const uni_char* to_add, int len)
{
	OpAutoPtr<OpString> ap_str(OP_NEW(OpString, ()));
	RETURN_IF_ERROR(ap_str.get()->Set(to_add, len));
	RETURN_IF_ERROR(m_itemtypes.Add(ap_str.get()));
	ap_str.release();
	return OpStatus::OK;
}

/* virtual */
DOM_ToplevelItemCollectionFilter::~DOM_ToplevelItemCollectionFilter()
{
	m_itemtypes.DeleteAll();
}

/* virtual */ void
DOM_ToplevelItemCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	visit_children = TRUE;
	include = FALSE;
	if (element->IsToplevelMicrodataItem())
	{
		UINT32 itemtypes_len = m_itemtypes.GetCount();
		if (itemtypes_len == 0)
			include = TRUE;
		else
		{
			const uni_char *itemtype = element->GetStringAttr(ATTR_ITEMTYPE);
			if (itemtype)
				for (UINT32 i = 0; i < itemtypes_len; i++)
					if (m_itemtypes.Get(i)->Compare(itemtype) == 0)
					{
						include = TRUE;
						return;
					}
		}
	}
}

/* virtual */ DOM_CollectionFilter *
DOM_ToplevelItemCollectionFilter::Clone() const
{
	DOM_ToplevelItemCollectionFilter *clone = OP_NEW(DOM_ToplevelItemCollectionFilter, ());
	if (clone)
		for (UINT32 i = 0; i < m_itemtypes.GetCount(); i++)
			if (OpStatus::IsMemoryError(clone->AddType(m_itemtypes.Get(i)->CStr())))
			{
				OP_DELETE(clone);
				return NULL;
			}
	return clone;
}

/* virtual */ BOOL
DOM_ToplevelItemCollectionFilter::IsMatched(unsigned collections)
{
	return (collections & DOM_Environment::COLLECTION_MICRODATA_TOPLEVEL_ITEMS) != 0;
}

/* virtual */ BOOL
DOM_ToplevelItemCollectionFilter::IsEqual(DOM_CollectionFilter *other)
{
	if (other->GetType() != TYPE_MICRODATA_TOPLEVEL_ITEM)
		return FALSE;
	DOM_ToplevelItemCollectionFilter *toplevel_item_other = static_cast<DOM_ToplevelItemCollectionFilter *>(other);
	UINT32 itemtypes_len = m_itemtypes.GetCount();
	if (itemtypes_len != toplevel_item_other->m_itemtypes.GetCount())
		return FALSE;

	/* A strict notion of equality. We could admit a larger set by handling permutations; not worth it. */
	for (UINT32 i = 0; i < itemtypes_len; i++)
		if (m_itemtypes.Get(i)->Compare(*toplevel_item_other->m_itemtypes.Get(i)) != 0)
			return FALSE;
	return TRUE;
}



#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_HTMLPropertiesCollection)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLPropertiesCollection, DOM_HTMLPropertiesCollection::item, "item", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLPropertiesCollection, DOM_HTMLPropertiesCollection::namedItem, "namedItem", "s-")
DOM_FUNCTIONS_END(DOM_HTMLPropertiesCollection)

DOM_FUNCTIONS_START(DOM_PropertyNodeList_Collection)
	DOM_FUNCTIONS_FUNCTION(DOM_PropertyNodeList_Collection, DOM_PropertyNodeList_Collection::item, "item", "n-")
	DOM_FUNCTIONS_FUNCTION(DOM_PropertyNodeList_Collection, DOM_PropertyNodeList_Collection::getValues, "getValues", "-")
DOM_FUNCTIONS_END(DOM_PropertyNodeList_Collection)
