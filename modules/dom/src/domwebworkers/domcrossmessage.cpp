/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domevents/domeventlistener.h"

#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domwebworkers/domwebworkers.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"
#include "modules/dom/src/domwebworkers/domwweventqueue.h"
#endif // DOM_WEBWORKERS_SUPPORT

#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domprivatenames.h"

/*-------------------------------------------------------------------------------------- */

/* ++++: DOM_MessageChannel */

/* virtual */
DOM_MessageChannel::~DOM_MessageChannel()
{
    port1 = NULL;
    port2 = NULL;
}

/* static */ OP_STATUS
DOM_MessageChannel::Make(DOM_MessageChannel *&chan, DOM_EnvironmentImpl* environment)
{
    OP_ASSERT(environment);
    DOM_Runtime *runtime = environment->GetDOMRuntime();

    chan = OP_NEW(DOM_MessageChannel, ());
    RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(chan, runtime, runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGECHANNEL_PROTOTYPE), "MessageChannel"));
    RETURN_IF_ERROR(DOM_MessagePort::Make(chan->port1, runtime));
    RETURN_IF_ERROR(DOM_MessagePort::Make(chan->port2, runtime));
    RETURN_IF_ERROR(chan->port1->Entangle(chan->port2));
    return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_MessageChannel::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_port1:
        DOMSetObject(value, port1);
        return GET_SUCCESS;
    case OP_ATOM_port2:
        DOMSetObject(value, port2);
        return GET_SUCCESS;
    default:
        return DOM_Object::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_MessageChannel::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_port1:
    case OP_ATOM_port2:
        return PUT_SUCCESS;
    default:
        return DOM_Object::PutName(property_name, value, origining_runtime);
    }
}

/* virtual */ int
DOM_MessageChannel_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
    DOM_MessageChannel *chan;
    CALL_FAILED_IF_ERROR(DOM_MessageChannel::Make(chan, GetEnvironment()));
    DOMSetObject(return_value, chan);
    return ES_VALUE;
}

/* virtual */ void
DOM_MessageChannel::GCTrace()
{
    if (port1)
        GetRuntime()->GCMark(port1);
    if (port2)
        GetRuntime()->GCMark(port2);
}

/*-------------------------------------------------------------------------------------- */

/* ++++: DOM_MessagePort */

/* virtual */
DOM_MessagePort::~DOM_MessagePort()
{
    Out();
    if (target)
    {
        /* Let go of strong reference to target..if in a sep. runtime */
        if (!GetRuntime()->IsSameHeap(target->GetRuntime()))
            target->DropStrongReference();
        target->target = NULL;
    }
    target = NULL;
}

/* static */ OP_STATUS
DOM_MessagePort::Make(DOM_MessagePort *&port, DOM_Runtime *runtime)
{
    RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime((port = OP_NEW(DOM_MessagePort, ())), runtime, runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEPORT_PROTOTYPE), "MessagePort"));
    return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_MessagePort::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_onmessage:
        DOMSetObject(value, message_handler);
        return GET_SUCCESS;

    default:
        return DOM_Object::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_MessagePort::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch (property_name)
    {
    case OP_ATOM_onmessage:
        PUT_FAILED_IF_ERROR(DOM_Object::CreateEventTarget());
        return DOM_CrossMessage_Utils::PutEventHandler(this, UNI_L("message"), ONMESSAGE, message_handler, &message_event_queue, value, origining_runtime);
    default:
        return DOM_Object::PutName(property_name, value, origining_runtime);
    }
}

void
DOM_MessagePort::Disentangle()
{
    if (!IsEntangled())
        return;

    DOM_MessagePort *t = target;
    target = NULL;

    if (!GetRuntime()->IsSameHeap(t->GetRuntime()))
    {
        t->DropStrongReference();
        DropStrongReference();
    }

    t->target = NULL;
    t->forwarding_port = NULL;
}

