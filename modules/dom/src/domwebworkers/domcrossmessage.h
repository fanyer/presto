/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_CROSSMESSAGE_H
#define DOM_CROSSMESSAGE_H

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#include "modules/dom/src/domobj.h"
#include "modules/dom/domtypes.h"
#include "modules/dom/src/domruntime.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"

#include "modules/dom/src/domwebworkers/domwweventqueue.h"

// local class declarations:
class DOM_MessagePort;
class DOM_MessageChannel;
class DOM_MessageEvent;
class DOM_WebWorkerBase;

/* To (non-principally) address the problem of attempts to clone and validate
 * payloads containing large arrays, an upper limit is imposed on the size of
 * a ports array. No reasonable application would want to pass anything
 * larger on a per-message basis.
 */
#define MAX_LENGTH_PORTSARRAY 65535

/** HTML Cross-Document Messaging
 *
 * This file defines the classes and types needed to support the cross-document API defined in
 *
 *     http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html
 *
 * Classes on offer:
 *
 *     DOM_MessageEvent:      the DOM-level representation of the events that messages can pass around,
 *                            cloneable native objects/values along with a sequence of optional message ports.
 *
 *     DOM_MessagePort:       a bi-directional port abstraction that's symmetrically attached/entangled
 *                            to another port (which isn't constrained to being with the same origin.).
 *                            Supports async postMessage() and message listener registration.
 *
 *     DOM_MessageChannel:    a port pairing helper abstraction.
 *
 */

/** DOM_MessagePort:

  Message ports implements point-to-point messaging, in the style of what HTML5
  (Chapter 8, Section 3) specifies. Ports are linear, and can at most be connected
  ("entangled") with one other port. That connection is symmetric and bi-directional,
  either party may send and receive messages along it. Receives are handled by
  registering a listener for the 'message' event in the DOM API. Both Level 0 and
  Level2 event listeners are supported.
 */
class DOM_MessagePort
    : public DOM_Object,
      public DOM_EventTargetOwner,
      public ListElement<DOM_MessagePort>
{
private:
    DOM_MessagePort()
    : target(NULL),
      forwarding_port(NULL),
      is_enabled(FALSE),
      message_handler(NULL),
      keep_alive_id(0)
    {
    }

    DOM_MessagePort *target;
    /**< The target that this port is entangled with;
      *  NULL if currently not entangled. */

    DOM_MessagePort *forwarding_port;
    /**< If non-NULL, this port relays messages received to another.
         Used to provide broadcasting behaviour within one runtime,
         multiplexing N ports by forwarding to one port's listeners. */

    BOOL is_enabled;
    /**< TRUE iff port is receptive to message delivery / propagation. */

    DOM_EventQueue message_event_queue;
    /**< Queue of not-yet-delivered MessageEvents (drained on start() or
         .onmessage registration.) */

    DOM_EventListener *message_handler;
    /**< Event handler registered for .onmessage. */

    int keep_alive_id;

    OP_STATUS AddStrongReference();
    void DropStrongReference();

public:
    static OP_STATUS Make(DOM_MessagePort *&p, DOM_Runtime *runtime);

    virtual ~DOM_MessagePort();
    virtual void GCTrace();

    DOM_MessagePort *GetTarget() { return target; }

    OP_STATUS Entangle(DOM_MessagePort *new_target);
    /**< Set up a connection with another port. If that port or 'ourselves'
         are already connected to a port, those connections will be shut
         down first ("dis-entangled"), followed by establishment of the
         new connection. */

    void Disentangle();
    /**< Tear down a connection to another port. The operation is symmetric,
         the entangled port (the target) will also be dis-entangled. */

    BOOL IsEntangled() { return (target != NULL); }
    /**< Returns TRUE if the port is entangled with another. */

    BOOL IsEnabled() { return is_enabled; }
    /**< Returns the state of the port; If TRUE, then message delivery and
         queue'ing proceeds as normal. See Enable() comments for more.
         @return TRUE if enabled, FALSE otherwise. */

    static OP_STATUS PrepareTransfer(DOM_MessagePort *&new_port, DOM_EnvironmentImpl *target_environment);
    /**< Prepare for transferring this port to a new environment. This creates
         a new message port in that environment, but doesn't transfer its
         state and dependency. That's delayed until the cloning operation has
         succesfully completed (and is handled by Transfer(), see it for more) */

    OP_STATUS Transfer(DOM_MessagePort *new_port, DOM_EnvironmentImpl *target_environment, DOM_WebWorkerBase *new_owner = NULL);
    /**< When posting a message, ports may optionally be transferred to
         the target. This is done by providing an array of (transferable)
         objects to postMessage(). Transfer() performs the transfer of
         a port's queued up events and re-assigns its entangled port. */

    OP_BOOLEAN IsCloned();
    /**< If OpBoolean::IS_TRUE, the port has already been cloned as part of
         postMessage(). It is an error to use such a cloned port in another
         postMessage() operation. Will report OOM as OpStatus::ERR_NO_MEMORY. */

    /* DOM infrastructure methods - please consult their defining classes for information. */

    virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    virtual BOOL IsA(int type) { return type == DOM_TYPE_MESSAGEPORT || DOM_Object::IsA(type); }
    /* DOM_EventTargetOwner */
    virtual DOM_Object *GetOwnerObject() { return this; }

    DOM_DECLARE_FUNCTION_WITH_DATA(accessEventListenerStart);

    /* User operations on MessagePorts: */
    DOM_DECLARE_FUNCTION(start);
    DOM_DECLARE_FUNCTION(close);
    DOM_DECLARE_FUNCTION(postMessage);

    static int SendMessage(DOM_MessagePort *port, DOM_MessageEvent *message_event);

    void SetForwardingPort(DOM_MessagePort *p) { forwarding_port = p; }
    DOM_MessagePort *GetForwardingPort() { return forwarding_port; }

    enum { FUNCTIONS_ARRAY_SIZE = 5 };
    enum {
        FUNCTIONS_WITH_DATA_BASIC = 2,
#ifdef DOM3_EVENTS
        FUNCTIONS_WITH_DATA_addEventListenerNS,
        FUNCTIONS_WITH_DATA_removeEventListenerNS,
#endif // DOM3_EVENTS
        FUNCTIONS_WITH_DATA_ARRAY_SIZE
    };
};


