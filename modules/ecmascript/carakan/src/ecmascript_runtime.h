/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2004
 *
 * class ES_Runtime
 */

#ifndef ECMASCRIPT_RUNTIME_H
#define ECMASCRIPT_RUNTIME_H

#include "modules/ecmascript/carakan/src/ecmascript_heap.h"

class FramesDocument;
class Window;
class URL;
class EcmaScript_Object;
class ES_RuntimeReaper;
class ES_Object;
class ES_Program;
class ES_ThreadScheduler;
class ES_AsyncInterface;
class ES_Context;
class ES_Parser;
struct ESRT_Data;
class ES_ErrorData;
class ES_ParseErrorInfo;

#include "modules/util/adt/opvector.h"
#include "modules/util/opstring.h"

/** @short  ES_Runtime represents an execution environment associated with a single
    ECMAScript global object.
	*/

class ES_Runtime
{
friend class EcmaScript_Object;			// For access to RegisterRuntimeOn
private:
	OP_STATUS RegisterRuntimeOn(EcmaScript_Object *obj);
		/**< Set up a reference from the object to the runtime so that the runtime is
			 kept alive as long as the object is live.  The reference is actually
			 from the object's foreign-object to the runtime's runtime-reaper.

			 Used only by our friend, EcmaScript_Object.
			 */

public:
	ES_Runtime();
		/**< Create and initialize the object, but do not hook it into any other
		     structures yet.  At this point, it is safe to destroy the runtime
			 with the delete operator.
			 */

	virtual ~ES_Runtime();
		/**< The destructor calls Clean() and releases any non-collectible resources
			 held by the runtime.  Normally this should not be called by client
			 code unless construction fails; use Detach() instead.
			 */

	virtual void GCTrace();
		/**< Called during GC when this runtime's runtime reaper is traced. */

	OP_STATUS Construct(EcmaScript_Object *host_global_object = NULL, const char *classname = NULL, BOOL use_native_dispatcher = FALSE, ES_Runtime *parent_runtime = NULL, BOOL create_host_global_object = TRUE, BOOL create_host_global_object_prototype = FALSE);
		/**< Initialize the runtime and make it usable.

		     Initialize the ECMAScript manager if not already initialized.
		     Create a global-object for this runtime and flag the runtime
		     as usable.  Set up structures so that the runtime will be
		     deleted when it is no longer needed (see Detach() below).

		     If the operation fails then the runtime cannot be used;
		     it must be deleted with the 'delete' operator.

		     If the operation succeeds then the runtime must not be deleted
		     with the 'delete' operator, instead it must be signalled as
		     deletable using Detach(), and deletion will occur automatically
		     when the garbage collector is run.

		     @param host_global_object The host object counterpart for
		            the global object in this runtime. If NULL, no such
		            counterpart is wanted.
		     @param classname The class name to use for the global object;
		            must be NULL iff foreign_global_object is NULL.
		     @param use_native_dispatcher If supported, a flag controlling
		            whether or not you want this runtime to generate and
		            execute native code.
		     @param parent_runtime If a runtime is running in the context
		            of another, create it to share underlying heap right
		            away.
		     @param create_host_global_object If TRUE and no host object
		            is supplied, create an empty one. Provided as FALSE
		            if you have to delay supplying the host object
		            counterpart, @see SetHostGlobalObject().
		     @param create_host_global_object_prototype If TRUE then
		            create a global object with a host object as prototype.
		            The expectation is that the caller must follow up with
		            a call to SetHostGlobalObject() to assign that host
		            object prototype.

		     @return OpStatus::OK on success, OpStatus::ERR on failed
		             argument validation, OpStatus::ERR_NO_MEMORY on OOM. */

	void InitializationFinished();
		/**< Should be called to signal that the initialization of the scripting
		     environment is finished. */

	void Detach();
		/**< Call Detach to say it's OK to delete the ES_Runtime whenever other
			 conditions are met.  Detach marks the global-object as closed,
			 and asks the RTS to consider doing a garbage collection.  The runtime
			 will be deleted when it is no longer referenced in a way visible to
			 the garbage collector.
			 */

	enum ObjectFlags
	{
		FLAG_NEED_DESTROY = 1,
		/**< Object has a destructor that needs to be called. */

		FLAG_SINGLETON = 2
		/**< Object will have a set of properties that is likely unique. */
	};

	OP_STATUS AllocateHostObject(ES_Object *&object, void *&payload, unsigned payload_size, ES_Object *prototype, const char *class_name, unsigned flags = FLAG_NEED_DESTROY);
		/**< Allocate and initialize a host object.  Additional 'payload_size'
		     bytes of memory is allocated to hold the EcmaScript_Object part of
		     the host object.  The caller should in-place construct its object
		     in the memory assigned to 'payload'.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS AllocateHostFunction(ES_Object *&object, void *&payload, unsigned payload_size, ES_Object *prototype, const uni_char *function_name, const char *class_name, const char *parameter_types, unsigned flags = 0);
		/**< Allocate and initialize a host function.  Additional 'payload_size'
		     bytes of memory is allocated to hold the EcmaScript_Object part of
		     the host object.  The caller should in-place construct its object
		     in the memory assigned to 'payload'.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS KeepAliveWithRuntime(EcmaScript_Object *obj);
		/**< Set up a reference from the runtime to the object so that the object is
			 kept alive as long as the runtime is live.  The reference is actually
			 from the runtime's runtime-reaper to the obj's native_object, so it
			 is OK if obj is deleted manually even after being registered (though this
			 is a weird thing to do and does not unregister or delete the obj's
			 native_object).
			 */

	OP_STATUS KeepAliveWithRuntime(ES_Object *obj);
		/**< Set up a reference from the runtime to the object so that the object is
			 kept alive as long as the runtime is live.  The reference is actually
			 from the runtime's runtime-reaper to the obj's native_object, so it
			 is OK if obj is deleted manually even after being registered (though this
			 is a weird thing to do and does not unregister or delete the obj's
			 native_object).
			 */

	void SuitableForReuse(BOOL merge);
		/**< Used to signal that it would be good to merge the next connected heap
		     immediately rather than on-demand. Call with TRUE and then FALSE after the
		     operation that should have triggered the merge. The calls must
		     ultimately be balanced, but it is ok to nest such
		     "TRUE operation FALSE" invocations so that it becomes
		     "TRUE (TRUE operation FALSE) FALSE". */

	BOOL HasHighMemoryFragmentation();
		/**< Returns TRUE if the memory system used by this heap suffers from high
		     fragmentation, i.e. has lots of unfreeable free memory. This is used as an
		     heuristic to determine when it would be good to merge heaps early rather
		     than on demand (merging late is a possible source of fragmentation and we
		     don't want to make things worse). */

	OP_STATUS MergeHeapWith(ES_Runtime *other);
		/**< Merge this runtime's and other's heaps.  If the operation fails,
		     nothing will have happened.  If the two runtime's already have the
		     same heap, nothing is done. */

	OP_STATUS SetHostGlobalObject(EcmaScript_Object *host_global_object, EcmaScript_Object *host_global_object_prototype);
		/**< Connect this runtime's global object with a host object and
		     the host object of the global object prototype. This must
		     match the call to Construct(), of having created a global
		     object with a host object as its prototype.

		     @param host_global_object The host object counterpart for the
		            runtime's global object. Cannot be NULL.
		     @param host_global_object_prototype The host object counterpart
		            for the runtime's global object prototype. Set to NULL
		            if it doesn't have such.
		     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	ES_Object *GetGlobalObject();
		/**< @return The global object of this runtime. */

