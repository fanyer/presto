/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/dom/src/domcore/docfrag.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domcore/domstaticnodelist.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/webforms2/webforms2dom.h"

#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/link.h"

static BOOL IsInvisibleFormElm(HTML_Element *elm, LogicalDocument* logdoc)
{
	// Some forms in HTML (not XHTML) are ignored by all form code, since they are nested
	// in other forms. They are kept for completeness (layout, dom, etc) but when looking
	// from a form perspective they shouldn't exist.
	OP_ASSERT(elm->IsMatchingType(HE_FORM, NS_HTML));

	if (!logdoc || logdoc->IsXml())
		return FALSE;

	return elm->GetInserted() < HE_INSERTED_BYPASSING_PARSER && elm->GetFormNr() == -1;
}

/* virtual */ void
DOM_SimpleCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	HTML_ElementType elm_type = element->Type();
	BOOL is_html = element->GetNsType() == NS_HTML;

	include = FALSE;
	visit_children = TRUE;

	switch (group)
	{
	case ALL:
	case ALLELEMENTS:
		if (Markup::IsRealElement(elm_type))
			include = TRUE;
		break;

	case CHILDREN:
		// MSDN: "Retrieves a collection of DHTML Objects that are direct descendants of the object."
		// We'll return all elements, skipping text nodes, comments and processing instructions. MSIE will
		// also return comments but the implementation is faster if it doesn't, and Mozilla does as Opera.
		if (Markup::IsRealElement(elm_type))
			include = TRUE;
		visit_children = FALSE;
		break;

	case CHILDNODES:
		include = TRUE;
		visit_children = FALSE;
		break;

	case IMAGES:
		if (is_html && elm_type == HE_IMG)
			include = TRUE;
		break;

	case AREAS:
		if (is_html && elm_type == HE_AREA)
			include = TRUE;
		break;

	case TABLE_SECTION_ROWS:
	case TABLE_ROWS:
		visit_children = FALSE;
		if (is_html)
			if (elm_type == HE_TR)
				include = TRUE;
			else if (group == TABLE_ROWS && (elm_type == HE_TBODY || elm_type == HE_TFOOT || elm_type == HE_THEAD))
			{
				HTML_Element* parent = element->ParentActual();
				if (parent && parent->IsMatchingType(HE_TABLE, NS_HTML))
					visit_children = TRUE;
			}
		break;

	case TABLE_CELLS:
	case TABLE_ROW_CELLS:
		visit_children = FALSE;
		if (is_html)
			if (elm_type == HE_TD || elm_type == HE_TH)
				include = TRUE;
			else if (group == TABLE_CELLS && (elm_type == HE_TBODY || elm_type == HE_TFOOT || elm_type == HE_THEAD || elm_type == HE_TR))
				visit_children = TRUE;
		break;

	case TBODIES:
		if (is_html && elm_type == HE_TBODY)
			include = TRUE;
		visit_children = FALSE;
		break;

	case OPTIONS:
	case SELECTED_OPTIONS:
		visit_children = FALSE;
		if (is_html)
		{
			if (elm_type == HE_OPTION)
			{
				if (group == OPTIONS || element->IsSelectedOption())
					include = TRUE;
			}
			else if (elm_type == HE_OPTGROUP)
				visit_children = TRUE;
		}
		break;

	case FORMELEMENTS:
		if (is_html)
		{
			switch (elm_type)
			{
			case HE_SELECT:
			case HE_DATALIST:
				visit_children = FALSE;
				include = TRUE;
				break;

			case HE_INPUT:
				include = element->GetInputType() != INPUT_IMAGE;
				break;

			case HE_KEYGEN:
			case HE_BUTTON:
			case HE_OBJECT:
			case HE_TEXTAREA:
			case HE_FIELDSET:
			case HE_OUTPUT:
				include = TRUE;
				break;
			}
		}
		break;

	case APPLETS:
		if (is_html && elm_type == HE_APPLET)
			include = TRUE;
		else if (is_html && elm_type == HE_OBJECT)
		{
			if (URL* inline_url = element->GetUrlAttr(ATTR_DATA, NS_IDX_HTML, logdoc))
			{
				HTML_ElementType element_type = elm_type;
				OP_BOOLEAN resolve_status = element->GetResolvedObjectType(inline_url, element_type, logdoc);
				if (resolve_status == OpBoolean::IS_TRUE && element_type == HE_APPLET)
					include = TRUE;
			}
		}
		break;

	case LINKS:
		if (is_html && ((elm_type == HE_AREA && element->GetAREA_URL(logdoc))
				|| (elm_type == HE_A && element->GetA_URL(logdoc))))
			include = TRUE;
		break;

	case FORMS:
		if (is_html && elm_type == HE_FORM && !IsInvisibleFormElm(element, logdoc))
			include = TRUE;
		break;

	case ANCHORS:
		if (is_html && elm_type == HE_A && element->GetName())
			include = TRUE;
		break;

	case DOCUMENTNODES:
		if (is_html)
		{
			if (elm_type == HE_OBJECT)
			{
				// We have to avoid returning a collection for the case
				// <object id=x><embed id=x></object>
				// All browsers do differently but Safari's model seems likely
				// to be accepted as HTML5. See bug 288935 and revised bug CORE-19579

				// Skip HE_OBJECT elements that has are not fallback-free
				HTML_Element* stop = element->NextSiblingActual();
				HTML_Element* it = element->NextActual();
				BOOL has_fallback = FALSE;
				while (it != stop)
				{
					if (it->IsMatchingType(HE_OBJECT, NS_HTML) ||
						it->IsMatchingType(HE_EMBED, NS_HTML))
					{
						has_fallback = TRUE;
						break;
					}
					it = it->NextActual();
				}

				if (has_fallback)
				{
					// Is not "fallback-free"
					break; // Skip this node
				}
			}

			BOOL accessible_through_id = TRUE;
			if (elm_type == HE_FORM ||
				elm_type == HE_IFRAME && !element->HasAttr(ATTR_NAME)) // in MSIE it's available for <iframe name=""> but not for <iframe name>
			{
				accessible_through_id = FALSE;
			}

			switch (elm_type)
			{
			case HE_FORM: // MSIE, FF: Only accessible through name (not id)
				if (IsInvisibleFormElm(element, logdoc))
					break;
				// fallthrough
			case HE_IMG: // MSIE: Only through the name. FF: through both id and name
			case HE_IFRAME: // MSIE: Through both the name and id _if_ there is a name attribute. HTML5, Safari, FF: Not included at all
			case HE_EMBED: // MSIE: Only through the name. FF: through both id and name
			case HE_APPLET:
			case HE_OBJECT: // MSIE, FF: Through both name and id but in Safari object element isn't included if it contains other content.
				// Elements that have a non empty name or id should be included
				if (*(const uni_char*)element->GetAttr(ATTR_NAME, ITEM_TYPE_STRING, (void*)UNI_L(""), NS_IDX_HTML))
					include = TRUE;
				else if (accessible_through_id)
				{
					const uni_char* id = element->GetId();
					include = id && *id;
				}

			default:
				break;
			}
		}
		break;

	case NAME_IN_WINDOW:
		// When MSIE looks for names in a window (!), it finds anything with a
		// name or an id, except form elements (FORM, INPUT, SELECT, TEXTAREA,
		// OPTION, BUTTON) inside FORM elements. There is further filtering
		// of this collection done in DOM_Collection::FetchPropertiesL() and
		// DOM_Collection::GetName(), the only methods reachable on this
		// collection.
		if (is_html && Markup::IsRealElement(elm_type))
			switch (elm_type)
			{
			case HE_FORM:
				if (IsInvisibleFormElm(element, logdoc))
					return;
				include = TRUE;
				break;

			case HE_INPUT:
			case HE_SELECT:
			case HE_OPTION:
			case HE_BUTTON:
			case HE_TEXTAREA:
				if (element->GetFormNr() != -1)
					break;

			default:
				include = TRUE;
			}
		break;

	case EMBEDS:
		if (is_html && elm_type == HE_EMBED)
			include = TRUE;
		break;

	case PLUGINS:
		if (is_html && elm_type == HE_EMBED)
			include = TRUE;
		else if (is_html && elm_type == HE_OBJECT)
		{
			if (URL* inline_url = element->GetUrlAttr(ATTR_DATA, NS_IDX_HTML, logdoc))
			{
				OP_BOOLEAN resolve_status = element->GetResolvedObjectType(inline_url, elm_type, logdoc);
				if (resolve_status == OpBoolean::IS_TRUE && (elm_type == HE_EMBED || elm_type == HE_OBJECT))
					include = TRUE;
			}
		}
		break;

	case SCRIPTS:
		if (is_html && elm_type == HE_SCRIPT)
			include = TRUE;
		break;

	case STYLESHEETS:
		if (element->IsLinkElement())
		{
			if (elm_type == HE_PROCINST)
				include = TRUE;
			else
			{
				const uni_char *rel = element->GetStringAttr(ATTR_REL);
				if (rel)
				{
					unsigned kinds = LinkElement::MatchKind(rel);
					include = (kinds & LINK_TYPE_STYLESHEET) != 0;
				}
			}
		}
		else if (element->IsStyleElement())
			include = TRUE;
		break;

	}
}

