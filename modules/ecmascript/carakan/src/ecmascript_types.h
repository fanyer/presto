/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2011
 *
 * Types exported from the ECMAScript engine
 *
 * @author Lars Thomas Hansen
 * @author Karl Anders Oygard
 */

#ifndef ECMASCRIPT_TYPES_H
#define ECMASCRIPT_TYPES_H

class ES_Object;						// Opaque engine-side type
class ES_Global_Object;					// Opaque engine-side type, compatible with ES_Object
class ES_Context;						// Opaque engine-side type
class ES_Program;						// Opaque engine-side type
class ES_Static_Program;
class ES_Compiled_Code;					// Opaque engine-side type
class EcmaScript_Object;
class ES_Runtime;

/** Type tags used by ES_Value.
  */
enum ES_Value_Type
{
	VALUE_UNDEFINED,
	VALUE_NULL,
	VALUE_BOOLEAN,
	VALUE_NUMBER,
	VALUE_STRING,
	VALUE_OBJECT,
	VALUE_STRING_WITH_LENGTH
};

/** Bits for designating attributes of properties
  */
enum ES_Property_Attributes
{
	PROP_READ_ONLY=1,
	PROP_DONT_ENUM=2,
	PROP_DONT_DELETE=4,
	PROP_IS_INDEXED=8,      // Internal use
	PROP_INDEX_STRUCT=16,   // Internal use
	PROP_EXISTS=64,         // Internal use
	PROP_FORCE=128,         // [[Put]] protocol: Override read-only flag on property
	PROP_HOST_PUT=256,      // [[Put]] protocol: property puts handled by host object, if one.
	PROP_ALL=-1             // Shorthand for all properties
};

/** Values for querying the engine for prototype objects.
  * FIXME  This is crude.  Why not use the same enum as is used in the engine?
  */
enum ES_Prototype
{
	ENGINE_OBJECT_PROTOTYPE,
	ENGINE_FUNCTION_PROTOTYPE,
	ENGINE_NUMBER_PROTOTYPE,
	ENGINE_STRING_PROTOTYPE,
	ENGINE_BOOLEAN_PROTOTYPE,
	ENGINE_ARRAY_PROTOTYPE,
	ENGINE_DATE_PROTOTYPE,
	ENGINE_ERROR_PROTOTYPE
};

/** Status values from call to ECMAScript function (from host).
  */
enum ES_Eval_Status
{
	ES_SUSPENDED,				///< Timeslice expired
	ES_NORMAL,					///< Normal return but no value
	ES_NORMAL_AFTER_VALUE,		///< Normal return with value
	ES_ERROR,					///< Generic failure
	ES_ERROR_NO_MEMORY,			///< Ran out of memory
	ES_BLOCKED,					///< Blocking suspend, requires explicit unblock
	ES_THREW_EXCEPTION			///< Exception was thrown
};

/** Status values from call to host function.
  *
  * It's possible to return multiple status values, so they are encoded
  * horizontally (all must fit into an int).  These values may be combined
  * by rules outlined in ecmascript.cpp.
  */
enum ES_Host_Call_Status
{
	ES_FAILED    = 0,			///< Generic error / "no interesting result" (ignored)
	ES_VALUE     = 1,			///< Value was returned
	ES_SUSPEND   = 2,			///< Engine must suspend
	ES_ABORT     = 4,			///< Engine must abort (fatal error)
	ES_NO_MEMORY = 8,			///< Ran out of memory; operation failed
	ES_RESTART   = 16,			///< Restart the operation (only makes sense with ES_SUSPEND)
	ES_TOSTRING  = 32,			///< Convert the return value from Call to a string
	ES_EXCEPT_SECURITY=64,		///< Origin check failed
	ES_EXCEPTION = 128,			///< Generic exception; return_value carries object to throw
	ES_NEEDS_CONVERSION = 256	///< Convert an argument into a given type; see @ES_ArgumentConversion@.
};

/** Return status codes from calls to GetName and GetIndex.
  *
  * OPTIMIZEME: if ES_GetState were to be subclassed from OpStatus then error
  * checking on the DOM side is likely to be faster and take less code space.
  */
enum ES_GetState
{
	GET_FAILED,				// Backwards compatible to BOOL
	GET_SUCCESS,			// Backwards compatible to BOOL
	GET_SUSPEND,			// Get has been blocked, must setup a restart
	GET_SECURITY_VIOLATION,
	GET_NO_MEMORY,
	GET_EXCEPTION
};

/** Host object return status codes from calls to PutName() and PutIndex(). */
enum ES_PutState
{
	PUT_SUCCESS,
	/**< The [[Put]] operation completed. To a first approximation
	     this signals that the supplied value was recorded by the
	     host object for the given property or index.

	     To the engine it signals that the host object has completed
	     the handling of the property update without errors; no more,
	     no less. */

