/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2006
 *
 * ECMAScript engine: run-time system kernel
 *
 * @author Lars T Hansen
 */

/**
 * @section Kernel                The run-time system kernel
 *
 * The kernel provides the following services:
 *
 * <ul>
 * <li>   Storage for global variables (most of which are constants
 *        computed at startup)
 * <li>   A representation for ECMAScript values
 * <li>   A representation for heap-allocated objects and strings
 * <li>   Heap allocation and garbage collection
 * </ul>
 *
 * @see es_value.h for a description of values and objects
 * @see es_collector.h for a description of the garbage collector
 */

#ifndef ES_RTS_H
#define ES_RTS_H

class ES_Program_Cache;
class ES_Identifier_List;
class OpExecMemory;

/**
 * Precomputed identifiers.
 *
 * There are two groups here: identifiers and reserved words.
 * Keep each group in alphabetical order, please, and update
 * the list of string names in es_rts.cpp when adding anything
 * here.
 *
 * I highly recommend recompiling Opera after changing these
 * enums. It seems to be some dependencies that are missing, at
 * least in visual studio. -- jhoff
 */
enum
{
    // Location 0 is reserved for a NULL value
    ESID_FIRST=1,

    // Identifiers
    ESID_Arguments=ESID_FIRST,
    ESID_Array,
    ESID_ArrayBuffer,
    ESID_BYTES_PER_ELEMENT,
    ESID_Boolean,
    ESID_DOMException,
    ESID_DOMSTRING_SIZE_ERR,
    ESID_DataView,
    ESID_Date,
    ESID_E,
    ESID_Error,
    ESID_EvalError,
    ESID_EventException,
    ESID_Float32Array,
    ESID_Float64Array,
    ESID_Function,
    ESID_HIERARCHY_REQUEST_ERR,
    ESID_INDEX_SIZE_ERR,
    ESID_INUSE_ATTRIBUTE_ERR,
    ESID_INVALID_ACCESS_ERR,
    ESID_INVALID_CHARACTER_ERR,
    ESID_INVALID_MODIFICATION_ERR,
    ESID_INVALID_STATE_ERR,
    ESID_Infinity,
    ESID_Int8Array,
    ESID_Int16Array,
    ESID_Int32Array,
    ESID_LN10,
    ESID_LN2,
    ESID_LOG10E,
    ESID_LOG2E,
    ESID_MAX_VALUE,
    ESID_MIN_VALUE,
    ESID_Math,
    ESID_MemoryError,
    ESID_NAMESPACE_ERR,
    ESID_NEGATIVE_INFINITY,
    ESID_NOT_FOUND_ERR,
    ESID_NOT_SUPPORTED_ERR,
    ESID_NO_DATA_ALLOWED_ERR,
    ESID_NO_MODIFICATION_ALLOWED_ERR,
    ESID_NaN,
    ESID_Number,
    ESID_Object,
    ESID_PI,
    ESID_POSITIVE_INFINITY,
    ESID_QName,
    ESID_RangeError,
    ESID_ReferenceError,
    ESID_RegExp,
    ESID_SQRT1_2,
    ESID_SQRT2,
    ESID_SYNTAX_ERR,
    ESID_String,
    ESID_SyntaxError,
    ESID_TypeError,
    ESID_UNSPECIFIED_EVENT_TYPE_ERR,
    ESID_URIError,
    ESID_InternalError,
    ESID_UTC,
    ESID_Uint8Array,
    ESID_Uint8ClampedArray,
    ESID_Uint16Array,
    ESID_Uint32Array,
    ESID_Variable,
    ESID_WRONG_DOCUMENT_ERR,
    ESID_defineGetter,  // __defineGetter__
    ESID_defineSetter,  // __defineSetter__
    ESID_lookupGetter,  // __lookupGetter__
    ESID_lookupSetter,  // __lookupSetter__
    ESID_proto,         // __proto__
    ESID_abs,
    ESID_acos,
    ESID_alert,
    ESID_anchor,
    ESID_apply,
    ESID_arguments,
    ESID_asin,
    ESID_atan,
    ESID_atan2,
    ESID_big,
    ESID_bind,
    ESID_blink,
    ESID_bold,
    ESID_buffer,
    ESID_byteLength,
    ESID_byteOffset,
    ESID_call,
    ESID_callee,
    ESID_caller,
    ESID_ceil,
    ESID_charAt,
    ESID_charCodeAt,
    ESID_code,
    ESID_compile,
    ESID_concat,
    ESID_configurable,
    ESID_constructor,
    ESID_copysign,
    ESID_cos,
    ESID_create,
    ESID_debugger,
    ESID_decodeURI,
    ESID_decodeURIComponent,
    ESID_defineProperties,
    ESID_defineProperty,
    ESID_dollar1,
    ESID_dollar2,
    ESID_dollar3,
    ESID_dollar4,
    ESID_dollar5,
    ESID_dollar6,
    ESID_dollar7,
    ESID_dollar8,
    ESID_dollar9,
    ESID_dollar_underscore,   // $_, alias for 'input' on RegExp constructor
    ESID_dollar_ampersand,    // $&, alias for 'lastMatch' on RegExp constructor
    ESID_dollar_plus,         // $+, alias for 'lastParent' on RegExp constructor
    ESID_dollar_asterisk,     // $*, alias for 'multiline' on RegExp constructor
    ESID_dollar_grave_accent, // $`, alias for 'leftContext' on RegExp constructor
    ESID_dollar_apostophe,    // $', alias for 'rightContext' on RegExp constructor
    ESID_encodeURI,
    ESID_encodeURIComponent,
    ESID_enumerable,
    ESID_escape,
    ESID_eval,
    ESID_every,
    ESID_exec,
    ESID_exp,
    ESID_extended,
    ESID_filter,
    ESID_fixed,
    ESID_floor,
    ESID_fontcolor,
    ESID_fontsize,
    ESID_forEach,
    ESID_freeze,
    ESID_fromCharCode,
    ESID_get,
    ESID_getDate,
    ESID_getDay,
    ESID_getFloat32,
    ESID_getFloat64,
    ESID_getFullYear,
    ESID_getHours,
    ESID_getInt8,
    ESID_getInt16,
    ESID_getInt32,
    ESID_getMilliseconds,
    ESID_getMinutes,
    ESID_getMonth,
    ESID_getOwnPropertyDescriptor,
    ESID_getOwnPropertyNames,
    ESID_getPrototypeOf,
    ESID_getSeconds,
    ESID_getTime,
    ESID_getTimezoneOffset,
    ESID_getUTCDate,
    ESID_getUTCDay,
    ESID_getUTCFullYear,
    ESID_getUTCHours,
    ESID_getUTCMilliseconds,
    ESID_getUTCMinutes,
    ESID_getUTCMonth,
    ESID_getUTCSeconds,
    ESID_getUint8,
    ESID_getUint16,
    ESID_getUint32,
    ESID_getYear,
    ESID_global,
    ESID_hasOwnProperty,
    ESID_ignoreCase,
    ESID_index,
    ESID_indexOf,
    ESID_input,
    ESID_isArray,
    ESID_isExtensible,
    ESID_isFinite,
    ESID_isFrozen,
    ESID_isIntegral,
    ESID_isNaN,
    ESID_isPrototypeOf,
    ESID_isSealed,
    ESID_italics,
    ESID_join,
    ESID_JSON,
    ESID_keys,
    ESID_lastIndex,
    ESID_lastIndexOf,
    ESID_lastMatch,
    ESID_lastParen,
    ESID_leftContext,
    ESID_length,
    ESID_link,
    ESID_localeCompare,
    ESID_log,
    ESID_map,
    ESID_match,
    ESID_max,
    ESID_message,
    ESID_min,
    ESID_multiline,
    ESID_name,
    ESID_noSearch,
    ESID_now,
    ESID_object_Array,
    ESID_object_Boolean,
    ESID_object_Function,
    ESID_object_Null,
    ESID_object_Number,
    ESID_object_Object,
    ESID_object_String,
    ESID_object_Undefined,
    ESID_object_Window,
    ESID_opera_sourceloc,
    ESID_parse,
    ESID_parseFloat,
    ESID_parseInt,
    ESID_pop,
    ESID_pow,
    ESID_preventExtensions,
    ESID_prompt,
    ESID_propertyIsEnumerable,
    ESID_prototype,
    ESID_push,
    ESID_random,
    ESID_readOnly,
    ESID_reduce,
    ESID_reduceRight,
    ESID_replace,
    ESID_reverse,
    ESID_rightContext,
    ESID_round,
    ESID_seal,
    ESID_search,
    ESID_set,
    ESID_setDate,
    ESID_setFloat32,
    ESID_setFloat64,
    ESID_setFullYear,
    ESID_setHours,
    ESID_setInt8,
    ESID_setInt16,
    ESID_setInt32,
    ESID_setMilliseconds,
    ESID_setMinutes,
    ESID_setMonth,
    ESID_setSeconds,
    ESID_setTime,
    ESID_setUTCDate,
    ESID_setUTCFullYear,
    ESID_setUTCHours,
    ESID_setUTCMilliseconds,
    ESID_setUTCMinutes,
    ESID_setUTCMonth,
    ESID_setUTCSeconds,
    ESID_setUint8,
    ESID_setUint16,
    ESID_setUint32,
    ESID_setYear,
    ESID_shift,
    ESID_sin,
    ESID_slice,
    ESID_small,
    ESID_some,
    ESID_sort,
    ESID_source,
    ESID_splice,
    ESID_split,
    ESID_sqrt,
    ESID_stack,
    ESID_stacktrace,
    ESID_strike,
    ESID_stringify,
    ESID_sub,
    ESID_subarray,
    ESID_substr,
    ESID_substring,
    ESID_sup,
    ESID_tan,
    ESID_test,
    ESID_toDateString,
    ESID_toExponential,
    ESID_toFixed,
    ESID_toGMTString,
    ESID_toISOString,
    ESID_toJSON,
    ESID_toLocaleDateString,
    ESID_toLocaleLowerCase,
    ESID_toLocaleString,
    ESID_toLocaleTimeString,
    ESID_toLocaleUpperCase,
    ESID_toLowerCase,
    ESID_toPrecision,
    ESID_toString,
    ESID_toTimeString,
    ESID_toUTCString,
    ESID_toUpperCase,
    ESID_trim,
    ESID_typeOf,
    ESID_undefined,
    ESID_unescape,
    ESID_unshift,
    ESID_value,
    ESID_valueOf,
    ESID_writable,
    ESID_Window,