OP_STATUS
DOM_MessagePort::Entangle(DOM_MessagePort *new_target)
{
    OP_ASSERT(new_target);

    if (target == new_target)
        return OpStatus::OK;

    if (IsEntangled())
        Disentangle();

    if (new_target->IsEntangled())
        new_target->Disentangle();

    /* If a foreign target port, add a strong reference to it. */
    if (!GetRuntime()->IsSameHeap(new_target->GetRuntime()))
    {
        RETURN_IF_ERROR(new_target->AddStrongReference());
        RETURN_IF_ERROR(AddStrongReference());
    }

    is_enabled = new_target->is_enabled = TRUE;
    target = new_target;
    new_target->target = this;

    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_MessagePort::PrepareTransfer(DOM_MessagePort *&new_port, DOM_EnvironmentImpl *target_environment)
{
    RETURN_IF_ERROR(DOM_MessagePort::Make(new_port, target_environment->GetDOMRuntime()));
    return OpStatus::OK;
}

OP_STATUS
DOM_MessagePort::Transfer(DOM_MessagePort *new_port, DOM_EnvironmentImpl *target_environment, DOM_WebWorkerBase *new_owner /* = NULL */)
{
    /* Clone 'this' port into the target environment and take over its
       queued-up events.

       Note:
        - events that have been queued for an enabled port but not yet processed,
          will _not_ be removed from the DOM event queue (Q: possible?)
        - may want to address this at some stage, perhaps by leaving behind a
          cloned-to forwarding pointer.

       This is an atomic operation -- copying queued events from existing port to new,
       followed by hijacking its entangled port. */

    /* Atomic begin */
    is_enabled = FALSE;
    ES_Value undefined;
    RETURN_IF_ERROR(GetRuntime()->PutPrivate(*this, DOM_PRIVATE_neutered, undefined));

    RETURN_IF_ERROR(message_event_queue.CopyEventQueue(&new_port->message_event_queue, new_port));
#ifdef DOM_WEBWORKERS_SUPPORT
    if (new_owner)
        new_owner->AddEntangledPort(new_port);
    else
#endif // DOM_WEBWORKERS_SUPPORT
        /* In this multi heap world we need to keep track of ports so that we can
         * properly disentangle and let go of them at shut down..
         */
        new_port->GetEnvironment()->AddMessagePort(new_port);

    if (IsEntangled())
    {
        /* Note: dropping our target, so 'this' port is now effectively disentangled. */
        DOM_MessagePort *forwarded_target = target;
        if (!GetRuntime()->IsSameHeap(target->GetRuntime()))
            target->DropStrongReference();

        target = NULL;
        OP_STATUS status = new_port->Entangle(forwarded_target);
        if (OpStatus::IsError(status))
        {
            /* If we failed, back out. */
            target = forwarded_target;
            if (!GetRuntime()->IsSameHeap(target->GetRuntime()))
                RETURN_IF_ERROR(target->AddStrongReference());
        }

        return status;
    }
    /* Atomic end */
    return OpStatus::OK;
}

OP_BOOLEAN
DOM_MessagePort::IsCloned()
{
    ES_Value dummy;
    return GetRuntime()->GetPrivate(*this, DOM_PRIVATE_neutered, &dummy);
}

/* virtual */ void
DOM_MessagePort::GCTrace()
{
    if (target && GetRuntime()->IsSameHeap(target->GetRuntime()))
        GetRuntime()->GCMark(target);

    OP_ASSERT(!forwarding_port || GetRuntime()->IsSameHeap(forwarding_port->GetRuntime()));
    GCMark(forwarding_port);

    message_event_queue.GCTrace(GetRuntime());
    GCMark(FetchEventTarget());
}

OP_STATUS
DOM_MessagePort::AddStrongReference()
{
    if (keep_alive_id == 0)
    {
        /* Where to add the strong reference? The probing done previously for either env->window
           or env->worker, and use it as the keep-alive provider does not carry over to a shared
           DOM setting (which is how page-side UserJS extension code is packaged to run, for instance.)
           The precise thing is to use the global object for the message port's runtime, and put the
           keep alive there.

           ES_Runtime::Protect()? It provides a stronger guarantee than wanted, i.e., want lifetime equal
           to that of the runtime's, and not heap's (which is what Protect() does.) Hence, somewhat
           hackily for now, use privates on the global object to provide the keep-alive behaviour wanted.

           If found to work out, should be tidied up. */

        keep_alive_id = reinterpret_cast<UINTPTR>(this);
        ES_Value value;
        DOMSetObject(&value, this);
        return GetRuntime()->PutPrivate(GetRuntime()->GetGlobalObjectAsPlainObject(), keep_alive_id, value);
    }

    return OpStatus::OK;
}

void
DOM_MessagePort::DropStrongReference()
{
    if (keep_alive_id != 0)
    {
        keep_alive_id = reinterpret_cast<UINTPTR>(this);
        /* Not ideal, ignoring OOM. */
        OpStatus::Ignore(GetRuntime()->DeletePrivate(GetRuntime()->GetGlobalObjectAsPlainObject(), keep_alive_id));
        keep_alive_id = 0;
   }
}

/** Performing 'postMessage' on ports just unravels the given arguments,
    creates the message event and forwards the event. */

/* static */ int
DOM_MessagePort::postMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef EXTENSION_SUPPORT
    /* The cross-document messaging spec does not define .source as pointing to the origining port, but
     * it used throughout for Opera Extensions messaging, providing convenient back-reference to origin.
     *
     * Undecided if it merits introducing two kinds of ports (and MessageChannels) to distinguish.
     */
    BOOL add_source_property = TRUE;
#else
    BOOL add_source_property = FALSE;
#endif // EXTENSION_SUPPORT

    DOM_THIS_OBJECT(port, DOM_TYPE_MESSAGEPORT, DOM_MessagePort);
    DOM_CHECK_ARGUMENTS("-|O");

    if (!port || !port->IsEnabled() || !port->IsEntangled())
        return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

    DOM_MessagePort *target_port = port->GetTarget();

    /* Construct a message event to be delivered into the target's context;
       use the port's origin and, optionally, it as a .source property. */
    DOM_MessageEvent *message_event = NULL;
    URL origin_url = port->GetEnvironment()->GetDOMRuntime()->GetOriginURL();
    int result = DOM_MessageEvent::Create(message_event, port, target_port->GetRuntime(), port, target_port, origin_url, argv, argc, return_value, add_source_property);
    if (result != ES_FAILED)
        return result;

    if ( !message_event || !target_port->IsEntangled() || !target_port->IsEnabled() || !port->IsEntangled() || !port->IsEnabled())
        return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

    /* Check if we are entangled with sender for a "message" */
    if (target_port->target != port)
        return DOM_CALL_DOMEXCEPTION(INVALID_ACCESS_ERR);
    if (message_event->GetKnownType() != ONMESSAGE)
        return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

    /* Have a valid port and a message; time to send. */
    message_event->SetTarget(target_port);
    message_event->SetSynthetic();

    /* Deliver..or if not yet in an enabled state, queue the event. */
    CALL_FAILED_IF_ERROR(DOM_MessagePort::SendMessage(target_port, message_event));
    return ES_FAILED;
}