	PUT_FAILED,
	/**< The value signals to the engine that the host object does not
	     acknowledge the given property name or index, and the [[Put]]
	     did not succeed. The engine is free to record the host object
	     as not having the property as a result, and optimize future
	     accesses based on that.

	     A host object implementation is well advised to use PUT_FAILED
	     with some care. */

	PUT_FAILED_DONT_CACHE,
	/**< The value also signals to the engine that the host object does
	     not acknowledge the given property name or index. The engine
	     may however not cache that 'negative' outcome, so subsequent
	     operations involving the same property should be performed
	     on the host objects (of its class.)

	     A host object may use this value to implement lighter weight
	     objects. */

	PUT_READ_ONLY,
	/**< The [[Put]] was attempted on a read-only property,
	     one with attribute [[Writable]] = false. The engine decides
	     how this host property update failure must be handled,
	     following what EcmaScript (and WebIDL) prescribes.

	     The default behaviour is to treat the operation as having
	     completed, mirroring PUT_SUCCESS's behaviour, but if the
	     [[Put]] was performed while in "strict mode", a native
	     TypeError exception will be thrown. */

	PUT_NEEDS_STRING,
	/**< The host object requests the engine to convert the value
	     to a VALUE_STRING before redoing the [[Put]] operation.
	     If the conversion fails, the host object operation
	     fails. */

	PUT_NEEDS_STRING_WITH_LENGTH,
	/**< The host object requests the engine to convert the value
	     to a VALUE_STRING_WITH_LENGTH before redoing the [[Put]]
	     operation. If the conversion fails, the host object operation
	     fails. */

	PUT_NEEDS_NUMBER,
	/**< The host object requests the engine to convert the value
	     to a VALUE_NUMBER before redoing the [[Put]] operation.
	     If the conversion fails, the host object operation
	     fails. */

	PUT_NEEDS_BOOLEAN,
	/**< The host object requests the engine to convert the value
	     to a VALUE_BOOLEAN before redoing the [[Put]] operation. */

	PUT_SECURITY_VIOLATION,
	/**< A security check failed while handling the [[Put]]; the
	     engine will propagate the exceptional condition by throwing
	     an EcmaScript ReferenceError. */

	PUT_NO_MEMORY,
	/**< An out-of-memory condition was encountered; the engine
	     will abort execution before propagating the OOM condition. */

	PUT_SUSPEND,
	/**< The host object requests the engine to suspend the current
	     execution context, asking it to keep the return value alive
	     across the suspension (the 'restart object'.) If the execution
	     context is explicitly unblocked later on, the engine arranges
	     for the [[Put]] operation to be restarted by calling the
	     corresponding PutNameRestart() / PutIndexRestart() operation
	     (see their documentation for details on the protocol.) */

	PUT_EXCEPTION
	/**< An exception has been thrown by the host object. The exception
	     value is communicated back using the PutName() / PutIndex()
	     'value' argument. */
};

/** Argument conversion - a host function may request the engine to further
  * converts its arguments. Either by asking for conversion of a single
  * argument into a specified primitive type, or by generalised conversion
  * of all arguments.
  *
  * The engine will take care of the conversion, and if successful,
  * restart the host function call with the converted arguments. If the
  * conversion fails, the engine propagates the conversion exception
  * as the result of the host function call -- this is done directly
  * without notifying the host function of failure.
  *
  * A host function signals argument conversion by returning ES_NEEDS_CONVERSION.
  * What kind is controlled via the return_value argument to the host function call.
  * If it contains a string (VALUE_STRING), it is considered a custom translation
  * of one or more of the arguments (see the host function specifier documentation.)
  *
  * If return_value contains a number (VALUE_NUMBER), it must be an
  * unsigned specifying both the argument number and the type of conversion
  * requested. The low 16-bit holds the ES_ArgumentConversion kind; bits 16
  * and above the argument (zero indexed.)
  *
  * If return value is a string (VALUE_STRING), the engine assumes this was
  * set up by the host call by invoking ES_CONVERT_ARGUMENTS_AS(). It extracts
  * the argument conversion string, which must be a (const char *), and
  * directs conversion based on that. If the conversion is successful,
  * the host function call is restarted with the converted argument array,
  * but with an encoded argc value. A host function call must determine
  * if the invocation is due to a conversion restart by testing argc
  * using ES_CONVERSION_COMPLETED(argc). If it is, the original argc
  * can be extracted using ES_CONVERSION_ARGC(argc). (See their documentation
  * for more information.) Notice that this restart protocol is only
  * used for general argument conversion; primitive one argument
  * conversion does not require it (nor uses it.)
  *
  * NOTE: a host method may not presently record any other resumption
  * values in return_value across a ES_NEEDS_CONVERSION return to the
  * ES engine. Converting multiple arguments at a time is supported
  * by using a host function specifier string, the other is one
  * argument at a time.
  *
  */
enum ES_ArgumentConversion
{
	ES_CALL_NEEDS_STRING = 1,
	ES_CALL_NEEDS_STRING_WITH_LENGTH,
	ES_CALL_NEEDS_NUMBER,
	ES_CALL_NEEDS_BOOLEAN
};

