/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifdef SCOPE_MESSAGE_TRANSCODING

#ifndef SCOPE_TRANSCODER_H
#define SCOPE_TRANSCODER_H

#include "modules/scope/scope_internal_proxy.h"
#include "modules/scope/src/scope_tp_message.h"

class OpProtobufInstanceProxy;
class OpScopeTranscoder;
class OpScopeClient;
class OpScopeServiceManager;
class OpProtobufMessage;

/**
 * Provides a managment interface towards transcoder objects. This class is
 * meant to be sub-classed, the sub-class must implement the GetTranscoder()
 * method.
 *
 * This class provides a way to set the transcoder format which is currently
 * in use with SetTranscodingFormat(), retrieve the current format with
 * GeTranscodingFormat() as well as return statistics about the current
 * transcoder process with GetTranscodingStats().
 *
 * The format is stored in this class and will be sent to the transcoder
 * when one is created. To notify about a new transcoder object call
 * OnNewTranscoder().
 */
class OpScopeTranscodeManager
{
public:
	/**
	 * Initialize transcoder manager with transcoding format set to OpScopeProxy::Format_None.
	 */
	OpScopeTranscodeManager();

	/**
	 * Stores the current format used in transcoding to @a format.
	 * Setting it to @c Format_None will disable transcoding.
	 *
	 * If GetTranscoder() returns an object it will also initiate the transcoder
	 * process.
	 */
	OP_STATUS SetTranscodingFormat(OpScopeProxy::TranscodingFormat format);

	/**
	 * Returns the current transcoding format.
	 */
	OpScopeProxy::TranscodingFormat GetTranscodingFormat() const { return transcoding_format; }

	/**
	 * Fetches stats from the transcoder. If the transcoder is not available
	 * it will return 0 in all fields.
	 *
	 * @param[out] client_to_host Will contain the number of transcoded message going from the client to the host.
	 * @param[out] host_to_client Will contain the number of transcoded message going from the host to the client.
	 */
	void GetTranscodingStats(unsigned &client_to_host, unsigned &host_to_client);

	/**
	 * This must be called when a new transcoder object is available. The manager
	 * will take care of initializing the transcoder with correct format.
	 */
	OP_STATUS OnNewTranscoder();

	/**
	 * @return The current transcoder object or @c NULL if none is available yet.
	 */
	virtual OpScopeTranscoder *GetTranscoder() const = 0;

private:
	/**
	 * Specifies the format used for transcoding, this is the format that will
	 * be used when converting from or to ECMAScript objects.
	 * Default is Format_None which means data is converted to JSON directly from ECMAScript,
	 * ie. not with an actual instance (OpESToJSON).
	 */
	OpScopeProxy::TranscodingFormat transcoding_format;
};

/**
 * Defines an API between the transcoder and the host (not scope host) which
 * should have its messages transcoded.
 */
class OpScopeTranscoderHost
{
public:
	/**
	 * Sends the message directly to the host, skipping any processing steps.
	 *
	 * @param msg The STP message to send
	 */
	virtual OP_STATUS SendMessage(OpAutoPtr<OpScopeTPMessage> &msg) = 0;

	/**
	 * @return The STP version currently in use, only version 1 and up can use the transcoder.
	 */
	virtual unsigned GetSTPVersion() const = 0;

};

/**
 * Provides functionality for transcoding STP messages from one format to another.
 * The transcoder will convert STP messages by parsing messages to a local
 * C++ object and then back to the resulting format.
 *
 * The transcoder requires that the connected host has the exact same messages
 * as this instance, this is done by checking the available services and
 * their services. Only if there is an exact match will the transcoder be
 * enabled (See IsEnabled()).
 * The transcoder starts the process by sending out a command (scope.HostInfo)
 * to the remote host which returns information about services and versions.
 *
 * The second thing that is needed for the transcoder to start working is for
 * a client to set the current transcoder format, by default this is set to
 * OpScopeProxy::Format_None which means that nothing will be transcoded.
 * Call SetTranscoderFormat() with a chosen format to start the process (if
 * enabled). This can be done at any time, even before the transcoder has
 * been enabled. When the format is changed the transcoder will send a
 * command (scope.Connect) to the host to tell it about the new format, however
 * the client will not know about this change and will still receive messages
 * in the expected format.
 *
 * In addition the transcoder needs a service manager to be set with
 * SetServiceManager();
 */
