/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "FavIconManager.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/desktop_util/search/search_net.h" // SearchTemplate
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/widgets/DocumentView.h"
#include "adjunct/quick/Application.h"

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"
#include "adjunct/desktop_util/image_utils/iconutils.h"
#include "adjunct/desktop_util/resources/ResourceSetup.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/cache/cache_int.h" // FileName_Store
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/util/opfile/opfile.h"
#include "modules/img/imagedump.h"
#include "modules/skin/OpSkinManager.h"

#define SUPPORT_REVERSE_FAVICON_MATCHING

static OpAutoStringHashTable<FileImage::ImageEntry> m_image_table;
Image FileImage::m_empty_image;
OpString FileImage::m_empty_string;



FileImage::FileImage( const uni_char* filename, const uni_char* url)
{
	m_url.Set(url);

	m_entry = NULL;
	ReloadImage( filename );
}


FileImage::~FileImage()
{
	if( m_entry )
	{
		m_entry->num_users--;
		if( m_entry->num_users == 0 )
		{
			ImageEntry* tmp = 0;
			m_image_table.Remove(m_entry->filename.CStr(), &tmp);
			if( tmp == m_entry )
			{
				m_image_table.Delete(m_entry);
			}
		}
	}
}


void FileImage::ReloadImage( const uni_char* filename )
{
	if( filename )
	{
		if( m_entry )
		{
			m_entry->num_users--;
			if( m_entry->num_users == 0 )
			{
				ImageEntry* tmp = 0;
				m_image_table.Remove(m_entry->filename.CStr(), &tmp);
				if( tmp == m_entry )
				{
					m_image_table.Delete(m_entry);
				}
			}
		}

		m_entry = NULL;
		m_image_table.GetData( filename, &m_entry );

		if( !m_entry )
		{
			m_entry = OP_NEW(ImageEntry, ());
			if (m_entry)
			{
				if (OpStatus::IsSuccess(m_entry->filename.Set(filename)))
				{
					m_entry->m_image_content_provider.LoadFromFile( m_entry->filename.CStr(), m_entry->image );
					m_entry->num_users = 1;

					// Must use 'm_entry->filename'. 'filename' argument can be deleted by caller.
					m_image_table.Add( m_entry->filename.CStr(), m_entry );
				}
			}
		}
		else
		{
			m_entry->num_users++;
		}
	}
}



// static
BOOL FileImage::IsICOFileFormat(const uni_char* filename)
{
	if (filename == NULL)
	{
		return FALSE;
	}

	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(filename), FALSE);
	RETURN_VALUE_IF_ERROR(file.Open(OPFILE_READ), FALSE);

	OpFileLength len;
	RETURN_VALUE_IF_ERROR(file.GetFileLength(len), FALSE);

	if( len < 4 )
		return FALSE;

	UINT8 data[4];
	OpFileLength bytes_read = 0;
	RETURN_VALUE_IF_ERROR(file.Read(data, 4, &bytes_read), FALSE);

	return CheckICOHeader(data);
}

// static
BOOL FileImage::CheckICOHeader(UINT8* data)
{
	return memcmp(data, "\0\0\1\0", 4) == 0 ? TRUE : FALSE;
}


FavIconLoader::FavIconLoader()
	:m_id(0)
	,m_finished(FALSE)
{
}

FavIconLoader::~FavIconLoader()
{
	if( g_main_message_handler )
	{
		if( IsLoading() )
		{
			m_url.StopLoading(g_main_message_handler);
		}
		g_main_message_handler->UnsetCallBacks(this);
	}
}


BOOL FavIconLoader::Load(const OpString& document_url, const OpString& favicon_url, URL_CONTEXT_ID id)
{
	m_document_url.Set(document_url);
	if( m_document_url.IsEmpty() )
		return FALSE;

	m_favicon_url.Set(favicon_url);
	if( m_favicon_url.IsEmpty() )
		return FALSE;

	m_id = id;

	m_url = urlManager->GetURL(m_favicon_url.CStr(), m_id);

	m_url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_GET);
	m_url.SetAttribute(URL::KDisableProcessCookies,TRUE);
	m_url.SetAttribute(URL::KBlockUserInteraction, TRUE);
	m_url.SetAttribute(URL::KDisableProcessCookies, TRUE);

	OpMessage msglist[4];
	msglist[0] = MSG_URL_DATA_LOADED;
	msglist[1] = MSG_URL_INLINE_LOADING;
	msglist[2] = MSG_URL_LOADING_FAILED;
	msglist[3] = MSG_URL_MOVED;
	g_main_message_handler->SetCallBackList(this, 0, msglist, 4);
	m_url.LoadDocument(g_main_message_handler, URL(), URL_Reload_Conditional);

	return TRUE;
}



void FavIconLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if( m_finished )
		return;

	if (msg == MSG_URL_DATA_LOADED)
	{
		URLStatus status = m_url.Status(TRUE);

		if( status == URL_LOADED )
		{
			for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
			{
				m_listeners.GetNext(iterator)->OnFavIconLoaded(m_document_url.CStr(), m_favicon_url.CStr(), m_id);
			}
			
			m_finished = TRUE;
		}
	}
	else if(msg == MSG_URL_INLINE_LOADING)
	{
	}
	else if(msg == MSG_URL_LOADING_FAILED)
	{
	}
	else if(msg == MSG_URL_MOVED)
	{
	}

	if( m_finished )
	{
		for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		{
			m_listeners.GetNext(iterator)->OnFavIconFinished();
		}
	}
}


BOOL FavIconLoader::IsLoading()
{
	return m_url.Status(TRUE) == URL_LOADING;
}



FavIconManager::FavIconManager()
	:m_disable_get_set(FALSE)
	,m_icon_table(0)
	,m_persistent_list(0)
	,m_bookmark_list(0)
	,m_timer(0)
	,m_cache_id(0)
	,m_cache(0)
	,m_cache_init(FALSE)
{
	const char* text = getenv("OPERA_TEST_NO_FAVICON_MANAGER");
	if( text && !strcmp(text, "1") )
		m_disable_get_set = TRUE;
}



FavIconManager::~FavIconManager()
{
	OP_DELETE(m_icon_table);
	OP_DELETE(m_persistent_list);
	OP_DELETE(m_bookmark_list);
	OP_DELETE(m_timer);	
	m_loader_list.DeleteAll();
	OP_DELETE(m_cache);
}


