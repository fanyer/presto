/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPCOMPONENTPLATFORM_H
#define MODULES_HARDCORE_COMPONENT_OPCOMPONENTPLATFORM_H

#include "modules/hardcore/opera/components.h" // Need auto-generated OpComponentType

class OpComponentManager;
class OpTypedMessage;


class OpComponentPlatform
{
public:
	virtual ~OpComponentPlatform() {}

	/** Re-schedule next run slice.
	 *
	 * The component manager will call this method when it discovers that
	 * the deadline specified by the previous (if any) return value from
	 * OpComponentManager::RunSlice() may need to be revised.
	 *
	 * RunSlice must \b not be called synchronously from this method.
	 *
	 * @see return value of OpComponentManager::RunSlice().
	 *
	 * @param limit Maximum number of msec to wait before calling RunSlice.
	 */
	virtual void RequestRunSlice(unsigned int limit) = 0;

	/** Request platform selection of component host.
	 *
	 * The component manager will call this method as part of creating a new
	 * component. This has the dual purpose of informing the platform as to
	 * which components are being created and allow the platform to decide
	 * which process and/or thread the new component is to be created within.
	 *
	 * This is to facilitate project-specific load balancing, isolation, and
	 * allow for component types to have differing platform needs, such as
	 * unix plug-in components requiring to run within a Gtk or Xt toolkit.
	 *
	 * When this call arrives, a platform should look at the 'type' argument
	 * and select a suitable component manager to host the new component. This
	 * may include starting a new process/thread and component manager, or
	 * simply always selecting the solitary component manager in a single-
	 * threaded environment. Once a component manager has been selected, the
	 * three arguments should be forwarded to the selected manager through
	 * its OpComponentManager::HandlePeerRequest() interface.
	 *
	 * This call will only be made by the initial component manager.
	 *
	 * @param peer On input this argument is set to the identifier of the
	 *        OpComponentManager which asks the platform to create a new
	 *        component. This is usually the value 0, because usually the
	 *        initial OpComponentManager is responsible for creating new
	 *        components. (Note: this is not the identifier of the
	 *        OpComponentManager which requested the component.) The
	 *        OpComponentPlatform implementation needs to set this parameter on
	 *        output to the identifier of the OpComponentManager that will be
	 *        hosting the new component. The output value will be sent by the
	 *        calling OpComponentManager in an OpPeerPendingMessage to the
	 *        requester.
	 * @param requester The address of the entity requesting a component
	 * @param type The type of component that is being created.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS RequestPeer(int& peer, OpMessageAddress requester, OpComponentType type) = 0;

	/** Request platform selection of in-process component host.
	 *
	 * This method is a restricted form of RequestPeer(), and is called by
	 * the component manager when it desires a host for the new component
	 * that is in the same process. The use case is multi-core utilization
	 * when operating on a shared, complex data structure.
	 *
	 * The platform MUST NOT select the calling component manager.
	 *
	 * The platform MAY return OpStatus::ERR_NOT_SUPPORTED. Components are
	 * not allowed to rely on support for this type of request.
	 *
	 * @param[out] peer The identifier of the component manager that will be
	 *             hosting the new component. Its initial value is invalid.
	 * @param requester The address of the entity requesting an in-process
	 *        component.
	 * @param type The type of component that is being created.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS RequestPeerThread(int& peer, OpMessageAddress requester, OpComponentType type) { return OpStatus::ERR_NOT_SUPPORTED; }

	/** Transmit a message to another component manager.
	 *
	 * The component manager will call this method when it needs to deliver
	 * a message to another component manager. The platform assumes ownership
	 * of the message.
	 *
	 * This implementation of this method  guarantees that either a) the message
	 * is successfully delivered to the recipient component manager, b) this
	 * call returns an error, or c) the remote component manager is destroyed
	 * within a reasonable timeframe. This ensures that end-users can reliably
	 * expect either an immediate error or a PeerDisconnected message if the
	 * message could not be delivered.
	 *
	 * The platforms has two choices in how to handle messages:
	 * -# (typical in multithread case) Pass the message pointer directly
	 *    to the receiver.
	 * -# (typical in multiprocess case) Serialize the message to a buffer,
	 *    delete the message, transmit the serialized data to the destination
	 *    process, deserialize it and deliver the resulting message.
	 *
	 * The message is delivered to the recipient component manager through its
	 * OpComponentManager::DeliverMessage() interface.
	 *
	 * WARNING: It is the responsibility of the platform to perform locking or
	 * otherwise ensure that two threads do not simultaneously call
	 * OpComponentManager::DeliverMessage(). OpComponentManager is not
	 * threadsafe.
	 *
	 * @param message A message to transmit. The address of the component
	 *                manager to deliver the message to can be extracted from
	 *                the message's destination address.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS SendMessage(OpTypedMessage* message) = 0;

	/** Notify the platform of a component's birth.
	 *
	 * The initial component manager and the hosting component manager will call
	 * this method when a new component has been started. The platform may use
	 * this method in concert with OnComponentDestroyed() to track existence and
	 * location of components.
	 *
	 * @see OpComponent::RequestComponent() for a complete workflow of creating
	 *      a new OpComponent instance.
	 *
	 * @param address The address of the new OpComponent.
	 */
	virtual void OnComponentCreated(OpMessageAddress address) {}