class OpScopeTranscoder
{
public:
	OpScopeTranscoder(OpScopeTranscoderHost &host);

	/**
	 * Checks whether the transcoder is enabled or not and returns the result.
	 * Even though it is enabled it might still not process any messages.
	 * @see IsActive() for more details.
	 *
	 * @return @c TRUE if the transcoder is enabled, @c FALSE otherwise.
	 */
	BOOL IsEnabled() const;

	/**
	 * Checks whether the transcoder is allowed to transcode messages or not.
	 * An active state means that transcoder is enabled and that a specific
	 * transcoder has been set.
	 *
	 * @return @c TRUE if the transcoder is active, @c FALSE otherwise.
	 */
	BOOL IsActive() const;

	/**
	 * Disables the transcoder and resets format to OpScopeProxy::Format_None.
	 */
	void Disable();

	/**
	 * To be called by host code whenever a new message is to be sent to a client.
	 * The transcoder will inspect the message and determine whether the client
	 * should receive it or not.
	 *
	 * @param message The message which is meant to be sent to the client.
	 * @param client The client which is meant to receive the message.
	 * @param[out] skip Is set to @c TRUE if the message should not be sent to the client, @c FALSE if it should.
	 */
	OP_STATUS PreProcessMessageToClient(const OpScopeTPMessage &message, OpScopeClient *client, BOOL &skip);
	/**
	 * Transcodes the message received in @a message to a format the client
	 * understands, the new message is placed in @a message_copy.
	 * If the transcoder is not active nothing will happen.
	 *
	 * @param[out] message_copy The transcoded message is placed here.
	 * @param client The client which should receive the message, used to retrieve the wanted format.
	 * @param message The original message which is supposed to be sent to the client.
	 */
	OP_STATUS TranscodeMessageToClient(OpScopeTPMessage &message_copy, OpScopeClient *client, const OpScopeTPMessage &message);

	/**
	 * To be called by the client code whenever a new message is to be sent to
	 * a host. The transcoder will inspect the message and determine whether
	 * the host should receive it or not.
	 *
	 * @param message The message which is meant to be sent to the host.
	 * @param client The client which sent the message.
	 * @param[out] skip Is set to @c TRUE if the message should not be sent to the host, @c FALSE if it should.
	 */
	OP_STATUS PreProcessMessageFromClient(OpScopeTPMessage &message, OpScopeClient *client, BOOL &skip);
	/**
	 * Transcodes the message received in @a message to the transcoder format,
	 * the new message is placed in @a message_copy.
	 * If the transcoder is not active nothing will happen.
	 *
	 * @param[out] message_copy The transcoded message is placed here.
	 * @param client The client which sent the message.
	 * @param message The original message which is supposed to be sent to the host.
	 */
	OP_STATUS TranscodeMessageFromClient(OpScopeTPMessage &message_copy, OpScopeClient *client, const OpScopeTPMessage &message);

	/**
	 * @return @c TRUE if the setup phase is complete, @c FALSE otherwise.
	 */
	BOOL IsSetupFinished() const;
	/**
	 * Process the setup phase. The transcoder will communicate with the
	 * host until it has determined whether the transcoder can be enabled
	 * or not.
	 *
	 * This should be called as long as IsSetupFinished() is @c TRUE.
	 *
	 * @param message The last received message or @c NULL if it is the first call.
	 */
	OP_STATUS ProcessSetup(const OpScopeTPMessage *message);

	/**
	 * @return The current service manager or @c NULL if none is set.
	 */
	OpScopeServiceManager *GetServiceManager() const { return service_manager; }
	/**
	 * Sets the current service manager to @a manager.
	 */
	void SetServiceManager(OpScopeServiceManager *manager);

