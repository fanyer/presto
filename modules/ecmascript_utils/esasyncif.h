/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESASYNCIF_H
#define ES_UTILS_ESASYNCIF_H

#include "modules/ecmascript/ecmascript.h"

class ES_ThreadScheduler;
class ES_Thread;
class ES_ThreadListener;

enum ES_AsyncOperation
{
	ES_ASYNC_EVAL,
	ES_ASYNC_CALL_FUNCTION,
	ES_ASYNC_CALL_METHOD,

#ifdef ESUTILS_ASYNCIF_PROPERTIES_SUPPORT
	ES_ASYNC_GET_SLOT,
	ES_ASYNC_SET_SLOT,
#endif // ESUTILS_ASYNCIF_PROPERTIES_SUPPORT

	ES_ASYNC_LAST_OPERATION
};

enum ES_AsyncStatus
{
	ES_ASYNC_SUCCESS,
	ES_ASYNC_FAILURE,
	ES_ASYNC_EXCEPTION,
	ES_ASYNC_NO_MEMORY,
	ES_ASYNC_CANCELLED
};

class ES_AsyncCallback
{
public:
	virtual ~ES_AsyncCallback() {}
	virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result) = 0;
	/**< Called after an operation has been completed, whether successfully
	     or not.

	     @param operation The type of operation that was completed.
	     @param status Exit status of the operation.
	     @param result If 'operation' is not ES_ASYNC_SET_SLOT and 'status'
	                   is ES_ASYNC_SUCCESS, the operation's result value.
	                   If 'status' is ES_ASYNC_EXCEPTION, the exception.
	                   Otherwise the value is undefined.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	    */
	virtual OP_STATUS HandleError(const uni_char *message, unsigned line, unsigned offset) { return OpStatus::OK; }
	/**< Called if the eval operation failed due to parse errors.

	     @param message One-line error message.
	     @param line Line on which error occurred.
	     @param offset Character offset into whole source (not line)
	                   where error occurred.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	    */
};

class ES_AsyncInterface
{
public:
	ES_AsyncInterface(ES_Runtime *runtime, ES_ThreadScheduler *scheduler);

	OP_STATUS Eval(const uni_char *text, ES_AsyncCallback *callback = NULL,
	               ES_Thread *interrupt_thread = NULL);
	/**< Evaluate the ES code in 'text'.  The optional callback will be
	     called iff this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_EVAL if you use this function.

	     @param text A string containing some ES code.
	     @param callback Optional callback object. Eval calls
	     HandleError on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation (e.g. an error
	             compiling 'text').
		*/

	OP_STATUS Eval(const uni_char *text, ES_Object **scope_chain, int scope_chain_length,
	               ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL,
	               ES_Object* this_object = NULL);
	/**< Evaluate the ES code in 'text', with the objects in
	     'scope_chain' as the scope (last object is the nearest scope).
	     The optional callback will be called iff this function returns
	     OpStatus::OK.

	     Import API_ESUTILS_ASYNC_EVAL if you use this function.

	     @param text A string containing some ES code.
	     @param callback Optional callback object. Eval calls
	     HandleError on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param scope_chain Scope chain.
	     @param scope_chain_length Length of scope chain.
	     @param interrupt_thread Optional thread to interrupt.
	     @param this_object Optional 'this' object for the evaluation

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation (e.g. an error
	             compiling 'text').
	    */

