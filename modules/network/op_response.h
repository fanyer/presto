/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OP_RESPONSE_H__
#define __OP_RESPONSE_H__

#include "modules/formats/hdsplit.h"
#include "modules/network/op_request.h"

/** @class OpResponse
 *
 *  OpResponse contains the response header result of a URL request, typically the HTTP headers.
 *  The content of a response is contained in the corresponding OpResource object.
 *
 *  Usage:
 *  @code
 * 	OpURL url = OpURL::Make("http://t/core/networking/http/cache/data/blue.jpg");
 *	OP_STATUS result = OpRequest::Make(request, requestListener, url);
 *
 *	if (result == OpStatus::OK)
 *  {
 *    ...
 *    request->SendRequest();
 *  }
 *  @endcode
 *
 *  As a result of this you will get a callback to the request-listener containing the OpResponse object:
 *
 *  @code
 *  void MyListener::OnResponseAvailable(OpRequest *request, OpResponse *response)
 *  {
 *    if (response->GetHTTPResponseCode() == 200)
 *    ...
 *  }
 *  @endcode
 */

class OpResponse
{
public:
	virtual ~OpResponse() {};

	/** Access content of response. NULL initially. Once the response contains any data the OpResource object will be available, either when
	 * OnResponseDataLoaded, OnResponseFinished, OnResponseMultipartBodyLoaded or OnRequestFailed is called. Each OpResponse has one OpResource object
	 * that remains for the lifetime of the OpResponse and OpRequest object.
	 */
	virtual OpResource *GetResource() const = 0;

	/** Copy all the HTTP headers that were received when this URL was retrieved. All headers will always be
	 * available for the lifetime of the OpResponse object.
	 *
	 *  @param	header_list_copy	A copy of the headers is stored in this object.
	 */
	virtual void CopyAllHeadersL(HeaderList &header_list_copy) const = 0;

	/** Type of content in the URL as decided by server and content detector. This method returns an enum.
	 *  To get the actual text value use GetMIMEType(). If the Content-Type header is empty or not recognized
	 *  this will return URL_UNKNOWN_CONTENT.
	 */
	virtual URLContentType GetContentType() const = 0;

	/** Get the original content type in the response. This method returns an enum. To get the actual text
	 *  value use GetOriginalMIMEType().
	 *
	 *  If the Content-Type header is empty or not recognized this will return URL_UNKNOWN_CONTENT.
	 *
	 */
	virtual URLContentType GetOriginalContentType() const = 0;

	/** Get the MIME type string as set by both server and processing by our content detection algorithms.
	 * This method returns a string. To get an enum instead use GetContentType(). Lifetime limited to the
	 *  lifetime of the OpResponse.
	 *
	 *  If the Content-Type header is empty or not recognized this will return "application/octet-stream".
	 */
	virtual const char *GetMIMEType() const = 0;

	/** Get the Original MIME type string. This method returns a string. To get an enum instead use
	 *  GetOriginalContentType(). Lifetime limited to the lifetime of the OpResponse.
	 *
	 *  If the Content-Type header is empty or not recognized this will return "application/octet-stream".
	 *
	 */
	virtual const char *GetOriginalMIMEType() const = 0;

	/** Check if the server MIME type was unknown or not set. */
	virtual BOOL GetMIMETypeUnknown() const = 0;

	/** Is this response's cache in use by any datadescriptors? */
	virtual BOOL IsCacheInUse() const = 0;

	/** Is this response's cache entry persistent. */
	virtual BOOL IsCachePersistent() const = 0;

	/** Do not store in disk cache, use RAM cache only. */
	virtual BOOL HasCachePolicyNoStore() const = 0;

	/** Always validate before displaying. */
	virtual BOOL HasCachePolicyAlwaysVerify() const = 0;

	/** Always validate before displaying, also when moving in history. */
	virtual BOOL HasCachePolicyMustRevalidate() const = 0;

	/** When was this response last modified, according to the server? */
	virtual time_t GetLastModified() const = 0;

	/** How many seconds did the Refresh header say we should wait before fetching the given URL? */
	virtual int GetHTTPRefreshInterval() const = 0;

	/** Auto detected character set. Returns a pointer to a static string. */
	virtual const char *GetAutodetectCharSet() const = 0;

	/** When did we last visit this URL (Time is UTC). */
	virtual time_t GetLocalTimeVisited() const = 0;

	/** When does this response's freshness expire. */
	virtual time_t GetHTTPExpirationDate() const = 0;

	/** Get the Content-Location URL specified by the server. */
	virtual OpURL GetHTTPContentLocationURL() const = 0;

	/** The HTTP Entity Tag. Lifetime limited to the lifetime of the OpResponse. */
	virtual const char *GetHTTPEntityTag() const = 0;

	/** Is the response fresh? Return FALSE if it more than 1 minute since the response was loaded. For files we use 10 seconds instead.
	 *  This is typically checked by the auto-proxy module to ensure that a recent version of the auto-proxy script is used.
	 */
	virtual BOOL IsFresh() const = 0;

	/** Security level of this response. */
	virtual SecurityLevel GetSecurityStatus() const = 0;

	/** Get the reason for a security level of SECURITY_STATE_LOW or SECURITY_STATE_HALF.
	 *  @return one bit set for of any of the SecurityLowReason enums.
	 */
	virtual int GetSecurityLowReason() const = 0;

