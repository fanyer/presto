/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
* @author Owner: Deepak Arora (deepaka) & Karianne Ekern (karie)
*/

#ifndef COLLECTION_THUMBNAIL_MANAGER
#define COLLECTION_THUMBNAIL_MANAGER

#include "adjunct/quick/widgets/CollectionViewPane.h"

#include "modules/hardcore/mh/messobj.h"
#include "modules/thumbnails/thumbnailmanager.h"

#define MAX_ZOOM								2.0
#define MIN_ZOOM								0.3
#define DEFAULT_ZOOM							1.0
#define SCALE_DELTA								0.1

class ViewPaneController;

/**
 * @brief This class handles thumbnail generate and cancel requests.
 * Also this class informs/notifies the result of thumbnail generate
 * request to CollectionViewPane.
 *
 * The mechanism on which this is based relies on message passing.
 * Reason behind using message passing is to have non-blocking,
 * seamless operations likes scrolling, deleting, editing, etc. on
 * items hosted by CollectionViewPane.
 */

class CollectionThumbnailGenerator
	: private MessageObject
{
public:
	enum ThumbnailStatus
	{
		STATUS_STARTED,
		STATUS_SUCCESSFUL,
		STATUS_FAILED,
	};

	 CollectionThumbnailGenerator(CollectionViewPane&);
	~CollectionThumbnailGenerator();

	/** Initializes thumbnail generator.
	  * @returns OpStatus::OK is successfully initialized other wise error
	  */
	OP_STATUS Init();

	/** Updates width and height states of thumbnail
	  */
	void UpdateThumbnailSize(double zoom_factor)
	{
		m_thumbnail_width  = m_base_width * zoom_factor;
		m_thumbnail_height = m_base_height * zoom_factor;
	}

	/** Fetches thumbnail size.
	  * @param[out] target_width width of thumbnail placeholder
	  * @param[out] target_height height of thumbnail placeholder
	  */
	void GetThumbnailSize(unsigned int& width, unsigned int& height) const { width = m_thumbnail_width ; height = m_thumbnail_height; }

	/** Initiates thumbnail requests
	  * @param item_id identifies id of item/placeholder associated with thumbnail
	  * @param reload identifies whether thumbnail should be reloaded
	  */
	void GenerateThumbnail(INT32 item_id, bool reload = false);

	/** Cancels requested thumbnail generation
	  * @param item_id identifies id of item associated with thumbnail
	  */
	void CancelThumbnail(INT32 item_id) { DeleteThumbnail(item_id); }

	/** Cancels all thumbnail requests.
	  */
	void CancelThumbnails();

private:
	class NullImageListener
		: public ImageListener
	{
		virtual void OnPortionDecoded() { }
		virtual void OnError(OP_STATUS status) { }
	};

	class CollectionThumbnailListener
		: public ThumbnailManagerListener
		, public ImageListener
	{
	public:
		CollectionThumbnailListener(CollectionThumbnailGenerator& generator, INT32 id, URL url, bool reload = false);
		~CollectionThumbnailListener();
		OP_STATUS					Initiate();
		URL&						GetURL() { return m_url; }

		// ThumbnailManagerListener
		virtual void				OnThumbnailRequestStarted(const URL& url, BOOL reload);
		virtual void				OnThumbnailReady(const URL& url, const Image& thumbnail, const uni_char *title, long preview_refresh);
		virtual void				OnThumbnailFailed(const URL& url, OpLoadingListener::LoadingFinishStatus status);
		virtual void				OnInvalidateThumbnails() { }
		bool						HasRequestInitiated() { return m_req_initiated; }
		bool						HasRequestProcessed() { return m_req_processed; }

	private:
		// ImageListener
		virtual void				OnPortionDecoded();
		virtual void				OnError(OP_STATUS status) {}
		OP_STATUS					GenerateThumbnail();

		CollectionThumbnailGenerator& m_thumbnail_generator;
		Image						m_image;
		URL							m_url;
		INT32						m_id;
		bool						m_req_initiated;
		bool						m_req_processed;
		bool						m_reload_thumbnail;
	};

	// MessageObject
	virtual void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void					AddIntoGenerateList(INT32 item_id, bool reload);
	void					NotifyThumbnailStatus(ThumbnailStatus status, INT32 id, Image& thumbnail, bool has_fixed_image = false);
	void					GenerateThumbnails();
	void					RemoveProcessedThumbnailRequest();
	void					DeleteThumbnail(INT32 item_id);
	void					DeleteAllThumbnails();
	void					RemoveMessages();
	bool					IsSpecialURL(URL& url);
	bool					GetFixedImage(URL& url, Image& image_name) const;
	const char*				GetSpecialURIImage(URLType type) const;

	CollectionViewPane&										m_collection_view_pane;
	OpINT32HashTable<CollectionThumbnailListener>			m_item_id_to_thumbnail_listener;
	OpINT32Vector											m_thumbnail_req_processed_item_list;
	unsigned int											m_num_of_req_initiated;
	NullImageListener										m_null_img_listener;
	const int												m_base_height;
	const int												m_base_width;
	unsigned int											m_thumbnail_width;
	unsigned int											m_thumbnail_height;

};

#endif //COLLECTIONS_THUMBNAIL_MANAGER
