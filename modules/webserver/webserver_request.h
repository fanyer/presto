/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation -- overall server logic
 */

#ifndef REQUEST_QUEUE_H_
#define REQUEST_QUEUE_H_

#ifdef WEBSERVER_SUPPORT

#include "modules/util/simset.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webserver_filename.h"
#include "modules/webserver/webserver_common.h"
#include "modules/webserver/webserver_callbacks.h"
#include "modules/webserver/webserver_user_management.h"


class HeaderList;

enum WebServerMethod
{
	WEB_METH_GET,					/* HTTP 0.9 */
	WEB_METH_HEAD,					/* HTTP 1.0 */
	WEB_METH_POST,					/* HTTP 1.0 */
	WEB_METH_PUT,					/* Poorly defined in HTTP 1.0 */
	WEB_METH_DELETE,				/* Poorly defined in HTTP 1.0 */
	WEB_METH_LINK,					/* Poorly defined in HTTP 1.0, not at all in HTTP 1.1 */
	WEB_METH_UNLINK,				/* Poorly defined in HTTP 1.0, not at all in HTTP 1.1 */
	WEB_METH_TRACE,					/* HTTP 1.1 */
	WEB_METH_CONNECT,				/* HTTP 1.1 */
	WEB_METH_OPTIONS,				/* HTTP 1.1 */
	WEB_METH_NONE_ERROR				/* In case of error*/
};

class WebserverRequest : public Link, private WebserverFileName, private AddToCollectionCallback
{
public:
	WebserverRequest();

	~WebserverRequest();	

	/* Constructs a requests from resource (for example index.html), headerlist and method
	 * 
	 *	
	 * @param 	resource 	 				Path and name of the resource requested
	 * @param 	resourceStringLength  	 	Length of the resource string
	 * @param 	clientHeaderList 	 		The headerlist in the request. 
	 * @param 	httpMethod	 				The method (like GET, POST etc).
	 * 
	 * @return 	OpStatus::ERR_NO_MEMORY If OOM.
	 * 			OpStatus::ERR 			If the function has been called before on this object.
	 * 			OpStatus::OK 			For sucess
	 * 				
	 */	
	OP_STATUS Construct(const char *resource, int resourceStringLength, HeaderList *clientHeaderList, WebServerMethod httpMethod, BOOL isProxied);
	  
	 /* Copy a request. Note that response headers are NOT copied.
	  * 
	  * @param original Request to Copy.
	  * 
	  * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	  * 
	  */
	OP_STATUS Copy(WebserverRequest *original);
	
	/* Reconstructs the this request
	 * 
	 * If the subserver name is changed after this reconstruct, GetSubServerNameEditedState() will
	 * return TRUE
	 * 
	 * @param resource				The resource that is requested.
	 * @param resourceStringLength	Length of resource string
	 * @param clientHeaderList		Client header list.
	 * 
	 * @param return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	OP_STATUS Reconstruct(const char *resource, int resourceStringLength, HeaderList *clientHeaderList = NULL);

	
	static OP_STATUS UrlEncodeString(const OpStringC &uri, OpString &encoded_uri); 


	 /* Returns the request string. This path has been normalized and
	 * 	checked for /../. 
	 * 
	 * 	@return the normalized path. */	
	const uni_char *GetRequest();

	
	 /* Returns the request path to subserver (the path after /subserver/...  
	 * 	This path has been normalized and checked for /../. 
	 * 
	 * 	@return the normalized path. */	
	const uni_char *GetSubServerRequest();
	
	OP_STATUS GetEscapedRequest(OpString &request);
	
	/* Returns the orignal URI as sent by the client
	 * 
	 * 	NOTE! The original request uri is not checked
	 *	for dangerous paths. 
	 *	MUST never be used in file operations. 
	 * 
	 * @param return The original request uri
	 */
	const char *GetOriginalRequest();
	const uni_char *GetOriginalRequestUni();

	/* Get the header list.
	 * 
	 * @return List of headers sent by the client.
	 */
	HeaderList *GetClientHeaderList();

	/* Get the http method.
	 * 
	 * @return The method of this request
	 */
	WebServerMethod GetHttpMethod();

	/* Get the proxy state.
	 * 
	 * @return TRUE when request was proxied
	 */
	BOOL IsProxied() { return m_isProxied; }

