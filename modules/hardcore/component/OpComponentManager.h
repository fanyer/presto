/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPCOMPONENTMANAGER_H
#define MODULES_HARDCORE_COMPONENT_OPCOMPONENTMANAGER_H

#include "modules/hardcore/component/OpMessenger.h" // Need OpMessenger
#include "modules/hardcore/component/OpComponent.h" // Need OpComponent
#include "modules/hardcore/component/OpComponentPlatform.h"
#include "modules/hardcore/component/OpChannel.h" // Need OpChannel
#include "modules/otl/map.h"
#include "modules/hardcore/opera/components.h" // Need auto-generated OpComponentType
#include "modules/util/OpHashTable.h" // Need OpINT32HashTable
#include "modules/pi/OpTimeInfo.h"
#include "modules/hardcore/component/OpTimeProvider.h"

/**
 * @page component_framework Component Framework
 *
 * Contents:
 * - @ref component_manager
 * - @ref component_platform
 * - @ref component
 * - @ref channel
 *
 * @see @ref component_messages
 *
 *
 * @section component_manager OpComponentManager
 *
 * One thread or process corresponds to one OpComponentManager instance. When
 * the platform starts a thread or process in which to run the component
 * framework, the platform creates an OpComponentManager instance and an
 * OpComponentPlatform instance. The platform (OpComponentPlatform) may decide
 * to run several components in the same thread/process, i.e., several
 * OpComponent instances may be associated with one OpComponentManager.
 *
 * Each thread/process (i.e., each OpComponentManager instance) has a unique id
 * (which is specified as argument \p address_cm on creating an
 * OpComponentManager instance). The id is part of the address of a component
 * manager and its components (see @ref message_address).
 *
 *
 * @section component_platform OpComponentPlatform
 *
 * An OpComponentPlatform is implemented by the platforms. Its main tasks are
 * - Starting components in processes or threads.
 * - Decide in which process or thread to start a component.
 * - Send messages between processes or threads.
 *
 * One OpComponentPlatform instance corresponds to one OpComponentManager
 * instance.
 *
 *
 * @section component OpComponent
 *
 * OpComponent is the base-class for a modular part of Opera which encapsulates
 * its content ("component"). The extend of the modularisation is declared by
 * some \c module.components file. That declaration specifies:
 * - An OpComponentType;
 * - The name of an OpComponent sub-class;
 * - ...
 *
 * The component-instance includes its code and data. It should be clear that it
 * is not allowed to use code from any other OpComponent sub-class.
 *
 * A new component is created and started by requesting a component of some
 * type: see OpComponent::RequestComponent(). Typically a component is created
 * and run by a platform in its own thread or process. Some components may have
 * several instances, others may only have a single instance.
 *
 * Each component, is assigned an id (co != 0, component number), which is
 * unique within the associated OpComponentManager. The OpComponentManager which
 * creates the OpComponent instance assigns this id (see
 * OpComponent::AssignNextChannelAddress()). The component number is part of the
 * address of a component and its channels (see @ref message_address).
 *
 *
 * @section channel OpChannel
 *
 * An OpChannel can be seen as a socket. It is used to send and receive
 * messages. Thus an OpChannel can be used to communicate between different
 * components or within the same component.
 *
 * A component can have several OpChannel instances. Each channel has a unique
 * id (ch != 0, channel-number) which is assigned on creating a new channel (see
 * OpComponent::CreateChannel()). The channel number is part of the address of a
 * channel (see @ref message_address).
 *
 * The channel allows to encapsulate the communication about one specific topic
 * between two components.
 *
 * A channel has exactly two ends:
 * - The local end (in this component).
 * - The remote end (in the other component).
 * If component A creates a channel to component B, then component B will have a
 * channel to component A.
 *
 * \e Example dialog-request: component A is the document and component B is the
 * UI.
 */


