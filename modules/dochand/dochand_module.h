/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_DOCHAND_DOCHAND_MODULE_H
#define MODULES_DOCHAND_DOCHAND_MODULE_H

#include "modules/hardcore/opera/module.h"

class WindowManager;
class OperaURL_Generator;


/**
 * Class which limits the amount of fraud checks being done after a request fails.
 * 
 * This is to stop DOS-ing the site check server once it comes up again because all
 * clients try to hammer it simultaniously.  Hence if a request fails we will not try
 * another attempt until the time FRAUD_CHECK_MINIMUM_GRACE_PERIOD has expired, and with
 * exponential fall off for each successive failed attempt until the grace period is
 * FRAUD_CHECK_MAXIMUM_GRACE_PERIOD.
 * 
 * See bug CORE-18851
 */
class FraudCheckRequestThrottler
{
public:
	FraudCheckRequestThrottler() : m_last_failed_request(0.0), m_grace_period(0) {}

	void					RequestFailed();
	void					RequestSucceeded() { m_grace_period = 0; }  // Reset grace period
	
	/// Should making a fraud check be allowed now, or are we still in the grace period from a previous failed check
	BOOL					AllowRequestNow();

private:
	double					m_last_failed_request;  // GetRuntimeMS() time of the last failed request or 0.0 if nothing has ever failed
	unsigned				m_grace_period;         // seconds from one failed request until we can try a new request
};

class DochandModule : public OperaModule
{
public:
	DochandModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	WindowManager* window_manager;
#ifdef TRUST_RATING
	FraudCheckRequestThrottler fraud_request_throttler;
#endif // TRUST_RATING

#ifdef WEBSERVER_SUPPORT
	OperaURL_Generator* unitewarningpage_generator;
#endif // WEBSERVER_SUPPORT
};

#define g_windowManager g_opera->dochand_module.window_manager
#define windowManager g_windowManager
#ifdef TRUST_RATING
# define g_fraud_check_request_throttler g_opera->dochand_module.fraud_request_throttler
#endif // TRUST_RATING

#define DOCHAND_MODULE_REQUIRED

#endif // !MODULES_DOCHAND_DOCHAND_MODULE_H