    // Reserved words
    ESID_break,
    ESID_case,
    ESID_catch,
    ESID_continue,
    ESID_default,
    ESID_delete,
    ESID_do,
    ESID_else,
    ESID_finally,
    ESID_for,
    ESID_function,
    ESID_goto,
    ESID_if,
    ESID_in,
    ESID_instanceof,
    ESID_new,
    ESID_return,
    ESID_switch,
    ESID_this,
    ESID_throw,
    ESID_try,
    ESID_typeof,
    ESID_var,
    ESID_void,
    ESID_while,
    ESID_with,
    ESID_false,
    ESID_true,
    ESID_null,

    ESID_NUMIDS
};

/** Identifiers for pre-created strings, a table of which is in RT_DATA.strings.
    Keep these in strict alphabetical order, with numeric strings last */
enum
{
    STRING_attributename,
    STRING_boolean,
    STRING_empty,
    STRING_false,
    STRING_function,
    STRING_length,
    STRING_null,
    STRING_number,
    STRING_object,
    STRING_proto, // __proto__
    STRING_string,
    STRING_toJSON,
    STRING_true,
    STRING_undefined,
    STRING_xml,
    STRING_empty_regexp,
    STRING_0                // Must be last string
};

#define STRING_NUMNUMBERS   100
#define STRING_nul          (STRING_0 + STRING_NUMNUMBERS)
#define STRING_NUMCHARS     127
#define STRING_NUMSTRINGS   (STRING_0 + STRING_NUMNUMBERS + STRING_NUMCHARS)

