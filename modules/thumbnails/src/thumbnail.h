/** -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef THUMBNAIL_H
#define THUMBNAIL_H

#ifdef CORE_THUMBNAIL_SUPPORT

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/dochand/fdelm.h"
#include "modules/img/image.h"

#include "modules/thumbnails/src/thumbnaillogofinder.h"
#include "modules/thumbnails/src/thumbnailiconfinder.h"

#define THUMBNAIL_DECODE_POLL_MAX_ATTEMPTS	50
#define THUMBNAIL_DECODE_POLL_TIMEOUT		250
#define THUMBNAIL_AUTH_TIMEOUT				100
/*
This is the width of the internal invisible window in which the thumbnails will be
rendered. Note that this will change for view-mode: minimized thumbnails in order to
have the content rendered in a smaller window.
The thumbnail height will be calculated basing on the target thumbnail aspect ratio.
*/
#define THUMBNAIL_INTERNAL_WINDOW_WIDTH		1024
/**
 * Page thumbnail renderer
 *
 * If you want to use this class in a self-updating UI widget,
 * subclass it and override the OnXXX methods.
 *
 * Alternatively, you can use the static method CreateThumbnail to
 * make a one-off snapshot of an existing window.
 */
class OpThumbnail
	: public OpLoadingListener,
	  public OpTimerListener,
	  // We load icon candidates using URL objects that signal back to the OpThumbnail.
	  public ThumbnailIconFinder::ThumbnailIconFinderListener
{
public:
	/** Mode in which a thumbnail is generated */
	enum ThumbnailMode
	{
		ViewPort,			//!< Standard mode; a miniature of the viewport
		FindLogo,			//!< Use the "find logo" algorithm
		ViewPortLowQuality,	//!< Like ViewPort, but scale the thumbnail by scaling the painter instead of scaling a bitmap - faster
		SpeedDial,			//!< Will scan the document for icons and use an icon as the thumbnail if an appropriate one is found
		IconThumbnail,		//!< Will size the icon so that it fits the thumbnail best
		MinimizedThumbnail, //!< View-mode: minimized thumbnail
		SpeedDialRefreshing //!< No smart content besides view-port: minimized
	};

    /**
     *
     */
    OpThumbnail();

    /**
     *
     */
    virtual ~OpThumbnail();

    /**
     * Starts a thumbnail creation.
     * Will aquire an invisible ui window, open a url and wait for loading to finish.
     * Use the ThumbnailMode::SpeedDial mode to generate a speeddial image by looking at the icon and logo candidates
     * for the URL requested.
     *
     * @param window		- A window used to render the page. This window can be invisible.
     * @param url			- The url of the page to thumbnail
     * @param source_width  - The width in screen coords of the page to thumbnail
	 * @param source_height	- The height in screen coords of the page to thumbnail
     * @param flags			-
	 * @param find_logo		- Use the find logo algorithm to generate the thumbnail
	 * @param suspend		- Do not immediately start to load the document, if
	 *						  suspended the OpThumbnail needs to be started by calling
	 *						  Start()
	 * @param target_width	- The desired width of the thumbnail, in pixels
	 * @param target_height - The desired height of the thumbnail, in pixels
	 * @return				- Possible memory error.
     */
	OP_STATUS Init(Window * window,
				   const URL url,
				   int source_width,
				   int source_height,
				   int flags,
				   ThumbnailMode mode = OpThumbnail::ViewPort,
				   BOOL suspend = FALSE,
				   int target_width = THUMBNAIL_WIDTH,
				   int target_height = THUMBNAIL_HEIGHT
				   );
	/**
	 * Starts a suspended thumbnail.
	 */
	OP_STATUS Start();

    /**
     * Stops a thumbnail creation.
     * If stop is not called, the thumbnail will go on creating thumbnail images for every refresh of the page.
     *
     * @return status. Possible memory error.
     */
    OP_STATUS Stop();

    /**
	 * Scale down a bitmap.
	 *
	 * @param bitmap Original bitmap.
	 * @param target_width Width of the target bitmap.
	 * @param target_height Height of the target bitmap.
	 * @param dst Target rectangle; Where and in which scale on the target bitmap the
	 *            source should be drawn.
	 * @param src Source rectangle; The fragment of bitmap to draw. Or
	 *            { 0, 0, width, height } for the whole bitmap.
	 */
	static OpBitmap* ScaleBitmap(OpBitmap* bitmap, int target_width, int target_height, const OpRect& dst, const OpRect& src);

	/**
     * @param mybuff     -
	 * @param result_len -
     * @param nodown     -
	 * @return
     */
    static char* EncodeImagePNG(OpBitmap *mybuff,
								int &result_len,
								int nodown );

    /**
	 * Render a thumbnail of the upper-left corner of the window (synchronously)
	 *
     * @param window        - The window to render a thumbnail for
	 * @param target_width  - Width of the thumbnail
	 * @param target_height - Height of the thumbnail
	 * @param auto_width	- Try to fit the width of the page content into the
	 *                        thumbnail.
	 * @param mode          - Rendering mode.
	 * @param keep_vertical_scroll - Whether the thumbnail should be rendered
	 *						  from the current scroll position or the top of
	 *						  the page. Does nothing in 'find logo'-mode.
	 * @return A bitmap containing the thumbnail or NULL in case of failure
     */
    static OpBitmap *CreateThumbnail(Window *window,
									 long target_width,
									 long target_height,
									 BOOL auto_width,
									 ThumbnailMode mode = OpThumbnail::ViewPort,
									 BOOL keep_vertical_scroll = FALSE
									 );

    /**
	 * Render a thumbnail of a selected area of the window (synchronously)
	 *
     * @param window        - The window to render a thumbnail for
	 * @param target_width  - Width of the thumbnail
	 * @param target_height - Height of the thumbnail
	 * @param area          - the area of the window to render into a thumbnail
	 * @param high_quality  - Whether thumbnail should be rendered by painting full-size
	                          and scaling down (high quality) or just by painting thumbnail-size
	                          (low quality, faster)
	 * @return A bitmap containing the thumbnail or NULL in case of failure
     */
	static OpBitmap* CreateThumbnail(Window *window,
									 long target_width,
									 long target_height,
									 const OpRect& area,
									 BOOL high_quality = TRUE);

	/**
	* Render a snapshot of a complete window (synchronously)
	*
	* @param window        - The window to render a thumbnail for
	* @return A bitmap containing the snapshot or NULL in case of failure
	*/
	static OpBitmap* CreateSnapshot(Window *window);

	/**
	 * Get the title of the thumbnail. Not valid until the document has loaded
	 * successfully i.e. in the first OnFinished() call or later.
	 */
	const uni_char* GetTitle() const { return m_title.CStr(); }

	/**
	 * @name Implementation of OpLoadingListener
	 *
	 * This class implements the OpLoadingListener to be able to handle the
	 * OnLoadingFinished(), OnAuthenticationRequired() and OnLoadingFailed()
	 *
	 * @{
	 */

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif
	virtual void OnUrlChanged(OpWindowCommander* commander, const uni_char* url);
	virtual void OnStartLoading(OpWindowCommander* commander) {}
	virtual void OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) {}
	virtual void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