/* static */ OP_STATUS
DOM_MessagePort::SendMessage(DOM_MessagePort *port, DOM_MessageEvent *message_event)
{
    if (port->forwarding_port)
    {
        message_event->SetTarget(port->forwarding_port);
        return port->forwarding_port->message_event_queue.DeliverEvent(message_event, port->GetEnvironment());
    }
    else
        return port->message_event_queue.DeliverEvent(message_event, port->GetEnvironment());
}


/* static */ int
DOM_MessagePort::start(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
    DOM_THIS_OBJECT(port, DOM_TYPE_MESSAGEPORT, DOM_MessagePort);
    if (!port)
        return ES_FAILED;

    port->is_enabled = TRUE;
    /* If any messages are queued waiting, unstick them now. */
    if (OpStatus::IsMemoryError(port->message_event_queue.DrainEventQueue(port->GetEnvironment())))
        return ES_NO_MEMORY;
    return ES_FAILED;
}

/* static */ int
DOM_MessagePort::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
    DOM_THIS_OBJECT(port, DOM_TYPE_MESSAGEPORT, DOM_MessagePort);

    if (!port || !port->is_enabled)
        return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

    port->Disentangle();
    port->Out();
    port->forwarding_port = NULL;

    /* Disable regardless of outcome of previous action. */
    port->is_enabled = FALSE;
    return ES_FAILED;
}

/*-------------------------------------------------------------------------------------- */

/* ++++: DOM_MessageEvent */

/**
  Perform transferables validation. Only returns a binary yes/no; extending it
  to report exactly what failed could be valuable.
 */
