/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DEVICEAPI_JIL_JILNETWORKREQUESTHANDLER_H
#define DEVICEAPI_JIL_JILNETWORKREQUESTHANDLER_H

#include "modules/device_api/jil/JILNetworkResources.h"
#include "modules/hardcore/timer/optimer.h"

class XMLFragment;
class XMLExpandedName;

class JILNetworkRequestHandler: public Link, private MessageObject, private OpTimerListener
{
protected:
	enum ServerError { SE_INVALID_TOKEN = 9001, SE_TRY_AGAIN = 9002};
public:
	enum State { STATE_NEW, STATE_WAITING, STATE_FAILED, STATE_NEW_TOKEN_NEEDED, STATE_FINISHED};

	JILNetworkRequestHandler(JilNetworkResources::JILNetworkRequestHandlerCallback *callback);
	virtual ~JILNetworkRequestHandler();

	virtual OP_STATUS SendRequest() = 0;

	State GetState();
	OP_STATUS SetSecurityToken( const char* securityToken);
	void FinishCallBackAndChangeState(OP_STATUS status);

protected:
	virtual OP_STATUS TakeResult(XMLFragment &fragment) = 0;

	void ChangeState(State state);

	OP_STATUS SendRequestInternal(const char *path, const char *xmlContent);
	OP_STATUS StringToInt(const uni_char *s, int &i);
	OP_STATUS StringToDouble(const uni_char *s, double &d);

	virtual OP_STATUS HandleServerError(int code);

private:
	BOOL EnterFirstElement(XMLFragment &fragment, const XMLExpandedName &name);
	OP_STATUS SetLoadingCallbacks(URL_ID url_id);
	OP_STATUS TakeErrorCode(XMLFragment &fragment, int &code);

	void NotifyCallbackWithErrorDescription(XMLFragment &fragment);

	virtual void OnTimeOut(OpTimer* timer);
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

protected:
	OpString8 m_securityToken;
	JilNetworkResources::JILNetworkRequestHandlerCallback *m_callback;

private:
	OpTimer m_retryTimer;
	int m_retryCount;
	State m_state;
	URL m_url;
};

#endif // DEVICEAPI_JIL_JILNETWORKREQUESTHANDLER_H
