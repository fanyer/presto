//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_UID_MANAGER_H
#define IMAP_UID_MANAGER_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/MessageDatabase/CommitListener.h"

#include "modules/search_engine/SingleBTree.h"

class MessageDatabase;

class ImapUidManager : public CommitListener
{
public:
	/** Constructor
	  */
	ImapUidManager(MessageDatabase& message_database);

	/** Destructor
	  */
	~ImapUidManager();

	/** Initialization function, run before using any other function
	  * @param account_id Account ID of the account this UID manager will manage
	  */
	OP_STATUS     Init(int account_id);

	/** Get a message by its UID
	  * @param folder_index_id Index number of the folder this message resides in
	  * @param uid UID of this message on the server
	  * @return M2 message ID of the message, or 0 if not found
	  */
	message_gid_t GetByUID(index_gid_t folder_index_id, unsigned uid);

	/** Add a server UID or replace information
	  * @param folder_index_id Index number of the folder this message resides in
	  * @param uid UID of message to add
	  * @param m2_message_id M2 message ID of the message
	  */
	OP_STATUS     AddUID(index_gid_t folder_index_id, unsigned uid, message_gid_t m2_message_id = 0);

	/** Remove a server UID
	  * @param folder_index_id Index number of the folder this message resides in
	  * @param uid UID of message to remove
	  */
	OP_STATUS     RemoveUID(index_gid_t folder_index_id, unsigned uid);

	/** Remove messages with UID [from .. to) from M2s message store and the UID manager
	  * NB: 'to' itself is not removed
	  * @param folder_index_id Index number of the folder this message resides in
	  * @param from Remove messages with a UID at least this value
	  * @param to Remove messages with a UID less than this value, or 0 for no limit
	  */
	OP_STATUS     RemoveMessagesByUID(index_gid_t folder_index_id, unsigned from, unsigned to = 0);

	/** Gets the highest UID in the database for a certain folder
	  * @param folder_index_id Index number of the folder to check
	  * @return Highest UID in this folder
	  */
	unsigned			  GetHighestUID(index_gid_t folder_index_id);

	// From CommitListener
	virtual void  OnCommitted();

#ifndef SELFTEST
private:
#endif //SELFTEST
	friend class MailRecovery;

	struct ImapUidKey
	{
		ImapUidKey() {}
		ImapUidKey(index_gid_t folder, unsigned uid, message_gid_t message_id) { data[0] = folder; data[1] = uid; data[2] = message_id; }

		BOOL operator<(const ImapUidKey& right) const { return (data[0] < right.data[0] || (data[0] == right.data[0] && data[1] < right.data[1])); }

		index_gid_t GetFolder()				 const { return data[0]; }
		unsigned GetUID()				 const { return data[1]; }
		message_gid_t GetMessageId() const { return data[2]; }

		UINT32 data[3];
	};

	MessageDatabase& m_message_database;
	SingleBTree<ImapUidKey> m_tree;
};

#endif // IMAP_UID_MANAGER_H
