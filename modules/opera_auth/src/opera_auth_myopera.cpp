/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef OPERA_AUTH_SUPPORT

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/util/adt/bytebuffer.h"

#include "modules/windowcommander/src/TransferManager.h"

#include "modules/url/url2.h"
#include "modules/forms/form.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"

#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/sslrand.h"

#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#ifdef PREFS_HAVE_OPERA_ACCOUNT
#include "modules/prefs/prefsmanager/collections/pc_opera_account.h"
#endif // PREFS_HAVE_OPERA_ACCOUNT

#include "modules/formats/hex_converter.h"
#include "modules/forms/formmanager.h"

#include "modules/opera_auth/opera_auth_base.h"
#include "modules/opera_auth/opera_auth_myopera.h"
#include "modules/libcrypto/include/CryptoHash.h"

#define OPERA_AUTH_SERVER UNI_L("https://auth.opera.com/xml")
#define OPERA_AUTH_TEST_SERVER UNI_L("https://auth-test.opera.com/xml")
#define OPERA_AUTH_NS UNI_L("http://xmlns.opera.com/2007/auth")
#define AUTHNAME(localname) XMLExpandedName(OPERA_AUTH_NS, UNI_L(localname))
#define SHARED_SECRET "ZdyPtSFV3n06Bidzl6tudREb"

MyOperaAuthentication::MyOperaAuthentication()
	: OperaAuthentication()
	, m_transferItem(NULL)
	, m_request_type(OperaAuthAuthenticate)
{
}

MyOperaAuthentication::~MyOperaAuthentication()
{
	if (m_transferItem)
	{
		m_transferItem->SetTransferListener(NULL);
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;
	}
}

void MyOperaAuthentication::OnTimeout(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_OPERA_AUTH_TIMEOUT);

	if(msg == MSG_OPERA_AUTH_TIMEOUT)
		Abort();
}

OP_STATUS MyOperaAuthentication::StartHTTPRequest(XMLFragment& xml)
{
	OpTransferItem *found = NULL;
	OpString server;

	ByteBuffer buffer;
	unsigned int bytes = 0;
	UINT32 chunk;
	OpString8 xml_send;

	RETURN_IF_ERROR(xml.GetEncodedXML(buffer, TRUE, "utf-8", FALSE));

	for(chunk = 0; chunk < buffer.GetChunkCount(); chunk++)
	{
		char *chunk_ptr = buffer.GetChunk(chunk, &bytes);
		if(chunk_ptr)
		{
			RETURN_IF_ERROR(xml_send.Append(chunk_ptr, bytes));
		}
	}

	// Setup the server to use
#ifdef PREFS_HAVE_OPERA_ACCOUNT
	if (g_pcopera_account)
	{
		OP_STATUS s;

		TRAP(s, g_pcopera_account->GetStringPrefL(PrefsCollectionOperaAccount::ServerAddress, server));
		RETURN_IF_ERROR(s);
	}
	else
#endif // PREFS_HAVE_OPERA_ACCOUNT
	{
		server.Set(OPERA_AUTH_SERVER);
    }
	g_transferManager->GetTransferItem(&found, server.CStr());
	if (found)
	{
		m_transferItem = (TransferItem*)found;
		URL *host_url = m_transferItem->GetURL();
		if(host_url)
		{
			host_url->SetHTTP_Method(HTTP_METHOD_POST);
			host_url->SetHTTP_Data(xml_send.CStr(), TRUE);
			host_url->SetHTTP_ContentType(FormUrlEncodedString);
			// Turn off UI interaction if the certificate is invalid and the servername is the default server.
			// This means that on the default server, invalid certificates will not show a UI dialog but fail instead.
#ifdef URL_DISABLE_INTERACTION
			if(!server.Compare(OPERA_AUTH_SERVER) || !server.Compare(OPERA_AUTH_TEST_SERVER))
			{
				RETURN_IF_ERROR(host_url->SetAttribute(URL::KBlockUserInteraction, TRUE));
			}
#endif
			// Initiate the download of the document pointed to by the URL, and forget about it.
			// The OnProgress function will deal with successful downloads and check whether a user
			// notification is required.
			m_transferItem->SetTransferListener(this);
			URL tmp;
			host_url->Load(g_main_message_handler, tmp, TRUE, FALSE);

			// If we haven't received any parsing data within one minute, assume something is wrong and abort
			RETURN_IF_ERROR(OpAuthTimeout::RestartTimeout());
		}
	}
	return OpStatus::OK;
}

