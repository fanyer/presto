/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_CODE_H
#define ES_CODE_H

#include "modules/ecmascript/carakan/src/vm/es_instruction.h"
#include "modules/ecmascript/carakan/src/compiler/es_source_location.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/memory/src/memory_executable.h"
#include "modules/util/simset.h"

class ES_Code;
class ES_FunctionCode;
class ES_RegExp_Information;
class ES_RegExp_Object;
class ES_NativeCodeBlock;
class ES_CodeOptimizationData;
class ES_Compact_Indexed_Properties;
class ES_FunctionCodeStatic;
class ES_ProgramCodeStaticReaper;

class ES_StaticStringData
{
private:
    unsigned ref_count;

public:
    ES_StaticStringData()
        : ref_count(1),
          storage(NULL)
    {
    }

    ~ES_StaticStringData()
    {
        if (storage)
            JStringStorage::FreeStatic(storage);
    }

    static ES_StaticStringData *IncRef(ES_StaticStringData *data) { if (data) ++data->ref_count; return data; }
    static void DecRef(ES_StaticStringData *data) { if (data) if (--data->ref_count == 0) OP_DELETE(data); }

    JStringStorage *storage;
};

class ES_StaticSourceData
    : public ES_StaticStringData
{
public:

    ES_StaticSourceData()
        : document_line(1)
        , document_column(0)
    {
    }

    static ES_StaticSourceData *IncRef(ES_StaticSourceData *data)
    {
        return static_cast<ES_StaticSourceData *>(ES_StaticStringData::IncRef(data));
    }

    static void DecRef(ES_StaticSourceData *data)
    {
        ES_StaticStringData::DecRef(data);
    }

    unsigned document_line;
    /**< For inline scripts, this is the line where the scripts source
         starts in the enclosing document. */

    unsigned document_column;
    /**< For inline scripts, this is the column on the line @c document_line
         where the script source starts. */
};

class ES_CodeStatic
{
public:
    enum Type
    {
        TYPE_PROGRAM,
        TYPE_FUNCTION
    };

    enum GlobalCacheAccess
    {
        PutGlobal = 1,
        GetGlobal = 2,
        GetScope  = 4
    };

    enum BuiltInException
    {
        TYPEERROR_EXPECTED_LVALUE = 1,
        TYPEERROR_INVALID_ASSIGNMENT
    };

    class String
    {
    public:
        unsigned offset;
        unsigned length;
    };

    class FunctionDeclaration
    {
    public:
        unsigned name;
        /**< Index into 'strings' of function's name. */
        unsigned function;
        /**< Index into 'functions' of function. */
    };

    class ObjectLiteralClass
    {
    public:
        ObjectLiteralClass() : properties(NULL), properties_count(0) {}
        ~ObjectLiteralClass() { OP_DELETEA(properties); }

        unsigned *properties;
        /**< Indeces into 'strings' of property names. */
        unsigned properties_count;
    };

    class ConstantArrayLiteral
    {
    public:
        ~ConstantArrayLiteral();

        unsigned elements_count;
        /**< Number of elements in the array; that is, number of elements in the
             'indeces' and 'values' arrays.  Note that this isn't the length of
             the array. */
        unsigned array_length;
        /**< Length of the array. This can be different from above since not all
             elements in the array might be literal expressions. */
        unsigned *indeces;
        /**< Indeces at which corresponding values in 'values' should be
             stored. */

        class Value
        {
        public:
            ES_ValueType type;
            /**< Type of value.  ESTYPE_OBJECT not allowed. */

            int value;
            /**< For ESTYPE_BOOLEAN: 0 for false, 1 for true.
                 For ESTYPE_INT32: actual value.
                 For ESTYPE_DOUBLE: index into 'doubles'.
                 For ESTYPE_STRING: index into 'strings'. */
        };

        Value *values;
        /**< Element values. */
    };

    class SwitchTable
    {
    public:
        SwitchTable() : minimum(0), codeword_indeces(NULL) {}
        ~SwitchTable() { codeword_indeces += minimum; OP_DELETEA(codeword_indeces); }
        SwitchTable(int minimum, int maximum) : minimum(minimum), maximum(maximum), codeword_indeces(NULL) {}

        int minimum;
        int maximum;

        unsigned *codeword_indeces, default_codeword_index;
    };

#ifdef ES_NATIVE_SUPPORT
    class LoopData
    {
    public:
        unsigned start;
        /**< Code word index of first instruction in loop body. */

        unsigned jump;
        /**< Code word index of loop jump instruction. */
    };
#endif // ES_NATIVE_SUPPORT

    class ExceptionHandler
    {
    public:
        ExceptionHandler()
            : nested_handlers(NULL)
        {
        }

        ~ExceptionHandler()
        {
            OP_DELETEA(nested_handlers);
        }

        enum Type { TYPE_CATCH, TYPE_FINALLY } type;
        /**< Type of exception handler. */

        unsigned start;
        /**< Index into 'codewords' of the first code word of the first
             instruction covered by the exception handler. */
        unsigned end;
        /**< Index into 'codewords' of the first code word of the first
             instruction not covered by the exception handler. */
        unsigned handler_ip;
        /**< Index into 'codewords' of the first code word of the first
             instruction of the exception handler. */

        ExceptionHandler *nested_handlers;
        unsigned nested_handlers_count;
    };