	ES_Object *GetGlobalObjectAsPlainObject() const;
		/**< @return The global object of this runtime, cast to an ES_Object. */

	static ES_Object *GetGlobalObjectInScope(ES_Context *context);
		/**< @return The global object currently in scope and active for the given
		             context. NULL if not available. */

	BOOL Enabled() const;
		/**< @return TRUE if execution is enabled in this runtime, otherwise FALSE */

	void Disable();
		/**< Disable execution in this runtime */

	BOOL GetErrorEnable() const;
		/**< @return TRUE if error messages will be processed by this runtime, otherwise FALSE */

	void SetErrorEnable(BOOL enable);
		/**< Set or reset the processing of error messages in this runtime. */

	FramesDocument* GetFramesDocument() const;
		/**< @return the frames-document associated with this runtime, or NULL if it has
			         no associated frames-document */

	void SetFramesDocument(FramesDocument* frames_document);
		/**< Set the frames-document to be associated with this runtime.  Passing a NULL
			 pointer disassociates the runtime from its current frames-document. */

	BOOL HasDebugPrivileges() const;
		/**< @return TRUE if this runtime is specially privileged to override
			         normal JavaScript security checks. */

	void SetDebugPrivileges();
		/**< Make this runtime privileged to override normal JavaScript security checks. */

    ES_Object* GetObjectPrototype();
		/**< @return the Object prototype of the global-object of this runtime */

    ES_Object* GetFunctionPrototype();
		/**< @return the Function prototype of the global-object of this runtime */

    ES_Object* GetNumberPrototype();
		/**< @return the Number prototype of the global-object of this runtime */

    ES_Object* GetStringPrototype();
		/**< @return the String prototype of the global-object of this runtime */

    ES_Object* GetBooleanPrototype();
		/**< @return the Boolean prototype of the global-object of this runtime */

    ES_Object* GetArrayPrototype();
		/**< @return the Array prototype of the global-object of this runtime */

    ES_Object* GetDatePrototype();
		/**< @return the Date prototype of the global-object of this runtime */

    ES_Object* GetErrorPrototype();
		/**< @return the Error prototype of the global-object of this runtime */

    ES_Object* GetTypeErrorPrototype();
		/**< @return the TypeError prototype of the global-object of this runtime */

	/** Privilege level of JS code; see bug CORE-9364. */
	enum PrivilegeLevel
	{
		PRIV_LVL_UNSPEC     = 0,
		PRIV_LVL_UNTRUSTED  = 1,
		PRIV_LVL_USERJS     = 2,
		PRIV_LVL_BROWSERJS  = 2,
		PRIV_LVL_BUILTIN    = 3
	};

	static BOOL  HasPrivilegeLevelAtLeast(const ES_Context* context, PrivilegeLevel privilege_level);
	/**< Checks if the specified context has the following property: each
	     activation record on its stack is associated with ES_Compiled_Code that
	     has Privilege Level equal to or greater than the specified priv_lvl.
	     This is used by:
	     - userjsmanager to check whether this context has atleast USERJS
	       privileges

	     @param context (in) (see above)
	     @param privilege_level (in) (see above)

	     @return (see above) */

	/** Extended fail status for StructuredClone(). */
	class CloneStatus
	{
	public:
		enum FaultReason
		{
			OK,
			/**< Cloning was succesful. */
			OBJ_IS_FOREIGN,
			/**< Unsupported cloning of foreign/host object. */
			OBJ_IS_FUNCTION,
			/**< Unsupported cloning of function values. */
			OBJ_HAS_GETTER_SETTER
			/**< Unsupported cloning of a getter/setter. */
		};

		FaultReason fault_reason;
		ES_Object *fault_object;

		CloneStatus()
			: fault_reason(OK),
			  fault_object(NULL)
		{
		}
	};

	/** Cloning transfer map, a set of objects with known
	    clone equivalents. */
	class CloneTransferMap
	{
	public:
		OpVector<ES_Object> source_objects;
		OpVector<ES_Object> target_objects;

	    /* The vectors must be of equal length, and all elements
	       non-NULL. Notice that the source objects belong
	       to another runtime from the target objects. */
	};

	OP_STATUS StructuredClone(ES_Object *source_object, ES_Runtime *target_runtime, ES_Object *&target_object, CloneTransferMap *transfer_map = NULL, CloneStatus *clone_status = NULL, BOOL uncloned_to_null = FALSE);
	/**< Make a structured clone of the supplied 'source_object' into
	     the given target runtime. See

	       http://www.whatwg.org/specs/web-apps/current-work/multipage/urls.html#structured-clone

	     for algorithm details on the cloning algorithm, but some
	     of its interesting properties are:

	        1. Referential integrity is preserved (if two properties refer to the
	           same object their clones will be referring to the same object.)
	        2. No custom prototypes and no custom properties in a prototype are preserved.
	        3. But custom properties in a prototype are copied into the clone itself.
	        4. Some host objects can be cloned, but most are unsupported, causing
	           the cloning to fail. i.e., unclonable objects are by default not mapped
	           to a null or undefined, but cause the cloning operation to fail. Previous
	           versions of the spec have used null, which is still supported through
	           the uncloned_to_null argument (set to TRUE.)

	     @param source_object The object to clone; either a native object or a host object.
	     @param target_runtime The runtime being cloned into; may not be the same as the source.
	     @param [out] target_object The reference to the resulting cloned object.
	            Partially cloned objects won't be observable if the cloning
	            process fails partway through.
	     @param transfer_map The cloning algorithm optionally takes a transfer
	            map, which is a mapping from source objects to their cloned
	            representation in the target runtime. Any occurences of the source
	            objects should be substituted for their target object counterparts
	            while cloning.
	     @param clone_status The extended context/status if cloning fails.
	     @param uncloned_to_null If TRUE, then map any object that does
	            not have a cloneable representation to 'null'. If uncloneable
	            objects are encountered with uncloned_to_null being FALSE,
	            the operation fails with OBJ_IS_FOREIGN.

	     @return OpStatus::OK on success, some error code otherwise
	             (ERR_NOT_SUPPORTED or ERR_NO_MEMORY.) In case ERR_NOT_SUPPORTED
	             is returned, the clone_status (if non-NULL) is filled in with
	             extended information on the reason of failure. */

