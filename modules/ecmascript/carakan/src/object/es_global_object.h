/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_GLOBAL_OBJECT_H
#define ES_GLOBAL_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_host_object.h"
#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/util/adt/opvector.h"

class ES_RegExp_Constructor;
class ES_RegExp_Object;
struct RegExpFlags;

class ES_Special_Error_StackTrace;

class ES_Global_Object : public ES_Host_Object
{
public:
    static ES_Global_Object *Make(ES_Context *context, const char *global_object_class_name, BOOL host_object_prototype);

    JString *GetVariableName(unsigned index)
    {
        JString *name;
        variables->Lookup(index, name);
        return name;
    }

    BOOL GetVariableIndex(unsigned &index, JString *name) { return variables->IndexOf(name, index); }
    BOOL GetVariableIsFunction(unsigned index) { return variable_is_function[index]; }

    void PrepareForOptimizeGlobalAccessors(ES_Context *context, unsigned variable_count);

    void AddVariable(ES_Context *context, JString *name, BOOL is_function, BOOL is_configurable);

    ES_Value_Internal &GetVariableValue(unsigned index)
    {
        return variable_values[index];
    }

    static void Initialize(ES_Global_Object *global_object);

    static void Destroy(ES_Global_Object *global_object)
    {
        op_free(global_object->variable_values);
    }

    ES_Class *GetEmptyClass() { return classes[CI_EMPTY]; }

    ES_Object *GetObjectPrototype() { return prototypes[PI_OBJECT]; }
    ES_Class *GetObjectClass() { return classes[CI_OBJECT]; }

    ES_Object *GetArrayPrototype() { return prototypes[PI_ARRAY]; }
    ES_Class *GetArrayClass() { return classes[CI_ARRAY]; }

    ES_Object *GetFunctionPrototype() { return prototypes[PI_FUNCTION]; }
    ES_Class *GetBuiltInFunctionClass() { return classes[CI_BUILTIN_FUNCTION]; }
    ES_Class *GetBuiltInConstructorClass() { return classes[CI_BUILTIN_CONSTRUCTOR]; }
    ES_Class *GetBuiltInGetterSetterClass() { return classes[CI_BUILTIN_CONSTRUCTOR]; }
    ES_Class *&GetNativeFunctionClass() { return classes[CI_NATIVE_FUNCTION]; }
    ES_Class *&GetNativeFunctionWithPrototypeClass() { return classes[CI_NATIVE_FUNCTION_WITH_PROTOTYPE]; }
    ES_Class *GetFunctionPrototypeClass() { return classes[CI_FUNCTION_PROTOTYPE]; }

    ES_Class *GetArrayBufferClass() { return classes[CI_ARRAYBUFFER]; }
    ES_Class *GetTypedArrayClass(unsigned i) { return classes[CI_INT8ARRAY + i]; }
    ES_Class *GetDataViewClass() { return classes[CI_DATAVIEW]; }

    ES_Object *GetStringPrototype() { return prototypes[PI_STRING]; }
    ES_Class *GetStringClass() { return classes[CI_STRING]; }

    ES_Object *GetNumberPrototype() { return prototypes[PI_NUMBER]; }
    ES_Class *GetNumberClass() { return classes[CI_NUMBER]; }

    ES_Object *GetBooleanPrototype() { return prototypes[PI_BOOLEAN]; }
    ES_Class *GetBooleanClass() { return classes[CI_BOOLEAN]; }

    ES_Object *GetRegExpPrototype() { return prototypes[PI_REGEXP]; }
    ES_Class *GetRegExpClass() { return classes[CI_REGEXP]; }
    ES_Class *GetRegExpExecResultClass() { return classes[CI_REGEXP_EXEC_RESULT]; }
    ES_Class *GetRegExpConstructorClass() { return classes[CI_REGEXP_CONSTRUCTOR]; }

    ES_Object *GetDatePrototype() { return prototypes[PI_DATE]; }
    ES_Class *GetDateClass() { return classes[CI_DATE]; }

    ES_Class *GetArgumentsClass() { return classes[CI_ARGUMENTS]; }
    ES_Class *GetStrictArgumentsClass() { return classes[CI_STRICT_ARGUMENTS]; }

    ES_Object *GetErrorPrototype() { return prototypes[PI_ERROR]; }
    ES_Class *GetErrorClass() { return classes[CI_ERROR]; }

    ES_Object *GetTypeErrorPrototype() { return prototypes[PI_TYPE_ERROR]; }
    ES_Class *GetTypeErrorClass() { return classes[CI_TYPE_ERROR]; }

    ES_Object *GetGlobalObjectPrototype() { return prototypes[PI_GLOBAL_OBJECT]; }

