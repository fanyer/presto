/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _AUTH_MANAGER_H_
#define _AUTH_MANAGER_H_

#include "modules/url/url_pd.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/otl/list.h"

#ifdef EXTERNAL_DIGEST_API
# include "modules/libssl/external/ext_digest/external_digest_man.h"
#endif // EXTERNAL_DIGEST_API


/* An implementation of the AuthenticationInterface can optionally be set via URL_Rep::SetAuthInterface()
   if you need special handling. If not URL_Rep will use its URL_DataStorage which also implements the
   AuthenticationInterface. */
class AuthenticationInterface
{
public:
	virtual void Authenticate(AuthElm *auth_elm) = 0;
	virtual void FailAuthenticate(int mode) = 0;
	virtual BOOL CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy) = 0;

	virtual authdata_st *GetAuthData() = 0;
	virtual MsgHndlList *GetMessageHandlerList() = 0;
};


class AuthenticationInformation;
class WindowCommander;

/* 	URL_Manager inherits Authentication_Manager. Access this api by urlManager->function( ..)
	
	Api for authenticating urls given by the callback OnAuthenticationRequired  on OpLoadingListener.
	
	For more information, take a look at OpWindowCommander and OpLoadingListener in the windowcommander module. */

class Authentication_Manager 
{
public:
	void			Authenticate(const OpStringC &user_name, const OpStringC &user_password, URL_ID authid = 0, BOOL authenticate_once = FALSE);
	/*< Authenticate an authid given by the callback OpLoadingListener::OnAuthenticationRequired
		
		When an url needs an authentication the callback OpLoadingListener::OnAuthenticationRequired(..) with authid as parameter
		will be called.
		
		To authenticate call this function with username, password and the authid from OnAuthenticationRequired.
		
		@parameter user_name		The username.
		@parameter user_password	The password.
		@parameter authid			The authid from the OpLoadingListener::OnAuthenticationRequired() callback.

		If username/passord is wrong, OnAuthenticationRequired will be called over again. 
		
		Note: This function is also available through OpWindowCommander. 	*/

	void            CancelAuthentication(URL_ID authid = 0);
	/*< Cancel authentication

		Cancel the authenction callback to OpLoadingListener::OnAuthenticationRequired(..)

		@param authid	The authentication id to cancel, given by the OnAuthenticationRequired callback.
		
		Note: This function is also available through OpWindowCommander. 	*/
		

	void			StopAuthentication(URL_Rep *url);	
	/*< Stop Authentication
	 * 	
		A call to this will stop the OnAuthenticationRequired callbacks on url 
		
		@param url The url that needs authentication. */ 
	

	/***Low level functions called from url module. ***/
	
	void			Clear_Authentication_List();
	/*< Clear all username/passwords given to Authentication_Manager for all urls. */  
	
	  
	BOOL			HandleAuthentication(URL_Rep *url, BOOL reauth);	
	/*< Called by url module when server has replied with 404 
		
		@param auth_obj The url that needs authentication.
		@param reauth 	Set to true if the previous authentication failed. */

	BOOL			OnMessageHandlerAdded(URL_Rep *auth_obj);
	/*< Called by url module when a new MessageHandler is added to a URL_Rep.

		@param auth_obj The URL_Rep that just got a new MessageHandler associated
		                with it.
		@return TRUE for success, FALSE for OOM. */
	
protected:
	Authentication_Manager();
	
	void InitL();
	
	~Authentication_Manager();
	
	OP_STATUS StartAuthentication();
	
private:

#ifdef _USE_PREAUTHENTICATION_
	Head		unprocessed_authentications;
#endif
	BOOL active;

	OtlList<class waiting_url*> waiting_urls;

	void AuthenticationDialogFinished(URL_ID authid=0);

	void RemoveWaitingUrl(waiting_url* wurl);
	void RemoveWaitingUrl(OtlList<waiting_url*>::Iterator wurl_iterator);

	void InternalInit();
	void InternalDestruct();
	
	int SetUpHTTPAuthentication(struct authenticate_params &auth_params, authdata_st *authinfo, unsigned int header_id, BOOL proxy);	