#ifndef CONSTANT_DATA_IS_EXECUTABLE
enum // OpPseudoThread:: meta method ids
{
    ES_OPT_Start,
    ES_OPT_Resume,
    ES_OPT_Yield,
    ES_OPT_Suspend,
    ES_OPT_Reserve,
    ES_OPT_StackSpaceRemaining,
    ES_OPT_COUNT
};
#endif // CONSTANT_DATA_IS_EXECUTABLE

/**
 * ESRT_Data holds all global data for the ECMAScript engine.
 *
 * One instance of this structure is allocated by engine initialization
 * code and initialized by ESRT::Initialize(), and is made available
 * through the global variable g_esrt.  Use the macro RT_DATA to read
 * that variable.
 */
struct ESRT_Data
{
    ESRT_Data() : initialized(FALSE) {;}

    BOOL                    initialized;                // flag: the engine has been initialized
    BOOL                    in_collector;               // flag: the garbage collector is currently running
    BOOL                    clean_latent_arguments;     // flag: the garbage collector should clean ES_Function::latent_arguments fields
    unsigned int            active_contexts;            // counter: number of contexts currently used by interpreter invocations on the execution stack
    unsigned int            suspended_contexts;         // counter: number of contexts that have started, but not completed, execution
    unsigned int            js_ident_hashcode;          // counter: next hash code for ES_Identifier
    unsigned int            temp_counter;               // counter: global temporary variable names (used by compiler)
    unsigned int            script_counter;             // counter: global script counter (used by compiler for debug info)
    unsigned int            unique;                     // unique number store used by ES_Class to give identities to pseudo classes
    ES_Context*             tmp_context;                // A null context
    ES_Context*             saved_callback_context;        // Is given a value on call-out to DOM, so that call-backs into
                                                        // the engine can find the original context.
    double                  load_factor;                // collection policy: ratio of allocated storage to live storage
    double                  offline_load_factor;        // collection policy when not running contexts: ratio of allocated storage to live storage
    ES_Value_Internal       undefined;                  // value always undefined
    ES_Value_Internal       arguments_cookie;           // unique value that program should *never* see
    JString*                strings[STRING_NUMSTRINGS]; // cached strings
    JString*                idents[ESID_NUMIDS];        // cached ids
    ES_MarkStack*           markstack;                  // shared mark stack
    ES_Heap*                heap;                       // heap used for shared strings and identifiers
    ES_Identifier_List*     shared_strings;             // table of dynamically shared strings
    TempBuffer              shared_string_buffer;