	/** Notify the platform of a component's demise.
	 *
	 * The initial component manager and the hosting component manager will call
	 * this method when a component has been destroyed in a controlled
	 * fashion. The platform may use this method in concert with
	 * OnComponentCreated() to track existence and location of components.
	 *
	 * An uncontrolled fashion is when a component is destroyed implicitly
	 * by a process or thread dying. When this happens, the platform must
	 * report the address of the dead component manager to the initial
	 * component manager. The platform is expected to deduce which components
	 * were destroyed, so this method will not be called.
	 *
	 * @param address The address of the destroyed OpComponent.
	 */
	virtual void OnComponentDestroyed(OpMessageAddress address) {}

#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
	/**
	 * Flags defining restrictions on a call to ProcessEvents().
	 */
	enum EventFlags {
		/** The implementation MUST
		 * - Send inter-OpComponentManager messages, i.e., send an
		 *   OpTypedMessage that was requested by
		 *   OpComponentPlatform::SendMessage() to another OpComponentManager
		 *   instance.
		 * - Receive inter-OpComponentManager messages, i.e., receive an
		 *   OpTypedMessage that was sent by another OpComponentManager. On
		 *   receiving such a message OpComponentManager::DeliverMessage() shall
		 *   be called.
		 */
		PROCESS_IPC_MESSAGES = 0x01,

		/** The implementation MUST satisfy all above requirements, but may also
		 * perform any other tasks as it sees fit. This includes (though it
		 * may not be a complete list):
		 * - UI events, like mouse-movement, keyboard input, ...
		 * - Events caused by file-io or socket-io. */
		PROCESS_ANY          = 0xFF
	};

	/** Process platform events.
	 *
	 * This method will be called when core code is waiting for some component
	 * messages and cannot relinquish control through normal means.
	 *
	 * The implementation MUST handle all available platform events and tasks as
	 * indicated by the specified flags.
	 *
	 * When there is nothing to do, the implementation shall wait for the
	 * specified timeout or for a new platform event which corresponds to the
	 * specified flags.
	 *
	 * When there are any platform events (corresponding to the specified
	 * flags), the implementation shall handle all available events and return
	 * immediately without waiting for the specified timeout. If possible then
	 * the implementation should also return at the specified timeout even if
	 * there are more platform events in the queue.
	 *
	 * @note It is recommended to return immediately after handling platform
	 *  events and not wait for the specified \c timeout. However the platform
	 *  may decide to continue to wait for the end of the specified \c timeout.
	 *  In such a case the platform must ensure to change the time-out when
	 *  RequestRunSlice() is called with a new time-out value. Messages
	 *  delivered to core during the handling of platform messages (see
	 *  OpComponentManager::DeliverMessage()) may result in a call to
	 *  RequestRunSlice() or in a nested call to ProcessEvents(). In this case
	 *  the implementation must ensure that all active ProcessEvents() calls
	 *  respect the last received value of any received \c timeout or
	 *  RequestRunSlice()'s \c limit argument. In particular if
	 *  RequestRunSlice() is called with a \c limit == 0 or a nested
	 *  ProcessEvents() is called with a \c timeout == 0, then the
	 *  implementation of this method shall return immediately.
	 *
	 * @note Calls to this method will be interleaved with calls to RunSlice(),
	 *  so the implementation does not need to call RunSlice(), although it may
	 *  choose to do so out of convenience. If it does, the call to RunSlice()
	 *  may result in nested calls to ProcessEvents() or RequestRunSlice().
	 *
	 * @note This method is called from core to create an inner message-loop. If
	 *  a platform cannot handle platform events outside its main message loop,
	 *  it is encouraged to return OpStatus::ERR_NOT_SUPPORTED. However some
	 *  OpComponent may depend on this functionality.
	 *
	 * @param timeout specifies the maximal time in milliseconds until the
	 *        implementation has to return control to the caller. It may return
	 *        early.
	 * @param flags specifies the requirements and recommendations of this call.
	 *        See EventFlags above.
	 *
	 * @retval OpStatus::OK on successfully handling the platform events.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if the platform does not support this
	 *         implementation.
	 * @retval OpStatus::ERR or any other error code to indicate some kind of
	 *         failure.
	 */
	virtual OP_STATUS ProcessEvents(unsigned int timeout, EventFlags flags = PROCESS_ANY) = 0;
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

};

#endif /* MODULES_HARDCORE_COMPONENT_OPCOMPONENTPLATFORM_H */