	/* Get the forward state.
	 * 
	 * If this is set to TRUE, javascript will not handle this request, 
	 * but will forward to any other resource that listen to this request. 
	 */
	BOOL GetForwardRequestState(){ return m_forwardRequestState; }

	/* Check if the subserver name has been changed in a call to Reconstruct(..) 
	 *
	 * @return TRUE if Reconstruct changed the subserver name.
	 */
	BOOL GetSubServerNameEditedState() { return m_subServerVirtualPathEdited; }
	
	/* Get a unique ID for this request
	 * 
	 * @return a unique ID.
	 */
	UINT GetRequestId(){ return m_requstId;  }
	
	/* Get the path of the sub server for this request.
	 * Trailing and prepending slashes are not included.
	 * 
	 * @return The path of the subserver. */
	const uni_char* GetSubServerVirtualPath(){ return m_subServerVirtualPath.CStr(); }
	
	OP_BOOLEAN GetItemInQuery(const uni_char *item, OpString &value, unsigned int &index, BOOL unescape);
	OP_STATUS GetAllItemsInQuery(OpStringHashTable<OpAutoStringVector> &item_collection, BOOL unescape);
	
	BOOL UserAuthenticated() { return GetAuthenticatedUsername() ? TRUE : FALSE; }
	
	const uni_char* GetAuthenticatedUsername() { return m_authenticated_user.CStr(); }
	
	/* Do not save this pointer. It might be deleted at any time */
	WebserverAuthSession *GetAuthSession() { return g_webserverUserManager->GetAuthSessionFromSessionId(m_session_id); }
	
	const uni_char *GetSessionId(){ return m_session_id.CStr(); }

	OP_STATUS DetectAuthenticationType(WebSubServerAuthType &type);
	
private:	
	OP_STATUS AddToCollection(const uni_char* key, const uni_char* dataString);
	
	/* Insert a list of response headers. Should only be used when re-issuing a request.
	 *
	 * @param	headers	The list of headers to be inserted, this object takes ownership of the items.
	 * 
	 * @return return OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */	
	OP_STATUS SetResponseHeaders(Header_List &headers);

	BOOL RequestHadSessionCookie() { return m_had_session_cookie; }
	
	OP_STATUS SetupSessionId();
	
	void SetResponseCode(int responseCode) { m_responseCode = responseCode; }
	int  GetResponseCode() { return m_responseCode; }
	
	BOOL HasResponseHeaders();
	
	void TakeOverResponseHeaders(Header_List &headers);
	
	OP_STATUS GetSubServerName(const char *request, int requestStringLength, OpString &subServerName);

	void SetForwardRequestState(BOOL forwardRequest) { m_forwardRequestState = forwardRequest;}

	void SetAuthStatus(BOOL hasAuthenticated) { m_auth_status = hasAuthenticated; }

	BOOL HasAuthenticated() { return m_auth_status; }

	OP_STATUS ReconstructOverrideSecurity(const char *resource, int resourceStringLength, HeaderList *clientHeaderList, BOOL overrideSecurity);
	
	/* This is the original request uri. This must 	only be used of authentication since the path might be malicous. */
	OpString8 m_originalRequestURI;
	OpString16 m_uniOriginalRequestURI; 

	/* The path that identifies a subserver. Trailing and prepending slashes are not included. */
	OpString16 m_subServerVirtualPath;
	OpString16 m_authenticated_user;
	
	OpString m_session_id;
	OpString m_session_auth;
		
	WebServerMethod m_httpMethod;
	HeaderList *m_clientHeaderList;
	/* If the request has been reissued (ReIssueRequest(), or closeAndRedispatch() in Javascript), we will remember the reponse headers */
	Header_List m_responseHeaderList;
	
	BOOL m_forwardRequestState;
	BOOL m_subServerVirtualPathEdited;
	BOOL m_auth_status;
	BOOL m_isProxied;

	UINT m_requstId;
	int m_responseCode;
	BOOL m_had_session_cookie;
	
	OpStringHashTable<OpAutoStringVector> *m_temp_item_collection; /* Not owned by this object */
	
	
	friend class WebResource_Auth;
	friend class WebResource;
	friend class WebServerConnection;
};

#endif // WEBSERVER_SUPPORT

#endif /*REQUEST_QUEUE_H_*/