/**
 * @page component_messages Message handling in the component framework
 *
 * Contents
 * - @ref message_address
 * - @ref messages
 * - @ref sending_messages_1
 * - @ref sending_messages_2
 * - @ref handling_messages
 * - @ref legacy_messages
 *
 * @see @ref component_framework
 *
 *
 * @section message_address Message address
 *
 * An OpComponentManager has a unique id: \c cm, an OpComponent has a unique id:
 * \c co and an OpChannel has a unique id: \c ch. The triple \c (cm.co.ch)
 * defines an address.
 *
 * If \c co=ch=0, it is the address of an OpComponentManager. If \c ch=0, it is
 * the address of an OpComponent. Otherwise it is the address of an OpChannel.
 *
 * An address is stored in an OpMessageAddress instance. An address is used to
 * send a message to from some source to a destination. Creating a new address
 * is equivalent to creating a new OpChannel (or an OpComponent or an
 * OpComponentManager).
 *
 *
 * @section messages Messages
 *
 * Handling messages is the only way to give some runtime to a component. A
 * component may communicate with other components only via messages. I.e., a
 * component may not use data from any other component (even if they are
 * instances of the same OpComponent sub-class).
 *
 * Messages are instances of an OpTypedMessage sub-class. A message is defined
 * by some \c module.protobuf and \c message.proto files. A Python script
 * generates generic message classes with set/get/serialise/deserialise
 * methods. Only data which can be serialised can be used in a message, i.e., no
 * pointer to some Object.
 *
 * An OpMessageListener implementation can be used to handle messages. A
 * component may add one or several \c OpMessageListener instances to some
 * channel. When the channel receives a message, all listener instances of the
 * channel are called and can handle the message (see
 * OpMessageListener::ProcessMessage()).
 *
 *
 * @section sending_messages_1 Sending messages within a component manager
 *
 * \msc
 * Object,
 * OpChannel [url="\ref OpChannel"],
 * OpComponentManager
 *   [label="OpComponent\nManager", url="\ref OpComponentManager"],
 * Inbox;
 * Object -> OpChannel
 *  [label="1. SendMessage()", url="\ref OpChannel::SendMessage()"];
 * OpChannel -> OpComponentManager
 *  [label="2. SendMessage()", url="\ref OpComponentManager::SendMessage()"];
 * OpComponentManager -> OpComponentManager
 *  [label="3. ReceiveMessage()",
 *   url="\ref OpComponentManager::ReceiveMessage()"];
 * OpComponentManager -> Inbox [label="4. Insert"];
 * \endmsc
 *
 * -# To send a message some object calls OpChannel::SendMessage().
 * -# OpChannel::SendMessage() inserts the channel's source and destination
 *    address to the message and calls OpComponentManager::SendMessage().
 * -# If OpComponentManager::SendMessage() discovers that the destination
 *    address is within the component-manager, it calls
 *    OpComponentManager::ReceiveMessage().
 * -# OpComponentManager::ReceiveMessage() inserts the message into the inbox of
 *    the component-manager. This step does not require message serialisation.
 *
 *
 * @section sending_messages_2 Sending messages to different component manager
 *
 * If the destination is in a different OpComponentManager instance, i.e., a
 * different thread/process, then the OpComponentPlatform is responsible to post
 * the message to the other thread/process.
 *
 * \msc
 * Object,
 * OpChannel [url="\ref OpChannel"],
 * OpComponentManager
 *   [label="OpComponent\nManager", url="\ref OpComponentManager"],
 * Inbox,
 * OpComponentPlatform
 *   [label="OpComponent\nPlatform", url="\ref OpComponentPlatform"],
 * connection;
 * Object -> OpChannel
 *  [label="1. SendMessage()", url="\ref OpChannel::SendMessage()"];
 * OpChannel -> OpComponentManager
 *  [label="2. SendMessage()", url="\ref OpComponentManager::SendMessage()"];
 * OpComponentManager -> OpComponentPlatform
 *  [label="3. SendMessage()",
 *   url="\ref OpComponentPlatform::SendMessage()"];
 * OpComponentPlatform -> connection [label="4. Serialise"];
 * ...;
 * connection -> OpComponentPlatform [label="5. Deserialise"];
 * OpComponentPlatform -> OpComponentManager
 *  [label="6. ReceiveMessage()",
 *   url="\ref OpComponentManager::ReceiveMessage()"];
 * OpComponentManager -> Inbox [label="7. Insert"];
 * \endmsc
 *
 * -# Some object calls OpChannel::SendMessage().
 * -# OpChannel::SendMessage() inserts the channel's source and destination
 *    address to the message and calls OpComponentManager::SendMessage().
 * -# If OpComponentManager::SendMessage() discovers that the destination
 *    address is outside the component-manager, it calls
 *    OpComponentPlatform::SendMessage().
 * -# OpComponentPlatform::SendMessage() uses its connection to the destination
 *    process/thread to serialise and send the data.
 * -# The OpComponentPlatform in the destination process/thread listens to
 *    incoming messages and deserialises the message.
 * -# After deserialising the message, the OpComponentPlatform calls
 *    OpComponentManager::ReceiveMessage().
 * -# OpComponentManager::ReceiveMessage() inserts the message into its inbox.
 *
 *
 * @section handling_messages Handling Messages
 *
 * \msc
 * Platform,
 * OpComponentManager
 *   [label="OpComponent\nManager", url="\ref OpComponentManager"],
 * OpComponent [url="\ref OpComponent"],
 * OpChannel [url="\ref OpChannel"],
 * OpMessageListener
 *   [label="OpMessage\nListener", url="\ref OpMessageListener"];
 * Platform -> OpComponentManager
 *   [label="1. RunSlice()", url="\ref OpComponentManager::RunSlice()"];
 * OpComponentManager -> OpComponent
 *   [label="2. DispatchMessage()", url="\ref OpComponent::DispatchMessage()"];
 * OpComponent -> OpChannel
 *   [label="3. ProcessMessage()", url="\ref OpChannel::ProcessMessage()"];
 * OpChannel -> OpMessageListener
 *   [label="4. ProcessMessage()",
 *    url="\ref OpMessageListener::ProcessMessage()"];
 * ...;
 * \endmsc
 *
 * -# The platform gives a thread/process some runtime by calling
 *    OpComponentManager::RunSlice().
 * -# In a run-slice the OpComponentManager takes a message from its inbox and
 *    calls OpComponent::DispatchMessage() on the destination component.
 * -# OpComponent::DispatchMessage() calls OpChannel::ProcessMessage() to
 *    forward the message to the destination channel.
 * -# OpChannel::ProcessMessage() call OpMessageListener::ProcessMessage() on
 *    all registered OpMessageListener instances.
 *
 * The destination of some messages are an OpComponentManager or an OpComponent.
 * In that case the message is handled directly by the destination and not
 * forwarded to an OpChannel or a channel's OpMessageListener.
 *
 * Example: sending a text-message:
 * \code
 * OP_STATUS MyClass::SayHelloWorld(OpChannel* channel)
 * {
 *     UniString hello_world;
 *     RETURN_IF_ERROR(hello_world.SetConstData(UNI_L("Hello World!")));
 *     return channel->SendMessage(OpTextMessage::Create(hello_world));
 * }
 * \endcode
 *
 * Example: handling the text-message:
 * \code
 * class MyMessageListener : public OpMessageListener {
 * public:
 *     virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);
 * private:
 *     OP_STATUS OnTextMessage(const OpTextMessage* text_message);
 * };
 *
 * OP_STATUS MyMessageListener::ProcessMessage(const OpTypedMessage* message)
 * {
 *     OP_ASSERT(message);
 *     switch (message->GetType()) {
 *     case OpTextMessage::Type:
 *         return OnTextMessage(OpTextMessage::Cast(message));
 *
 *     default:
 *         OP_ASSERT(!"received unexpected message!");
 *         return OpStatus::ERR;
 *     }
 * }
 *
 * OP_STATUS MyMessageListener::OnTextMessage(const OpTextMessage* text_message)
 * {
 *     UniString reply;
 *     if (text_message->HasText())
 *         RETURN_IF_ERROR(reply.AppendFormat(UNI_L("Received '%s'"),
 *                                            text_message->GetText().Data(true)));
 *     else
 *         RETURN_IF_ERROR(reply.SetConstData(UNI_L("Received empty text message")));
 *     OpComponent* component = ...;
 *     return component->ReplyTo(text_message, OpTextMessage::Create(reply));
 * }
 * \endcode
 *
 *
 * @section legacy_messages Legacy Messages
 *
 * Before the component framework, the tuple (#OpMessage, #MH_PARAM_1,
 * #MH_PARAM_2, #MessageHandler*) was used for messages. These messages are now
 * wrapped into an OpLegacyMessage, which is a sub-class of OpTypedMessage.
 *
 * The OpLegacyMessage is delivered and handled like any other OpTypedMessage.
 * An OpLegacyMessage can only be used if the source and destination address is
 * inside the same component.
 */