/** Instruct the engine to perform primitive conversion of
    the argument at index 'arg', using conversion 'kind'.
    A host function must use this prior to returning
    ES_NEEDS_CONVERSION. */
#define ES_CONVERT_ARGUMENT(kind, arg) ((arg << 16) | kind)

/** Set the argument conversion string to use for a host call
    asking the engine to convert its arguments again, by
    return ES_NEEDS_CONVERSION. */
#define ES_CONVERT_ARGUMENTS_AS(return_value, conversion_string8) \
    do {                                                          \
        return_value->type = VALUE_STRING;                        \
        return_value->value.string = reinterpret_cast<const uni_char *>(conversion_string8); \
    } while(0)

/** Returns TRUE if the host call has been restarted after the
    successful completion of argument conversion. A host call
    making use of general argument conversion must test for
    this, and if TRUE, reconstruct the original argument count
    (argc) by using ES_CONVERSION_ARGC(). */
#define ES_CONVERSION_COMPLETED(argc) (argc <= -2)

/** If ES_CONVERSION_COMPLETED() returns TRUE, ES_CONVERSION_ARGC()
    will extract the original argument count for a host call
	that has been restarted due to conversion. */
#define ES_CONVERSION_ARGC(argc) (-(2 + argc))

/** The ECMAScript native errors, used when creating native errors
    from host code. Errors that are either deprecated (EvalError) or
    just used by some of the global objects (URIError) have been
    omitted. For details on their meaning and areas of use, consult
    the ES5 specification (Section 15.11), (and to some extent)
    WebIDL for when to throw appropriate native errors. */
enum ES_NativeError
{
	ES_Native_Error,
	ES_Native_RangeError,
	ES_Native_ReferenceError,
	ES_Native_SyntaxError,
	ES_Native_TypeError
};

/** Return values from host object [[Delete]] operations, invoked
    via DeleteName() and DeleteIndex(). */
enum ES_DeleteStatus
{
	DELETE_REJECT,
	/**< Host property delete operation rejected, see WebIDL and
	     host object interface definition for when the operation
	     will be rejected. Depending on context, the engine may
	     raise a TypeError exception or return 'false' upon receiving
	     this return value. */
	DELETE_OK,
	/**< The delete operation was not rejected by the host object and
	     'true' should be returned as result. */
	DELETE_NO_MEMORY
	/**< OOM condition encountered while handling operation. */
};

/** Used in ES_Value to represent strings that may contain null characters.
  */
class ES_ValueString
{
public:
	const uni_char *string; // always null terminated after 'length' chars
	unsigned length;
};

/** Value union used to pass ECMAScript values across the engine boundary.
  */
class ES_Value
{
public:
	ES_Value() : type(VALUE_UNDEFINED), string_length(0xffffffu) {}

	union
	{
		bool			boolean;
		double			number;
		const uni_char*	string;
		ES_Object*		object;
		ES_ValueString* string_with_length;
	} value;

	ES_Value_Type type:8;
	unsigned string_length:24;

	unsigned GetStringLength() const { OP_ASSERT(type == VALUE_STRING); return string_length != 0xffffffu ? string_length : uni_strlen(value.string); }
};


/** Base class for data containers representing clonable host objects in
    "persistent" form.  The IsA() function is used to identify the type of
    object being represented, and should typically simply return TRUE for the
    same object type as the EcmaScript_Object::IsA() function of the object
    originally cloned. */
class ES_PersistentItem
{
public:
	virtual ~ES_PersistentItem() {}
	virtual BOOL IsA(int tag) = 0;
};

/** Class representing a cloned ECMAScript value in persistent form. */
class ES_PersistentValue
{
public:
	ES_PersistentValue()
		: data(NULL),
		  length(0),
		  items(NULL),
		  items_count(0)
	{
	}

	~ES_PersistentValue()
	{
		OP_DELETEA(data);
		for (unsigned index = 0; index < items_count; ++index)
			OP_DELETE(items[index]);
		OP_DELETEA(items);
	}

	unsigned *data;
	unsigned length;

	ES_PersistentItem **items;
	unsigned items_count;
};

/** A host object clone handler is responsible for cloning host objects as
    needed when ES_Runtime::Clone(), ES_Runtime::CloneToPersistent() or
    ES_Runtime::CloneFromPersistent() is used clone values.

    Observe that the "structured cloning" algorithm defined by HTML5 only
    supports cloning of a specific set of host objects.

    A host object clone handler should be registered using
    EcmaScript_Manager::RegisterHostObjectCloneHandler(), typically during
    start-up. */
class ES_HostObjectCloneHandler
{
public:
	ES_HostObjectCloneHandler()
		: next(NULL)
	{
	}

	virtual ~ES_HostObjectCloneHandler() {}

