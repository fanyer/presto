/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_RESPONSE_IMPL_H__
#define __OP_RESPONSE_IMPL_H__

#include "modules/network/op_response.h"

/** @class OpResponseImpl
 *
 */

class OpResponseImpl: public OpResponse, public ListElement<OpResponseImpl>
{
public:
	virtual ~OpResponseImpl();

	virtual OpResource *GetResource() const { return m_resource; }

	virtual void CopyAllHeadersL(HeaderList& header_list_copy) const { m_url.CopyAllHeadersL(header_list_copy); }

	virtual URLContentType GetContentType() const {	return (URLContentType) m_url.GetAttribute(URL::KContentType, URL::KNoRedirect); }

	virtual URLContentType GetOriginalContentType() const { return (URLContentType) m_url.GetAttribute(URL::KOriginalContentType, URL::KNoRedirect); }

	virtual const char *GetMIMEType() const { return m_url.GetAttribute(URL::KMIME_Type, URL::KNoRedirect).CStr(); }

	virtual const char *GetOriginalMIMEType() const { return m_url.GetAttribute(URL::KOriginalMIME_Type, URL::KNoRedirect).CStr(); }

	virtual BOOL GetMIMETypeUnknown() const { return m_url.GetAttribute(URL::KMIME_Type_Was_NULL, URL::KNoRedirect); }

	virtual BOOL IsCacheInUse() const { return (BOOL) m_url.GetAttribute(URL::KCacheInUse, URL::KNoRedirect); }

	virtual BOOL IsCachePersistent() const { return (BOOL) m_url.GetAttribute(URL::KCachePersistent, URL::KNoRedirect); }

	virtual BOOL HasCachePolicyNoStore() const { return (BOOL) m_url.GetAttribute(URL::KCachePolicy_NoStore, URL::KNoRedirect); }

	virtual BOOL HasCachePolicyAlwaysVerify() const { return (BOOL) m_url.GetAttribute(URL::KCachePolicy_AlwaysVerify, URL::KNoRedirect); }

	virtual BOOL HasCachePolicyMustRevalidate() const { return (BOOL) m_url.GetAttribute(URL::KCachePolicy_MustRevalidate, URL::KNoRedirect); }

	virtual time_t GetLastModified() const { time_t last_modified; m_url.GetAttribute(URL::KVLastModified, &last_modified, URL::KNoRedirect); return last_modified; }

	virtual int GetHTTPRefreshInterval() const { return (int) m_url.GetAttribute(URL::KHTTP_Refresh_Interval, URL::KNoRedirect); }

	virtual const char *GetAutodetectCharSet() const { return m_url.GetAttribute(URL::KAutodetectCharSet, URL::KNoRedirect).CStr(); }

	virtual time_t GetLocalTimeVisited() const { time_t time_visited; m_url.GetAttribute(URL::KVLocalTimeVisited, &time_visited, URL::KNoRedirect); return time_visited; }

	virtual time_t GetHTTPExpirationDate() const { time_t expiration_date; m_url.GetAttribute(URL::KVHTTP_ExpirationDate, &expiration_date, URL::KNoRedirect); return expiration_date; }

	virtual OpURL GetHTTPContentLocationURL() const { return OpURL(m_url.GetAttribute(URL::KHTTPContentLocationURL, URL::KNoRedirect)); }

	virtual const char *GetHTTPEntityTag() const { return m_url.GetAttribute(URL::KHTTP_EntityTag, URL::KNoRedirect).CStr(); }

	virtual BOOL IsFresh() const { if (m_url.GetAttribute(URL::KFermented, URL::KNoRedirect)) return FALSE; return TRUE; }

	virtual SecurityLevel GetSecurityStatus() const { return (SecurityLevel) m_url.GetAttribute(URL::KSecurityStatus, URL::KNoRedirect); }

	virtual int GetSecurityLowReason() const { return (int)m_url.GetAttribute(URL::KSecurityLowStatusReason, URL::KNoRedirect); }

	virtual OP_STATUS GetSecurityText(OpString &value) const { return m_url.GetAttribute(URL::KSecurityText, value, URL::KNoRedirect); }

#ifdef _SECURE_INFO_SUPPORT
	virtual OpURL GetSecurityInformationURL() const { return OpURL(m_url.GetAttribute(URL::KSecurityInformationURL, URL::KNoRedirect)); }
#endif

	virtual OP_STATUS SetCacheTypeStreamCache(BOOL value) { return m_url.SetAttribute(URL::KCacheTypeStreamCache, (UINT32) value);}

	virtual BOOL IsUntrustedContent() const { return (BOOL) m_url.GetAttribute(URL::KUntrustedContent, URL::KNoRedirect); }

	virtual BOOL WasProxyUsed() const { return (BOOL) m_url.GetAttribute(URL::KUseProxy, URL::KNoRedirect); }

	virtual unsigned short GetHTTPResponseCode() const { return (unsigned short) m_url.GetAttribute(URL::KHTTP_Response_Code, URL::KNoRedirect); }