/** DOM_MessageChannel - a channel is just a convenience grouping of
    a pair of entangled MessagePorts and a corresponding constructor.
    The constructor creating the port pair and entangling them for you. */
class DOM_MessageChannel
    : public DOM_Object
{
private:
    DOM_MessageChannel()
        : port1(NULL),
          port2(NULL)
    {
    }

    DOM_MessagePort *port1;
    DOM_MessagePort *port2;

public:
    static OP_STATUS Make(DOM_MessageChannel *&channel, DOM_EnvironmentImpl *environment);
    /**< Create a MessageChannel in the given DOM execution context / environment,
         i.e., a pair of message ports that are entangled with one another.
         The ports may then be selected and communicated along (or passed to
         other parties via postMessage().)

         @param [out]channel The result parameter, filled in with reference to
                new DOM_MessageChannel upon success.
         @param environment The environment in which to create the channel
                and its ports.
         @return OpStatus::ERR_NO_MEMORY if OOM prevents instance creation,
                 OpStatus::ERR for all other failure conditions.
                 OpStatus::OK upon successful creation. */

    virtual ~DOM_MessageChannel();
    virtual void GCTrace();

    virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
    virtual BOOL IsA(int type) { return type == DOM_TYPE_MESSAGECHANNEL || DOM_Object::IsA(type); }
};


