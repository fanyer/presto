/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/object/es_host_object.h"
#include "modules/ecmascript/carakan/src/vm/es_suspended_call.h"
#include "modules/ecmascript/carakan/src/builtins/es_array_builtins.h"

ES_Host_Object *
ES_Host_Object::Make(ES_Context *context, EcmaScript_Object* host_object, ES_Object* prototype, const char* object_class)
{
    OP_ASSERT(prototype);
#if 0
    OP_ASSERT(prototype->IsPrototypeObject());
#endif
    OP_ASSERT(!prototype->HasMultipleIdentities());

    ES_Host_Object *self;

    GC_ALLOCATE(context, self, ES_Host_Object, (self, TRUE));
    ES_CollectorLock gclock(context);

    self->klass = ES_Class::MakeRoot(context, prototype, object_class);
    self->AllocateProperties(context);

    /* FIXME: We would much rather want prototype to be a prototype to begin with. */
    if (!prototype->IsPrototypeObject())
        prototype->ConvertToPrototypeObject(context, self->klass);
    else
        prototype->AddInstance(context, self->klass, FALSE);

    self->host_object = host_object;
    return self;
}

/* static */ ES_Host_Object *
ES_Host_Object::Make(ES_Context *context, void *&payload, unsigned payload_size, ES_Object *prototype, const char *object_class, BOOL need_destroy, BOOL is_singleton)
{
    OP_ASSERT(prototype);
#if 0
    OP_ASSERT(prototype->IsPrototypeObject());
#endif
    OP_ASSERT(!prototype->HasMultipleIdentities());

    ES_Host_Object *self;

    GC_ALLOCATE_WITH_EXTRA_ALIGNED_PAYLOAD(context, self, payload, payload_size, ES_Host_Object, (self, need_destroy));
    ES_CollectorLock gclock(context);

    if (is_singleton)
        self->klass = ES_Class::MakeSingleton(context, prototype, object_class);
    else
        self->klass = ES_Class::MakeRoot(context, prototype, object_class, is_singleton);
    self->AllocateProperties(context);

    /* FIXME: We would much rather want prototype to be a prototype to begin with. */
    if (!prototype->IsPrototypeObject())
        prototype->ConvertToPrototypeObject(context, self->klass);
    else
        prototype->AddInstance(context, self->klass, is_singleton);

    return self;
}

void
ES_Host_Object::Initialize(ES_Host_Object *object, BOOL need_destroy, ES_Class *klass/* = NULL*/)
{
    ES_Object::Initialize(object, klass);

    object->SetIsHostObject();
    object->host_object = NULL;

    if (need_destroy)
        object->SetNeedDestroy();
}

/* static */ void
ES_Host_Object::Destroy(ES_Host_Object *object)
{
    if (EcmaScript_Object *host_object = object->host_object)
        if (reinterpret_cast<char *>(host_object) > reinterpret_cast<char *>(object) &&
            reinterpret_cast<char *>(host_object) < reinterpret_cast<char *>(object) + ObjectSize(object))
            host_object->~EcmaScript_Object();
        else
            OP_DELETE(host_object);
}

static DeleteResult
FinishDelete(ES_Execution_Context *context, ES_DeleteStatus status)
{
    switch (status)
    {
    default:
        OP_ASSERT(!"Cannot happen");
    case DELETE_OK:
        return TRUE;
    case DELETE_REJECT:
        return FALSE;
    case DELETE_NO_MEMORY:
        context->AbortOutOfMemory();
        return FALSE;
    }
}

DeleteResult
ES_Host_Object::DeleteHostL(ES_Execution_Context *context, JString *name, unsigned &result)
{
    OP_ASSERT(this == Identity(context, this));

    ES_Property_Info info;
    if (HasOwnNativeProperty(name, info))
    {
        if (!SecurityCheck(context))
        {
            context->ThrowReferenceError("Security error: attempted to delete protected variable: ", Storage(context, name), Length(name));
            return FALSE;
        }

        if (info.IsDontDelete())
            result = FALSE;
        else
            result = DeleteOwnPropertyL(context, name);
    }
    else
    {
        ES_SuspendedHostDeleteName call(host_object, StorageZ(context, name), context->GetRuntime());
        context->SuspendedCall(&call);
        result = FinishDelete(context, call.status);
    }

    return TRUE;
}

DeleteResult
ES_Host_Object::DeleteHostL(ES_Execution_Context *context, unsigned index, unsigned &result)
{
    OP_ASSERT(this == Identity(context, this));

    if (HasOwnNativeProperty(index))
    {
        if (!SecurityCheck(context))
        {
            context->ThrowReferenceError("Security error: attempted to delete protected variable");
            return FALSE;
        }

        result = DeleteOwnPropertyL(context, index);
    }
    else
    {
        ES_SuspendedHostDeleteIndex call(host_object, index, context->GetRuntime());
        context->SuspendedCall(&call);
        result = FinishDelete(context, call.status);
    }

    return TRUE;
}

GetResult
ES_Host_Object::GetHostL(ES_Execution_Context *context, ES_Object *this_object, JString *name, ES_Value_Internal &value, ES_Object *&prototype_object, ES_PropertyIndex &index)
{
    OP_ASSERT(this == Identity(context, this));

    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;
    BOOL allow_cache = !HasVolatilePropertySet();
    ES_Object *prototype = NULL;
    BOOL is_secure = FALSE;
    BOOL checked_prototype_chain = FALSE;

    /* First check if the object has the property, if not look in the prototype chain else check security.
       HasPropertyWithInfo keeps track of security in the prototype chain.
       Delay checking the prototype chain if we know there isn't a getter somewhere, but if we do check
       it, make sure to reuse that information.
    */
    ES_Object *klass_prototype = klass->Prototype();
    if (!GetOwnLocation(name, info, value_ref) && (klass_prototype && klass_prototype->HasGetterOrSetter() || name->Equals("__proto__", 9)))
    {
        checked_prototype_chain = TRUE;
        if (klass_prototype && klass_prototype->HasPropertyWithInfo(context, name, info, prototype, is_secure, allow_cache))
            /* If the property is found in the prototype chain, check if the property
               has a getter function. If it has, check if the host object allows an
               accessor function for this property. Allow [[Get]] if that doesn't apply. */
            if (!is_secure)
            {
                context->ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(context, name), Length(name));
                return PROP_GET_FAILED;
            }
            else if (prototype->GetOwnLocation(name, info, value_ref) && value_ref.IsGetterOrSetter() && static_cast<ES_Special_Mutable_Access *>(value_ref.GetBoxed())->getter)
            {
                /* AllowGetterSetterFor() also checks security. */
                if (!AllowGetterSetterFor(context, name))
                    return PROP_GET_FAILED;
            }
            else
                value_ref.Set(NULL, ES_STORAGE_WHATEVER, ES_PropertyIndex(UINT_MAX));
    }
    else if (!value_ref.IsNull() && !SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(context, name), Length(name));
        return PROP_GET_FAILED;
    }

    /* Here value_loc either point to a property of the instance or an
       accessor function located in an object in the prototype chain or
       to NULL. If it isn't NULL then security has been checked at this
       point.
    */
    GetResult res = PROP_GET_NOT_FOUND;
    if (!value_ref.IsNull())
    {
        allow_cache = allow_cache && info.CanCacheGet();
        res = GetWithLocationL(context, this_object, info, value_ref, value);
        if (GET_FOUND(res))
        {
            if (!allow_cache)
                res = GET_CANNOT_CACHE(res);
            else
                index = value_ref.Index();

            return res;
        }
    }
    else if (GET_FOUND(res = GetInOwnHostPropertyL(context, name, value)) || res == PROP_GET_FAILED)
        /* If we haven't found it yet, look in the host object */
        return allow_cache ? res : GET_CANNOT_CACHE(res);
    else
    {
        /* If we haven't checked the prototype chain, do so now and check security. */
        if (!checked_prototype_chain && klass_prototype && klass_prototype->HasPropertyWithInfo(context, name, info, prototype, is_secure, allow_cache) && !is_secure)
        {
            context->ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(context, name), Length(name));
            return PROP_GET_FAILED;
        }

        if (prototype)
        {
            /* If we found it in the prototype chain, get it from there. */
            if (prototype->IsHostObject())
            {
                res = prototype->GetOwnPropertyL(context, this_object, this_object, name, value, index);
                if (!GET_FOUND(res))
                    res = static_cast<ES_Host_Object *>(prototype)->GetInOwnHostPropertyL(context, name, value);
            }
            /* Impose a security check for prototype.constructor even if prototype is accessible; the
               property is native and unprotected. */
            else if ((!prototype->IsCrossDomainAccessible() || name->Equals("constructor", 11)) && !SecurityCheck(context))
            {
                context->ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(context, name), Length(name));
                return PROP_GET_FAILED;
            }
            else
                res = prototype->GetOwnPropertyL(context, this_object, this_object, name, value, index);

            if (GET_FOUND(res))
            {
                if (allow_cache)
                {
                    prototype_object = prototype;
                    return res;
                }
                else
                    return GET_CANNOT_CACHE(res);
            }
        }
    }

    if (!SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(context, name), Length(name));
        return PROP_GET_FAILED;
    }
    else
        return PROP_GET_NOT_FOUND;
}

