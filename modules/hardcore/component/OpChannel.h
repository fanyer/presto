/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPCHANNEL_H
#define MODULES_HARDCORE_COMPONENT_OPCHANNEL_H

#include "modules/hardcore/component/OpMessenger.h" // Need OpMessenger

class OpComponent; // Need OpComponent* for OpChannel::m_component
class OpComponentManagerResponseMessage;

/** An OpChannel can be used to send and receive messages.
 *
 * A channel can be used to communicate between different OpComponent instances
 * or within the same OpComponent.
 *
 * A channel has a local address and a destination address. The destination
 * address is the address of a remote end. The remote end is usually another
 * channel in another component.
 *
 * A channel may exist in one of three states:
 * -# In the OpChannel::UNDIRECTED state, it may receive messages but cannot
 *    send. See also \ref ex_channel_undirected.
 * -# In the OpChannel::DIRECTED state, it may send messages to a remote
 *    end and receive a reply. The remote end is usually not aware of the
 *    directed channel. See also \ref ex_channel_directed.
 * -# In the OpChannel::CONNECTED state, it may receive messages, and send
 *    messages to a symmetrically opposed channel which is also in the
 *    OpChannel::CONNECTED state. See also \ref ex_channel_connected.
 *
 * When SendMessage() is invoked on a channel, the message is sent to the
 * remote end. Replies may be received by adding a message listener to the
 * channel.
 *
 * If two channel objects A and B are OpChannel::CONNECTED, destroying or
 * redirecting A will cause an OpPeerDisconnectedMessage to be sent to B,
 * causing B to transition from OpChannel::CONNECTED to OpChannel::UNDIRECTED.
 *
 *
 * @section ex_channel_undirected Example: create a channel for listening to messages
 *
 * The first example shows the simplest use-case: Some component creates a
 * channel to be able to register an OpMessageListener and receive messages:
 *
 * Define a message-listener class which can handle some message:
 * \code
 * class MyMessageListener : public OpMessageListener {
 * public:
 *     ...
 *     virtual OP_STATUS ProcessMessage(const OpTypedMessage* message)
 *     {
 *         switch (message->GetType()) {
 *         case OpSomeMessage::Type:
 *             return OnSomeMessage(OpSomeMessage::Cast(message));
 *         ...
 *         }
 *     }
 *
 * private:
 *     OP_STATUS OnSomeMessage(const OpSomeMessage* message)
 *     {
 *         // Do something
 *         ...;
 *     }
 * };
 * \endcode
 *
 * The following function creates a new OpChannel instance in the specified
 * OpComponent and register the specified listener.
 * \code
 * OpChannel* CreateMyListenerChannel(OpComponent* component, MyMessageListener* listener)
 * {
 *     OpAutoPtr<OpChannel> channel(OP_NEW(OpChannel, (OpMessageAddress(), component)));
 *     RETURN_VALUE_IF_NULL(channel.get(), NULL);
 *     RETURN_VALUE_IF_ERROR(channel->Construct(), NULL);
 *     RETURN_VALUE_IF_ERROR(channel->AddMessageListener(listener), NULL);
 *     return channel.release();
 * }
 * \endcode
 *
 * On constructing the channel, the channel receives an address. The channel is
 * returned to the caller and its address can be passed to other components
 * which may then send \c OpSomeMessages (or any other message which is handled
 * by the listener) to that address. The message listener will wait for messages
 * and "do something" on receiving such a message.
 * \code
 * OpComponent* component_1 = ...;
 * MyMessageListener* listener = ...;
 * OpChannel* channel_a = CreateMyListenerChannel(component_1, listener);
 * OpMessageAddress address_a = channel_a->GetAddress();
 * OP_ASSERT(address_a.IsValid());
 * \endcode
 *
 * @note
 *   - The channel in this example is in state OpChannel::UNDIRECTED, i.e., the
 *     channel may receive messages, but it cannot be used to send messages. A
 *     call to \c channel->SendMessage() will return the error
 *     OpStatus::ERR_NO_SUCH_RESOURCE.
 *   - In this example we don't need to delete the \c channel on success. The
 *     OpChannel instance is stored in the OpComponent instance \c component and
 *     when the \c component is deleted, the \c channel will be deleted as
 *     well.
 *   - If the \c listener instance may be deleted before the \c channel is
 *     deleted, then the \c listener instance should keep a pointer to the \c
 *     channel and call OpChannel::RemoveMessageListener() when the \c listner
 *     is destroyed.
 *   - There is a convenience method OpComponent::CreateChannel() which does
 *     about the same as \c CreateMyListenerChannel() in the above example.
 *
 * Sending a message to the above listener can now be done as:
 * \code
 * OpComponent* component_2 = ...;
 * OpMessageAddress address_a = ...; // address to the above created channel
 * OP_ASSERT(address_a.IsValid());
 * OpMessageAddress empty;
 * component_2->SendMessage(OpSomeMessage::Create(..., empty, address_a));
 * \endcode
 * Here we create an \c OpSomeMessage instance with an empty source address and
 * use the \c address_a of the other component's channel as destination.
 *
 *
 * @section ex_channel_directed Example: Use a directed channel for sending a simple message and receiving a reply
 *
 * This example shows how a component can create an OpChannel::DIRECTED channel
 * to send a message to another component and receive a reply on that channel.
 *
 * First let's extend the listener of the first example (see \ref
 * ex_channel_undirected) to be able to handle a message and send some reply:
 * \code
 * class MyMessageListener : public OpMessageListener {
 * public:
 *     ...
 *     virtual OP_STATUS ProcessMessage(const OpTypedMessage* message)
 *     {
 *         switch (message->GetType()) {
 *         case OpDoAndReplyMessage::Type:
 *             return OnDoAndReplyMessage(OpDoAndReplyMessage::Cast(message));
 *         ...
 *         }
 *     }
 *
 * private:
 *     OpComponent* m_component;
 *
 *     OP_STATUS OnDoAndReplyMessage(const OpDoAndReplyMessage* message)
 *     {
 *         // Do something
 *         ...;
 *         return m_component->ReplyTo(*message, OpReplyMessage::Create(...));
 *     }
 * };
 * \endcode
 * @note Here the \c MyMessageListener needs an instance of its OpComponent to
 *       send a reply. That instance should be assigned on creating the listener
 *       instance.
 *
 * The listener handles an \c OpDoAndReplyMessage by doing something and sending
 * an \c OpReplyMessage to the source address of the message. To send a reply,
 * the listener instance does not need to have a channel to the sender.
 *
 * In the first component (\c component_1) we create an OpChannel::UNDIRECTED
 * channel with such a listener instance as we did in the above example \ref
 * ex_channel_undirected (however, now we use the convenience method
 * OpComponent::CreateChannel()):
 * \code
 * OpComponent* component_1 = ...;
 * OpMessageListener* listener = OP_NEW(MyMessageListener, (component_1, ...));
 * OpChannel* channel_a = component_1->CreateChannel(OpMessageAddress(), listener);
 * OpMessageAddress address_a = channel_a->GetAddress();
 * OP_ASSERT(address_a.IsValid());
 * \endcode
 *
 * Another component (\c component_2) can define an OpMessageListener class
 * which handles the \c OpReplyMessage (I skip the example code for such a
 * listener, it will look very much like the other examples). In \c component_2
 * we create an OpChannel::DIRECTED channel by specifying the destination
 * address for the channel.
 * \code
 * OpComponent* component_2 = ...;
 * OpMessageListener* reply_listener = ...;
 * OpMessageAddress address_a = ...; // Address of channel_a in component_1
 * OP_ASSERT(address_a.IsValid());
 * OpChannel* channel_b = component_2->CreateChannel(address_a, reply_listener);
 * channel_b->SendMessage(OpDoAndReplyMessage::Create(...));
 * \endcode
 *
 * Now \c channel_b is a directed OpChannel in \c component_2, which points to
 * \c channel_a in \c component_1, but \c channel_a does not point back to \c
 * channel_b:
 * \dot
 *   // component_1    component_2
 *   // channel_a -----o channel_b
 *   digraph {
 *       fontsize=10;
 *       node [shape=box, fontsize=10, width=.25, height=.25];
 *       subgraph cluster1 {
 *           label="component_1"; style=filled; color=lightgrey;
 *           node [style=filled,color=white];
 *           channel_a
 *       }
 *       subgraph cluster2 {
 *           label="component_2"; color=blue;
 *           node [style=filled];
 *           channel_b
 *       }
 *       {rank=same; channel_a; channel_b;}
 *       channel_b -> channel_a [arrowtail="odot", dir="back"];
 *   }
 * \enddot
 * However \c channel_a can still reply to messages received from \c channel_a.
 *
 * @note In this example \c channel_a was an undirected channel. But we can use
 *       any kind of channel, i.e., \c channel_a may be OpChannel::DIRECTED or
 *       OpChannel::CONNECTED (directed or connected to some other channel in a
 *       third component). We only need the address of that channel and the fact
 *       that the channel has a listener which handles the message \c
 *       component_2 sends to that channel.
 *
 *
 * @section ex_channel_connected Example: create a connected channel
 *
 * In this example we have two components (\c component_1 and \c component_2)
 * and we want to create a two-way channel to communicate between these
 * components. To establish the connection we need a handshake between both
 * components. There is no generic handshake message, instead the components
 * should agree on some initial message which already contains some actual data.
 *
 * So let the first component (\c component_1) define a message-listener which
 * can handle some initial message:
 * \code
 * class MyMessageListener : public OpMessageListener {
 * public:
 *     ...
 *     virtual OP_STATUS ProcessMessage(const OpTypedMessage* message)
 *     {
 *         switch (message->GetType()) {
 *         case OpMyFirstMessage::Type:
 *             return OnMyFirstMessage(OpMyFirstMessage::Cast(message));
 *         ...
 *         }
 *     }
 *
 * private:
 *     OpComponent* m_component;
 *
 *     OP_STATUS OnMyFirstMessage(const OpMyFirstMessage* message);
 * };
 * \endcode
 *
 * In the first component (\c component_1) we start with creating an undirected
 * channel with the above defined listener:
 * \code
 * OpComponent* component_1 = ...;
 * OpMessageListener* listener = OP_NEW(MyMessageListener, (component_1, ...));
 * OpChannel* channel_0 = component_1->CreateChannel(OpMessageAddress(), listener);
 * OpMessageAddress address_0 = channel_0->GetAddress();
 * OP_ASSERT(address_0.IsValid());
 * \endcode
 * Now \c channel_0 is an undirected (OpChannel::UNDIRECTED) channel in \c
 * component_1, though it doesn't matter if \c channel_0 was directed or
 * connected to some other channel in a third component, it only matters that
 * the channel's message-listener can handle the handshake message
 * \c OpMyFirstMessage.
 *
 * In the second component (\c component_2) we define another OpMessageListener
 * class which handles any incoming message which may be sent from the first
 * component and it handles the OpPeerConnectedMessage.
 * \code
 * class SecondMessageListener : public OpMessageListener {
 * public:
 *     ...
 *     virtual OP_STATUS ProcessMessage(const OpTypedMessage* message)
 *     {
 *         switch (message->GetType()) {
 *         case OpPeerConnectedMessage::Type:
 *             return OnPeerConnected(OpPeerConnectedMessage::Cast(message));
 *         ...
 *         }
 *     }
 *
 * private:
 *     OP_STATUS OnPeerConnected(const OpPeerConnectedMessage* message);
 * };
 * \endcode
 *
 * Now we can start the handshake in the second component by creating a directed
 * channel to the first component and sending an \c OpMyFirstMessage:
 * \code
 * OpComponent* component_2 = ...;
 * OpMessageAddress address_0 = ...; // Address of channel_0 in component_1
 * OP_ASSERT(address_0.IsValid());
 * OpMessageListener* listener = OP_NEW(SecondMessageListener, (...));
 * OpChannel* channel_b = component_2->CreateChannel(address_0, listener);
 * channel_b->SendMessage(OpMyFirstMessage::Create(...));
 * \endcode
 *
 * Now \c channel_b is directed to \c channel_0 (though \c channel_0 has not
 * changed):
 * \dot
 *   // component_1    component_2
 *   // channel_0 -----o channel_b
 *   digraph {
 *       fontsize=10;
 *       node [shape=box, fontsize=10, width=.25, height=.25];
 *       subgraph cluster1 {
 *           label="component_1"; style=filled; color=lightgrey;
 *           node [style=filled,color=white];
 *           channel_0
 *       }
 *       subgraph cluster2 {
 *           label="component_2"; color=blue;
 *           node [style=filled];
 *           channel_b
 *       }
 *       {rank=same; channel_0; channel_b;}
 *       channel_b -> channel_0 [arrowtail="odot", dir="back"];
 *   }
 * \enddot
 *
 * On receiving the message (\c OpMyFirstMessage), the message-listener in \c
 * component_1 creates a new OpChannel and connects it to the first channel:
 * \code
 * OP_STATUS MyMessageListener::OnMyFirstMessage(const OpMyFirstMessage* message)
 * {
 *     OpChannel* channel_a = m_component->CreateChannel(message->GetSrc(), this);
 *     RETURN_OOM_IF_NULL(channel_a);
 *     return channel_a->Connect();
 * }
 * \endcode
 * Now \c channel_a is a new OpChannel which has the same listener instance as
 * \c channel_0 (in an actual implementation you may want to split handling the
 * initial message and handling other messages into two different message
 * listener classes). \c channel_a is connected (OpChannel::CONNECTED) to \c
 * channel_b in \c component_2:
 * \dot
 *   // component_1    component_2
 *   // channel_0 -----o channel_b
 *   // channel_a -------^
 *   digraph {
 *       fontsize=10;
 *       node [shape=box, fontsize=10, width=.25, height=.25];
 *       subgraph cluster1 {
 *           label="component_1"; style=filled; color=lightgrey;
 *           node [style=filled,color=white];
 *           channel_0 -> channel_a [dir=none,style=dotted];
 *       }
 *       subgraph cluster2 {
 *           label="component_2"; color=blue;
 *           node [style=filled];
 *           channel_b
 *       }
 *       {rank=same; channel_a; channel_b;}
 *       channel_b -> channel_0 [arrowtail="odot", dir="back"];
 *       channel_a -> channel_b;
 *   }
 * \enddot
 * \c channel_b is not yet connected, but the call to \c channel_a->Connect()
 * sends an OpPeerConnectedMessage from \c channel_a to \c channel_b.
 *
 * When \c channel_b receives the OpPeerConnectedMessage, it changes the
 * destination address to \c channel_a its state to OpChannel::CONNECTED:
 * \dot
 *   // component_1    component_2
 *   // channel_0
 *   // channel_a <----> channel_b
 *   digraph {
 *       fontsize=10;
 *       node [shape=box, fontsize=10, width=.25, height=.25];
 *       subgraph cluster1 {
 *           label="component_1"; style=filled; color=lightgrey;
 *           node [style=filled,color=white];
 *           channel_0 -> channel_a [dir=none,style=dotted];
 *       }
 *       subgraph cluster2 {
 *           label="component_2"; color=blue;
 *           node [style=filled];
 *           channel_b
 *       }
 *       {rank=same; channel_a; channel_b;}
 *       channel_a -> channel_b [dir="both"];
 *   }
 * \enddot
 * When the message-listener in \c channel_b receives the OpPeerConnectedMessage
 * it knows that the remote end is now ready to receive messages through the
 * connected channel.
 * \code
 * OP_STATUS SecondMessageListener::OnPeerConnected(const OpPeerConnectedMessage* message)
 * {
 *     // now channel_b is connected to channel_a ...
 *     ....
 * }
 * \endcode
 */
