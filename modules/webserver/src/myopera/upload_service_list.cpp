/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 * Web server implementation 
 */

#include "core/pch.h"

#if defined(WEBSERVER_SUPPORT) && defined(WEB_UPLOAD_SERVICE_LIST)

#include "modules/webserver/src/myopera/upload_service_list.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "modules/forms/form.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/webserver/webserver-api.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/util/htmlify.h"

#define UPLOAD_SERVICE_PROTOCOL_VERSION "1.0"

UploadServiceListManager::UploadServiceListManager(WebserverEventListener *listener)
	: m_transferItem(NULL)
	, m_event_listener(listener)
	, m_in_progress(FALSE)
	, m_lingering_upload(FALSE)
	, m_authenticated(FALSE)	
	, m_retries(0)
	, m_firstTimeout(TRUE)
{
}

UploadServiceListManager::~UploadServiceListManager()
{
	if (m_transferItem)
	{
		m_transferItem->SetTransferListener(NULL);
		g_transferManager->ReleaseTransferItem(m_transferItem);		
	}
}

/* static */ OP_STATUS
UploadServiceListManager::Make(UploadServiceListManager *&uploader, const uni_char *upload_service_uri, const uni_char *device_uri, const uni_char *devicename, const uni_char *username, WebserverEventListener *listener)
{
	if (!g_webserver->IsRunning())
	{
		OP_ASSERT(!"webserver must be running to before starting UploadServiceListManager");
		return OpStatus::ERR;
	}
	
	uploader = NULL;
	OpAutoPtr<UploadServiceListManager> temp_uploader(OP_NEW(UploadServiceListManager,(listener)));
	RETURN_OOM_IF_NULL(temp_uploader.get());
	
	RETURN_IF_ERROR(temp_uploader->m_sentral_server_uri.Set(upload_service_uri));
	RETURN_IF_ERROR(temp_uploader->m_device_uri.Set(device_uri));
	RETURN_IF_ERROR(temp_uploader->m_username.Set(username));
	RETURN_IF_ERROR(temp_uploader->m_devicename.Set(devicename));
	
	temp_uploader->m_timer.SetTimerListener(temp_uploader.get()); 
		
	uploader = temp_uploader.release();

	return OpStatus::OK;
}

