/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_NOTATION_H
#define DOM_NOTATION_H

#include "modules/dom/src/domcore/node.h"

#ifdef DOM_SUPPORT_NOTATION

#include "modules/xmlparser/xmldoctype.h"

class DOM_DocumentType;
class DOM_NamedNodeMap;

class DOM_Notation
	: public DOM_Node
{
protected:
	DOM_Notation(DOM_DocumentType *doctype, XMLDoctype::Notation *notation);

	DOM_DocumentType *doctype;
	XMLDoctype::Notation *notation;

public:
	static OP_STATUS Make(DOM_Notation *&notation, DOM_DocumentType *doctype, XMLDoctype::Notation *xml_notation);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_NOTATION || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	XMLDoctype::Notation *GetXMLNotation() { return notation; }
};

class DOM_NotationsMapImpl
	: public DOM_NamedNodeMapImpl
{
protected:
	DOM_DocumentType *doctype;
	DOM_NamedNodeMap *notations;
	XMLDoctype *xml_doctype;

public:
	DOM_NotationsMapImpl(DOM_DocumentType *doctype, XMLDoctype *xml_doctype);
	void SetNotations(DOM_NamedNodeMap *notations);

	virtual void GCTrace();

	virtual int Length();
	virtual int Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual int GetNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
	virtual int RemoveNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
	virtual int SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
};

#endif // DOM_SUPPORT_NOTATION
#endif // DOM_NOTATION_H
