//
// Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_PROTOCOL_H
#define IMAP_PROTOCOL_H

#include "adjunct/m2/src/backend/compat/crcomm.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"

#include "adjunct/m2/src/backend/imap/imap-flags.h"
#include "adjunct/m2/src/backend/imap/commands/ImapCommandItem.h"

#include "modules/hardcore/timer/optimer.h"

class ImapFolder;
class ImapBackend;
class ImapSequence;
class GenericIncomingProcessor;
class ZlibStream;

/** @brief Manages connection to IMAP server and sends commands
  * @author Arjan van Leeuwen
  *
  * The IMAP4 class can start, restart and end the connection to the IMAP server, and contains logic
  * to queue and send commands to the server. All actions that influence the command queue should
  * also be contained in this class.
  */
class IMAP4 : public ClientRemoteComm, public MessageLoop::Target, public OpTimerListener
{
public:
	/** Constructor
	  * @param backend ImapBackend object this object has to report to
	  * @param id Necessary? ID of this connection
	  */
	IMAP4(ImapBackend* backend, INT8 id);

	virtual ~IMAP4();

	/** Run this function and check return value before using an IMAP4 object
	  */
	virtual OP_STATUS Init();

	/** Connect to the server
	  */
	OP_STATUS Connect();

	/** Disconnect the connection to the server (forcefully - use EmptyQueueAndLogout() for a nicer way)
	  * @param why Reason for disconnect
	  */
	OP_STATUS Disconnect(const OpStringC8& why);

	/** Called when data is received, handles all processing of incoming data
	  */
	OP_STATUS ProcessReceivedData();

	/** Inserts 'command' into the send queue and sends the next command if possible
	  * @param command The command to insert
	  * @param at_start Whether the command should be inserted at the start of the queue
	  */
	OP_STATUS InsertCommand(ImapCommandItem* command, BOOL at_start = FALSE);
	OP_STATUS InsertCommand(ImapCommands::MessageSetCommand* command, BOOL at_start = FALSE);

	/** Run this function when the sent command has been successfully completed
	  * Removes the command from the queue and reinserts its children
	  * Sends the next command in the queue
	  * @param tag Tag of the completed command (DJBHash of tag)
	  */
	OP_STATUS OnSuccessfulComplete(int tag);

	/** Run this function when the sent command has been completed, but with errors
	  * Removes the command and its children from the queue
	  * Sends the next command in the queue
	  * @param tag Tag of the completed command (DJBHash of tag)
	  * @param response_text Message from the server for failed command
	  */
	OP_STATUS OnFailedComplete(int tag, const OpStringC8& response_text);

	/** Send a continuation of the sent command
	  * @param response_text Any text that was sent with the continuation request
	  */
	OP_STATUS OnContinuationRequest(const OpStringC8& response_text);

	/** Signal that the server is going to close the connection
	  */
	OP_STATUS OnBye();

	/** Act on search results, add or edit to other commands if needed
	  * @param search_results Received search results
	  */
	OP_STATUS OnSearchResults(OpINT32Vector& search_results);

	/** Act on expunged message
	  * @param expunged_id Message sequence number of expunged message
	  */
	OP_STATUS OnExpunge(unsigned expunged_id);

	/** Get the last sent command
	  */
	ImapCommandItem* GetSentCommand() const;

	/** Set the capabilities for this connection
	  * @param capability_list List of capabilities, see ImapFlags::Capability
	  */
	void	  SetCapabilities(int capability_list);

	/** Get the capabilities for this connection
	  * @return List of capabilities, see ImapFlags::Capability
	  */
	int		  GetCapabilities() { return m_capabilities; }

	/** Get the backend that created this connection
	  * @return Reference to the backend that created this connection
	  */
	ImapBackend& GetBackend() const { return *m_backend; }

	/** Check whether this connection is responsible for a specified mailbox
	  * @param mailbox The mailbox to check
	  * @return Whether this connection is responsible for mailbox
	  */
	BOOL	  HasMailbox(ImapFolder* mailbox);

	/** Check whether we are connected (or connecting) to a server
	  */
	BOOL	  IsConnected() const { return m_state & (ImapFlags::STATE_CONNECTING | ImapFlags::STATE_CONNECTED); }

	/** Return whether this connection is doing something
	  */
	BOOL	  IsBusy() const { return IsConnected() && m_send_queue.FirstChild() && !m_send_queue.FirstChild()->WaitForNext(); }

