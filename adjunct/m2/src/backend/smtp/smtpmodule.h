// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef SMTPMODULE_H
#define SMTPMODULE_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"

class Account;
class SMTP;

// ----------------------------------------------------

class SmtpBackend : public ProtocolBackend
{

public:
	SmtpBackend(MessageDatabase& database);
    ~SmtpBackend();

    OP_STATUS Init(Account* account);
    AccountTypes::AccountType GetType() const {return AccountTypes::SMTP;}
    OP_STATUS SettingsChanged(BOOL startup=FALSE);
    UINT16  GetDefaultPort(BOOL secure) const { return secure ? 25 : 25; }
	UINT32  GetAuthenticationSupported() {return (UINT32)1<<AccountTypes::NONE |
												   (UINT32)1<<AccountTypes::PLAIN |
												   (UINT32)1<<AccountTypes::LOGIN |
												   (UINT32)1<<AccountTypes::CRAM_MD5 |
                                                   (UINT32)1<<AccountTypes::AUTOSELECT;}
    OP_STATUS PrepareToDie() { return OpStatus::OK; }

    OP_STATUS FetchMessages(BOOL enable_signalling) { return OpStatus::ERR; }
	OP_STATUS FetchMessage(message_index_t, BOOL user_request, BOOL force_complete) { return OpStatus::ERR; }
	OP_STATUS StopFetchingMessages() { return OpStatus::OK; }
	OP_STATUS InsertExternalMessage(Message& message) { return OpStatus::OK; }

    OP_STATUS SendMessage(Message& message, BOOL anonymous);
	OP_STATUS StopSendingMessage();

    OP_STATUS Sent(message_gid_t uid, OP_STATUS transmit_status);

	OP_STATUS Connect();
    OP_STATUS Disconnect();

private:
    SMTP*		m_protocol;
};

// ----------------------------------------------------

#endif // SMTPMODULE_H

// ----------------------------------------------------
