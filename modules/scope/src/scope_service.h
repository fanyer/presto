/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_SERVICE_H
#define SCOPE_SERVICE_H

//#include "modules/scope/src/generated/g_scope_service_interface.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/protobuf/src/protobuf_message.h"
#include "modules/scope/src/scope_excepts.h"

class OpProtobufInstanceProxy;
class OpScopeClient;
class OpScopeNetwork;
class OpProtobufMessage;
class OpScopeCommand;
class OpScopeTransportManager;
class OpScopeServiceManager;
class FramesDocument;

/**
 * Defines a scope service. A service is a collection of functionality which
 * is exposed on the scope protocol (STP). It acts as a remote-procedure call
 * for the client to use, it also sends events to the client when something
 * changes in the host.
 */
class OpScopeService : public ListElement<OpScopeService>
{
public:
	/**
	 * Decided how the service is controlled. A service can either be
	 * controlled (enabled/disabled) by the scope protocol (CONTROL_MANUAL) or
	 * the service should always be enabled (CONTROL_FORCED).
	 */
	enum ControlType
	{
		/**
		 * Service can be enabled or disabled by the scope protocol.
		 */
		CONTROL_MANUAL,
		/**
		 * Service cannot be enabled/disables, instead it is always enabled.
		 */
		CONTROL_FORCED
	};

	/**
	 * Provides a forward range over all message IDs in a service.
	 * The range goes from @a start and up to but not including @a end.
	 */
	class MessageIDRange
	{
	public:
		MessageIDRange(unsigned start, unsigned end)
			: front(start)
			, back(end)
		{}

		/**
		 * @return @c TRUE when the range is empty (no more IDs).
		 */
		BOOL IsEmpty() const { return front >= back; }
		/**
		 * @return The front-most message ID.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		unsigned Front() const { return front; }
		/**
		 * Advance the front to the next message ID.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		void PopFront()
		{
			OP_ASSERT(!IsEmpty());
			++front;
		}

	private:
		unsigned front, back;
	};

	/**
	 * Provides a forward range over all messages descriptors in a service.
	 * The range goes from @a start and up to but not including @a end.
	 */
	class MessageRange
	{
	public:
		MessageRange(const OpScopeService *service, unsigned start, unsigned end)
			: service(service)
			, range(start, end)
			, front(service && !range.IsEmpty() ? service->GetMessage(range.Front()) : NULL)
		{
			OP_ASSERT(service);
			OP_ASSERT(range.IsEmpty() || front);
		}

		/**
		 * @return @c TRUE when the range is empty (no more descriptors).
		 */
		BOOL IsEmpty() const { return range.IsEmpty(); }
		/**
		 * @return The front-most message descriptor.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		const OpProtobufMessage *Front() const { return front; }
		/**
		 * @return The front-most message ID.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		unsigned FrontID() const { return range.Front(); }
		/**
		 * Advance the front to the next message.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		void PopFront()
		{
			OP_ASSERT(!IsEmpty());
			range.PopFront();
			front = service && !range.IsEmpty() ? service->GetMessage(range.Front()) : NULL;
			OP_ASSERT(range.IsEmpty() || front);
		}

	private:
		const OpScopeService *service;
		MessageIDRange range;
		const OpProtobufMessage *front;
	};

	/**
	 * Provides a forward range over all commands in a service.
	 */
	class CommandRange
	{
	public:
		CommandRange(const OpScopeCommand *commands, unsigned count)
			: front(commands)
			, end(commands + count)
		{
			OP_ASSERT(IsEmpty() || front);
		}

		/**
		 * @return @c TRUE when the range is empty (no more commands).
		 */
		BOOL IsEmpty() const { return !front || front == end; }
		/**
		 * @return The front-most command.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		const OpScopeCommand *Front() const { return front; }
		/**
		 * Advance the front to the next command.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		void PopFront()
		{
			OP_ASSERT(!IsEmpty());
			++front;
		}

	private:
		const OpScopeCommand *front, *end;
	};

	/**
	 * Provides a forward range over all enum IDs in a service.
	 */
	class EnumIDRange
	{
	public:
		EnumIDRange(unsigned *ids, unsigned count)
			: front(ids)
			, end(ids + count)
		{
			OP_ASSERT(IsEmpty() || front);
		}