OP_STATUS FavIconManager::Init()
{
	// Do nothing now - OpBootManager will call InitSpecialIcons when it
	// gets name of user's region. This may be delayed for a few seconds
	// if Opera needs to contact the autoupdate server to get IP-based
	// country code (DSK-351304).

	return OpStatus::OK;
}

OP_STATUS FavIconManager::InitSpecialIcons()
{
	BOOL force_update = FALSE;

	OpFile file;
	file.Construct(UNI_L("persistent.txt"), OPFILE_ICON_FOLDER);
	BOOL exists = FALSE;
	if( OpStatus::IsError(file.Exists(exists)) || !exists )
	{
		// Remove broken index file (contains icon files in page url entry)
		file.Construct(UNI_L("redir.opera.com.idx"), OPFILE_ICON_FOLDER);
		file.Delete(FALSE);

		force_update = TRUE;
	}

	Application::RunType run_type = g_application->DetermineFirstRunType();
	if( force_update ||
		run_type == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER ||
		run_type == Application::RUNTYPE_FIRST ||
		run_type == Application::RUNTYPE_FIRSTCLEAN ||
		g_region_info->m_changed )
	{
		LoadSpecialIcons();
	}
	return OpStatus::OK; 
}


void FavIconManager::LoadBookmarkIcon(const uni_char* document_url, const uni_char* icon_url)
{
	if( document_url )
	{
		if( icon_url )
		{
			OpString s1, s2;
			RETURN_VOID_IF_ERROR(s1.Set(document_url));
			RETURN_VOID_IF_ERROR(s2.Set(icon_url));
			GetInternal(s1, s2);
		}
		else
		{
			OpVector<DocAndFavIcon>* list = LoadBookmarkList();
			if( list )
			{
				for(UINT32 i=0; i<list->GetCount(); i++)
				{
					if( list->Get(i)->url.Compare(document_url) == 0 )
					{
						GetInternal(list->Get(i)->url, list->Get(i)->icon_url);
						break;
					}
				}
			}
		}
	}
}


void FavIconManager::LoadSearchIcon(const OpStringC& guid)
{
	LoadSearchIcon(g_searchEngineManager->GetByUniqueGUID(guid));
}


void FavIconManager::LoadSearchIcon(SearchTemplate* search)
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	// Only entries with a key set show in the UI
	if( search && search->GetKey().HasContent() )
	{
		OpString doc_url;
		OpString icon_url;

		doc_url.Set(search->GetUrl());
		icon_url.Set(search->GetAbsoluteIconAddress());

		if( doc_url.HasContent() )
		{
			if( icon_url.IsEmpty() )
			{
				OpString8 hostname;
				URL	url = urlManager->GetURL(search->GetUrl());
				TRAPD(err,url.GetAttributeL(URL::KHostName,hostname));
				if( !hostname.IsEmpty() )
				{
					icon_url.Set("http://");
					icon_url.Append(hostname);
					icon_url.Append("/favicon.ico");
				}
			}
			
			if( icon_url.HasContent() )
			{
				GetInternal( doc_url, icon_url );
			}
		}
	}
#endif
}


OpVector<FavIconManager::DocAndFavIcon>* FavIconManager::LoadBookmarkList()
{
	if( !m_bookmark_list )
	{
		m_bookmark_list = OP_NEW(OpAutoVector<DocAndFavIcon>, ());
		if(!m_bookmark_list )
			return 0;
		
		OpFile file;

		if ( OpStatus::IsSuccess(ResourceSetup::GetDefaultPrefsFile(DESKTOP_RES_PACKAGE_BOOKMARK, file)) &&
			 OpStatus::IsSuccess(file.Open(OPFILE_READ)) )
		{
			BOOL in_url_segment = TRUE;
			OpString8 line, icon_url, doc_url;
			
			BOOL end_of_file = FALSE;
			for( INT32 i=0; i<1000 && !end_of_file; i++ )
			{
				if( file.Eof() )
				{
					if( !in_url_segment )
					{
						break;
					}
					end_of_file = TRUE;
				}
				else
				{
					line.Empty();
					OP_STATUS s = file.ReadLine(line);
					if( OpStatus::IsError(s) )
					{
						break;
					}
					line.Strip();
				}
				
				if( end_of_file || line.Compare("#URL") == 0 || line.Compare("#FOLDER") == 0)
				{
					if( in_url_segment )
					{
						if( doc_url.HasContent() )
						{
							if( icon_url.IsEmpty() )
							{
								// Request http://<hostname>/favicon.ico since we have no other information
								
								OpString8 hostname;
								URL	url = urlManager->GetURL(doc_url);
								url.GetAttribute(URL::KHostName,hostname);
								if( !hostname.IsEmpty() )
								{
									icon_url.Set("http://");
									icon_url.Append(hostname);
									icon_url.Append("/favicon.ico");
								}
							}
							
							if( icon_url.HasContent() )
							{
								DocAndFavIcon* item = OP_NEW(DocAndFavIcon, ());
								if( item && (OpStatus::IsError(item->url.Set(doc_url)) || 
											 OpStatus::IsError(item->icon_url.Set(icon_url)) ||
											 OpStatus::IsError(m_bookmark_list->Add(item))) )
								{
									OP_DELETE(item);
								}
							}
						}
					}
					
					in_url_segment = (line.Compare("#URL") == 0);
					doc_url.Empty();
					icon_url.Empty();
				}
				else if( in_url_segment )
				{
					GetIniFileValue(line, "ICONFILE=", icon_url );
					GetIniFileValue(line, "URL=", doc_url );
				}
			}
		}
	}

	return m_bookmark_list;
}



