/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
 * Copyright (C) 2002-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESENGINEDEBUGGER_H
#define ES_UTILS_ESENGINEDEBUGGER_H

#ifdef ECMASCRIPT_ENGINE_DEBUGGER

#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esobjman.h"
#include "modules/ecmascript_utils/esasyncif.h"

class ES_DebugObjectMap;
class ES_DebugThread;
class ES_DebugEvalCallback;


class ES_EngineDebugBackendGetPropertyListener : public ES_PropertyHandler
{
private:
	unsigned m_root_object_count;
	/**< Expected number of objects. */
	unsigned m_remaining_objects;
	/**< Object counter. */
	unsigned m_tag;
	/**< Scope async tag index. */
	unsigned m_runtime_id;
	/**< Runtime ID of ES_Runtime owned by debugger. */
	ES_DebugObjectChain *m_objects;
	/**< Root objects with properties. */
	ES_DebugObjectProperties *m_proto_object_properties;
	/**< Root object properties. */
	ES_DebugFrontend* m_frontend;
	/**< Debugger frontend instance. */
	Head m_object_properties;
	/**< List of property handlers, ES_ObjectPropertiesListener. */
	BOOL m_export_chain_info;
	/**< Flag to ES_EngineDebugBackend::ExportObject. */
	BOOL m_sync_mode;
	/**< When reporting properties synchronously remaining objects can be untrue. */
	ES_Thread* m_blocked_thread;
	/**< If the thread is blocked by a debugger statement or breakpoint. */
	unsigned m_unreported_objects;
	/**< Number of objects where properties was set synchronously. */
protected:
	ES_EngineDebugBackend* m_backend;
	/**< Debugger backend instance. */
public:

	ES_EngineDebugBackendGetPropertyListener(ES_EngineDebugBackend *backend, ES_DebugFrontend *frontend, unsigned runtimeid, unsigned asynctag, ES_Thread *blocked_thread);
	virtual ~ES_EngineDebugBackendGetPropertyListener();

	void Done();

	void ObjectPropertiesDone();

	unsigned GetNumberOfUnreportedObjects() { return m_unreported_objects; }

	BOOL HasReportedAllProperties() { return m_remaining_objects == 0; }

	// ES_PropertyHandler
	virtual OP_STATUS OnPropertyValue(ES_DebugObjectProperties *props, const uni_char *propertyname, const ES_Value &result, unsigned propertyindex, BOOL exclude, GetNativeStatus getstatus)
	{
		return GetObjectPropertyListener(props)->OnPropertyValue(result, propertyindex, propertyname, m_export_chain_info, exclude, getstatus);
	}
	virtual void OnDebugObjectChainCreated(ES_DebugObjectChain *targetstruct, unsigned count);
	virtual OP_STATUS AddObjectPropertyListener(ES_Object *owner, unsigned property_count, ES_DebugObjectProperties* properties, BOOL chain_info);
	virtual ES_Thread* GetBlockedThread()
	{
		return m_blocked_thread;
	}
	void SetSynchronousMode()
	{
		m_sync_mode = TRUE;
	}
	void SetAsynchronousMode()
	{
		m_sync_mode = FALSE;
	}
	void OnError()
	{
	}

	/** ES_EngineDebugBackendGetPropertyListener keep one ES_ObjectPropertiesListener per ES_Object to handle property fetching. */
	class ES_ObjectPropertiesListener : public Link
	{
		ES_EngineDebugBackendGetPropertyListener *m_owner_listener;
		unsigned m_property_count, m_remaining_properties;
		unsigned m_property_index_alignment;
		/**< Used to align arrayindex for filtered, skipped, properties. This ensures that the property array is packed and does not have empty slots. */
	protected:
		ES_DebugObjectProperties *m_properties;
		/**< Gets mapped to ES_EngineDebugBackendGetPropertyListener::m_objects. */
	public:
		ES_Object* m_owner_object;
		/**< Owner object of m_properties. */

		ES_ObjectPropertiesListener(ES_EngineDebugBackendGetPropertyListener *listener, ES_Object *owner, ES_DebugObjectProperties *properties, unsigned count)
			: m_owner_listener(listener),
			  m_property_count(count),
			  m_remaining_properties(count),
			  m_property_index_alignment(0),
			  m_properties(properties),
			  m_owner_object(owner)
			{ }

