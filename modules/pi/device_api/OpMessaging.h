/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 */
#ifndef PI_DEVICE_API_OPMESSAGING_H
#define PI_DEVICE_API_OPMESSAGING_H

#ifdef PI_MESSAGING

class OpMessagingListener;

typedef OP_STATUS OP_MESSAGINGSTATUS;

/** Object for manipulation on messages (SMS, MMS and Email), and message folders (drafts, inbox, etc).
 *
 * This class is used by dom and deviceapi modules and exposed to user via
 * ecmascript.  Therefore there is correspondence between OpMessaging and
 * widget (or window), so there can be multiple instances of OpMessaging
 * runninng concurrently, however they don't communicate.
 *
 * The platform implementation of OpMessaging needs to listen to system events
 * about incoming messages and should inform the OpMessagingListener instance,
 * that is provided by core to the method OpMessaging::Construct(), by calling
 * OpMessagingListener::OnMessageArrived() when a new message has arrived.
 *
 * Sending messages, listening to incoming messages, copying, moving, reading
 * and deleting messages is provided.  Also copying and deleting of message
 * folders and filesystem operation for saving attachments.  All operations
 * are asynchronous.
 *
 * This class is an abstract interface that provides methods for sending
 * messages (see OpMessaging::SendMessage()), folder manipulation (see
 * OpMessaging::CreateFolder(), OpMessaging::DeleteFolder(),
 * OpMessaging::GetFolderNames()), email account manipulation (see
 * OpMessaging::GetEmailAccounts(), OpMessaging::GetSingleEmailAccount(),
 * OpMessaging::DeleteEmailAccount()), message manipulation, including their
 * attachments (see OpMessaging::SaveAttachment(),
 * OpMessaging::GetMessageCount(), OpMessaging::GetMessage(),
 * OpMessaging::DeleteMessage(), OpMessaging::PutMessage(),
 * OpMessaging::Iterate()). The platform needs to implement these methods by
 * calling the corresponding system interface. All operations are asynchronous.
 *
 * If calls to OEM gives obviously invalid data, platform may try to sanitize
 * values to fit them in OpMessaging structures, or to silently filter-out
 * the date, or return OP_STATUS_ERR.
 *
 * When core passes data to platform, for example in
 * OpMessaging::CreateFolder(), it makes sure the data is a valid string,
 * nothing more;  or in passing message in OpMessage::SendMessage(), addresses
 * are valid, MIMEType of attachment is set properly and so on.
 *
 * In general, messages fetched from platform should have ids set.
 *
 */
class OpMessaging
{
public:
	class Status : public ::OpStatus
	{
		public:
			enum
			{
				/** Message type is not from OpMessaging::Message::Type
				 * or it is OpMessaging::Message::TypeUnknown.
				 */
				ERR_WRONG_MESSAGE_TYPE = USER_ERROR + 1,
				ERR_MESSAGE_PARAMETER,
				/** There is no destination address set. */
				ERR_MESSAGE_NO_DESTINATION,
				/** One of destination, cc, bcc or reply_to addresses is wrong. */
				ERR_MESSAGE_ADDRESS,
				ERR_MESSAGE_ATTACHMENT,
				/** One of parameters should be empty,
				 * for example there should be no subject in SMS.
				 */
				ERR_MESSAGE_PARAMETER_NOT_EMPTY,
				ERR_NO_DIRECTORY,
				ERR_NO_NETWORK_COVERAGE,
				ERR_EMAIL_ACCOUNT,
				ERR_EMAIL_ACCUNT_IN_USE,
				ERR_FOLDER_DOESNT_EXIST,
				ERR_FOLDER_ALREADY_EXISTS,
				/** Folder name is invalid (e.g. empty string or contains characters that
				 * are not supported by the platform. */
				ERR_FOLDER_INVALID_NAME,
				ERR_FOLDER_NOT_EMPTY,
				ERR_NO_MESSAGE,
				ERR_MMS_TOO_BIG
			};
	};

	/** Attachment struct methods are implemented by core.
	 * Used as part of Message, can be saved to filesystem. */
	struct Attachment
	{
		Attachment()
			: m_inside_message(FALSE)
			, m_size(0)
		{}

		/** Copy contents of the Attachment.
		 *
		 *	@param[out] new_attachment placeholder for the copy of the attachment.
		 *		Caller is responsible for releasing it.
		 *
		 *	@return
		 *		OpStatus::OK,
		 *		OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS CopyTo(Attachment** new_attachment);

		/** Absolute filename of attachment.
		 *
		 *	@see m_inside_message.
		 */
		OpString m_full_filename;

