/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef URLIMGCONTPROV_H
#define URLIMGCONTPROV_H

#include "modules/util/simset.h"
#include "modules/img/image.h"
#include "modules/url/url2.h"
#include "modules/hardcore/mh/messobj.h"

class HEListElm;
class HEListElmRef;

#ifdef SVG_SUPPORT
class SVGImageRef;
class LogicalDocument;
#endif // SVG_SUPPORT

class UrlImageContentProvider : public Link, public ImageContentProvider, public MessageObject
{
public:
	UrlImageContentProvider(const URL& url, HEListElm *dummy = NULL);
	~UrlImageContentProvider();

	void		IncRef();
	void		IncRef(HEListElm* hle);
	void		DecRef();
	void		DecRef(HEListElm* hle);

	void		MoreDataLoaded();

	Image		GetImage();
	void		ResetAllListElm();

#ifdef CPUUSAGETRACKING
	// Implementing the ImageContentProvider API.
	virtual CPUUsageTracker* GetCPUUsageTracker();
#endif // CPUUSAGETRACKING
	BOOL		IsUrlProvider() { return TRUE; }
	OP_STATUS	GetData(const char*& data, INT32& len, BOOL& more);
	OP_STATUS	Grow();
	INT32		GetDataLen();
	void		ConsumeData(INT32 len);
	INT32		ContentType();
	INT32		ContentSize();
	void		SetContentType(INT32 content_type);
	void		Reset();
	void		Rewind();

	BOOL		IsEqual(ImageContentProvider* content_provider);

	BOOL		IsLoaded();

	void		UrlMoved();
	URL*		GetUrl() { return &url; }
	UINT32		GetStorageId() { return storage_id; }

	/**
	 * Return the security state of the source (or sources in case of
	 * redirects).
	 *
	 * @param requested_url The security state depends on all URL:s of the chain
	 * from requested url via all redirects to the final url, so the total
	 * security depends on which url redirect chain was used, not only the final url.
	 */
	BYTE		GetSecurityStateOfSource(URL& requested_url);


	/**
	 * Returns if this provider has returned data since creation or last reset.
	 * If it returns TRUE it is an indication that the URL has been loaded and
	 * its security state can be determined.
	 */
	BOOL		HasEverProvidedData() { return has_ever_provided_data; }

#ifdef SVG_SUPPORT
	void		SetSVGImageRef(SVGImageRef* ref) { svgimageref = ref; }
	SVGImageRef*	GetSVGImageRef() const { return svgimageref; }
	/**
	 * @param new_image New image for this UrlImageContentProvider.
	 * @param undisplay If TRUE, then will iterate through HEListElm list
	 * of self and undisplay all HEListElms that were previously displayed
	 * before the new image is assigned.
	 */
	void		ResetImage(Image new_image, BOOL undisplay = FALSE);
	/**
	 * Make sure that the SVGImageRef this UrlImageContentProvider
	 * carries is up-to-date and usable.
	 */
	void		UpdateSVGImageRef(LogicalDocument* logdoc);
	void		SVGFinishedLoading();

	/**
	 * Sends onload event for a svg image.
	 * @return TRUE if the even was sent successfully.
	 */
	BOOL OnSVGImageLoad();

	/**
	 * Sends onerror event for a svg image.
	 */
	void OnSVGImageLoadError();
#endif // SVG_SUPPORT

	static UrlImageContentProvider*
				FindImageContentProvider(const URL& url) { return FindImageContentProvider(url.Id(TRUE), TRUE); }
	static UrlImageContentProvider*
				FindImageContentProvider(URL_ID id, BOOL follow_ref=FALSE);

	static Image
				GetImageFromUrl(const URL& url);
	static void	ResetAndRestartImageFromID(URL_ID id);
	static void	ResetImageFromUrl(const URL& url);
	static void	UrlMoved(const URL& url);
	static BOOL	IsImageDisplayed(const URL& url);

	void		IncLoadingCounter() { ++loading_counter; }
	void		DecLoadingCounter();

private:
	void		SetCallbacks();
	void		UnsetCallbacks();

	void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	URL		url;
	URL_DataDescriptor*
			url_data_desc;
	Image	image;
	UINT	refcount;
	BOOL	all_read;
	BOOL	need_reset;
	BOOL	is_multipart_reload;
	List<HEListElmRef>	m_hle_list; // A list of HEListElm's so we know what to update if the url change/reset.
#ifdef SVG_SUPPORT
	SVGImageRef*
			svgimageref;
#endif // SVG_SUPPORT
	BYTE	security_state_of_source;
	BOOL	has_ever_provided_data;
	INT32	forced_content_type;
	UINT32	storage_id;
	INT32	current_data_len;

	unsigned loading_counter;
	/**< When non-zero, the URL is currently being loaded by a document, so this
	     content provider can expect to see some decoding action. */
};

#endif // URLIMGCONTPROV_H
