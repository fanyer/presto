/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESDEBUGGER_H
#define ES_UTILS_ESDEBUGGER_H

#ifdef ECMASCRIPT_DEBUGGER

#include "modules/ecmascript/ecmascript.h"

class ES_Runtime;
class ES_Context;
class ES_AsyncInterface;
class ES_Object;
struct ES_ErrorInfo;
class ES_DebugScriptData;
class ES_DebugFrontend;
class ES_DebugBackend;
class ES_DebugThread;
class ES_DebugRuntime;
class ES_Thread;
class ES_ThreadScheduler;

class OpUINTPTRVector : public OpGenericVector
{
public:
	UINT32 GetCount() const
	{
		return OpGenericVector::GetCount();
	}

	UINTPTR Get(UINT32 idx) const
	{
		return (UINTPTR)OpGenericVector::Get(idx);
	}

	OP_STATUS Add(UINTPTR item)
	{
		return OpGenericVector::Add((void*) item);
	}
};

class ES_DebugTempContext
{
public:
	ES_DebugTempContext();
	~ES_DebugTempContext();
	OP_STATUS Construct(ES_Runtime *runtime);
	ES_Context *GetContext() const;
private:
	ES_Context *context;
	ES_Runtime *runtime;
};

class ES_DebugPosition
{
public:
	ES_DebugPosition();
	ES_DebugPosition(unsigned scriptid, unsigned linenr);

	BOOL operator==(const ES_DebugPosition &other) const;
	BOOL operator!=(const ES_DebugPosition &other) const;

	BOOL Valid() const;

	unsigned scriptid, linenr;
};

class ES_DebugObjectInfo;

class ES_DebugObject
{
public:
	unsigned id;
	ES_DebugObjectInfo *info;
};

class ES_DebugObjectInfo
{
public:
	ES_DebugObjectInfo();
	~ES_DebugObjectInfo();

	union
	{
		struct
		{
			unsigned is_function:1;
			unsigned is_callable:1;
			unsigned delete_class_name:1;
			unsigned delete_function_name:1;
		} packed;
		unsigned init;
	} flags;

	ES_DebugObject prototype;

	char *class_name;
	uni_char *function_name;
};

class ES_DebugValue
{
public:
	ES_DebugValue();
	~ES_DebugValue();

	OP_STATUS GetString(OpString& str) const;
	/**< Store the string represented by this value in the specified
	     OpString. May only be used when type is VALUE_STRING, or
	     VALUE_STRING_WITH_LENGTH.

	     @param [out] str The OpString to store the string in.
	     @return OpStatus::OK on success; OpStatus::ERR if called on a
	             ES_DebugValue with a non-string type; or
	             OpStatus::ERR_NO_MEMORY. */

	ES_Value_Type type;
	union
	{
		bool boolean;
		double number;
		struct
		{
			char *value;
			unsigned length;
		} string8;
		struct
		{
			uni_char *value;
			unsigned length;
		} string16;
		ES_DebugObject object;
	} value;
	BOOL is_8bit;
	BOOL owns_value;
};

class ES_DebugObjectProperties
{
public:
	ES_DebugObjectProperties();
	~ES_DebugObjectProperties();

	class Property
	{
	public:
		Property();
		~Property();

		uni_char *name;
		ES_DebugValue value;
		GetNativeStatus value_status;
	};

	ES_DebugObject object;
	unsigned properties_count;
	Property *properties;
};

/**
* Contains properties for the prototype chain of an object.
**/
class ES_DebugObjectChain
{
public:
	ES_DebugObjectChain();
	~ES_DebugObjectChain();

	ES_DebugObjectProperties prop;

	ES_DebugObjectChain *prototype;
	/**< NULL if no prototype. **/
};

class ES_DebugReturnValue
{
public:
	ES_DebugReturnValue();
	~ES_DebugReturnValue();

	ES_DebugObject function;
	ES_DebugValue value;

	ES_DebugPosition from;
	ES_DebugPosition to;

	ES_DebugReturnValue *next;
};

class ES_DebugStackFrame
{
public:
	ES_DebugStackFrame();
	~ES_DebugStackFrame();

	ES_DebugObject function;
	ES_DebugObject arguments;
	ES_DebugObject this_obj;
	unsigned variables;
	unsigned script_guid;
	unsigned line_no;
	unsigned *scope_chain;
	unsigned scope_chain_length;
};

class ES_DebugBreakpointData
{
public:
	enum Type { TYPE_POSITION = 1, TYPE_FUNCTION, TYPE_EVENT } type;
	union
	{
		struct
		{
			unsigned scriptid;
			unsigned linenr;
		} position;
		ES_Object *function;
		struct
		{
			const uni_char *namespace_uri;
			const uni_char *event_type;
		} event;
	} data;
	unsigned window_id; ///< Window ID the breakpoint should occur in or 0 for any window
};

