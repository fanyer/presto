/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef THUMBNAIL_ICON_CHOOSER_H
#define THUMBNAIL_ICON_CHOOSER_H

#if defined(CORE_THUMBNAIL_SUPPORT)

#include "modules/logdoc/urlimgcontprov.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/link.h"

class ThumbnailIconFinder:
	public MessageObject
{
public:
	// Make friends so that we can access m_listener from within IconCandidate
	class IconCandidate;
	friend class IconCandidate;

	/**
	 * Used to gather information about the <LINK REL=...> images available in the document.
	 * Contains the following information:
	 *
	 * - core_url is the URL of the image,
	 * - width, height is the size of the image,
	 * - loaded is a flag that is set to TRUE when the URL has been loaded,
	 * - provider is the image content provider for the image for which the core_url points to.
	 */
	class IconCandidate:
		public ImageListener
	{
	public:
		/**
		 * @param url The URL that this icon will be downloaded from. Passing a NULL URL is not allowed.
		 */
		IconCandidate(URL url);

		~IconCandidate();

		// From ImageListener
		/**
		 * Called when a portion of the image has been decoded.
		 */
		virtual void OnPortionDecoded();

		/**
		 * Called when there was an error while decoding the image that the ImageListener is connected to.
		 * @param status OpStatus::ERR if decoding failed, OpStatus::ERR_NO_MEMORY if decoding failed because of we run out of memory.
		 */
		virtual void OnError(OP_STATUS status);

		/**
		 * Acuires an UrlImageContentProvider for the URL passed to constructor from UrlImageContentProvider::FindImageContentProvider().
		 * If no provider can be acquired this way, a new one is created for that URL.
		 * If that is not possible, the function fails.
		 * GetWidth() and GetHeight() will return correct values for the given URL only after this function call succeeded.
		 *
		 * @return OpStatus::OK if the prived was acquired, OpStatus::ERR if not.
		 */
		OP_STATUS SetProviderFromURL();

		/**
		 * Returns the width of this icon candidate. Returns a valid value only after a successful call to SetProviderFromURL() was done.
		 *
		 * @return Image width or -1 if a provider was not acquired.
		 */
		int	GetWidth();

		/**
		 * Returns the height of this icon candidate. Returns a valid value only after a successful call to SetProviderFromURL() was done.
		 *
		 * @return Image height or -1 if a provider was not acquired.
		 */
		int GetHeight();

		/**
		 * Returns the URL representing the icon candidate. This is the same URL that was passed to the constructor.
		 *
		 * @return URL reference.
		 */
		URL GetCoreURL() { return m_core_url; }

		/**
		 * Returns pointer to the internal bitmap representing the icon found.
		 *
		 * @return a pointer to the internal bitmap
		 */
		OpBitmap* GetBitmap() { return m_bitmap; }

		/**
		 * Starts decoding the icon image from the provider that we get from the URL for this candidate.
		 * Notifies the ThumbnailIconFinder pointed to by the parameter when decoding result is known.
		 *
		 * @param finder - pointer to the ThumbnailIconFinder that is to be notified about the decoding result
		 */
		void DecodeAndNotify(ThumbnailIconFinder* finder);

	private:
		URL m_core_url;
		UrlImageContentProvider* m_provider;

		Image m_image;
		OpBitmap* m_bitmap;

		ThumbnailIconFinder* m_finder;
	};

	/**
	 * This listener interface is usually implemented by OpThumbnail to be informed about the result of choosing an IconCandidate.
	 * An instance of the implementation of this class is specified on ThumbnailIconFinder::Init(). The listener
	 * instance needs to be alive at least as long as the associated ThumbnailIconFinder instance is alive.
	 *
	 * When a LogoCandidate is determined, OnIconCandidateChosen() is called. This is not done before all the icons
	 * that are found in a document are loaded and their size is known.
	 */
	class ThumbnailIconFinderListener
	{
	public:
		/**
		 * This method is called when it is known that ThumbnailIconFinder::GetBestCandidate() will return a good candidate.
		 * This call occurs when there are no suitable icon links in the document or when all suitable icons were loaded and
		 * their size is recognized. Failing to load an icon link also counts for marking the icon as "loaded".
		 */
		virtual void OnIconCandidateChosen(ThumbnailIconFinder::IconCandidate *best) = 0;
	};

	ThumbnailIconFinder();

	~ThumbnailIconFinder();

	/**
	 * Scans the document in the specified window for icon links. An icon is considered to be suitable for an speeddial icon when
	 * it is linked via any of the following HTML elements:
	 * <LINK REL="ICON" ...>
	 * <LINK REL="IMAGE_SRC" ...>
	 * <LINK REL="APPLE_TOUCH_ICON" ...>
	 *
	 * The document in the specified window is expected to be loaded so that calling Window::GetLinkDatabase() is possible.
	 * The specified listener is be notified about the scan being finished instantly if there are no suitable icons.
	 * In case there are icons that need loading the listener will be notified when all icons are loaded or fail to load.
	 *
	 * @param listener is the listerner implementation to notify when the icon sizes are known.
	 * @param window is the window that contains the document to be scanned for icon links.
	 * @param min_icon_width, min_icon_height are the minimal icon dimension for it to qualify for a "best icon" competition.
	 *
	 */
	OP_STATUS StartIconFinder(ThumbnailIconFinderListener *listener, Window *window, unsigned int min_icon_width, unsigned int min_icon_height);

	BOOL IsBusy();

	ThumbnailIconFinderListener* GetListener() { return m_listener; }

private:
	/**
	 * Finds the "best" link icon among those available in the m_candidates
	 * vector. Only gives a valid result when called after the OnIconCandidateChosen() callback was made.
	 * The choice for the best candidate is based on the size of the loaded images.
	 *
	 * If no suitable candidate can be found, NULL is returned. In this case the HTML document should be used
	 * for generating the thumbnail.
	 *
	 * @return A pointer to the best LinkIconData element or NULL if a best element can't be found. The
	 *         pointer is valid through the ThumbnailIconFinder lifetime.
	 */
	IconCandidate* GetBestCandidate();

	/**
	 * ScanDocForCandidates()
	 *
	 * Iterates all possible icon images that are linked in the document and saves their URLs in the
	 * m_candidates vector. A possible icon image for a page is an image linked by any of the
	 * following appearing in the page header:
	 * <LINK REL="ICON" ...>
	 * <LINK REL="IMAGE_SRC" ...>
	 * <LINK REL="APPLE_TOUCH_ICON" ...>
	 *
	 * The m_candidates vector is populated with the result of the scan, so that
	 * it contains one element per URL representing a link. The width/size attributes
	 * of the IconCandidate are initialized with value -1 and will be updated with the
	 * icon's size when the icon's URL has been loaded.
	 */
	OP_STATUS ScanDocForCandidates();

	/**
	 * Will add or remove this ThumbnailIconFinder as a message handler for URL loading.
	 */
	OP_STATUS AddCallBacksForUrlID(URL_ID id);
	void RemoveCallBacksForUrlID(URL_ID id);

	/**
	 * From MessageObject
	 */
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * Holds descriptions for <LINK REL=..> elements that we found while scanning the document
	 * that we're generating the thumbnail for and that one of might contain an icon image
	 * that we'd use instead of the page content.
	 */
	OpVector<IconCandidate> m_candidates;

	Window *m_window;

	/**
	 * Indicates that this ThumbnailIconFinder is busy downloading URLs for the images found.
	 */
	BOOL	m_is_busy;
	INT32	m_min_icon_width;
	INT32	m_min_icon_height;
	/**
	 * The number of URLs that are still waiting to download/timeout/fail.
	 */
	UINT32	m_pending_link_count;

	ThumbnailIconFinderListener* m_listener;
};
#endif // defined(CORE_THUMBNAIL_SUPPORT)
#endif // THUMBNAIL_ICON_CHOOSER_H
