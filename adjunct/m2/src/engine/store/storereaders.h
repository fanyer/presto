// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// George Refseth (rfz)

#ifndef STORE_READERS_H
#define STORE_READERS_H

#include "adjunct/m2/src/util/blockfile.h"
#include "adjunct/m2/src/engine/message.h"

/** @brief Class to read a historic opera store
  * @author George Refseth
  */
class StoreReader
{
public:

	// Revision of mailbox storage for opera and
	// mail database
	enum StoreRevision {
		REVISION_UNKNOWN,
		REVISION_1_1,
		REVISION_2_1,
		REVISION_2_2,
		REVISION_2_3,
		REVISION_2_4,
		REVISION_3_0
	};

	/** Constructor
	 */
							StoreReader(){}

	/** Destructor
	  */
	virtual					~StoreReader(){}

	/** Creator
	 */
	static StoreReader *	Create(OpString& error_message, const OpStringC& path_to_maildir);
	
	virtual StoreRevision	GetStoreRevision() = 0;
	
	virtual OP_STATUS		Init(OpString& error_message, const OpStringC& path_to_maildir) = 0;

	virtual OP_STATUS		GetNextAccountMessage(Message& message, UINT16 accountId, BOOL overwrite, BOOL import) = 0;
	
	virtual OP_STATUS		GetNextMessage(Message& message, BOOL overwrite, BOOL import) = 0;
	
	virtual BOOL			MoreMessagesLeft() = 0;
	
	virtual OP_STATUS		InitializeAccount(UINT16 account_nr, Account * account, OpFileLength *total_message_count = NULL) = 0;
};

#endif // STORE_READERS_H
