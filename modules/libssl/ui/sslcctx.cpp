/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/handshake/asn1certlist.h"
#include "modules/libssl/ui/sslcctx.h"
#include "modules/libssl/certs/certinst_base.h"
#include "modules/libssl/certs/certexp.h"
#include "modules/cache/url_stream.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dochand/winman.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#ifdef SECURITY_INFORMATION_PARSER
# include "modules/windowcommander/src/SecurityInfoParser.h"
#endif // SECURITY_INFORMATION_PARSER

#ifdef TRUST_INFO_RESOLVER
# include "modules/windowcommander/src/TrustInfoResolver.h"
#endif // TRUST_INFO_RESOLVER

OP_STATUS ParseCommonName(const OpString_list &info, OpString &title);

OP_STATUS ReadStringFromURL(URL &source, OpString &target)
{
	SSL_varvector32 data;

	target.Empty();

	URL_DataStream source_stream(source);

	TRAP_AND_RETURN(op_err, data.AddContentL(&source_stream));

	return target.Set((const uni_char *)data.GetDirect(), UNICODE_DOWNSIZE(data.GetLength()));
}


SSL_Certificate_DisplayContext::SSL_Certificate_DisplayContext(WORD action)
	: add_comments_to_certificate_view(TRUE), title_id(Str::NOT_A_STRING), message_id(Str::NOT_A_STRING),
	action(action)
{
	InternalInit(action);
}

void SSL_Certificate_DisplayContext::InternalInit(WORD action)
{
	CertChain = NULL;
	CertVector = NULL;
	Certselectlist = NULL;
    window = NULL;
	with_options = FALSE;
	ok_button_is_accept = FALSE;
	show_delete_install = FALSE;
	delete_install_is_install = FALSE;
	hide_ok		= FALSE;
	hide_cancel = FALSE;
	show_import = FALSE;
	show_export = FALSE;
	Warn =  FALSE;
	Deny = FALSE;
	use_protocoldata = FALSE;
	show_cert_store = SSL_CA_Store;
	cert_origin = SSL_CERT_RETRIEVE_FROM_OPTIONS;
	current_certificate = 0;
	certificate_count = 0;
	CertificateStatus = NULL;
	selected_action = SSL_CERT_CANCEL_OPERATION;
	complete_id = 0;
	optionsManager = NULL;
	servername = NULL;

    switch(action)
    {
      case  IDM_SITE_CERTIFICATES_BUTT :
        with_options = TRUE;
        title_id = Str::SI_SITE_CERT_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_EDIT_CA_CERT;
        show_delete_install = TRUE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_OPTIONS;
        show_cert_store = SSL_CA_Store ;
		show_import = TRUE;
		show_export = TRUE;
        break;
      case  IDM_PERSONAL_CERTIFICATES_BUTT :
        title_id = Str::SI_PERSONAL_CERT_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_EDIT_PERSONAL_CERT;
        show_delete_install = TRUE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_OPTIONS;
        show_cert_store = SSL_ClientStore;
		show_import = TRUE;
		show_export = TRUE;
        break;
      case  IDM_TRUSTED_CERTIFICATE_BUTT :
        title_id = Str::SI_PERSONAL_CERT_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_EDIT_PERSONAL_CERT;
        show_delete_install = TRUE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_OPTIONS;
        show_cert_store = SSL_TrustedSites;
		show_import = FALSE;
		show_export = TRUE;
        break;
      case  IDM_INTERMEDIATE_CERTIFICATE_BUTT :
        with_options = TRUE;
        title_id = Str::S_INTERMEDIATE_CERT_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_EDIT_CA_CERT;
        show_delete_install = TRUE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_OPTIONS;
        show_cert_store = SSL_IntermediateCAStore ;
		show_import = TRUE;
		show_export = TRUE;
        break;
      case  IDM_UNTRUSTED_CERTIFICATE_BUTT :
        title_id = Str::SI_PERSONAL_CERT_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_EDIT_PERSONAL_CERT;
        show_delete_install = TRUE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_OPTIONS;
        show_cert_store = SSL_UntrustedSites;
		show_import = TRUE;
		show_export = TRUE;
        break;
      case  IDM_INSTALL_CA_CERTIFICATE :
        with_options = TRUE;
        message_id = Str::SI_MSG_SECURE_INSTALL_CA_CERT;
        title_id = Str::SI_INSTALL_CA_CERTIFICATE_DLG_TITLE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
		hide_ok = TRUE;
		show_delete_install = TRUE;
		delete_install_is_install = TRUE;
		Warn = (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::WarnAboutImportedCACertificates)) ? TRUE : FALSE;
		Deny = FALSE;
		with_options = TRUE;
        break;
      case  IDM_INSTALL_PERSONAL_CERTIFICATE :
        message_id =  Str::SI_MSG_SECURE_INSTALL_USER_CERT;
        title_id = Str::SI_INSTALL_USER_CERTIFICATE_DLG_TITLE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
		hide_ok = TRUE;
		show_delete_install = TRUE;
		delete_install_is_install = TRUE;
		Warn = TRUE;
		Deny = TRUE;
		with_options = TRUE;
        break;
      case  IDM_SELECT_USER_CERTIFICATE :
        message_id = Str::SI_MSG_SECURE_SELECT_USER_CERT;
        title_id = Str::SI_SELECT_USER_CERTIFICATE_DLG_TITLE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERT_CHAIN;
        break;
      case  IDM_NO_CHAIN_CERTIFICATE :
        title_id = Str::SI_MSG_SECURE_NO_CHAIN_CERTIFICATE_DLG_TITLE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        message_id = Str::SI_MSG_SECURE_NO_CHAIN_CERTIFICATE_AXP;
        ok_button_is_accept = TRUE;
        break;
      case  IDM_WARN_CERTIFICATE :
        title_id = Str::SI_WARN_CERTIFICATE_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_WARN_CA_CERT;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
	  case  IDM_SSL_LOW_ENCRYPTION_LEVEL:
        title_id = Str::S_SSL_LOW_ENCRYPTION_TITLE;
        message_id = Str::S_SSL_LOW_ENCRYPTION_WARNING;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
	  case  IDM_SSL_DIS_CIPHER_LOW_ENCRYPTION_LEVEL:
        title_id = Str::S_SSL_LOW_ENCRYPTION_TITLE;
        message_id = Str::S_SSL_LOW_ENCRYPTION_DIS_CIPHER_WARNING;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
	  case  IDM_SSL_ANONYMOUS_CONNECTION:
        title_id = Str::S_SSL_LOW_ENCRYPTION_TITLE;
        message_id = Str::S_SSL_ANONYMOUS_WARNING;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
	  case  IDM_SSL_AUTHENTICATED_ONLY:
        title_id = Str::S_SSL_LOW_ENCRYPTION_TITLE;
        message_id = Str::S_MSG_HTTP_SSL_AUTHENTICATION_ONLY;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
      case  IDM_WRONG_CERTIFICATE_NAME :
        title_id = Str::SI_WRONG_CERTIFICATE_NAME_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_WARN_INVALID_NAME_CERT;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
      case IDM_EXPIRED_SERVER_CERTIFICATE:
        title_id = Str::SI_EXPIRED_SERVER_CERTIFICATE_DLG_TITLE;
        message_id = Str::SI_MSG_SECURE_EXPIRED_SERVER_CERTIFICATE_TEXT;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
        break;
	  case IDM_APPLET_CERTIFICATE_GRANTPERM:
		title_id = Str::SI_IDSTR_JAVA_ACCEPTCERT_DLG_TITLE;
		message_id = Str::SI_IDSTR_JAVA_ACCEPTCERT_TEXT;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
        ok_button_is_accept = TRUE;
		break;
	  case IDM_SSL_IMPORT_KEY_AND_CERTIFICATE:
		title_id = (Str::LocaleString)Str::SI_TITLE_SSL_IMPORT_KEY_AND_CERTIFICATE;
		message_id = (Str::LocaleString)Str::SI_MSG_SSL_IMPORT_KEY_AND_CERTIFICATE;
        cert_origin = SSL_CERT_RETRIEVE_FROM_CERTLIST;
		break;
    }
}

