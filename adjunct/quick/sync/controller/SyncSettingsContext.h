/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef SYNC_SETTINGS_CONTEXT_H
#define SYNC_SETTINGS_CONTEXT_H

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/controller/FeatureSettingsContext.h"

#include "modules/sync/sync_state.h"

/***********************************************************************************
**  @class	SyncSettingsContext
**	@brief	Settings that can be specified for sync.
************************************************************************************/
class SyncSettingsContext : public FeatureSettingsContext
{
public:
	SyncSettingsContext();
	virtual ~SyncSettingsContext() {}

	virtual FeatureType			GetFeatureType() const;
	virtual Str::LocaleString	GetFeatureStringID() const;
	virtual Str::LocaleString	GetFeatureLongStringID() const;

	// getters
	BOOL	SupportsBookmarks() const;
	BOOL	SupportsSpeedDial() const;
	BOOL	SupportsPersonalbar() const;
#ifdef SUPPORT_SYNC_NOTES
	BOOL	SupportsNotes() const;
#endif // SUPPORT_SYNC_NOTES
#ifdef SYNC_TYPED_HISTORY
	BOOL	SupportsTypedHistory() const;
#endif // SYNC_TYPED_HISTORY
#ifdef SUPPORT_SYNC_SEARCHES
	BOOL	SupportsSearches() const;
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	BOOL	SupportsURLFilter() const { return m_supports_urlfilter; }
#endif // SYNC_CONTENT_FILTERS
	BOOL    SupportsPasswordManager() const;

	// Generic getter
	BOOL    GetSupportsType(OpSyncSupports supports_type);

	// setters
	void	SetSupportsBookmarks(BOOL supports_bookmarks);
	void	SetSupportsSpeedDial(BOOL supports_speeddial);
	void	SetSupportsPersonalbar(BOOL supports_personalbar);
#ifdef SUPPORT_SYNC_NOTES
	void	SetSupportsNotes(BOOL supports_notes);
#endif // SUPPORT_SYNC_NOTES
#ifdef SYNC_TYPED_HISTORY
	void	SetSupportsTypedHistory(BOOL supports_typed_history);
#endif // SYNC_TYPED_HISTORY
#ifdef SUPPORT_SYNC_SEARCHES
	void	SetSupportsSearches(BOOL supports_searches);
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	void	SetSupportsURLFilter(BOOL supports_urlfilter) { m_supports_urlfilter = supports_urlfilter; }
#endif // SYNC_CONTENT_FILTERS
	void	SetSupportsPasswordManager(BOOL supports_password_manager);

	// generic setter
	void	SetSupportsType(OpSyncSupports supports_type, BOOL supported);

private:
	BOOL	m_supports_bookmarks;
	BOOL	m_supports_speeddial;
	BOOL	m_supports_personalbar;
#ifdef SUPPORT_SYNC_NOTES
	BOOL	m_supports_notes;
#endif // SUPPORT_SYNC_NOTES
#ifdef SYNC_TYPED_HISTORY
	BOOL	m_supports_typed_history;
#endif // SYNC_TYPED_HISTORY
#ifdef SUPPORT_SYNC_SEARCHES
	BOOL	m_supports_searches;
#endif // SUPPORT_SYNC_SEARCHES
#ifdef SYNC_CONTENT_FILTERS
	BOOL	m_supports_urlfilter;
#endif // SYNC_CONTENT_FILTERS
	BOOL	m_supports_password_manager;
};

#endif // SUPPORT_DATA_SYNC

#endif // SYNC_SETTINGS_CONTEXT_H
