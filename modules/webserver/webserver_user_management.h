/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBSERVER_USER_MANAGEMENT_H_
#define WEBSERVER_USER_MANAGEMENT_H_

class WebserverAuthSession;
class WebserverAuthenticationId;
class WebserverAuthUser;
class WebSubServer;
class XMLFragment;
class WebResource_Auth;
class WebResourceWrapper;
class WebserverRequest;

/* 
	Global system for managing authentication of users and access control for services.
	
	There is only one global user list. Each user in this list can be given access 
	to services (subservers) if the user is authenticated.

	The user can authenticate himself in many different ways. Currently only openid 
	and magic url is implemented (see WebSubServerAuthType below).

	The actual user is given one or several auth-ids (for example user.myopenid.com for 
	openid or just "username" for magic-urls. When the user authenticate himself
	using any of these auth-ids the webserver will recognize the user.
	
	The auth-ids for each authentication types must be globally unique. 
	
	Example:
	
	username: 		authentication ids:   type:
	
					dill.myopenid.com     openid
		 	 	/
	nickname 	--  dall.my.opera.com     openid
		 	 	\
					test-auth-id	      magic-url

	So if a user authenticates using dill.myopenid.com,  dall.my.opera.com or test-auth-id, 
	the webserver will recognize the user as "nickname".

	It's recommended though, that each user is only given one authentication id of each 
	authentication type.
	
	
    Step by step explanation for how to create a user and give the user access to services 
    using both openid and magic url follows:
    
    !! Note that to keep it simple we don't have proper error handling in this example. Please read api's to add error handling. 
 
    0) ADD ACCESS-CONTROL TO A SERVICE:
    -----------------------------------
    
	| WebSubServer* service = g_webserver->GetSubServer(UNI_L("some-service"));    
    
    | WebserverResourceDescriptor_AccessControl *access_control_descriptor;	
    | BOOL allow_access_to_all_known_users = FALSE; // If this is set to TRUE, all authenticated and known users will have access to the service. 
											 // No need to add users to the service. This option should normally be set to FALSE. Unknown authenticated openid users
											 // will still not have access 
	| WebserverResourceDescriptor_AccessControl::Make(access_control_descriptor, UNI_L("/"), UNI_L("an auth text"), TRUE, allow_access_to_all_known_users);			
	| subserver->AddSubServerAccessControlResource(access_control_descriptor); 
	  	
  	
  	1) CREATE A USER:
  	----------------------

	| g_webserverUserManager->CreateUser(UNI_L("nick"));
	
	This will create a user "nick".   


	2) GIVE "NICK" ACCESS TO A SERVICE
	--------------------------------------
 	
 	First get the service:
	| WebSubServer* service = g_webserver->GetSubServer(UNI_L("some-service"));

	| WebserverResourceDescriptor_AccessControl *access_control_descriptor;	 		
	| service->GetSubserverAccessControlResource(UNI_L("/", access_control_descriptor);  // We assume that there have been added access control on path "/" on this service.
  	| access_control_descriptor->AddUser(UNI_L("nick"));
	
		
	3) ADD AUTHENTICATION ID'S TO "NICK"
	-----------------------------------------
	Before a user can authenticate we must add at least one authentication id to the 
	user.
	
	OPENID
	-------
	
	| WebserverAuthUser *user = g_webserverUserManager->GetUserFromName(UNI_L("nick"));	
	| WebserverAuthenticationId_OpenId *openid;
	| WebserverAuthenticationId_OpenId::Make(openid, UNI_L("nick.someopenidprovider.com")); 
	

	| if (OpStatus::IsError(user->AddAuthenticationId(openid)))
	| {
	|	delete user;
	| }
	
	If the user logs in with myopenid.someopenidprovider.com, he will now be recognized  
	as "nick". It is possible to add several openid's to one user, although
	we recommend only one per user.
	
	
	MAGIC URL
	---------
	
	| WebserverAuthenticationId_MagicUrl *magic_url_id;
	| WebserverAuthenticationId_MagicUrl::Make(magic_url_id, UNI_L("nick")); // You may use nick as auth-id (recommended),
	 																			but you can use any random string. 
	
	Add the id to a user
	| if (OpStatus::IsError(user->AddAuthenticationId(magic_url_id)))
	| {
	| 	delete user;
	| }

	You only need to add one magic url id to the user, since we can generate
	as many urls as needed from that.
	
	To get a magic url string:
		
	| OpString &magic_url_string;
	| WebSubServer* service = g_webserver->GetSubServer(UNI_L("some-service"));
	| magic_url_id->GetMagicUrlString(magic_url_string, service, UNI_L("some/path/inside/the/service"));
	
	This will generate an url similar to:
	"http://device.someuser.operaunite.com/some-service/some/path/inside/the/service?uniteauth.id=nick&uniteauth.auth=389668def2aeacd6de7f1496&uniteauth.type=magic-url"

	Note that uniteauth.id=nick
	and uniteauth.type=magic-url
	
*/

