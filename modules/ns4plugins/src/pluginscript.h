/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PLUGINSCRIPT_H
#define PLUGINSCRIPT_H

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/plugincommon.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/opstring.h"

class PluginRestartObject;

class OpNPIdentifier
{
protected:
	OpString name;

	BOOL is_string;
	OpString8 name_utf8;
	int name_int;

	OpNPIdentifier();

	void CheckInteger();

public:
	~OpNPIdentifier();

	static OpNPIdentifier *Make(const char *name);
	static OpNPIdentifier *Make(const uni_char *name);
	static OpNPIdentifier *Make(int name);

	const uni_char *GetName() { return name.CStr(); }

	BOOL IsString() { return is_string; }
	const char *GetNameUTF8() { return name_utf8.CStr(); }
	int GetNameInt() { return name_int; }
};

class PluginScriptData;

/** An OpNPObject is a binding between an internal ES_Object
 * and a corresponding external NPObject.
 *
 * This binding is uniquely tied to one plug-in instance, and
 * comes in two flavors:
 *
 * 1. A binding produced by exporting an ES object to a plug-in
 *    instance. In this case the internal object is protected for
 *    the lifetime of the binding.
 *
 * 2. A binding produced by a plug-in instance creating an NPObject
 *    through NPN_CreateObject. In this case, the internal object
 *    is unset until the external value is imported into the ES
 *    engine. The internal value is not protected, and may become
 *    unset again if collected.
 *
 * The boolean member is_pure_internal classifies the two types,
 * where TRUE indicates type 1 and FALSE indicates type 2.
 */
class OpNPObject
	: public ListElement<OpNPObject>
{
protected:
	OpNPObject(Plugin* plugin, ES_Object* internal, NPObject* external);

	ES_ObjectReference reference;

	BOOL Protect(ES_Runtime *runtime, ES_Object *object);
	void Unprotect();

	ES_Object *internal;
	NPObject *external;

	BOOL is_pure_internal;

	BOOL has_exception;
	OpString exception;

	Plugin* plugin;

public:
	~OpNPObject();

	/** Create a binding between a pure internal ES_Object and an external
	 * NPObject. Intended for exporting internal objects.
	 *
	 * @param plugin The plug-in instance to which this binding belongs.
	 * @param runtime ES runtime.
	 * @param internal The internal object of this binding. Must be non-NULL.
	 *
	 * @return A binding with external object set to a new NPObject, or NULL on OOM.
	 */
	static OpNPObject *Make(Plugin* plugin, ES_Runtime* runtime, ES_Object *internal);

	/** Create an open binding to an externally created NPObject. Intended
	 * for importing external objects.
	 *
	 * @param plugin The plug-in instance to which this binding belongs.
	 * @param external The external object of this binding. Must be non-NULL.
	 *
	 * @return A binding with internal object unset, or NULL on OOM.
	 */
	static OpNPObject *Make(Plugin* plugin, NPObject *external);

	/** Increment the reference count of the bound NPObject. */
	void Retain();

	/** Decrement the reference count of the bound NPObject. The binding
	 * will be destroyed if the reference count hits zero.
	 *
	 * @param soft If TRUE, the reference count will not be permitted to
	 * drop below one.
	 *
	 * The 'soft' argument is only set TRUE when releasing return values
	 * from function calls to the plug-in, such that returning a value
	 * will never destroy it directly. Note that the cases where the soft
	 * argument makes a difference all occur immediately after importing the
	 * object into the ecmascript environment, ensuring that the object will
	 * not linger or leak, but be destroyed by the ecmascript garbage
	 * collector when the final reference is gone.
	 *
	 * The argument was introduced to be compatible with Firefox 4.
	 */
	void Release(BOOL soft = FALSE);

	/** Get the internal ES_Object of this binding.
	 *
	 * @return ES_Object or NULL if none exist.
	 */
	ES_Object* GetInternal() { return internal; }

	/** Get the external NPObject of this binding.
	 *
	 * @return NPObject, never NULL.
	 */
	NPObject* GetExternal() const { return external; }

	/** Get the plug-in owning this binding.
	 *
	 * @return Plug-in instance, never NULL.
	 */
	Plugin* GetPlugin() const { return plugin; }

	/** Check exception.
	 *
	 * @return TRUE if the NPObject has an exception set.
	 */
	BOOL HasException() const { return has_exception; }

	/** Set exception status.
	 *
	 * @param set TRUE if exception set.
	 */
	void SetHasException(BOOL set) { has_exception = set; }

	/** Get the exception set.
	 *
	 * @return Exception string.
	 */
	const OpString& GetException() const { return exception; }

	/** Set an exception. */
	OP_STATUS SetException(const OpString& string) { return exception.Set(string); }

	/** Classify binding.
	 *
	 * @return TRUE if pure internal (type 1.)
	 */
	BOOL IsPureInternal() const { return is_pure_internal; }

	/** Set internal ES_Object NULL. */
	void UnsetInternal();

	/** Import external NPObject. The resulting internal ES_Object
	 * will not be protected. If the external value has already been
	 * imported, the existing internal value will remain unchanged.
	 *
	 * @param runtime ES runtime.
	 *
	 * @return ES_Object created or NULL on OOM.
	 */
	ES_Object* Import(ES_Runtime* runtime);

	/** @defgroup Helper methods for calling NPClass methods of external object
	 * directly, without involving Ecmascript in the process.
	 *
	 * @param ... Method specific arguments. For documentation on those
	 * @See https://developer.mozilla.org/en/NPClass
	 *
	 * @return OP_BOOLEAN value returns OpStatus::ERR when external object does
	 * not implement given method or 'this' object is 'pure internal'. Otherwise
	 * IS_TRUE or IS_FALSE value reflecting bool value returned from method call.
	 *
	 * NOTE: 'This' object may be deallocated during execution of any of these methods.
	 *
	 * @{
	 */
	OP_BOOLEAN HasMethod(NPIdentifier method_name);
	OP_BOOLEAN Invoke(NPIdentifier method_name, const NPVariant* args, uint32_t arg_count, NPVariant* result);
	OP_BOOLEAN InvokeDefault(const NPVariant* args, uint32_t arg_count, NPVariant* result);
	OP_BOOLEAN HasProperty(NPIdentifier property_name);
	OP_BOOLEAN GetProperty(NPIdentifier property_name, NPVariant* result);
	OP_BOOLEAN SetProperty(NPIdentifier property_name, const NPVariant* value);
	OP_BOOLEAN RemoveProperty(NPIdentifier property_name);
	OP_BOOLEAN Construct(const NPVariant *args, uint32_t arg_count, NPVariant* result);
	/** @} */
};

