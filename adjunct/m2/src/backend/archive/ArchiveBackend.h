/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ARCHIVE_BACKEND_H
#define ARCHIVE_BACKEND_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"

/**
 * @brief Backend for archiving mails
 * @author Arjan van Leeuwen
 *
 * This backend doesn't keep any actual server connections - it only makes sure
 * that there is a structure to keep archived mails in. Used for mails that
 * should no longer remain on the server (with IMAP for example) or when
 * deleting an account, but keeping the messages.
 */
class ArchiveBackend : public ProtocolBackend
{
public:
	ArchiveBackend(MessageDatabase& database);

	// From ProtocolBackend
	virtual OP_STATUS Init(Account* account);

	virtual AccountTypes::AccountType GetType() const;

	virtual OP_STATUS SettingsChanged(BOOL startup);

	virtual OP_STATUS PrepareToDie();

	virtual UINT32    GetAuthenticationSupported();

	virtual IndexTypes::Type GetIndexType() const;

	virtual const char* GetIcon(BOOL progress_icon);

	virtual OP_STATUS FetchMessage(message_index_t id, BOOL user_request, BOOL force_complete);

	virtual OP_STATUS FetchMessages(BOOL enable_signalling);

	virtual OP_STATUS StopFetchingMessages();

	virtual OP_STATUS Connect();

	virtual OP_STATUS Disconnect();

	virtual OP_STATUS InsertExternalMessage(Message& message);

	virtual OP_STATUS MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id);

	virtual OP_STATUS CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id);

private:
	OP_STATUS ArchiveMessage(message_gid_t message_id);

	index_gid_t GetMirrorIndex(index_gid_t orig_index_id);
};

#endif // ARCHIVE_BACKEND_H