GetResult
ES_Host_Object::GetHostL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, ES_Value_Internal &value)
{
    GetResult result = GetOwnHostPropertyL(context, this_object, index, value);

    if (GET_FOUND(result))
        return result;

    ES_Host_Object *foreign_object = this;
    for (ES_Object *object = Class()->Prototype(); object != NULL; object = object->Class()->Prototype())
    {
        if (object->IsHostObject())
        {
            foreign_object = static_cast<ES_Host_Object *>(object);
            result = foreign_object->GetOwnHostPropertyL(context, this_object, index, value);
            if (GET_FOUND(result))
                return result;
        }
        else if (object->HasOwnNativeProperty(index))
        {
            if (!foreign_object->SecurityCheck(context))
            {
                context->ThrowReferenceError("Security error: attempted to read protected variable");
                return PROP_GET_FAILED;
            }
            return object->GetOwnPropertyL(context, this_object, this_object, index, value);
        }
    }

    if (!SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to read protected variable");
        return PROP_GET_FAILED;
    }
    else
        return PROP_GET_NOT_FOUND;
}

PutResult
ES_Host_Object::PutHostL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value, BOOL in_class, ES_PropertyIndex &index)
{
    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;
    BOOL allow_cache = TRUE;
    ES_Object *prototype;
    BOOL is_secure = FALSE;
    BOOL checked_prototype_chain = FALSE;

    /* First check if the object has the property, if not look in the prototype chain else check security.
       HasPropertyWithInfo keeps track of security in the prototype chain.
       Delay checking the prototype chain if we know there isn't a getter somewhere, but if we do check
       it, make sure to reuse that information.
    */
    ES_Object *klass_prototype = klass->Prototype();
    if (!GetOwnLocation(name, info, value_ref) && (klass_prototype && klass_prototype->HasGetterOrSetter() || name->Equals("__proto__", 9)))
    {
        checked_prototype_chain = TRUE;
        if (klass_prototype && klass_prototype->HasPropertyWithInfo(context, name, info, prototype, is_secure, allow_cache))
            /* If the property is found in the prototype chain and writable, check
               if it has a setter function. If it has, check if the host object allows
               an accessor function for this property. */
            if (!is_secure)
            {
                context->ThrowReferenceError("Security error: attempted to write protected variable: ", Storage(context, name), Length(name));
                return PROP_PUT_FAILED;
            }
            else if (info.IsSpecial() && prototype->GetOwnLocation(name, info, value_ref) && value_ref.IsGetterOrSetter())
                if (static_cast<ES_Special_Mutable_Access *>(value_ref.GetBoxed())->setter)
                {
                    /* AllowGetterSetterFor() also checks security. */
                    if (!AllowGetterSetterFor(context, name))
                        return PROP_PUT_FAILED;
                }
                else
                    return PROP_PUT_READ_ONLY;
            else if (info.IsReadOnly())
                return PROP_PUT_READ_ONLY;
            else
                value_ref.Set(NULL, ES_STORAGE_WHATEVER, ES_PropertyIndex(UINT_MAX));
    }
    else if (!value_ref.IsNull() && !SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to write protected variable: ", Storage(context, name), Length(name));
        return PROP_PUT_FAILED;
    }

    /* Here value_loc points either to a property of the instance or an
       accessor function located in an object in the prototype chain or
       to NULL.
       Security has been checked at this point.
    */
    if (!value_ref.IsNull() && info.IsNativeWriteable())
    {
        PutResult res = PutWithLocation(context, this, info, value_ref, value);

        allow_cache = allow_cache && !info.IsSpecial();
        if (allow_cache && PUT_OK(res) && info.IsClassProperty())
        {
            index = value_ref.Index();
            return PROP_PUT_OK_CAN_CACHE;
        }
        else
            return res;
    }

    FailedReason reason;
    BOOL can_cache = TRUE;
    if (PutInHostL(context, name, value, reason, can_cache, index) || !value_ref.IsNull() && reason == NotFound)
        return PROP_PUT_OK;

    if (reason == ReadOnly)
        return PROP_PUT_READ_ONLY;

    if (reason != NotFound)
        return PROP_PUT_FAILED;

    if (!SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to write protected variable: ", Storage(context, name), Length(name));
        return PROP_PUT_FAILED;
    }

    /* If we haven't checked the prototype chain, do so now and check security. */
    if (!checked_prototype_chain && klass_prototype && klass_prototype->HasPropertyWithInfo(context, name, info, prototype, is_secure, allow_cache))
        /* See GetHostL() comment for why "constructor" is special-cased. */
        if (!is_secure || prototype && prototype->IsCrossDomainAccessible() && name->Equals("constructor", 11) && !SecurityCheck(context))
        {
            context->ThrowReferenceError("Security error: attempted to write protected variable: ", Storage(context, name), Length(name));
            return PROP_PUT_FAILED;
        }
        else if (info.IsReadOnly())
            return PROP_PUT_READ_ONLY;

    info.Reset();

    if (in_class)
    {
        AppendValueL(context, name, index, value, CP, value.GetStorageType());

        Invalidate();

        if (reason == NotFound && can_cache == FALSE)
            /* Some host objects track if a native property
               have been created by catching when they first report a
               failed put -- not marking their native objects during GC
               prior to that event. When that transition do happen, the host object
               must report that property as non-cacheable so as to ensure
               that PutName() gets called on other host object instances of
               the same class. PUT_FAILED_DONT_CACHE serves that purpose. */
            return PROP_PUT_OK;
        else if (IsSingleton() || HasHashTableProperties())
            return PROP_PUT_OK_CAN_CACHE;
        else
            return PROP_PUT_OK_CAN_CACHE_NEW;
    }
    else
    {
        if (!HasHashTableProperties())
            klass = ES_Class::MakeHash(context, klass, Count());

        static_cast<ES_Class_Hash_Base *>(klass)->InsertL(context, name, value, info, Count());

        Invalidate();

        return PROP_PUT_OK;
    }
}

