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

#include "modules/util/opfile/opfile.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/timecache.h"

#include "modules/url/url2.h"
#include "modules/util/htmlify.h"
#include "modules/util/str.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/url/loadhandler/url_lh.h"
#ifdef _MIME_SUPPORT_
#include "modules/mime/mime_cs.h"
#endif // _MIME_SUPPORT_
#include "modules/about/operadrives.h"
#include "modules/gadgets/OpGadgetManager.h"

#ifdef UTIL_ZIP_CACHE
#include "modules/util/zipload.h"
#endif

#ifdef REMOVABLE_DRIVE_SUPPORT
extern BOOL DriveAvailable(const uni_char* aDrive);
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "modules/formats/uri_escape.h"
#include "modules/about/opfolderlisting.h"
#include "modules/url/tools/content_detector.h"

#ifndef WILDCHARS
# define WILDCHARS	"*?"
#endif

// Url_lhfd.cpp

// URL Load Handler File Directories

#ifdef _LOCALHOST_SUPPORT_
void URL_DataStorage::StartLoadingFile(BOOL guess_content_type, BOOL treat_zip_as_folder)
{
	// NOTE: OnLoadStarted() must be called before calling this method as this will report on failure/finished.
	OP_STATUS op_err = OpStatus::OK;
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NoLocalFile))
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(S, ERR_LOCALHOST_REMOVED_IN_VERSION_TEXT));
		OnLoadFailed();
		return;
	}
	url->SetAttribute(URL::KIsGeneratedByOpera, FALSE);

	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	uni_char *filename = (uni_char *) g_memory_manager->GetTempBuf();
	const uni_char *temp;

	op_err = OpStatus::OK;

	do{
		OpString temp_path;

		op_err = url->GetAttribute(URL::KUniPath_FollowSymlink_Escaped, 0, temp_path);

		if(OpStatus::IsError(op_err))
		{
			break;
		}

		if((size_t) ((server_name ? server_name->GetUniName().Length() : 0) + temp_path.Length() + 20) > UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()))
		{
			op_err = OpStatus::ERR;
			break;
		}

		temp = temp_path.CStr();

		filename[0] = '\0';
		if(server_name)
		{
			OpStringC tempserver(server_name->GetUniName());

			if(tempserver.HasContent() && tempserver.Compare(UNI_L("localhost")) != 0 && tempserver.Compare(".") != 0)
			{
				if(temp == NULL || !*temp)
					temp = UNI_L("/");
				OP_ASSERT(*temp == '/');
				uni_snprintf(filename, UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()), UNI_L("//%s%s"), tempserver.CStr(), temp);
				temp = NULL; // used, disable the cat operation further down;
			}
		}

		if(temp)
		{
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
			while(*temp == '/')
				temp ++;
#endif // SYS_CAP_FILESYSTEM_HAS_DRIVES

			uni_strlcpy(filename, temp,  UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()));
		}
	}while(0);

	if(OpStatus::IsError(op_err))
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		OnLoadFailed();
		return;
	}


	UriUnescape::ReplaceChars(filename, UriUnescape::LocalfileAllUtf8);

	url->Access(FALSE);
#ifdef SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
	if (!*filename)
	{
		URL drive(url, (char *) NULL);
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
		OperaDrives document(drive);
		if(OpStatus::IsError(document.GenerateData()))
		{
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			OnLoadFailed();
			return;
		}
		BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
		UnsetListCallbacks();
		mh_list->Clean();
		OnLoadFinished(URL_LOAD_FINISHED);
		return;
	}
#endif // SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES

	int flen = uni_strlen(filename);
	BOOL dir_end = filename[flen-1] == PATHSEPCHAR;
#ifdef SYS_CAP_FILESYSTEM_HAS_DRIVES
	if (flen == 2 && filename[1] == ':')
	{
		uni_strcat(filename, UNI_L(PATHSEP));
	}
	else if (flen>3 && dir_end)
