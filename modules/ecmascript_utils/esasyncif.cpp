/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esutils.h"

#include "modules/doc/frm_doc.h"

static OP_STATUS
SetupSlotOperation(ES_Runtime *runtime, ES_Object *object, const ES_Value &slot, const ES_Value *value, ES_Object *&scope)
{
	EcmaScript_Object *scope_object;

	if (!(scope_object = OP_NEW(EcmaScript_Object, ())))
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(scope_object->SetObjectRuntime(runtime, runtime->GetObjectPrototype(), "Object")))
	{
		OP_DELETE(scope_object);
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(scope_object->Put(UNI_L("x"), object));
	RETURN_IF_ERROR(scope_object->Put(UNI_L("y"), slot));

	if (value)
		RETURN_IF_ERROR(scope_object->Put(UNI_L("z"), *value));

	scope = *scope_object;

	return OpStatus::OK;
}

static void
FreeArguments(ES_Runtime *runtime, ES_Value *arguments, int count)
{
	for (int index = 0; index < count; ++index)
		if (arguments[index].type == VALUE_STRING)
			op_free((void *) arguments[index].value.string);
		else if (arguments[index].type == VALUE_OBJECT)
			runtime->Unprotect(arguments[index].value.object);
}

class ES_AsyncInterface_ThreadListener
	: public ES_ThreadListener
{
private:
	ES_AsyncOperation operation;
	ES_AsyncCallback *callback;

public:
	ES_AsyncInterface_ThreadListener(ES_AsyncOperation operation, ES_AsyncCallback *callback)
		: operation(operation), callback(callback)
	{
	}

	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		OP_STATUS status = OpStatus::OK;
		OpStatus::Ignore(status);

		if (callback)
		{
			ES_AsyncStatus async_status;
			ES_Value value;
			value.type = VALUE_UNDEFINED;

			switch (signal)
			{
			case ES_SIGNAL_FINISHED:
				if (!thread->IsFailed())
				{
					if (thread->ReturnedValue())
						status = thread->GetReturnedValue(&value);

					async_status = ES_ASYNC_SUCCESS;
				}
				else
					async_status = ES_ASYNC_FAILURE;
				break;

			case ES_SIGNAL_FAILED:
				if (thread->GetScheduler()->GetLastError() == OpStatus::ERR_NO_MEMORY)
					async_status = ES_ASYNC_NO_MEMORY;
				else if (thread->ReturnedValue())
				{
					status = thread->GetReturnedValue(&value);
					async_status = ES_ASYNC_EXCEPTION;
				}
				else
					async_status = ES_ASYNC_FAILURE;
				break;

			case ES_SIGNAL_CANCELLED:
				async_status = ES_ASYNC_CANCELLED;
				break;

			default:
				return OpStatus::OK;
			}

			if (OpStatus::IsMemoryError(status))
				async_status = ES_ASYNC_NO_MEMORY;
			else if (OpStatus::IsError(status))
				async_status = ES_ASYNC_FAILURE;

			if (OpStatus::IsMemoryError(callback->HandleCallback(operation, async_status, value)))
				status = OpStatus::ERR_NO_MEMORY;
		}

		return status;
	}
};