		/**
		 * @return @c TRUE when the range is empty (no more IDs).
		 */
		BOOL IsEmpty() const { return !front || front == end; }
		/**
		 * @return The front-most enum ID.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		unsigned Front() const { return *front; }
		/**
		 * Advance the front to the next enum ID.
		 * @note Do not call when IsEmpty() is @c TRUE.
		 */
		void PopFront()
		{
			OP_ASSERT(!IsEmpty());
			++front;
		}

	private:
		unsigned *front, *end;
	};

	/**
	 * Construct a new service.
	 * If @a manager is supplied the service will be automatically registered
	 * in the manager (OpScopeServiceManager::RegisterService).
	 * The parameter @a control decides whether a service can be enabled/disabled
	 * by the scope protocol or whether it should always be enabled. The default
	 * is CONTROL_MANUAL which allows manual control, some services
	 * such as meta-services (@c stp-0, @c stp-1) and the @c scope service set
	 * this to CONTROL_FORCED.
	 *
	 * The @a name parameter is the name used on the scope protocol (STP/1).
	 * @note This differs from the name used in .proto files, it is actually
	 *       a lowercase version with dashes between words. The reason for this
	 *       are compatibility with the original scope protocol (STP/0).
	 *
	 * @param name  The unique name of this service (eg, @c "console-logger").
	 * @param manager The manager which will take control of this service or
	 *                @c NULL to leave it unmanaged. Default is @c NULL.
	 * @param is_always_enabled Whether the service is always enabled or not.
	 */
	OpScopeService(const uni_char* name, OpScopeServiceManager *manager = NULL, ControlType control = CONTROL_MANUAL);
	/**
	 * Unregister the service from the manager if it was registered.
	 * Also disables the service if it was enabled.
	 */
	virtual ~OpScopeService();

	/**
	 * Optional second stage constructor for subclasses.
	 *
	 * Note: If OpStatus::ERR is returned from this function, we will still try to
	 * create other services. Only OpStatus::ERR_NO_MEMORY will cause further service
	 * creation to fail.
	 *
	 * @return OpStatus::OK, OpStatus::ERR, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Construct() { return OpStatus::OK; }

	/**
	 * @return The current service manager or @c NULL if the service is not managed.
	 */
	OpScopeServiceManager *GetManager() const { return manager; }

	/**
	 * Sets the current manager to @c manager.
	 * @param The service manager to set as current or @c NULL to reset the pointer.
	 */
	void SetManager(OpScopeServiceManager *manager) { this->manager = manager; }

	/** 
	 * @return the name of this service, e.g. @c "scope" or @c "window-manager"
	 */
	const uni_char* GetName() const { return name; }

	/**
	 * Returns the type of control this service requires, this is either
	 * manual control (the default) or forced control.
	 * Services that have forced control set behave slightly different, for
	 * instance it is not possible to enable/disable them with the scope protocol.
	 * Secondly they don't use the OnServiceEnabled()/OnServiceDisabled()
	 * callbacks.
	 *
	 * @return The type of control mechanism this service uses.
	 */
	ControlType GetControlType() const { return control; }

	/**
	 * Called after all services has been initialized. This allows services
	 * to perform intra-service initialization, for instance by looking
	 * up a service by name and storing a pointer to the service object for
	 * later use.
	 *
	 * @return OpStatus::ERR_NO_MEMORY for OOM, OpStatus::OK for success.
	 */
	virtual OP_STATUS OnPostInitialization();

	/**
	 * Checks whether the service is enabled or not. Enabling and disabling
	 * a service can be performed with EnableService() and DisableService().
	 * The exception are services which are meant to be enabled at all times
	 * (GetControlType() == CONTROL_FORCED).
	 *
	 * @return TRUE if the service is enabled, FALSE otherwise.
	 */
	virtual BOOL IsEnabled() const { return GetControlType() == CONTROL_FORCED || is_enabled; }

	/**
	 * Enables the service for usage in the scope protocol.
	 *
	 * Sub-classes which are interested in knowing when the service is enabled
	 * should re-implement OnEnabled().
	 *
	 * @note If the service is marked as being always enabled (IsAlwaysEnabled())
	 *       then this call will do nothing.
	 *
	 * @return OK if enabled, or ERR if it failed to enable the service or it
	 *         was already enabled.
	 */
	OP_STATUS Enable();

