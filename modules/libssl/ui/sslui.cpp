/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"

#include "modules/url/url_sn.h"
#include "modules/libssl/protocol/op_ssl.h"

#include "modules/libssl/ui/sslcctx.h"

#include "modules/hardcore/mh/mh.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dochand/winman.h"
#include "modules/network/src/op_request_impl.h"
#include "modules/url/url_ds.h"

// FIXME: Not all strings listed in strings database
static const struct SSL_Select_ErrorMessages_st{
	SSL_AlertDescription desc;
	Str::StringList1 message;
	int always_fatal;
} SSL_Select_ErrorMessages[] = {
	{SSL_Close_Notify              , (Str::StringList1) Str::NOT_A_STRING                  , 0 },
	{SSL_No_Certificate            , (Str::StringList1) Str::NOT_A_STRING                  , 0 },
	{SSL_No_Renegotiation          , (Str::StringList1) Str::NOT_A_STRING                  , 0 },
	{SSL_User_Canceled             , (Str::StringList1) Str::NOT_A_STRING                  , 0 },

	{SSL_Unexpected_Message        , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Bad_Record_MAC            , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Decryption_Failed         , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Record_Overflow           , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Decompression_Failure     , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Handshake_Failure         , (Str::StringList1) Str::SI_MSG_HTTP_Connection_Failure        , 1 },
	{SSL_Bad_Certificate           , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Bad_Certificate       , 1 },
	{SSL_Unsupported_Certificate   , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Unsupported_Certificate, 1},
	{SSL_Certificate_Revoked       , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Certificate_Revoked   , 1 },
	{SSL_Fraudulent_Certificate    , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Fraudulent_Certificate  , 1 },
	{SSL_Certificate_Expired       , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Certificate_Expired   , 1 },// when received from server
	{SSL_Certificate_Unknown       , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Certificate_Unknown   , 1 },
	{SSL_Illegal_Parameter         , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Unknown_CA                , (Str::StringList1) Str::S_MSG_HTTP_SSL_UNKNOWN_CA				, 1 },
	{SSL_Access_Denied             , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Access_Denied         , 1 },
	{SSL_Decode_Error              , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Decrypt_Error             , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Export_Restriction        , (Str::StringList1) Str::S_MSG_HTTP_SSL_EXPORT_RESTRICTION		, 1 },
	{SSL_Protocol_Version_Alert    , (Str::StringList1) Str::S_MSG_HTTP_SSL_PROTOCOL_VERSION_ALERT	, 1 },
	{SSL_Insufficient_Security     , (Str::StringList1) Str::S_MSG_HTTP_SSL_INSUFFICIENT_SECURITY	 , 1 },
	{SSL_InternalError             , (Str::StringList1) Str::SI_MSG_HTTP_SSL_InternalError         , 1 },
	{SSL_Restart_Handshake		   , (Str::StringList1) Str::S_MSG_SSL_NonFatalError_Retry		, 1}, // "Fatal" because the connection must be shut down
	{SSL_Renego_PatchUnPatch	   , (Str::StringList1) Str::S_MSG_SSL_Renego_PatchUnPatch		, 1}, 
	{SSL_Certificate_Verify_Error  , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Certificate_Verify_Error, 1},
	{SSL_Certificate_OCSP_error	   , (Str::StringList1) Str::S_MSG_SSL_OCSP_CERTIFICATE_VERIFY_ERROR, 1},
	{SSL_Certificate_OCSP_Verify_failed, (Str::StringList1) Str::S_MSG_SSL_OCSP_CERTIFICATE_VERIFY_ERROR, 1},
	{SSL_Certificate_Purpose_Invalid,(Str::StringList1) Str::SI_MSG_HTTP_SSL_Certificate_Purpose_Error, 1},
	{SSL_Certificate_CRL_Verify_failed  , (Str::StringList1) Str::SI_MSG_HTTP_SSL_CRL_Verify_Error, 1},
	{SSL_Not_TLS_Server            , (Str::StringList1) Str::S_MSG_NOT_A_TLS_Server     , 1 },
	{SSL_Certificate_Not_Yet_Valid , (Str::StringList1) Str::SI_MSG_HTTP_SSL_Certificate_Expired   , 1 }, // when received from server
	{SSL_Allocation_Failure        , (Str::StringList1) Str::SI_MSG_HTTP_SSL_InternalError         , 1 },
	{SSL_No_Cipher_Selected        , (Str::StringList1) Str::SI_MSG_HTTP_SSL_No_Cipher_Selected    , 1 },
	{SSL_Components_Lacking        , (Str::StringList1) Str::SI_MSG_SECURITY_UNAVAILABLE           , 1 },
	{SSL_Security_Disabled         , (Str::StringList1) Str::SI_MSG_SECURITY_DISABLED              , 1 },
	{SSL_NoRenegExtSupport         , (Str::StringList1) Str::S_MSG_RENEG_EXT_UNSUPPORTED          , 1 },
	{SSL_Insufficient_Security1    , (Str::StringList1) Str::S_MSG_HTTP_SSL_INSUFFICIENT_SECURITY	, 1 },
	//{SSL_Insufficient_Security2    , Str::NOT_A_STRING/*MSG_HTTP_SSL_Insufficient_Security2*/, 0 },
	{SSL_Authentication_Only_Warning,(Str::StringList1) Str::S_MSG_HTTP_SSL_AUTHENTICATION_ONLY/*MSG_HTTP_SSL_Authentication_only*/   , 0 },

	//{SSL_UnSupported_Extension     , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	//{SSL_Certificate_Unobtainable  , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Unrecognized_Name         , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error        , 1 },
	{SSL_Bad_Certificate_Status_Response  , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error , 1 },
	//{SSL_Bad_Certificate_Hash_Value       , (Str::StringList1) Str::SI_MSG_HTTP_Transmission_Error , 1 },

	// Place all entries above this line
	{SSL_No_Description            , (Str::StringList1) Str::SI_MSG_HTTP_SSL_No_Description        , 1 }
};

