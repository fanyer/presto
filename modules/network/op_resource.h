/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_RESOURCE_H__
#define __OP_RESOURCE_H__

#include "modules/network/op_request.h"
#include "modules/network/op_data_descriptor.h"

class OpResourceSaveFileListener;

/** @class OpResource
 *
 *  OpResource contains the response body result of a URL request. This could be a HTML, PDF etc.
 *  The response headers are contained in the corresponding OpResponse object. The OpResource object
 *  can be retrieved from the OpResponse object.
 *
 *  Usage:
 *  @code
 * 	OpURL url = OpURL::Make("http://t/core/networking/http/cache/data/blue.jpg");
 *  OP_STATUS result = OpRequest::Make(request, requestListener, url);
 *
 *  if (result == OpStatus::OK)
 *  {
 *    request->SendRequest();
 *  }
 *  @endcode
 *
 *  As a result of this you will get a callback to the request-listener containing the OpResponse object, from which
 *  you can fetch the OpResource object. For instance:
 *
 *  @code
 *  void MyListener::OnResponseFinished(OpRequest *request, OpResponse *response)
 *  {
 *    OpResource *resource = response->GetResource();
 *    if (resource)
 *      resource->GetDataDescriptor(...);
 *      ...
 *  }
 *  @endcode
 */

class OpResource
{
public:
	virtual ~OpResource() {}

	/** Quick load from cache if it is possible. Currently used for file (not directories) and data URLs.
	 *
	 *  @param	guess_content_type		Should we try to guess the content type of this resource
	 *  @return TRUE if the resource was loaded successfully.
	 */
	static BOOL QuickLoad(OpResource *&resource, OpURL url, BOOL guess_content_type);

	/** @return What type of cache is used. Useful for special-handling of stream based cache where it is not possible to retrieve data more than once. */
	virtual URLCacheType GetCacheType() const = 0;

	/** Do we think/know if this request can be resumed (if stopped)? */
	virtual URL_Resumable_Status GetResumeSupported() const = 0;

	/** Set when we started loading this resource.
	 *  @param value when we started loading (time is UTC, unit is seconds since 1970-01-01).
	 *  @return OK or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetLocalTimeLoaded(const time_t &value) = 0;

	/** @return When this resource started loading (time is UTC, unit is seconds since 1970-01-01). */
	virtual time_t GetLocalTimeLoaded() const = 0;

	/** @return The unique id of the storage. */
	virtual UINT32 GetResourceId() const = 0;

	/** Get the context ID of the resource.
	 *
	 *  In some cases (such as MHTML), this context id might be different from
	 *  what was set in the OpRequest. To explain, the original request might
	 *  be done for a completely normal url in the "global" cache context. When
	 *  it turns out that the url contains an MHTML archive, we need a separate
	 *  context for this to avoid mixing archived and live resources.
	 *
	 *  @return The context ID of the resource.
	 */
	virtual URL_CONTEXT_ID GetContextId() const = 0;

	/** Set a HTTP Pragma directive.
	 *  @param value Directive string.
	 *  @return OK or ER_NO_MEMORY.
	 */
	virtual OP_STATUS SetHTTPPragma(const OpStringC8 &value) = 0;

	/** Set a HTTP Cache Control directive.
	 *  @param value Directive string.
	 *  @return OK or ER_NO_MEMORY.
	 */
	virtual OP_STATUS SetHTTPCacheControl(const OpStringC8 &value) = 0;

	/** Set a Cache expiration directive.
	 *  @param value Directive string.
	 *  @return OK or ER_NO_MEMORY.
	 */
	virtual OP_STATUS SetHTTPExpires(const OpStringC8 &value) = 0;

	/** Set the HTTP Entity Header.
	 *  @param value Header string.
	 *  @return OK or ER_NO_MEMORY.
	 */
	virtual OP_STATUS SetHTTPEntityTag(const OpStringC8 &value) = 0;

	/** The resource was accessed at this time, update the cache system so that it will stay on top in the LRU list
	 * and not be removed prematurely from cache.
	 */
	virtual void SetAccessed() = 0;