	OP_STATUS Clone(const ES_Value &source_value, ES_Runtime *target_runtime, ES_Value &target_value, CloneTransferMap *transfer_map = NULL, CloneStatus *clone_status = NULL, BOOL uncloned_to_null = FALSE);
	/**< Create a deep clone of the source value into the target runtime.  Any
	     structured supported by the "structured cloning" algorithm defined by
	     HTML5 can be cloned.  See

	       http://www.whatwg.org/specs/web-apps/current-work/multipage/urls.html#structured-clone

	     for more information.

	     If the value is not supported, OpStatus::ERR_NOT_SUPPORTED is returned,
	     and the 'target_value' parameter is not modified.  If a CloneStatus
	     object is provided, it will contain additional information about the
	     reason of the failure.

	     @param source_value Value to clone.
	     @param target_runtime Runtime to clone into.  Can be the same as this
	                           runtime, if the cloned value should be used in
	                           the same runtime as the original value.

	     @param target_value Value set to clone result on success.
	     @param transfer_map The cloning algorithm optionally takes a transfer
	            map, which is a mapping from source objects to their cloned
	            representation in the target runtime. Any occurences of the source
	            objects should be substituted for their target object counterparts
	            while cloning.
	     @param clone_status Receives additional information on failure if not
	                         NULL.
	     @param uncloned_to_null If TRUE, objects that can't be cloned don't
	                             cause failure but are replaced by null values.

	     @return OpStatus::OK, OpStatus::ERR_NOT_SUPPORTED if the value can't be
	             cloned, or OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef ES_PERSISTENT_SUPPORT
	OP_STATUS CloneToPersistent(const ES_Value &source, ES_PersistentValue *&target, unsigned size_limit = 0);
	/**< Clone the 'source' value into a form suitable for persistent storage.
	     The persistent form is a simple array of unsigned integers, and can
	     moved in memory, stored on disk or even sent over the network, and
	     still be used to recreate the value.  Any structured value supported by
	     the "structured cloning" algorithm can be used.  If the value is not
	     supported, OpStatus::ERR_NOT_SUPPORTED is returned, and the 'target'
	     parameter is not modified.

	     When successful, the 'target' parameter is set to a pointer to a newly
	     allocated object and OpStatus::OK is returned.  The target value is
	     owned by the caller and needs to be freed using OP_DELETE(), or via
	     CloneFromPersistent() using its 'destroy' argument.

	     If 'size_limit' is not zero, it limits the size of the target value in
	     bytes.  If set and exceeded, OpStatus::ERR is returned.

	     @param source Source value.
	     @param target Target value.
	     @param size_limit Optional size limit.

	     @return OpStatus::OK, OpStatus::ERR_NOT_SUPPORTED, OpStatus::ERR or
	             OpStatus::ERR_NO_MEMORY. */

	OP_STATUS CloneFromPersistent(ES_PersistentValue *source, ES_Value &target, BOOL destroy = FALSE);
	/**< Recreate a regular value from persistent form.  If the 'destroy'
	     argument is TRUE the 'source' value is destroyed regardless of whether
	     the operation succeeds or not.

	     @param source Source value.
	     @param target Target value.
	     @param destroy If TRUE, destroy 'source' value.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */
#endif // ES_PERSISTENT_SUPPORT

	OP_STATUS ParseJSON(ES_Value &value, const uni_char *string, unsigned length = UINT_MAX);
	/**< Parse 'string' as JSON and store result in 'value'.  If the string is
	     not valid JSON, OpStatus::ERR is returned, and 'value' is not
	     modified.

	     @param value Set to parse result on success.
	     @param string String to parse.
	     @param length The input length in characters. If UINT_MAX,
	            length of the null terminated 'string' is computed.

	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS SerializeJSON(TempBuffer &buffer, const ES_Value &value);
	/**< Serialize 'value' into a JSON string and append to 'buffer'.  If the
	     value stored in 'value' is not possible to serialize, for instance if
	     it is a cyclic object structure, OpStatus::ERR is returned and 'buffer'
	     is not modified.

	     @param buffer Buffer to append JSON string to.
	     @param value Value to serialize.

	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	/** Common optional arguments to CompileProgram and CreateFunction. */
	struct CompileOptions
	{
		CompileOptions();
		/**< Constructor.  Sets all members to their documented default values. */

		PrivilegeLevel privilege_level;
		/**< The execution privilege level of the code that will be generated.
		     See enum PrivilegeLevel for possible values.  As built-ins are not
		     compiled through this API function, the legitimate values for this
		     parameter are UNTRUSTED, USERJS and BROWSERJS.

		     Default: PRIV_LVL_UNSPEC */

		BOOL prevent_debugging;
		/**< Some scripts used internally in Opera should not be seen from the
		     debugger.  For these scripts, set prevent_debugging to TRUE.
		     Otherwise, just leave it at TRUE.

		     Default: FALSE */

		BOOL report_error;
		/**< If TRUE then compilation errors are reported to window.onerror
		     and/or to the console.  The behavior of the call is otherwise
		     unaffected, including return value.

		     Default: TRUE */

		BOOL allow_cross_origin_error_reporting;
		/**< If TRUE, then any errors during parsing and compiling this script
		     should reported with full details to the toplevel error handler.
		     If FALSE, then only scripts within the same origin as the runtime
		     will see full details (error message and URL of the script.)

		     Default: FALSE */

		BOOL reformat_source;
		/**< If TRUE then the script source is reformatted before being parsed
		     and compiled.
		     Reformatting adds whitespace for better readability and make
		     debugging of minified scripts easier.

		     Default: FALSE */

		URL *script_url;
		/**< URL of script compiled.  For a script that is part of another
		     resource, such as a SCRIPT element without a SRC attribute, this
		     should be the URL of the containing HTML file.  If the script is
		     not linked to a specific resource, set to NULL.

		     If not NULL, 'script_url_string' is ignored.

		     Default: NULL */

		const uni_char *script_url_string;
		/**< URL of script compiled.  Similar to 'script_url' and ignored if
		     'script_url' is not NULL.

		     Default: NULL */

		ES_ScriptType script_type;
		/**< Type of script compiled.

		     Default: SCRIPT_TYPE_UNKNOWN */

		const uni_char *when;
		/**< Error message addition.  Appended to the string "Syntax error at
		     line N " unless NULL.  Typically a string like "while loading".

		     Default: NULL */

		const uni_char *context;
		/**< Stored in OpConsoleEngine::Message::context if an error is reported
		     to the console.

		     Default: "Script compilation" */

		typedef OP_STATUS (*ErrorCallback)(const uni_char *message, const uni_char *full_message, const ES_SourcePosition &position, void *user_data);
		/**< Callback function for compilation errors.

		     @param message One-line error message.
		     @param line Line where error occurred.
		     @param offset Offset into source code where error occurred.
		     @param user_data Whatever, or NULL.
		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

		ErrorCallback error_callback;
		/**< If non-NULL, called if a compilation error occurs.  If this
		     function returns OpStatus::ERR_NO_MEMORY then CompileProgram() does
		     too, otherwise CompileProgram() returns OpStatus::ERR.

		     Default: NULL */

		void *error_callback_user_data;
		/**< Used if 'error_callback' is set and called, as the 'user_data'
		     argument.  Otherwise ignored.

		     Default: NULL */
	};

	/** Option-arguments to CompileProgram */
	struct CompileProgramOptions : CompileOptions
	{
		CompileProgramOptions();
		/**< Constructor.  Sets all members to their documented default values. */

		BOOL program_is_function;
		/**< If TRUE then a 'return' statement is allowed at the top level.

		     Default: FALSE */

		BOOL generate_result;
		/**< If TRUE then the "statement result" of the program will be
		     retained, to be read by ES_Runtime::GetReturnValue() after
		     execution has finished successfully.

		     Default: FALSE */

		BOOL global_scope;
		/**< If TRUE then the program must be run with no extra scope objects
		     (the scope_length parameter to PushProgram must be 0).  It is
		     useful to specify this argument as TRUE when you can, as it will
		     allow the compiler to optimize the program better.

		     Default: TRUE */

		BOOL is_external;
		/**< If TRUE the script source code is known to come from an external
		     source, typically a linked script.  This bit is for plugin
		     activation later (#EOLAS patent circumvention).  It must *NEVER* be
		     TRUE if the script does not come from an external source.

			 Default: FALSE */

		BOOL allow_top_level_function_expr;
		/**< If TRUE a function expressions are allowed at the top level.  Used
		     for some internal scripts, e.g., scope selftests.

		     Default: FALSE */

		BOOL is_eval;
		/**< If TRUE then the program is compiled as if it was an eval call.
		     Eval calls use additional symbol lookups from a suspended context
		     (if supplied) to find symbols not in the current scope.  This is
		     used by the Eval calls from the debugger.

		     Default: FALSE */