#else // SYS_CAP_FILESYSTEM_HAS_DRIVES
    if (dir_end && flen > 1)
#endif // SYS_CAP_FILESYSTEM_HAS_DRIVES
		filename[flen-1] = '\0';

#ifdef SYS_CAP_NETWORK_FILESYSTEM_BACKSLASH_PATH
	// Handle WIN32 network filenames
	if(filename[0] == '\\' && filename[1] == '\\')
	{
		uni_char *firstlevel = uni_strchr(filename+2,'\\');
		if(firstlevel)
		{
			while(*firstlevel == '\\')
				firstlevel++;
			uni_char *secondlevel = uni_strchr(firstlevel,'\\');
			const uni_char *catstring= NULL;
			if (!secondlevel)
			{
				catstring = UNI_L("\\\\");
			}
			else if(secondlevel[1] == '\0')
			{
				catstring = UNI_L("\\");
			}

			if(catstring &&
			   uni_strlen(filename) + uni_strlen(catstring) + 1 <= UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()))
			{
				uni_strcat(filename,catstring);
			}
		}

	}
#endif // SYS_CAP_NETWORK_FILESYSTEM_BACKSLASH_PATH

#ifdef SYS_CAP_BLOCK_CON_FILE
	if(uni_stri_eq(filename, "C:\\CON") ||
		!((filename[0] == '\\' && filename[1] == '\\') || (op_isalpha((unsigned char) filename[0]) && filename[1] == ':')) )
	{
		url->SetAttribute(URL::KIsGeneratedByOpera, TRUE);
		// this File is illegal, and may cause crashes on Windows 95 and later, but not NT
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		OnLoadFailed();
		return;
	}
#endif // SYS_CAP_BLOCK_CON_FILE

	BOOL exists=FALSE;

	OpFile temp_file;

	temp_file.Construct(filename);

	temp_file.Exists(exists);
	if(!exists && uni_strrchr(filename,'*'))
	{
		uni_char *temp1 = uni_strrchr(filename, PATHSEPCHAR);
		if(temp1)
		{
			if(temp1 == uni_strchr(filename, PATHSEPCHAR))
				temp1++; // have to have at least one slash
			uni_char tempc = *temp1;
			*temp1 = '\0';

			temp_file.Construct(filename);
			temp_file.Exists(exists);
			*temp1 = tempc;
		}
	}

	if(!exists)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		OnLoadFailed();
		return;
	}

	OpFileInfo::Mode mode = OpFileInfo::NOT_REGULAR;

	op_err = temp_file.GetMode(mode);

	// Prevents opening special files that may hang Opera
	// Bug #221092 [espen 2006-08-07]
	if (OpStatus::IsError(op_err) 
#if defined(URL_DONT_LOAD_NONREGULAR_FILES)
		|| mode == OpFileInfo::NOT_REGULAR
#endif
		)
	{
		if(OpStatus::IsMemoryError(op_err))
			g_memory_manager->RaiseCondition(op_err);
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		OnLoadFailed();
		return;
	}

	uni_char *fileext = uni_strrchr(filename, '.');

	if (fileext)
    {
        URLContentType cnt_typ= URL_UNDETERMINED_CONTENT;

        if(OpStatus::IsError(op_err=FindContentType(cnt_typ,NULL,fileext+1, NULL, TRUE)))
        {
            g_memory_manager->RaiseCondition(op_err);
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_OUT_OF_MEMORY));
            OnLoadFailed();
            return;
        }
        SetAttribute(URL::KContentType, cnt_typ);
    }

	SetAttribute(URL::KLoadStatus,URL_LOADING);
	content_size = 0;

	BOOL is_zip_folder = FALSE;
