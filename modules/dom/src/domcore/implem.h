/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_DOMIMPLEMENTATION_
#define _DOM_DOMIMPLEMENTATION_

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

// This list must be synced with the list starting with DOM_FEATURES_START in implem.cpp
class DOM_FeatureInfo
{
public:
	enum
	{
		FEATURE_CORE,
		FEATURE_XML,
		FEATURE_HTML,
		FEATURE_XHTML,
#ifdef DOM3_EVENTS
		FEATURE_EVENTS,
		FEATURE_UIEVENTS,
		FEATURE_MOUSEEVENTS,
#ifdef DOM2_MUTATION_EVENTS
		FEATURE_MUTATIONEVENTS,
#endif // DOM2_MUTATION_EVENTS
		FEATURE_HTMLEVENTS,
#else // DOM3_EVENTS
		FEATURE_EVENTS,
		FEATURE_UIEVENTS,
		FEATURE_MOUSEEVENTS,
#ifdef DOM2_MUTATION_EVENTS
		FEATURE_MUTATIONEVENTS,
#endif // DOM2_MUTATION_EVENTS
		FEATURE_HTMLEVENTS,
#endif // DOM3_EVENTS
		FEATURE_VIEWS,
		FEATURE_STYLESHEETS,
		FEATURE_CSS,
		FEATURE_CSS2,
#ifdef DOM2_RANGE
		FEATURE_RANGE,
#endif // DOM2_RANGE
#ifdef DOM2_TRAVERSAL
		FEATURE_TRAVERSAL,
#endif // DOM2_TRAVERSAL
#if defined DOM3_LOAD || defined DOM3_SAVE
		FEATURE_LS,
		FEATURE_LS_ASYNC,
#endif // DOM3_LOAD || DOM3_SAVE
#ifdef DOM3_XPATH
		FEATURE_XPATH,
#endif // DOM3_XPATH
#ifdef DOM_SELECTORS_API
		FEATURE_SELECTORS_API,
#endif // DOM_SELECTORS_API
		FEATURES_COUNT
	};

	enum
	{
		VERSION_NONE = 0,
		VERSION_1_0  = 0x01,
		VERSION_2_0  = 0x02,
		VERSION_3_0  = 0x04,
		VERSION_ANY  = 0xff
	};

	const char *name;
	unsigned version;
};

class DOM_DOMImplementation
	: public DOM_Object
{
protected:
	DOM_DOMImplementation() {}

	static void ConstructDOMImplementationL(ES_Object *object, DOM_Runtime *runtime);
	static OP_STATUS ConstructDOMImplementation(ES_Object *object, DOM_Runtime *runtime) { TRAPD(status, ConstructDOMImplementationL(object, runtime)); return status; }

public:
	static OP_STATUS Make(DOM_DOMImplementation *&implementation, DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DOMIMPLEMENTATION || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION_WITH_DATA(accessFeature);
	DOM_DECLARE_FUNCTION(createDocument);
	DOM_DECLARE_FUNCTION(createDocumentType);

#ifdef DOM3_LOAD
	DOM_DECLARE_FUNCTION(createLSParser);
	DOM_DECLARE_FUNCTION(createLSInput);
#endif // DOM3_LOAD

#ifdef DOM3_SAVE
	DOM_DECLARE_FUNCTION(createLSSerializer);
	DOM_DECLARE_FUNCTION(createLSOutput);
#endif // DOM3_SAVE

#ifdef DOM_HTTP_SUPPORT
	DOM_DECLARE_FUNCTION(createHTTPRequest);
#endif // DOM_HTTP_SUPPORT
};

#endif
