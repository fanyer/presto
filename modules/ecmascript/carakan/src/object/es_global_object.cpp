/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_array_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_boolean_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_date_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_error_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_function_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_global_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_json_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_math_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_number_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_object_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_regexp_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_string_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_typedarray_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"

#ifdef ES_DEBUG_BUILTINS
#include "modules/ecmascript/carakan/src/builtins/es_debug_builtins.h"
#endif // ES_DEBUG_BUILTINS

/* static */ ES_Global_Object *
ES_Global_Object::Make(ES_Context *context, const char *global_object_class_name, BOOL host_object_prototype)
{
    OP_ASSERT(global_object_class_name);

    ES_Global_Object *global_object;

    GC_ALLOCATE(context, global_object, ES_Global_Object, (global_object));
    ES_CollectorLock gclock(context);

    global_object->variable_values = static_cast<ES_Value_Internal *>(op_malloc(sizeof(ES_Value_Internal) * 8 + sizeof(BOOL) * 8));
    if (!global_object->variable_values)
        context->AbortOutOfMemory();

    global_object->variable_is_function = reinterpret_cast<BOOL *>(global_object->variable_values + 8);

    global_object->variables = static_cast<ES_Identifier_Mutable_List*>(ES_Identifier_List::Make(context, 8));
    global_object->rt_data = context->rt_data;

    global_object->special_function_name = ES_Special_Function_Name::Make(context);
    global_object->special_function_length = ES_Special_Function_Length::Make(context);
    global_object->special_function_prototype = ES_Special_Function_Prototype::Make(context);
    global_object->special_function_arguments = ES_Special_Function_Arguments::Make(context);
    global_object->special_function_caller = ES_Special_Function_Caller::Make(context);

    JString **idents = context->rt_data->idents;

    global_object->classes[CI_EMPTY] = ES_Class::MakeRoot(context, NULL, "Object", idents[ESID_Object]);

    // Object prototype and class

    global_object->SetPrototype(context, PI_OBJECT, ES_Object::MakePrototypeObject(context, global_object->classes[CI_OBJECT]));

    ES_Object *global_object_prototype = global_object->prototypes[PI_OBJECT];

    // Specialised class for built in prototypes, essentially the same as CI_OBJECT but with a few extra properties
    if (host_object_prototype)
        global_object_prototype = static_cast<ES_Object *>(ES_Host_Object::Make(context, NULL, global_object_prototype, global_object_class_name));

    ES_Class *global_object_class = ES_Class::MakeSingleton(context, global_object_prototype, global_object_class_name);

    global_object->prototypes[PI_GLOBAL_OBJECT] = global_object_prototype;
    global_object->prototype_class_ids[PI_GLOBAL_OBJECT] = global_object_class->GetId(context);

    ES_Host_Object::Initialize(global_object, TRUE, global_object_class); // host_object is set later in ES_Runtime::Construct

    global_object->object_bits |= MASK_IS_GLOBAL_OBJECT;

    // Function prototype object class

    ES_Class *function_class;
    global_object->SetPrototype(context, PI_FUNCTION, ES_Function::MakePrototypeObject(context, global_object, function_class));

    ES_Class *native_function_class = NULL, *native_function_with_proto_class = NULL, *builtin_function_class = NULL;

    // create sub-object classes for native and builtin functions.
    ES_Function::CreateFunctionClasses(context, function_class, native_function_class, native_function_with_proto_class, builtin_function_class);

    global_object->classes[CI_BUILTIN_FUNCTION] = builtin_function_class;
    global_object->classes[CI_NATIVE_FUNCTION]  = native_function_class;
    global_object->classes[CI_NATIVE_FUNCTION_WITH_PROTOTYPE] = native_function_with_proto_class;

    unsigned size = native_function_class->SizeOf(5);
    ES_Box *properties = global_object->default_function_properties = ES_Box::MakeStrict(context, size);
    ES_Object::Initialize(properties);
    char *storage = properties->Unbox();

    OP_ASSERT(properties->Size() == size);

    ES_Layout_Info layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::LENGTH));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_length, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NAME));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_name, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NATIVE_PROTOTYPE));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_prototype, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NATIVE_ARGUMENTS));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_arguments, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NATIVE_CALLER));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_caller, layout.GetStorageType());

    size = builtin_function_class->SizeOf(4);
    properties = global_object->default_builtin_function_properties = ES_Box::MakeStrict(context, size);
    ES_Object::Initialize(properties);
    storage = properties->Unbox();

    OP_ASSERT(properties->Size() == size);

    layout = builtin_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::LENGTH));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_length, layout.GetStorageType());

    layout = builtin_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NAME));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_name, layout.GetStorageType());

    ES_Value_Internal null; null.SetNull();

    layout = builtin_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::BUILTIN_ARGUMENTS));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), null, layout.GetStorageType());

    layout = builtin_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::BUILTIN_CALLER));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), null, layout.GetStorageType());

    size = native_function_class->SizeOf(5);
    properties = global_object->default_strict_function_properties = ES_Box::MakeStrict(context, size);
    ES_Object::Initialize(properties);
    storage = properties->Unbox();

    OP_ASSERT(properties->Size() == size);

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::LENGTH));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_length, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NAME));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_name, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NATIVE_PROTOTYPE));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_prototype, layout.GetStorageType());

    global_object->throw_type_error = ES_Function::Make(context, global_object, 0, ES_GlobalBuiltins::ThrowTypeError, 0, TRUE);
    global_object->special_function_throw_type_error = ES_Special_Mutable_Access::Make(context, global_object->throw_type_error, global_object->throw_type_error);

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NATIVE_ARGUMENTS));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_throw_type_error, layout.GetStorageType());

    layout = native_function_class->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(ES_Function::NATIVE_CALLER));
    ES_Value_Internal::Memcpy(storage + layout.GetOffset(), global_object->special_function_throw_type_error, layout.GetStorageType());

    // the function prototype is populated here instead of ES_Function::MakePrototypeObject
    ES_FunctionBuiltins::PopulatePrototype(context, global_object, global_object->prototypes[PI_FUNCTION]);

    // Function instance prototype class
    ES_Class_Node *function_prototype_class0 = ES_Class::MakeRoot(context, global_object->prototypes[PI_OBJECT], "Object", idents[ESID_Object]);
    global_object->classes[CI_FUNCTION_PROTOTYPE] = function_prototype_class0->ExtendWithL(context, idents[ESID_constructor], ES_Property_Info(DE), ES_STORAGE_WHATEVER, FALSE);

    // Specialised class for built in constructors, essentially the same as CI_FUNCTION, but with RO, DD, DE properties

    ES_Class *constructor_class0 = global_object->classes[CI_BUILTIN_FUNCTION]->GetRootClass();
    constructor_class0 = ES_Class::ExtendWithL(context, constructor_class0, idents[ESID_length], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);
    constructor_class0 = ES_Class::ExtendWithL(context, constructor_class0, idents[ESID_name], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);
    constructor_class0 = ES_Class::ExtendWithL(context, constructor_class0, idents[ESID_prototype], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER);
    global_object->classes[CI_BUILTIN_CONSTRUCTOR] = constructor_class0;
    // Note: CI_BUILTIN_CONSTRUCTOR is also used by the functions that getter/setter
    // lookups return for host properties.

    // the object prototype is populated here instead of ES_Object::MakePrototypeObject
    ES_ObjectBuiltins::PopulatePrototype(context, global_object, global_object->prototypes[PI_OBJECT]);

    // Object constructor (after Function prototype and class, because it's a function)
    ES_Object *object_constructor = ES_Function::Make(context, constructor_class0, global_object, 1, ES_ObjectBuiltins::constructor, ES_ObjectBuiltins::constructor, ESID_Object, NULL, global_object->prototypes[PI_OBJECT]);

    ES_ObjectConstructorBuiltins::PopulateConstructor(context, global_object, object_constructor);

    global_object->InitPropertyL(context, idents[ESID_Object], object_constructor, DE);
    global_object->prototypes[PI_OBJECT]->InitPropertyL(context, idents[ESID_constructor], object_constructor, DE);

    // Function constructor

    ES_Object *function_constructor = ES_Function::Make(context, constructor_class0, global_object, 1, ES_FunctionBuiltins::constructor, ES_FunctionBuiltins::constructor, ESID_Function, NULL, global_object->prototypes[PI_FUNCTION]);

    global_object->InitPropertyL(context, idents[ESID_Function], function_constructor, DE);
    global_object->prototypes[PI_FUNCTION]->InitPropertyL(context, idents[ESID_constructor], function_constructor, DE);

    // Array

    global_object->SetPrototype(context, PI_ARRAY, ES_Array::MakePrototypeObject(context, global_object, global_object->classes[CI_ARRAY]));

    ES_Object *array_constructor = ES_Function::Make(context, constructor_class0, global_object, 1, ES_ArrayBuiltins::constructor, ES_ArrayBuiltins::constructor, ESID_Array, NULL, global_object->prototypes[PI_ARRAY]);

    global_object->InitPropertyL(context, idents[ESID_Array], array_constructor, DE);
    global_object->prototypes[PI_ARRAY]->InitPropertyL(context, idents[ESID_constructor], array_constructor, DE);

    ES_ArrayBuiltins::PopulateConstructor(context, global_object, array_constructor);

    // String

    global_object->SetPrototype(context, PI_STRING, ES_String_Object::MakePrototypeObject(context, global_object, global_object->classes[CI_STRING]));

    ES_Object *string_constructor = ES_Function::Make(context, constructor_class0, global_object, 1, ES_StringBuiltins::constructor_call, ES_StringBuiltins::constructor_construct, ESID_String, NULL, global_object->prototypes[PI_STRING]);

    ES_StringBuiltins::PopulateConstructor(context, global_object, string_constructor);

    global_object->InitPropertyL(context, idents[ESID_String], string_constructor, DE);
    global_object->prototypes[PI_STRING]->InitPropertyL(context, idents[ESID_constructor], string_constructor, DE);

    global_object->String_prototype_toString.Initialize(global_object->prototypes[PI_STRING], idents[ESID_toString]);

    // Number

    global_object->SetPrototype(context, PI_NUMBER, ES_Number_Object::MakePrototypeObject(context, global_object, global_object->classes[CI_NUMBER]));

    ES_Object *number_constructor = ES_Function::Make(context, constructor_class0, global_object, 1, ES_NumberBuiltins::constructor_call, ES_NumberBuiltins::constructor_construct, ESID_Number, NULL, global_object->prototypes[PI_NUMBER]);

    ES_NumberBuiltins::PopulateConstructor(context, number_constructor);

    global_object->InitPropertyL(context, idents[ESID_Number], number_constructor, DE);
    global_object->prototypes[PI_NUMBER]->InitPropertyL(context, idents[ESID_constructor], number_constructor, DE);

    global_object->Number_prototype_valueOf.Initialize(global_object->prototypes[PI_NUMBER], idents[ESID_valueOf]);

    // Boolean

    global_object->SetPrototype(context, PI_BOOLEAN, ES_Boolean_Object::MakePrototypeObject(context, global_object, global_object->classes[CI_BOOLEAN]));

    ES_Object *boolean_constructor = ES_Function::Make(context, constructor_class0, global_object, 1, ES_BooleanBuiltins::constructor_call, ES_BooleanBuiltins::constructor_construct, ESID_Boolean, NULL, global_object->prototypes[PI_BOOLEAN]);

    global_object->InitPropertyL(context, idents[ESID_Boolean], boolean_constructor, DE);
    global_object->prototypes[PI_BOOLEAN]->InitPropertyL(context, idents[ESID_constructor], boolean_constructor, DE);

    // RegExp

    global_object->SetPrototype(context, PI_REGEXP, ES_RegExp_Object::MakePrototypeObject(context, global_object, global_object->classes[CI_REGEXP]));

    ES_Class_Singleton *regexp_ctor_class = ES_Class::MakeSingleton(context, global_object->prototypes[PI_FUNCTION], "Function", idents[ESID_Function]);

    regexp_ctor_class->AddL(context, idents[ESID_length], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_name], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_prototype], ES_Property_Info(RO|DE|DD), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_input], ES_Property_Info(DD), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar1], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar2], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar3], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar4], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar5], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar6], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar7], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar8], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar9], ES_Property_Info(RO|DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar_underscore], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_lastMatch], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar_ampersand], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_lastParen], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar_plus], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_leftContext], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar_grave_accent], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_rightContext], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar_apostophe], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_multiline], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);
    regexp_ctor_class->AddL(context, idents[ESID_dollar_asterisk], ES_Property_Info(DD|SP), ES_STORAGE_WHATEVER, FALSE);

    global_object->classes[CI_REGEXP_CONSTRUCTOR] = regexp_ctor_class;

    ES_Object *regexp_constructor = global_object->regexp_ctor = ES_RegExp_Constructor::Make(context, global_object);

    global_object->InitPropertyL(context, idents[ESID_RegExp], regexp_constructor, DE);
    global_object->prototypes[PI_REGEXP]->InitPropertyL(context, idents[ESID_constructor], regexp_constructor, DE);

    ES_Class *exec_result_class = ES_Class::ExtendWithL(context, global_object->classes[CI_ARRAY], idents[ESID_index], ES_Property_Info(0), ES_STORAGE_WHATEVER);
    global_object->classes[CI_REGEXP_EXEC_RESULT] = ES_Class::ExtendWithL(context, exec_result_class, idents[ESID_input], ES_Property_Info(0), ES_STORAGE_WHATEVER);

    // Date
    global_object->SetPrototype(context, PI_DATE, ES_Date_Object::MakePrototypeObject(context, global_object, global_object->classes[CI_DATE]));

    ES_Object *date_constructor = ES_Function::Make(context, constructor_class0, global_object, 7, ES_DateBuiltins::constructor_call, ES_DateBuiltins::constructor_construct, ESID_Date, NULL, global_object->prototypes[PI_DATE]);

    ES_DateBuiltins::PopulateConstructor(context, global_object, date_constructor);

    global_object->prototypes[PI_DATE]->InitPropertyL(context, idents[ESID_constructor], date_constructor, DE);
    global_object->InitPropertyL(context, idents[ESID_Date], date_constructor, DE);

    // Math

    ES_Class_Singleton *math_class = ES_Class::MakeSingleton(context, global_object->prototypes[PI_OBJECT], "Math", idents[ESID_Math], ES_MathBuiltins::ES_MathBuiltinsCount);

    ES_MathBuiltins::PopulateMathClass(context, math_class);

    ES_Object *math = ES_Object::Make(context, math_class);

    ES_MathBuiltins::PopulateMath(context, global_object, math);

    global_object->InitPropertyL(context, idents[ESID_Math], math, DE);

    // JSON

    ES_Class_Singleton *json_class = ES_Class::MakeSingleton(context, global_object->prototypes[PI_OBJECT], "JSON", idents[ESID_JSON], ES_JsonBuiltins::ES_JsonBuiltinsCount);

    ES_JsonBuiltins::PopulateJsonClass(context, json_class);

    ES_Object *json = ES_Object::Make(context, json_class);

    ES_JsonBuiltins::PopulateJson(context, global_object, json);

    global_object->InitPropertyL(context, idents[ESID_JSON], json, DE);

    // Typed Arrays
    ES_ArrayBufferBuiltins::PopulateGlobalObject(context, global_object);
    ES_TypedArrayBuiltins::PopulateGlobalObject(context, global_object);
    ES_DataViewBuiltins::PopulateGlobalObject(context, global_object);

    // Arguments Object
    ES_Class_Node *arguments_class_base = ES_Class::MakeRoot(context, global_object->prototypes[PI_OBJECT], "Arguments", idents[ESID_Arguments]);
    arguments_class_base = arguments_class_base->ExtendWithL(context, idents[ESID_length], ES_Property_Info(DE), ES_STORAGE_WHATEVER, FALSE);

    ES_Class_Node *arguments_class = arguments_class_base->ExtendWithL(context, idents[ESID_callee], ES_Property_Info(DE), ES_STORAGE_WHATEVER, FALSE);
    global_object->classes[CI_ARGUMENTS] = arguments_class;

    // Arguments Object for strict mode functions

    ES_Class_Node *strict_arguments_class = arguments_class_base->ExtendWithL(context, idents[ESID_callee], ES_Property_Info(DE|DD|SP), ES_STORAGE_BOXED, FALSE);
    strict_arguments_class = strict_arguments_class->ExtendWithL(context, idents[ESID_caller], ES_Property_Info(DE|DD|SP), ES_STORAGE_BOXED, FALSE);
    global_object->classes[CI_STRICT_ARGUMENTS] = strict_arguments_class;

    // Error

    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_Error);
    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_EvalError);
    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_RangeError);
    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_ReferenceError);
    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_SyntaxError);
    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_TypeError);
    MakeNativeErrorObject(context, constructor_class0, global_object, ESID_URIError);

    global_object->special_stacktrace = ES_Special_Error_StackTrace::Make(context, ES_Error::FORMAT_READABLE);
    global_object->special_stack = ES_Special_Error_StackTrace::Make(context, ES_Error::FORMAT_MOZILLA);