		unsigned start_line;
		/**< The line number of the source file where this script starts.
		     Will usually be 1, unless it's an inline script.

		     Default: 1 */

		unsigned start_line_position;
		/**< The character position in the line where this script starts.
		     Will usually be 0, unless it's an inline script.

		     Default: 0 */
	};

	OP_STATUS CompileProgram(ES_ProgramText* program_array, int elements, ES_Program** return_program, const CompileProgramOptions& options);
		/**< Compile the program in the context of this runtime and return its compiled
		     representation.

			 @param program_array (in) an array of program text segments
			 @param elements (in) the length of program_array, must be nonzero
			 @param return_program (out) a location in which to store the newly allocated program
			 @param options (in) see class CompileProgramOptions for details on specific options.

			 @return OpStatus::OK, OpStatus::ERR (on syntax errors,) or OpStatus::ERR_NO_MEMORY (on OOM.) */

	OP_STATUS ExtractStaticProgram(ES_Static_Program *&static_program, ES_Program *program);
		/**< Extract a static version of the program that can be used to execute the same code in different runtimes
		     in the future without recompiling from source code. */

	OP_STATUS CreateProgramFromStatic(ES_Program *&program, ES_Static_Program *static_program);
		/**< Prepare am executable program from a static program previous extracted from another executable program. */

	static void DeleteStaticProgram(ES_Static_Program *static_program);
		/**< Free the resources that are referenced by the static program. */

	/** Option-arguments to CreateFunction */
	struct CreateFunctionOptions : CompileOptions
	{
		CreateFunctionOptions();
		/**< Constructor.  Sets all members to their documented default values. */

		BOOL generate_result;
		/**< If TRUE then the "statement result" of the function will be
		     retained, fto be read by GetReturnValue().

		     Default: FALSE */

		BOOL is_external;
		/**< If TRUE the script source code is known to come from an external
		     source, typically a linked script.  This bit is for plugin
		     activation later (#EOLAS patent circumvention).  It must *NEVER* be
		     TRUE if the script does not come from an external source.

		     Default: FALSE */
	};

	OP_STATUS CreateFunction(ES_Object** scope_chain, int scope_chain_length, const uni_char* function_text, unsigned function_text_length, ES_Object** return_object, unsigned argc, const uni_char** argn, const CreateFunctionOptions& options);
		/**< Compile the function in the context of this runtime and a lexical scope,
		     and return its compiled representation as a native object.

			 @param scope_chain (in) an array of native objects, where scope_chain[0]
			        is the outermost scope object and scope_chain[scope_chain_length-1]
			        is the innermost.  NULL elements are ignored.
			 @param scope_chain_length (in) the number of elements in scope_chain
			 @param function_text (in) the text of the body of the function
			 @param function_text_length (in) the lenght of function_text
			 @param return_object (out) a location in which to store the compiled function
			 @param argc (in) the number of formal parameters, also the length of argn
			 @param argn (in) the names of the formal parameters
			 @param options (in) see class CreateFunctionOptions for details on specific options.

			 @return OpStatus::OK, OpStatus::ERR (on syntax errors,) or OpStatus::ERR_NO_MEMORY (on OOM.) */

    OP_STATUS CreateNativeObject(ES_Value value, ES_Prototype tag, ES_Object** obj);
        /**< Create a standard native object with the given type and value.
             @param value (in) the value to store.
             @param tag (in) the type of object created.
             @param obj (out) a location in which to store the result.
             @return OK if *obj has a value; ERR for illegal tags or values; ERR_NO_MEMORY for OOM.

             The interpretation of 'value' is relative to 'tag', in the following way:

             ENGINE_NUMBER_PROTOTYPE     value.value.number, as number   <br>
             ENGINE_STRING_PROTOTYPE     value.value.string, must be null-terminated  <br>
             ENGINE_BOOLEAN_PROTOTYPE    value.value.boolean, must be TRUE or FALSE  <br>
             ENGINE_DATE_PROTOTYPE       value.value.number, as milliseconds since 01-01-1970 00:00:00:00 UTC  <br>

             No other tags are supported at this time.  */

	OP_STATUS CreateNativeObject(ES_Object **obj, ES_Object *prototype, const char *class_name, unsigned flags = 0);
		/**< Create a native object, to use as a host prototype, with given
		     prototype and class name. */

	OP_STATUS CreatePrototypeObject(ES_Object **obj, ES_Object *prototype, const char *class_name, unsigned size);
		/**< Create a native object, to use as a host prototype, with given
		     prototype and class name.  The object and it's class will be
		     "optimized" for having 'size' properties, but it's not really
		     critical that 'size' is accurate. */

	OP_STATUS CreateNativeObjectObject(ES_Object** obj);
		/**< Create a native Object object
		     @param obj (out) a location in which to store the result.
		     @return OK if *obj has a value; ERR_NO_MEMORY for OOM.

		     Once created, you can manipulate object's properties by the [Get/Put/Delete][Index/Name] set of methods
		     */

	OP_STATUS CreateNativeArrayObject(ES_Object** obj, unsigned len=0);
		/**< Create a native Array object
		     @param obj (out) a location in which to store the result.
		     @param len (in) 'length' property of the created array object.
		     @return OK if *obj has a value (ES_Array upcasted to ES_Object); ERR_NO_MEMORY for OOM.

		     Once created, you can manipulate array's properties by the [Get/Put/Delete][Index/Name] set of methods
		     */

	OP_STATUS CreateNativeErrorObject(ES_Object **obj, ES_NativeError type, const uni_char *message);
		/**< Create a native ES Error object.
		     @param obj[out] where to store the resulting object.
		     @param type which one of the ES native errors to create.
		     @param message message further describing the error.
		     */

	OP_STATUS CreateErrorPrototype(ES_Object **prototype, ES_Object* prototype_prototype, const char *class_name);
		/**< Create an Error object's prototype.
		     @param prototype (out) where to store the resulting prototype object.
		     @param prototype_prototype (in) the prototype of this prototype. NULL if the global Error object prototype.
		     @param class_name (in) unique class name for the kind of error this represents.
		     */

	OP_STATUS CreateErrorObject(ES_Object **obj, const uni_char *message, ES_Object *prototype = NULL);
		/**< Create a native Error object.
		     @param obj[out] where to store the resulting object.
		     @param message message further describing the error (included when error is converted to a string.)
		     @param prototype prototype of error object; if NULL, it defaults to Error.
		     */

	static unsigned char* GetByteArrayStorage(ES_Object* obj);
		/**< Returns a pointer to the byte array's underlying storage.
		     The object 'obj' must be an ArrayBuffer.
		     */

	static unsigned GetByteArrayLength(ES_Object* obj);
		/**< Returns the byte length of the static byte array.
		     The object 'obj' must be an ArrayBuffer.
		     */

	enum NativeTypedArrayKind
	{
		TYPED_ARRAY_INT8,
		TYPED_ARRAY_UINT8,
		TYPED_ARRAY_UINT8CLAMPED,
		TYPED_ARRAY_INT16,
		TYPED_ARRAY_UINT16,
		TYPED_ARRAY_INT32,
		TYPED_ARRAY_UINT32,
		TYPED_ARRAY_FLOAT32,
		TYPED_ARRAY_FLOAT64,

		TYPED_ARRAY_ANY
		/**< Used for argument checking of DOM functions taking an ArrayBufferView.
		     Corresponds to any typed array (including DataView).
		     */
	};
	OP_STATUS CreateNativeArrayBufferObject(ES_Object** object, unsigned length, unsigned char *bytes = NULL);
		/**< Create a native ArrayBuffer object
		     @param object (out) location where to store the result.
		     @param length The byte length of the ArrayBuffer instance.
		     @param bytes The contents of the new array buffer. If the
		            creation of new instance succeeds, the object will take
		            ownership of this data. The external storage must have
					been allocated using op_malloc() / op_realloc().
		     @return OK if *object has a value; ERR_NO_MEMORY for OOM.
		     */