	/**
	 * Disables the service. After a service is disable it should not
	 * receive any callbacks from listeners or other external code.
	 *
	 * Sub-classes which are interested in knowing when the service is disabled
	 * should re-implement OnDisabled().
	 *
	 * @note If the service is marked as being always enabled (IsAlwaysEnabled())
	 *       then this call will do nothing.
	 *
	 * @return OK if disabled, or ERR if it failed to disable the service or it
	 *         was already disabled.
	 */
	OP_STATUS Disable();

	/**
	 * Called whenever the service is enabled. If this returns an error code
	 * the service will be disabled again.
	 * After this point the service can initialize resources which is needed.
	 *
	 * The default implementation does nothing.
	 *
	 * @return OK if everything went ok, or any error code if it failed while initializing.
	 */
	virtual OP_STATUS OnServiceEnabled();

	/**
	 * Called whenever the service is disabled. After this point the service
	 * will not be used anymore and the service should cleanup any resources
	 * it does not need anymore.
	 *
	 * The default implementation does nothing.
	 *
	 * @return OK if everything went ok, or any error code if it failed while shutting down.
	 */
	virtual OP_STATUS OnServiceDisabled();

	void SetClient(OpScopeClient *c) { client = c; }
	OpScopeClient *GetClient() const { return client; }

	/**
	 * Called whenever the service must execute a command.
	 *
	 * @param client The client that send the command. Responses are sent back to this client.
	 * @param message The message containing the command to execute.
	 */
	virtual OP_STATUS OnReceive( OpScopeClient *client, const OpScopeTPMessage &message );

	/**
	 * Second stage of command execution, called from OnReceive().
	 * For sub-classes to re-implement.
	 */
	virtual OP_STATUS DoReceive( OpScopeClient *client, const OpScopeTPMessage &message );

	/**
	 * Send back an STP error to the client.
	 *
	 * @param client The client which should receive the error.
	 * @param orig_message The original message which was received, used to
	 *                     deliver error to corresponding command.
	 * @param err The error to send, the error is converted into an STP error message.
	 * @return OpStatus::ERR_NULL_POINTER if no client is set,
	 *         OpStatus::ERR_NO_MEMORY on OOM or
	 *         OpStatus::OK for success.
	 */
	OP_STATUS SendError( OpScopeClient *client, const OpScopeTPMessage &orig_message, const OpScopeTPError &err );

	/**
	 * Send back a response to the client.
	 *
	 * @param client The client which should receive the response message.
	 * @param original Header information from the original command, used to
	 *                 deliver response to the corresponding command.
	 * @param proxy The instance proxy containing the response message to send.
	 * @return OpStatus::ERR_NULL_POINTER if no client is set,
	 *         OpStatus::ERR_NO_MEMORY on OOM or
	 *         OpStatus::OK for success.
	 */
	OP_STATUS SendResponse( OpScopeClient *client, const OpScopeTPHeader &original, const OpProtobufInstanceProxy &proxy );

	/**
	 * Sends back an async error message to the client associated with @a tag.
	 *
	 * @param tag The tag value from the previous incoming command message.
	 *            Used to find the command information.
	 * @param responseID Unused.
	 * @param status The error type to send, converted to an STP error type and error description.
	 * @return OpStatus::ERR_NO_SUCH_RESOURCE if tag is invalid,
	 *         OpStatus::ERR_NULL_POINTER if no client is set,
	 *         OpStatus::ERR_NO_MEMORY on OOM or
	 *         OpStatus::OK for success.
	 */
	OP_STATUS SendAsyncError(unsigned tag, unsigned responseID, OP_STATUS status);
	OP_STATUS SendAsyncError( unsigned int async_tag, const OpScopeTPError &error_data );

	/**
	 * Sends back an async response message to the client associated with @a tag.
	 *
	 * @param tag The tag value from the previous incoming command message.
	 *            Used to find the command information.
	 * @param proxy The instance proxy containing the response message to send.
	 * @param responseID The ID of command this is a response to.
	 * @return OpStatus::ERR_NO_SUCH_RESOURCE if tag is invalid,
	 *         OpStatus::ERR_NULL_POINTER if no client is set,
	 *         OpStatus::ERR_NO_MEMORY on OOM or
	 *         OpStatus::OK for success.
	 */
	OP_STATUS SendAsyncResponse( unsigned int tag, const OpProtobufInstanceProxy &proxy, unsigned int responseID );

