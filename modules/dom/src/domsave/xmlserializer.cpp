/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_SAVE

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domsave/lsserializer.h"
#include "modules/dom/src/domsave/xmlserializer.h"

/* static */ OP_STATUS
DOM_XMLSerializer::Make(DOM_XMLSerializer *&xmlserializer, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(xmlserializer = OP_NEW(DOM_XMLSerializer, ()), runtime, runtime->GetPrototype(DOM_Runtime::XMLSERIALIZER_PROTOTYPE), "XMLSerializer");
}

/* static */ int
DOM_XMLSerializer::serializeToString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(xmlserializer, DOM_TYPE_XMLSERIALIZER, DOM_XMLSerializer);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(root, 0, DOM_TYPE_NODE, DOM_Node);

	DOM_EnvironmentImpl *environment = xmlserializer->GetEnvironment();

	DOM_LSSerializer *lsserializer;
	CALL_FAILED_IF_ERROR(DOM_LSSerializer::Make(lsserializer, environment));

	ES_Value arguments[1];
	DOMSetObject(&arguments[0], root);

	return DOM_LSSerializer::write(lsserializer, arguments, 1, return_value, origining_runtime, 2);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XMLSerializer)
	DOM_FUNCTIONS_FUNCTION(DOM_XMLSerializer, DOM_XMLSerializer::serializeToString, "serializeToString", "-")
DOM_FUNCTIONS_END(DOM_XMLSerializer)

/* virtual */ int
DOM_XMLSerializer_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_XMLSerializer *xmlserializer;
	CALL_FAILED_IF_ERROR(DOM_XMLSerializer::Make(xmlserializer, GetEnvironment()));

	DOMSetObject(return_value, xmlserializer);
	return ES_VALUE;
}

#endif // DOM3_SAVE