		/** Unique identity string of the attachment.
		 * Can be in form "87skc1k7al.fsf@opera.com:attachment:4", where
		 * "87skc1k7al.fsf@opera.com" is message id and
		 * 4 is the consecutive number of attachment in given message.
		 *
		 * @see m_inside_message.
		 */
		OpString m_id;

		/** Flag indicating if the attachment lays encoded inside a message.
		 *
		 *	If it is inside a message, m_id field should be used to access
		 *	the attachment.
		 *
		 *	If it is not yet attached, m_full_filename field should	be used
		 *	to access the attachment.
		 *
		 *	@see m_full_filename.
		 *	@see m_id.
		 */
		BOOL m_inside_message;

		/** Suggested name of the attachment or a "name" property of email
		 * multipart part.
		 */
		OpString m_suggested_name;

		/** Mime type of the attachment. */
		OpString m_mimetype;

		/** Filesize of attachment in bytes.
		 * *Not* size of attachment after encoding in base64 or similar.
		 */
		OpFileLength m_size;
	};

	/** Message struct methods are implemented by core.
	 */
	struct Message
	{
		enum Type
		{
			SMS,
			MMS,
			Email,
			TypeUnknown
		};

		enum Priority
		{
			Highest,
			High,
			Normal,
			Low,
			Lowest,
			PriorityUnknown
		};

		Message()
			: m_is_read(MAYBE)
			, m_priority(Normal)
			, m_sending_timeout(0)
			, m_sending_time(0)
			, m_type(TypeUnknown)
		{}

		/** Copy contents of the Message.
		 *
		 * @param[out] new_message placeholder for the copy of the message.
		 *
		 * @return
		 *		OpStatus::OK,
		 *		OpStatus::ERR_NO_MEMORY.
		 */
		OP_STATUS CopyTo(Message** new_message);

		/** List of attachments.
		 * Applies to MMS and email, doesn't apply to SMS.
		 */
		OpAutoVector<OpMessaging::Attachment> m_attachment;

		/** List of cc addresses.
		 * Applies to MMS and email, doesn't apply to SMS.
		 */
		OpAutoVector<OpString> m_cc_address;

		/** List of bcc addresses.
		 * Applies to MMS and email, doesn't apply to SMS.
		 */
		OpAutoVector<OpString> m_bcc_address;

		/** List of destination addresses.
		 * In MMS can be both email addresses and phone numbers,
		 * in email can be email addresses only,
		 * in SMS can be phone numbers only.
		 *
		 * The format of the email should be compliant with subset of RFC 2822
		 * whose BNF is described in FormValidator::IsValidEmailAddress.
		 *
		 * The format of the phone number should be compliant with subset of RFC 2806
		 * which allows following characters:
		 *	0123456789ABCD*\#pw+-.()
		 */
		OpAutoVector<OpString> m_destination_address;

		/** For email - "Reply-to" field,
		 * for MMS - emails or phone numbers,
		 * for SMS - phone numbers.
		 */
		OpAutoVector<OpString> m_reply_to_address;

		/** "From" field for email or sender phone number for SMS and MMS.
		 */
		OpString m_sender_address;

		/** "Subject" field for email and MMS.
		 * Applies for email and MMS, doesn't apply to SMS.
		 */
		OpString m_subject;

		/** Text contents of the message. */
		OpString m_body;

		/** Id of the message, preferrably in RFC 2111 format
		 *	for example "87skc1k7al.fsf@opera.com".
		 *	Every message coming from platform must have m_id set.
		 */
		OpString m_id;

		/** Flag indicating if device user has read the message. */
		BOOL3 m_is_read;

		/** Priority of the message.
		 *
		 * "X-Priority" email field. Possible states:
		 *		OpMessaging::Message::Highest
		 *		OpMessaging::Message::High
		 *		OpMessaging::Message::Normal
		 *		OpMessaging::Message::Low
		 *		OpMessaging::Message::Lowest
		 *		OpMessaging::Message::PriorityUnknown
		 *
		 * Or priority of SMS/MMS. Possible states:
		 *		OpMessaging::Message::High
		 *		OpMessaging::Message::Normal
		 *		OpMessaging::Message::Low
		 *		OpMessaging::Message::PriorityUnknown
		 */
		Priority m_priority;

