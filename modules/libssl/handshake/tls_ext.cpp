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

#include "core/pch.h"

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/handshake/tls_ext.h"
#include "modules/libssl/handshake/cert_status.h"

//uint32 Size(TLS_Extension_ID){return 2;}
//uint32 Size(TLS_ServerNameType){return 1;}

TLS_ExtensionItem::TLS_ExtensionItem()
: extension_id(TLS_Ext_MaxValue)
{
	InternalInit();
}

void TLS_ExtensionItem::InternalInit()
{
	AddItem(&extension_id);
	AddItem(&extension_data);
}


TLS_ServerNameItem::TLS_ServerNameItem()
: name_type(TLS_SN_MaxValue)
{
	InternalInit();
}

void TLS_ServerNameItem::InternalInit()
{
	AddItem(&name_type);
	AddItem(&host_name);
}

void TLS_ServerNameItem::SetHostName(const OpStringC8 &name)
{
	name_type = TLS_SN_HostName;
	host_name.Set(name.CStr());
}


#ifndef TLS_NO_CERTSTATUS_EXTENSION
TLS_OCSP_Request::TLS_OCSP_Request()
{
	InternalInit();
}

void TLS_OCSP_Request::InternalInit()
{
	AddItem(&responder_id_list);
	AddItem(&request_extensions);
}

void TLS_OCSP_Request::ConstructL(SSL_ConnectionState *pending)
{
	responder_id_list.Resize(0);

	extern OP_STATUS CreateOCSP_Extensions(SSL_varvector16 &extenions);
	CreateOCSP_Extensions(request_extensions);
	pending->session->sent_ocsp_extensions = request_extensions;
	pending->session->ocsp_extensions_sent=TRUE;
}


TLS_CertificateStatusRequest::TLS_CertificateStatusRequest()
{
	InternalInit();
}

void TLS_CertificateStatusRequest::InternalInit()
{
	AddItem(&status_type);
	AddItem(&ocsp_request);
}

void TLS_CertificateStatusRequest::ConstructL(SSL_ConnectionState *pending)
{
	status_type = TLS_CST_OCSP;
	ocsp_request.ConstructL(pending);
}


TLS_CertificateStatusResponse::TLS_CertificateStatusResponse()
{
	InternalInit();
}

void TLS_CertificateStatusResponse::InternalInit()
{
	AddItem(&status_type);
	status_type.SetItemID(TLS_CSR_STATUS_TYPE);
	AddItem(&ocsp_response);

	AddItem(&unknown_data);
	unknown_data.SetEnableRecord(FALSE);
}

void TLS_CertificateStatusResponse::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KReadAction:
		if((int) arg2 == TLS_CSR_STATUS_TYPE)
		{
			BOOL is_ocsp = (status_type == TLS_CST_OCSP);

			ocsp_response.SetEnableRecord(is_ocsp);
			unknown_data.SetEnableRecord(!is_ocsp);
		}
		break;
	}
	LoadAndWritableList::PerformActionL(action, arg1, arg2);
}

SSL_KEA_ACTION TLS_CertificateStatusResponse::ProcessMessage(SSL_ConnectionState *pending)
{
	if(!pending)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return SSL_KEA_Handle_Errors;
	}

	if(status_type == TLS_CST_OCSP)
	{
		pending->session->received_ocsp_response.Set(ocsp_response);
		RaiseAlert(pending->session->received_ocsp_response);

		if(Error())
			return SSL_KEA_Handle_Errors;

		return pending->key_exchange->ReceivedCertificate();
	}
	RaiseAlert(SSL_Fatal, SSL_Bad_Certificate_Status_Response);
	return SSL_KEA_Handle_Errors;
}

#endif // CERTSTATUS

#endif // _NATIVE_SSL_SUPPORT_