    JString*                GetSharedString(const char *value, unsigned length = UINT_MAX);
    JString*                GetSharedString(const uni_char *value, unsigned length = UINT_MAX);
    JString*                GetSharedString(JString *value);

#ifndef CONSTANT_DATA_IS_EXECUTABLE
    const OpExecMemory*                                 // executable block that holds platform-dependant, assembler-coded implementations of
                            opt_meta_method_block[ES_OPT_COUNT]; // OpPseudoThread's meta-methods: Start, Resume, Yield, Suspend, Reserve, StackSpaceRemaining
#endif // CONSTANT_DATA_IS_EXECUTABLE

    double                  NaN;                        // Not-a-number
    double                  NegativeZero;               // -0.0
    double                  DoubleMin;                  // Smallest representable double value
    double                  PositiveInfinity;           // +Inf.0
    double                  NegativeInfinity;           // -Inf.0
    ES_Context*             cur_exec_context;           // currently executing context (or NULL if none)
#ifdef ES_EXECUTE_SUPPORT
# ifdef ECMASCRIPT_DEBUGGER
    ES_DebugListener*       debugger_listener;          // or NULL
    ES_Object**             debugger_scope_chain;       // Just a pointer array, NOT TRACED BY GC!
    unsigned int            debugger_scope_chain_length;//   and its length
    ES_DebugStackElement*   debugger_call_stack;        // An array of POD classes, NOT TRACED BY GC!
    unsigned int            debugger_call_stack_length; //   and its length
# endif
#endif

#ifdef ECMASCRIPT_OPTIMIZER
    int*                    temporary_int_array;
    unsigned int            temporary_int_array_length;
#endif