class ES_DebugRuntimeInformation
{
public:
	ES_DebugRuntimeInformation()
		: dbg_runtime_id(0)
		, dbg_globalobject_id(0)
		, framepath(NULL)
		, documenturi(NULL)
		, description(NULL)
		, extension_name(NULL)
	{}
	~ES_DebugRuntimeInformation() { op_free(framepath); op_free(documenturi); op_free(extension_name); }

	unsigned dbg_runtime_id;
	unsigned dbg_globalobject_id;
	uni_char *framepath;
	uni_char *documenturi;
	const char *description;
	uni_char *extension_name;
	OpVector<Window> windows;
};

class ES_DebugVariable
{
public:
	const uni_char *name;
	ES_DebugValue value;
};

enum ES_DebugStopReason
{
	STOP_REASON_UNKNOWN,
	STOP_REASON_NEW_SCRIPT,
	STOP_REASON_EXCEPTION,
	STOP_REASON_ERROR,
	STOP_REASON_HANDLED_ERROR,
	STOP_REASON_ABORT,
	STOP_REASON_BREAK,
	STOP_REASON_DEBUGGER_STATEMENT,
	STOP_REASON_BREAKPOINT,
	STOP_REASON_STEP
};

class ES_DebugStoppedAtData
{
public:

	ES_DebugStoppedAtData()
		: reason(STOP_REASON_UNKNOWN){ }

	ES_DebugStopReason reason;
	/**< Why the thread stopped. */

	ES_DebugPosition position;
	/**< The source position where the thread stopped. */

	union
	{
		unsigned breakpoint_id;
		/**< The breakpoint ID, if STOP_REASON_BREAKPOINT. */

		ES_DebugValue *exception;
		/**< The thrown value, if STOP_REASON_EXCEPTION/ERROR. */
	} u;
};

enum StopType
{
	STOP_TYPE_SCRIPT,
	STOP_TYPE_EXCEPTION,
	STOP_TYPE_ERROR,
	STOP_TYPE_HANDLED_ERROR,
	STOP_TYPE_ABORT,
	STOP_TYPE_DEBUGGER_STATEMENT
};

class ES_DebugWindowAccepter
{
public:
	virtual ~ES_DebugWindowAccepter() { }

	virtual BOOL AcceptWindow(Window* window) = 0;
};

class ES_DebugPropertyFilters
{
public:
	virtual ~ES_DebugPropertyFilters(){}
	virtual ES_PropertyFilter *GetPropertyFilter(const char *classname) = 0;
};

class ES_DebugFrontend
{
public:
	enum Mode
	{
		RUN = 1,
		STEPINTO,
		STEPOVER,
		FINISH
	};

	enum ThreadType
	{
		THREAD_TYPE_INLINE = 1,
		THREAD_TYPE_EVENT,
		THREAD_TYPE_URL,
		THREAD_TYPE_TIMEOUT,
		THREAD_TYPE_DEBUGGER_EVAL,
		THREAD_TYPE_OTHER
	};

	enum ThreadStatus
	{
		THREAD_STATUS_FINISHED = 1,
		THREAD_STATUS_EXCEPTION,
		THREAD_STATUS_ABORTED,
		THREAD_STATUS_CANCELLED
	};

	enum EvalStatus
	{
		EVAL_STATUS_FINISHED = 1,
		EVAL_STATUS_EXCEPTION,
		EVAL_STATUS_ABORTED,
		EVAL_STATUS_CANCELLED
	};

	ES_DebugFrontend();
	virtual ~ES_DebugFrontend();