    /** Class defining an inner/dynamic scope chain (inner = inside the local
        scope of a function.)  Typically 'with' statements and 'catch' clauses
        produce inner scopes (though in the case of 'catch' clauses the compiler
        can usually optimize it away.)  In the case of two nested inner scopes
        (e.g. nested 'with' statements) there will be three InnerScope objects,
        one for the first range where only one inner scope object is active, one
        where both inner scope objects are active, and then one where only the
        first inner scope object is active again.  IOW, there is really no
        special handling of nested inner scope ranges. */
    class InnerScope
    {
    public:
        InnerScope()
            : registers(NULL)
        {
        }

        ~InnerScope()
        {
            OP_DELETEA(registers);
        }

        ES_CodeWord::Index *registers;
        /**< Registers containing inner scope objects.  Highest index is
             innermost scope. */
        unsigned registers_count;
        /**< Number of inner scopes. */
    };

    class DebugRecord
    {
    public:
        enum Type
        {
            TYPE_EXTENT_INFORMATION,
            /**< Just specifying the source code extent that the current
                 instruction should be mapped to in an error message. */

            TYPE_BASE_EXPRESSION
            /**< Specifying the source code extent of the base expression of
                 a property accessor or function call. */
        };

        unsigned codeword_index:31;
        /**< Code word index of the first instruction this record is valid for.
             It's valid for all instructions between that instruction and the
             first instruction of the next record. */
        unsigned type:1;
        /**< Type of debug record. */

        ES_SourceLocation location;
        /**< Source code location.  Always relative this code object's source
             code. */

        static const unsigned char *Compress(DebugRecord *records, unsigned count);
        /**< Compress non-empty array of debug records. */
        static DebugRecord *Decompress(const unsigned char *data, unsigned count, ES_Execution_Context *context = NULL);
        /**< Decompress non-empty array of debug records. */
    };

    class VariableRangeLimitSpan
    {
    public:
        ES_CodeWord::Index index;
        /**< Register index. */

        int lower_bound;
        /**< Lowest value the variable ever has, inclusive. */

        int upper_bound;
        /**< Highest value the variable ever has, inclusive. */

        unsigned start;
        /**< Index into 'codewords' of the first code word of the first
             instruction for which this range limit is valid. */

        unsigned end;
        /**< Index into 'codewords' of the first code word of the first
             instruction for which this range limit is no longer valid. */

        VariableRangeLimitSpan *next;
    };

protected:
    ES_CodeStatic(Type type);

public:
    virtual ~ES_CodeStatic();

    static ES_CodeStatic *IncRef(ES_CodeStatic *data) { ++data->ref_count; return data; }
    static void IncCacheRef(ES_CodeStatic *data) { ++data->cache_ref_count; OP_ASSERT(data->cache_ref_count == 1); }
    static BOOL DecRef(ES_CodeStatic *data);
    static BOOL DecCacheRef(ES_CodeStatic *data);

    BOOL IsShared() { return ref_count > 0; }

    ES_SourceCode source;
    /**< Source code fragment that this code was compiled from. */

    ES_SourceLocation start_location, end_location;

    ES_CodeWord *codewords;
    unsigned *instruction_offsets;
    /**< The index of each instruction code word in the ES_Code::codewords
         array.  Useful when traversing the code word array backwards. */
    ES_CodeStatic *parent_code;

    ES_StaticSourceData *source_storage_owner;
    JStringStorage *source_storage;

    ES_StaticStringData *string_storage_owner;
    JStringStorage *string_storage;
    String *strings;
    double *doubles;
    ES_FunctionCodeStatic **functions;
    FunctionDeclaration *function_declarations;
    ObjectLiteralClass *object_literal_classes;
    ConstantArrayLiteral *constant_array_literals;
    ES_RegExp_Information *regexps;
    unsigned *global_accesses;
    /**< Array of global caches accesses, recording string index of property
         along with access kind. Access kind is recorded in the lower three
         bits, the string index is kept above (>= bit 3). */
    SwitchTable *switch_tables;
#ifdef ES_NATIVE_SUPPORT
    LoopData *loop_data;
#endif // ES_NATIVE_SUPPORT
    ExceptionHandler *exception_handlers;
    InnerScope *inner_scopes;
    const unsigned char *compressed_debug_records;
    DebugRecord *debug_records;
#ifdef ES_NATIVE_SUPPORT
    VariableRangeLimitSpan *first_variable_range_limit_span;
    ES_CodeOptimizationData *optimization_data;
    /**< Optimization data. */

    inline BOOL CanHaveNativeDispatcher();

    unsigned char *profile_data;
    /**< One byte per codeword.  Usage per codeword type:

           instruction: slow case call counter
           register: bitfield of value types seen
           others: not used

         The array is allocated when first needed. */

    /** Flags used with profile_data. */
    enum ProfileFlags
    {
        GET_FAILED_INLINE = 0x1,
        /**< Set on the 'name' codeword of an ESI_GETN_IMM,
             indicating that a function inline attempt has
             failed for this instruction. */

        GET_CACHE_HIT = 0x2
        /**< Set on the 'name' codeword of an ESI_GETN_IMM,
             indicating that the property lookup has been
             satisfied by its property cache. */
    };
#endif // ES_NATIVE_SUPPORT

private:
    unsigned ref_count:30;
    unsigned cache_ref_count:2;

public:
    Type type:8;
    ES_Runtime::PrivilegeLevel privilege_level:8;

    unsigned prepared_for_sharing:1;
    unsigned is_external:1;
    unsigned is_strict_mode:1;

#ifdef ECMASCRIPT_DEBUGGER
    unsigned has_debug_code:1;
    /**< TRUE if program is compiled with debugger stop instructions. */
#endif // ECMASCRIPT_DEBUGGER

    unsigned codewords_count;
    /**< Number of elements in 'codewords'. */
    unsigned instruction_count;
    /**< Number of elements in 'instruction_offsets'. */