class OpNPExternalMethod;

enum Ns4PluginsESTypes
{
	NS4_TYPE_EXTERNAL_OBJECT = ES_HOSTOBJECT_NS4PLUGIN,
	NS4_TYPE_RESTART_OBJECT
};

/** An OpNPExternalObject represents an externally created NPObject.
 *
 * Its ES_Object may be the internal value of several OpNPObjects, where
 * exactly one is a type 2 binding. When the ES_Object is garbage collected,
 * the internal value of the type 2 binding will be unset. Note that since
 * type 1 bindings protect the ES_Object, the type 2 binding will be the
 * last remaining OpNPObject referencing the ES_Object.
 */
class OpNPExternalObject
	: public EcmaScript_Object
{
protected:
	OpNPObject *npobject;
	OpNPExternalMethod *methods;
	uni_char**	m_enum_array;
	uint32_t	m_enum_count;

	/* InitEnum() checks if the plugin's enumerate function is available, does initial lookup of npobject's
	 * enumeration values and stores the values in m_enum_array and m_enum_count for later use by the
	 * ES engine when enumerating the plugin object via FetchPropertiesL().
	 * The values are deallocated by ~OpNPExternalObject().
	 *
	 * @return status value
	 */
	OP_STATUS	InitEnum();

public:
	OpNPExternalObject(OpNPObject *npobject);
	~OpNPExternalObject();

	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetNameRestart(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object);
	virtual ES_GetState GetIndex(int property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexRestart(int property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object);

	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object);
	virtual ES_PutState PutIndex(int property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndexRestart(int property_name, ES_Value *value, ES_Runtime *origining_runtime, ES_Object *restart_object);

	virtual ES_DeleteStatus DeleteName(const uni_char* property_name, ES_Runtime* origining_runtime);
	virtual ES_DeleteStatus DeleteIndex(int property_index, ES_Runtime* origining_runtime);

	/* Registers the enumerable named and indexed properties of the npobject,
	 * determined by the plugin's NPClass enumerate function. See base class
	 * documentation, EcmaScript_Object::FetchPropertiesL().
	 *
	 * @return result of operation, as an ES_GetState.
	 */
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	/* Returns the number of indexed properties in the object.  Must not modify the object's native part.
	 * @param count[out] denotes the count of indexed properties.
	 *
	 * @return status value.
	 */
	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	/* Indicates whether the object is a true plugin function or a plugin object that happens to be callable.
	 * Since a true function is always implemented by OpNPExternalMethod, which inherits directly from
	 * EcmaScript_Object, it will by design never be affected by OpNPExternalMethod::TypeofYieldsObject().
	 * A true plugin function is by definition not an OpNPExternalObject.
	 *
	 * @return value is always TRUE and indicates an object.
	 */
	virtual BOOL		TypeofYieldsObject();

	virtual int Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);
	virtual int Construct(ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == NS4_TYPE_EXTERNAL_OBJECT; }
	virtual void GCTrace();

	OpNPExternalMethod *GetMethod(OpNPIdentifier *name);

	/** Inform this object and its OpNPExternalMethod children that their OpNPObject has been invalidated. */
	void MarkNPObjectAsDeleted();
};

