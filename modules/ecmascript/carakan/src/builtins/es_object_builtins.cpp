/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_object_builtins.h"

static BOOL
InterpretPropertyName(ES_Execution_Context *context, JString *&name, unsigned &index, ES_Value_Internal *argv, unsigned argc, unsigned argi)
{
    if (argi < argc)
    {
        ES_Value_Internal &value = argv[argi];

        if (value.IsUInt32())
        {
            name = NULL;
            index = value.GetNumAsUInt32();
            if (index == UINT_MAX)
            {
                value.ToString(context);
                name = value.GetString();
            }
        }
        else
        {
            if (!value.ToString(context))
                return FALSE;
            name = value.GetString();
            if (convertindex(Storage(context, name), Length(name), index))
                name = NULL;
        }
    }
    else
        name = context->rt_data->strings[STRING_undefined];
    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc == 0 || argv[0].IsNull() || argv[0].IsUndefined())
        return_value->SetObject(ES_Object::Make(context, ES_GET_GLOBAL_OBJECT()->GetObjectClass()));
    else
    {
        argv[0].ToObject(context);
        return_value->SetObject(argv[0].GetObject());
    }

    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    const ES_Value_Internal &this_value = argv[-2];
    JString *the_class;
    JString **idents = context->rt_data->idents;

    switch (this_value.Type())
    {
    case ESTYPE_OBJECT:
    {
        the_class = this_value.GetObject(context)->Class()->ObjectName(context);

        /* Libraries (such as jQuery) do portable type tests via toString(); if you do that often, it adds up. */
        if (the_class == idents[ESID_Array])
        {
            return_value->SetString(idents[ESID_object_Array]);
            return TRUE;
        }
        else if (the_class == idents[ESID_Object])
        {
            return_value->SetString(idents[ESID_object_Object]);
            return TRUE;
        }
        else if (the_class == idents[ESID_Function])
        {
            return_value->SetString(idents[ESID_object_Function]);
            return TRUE;
        }
		/* borderline... have 'Window' as an ESID. */
        else if (the_class == idents[ESID_Window])
        {
            return_value->SetString(idents[ESID_object_Window]);
            return TRUE;
        }
        break;
    }

    case ESTYPE_STRING:
        return_value->SetString(idents[ESID_object_String]);
        return TRUE;

    case ESTYPE_INT32:
    case ESTYPE_DOUBLE:
        return_value->SetString(idents[ESID_object_Number]);
        return TRUE;

    case ESTYPE_NULL:
        return_value->SetString(idents[ESID_object_Null]);
        return TRUE;

    case ESTYPE_UNDEFINED:
        return_value->SetString(idents[ESID_object_Undefined]);
        return TRUE;

    default:
        OP_ASSERT(this_value.Type() == ESTYPE_BOOLEAN);
        return_value->SetString(idents[ESID_object_Boolean]);
        return TRUE;
    }

    unsigned len_class = Length(the_class);
    unsigned len = 9 + len_class;

    JString *string = JString::Make(context, len);
    uni_char *storage = Storage(context, string);

    uni_strcpy(storage, UNI_L("[object "));
    op_memcpy(storage + 8, Storage(context, the_class), UNICODE_SIZE(len_class));
    storage[len-1] = ']';

    return_value->SetString(string);
    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (!argv[-2].ToObject(context))
        return FALSE;

    const ES_Value_Internal &this_value = argv[-2];
    ES_Object *this_object = this_value.GetObject(context);

    ES_Value_Internal toString_value;

    if (this_object->GetL(context, context->rt_data->idents[ESID_toString], toString_value) == PROP_GET_FAILED)
        return FALSE;

    if (toString_value.IsObject() && toString_value.GetObject(context)->IsFunctionObject())
    {
        ES_Value_Internal *registers = context->SetupFunctionCall(toString_value.GetObject(context), 0);

        registers[0] = argv[-2];
        registers[1] = toString_value;

        if (!context->CallFunction(registers, 0, return_value))
            return FALSE;
    }
    else
    {
        context->ThrowTypeError("Object.prototype.toLocaleString: object's toString is not callable");
        return FALSE;
    }

    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (!argv[-2].ToObject(context))
        return FALSE;

    *return_value = argv[-2];
    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::hasOwnProperty(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *name;
    unsigned index;

    if (!InterpretPropertyName(context, name, index, argv, argc, 0))
        return FALSE;

    unsigned class_id = ES_Class::NOT_CACHED_CLASS_ID;

    if (name)
    {
        unsigned cached_offset, cached_type;
        context->IsEnumeratedPropertyName(name, class_id, cached_offset, cached_type);
    }

    const ES_Value_Internal &this_value = argv[-2];

    if (!this_value.SimulateToObject(context))
        return FALSE;

    BOOL result = FALSE;

    if (this_value.IsObject())
    {
        ES_Object *object = this_value.GetObject(context);

        if (name)
            if (class_id == object->Class()->Id())
                result = TRUE;
            else
                result = object->HasOwnProperty(context, name);
        else
            result = object->HasOwnProperty(context, index);
    }
    else if (this_value.IsString()) // magic "123".hasOwnProperty(i) for i = 0, 1, 2;
        if (name)
            result = name->Equals(UNI_L("length"), 6);
        else
            result = index < Length(this_value.GetString());

    return_value->SetBoolean(result);
    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::isPrototypeOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    BOOL result = FALSE;

    if (argc >= 1 && argv[0].IsObject())
    {
        const ES_Value_Internal &this_value = argv[-2];

        if (!this_value.SimulateToObject(context))
            return FALSE;

        if (this_value.IsObject())
        {
            ES_Object *this_object = this_value.GetObject(context);
            ES_Object *prototype = argv[0].GetObject(context)->Class()->Prototype();

            while (prototype)
                if (prototype == this_object)
                {
                    result = TRUE;
                    break;
                }
                else
                    prototype = prototype->Class()->Prototype();
        }
    }

    return_value->SetBoolean(result);
    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::propertyIsEnumerable(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *name;
    unsigned index;

    if (!InterpretPropertyName(context, name, index, argv, argc, 0))
        return FALSE;

    const ES_Value_Internal &this_value = argv[-2];

    if (!this_value.SimulateToObject(context))
        return FALSE;

    ES_Object::PropertyDescriptor desc;
    BOOL result;

    if (this_value.IsObject())
    {
        if (!this_value.GetObject(context)->GetOwnPropertyL(context, desc, name, index, FALSE, FALSE))
            return FALSE;

        result = !desc.is_undefined && desc.info.IsEnumerable();
    }
    else
        result = FALSE;

    return_value->SetBoolean(result);
    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::defineGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *name;
    unsigned index;

    if (!InterpretPropertyName(context, name, index, argv, argc, 0))
        return FALSE;

    ES_Value_Internal thisArg = argv[-2];

    if (!thisArg.ToObject(context))
        return FALSE;

    if (argc < 2 || !argv[1].IsCallable(context))
    {
        context->ThrowTypeError("__defineGetter__: getter argument is not callable");
        return FALSE;
    }

    ES_Object::PropertyDescriptor desc;

    desc.SetEnumerable(TRUE);
    desc.SetConfigurable(TRUE);
    desc.SetGetter(argv[1].GetObject(context));

    const char *message;

    if (thisArg.GetObject(context)->DefineOwnPropertyL(context, name, index, desc, message))
    {
        return_value->SetUndefined();
        return TRUE;
    }
    else if (message)
    {
        char buffer[128]; // ARRAY OK 2011-07-29 jl

        op_strlcpy(buffer, "__defineGetter__: ", 128);
        op_strlcat(buffer, message, 128);

        context->ThrowTypeError(buffer);
        return FALSE;
    }
    else
        return FALSE;
}

/* static */ BOOL
ES_ObjectBuiltins::defineSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *name;
    unsigned index;

    if (!InterpretPropertyName(context, name, index, argv, argc, 0))
        return FALSE;

    ES_Value_Internal thisArg = argv[-2];

    if (!thisArg.ToObject(context))
        return FALSE;

    if (argc < 2 || !argv[1].IsCallable(context))
    {
        context->ThrowTypeError("__defineSetter__: setter argument is not callable");
        return FALSE;
    }

    ES_Object::PropertyDescriptor desc;

    desc.SetEnumerable(TRUE);
    desc.SetConfigurable(TRUE);
    desc.SetSetter(argv[1].GetObject(context));

    const char *message;

    if (thisArg.GetObject(context)->DefineOwnPropertyL(context, name, index, desc, message))
    {
        return_value->SetUndefined();
        return TRUE;
    }
    else if (message)
    {
        char buffer[128]; // ARRAY OK 2011-07-29 jl

        op_strlcpy(buffer, "__defineSetter__: ", 128);
        op_strlcat(buffer, message, 128);

        context->ThrowTypeError(buffer);
        return FALSE;
    }
    else
        return FALSE;
}

