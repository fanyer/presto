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

#ifndef SSLSTAT_H
#define SSLSTAT_H

#if defined _NATIVE_SSL_SUPPORT_


#include "modules/util/smartptr.h"
#include "modules/libssl/handshake/sslhand3.h"

class SSL_CipherDescriptions;
typedef OpSmartPointerWithDelete<SSL_CipherDescriptions> SSL_CipherDescriptions_Pointer;

class ServerName;
class SSL_CertificateHandler;
class SSL_SessionStateRecordList;
class SSL_CertificateItem;
class SSL_CertificateHandler_List;
class SSL_CertificateHandler_ListHead;
class SSL_KeyExchange;
class SSL_Port_Sessions;


#include "modules/libssl/protocol/stat_enum.h"

class SSL_SessionStateRecordHead : public SSL_Head{
public:
	SSL_SessionStateRecordList* First() const{
		return (SSL_SessionStateRecordList *)SSL_Head::First();
	};       //pointer
	SSL_SessionStateRecordList* Last() const{
		return (SSL_SessionStateRecordList *)SSL_Head::Last();
	};     //pointer
};

#include "modules/libssl/protocol/ssl_portsess.h"
#include "modules/libssl/protocol/ssl_sess.h"
#include "modules/libssl/protocol/ssl_connsess.h"

#endif
#endif // SSLSTAT_H