	OP_STATUS Eval(ES_ProgramText *program_array, int program_array_length,
	               ES_Object **scope_chain, int scope_chain_length,
	               ES_AsyncCallback *callback, ES_Thread *interrupt_thread,
	               ES_Object* this_object = NULL);
	/**< Evaluate the ES code represented by the array of fragments in the
	     'program_array' argument.  The optional callback will be called
	     iff this functions returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_EVAL if you use this function.

	     @param program_array List of fragments.
	     @param program_array_length Length of 'program_array'.
	     @param scope_chain Scope chain.
	     @param scope_chain_length Length of scope chain.
	     @param callback Optional callback object. Eval calls
	     HandleError on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.
	     @param this_object Optional 'this' object for the evaluation

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS CallFunction(ES_Object *function, ES_Object *this_object, int argc, const ES_Value *argv,
	                       ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL);
	/**< Call the function 'function' with this object 'this_object' and the
	     'argc' first values in 'argv' as arguments.  The optional callback
	     will be called iff this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_CALLFUNCTION if you use this function.

	     @param function An ES object (should be a function).
	     @param this_object An ES object (can be NULL in which case the
	                        global object is used.)
	     @param argc Number of arguments in 'argv'.
	     @param argv Arguments (can be NULL if 'argc' is zero.)
	     @param callback Optional callback object. HandleCallback is
	     called asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS CallMethod(ES_Object *object, const uni_char *method, int argc, const ES_Value *argv,
	                     ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL);
	/**< Call method 'method' on object 'object' with the 'argc' first values
	     in 'argv' as arguments.  The optional callback will be called iff
	     this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_CALLMETHOD if you use this function.

	     @param object An ES object.
	     @param method The name of a property on 'object' that can be called.
	     @param argc Number of arguments in 'argv'.
	     @param argv Arguments (can be NULL if 'argc' is zero.)
	     @param callback Optional callback object. HandleCallback is
	     called asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

#ifdef ESUTILS_ASYNCIF_PROPERTIES_SUPPORT

	OP_STATUS GetSlot(ES_Object *object, const uni_char *slot, ES_AsyncCallback *callback = NULL,
	                  ES_Thread *interrupt_thread = NULL);
	/**< Get a property named 'slot' on 'object'.  The optional callback will
	     be called iff this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param slot The name of a property on 'object'.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation (e.g. if 'object'
	             has no property named 'slot').
	    */

	OP_STATUS GetSlot(ES_Object *object, int index, ES_AsyncCallback *callback = NULL,
	                  ES_Thread *interrupt_thread = NULL);
	/**< Get a property at index 'index' on 'object'.  The optional callback will
	     be called iff this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param index The index of a property on 'object'.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation (e.g. if 'object'
	             has no property at index 'index').
	    */

	OP_STATUS SetSlot(ES_Object *object, const uni_char *slot, const ES_Value &value, ES_AsyncCallback *callback = NULL,
	                  ES_Thread *interrupt_thread = NULL);
	/**< Set a property named 'slot' on 'object'.  The optional callback will
	     be called iff this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param slot The name of a property on 'object'.
	     @param value The value to set to the property.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS SetSlot(ES_Object *object, int index, const ES_Value &value, ES_AsyncCallback *callback = NULL,
	                  ES_Thread *interrupt_thread = NULL);
	/**< Set a property at index 'index' on 'object'.  The optional callback
	     will be called iff this function returns OpStatus::OK.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param index The index of a property on 'object'.
	     @param value The value to set to the property.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS RemoveSlot(ES_Object *object, const uni_char *slot, ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL);
	/**< Remove the property named 'slot' on 'object'.  The optional
	     callback will be called iff this function returns
	     OpStatus::OK.

	     The value returned by this operation is the same as the value
	     the delete operator would have returned had the same
	     operation been performed by a script.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param slot The name of a property on 'object'.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS RemoveSlot(ES_Object *object, int index, ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL);
	/**< Remove the property at index 'index' on 'object'.  The
	     optional callback will be called iff this function returns
	     OpStatus::OK.

	     The value returned by this operation is the same as the value
	     the delete operator would have returned had the same
	     operation been performed by a script.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param index The index of a property on 'object'.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS HasSlot(ES_Object *object, const uni_char *slot, ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL);
	/**< Remove the property named 'slot' on 'object'.  The optional
	     callback will be called iff this function returns
	     OpStatus::OK.

	     The value returned by this operation is true if the object
	     had the named or indexed slot, and false otherwise.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param slot The name of a property on 'object'.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

	OP_STATUS HasSlot(ES_Object *object, int index, ES_AsyncCallback *callback = NULL, ES_Thread *interrupt_thread = NULL);
	/**< Remove the property at index 'index' on 'object'.  The
	     optional callback will be called iff this function returns
	     OpStatus::OK.

	     The value returned by this operation is true if the object
	     had the named or indexed slot, and false otherwise.

	     Import API_ESUTILS_ASYNC_PROPERTIES if you use this function.

	     @param object An ES object.
	     @param index The index of a property on 'object'.
	     @param callback Optional callback object. HandleError is
	     called on this on parse error. HandleCallback is called
	     asynchronously when the operation has finished.
	     @param interrupt_thread Optional thread to interrupt.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if
	             there was an error starting the operation.
	    */

#endif // ESUTILS_ASYNCIF_PROPERTIES_SUPPORT