PutResult
ES_Host_Object::PutHostL(ES_Execution_Context *context, unsigned index, unsigned *attributes, const ES_Value_Internal &value)
{
    OP_ASSERT(this == Identity(context, this));

    if (indexed_properties && ES_Indexed_Properties::HasProperty(indexed_properties, index))
    {
        if (!SecurityCheck(context))
        {
            context->ThrowReferenceError("Security error: attempted to write protected variable");
            return PROP_PUT_FAILED;
        }

        return ES_Indexed_Properties::PutL(context, this, index, attributes, value, this);
    }

    FailedReason reason;
    if (PutInHostL(context, index, value, reason))
        return PROP_PUT_OK;

    if (reason == ReadOnly)
        return PROP_PUT_READ_ONLY;

    if (reason != NotFound)
        return PROP_PUT_FAILED;

    if (!SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to write protected variable");
        return PROP_PUT_FAILED;
    }

    return ES_Indexed_Properties::PutL(context, this, index, value, this);
}

BOOL
ES_Host_Object::HasPropertyHost(ES_Execution_Context *context, JString *name)
{
    OP_ASSERT(this == Identity(context, this));

    ES_Property_Info info;
    BOOL is_secure = TRUE;
    if (HasOwnHostProperty(context, name, info, is_secure))
        return TRUE;

    for (ES_Object *object = Class()->Prototype(); object != NULL; object = object->Class()->Prototype())
        if (object->IsHostObject())
        {
            ES_Property_Info info;
            if (static_cast<ES_Host_Object *>(object)->HasOwnHostProperty(context, name, info, is_secure))
                return TRUE;
        }
        else if (object->HasOwnNativeProperty(name))
            return TRUE;

    return FALSE;
}

BOOL
ES_Host_Object::HasPropertyHost(ES_Execution_Context *context, unsigned index)
{
    OP_ASSERT(this == Identity(context, this));

    ES_Property_Info info;
    BOOL is_secure = TRUE;
    if (HasOwnHostProperty(context, index, info, is_secure))
        return TRUE;

    for (ES_Object *object = Class()->Prototype(); object != NULL; object = object->Class()->Prototype())
        if (object->IsHostObject() && static_cast<ES_Host_Object *>(object)->HasOwnHostProperty(context, index, info, is_secure))
            return TRUE;
        else if (object->HasOwnNativeProperty(index))
            return TRUE;

    return FALSE;
}

/* static */ BOOL
ES_Host_Object::ConvertL(ES_Execution_Context *context, ES_PutState reason, const ES_Value_Internal &from, ES_Value_Internal &to)
{
    to = from;

    switch (reason)
    {
    case PUT_NEEDS_BOOLEAN:
        to.ToBoolean();
        return TRUE;

    case PUT_NEEDS_NUMBER:
        return to.ToNumber(context);

    default:
        return to.ToString(context);
    }
}

/* static */ BOOL
ES_Host_Object::ConvertL(ES_Execution_Context *context, ES_ArgumentConversion reason, const ES_Value_Internal &from, ES_Value_Internal &to)
{
    to = from;

    switch (reason)
    {
    case ES_CALL_NEEDS_BOOLEAN:
        to.ToBoolean();
        return TRUE;

    case ES_CALL_NEEDS_NUMBER:
        return to.ToNumber(context);

    default:
        return to.ToString(context);
    }
}

/* static */ GetResult
ES_Host_Object::FinishGet(ES_Execution_Context *context, ES_GetState state, const ES_Value &external_value)
{
    GetResult result = PROP_GET_FAILED;
    switch (state)
    {
        case GET_SECURITY_VIOLATION:
            context->ThrowReferenceError("Security error: attempted to read protected variable");
            result = PROP_GET_FAILED;
            break;
        case GET_EXCEPTION:
            context->ThrowValue(external_value);
            result = PROP_GET_FAILED;
            break;
        case GET_NO_MEMORY:
            context->AbortOutOfMemory();
            result = PROP_GET_FAILED;
            break;
        case GET_FAILED:
            result = PROP_GET_NOT_FOUND;
            break;
        case GET_SUCCESS:
            result = PROP_GET_OK;
            break;
        default: OP_ASSERT(!"implementation error");
    }
    return result;
}

/* static */ BOOL
ES_Host_Object::FinishPut(ES_Execution_Context *context, ES_PutState state, FailedReason &reason, BOOL &can_cache, const ES_Value &external_value)
{
    switch (state)
    {
    case PUT_SUCCESS:
        reason = NoReason;
        return TRUE;
    case PUT_FAILED_DONT_CACHE:
        can_cache = FALSE;
        /* fall through */
    case PUT_FAILED:
        reason = NotFound;
        break;
    case PUT_READ_ONLY:
        reason = ReadOnly;
        break;
    case PUT_SECURITY_VIOLATION:
        reason = SecurityViolation;
        context->ThrowReferenceError("Security error: attempted to write protected variable");
        break;
    case PUT_NEEDS_STRING:
        reason = NeedsString;
        break;
    case PUT_NEEDS_STRING_WITH_LENGTH:
        reason = NeedsStringWithLength;
        break;
    case PUT_NEEDS_NUMBER:
        reason = NeedsNumber;
        break;
    case PUT_NEEDS_BOOLEAN:
        reason = NeedsBoolean;
        break;
    case PUT_NO_MEMORY:
        context->AbortOutOfMemory();
        break;
    case PUT_EXCEPTION:
        reason = Exception;
        context->ThrowValue(external_value);
        break;
    default: OP_ASSERT(!"implementation error");
    }
    return FALSE;
}

/* static */ void
ES_Host_Object::SuspendL(ES_Execution_Context *context, ES_Value_Internal *restart_object, const ES_Value &external_value)
{
    restart_object->ImportL(context, external_value);
    context->Block();
}

static void
CheckDOMReturn(ES_Context *context, OP_STATUS e)
{
    if (OpStatus::IsMemoryError(e))
        context->AbortOutOfMemory();
    else if (OpStatus::IsError(e))
    {
        OP_ASSERT(!"Implementation error, only oom errors are allowed");
        context->AbortError();
    }
}

/* static */ ES_Object *
ES_Host_Object::Identity(ES_Context *context, ES_Host_Object *target)
{
    ES_Execution_Context *exec_context = context->GetExecutionContext();
    if (!exec_context)
        return IdentityStandardStack(target);

    for (;;)
    {
        EcmaScript_Object *host = target->GetHostObject();
        if (host == NULL)
            return target;

        if (host->GetRuntime() == NULL)
            return target;

        ES_SuspendedHostIdentity call(host);
        exec_context->SuspendedCall(&call);

        CheckDOMReturn(exec_context, call.result);

        ES_Host_Object *identity = static_cast<ES_Host_Object *>(call.loc);
        if (identity == target || !identity->HasMultipleIdentities())
            return identity;

        target = identity;
    }
}

/* static */ ES_Object *
ES_Host_Object::IdentityStandardStack(ES_Host_Object *target)
{
    for (;;)
    {
        EcmaScript_Object *host = target->GetHostObject();
        if (host == NULL)
            return target;

        if (host->GetRuntime() == NULL)
            return target;

        ES_Object *loc;
        OP_STATUS result = host->Identity(&loc);

        if (OpStatus::IsMemoryError(result))
            return target;
        else if (OpStatus::IsError(result))
        {
            OP_ASSERT(!"Implementation error, only oom errors are allowed");
            return target;
        }

        ES_Host_Object *identity = static_cast<ES_Host_Object *>(loc);
        if (identity == target || !identity->HasMultipleIdentities())
            return identity;

        target = identity;
    }
}