void FavIconManager::LoadSpecialIcons()
{
	if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
	{
		return;
	}

	UINT32 i;
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	for(i=0; i<g_searchEngineManager->GetSearchEnginesCount(); i++)
	{
		LoadSearchIcon(g_searchEngineManager->GetSearchEngine(i));
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES


	// Load bookmark icons. They are generally different from search urls.
	// We load from bookmark file that comes with package.
	
	OpVector<DocAndFavIcon>* bookmark_list = LoadBookmarkList();
	if( bookmark_list )
	{
		for( i=0; i<bookmark_list->GetCount(); i++)
		{
			GetInternal( bookmark_list->Get(i)->url, bookmark_list->Get(i)->icon_url);
		}
	}

	// load webmail icons
	for(i=0; i < WebmailManager::GetInstance()->GetCount(); i++)
	{
		unsigned int id = WebmailManager::GetInstance()->GetIdByIndex(i);
		OpString doc_url, icon_url;
		
		if(OpStatus::IsError(WebmailManager::GetInstance()->GetURL(id,doc_url))||
		   OpStatus::IsError(WebmailManager::GetInstance()->GetFaviconURL(id,icon_url)))
			continue;

		if( doc_url.HasContent() )
		{
			if( icon_url.IsEmpty() )
			{
				OpString8 hostname;
				URL	url = urlManager->GetURL(doc_url);
				TRAPD(err,url.GetAttributeL(URL::KHostName,hostname));
				if( !hostname.IsEmpty() )
				{
					icon_url.Set("http://");
					icon_url.Append(hostname);
					icon_url.Append("/favicon.ico");
				}
			}

			if( icon_url.HasContent() )
			{
				GetInternal(doc_url, icon_url);
			}
		}	
	}
}


void FavIconManager::Add(const uni_char* document_url, const unsigned char* icon_data, UINT32 len)
{
	if (m_disable_get_set)
		return;

	if (!icon_data || !document_url || !*document_url || !len)
		return;

	OpString icon_filename;
	GetImageFilename(document_url, icon_filename, FALSE, FALSE);
	if (icon_filename.IsEmpty())
		RETURN_VOID_IF_ERROR(MakeImageFilename(document_url, ".png", icon_filename));

	// Ensure icon is in PNG format and of correct size
	RETURN_VOID_IF_ERROR(ReplaceExtensionToPNG(icon_filename));

	StreamBuffer<UINT8> src, buffer;
	RETURN_VOID_IF_ERROR(src.Append(icon_data, len));
	RETURN_VOID_IF_ERROR(BufferToPNGBuffer(src, buffer));
	RETURN_VOID_IF_ERROR(BufferToFile(buffer,icon_filename));

	BOOL dummy;
	AppendToIndex(document_url, icon_filename, dummy);

	// We need full path
	OpFile file;
	RETURN_VOID_IF_ERROR(file.Construct(icon_filename, OPFILE_ICON_FOLDER));
	RETURN_VOID_IF_ERROR(icon_filename.Set(file.GetFullPath()));

	UpdateCache(document_url);

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFavIconAdded(document_url, icon_filename.CStr());
	}
}


void FavIconManager::Add(const uni_char* document_url, const uni_char* icon_url, BOOL on_demand, URL_CONTEXT_ID id)
{
	if (m_disable_get_set)
		return;

	if (!icon_url || !*icon_url || !document_url || !*document_url)
		return;

	OpString icon_filename;
	RETURN_VOID_IF_ERROR(MakeImageFilename(icon_url, 0, icon_filename));

	URL url = urlManager->GetURL(icon_url, id);
	BOOL is_image = imgManager->IsImage( URLContentType(url.GetAttribute(URL::KContentType,TRUE)) );
	if (!is_image)
	{
		// The test above does not cover the ico format. We examine the content itself
		URL_DataDescriptor* desc = url.GetDescriptor(NULL, TRUE);
		if (desc)
		{
			UINT8* header = (UINT8*)desc->GetBuffer();
			unsigned headersize = desc->GetBufSize();
			if (headersize > 3)
			{
				is_image = FileImage::CheckICOHeader(header);
			}
			OP_DELETE(desc);
		}
	}

	// Test if we have an image. If not, we can not do anything
	if (!is_image)
		return;

	BOOL file_on_disk_changed = FALSE;
	BOOL icon_is_in_index = FALSE;

	// 1 Convert incoming URL to a PNG buffer in memory. Scaled to 16x16
	// 2 Read file from disk to memory
	// 3 If buffers differ write incoming buffer to disk as PNG file
	// 4 Update index with PNG file name

	StreamBuffer<UINT8> icon_buffer;
	RETURN_VOID_IF_ERROR(UrlToPNGBuffer(url, icon_buffer));
	if (icon_buffer.GetFilled() > 0)
	{
		// Read existing file (if any) into buffer. Try PNG extension if original fails
		StreamBuffer<UINT8> cached_buffer;
		RETURN_VOID_IF_ERROR(FileToBuffer(icon_filename, cached_buffer));
		RETURN_VOID_IF_ERROR(ReplaceExtensionToPNG(icon_filename));
		if (cached_buffer.GetFilled() == 0)
			RETURN_VOID_IF_ERROR(FileToBuffer(icon_filename, cached_buffer));

		// Write to disk if the new buffer differs from saved
		if (icon_buffer.GetFilled() != cached_buffer.GetFilled() ||
			memcmp(icon_buffer.GetData(), cached_buffer.GetData(), icon_buffer.GetFilled()))
		{
			RETURN_VOID_IF_ERROR(BufferToFile(icon_buffer, icon_filename));
		}

		icon_is_in_index = AppendToIndex(document_url, icon_filename, file_on_disk_changed);
	}

	if (on_demand)
		AddPersistentItem(document_url, icon_filename.CStr());


	// We only have to update the cache if the file on disk is new or content has changed. 
	// That is signalled by 'file_on_disk_changed'. If a new icon in not a valid image we 
	// keep the old version if any. It is important that we do not call the code below 
	// more than we have to as we have to iterate through the entire icon list to determine 
	// what urls use the icon (does not scale well). FavIconManager::Add() gets called
	// every time a page has completed loading and has a favicon associated with it so startup
	// is a critical point.
	//
	// Calling UpdateCache() must trigger calling OnFavIconAdded() as well. Otherwise 
	// we get problems as described in bug #DSK-229231 [espen 2009-08-18]

	if (icon_is_in_index && (file_on_disk_changed || on_demand))
	{
		// We need full path
		OpFile file;
		RETURN_VOID_IF_ERROR(file.Construct(icon_filename, OPFILE_ICON_FOLDER));
		RETURN_VOID_IF_ERROR(icon_filename.Set(file.GetFullPath()));

		UpdateCache(document_url);

		// Save urls, if any, that use the new icon
		OpAutoVector<OpString> list;
		// Save entries to remove, this is to avoid deleting entries while iterating them
		OpAutoVector<FileImage> removal_list;

		OpString* s = OP_NEW(OpString, ());
		if (s && (OpStatus::IsError(s->Set(document_url)) || OpStatus::IsError(list.Add(s))))
			OP_DELETE(s);

		// Remove cached entries. Note that m_icon_table can be NULL
		if (m_icon_table)
		{
			OpHashIterator* it = m_icon_table->GetIterator();

			OP_STATUS rc = it->First();
			while (OpStatus::IsSuccess(rc))
			{
				FileImage* image = (FileImage*)it->GetData();
				if (image->GetFilename().Compare(icon_filename) == 0)
				{
					if (uni_strcmp(document_url, image->GetURL()) != 0)
					{
						s = OP_NEW(OpString, ());
						if (s && (OpStatus::IsError(s->Set(image->GetURL())) || OpStatus::IsError(list.Add(s))))
							OP_DELETE(s);
					}

					removal_list.Add(image);
				}

				rc = it->Next();
			}
			OP_DELETE(it);
		}

		for(UINT32 i=0; i<removal_list.GetCount(); i++)
		{
			FileImage* fi = 0;
			m_icon_table->Remove(removal_list.Get(i)->GetURL(), &fi);
		}

		for(UINT32 i=0; i<list.GetCount(); i++)
		{
			for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
			{
				m_listeners.GetNext(iterator)->OnFavIconAdded(list.Get(i)->CStr(), icon_filename.CStr());
			}
		}
	}
}