/* virtual */ BOOL
DOM_SimpleCollectionFilter::IsMatched(unsigned collections)
{
	switch (group)
	{
	default:
		return FALSE;

	case SELECTED_OPTIONS:
		return (collections & DOM_Environment::COLLECTION_SELECTED_OPTIONS) != 0;

	case FORMELEMENTS:
		return (collections & DOM_Environment::COLLECTION_FORM_ELEMENTS) != 0;

	case APPLETS:
		return (collections & DOM_Environment::COLLECTION_APPLETS) != 0;

	case LINKS:
		return (collections & DOM_Environment::COLLECTION_LINKS) != 0;

	case ANCHORS:
		return (collections & DOM_Environment::COLLECTION_ANCHORS) != 0;

	case DOCUMENTNODES:
		return ((collections & DOM_Environment::COLLECTION_DOCUMENT_NODES) != 0 || (collections & DOM_Environment::COLLECTION_NAME_OR_ID) != 0);

	case PLUGINS:
		return (collections & DOM_Environment::COLLECTION_PLUGINS) != 0;

	case STYLESHEETS:
		return (collections & DOM_Environment::COLLECTION_STYLESHEETS) != 0;
	}
}


/* virtual */ DOM_CollectionFilter *
DOM_SimpleCollectionFilter::Clone() const
{
	DOM_SimpleCollectionFilter *filter = OP_NEW(DOM_SimpleCollectionFilter, (group));
	if (!filter)
		return NULL;

	filter->allocated = TRUE;
	filter->is_incompatible = is_incompatible;
	return filter;
}

/* virtual */ BOOL
DOM_SimpleCollectionFilter::CanSkipChildren()
{
	switch (group)
	{
	case CHILDREN:
	case CHILDNODES:
	case OPTIONS:
	case SELECTED_OPTIONS:
	case TABLE_ROWS:
	case TABLE_SECTION_ROWS:
	case TABLE_CELLS:
	case TABLE_ROW_CELLS:
	case TBODIES:
	case FORMELEMENTS:
		/* These can set visit_children to FALSE, which is incompatible
		   with fast search. */
		return TRUE;

	default:
		return FALSE;
	}
}

/* virtual */ BOOL
DOM_SimpleCollectionFilter::CanIncludeGrandChildren()
{
	switch (group)
	{
	case CHILDNODES:
	case CHILDREN:
	case TABLE_SECTION_ROWS:
	case TABLE_ROW_CELLS:
	case TBODIES:
		/* These will not set visit_children to TRUE */
		return FALSE;

	default:
		return TRUE;
	}
}

/* virtual */ BOOL
DOM_SimpleCollectionFilter::IsEqual(DOM_CollectionFilter *other)
{
	if (other->GetType() != TYPE_SIMPLE)
		return FALSE;

	DOM_SimpleCollectionFilter *other_simple = static_cast<DOM_SimpleCollectionFilter *>(other);

	return (group == other_simple->group);
}

/* virtual */ void
DOM_HTMLElementCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	HTML_ElementType elm_type = element->Type();

	if (!Markup::IsRealElement(elm_type) && elm_type != HE_ANY)
	{
		include = FALSE;
		visit_children = FALSE;
		return;
	}

	BOOL is_html = element->GetNsType() == NS_HTML;
	if (is_html && elm_type == tag_match)
	{
		include = TRUE;
		visit_children = tag_match_children;
	}
	else if (!is_html)
	{
		const uni_char *elm_tag = element->GetTagName();
		include = elm_tag && uni_str_eq(tag_name, elm_tag);
		visit_children = tag_match_children;
	}
	else
	{
		include = FALSE;
		visit_children = TRUE;
	}
}

/* virtual */ DOM_CollectionFilter *
DOM_HTMLElementCollectionFilter::Clone() const
{
	DOM_HTMLElementCollectionFilter *filter = OP_NEW(DOM_HTMLElementCollectionFilter, (tag_match, tag_name, tag_match_children));
	if (!filter)
		return NULL;

	filter->allocated = TRUE;
	filter->is_incompatible = is_incompatible;
	return filter;
}

/* virtual */ BOOL
DOM_HTMLElementCollectionFilter::IsEqual(DOM_CollectionFilter *other)
{
	if (other->GetType() != TYPE_HTML_ELEMENT)
		return FALSE;

	DOM_HTMLElementCollectionFilter *other_elem = static_cast<DOM_HTMLElementCollectionFilter *>(other);

	return (tag_match == other_elem->tag_match && tag_match_children == other_elem->tag_match_children);
}