	/**
	 * Send an event to client connected to this service.
	 *
	 * @param proxy The instance proxy containing the event message to send.
	 * @param eventID The ID of event.
	 * @return OpStatus::ERR_NULL_POINTER if no client is set,
	 *         OpStatus::ERR_NO_MEMORY on OOM or
	 *         OpStatus::OK for success.
	 */
	OP_STATUS SendEvent( const OpProtobufInstanceProxy &proxy, unsigned int eventID );

	/**
	 * Converts a core status value into an STP message status.
	 * @return The converted STP message status value.
	 */
	static OpScopeTPHeader::MessageStatus ErrorCode(OP_STATUS status);

	/**
	 * Report an error which occurred while processing a command.
	 *
	 * The error type is recorded with optional detailed information such as
	 * description or information on where the error occurred.
	 * An optional description can be set in @a description to better explain
	 * what went wrong, these messages are meant to be human-readable.
	 * In addition if the error occurred during parsing of input data or other
	 * text then the exact position can be specified by sending @a line,
	 * @a col and @a offset.
	 *
	 * For instance if the input data from JSON or XML fails it will contain
	 * the line number where it failed and description will explain the type
	 * of error.
	 *
	 * @param status The error type, choose among OpScopeTPHeader::MessageStatus (except OpScopeTPHeader::OK).
	 * @param description An optional description of the error, string is not copied only the pointer.
	 * @param line Line number where error occurred, set to -1 to disable.
	 * @param col Column number where error occurred, set to -1 to disable.
	 * @param offset Byte offset where error occurred, set to -1 to disable.
	 * @returns OpStatus::ERR, this allows the code to return the return value from this function.
	 *
	 * @note Setting errors outside of a the processing of a command (ie. before or after a command) will not produce any results.
	 */
	OP_STATUS SetCommandError(OpScopeTPHeader::MessageStatus status, const uni_char *description = NULL, int line = -1, int col = -1, int offset = -1);
	/**
	 * @overload OP_STATUS OpScopeService::SetCommandError(const OpScopeTPError &error)
	 */
	OP_STATUS SetCommandError(const OpScopeTPError &error);

	/**
	 * Sets the current Command error by analyzing the status code.
	 * @returns OpStatus::ERR, this allows the code to return the return value from this function.
	 */
	OP_STATUS ReportCommandStatus(OP_STATUS status);
	OP_STATUS FormatCommandError(OpScopeTPError &error, uni_char *buffer, size_t count, const uni_char *format, ...);
	/**
	 * @note The error status code is reset to OK when a new command is processed.
	 * @return The error structure from the last command error.
	 */
	const OpScopeTPError &GetCommandError() const { return command_error; }

	/**
	 * Get version string on the format 'major.minor.patch'. 
	 */
	virtual const char *GetVersionString() const = 0; 
	/**
	 * @return The major version of the service.
	 */
	virtual int GetMajorVersion() const = 0;
	/**
	 * @return The minor version of the service.
	 */
	virtual int GetMinorVersion() const = 0;
	/**
	 * @return the patch version number for the service, may be an empty string.
	 */
	virtual const char *GetPatchVersion() const = 0;

	/**
	 * Returns TRUE if the response to an active command was sent.
	 *
	 * @note Only valid while a command is executing.
	 */
	BOOL IsResponseSent() const { return is_response_sent; }

	/**
	 * @return The number of commands in this service.
	 * @note To be implemented by sub-classes.
	 */
	virtual int GetCommandCount() const = 0;

	/**
	 * Provides a way to inspect all commands in this service.
	 * @return A range covering all commands in the service.
	 * @note To be implemented by sub-classes.
	 */
	virtual CommandRange GetCommands() const = 0;

	/**
	 * @return The message descriptor associated with the given ID @a message_id.
	 * @param message_id ID of the message to fetch.
	 * @note To be implemented by sub-classes.
	 */
	virtual const OpProtobufMessage *GetMessage(unsigned int message_id) const = 0;

	/**
	 * @return The number of messages in this service.
	 * @note To be implemented by sub-classes.
	 */
    virtual unsigned                 GetMessageCount() const = 0;