#define TITLE_TEXT_LEN 255

BOOL SSL::ApplicationHandleWarning(const SSL_Alert &msg, BOOL recv, BOOL &cont)
{
	cont = TRUE;
	if(msg.GetLevel() == SSL_NoError)
		return TRUE;
	if(msg.GetLevel() == SSL_Warning)
	{
		// Only these are allowed to continue, if they are non-fatal
		switch(msg.GetDescription())
		{
		case SSL_Close_Notify:
		case SSL_No_Certificate:
		case SSL_No_Renegotiation:
		case SSL_User_Canceled:
		case SSL_Unrecognized_Name:
		case SSL_Certificate_OCSP_Verify_failed:
		case SSL_Certificate_CRL_Verify_failed:
			return TRUE;
		}
	}
	// Treat all current alerts as fatal
	cont = FALSE;
	ApplicationHandleFatal(msg,recv);
	return FALSE;
}

void SSL::ApplicationHandleFatal(const SSL_Alert &msg, BOOL recv)
{
	if(msg.GetLevel() == SSL_NoError)
		return;

	const SSL_Select_ErrorMessages_st *current;
	SSL_AlertDescription desc;

	current = SSL_Select_ErrorMessages;
	desc = msg.GetDescription();
	while (current->desc != SSL_No_Description)
	{
		if(current->desc == desc)
			break;
		current++;
	}

	Str::LocaleString txtitem = current->message;
	if(txtitem != Str::NOT_A_STRING)
	{
		OpString titletemplate;
		OpString title;
		/* OP_STATUS err = */ SetLangString(recv ? Str::SI_MSG_HTTP_SSL_Fatal_Received : Str::SI_MSG_HTTP_SSL_Fatal, titletemplate);
		//FIXME: RETURN_IF_ERROR(err);
		if(titletemplate.HasContent())
			title.AppendFormat(titletemplate.CStr(), (unsigned int) desc);

		OpString appinfo,error_message;
		OpStatus::Ignore(pending_connstate->ActiveURL.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, appinfo));

		SetLangString(txtitem, error_message);

		OpString totalstring;

		totalstring.SetConcat(title, UNI_L("\r\n\r\n"), appinfo);
		totalstring.Append(UNI_L("\r\n\r\n"));
		totalstring.Append(error_message);
		if(msg.GetReason().HasContent())
		{
			totalstring.Append("\r\n\r\n");
			totalstring.Append(msg.GetReason());
		}
#ifdef LIBSSL_HANDSHAKE_HEXDUMP
	if(g_selftest.running)
	{
		if(!pending_connstate->selftest_sequence.Empty())
		{
			SSL_secure_varvector32 *data = pending_connstate->selftest_sequence.First();
			while (data)
            {
				unsigned long pos1;
				unsigned long pos2;
				unsigned long len = data->GetLength();
				const unsigned char *source = data->GetDirect();

				const uni_char *type;
				switch(data->GetTag())
				{
				case 1: type = UNI_L("Received");break;
				case 2: type = UNI_L("Sent");break;
				case 3: type = UNI_L("Calculated keyblock");break;
				case 4: type = UNI_L("Pre master secret");break;
				case 5: type = UNI_L("master secret");break;
				default: type = UNI_L("Unknown");break;
				}

                // Pre-allocate (probably most of) enough for this block:
                totalstring.Reserve(totalstring.Length() +
                                    12 + op_strlen(type) + INT_STRSIZE(int) +
                                    ((len + 15) / 16) * 6 + 3 * len);
				totalstring.AppendFormat(UNI_L("\n %s : %d bytes \n"), type,(int) len);

				for(pos1 = 0; pos1 <len;pos1+=16)
				{
					totalstring.AppendFormat(UNI_L("%04.4lx :"),pos1);
					for(pos2 = pos1; pos2<pos1+16; pos2++)
					{
						if(pos2 >= len)
							break;
						else
							totalstring.AppendFormat(UNI_L(" %02.2x"), source[pos2]);
					}
					totalstring.Append(UNI_L("\n"));
				}
                data = (SSL_secure_varvector32 *)data->Suc();
			}
		}
	}
#endif // LIBSSL_HANDSHAKE_HEXDUMP

		flags.internal_error_message_set = TRUE;

		SetProgressInformation(SET_INTERNAL_ERROR_MESSAGE,0, totalstring.CStr());
	}
}