	enum RetrievalMode
	{
		/** Retrieve a raw unprocessed data stream. */
		Unprocessed,
		/** Retrieve a decompressed data stream. */
		Decompress,
		/** Retrieve decompressed and char-set converted data stream. */
		DecompressAndCharsetConvert
	};

	/** Prepare the resource for viewing in an external application, possibly stripping content encodings.
	 *  This operation replaces the current cache file. The file name can be queried with GetCacheFileFullName,
	 *  or you can use SaveAsFile to place the resource where you want it.
	 *
	 *  @param	mode	Default set to retrieve a decompressed and char-set converted data stream. For alternatives see RetrievalMode.
	 *  @param	force_to_file	The caller require a file to be created, used to cause a flush to disk if memory only cache is used.
	 *
	 *  @return OK or ERR.
	 */
	virtual OP_STATUS PrepareForViewing(RetrievalMode mode = DecompressAndCharsetConvert, BOOL force_to_file = FALSE) = 0;

#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	/** Get the full path and name of the cache file.
	 *  @param value Returned filename, or an empty string if the resource is not cached in a file.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS GetCacheFileFullName(OpString &value) const = 0;

	/** Get the name of the cache file excluding path.
	 *  @param value Returned filename, or an empty string if the resource is not cached in a file.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS GetCacheFileBaseName(OpString &value) const = 0;
#endif

	/** Create an OpDataDescriptor.
	 *  @param dd Returned data descriptor.
	 *  @param mode Retrieval mode.
	 *  @param override_content_type If overridden, the descriptor will assume the content is of this type.
	 *           Use this if the server content-type is different from the detected type.
	 *  @param override_charset_id If overridden, the descriptor will apply this charset when converting.
	 *           Use this if the server charset is different from the detected charset.
	 *  @param get_original_content If TRUE, the original content will be retrieved. E.g., in case the content has been decoded,
	 *           and the original content is still available, such as for MHTML.
	 *  @return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetDataDescriptor(OpDataDescriptor *&dd, RetrievalMode mode = DecompressAndCharsetConvert,
							URLContentType override_content_type = URL_UNDETERMINED_CONTENT,
							unsigned short override_charset_id = 0, BOOL get_original_content = FALSE) = 0;

	/** Evict from cache as soon as the resource is no longer referenced. */
	virtual void EvictFromCache() = 0;

#ifdef TRUST_RATING
	/** Set the trust rating after retrieving this from Operas trust servers.
	 *  @param value trust rating.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetTrustRating(TrustRating value) = 0;

	/** @return The trust rating as retrieved from Operas trust servers. */
	virtual TrustRating GetTrustRating() const = 0;
#endif

#if defined HAVE_DISK && defined URL_ENABLE_ASSOCIATED_FILES

	enum AssociatedFileType {
		Thumbnail = 1 << 0,			// Not allowed in HTTPS pages
		CompiledECMAScript = 1 << 1,
		ConvertedwOFFFont = 1 << 2,
	};

	/** Create an empty file for writing, erases contents of the file if it exists,
	 *  caller must close and delete the file.
	 *  @param type Type of file to create.
	 *  @return NULL on error.
	 */
	virtual OpFile *CreateAssociatedFile(AssociatedFileType type) = 0;

	/** Open an existing associated file for reading, caller must close and delete the file,
	 *  it is not possible to open the file for reading while it is opened for writing.
	 *  @param type Type of file to create.
	 *  @return NULL if the file doesn't exist or on error.
	 */
	virtual OpFile *OpenAssociatedFile(AssociatedFileType type) = 0;
#endif // HAVE_DISK && URL_ENABLE_ASSOCIATED_FILES

#ifdef HAVE_DISK

	/** Checks if the file has been completely downloaded.
	 *  For instance, a Multimedia resource might only have downloaded a few segments.
	 *
	 *  @return TRUE if the file has been completely downloaded.
	 */
	virtual BOOL IsDownloadComplete() = 0;