SSL_Certificate_DisplayContext::~SSL_Certificate_DisplayContext()
{
	InternalDestruct();
}

void SSL_Certificate_DisplayContext::InternalDestruct()
{
	OP_DELETEA(CertificateStatus);
	CertificateStatus = NULL;

	OP_DELETE(CertChain);
	CertChain = NULL;

	OP_DELETE(Certselectlist);
	Certselectlist = NULL;

	OP_DELETE(CertVector);
	CertVector = NULL;

	if(optionsManager && optionsManager->dec_reference()<=0)
		OP_DELETE(optionsManager);

	// destroy the list of certificates used by windowcommander API
	WicCertificate* cert = (WicCertificate*)m_certificates.First();
	while (cert)
	{
		WicCertificate* next = (WicCertificate*)cert->Suc();
		cert->Out();
		OP_DELETE(cert);

		cert = next;
	}

	Window* win;
	WindowCommander* wic = NULL;
	if (g_windowManager)
	{
		for(win = g_windowManager->FirstWindow(); win; win = win->Suc())
		{
			if (win->GetOpWindow() == GetWindow())
			{
				wic = win->GetWindowCommander();
				break;
			}
		}
	}

	OpSSLListener *ssl_listener = NULL;

	if (wic && (ssl_listener = wic->GetSSLListener()))
		ssl_listener->OnCertificateBrowsingCancel(wic, static_cast<OpSSLListener::SSLCertificateContext*>(this));
	else if (g_windowCommanderManager && (ssl_listener = g_windowCommanderManager->GetSSLListener())) // if no window is available, fallback on the windowcommanderManager SSL-listener
		ssl_listener->OnCertificateBrowsingCancel(NULL, static_cast<OpSSLListener::SSLCertificateContext*>(this));

}

BOOL SSL_Certificate_DisplayContext::CheckOptionsManager()
{
	if(!optionsManager)
	{
		optionsManager = g_ssl_api->CreateSecurityManager(TRUE);

        if (optionsManager == NULL)
            return FALSE;
	}

	if(optionsManager)
	{
		optionsManager->Init(SSL_LOAD_ALL_STORES);
	}

	return (optionsManager == NULL ? FALSE : TRUE);
}