OP_STATUS FavIconManager::AddPersistentItem(const uni_char* document_url, const uni_char* escaped_icon_filename)
{
	UINT32 i;

	OpString8 document_url_8;
	RETURN_IF_ERROR(document_url_8.Set(document_url));

	OpString8 icon_filename_8;
	RETURN_IF_ERROR(icon_filename_8.Set(escaped_icon_filename));

	OpVector<OpString8>* list = GetPersistentList();
	if (!list)
		return OpStatus::ERR;

	// Order: <document url, icon file name, timestamp> each on separate line

	for (i=2; i<list->GetCount(); i+= 3)
	{
		if (list->Get(i-2)->Compare(document_url_8) == 0)
		{
			// Document matches. Test image name as well. Replace image if it differs
			if (list->Get(i-1)->Compare(icon_filename_8))
			{
				// Delete the three entries and append to end of list
				list->Delete(i-2, 3);
				break;
			}
			return OpStatus::OK;
		}
	}

	OpString8 now;
	OpAutoPtr<OpString8> s1(OP_NEW(OpString8,()));
	OpAutoPtr<OpString8> s2(OP_NEW(OpString8,()));
	OpAutoPtr<OpString8> s3(OP_NEW(OpString8,()));

	if (!s1.get() || !s2.get() || !s3.get() ||
		OpStatus::IsError(now.AppendFormat("%u", (UINT32)(g_op_time_info->GetTimeUTC()/1000.0))) ||
		OpStatus::IsError(s1->Set(document_url)) ||
		OpStatus::IsError(s2->Set(icon_filename_8)) ||
		OpStatus::IsError(s3->Set(now)))
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	if (OpStatus::IsError(list->Add(s1.get())))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	s1.release();
	if (OpStatus::IsError(list->Add(s2.get())))
	{
		list->Delete(list->GetCount()-1); // release s1
		return OpStatus::ERR_NO_MEMORY;
	}
	s2.release();
	if (OpStatus::IsError(list->Add(s3.get())))
	{
		list->Delete(list->GetCount()-1); // release s2
		list->Delete(list->GetCount()-1); // release s1
		return OpStatus::ERR_NO_MEMORY;
	}
	s3.release();

	// Write to disk
	OpFile file;
	RETURN_IF_ERROR(file.Construct(UNI_L("persistent.txt"), OPFILE_ICON_FOLDER));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
	for (i=0; i< list->GetCount(); i++)
	{
		RETURN_IF_ERROR(file.Write(list->Get(i)->CStr(), list->Get(i)->Length()));
		RETURN_IF_ERROR(file.Write("\n", 1));
	}

	return OpStatus::OK;
}


OpVector<OpString8>* FavIconManager::GetPersistentList()
{
	if (!m_persistent_list)
	{
	 	m_persistent_list = OP_NEW(OpAutoVector<OpString8>, ());
		if (!m_persistent_list )
			return 0;

		OpFile file;
		if (OpStatus::IsSuccess(file.Construct(UNI_L("persistent.txt"), OPFILE_ICON_FOLDER)) &&
			OpStatus::IsSuccess(file.Open(OPFILE_READ)))
		{
			while (!file.Eof())
			{
				OpString8* s = OP_NEW(OpString8, ());
				if (s && (OpStatus::IsError(file.ReadLine(*s)) || s->IsEmpty() || OpStatus::IsError(m_persistent_list->Add(s))))
					OP_DELETE(s);
			}
		}
		file.Close();
	}

	return m_persistent_list;
}


