/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/unicode/unicode_module.h"

#if defined UNICODE_MODULE_REQUIRED

UnicodeModule::UnicodeModule()
{
#ifdef SUPPORT_TEXT_DIRECTION
	m_last_mirror[0] = m_last_mirror[1] = 0;
	m_last_bidi_block = 0;
	m_last_bidi_plane = 0;
#endif
#ifndef _NO_GLOBALS_
	m_cc_cache[0].c = m_cc_cache[1].c = 0;
	m_cc_cache[0].cls = m_cc_cache[1].cls = CC_Unknown;
	m_cc_cache_old = 0;
	m_lbc_cache[0].c = m_lbc_cache[1].c = 0;
	m_lbc_cache[0].lbc = m_lbc_cache[1].lbc = LB_XX;
	m_lbc_cache_old = 0;
#endif // !_NO_GLOBALS_
}

void UnicodeModule::InitL(const OperaInitInfo &)
{
	/* nothing */
}

void UnicodeModule::Destroy()
{
	/* nothing; keep cache from last run if restarted */
}

#endif // UNICODE_MODULE_REQUIRED