/* static */ OP_STATUS
DOM_MessageEvent::ValidateTransferables(ES_Object *transferables, DOM_MessagePort *source, DOM_MessagePort *target, unsigned &error_index, DOM_EnvironmentImpl *environment)
{
    if (!transferables)
        return OpStatus::OK;

    ES_Runtime *runtime = environment->GetRuntime();
    /* Section 8.2.4, bullet 4 check: Failure if not an array..here in the wider sense of having a .length property. */
    ES_Value length_value;
    RETURN_IF_ERROR(runtime->GetName(transferables, UNI_L("length"), &length_value));

    if (length_value.type != VALUE_NUMBER || !op_isfinite(length_value.value.number) || length_value.value.number < 0 || length_value.value.number > MAX_LENGTH_PORTSARRAY)
        return OpStatus::ERR;

    unsigned length = TruncateDoubleToUInt(length_value.value.number);

    ES_Value transfer_value;
    for (unsigned i = 0; i < length; i++ )
    {
        error_index = i;
        RETURN_IF_ERROR(runtime->GetIndex(transferables, i, &transfer_value));

        if (transfer_value.type == VALUE_UNDEFINED)
            continue;

        /* null values cannot appear, but pretty much everything else can per
           spec (non-object values are ignored.) */
        if (transfer_value.type == VALUE_NULL)
            return OpStatus::ERR;
        if (transfer_value.type == VALUE_OBJECT)
        {
            ES_Object *transfer_object = transfer_value.value.object;
            DOM_Object *host_object = DOM_HOSTOBJECT(transfer_object, DOM_Object);
            if (host_object && host_object->IsA(DOM_TYPE_MESSAGEPORT))
            {
                DOM_MessagePort *port = static_cast<DOM_MessagePort *>(host_object);
                OP_BOOLEAN result = port->IsCloned();
                RETURN_IF_ERROR(result);
                if (result == OpBoolean::IS_TRUE)
                    return OpStatus::ERR;

                /* Transmitting either party of a postMessage() over ports isn't permitted */
                if (source == port || target == port)
                    return OpStatus::ERR;

                for (unsigned j = 0; j < i; j++)
                {
                    ES_Value inner_value;
                    RETURN_IF_ERROR(runtime->GetIndex(transferables, j, &inner_value));
                    if (inner_value.type == VALUE_OBJECT)
                    {
                        DOM_Object *inner_host_object = DOM_HOSTOBJECT(inner_value.value.object, DOM_Object);
                        if (host_object == inner_host_object)
                            return OpStatus::ERR;
                    }
                }
            }
            else if (ES_Runtime::IsNativeArrayBufferObject(transfer_object))
            {
                OP_BOOLEAN result = runtime->GetPrivate(transfer_object, DOM_PRIVATE_neutered, &transfer_value);
                RETURN_IF_ERROR(result);
                /* A generic error is flagged if arrays have already been
                   transferred (neutered is the term used by the spec.) */
                if (result == OpBoolean::IS_TRUE)
                    return OpStatus::ERR;
                for (unsigned j = 0; j < i; j++)
                {
                    ES_Value inner_value;
                    RETURN_IF_ERROR(runtime->GetIndex(transferables, j, &inner_value));
                    if (inner_value.type == VALUE_OBJECT && transfer_object == inner_value.value.object)
                        return OpStatus::ERR;
                }
            }
        }
    }
    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_MessageEvent::CloneTransferables(ES_Object *transferables, ES_Runtime::CloneTransferMap &transfer_map, ES_Value &target, DOM_EnvironmentImpl *target_environment, BOOL no_transferables_to_null, DOM_WebWorkerBase *new_owner)
{
    DOM_Object::DOMSetNull(&target);
    /* The common case, no ports/transferables passed to postMessage();
       map it to null or an empty ports array? */
    if (!transferables && no_transferables_to_null)
        return OpStatus::OK;

    ES_Runtime *target_runtime = target_environment->GetRuntime();

    ES_Value length_value;
    unsigned transferables_length;
    if (transferables)
    {
        RETURN_IF_ERROR(target_runtime->GetName(transferables, UNI_L("length"), &length_value));
        if (length_value.type != VALUE_NUMBER || !op_isfinite(length_value.value.number) || length_value.value.number < 0 || length_value.value.number > MAX_LENGTH_PORTSARRAY)
            return OpStatus::ERR;
        transferables_length = TruncateDoubleToUInt(length_value.value.number);
    }
    else
        transferables_length = 0;

    ES_Object *ports_array = NULL;
    RETURN_IF_ERROR(target_runtime->CreateNativeArrayObject(&ports_array, 0));
    if (!transferables)
    {
        DOMSetObject(&target, ports_array);
        return OpStatus::OK;
    }

    ES_Value transfer_value;
    unsigned ports_array_length = 0;
    for (unsigned i = 0; i < transferables_length; i++)
    {
        RETURN_IF_ERROR(target_runtime->GetIndex(transferables, i, &transfer_value));
        if (transfer_value.type == VALUE_OBJECT)
        {
            ES_Object *transfer_object = transfer_value.value.object;
            DOM_Object *host_object = DOM_HOSTOBJECT(transfer_object, DOM_Object);
            if (host_object && host_object->IsA(DOM_TYPE_MESSAGEPORT))
            {
                DOM_MessagePort *transferred_port = NULL;

                RETURN_IF_ERROR(DOM_MessagePort::PrepareTransfer(transferred_port, target_environment));
                DOMSetObject(&transfer_value, transferred_port);
                RETURN_IF_ERROR(target_runtime->PutIndex(ports_array, ports_array_length, transfer_value));
                ports_array_length++;
                RETURN_IF_ERROR(transfer_map.source_objects.Add(transfer_object));
                RETURN_IF_ERROR(transfer_map.target_objects.Add(*transferred_port));
            }
            else if (ES_Runtime::IsNativeArrayBufferObject(transfer_object))
            {
                BOOL present = FALSE;
                for (unsigned i = 0; !present && i < transfer_map.source_objects.GetCount(); i++)
                    if (transfer_object == transfer_map.source_objects.Get(i))
                        present = TRUE;
                if (present)
                    continue;

                ES_Object *transferable = NULL;
                RETURN_IF_ERROR(target_runtime->CreateNativeArrayBufferObject(&transferable, 0));
                RETURN_IF_ERROR(transfer_map.source_objects.Add(transfer_object));
                RETURN_IF_ERROR(transfer_map.target_objects.Add(transferable));
            }
        }
    }
    DOMSetNumber(&length_value, ports_array_length);
    RETURN_IF_ERROR(target_runtime->PutName(ports_array, UNI_L("length"), length_value));
    DOMSetObject(&target, ports_array);
    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_MessageEvent::CloneData(DOM_Object *this_object, ES_Runtime::CloneTransferMap *transfer_map, ES_Value &target_value, const ES_Value &source_value, const uni_char *location, ES_Value *return_value, DOM_EnvironmentImpl *target_environment, DOM_WebWorkerBase *new_owner /* = NULL */)
{
    switch (source_value.type)
    {
    case VALUE_NULL:
    case VALUE_UNDEFINED:
    case VALUE_NUMBER:
    case VALUE_BOOLEAN:
    case VALUE_STRING:
    case VALUE_STRING_WITH_LENGTH:
        RETURN_IF_ERROR(DOMCopyValue(target_value, source_value));
        break;
    case VALUE_OBJECT:
    {
        ES_Runtime::CloneStatus clone_status;
        ES_Object *clone;
        ES_Runtime *target_runtime = target_environment->GetRuntime();
        ES_Runtime *runtime = this_object->GetRuntime();

        OP_STATUS status = runtime->StructuredClone(source_value.value.object, target_runtime, clone, transfer_map, &clone_status);

        if (OpStatus::IsMemoryError(status))
            return status;
        else if (OpStatus::IsError(status))
        {
            /* Cloning errors are raised as DATA_CLONE_ERR; attach some
               more helpful information to the DOMException by pinning
               on extra properties. */
            this_object->CallDOMException(DOM_Object::DATA_CLONE_ERR, return_value); // ignoring the ES_EXCEPTION return value.
            OP_ASSERT(return_value->type == VALUE_OBJECT);
            if (return_value->type == VALUE_OBJECT)
            {
                const uni_char *error_description = NULL;
                switch (clone_status.fault_reason)
                {
                default:
                    OP_ASSERT(!"Unhandled clone status; cannot happen.");
                    /* fallthrough */
                case ES_Runtime::CloneStatus::OBJ_IS_FOREIGN:
                    error_description = UNI_L("Unable to clone host objects.");
                    break;
                case ES_Runtime::CloneStatus::OBJ_IS_FUNCTION:
                    error_description = UNI_L("Unable to clone function values.");
                    break;
                case ES_Runtime::CloneStatus::OBJ_HAS_GETTER_SETTER:
                    error_description = UNI_L("Unable to getter and setter properties.");
                    break;
                }
                ES_Value prop_value;
                DOMSetString(&prop_value, error_description);
                RETURN_IF_MEMORY_ERROR(runtime->PutName(return_value->value.object, UNI_L("description"), prop_value));
                DOMSetString(&prop_value, location);
                RETURN_IF_MEMORY_ERROR(runtime->PutName(return_value->value.object, UNI_L("location"), prop_value));
            }
            return status;
        }

        DOMSetObject(&target_value, clone);
        break;
    }
    default:
        OP_ASSERT(0);
        return OpStatus::ERR;
    }

    if (transfer_map)
    {
        ES_Value undefined;
        ES_Runtime *runtime = this_object->GetRuntime();
        for (unsigned i = 0; i < transfer_map->source_objects.GetCount(); i++)
        {
            ES_Object *source = transfer_map->source_objects.Get(i);
            ES_Object *target = transfer_map->target_objects.Get(i);
            DOM_Object *host_object = DOM_HOSTOBJECT(source, DOM_Object);
            if (host_object && host_object->IsA(DOM_TYPE_MESSAGEPORT))
            {
                DOM_MessagePort *port = static_cast<DOM_MessagePort *>(host_object);
                DOM_Object *target_port = DOM_HOSTOBJECT(target, DOM_Object);
                OP_ASSERT(target_port && target_port->IsA(DOM_TYPE_MESSAGEPORT));
                RETURN_IF_ERROR(port->Transfer(static_cast<DOM_MessagePort *>(target_port), target_environment, new_owner));
            }
            else if (ES_Runtime::IsNativeArrayBufferObject(source))
            {
                ES_Runtime::TransferArrayBuffer(source, target);
                RETURN_IF_ERROR(runtime->PutPrivate(source, DOM_PRIVATE_neutered, undefined));
            }
        }
    }
    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_MessageEvent::Make(DOM_MessageEvent *&message_event, DOM_Runtime *runtime)
{
    message_event = OP_NEW(DOM_MessageEvent, ());
    RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(message_event, runtime, runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE), "MessageEvent"));

    ES_Value argv[3];
    DOMSetString(&argv[0], UNI_L("message"));
    DOMSetBoolean(&argv[1], FALSE); /* no bubbling */
    DOMSetBoolean(&argv[2], FALSE); /* not cancelable */

    ES_Value unused;
    initEvent(message_event, argv, ARRAY_SIZE(argv), &unused, runtime);

    return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_MessageEvent::Make(DOM_MessageEvent *&message_event, DOM_Object *this_object, DOM_MessagePort *source_port, DOM_MessagePort *target_port, DOM_EnvironmentImpl *environment, const URL &url, ES_Value *argv, int argc, ES_Value *return_value, DOM_WebWorkerBase *target_worker)
{
    OP_ASSERT(message_event);
    if (argc < 5)
        return OpStatus::ERR;

    OP_STATUS status;

    /* Fill in the fields from the incoming arguments */
    if (argv[1].type == VALUE_STRING)
        RETURN_IF_ERROR(message_event->SetOrigin(argv[1].value.string));
    if (argv[2].type == VALUE_STRING)
        RETURN_IF_ERROR(message_event->SetLastEventId(argv[2].value.string));
    if (argv[3].type == VALUE_OBJECT && argv[3].value.object)
        message_event->SetSource(DOM_GetHostObject(argv[3].value.object));

    ES_Object *transferables = NULL;
    if (argv[4].type == VALUE_OBJECT)
    {
        transferables = argv[4].value.object;

        unsigned error_index = 0;
        if (OpStatus::IsError(status = DOM_MessageEvent::ValidateTransferables(transferables, source_port, target_port, error_index, environment)))
        {
            DOM_CALL_DOMEXCEPTION(DATA_CLONE_ERR);
            return status;
        }
    }

    BOOL ports_to_null = TRUE;
    if (argc > 5 && argv[5].type == VALUE_BOOLEAN)
        ports_to_null = argv[5].value.boolean;
    ES_Value clone_value;
    ES_Runtime::CloneTransferMap transfer_map;
    if (OpStatus::IsError(status = DOM_MessageEvent::CloneTransferables(transferables, transfer_map, clone_value, environment, ports_to_null, target_worker)))
    {
        DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
        return status;
    }
    if (clone_value.type == VALUE_OBJECT)
        message_event->SetPorts(clone_value.value.object);

    OpString url_str;
    RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment, url_str));
    return DOM_MessageEvent::CloneData(this_object, &transfer_map, message_event->data, argv[0], url_str.CStr(), return_value, environment, target_worker);
}