#ifdef ZIPFILE_DIRECT_ACCESS_SUPPORT
	if(treat_zip_as_folder && mode == OpFileInfo::FILE)
	{
		// Show file listings for ZIP folders like for regular folders.
# ifdef UTIL_ZIP_CACHE
		is_zip_folder = g_zipcache->IsZipFile(filename);
# else // !UTIL_ZIP_CACHE
		OpZip zipfile;
		is_zip_folder = OpStatus::IsSuccess(zipfile.Open(filename, FALSE));
# endif
# ifdef GADGET_SUPPORT
		// But still allow installing gadgets by clicking them.
		if (is_zip_folder)
			is_zip_folder = !g_gadget_manager->IsThisAGadgetPath(filename);
# endif
	}
#endif // ZIPFILE_DIRECT_ACCESS_SUPPORT

	if(mode == OpFileInfo::DIRECTORY || is_zip_folder)
	{
#ifdef REMOVABLE_DRIVE_SUPPORT
		if( uni_strlen(filename) <= 3 && !DriveAvailable(filename) )
		{
			// The drive is not available
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		
			OnLoadFailed();
			return;
		}
#endif
		loading = OP_NEW(URL_FILE_DIR_LoadHandler, (url, g_main_message_handler, filename));//FIXME:OOM (unable to report)
		return;
	}

	temp_file.GetFileLength(content_size);

	SetAttribute(URL::KHeaderLoaded,TRUE);
	SetAttribute(URL::KMIME_CharSet, NULL);
	storage = Local_File_Storage::Create(url, filename);
	
	if(storage == NULL)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		OnLoadFailed();
		return;
	}

	if(old_storage)
	{
		OP_DELETE(old_storage);
		old_storage = NULL;
	}
	if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT && guess_content_type)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		op_err = content_detector.DetectContentType();
		if(OpStatus::IsError(op_err))
			goto err;
	}

	op_err = OpStatus::OK;
	switch((URLContentType) GetAttribute(URL::KContentType))
	{
	case URL_HTML_CONTENT:
		op_err = SetAttribute(URL::KMIME_Type, "text/html");
		break;
	case URL_TEXT_CONTENT:
		op_err = SetAttribute(URL::KMIME_Type, "text/plain");
		break;
	default:
		break;
	}
err:
	if(OpStatus::IsError(op_err))
	{
		OP_DELETE(storage);
		storage = NULL;
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
		
		OnLoadFailed();
		return;
	}


#ifdef _MIME_SUPPORT_
	if(((URLContentType) GetAttribute(URL::KContentType) == URL_MIME_CONTENT
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		|| (URLContentType) GetAttribute(URL::KContentType) == URL_MHTML_ARCHIVE
#endif
		) && !url->GetAttribute(URL::KUnique))
	{
		MIME_DecodeCache_Storage *processor = OP_NEW(MIME_DecodeCache_Storage, (url, NULL));

		if(!processor) // FIXME:OOM-yngve (unable to report OOM)
		{
			// Storage has not been taken
			//delete processor;
			//processor = NULL;
		}
		else
		{
			processor->Construct(url, storage);
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
			processor->SetFinished(TRUE);
#endif
			storage = processor;
			SetAttribute(URL::KContentType, URL_XML_CONTENT);
		}
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
		BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
		SetAttribute(URL::KLoadStatus,URL_LOADED);
	}
	else
#endif // _MIME_SUPPORT_
	{
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
		BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
		SetAttribute(URL::KLoadStatus,URL_LOADED);

		if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT && guess_content_type)
		{
			ContentDetector content_detector(url, TRUE, TRUE);
			OpStatus::Ignore(content_detector.DetectContentType()); 
		}
	}

	time_t time_now = g_timecache->CurrentTime();
	TRAP(op_err, SetAttributeL(URL::KVLocalTimeLoaded, &time_now));
	OpStatus::Ignore(op_err);

	UnsetListCallbacks();
	mh_list->Clean();
	OnLoadFinished(URL_LOAD_FINISHED);
}
#endif // _LOCALHOST_SUPPORT_

File_Info::File_Info()
{
	processed = FALSE;
	attrib = OpFileInfo::FILE;
	wr_time = 0;
	size = 0;
	type = 0;
}

