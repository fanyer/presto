/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/procinst.h"

#include "modules/logdoc/htm_elm.h"

DOM_ProcessingInstruction::DOM_ProcessingInstruction()
	: DOM_CharacterData(PROCESSING_INSTRUCTION_NODE)
{
}

/* static */ OP_STATUS
DOM_ProcessingInstruction::Make(DOM_ProcessingInstruction *&procinst, DOM_Node *reference, const uni_char *target, const uni_char *data)
{
	DOM_EnvironmentImpl *environment = reference->GetEnvironment();

	HTML_Element *element;
	DOM_Node *node;

	RETURN_IF_ERROR(HTML_Element::DOMCreateProcessingInstruction(element, environment, target, data));

	if (OpStatus::IsMemoryError(environment->ConstructNode(node, element, reference->GetOwnerDocument())))
	{
		HTML_Element::DOMFreeElement(element, environment);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_ASSERT(node->IsA(DOM_TYPE_PROCESSINGINSTRUCTION));

	procinst = (DOM_ProcessingInstruction *) node;
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_ProcessingInstruction::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
	case OP_ATOM_target:
		DOMSetString(value, this_element->DOMGetPITarget(GetEnvironment()));
		return GET_SUCCESS;

	default:
		return DOM_CharacterData::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ProcessingInstruction::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_nodeName:
	case OP_ATOM_target:
		return PUT_READ_ONLY;

	case OP_ATOM_data:
	case OP_ATOM_nodeValue:
	case OP_ATOM_textContent:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;

		PUT_FAILED_IF_ERROR(SetValue(value->value.string, static_cast<DOM_Runtime *>(origining_runtime)));
		return PUT_SUCCESS;
	}

	return DOM_CharacterData::PutName(property_name, value, origining_runtime);
}