DOM_MessageEvent::~DOM_MessageEvent()
{
    DOM_Object::DOMFreeValue(data);
}

/* virtual */ OP_STATUS
DOM_MessageEvent::DefaultAction(BOOL cancelled)
{
    /* Message events have no default action. Hereby provided! :-) */
    return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_MessageEvent::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_data:
        switch (data.type)
        {
        case VALUE_STRING:    DOMSetString(value, data.value.string, data.string_length); break;
        case VALUE_UNDEFINED: DOMSetUndefined(value); break;
        case VALUE_NULL:      DOMSetNull(value); break;
        case VALUE_BOOLEAN:   DOMSetBoolean(value, data.value.boolean); break;
        case VALUE_NUMBER:    DOMSetNumber(value, data.value.number); break;
        case VALUE_OBJECT:    DOMSetObject(value, data.value.object); break;
        case VALUE_STRING_WITH_LENGTH: DOMSetStringWithLength(value, &g_DOM_globalData->string_with_length_holder, data.value.string_with_length->string, data.value.string_with_length->length);
        default:
            break;
        }
        return GET_SUCCESS;
    case OP_ATOM_origin:
        DOMSetString(value, origin.CStr());
        return GET_SUCCESS;
    case OP_ATOM_lastEventId:
        DOMSetString(value, lastEventId.CStr());
        return GET_SUCCESS;
    case OP_ATOM_source:
        DOMSetObject(value, source);
        return GET_SUCCESS;
    case OP_ATOM_ports:
        DOMSetObject(value, ports);
        return GET_SUCCESS;
    default:
        return DOM_Event::GetName(property_name, value, origining_runtime);
    }
}

