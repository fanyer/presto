// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef POPMODULE_H
#define POPMODULE_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/backend/pop/UidlManager.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"

// ----------------------------------------------------

//#define AUTODETECT_POP_PLAINTEXT_AUTH

class Account;
class POP3;

// ----------------------------------------------------

class PopBackend : public ProtocolBackend
{
public:
    PopBackend(MessageDatabase& database);
    ~PopBackend();

	// ProtocolBackend:

    OP_STATUS Init(Account* account);
    AccountTypes::AccountType GetType() const {return AccountTypes::POP; }
    UINT16    GetDefaultPort(BOOL secure) const { return secure ? 110 : 110; }
	BOOL	  GetUseSSL() const				  { UINT16 port; return OpStatus::IsSuccess(GetPort(port)) && port == 995; }
	UINT32    GetAuthenticationSupported()	  {  return (UINT32)1 << AccountTypes::NONE |
														(UINT32)1 << AccountTypes::PLAINTEXT |
														(UINT32)1 << AccountTypes::APOP |
														(UINT32)1 << AccountTypes::CRAM_MD5 |
														(UINT32)1 << AccountTypes::AUTOSELECT; }
	OP_STATUS PrepareToDie()				  { return OpStatus::OK; }

	BOOL	  Verbose()						  { return TRUE; }

	OP_STATUS SettingsChanged(BOOL startup=FALSE);

    OP_STATUS FetchMessages(BOOL enable_signalling);

	BOOL	  HasExternalMessage(Message& message);
	OP_STATUS InsertExternalMessage(Message& message);
	OP_STATUS WriteToOfflineLog(OfflineLog& offline_log);

	OP_STATUS StopFetchingMessages();

	OP_STATUS Connect();
    OP_STATUS Disconnect();

	OP_STATUS FetchMessage(message_index_t idx, BOOL user_request, BOOL force_complete);
	BOOL	  IsScheduledForFetch(message_gid_t id) const;

	OP_STATUS RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently);
	OP_STATUS RemoveMessage(const OpStringC8& internet_location);
	OP_STATUS ReadMessages(const OpINT32Vector& message_ids, BOOL read);
	OP_STATUS EmptyTrash(BOOL done_removing_messages);

    OP_STATUS SignalStartSession();
	OP_STATUS SignalEndSession(int message_count);

	OP_STATUS UpdateStore(int old_version);

	UidlManager& GetUIDLManager() { return m_uidl; }
	MessageDatabase& GetMessageDatabase() { return ProtocolBackend::GetMessageDatabase(); }

private:
	POP3*              m_protocol;
	UidlManager		   m_uidl;
	OpINTSortedVector  m_scheduled_messages;
};

// ----------------------------------------------------

#endif // POPMODULE_H

// ----------------------------------------------------
