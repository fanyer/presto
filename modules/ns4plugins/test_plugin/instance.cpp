/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Plugin functionality and behavior common to all plugin instance types.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include <cassert>
#include <cctype>
#include <cstdio>

#include <list>

#include "common.h"

/**
 * Plugin instance constructor.
 *
 * Called by NPP_New.
 *
 * @param instance NPP instance.
 */
PluginInstance::PluginInstance(NPP instance, const char* bgcolor)
	: instance(instance), global_object(0), scriptable_object(0), plugin_window(0)
	, paint_count(0), setwindow_count(0), windowposchanged_count(0)
	, bgcolor(0), is_destroying(false)
#ifdef XP_MACOSX
	, ca_layer(NULL)
#endif // XP_MACOSX
{
	CountUse of(this);
	if (bgcolor)
		if ((this->bgcolor = static_cast<char*>(g_browser->memalloc(strlen(bgcolor) + 1))))
			memcpy(this->bgcolor, bgcolor, strlen(bgcolor) + 1);
}


/**
 * Plugin instance destructor.
 *
 * Called by NPP_Destroy.
 */
PluginInstance::~PluginInstance()
{
	is_destroying = true;

	/* Forget all remaining objects. */
	std::list<NPObject*> live_objects;

	for (ObjectInstanceMap::iterator it = g_object_instance_map.begin(); it != g_object_instance_map.end(); it++)
		if (it->second == this)
			live_objects.push_back(it->first);

	for (std::list<NPObject*>::iterator it = live_objects.begin(); it != live_objects.end(); it++)
		Deallocate(*it);

	/* Release additional resources. */
	if (bgcolor)
		g_browser->memfree(bgcolor);

#ifdef XP_MACOSX
	UninitializeCoreAnimationLayer();
#endif // XP_MACOSX
}


/**
 * Allocate plugin instance using browser supplied allocator.
 *
 * @param size Bytes required.
 *
 * @return Pointer to allocated memory.
 */
void* PluginInstance::operator new(size_t size) throw ()
{
	if (g_browser)
		return g_browser->memalloc(static_cast<uint32_t>(size));

	return 0;
}


/**
 * Free plugin instance using browser supplied allocator.
 *
 * @param Pointer to instance.
 */
void PluginInstance::operator delete(void* p)
{
	if (g_browser)
		g_browser->memfree(p);
}


/**
 * Add native method to our dom node.
 *
 * @param identifier Name of method.
 * @param method handler.
 */
void PluginInstance::AddMethod(const char* identifier, Method method)
{
	NPIdentifier id = g_browser->getstringidentifier(identifier);
	methods[id] = method;
}


/**
 * Remove native method from our dom node.
 *
 * @param identifier Name of method.
 */
void PluginInstance::RemoveMethod(const char* identifier)
{
	NPIdentifier id = g_browser->getstringidentifier(identifier);
	RemoveMethod(id);
}

void PluginInstance::RemoveMethod(NPIdentifier identifier)
{
	MethodMap::iterator it = methods.find(identifier);
	if (it != methods.end())
		methods.erase(it);
}

/**
 * Retrieve the global ecmascript object.
 *
 * @return Global object or 0.
 */
NPObject* PluginInstance::GetGlobalObject()
{
	CountUse of(this);

	if (!global_object && g_browser->getvalue(instance, NPNVWindowNPObject, &global_object) != NPERR_NO_ERROR)
		global_object = 0;

	return global_object;
}


/**
 * Invoke a handler set as a property on our object by the document, or
 * if no such property exists, attempt to invoke a handler set as property
 * on the global object.
 *
 * @param name Name of property whose function value to invoke. Note that
 * this identifier will need to be flagged as a known property in
 * CreateScriptableObject before a script can override it.
 * @param args Function arguments.
 * @param argc Argument count.
 * @param return_value Optional storage for a returned value. If set, the
 * returned value must be released by caller using NPN_ReleaseInvariantValue.
 *
 * @return true if property function existed and was invoked successfully.
 */
bool PluginInstance::InvokeHandler(const char* name, const NPVariant* args, uint32_t argc, NPVariant* return_value)
{
	if (is_destroying)
		return false;

	NPIdentifier nameid = GetIdentifier(name);
	PropertyMap::iterator it = properties.find(nameid);
	if (it != properties.end() && NPVARIANT_IS_OBJECT(it->second))
	{
		/* Invoke scriptable object property. */
		NPVariant returned_value;
		if (!g_browser->invokeDefault(instance, it->second.value.objectValue, args, argc, &returned_value))
			return false;

		if (return_value)
			*return_value = returned_value;
		else
			g_browser->releasevariantvalue(&returned_value);

		return true;
	}
	else if (NPObject* global = GetGlobalObject())
	{
		if (!g_browser->hasmethod(instance, global, nameid))
			return false;

		/* Invoke global method. */
		NPVariant returned_value;
		bool ret = g_browser->invoke(instance, global, nameid, args, argc, &returned_value);

		if (ret)
		{
			if (return_value)
				*return_value = returned_value;
			else
				g_browser->releasevariantvalue(&returned_value);
		}

		return ret;
	}

	return false;
}


/**
 * Create an ecmascript Array object.
 *
 * @return NPObject or 0 on failure.
 */
NPObject* PluginInstance::CreateArray()
{
	NPObject* global = GetGlobalObject();
	if (!global)
		return 0;

	/* Create array object.  */
	NPString arrstr;
	arrstr.UTF8Characters = "(function(){ return new Array(); })()";
	arrstr.UTF8Length = static_cast<uint32_t>(strlen(arrstr.UTF8Characters));

	NPVariant arrobj;
	if (!g_browser->evaluate(instance, global, &arrstr, &arrobj))
		return 0;

	if (!NPVARIANT_IS_OBJECT(arrobj))
	{
		g_browser->releasevariantvalue(&arrobj);
		return 0;
	}

	return arrobj.value.objectValue;
}


/**
 * Create an ecmascript object using NPN_Evaluate.
 *
 * @param script The script fragment to evaluate.
 *
 * @return Created object or 0 on failure.
 */
NPObject* PluginInstance::CreateObject(const char* script)
{
	NPObject* global = GetGlobalObject();
	if (!global)
		return 0;

	NPString fragment;
	fragment.UTF8Characters = script;
	fragment.UTF8Length = script ? static_cast<uint32_t>(strlen(script)) : 0;

	NPVariant object;
	if (!g_browser->evaluate(instance, global,  &fragment, &object))
		return 0;

	if (!NPVARIANT_IS_OBJECT(object))
	{
		g_browser->releasevariantvalue(&object);
		return 0;
	}

	NPObject* ret = object.value.objectValue;

	g_browser->retainobject(ret);
	g_browser->releasevariantvalue(&object);

	return ret;
}


/**
 * Create ecmascript stream object from NPStream*. Will replace the pdata
 * pointer in the NPStream object, and register an instance wide mapping.
 *
 * @param stream Source NPStream object.
 *
 * @return NPObject or 0 on failure.
 */
NPObject* PluginInstance::CreateStreamObject(NPStream* stream)
{
	/*
	 * Use NPN_Evaluate to avoid having to handle accessors manually.
	 */
	char fragment[512]; /* ARRAY OK 2010-10-08 terjes */
	SNPRINTF(fragment, sizeof(fragment), "(function() { return { end: %u, lastmodified: %u }; })()",
		static_cast<unsigned int>(stream->end), static_cast<unsigned int>(stream->lastmodified));

	NPObject* stream_object = CreateObject(fragment);
	if (!stream_object)
		return 0;

	/* Supply it with url headers. Done separately as direct evaluation requires escaping. */
	g_browser->setproperty(instance, stream_object, GetIdentifier("url"), &StringVariant(stream->url));
	g_browser->setproperty(instance, stream_object, GetIdentifier("headers"), &StringVariant(stream->headers));

	/* Supply it with the notification function if set. */
	if (stream->notifyData)
	{
		NPVariant notify_object = *ObjectVariant(stream->notifyData);
		g_browser->setproperty(instance, stream_object, GetIdentifier("notifyData"), &notify_object);
	}

	/*
	 * Store stream object.
	 */
	stream->pdata = stream_object;
	streams[stream_object] = stream;

	return stream_object;
}


/**
 * Create ecmascript stream object from NPWindow*.
 *
 * @return NPObject or 0 on failure.
 */
NPObject* PluginInstance::CreateWindowObject()
{
	if (!plugin_window)
		return 0;

	char fragment[1024]; /* ARRAY OK 2010-10-08 terjes */
	SNPRINTF(fragment, sizeof(fragment), "(function() { return { window: %lu, x: %d, y: %d, width: %u, height: %u, "\
		"clipRect: { top: %d, left: %d, bottom: %d, right: %d } }; })()",
		reinterpret_cast<unsigned long>(plugin_window->window),
		static_cast<int>(plugin_window->x), static_cast<int>(plugin_window->y),
		static_cast<unsigned int>(plugin_window->width), static_cast<unsigned int>(plugin_window->height),
		plugin_window->clipRect.top, plugin_window->clipRect.left,
		plugin_window->clipRect.bottom, plugin_window->clipRect.right);

	return CreateObject(fragment);
}


/**
 * Create a char* string from an NPString.
 *
 * @param string NPString to convert.
 *
 * @return Character array to be freed by NPN_MemFree by caller.
 */
char* PluginInstance::GetString(const NPString& string)
{
	if (char* s = static_cast<char*>(g_browser->memalloc(string.UTF8Length + 1)))
	{
		memcpy(s, string.UTF8Characters, string.UTF8Length);
		s[string.UTF8Length] = 0;
		return s;
	}

	return 0;
}


/**
 * Create or retrieve NPIdentifier.
 *
 * Will truncate strings that are not pure ascii.
 *
 * @param string NPString object that requires alternate representation.
 *
 * @return Identifier.
 */
NPIdentifier PluginInstance::GetIdentifier(const NPString& string)
{
	char* buffer = static_cast<char*>(alloca(string.UTF8Length + 1));
	memcpy(buffer, string.UTF8Characters, string.UTF8Length);
	buffer[string.UTF8Length] = 0;

	return g_browser->getstringidentifier(buffer);
}

NPIdentifier PluginInstance::GetIdentifier(const char* string)
{
	return g_browser->getstringidentifier(string);
}

NPIdentifier PluginInstance::GetIdentifier(int32_t value)
{
	return g_browser->getintidentifier(value);
}


/**
 * Retrieve an integer property value from an ecmascript object.
 *
 * @param object Object to retrieve value from.
 * @param property Name of property containing value.
 * @param default_value Value returned on error.
 *
 * @return Integer read from property or default_value.
 */
int32_t PluginInstance::GetIntProperty(NPObject* object, NPIdentifier name, int32_t default_value)
{
	int32_t ret = default_value;

	NPVariant property;
	if (!g_browser->getproperty(instance, object, name, &property))
		return ret;

	if (NPVARIANT_IS_NUMBER(property))
		ret = NPVARIANT_GET_NUMBER(property);

	g_browser->releasevariantvalue(&property);
	return ret;
}


/**
 * Create and populate DOM node object representing the plugin.
 *
 * Called by GetValue when browser asks for an NPObject.
 */