	virtual OpFileLength GetHTTPUploadTotalBytes() { OpFileLength ret; m_url.GetAttribute(URL::KHTTP_Upload_TotalBytes, &ret, URL::KNoRedirect); return ret; }

	virtual BOOL IsDirectoryListing() const { return (BOOL) m_url.GetAttribute(URL::KIsDirectoryListing, URL::KNoRedirect); }

	virtual OP_STATUS GetHeader(OpString8 &value, const char* header_name, BOOL concatenate = FALSE) const;

	virtual void GetHTTPLinkRelations(HTTP_Link_Relations *relation) { m_url.GetAttribute(URL::KHTTPLinkRelations, relation, URL::KNoRedirect); }

	virtual const char *GetHTTPLastModified() const { return m_url.GetAttribute(URL::KHTTP_LastModified, URL::KNoRedirect).CStr(); }

	virtual const char *GetHTTPRefreshUrlName() const { return m_url.GetAttribute(URL::KHTTPRefreshUrlName, URL::KNoRedirect).CStr(); }

	virtual const char *GetHTTPLocation() const { return m_url.GetAttribute(URL::KHTTP_Location, URL::KNoRedirect).CStr(); }

	virtual const char *GetHTTPResponseText() const { return m_url.GetAttribute(URL::KHTTPResponseText, URL::KNoRedirect).CStr(); }

	virtual OP_STATUS GetHTTPAllResponseHeaders(OpString8 &value) { OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KHTTPAllResponseHeadersL, value, URL::KNoRedirect)); return result; }

	virtual OpFileLength GetLoadedContentSize() const { OpFileLength ret; m_url.GetAttribute(URL::KContentLoaded_Update, &ret, URL::KNoRedirect); return ret; }

	virtual OpFileLength GetContentSize() const { OpFileLength ret; m_url.GetAttribute(URL::KContentSize, &ret, URL::KNoRedirect); return ret; }

	virtual OP_STATUS GetSuggestedFileName(OpString &value) const { OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KSuggestedFileName_L, value, URL::KNoRedirect)); return result; }

	virtual OP_STATUS GetSuggestedFileNameExtension(OpString &value) const { OP_STATUS result=OpStatus::ERR; RETURN_IF_LEAVE(result = m_url.GetAttribute(URL::KSuggestedFileNameExtension_L, value, URL::KNoRedirect)); return result; }

	virtual BOOL IsGenerated() const { return (BOOL) m_url.GetAttribute(URL::KIsGenerated, URL::KNoRedirect); }

	virtual BOOL IsGeneratedByOpera() const { return (BOOL) m_url.GetAttribute(URL::KIsGeneratedByOpera, URL::KNoRedirect); }

#ifdef DRM_DOWNLOAD_SUPPORT
	virtual int GetDrmSeparateDeliveryTimeout() const { return (int) m_url.GetAttribute(URL::KDrmSeparateDeliveryTimeout, URL::KNoRedirect); }
#endif

#ifdef ABSTRACT_CERTIFICATES
	virtual const OpCertificate *GetRequestedCertificate() { return static_cast<const OpCertificate *>(m_url.GetAttribute(URL::KRequestedCertificate, NULL, URL::KNoRedirect)); }
#endif

#ifdef WEB_TURBO_MODE
	virtual OpFileLength GetTurboTransferredBytes() { OpFileLength result; UINT32 temp; m_url.GetAttribute(URL::KTurboTransferredBytes, &temp, URL::KNoRedirect); result = temp; return result; }

	virtual OpFileLength GetTurboOriginalTransferredBytes() { OpFileLength result; UINT32 temp; m_url.GetAttribute(URL::KTurboOriginalTransferredBytes, &temp, URL::KNoRedirect); result = temp; return result; }

	virtual BOOL IsTurboBypassed() const { return (BOOL) m_url.GetAttribute(URL::KTurboBypassed, URL::KNoRedirect); }

	virtual BOOL IsTurboCompressed() const { return (BOOL) m_url.GetAttribute(URL::KTurboCompressed, URL::KNoRedirect); }
#endif // WEB_TURBO_MODE

	virtual BOOL IsContentDispositionAttachment() const { return (BOOL) m_url.GetAttribute(URL::KUsedContentDispositionAttachment, URL::KNoRedirect); }

	virtual OpURL GetURL() const { return m_op_url; }

	virtual OpResponse *GetRedirectedFromResponse() const { return Pred(); }

	virtual OpResponse *GetRedirectedToResponse() const { return Suc(); }

protected:
	OpResponseImpl(OpURL url);
	URL m_url;
	OpURL m_op_url;
private:
	OP_STATUS CreateResource(class OpRequest *request);
	OpResource *m_resource;
	friend class OpRequestImpl;
	friend class OpResourceImpl;
	friend class OpGeneratedResponseImpl;
	friend class List<OpResponseImpl>;
};

#endif
