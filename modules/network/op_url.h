/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OPURL_H__
#define __OPURL_H__

#include "modules/url/url2.h"
#include "modules/about/opillegalhostpage.h"

/** @class OpURL
 *
 *  OpURL contains the URL. This could be for a file, http, ftp or other resource.
 *  It is used together with OpRequest to retrieve the resources.
 *
 *  Usage:
 *  @code
 * 	OpURL url = OpURL::Make("http://t/core/networking/http/cache/data/blue.jpg");
 *  @endcode
 *
 *  NOTE: Below, when it says "non-special escaped sequences are unescaped", it means that
 *  escape sequences such as "%30" will be replaced with the character they represent (in this
 *  case "0"), unless the character is special to the syntax of URLs or HTML code (or in a
 *  wider sense anything that can be used for trickery to open security holes), in which case
 *  it will be left alone. The query part of the url (following the "?") will not be touched.
 *  The characters that are considered special and that will not be unescaped in any case are
 *  00..1f," \"#$%&+,/:;<=>?@[\\]^{|}",80..ff. So even if we unescape, the output string
 *  should always be safe, and since it is not common to escape the other characters it will
 *  rarely differ from the "escaped" form. One reason to still choose the escaped form of
 *  output is if we want to present the original form of the URL.
 */

class OpURL
{
public:
	OpURL(){}
	OpURL(const OpURL &url);
	OpURL &operator=(const OpURL &url);

	/** Get an OpURL for the given urlname and fragment.
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL. May include a fragment identifier.
	 */
	static OpURL Make(const uni_char *url);

	/** Get an OpURL for the given urlname and fragment
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL. May include a fragment identifier
	 */
	static OpURL Make(const char *url);

	/** Get an OpURL for the given urlname and fragment
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 */
	static OpURL Make(const char *url, const char *rel);

	/** Get an OpURL for the given urlname and fragment
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 */
	static OpURL Make(const uni_char *url, const uni_char *rel);

	/** Get an OpURL for the given urlname and fragment, The name may be an absolute URL, or a
	 *	relative URL resolved relative to the parent URL
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, may be a relative URL.
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 */
	static OpURL Make(OpURL prnt_url, const char *url, const char *rel);

	/** Get an OpURL for the given urlname and fragment, The name may be an absolute URL, or a
	 *	relative URL resolved relative to the parent URL
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, May be a relative
	 *					URL, and may include a fragment identifier
	 */
	static OpURL Make(OpURL prnt_url, const uni_char *url);

	/** Get an OpURL for the given urlname and fragment, The name may be an absolute URL, or a
	 *	relative URL resolved relative to the parent URL
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, may be a relative URL.
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 */
	static OpURL Make(OpURL prnt_url, const uni_char *url, const uni_char *rel);

	/** Get an OpURL for the given urlname and fragment, The name may be an absolute URL, or a
	 *	relative URL resolved relative to the parent URL
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, May be a relative
	 */
	static OpURL Make(OpURL prnt_url, const char *url);

	/** @return TRUE if the two OpURL's are the same, i.e. that they would resolve to the same resource. Disregards fragment part. */
	BOOL operator==(const OpURL &url) const;

	/** Is this OpURL empty, i.e. not valid?
	 *  @return TRUE if it is an empty URL.
	 */
	BOOL IsEmpty() const;

	/** Get scheme for URL, if known. Otherwise return URL_UNKNOWN.
	 *  @return URLType representing the URL scheme.
	 */
	URLType GetType() const;

	/** Get the real scheme for a URL, see CORE-21372.
	 *  This is used when dom code registers a new scheme that acts like a HTTP scheme, but has another real name/type.
	 *  @return URLType representing the real URL scheme.
	 */
	URLType GetRealType() const;

	/** Get the name of the scheme used by the URL, corresponding to GetType().
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetTypeName(OpString8 &val) const;

	/** Get the actual port this URL will access.
	 *  @return a port number
	 */
	unsigned short GetResolvedPort() const;

	/** Get the port designated by the URL, 0 is default port for the protocol.
	 *  @return a port number, or 0
	 */
	unsigned short GetServerPort() const;

	/** Used by Same Server check */
	enum Server_Check {
		/** Don't check port */
		KNoPort,
		/** Check port */
		KCheckPort,
		/** Check resolved port */
		KCheckResolvedPort
	};

	/** Has this URL a ServerName and is it the same server (and optionally, port or resolved port) as the other URL?
	 *  @param  other        The other URL.
	 *  @param  include_port Also check if it is the same port?
	 *  @return TRUE if there is a match.
	 */
	BOOL IsSameServer(OpURL other, OpURL::Server_Check include_port = OpURL::KNoPort) const;

	/** Does this URL have a ServerName?
	 *  @return TRUE if there is a ServerName object
	 */
	BOOL HasServerName() const;

	/** Get the ServerName object.
	 *  @return the ServerName, or NULL
	 */
	void GetServerName(class ServerNameCallback *callback) const;

