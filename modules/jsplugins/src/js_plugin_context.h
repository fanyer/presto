/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JS_PLUGIN_CONTEXT_H
#define JS_PLUGIN_CONTEXT_H

#ifdef JS_PLUGIN_SUPPORT
#include "modules/jsplugins/jsplugin.h"
#include "modules/jsplugins/src/js_plugin_object.h"
#include "modules/jsplugins/src/js_plugin_version.h"

#include "modules/util/simset.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esthread.h"

struct jsplugin_cap;
class JS_Plugin_Manager;
class EcmaScript_Object;
class HTML_Element;
class DOM_Object;
class ES_Runtime;

class JS_Plugin_Poll
	: public Link
{
public:
	JS_Plugin_Poll(const jsplugin_obj *ctx)
		: global_context(ctx)
	{}

	unsigned id;

	const jsplugin_obj *global_context;
	unsigned interval;
	jsplugin_poll_callback *callback;
};

/**
 * JavaScript plug-in context. There is one of these per DOM environment
 * (window), representing the list of JavaScript plug-in objects available
 * to the system.
 */
class JS_Plugin_Context
	: public MessageObject
{
public:
	enum SuspensionOrigin
	{
		SUSPEND_NONE,
		SUSPEND_CALL,
		SUSPEND_GET
	};
private:
	JS_Plugin_Manager *manager;
	Head pluginelms;

	MessageHandler *poll_mh;
	unsigned poll_id_counter;
	Head polls;
	ES_Runtime *runtime;

	Head pluginobjs;

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	class ThreadState : public ES_ThreadListener
	{
	public:
		enum State
		{
			STATE_RUNNING,
			STATE_SUSPENDED,
			STATE_TERMINATED
		};

		ThreadState()
			: state(STATE_RUNNING)
			, origin(SUSPEND_NONE)
			, thread(NULL)
		{}

		void Suspend(ES_Thread *thread, SuspensionOrigin origin);
		OP_STATUS Resume();

		virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

		State state;
		SuspensionOrigin origin;
		ES_Thread *thread;
	};

	ThreadState thread_state;

public:
	/** Wrapper for information about JavaScript plug-ins. */
	class PluginElm : public Link
	{
	public:
		PluginElm() 
			: obj(NULL)
			, obj_opera(NULL)
			, cap(NULL)
#ifdef JS_PLUGIN_ATVEF_VISUAL
			, cap_ext(NULL)
#endif
		{}

		jsplugin_obj *obj;
		JS_Plugin_Object *obj_opera;
		jsplugin_cap *cap;
#ifdef JS_PLUGIN_ATVEF_VISUAL
		jsplugin_cap_ext *cap_ext;
#endif
	};

	JS_Plugin_Context(JS_Plugin_Manager *manager, ES_Runtime *runtime);
	~JS_Plugin_Context();

	/** Add a js plug-in allowed in this context.
	 * @param obj The global jsplugin object context to add.
	 * @param cap The plug-in's capability structure.
	 * @param cap_ext The plug-in's extended capability structure.
	 */
	OP_STATUS AddPlugin(JS_Plugin_Object *obj, jsplugin_cap *cap
#ifdef JS_PLUGIN_ATVEF_VISUAL
						, jsplugin_cap_ext *cap_ext
#endif
						);

	DEPRECATED(ES_GetState LookupName(const uni_char* property_name, ES_Value *value, BOOL *cacheable));	// Use GetName instead
	ES_GetState GetName(const uni_char* property_name, ES_Value *value, BOOL *cacheable);
	ES_PutState PutName(const uni_char* property_name, ES_Value *value);

	PluginElm *GetFirstPlugin();

	/**
	 * Check if Content-Type is handled by jsplugin. obj_p is set if TRUE
	 * is returned and obj_p is non-NULL.
	 * @param type Content-Type to check.
	 * @param obj_pp[out] Pointer to the JS_Plugin_Object handling this type.
	 * @return TRUE if there is an object handler for this Content-Type.
	 */
	BOOL HasObjectHandler(const uni_char *type, JS_Plugin_Object **obj_pp);

	ES_Runtime *GetRuntime() { return runtime; }

	OP_STATUS HandleObject(const uni_char *type, HTML_Element *element, ES_Runtime *runtime, EcmaScript_Object *global_object);

	OP_STATUS AddPoll(const jsplugin_obj *global_context, unsigned interval, jsplugin_poll_callback *callback);

	OP_STATUS ExportNativeObject(const jsplugin_obj *&eo, ES_Object *io);

	void AddPluginObject(JS_Plugin_Object *obj) { obj->Into(&pluginobjs); }
	void BeforeUnload();

	// Methods for handling suspend/resume

	/**
	 * Suspends the current ES thread by blocking it and records the origin of the suspension.
	 */
	void Suspend(SuspensionOrigin origin);
	/**
	 * Resumes a previously suspended ES thread.
	 */
	OP_STATUS Resume();
	/**
	 * Returns TRUE if the context is suspended, FALSE if running or terminated.
	 */
	BOOL IsSuspended();

	/**
	 * Returns the origin of the last suspension.
	 * For function calls it returns SUSPEND_CALL and for name lookups it returns SUSPEND_GET.
	 * @note The origin is kept until the suspended call or get is properly resumed.
	 *       A plugin that calls Resume() will not reset the type.
	 */
	SuspensionOrigin GetSuspensionOrigin();
	/**
	 * Reset the suspension origin to SUSPEND_NONE.
	 */
	void ResetSuspensionOrigin();
};

inline ES_GetState JS_Plugin_Context::LookupName(const uni_char* property_name, ES_Value *value, BOOL *cacheable) { return GetName(property_name, value, cacheable); }

#endif // JS_PLUGIN_SUPPORT
#endif // JS_PLUGIN_CONTEXT_H
