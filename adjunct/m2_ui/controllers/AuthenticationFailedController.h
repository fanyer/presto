/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef AUTHENTICATION_FAILED_CONTROLLER_H
#define AUTHENTICATION_FAILED_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"


class AuthenticationFailedController : public OkCancelDialogContext
{
public:
	AuthenticationFailedController(ProtocolBackend& backend, const OpStringC& server_message);

protected:
	virtual void OnCancel();
	virtual void OnOk();

private:
	virtual void InitL();

	void InitOptionsL();

	OpProperty<OpString> m_username;
	OpProperty<OpString> m_password;
	OpProperty<INT32>    m_method;
	OpProperty<bool>     m_remember_password;

	ProtocolBackend&     m_backend;
	const OpStringC&     m_server_message;
};

#endif // AUTHENTICATION_FAILED_CONTROLLER_H
