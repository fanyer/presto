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

#ifndef _TLS_EXTENSIONS_H_
#define _TLS_EXTENSIONS_H_

#if defined _NATIVE_SSL_SUPPORT_

enum TLS_Extension_ID {
	TLS_Ext_ServerName			= 0,
	TLS_Ext_MaxFragmentLength	= 1,
	TLS_Ext_ClientCertificateUrl= 2,
	TLS_Ext_TrustedCAKeys		= 3,
	TLS_Ext_TruncatedHMAC		= 4, 
	TLS_Ext_StatusRequest		= 5, 
	TLS_Ext_SignatureList		= 13,
	TLS_Ext_RenegotiationInfo	= 0xff01,
	TLS_Ext_NextProtocol		= 13172,
	TLS_Ext_MaxValue			= 65535
};

enum TLS_ServerNameType {
	TLS_SN_HostName = 0,
	TLS_SN_MaxValue	= 255
};

class TLS_ExtensionItem : public LoadAndWritableList
{
public:
	DataStream_IntegralType<TLS_Extension_ID,2>	extension_id;
	SSL_varvector16		extension_data;

private:
	void InternalInit();

public:
	TLS_ExtensionItem();
    TLS_ExtensionItem &operator =(const TLS_ExtensionItem &other){extension_id = other.extension_id;extension_data = other.extension_data; return *this;}
	TLS_ExtensionItem(const TLS_ExtensionItem &other):LoadAndWritableList(){InternalInit(); operator =(other); };
};

typedef SSL_LoadAndWriteableListType<TLS_ExtensionItem, 2, 0xffff>  TLS_ExtensionList;

class TLS_ServerNameItem : public LoadAndWritableList
{
public:
	DataStream_IntegralType<TLS_ServerNameType,1> name_type;
	SSL_varvector16		host_name;

private:
	void InternalInit();

public:
	TLS_ServerNameItem();
    TLS_ServerNameItem &operator =(const TLS_ServerNameItem &other){name_type = other.name_type;host_name = other.host_name; return *this;}
	TLS_ServerNameItem(const TLS_ServerNameItem &other):LoadAndWritableList(){InternalInit();operator =(other); };
	
	void SetHostName(const OpStringC8 &name);
};

typedef SSL_LoadAndWriteableListType<TLS_ServerNameItem, 2, 0xffff>  TLS_ServerNameList;

#ifndef TLS_NO_CERTSTATUS_EXTENSION
typedef SSL_varvector16 TLS_OSCP_ResponderID;
typedef SSL_LoadAndWriteableListType<TLS_OSCP_ResponderID, 2, 0xffff> TLS_OSCP_ResponderID_List;
typedef SSL_varvector16 TLS_OCSP_Extensions;

class TLS_OCSP_Request : public LoadAndWritableList
{
public:
	TLS_OSCP_ResponderID_List	responder_id_list;
	TLS_OCSP_Extensions		request_extensions;

private:
	void InternalInit();

public:
	TLS_OCSP_Request();

	void ConstructL(SSL_ConnectionState *pending);
};

class TLS_CertificateStatusRequest : public LoadAndWritableList
{
public:
	DataStream_IntegralType<TLS_CertificateStatusType,1> status_type;
	TLS_OCSP_Request	ocsp_request;

private:
	void InternalInit();

public:
	TLS_CertificateStatusRequest();

	void ConstructL(SSL_ConnectionState *pending);
};
#endif

class TLS_RenegotiationInfo : public LoadAndWritableList
{
public:
	SSL_varvector8 renegotiated_connection;

public:
	TLS_RenegotiationInfo(){AddItem(&renegotiated_connection);}
};

class TLS_NextProtocols : public LoadAndWritableList
{
public:
	SSL_NextProtocols protocols;

public:
	TLS_NextProtocols()
	{
		DataRecord_Spec spec = *protocols.GetRecordSpec();
		spec.enable_length = FALSE;
		protocols.SetRecordSpec(spec);
		AddItem(&protocols);
	}
};
#endif
#endif // _TLS_EXTENSIONS_H_