    void SetIsExternal(BOOL value);
    void SetPrivilegeLevel(ES_Runtime::PrivilegeLevel value);

    void FindInstructionOffsets(ES_Execution_Context *context=NULL);

    unsigned register_frame_size;
    unsigned first_temporary_register;
    unsigned strings_count;
    unsigned doubles_count;
    unsigned functions_count, static_functions_count;
    unsigned function_declarations_count;
    unsigned object_literal_classes_count;
    unsigned constant_array_literals_count;
    unsigned regexps_count;
    unsigned global_accesses_count;
    unsigned property_get_caches_count;
    unsigned property_put_caches_count;
#ifdef ES_NATIVE_SUPPORT
    unsigned loop_data_count;
#endif // ES_NATIVE_SUPPORT
    unsigned format_string_caches_count;
    unsigned eval_caches_count;
    unsigned switch_tables_count;
    unsigned exception_handlers_count;

    BOOL FindExceptionHandler(unsigned current_ip, unsigned &handler_ip, ES_CodeStatic::ExceptionHandler::Type &handler_type);
    /**< Find the exception handler (if any) that handles exception occuring at
         'current_ip' and return the handler's instruction pointer via
         'handler_ip'.  If an exception handler in this code (that is, in "this"
         stack frame) TRUE is returned.  Note: 'current_ip' is assumed to be one
         word past the end of the current instruction, or in other words,
         actually point to the next instruction. */

    BOOL FindFinallyHandler(unsigned current_ip, unsigned target, unsigned &handler_ip);
    /**< FindFinallyHandler operates almost as FindExceptionHandler but only
         takes handlers of typ TYPE_FINALLY into account and only handlers that
         would be crossed by jumping to target. */

    unsigned inner_scopes_count;

    unsigned debug_records_count;

    DebugRecord *FindDebugRecord(ES_Execution_Context *context, DebugRecord::Type type, ES_CodeWord *codeword);

    unsigned source_string;
    /**< Index into 'strings' of string containing source code. */

    void SetParentCode(ES_CodeStatic *new_parent_code);
};

/** (C++) class used to record (compact) classes of object literals.

    These classes are kept alive by a code object (only), hence
    the classes are kept in a table whereby the GC can remove
    them from the class tree once they become unreferenced. */
class ES_Code_ClassTable
{
public:
    ES_Code_ClassTable();

    enum { MAX_OBJECT_LITERAL_COMPACT_CLASSES = 4 };
    ES_Class *table[MAX_OBJECT_LITERAL_COMPACT_CLASSES];
    ES_Code_ClassTable *next;
};

