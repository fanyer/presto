/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_COMPONENT_OPTYPEDMESSAGE_H
#define MODULES_HARDCORE_COMPONENT_OPTYPEDMESSAGE_H

#include "modules/opdata/OpData.h"
#include "modules/hardcore/opera/components.h" // Need auto-generated OpComponentType
#include "modules/util/simset.h" // Need ListElement

#include "modules/hardcore/src/generated/OpTypedMessageBase.h" // OpTypedMessageBase
#include "modules/opdata/OpStringStream.h"

class OpTypedMessage; // Forward-declaration needed by OpSerializedMessage
class OpChannel; // Forward-declaration needed by OpTypedMessage::GetChannel()

typedef UINT32 OpMessageType;


/**
 * Message address; identifies the endpoints of an OpTypedMessage.
 *
 * A messaging address is an <int cm, int co, int ch>-tuple where "cm"
 * identifies an OpComponentManager instance, "co" identifies an OpComponent
 * instance within that OpComponentManager instance, and "ch" identifies an
 * OpChannel instance managed by that OpComponent instance. Together, the
 * three parts uniquely identify the ultimate sender or receiver of an
 * OpTypedMessage across the entire system of running threads/processes.
 *
 * Addresses are usually communicated in an "X.Y.Z" format, where X/Y/Z are
 * integers corresponding to the cm/co/ch parts, respectively.
 *
 * Here are the adresses with special meaning:
 *  - "X.Y.Z": (Y and Z are non-zero) The address of channel Z running inside
 *             component Y within component manager X.
 *  - "X.Y.0": (Y is non-zero) The address of component Y within component
 *             manager X.
 *  - "X.0.0": The address of component manager X.
 *  - "0.0.0": The very first component manager started.
 */
struct OpMessageAddress
{
	OpMessageAddress(int m = -1, int o = -1, int h = -1)
		: cm(m), co(o), ch(h) {}

	/**
	 * An address is invalid if it is the triple <-1,-1,-1>.
	 *
	 * This special address indicates that the address has not been
	 * initialized. Depending on context, this may mean that we are
	 * waiting for an addressee to make contact.
	 */
	inline bool IsValid(void) const
		{ return cm != -1 && co != -1 && ch != -1; }

	inline bool IsChannel(void) const
		{ return IsValid() && ch > 0; }

	inline bool IsComponent(void) const
		{ return IsValid() && ch == 0 && co > 0; }

	inline bool IsComponentManager(void) const
		{ return IsValid() && ch == 0 && co == 0; }

	inline bool operator!=(const OpMessageAddress& o) const
		{ return cm != o.cm || co != o.co || ch != o.ch; }

	inline bool operator==(const OpMessageAddress& o) const
		{ return cm == o.cm && co == o.co && ch == o.ch; }

	inline bool operator<(const OpMessageAddress& o) const
		{ return ((cm < o.cm) || (cm <= o.cm && co < o.co) || (cm <= o.cm && co <= o.co && ch < o.ch)); }

	inline OpMessageAddress& operator=(const OpMessageAddress& o)
	{
		cm = o.cm; co = o.co; ch = o.ch;
		return *this;
	}

	bool IsSameComponentManager(const OpMessageAddress& address) const { return cm == address.cm; }
	bool IsSameComponent(const OpMessageAddress& address) const { return IsSameComponentManager(address) && co == address.co; }

	static OpMessageAddress Deserialize(const OpData& data);
	OP_STATUS Serialize(OpData& data) const;

	int cm; ///< component manager number
	int co; ///< component number
	int ch; ///< channel number
};

#ifdef DEBUG
/** This operator can be used to print the specified OpMessageAddress in an
 * OP_DBG message.
 *
 * Example:
 * @code
 * struct OpMessageAddress address = ...;
 * OP_DBG(("Address: ") << address);
 * @endcode
 */
Debug& operator<<(Debug& d, const struct OpMessageAddress& addr);
#endif // DEBUG

#ifdef OPDATA_STRINGSTREAM
OpUniStringStream& operator<<(OpUniStringStream& in, const struct OpMessageAddress& addr);
#endif // OPDATA_STRINGSTREAM


/**
 * A thin wrapper around a serialized message.
 *
 * The only useful things you can do with a serialized message is:
 * - Send it somewhere by writing its data to a socket or pipe of some kind.
 * - Deserialize it into a "proper" OpTypedMessage.
 */
struct OpSerializedMessage
{
	/**
	 * Deserialize a serialized message. Return the deserialized message.
	 *
	 * This triggers the deserialization code that is generated from the
	 * protobuf specifications for each message type. It consumes data
	 * from the serialized message data, and produces an OpTypedMessage
	 * object that contains the same data that the original message
	 * contained.
	 *
	 * If the deserialization fails for any reason, the serialized message
	 * is left untouched (no data is consumed), and NULL is returned.
	 * Thus, if NULL is returned, the given data is intact, but if
	 * non-NULL is returned, the message data has been consumed from the
	 * serialized message buffer.
	 *
	 * @param message Message to be deserialized.
	 *
	 * @return Pointer to deserialized/reconstructed message. Must be
	 *         OP_DELETE()d by the caller (typically done automatically
	 *         after being processed by the receiving OpMessenger).
	 *         On error, NULL is returned, and the serialized message is
	 *         left intact.
	 */
	static OpTypedMessage* Deserialize(const OpSerializedMessage* message);

