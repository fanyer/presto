// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "smtpmodule.h"

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/message.h"

#include "adjunct/m2/src/backend/smtp/smtp-protocol.h"
#include "adjunct/m2/src/util/autodelete.h"

#include <assert.h>

// ----------------------------------------------------

SmtpBackend::SmtpBackend(MessageDatabase& database)
	: ProtocolBackend(database)
	, m_protocol(NULL)
{
}

SmtpBackend::~SmtpBackend()
{
	OP_DELETE(m_protocol);
}

OP_STATUS SmtpBackend::Init(Account* account)
{
    if (!account)
        return OpStatus::ERR_NULL_POINTER;

    m_account = account;

    return OpStatus::OK;
}

OP_STATUS SmtpBackend::SettingsChanged(BOOL startup)
{
	OP_DELETE(m_protocol);
	m_protocol = NULL;

    return OpStatus::OK;
}

OP_STATUS SmtpBackend::SendMessage(Message& message, BOOL anonymous)
{
	if (!HasRetrievedPassword())
		return RetrievePassword();

    if (!m_protocol)
    {
        OP_STATUS ret;
        if ((ret=Connect()) != OpStatus::OK)
            return ret;
    }

    return m_protocol->SendMessage(message, anonymous);
}

OP_STATUS SmtpBackend::Sent(message_gid_t uid, OP_STATUS transmit_status)
{
    return m_account->Sent(uid, transmit_status);
}

OP_STATUS SmtpBackend::StopSendingMessage()
{
	return Disconnect();
}

OP_STATUS SmtpBackend::Connect()
{
    OP_ASSERT(!m_protocol);
    if (m_protocol)
        return OpStatus::OK;

    OpString8 servername;
	RETURN_IF_ERROR(GetServername(servername));

    UINT16 port;
	RETURN_IF_ERROR(GetPort(port));

    m_protocol = OP_NEW(SMTP, (this));
    if (!m_protocol)
        return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret = m_protocol->Init(servername, port);
	if (OpStatus::IsError(ret))
    {
        OP_DELETE(m_protocol);
        m_protocol = NULL;
        return ret;
    }

    return OpStatus::OK;
}


OP_STATUS SmtpBackend::Disconnect()
{
    if (!m_protocol)
        return OpStatus::OK; //Already disconnected

	return m_protocol->Finished();
}


// ----------------------------------------------------

#endif //M2_SUPPORT