void SSL_Certificate_DisplayContext::SetCertificateChain(SSL_CertificateHandler *chain, BOOL allow_root_install)
{
    CertChain = chain;
    if(allow_root_install && CertChain != NULL && CertChain->SelfSigned(CertChain->CertificateCount() -1) && CertVector != NULL)
    {
		if(CheckOptionsManager())
		{
			uint24 index = CertChain->CertificateCount() -1;
			SSL_DistinguishedName name;
			CertChain->GetIssuerName(index, name) ;

			SSL_CertificateItem *item = optionsManager->FindTrustedCA(name, NULL);

			while(item)
			{
				if(CertVector->operator [](index) == item->certificate)
				{
					allow_root_install = FALSE;
					break;
				}
				item = optionsManager->FindTrustedCA(name, item);
			}
		}

		if(allow_root_install)
		{
			with_options = TRUE;
			if(message_id == Str::SI_MSG_SECURE_NO_CHAIN_CERTIFICATE_AXP)
				message_id = Str::SI_MSG_SECURE_NO_CHAIN_CERTIFICATE_INSTALL;
			show_delete_install = TRUE;
			delete_install_is_install = TRUE;
			Warn = FALSE;
			Deny = FALSE;
		}
	}
}

BOOL SSL_Certificate_DisplayContext::GetCertificateShortName(OpString &name, unsigned int i)
{
	//if(i<0)
	//	return FALSE;

	BOOL valid = FALSE;
    OpString_list info;

	name.Empty();

	switch (cert_origin)
	{
	case SSL_CERT_RETRIEVE_FROM_OPTIONS :
		if(CheckOptionsManager() &&
			optionsManager->Get_Certificate_title(show_cert_store, i, name))
			valid = TRUE;
		break;
	case SSL_CERT_RETRIEVE_FROM_CERTLIST :
		valid = (CertChain && (uint16) i < CertChain->CertificateCount());
		if(valid)
		{
			if(OpStatus::IsError(CertChain->GetSubjectName(i,info)))
			{
				valid = FALSE;
				break;
			}
			if(OpStatus::IsError(ParseCommonName(info,name)))
			{
				valid = FALSE;
				break;
			}
		}
		break;
	case SSL_CERT_RETRIEVE_FROM_CERT_CHAIN :
		valid = (i < Certselectlist->Cardinal());
		if(valid)
		{
			SSL_CertificateHandler_List *item;

			item = Certselectlist->Item(i);
			if(item != NULL)
			{
#ifdef _SSL_USE_SMARTCARD_
				if(item->external_key_item)
				{
					SSL_CertificateHandler *cert = item->external_key_item->Certificate();

					if(cert == NULL || OpStatus::IsError(cert->GetSubjectName(0,info)))
					{
						valid = FALSE;
						break;
					}
				}
				else
#endif
				if(OpStatus::IsError(item->certitem->handler->GetSubjectName(0,info)))
				{
					valid = FALSE;
					break;
				}
				if(OpStatus::IsError(ParseCommonName(info,name)))
				{
					valid = FALSE;
					break;
				}
			}
		}
    default      : break;
	}
	if(!valid && !certificate_count && i)
	{
		certificate_count = i;
	    CertificateStatus = OP_NEWA(SSL_Certificate_Status, certificate_count);
		if(CertificateStatus == NULL)
			certificate_count = 0;

	}

	return valid;
}

BOOL SSL_Certificate_DisplayContext::GetIssuerShortName(OpString &name, unsigned int i)
{
	//if(i<0)
	//	return FALSE;

	BOOL valid = FALSE;
    OpString_list info;

	name.Empty();

	switch (cert_origin)
	{
	case SSL_CERT_RETRIEVE_FROM_OPTIONS :
		if(CheckOptionsManager() &&
			optionsManager->Get_CertificateIssuerName(show_cert_store, i, info) &&
			OpStatus::IsSuccess(ParseCommonName(info,name)))
			valid = TRUE;
		break;
	case SSL_CERT_RETRIEVE_FROM_CERTLIST :
		valid = ((uint16) i < CertChain->CertificateCount());
		if(valid)
		{
			if(OpStatus::IsError(CertChain->GetIssuerName(i,info)))
			{
				valid = FALSE;
				break;
			}
			if(OpStatus::IsError(ParseCommonName(info,name)))
			{
				valid = FALSE;
				break;
			}
		}
		break;
	case SSL_CERT_RETRIEVE_FROM_CERT_CHAIN :
		valid = (i < Certselectlist->Cardinal());
		if(valid)
		{
			SSL_CertificateHandler_List *item;

			item = Certselectlist->Item(i);
			if(item != NULL)
			{
#ifdef _SSL_USE_SMARTCARD_
				if(item->external_key_item)
				{
					SSL_CertificateHandler *cert = item->external_key_item->Certificate();

					if(cert == NULL )
					{
						valid = FALSE;
						break;
					}
					cert->GetIssuerName(0,info);
				}
				else
#endif
				if(OpStatus::IsError(item->certitem->handler->GetIssuerName(0,info)))
				{
					valid = FALSE;
					break;
				}
				if(OpStatus::IsError(ParseCommonName(info,name)))
				{
					valid = FALSE;
					break;
				}
			}
		}
    default      : break;
	}

	return valid;
}

time_t SSL_Certificate_DisplayContext::GetTrustedUntil(unsigned int i)
{
	if(cert_origin != SSL_CERT_RETRIEVE_FROM_OPTIONS || show_cert_store != SSL_TrustedSites || !CertificateStatus || !CheckOptionsManager())
		return 0;

	SSL_CertificateItem *item = optionsManager->Find_Certificate(show_cert_store, i);
	if(!item)
		return 0;

	return item->trusted_until;
}


