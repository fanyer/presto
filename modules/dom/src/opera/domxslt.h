/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMXSLT_H
#define DOM_DOMXSLT_H

#ifdef DOM_XSLT_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/xmlutils/xmltypes.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/util/simset.h"
#include "modules/xslt/xslt.h"

class DOM_XSLTParseCallback;
class DOM_XSLTTransformCallback;
class DOM_XSLTTransform_State;
class XMLSerializer;

class DOM_XSLTProcessor
	: public DOM_Object,
	  public Link
{
protected:
	DOM_XSLTProcessor();
	~DOM_XSLTProcessor();

	XSLT_StylesheetParser *stylesheetparser;
	XSLT_Stylesheet *stylesheet;
	XMLVersion version;
	BOOL is_disabled;

	friend class DOM_XSLTParseCallback;
	DOM_XSLTParseCallback *parse_callback;

	friend class DOM_XSLTTransformCallback;
	DOM_XSLTTransformCallback *transform_callback;

	friend class DOM_XSLTTransform_State;
	DOM_XSLTTransform_State *transform_state;

	XMLSerializer *serializer;

	class ParameterValue
	{
	public:
		XMLExpandedName name;
		ES_Value value;
		ParameterValue *next;
	} *parameter_values;

	OP_STATUS SetParameters(DOM_XSLTTransform_State *state, XSLT_Stylesheet::Input &input);

	void RemoveParameter(const XMLExpandedName &name);
	void ClearParameters();

public:
	static OP_STATUS Make(DOM_XSLTProcessor *&processor, DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XSLTPROCESSOR || DOM_Object::IsA(type); }
	virtual void GCTrace();

	void Cleanup();

	DOM_DECLARE_FUNCTION(importStylesheet);
	DOM_DECLARE_FUNCTION(getParameter);
	DOM_DECLARE_FUNCTION(setParameter);
	DOM_DECLARE_FUNCTION(removeParameter);
	DOM_DECLARE_FUNCTION(clearParameters);
	DOM_DECLARE_FUNCTION(reset);
	enum { FUNCTIONS_ARRAY_SIZE = 7 };

	DOM_DECLARE_FUNCTION_WITH_DATA(transform);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };
};

class DOM_XSLTProcessor_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_XSLTProcessor_Constructor();

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_XSLT_SUPPORT
#endif // DOM_DOMXSLT_H
