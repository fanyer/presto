/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARKS_SD_MANAGER_H
#define BOOKMARKS_SD_MANAGER_H

#ifdef CORE_SPEED_DIAL_SUPPORT

#include "modules/bookmarks/bookmark_manager.h"

#include "modules/bookmarks/operaspeeddial.h"
#include "modules/bookmarks/speeddial.h"
#include "modules/bookmarks/speeddial_listener.h"

#define SPEEDDIAL_ROWS			3
#define SPEEDDIAL_COLS			3
#define NUM_SPEEDDIALS			SPEEDDIAL_ROWS * SPEEDDIAL_COLS

class SpeedDialManager : public BookmarkManagerListener
{
public:	
	SpeedDialManager();
	~SpeedDialManager();
	
	OP_STATUS	Init();
	
	/** Set the url of a speed dial */
	OP_STATUS	SetSpeedDial(int index, const uni_char* url);

	/** Reloads the specified speed dial */
	OP_STATUS	ReloadSpeedDial(int index);
	
	/** Swap the location of two speed dials */
	OP_STATUS	SwapSpeedDials(int id1, int id2);
	
	/** 
	 * Set the reload interval of a speed dial.
	 * 
	 * @param index The speed dial in question.
	 * @param seconds Reload interval in seconds.
	 */
	OP_STATUS	SetSpeedDialReloadInterval(int index, int seconds);

	/** The position of a speed dial */
	int GetSpeedDialPosition(BookmarkItem *speed_dial) const;
	
	/** Get the speed dial in a certain position */
	SpeedDial* GetSpeedDial(int position) const;

	BOOL HasOpenPages() const { return m_speed_dial_pages > 0; }

	OP_STATUS	AddSpeedDialListener(OpSpeedDialListener *listener);
	void		RemoveSpeedDialListener(OpSpeedDialListener *listener);		

	// From BookmarkManagerListener
	virtual void OnBookmarksSaved(OP_STATUS ret, UINT32 operation_count);
	virtual void OnBookmarksLoaded(OP_STATUS ret, UINT32 operation_count);
#ifdef SUPPORT_DATA_SYNC
	virtual void OnBookmarksSynced(OP_STATUS ret);
#endif // SUPPORT_DATA_SYNC
	virtual void OnBookmarkAdded(BookmarkItem *bookmark) { }

private:
	OP_STATUS InitBookmarkStorage();
	
	BookmarkItem*			m_speed_dial_folder;	
	OpAutoVector<SpeedDial>	m_speed_dials;

	unsigned int			m_speed_dial_pages;

#ifdef OPERASPEEDDIAL_URL
	OperaSpeedDialURLGenerator m_url_generator;
#endif
};

#define g_speed_dial_manager g_opera->bookmarks_module.m_speed_dial_manager

#endif // CORE_SPEED_DIAL_SUPPORT

#endif // BOOKMARKS_SD_MANAGER_H
