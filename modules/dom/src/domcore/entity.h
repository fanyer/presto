/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_ENTITY_H
#define DOM_ENTITY_H

#include "modules/dom/src/domcore/node.h"

#ifdef DOM_SUPPORT_ENTITY

#include "modules/xmlparser/xmldoctype.h"

class DOM_DocumentType;
class DOM_NamedNodeMap;

class DOM_Entity
	: public DOM_Node
{
protected:
	DOM_Entity(DOM_DocumentType *doctype, XMLDoctype::Entity *entity);

	DOM_DocumentType *doctype;
	XMLDoctype::Entity *entity;
	HTML_Element *placeholder;
	BOOL is_being_parsed;

public:
	static OP_STATUS Make(DOM_Entity *&entity, DOM_DocumentType *doctype, XMLDoctype::Entity *xml_entity, DOM_Runtime *origining_runtime);

	virtual ~DOM_Entity();

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_ENTITY || DOM_Node::IsA(type); }
	virtual void GCTraceSpecial(BOOL via_tree);

	XMLDoctype::Entity *GetXMLEntity() { return entity; }
	HTML_Element *GetPlaceholderElement() { return placeholder; }
	void ClearPlaceholderElement() { placeholder = NULL; }

	BOOL IsBeingParsed() { return is_being_parsed; }
	void SetBeingParsed(BOOL value) { is_being_parsed = value; }
};

class DOM_EntitiesMapImpl
	: public DOM_NamedNodeMapImpl
{
protected:
	DOM_DocumentType *doctype;
	DOM_NamedNodeMap *entities;
	XMLDoctype *xml_doctype;

public:
	DOM_EntitiesMapImpl(DOM_DocumentType *doctype, XMLDoctype *xml_doctype);
	void SetEntities(DOM_NamedNodeMap *entities);

	virtual void GCTrace();

	virtual int Length();
	virtual int Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual int GetNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
	virtual int RemoveNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
	virtual int SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime);
};

#endif // DOM_SUPPORT_ENTITY
#endif // DOM_ENTITY_H