	/// Create OpSerializedMessage object wrapping the given data
	OpSerializedMessage(OpData d) { data = d; }

	OpData data;
};

/**
 * A message has a type, a source address and a destination address.
 *
 * Subclasses of OpTypedMessage add different kinds of payload to the object.
 *
 * Some notes about message lifetime:
 * Message objects are OP_NEW()ed by the message source/sender, and passed to
 * the messaging infrastructure (using the SendMessage() method of some
 * OpMessenger subclass). When received at the destination, message objects are
 * queued in the receiver's inbox awaiting processing. When message have been
 * processed, they are then OP_DELETE()d by the receiver.
 * EXCEPTION: If the platform needs to serialize the message in order to
 * transmit it across thread/process boundaries, the serialization code will
 * OP_DELETE() the message after creating a serialized version of the message.
 * Then, at the other end, when the OpTypedMessage is reconstructed from the
 * serialized version, a new OpTypedMessage object will be OP_NEW()ed (which
 * will be OP_DELETE()d by the receiver after processing).
 *
 * IMPORTANT: All data structures that are passed through OpTypedMessages MUST
 * be threadsafe.
 */
class OpTypedMessage
	: public ListElement<OpTypedMessage>
	, public OpTypedMessageBase
{
public:
	virtual ~OpTypedMessage() {}

	/*
	 * The rest of this class is NOT (necessarily) autogenerated from
	 * protobuf specifications.
	 */

	/**
	 * Serialize a message object. Return the serialized message.
	 *
	 * This triggers the serialization code that is generated from the
	 * protobuf specifications for each message type, and produces an
	 * OpSerializedMessage object that contains the serialized version
	 * of the given message. The serialized message can be sent over the
	 * wire, and then deserialized on the other side by calling
	 * OpSerializedMessage::Deserialize().
	 *
	 * If the serialization fails for any reason, NULL is returned.
	 *
	 * @param message Message to be serialized.
	 *
	 * @return Pointer to serialized message. Must be OP_DELETE()d by the
	 *         caller (after forwarding it to its destination). On error,
	 *         NULL is returned.
	 */
	static OpSerializedMessage* Serialize(const OpTypedMessage* message);

	/**
	 * Create a new message of this type.
	 *
	 * This method is not implemented in this abstract base class, but it
	 * MUST be implemented for each non-abstract message type.
	 *
	 * @return Created message. NULL on OOM.
	 */
	// static OpTypedMessage* Create(...);

	/**
	 * @return The type values that identifies the OpTypedMessage sub-class.
	 */
	virtual OpMessageType GetType() const { return m_type; }

	virtual const OpMessageAddress& GetSrc() const { return m_src; }
	virtual const OpMessageAddress& GetDst() const { return m_dst; }

	virtual double GetDueTime() const { return m_due_time; }

	/**
	 * Return number of milliseconds until this message should be processed.
	 *
	 * @param now If set, use this as the current timestamp instead of
	 *        calling g_op_time_info->GetRuntimeMS().
	 * @return Number of msecs until this message should be processed. If
	 *         the resulting delay is negative (the processing time has
	 *         already passed), 0 is returned instead.
	 */
	virtual double GetDelay(double now = 0) const;

	virtual void SetSrc(const OpMessageAddress& src) { m_src = src; }
	virtual void SetDst(const OpMessageAddress& dst) { m_dst = dst; }

	virtual void SetDueTime(double due_time) { m_due_time = due_time; }

	virtual bool IsValid() const { return m_src.IsValid() && m_dst.IsValid(); }

	virtual OP_STATUS Serialize(OpData& data) const = 0;

	/**
	 * Get the name of this message, as defined in the Protobuf file.
	 *
	 * @return A string containing the name, or NULL on OOM.
	 */
	virtual const char *GetTypeString() const = 0;

	/**
	 * Send a reply to the sender of this message.
	 *
	 * @param reply The reply. If NULL, OpStatus::ERR_NO_MEMORY will be
	 *        returned. This method takes ownership of the message regardless of
	 *        the value returned.
	 *
	 * @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY if reply is NULL
	 *        or on OOM. OpStatus::ERR_NULL_POINTER if not running in a
	 *        component context (i.e. if g_component is NULL). See also return
	 *        values from OpMesssenger::ReplyTo().
	 */
	OP_STATUS Reply(OpTypedMessage* reply) const;

#ifdef OPDATA_STRINGSTREAM
	virtual OpUniStringStream& ToStream(OpUniStringStream& in) const;
#endif //DEBUG

protected:
	OpTypedMessage(
		OpMessageType type,
		const OpMessageAddress& src,
		const OpMessageAddress& dst,
		double delay);

	OpMessageType m_type;
	OpMessageAddress m_src;
	OpMessageAddress m_dst;
	double m_due_time;
};


#ifdef OPDATA_STRINGSTREAM
OpUniStringStream& operator<<(OpUniStringStream& in, const OpTypedMessage& msg);
#endif //DEBUG

#endif /* MODULES_HARDCORE_COMPONENT_OPTYPEDMESSAGE_H */