BOOL FavIconManager::IsPersistentFile(const uni_char* filename)
{
	OpString8 filename_8;
	RETURN_VALUE_IF_ERROR(filename_8.Set(filename), FALSE);

	OpVector<OpString8>* list = GetPersistentList();
	if( list )
	{
		// Order: <document url, icon file name, timestamp> each on separate line
		for(UINT32 i=1; i<list->GetCount(); i+= 3)
		{
			if( list->Get(i)->Compare(filename_8) == 0 )
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}


void FavIconManager::RecreatePersistentIndex()
{
	OpVector<OpString8>* list = GetPersistentList();
	if (list)
	{
		// Order: <document url, icon file name, timestamp> each on separate line
		for (UINT32 i=1; i<list->GetCount(); i+=3)
		{
			OpString doc_url, icon_filename;
			doc_url.Set(list->Get(i-1)->CStr());
			icon_filename.Set(list->Get(i)->CStr());

			BOOL dummy;
			AppendToIndex(doc_url.CStr(), icon_filename, dummy);
		}
	}
}


BOOL FavIconManager::CreateIconTable()
{
	if (!m_icon_table)
	{
		m_icon_table = OP_NEW(OpAutoStringHashTable<FileImage>, ());
		if (!m_icon_table)
		{
			return FALSE;
		}
	}
	return TRUE;
}


Image FavIconManager::LookupImage( const uni_char* document_url, BOOL update_cache, BOOL allow_near_match)
{
	if( m_disable_get_set 
		|| g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon) == 0
		|| ( !document_url || !*document_url ))
	{
		return Image();
	}

	if (!CreateIconTable())
	{
		return Image();
	}

	FileImage* image = NULL;

	m_icon_table->GetData( document_url, &image );
	
	if (image && !update_cache)
	{
		return image->GetImage();
	}

	// Not in cache so load it.	
	for (int i=0; i<2; i++)
	{
		OpString filename;
		OpFile file;
		BOOL exists;
		GetImageFilename(document_url, filename, TRUE, allow_near_match);
		if (filename.IsEmpty() || OpStatus::IsError(file.Construct(filename)) || OpStatus::IsError(file.Exists(exists)) || !exists)
			break;

		if (image)
			image->ReloadImage(filename.CStr());
		else
		{
			image = OP_NEW(FileImage, (filename.CStr(), document_url));
			if (!image || image->GetImage().IsEmpty() || !image->GetURL())
			{
				OP_DELETE(image);
				image = NULL;
				break;
			}

			if (image->GetImage().Width() == 16 && image->GetImage().Height() == 16 && IsPNGFile(filename))
				break;

			if (i==0)
			{
				// This occurs for a favicon that was saved in the native format and without any
				// size limitation. Opera 11.50 and onwards converts all incoming icons to PNG/16x16

				GetImageFilename(document_url, filename, FALSE, allow_near_match);
				if (filename.IsEmpty())
					break;
				StreamBuffer<UINT8> png_buffer;
				if (OpStatus::IsError(FileToPNGBuffer(filename, png_buffer)))
					break;
				OpString png_filename;
				if (OpStatus::IsError(png_filename.Set(filename)) || OpStatus::IsError(ReplaceExtensionToPNG(png_filename)))
					break;
				if (OpStatus::IsError(BufferToFile(png_buffer, png_filename)))
					break;
				if (OpStatus::IsError(ReplaceIndex(document_url, filename, png_filename)))
					break;
				file.Delete(FALSE);
			}
		}
	}

	Image img;
	
	if (image)
	{
		m_icon_table->Add( image->GetURL(), image );
		img = image->GetImage();
	}
	else
	{
		const char* image_name = NULL; 
		switch(DocumentView::GetType(document_url))
		{
			case DocumentView::DOCUMENT_TYPE_SPEED_DIAL:
				 image_name = "Window Speed Dial Icon";
				 break;

			case DocumentView::DOCUMENT_TYPE_COLLECTION:
				 image_name = "Window Collection View Icon";
				 break;

			default:
				 return img;
		}

		OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(image_name);
		if (skin_elm)
			img = skin_elm->GetImage(0);
	}

	return img;
}



URL_CONTEXT_ID FavIconManager::GetURLContext(const OpString& favicon_url)
{
	URL_CONTEXT_ID id = 0;

	if( favicon_url.Find(UNI_L("redir.opera.com")) != KNotFound )
	{
		if( m_cache_init == FALSE )
		{
			m_cache_init = TRUE;

			if( m_cache_id == 0 )
			{
				m_cache_id = urlManager->GetNewContextID();
			}

			if( m_cache_id != 0 )
			{
				BOOL success = FALSE;

				if( !m_cache )
				{
					m_cache = OP_NEW(FileName_Store, (8191));
					if(m_cache)
					{
						m_cache->Construct();
						OpFileFolder folder;
						g_folder_manager->AddFolder(OPFILE_ICON_FOLDER, UNI_L("cache"),&folder);
						TRAPD(err,urlManager->AddContextL(m_cache_id, folder, folder, folder, folder, FALSE, -1));
						success = OpStatus::IsSuccess(err);
					}
				}

				if( !success )
				{
					OP_DELETE(m_cache);
					m_cache = 0;
					urlManager->RemoveContext(m_cache_id, FALSE);
					m_cache_id = 0;
				}
			}
		}
		id = m_cache_id;
	}

	return id;
}





BOOL FavIconManager::GetInternal(const OpString& document_url, const OpString& favicon_url)
{
	FavIconLoader* loader = OP_NEW(FavIconLoader, ());
	if( !loader )
	{
		return FALSE;
	}

	loader->AddListener(this);

	m_loader_list.Add(loader);

	return loader->Load(document_url, favicon_url, GetURLContext(favicon_url));
}




void FavIconManager::OnFavIconFinished()
{
	if( !m_timer )
	{
		if (!(m_timer = OP_NEW(OpTimer, ())))
			return;
	}
	m_timer->SetTimerListener( this );
	m_timer->Start(1000);
}


void FavIconManager::OnTimeOut(OpTimer* timer)
{
	for( UINT32 i=0; i<m_loader_list.GetCount(); )
	{
		if( !m_loader_list.Get(i)->IsLoading() )
		{
			m_loader_list.Delete(i);
		}
		else
		{
			i++;
		}
	}
}


OpVector<OpString8>* FavIconManager::ReadIndex(const uni_char* document_url)
{
	OpString filename;
	RETURN_VALUE_IF_ERROR(MakeIndexFilename(document_url, filename), 0);

	OpAutoVector<OpString8>* listdata = 0;
	m_index_cache.GetData(filename.CStr(), &listdata);
	if( listdata )
	{
		return listdata;
	}

	listdata = OP_NEW(OpAutoVector<OpString8>, ());
	if (!listdata)
		return 0;

	OpString* key = OP_NEW(OpString, ());

	if (!key || OpStatus::IsError(key->Set(filename)) || OpStatus::IsError(m_index_key_list.Add(key)))
	{
		OP_DELETE(key);
		OP_DELETE(listdata);
		return 0;
	}

	OpFile file;
	if (OpStatus::IsSuccess(file.Construct(filename.CStr(), OPFILE_ICON_FOLDER)) &&
		OpStatus::IsSuccess(file.Open(OPFILE_READ)))
	{
		while(!file.Eof())
		{
			OpString8* s = OP_NEW(OpString8, ());
			if (s && (OpStatus::IsError(file.ReadLine(*s)) || s->IsEmpty() || OpStatus::IsError(listdata->Add(s))))
				OP_DELETE(s);
		}
	}
	file.Close();

	if (listdata->GetCount() % 2 != 0)
	{
		// index file is probably corrupted - remove the odd element so at least
		// Opera does not crash on it (DSK-314586)
		listdata->Delete(listdata->GetCount() - 1);
	}

	if( OpStatus::IsError(m_index_cache.Add(key->CStr(), listdata)) )
	{
		listdata->DeleteAll();
		OP_DELETE(listdata);
		return 0;
	}

	return listdata;
}


OP_STATUS FavIconManager::MakeImageFilename(const uni_char* icon_url, const char* extension, OpString& filename)
{
	if (!icon_url)
		return OpStatus::ERR_NULL_POINTER;

	int len = uni_strlen(icon_url);

	OpString escaped;
	if (!escaped.Reserve( len*3 + 1))
		return OpStatus::ERR_NO_MEMORY;

	uni_char* p = escaped.CStr();
	for (int i=0; i<len; i++)
	{
		if (icon_url[i] == '\\' 
			|| icon_url[i] == '/' 
			|| icon_url[i] == ':' 
			|| icon_url[i] == '~'
			|| icon_url[i] == '?')
		{
			uni_sprintf(p, UNI_L("%%%02X"), (int)icon_url[i]);
			p+=3;
		}
		else
		{
			*p++ = icon_url[i];
		}
	}
	*p = 0;

	RETURN_IF_ERROR(filename.Set(escaped));
	if (extension && filename.HasContent())
		RETURN_IF_ERROR(filename.Append(extension));

	return filename.HasContent() ? OpStatus::OK : OpStatus::ERR; 
}


OP_STATUS FavIconManager::MakeIndexFilename(const uni_char* document_url, OpString& filename)
{
	if (!document_url || !*document_url)
		return OpStatus::ERR;

	OpString tmp;
	for (int i=0; i<2; i++)
	{
		OpString server_name;
		RETURN_IF_ERROR(GetServerName(document_url, server_name));
		if (server_name.HasContent())
		{
			RETURN_IF_ERROR(filename.Set(server_name));
			RETURN_IF_ERROR(filename.Append(".idx"));
			OpString illegalfilechars;
			static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetIllegalFilenameCharacters(&illegalfilechars);
			RETURN_IF_ERROR(RemoveChars(filename, illegalfilechars));
			break;
		}

		if (i==0)
		{
			// Fallback for incomplete addresses. Mostly needed when fetching an icon for typed addresses.
			BOOL match = uni_strncmp( document_url, UNI_L("www."), 4) == 0;
			RETURN_IF_ERROR(tmp.Set(match ? "http://" : "http://www."));
			RETURN_IF_ERROR(tmp.Append(document_url));
			document_url = tmp.CStr();
		}
	}

	return filename.HasContent() ? OpStatus::OK : OpStatus::ERR;
}


BOOL FavIconManager::AppendToIndex(const uni_char* document_url, const OpString& escaped_icon_filename, BOOL& saved_data_changed)
{
	OpVector<OpString8>* list = ReadIndex(document_url);
	if (!list)
		return FALSE;

	OpString8 escaped8;
	RETURN_VALUE_IF_ERROR(escaped8.Set(escaped_icon_filename.CStr()), FALSE);

	OpString8 document_url_8;
	RETURN_VALUE_IF_ERROR(document_url_8.Set(document_url), FALSE);

	BOOL updated_icon = FALSE;
	BOOL present = FALSE;
	INT32 num_changes = 0;
	INT32 num_match = 0;

	saved_data_changed = FALSE;

	if (list->GetCount() > 0)
	{
		UINT32 i;

		// Document urls at postion 0, 2, 4 etc
		// Icon names (file name) at positions 1, 3, 5 etc
		for (i = 1; i<list->GetCount(); i+= 2)
		{
			OpString8* document_url_entry = list->Get(i-1);
			OpString8* icon_entry = list->Get(i);

			if (icon_entry->Compare(escaped8) == 0)
			{
				// Icon file name identical to this entry. Try to simplify saved url to improve lookup later on.

				if (document_url_entry->Compare(document_url_8))
				{
					if (document_url_entry->Find(document_url_8) == 0)
					{
						// For the matched icon, the incoming document url is not the same as the saved but it
						// starts with the same string. So we will simplify the saved document url with the new.

						// This happens when we have previously saved an icon for the url "http://somesite/page/"
						// and we have now received the same icon when visiting "http://somesite/"

						OpString8* s1 = OP_NEW(OpString8, ());
						OpString8* s2 = document_url_entry;

						if (!s1 || OpStatus::IsError(s1->Set(document_url)) || OpStatus::IsError(list->Replace(i-1,s1)))
						{
							OP_DELETE(s1);
						}
						else
						{
							OP_DELETE(s2); // Delete old item in list
						}

						num_changes++;
						present = TRUE;
					}
					else if (document_url_8.Find(*document_url_entry) == 0)
					{
						// Current list entry is simpler. Keep it.

						// This happens when we have previously saved an icon for the url "http://somesite/"
						// and we have now received the same icon when visiting "http://somesite/page/"

						present = TRUE;
					}
				}
				else
				{
					num_match++;
					present = TRUE;
				}
			}
			else
			{
				// Incoming icon file name did not match the entry in the list. However, if
				// the document url shows an exact match we will replace the existing icon file
				// with the new (see DSK-249751) as the icon can have changed on site or we
				// (new from opera 11.50) have scaled and changed format to PNG.

				if (document_url_entry->Compare(document_url_8) == 0)
				{
					OpString8* s1 = OP_NEW(OpString8, ());
					OpString8* s2 = list->Get(i);

					if (!s1 || OpStatus::IsError(s1->Set(escaped8)) || OpStatus::IsError(list->Replace(i,s1)) )
					{
						OP_DELETE(s1);
					}
					else
					{
						// Delete old file to avoid duplication on disk
						OpFile image_file;
						OpString filename;
						if (OpStatus::IsSuccess(filename.Set(*s2)) && 
							OpStatus::IsSuccess(image_file.Construct(filename, OPFILE_ICON_FOLDER)))
							image_file.Delete(FALSE);

						updated_icon = TRUE;
						OP_DELETE(s2);
					}

					// Do not append new entry to list. We have already replaced the icon
					present = TRUE;
				}
			}
		}

		// Test for and remove any duplicate entries for the page url
		if (num_changes > 0 || num_match > 1)
		{
			int num_to_remove = num_changes + num_match - 1;
			for (i = 1; num_to_remove > 0 && i<list->GetCount(); i+= 2)
			{
				OpString8* icon_entry = list->Get(i);
				if (icon_entry->Compare(escaped8) == 0)
				{
					OpString8* document_url_entry = list->Get(i-1);
					if (document_url_entry->Compare(document_url_8) == 0)
					{
						num_to_remove --;
						list->Delete(i-1, 2);
						i -= 2; // List has become shorter, counter i+=2 in for loop
					}
				}
			}
		}
	}

	if (!present)
	{
		num_changes++;

		OpString8* s1 = OP_NEW(OpString8, ());
		OpString8* s2 = OP_NEW(OpString8, ());

		if (!s1 || !s2 || OpStatus::IsError(s1->Set(document_url)) || OpStatus::IsError(s2->Set(escaped8)) || OpStatus::IsError(list->Add(s1)))
		{
			// Just ignore to append item
			OP_DELETE(s1);
			OP_DELETE(s2);
		}
		else if (OpStatus::IsError(list->Add(s2)))
		{
			list->Delete(list->GetCount()-1);
			OP_DELETE(s2);
		}
	}

	if (num_changes > 0 || num_match > 1 || updated_icon)
	{
		saved_data_changed = TRUE;
		present = OpStatus::IsSuccess(WriteIndexToFile(document_url, *list));
	}

	return present;
}


OP_STATUS FavIconManager::ReplaceIndex(const uni_char* document_url, const OpString& old_icon_filename, const OpString& new_icon_filename)
{
	OpVector<OpString8>* list = ReadIndex(document_url);
	if (!list)
		return OpStatus::ERR;

	OpString8 old_escaped, new_escaped;
	RETURN_IF_ERROR(old_escaped.Set(old_icon_filename.CStr()));
	RETURN_IF_ERROR(new_escaped.Set(new_icon_filename.CStr()));

	// Icon names (file name) at positions 1, 3, 5 etc
	for(UINT32 i=1; i<list->GetCount(); i+=2)
	{
		OpString8* icon_entry = list->Get(i);
		if (!icon_entry->Compare(old_escaped))
		{
			OpString8* s = OP_NEW(OpString8,());
			if (!s || OpStatus::IsError(s->Set(new_escaped)))
				return OpStatus::ERR_NO_MEMORY;
			list->Replace(i, s);
			OP_DELETE(icon_entry);
		}
	}

	return WriteIndexToFile(document_url, *list);
}


OP_STATUS FavIconManager::WriteIndexToFile(const uni_char* document_url, OpVector<OpString8>& list)
{
	OpString filename;
	RETURN_IF_ERROR(MakeIndexFilename(document_url, filename));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr(), OPFILE_ICON_FOLDER));
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));

	for (UINT32 i=0; i<list.GetCount(); i++)
	{
		RETURN_IF_ERROR(file.Write(list.Get(i)->CStr(), list.Get(i)->Length()));
		RETURN_IF_ERROR(file.Write("\n", 1));
	}

	return OpStatus::OK;
}

