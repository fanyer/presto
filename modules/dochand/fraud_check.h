// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef DOCHAND_FRAUD_CHECK_H
#define DOCHAND_FRAUD_CHECK_H

#ifdef TRUST_RATING

class XMLParser;

#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmlparser.h"

#include "modules/about/opgenerateddocument.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/security_manager/include/security_manager.h"

#define SITECHECK_HOST "sitecheck2.opera.com" ///< hostname of server to contact for sitecheck
#define SITECHECK_HDN_SUFFIX "-Oscar0308"    ///< suffix used for the hdn parametre of the check
#define PHISHING_WARNING_URL "opera:site-warning"   ///< URL of generated page used for warning agains phishing sites and sites distributing malware
#define PHISHING_WARNING_URL_PATH  "site-warning"

#define FRAUD_CHECK_MINIMUM_GRACE_PERIOD (4 * 60 * 1000)  // 4 minutes in milliseconds 
#define FRAUD_CHECK_MAXIMUM_GRACE_PERIOD (64 * 60 * 1000) // 64 minutes in milliseconds

class TrustInfoParser;

class ServerTrustChecker : public Link
{
public:
	ServerTrustChecker(UINT id, DocumentManager* docman);
	~ServerTrustChecker();
	OP_STATUS			Init(URL& url);

	OP_STATUS			AddURL(URL& url);
	BOOL				URLBelongsToThisServer(URL& url);
	BOOL				IsCheckingURL(URL& url);

	OP_STATUS			StartCheck(BOOL resolve_first);

	/// called by parser when it has the results.  May block the page if
	/// one of the results is a fraud warning
	OP_STATUS			CheckDone(BOOL check_succeeded);

	static OP_STATUS	GetTrustRating(URL& url, TrustRating& rating, BOOL& needs_online_check);

	static OP_STATUS 	IsLocalURL(URL& url, BOOL& is_local, BOOL& need_to_resolve);

	UINT				GetId() { return m_id; }
protected:
	class URLCheck : public Link
	{
	public:
		URL				url;
        unsigned		type;
	};

	UINT				m_id;
	URL					m_initial_url;
	AutoDeleteHead		m_checking_urls;
	TrustInfoParser*	m_parser;
	ServerName*			m_server_name;
	DocumentManager*	m_docman;
};



class TrustInfoParser : public XMLTokenHandler, public XMLParser::Listener, public MessageObject
{
public:
	TrustInfoParser(ServerTrustChecker* checker, MessageHandler* mh);
	OP_STATUS Init();

	~TrustInfoParser();

	/*
	 *  Public API - these static methods can safely be called from outside core
	 *  the other methods should not be called from outside core
	 */

	/**
	 *  Generates the URL necessary for sending a request for checking a URL against
	 *  the sitecheck server.
	 *  @param url_to_check [in]  The URL to be checked
	 *  @param request_url [out]  The URL to sitecheck to use to check if the
	 *                            first URL is a trusted site
	 *  @param request_info_page  If TRUE the request will be one which returns
	 *                            a HTML page with the result which can be displayed
	 *                            in the UI.  If FALSE the result will be a XML page
	 *                            which this class can parse itself.
	 *  @param unused             Unused parameter, will be removed
	 *  @return					  If the return status is OpStatus::OK then the request_url
	 *                            is valid.
	 */
	static OP_STATUS GenerateRequestURL(URL& url_to_check, URL& request_url, BOOL request_info_page=TRUE, const char* unused=NULL);

	/**
	 * Generates a base64-encoded MD5-hash from the full path.
	 * @param to_generate_hash_from the URL path to generate a MD5-hash from
	 * @param hash The string were the hash will be returned
	 */
	static OP_STATUS GenerateURLHash(URL& to_generate_hash_from, OpString8& hash);

	static OP_STATUS GetNormalizedHostname(URL& url, OpString8& hostname);