/* static */ BOOL
ES_ObjectBuiltins::lookupGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *name;
    unsigned index;

    if (!InterpretPropertyName(context, name, index, argv, argc, 0))
        return FALSE;

    ES_Value_Internal thisArg = argv[-2];

    if (!thisArg.ToObject(context))
        return FALSE;

    ES_Object::PropertyDescriptor desc;

    if (!thisArg.GetObject(context)->GetPropertyL(context, desc, name, index, FALSE, TRUE))
        return FALSE;

    if (desc.has_getter)
        return_value->SetObject(desc.getter);
    else
        return_value->SetUndefined();

    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::lookupSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    JString *name;
    unsigned index;

    if (!InterpretPropertyName(context, name, index, argv, argc, 0))
        return FALSE;

    ES_Value_Internal thisArg = argv[-2];

    if (!thisArg.ToObject(context))
        return FALSE;

    ES_Object::PropertyDescriptor desc;

    if (!thisArg.GetObject(context)->GetPropertyL(context, desc, name, index, FALSE, TRUE))
        return FALSE;

    if (desc.has_setter)
        return_value->SetObject(desc.setter);
    else
        return_value->SetUndefined();

    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::protoGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Value_Internal thisArg = argv[-2];

    if (!thisArg.ToObject(context))
        return FALSE;

    ES_Object *object = thisArg.GetObject(context);

    if (!object->SecurityCheck(context))
        return_value->SetUndefined();
    else
    {
        GetResult result = object->GetPrototypeL(context, *return_value);
        if (result == PROP_GET_FAILED)
            return FALSE;
    }

    return TRUE;
}