#ifdef ES_DEBUG_BUILTINS
    // Debug
    JString *debug_name = context->rt_data->GetSharedString("Debug", 5);
    ES_Class_Singleton *debug_class = ES_Class::MakeSingleton(context, global_object->prototypes[PI_OBJECT], "Debug", debug_name, ES_DebugBuiltins::ES_DebugBuiltinsCount);
    ES_DebugBuiltins::PopulateDebugClass(context, debug_class);
    ES_Object *debug = ES_Object::Make(context, debug_class);
    ES_DebugBuiltins::PopulateDebug(context, global_object, debug);
    global_object->InitPropertyL(context, debug_name, debug, DE);
#endif // ES_DEBUG_BUILTINS

    // Global Object

    ES_GlobalBuiltins::PopulateGlobalObject(context, global_object);

    OP_ASSERT(global_object->prototypes[PI_OBJECT]->GetSubObjectClass() == global_object->classes[CI_OBJECT]);
    // Builtins and native functions are sub-object classes of PI_FUNCTION's.
    OP_ASSERT(global_object->prototypes[PI_FUNCTION]->GetSubObjectClass()->GetRootClass() == global_object->classes[CI_BUILTIN_FUNCTION]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_FUNCTION]->GetSubObjectClass()->GetRootClass() == global_object->classes[CI_NATIVE_FUNCTION]->GetRootClass());

    OP_ASSERT(global_object->prototypes[PI_ARRAY]->GetSubObjectClass() == global_object->classes[CI_ARRAY]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_STRING]->GetSubObjectClass() == global_object->classes[CI_STRING]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_NUMBER]->GetSubObjectClass() == global_object->classes[CI_NUMBER]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_BOOLEAN]->GetSubObjectClass() == global_object->classes[CI_BOOLEAN]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_REGEXP]->GetSubObjectClass() == global_object->classes[CI_REGEXP]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_DATE]->GetSubObjectClass() == global_object->classes[CI_DATE]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_ERROR]->GetSubObjectClass() == global_object->classes[CI_ERROR]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_EVAL_ERROR]->GetSubObjectClass() == global_object->classes[CI_EVAL_ERROR]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_RANGE_ERROR]->GetSubObjectClass() == global_object->classes[CI_RANGE_ERROR]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_REFERENCE_ERROR]->GetSubObjectClass() == global_object->classes[CI_REFERENCE_ERROR]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_SYNTAX_ERROR]->GetSubObjectClass() == global_object->classes[CI_SYNTAX_ERROR]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_TYPE_ERROR]->GetSubObjectClass() == global_object->classes[CI_TYPE_ERROR]->GetRootClass());
    OP_ASSERT(global_object->prototypes[PI_URI_ERROR]->GetSubObjectClass() == global_object->classes[CI_URI_ERROR]->GetRootClass());
    global_object->prototypes[PI_OBJECT]->AddInstance(context, global_object_class, TRUE);

    return global_object;
}

