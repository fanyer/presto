/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#ifndef ES_UTILS_ESSYNCIF_H
#define ES_UTILS_ESSYNCIF_H

#ifdef ESUTILS_SYNCIF_SUPPORT

#include "modules/ecmascript/ecmascript.h"

class ES_AsyncInterface;
class ES_Thread;

#ifdef ESUTILS_ES_ENVIRONMENT_SUPPORT
class ES_Environment;
#endif // ESUTILS_ES_ENVIRONMENT_SUPPORT

class ES_SyncInterface
{
public:
#ifdef ESUTILS_ES_ENVIRONMENT_SUPPORT
	ES_SyncInterface(ES_Environment *environment);
#endif // ESUTILS_ES_ENVIRONMENT_SUPPORT

	ES_SyncInterface(ES_Runtime *runtime, ES_AsyncInterface *esasyncif);

	class Callback
	{
	public:
		enum Status
		{
			ESSYNC_STATUS_SUCCESS,
			ESSYNC_STATUS_FAILURE,
			ESSYNC_STATUS_EXCEPTION,
			ESSYNC_STATUS_NO_MEMORY,
			ESSYNC_STATUS_CANCELLED
		};

		virtual OP_STATUS HandleCallback(Status status, const ES_Value &value) = 0;
		/**< Called when an operation has finished.  If status is
		     STATUS_FINISHED and the operation was one that returns a
		     value (all operations except SetSlot.)

		     @return Any OP_STATUS code. */

	protected:
		virtual ~Callback() {}
		/**< ES_SyncInterface never deletes callbacks, so this
		     destructor doesn't need to be virtual. */
	};

#ifdef ESUTILS_SYNCIF_EVAL_SUPPORT

	class EvalData
	{
	public:
		EvalData();
		/**< Constructor that sets all members to zero or NULL,
		     except allow_nested_message_loops that is set to TRUE.
		     At least one of program and program_array must be set
		     before Eval is called. */

		const uni_char *program;
		/**< Used unless it is NULL, in which case program_array
		     (which then must be non-NULL) is used instead. */

		ES_ProgramText *program_array;
		/**< Used only if program is NULL. */
		unsigned program_array_length;
		/**< Used only if program is NULL. */

		ES_Object **scope_chain;
		/**< Scope chain or NULL. */
		unsigned scope_chain_length;
		/**< Length of scope chain, ignored if scope_chain is NULL. */

		ES_Object *this_object;
		/**< An object that will be returned by "this" in the script,
		     or NULL for the default (the global object.) */

		ES_Thread *interrupt_thread;
		/**< Thread to interrupt or NULL. */

		BOOL want_exceptions;
		/**< If TRUE and the script ends with an uncaught exception, it
		     is signalled through the status ESSYNC_STATUS_EXCEPTION
		     instead of being reported in the error console. */

		BOOL want_string_result;
		/**< If TRUE, the script's result is automatically converted to
		     a string before returning. */

		BOOL allow_nested_message_loop;
		/**< If a script can't finish within the normal timeslice
		     (typically about 100ms) or if a script needs to run
		     async because of for instance waiting for a network
		     load or plugin load or the UI, then we can nest the
		     message loop. That has certain risks, since everything
		     can happen. Documents be deleted and windows closed
		     so the calling code must be very careful. */

		unsigned max_timeslice;
		/**< If allow_nested_message_loop is TRUE, then this will
		     will control how long we wait until we start nesting.
		     If allow_nested_message_loop is FALSE, then this controls
		     how long we wait before we give up. If the value is
		     0 (the default), then a suitable default time will be
		     used. The value is given in milliseconds*/
	};

	OP_STATUS Eval(const EvalData &data, Callback *callback = NULL);
	/**< Evaluate some code.  The optional callback will be called iff
	     this function returns OpStatus::OK.  If it is called, this
	     function returns the value returned by the callback's
	     HandleCallback function.

	     Import API_ESUTILS_SYNC_EVAL if you use this function.

	     @param data Eval data.
	     @param callback Optional callback object.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM,
	             OpStatus::ERR if there was an error starting the
	             operation (e.g. an error compiling the script) plus
	             any value returned the supplied callback's
	             HandleCallback function. */

#endif // ESUTILS_SYNCIF_EVAL_SUPPORT

#ifdef ESUTILS_SYNCIF_CALL_SUPPORT

	class CallData
	{
	public:
		CallData();
		/**< Constructor that sets all members to zero or NULL,
		     except allow_nested_message_loops that is set to TRUE.  At
		     least one of function or this_object+method must be set
		     before Call is called. */

		ES_Object *this_object;
		/**< An object that will be used as the this object in the
		     call. */

		ES_Object *function;
		/**< Function to call or NULL, in which case the property
		     named by 'method' (which then must be non-NULL) will be
		     read on this_object and, if its value was a function,
		     called. */

		const uni_char *method;
		/**< Name of method to call or NULL.  Used only if function is
		     NULL. */

		ES_Value *arguments;
		/**< Array of arguments or NULL, in which case the function is
		     called with no arguments. */
		unsigned arguments_count;
		/**< Number of arguments in arguments. */

		ES_Thread *interrupt_thread;
		/**< Thread to interrupt or NULL. */

		BOOL want_exceptions;
		/**< If TRUE and the call ends with an uncaught exception, it
		     is signalled through the status ESSYNC_STATUS_EXCEPTION
		     instead of being reported in the error console. */

		BOOL want_string_result;
		/**< If TRUE, the script's result is automatically converted to
		     a string before returning. */

		BOOL allow_nested_message_loop;
		/**< If a script can't finish within the normal timeslice
		     (typically about 100ms) or if a script needs to run
		     async because of for instance waiting for a network
		     load or plugin load or the UI, then we can nest the
		     message loop. That has certain risks, since everything
		     can happen. Documents be deleted and windows closed
		     so the calling code must be very careful. */