		~ES_ObjectPropertiesListener()
		{
			Out();
		}

		OP_STATUS OnPropertyValue(const ES_Value &getslotresult, unsigned property_index, const uni_char *propertyname, BOOL chain_info, BOOL exclude, GetNativeStatus getstatus);

		// Link
		ES_ObjectPropertiesListener *Suc() { return static_cast<ES_ObjectPropertiesListener *>(Link::Suc()); }
		friend class ES_EngineDebugBackendGetPropertyListener;
	};

	/** Find the propertyhandler via props */
	ES_ObjectPropertiesListener* GetObjectPropertyListener(ES_DebugObjectProperties *props)
	{
		ES_ObjectPropertiesListener* listener = static_cast<ES_ObjectPropertiesListener *>(m_object_properties.First());
		while (listener)
		{
			if (listener->m_properties == props)
				break;
			listener = listener->Suc();
		}
		return listener;
	}

	friend class ES_ObjectPropertiesListener;
};

/**< Class to signal Object GetSlot() results. */
class ES_GetSlotHandler : public ES_AsyncCallback
{
	ES_Object			*m_owner;
	ES_PropertyHandler	*m_callback;
	unsigned			m_index;
	OpString			m_name;
	ES_PropertyFilter *m_filter;
	ES_DebugObjectProperties *m_properties;
	ES_GetSlotHandler(ES_Object* owner, ES_PropertyHandler* listener, ES_PropertyFilter *filter, unsigned index, ES_DebugObjectProperties *properties)
		: m_owner(owner),
		  m_callback(listener),
		  m_index(index),
		  m_filter(filter),
		  m_properties(properties)
	{
	}

public:

	/**
	  * Create a ES_GetSlotHandler
	  *
	  * @param getslothandler [out]
	  * @param owner [in] object that has the property
	  * @param listener [in] listener class
	  * @param filter [in] property filter
	  * @param index [in] property index
	  * @param name [in] property name
	  * @param properties [in] optional, if no properties are stored this is not needed. Callbacks (ES_PropertyHandler::OnPropertyValue) will also receive this.
	  * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if
	  *         getslothandler could not be created or failed during initialization.
	  */
	static OP_STATUS Make(ES_GetSlotHandler *&getslothandler, ES_Object* owner, ES_PropertyHandler* listener, ES_PropertyFilter *filter, unsigned index, const uni_char* name, ES_DebugObjectProperties *properties)
	{
		getslothandler = OP_NEW(ES_GetSlotHandler, (owner, listener, filter, index, properties));
		if (!getslothandler)
			return OpStatus::ERR_NO_MEMORY;
		if (OpStatus::IsError(getslothandler->m_callback->GetSlotHandlerCreated(getslothandler)) ||
			OpStatus::IsError(getslothandler->m_name.Set(name)))
		{
			OP_DELETE(getslothandler);
			getslothandler = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}

	~ES_GetSlotHandler()
	{
		if (m_callback)
			m_callback->GetSlotHandlerDied(this);
		m_callback = NULL;
	}

	void OnListenerDied()
	{
		m_callback = NULL;
	}

	  //ES_AsyncCallback
	OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
	{
		if (m_callback)
		{
			if (status == ES_ASYNC_SUCCESS)
			{
				BOOL exclude = (m_filter && m_filter->Exclude(m_name.CStr(), result));
				m_callback->OnPropertyValue(m_properties, m_name.CStr(), result, m_index, exclude, GETNATIVE_SUCCESS);
			}
			else if (status == ES_ASYNC_FAILURE)
				m_callback->OnError();
		}
		OP_DELETE(this);
		return OpStatus::OK;
	}
};

class ES_EngineDebugBackend
	: public ES_DebugBackend,
	  public ES_DebugListener
{
public:
	ES_EngineDebugBackend();
	virtual ~ES_EngineDebugBackend();

	OP_STATUS Construct(ES_DebugFrontend *frontend, ES_DebugWindowAccepter *accepter);

	void ImportValue(ES_Runtime *runtime, const ES_DebugValue &external, ES_Value &internal);
	void ThreadFinished(ES_DebugThread *dbg_thread, ES_ThreadSignal signal);

	OP_STATUS ThreadMigrated(ES_Thread *thread, ES_Runtime *from, ES_Runtime *to);

	OP_STATUS RemoveObjectListener(ES_EngineDebugBackendGetPropertyListener*p){ return current_propertylisteners.RemoveByItem(p); }

	/* From ES_DebugBackend. */
	virtual OP_STATUS Detach();
	virtual void Prune();
	virtual void FreeObject(unsigned object_id);
	virtual void FreeObjects();

	virtual OP_STATUS Runtimes(unsigned dbg_runtime_id, OpUINTPTRVector &runtime_ids, BOOL force_create_all = FALSE);
	virtual OP_STATUS Continue(unsigned dbg_runtime_id, ES_DebugFrontend::Mode new_mode);
	virtual OP_STATUS Backtrace(ES_Runtime *runtime, ES_Context *context, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames);
	virtual OP_STATUS Backtrace(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames);
	virtual OP_STATUS Eval(unsigned id, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count, BOOL want_debugging);
	virtual OP_STATUS Examine(unsigned dbg_runtime_id, unsigned objects_count, unsigned *object_ids, BOOL examine_prototypes, BOOL skip_non_enum, ES_DebugPropertyFilters *filters, /* out */ ES_DebugObjectChain *&chains, unsigned int async_tag);
	virtual BOOL      HasBreakpoint(unsigned id);
	virtual OP_STATUS AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data);
	virtual OP_STATUS RemoveBreakpoint(unsigned tag);
	virtual OP_STATUS SetStopAt(StopType stop_type, BOOL value);
	virtual OP_STATUS GetStopAt(StopType stop_type, BOOL &return_value);
	virtual OP_STATUS Break(unsigned dbg_runtime_id, unsigned dbg_thread_id);
	virtual OP_STATUS GetFunctionPosition(unsigned dbg_runtime_id, unsigned function_id, unsigned &script_guid, unsigned &line_no);