/* static */ void
ES_Global_Object::Initialize(ES_Global_Object *global_object)
{
    global_object->variables = NULL;
    global_object->variable_values = NULL;
#ifdef ES_NATIVE_SUPPORT
    global_object->immediate_function_serial_nr = 0;
#endif // ES_NATIVE_SUPPORT
    global_object->String_prototype_toString.Initialize(NULL, NULL);
    global_object->Number_prototype_valueOf.Initialize(NULL, NULL);
    global_object->cached_string_prototype_class_id = ES_Class::NOT_CACHED_CLASS_ID;
    global_object->cached_number_prototype_class_id = ES_Class::NOT_CACHED_CLASS_ID;
    global_object->cached_date_prototype_class_id = ES_Class::NOT_CACHED_CLASS_ID;
    global_object->cached_boolean_prototype_class_id = ES_Class::NOT_CACHED_CLASS_ID;
    global_object->regexp_ctor = NULL;
    global_object->cached_parsed_date = NULL;
    global_object->dynamic_regexp_cache = NULL;
    global_object->dynamic_regexp_cache_g = NULL;
    global_object->dynamic_regexp_cache_i = NULL;
    global_object->dynamic_regexp_cache_gi = NULL;
    global_object->special_stacktrace = global_object->special_stack = NULL;
    global_object->special_function_name = NULL;
    global_object->special_function_length = NULL;
    global_object->special_function_prototype = NULL;
    global_object->special_function_arguments = NULL;
    global_object->special_function_caller = NULL;
    global_object->special_function_throw_type_error = NULL;
    global_object->default_function_properties = NULL;
    global_object->default_builtin_function_properties = NULL;
    global_object->default_strict_function_properties = NULL;
    global_object->throw_type_error = NULL;
}