class ES_Code
    : public ES_Boxed
{
public:
    enum Type
    {
        TYPE_PROGRAM,
        TYPE_FUNCTION,
        TYPE_EVAL_PLAIN,
        TYPE_EVAL_ODD
    };

    class PropertyCache
    {
    public:
        enum
        {
            CACHED_OFFSET_MASK = 0x0003ffffu,
            CACHE_LIMIT_SHIFT  = 18
        };

        PropertyCache() : class_id(ES_Class::NOT_CACHED_CLASS_ID), cache_data(CACHED_OFFSET_MASK), cached_type(0), object_class(NULL), next(NULL) { data.boxed = NULL; }

        unsigned class_id;
        /**< For get and put: the object's class's ID. */
        unsigned cache_data;
        /**< The property offset and limit of the property, packed into one unsigned.

             The offset part is stored in bits 0-17:
                 For get and put: the property offset. In case of GET: 0x3ffff
                 means the property does not exist. This should be queried via
                 IsNegativeCache()

             The limit part is stored in bits 18-31:
                 For get: In case of offset == 0x3ffff (must also be queried by
                 IsNegativeCache()) or data.prototype_object != NULL the largest size
                 the object considered in a cached lookup can have while still
                 considering the cache to be valid, otherwise the index where the
                 property was found. */

        unsigned cached_type;
        /**< Information of the type of the cached property.

             Bits:
             0  - 7:  cached ES_ValueType.
             8  - 15: cached ES_StorageType
             16 - 23: size of stored value
             24 - 32: if value can be NULL
        */

        union
        {
            ES_Boxed *boxed;
            /**< Used when tracing. */

            ES_Object *prototype_object;
            /**< In case of GET: NULL or the prototype from which to read the
                 property.  This means 'cached_index' is not an index into the
                 primary object's properties array but instead into the property
                 array of this prototype object. */
            ES_Class *new_class;
            /**< In case of PUT: NULL or the new class that the object
                 transitions to when the put property is added to it. */
        } data;

        ES_Class *object_class;
        /**< Class whose ID was 'class_id' when this cache was established. */

        PropertyCache *next;
        /**< Next cache record, or NULL. */

        inline unsigned GetOffset() const { OP_ASSERT((cache_data & CACHED_OFFSET_MASK) != CACHED_OFFSET_MASK); return cache_data & CACHED_OFFSET_MASK; }
        /**< Get cached offset. Should never be called unless IsNegativeCache() returns FALSE. */
        inline ES_LayoutIndex GetLimit() const { return ES_LayoutIndex(cache_data >> CACHE_LIMIT_SHIFT); }
        /**< Get cache limit. */
        inline void SetOffsetAndLimit(unsigned offset, unsigned limit) { cache_data = (offset & CACHED_OFFSET_MASK) | (limit << CACHE_LIMIT_SHIFT); }
        /**< Set offset and limit. */
        inline BOOL IsNegativeCache() const { return (cache_data & CACHED_OFFSET_MASK) == CACHED_OFFSET_MASK; }
        /**< Checks if the cache represents a non-existing property. */
        inline ES_StorageType GetStorageType() const { return ES_Value_Internal::StorageTypeFromCachedTypeBits(cached_type); }
        /**< Get the storage type of the cache entry. */
        inline unsigned GetStorageSize() const { return ES_Value_Internal::SizeFromCachedTypeBits(cached_type); }
        /**< Get the storage size of the cache entry. */
    };

    class GlobalCache
    {
    public:
        GlobalCache() : class_id(ES_Class::NOT_CACHED_CLASS_ID), cached_index(UINT_MAX), cached_offset(UINT_MAX), cached_type(0) {}

        unsigned class_id;
        /**< The global object's class's ID. */
        unsigned cached_index;
        /**< The property index or global variable index. */
        unsigned cached_offset;
        /**< The property offset, never a variable index. */
        unsigned cached_type;
        /**< Information of the type of the cached property.

             Bits:
             0  - 7:  cached ES_ValueType.
             8  - 15: cached ES_StorageType
             16 - 23: size of stored value
             24 - 32: if value can be NULL
        */
    };

    class FormatStringCache
    {
    public:
        FormatStringCache() : from(NULL), to(NULL) {}

        JString *from;
        JString *to;
    };

    class EvalCache
    {
    public:
        EvalCache() : source(NULL), code(NULL) {}

        JString *source;
        ES_Code *code;
    };

#ifdef ES_NATIVE_SUPPORT
    class SwitchTable
    {
    public:
        SwitchTable() : native_jump_targets(NULL), default_native_jump_target(NULL) {}

        void **native_jump_targets, *default_native_jump_target;
    };

    class CodeWordFromReturnAddress
    {
    public:
        void *return_address;
        ES_CodeWord *codeword;
    };

    class LoopIO
    {
    public:
        enum Type { TYPE_GLOBAL_CACHED, TYPE_GLOBAL_IMM } type;

        JString *name;
        unsigned index;

        BOOL input, output;
        unsigned alias_register;

        LoopIO *next;
    };
#endif // ES_NATIVE_SUPPORT

    ES_CodeStatic *data;
    ES_Global_Object *global_object;
    ES_Code *parent_code;
private:
    friend class ES_Compiler;
    friend class ESMM;
    JString **strings;
public:
    ES_RegExp_Object **regexps;

    ES_Class **object_literal_classes;

    ES_Code_ClassTable *object_literal_prototype_classes;

    ES_Compact_Indexed_Properties **constant_array_literals;

    ES_FunctionCode **functions;
    JString *url;

    PropertyCache *property_get_caches;
    PropertyCache *property_put_caches;
    GlobalCache *global_caches;

#ifdef ES_NATIVE_SUPPORT
    unsigned *loop_counters;
    ES_Code **loop_dispatcher_codes;
    /**< Child code objects for native loop dispatchers. */
    SwitchTable *switch_tables;
    void *native_dispatcher;
    ES_NativeCodeBlock *native_code_block;
    CodeWordFromReturnAddress *codeword_from_return_address;
    LoopIO *first_loop_io;
#endif // ES_NATIVE_SUPPORT
    FormatStringCache *format_string_caches;
    EvalCache *eval_caches;
    ES_Object **scope_chain;
    /**< Scope chain. */

    Type type:8;
    unsigned is_strict_eval:1;
    /**< Set to 1 if 'type' is TYPE_FUNCTION, but the code object
         represents a strict mode eval(). */

    unsigned scope_chain_length;
    /**< Length of scope chain. */

#ifdef ES_NATIVE_SUPPORT
    unsigned call_count;
    /**< Number of times this function has been called (unless it has a native
         dispatcher.)  Used to decide when to generate a native dispatcher for
         the function. */

    unsigned slow_case_calls;
    /**< Number of times any slow case has been called. */

    unsigned has_integer_overflows;
    /**< An integer increment or decrement has ever overflowed into a double.
         Until this happens, integer operations will be generated such that the
         whole native dispatcher is regenerated if an overflow occurs:

           0 => no integer overflows have occured
           1 => an integer overflow has just occured, dispatcher needs to be
                regenerated
           2 => an integer overflow has occured and the dispatcher has since
                been regenerated. */

    unsigned has_failed_preconditions;
    /**< Statically calculated loop pre-condition has ever failed: always
         generate native dispatcher with such guessing disabled. */

    unsigned has_failed_inlined_function, total_inline_failures;

    unsigned loop_io_start;
    unsigned loop_io_end;
#endif // ES_NATIVE_SUPPORT

    JString *GetString(unsigned index) { if (index & 0x40000000u) return global_object->rt_data->strings[index & 0x3fffffffu]; else return strings[index]; }

    BOOL CanHaveVariableObject();
    /**< TRUE if invocations of this code can have their variable objects
         created.  This is typically the case if the code uses ESI_EVAL or has
         nested functions that use ESI_{GET,PUT}_{SCOPE,LEXICAL}.

         If true, we need to be more conservative when optimizing the code. */

    void PrepareForExecution(ES_Context *context);

    void SetParentCode(ES_Code *parent_code);

    unsigned GetScriptGuid() { return data->source.GetScriptGuid(); }

    void SetIsExternal(BOOL value);
    void SetPrivilegeLevel(ES_Runtime::PrivilegeLevel value);

    ES_Class *GetObjectLiteralClass(ES_Context *context, unsigned index, ES_CodeWord *values = NULL, ES_Value_Internal *reg = NULL);
    void SetObjectLiteralClass(ES_Context *context, unsigned index, ES_Class *klass);

    unsigned GetObjectLiteralClassCount(unsigned index);

    inline static BOOL CanCache(unsigned offset, unsigned limit, BOOL is_negative) { return (is_negative || offset < PropertyCache::CACHED_OFFSET_MASK) && limit <= 0x3fffu; }
    /**< Check if it is allowed to set up a property cache on a given limit and offset. */

    void DiscardCacheRecord(ES_Execution_Context *context, BOOL get_cache, unsigned index, PropertyCache *record);
    void RemoveCacheRecord(ES_Execution_Context *context, BOOL get_cache, unsigned index, PropertyCache *record);
    static BOOL IsSimplePropertyCache(PropertyCache *cache, BOOL is_get_cache);

    void OptimizePropertyCaches(ES_Context *context);

    unsigned PropertyCacheSize(unsigned index, BOOL is_get_cache);
    unsigned PropertyCacheType(unsigned index, BOOL is_get_cache);

    OP_STATUS SetScopeChain(ES_Object **objects, unsigned length);

#ifdef ES_NATIVE_SUPPORT
    BOOL ShouldGenerateNativeDispatcher() { return slow_case_calls >= (data->codewords_count * ES_JIT_GENERATE_FACTOR); }
    BOOL ShouldRegenerateNativeDispatcher() { return slow_case_calls >= (data->codewords_count * ES_JIT_REGENERATE_FACTOR) || has_integer_overflows == 1; }
#endif // ES_NATIVE_SUPPORT

protected:
    static void Initialize(ES_Code *code, ES_CodeStatic *data, ES_Global_Object *global_object, Type type);
    static void Destroy(ES_Code *code);

    static void InitializeFromStatic(ES_Context *context, ES_Global_Object *global_object, ES_Code *code, ES_Identifier_List *strings_table, ES_ProgramCodeStaticReaper *program_reaper, JString *url_str);
    static void CalculateStringOffsets(ES_Context *context, ES_Code *code, ES_Identifier_Hash_Table *long_string_literal_table, ES_Identifier_List *strings_table, unsigned *offsets, unsigned &current_offset);
    static unsigned CopyStrings(ES_Context *context, ES_Code *code, ES_StaticStringData *data, unsigned write_offset);

    void OptimizeGlobalAccessors(ES_Context *context);
    /**< Rewrite ESI_{GET,PUT}_GLOBAL to corresponding _IMM variants where
         possible. */
};