/* static */ BOOL
DOM_NameCollectionFilter::IsHETypeWithNameAllowedOnWindow(HTML_ElementType type)
{
	/**
	 * CORE-27753
	 * when looking up window[name] where name is the name=""
	 * attribute do NOT return non form and form field elements
	 * and other special types.
	 *
	 * Note that HE_FRAME and HE_IFRAME are missing from this list
	 * because the nameinwindow collection maps elements by name,
	 * but JS_Window::GetName calls DOM_GetWindowFrame before
	 * accessing nameinwindow which properly returns frames by their
	 * name.
	 */
	switch (type)
	{
	//Form elements
	case HE_INPUT:
	case HE_SELECT:
	case HE_OPTION:
	case HE_BUTTON:
	case HE_TEXTAREA:
	case HE_FORM:
	// Special stuff http://www.whatwg.org/specs/web-apps/current-work/#named-access-on-the-window-object
	case HE_A:
	case HE_APPLET:
	case HE_AREA:
	case HE_EMBED:
	case HE_FRAMESET:
	case HE_IMG:
	case HE_OBJECT:
		return TRUE;
	}

	return FALSE;
}

#define IS_DOM_FILTER_NAMEINWINDOW(filter) \
	((filter) && (filter)->GetType() == DOM_CollectionFilter::TYPE_SIMPLE && \
		static_cast<DOM_SimpleCollectionFilter *>(filter)->IsNameInWindow())


/* virtual */ void
DOM_NameCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	if (base)
		base->Visit(element, include, visit_children, logdoc);
	else
	{
		include = TRUE;
		visit_children = TRUE;
	}

	if (include)
	{
		BOOL name_match = FALSE;

		if (check_name)
		{
			const uni_char *elm_name = element->GetName();
			if (elm_name)
			{
				if (IS_DOM_FILTER_NAMEINWINDOW(base) && !IsHETypeWithNameAllowedOnWindow(element->Type()))
					elm_name = NULL;

				if (elm_name && uni_str_eq(elm_name, name))
					name_match = TRUE;
			}
		}

		if (check_id && !name_match)
		{
			const uni_char *elm_id = element->GetId();
			if (elm_id && uni_str_eq(elm_id, name))
				name_match = TRUE;
		}

		include = name_match;
	}
}

/* virtual */ DOM_CollectionFilter *
DOM_NameCollectionFilter::Clone() const
{
	DOM_CollectionFilter *base_copy = NULL;

	if (base)
	{
		base_copy = base->Clone();
		if (!base_copy)
			return NULL;
	}

	const uni_char *name_copy = uni_strdup(name);
	if (!name_copy)
	{
		OP_DELETE(base_copy);
		return NULL;
	}

	DOM_NameCollectionFilter *filter = OP_NEW(DOM_NameCollectionFilter, (base_copy, name_copy, check_name, check_id));
	if (!filter)
	{
		OP_DELETE(base_copy);
		op_free((uni_char *) name_copy);
		return NULL;
	}

	filter->allocated = TRUE;
	filter->is_incompatible = is_incompatible;
	return filter;
}

BOOL
DOM_NameCollectionFilter::CheckIncompatible(DOM_CollectionFilter *base_filter)
{
	if (!base_filter || base_filter->GetType() != TYPE_NAME)
		return FALSE;

	DOM_NameCollectionFilter *base_name = static_cast<DOM_NameCollectionFilter*>(base);

	if (!uni_str_eq(name, base_name->name))
		return TRUE;

	/* If we added a new filter of same name, then the check for same-name has already been performed for the base filter,
	   hence re-using its is-incompatible is perfectly fine (and desirable.) */
	return base_filter->IsIncompatible();
}

/*static*/ BOOL
DOM_NameCollectionFilter::IsNameFilterFor(DOM_CollectionFilter *filter, const uni_char *name)
{
	if (!filter || filter->GetType() != TYPE_NAME)
		return FALSE;

	DOM_NameCollectionFilter *name_filter = static_cast<DOM_NameCollectionFilter*>(filter);

	return (name_filter->check_id && name_filter->check_name && name_filter->name && name && uni_str_eq(name, name_filter->name));
}

/* virtual */ BOOL
DOM_NameCollectionFilter::IsMatched(unsigned collections)
{
	return (collections & DOM_Environment::COLLECTION_NAME_OR_ID) != 0 || base && base->IsMatched(collections);
}

/* virtual */ BOOL
DOM_NameCollectionFilter::IsEqual(DOM_CollectionFilter *other)
{
	if (other->GetType() != TYPE_NAME)
		return FALSE;

	DOM_NameCollectionFilter *other_name = static_cast<DOM_NameCollectionFilter *>(other);

	BOOL same_fields =
			(name == other_name->name || name && other_name->name && uni_str_eq(name, other_name->name)) &&
			 check_name == other_name->check_name && check_id == other_name->check_id;

	return (same_fields && (base == other_name->base || base && base->IsEqual(other)));
}

DOM_TagsCollectionFilter::DOM_TagsCollectionFilter(DOM_CollectionFilter *base, const uni_char *ns_uri, const uni_char *name, BOOL is_xml)
	: base(base),
	  any_ns(ns_uri && ns_uri[0] == '*' && !ns_uri[1]),
	  any_name(name && name[0] == '*' && !name[1]),
	  ns_uri(ns_uri),
	  name(name),
	  is_xml(is_xml)
{
	is_incompatible = base && base->IsIncompatible();
}

/* virtual */
DOM_TagsCollectionFilter::~DOM_TagsCollectionFilter()
{
	if (allocated)
	{
		OP_DELETE(base);
		uni_char *nc_name = const_cast<uni_char *>(name);
		OP_DELETEA(nc_name);
		uni_char *nc_ns_uri = const_cast<uni_char *>(ns_uri);
		OP_DELETEA(nc_ns_uri);
	}
}

BOOL
DOM_TagsCollectionFilter::TagNameMatches(HTML_Element* elm)
{
	const uni_char *tagname = elm->GetTagName();

	if (!tagname || !*tagname)
		return FALSE;

	return UseCaseSensitiveCompare(elm) ? uni_str_eq(tagname, name) : uni_stri_eq(tagname, name);
}

/* virtual */ void
DOM_TagsCollectionFilter::Visit(HTML_Element *element, BOOL &include, BOOL &visit_children, LogicalDocument *logdoc)
{
	if (base)
	{
		base->Visit(element, include, visit_children, logdoc);

		if (!include)
			return;
	}
	else
		visit_children = TRUE;

	include = FALSE;

	if (name && Markup::IsRealElement(element->Type()))
	{
		if (!any_ns)
		{
			const uni_char *uri = g_ns_manager->GetElementAt(element->GetNsIdx())->GetUri();

			if (uri && !*uri)
				uri = NULL;

			if (!ns_uri != !uri || uri && ns_uri && uni_strcmp(uri, ns_uri) != 0)
				return;
		}

		if (!any_name)
		{
			if (!TagNameMatches(element))
				return;
		}

		include = TRUE;
	}
}

