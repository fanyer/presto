/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/entity.h"
#include "modules/dom/src/domcore/doctype.h"

#ifdef DOM_SUPPORT_ENTITY

#include "modules/logdoc/htm_elm.h"
#include "modules/dom/src/domload/lsinput.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/tempbuf.h"

class DOM_EntityParseThread
	: public ES_Thread
{
protected:
	DOM_Entity *entity;

	DOM_EntityParseThread();
	~DOM_EntityParseThread();

public:
	static OP_STATUS Start(DOM_Entity *entity, DOM_Runtime *origining_runtime);

	virtual OP_STATUS EvaluateThread();
	virtual OP_STATUS Signal(ES_ThreadSignal signal);
};

DOM_EntityParseThread::DOM_EntityParseThread()
	: ES_Thread(NULL),
	  entity(NULL)
{
}

DOM_EntityParseThread::~DOM_EntityParseThread()
{
	if (entity)
		entity->GetRuntime()->Unprotect(*entity);
}

/* static */ OP_STATUS
DOM_EntityParseThread::Start(DOM_Entity *entity, DOM_Runtime *origining_runtime)
{
	DOM_EntityParseThread *thread = OP_NEW(DOM_EntityParseThread, ());
	if (!thread)
		return OpStatus::ERR_NO_MEMORY;

	if (!entity->GetRuntime()->Protect(*entity))
	{
		OP_DELETE(thread);
		return OpStatus::ERR_NO_MEMORY;
	}

	/* The thread will unprotect the entity. */
	thread->entity = entity;

	ES_ThreadScheduler *scheduler = origining_runtime->GetESScheduler();

	OP_BOOLEAN result = scheduler->AddRunnable(thread, scheduler->GetCurrentThread());

	if (OpStatus::IsError(result))
		return result;
	else
		return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_EntityParseThread::EvaluateThread()
{
	if (!is_started)
	{
		is_started = TRUE;

		DOM_EnvironmentImpl *environment = ((DOM_Runtime *) scheduler->GetRuntime())->GetEnvironment();

		ES_Object *input;
		DOM_LSParser *parser;
		TempBuffer buffer;
		XMLDoctype::Entity *xml_entity = entity->GetXMLEntity();

		RETURN_IF_ERROR(buffer.Append(xml_entity->GetValue(), xml_entity->GetValueLength()));
		RETURN_IF_ERROR(DOM_LSInput::Make(input, environment, buffer.GetStorage(), NULL));
		RETURN_IF_ERROR(DOM_LSParser::Make(parser, environment, FALSE));
		RETURN_IF_ERROR(entity->PutPrivate(DOM_PRIVATE_parser, parser));

		ES_Value argv[3], return_value;
		DOM_Object::DOMSetObject(&argv[0], input);
		DOM_Object::DOMSetObject(&argv[1], entity);
		DOM_Object::DOMSetNumber(&argv[2], DOM_LSParser::ACTION_APPEND_AS_CHILDREN);

		entity->SetBeingParsed(TRUE);

		int result = DOM_LSParser::parse(parser, argv, 3, &return_value, environment->GetDOMRuntime(), 2);

		if (result == ES_NO_MEMORY)
		{
			entity->SetBeingParsed(FALSE);
			return OpStatus::ERR_NO_MEMORY;
		}
		else if (result == (ES_SUSPEND | ES_RESTART))
			return OpStatus::OK;
	}

	entity->SetBeingParsed(FALSE);

	is_completed = TRUE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_EntityParseThread::Signal(ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED)
	{
		ES_Value value;

		if (entity->GetPrivate(DOM_PRIVATE_parser, &value) == OpBoolean::IS_TRUE)
		{
			DOM_LSParser *parser = DOM_VALUE2OBJECT(value, DOM_LSParser);
			DOM_LSParser::abort(parser, &value, 0, &value, (DOM_Runtime *) scheduler->GetRuntime());
		}
	}

	return ES_Thread::Signal(signal);
}

DOM_Entity::DOM_Entity(DOM_DocumentType *doctype, XMLDoctype::Entity *entity)
	: DOM_Node(ENTITY_NODE),
	  doctype(doctype),
	  entity(entity),
	  placeholder(NULL),
	  is_being_parsed(FALSE)
{
	SetOwnerDocument(doctype->GetOwnerDocument());
}

/* static */ OP_STATUS
DOM_Entity::Make(DOM_Entity *&entity, DOM_DocumentType *doctype, XMLDoctype::Entity *xml_entity, DOM_Runtime *origining_runtime)
{
	DOM_Runtime *runtime = doctype->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(entity = OP_NEW(DOM_Entity, (doctype, xml_entity)), runtime, runtime->GetPrototype(DOM_Runtime::NODE_PROTOTYPE), "Entity"));

	RETURN_IF_ERROR(HTML_Element::DOMCreateNullElement(entity->placeholder, doctype->GetEnvironment()));
	entity->placeholder->SetESElement(entity);

	if (xml_entity->GetValueLength() != 0)
		return DOM_EntityParseThread::Start(entity, origining_runtime);
	else
		return OpStatus::OK;
}

