/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_CERTSTATUS_H
#define LIBSSL_CERTSTATUS_H

#if defined _NATIVE_SSL_SUPPORT_ && !defined TLS_NO_CERTSTATUS_EXTENSION
typedef SSL_varvector24 TLS_OCSP_Response;

class TLS_CertificateStatusResponse : public SSL_Handshake_Message_Base
{
private:
	enum{
		TLS_CSR_STATUS_TYPE = BASERECORD_START_VALUE+1,
		TLS_CSR_START_VALUE
	};
public:
	DataStream_IntegralType<TLS_CertificateStatusType,1> status_type;
	TLS_OCSP_Response	ocsp_response;
	DataStream_ByteArray	unknown_data;

private:
	void InternalInit();

public:
	TLS_CertificateStatusResponse();
	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);
	virtual void PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "TLS_CertificateStatusResponse";}
#endif
};
#endif

#endif // LIBSSL_CERTSTATUS_H