void
ES_Host_Object::FetchProperties(ES_Context *context, ES_PropertyEnumerator *enumerator)
{
    if (context->IsUsingStandardStack())
    {
        ES_CollectorLock gclock(context);

        ES_GetState result = host_object->FetchPropertiesL(enumerator, context->GetRuntime());
        if (result == GET_NO_MEMORY)
            context->AbortOutOfMemory();
        OP_ASSERT(result != GET_SUSPEND);
    }
    else
    {
        ES_Execution_Context *exec_context = context->GetExecutionContext();
        OP_ASSERT(exec_context);
        ES_SuspendedHostFetchProperties call(host_object, enumerator, context->GetRuntime());
        while (exec_context->SuspendedCall(&call), call.result == GET_SUSPEND)
            exec_context->Block();

        if (call.result == GET_NO_MEMORY)
            context->AbortOutOfMemory();
    }
}

unsigned
ES_Host_Object::GetIndexedPropertiesLength(ES_Context *context)
{
    if (context->IsUsingStandardStack())
    {
        ES_CollectorLock gclock(context);

        unsigned length;
        ES_GetState result = host_object->GetIndexedPropertiesLength(length, context->GetRuntime());
        if (result == GET_NO_MEMORY)
            context->AbortOutOfMemory();
        OP_ASSERT(result != GET_SUSPEND);
        return length;
    }
    else
    {
        ES_Execution_Context *exec_context = context->GetExecutionContext();
        OP_ASSERT(exec_context);
        ES_SuspendedHostGetIndexedPropertiesLength call(host_object, context->GetRuntime());
        while (exec_context->SuspendedCall(&call), call.result == GET_SUSPEND)
            exec_context->Block();

        if (call.result == GET_NO_MEMORY)
            context->AbortOutOfMemory();

        return call.return_value;
    }
}

BOOL
ES_Host_Object::AllowOperationOnProperty(ES_Execution_Context *context, JString *name, EcmaScript_Object::PropertyOperation op)
{
    ES_SuspendedHostAllowPropertyOperationFor call(host_object, context->GetRuntime(), StorageZ(context, name), op);
    context->SuspendedCall(&call);

    if (!call.result && op == EcmaScript_Object::ALLOW_ACCESSORS)
        context->ThrowTypeError("Security error: getter/setter not allowed for: ", Storage(context, name), Length(name));

    return call.result;
}

BOOL
ES_Host_Object::GetOwnHostPropertyL(ES_Execution_Context *context, PropertyDescriptor &desc, JString *name, unsigned index, BOOL fetch_value, BOOL fetch_accessors)
{
    BOOL is_secure = IsCrossDomainAccessible() || SecurityCheck(context);
    BOOL has_property;
    if (name)
    {
        ES_SuspendedHostHasPropertyName call(host_object, StorageZ(context, name), name->HostCode(), context->GetRuntime());
        while (context->SuspendedCall(&call), call.result == GET_SUSPEND)
        {
            context->Block();
            call.is_restarted = TRUE;
        }

        if (call.result == GET_NO_MEMORY)
            context->AbortOutOfMemory();

        /* The handling of is_secure mirrors that for [[HasProperty]].
           See it for explanation. */
        has_property = call.result == GET_SUCCESS;

        if (call.result == GET_SECURITY_VIOLATION)
            is_secure = FALSE;
        else if (has_property)
            is_secure = TRUE;
    }
    else
    {
        ES_SuspendedHostHasPropertyIndex call(host_object, index, context->GetRuntime());

        while (context->SuspendedCall(&call), call.result == GET_SUSPEND)
        {
            context->Block();
            call.is_restarted = TRUE;
        }

        if (call.result == GET_NO_MEMORY)
            context->AbortOutOfMemory();

        has_property = call.result == GET_SUCCESS;

        if (call.result == GET_SECURITY_VIOLATION)
            is_secure = FALSE;
        else if (has_property)
            is_secure = TRUE;
    }

    if (!is_secure)
    {
        context->ThrowTypeError("Security error: protected variable accessed");
        return FALSE;
    }

    if (context->ShouldYield())
        context->YieldImpl();

    if (has_property)
    {
        ES_Object *getter = NULL, *setter = NULL;

        if (fetch_accessors)
        {
            ES_Global_Object *global_object = context->GetGlobalObject();
            getter = ES_Function::Make(context, global_object->GetBuiltInGetterSetterClass(), global_object, 0, HostGetter, 0, 0, name, NULL);
            ES_CollectorLock gclock(context);
            setter = ES_Function::Make(context, global_object->GetBuiltInGetterSetterClass(), global_object, 1, HostSetter, 0, 0, name, NULL);
        }

        desc.SetEnumerable(TRUE);
        if (name)
            desc.SetConfigurable(AllowOperationOnProperty(context, name, EcmaScript_Object::ALLOW_NATIVE_OVERRIDE));
        else
            desc.SetConfigurable(FALSE);
        desc.SetGetter(getter);
        desc.SetSetter(setter);

        desc.is_host_property = TRUE;
    }
    else
        desc.is_undefined = TRUE;

    return TRUE;
}

/* static */ BOOL
ES_Host_Object::HostGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    OP_ASSERT(argv[-1].GetObject()->IsFunctionObject());

    ES_Function *function_object = static_cast<ES_Function *>(argv[-1].GetObject());
    ES_Value_Internal name_value;
    function_object->GetCachedAtIndex(ES_PropertyIndex(ES_Function::NAME), name_value);
    OP_ASSERT(name_value.IsString());
    JString *name = name_value.GetString();

    if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsHostObject())
    {
        context->ThrowTypeError("Failed to call host getter");
        return FALSE;
    }

    ES_Host_Object *this_object = static_cast<ES_Host_Object *>(argv[-2].GetObject(context));

    GetResult res = this_object->GetInOwnHostPropertyL(context, name, *return_value);

    if (GET_NOT_FOUND(res))
        return_value->SetUndefined();
    else if (!GET_OK(res))
    {
        context->ThrowTypeError("Failed to call host getter");
        return FALSE;
    }

    return TRUE;
}

/* static */ BOOL
ES_Host_Object::HostSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    OP_ASSERT(argv[-1].GetObject()->IsFunctionObject());

    ES_Function *function_object = static_cast<ES_Function *>(argv[-1].GetObject());
    ES_Value_Internal name_value;
    function_object->GetCachedAtIndex(ES_PropertyIndex(ES_Function::NAME), name_value);
    OP_ASSERT(name_value.IsString());
    JString *name = name_value.GetString();

    if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsHostObject() || argc < 1)
    {
        context->ThrowTypeError("Failed to call host setter");
        return FALSE;
    }

    ES_Host_Object *this_object = static_cast<ES_Host_Object *>(argv[-2].GetObject(context));

    FailedReason reason;
    ES_PropertyIndex dummy;
    BOOL can_cache = TRUE;

    if (this_object->PutInHostL(context, name, argv[0], reason, can_cache, dummy))
        return TRUE;

    context->ThrowTypeError("Failed to call host setter");

    return FALSE;
}