	virtual OP_STATUS Clone(EcmaScript_Object *source_object, ES_Runtime *target_runtime, EcmaScript_Object *&target_object) = 0;
	/**< Clone 'source_object' into 'target_runtime', if supported.  If not
	     supported, OpStatus::ERR should be returned.

	     @param source_object Object to clone.
	     @param target_runtime Runtime to clone into.
	     @param target_object Resulting object.

	     @return OpStatus::OK, OpStatus::ERR if 'source_object' can't be cloned
	             (by this handler) or OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef ES_PERSISTENT_SUPPORT
	virtual OP_STATUS Clone(EcmaScript_Object *source_object, ES_PersistentItem *&target_item) = 0;
	/**< Clone 'source_object' into a runtime-independent "persistent" form.  If
	     not supported, OpStatus::ERR should be returned.

	     @param source_object Object to clone.
	     @param target_item Resulting item.

	     @return OpStatus::OK, OpStatus::ERR if 'source_object' can't be cloned
	             (by this handler) or OpStatus::ERR_NO_MEMORY on OOM. */

	virtual OP_STATUS Clone(ES_PersistentItem *&source_item, ES_Runtime *target_runtime, EcmaScript_Object *&target_object) = 0;
	/**< Create an object from the runtime-independent "persistent" form.  If
	     not supported, OpStatus::ERR should be returned.

	     @param source_item Item to clone
	     @param target_runtime Runtime to clone into.
	     @param target_object Resulting object.

	     @return OpStatus::OK, OpStatus::ERR if 'source_item' can't be cloned
	             (by this handler) or OpStatus::ERR_NO_MEMORY on OOM. */
#endif // ES_PERSISTENT_SUPPORT

private:
	friend class EcmaScript_Manager;
	ES_HostObjectCloneHandler *next;
};

/** Representation of a piece of program text as passed to the parser.
  */
struct ES_ProgramText
{
	const uni_char* program_text;				///< Non-NUL terminated string
	unsigned int	program_text_length;		///< Number of significant characters in that string
};

enum ES_ScriptType
{
	SCRIPT_TYPE_LINKED,
	/**< SCRIPT element with SRC attribute. */
	SCRIPT_TYPE_INLINE,
	/**< SCRIPT element without SRC attribute, not via document.write(). */
	SCRIPT_TYPE_GENERATED,
	/**< SCRIPT element without SRC attribute, via document.write(). */
	SCRIPT_TYPE_EVENT_HANDLER,
	/**< Event handler attribute. */
	SCRIPT_TYPE_EVAL,
	/**< Compiled by eval() function. */
	SCRIPT_TYPE_TIMEOUT,
	/**< Compiled via setTimeout() or setInterval() function. */
	SCRIPT_TYPE_DEBUGGER,
	/**< Code evaluated by the debugger. */
	SCRIPT_TYPE_JAVASCRIPT_URL,
	/**< URL using javascript: scheme. */
	SCRIPT_TYPE_USER_JAVASCRIPT,
	/**< Regular user javascript file. */
	SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY,
	/**< "Greasemonkey"-style user javascript file. */
	SCRIPT_TYPE_BROWSER_JAVASCRIPT,
	/**< The automatically downloaded browser.js file. */
	SCRIPT_TYPE_EXTENSION_JAVASCRIPT,
	/**< User javascript file from an extension. */
	SCRIPT_TYPE_UNKNOWN
	/**< None of the above. */
};

/** Contains information about a position in source code.

    This class can provide line/column information relative to the script
    source, or relative to the enclosing document ("absolute").

    For instance, if you have a document that looks like the example below,
    then you will get an runtime error because you tried to access 'foo'
    (which does not exist).

      <!doctype html>
      <!-- Intentional waste of space.  -->
      <script>foo.bar = 1;</script>

    The resulting error would then have relative line number '1', and relative
    column '0'. Relative line numbers start counting from the start of the
    script source, and not from the start of the enclosing document.

    The absolute line number of the error will be '3', and the absolute column
    will be '8', because those values take into account the position of the
    script in the original document.

    For linked scripts (and other scripts which do not have an enclosing
    document, the relative and absolute line numbers will be equal. */
class ES_SourcePosition
{
public:

	ES_SourcePosition()
		: index(0)
		, line(1)
		, column(0)
		, document_line(1)
		, document_column(0)
	{
	}

	ES_SourcePosition(unsigned index, unsigned line, unsigned column, unsigned document_line, unsigned document_column)
		: index(index)
		, line(line)
		, column(column)
		, document_line(document_line)
		, document_column(document_column)
	{
	}

	unsigned GetRelativeIndex() const { return index; }
	/**< Get the character index, relative to the start of the script source. */
	unsigned GetRelativeLine() const { return line; }
	/**< Get the line number, relative to the start of the script source. */
	unsigned GetRelativeColumn() const { return column; }
	/**< Get the column, relative to the script source. */

	unsigned GetAbsoluteLine() const { return line + document_line - 1; }
	/**< Get the line number, relative to the enclosing document (if any). */
	unsigned GetAbsoluteColumn() const {return line == 1 ? column + document_column : column; }
	/**< Get the column, relative to the enclosing document (if any). */

private:

