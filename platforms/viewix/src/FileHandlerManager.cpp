/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

// Core :
#include "modules/locale/oplanguagemanager.h"
#include "modules/viewers/viewers.h"

#include "modules/util/filefun.h"
#include "modules/pi/OpBitmap.h"

#include "platforms/posix/posix_native_util.h"

// File Handler Manager :
#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"
#include "platforms/viewix/src/StringHash.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern BOOL g_is_crashdialog_active; // DSK-287850 Used to check if crashlog dialog is active. FixMe There should be an API for this

/***********************************************************************************
 ** InitializeFileHandlers
 **
 **
 **
 ** Called from an inactivity timer in desktopapplication.cpp
 ***********************************************************************************/
void InitializeFileHandlers()
{
	FileHandlerManager::GetManager()->DelayedInit();
}


/***********************************************************************************
 ** GetManager
 **
 ** The FileHandlerManager class is a singleton - the static variable m_manager points
 ** to the only instance.
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
OpAutoPtr<FileHandlerManager> FileHandlerManager::m_manager;
// static
FileHandlerManager* FileHandlerManager::GetManager()
{
	if( !m_manager.get() )
	{
		m_manager = OP_NEW(FileHandlerManager, ()); //OOM
		if( m_manager.get() )
		{
			TRAPD(err,m_manager->LoadL());
		}
	}

	return m_manager.get();
}

void FileHandlerManager::DeleteManager()
{
	OP_DELETE(m_manager.get());
}


/***********************************************************************************
 ** FileHandlerManager Constructor
 **
 **
 **
 **
 ***********************************************************************************/
FileHandlerManager::FileHandlerManager()
	: m_initialized(FALSE)
	, m_validation_enabled(TRUE)
{
	OP_ASSERT(!m_manager.get());
}


/***********************************************************************************
 ** FileHandlerManager Destructor
 **
 **
 **
 **
 ***********************************************************************************/
FileHandlerManager::~FileHandlerManager()
{
	if (this == m_manager.get())
		m_manager.release();
	else
		OP_ASSERT(!"Class expected to be singleton; but is freeing an extra instance");
}


/***********************************************************************************
 ** DelayedInit
 **
 **
 **
 **
 ***********************************************************************************/
void FileHandlerManager::DelayedInit()
{
	if(m_initialized)
	{
		return;
	}

	m_initialized = TRUE;

	// Make search path
	// ------------------------
	m_input_manager.InitDataDirs();

	// Process mimeinfo.cache files
	// ------------------------
	OpAutoVector<OpString> files;
 	m_input_manager.GetMimeInfoFiles(files);

	OpAutoVector<OpString> gnome_files;
	m_input_manager.GetGnomeVFSFiles(gnome_files);

	// Use them to create the mime table
 	m_store.InitMimeTable(files, gnome_files);

	// Process mailcap files
	// ------------------------
	m_input_manager.ParseMailcapFiles();

	// Process default files
	// ------------------------
	OpAutoVector<OpString> default_files;
	OpAutoVector<OpString> profilerc_files;
	m_input_manager.GetDefaultsFiles(profilerc_files, default_files);

	//Use them to create the default table
	m_store.InitDefaultTable(profilerc_files, default_files);

	// Process aliases and subclasses files
	// ------------------------
	OpAutoVector<OpString> aliases_files;
 	m_input_manager.GetAliasesFiles(aliases_files);

	OpAutoVector<OpString> subclasses_files;
 	m_input_manager.GetSubclassesFiles(subclasses_files);

	// Use them to update the mime table
	m_input_manager.UseAliasesAndSubclasses(aliases_files, subclasses_files);

	// Process globs files
	// ------------------------
	OpAutoVector<OpString> glob_files;
 	m_input_manager.GetGlobsFiles(glob_files);

	//Use them to create the glob table
 	m_store.InitGlobsTable(glob_files);

	// Make icon search path
	// ------------------------
	m_input_manager.InitIconSpecificDirs();

	// Process index.theme files
	// ------------------------
	OpAutoVector<OpString> index_theme_files;
	m_input_manager.GetIndexThemeFiles(index_theme_files);

	//Use them to set up the theme info
	m_input_manager.InitTheme(index_theme_files);
	m_input_manager.InitMimetypeIconDesktopDirs();
}