File_Info::~File_Info()
{
	if(InList())
		Out();
}

void File_Info_List::Sort_List(int (*compare)(const File_Info *, const File_Info *))
{
	File_Info_List list_a, list_b;
	int count;


	File_Info *item1, *item2;

	item1 = First();
	if(!item1)
		return; // Empty List;

	item1 = item1->Suc();
	if(!item1)
		return; // Single item in list, no sorting needed

	count = 0;

	while(!Empty())
	{
		item1 = First();
		item1->Out();
		item1->IntoStart(&list_a);
		count++;

		item1 = First();
		if(item1)
		{
			item1->Out();
			item1->IntoStart(&list_b);
		}
	}

	if(count > 1)
	{
		// Only need to sort if at least one of the lists have more than one item
		list_a.Sort_List(compare);
		list_b.Sort_List(compare);
	}

	item1 = list_a.First();
	item2 = list_b.First();

	while(item1 && item2)
	{
		int d = compare(item1, item2);
		if(d <= 0)
		{
			item1->Out();
			item1->Into(this);
			item1 = list_a.First();
		}
		if(d >= 0)
		{
			item2->Out();
			item2->Into(this);
			item2 = list_b.First();
		}
	}

	Append(&list_a);
	Append(&list_b);

#if 1
	item1 = First();
	while(item1)
	{
		item2 = item1->Suc();
		if(item2)
		{
			OP_ASSERT(compare(item1, item2) <=0);
		}
		item1 = item2;
	}
#endif
}


#ifdef _LOCALHOST_SUPPORT_

URL_FILE_DIR_LoadHandler::URL_FILE_DIR_LoadHandler(URL_Rep *url_rep,
												   MessageHandler *mh,
												   const uni_char *filename)
: URL_LoadHandler(url_rep, mh)
, generator(NULL)
{
	dirname = SetNewStr(filename);//FIXME:OOM (bad design)
	started = FALSE;
}

URL_FILE_DIR_LoadHandler::~URL_FILE_DIR_LoadHandler()
{
	OP_DELETEA(dirname);
	OP_DELETE(generator);
}


void URL_FILE_DIR_LoadHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
}

CommState URL_FILE_DIR_LoadHandler::Load()
{
	URL_DataStorage *url_ds = url->GetDataStorage();

	if(url_ds == NULL || dirname == NULL)
		return COMM_REQUEST_FAILED;

	url_ds->SetAttribute(URL::KContentType, URL_HTML_CONTENT);
	url_ds->SetAttribute(URL::KMIME_ForceContentType, "text/html; charset=utf-16");

	url_ds->SetAttribute(URL::KLoadStatus, URL_LOADING);
	url_ds->SetAttribute(URL::KHeaderLoaded, TRUE);

	url_ds->BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);

	url->Access(FALSE);

	CreateDirList();

	/*
    static const int messages[] =
    {
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED,
		MSG_COMM_DATA_READY
    };
	*/

    //mh->SetCallBackList(url, Id(), 0, messages, ARRAY_SIZE(messages));
	mh->PostMessage(MSG_COMM_DATA_READY,Id(),0); 
	return COMM_LOADING;
}

unsigned URL_FILE_DIR_LoadHandler::ReadData(char *buffer, unsigned buffer_len)
{

	OP_STATUS stat = OpStatus::OK;
	if(buffer_len <= 1)
	{
		mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0); 
		return 0;
	}

	BOOL finished;
	if(!started)
	{
		OP_ASSERT(!generator);
		URL the_url(url, (char *) NULL);
		generator = OP_NEW(OpFolderListing, (the_url));
		OP_STATUS rc;
		if (!generator)
		{
			rc = OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			rc = generator->GenerateData();
		}
		if (OpStatus::IsRaisable(rc))
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
		started = TRUE;
	}
	stat =
		GenerateDirectoryHTML(generator, finfo_list, finished, TRUE); 
	if(OpStatus::IsMemoryError(stat))
		g_memory_manager->RaiseCondition(stat);

	/*
	unsigned used =GenerateDirectoryHTML(buffer, buffer_len, pos, count,
		started, dirname, &finfo, finished);
	*/

	if(finished)
	{
		OP_ASSERT(generator);
		if (generator)
		{
			generator->EndFolderListing();
		}
	}
	
	mh->PostMessage((finished ? MSG_COMM_LOADING_FINISHED : MSG_COMM_DATA_READY), Id(), 0); 
	//return used;
	return 0;
}

