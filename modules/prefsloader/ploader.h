/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Tord Akerbæk
*/

/**
 * @file ploader.h
 * Object administrating the downloading of preferences and preference data files.
 *
 * The classes in this file are only for internal use in the prefsload submodule.  
 * They should not be used outside of the module. The result would be unpredictable.
 * Interact with the submodule only through g_PrefsLoadManager.
 */

#if !defined PLOADER_H && defined PREFS_DOWNLOAD 
#define PLOADER_H

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/simset.h"

/**
 * A loading unit. May be xml preference data being loading, or a utility file of type javascrpt or css.
 * This base class is mainly used to keep track of the loading status. 
 */
class PLoader : public Link, public MessageObject
{
public:
	inline PLoader() : m_dead(FALSE), m_has_changes(FALSE) {}

	/**
	 * Mark for deletion.
	 */
	inline void MarkDead() { m_dead = TRUE; }

	/**
	 * Check if marked for deletion.
	 */
	inline BOOL IsDead() { return m_dead; }

	/**
	 * Commit changes if pending.
	 */
	void Commit();

private:
	BOOL m_dead;

protected:
	BOOL m_has_changes;
};

#endif