/* static */ void
ES_Global_Object::MakeNativeErrorObject(ES_Context *context, ES_Class *constructor_class, ES_Global_Object *global_object, int id)
{
    ClassIndex class_idx = CI_ERROR;
    PrototypeIndex prototype_idx = PI_ERROR;
    ES_Function::BuiltIn *constructor = ES_ErrorBuiltins::constructor;
    unsigned name = ESID_Error;

    switch (id)
    {
    case ESID_Error:
        class_idx = CI_ERROR;
        prototype_idx = PI_ERROR;
        constructor = ES_ErrorBuiltins::constructor;
        name = ESID_Error;
        break;
    case ESID_EvalError:
        class_idx = CI_EVAL_ERROR;
        prototype_idx = PI_EVAL_ERROR;
        constructor = ES_EvalErrorBuiltins::constructor;
        name = ESID_EvalError;
        break;
    case ESID_RangeError:
        class_idx = CI_RANGE_ERROR;
        prototype_idx = PI_RANGE_ERROR;
        constructor = ES_RangeErrorBuiltins::constructor;
        name = ESID_RangeError;
        break;
    case ESID_ReferenceError:
        class_idx = CI_REFERENCE_ERROR;
        prototype_idx = PI_REFERENCE_ERROR;
        constructor = ES_ReferenceErrorBuiltins::constructor;
        name = ESID_ReferenceError;
        break;
    case ESID_SyntaxError:
        class_idx = CI_SYNTAX_ERROR;
        prototype_idx = PI_SYNTAX_ERROR;
        constructor = ES_SyntaxErrorBuiltins::constructor;
        name = ESID_SyntaxError;
        break;
    case ESID_TypeError:
        class_idx = CI_TYPE_ERROR;
        prototype_idx = PI_TYPE_ERROR;
        constructor = ES_TypeErrorBuiltins::constructor;
        name = ESID_TypeError;
        break;
    case ESID_URIError:
        class_idx = CI_URI_ERROR;
        prototype_idx = PI_URI_ERROR;
        constructor = ES_URIErrorBuiltins::constructor;
        name = ESID_URIError;
        break;
    default:
        OP_ASSERT(!"This should not happen");
    }

    OP_ASSERT(class_idx < CI_MAXIMUM && prototype_idx < PI_MAXIMUM);

    JString **idents = context->rt_data->idents;

    if (id == ESID_Error)
        global_object->prototypes[prototype_idx] = ES_Error::MakePrototypeObject(context, global_object, global_object->classes[class_idx]);
    else
        global_object->prototypes[prototype_idx] = ES_Error::MakePrototypeObject(context, global_object, global_object->classes[class_idx], id);

    ES_Object *error_constructor = ES_Function::Make(context, constructor_class, global_object, 1, constructor, constructor, name, NULL, global_object->prototypes[prototype_idx]);

    global_object->InitPropertyL(context, idents[id], error_constructor, DE);
    global_object->prototypes[prototype_idx]->InitPropertyL(context, idents[ESID_constructor], error_constructor, DE);
}

