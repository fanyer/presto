/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Tord Akerbæk
*/

#include "core/pch.h"

#ifdef PREFS_FILE_DOWNLOAD

#include "modules/prefsloader/prefsfileloader.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"

#include "modules/util/opfile/opfolder.h"

OP_STATUS PrefsFileLoader::Construct(const OpStringC &host, const OpStringC8 &section, const OpStringC8 &pref, const OpStringC &from, const OpStringC &to)
{
	RETURN_IF_ERROR(m_host.Set(host));
	RETURN_IF_ERROR(m_section.Set(section));
	RETURN_IF_ERROR(m_pref.Set(pref));

    if(to.IsEmpty()) // No file name given, use existing file
    {
        OpString file_to;
        m_setPref = FALSE; // No need to set the pref, only get the value
        if(g_prefsManager->GetPreferenceL(section, pref, file_to, FALSE, host.CStr()))
    	    RETURN_IF_ERROR(m_file.Construct(file_to.CStr(),OPFILE_ABSOLUTE_FOLDER));
        else
            return OpStatus::ERR;
    }
    else // Construct path, using prefsload folder
    {
        m_setPref = TRUE; // Remember to set the pref when file is loaded
    	RETURN_IF_ERROR(m_file.Construct(to,OPFILE_PREFSLOAD_FOLDER));
    }
	URL url = g_url_api->GetURL(from.CStr());
	if (url.IsEmpty())
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_fileURL.SetURL(url);

	return OpStatus::OK;
}

OP_STATUS PrefsFileLoader::StartLoading()
{
	MessageHandler *mmh = g_main_message_handler;
	CommState stat =  m_fileURL.GetURL().Load(mmh, URL(),FALSE,FALSE,FALSE,FALSE);
	if(stat == COMM_REQUEST_FAILED)
		return OpStatus::ERR;
	RETURN_IF_ERROR(m_fileURL.GetURL().LoadToFile(m_file.GetFullPath()));

	URL_ID url_id = m_fileURL.GetURL().Id();

	OP_STATUS rc = OpStatus::OK;
	if (OpStatus::IsError(rc = mmh->SetCallBack(this, MSG_URL_MOVED, url_id)) ||
		OpStatus::IsError(rc = mmh->SetCallBack(this, MSG_URL_LOADING_FAILED, url_id)) ||
		OpStatus::IsError(rc = mmh->SetCallBack(this, MSG_URL_DATA_LOADED, url_id)))
	{
		mmh->UnsetCallBacks(this);
	}

	return rc;
}

void PrefsFileLoader::LoadData(int url_id)
{
	if (m_fileURL.GetURL().Status(TRUE) != URL_LOADING)
	{
        OP_STATUS rc = OpStatus::OK;
		if(m_setPref)
        {
            OP_MEMORY_VAR BOOL success = FALSE;
            if(m_host.IsEmpty())
			{
                TRAP(rc, success = g_prefsManager->WritePreferenceL(m_section.CStr(),m_pref.CStr(),m_file.GetFullPath()));
			}
            else
			{
	    	    TRAP(rc, success = g_prefsManager->OverridePreferenceL(m_host.CStr(),m_section.CStr(),m_pref.CStr(),m_file.GetFullPath(),FALSE));
			}
		    m_has_changes = m_has_changes || success;
        }
        else
        {
          // The file is downloaded, but the preference is untouched.
          // FIXME: (peter) Signal file users that the content of the file may have changed.
        }
		// Signal errors globally, as we cannot do much here.
		// FIXME: OOM: Or can we?
		if (OpStatus::IsRaisable(rc))
		{
			g_memory_manager->RaiseCondition(rc);
		}

		FinishLoading(url_id);
	}
}


OP_STATUS PrefsFileLoader::FinishLoading(int url_id)
{
	g_main_message_handler->RemoveCallBacks(this, url_id);
	m_fileURL.UnsetURL();
	MarkDead();
	Commit();

	return OpStatus::OK;
}

void PrefsFileLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_URL_LOADING_FAILED:
		FinishLoading(par2);
		break;

	case MSG_URL_MOVED:
		{
			MessageHandler *mmh = g_main_message_handler;
			mmh->UnsetCallBack(this, MSG_URL_DATA_LOADED);
			mmh->UnsetCallBack(this, MSG_URL_LOADING_FAILED);
			mmh->UnsetCallBack(this, MSG_URL_MOVED);
			mmh->SetCallBack(this, MSG_URL_DATA_LOADED, par2);
			mmh->SetCallBack(this, MSG_URL_LOADING_FAILED, par2);
			mmh->SetCallBack(this, MSG_URL_MOVED, par2);
		}
		break;

	case MSG_URL_DATA_LOADED:
		LoadData(par1);
		break;

	}
}

#endif // PREFS_FILE_DOWNLOAD