OP_STATUS MyOperaAuthentication::Authenticate(const OpStringC& username, const OpStringC& password, const OpStringC& device_name, const OpStringC& install_id, BOOL force)
{
	XMLFragment xmlfragment;

	if(device_name.HasContent())
	{
		OperaRegistrationInformation reg_info;

		RETURN_IF_ERROR(reg_info.m_username.Set(username));
		RETURN_IF_ERROR(reg_info.m_password.Set(password));
		RETURN_IF_ERROR(reg_info.m_webserver_name.Set(device_name));
		RETURN_IF_ERROR(reg_info.m_install_id.Set(install_id));

		reg_info.m_registration_type = OperaRegWebserver;
		reg_info.m_force_webserver_name = force;

		return CreateAccount(reg_info);
	}
	else
	{
		m_request_type = OperaAuthAuthenticate;

		if (username.IsEmpty() || password.IsEmpty())
		{
			CallListenerWithError(AUTH_ACCOUNT_AUTH_FAILURE);
			return OpStatus::OK;
		}

		RETURN_IF_ERROR(xmlfragment.OpenElement(AUTHNAME("auth")));
		RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("version"), UNI_L("1.1")));
		RETURN_IF_ERROR(xmlfragment.OpenElement(AUTHNAME("login")));
		RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("username"), username.CStr()));
		RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("password"), password.CStr()));
		xmlfragment.CloseElement();

		return StartHTTPRequest(xmlfragment);
	}
}

OP_STATUS MyOperaAuthentication::ChangePassword(const OpStringC& username, const OpStringC& old_password, const OpStringC& new_password)
{
	XMLFragment xmlfragment;

	OpString8 username_utf8;
	OpString8 old_password_utf8;
	OpString8 new_password_utf8;


	RETURN_IF_ERROR(username_utf8.Set(username.CStr()));
	RETURN_IF_ERROR(old_password_utf8.Set(old_password.CStr()));
	RETURN_IF_ERROR(new_password_utf8.Set(new_password.CStr()));

	m_request_type = OperaAuthChangePassword;

	if (username.IsEmpty() || old_password.IsEmpty() || new_password.IsEmpty())
	{
		CallListenerWithError(AUTH_ACCOUNT_AUTH_FAILURE);
		return OpStatus::OK;
	}

	OpAutoPtr<CryptoHash> hash(CryptoHash::CreateSHA1());
	if (!hash.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(hash->InitHash());

	hash->CalculateHash(username_utf8.CStr());
	hash->CalculateHash(old_password_utf8.CStr());
	hash->CalculateHash(new_password_utf8.CStr());
	hash->CalculateHash(SHARED_SECRET);

	OP_ASSERT(hash->Size() == 20);
	UINT8 result[20]; /* Array OK haavardm 2011-02-17 */

	hash->ExtractHash(result);

	OpString key;
	RETURN_IF_ERROR(HexConverter::AppendAsHex(key, result, 20, HexConverter::UseLowercase));

	RETURN_IF_ERROR(xmlfragment.OpenElement(AUTHNAME("auth")));
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("version"), UNI_L("1.1")));
	RETURN_IF_ERROR(xmlfragment.OpenElement(AUTHNAME("change_password")));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("username"), username.CStr()));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("old_password"), old_password.CStr()));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("new_password"), new_password.CStr()));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("key"), key.CStr()));
	xmlfragment.CloseElement();

	return StartHTTPRequest(xmlfragment);
}