	/**
	 * Initiates transcoding with given format and STP version.
	 * The host will be re-configured to send data in the new format
	 * and any messages to-from the host will be transcoded to the
	 * specified format.
	 * If the initiation fails somehow the transcoder will be disabled.
	 *
	 * To disable transcoding do:
	 * @code
	 *   g_scope_manager->SetTranscodingFormat(OpScopeProxy::Format_None);
	 * @endcode
	 *
	 * @param format The format to start the transcoder with.
	 */
	OP_STATUS SetTranscoderFormat(OpScopeProxy::TranscodingFormat format);

	/**
	 * @return The number of messages sent from a client which have been transcoded.
	 */
	unsigned GetClientToHostTranscodedCount() const { return transcoded_client_to_host; }
	/**
	 * @return The number of messages sent from a host which have been transcoded.
	 */
	unsigned GetHostToClientTranscodedCount() const { return transcoded_host_to_client; }

private:
	static const OpProtobufMessage *GetProtobufMessage(OpScopeServiceManager *service_manager, const OpScopeTPMessage *message);
	static OpScopeTPHeader::MessageType ToMessageType(OpScopeProxy::TranscodingFormat format);
	OP_STATUS TranscodeFromClient( OpScopeClient *client, OpProtobufInstanceProxy &proxy, OpScopeTPMessage &to, const OpScopeTPMessage &from, OpScopeProxy::TranscodingFormat format );
	OP_STATUS TranscodeToClient( OpScopeClient *client, OpProtobufInstanceProxy &proxy, OpScopeTPMessage &to, const OpScopeTPMessage &from, OpScopeTPHeader::MessageType type );
	static BOOL TranscoderVersionCheck(const OpString &version, int major_version, int minor_version);

	/**
	* Handle the HostInfo response. If the remote host has the exact same
	* services as the local one then we enable the transcoder.
	*/
	OP_STATUS HandleTranscoderResult(const OpScopeTPMessage *message);

	OP_STATUS DoSetTranscoderFormat(OpScopeProxy::TranscodingFormat format);

	void SetEnabled(BOOL enabled);

private:
	OpScopeTranscoderHost &host;

	enum SetupState
	{
		  SetupState_HostInfo //< HostInfo is request from remote host
		, SetupState_HostInfoResponse //< Awaiting HostInfo response
		, SetupState_Finished //< Transcoder setup phase is complete
	};
	/**
	 * Controls whether transcoding is enabled or not. Initially FALSE and set to
	 * TRUE when the remote host has been verified to contain the same services
	 * and versions as this host.
	 */
	BOOL transcoder_enabled;
	/**
	 * The format which messages should be transcoded to and from.
	 * If it is set to OpScopeTPHeader::None then no transcoding is performed.
	 */
	OpScopeProxy::TranscodingFormat transcoding_format;
	/**
	 * The format which was configured by the client or OpScopeTPHeader::None if nothing has been configured yet.
	 * This format will be re-sent to the host whenever the transcoder is
	 * disabled, thus ensuring that the client gets messages in the expected
	 * format.
	 * The exception is when it is set to @c None, then no action is performed.
	 */
	OpScopeTPHeader::MessageType client_format;
	/**
	 * Number of messages from client which has been transcoded before being sent to host.
	 */
	unsigned transcoded_client_to_host;
	/**
	 * Number of messages from host which has been transcoded before being sent to client.
	 */
	unsigned transcoded_host_to_client;
	/**
	 * The service manager of the builtin host. It will only be used when
	 * transcoding is enabled and is used to figure out if the remote host
	 * is compatible with the builtin host.
	 */
	OpScopeServiceManager *service_manager;
	/**
	 * State for transcoder setup phase.
	 */
	SetupState setup_state;
	/**
	 * Counter for tag values during setup phase.
	 */
	unsigned setup_tag;
};

#endif // SCOPE_TRANSCODER_H

#endif // SCOPE_MESSAGE_TRANSCODING
