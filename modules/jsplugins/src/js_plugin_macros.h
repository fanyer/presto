/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JSPLUGINS_JS_PLUGIN_MACROS_H
#define JSPLUGINS_JS_PLUGIN_MACROS_H

/** Get EcmaScript_Object from a jsplugin_obj.
  * @param obj jsplugin_obj instance. */
static inline EcmaScript_Object *GET_ECMASCRIPT_OBJECT(const jsplugin_obj *obj)
{
	return static_cast<EcmaScript_Object *>(obj->opera_private);
}

/** Get JS_Plugin_Object from a jsplugin_obj.
  * @param obj jsplugin_obj instance. */
static inline JS_Plugin_Object *GET_JS_PLUGIN_OBJECT(const jsplugin_obj *obj)
{
	return static_cast<JS_Plugin_Object *>(static_cast<EcmaScript_Object *>(obj->opera_private));
}

/** Get JS_Plugin_Native from a jsplugin_obj.
  * @param obj jsplugin_obj instance. */
static inline JS_Plugin_Native *GET_JS_PLUGIN_NATIVE_OBJECT(const jsplugin_obj *obj)
{
	return static_cast<JS_Plugin_Native *>(static_cast<EcmaScript_Object *>(obj->opera_private));
}

/** Get ES_Object from a jsplugin_obj.
  * @param obj jsplugin_obj instance. */
static inline ES_Object *GET_ES_OBJECT(const jsplugin_obj *obj)
{
	ES_Object *eso = GET_ECMASCRIPT_OBJECT(obj)->GetNativeObject();
	if (op_strcmp(ES_Runtime::GetClass(eso), "JS_Plugin_Native") == 0)
		return GET_JS_PLUGIN_NATIVE_OBJECT(obj)->GetShadow();
	else
		return eso;
}

#endif // JSPLUGINS_JS_PLUGIN_MACROS_H