void URL_FILE_DIR_LoadHandler::EndLoading(BOOL force)
{
	finfo_list.Clear();
	started = FALSE;
	url->OnLoadFinished(URL_LOAD_FINISHED);
}

int finfo_compare(const File_Info *e1, const File_Info *e2 )
{
	if(e1->attrib != e2->attrib && (e1->attrib == OpFileInfo::DIRECTORY || e2->attrib == OpFileInfo::DIRECTORY))
		return ((e1->attrib  == OpFileInfo::DIRECTORY)?  -1 : 1);
	return e1->name.CompareI(e2->name);
}

void URL_FILE_DIR_LoadHandler::CreateDirList()
{
	// FIXME: Don't use stat
#ifdef DIRECTORY_SEARCH_SUPPORT
	const uni_char *pattern = UNI_L(SYS_CAP_WILDCARD_SEARCH);
	OpString temp_pattern;

	uni_char *temp1 = uni_strrchr(dirname, PATHSEPCHAR);
	uni_char tempc=0;
	if(temp1 && uni_strchr(temp1,'*') != NULL)
	{
		temp_pattern.Set(temp1+1);
		if(temp_pattern.HasContent())
			pattern = temp_pattern.CStr();

		if(temp1 == uni_strchr(dirname, PATHSEPCHAR))
			temp1++; // have to have at least on slash

		tempc = *temp1;
		*temp1 = '\0';
	}

	OpFolderLister *dir = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, pattern, dirname);

	if(temp1)
		*temp1 = tempc;

	if(dir == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

	File_Info* current_file;

	while(dir->Next())
	{
		current_file = OP_NEW(File_Info, ());//FIXME:OOM (unable to report)
		if(current_file == NULL)
			break;

		OP_STATUS rc;
		{
			OpFile file;

			if(OpStatus::IsError(file.Construct(dir->GetFullPath())))
			{
				OP_DELETE(current_file);
				continue;
			}

			if (OpStatus::IsSuccess(rc = file.GetLastModified(current_file->wr_time)))
			{
				current_file->attrib = OpFileInfo::FILE;
				
				rc = file.GetMode(current_file->attrib);
				if(OpStatus::IsError(rc))
				{
					OP_DELETE(current_file);
					continue;
				}

				file.GetFileLength(current_file->size);
				rc = current_file->name.Set(dir->GetFileName());
			}
		}

		if(OpStatus::IsError(rc))
		{
			OP_DELETE(current_file);
			if (OpStatus::IsRaisable(rc))
			{
				g_memory_manager->RaiseCondition(rc);
			}
		}
		else
		{
			current_file->Into(&finfo_list);

		}
		current_file = NULL;
	}

	OP_DELETE(dir);

	/*k=0;
	if (count && uni_strcmp(finfo[0].name, UNI_L("..")) == 0)
		k++;

	if(count > k)
		op_qsort(finfo+k, count -k, sizeof(file_info), finfo_compare);

	pos = 0;
	*/
	finfo_list.Sort_List(finfo_compare);

	started = FALSE;
#endif // DIRECTORY_SEARCH_SUPPORT
}

#endif // _LOCALHOST_SUPPORT_


#if defined _LOCALHOST_SUPPORT_ || defined _FILE_UPLOAD_SUPPORT_