/***********************************************************************************
 ** GuessMimeType
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GuessMimeType(const OpStringC & filename,
											OpString & content_type)
{
	content_type.Empty();
	return content_type.Set(GetMimeType(filename, content_type));
}


/***********************************************************************************
 ** GetMimeType
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char *  FileHandlerManager::GetMimeType(const OpStringC & filename,
												  const OpStringC & content_type,
												  BOOL strip_path)
{
	if(!m_initialized)
		DelayedInit();

    if(content_type.HasContent())
		return content_type.CStr();

	GlobNode * node = m_store.GetGlobNode(filename, strip_path);

	return node ? node->GetMimeType().CStr() : 0;
}


/***********************************************************************************
 ** GetDefaultFileHandler
 **
 **
 **
 **
 ***********************************************************************************/
ApplicationNode * FileHandlerManager::GetDefaultFileHandler(const OpStringC & content_type)
{
	if(!m_initialized)
		DelayedInit();

	MimeTypeNode* default_node = m_store.GetMimeTypeNode(content_type);

	return default_node ? default_node->GetDefaultApp() : 0;
}


/***********************************************************************************
 ** GetAllHandlers
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetAllHandlers(const OpStringC & content_type,
											 OpVector<ApplicationNode>& handlers)
{
	if(!m_initialized)
		DelayedInit();

	if(content_type.IsEmpty())
		return OpStatus::OK;

	MimeTypeNode* node = m_store.GetMimeTypeNode(content_type);

	if(!node)
		return OpStatus::OK;

	ApplicationNode * app = 0;

	OpHashIterator*	it2 = node->m_applications->GetIterator();
	OP_STATUS ret_val2 = it2->First();

	while (OpStatus::IsSuccess(ret_val2))
	{
		app = (ApplicationNode*) it2->GetData();

		if(app)
		{
			LoadNode(app);
			handlers.Add(app);
		}
		ret_val2 = it2->Next();
	}
	OP_DELETE(it2);

	ApplicationNode* default_handler = 0;

	// There will only be a default handler on KDE and Gnome :
	if(m_default_file_handler.HasHandler())
		default_handler = m_store.MakeApplicationNode( &m_default_file_handler );

	if(default_handler)
	{
		handlers.Add(default_handler);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetDefaultFileHandler
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetDefaultFileHandler(const OpString& filename,
													const OpString &content_type,
													OpString &handler,
													OpString &handler_name,
													OpBitmap ** handler_bitmap)
{
	if(!m_initialized)
		DelayedInit();

	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	//Must have mime-type
	OP_ASSERT(content_type.HasContent());
	if(!content_type.HasContent())
	{
		return OpStatus::OK;
	}
	//-----------------------------------------------------

	ApplicationNode * app = GetDefaultFileHandler(content_type);

	if(app)
	{
		LoadNode(app); //Read in the desktop file if it hasn't been read already

		const OpStringC clean_comm = app->GetCleanCommand();

		if(clean_comm.HasContent())
		{
			handler.Set(clean_comm);
			handler_name.Set(app->GetName());
			*handler_bitmap = app->GetBitmapIcon();
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetOperaDefaultFileHandler
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetOperaDefaultFileHandler(const OpString& filename,
														 const OpString &content_type,
														 OpString &handler,
														 OpString &handler_name)
{
	if(!m_initialized)
		DelayedInit();

	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	//Must have mime-type
	OP_ASSERT(content_type.HasContent());
	if(!content_type.HasContent())
	{
		return OpStatus::OK;
	}
	//-----------------------------------------------------

	Viewer * viewer    = 0;

	if(content_type.CStr()) {
		viewer = viewers->FindViewerByMimeType(content_type);
	}

	if(viewer)
	{
		ViewAction action =  viewer->GetAction();

		if(action == VIEWER_APPLICATION)
		{
			const uni_char * app = viewer->GetApplicationToOpenWith();

			if(uni_strlen(app) > 0)
			{
				handler_name.Set( app );
				return handler.Set( app );
			}
		}
		else if(action == VIEWER_OPERA)
		{
			const uni_char * app = UNI_L("opera");

			if(uni_strlen(app) > 0)
			{
				handler_name.Set( app );
				return handler.Set( app );
			}
		}
	}

	if(FileHandlerManagerUtilities::IsDirectory(filename))
	{
		handler_name.Set( m_default_directory_handler.handler );
		return handler.Set(m_default_directory_handler.handler);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetFileHandlers
 **
 **
 **
 ** NOTE: The strings returned in handlers are the caller's resposibility to delete
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetFileHandlers(const OpString& filename,
											  const OpString& mimetype,
											  OpVector<OpString>& handlers,
											  OpVector<OpString>& handler_names,
											  OpVector<OpBitmap>& handler_icons,
											  URLType type,
											  UINT32 icon_size)
{
	if(!m_initialized)
		DelayedInit();

	BOOL file_is_local = (type == URL_FILE);
	BOOL file_is_url   = !file_is_local;

	if(file_is_url)
	{
		OpString type;
		type.Set("text/html");

		return GetFileHandlers(filename,
							   type,
							   handlers,
							   handler_names,
							   handler_icons,
							   URL_FILE,
							   icon_size);
	}

	OpString content_type;
	content_type.Set(mimetype.CStr());

	BOOL has_content_type = TRUE;

	if(content_type.IsEmpty())
	{
		has_content_type = FALSE;
		content_type.Empty();
		const OpStringC mime_type = GetMimeType(filename, content_type);
		content_type.Set(mime_type);
	}

	if(content_type.IsEmpty())
		return OpStatus::OK;

	//----------------- OPERA ------------------------
	//Only return the opera default to contexts that do not provide mime-type
	//For contexts that have mime-type available should use the viewers interface
	//to get this handler and GetFileHandler only for system default.

	// g_languageManager->GetStringL(Str::S_CONFIG_DEFAULT, default);

	if(!has_content_type && !file_is_url)
	{
		OpString * opera_default_handler      = OP_NEW(OpString, ());
		OpString * opera_default_handler_name = OP_NEW(OpString, ());

		GetOperaDefaultFileHandler(filename, content_type, *opera_default_handler, *opera_default_handler_name);

		if(opera_default_handler->HasContent())
		{
			handlers.Add( opera_default_handler );
			handler_names.Add( opera_default_handler_name );
			handler_icons.Add(NULL);
		}
		else
		{
			OP_DELETE(opera_default_handler);
			OP_DELETE(opera_default_handler_name);
		}
	}

	//----------------- SYSTEM ------------------------

	OpVector<ApplicationNode> app_nodes;

	ApplicationNode * default_app = GetDefaultFileHandler(content_type);

	if(default_app)
	{
		LoadNode(default_app); //Read in the desktop file if it hasn't been read already
		LoadIcon(default_app, icon_size);

		if(default_app->IsExecutable())
			app_nodes.Add(default_app);
	}

	//----------------- REST ------------------------

	GetAllHandlers(content_type, app_nodes);

	for(UINT32 i = 0; i < app_nodes.GetCount(); i++)
	{
		ApplicationNode * node = app_nodes.Get(i);

		LoadNode(node); //Read in the desktop file if it hasn't been read already
		LoadIcon(node, icon_size);

		if(!node->IsExecutable())
			continue;

		const OpStringC comm = node->GetCleanCommand();

		if(comm.HasContent())
		{
			//Get command:
			OpString * command = OP_NEW(OpString, ());
			command->Set(comm);

			//Get name:
			OpString * name    = OP_NEW(OpString, ());
			name->Set(node->GetName());

			handlers.Add( command );
			handler_names.Add( name );
			handler_icons.Add( node->GetBitmapIcon(icon_size) );
		}
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetApplicationIcon
 **
 **
 **
 **
 ***********************************************************************************/
