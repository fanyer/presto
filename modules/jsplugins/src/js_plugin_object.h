/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JS_PLUGIN_OBJECT_H
#define JS_PLUGIN_OBJECT_H

#ifdef JS_PLUGIN_SUPPORT

#include "modules/jsplugins/jsplugin.h"
#include "modules/jsplugins/src/js_plugin_version.h"
#include "modules/jsplugins/src/js_plugin_manager.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/windowcommander/OpTv.h"

class JS_Plugin_Context;
class HTML_Element;

#define ES_HOSTOBJECT_JSPLUGIN_HTML		(ES_HOSTOBJECT_JSPLUGIN + 1);
#define ES_HOSTOBJECT_JSPLUGIN_EVAL		(ES_HOSTOBJECT_JSPLUGIN + 2);
#define ES_HOSTOBJECT_JSPLUGIN_NATIVE	(ES_HOSTOBJECT_JSPLUGIN + 3);

class JS_Plugin_Native
	: public EcmaScript_Object
{
public:
	static OP_STATUS
	Make(JS_Plugin_Native *&new_obj, JS_Plugin_Context *context, ES_Object* obj);
	
	virtual
	~JS_Plugin_Native() {};

	void
	GCTrace() { GetRuntime()->GCMarkSafe(obj); }

	virtual BOOL
	IsA(int tag) { return tag == ES_HOSTOBJECT_JSPLUGIN_NATIVE; }

	jsplugin_obj *
	GetRepresentation() { return &self; }

	ES_Object *
	GetShadow() { return obj; }

private:
	JS_Plugin_Native(ES_Object *obj) : obj(obj) { self.opera_private = static_cast<EcmaScript_Object *>(this); self.plugin_private = NULL; }

	ES_Object *obj;
	jsplugin_obj self;
};

class JS_Plugin_Object
	: public EcmaScript_Object
	, public Link
#ifdef JS_PLUGIN_ATVEF_VISUAL
	, public OpTvWindowListener
#endif
{
protected:
	JS_Plugin_Context			*context;
	jsplugin_obj				self;
	jsplugin_getter				*getname;
	jsplugin_setter				*putname;
	jsplugin_function			*call;
	jsplugin_function			*construct;
	jsplugin_destructor			*destruct;
	jsplugin_gc_trace			*trace;

    jsplugin_attr_change_listener *attr_change_listener;
	jsplugin_param_set_listener	  *param_set_listener;
#ifdef JS_PLUGIN_ATVEF_VISUAL
	jsplugin_tv_become_visible	*tv_become_visible;		///< TV callback for visibility.
	jsplugin_tv_position		*tv_position;			///< TV callback for positioning.
	OpString					 tv_url;				///< TV URL to listen to.
#endif
	Head						unload_listeners;
	Head						eval_elms;

	class UnloadListenerElm : public Link
	{
	public:
		UnloadListenerElm(jsplugin_notify *listener) : listener(listener) {};

		jsplugin_notify *listener;
	};

	int
	CallOrConstruct(jsplugin_function *f, ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

public:
	JS_Plugin_Object(JS_Plugin_Context *context);

	virtual
	~JS_Plugin_Object();

	void
	GCTrace();

	void
	BeforeUnload();

	OP_STATUS
	AddUnloadListener(jsplugin_notify *listener);

	OP_STATUS
	SetAttrChangeListener(jsplugin_attr_change_listener *listener);

	void AttributeChanged(const uni_char *name, const uni_char *value);

	OP_STATUS
	SetParamSetListener(jsplugin_param_set_listener *listener);

	void ParamSet(const uni_char *name, const uni_char *value);

	void
	SetCallbacks(jsplugin_getter *g, jsplugin_setter *s, jsplugin_function *c,
				 jsplugin_function *ctor, jsplugin_destructor *dtor, jsplugin_gc_trace *t);

#ifdef JS_PLUGIN_ATVEF_VISUAL
	/** Register this jsplugin's callbacks for TV objects. If they are
	  * non-null, this jsplugin will be given an ATVEF visual representation.
	  * @param become_visible Callback for visibility changes.
	  * @param position Callback for positioning changes.
	  */
	void
	SetTvCallbacks(jsplugin_tv_become_visible *become_visible,
				   jsplugin_tv_position *position);
#endif

	JS_Plugin_Context *
	GetContext() { return context; }

	JS_Plugin_Context *
	GetOriginingContext(ES_Runtime* origining_runtime);

	jsplugin_obj *
	GetRepresentation() { return &self; }

	BOOL
	SecurityCheck(ES_Runtime* origining_runtime);

	static OP_STATUS
	ExportNativeObject(JS_Plugin_Context *context, jsplugin_obj *&eo, ES_Object *io);

	static OP_STATUS
	ExportObject(JS_Plugin_Context *context, jsplugin_obj *&eo, ES_Object *io);

	static OP_STATUS
	ExportString(char **es, int *es_len, const uni_char *is, unsigned is_len);

	static OP_STATUS
	Export(JS_Plugin_Context *context, jsplugin_value *ev, const ES_Value *iv);

	OP_STATUS
	ImportExpression(ES_Value *iv, jsplugin_value *ev);

	static OP_STATUS
	ImportString(uni_char **is, unsigned *is_len,  const char *es, int es_len, BOOL temp = TRUE);

	OP_STATUS
	Import(ES_Value *iv, jsplugin_value *ev, BOOL temp = TRUE);

	static void
	FreeValue(jsplugin_value *ev);

	static void
	FreeValue(ES_Value *ev);

	ES_GetState
	GetName(const uni_char* property_name, ES_Value* value, BOOL *cacheable, ES_Runtime* origining_runtime, ES_Object* restart_object = NULL);

	virtual ES_GetState
	GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState
	GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_PutState
	PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_GetState
	GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);

	virtual ES_PutState
	PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);

	virtual int
	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

	virtual int
	Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);

	virtual BOOL
	IsObject() { return FALSE; }

	virtual BOOL
	IsA(int tag) { return tag == ES_HOSTOBJECT_JSPLUGIN; }