/* virtual */ ES_PutState
DOM_MessageEvent::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
    switch(property_name)
    {
    case OP_ATOM_data:
    case OP_ATOM_origin:
    case OP_ATOM_lastEventId:
    case OP_ATOM_source:
    case OP_ATOM_ports:
        return PUT_SUCCESS;
    default:
        return DOM_Event::PutName(property_name, value, origining_runtime);
    }
}

/* virtual */ void
DOM_MessageEvent::GCTrace()
{
    GCMark(data);
    GCMark(ports);
    GCMark(source);
    DOM_Event::GCTrace();
}

/*
 * The MessageEvent interface:
 *   // 2009-09-04 snapshot
 *   // http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html
 *
 *   interface MessageEvent : Event {
 *     readonly attribute any data;
 *     readonly attribute DOMString origin;
 *     readonly attribute DOMString lastEventId;
 *     readonly attribute WindowProxy source;
 *     readonly attribute MessagePortArray ports;
 *     void initMessageEvent(in DOMString typeArg,
 *                           in boolean canBubbleArg,
 *                           in boolean cancelableArg,
 *                           in any dataArg,
 *                           in DOMString originArg,
 *                           in DOMString lastEventIdArg,
 *                           in WindowProxy sourceArg,
 *                           in MessagePortArray portsArg);
 *     void initMessageEventNS(in DOMString namespaceURI,
 *                             in DOMString typeArg,
 *                             in boolean canBubbleArg,
 *                             in boolean cancelableArg,
 *                             in any dataArg,
 *                             in DOMString originArg,
 *                             in DOMString lastEventIdArg,
 *                             in WindowProxy sourceArg,
 *                             in MessagePortArray portsArg);
 *   }
 */

/* static */ int
DOM_MessageEvent::initMessageEvent(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
    DOM_THIS_OBJECT(message_event, DOM_TYPE_MESSAGEEVENT, DOM_MessageEvent);
    return DOM_MessageEvent::initMessageEventPostMessage(message_event, this_object, NULL, NULL, argv, argc, return_value, origining_runtime);
}