OP_STATUS UploadServiceListManager::Upload(OpAutoVector<WebSubServer> *subservers, const char *upload_service_session_token)
{
	if (!g_webserver->IsRunning())
		return OpStatus::OK;

	if (!g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::ServiceDiscoveryEnabled))
		return OpStatus::OK;

	if (m_in_progress == TRUE)
	{
		m_lingering_upload = TRUE;
		return OpStatus::OK;
	}
	
	/* This xml is defined in http://wiki/developerwiki/index.php/Spec_for_logging_proxy_activity */
	
	OpString xml_data;

	xml_data.AppendFormat(UNI_L("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<services version=\"%s\">\r\n"), UNI_L(UPLOAD_SERVICE_PROTOCOL_VERSION));
	//xml_data.AppendFormat(UNI_L("<server>%s</server>\r\n"), m_device_uri.CStr());
	xml_data.AppendFormat(UNI_L("<user>%s</user>\r\n"), m_username.CStr());
	xml_data.AppendFormat(UNI_L("<device>%s</device>\r\n"), m_devicename.CStr());
	xml_data.Append("<secret>");
	xml_data.Append(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword));
	xml_data.Append("</secret>\r\n");

	xml_data.AppendFormat(UNI_L("<serviceList>\r\n"));
	
	int n = subservers->GetCount();
	int i;
	WebSubServer *subserver;
	for ( i = 0; i < n; i++)
	{
		subserver = subservers->Get(i);
		
		if(subserver && subserver->IsVisibleASD())
		{
			uni_char *converted_subServerName = NULL;
			uni_char *converted_subServerVirtualPath = NULL;
			uni_char *converted_subServerAuthor = NULL;
			uni_char *converted_subServerDescription = NULL;
			uni_char *converted_subServerOriginUrl = NULL;
			uni_char *converted_subServerVersion = NULL;

			if (subserver->GetSubServerName().CStr())
			{
				converted_subServerName = XHTMLify_string(subserver->GetSubServerName().CStr(), subserver->GetSubServerName().Length(), TRUE, TRUE, FALSE);

				if(converted_subServerName == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (subserver->GetSubServerVirtualPath().CStr())
			{
				converted_subServerVirtualPath = XHTMLify_string(subserver->GetSubServerVirtualPath().CStr(), subserver->GetSubServerVirtualPath().Length(), TRUE, TRUE, FALSE);

				if(converted_subServerVirtualPath == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (subserver->GetSubServerAuthor().CStr())
			{
				converted_subServerAuthor = XHTMLify_string(subserver->GetSubServerAuthor().CStr(), subserver->GetSubServerAuthor().Length(), TRUE, TRUE, FALSE);

				if(converted_subServerAuthor == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (subserver->GetSubServerDescription().CStr())
			{
				converted_subServerDescription = XHTMLify_string(subserver->GetSubServerDescription().CStr(), subserver->GetSubServerDescription().Length(), TRUE, TRUE, FALSE);

				if(converted_subServerDescription == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (subserver->GetSubServerOriginUrl().CStr())
			{
				converted_subServerOriginUrl = XHTMLify_string(subserver->GetSubServerOriginUrl().CStr(), subserver->GetSubServerOriginUrl().Length(), TRUE, TRUE, FALSE);

				if(converted_subServerOriginUrl == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (subserver->GetVersion().CStr())
			{
				converted_subServerVersion = XHTMLify_string(subserver->GetVersion().CStr(), subserver->GetVersion().Length(), TRUE, TRUE, FALSE);
			
				if(converted_subServerVersion == NULL)
					return OpStatus::ERR_NO_MEMORY;
			}

			xml_data.AppendFormat(UNI_L("<service name=\"%s\" path=\"%s\" author=\"%s\" description=\"%s\" originurl=\"%s\" version=\"%s\" access=\"%s\" />\r\n"), 
				                  converted_subServerName,
					              converted_subServerVirtualPath,
						          converted_subServerAuthor, 
							      converted_subServerDescription,
								  converted_subServerOriginUrl,
								  converted_subServerVersion,
								  subserver->GetPasswordProtected()==1?UNI_L("private"):UNI_L("public"));

			OP_DELETEA(converted_subServerName);
			OP_DELETEA(converted_subServerVirtualPath);
			OP_DELETEA(converted_subServerAuthor);
			OP_DELETEA(converted_subServerOriginUrl);
			OP_DELETEA(converted_subServerDescription);
			OP_DELETEA(converted_subServerVersion);
		}
	}
	
	xml_data.Append("</serviceList>\r\n</services>\r\n");
	
	RETURN_IF_ERROR(m_xml_data.SetUTF8FromUTF16(xml_data.CStr()));
	

	if (m_firstTimeout)
	{
		m_timer.Start(2000);
	}
	else
		m_timer.Start(30000); /* We wait 30 seconds before we send up the list, in case there is a burst of new services. If a new service is started
	 					    before the seconds have past, the upload is restarted */
	
	m_retries = 0;

	return OpStatus::OK;
}

void
UploadServiceListManager::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	if (!g_webserver->IsRunning() || !g_webserver->GetRendezvousConnected())
		return;
		
	UploadServiceStatus ret = UPLOAD_SERVICE_OK;

	switch (status) // Switch on the various status messages.
	{
		case TRANSFER_ABORTED:
		{
			ret = UPLOAD_SERVICE_ERROR_COMM_ABORTED;
			m_in_progress = FALSE;
			break;
		}
		case TRANSFER_DONE:
		{
			m_in_progress = FALSE;

			URL* url = transferItem->GetURL();
			if (url)
			{
				OpString data;
				BOOL more = TRUE;
				OpAutoPtr<URL_DataDescriptor> url_data_desc(url->GetDescriptor(NULL, URL::KFollowRedirect));

				if(!url_data_desc.get())
				{
					more = FALSE;
					ret = UPLOAD_SERVICE_ERROR_COMM_FAIL;
				}

				while(more)
				{
					TRAPD(err, url_data_desc->RetrieveDataL(more));
					if (OpStatus::IsError(err))
					{
						if (OpStatus::IsMemoryError(err))
							ret = UPLOAD_SERVICE_ERROR_MEMORY;
						else
							ret = UPLOAD_SERVICE_ERROR_COMM_FAIL;

						break;
					}
					if(OpStatus::IsError(data.Append((uni_char *)url_data_desc->GetBuffer(), UNICODE_DOWNSIZE(url_data_desc->GetBufSize()))))
					{
						ret = UPLOAD_SERVICE_ERROR_MEMORY;
						break;
					}
					url_data_desc->ConsumeData(UNICODE_SIZE(UNICODE_DOWNSIZE(url_data_desc->GetBufSize())));
				}
				
				if(ret == UPLOAD_SERVICE_OK)
				{
					XMLFragment fragment;

					if(OpStatus::IsError(fragment.Parse(data.CStr(), data.Length())))
					{
						ret = UPLOAD_SERVICE_ERROR_PARSER;
						break;
					}
					
					const uni_char *code = NULL;
					const uni_char *version = NULL;
					
					if(fragment.EnterElement(UNI_L("services")))
					{
						while(fragment.HasMoreElements())
						{
							version = fragment.GetAttribute(UNI_L("version"));
							
							if(fragment.EnterElement(UNI_L("code")))
							{
								code = fragment.GetText();
							}
							else 
							{
								fragment.EnterAnyElement();
							}
							
							fragment.LeaveElement();
						}
						fragment.LeaveElement();
					}
					
					if (version && uni_atof(version) < uni_atof(UNI_L(UPLOAD_SERVICE_PROTOCOL_VERSION)))
					{
						if (m_event_listener)
							m_event_listener->OnWebserverUploadServiceStatus(UPLOAD_SERVICE_WARNING_OLD_PROTOCOL);
					}
					
					if (code && version)
					{
						switch(uni_atoi(code))
						{
							case 200:
								ret = UPLOAD_SERVICE_OK;
								break;
	
							case 403:
								ret = UPLOAD_SERVICE_AUTH_FAILURE;
								break;
	
							case 400:
								ret = UPLOAD_SERVICE_ERROR_PARSER;
								break;
								
							default:
								ret = UPLOAD_SERVICE_ERROR;
								break;
						}
					}
					else
					{
						ret = UPLOAD_SERVICE_ERROR_PARSER;
					}
				}
			}
			else
			{
				ret = UPLOAD_SERVICE_ERROR_COMM_FAIL;
			}
	
			break;
		}
		case TRANSFER_PROGRESS:
		{
			m_in_progress = TRUE;
			return;
		}
		case TRANSFER_FAILED:
		{
			m_in_progress = FALSE;
			ret = UPLOAD_SERVICE_ERROR_COMM_FAIL;
			break;
		}
		default:
		{
			m_in_progress = FALSE;
			break;
		}

	}

	
	
	if (m_in_progress == FALSE)
	{
		m_transferItem->SetTransferListener(NULL);
		g_transferManager->ReleaseTransferItem(m_transferItem);
		
		if (m_event_listener)
			m_event_listener->OnWebserverUploadServiceStatus(ret);
		
		m_transferItem = NULL;		
		
		if (m_lingering_upload == TRUE)
		{
			m_lingering_upload = FALSE;
			

			m_timer.Start(2000);	/* If there was a request for uploading a new list before the previous was finished, start this now */
			m_retries = 0;
		}
		else 
		{
			m_timer.Stop();
		}
	}
}

void
UploadServiceListManager::OnReset(OpTransferItem* transferItem)
{
}


void
UploadServiceListManager::OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to)
{
}

/* virtual */ void 
UploadServiceListManager::OnTimeOut(OpTimer* timer)
{
	if (!g_webserver->IsRunning() 
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		|| !g_webserver->GetRendezvousConnected()
#endif
		)
	{
		m_in_progress = FALSE;
		return;
	}
		
	if (m_in_progress == TRUE) /* If we got a timeout while in progress, this was a upload timeout. The upload is restarted. */
	{
		m_in_progress = FALSE;

		URL *url = m_transferItem->GetURL();
		if(url && url->GetAttribute(URL::KLoadStatus) == URL_LOADING)
		{
			url->StopLoading(g_main_message_handler);

			m_transferItem->SetTransferListener(NULL);
			g_transferManager->ReleaseTransferItem(m_transferItem);
			m_transferItem = NULL;
		}

		m_retries++;
		if (m_retries > 2)
		{
			if (m_event_listener)
				m_event_listener->OnWebserverUploadServiceStatus(UPLOAD_SERVICE_ERROR_COMM_TIMEOUT);
			/* After 5 retries, stop trying */
			return;
		}
	}

	OpTransferItem *found = 0;
	URL *server_url = NULL;
	if (OpStatus::IsMemoryError(g_transferManager->GetTransferItem(&found, m_sentral_server_uri.CStr())))
	{
		if (m_event_listener)
			m_event_listener->OnWebserverUploadServiceStatus(UPLOAD_SERVICE_ERROR_MEMORY);
	}

	if (found)
	{
		m_transferItem = found;
		server_url = m_transferItem->GetURL();
		if (server_url == NULL)
			return;

		server_url->SetHTTP_Method(HTTP_METHOD_POST);

		if (	OpStatus::IsError(server_url->SetHTTP_Data(m_xml_data.CStr(), TRUE)) ||
				OpStatus::IsError(server_url->SetAttribute(URL::KHTTP_ContentType, "application/xml")))
		{
			m_transferItem->SetTransferListener(NULL);
			g_transferManager->ReleaseTransferItem(m_transferItem);
			m_transferItem = NULL;
			if (m_event_listener)
				m_event_listener->OnWebserverUploadServiceStatus(UPLOAD_SERVICE_ERROR_MEMORY);
			return;
		}
		
		// Initiate the download of the document pointed to by the URL, and forget about it.
		// The OnProgress function will deal with successful downloads and check whether a user
		// notification is required.
		m_transferItem->SetTransferListener(this);

		URL tmp;
		server_url->Load(g_main_message_handler, tmp, TRUE, FALSE); /* Error handling? */
		m_firstTimeout = FALSE;
		m_timer.Start(30000); /* We set a timeout for this upload */
		m_in_progress = TRUE;
		
	}
}

#endif // WEBSERVER_SUPPORT && WEB_UPLOAD_SERVICE_LIST