	/** Save a resource asynchronously to a named file. The content is not decoded, however
	 *  PrepareForViewing can be used to decode the content beforehand.
	 *
	 *  If the resource is currently loading, this call will re-route the download from being
	 *  loaded to an internal cache location to the explicitly named file. This file then becomes
	 *  the cache location of the resource.
	 *
	 *  This method is also useful for multimedia resources, but if the resource is fragmented (i.e. it does not
	 *  constitute a continuous segment starting at the beginning of the resource), SaveAsFile will fail.
	 *
	 *  NOTE! Currently only one save operation can be in progress for the same OpResource.
	 *  NOTE! In case the system doesn't support async file I/O, the listener may be called before this function returns.
	 *
	 *  @param file_name Complete path to the file where the resource will be saved.
	 *  @param listener Listener that will be notified of the save progress (it cannot be NULL).
	 *  @param delete_if_error If TRUE, the target file will be deleted in case of error.
	 *  @return If OK is returned, OnSaveFileFinished is guaranteed to be called. If an error is returned, no callbacks will be made.
	 *          ERR_NO_MEMORY on out of memory.
	 *          ERR on internal problems or if there is an SaveAsFile in progress (one that hasn't received OnSaveFileFinished).
	 */
	virtual OP_STATUS SaveAsFile(const OpStringC &file_name, OpResourceSaveFileListener *listener, BOOL delete_if_error = TRUE) = 0;

#ifdef _MIME_SUPPORT_
	/** @return TRUE if the resource is an MHTML archive */
	virtual BOOL IsMHTML() const = 0;

	/* Get the root part of a MHTML archive. When OnResponseFinished has been
	 * received for the original request, the root part will be available.
	 * The returned OpResponse is owned and destructed by this OpResource.
	 *
	 * @param root_part The returned root part, or NULL if there is no root part (yet)
	 * @return OK, ERR if there is no root part, ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS GetMHTMLRootPart(OpResponse *&root_part) = 0;
#endif // _MIME_SUPPORT_

protected:
	// Functions needed for "friend" access to OpResourceSaveFileListener::m_resource
	void ConnectListener(OpResourceSaveFileListener *listener);
	void DisconnectListener(OpResourceSaveFileListener *listener);

#endif // HAVE_DISK

public:
	/**	Get a sorted vector of the available byte ranges.
	 *
	 *  @param segments An empty vector that will be populated with
	 *  the available ranges, sorted by the start byte in each range.
	 *  From one OpResponse you can retrieve all available ranges using one OpDataDescriptor.
	 *  @return OK, ERR_NO_MEMORY or ERR on internal problems.
	 */
	virtual OP_STATUS GetBufferedRanges(OpAutoVector<StorageSegment> &segments) = 0;
};

#ifdef HAVE_DISK

/** @class OpResourceSaveFileListener
 *  Listener to the asynchronous OpResource::SaveAsFile call.
 */
class OpResourceSaveFileListener
{
public:
	OpResourceSaveFileListener();
	virtual ~OpResourceSaveFileListener();

	/** Signal save file progress.
	 *  @param bytes_saved Bytes saved on disk.
	 *  @param length Total length of the file. If 0, the total length is unknown.
	 */
	virtual void OnSaveFileProgress(OpFileLength bytes_saved, OpFileLength length) = 0;

	/** Signal that save file is finished (with or without errors). If there are no errors, OnSaveFileProgress() will also be called before this.
	 *  This method will always be called, unless SaveAsFile() returned an error (which means that the save did not start).
	 *  @param status OK or ERR_NO_MEMORY.
	 *                ERR_NO_DISK, ERR_NO_ACCESS, ERR_FILE_NOT_FOUND on file problems.
	 *                ERR_NO_SUCH_RESOURCE on any kind of network problem.
	 *                ERR on internal problems.
	 *  @param bytes_saved Bytes saved on disk.
	 *  @param length Total length of the file. If 0, the total length is unknown.
	 */
	virtual void OnSaveFileFinished(OP_STATUS status, OpFileLength bytes_saved, OpFileLength length) = 0;

private:
	friend class OpResource;
	OpResource *m_resource;
};

#endif // HAVE_DISK

#endif //  __OP_RESOURCE_H__
