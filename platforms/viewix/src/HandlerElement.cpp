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
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"

// File Handler Manager :
#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/HandlerElement.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"

/***********************************************************************************
 ** LoadL
 **
 **
 **
 **
 ***********************************************************************************/
void FileHandlerManager::LoadL()
{
    m_list.DeleteAll();

    OpStackAutoPtr <OpFile> file(NULL);

    file.reset(OP_NEW_L(OpFile, ()));

    static const uni_char * const filename = UNI_L("filehandler.ini");
    LEAVE_IF_ERROR(file->Construct(filename, OPFILE_HOME_FOLDER));
    BOOL exists;
    OP_STATUS res = file->Exists(exists);
    if (OpStatus::IsError(res) || !exists)
    {
		file.reset(OP_NEW_L(OpFile, ()));
		LEAVE_IF_ERROR(file->Construct(filename, OPFILE_RESOURCES_FOLDER));
    }

    OpStackAutoPtr<PrefsFile> prefs(OP_NEW_L(PrefsFile, (PREFS_STD))); //OOM
    prefs->ConstructL();
    prefs->SetFileL(file.get());
    prefs->LoadAllL();

    OpStackAutoPtr<PrefsSection> section(prefs->ReadSectionL(UNI_L("Handlers")));
    if( section.get() )
    {
		const PrefsEntry* entry = (const PrefsEntry*)section->Entries();
		while( entry )
		{
			HandlerElement* item = OP_NEW(HandlerElement, ()); //OOM
			if( item )
			{
				item->SetExtension(entry->Key());
				item->SetHandler(entry->Value());
				m_list.Add(item);
			}
			entry = (const PrefsEntry*)entry->Suc();
		}
    }

    OpString s;

    const uni_char *handler_fallback = UNI_L("");

    if (FileHandlerManagerUtilities::isKDERunning())
		handler_fallback = UNI_L("kioclient exec,1");
    else if (FileHandlerManagerUtilities::isGnomeRunning())
		handler_fallback = UNI_L("gnome-open,1");
	else
		handler_fallback = UNI_L("xdg-open,1");

    prefs->ReadStringL( "Settings", "Default File Handler", s, handler_fallback );

    m_default_file_handler.SetHandler( s.CStr() );

    g_languageManager->GetStringL(Str::S_FALLBACK_FILE, s);
    m_default_file_handler.extension.Set( s.CStr() ); // why not SetExtensionL ?

    prefs->ReadStringL( "Settings", "Default Directory Handler", s, handler_fallback );
    m_default_directory_handler.SetHandler( s.CStr() );

    g_languageManager->GetStringL(Str::S_FALLBACK_FOLDER, s);
    m_default_directory_handler.extension.Set( s.CStr() ); // why not SetExtensionL ?
}


/***********************************************************************************
 ** WriteHandlersL
 **
 **
 **
 **
 ***********************************************************************************/
void FileHandlerManager::WriteHandlersL(HandlerElement& file_handler,
										HandlerElement& directory_handler,
										OpVector<HandlerElement>& list)
{
    OpFile file;
    file.Construct( UNI_L("filehandler.ini"), OPFILE_HOME_FOLDER );

    OpStackAutoPtr<PrefsFile> prefs(OP_NEW_L(PrefsFile, (PREFS_STD))); //OOM
    prefs->ConstructL();
    prefs->SetFileL(&file);
    prefs->LoadAllL();

    prefs->DeleteSectionL(UNI_L("Settings"));
    prefs->DeleteSectionL(UNI_L("Handlers"));

    OpString value_string;

    value_string.SetL(file_handler.handler.CStr());
    value_string.AppendL( file_handler.ask ? UNI_L(",1") : UNI_L(",0") );
    prefs->WriteStringL( UNI_L("Settings"), UNI_L("Default File Handler"), value_string.CStr() );

    value_string.SetL(directory_handler.handler.CStr());
    value_string.AppendL( directory_handler.ask ? UNI_L(",1") : UNI_L(",0") );
    prefs->WriteStringL( UNI_L("Settings"), UNI_L("Default Directory Handler"), value_string.CStr() );

    for( UINT32 i=0; i<list.GetCount(); i++ )
    {
		HandlerElement* item = list.Get(i);
		if( item )
		{
			value_string.SetL(item->handler.CStr());
			value_string.AppendL( item->ask ? UNI_L(",1") : UNI_L(",0") );
			prefs->WriteStringL( UNI_L("Handlers"), item->extension.CStr(), value_string.CStr() );
		}
    }

    prefs->CommitL();
    LoadL(); // Sync
}


/***********************************************************************************
 ** SupportsExtension
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
BOOL HandlerElement::SupportsExtension(const OpString& candidate)
{
    for( UINT32 i=0; i<extension_list.GetCount(); i++ )
    {
		if( candidate.CompareI(*extension_list.Get(i)) == 0 )
		{
			return TRUE;
		}
    }
    return FALSE;
}

/***********************************************************************************
 ** SetExtension
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
OP_STATUS HandlerElement::SetExtension(const OpStringC & candidate)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    OP_ASSERT(candidate.HasContent());
    if(candidate.IsEmpty())
		return OpStatus::ERR;
    //-----------------------------------------------------

    RETURN_IF_ERROR(extension.Set(candidate));
    extension.Strip();
	FileHandlerManagerUtilities::SplitString( extension_list, extension, ',' );

    return OpStatus::OK;
}

/***********************************************************************************
 ** SetHandler
 **
 **
 **
 ** [espen - Espen Sand]
 ***********************************************************************************/
OP_STATUS HandlerElement::SetHandler(const OpStringC & candidate)
{
    handler.Empty();
    ask = FALSE;

    if( candidate.HasContent() )
    {
		uni_char *s = uni_strchr( candidate.CStr(), ',' );
		if( s )
		{
			int val = 0;
			uni_sscanf(s+1, UNI_L("%d"), &val);
			ask = val != 0;

			if( s > candidate.CStr() )
			{
				handler.SetL(candidate.CStr(), s - candidate.CStr());
			}
		}
		else
		{
			ask = FALSE;
			RETURN_IF_ERROR(handler.Set(candidate));
		}

		handler.Strip();
    }

    return OpStatus::OK;
}