/* static */ BOOL
ES_ObjectBuiltins::protoSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Value_Internal thisArg = argv[-2];
    ES_Value_Internal parent;

    if (argc >= 1)
        parent = argv[0];

    if (!thisArg.ToObject(context))
        return FALSE;

    ES_CollectorLock gclock(context);

    PutResult result = thisArg.GetObject(context)->PutPrototypeL(context, parent);

    return result != PROP_PUT_FAILED;
}

/* static */ void
ES_ObjectBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;
    ES_Value_Internal protoAccessor = ES_Special_Mutable_Access::Make(context, MAKE_BUILTIN_ANONYMOUS(0, protoGetter), MAKE_BUILTIN_ANONYMOUS(1, protoSetter));

    ASSERT_CLASS_SIZE(ES_ObjectBuiltins);

    APPEND_PROPERTY(ES_ObjectBuiltins, constructor,             undefined);
    APPEND_PROPERTY(ES_ObjectBuiltins, toString,                MAKE_BUILTIN(0, toString));
    APPEND_PROPERTY(ES_ObjectBuiltins, toLocaleString,          MAKE_BUILTIN(0, toLocaleString));
    APPEND_PROPERTY(ES_ObjectBuiltins, valueOf,                 MAKE_BUILTIN(0, valueOf));
    APPEND_PROPERTY(ES_ObjectBuiltins, hasOwnProperty,          MAKE_BUILTIN(1, hasOwnProperty));
    APPEND_PROPERTY(ES_ObjectBuiltins, isPrototypeOf,           MAKE_BUILTIN(1, isPrototypeOf));
    APPEND_PROPERTY(ES_ObjectBuiltins, propertyIsEnumerable,    MAKE_BUILTIN(1, propertyIsEnumerable));
    APPEND_PROPERTY(ES_ObjectBuiltins, defineGetter,            MAKE_BUILTIN(2, defineGetter));
    APPEND_PROPERTY(ES_ObjectBuiltins, defineSetter,            MAKE_BUILTIN(2, defineSetter));
    APPEND_PROPERTY(ES_ObjectBuiltins, lookupGetter,            MAKE_BUILTIN(1, lookupGetter));
    APPEND_PROPERTY(ES_ObjectBuiltins, lookupSetter,            MAKE_BUILTIN(1, lookupSetter));
    APPEND_PROPERTY(ES_ObjectBuiltins, proto,                   protoAccessor);

    ASSERT_OBJECT_COUNT(ES_ObjectBuiltins);
}