void SSL_Certificate_DisplayContext::UpdatedCertificates()
{
	if(cert_origin == SSL_CERT_RETRIEVE_FROM_OPTIONS && CertificateStatus)
	{
		unsigned int n= 0;

		while(optionsManager->Find_Certificate(show_cert_store, n))
		{
			n++;
		}

		if(n>certificate_count)
		{
			SSL_Certificate_Status *new_cert_status = OP_NEWA(SSL_Certificate_Status, n);
			if(new_cert_status)
			{
				unsigned int j;
				for(j=0; j < certificate_count; j++)
				{
					new_cert_status[j] = CertificateStatus[j];
				}

				OP_DELETEA(CertificateStatus);
				CertificateStatus = new_cert_status;
				certificate_count = n;
			}
		}
	}
}

// Access only after GetCertificateShortName have returned FALSE
URL SSL_Certificate_DisplayContext::AccessCurrentCertificateInfo(BOOL &warn, BOOL &deny, OpString &text_field , Str::LocaleString description, SSL_Certinfo_mode info_type)
{
	info_url.UnsetURL();

	CurrentCertificateInfo = AccessCertificateInfo(current_certificate, warn, deny, text_field, description, info_type);

    if(CertificateStatus && !CurrentCertificateInfo.IsEmpty())
        CertificateStatus[current_certificate].visited = TRUE;

	info_url.SetURL(CurrentCertificateInfo);
	return CurrentCertificateInfo;
}

URL SSL_Certificate_DisplayContext::AccessCertificateInfo(unsigned int cert_number, BOOL &warn, BOOL &deny, OpString &text_field , Str::LocaleString description, SSL_Certinfo_mode info_type)
{
	URL info;

	if(cert_origin == SSL_CERT_RETRIEVE_FROM_OPTIONS && CertificateStatus)
	{
		unsigned int j,tid;

		for(tid=j=0;j<certificate_count;j++)
			if(!CertificateStatus[j].deleted)
			{
				if(tid == cert_number)
					break;
				tid++;
			}

		cert_number = j;
	}

	if(cert_number>= certificate_count)
	{
		return URL();
	}

	warn = FALSE;
	deny = FALSE;
	BOOL fail = FALSE;

	switch (cert_origin)
	{
	case SSL_CERT_RETRIEVE_FROM_OPTIONS :
		if(CertificateStatus == NULL)
		{
			fail = TRUE;
			break;
		}
		if(!optionsManager->Get_Certificate(show_cert_store, cert_number, info, warn,deny, description, info_type))
			fail = TRUE;
		if(CertificateStatus[cert_number].visited){
			warn = CertificateStatus[cert_number].warn;
			deny = CertificateStatus[cert_number].deny;
		}
		else
		{
			CertificateStatus[cert_number].warn = warn;
			CertificateStatus[cert_number].deny = deny;
		}
		break;
	case SSL_CERT_RETRIEVE_FROM_SINGLE_CERT:
	case SSL_CERT_RETRIEVE_FROM_CERTLIST :
		fail = (cert_number >= CertChain->CertificateCount());
		if(!fail)
		{
			if(OpStatus::IsError(CertChain->GetCertificateInfo(cert_number, info, description, info_type)))
				fail = TRUE;
			warn = Warn;
			deny = Deny;
		}
		break;
	case SSL_CERT_RETRIEVE_FROM_CERT_CHAIN :
		fail = (cert_number >= Certselectlist->Cardinal());
		if(!fail)
		{
			SSL_CertificateHandler_List *item;

			item = Certselectlist->Item(cert_number);
			if(item != NULL)
			{
#ifdef _SSL_USE_SMARTCARD_
				if(item->external_key_item)
				{
					SSL_CertificateHandler *cert = item->external_key_item->Certificate();

					if(cert == NULL || OpStatus::IsError(cert->GetCertificateInfo(0,info, description, info_type)))
					info_url.SetURL(CurrentCertificateInfo);
						fail = TRUE;

					OpStatus::Ignore(text_field.AppendFormat(OpString("SmartCard Label: %s\r\n"), item->external_key_item->Label().CStr()));
					OpStatus::Ignore(text_field.AppendFormat(OpString("SmartCard Serialnumber: %s\r\n"), item->external_key_item->SerialNumber().CStr()));
				}
				else
#endif
				{
					SSL_CertificateHandler *cert = item->certitem->GetCertificateHandler();

					if(cert != NULL)
						fail = OpStatus::IsError(cert->GetCertificateInfo(0, info, description, info_type));
					else
						fail = TRUE;
				}
			}
			else
				fail = TRUE;
		}
		break;
	}

	URL empty_url;
	return (!fail ? info : empty_url);
}

