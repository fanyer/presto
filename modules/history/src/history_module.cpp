/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/history/history_module.h"
#include "modules/history/src/HistoryModel.h"
#include "modules/history/direct_history.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"

#ifdef HISTORY_SUPPORT

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
HistoryModule::HistoryModule() :
    m_history_model(NULL)
#ifdef DIRECT_HISTORY_SUPPORT
	,m_direct_history(NULL)
#endif // DIRECT_HISTORY_SUPPORT
{}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModule::InitL(const OperaInitInfo& )
{
	OpFile file; ANCHOR(OpFile, file);

	//--------------------------------------------------
	// Create the history model
	//--------------------------------------------------
	m_history_model = HistoryModel::Create(g_pccore->GetIntegerPref(PrefsCollectionCore::MaxGlobalHistory));

	if(!m_history_model)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	//--------------------------------------------------
	// Read the history file
	//--------------------------------------------------
	OP_STATUS s;
	g_pcfiles->GetFileL(PrefsCollectionFiles::GlobalHistoryFile, file);
	if (file.GetFullPath())
	{
		s = file.Open(OPFILE_READ);
		if (OpStatus::IsSuccess(s))
		{
			if (OpStatus::IsMemoryError(s = globalHistory->Read(&file)))
				LEAVE(s);
			file.Close();
		}
	}
	
	//--------------------------------------------------
	// Read in the opera pages
	//--------------------------------------------------
	if (OpStatus::IsMemoryError(s = m_history_model->InitOperaPages()))
		LEAVE(s);

#if defined(URL_USE_HISTORY_VLINKS) && defined(DISK_CACHE_SUPPORT)
	//--------------------------------------------------
	// Read in the visited links file
	//--------------------------------------------------
	if (OpStatus::IsMemoryError(s = globalHistory->ReadVlink()))
		LEAVE(s);
#endif // URL_USE_HISTORY_VLINKS && DISK_CACHE_SUPPORT

#ifdef DIRECT_HISTORY_SUPPORT
	//--------------------------------------------------
	// Create the typed history model
	//--------------------------------------------------
	m_direct_history = DirectHistory::CreateL(g_pccore->GetIntegerPref(PrefsCollectionCore::MaxDirectHistory));
	
	if(!m_direct_history)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	//--------------------------------------------------
	// Read the typed history file
	//--------------------------------------------------

	if (OpStatus::IsMemoryError(m_direct_history->Read()))
		LEAVE(s);
#endif // DIRECT_HISTORY_SUPPORT

	//--------------------------------------------------
	// Listen to the prefs
	//--------------------------------------------------
	g_pccore->RegisterListenerL(this);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModule::Destroy()
{
	//--------------------------------------------------
	// Stop listening to prefs
	//--------------------------------------------------
	
	if (g_pccore)
		g_pccore->UnregisterListener(this);

	//--------------------------------------------------
	// Delete the history model
	//--------------------------------------------------
    OP_DELETE(m_history_model);
    m_history_model = NULL;

# ifdef DIRECT_HISTORY_SUPPORT
	//--------------------------------------------------
    // Delete the typed history model
	//--------------------------------------------------
	OP_DELETE(m_direct_history);
	m_direct_history = NULL;
# endif // DIRECT_HISTORY_SUPPORT
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void HistoryModule::PrefChanged(enum OpPrefsCollection::Collections id,
								int pref,
								int newvalue)
{
	if(id != OpPrefsCollection::Core)
		return;

	switch(pref)
	{
	case PrefsCollectionCore::MaxGlobalHistory:
		//--------------------------------------------------
		// History size changed, resize
		//--------------------------------------------------
		m_history_model->Size(newvalue);
		break;

#ifdef DIRECT_HISTORY_SUPPORT
	case PrefsCollectionCore::MaxDirectHistory:
		//--------------------------------------------------
		// Typed history size changed, resize
		//--------------------------------------------------
		m_direct_history->Size(newvalue);
		break;
#endif // DIRECT_HISTORY_SUPPORT
	}
}

#endif // HISTORY_SUPPORT