ES_Object *
ES_Global_Object::GetNativeErrorPrototype(ES_NativeError type)
{
    ES_Object *prototype = NULL;
    switch (type)
    {
    default:
        OP_ASSERT(!"Incomplete handling of error types; cannot happen.");
    case ES_Native_Error:
        prototype = prototypes[PI_ERROR];
        break;
    case ES_Native_RangeError:
        prototype = prototypes[PI_RANGE_ERROR];
        break;
    case ES_Native_ReferenceError:
        prototype = prototypes[PI_REFERENCE_ERROR];
        break;
    case ES_Native_SyntaxError:
        prototype = prototypes[PI_SYNTAX_ERROR];
        break;
    case ES_Native_TypeError:
        prototype = prototypes[PI_TYPE_ERROR];
        break;
    }
    return prototype;
}

void
ES_Global_Object::PrepareForOptimizeGlobalAccessors(ES_Context *context, unsigned variable_count)
{
    ES_Property_Table *property_table = klass->GetPropertyTable();
    variable_count += property_table->Count();
    if (property_table->Capacity() < variable_count)
        property_table->GrowToL(context, static_cast<unsigned int>(static_cast<double>(variable_count) * 1.1));
}

void
ES_Global_Object::AddVariable(ES_Context *context, JString *name, BOOL is_function, BOOL is_configurable)
{
    unsigned index;
    if (is_configurable)
        InitPropertyL(context, name, ES_Value_Internal(), 0, ES_STORAGE_WHATEVER);
    else if (variables->AppendL(context, name, index))
    {
        if ((index & (index - 1)) == 0 && index >= 8)
        {
            unsigned capacity = index * 2;

            ES_Value_Internal *new_variable_values = static_cast<ES_Value_Internal *>(op_malloc(sizeof(ES_Value_Internal) * capacity + sizeof(BOOL) * capacity));
            if (!new_variable_values)
            {
                variables->RemoveLast(index);
                context->AbortOutOfMemory();
            }

            BOOL *new_variable_is_function = reinterpret_cast<BOOL *>(new_variable_values + capacity);

            op_memcpy(new_variable_values, variable_values, index * sizeof(ES_Value_Internal));
            op_memcpy(new_variable_is_function, variable_is_function, index * sizeof(BOOL));

            op_free(variable_values);

            variable_values = new_variable_values;
            variable_is_function = new_variable_is_function;
        }

        variable_values[index].SetUndefined();
        variable_is_function[index] = is_function;

        ES_Value_Internal indir;
        indir.SetBoxed(ES_Special_Global_Variable::Make(context, index));
        ES_CollectorLock gclock(context);
        InitPropertyL(context, name, indir, SP | DD | (is_function ? FN : 0), ES_STORAGE_BOXED, TRUE);
    }
}

