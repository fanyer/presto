/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcore/domstaticnodelist.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/domutils.h"

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"

/* static */ OP_STATUS
DOM_StaticNodeList::Make(DOM_StaticNodeList *&dom_list, OpVector<HTML_Element>& elements, DOM_Document *document)
{
	DOM_Runtime *runtime = document->GetRuntime();
	DOM_EnvironmentImpl* environment = runtime->GetEnvironment();

	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(dom_list = OP_NEW(DOM_StaticNodeList, ()), runtime, runtime->GetPrototype(DOM_Runtime::NODELIST_PROTOTYPE), "NodeList"));

	OP_STATUS status = OpStatus::OK;
	for(unsigned i = 0; i < elements.GetCount(); i++)
	{
		DOM_Node* node;
		if (OpStatus::IsError(status = environment->ConstructNode(node, elements.Get(i), document)))
			break;

		if (OpStatus::IsError(status = dom_list->m_dom_nodes.Add(node)))
			break;
	}
	return status;
}

/* virtual */
DOM_StaticNodeList::~DOM_StaticNodeList()
{
	m_dom_nodes.Clear();
	/* the list is optionally linked into a cache holding querySelector() results; remove. */
	Out();
}

/* virtual */ void
DOM_StaticNodeList::GCTrace()
{
	for (unsigned i = 0; i < m_dom_nodes.GetCount(); ++i)
		GCMark(m_dom_nodes.Get(i));
}

/* virtual */ void
DOM_StaticNodeList::DOMChangeRuntime(DOM_Runtime *new_runtime)
{
	for (unsigned i = 0; i < m_dom_nodes.GetCount(); ++i)
		m_dom_nodes.Get(i)->DOMChangeRuntime(new_runtime);
}

/* virtual */ ES_GetState
DOM_StaticNodeList::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, m_dom_nodes.GetCount());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_StaticNodeList::GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && static_cast<unsigned>(property_index) < m_dom_nodes.GetCount())
	{
		DOMSetObject(value, m_dom_nodes.Get(property_index));
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_StaticNodeList::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
		return PUT_READ_ONLY;
	else
		return DOM_Object::PutName(property_name, value, origining_runtime);

}

/* virtual */ ES_GetState
DOM_StaticNodeList::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	count = m_dom_nodes.GetCount();
	return GET_SUCCESS;
}

/* static */ int
DOM_StaticNodeList::getItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(list, DOM_TYPE_STATIC_NODE_LIST, DOM_StaticNodeList);
	DOM_CHECK_ARGUMENTS("n");

	DOMSetNull(return_value);

	double index = argv[0].value.number;

	if (index < 0 || index > UINT_MAX)
		return ES_VALUE;

	unsigned property_index = static_cast<unsigned>(index);

	if (property_index < list->m_dom_nodes.GetCount())
		DOMSetObject(return_value, list->m_dom_nodes.Get(property_index));

	return ES_VALUE;
}