class OpComponentPlatform;
class OpTypedMessageSelection;
class OpComponentActivator;

// Specific OpTypedMessages used by this class
class OpComponentRequestMessage;
class OpPeerConnectedMessage;
class OpPeerDisconnectedMessage;

// The global descriptor set used by the protobuf system
class OpProtobufDescriptorSet;

/** Manage components running in a thread/process.
 *
 * There is only \e one of these per thread/process, and Core code can always
 * access the current manager (i.e. the manager running in the current
 * thread/process) through the \c g_component_manager macro (which is \#defined
 * in platform code).
 *
 * There is one special OpComponentManager which is known as the initial
 * component manager (with address 0). The initial component manager is
 * responsible to start new threads/processes and delegate the creation of new
 * OpComponent instances to another running OpComponentManager.
 */
class OpComponentManager
	: public OpMessenger, public OpTimeProvider
{
public:
	/**
	 * Create a component manager.
	 *
	 * @param[out] manager On success, the newly created manager. Pointer argument must be non-NULL.
	 * @param address_cm Integer (OpMessageAddress 'cm' part) identifying this component manager.
	 *                   The initial component manager must receive address 0.
	 * @param platform An object implementing OpComponentPlatform, whose lifetime extends beyond the
	 *                 lifetime of this manager. May be NULL, in which case the platform may be set
	 *                 using SetPlatform().
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS Create(OpComponentManager**, int address_cm = 0, OpComponentPlatform* platform = 0);

	/**
	 * Destroy component manager and release all resources.
	 *
	 * Will call Destroy().
	 */
	~OpComponentManager();

	/**
	 * Destroy the component manager.
	 */
	OP_STATUS Destroy();

	/**
	 * Returns the global protobuf descriptor set which contains all descriptor
	 * objects in the system. This object is used by the generated classes
	 * when serializing/deserializing data.
	 * @return Pointer to the descriptor set.
	 */
	OpProtobufDescriptorSet *GetProtobufDescriptorSet() const { return m_protobuf_descriptors; }

	/**
	 * Retrieve platform interface. Used for peer requests and message
	 * transmission.
	 *
	 * @return pointer to platform's listener.
	 */
	inline OpComponentPlatform* GetPlatform() const { return m_platform; }

	/**
	 * Set platform listener.
	 *
	 * This is not necessary if the listener was set during construction. It
	 * should be set at the earliest opportunity, since the component manager
	 * will not be able to perform all its duties until it is. It MUST be set
	 * before calling RunSlice().
	 * The listener needs to be available until the OpComponentManager is deleted (normally when the component is shut down).
	 *
	 * @see OpComponentPlatform.
	 */
	void SetPlatform(OpComponentPlatform* platform);

	/**
	 * Retrieve currently running component.
	 *
	 * @return pointer to component currently running, if any, or NULL.
	 */
	OpComponent* GetRunningComponent() const;

	/**
	 * Retrieve a count of the currently hosted components.
	 */
	unsigned int GetComponentCount() const { return m_components.GetCount(); }

	/**
	 * Retrieve an iterator over the currently hosted components.
	 *
	 * @return Pointer to iterator where keys are component addresses and
	 *         values are OpComponents, or NULL on OOM.
	 */
	OpHashIterator* GetComponentIterator() { return m_components.GetIterator(); }

	/**
	 * Run embedded components.
	 *
	 * Called by platform to give runtime to this component manager and
	 * its children (channels and components).
	 *
	 * @param runtime Deadline in msec, after which RunSlice() should
	 *         return instead of processing more messages. RunSlice() will
	 *         process message until either there are no more messages to
	 *         process, or until \c runtime is reached (whichever happens
	 *         first). Passing \c runtime == 0 means that at most one
	 *         message will be processed before returning.
	 *
	 * @return Upper bound on msec to wait before calling RunSlice() again.
	 *         If UINT_MAX is returned, there is no upper bound. Typically
	 *         this means that the component manager requires no more
	 *         runtime until some external stimuli (e.g. an incoming
	 *         message) occurs.
	 */
	unsigned int RunSlice(unsigned int runtime = 0);

#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
	/**
	 * Calls RunSlice() once and then asks the platform to handle platform
	 * events by calling OpComponentPlatform::ProcessEvents().
	 *
	 * A component may call this method when it cannot relinquish control
	 * by returning to the component manager, but still needs to keep the
	 * browser ticking. See also OpComponentManager::AddMessageFilter().
	 *
	 * Example:
	 * @code
	 *  OpComponentManager* my_component_manager = ...;
	 *  while (not_done)
	 *      my_component_manager->YieldPlatform();
	 * @endcode
	 * The above loop exits when some of the messages that are handled by the
	 * RunSlice() call causes the condition \c not_done to turn false.
	 *
	 * @param flags Flags to pass to OpComponentPlatform::ProcessEvents().
	 *        See OpComponentPlatform::EventFlags.
	 *
	 * @return The return value of OpComponentPlatform::ProcessEvents().
	 */
	OP_STATUS YieldPlatform(OpComponentPlatform::EventFlags flags = OpComponentPlatform::PROCESS_ANY);
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

	/**
	 * Handle peer request from another component manager.
	 *
	 * Called by the platform to create a new OpComponent of the specified \p
	 * type in this component manager. If the component was created
	 * successfully, then:
	 * - an OpPeerConnectedMessage is sent to the initial component manager and
	 *   to the specified \p requester address.
	 * - OpComponentPlatform::OnComponentCreated() is called on the associated
	 *   OpComponentPlatform with the address of the new component.
	 *
	 * @see OpComponent::RequestComponent() for a complete workflow of creating
	 *      a new OpComponent instance.
	 *
	 * @param requester Address of the requester.
	 * @param type Type of component to create an instance of.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS HandlePeerRequest(const OpMessageAddress& requester, OpComponentType type);

	/**
	 * Let the initial component manager know that a component manager has
	 * been destroyed.
	 *
	 * This information is broadcast to all component managers and is used
	 * to ensure all existing channels with an end-point inside the dead
	 * component manager send a PeerDisconnected message to their listeners.
	 *
	 * Platforms are \b required to notify the initial component manager of
	 * a peer's demise. If it does not, components may hang or behave
	 * unexpectedly.
	 *
	 * @param component_manager The identifier of the dead component manager
	 *                          (the first number in its address.)
	 *
	 * @return OpStatus::OK on success, or OpStatus::ERR_NOT_SUPPORTED if
	 *         this method is invoked on a component manager other than the
	 */
	OP_STATUS PeerGone(int component_manager);

	/**
	 * Create a new component of the given type.
	 *
	 * This method should only be called to create the first component
	 * in the initial component manager.
	 *
	 * @param[out] component A ptr to the created component is stored here.
	 * @param type The type of component to create
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS CreateComponent(OpComponent*& component, OpComponentType type);

	/**
	 * Destroy the given component.
	 *
	 * Used by components to delete themselves. May not be destroyed
	 * immediately.
	 *
	 * @param component Component to destroy
	 *
	 * @retval OpStatus::OK on success.
	 */
	OP_STATUS DestroyComponent(OpComponent* component);

	/**
	 * Deliver a message to this component manager.
	 *
	 * WARNING! OpComponentManager is not thread-safe. The caller must
	 * ensure that no two threads simultaneously call this method.
	 *
	 * @param message The message to deliver.
	 * @note This function takes ownership of the given \c message, and the
	 * caller MUST NOT delete the message, as it will be automatically
	 * deleted by the messaging infrastructure.
	 *
	 * @return OpStatus::OK or an error from an internal ReceiveMessage()
	 *         call.
	 */
	OP_STATUS DeliverMessage(OpTypedMessage* message) { return ReceiveMessage(message); }

	/** Get the time elapsed since "something".
	 *
	 * This is using the PI OpSystemInfo::GetRuntimeMS(), and exists here
	 * because it may be used before and after the PI module has been initialized, and
	 * thus when g_op_system_info is invalid.
	 *
	 * @return The number of milliseconds since "something"
	 */
	virtual double GetRuntimeMS() { return m_op_time_info->GetRuntimeMS(); }

#ifdef AVERAGE_MESSAGE_LAG
	/**
	 * Obtain average messaging lag. Used for throttling.
	 *
	 * Returns the average lag of all lagged messages in the queue (the
	 * time since the messages should be already triggered). The calculated
	 * value along with the value of
	 * PrefsCollectionCore::LagThresholdForAnimationThrottling are used to
	 * determine whether the throttling of animated content should be enabled or
	 * not. This method has complexity of O(n) so it shouldn't be called too
	 * often (however the optimistic complexity is O(1)).
	 */
	double GetAverageLag() const;
#endif // AVERAGE_MESSAGE_LAG

	/**
	 * Send a message from this component manager or one of its children.
	 *
	 * If the message is not self-addressed, it is forwarded to the
	 * platform's OpComponentPlatform::SendMessage().
	 *
	 * SendMessage() assumes ownership of the given \c message, and the
	 * caller MUST NOT delete the message, as it will be automatically
	 * deleted by the messaging infrastructure.
	 *
	 * Also, with default flags (includes #SENDMESSAGE_OOM_IF_NULL), passing
	 * message == NULL makes SendMessage() return OpStatus::ERR_NO_MEMORY. If
	 * #SENDMESSAGE_OOM_IF_NULL is disabled and message == NULL, SendMessage()
	 * returns OpStatus::ERR_NULL_POINTER.
	 *
	 * @see OpMessenger::SendMessageFlags
	 *
	 * @param message Message. If NULL, OpStatus::ERR_NO_MEMORY or
	 *         OpStatus::ERR_NULL_POINTER is returned, depending on \c flags.
	 * @param flags Bitwise OR-combination of OpMessenger::SendMessageFlags
	 *         enum values. See OpMessenger::SendMessageFlags for details.
	 *
	 * @return OpStatus::OK on success. Otherwise, some kind of OpStatus
	 *         error code. Note the above discussion about ownership and the
	 *         given \c flags.
	 */
	OP_STATUS SendMessage(OpTypedMessage* message, unsigned int flags = SENDMESSAGE_OOM_IF_NULL);

	/**
	 * Remove a set of messages from the component manager's inbox.
	 *
	 * @param selection Predicate determining the message selection.
	 *
	 * @return Number of messages removed.
	 */
	unsigned int RemoveMessages(OpTypedMessageSelection* selection);

	/**
	 * Remove the first message from the component manager's inbox which matches
	 * the specified filter.
	 *
	 * @param selection Predicate determining the message selection.
	 *
	 * @return Number of messages removed.
	 */
	unsigned int RemoveFirstMessage(OpTypedMessageSelection* selection);

	/**
	 * Add a message filter to be used by the message dispatcher.
	 *
	 * This is a helper method to use with GetMessage(). When part of a
	 * component must wait for an externally originating message, but cannot
	 * allow the rest of the component to handle incoming messages, a filter may
	 * be installed to temporarily restrict message processing.
	 *
	 * The filter must be removed using RemoveMessageFilter() for operations to
	 * resume as normal.
	 *
	 * @param filter The filter to install. The selection defines messages that
	 * may be processed. The filter object must remain alive until removed.
	 *
	 * @return OpStatus:: OK if the filter was successfully installed.
	 * @see RemoveMessageFilter()
	 * @see MessageIsBlocked()
	 */
	OP_STATUS AddMessageFilter(OpTypedMessageSelection* filter);

	/**
	 * Check if a specific message filter is in place.
	 *
	 * @param filter The filter to check for.
	 *
	 * @return True if the filter is present.
	 */
	bool HasMessageFilter(OpTypedMessageSelection* filter) const;

	/**
	 * Remove a message filter installed by AddMessageFilter().
	 *
	 * @param filter The filter to remove.
	 *
	 * @return OpStatus:: OK if the filter was successfully removed.
	 */
	OP_STATUS RemoveMessageFilter(OpTypedMessageSelection* filter);

	/**
	 * Retrieve component by address.
	 *
	 * @param address The local address of the component.
	 *
	 * @return Pointer to OpComponent or NULL if not found. The component object
	 *         remains owned by this manager.
	 */
	OpComponent* GetComponent(int address);

	/**
	 * Value type used by LoadValue and StoreValue.
	 */
	struct Value
	{
		virtual ~Value() {}
	};

	/**
	 * Load value by key.
	 *
	 * This mechanism is intended for non-performance-sensitive globals. To prevent
	 * collisions, keywords should be prefixed by the module storing the value.
	 *
	 * @param key Keyword.
	 * @param[out] out_value On success, pointer to retrieved value.
	 *
	 * @return OpStatus::OK on successful retrieval, or OpStatus::ERR_NO_SUCH_RESOURCE
	 *         if the key does not exist.
	 */
	OP_STATUS LoadValue(const char* key, Value*& out_value);

	/**
	 * Store value by key.
	 *
	 * This mechanism is intended for non-performance-sensitive globals. To prevent
	 * collisions, keywords should be prefixed by the module storing the value.
	 *
	 * If a value is already stored using the provided key, it will be deleted and
	 * replaced. When the component manager is destroyed, all stored values will be
	 * deleted AFTER all components have been destroyed.
	 *
	 * @param key Keyword.
	 * @param value Value to store.
	 *
	 * @return OpStatus::OK on successful storage and OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS StoreValue(const char* key, Value* value);

	/** Returns the timeout in milliseconds until the next message needs to be
	 * handled or UINT_MAX if the inbox is empty. The returned value can e.g. be
	 * used as a return value for RunSlice() or as \c timeout argument to
	 * OpComponentPlatform::ProcessEvents() or as \c limit argument to
	 * OpComponentPlatform::RequestRunSlice().
	 * @param now If set, use this as the current timestamp instead of
	 *        calling g_op_system_info->GetRuntimeMS().
	 * @return Number of milliseconds until the first message in the inbox
	 *         should be processed. UINT_MAX if the inbox is empty.
	 */
	unsigned int NextTimeout(double now = 0) const;

protected:
	/**
	 * Protected constructor. Use Create().
	 */
	OpComponentManager(int address_cm, OpComponentPlatform* platform);

	/**
	 * Allocate resources and initialize the component manager.
	 *
	 * Must be called before RunSlice().
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR on failure to initialize protobuf.
	 */
	OP_STATUS Construct();

	/**
	 * Return TRUE if we are the initial component manager.
	 */
	inline BOOL IsInitialManager() const
	{
		return GetAddress().cm == 0;
	}

	/**
	 * Return TRUE if the given address belongs to a component manager.
	 */
	inline BOOL IsManager(const OpMessageAddress& address) const
	{
		return address.co == 0;
	}

	/**
	 * Return TRUE if given address belongs inside this component manager.
	 *
	 * When SendMessage() is invoked, messages destined for a local address
	 * are inserted directly into the local message queue, while messages
	 * destined for other components are delivered to the platform.
	 */
	inline BOOL IsLocal(const OpMessageAddress& address) const
	{
		return address.cm == GetAddress().cm;
	}

	/**
	 * Check whether the given address exists within this object.
	 *
	 * Return TRUE if the given address refers to an existing component
	 * manager, component or channel within this object.
	 *
	 * @param address Address to check if exists within this object.
	 * @returns TRUE if address exists, FALSE otherwise.
	 */
	BOOL AddressExists(const OpMessageAddress& address);

	/**
	 * Check if a message is blocked by an installed message filter.
	 *
	 * @param message The message to check against filters.
	 *
	 * @return True if the message is blocked.
	 * @see AddMessageFilter()
	 */
	bool MessageIsBlocked(const OpTypedMessage* message) const;

	/**
	 * Retrieve the first message eligible for delivery from the message queue.
	 *
	 * The message is removed from the queue.
	 *
	 * @param now The time at which to extract the first message due.
	 *
	 * @return An OpTypedMessage ready for delivery, or NULL if none are.
	 */
	OpTypedMessage* GetMessage(double now);

	/**
	 * Retrieve a registered component.
	 *
	 * @param address Component address.
	 *
	 * @return OpComponent pointer, or NULL if not found.
	 */
	OpComponent* GetComponent(const OpMessageAddress& address) const;

	/**
	 * Receive a message destined for this manager.
	 *
	 * Deliver a message to this component manager, or some component or
	 * channel contained within. Called by either the underlying platform,
	 * or (in case of internal messages) directly from SendMessage().
	 *
	 * WARNING! OpComponentManager is not thread-safe. The caller must
	 * ensure that no two threads simultaneously call this method.
	 *
	 * @param message The message to deliver.
	 * @note This function takes ownership of the given \c message, and the
	 * caller MUST NOT delete the message, as it will be automatically
	 * deleted by the messaging infrastructure.
	 *
	 * @return OpStatus::OK or,
	 *         OpStatus::ERR_NO_SUCH_RESOURCE if the message lacks a
	 *         destination address (in this case the message is deallocated
	 *         and discarded).
	 */
	OP_STATUS ReceiveMessage(OpTypedMessage* message);

	/**
	 * Deliver a message to the actual message queue.
	 * Used internally when removing message filters, or
	 * when receiving messages.
	 */
	void AddToInbox(OpTypedMessage* message);

	/** @name Reimplementing OpMessenger
	 * @{ */

	/**
	 * Process an incoming component manager message.
	 *
	 * An instance of this class processes an OpComponentRequestMessage, an
	 * OpPeerConnectedMessage and an OpPeerDisconnectedMessage. Any other
	 * message is passed on to the associated OpMessageListener (see
	 * OpMessenger::AddMessageListener()).
	 *
	 * @see HandleComponentRequest(), HandlePeerConnected(),
	 *      HandlePeerDisconnected().
	 *
	 * @param message The message to be processed.
	 *
	 * @return OpStatus::OK.
	 */
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

	/** @} */ // Reimplementing OpMessenger

	/**
	 * Assign and return the next appropriate OpComponent instance address.
	 *
	 * @return Next available address for a local OpComponent instance.
	 */
	OpMessageAddress AssignNextComponentAddress();

	/**
	 * Begin a new currently running component context.
	 *
	 * @param component The currently running component
	 * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY otherwise.
	 */
	OP_STATUS PushRunningComponent(OpComponent* component);

	/**
	 * End a currently running component context.
	 */
	void PopRunningComponent();

	/**
	 * Destroy the given component.
	 *
	 * Deregisters the component from the internal data structures but
	 * does not delete it, as it's called from OpComponent's destructor.
	 *
	 * @param component Component to destroy
	 *
	 * @retval OpStatus::OK on success.
	 */
	OP_STATUS Destroy(OpComponent* component);

	/**
	 * Disconnect all channels to a component or component manager.
	 *
	 * @param address The address of the entity to forget.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS DisconnectMessenger(const OpMessageAddress& address);

	/// Forward a message to the appropriate messenger for processing.
	void DispatchMessage(const OpTypedMessage *message);

	/// Handle request for a local component of a given type.
	OP_STATUS HandleComponentRequest(OpComponentRequestMessage* message);

	/// Handle notification of a created component.
	OP_STATUS HandlePeerConnected(OpPeerConnectedMessage* message);

	/// Handle notification of a disconnected component or component manager.
	OP_STATUS HandlePeerDisconnected(OpPeerDisconnectedMessage* message);

#ifdef _DEBUG
	/// Verify that all the message queue satisfies all requirements.
	void ValidateInbox();
#endif // _DEBUG

	/// Underlying platform implementation.
	OpComponentPlatform* m_platform;

	/// List of incoming messages awaiting procesing.
	List<OpTypedMessage> m_inbox;

	/// Provide fast access to the first delayed message in the inbox.
	OpTypedMessage* m_first_delayed_message;

	/// Provide fast access to the first message in the inbox not blocked.
	OpTypedMessage* m_first_unblocked_message;

	/// Next appropriate 'co' number for a local component address.
	int m_next_component_number;

	/// Local components.
	OpINT32HashTable<OpComponent> m_components;

	/// Stack of running components. Managed with Push/PopRunningComponent().
	struct RunningComponentStack {
		OpComponent* component;
		struct RunningComponentStack* next;
	};

	/// Currently running component. Used for g_opera lookup.
	struct RunningComponentStack* m_component_stack;

	/// Pointer to the protobuf descriptor set, allocated in constructor.
	OpProtobufDescriptorSet *m_protobuf_descriptors;

	/// List of peers.
	OpINT32Vector m_peers;

	/// Flag stating whether we have registered with the initial component manager.
	bool m_connected;

	/// Active message filters. @see AddMessageFilter().
	OtlList<OpTypedMessageSelection*> m_message_filters;

	/// Allow ~OpComponent to access Destroy(OpComponent*).
	friend class OpComponent;

	/// Allow OpComponentActivator to push components onto the running component stack.
	friend class OpComponentActivator;

	/// Hold time info, basically to be able to tell the time before and after pi is initialized.
	OpTimeInfo* m_op_time_info;

	/// Stack of items to cleanup on LEAVE. @see CleanupItem for more information.
	friend class CleanupItem;
	CleanupItem* m_cleanup_stack;

	/// Global values. See LoadValue(), StoreValue().
	typedef OtlMap<const char*, Value*> ValueMap;
	ValueMap m_values;
};

/**
 * Message selection interface.
 *
 * Can be used by OpComponentManager::RemoveMessages() or
 * OpComponentManager::AddMessageFilter().
 */
class OpTypedMessageSelection
{
public:
	/**
	 * Define whether the message is part of the selection.
	 *
	 * @return TRUE if the message is part of the selection, otherwise FALSE.
	 */
	virtual BOOL IsMember(const OpTypedMessage* message) const = 0;
};

/**
 * Abbreviated access to the current component. Exists for symmetry with g_opera.
 */
#define g_component g_component_manager->GetRunningComponent()


/**
 * Helper class keeping a given component on the running stack.
 */
class OpComponentActivator
{
public:
	OpComponentActivator();
	~OpComponentActivator();

	OP_STATUS Activate(OpComponent* component);

protected:
	bool m_active;
};


#endif /* MODULES_HARDCORE_COMPONENT_OPCOMPONENTMANAGER_H */