		/** Timeout left for sending attempts of this message.
		 *  Has absolute date format in seconds since Jan 1, 1970 UTC.
		 *  Applies to SMS and MMS, doesn't apply to email.
		 *
		 *  The value of 0 means there is no value.
		 */
		time_t m_sending_timeout;

		/** Time when the message has been sent.
		 *  Has absolute date format in seconds since Jan 1, 1970 UTC.
		 *
		 *  The value of 0 means there is no value.
		 */
		time_t m_sending_time;

		/** Type of the message. */
		Type m_type;
	};

	/** The platform implementation of OpMessaging::Create() and
	 * OpMessaging::GetEmailAccounts() and OpMessaging::GetSingleEmailAccount()
	 * retrieve the name and the id of the account from the system interface.
	 * The id of the email account id can be used to retrieve further data
	 * like folders or messages from the system or to send mails.
	 */
	struct EmailAccount
	{
		/** Name of the email account.  The name is usually used in a UI to display the account. */
		OpString m_name;

		/** Unique identity string of the account. */
		OpString m_id;
	};

	/** Folder is a container for messages.  Hierarchical structure of folders
	 * can be achieved by using special character '/' as part of m_name.
	 * Used in several OpMessaging methods which operates on folders and
	 * wherever place of a message is needed.
	 *
	 * The platform implementation of OpMessaging can retrieve the folder
	 * information from the system, when it is asked to enumerate the list
	 * of available folders. Core may create a new folder when it asks the
	 * OpMessaging implementation to create a new folder.
	 *
	 * If the folder can store emails, it may reflect the entry in IMAP
	 * mailbox structure.
	 *
	 * A folder of Folder::m_type == OpMessaging::Email is identified by
	 * the tuple (m_type, m_name, m_email_account_id). This means that
	 * different email accounts may each have a folder with the same name.
	 * Folders with type OpMessaging::SMS and OpMessaging::MMS only use
	 * (m_type, m_name) to identify the folder.
	 *
	 * A folder of Folder::m_type == OpMessaging::TypeUnknown is identified by
	 * the tuple (m_type, m_name, m_email_account_id) like the folder of type
	 * OpMessaging::Email. In addition to the emails for the specified account
	 * and folder-name, it also contains the SMS and MMS:
	 * Folder(OpMessaging::TypeUnknown, m_name, m_email_account) =
	 * Folder(OpMessaging::Email, m_name, m_email_account) +
	 * Folder(OpMessaging::SMS, m_name) + Folder(OpMessaging::MMS, m_name).
	 */
	struct Folder
	{
		/** Specifies the type of messages that are stored in the folder.
		 * If the value is OpMessaging::TypeUnknown, then the folder may
		 * contain messages of any type.
		 */
		Message::Type m_type;

		/** The name of the folder.  It may contain '/' to indicate hierarchy.
		 * Folder name can be case-sensitive or case-insensitive (it depends
		 * on platform and on IMAP server).  Since there's no information from
		 * server regarding case-sensitiveness, there's no way to handle it
		 * in consistent manner.
		 *
		 * Se also RFC 3501, section 5.1.
		 */
		OpString m_name;

		/** Used only when m_type == Email or m_type == TypeUnknown. */
		OpString m_email_account_id;
	};


	/** This method is called by core to construct an OpMessaging instance.
	 *
	 * @param[out] new_messaging placeholder for the newly created messaging
	 *		object.  The caller has the responsibility to delete the created
	 *		instance, i.e. call OP_DELETE, when it is no longer needed.
	 * @param[in] listener is a listener provided by the caller. The
	 *		implementation of OpMessaging needs to notify the listener when it
	 *		encounters the corresponding events. It is the responsibility of
	 *		the caller to create and delete the listener instance and to ensure
	 *		that the listener is not deleted before the OpMessaging instance is
	 *		deleted.
	 * @param[out] default_email_account A placeholder for resulting account.
	 *		It's up to platform to decide, which email account is the default one.
	 *		If there is any email account defined, both
	 *		default_email_account.m_name and default_email_account.m_id cannot
	 *		be empty.  Otherwise default_email_account.m_id must be empty.
	 *
	 * @return
	 *		OpStatus::OK,
	 *		OpStatus::ERR_NULL_POINTER - if new_messaging is NULL
	 *			or listener is NULL,
	 *		OpStatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpMessaging** new_messaging, OpMessagingListener* listener, EmailAccount& default_email_account);

	/** Sends SMS, MMS or email message to recipients.
	 *
	 * The listener OpMessagingListener::OnMessageSendingError() method
	 *		should be called upon sending error.
	 * The listener OpMessagingListener::OnMessageSendingSuccess() method
	 *		should be called when message has been sent.
	 *
	 * @param[in] message The message to be sent.
	 *		Upon call to SendMessage, this object takes over ownership of
	 *		the message, which must be cleaned up by OpMessaging when it is
	 *		done with the message.
	 *	@param[in] email_account_id If message type is email, then it should be
	 *		sent using email_account_id.  Otherwise it is unused.
	 *	@return See OpMessaging::Status and OpStatus.  In particular:
	 *		OpStatus::ERR_NULL_POINTER - if message parameter is NULL,
	 *		OpStatus::ERR_NOT_SUPPORTED.
	 */
	virtual OP_MESSAGINGSTATUS SendMessage(Message* message, const uni_char* email_account_id=NULL) = 0;