	unsigned index;  ///< The character index, relative to the script source.
	unsigned line;   ///< The line number, relative to the script source.
	unsigned column; ///< The column, relative to the script source.

	/* For inline scripts, we are interested in knowing where in the enclosing
	   document the script source begins. This allows us to provide 'absolute'
	   line information in error messages.

	   This information is called the "document line", and the "document
	   column". */

	unsigned document_line;    ///< The line where the script source starts.
	unsigned document_column;  ///< The column where the script source starts.
};

/** Data type for constructing syntax error messages. */
struct ES_ErrorInfo
{
	uni_char       error_text[1024];			///< Buffer in which the message will be produced
	int            error_length;				///< Length of that buffer
	const uni_char *error_context;				///< The context for the error
	ES_SourcePosition error_position;           ///< The position of the error.
	unsigned int   script_guid;                 ///< ID of the script that the error occured in or 0 if no there is no script

	/** Initialize this object with the given context.
	  *
	  * @param context  Some user-legible string saying the context in which the
	  *                 error occured.  Must not be deallocated or altered while
	  *				    this object is live.
	  */
    ES_ErrorInfo(const uni_char *context)
		: error_length(sizeof(error_text)/sizeof(error_text[0]))
		, error_context(context)
 		, script_guid(0)
	{
	}

	void SetError(const uni_char* fmt, ...);
};

struct ES_CompilerOptions
{
	ES_CompilerOptions();

	BOOL disassemble;		/* Disassemble the resulting code, if disassembler available */
	BOOL symbolic;			/* Disassemble to a symbolic format */
	BOOL cplusplus;			/* Disassemble to C++ format */
	BOOL prune;				/* Prune trailing NOPs from disassembled output */
	BOOL protect_header;	/* Put #include .../es_pch.h" inside the #ifdefs */
	const char* condition;	/* If not NULL, wrap disassembled C++ in #if <condition> */
	const char* prefix;		/* If not NULL; the prefix to use instead of "es_library" */
	int  optimize_level;	/* 0, 1, or 2 */
	BOOL function_body;		/* Compile as function body, ie, allow top-level return statement */
};

#ifdef ECMASCRIPT_DEBUGGER

/* Debugger support classes.

   The engine supports a small event-and-control based debugging API; debuggers
   can hook into this to provide eg a GUI debugger.

   Debugging in the engine is enabled through a preference:

      [Extensions]
	  Script Debugger=1

   This preference causes code to be compiled with less optimization, which allows
   the debugger easier access to variables and so on.  (Some code may run more
   slowly as a consequence.)  A very nice debugger will turn this preference on
   when it is started and turn it off again when it is done.

   The debugger hooks itself into the engine by installing a debug listener: an
   instance of ES_DebugListener, installed through EcmaScript_Manager::SetDebugListener().

   The engine reports status information and run-time events on this listener.
   The listener responds to run-time events with a code: whether to continue execution
   or suspend execution.  Typically the scheduler will be in cahoots with the debugger
   to handle script suspensions.

   When the engine has reported an event, or when it is suspended, its state can be
   queried using the debugger API in EcmaScript_Manager.

   When the debugger is finished, it can uninstall itself through another call to
   SetDebugListener, passing NULL as an argument.
   */

class ES_DebugScriptData
{
public:
	const uni_char *source;			///< Complete source code, in its original form
	const uni_char *url;			///< The URL of origin, or NULL
	ES_ScriptType type;				///< The script type.
	unsigned int script_guid;		///< A globally unique script serial number, 0 means "no information"
};

class ES_DebugStackElement
{
public:
	ES_Object *this_obj;			///< The 'this' object for this frame, NULL if not applicable
	ES_Object *function;			///< The function of the stack frame, or NULL if program/eval/etc.
	ES_Object *arguments;			///< The arguments object, NULL if not applicable
	ES_Object *variables;			///< The local scope object, NULL if not applicable or possible
	unsigned script_guid;			///< A globally unique script serial number, 0 means "no information"
	unsigned line_no;				///< The source position relative to start of script, 0 means "no information"
};

enum ES_DebugObjectType
{
	OBJTYPE_NATIVE_OBJECT,			///< Any non-function non-host object type
	OBJTYPE_NATIVE_FUNCTION,		///< Any native function type (whether C++ or bytecoded)
	OBJTYPE_HOST_OBJECT,			///< Any host non-callable object type
	OBJTYPE_HOST_FUNCTION,			///< Any host function type
	OBJTYPE_HOST_CALLABLE_OBJECT	///< Any callable (but non-function) host object
};

class ES_DebugObjectElement
{
public:
	ES_Object *prototype;			///< The object's prototype, or NULL
	const char* classname;			///< The object's class name
	ES_DebugObjectType type;		///< Discriminator for the following union
	union
	{
		struct						///< If NATIVE_FUNCTION or HOST_FUNCTION
		{
			unsigned int nformals;	///< Number of formals; 0 if not known
			const uni_char* name;	///< NUL-terminated function name; empty string if anonymous function
			const uni_char* alias;  ///< NUL-terminated alias, if 'name' is NULL. May be NULL if no alias.
		} function;
	} u;
};