int InitSecurityCertBrowsing(OpWindow *parent_window, SSL_Certificate_DisplayContext *ctx)
{
 	if(ctx == NULL)
 		return FALSE;

	Window* win;
	WindowCommander* wic = NULL;
	for(win = g_windowManager->FirstWindow(); win; win = win->Suc())
	{
		if (win->GetOpWindow() == ctx->GetWindow())
		{
			wic = win->GetWindowCommander();
			break;
		}
	}

	OpSSLListener::SSLCertificateReason reason;

	switch (ctx->GetAction())
	{
		/*
		 * Seems there's no way to discern whether it should be
		 * OpSSLListener::SSL_REASON_NO_CHAIN than
		 * OpSSLListener::SSL_REASON_INCOMPLETE_CHAIN, therefore
		 * we stick with the most restrictive value here.
		 */
		case IDM_NO_CHAIN_CERTIFICATE:
			reason = OpSSLListener::SSL_REASON_NO_CHAIN;
			break;

		case IDM_WRONG_CERTIFICATE_NAME:
			reason = OpSSLListener::SSL_REASON_DIFFERENT_CERTIFICATE;
			break;

		case IDM_EXPIRED_SERVER_CERTIFICATE:
			reason = OpSSLListener::SSL_REASON_CERT_EXPIRED;
			break;

		case IDM_SSL_ANONYMOUS_CONNECTION:
			reason = OpSSLListener::SSL_REASON_ANONYMOUS_CONNECTION;
			break;

		case IDM_WARN_CERTIFICATE:
			reason = OpSSLListener::SSL_REASON_CERTIFICATE_WARNING;
			break;

		case IDM_SSL_AUTHENTICATED_ONLY:
			reason = OpSSLListener::SSL_REASON_AUTHENTICATED_ONLY;
			break;

		case IDM_SSL_DIS_CIPHER_LOW_ENCRYPTION_LEVEL:
			reason = OpSSLListener::SSL_REASON_DISABLED_CIPHER;
			break;

		case IDM_SSL_LOW_ENCRYPTION_LEVEL:
			reason = OpSSLListener::SSL_REASON_LOW_ENCRYPTION;
			break;

		default:
			reason = OpSSLListener::SSL_REASON_UNKNOWN;
			break;
	}

	// set up possible options/actions that can be done during this interaction
	int options = OpSSLListener::SSL_CERT_OPTION_NONE;
	if (ctx->GetOKButtonIsAcceptButton())
		options |= OpSSLListener::SSL_CERT_OPTION_ACCEPT;
	else
		options |= OpSSLListener::SSL_CERT_OPTION_OK;

	if (ctx->GetShowImportButton())
		options |= OpSSLListener::SSL_CERT_OPTION_IMPORT;

	if (ctx->GetShowExportButton())
		options |= OpSSLListener::SSL_CERT_OPTION_EXPORT;

	if (ctx->GetShowDeleteInstallButton())
	{
		if (ctx->GetDeleteInstallButtonIsInstall())
			options |= OpSSLListener::SSL_CERT_OPTION_INSTALL;
		else
			options |= OpSSLListener::SSL_CERT_OPTION_DELETE;
	}

	OpRequestImpl *request = NULL;
	if (ctx->GetURL().GetRep()->GetDataStorage())
		request = ctx->GetURL().GetRep()->GetDataStorage()->GetOpRequestImpl();

	if (!ctx->GetURL().GetAttribute(URL::KUseGenericAuthenticationHandling) && request)
	{
		request->CertificateBrowsingRequired(static_cast<OpSSLListener::SSLCertificateContext*>(ctx), reason, static_cast<OpSSLListener::SSLCertificateOption>(options));
	}
	else
	{
		if (wic)
			wic->GetSSLListener()->OnCertificateBrowsingNeeded(wic, static_cast<OpSSLListener::SSLCertificateContext*>(ctx), reason, static_cast<OpSSLListener::SSLCertificateOption>(options));
		else // if no window is available, fallback on the windowcommanderManager SSL-listener
			g_windowCommanderManager->GetSSLListener()->OnCertificateBrowsingNeeded(NULL, static_cast<OpSSLListener::SSLCertificateContext*>(ctx), reason, static_cast<OpSSLListener::SSLCertificateOption>(options));
	}

	return TRUE;
}

#endif // _NATIVE_SSL_SUPPORT_