class ES_AsyncInterface_CallMethod_ThreadListener
	: public ES_ThreadListener
{
private:
	ES_Runtime *runtime;
	ES_Object *object;
	ES_AsyncCallback *callback;
	int argc;
	ES_Value *argv;
	BOOL want_exceptions;
	BOOL want_string_result;
	BOOL is_user_requested;
	BOOL open_in_new_window;
	BOOL open_in_background;
	BOOL is_plugin_requested;

public:
	ES_AsyncInterface_CallMethod_ThreadListener(ES_Runtime *runtime, ES_Object *object, int argc, ES_Value *argv, ES_AsyncCallback *callback, BOOL want_exceptions, BOOL want_string_result, BOOL is_user_requested, BOOL open_in_new_window, BOOL open_in_background, BOOL is_plugin_requested)
		: runtime(runtime),
		  object(object),
		  callback(callback),
		  argc(argc),
		  argv(argv),
		  want_exceptions(want_exceptions),
		  want_string_result(want_string_result),
		  is_user_requested(is_user_requested),
		  open_in_new_window(open_in_new_window),
		  open_in_background(open_in_background),
		  is_plugin_requested(is_plugin_requested)
	{
	}

	~ES_AsyncInterface_CallMethod_ThreadListener()
	{
		FreeArguments(runtime, argv, argc);
		OP_DELETEA(argv);
	}

	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		ES_ThreadScheduler *scheduler = thread->GetScheduler();
		ES_Runtime *runtime = scheduler->GetRuntime();
		ES_Value value;

		OP_STATUS status = OpStatus::OK;
		OpStatus::Ignore(status);

		ES_AsyncStatus async_status = ES_ASYNC_FAILURE;

		switch (signal)
		{
		case ES_SIGNAL_FINISHED:
			if (!thread->IsFailed() && thread->ReturnedValue())
			{
				status = thread->GetReturnedValue(&value);

				if (OpStatus::IsSuccess(status) && value.type == VALUE_OBJECT)
				{
					ES_Context *context = runtime->CreateContext(object);

					ES_Thread *new_thread = OP_NEW(ES_Thread, (context, thread ? thread->GetSharedInfo() : NULL));
					ES_ThreadListener *listener = NULL;

					if (new_thread && new_thread->GetSharedInfo() && callback)
					{
						listener = OP_NEW(ES_AsyncInterface_ThreadListener, (ES_ASYNC_CALL_METHOD, callback));

						if (listener)
							new_thread->AddListener(listener);
					}

					if (!context || !new_thread || !new_thread->GetSharedInfo() || (callback && !listener) || OpStatus::IsError(status = runtime->PushCall(context, value.value.object, argc, argv)))
					{
						if (status == OpStatus::OK)
							status = OpStatus::ERR_NO_MEMORY;

						if (new_thread)
							OP_DELETE(new_thread);
						else if (context)
							runtime->DeleteContext(context);
					}
					else
					{
						if (want_exceptions)
							new_thread->SetWantExceptions();

						if (want_string_result)
							new_thread->SetWantStringResult();

						if (is_user_requested)
							new_thread->SetIsUserRequested();

						if (open_in_new_window)
							new_thread->SetOpenInNewWindow();

						if (open_in_background)
							new_thread->SetOpenInBackground();

						if (is_plugin_requested)
							new_thread->SetIsPluginThread();

						OP_BOOLEAN result = scheduler->AddRunnable(new_thread, thread);

						if (result == OpBoolean::IS_TRUE)
							return OpStatus::OK;
						else if (result == OpBoolean::IS_FALSE)
							async_status = ES_ASYNC_CANCELLED;
						else
							status = result;
					}
				}
			}
			break;

		case ES_SIGNAL_FAILED:
			if (scheduler->GetLastError() == OpStatus::ERR_NO_MEMORY)
				async_status = ES_ASYNC_NO_MEMORY;
			else if (thread->ReturnedValue())
			{
				status = thread->GetReturnedValue(&value);
				async_status = ES_ASYNC_EXCEPTION;
			}
			else
				async_status = ES_ASYNC_FAILURE;
			break;

		case ES_SIGNAL_CANCELLED:
			async_status = ES_ASYNC_CANCELLED;
			break;

		default:
			return OpStatus::OK;
		}

		if (OpStatus::IsMemoryError(status))
			async_status = ES_ASYNC_NO_MEMORY;
		else if (OpStatus::IsError(status))
			async_status = ES_ASYNC_FAILURE;

		if (callback)
			callback->HandleCallback(ES_ASYNC_CALL_METHOD, async_status, value);

		return status;
	}
};

ES_AsyncInterface::ES_AsyncInterface(ES_Runtime *runtime, ES_ThreadScheduler *scheduler)
	: runtime(runtime),
	  scheduler(scheduler),
	  override_listener(NULL),
	  last_thread(NULL)
{
	Reset();
}