void FavIconManager::GetImageFilename(const uni_char* document_url, OpString& image_filename, BOOL full_path, BOOL allow_near_match)
{
	OpVector<OpString8>* list = ReadIndex(document_url);
	if (!list)
		return;

	INT32 match_index = -1;

	if( list->GetCount() >= 2 )
	{
		OpString8 key;
		key.Set(document_url);
		if( key.HasContent() && key.CStr()[key.Length()-1] != '/' )
		{
			// All server urls in the index ends with a /. Improves matching
			key.Append("/");
		}

		UINT32 i;
		UINT32 max_length = 0;
		BOOL no_protocol = strstr(key.CStr(), "://") == 0;

		for(i=0; i<list->GetCount(); i+=2 )
		{
			const char* index_url = list->Get(i)->CStr();
			if( no_protocol )
			{
				// If url we test for does not contain a protocol, then
				// ignore the protocol part og the index url as well.
				// Improves matching
				const char* p = strstr(index_url, "://");
				if( p )
				{
					index_url = p + 3;
				}
			}

			const char* p = strstr(key.CStr(), index_url);
			if( p )
			{
				if( p == key.CStr() )
				{
					UINT32 length = list->Get(i)->Length();
					if( max_length < length )
					{
						max_length = length;
						match_index = i+1;
					}
				}
			}
		}
		if( match_index == -1 && allow_near_match)
		{
			match_index = 1;
		}

#if defined(SUPPORT_REVERSE_FAVICON_MATCHING)
		// This allows a key=http://<host>/ to match a list entry like http://<host>/some/sub/path
		// It might cause the key to match a wrong favicon if the site supports multiple icons
		// so to reduce this risk we only allow it to take place if there is one url-icon entry in 
		// the index. It will help in cases where a bookmark url gets redirected to another url
		// within the site
		if( match_index == -1 && list->GetCount() == 2 )
		{
			for(i=0; i<list->GetCount(); i+=2 )
			{
				const char* index_url = list->Get(i)->CStr();
				if( no_protocol )
				{
					// If url we test for does not contain a protocol, then
					// ignore the protocol part og the index url as well.
					// Improves matching
					const char* p = strstr(index_url, "://");
					if( p )
					{
						index_url = p + 3;
					}
				}

				const char* p = strstr(index_url, key.CStr());
				if( p )
				{
					if( p == index_url )
					{
						UINT32 length = list->Get(i)->Length();
						if( max_length < length )
						{
							max_length = length;
							match_index = i+1;
						}
					}
				}
			}
		}
#endif

	}

	if( match_index != -1 )
	{
		if( full_path )
		{
			OpString tmp_storage;
			image_filename.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_ICON_FOLDER, tmp_storage));
			image_filename.Append(*list->Get(match_index));
		}
		else
		{
			image_filename.Set(*list->Get(match_index));
		}
	}
}