	/** Return whether this is an authenticated connection
	  */
	BOOL	  IsAuthenticated() const { return m_state & ImapFlags::STATE_AUTHENTICATED; }

	/** Report a server error to the user
	  * @param error_message Message to show the user
	  */
	void	  ReportServerError(const OpStringC8& error_message) const;

	/** Check for new messages in all folders that this connection maintains
	  * @param force Whether to force a login, even if the connection doesn't maintain folders currently
	  */
	OP_STATUS FetchNewMessages(BOOL force = FALSE);

	/** Force a full sync of all folders
	  */
	OP_STATUS RefreshAll();

	/** Synchronize a specific folder
	  * @param folder Which folder to synchronize
	  */
	OP_STATUS SyncFolder(ImapFolder* folder);

	/** Fetch a message by server UID
	  * @param folder Folder on server where message is located
	  * @param uid Server UID of the message to fetch
	  * @param force_fetch_body If set, always fetch the body, otherwise, get headers only or body according to preferences
	  * @param force_fetch_full_body If set, always fetch the complete body, otherwise, get text only or complete according to preferences
	  * @param high_priority Whether the message should be fetched with high priority
	  * @param force_no_bodystructure If set, forces a fetch without bodystructure
	  */
	OP_STATUS FetchMessageByUID(ImapFolder* folder, unsigned uid, BOOL force_fetch_body = FALSE, BOOL force_fetch_full_body = FALSE, BOOL high_priority = FALSE, BOOL force_no_bodystructure = FALSE);

	/** Fetch messages by sequence set ID
	  * @param folder Folder on server where message is located
	  * @param id_from Start of the set to fetch (inclusive)
	  * @param id_to End of the set to fetch (inclusive)
	  * @param force_full_message If set, always fetch the complete message, otherwise, get headers only or body according to preferences
	  */
	OP_STATUS FetchMessagesByID(ImapFolder* folder, unsigned id_from, unsigned id_to);

	/** Check if there is an operation in the queue that affects a certain message
	  * @param folder Folder on server where message is located
	  * @param uid Server UID of the message to check
	  * @return Whether there is an operation waiting in the queue that will change this message
	  */
	BOOL	  IsOperationInQueue(ImapFolder* folder, unsigned uid) const;

	/** Make this connection responsible for a folder
	  * @param folder Folder that this connection will have to maintain
	  */
	OP_STATUS AddFolder(ImapFolder* folder);

	/** Remove a folder from this connection's list
	  * @param folder Folder that this connection should stop maintaining
	  */
	void	  RemoveFolder(ImapFolder* folder);

	/** Take over the connection and all details from another connection
	  * @param other_connection The connection to take over
	  */
	OP_STATUS TakeOverCompletely(IMAP4& other_connection);

	/** Write commands in this connection's queue to the offline log
	  * @param offline_log Offline log to write to
	  */
	OP_STATUS WriteToOfflineLog(OfflineLog& offline_log);

	/** Generate a new tag for a command sent over this connection
	  * @param new_tag Where to save the tag
	  */
	OP_STATUS GenerateNewTag(OpString8& new_tag);

	/** Set that the connection is in a certain state
	  * @param state The state to set
	  */
	void	  AddState(ImapFlags::State state) { m_state |= state; }

	/** Remove a certain state for this connection
	  * @param state The state to remove
	  */
	void	  RemoveState(ImapFlags::State state) { m_state &= ~state; }

	/** Start a secure TLS connection with the current server
	  */
	OP_STATUS UpgradeToTLS();

	/** Start a timer that will regenerate an IDLE event when it times out
	  */
	OP_STATUS StartIdleTimer();

	/** Stop the Idle timer
	  */
	OP_STATUS StopIdleTimer();

	/** Add a folder to the subscribed folder list
	  * @param folder_name Folder to add
	  */
	OP_STATUS AddToSubscribedFolderList(const OpStringC8& folder_name);

	/** Reset the list of subscribed folders, call when preparing LSUB or when finished updating the list
	  */
	void	  ResetSubscribedFolderList() { m_subscribed_folder_list.Clear(); }

	/** Sync the actual folder list with m_subscribed_folder_list, call after successful LSUB
	  */
	OP_STATUS ProcessSubscribedFolderList();

	/** Get the next selectable folder available (not the current folder), or NULL if none available
	  */
	ImapFolder* GetNextFolder();

	/** Gets the ID of this connection
	  */
	INT8	  GetId() const { return m_id; }

	/** Gets the state of this connection
	  */
	int		  GetState() const { return m_state; }