// Access only after GetCertificateShortName have returned FALSE
void SSL_Certificate_DisplayContext::GetCertificateTexts(
			OpString &subject, OpString &issuer,
			OpString &certificatedata,
			BOOL &allow_flag, BOOL &warn_flag,
			unsigned int cert_number
			)
{
	URL info;
	OpString dummy;
	BOOL warn = FALSE;
	BOOL deny = FALSE;

	subject.Empty();
	issuer.Empty();
	certificatedata.Empty();
	allow_flag = warn_flag = FALSE;

	info = AccessCertificateInfo(cert_number, warn, deny, dummy, Str::NOT_A_STRING, SSL_Cert_Text_Subject);
	if(CertificateStatus && !info.IsEmpty())
		CertificateStatus[cert_number].visited = TRUE;
	ReadStringFromURL(info, subject);
	info.Unload();
	info = URL();

	info = AccessCertificateInfo(cert_number, warn, deny, dummy, Str::NOT_A_STRING, SSL_Cert_Text_Issuer);
	if(CertificateStatus && !info.IsEmpty())
		CertificateStatus[cert_number].visited = TRUE;
	ReadStringFromURL(info, issuer);
	info.Unload();
	info = URL();

	info = AccessCertificateInfo(cert_number, warn, deny, dummy, Str::NOT_A_STRING, SSL_Cert_Text_General);
	if(CertificateStatus && !info.IsEmpty())
		CertificateStatus[cert_number].visited = TRUE;

	OpString temp_info;
	ReadStringFromURL(info, temp_info);
	info.Unload();
	info = URL();

	if(CipherName.Length())
	{
		OpString Path;
		if(!url.IsEmpty())
		{
			OpStatus::Ignore(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, Path));
		}
		if(Path.Length())
		{
			OpStatus::Ignore(certificatedata.Append(Path)); // Ignore, only loses displayed text
			OpStatus::Ignore(certificatedata.Append(UNI_L("\r\n"))); // Ignore, only loses displayed text
		}

		uint8 jor, nor;
		CipherVersion.Get(jor, nor);
		uint8 OP_MEMORY_VAR major = jor, minor = nor; // may be trampled by TRAP

		// Added TLS Support 24/01/98 YNP
		OpString connection_text;
		SetLangString(Str::SI_MSG_SSL_CONNECTION_TEXT, connection_text);
		//FIXME: RETURN_IF_ERROR(err);

		const uni_char *protocol = UNI_L("SSL");

		if(major > 3 || (major == 3  && minor > 0) )
		{
			protocol = UNI_L("TLS");
			major -= 2;
			if(major == 1)
				minor--;
		}

		OpStatus::Ignore(certificatedata.AppendFormat(UNI_L("%s : %s v%d.%d   "),
			connection_text.CStr(),
			protocol,(int) major, (int) minor)); // Ignore, only loses displayed text
		OpStatus::Ignore(certificatedata.Append(CipherName)); // Ignore, only loses displayed text
		OpStatus::Ignore(certificatedata.Append(UNI_L("\r\n\r\n"))); // Ignore, only loses displayed text
	}
	if(add_comments_to_certificate_view)
	{
		SSL_Certificate_Comment *comment = certificate_comments.First();
		while(comment)
		{
			certificatedata.Append(comment->GetComment());
			certificatedata.Append(UNI_L("\r\n\r\n"));

			comment = comment->Suc();
		}
	}

	certificatedata.Append(temp_info);

	if(with_options)
	{
		allow_flag = !deny;
		warn_flag = warn;
	}
}

void SSL_Certificate_DisplayContext::GetCertificateExpirationDate(OpString &date_string, unsigned int cert_number, BOOL to_date)
{
	BOOL warn, deny;
	URL info;
	OpString dummy;

	date_string.Empty();

	info = AccessCertificateInfo(cert_number, warn, deny, dummy, Str::NOT_A_STRING, (to_date ? SSL_Cert_Text_ValidTo : SSL_Cert_Text_ValidFrom));

	ReadStringFromURL(info, date_string);
}


void SSL_Certificate_DisplayContext::UpdateCurrentCertificate(BOOL warn, BOOL allow)
{
	//if(current_certificate< 0)
	//	return;

	unsigned int cert_number = current_certificate;

	if(cert_origin == SSL_CERT_RETRIEVE_FROM_OPTIONS && CertificateStatus)
	{
		unsigned int j,tid;

		for(tid=j=0;j<certificate_count;j++)
			if(!CertificateStatus[j].deleted)
			{
				if(tid == cert_number)
					break;
				tid++;
			}

		cert_number = j;
	}

	if(cert_number>= certificate_count)
		return;

    if(CertificateStatus)
    {
		CertificateStatus[cert_number].visited = TRUE;
        CertificateStatus[cert_number].deny = Deny =  !allow;
        CertificateStatus[cert_number].warn = Warn =  warn;
    }
}

ssl_cert_display_action SSL_Certificate_DisplayContext::PerformAction(ssl_cert_button_action action)
{
	switch(action)
	{
	case SSL_CERT_BUTTON_OK:
		selected_action = SSL_CERT_ACCEPT;
		if(cert_origin == SSL_CERT_RETRIEVE_FROM_OPTIONS)
			SaveCertSettings();
		else
			current_certificate++;

		return SSL_CERT_CLOSE_WINDOW;
	case SSL_CERT_BUTTON_CANCEL:
		selected_action = SSL_CERT_CANCEL_OPERATION;
		return SSL_CERT_CLOSE_WINDOW;
	case SSL_CERT_BUTTON_INSTALL_DELETE:
		return DeleteInstallCertificate();
	case SSL_CERT_ACCEPT_REMEMBER:
		selected_action = SSL_CERT_ACCEPT;
		if(remember_accept_refuse && CertVector && CheckOptionsManager())
		{
			if(optionsManager->AddTrustedServer((*CertVector)[0], servername, serverport, is_expired,
				(title_id != Str::SI_IDSTR_JAVA_ACCEPTCERT_DLG_TITLE ? CertAccept_Site : CertAccept_Applet)))
				g_ssl_api->CommitOptionsManager(optionsManager);
		}
		return SSL_CERT_CLOSE_WINDOW;

	case SSL_CERT_REFUSE_REMEMBER:
		selected_action = SSL_CERT_CANCEL_OPERATION;
		if(remember_accept_refuse && CertVector && CheckOptionsManager())
		{
			if(optionsManager->AddUnTrustedCert((*CertVector)[0]))
				g_ssl_api->CommitOptionsManager(optionsManager);
		}
		return SSL_CERT_CLOSE_WINDOW;
	}
	return SSL_CERT_DO_NOTHING;
}


