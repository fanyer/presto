/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_XPATHRESULT
#define DOM_XPATHRESULT

#ifdef DOM3_XPATH

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/util/adt/opvector.h"
#include "modules/xpath/xpath.h"

class DOM_Document;
class DOM_Node;
class XPathNode;

class DOM_XPathResult
	: public DOM_Object,
	  public DOM_MutationListener
{
private:
	OP_STATUS EnsureTypeAndFirstResult();

protected:
	DOM_XPathResult(DOM_Document *document, XMLTreeAccessor *tree, XPathExpression::Evaluate *xpathresult);

	XMLTreeAccessor *tree;
	HTML_Element *rootelement;
	XPathExpression::Evaluate *xpathresult;
	unsigned result_type; // Bitfield with XPathExpression::Evaluate::Type bits
	unsigned snapshot_length;

	DOM_Document *document;
	BOOL invalid, check_changes;
	OpVector<DOM_Node> nodes;

	OP_STATUS Initialize();
	OP_STATUS AddNode(XPathNode *xpathnode);

public:
	virtual ~DOM_XPathResult();

	static OP_STATUS Make(DOM_XPathResult *&domresult, DOM_Document *document, XMLTreeAccessor *tree, XPathExpression::Evaluate *xpathresult);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XPATHRESULT || DOM_Object::IsA(type); }
	virtual void GCTrace();

	// From DOM_MutationListener
	virtual OP_STATUS OnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime);
	virtual OP_STATUS OnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime);
	virtual OP_STATUS OnAfterValueModified(HTML_Element *element, const uni_char *new_value, ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime);
	virtual OP_STATUS OnAttrModified(HTML_Element *element, const uni_char *name, int ns_idx, DOM_Runtime *origining_runtime);

	OP_STATUS SetValue(XMLTreeAccessor *tree, XPathExpression::Evaluate *xpathresult);
	XPathExpression::Evaluate *GetValue() { return xpathresult; }

	BOOL IsInvalid() { return !xpathresult || invalid; }

	unsigned GetCount() { return nodes.GetCount(); }
	DOM_Node *GetNode(unsigned index) { return nodes.Get(index); }

	static void ConstructXPathResultObjectL(ES_Object *object, DOM_Runtime *runtime);
	static OP_STATUS GetDOMNode(DOM_Node *&domnode, XPathNode *xpathnode, DOM_Document *document);

	DOM_DECLARE_FUNCTION_WITH_DATA(getNode); // iterateNext and snapshotItem
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };

	enum
	{
		// Constant numbers are from the DOM3 XPath specification

		// XPathResultType
		// const unsigned short      ANY_TYPE                       = 0;
		// const unsigned short      NUMBER_TYPE                    = 1;
		// const unsigned short      STRING_TYPE                    = 2;
		// const unsigned short      BOOLEAN_TYPE                   = 3;
		// const unsigned short      UNORDERED_NODE_ITERATOR_TYPE   = 4;
		// const unsigned short      ORDERED_NODE_ITERATOR_TYPE     = 5;
		// const unsigned short      UNORDERED_NODE_SNAPSHOT_TYPE   = 6;
		// const unsigned short      ORDERED_NODE_SNAPSHOT_TYPE     = 7;
		// const unsigned short      ANY_UNORDERED_NODE_TYPE        = 8;
		// const unsigned short      FIRST_ORDERED_NODE_TYPE        = 9;

		ANY_TYPE = 0,
		NUMBER_TYPE = 1,
		STRING_TYPE = 2,
		BOOLEAN_TYPE = 3,
		UNORDERED_NODE_ITERATOR_TYPE = 4,
		ORDERED_NODE_ITERATOR_TYPE = 5,
		UNORDERED_NODE_SNAPSHOT_TYPE = 6,
		ORDERED_NODE_SNAPSHOT_TYPE = 7,
		ANY_UNORDERED_NODE_TYPE = 8,
		FIRST_ORDERED_NODE_TYPE = 9
	};
};

#endif // DOM3_XPATH
#endif // DOM_XPATHRESULT
