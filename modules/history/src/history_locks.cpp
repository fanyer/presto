/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/history/direct_history.h"

#ifdef DIRECT_HISTORY_SUPPORT
OpTypedHistorySyncLock::OpTypedHistorySyncLock()
{
#ifdef SYNC_TYPED_HISTORY
    OP_ASSERT(!IsLocked());
    g_directHistory->m_sync_blocked = TRUE;
#endif // SYNC_TYPED_HISTORY
}

OpTypedHistorySyncLock::~OpTypedHistorySyncLock()
{
#ifdef SYNC_TYPED_HISTORY
    OP_ASSERT(IsLocked());
    g_directHistory->m_sync_blocked = FALSE;
#endif // SYNC_TYPED_HISTORY
}

BOOL OpTypedHistorySyncLock::IsLocked()
{
#ifdef SYNC_TYPED_HISTORY
    return g_directHistory->m_sync_blocked;
#else
	return FALSE;
#endif // SYNC_TYPED_HISTORY
}
/***********************************************************************************
 **
 **
 **
 **	
 **
 ***********************************************************************************/

OpTypedHistorySaveLock::OpTypedHistorySaveLock()
{
    OP_ASSERT(!IsLocked());
    g_directHistory->m_save_blocked = TRUE;
}

OpTypedHistorySaveLock::~OpTypedHistorySaveLock()
{
    OP_ASSERT(IsLocked());
    g_directHistory->m_save_blocked = FALSE;
}

BOOL OpTypedHistorySaveLock::IsLocked()
{
    return g_directHistory->m_save_blocked;
}
#endif // DIRECT_HISTORY_SUPPORT