OP_STATUS MyOperaAuthentication::CreateAccount(OperaRegistrationInformation& reg_info)
{
	XMLFragment xmlfragment;
	OpString key;

	m_request_type = reg_info.m_registration_type == OperaRegNormal ? OperaAuthCreateUser : OperaAuthCreateDevice;
	if (!PreValidateCredentials(reg_info))
		return OpStatus::OK;

	RETURN_IF_ERROR(m_reg_info.Set(reg_info));

	RETURN_IF_ERROR(key.Append(reg_info.m_username));
	RETURN_IF_ERROR(key.Append(reg_info.m_password));
	RETURN_IF_ERROR(key.Append(MYOPERA_SECRET_KEY));

	OpAutoPtr<CryptoHash> hash(CryptoHash::CreateMD5());
	if (!hash.get())
		return OpStatus::ERR_NO_MEMORY;

	byte *target = OP_NEWA(byte, hash->Size());
	ANCHOR_ARRAY(byte, target);
	RETURN_OOM_IF_NULL(target);

	OpString8 key8;

	RETURN_IF_ERROR(key8.SetUTF8FromUTF16(key));
	hash->InitHash();
	hash->CalculateHash((const byte *)key8.CStr(), key8.Length());
	hash->ExtractHash(target);

	key.Empty();
	RETURN_IF_ERROR(HexConverter::AppendAsHex(key, target, hash->Size()));

	RETURN_IF_ERROR(xmlfragment.OpenElement(UNI_L("create")));
	RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("username"), reg_info.m_username.CStr()));
	RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("password"), reg_info.m_password.CStr()));
	RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("key"), key.CStr()));

	if(reg_info.m_registration_type == OperaRegNormal)
	{
		// create user:
		// "<?xml version=\"1.0\" encoding=\"utf-8\"?><create type=\"user\"><username>%s</username><password>%s</password><email>%s</email><key>%s</key></create>"

		RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("type"), UNI_L("user")));
		RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("email"), reg_info.m_email.CStr()));
	}
	else
	{
		// create device:
		// "<?xml version=\"1.0\" encoding=\"utf-8\"?><create type=\"device\"><username>%s</username><password>%s</password><devicename>%s</devicename><key>%s</key></create>"

		RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("type"), UNI_L("device")));
		RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("devicename"), reg_info.m_webserver_name.CStr()));
		RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("installid"), reg_info.m_install_id.CStr()));
		RETURN_IF_ERROR(xmlfragment.AddText(UNI_L("force"), reg_info.m_force_webserver_name ? UNI_L("1") : UNI_L("0")));
	}
	xmlfragment.CloseElement();

	return StartHTTPRequest(xmlfragment);
}

OP_STATUS MyOperaAuthentication::ReleaseDevice(const OpStringC& username, const OpStringC& password, const OpStringC& devicename)
{
	XMLFragment xmlfragment;

	m_request_type = OperaAuthReleaseDevice;

	RETURN_IF_ERROR(xmlfragment.OpenElement(AUTHNAME("auth")));
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("version"), UNI_L("1.1")));
	RETURN_IF_ERROR(xmlfragment.OpenElement(AUTHNAME("release")));
	RETURN_IF_ERROR(xmlfragment.SetAttribute(UNI_L("type"), UNI_L("webserver")));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("username"), username.CStr() ? username.CStr() : UNI_L("")));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("password"), password.CStr() ? password.CStr() : UNI_L("")));
	RETURN_IF_ERROR(xmlfragment.AddText(AUTHNAME("devicename"), devicename.CStr() ? devicename.CStr() : UNI_L("")));
	xmlfragment.CloseElement(); // </release>
	xmlfragment.CloseElement(); // </auth>

	return StartHTTPRequest(xmlfragment);
}

void MyOperaAuthentication::Abort(BOOL userAction)
{
	OpAuthTimeout::StopTimeout();

	URL *url = m_transferItem ? m_transferItem->GetURL() : NULL;
	if(url && url->GetAttribute(URL::KLoadStatus) == URL_LOADING)
	{
		url->StopLoading(g_main_message_handler);

		m_transferItem->SetTransferListener(NULL);
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;
		CallListenerWithError(userAction ? AUTH_ERROR_COMM_ABORTED : AUTH_ERROR_COMM_TIMEOUT);
	}
}

BOOL MyOperaAuthentication::OperationInProgress()
{
	URL *url = m_transferItem ? m_transferItem->GetURL() : NULL;
	return url && url->GetAttribute(URL::KLoadStatus) == URL_LOADING;	
}

