/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPCOMPONENT_H
#define MODULES_HARDCORE_COMPONENT_OPCOMPONENT_H

#include "modules/hardcore/component/OpMessenger.h" // Need OpMessenger
#include "modules/otl/map.h"
#include "modules/hardcore/opera/components.h" // Need auto-generated OpComponentType

class OpComponentManager;

/** An OpComponent is the base-class for a modular part of Opera which
 * encapsulates its content.
 *
 * The extend of the modularisation is declared by some \c module.components
 * file. That declaration specifies an #OpComponentType and an OpComponent
 * sub-class which provides its implementation. The encapsulated content of an
 * OpComponent instance includes its code and data. Some OpComponent sub-classes
 * may have several instance, others may only have a single instance. In the
 * following documentation we use the term \e component for an instance of an
 * OpComponent sub-class.
 * @see \ref component_framework
 *
 * A new component is created and started by requesting a component of some type
 * (see OpComponent::RequestComponent()). Typically a component is created and
 * run by a platform in its own thread or process. One thread or process
 * corresponds to one OpComponentManager instance. However the platform may
 * decide to run several components in the same thread/process, i.e., several
 * OpComponent instances may be associated with one OpComponentManager. On
 * creating a component, it is assigned a component number, which is unique
 * within the associated OpComponentManager. The triple (cm,co,0), where cm is
 * the number of the associated OpComponentManager and co is the component
 * number, form the unique OpMessageAddress of the component.
 * @see \ref message_address
 *
 * Handling messages is the only way to give some runtime to some component. A
 * component may communicate with other components only via messages (see
 * #OpTypedMessage). I.e., a component may not use data from any other component
 * (even if they are instances of the same OpComponent sub-class). It should be
 * clear that it is not allowed to use code from any other OpComponent
 * sub-class. Messages are sent through channels (see OpChannel). A channel has
 * a local address and a remote address. E.g., on requesting a component (see
 * OpComponent::RequestComponent()), the requester receives an OpChannel
 * instance which can be used to communicate with the requested component.
 *
 * To communicate with another component, a component creates an instance of an
 * #OpTypedMessage sub-class and sends it via an OpChannel.
 * @see \ref component_messages
 */