	/** The last security level text.
	 * "<PROTOCOL> <Major Version>.<Minor Version> <Encryption Name> (<RSA Key Size> bit <Key Exchange Algorithm>/<Hash Name>)"
	 */
	virtual OP_STATUS GetSecurityText(OpString &value) const = 0;

#ifdef _SECURE_INFO_SUPPORT
	/** Get the URL with the security information for this response. */
	virtual OpURL GetSecurityInformationURL() const = 0;
#endif

	/** Convert the current cache to a stream cache (no permanent storage). */
	virtual OP_STATUS SetCacheTypeStreamCache(BOOL value) = 0;

	/** Is the content of this response untrusted? During pageload the browser will connect to
	 * Operas trust servers and verify a web page/download's trustworthiness. Afterwards the resulting
	 * status can be retrieved through this method. The trust rating system protects against fraud and
	 * malware.
	 */
	virtual BOOL IsUntrustedContent() const = 0;

	/** Was proxy used when the response was loaded. */
	virtual BOOL WasProxyUsed() const = 0;

	/** The received HTTP response code. */
	virtual unsigned short GetHTTPResponseCode() const = 0;

	/** Gets the number of bytes the URL uploaded. */
	virtual OpFileLength GetHTTPUploadTotalBytes() = 0;

	/** Is this a generated directory listing? This can be true for file and FTP URLs. */
	virtual BOOL IsDirectoryListing() const = 0;

	/** Retrieve header from this response. if concatenate is set it will concatenate the headers if these are set with the same header name. */
	virtual OP_STATUS GetHeader(OpString8 &value, const char* header_name, BOOL concatenate = FALSE) const = 0;

	/** Get the HTTP link relations structure. */
	virtual void GetHTTPLinkRelations(HTTP_Link_Relations *relation) = 0;

	/** The received Last modified date. Lifetime limited to the lifetime of the OpResponse. */
	virtual const char *GetHTTPLastModified() const = 0;

	/** The URL part of the Refresh header. Lifetime limited to the lifetime of the OpResponse. */
	virtual const char *GetHTTPRefreshUrlName() const = 0;

	/** The received location header. Lifetime limited to the lifetime of the OpResponse. */
	virtual const char *GetHTTPLocation() const = 0;

	/** The response explanation part of the HTTP response. Lifetime limited to the lifetime of the OpResponse. */
	virtual const char *GetHTTPResponseText() const = 0;

	/** Retrieve a string with all preserved headers from the last response. Formatted according to the WhatWG
	 * XmlHttpRequest specification with <header name>: <value1>, <value2>\r\n<header name>: <value>\r\n\r\n.
	 */
	virtual OP_STATUS GetHTTPAllResponseHeaders(OpString8 &value) = 0;

	/** Number of bytes loaded from server. This is continuously updated until OnResponseFinished is triggered. */
	virtual OpFileLength GetLoadedContentSize() const = 0;

	/** Number of bytes expected. */
	virtual OpFileLength GetContentSize() const = 0;

	/** Suggested Filename. */
	virtual OP_STATUS GetSuggestedFileName(OpString &value) const = 0;

	/** Suggested Filename extension. */
	virtual OP_STATUS GetSuggestedFileNameExtension(OpString &value) const = 0;

	/** Is the content generated? All responses that are created with the OpGeneratedResponse class are generated. */
	virtual BOOL IsGenerated() const = 0;

	/** Is the content generated entirely from sources controlled by Opera (and not on behalf of external input)?
	 *  In that case we can assume some properties of the content, such as that we can trust the security, that the
	 *  charset is well-defined and doesn't need auto-detection, that we shouldn't index the content for search, etc. */
	virtual BOOL IsGeneratedByOpera() const = 0;

#ifdef DRM_DOWNLOAD_SUPPORT
	/** Maximum number of seconds to wait for separate delivery (separated rights object + encrypted media object).
	 *  -1 means that we shouldn't wait. This is fetched from the x-oma-drm-separate-delivery response header.
	 */
	virtual int GetDrmSeparateDeliveryTimeout() const = 0;
#endif

#ifdef ABSTRACT_CERTIFICATES
	/** Gets the certificate relating to this Response. The request must be for a HTTPS url, and you must
	 *  call SetCertificateRequested(TRUE) on the request first.
	 *  @return The requested certificate, or NULL if there isn't any. Lifetime limited to the lifetime of the OpResponse. */
	virtual const OpCertificate *GetRequestedCertificate() = 0;
#endif

#ifdef WEB_TURBO_MODE
	/** The number of bytes transferred from the Turbo proxy, including HTTP protocol overhead. */
	virtual OpFileLength GetTurboTransferredBytes() = 0;

	/** The number of bytes received by the Turbo proxy when loading this resource, including HTTP protocol overhead.
	 *  NB! This may in special cases return zero even if KTurboTransferredBytes returns non-zero (e.g. host lookup failed). */
	virtual OpFileLength GetTurboOriginalTransferredBytes() = 0;

	/** Was this Turbo URL loaded in bypass mode (directly from origin server)? */
	virtual BOOL IsTurboBypassed() const = 0;

	/** Was the resource compressed by the Opera Turbo Proxy? */
	virtual BOOL IsTurboCompressed() const = 0;
#endif // WEB_TURBO_MODE

	/** Did the server use a "Content-Disposition:attachment" header? */
	virtual BOOL IsContentDispositionAttachment() const = 0;

	/** @return The URL of this response */
	virtual OpURL GetURL() const = 0;

	/** @return The previous redirect response that redirected to this response, if any, otherwise NULL */
	virtual OpResponse *GetRedirectedFromResponse() const = 0;

	/** @return The response that this response redirected to, if any, otherwise NULL */
	virtual OpResponse *GetRedirectedToResponse() const = 0;
};

#endif
