/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM3_LOAD

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domload/lsinput.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domload/domparser.h"

/* static */ OP_STATUS
DOM_DOMParser::Make(DOM_DOMParser *&domparser, DOM_EnvironmentImpl *environment)
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
	return DOMSetObjectRuntime(domparser = OP_NEW(DOM_DOMParser, ()), runtime, runtime->GetPrototype(DOM_Runtime::DOMPARSER_PROTOTYPE), "DOMParser");
}

/**
 * Returns if the MIME type is valid per the DOM Parsing and Serialization spec.
 */
static BOOL
ValidXMLMimeType(const uni_char *mime_type)
{
	if (uni_strcmp(mime_type, "text/xml") == 0 ||
		uni_strcmp(mime_type, "application/xml") == 0 ||
		uni_strcmp(mime_type, "application/xhtml+xml") == 0)
		return TRUE;

	return FALSE;
}

/* static */ int
DOM_DOMParser::parseFromString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// If the parser fails with a parse error, then we shouldn't throw an exception, but
	// rather create a small xml document with a user friendly xml error message inside.
	// This is not documented, but that's what Mozilla, the creator of the API do and
	// by bug 280221 we have to copy that.

	DOM_THIS_OBJECT(domparser, DOM_TYPE_DOMPARSER, DOM_DOMParser);

	DOM_LSParser *lsparser = NULL;
	ES_Value arguments[1];
	BOOL parse_error_document = FALSE;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("ss");

		if (!ValidXMLMimeType(argv[1].value.string))
			return domparser->CallDOMException(DOM_Object::NOT_SUPPORTED_ERR, return_value);

	again:
		DOM_EnvironmentImpl *environment = domparser->GetEnvironment();
		CALL_FAILED_IF_ERROR(DOM_LSParser::Make(lsparser, environment, FALSE));

		const uni_char *source;
		if (parse_error_document)
		{
			source = UNI_L("<parsererror xmlns='http://www.mozilla.org/newlayout/xml/parsererror.xml'>Error<sourcetext>Unknown source</sourcetext></parsererror>");
			lsparser->SetIsParsingErrorDocument();
		}
		else
			source = argv[0].value.string;

		ES_Object *lsinput;
		CALL_FAILED_IF_ERROR(DOM_LSInput::Make(lsinput, environment, source, NULL));

		DOMSetObject(&arguments[0], lsinput);

		argv = arguments;
		argc = 1;
	}
	else
		lsparser = DOM_VALUE2OBJECT(*return_value, DOM_LSParser);

	int call_status = DOM_LSParser::parse(lsparser, argv, argc, return_value, origining_runtime, 0);

	if (call_status == ES_EXCEPTION && !lsparser->GetIsParsingErrorDocument())
	{
		parse_error_document = TRUE;
		goto again;
	}

	return call_status;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_DOMParser)
	DOM_FUNCTIONS_FUNCTION(DOM_DOMParser, DOM_DOMParser::parseFromString, "parseFromString", "ss-")
DOM_FUNCTIONS_END(DOM_DOMParser)

/* virtual */ int
DOM_DOMParser_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_DOMParser *domparser;
	CALL_FAILED_IF_ERROR(DOM_DOMParser::Make(domparser, GetEnvironment()));

	DOMSetObject(return_value, domparser);
	return ES_VALUE;
}

#endif // DOM3_LOAD