    ES_Object *GetNativeErrorPrototype(ES_NativeError type);

    ES_Class *GetEvalErrorClass() { return classes[CI_EVAL_ERROR]; }
    ES_Class *GetRangeErrorClass() { return classes[CI_RANGE_ERROR]; }
    ES_Class *GetReferenceErrorClass() { return classes[CI_REFERENCE_ERROR]; }
    ES_Class *GetSyntaxErrorClass() { return classes[CI_SYNTAX_ERROR]; }
    ES_Class *GetURIErrorClass() { return classes[CI_URI_ERROR]; }

    ES_Object *GetPrimitivePrototype(const ES_Value_Internal &value)
    {
        if (value.IsString())
            return prototypes[PI_STRING];
        else if (value.IsNumber())
            return prototypes[PI_NUMBER];
        else if (value.IsBoolean())
            return prototypes[PI_BOOLEAN];
        else
            return NULL;
    }

    ES_Class *GetPrimitiveClass(const ES_Value_Internal &value)
    {
        if (value.IsString())
            return classes[CI_STRING];
        else if (value.IsNumber())
            return classes[CI_NUMBER];
        else if (value.IsBoolean())
            return classes[CI_BOOLEAN];
        else
            return NULL;
    }

    ES_RegExp_Constructor *GetRegExpConstructor() { return regexp_ctor; }

    SimpleCachedPropertyAccess String_prototype_toString;
    SimpleCachedPropertyAccess Number_prototype_valueOf;

    BOOL IsSimpleStringObject(ES_Object *object)
    {
        if (object->Class() == classes[CI_STRING])
        {
            if (prototypes[PI_STRING]->Class()->Id() != cached_string_prototype_class_id)
                UpdatePrototypeClassCaches();
            return simple_string_objects;
        }
        else
            return FALSE;
    }

    BOOL IsSimpleNumberObject(ES_Object *object)
    {
        if (object->Class() == classes[CI_NUMBER])
        {
            if (prototypes[PI_NUMBER]->Class()->Id() != cached_number_prototype_class_id)
                UpdatePrototypeClassCaches();
            return simple_number_objects;
        }
        else
            return FALSE;
    }

    BOOL IsSimpleDateObject(ES_Object *object)
    {
        if (object->Class() == classes[CI_DATE])
        {
            if (prototypes[PI_DATE]->Class()->Id() != cached_date_prototype_class_id)
                UpdatePrototypeClassCaches();
            return simple_date_objects;
        }
        else
            return FALSE;
    }

    BOOL IsSimpleBooleanObject(ES_Object *object)
    {
        if (object->Class() == classes[CI_BOOLEAN])
        {
            if (prototypes[PI_BOOLEAN]->Class()->Id() != cached_boolean_prototype_class_id)
                UpdatePrototypeClassCaches();
            return simple_boolean_objects;
        }
        else
            return FALSE;
    }

    BOOL GetCachedParsedDate(JString *string, double &utc, double &local)
    {
        if (cached_parsed_date && Equals(string, cached_parsed_date))
        {
            utc = cached_date_utc;
            local = cached_date_local;
            return TRUE;
        }
        else
            return FALSE;
    }
    void SetCachedParsedDate(JString *string, double utc, double local)
    {
        cached_parsed_date = string;
        cached_date_utc = utc;
        cached_date_local = local;
    }

#ifdef ES_NATIVE_SUPPORT
    void InvalidateInlineFunction(ES_Context *context, unsigned index);
#endif // ES_NATIVE_SUPPORT

    ES_RegExp_Object *GetDynamicRegExp(ES_Execution_Context *context, JString *source, RegExpFlags &flags, unsigned flagbits);

    ES_Special_Error_StackTrace *GetSpecialStacktrace() { return special_stacktrace; }
    ES_Special_Error_StackTrace *GetSpecialStack() { return special_stack; }

    ES_Special_Function_Arguments *GetSpecialArguments() { return special_function_arguments; }
    ES_Special_Function_Caller *GetSpecialCaller() { return special_function_caller; }

    ES_Box *GetDefaultFunctionProperties() { return default_function_properties; }
    ES_Box *GetDefaultStrictFunctionProperties() { return default_strict_function_properties; }
    ES_Box *GetDefaultBuiltinFunctionProperties() { return default_builtin_function_properties; }

    BOOL IsDefaultFunctionProperties(ES_Box *properties) { return properties == default_function_properties || properties == default_strict_function_properties; }