GetResult
ES_Host_Object::GetOwnHostPropertyL(ES_Execution_Context *context, ES_Object *this_object, JString *name, ES_Value_Internal &value, ES_PropertyIndex &index)
{
    BOOL allow_cache = !HasVolatilePropertySet();

    if (!SecurityCheck(context))
    {
        context->ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(context, name), Length(name));
        return PROP_GET_FAILED;
    }

    GetResult res = GetOwnPropertyL(context, this_object, this_object, name, value, index);

    if (allow_cache)
        return res;
    else
        return GET_CANNOT_CACHE(res);
}

GetResult
ES_Host_Object::GetOwnHostPropertyL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, ES_Value_Internal &value)
{
    if (indexed_properties && ES_Indexed_Properties::HasProperty(indexed_properties, index))
    {
        if (!SecurityCheck(context))
        {
            context->ThrowReferenceError("Security error: attempted to read protected variable");
            return PROP_GET_FAILED;
        }
        ES_Indexed_Properties::GetL(context, this_object, indexed_properties, index, value, this_object);
        return PROP_GET_OK;
    }

    ES_Value external_value;
    ES_Value_Internal tmp_value;
    ES_Runtime *runtime = context->GetRuntime();
    ES_Value_Internal *restart_object = NULL;

    ES_SuspendedHostGetIndex call(host_object, index, &external_value, runtime);

    while (context->SuspendedCall(&call), call.state == GET_SUSPEND)
    {
        OP_ASSERT(external_value.type == VALUE_NULL || external_value.type == VALUE_OBJECT);

        if (!restart_object)
            restart_object = context->AllocateRegisters(1);

        SuspendL(context, restart_object, external_value);

        call.is_restarted = TRUE;
        call.restart_object = restart_object->IsNull() ? NULL : restart_object->GetObject();
    }

    if (restart_object)
        context->FreeRegisters(1);

    OP_ASSERT(call.state != GET_SUSPEND);

    GetResult result;
    if (GET_OK(result = FinishGet(context, call.state, external_value)))
        value.ImportUnlockedL(context, external_value);

    context->MaybeYield();

    return result;
}

GetResult
ES_Host_Object::GetInOwnHostPropertyL(ES_Execution_Context *context, JString *name, ES_Value_Internal &value)
{
    ES_Value external_value;
    ES_Value_Internal tmp_value;
    ES_Runtime *runtime = context->GetRuntime();
    ES_Value_Internal *restart_object = NULL;

    ES_SuspendedHostGetName call(host_object, StorageZ(context, name), name->HostCode(), &external_value, runtime);

    while (context->SuspendedCall(&call), call.state == GET_SUSPEND)
    {
        OP_ASSERT(external_value.type == VALUE_NULL || external_value.type == VALUE_OBJECT);

        if (!restart_object)
            restart_object = context->AllocateRegisters(1);

        SuspendL(context, restart_object, external_value);

        call.is_restarted = TRUE;
        call.restart_object = restart_object->IsNull() ? NULL : restart_object->GetObject();
    }

    if (restart_object)
        context->FreeRegisters(1);

    OP_ASSERT(call.state != GET_SUSPEND);

    GetResult result;
    if (GET_OK(result = FinishGet(context, call.state, external_value)))
        value.ImportUnlockedL(context, external_value);

    context->MaybeYield();

    return result;
}

BOOL
ES_Host_Object::PutInHostL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value, FailedReason &reason, BOOL &can_cache, ES_PropertyIndex &index)
{
    ES_Value external_value;
    ES_ValueString external_string;

    ES_Runtime *runtime = context->GetRuntime();
    value.ExportL(context, external_value);

    ES_Value_Internal *restart_object = NULL;

    ES_SuspendedHostPutName call(host_object, StorageZ(context, name), name->HostCode(), &external_value, runtime);

    while (context->SuspendedCall(&call), call.state == PUT_SUSPEND || call.state >= PUT_NEEDS_STRING && call.state <= PUT_NEEDS_BOOLEAN)
    {
        if (call.state == PUT_SUSPEND)
        {
            OP_ASSERT(external_value.type == VALUE_NULL || external_value.type == VALUE_OBJECT);

            if (!restart_object)
                restart_object = context->AllocateRegisters(1);

            SuspendL(context, restart_object, external_value);

            call.restart_object = restart_object->IsNull() ? NULL : restart_object->GetObject();
            call.is_restarted = TRUE;
            value.ExportL(context, external_value);
        }
        else
        {
            ES_Value_Internal converted;

            if (!ConvertL(context, call.state, value, converted))
                return FALSE;

            char format;
            if (call.state == PUT_NEEDS_STRING_WITH_LENGTH)
                format = 'z';
            else
                format = '-';

            converted.ExportL(context, external_value, format, &external_string);
        }
    }

    if (restart_object)
        context->FreeRegisters(1);

    OP_ASSERT(call.state != PUT_SUSPEND);

    BOOL success = FinishPut(context, call.state, reason, can_cache, external_value);

    context->MaybeYield();

    return success;
}

BOOL
ES_Host_Object::PutInHostL(ES_Execution_Context *context, unsigned index, const ES_Value_Internal &value, FailedReason &reason)
{
    ES_Value external_value;
    ES_ValueString external_string;

    ES_Runtime *runtime = context->GetRuntime();
    value.ExportL(context, external_value);

    ES_Value_Internal *restart_object = NULL;

    ES_SuspendedHostPutIndex call(host_object, index, &external_value, runtime);

    while (context->SuspendedCall(&call), call.state == PUT_SUSPEND || call.state >= PUT_NEEDS_STRING && call.state <= PUT_NEEDS_BOOLEAN)
    {
        if (call.state == PUT_SUSPEND)
        {
            OP_ASSERT(external_value.type == VALUE_NULL || external_value.type == VALUE_OBJECT);

            if (!restart_object)
                restart_object = context->AllocateRegisters(1);

            SuspendL(context, restart_object, external_value);

            call.restart_object = restart_object->IsNull() ? NULL : restart_object->GetObject();
            call.is_restarted = TRUE;
            value.ExportL(context, external_value);
        }
        else
        {
            ES_Value_Internal converted;

            if (!ConvertL(context, call.state, value, converted))
                return FALSE;

            char format;
            if (call.state == PUT_NEEDS_STRING_WITH_LENGTH)
                format = 'z';
            else
                format = '-';

            converted.ExportL(context, external_value, format, &external_string);
        }
    }

    if (restart_object)
        context->FreeRegisters(1);

    OP_ASSERT(call.state != PUT_SUSPEND);

    BOOL can_cache_unused;
    BOOL success = FinishPut(context, call.state, reason, can_cache_unused, external_value);

    context->MaybeYield();

    return success;
}

