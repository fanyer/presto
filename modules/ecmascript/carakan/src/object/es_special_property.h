/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_SPECIAL_PROPERTY_H
#define ES_SPECIAL_PROPERTY_H

#include "modules/ecmascript/carakan/src/object/es_error_object.h"

/**
 * ES_Special_Property is a common interface for special properties that
 * make use of the fact that ES_Boxed is a legal value for
 * ES_Value_Internals. ES_Special_Property is intended to be handled
 * exclusively by ES_Object and should never escape out to the
 * virtual machine.
 */
class ES_Special_Property : public ES_Boxed
{
public:
    PutResult SpecialPutL(ES_Context *context, ES_Object *this_object, const ES_Value_Internal &value, const ES_Value_Internal &this_value);
    /**< Handles special types of Put on this_object. Can call
         ES_Execution_Context::CallFunction if value is a setter.

         @param this_object The object on which the special property was
                            defined.

         @param this_value The 'this' value for calls to accessor functions.
                           This is the Reference's base value, and can be a
                           primitive.  If it is an object, it's not necessarily
                           the same as 'this_object' (which can be an object
                           from the prototype chain.) */

    GetResult SpecialGetL(ES_Context *context, ES_Object *this_object, ES_Value_Internal &value, const ES_Value_Internal &this_value);
    /**< Handles special types of Put on this_object. Can call
         ES_Execution_Context::CallFunction if value is a setter.

         @param this_object The object on which the special property was
                            defined.

         @param this_value The 'this' value for calls to accessor functions.
                           This is the Reference's base value, and can be a
                           primitive.  If it is an object, it's not necessarily
                           the same as 'this_object' (which can be an object
                           from the prototype chain.) */

    inline BOOL
    IsAliasedProperty() const
    {
        return GCTag() == GCTAG_ES_Special_Aliased;
    }

    inline BOOL
    IsMutableAccess() const
    {
        return GCTag() == GCTAG_ES_Special_Mutable_Access;
    }

    inline BOOL
    IsGlobalVariable() const
    {
        return GCTag() == GCTAG_ES_Special_Global_Variable;
    }

    inline BOOL
    IsFunctionArguments() const
    {
        return GCTag() == GCTAG_ES_Special_Function_Arguments;
    }

    inline BOOL
    IsFunctionCaller() const
    {
        return GCTag() == GCTAG_ES_Special_Function_Caller;
    }

    inline BOOL
    IsRegExpCapture() const
    {
        return GCTag() == GCTAG_ES_Special_RegExp_Capture;
    }

    BOOL WillCreatePropertyOnGet(ES_Object *this_object) const;
};

/**
 * ES_Special_Mutable_Access wraps two ES_Functions that impelement the
 * getter and setter for a property.
 */
class ES_Special_Mutable_Access : public ES_Special_Property
{
public:
    static ES_Special_Mutable_Access *Make(ES_Context *context, ES_Function *ma_getter, ES_Function *ma_setter);
    static void Initialize(ES_Special_Mutable_Access *self, ES_Function *ma_getter, ES_Function *ma_setter);

    ES_Function *getter;
    ES_Function *setter;
};

/**
 * ES_Special_Aliased wraps a ES_Value_Internal * to be able to read and write a
 * property from separate locations.  Note that the location must be traced
 * independently (for instance because it is a VM register,) it will not be
 * traced through the ES_Special_Aliased object.
 */
class ES_Special_Aliased : public ES_Special_Property
{
public:
    static ES_Special_Aliased *Make(ES_Context *context, ES_Value_Internal *alias);
    static void Initialize(ES_Special_Aliased *self, ES_Value_Internal *alias);

    inline ES_Value_Internal Zap() const { return *property; }

    ES_Value_Internal *property;

#ifndef SIXTY_FOUR_BIT
    void *padding;
    /**< Makes sizeof(ES_Special_Aliased) a multiple of 8 to be able to split an
       array of ES_Special_Aliased. */
#endif // SIXTY_FOUR_BIT
};

/**
 * ES_Special_Global_Variable makes global variables available as ordinary
 * properties on the global object.
 */
class ES_Special_Global_Variable : public ES_Special_Property
{
public:
    static ES_Special_Global_Variable *Make(ES_Context *context, unsigned index);
    static void Initialize(ES_Special_Global_Variable *self, unsigned index);

    unsigned index;
};

/**
 * ES_Special_Function_Name returns the name of a built-in function
 * using ES_Function::data.builtin.information.
 */
class ES_Special_Function_Name : public ES_Special_Property
{
public:
    static ES_Special_Function_Name *Make(ES_Context *context);
    static void Initialize(ES_Special_Function_Name *self);
};

/**
 * ES_Special_Function_Name returns the length of a built-in function
 * using ES_Function::data.builtin.information.
 */
class ES_Special_Function_Length : public ES_Special_Property
{
public:
    static ES_Special_Function_Length *Make(ES_Context *context);
    static void Initialize(ES_Special_Function_Length *self);
};

/**
 * ES_Special_Function_Prototype returns 'undefined' when read and
 * copies the properties array when written.
 */
class ES_Special_Function_Prototype : public ES_Special_Property
{
public:
    static ES_Special_Function_Prototype *Make(ES_Context *context);
    static void Initialize(ES_Special_Function_Prototype *self);
};

/**
 * ES_Special_Function_Arguments dynamically creates arguments objects
 * via 'functionObject.arguments' for functions that don't use their
 * own arguments object.
 */
class ES_Special_Function_Arguments : public ES_Special_Property
{
public:
    static ES_Special_Function_Arguments *Make(ES_Context *context);
    static void Initialize(ES_Special_Function_Arguments *self);
};

/**
 * ES_Special_Function_Caller searches the call stack to find the most recent
 * caller of the subject.
 */
class ES_Special_Function_Caller : public ES_Special_Property
{
public:
    static ES_Special_Function_Caller *Make(ES_Context *context);
    static void Initialize(ES_Special_Function_Caller *self);
};

/**
 * ES_Special_RegExp_Capture dynamically creates a string for the N:th capture
 * of the most recent RegExp match, when reading the $1-$9 properties off the
 * RegExp constructor.
 */
class ES_Special_RegExp_Capture : public ES_Special_Property
{
public:
    static ES_Special_RegExp_Capture *Make(ES_Context *context, unsigned index);
    static void Initialize(ES_Special_RegExp_Capture *self, unsigned index);

    unsigned index;
};

/**
 * ES_Special_Error_StackTrace dynamically generates a stacktrace string.
 */
class ES_Special_Error_StackTrace : public ES_Special_Property
{
public:
    static ES_Special_Error_StackTrace *Make(ES_Context *context, ES_Error::StackTraceFormat format);
    static void Initialize(ES_Special_Error_StackTrace *self, ES_Error::StackTraceFormat format);

    ES_Error::StackTraceFormat format;
};

#endif // ES_SPECIAL_PROPERTY_H