#ifdef URL_MOVED_EVENT
	virtual void OnUrlMoved(OpWindowCommander* commander, const uni_char* url) {}
#endif // URL_MOVED_EVENT
	virtual void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);
	virtual void OnStartUploading(OpWindowCommander* commander) {}
	virtual void OnUploadingFinished(OpWindowCommander* browser_view, OpLoadingListener::LoadingFinishStatus status) {}
	virtual BOOL OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url);
#ifdef EMBROWSER_SUPPORT
	virtual void OnUndisplay(OpWindowCommander* commander) {}
	virtual void OnLoadingCreated(OpWindowCommander* commander) {}
#endif // EMBROWSER_SUPPORT
	virtual void OnXmlParseFailure(OpWindowCommander* commander) {}
#ifdef XML_AIT_SUPPORT
	virtual OP_STATUS OnAITDocument(OpWindowCommander*, AITData*) { return OpStatus::OK; }
#endif // XML_AIT_SUPPORT

	/** @} */ // Implementation of OpLoadingListener

	/**
	 * @name Implementation of OpTimerListener
	 * @{
	 */
	virtual void OnTimeOut(OpTimer* timer);
	/** @} */ // Implementation of OpTimerListener

	static OpRect FindDefaultArea(Window* window, long target_width, long target_height, BOOL auto_width, BOOL keep_vertical_scroll = FALSE);

	// From ThumbnailIconFinder::ThumbnailIconFinderListener
	void OnIconCandidateChosen(ThumbnailIconFinder::IconCandidate *best);

	enum SettingValue {
		LOGO_SCORE_THRESHOLD,
		LOGO_SCORE_X,
		LOGO_SCORE_Y,
		LOGO_SCORE_LOGO_URL,
		LOGO_SCORE_LOGO_ALT,
		LOGO_SCORE_SITE_URL,
		LOGO_SCORE_SITE_ALT,
		LOGO_SCORE_SITE_LINK,
		LOGO_SCORE_BANNER,
		LOGO_SIZE_MIN_X,
		LOGO_SIZE_MIN_Y,
		LOGO_SIZE_MAX_X,
		LOGO_SIZE_MAX_Y,
		LOGO_POS_MAX_X,
		LOGO_POS_MAX_Y,
		LOGO_ICON_MIN_W,
		LOGO_ICON_MIN_H
	};

	static int GetSettingValue(SettingValue which);

	static OpRect FindArea(Window* window, long target_width, long target_height, BOOL auto_width, ThumbnailMode mode, long source_width = 0, long source_height = 0, OpRect* logo_rect = NULL);