	void ResetFunctionFilter() { m_function_filter.DeleteAll(); }
	OP_STATUS AddFunctionClassToFilter(const char* classname);

	virtual void SetReformatFlag(BOOL enable);
	virtual OP_STATUS SetReformatConditionFunction(const uni_char *script);

	virtual ES_Object *GetObjectById(ES_Runtime *runtime, unsigned object_id);
	virtual ES_Object *GetObjectById(unsigned object_id);
	virtual ES_Runtime *GetRuntimeById(unsigned runtime_id);
	virtual unsigned GetObjectId(ES_Runtime *runtime, ES_Object *object);
	virtual unsigned GetRuntimeId(ES_Runtime *runtime);
	virtual unsigned GetThreadId(ES_Thread *thread);
	virtual ES_DebugReturnValue *GetReturnValue(unsigned dbg_runtime_id, unsigned dbg_thread_id);
	virtual void ExportObject(ES_Runtime *rt, ES_Object *internal, ES_DebugObject &external, BOOL chain_info = TRUE);
	virtual void ExportValue(ES_Runtime *rt, const ES_Value &internal, ES_DebugValue &external, BOOL chain_info = TRUE);

	/* From ES_DebugListener. */
	virtual BOOL EnableDebugging(ES_Runtime *runtime);
	virtual void NewScript(ES_Runtime *runtime, ES_DebugScriptData *data);
	virtual void ParseError(ES_Runtime *runtime, ES_ErrorInfo *err);
	virtual void NewContext(ES_Runtime *runtime, ES_Context *context);
	virtual void EnterContext(ES_Runtime *runtime, ES_Context *context);
	virtual void LeaveContext(ES_Runtime *runtime, ES_Context *context);
	virtual void DestroyContext(ES_Runtime *runtime, ES_Context *context);
	virtual void DestroyRuntime(ES_Runtime *runtime);
	virtual void DestroyObject(ES_Object *object);
	virtual EventAction HandleEvent(EventType type, unsigned int script_guid, unsigned int line_no);
	virtual void RuntimeCreated(ES_Runtime *runtime);

private:
	Head dbg_runtimes, dbg_windows, dbg_breakpoints;
	Head eval_callbacks;
	ESU_ObjectManager objman;
	ES_DebugThread *current_dbg_thread;
	ES_DebugWindowAccepter *accepter;
	OpAutoVector<ES_EngineDebugBackendGetPropertyListener> current_propertylisteners;