class ES_DebugListener
{
public:
	virtual ~ES_DebugListener();  // Kills GCC warnings

	virtual BOOL EnableDebugging(ES_Runtime *runtime) = 0;
		/**< Called when the engine wants to know whether debugging should be
			 enabled for a certain runtime.
			 @param runtime (in) The runtime which should be checked.
			 @return TRUE if debugging should be enabled, FALSE if it should not.
			 */

	virtual void NewScript(ES_Runtime *runtime, ES_DebugScriptData *data) = 0;
		/**< Called when a script (including functions and eval strings) is compiled.
			 @param runtime (in) The runtime to which the script belongs, or NULL if unknown
			 @param data (in) Information about the script.  Copy this information if you need
						 it, it may be deleted at anytime after NewScript() returns.
	         */

	virtual void ParseError(ES_Runtime *runtime, ES_ErrorInfo *err) {}
		/**< Called when an error occured during parsing of the script */

	virtual void NewContext(ES_Runtime *runtime, ES_Context *context) = 0;
		/**< Called when a context is created */

	virtual void EnterContext(ES_Runtime *runtime, ES_Context *context) = 0;
		/**< Called when execution of a context starts (every time, not just first time per context).
		     Sets current context for calls to HandleEvent. */

	virtual void LeaveContext(ES_Runtime *runtime, ES_Context *context) = 0;
		/**< Called when execution stops (temporarily or finally). */

	virtual void DestroyContext(ES_Runtime *runtime, ES_Context *context) = 0;
		/**< Called when a context is destroyed */

	virtual void DestroyRuntime(ES_Runtime *runtime) = 0;
		/**< Called when a runtime is destroyed */

	virtual void DestroyObject(ES_Object *object) = 0;
		/**< Called when a object, tracked by the debugger, is destroyed */

	enum EventType
	{
		ESEV_NONE,			// Invalid or uninitialized.
		ESEV_NEWSCRIPT,		// New script is about to execute
		ESEV_ENTERFN,		// Just entered a function: parameters have been set up, about to execute body
		ESEV_LEAVEFN,		// About to leave a function: return value has been computed
		ESEV_RETURN,		// Function return statement (also implicit return)
		ESEV_CALLSTARTING,	// About to call a function
		ESEV_CALLCOMPLETED,	// Just returned from a function
		ESEV_STATEMENT,		// About to start executing a statement
		ESEV_ERROR,			// About to throw an exception due to a checked runtime error, the exception object has been constructed
		ESEV_HANDLED_ERROR,	// About to throw an exception handled in a try/catch block, the exception object has been constructed
		ESEV_EXCEPTION,		// About to throw an exception in a 'throw' statement; the exception object has been computed
		ESEV_ABORT,			// About to abort the computation (popup blocker; stack overflow)
		ESEV_BREAKHERE		// User-inserted breakpoint ("debugger;" statement)
	};

	enum EventAction
	{
		ESACT_SUSPEND,		// Script should suspend as if its timeslice were expired.  May only be used
							//   if the debug information passed to HandleEvent is not (0,0).
		ESACT_CONTINUE		// Script should continue executing
	};

	virtual EventAction HandleEvent(EventType type, unsigned int script_guid, unsigned int line_no) = 0;
		/**< Engine should suspend if function returns SUSPEND.

			 When signalling an event, the current source position will be roughly:

			   ENTERFN:       function's "{"
			   LEAVEFN:       function's "}"
			   RETURN:        return statement (when no explicit return, same as LEAVEFN)
			   CALLSTARTING:  statement containing the call
			   CALLCOMPLETED: statement containing the call
			   STATEMENT:     first line of statement (but for do{}while should be the line with "while" on it)
			   ERROR:         like statement for the statement that caused the error
			   EXCEPTION:     like statement for the statement that threw the exception
			   ABORT:         like statement for the statement that caused the abort

			 For script_guid and line_no, the value 0 means "no information available".
			 */

	virtual void RuntimeCreated(ES_Runtime *runtime) = 0;
};

class ES_PropertyFilter
{
public:
	virtual ~ES_PropertyFilter(){};
	virtual BOOL Exclude(const uni_char *name, const ES_Value &value) = 0;
};

struct ES_DebugControl
{
	enum					// Operations on the VM state when stopped in a debug event
	{
		SET_LISTENER,
		IGNORE_ERROR,
		GET_SCOPE,
		GET_STACK,
		GET_EXCEPTION,
		GET_RETURN,
		GET_CALLEE,
		GET_PROPERTIES,
		GET_ATTRIBUTES,
		DESTROY_RUNTIME
	} op;

	enum
	{
		OUTERMOST_FRAME = 0x7FFFFFFF,
		ALL_FRAMES = 0x7FFFFFFF
	};