/* virtual */ DOM_CollectionFilter *
DOM_TagsCollectionFilter::Clone() const
{
	DOM_CollectionFilter *base_copy = NULL;
	if (base)
	{
		base_copy = base->Clone();
		if (!base_copy)
			return NULL;
	}

	uni_char *name_copy = (uni_char *) name;
	if (name_copy && !(name_copy = UniSetNewStr(name_copy)))
	{
		OP_DELETE(base_copy);
		return NULL;
	}

	uni_char *ns_uri_copy = (uni_char *) ns_uri;
	if (ns_uri_copy && !(ns_uri_copy = UniSetNewStr(ns_uri_copy)))
	{
		OP_DELETE(base_copy);
		OP_DELETEA(name_copy);
		return NULL;
	}

	DOM_TagsCollectionFilter *filter = OP_NEW(DOM_TagsCollectionFilter, (base_copy, ns_uri_copy, name_copy, is_xml));
	if (!filter)
	{
		OP_DELETE(base_copy);
		OP_DELETEA(name_copy);
		OP_DELETEA(ns_uri_copy);
		return NULL;
	}

	filter->allocated = TRUE;
	filter->is_incompatible = is_incompatible;
	return filter;
}

/* virtual */ BOOL
DOM_TagsCollectionFilter::IsMatched(unsigned collections)
{
	return base && base->IsMatched(collections);
}

/* virtual */ BOOL
DOM_TagsCollectionFilter::IsEqual(DOM_CollectionFilter *other)
{
	if (other->GetType() != TYPE_TAGS)
		return FALSE;

	DOM_TagsCollectionFilter *other_tags = static_cast<DOM_TagsCollectionFilter *>(other);

	return (base == other_tags->base &&
			is_xml == other_tags->is_xml &&
			(any_ns && any_ns == other_tags->any_ns || ns_uri == other_tags->ns_uri || ns_uri && other_tags->ns_uri && uni_str_eq(ns_uri, other_tags->ns_uri)) &&
			(any_name && any_name == other_tags->any_name || name == other_tags->name || name && other_tags->name && uni_str_eq(name, other_tags->name)));
}

DOM_NodeCollection::DOM_NodeCollection(BOOL reusable)
	: DOM_CollectionLink(reusable),
	  root(NULL),
	  filter(NULL),
	  subcollections(NULL),
	  tree_root(NULL),
	  owner(NULL),
	  include_root(FALSE),
	  create_subcollections(FALSE),
	  check_name(TRUE),
	  check_id(TRUE),
	  has_named_element_properties(TRUE),
	  use_form_nr(FALSE),
	  prefer_window_objects(FALSE),
	  needs_invalidation(TRUE),
	  serial_nr(-1),
	  cached_valid(TRUE),
	  cached_index(-1),
	  cached_length(-1),
	  cached_index_match(NULL)
{
	missing_names[0] = missing_names[1] = missing_names[2] = OP_ATOM_UNASSIGNED;
}

/* static */ OP_STATUS
DOM_NodeCollection::Make(DOM_NodeCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, BOOL include_root, BOOL has_named_properties, const DOM_CollectionFilter &filter_prototype, BOOL reusable)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	if (OpStatus::IsMemoryError(DOMSetObjectRuntime(collection = OP_NEW(DOM_NodeCollection, (reusable)), runtime)) ||
		OpStatus::IsError(collection->SetFilter(filter_prototype)))
	{
		collection = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	collection->has_named_element_properties = has_named_properties;
	collection->SetRoot(root, include_root);

	return OpStatus::OK;
}

