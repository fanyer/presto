//
// Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_FOLDER_H
#define IMAP_FOLDER_H

#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/m2/src/include/defs.h"

class Index;
class ImapBackend;
class Message;
class IMAP4;
class ImapCommandItem;

/** @brief Represents a folder or 'mailbox' on the IMAP server
  * @author Arjan van Leeuwen
  *
  * The ImapFolder class represents a folder on the IMAP server and forms the connection between
  * the folder on the server and the local folder as contained in the M2 Index.
  */
class ImapFolder
{
public:
	ImapFolder(ImapBackend& backend, unsigned exists, unsigned uid_next, unsigned uid_validity, UINT64 last_known_mod_seq, index_gid_t index_id);

	static OP_STATUS Create(ImapFolder*& folder, ImapBackend& backend, const OpStringC8& name, char delimiter = '/', int flags = 0, unsigned exists = 0, unsigned uid_next = 0, unsigned uid_validity = 0, UINT64 last_known_mod_seq = 0);

	static OP_STATUS Create(ImapFolder*& folder, ImapBackend& backend, const OpStringC& name, char delimiter, unsigned exists, unsigned uid_next, unsigned uid_validity, UINT64 last_known_mod_seq);

	/** To be called when this folder is opened and server signals expunged message
	  * @param expunged_message_id The id of the expunged message as signaled by server
	  */
	OP_STATUS Expunge(int expunged_message_id);

	/** To be called when this folder is opened and server signals vanished messages
	  * @param vanished_uids UIDs of vanished messages
	  */
	OP_STATUS Vanished(const OpINT32Vector& vanished_uids);

	/** Set the map between server_id and uid, will synchronise local and remote if necessary
	  * @param server_id ID on the server (sequence id)
	  * @param uid Unique ID on the server
	  */
	OP_STATUS SetMessageUID(unsigned server_id, unsigned uid);

	/** Call this function after the last available UID has been set with SetMessageUID
	  */
	OP_STATUS LastUIDReceived();

	/** Add a UID to this folder, means that this UID has been fetched
	  * @param uid UID that is fetched
	  * @param m2_id M2 message id of the fetched message
	  */
	OP_STATUS AddUID(unsigned uid, message_gid_t m2_id);

	/** Get a message store ID by specifying the server ID
	  * @param server_id ID on the server (sequence id)
	  */
	message_gid_t GetMessageByServerID(int server_id);

	/** Get a message store ID by specifying the UID
	  * @param uid UID on the server
	  */
	message_gid_t GetMessageByUID(unsigned uid);

	/** Set if folder is subscribed or unsubscribed
	  * @param subscribed Whether the folder is subscribed or not
	  * @param sync_now Whether to sync the folder at this point
	  */
	OP_STATUS SetSubscribed(BOOL subscribed, BOOL sync_now = TRUE);

	/** Check whether folder is subscribed
	  * @return Whether folder is subscribed
	  */
	BOOL	  IsSubscribed() const { return m_subscribed; }

	/** Folder is not selectable
	  * @param force Go through all normal actions of marking unselectable, even
	  *              if the folder is already marked as unselectable
	  * @param remove_messages Remove messages in the index - used when unsubscribing
	  */
	OP_STATUS MarkUnselectable(BOOL force = FALSE, BOOL remove_messages = FALSE);

	/** Check whether folder is selectable
	 * @return Whether folder is selectable
	 */
	BOOL	  IsSelectable() const { return m_selectable; }

	/** Set the exists value for this folder
	  * @param exists New exists value
	  * @param do_sync Whether to synchronize messages depending on value of exists
	  */
	OP_STATUS SetExists(unsigned exists, BOOL do_sync = TRUE);

	unsigned  GetFetched() const { return m_uid_map.GetCount(); }

	unsigned  GetExists() const { return m_exists; }

	OP_STATUS SetUidNext(unsigned uid_next);

	unsigned  GetUidNext() const { return m_uid_next; }

	OP_STATUS SetUidValidity(unsigned uid_validity);

	unsigned  GetUidValidity() const { return m_uid_validity; }

	OP_STATUS UpdateModSeq(UINT64 mod_seq, bool from_fetch_response = false);

	UINT64	  LastKnownModSeq() const { return m_last_known_mod_seq; }

	OP_STATUS SetUnseen(unsigned unseen) { m_unseen = unseen; return OpStatus::OK; }

