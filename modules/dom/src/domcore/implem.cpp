/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"

#include "modules/dom/src/domcore/implem.h"
#include "modules/dom/src/domcore/domxmldocument.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domload/lsinput.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domsave/lsserializer.h"
#include "modules/dom/src/domsave/lsoutput.h"
#include "modules/dom/src/opera/domhttp.h"

#ifdef SVG_DOM
# include "modules/svg/svg_dominterfaces.h"
#endif // SVG_DOM

#include "modules/logdoc/htm_elm.h"
#include "modules/xmlutils/xmlutils.h"

/* static */ void
DOM_DOMImplementation::ConstructDOMImplementationL(ES_Object *object, DOM_Runtime *runtime)
{
	AddFunctionL(object, runtime, accessFeature, 0, "hasFeature", "sS-");
	AddFunctionL(object, runtime, createDocument, "createDocument", "SZ-");
	AddFunctionL(object, runtime, createDocumentType, "createDocumentType", "zzs-");
	AddFunctionL(object, runtime, accessFeature, 1, "getFeature", "s-");

#ifdef DOM3_LOAD
	AddFunctionL(object, runtime, createLSParser, "createLSParser", "ns-");
	AddFunctionL(object, runtime, createLSInput, "createLSInput");

	PutNumericConstantL(object, "MODE_SYNCHRONOUS", DOM_LSParser::MODE_SYNCHRONOUS, runtime);
	PutNumericConstantL(object, "MODE_ASYNCHRONOUS", DOM_LSParser::MODE_ASYNCHRONOUS, runtime);
#endif // DOM3_LOAD

#ifdef DOM3_SAVE
	AddFunctionL(object, runtime, createLSSerializer, "createLSSerializer");
	AddFunctionL(object, runtime, createLSOutput, "createLSOutput");
#endif // DOM3_SAVE

#ifdef DOM_HTTP_SUPPORT
	AddFunctionL(object, runtime, createHTTPRequest, "createHTTPRequest", "ss-");
#endif // DOM_HTTP_SUPPORT
}

