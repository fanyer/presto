#ifndef WEBSERVER_DIGEST_AUTH_H_
#define WEBSERVER_DIGEST_AUTH_H_

#ifdef WEB_HTTP_DIGEST_SUPPORT
/* Work in progress. Do not use */

class WebserverServerNonceElement
{
public:
	WebserverServerNonceElement();
	
	~WebserverServerNonceElement();
	
	/* construct a nonce
	 * return The constructed nonce
	 */
	OP_STATUS ContructNonce();

	BOOL CheckAndSetUsedNonce(unsigned int clientNonce);

	/*return the nonce*/
	const OpStringC8 &GetNonce();
	
	/*returns the time stamp*/
	double GetTimeStamp();
	
private:
	OpString8 m_nonce;
	UINT32 m_nc; /*32 bit nonce counter*/
	double m_timeStamp;	
	UINT32 *m_nonceBitTable;
};

class WebserverAuthenticationId_HttpDigest 
	: public WebserverAuthenticationId
{ 
public:
	
	static OP_STATUS Make(WebserverAuthenticationId_HttpDigest *&auhtentication_object, const OpStringC &username, const OpStringC &password); 
	static OP_STATUS MakeFromXML(WebserverAuthenticationId_HttpDigest *&auhtentication_object, XMLFragment &auth_element);
	                             
	virtual WebSubServerAuthType GetAuthType() { return WEB_AUTH_TYPE_HTTP_DIGEST; }

	virtual OP_STATUS CheckAuthentication(WebserverRequest *request_object);
	
	virtual OP_STATUS CreateAuthXML(XMLFragment &auth_element);
		
private:
	
	static unsigned int HexStringToUint(const char *hexString, unsigned int length);
	
	static OP_STATUS calculateCorrectResponseDigest( const OpStringC &userName,
															const OpStringC &HA1_hash,
															const OpStringC8 &OriginalRequestURI,
															const OpStringC8 &method,
															const OpStringC8 &nc,
															const OpStringC8 &cnonce,
															const OpStringC8 &qop,
															const OpStringC8 &nonce,													
															/*out*/ OpString8 &reponse);
	
	OP_STATUS GenerateDigestResponse(BOOL nonceExpired);
	
	static OP_STATUS calculateHA1(const OpStringC &user, const OpStringC &password, const OpStringC &realm, OpString &digest);
	static OP_STATUS calculateHA2(const OpStringC8 &method, const OpStringC8 &originalUri, OpString8 &digest);
	
	static OP_BOOLEAN checkBasicAuthentication(ParameterList *authHeaderParameters,
															const OpStringC8 &uriDirectiveValue,
															const OpStringC8 &method,
															OpAutoStringHashTable< WebserverAuthUser > &m_user_list,
															/*out*/ BOOL &nonceExpired );
	
	static OP_BOOLEAN checkDigestAuthentication(ParameterList *authHeaderParameters,
															const OpStringC8 &uriDirectiveValue,
															const OpStringC8 &method,
															OpAutoStringHashTable< WebserverAuthUser > &m_user_list,
															/*out*/ BOOL &nonceExpired );	
};
#endif // WEB_HTTP_DIGEST_SUPPORT
#endif /*WEBSERVER_DIGEST_AUTH_H_*/