void SSL_Certificate_DisplayContext::SaveCertSettings()
{
#ifndef _NO_SECURITY_PREFS_
	unsigned int i,current;

    if(CertificateStatus)
    {
        for (current =i=0;i<certificate_count;i++)
            if (CertificateStatus[i].visited)
            {
                if (CertificateStatus[i].deleted)
                    optionsManager->Remove_Certificate(show_cert_store, current);
                else
                {
                    optionsManager->Set_Certificate(show_cert_store, current,
														CertificateStatus[i].warn,
														CertificateStatus[i].deny);
                    current++;
                }
            }
            else
                current++;

		OP_DELETEA(CertificateStatus);
		CertificateStatus = NULL;
	}
#endif
}

ssl_cert_display_action  SSL_Certificate_DisplayContext::DeleteInstallCertificate()
{
#ifndef _NO_SECURITY_PREFS_
  if(delete_install_is_install)
  {
        SSL_ASN1Cert_list list;
		//BOOL set_manager = FALSE;
        selected_action = (Deny ? SSL_CERT_CANCEL_OPERATION : SSL_CERT_ACCEPT) ;

#if 1
		if(!CertVector)
			return SSL_CERT_CLOSE_WINDOW;

		if(!CheckOptionsManager())
			return SSL_CERT_CLOSE_WINDOW;
#else
		if(!CertVector)
			return SSL_CERT_CLOSE_WINDOW;

		if(updated_securityManager == NULL)
		{
            updated_securityManager = OP_NEW(SSL_Options, ());
            if(updated_securityManager == NULL)
                return SSL_CERT_CLOSE_WINDOW;
		}
#endif // Fix merge conflict

#ifdef USE_SSL_CERTINSTALLER
		SSL_Certificate_Installer_flags flags(show_cert_store, Warn, Deny);
		SSL_Certificate_Installer_Base * OP_MEMORY_VAR installer = NULL;
		TRAPD(op_err, installer = g_ssl_api->CreateCertificateInstallerL(CertVector->operator[](CertVector->Count()-1), flags, NULL, NULL));
		if(OpStatus::IsSuccess(op_err) && OpStatus::IsSuccess(installer->StartInstallation()) && installer->Finished() && installer->InstallSuccess())
			g_ssl_api->CommitOptionsManager(optionsManager);
		OP_DELETE(installer);
#else
		OP_ASSERT(0); // No longer supported. Enable new certificate installation API.
#endif

		return SSL_CERT_CLOSE_WINDOW;
  }
  else if(cert_origin == SSL_CERT_RETRIEVE_FROM_SINGLE_CERT)
  {
	  SSL_DistinguishedName name;
	  SSL_CertificateItem *item;

	  if(!CheckOptionsManager())
		  return SSL_CERT_CLOSE_WINDOW;

	  CertChain->GetSubjectName(current_certificate,name);
	  item =  optionsManager->FindTrustedCA(name);
	  if(item != NULL){
		  item->cert_status = Cert_Deleted;
		  g_ssl_api->CommitOptionsManager(optionsManager);
	  }

	  return SSL_CERT_CLOSE_WINDOW;
  }
  else
  {
	  unsigned int cert_number = current_certificate;

	  unsigned int j = 0;
	  unsigned int tid = 0;

      if(CertificateStatus != NULL)
      {

          for(tid=j=0;j<certificate_count;j++)
              if(!CertificateStatus[j].deleted)
              {
                  if(tid == cert_number)
                      break;
                  tid++;
              }
      }
	  cert_number = j;

	  if(cert_number>= certificate_count)
		  return SSL_CERT_DO_NOTHING;

      if(CertificateStatus)
	  {
		  CertificateStatus[cert_number].visited = TRUE;
          CertificateStatus[cert_number].deleted = TRUE;
	  }

	  if(cert_number== certificate_count-1 && current_certificate)
		  current_certificate --;
	  else
	  {
          if(CertificateStatus)
              while(cert_number< certificate_count)
              {
                  if(!CertificateStatus[cert_number].deleted)
                      break;
                  cert_number ++;
              }

		  if(cert_number >= certificate_count)
			  current_certificate --;
	  }

	  return SSL_CERT_DELETE_CURRENT_ITEM;
  }
#endif
}

void SSL_Certificate_DisplayContext::SetCertificateList(SSL_ASN1Cert_list &vector)
{
	if(CertVector)
		OP_DELETE(CertVector);

	CertVector = OP_NEW(SSL_ASN1Cert_list, ());
	if(CertVector)
		CertVector->operator =(vector);
}