OpBitmap*  FileHandlerManager::GetApplicationIcon(const OpStringC & handler,
												  const OpStringC & filename,
												  const OpStringC & content_type,
												  UINT32 icon_size)
{
	const OpStringC desktop_filename = GetDesktopFilename(filename,
														  content_type,
														  handler);

	const OpStringC icon_filename = GetApplicationIcon(desktop_filename,
													   icon_size);

	if(icon_filename.HasContent())
	{
		return FileHandlerManagerUtilities::GetBitmapFromPath(icon_filename);
	}

	return 0;
}


/***********************************************************************************
 ** GetFileTypeIcon
 **
 **
 **
 **
 ***********************************************************************************/
OpBitmap* FileHandlerManager::GetFileTypeIcon(const OpStringC& filename,
											  const OpStringC& content_type,
											  UINT32 icon_size)
{
	const OpStringC icon_filename = GetFileTypeIconPath(filename,
														content_type,
														icon_size);

	if(icon_filename.HasContent())
	{
		return FileHandlerManagerUtilities::GetBitmapFromPath(icon_filename);
	}

	return 0;
}


/***********************************************************************************
 ** GetApplicationIcon
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char * FileHandlerManager::GetApplicationIcon( const OpStringC & handler,
														  UINT32 icon_size)
{
	if(!m_initialized)
		DelayedInit();

	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	//Handler cannot be null
	OP_ASSERT(handler.HasContent());
	if(handler.IsEmpty())
		return 0;
	//-----------------------------------------------------

	//Get Application Node:
	void *vapp = 0;
	m_store.GetApplicationTable().GetData((void *) handler.CStr(), &vapp);
	ApplicationNode * app = (ApplicationNode * )vapp;

	//Application must be set - but this may just be a test to see if handler was
	//an application
	if(!app)
		return 0;

	OpString iconfile;
	m_input_manager.MakeIconPath(app, iconfile, icon_size);

	if(iconfile.HasContent())
	{
		app->SetIconPath(iconfile.CStr(), icon_size);
	}

	return app->GetIconPath(icon_size);
}


/***********************************************************************************
 ** GetFileTypeIconPath
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char * FileHandlerManager::GetFileTypeIconPath(const OpStringC & filename,
														 const OpStringC & content_type,
														 UINT32 icon_size)
{
	if(!m_initialized)
		DelayedInit();

	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	// Filename cannot be null
	OP_ASSERT(filename.HasContent());
	if(filename.IsEmpty())
		return 0;
	//-----------------------------------------------------

	//Get the mime type :
	const OpStringC mime_type = GetMimeType(filename, content_type, TRUE);

	if(mime_type.IsEmpty())
		return 0;

	//Get a MimeTypeNode :
	MimeTypeNode* node = 0;

	//If it does not exist MakeMimeTypeNode will make a new node
	//if it exists MakeMimeTypeNode will reuse it
	node = m_store.MakeMimeTypeNode(mime_type);

	LoadIcon(node, icon_size);

	return node ? node->GetIconPath(icon_size) : 0;
}


/***********************************************************************************
 ** GetFileTypeName
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char * FileHandlerManager::GetFileTypeName(const OpStringC& filename,
													 const OpStringC& content_type)
{
	if(!m_initialized)
		DelayedInit();

	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	//Filename cannot be null
	OP_ASSERT(filename.CStr());
	if(!filename.CStr())
		return 0;
	//-----------------------------------------------------

	// Get mime-type of file:
	const OpStringC mime_type = GetMimeType(filename, content_type, TRUE);

	if(mime_type.IsEmpty())
		return 0;

	MimeTypeNode* node = m_store.GetMimeTypeNode(mime_type);

	if(!node)
		return 0;

	LoadNode(node);

	if(node->GetName().HasContent())
		return node->GetName().CStr();

	if(node->GetComment().HasContent())
		return node->GetComment().CStr();

	return 0;
}


/***********************************************************************************
 ** GetFileTypeInfo
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetFileTypeInfo(const OpStringC& filename,
											  const OpStringC& content_type,
											  OpString & content_type_name,
											  OpBitmap *& content_type_bitmap,
											  UINT32 content_type_bitmap_size)
{
	if(!m_initialized)
		DelayedInit();

	//Get the mime type :
	const OpStringC mime_type = GetMimeType(filename, content_type, TRUE);

	if(mime_type.IsEmpty())
		return OpStatus::ERR;

	//Get a MimeTypeNode :
	MimeTypeNode* node = 0;

	//If it does not exist MakeMimeTypeNode will make a new node
	//if it exists MakeMimeTypeNode will reuse it
	node = m_store.MakeMimeTypeNode(mime_type);

	if(!node)
		return OpStatus::ERR;

	LoadIcon(node, content_type_bitmap_size);

	if(node->GetName().HasContent())
		content_type_name.Set(node->GetName());
	else if(node->GetComment().HasContent())
		content_type_name.Set(node->GetComment());

	const uni_char * icon_path = node->GetIconPath(content_type_bitmap_size);

	if(icon_path)
	{
		content_type_bitmap = FileHandlerManagerUtilities::GetBitmapFromPath(icon_path);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** GetFileHandlerName
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char * FileHandlerManager::GetFileHandlerName(const OpStringC& handler,
														const OpStringC& filename,
														const OpStringC& content_type)
{
	if(!m_initialized)
		DelayedInit();

	ApplicationNode * app = GetDefaultFileHandler(content_type);

	if(app)
	{
		LoadNode(app); //Read in the desktop file if it hasn't been read already
		return app->GetName().CStr();
	}
	else
	{
		OpVector<ApplicationNode> app_nodes;
		GetAllHandlers(content_type, app_nodes);

		if(app_nodes.GetCount())
			return app_nodes.Get(0)->GetName().CStr();
	}

	return 0;
}



/***********************************************************************************
 ** GetDesktopFilename
 **
 **
 **
 **
 ***********************************************************************************/
