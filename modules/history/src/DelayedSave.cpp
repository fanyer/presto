/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(DIRECT_HISTORY_SUPPORT) || defined(HISTORY_SUPPORT)

#include "modules/history/src/DelayedSave.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DelayedSave::RequestSave(BOOL force)
{
	if(force)
	{
		m_timer.Stop(); //If timer is running stop it - we are saving now
		OP_STATUS rc = OpStatus::OK;

		OpFile file;
		TRAP(rc, g_pcfiles->GetFileL(GetFilePref(), file));

		if (OpStatus::IsSuccess(rc))
		{
			RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
			rc = Write(&file);
			file.Close();
		}

		return rc;
	}
	else
	{
		SetDirty();
		return OpStatus::OK;
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DelayedSave::SetDirty ()
{
	if( !m_is_dirty && m_save_timer_enabled)
	{
		m_is_dirty = TRUE;
		m_timer.SetTimerListener( this );
		m_timer.Start(m_timeout_ms);
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void DelayedSave::OnTimeOut(OpTimer* timer)
{
	if( timer == &m_timer && m_is_dirty )
	{
		OP_STATUS rc = RequestSave(TRUE);
		if (OpStatus::IsSuccess(rc))
		{
			m_is_dirty = FALSE;
		}

		//Just in case:
		if(m_is_dirty)
		{
			OP_ASSERT(FALSE);
			// Noone has saved - resetting dirty anyway
			// must clear this, otherwise SetDirty will not trigger
			// another timer next time if the save failed somehow
			m_is_dirty = FALSE;
		}
	}
}

#endif // DIRECT_HISTORY_SUPPORT || HISTORY_SUPPORT