NPObject* PluginInstance::CreateScriptableObject()
{
	/* Cache value in case the browser asks more than once. */
	if (scriptable_object)
		return scriptable_object;

	/* Create node. */
	if ((scriptable_object = g_browser->createobject(instance, build_object_class())))
	{
		g_object_instance_map[scriptable_object] = this;

		AddMethod("toString", &PluginInstance::toString);
		AddMethod("valueOf", &PluginInstance::toString);
		AddMethod("getWindow", &PluginInstance::getWindow);
		AddMethod("injectMouseEvent", &PluginInstance::injectMouseEvent);
		AddMethod("injectKeyEvent", &PluginInstance::injectKeyEvent);
		AddMethod("crash", &PluginInstance::crash);
		AddMethod("paint", &PluginInstance::paint);
		AddMethod("getCookieCount", &PluginInstance::getCookieCount);
		AddMethod("setCookie", &PluginInstance::setCookie);

		/* Add properties for counters. */
		SetProperty(scriptable_object, GetIdentifier("paintCount"), &IntVariant(paint_count));
		SetProperty(scriptable_object, GetIdentifier("setWindowCount"), &IntVariant(setwindow_count));
		SetProperty(scriptable_object, GetIdentifier("windowPosChangedCount"), &IntVariant(windowposchanged_count));

		/* Add NPRuntime function wrappers. */
		AddMethod("testInvoke", &PluginInstance::testInvoke);
		AddMethod("testInvokeDefault", &PluginInstance::testInvokeDefault);
		AddMethod("testEvaluate", &PluginInstance::testEvaluate);
		AddMethod("testGetProperty", &PluginInstance::testGetProperty);
		AddMethod("testSetProperty", &PluginInstance::testSetProperty);
		AddMethod("testRemoveProperty", &PluginInstance::testRemoveProperty);
		AddMethod("testHasProperty", &PluginInstance::testHasProperty);
		AddMethod("testHasMethod", &PluginInstance::testHasMethod);
		AddMethod("testEnumerate", &PluginInstance::testEnumerate);
		AddMethod("testConstruct", &PluginInstance::testConstruct);
		AddMethod("testSetException", &PluginInstance::testSetException);

		AddMethod("testCreateObject", &PluginInstance::testCreateObject);
		AddMethod("testRetainObject", &PluginInstance::testRetainObject);
		AddMethod("testReleaseObject", &PluginInstance::testReleaseObject);

		/* And helpers. */
		AddMethod("getReferenceCount", &PluginInstance::getReferenceCount);
		AddMethod("setReferenceCount", &PluginInstance::setReferenceCount);

		/* Add NPAPI test methods. */
		AddMethod("testGetURLNotify", &PluginInstance::testGetURLNotify);
		AddMethod("testGetURL", &PluginInstance::testGetURL);
		AddMethod("testPostURLNotify", &PluginInstance::testPostURLNotify);
		AddMethod("testPostURL", &PluginInstance::testPostURL);
		AddMethod("testRequestRead", &PluginInstance::testRequestRead);
		AddMethod("testNewStream", &PluginInstance::testNewStream);
		AddMethod("testWrite", &PluginInstance::testWrite);
		AddMethod("testDestroyStream", &PluginInstance::testDestroyStream);
		AddMethod("testStatus", &PluginInstance::testStatus);
		AddMethod("testUserAgent", &PluginInstance::testUserAgent);
		AddMethod("testMemFlush", &PluginInstance::testMemFlush);
		AddMethod("testReloadPlugins", &PluginInstance::testReloadPlugins);
		AddMethod("testGetJavaEnv", &PluginInstance::testGetJavaEnv);
		AddMethod("testGetJavaPeer", &PluginInstance::testGetJavaPeer);
		AddMethod("testGetValue", &PluginInstance::testGetValue);
		AddMethod("testSetValue", &PluginInstance::testSetValue);
		AddMethod("testInvalidateRect", &PluginInstance::testInvalidateRect);
		AddMethod("testInvalidateRegion", &PluginInstance::testInvalidateRegion);
		AddMethod("testForceRedraw", &PluginInstance::testForceRedraw);
		AddMethod("testPushPopupsEnabledState", &PluginInstance::testPushPopupsEnabledState);
		AddMethod("testPopPopupsEnabledState", &PluginInstance::testPopPopupsEnabledState);
		AddMethod("testPluginThreadAsyncCall", &PluginInstance::testPluginThreadAsyncCall);
		AddMethod("testGetValueForURL", &PluginInstance::testGetValueForURL);
		AddMethod("testSetValueForURL", &PluginInstance::testSetValueForURL);
		AddMethod("testGetAuthenticationInfo", &PluginInstance::testGetAuthenticationInfo);
		AddMethod("testScheduleTimer", &PluginInstance::testScheduleTimer);
		AddMethod("testUnscheduleTimer", &PluginInstance::testUnscheduleTimer);
		AddMethod("testPopupContextMenu", &PluginInstance::testPopupContextMenu);
		AddMethod("testConvertPoint", &PluginInstance::testConvertPoint);

		/* Add platform property for selftest convenience. */
#ifdef XP_WIN
		SetProperty(scriptable_object, GetIdentifier("platform"), &StringVariant("XP_WIN"));
#elif XP_UNIX
		SetProperty(scriptable_object, GetIdentifier("platform"), &StringVariant("XP_UNIX"));
#elif XP_MACOSX
		SetProperty(scriptable_object, GetIdentifier("platform"), &StringVariant("XP_MAC"));
#elif XP_GOGI
		SetProperty(scriptable_object, GetIdentifier("platform"), &StringVariant("XP_GOGI"));
#endif // XP_GOGI

		/*
		 * Mark script-settable properties as such. Browsers will not allow a property
		 * to be set if we do not acknowledge that we carry said property.
		 */
		SetProperty(scriptable_object, GetIdentifier("onAsyncCall"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onConstruct"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onDestroyStream"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onGetProperty"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onGetValue"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onHasMethod"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onHasProperty"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onInvoke"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onInvokeDefault"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onKeyEvent"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onMouseEvent"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onNewStream"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onPaint"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onPrint"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onRemoveProperty"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onSetProperty"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onSetValue"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onSetWindow"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onStreamAsFile"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onURLNotify"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onWrite"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onWriteReady"), &VoidVariant());
		SetProperty(scriptable_object, GetIdentifier("onNew"), &VoidVariant());
	}

	return scriptable_object;
}


/**
 * Invalidate an object.
 *
 * @param object Object to invalidate.
 */
void PluginInstance::Invalidate(NPObject* /*object*/)
{
}


/**
 * Called by browser when deallocating an object we've created.
 *
 * @param object Object that is to be deallocated.
 */
void PluginInstance::Deallocate(NPObject* object)
{
	CountUse of(this);

	ObjectInstanceMap::iterator it = g_object_instance_map.find(object);
	if (it != g_object_instance_map.end())
	{
		g_object_instance_map.erase(it);

		if (object == scriptable_object)
		{
			/* Release property objects. */
			for (PropertyMap::iterator prop_it = properties.begin(); prop_it != properties.end(); prop_it = properties.begin())
				RemoveProperty(object, prop_it->first);

			this->scriptable_object = 0;
		}
	}

	/* As of NPP_Destroy, the browser has likely deleted objects without asking. */
	if (!is_destroying)
		g_browser->memfree(object);
}


/**
 * Check whether an object carries a given method.
 *
 * @param object Object to check.
 * @param name Name of method.
 *
 * @return true if method exists.
 */
bool PluginInstance::HasMethod(NPObject* object, NPIdentifier name)
{
	CountUse of(this);
	bool has_method = false;

	if (g_browser->identifierisstring(name) && !strcmp(g_browser->utf8fromidentifier(name), "toString"))
		has_method = true;
	else if (object == scriptable_object)
		has_method = methods.find(name) != methods.end();

	/* Invoke document listener. */
	NPVariant listener_args[2] = { *ObjectVariant(object), *AutoVariant(name) };
	NPVariant returned_value;
	if (InvokeHandler("onHasMethod", listener_args, 2, &returned_value))
	{
		if (NPVARIANT_IS_BOOLEAN(returned_value))
			has_method = !!returned_value.value.boolValue;

		g_browser->releasevariantvalue(&returned_value);
	}

	return has_method;
}


/**
 * Invoke a method. If the document has defined a listener and this
 * listener does not return undefined, no native method will be invoked.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on successful invoke.
 */
bool PluginInstance::Invoke(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	CountUse of(this);

	/* Invoke document listener. */
	NPVariant* listener_args = static_cast<NPVariant*>(alloca(sizeof(NPVariant) * (argc + 2)));
	listener_args[0] = *ObjectVariant(object);
	listener_args[1] = *AutoVariant(name);
	memcpy(listener_args + 2, args, sizeof(*listener_args) * argc);

	NPVariant returned_value;
	if (InvokeHandler("onInvoke", listener_args, argc + 2, &returned_value))
	{
		if (!NPVARIANT_IS_VOID(returned_value))
		{
			*result = returned_value;
			return true;
		}

		g_browser->releasevariantvalue(&returned_value);
	}

	/* Handle toString for all objects created with NPN_CreateObject. */
	if (g_browser->identifierisstring(name) && !strcmp(g_browser->utf8fromidentifier(name), "toString"))
		return toString(object, name, args, argc, result);

	/* Handle general methods. */
	if (object == this->scriptable_object)
	{
		/* Find and invoke handler. */
		MethodMap::iterator it = methods.find(name);
		if (it == methods.end())
			return false;

		return (*this.*it->second)(object, name, args, argc, result);
	}

	return false;
}


/**
 * Invoke default method.
 *
 * @param object Object whose default method we are to invoke.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on successful invoke.
 */
bool PluginInstance::InvokeDefault(NPObject* object, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	CountUse of(this);

	/* Invoke document listener. */
	NPVariant* listener_args = static_cast<NPVariant*>(alloca(sizeof(NPVariant) * (argc + 1)));
	listener_args[0] = *ObjectVariant(object);
	memcpy(listener_args + 1, args, sizeof(*listener_args) * argc);

	NPVariant return_value;
	if (InvokeHandler("onInvokeDefault", listener_args, argc + 1, &return_value))
	{
		*result = return_value;
		return true;
	}

	/* We have no objects with default methods. */
	result->type = NPVariantType_Void;
	return false;
}


/**
 * Check if an object carries a given property.
 *
 * @param object Object to query.
 * @param name Name of property.
 *
 * @return true if property exists.
 */
bool PluginInstance::HasProperty(NPObject* object, NPIdentifier name)
{
	CountUse of(this);
	bool has_property = false;

	if (object == this->scriptable_object)
		has_property = properties.find(name) != properties.end();

	/* Invoke document listener. */
	NPVariant listener_args[2] = { *ObjectVariant(object), *AutoVariant(name) };
	NPVariant returned_value;
	if (InvokeHandler("onHasProperty", listener_args, 2, &returned_value))
	{
		if (NPVARIANT_IS_BOOLEAN(returned_value))
			has_property = !!returned_value.value.boolValue;

		g_browser->releasevariantvalue(&returned_value);
	}

	return has_property;
}


/**
 * Retrieve a property from an object.
 *
 * @param object Object to query.
 * @param name Name of property.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::GetProperty(NPObject* object, NPIdentifier name, NPVariant* result)
{
	CountUse of(this);
	result->type = NPVariantType_Void;

	/* Invoke document listener. */
	NPVariant listener_args[2] = { *ObjectVariant(object), *AutoVariant(name) };
	NPVariant return_value;
	if (InvokeHandler("onGetProperty", listener_args, 2, &return_value))
	{
		*result = return_value;
		return true;
	}

	if (object == this->scriptable_object)
	{
		/* Look up property. */
		PropertyMap::iterator it = properties.find(name);
		if (it != properties.end())
		{
			*result = it->second;
			return true;
		}
	}

	return false;
}


/**
 * Set a property on an object.
 *
 * @param object Object to target.
 * @param name Name of property.
 * @param value Value of property.
 *
 * @return true on success.
 */
bool PluginInstance::SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value)
{
	CountUse of(this);

	/* Invoke document listener. */
	NPVariant listener_args[3] = { *ObjectVariant(object), *AutoVariant(name), *value };
	bool ret = InvokeHandler("onSetProperty", listener_args, 3);

	if (object == this->scriptable_object)
	{
		/* Decrement refcount if there is an existing value and it is an object. */
		PropertyMap::iterator it = properties.find(name);
		if (it != properties.end() && NPVARIANT_IS_OBJECT(it->second))
			g_browser->releaseobject(it->second.value.objectValue);

		/* Increment refcount of objects that are being set as a value of the property. */
		if (NPVARIANT_IS_OBJECT(*value))
			g_browser->retainobject(value->value.objectValue);

		properties[name] = *value;

		return true;
	}

	return ret;
}


/**
 * Remove a property from an object.
 *
 * @param object Object to target.
 * @param name Name of property.
 *
 * @return true on success.
 */
bool PluginInstance::RemoveProperty(NPObject* object, NPIdentifier name)
{
	CountUse of(this);

	/* Invoke document listener. */
	NPVariant listener_args[2] = { *ObjectVariant(object), *AutoVariant(name) };
	bool ret = InvokeHandler("onRemoveProperty", listener_args, 2);

	if (object == this->scriptable_object)
	{
		PropertyMap::iterator it = properties.find(name);
		if (it != properties.end())
		{
			NPVariant variant = it->second;
			properties.erase(it);

			/* Decrement refcount if the value is an object. */
			if (NPVARIANT_IS_OBJECT(variant))
				g_browser->releaseobject(variant.value.objectValue);

			return true;
		}
	}

	return ret;
}


/**
 * Get the names of properties and methods of the specified NPObject.
 *
 * @param object Object to target.
 * @param identifiers Identifiers of properties and methods. The memory pointed
 * to on return has been allocated using NPN_MemAlloc and will be freed by the
 * caller using NPN_MemFree.
 * @param count Number of identifiers returned.
 *
 * @return true on success.
 */
bool PluginInstance::Enumerate(NPObject* object, NPIdentifier** identifiers, uint32_t* count)
{
	CountUse of(this);

	if (object != this->scriptable_object)
		return false;

	*count = static_cast<uint32_t>(methods.size()) + static_cast<uint32_t>(properties.size());
	*identifiers = static_cast<NPIdentifier*>(g_browser->memalloc(sizeof(NPIdentifier) * (*count)));
	if (!*identifiers)
		return false;

	int i = 0;

	for (MethodMap::iterator it = methods.begin(); it != methods.end(); it++)
		(*identifiers)[i++] = it->first;

	for (PropertyMap::iterator it = properties.begin(); it != properties.end(); it++)
		(*identifiers)[i++] = it->first;

	return true;
}


