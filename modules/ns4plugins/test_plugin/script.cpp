/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Methods called by browser upon DOM object access and their helper functions.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include "common.h"


static void NPO_Invalidate(NPObject* object)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	if (it != g_object_instance_map.end())
		it->second->Invalidate(object);
}


static void NPO_Deallocate(NPObject* object)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	if (it != g_object_instance_map.end())
		it->second->Deallocate(object);
}


static bool NPO_HasMethod(NPObject* object, NPIdentifier name)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->HasMethod(object, name) : false;
}


static bool NPO_Invoke(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->Invoke(object, name, args, argc, result) : false;
}


static bool NPO_InvokeDefault(NPObject* object, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->InvokeDefault(object, args, argc, result) : false;
}


static bool NPO_HasProperty(NPObject* object, NPIdentifier name)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->HasProperty(object, name) : false;
}


static bool NPO_GetProperty(NPObject* object, NPIdentifier name, NPVariant* result)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->GetProperty(object, name, result) : false;
}


static bool NPO_SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->SetProperty(object, name, value) : false;
}


static bool NPO_RemoveProperty(NPObject* object, NPIdentifier name)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->RemoveProperty(object, name) : false;
}


static bool NPO_Enumerate(NPObject* object, NPIdentifier** value, uint32_t* count)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->Enumerate(object, value, count) : false;
}


static bool NPO_Construct(NPObject* object, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	return it != g_object_instance_map.end() ? it->second->Construct(object, args, argc, result) : false;
}


/**
 * Build object class required by NPN_CreateObject.
 *
 * Returns a static object valid until the plug-in library is unloaded.
 *
 * @See https://developer.mozilla.org/en/NPClass.
 */
NPClass* build_object_class()
{
	static NPClass s_class;
	static bool s_class_initialized = false;

	if (!s_class_initialized)
	{
		memset(&s_class, 0, sizeof(s_class));
		s_class.structVersion = 3;

		s_class.deallocate = NPO_Deallocate;
		s_class.invalidate = NPO_Invalidate;
		s_class.hasMethod = NPO_HasMethod;
		s_class.invoke = NPO_Invoke;
		s_class.invokeDefault = NPO_InvokeDefault;
		s_class.hasProperty = NPO_HasProperty;
		s_class.getProperty = NPO_GetProperty;
		s_class.setProperty = NPO_SetProperty;
		s_class.removeProperty = NPO_RemoveProperty;
		s_class.enumerate = NPO_Enumerate;
		s_class.construct = NPO_Construct;

		s_class_initialized = true;
	}

	return &s_class;
}