void FavIconManager::EraseDataFile(const uni_char* document_url, const uni_char *icon_filename)
{
	OpVector<OpString8>* list = ReadIndex(document_url);
	if( !list )
		return;

	OpString8 tmp;
	tmp.Set(icon_filename);

	if( list->GetCount() > 0 )
	{
		UINT32 i;

		for(i = 1; i< list->GetCount(); i+= 2)
		{
			if( list->Get(i)->Compare(tmp) == 0 )
			{
				list->Delete(i);   // icon
				list->Delete(i-1); // url
				break;
			}
		}
	}

	OpFile image_file;
	if (OpStatus::IsSuccess(image_file.Construct(icon_filename, OPFILE_ICON_FOLDER)))
		image_file.Delete(FALSE);

	if( list->GetCount() <= 1 )
	{
		// Nothing left. Remove index file

		OpString index_filename;
		if (OpStatus::IsSuccess(MakeIndexFilename(document_url, index_filename)))
		{
			// Remove cache data

			OpAutoVector<OpString8>* listdata = 0;
			m_index_cache.Remove(index_filename.CStr(), &listdata);
			OP_DELETE(listdata);

			UINT32 i;

			for(i = 0; i<m_index_key_list.GetCount(); i++)
			{
				if( !m_index_key_list.Get(i)->Compare(index_filename) )
				{
					m_index_key_list.Delete(i);
					break;
				}
			}

			// Remove file
			OpFile index_file;
			if (OpStatus::IsSuccess(index_file.Construct(index_filename, OPFILE_ICON_FOLDER)))
				image_file.Delete(FALSE);
		}
	}
	else
	{
		// Remove entry in index file

		WriteIndexToFile(document_url, *list);
	}
}


/**
 * This deletes everything expect for icons mentioned in the persitent list
 */