/* static */ int
DOM_MessageEvent::initMessageEventPostMessage(DOM_MessageEvent *message_event, DOM_Object *this_obj, DOM_MessagePort *source_port, DOM_MessagePort *target_port, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
    DOM_CHECK_ARGUMENTS("sbb-ssOO");

    int result = initEvent(message_event, argv, argc, return_value, origining_runtime);

    if (result != ES_FAILED)
        return result;

    message_event->SetSynthetic();

    URL url;

    DOM_WebWorkerBase *target_worker = NULL;
#ifdef DOM_WEBWORKERS_SUPPORT
    if (this_obj && this_obj->IsA(DOM_TYPE_WEBWORKERS_WORKER))
    {
        DOM_WebWorker *worker = static_cast<DOM_WebWorker *>(this_obj);
        url = worker->GetLocationURL();
        target_worker = worker->GetDedicatedWorker();
    }
    if (this_obj && this_obj->IsA(DOM_TYPE_WEBWORKERS_WORKER_OBJECT))
    {
        DOM_WebWorkerObject *worker = static_cast<DOM_WebWorkerObject *>(this_obj);
        if (worker)
            url = worker->GetLocationURL();
        else
            url = this_obj->GetRuntime()->GetOriginURL();
        target_worker = worker->GetWorker();
    }
    else if (this_obj)
#else
    if (this_obj)
#endif // DOM_WEBWORKERS_SUPPORT
        url = this_obj->GetRuntime()->GetOriginURL();

    /* Note: we need to flag a harder ES_EXCEPTION here on failure to have the DOM exception we've set via return_value propagate correctly along with return code */
    OP_STATUS status = DOM_MessageEvent::Make(message_event, this_obj, source_port, target_port, origining_runtime->GetEnvironment(), url, &argv[3], argc - 3, return_value, target_worker/*new worker owner*/);
    if (OpStatus::IsError(status))
        return (OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : ES_EXCEPTION);

    DOMSetObject(return_value, message_event);
    return ES_FAILED;
}

OP_STATUS
DOM_MessageEvent::SetOrigin(const uni_char *s)
{
    RETURN_IF_ERROR(origin.Set(s));
    return OpStatus::OK;
}

OP_STATUS
DOM_MessageEvent::SetOrigin(const URL &url)
{
    RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment, origin));
    return OpStatus::OK;
}

OP_STATUS
DOM_MessageEvent::SetLastEventId(const uni_char *s)
{
    RETURN_IF_ERROR(lastEventId.Set(s));
    return OpStatus::OK;
}

OP_STATUS
DOM_MessageEvent::SetData(ES_Value &value)
{
    /* In case of string content, taking ownership. */
    DOM_Object::DOMFreeValue(data);
    return DOM_Object::DOMCopyValue(data, value);
}

/* static */ int
DOM_MessageEvent::Create(DOM_MessageEvent *&message_event, DOM_Object *this_object, DOM_Runtime *runtime, DOM_MessagePort *source_port, DOM_MessagePort *target_port, URL &origin_url, ES_Value *argv, int argc, ES_Value *return_value, BOOL add_source_property /* = FALSE*/)
{
    message_event = OP_NEW(DOM_MessageEvent, ());
    CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(message_event, runtime, runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE), "MessageEvent"));

    ES_Value arguments[8];
    OpString url_str;
    CALL_FAILED_IF_ERROR(origin_url.GetAttribute(URL::KUniName_With_Fragment, url_str));

    /* Filling in that (big) slab of arguments:
     *
     *   event_name    (s)
     *   bubble flag   (b)
     *   cancel flag   (b)
     *   message val   (O)
     *   origin        (s)
     *   lastEventId   (s)   (server sent events only)
     *   source        (O)   (always null for MessagePort traffic.)
     *   ports_array   (A)   (optional array of port values.)
     *
     */
    DOMSetString(&arguments[0], UNI_L("message"));
    DOMSetBoolean(&arguments[1], FALSE); /* no bubbling */
    DOMSetBoolean(&arguments[2], FALSE); /* not cancelable */
    // NOTE: a shallow copy, do not want to copy string (if one.)
    arguments[3] = argv[0];

    DOMSetString(&arguments[4], url_str.CStr());
    DOMSetString(&arguments[5], UNI_L(""));

    /* source field is by default not supplied, but can be overridden. */
    if (!add_source_property)
        DOMSetNull(&arguments[6]);
    else
    {
        /* This sets the .source on the event, which is reference to the port that can be used when
         * the receiver wants to respond. _It's_ target would then be source_port.
         */
        OP_ASSERT(!target_port || runtime == target_port->GetRuntime());
        DOMSetObject(&arguments[6], target_port);
    }

    // the ports array.
    if (argc == 1)
        DOM_Object::DOMSetUndefined(&arguments[7]);
    else
        CALL_FAILED_IF_ERROR(DOM_Object::DOMCopyValue(arguments[7], argv[1]));

    return DOM_MessageEvent::initMessageEventPostMessage(message_event, this_object ? this_object : message_event, source_port, target_port, arguments, ARRAY_SIZE(arguments), return_value, message_event->GetRuntime());
}