const uni_char * FileHandlerManager::GetDesktopFilename(const OpStringC & filename,
														const OpStringC & content_type,
														const OpStringC & handler)
{
	if(!m_initialized)
		DelayedInit();

	const OpStringC mime_type = GetMimeType(filename, content_type);

	//Mime type must be found
	if(mime_type.IsEmpty())
		return 0;

	const OpStringC command = handler.CStr();

	//Command must be specified
	if(command.IsEmpty())
		return 0;

	//TODO - try to find desktopfiles for user specified handlers?
    //	     Viewer * viewer = g_viewers->GetViewer(mime_type);

	MimeTypeNode* node = m_store.GetMimeTypeNode(mime_type);

	if(node)
	{
		ApplicationNode * app = 0;

		//Try to get the default:
		app = node->GetDefaultApp();

		if(app)
		{
			if(app->HasCommand(command))
			{
				return app->GetDesktopFileName().CStr();
			}
		}

		//Failing a default - get any handler:
		OpHashIterator*	it2 = node->m_applications->GetIterator();
		OP_STATUS ret_val2 = it2->First();

		while (OpStatus::IsSuccess(ret_val2))
		{
			app = (ApplicationNode*) it2->GetData();

			if(app)
			{
				if(app->HasCommand(command))
 				{
					return app->GetDesktopFileName().CStr();
				}
			}

			ret_val2 = it2->Next();
		}
		OP_DELETE(it2);
	}

	return 0;
}