BOOL
ES_Host_Object::HasOwnHostProperty(ES_Context *context, JString *name, ES_Property_Info &info, BOOL &is_secure)
{
    if (is_secure || (is_secure = IsCrossDomainAccessible() || SecurityCheck(static_cast<ES_Execution_Context *>(context))))
    {
        ES_Value_Internal_Ref value_ref;
        if (GetOwnLocation(name, info, value_ref))
            return TRUE;

        info.Reset();
        info.data.dont_enum = 1;
    }
    /* Property is not a secure native property.

       At this point, 'is_secure' is set to TRUE if the caller considers
       the access secure or the above security check passed. 'is_secure'
       is FALSE if the security check failed.

       Even if FALSE, [[HasProperty]] will be performed on the host object.
       If that property is cross-domain accessible, that takes precedence
       and must set is_secure to TRUE and allow the access. */

    ES_Execution_Context *exec_context = static_cast<ES_Execution_Context *>(context);
    ES_SuspendedHostHasPropertyName call(host_object, StorageZ(context, name), name->HostCode(), context->GetRuntime());
    while (exec_context->SuspendedCall(&call), call.result == GET_SUSPEND)
    {
        exec_context->Block();
        call.is_restarted = TRUE;
    }

    if (exec_context->ShouldYield())
    {
        exec_context->YieldImpl();
        exec_context->SuspendedCall(&call);
    }

    BOOL result = call.result == GET_SUCCESS;

    /* If 'result' is TRUE, the corresponding 'is_secure' value allows or denies
       the access. If 'result' is FALSE, 'is_secure' determines the security
       of performing [[HasProperty]] further along the prototype chain.

       Hence updating 'is_result' correctly in light of the host [[HasProperty]]
       call matters and must be done carefully.

       The 'is_secure' result must be updated iff:

        - The host property access was reported as insecure and not
          present; set 'is_secure' to FALSE.

        - The host property was reported as present, then it is a
          cross-domain property; set 'is_secure' to TRUE.

        - The host property doesn't exist but the prototype is marked as
          having insecure host properties; delegate the security decision
          to the prototype by setting 'is_secure' to TRUE. */

    if (call.result == GET_SECURITY_VIOLATION)
        is_secure = FALSE;
    else if (result)
        is_secure = TRUE;
    else if (!is_secure)
        if (ES_Object *prototype = klass->Prototype())
            is_secure = prototype->IsCrossDomainHostAccessible();

    return result;
}

BOOL
ES_Host_Object::HasOwnHostProperty(ES_Context *context, unsigned index, ES_Property_Info &info, BOOL &is_secure)
{
    if (is_secure || (is_secure = IsCrossDomainAccessible() || SecurityCheck(static_cast<ES_Execution_Context *>(context))))
    {
        if (indexed_properties && ES_Indexed_Properties::HasProperty(indexed_properties, index, info))
            return TRUE;

        info.Reset();
        info.data.dont_enum = 1;
    }

    ES_SuspendedHostHasPropertyIndex call(host_object, index,  context->GetRuntime());
    ES_Execution_Context *exec_context = static_cast<ES_Execution_Context *>(context);
    while (exec_context->SuspendedCall(&call), call.result == GET_SUSPEND)
    {
        exec_context->Block();
        call.is_restarted = TRUE;
    }

    if (exec_context->ShouldYield())
    {
        exec_context->YieldImpl();
        exec_context->SuspendedCall(&call);
    }

    BOOL result = call.result == GET_SUCCESS;

    /* See above comment for [[HasProperty]] over property names for explanation. */
    if (call.result == GET_SECURITY_VIOLATION)
        is_secure = FALSE;
    else if (result)
        is_secure = TRUE;
    else if (!is_secure)
        if (ES_Object *prototype = klass->Prototype())
            is_secure = prototype->IsCrossDomainHostAccessible();

    return call.result;
}

BOOL
ES_Host_Object::SecurityCheck(ES_Context *context)
{
    if (host_object->GetRuntime() == context->GetRuntime())
        return TRUE;
    else if (context->IsUsingStandardStack())
    {
        return host_object->SecurityCheck(context->GetRuntime());
    }
    else
    {
        ES_SuspendedHostSecurityCheck call(host_object, context->GetRuntime());
        static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
        return call.result;
    }
}

void
ES_Host_Object::SignalIdentityChange(ES_Object *previous_identity)
{
    if (previous_identity && IsProxyInstanceAdded())
    {
        previous_identity->Class()->RemoveInstance(Class()->GetRootClass());
        ClearProxyInstanceAdded();
        Class()->Invalidate();
    }
}

void
ES_Host_Object::SignalDisableCaches(ES_Object *current_identity)
{
    if (current_identity && IsProxyInstanceAdded())
    {
        current_identity->Class()->RemoveInstance(Class()->GetRootClass());
        ClearProxyInstanceAdded();
    }

    SetProxyCachingDisabled();

    Class()->Invalidate();
}

/* host functions */

ES_Host_Function *
ES_Host_Function::Make(ES_Context *context, ES_Global_Object *global_object, EcmaScript_Object* host_object, ES_Object* prototype, const uni_char *function_name, const char* object_class, const char *parameter_types)
{
    OP_ASSERT(prototype);
#if 0
    OP_ASSERT(prototype->IsPrototypeObject());
#endif
    OP_ASSERT(!prototype->HasMultipleIdentities());

    ES_Class *klass = ES_Class::MakeRoot(context, prototype, object_class);
    ES_CollectorLock gclock(context);

    ES_Host_Function *self;
    GC_ALLOCATE(context, self, ES_Host_Function, (self, TRUE, klass, parameter_types));

    /* FIXME: We would much rather want prototype to be a prototype to begin with. */
    if (!prototype->IsPrototypeObject())
        prototype->ConvertToPrototypeObject(context, self->klass);
    else
        prototype->AddInstance(context, self->klass, FALSE);

    self->function_name = context->rt_data->GetSharedString(function_name);
    self->AllocateProperties(context);

    self->host_object = host_object;
    return self;
}

/* static */ ES_Host_Function *
ES_Host_Function::Make(ES_Context *context, ES_Global_Object *global_object, void *&payload, unsigned payload_size, ES_Object* prototype, const uni_char *function_name, const char* object_class, const char *parameter_types, BOOL need_destroy, BOOL is_singleton)
{
    OP_ASSERT(prototype);
#if 0
    OP_ASSERT(prototype->IsPrototypeObject());
#endif
    OP_ASSERT(!prototype->HasMultipleIdentities());

    ES_Class *klass;

    if (is_singleton)
        klass = ES_Class::MakeSingleton(context, prototype, object_class);
    else
        klass = ES_Class::MakeRoot(context, prototype, object_class);

    ES_CollectorLock gclock(context);

    ES_Host_Function *self;
    GC_ALLOCATE_WITH_EXTRA_ALIGNED_PAYLOAD(context, self, payload, payload_size, ES_Host_Function, (self, need_destroy, klass, parameter_types));

    /* FIXME: We would much rather want prototype to be a prototype to begin with. */
    if (!prototype->IsPrototypeObject())
        prototype->ConvertToPrototypeObject(context, self->klass);
    else
        prototype->AddInstance(context, self->klass, is_singleton);

    self->function_name = context->rt_data->GetSharedString(function_name);
    self->AllocateProperties(context);

    return self;
}

void
ES_Host_Function::Initialize(ES_Host_Function *object, BOOL need_destroy, ES_Class *klass, const char *parameter_types)
{
    ES_Host_Object::Initialize(object, need_destroy, klass);

    object->ChangeGCTag(GCTAG_ES_Object_Function);

    object->function_name = NULL;
    object->parameter_types = parameter_types;
    object->parameter_types_length = parameter_types ? op_strlen(parameter_types) : 0;
}


/*
There's a lot of code duplication going on in Call and Construct below:
*/