class ES_Program_Cache;


class ES_ProgramCodeStatic
    : public ES_CodeStatic,
      public ListElement<ES_ProgramCodeStatic>
{
public:
    static ES_ProgramCodeStatic *IncRef(ES_ProgramCodeStatic *data) { return static_cast<ES_ProgramCodeStatic *>(ES_CodeStatic::IncRef(data)); }

    ES_ProgramCodeStatic()
        : ES_CodeStatic(TYPE_PROGRAM),
          variable_declarations(NULL),
          variable_declarations_count(0),
          generate_result(FALSE),
          program_cache(NULL)
    {
    }

    virtual ~ES_ProgramCodeStatic();

    unsigned *variable_declarations;
    /**< Indeces into 'ES_CodeStatic::strings'/'ES_Code::strings' of the names
         of declared variables.

         The variable declarations are tagged in the highest bit to indicate that
         they are function declarations when we have native support. This to
         enable support for inlining of globally declared functions.*/
    unsigned variable_declarations_count;

    BOOL generate_result;
    /**< If TRUE, the program "returns" the value of the last statement. */

    ES_Program_Cache *program_cache;

    double program_cache_time;
    /**< Used by ES_Program_Cache. */
};

class ES_ProgramCode
    : public ES_Code
{
public:
    ES_ProgramCodeStatic *GetData() { return static_cast<ES_ProgramCodeStatic *>(data); }

    static ES_ProgramCode *Make(ES_Context *context, ES_Global_Object *global_object, ES_ProgramCodeStatic *data, BOOL initialize_from_static = FALSE, JString *url_str = NULL);

    static void Initialize(ES_ProgramCode *code, ES_ProgramCodeStatic *data, ES_Global_Object *global_object)
    {
        ES_Code::Initialize(code, data, global_object, TYPE_PROGRAM);
        code->InitGCTag(GCTAG_ES_ProgramCode, TRUE);
        code->long_string_literal_table = NULL;
    }

    static void Destroy(ES_ProgramCode *code)
    {
        ES_Code::Destroy(code);
    }

    ES_Identifier_Hash_Table *long_string_literal_table;

    void PrepareStaticForSharing(ES_Context *context);

    void StartExecution(ES_Runtime *runtime, ES_Context *context);
    /**< Register global variables; create function objects for every function
         declaration and populate the appropriate global variable slots or
         global object properties. */
};

class ES_ProgramCodeStaticReaper
{
private:
    void FunctionCreatedInternal();
    void FunctionDeletedInternal();

    unsigned ref_count;
    ES_ProgramCodeStatic *data;
    unsigned functions_count, deleted_functions;

public:
    ES_ProgramCodeStaticReaper();
    ~ES_ProgramCodeStaticReaper();
    void Initialize(ES_ProgramCodeStatic *data);

    ES_ProgramCodeStatic *GetData() { return data; }

    static void FunctionCreated(ES_ProgramCodeStaticReaper *reaper);
    static void FunctionDeleted(ES_ProgramCodeStaticReaper *reaper);
    static ES_ProgramCodeStaticReaper *IncRef(ES_ProgramCodeStaticReaper *reaper);
    static void DecRef(ES_ProgramCodeStaticReaper *reaper);
};