/***********************************************************************************
 ** GetFileHandler
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetFileHandler(const OpString& filename,
											 const OpString& mimetype,
											 OpString &handler,
											 OpString &handler_name)
{
	if(!m_initialized)
		DelayedInit();

	if(filename.IsEmpty() && mimetype.IsEmpty())
		return OpStatus::ERR;

	OpString content_type;
	content_type.Set(mimetype.CStr());

	if(content_type.IsEmpty())
	{
		content_type.Empty();
		const OpStringC mime_type = GetMimeType(filename, content_type);
		content_type.Set(mime_type);
	}

	handler.Empty();
	handler_name.Empty();

	OpAutoVector<OpString> handlers;
	OpAutoVector<OpString> handler_names;
	OpAutoVector<OpBitmap> handler_icons;

	RETURN_IF_ERROR(GetFileHandlers(filename, mimetype,	handlers, handler_names, handler_icons,	URL_FILE));

	if(handlers.GetCount())
	{
		if (!g_is_crashdialog_active)
		{
			RETURN_IF_ERROR(handler.Set(handlers.Get(0)->CStr()));
			RETURN_IF_ERROR(handler_name.Set(handler_names.Get(0)->CStr()));
		}
		else
		{
 			// Find the first candidate which is not Opera in case of crash dialog
			for (UINT32 i=0; i<handlers.GetCount(); i++)
 			{
 				const char *programname_for_opera= "opera"; // FixMe: Get the name of this application by other means (such as GetExecutablePath() in main_contentL())
 				if (handlers.Get(i)->Compare(programname_for_opera, op_strlen(programname_for_opera)) != 0)
 				{
					RETURN_IF_ERROR(handler.Set(handlers.Get(i)->CStr()));
					RETURN_IF_ERROR(handler_name.Set(handler_names.Get(i)->CStr()));
 					break;
 				}
 			}
		}
	}
	return OpStatus::OK;
}


/***********************************************************************************
 ** GetProtocolHandler
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetProtocolHandler(const OpString &uri_string,
												 const OpString& protocol,
												 OpString &handler)
{
	if(!m_initialized)
		DelayedInit();

	handler.Empty(); //TODO - make some backend for protocols

	return OpStatus::ERR_NOT_SUPPORTED;
}


/***********************************************************************************
 ** ValidateHandler
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
BOOL FileHandlerManager::ValidateHandler( const OpStringC & filename,
										  const OpStringC & handler )
{
	OP_ASSERT(filename.HasContent());
	if(filename.IsEmpty())
		return FALSE;

	BOOL found_security_issue = CheckSecurityProblem(filename);

	if( !found_security_issue && !m_validation_enabled )
	{
		return TRUE;
	}

	OP_ASSERT(handler.HasContent());
	if(handler.IsEmpty())
		return FALSE;

	PosixNativeUtil::NativeString handler_path (handler);
	struct stat buf;
	if( stat( handler_path.get(), &buf ) == 0 && S_ISDIR(buf.st_mode) )
	{
		HandlerElement* item = &m_default_directory_handler;
		if( item->ask && item->handler.HasContent())
		{
			BOOL do_not_show_again = FALSE;
			BOOL accepted = FileHandlerManagerUtilities::ValidateFolderHandler(handler, item->handler, do_not_show_again);

			if( accepted && do_not_show_again )
			{
				item->ask = FALSE;
				TRAPD(err,WriteHandlersL(m_default_file_handler, m_default_directory_handler, m_list));
			}
		}
	}

	if( filename.IsEmpty() )
	{
		return TRUE;
	}

	OpString extension, tmp;

	tmp.Set(filename);
	TRAPD(err,SplitFilenameIntoComponentsL(tmp,0,0,&extension));
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what on failure ?

	HandlerElement* item = 0;
	for( UINT32 i=0; i<m_list.GetCount(); i++ )
	{
		HandlerElement* e = m_list.Get(i);
		if( e->SupportsExtension(extension) )
		{
			if( e->handler.CStr() && uni_stricmp(e->handler.CStr(), handler.CStr()) == 0 )
			{
				item = e;
				break;
			}
		}
	}

	if( !item )
	{
		HandlerElement* e = &m_default_file_handler;
		if( e->handler.CStr() && uni_stricmp(e->handler.CStr(), handler.CStr()) == 0 )
		{
			item = e;
		}
	}

	if( item && (item->ask || found_security_issue) )
	{
		BOOL do_not_show_again = FALSE;
		BOOL accepted = FileHandlerManagerUtilities::AcceptExecutableHandler(found_security_issue, filename, handler, do_not_show_again);

		if( accepted && do_not_show_again )
		{
			item->ask = FALSE;
			TRAPD(status, WriteHandlersL(m_default_file_handler, m_default_directory_handler, m_list));
		}
	}
	return TRUE; // Always allow to open, if handler is no registered.
}

/***********************************************************************************
 ** OpenFileFolder
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerManager::OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files)
{
	OpString path;
	OpString filename;

	if(FileHandlerManagerUtilities::IsDirectory(file_path) && !treat_folders_as_files)
		RETURN_IF_ERROR(path.Set(file_path));
	else
		RETURN_IF_ERROR(FileHandlerManagerUtilities::GetPath(file_path, path));

	if(!FileHandlerManagerUtilities::IsDirectory(path))
	{
		return OpStatus::ERR;
	}

	if(treat_folders_as_files)
		RETURN_IF_ERROR(FileHandlerManagerUtilities::StripPath(filename, file_path));

	OpString handler;
	RETURN_IF_ERROR(handler.Set(m_default_directory_handler.handler));

	//TODO - select the file

	FileHandlerManagerUtilities::RunExternalApplication(handler, path);

	return OpStatus::OK;
}

/***********************************************************************************
 ** OpenFileInApplication
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
BOOL FileHandlerManager::OpenFileInApplication( const OpStringC & filename,
												const OpStringC & handler,
												BOOL validate,
												BOOL convert_to_locale_encoding)
{
	// A number of bugs happen because we do not set this flag to TRUE when we should
	// It will most likly not cause any problems to alway attempt a conversion
	// since we save every file with OpFile anyway [espen 2008-03-03] 
	convert_to_locale_encoding = TRUE;



	if( convert_to_locale_encoding )
	{
		// Bug #240072
		// 1) Core has downloaded a file
		// 2) Core asks platform to save to disk -> filename gets encoded
		// 3) Core asks platform to launch file. We have to ensure same encoding to find the saved file.

		OpString tmp;
		FileHandlerManagerUtilities::NativeString(filename, tmp);

		return OpenFileInApplication(tmp, 0, handler, validate );
	}
	else
	{
		return OpenFileInApplication(filename, 0, handler, validate );
	}
}

/***********************************************************************************
 ** OpenFileInApplication
 **
 **
 **
 **
 ***********************************************************************************/
