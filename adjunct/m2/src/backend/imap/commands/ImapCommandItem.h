//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// @author Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_COMMAND_ITEM_H
#define IMAP_COMMAND_ITEM_H

#include "modules/util/tree.h"
#include "adjunct/m2/src/include/defs.h"

#include "adjunct/m2/src/engine/progressinfo.h"

class OfflineLog;
class IMAP4;
class ImapFolder;

namespace ImapCommands
{
	class MessageSetCommand;
}

/** @brief Represents IMAP commands sent to server
  * @author Arjan van Leeuwen
  *
  * The ImapCommandItem class represents either a real IMAP command that is to be sent to the IMAP server, or a
  * command that can be replaced by real IMAP commands.
  */
class ImapCommandItem : private Tree
{
public:
	ImapCommandItem();
	virtual ~ImapCommandItem();

	// Some functions to ease tree navigation
	ImapCommandItem* Last()			const { return static_cast<ImapCommandItem*>(Tree::Last());		}
	ImapCommandItem* First()		const { return static_cast<ImapCommandItem*>(Tree::First());	}
	ImapCommandItem* Parent()		const { return static_cast<ImapCommandItem*>(Tree::Parent());	}
	ImapCommandItem* Suc()			const { return static_cast<ImapCommandItem*>(Tree::Suc());		}
	ImapCommandItem* Prev()			const { return static_cast<ImapCommandItem*>(Tree::Prev());		}
	ImapCommandItem* LastChild()	const { return static_cast<ImapCommandItem*>(Tree::Last());		}
	ImapCommandItem* FirstChild()	const { return static_cast<ImapCommandItem*>(Tree::First());	}
	ImapCommandItem* Next()			const;

	// Tree/queue manipulation functions
	/** Make this command depend on other_command. Existing dependencies of other_command will now depend
	  * on this command.
	  * @param other_command Command that this command should depend on
	  * @param protocol Connection that maintains this item
	  */
	void DependsOn(ImapCommandItem* other_command, IMAP4& protocol);

	/** Insert this command at the end of the specified queue
	  * @param queue Queue to insert the command into
	  * @param protocol Connection that maintains this item
	  */
	void IntoQueue(ImapCommandItem* queue, IMAP4& protocol);

	/** Insert this command at the start of the specified queue
	  * @param queue Queue to insert the command into
	  * @param protocol Connection that maintains this item
	  */
	void IntoStartOfQueue(ImapCommandItem* queue, IMAP4& protocol);

	/** Make this command follow other_command (i.e. will be executed after other_command).
	  * There is no dependency relation.
	  * @param other_command Command that this command should follow
	  * @param protocol Connection that maintains this item
	  */
	void Follow(ImapCommandItem* other_command, IMAP4& protocol);

	/** Remove this command and its dependencies from any queue it's in
	  * @param protocol Connection that maintains this item
	  */
	void Out(IMAP4& protocol);

	/** Get if this command has been sent
	  * @return TRUE if the command has been sent
	  */
	BOOL	  IsSent() const { return m_sent; }

	/** Set if this command has been sent
	  * @param sent Sent or not?
	  */
	void	  SetSent(BOOL sent = TRUE) { m_sent = sent; }

	/** Get the tag on this command
	  * @return the tag
	  */
	int		  GetTag() const { return m_sent_tag; }

	/** Sets the string 'command' to the complete IMAP command string this item represents, to be sent to the IMAP server
	  * @param command Output string
	  * @param protocol Communication channel by which this command is going to be sent
	  */
	OP_STATUS GetImapCommand(OpString8& command, IMAP4& protocol);

	///////////////////////////////////////////////
	// All functions below this line are virtual //
	///////////////////////////////////////////////

	/** Get if this command is unnecessary right now
	  * @param protocol The connection that wants to send this command
	  */
	virtual BOOL		IsUnnecessary(const IMAP4& protocol) const { return FALSE; }

	/** Executes all actions necessary to make this command safe to send. Always run this before sending an IMAP command.
	  * @param protocol Communication channel by which this command is going to be sent
	  */
	virtual OP_STATUS	PrepareToSend(IMAP4& protocol) { return OpStatus::OK; }

	/** Executes all actions necessary when this command is completed
	  * @param protocol Communication channel that received successful complete
	  */
	virtual OP_STATUS	OnSuccessfulComplete(IMAP4& protocol) { return OpStatus::OK; }