	/**
	 * Provides a way to inspect all message IDs in this service.
	 * @return A range covering all message IDs in the service.
	 * @note To be implemented by sub-classes.
	 */
	virtual MessageIDRange           GetMessageIDs() const = 0;

	/**
	 * Provides a way to inspect all message descriptors in this service.
	 * @return A range covering all message descriptors in the service.
	 * @note To be implemented by sub-classes.
	 */
	virtual MessageRange             GetMessages() const = 0;

	/**
	 * Provides a way to inspect all enums in this service.
	 * @return A range covering all enum IDs in the service.
	 * @note To be implemented by sub-classes.
	 */
	virtual EnumIDRange GetEnumIDs() const;

	/**
	 * @return The number of enums in this service.
	 * @note To be implemented by sub-classes.
	 */
	virtual unsigned  GetEnumCount() const;

	/**
	 * Check if an enum exists in the service.
	 * @param enum_id The ID of the enum to find.
	 * @return @c TRUE if the enum with ID @a enum_id exists in this service.
	 * @note To be implemented by sub-classes.
	 */
	virtual BOOL      HasEnum(unsigned enum_id) const;

	/**
	 * Fetches information on a specific enum.
	 *
	 * For instance if an enum was declared as:
	 * @verbatim
	 * enum Dominion
	 * {
	 *   ARCTURUS = 1;
	 * }
	 * @endverbatim
	 * and the enum had ID 1, then it would possible to get information with:
	 * @code
	 * const uni_char *name;
	 * unsigned count;
	 * GetEnum(1, 0, name, count)
	 * // name is now "Dominion", count is 1
	 * @endcode
	 *
	 * @param enum_id The ID of the enum to find.
	 * @param[out] name Pointer to name of service is stored here. String is owned by service.
	 * @param[out] value_count Number of values in the enum is placed here.
	 * @return OpStatus::ERR_NO_SUCH_RESOURCE if the enum could not be found or
	 *         OpStatus::OK if it was found.
	 * @note To be implemented by sub-classes.
	 */
	virtual OP_STATUS GetEnum(unsigned enum_id, const uni_char *&name, unsigned &value_count) const;

	/**
	 * Fetch name and number of a given enum value.
	 *
	 * For instance if an enum was declared as:
	 * @verbatim
	 * enum Dominion
	 * {
	 *   ARCTURUS = 1;
	 * }
	 * @endverbatim
	 * and the enum had ID 1, then it would possible to get the enum value with:
	 * @code
	 * const uni_char *name;
	 * unsigned value;
	 * GetEnumValue(1, 0, name, value)
	 * // name is now "ARCTURUS", value is 1
	 * @endcode
	 *
	 * @param enum_id The ID of the enum to find.
	 * @param idx The index of the enum value, indexes are from 0 and up.
	 * @param[out] value_name The name of the enum value is stored here. String is owned by service.
	 * @param[out] value_number The number of the enum value is placed here.
	 * @return OpStatus::ERR_NO_SUCH_RESOURCE if the enum could not be found or
	 *         OpStatus::OK if it was found.
	 * @note To be implemented by sub-classes.
	 */
	virtual OP_STATUS GetEnumValue(unsigned enum_id, unsigned idx, const uni_char *&value_name, unsigned &value_number) const;

	/**
	 * Called if the filter in the window manager is changed. Default implementation is to
	 * do nothing.
	 */
	virtual void FilterChanged();

	/**
	 * Called when a Window is closed. Default implementation does nothing.
	 *
	 * FIXME: This function is too specific for OpScopeService. This is a temporary
	 *        fix for CORE-35300. It should be replaced with a proper listener
	 *        interface on WindowManager directly.
	 */
	virtual void WindowClosed(Window *window);

protected:
	void SilentError(OP_STATUS status);

	/**
	 * Parses the STP message into a native object. The client is responsible
	 * for the actual parsing since it knows about the format of the message.
	 * If it fails to parse the message SetCommandError() is called.
	 *
	 * @param client Client which should parse the message.
	 * @param msg The STP message which should be parsed.
	 * @param[out] proxy Contains the message descriptor to use and instance to place result in.
	 * @return The result of the OpScopeClient::Parse().
	 */
	OP_STATUS ParseMessage( OpScopeClient *client, const OpScopeTPMessage &msg, OpProtobufInstanceProxy &proxy );