class ES_FunctionCodeStatic
    : public ES_CodeStatic
{
public:
    static ES_FunctionCodeStatic *IncRef(ES_FunctionCodeStatic *data) { return static_cast<ES_FunctionCodeStatic *>(ES_CodeStatic::IncRef(data)); }

    ES_FunctionCodeStatic()
        : ES_CodeStatic(TYPE_FUNCTION),
          formals_and_locals(NULL),
          formals_count(0)
    {
    }

    virtual ~ES_FunctionCodeStatic();

    unsigned *formals_and_locals;
    /**< Indeces into 'ES_CodeStatic::strings'/'ES_Code::strings' of the names
         of the function's formal parameters and local variables. */

    unsigned name;
    /**< Index into 'ES_CodeStatic::strings'/'ES_Code::strings' of the
         function's name, or "debug name" if highest bit is set, or UINT_MAX. */

    unsigned formals_count;
    unsigned LocalsCount() { return first_temporary_register - (2 + formals_count); }

    enum { ARGUMENTS_NOT_USED = (1 << 24) - 1 };

    unsigned arguments_index:24;
    /**< Register index where 'arguments' is stored or ARGUMENTS_NOT_USED if
         'arguments' isn't used and thus no register is allocated for it.

         24 bits plus the following one-bit flags makes a full 32-bit word, and
         is enough unless the function has more than 16777215 formals, which
         seems unlikely. */

    unsigned uses_eval:1;
    unsigned uses_arguments:1;
    unsigned uses_get_scope_ref:1;
    unsigned has_redirected_call:1;
    unsigned is_function_expr:1;
    unsigned is_void_function:1;
    unsigned is_static_function:1;

    unsigned has_created_arguments_array:1;
    /**< TRUE if the function has ever had its arguments array created.  This is
         automatically set if the function itself uses the 'arguments'
         identifier (and arguments_index != ARGUMENTS_NOT_USED), but can also be set
         dynamically if the function is called with too many arguments or if its
         arguments array is accessed via '<function>.arguments'. */

    BOOL CanHaveVariableObject();
    /**< TRUE if invocations of this function code can have their variable
         objects created.  This is typically the case if the code uses ESI_EVAL
         or has nested functions that use ESI_{GET,PUT}_{SCOPE,LEXICAL}.

         If true, we need to be more conservative when optimizing the code. */

    BOOL CanInline();
};


class ES_FunctionCode
    : public ES_Code
{
    ES_ProgramCodeStaticReaper *program_reaper;

public:

    ES_FunctionCodeStatic *GetData() { return static_cast<ES_FunctionCodeStatic *>(data); }

    JString *GetName() { return (GetData()->name & 0x80000000u) == 0 ? GetString(GetData()->name) : NULL; }
    JString *GetDebugName() { return (GetData()->name & 0x80000000u) != 0 && GetData()->name != UINT_MAX ? GetString(GetData()->name & ~0x80000000u) : NULL; }

    ES_Class *OptimizeConstructorPropertyCaches(ES_Execution_Context *context, ES_Class *root_class, unsigned &count);

    ES_Class_Compact_Node *klass;
    /**< (Initial) object class of this function's variable object. */

    static void Initialize(ES_FunctionCode *code, ES_FunctionCodeStatic *data, ES_Global_Object *global_object, ES_ProgramCodeStaticReaper *program_reaper)
    {
        ES_Code::Initialize(code, data, global_object, TYPE_FUNCTION);
        code->InitGCTag(GCTAG_ES_FunctionCode, TRUE);

        code->klass = NULL;

        code->program_reaper = ES_ProgramCodeStaticReaper::IncRef(program_reaper);
        ES_ProgramCodeStaticReaper::FunctionCreated(code->program_reaper);

        //code->source.Initialize();

#ifdef ES_NATIVE_PROFILING
        code->self.tv_sec = 0;
        code->self.tv_nsec = 0;
        code->inclusive.tv_sec = 0;
        code->inclusive.tv_nsec = 0;
#endif // ES_NATIVE_PROFILING
    }

    static void Destroy(ES_FunctionCode *code)
    {
        ES_ProgramCodeStaticReaper::FunctionDeleted(code->program_reaper);
        ES_ProgramCodeStaticReaper::DecRef(code->program_reaper);

        ES_Code::Destroy(code);

#ifdef ES_NATIVE_PROFILING
        printf("%p: self = %us %uns, inclusive = %us %uns\n", code, (unsigned) code->self.tv_sec, (unsigned) code->self.tv_nsec, (unsigned) code->inclusive.tv_sec, (unsigned) code->inclusive.tv_nsec);
#endif // ES_NATIVE_PROFILING
    }

    void ConstructClass(ES_Context *context);

#ifdef ES_NATIVE_PROFILING
    struct timespec self, inclusive;
#endif // ES_NATIVE_PROFILING
};