enum WebSubServerAuthType
{
	WEB_AUTH_TYPE_HTTP_MAGIC_URL = 0,
	WEB_AUTH_TYPE_HTTP_OPEN_ID,
	WEB_AUTH_TYPE_HTTP_DIGEST,		
	WEB_AUTH_TYPE_NONE // Allays keep NONE as the last type.
};

/* class for managing users, and authentication methods for users. Use the global variable g_webserverUserManager to access */

class WebserverUserManager
{
public:
	/* Persistent storage. */

	/** Save the users do disk. 
	 * 
	 * @return OpStatus::Success or OpStatus::ERR_NO_MEMORY. 
	 */
	OP_STATUS LoadUserList();

	/** Load the users from disk.
	 * 
	 * @return OpStatus::Success or OpStatus::ERR_NO_MEMORY. 
	 */	
	OP_STATUS SaveUserList();

	/** Get the iterator to list all the users.
	 * 
	 * The iterator is owned by WebserverUserManager. Do not delete!
	 * 
	 * @return The iterator.
	 */ 
	OpHashIterator *GetUserListIterator()  { return m_users_iterator; } 

	/** Create a user 
	 * 
	 * @param 	username 	The name of the user. Must be globally unique.
	 * @param 	email		The user's email.
	 * @return 	OpStatus::ERR if the user exists.
	 * 		  	OpStatus::ERR_NO_MEMORY for OOM.
	 * 			OpStatus::OK if success.
	 */
	OP_STATUS CreateUser(const OpStringC &username, const OpStringC &email = UNI_L(""));

	
	/** Delete a user 
	 * 
	 * @return 	OpStatus::ERR if the user is not found.
	 * 		  	OpStatus::ERR_NO_MEMORY for OOM.
	 * 			OpStatus::OK if success. 
	 */
	OP_STATUS DeleteUser(const OpStringC &username);

	
	/** Delete all users */
	void DeleteAllUsers();
		
	/** Getters */
	WebserverAuthUser *GetUserFromName(const OpStringC &username);
	WebserverAuthUser *GetUserFromSessionId(const OpStringC &session_id);
	WebserverAuthUser *GetUserFromAuthId(const OpStringC &auth_id,  WebSubServerAuthType type);
	WebserverAuthSession *GetAuthSessionFromSessionId(const OpStringC &session_id);
	WebserverAuthenticationId *GetAuthenticationIdFromId(const OpStringC &auth_id, WebSubServerAuthType type);

	/** Generate a random string.*/
	static OP_STATUS GenerateRandomTextString(OpString &random_hex, unsigned int length);
	
	BOOL AllowUnknownOpenIdUsers() { return m_allow_unknown_open_id_users; }
	void SetAllowUnknownOpenIdUsers(BOOL allow) { m_allow_unknown_open_id_users = allow; }
private:

	virtual ~WebserverUserManager();

	/** Singleton creator */
	static OP_STATUS Init(const OpStringC16 persistent_storage_file);	
	static void Destroy();

	OP_BOOLEAN CheckSessionAuthentication(WebserverRequest *request_object, const OpStringC &session_id, WebserverAuthUser *&user_object, WebserverAuthSession *&session);
	

	/** Add a user */
	OP_STATUS AddUser(WebserverAuthUser *user);
	/** Caller takes over ownership */
	OP_STATUS RemoveUser(WebserverAuthUser *user);	
	
	
#ifdef WEB_HTTP_DIGEST_SUPPORT	
	OP_STATUS CreateAuthenctionNonceElement(OpString8 &nonce);
	/* Constructs an nonceElement. NB! the element is owned by WebserverPrivateGlobalData */
	
	WebserverServerNonceElement *GetAuthenctionNonceElement(const OpStringC8 &nonce);
	
	OpAutoVector<WebserverServerNonceElement> m_authenctionNonces;
	
#endif // WEB_HTTP_DIGEST_SUPPORT
	
	WebserverUserManager();
	
	BOOL m_allow_unknown_open_id_users;
	
	OpHashIterator *m_users_iterator;
	
	OpAutoStringHashTable< WebserverAuthUser > m_users;
		
	OpString m_persistent_storage_file;
	
	friend class WebserverModule;
	friend class WebserverRequest;
	friend class WebResource_Auth;	
};

class WebserverAuthUser
{
public:
	/** Add an authentication id type to this user.
	 * 
	 * An authentication id enables the user to log in using the
	 * authentication method given by the id.
	 * For example openId, or magic-url.
	 * 
	 * The auth-id of the authentication id must be unique for all ids of the same WebSubServerAuthType.
	 * 
	 * @param auth_id	 	The authentication.
	 * @return 				OpStatus::ERR if auth-id not unique.
	 * 						OpStatus::ERR_NO_MEMORY for OOM.
	 * 						OpStatus::OK for success.
	 */
	OP_STATUS AddAuthenticationId(WebserverAuthenticationId *auth_id);

	/** Remove an authentication id from this user 
	 * 
	 * @param auth_id 			The id string
	 * @param type				The type of the id-object.
	 * @return OpStatus::ERR 	if the id did not exist.
	 * 		   OpStatus::OK 	if success.
	 *   
	 * */
	OP_STATUS RemoveAuthenticationId(const OpStringC &auth_id, WebSubServerAuthType type);