OP_STATUS
ES_AsyncInterface::Eval(const uni_char *text, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	return Eval(text, NULL, 0, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::Eval(const uni_char *text, ES_Object **scope_chain, int scope_chain_length,
                        ES_AsyncCallback *callback, ES_Thread *interrupt_thread,
                        ES_Object *this_object)
{
	if (!runtime || !scheduler || !runtime->Enabled())
		/* ECMAScript disabled or not available. */
		return OpStatus::ERR;

	ES_ProgramText program_array;
	program_array.program_text = text;
	program_array.program_text_length = uni_strlen(text);

	return Eval(&program_array, 1, scope_chain, scope_chain_length, callback, interrupt_thread, this_object);
}

static OP_STATUS
ES_AsyncInterface_ErrorCallback(const uni_char *message, const uni_char *full_message, const ES_SourcePosition &position, void *user_data)
{
	ES_AsyncCallback *callback = static_cast<ES_AsyncCallback *>(user_data);

	// Relative and absolute position information should be equal for evals.
	return callback->HandleError(message, position.GetRelativeLine(), position.GetRelativeIndex());
}

OP_STATUS
ES_AsyncInterface::Eval(ES_ProgramText *program_array, int program_array_length,
	                    ES_Object **scope_chain, int scope_chain_length,
	                    ES_AsyncCallback *callback, ES_Thread *interrupt_thread,
	                    ES_Object *this_object)
{
	OP_STATUS status;
	ES_Program *program;

	BOOL wanted_exceptions = want_exceptions;
	BOOL wanted_string_result = want_string_result;
	BOOL was_user_requested = is_user_requested;
	BOOL was_open_in_new_window = open_in_new_window;
	BOOL was_open_in_background = open_in_background;
	BOOL wanted_debugging = want_debugging;
	BOOL wanted_reformat_source = want_reformat_source;
	unsigned wanted_start_line = start_line;
	BOOL was_plugin_requested = is_plugin_requested;

	Reset();

	ES_Runtime::CompileProgramOptions options;
	options.program_is_function = TRUE;
	options.generate_result = TRUE;
	options.global_scope = scope_chain_length == 0;
	options.prevent_debugging = !wanted_debugging;
	options.is_eval = TRUE;
	options.allow_top_level_function_expr = TRUE;
	options.reformat_source = wanted_reformat_source;
	options.start_line = wanted_start_line;

	if (callback)
	{
		options.error_callback = &ES_AsyncInterface_ErrorCallback;
		options.error_callback_user_data = callback;
	}

	if (interrupt_thread)
		scheduler->SetErrorHandlerInterruptThread(interrupt_thread);

	/* If debugging of the script is requested set the type to 'eval'.
	   Otherwise it is a script type that is internal to the debugger. */
	options.script_type = wanted_debugging ? SCRIPT_TYPE_EVAL : SCRIPT_TYPE_DEBUGGER;

	status = runtime->CompileProgram(program_array, program_array_length, &program, options);

	if (interrupt_thread)
		scheduler->ResetErrorHandlerInterruptThread();

	if (OpStatus::IsSuccess(status))
	{
		if (program)
		{
			ES_Context *context;

			if ((context = runtime->CreateContext(this_object, !wanted_debugging)) != NULL)
			{
				if (OpStatus::IsSuccess(status = runtime->PushProgram(context, program, scope_chain, scope_chain_length)))
				{
					ES_Thread *thread;

					if(wanted_debugging)
						thread = OP_NEW(ES_DebuggerEvalThread, (context, interrupt_thread ? interrupt_thread->GetSharedInfo() : NULL));
					else
						thread = OP_NEW(ES_Thread, (context, interrupt_thread ? interrupt_thread->GetSharedInfo() : NULL));

					if (thread)
					{
						thread->SetProgram(program, FALSE);

						if (callback)
						{
							ES_ThreadListener *listener = OP_NEW(ES_AsyncInterface_ThreadListener, (ES_ASYNC_EVAL, callback));

							if (listener)
								thread->AddListener(listener);
							else
							{
								OP_DELETE(thread);
								return OpStatus::ERR_NO_MEMORY;
							}
						}
						else if (override_listener)
						{
							thread->AddListener(override_listener);
							override_listener = NULL;
						}

						if (wanted_exceptions)
							thread->SetWantExceptions();

						if (wanted_string_result)
							thread->SetWantStringResult();

						if (was_user_requested)
							thread->SetIsUserRequested();

						if (was_open_in_new_window)
							thread->SetOpenInNewWindow();

						if (was_open_in_background)
							thread->SetOpenInBackground();

						if (was_plugin_requested)
							thread->SetIsPluginThread();

						OP_BOOLEAN result = scheduler->AddRunnable(thread, interrupt_thread);

						if (OpStatus::IsError(result))
							return result;
						else if (result == OpBoolean::IS_FALSE)
							return OpStatus::ERR;
						else
						{
							last_thread = thread;
							return OpStatus::OK;
						}
					}
					else
						status = OpStatus::ERR_NO_MEMORY;
				}

				ES_Runtime::DeleteContext(context);
			}
			else
				status = OpStatus::ERR_NO_MEMORY;

			ES_Runtime::DeleteProgram(program);
		}
		else
			status = OpStatus::ERR;
	}

	if (override_listener)
	{
		OP_DELETE(override_listener);
		override_listener = NULL;
	}

	return status;
}

OP_STATUS
ES_AsyncInterface::CallFunction(ES_Object *function, ES_Object *this_object, int argc, const ES_Value *argv,
                                ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	if (!this_object)
		this_object = (ES_Object *) runtime->GetGlobalObject();

	ES_Context *context = runtime->CreateContext(this_object);
	ES_Thread *thread = OP_NEW(ES_Thread, (context, interrupt_thread ? interrupt_thread->GetSharedInfo() : NULL));
	ES_ThreadListener *listener = NULL;

	BOOL wanted_exceptions = want_exceptions;
	BOOL wanted_string_result = want_string_result;
	BOOL was_user_requested = is_user_requested;
	BOOL was_open_in_new_window = open_in_new_window;
	BOOL was_open_in_background = open_in_background;
	BOOL was_plugin_requested = is_plugin_requested;

	Reset();

	if (thread && callback)
	{
		listener = OP_NEW(ES_AsyncInterface_ThreadListener, (ES_ASYNC_CALL_FUNCTION, callback));

		if (listener)
			thread->AddListener(listener);
	}

	if (!context || !thread || !thread->GetSharedInfo() || (callback && !listener) || OpStatus::IsMemoryError(runtime->PushCall(context, function, argc, argv)))
	{
		if (thread)
			OP_DELETE(thread);
		else if (context)
			runtime->DeleteContext(context);

		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		if (wanted_exceptions)
			thread->SetWantExceptions();

		if (wanted_string_result)
			thread->SetWantStringResult();

		if (was_user_requested)
			thread->SetIsUserRequested();

		if (was_open_in_new_window)
			thread->SetOpenInNewWindow();

		if (was_open_in_background)
			thread->SetOpenInBackground();

		if (was_plugin_requested)
			thread->SetIsPluginThread();

		OP_BOOLEAN result = scheduler->AddRunnable(thread, interrupt_thread);

		if (result == OpBoolean::IS_TRUE)
		{
			last_thread = thread;
			return OpStatus::OK;
		}
		else if (result == OpBoolean::IS_FALSE)
			return OpStatus::ERR;
		else
			return result;
	}
}

OP_STATUS
ES_AsyncInterface::CallMethod(ES_Object *object, const uni_char *method, int argc, const ES_Value *argv,
                              ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_STRING;
	slot_value.value.string = method;

	RETURN_IF_ERROR(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0]));

	ES_Value *argv_copy = OP_NEWA(ES_Value, argc);
	if (!argv_copy)
		goto oom;

	int index;

	for (index = 0; index < argc; ++index)
	{
		argv_copy[index] = argv[index];

		if (argv_copy[index].type == VALUE_STRING)
		{
			if (!(argv_copy[index].value.string = uni_strdup(argv_copy[index].value.string)))
				break;
		}
		else if (argv_copy[index].type == VALUE_OBJECT)
		{
			if (!runtime->Protect(argv_copy[index].value.object))
				break;
		}
	}

	if (index < argc)
	{
		if (index > 1)
			FreeArguments(runtime, argv_copy, index - 1);
		OP_DELETEA(argv_copy);
		goto oom;
	}

	override_listener = OP_NEW(ES_AsyncInterface_CallMethod_ThreadListener, (runtime, object, argc, argv_copy, callback, want_exceptions, want_string_result, is_user_requested, open_in_new_window, open_in_background, is_plugin_requested));
	if (!override_listener)
	{
		FreeArguments(runtime, argv_copy, argc);
		OP_DELETEA(argv_copy);
		goto oom;
	}

	/* This evaluation fetches the function to call.  We don't want it
	   converted to a string. */
	want_string_result = FALSE;

	return Eval(UNI_L("x[y]"), scope_chain, 1, NULL, interrupt_thread);

 oom:
	Reset();
	return OpStatus::ERR_NO_MEMORY;
}