void MyOperaAuthentication::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	OperaAuthError ret = AUTH_OK;
	TempBuffer secret;
	TempBuffer token;
	TempBuffer message;

	switch (status) // Switch on the various status messages.
	{
		case TRANSFER_ABORTED:
			{
				ret = AUTH_ERROR_COMM_ABORTED;
				break;
			}
		case TRANSFER_DONE:
			{
				// This is the interesting case, the transfer has completed.
				URL* url = transferItem->GetURL();
				if (url)
				{
					OpString data;
					BOOL more = TRUE;
					OpAutoPtr<URL_DataDescriptor> url_data_desc(url->GetDescriptor(NULL, TRUE));

					if(!url_data_desc.get())
					{
						more = FALSE;
						ret = AUTH_ERROR_COMM_FAIL;
					}
					while(more)
					{
						TRAPD(err, url_data_desc->RetrieveDataL(more));
						if (OpStatus::IsError(err))
						{
							if (OpStatus::IsMemoryError(err))
								ret = AUTH_ERROR_MEMORY;
							else
								ret = AUTH_ERROR_COMM_FAIL;

							break;
						}
						if(OpStatus::IsError(data.Append((uni_char *)url_data_desc->GetBuffer(), UNICODE_DOWNSIZE(url_data_desc->GetBufSize()))))
						{
							ret = AUTH_ERROR_MEMORY;
							break;
						}
						url_data_desc->ConsumeData(UNICODE_SIZE(UNICODE_DOWNSIZE(url_data_desc->GetBufSize())));
					}
/* 

User authentication response:

<?xml version="1.0" encoding="utf-8"?>
<auth version="1.1" xmlns="http://xmlns.opera.com/2007/auth">
<code>CDATA</code>
<message>CDATA</message>
<token>CDATA</token>
</auth>

Device creation response:

<?xml version="1.0" encoding="utf-8"?>
<response>
<code>CDATA</code>
<secret>CDATA</secret>
</response>

*/
					if(ret == AUTH_OK)
					{
						XMLFragment fragment;

						if(OpStatus::IsError(fragment.Parse(data.CStr(), data.Length())))
						{
							ret = AUTH_ERROR_PARSER;
							break;
						}
						TempBuffer buffer;
						if(fragment.EnterElement(UNI_L("response")) || fragment.EnterElement(AUTHNAME("auth")))
						{
							while(fragment.HasMoreElements())
							{
								if(fragment.EnterAnyElement())
								{
									if(!uni_stricmp(fragment.GetElementName().GetLocalPart(), UNI_L("code")))
									{
										OpStatus::Ignore(fragment.GetAllText(buffer));
									}
									else if(!uni_stricmp(fragment.GetElementName().GetLocalPart(), UNI_L("secret")))
									{
										OpStatus::Ignore(fragment.GetAllText(secret));
									}
									else if(!uni_stricmp(fragment.GetElementName().GetLocalPart(), UNI_L("message")))
									{
										OpStatus::Ignore(fragment.GetAllText(message));
									}
									else if(!uni_stricmp(fragment.GetElementName().GetLocalPart(), UNI_L("token")))
									{
										OpStatus::Ignore(fragment.GetAllText(token));
									}
									fragment.LeaveElement();
								}
								else
								{
									break;
								}
							}
							fragment.LeaveElement();
						}
						int code = uni_atoi(buffer.GetStorage());

						switch(code)
						{
							case 200:
								ret = AUTH_OK;
								break;

							case 400:
								ret = AUTH_ERROR_PARSER;
								break;

							case 401:
								ret = AUTH_ACCOUNT_CREATE_USER_EXISTS;
								break;

							case 402:
								ret = AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR;
								break;

							case 403:
							case 404:
								ret = AUTH_ACCOUNT_AUTH_FAILURE;
								break;

							case 405:
								ret = AUTH_ACCOUNT_CREATE_DEVICE_EXISTS;
								break;

							case 406:
								ret = AUTH_ACCOUNT_CREATE_USER_EMAIL_EXISTS;
								break;

							case 407:
								ret = AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS;
								break;

							case 408:
								ret = AUTH_ACCOUNT_CREATE_USER_TOO_SHORT;
								break;

							case 409:
								ret = AUTH_ACCOUNT_CREATE_USER_TOO_LONG;
								break;

							case 410:
								ret = AUTH_ACCOUNT_AUTH_INVALID_KEY;
								break;

							case 411:
								ret = AUTH_ACCOUNT_CREATE_DEVICE_INVALID;
								break;

							case 413:
								ret = AUTH_ACCOUNT_CHANGE_PASSWORD_TO_MANY_CHANGES;

							default:
								ret = AUTH_ERROR;
								break;
						}
					}
				}
				else
				{
					ret = AUTH_ERROR_COMM_FAIL;
				}
				break;
			}
		case TRANSFER_PROGRESS:
			{
				// This is the only case where new callbacks to this function must be expected.
				// To delete would cause an error.
				return;
			}
		case TRANSFER_FAILED:
			{
				ret = AUTH_ERROR_COMM_FAIL;
				break;
			}
	}
	OpAuthTimeout::StopTimeout();

	m_transferItem->SetTransferListener(NULL);
	g_transferManager->ReleaseTransferItem(m_transferItem);
	m_transferItem = NULL;

	// Send the error at the end otherwise the OperaAuthError can try and do stuff before
	// the transfer item is cleaned up
	if (m_listener)
	{
		switch(m_request_type)
		{
			case OperaAuthCreateDevice:
				{
					OpString secret_string;
					OpString msg_string;

					OpStatus::Ignore(secret_string.Set(secret.GetStorage(), secret.GetCapacity()));
					OpStatus::Ignore(msg_string.Set(message.GetStorage(), message.GetCapacity()));
					m_listener->OnAuthCreateDevice(ret, secret_string, msg_string);
				}
				break;

			case OperaAuthAuthenticate:
				{
					OpString secret_string;
					OpString token_string;
					OpString msg_string;

					OpStatus::Ignore(secret_string.Set(secret.GetStorage(), secret.GetCapacity()));
					OpStatus::Ignore(msg_string.Set(message.GetStorage(), message.GetCapacity()));
					OpStatus::Ignore(token_string.Set(token.GetStorage(), token.GetCapacity()));
					m_listener->OnAuthAuthenticate(ret, secret_string, msg_string, token_string);
				}
				break;

			case OperaAuthChangePassword:
				m_listener->OnAuthPasswordChanged(ret);
				break;

			case OperaAuthCreateUser:
				m_listener->OnAuthCreateAccount(ret, m_reg_info);
				break;

			case OperaAuthReleaseDevice:
				m_listener->OnAuthReleaseDevice(ret);
				break;
		}
	}
}

