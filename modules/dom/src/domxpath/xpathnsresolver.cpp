/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_XPATH

#include "modules/dom/src/domxpath/xpathnsresolver.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"

#include "modules/logdoc/htm_elm.h"

DOM_XPathNSResolver::DOM_XPathNSResolver(DOM_Node *node)
	: node(node)
{
}

/* static */ OP_STATUS
DOM_XPathNSResolver::Make(DOM_XPathNSResolver *&resolver, DOM_Node *node)
{
	DOM_Runtime *runtime = node->GetRuntime();
	return DOMSetObjectRuntime(resolver = OP_NEW(DOM_XPathNSResolver, (node)), runtime, runtime->GetPrototype(DOM_Runtime::XPATHNSRESOLVER_PROTOTYPE), "XPathNSResolver");
}

/* virtual */ void
DOM_XPathNSResolver::GCTrace()
{
	GCMark(node);
}

/* static */ int
DOM_XPathNSResolver::lookupNamespaceURI(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(resolver, DOM_TYPE_XPATHNSRESOLVER, DOM_XPathNSResolver);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char *ns_prefix = argv[0].value.string, *ns_uri = NULL;

	if (*ns_prefix)
		ns_uri = resolver->LookupNamespaceURI(ns_prefix);

	if (ns_uri)
		DOMSetString(return_value, ns_uri);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

const uni_char *
DOM_XPathNSResolver::LookupNamespaceURI(const uni_char *prefix)
{
	return node->GetThisElement()->DOMLookupNamespaceURI(GetEnvironment(), prefix);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_XPathNSResolver)
	DOM_FUNCTIONS_FUNCTION(DOM_XPathNSResolver, DOM_XPathNSResolver::lookupNamespaceURI, "lookupNamespaceURI", "s-")
DOM_FUNCTIONS_END(DOM_XPathNSResolver)

#endif // DOM3_XPATH
