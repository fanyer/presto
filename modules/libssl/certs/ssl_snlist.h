/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSLSNLIST_H
#define SSLSNLIST_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_ServerName_List : public SSL_Error_Status
{
public:
	class SNList_string : public OpAutoVector<OpString8>
	{
	public:
		OP_STATUS AddString(const char *str, int len);
	};
	SNList_string DNSNames;
	SSL_varvector16_list   IPadr_List;
	OpAutoVector<OpString8> CommonNames;

	OpString8 Namelist;

	OpString Matched_name;

public:
	SSL_ServerName_List();
	void Reset();

	BOOL MatchName(ServerName *servername);
	BOOL MatchNameRegexp(ServerName *servername, OpStringC8 *name, BOOL reg_exp);
private:
	void InternalInit();
};

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLSNLIST_H */