/**
 * Invoke the new method on an object.
 *
 * @param object Object to target.
 * @param args Constructor arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::Construct(NPObject* /*object*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	CountUse of(this);

	/* Invoke listener. */
	InvokeHandler("onConstruct", args, argc);

	/* We maintain no objects with constructors. */
	return false;
}


/**
 * Window create, move, resize or destroy notification.
 *
 * @See https://developer.mozilla.org/en/NPP_SetWindow.
 */
NPError PluginInstance::SetWindow(NPWindow* window)
{
	CountUse of(this);
	plugin_window = window;

#ifdef XP_MACOSX
	if (drawing_model == NPDrawingModelCoreAnimation)
		UpdateCoreAnimationLayer();
#endif // XP_MACOSX

	/* Invoke document listener. */
	if (NPObject* window_object = CreateWindowObject())
	{
		SetProperty(scriptable_object, GetIdentifier("setWindowCount"), &IntVariant(++setwindow_count));
		InvokeHandler("onSetWindow", &ObjectVariant(window_object), 1);
		g_browser->releaseobject(window_object);
	}

	return NPERR_NO_ERROR;
}


/**
 * New data stream notification.
 *
 * Creates an ecmascript object based on stream argument and stores it in stream->pdata.
 *
 * @See https://developer.mozilla.org/en/NPP_NewStream.
 */
NPError PluginInstance::NewStream(NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
	CountUse of(this);

	/* Create stream object to give document listeners. */
	NPObject* stream_object = CreateStreamObject(stream);
	if (!stream_object)
		return NPERR_OUT_OF_MEMORY_ERROR;

	/* Invoke document listener. */
	NPVariant args[4] = { *StringVariant(type), *ObjectVariant(stream_object), *BoolVariant(!!seekable), *IntVariant(*stype) };
	NPVariant return_value;

	if (InvokeHandler("onNewStream", args, 4, &return_value))
	{
		*stype = NPVARIANT_IS_NUMBER(return_value) ? NPVARIANT_GET_NUMBER(return_value) : NP_NORMAL;
		g_browser->releasevariantvalue(&return_value);
	}
	else
		*stype = NP_NORMAL;

	return NPERR_NO_ERROR;
}


/**
 * Data stream close or destroy notification.
 *
 * @See https://developer.mozilla.org/en/NPP_DestroyStream.
 */
NPError PluginInstance::DestroyStream(NPStream* stream, NPReason reason)
{
	CountUse of(this);
	int ret = NPERR_NO_ERROR;

	/* Invoke document listener. */
	NPVariant args[2] = { *ObjectVariant(stream->pdata), *IntVariant(reason) };
	NPVariant return_value;
	if (InvokeHandler("onDestroyStream", args, 2, &return_value))
	{
		if (NPVARIANT_IS_NUMBER(return_value))
			ret = NPVARIANT_GET_NUMBER(return_value);

		g_browser->releasevariantvalue(&return_value);
	}

	/* Delete mapping. */
	if (stream->pdata)
	{
		StreamMap::iterator it = streams.find(static_cast<NPObject*>(stream->pdata));
		if (it != streams.end())
			streams.erase(it);

		/* Release stream object. */
		g_browser->releaseobject(args[0].value.objectValue);
	}

	return ret;
}


/**
 * Provide a local file name for the data from a stream.
 *
 * @See https://developer.mozilla.org/en/NPP_StreamAsFile.
 */
void PluginInstance::StreamAsFile(NPStream* stream, const char* fname)
{
	CountUse of(this);

	/* Invoke document listener. */
	NPVariant args[2] = { *ObjectVariant(stream->pdata), *StringVariant(fname) };
	InvokeHandler("onStreamAsFile", args, 2);
}


/**
 * Retrieve maximum number of bytes plugin is ready to consume.
 *
 * @See https://developer.mozilla.org/en/NPP_WriteReady.
 */
int32_t PluginInstance::WriteReady(NPStream* stream)
{
	CountUse of(this);

	/* Default return value. */
	int32_t ret = 4096;

	/* Invoke document listener. */
	NPVariant return_value;
	if (InvokeHandler("onWriteReady", &ObjectVariant(stream->pdata), 1, &return_value))
	{
		if (NPVARIANT_IS_NUMBER(return_value))
			ret = NPVARIANT_GET_NUMBER(return_value);
		g_browser->releasevariantvalue(&return_value);
	}

	return ret;
}


/**
 * Deliver stream data.
 *
 * @See https://developer.mozilla.org/en/NPP_Write.
 */
int32_t PluginInstance::Write(NPStream* stream, int32_t offset, int32_t len, void* buf)
{
	CountUse of(this);

	/* Default return value is perfect consumption. */
	int32_t ret = len;

	/* Invoke dokument listener. */
	NPVariant args[3] = { *ObjectVariant(stream->pdata), *IntVariant(offset), *ObjectVariant(CreateArray()) };
	if (!args[2].value.objectValue)
		return -1;

	for (int i = 0; i < len; i++)
	{
		NPVariant value = *IntVariant(static_cast<char*>(buf)[i]);
		if (!g_browser->setproperty(instance, args[2].value.objectValue, GetIdentifier(i), &value))
		{
			g_browser->releaseobject(args[2].value.objectValue);
			return -1;
		}
	}

	NPVariant return_value;
	if (InvokeHandler("onWrite", args, 3, &return_value))
	{
		if (NPVARIANT_IS_NUMBER(return_value))
			ret = NPVARIANT_GET_NUMBER(return_value);
		g_browser->releasevariantvalue(&return_value);
	}

	g_browser->releaseobject(args[2].value.objectValue);

	return ret;
}


/**
 * Request platform specific print operation.
 *
 * @See https://developer.mozilla.org/en/NPP_Print.
 */
void PluginInstance::Print(NPPrint* /*print_info*/)
{
	CountUse of(this);

	/* Should pass on arguments, but they are terribly platform dependent, so I'll postpone. */
	InvokeHandler("onPrint");
}


/**
 * URL request completion notification.
 *
 * The data argument is set by testGetURLNotify and is either 0 or an NPObject pointer.
 *
 * @See https://developer.mozilla.org/en/NPP_URLNotify.
 */
void PluginInstance::URLNotify(const char* url, NPReason reason, void* data)
{
	CountUse of(this);

	/* Invoke listener. */
	NPVariant args[3] = { *StringVariant(url), *IntVariant(reason), *ObjectVariant(data) };
	InvokeHandler("onURLNotify", args, data ? 3 : 2);

	/* Release the notification data now that the get operation is complete. */
	if (data)
		g_browser->releaseobject(static_cast<NPObject*>(data));
}


/**
 * Query plugin information.
 *
 * @See https://developer.mozilla.org/en/NPP_GetValue.
 */
NPError PluginInstance::GetValue(NPPVariable variable, void* value)
{
	CountUse of(this);
	NPError ret = NPERR_GENERIC_ERROR;

	/* Handle request. */
	switch (variable)
	{
		case NPPVpluginScriptableNPObject:
			if ((*static_cast<NPObject**>(value) = CreateScriptableObject()))
			{
				g_browser->retainobject(*static_cast<NPObject**>(value));
				ret = NPERR_NO_ERROR;
			}
			break;
		case NPPVpluginWantsAllNetworkStreams:
			*static_cast<NPBool*>(value) = false;
			ret = NPERR_NO_ERROR;
			break;
		case NPPVsupportsAdvancedKeyHandling:
			*static_cast<NPBool*>(value) = true;
			ret = NPERR_NO_ERROR;
			break;
		case NPPVpluginWindowBool:
			*static_cast<NPBool*>(value) = !IsWindowless();
			ret = NPERR_NO_ERROR;
			break;
		case NPPVpluginNeedsXEmbed:
			*static_cast<NPBool*>(value) = true;
			ret = NPERR_NO_ERROR;
			break;
#ifdef XP_MACOSX
		case NPPVpluginDrawingModel:
			*static_cast<NPDrawingModel*>(value) = drawing_model;
			ret = NPERR_NO_ERROR;
			break;
		case NPPVpluginEventModel:
			*static_cast<NPEventModel*>(value) = event_model;
			ret = NPERR_NO_ERROR;
			break;
		case NPPVpluginCoreAnimationLayer:
			SetCoreAnimationLayer(value);
			ret = NPERR_NO_ERROR;
			break;
#endif // XP_MACOSX
		case NPPVpluginNameString:
		case NPPVpluginDescriptionString:
		case NPPVpluginTransparentBool:
		case NPPVjavaClass:
		case NPPVpluginWindowSize:
		case NPPVpluginTimerInterval:
		case NPPVpluginScriptableInstance:
		case NPPVpluginScriptableIID:
		case NPPVjavascriptPushCallerBool:
		case NPPVpluginKeepLibraryInMemory:
		case NPPVformValue:
		case NPPVpluginUrlRequestsDisplayedBool:
		case NPPVpluginNativeAccessibleAtkPlugId:
		case NPPVpluginCancelSrcStream:
		case NPPVpluginUsesDOMForCursorBool:
			break;
	};

	/* Invoke listener. */
	InvokeHandler("onGetValue", &IntVariant(variable), 1);

	return ret;
}


/**
 * Set browser information.
 *
 * @See https://developer.mozilla.org/en/NPP_SetValue.
 */
NPError PluginInstance::SetValue(NPNVariable variable, void* value)
{
	CountUse of(this);

	/* Invoke listener. */
	NPVariant args[2] = { *IntVariant(variable), *IntVariant(*reinterpret_cast<int*>(value)) };
	InvokeHandler("onSetValue", args, 2);

	return NPERR_GENERIC_ERROR;
}


/**
 * Set focus.
 *
 * @See https://wiki.mozilla.org/NPAPI:AdvancedKeyHandling.
 */
NPBool PluginInstance::GotFocus(NPFocusDirection /*direction*/)
{
	CountUse of(this);
	assert(!"NPP_GotFocus not implemented!");

	return false;
}


/**
 * Loose focus.
 *
 * @See https://wiki.mozilla.org/NPAPI:AdvancedKeyHandling.
 */
void PluginInstance::LostFocus()
{
	CountUse of(this);
	assert(!"NPP_LostFocus not implemented!");
}


/**
 * Redirect notification.
 *
 * @See https://wiki.mozilla.org/NPAPI:HTTPRedirectHandling.
 */
void PluginInstance::URLRedirectNotify(const char* /*url*/, int32_t /*status*/, void* /*notifyData*/)
{
	CountUse of(this);
	assert(!"NPP_URLRedirectNotify not implemented!");
}


/**
 * Check validity of browser-supplied arguments. Called by NPP_New.
 *
 * Send arguments to document's onNew, if it exists, and return the
 * numeric return value, if any.
 *
 * @parameter argc Argument count.
 * @parameter argn Argument names.
 * @parameter argv Argument values.
 *
 * @return NPERR_NO_ERROR if arguments are accepted.
 */
NPError PluginInstance::CheckArguments(int16_t argc, char** argn, char** argv)
{
	CountUse of(this);

	/*
	 * Create ecmascript argument name -> value map.
	 */
	AutoReleaseObject arguments = CreateArray();
	if (!*arguments)
		return NPERR_GENERIC_ERROR;

	for (int i = 0; i < argc; i++)
	{
		/* Create { name: "argn", value: "argv" } object. */
		AutoReleaseObject argument = CreateObject("(function() { return {}; })()");
		if (!*argument)
			return NPERR_GENERIC_ERROR;

		if (!g_browser->setproperty(instance, *argument, GetIdentifier("name"), &StringVariant(argn[i])))
			return NPERR_GENERIC_ERROR;

		if (!g_browser->setproperty(instance, *argument, GetIdentifier("value"), &StringVariant(argv[i])))
			return NPERR_GENERIC_ERROR;

		/* Assign it to element 'i' in argument array. */
		if (!g_browser->setproperty(instance, *arguments, GetIdentifier(i), &ObjectVariant(*argument)))
			return NPERR_GENERIC_ERROR;
	}

	/*
	 * Query document as to how to react to the arguments.
	 */
	int32_t ret = NPERR_NO_ERROR;
	NPVariant returned_value;

	if (InvokeHandler("onNew", &ObjectVariant(*arguments), 1, &returned_value))
	{
		if (NPVARIANT_IS_NUMBER(returned_value)) // Return value from onNew() selftest script function, depending on arguments validation.
			ret = NPVARIANT_GET_NUMBER(returned_value);

		g_browser->releasevariantvalue(&returned_value);
	}

	return ret;
}