#ifdef THUMBNAILS_LOGO_FINDER
	/**
	 * Find a rectangle of the given size in a window that is likely to contain
	 * the logo of the page. If no logos are found, this returns a rectangle of
	 * the given size in the top-left corner of the page.
	 */
	static OpRect FindLogoRect(Window *window, long width, long height);
#endif

	static BOOL DocumentOk(FramesDocument* frames_doc);

protected:
	/** Preview refresh is a HTTP header attribute and/or a HTML META tag.
	 * If both exists, the meta tag is preferred. Measuring unit is seconds.
	 */
	long GetPreviewRefresh() const { return m_preview_refresh; }

    /**
     *
     */
    OpBitmap *ReleaseBuffer(OpBitmap *buffer);

private:
	/** Used for parsing the HTTP header and META tag to find preview refresh. */
	OP_STATUS SetPreviewRefresh();

	/** Used for temporary storage during PNG encoding */
	class ImageBuffer
	{
	public:
		ImageBuffer() : m_buffer_size(0), m_actual_size(0), m_image_buffer(0) {}
		~ImageBuffer() { OP_DELETEA(m_image_buffer); }

		/** Preallocate a given amount of data, will reset the internal state of the object */
		OP_STATUS Reserve(unsigned int size);

		/** Append data to the image buffer, grows the buffer if needed */
		OP_STATUS Append(const unsigned char* data, unsigned int data_size);

		/** Leaves ownership of the internal buffer to the caller, the internal state of the object is wiped */
		unsigned char* TakeOver();

		/** The amount of data stored in the buffer (not the amount allocated) */
		unsigned int ActualSize() const { return m_actual_size; }

	private:
		unsigned int	m_buffer_size;
		unsigned int	m_actual_size;
		unsigned char*	m_image_buffer;
	};

	/**
     * Callback when thumbnail is created
     *
     * @param buffer OpBitmap containing thumbnail of the page.
     */
    virtual void OnFinished(OpBitmap * buffer) = 0;

	virtual void OnCleanup() {}

    /**
     *
     */
    virtual void OnError(LoadingFinishStatus status) {};

	// Internal helper functions
	struct RenderData
	{
		RenderData() :
			zoomed_width(0), zoomed_height(0),
			target_width(0), target_height(0),
			left(0),         top(0),
			offset_x(0),     offset_y(0)
		{}

		int zoomed_width, zoomed_height;
		int target_width, target_height;
		int left,         top;
		int offset_x,     offset_y;

		OpRect logo_rect;
	};

	static OP_STATUS CalculateRenderData(Window* window, long target_width, long target_height, OpRect area, RenderData &rd, BOOL high_quality, BOOL shrink_width_if_needed = TRUE);

	/**
	 * Computes the RenderData for an image content, used for generating a thumbnail from an icon.
	 * Centers the icon using white padding of needed size on needed edges. See implemenetation for details.
	 *
	 * @param target_width, target_height The px size of the thumbnail that is to be generated. Both dimensions must be greater than 0.
	 * @param area The px size of the icon found to be suitable to represent this document in it's thumbnail. The area cannot be Empty().
	 * @rd The RenderData calculated by this function.
	 *
	 * @return Always returns OpStatus::OK.
	 */
	OP_STATUS CalculateMinimizedRenderData(long target_width, long target_height, RenderData &rd);

	static void Paint(Window *window, OpPainter *painter, RenderData &rd, BOOL dry_paint);
	static void Paint(Window *window, OpPainter *painter, INT32 width, INT32 height);

    /**
     * Paints the thumbnail and returns the generated bitmap. In case
     * of an error NULL is returned.
     *
     * m_render_data is expected to contain the data about the region
     * to paint:
     * - m_render_data.target_width and m_render_data.target_height
     *   specify the size of the generated bitmap.
     * - m_render_data.zoomed_width and m_render_data.zoomed_height
     *   specifies the area of the window to be painted (see also
     *   Paint()).
     * If the target_width is not equal to the zoomed_width, then the
     * bitmap is scaled to the specified target (see ScaleBitmap()).
     *
     * If THUMBNAILS_PREALLOCATED_BITMAPS is used and there is a
     * pre-allocated bitmap in m_preallocated_bitmap which has the
     * same size as the m_render_data.zoomed_width,
     * m_render_data.zoomed_height, then that bitmap is used for
     * painting (instead of allocating a new bitmap).
     *
     * If THUMBNAILS_PREALLOCATED_BITMAPS is used and the resulting
     * bitmap should be scaled (i.e. m_render_data.zoomed_width is not
     * equal to m_render_data.target_width), then OnCleanup() is
     * called to allow ThumbnailManager::OnThumbnailCleanup() to free
     * some memory before the scaled bitmap is allocated. In this case
     * also the m_window is set to 0.
     *
     * This method is called by OnTimeOut() when the m_generate_timer
     * is triggered.
     */
	OpBitmap* RenderThumbnailInternal();

    /**
     * Paints the thumbnail to a dummy (empty) bitmap and discards the
     * resulting bitmap. This allows to poll for non-decoded images
     * after the call.
     *
     * This method is called by OnTimeOut() when the m_generate_timer
     * is triggered.
     */
    void RenderThumbnailDryPaint();

	void GenerateThumbnailAsync();

    /**
     * This enum is used by m_state to describe the state of a
     * thumbnail:
     *
     * - The constructor initializes m_state with THUMBNAIL_INITIAL.
     * - Init() changes the state from THUMBNAIL_INITIAL to
     *   THUMBNAIL_STARTED (even if thumbnail generation is
     *   initialized as suspended).
     * - Init() sets the window's OpLoadingListener to this and
     *   Start() starts the m_load_timer. Either the load-timer or the
     *   OpLoadingListener::OnLoadingFinished() can trigger a call to
     *   GenerateThumbnailAsync().
     * - GenerateThumbnailAsync() starts the m_generate_timer (with
     *   timeout 0, so the timer is handled in the next message loop).
     * - On handling the m_generate_timer event (see OnTimeOut()), the
     *   state is changed from THUMBNAIL_STARTED (or
     *   THUMBNAIL_FINISHED if the thumbnail is re-generated) to
     *   THUMBNAIL_ACTIVE.
     * - In state THUMBNAIL_ACTIVE the instance waits for the document
     *   to be loaded and all images to be decoded. If this is the
     *   case and the thumbnail is rendered, the state is changed to
     *   THUMBNAIL_FINISHED.
     * - Stop() changes the state to THUMBNAIL_STOPPED. After stopping
     *   the thumbnail generation, it can be started again by calling
     *   Init().
     */
	enum ThumbnailState
	{
        /**
         * The OpThumbnail instance was just created. This is the
         * state before calling Init().
         */
	    THUMBNAIL_INITIAL,

        /**
         * Generating a thumbnail was started by calling Init(). This
         * state is also used for starting a suspended thumbnail.
         */
	    THUMBNAIL_STARTED,

        /**
         * Generating the thumbnail was stopped by calling
         * Stop(). After stopping a thumbnail, Init() has to be called
         * again to re-start generating the thumbnail.
         */
	    THUMBNAIL_STOPPED,

        /**
         * The thumbnail is busy rendering. This state is set in
         * OnTimeOut() after the m_generate_timer has been triggered
         * for the first time. If the document is loaded and all
         * images are decoded, OnTimeOut() finishes rendering the
         * thumbnail and sets the state to THUMBNAIL_FINISHED. If the
         * document is not yet loaded or images are not yet ready,
         * another iteration is executed.
         */
	    THUMBNAIL_ACTIVE,

        /**
         * Rendering the thumbnail has finished and m_buffer now
         * contains a thumbnail bitmap.
         */
	    THUMBNAIL_FINISHED      //< Finished rendering, can be stopped.
	};

    /**
	 * Get the width of the given document.
	 *
	 * @para doc			- The document in question
	 * @param left			- The left document bondary is returned by this reference
	 * @param right			- The right document bondary is returned by this reference

	 * @return OpStatus::OK if the width of the document was found, ERR otherwise.
     */
	static OP_STATUS GetDocumentWidth(FramesDocument* doc, long &left, long &right);

    /**
	 * Set the size of the internal window used for thumbnail generation basing on m_source_width and m_source_height.
     */
	void ConfigureWindow();

    Window*		m_window;
    OpBitmap*	m_buffer;

