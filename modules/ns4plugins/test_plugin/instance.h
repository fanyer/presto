/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Base plugin instance representation.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#ifndef PLUGIN_INSTANCE_H
#define PLUGIN_INSTANCE_H

#include "common.h"



/** Property name -> value map type. See SetProperty. */
typedef std::map<NPIdentifier, NPVariant> PropertyMap;

/** Method name -> C++ method map type. See Invoke. */
class PluginInstance;
typedef bool (PluginInstance::*Method)(NPObject*, NPIdentifier, const NPVariant*, uint32_t, NPVariant*);
typedef std::map<NPIdentifier, Method> MethodMap;

/** Ecmascript stream object -> NPStream* map type. See NewStream. */
typedef std::map<NPObject*, NPStream*> StreamMap;

/** Timer -> Function map. See testScheduleTimer. */
typedef std::map<uint32_t, NPObject*> TimerMap;

/** Async callback structure. */
struct AsyncCallRecord
{
	enum Reason { DocumentRequest, WindowedRequest } reason;
	PluginInstance* instance;
};

/**
 * Helper class to count use and prevent a class from being deleted
 * while it's still on the stack.
 */
class UseCounted
{
public:
	UseCounted() : m_use_count(0), m_delete(false) {}
	~UseCounted() {}

	void IncUse() { m_use_count++; }
	void DecUse() { if (!--m_use_count && m_delete) SafeDelete(); }
	virtual void SafeDelete() {
		m_delete = true;
		if (!m_use_count) {
			m_use_count = 1; /* Prevent premature deletion. */
			Delete();
		}
	}

	virtual void Delete() = 0;

protected:
	int m_use_count;
	bool m_delete;
};

/**
 * Helper class for UseCounted classes.
 */
class CountUse
{
public:
	CountUse(UseCounted* uc) : m_uc(uc) { m_uc->IncUse(); }
	~CountUse() { m_uc->DecUse(); }

protected:
	UseCounted* m_uc;
};

/**
 * Plugin instance base implementation and interface.
 *
 * Note that HandleEvent is defined pure virtual as it differs between windowed and windowless.
 */
