/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARKS_SPEEDDIAL_H
#define BOOKMARKS_SPEEDDIAL_H

#ifdef CORE_SPEED_DIAL_SUPPORT

#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/speeddial_listener.h"
#include "modules/hardcore/timer/optimer.h"

#include "modules/util/adt/oplisteners.h"

#ifdef SUPPORT_GENERATE_THUMBNAILS
#include "modules/thumbnails/thumbnailmanager.h"
#endif // SUPPORT_GENERATE_THUMBNAILS

/**
 * Representation of a single speed dial item.
 */ 
class SpeedDial 	
	: public OpTimerListener
#ifdef SUPPORT_GENERATE_THUMBNAILS
	, public ThumbnailManagerListener
#endif
	, public BookmarkItemListener
{
public:
	SpeedDial(unsigned int index);
	~SpeedDial();

	/** Set the URL of a speed dials, the title and thumbnail will be updated if needed */
	OP_STATUS		SetSpeedDial(const uni_char* url
#ifdef SUPPORT_DATA_SYNC
								 , BOOL from_sync = FALSE
#endif // SUPPORT_DATA_SYNC
		);
		
	void			SetBookmarkStorage(BookmarkItem* storage);

	/** Set the position of the speed dial */
	void			SetPosition(int pos) { m_index = pos; }
	
	/** Get the position of this speed dial */
	int				GetPosition() const { return m_index; }
	
	/**
	 * Called whenever a speed dial is modified. Responsible for updating the speed dial
	 * and notifying the front end.
	 *
	 * @param old_url The previous URL, an empty URL object if none.
	 * @param check_if_expired Reload the thumbnail and title if the page has expired.
	 * @param reload Force a reload of thumbnail and title.
	 * @param from_sync TRUE if the speed dial was modified by a sync action. If FALSE
	 *                  the speed dial will try to sync itself.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS		SpeedDialModified(URL old_core_url, BOOL check_if_expired, BOOL reload
#ifdef SUPPORT_DATA_SYNC
									  , BOOL from_sync = FALSE
#endif // SUPPORT_DATA_SYNC
		);
#ifdef SUPPORT_DATA_SYNC
	OP_STATUS SpeedDialSynced();
#endif // SUPPORT_DATA_SYNC
	
	/** Refresh the speed dial thumbnail */
	OP_STATUS		Reload();

#ifdef SUPPORT_GENERATE_THUMBNAILS
	/** Start loading the speed dial preview */
	OP_STATUS		StartLoading(BOOL reload);

	/** Stop loading the speed dial preview */
	OP_STATUS		StopLoading();
#endif // SUPPORT_GENERATE_THUMBNAILS

	/** 
	 * Set the speed dial reload policy.
	 * 
	 * @param enable Enable reloading.
	 * @param timeout Reload timeout in seconds.
	 */
	OP_STATUS		SetReload(BOOL enable, int timeout);

	/**
	 * Fetches URL string that represents the page that was used to generate the thumbnail for this SpeedDial.
	 *
	 * @param url_string an OpString reference that will be filled with the desired URL string.
	 * @return OpStatus::OK on success, ERR otherwise.
	 */
	OP_STATUS		GetUrl(OpString &url) const;
	OP_STATUS		GetTitle(OpString &title) const;
	const uni_char* GetThumbnail() const { return m_thumbnail_url.CStr(); }
	
	/** Is the speed dial considered empty */
	BOOL			IsEmpty() const;

	/** Is the speed dial currently loading */
	BOOL			IsLoading() const { return m_loading; }
	
	/** Clear the contents of the speed dial */
	void			Clear(
#ifdef SUPPORT_DATA_SYNC
		BOOL from_sync = FALSE
#endif // SUPPORT_DATA_SYNC
		);
	
	/** Add a speed dial listener */
	void			AddListener(OpSpeedDialListener* l);

	/** Remove a speed dial listener */
	void			RemoveListener(OpSpeedDialListener* l);
	
	// From OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

#ifdef SUPPORT_GENERATE_THUMBNAILS
	// From ThumbnailManagerListener
	virtual void OnThumbnailRequestStarted(const URL &document_url, BOOL reload);
	virtual void OnThumbnailReady(const URL &document_url, const Image &thumbnail, const uni_char *title, long preview_refresh);
	virtual void OnThumbnailFailed(const URL &document_url, OpLoadingListener::LoadingFinishStatus status);
	virtual void OnInvalidateThumbnails() {}
#endif // SUPPORT_GENERATE_THUMBNAILS

	// From BookmarkItemListener
	virtual void OnBookmarkDeleted();
	virtual void OnBookmarkChanged(BookmarkAttributeType attr_type, BOOL moved) { }
	virtual void OnBookmarkRemoved() { }

	/** Called when a speed dial has been swapped with another one */
	OP_STATUS		NotifySpeedDialSwapped();

	BookmarkItem*	GetStorage() const { return m_storage; }

private:
	/**
	 * Checks if the URL passed as parameter is the same URL that was used to load the page that was used to generate
	 * the thumnbnail represented by this SpeedDial. This method checks if the Id() of the given URL is the same as
	 * m_core_url.Id().
	 *
	 * @param document_url The URL to be checked against m_core_url.
	 * @return TRUE if this SpeedDial was generated using the URL passed as the paremeter, FALSE otherwise.
	 */
	BOOL			MatchingDocument(const URL &document_url) const;

	OP_STATUS		SetTitle(const uni_char* title);	
#ifdef SUPPORT_GENERATE_THUMBNAILS
	OP_STATUS		SetThumbnail(const uni_char* thumbnail_url) { return m_thumbnail_url.Set(thumbnail_url); }
#endif

	OP_STATUS		NotifySpeedDialRemoved();
	OP_STATUS		NotifySpeedDialChanged();

	OP_STATUS		StartTimer(int timeout);
	void			StopTimer();

	/**
	 * Sets the m_core_url to the proper unique core URL object that from now on represents the address that is
	 * used to generate the thumbnail image for this SpeedDial. Note, that passing an URL string here causes the
	 * URL to be resolved via the URL API and the URL object requested from the URL API basing on the resolved string
	 * is unique and has the URL::KStoreAllHeaders attribute set in order to be inline with the thumbnails module
	 * that requires both changes to be able to serve the "view-mode: minimized" CSS mode and the "X-Purpose: Preview"
	 * HTTP header correctly while maintaing the proper URL caching.

	 * NOTE that this method will also call SetStorageURLString() in order to have the two URL representations in sync.
	 * NOTE that it is not advised to manipulate the m_core_url directly.
	 *
	 * @param url_string The URL string that will be used when setting the m_core_url.
	 * @return OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS		SetCoreURL(const OpStringC& url_string);

	/**
	 * Returns a copy of m_core_url. Note that it is not possible to get the very same URL in any other way and that
	 * this is the only way to fetch the very same URL that was used to load the page used to generate the thumbnail
	 * displayed by this SpeedDial.
	 *
	 * @returns A copy of m_core_url.
	 */
	URL				GetCoreURL() const;

	/**
	 * Sets the m_core_url to an empty URL.
	 */
	void			EmptyCoreURL();

	/**
	 * Sets the BOOKMARK_URL attribute of the m_storage BookmarkItem.
	 * Note that while it is not the best solution, but we store the URL representing the source of the thumbnail
	 * for this SpeedDial twice - once within the BookmarkItem as text, that is needed to keep the URL persistent
	 * along browser sessions, and once with the m_core_url since the URL object has to be requested once via the
	 * URL API and then kept for further reference since the callbacks from the thumbnail manager will be made
	 * with this very URL that is - most probably - not possible to fetch again from the URL API since it is
	 * unique.
	 *
	 * @param url_string The URL string that will be stored in m_storage.
	 * @return OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS		SetStorageURLString(const OpStringC& url_string);

	/**
	 * Returns the URL string that is stored in the BOOKMARK_URL attribute of the m_storage BookmarkItem.
	 * See SetStorageURLString() description to know more.
	 *
	 * @param url_string The OpString reference that will be set using the URL stored in m_storage.
	 * @return OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS		GetStorageURLString(OpString& url_string) const;

	unsigned int m_index;
	BOOL m_loading;

	/**
	 * This is the core URL object that represents the URL that is used to generate the thumbnail displayed by this
	 * SpeedDial.
	 * NOTE that this URL should only be set via SetCoreURL() and EmptyCoreURL().
	 */
	URL m_core_url;

	/**
	 * This is the string that is representing the "file://" URL of the thumbnail image in the local disk storage.
	 * This speeddial implementation doesn't do blitting from the Image object passed from the thumbnail manager,
	 * but rather displays the thumbnail image PNG as found in the thumbnail disk cache.
	 */
	OpString m_thumbnail_url;
	OpTimer *m_reload_timer;
	unsigned int m_reload_timeout;

	BookmarkItem* m_storage;

	OpListeners<OpSpeedDialListener> m_listeners;
};

#endif // CORE_SPEED_DIAL_SUPPORT

#endif // BOOKMARKS_SPEEDDIAL_H