	/** A callback to call after operation of SaveAttachment has ended. */
	class SaveAttachmentCallback
	{
		public:
			virtual void OnFailed(OP_MESSAGINGSTATUS error) = 0;
			virtual void OnFinished() = 0;
	};
	/** Saves given message attachment on filesystem.
	 *
	 * The save overwrites existing file.
	 *
	 * If destination folder does not exist, SaveAttachment() returns
	 * ERR_FOLDER_DOESNT_EXIST error in callback.
	 *
	 * @param[in] full_filename Absolute destination filename of the attachment.
	 * @param[in] to_save The attachment to save.
	 *		See OpMessaging::Attachment::m_inside_message for the place where
	 *		the attachment resides.
	 * @param[in] callback To be called upon completition of saving or error.
	 *		For more detailed description see OpMessaging::GetFolderNames()
	 *		callback parameter.
	 */
	virtual void SaveAttachment(const uni_char* full_filename, const Attachment* to_save, SaveAttachmentCallback* callback) = 0;

	class SimpleMessagingCallback
	{
		public:
			virtual void OnFailed(OP_MESSAGINGSTATUS error) = 0;
			virtual void OnFinished() = 0;
	};

	/** Creates folder for storing messages.
	 *
	 * In case the folder already exists, the callback's OnFailed should be
	 * called with ERR_FOLDER_ALREADY_EXISTS as argument. See OpMessaging::Status
	 * for other valid error codes.
	 *
	 * @param[in] folder Identity of folder which will be created.
	 *		See OpMessaging::Folder.
	 * @param[in] callback To be called upon completition of folder creation
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void CreateFolder(const Folder* folder, SimpleMessagingCallback* callback) = 0;

	/** Removes folder.
	 *
	 * If the given folder contains messages or other folders and parameter
	 * force == FALSE, the function should call callback's OnFailed with
	 * ERR_FOLDER_NOT_EMPTY.
	 *
	 * @param[in] folder Identity of folder which will be deleted.
	 *		See OpMessaging::Folder.
	 * @param[in] callback To be called upon completition of folder deletion
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 * @param[in] force If FALSE, only empty folder can be deleted.
	 *		If TRUE, folder will be deleted together with its contents.
	 */
	virtual void DeleteFolder(const Folder* folder, SimpleMessagingCallback* callback, BOOL force=FALSE) = 0;

	class GetFolderNamesCallback
	{
		public:
			virtual void OnFailed(OP_MESSAGINGSTATUS) = 0;
			virtual void OnFinished(const OpAutoVector<OpString>* result) = 0;
	};
	/** Get all names of folders of given type.
	 *
	 * Following folders are mandatory and should be at the beginnig of
	 * resulting vector: drafts, inbox, outbox, sentbox.  If the system
	 * provides some natural order of the folders, the platform should pass the
	 * folder names in that order to the callback.
	 *
	 * @param[in] folder_type Only folders of given type will be in result.
	 * @param[in] callback The platform implementation of this method should
	 *		call OpMessaging::GetFolderNamesCallback::OnFinished() when all
	 *		message folders of the specified type have been retrieved
	 *		successfully from the system. In case of an error
	 *		OpMessaging::GetFolderNamesCallback::OnFailed() should be called.
	 *		The caller of this method provides the instance to the callback
	 *		(cannot be NULL) and ensures that it is not deleted before the
	 *		OpMessaging instance is deleted.  In case of partial success (like
	 *		OOM in the middle of collecting results) only OnFailed() should
	 *		be called.
	 *
	 * @param[in] email_account_id If folder_type == Email, then
	 *		email_account_id cannot be NULL.  Resulting folders will be folders
	 *		of given account.
	 */
	virtual void GetFolderNames(Message::Type folder_type, GetFolderNamesCallback* callback, const uni_char* email_account_id=NULL) = 0;

