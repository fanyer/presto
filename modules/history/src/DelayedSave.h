/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HISTORY_DELAYED_SAVE_H
#define HISTORY_DELAYED_SAVE_H

#if defined(DIRECT_HISTORY_SUPPORT) || defined(HISTORY_SUPPORT)

#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/hardcore/timer/optimer.h"

class DelayedSave : public OpTimerListener
{
public:

	DelayedSave() : m_is_dirty(FALSE), m_save_timer_enabled(TRUE), m_timeout_ms(HISTORY_WRITE_TIMEOUT_PERIOD) {}

	virtual ~DelayedSave() {}

	/**
	 *
	 *
	 * @param force
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    OP_STATUS RequestSave(BOOL force = FALSE);

    /**
	 * Enable the save timer (on by default)
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	OP_STATUS EnableTimer() {m_save_timer_enabled = TRUE; return OpStatus::OK;}

    /**
	 * Disable the save timer (on by default) - no unforced saves will be performed
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
	OP_STATUS DisableTimer() {m_save_timer_enabled = FALSE; return OpStatus::OK;}

	/**
	 * This timeout function is activated when the data in the list
	 * should be saved
	 *
	 * @param timer The timer that triggered the timeout
	 */
	virtual void OnTimeOut(OpTimer* timer);

	/**
	 * Writes the items to file
	 *
	 * @param ofp             - the file to write to
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if out of memory
	 */
    virtual OP_STATUS Write(OpFile *ofp) = 0;

	/**
	 * @return the filepref of the file to save to
	 */
	virtual PrefsCollectionFiles::filepref GetFilePref() = 0;

private:

	void SetDirty();

	// Private fields

	OpTimer m_timer;
	BOOL m_is_dirty;
	BOOL m_save_timer_enabled;
	UINT32 m_timeout_ms;
};

#endif // DIRECT_HISTORY_SUPPORT || HISTORY_SUPPORT

#endif // HISTORY_DELAYED_SAVE_H
