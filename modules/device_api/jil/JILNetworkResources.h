/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DEVICEAPI_JIL_JILNETWORKRESOURCES_H
#define DEVICEAPI_JIL_JILNETWORKRESOURCES_H

class AuthenticationRequestHandler;
class JILNetworkRequestHandler;

class JilNetworkResources
	: private MessageObject
{
	Head m_requests;
	OpString8 m_securityToken;
	AuthenticationRequestHandler *m_authRequest;

public:
	JilNetworkResources();
	~JilNetworkResources();

	OP_STATUS Construct();
	
	struct JILNetworkRequestHandlerCallback
	{
		virtual ~JILNetworkRequestHandlerCallback() {}
		virtual OP_STATUS SetErrorCodeAndDescription(int code, const uni_char* description) = 0;
		virtual void Finished(OP_STATUS error) = 0;
	};

	struct GetUserAccountBalanceCallback: public JILNetworkRequestHandlerCallback
	{
		virtual OP_STATUS SetCurrency(const uni_char* balance) = 0;
		virtual void SetCash(const double &cash) = 0;
	};
	void GetUserAccountBalance(GetUserAccountBalanceCallback *callback);

	struct GetUserSubscriptionTypeCallback: public JILNetworkRequestHandlerCallback
	{
		virtual OP_STATUS SetType(const uni_char* type) = 0;
	};
	void GetUserSubscriptionType(GetUserSubscriptionTypeCallback *callback);

	struct GetSelfLocationCallback: public JILNetworkRequestHandlerCallback
	{
		virtual void SetCellId(int cellId) = 0;
		virtual void SetLatitude(const double &latitude) = 0;
		virtual void SetLongitude(const double &longitude) = 0;
		virtual void SetAccuracy(const double &accuracy) = 0;
	};
	void GetSelfLocation(GetSelfLocationCallback *callback);

private:
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void PrepareTokenRequiringRequest(JILNetworkRequestHandlerCallback *callback, JILNetworkRequestHandler *handler);
	OP_STATUS GetSecurityToken();
};

#endif // DEVICEAPI_JIL_JILNETWORKRESOURCES_H