/* static */ BOOL
ES_Host_Function::Call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    const ES_Value_Internal &host_fn_value = argv[-1];
    ES_Value_Internal &this_value = argv[-2];

    if (!this_value.ToObject(context, FALSE))
        this_value.SetObject(context->GetGlobalObject());

    ES_Host_Function *host_fn =  static_cast<ES_Host_Function *>(host_fn_value.GetObject(context));
    ES_Object *this_object = this_value.IsObject() ? this_value.GetObject() : context->GetGlobalObject();

    OP_ASSERT(host_fn->IsHostFunctionObject());

    ES_Value *conv_argv;
    ES_ValueString *conv_argv_strings;

    context->GetHostArguments(conv_argv, conv_argv_strings, argc);

    if (!FormatArguments(context, argc, argv, conv_argv, conv_argv_strings, host_fn->parameter_types))
    {
        context->ReleaseHostArguments();
        return FALSE;
    }

    ES_Value_Internal *restart_object = NULL;
    ES_Value retval;

    ES_SuspendedHostCall call(host_fn->host_object, this_object, conv_argv, argc, &retval, context->GetRuntime());

    while (context->SuspendedCall(&call), (call.result & ES_RESTART) || (call.result & ES_NEEDS_CONVERSION))
    {
        if (call.result & ES_RESTART)
        {
            if (!restart_object)
                restart_object = context->AllocateRegisters(1);

            OP_ASSERT(call.result & ES_SUSPEND); // One without the other makes little sense
            SuspendL(context, restart_object, retval);
        }
        else
        {
            OP_ASSERT ((call.result & ES_NEEDS_CONVERSION) && (retval.type == VALUE_NUMBER || retval.type == VALUE_STRING));
            if (retval.type == VALUE_STRING)
            {
                // A hack..but allocating a temporary (const char *) here is trouble and somewhat lacking in point.
                const char *format = reinterpret_cast<const char *>(retval.value.string);
                if (!FormatArguments(context, argc, argv, conv_argv, conv_argv_strings, format))
                    return FALSE;
                call.conversion_restarted = TRUE;
            }
            else
            {
                ES_ArgumentConversion convert = static_cast<ES_ArgumentConversion>(static_cast<unsigned>(retval.value.number) & 0xffff);
                unsigned arg_number = (static_cast<unsigned>(retval.value.number) >> 16) & 0xffff;
                OP_ASSERT(arg_number < argc);
                ES_Value_Internal converted;

                if (!ConvertL(context, convert, argv[arg_number], converted))
                {
                    context->ReleaseHostArguments();
                    return FALSE;
                }

                char format;
                if (convert == ES_CALL_NEEDS_STRING_WITH_LENGTH)
                    format = 'z';
                else
                    format = '-';

                converted.ExportL(context, conv_argv[arg_number], format, &conv_argv_strings[arg_number]);
            }
            call.restarted = FALSE;
        }
    }

    if (restart_object)
        context->FreeRegisters(1);

    BOOL success = ES_Host_Function::FinishL(context, call.result, retval, return_value, FALSE);

    context->ReleaseHostArguments();
    context->MaybeYield();

    return success;
}

/* static */ BOOL
ES_Host_Function::Construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    const ES_Value_Internal &host_fn_value = argv[-1];

    ES_Host_Function *host_fn =  static_cast<ES_Host_Function *>(host_fn_value.GetObject(context));

    OP_ASSERT(host_fn->IsHostFunctionObject());

    ES_Value *conv_argv;
    ES_ValueString *conv_argv_strings;

    context->GetHostArguments(conv_argv, conv_argv_strings, argc);

    if (!FormatArguments(context, argc, argv, conv_argv, conv_argv_strings, host_fn->parameter_types))
        return FALSE;

    ES_Value_Internal *restart_object = NULL;
    ES_Value retval;

    ES_SuspendedHostConstruct call(host_fn->host_object, conv_argv, argc, &retval, context->GetRuntime());

    while (context->SuspendedCall(&call), (call.result & ES_RESTART) || (call.result & ES_NEEDS_CONVERSION))
    {
        if (call.result & ES_RESTART)
        {
            if (!restart_object)
                restart_object = context->AllocateRegisters(1);

            OP_ASSERT(call.result & ES_SUSPEND); // One without the other makes little sense
            SuspendL(context, restart_object, retval);
        }
        else
        {
            OP_ASSERT ((call.result & ES_NEEDS_CONVERSION) && (retval.type == VALUE_NUMBER || retval.type == VALUE_STRING));
            if (retval.type == VALUE_STRING)
            {
                // A hack..but allocating a temporary (const char *) here is trouble and somewhat lacking in point.
                const char *format = reinterpret_cast<const char *>(retval.value.string);
                if (!FormatArguments(context, argc, argv, conv_argv, conv_argv_strings, format))
                    return FALSE;
                call.conversion_restarted = TRUE;
            }
            else
            {
                ES_ArgumentConversion convert = static_cast<ES_ArgumentConversion>(static_cast<unsigned>(retval.value.number) & 0xffff);
                unsigned arg_number = (static_cast<unsigned>(retval.value.number) >> 16) & 0xffff;
                OP_ASSERT(arg_number < argc);
                ES_Value_Internal converted;

                if (!ConvertL(context, convert, argv[arg_number], converted))
                {
                    context->ReleaseHostArguments();
                    return FALSE;
                }

                char format;
                if (convert == ES_CALL_NEEDS_STRING_WITH_LENGTH)
                    format = 'z';
                else
                    format = '-';

                converted.ExportL(context, conv_argv[arg_number], format, &conv_argv_strings[arg_number]);
            }
            call.restarted = FALSE;
        }
    }

    if (restart_object)
        context->FreeRegisters(1);

    BOOL success = ES_Host_Function::FinishL(context, call.result, retval, return_value, TRUE);

    context->MaybeYield();

    return success;
}

static void
ES_SkipFormatSpecifier(const char *&format)
{
    unsigned level = 0;

    if (*format == '?')
        ++format;

    if (*format == '#')
    {
        ++format;
        while (*format != '|')
        {
            OP_ASSERT(*format != ')');
            ++format;
        }
    }
    else
        do
        {
            if (*format == '{' || *format == '[' || *format == '(')
                ++level;
            else if (*format == '}' || *format == ']' || *format == ')')
                --level;

            ++format;
        }
        while (level > 0);
}

/* static */ BOOL
ES_Host_Function::FormatDictionary(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source)
{
    ES_Value_Internal *registers = context->AllocateRegisters(2);
    ES_Value_Internal &anchor = registers[0];
    ES_Value_Internal &value = registers[1];

    ES_Object *object = source.GetObject(context);
    ES_Object *dictionary = ES_Object::Make(context, context->GetGlobalObject()->GetEmptyClass());

    anchor.SetObject(dictionary);

    OP_ASSERT(*format == '{');

    do
    {
        const char *name_start = ++format;

        while (*format != ':')
        {
            OP_ASSERT(*format && *format != '}');
            ++format;
        }

        JString *name = context->rt_data->GetSharedString(name_start, format - name_start);
        GetResult result = object->GetL(context, name, value);

        ++format;

        if (GET_OK(result))
        {
            if (!FormatValue(context, format, value))
                goto failure;

#ifdef DEBUG_ENABLE_OPASSERT
            PutResult result =
#endif // DEBUG_ENABLE_OPASSERT
                dictionary->PutL(context, name, value);

            /* We allocated an empty object that doesn't even have a prototype;
               there's no way we can fail to put properties on it. */
            OP_ASSERT(PUT_OK(result));
        }
        else if (result == PROP_GET_FAILED)
            goto failure;
        else
            ES_SkipFormatSpecifier(format);

        OP_ASSERT(*format == ',' || *format == '}');
    }
    while (*format == ',');

    OP_ASSERT(*format == '}');

    ++format;

    source.SetObject(dictionary);

    context->FreeRegisters(2);
    return TRUE;

failure:
    context->FreeRegisters(2);
    return FALSE;
}