/** DOM_MessageEvent - MessageEvent as seen and used in a DOM context. */
class DOM_MessageEvent
    : public DOM_Event
{
public:
    DOM_MessageEvent()
        : source(NULL),
          ports(NULL)
    {
        DOMSetUndefined(&data);
    }

    static OP_STATUS Make(DOM_MessageEvent *&message_event, DOM_Runtime *runtime);

    static OP_STATUS Make(DOM_MessageEvent*&message_event, DOM_Object *this_object, DOM_MessagePort *source_port, DOM_MessagePort *target_port, DOM_EnvironmentImpl *environment, const URL &url, ES_Value *argv, int argc, ES_Value *return_value, DOM_WebWorkerBase *target_worker = NULL);
    static int Create(DOM_MessageEvent *&message_event, DOM_Object *this_object, DOM_Runtime* target_runtime, DOM_MessagePort *source_port, DOM_MessagePort *target_port, URL &origin_url, ES_Value* argv, int argc, ES_Value* return_value, BOOL add_source_property = FALSE);

    static OP_STATUS MakeConnect(DOM_MessageEvent *&connect_event, BOOL is_connect, const URL &url_target, DOM_Object *target, DOM_MessagePort *port_out, BOOL add_ports_alias = TRUE, DOM_WebWorkerBase *target_worker = NULL);
    /**< Create a DOM_MessageEvent holding a 'connect' or 'disconnect' event
         for delivery to the given target. A communication path is opened up
         as part of this, creating a new message port in the context of the
         target and entangling it with 'port_out'. Supply a NULL message port
         to not have this be done.

         @param [out] connect_event The DOM_MessageEvent result object.
         @param is_connect If TRUE, create a 'connect' message event;
         FALSE, a 'disconnect'.
         @param url_target Origin URL of the target receiver.
         @param target The object at which the connect event is aimed at.
         @param port_out The optional message port of the receiver wanting to connect.
         @param add_ports_alias If TRUE, create a .ports array with the .source message port
         at index 0. If FALSE, .ports is left as null.
         @param target_worker If the receiver is a Web Worker, reference to it.

         @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR on other errors
         during initialisation of event; OpStatus::OK otherwise. */

    virtual void GCTrace();

    const ES_Value &GetData() { return data; }
    const uni_char *GetOrigin() { return origin.CStr(); }
    const uni_char *GetLastEventId() { return lastEventId.CStr(); }
    EcmaScript_Object *GetSource() { return source; }
    ES_Object *GetPorts() { return ports; }

    OP_STATUS SetOrigin(const uni_char *s);
    OP_STATUS SetOrigin(const URL &url);

    OP_STATUS SetLastEventId(const uni_char *id);
    void SetSource(DOM_Object *o) { source = o; }
    void SetPorts(ES_Object *o) { ports = o; }
    OP_STATUS SetData(ES_Value &value);

    static OP_STATUS ValidateTransferables(ES_Object *transferables, DOM_MessagePort *source, DOM_MessagePort *target, unsigned &error_index, DOM_EnvironmentImpl* env);
    /**< Validate an array of MessagePort values as specified by the
         postMessage() APIs. Constraints checked for:

          - the 'ports' argument is expected to be a native Array
            object; anything else returns OpStatus::ERR.
          - OpStatus::ERR is also returned if any of the entries in the
            array is VALUE_NULL, duplicate DOM_MessagePort values appear
            or either the 'source' or 'target' ports appear.
          - it is also an OpStatus::ERR condition if a DOM_MessagePort
            that has been cloned before appears.

         @param ports The native array holding the ports to be sent in a
                postMessage()'s message event. If NULL, the validation
                is vacuously OpStatus::OK.
         @param source The MessagePort you are performing the
                postMessage on. NULL if between Window objects.
         @param target The MessagePort that the ports are destined for.
                Also NULL if between Window objects.
         @param[out] error_index if validation fails, this index will
               be set to the array index of the first array value
               that failed validation.
         @param environment The environmentt we are targetting
                the ports at.

         @return OpStatus::ERR if validation fails, error_index refers
                 to the first illegal element. OpStatus::OK upon successful
                 validation. */

    static OP_STATUS CloneData(DOM_Object *this_object, ES_Runtime::CloneTransferMap *transfer_map, ES_Value &target, const ES_Value &source, const uni_char *location, ES_Value *return_value, DOM_EnvironmentImpl *environment, DOM_WebWorkerBase *new_owner = NULL);
    /**<  Copy the given 'source' argument into the target's DOM environment,
          structurally cloning it. The cloning algorithm is the HTML5 one,
          supporting the faithful cloning of native values.

          @param this_object The context from which we are cloning; only used
                 when creating DOM exception result values.
          @param [out] target Reference to the value receiving the cloned
                 value upon success.
          @param source The value to clone, best effort.
          @param location The location / URL of the operation wanting to
                 clone; included as the 'filename' field of a DOM_ErrorEvent,
                 should one have to be generated.
          @param [out] return_value The exception return value _only_; if
                 the result is OpStatus::OK, the result is provided in 'target'.
          @param environment The environment/context to clone 'source' into,
                 new objects will be attached to its associated runtime.
          @param new_worker If cloning into a web worker's context, the web worker
                 object controlling it.

          @return OpStatus::ERR if cloning operation fails, OpStatus::ERR_NO_MEMORY
                  if OOM condition is encountered. In the case of OpStatus::ERR,
                  return_value contains a throwable DOM_ErrorEvent exception value.
                  Upon success, OpStatus::OK is returned and the cloned value
                  is contained in 'target'. */

    static OP_STATUS CloneTransferables(ES_Object *transferables, ES_Runtime::CloneTransferMap &transfer_map, ES_Value &target, DOM_EnvironmentImpl *environment, BOOL no_transferables_to_null = TRUE, DOM_WebWorkerBase *new_owner = NULL);
    /**< Clone and prepare the transferable objects to be included in
         a message event. The transferables array contains objects that
         implement the Transferable interface; MessagePort and ArrayBuffer+TypedArrays
         is the initial set of object types supported.

         The method assumes that the caller have already validated the
         array for well-formedness and consistency (see ValidateTransferables().)

         The Transferable objects may be referred to in the message data
         that is separately cloned. Hence, the objects are prepared here
         by constructing their transferred (or cloned in the case of
         message ports) representations. A transfer map between the original
         transferable objects and their new copies (in the target context)
         is supplied as input when structurally cloning the message data.

         Notice that for MessagePort, cloned port instances are created
         within a target context, taking over any entanglements of the
         clonees (i.e., if port A, which is entangled with portB, is
         cloned; A_clone is created and initially  entangled with 'port B'.
         Port A is left in a disentangled state and is essentially
         useless post-cloning.)

         @param transferables The native array of ports.
         @param[out] transfer_map The transfer map, used when
               cloning.
         @param[out] target The value to receive the resulting array of
               cloned ports.
         @param new_owner The receiving Worker of the cloned ports. Optional,
                but if supplied, the cloned ports are added to that Worker's
                list of entangled ports.
         @param environment the DOM environment to clone into.
         @param no_transferables_to_null If TRUE and the ports array is NULL,
                represent this as VALUE_NULL. If not, represent it as an
                empty array.

         @return OpStatus::ERR_NO_MEMORY in case of OOM,
                 OpStatus::ERR if argument validation or cloning operation
                 hits any other error condition. OpStatus::OK on successful
                 completion and resulting value written to 'target'. */

    static int initMessageEventPostMessage(DOM_MessageEvent *message_event, DOM_Object *this_obj, DOM_MessagePort *source_port, DOM_MessagePort *target_port, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime);

    /* DOM infrastructure methods and functionality; see the defining classes for documentation. */
    virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
    virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

    virtual OP_STATUS DefaultAction(BOOL cancelled);

    virtual BOOL IsA(int type) { return type == DOM_TYPE_MESSAGEEVENT || DOM_Event::IsA(type); }

    DOM_DECLARE_FUNCTION(initMessageEvent);
    enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 2 };

private:
    virtual ~DOM_MessageEvent();

    ES_Value data;

    OpString origin;
    OpString lastEventId;
    DOM_Object *source;
    ES_Object *ports;
};

class DOM_MessageChannel_Constructor
    : public DOM_BuiltInConstructor
{
public:
    DOM_MessageChannel_Constructor()
        : DOM_BuiltInConstructor(DOM_Runtime::CROSSDOCUMENT_MESSAGECHANNEL_PROTOTYPE)
    {
    }

    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_MessageEvent_Constructor
    : public DOM_BuiltInConstructor
{
public:

    DOM_MessageEvent_Constructor()
        : DOM_BuiltInConstructor(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE)
    {
    }
    virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);

};
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#endif // DOM_CROSSMESSAGE_H