	/** Executes all actions necessary when this command has failed
	  * @param protocol Communication channel that received the failure
	  * @param failed_msg Message from the server for failed command
	  */
	virtual OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg);

	/** Executes any necessary actions when the command received an untagged BYE response, if not handled it will be handled by OnUnexpectedDisconnect 
	  * @param protocol Communication channel that received the failure
	  * @return TRUE if handled, FALSE if not
	  */
	virtual BOOL		HandleBye(IMAP4& protocol) { return FALSE; }

	/** Whether this command should be removed if it causes disconnection
	  */
	virtual BOOL		RemoveIfCausesDisconnect(IMAP4& protocol) const { return FALSE; }

	/** Returns whether this command will change a certain message
	  * @param folder Folder where the message is located on the server
	  * @param uid Server UID of the message
	  */
	virtual BOOL		Changes(ImapFolder* folder, unsigned uid) const { return FALSE; }

	/** Returns whether this command will use a password
	  */
	virtual BOOL		UsesPassword() const { return FALSE; }

	/** Adjusts any server ids saved in this command when expunged_server_id is expunged
	  * @param expunged_server_id The server_id that has been reported as expunged by the server
	  */
	virtual OP_STATUS	OnExpunge(unsigned expunged_server_id) { return OpStatus::OK; }

	/** Which state is required to execute this command
	  * @param secure Whether we use a secure connection
	  * @return Required states, see ImapFlags::State
	  */
	virtual int			NeedsState(BOOL secure, IMAP4& protocol) const { return GetDefaultFlags(secure, protocol); }

	/** Whether this command can be saved and reexecuted
	  * Used to see whether command should be retained across sessions
	  * @return whether this command can be saved and reexecuted
	  */
	virtual BOOL		IsSaveable() const { return FALSE; }

	/** Make this command 'saveable' (so that it can be reused on a different connection)
	  */
	virtual void		MakeSaveable() { SetSent(FALSE); ResetContinuation(); }

	/** Prepare this (continuation) command to send the continuation
	  * Implement this function if your command needs extra steps to create a continuation
	  * @param protocol Connection calling this function
	  * @param response_text Any text that was sent with the continuation request
	  */
	virtual OP_STATUS	PrepareContinuation(IMAP4& protocol, const OpStringC8& response_text) { return OpStatus::OK; }

	/** See if this (continuation) command should not be sent until later
	  * @return Whether the sender should wait sending this
	  */
	virtual BOOL		WaitForNext() const { return FALSE; }

	/** Change this command based on search results received in response to this command
	  * @param search_results Received search_results
	  */
	virtual OP_STATUS	OnSearchResults(OpINT32Vector& search_results) { return OpStatus::OK; }

	/** Write the command to an offline log
	 * @param offline_log The log to write to
	 */
	virtual OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log) { return OpStatus::OK; }

	/** Retrieve progress information for this command and its children
	  * @return Progress action corresponding to this command
	  */
	virtual ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::CONTACTING; }

	/** Update the progress of this item (by one)
	  * @param protocol Connection calling this function
	  */
	virtual void		UpdateProgress(IMAP4& protocol) {}

	/** Get the amount of finished progress units for this command
	  */
	virtual unsigned	GetProgressCount() const { return 0; }

	/** Get the total amount of finished progress units for this command
	  */
	virtual unsigned	GetProgressTotalCount() const { return 1; }

	/** Is this command a meta-command? Meta-commands are commands that will be
	  * replaced with a queue generated by the meta-command.
	  * Implement GetExpandedQueue() if this returns TRUE!
	  * @return Whether this command is a meta-command
	  */
	virtual BOOL		IsMetaCommand(IMAP4& protocol) const { return FALSE; }

	/** Get an expanded queue to replace a meta-command, only implement if IsMetaCommand() is TRUE
	  * @return a command or queue that the meta-command should be replaced with
	  */
	virtual ImapCommandItem* GetExpandedQueue(IMAP4& protocol) { return NULL; }

	/** See if a command needs a certain mailbox to be selected
	 * @return If non-NULL, the mailbox that needs to be selected to execute this command
	 */
	virtual ImapFolder* NeedsSelectedMailbox() const { return NULL; }

	/** See if a command needs a certain mailbox to be unselected
	 * @return if non-NULL, the mailbox that can't be selected when this command is executed
	 */
	virtual ImapFolder* NeedsUnselectedMailbox() const { return NULL; }

	/** Returns the mailbox this command operates on
	  * @return The mailbox this command operates on, if any
	  */
	virtual ImapFolder* GetMailbox() const { return NULL; }

	/** Whether this command can be extended by another command
	  */
	virtual BOOL		CanExtendWith(const ImapCommands::MessageSetCommand* command) const { return FALSE; }

	/** Extend this command with the message set of a different command
	  */
	virtual OP_STATUS	ExtendWith(const ImapCommands::MessageSetCommand* command, IMAP4& protocol) { return OpStatus::ERR; }

	/** React to an 'appenduid' response code (UIDPLUS only)
	  */
	virtual OP_STATUS	OnAppendUid(IMAP4& protocol, unsigned uid_validity, unsigned uid) { return OpStatus::OK; }

	/** React to a 'copyuid' response code (UIDPLUS only)
	  */
	virtual OP_STATUS	OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set) { return OpStatus::OK; }

	/** To commands are completely equal to each other
	  */
	virtual BOOL		IsSameAs(const ImapCommandItem* other_item) const { return FALSE; }

	/** Get flags used for fetching (ONLY for commands that fetch messages)
	  */
	virtual int			GetFetchFlags() const { return 0; }

	/** Is this command a continuation?
	  * NB: If this is implemented and the command is saveable, ResetContinuation() should be implemented too!
	  * @return Whether this command is a continuation
	  */
	virtual BOOL		IsContinuation() const { return FALSE; }

	/** Reset the continuation state of a command
	  */
	virtual void		ResetContinuation() {}

	/** Set depth of a text part in the message to be fetched (only applicable to commands that fetch parts)
	  * @param uid UID of message that text part depth was received for
	  * @param textpart_depth Depth of the text part to be fetched
	  */
	virtual OP_STATUS	OnTextPartDepth(unsigned uid, unsigned textpart_depth, IMAP4& protocol) { return OpStatus::OK; }

protected:
	/** Append the IMAP command
	  * @param command IMAP command (excluding a tag) should be appended to this string
	  * @param protocol Connection that asked for the command
	  */
	virtual OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return OpStatus::ERR; }

	/** Add a dependency to this command. Will return an error if dependency is NULL
	  * @param dependency Dependency to add to this command
	  * @param protocol Connection
	  */
	OP_STATUS			AddDependency(ImapCommandItem* dependency, IMAP4& protocol);

	int					GetDefaultFlags(BOOL secure, IMAP4& protocol) const;

	// Data needed by queue manager
	int					m_sent_tag;
	BOOL				m_sent;

private:
	void				Out() { Tree::Out(); }
	void				Into(ImapCommandItem* list) { Tree::Into(list); }
};

#endif // IMAP_COMMAND_ITEM_H