		unsigned max_timeslice;
		/**< If allow_nested_message_loop is TRUE, then this will
		     will control how long we wait until we start nesting.
		     If allow_nested_message_loop is FALSE, then this controls
		     how long we wait before we give up. If the value is
		     0 (the default), then a suitable default time will be
		     used. The value is given in milliseconds*/
	};

	OP_STATUS Call(const CallData &data, Callback *callback = NULL);
	/**< Call function or method.  The optional callback will be
	     called iff this function returns OpStatus::OK.  If it is
	     called, this function returns the value returned by the
	     callback's HandleCallback function.

	     Import API_ESUTILS_SYNC_CALL if you use this function.

	     @param data Call data.
	     @param callback Optional callback object.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM,
	             OpStatus::ERR if there was an error starting the
	             operation (e.g. an error compiling the script) plus
	             any value returned the supplied callback's
	             HandleCallback function. */

#endif // ESUTILS_SYNCIF_CALL_SUPPORT

#ifdef ESUTILS_SYNCIF_PROPERTIES_SUPPORT

	class SlotData
	{
	public:
		SlotData();
		/**< Constructor that sets all members to zero or NULL,
		     except allow_nested_message_loops that is set to TRUE.  At
		     least one of program and program_array must be set before
		     Eval is called. */

		ES_Object *object;
		/**< Object to access slot on.  If NULL, the global object is
		     used instead. */

		const uni_char *name;
		/**< Name of slot to access or NULL, in which case index will
		     be used. */

		unsigned index;
		/**< Index of slot to access.  Used only if name is NULL. */

		ES_Thread *interrupt_thread;
		/**< Thread to interrupt or NULL. */

		BOOL want_exceptions;
		/**< If TRUE and the operation ends with an uncaught
		     exception, it is signalled through the status
		     ESSYNC_STATUS_EXCEPTION instead of being reported in the
		     error console. */

		BOOL want_string_result;
		/**< If TRUE, the script's result is automatically converted to
		     a string before returning. */

		BOOL allow_nested_message_loop;
		/**< If a script can't finish within the normal timeslice
		     (typically about 100ms) or if a script needs to run
		     async because of for instance waiting for a network
		     load or plugin load or the UI, then we can nest the
		     message loop. That has certain risks, since everything
		     can happen. Documents be deleted and windows closed
		     so the calling code must be very careful. */

		unsigned max_timeslice;
		/**< If allow_nested_message_loop is TRUE, then this will
		     will control how long we wait until we start nesting.
		     If allow_nested_message_loop is FALSE, then this controls
		     how long we wait before we give up. If the value is
		     0 (the default), then a suitable default time will be
		     used. The value is given in milliseconds*/
	};

	OP_STATUS GetSlot(const SlotData &data, Callback *callback = NULL);
	/**< Get a named or indexed property.  The optional callback will
	     be called iff this function returns OpStatus::OK.  If it is
	     called, this function returns the value returned by the
	     callback's HandleCallback function.

	     Import API_ESUTILS_SYNC_PROPERTIES if you use this function.

	     @param data Slot data.
	     @param callback Optional callback object.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM,
	             OpStatus::ERR if there was an error starting the
	             operation (e.g. an error compiling the script) plus
	             any value returned the supplied callback's
	             HandleCallback function. */

	OP_STATUS SetSlot(const SlotData &data, const ES_Value &value, Callback *callback = NULL);
	/**< Set a named or indexed property.  The optional callback will
	     be called iff this function returns OpStatus::OK.  If it is
	     called, this function returns the value returned by the
	     callback's HandleCallback function.

	     Import API_ESUTILS_SYNC_PROPERTIES if you use this function.

	     @param data Slot data.
	     @param value New value.
	     @param callback Optional callback object.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM,
	             OpStatus::ERR if there was an error starting the
	             operation (e.g. an error compiling the script) plus
	             any value returned the supplied callback's
	             HandleCallback function. */

	OP_STATUS RemoveSlot(const SlotData &data, Callback *callback = NULL);
	/**< Set a named or indexed property.  The optional callback will
	     be called iff this function returns OpStatus::OK.  If it is
	     called, this function returns the value returned by the
	     callback's HandleCallback function.

	     The value returned by this operation is the same as the value
	     the delete operator would have returned had the same
	     operation been performed by a script.

	     Import API_ESUTILS_SYNC_PROPERTIES if you use this function.

	     @param data Slot data.
	     @param callback Optional callback object.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM,
	             OpStatus::ERR if there was an error starting the
	             operation (e.g. an error compiling the script) plus
	             any value returned the supplied callback's
	             HandleCallback function. */

	OP_STATUS HasSlot(const SlotData &data, Callback *callback = NULL);
	/**< Set a named or indexed property.  The optional callback will
	     be called iff this function returns OpStatus::OK.  If it is
	     called, this function returns the value returned by the
	     callback's HandleCallback function.

	     The value returned by this operation is true if the object
	     had the named or indexed slot, and false otherwise.

	     Import API_ESUTILS_SYNC_PROPERTIES if you use this function.

	     @param data Slot data.
	     @param callback Optional callback object.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY on OOM,
	             OpStatus::ERR if there was an error starting the
	             operation (e.g. an error compiling the script) plus
	             any value returned the supplied callback's
	             HandleCallback function. */

#endif // ESUTILS_SYNCIF_PROPERTIES_SUPPORT

private:
	ES_Runtime *runtime;
	ES_AsyncInterface *asyncif;
};

#endif // ESUTILS_SYNCIF_SUPPORT
#endif // ES_UTILS_ESSYNCIF_H