static BOOL
IsBuiltinFunction(ES_Object::ES_Value_Internal_Ref &value_ref, ES_Function::BuiltIn *call)
{
    return value_ref.IsObject() && value_ref.GetObject()->IsFunctionObject() && !value_ref.GetObject()->IsHostObject() && static_cast<ES_Function *>(value_ref.GetObject())->GetCall() == call;
}

void
ES_Global_Object::UpdatePrototypeClassCaches()
{
    ES_Allocation_Context context(GetRuntime());

    cached_string_prototype_class_id = prototypes[PI_STRING]->Class()->GetId(&context);
    cached_number_prototype_class_id = prototypes[PI_NUMBER]->Class()->GetId(&context);
    cached_date_prototype_class_id = prototypes[PI_DATE]->Class()->GetId(&context);
    cached_boolean_prototype_class_id = prototypes[PI_BOOLEAN]->Class()->GetId(&context);

    ES_Property_Info info;

    ES_Value_Internal_Ref value_ref;

    simple_string_objects = (prototypes[PI_STRING]->GetOwnLocation(rt_data->idents[ESID_toString], info, value_ref) && IsBuiltinFunction(value_ref, ES_StringBuiltins::toString) &&
                             prototypes[PI_STRING]->GetOwnLocation(rt_data->idents[ESID_valueOf], info, value_ref) && IsBuiltinFunction(value_ref, ES_StringBuiltins::valueOf));

    simple_number_objects = (prototypes[PI_NUMBER]->GetOwnLocation(rt_data->idents[ESID_toString], info, value_ref) && IsBuiltinFunction(value_ref, ES_NumberBuiltins::toString) &&
                             prototypes[PI_NUMBER]->GetOwnLocation(rt_data->idents[ESID_valueOf], info, value_ref) && IsBuiltinFunction(value_ref, ES_NumberBuiltins::valueOf));

    simple_date_objects = (prototypes[PI_DATE]->GetOwnLocation(rt_data->idents[ESID_toString], info, value_ref) && IsBuiltinFunction(value_ref, ES_DateBuiltins::toString) &&
                           prototypes[PI_DATE]->GetOwnLocation(rt_data->idents[ESID_valueOf], info, value_ref) && IsBuiltinFunction(value_ref, ES_DateBuiltins::valueOf));

    simple_boolean_objects = (prototypes[PI_BOOLEAN]->GetOwnLocation(rt_data->idents[ESID_toString], info, value_ref) && IsBuiltinFunction(value_ref, ES_BooleanBuiltins::toString) &&
                              prototypes[PI_BOOLEAN]->GetOwnLocation(rt_data->idents[ESID_valueOf], info, value_ref) && IsBuiltinFunction(value_ref, ES_BooleanBuiltins::valueOf));
}