/* static */ OP_STATUS
DOM_MessageEvent::MakeConnect(DOM_MessageEvent *&connect_event, BOOL is_connect, const URL &url_target, DOM_Object *target, DOM_MessagePort *port_out, BOOL add_ports_alias /* = TRUE */, DOM_WebWorkerBase *target_worker /* = NULL */)
{
    DOM_EnvironmentImpl *target_environment = target->GetEnvironment();
    DOM_Runtime *target_runtime = target_environment->GetDOMRuntime();

    /* Create a port within the context of the target..and entangle it with source port at our end. */
    DOM_MessagePort *proxy_port_at_target = NULL;
    if (port_out)
        if (port_out->IsEntangled())
            proxy_port_at_target = port_out->GetTarget();
        else
        {
            RETURN_IF_ERROR(DOM_MessagePort::Make(proxy_port_at_target, target_runtime));
            RETURN_IF_ERROR(port_out->Entangle(proxy_port_at_target));
        }

    DOM_MessageEvent *message_event = OP_NEW(DOM_MessageEvent, ());
    RETURN_IF_ERROR(DOMSetObjectRuntime(message_event, target_runtime, target_runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE), "MessageEvent"));

    ES_Value argv[5];

#ifdef EXTENSION_SUPPORT
    DOMSetString(&argv[0]);
    DOM_EventType event_type = is_connect ? ONCONNECT : ONDISCONNECT;
#else
    OP_ASSERT(is_connect);
    DOMSetString(&argv[0]);
    DOM_EventType event_type = ONCONNECT;
#endif // EXTENSION_SUPPORT

    OpString url_str;
    RETURN_IF_ERROR(url_target.GetAttribute(URL::KUniName_With_Fragment, url_str));
    DOMSetString(&argv[1], url_str.CStr());
    DOMSetString(&argv[2], UNI_L(""));
    DOMSetObject(&argv[3], proxy_port_at_target);
    DOMSetNull(&argv[4]);

    ES_Value return_value;
    /* Note: both the source and target ports are left as empty here - the communication will
     * be happening across these (as we are about to set up..) */

    RETURN_IF_ERROR(DOM_MessageEvent::Make(message_event, target, NULL/*source port*/, NULL/* no target port*/, target_environment, url_target, argv, ARRAY_SIZE(argv), &return_value, target_worker));

    if (add_ports_alias)
    {
        /* Create the ports array holding the source message port and set
           it on the message event. Shared worker connect events require this. */
        ES_Object *ports_array = NULL;
        RETURN_IF_ERROR(target_runtime->CreateNativeArrayObject(&ports_array));
        if (proxy_port_at_target)
        {
            ES_Value port_val;
            DOMSetObject(&port_val, proxy_port_at_target);
            RETURN_IF_ERROR(target_runtime->PutIndex(ports_array, 0, port_val));
        }
        message_event->SetPorts(ports_array);
    }

    message_event->InitEvent(event_type, target);
    message_event->SetCurrentTarget(target);
    message_event->SetSynthetic();
    message_event->SetEventPhase(ES_PHASE_ANY);

    connect_event = message_event;
    return OpStatus::OK;
}

/* static */ int
DOM_MessagePort::accessEventListenerStart(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
    DOM_THIS_OBJECT(port, DOM_TYPE_MESSAGEPORT, DOM_MessagePort);
    int status = DOM_Node::accessEventListener(port, argv, argc, return_value, origining_runtime, data);

    /* addEventListener() use won't "start()" the port in the context of Web Workers (per spec);
     * a source of confusion. Offer a version that does start() when ports are accessed from extensions.
     */
    if (argc >= 1 && (data == 0 || data == 2))
    {
        if (port->GetEventTarget() && port->GetEventTarget()->HasListeners(ONMESSAGE, UNI_L("message"), ES_PHASE_ANY))
            port->message_event_queue.DrainEventQueue(port->GetEnvironment());
    }
    return status;
}

/* virtual */ int
DOM_MessageEvent_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
    DOM_CHECK_ARGUMENTS("-|O");
    DOM_MessageEvent *message_event;
    DOM_Runtime *target_runtime = GetEnvironment()->GetDOMRuntime();
    URL origin_url = target_runtime->GetOriginURL();

    int result = DOM_MessageEvent::Create(message_event, NULL/*this_object*/, target_runtime, NULL/*source msg port*/, NULL/*target port*/, origin_url, argv, argc, return_value);
    if (result == ES_FAILED)
        DOM_Object::DOMSetObject(return_value, message_event);

    return result;
}

DOM_FUNCTIONS_START(DOM_MessageEvent)
    DOM_FUNCTIONS_FUNCTION(DOM_MessageEvent, DOM_MessageEvent::initMessageEvent, "initMessageEvent", "sbb-ss-")
DOM_FUNCTIONS_END(DOM_MessageEvent)

DOM_FUNCTIONS_START(DOM_MessagePort)
    DOM_FUNCTIONS_FUNCTION(DOM_MessagePort, DOM_MessagePort::start, "start", NULL)
    DOM_FUNCTIONS_FUNCTION(DOM_MessagePort, DOM_MessagePort::close, "close", NULL)
    DOM_FUNCTIONS_FUNCTION(DOM_MessagePort, DOM_MessagePort::postMessage, "postMessage", "-[-]-")
    DOM_FUNCTIONS_FUNCTION(DOM_MessagePort, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_MessagePort)

DOM_FUNCTIONS_WITH_DATA_START(DOM_MessagePort)
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_MessagePort, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
    DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_MessagePort, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_MessagePort)

#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