    ES_Program_Cache*       program_cache;

    ESMM_Debug              esmmd;                      // GC debug and monitoring

    unsigned                script_guid_counter;        // global unique counter for script id

#ifdef ES_PROPERTY_CACHE_PROFILING
    unsigned                pcache_misses;              // Cache records looked at but not used.
    unsigned                pcache_fails;               // Slow cases when there was at least one cache record.
    unsigned                pcache_fills;               // Inserted cache records (total).
    unsigned                pcache_fills_poly;          // Inserted cache records into non-empty cache.

    unsigned                pcache_last_reported;       // Sum of all counters at last report.

    unsigned                pcache_allocated_property;   // Bytes of memory allocated for property caches.
    unsigned                pcache_allocated_global;    // Bytes of memory allocated for global caches.
#endif // ES_PROPERTY_CACHE_PROFILING

#ifdef ES_SLOW_CASE_PROFILING
    unsigned                 slow_case_calls[ESI_LAST_INSTRUCTION];
#endif // ES_SLOW_CASE_PROFILING
};

/**
 * ESRT is a namespace for central run-time system functions.
 *
 * No instances are ever created: there is a separate class
 * ESRT_Data that holds run-time system data.
 */
class ESRT
{
private:
    ESRT();
        /**< No instances of ESRT are ever created */

    static void    CreateStandardIdentifiers(ES_Context *context, ESRT_Data &rt_data);
        /**< Allocate and initialize known identifiers */

    static void CreateStandardStrings(ES_Context *context, ESRT_Data &rt_data);
        /**< Allocate and initialize known JStrings */

public:
    static ESRT_Data *Initialize();
        /**< Initialize runtime.  The garbage collector must already have
             been initialized. */

    static void Shutdown(ESRT_Data *rt_data);
        /**< Clean up root tables, symbol tables, and delete cached data.
             Run the garbage collector.  Unless there are undeleted
             dynamic roots, all data owned by the engine except the GC
             will be deleted at this time. */

    static BOOL MaybeCollect(BOOL pass_debug_info=FALSE);
        /**< Make a policy decision about whether to run the collector, and
             run if it that is the decision.

             If pass_debug_info is TRUE then the collector is passed information
             about the items on the top of the execution stack in the current
             context, if any, and will pass them on to the debugger if the debugger
             is installed.

             @return TRUE if the collector was run and the debugger told the
                     engine to suspend, otherwise FALSE.
             */

    static BOOL ForceCollect(GC_Reason reason, unsigned int script_guid=0, unsigned int line_no=0);
        /**< Run a garbage collection.  If a debugger is installed, signal it with
             the provided debug information.

             @return TRUE if the installed debugger told the engine to suspend,
                     FALSE otherwise */

    static unsigned GetGlobalUniqueScriptId(ESRT_Data *rt_data);
        /**< @return a global unique script id */
};

#ifdef _DEBUG
struct ExecutedInstr
{
    int                  instr;
    ES_Compiled_Code* code;
};
#endif

#define RT_DATA                (*g_esrt)

#endif  // ES_RTS_H