	/** Get an iterator to list out all the authentication objects for this user */
	OpHashIterator *GetAuthenticationIterator()  { return m_users_iterator; }	

	/** Get the username */ 
	const uni_char *GetUsername() const { return m_username.CStr();}
	
	/** Get the email. */
	const uni_char *GetUserEmail() const { return m_user_email.CStr();}

	/** Set user to temporary.
	 *
	 * @param temporary If true, the user will not be saved disk.  
	 */
	void SetTemporary(BOOL temporary) { m_temporary = temporary; }
	
	/** Check if user is temporary.
	 * 
	 * @return TRUE if temporary.
	 */
	BOOL IsTemporary() { return m_temporary; }
	
	/** Removes the session. This will log out the user owning this session. */
	BOOL RemoveSession(const OpStringC &session_id);
	
	/** Removes all sessions. This will log out the user for all sessions. */
	void RemoveAllSessions();

	virtual ~WebserverAuthUser();
	
private:
	WebserverAuthUser();	
	WebserverAuthenticationId *GetAuthenticationIdFromId(const OpStringC &auth_id, WebSubServerAuthType type) const ;
	
	OP_STATUS CheckAuthentication(WebserverRequest *request_object, const OpStringC &auth_id, WebResource_Auth *auth_resource, WebSubServerAuthType authentication_type);

	WebserverAuthSession *GetAuthSessionFromSessionId(const OpStringC &session_id) const;
		
	static OP_STATUS Make(WebserverAuthUser *&user, const OpStringC &username, const OpStringC &email = UNI_L(""));
	static OP_STATUS MakeFromXML(WebserverAuthUser *&user, XMLFragment &user_xml);

	OP_STATUS CreateXML(XMLFragment &user_xml);

	OpString m_username;
	OpString m_user_email;
	
	OpAutoVector<WebserverAuthenticationId> m_auth_ids;
	
	OpAutoStringHashTable<WebserverAuthSession> m_auth_sessions;

	OpHashIterator *m_users_iterator;
	
	/** If true, this user will not be saved to disk */
	BOOL m_temporary;
	
	friend class WebserverAuthenticationId;
	friend class WebserverUserManager;
	friend class WebResource_Auth;
};

/* virtual class for authentication methods. */
class WebserverAuthenticationId 
{
public:	
	virtual ~WebserverAuthenticationId();
		
	static OP_STATUS MakeFromXML(WebserverAuthenticationId *&authentication_id, XMLFragment &user_xml);
	
	virtual OP_STATUS CreateXML(XMLFragment &user_xml);
		
	virtual WebSubServerAuthType GetAuthType() const = 0;
	
	virtual const uni_char *GetAuthId() const { return m_auth_id.CStr(); }	
	virtual OP_STATUS SetAuthId(const OpStringC &auth_id){ return m_auth_id.Set(auth_id); }

	WebserverAuthUser *GetUser() const { return m_user; }
	
	static const uni_char *AuthTypeToString(WebSubServerAuthType type);	
	static WebSubServerAuthType AuthStringToType(const uni_char *authentication_type);
	
	enum AuthState
	{
		SERVER_AUTH_STATE_SUCCESS,						// If authenticated.
		SERVER_AUTH_STATE_FAILED,								// If authentication failed.
		SERVER_AUTH_STATE_ANOTHER_AGENT_IS_AUTHENTICATING, 	// If another user agent is in the progress of authenticating.
		SERVER_AUTH_STATE_AUTH_SERVER_ERROR	
	};
	
protected:

	OP_STATUS CreateSessionAndSendCallback(AuthState success);
	virtual OP_STATUS CheckAuthentication(WebserverRequest *request_object) = 0;
	virtual OP_STATUS CreateAuthXML(XMLFragment &user_xml) = 0;

	WebserverAuthenticationId();
	WebResource_Auth *m_auth_resource;
private:
	
	virtual OP_STATUS CheckAuthenticationBase(WebserverRequest *request_object, WebResource_Auth *auth_resource);
		
	void AccessControlResourceKilled() { OP_ASSERT(m_auth_resource != NULL); m_auth_resource = NULL; }
	
	void SetUser(WebserverAuthUser *user) { m_user = user; }
	
	OpString m_auth_id;	

	WebserverAuthUser *m_user;

	friend class WebserverAuthUser;
	friend class WebResource_Auth;
};

class WebserverAuthSession
{
public:

	static OP_STATUS Make(WebserverAuthSession *&session, const OpStringC &session_id, WebSubServerAuthType type);
	virtual ~WebserverAuthSession(){}

	const uni_char *GetSessionId() const { return m_session_id.CStr(); }
	const uni_char *GetSessionAuth() const  { return m_session_auth.CStr(); }
	WebSubServerAuthType GetSessionAuthType() const { return m_type; }
	
private:
	WebserverAuthSession(WebSubServerAuthType m_type);

	OpString m_session_id;
	OpString m_session_auth;
	
	WebSubServerAuthType m_type;
	/* FIXME: Add Max age */
};

#endif /*WEBSERVER_USER_MANAGEMENT_H_*/
