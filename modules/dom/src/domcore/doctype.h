/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOCTYPE_H
#define DOM_DOCTYPE_H

#include "modules/dom/src/domcore/node.h"

class XMLDocumentInformation;

class XMLDoctype;

class DOM_DocumentType
	: public DOM_Node
{
private:
	DOM_DocumentType();

	HTML_Element *this_element;
	XMLDocumentInformation *xml_document_info;

	uni_char *name;
	uni_char *public_id;
	uni_char *system_id;

	void CleanDT();

public:
	static OP_STATUS Make(DOM_DocumentType *&doctype, HTML_Element *this_element, DOM_Document *owner_document);
	static OP_STATUS Make(DOM_DocumentType *&doctype, DOM_EnvironmentImpl *environment, const uni_char *name, const uni_char *public_id, const uni_char *system_id);

	virtual ~DOM_DocumentType();

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime );
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOCUMENTTYPE || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	const uni_char *GetName() { return name; }
	const uni_char *GetPublicId() { return public_id; }
	const uni_char *GetSystemId() { return system_id; }

	OP_STATUS CopyFrom(DOM_DocumentType* dt);

	void SetThisElement(HTML_Element *element) { this_element = element; }
	HTML_Element *GetThisElement() { return this_element; }

	XMLDoctype *GetXMLDoctype();
};

#endif // DOM_DOCTYPE_H
