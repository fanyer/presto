/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef RECOVERY_H
#define RECOVERY_H

#include "modules/search_engine/SingleBTree.h"
#include "modules/util/opfile/opfile.h"

class MailRecovery
{

public: 

	OP_STATUS			CheckConsistencyAndFixAllMailDBs();

	OP_STATUS			CheckAndFixMessageIdDatabase();

	OP_STATUS			CheckAndFixFeedDatabases();

	OP_STATUS			CheckAndFixPOPDatabase(int account_id);

	OP_STATUS			CheckAndFixIMAPDatabase(int account_id);


						/**
						 * CheckAndFixAccountSpecificDBs
						 * Checks consistency on all account databases (pop, imap, rss) and fixes them if necessary
						 * returns OpStatus::OK if the recovery can continue
						 * returns OpStatus::ERR_OUT_OF_RANGE if the account version number is incompatible
						 * it might be that m2 needs to upgrade
						 */
	OP_STATUS			CheckAndFixAccountSpecificDBs();
	
	OP_STATUS			CheckAndFixLexicon();

	OP_STATUS			CheckAndFixIndexerStringTable();

						/**
						* Goes through all indexes and loops through all messages to find messages that are not in store.
						* This function needs to run while store and indexer are still instanciated,
						* unlike the other functions in this class
						*/
	OP_STATUS			GhostBuster();

						MailRecovery();
						~MailRecovery();

private:
	
	OpFile*				m_recovery_log;
};

#endif // RECOVERY_H
