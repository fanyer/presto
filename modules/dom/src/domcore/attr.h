/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/* See comments in core/ecma.cpp for information about object structure
   and storage management.
   */

#ifndef DOM_ATTR_H
#define DOM_ATTR_H

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"

class HTML_Element;
class DOM_Element;
class DOM_AttrMutationListener;

class DOM_Attr
  : public DOM_Node
{
private:
	DOM_Attr(uni_char *name);

	DOM_Element *owner_element;

	uni_char *name;
	uni_char *value;
	uni_char *ns_uri;
	uni_char *ns_prefix;
	BOOL dom1node, is_case_sensitive;

	DOM_Attr *next_attr;

	HTML_Element *value_root;
	DOM_AttrMutationListener *listener;
	BOOL is_syncing_tree_and_value;
	BOOL valid_childnode_collection;

	OP_STATUS BuildValueTree(DOM_Runtime *origining_runtime);

public:
	static OP_STATUS Make(DOM_Attr *&attr, DOM_Node *reference, const uni_char *name, const uni_char *ns_uri, const uni_char *ns_prefix, BOOL dom1node);

	virtual ~DOM_Attr();

	void SetOwnerElement(DOM_Element *element);

	BOOL HasName(const uni_char *attr_name, const uni_char *ns_uri);
		/**< HasName returns TRUE iff this attribute has the give name. */

	OP_STATUS CopyValueIn();
		/**< Copy the attribute value from the 'owner_element' into private storage in this node. */

	const uni_char *GetName() { return name; }
	const uni_char *GetNsUri() { return ns_uri; }
	const uni_char *GetNsPrefix() { return ns_prefix; }
	BOOL IsCaseSensitive() { return is_case_sensitive; }

	HTML_Element *GetPlaceholderElement();
	void ClearPlaceholderElement();
		/**< The subtree has been garbage collected. */

	OP_STATUS GetName(TempBuffer *buffer);

	OP_STATUS Clone(DOM_Attr *&clone);
	/** Make a new clone of this node as specified in the DOM2 core spec for cloneNode(). */

	OP_STATUS Import(DOM_Attr *&clone, DOM_Document *document);
	/** Created an imported copy of this node to document as specified in the DOM2 core spec for importNode(). */

	OP_STATUS CreateValueTree(DOM_Runtime *origining_runtime);
	OP_STATUS UpdateValueTreeFromValue(DOM_Runtime *origining_runtime);
	OP_STATUS UpdateValueFromValueTree(HTML_Element *exclude, DOM_Runtime *origining_runtime);

	// Standard protocol
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_ATTR || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	DOM_Element *GetOwnerElement() { return owner_element; }

	void AddToElement(DOM_Element *element, BOOL case_sensitive);
	/**< Add this attribute node to an element's list of attribute nodes.

	     @param assign If TRUE, the attribute node's current value is
	                   assigned to the element's attribute.

	     @param case_sensitive If TRUE, The name of the attribute will
		               be treated case sensitively.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS RemoveFromElement();
	/**< Remove this attribute from the element it belongs to. */

	int GetNsIndex();

	const uni_char *GetValue();
	/**< Get this attribute's value.  The value is always a string.

	     @param value Set to the attribute's value on succes.  Can be NULL.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS SetValue(const uni_char *value, DOM_Runtime *origining_runtime);
	/**< Set this attribute's value.  The value should always be a string.

	     @param value The attribute's new value.  Must be a string.

	     @return OpStatus::OK, OpStatus::ERR if the attribute's value is
	             read only or OpStatus::ERR_NO_MEMORY on OOM. */

	DOM_Attr *GetNextAttr() { return next_attr; }
	void SetNextAttr(DOM_Attr *next) { next_attr = next; }
};

#endif // !DOM_ATTR_H