/* static */ OP_STATUS
DOM_DOMImplementation::Make(DOM_DOMImplementation *&implementation, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(implementation = OP_NEW(DOM_DOMImplementation, ()), runtime, runtime->GetObjectPrototype(), "DOMImplementation"));

	return ConstructDOMImplementation(*implementation, runtime);
}

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_FEATURES_START() void DOM_featureList_Init(DOM_GlobalData *global_data) { DOM_FeatureInfo *featureList = global_data->featureList;
# define DOM_FEATURES_ITEM(name_, version_) featureList->name = name_; featureList->version = version_; ++featureList;
# define DOM_FEATURES_END() featureList->name = NULL; }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_FEATURES_START() const DOM_FeatureInfo g_DOM_featureList[] = {
# define DOM_FEATURES_ITEM(name_, version_) { name_, version_ },
# define DOM_FEATURES_END() { NULL, DOM_FeatureInfo::VERSION_NONE } };
#endif // DOM_NO_COMPLEX_GLOBALS

/* Remember to update selftest/core/domimplementation.ot and the affected
   tests in testsuite/DOM-regression (run them and modify those that fail
   in expected ways) when you change anything in this table. */

// This list must be synced with the list named DOM_FeatureInfo in implem.h
DOM_FEATURES_START()
	DOM_FEATURES_ITEM("CORE",           DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("XML",            DOM_FeatureInfo::VERSION_1_0 | DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("HTML",           DOM_FeatureInfo::VERSION_1_0 | DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("XHTML",          DOM_FeatureInfo::VERSION_1_0 | DOM_FeatureInfo::VERSION_2_0)
#ifdef DOM3_EVENTS
	DOM_FEATURES_ITEM("EVENTS",         DOM_FeatureInfo::VERSION_2_0 | DOM_FeatureInfo::VERSION_3_0)
	DOM_FEATURES_ITEM("UIEVENTS",       DOM_FeatureInfo::VERSION_2_0 | DOM_FeatureInfo::VERSION_3_0)
	DOM_FEATURES_ITEM("MOUSEEVENTS",    DOM_FeatureInfo::VERSION_2_0 | DOM_FeatureInfo::VERSION_3_0)
#ifdef DOM2_MUTATION_EVENTS
	DOM_FEATURES_ITEM("MUTATIONEVENTS", DOM_FeatureInfo::VERSION_2_0 | DOM_FeatureInfo::VERSION_3_0)
#endif // DOM2_MUTATION_EVENTS
	DOM_FEATURES_ITEM("HTMLEVENTS",     DOM_FeatureInfo::VERSION_2_0 | DOM_FeatureInfo::VERSION_3_0)
#else // DOM3_EVENTS
	DOM_FEATURES_ITEM("EVENTS",         DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("UIEVENTS",       DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("MOUSEEVENTS",    DOM_FeatureInfo::VERSION_2_0)
#ifdef DOM2_MUTATION_EVENTS
	DOM_FEATURES_ITEM("MUTATIONEVENTS", DOM_FeatureInfo::VERSION_2_0)
#endif // DOM2_MUTATION_EVENTS
	DOM_FEATURES_ITEM("HTMLEVENTS",     DOM_FeatureInfo::VERSION_2_0)
#endif // DOM3_EVENTS
	DOM_FEATURES_ITEM("VIEWS",          DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("STYLESHEETS",    DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("CSS",            DOM_FeatureInfo::VERSION_2_0)
	DOM_FEATURES_ITEM("CSS2",           DOM_FeatureInfo::VERSION_2_0)
#ifdef DOM2_RANGE
	DOM_FEATURES_ITEM("RANGE",          DOM_FeatureInfo::VERSION_2_0)
#endif // DOM2_RANGE
#ifdef DOM2_TRAVERSAL
	DOM_FEATURES_ITEM("TRAVERSAL",      DOM_FeatureInfo::VERSION_2_0)
#endif // DOM2_TRAVERSAL
#if defined DOM3_LOAD || defined DOM3_SAVE
	DOM_FEATURES_ITEM("LS",             DOM_FeatureInfo::VERSION_3_0)
	DOM_FEATURES_ITEM("LS-ASYNC",       DOM_FeatureInfo::VERSION_3_0)
#endif // DOM3_LOAD || DOM3_SAVE
#ifdef DOM3_XPATH
	DOM_FEATURES_ITEM("XPATH",          DOM_FeatureInfo::VERSION_3_0)
#endif // DOM3_XPATH
#ifdef DOM_SELECTORS_API
	DOM_FEATURES_ITEM("SELECTORS-API",	DOM_FeatureInfo::VERSION_1_0)
#endif // DOM_SELECTORS_API
DOM_FEATURES_END()


/* static */ int
DOM_DOMImplementation::accessFeature(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	/* NOTE: this_object is NULL when called as Node.isSupported or Node.getFeature. */

	if (this_object)
		DOM_THIS_OBJECT_UNUSED(DOM_TYPE_DOMIMPLEMENTATION);

	DOM_CHECK_ARGUMENTS("sS");

	const uni_char *name = argv[0].value.string;
	unsigned version = DOM_FeatureInfo::VERSION_ANY;
	BOOL supported = FALSE;
#ifdef SVG_DOM
	BOOL found = FALSE;
#endif

	if (name[0] == '+')
		++name;

	if (argv[1].type == VALUE_STRING)
	{
		const uni_char *version_string = argv[1].value.string;

		if (*version_string)
			if (uni_str_eq(version_string, "1.0"))
				version = DOM_FeatureInfo::VERSION_1_0;
			else if (uni_str_eq(version_string, "2.0"))
				version = DOM_FeatureInfo::VERSION_2_0;
			else if (uni_str_eq(version_string, "3.0"))
				version = DOM_FeatureInfo::VERSION_3_0;
			else
				version = DOM_FeatureInfo::VERSION_NONE;
	}

	const DOM_FeatureInfo *features = g_DOM_featureList;

	if (version != DOM_FeatureInfo::VERSION_NONE)
		for (int index = 0; features[index].name; ++index)
			if (uni_stri_eq(name, features[index].name))
			{
#ifdef SVG_DOM
				found = TRUE;
#endif
				if ((features[index].version & version) != 0)
					supported = TRUE;
				break;
			}

#ifdef SVG_DOM
	if(!found)
	{
		const uni_char *version_string = NULL;
		if (argv[1].type == VALUE_STRING)
			version_string = argv[1].value.string;
		SVGDOM::HasFeature(name, version_string, supported);
	}
#endif // SVG_DOM

	if (data == 0)
		DOMSetBoolean(return_value, supported);
	else
	{
		/* this_object==NULL if called from DOM_Node::isSupported, but it calls
		   with data==0, so we should never have no this object here. */
		OP_ASSERT(this_object);

		DOMSetObject(return_value, supported ? this_object : NULL);
	}

	return ES_VALUE;
}

/* static */ int
DOM_DOMImplementation::createDocument(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);
	DOM_CHECK_ARGUMENTS("SZO");

	DOM_ARGUMENT_OBJECT(doctype, 2, DOM_TYPE_DOCUMENTTYPE, DOM_DocumentType);

	DOM_Document *document;
	BOOL is_html = FALSE;

	TempBuffer qname_buffer;
	qname_buffer.SetCachedLengthPolicy(TempBuffer::UNTRUSTED);

	const uni_char *qname = NULL;

	if (argv[1].type == VALUE_STRING_WITH_LENGTH)
	{
		qname = argv[1].value.string_with_length->string;

		if (uni_strlen(qname) != argv[1].value.string_with_length->length)
			/* Embedded null characters: not a valid character in element names: */
			return implementation->CallDOMException(INVALID_CHARACTER_ERR, return_value);
	}

	if (qname && argv[0].type == VALUE_STRING && uni_str_eq(argv[0].value.string, "http://www.w3.org/1999/xhtml") && uni_str_eq(qname, "html"))
		is_html = TRUE;

	if (is_html)
	{
		DOM_HTMLDocument *htmldocument;
		CALL_FAILED_IF_ERROR(DOM_HTMLDocument::Make(htmldocument, implementation, TRUE, TRUE));
		document = htmldocument;
	}
	else
		CALL_FAILED_IF_ERROR(DOM_XMLDocument::Make(document, implementation, TRUE));

	if (doctype)
	{
		if (doctype->GetThisElement()->Parent())
			return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

		if (doctype->GetRuntime() != document->GetRuntime())
			doctype->DOMChangeRuntime(document->GetRuntime());
		doctype->DOMChangeOwnerDocument(document);

		ES_Value arguments[2];
		DOMSetObject(&arguments[0], doctype);
		DOMSetNull(&arguments[1]);

		int result = DOM_Node::insertBefore(document, arguments, 2, return_value, origining_runtime);

		/* There couldn't possibly be any Mutation Events handlers involved here. */
		OP_ASSERT(result != (ES_SUSPEND | ES_RESTART));

		if (result != ES_VALUE)
			return result;
	}

	if (qname && *qname)
	{
		int result = DOM_Document::createNode(document, argv, 2, return_value, origining_runtime, 1);
		if (result != ES_VALUE)
			return result;

		ES_Value arguments[2];
		arguments[0] = *return_value;
		DOMSetNull(&arguments[1]);

		result = DOM_Node::insertBefore(document, arguments, 2, return_value, origining_runtime);

		/* There couldn't possibly be any Mutation Events handlers involved here. */
		OP_ASSERT(result != (ES_SUSPEND | ES_RESTART));

		if (result != ES_VALUE)
			return result;
	}

	DOMSetObject(return_value, document);
	return ES_VALUE;
}

/* static */ int
DOM_DOMImplementation::createDocumentType(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);
	DOM_CHECK_ARGUMENTS("zzs");

	const uni_char *qualifiedName = argv[0].value.string_with_length->string;
	if (uni_strlen(qualifiedName) != argv[0].value.string_with_length->length)
		/* Embedded null characters: not a valid character in element names: */
		return implementation->CallDOMException(INVALID_CHARACTER_ERR, return_value);

	const uni_char *publicId = argv[1].value.string_with_length->string;
	if (uni_strlen(publicId) != argv[1].value.string_with_length->length)
		/* Embedded null characters: not a valid character in a public ID literal: */
		return implementation->CallDOMException(INVALID_CHARACTER_ERR, return_value);

	const uni_char *systemId = argv[2].value.string;

	if (*qualifiedName && !XMLUtils::IsValidName(XMLVERSION_1_0, qualifiedName))
		return DOM_CALL_DOMEXCEPTION(INVALID_CHARACTER_ERR);
	else if (!XMLUtils::IsValidQName (XMLVERSION_1_0, qualifiedName))
		return DOM_CALL_DOMEXCEPTION(NAMESPACE_ERR);

	DOM_DocumentType *doctype;
	CALL_FAILED_IF_ERROR(DOM_DocumentType::Make(doctype, implementation->GetEnvironment(), qualifiedName, publicId, systemId));
	doctype->SetIsSignificant();

	DOMSetObject(return_value, doctype);
	return ES_VALUE;
}

#ifdef DOM3_LOAD

/* static */ int
DOM_DOMImplementation::createLSParser(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);
	DOM_CHECK_ARGUMENTS("ns");

	DOM_LSParser *parser;
	CALL_FAILED_IF_ERROR(DOM_LSParser::Make(parser, implementation->GetEnvironment(), argv[0].value.number == DOM_LSParser::MODE_ASYNCHRONOUS));

	DOMSetObject(return_value, parser);
	return ES_VALUE;
}

/* static */ int
DOM_DOMImplementation::createLSInput(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);

	ES_Object *input;
	CALL_FAILED_IF_ERROR(DOM_LSInput::Make(input, implementation->GetEnvironment()));

	DOMSetObject(return_value, input);
	return ES_VALUE;
}

#endif // DOM3_LOAD

#ifdef DOM3_SAVE

/* static */ int
DOM_DOMImplementation::createLSSerializer(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);

	DOM_LSSerializer *serializer;
	CALL_FAILED_IF_ERROR(DOM_LSSerializer::Make(serializer, implementation->GetEnvironment()));

	DOMSetObject(return_value, serializer);
	return ES_VALUE;
}

/* static */ int
DOM_DOMImplementation::createLSOutput(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);

	ES_Object *output;
	CALL_FAILED_IF_ERROR(DOM_LSOutput::Make(output, implementation->GetEnvironment()));

	DOMSetObject(return_value, output);
	return ES_VALUE;
}

#endif // DOM3_SAVE

#ifdef DOM_HTTP_SUPPORT

/* static */ int
DOM_DOMImplementation::createHTTPRequest(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(implementation, DOM_TYPE_DOMIMPLEMENTATION, DOM_DOMImplementation);
	DOM_CHECK_ARGUMENTS("ss");

	DOM_HTTPRequest *request;
	CALL_FAILED_IF_ERROR(DOM_HTTPRequest::Make(request, implementation->GetEnvironment(), argv[0].value.string, argv[1].value.string));

	DOMSetObject(return_value, request);
	return ES_VALUE;
}

#endif // DOM_HTTP_SUPPORT
