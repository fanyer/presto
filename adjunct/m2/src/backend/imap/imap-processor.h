//
// Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_PROCESSOR_H
#define IMAP_PROCESSOR_H

#include "adjunct/m2/src/include/defs.h"

#include "adjunct/m2/src/backend/imap/imap-parseelements.h"
#include "adjunct/m2/src/backend/imap/imap-namespace.h"
#include "adjunct/m2/src/backend/imap/imap-folder.h"
#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/util/parser.h"

template<typename T> class IncomingParser;
class Message;

class GenericIncomingProcessor
{
public:
	/** Destructor
	  */
	virtual ~GenericIncomingProcessor() {}

	/** Process new incoming data
	  * @param data Incoming data
	  * @param length Length of data
	  */
	virtual OP_STATUS Process(char* data, unsigned length) { return OpStatus::OK; }

	/** Reset the processor (removes all items in parse stack)
	 */
	virtual void Reset() {}
};

/** @brief Processes data received from IMAP server
  * @author Arjan van Leeuwen
  *
  * The ImapProcessor class has all the necessary logic to parse and process
  * received IMAP commands. It depends on an external parser to do the actual
  * parsing of commands, which calls back into ImapProcessor for processing the
  * parsed commands.
  */
class ImapProcessor : public GenericIncomingProcessor
{
public:
	/** Constructor
	  * @param parent IMAP4 protocol object this processor reports to
	  */
	ImapProcessor(IMAP4& parent);

	/** Destructor
	  */
	virtual ~ImapProcessor();

	/** Initialize the internal scanner and parser, call this and check result before calling any other methods
	  */
	virtual OP_STATUS Init();

	/**** For the owner ****/

	/** Process incoming data
	  * @param data Data that came in
	  * @param length Length of data
	  */
	virtual OP_STATUS Process(char* data, unsigned length);

	/**** For the parser (processing when a command has been successfully parsed) ****/

	/** Process a continuation request
	  * @param text Text that came with the continuation request
	  */
	OP_STATUS Continue(ImapUnion* text) { return m_parent.OnContinuationRequest(text ? text->m_val_string : NULL); }

	/** Process a tagged response
	  * @param tag The tag on the response (DJBHash of tag)
	  * @param state State reported for this tag
	  * @param response_code Response code supplied with the state
	  * @param response_text Response text supplied with the state
	  */
	OP_STATUS TaggedState(int tag, int state, int response_code, ImapUnion* response_text);

	/** Process an untagged state response
	  * @param state Untagged state reported
	  * @param response_code Response code supplied with the state
	  * @param response_text Response text supplied with the state
	  */
	OP_STATUS UntaggedState(int state, int response_code, ImapUnion* response_text);

	/** Process an untagged bye response
	  */
	OP_STATUS Bye() { return m_parent.OnBye(); }

	/** Process a capability list
	  * @param capability_list List of capabilities for this server (flags)
	  */
	void Capability(int capability_list) { m_parent.SetCapabilities(capability_list); }

	/** Process an enabled list
	  * @param capability_list List of capabilities for this server (flags)
	  */
	void Enabled(int capability_list) { if (capability_list & ImapFlags::CAP_QRESYNC) m_parent.AddState(ImapFlags::STATE_ENABLED_QRESYNC); }

	/** Process an 'appenduid' response code (UIDPLUS only)
	  * @param uid_validity UID validity of the mailbox a message was appended to
	  * @param uid UID of the appended message
	  */
	OP_STATUS AppendUid(unsigned uid_validity, unsigned uid);

	/** Process a 'copyuid' response code (UIDPLUS only)
	 * @param uid_validity UID validity of the mailbox messages were copied to
	 * @param source_set message sequence set (UIDs) of the original messages
	 * @param dest_set message sequence set (UIDs) of the new messages
	 */
	OP_STATUS CopyUid(unsigned uid_validity, ImapNumberVector* source_set, ImapNumberVector* dest_set);

	/** Process 'flags' or 'permanentflags' response
	  * @param flags The flags applicable to the current mailbox
	  * @param keywords The keywords applicable to the current mailbox
	  * @param permanent_flags whether flags describes only flags that can be set permanently
	  */
	OP_STATUS Flags(int flags, ImapSortedVector* keywords, BOOL permanent_flags);

