/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marius Blomli, Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/statusxmldownloader.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/autoupdate/updatablesetting.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/upload/upload.h"
#include "modules/util/adt/bytebuffer.h"

#ifdef _DEBUG
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#endif

#include "modules/util/opguid.h"

#define AUTOUPDATE_XML_VERSION UNI_L("1.0")
#define AUTOUPDATE_RESPONSE_FILE_NAME UNI_L("autoupdate_response.xml")

OpVector<StatusXMLDownloader> StatusXMLDownloaderHolder::downloader_list;

OP_STATUS StatusXMLDownloaderHolder::AddDownloader(StatusXMLDownloader* downloader)
{
	OP_ASSERT(downloader);
	return downloader_list.Add(downloader);
}

OP_STATUS StatusXMLDownloaderHolder::RemoveDownloader(StatusXMLDownloader* downloader)
{
	OP_ASSERT(downloader);
	return downloader_list.RemoveByItem(downloader);
}

void StatusXMLDownloaderHolder::ReplaceRedirectedFileURLs(const OpString& url, const OpString redirected_url)
{
	for (UINT32 i=0; i<downloader_list.GetCount(); i++)
	{
		StatusXMLDownloader* downloader = downloader_list.Get(i);
		OP_ASSERT(downloader);
		OpStatus::Ignore(downloader->ReplaceRedirectedFileURL(url, redirected_url));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::Init(CheckType check_type, StatusXMLDownloaderListener* listener)
{
	OP_ASSERT(listener);
	OP_ASSERT(check_type != CheckTypeInvalid);

	m_listener = listener;
	m_check_type = check_type;

	RETURN_IF_ERROR(GenerateResponseFilename());
	RETURN_IF_ERROR(StatusXMLDownloaderHolder::AddDownloader(this));

	return OpStatus::OK;
}

OP_STATUS StatusXMLDownloader::GenerateResponseFilename()
{
	switch (m_check_type)
	{
		case (CheckTypeUpdate):
		{
			m_response_file_folder = OPFILE_HOME_FOLDER;
			return m_response_file_name.Set(AUTOUPDATE_RESPONSE_FILE_NAME);
		}
		case (CheckTypeOther):
		{
			m_response_file_folder = OPFILE_TEMP_FOLDER;
			OpGuid guid;
			OpString8 temp;
			// GUID(37) + ".xml\0" */
			RETURN_OOM_IF_NULL(temp.Reserve(42));

			RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));
			OpGUIDManager::ToString(guid, temp.DataPtr(), temp.Capacity());
			RETURN_IF_ERROR(temp.Append(".xml"));
			return m_response_file_name.Set(temp);
		}
		default:
		{
			OP_ASSERT(!"Unknown check type!");
			return OpStatus::ERR;
		}
	}
}

OP_STATUS StatusXMLDownloader::DeleteResponseFile()
{
#ifdef _DEBUG
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::SaveAutoUpdateXML))
		return OpStatus::OK;
#endif // _DEBUG

	// Don't delete the autoupdate_response.xml file since it might come in handy when it comes to reparsing the 
	// resposne XML after a crash.
	if (CheckTypeUpdate == m_check_type)
		return OpStatus::OK;

	OP_ASSERT(m_response_file_name.HasContent());
	OpFile response_file;
	RETURN_IF_ERROR(response_file.Construct(m_response_file_name, m_response_file_folder));
	RETURN_IF_ERROR(response_file.Delete(FALSE));
	return OpStatus::OK;
}