#ifdef JS_PLUGIN_ATVEF_VISUAL
	/** Is this plug-in an ATVEF visual plug-in? */
	BOOL
	IsAtvefVisualPlugin() { return tv_become_visible != NULL && tv_position != NULL; }

	/** Register as TV listener for a specific URL.
	  * @param url URL used in callbacks destined for this object.
	  * @return Status of the operation. ERR_NULL_POINTER if there are
	  * no TV callbacks in the plugin.
	  */
	OP_STATUS
	RegisterAsTvListener(const uni_char *url);

	// Inherited from OpTvWindowListener
	virtual void
	SetDisplayRect(const uni_char *url, const OpRect &rect);

	virtual void
	OnTvWindowAvailable(const uni_char *url, BOOL available);
#ifdef MEDIA_SUPPORT
	// Ignore the media-specific ATVEF variant, ATVEF is used here
	// only to give overlay capabilities to jsplugins.
	virtual void
	SetDisplayRect(OpMediaHandle handle, const OpRect& rect, const OpRect &clipRect) {}

	virtual void
	OnTvWindowAvailable(OpMediaHandle handle, BOOL available) {}
#endif // MEDIA_SUPPORT
#endif // JS_PLUGIN_ATVEF_VISUAL
};

class JS_Plugin_HTMLObjectElement_Object
	: public JS_Plugin_Object
{
protected:
	HTML_Element *element;
	jsplugin_notify *inserted;
	jsplugin_notify *removed;
	BOOL is_inserted;

public:
	JS_Plugin_HTMLObjectElement_Object(JS_Plugin_Context *context, HTML_Element *element)
		: JS_Plugin_Object(context), element(element), inserted(NULL), removed(NULL), is_inserted(FALSE)
	{
	}

	virtual BOOL
	IsObject() { return TRUE; }

	void
	SetCallbacks(jsplugin_getter *g, jsplugin_setter *s, jsplugin_function *c,
				 jsplugin_function *ctor, jsplugin_destructor *dtor, jsplugin_gc_trace *t,
				 jsplugin_notify *inserted, jsplugin_notify *removed);

	void
	Inserted();

	void
	Removed();

	HTML_Element *
	GetElement() { return element; }

	virtual BOOL
	IsA(int tag) { return tag == ES_HOSTOBJECT_JSPLUGIN || tag == ES_HOSTOBJECT_JSPLUGIN_HTML; }
};

class JS_Eval_Elm
	: public Link
	, public ES_AsyncCallback
	, public EcmaScript_Object
{
public:
	static OP_STATUS
	Make(JS_Eval_Elm *&new_obj, JS_Plugin_Context *context, const uni_char* expression);
	
	OP_STATUS
	Init();

	OP_STATUS
	HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	void
	Import(ES_Value *iv);

	virtual BOOL
	IsA(int tag) { return tag == ES_HOSTOBJECT_JSPLUGIN || tag == ES_HOSTOBJECT_JSPLUGIN_EVAL; }

private:
	JS_Eval_Elm(JS_Plugin_Context *context, const uni_char *expression);
	
	virtual
	~JS_Eval_Elm() { Out(); }

	ES_Value value;
	JS_Plugin_Context *context;
	const uni_char *expression;
};

#endif // JS_PLUGIN_SUPPORT
#endif // JS_PLUGIN_OBJECT_H