void FavIconManager::EraseDataFiles()
{
	// Table with icons
	OP_DELETE(m_icon_table);
	m_icon_table = 0;

	// Hash table with content of index files
	m_index_cache.DeleteAll();

	static const uni_char* extension_mask[] =
		{ UNI_L("*.ico"), UNI_L("*.bmp"), UNI_L("*.gif"), UNI_L("*.png"), UNI_L("*.jpg"), UNI_L("*.jpeg"), UNI_L("*.idx"), NULL };

	for( INT32 i=0; extension_mask[i]; i++ )
	{
		OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ICON_FOLDER, extension_mask[i]);

		if(lister)
		{
			BOOL more = lister->Next();
			while( more )
			{
				if( !lister->IsFolder() )
				{
					const uni_char* candidate = lister->GetFullPath();
					if( candidate )
					{
						if( !IsPersistentFile(lister->GetFileName()) )
						{
							OpFile file;
							if (OpStatus::IsSuccess(file.Construct(candidate)))
								file.Delete();
						}
					}
				}
				more = lister->Next();
			}
			OP_DELETE(lister);
		}
	}

	// Restore index files for icons we did not remove
	RecreatePersistentIndex();

	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFavIconsRemoved();
	}
}

OP_STATUS FavIconManager::GetServerName(const uni_char* url, OpString& server_name)
{
	server_name.Empty();

	if (!url || !*url)
		return OpStatus::OK;

	const uni_char* s1 = uni_strstr(url, UNI_L("://") );
	if (!s1)
		return OpStatus::OK;
	const uni_char* s2 = uni_strstr(s1+3, UNI_L("/") );

	INT32 p1 = s1 + 3 - url;
	INT32 p2 = s2 ? s2 - url : uni_strlen(url);

	if (p2-p1 <= 0)
		return OpStatus::OK;

	return server_name.Set(url+p1, p2-p1);
}


OP_STATUS FavIconManager::UrlToBuffer(URL& url, StreamBuffer<UINT8>& buffer)
{
	URL_DataDescriptor* url_data_desc = url.GetDescriptor(NULL, TRUE);
	if (!url_data_desc)
		return OpStatus::ERR_NULL_POINTER;

	unsigned long data_size = 0;
	BOOL more = FALSE;
	OP_STATUS status;

	while (1)
	{
		TRAP(status, data_size = url_data_desc->RetrieveDataL(more));
		if (OpStatus::IsError(status) || data_size == 0 || !url_data_desc->GetBuffer())
			break;

		status = buffer.Append((unsigned char*)url_data_desc->GetBuffer(), data_size);
		if (OpStatus::IsError(status))
			break;

		url_data_desc->ConsumeData(data_size);
	}

	OP_DELETE(url_data_desc);
	OP_ASSERT(!more || !"We failed to save the complete icon");
	return status;
}


OP_STATUS FavIconManager::UrlToPNGBuffer(URL url, StreamBuffer<UINT8>& buffer)
{
	StreamBuffer<UINT8> tmp;
	RETURN_IF_ERROR(UrlToBuffer(url, tmp));

	OpBitmap* bitmap = IconUtils::GetBitmap(tmp, 16, 16);
	if (!bitmap)
		return OpStatus::ERR;

	OP_STATUS rc = IconUtils::WriteBitmapToPNGBuffer(bitmap, buffer);
	OP_DELETE(bitmap);

	return rc;
}


OP_STATUS FavIconManager::FileToBuffer(const OpString& filename, StreamBuffer<UINT8>& buffer)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename, OPFILE_ICON_FOLDER));
	BOOL exists;
	RETURN_IF_ERROR(file.Exists(exists));
	if (!exists)
		return OpStatus::OK;
	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	OpFileLength length;
	RETURN_IF_ERROR(file.GetFileLength(length));
	if (length > 0)
	{
		char* data = OP_NEWA(char, length);
		if (!data)
			return OpStatus::ERR_NO_MEMORY;
		OpFileLength bytes_read;
		if (OpStatus::IsError(file.Read(data, length, &bytes_read)) || bytes_read != length)
		{
			OP_DELETEA(data);
			return OpStatus::ERR;
		}

		buffer.TakeOver(data, bytes_read);
	}

	return OpStatus::OK;
}


OP_STATUS FavIconManager::FileToPNGBuffer(const OpString& filename, StreamBuffer<UINT8>& buffer)
{
	StreamBuffer<UINT8> tmp;
	RETURN_IF_ERROR(FileToBuffer(filename, tmp));

	OpBitmap* bitmap = IconUtils::GetBitmap(tmp, 16, 16);
	if (!bitmap)
		return OpStatus::ERR;

	OP_STATUS rc = IconUtils::WriteBitmapToPNGBuffer(bitmap, buffer);
	OP_DELETE(bitmap);
	return rc;
}


OP_STATUS FavIconManager::BufferToPNGBuffer(StreamBuffer<UINT8>& src, StreamBuffer<UINT8>& buffer)
{
	OpBitmap* bitmap = IconUtils::GetBitmap(src, 16, 16);
	if (!bitmap)
		return OpStatus::ERR;

	OP_STATUS rc = IconUtils::WriteBitmapToPNGBuffer(bitmap, buffer);
	OP_DELETE(bitmap);
	return rc;
}


OP_STATUS FavIconManager::BufferToFile(const StreamBuffer<UINT8>& buffer, const OpString& filename)
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename, OPFILE_ICON_FOLDER));
	RETURN_IF_ERROR(file.Open(OPFILE_OVERWRITE));
	return file.Write(buffer.GetData(), buffer.GetFilled());
}


OP_STATUS FavIconManager::ReplaceExtensionToPNG(OpString& filename)
{
	int length = filename.Length();
	static const uni_char* extension[] =
		{ UNI_L(".ico"), UNI_L(".bmp"), UNI_L(".gif"), UNI_L(".jpg"), UNI_L(".jpeg"), NULL };
	const uni_char* replacement = UNI_L(".png");

	BOOL match = FALSE;
	for (UINT32 i=0; extension[i]; i++)
	{
		int l = uni_strlen(extension[i]);
		if (length > l && uni_stricmp(&filename.CStr()[length-l-1], extension[i]))
		{
			filename.Delete(length-l);
			match = TRUE;
			break;
		}
	}

	if (!match)
	{
		int l = uni_strlen(replacement);
		if (length > l && !uni_stricmp(&filename.CStr()[length-l-1], replacement))
			return OpStatus::OK;
	}

	return filename.Append(replacement);
}


BOOL FavIconManager::IsPNGFile(const OpString& filename)
{
	int length = filename.Length();
	if (length > 5 && uni_stricmp(&filename.CStr()[length-4], UNI_L(".png")))
		return FALSE;
	return TRUE;
}
