/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JS_PLUGIN_H
#define JS_PLUGIN_H

#include "modules/dom/src/domobj.h"

class PluginViewer;

class JS_PluginArray
	: public DOM_Object
{
public:
	static OP_STATUS Make(JS_PluginArray *&plugins, DOM_EnvironmentImpl *environment);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
#ifdef NS4P_COMPONENT_PLUGINS
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_GetState GetIndexRestart(int property_index, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object);
#endif // NS4P_COMPONENT_PLUGINS
	virtual BOOL IsA(int type) { return type == DOM_TYPE_PLUGINARRAY || DOM_Object::IsA(type); }

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(refresh);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#ifdef _PLUGIN_SUPPORT_

class JS_Plugin
	: public DOM_Object
{
private:
	JS_Plugin();

	uni_char *description;
	uni_char *filename;
	uni_char *name;

public:
	static OP_STATUS Make(JS_Plugin *&plugin, DOM_EnvironmentImpl *environment, PluginViewer *plugin_viewer);

	virtual ~JS_Plugin();
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState	GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
};

#endif // _PLUGIN_SUPPORT_
#endif // JS_PLUGIN_H