class OpChannel
	: public OpMessenger
{
public:
	/**
	 * Primary constructor.
	 *
	 * @param destination Destination address of messages sent on this channel.
	 * @param component For component-internal use. Owning component.
	 */
	OpChannel(const OpMessageAddress& destination = OpMessageAddress(), OpComponent* component = NULL);
	virtual ~OpChannel();

	/**
	 * Secondary constructor.
	 *
	 * Aquires an address and registers with the owning component.
	 *
	 * @param root For component-internal use. True if this channel is the
	 *        component's root channel.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Construct(bool root = false);

	/**
	 * Return the destination address of this channel.
	 */
	virtual const OpMessageAddress& GetDestination() const { return m_destination; }

	/**
	 * Check if the secondary constructor completed successfully.
	 * @see Construct()
	 */
	bool IsInitialized() const { return m_state > UNINITIALIZED; }

	/**
	 * Check if this channel has a valid destination address.
	 * @see ex_channel_directed
	 */
	bool IsDirected() const { return m_state >= DIRECTED; }

	/**
	 * Set the destination address of messages sent through this channel.
	 *
	 * If the channel is already connected, the previous destination will receive a disconnect
	 * message.
	 *
	 * @param destination Address to be set as destination.
	 * @see ex_channel_directed
	 */
	void Direct(const OpMessageAddress& destination);

	/**
	 * Reset the channel to an undirected state.
	 *
	 * If the channel is connected, the destination will receive a disconnect message.
	 */
	void Undirect() { OP_ASSERT(IsInitialized()); SetState(UNDIRECTED); }

	/**
	 * Check if this channel is connected.
	 *
	 * @see Connect(), #CONNECTED
	 * @see \ref ex_channel_connected
	 */
	bool IsConnected() const { return m_state >= CONNECTED; }

	/**
	 * Connect this channel to the channel at the destination address.
	 *
	 * @see \ref ex_channel_connected
	 *
	 * \b WARNING: This is intended to conclude a connect handshake, and will
	 * rewrite the destination address of the remote end. Only call this method
	 * after receiving a message intended to initiate a handshake.
	 *
	 * On success, both this channel and the remote channel will transition
	 * to the #CONNECTED state.
	 *
	 * @return See return values of SendMessage().
	 */
	OP_STATUS Connect();

	/* Reimplementing OpMessenger */

	/** Send a message to the remote end of this channel.
	 *
	 * This is a reimplementation of OpMessenger::SendMessage(), which
	 * sets the message source/destination addresses to the local/destination
	 * addresses of the channel. There is therefore no need to set message
	 * addresses before calling this method.
	 *
	 * If the channel is not directed to a destination address (state
	 * #UNDIRECTED), this method returns OpStatus::ERR_NO_SUCH_RESOURCE.
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
	 * @param message Message to send. If NULL, OpStatus::ERR_NO_MEMORY or
	 *        OpStatus::ERR_NULL_POINTER is returned, depending on \c flags.
	 * @param flags Bitwise OR-combination of OpMessenger::SendMessageFlags
	 *        enum values. See OpMessenger::SendMessageFlags for details.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE if channel is not connected to a
	 *         remote end.
	 * @retval ... Otherwise, some kind of OpStatus error code. Note the above
	 *         discussion about ownership and the given \c flags.
	 *
	 * @see \ref sending_messages_1
	 * @see \ref sending_messages_2
	 * @see \ref handling_messages
	 */
	virtual OP_STATUS SendMessage(OpTypedMessage* message, unsigned int flags = SENDMESSAGE_OOM_IF_NULL);

	/**
	 * Process an incoming message.
	 *
	 * This distributes incoming messages to the registered listeners (by
	 * redirecting to the OpMessenger base class implementation), except
	 * that it handles some infrastructure-level internal messages for
	 * administering the message and addressing details.
	 *
	 * @param message The message to be processed.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

	/**
	 * Add a listener to this messenger.
	 *
	 * This is a reimplementation of OpMessenger::AddMessageListener(), which
	 * immediately calls OpMessageListener::ProcessMessage() with an
	 * OpPeerConnectedMessage on the specified listener instance if this channel
	 * is already connected to the remote end. Otherwise, it defers to the base
	 * class implementation.
	 *
	 * The caller must ensure to call RemoveMessageListener() If the specified
	 * OpMessageListener instance is deleted before the channel is destroyed.
	 *
	 * @param listener Object wishing to receive messages from this channel.
	 * @param silent If true and the channel is already connected, then no
	 *        OpPeerConnectedMessage will be generated.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS AddMessageListener(OpMessageListener* listener, bool silent);
	virtual OP_STATUS AddMessageListener(OpMessageListener* listener) { return AddMessageListener(listener, false); }

	enum Identifier
	{
		/** Components (x.y.0) and component managers (x.0.0) use this channel
		 *  identifier. */
		NONE = 0,

		/** A component's root channel is the channel through which a component
		 *  was requested. When this channel is disconnected, the component is
		 *  destroyed.
		 * @see IsRoot()
		 * @see OpComponent::RequestComponent() */
		ROOT = 1,

		/** First unreserved number. */
		FIRST
	};

	enum State
	{
		/** The channel object has been created but the secondary constructor
		 * (Construct()) has not been run, and so the object has neither a local
		 * address nor a destination address. This is the initial state. */
		UNINITIALIZED,

		/** The channel has a local address, but no destination address.
		 * The channel may receive messages, but it cannot be used to send
		 * messages.
		 * @see \ref ex_channel_undirected
		 * @see SendMessage()
		 *
		 * A #DIRECTED or #CONNECTED channel may enter this state when the
		 * remote end disconnects, i.e., the remote end sends an
		 * OpPeerDisconnectedMessage. */
		UNDIRECTED,

		/** The channel has a local address and a destination address, but the
		 * remote end does not (yet) point back to this channel.
		 *
		 * A directed channel can be used to send messages in one direction only
		 * and possibly send a direct answer back.
		 * @see \ref ex_channel_directed
		 *
		 * A channel use this as an intermediate state on creating a #CONNECTED
		 * channel. The channel becomes connected, when the remote end sends an
		 * OpPeerConnectedMessage by calling Connect().
		 * @see \ref ex_channel_connected */
		DIRECTED,

		/** The channel has a local address and a destination address, and the
		 * remote end has a channel which points back to this channel.
		 *
		 * A channel enters this state when a connection handshake was
		 * completed.
		 * @see \ref ex_channel_connected
		 * @see Connect() */
		CONNECTED
	};

	/** Check if this channel is a root channel.
	 *
	 * If a new OpComponent is created, the requester of the component receives
	 * the root-channel to the component. If the requester deletes the root
	 * channel, the component is deleted.
	 *
	 * @see #ROOT
	 * @see OpComponent::RequestComponent()
	 * @see OpComponent::OnRootChannelDisconnected()
	 */
	bool IsRoot() { return m_address.IsValid() && m_address.ch == ROOT; }

protected:
	/** Update \c m_state to the specified \p new_state.
	 *
	 * @param new_state is the new State.
	 * @param send_disconnect If the channel is currently IsConnected() and the
	 *        \p new_state is neither #DIRECTED nor #UNDIRECTED, then an
	 *        OpPeerDisconnectedMessage is sent to the peer (unless this
	 *        parameter is set to false) to inform the peer that this part is
	 *        no longer connected to the peer. This parameter should only be set
	 *        to false if the caller knows that the peer no longer exists -
	 *        e.g. on receiving an OpPeerDisconnectedMessage from the peer.
	 */
	void SetState(State new_state, bool send_disconnect = true);

	/// The component to which we belong.
	OpComponent* m_component;

	/// Address of remote end.
	OpMessageAddress m_destination;

	/// State of connection. See #State.
	State m_state;

	/// OpComponent may call SetState().
	friend class OpComponent;
};

#endif /* MODULES_HARDCORE_COMPONENT_OPCHANNEL_H */