	OP_STATUS CreateNativeTypedArrayObject(ES_Object** object, NativeTypedArrayKind kind, ES_Object* data);
		/**< Create a native "typed array" object, with given array buffer object as underlying storage.
		     @param object (out) location where to store the result.
		     @param kind The kind of TypedArray to create.
		     @param data An ArrayBuffer to use as a backing store for the TypedArray.
		     @return OK if *object has a value; ERR_NO_MEMORY for OOM.
		     */

	static unsigned char *GetArrayBufferStorage(ES_Object* obj);
		/**< Returns a pointer to the array buffer's underlying storage.
		     The object 'obj' must be an ArrayBuffer.
		     */

	static unsigned GetArrayBufferLength(ES_Object* obj);
		/**< Returns the byte length of the array buffer.
		     The object 'obj' must be an ArrayBuffer.
		     */

	static BOOL IsNativeArrayBufferObject(ES_Object* object);
		/**< Check if an object is a native ArrayBuffer.
		     @param object the object to check.
		     @return TRUE if the object is a native ArrayBuffer, FALSE otherwise. */

	static BOOL IsCallable(ES_Object* object);
		/**< Check if the object is callable, like a function.
		     @param object the object to check.
		     @return TRUE if the object is callable, FALSE otherwise. */

	static BOOL IsNativeTypedArrayObject(ES_Object* object, NativeTypedArrayKind kind = TYPED_ARRAY_ANY);
		/**< Check if an object is a native TypedArray of the specified kind.
		     @param object the object to check.
		     @param kind the type of TypedArray to check for, or any to just check if it is a typed array at all.
		     @return TRUE if the object is a typed array of the specified kind, FALSE otherwise. */

	static unsigned GetStaticTypedArraySize(ES_Object* object);
		/**< Returns the size of the static typed array.
		     The object 'object' must be a static typed array. */

	static unsigned GetStaticTypedArrayLength(ES_Object* object);
		/**< Returns the byte length of the static typed array.
		     The object 'object' must be a static typed array. */

	static void *GetStaticTypedArrayStorage(ES_Object* object);
		/**< Returns a pointer to the typed array's underlying storage.
		     The object 'object' must be a static typed array. */

	static ES_Object *GetStaticTypedArrayBuffer(ES_Object* object);
		/**< Returns the array buffer of the static typed array.
		     The object 'object' must be a static typed array;
		     the returned object is a native array buffer. */

	static NativeTypedArrayKind GetStaticTypedArrayKind(ES_Object* object);
		/**< Returns the kind of the static typed array.
		     The object 'object' must be a static typed array. */

	static void TransferArrayBuffer(ES_Object* source, ES_Object *target);
		/**< Transfer the ArrayBuffer contents of 'source' to 'target',
		     emptying 'source'. The underlying storage for 'target' is
		     reassigned to 'source'. Both objects must be array buffer
		     objects. */

	static BOOL IsStringObject(ES_Object* obj);
	/**< Check if the object is an ecmascript String object.
	     @param object the object to check.
	     @return TRUE if the object is a string, FALSE otherwise. */

	static BOOL IsFunctionObject(ES_Object* obj);
	/**< Check if the object is a function or constructor, either a built-in or ecmascript one.
	     @param object the object to check.
	     @return TRUE if the object is a function, FALSE otherwise. */

	OP_STATUS PutInGlobalObject(EcmaScript_Object* object, const uni_char* property_name);
		/**< Store the native object of 'object' in the global-object of this runtime,
			 in the property named by 'property_name'.
			 */

    OP_STATUS PutInGlobalObject(ES_Object* object, const uni_char* property_name);
        /**< Store the native object 'object' in the global-object of this runtime, in
		    the property named by 'property_name'.
		    */

	ES_Context* CreateContext(ES_Object* this_object, BOOL prevent_debugging=FALSE);
		/**< Create a fresh execution context in the context of this runtime.

			 FIXME  Protocol: this_object should be pushed as part of PushCall or PushProgram,
			        not here.

			 @param this_object (in) if not NULL, this_object is pushed onto the this stack
								of the new context.
			 @param prevent_debugging (in) if TRUE it will prevent the debugger (if any)
			                          from knowing of the new context object.
			 @return the new context, or NULL on OOM.
		 	 */

	void MigrateContext(ES_Context* context);
		/**< Migrate the context from its current runtime to this runtime.

			 @param context (in) context to migrate. */

    OP_STATUS PushProgram(ES_Context* context, ES_Program* program, ES_Object** scope=NULL, int scope_length=0);
		/**< Clear the execution context (apart from the 'this' stack) and push the
			 program.

			 FIXME  Protocol: not clearing the this stack is a bug
					see comments in ecmascript_engine.h

			 @param scope (in) an array of objects to push onto the scope in which the program executes.
						  NULL elements are ignored.
			 @param scope_length (in) the length of 'scope'
			 */

    OP_STATUS SignalExecutionEvent(unsigned int ev);
        /**< For interal use.  Decode the event code and call a virtual method on this
             object to signal some sort of execution event.  */

	OP_STATUS GetNativeValueOf(ES_Object *obj, ES_Value *return_value);
		/**< Get the primitive value representing this native ES object. In EcmaScript specification terms,
		     returning the [[PrimitiveValue]] of the native object (if defined, see Section 8.6.2 of ES5 spec, table 9.).
		     @param obj (in) native object you want to inspect.
		     @param value (out) location where you want to store the result.

		     @return ERR if either 'obj' or 'value' aren't valid.
		             OK if 'obj' is a native object with a [[PrimitiveValue]] property; ERR otherwise.
		     */

#ifdef USER_JAVASCRIPT

    virtual OP_STATUS OnAfterGlobalVariablesInitialized();
        /**< Called following initialization of global properties for a program,
             when functions have been defined and variables have been given
             undefined values.  No part of the user program will have run at this
             point.  */

#endif

	static ES_Object* CreateHostObjectWrapper(EcmaScript_Object* host_object, ES_Object* prototype, const char* object_class, BOOL phase2=FALSE);
		/**< Create a native object that references a host object.

			 @param object (in) the host object to be wrapped
			 @param prototype (in) the prototype object to install in the new native object
			 @param object_class (in) the class name to use in the new native object
			 @param phase2 (in) if TRUE then the new host object is to be deleted in the second
					       phase of garbage collection.  This is usually only the case for
						   objects that are used during deletion of other objects.
			 @return the new native object, or NULL on OOM.
			 */

	static ES_Object* CreateHostFunctionWrapper(EcmaScript_Object* host_object, ES_Object* prototype, const uni_char* function_name, const char* object_class, const char* parameters);
			/**< Create a native function object that references a host object.

			   @param prototype			  The new object's prototype object
			   @param host_object		  A pointer to a host object, or NULL
			   @param function_name		  The name of the function (for printing)
			   @param object_class		  The name of the new object's class
			   @param parameters          A string that encodes the parameter types of the foreign function,
                                          or NULL.  The actual parameters are converted to the indicated types
										    n	convert argument to number
										    s	convert argument to string
										    b	convert argument to boolean
										    -	no conversion
										  So a string containing "-sb" will convert the second argument (if
										  supplied) to string and the third (ditto) to boolean, and leave the
										  first (ditto) unconverted.  Arguments after the third will also
										  be passed through unconverted.
			   @return                    The new object, or NULL on OOM
		 */