	virtual OP_STATUS RuntimeStarted(unsigned dbg_runtime_id, const ES_DebugRuntimeInformation* runtimeinfo) = 0;
	virtual OP_STATUS RuntimeStopped(unsigned dbg_runtime_id, const ES_DebugRuntimeInformation* runtimeinfo) = 0;
	virtual OP_STATUS RuntimesReply(unsigned tag, OpVector<ES_DebugRuntimeInformation>& runtimes) = 0;
	virtual OP_STATUS NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo = NULL) = 0;
	virtual OP_STATUS ParseError(unsigned dbg_runtime_id, ES_ErrorInfo *err) { return OpStatus::OK; }
	virtual OP_STATUS ThreadStarted(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo = NULL) = 0;
	virtual OP_STATUS ThreadFinished(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value) = 0;
	virtual OP_STATUS ThreadMigrated(unsigned dbg_thread_id, unsigned dbg_from_runtime_id, unsigned dbg_to_runtime_id) = 0;
	virtual OP_STATUS StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data) = 0;
	virtual OP_STATUS EvalReply(unsigned tag, EvalStatus status, const ES_DebugValue &result) = 0;
	virtual OP_STATUS ExamineObjectsReply(unsigned tag, unsigned object_count, ES_DebugObjectChain *result) = 0;
	virtual OP_STATUS EvalError(unsigned tag, const uni_char *message, unsigned line, unsigned offset) { return OpStatus::OK; }
	virtual OP_STATUS FunctionCallStarting(unsigned dbg_runtime_id, unsigned threadid, ES_Value* arguments, int argc, ES_Object* thisval, ES_Object* function, const ES_DebugPosition& position) { return OpStatus::OK; }
	virtual OP_STATUS FunctionCallCompleted(unsigned dbg_runtime_id, unsigned threadid, const ES_Value &returnvalue,  BOOL exception_thrown) { return OpStatus::OK; }

	OP_STATUS ConstructEngineBackend(ES_DebugWindowAccepter *accepter = 0);
	OP_STATUS ConstructRemoteBackend(BOOL active, const OpStringC &address, int port);

	OP_STATUS Detach();
	OP_STATUS Runtimes(unsigned tag, OpUINTPTRVector &runtimes, BOOL force_create_all = FALSE);
	OP_STATUS Continue(unsigned dbg_runtime_id, Mode mode);
	OP_STATUS Backtrace(ES_Runtime *runtime, ES_Context *context, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames);
	OP_STATUS Backtrace(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames);
	OP_STATUS Eval(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count, BOOL want_debugging=FALSE);
	OP_STATUS Examine(unsigned dbg_runtime_id, unsigned objects_count, unsigned *object_ids, BOOL examine_prototypes, BOOL skip_non_enum, ES_DebugPropertyFilters *filters, /* out */ ES_DebugObjectChain *&chains, unsigned int async_tag);
	/**< This function can extract additional information about objects, such as their class names, their properties, and so forth.

		 @param [in] dbg_runtime_id The ID of the runtime to which the objects belong.
		 @param [in] objects_count The number of objects in 'object_ids'.
		 @param [in] object_ids The IDs for the objects to be examined.
		 @param [in] examine_prototypes TRUE if the prototype chain should be examined. If FALSE, only the root object will be examined.
		 @param [in] skip_non_enum TRUE if non-enumerable properties should be excluded, or FALSE to include them.
		 @param [in] filters These filters will be applied to the properties, or NULL to disable filtering.
		 @param [out] chains On success, the examined chain(s) will be stored here (allocated by OP_NEWA). The number of chains will match 'objects_count'.
		 @param [in] owns_chains If TRUE chains will be deleted by the owning async property handler
		 @param [in] async_tag Asyncronous job id
		 @return OpStatus::OK if everything went well;
		         OpStatus::ERR, if nonexistent IDs were used; or
		         OpStatus::ERR_NO_MEMORY. */

	BOOL      HasBreakpoint(unsigned id);
	OP_STATUS AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data);
	OP_STATUS RemoveBreakpoint(unsigned id);
	OP_STATUS SetStopAt(StopType stop_type, BOOL value);
	OP_STATUS GetStopAt(StopType stop_type, BOOL &return_value);
	OP_STATUS Break(unsigned dbg_runtime_id, unsigned dbg_thread_id);
	void Prune();
	void FreeObject(unsigned object_id);
	void FreeObjects();

	ES_Object *GetObjectById(ES_Runtime *runtime, unsigned object_id);
	ES_Object *GetObjectById(unsigned object_id);
	ES_Runtime *GetRuntimeById(unsigned runtime_id);
	unsigned GetObjectId(ES_Runtime *rt, ES_Object *object);
	unsigned GetRuntimeId(ES_Runtime *runtime);
	unsigned GetThreadId(ES_Thread *thread);
	ES_DebugReturnValue *GetReturnValue(unsigned dbg_runtime_id, unsigned dbg_thread_id);
	OP_STATUS GetFunctionPosition(unsigned dbg_runtime_id, unsigned function_id, unsigned &script_guid, unsigned &line_no);
	/**< Retrieves script ID and line number for a native function object.
		 @param [in] dbg_runtime_id The ID of the runtime to which the function object belongs.
		 @param [in] function_id The ID of the object containing the native function to inspect.
		 @param [out] script_guid The ID of the script where the function was defined.
		 @param [out] line_no The line number where the function was defined.
		 @return OpStatus::OK if the position was retrieved,
		         OpStatus::ERR_NO_SUCH_RESOURCE if the object does not exist,
		         or OpStatus::ERR if the object is not a native function. */

	void ExportObject(ES_Runtime *rt, ES_Object *internal, ES_DebugObject &external, BOOL chain_info = TRUE);
	void ExportValue(ES_Runtime *rt, const ES_Value &internal, ES_DebugValue &external, BOOL chain_info = TRUE);

	void ResetFunctionFilter();
	OP_STATUS AddFunctionClassToFilter(const char* classname);

	void SetReformatFlag(BOOL enable);
	OP_STATUS SetReformatConditionFunction(const uni_char *source);

