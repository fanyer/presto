/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPMESSENGER_H
#define MODULES_HARDCORE_COMPONENT_OPMESSENGER_H

#include "modules/hardcore/component/OpTypedMessage.h" // Need OpMessageAddress
#include "modules/otl/list.h"
#include "modules/util/OpSharedPtr.h"


/**
 * This is the base class for all objects interested in processing messages.
 *
 * It simply has one pure virtual method - ProcessMessage() - that is called
 * for each message to be processed.
 *
 * @see OpMessenger::AddMessageListener() and
 *      OpMessenger::RemoveMessageListener() for more details on how to connect
 *      OpMessageListener instances to the messaging infrastructure.
 * @see \ref component_messages on how to send and handle messages.
 */
class OpMessageListener
{
public:
	/**
	 * Process a single incoming message.
	 *
	 * @param message Message.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message) = 0;

	virtual ~OpMessageListener(void) { }
};


class OpMessageLogger;

/**
 * This is the base class for objects that want to send and receive messages.
 *
 * Each messenger has
 * - An attribute to store an address which identifies the messenger's place in
 *   the messaging hierarchy (see SetAddress(), GetAddress()). The address is
 *   not used by the messenger implementation itself, but it may be used by the
 *   classes derived from the Messenger (i.e., by an OpComponentManager,
 *   an OpComponent or an OpChannel) or by code using a messenger. The
 *   OpComponentManager, OpComponent and OpChannel store here the respective
 *   address of that instance.
 * - A method for submitting outgoing messages (see SendMessage(), ReplyTo()),
 *   which starts the message delivery phase.
 * - A method for processing incoming messages (see ProcessMessage()), which
 *   ends the message processing phase.
 * - A list of OpMessageListener instances. To register a listener call
 *   AddMessageListener() and to remove a listener call RemoveMessageListener().
 *
 * Outgoing messages are passed to SendMessage(), which (in its default
 * implementation) forwards the message to the component manager. The component
 * manager's reimplementation of SendMessage() takes care of forwarding the
 * message to the component manager containing the destination address (if
 * necessary), and storing it in that component manager's inbox.
 *
 * In the message processing phase, the message is taken from the component
 * manager's inbox, and passed to the ProcessMessage() method of the messenger
 * having the message's destination address. The default implementation of
 * ProcessMessage() forwards incoming messages to all registered
 * OpMessageListener instances.
 * @note Because the OpMessenger is a also an OpMessageListener, it is the
 *       responsibility of the user of an OpMessenger to not add an OpMessenger
 *       to itself as an OpMessageListener.
 *
 * @see \ref component_messages for an overview about sending and handling
 *      messages.
 */
class OpMessenger
	: public OpMessageListener
{
public:
	/**
	 * Create a messenger with the given address.
	 *
	 * @param address (optional) The address of this messenger.
	 */
	OpMessenger(const OpMessageAddress& address = OpMessageAddress());

	/**
	 * Destructor.
	 */
	virtual ~OpMessenger(void);

	/**
	 * Retrieve the address of this messenger.
	 *
	 * @return Address.
	 */
	virtual const OpMessageAddress& GetAddress(void) const { return m_address; }

	/**
	 * Set the address of this messenger.
	 *
	 * @param address New address.
	 */
	virtual void SetAddress(const OpMessageAddress& address) { m_address = address; }

	/**
	 * SendMessage() flags.
	 *
	 * These flags can be given to modify the behaviour of the below
	 * SendMessage() method. Unless otherwise noted, multiple
	 * flags can be combined using bitwise OR.
	 */
	enum SendMessageFlags
	{
		/**
		 * Return ERR_NO_MEMORY is message is NULL.
		 *
		 * This flag is useful if you want to shorten the following
		 * lines of code:
		 *
		 *   OP_STATUS s;
		 *   OpTypedMessage* message = OpFooMessage::Create(...);
		 *   if (!msg)
		 *       s = OpStatus::ERR_NO_MEMORY;
		 *   else
		 *       return SendMessage(message);
		 *
		 * to:
		 *
		 *   return SendMessage(OpFooMessage::Create(...));
		 *
		 * If this flag is not given, SendMessage() will return
		 * ERR_NULL_POINTER is the given message == NULL.
		 */
		SENDMESSAGE_OOM_IF_NULL = (1 << 0),
	};

	/**
	 * Send an outgoing message towards its destination.
	 *
	 * SendMessage() assumes ownership of the given \c message, and the
	 * caller MUST NOT delete the message, as it will be automatically
	 * deleted by the messaging infrastructure.
	 *
	 * With default flags (#SENDMESSAGE_OOM_IF_NULL), passing message == NULL
	 * makes SendMessage() return OpStatus::ERR_NO_MEMORY.
	 * If #SENDMESSAGE_OOM_IF_NULL is disabled, SendMessage() returns
	 * OpStatus::ERR_NULL_POINTER if message == NULL.
	 *
	 * See #SendMessageFlags, for more details.
	 *
	 * The implementation of this abstract method should forward the message to
	 * OpComponentManager::SendMessage() of the parent component manager. The
	 * component manager's implementation of OpComponentManager::SendMessage()
	 * shall deliver a local message directly to its own inbox, and submit
	 * remote messages to the platform's OpComponentPlatform::SendMessage()
	 * implementation.
	 *
	 * The default implementation simply calls
	 * g_component_manager->SendMessage() which should be sufficient for
	 * all cases running within a component manager.
	 *
	 * @param message Message. If NULL, OpStatus::ERR_NO_MEMORY or
	 *         OpStatus::ERR_NULL_POINTER is returned, depending on \c flags.
	 * @param flags Bitwise OR-combination of the #SendMessageFlags enum
	 *         values. See #SendMessageFlags documentation for details.
	 *
	 * @return OpStatus::OK on success. Otherwise, some kind of OpStatus
	 *         error code. Note the above discussion about the given \c flags.
	 */
	virtual OP_STATUS SendMessage(OpTypedMessage* message, unsigned int flags = SENDMESSAGE_OOM_IF_NULL);

	/**
	 * Send a \c reply to the sender of a received \c message.
	 *
	 * Both source and destination address of the reply are set to the
	 * corresponding destination resp. source address of the received message.
	 *
	 * With default flags (#SENDMESSAGE_OOM_IF_NULL), passing reply == NULL
	 * makes ReplyTo() return OpStatus::ERR_NO_MEMORY. If
	 * #SENDMESSAGE_OOM_IF_NULL is disabled, ReplyTo() returns
	 * OpStatus::ERR_NULL_POINTER if reply == NULL.
	 *
	 * @param message The received message. The reply is sent to the sender of
	 *        this message.
	 * @param reply The reply to send. If NULL, OOM will be returned. This
	 *        method takes ownership of the message regardless of the value
	 *        returned.
	 * @param flags Bitwise OR-combination of the #SendMessageFlags enum
	 *        values. See #SendMessageFlags documentation for details.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if there was not enough memory to
	 *         complete the operation.
	 * @retval OpStatus::ERR_NULL_POINTER if not
	 *         running in a component context. See also return values from
	 *         OpComponent::SendMessage().
	 */
	OP_STATUS ReplyTo(const OpTypedMessage& message, OpTypedMessage* reply, unsigned int flags = SENDMESSAGE_OOM_IF_NULL);

	/**
	 * Process one incoming message.
	 *
	 * The default implementation of this passes the message to each
	 * registered OpMessageListener (in the order they were registered).
	 *
	 * @see AddMessageListener(), RemoveMessageListener()
	 *
	 * @param message Message.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

	/**
	 * Add a listener to this messenger.
	 *
	 * When incoming messages are processed (in the default implementation
	 * of ProcessMessage()), the message is passed to each registered
	 * listener in the order they were registered.
	 *
	 * The caller needs to ensure to call RemoveMessageListener() for the
	 * specified listener instance, if it is deleted before the messenger
	 * instance is deleted.
	 *
	 * @param listener Object wishing to receive messages from this component.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS AddMessageListener(OpMessageListener* listener);

	/**
	 * Remove a listener from this messenger.
	 *
	 * @param listener Listener to remove.
	 *
	 * @return OpStatus::OK on success.
	 */
	virtual OP_STATUS RemoveMessageListener(OpMessageListener* listener);

	/**
	 * Delete this object at the earliest safe opportunity.
	 *
	 * This object may be deleted directly. No messages will be sent to listeners
	 * after this method has been called.
	 */
	virtual void SafeDelete();

	/** Modify use count. May delete this object. */
	virtual int IncUseCount();
	virtual int DecUseCount();

#ifdef HC_MESSAGE_LOGGING
	/**
	 * Set message logger.
	 *
	 * @param logger Logger to use. Empty OpSharedPtr<OpMessageLogger> disables
	 * logging.
	 */
	virtual void SetMessageLogger(OpSharedPtr<OpMessageLogger> logger);

	/**
	 * Get message logger.
	 *
	 * @return pointer to a usable OpMessageLogger or NULL if it has not been set up
	 * succsessfully.
	 */
	virtual OpMessageLogger* GetMessageLogger() const;
#endif

protected:
	/**
	 * Set the initial address of this messenger.
	 *
	 * Must NOT be used to change an existing address.
	 *
	 * @param address New address.
	 */
	void SetInitialAddress(const OpMessageAddress& address) { OP_ASSERT(!m_address.IsValid()); m_address = address; }
	friend class OpComponent;

	/// Address of this messenger
	OpMessageAddress m_address;

	/// List of registered listeners
	OtlList<OpMessageListener*> m_listeners;

	/// Count of how many times ProcessEvents() exists on the stack. Exists to prevent premature deletion.
	int m_use_count;

	/// Flag indicating that delayed (see m_use_count) deletion has been scheduled.
	bool m_delete;

#ifdef HC_MESSAGE_LOGGING
	/// Message logger
	OpSharedPtr<OpMessageLogger> m_message_logger;
#endif
};

/**
 * Helper class to maintain messenger use count.
 */
class CountMessengerUse
{
public:
	CountMessengerUse(OpMessenger* messenger)
		: messenger(messenger) {
		messenger->IncUseCount();
	}
	~CountMessengerUse() {
		messenger->DecUseCount();
	}

protected:
	OpMessenger* messenger;
};

#endif /* MODULES_HARDCORE_COMPONENT_OPMESSENGER_H */
