/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JS_MIMETYPE_H
#define JS_MIMETYPE_H

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

class Viewer;

class JS_MimeTypes
	: public DOM_Object
{
public:
	static OP_STATUS Make(JS_MimeTypes *&mimeTypes, DOM_EnvironmentImpl *environment);

	virtual ES_GetState	GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);
};

class JS_MimeType
	: public DOM_Object
{
private:
	JS_MimeType();

	uni_char*       description;
	uni_char*       suffixes;
	uni_char*       type;

public:
	static OP_STATUS Make(JS_MimeType *&mimetype, DOM_EnvironmentImpl *environment, Viewer *viewer);

	virtual ~JS_MimeType();
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#endif // JS_MIMETYPE_H