/**
 * Request asynchronous call.
 */
void PluginInstance::RequestAsyncCall(AsyncCallRecord::Reason reason)
{
	if (AsyncCallRecord* acr = static_cast<AsyncCallRecord*>(g_browser->memalloc(sizeof(AsyncCallRecord))))
	{
		acr->reason = reason;
		acr->instance = this;
		g_browser->pluginthreadasynccall(instance, async_call, acr);
	}
}


/**
 * Called by browser as a response to RequestAsyncCall.
 */
void PluginInstance::AsyncCall(AsyncCallRecord::Reason reason)
{
	if (reason == AsyncCallRecord::DocumentRequest)
		InvokeHandler("onAsyncCall");
}


/**
 * Called by browser as a response to NPN_ScheduleTimer.
 *
 * @param id Timer ID as returned by NPN_ScheduleTimer.
 */
void PluginInstance::Timeout(uint32_t id)
{
	CountUse of(this);

	TimerMap::iterator it = timers.find(id);
	if (it != timers.end())
	{
		NPVariant returned_value;
		if (g_browser->invokeDefault(instance, it->second, 0, 0, &returned_value))
			g_browser->releasevariantvalue(&returned_value);
	}
}


/**
 * Ecmascript native method.
 *
 * Returns the most natural string representation of an object.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::toString(NPObject* object, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	const char* id = object == scriptable_object ? "[OperaTestPlugin]" : "[OperaTestPluginObject]";
	const size_t len = strlen(id);

	/* The browser will attempt to release the returned string using NPN_MemFree. */
	char* ret = static_cast<char*>(g_browser->memalloc(static_cast<uint32_t>(len) + 1));
	memcpy(ret, id, len);
	ret[len] = 0;

	*result = *StringVariant(ret);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Takes no arguments. Returns the window structure most recently acquired by SetWindow.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::getWindow(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	if (NPObject* window_object = CreateWindowObject())
	{
		*result = *ObjectVariant(window_object);
		return true;
	}

	return false;
}


/**
 * Ecmascript native method. Injects a mouse event into the graphical shell in which
 * the browser is running, intended to ensure events are routed to the plugin.
 *
 * Takes four arguments, x, y, button and type, where the coordinates are given
 * relative to the plugin window, button is a numeric id, and type is 0 for mousedown,
 * 1 for mousemove and 2 for mouseup.
 *
 * @param object Object that received call.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
*/
bool PluginInstance::injectMouseEvent(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4 || !NPVARIANT_IS_NUMBER(args[0]) || !NPVARIANT_IS_NUMBER(args[1]) ||
			!NPVARIANT_IS_NUMBER(args[2]) || !NPVARIANT_IS_NUMBER(args[3]))
		return false;

	bool ret = false;

#ifdef XP_WIN
	assert(!"Implement me.");
#endif // XP_WIN

#ifdef XP_UNIX
	if (plugin_window && plugin_window->window && plugin_window->ws_info)
	{
		Window root = 0;
		Window window = reinterpret_cast<Window>(plugin_window->window);
		Display* display = static_cast<NPSetWindowCallbackStruct*>(plugin_window->ws_info)->display;

		/* Obtain coordinates of plugin window relative to the window it inhabits. */
		int x = 0, y = 0;
		GetOriginRelativeToWindow(x, y);

		/* Obtain coordinates of plugin window relative to the root window. */
		int x_root = 0, y_root = 0;

		Window win = window;
		while (win)
		{
			/* Translate screen coordinates by offset of current window within parent. */
			XWindowAttributes attributes;
			if (XGetWindowAttributes(display, window, &attributes))
			{
				x_root += attributes.x;
				y_root += attributes.y;
			}

			/* Find parent. */
			unsigned int num_children;
			Window parent, *children;
			if (XQueryTree(display, window, &root, &parent, &children, &num_children))
				XFree(children);
			else
				parent = 0;

			win = parent;
		}

		/* Send mouse event. */
		XEvent event;
		int mask;
		if (NPVARIANT_GET_NUMBER(args[3]) < 2 /* mousedown, mouseup */)
		{
			if (NPVARIANT_GET_NUMBER(args[3]) == 0)
			{
				mask = ButtonPressMask;
				event.xbutton.type = ButtonPress;
			}
			else
			{
				mask = ButtonReleaseMask;
				event.xbutton.type = ButtonRelease;
			}

			event.xbutton.send_event = True;
			event.xbutton.display = display;
			event.xbutton.window = window;
			event.xbutton.root = root;
			event.xbutton.x = x;
			event.xbutton.y = y;
			event.xbutton.x_root = x_root;
			event.xbutton.y_root = y_root;
		}
		else /* mousemove */
		{
			mask = PointerMotionMask;
			event.xmotion.type = MotionNotify;
			event.xmotion.send_event = True;
			event.xbutton.display = display;
			event.xbutton.window = window;
			event.xbutton.root = root;
			event.xbutton.x = x;
			event.xbutton.y = y;
			event.xbutton.x_root = x_root;
			event.xbutton.y_root = y_root;
		}

		ret = !!XSendEvent(display, window, True, mask, &event);
		XFlush(display);
	}
#endif // XP_UNIX

	*result = *BoolVariant(ret);
	return ret;
}


/**
 * Ecmascript native method. Injects a key event into the graphical shell in which
 * the browser is running, intended to ensure events are routed to the plugin.
 *
 * Takes two arguments, a keycode and a type, where type is 0 for press and 1 for release.
 *
 * @param object Object that received call.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
*/
bool PluginInstance::injectKeyEvent(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 2 || !NPVARIANT_IS_NUMBER(args[0]) || !NPVARIANT_IS_NUMBER(args[1]))
		return false;

	bool ret = false;

#ifdef XP_WIN
	/* Deduce plugin window in screen coordinates using GetClientRect, GetWindowRect and window_object. */
	/* Call SendInput (see http://msdn.microsoft.com/en-us/library/ms646310(VS.85).asp). */
	assert(!"Implement me.");
#endif // XP_WIN

#ifdef XP_UNIX
	if (plugin_window && plugin_window->ws_info)
	{
		Window window = reinterpret_cast<Window>(plugin_window->window);
		Display* display = static_cast<NPSetWindowCallbackStruct*>(plugin_window->ws_info)->display;

		/* Send key event. */
		XEvent event;

		int mask = KeyPressMask;
		event.type = KeyPress;
		if (NPVARIANT_GET_NUMBER(args[1]))
			mask = KeyReleaseMask, event.type = KeyRelease;

		event.xkey.type = event.type;
		event.xkey.send_event = True;
		event.xkey.display = display;
		event.xkey.window = window;
		event.xkey.keycode = NPVARIANT_GET_NUMBER(args[0]);

		ret = !!XSendEvent(display, window, True, mask, &event);
		XFlush(display);
	}
#endif // XP_UNIX

	*result = *BoolVariant(ret);
	return ret;

}