private:
	ES_DebugBackend *dbg_backend;
};

class ES_DebugBackend
{
public:
	ES_DebugBackend();
	virtual ~ES_DebugBackend();

	virtual OP_STATUS Detach() = 0;

	virtual OP_STATUS Runtimes(unsigned tag, OpUINTPTRVector &runtimes, BOOL force_create_all = FALSE) = 0;
	virtual OP_STATUS Continue(unsigned dbg_runtime_id, ES_DebugFrontend::Mode new_mode) = 0;
	virtual OP_STATUS Backtrace(ES_Runtime *runtime, ES_Context *context, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames) = 0;
	virtual OP_STATUS Backtrace(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames, unsigned &length, ES_DebugStackFrame *&frames) = 0;
	virtual OP_STATUS Eval(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count, BOOL want_debugging) = 0;
	virtual OP_STATUS Examine(unsigned dbg_runtime_id, unsigned objects_count, unsigned *object_ids, BOOL examine_prototypes, BOOL skip_non_enum, ES_DebugPropertyFilters *filters, /* out */ ES_DebugObjectChain *&chains, unsigned int async_tag) = 0;
	virtual BOOL      HasBreakpoint(unsigned id) = 0;
	virtual OP_STATUS AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data) = 0;
	virtual OP_STATUS RemoveBreakpoint(unsigned id) = 0;
	virtual OP_STATUS SetStopAt(StopType stop_type, BOOL value) = 0;
	virtual OP_STATUS GetStopAt(StopType stop_type, BOOL &return_value) = 0;
	virtual OP_STATUS Break(unsigned dbg_runtime_id, unsigned dbg_thread_id) = 0;
	virtual void Prune() = 0;
	virtual void FreeObject(unsigned object_id) = 0;
	virtual void FreeObjects() = 0;

	virtual ES_Object *GetObjectById(ES_Runtime *runtime, unsigned object_id) = 0;
	virtual ES_Object *GetObjectById(unsigned object_id) = 0;
	virtual ES_Runtime *GetRuntimeById(unsigned runtime_id) = 0;
	virtual unsigned GetObjectId(ES_Runtime *runtime, ES_Object *object) = 0;
	virtual unsigned GetRuntimeId(ES_Runtime *runtime) = 0;
	virtual unsigned GetThreadId(ES_Thread *thread) = 0;
	virtual ES_DebugReturnValue *GetReturnValue(unsigned dbg_runtime_id, unsigned dbg_thread_id) = 0;
	virtual OP_STATUS GetFunctionPosition(unsigned dbg_runtime_id, unsigned function_id, unsigned &script_guid, unsigned &line_no) = 0;
	/**< Retrieves script ID and line number for a native function object.
		 @param [in] dbg_runtime_id The ID of the runtime to which the function object belongs.
		 @param [in] function_id The ID of the object containing the native function to inspect.
		 @param [out] script_guid The ID of the script where the function was defined.
		 @param [out] line_no The line number where the function was defined.
		 @return OpStatus::OK if the position was retrieved,
		         OpStatus::ERR_NO_SUCH_RESOURCE if the object does not exist,
		         or OpStatus::ERR if the object is not a native function. */
	virtual void ExportObject(ES_Runtime *rt, ES_Object *internal, ES_DebugObject &external, BOOL chain_info = TRUE) = 0;
	virtual void ExportValue(ES_Runtime *rt, const ES_Value &internal, ES_DebugValue &external, BOOL chain_info = TRUE) = 0;

	virtual void ResetFunctionFilter() = 0;
	virtual OP_STATUS AddFunctionClassToFilter(const char* classname) = 0;

	virtual void SetReformatFlag(BOOL enable) = 0;
	virtual OP_STATUS SetReformatConditionFunction(const uni_char *source) = 0;

protected:
	ES_DebugFrontend *frontend;
};

#endif /* ECMASCRIPT_DEBUGGER */
#endif /* ES_UTILS_ESDEBUGGER_H */