/**
 * "Optional" data relevant to a specific ES_Code object aimed at enabling
 * better secondary (native) code generation.
 *
 * For non-temporary registers (i.e. formals and locals), the computed
 * information consists of ranges of bytecode instructions during which the type
 * of the corresponding register's value is known (interleaved, possibly, with
 * ranges of bytecode instructions during which the value is of an unknown
 * type.)  When the type is known to be one out of two (or more) alternatives,
 * it is recorded as unknown, except if the two alternative types are int32 and
 * double, in which case the type is recorded as known to be double.  (In other
 * words, ESTYPE_DOUBLE means "int32 or double".)  There are two types of
 * ranges: those that start with an actual write to the register that changes
 * the value's type, and those that start at a jump target instruction
 * (i.e. places where separate branches merge) where the jump crossed writes to
 * the register.  The latter type can change the type of the value as well, if
 * it differed between the source and the target of the jump, but the range is
 * recorded even if it doesn't.  In the case of instructions that both read and
 * write the same (non-temporary) register, the previous range determines the
 * read value, and a range starting at that instruction determines the written
 * value.  If an instruction that writes the register is also the target of a
 * jump across writes to the register, there will be an empty range just before
 * signalling that.
 *
 * For temporary registers the computed information rather consists of (mostly)
 * independent "1 write, N reads" segments.  They start at the instruction that
 * writes the register, and ends at the last bytecode instruction at which the
 * value might still be used.  (This may be a backwards jump instruction to
 * before the last actual read of the value, in which case the instruction doing
 * the last actual read is also recorded.)  There may be holes between segments,
 * where the temporary register essentially holds no value (though in the
 * virtual machine it of course retains its previous value.)  Each segment
 * records the index of the instruction writing the register, the type and value
 * written (both optional,) the indeces of the instructions that use the value,
 * the type they'd prefer it was if known and shared by all reads (i.e. the type
 * they are sure to convert it into if it isn't already.)
 *
 * It is possible that several execution branches writes values into the same
 * temporary register, and that value is then read by a shared later branch.
 * This occurs primarily in the case of conditional expressions, such as "a ? b
 * : c" where both "b" and "c" would end up writing a value into the same
 * temporary register.  This is represented by "b" and "c" producing segments
 * with the 'continued_by_later' flag set, and a later segment (immediately
 * following the last segment with the 'continued_by_later' flag set) having the
 * 'continued_from_earlier' flag set.  For a machine code generator, this
 * essentially means that each 'continued_by_later' segment must write the value
 * into the virtual machine register, and the 'continued_from_earlier' segment
 * must unconditionally read it back; or that each 'continued_by_later' must
 * store the value in the same CPU register.
 */
class ES_CodeOptimizationData
{
public:
    ES_CodeOptimizationData(ES_CodeStatic *code)
        : code(code),
          calls_function(FALSE),
          accesses_property(FALSE),
          trampled_variables(FALSE),
          uses_this(FALSE),
          register_accesses(NULL),
          register_access_counts(NULL),
          first_jump_target(NULL),
          last_jump_target(NULL),
          jump_targets_count(0)
    {
    }

    ~ES_CodeOptimizationData();

    ES_CodeStatic *code;
    /**< The code that was analyzed. */

    BOOL calls_function;
    /**< There's at least one ESI_CALL or ESI_CONSTRUCT instruction. */
    BOOL accesses_property;
    /**< There's at least one property access instruction. */
    BOOL trampled_variables;
    /**< The function uses eval() or has nested functions that can write locals
         through its lexical scope. */
    BOOL uses_this;
    /**< Register zero's value is used. */

    class RegisterAccess
    {
    public:
        unsigned cw_index;
        /**< Index of first codeword of the instruction. */

        unsigned data;
        /**< Bit field:

             0-7   : type bit field (see ES_ValueTypeBits)
             8     : 0=read, 1=write
             9     : 0=implicit, 1=explicit
             10    : 0=type is hint, 1=type is forced (for read only)
             11    : 0=value is unknown, 1=value is known (for write only)
             12    : 0=regular entry, 1=terminator entry at end of array
             13-31 : codeword index of related write

             Notes:

             For a write, each set bit in the type bit field represents a type
             the value can possibly have.  For a completely unknown value, all
             bits are set.  For a read, if bit 10 is zero, bits are set in the
             type bit field to signal preferred types, and is thus a hint rather
             than a fact.  If bit 10 is one, the type bit field will have a
             single bit set, and this bit represents the type that the value
             will be converted into when read.

             An explicit read means that the register is used as an input
             operand for the instruction (even if there isn't an actual operand
             codeword naming the register, e.g. in the case of arguments to
             function calls.)  An implicit read is signalled at a jump that
             jumps to a code path that may read this value.

             An explicit write means that the register is used as an output
             operand for the instruction.  An implicit write is signalled for an
             instruction that is the target of a jump from a different code path
             where the register may be written, and at the beginning of the
             analyzed code to signal registers where an initial value is set
             (such as all formal parameters and all local variables.)  The type
             field of an implicit write is the union of the possible values in
             all the joining code paths.

             Related write
             -------------

             For a read, the related write is the codeword index at which the
             most recent write (implicit or explicit) was recorded.  This is
             used to quickly find the write being read in cases where the same
             write is read very many times (to avoid a linear scan backwards in
             the array.)

             For a write, if the value is known, the value is specified by
             specifying the codeword index of the instruction that initially
             wrote it, which will always be an ESI_LOAD_* instruction.

             Termination
             -----------

             The array of register accesses is always terminated by an implicit
             write with the FLAG_IS_TERMINATOR bit set.

         */

        enum Flags
        {
            MASK_TYPE             = 0x00ff,

            FLAG_READ             = 0x0000,
            FLAG_WRITE            = 0x0100,

            FLAG_IMPLICIT         = 0x0000,
            FLAG_EXPLICIT         = 0x0200,

            FLAG_TYPE_IS_HINT     = 0x0000,
            FLAG_TYPE_IS_FORCED   = 0x0400,

            FLAG_VALUE_IS_UNKNOWN = 0x0000,
            FLAG_VALUE_IS_KNOWN   = 0x0800,

            FLAG_IS_TERMINATOR    = 0x1000,

            SHIFT_WRITE_CW_INDEX  = 13,
            MASK_WRITE_CW_INDEX   = ~0u << SHIFT_WRITE_CW_INDEX
        };

        RegisterAccess()
        {
        }

        RegisterAccess(unsigned cw_index, unsigned data)
            : cw_index(cw_index),
              data(data)
        {
        }