	void NotifyAuthenticationRequired(Window *window, OpAuthenticationCallback* callback);
	/*< Notify the OpLoadingListener of the specified Window that authentication
	    is required.

	    If the specified Window is not 0, the associated OpLoadingListener
	    instance's OpLoadingListener::OnAuthenticationRequired() will be called.
	    the specified Window is 0, the global OpAuthenticationListener
	    instance's OpAuthenticationListener::OnAuthenticationRequired() will
	    be notified instead.

	    @param window Invoke the authentication for this Window. This
	                  will typically cause an authentication dialog to appear
	                  for the corresponding OpWindowCommander.
	    @param callback The OpAuthenticationCallback instance to pass to
	                    OpLoadingListener::OnAuthenticationRequired(). */

	OP_STATUS NotifyAuthenticationRequired(class waiting_url* wurl, Window *window, struct authenticate_params &auth_params);
	/*< Convenience function for creating an OpAuthenticationCallback instance,
	    then calling (the other) NotifyAuthenticationRequired.

	    @param window Invoke the authentication for this Window. This
	                  will typically cause an authentication dialog to appear
	                  for the corresponding OpWindowCommander. The window is
	                  added to the list waiting_url::windows.
	    @param auth_params The OpAuthenticationCallback will be initialised with
	                       the information from this.
	    @return OpStatus::OK on success and the OpAuthenticationCallback cannot
	            be created, e.g. because of OOM, or if the specified window
	            cannot be added to the waiting_url. */

	class waiting_url * FindWaitingURL(const URL_Rep *rep) { return FindWaitingURL(rep->GetID()); }
	/*< Find a waiting_url for a certain URL_Rep (if any).

		@param rep [in] The URL_Rep to look for in the list of waiting URLs.
		@return The waiting_url for this URL_Rep, or NULL if no
		        waiting_url was found. */
	class waiting_url* FindWaitingURL(URL_ID id);
	/*< Find a waiting_url for a certain URL_ID (if any).

		@param id [in] The URL_ID to look for in the list of waiting URLs.
		@return The waiting_url for this URL_ID, or NULL if no
		        waiting_url was found. */

	static void CleanupAuthenticateParams(authenticate_params& auth_params);
	/*< Method to clean up the specified authenticate_params structure.

	    This method shall be called when the associated waiting_url is about to
	    be deleted.

	    @param auth_params is the structure to clean. */

	friend class URL_LoadHandler;

	friend class AuthenticationCallback;
	/*< Declare AuthenticationCallback as friend because
	    AuthenticationCallback::Authenticate() and
	    AuthenticationCallback::CancelAuthentication() need to call the private
	    method AuthenticationDialogFinished(). */
};

struct authenticate_params
{
	int			type;
	AuthScheme		scheme;

	URL				url;
	Window 			*first_window;
	ServerName_Pointer	servername;
	unsigned short	port;
	BOOL proxy;

	ParameterList 	*auth_arguments;

	const char 	*message;
	
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	ParameterList *	ntlm_auth_arguments; // used if both NTLM and Negotiate is presented
#endif
#if defined EXTERNAL_DIGEST_API
	OpSmartPointerWithDelete<External_Digest_Handler> digest_handler;
#endif
};

#ifdef _DEBUG
class waiting_url;
/**
 * This operator can be used in an OP_DBG(()) message to print the
 * waiting_url in a human readable string. Example:
 * @code
 * waiting_url* wurl = ...;
 * OP_DBG(("wurl:") << *wurl);
 * @endcode
 */
Debug& operator<<(Debug& d, const waiting_url& wurl);

#include "modules/auth/auth_enum.h"
/**
 * This operator can be used in an OP_DBG(()) message to print the
 * auth_arguments in a human readable string. Example:
 * @code
 * struct authenticate_params* auth_params = ...;
 * OP_DBG(("auth_params:") << *auth_params);
 * @endcode
 */
Debug& operator<<(Debug& d, const struct authenticate_params& a);
#endif // _DEBUG

#endif // _AUTH_MANAGER_H_