	/** Gets the current folder this connection has selected
	  */
	ImapFolder* GetCurrentFolder() const { return m_current_folder; }

	/** Sets the current folder this connection has selected
	  */
	void	  SetCurrentFolder(ImapFolder* folder) { m_current_folder = folder; m_current_folder_selected = false; }

	/** Set that the current folder is successfully selected
	  */
	void	  SetCurrentFolderIsSelected() { m_current_folder_selected = true; }

	/** Has a folder selected, used to know whether we can update the highest mod seq safely or if we have to wait for a sync to complete
	  */
	bool	  HasSelectedFolder() { return m_current_folder_selected; }

	//ClientRemoteComm interface
	void	  OnClose(OP_STATUS rc);
	void      OnRestartRequested();
	void	  RequestMoreData() {}

	//OpTimerListener interface
	void      OnTimeOut(OpTimer* timer);

	// MessageLoop::Target interface
	OP_STATUS Receive(OpMessage message);

	OP_STATUS RequestDisconnect() { return m_loop->Post(MSG_M2_DISCONNECT); }

	// Stop the connection time out from cutting the connection when we don't expect more data
	void	  StopConnectionTimer();

protected:
	/** Removes 'command' from the send queue
	  * @param command The command to remove
	  * @param reinsert_children Whether to reinsert the children of this command into the queue
	  */
	void	  RemoveCommand(ImapCommandItem* command, BOOL reinsert_children);

	/** Posts a message that will lead to sending the next command
	  */
	OP_STATUS DelayedSendNextCommand();

	/** If it exists, sends the next available unsent command in the queue and asks for another send
	  */
	OP_STATUS SendNextCommand();

	/** Send a command
	  * @param command_to_send The command to send
	  */
	OP_STATUS SendCommand(ImapCommandItem* command_to_send);

	/** Replace the first command in the queue
	  * @param replace_with Queue or command to replace the first command with
	  */
	OP_STATUS ReplaceFirstCommand(ImapCommandItem* replace_with);

	/** Generates commands that should be executed when there is nothing to do
	  */
	OP_STATUS GenerateIdleCommands();

	/** Removes items from the queue that are not saveable
	  */
	void	  CreateSaveableQueue();

	/** Run when we were disconnected unexpectedly
	  */
	OP_STATUS OnUnexpectedDisconnect();

	/** Sets up a state for this connection so that command can be sent
	  * @param command The command for which to setup a state
	  * @param succeeded Set to TRUE if and only if we are now in a correct state to execute this command
	  */
	OP_STATUS SetupStateForCommand(ImapCommandItem* command, BOOL& succeeded);

	/** Try to make the connection secure
	  */
	OP_STATUS MakeSecure();

	/** Inserts commands necessary to setup and login to a new connection
	 */
	OP_STATUS SetupAndLogin();

	/** Process any data in the parser that is fully parsed
	  */
	OP_STATUS ProcessParsedData();

	/** Get first unsent command
	  */
	ImapCommandItem* FirstUnsent() const { return m_send_queue.FirstChild() ? (m_send_queue.FirstChild()->IsSent() ? m_send_queue.FirstChild()->Suc() : m_send_queue.FirstChild()) : 0; }

	/** Get first selectable folder for this connection
	  */
	ImapFolder* GetFirstSelectableFolder() const;

	/** Initialize processor
	  */
	virtual OP_STATUS InitProcessor();

	/** Start authentication
	  */
	virtual OP_STATUS Authenticate();

	/** Decoding / encoding of data
	  */
	OP_STATUS DecodeReceivedData(char*& buf, unsigned& buf_length);
	OP_STATUS EncodeAndSendData(OpString8& command_string);

	ImapFolder*				   m_current_folder;			///< Current folder selected by connection
	ImapBackend* 			   m_backend;					///< Parent backend, pointer is always valid
	ImapCommandItem 		   m_send_queue;
	INT8					   m_id;
	bool					   m_current_folder_selected;
	OpSortedVector<ImapFolder, PointerLessThan<ImapFolder> >
							   m_own_folders; ///< Folders that this connection is responsible for
	OpINTSortedVector		   m_subscribed_folder_list;	///< List of index ids of confirmed subscribed folders
	GenericIncomingProcessor*  m_processor;
	int						   m_state;
	int						   m_capabilities;
	MessageLoop*			   m_loop;
	OpTimer					   m_idle_timer;
	ZlibStream*				   m_compressor;
	ZlibStream*				   m_decompressor;
};

#endif // IMAP_PROTOCOL_H