class PluginInstance
	: public UseCounted
{
protected:
	/* Our identifier. */
	NPP instance;

	/* The global ecmascript object. May be NULL. */
	NPObject* global_object;

	/* Our DOM node. */
	NPObject* scriptable_object;

	/* Our properties and methods. */
	PropertyMap properties;
	MethodMap methods;

	/* Our live stream objects. */
	StreamMap streams;

	/* Our list of scheduled timers. */
	TimerMap timers;

	/* Window through which we interact. May be null. */
	NPWindow* plugin_window;

	/* Count of paint, setwindow calls. */
	int paint_count;
	int setwindow_count;
	int windowposchanged_count;

	/* Background color, formatted as text. May be null. */
	char* bgcolor;

	/* Set on running destructor to stop calls to browser that we would rather avoid. */
	bool is_destroying;

#ifdef XP_MACOSX
	/* Layer to be used for drawing in CoreAnimation model. */
	void* ca_layer;

	/* Plugin drawing model. Negotiated on runtime. */
	NPDrawingModel drawing_model;

	/* Plugin event model. Negotiated on runtime. */
	NPEventModel event_model;
#endif // XP_MACOSX

	/* Add/remove DOM node methods. */
	virtual void AddMethod(const char* identifier, Method);
	virtual void RemoveMethod(const char* identifier);
	virtual void RemoveMethod(NPIdentifier identfier);

	/* Invoke property handler. */
	virtual bool InvokeHandler(const char* name, const NPVariant* args = 0, uint32_t argc = 0, NPVariant* return_value = 0);

	/* Create Array object. */
	virtual NPObject* CreateArray();

	/* Create object via evaluate. */
	virtual NPObject* CreateObject(const char* script);

	/* Create ecmascript object from NPStream. */
	virtual NPObject* CreateStreamObject(NPStream* stream);

	/* Create ecmascript object from Window. */
	virtual NPObject* CreateWindowObject();

	/* Convert NPString to C string. */
	virtual char* GetString(const NPString& string);

#ifdef XP_MACOSX
	/* Convert NPNSString to C string. */
	virtual const char* GetString(NPNSString* nsstring);
#endif // XP_MACOSX

	/* Create or retrieve an NPIdentifier. */
	virtual NPIdentifier GetIdentifier(const NPString& string);
	virtual NPIdentifier GetIdentifier(const char* string);
	virtual NPIdentifier GetIdentifier(int32_t value);

	/* Retrieve INT32 property. */
	virtual int32_t GetIntProperty(NPObject* object, NPIdentifier name, int32_t default_value = 0);

public:
	PluginInstance(NPP instance, const char* bgcolor = 0);
	virtual ~PluginInstance();

	/* Enforce use of browser supplied memory allocation methods. */
	void* operator new(size_t) throw ();
	void operator delete(void*);

	/* Setup instance. Called during NPP_New. */
	virtual bool Initialize() = 0;

	/* Verify validity of supplied arguments. */
	virtual NPError CheckArguments(int16_t argc, char** argn, char** argv);

	/* Get the origin of the plugin window relative to the platform window. True on success. */
	virtual bool GetOriginRelativeToWindow(int& x, int& y) = 0;

	/* Retrieve the global object. */
	virtual NPObject* GetGlobalObject();

	/* Create DOM node representing this plugin instance. */
	virtual NPObject* CreateScriptableObject();

	/* Is plugin of type windowed or windowless. */
	virtual bool IsWindowless() = 0;

	/* DOM access methods. */
	virtual void Invalidate(NPObject* object);
	virtual void Deallocate(NPObject* object);
	virtual bool HasMethod(NPObject* object, NPIdentifier name);
	virtual bool Invoke(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	virtual bool InvokeDefault(NPObject* object, const NPVariant* args, uint32_t argc, NPVariant* result);
	virtual bool HasProperty(NPObject* object, NPIdentifier name);
	virtual bool GetProperty(NPObject* object, NPIdentifier name, NPVariant* result);
	virtual bool SetProperty(NPObject* object, NPIdentifier name, const NPVariant* value);
	virtual bool RemoveProperty(NPObject* object, NPIdentifier name);
	virtual bool Enumerate(NPObject* npobj, NPIdentifier** value, uint32_t* count);
	virtual bool Construct(NPObject* npobj, const NPVariant* args, uint32_t argc, NPVariant* result);

	/* Browser callbacks. */
	virtual NPError SetWindow(NPWindow* window);
	virtual NPError NewStream(NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
	virtual NPError DestroyStream(NPStream* stream, NPReason reason);
	virtual void StreamAsFile(NPStream* stream, const char* fname);
	virtual int32_t WriteReady(NPStream* stream);
	virtual int32_t Write(NPStream* stream, int32_t offset, int32_t len, void* buf);
	virtual void Print(NPPrint* PrintInfo);
	virtual void URLNotify(const char* url, NPReason reason, void* data);
	virtual NPError GetValue(NPPVariable variable, void* value);
	virtual NPError SetValue(NPNVariable variable, void* value);
	virtual NPBool GotFocus(NPFocusDirection direction);
	virtual void LostFocus();
	virtual void URLRedirectNotify(const char* url, int32_t status, void* notifyData);

	virtual void RequestAsyncCall(AsyncCallRecord::Reason reason);
	virtual void AsyncCall(AsyncCallRecord::Reason reason);
	virtual void Timeout(uint32_t id);

	virtual void Delete() { delete this; }
	virtual void SafeDelete();

protected:
	/*
	 * Native ecmascript methods. Note camel case convention.
	 */
	bool toString(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool getWindow(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool injectMouseEvent(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool injectKeyEvent(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool crash(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	virtual bool paint(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool getCookieCount(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool setCookie(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);

	/* Wrappers for NPRuntime functions implemented by the browser. */
	bool testInvoke(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testInvokeDefault(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testEvaluate(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetProperty(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testSetProperty(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testRemoveProperty(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testHasProperty(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testHasMethod(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testEnumerate(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testConstruct(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testSetException(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);

	bool testCreateObject(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testRetainObject(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testReleaseObject(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);

	/* And accompanying prober methods. */
	bool getReferenceCount(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool setReferenceCount(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);

	/* Test functions for NPAPI functions implemented by the browser. */
	bool testGetURLNotify(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetURL(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testPostURLNotify(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testPostURL(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testRequestRead(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testNewStream(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testWrite(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testDestroyStream(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testUserAgent(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testStatus(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testMemAlloc(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testMemFree(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testMemFlush(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testReloadPlugins(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetJavaEnv(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetJavaPeer(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetValue(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testSetValue(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testInvalidateRect(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testInvalidateRegion(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testForceRedraw(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testPushPopupsEnabledState(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testPopPopupsEnabledState(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testPluginThreadAsyncCall(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetValueForURL(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testSetValueForURL(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testGetAuthenticationInfo(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testScheduleTimer(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testUnscheduleTimer(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testPopupContextMenu(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	bool testConvertPoint(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);

	/* Event handler methods used by descendants. */
#ifdef XP_WIN
	virtual bool OnPaint(HDC drawable, int x, int y, int width, int height);
#endif // XP_WIN

#ifdef XP_GOGI
	void GOGIDrawPixel(int* buffer, GOGI_FORMAT desired_format, int r, int g, int b, int a);
	virtual bool HandleGOGIEvent(GogiPluginEvent* event);
	virtual bool OnPaint(GOGI_FORMAT pixel_format, void* paint_buffer, int x, int y, int width, int height);
#endif // XP_GOGI

#ifdef XP_UNIX
	virtual bool HandleXEvent(XEvent* event);
	virtual bool OnPaint(Display* display, Drawable drawable, int x, int y, int width, int height);
	virtual bool OnPaint(GdkDrawable* drawable, GdkGC* context, int x, int y, int width, int height);
#endif // XP_UNIX

#ifdef XP_MACOSX
public:
	virtual bool OnPaint(CGContextRef context, int x, int y, int width, int height);
protected:
# ifndef NP_NO_CARBON
	virtual bool HandleCarbonEvent(EventRecord* event);
# endif // NP_NO_CARBON
	virtual bool HandleCocoaEvent(NPCocoaEvent* event);
	virtual void InitializeCoreAnimationLayer(PluginInstance* instance);
	virtual void UninitializeCoreAnimationLayer();
	virtual void SetCoreAnimationLayer(void* value);
	virtual void UpdateCoreAnimationLayer();
#endif // XP_MACOSX

	virtual bool OnMouseButton(int type, int x, int y, int button);
	virtual bool OnMouseMotion(int x, int y, int type);
	virtual bool OnMouseWheel(double delta, bool vertical, int x, int y);
	virtual bool OnKey(int type, int keycode, int modifiers, const char* chars, const char* chars_no_mod);
	virtual bool OnFocus(bool type, bool focus);

	void GetColor(NPObject* document_painter, int x, int y, int& r, int& g, int& b);
	bool GetBGColor(int& r, int& g, int& b);
};

#endif // PLUGIN_INSTANCE_H
