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

#include "modules/dom/src/domcore/implem.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domhtml/htmlimplem.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domhtml/htmlelem.h"

DOM_HTMLDOMImplementation::DOM_HTMLDOMImplementation()
{
}

void
DOM_HTMLDOMImplementation::InitializeL()
{
	DOM_DOMImplementation::ConstructDOMImplementationL(*this, GetRuntime());

	AddFunctionL(createHTMLDocument, "createHTMLDocument", "s-");
}

/* static */ OP_STATUS
DOM_HTMLDOMImplementation::Make(DOM_HTMLDOMImplementation *&implementation, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(implementation = OP_NEW(DOM_HTMLDOMImplementation, ()), runtime, runtime->GetObjectPrototype(), "DOMImplementation"));

	TRAPD(status, implementation->InitializeL());
	return status;
}

int DOM_HTMLDOMImplementation::createHTMLDocument(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_HTML_IMPLEMENTATION, DOM_HTMLDOMImplementation);
	DOM_CHECK_ARGUMENTS("s");

	DOM_HTMLDocument *document;
	CALL_FAILED_IF_ERROR(DOM_HTMLDocument::Make(document, implementation, TRUE));

	DOM_DocumentType *doctype;
	CALL_FAILED_IF_ERROR(DOM_DocumentType::Make(doctype, origining_runtime->GetEnvironment(), UNI_L("html"), NULL, NULL));
	doctype->DOMChangeOwnerDocument(document);
	CALL_FAILED_IF_ERROR(document->InsertChild(doctype, document, origining_runtime));

    DOM_HTMLElement *html;
    CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(html, document, UNI_L("html")));

    DOM_HTMLElement *head;
    CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(head, document, UNI_L("head")));

	DOM_HTMLElement *title;
	CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(title, document, UNI_L("title")));

	DOM_Text *title_text;
	CALL_FAILED_IF_ERROR(DOM_Text::Make(title_text, document, argv[0].value.string));

	DOM_HTMLElement *body;
	CALL_FAILED_IF_ERROR(DOM_HTMLElement::CreateElement(body, document, UNI_L("body")));

	CALL_FAILED_IF_ERROR(document->InsertChild(html, NULL, origining_runtime));
	CALL_FAILED_IF_ERROR(html->InsertChild(head, NULL, origining_runtime));
	CALL_FAILED_IF_ERROR(head->InsertChild(title, NULL, origining_runtime));
	CALL_FAILED_IF_ERROR(title->InsertChild(title_text, NULL, origining_runtime));
	CALL_FAILED_IF_ERROR(html->InsertChild(body, NULL, origining_runtime));

	DOM_Object::DOMSetObject(return_value, document);
	return ES_VALUE;
}