BOOL FileHandlerManager::OpenFileInApplication(const OpStringC & filename,
											   const OpStringC & parameters,
											   const OpStringC & handler,
											   BOOL validate)
{
	OP_ASSERT(filename.HasContent());
	if(filename.IsEmpty())
		return FALSE;

	// Store local copy of the handler application :
	OpString application;
	RETURN_VALUE_IF_ERROR(application.Set(handler), FALSE);
	application.Strip();

	// If it does not have a handler choose the default app :
	if(application.IsEmpty())
		RETURN_VALUE_IF_ERROR(GetDefaultApplication(filename, application), FALSE);

	// If no default was found either - give up :
	if(application.IsEmpty())
		return FALSE;

	// If the call should be validated then validate :
	if( validate && !ValidateHandler(filename, application) )
		return FALSE;

	// If the handler is a directory then the calling code is old
	// and should use OpenFileFolder instead.
	if(FileHandlerManagerUtilities::IsDirectory(application))
	{
		OP_ASSERT(FALSE); //Caller should be using OpenFileFolder
		return FALSE;
	}

	// Quote the filename to preserve spaces in the path :
	OpString f;

	if(filename.HasContent())
		f.AppendFormat(UNI_L("\"%s\""), filename.CStr());

	if(parameters.HasContent())
	{
		f.Append(" ");
		f.Append(parameters);
	}

	FileHandlerManagerUtilities::LaunchHandler(application, f );

	return TRUE;
}