	/** Get the path part of the URL (does not include the query portion). Non-special escaped sequences are unescaped.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPath(OpString8 &val) const;

	/** Get the path part of the URL (does not include the query portion).
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPath(OpString &val, BOOL escaped = FALSE, unsigned short charsetID = 0) const;

	/**	Get The fragment part of the URL.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetFragmentName(OpString8 &val) const;

	/**	Get The fragment part of the URL.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetFragmentName(OpString &val) const;

	/** Get the name of the server.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetHostName(OpString8 &val) const;

	/** Get the name of the server, converted to unicode according to IDNA.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetHostName(OpString &val) const;

	/** Get the user name part of the URL. If user name is not set this will return an empty string.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetUsername(OpString8 &val) const;

	/** Get the password part of the URL.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPassword(OpString8 &val) const;

	/** Get query component of URL. No unescaping performed. When a query is present, this string will always include the leading "?".
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetQuery(OpString8 &val) const;

	/** Get query component of URL. No unescaping or charset conversion performed. When a query is present, this string will always include the leading "?".
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetQuery(OpString &val) const;

	/** Get the "hostname:port" of the server.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetHostNameAndPort(OpString8 &val) const;

	/** Get the "hostname:port" of the server. Host name is converted to unicode according to IDNA.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetHostNameAndPort(OpString &val) const;

	/** The name of the URL, without username and password.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetName(OpString8 &val, BOOL escaped = FALSE) const;

	/** The name of the URL, without username and password. Host name is converted to unicode according to IDNA.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetName(OpString &val, BOOL escaped = FALSE, unsigned short charsetID = 0) const;

	/** How to display password. */
	enum Password_Display {
		/** No password. */
		NoPassword,
		/** Hidden password. */
		PasswordHidden,
		/** Visible password. MUST NOT be exported to the UI or
		 *  scripts unless there is a very good reason for it!
		 */
		PasswordVisible_NOT_FOR_UI
	};

	/** The name of the URL, including username but not password.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped.
	 *  @param  password  How the password (if any) should be displayed.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameUsername(OpString8 &val, BOOL escaped = FALSE, Password_Display password = NoPassword) const;

	/** The name of the URL, including username but not password. Host name is converted to unicode according to IDNA.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  password  How the password (if any) should be displayed.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameUsername(OpString &val, BOOL escaped = FALSE, Password_Display password = NoPassword, unsigned short charsetID = 0) const;

	/** The name of the URL with the fragment identifier, without username and password.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameWithFragment(OpString8 &val, BOOL escaped = FALSE) const;

	/** The name of the URL with the fragment identifier, without username and password. Host name is converted to unicode according to IDNA.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameWithFragment(OpString &val, BOOL escaped = FALSE, unsigned short charsetID = 0) const;

	/** The name of the URL with the fragment identifier, including username but not password.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped.
	 *  @param  password  How the password (if any) should be displayed.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameWithFragmentUsername(OpString8 &val, BOOL escaped = FALSE, Password_Display password = NoPassword) const;

	/** The name of the URL with the fragment identifier, including username but not password. Host name is converted to unicode according to IDNA.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  password  How the password (if any) should be displayed.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameWithFragmentUsername(OpString &val, BOOL escaped = FALSE, Password_Display password = NoPassword, unsigned short charsetID = 0) const;

	/** Get the path and query part of the URL.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPathAndQuery(OpString &val, BOOL escaped = FALSE, unsigned short charsetID = 0) const;

	/** Get the path and query part of the URL. No unescaping performed.
	 *  @param  val       The returned string.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPathAndQuery(OpString8 &val) const;

	/** The name of the URL to be used for reading data.
	 *  It is a GetName with path gotten by GetPathFollowSymlink. Non-special escaped sequences are unescaped.
	 *  @param  val       The returned string.
	 *  @param  charsetID If set, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetNameForData(OpString &val, unsigned short charsetID = 0) const;

	/** Path to use for reading data. It is either a GetPath or a GetPathSymlinkTarget.
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPathFollowSymlink(OpString &val, BOOL escaped = FALSE, unsigned short charsetID = 0) const;

	/** Get pointer to scheme specification structure, defined in "prot_sel.h". */
	protocol_selentry *GetProtocolScheme() const;

#ifdef URL_FILE_SYMLINKS
	/** Target path of a URL that is a "symbolic link".
	 *  @param  val       The returned string.
	 *  @param  escaped   If FALSE, non-special escaped sequences are unescaped. If TRUE, charsetID is ignored.
	 *  @param  charsetID If set and escaped==FALSE, the url path is converted to unicode based on the given charset. Otherwise utf-8 is assumed.
	 *                    All escaped sequences are unescaped before charset conversion is applied.
	 *  @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS GetPathSymlinkTarget(OpString &val, BOOL escaped = FALSE, unsigned short charsetID = 0) const;
#endif // URL_FILE_SYMLINKS

#ifdef ERROR_PAGE_SUPPORT
	/** @return Type of problem if the url is invalid. */
	virtual OpIllegalHostPage::IllegalHostKind GetInvalidURLKind() const;
#endif // ERROR_PAGE_SUPPORT

private:
	// The following reference to the old URL class and the associated constructor will be removed in the future
	URL m_url;
	OpURL(const URL &url);

	OpURL(const OpURL &url, const uni_char *rel);
	OpURL(URL_Rep *url_rep, const char *rel);
	const URL &GetURL() const { return m_url; }
	friend class Network;
	friend class OpRequestImpl;
	friend class OpResourceImpl;
	friend class OpResponseImpl;
	friend class SSL_CertificateVerifier;
#ifdef SELFTEST
	friend class ST_opurl_tests;
#endif
};

#include "modules/network/src/op_url_inl.h"

#endif