/* static */ void
ES_ObjectBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_ObjectBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_ObjectBuiltins, constructor,            DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_ObjectBuiltins, toString,               DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, toLocaleString,         DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, valueOf,                DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, hasOwnProperty,         DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, isPrototypeOf,          DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, propertyIsEnumerable,   DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, defineGetter,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, defineSetter,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, lookupGetter,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, lookupSetter,           DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ObjectBuiltins, proto,                  DE|SP, ES_STORAGE_BOXED);
}

/* static */ void
ES_ObjectConstructorBuiltins::PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor)
{
    JString **idents = context->rt_data->idents;

    constructor->InitPropertyL(context, idents[ESID_getPrototypeOf],           MAKE_BUILTIN(1, getPrototypeOf), DE);
    constructor->InitPropertyL(context, idents[ESID_getOwnPropertyDescriptor], MAKE_BUILTIN(2, getOwnPropertyDescriptor), DE);
    constructor->InitPropertyL(context, idents[ESID_getOwnPropertyNames],      MAKE_BUILTIN(1, getOwnPropertyNames), DE);
    constructor->InitPropertyL(context, idents[ESID_create],                   MAKE_BUILTIN(2, create), DE);
    constructor->InitPropertyL(context, idents[ESID_defineProperty],           MAKE_BUILTIN(3, defineProperty), DE);
    constructor->InitPropertyL(context, idents[ESID_defineProperties],         MAKE_BUILTIN(2, defineProperties), DE);
    constructor->InitPropertyL(context, idents[ESID_seal],                     MAKE_BUILTIN(1, seal), DE);
    constructor->InitPropertyL(context, idents[ESID_freeze],                   MAKE_BUILTIN(1, freeze), DE);
    constructor->InitPropertyL(context, idents[ESID_preventExtensions],        MAKE_BUILTIN(1, preventExtensions), DE);
    constructor->InitPropertyL(context, idents[ESID_isSealed],                 MAKE_BUILTIN(1, isSealed), DE);
    constructor->InitPropertyL(context, idents[ESID_isFrozen],                 MAKE_BUILTIN(1, isFrozen), DE);
    constructor->InitPropertyL(context, idents[ESID_isExtensible],             MAKE_BUILTIN(1, isExtensible), DE);
    constructor->InitPropertyL(context, idents[ESID_keys],                     MAKE_BUILTIN(1, keys), DE);
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::getPrototypeOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.2: Object.getPrototypeOf(O) */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.getPrototypeOf: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (object->SecurityCheck(context))
    {
        GetResult result = object->GetPrototypeL(context, *return_value);
        if (result == PROP_GET_FAILED)
            return FALSE;
    }
    else
        return_value->SetUndefined();

    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::getOwnPropertyDescriptor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.3: Object.getOwnPropertyDescriptor(O) */
    ES_Value_Internal O, P;

    if (argc >= 1)
        O = argv[0];
    if (argc >= 2)
        P = argv[1];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.getOwnPropertyDescriptor: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    JString *name = NULL;
    unsigned index = 0;

    if (P.IsUInt32())
        index = P.GetNumAsUInt32();
    else
    {
        if (!P.ToString(context))
            return FALSE;
        name = P.GetString();
        if (convertindex(Storage(context, name), Length(name), index))
            name = NULL;
    }

    ES_Object::PropertyDescriptor desc;

    if (!object->GetOwnPropertyL(context, desc, name, index, TRUE, TRUE))
        return FALSE;

    if (desc.is_undefined)
    {
        return_value->SetUndefined();
        return TRUE;
    }

    JString **idents = context->rt_data->idents;
    ES_Object *info_object = ES_Object::Make(context, context->GetGlobalObject()->GetObjectClass(), 0);
    ES_CollectorLock gclock(context);

    if (desc.has_value)
    {
        info_object->InitPropertyL(context, idents[ESID_value], desc.value);
        info_object->InitPropertyL(context, idents[ESID_writable], ES_Value_Internal::Boolean(!desc.info.IsReadOnly()));
    }
    else
    {
        ES_Value_Internal setter;
        unsigned setter_attributes;
        if (desc.setter && desc.setter->IsBuiltInFunction() && static_cast<ES_Function*>(desc.setter)->GetCall() == &ES_ObjectBuiltins::protoSetter)
        {
            setter = context->GetGlobalObject()->GetThrowTypeErrorProperty();
            setter_attributes = SP;
        }
        else
        {
            setter = ES_Value_Internal::Object(desc.setter, ESTYPE_UNDEFINED);
            setter_attributes = 0;
        }

        info_object->InitPropertyL(context, idents[ESID_get], ES_Value_Internal::Object(desc.getter, ESTYPE_UNDEFINED));
        info_object->InitPropertyL(context, idents[ESID_set], setter, setter_attributes);
    }

    info_object->InitPropertyL(context, idents[ESID_enumerable], ES_Value_Internal::Boolean(!desc.info.IsDontEnum()));
    info_object->InitPropertyL(context, idents[ESID_configurable], ES_Value_Internal::Boolean(!desc.info.IsDontDelete()));

    return_value->SetObject(info_object);
    return TRUE;
}