	/** Set if the current mailbox is readonly
	  * @param read_only TRUE if the current mailbox is readonly, FALSE otherwise
	  */
	OP_STATUS SetReadOnly(BOOL read_only) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->SetReadOnly(read_only) : OpStatus::ERR_NULL_POINTER); }

	/** Process 'uidnext' response
	 * @param new_uid_next UIDNEXT value for the current mailbox
	 */
	OP_STATUS UidNext(int new_uid_next) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->SetUidNext(new_uid_next) : OpStatus::ERR_NULL_POINTER); }

	/** Process 'uidvalidity' response
	 * @param new_uidvalidity UIDVALIDITY value for the current mailbox
	 */
	OP_STATUS UidValidity(int new_uid_validity) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->SetUidValidity(new_uid_validity) : OpStatus::ERR_NULL_POINTER); }

	/** Process 'unseen' response
	 * @param new_unseen UNSEEN value for the current mailbox
	 */
	OP_STATUS Unseen(int new_unseen) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->SetUnseen(new_unseen) : OpStatus::ERR_NULL_POINTER); }

	/** Process 'highestmodseq' response
	 * @param highest_mod_seq HIGHESTMODSEQ value for the current mailbox
	 */
	OP_STATUS HighestModSeq(UINT64 highest_mod_seq) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->UpdateModSeq(highest_mod_seq) : OpStatus::ERR_NULL_POINTER); }

	/** Process 'nomodseq' response
	  */
	OP_STATUS NoModSeq() { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->UpdateModSeq(0) : OpStatus::ERR_NULL_POINTER); }

	/** Process an 'exists' response
	  * @param new_exists Number of messages the current mailbox
	  */
	OP_STATUS Exists(int new_exists) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->SetExists(new_exists) : OpStatus::ERR_NULL_POINTER); }

	/** Process a 'recent' response
	  * @param new_recent Number of messages marked 'recent' in current mailbox
	  */
	OP_STATUS Recent(int new_recent) { return (m_parent.GetCurrentFolder() ? m_parent.GetCurrentFolder()->SetRecent(new_recent) : OpStatus::ERR_NULL_POINTER); }

	/** Process an 'expunge' response
	  * @param expunged_message Message sequence number of the expunged message
	  */
	OP_STATUS Expunge(int expunged_message) { return m_parent.OnExpunge(expunged_message); }

	/** Process a 'vanished' response
	  * @param vanished_uids UIDs of messages that vanished
	  * @param earlier Whether the UIDs vanished earlier or now
	  */
	OP_STATUS Vanished(ImapNumberVector* vanished_uids, BOOL earlier) { return (m_parent.GetCurrentFolder() && vanished_uids ? m_parent.GetCurrentFolder()->Vanished(*vanished_uids) : OpStatus::ERR_NULL_POINTER); }

	/** Process received namespace data
	  * @param personal Personal namespace data
	  * @param other Other namespace data
	  * @param shared Shared namespace data
	  */
	OP_STATUS Namespace(ImapVector* personal, ImapVector* other, ImapVector* shared);

	/** Process a 'search' response
	  * @param search_results Messages found that conform to the previously sent search criteria
	  */
	OP_STATUS Search(ImapNumberVector* search_results);

	/** Process a 'list' or 'lsub' response
	  * @param flags List flags for this mailbox
	  * @param delimiter Delimiter character used in this mailbox
	  * @param mailbox Name of the mailbox
	  * @param is_lsub Whether the mailbox is subscribed (lsub response)
	  */
	OP_STATUS List(int flags, char delimiter, ImapUnion* mailbox, BOOL is_lsub = FALSE);

	/** Process a part of a status response
	  * @param status_type Type of status message received
	  * @param status_value The value for this type
	  */
	OP_STATUS Status(int status_type, UINT64 status_value);

	/** Process a 'fetch' response. Assumes that m_message has been set with values for this message.
	  */
	OP_STATUS Fetch();

	/**** For the scanner or parser (functions/variables called/accessed when scanning or parsing) ****/

	/** Do all necessary actions to start processing a new command
	  */
	void NextCommand();

	/** Do stuff when a parse error occurs
	  * @param error_message Error message that can be displayed to the user
	  */
	void OnParseError(const char* error_message);

	/** Set an error status that can later be reported as the return status of parsing
	  * @param status The status to set
	  * @return status
	  */
	OP_STATUS SetErrorStatus(OP_STATUS status) { m_error_status = status; return status; }

	/** Reset the processor (removes all items in parse stack)
	  */
	virtual void Reset();

	/** Set message properties
	  */
	void SetMessageServerId(int server_id) { m_message.m_server_id = server_id; }
	void SetMessageUID(unsigned uid) { m_message.m_uid = uid; }
	void SetMessageFlags(int flags) { m_message.m_flags = flags | ImapFlags::FLAGS_INCLUDED_IN_FETCH; }
	void SetMessageKeywords(ImapSortedVector* keywords) { m_message.m_keywords = keywords; }
	void SetMessageBodyStructure(ImapBody* body_structure) { m_message.m_body_structure = body_structure; }
	void SetMessageRawText(int type, ImapUnion* raw_text);
	void SetMessageSize(int size) { m_message.m_size = size; }
	void SetMessageModSeq(UINT64 mod_seq) { m_message.m_mod_seq = mod_seq; }

	/** Reset the 'buffer' message
	  */
	void ResetMessage() { op_memset(&m_message, 0, sizeof(m_message)); }

	/** Get parser this processor uses
	  */
	GenericIncomingParser* GetParser() { return m_parser; }

private:
	struct ImapMessage
	{
		int m_server_id;
		int m_uid;
		int m_flags;
		ImapSortedVector* m_keywords;
		ImapBody* m_body_structure;
		ImapUnion* m_raw_headers;
		ImapUnion* m_raw_mime_headers;
		ImapUnion* m_raw_text_complete;
		ImapUnion* m_raw_text_part;
		int m_size;
		UINT64 m_mod_seq;
	};

	OP_STATUS NewMessage();
	OP_STATUS UpdateMessage(message_gid_t message_id, BOOL safety_check = TRUE);
	OP_STATUS UpdateMessage(Message& message, BOOL set_flags);
	void	  UpdateBodyStructure(Message& message, ImapBody* body_structure, BOOL& first, unsigned depth = 0);
	OP_STATUS AddNamespaces(ImapVector* namespaces, ImapNamespace::NamespaceType namespace_type);
	OP_STATUS UpdateKeywords(message_gid_t message_id, ImapSortedVector* keywords, bool new_message);
	OP_STATUS m_error_status;

	IMAP4&		 m_parent;

	ImapMessage	 m_message;					///< A variable that can be used by the parser to save message data

	GenericIncomingParser* m_parser;
};

#endif // IMAP_PROCESSOR_H