class OpComponent
	: public OpMessenger
{
private:
	/** The class OpComponentManager is declared as friend to let its method
	 * OpComponentManager::CreateComponent() call the static factory method
	 * OpComponent::Create(). */
	friend class OpComponentManager;

	/** Private factory method to create an OpComponent of the specified type.
	 * Only OpComponentManager::CreateComponent() may call this method.
	 * @param component receives the pointer to the created OpComponent
	 *        instance.
	 * @param type specifies the type of the OpComponent to create.
	 * @param address specifies the address of the OpComponent.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if the specified type was unknown.
	 * @retval OpStatus::ERR or any other error code if the component was not
	 *         created because of the returned error code. */
	static OP_STATUS Create(OpComponent** component, OpComponentType type, const OpMessageAddress* address);

public:
	virtual ~OpComponent();

	/**
	 * Return this component's type.
	 *
	 * @return Component type,
	 */
	virtual OpComponentType GetType() const { return m_type; }

	/**
	 * Return TRUE if given address belongs inside this component.
	 */
	inline BOOL IsLocal(const OpMessageAddress& address) const
	{
		return address.IsSameComponent(GetAddress());
	}

	/** Check whether the given address exists within this object.
	 *
	 * Return TRUE if the given address refers to this component or a channel
	 * within this component. If this method returns true, a message with the
	 * specified destination address (see OpTypedMessage::GetDst()) will be
	 * processed by DispatchMessage().
	 * @see \ref handling_messages
	 * @see OpMessenger::ProcessMessage()
	 *
	 * @param address Address to check if exists within this object.
	 * @returns TRUE if address exists, FALSE otherwise.
	 */
	BOOL AddressExists(const OpMessageAddress& address);

	/** Allocate and construct an OpChannel.
	 *
	 * A new address for the channel is assigned and the channel is registered
	 * in this component (see RegisterChannel()).
	 *
	 * This is a convenience method for creating an OpChannel with OP_NEW,
	 * calling OpChannel::Construct() and possibly adding a first
	 * OpMessageListener instance:
	 * \code
	 * OpChannel* channel = OP_NEW(OpChannel, (destination, this));
	 * channel->Construct();
	 * channel->AddMessageListener(listener);
	 * \endcode
	 *
	 * @param destination Destination of messages sent on the returned channel.
	 * @param listener An OpMessageListener instance which should be called when
	 *        the channel receives some message.
	 *        If the specified \c listener instance may be deleted before the
	 *        returned channel is deleted, then the caller must ensure that
	 *        OpChannel::RemoveMessageListener() is called.
	 *
	 * @return Pointer to a newly created OpChannel, or NULL on OOM.
	 */
	OpChannel* CreateChannel(const OpMessageAddress& destination = OpMessageAddress(), OpMessageListener* listener=NULL);

	/**
	 * Assign an address to and register a channel.
	 *
	 * This method should only be called by OpChannel's secondary constructor
	 * (OpChannel::Construct()).
	 *
	 * @param channel Channel to register.
	 * @param root True if the OpComponent with the specified remote address
	 *        requested the creation of this OpComponent. Each component has
	 *        only one root channel. Disconnecting the root channel will destroy
	 *        the component.
	 *
	 * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS RegisterChannel(OpChannel* channel, bool root = false);

	/**
	 * Deregister channel.
	 *
	 * Called by OpChannel's destructor.
	 *
	 * @param channel Channel to deregister.
	 *
	 * @retval OpStatus::OK if channel was known and deregistered.
	 * @retval OpStatus::ERR if not.
	 */
	OP_STATUS DeregisterChannel(OpChannel* channel);

	/**
	 * Retrieve a registered channel.
	 *
	 * @param address The local channel address.
	 *
	 * @return OpChannel pointer, or NULL if not found.
	 */
	OpChannel* GetChannel(const OpMessageAddress& address) const;

	/** Request a channel to a (remote) component of the given type.
	 *
	 * If the requested component type belongs to a singleton component and the
	 * singleton component is already running, then an OpChannel to the running
	 * component is returned. Otherwise the request is forwarded via the initial
	 * OpComponentManager to the platform (OpComponentPlatform) which will start
	 * a new component of the specified type.
	 *
	 * This method creates a channel for communicating with the remote
	 * component, and stores it in the \c channel output-parameter. The channel
	 * is also registered in this instance (see RegisterChannel()). When the
	 * remote component becomes available for communication, an
	 * OpPeerConnectedMessage is sent to the local end of this channel. If a new
	 * component is created, then the returned channel will be the remote
	 * component's root channel (see OpChannel::IsRoot()).
	 *
	 * When the requested remote component is no longer needed, the caller can
	 * simply OP_DELETE the returned \c channel and the remote component will
	 * receive a notification (see OpPeerDisconnectedMessage) that the channel
	 * was disconnected. If a root channel is deleted, then the remote component
	 * is shut down.
	 *
	 * If there is no need to keep an open channel to the requested remote
	 * component, but the component is to keep running and terminate itself,
	 * then the returned channel object may simply be forgotten. It will be
	 * released when either this component or the remote component dies.
	 *
	 * Workflow for requesting a new component:
	 * \msc
	 * Requester, OpComponent [url="\ref OpComponent"],
	 * RequestMessage [label="OpComponent\nRequestMessage",
	 *                 url="\ref OpComponentRequestMessage"],
	 * Manager [label="OpComponent\nManager", url="\ref OpComponentManager"],
	 * Platform [label="OpComponent\nPlatform", url="\ref OpComponentPlatform"];
	 * Requester => OpComponent
	 *  [label="1. RequestComponent()",
	 *   url="\ref OpComponent::RequestComponent()"];
	 * OpComponent -> RequestMessage
	 *  [label="Create msg", url="\ref OpComponentRequestMessage::Create()"];
	 * OpComponent => Manager
	 *  [label="2. SendMessage(msg)",
	 *   url="\ref OpMessenger::SendMessage()"];
	 * Manager => Platform
	 *  [label="3. SendMessage(msg)",
	 *   url="\ref OpComponentPlatform::SendMessage()"];
	 * Manager << Platform;
	 * OpComponent << Manager;
	 * Requester << OpComponent;
	 * ...; --- [label="4. msg is posted to the initial process"]; ...;
	 * Platform => Manager
	 *  [label="5. ReceiveMessage(msg)",
	 *   url="\ref OpComponentManager::ReceiveMessage()"];
	 * Platform << Manager;
	 * --- [label="Next run-slice"];
	 * Manager => Manager
	 *  [label="6. ProcessMessage(msg)",
	 *   url="\ref OpComponentManager::ProcessMessage()"];
	 * Manager => Manager
	 *  [label="7. HandleComponentRequest()",
	 *   url="\ref OpComponentManager::HandleComponentRequest()"];
	 * Manager => Platform
	 *  [label="8. RequestPeer()", url="\ref OpComponentPlatform::RequestPeer"];
	 * Platform -> Platform
	 *  [label="9. forward msg to destination process"];
	 * Manager << Platform;
	 * Manager -> Requester
	 *  [label="10. OpPeerPendingMessage", url="\ref OpPeerPendingMessage"];
	 * ...; ---[label="11. In the destination process"]; ...;
	 * Platform => Manager
	 *  [label="12. HandlePeerRequest()",
	 *   url="\ref OpComponentManager::HandlePeerRequest()"];
	 * Manager -> OpComponent [label="13. Create new component"];
	 * Manager -> Requester
	 *  [label="14. OpPeerConnectedMessage", url="\ref OpPeerConnectedMessage"];
	 * Manager -> Platform
	 *  [label="15. OnComponentCreated()",
	 *   url="\ref OpComponentPlatform::OnComponentCreated()"];
	 * Manager << Platform;
	 * Platform << Manager;
	 * \endmsc
	 * -# The requester calls OpComponent::RequestComponent() to request a
	 *    channel to a component of a specified type.
	 * -# OpComponent::RequestComponent() creates an OpComponentRequestMessage
	 *    and sends the message via its OpComponentManager to the initial
	 *    OpComponentManager.
	 * -# The OpComponentManager passes the message to the OpComponentPlatform.
	 * -# The OpComponentPlatform posts the message to the initial process
	 * -# and calls OpComponentManager::ReceiveMessage() to insert it in the
	 *    OpComponentManager's inbox.
	 * -# On the next OpComponentManager::RunSlice() of the initial process, the
	 *    OpComponentRequestMessage is processed by the OpComponentManager ...
	 * -# by calling OpComponentManager::HandleComponentRequest(), which ...
	 * -# calls OpComponentPlatform::RequestPeer().
	 * -# The OpComponentPlatform controls which process shall run the requested
	 *    component, so it forwards the request to the destination process.
	 * -# After the platform forwarded the request to create a new component,
	 *    the OpComponentManager sends an OpPeerPendingMessage to the requester
	 *    to notify the intermediate step.
	 * -# In the destination process, the OpComponentPlatform receives the
	 *    request and ...
	 * -# calls OpComponentManager::HandlePeerRequest().
	 * -# The OpComponentManager creates a new OpComponent, ...
	 * -# sends an OpPeerConnectedMessage to the requester and
	 * -# class OpComponentPlatform::OnComponentCreated() on the associated
	 *    OpComponentPlatform instance to notify the platform about the birth of
	 *    a new component.
	 *
	 * @note If the new component is created before the OpPeerPendingMessage in
	 *       step 10 is sent, the OpPeerConnectedMessage may arrive first at the
	 *       requester.
	 *
	 * @param[out] channel The channel to requested component is stored here.
	 * @param type OpComponentType of requested peer.
	 *
	 * @return OpStatus::OK on success. On error, the channel argument will
	 *         be set to NULL.
	 */
	OP_STATUS RequestComponent(OpChannel** channel, OpComponentType type);

	/**
	 * Deliver a message for processing to this component or one of its channels.
	 *
	 * The given message must be destined for this component or one of its
	 * channels. The message is passed to the destination's ProcessMessage()
	 * method, and its return value is propagated.
	 * If the destination address no longer exists an OpPeerDisconnectedMessage
	 * is sent to the source of the message.
	 *
	 * @see AddressExists()
	 * @param message Message to be processed.
	 * @returns Return value from the appropriate ProcessMessage() method.
	 */
	OP_STATUS DispatchMessage(const OpTypedMessage* message);

	/**
	 * Disconnect channels with endpoints in the given component or component
	 * manager.
	 *
	 * @param address The address of the component or component manager to forget.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS DisconnectMessenger(const OpMessageAddress& address);

	/**
	 * Called by the associated root OpChannel when it was disconnected.
	 *
	 * The root channel controls the component's lifetime. If the remote end
	 * disconnects the root channel, this method is called by
	 * OpChannel::ProcessMessage() on handling the #OpPeerDisconnectedMessage
	 * to destroy this component. See OpComponentManager::DestroyComponent().
	 */
	OP_STATUS OnRootChannelDisconnected();

protected:
	/// Constructor. Only accessible to subclasses
	OpComponent(const OpMessageAddress& address, OpComponentType type);

	/// Destroy this component and all its channels.
	virtual void Destroy();

	/// Remove local channels
	void RemoveLocalChannels();

	/**
	 * Assign and return the next appropriate OpChannel instance address.
	 *
	 * @return Next available address for a local OpChannel instance.
	 */
	OpMessageAddress AssignNextChannelAddress();


	/// The type of this component.
	OpComponentType m_type;

	/// Next appropriate 'ch' number for a local channel address.
	int m_next_channel_number;

	/// Local channels.
	typedef OtlMap<OpMessageAddress,OpChannel*> ChannelMap;
	ChannelMap m_channels;
};

#endif /* MODULES_HARDCORE_COMPONENT_OPCOMPONENT_H */