OperaAuthError MyOperaAuthentication::PreValidateUsername(OpStringC& username)
{
	OpString host;
	if (username.Length() < 3)
		return AUTH_ACCOUNT_CREATE_USER_TOO_SHORT;
	if (username.Length() > 30)
		return AUTH_ACCOUNT_CREATE_USER_TOO_LONG;
	if (username.Find("xn--") == 0 ||
		username.FindFirstOf(OpStringC16(UNI_L("+.@/?&_ "))) != KNotFound)
		return AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS;
	if (OpStatus::IsError(host.AppendFormat(UNI_L("%s.opera.com"), username.CStr()))) // create dummy servername
		return AUTH_ERROR_MEMORY;
	if (g_url_api->GetServerName(host, TRUE) == 0)
		return AUTH_ACCOUNT_CREATE_USER_INVALID_CHARS;

	return AUTH_OK;
}

OperaAuthError MyOperaAuthentication::PreValidatePassword(OpStringC& password)
{
	if (password.Length() < 5)
		return AUTH_ACCOUNT_CREATE_PASSWORD_TOO_SHORT;
	if (password.Length() > 256)
		return AUTH_ACCOUNT_CREATE_PASSWORD_TOO_LONG;

	return AUTH_OK;
}

OperaAuthError MyOperaAuthentication::PreValidateEmail(OpStringC& email)
{
	if (email.IsEmpty())
		return AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR;
#ifdef FORMS_VALIDATE_EMAIL
	if (!FormManager::IsValidEmailAddress(email.CStr()))
		return AUTH_ACCOUNT_CREATE_USER_EMAIL_ERROR;
#endif

	return AUTH_OK;
}

OperaAuthError MyOperaAuthentication::PreValidateDevicename(OpStringC& devicename, OpStringC& username, OpStringC& install_id)
{
	OpString host;
	if (devicename.IsEmpty())
		return AUTH_ACCOUNT_CREATE_DEVICE_INVALID;
	if (OpStatus::IsError(host.AppendFormat(UNI_L("%s.%s.opera.com"), devicename.CStr(), username.CStr()))) // create dummy servername
		return AUTH_ERROR_MEMORY;
	if (g_url_api->GetServerName(host, TRUE) == 0)
		return AUTH_ACCOUNT_CREATE_DEVICE_INVALID;
	if (install_id.IsEmpty())
		return AUTH_ERROR_PARSER;

	return AUTH_OK;
}

BOOL MyOperaAuthentication::PreValidateCredentials(OperaRegistrationInformation& reg_info)
{
	OperaAuthError error = PreValidateUsername(reg_info.m_username);
	if (error == AUTH_OK)
		error = PreValidatePassword(reg_info.m_password);
	if (error == AUTH_OK && m_request_type == OperaAuthCreateUser)
		error = PreValidateEmail(reg_info.m_email);
	if (error == AUTH_OK && m_request_type == OperaAuthCreateDevice)
		error = PreValidateDevicename(reg_info.m_webserver_name, reg_info.m_username, reg_info.m_install_id);

	if (error != AUTH_OK)
	{
		CallListenerWithError(error);
		return FALSE;
	}
	return TRUE;
}

void MyOperaAuthentication::CallListenerWithError(OperaAuthError error)
{
	if (m_listener)
	{
		switch(m_request_type)
		{
			case OperaAuthCreateDevice:
				m_listener->OnAuthCreateDevice(error, UNI_L(""), UNI_L(""));
				break;
			case OperaAuthAuthenticate:
				m_listener->OnAuthAuthenticate(error, UNI_L(""), UNI_L(""), UNI_L(""));
				break;
			case OperaAuthCreateUser:
				m_listener->OnAuthCreateAccount(error, m_reg_info);
				break;
			case OperaAuthReleaseDevice:
				m_listener->OnAuthReleaseDevice(error);
				break;
		}
	}
}

#endif // OPERA_AUTH_SUPPORT
