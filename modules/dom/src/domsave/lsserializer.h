/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_LSSERIALIZER
#define DOM_LSSERIALIZER

#ifdef DOM3_SAVE

#include "modules/dom/src/domobj.h"

class DOM_DOMConfiguration;

class DOM_LSSerializer
	: public DOM_Object
{
protected:
	DOM_LSSerializer();
	virtual ~DOM_LSSerializer();

	DOM_DOMConfiguration *config;
	uni_char *newLine;
	ES_Object *filter;
	unsigned whatToShow;

	enum
	{
		SHOW_ALL                       = 0xfffffffful, // Might be stored as -1 depending on compiler.
		SHOW_ELEMENT                   = 0x00000001ul,
		SHOW_ATTRIBUTE                 = 0x00000002ul,
		SHOW_TEXT                      = 0x00000004ul,
		SHOW_CDATA_SECTION             = 0x00000008ul,
		SHOW_ENTITY_REFERENCE          = 0x00000010ul,
		SHOW_ENTITY                    = 0x00000020ul,
		SHOW_PROCESSING_INSTRUCTION    = 0x00000040ul,
		SHOW_COMMENT                   = 0x00000080ul,
		SHOW_DOCUMENT                  = 0x00000100ul,
		SHOW_DOCUMENT_TYPE             = 0x00000200ul,
		SHOW_DOCUMENT_FRAGMENT         = 0x00000400ul,
		SHOW_NOTATION                  = 0x00000800ul
	};

public:
	enum
	{
		FILTER_EXCEPTION = -2,
		FILTER_ACCEPT = 1,
		FILTER_REJECT,
		FILTER_SKIP
	};

	static OP_STATUS Make(DOM_LSSerializer *&serializer, DOM_EnvironmentImpl *environment);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_LSSERIALIZER || DOM_Object::IsA(type); }
	virtual void GCTrace();

	const uni_char *GetNewLine() { return newLine; }
	ES_Object *GetFilter() { return filter; }

	DOM_DOMConfiguration *GetConfig() { return config; }

	DOM_DECLARE_FUNCTION_WITH_DATA(write);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 4 };
};

#endif // DOM3_SAVE
#endif // DOM_LSSERIALIZER