#ifdef ESUTILS_ASYNCIF_PROPERTIES_SUPPORT

OP_STATUS
ES_AsyncInterface::GetSlot(ES_Object *object, const uni_char *slot, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_STRING;
	slot_value.value.string = slot;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("x[y]"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::GetSlot(ES_Object *object, int index, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_NUMBER;
	slot_value.value.number = index;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("x[y]"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::SetSlot(ES_Object *object, const uni_char *slot, const ES_Value &value, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_STRING;
	slot_value.value.string = slot;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, &value, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("void(x[y]=z)"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::SetSlot(ES_Object *object, int index, const ES_Value &value, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_NUMBER;
	slot_value.value.number = index;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, &value, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("void(x[y]=z)"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::RemoveSlot(ES_Object *object, const uni_char *slot, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_STRING;
	slot_value.value.string = slot;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("delete x[y]"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::RemoveSlot(ES_Object *object, int index, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_NUMBER;
	slot_value.value.number = index;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("delete x[y]"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::HasSlot(ES_Object *object, const uni_char *slot, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_STRING;
	slot_value.value.string = slot;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("y in x"), scope_chain, 1, callback, interrupt_thread);
}

OP_STATUS
ES_AsyncInterface::HasSlot(ES_Object *object, int index, ES_AsyncCallback *callback, ES_Thread *interrupt_thread)
{
	ES_Value slot_value;
	ES_Object *scope_chain[1];

	slot_value.type = VALUE_NUMBER;
	slot_value.value.number = index;

	if (OpStatus::IsMemoryError(SetupSlotOperation(runtime, object, slot_value, NULL, scope_chain[0])))
	{
		Reset();
		return OpStatus::ERR_NO_MEMORY;
	}

	return Eval(UNI_L("String(y) in x"), scope_chain, 1, callback, interrupt_thread);
}
#endif // ESUTILS_ASYNCIF_PROPERTIES_SUPPORT

void
ES_AsyncInterface::SetWantExceptions()
{
	want_exceptions = TRUE;
}

void
ES_AsyncInterface::SetWantDebugging()
{
	want_debugging = TRUE;
}

void
ES_AsyncInterface::SetWantReformatSource()
{
	want_reformat_source = TRUE;
}

void
ES_AsyncInterface::SetWantStringResult()
{
	want_string_result = TRUE;
}

ES_Thread *
ES_AsyncInterface::GetLastStartedThread()
{
	return last_thread;
}

void
ES_AsyncInterface::Reset()
{
	want_exceptions = FALSE;
	want_string_result = FALSE;
	is_user_requested = FALSE;
	open_in_new_window = FALSE;
	open_in_background = FALSE;
	want_debugging = FALSE;
	want_reformat_source = FALSE;
	start_line = 1;
	is_plugin_requested = FALSE;
}