	static void	DeleteProgram(ES_Program* program);
		/**< Destroy the program. */

	static void	DecoupleHostObject(ES_Object* object);
		/**< Clear the object's host object pointer.  This can only be done if the host
		     object pointer is already NULL or if in turn its native object pointer is
			 NULL or about to be set to NULL.  (See "Invariant 1", above).
			 */

	static void	DeleteContext(ES_Context* context);
		/**< Delete the context and free its resources. */

	static ES_Eval_Status ExecuteContext(ES_Context* context, BOOL want_string_result = FALSE, BOOL want_exceptions = FALSE, BOOL allow_cross_origin_errors = FALSE, BOOL (*external_out_of_time)(void *) = NULL, void *external_out_of_time_data = NULL);
	/**< Execute the program installed in the context a while.

	     @param context (in) a non-executing context which has not been run since the last
	            ES_PushCall or ES_PushProgram
	     @param want_string_result (in) if TRUE, then the result of the computation is converted
	            to string before being returned
	     @param want_exceptions (in) if TRUE, then ES_THREW_EXCEPTION may be returned from
	            this method; otherwise an uncaught exception is handled by
	            printing an error in the console.
	     @param allow_cross_origin_errors (in) if TRUE, allow runtime errors to be reported in full
	            irrespective of origin of script and this runtime.
		 */

	static void	RequestTimeoutCheck(ES_Context* context);
		/**< Signal the context that it should do a timeout-check when it receives control. */

	static void	SuspendContext(ES_Context* context);
		/**< Signal the context that it should suspend execution when it receives control. */

#ifdef ECMASCRIPT_DEBUGGER
	static BOOL HasDebugInfo(ES_Program *program);
		/**< Checks if the program has debug information.
			 @return TRUE if it has, FALSE otherwise. */

	static BOOL IsContextDebugged(ES_Context *context);
		/**< Checks if the specified context is being debugged.
			 @return TRUE if it has, FALSE otherwise. */
#endif // ECMASCRIPT_DEBUGGER

    static OP_STATUS PushCall(ES_Context* context, ES_Object* function, int argc=0, const ES_Value* argv=NULL);
		/**< Clear the execution context (apart from the 'this' stack) and push a
		     call to the function with the given arguments.

			 FIXME  Protocol: not clearing the this stack is a bug
			        see comments in ecmascript_engine.h
			 */

	static OP_STATUS GetReturnValue(ES_Context* context, ES_Value* return_value);
		/**< Retrieve the final value of an execution, following a call to ExecuteContext
			 that produced a value.
			 */

    static BOOL GetIsExternal(ES_Context* context);
        /**< \return the value of the is_external flag of the topmost native function on the
                     context's stack.  */

	static BOOL GetIsInIdentifierExpression(ES_Context* context);
		/**< \return whether we're currently setting a variable in an
		   identifier expression, as opposed to changing a property on
		   an object. Needed to see the difference between things like
		   "name=0" and "window.name=0".
			*/

	/** Structure returned by ES_Runtime::GetCallerInformation(). */
	struct CallerInformation
	{
		const uni_char* url;
		/**< URL of calling script, or NULL if not known. */

		PrivilegeLevel privilege_level;
		/**< Privilege level of calling code, or PRIV_LVL_UNSPEC if not
		     known. */

		unsigned script_guid;
		/**< Globally unique script serial number. Zero means "no
		     information". */

		unsigned line_no;
		/**< The line number of the call, relative to the script source
		     (not the enclosing document, if if any). Zero means "no
		     information". */
	};

	static OP_STATUS GetCallerInformation(ES_Context* context, CallerInformation& call_info);
	/**< Returns information about the current top stack frame for the context.
	     Requesting this information is only useful or valid during a call out
	     to a host object API function such as GetName() or Call().

	     @param context (in) The execution context.
	     @param call_info (out) The CallerInformation structure to store the
	                            information in.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	static OP_STATUS GetErrorStackCallerInformation(ES_Context* context, ES_Object* error_object, unsigned stack_position, BOOL resolve, CallerInformation& call_info);
	/**< Returns information for the given position in the stack trace of an exception object.

	     @param context (in) The execution context.
	     @param error_object The thrown exception..
	     @param stack_position Position of stack element, 0 is the top most one.
	     @param resolve If TRUE, the line number is resolved against the document,
	                    else if FALSE the line number returned is relative to the
	                    script itself.
	     @param call_info (out) The CallerInformation structure to store the
	                            information in.
	     @retval OpStatus::OK if the information was successfully retrieved
	             OpStatus::ERR_NO_MEMORY in case of OOM
	             OpStatus::ERR if the stack or the requested stack position do not exist. */

	OP_BOOLEAN GetIndex(ES_Object* object, unsigned index, ES_Value* value);
		/**< Lookup a property by index in the object and return its value.
			 @return IS_TRUE if the property was found, otherwise IS_FALSE.
			 */

	OP_STATUS PutIndex(ES_Object* object, unsigned index, const ES_Value& value);
		/**< Lookup a property by index in the object and set its value */

	OP_BOOLEAN GetName(ES_Object* object, const uni_char* property_name, ES_Value* value);
		/**< Lookup a property by name in the object and return its value.
			 @return IS_TRUE if the property was found, otherwise IS_FALSE.
			 */

	OP_STATUS PutName(ES_Object* object, const uni_char* property_name, const ES_Value& value, int flags=0);
		/**< Lookup a property by name in the object and set its value */

	OP_STATUS DeleteName(ES_Object* object, const uni_char* property_name, BOOL force = FALSE);
		/**< Delete a property by name in the object.  If force==TRUE then delete it even if it
			 is marked as DONT_DELETE. */

	BOOL IsSpecialProperty(ES_Object* object, const uni_char* property_name);
		/**< Check if property is considered "special" by the engine. Host code must exercise
			 care if trying to update special properties on a native object. */

	static EcmaScript_Object* GetHostObject(ES_Object* object);
		/**< @return the host object associated with the native object 'object' */

	static ES_Object* Identity(ES_Object* object);
	/**< If 'object' is a proxy for another object, return the object being
	     proxied. Otherwise, return the parameter 'object'.

	     The function will unpack all proxy levels, in other words: this
	     function is guaranteed to not return another proxy object.

	     @param object The suspected proxy object. NULL is not allowed.
	     @return The object being proxied (if 'object' is a proxy); 'object'
	             (if 'object' is not a proxy); or NULL on OOM. */

	OP_STATUS SetPrototype(EcmaScript_Object* object, ES_Object* prototype);
		/**< Set the prototype object of the native object associated with 'object' to 'prototype'. */

	static const char* GetClass(ES_Object* object);
		/**< @return the class name of the native object, as a pointer to volatile storage */

	BOOL GCMark(EcmaScript_Object *obj, BOOL mark_host_as_excluded=FALSE);
		/**< Mark the native object of this object (if any) as live for the purposes
		     of garbage collection.  May only be called while a call to
			 EcmaScript_Object::GCTrace is active on an object on the same heap as 'obj'.
			 @param obj (in) the object to mark
			 @param mark_host_as_excluded (in) if TRUE, then the host object of the native
										  object of obj, if any (normally the same as obj
										  itself), will be marked so it won't be traced again
										  during this collection cycle.  Use with caution.
			 @return      TRUE if the object was already marked, FALSE otherwise
			 */