	ES_Runtime *common_runtime;
	/**< We merge all debugged runtimes into this one, to avoid problems with invalid
	     object references. */

	BOOL stop_at_script, stop_at_exception, stop_at_error, stop_at_handled_error, stop_at_abort, include_objectinfo;
	unsigned next_dbg_runtime_id, next_dbg_thread_id;
	unsigned break_dbg_runtime_id, break_dbg_thread_id;

	class FunctionFilterHash : public OpGenericString8HashTable
	{
	public:
		FunctionFilterHash() : OpGenericString8HashTable(TRUE)  { }
		virtual void Delete(void* data) { OP_DELETEA((char*)data); }
	};

	FunctionFilterHash m_function_filter;	// only report functions with class from this hash

	EventAction Stop(const ES_DebugStoppedAtData &data);
	EventAction Stop(ES_DebugStopReason reason, const ES_DebugPosition &position);
	EventAction StopBreakpoint(const ES_DebugPosition &position, unsigned breakpoint_id);
	EventAction StopException(ES_DebugStopReason reason, const ES_DebugPosition &position, ES_DebugValue &exception);

	OP_STATUS GetDebugRuntime(ES_DebugRuntime *&dbg_runtime, ES_Runtime *runtime, BOOL create);
	ES_DebugRuntime *GetDebugRuntimeById(unsigned id);

	OP_STATUS GetDebugThread(ES_DebugThread *&dbg_thread, ES_Thread *thread, BOOL create);
	ES_DebugThread *GetDebugThreadById(ES_DebugRuntime *dbg_runtime, unsigned id);
	OP_STATUS SignalNewThread(ES_DebugThread *dbg_thread);

	ES_Object *GetCurrentFunction();
	OP_STATUS GetRuntimeInformation(ES_DebugRuntime *dbg_runtime, ES_DebugRuntimeInformation &info, TempBuffer &buffer);
	OP_STATUS GetWindowId(Window *window, unsigned &id);

	OP_STATUS GetScope(ES_DebugRuntime *dbg_runtime, unsigned dbg_thread_id, unsigned frame_index, unsigned *length, ES_Object ***scope);
	OP_STATUS ExamineChain(ES_Runtime *rt, ES_Context *context, ES_Object *root, BOOL skip_non_enum, BOOL examine_prototypes, ES_DebugObjectChain &chain, ES_DebugPropertyFilters *filters, ES_EngineDebugBackendGetPropertyListener *listener, unsigned int async_tag);
	OP_STATUS ExamineObject(ES_Runtime *rt, ES_Context *context, ES_Object *object, BOOL skip_non_enum, BOOL chain_info, ES_DebugObjectProperties &properties, ES_PropertyFilter *filter, ES_EngineDebugBackendGetPropertyListener *listener, unsigned int async_tag);

	ES_AsyncInterface *GetAsyncInterface(ES_DebugRuntime *dbg_runtime);
	ES_Thread *GetCurrentThread(ES_DebugRuntime *dbg_runtime);

	OP_STATUS AddDebugRuntime(OpAutoVector<ES_DebugRuntimeInformation> &runtimes, ES_DebugRuntime *debug_runtime, TempBuffer &buffer);

	unsigned GetWindowId(ES_Runtime* runtime);
	BOOL AcceptRuntime(ES_Runtime *runtime); //< Decide whether to accept runtime for debugging
	void AddRuntime(ES_DebugRuntime *dbg_runtime);

	void DetachEvalCallback(ES_DebugEvalCallback *c);
	OP_STATUS Detach(ES_DebugRuntime *dbg_runtime);
	/**< Remove a callback from 'eval_callbacks'. */

	void CancelEvalThreads(ES_Runtime *runtime = NULL);
	/**< Cancel (and delete) active Eval threads for a certain runtime.

	     @param runtime The runtime to cancel threads for. NULL to cancel
	                    ALL eval threads across runtimes. */

	BOOL ReportFunctionCallComplete(ES_Value& returnvalue, BOOL isexception);

	friend class ES_DebugEvalCallback; // Needed for method DetachEvalCallback
};

#endif /* ECMASCRIPT_ENGINE_DEBUGGER */
#endif /* ES_UTILS_ESENGINEDEBUGGER_H */
