/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2006
 *
 * Object header for garbage-collected storage.
 *
 * @author Lars T Hansen
 *
 * @see es_value.h for a general discussion of values and their representation.
 * @see es_boxed.h for documentation about objects in the heap.
 * @see es_collector.h for documentation about garbage collection.
 */

#ifndef ES_HEAP_OBJECT_H
#define ES_HEAP_OBJECT_H

/**
 * GC_Tag enumerates the tag values relevant to garbage collection.
 *
 * The GC_Tag value is used alone or in conjunction with a JS_Class_ID
 * to figure out the representation of anything derived from ES_Boxed.
 */
enum GC_Tag
{
    GCTAG_free                = 0, // Free storage

    // The following are used if the JS_Class_ID is CLASSID_NONE

    // These don't need further tracing; they are just marked.
    GCTAG_JStringStorage,
    GCTAG_ES_Box,

    // These need further tracing; they are added to the mark stack.
    GCTAG_JString,
    GCTAG_FIRST_THAT_NEED_TRACING = GCTAG_JString,
    GCTAG_JStringSegmented,
    GCTAG_ES_Identifier_Array,
    GCTAG_ES_IdentifierCell_Array,
    GCTAG_ES_Property_Table,
    GCTAG_ES_Property_Value_Table,
    GCTAG_ES_Compact_Indexed_Properties,
    GCTAG_ES_Sparse_Indexed_Properties,
    GCTAG_ES_Byte_Array_Indexed,
    GCTAG_ES_Type_Array_Indexed,
    GCTAG_ES_Identifier_Hash_Table,
    GCTAG_ES_Identifier_List,
    GCTAG_ES_Identifier_Boxed_Hash_Table,
    GCTAG_ES_Boxed_Array,
    GCTAG_ES_Boxed_List,
    GCTAG_FIRST_CLASS,
    GCTAG_ES_Class_Node = GCTAG_FIRST_CLASS,
    GCTAG_ES_Class_Compact_Node,
    GCTAG_ES_Class_Compact_Node_Frozen,
    GCTAG_ES_Class_Singleton,
    GCTAG_ES_Class_Hash,
    GCTAG_LAST_CLASS = GCTAG_ES_Class_Hash,
    GCTAG_ES_Class_Extra,
    GCTAG_ES_Class_Data,
    GCTAG_ES_ProgramCode,
    GCTAG_ES_FunctionCode,
    GCTAG_ES_Properties,

    GCTAG_FIRST_SPECIAL,
    GCTAG_ES_Special_Aliased = GCTAG_FIRST_SPECIAL,
    GCTAG_ES_Special_Mutable_Access,
    GCTAG_ES_Special_Global_Variable,
    GCTAG_ES_Special_Function_Name,
    GCTAG_ES_Special_Function_Length,
    GCTAG_ES_Special_Function_Prototype,
    GCTAG_ES_Special_Function_Arguments,
    GCTAG_ES_Special_Function_Caller,
    GCTAG_ES_Special_RegExp_Capture,
    GCTAG_ES_Special_Error_StackTrace,
    GCTAG_LAST_SPECIAL = GCTAG_ES_Special_Error_StackTrace,

    // These apply if the JS_Class_ID is not CLASSID_NONE.  They provide
    // extra information for the garbage collector

    GCTAG_ES_Object,             // Generically derived from ES_Object
    GCTAG_ES_Object_Number,      // ES_Number_Object
    GCTAG_ES_Object_String,      // ES_String_Object
    GCTAG_ES_Object_Date,        // ES_Date_Object
    GCTAG_ES_Object_Boolean,     // ES_Boolean_Object
    GCTAG_ES_Object_Array,       // Builtin array object, but really just a plain ES_Object
    GCTAG_ES_Object_Function,    // Either ES_Function, ES_Host_Function or ES_Global_Object, depending on other bits in the header
    GCTAG_ES_Object_RegExp,      // ES_RegExp_Object
    GCTAG_ES_Object_RegExp_CTOR, // ES_RegExp_Constructor
    GCTAG_ES_Object_Error,       // ES_Error
    GCTAG_ES_Object_Arguments,   // ES_Arguments_Object
    GCTAG_ES_Object_Variables,   // Activation/variables object, but really just a plain ES_Object
    GCTAG_ES_Object_TypedArray,  // ES_TypedArray
    GCTAG_ES_Object_ArrayBuffer, // ES_ArrayBuffer

    GCTAG_UNINITIALIZED
};

enum ES_BuiltInFunction
{
    ES_BUILTIN_FN_NONE,
    ES_BUILTIN_FN_sin,
    ES_BUILTIN_FN_cos,
    ES_BUILTIN_FN_tan,
    ES_BUILTIN_FN_sqrt,
    ES_BUILTIN_FN_floor,
    ES_BUILTIN_FN_ceil,
    ES_BUILTIN_FN_pow,
    ES_BUILTIN_FN_abs,
    ES_BUILTIN_FN_log,
    ES_BUILTIN_FN_parseInt,
    ES_BUILTIN_FN_push,
    ES_BUILTIN_FN_fromCharCode,
    ES_BUILTIN_FN_charAt,
    ES_BUILTIN_FN_charCodeAt,
    ES_BUILTIN_FN_OTHER,
    ES_BUILTIN_FN_LAST_INLINED = ES_BUILTIN_FN_OTHER,

    ES_BUILTIN_FN_apply,
    ES_BUILTIN_FN_eval,
    ES_BUILTIN_FN_DISABLE
};

/**
 * ES_Header is the data type for object headers in the garbage-collected heap.
 *
 * The header needs to be as small as possible.  It should be exactly 4 bytes long
 * on all platforms, even on 64-bit systems.
 *
 * There are no operations defined directly on ES_Header.  Instead, they are all
 * defined on the union ES_Boxed, the members of which all contain an ES_Header
 * as the first member.  See es_boxed.h.
 */
class ES_Header
{
public:
    enum
    {
        // Shared (omni-present) bits:
        MASK_GCTAG                 = (1 << 6) - 1,
        MASK_MARK                  = 1 << 6,
        MASK_NEED_DESTROY          = 1 << 7,
        MASK_NO_FOREIGN_TRACE      = 1 << 8,
        MASK_ON_LARGE_PAGE         = 1 << 9,

        // Bits on JString:
        MASK_IS_BUILTIN_STRING     = 1 << 10, ///< String is one of the builtin strings so we have to share it before append.

        // Bits on ES_Indexed_Properties:
        MASK_HAS_SPECIAL           = 1 << 10,
        MASK_HAS_READ_ONLY         = 1 << 11,
        MASK_HAS_ATTRIBUTES        = 1 << 12,
        MASK_COPY_ON_WRITE         = 1 << 13,

        SIZE_SHIFT                  = 16,
        MASK_SIZE                   = 0xffff0000u,
        MASK_BITS                   = 0x0000ffffu
    };

    unsigned header;
    /**<  The high 16 bits of 'header' contains the object's size in bytes, the low
          16 bits contains header bits described by the above enum. */
};

#endif // ES_HEAP_OBJECT_H