	static void GCExcludeHostFromTrace(EcmaScript_Object *obj);
		/**< WITHOUT MARKING the native object of this object (if any) as live for the
			 purposes of garbage collection, set the flag in it that tells the garbage
			 collector not to trace the host object.  May only be called while a call to
			 EcmaScript_Object::GCTrace is active.
			 @param obj (in) the object to mark
			 @param mark_host_as_excluded (in) if TRUE, then the host object of the native
										  object of obj, if any (normally the same as obj
										  itself), will be marked so it won't be traced again
										  during this collection cycle.  Use with caution.
			 @return      TRUE if the object was already marked, FALSE otherwise
			 */

    BOOL GCMark(ES_Object *obj, BOOL mark_host_as_excluded=FALSE);
		/**< Mark the object as live for the purposes of garbage collection.
		     May only be called while a call to EcmaScript_Object::GCTrace is active on
		     an object on the same heap as 'obj'.  If you're unsure, talk to experts or
		     use GCMarkSafe() instead.
			 @param obj (in) the object to mark
			 @param mark_host_as_excluded (in) if TRUE, then the host object of obj, if any,
										  will be marked so it won't be traced again
										  during this collection cycle.  Use with caution.
			 @return      TRUE if the object was already marked, FALSE otherwise
			 */

	void GCMarkRuntime();
		/**< Mark the runtime itself as live; use with considerable care to keep a
		     runtime and its references alive. Does not return a result. */

    BOOL GCMarkSafe(ES_Object *obj);
		/**< Mark the object as live for the purposes of garbage collection.
		     May only be called while a call to EcmaScript_Object::GCTrace is active.
		     If 'obj' is not allocated on the heap that is currently being GC:ed, this
		     function does nothing (unlike GCMark() which will do something that is
		     wrong and eventually leads to crashes.)
			 @param obj (in) the object to mark
			 @return      TRUE if the object was already marked, FALSE otherwise
			 */

	OP_BOOLEAN GetPrivate(ES_Object* object, int private_name, ES_Value* value);
		/**< Read a private property of 'object', storing the result in 'value'.
			 @return IS_TRUE if the property was read, otherwise IS_FALSE.
			 */

	OP_STATUS PutPrivate(ES_Object* object, int private_name, const ES_Value& value);
		/**< Write a private property of 'object' with 'value'.
			 @return IS_TRUE if the property could be written, otherwise IS_FALSE.
			 */

	BOOL Protect(ES_Object *obj);
		/**< Protect the object from being garbage collected even if there are no
		     references to it from within the collected heap.  If called several
			 times for the same object, the effect is to require a matching number
			 of Unprotect calls before the object becomes collectible -- effectively,
			 a reference counting scheme.
			 @param  obj  The object to be protected
			 @return      FALSE on OOM, otherwise TRUE */

	void Unprotect(ES_Object *obj);
		/**< Allow the object to be garbage collected.
			 @param  obj  The object to be unprotected
			 */

	OP_STATUS DeletePrivate(ES_Object* object, int private_name);
		/**< Delete a private property of 'object'. */

	static void MarkObjectAsHidden(ES_Object *obj);
		/**< Mark this object as 'hidden'. e.g. - document.all (see bug 148144). */

    static void SetIsCrossDomainAccessible(ES_Object *obj);
		/**< Mark this object as having properties that the engine operations
		     [[HasProperty]] and [[Get]] should allow to go ahead security
		     wise, even across domains.

		     This applies to native properties and if 'obj' is a host object,
		     its host properties. However, in the latter case, the host
		     object can impose separate security checks on access and deny.

		     Naturally, before marking an object like this, very careful
		     considerations of the security implications are in order.

		     [It is currently only used for Location.prototype. */

    static void SetIsCrossDomainHostAccessible(EcmaScript_Object *obj);
		/**< Mark this object as having host properties that are cross-domain
		     accessible.

		     If an object has as prototype a host object marked with
		     SetIsCrossDomainHostAccessible(), the engine operation
		     [[HasProperty]] will allow the lookup of a property on the
		     prototype, even if the same access on the object wasn't
		     allowed. The security check and handling is delegated to the
		     host object prototype instead.

		     This is provided to allow an object's prototype to expose
		     cross-domain properties and methods, while imposing the standard
		     security checks on access to the object itself.

		     Like SetIsCrossDomainAccessible(), this operation alters
		     security policy and must only be used if you've carefully
		     considered the consequences and alternatives.

		     [It is currently only used for Window.prototype.] */

	ES_ThreadScheduler *GetESScheduler();
		/**< The thread scheduler that is associated with this runtime, or NULL
			 if none is. */

	void SetESScheduler(ES_ThreadScheduler* s) { scheduler = s; }
		/**< Set the thread scheduler that is associated with this runtime.
			 Need only be set for runtimes not associated with a document, and
			 should only be set by the runtime's owner. */

	ES_AsyncInterface *GetESAsyncInterface();
		/**< The asynchronous interface object that is associated with this
			 runtime, or NULL if none is. */
	void SetESAsyncInterface(ES_AsyncInterface* ai) { async_interface = ai; }
		/**< Set the asynchronous interface object that is associated with this
			 runtime.  Need only be set for runtimes not associated with a
			 document, and should only be set by the runtime's owner. */

	/**< Callback for handling compilation errors and uncaught runtime errors
	     before they are reported to the error console. */
	class ErrorHandler
	{
	public:
		enum ErrorType { LOAD_ERROR, COMPILATION_ERROR, RUNTIME_ERROR };

		class ErrorInformation
		{
		protected:
			ErrorInformation(ErrorType type)
			    : type(type),
			      cross_origin_allowed(FALSE)
		{
		}

		public:
			ErrorType type;

			BOOL cross_origin_allowed;
			/**< If FALSE, do not include message and url in the error handler invocation
			     if the runtime is in a different origin to that of the error's url.
			     If TRUE, include message and url information irrespective of origin. */
		};

		class CompilationErrorInformation
			: public ErrorInformation
		{
		public:
			CompilationErrorInformation() : ErrorInformation(COMPILATION_ERROR) {}
			ES_ScriptType script_type;
		};

		class RuntimeErrorInformation
			: public ErrorInformation
		{
		public:
			RuntimeErrorInformation() : ErrorInformation(RUNTIME_ERROR) {}
			ES_Value exception;
		};

		class LoadErrorInformation
			: public ErrorInformation
		{
		public:
			LoadErrorInformation() : ErrorInformation(LOAD_ERROR) {}
		};

		virtual OP_STATUS HandleError(ES_Runtime *runtime, const ErrorInformation *information, const uni_char *msg, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data) = 0;
		/**< Called before an uncaught exception is reported in the error
		     console.

		     If the callback doesn't wish to consume the error, and instead want
		     it to be reported in the error console, it should call
		     ReportErrorToConsole() with the 'data' argument.  If the callback
		     wishes to consume the error, it should should call IgnoreError()
		     instead.

		     One of ReportErrorToConsole() and IgnoreError() must be called, or
		     information stored in the 'data' argument is leaked.  They can be
		     called directly from this function or at a later point.  If this
		     function fails due to OOM, it need not and must not call either;
		     the 'data' argument is then released by the caller.

		     @param runtime The runtime.
		     @param information The structure holding error-specific details. Not NULL.
		     @param msg A one-line error message. Not NULL.
		     @param url Absolute URL of the resource in which the error
		                occurred or NULL if unknown.
		     @param position The location of the error. All zero if unknown.
		     @param data Opaque data structure containing information used by
		                 the engine to report the error to the console if the
		                 callback doesn't consume it.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY on OOM. */

	protected:
		virtual ~ErrorHandler() {}
		/**< The callback is never deleted through this interface. */
	};

