/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_ECMASCRIPT_H
#define SCOPE_ECMASCRIPT_H

#ifdef SCOPE_ECMASCRIPT

#include "modules/ecmascript_utils/esobjman.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_ecmascript_interface.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/esenginedebugger.h"
#include "modules/scope/scope_readystate_listener.h"

class OpScopeClientManager;

class ExamineResult;
class ExamineResultCallback
{
public:
	virtual OP_STATUS OnExamineResult(ExamineResult *result) = 0;
	/***< OnExamineResult is called from ExamineResult::OnObjectFinished to report when all properties on a object has been fetched */

};

class OpScopeEcmascript
	: public OpScopeEcmascript_SI
	, public ExamineResultCallback
{
	List<ExamineResult> examine_results;
public:
	OpScopeEcmascript();
	virtual ~OpScopeEcmascript();

	// OpScopeService
	virtual OP_STATUS OnServiceDisabled();

	void ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state);

	OP_STATUS EvalReply(unsigned tag, ES_Runtime *rt, ES_AsyncStatus status, const ES_Value &result);

	EvalResult::Status Convert(ES_AsyncStatus status) const;

	/**
	 * Convert from ReadyState type to ReadyStateChange::State type.
	 *
	 * @param [in] state_in The ReadyState to convert from.
	 * @param [out] state_out The ReadyStateChange::State type corresponding
	                          to the given input state.
	 * @return OpStatus::ERR_NOT_SUPPORTED in case there is no matching event
	 *         for the given state, OpStatus::OK otherwise.
	 */
	OP_STATUS Convert(OpScopeReadyStateListener::ReadyState state_in, ReadyStateChange::State &state_out) const;

	// ExamineResultCallback
	virtual OP_STATUS OnExamineResult(ExamineResult *result);

	// Request/Response functions
	virtual OP_STATUS DoListRuntimes(const ListRuntimesArg &in, RuntimeList &out);
	virtual OP_STATUS DoReleaseObjects(const ReleaseObjectsArg &in);
	virtual OP_STATUS DoSetFormElementValue(const SetFormElementValueArg &in);
	virtual OP_STATUS DoEval(const EvalArg &in, unsigned int async_tag);
	virtual OP_STATUS DoExamineObjects(const ExamineObjectsArg &in, unsigned int async_tag);

	// Exports ES_Object info to Object
	class ObjectHandler
		: public ESU_ObjectExporter::Handler
		, public ListElement<ObjectHandler>
	{
	public:
		ObjectHandler(ESU_ObjectManager &objman, ES_Runtime *runtime, ES_Object *obj, OpScopeEcmascript_SI::Object *out_object);

		~ObjectHandler();

		ES_Object *GetPrototype() const { return es_prototype; }
		ES_Object *GetObject() const { return es_object; }

		static OP_STATUS ExportValue(ESU_ObjectManager &objman, ES_Runtime *runtime, const ES_Value &value, OpScopeEcmascript_SI::Value &out);
		static OP_STATUS ExportPrimitiveValue(const ES_Value &value, OpScopeEcmascript_SI::Value &out);

		// Implemented from ESU_ObjectExporter::ObjectHandler
		virtual OP_STATUS Object(BOOL is_callable, const char *class_name, const uni_char *function_name, ES_Object *prototype);
		virtual OP_STATUS PropertyCount(unsigned);
		virtual OP_STATUS Property(const uni_char *, const ES_Value &);
		virtual void OnError();

		// Implemented from ES_PropertyHandler
		OP_STATUS OnPropertyValue(ES_DebugObjectProperties *properties, const uni_char *propertyname, const ES_Value &result, unsigned propertyindex, BOOL exclude, GetNativeStatus getstatus);

		void SetExamineResult(ExamineResult *result);
		/**< called by OpScopeEcmascript when IsFinished() returns FALSE */

		BOOL IsFinished();
		/**< return TRUE when all properties have been reported */

		void OnPropertyExcluded();
		/**< Called when a property was excluded  */

	private:
		ESU_ObjectManager &objman;
		ES_Runtime *runtime;
		ES_Object *es_object;
		ES_Object *es_prototype;
		OpScopeEcmascript_SI::Object *out_object;
		ExamineResult *result;
		unsigned remaining_properties;
	};

private:

	class EvalCallback
		: public ListElement<EvalCallback>
		, public ES_AsyncCallback
	{
	public:
		EvalCallback(OpScopeEcmascript *parent, ES_Runtime *rt, unsigned tag);
		virtual ~EvalCallback();
		virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);
		void Detach();
	private:
		OpScopeEcmascript *parent;
		ES_Runtime *rt;
		unsigned tag;
	};

	ESU_ObjectManager objman;
	ESU_RuntimeManager rtman;
	List<EvalCallback> callbacks;
};

/**
 * This is an ExamineObject result-holder kept in OpScopeEcmascript and used in 
 * the ExamineResultCallback when all object properties has been fetched.
 */
class ExamineResult : public ListElement<ExamineResult>
{
	OpScopeEcmascript::ObjectList *objects;			//< The object descriptions actually sent to the client.
	unsigned async_tag;								//< Asynchronous command tag.
	ExamineResultCallback *callback;				//< OnExamineResult receiver.
	List<OpScopeEcmascript::ObjectHandler> handlers;//< Property handlers.
	BOOL has_error;									//< Set to true if and error occurs, no result is sent to client.
public:
	ExamineResult(unsigned tag, ExamineResultCallback* finishcallback, OpScopeEcmascript::ObjectList *objs);
	~ExamineResult();

	void AddPropertyHandler(OpScopeEcmascript::ObjectHandler* handler);
	/**<  Add a property handler, will be removed when the property has been reported.*/
	unsigned GetAsyncTag() { return async_tag; }
	/**<  Get asynchronous command tag.*/
	OpScopeEcmascript::ObjectList *GetObjects() { return objects; }
	/**<  Asynchronous command tag.*/
	BOOL HasError() { return has_error; }
	/**<  The examine command has encountered an error.*/
	void SetHasError() { has_error = TRUE; }
	/**<  Set if an error has occurred.*/
	OP_STATUS OnObjectFinished(OpScopeEcmascript::ObjectHandler *handler);
	/**< Called when all properties on an object has been fetched.*/
	BOOL IsFinished() const;
	/**< Returns TRUE when all async properties has been evaled.*/
	OP_STATUS ExportProperties(ES_Runtime *rt);
	/**< Calls ESU_ObjectExporter::ExportObject() on all OpScopeEcmascript::ObjectHandler's in handlers.*/
	void Cancel();
	/**< Clears all ExamineResult pointers in handlers.*/
};

#endif // SCOPE_ECMASCRIPT
#endif // SCOPE_ECMASCRIPT_H
