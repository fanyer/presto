/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef _OPERA_AUTHMYOPERA_H_INCLUDED_
#define _OPERA_AUTHMYOPERA_H_INCLUDED_

#include "modules/windowcommander/OpTransferManager.h"
#include "modules/opera_auth/opera_auth_base.h"
#include "modules/xmlutils/xmlfragment.h"

//# define MYOPERA_AUTH_URL	"https://auth.n.nevada.oslo.opera.com/xml"

#define MYOPERA_SECRET_KEY "345gdfge678rtD787FGRERT"

class OpTransferItem;
class TransferItem;

enum AuthRequestType
{
	OperaAuthAuthenticate,
	OperaAuthChangePassword,
	OperaAuthCreateUser,
	OperaAuthCreateDevice,
	OperaAuthReleaseDevice
};

class MyOperaAuthentication : 
		public OperaAuthentication,
		public OpTransferListener
{
public:
	MyOperaAuthentication();
	virtual ~MyOperaAuthentication();

	OP_STATUS Authenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force = FALSE);
	OP_STATUS ChangePassword(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password);
	OP_STATUS CreateAccount(OperaRegistrationInformation& reg_info);
	OP_STATUS ReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename);
	void Abort(BOOL userAction=FALSE);
	BOOL OperationInProgress();

	// OpTransferListener
	void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void OnReset(OpTransferItem* transferItem) {};
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) {};

	void OnTimeout(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

protected:
	virtual /* for selftest */
	OP_STATUS StartHTTPRequest(XMLFragment& xml);
	OperaAuthError PreValidateUsername(OpStringC& username);
	OperaAuthError PreValidatePassword(OpStringC& password);
	OperaAuthError PreValidateEmail(OpStringC& email);
	OperaAuthError PreValidateDevicename(OpStringC& devicename, OpStringC& username, OpStringC& install_id);
	BOOL PreValidateCredentials(OperaRegistrationInformation& reg_info);
	void CallListenerWithError(OperaAuthError error);

private:
	OperaRegistrationInformation m_reg_info;
	TransferItem			*m_transferItem;
	AuthRequestType			m_request_type;
};

#endif //_OPERA_AUTHMYOPERA_H_INCLUDED_