	void SetErrorHandler(ErrorHandler *callback);
	/**< Set an external handler for uncaught compilation or runtime errors.
	     The callback is consulted before errors are reported to the console,
	     and can suppress them.  The callback is not owned by ES_Runtime.  To
	     unregister the callback, call this function again with a NULL argument.

	     @param callback Callback to install.  The object must remain valid
	                     until the runtime is detached or the callback
	                     unregistered. */

	ErrorHandler *GetErrorHandler();
	/**< Retrieve previously installed handler for uncaught errors.

	     @return Handler or NULL if one isn't installed. */

	void ReportErrorToConsole(const ES_ErrorData *data);
	/**< Report error to the error console.  This deallocates 'data' which must
	     not be used further.

	     @param data Error information to report. */

	void IgnoreError(const ES_ErrorData *data);
	/**< Deallocate 'data' without reporting to the error console.

	     @param data Error information to deallocate. */

	OP_STATUS  GetPropertyNames (ES_Object* object, OpVector<OpString>& propnames);
	/**< API for enumerating ES_Object properties; Should yield the same results as for (p in object) ...
	     Collects property names (as new OpString(s) which caller should delete)
		 in the supplied propnames vector.
	     See bug #362509 for details.
	*/

	void       GetPropertyNamesL(ES_Object* object, OpVector<OpString>& propnames);
	/**< Same as above but leaves in case of OOM (instead of returning ERR_NO_MEMORY)
	*/

	ESRT_Data *GetRuntimeData();

	ES_Runtime *&GetNextRuntimePerHeap();
	ES_Heap *GetHeap();
	void SetHeap(ES_Heap *h);

	BOOL InGC();
	/**< Returns TRUE if this runtime's heap is currently being garbage collected. */

	BOOL IsSameHeap(ES_Runtime *runtime);
	/**< Returns TRUE if given runtime shares the same heap.
	*/

	void RecordExternalAllocation(unsigned bytes);
	/**< Record 'bytes' as allocated by a host object residing in
	     this runtime's heap. Host objects with significant allocations
	     outside their GC heap allocation can improve garbage collection
	     behavior by recording such allocations with the runtime --
	     a heap with a larger set of heavy recorded host objects
	     will be collected more often than one having none recorded.
	     Heavy host objects that become unused will be garbage collected
	     sooner.

	     A runtime's heap keeps a counter of the external allocations recorded.
	     This counter is reset and recomputed at each garbage collection, so
	     a host object must record its external allocation from its tracing
	     method (GCTrace) each time around. It makes good sense to record
	     it upon host object creation also.
	*/

#if defined(_DEBUG) || defined(SELFTEST)
	void DebugForceGC();
	/**< Hook into some inner parts of the ecmascript engine to allow tests to control the timing
	     of things. Would be a very stupid thing if this was called from non-test-code.
	*/
#endif // _DEBUG || SELFTEST
#ifdef ECMASCRIPT_DEBUGGER

	virtual const char *GetDescription() const;
		/**< @return A description for the runtime. The default description is "unknown",
		             but the subclass may override with more useful descriptions, such as
		             "document", or "worker". */

	virtual OP_STATUS GetWindows(OpVector<Window> &windows);
		/**< Get zero or more Windows associated with this runtime. By default, a runtime
		     is not associated with a Window. It is up to the subclass to make this
		     association.

		     A runtime can be associated with more than one window, for example in the case
		     of shared WebWorkers.

		     @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS GetURLDisplayName(OpString &str);
		/**< Get the display name of the URL associated with this runtime, if any. By default,
		     a runtime is  not associated with any URL. It is up to the subclass to make this
		     association. (If there is no URL associated, the string will be empty).

		     @param str [out] The OpString to store the URL in.
		     @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY. */

# ifdef EXTENSION_SUPPORT
	virtual OP_STATUS GetExtensionName(OpString &extension_name);
		/**< Get the name of the extension associated with this runtime, if any.

		     A DOM_Runtime of the TYPE_EXTENSION_JS type has an associated
		     extension whose name can be obtained through this method.

		     @param[out] extension_name The OpString to store the extension name in.
		     @return OpStatus::OK if this is an extension runtime and the extension
		             name was set successfully (the provided OpString is emptied
		             if the extension doesn't have a name).
		             OpStatus::ERR for non-extension runtime, OpStatus::ERR_NO_MEMORY on OOM. */
# endif // EXTENSION_SUPPORT

#endif // ECMASCRIPT_DEBUGGER

	OP_STATUS ReportScriptError(const uni_char *message, URL *script_url, unsigned document_line_number, BOOL allow_cross_origin_errors);
		/**< Report a script-related error to this runtime's current error handler. */

private:

	static ES_Eval_Status ExecuteContextL(ES_Context* context, BOOL want_string_result = FALSE, BOOL want_exceptions = FALSE, BOOL allow_cross_origin_errors = FALSE, BOOL (*external_out_of_time)(void *) = NULL, void *external_out_of_time_data = NULL);

	void ReportCompilationErrorL(ES_Context *context, ES_Parser &parser, const CompileOptions &options, ES_ParseErrorInfo *error_info = NULL);
	/**< Used by CompileProgram() and CreateFunction() to report compilation errors to the console. */

	friend class ES_RuntimeReaper;
	friend class ESMM;
	friend class EcmaScript_Manager;
	friend class ES_ObjectReference;

    inline static OP_STATUS sanitize(OP_STATUS r)
    {
        if (OpStatus::IsMemoryError(r) || OpStatus::IsSuccess(r))
            return r;
        else
            return OpStatus::ERR;
    }

	ES_Runtime*			next_runtime_per_heap;		///< List of other runtimes with the same heap
	ES_RuntimeReaper*	runtime_reaper;				///< An object that performs garbage collection of the ES_Runtime
    ES_Global_Object*	global_object;				///< The global-object associated with this runtime
    ESRT_Data*          rt_data;					///< Runtime data associated with this runtime
    ES_Heap*	        heap;						///< The heap associated with this runtime
	FramesDocument*		frames_doc;					///< The frames-document associated with this runtime; NULL if the runtime is documentless
	int					keepalive_counter;			///< The next ID to use for an argument to KeepAliveWithRuntime
	BOOL				initializing;				///< InitializationFinished() not yet called
	BOOL				enabled;					///< TRUE if execution is enabled in this runtime
	BOOL				error_messages;				///< TRUE if error messages are to be sent to the ECMAScript manager
	BOOL				allows_suspend;				///< TRUE if it is ok to suspend this runtime at the present moment
	BOOL				debug_privileges;			///< TRUE if the runtime has debug privileges (allowing it to bypass security checks)
	BOOL				use_native_dispatcher;		///< TRUE if runtime should compile and run native code
	unsigned int		next_inline_script_no;		///< Identifiers
	unsigned int		next_linked_script_no;		///<   for
	unsigned int		next_generated_script_no;	///<     debugging
#ifdef _DEBUG
	ES_Runtime*			debug_next;					///< Next runtime in a list of live runtimes
#endif
	ES_ThreadScheduler*	scheduler;					///< Associated thread scheduler
	ES_AsyncInterface*	async_interface;			///< Associated asynchronous interface object
	ES_Context*			tostring_context;			///< Temporary context used for ToString() conversions.  Stored only for cleanup purposes; never for reusal.

	ErrorHandler*		error_handler;

#ifdef CRASHLOG_CAP_PRESENT
	friend class ES_CurrentURL;
	void				UpdateURL();
	OpString8			url;
#endif // CRASHLOG_CAP_PRESENT
};

#endif // ECMASCRIPT_RUNTIME_H