OP_STATUS
DOM_NodeCollection::SetFilter(const DOM_CollectionFilter &filter_prototype)
{
	OP_ASSERT(!filter);
	if (!(filter = filter_prototype.Clone()))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void
DOM_NodeCollection::SetRoot(DOM_Node *new_root, BOOL new_include_root)
{
	root = new_root;
	include_root = new_include_root;

	RecalculateTreeRoot();
}

void
DOM_NodeCollection::SetCreateSubcollections()
{
	create_subcollections = TRUE;
}

void
DOM_NodeCollection::AddToIndexCache(unsigned int index, HTML_Element *new_element)
{
	/* Update the cache if the index is later than already known about. */
	if (cached_index < 0 || cached_index < static_cast<int>(index))
	{
		cached_valid = TRUE;
		/* ToDo: insert assert verifying that the item at the given index is indeed correct? */
		cached_index = index;
		cached_index_match = new_element;
	}
}

void
DOM_NodeCollection::RefreshIndexCache(HTML_Element *new_element)
{
	/* Re-enable the cache if the indexed element is the same. It is guaranteed by the caller that it
	 * is now the current element. */
	if (!cached_valid && cached_index > 0 && cached_index_match == new_element)
	{
		cached_valid = TRUE;
	}
}

void
DOM_NodeCollection::SetOwner(DOM_Object *new_owner)
{
	owner = new_owner;
}

BOOL
DOM_NodeCollection::IsSameCollection(DOM_Node* other_root, BOOL other_include_root, DOM_CollectionFilter* other_filter)
{
	return (root == other_root && include_root == static_cast<unsigned>(other_include_root) &&
		((filter && filter->IsEqual(other_filter)) || (!filter && !other_filter)));
}

void
DOM_NodeCollection::ElementCollectionStatusChanged(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections)
{
	if (added && element == tree_root)
	{
		// Our subtree was added to another tree so we got a new root.
		GetEnvironment()->MoveCollection(this, tree_root, affected_tree_root);
		tree_root = affected_tree_root;
	}

	if (tree_root == affected_tree_root)
	{
		BOOL invalidate_collection = TRUE;
		if (added || removed)
		{
			// Shallow collections (which are common: childNodes, cells, rows, ...) can
			// easily avoid being invalidated unnecessary by checking the root element in the
			// inserted/removed subtree.
			if (filter && !filter->CanIncludeGrandChildren())
			{
				// We can just check if the element itself is/will be a part of the
				// collection. If not, then this collection will not be affected by
				// the change and can keep its cache.
				invalidate_collection = FALSE;
				if (root && element->ParentActual() == root->GetPlaceholderElement())
				{
					LogicalDocument *logdoc = root->GetOwnerDocument()->GetLogicalDocument();
					BOOL include;
					BOOL visit_children;

					filter->Visit(element, include, visit_children, logdoc);
					OP_ASSERT(!visit_children || !"The filter claimed to never set visit_children by returning FALSE in CanIncludeGrandChildren()");
					invalidate_collection = include;
				}
			}
		}

		if (invalidate_collection && needs_invalidation)
		{
			needs_invalidation = FALSE;
			if (owner)
				owner->SignalPropertySetChanged();

			SignalPropertySetChanged();

			/* Notify caches on the DOM_Collections that share this NodeCollection. */
			for (DOM_Collection *collection = collection_listeners.First(); collection; collection = collection->Suc())
				collection->SignalPropertySetChanged();
		}

		HTML_Element *root_element = root->GetPlaceholderElement();

		if (removed && element->IsAncestorOf(root_element))
		{
			GetEnvironment()->MoveCollection(this, tree_root, element);
			tree_root = element;
		}

		/* The selectedOptions collection doesn't include the <select> root
		   element, but changes are reported via it. Make any invalidation
		   go ahead for that collection. */
		if (element == root_element && !include_root && collections != DOM_EnvironmentImpl::COLLECTION_SELECTED_OPTIONS)
			return;

		if (invalidate_collection)
			ElementCollectionStatusChangedSpecific(affected_tree_root, element, added, removed, collections);
	}
}

void
DOM_NodeCollection::ElementCollectionStatusChangedSpecific(HTML_Element *affected_tree_root, HTML_Element *element, BOOL added, BOOL removed, unsigned collections)
{
	OP_ASSERT(filter); // Called on a not-fully created collection. Check the construction code.
	if (collections == DOM_Environment::COLLECTION_ALL || filter->IsMatched(collections))
	{
		cached_valid = FALSE;
		cached_length = -1;

		if (added)
			missing_names[0] = missing_names[1] = missing_names[2] = OP_ATOM_UNASSIGNED;
	}

	if (collections == DOM_Environment::COLLECTION_NAME_OR_ID && added)
		missing_names[0] = missing_names[1] = missing_names[2] = OP_ATOM_UNASSIGNED;
	OP_ASSERT(!root || tree_root && !tree_root->Parent() || removed && tree_root == element);
}

void
DOM_NodeCollection::ResetRoot(HTML_Element *affected_tree_root)
{
	if (tree_root == affected_tree_root)
	{
		/* Our tree is being garbage collected.	 Since we mark our root, and
		   thus the tree, in GCTrace(), this means we are also about to be
		   garbage collected.  Nevertheless, need to reset, in case something
		   happens in the meantime. */

		root = NULL;
		GetEnvironment()->MoveCollection(this, tree_root, NULL);
		tree_root = NULL;
	}
}

/* virtual */
DOM_NodeCollection::~DOM_NodeCollection()
{
	OP_DELETE(filter);
	collection_listeners.RemoveAll();
}

void
DOM_NodeCollection::UnregisterCollection(DOM_Collection *c)
{
	c->Out();
}

OP_STATUS
DOM_NodeCollection::RegisterCollection(DOM_Collection *c)
{
	c->Into(&collection_listeners);
	return OpStatus::OK;
}

ES_GetState
DOM_NodeCollection::GetLength(int &length, BOOL allowed)
{
	OP_ASSERT(filter);

	needs_invalidation = TRUE;

	BOOL has_length_property = !root || filter->GetType() != DOM_CollectionFilter::TYPE_SIMPLE || static_cast<DOM_SimpleCollectionFilter*>(filter)->HasLengthProperty();
	if (!has_length_property)
		return GET_FAILED;

	if (!allowed)
		return GET_SECURITY_VIOLATION;

	length = Length();
	return GET_SUCCESS;
}

ES_GetState
DOM_NodeCollection::GetCollectionProperty(DOM_Collection *collection, BOOL allowed, const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	OpAtom atom = static_cast<OpAtom>(property_code);
	OP_ASSERT(atom >= OP_ATOM_UNASSIGNED && atom < OP_ATOM_ABSOLUTELY_LAST_ENUM);

	BOOL has_length_value = FALSE;

	needs_invalidation = TRUE;
	if (atom == OP_ATOM_length && collection->GetName(atom, value, origining_runtime) == GET_SUCCESS)
	{
		/* A few collections let an object named "length" hide the length property */
		OP_ASSERT(filter);
		BOOL obj_named_length_hides_coll_length = filter->GetType() == DOM_CollectionFilter::TYPE_SIMPLE && static_cast<DOM_SimpleCollectionFilter*>(filter)->IsObjectHidingLengthProperty();
		if (!obj_named_length_hides_coll_length)
			return GET_SUCCESS;

		has_length_value = TRUE;
	}

	if (create_subcollections)
	{
		BOOL found_many;

		if (HTML_Element *element = NamedItem(atom, property_name, &found_many))
		{
			if (!allowed)
				return GET_SECURITY_VIOLATION;

			if (value)
				if (found_many)
				{
					DOM_Collection *subcollection;

					GET_FAILED_IF_ERROR(GetCachedSubcollection(subcollection, property_name));

					if (!subcollection)
					{
						if (DOM_NameCollectionFilter::IsNameFilterFor(filter, property_name))
							subcollection = collection;
						else
						{
							DOM_NameCollectionFilter subfilter(filter, property_name);
							GET_FAILED_IF_ERROR(DOM_Collection::Make(subcollection, GetEnvironment(), collection->GetRuntime()->GetClass(*collection), root, include_root, TRUE, subfilter));

							DOM_NodeCollection *node_subcollection = subcollection->GetNodeCollection();

							node_subcollection->create_subcollections = TRUE;
							node_subcollection->check_id = check_id;
							node_subcollection->check_name = check_name;
							node_subcollection->has_named_element_properties = has_named_element_properties;
							node_subcollection->use_form_nr = use_form_nr;

							GET_FAILED_IF_ERROR(SetCachedSubcollection(property_name, subcollection));
						}
					}

					DOMSetObject(value, *subcollection);
					return GET_SUCCESS;
				}
				else
					return SetElement(value, element, static_cast<DOM_Runtime *>(origining_runtime));
			else
				return GET_SUCCESS;
		}
	}
	else if (HTML_Element *element = NamedItem(atom, property_name))
	{
		if (!allowed)
			return GET_SECURITY_VIOLATION;

		if (value)
			return SetElement(value, element, static_cast<DOM_Runtime *>(origining_runtime));
		else
			return GET_SUCCESS;
	}

	if (atom == OP_ATOM_length)
		return has_length_value ? GET_SUCCESS : GET_FAILED;
	if (atom == OP_ATOM_UNASSIGNED)
		return GET_FAILED;
	return collection->GetName(atom, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_Collection::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	BOOL allowed = OriginCheck(origining_runtime);

	return GetNodeCollection()->GetCollectionProperty(this, allowed, property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_Collection::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	BOOL allowed = OriginCheck(origining_runtime);

	if (HTML_Element *element = GetNodeCollection()->Item(property_index))
		if (!allowed)
			return GET_SECURITY_VIOLATION;
		else
			return GetNodeCollection()->SetElement(value, element, static_cast<DOM_Runtime *>(origining_runtime));
	else
		return GET_FAILED;
}

/* static */ OP_STATUS
DOM_Collection::Make(DOM_Collection *&collection, DOM_EnvironmentImpl *environment, const uni_char *uni_class_name, const char *class_name, DOM_Node *root, BOOL include_root, BOOL try_sharing, const DOM_CollectionFilter &filter_prototype, DOM_Runtime::Prototype &prototype)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();

	if (!(collection = OP_NEW(DOM_Collection, ())) ||
		OpStatus::IsMemoryError(collection->SetFunctionRuntime(runtime, runtime->GetPrototype(prototype), uni_class_name, class_name, "s-")))
	{
		OP_DELETE(collection);
		return OpStatus::ERR_NO_MEMORY;
	}

	DOM_NodeCollection *node_collection;

	if (try_sharing)
	{
		BOOL found = filter_prototype.IsElementFilter() ?
			environment->FindElementCollection(node_collection, root, include_root, const_cast<DOM_CollectionFilter *>(&filter_prototype)) :
			environment->FindNodeCollection(node_collection, root, include_root, const_cast<DOM_CollectionFilter *>(&filter_prototype));
		if (found)
		{
			RETURN_IF_ERROR(node_collection->RegisterCollection(collection));
			collection->node_collection = node_collection;
			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(DOM_NodeCollection::Make(node_collection, environment, root, include_root, prototype == DOM_Runtime::HTMLCOLLECTION_PROTOTYPE, filter_prototype, try_sharing));
	RETURN_IF_ERROR(node_collection->RegisterCollection(collection));
	collection->node_collection = node_collection;
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_Collection::Make(DOM_Collection *&collection, DOM_EnvironmentImpl *environment, const char *class_name, DOM_Node *root, BOOL include_root, BOOL try_sharing, const DOM_CollectionFilter &filter_prototype)
{
	DOM_Runtime::Prototype prototype;

	if (op_strcmp(class_name, "NodeList") == 0)
		prototype = DOM_Runtime::NODELIST_PROTOTYPE;
	else
		prototype = DOM_Runtime::HTMLCOLLECTION_PROTOTYPE;

	TempBuffer buffer;
	RETURN_IF_ERROR(buffer.Append(class_name));

	return DOM_Collection::Make(collection, environment, buffer.GetStorage(), class_name, root, include_root, try_sharing, filter_prototype, prototype);
}

/* static */ OP_STATUS
DOM_Collection::MakeNodeList(DOM_Collection *&collection, DOM_EnvironmentImpl *environment, DOM_Node *root, BOOL include_root, BOOL try_sharing, const DOM_CollectionFilter &filter_prototype)
{
	DOM_Runtime::Prototype prototype = DOM_Runtime::NODELIST_PROTOTYPE;

	return DOM_Collection::Make(collection, environment, UNI_L("NodeList"), "NodeList", root, include_root, try_sharing, filter_prototype, prototype);
}

void
DOM_Collection::SetCreateSubcollections()
{
	GetNodeCollection()->SetCreateSubcollections();
}

/* virtual */ ES_GetState
DOM_Collection::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		BOOL allowed = OriginCheck(origining_runtime);
		int length;

		ES_GetState result = GetNodeCollection()->GetLength(length, allowed);
		if (result == GET_SUCCESS)
			DOMSetNumber(value, length);

		return result;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Collection::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ int
DOM_Collection::Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");

	if (!OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	return CallAsGetNameOrGetIndex(argv[0].value.string, return_value, (DOM_Runtime *) origining_runtime);
}

/* virtual */
DOM_Collection::~DOM_Collection()
{
	Out();
}

/* virtual */ void
DOM_Collection::GCTrace()
{
	GCMark(node_collection);
}

/* virtual */ void
DOM_NodeCollection::GCTrace()
{
	GCMark(root);
	GCMark(subcollections);
}

/* virtual */ ES_GetState
DOM_Collection::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	if (!GetNodeCollection()->has_named_element_properties)
		return GET_SUCCESS;

	BOOL is_name_in_window = IS_DOM_FILTER_NAMEINWINDOW(GetNodeCollection()->GetFilter());

	int length = GetNodeCollection()->Length();

	for (int index = 0; index < length; ++index)
	{
		HTML_Element *item = GetNodeCollection()->Item(index);

		if (GetNodeCollection()->check_name)
		{
			const uni_char *name = item->GetName();
			if (name && *name && (!is_name_in_window || DOM_NameCollectionFilter::IsHETypeWithNameAllowedOnWindow(item->Type())))
				enumerator->AddPropertyL(name);
		}

		if (GetNodeCollection()->check_id)
		{
			const uni_char *id = item->GetId();
			if (id && *id)
				enumerator->AddPropertyL(id);
		}
	}

	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_Collection::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = GetNodeCollection()->Length();
	return GET_SUCCESS;
}

int
DOM_NodeCollection::Length()
{
	if (!root || filter->IsIncompatible())
		return 0;

	UpdateSerialNr();

	if (cached_length != -1)
		return cached_length;

	int length = 0;
	const uni_char *name;

	unsigned index_to_cache = 0;
	HTML_Element *elem_to_cache = NULL;

	HTML_Element *root_element = GetRootElement();
	if (!root_element)
		return 0;

	LogicalDocument *logdoc = root->GetOwnerDocument()->GetLogicalDocument();

	if (logdoc && check_id && check_name && (!filter->CanSkipChildren() && filter->IsNameSearch(name)))
	{
		NamedElementsIterator iterator;

		int matched = logdoc->SearchNamedElements(iterator, root_element, name, TRUE, TRUE);
		HTML_Element* form = use_form_nr ? GetForm() : NULL;

		for (int index2 = 0; index2 < matched; ++index2)
		{
			BOOL include;
			BOOL visit_children;

			HTML_Element *iter = iterator.GetNextElement();

			if (!use_form_nr || iter->BelongsToForm(root->GetFramesDocument(), form))
				filter->Visit(iter, include, visit_children, logdoc);
			else
				include = FALSE;

			if (include)
			{
				OP_ASSERT(!(IsElementCollection() && !Markup::IsRealElement(iter->Type())) || !"Element collections must not include non-element nodes");
				index_to_cache = length;
				elem_to_cache  = iter;
				++length;
			}
		}
	}
	else
	{
		HTML_Element *iter = root_element;
		HTML_Element *stop = iter->NextSiblingActual();
		HTML_Element* form = NULL;

		if (!include_root)
			iter = iter->NextActual();

		if (use_form_nr)
		{
			form = GetForm();
			stop = NULL;
		}

		while (iter != stop)
		{
			BOOL include;
			BOOL visit_children;

			filter->Visit(iter, include, visit_children, logdoc);

			// Check that an invalidation assumption is correct.
			OP_ASSERT(!include || Markup::IsRealElement(iter->Type()) || !IsElementCollection() || !"Assumption in ElementCollectionStatusChanged about only one collection included text nodes is incorrect");

			include = include && (!use_form_nr || iter->BelongsToForm(root->GetFramesDocument(), form));

			if (include)
			{
				elem_to_cache = iter;
				index_to_cache = length;
				++length;
			}

			if (visit_children)
				iter = iter->NextActual();
			else
				iter = iter->NextSiblingActual();
		}
	}

	if (cached_index < 0 && elem_to_cache)
	{
		cached_valid = TRUE;
		cached_index_match = elem_to_cache;
		cached_index = index_to_cache;
	}

	cached_length = length;

	return length;
}

/* virtual */ HTML_Element *
DOM_NodeCollection::Item(int index)
{
	if (!root || filter->IsIncompatible())
		return NULL;

	UpdateSerialNr();

	if (cached_length != -1 && index >= cached_length)
		return NULL;

	if (cached_valid && cached_index_match && cached_index == index)
		return cached_index_match;

	/* Force the computation of length (and seeding of cache) to determine if given index is out of bounds. */
	if (cached_length == -1)
	{
		cached_length = Length();
		if (index >= cached_length)
			return NULL;
	}

	HTML_Element *root_element = GetRootElement();
	if (!root_element)
		return NULL;

	HTML_Element *iter = root_element;
	HTML_Element *stop = iter->NextSiblingActual();
	int start_index = index;
	HTML_Element* form = NULL;

	BOOL forward = TRUE;

	if (!include_root)
		iter = iter->NextActual();

	if (use_form_nr)
	{
		form = GetForm();
		stop = NULL;
	}

	BOOL can_skip_children = filter->CanSkipChildren();

	if (cached_valid && cached_index_match)
	{
		if (cached_index == index)
			return cached_index_match;
		else if (cached_index < index)
		{
			index -= cached_index;
			iter = cached_index_match;
		}
		else if (cached_index - index < index)
		{
			index = cached_index - index;
			iter = cached_index_match;
			forward = FALSE;
		}
	}

	BOOL include = FALSE;
	BOOL visit_children = FALSE;
	LogicalDocument *logdoc = GetLogicalDocument();
	FramesDocument	*root_doc = root->GetFramesDocument();

	while (iter != stop)
	{
		filter->Visit(iter, include, visit_children, logdoc);

	already_visited:
		include = include && (!use_form_nr || iter->BelongsToForm(root_doc, form));

		if (include)
			if (index == 0)
			{
				cached_valid = TRUE;
				cached_index = start_index;
				cached_index_match = iter;

				return iter;
			}
			else
				--index;

		if (forward)
			if (visit_children)
				iter = iter->NextActual();
			else
				iter = iter->NextSiblingActual();
		else
		{
			iter = iter->PrevActual();

			if (can_skip_children && iter)
			{
				HTML_Element *candidate = NULL;
				BOOL candidate_include = FALSE;

				for (HTML_Element *ancestor = iter->ParentActual(); ancestor; ancestor = ancestor->ParentActual())
				{
					if (!include_root && ancestor == root_element)
						break;

					if (filter)
						filter->Visit(ancestor, include, visit_children, logdoc);

					if (!visit_children)
					{
						candidate = ancestor;
						candidate_include = include;
					}

					if (ancestor == root_element)
						break;
				}

				if (candidate)
				{
					iter = candidate;
					include = candidate_include;
					goto already_visited;
				}
			}
		}
	}

	return NULL;
}

HTML_Element *
DOM_NodeCollection::NamedItem(OpAtom atom, const uni_char *name, BOOL *found_many)
{
	if (!root || filter->IsIncompatible())
		return NULL;

	UpdateSerialNr();

	if (atom != OP_ATOM_UNASSIGNED)
		if (atom == missing_names[0] || atom == missing_names[1] || atom == missing_names[2])
			return NULL;

	DOM_NameCollectionFilter name_filter(filter, name, check_name, check_id);
	HTML_Element *found_element = NULL;

	HTML_Element *root_element = GetRootElement();
	if (!root_element)
		return NULL;

	LogicalDocument *logdoc = root->GetOwnerDocument()->GetLogicalDocument();

	if (logdoc && logdoc->GetRoot()->IsAncestorOf(root_element))
	{
		BOOL found = FALSE;
		BOOL can_skip_children = name_filter.CanSkipChildren();
		NamedElementsIterator iterator;

		int matched = logdoc->SearchNamedElements(iterator, root_element, name, check_id, check_name);
		HTML_Element* form = use_form_nr ? GetForm() : NULL;

		for (int index = 0; index < matched; ++index)
		{
			BOOL include;
			BOOL visit_children;

			HTML_Element *iter = iterator.GetNextElement();

			name_filter.Visit(iter, include, visit_children, logdoc);

			include = include && (!use_form_nr || iter->BelongsToForm(root->GetFramesDocument(), form));

			if (include && can_skip_children && !use_form_nr)
				for (HTML_Element *parent = iter->Parent(); parent && parent != root_element; parent = parent->Parent())
				{
					BOOL include2, visit_children2;
					name_filter.Visit(parent, include2, visit_children2, logdoc);
					if (!visit_children2)
					{
						include = FALSE;
						break;
					}
				}

			if (include)
				if (found)
				{
					*found_many = TRUE;
					return found_element;
				}
				else if (found_many)
				{
					found = TRUE;
					*found_many = FALSE;
					found_element = iter;
				}
				else
					return iter;
		}
	}
	else
	{
		HTML_Element *iter = root_element;
		HTML_Element *stop = iter->NextSiblingActual();
		HTML_Element* form = NULL;

		if (!include_root)
			iter = iter->NextActual();

		if (use_form_nr)
		{
			form = GetForm();
			stop = NULL;
		}

		BOOL found = FALSE;

		while (iter != stop)
		{
			BOOL include;
			BOOL visit_children;

			name_filter.Visit(iter, include, visit_children, logdoc);

			include = include && (!use_form_nr ||
				iter->BelongsToForm(root->GetFramesDocument(), form)
				);

			if (include)
				if (found)
				{
					*found_many = TRUE;
					return found_element;
				}
				else if (found_many)
				{
					found = TRUE;
					*found_many = FALSE;
					found_element = iter;
				}
				else
					return iter;

			if (visit_children)
				iter = iter->NextActual();
			else
				iter = iter->NextSiblingActual();
		}
	}

	if (atom != OP_ATOM_UNASSIGNED && !found_element)
	{
		missing_names[2] = missing_names[1];
		missing_names[1] = missing_names[0];
		missing_names[0] = atom;
	}

	return found_element;
}

int
DOM_Collection::item(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (this_object && this_object->IsA(DOM_TYPE_STATIC_NODE_LIST))
		return DOM_StaticNodeList::getItem(this_object, argv, argc, return_value, origining_runtime);

	DOM_THIS_OBJECT(collection, DOM_TYPE_COLLECTION, DOM_Collection);
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

int
DOM_HTMLCollection::namedItem(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(collection, DOM_TYPE_COLLECTION, DOM_Collection);
	DOM_CHECK_ARGUMENTS("s");

	ES_GetState state = collection->GetName(argv[0].value.string, OP_ATOM_UNASSIGNED, return_value, origining_runtime);

	if (state == GET_FAILED)
	{
		DOMSetNull(return_value);
		return ES_VALUE;
	}
	else
		return ConvertGetNameToCall(state, return_value);
}

int
DOM_NodeCollection::GetTags(const uni_char *name, ES_Value *return_value)
{
	BOOL is_xml = root && root->GetOwnerDocument()->IsXML(); // if we have an empty (not loaded) document, it doesn't matter if it's xml or not.
	DOM_TagsCollectionFilter tag_filter(filter, NULL, name, is_xml);
	DOM_Collection *tags;

	CALL_FAILED_IF_ERROR(DOM_Collection::Make(tags, GetEnvironment(), "TagsArray", root, include_root, TRUE, tag_filter));

	tags->GetNodeCollection()->SetCreateSubcollections();

	DOMSetObject(return_value, tags);
	return ES_VALUE;
}

int
DOM_HTMLCollection::tags(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(base, DOM_TYPE_COLLECTION, DOM_Collection);
	DOM_CHECK_ARGUMENTS("s");

	return base->GetNodeCollection()->GetTags(argv[0].value.string, return_value);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_HTMLCollection)
	DOM_FUNCTIONS_FUNCTION(DOM_HTMLCollection, DOM_HTMLCollection::namedItem, "namedItem", "s-")
DOM_FUNCTIONS_END(DOM_HTMLCollection)

DOM_FUNCTIONS_START(DOM_Collection)
	DOM_FUNCTIONS_FUNCTION(DOM_Collection, DOM_Collection::item, "item", "n-")
DOM_FUNCTIONS_END(DOM_Collection)

void
DOM_NodeCollection::RecalculateTreeRoot()
{
	HTML_Element* new_tree_root;
	if (root)
	{
		new_tree_root = root->GetPlaceholderElement();
		if (new_tree_root)
			while (HTML_Element *parent = new_tree_root->Parent())
				new_tree_root = parent;
	}
	else
		new_tree_root = NULL;

	if (new_tree_root != tree_root)
	{
		GetEnvironment()->MoveCollection(this, tree_root, new_tree_root);
		tree_root = new_tree_root;
	}
}

OP_STATUS
DOM_NodeCollection::GetCachedSubcollection(DOM_Collection *&subcollection, const uni_char *name)
{
	subcollection = NULL;

	if (subcollections)
	{
		OP_BOOLEAN result;
		ES_Value value;

		RETURN_IF_ERROR(result = subcollections->Get(name, &value));

		if (result == OpBoolean::IS_TRUE)
			subcollection = DOM_VALUE2OBJECT(value, DOM_Collection);
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_NodeCollection::SetCachedSubcollection(const uni_char *name, DOM_Collection *subcollection)
{
	if (!subcollections)
	{
		DOM_Runtime *runtime = GetEnvironment()->GetDOMRuntime();

		if (OpStatus::IsMemoryError(DOMSetObjectRuntime(subcollections = OP_NEW(DOM_Object, ()), runtime)))
		{
			subcollections = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return subcollections->Put(name, *subcollection);
}

ES_GetState
DOM_NodeCollection::SetElement(ES_Value *value, HTML_Element *element, DOM_Runtime *origining_runtime)
{
	ES_GetState state = root->DOMSetElement(value, element);

	if (prefer_window_objects && value->type == VALUE_OBJECT)
	{
		DOM_Node *node = DOM_HOSTOBJECT(value->value.object, DOM_Node);

		if (node->IsA(DOM_TYPE_HTML_FRAMEELEMENT))
		{
			DOM_HTMLFrameElement *frameelement = static_cast<DOM_HTMLFrameElement *>(node);

			return frameelement->GetName(OP_ATOM_contentWindow, value, origining_runtime);
		}
	}

	return state;
}

HTML_Element *
DOM_NodeCollection::GetRootElement()
{
	if (!root)
		return NULL;

	HTML_Element *element = root->GetPlaceholderElement();

	if (use_form_nr)
	{
		HTML_Element *document_element = root->GetOwnerDocument()->GetPlaceholderElement();

		if (document_element && document_element->IsAncestorOf(element))
			return document_element;
	}

	return element;
}

int
DOM_NodeCollection::GetFormNr()
{
	OP_ASSERT(root->GetPlaceholderElement()->Type() == HE_FORM);
	return root->GetPlaceholderElement()->GetFormNr();
}

HTML_Element*
DOM_NodeCollection::GetForm()
{
	OP_ASSERT(root->GetPlaceholderElement()->Type() == HE_FORM);
	return root->GetPlaceholderElement();
}

void
DOM_NodeCollection::UpdateSerialNr()
{
	if (!GetEnvironment()->GetFramesDocument())
	{
		/* This collection belongs to a 'disconnected' environment, in which
		   collection updating won't be performed by logdoc, so just drop
		   anything we think we know.  This means collections in disconnected
		   environments are slow, but they ought to be used sparingly. */

		cached_valid = FALSE;
		cached_index = -1;
		cached_length = -1;
		cached_index_match = NULL;

		missing_names[0] = missing_names[1] = missing_names[2] = OP_ATOM_UNASSIGNED;
	}
	needs_invalidation = TRUE;
}

/* static */ OP_STATUS
DOM_HTMLOptionsCollection::Make(DOM_HTMLOptionsCollection *&collection, DOM_EnvironmentImpl *environment, DOM_Element *select_element)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	DOM_SimpleCollectionFilter filter(OPTIONS);

	DOM_NodeCollection *node_collection;

	RETURN_IF_ERROR(DOM_NodeCollection::Make(node_collection, environment, select_element, FALSE, TRUE, filter, FALSE));

	if (!(collection = OP_NEW(DOM_HTMLOptionsCollection, ())) ||
		OpStatus::IsMemoryError(collection->SetFunctionRuntime(runtime, runtime->GetPrototype(DOM_Runtime::HTMLOPTIONSCOLLECTION_PROTOTYPE), "HTMLOptionsCollection", "s")))
	{
		OP_DELETE(collection);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(node_collection->RegisterCollection(collection));
	collection->node_collection = node_collection;
	node_collection->SetRoot(select_element, FALSE);

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_HTMLOptionsCollection::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_selectedIndex:
	case OP_ATOM_value:
		return GetNodeCollection()->root->GetName(property_name, value, origining_runtime);

	default:
		return DOM_Collection::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLOptionsCollection::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_length:
	case OP_ATOM_selectedIndex:
	case OP_ATOM_value:
		return GetNodeCollection()->GetRoot()->PutName(property_name, value, origining_runtime);

	default:
		return DOM_Collection::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_HTMLOptionsCollection::PutNameRestart(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object* restart_object)
{
	return static_cast<DOM_HTMLSelectElement *>(GetNodeCollection()->GetRoot())->PutNameRestart(property_name, value, origining_runtime, restart_object);
}

/* virtual */ ES_PutState
DOM_HTMLOptionsCollection::PutIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	return static_cast<DOM_HTMLSelectElement *>(GetNodeCollection()->GetRoot())->PutIndex(property_index, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_HTMLOptionsCollection::PutIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object)
{
	return static_cast<DOM_HTMLSelectElement *>(GetNodeCollection()->GetRoot())->PutIndexRestart(property_index, value, origining_runtime, restart_object);
}

int
DOM_HTMLOptionsCollection::addOrRemove(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	if (argc >= 0)
	{
		DOM_THIS_OBJECT(options, DOM_TYPE_HTML_OPTIONS_COLLECTION, DOM_HTMLOptionsCollection);
		return DOM_HTMLSelectElement::addOrRemove((DOM_HTMLSelectElement *) options->GetNodeCollection()->GetRoot(), argv, argc, return_value, origining_runtime, data);
	}
	else
		return DOM_HTMLSelectElement::addOrRemove(NULL, NULL, -1, return_value, origining_runtime, data);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_HTMLOptionsCollection)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLOptionsCollection, DOM_HTMLOptionsCollection::addOrRemove, 0, "add", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_HTMLOptionsCollection, DOM_HTMLOptionsCollection::addOrRemove, 1, "remove", "n-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_HTMLOptionsCollection)