#ifdef ES_NATIVE_SUPPORT

void
ES_Global_Object::InvalidateInlineFunction(ES_Context *context, unsigned index)
{
    if (variable_is_function[index])
    {
        const ES_Value_Internal &value = GetVariableValue(index);

        if (!value.IsUndefined())
        {
            variable_is_function[index] = FALSE;

            ES_Property_Info info;
            klass->Find(GetVariableName(index), info, Count());

            if (info.IsFunction() && value.IsObject() && value.GetObject()->HasBeenInlined())
                /* This is what actually invalidates the inlined function. */
                immediate_function_serial_nr++;

            info.ClearIsFunction();
            BOOL dummy_needs_conversion = TRUE;
            klass = ES_Class::ChangeAttribute(context, klass, GetVariableName(index), info, Count(), dummy_needs_conversion);
        }
    }
}

#endif // ES_NATIVE_SUPPORT

ES_RegExp_Object *
ES_Global_Object::GetDynamicRegExp(ES_Execution_Context *context, JString *source, RegExpFlags &flags, unsigned flagbits)
{
    ES_Property_Info info;
    ES_Value_Internal *value;
    ES_Property_Value_Table **cache;

    if (flagbits == 0)
        cache = &dynamic_regexp_cache;
    else if (flagbits == REGEXP_FLAG_GLOBAL)
        cache = &dynamic_regexp_cache_g;
    else if (flagbits == REGEXP_FLAG_IGNORECASE)
        cache = &dynamic_regexp_cache_i;
    else if (flagbits == (REGEXP_FLAG_GLOBAL | REGEXP_FLAG_IGNORECASE))
        cache = &dynamic_regexp_cache_gi;
    else
        cache = NULL;

    if (cache && *cache && (*cache)->FindLocation(source, info, value))
    {
        /* We need to lock the GC so that 'object' doesn't get GC:ed while we
           allocate the clone. */
        ES_CollectorLock gclock(context);

        ES_RegExp_Object *object = static_cast<ES_RegExp_Object *>(value->GetObject());
        ES_RegExp_Information info;

        info.regexp = object->GetValue();
        info.source = UINT_MAX;
        info.flags = flagbits;

        return ES_RegExp_Object::Make(context, this, info, object->GetSource());
    }
    else
    {
        ES_RegExp_Object *object = ES_RegExp_Object::Make(context, this, source, flags, flagbits, TRUE);

        if (object)
        {
            ES_CollectorLock gclock(context);

            if (cache)
            {
                if (!*cache)
                    *cache = ES_Property_Value_Table::Make(context, 4);

                ES_Value_Internal value(object);

                (*cache)->InsertL(context, *cache, source, value, info, /*serial nr*/0);
            }
        }

        return object;
    }
}