    enum ClassIndex
    {
        CI_EMPTY,
        CI_OBJECT,
        CI_BUILTIN_FUNCTION,
        CI_BUILTIN_CONSTRUCTOR,
        CI_NATIVE_FUNCTION,
        CI_NATIVE_FUNCTION_WITH_PROTOTYPE,
        CI_FUNCTION_PROTOTYPE,
        CI_ARRAY,
        CI_STRING,
        CI_NUMBER,
        CI_BOOLEAN,
        CI_REGEXP,
        CI_REGEXP_EXEC_RESULT,
        CI_REGEXP_CONSTRUCTOR,
        CI_DATE,
        CI_ARGUMENTS,
        CI_STRICT_ARGUMENTS,
        CI_ERROR,
        CI_EVAL_ERROR,
        CI_RANGE_ERROR,
        CI_REFERENCE_ERROR,
        CI_SYNTAX_ERROR,
        CI_TYPE_ERROR,
        CI_URI_ERROR,

        CI_ARRAYBUFFER,
        CI_DATAVIEW,
        CI_INT8ARRAY,
        CI_INT16ARRAY,
        CI_INT32ARRAY,
        CI_UINT8ARRAY,
        CI_UINT8CLAMPEDARRAY,
        CI_UINT16ARRAY,
        CI_UINT32ARRAY,
        CI_FLOAT32ARRAY,
        CI_FLOAT64ARRAY,

        CI_MAXIMUM
    };

    enum PrototypeIndex
    {
        PI_OBJECT,
        PI_FUNCTION,
        PI_ARRAY,
        PI_STRING,
        PI_NUMBER,
        PI_BOOLEAN,
        PI_REGEXP,
        PI_DATE,
        PI_ERROR,
        PI_EVAL_ERROR,
        PI_RANGE_ERROR,
        PI_REFERENCE_ERROR,
        PI_SYNTAX_ERROR,
        PI_TYPE_ERROR,
        PI_URI_ERROR,
        PI_GLOBAL_OBJECT,

        PI_MAXIMUM
    };

    BOOL IsPrototypeUnmodified(PrototypeIndex index) { return prototypes[index]->Class()->Id() == prototype_class_ids[index]; }

    ES_Special_Mutable_Access *GetThrowTypeErrorProperty() { return special_function_throw_type_error; }
    ES_Function *GetThrowTypeError() { return throw_type_error; }
    /**< 13.2.3 The [[ThrowTypeError]] Function Object */

private:
    friend class ESMM;
#ifdef ES_NATIVE_SUPPORT
    friend class ES_Native;
#endif // ES_NATIVE_SUPPORT
    friend class ES_Code;
    friend class ES_ArrayBufferBuiltins;
    friend class ES_TypedArrayBuiltins;
    friend class ES_DataViewBuiltins;

    static void MakeNativeErrorObject(ES_Context *context, ES_Class *constructor_class, ES_Global_Object *global_object, int id);

    void SetPrototype(ES_Context *context, PrototypeIndex index, ES_Object *object) { prototypes[index] = object; prototype_class_ids[index] = object->Class()->GetId(context); }

    ES_Identifier_Mutable_List *variables;
    ES_Value_Internal *variable_values;
    BOOL *variable_is_function;

#ifdef ES_NATIVE_SUPPORT
    unsigned immediate_function_serial_nr;
#endif // ES_NATIVE_SUPPORT

    ES_Class *classes[CI_MAXIMUM];
    ES_Object *prototypes[PI_MAXIMUM];
    unsigned prototype_class_ids[PI_MAXIMUM];

    void UpdatePrototypeClassCaches();

    BOOL simple_string_objects;
    BOOL simple_number_objects;
    BOOL simple_date_objects;
    BOOL simple_boolean_objects;

    unsigned cached_string_prototype_class_id;
    unsigned cached_number_prototype_class_id;
    unsigned cached_date_prototype_class_id;
    unsigned cached_boolean_prototype_class_id;

    ES_RegExp_Constructor *regexp_ctor;
    ESRT_Data *rt_data;

    JString *cached_parsed_date;
    double cached_date_utc, cached_date_local;

    ES_Property_Value_Table *dynamic_regexp_cache;
    ES_Property_Value_Table *dynamic_regexp_cache_g;
    ES_Property_Value_Table *dynamic_regexp_cache_i;
    ES_Property_Value_Table *dynamic_regexp_cache_gi;

    ES_Special_Error_StackTrace *special_stacktrace, *special_stack;

    ES_Special_Function_Name *special_function_name;
    ES_Special_Function_Length *special_function_length;
    ES_Special_Function_Prototype *special_function_prototype;
    ES_Special_Function_Arguments *special_function_arguments;
    ES_Special_Function_Caller *special_function_caller;
    ES_Special_Mutable_Access *special_function_throw_type_error;

    ES_Box *default_function_properties, *default_builtin_function_properties, *default_strict_function_properties;

    ES_Function *throw_type_error;
};

#endif // ES_GLOBAL_OBJECT_H