	union
	{
		struct
		{
			ES_DebugListener *listener;			// in
		} set_listener;
		struct
		{
			ES_Context *context;				// in
			ES_Value *value;					// in
		} ignore_error;
		struct
		{
			ES_Context *context;				// in
			unsigned int frame_index;			// in
			unsigned int length;				// out
			ES_Object **scope_objects;			// out
		} get_scope;
		struct
		{
			ES_Context *context;				// in
			unsigned int length;				// in/out
			ES_DebugStackElement *stack_frames; // out
		} get_stack;
		struct
		{
			ES_Context *context;				// in
			ES_Value *value;					// in/out
		} get_exception;
		struct
		{
			ES_Context *context;				// in
			ES_Value *value;					// in/out
		} get_return_value;
		struct
		{
			ES_Context *context;				// in
			ES_Object *object;					// out
		} get_callee;
		struct
		{
			ES_Context *context;				// in
			ES_Object *object;					// in
			ES_PropertyFilter *filter;			// in
			bool skip_non_enum;					// in
			uni_char **names;					// out
			ES_Value *values;					// out
		} get_properties;
		struct
		{
			ES_Object *object;					// in
			ES_DebugObjectElement *attr;		// in
		} get_attributes;
		struct
		{
			ES_Runtime* rt;						// in
		} destroy_runtime;
	} u;
};

#endif // ECMASCRIPT_DEBUGGER

#ifdef ECMASCRIPT_NATIVE_SUPPORT
#if 0 // WIP: not hooked up yet.

class ES_JITListener
{
public:
	enum EventType
	{
		EVENT_COMPILING,
		/**< Initially JIT compiling a function or other code block. */

		EVENT_RECOMPILING,
		/**< Recompiling function or other code block after the previous version
		     has proven inadequate. */

		EVENT_INLINING,
		/**< Inlining call to function into another function or other code
		     block.  The 'other' argument names the function that was
		     inlined. */

		EVENT_INLINE_FAILED
		/**< The inlining of a previously inlined function has failed, either
		     because the function expression yielded a different function, or
		     because the inlined function turned out to be too complex to be
		     inlined when executed. */
	};

	enum CodeType
	{
		CODE_FUNCTION,
		/**< Complete function. */

		CODE_CONSTRUCTOR,
		/**< Complete function, called by aa new expression. */

		CODE_PROGRAM_LOOP,
		/**< Loop in program code. */

		CODE_EVAL_LOOP
		/**< Loop in eval:ed code. */
	};

	virtual void OnJITEvent(EventType event_type, CodeType code_type, const uni_char *name, const uni_char *other) = 0;
	/**< Called for various JIT compiler events.  The 'event_type' argument
	     defines the type of event and the 'code_type' argument defines the type
	     of the code that was being JIT compiled or was otherwise affected.  If
	     'code_type' is CODE_FUNCTION or CODE_CONSTRUCTOR, 'name' is the name of
	     that function (or NULL if anonymous) and otherwise NULL.  If
	     'event_type' is EVENT_INLINING 'other' is the name of the function that
	     is being inlined (or NULL if the function is anonymous), otherwise it
	     is NULL. */
};

#endif // 0
#endif // ECMASCRIPT_NATIVE_SUPPORT

/** Base cases for tags passed to EcmaScript_Object::IsA().  The idea here is
    that each disjoint use of EcmaScript_Object as a base class will have one
    code in this enum to use as a basis for its own type tags (the low 10 bits
    are available for this) and will override IsA() to recognize its own types,
    and guard its operations with these type tests.
    */
enum EcmaScript_HostObjectType
{
       ES_HOSTOBJECT_DOM        = (1<<10),
       ES_HOSTOBJECT_NS4PLUGIN  = (2<<10),
       ES_HOSTOBJECT_JSPLUGIN   = (4<<10)
};

/** Base cases for tags passed to ES_Runtime::PutPrivate().
    */
enum EcmaScript_PrivateName
{
       ES_PRIVATENAME_DOM        = (0<<10),	// DOM uses the 2048 first positions
       ES_PRIVATENAME_JSPLUGIN   = (2<<10)
};

/** Event codes, generally for User Javascript support */
enum
{
    ES_EVENT_AFTER_GLOBAL_VARIABLES_INITIALIZED=1
};

/** Convenience macros used by DOM, exported from module since they translate to module return values. */

/** Return an ES_GetState value translated from an OP_STATUS, if the status was an error. */
#define GET_FAILED_IF_ERROR(expr)                                                                 \
	do {                                                                                            \
		OP_STATUS GET_FAILED_IF_ERROR_TMP = expr;                                                   \
		if (OpStatus::IsError(GET_FAILED_IF_ERROR_TMP))                                             \
			return GET_FAILED_IF_ERROR_TMP == OpStatus::ERR_NO_MEMORY ? GET_NO_MEMORY : GET_FAILED; \
   } while(0)