/* static */ BOOL
ES_Host_Function::FormatArray(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source)
{
    ES_Value_Internal *registers = context->AllocateRegisters(2);
    ES_Value_Internal &anchor = registers[0];
    ES_Value_Internal &value = registers[1];

    ES_Object *object = source.GetObject(context);
    ES_Object *array = ES_Object::Make(context, context->GetGlobalObject()->GetEmptyClass());

    anchor.SetObject(array);

    OP_ASSERT(*format == '[');

    ++format;

    GetResult result = object->GetL(context, context->rt_data->idents[ESID_length], value);

    if (GET_OK(result))
    {
        if (!value.ToNumber(context))
            goto failure;

        unsigned length = value.GetNumAsUInt32(), index;
        value.SetUInt32(length);

#ifdef DEBUG_ENABLE_OPASSERT
        PutResult result =
#endif // DEBUG_ENABLE_OPASSERT
            array->PutL(context, context->rt_data->idents[ESID_length], value);

        OP_ASSERT(PUT_OK(result));

        ES_Array_Property_Iterator iterator(context, object, length);

        while (iterator.Next(index))
        {
            if (!iterator.GetValue(value))
                goto failure;

            const char *format_before = format;

            if (!FormatValue(context, format, value))
                goto failure;

            format = format_before;

#ifdef DEBUG_ENABLE_OPASSERT
            PutResult result =
#endif // DEBUG_ENABLE_OPASSERT
                array->PutL(context, index, value);

            OP_ASSERT(PUT_OK(result));
        }
    }
    else if (result == PROP_GET_FAILED)
        goto failure;

    ES_SkipFormatSpecifier(format);

    OP_ASSERT(*format == ']');

    ++format;

    source.SetObject(array);

    context->FreeRegisters(2);
    return TRUE;

failure:
    context->FreeRegisters(2);
    return FALSE;
}

/* static */ BOOL
ES_Host_Function::FormatAlternatives(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source, char &export_conversion)
{
    OP_ASSERT(*format == '(');

    export_conversion = '-';

    do
    {
        ++format;

        if (*format == '#')
        {
            const char *name_start = ++format;

            while (*format != '|')
            {
                OP_ASSERT(*format != ')');
                ++format;
            }

            if (source.IsObject())
            {
                unsigned name_length = format - name_start;
                const char *object_name = source.GetObject(context)->ObjectName();

                if (op_strncmp(object_name, name_start, name_length) == 0 && !object_name[name_length])
                    break;
            }
        }
        else
        {
            const char *format_before = format;

            ES_SkipFormatSpecifier(format);

            if (*format == '|')
            {
                BOOL matched;

                switch (*format_before)
                {
                case 'b': matched = source.IsBoolean(); break;
                case 'n': matched = source.IsNumber(); break;
                case 's':
                case 'z': matched = source.IsString(); break;
                default: OP_ASSERT(FALSE); matched = FALSE;
                }

                if (matched)
                {
                    export_conversion = *format_before;
                    break;
                }
            }
            else
            {
                if (!FormatValue(context, format_before, source))
                    return FALSE;

                export_conversion = *format_before;
                break;
            }
        }
    }
    while (*format == '|');

    while (*format == '|')
    {
        ++format;
        ES_SkipFormatSpecifier(format);
    }

    OP_ASSERT(*format == ')');

    ++format;
    return TRUE;
}

/* static */ BOOL
ES_Host_Function::FormatValue(ES_Execution_Context *context, const char *&format, ES_Value_Internal &source, ES_Value *destination, ES_ValueString *destination_string)
{
    BOOL object_optional = *format == '?';

    if (object_optional)
    {
        ++format;

        if (source.IsNullOrUndefined())
        {
            ES_SkipFormatSpecifier(format);
            if (destination)
                if (source.IsNull())
                    destination->type = VALUE_NULL;
                else
                    destination->type = VALUE_UNDEFINED;
            return TRUE;
        }
    }

    if (*format == '{' || *format == '[')
    {
        if (!source.IsObject())
        {
            context->ThrowTypeError("object argument expected");
            return FALSE;
        }

        BOOL success;

        if (*format == '{')
            success = FormatDictionary(context, format, source);
        else
            success = FormatArray(context, format, source);

        if (!success)
            return FALSE;

        if (destination)
            source.ExportL(context, *destination, '-');
    }
    else if (*format == '(')
    {
        char export_conversion;

        if (!FormatAlternatives(context, format, source, export_conversion))
            return FALSE;

        if (destination)
            source.ExportL(context, *destination, export_conversion, destination_string);
    }
    else
    {
        if (*format == 'p' || *format == 'P')
        {
            if (source.IsObject())
            {
                ES_Value_Internal::HintType hint;

                if (*format == 'p')
                    hint = ES_Value_Internal::HintNone;
                else
                {
                    ++format;

                    OP_ASSERT(*format == 's' || *format == 'n');

                    if (*format == 's')
                        hint = ES_Value_Internal::HintString;
                    else
                        hint = ES_Value_Internal::HintNumber;
                }

                if (!source.ToPrimitive(context, hint))
                    return FALSE;
            }

        }
        else
        {
            if (!source.ConvertL(context, *format))
                return FALSE;
        }

        if (destination)
            source.ExportL(context, *destination, *format, destination_string);

        ++format;
    }

    return TRUE;
}

/* static */ BOOL
ES_Host_Function::FormatArguments(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *in_argv, ES_Value *out_argv, ES_ValueString *out_argv_strings, const char *format)
{
    if (!format || !*format)
        format = "-";

    for (unsigned argi = 0; argi < argc; ++argi)
    {
        const char *format_before = format;

        if (!FormatValue(context, format, in_argv[argi], &out_argv[argi], &out_argv_strings[argi]))
            return FALSE;

        if (!*format)
            format = format_before;
    }

    return TRUE;
}

/* static */ BOOL
ES_Host_Function::FinishL(ES_Execution_Context *context, unsigned result, const ES_Value &external_value, ES_Value_Internal *return_value, BOOL constructing)
{
    OP_ASSERT((result & ES_RESTART) ^ ES_RESTART);
    OP_ASSERT((result & ES_TOSTRING) == 0); // ES_TOSTRING no longer supported

    if (result & ES_VALUE)
    {
        return_value->ImportUnlockedL(context, external_value);
        if (result == ES_VALUE)
            return TRUE;
    }
    else if (result == 0)
    {
        if (constructing)
        {
            ES_CollectorLock gclock(context);
            context->ThrowTypeError("Object is not a constructor.");
            return FALSE;
        }
        else
        {
            return_value->SetUndefined();
            return TRUE;
        }
    }

    if (result & ES_SUSPEND)
        context->YieldExecution();

    ES_CollectorLock gclock(context);

    if (result & ES_EXCEPTION)
    {
        context->ThrowValue(external_value);
        return FALSE;
    }

    if (result & ES_VALUE)
        return TRUE;

    if (result & ES_ABORT)
    {
#ifndef SELFTEST
        OP_ASSERT(!"Should not happen normally, but not technically an error");
#endif
        return TRUE;
    }

    if (result & ES_NO_MEMORY)
    {
        context->AbortOutOfMemory();
        return FALSE;
    }

    if (result & ES_EXCEPT_SECURITY)
    {
        // FIXME: Throw the correct host function security error
        context->ThrowReferenceError("Security violation");
        return FALSE;
    }

    // Return undefined.  Must always return something.
    return_value->SetUndefined();
    return TRUE;
}