	// Error messages
	static const uni_char *GetInvalidStatusFieldText();
	static const uni_char *GetIncorrectServiceText();
	static const uni_char *GetCommandNotFoundText();

	static const uni_char *GetInitAsyncCommandFailedText();
	static const uni_char *GetParseCommandMessageFailedText();
	static const uni_char *GetCommandExecutionFailedText();
	static const uni_char *GetCommandResponseFailedText();

protected:
	/**
	 * Initialize the async commmand by associating it with a message header.
	 * The request will get a new unique tag which can be used internally in this service.
	 *
	 * @param header The STP header to register as an async command.
	 * @param [out] The new tag that is associated with the async command.
	 * @return OpStatus::ERR_NO_MEMORY if a new async request could not be allocated.
	 */
	OP_STATUS InitAsyncCommand(const OpScopeTPHeader &header, unsigned &async_tag);
	/**
	 * Cleans up the specified async request.
	 *
	 * @param async_tag The tag associated with the async request.
	 */
	void      CleanupAsyncCommand(unsigned async_tag);

private:
	OP_STATUS SerializeResponse(OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &out, const OpScopeTPHeader &original, const  OpProtobufInstanceProxy &proxy, unsigned int responseID);

	/**
	 * Represents an active async command, it contains a unique tag and a header information for the command.
	 */
	class AsyncCommand : public ListElement<AsyncCommand>
	{
	public:
		AsyncCommand(unsigned tag)
			: tag(tag)
		{
		}

		OP_STATUS Construct(const OpScopeTPHeader &header)
		{
			return this->header.Copy(header);
		}

		unsigned tag;
		OpScopeTPHeader header;
	};
	AsyncCommand *GetAsyncCommand(unsigned async_tag);

	/**
	 * The name of the service which is used on the scope protocol (STP/1).
	 * @note This differs from the name used in .proto files, it is actually
	 *       a lowercase version with dashes between words. The reason for this
	 *       are compatibility with the original scope protocol (STP/0).
	 */
	const uni_char* name;
	OpScopeServiceManager *manager; ///< The manager which manage this service, or @c NULL if not unmanaged.
	BOOL  is_enabled;
	OpScopeTPHeader current_command;
	List<AsyncCommand> async_commands;
	OpScopeTPError command_error;
	OpScopeClient *client;
	BOOL is_response_sent;

private:
	/**
	 * Sets whether this service is enabled or not.
	 * If the service is marked as being always enabled (IsAlwaysEnabled())
	 * then this call will do nothing, the service will still be enabled.
	 *
	 * @note Some services cannot be disabled, in this case this call does nothing.
	 */
	virtual void SetIsEnabled(BOOL v) { is_enabled = v || GetControlType() == CONTROL_FORCED; }

	ControlType control;
};

/**
 * A meta-service is a special service which has no functionality only a name.
 * The service cannot be enabled/disabled and is only present to give back
 * some simple information to STP clients. For instance the meta-service
 * @c "stp-1" tells the client that the host supports STP/1.
 *
 * The meta-service does not support introspection and will return back 0 for
 * counts and @c NULL for introspection arrays.
 */
class OpScopeMetaService : public OpScopeService
{
public:
	OpScopeMetaService(const uni_char* id, OpScopeServiceManager *manager = NULL);
	virtual ~OpScopeMetaService();

	// Enum introspection is handled by default implementation

	// Meta-service has no commands
	virtual int GetCommandCount() const { return 0; }
	virtual CommandRange GetCommands() const { return CommandRange(NULL, 0); }

	// Version is always "0.0.0" for meta-services.
	virtual const char *GetVersionString() const;
	virtual int GetMajorVersion() const;
	virtual int GetMinorVersion() const;
	virtual const char *GetPatchVersion() const;

	// Meta-service has no messages
	virtual const OpProtobufMessage *GetMessage(unsigned int message_id) const { return NULL; }
	virtual unsigned                 GetMessageCount() const { return 0; }
	virtual MessageIDRange           GetMessageIDs() const { return MessageIDRange(0, 0); }
	virtual MessageRange             GetMessages() const { return MessageRange(this, 0, 0); }
}; 

#endif // SCOPE_SERVICE_H
