/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOP_UTIL_MODULE_H
#define DESKTOP_UTIL_MODULE_H

#include "modules/hardcore/opera/module.h"
#ifdef _BITTORRENT_SUPPORT_
#include "adjunct/bittorrent/bt-globals.h"
#endif

class OpSessionManager;
class OpSpellcheck;

class DesktopUtilModule : public OperaModule
{
public:
	DesktopUtilModule()
		: OperaModule()
#ifdef SESSION_SUPPORT
		, m_session_manager(0)
#endif // SESSION_SUPPORT
#ifdef SPELLCHECK_SUPPORT
		, m_spellcheck(0)
#endif // SPELLCHECK_SUPPORT
		{}

	void InitL(const OperaInitInfo& info);
	void Destroy();

#ifdef SESSION_SUPPORT
	OpSessionManager* m_session_manager;
#endif // SESSION_SUPPORT

#ifdef SPELLCHECK_SUPPORT
	OpSpellcheck* m_spellcheck;
#endif // SPELLCHECK_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
	P2PGlobalData m_P2PGlobalData;
#endif
};

#ifdef SESSION_SUPPORT
#define g_session_manager g_opera->desktop_util_module.m_session_manager
#endif // SESSION_SUPPORT
#ifdef SPELLCHECK_SUPPORT
#define g_spellcheck g_opera->desktop_util_module.m_spellcheck
#endif // SPELLCHECK_SUPPORT
#ifdef _BITTORRENT_SUPPORT_
#define g_P2PGlobalData g_opera->desktop_util_module.m_P2PGlobalData
#endif

#define DESKTOP_UTIL_MODULE_REQUIRED

#endif // DESKTOP_UTIL_MODULE_H