#ifdef USE_SSL_CERTINSTALLER
BOOL SSL_Certificate_DisplayContext::ImportCertificate(const OpStringC &filename,
														SSL_dialog_config &dialog_config,
														SSL_Certificate_Installer_Base *&installer
													   )
{
	OpString fileurl;
	OP_MEMORY_VAR BOOL resolved = FALSE;
	OP_STATUS op_err;

	TRAP(op_err,resolved = g_url_api->ResolveUrlNameL(filename, fileurl));
	if (!resolved || OpStatus::IsError(op_err))
		return FALSE;

	URL ref;
	URL source = g_url_api->GetURL(ref, fileurl, TRUE);

	if(source.IsEmpty())
		return FALSE;

	if(source.QuickLoad(FALSE))
	{
		SSL_Certificate_Installer_flags flags(show_cert_store, Warn, Deny);

		installer = NULL;
		TRAPD(op_err, installer = g_ssl_api->CreateCertificateInstallerL(source, flags, &dialog_config, optionsManager));
		if(OpStatus::IsSuccess(op_err) && OpStatus::IsSuccess(installer->StartInstallation()) && installer->Finished() && installer->InstallSuccess())
			g_ssl_api->CommitOptionsManager(optionsManager);
	}
	source.Unload();

	UpdatedCertificates();

	return TRUE;
}
#endif

#ifdef USE_SSL_CERTINSTALLER
OP_STATUS SSL_Certificate_DisplayContext::ExportCertificate(const OpStringC &filename, const OpStringC &extension,
													   SSL_dialog_config *dialog_config, SSL_Certificate_Export_Base *&exporter)
{
	exporter = NULL;

	if(!CheckOptionsManager())
		return OpStatus::ERR_NO_MEMORY;

	BOOL client = (show_cert_store == SSL_ClientStore) ? TRUE: FALSE;
	BOOL use_pkcs7 = FALSE;
	BOOL all_certs = FALSE;
	BOOL use_pem = FALSE;

#ifdef LIBOPEAY_PKCS12_SUPPORT
	BOOL use_pkcs12 = FALSE;
	if(client && extension.CompareI(UNI_L("p12")) == 0)
		all_certs = use_pkcs12 = TRUE;
	else
#endif
	if(extension.CompareI(UNI_L("p7s")) == 0)
		all_certs = TRUE;
	else if(extension.CompareI(UNI_L("p7")) == 0)
		use_pkcs7 = all_certs = TRUE;
	else if(extension.CompareI(UNI_L("pem")) == 0)
		use_pem = TRUE;
	else if(client && extension.CompareI(UNI_L("usr")) == 0)
	{ /* do not need to do any thing */ }
	else if(!client && extension.CompareI(UNI_L("crt")) == 0)
	{ /* do not need to do any thing */ }
	else
	{
		OP_ASSERT(0);
	}

#ifdef LIBOPEAY_PKCS12_SUPPORT
	if(use_pkcs12)
	{
		SSL_PKCS12_Certificate_Export *exporter_temp = OP_NEW(SSL_PKCS12_Certificate_Export, ());

		if(exporter_temp == NULL)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS op_err = exporter_temp->Construct(optionsManager, GetCurrentCertificateNumber(), filename, *dialog_config);
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(exporter_temp);
			exporter_temp = NULL;
		}

		exporter = exporter_temp;

		return op_err;
	}
#endif

	return optionsManager->Export_Certificate(filename, show_cert_store, GetCurrentCertificateNumber(), all_certs, use_pem, use_pkcs7);
}
#endif

void SSL_Certificate_DisplayContext::AddCertificateComment(Str::LocaleString string_id, const OpStringC &arg1, const OpStringC &arg2)
{
	SSL_Certificate_Comment *item;

	item = OP_NEW(SSL_Certificate_Comment, ());
	if(!item)
		return;

	if(OpStatus::IsError(item->Init(string_id, arg1, arg2)))
	{
		OP_DELETE(item);
		return;
	}

	item->Into(&certificate_comments);
}

const OpStringC *SSL_Certificate_DisplayContext::AccessCertificateComment(unsigned int i) const
{
	SSL_Certificate_Comment* item = certificate_comments.First();
	while(i>0 && item)
	{
		item = item->Suc();
		i--;
	}

	return (item ? &item->GetComment() : NULL);
}

WicCertificate::WicCertificate(SSL_Certificate_DisplayContext* context, int number)
{
	OP_ASSERT(context);

	m_num = number;
	m_context = context;

	m_context->SetCurrentCertificateNumber(m_num);

	UpdateInfo(m_num);
}

void WicCertificate::UpdateInfo(unsigned int cert_number)
{
	m_context->GetCertificateShortName(m_short_name, cert_number);
	m_context->GetCertificateTexts(m_full_name, m_issuer, m_info, m_allow, m_warn, cert_number);
	m_context->GetCertificateExpirationDate(m_valid_from, cert_number, false);
	m_context->GetCertificateExpirationDate(m_valid_to, cert_number, true);
}

int WicCertificate::GetSettings() const
{
	int settings = OpSSLListener::SSL_CERT_SETTING_NONE;

	if (m_allow)
		settings |= OpSSLListener::SSL_CERT_SETTING_ALLOW_CONNECTIONS;

	if (m_warn)
		settings |= OpSSLListener::SSL_CERT_SETTING_WARN_WHEN_USE;

	return settings;
}

void WicCertificate::UpdateSettings(int settings)
{
	m_allow = (settings & OpSSLListener::SSL_CERT_SETTING_ALLOW_CONNECTIONS) != 0;
	m_warn = (settings & OpSSLListener::SSL_CERT_SETTING_WARN_WHEN_USE) != 0;

	m_context->SetCurrentCertificateNumber(m_num);
	m_context->UpdateCurrentCertificate(m_warn, m_allow);

	UpdateInfo(m_num);
}