StatusXMLDownloader::DownloadStatus StatusXMLDownloader::ParseDownloadedXML()
{
	OP_ASSERT(m_response_file_name.HasContent());

	OpFile file;
	
	if (OpStatus::IsError(file.Construct(m_response_file_name.CStr(), m_response_file_folder)))
		return OUT_OF_MEMORY;
	
	if (OpStatus::IsError(file.Open(OPFILE_READ)))
		return LOAD_FAILED;

	// Define some element names for later use.
	const XMLCompleteName versioncheck_elm(UNI_L("urn:OperaAutoupdate"), UNI_L("oau"), UNI_L("versioncheck"));
	const XMLExpandedName file_elm(UNI_L("file"));
	const XMLExpandedName setting_elm(UNI_L("setting"));
	const XMLExpandedName version_att(UNI_L("version"));
	const XMLExpandedName resources_elm(UNI_L("resources"));
	
	// Parse downloaded xml.
	XMLFragment fragment;
	if (OpStatus::IsError(fragment.Parse(&file)))
	{
		file.Close();
		return PARSE_ERROR;
	}
	
	file.Close();
	
	// Enter the first versioncheck element. If the response contain more than one, ignore the rest. If the response doesen't contain any, fail.
	if (!fragment.EnterElement(versioncheck_elm)) 
	{
		return WRONG_XML;
	}
/*
	const uni_char* version_att_str = fragment.GetAttribute(version_att);
	if(!version_att_str || uni_strcmp(version_att_str, AUTOUPDATE_XML_VERSION) != 0)
	{
		return WRONG_XML;
	}
*/

	while(fragment.EnterAnyElement())
	{
		XMLCompleteName element_name;
		element_name = fragment.GetElementName();
		
		if(element_name == resources_elm)
		{
			OpStatus::Ignore(ParseResourcesElement(fragment));
		}
		else if(element_name == file_elm || element_name == setting_elm)
		{
			const uni_char* resource_type = element_name.GetLocalPart();
			OP_ASSERT(resource_type);
			OpString resource_type_string;
			if (OpStatus::IsError(resource_type_string.Set(resource_type)))
				return PARSE_ERROR;

			OpStatus::Ignore(ParseResourceElement(resource_type_string, fragment));
		}
		fragment.LeaveElement();
	}
	
	fragment.LeaveElement();
	return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::ParseResourcesElement(XMLFragment& fragment)
{
	const XMLCompleteName downloadurl_elm(UNI_L("downloadurl"));
	if(fragment.EnterElement(downloadurl_elm))
	{
		const uni_char* text = fragment.GetText();
		RETURN_IF_ERROR(m_download_url.Set(text));
		fragment.LeaveElement();
		return OpStatus::OK;
	}
	return OpStatus::OK;
}

OP_STATUS StatusXMLDownloader::ParseResourceElement(const OpStringC& resource_type, XMLFragment& fragment)
{
    OP_NEW_DBG("StatusXMLDownloader::ParseFileElement", "Autoupdate");
	
	UpdatableResource* resource = NULL;
	OpAutoPtr<UpdatableResource> keeper;
	RETURN_IF_ERROR(UpdatableResourceFactory::GetResourceFromXML(resource_type, fragment, &resource));
	keeper = resource;
	OP_ASSERT(resource);
	OP_DBG(("Found resource"));

	RETURN_IF_ERROR(m_resource_list.Add(keeper.get()));
	keeper.release();
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::GetSubnodeText(XMLFragment* frag, const uni_char* node_name, OpString& text)
{
	const XMLExpandedName elm(node_name);
	if (frag->EnterElement(elm))
	{
		const uni_char* ret = frag->GetText();
		frag->CloseElement();
		return text.Set(ret);
	}
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

StatusXMLDownloader::StatusXMLDownloader(): 
	m_check_type(CheckTypeInvalid),
	m_listener(NULL),
	m_transferItem(NULL),
	m_resource_iterator(0),
	m_status(READY)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

StatusXMLDownloader::~StatusXMLDownloader()
{
	StatusXMLDownloaderHolder::RemoveDownloader(this);
	if (m_transferItem && g_transferManager)
	{
		g_transferManager->ReleaseTransferItem(m_transferItem);
	}
	m_resource_list.DeleteAll();
	// Not much that we can do anyway
	OpStatus::Ignore(DeleteResponseFile());
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::StartXMLRequest(const OpString& post_address, const OpString8& xml)
{
	OP_ASSERT(m_response_file_name.HasContent());

	if (m_transferItem)
	{
		// An instance of this class can only hold one transferitem, 
		// so it will refuse to start a second download until the first 
		// transferitem is released.
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	
	// Delete the old response file
	OpFile file;
	if (OpStatus::IsSuccess(file.Construct(m_response_file_name.CStr(), m_response_file_folder)))
		OpStatus::Ignore(file.Delete(FALSE));

	// Clear list of downloaded information
	m_resource_list.Clear();
	m_resource_iterator = 0;
	// Initiate transfer and listen in.
	RETURN_IF_ERROR(g_transferManager->GetNewTransferItem(&m_transferItem, post_address.CStr()));
	if (!m_transferItem)
	{
		m_status = NO_TRANSFERITEM;
		return OpStatus::ERR;
	}

	URL* url = m_transferItem->GetURL();
	if (!url)
	{
		m_status = NO_URL;
		return OpStatus::ERR;
	}
	
	url->SetHTTP_Method(HTTP_METHOD_POST);
	RETURN_IF_ERROR(url->SetHTTP_Data(xml.CStr(), TRUE));
	RETURN_IF_ERROR(url->SetHTTP_ContentType("application/x-www-form-urlencoded"));

	// Turn off UI interaction if the certificate is invalid.
	// This means that invalid certificates will not show a UI dialog but fail instead.
	RETURN_IF_ERROR(url->SetAttribute(URL::KBlockUserInteraction, TRUE));
	
	m_transferItem->SetTransferListener(this);
	URL tmp;
	if (url->Load(g_main_message_handler, tmp, TRUE, FALSE) != COMM_LOADING)
	{
		m_status = LOAD_FAILED;
		return OpStatus::ERR;
	}
	m_status = INPROGRESS;
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::StopRequest()
{
	if(m_transferItem)
	{
		m_transferItem->Stop();
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;		
	}
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

UpdatableResource* StatusXMLDownloader::GetNextResource()
{
	UpdatableResource* ret = NULL;
	if (m_resource_iterator < m_resource_list.GetCount())
	{
		ret = m_resource_list.Get(m_resource_iterator);
		m_resource_iterator++;
	}
	else
	{
		m_resource_iterator = 0;
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::RemoveResource(UpdatableResource* res)
{
	INT32 indx = m_resource_list.Find(res);

	if(indx < 0)
	{
		return OpStatus::ERR;
	}

	if(m_resource_iterator > 0)
	{
		if((UINT32)indx < m_resource_iterator)
		{
			m_resource_iterator--;
		}
	}

	m_resource_list.Remove(indx);
	return OpStatus::OK;
}

UINT32 StatusXMLDownloader::GetResourceCount()
{
	return m_resource_list.GetCount();
}

////////////////////////////////////////////////////////////////////////////////////////////////

void StatusXMLDownloader::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	OP_ASSERT(transferItem == m_transferItem);

	OP_ASSERT(m_response_file_name.HasContent());

	switch (status)
	{
		case TRANSFER_ABORTED:
		{
			m_status = DOWNLOAD_ABORTED;
			break;
		}
		case TRANSFER_FAILED:
		{
			m_status = DOWNLOAD_FAILED;
			break;
		}
		case TRANSFER_DONE:
		{
			OpString filename;
			OpString tmp_storage;
			filename.Set(g_folder_manager->GetFolderPathIgnoreErrors(m_response_file_folder, tmp_storage));
			filename.Append(m_response_file_name.CStr());
			
			m_transferItem->GetURL()->SaveAsFile(filename);

#ifdef _DEBUG
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::SaveAutoUpdateXML))
			{
				// Open xml response in a tab
				OpString url_string;
				OpString resolved;
				BOOL successful;
				url_string.Set(UNI_L("file:"));
				url_string.Append(filename);
				TRAPD(err, successful = g_url_api->ResolveUrlNameL(url_string, resolved));
				
				if (successful && resolved.Find("file://") == 0)
				{
					URL url = g_url_api->GetURL(resolved.CStr());
					URL ref_url = URL();
					
					BOOL3 open_in_new_window = YES;
					BOOL3 open_in_background = YES;

					Window* feedback_window;
					feedback_window = windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background);
					feedback_window->OpenURL(url, DocumentReferrer(ref_url), TRUE, FALSE, -1, TRUE);
				}
			}
#endif
			
			m_status = ParseDownloadedXML();
			break;
		}
		case TRANSFER_PROGRESS:
		default:
		{
			break;
		}
	}
	if (m_status == INPROGRESS)
		return; // Transfer not complete, wait for a useful callback.
	else
	{
		g_transferManager->ReleaseTransferItem(m_transferItem);
		m_transferItem = NULL;
		if (m_status == SUCCESS)
			m_listener->StatusXMLDownloaded(this);
		else
			m_listener->StatusXMLDownloadFailed(this, m_status);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS StatusXMLDownloader::ReplaceRedirectedFileURL(const OpString& url, const OpString& redirected_url)
{
	OP_ASSERT(m_response_file_name.HasContent());

	OpFile file;
	
	RETURN_IF_ERROR(file.Construct(m_response_file_name.CStr(), m_response_file_folder));
	
	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	
	// Define some element names for later use.
	const XMLCompleteName versioncheck_elm(UNI_L("urn:OperaAutoupdate"), UNI_L("oau"), UNI_L("versioncheck"));
	const XMLExpandedName file_elm(UNI_L("file"));
	
	// Parse downloaded xml.
	XMLFragment fragment;
	if (OpStatus::IsError(fragment.Parse(&file)))
	{
		file.Close();
		return OpStatus::ERR;
	}
	
	file.Close();
	
	// Enter the first versioncheck element. If the response contain more than one, ignore the rest. If the response doesen't contain any, fail.
	if (!fragment.EnterElement(versioncheck_elm)) 
	{
		return WRONG_XML;
	}
	
	BOOL replaced = FALSE;

	while(fragment.EnterAnyElement() && !replaced)
	{
		XMLCompleteName element_name;
		element_name = fragment.GetElementName();
		
		if(element_name == file_elm)
		{
			const XMLCompleteName url_elm(UNI_L("url"));
			
			while(fragment.EnterAnyElement() && !replaced)
			{
				XMLCompleteName element_name;
				element_name = fragment.GetElementName();
				
				if(element_name == url_elm)
				{
					const uni_char* text = fragment.GetText();
					if(url.Compare(text) == 0)
					{
						fragment.AddText(redirected_url.CStr());
						replaced = TRUE;
					}
				}
				fragment.LeaveElement();
			}
		}
		fragment.LeaveElement();
	}
	
	fragment.LeaveElement();

	if(replaced)
	{
		RETURN_IF_ERROR(file.Delete(FALSE));
		RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
		
		ByteBuffer buffer;
		unsigned int bytes = 0;
		UINT32 chunk;
		OpString8 xml;
		
		RETURN_IF_ERROR(fragment.GetEncodedXML(buffer, TRUE, "utf-8", FALSE));
		
		for(chunk = 0; chunk < buffer.GetChunkCount(); chunk++)
		{
			char *chunk_ptr = buffer.GetChunk(chunk, &bytes);
			if(chunk_ptr)
			{
				RETURN_IF_ERROR(xml.Append(chunk_ptr, bytes));
			}
		}
		
		RETURN_IF_ERROR(file.Write(xml.CStr(), xml.Length()));

		RETURN_IF_ERROR(file.Close());
	}
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTO_UPDATE_SUPPORT