/***********************************************************************************
 ** GetDefaultApplication
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
OP_STATUS FileHandlerManager::GetDefaultApplication(const OpStringC& filename,
													OpString& application)
{
	OpString file;
	RETURN_IF_ERROR(file.Set(filename));
	OpString application_name;
	OpString content_type;
	return GetFileHandler(file, content_type, application, application_name);
}




// Temporary function (used in BackslashSplitString only)
static OP_STATUS AppendArgument( OpVector<OpString>& list, const uni_char *str, int len = KAll )
{
	OpString* s = OP_NEW(OpString, ());
	if (!s)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(s->Set(str, len));
	return list.Add(s);
}

// Temporary function. Should be moved out of this module, to a UNIX specific util collection later. 
// Backslash escaping is mostly a UNIX specific issue
static OP_STATUS BackslashSplitString( OpVector<OpString>& list, const OpStringC &candidate, int sep, BOOL keep_strings )
{
	if (candidate.IsEmpty())
		return OpStatus::OK;

	const uni_char* scanner = candidate.CStr();
	const uni_char* start = NULL;
	BOOL inside_string = FALSE;
	BOOL last_char_was_separator = TRUE;
	BOOL last_char_was_backslash = FALSE;

	while (uni_char curr_char = *scanner)
	{
		if( keep_strings && (inside_string || last_char_was_separator) && curr_char == '\"' )
		{
			if( inside_string )
			{
				if( start )
				{
					int length  = scanner - start;
					RETURN_IF_ERROR(AppendArgument(list, start, length));
					start = NULL;
				}
			}
			else
			{
				start = scanner + 1;
			}
			inside_string = !inside_string;
		}
		else if(!inside_string && curr_char == sep && !last_char_was_backslash)
		{
			if( start )
			{
				int length = scanner - start;
				RETURN_IF_ERROR(AppendArgument(list, start, length));
				start = NULL;
			}
		}
		else if(!inside_string)
		{
			if( !start )
			{
				start = scanner;
			}
		}

		scanner++;
		last_char_was_separator = (curr_char == sep);
		last_char_was_backslash = (curr_char == '\\');
	}

	if (start && start < scanner)
	{
		int length = scanner - start;
		RETURN_IF_ERROR(AppendArgument(list, start, length));
	}

	return OpStatus::OK;
}





/***********************************************************************************
 ** ExecuteApplication
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
BOOL FileHandlerManager::ExecuteApplication (const OpStringC & protocol,
											 const OpStringC & application,
											 const OpStringC & parameters,
											 BOOL run_in_terminal)
{
	if( application.IsEmpty() )
	{
		return FALSE;
	}

	OpString program;
	program.Set(application);

	// Allow spaces in arguments Bug #DSK-259851
	OpAutoVector<OpString> substrings;	
	BackslashSplitString( substrings, program, ' ', TRUE ); // respects "quoting within" the string

	OpString full_application_path;
	if( substrings.GetCount() )
	{
		BOOL success = FALSE;
		if( FileHandlerManagerUtilities::ExpandPath( substrings.Get(0)->CStr(), X_OK, full_application_path ) )
		{
			OpString a;
			a.Set("\"");
			a.Append(full_application_path);
			a.Append("\"");

			for(unsigned int i = 1; i < substrings.GetCount(); i++)
			{
				OpString * str = substrings.Get(i);
				a.Append(" ");
				a.Append(str->CStr());
			}

			// We might need some extra handling of the parameters if a known trusted 
			// protocol is specified. That is not dealt with neither in core nor quick.
			// after telnet support was removed [espen 2007-11-22]
			
			if( protocol.HasContent() )
			{
				success = FileHandlerManagerUtilities::HandleProtocol(protocol, parameters);
			}

			if( !success )
			{
				success = FileHandlerManagerUtilities::LaunchHandler(a, parameters, run_in_terminal);
			}
		}

		if( !success )
		{
			FileHandlerManagerUtilities::ExecuteFailed(substrings.Get(0)->CStr(), protocol);
		}

		return success;
	}
	else
	{
		// Find default handler for the application (file)
		return OpenFileInApplication(application, parameters, (const uni_char*)NULL, TRUE);
	}
}


/***********************************************************************************
 ** CheckSecurityProblem
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
BOOL FileHandlerManager::CheckSecurityProblem( const OpStringC & filename )
{
	//-----------------------------------------------------
    // Parameter checking:
	//-----------------------------------------------------
	OP_ASSERT(filename.HasContent());
	if(filename.IsEmpty())
		return FALSE;
	//-----------------------------------------------------

	PosixNativeUtil::NativeString filename_path (filename.CStr());
	BOOL is_executable = access( filename_path.get(), X_OK ) == 0;

#if defined(CHECK_EXECUTABLE_FLAG_ITSELF)
	if( is_executable )
		return TRUE;
#endif

	// Extension
	const uni_char* ext = uni_strrchr( filename.CStr(), '.' );
	if( ext )
	{
#if defined(CHECK_DESKTOP_FILE_SECURITY)
		if( uni_stricmp(ext, UNI_L(".desktop")) == 0 )
			return TRUE;
#endif
#if defined(CHECK_SHELL_SCRIPT_SECURITY)
		if( is_executable && uni_stricmp(ext, UNI_L(".sh")) == 0 )
			return TRUE;
#endif
	}

	// File sniffing

	OpFile file;
	if(OpStatus::IsError(file.Construct(filename.CStr(), OPFILE_ABSOLUTE_FOLDER)) ||
	   OpStatus::IsError(file.Open(OPFILE_READ)))
		return FALSE;

	int read_count = 0;
	int line = 0;
	while( !file.Eof() && read_count < 20 )
	{
		line ++;

		OpString buf;
		OpString8 buf1;
		file.ReadLine(buf1);
		buf.SetFromUTF8(buf1.CStr());
		buf.Strip();
		if( buf.Length() > 0 )
		{
			read_count ++;

#if defined(CHECK_DESKTOP_FILE_SECURITY)
			// Locate [KDE Desktop Entry] or [Desktop Entry]
			const uni_char *p1 = uni_strchr( buf.CStr(), '[' );
			const uni_char *p2 = p1 ? uni_strchr( p1, ']' ) : 0;
			const uni_char *p3 = p2 ? uni_stristr(p1, UNI_L("Desktop Entry")) : 0;
			if( !p3 )
				p3 = p2 ? uni_stristr(p1, UNI_L("Desktop Action")) : 0;

			if( p1 && p2 && p3 )
				return TRUE;
#endif

#if defined(CHECK_SHELL_SCRIPT_SECURITY)
			if( line == 1 )
			{
				if( is_executable )
				{
					// Locate executable shellscripts
					p1 = uni_stristr(buf.CStr(), UNI_L("#!"));
					p2 = p1 ? uni_stristr(p1, UNI_L("sh")) : 0;
					if( p1 && p2 )
						return TRUE;
				}
			}
#endif
		}
	}

	return FALSE;
}