void SSL_Certificate_DisplayContext::InitCertificates()
{
	if (m_certificates.Cardinal() > 0)
		return; // no need to init more than one time

	OpString dummy;
	int num_cert = 0;

	while (GetCertificateShortName(dummy, num_cert))
	{
		WicCertificate* new_cert = OP_NEW(WicCertificate, (this, num_cert++));
		if (!new_cert)
			return; // handle OOM!

		new_cert->Into(&m_certificates);
	}

	num_cert = 0;
	WicCertificate* cert = (WicCertificate*)m_certificates.First();

	while (cert)
	{
		cert->UpdateInfo(num_cert);
		cert = (WicCertificate*)cert->Suc();
		num_cert++;
	}
}

// from OpSSLListener::SSLCertificateContext
unsigned int SSL_Certificate_DisplayContext::GetNumberOfCertificates() const
{
	if (m_certificates.Cardinal() == 0)
	{
		// Must be locked correctly in multithreaded Core.
		SSL_Certificate_DisplayContext* non_const_this =
			const_cast <SSL_Certificate_DisplayContext*> (this);
		non_const_this->InitCertificates();
	}

	return m_certificates.Cardinal();
}

OpSSLListener::SSLCertificate* SSL_Certificate_DisplayContext::GetCertificate(unsigned int certificate)
{
	WicCertificate* cert = (WicCertificate*)m_certificates.First();

	while (cert)
	{
		if (cert->GetCertificateNumber() == certificate)
			return static_cast<OpSSLListener::SSLCertificate*>(cert);

		cert = (WicCertificate*)cert->Suc();
	}

	return NULL;
}

unsigned int SSL_Certificate_DisplayContext::GetNumberOfComments() const
{
	return certificate_comments.Cardinal();
}

const uni_char* SSL_Certificate_DisplayContext::GetCertificateComment(unsigned int i) const
{
	const OpStringC* comment = AccessCertificateComment(i);

	if (comment)
		return comment->CStr();

	return NULL;
}

OP_STATUS SSL_Certificate_DisplayContext::GetServerName(OpString* server_name) const
{
    return server_name->Set(servername ? servername->UniName() : NULL);
}

#ifdef SECURITY_INFORMATION_PARSER
OP_STATUS SSL_Certificate_DisplayContext::CreateSecurityInformationParser(OpSecurityInformationParser ** osip)
{
		SecurityInformationParser * parser = OP_NEW(SecurityInformationParser, ());
		if (!parser) return OpStatus::ERR_NO_MEMORY;

		OP_STATUS sts = parser->SetSecInfo(CurrentCertificateInfo);
		if (OpStatus::IsSuccess(sts))
			sts = parser->Parse();
		if (OpStatus::IsSuccess(sts))
			*osip = parser;
		if (OpStatus::IsError(sts))
			OP_DELETE(parser);
		return sts;
}
#endif // SECURITY_INFORMATION_PARSER

void SSL_Certificate_DisplayContext::OnCertificateBrowsingDone(BOOL ok, OpSSLListener::SSLCertificateOption options)
{
	/*
	if (!ok)
	{
		// cancel the interaction
		PerformAction(SSL_CERT_BUTTON_CANCEL);
		g_main_message_handler->PostMessage(GetCompleteMessage(), GetCompleteId(), IDCANCEL);
		return;
	}
	*/

	BOOL remember = (options & OpSSLListener::SSL_CERT_OPTION_REMEMBER) != 0;

	if (options & OpSSLListener::SSL_CERT_OPTION_OK)
		PerformAction(SSL_CERT_BUTTON_OK);

	if (options & OpSSLListener::SSL_CERT_OPTION_ACCEPT)
	{
		if (remember)
			PerformAction(SSL_CERT_ACCEPT_REMEMBER);
		else
			PerformAction(SSL_CERT_BUTTON_OK);
	}
	else if (options & OpSSLListener::SSL_CERT_OPTION_REFUSE)
	{
		if (remember)
			PerformAction(SSL_CERT_REFUSE_REMEMBER);
		else
			PerformAction(SSL_CERT_BUTTON_CANCEL);
	}
	else if (options & OpSSLListener::SSL_CERT_OPTION_INSTALL ||
			 options & OpSSLListener::SSL_CERT_OPTION_DELETE)
	{
		PerformAction(SSL_CERT_BUTTON_INSTALL_DELETE);
	}
	else if (!ok)
	{
		SSL_Alert msg(SSL_Internal, SSL_InternalError);
		SetAlertMessage(msg);
		PerformAction(SSL_CERT_BUTTON_CANCEL);
	}

	g_main_message_handler->PostMessage(GetCompleteMessage(), GetCompleteId(), IDOK);
}

OP_STATUS SSL_Certificate_Comment::Init(Str::LocaleString string_id, const OpStringC &arg1, const OpStringC &arg2)
{
	OpString string;

	uni_char *comment_temp = (uni_char *) g_memory_manager->GetTempBufUni();
	unsigned int comment_temp_len = g_memory_manager->GetTempBufUniLenUniChar();

	RETURN_IF_ERROR(SetLangString(string_id, string));
	if (string.IsEmpty())
		return OpStatus::OK;

	const uni_char *arg1_temp = (arg1.HasContent() ? arg1.CStr() : UNI_L(""));
	const uni_char *arg2_temp = (arg2.HasContent() ? arg2.CStr() : UNI_L(""));

	uni_snprintf_ss(comment_temp, comment_temp_len, string.CStr(), arg1_temp, arg2_temp);

	return comment.Set(comment_temp);
}

#endif // _NATIVE_SSL_SUPPORT_
