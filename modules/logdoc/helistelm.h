/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HELISTELM_H
#define HELISTELM_H

#include "modules/url/url2.h"
#include "modules/util/simset.h"
#include "modules/img/image.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/display/vis_dev_transform.h"
#include "modules/logdoc/src/helist.h"
#include "modules/logdoc/elementref.h"

class HEListElm;
class HTML_Element;
class LoadInlineElm;
class FramesDocument;
class ImageAnimationHandler;
class UrlImageContentProvider;

class HEListElmRef : public ListElement<HEListElmRef>
{
public:
	HEListElmRef(HEListElm* hle) : hle(hle) {}
	HEListElm* hle;
};

/// Used in the LoadInlineElm to reference the elements associated with it.
class HEListElm
  : public ListElement<HEListElm>
  , public ImageListener
  , public ElementRef
{
friend class HLDocProfile;
private:
	/// Previous URL that this element was associated with. Used while a new URL is loading
	URL				old_url;

	/// The animation handler for animated images
	ImageAnimationHandler* animation_handler;

	/// The content provider for this image
	UrlImageContentProvider* url_imagecontent_provider;

	/// The document that started the loading of the element
	FramesDocument* doc;

	/// absolute left position if image and image is displayed/visible.
	AffinePos		image_pos;

	HEListElmRef	ref;

	/// width of area where image is to be painted (normal, scaled or repeated)
	long			image_width;

	/// height of area where image is to be painted (normal, scaled or repeated)
	long			image_height;

	/// last decoded height of a loading image (not interlaced) last time it was painted
	UINT32			image_last_decoded_height;

	/// The type of inline
	InlineResourceType	inline_type;

	/// TRUE if this element has been finished and handled already
	unsigned int	handled:1;

	/// TRUE if the animation should be syncronized with [something]
	unsigned int	syncronize_animation:1;

	/// TRUE if the onload event has been sent for this element
	unsigned int	event_sent:1;

#ifdef REMOVE_IRRELEVANT_IMAGES
	unsigned int	signaled_irrelevant:1;
#endif

	/// TRUE if this image in the list of background images is in use
	unsigned int	image_active: 1;

#ifdef HAS_ATVEF_SUPPORT
	unsigned int	is_atvef_image:1;
#endif // HAS_ATVEF_SUPPORT

	/// TRUE if this image is visible (it is currently 'Display(...)':ed)
	unsigned int	image_visible:1;

	/// TRUE if we have posted an invalidation from a gif animation. Used to make sure we don't animate things that are not painted.
	unsigned int    has_pending_animation_invalidation:1;

	/// TRUE if this resource should not affect the document's progress display and delay the load event.
	unsigned int    delay_load:1;

	/// TRUE if this resource is loaded from a user stylesheet.
	unsigned int    from_user_css:1;

#ifdef CORS_SUPPORT
	unsigned int	is_crossorigin_allowed:1;
#endif // CORS_SUPPORT

	void			SetImageVisible(BOOL visible) { image_visible = visible ? 1 : 0; }
	void			Update(Image& image);

	OP_STATUS		SendLoadEvent();

	// Make visibility of these private because only LoadInlineElm
	// can remove or add them to the list in order to update its flags.
	inline void Out(){ Link::Out(); }
	inline void Into(Head *h){ Link::Into(h); }
	friend class LoadInlineElm;

public:

	                HEListElm(HTML_Element* inline_elm, InlineResourceType inline_type, FramesDocument* doc);
	virtual			~HEListElm();

	/// When changing an image (via a script or a style sheet), set the previous URL, so that it can be used until the new image is loaded.
	void            SetOldUrl(const URL &url) { old_url = url; }

	/// Get the previous URL. Will be available while the new URL is loading.
	URL				GetOldUrl() { return old_url; }

	/// Updates the image if the URL has changed
	BOOL			UpdateImageUrlIfNeeded(URL* img_url);
	/// Update image. Creates content provider and tries to decode. This is used when the HEListElm has been used to display an old image and it's now time to start displaying a new image because it's finally available.
	BOOL			UpdateImageUrl(URL* img_url);
	/// Called when the url is redirected
	void            UrlMoved(URL moved_to_url);
	/**
		Decodes all available data for the first frame in the image.
		If decode_absolutely_all is FALSE, it will only decode all if it's small enough and abort if it takes to much time.
	*/
	void			LoadAll(BOOL decode_absolutely_all = TRUE);

	/**
	 * Does a small attempt at decoding the image if it's small so that we don't
	 * do one more paint just to show it.
	 */
	void			LoadSmall();

	/// Resets the image to undecoded state
	void			Reset();

  	BOOL			IsBgImageInline() const { return inline_type == BGIMAGE_INLINE || inline_type == EXTRA_BGIMAGE_INLINE; }
  	BOOL			IsImageInline() const { return IsImageInline(inline_type); }
#ifdef SHORTCUT_ICON_SUPPORT
	BOOL			IsIconInline() const { return inline_type == ICON_INLINE; }
#endif // SHORTCUT_ICON_SUPPORT
  	BOOL			IsEmbedInline() const { return inline_type == EMBED_INLINE; }
  	BOOL			IsScriptInline() const { return inline_type == SCRIPT_INLINE; }
  	BOOL			IsCSS_Inline() const { return inline_type == CSS_INLINE; }
  	BOOL			IsGenericInline() const { return inline_type == GENERIC_INLINE; }
#ifdef HAS_ATVEF_SUPPORT
	BOOL            IsImageAtvef() { return is_atvef_image; }
#endif // HAS_ATVEF_SUPPORT

	InlineResourceType	GetInlineType() const { return inline_type; }
	/// Returns the associated HTML element
  	HTML_Element*	HElm() const { return GetElm(); }
	/// Helper function to get the next element in LoadInlineElm
  	HEListElm*		Suc() const { return (HEListElm*) Link::Suc(); }

	const AffinePos& GetImagePos() const { return image_pos; }
	long			GetImageWidth() const { return image_width; }
	long			GetImageHeight() const { return image_height; }
	OpRect			GetImageBBox() const
	{
		OpRect bbox(0, 0, image_width, image_height);
		image_pos.Apply(bbox);
		return bbox;
	}

	UINT32			GetImageLastDecodedHeight() const { return image_last_decoded_height; }
	void			SetImageLastDecodedHeight(UINT32 value) { image_last_decoded_height = value; }

	BOOL			GetHandled() const { return handled; }
	void			SetHandled() { handled = TRUE; }

	/* @return TRUE if there is a pending animation invalidation on this element that has not been painted. */
	BOOL            HasPendingAnimationInvalidation() { return has_pending_animation_invalidation; }

	/* @see HasPendingAnimationInvalidation(). */
	void            SetHasPendingAnimationInvalidation(BOOL set) { has_pending_animation_invalidation = !!set; }

	/**
	 * Gets delay_load flag.
	 * @returns TRUE if this resource affects the document's progress
	 *          display and delays the load event, FALSE otherwise.
	 */
	BOOL			GetDelayLoad() const { return delay_load; }

	/**
	 * Sets delay_load flag.
	 * @see GetDelayLoad()
	 */
	void			SetDelayLoad(BOOL v);

	/** Tells if this resource is loaded from a user stylesheet. */
	BOOL			IsFromUserCSS() const { return from_user_css; }
	/** Sets that this resource is loaded from a user stylesheet. */
	void			SetFromUserCSS(BOOL v) { from_user_css = v; }

	BOOL			GetImageVisible() { return image_visible; }

	/// Returns the LoadInlineElm that this element belongs to.
	LoadInlineElm*  GetLoadInlineElm() { return GetList() ? static_cast<HEList*>(GetList())->GetLoadInlineElm() : NULL; }
	/// Returns TRUE if this element is in the element list of lie.
	BOOL			IsInList(LoadInlineElm* lie);
	/// Returns the Image object created when decoding the loaded image data. Can be empty. When there is both an old url and a new url loading, this will return the Image of the old image. When the new image is ready, UpdateImageUrl() will switch this to the new image.
	Image			GetImage();

	UrlImageContentProvider* GetUrlContentProvider() { return url_imagecontent_provider; }

	/// Called when more data is received from the URL. Will decode image.
	void			MoreDataLoaded();

	/**
	 * Called when URL is loaded and the image decoded. Triggers an DOM onload event.

	 * @param force if TRUE the onload event will be sent even though
	 * the image is not decoded. Used for svg images when they have
	 * finished parsing.
	 */
	OP_STATUS		OnLoad(BOOL force=FALSE);
	BOOL			GetEventSent() { return event_sent; }
	void			ResetEventSent() { event_sent = 0; }

	/// To be called if the resource failed and there should be an ONERROR event sent to the page.
	void			SendOnError();
	/// To be called when an image finish loading and will send onload or onerror if appropriate.
	OP_STATUS		SendImageFinishedLoadingEvent(FramesDocument* doc);

	/// Called when decoding image. Will paint the newly decoded parts of the image. Part of the ImageListener interface.
	virtual void	OnPortionDecoded();
	/// Called when a loading or decoding error occurs. Will trigger a JS OnError event. Part of the ImageListener interface.
	virtual void	OnError(OP_STATUS status);

	/// Will "display" the image if not already displayed. That means that it will locked in the image cache until it's Undisplayed().
	/// Will always set the coordinates.
	void			Display(FramesDocument* doc, const AffinePos& doc_pos, long width, long height, BOOL background, BOOL expand_area);

	/// Will undisplay the image if not already undisplayed. That will release the image cache lock and allow the image's memoty to be recovered.
	void			Undisplay();

	/// Set however the multiple background image corresponding to this is active.
	void			SetActive(BOOL active) { image_active = active; }

	/**
	 * Is the multiple background image corresponding to this active.
	 *
	 * @return TRUE if multiple background image active, FALSE
	 * otherwise.
	 */
	BOOL			IsActive() { return image_active; }

	/** From the ElementRef interface */
	virtual	void	OnDelete(FramesDocument *document);
	virtual	void	OnRemove(FramesDocument *document) { return OnDelete(document); }
	virtual void	OnInsert(FramesDocument *old_document, FramesDocument *new_document) { if (old_document && old_document != new_document) OP_DELETE(this); }
	virtual BOOL	IsA(ElementRef::ElementRefType type) { return type == ElementRef::HELISTELM; }

	FramesDocument *GetFramesDocument() { return doc; }

	HEListElmRef   *GetHEListElmRef() { return &ref; }

	/**
	 * Creates an URL_DataDescriptor except if there is none available or if
	 * it would be a security issue to create one for the HEListElm. See the method
	 * URL::GetDataDescriptor() for a deep description of most of arguments.
	 *
	 * @param[out] url_data_descriptor The created url_data_descriptor if OpStatus::OK is
	 * returned. Otherwise undefined.
	 *
	 * @param[in] mh		MessageHandler that will be used to post update messages to the caller
	 * @param[in follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *                     		Otherwise return a descriptor of this URL.
	 * @param[in] get_raw_data	If TRUE, don't use character set convertion of the data (if relevant)
	 * @param[in] get_decoded_data	If TRUE, decode the data using the content-encoding specification (usually decompression)
	 * @param[in] window			The Window whose charset preferences is used to guess/force the character set of the document
	 * @param[in] override_content_type	What content-type should the descriptor assume the content is. Use this if the server
	 *                                 	content-type is different from the actual type.
	 * @param[in] override_charset_id		What charset ID should the descriptor use for the content?
	 * @param[in] get_original_content	Should the original content be retrieved? E.g., in case the content has been decoded,
	 *                                	and the original content is still availablable, such as for MHTML
	 * @param[in] parent_charset_id		Character encoding (charset) passed from the parent resource.
	 *									Should either be the actual encoding of the enclosing resource,
	 *									or an encoding recommended by it, for instance as set by a CHARSET
	 *									attribute in a SCRIPT tag. Pass 0 if no such contextual information
	 *									is available.
	 *
	 * @return OpStatus::OK if a data decriptor was created, OpStatus::ERR_NO_ACCESS if it
	 * would be a security hole to read data from this URL, OpStatus::ERR_NO_MEMORY if out of
	 * memory or OpStatus::ERR_NO_SUCH_RESOURCE if there was no data available.
	 */
	OP_STATUS       CreateURLDataDescriptor(URL_DataDescriptor*& url_data_descriptor, MessageHandler *mh, URL::URL_Redirect follow_ref, BOOL get_raw_data=FALSE, BOOL get_decoded_data=TRUE, Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT, unsigned short override_charset_id=0, BOOL get_original_content=FALSE, unsigned short parent_charset_id=0);

	/**
	 * Is this inline type an image, background image, part of several
	 * background images, a border image or a video poster?
	 *
	 * @param inline_type the inline type from enum in logdocenum.h
	 *
	 * @return TRUE if inline type is a image, background image, part of several
	 * background images, a border image or a video poster, FALSE otherwise.
	 */
	static BOOL		IsImageInline(InlineResourceType inline_type) {
		return inline_type == IMAGE_INLINE ||
			inline_type == BGIMAGE_INLINE ||
			inline_type == EXTRA_BGIMAGE_INLINE ||
			inline_type == BORDER_IMAGE_INLINE ||
			inline_type == VIDEO_POSTER_INLINE;
	}

#ifdef CORS_SUPPORT
	/**
	 * Returns TRUE if this inline was allowed loaded
	 * by CORS. FALSE if the CORS access check failed
	 * or the inline has yet to load.
	 */
	BOOL		IsCrossOriginAllowed() { return is_crossorigin_allowed; }

	/**
	 * Set if this inline has passed a final CORS
	 * resource access check, meaning that the
	 * cross-origin access of the resource was allowed.
	 */
	void		SetIsCrossOriginAllowed() { is_crossorigin_allowed = TRUE; }
#endif // CORS_SUPPORT

	/** Tells if this inline is blocked due to security policies. */
	BOOL GetAccessForbidden();
};

#endif // HELISTELM_H