	/** Callback to be called after GetEmailAccounts is done.
	 *
	 * @param[out] result All email accounts.  All of them must have both m_id
	 *		and m_name not empty.
	 */
	class GetEmailAccountsCallback
	{
		public:
			virtual void OnFailed(OP_MESSAGINGSTATUS) = 0;
			/** @param[out] result All EmailAccounts must have both m_id and m_name not empty. */
			virtual void OnFinished(const OpAutoVector<EmailAccount>* result) = 0;
	};
	/** Get all available email accounts.
	 *
	 * @param[in] callback To be called upon completition of fetching accounts
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void GetEmailAccounts(GetEmailAccountsCallback* callback) = 0;

	/** Get email account of given id.
	 *
	 * @param[in] email_account_id Identifies the account to fetch.
	 * @param[in] callback To be called when account is ready or error.
	 *		In case of success, resulting vector shuld contain only one entry.
	 *		For more detailed description see OpMessaging::GetFolderNames()
	 *		callback parameter.
	 */
	virtual void GetSingleEmailAccount(const uni_char* email_account_id, GetEmailAccountsCallback* callback) = 0;

	/** Removes email account together with folders and messages.
	 *
	 * If sending message from given account is in progress,
	 * DeleteEmailAccount should fail with ERR_EMAIL_ACCUNT_IN_USE error.
	 *
	 * @param[in] email_account_id The id of account which will be deleted.
	 * @param[in] callback To be called upon completition of email account
	 *		deletion or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void DeleteEmailAccount(const uni_char* email_account_id, SimpleMessagingCallback* callback) = 0;

	/** Callback to be called after GetMessageCount is done. */
	class GetMessageCountCallback
	{
		public:
			virtual void OnFailed(OP_MESSAGINGSTATUS) = 0;
			/**
			 * @param[in] total Number of all messages in folder.
			 * @param[in] read Number of messages, which are read.
			 * @param[in] unread Number of messages, which are unread.
			 *
			 * Following equation should be fulfilled: total >= read + unread.
			 *
			 * */
			virtual void OnFinished(unsigned int total, unsigned int read, unsigned int unread) = 0;
	};
	/** Gets number of messages in given folder.
	 *
	 * @param[in] folder The folder identity, for which the message count
	 *		will be performed.
	 * @param[in] callback To be called upon completition of the operation
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void GetMessageCount(const Folder* folder, GetMessageCountCallback* callback) = 0;

	/**	Callback to be called after GetMessage is done. */
	class GetMessageCallback
	{
		public:
			virtual void OnFailed(OP_MESSAGINGSTATUS) = 0;
			virtual void OnFinished(const Message* result) = 0;
	};
	/** Gets n-th message from given folder.
	 *
	 * It should have been "message id", but it was not corrected in the JIL spec in the right time.
	 *
	 * @param[in] folder  Identity of folder, from which message
	 *		will be fetched.
	 * @param[in] message_index Index of message in folder, which will be
	 *		fetched, starting from 0.  The order of messages must be set,
	 *		but can be arbitrary.
	 *		WARNING:  This parameter can change to message_id in future
	 *		versions.
	 * @param[in] callback To be called upon completition of the operation
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void GetMessage(const Folder* folder, UINT32 message_index, GetMessageCallback* callback) = 0;

	class MessagesSearchIterationCallback
	{
		public:
		/** Called for every iterated item.
		 *
		 * @return If TRUE, then iteration should be continued.
		 *		If FALSE, then iteration must be stopped.
		 */
		virtual BOOL OnItem(Message* result) = 0;

		/** Called when find operation finishes successfully. */
		virtual void OnFinished() = 0;