/** Return an ES_GetState value translated from an OP_STATUS, if the status was a memory error. */
#define GET_FAILED_IF_MEMORY_ERROR(expr)                                                            \
	do {                                                                                            \
		if (OpStatus::IsMemoryError(expr))                                                          \
			return GET_NO_MEMORY;                                                                   \
   } while(0)

/** Return an ES_PutState value translated from an OP_STATUS, if the status was an error */
#define PUT_FAILED_IF_ERROR(expr)                                                                 \
	do {                                                                                            \
		OP_STATUS PUT_FAILED_IF_ERROR_TMP = expr;                                                   \
		if (OpStatus::IsError(PUT_FAILED_IF_ERROR_TMP))                                             \
			return PUT_FAILED_IF_ERROR_TMP == OpStatus::ERR_NO_MEMORY ? PUT_NO_MEMORY : PUT_FAILED; \
   } while(0)

/** Return an ES_PutState value translated from an OP_STATUS, if the status was a memory error */
#define PUT_FAILED_IF_MEMORY_ERROR(expr)    \
	do {                                    \
		if (OpStatus::IsMemoryError(expr))  \
			return PUT_NO_MEMORY; \
	} while(0)

/** Return an ES_GetState value translated from an OP_STATUS, if the status was an error */
#define CALL_FAILED_IF_ERROR(expr)                                                                \
	do {                                                                                            \
		OP_STATUS CALL_FAILED_IF_ERROR_TMP = expr;                                                  \
		if (OpStatus::IsError(CALL_FAILED_IF_ERROR_TMP))                                            \
			return CALL_FAILED_IF_ERROR_TMP == OpStatus::ERR_NO_MEMORY ? ES_NO_MEMORY : ES_FAILED;  \
   } while(0)

/** Return an ES_DeleteStatus value translated from an OP_STATUS, if the status was a memory error. */
#define DELETE_FAILED_IF_MEMORY_ERROR(expr) \
	do {                                    \
		if (OpStatus::IsMemoryError(expr))  \
			return  DELETE_NO_MEMORY;       \
   } while(0)

/** Return a value from GetName as a CALL result code */
#define RETURN_GETNAME_AS_CALL(expr)                      \
	do {                                                    \
		ES_GetState XXX_TMP = expr;                         \
		if (XXX_TMP == GET_SUCCESS) return ES_VALUE;        \
		if (XXX_TMP == GET_NO_MEMORY) return ES_NO_MEMORY;  \
		return ES_FAILED;                                   \
	} while (0)

/** Return a value from PutName as a CALL result code */
#define RETURN_PUTNAME_AS_CALL(expr)                      \
	do {                                                    \
		ES_PutState XXX_TMP = expr;                         \
		if (XXX_TMP == PUT_SUCCESS) return ES_VALUE;        \
		if (XXX_TMP == PUT_NO_MEMORY) return ES_NO_MEMORY;  \
		return ES_FAILED;                                   \
	} while (0)

class ES_HeapReference;

class ES_ObjectReference
{
public:
	ES_ObjectReference() : heap(NULL), object(NULL) {}
	~ES_ObjectReference() { Unprotect(); }

	OP_STATUS Protect(ES_Runtime *runtime, ES_Object *object);
	/**< Protect 'object' from the garbage collector.  This may fail due to OOM,
	     in which case the object will remain unprotected and thus unsafe to
	     keep a pointer to.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	void Unprotect();
	/**< Unprotect previously protected object, or no-op if nothing is
	     protected. */

	ES_Object *GetObject() { return object; }

private:
	ES_HeapReference *heap;
	ES_Object *object;
};


/** An abstract interface for reporting enumerable host properties,
    named and/or indexed.

    Host objects will be passed an instance of this interface when the
    engine wants to determine their set of enumerable properties,
    @see EcmaScript_Object::FetchPropertiesL(). */
class ES_PropertyEnumerator
{
public:
	virtual void AddPropertyL(const char *name, unsigned host_code = 0) = 0;
	virtual void AddPropertyL(const uni_char *name, unsigned host_code = 0) = 0;
	/**< Register 'name' as an enumerated named property.

	     The implementation accumulates a set of property names, hence
	     registering the same property name more than once will only
	     result in one entry being added.

	     @param name The property name. If NULL, this is treated as if
	            an empty ("") property name was supplied.
	     @param host_code If non-zero, the unique host code to associate
	            with the property name. A minor optimization; if not
	            supplied, the property name's host code will be computed
	            on demand later.

	     The method will leave on OOM. */

	virtual void AddPropertiesL(unsigned index, unsigned count) = 0;
	/**< Register the range [index..(index + count)) as enumerable
	     indexed properties.

	     The implementation accumulates a set of property indices, hence
	     registering the same index more than once will only result in
	     one entry being added.

	     @param index The starting index.
	     @param count Starting from 'index', how many indices are
	            included in the range. Zero meaning that the range
	            is empty.

	     @return OpStatus::OK on successful addition,
	             OpStatus::ERR_NO_MEMORY on OOM. */
};

#endif // ECMASCRIPT_TYPES_H