void EscapeFileURL(uni_char* esc_url, uni_char* url, BOOL include_specials, BOOL escape_8bit)
{
	UriEscape::Escape(esc_url, url, 
		UriEscape::Ctrl | UriEscape::URIExcluded | UriEscape::Backslash | UriEscape::Hash | UriEscape::Percent |
		(include_specials ? UriEscape::AmpersandQmark | UriEscape::ColonSemicolon : static_cast<UriEscape::EscapeFlags>(0)) |
		(escape_8bit ? UriEscape::Range_80_ff : static_cast<UriEscape::EscapeFlags>(0)) |
		UriEscape::UsePlusForSpace_DEF);
}

#endif // _LOCALHOST_SUPPORT_

#if defined _LOCALHOST_SUPPORT_ || !defined NO_FTP_SUPPORT

//#define DirectoryTextMaxLen 64
#define FILE_LOCALHOST_STRING UNI_L("file://localhost/")

/*unsigned */
OP_STATUS URL_LoadHandler::GenerateDirectoryHTML(
	OpFolderListing *generator,
	File_Info_List &flist, BOOL &finished,BOOL delete_finfo)
{
	finished = TRUE;
	if(flist.Empty())
		return OpStatus::OK; 


	finished = FALSE;

	//buffer[0] = '\0';
	//char *bufferpos = buffer;

	//unsigned used = 0;
	File_Info * OP_MEMORY_VAR current = flist.First();
	uni_char *OP_MEMORY_VAR tempbuf1 = (uni_char*)g_memory_manager->GetTempBuf();
	uni_char *OP_MEMORY_VAR tempbuf1a = NULL;

	{
		size_t max_len = 0;
		size_t temp_len;

		File_Info *temp= flist.First();
		while(temp)
		{
			if(temp->name.HasContent())
			{
				temp_len = temp->name.Length();
				if(temp_len > max_len)
					max_len = temp_len;
			}
			temp = temp->Suc();
		}

		if(UNICODE_SIZE((max_len + 256) * 3) > g_memory_manager->GetTempBufLen())
		{
			tempbuf1a = OP_NEWA(uni_char, (max_len + 256) * 3);
			if(!tempbuf1a)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				if(delete_finfo)
				{
					flist.Clear();
				}
				finished = TRUE;
				return OpStatus::ERR_NO_MEMORY; 
			}

			tempbuf1 = tempbuf1a;
		}
	}

	OP_MEMORY_VAR uint32 processed_count =0;

	while(current && current->processed)
	{
		current = current->Suc();
	}

	//BOOL escape_8bit = (url->GetType() == URL_FTP);
	while(current)
	{
		OpString filename;

		if((URLType) url->GetAttribute(URL::KType) == URL_FILE)
		{
			OpString8 encoded_name;
			RETURN_IF_ERROR(encoded_name.SetUTF8FromUTF16(current->name.CStr(), current->name.Length()) );
			RETURN_IF_ERROR(filename.Set(encoded_name));
		}
		else
		{
			// Use the raw name for ftp links; we upconvert to UTF-16 here
			// by just extending the string. This is ok since we call
			// EscapeFileURL below.
			if (current->rawname.HasContent())
				RETURN_IF_ERROR(filename.Set(current->rawname));
			else
				RETURN_IF_ERROR(filename.Set(current->name));
		}

		current->processed = TRUE;
		EscapeFileURL(tempbuf1, filename.CStr(), TRUE, TRUE);

		if (generator)
		{
			struct tm *filetime_p = NULL;
			filetime_p = op_localtime(&current->wr_time);
			generator->WriteEntry(tempbuf1, current->name, current->type,
								  current->attrib, current->size,
								  filetime_p);
		}
		
		processed_count++;
		if(processed_count > 160)
			break;
		current = current->Suc();
	}

	OP_DELETEA(tempbuf1a);
	if(!current)
	{
		if(delete_finfo)
		{
			flist.Clear();
		}
		finished = TRUE;
	}
	return OpStatus::OK;
}

#endif // _LOCALHOST_SUPPORT_ || !NO_FTP_SUPPORT