/* Defined in element.cpp. */

/* virtual */
DOM_Entity::~DOM_Entity()
{
	if (placeholder)
		FreeElementTree(placeholder);
}

/* virtual */
ES_GetState
DOM_Entity::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
		DOMSetString(value, entity->GetName());
		return GET_SUCCESS;

	case OP_ATOM_firstChild:
		return DOMSetElement(value, placeholder->FirstChildActual());

	case OP_ATOM_lastChild:
		return DOMSetElement(value, placeholder->LastChildActual());

	case OP_ATOM_childNodes:
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_childNodes);
			if (result != GET_FAILED)
				return result;
			else
			{
				DOM_SimpleCollectionFilter filter(CHILDNODES);
				DOM_Collection *collection;

				GET_FAILED_IF_ERROR(DOM_Collection::MakeNodeList(collection, GetEnvironment(), this, FALSE, FALSE, filter));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_childNodes, *collection));

				DOMSetObject(value, collection);
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_publicId:
	case OP_ATOM_systemId:
	case OP_ATOM_notationName:
		if (entity->GetValueType() != XMLDoctype::Entity::VALUE_TYPE_Internal)
			if (property_name == OP_ATOM_publicId)
				DOMSetString(value, entity->GetPubid());
			else if (property_name == OP_ATOM_systemId)
				DOMSetString(value, entity->GetSystem());
			else
				DOMSetString(value, entity->GetNDataName());
		else
			DOMSetNull(value);
		return GET_SUCCESS;
	}

	return DOM_Node::GetName(property_name, value, origining_runtime);
}

/* virtual */
ES_PutState
DOM_Entity::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_publicId:
	case OP_ATOM_systemId:
	case OP_ATOM_notationName:
		return PUT_READ_ONLY;
	}

	return DOM_Node::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_Entity::GCTraceSpecial(BOOL via_tree)
{
	DOM_Node::GCTraceSpecial(via_tree);
	GCMark(doctype);

	if (!via_tree)
		TraceElementTree(placeholder);
}

DOM_EntitiesMapImpl::DOM_EntitiesMapImpl(DOM_DocumentType *doctype, XMLDoctype *xml_doctype)
	: doctype(doctype),
	  entities(NULL),
	  xml_doctype(xml_doctype)
{
}

void
DOM_EntitiesMapImpl::SetEntities(DOM_NamedNodeMap *new_entities)
{
	entities = new_entities;
}

/* virtual */ void
DOM_EntitiesMapImpl::GCTrace()
{
	DOM_Object::GCMark(doctype);
	DOM_Object::GCMark(entities);
}

/* virtual */ int
DOM_EntitiesMapImpl::Length()
{
	return xml_doctype ? xml_doctype->GetEntitiesCount(XMLDoctype::Entity::TYPE_General) : 0;
}

/* virtual */ int
DOM_EntitiesMapImpl::Item(int index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	DOM_Object::DOMSetNull(return_value);

	if (xml_doctype && index >= 0)
		if (XMLDoctype::Entity *xml_entity = xml_doctype->GetEntity(XMLDoctype::Entity::TYPE_General, index))
			return GetNamedItem(NULL, xml_entity->GetName(), TRUE, return_value, (DOM_Runtime *) origining_runtime);

	return ES_VALUE;
}

/* virtual */ int
DOM_EntitiesMapImpl::GetNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_Object::DOMSetNull(return_value);

	if (xml_doctype && !ns_uri)
		if (XMLDoctype::Entity *xml_entity = xml_doctype->GetEntity(XMLDoctype::Entity::TYPE_General, item_name))
			if (!entities->GetInternalValue(xml_entity->GetName(), return_value))
			{
				DOM_Entity *entity;
				CALL_FAILED_IF_ERROR(DOM_Entity::Make(entity, doctype, xml_entity, (DOM_Runtime *) origining_runtime));
				DOM_Object::DOMSetObject(return_value, entity);
				CALL_FAILED_IF_ERROR(entities->SetInternalValue(xml_entity->GetName(), *return_value));
			}

	return ES_VALUE;
}

/* virtual */ int
DOM_EntitiesMapImpl::RemoveNamedItem(const uni_char *ns_uri, const uni_char *item_name, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	return entities->CallDOMException(DOM_Object::NO_MODIFICATION_ALLOWED_ERR, return_value);
}

/* virtual */ int
DOM_EntitiesMapImpl::SetNamedItem(DOM_Node *node, BOOL ns, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	return entities->CallDOMException(DOM_Object::NO_MODIFICATION_ALLOWED_ERR, return_value);
}

#endif // DOM_SUPPORT_ENTITY