/**
 * Ecmascript native method.
 *
 * Attempts to crash the plugin with a segmentation violation.
 * Returns false on failure.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::crash(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	g_crashed = true;
	*static_cast<volatile char*>(0) = 0;
	g_crashed = false;

	*result = *BoolVariant(false);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Repaint window. See WindowedInstance override.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::paint(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	return false;
}


/**
 * Ecmascript native method.
 *
 * Get count of in-memory cookies.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::getCookieCount(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	*result = *IntVariant(g_cookies.size());
	return true;
}


/**
 * Ecmascript native method.
 *
 * Set an in-memory cookie.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::setCookie(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 2 || !NPVARIANT_IS_STRING(args[0]) || !NPVARIANT_IS_STRING(args[1]))
		return false;

	bool success = false;
	char* domain = GetString(args[0].value.stringValue);
	if (domain)
	{
		if (*domain != '\0')
		{
			char* cookie = GetString(args[1].value.stringValue);
			if (cookie)
			{
				g_cookies[domain] = cookie;
				success = true;
			}
		}
		g_browser->memfree(domain);
	}
	return success;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_Invoke. The first argument is the object to invoke
 * the method on, the second argument is the method name, and the
 * remaining arguments are passed as arguments to the method.
 *
 * If either of the first two arguments are 'null', the corresponding
 * C call will send a null pointer in its place.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testInvoke(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 2 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0]))
		|| (!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1])))
		return false;

	NPObject* target = NULL;
	NPIdentifier method = NULL;

	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (NPVARIANT_IS_STRING(args[1]))
		method = GetIdentifier(args[1].value.stringValue);

	return g_browser->invoke(instance, target, method, args + 2, argc - 2, result);
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_InvokeDefault. The first argument is the object to
 * invoke the method on and the remaining arguments are passed as
 * arguments to the method.
 *
 * If the first argument is 'null', the corresponding C call will send
 * a null pointer in its place.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testInvokeDefault(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0])))
		return false;

	NPObject* target = NULL;
	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (argc >= 2 && NPVARIANT_IS_NULL(args[1]))
	{
		/* Set return value undefined and deny the browser its return value path. */
		*result = *VoidVariant();
		result = NULL;
	}

	return g_browser->invokeDefault(instance, target, args + 1, argc - 1, result);
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_Evaluate. Takes one argument, which is a string to evaluate.
 *
 * If the argument is 'null', the corresponding C call will send
 * a null pointer in its place. If an optional second argument is
 * 'null', the result parameter to the C call will be a null pointer.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testEvaluate(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc == 0)
		return false;

	if (argc > 1 && NPVARIANT_IS_NULL(args[1]))
	{
		/* Set return value undefined and deny the browser its return value path. */
		*result = *VoidVariant();
		result = NULL;
	}

	if (NPVARIANT_IS_STRING(args[0]))
	{
		NPString script;
		memcpy(&script, &args[0].value.stringValue, sizeof(NPString));
		return g_browser->evaluate(instance, GetGlobalObject(), &script, result);
	}
	else if (NPVARIANT_IS_NULL(args[0]))
		return g_browser->evaluate(instance, GetGlobalObject(), NULL, result);

	return false;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_GetProperty. The first argument is the object to retrieve the
 * property from, the second is a string naming its property.
 *
 * If either of the first two arguments are 'null', the corresponding
 * C call will send a null pointer in its place. An optional third
 * argument may be specified as 'null' to make the C call's 'result'
 * parameter be sent as a null pointer.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetProperty(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 2 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0]))
		|| (!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1])))
		return false;

	NPObject* target = NULL;
	NPIdentifier property = NULL;

	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (NPVARIANT_IS_STRING(args[1]))
		property = GetIdentifier(args[1].value.stringValue);

	if (argc >= 3 && NPVARIANT_IS_NULL(args[2]))
	{
		/* Set return value undefined and deny the browser its return value path. */
		*result = *VoidVariant();
		result = NULL;
	}

	return g_browser->getproperty(instance, target, property, result);
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_SetProperty. The first argument is the object to set a
 * property of, the second is the name of the property, and the third is
 * the value to set.
 *
 * If either of the first two arguments are 'null', the corresponding
 * C call will send a null pointer in its place.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testSetProperty(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 3 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0])) ||
		(!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1])))
		return false;

	NPObject* target = NULL;
	NPIdentifier property = NULL;

	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (NPVARIANT_IS_STRING(args[1]))
		property = GetIdentifier(args[1].value.stringValue);

	return g_browser->setproperty(instance, target, property, &args[2]);
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_RemoveProperty. The first argument is the object to remove
 * the property from, and the second is the name of the property to remove.
 *
 * If either of the first two arguments are 'null', the corresponding
 * C call will send a null pointer in its place.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testRemoveProperty(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 2 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0])) |
		(!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1])))
		return false;

	NPObject* target = NULL;
	NPIdentifier property = NULL;

	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (NPVARIANT_IS_STRING(args[1]))
		property = GetIdentifier(args[1].value.stringValue);

	return g_browser->removeproperty(instance, target, property);
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_HasProperty. The first argument is the object to query,
 * and the second is the name of the property.
 *
 * If either of the first two arguments are 'null', the corresponding
 * C call will send a null pointer in its place.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testHasProperty(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 2 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0])) |
		(!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1])))
		return false;

	NPObject* target = NULL;
	NPIdentifier property = NULL;

	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (NPVARIANT_IS_STRING(args[1]))
		property = GetIdentifier(args[1].value.stringValue);

	*result = *BoolVariant(g_browser->hasproperty(instance, target, property));
	return true;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_HasMethod. The first argument is the object to query,
 * and the second is the name of the method.
 *
 * If either of the first two arguments are 'null', the corresponding
 * C call will send a null pointer in its place.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testHasMethod(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 2 || (!NPVARIANT_IS_OBJECT(args[0]) && !NPVARIANT_IS_NULL(args[0])) |
		(!NPVARIANT_IS_STRING(args[1]) && !NPVARIANT_IS_NULL(args[1])))
		return false;

	NPObject* target = NULL;
	NPIdentifier method = NULL;

	if (NPVARIANT_IS_OBJECT(args[0]))
		target = args[0].value.objectValue;

	if (NPVARIANT_IS_STRING(args[1]))
		method = GetIdentifier(args[1].value.stringValue);

	*result = *BoolVariant(g_browser->hasmethod(instance, target, method));
	return true;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_Enumerate. The first argument is the target
 * object, and the return value is an array of identifiers.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testEnumerate(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	result->type = NPVariantType_Void;

	if (argc == 0 || !NPVARIANT_IS_OBJECT(args[0]))
		return false;

	/* Perform enumerate call. */
	NPIdentifier* identifiers;
	uint32_t identifier_count;

	if (!g_browser->enumerate(instance, args[0].value.objectValue, &identifiers, &identifier_count))
		return false;

	/* Create Array of identifier strings. */
	NPObject* array = CreateArray();
	if (!array)
	{
		g_browser->memfree(identifiers);
		return false;
	}

	for (unsigned int i = 0; i < identifier_count; i++)
	{
		NPVariant value = *AutoVariant(identifiers[i]);

		if (!g_browser->setproperty(instance, array, g_browser->getintidentifier(i), &value))
		{
			g_browser->memfree(identifiers);
			g_browser->releaseobject(array);
			return false;
		}
	}

	g_browser->memfree(identifiers);

	*result = *ObjectVariant(array);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_Construct. The first argument is the object type to
 * be constructed, and the remaining arguments will be passed on to the
 * object's constructor directly.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testConstruct(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc == 0 || !NPVARIANT_IS_OBJECT(args[0]))
		return false;

	return g_browser->construct(instance, args[0].value.objectValue, args + 1, argc - 1, result);
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_SetException. The only argument is a string.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testSetException(NPObject* object, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 1 || !NPVARIANT_IS_STRING(args[0]))
		return false;

	/* Copy the string in case the browser decides to release it. */
	NPUTF8* message = static_cast<NPUTF8*>(g_browser->memalloc(args[0].value.stringValue.UTF8Length + 1));
	memcpy(message, args[0].value.stringValue.UTF8Characters, args[0].value.stringValue.UTF8Length);
	message[args[0].value.stringValue.UTF8Length] = 0;

	g_browser->setexception(object, message);

	return true;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_CreateObject. Takes no arguments, returns an object.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testCreateObject(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	if (NPObject* obj = g_browser->createobject(instance, build_object_class()))
	{
		*result = *ObjectVariant(obj);
		g_object_instance_map[obj] = this;
	}
	else
		result->type = NPVariantType_Null;

	return true;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_RetainObject. Takes one argument, an NPObject.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testRetainObject(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_OBJECT(args[0]))
		return false;

	g_browser->retainobject(args[0].value.objectValue);

	result->type = NPVariantType_Void;
	return true;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_ReleaseObject. Takes one argument, an NPObject.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testReleaseObject(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_OBJECT(args[0]))
		return false;

	g_browser->releaseobject(args[0].value.objectValue);

	result->type = NPVariantType_Void;
	return true;
}


/**
 * Ecmascript native method.
 *
 * Retrieve reference count of an object as claimed by the browser.
 * Takes one object argument and returns an integral value.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::getReferenceCount(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_OBJECT(args[0]))
		return false;

	*result = *IntVariant(args[0].value.objectValue->referenceCount);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Force reference count of NPObject to a specific value. Takes
 * two arguments, an object and a numeric value. Effects on
 * browser are undefined.
 *
 * @param object Object to describe.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::setReferenceCount(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_NUMBER(args[1]))
		return false;

	args[0].value.objectValue->referenceCount = NPVARIANT_GET_NUMBER(args[1]);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_GetURLNotify. The first argument is a string containing
 * the URL for contents to be fetched, the second is a string giving the target
 * of the request (null is acceptable), and the third is an object that will be
 * passed as notifyData to the stream and notification handlers.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetURLNotify(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_STRING(args[0]))
		return false;

	char* url = GetString(args[0].value.stringValue);
	char* window = (argc > 1 && NPVARIANT_IS_STRING(args[1])) ? GetString(args[1].value.stringValue) : 0;
	NPObject* data = (argc > 2 && NPVARIANT_IS_OBJECT(args[2])) ? args[2].value.objectValue : 0;

	bool ret = url && g_browser->geturlnotify(instance, url, window, data) == NPERR_NO_ERROR;

	if (data)
	{
		if (ret)
			g_browser->retainobject(data);
		else
			g_browser->releaseobject(data);
	}

	g_browser->memfree(url);
	g_browser->memfree(window);

	*result = *BoolVariant(ret);
	return ret;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_GetURL. The first argument is a string containing
 * the URL, the second argument is a string describing the target.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetURL(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_STRING(args[0]))
		return false;

	char* url = GetString(args[0].value.stringValue);
	char* window = (argc > 1 && NPVARIANT_IS_STRING(args[1])) ? GetString(args[1].value.stringValue) : 0;

	bool ret = false;

	if (g_browser->geturl(instance, url, window) == NPERR_NO_ERROR)
		ret = true;

	g_browser->memfree(url);
	g_browser->memfree(window);

	*result = *BoolVariant(ret);
	return ret;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_PostURLNotify. The first argument is an URL, the second a target,
 * the third a string. The fourth is a boolean value defining whether the preceding
 * string is a file path (true) or raw data. The final and fifth argument is an object
 * that will be used as notifyData.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testPostURLNotify(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4 || !NPVARIANT_IS_STRING(args[0]) || !NPVARIANT_IS_STRING(args[2]) || !NPVARIANT_IS_BOOLEAN(args[3]))
		return false;

	char* url = GetString(args[0].value.stringValue);
	char* window = NPVARIANT_IS_STRING(args[1]) ? GetString(args[1].value.stringValue) : 0;
	char* content = GetString(args[2].value.stringValue);
	bool file = !!args[3].value.boolValue;
	NPObject* data = (argc > 4 && NPVARIANT_IS_OBJECT(args[4])) ? args[4].value.objectValue : 0;

	bool ret = url && g_browser->posturlnotify(instance, url, window, args[2].value.stringValue.UTF8Length, content, file, data) == NPERR_NO_ERROR;
	if (!ret && data)
		g_browser->releaseobject(data);
	else
		g_browser->retainobject(data);

	g_browser->memfree(url);
	g_browser->memfree(window);
	g_browser->memfree(content);

	*result = *BoolVariant(ret);
	return ret;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_PostURL. The first argument is an URL, the second a target,
 * the third a string. The fourth is a boolean value defining whether the preceding
 * string is a file path (true) or raw data.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testPostURL(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4 || !NPVARIANT_IS_STRING(args[0]) || !NPVARIANT_IS_STRING(args[2]) || !NPVARIANT_IS_BOOLEAN(args[3]))
		return false;

	char* url = GetString(args[0].value.stringValue);
	char* window = NPVARIANT_IS_STRING(args[1]) ? GetString(args[1].value.stringValue) : 0;
	char* content = GetString(args[2].value.stringValue);
	bool file = !!args[3].value.boolValue;

	bool ret = url && g_browser->posturl(instance, url, window, args[2].value.stringValue.UTF8Length, content, file) == NPERR_NO_ERROR;

	g_browser->memfree(url);
	g_browser->memfree(window);
	g_browser->memfree(content);

	*result = *BoolVariant(ret);
	return ret;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_RequestRead. The first argument is a stream object received
 * from .onNewStream, the remaining arguments are paired integers, such that e.g.
 * testRequestRead(stream, 4, 1023, 1654, 1660) will request reads of the intervals
 * 4-1023 and 1654-1660. Hence any call must have an odd number of arguments > 1.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testRequestRead(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	/* Verify odd number of arguments > 1 starting with an object. */
	if (argc % 2 == 0 || argc < 3 || !NPVARIANT_IS_OBJECT(args[0]))
		return false;

	/* Verify that the stream supplied is valid. */
	StreamMap::iterator it = streams.find(args[0].value.objectValue);
	if (it == streams.end())
		return false;

	/* Create linked list of byte ranges on the stack. */
	const int num_ranges = (argc - 1) / 2;
	NPByteRange* byte_ranges = static_cast<NPByteRange*>(alloca(sizeof(NPByteRange) * num_ranges));
	for (int i = 0; i < num_ranges; i++)
	{
		int first = 1 + i * 2, second = 2 + i * 2;
		if (!NPVARIANT_IS_NUMBER(args[first]) || !NPVARIANT_IS_NUMBER(args[second]))
			return false;

		byte_ranges[i].offset = NPVARIANT_GET_NUMBER(args[first]);
		byte_ranges[i].length = NPVARIANT_GET_NUMBER(args[second]) - NPVARIANT_GET_NUMBER(args[first]);
		byte_ranges[i].next = i + 1 < num_ranges ? &byte_ranges[i + 1] : 0;
	}

	/* Call browser. */
	return g_browser->requestread(it->second, byte_ranges) == NPERR_NO_ERROR;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_NewStream. The first argument is a string containing a mime type,
 * the second is a string containing the target (which may be null). The function will
 * return the stream object created by the browser, which can be used as an argument to
 * subsequent testWrite operations to send data to the browser.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testNewStream(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_STRING(args[0]))
		return false;

	char* mime_type = GetString(args[0].value.stringValue);
	char* target = argc > 1 && NPVARIANT_IS_STRING(args[1]) ? GetString(args[1].value.stringValue) : 0;

	NPStream* stream;
	bool ret = g_browser->newstream(instance, mime_type, target, &stream) == NPERR_NO_ERROR;

	if (ret)
	{
		if (NPObject* stream_object = CreateStreamObject(stream))
			*result = *ObjectVariant(stream_object);
		else
			ret = false;
	}

	g_browser->memfree(mime_type);
	g_browser->memfree(target);

	return ret;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_Write. The first argument is a stream object received via
 * testNewStream or onNewStream, the second is an array of byte values that will
 * be transmitted to the browser. Returns the number of bytes written.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testWrite(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
#if 0
	if (argc < 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_OBJECT(args[1]))
		return false;

	StreamMap::iterator it = streams.find(args[0].value.objectValue);
	if (it == streams.end())
		return false;

	/* Obtain array size. */
	NPVariant length;
	if (!g_browser->getproperty(instance, args[0].value.objectValue, "length", &length) || !NPVARIANT_IS_NUMBER(length))
		return false;

	/* Allocate sufficient storage on the stack. */
	char* buffer = static_cast<char*>(alloca(NPVARIANT_GET_NUMBER(length)));
	if (!buffer)
		return false;

	/* Retrieve all array values and enter them into native storage. */
	for (int i = 0; i < NPVARIANT_GET_NUMBER(length); i++)
	{
		NPVariant value;
		if (!g_browser->getproperty(instance, args[0].value.objectValue, GetIdentifier(i), &value) || !NPVARIANT_IS_NUMBER(value))
			return false;

		buffer[i] = static_cast<char>(NPVARIANT_GET_NUMBER(value));
	}

	/* Transmit. */
	*result = *IntVariant(g_browser->write(instance, it->second, NPVARIANT_GET_NUMBER(length), buffer));

	return true;
#else
	return false;
#endif
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_DestroyStream. The first argument is a stream object
 * (as provided by onNewStream et al), and the second is a reason for the
 * destruction, an integer mapping directly to NPReason.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testDestroyStream(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 2 || !NPVARIANT_IS_OBJECT(args[0]) || !NPVARIANT_IS_NUMBER(args[1]))
		return false;

	StreamMap::iterator it = streams.find(args[0].value.objectValue);
	if (it == streams.end())
		return false;

	return g_browser->destroystream(instance, it->second, NPVARIANT_GET_NUMBER(args[1])) == NPERR_NO_ERROR;
}


/**
 * Ecmascript native method.
 *
 * Wrapper for NPN_Status. The first and only argument is a string
 * that will be sent as an argument to NPN_Status.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testStatus(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	/* Require one string argument. */
	if (argc == 0 || !NPVARIANT_IS_STRING(args[0]))
		return false;

	char* text = GetString(args[0].value.stringValue);
	g_browser->status(instance, text);
	g_browser->memfree(text);

	/* Return success. */
	result->type = NPVariantType_Null;
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_UserAgent. Takes no arguments. Returns a string
 * with the user agent.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testUserAgent(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	const char* ua = g_browser->uagent(instance);
	char* newstr = static_cast<char*>(g_browser->memalloc(static_cast<uint32_t>(strlen(ua))+1));

	memcpy(newstr, ua, strlen(ua)+1);

	*result = *StringVariant(newstr);
	return result->value.stringValue.UTF8Length > 0;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_MemFlush. First and only argument is the number
 * of bytes to free. Will return the number of bytes actually released.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testMemFlush(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_NUMBER(args[0]))
		return false;

	*result = *IntVariant(g_browser->memflush(NPVARIANT_GET_NUMBER(args[0])));
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_ReloadPlugins. Takes one boolean argument specifying
 * whether to reload pages. Returns nothing.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testReloadPlugins(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 1 || !NPVARIANT_IS_BOOLEAN(args[0]))
		return false;

	g_browser->reloadplugins(!!args[0].value.boolValue);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_GetJavaEnv. Takes no arguments, and returns
 * a partial numeric conversion of the pointer returned.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetJavaEnv(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	void* p = g_browser->getJavaEnv ? g_browser->getJavaEnv() : NULL;
	*result = *IntVariant(p ? 1 : 0);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_GetJavaPeer. Takes no arguments, and returns
 * a partial numeric conversion of the pointer returned.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetJavaPeer(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* result)
{
	void* p = g_browser->getJavaPeer ? g_browser->getJavaPeer(instance) : NULL;
	*result = *IntVariant(p ? 1 : 0);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_GetValue. The argument is a numeric value mapping to
 * the NPNVariable enum. The value returned is dependent upon choice of
 * variable (see source), but will be undefined on unhandled values.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetValue(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_NUMBER(args[0]))
		return false;

	NPNVariable var = static_cast<NPNVariable>(NPVARIANT_GET_NUMBER(args[0]));

	switch(var)
	{
	case NPNVjavascriptEnabledBool:
	case NPNVasdEnabledBool:
	case NPNVisOfflineBool:
	case NPNVSupportsXEmbedBool:
	case NPNVprivateModeBool:
	case NPNVSupportsWindowless:
	case NPNVsupportsAdvancedKeyHandling:
#ifdef XP_MACOSX
# ifndef NP_NO_CARBON
	case NPNVsupportsQuickDrawBool:
	case NPNVsupportsCarbonBool:
# endif // NP_NO_CARBON
	case NPNVsupportsCoreGraphicsBool:
	case NPNVsupportsOpenGLBool:
	case NPNVsupportsCoreAnimationBool:
	case NPNVsupportsInvalidatingCoreAnimationBool:
	case NPNVsupportsCocoaBool:
	case NPNVsupportsUpdatedCocoaTextInputBool:
#endif // XP_MACOSX
		*result = *BoolVariant(false);
		return g_browser->getvalue(instance, var, &result->value.boolValue) == NPERR_NO_ERROR;

	case NPNVxDisplay:
	case NPNVxtAppContext:
	case NPNVnetscapeWindow:
	case NPNVToolkit:
	case NPNVserviceManager:
	case NPNVDOMElement:
	case NPNVDOMWindow:
		assert(!"Implement me.");
		return false;

	case NPNVWindowNPObject:
	case NPNVPluginElementNPObject:
		*result = *ObjectVariant(0);
		return g_browser->getvalue(instance, var, &result->value.objectValue) == NPERR_NO_ERROR;

#ifdef XP_MACOSX
	case NPNVpluginDrawingModel:
		*result = *IntVariant(-1);
		return g_browser->getvalue(instance, var, &result->value.intValue) == NPERR_NO_ERROR;
		return false;
#endif // XP_MACOSX
	};

	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_SetValue. The first argument is an integer mapping to the
 * NPPVariable enum, the second is the value, dependent upon the value. See source.
 * Will return undefined on unknown or unhandled enum.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testSetValue(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (argc < 2 || !NPVARIANT_IS_NUMBER(args[0]))
		return false;

	NPPVariable var = static_cast<NPPVariable>(NPVARIANT_GET_NUMBER(args[0]));

	switch(var)
	{
	case NPPVpluginWindowBool:
	case NPPVpluginTransparentBool:
	case NPPVjavascriptPushCallerBool:
	case NPPVpluginKeepLibraryInMemory:
	case NPPVpluginUrlRequestsDisplayedBool:
	case NPPVpluginUsesDOMForCursorBool:
	case NPPVsupportsAdvancedKeyHandling:
	case NPPVpluginWantsAllNetworkStreams:
	case NPPVpluginCancelSrcStream:
		return g_browser->setvalue(instance, var, const_cast<bool*>(&args[0].value.boolValue)) == NPERR_NO_ERROR;

	case NPPVpluginScriptableNPObject:
		return g_browser->setvalue(instance, var, (NPObject*)(&args[0].value.objectValue)) == NPERR_NO_ERROR;

	case NPPVjavaClass:
	case NPPVpluginWindowSize:
	case NPPVpluginTimerInterval:
	case NPPVpluginScriptableInstance:
	case NPPVpluginScriptableIID:
	case NPPVpluginNeedsXEmbed:
	case NPPVformValue:
	case NPPVpluginNameString:
	case NPPVpluginDescriptionString:
	case NPPVpluginNativeAccessibleAtkPlugId:
#ifdef XP_MACOSX
	case NPPVpluginDrawingModel:
	case NPPVpluginEventModel:
	case NPPVpluginCoreAnimationLayer:
#endif // XP_MACOSX
		assert(!"Implement me.");
		return false;
	};

	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_InvalidateRect. Takes four numeric arguments,
 * x, y, width, height, defining the rectangle to be invalidated.
 * Returns null on success.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testInvalidateRect(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4)
		return false;

	for (int i = 0; i < 4; i++)
		if (!NPVARIANT_IS_NUMBER(args[i]))
			return false;

	NPRect rect;
	rect.left = NPVARIANT_GET_NUMBER(args[0]);
	rect.top = NPVARIANT_GET_NUMBER(args[1]);
	rect.right = rect.left + NPVARIANT_GET_NUMBER(args[2]);
	rect.bottom = rect.top + NPVARIANT_GET_NUMBER(args[3]);

	g_browser->invalidaterect(instance, &rect);
	result->type = NPVariantType_Null;
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_InvalidateRegion. Takes four numeric arguments,
 * x, y, width, height, defining the rectangle to be invalidated.
 * Returns null if implemented for the platform.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testInvalidateRegion(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4)
		return false;

	for (int i = 0; i < 4; i++)
		if (!NPVARIANT_IS_NUMBER(args[i]))
			return false;

#ifdef XP_WIN
	HRGN hrgn = CreateRectRgn(NPVARIANT_GET_NUMBER(args[0]), NPVARIANT_GET_NUMBER(args[1]),
			NPVARIANT_GET_NUMBER(args[0]) + NPVARIANT_GET_NUMBER(args[2]), NPVARIANT_GET_NUMBER(args[1]) + NPVARIANT_GET_NUMBER(args[3]));
	g_browser->invalidateregion(instance, hrgn);
	DeleteObject(hrgn);
#elif XP_UNIX
	XRectangle rect;
	rect.x = NPVARIANT_GET_NUMBER(args[0]);
	rect.y = NPVARIANT_GET_NUMBER(args[1]);
	rect.width = NPVARIANT_GET_NUMBER(args[2]);
	rect.height = NPVARIANT_GET_NUMBER(args[3]);

	Region region = XCreateRegion();
	XUnionRectWithRegion(&rect, region, region);

	g_browser->invalidateregion(instance, region);
	XDestroyRegion(region);
#elif XP_MACOSX
	CGRect rect;
	rect.origin.x = NPVARIANT_GET_NUMBER(args[0]);
	rect.origin.y = NPVARIANT_GET_NUMBER(args[1]);
	rect.size.width = NPVARIANT_GET_NUMBER(args[2]);
	rect.size.height = NPVARIANT_GET_NUMBER(args[3]);

	CGMutablePathRef path = CGPathCreateMutable();
	CGPathAddRect(path, NULL, rect);

	g_browser->invalidateregion(instance, path);
	CGPathRelease(path);
#endif // XP_MACOSX

	result->type = NPVariantType_Null;
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_ForceRedraw. Takes no arguments.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testForceRedraw(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	g_browser->forceredraw(instance);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_PushPopupsEnabledState. Takes one boolean argument
 * specifying whether popups should be allowed to open. Returns nothing.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testPushPopupsEnabledState(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* /*result*/)
{
	if (!g_browser->pushpopupsenabledstate || argc < 1 || !NPVARIANT_IS_BOOLEAN(args[0]))
		return false;

	g_browser->pushpopupsenabledstate(instance, args[0].value.boolValue);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_PopPopupsEnabledState. Takes no arguments.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testPopPopupsEnabledState(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	if (!g_browser->poppopupsenabledstate)
		return false;

	g_browser->poppopupsenabledstate(instance);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_PluginThreadAsyncCall. Takes no arguments,
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testPluginThreadAsyncCall(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	RequestAsyncCall(AsyncCallRecord::DocumentRequest);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetValueForURL(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	assert(!"Implement me.");
	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testSetValueForURL(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	assert(!"Implement me.");
	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testGetAuthenticationInfo(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	assert(!"Implement me.");
	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_ScheduleTimer. The first argument is a timer interval,
 * the second is a boolean value indicating whether repetition is desired,
 * and the third is a function to call when the timer triggers. The return
 * value is the numeric ID of the timer, if the call succeded.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testScheduleTimer(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 3 || !NPVARIANT_IS_NUMBER(args[0]) || !NPVARIANT_IS_BOOLEAN(args[1]) || !NPVARIANT_IS_OBJECT(args[2]))
		return false;

	int id = g_browser->scheduletimer(instance, NPVARIANT_GET_NUMBER(args[0]), !!args[1].value.boolValue, timeout);
	if (!id)
		return false;

	g_browser->retainobject(args[2].value.objectValue);
	timers[id] = args[2].value.objectValue;

	*result = *IntVariant(id);
	return true;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_UnscheduleTimer. The first argument is the numeric
 * ID of the timer as returned from NPN_ScheduleTimer. The return value is
 * true if the removal succeded.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testUnscheduleTimer(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 1 || !NPVARIANT_IS_NUMBER(args[0]))
		return false;

	TimerMap::iterator it = timers.find(NPVARIANT_GET_NUMBER(args[0]));
	if (it != timers.end())
	{
		timers.erase(it);

		g_browser->unscheduletimer(instance, NPVARIANT_GET_NUMBER(args[0]));
		*result = *BoolVariant(true);
		return true;
	}

	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testPopupContextMenu(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* /*args*/, uint32_t /*argc*/, NPVariant* /*result*/)
{
	assert(!"Implement me.");
	return false;
}


/**
 * Ecmascript native method.
 *
 * Test method for NPN_ConvertPoint. The first two arguments are x and y
 * coordinates. Third and fourth are source and destination coordinate space
 * (NPCoordinateSpace enumeration value). The return value is a javascript
 * array with converted coordinates.
 *
 * @param object Object carrying the method.
 * @param name Name of method.
 * @param args Method arguments.
 * @param argc Argument count.
 * @param result Returned value.
 *
 * @return true on success.
 */
bool PluginInstance::testConvertPoint(NPObject* /*object*/, NPIdentifier /*name*/, const NPVariant* args, uint32_t argc, NPVariant* result)
{
	if (argc < 4)
		return false;

	for (int i = 0; i < 4; i++)
		if (!NPVARIANT_IS_NUMBER(args[i]))
			return false;

	double dest_x = 0;
	double dest_y = 0;

	if (g_browser->convertpoint(instance, NPVARIANT_GET_NUMBER(args[0]), NPVARIANT_GET_NUMBER(args[1]),
		static_cast<NPCoordinateSpace>(NPVARIANT_GET_NUMBER(args[2])),
		&dest_x, &dest_y,
		static_cast<NPCoordinateSpace>(NPVARIANT_GET_NUMBER(args[3]))))
	{
		/* Create an array with converted coordinates. */
		if (NPObject* value = CreateArray())
		{
			if (!g_browser->setproperty(instance, value, GetIdentifier(0), &DoubleVariant(dest_x))
				|| !g_browser->setproperty(instance, value, GetIdentifier(1), &DoubleVariant(dest_y)))
			{
				g_browser->releaseobject(value);
				return false;
			}

			*result = *ObjectVariant(value);
			return true;
		}
	}

	return false;
}


#ifdef XP_WIN
/**
 * Paint windowless plugin contents.
 *
 * Called by HandleEvent{s} on WM_PAINT.
 *
 * @param drawable Windows drawable context.
 * @param x X coordinate of region requiring repaint.
 * @param y Y coordinate of region requiring repaint.
 * @param width Width of region requiring repaint.
 * @param height Height of region requiring repaint.
 *
 * @returns true if event was handled.
 */
bool PluginInstance::OnPaint(HDC drawable, int x, int y, int width, int height)
{
	SetProperty(scriptable_object, GetIdentifier("paintCount"), &IntVariant(++paint_count));

	/* Invoke document listener. */
	NPVariant paint_function;
	paint_function.type = NPVariantType_Void;

	NPVariant args[4] = { *IntVariant(x), *IntVariant(y), *IntVariant(width), *IntVariant(height) };

	bool document_involved = InvokeHandler("onPaint", args, 4, &paint_function);
	bool has_paint_function = document_involved && NPVARIANT_IS_OBJECT(paint_function);

	/* Paint using default method or function supplied by document. */
	int r, g, b;
	bool painted = false;

	if (!has_paint_function && GetBGColor(r, g, b))
		if (HBRUSH brush = CreateSolidBrush(RGB(r, g, b)))
		{
			RECT rect;
			rect.left = x;
			rect.top = y;
			rect.right = x + width;
			rect.bottom = y + height;

			painted = !!FillRect(drawable, &rect, brush);
			DeleteObject(brush);
		}

	if (!painted)
		for (int yt = y; yt < y + height; yt++)
			for (int xt = x; xt < x + width; xt++)
			{
				GetColor(has_paint_function ? paint_function.value.objectValue : 0, xt, yt, r, g, b);
				SetPixel(drawable, xt, yt, RGB(r, g, b));
			}

	if (document_involved)
		g_browser->releasevariantvalue(&paint_function);

	return true;
}
#endif // XP_WIN


#ifdef XP_UNIX
/**
 * Dispatch X11 event to the appropriate handler.
 *
 * @param event X11 event.
 *
 * @return True if handler found and said handled.
 */
bool PluginInstance::HandleXEvent(XEvent* event)
{
	switch (event->type)
	{
		case Expose:
		case GraphicsExpose:
			return OnPaint(event->xgraphicsexpose.display, event->xgraphicsexpose.drawable,
					event->xgraphicsexpose.x, event->xgraphicsexpose.y,
					event->xgraphicsexpose.width, event->xgraphicsexpose.height);

		case ButtonPress:
		case ButtonRelease:
			return OnMouseButton(event->xbutton.type, event->xbutton.x, event->xbutton.y, event->xbutton.button);

		case MotionNotify:
			return OnMouseMotion(event->xmotion.x, event->xmotion.y, MouseMove);

		case KeyPress:
		case KeyRelease:
			return OnKey(event->xkey.type, event->xkey.keycode, 0, NULL, NULL);
	};

	return false;
}


#define RGB(r, g, b) ((r << 16) | (g << 8) | (b))
/**
 * Paint windowless plugin contents.
 *
 * Called by HandleEvent on GraphicsExpose.
 *
 * @param display X11 connection.
 * @param drawable Target on which to draw.
 * @param x Left coordinate of exposed rectangle.
 * @param y Upper coordinate of exposed rectangle.
 * @param width Width of exposed rectangle.
 * @param height Height of exposed rectangle.
 *
 * @returns true if event was handled.
 */
bool PluginInstance::OnPaint(Display* display, Drawable drawable, int x, int y, int width, int height)
{
	SetProperty(scriptable_object, GetIdentifier("paintCount"), &IntVariant(++paint_count));

	/* Invoke document listener. */
	NPVariant paint_function;
	paint_function.type = NPVariantType_Void;

	NPVariant args[4] = { *IntVariant(x), *IntVariant(y), *IntVariant(width), *IntVariant(height) };
	bool document_involved = InvokeHandler("onPaint", args, 4, &paint_function);
	bool has_paint_function = document_involved && NPVARIANT_IS_OBJECT(paint_function);

	/* Paint using default method or function supplied by document. */
	GC gc = XCreateGC(display, drawable, 0, 0);

	int r, g, b;
	if (!has_paint_function && GetBGColor(r, g, b))
	{
		XSetForeground(display, gc, RGB(r, g, b));
		XFillRectangle(display, drawable, gc, x, y, width, height);
	}
	else
	{
		for (int yt = y; yt < y + height; yt++)
			for (int xt = x; xt < x + width; xt++)
			{
				GetColor(has_paint_function ? paint_function.value.objectValue : 0, xt, yt, r, g, b);
				XSetForeground(display, gc, RGB(r, g, b));
				XDrawRectangle(display, drawable, gc, xt, yt, 0, 0);
			}
	}

	XFlush(display);
	XFreeGC(display, gc);

	if (document_involved)
		g_browser->releasevariantvalue(&paint_function);

	return true;
}


/**
 * Paint windowed plugin contents.
 *
 * Called by HandleGtkEvent on GDK_EXPOSE.
 *
 * @param drawable Gdk drawable.
 * @param context context on which to draw.
 * @param x Left coordinate of exposed rectangle.
 * @param y Upper coordinate of exposed rectangle.
 * @param width Width of exposed rectangle.
 * @param height Height of exposed rectangle.
 *
 * @returns true if event was handled.
 */
bool PluginInstance::OnPaint(GdkDrawable* drawable, GdkGC* context, int x, int y, int width, int height)
{
	SetProperty(scriptable_object, GetIdentifier("paintCount"), &IntVariant(++paint_count));

	/* Invoke document listener. */
	NPVariant paint_function;
	paint_function.type = NPVariantType_Void;

	NPVariant args[4] = { *IntVariant(x), *IntVariant(y), *IntVariant(width), *IntVariant(height) };
	bool document_involved = InvokeHandler("onPaint", args, 4, &paint_function);
	bool has_paint_function = document_involved && NPVARIANT_IS_OBJECT(paint_function);

	/* Verify that our paint data wasn't invalidated while we waited for the document handler. */
	if (!drawable || !context || !GDK_IS_GC(context))
		return false;

	/* Paint using default method or function supplied by document. */
	int r, g, b;
	if (!has_paint_function && GetBGColor(r, g, b))
	{
		GdkColor color = {0, r * 257, g * 257, b * 257};
		gdk_gc_set_rgb_fg_color(context, &color);
		gdk_draw_rectangle(drawable, context, TRUE, x, y, width, height);
	}
	else
	{
		for (int yt = y; yt < y + height; yt++)
			for (int xt = x; xt < x + width; xt++)
			{
				GetColor(has_paint_function ? paint_function.value.objectValue : 0, xt, yt, r, g, b);
				GdkColor color = {0, r * 257, g * 257, b * 257};
				gdk_gc_set_rgb_fg_color(context, &color);
				gdk_draw_point(drawable, context, xt, yt);
			}
	}

	if (document_involved)
		g_browser->releasevariantvalue(&paint_function);

	return true;
}
#endif // XP_UNIX

#ifdef XP_GOGI

bool PluginInstance::HandleGOGIEvent(GogiPluginEvent* event)
{
	switch (event->type)
	{
		case GOGI_PLUGIN_EVENT_PAINT:
		{
			void* paint_buffer;
			GogiPluginWindow* pw;
			GogiPluginPaintInfo* pi;

			if (!plugin_window)
				return false;

			// Extract the native window from plugin_window
			pw = (GogiPluginWindow*) plugin_window->window;
			if (!pw)
				return false;

			pi = pw->get_paint_info(pw);
			if (!pi)
				return false;

			GogiPluginEventPaint* paint_event = (GogiPluginEventPaint*) event;

			// Set the buffer that the plugin should draw on
			paint_buffer = paint_event->area_ptr;

			// Retrieve the coordinates for the drawing area
			int x = paint_event->paint_area.x;
			int y = paint_event->paint_area.y;
			int width = paint_event->paint_area.w;
			int height = paint_event->paint_area.h;

			return OnPaint(pi->pixel_format, paint_buffer, x, y, width, height);
		}
		case GOGI_PLUGIN_EVENT_KEYBOARD:
		{
			// Convert the event to a keyboard event
			GogiPluginEventKeyboard* keyb_event = (GogiPluginEventKeyboard*) event;

			return OnKey(keyb_event->reason == GOGI_KEYDOWN ? KeyPress : KeyRelease,
					keyb_event->virtual_key, keyb_event->keymod, NULL, NULL);
		}
		case GOGI_PLUGIN_EVENT_MOUSE:
		{
			// Convert the event to a mouse event
			GogiPluginEventMouse* mouse_event = (GogiPluginEventMouse*) event;

			switch (mouse_event->reason)
			{
				case GOGI_MOUSE_MOVE:
					return OnMouseMotion(mouse_event->x, mouse_event->y, MouseMove);
				case GOGI_MOUSE_DOWN:
					return OnMouseButton(ButtonPress, mouse_event->x, mouse_event->y, mouse_event->button);
				case GOGI_MOUSE_UP:
					return OnMouseButton(ButtonRelease, mouse_event->x, mouse_event->y, mouse_event->button);
				default:
					return false;
			}

			return false;
		}
		case GOGI_PLUGIN_EVENT_ACTIVATE:
			return true;
		case GOGI_PLUGIN_EVENT_FOCUS:
			return OnFocus(true, ((GogiPluginEventFocus*)event)->focus);
		default:
			printf("Unknown event received\n!");
	}

	return false;
}

#define BGRA32(r, g, b, a) ((a << 24) | (r << 16) | (g << 8) | (b))
#define RGBA32(r, g, b, a) ((a << 24) | (b << 16) | (g << 8) | (r))
#define ABGR32(r, g, b, a) ((r << 24) | (g << 16) | (b << 8) | (a))

#define RGBA16(r, g, b, a) ((a << 12) | (b << 8) | (g << 4) | (r))
#define ABGR16(r, g, b, a) ((r << 12) | (g << 8) | (b << 4) | (a))
#define RGB16(r, g, b, a) ((b << 11) | (g << 5) | r)
#define BGR16(r, g, b, a) ((r << 11) | (g << 5) | b)

/**
 * Paints a pixel with a given color into a given buffer and converts the pixel
 * to the desired format.
 *
 * @param buffer A pointer to where the pixel should be put.
 * @param desired_format The format that the pixel should be saved in.
 * @param r The value for the red channel.
 * @param g The value for the green channel.
 * @param b The value for the blue channel.
 * @param a The value for the alpha channel.
 */
void PluginInstance::GOGIDrawPixel(int* buffer, GOGI_FORMAT desired_format, int r, int g, int b, int a)
{
	switch(desired_format)
	{
		case GOGI_FORMAT_VEGA_BGRA32:
			*buffer = BGRA32(r, g, b, a);
			break;
		case GOGI_FORMAT_VEGA_RGBA32:
			*buffer = RGBA32(r, g, b, a);
			break;
		case GOGI_FORMAT_VEGA_ABGR32:
			*buffer = ABGR32(r, g, b, a);
			break;
		case GOGI_FORMAT_VEGA_RGBA16:
			*buffer = RGBA16(r, g, b, a);
			break;
		case GOGI_FORMAT_VEGA_ABGR16:
			*buffer = ABGR16(r, g, b, a);
			break;
		case GOGI_FORMAT_VEGA_RGB16:
			*buffer = RGB16(r, g, b, a);
			break;
		case GOGI_FORMAT_VEGA_BGR16:
			*buffer = BGR16(r, g, b, a);
			break;
		default:
			printf("test_plugin: Unknown pixel format\n");
	}
}

/**
 * OnPaint should be called when a paint event has been triggered in the plugin. OnPaint will
 * take a surface to paint on, paint_buffer, what pixel format it should use and where to draw it.
 * It will then choose between painting a single colored background color, given by GetBGColor, or
 * it will use the default pattern from GetColor, or use a Javascript function as source.
 *
 * @param pixel_format Defines what pixel_format should be used when painting to the buffer.
 * @param paint_buffer The buffer that the plugin should draw on.
 * @param x Defines where the painting should begin on the x axis.
 * @param y Defines where the painting should begin on the y axis.
 * @param width Defines the width of the surface.
 * @param height Defines the height of the surface.
 */
bool PluginInstance::OnPaint(GOGI_FORMAT pixel_format, void* paint_buffer, int x, int y, int width, int height)
{
	// Set the javascript property paintCount
	SetProperty(scriptable_object, GetIdentifier("paintCount"), &IntVariant(++paint_count));

	// Invoke document listener.
	NPVariant paint_function;
	paint_function.type = NPVariantType_Void;

	NPVariant args[4] = { *IntVariant(x), *IntVariant(y), *IntVariant(width), *IntVariant(height) };
	bool document_involved = InvokeHandler("onPaint", args, 4, &paint_function);
	bool has_paint_function = document_involved && NPVARIANT_IS_OBJECT(paint_function);

	// Create variables for keeping track of what color we should paint and where.
	int r, g, b;
	int row, col;

	// Create a pointer that points to the start of the paint_buffer. Will be used to
	// iterate over the buffer and fill it with pixel data.
	int* buffer_ptr = (int*) paint_buffer;

	// See if we should draw the background color or use the GetColor method.
	bool draw_background = !has_paint_function && GetBGColor(r, g, b);

	// Perform painting
	for (row = y; row < height; row++)
	{
		for (col = x; col < width; col++)
		{
			if (!draw_background)
				GetColor(has_paint_function ? paint_function.value.objectValue : 0, col, row, r, g, b);

			GOGIDrawPixel(buffer_ptr, pixel_format, r, g, b, 0xFF);
			buffer_ptr++;
		}
	}

	return true;
}
#endif // XP_GOGI


#ifdef XP_MACOSX
# ifndef NP_NO_CARBON
/**
 * Dispatch Carbon event to the appropriate handler.
 *
 * @param event Carbon event.
 *
 * @return True if handler found and said handled.
 */
bool PluginInstance::HandleCarbonEvent(EventRecord* event)
{
	switch (event->what)
	{
		case updateEvt:
			if (drawing_model == NPDrawingModelCoreGraphics)
				/* Paint event does not carry information about dirty rectangle.
				   There is a possibility of checking CGPathRef and only painting
				   path defined by it but at least Opera does not set it. */
				return OnPaint(reinterpret_cast<NP_CGContext*>(plugin_window->window)->context,
						0, 0, plugin_window->width, plugin_window->height);
			else
				return false;

		case mouseDown:
		case mouseUp:
			return OnMouseButton(event->what == mouseDown ? ButtonPress : ButtonRelease,
					event->where.h, event->where.v, 1);

		case NPEventType_AdjustCursorEvent:
			return OnMouseMotion(event->where.h, event->where.v, MouseMove);

		case keyDown:
		case keyUp:
			return OnKey(event->what == keyDown ? KeyPress : KeyRelease, event->message, 0, NULL, NULL);
	}

	return false;
}
#endif // NP_NO_CARBON


/**
 * Dispatch Cocoa event to the appropriate handler.
 *
 * @param event Cocoa event.
 *
 * @return True if handler found and said handled.
 */
bool PluginInstance::HandleCocoaEvent(NPCocoaEvent* event)
{
	switch (event->type)
	{
		case NPCocoaEventDrawRect:
			if (drawing_model == NPDrawingModelCoreGraphics)
				return OnPaint(event->data.draw.context,
						event->data.draw.x, event->data.draw.y,
						event->data.draw.width, event->data.draw.height);
			else
				return false;

		case NPCocoaEventMouseMoved:
			return OnMouseMotion(event->data.mouse.pluginX, event->data.mouse.pluginY, MouseMove);

		case NPCocoaEventMouseDown:
		case NPCocoaEventMouseUp:
			return OnMouseButton(event->type == NPCocoaEventMouseDown ? ButtonPress : ButtonRelease,
				event->data.mouse.pluginX, event->data.mouse.pluginY, event->data.mouse.buttonNumber + 1);

		case NPCocoaEventMouseEntered:
			return OnMouseMotion(event->data.mouse.pluginX, event->data.mouse.pluginY, MouseOver);

		case NPCocoaEventMouseExited:
			return OnMouseMotion(event->data.mouse.pluginX, event->data.mouse.pluginY, MouseOut);

		case NPCocoaEventMouseDragged:
			return OnMouseMotion(event->data.mouse.pluginX, event->data.mouse.pluginY, MouseDrag);

		case NPCocoaEventScrollWheel:
			if (event->data.mouse.deltaY)
				OnMouseWheel(event->data.mouse.deltaY, true, event->data.mouse.pluginX, event->data.mouse.pluginY);
			if (event->data.mouse.deltaX)
				OnMouseWheel(event->data.mouse.deltaX, false, event->data.mouse.pluginX, event->data.mouse.pluginY);
			return true;

		case NPCocoaEventKeyDown:
		case NPCocoaEventKeyUp:
			return OnKey(event->type == NPCocoaEventKeyDown ? KeyPress : KeyRelease, event->data.key.keyCode, event->data.key.modifierFlags,
				GetString(event->data.key.characters), GetString(event->data.key.charactersIgnoringModifiers));

		case NPCocoaEventFlagsChanged:
			return OnKey(KeyModifierChange, event->data.key.keyCode, event->data.key.modifierFlags, NULL, NULL);

		case NPCocoaEventTextInput:
			return OnKey(TextInput, 0, 0, GetString(event->data.text.text), 0);

		case NPCocoaEventFocusChanged:
		case NPCocoaEventWindowFocusChanged:
			return OnFocus(event->type == NPCocoaEventFocusChanged, event->data.focus.hasFocus);
	}

	return false;
}


#define INT_TO_CGFLOAT(v) (CGFloat(v) / 255)
/**
 * Paint plugin contents to Core Graphics context.
 *
 * Called by HandleEvent on updateEvt (Carbon) or NPCocoaEventDrawRect (Cocoa).
 *
 * @param context Graphics context on which to draw.
 * @param x Left coordinate of exposed rectangle.
 * @param y Upper coordinate of exposed rectangle.
 * @param width Width of exposed rectangle.
 * @param height Height of exposed rectangle.
 *
 * @returns true if event was handled.
 */
bool PluginInstance::OnPaint(CGContextRef context, int x, int y, int width, int height)
{
	SetProperty(scriptable_object, GetIdentifier("paintCount"), &IntVariant(++paint_count));

	/* Invoke document listener. */
	NPVariant paint_function;
	paint_function.type = NPVariantType_Void;

	NPVariant args[4] = { *IntVariant(x), *IntVariant(y), *IntVariant(width), *IntVariant(height) };
	bool document_involved = InvokeHandler("onPaint", args, 4, &paint_function);
	bool has_paint_function = document_involved && NPVARIANT_IS_OBJECT(paint_function);

	CGContextSaveGState(context);

	/* Paint using default method or function supplied by document. */
	int r, g, b;
	if (!has_paint_function && GetBGColor(r, g, b))
	{
		CGContextSetRGBFillColor(context, INT_TO_CGFLOAT(r), INT_TO_CGFLOAT(g), INT_TO_CGFLOAT(b), 1);
		CGContextFillRect(context, CGRectMake(x, y, width, height));
	}
	else
	{
		for (int yt = y; yt < y + height; yt++)
			for (int xt = x; xt < x + width; xt++)
			{
				GetColor(has_paint_function ? paint_function.value.objectValue : 0, xt, yt, r, g, b);
				CGContextAddRect(context, CGRectMake(xt, yt, 1, 1));
				CGContextSetRGBFillColor(context, INT_TO_CGFLOAT(r), INT_TO_CGFLOAT(g), INT_TO_CGFLOAT(b), 1);
				CGContextDrawPath(context, kCGPathFill);
			}
	}

	CGContextRestoreGState(context);

	if (document_involved)
		g_browser->releasevariantvalue(&paint_function);

	return true;
}
#endif // XP_MACOSX


/**
 * Handle mouse button press or release.
 *
 * @param type ButtonPress or ButtonRelease.
 * @param x X coordinate of event.
 * @param y Y coordinate of event.
 * @param button Button identifier.
 *
 * @return true if event was handled.
 */
bool PluginInstance::OnMouseButton(int type, int x, int y, int button)
{
	/* Translate coordinates to plugin window. */
	int xt, yt;
	GetOriginRelativeToWindow(xt, yt);

	int typet = type == ButtonPress ? 0 : 1;

	/* Alert document. */
	NPVariant args[4] = { *IntVariant(x - xt), *IntVariant(y - yt), *IntVariant(typet), *IntVariant(button) };
	return InvokeHandler("onMouseEvent", args, 4);
}


/**
 * Handle mouse movement and related events.
 *
 * @param x X coordinate of mouse pointer.
 * @param y Y coordinate of mouse pointer.
 * @param type Type of the event.
 *
 * @return true if event was handled.
 */
bool PluginInstance::OnMouseMotion(int x, int y, int type)
{
	/* Translate coordinates to plugin window. */
	int xt, yt;
	GetOriginRelativeToWindow(xt, yt);

	switch (type)
	{
		case MouseMove: type = 2; break;
		case MouseOver: type = 4; break;
		case MouseOut:  type = 5; break;
		case MouseDrag: type = 6; break;
	}

	/* Alert document. */
	NPVariant args[3] = { *IntVariant(x - xt), *IntVariant(y - yt), *IntVariant(type) };
	return InvokeHandler("onMouseEvent", args, 3);
}


/**
 * Handle mouse wheel.
 *
 * @param delta Distance the wheel is rotated.
 * @param vertical True for vertical scroll, false for horizontal.
 * @param x X coordinate of mouse pointer.
 * @param y Y coordinate of mouse pointer.
 *
 * @return true if event was handled.
 */
bool PluginInstance::OnMouseWheel(double delta, bool vertical, int x, int y)
{
	/* Translate coordinates to plugin window. */
	int xt, yt;
	GetOriginRelativeToWindow(xt, yt);

	/* Alert document. */
	NPVariant args[5] = { *IntVariant(x - xt), *IntVariant(y - yt), *IntVariant(3), *DoubleVariant(delta), *BoolVariant(vertical) };
	return InvokeHandler("onMouseEvent", args, 5);
}


/**
 * Handle key press or release.
 *
 * @param type KeyPress or KeyRelease.
 * @param keycode Platform specific keycode generating event.
 * @param modifiers Key modifiers state.
 * @param chars Text representation of the event.
 * @param chars_no_mod Text representation of the event without considering modifiers.
 *
 * @return true if event was handled.
 */
bool PluginInstance::OnKey(int type, int keycode, int modifiers, const char* chars, const char* chars_no_mod)
{
	switch (type)
	{
		case KeyPress:          type = 0; break;
		case KeyRelease:        type = 1; break;
		case KeyModifierChange: type = 2; break;
		case TextInput:         type = 3; break;
	}

	/* Alert document. */
	NPVariant args[5] = { *IntVariant(keycode), *IntVariant(type), *IntVariant(modifiers), *StringVariant(chars), *StringVariant(chars_no_mod) };
	return InvokeHandler("onKeyEvent", args, 5);
}


/**
 * Handle focus event.
 *
 * @param type True if plugin changes focus or false if embedding tab does.
 * @param focus New focus state of the plugin or tab.
 *
 * @return true if event was handled.
 */
bool PluginInstance::OnFocus(bool type, bool focus)
{
	/* Alert document. */
	NPVariant args[2] = { *BoolVariant(type), *BoolVariant(focus) };
	return InvokeHandler("onFocusEvent", args, 2);
}


#define HEX_TO_NUM(c) (isdigit(static_cast<unsigned char>(c)) ? c - 48 : tolower(static_cast<unsigned char>(c)) - 97 + 10)
/**
 * Request color to paint at given pixel.
 *
 * @param[in] document_painter Document supplied paint function, may be 0.
 * @param[in] x X coordinate of pixel to paint.
 * @param[in] y Y coordinate of pixel to paint.
 * @param[out] r Red component of color to paint, in range 0-255.
 * @param[out] g Green component of color to paint, in range 0-255.
 * @param[out] b Blue component of color to paint, in range 0-255.
 */
void PluginInstance::GetColor(NPObject* document_painter, int x, int y, int& r, int& g, int& b)
{
	/* Default to checkerboard pattern. */
	r = g = b = (((x / 16) & 1) ^ ((y / 16) & 1)) * 255;

	if (document_painter)
	{
		NPVariant coordinates[2] = { *IntVariant(x), *IntVariant(y) };
		NPVariant color;
		bool got_color = g_browser->invokeDefault(instance, document_painter, coordinates, 2, &color);

		r = got_color ? GetIntProperty(color.value.objectValue, GetIdentifier("r"), r) : r;
		g = got_color ? GetIntProperty(color.value.objectValue, GetIdentifier("g"), g) : g;
		b = got_color ? GetIntProperty(color.value.objectValue, GetIdentifier("b"), b) : b;

		if (got_color)
			g_browser->releasevariantvalue(&color);
	}
}

/**
 * Request bgcolor if set as attribute or property.
 *
 * @param[out] r Red component of bgcolor.
 * @param[out] g Green component of bgcolor.
 * @param[out] b Blue component of bgcolor.
 */
bool PluginInstance::GetBGColor(int& r, int& g, int& b)
{
	NPString color;
	color.UTF8Length = 0;

	if (HasProperty(scriptable_object, GetIdentifier("bgcolor")))
	{
		NPVariant property;
		GetProperty(scriptable_object, GetIdentifier("bgcolor"), &property);
		color = property.value.stringValue;
	}
	else if (bgcolor)
	{
		color.UTF8Characters = bgcolor;
		color.UTF8Length = (uint32_t)strlen(bgcolor);
	}

	if (color.UTF8Length == 7 && color.UTF8Characters[0] == '#')
	{
		r = HEX_TO_NUM(color.UTF8Characters[1]) * 0x10 + HEX_TO_NUM(color.UTF8Characters[2]);
		g = HEX_TO_NUM(color.UTF8Characters[3]) * 0x10 + HEX_TO_NUM(color.UTF8Characters[4]);
		b = HEX_TO_NUM(color.UTF8Characters[5]) * 0x10 + HEX_TO_NUM(color.UTF8Characters[6]);

		return true;
	}

	return false;
}

/* virtual */ void PluginInstance::SafeDelete()
{
	instance = NULL;
	UseCounted::SafeDelete();
}