		/** Called when the find operation fails.
		 *
		 * @param[in] error See OP_MESSAGINGSTATUS.
		 */
		virtual void OnError(OP_MESSAGINGSTATUS error) = 0;
	};

	/** Calls given callback for each Message of given type.
	 *
	 * This method is called to iterate over all messages of the specified type
	 * in the specified folder. The platform implementation of this method
	 * should call OpMessaging::MessagesSearchIterationCallback::OnItem() for
	 * each message that was found.
	 * OpMessaging::MessagesSearchIterationCallback::OnFinished() should be
	 * called after the last message was found and the iteration has finished
	 * successfully. In case of an error
	 * OpMessaging::MessagesSearchIterationCallback::OnError() should be
	 * called, which finishes the Iterate() operation (no further call to
	 * OpMessaging::MessagesSearchIterationCallback::OnFinished() should be
	 * performed).  Iteration is stopped in the same way, when
	 * OpMessaging::MessagesSearchIterationCallback::OnItem() returns FALSE.
	 *
	 * The implementation of this method should if possible report the messages
	 * to the callback in the same order for each call. However the
	 * implementation may choose the order itself. A natural choice is to use
	 * the order provided by the system. Other possible orders are by date/time
	 * or by message-id.
	 *
	 * @param[in] folder_name If folder_name is not NULL (can be empty string),
	 *		this method should iterate over all messages in the specified
	 *		folder. If the system provides a folder hierarchy, then this method
	 *		should not enter any sub-folders.  If folder_name is NULL, this
	 *		method should iterate over all folders.  If folder_name is NULL and
	 *		one message (with the same id) is in several folders, the message
	 *		will be returned in
	 *		OpMessaging::MessagesSearchIterationCallback::OnIterate() multiply
	 *		times.
	 * @param[in] type The type of Messages which will be iterated.
	 *		If type == TypeUnknown, then iteration is performed on all types
	 *		of Messages.
	 * @param[in] callback Object which is called when operation finishes
	 *		or fails. Can be called synchronously (during execution of this
	 *		function) or asynchronously. Must not be NULL.  For more detailed
	 *		description see OpMessaging::GetFolderNames() callback parameter.

	 * @param[in] email_account_id If type == Email and email_account_id
	 *		is not NULL, then iteration is performed only on messages from
	 *		this account.  Otherwise email_account_id is unused.
	 */
	virtual void Iterate(const uni_char* folder_name, Message::Type type, MessagesSearchIterationCallback* callback, const uni_char* email_account_id=NULL) = 0;

	/** Deletes message or messages from given folder.
	 *  Id indicates whether one particular message or all messages should be deleted
	 *
	 * @param[in] folder Identity of folder message(s) to be deleted is/are located in.
	 *		See OpMessaging::Folder.
	 * @param[in] message_id - The id of the message to be deleted.
	 * NOTE: If message_id's NULL, this means all messages from given folder should be deleted.
	 * @param[in] callback To be called upon completition of message(s) deletion
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void DeleteMessage(const Folder* folder, const uni_char* message_id, SimpleMessagingCallback* callback) = 0;

	/** Puts message into given folder.
	 *
	 * @param[in] msg The message to be put into the folder.
	 * Upon call to PutMessage, this object takes over ownership of
	 * the message, which must be cleaned up by OpMessaging when it is
	 * done with the message.
	 * @param[in] folder Identity of folder message should be put into.
	 *		See OpMessaging::Folder.
	 * @param[in] delete_from_current_folder - If true message should be deleted from the folder it's already in.
	 * If delete_from_current_folder == TRUE and message is not in any folder - nothing is deleted.
	 * NOTE: delete_from_curr_folder parameter in practice allows to recognize whether this put operation is result of copying or moving message
	 * @param[in] callback To be called upon completition of message put
	 *		or error.  For more detailed description see
	 *		OpMessaging::GetFolderNames() callback parameter.
	 */
	virtual void PutMessage(Message* msg, const Folder* folder, BOOL delete_from_current_folder, SimpleMessagingCallback* callback) = 0;

	virtual ~OpMessaging() {}
};

/** Callback class for OpMessaging. */
class OpMessagingListener
{
public:
	/** Called if sending of message couldn't be performed.
	 *
	 *  Note that even if sending message did succeed on the device side,
	 *  there still can be an sending error indicated by server side,
	 *  such as "recipient doesn't exist." In such case OnMessageSendingSuccess
	 *  will be called, but OnMessageSendingError must not be called.
	 *
	 *  @param[out] error Why message sending has failed.
	 *  @param[out] message Which message sending failed. Must not be null.
	 */
	virtual void OnMessageSendingError(OP_MESSAGINGSTATUS error, const OpMessaging::Message* message) = 0;

	/** Called when message has been sent from device.
	 *
	 *  @param[out] message Which message has been sent. Must not be null.
	 */
	virtual void OnMessageSendingSuccess(const OpMessaging::Message* message) = 0;

	/** Called when message has arrived.
	 *
	 *  @param[in] message Message which has arrived. Must not be null.
	 */
	virtual void OnMessageArrived(const OpMessaging::Message* message) = 0;

	virtual ~OpMessagingListener() {}
};

#endif // PI_MESSAGING
#endif // PI_DEVICE_API_OPMESSAGING_H