class OpNPExternalMethod
	: public EcmaScript_Object
{
protected:
	OpNPObject *npobject;
	OpNPIdentifier *name;
	OpNPExternalMethod *next;

	OpNPExternalMethod();

public:
	static OpNPExternalMethod *Make(ES_Runtime *runtime, OpNPObject *this_object, OpNPIdentifier *name, OpNPExternalMethod *next);

	virtual int Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime);

	virtual void GCTrace();

	OpNPIdentifier *GetMethodName() { return name; }
	OpNPExternalMethod *GetNext() { return next; }

	void MarkNPObjectAsDeleted() { npobject = NULL; }
};

class PluginScriptData
{
protected:
	OpAutoStringHashTable<OpNPIdentifier> string_identifiers; ///< const uni_char* => OpNPIdentifier
	OpAutoINT32HashTable<OpNPIdentifier> int_identifiers;    ///< int => OpNPIdentifier
	OpVector<PluginRestartObject> restart_objects;
	OpPointerHashTable<NPObject, OpNPObject> external_objects;    ///< NPObject* => OpNPObject*
	BOOL m_allow_nested_message_loop;

public:
	PluginScriptData();
	~PluginScriptData();

	OpNPIdentifier *GetStringIdentifier(const char *name);
	OpNPIdentifier *GetStringIdentifier(const uni_char *name);
	OpNPIdentifier *GetIntIdentifier(int name);

	void SetAllowNestedMessageLoop(BOOL b) { m_allow_nested_message_loop = b; }
	BOOL AllowNestedMessageLoop() { return m_allow_nested_message_loop; }

	OP_STATUS AddPluginRestartObject(PluginRestartObject* restart_object);
	void RemovePluginRestartObject(PluginRestartObject* restart_object);
	void ReleaseObjectFromRestartObjects(OpNPObject* object);

	/** Add NPObject -> OpNPObject binding.
	 *
	 * @param object OpNPObject to register.
	 *
	 * @return OpStatus::Ok or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AddObject(OpNPObject* object);

	/** Remove NPObject -> OpNPObject binding.
	 *
	 * @param object OpNPObject to unregister.
	 */
	void RemoveObject(OpNPObject* object);

	/** Look up OpNPObject based on its external NPObject.
	 *
	 * @param external External NPObject.
	 *
	 * @return Uniquely matching OpNPObject or NULL if none exist.
	 */
	OpNPObject* FindObject(const NPObject* external);
};

BOOL SynchronouslyEvaluateJavascriptURL(Plugin *plugin, FramesDocument *frames_doc, const uni_char *script, NPVariant& result);
void PluginReleaseExternalValue(NPVariant &value, BOOL soft = FALSE);
OP_STATUS GetLocation(FramesDocument* doc, NPVariant*& result);

class PluginRestartObject
	: public EcmaScript_Object,
	  public ES_ThreadListener,
	  public MessageObject
{
public:
	enum Type { PLUGIN_UNINITIALIZED, PLUGIN_HASPROPERTY, PLUGIN_GET, PLUGIN_PUT, PLUGIN_DELETE, PLUGIN_CALL, PLUGIN_RESTART, PLUGIN_RESTART_AFTER_INIT, PLUGIN_CONSTRUCT } type;
	ES_Thread *thread;
	MessageHandler *mh;
	OpNPObject *object;
	OpNPIdentifier *identifier;
	ES_Value value;
	NPVariant *argv;
	int argc;
	BOOL success, oom, called;

	PluginRestartObject* next_restart_object;

	PluginRestartObject();
	virtual ~PluginRestartObject();
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 p1, MH_PARAM_2 p2);
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);
	virtual void GCTrace();
	virtual BOOL IsA(int tag) { return tag == NS4_TYPE_RESTART_OBJECT; }
	/**
	 * @param for_direct_use If TRUE, then it is assumed the restart object will be used directly, no as a
	 *                       "real" restart object. If FALSE, then the created object will be sent back in
	 *                       an async MSG_PLUGIN_ECMASCRIPT_RESTART message.
	 */
	static OP_STATUS Make(PluginRestartObject *&restart_object, ES_Runtime *runtime, OpNPObject *object, BOOL for_direct_use);
	static OP_STATUS Suspend(PluginRestartObject *&restart_object, ES_Runtime *runtime);
	void Resume();
};

#endif // _PLUGIN_SUPPORT_
#endif // !PLUGINSCRIPT_H
