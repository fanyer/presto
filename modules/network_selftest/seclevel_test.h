/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#ifndef SEC_LEVEL_TESTER_H
#define SEC_LEVEL_TESTER_H

#ifdef SELFTEST

#include "modules/network_selftest/justload.h"

class Security_Level_Tester : public JustLoad_Tester
{
private:
	int security_level_status;

public:
	Security_Level_Tester(int sec_level) : security_level_status(sec_level)
	{
#ifndef SSL_CHECK_EXT_VALIDATION_POLICY
		// Do not expect Extended Validation security levels unless TWEAK_LIBSSL_CHECK_EXT_VALIDATION_STATUS is enabled
		if (security_level_status > SECURITY_STATE_FULL)
			security_level_status = SECURITY_STATE_FULL;
#endif
	}

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
	{
		switch(event)
		{
		case URLDocST_Event_Data_Received:
			{
				URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect);

				if(status == URL_LOADED)
				{
					int got_level = url.GetAttribute(URL::KSecurityStatus);
					if(got_level < security_level_status)
					{
						ReportFailure("Wrong Security status. Expected %d, got %d", security_level_status, got_level);
						return FALSE;
					}
				}
				else if(status != URL_LOADING)
				{
					ReportFailure("Some kind of loading failure %d.", status);
					return FALSE;
				}
			}
			return TRUE;
		}
		return JustLoad_Tester::Verify_function(event, status_code);
	}	

};

#endif  // SELFTEST
#endif  // SEC_LEVEL_TESTER_H

