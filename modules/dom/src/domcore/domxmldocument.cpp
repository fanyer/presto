/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domxmldocument.h"

#ifdef DOM3_LOAD
# include "modules/dom/src/domload/lsparser.h"
# include "modules/dom/src/domload/lsinput.h"
#endif // DOM3_LOAD

DOM_XMLDocument::DOM_XMLDocument(DOM_DOMImplementation *implementation)
	: DOM_Document(implementation, TRUE)
#ifdef DOM3_LOAD
	, async(TRUE)
#endif // DOM3_LOAD
{
}

/* status */ OP_STATUS
DOM_XMLDocument::Make(DOM_Document *&document, DOM_DOMImplementation *implementation, BOOL create_placeholder)
{
	DOM_Runtime *runtime = implementation->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(document = OP_NEW(DOM_XMLDocument, (implementation)), runtime, runtime->GetPrototype(DOM_Runtime::XMLDOCUMENT_PROTOTYPE), "XMLDocument"));

	if (create_placeholder)
		RETURN_IF_ERROR(document->ResetPlaceholderElement());

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_XMLDocument::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef DOM3_LOAD
	if (property_name == OP_ATOM_async)
	{
		DOMSetBoolean(value, async);
		return GET_SUCCESS;
	}
#endif // DOM3_LOAD

	return DOM_Document::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_XMLDocument::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef DOM3_LOAD
	if (property_name == OP_ATOM_async)
	{
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		async = value->value.boolean;
		return PUT_SUCCESS;
	}
#endif // DOM3_LOAD

	return DOM_Document::PutName(property_name, value, origining_runtime);
}

#ifdef DOM3_LOAD

/* static */ int
DOM_XMLDocument::load(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (argc >= 0)
	{
		DOM_THIS_OBJECT(document, DOM_TYPE_XML_DOCUMENT, DOM_XMLDocument);
		DOM_CHECK_ARGUMENTS("s");

		DOM_LSParser *lsparser;
		CALL_FAILED_IF_ERROR(DOM_LSParser::Make(lsparser, document->GetEnvironment(), document->async));

		lsparser->SetIsDocumentLoad();

		ES_Object *lsinput;
		CALL_FAILED_IF_ERROR(DOM_LSInput::Make(lsinput, document->GetEnvironment(), NULL, argv[0].value.string));

		ES_Value arguments[3];
		DOMSetObject(&arguments[0], lsinput);
		DOMSetObject(&arguments[1], document);
		DOMSetNumber(&arguments[2], DOM_LSParser::ACTION_REPLACE_CHILDREN);

		return DOM_LSParser::parse(lsparser, arguments, 3, return_value, origining_runtime, 2);
	}
	else
		return DOM_LSParser::parse(NULL, NULL, -1, return_value, origining_runtime, 2);
}

#endif // DOM3_LOAD

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XMLDocument)
#ifdef DOM3_LOAD
	DOM_FUNCTIONS_FUNCTION(DOM_XMLDocument, DOM_XMLDocument::load, "load", "s-")
#endif // DOM3_LOAD
DOM_FUNCTIONS_END(DOM_XMLDocument)