	OP_STATUS SetRecent(unsigned recent) { m_recent = recent; return OpStatus::OK; }

	/** Get the index for this folder, or NULL if not available
	  */
	Index*	  GetIndex();

	index_gid_t	GetIndexId() { return m_index_id; }

	/** Create an index for this folder if it did not already exist
	  */
	OP_STATUS CreateIndexForFolder();

	OP_STATUS SetName(const OpStringC8& name, char delimiter) { return SetName(name, UNI_L(""), delimiter); }

	OP_STATUS SetName(const OpStringC16& name, char delimiter) { return SetName("", name, delimiter); }

	OP_STATUS ApplyNewName();

	const OpStringC8& GetName() const { return m_name; }

	const OpStringC8& GetQuotedName() const { return m_quoted_name; }

	const OpStringC16& GetName16() const { return m_name16; }

	const OpStringC8& GetOldName() const { return m_quoted_name; }

	const OpStringC8& GetNewName() const { return m_quoted_new_name; }

	/** Get the name that should be used to display this folder
	  * @return Name of the folder without the full path
	  */
	const uni_char* GetDisplayName() const;

	char	  GetDelimiter() const { return m_delimiter; }

	void	  SetDelimiter(char delimiter) { m_delimiter = delimiter; }

	OP_STATUS SetNewName(const OpStringC8& new_name);

	int		  GetSettableFlags() const { return m_settable_flags; }

	int		  GetPermanentFlags() const { return m_permanent_flags; }

	void	  SetFlags(int flags, BOOL permanent);

	void	  SetListFlags(int flags);

	BOOL	  IsScheduledForSync() const { return m_scheduled_for_sync > 0; }

	BOOL	  IsScheduledForMultipleSync() const { return m_scheduled_for_sync > 1; }

	void	  IncreaseSyncCount() { m_scheduled_for_sync++; }

	void	  DecreaseSyncCount() { m_scheduled_for_sync--; }

	OP_STATUS GetInternetLocation(unsigned uid, OpString8& internet_location) const;

	OP_STATUS SetReadOnly(BOOL readonly) { m_readonly = readonly; return OpStatus::OK; }

	OP_STATUS DoneFullSync();

	BOOL	  NeedsFullSync() const { return g_timecache->CurrentTime() - m_last_full_sync > ForceFullSync; }

	void	  RequestFullSync() { m_last_full_sync = 0; }

	static OP_STATUS EncodeMailboxName(const OpStringC16& source, OpString8& target);

	static OP_STATUS DecodeMailboxName(const OpStringC8& source, OpString16& target);

	/** Reads UIDs of all messages contained in this folder and adds them to
	  * the UID manager. Use when updating from UID-manager-less account
	  */
	OP_STATUS InsertUIDs();

	/** Reads the uids in 'container' and converts them to M2 ids
	  * @param container Vector containing uids, will contain M2 ids if successful
	  */
	OP_STATUS ConvertUIDToM2Id(OpINT32Vector& container);

	/** Saves data about this folder to the IMAP account configuration file
 	  * @param commit Whether to commit data to disk
	  */
	OP_STATUS SaveToFile(BOOL commit = FALSE);

	/** Gets the highest UID known in this folder
	  * @return The highest UID known in this folder
	  */
	unsigned GetHighestUid();

private:
	OP_STATUS ResetFolder();

	OP_STATUS SetName(const OpStringC8& name8, const OpStringC16& name16, char delimiter);

	int						m_scheduled_for_sync;
	unsigned				m_exists;
	unsigned				m_uid_next;
	unsigned				m_uid_validity;
	UINT64					m_last_known_mod_seq;
	UINT64					m_next_mod_seq_after_sync;
	unsigned				m_unseen;
	unsigned				m_recent;
	OpINTSortedVector		m_uid_map;
	OpString8				m_name;
	OpString8				m_quoted_name;
	OpString8				m_quoted_new_name;
	OpString				m_name16;
	OpString8				m_internet_location_format;
	char					m_delimiter;
	int						m_settable_flags;
	int						m_permanent_flags;
	BOOL					m_readonly;
	time_t					m_last_full_sync;
	index_gid_t				m_index_id;
	BOOL					m_subscribed;
	BOOL					m_selectable;

	ImapBackend&			m_backend;

	static const time_t		ForceFullSync = 3600;	///< Force a full sync every x seconds
};

#endif // IMAP_FOLDER_H