	void SetWantExceptions();
	/**< Set a temporary flag indicating that the next call to Eval,
	     CallFunction, CallMethod, GetSlot or SetSlot should propagate
	     ECMAScript exceptions using the ES_ASYNC_EXCEPTION status
	     code.  Otherwise, exceptions are reported as for other types
	     of scripts (e.g. displayed in the JavaScript console) and the
	     callback is called with the ES_ASYNC_FAILURE status code.

	     This function should only be called right before a call to
	     one of the functions above.  The flag is reset by each such
	     call. */

	void SetWantDebugging();
	/**< Set a temporary flag indicating that the next call to Eval,
	     CallFunction, CallMethod, GetSlot or SetSlot should send
	     events to ES_DebugListener */

	void SetWantReformatSource();
	/**< Set a temporary flag indicating that the next call to Eval,
	     CallFunction, CallMethod, GetSlot or SetSlot should reformat
	     provided source before it's parsed and compiled. */

	void SetWantStringResult();
	/**< Set a temporary flag indicating that the next call to Eval,
	     CallFunction, CallMethod or GetSlot should convert the result
	     of the operation to a string before returning.

	     This function should only be called right before a call to
	     one of the functions above.  The flag is reset by each such
	     call. */

	void SetIsUserRequested() { is_user_requested = TRUE; }
	/**< Sets a flag signalling that the next thread started was
	     directly requested by the user (by using the mouse, keyboard
	     or UI to trigger some action.) */

	void SetOpenInNewWindow() { open_in_new_window = TRUE; }
	/**< Sets a flag signalling that the next thread started was
	     started by the user by clicking a link while holding down the
	     shift key (or otherwise requesting that a link be opened in a
	     new window.) */

	void SetOpenInBackground() { open_in_background = TRUE; }
	/**< Sets a flag signalling that the next thread started was
	     started by the user by clicking a link while holding down the
	     control key (or otherwise requesting that a link be opened in
	     the background.) */

	void SetStartLine(unsigned line) { OP_ASSERT(line != 0); start_line = line; }
	/**< Sets the initial line number for the script block in order
	     for line numbers in exceptions to be adjusted. */

	void SetIsPluginRequested() { is_plugin_requested = TRUE; }
	/**< Sets a flag signalling that the next thread started was
	     directly requested by plugins. */

	ES_Thread *GetLastStartedThread();
	/**< Returns the thread that was started by the last call to Eval,
	     CallFunction, CallMethod, GetSlot or SetSlot, if that call
	     was successful (returned OpStatus::OK).  This is only defined
	     immediately after the call (after the call and before control
	     is returned to the message loop or the scheduler, whichever
	     comes first).

	     @return A thread object. */

private:
	void Reset();

	ES_Runtime *runtime;
	ES_ThreadScheduler *scheduler;
	ES_ThreadListener *override_listener;
	BOOL want_exceptions;
	BOOL want_debugging;
	BOOL want_reformat_source;
	BOOL want_string_result;
	BOOL is_user_requested;
	BOOL open_in_new_window;
	BOOL open_in_background;
	unsigned start_line;
	BOOL is_plugin_requested;
	ES_Thread *last_thread;
};

#endif /* ES_UTILS_ESASYNCIF_H */
