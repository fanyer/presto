/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_RESOURCE_IMPL_H__
#define __OP_RESOURCE_IMPL_H__

#include "modules/network/op_resource.h"
#include "modules/url/tools/url_util.h"
#include "modules/cache/cache_exporter.h"

#ifdef HAVE_DISK
class OpResourceImpl;

/** Convenience class to encapsulate SaveAsFile functionality */
class OpResourceSaveFileHandler
	: public OpRequestListener
#ifdef PI_ASYNC_FILE_OP
	, public AsyncExporterListener
#endif
{
private:
	OpResourceSaveFileHandler(OpResourceImpl *resource);
	~OpResourceSaveFileHandler();

	OP_STATUS SaveAsFile(const OpStringC &file_name, OpResourceSaveFileListener *listener, BOOL delete_if_error);

#ifdef PI_ASYNC_FILE_OP
	// AsyncExporterListener implementation
	void NotifyStart(OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param) {}
	void NotifyProgress(OpFileLength bytes_saved, OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param);
	void NotifyEnd(OP_STATUS ops, OpFileLength bytes_saved, OpFileLength length, URL_Rep *rep, const OpStringC &name, UINT32 param);
#endif

	// OpRequestListener implementation
	void OnResponseDataLoaded(OpRequest *req, OpResponse *res);
	void OnResponseFinished(OpRequest *req, OpResponse *res);
	void OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error);

	OpResourceImpl *m_resource;
	OpResourceSaveFileListener *m_save_file_listener;
	friend class OpResourceImpl;
	friend class OpResourceSaveFileListener;
};
#endif // HAVE_DISK

class OpResourceImpl: public OpResource
{
public:
	OpResourceImpl(URL &url, OpRequest *request);

	virtual ~OpResourceImpl();

	static BOOL QuickLoad(OpResource *&resource, OpURL url, BOOL guess_content_type);

	URLCacheType GetCacheType() const { return (URLCacheType) m_url.GetAttribute(URL::KCacheType); }

	virtual URL_Resumable_Status GetResumeSupported() const { return (URL_Resumable_Status) m_url.GetAttribute(URL::KResumeSupported); }

	OP_STATUS SetLocalTimeLoaded(const time_t &value) { return m_url.SetAttribute(URL::KVLocalTimeLoaded, (const void *)&value); }

	time_t GetLocalTimeLoaded() const  { time_t local_time_loaded; m_url.GetAttribute(URL::KVLocalTimeLoaded, &local_time_loaded); return local_time_loaded; }

	UINT32 GetResourceId() const { return (UINT32) m_url.GetAttribute(URL::KStorageId); }

	URL_CONTEXT_ID GetContextId() const { return m_url.GetContextId(); }

	OP_STATUS SetHTTPPragma(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTPPragma, value); }

	OP_STATUS SetHTTPCacheControl(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTPCacheControl, value); }

	OP_STATUS SetHTTPExpires(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTPExpires, value); }

	OP_STATUS SetHTTPEntityTag(const OpStringC8 &value) { return m_url.SetAttribute(URL::KHTTP_EntityTag, value); }

	void SetAccessed() { m_url.Access(FALSE); }

	OP_STATUS PrepareForViewing(RetrievalMode mode, BOOL force_to_file) { if (m_url.PrepareForViewing(FALSE, mode != DecompressAndCharsetConvert ? TRUE : FALSE, mode != Unprocessed? TRUE : FALSE, force_to_file) == 0) return OpStatus::OK; return OpStatus::ERR; }

#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	OP_STATUS GetCacheFileFullName(OpString &value) const { OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KFilePathName_L, value)); return result; }

	OP_STATUS GetCacheFileBaseName(OpString &value) const { return m_url.GetAttribute(URL::KFileName, value); }
#endif

	OP_STATUS GetDataDescriptor(OpDataDescriptor *&dd, RetrievalMode mode,
							URLContentType override_content_type,
							unsigned short override_charset_id, BOOL get_original_content);

	void EvictFromCache() { m_url.SetAttribute(URL::KUnique, (UINT32) TRUE); }

#ifdef TRUST_RATING
	OP_STATUS SetTrustRating(TrustRating value) { return m_url.SetAttribute(URL::KTrustRating, (UINT32) value);}

	TrustRating GetTrustRating() const { return (TrustRating) m_url.GetAttribute(URL::KTrustRating); }
#endif

#if defined HAVE_DISK && defined URL_ENABLE_ASSOCIATED_FILES
	OpFile *CreateAssociatedFile(OpResource::AssociatedFileType type) { return m_url.CreateAssociatedFile((URL::AssociatedFileType)type); }

	OpFile *OpenAssociatedFile(OpResource::AssociatedFileType type) { return m_url.OpenAssociatedFile((URL::AssociatedFileType)type); }
#endif // HAVE_DISK && URL_ENABLE_ASSOCIATED_FILES

#ifdef HAVE_DISK
	BOOL IsDownloadComplete() { return m_url.IsExportAllowed(); }

	OP_STATUS SaveAsFile(const OpStringC &file_name, OpResourceSaveFileListener *listener, BOOL delete_if_error) { return m_save_file_handler.SaveAsFile(file_name, listener, delete_if_error); }
#endif // HAVE_DISK

#ifdef _MIME_SUPPORT_
	BOOL IsMHTML() const { return m_url.IsMHTML(); }

	OP_STATUS GetMHTMLRootPart(OpResponse *&root_part);
#endif

	OP_STATUS GetBufferedRanges(OpAutoVector<StorageSegment> &segments) { return SquashStatus(m_url.GetSortedCoverage(segments), OpStatus::ERR_NO_MEMORY ); }

private:
	URL m_url;
	OpRequest *m_request;
	class OpResponseImpl* m_multipart_related_root_part;

#ifdef HAVE_DISK
	OpResourceSaveFileHandler m_save_file_handler;
	friend class OpResourceSaveFileListener;
	friend class OpResourceSaveFileHandler;
#endif // HAVE_DISK
};
#endif