static BOOL
GetOwnPropertyNames(ES_Execution_Context *context, ES_Object *&result, ES_Object *object, BOOL only_enumerable)
{
    if (!object->SecurityCheck(context, "Security error: attempted to enumerate protected object"))
        return FALSE;

    ES_Indexed_Properties *names = object->GetOwnPropertyNamesSafeL(context, NULL, !only_enumerable);
    ES_CollectorLock gclock(context);

    result = ES_Array::Make(context, context->GetGlobalObject());

    ES_Indexed_Property_Iterator iterator(context, NULL, names);
    unsigned index = 0;
    unsigned result_index = 0;

    ES_Value_Internal it_value;
    while (iterator.Next(index))
    {
        iterator.GetValue(it_value);
        it_value.ToString(context);
        result->PutL(context, result_index++, it_value);
    }

    ES_Array::SetLength(context, result, result_index);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::getOwnPropertyNames(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.4: Object.getOwnPropertyNames(O) */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.getOwnPropertyNames: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    ES_Object *result;

    if (!GetOwnPropertyNames(context, result, object, FALSE))
        return FALSE;

    return_value->SetObject(result);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::create(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.5: Object.create(O [,Properties]) */
    ES_Value_Internal O, Properties;

    if (argc >= 1)
        O = argv[0];
    if (argc >= 2)
        Properties = argv[1];

    ES_Object *object;

    if (O.IsNull())
        object = NULL;
    else if (!O.IsObject())
    {
        context->ThrowTypeError("Object.create: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    ES_Class *klass = object ? object->SetSubObjectClass(context) : ES_GET_GLOBAL_OBJECT()->GetEmptyClass();
    ES_CollectorLock gclock(context); // FIXME: We can't lock here.

    ES_Object *result = ES_Object::Make(context, klass, 0);

    if (!Properties.IsUndefined())
    {
        if (!Properties.ToObject(context))
        {
            context->ThrowTypeError("Object.create: second argument not an Object");
            return FALSE;
        }

        ES_Value_Internal arguments[2];
        arguments[0].SetObject(result);
        arguments[1] = Properties;

        if (!ES_ObjectConstructorBuiltins::defineProperties(context, 2, arguments, return_value))
            return FALSE;
    }

    return_value->SetObject(result);
    return TRUE;
}

static BOOL
ToPropertyDescriptor(ES_Execution_Context *context, ES_Object::PropertyDescriptor &desc, ES_Object *object)
{
    JString **idents = context->rt_data->idents;
    ES_Value_Internal value;
    GetResult result;

    result = object->GetL(context, idents[ESID_enumerable], value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (GET_OK(result))
        desc.SetEnumerable(value.AsBoolean().GetBoolean());
    else
        desc.info.SetEnumerable(FALSE);

    result = object->GetL(context, idents[ESID_configurable], value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (GET_OK(result))
        desc.SetConfigurable(value.AsBoolean().GetBoolean());
    else
        desc.info.SetConfigurable(FALSE);

    result = object->GetL(context, idents[ESID_value], desc.value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (GET_OK(result))
        desc.has_value = TRUE;

    result = object->GetL(context, idents[ESID_writable], value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (GET_OK(result))
        desc.SetWritable(value.AsBoolean().GetBoolean());
    else
        desc.info.SetWritable(FALSE);

    result = object->GetL(context, idents[ESID_get], value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (GET_OK(result))
    {
        if (!value.IsUndefined())
        {
            if (!value.IsCallable(context))
            {
                context->ThrowTypeError("ToPropertyDescriptor: value of 'get' is not callable");
                return FALSE;
            }
            desc.getter = value.GetObject(context);
        }
        desc.has_getter = 1;
    }

    result = object->GetL(context, idents[ESID_set], value);
    if (result == PROP_GET_FAILED)
        return FALSE;
    if (GET_OK(result))
    {
        if (!value.IsUndefined())
        {
            if (!value.IsCallable(context))
            {
                context->ThrowTypeError("ToPropertyDescriptor: value of 'set' is not callable");
                return FALSE;
            }
            desc.setter = value.GetObject(context);
            desc.info.SetWritable(TRUE);
        }
        desc.has_setter = 1;
    }

    if (ES_Object::IsDataDescriptor(desc) && ES_Object::IsAccessorDescriptor(desc))
    {
        context->ThrowTypeError("ToPropertyDescriptor: both [[Value]]/[[Writable]] and [[Get]]/[[Set]] present");
        return FALSE;
    }

    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::defineProperty(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.6: Object.defineProperty(O, Name, Properties) */
    ES_Value_Internal O, Attributes;

    if (argc >= 1)
        O = argv[0];
    if (argc >= 3)
        Attributes = argv[2];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.defineProperty: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    JString *name = NULL;
    unsigned index = 0;

    if (!InterpretPropertyName(context, name, index, argv, argc, 1))
        return FALSE;

    ES_Object::PropertyDescriptor desc;

    if (!Attributes.IsObject())
    {
        context->ThrowTypeError("Object.defineProperty: third argument not an Object");
        return FALSE;
    }
    else if (!ToPropertyDescriptor(context, desc, Attributes.GetObject(context)))
        return FALSE;

    const char *message;

    if (object->DefineOwnPropertyL(context, name, index, desc, message))
    {
        return_value->SetObject(object);
        return TRUE;
    }
    else if (message)
    {
        char buffer[128]; // ARRAY OK 2011-07-29 jl

        op_strlcpy(buffer, "Object.defineProperty: ", 128);
        op_strlcat(buffer, message, 128);

        context->ThrowTypeError(buffer);
        return FALSE;
    }
    else
        return FALSE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::defineProperties(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.7: Object.defineProperties(O, Properties) */
    ES_Value_Internal O, Properties;

    if (argc >= 1)
        O = argv[0];
    if (argc >= 2)
        Properties = argv[1];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.defineProperties: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    ES_Object *properties;

    if (!Properties.ToObject(context))
        return FALSE;

    properties = Properties.GetObject(context);

    ES_Indexed_Properties *names = properties->GetOwnPropertyNamesSafeL(context, NULL, FALSE);
    ES_Indexed_Property_Iterator iterator(context, NULL, names);

    unsigned count = ES_Indexed_Properties::GetUsed(names);

    if (count)
    {
        AutoRegisters values(context, count * 2 + 2);

        values[count * 2].SetBoxed(names);

        ES_Box *descriptors_box = ES_Box::Make(context, count * sizeof(ES_Object::PropertyDescriptor));
        ES_Object::PropertyDescriptor *descriptors = reinterpret_cast<ES_Object::PropertyDescriptor *>(descriptors_box->Unbox());

        values[count * 2 + 1].SetBoxed(descriptors_box);

        unsigned property_index = 0;
        unsigned desc_index = 0;
        while (iterator.Next(property_index))
        {
            ES_Value_Internal value;

            iterator.GetValue(value);

            JString *name = NULL;
            unsigned index = 0;
            GetResult result;

            if (value.IsUInt32())
                result = properties->GetL(context, index = value.GetNumAsUInt32(), value);
            else
                result = properties->GetL(context, name = value.GetString(), value);

            if (result == PROP_GET_FAILED)
                return FALSE;

            ES_Object::PropertyDescriptor &desc = descriptors[desc_index];

            desc.Reset();

            if (!value.IsObject())
            {
                context->ThrowTypeError("Object.defineProperties: property descriptor is not object");
                return FALSE;
            }
            else if (!ToPropertyDescriptor(context, desc, value.GetObject(context)))
                return FALSE;

            if (desc.has_value)
                values[desc_index * 2] = desc.value;
            else
            {
                if (desc.has_getter)
                    values[desc_index * 2].SetObject(desc.getter);
                if (desc.has_setter)
                    values[desc_index * 2 + 1].SetObject(desc.setter);
            }

            ++desc_index;
        }

        iterator.Reset();
        desc_index = 0;

        while (iterator.Next(property_index))
        {
            ES_Value_Internal value;

            iterator.GetValue(value);

            JString *name = NULL;
            unsigned index = 0;

            if (value.IsUInt32())
                index = value.GetNumAsUInt32();
            else
                name = value.GetString();

            const char *message;

            if (!object->DefineOwnPropertyL(context, name, index, descriptors[desc_index++], message))
            {
                if (message)
                {
                    char buffer[128]; // ARRAY OK 2011-07-29 jl

                    op_strlcpy(buffer, "Object.defineProperties: ", 128);
                    op_strlcat(buffer, message, 128);

                    context->ThrowTypeError(buffer);
                }
                return FALSE;
            }
        }

        values[0].SetNull();
        context->heap->Free(descriptors_box);
    }

    return_value->SetObject(object);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::seal(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.8: set [[Configurable]] of argument O's properties to 'false'.
                     Set O's [[Extensible]] to 'false'; return O. */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.seal: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (!object->SecurityCheck(context, "Security error: attempted to modify protected object"))
        return FALSE;

    ES_Value_Internal *registers = context->AllocateRegisters(1);

    ES_Indexed_Properties *names = object->GetOwnPropertyNamesSafeL(context, NULL, TRUE);
    ES_Indexed_Property_Iterator iterator(context, NULL, names);

    registers[0].SetBoxed(names);

    unsigned property_index = UINT_MAX;
    while (iterator.Previous(property_index))
    {
        ES_Value_Internal value;

        iterator.GetValue(value);

        JString *name = NULL;
        unsigned index = 0;

        if (value.IsUInt32())
            index = value.GetNumAsUInt32();
        else
            name = value.GetString();

        ES_Object::PropertyDescriptor desc;
        if (object->GetOwnPropertyL(context, desc, name, index, TRUE, TRUE))
        {
            if (desc.info.IsConfigurable())
                desc.info.SetConfigurable(FALSE);

            const char *message;
            if (!object->DefineOwnPropertyL(context, name, index, desc, message))
            {
                context->FreeRegisters(1);

                if (message)
                {
                    char buffer[128]; // ARRAY OK 2011-09-13 sof

                    op_strlcpy(buffer, "Object.seal: ", 128);
                    op_strlcat(buffer, message, 128);

                    context->ThrowTypeError(buffer);
                    return FALSE;
                }
                else
                    return FALSE;
            }
        }
    }

    context->FreeRegisters(1);

    object->SetIsNotExtensible();

    return_value->SetObject(object);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::freeze(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.9: set [[Writable]]/[[Configurable]] of argument O's properties to 'false'.
                     Set O's [[Extensible]] to 'false'; return O. */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.freeze: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (!object->SecurityCheck(context, "Security error: attempted to modify protected object"))
        return FALSE;

    ES_Value_Internal *registers = context->AllocateRegisters(1);

    ES_Indexed_Properties *names = object->GetOwnPropertyNamesSafeL(context, NULL, TRUE);
    ES_Indexed_Property_Iterator iterator(context, NULL, names);

    registers[0].SetBoxed(names);

    unsigned property_index = UINT_MAX;
    while (iterator.Previous(property_index))
    {
        ES_Value_Internal value;

        iterator.GetValue(value);

        JString *name = NULL;
        unsigned index = 0;

        if (value.IsUInt32())
            index = value.GetNumAsUInt32();
        else
            name = value.GetString();

        ES_Object::PropertyDescriptor desc;
        if (object->GetOwnPropertyL(context, desc, name, index, TRUE, TRUE))
        {
            if (ES_Object::IsDataDescriptor(desc) && desc.info.IsWritable())
                desc.info.SetWritable(FALSE);
            desc.info.SetConfigurable(FALSE);

            const char *message;
            if (!object->DefineOwnPropertyL(context, name, index, desc, message))
            {
                context->FreeRegisters(1);

                if (message)
                {
                    char buffer[128]; // ARRAY OK 2011-09-13 sof

                    op_strlcpy(buffer, "Object.freeze: ", 128);
                    op_strlcat(buffer, message, 128);

                    context->ThrowTypeError(buffer);
                    return FALSE;
                }
                else
                    return FALSE;
            }
        }
    }

    context->FreeRegisters(1);

    object->SetIsNotExtensible();

    return_value->SetObject(object);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::preventExtensions(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.10: set [[Extensible]] of argument 'o' to 'false'; return 'o'. */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.preventExtensions: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (!object->SecurityCheck(context, "Security error: attempted to modify protected object"))
        return FALSE;

    object->SetIsNotExtensible();

    return_value->SetObject(object);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::isSealed(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.11:
       'false' iff any own properties are [[Configurable]], or object's [[Extensible]] is 'true'.
       'true' otherwise. */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.isSealed: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (!object->SecurityCheck(context, "Security error: attempted to inspect protected object"))
        return FALSE;

    BOOL is_sealed = TRUE;

    if (object->IsExtensible())
        is_sealed = FALSE;
    else
    {
        ES_Indexed_Properties *names = object->GetOwnPropertyNamesSafeL(context, NULL, TRUE);
        ES_Indexed_Property_Iterator iterator(context, NULL, names);

        ES_CollectorLock gclock(context);

        unsigned property_index = 0;
        while (iterator.Next(property_index))
        {
            ES_Value_Internal value;

            iterator.GetValue(value);

            JString *name = NULL;
            unsigned index = 0;

            if (value.IsUInt32())
                index = value.GetNumAsUInt32();
            else
                name = value.GetString();

            ES_Object::PropertyDescriptor desc;

            if (!object->GetOwnPropertyL(context, desc, name, index, FALSE, FALSE))
                return FALSE;

            if (desc.info.IsConfigurable())
            {
                is_sealed = FALSE;
                break;
            }
        }
    }

    return_value->SetBoolean(is_sealed);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::isFrozen(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.12:
       'false' iff any own properties are [[Writable]]/[[Configurable]], or object's [[Extensible]] is 'true'.
       'true' otherwise. */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.isFrozen: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (!object->SecurityCheck(context, "Security error: attempted to inspect protected object"))
        return FALSE;

    BOOL is_frozen = TRUE;

    if (object->IsExtensible())
        is_frozen = FALSE;
    else
    {
        ES_Indexed_Properties *names = object->GetOwnPropertyNamesSafeL(context, NULL, TRUE);
        ES_Indexed_Property_Iterator iterator(context, NULL, names);

        ES_CollectorLock gclock(context);

        unsigned property_index = 0;
        while (iterator.Next(property_index))
        {
            ES_Value_Internal value;

            iterator.GetValue(value);

            JString *name = NULL;
            unsigned index = 0;

            if (value.IsUInt32())
                index = value.GetNumAsUInt32();
            else
                name = value.GetString();

            ES_Object::PropertyDescriptor desc;

            if (!object->GetOwnPropertyL(context, desc, name, index, FALSE, FALSE))
                return FALSE;

            if (desc.info.IsConfigurable() || ES_Object::IsDataDescriptor(desc) && desc.info.IsWritable())
            {
                is_frozen = FALSE;
                break;
            }
        }
    }

    return_value->SetBoolean(is_frozen);
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::isExtensible(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.13: Return the Boolean value of the [[Extensible]] internal property of object argument. */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.isExtensible: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    if (!object->SecurityCheck(context, "Security error: attempted to inspect protected object"))
        return FALSE;

    return_value->SetBoolean(object->IsExtensible());
    return TRUE;
}

/* static */ BOOL
ES_ObjectConstructorBuiltins::keys(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    /* ES5 15.2.3.14: Object.keys(O) */
    ES_Value_Internal O;

    if (argc >= 1)
        O = argv[0];

    ES_Object *object;

    if (!O.IsObject())
    {
        context->ThrowTypeError("Object.keys: first argument not an Object");
        return FALSE;
    }
    else
        object = O.GetObject(context);

    ES_Object *result;

    if (!GetOwnPropertyNames(context, result, object, TRUE))
        return FALSE;

    return_value->SetObject(result);
    return TRUE;
}