protected:
	int			m_target_width, m_target_height;
	int			m_request_width, m_request_height;
	ThumbnailMode m_mode;
	RenderData	m_render_data;

private:
	int			m_source_width, m_source_height;
    enum ThumbnailState m_state;

	OpTimer		m_generate_timer;
#ifdef THUMBNAILS_LOADING_TIMEOUT
	OpTimer		m_load_timer;
#endif
	/**
	 * It may happen that the Authentication_Manager crashes due to immediate cancel of the auth callback.
	 * In order to avoid that, we use a timer to delay the callback.
	 * More information might be available in CORE-41531.
	 */
	OpTimer		m_auth_timer;
	OpAuthenticationCallback*	m_auth_callback;

	int			m_decode_poll_attemps;
	OpString	m_title, m_url_string;
	int			m_flags;
	URL         m_url;
#ifdef THUMBNAILS_PREALLOCATED_BITMAPS
	OpBitmap*	m_preallocated_bitmap;
#endif

	/** Preview refresh is a HTTP header attribute and/or a HTML META tag.
	 * If both exists, the meta tag is preferred. Measuring unit is seconds.
	 */
	long		m_preview_refresh;

	/**
	 * The ThumbnailIconFinder lives as long as the OpThumbnail since it needs to load all icon URLs found in the document.
	 */
	ThumbnailIconFinder m_icon_chooser;
	ThumbnailIconFinder::IconCandidate* m_best_icon;
};

#endif // CORE_THUMBNAIL_SUPPORT
#endif // THUMBNAIL_H
