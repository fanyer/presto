/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/desktop_util/desktop_util_module.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#ifdef M2_SUPPORT
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/opera_glue/m2glue.h"
# include "adjunct/quick/managers/KioskManager.h"
#endif // M2_SUPPORT

#ifdef SPELLCHECK_SUPPORT
# include "adjunct/spellcheck/opsrc/spellcheck.h"
#endif // SPELLCHECK_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
# include "adjunct/bittorrent/bt-globals.h"
#endif	// _BITTORRENT_SUPPORT_

#ifdef EMBROWSER_SUPPORT
extern long gVendorDataID;
#endif //EMBROWSER_SUPPORT

#ifdef DU_REMOTE_URL_HANDLER
# include "adjunct/desktop_util/handlers/RemoteURLHandler.h"
#endif // DU_REMOTE_URL_HANDLER

void
DesktopUtilModule::InitL(const OperaInitInfo& info)
{
	// Check if Core changed OPFILE_LANGUAGE_FOLDER and update desktop-specific language folders if it did (DSK-345980)
	// This has to be done before initialization of SearchEngineManager
	LEAVE_IF_ERROR(FileUtils::UpdateDesktopLocaleFolders());

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	SearchEngineManager* manager = SearchEngineManager::GetInstance();
	manager->ConstructL();
#endif // !NO_SEARCH_ENGINES

#ifdef SESSION_SUPPORT
	m_session_manager = OP_NEW_L(OpSessionManager, ());
	m_session_manager->InitL();
#endif // SESSION_SUPPORT

#ifdef SPELLCHECK_SUPPORT
	m_spellcheck = OP_NEW_L(OpSpellcheck, ());
	if (m_spellcheck->Init() != OpStatus::OK)
	{
		OP_DELETE(m_spellcheck);
		m_spellcheck = NULL;
	}
#endif // SPELLCHECK_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
	m_P2PGlobalData.Init();
#endif

}

void
DesktopUtilModule::Destroy()
{
#ifdef _BITTORRENT_SUPPORT_
	m_P2PGlobalData.Destroy();
#endif

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	g_searchEngineManager->CleanUp();
#endif // DESKTUP_UTIL_SEARCH_ENGINES

#ifdef SESSION_SUPPORT
	OP_DELETE(m_session_manager);
	m_session_manager = NULL;
#endif // SESSION_SUPPORT

#ifdef SPELLCHECK_SUPPORT
	OP_DELETE(m_spellcheck);
	m_spellcheck = NULL;
#endif // SPELLCHECK_SUPPORT

#ifdef DU_REMOTE_URL_HANDLER
	RemoteURLHandler::FlushURLList();
#endif // DU_REMOTE_URL_HANDLER

}