        BOOL IsTypeKnown() const { return (data & MASK_TYPE) != MASK_TYPE; }
        unsigned GetType() const { return data & MASK_TYPE; }

        BOOL IsSingleType() const { return GetType() != 0 && (GetType() & (GetType() - 1)) == 0; }
        BOOL IsNumberType() const { return GetType() != 0 && (GetType() & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) == GetType(); }
        ES_ValueType GetSingleType() const { OP_ASSERT(IsSingleType()); return ES_Value_Internal::TypeFromBits(GetType()); }

        BOOL IsRead() const { return (data & FLAG_WRITE) == 0; }
        BOOL IsWrite() const { return (data & FLAG_WRITE) != 0; }

        BOOL IsImplicit() const { return (data & FLAG_EXPLICIT) == 0; }
        BOOL IsExplicit() const { return (data & FLAG_EXPLICIT) != 0; }

        BOOL IsImplicitRead() const { return IsRead() && IsImplicit(); }
        BOOL IsExplicitRead() const { return IsRead() && IsExplicit(); }
        BOOL IsImplicitWrite() const { return IsWrite() && IsImplicit(); }
        BOOL IsExplicitWrite() const { return IsWrite() && IsExplicit(); }

        BOOL IsReadTypeHint() const { return (data & FLAG_TYPE_IS_FORCED) == 0; }
        BOOL IsReadTypeForced() const { return (data & FLAG_TYPE_IS_FORCED) != 0; }

        BOOL IsValueKnown() const { return (data & FLAG_VALUE_IS_KNOWN) != 0; }
        BOOL GetValue(ES_Code *code, ES_Value_Internal &value) const;
        unsigned GetWriteIndex() const { return data >> SHIFT_WRITE_CW_INDEX; }

        BOOL IsTerminator() const { return (data & FLAG_IS_TERMINATOR) != 0; }

        unsigned GetLastReadIndex(BOOL ignore_implicit_writes = FALSE) const;
        unsigned GetNumberOfReads(BOOL ignore_implicit_writes = FALSE) const;

        const RegisterAccess *GetNextRead(BOOL ignore_implicit_writes = FALSE) const;
        const RegisterAccess *GetNextWrite() const;

        BOOL IsWrittenValueRead() const;
        /**< Should only be called on writes.  Returns whether the value written
             here is or might be read. */
    };

    const RegisterAccess *FindWriteAt(unsigned cw_index, unsigned register_index);
    /**< Returns a pointer to the RegisterAccess that represents a write into
         the register at the specified instruction.  This will always be an
         explicit write. */

    const RegisterAccess *FindCorrespondingImplicitWrite(unsigned register_index, const RegisterAccess *implicit_read);
    /**< For an implicit read (at a jump instruction,) find the corresponding
         implicit write (at the jump target instruction.)  If the jump is
         complicated (for instance a table switch jump) NULL is returned. */

    const RegisterAccess *FindReadAt(unsigned cw_index, unsigned register_index);
    /**< Returns a pointer to the RegisterAccess that represents a read from the
         register at the specified instruction.  This will always be an explicit
         read. */

    const RegisterAccess *FindWriteReadAt(unsigned cw_index, unsigned register_index);
    /**< Returns a pointer to the RegisterAccess that represents the write that
         defines the value that is read at 'cw_index'.  This will always be a
         write, either implicit or explicit. */

    const RegisterAccess *const *register_accesses;
    /**< Array of RegisterAccess arrays for each virtual register.  Each such
         array contains a series of writes (implicit or explicit) each
         optionally followed by a series of reads (also implicit or explicit)
         representing uses of the previously written value.

         In cases where multiple types of accesses apply to the same register at
         the same instruction, the order between accesses is always

           1) implicit write
           2) explicit read
           3) explicit write
           4) implicit read

         or, in English, "first we jump to the instruction, then it reads its
         operands, then it writes its result, and then we jump away."  (No
         actual instruction does all that, but most combinations can occur.) */

    const unsigned *register_access_counts;

    class JumpTarget
    {
    public:
        unsigned index;
        /**< Code word index of the instruction that a jump somewhere jumps
             to. */

        unsigned number_of_forward_jumps;
        /**< Number of forward jumps the instruction is a target of. */

        unsigned number_of_backward_jumps;
        /**< Number of backward jumps the instruction is a target of. */

        JumpTarget *previous;
        /**< Previous jump target. */

        JumpTarget *next;
        /**< Next jump target. */
    };

    JumpTarget *first_jump_target, *last_jump_target;
    /**< Sorted list of jump targets (lowest target code word index first.) */

    unsigned jump_targets_count;
    /**< Number of different jump targets. */
};

/** ES_Program represents a block of code to be run.

    It is a static root for garbage collection; it must be deleted explicitly. */

class ES_Program : public ES_GCRoot
{
public:
    ES_Program(ES_Heap *heap, ES_ProgramCode* program) : ES_GCRoot(heap), program(program), heap(heap) {}

    virtual void GCTrace();	   // Make sure 'program' is not garbage collected
    virtual void NewHeap(ES_Heap *heap);

    ES_ProgramCode* program;  // The top-level code for the program
    ES_Heap *heap;
};

class ES_Static_Program
{
public:
    ES_Static_Program(ES_ProgramCodeStatic *program)
        : program(ES_ProgramCodeStatic::IncRef(program))
    {
    }

    ~ES_Static_Program()
    {
        ES_CodeStatic::DecRef(program);
    }

    ES_ProgramCodeStatic *program;
};

#include "modules/ecmascript/carakan/src/compiler/es_code_inlines.h"

#endif // ES_CODE_H