	/** Calculates the MD5 hash of the buffer and writes it as a base64
	 *  encoded line
	 *  @param input The string to take make a hash value from
	 *  @param md5hash The resulting md5-hash. Will be newly allocated memory which
	 *                 has to be delete[]-ed if the return status is OpStatus::OK
	 *  @param url_escape_hash If TRUE then pluses in the hash will be escaped as %2B,
	 *                 safe for use in an URL.
	 */
	static OP_STATUS CalculateMD5Hash(const char* input, char*& md5hash, BOOL url_escape_hash);
	static OP_STATUS CalculateMD5Hash(OpString& input, char*& md5hash, BOOL url_escape_hash);

	/*
	 *  End of public API
	 *  Don't call any of the methods below from outside core (and preferably not from outside doc or xmlparser)
	 */


	/** Will escape all pluses with %2Bs in string.
	 *  If there are no characters to escape then it returns the same string
	 *
	 *  Else it will return a new[]-ed string which is escaped (or NULL on OOM)
	 * and *** string will be deleted[]! ***/
	static char* EscapePluses(char* string);

	OP_STATUS CheckURL(URL& url_to_check, const char* unused=NULL);

	static OP_BOOLEAN RegExpMatchUrl(const uni_char* reg_exp, const uni_char* url);
	
	/* Does this URL match one of the blacklisted URLs/regexps for this
	 * server */
	static OP_BOOLEAN MatchesUntrustedURL(const uni_char* url_string, ServerName* server_name
                                         , const uni_char **matching_text = NULL);

	OP_STATUS ResolveThenCheckURL(URL& url_to_check, const char* full_path_hash);

	BOOL IsCheckingServerName(ServerName*);
	void SetId(UINT id);

	// Callbacks from XML parser
	Result HandleToken(XMLToken &token);
	void Continue(XMLParser* parser);
	void Stopped(XMLParser* parser);

	// Token handlers:
	OP_STATUS HandleTextToken(XMLToken &token);
	OP_STATUS HandleStartTagToken(XMLToken &token);
	OP_STATUS HandleEndTagToken(XMLToken &token);

	// Callback for MessageObject's MSG_COMM_LOADING_FAILED and MSG_COMM_NAMERESOLVED.
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void OnHostResolved(OpSocketAddress* address);

	class Advisory : public Link
    {
    public:
        Advisory() : 
            homepage_url(NULL),
            advisory_url(NULL),
            text(NULL),
            type(0),
            id(0) 
        {}
        ~Advisory()
        {
            OP_DELETEA(homepage_url);
            OP_DELETEA(advisory_url);
            OP_DELETEA(text);
        }

        uni_char *homepage_url;
        uni_char *advisory_url;
        uni_char *text;
        unsigned type;
        unsigned id;
    };

    class UrlListItem : public Link
    {
    public:
        UrlListItem() : 
            url(NULL),
            src(0)
        {}
        ~UrlListItem()
        {
            OP_DELETEA(url);
        }

        uni_char *url;
        unsigned src;
    };

    Advisory* GetAdvisory(const uni_char *matching_text);
    const Head& GetAdvisoryList() const { return m_advisory_list; }

protected:
	enum ElementType
	{
		TrustLevelElement,
		HostElement,
		PhElement,
		ClientExpireElement,
		RegExpElement,
		UrlElement,
		SourceElement,
		OtherElement	
	};

	static OP_STATUS NormalizeURL(URL& url, OpString8& normalized_url, OpString8& normalized_hostname);

	ServerTrustChecker*		m_checker;

	XMLParser*				m_xml_parser;
	ElementType				m_current_element;

	URL						m_host_url;
	BOOL					m_in_blacklist;

	MessageHandler*			m_message_handler;
	OpSecurityState			m_state;
	OpString8				m_full_path_hash;

    Advisory *m_current_advisory;
    UrlListItem *m_current_url;
    Head m_advisory_list;
    Head m_url_list;
};

#endif // TRUST_RATING
#endif /*DOCHAND_FRAUD_CHECK_H*/
