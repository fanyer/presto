/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Arjan van Leeuwen (arjanl)
*/

#ifndef _UIDL_MANAGER_H
#define _UIDL_MANAGER_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/MessageDatabase/CommitListener.h"
#include "modules/search_engine/SingleBTree.h"

class MessageDatabase;
class Account;
// remove this before peregrine alpha 1:
#define M2_FIRST_PEREGRINE_COMPATIBILITY

struct POP3Info;
class  PopBackend;

class UidlManager : public CommitListener
{
public:
	enum Flags {
		IS_READ=0,
		PARTIALLY_FETCHED=1
	};

	/** Constructor
	  * @param database MessageDatabase to use
	  */
	UidlManager(MessageDatabase& database);

	/** Destructor
	  */
	~UidlManager();

	/** Initialization function, run before using any other function
 	  * @param account_id Account ID of the account this UIDL manager will manage
	  */
	OP_STATUS Init(int account_id);

	/** Adds a UIDL
	  * Call this function whenever a message has been successfully fetched
	  * @param uidl UIDL of the message that has been fetched
	  * @param m2_id m2 message id of the message that has been fetched
	  */
	OP_STATUS AddUIDL(const OpStringC8& uidl, message_gid_t m2_id);

	/** Removes a UIDL
	  * @param uidl UIDL to remove
	  */
	OP_STATUS RemoveUIDL(const OpStringC8& uidl);

	/** Get M2 ID for a UIDL
	  * @param uidl UIDL to check for
	  * @return the id, or 0 if not found
	  */
	message_gid_t GetUIDL(const OpStringC8& uidl);


	/** Get the received time for a UIDL
	  * @param uidl UIDL to check for
	  * @return the time the message was received, or 0 if not found
	  */
	time_t GetReceivedTime(const OpStringC8& uidl);

	/** Get a flag for a UIDL
	  * @param uidl UIDL to check for
	  * @param flag - the flag you want to know the value of
	  * @return the value of the flag
	  */

	BOOL GetUIDLFlag(const OpStringC8& uidl, Flags flag);
	
	/** Set a flag for a UIDL
	  * @param uidl UIDL to check for
	  * @param flag - the flag you want to know the value of
	  * @param value - TRUE or FALSE
	  * @return the value of the flag
	  */
	OP_STATUS SetUIDLFlag(const OpStringC8& uidl, Flags flag, BOOL value);

	/** Update old UIDL storages
	  * @param account_id Account we're updating
	  * @param old_version Old version of UIDL
	  */
	OP_STATUS UpdateOldUIDL(Account* account, int old_version);

	// From CommitListener
	virtual void OnCommitted();

private:
#ifdef M2_MERLIN_COMPATIBILITY
	OP_STATUS UpdateMerlinUIDL(Account* account);
#endif // M2_MERLIN_COMPATIBILITY

#ifdef M2_KESTREL_BETA_COMPATIBILITY
	OP_STATUS UpdateKestrelBetaUIDL(Account* account);
#endif // M2_KESTREL_BETA_COMPATIBILITY

#ifdef M2_95X_COMPATIBILITY
	OP_STATUS UpdateKestrelFinalUIDL(Account* account);
#endif // M2_95X_COMPATIBILITY

#ifdef M2_FIRST_PEREGRINE_COMPATIBILITY
	OP_STATUS UpdateFirstPeregrineUIDL(Account* account);
#endif

protected:
	friend class MailRecovery;

	class UidlKey
	{
	public:

		UidlKey(const OpStringC8& uidl, message_gid_t m2_id);

		OpStringC8    GetUIDL() const { return m_uidl; }
		message_gid_t GetM2ID() const { return m_data[1]; }
		time_t		  GetTime() const { return m_data[2]; }

		void		  SetFlags( INT32 flags) { m_data[3] = flags; }
		INT32		  GetFlags() const { return m_data[3]; }

		BOOL operator<(const UidlKey& rhs) const;

	private:
		static const size_t MaxUIDLLength = 70;

		unsigned m_data[4];
		char     m_uidl[MaxUIDLLength];
	};
private:
	SingleBTree<UidlKey> m_tree;
	MessageDatabase& m_database;
};

#endif // _UIDL_MANAGER_H
