/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef _CONTENT_FILTER_H_INCLUDED_
#define _CONTENT_FILTER_H_INCLUDED_

#ifdef URL_FILTER

#include "modules/dochand/win.h"
#include "modules/prefsfile/accessors/ini.h"
#include "modules/url/url2.h"
#include "modules/logdoc/html.h"

#ifdef SYNC_CONTENT_FILTERS
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_dataitem.h"
#endif // SYNC_CONTENT_FILTERS

// Defines used for testing only
#ifdef _DEBUG
	//#define FILTER_BYPASS_RULES
	//#define FILTER_REDIRECT_RULES
	//#define CF_BLOCK_OPERA_DEBUG
#endif

class URL;
class PrefsFile;
class URLFilter;
class ExtensionList;
class DOMLoadContext;
class DOM_Environment;
class DOM_Runtime;
class DOM_Object;

#if defined(SUPPORT_VISUAL_ADBLOCK) && defined(QUICK)
class SimpleTreeModel;
#endif // SUPPORT_VISUAL_ADBLOCK and QUICK
class FilterURLList;

/// Listener notified every time that an URL is blocked
class URLFilterListener
{
public:
	/// Fires when a URL has been blocked
	virtual void URLBlocked(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)=0;
	/// Destructor
	virtual ~URLFilterListener() { }
};

/// Listener notified every time that an URL is blocked
class URLFilterExtensionListener: public URLFilterListener
{
public:
	/// Fires when a URL has been allowed by the white list after being blocked by the black list
	virtual void URLUnBlocked(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)=0;

#ifdef SELFTEST
	/// Testing event fired when a URL has been allowed.
	/// While URLUnBlocked() requires the URL to be blocked by the black list but allowed by the whitelist,
	/// URLAllowed() fires when the URL is not blocked by the black list (and the white list is not checked at all)
	/// This is only intended for testing, as it could impact too much the performance of Opera
	virtual void URLAllowed(const uni_char* url, OpWindowCommander* wic, DOMLoadContext *dom_ctx)=0;
#endif // SELFTEST
};

class URLFilterExtensionListenerOverride: public URLFilterExtensionListener
{
private:
	/// Listener meant to be called  
	URLFilterExtensionListener *original_listener;

public:
	/// Set the listener that should have been called is no override listener was available
	void SetOriginalListener(URLFilterExtensionListener *original_listener) { this->original_listener = original_listener; }
	/// @return the original listener
	URLFilterExtensionListener *GetOriginalListener() { return original_listener; }
};

/// Listener for the asynchronous URL filter.
class AsyncURLFilterListener
{
public:
	/// Callback class for the async callback reply on OnCheckURLAllowed
	class CheckURLAllowedCallback
	{		
	public:
		virtual ~CheckURLAllowedCallback() {};
		virtual void CheckURLAllowedReply(BOOL allowed) = 0;
	};

	/** Called each time a URL is loaded.
	 *  @param url URL that is being loaded
	 *  @param cbk Callback function to call with async respons
	 *  @param wait Set this flag to FALSE if no async callback will be made.
	 */
	virtual OP_STATUS OnCheckURLAllowed(const uni_char* url,
										CheckURLAllowedCallback* cbk,
										BOOL& wait)=0;
	/// Destructor
	virtual ~AsyncURLFilterListener() { }
};

/// Holds information about whether a URL is allowed or not in the given context
class CheckURLAllowedContext :
	public ListElement<CheckURLAllowedContext>,
	public AsyncURLFilterListener::CheckURLAllowedCallback
{
protected:
	MH_PARAM_1 m_id;	
	OpMessage m_msg;
	BOOL m_is_canceled;

public:
	CheckURLAllowedContext(MH_PARAM_1 id, OpMessage msg)
		: m_id(id), m_msg(msg), m_is_canceled(FALSE) {}
	virtual ~CheckURLAllowedContext() { Out(); }

	MH_PARAM_1 Id() { return m_id; }
	void Cancel() { m_is_canceled = TRUE; }

	// AsyncURLFilterListener::CheckURLAllowedCallback
	void CheckURLAllowedReply(BOOL allowed);
};

/** Algorithms used to try to achieve the best possible performance */
enum FilterAlgorithm
{
	/** Always use the "fast version" (slow setup time, quick execution) */
	FILTER_FAST,
	/** Always use the "slow version" (fast setup time, slow execution) */
	FILTER_SLOW,
	/** Try to switch between FAST and slow to improve performances. At the moment, it switched to fast with more than 25 rules */
	FILTER_ADAPTIVE
};

/// Enum that identifies which element requested the loading process; multiple HTML tags can be grouped in a single HTMLResource
enum HTMLResource
{
	/// A generic HTML Tag not included in the other categories
	RESOURCE_OTHER=0x00000001,
	/// javascript urls
	RESOURCE_SCRIPT=0x00000002,
	/// images, favicon, background images, ...
	RESOURCE_IMAGE=0x00000004,
	/// Stylesheets, CSS
	RESOURCE_STYLESHEET=0x00000008,
	/// flash, java, media-player, plugins
	RESOURCE_OBJECT=0x00000010,
	/// frames and iframes
	RESOURCE_SUBDOCUMENT=0x00000020,
	// Main document
	RESOURCE_DOCUMENT=0x00000040,
	/// Timed refresh
	RESOURCE_REFRESH=0x00000080,
	/// XMLHttpRequest
	RESOURCE_XMLHTTPREQUEST=0x00000800,
	/// Object subrequest, resources loaded by the plugins
	RESOURCE_OBJECT_SUBREQUEST=0x00001000,
	/// HTML 5 media types: audio, video tags
	RESOURCE_MEDIA=0x00004000,
	/// Fonts
	RESOURCE_FONT=0x00008000,
	/// The source is unknown
	RESOURCE_UNKNOWN=0
};

/// Class describing the "context" that is loading the URL, to enable a finer filtering
class HTMLLoadContext
{
private:
	// Type of the element
	HTMLResource resource;
	// Domain of the page
	ServerName *domain;
	// Listener than can ovverride or supplement the extension listeners
	URLFilterExtensionListenerOverride *listener;
	/// TRUE if the URL module should bypass CheckURL check
	BOOL url_bypass;
	/// TRUE if the call is executed from a widget
	BOOL widget;

public:
	HTMLLoadContext(HTMLResource resource, ServerName *domain, URLFilterExtensionListenerOverride *listener = NULL, BOOL widget = FALSE) { this->resource=resource; this->domain=domain; this->listener = listener; url_bypass = FALSE; this->widget = widget; }
	HTMLLoadContext(HTMLResource resource, URL &url, URLFilterExtensionListenerOverride *listener = NULL, BOOL widget = FALSE) { this->resource=resource; this->domain=(ServerName *) url.GetAttribute(URL::KServerName, NULL); this->listener = listener; url_bypass = FALSE; this->widget = widget; }

	/// @return the type of resource that initiated the request, if available
	HTMLResource GetResource() const { return resource; }
	/// @return the domain, if available
	ServerName *GetDomain() { return domain; }
	/// @return the listener that can override or supplment the extension listeners
	URLFilterExtensionListenerOverride *GetListenerOverride() { return listener; }
	/// @return TRUE if the load originated from a widget
	BOOL IsFromWidget() { return widget; }

	/// Converts a HTML_ElementType to a HTMLLoadingElement constant
	static HTMLResource ConvertElement(HTML_Element *elem);
	/// Converts a string (with the values defined by the specification, e.g. RESOURCE_DOCUMENT) to a HTMLLoadingElement constant
	static HTMLResource ParseResource(uni_char *elem);
	/// Get if the URL module should bypass CheckURL()
	BOOL GetURLBypass() { return url_bypass; }
	/// Set if the URL module should bypass CheckURL()
	void SetURLBypass(BOOL b) { url_bypass = b; }
};

/// Context used to generate the DOM Events sent by content_filter extensions
class DOMLoadContext
{
private:
	friend class DOM_ExtensionURLFilter;
	/// HTML_Element that requested the load
	HTML_Element *element;
	/// Environment of the Element
	DOM_Environment *env;

	/// Retrieves the DOM object that can be used to notify the event handler
	OP_STATUS GetDOMObject(DOM_Object *&obj, OpGadget *owner);
	/// Returns the environment of the element
	DOM_Environment *GetEnvironment() { return env; }

public:
	DOMLoadContext(DOM_Environment *env, HTML_Element *element) { this->element = element; this->env = env; }

	/// Returns the HTMLResource of the context
	HTMLResource GetHTMLResource() { return HTMLLoadContext::ConvertElement(element); }

	// Return the Markup type of the element
	Markup::Type GetMarkupType();
};

/// Interface of a "rule" (conditions that can be added to a pattern)
/// Each pattern can have an optional list of rules. It's important to understand that when there is a match in a pattern:
/// * if the pattern has no rules, then the match is valid
/// * If there are rules, then the match is valid only if at least one rule does apply
class FilterRule
{
public:
	/**
	* @param url URL currently under check
	* @param html_ctx Context to use for the check
	* @return TRUE if the rule applies to this pattern, validating the match
	*/
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx) = 0;

	virtual ~FilterRule() { }
};

#ifdef SELFTEST
/// Debug class of a rule that always accepts the match
class DebugRuleAlways: public FilterRule { virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx) { return TRUE; }; };

/// Debug class of a rule that never accepts the match
class DebugRuleNever: public FilterRule { virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx) { return FALSE; }; };
#endif // SELFTEST

/// Rule that verifies if the domain is a given one. E.g. for rule "Block FB from CNN."
class RuleInDomain: public FilterRule
{
protected:
	/// Domain to check
	ServerName *server_name;

public:
	RuleInDomain(ServerName * domain) { OP_ASSERT(domain); server_name = domain; }
	RuleInDomain(URL &url) { server_name = (ServerName *) url.GetAttribute(URL::KServerName, NULL); OP_ASSERT(server_name);  }

	virtual ~RuleInDomain() { }
	
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx)
	{ if(!html_ctx || !html_ctx->GetDomain()) return FALSE; return server_name && html_ctx->GetDomain()==server_name; };
};

/// Rule that verifies if the URL is called in its domain or in one of its parent domain (it checks all of them)
class RuleInDomainDeep: public RuleInDomain
{
public:
	RuleInDomainDeep(ServerName *domain): RuleInDomain(domain) { }
	RuleInDomainDeep(URL &url): RuleInDomain(url) { }
	
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx);
};

/// Rule that verifies if the domain is not a given one. E.g. for rule "Block FB everywhere except in CNN."
/// This rule is also used for 3rd party check, which is a corner case of "Not in domain"
class RuleNotInDomain: public RuleInDomain
{
public:
	RuleNotInDomain(ServerName *domain): RuleInDomain(domain) { }
	RuleNotInDomain(URL &url): RuleInDomain(url) { }

	virtual ~RuleNotInDomain() { }
	
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx)
	{ if(!html_ctx || !html_ctx->GetDomain()) return FALSE; return server_name && html_ctx->GetDomain()!=server_name; };
};

/// Rule that verifies if the URL is not called in its domain or in one of its parent domain (it checks all of them)
class RuleNotInDomainDeep: public RuleNotInDomain
{
public:
	RuleNotInDomainDeep(ServerName *domain): RuleNotInDomain(domain) { }
	RuleNotInDomainDeep(URL &url): RuleNotInDomain(url) { }
	
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx);
};

/// Rule that verifies if the URL is not called in its domain or in one of its parent domain (it checks all of them)
class RuleThirdParty: public FilterRule
{
private:
	/// Return the deepest ancestor for a ServerName
	static ServerName *GetAncestorDomain(ServerName *domain);

public:
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx);
};

/// Rule that verifies if the URL is called in its domain or in one of its parent domain (it checks all of them)
class RuleNotThirdParty: public RuleThirdParty
{
public:
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx) { return !RuleThirdParty::MatchRule(url, html_ctx); }
};

/// Rule that verifies if the URL is called form a specific type of HTML tag
class RuleResource: public FilterRule
{
private:
	/// Elements that will be blocked
	HTMLResource elem;

public:
	RuleResource(HTMLResource elem) { this->elem = elem; }

	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx)
	{ if(!html_ctx) return FALSE; return html_ctx->GetResource() == elem; }
};

/// Base class for a rule that groups multiple subrules, to simplify management.
class RuleGroup: public FilterRule
{
protected:
	/// Rules hosted by the group
	OpAutoVector<FilterRule> rules;
	/// When empty_ctx_means_false parameter is TRUE, if there is no context, the group is completely bypassed, and FALSE is returned.
	/// This is an optimization (as the rules that we add to groups at the moment can't match without context), 
	/// but also basically a requirements for the NOR group
	BOOL empty_ctx_means_false;

public:
	RuleGroup(BOOL empty_ctx_means_false = TRUE)  { this->empty_ctx_means_false=empty_ctx_means_false; }
	// Add a rule to the group
	OP_STATUS AddRule(FilterRule *rule) { return rules.Add(rule); }
	/** Remove a rule from the group, deleting it if required. */
	OP_STATUS		RemoveRule(FilterRule *rule, BOOL delete_rule);
	/** Remove and delete all the rules; */
	void		DeleteAllRules() { rules.DeleteAll(); }
};


/// Rule that groups multiple subrules with an AND logic, to simplify management.
/// All the subrules need to match for this rule to suceed, or, put in another way, if a subrule fails, this rule fails as well
class RuleGroupAND: public RuleGroup
{
public:
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx);
};

/// Rule that groups multiple subrules with an OR logic, to simplify management.
// If at least one single subrule matches then this rule will suceed
class RuleGroupOR: public RuleGroup
{
public:
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx);
};

/// Rule that groups multiple subrules with an NOR logic, to simplify management.
/// It is the opposite of RuleGroupOR
class RuleGroupNOR: public RuleGroupOR
{
public:
	virtual BOOL MatchRule(URL *url, HTMLLoadContext *html_ctx);
};

/**
	This class represent a single URL pattern, that can be used to check if a "given URL" match it.<br />
	We will have a list of URL patterns, and they will be checked against the given URL.<br />
	For each URL pattern, the match is done in two steps:
	- an hash comparison is performed
	- if there is a possible match, then a true comparison is done, else there is no match
*/
class FilterURLnode
{
public:
	/** Constructor */
	FilterURLnode(BOOL is_excluded=FALSE);
	/** Operator = */
	FilterURLnode operator = (FilterURLnode& p);
    /** Copy constructor */
	FilterURLnode (FilterURLnode& p);

	/** Retrieve the URL. WARNING: don't change it, because the hash will not be computed! */
	const OpString*	GetURL()		{ return &m_url; };
	/** Set the URL pattern */
	OP_STATUS	SetURL(const uni_char* url, BOOL remove_trailing_spaces=TRUE);

	/** True if the pattern is excluded from the loading */
	BOOL		Excluded()		{ return excluded; };
	/** Set if the pattern has to be excluded from the loading */
	void		SetIsExclude(BOOL b)			{ excluded = b; };

	/** Add a new rule to the node, which will then be responsible to delete it */
	OP_STATUS		AddRule(FilterRule *rule);
	/** Remove a rule, deleting it if required. This will possibly trigger a reorganization of the rules */
	OP_STATUS		RemoveRule(FilterRule *rule, BOOL delete_rule);
	/** Remove and delete all the rules; */
	void		DeleteAllRules();

	///////////// BEGIN Functions called by DOM to add specific rules

	/** Add a group with all the "domains" specified.
	    This method is intended to be called at most once per node, as subsequent calls on the same node would create
		a new group, with unintended side effects
	*/
	OP_STATUS AddRuleInDomains(OpVector<uni_char> &domains);
	/** Add a group with all the "not domains" specified.
	    This method is intended to be called at most once per node, as subsequent calls on the same node would create
		a new group, with unintended side effects
	*/
	OP_STATUS AddRuleNotInDomains(OpVector<uni_char> &not_domains);
	/** Add a "Thrid Party" rule */
	OP_STATUS AddRuleThirdParty();
	/** Add a "Not Thrid Party" rule */
	OP_STATUS AddRuleNotThirdParty();
	/** Add a rule matching all the specified resources (as bit fields).
	    This method is intended to be called at most once per node, as subsequent calls on the same node would create
		a new group, with unintended side effects
	*/
	OP_STATUS AddRuleResources(UINT32 resources);

	///////////// END Functions called by DOM to add specific rules

#ifdef SYNC_CONTENT_FILTERS
	OP_STATUS	SetGUID(const uni_char* guid)
	{
		// strip the "UUID:" part from the ID, we'll add it back when writing
		if(guid && !uni_strncmp(guid, UNI_L("UUID:"), 5))
		{
			return m_guid.Set(&guid[5]);
		}
		return m_guid.Set(guid);
	}
	uni_char*	GUID() { return m_guid.CStr(); }
#endif // SYNC_CONTENT_FILTERS

#ifdef SELFTEST
	/// Checks if the 3 indexes supplied match with the ones used internally
	BOOL VerifyIndexes(unsigned short idx1, unsigned short idx2, unsigned short idx3);
	/// Checks if the 3 indexes computed used the first 2, 4 or 6 characters (depending on its length) of the string supplied match with the ones used internally
	BOOL VerifyIndexes(const uni_char *str);
#endif

protected:
	friend class FilterURLList;
	friend class URLFilter;

	// exclude this pattern from loading
	BOOL excluded;

	OpString		m_url;								///< string representation of the url

	// Variables used for the hash. We need to identify a single bit in match_array, so we need an byte index (e.g. index1 ==> 37 )
	// and a bit mask (e.g. bit1 ==> 4th bit ==> 2^3 ==> 8).
	// This version uses 3 bits. See also ComputeIndex()
	unsigned short index1;								///< First byte index
	unsigned short index2;								///< Second byte index
	unsigned short index3;								///< Third byte index
	unsigned char bit1;									///< First bit mask
	unsigned char bit2;									///< Second bit mask
	unsigned char bit3;									///< Third bit mask

	OpAutoVector<FilterRule> m_rules;					///< List of rules to check before accepting a match

	#ifdef FILTER_BYPASS_RULES
    	BOOL            m_bypass;                           ///< flag TRUE=exclude or FALSE=include; seems to be useless
  	#endif // FILTER_BYPASS_RULES
	#ifdef FILTER_REDIRECT_RULES
    	BOOL            m_redirect;                         ///< flag TRUE=exclude or FALSE=include; seems to be useless
	#endif // FILTER_REDIRECT_RULES

#ifdef SYNC_CONTENT_FILTERS
	OpString m_guid;									// The unique identifier for this item. Only used when synchronizing rules
#endif // SYNC_CONTENT_FILTERS

	/** Compute the indexes used for the hash match.  */
	void		ComputeMatchIndexes();
	#ifdef FILTER_BYPASS_RULES
		/** Set the bypass rule */
    	void        SetIsBypass(BOOL p)             { m_bypass = p; };
	#endif // FILTER_BYPASS_RULES
	#ifdef FILTER_REDIRECT_RULES
		/** Set the redirection rule */
    	void        SetIsRedirect(BOOL p)               { m_redirect = p; };
 	#endif // FILTER_REDIRECT_RULES

	//BOOL IsHashMeaningful() { return index1 && index2 && index3; }

	static inline BOOL IsCharacterWildCard(uni_char c) { return c=='*' || c=='^'; }

	// Compute the index used for the hash match, for the current character
	static inline unsigned short int ComputeIndex(const uni_char *p)
	{
		// The index now is a 14 bits integer, that try to maximize the effect of the hash, minimizing the
		// memory consumption and the time spent for building the match_array.
		// The index is composed by the first 2 characters of the string, considered as 7-bits character, and compressed
		// in a 14 bits index.
		// The index will be checked against match_arraym to see if the given URL has the same pair of characters, which
		// is a requirement for matching, but of course it is not enough.
		// The complete algorithm check for 3 indexes, to have a more meaningfull match

		//return (((unsigned short int)p[0])<<8)+((unsigned char)p[1]);  /*8bit version*/
		return ( ((unsigned short int)(p[0]&0x7f))<<7)+ ((unsigned char)(p[1]&0x7f)); /* 7bit version */
	}

	/** @return TRUE if all the rules allows the match; no rules also means that the match is valid.  */
	BOOL CheckNodeRules(class URL *url, HTMLLoadContext *html_ctx);

	/** Add to a group all the "domains" specified */
	OP_STATUS AddRuleInDomainsKernel(OpVector<uni_char> &domains, RuleGroup *group, BOOL deep);

    /** Create the specified RuleResource and add it to the group */
	OP_STATUS AddRuleResourceToGroup(HTMLResource resource, RuleGroup *group);
};

/** List of URL pattern, used to check if a given URL matches one of the patterns.
	This class must be used only by URLFilter, so every method or attribute is private.
*/
class FilterURLList
{
	friend class URLFilter;
	friend class ExtensionList;
private:
	OpAutoVector<FilterURLnode> m_list;		///< List of the patterns
	BOOL m_loaded;							///<TRUE if a matching array can be loaded, FALSE if it has to be blocked

#if defined(_DEBUG) || defined(SELFTEST)
	// Statistical variables
	unsigned long m_hash_matches_true;		///< Number of possible and meaningfull hash matches
	unsigned long m_hash_matches;			///< Number of possible hash matches
	unsigned long m_matches;				///< Number of positive matches
	unsigned long m_totalcalls;				///< Total calls of Find() method, even when the hash match fails
	unsigned long m_totalcompares;			///< Total number of pattern-URL compares
#endif // SELFTEST

	/**
		Look foor the given URL
		@param match_array Match Array, used to speed-up the search
		@param url URL that must be checked against the patterns
		@param url_str String prepresentation of the url that must be checked against the patterns
		@param html_ctx Context of the HTML call, used for filters that require more information
		@return TRUE if the URL match one of the patterns
	*/
	BOOL	Find(unsigned char *match_array, URL *url, OpString* url_str, HTMLLoadContext *html_ctx);
	/**
		Look foor the given URL, using the requested algorithm

		@param match_array Match Array, used to speed-up the search
		@param url URL that must be checked against the patterns
		@param url_str String prepresentation of the url that must be checked against the patterns
		@param html_ctx Context of the HTML call, used for filters that require more information
		@return TRUE if the URL match one of the patterns
	*/
	inline BOOL	FindAlg(unsigned char *match_array, URL *url, OpString* url_str, FilterAlgorithm alg, HTMLLoadContext *html_ctx);
	
	/** Version that look for a "perfect match", so that the URL is the same as one of the patterns */
	int	FindExact(const OpString *url);
#ifdef SYNC_CONTENT_FILTERS
	/** Find a FilterURLnode based on the given GUID. */
	FilterURLnode*	FindNodeByID(const uni_char* guid);
	BOOL			DeleteNodeByID(const uni_char* guid);
#endif // SYNC_CONTENT_FILTERS

	/** Default constructor */
	FilterURLList(BOOL loaded);
	/** Destructor */
	virtual ~FilterURLList();
	/** @return Return the number of nodes in the list */
	UINT32	GetCount() { return m_list.GetCount(); };
	/** Add a new node in the list. If successful, the class will take care of deleting the node.
	 *
	 * MEMORY MANAGEMENT:
			- If the function returns an error, the caller is responsible to delete the node
			- If the function returns OK:
				- if node_acquired is NULL (automatic memory management), this object will manage the life cycle of the node, in particular, it could be deleted immediately.
				- if node_acquired is != NULL (manual memory management):
					- *node_acquired will be FALSE if the caller must free the node itself.
					- *node_acquired will be TRUE if this object will manage the life cycle of the node.
					- one key difference, is that this method will not delete the node immediately if node_acquired != NULL
	 *  */
	OP_STATUS		InsertURL(FilterURLnode* node, BOOL allow_duplicates=FALSE, BOOL *node_acquired=NULL, BOOL *node_duplicated=NULL);
	/** Add a pattern to the list, effectively calling InsertURL() */
	OP_STATUS InsertURLString(const uni_char *pattern, BOOL allow_duplicates=FALSE, BOOL add_star_at_end=FALSE);
	/** Remove and delete the URL */
	BOOL			DeleteURL(FilterURLnode* url);
	BOOL			DeleteURL(const OpString& url);
	FilterURLnode*	Get(UINT32 i) { return m_list.Get(i); }

#ifdef CF_WRITE_URLFILTER
	/** Save all the nodes in a preference file. */
	OP_STATUS		SaveNodes(URLFilter *filter);
#endif // CF_WRITE_URLFILTER
	/** Save all the nodes in a list; if there is an error, the caller is responsible for deallocating the nodes already added. */
	OP_STATUS		SaveNodesToList(OpVector<OpString>& list);
#if defined(SUPPORT_VISUAL_ADBLOCK) && defined(QUICK)
	/** Save the list in a tree. It is better to not use this method... */
	OP_STATUS		SaveNodesToTree(SimpleTreeModel *model);
#endif // SUPPORT_VISUAL_ADBLOCK and QUICK

#if defined(_DEBUG) || defined(SELFTEST)
	// Get the statistical variables
	unsigned long	GetTrueHashMatches() { return m_hash_matches_true; }
	unsigned long	GetHashMatches() { return m_hash_matches; }
	unsigned long	GetMatches() { return m_matches; }
	unsigned long	GetTotalCalls() { return m_totalcalls; }
	unsigned long	GetTotalCompares() { return m_totalcompares; }
#endif // SELFTEST

	/** Slow version of the Find(); */
	OP_STATUS	FindSlow(URL *url, OpString* url_str, BOOL& findresult, BOOL &found, HTMLLoadContext *html_ctx);
};

// Class containing everything needed by an extensions
class ExtensionList
{
private:
	friend class URLFilter;
	FilterURLList	m_exclude_rules;				///< List of the exclusion URL patterns
	FilterURLList	m_include_rules;				///< List of the inclusion URL patterns
	OpGadget *extension_ptr;						///< Pointer used to identify the extension
	OpVector<URLFilterExtensionListener> listeners;			///< Listeners list

public:
	/// Type of event to send
	enum EventType
	{
		/// The URL has been blocked
		BLOCKED,
		/// The URL would have been blocked, but it is white listed
		UNBLOCKED,
	#ifdef SELFTEST
		/// The URL is allowed (this event is for testing only)
		ALLOWED
	#endif //SELFTEST
	};

	ExtensionList(OpGadget * extension, URLFilterExtensionListener *lsn): m_exclude_rules(FALSE), m_include_rules(TRUE), extension_ptr(extension) { OP_ASSERT(extension_ptr); }

	/// Set the listener watching the extension
	OP_STATUS AddListener(URLFilterExtensionListener *lsn);
	/// Remove the lsitener watching the extension
	OP_STATUS RemoveListener(URLFilterExtensionListener *lsn);
	/// Send the event to all the listener
	void SendEvent(EventType svt, const uni_char* url_str, OpWindowCommander *wc, HTMLLoadContext *ctx);

	virtual ~ExtensionList() { }
};

/** Class that exposes the Content Filter feature. This is the main class from a caller perspective, and is used to see if a given
    URL must be loaded or not, checking it against two list of pattern (one for inclusion, one for exclusion).

	Hash-like comparisons are performed to speed-up the process, because checking an URL against a pattern is relatively expensive.
	What we do is to check if some pairs of letters of the pattern are present in the given URL. If they are not present, no match is possible,
	if they are present, a complete check is to be performed.

	The idea is to have an array that is able to say if a given pair of letters (e.g.: "ab", meaning "a' followed by "b").
	A bit for each pair is enough, so we need a 64kb array ==> 8KB. The allocation of the array is quite slow, so I tried a
	"7 bits character" version, because usually the higher bit is 0. So for a pair we need 14 bits ==> 16Kb ==> 2KB.
	For every pair in the URL, the according bit will be set, so we can easily know if a pair is present.
	Please remember that this is only an hash. If "ab" is present and "bc" is present, we don't know anything about where are
	this pairs, so checking for "ab" and "bc" returns true for "abc", "abadbc", "bcaaabbb" and so on... so after the hash you need a
	full check.

	The actual hash check the first 6 letters (if available), as 3 pairs.
	So, for example, in "operaunite*.com" it checks for "op"+"er"+"au".

*/
class URLFilter
#ifdef SYNC_CONTENT_FILTERS
	: public OpSyncDataClient
#endif // SYNC_CONTENT_FILTERS
{
	friend class ContentFilterModule;
	friend class FilterURLList;
	friend class ExtensionList;
	unsigned char *match_array;

	// 16Kb ==> 2KB array for matching; I check "7 bits" characters (7+7=14 bit; 2^14=16K), to speed-up op_memset
	#define MATCH_ARRAY_SIZE 2048

public:

	/** Block mode of the filter */
	enum URLFilterBlockMode
	{
		BlockModeNormal = 0,		// normal blocking
		BlockModeOff = 1,			///< Don't block anything, always load  (every URL is accepted)
		BlockModeRestrictive = 2	///< always block (apply only if the include or the exclude list is empty)
	};

private:
	PrefsFile*		m_filterfile;					///< this is the prefsfile that contain the list of exclude and include urls
	FilterURLList	m_exclude_rules;				///< List of the exclusion URL patterns
	FilterURLList	m_include_rules;				///< List of the inclusion URL patterns
	OpAutoVector<ExtensionList> m_extensions;			///< Temporary lists used for the extensions
													///< They are not saved or synchronized, just checked when available
	FilterURLList	m_unstoppable_rules;			///< WhiteList of the patterns that cannot be blocked (this include by default things like browser.js and such)
#ifdef CF_BLOCK_OPERA_DEBUG
	FilterURLList	m_soft_disabled_rules;			///< List of patterns that are disabled but can be reenabled via urlfilter.ini
#endif
	BOOL			m_exclude_include;				///< Precedence, TRUE : exclude has priority; FALSE : vice versa
	URLFilterBlockMode	m_block_mode;				///< Block mode, for changing the behavior of the filter
	URLFilterListener   *m_listener;				///< Listener that can be notified of the URL blocked
	AsyncURLFilterListener *m_async_listener;       ///< Listener that is called to check if URL in external filter
	FilterAlgorithm alg;							///< Algorithm used for matching

#ifdef FILTER_BYPASS_RULES
	FilterURLList	m_bypass_rules;					///< List of the Bypass URLs
#endif // FILTER_BYPASS_RULES
#ifdef FILTER_REDIRECT_RULES
     FilterURLList  m_redirect_rules;				///< List of the Redirect URLs
#endif // FILTER_REDIRECT_RULES
#ifdef WEB_TURBO_MODE
	 OpString currBypassURLs;						///< Whitespace separated list of servers and/or domains provided to SetWebTurboBypassURLsL()
#endif // WEB_TURBO_MODE
	
	List<CheckURLAllowedContext> m_pending_url_allowed_ctx;	///< List of pending URL allowed context

	/** Initialization; it loads the rules */
	OP_STATUS		InitL();
	/** Initialize the match array, to have the bits specifying the presence of each pair */
	OP_STATUS CreateMatchArray(unsigned char *&match, OpString* url, BOOL clean_array);
	/** Inizialize the hard coded lists (m_unstoppable_rules and m_soft_disabled_rules)*/
	OP_STATUS InitializeHardCodedLists();
	/** @return return TRUE if the URL passes the filtering, FALSE if the URL should be ignored; it is better to
		initialize load to the "default value" because sometimes its value is not forced...
		This is the method that actually implements the check
	 */
	OP_STATUS CheckURLKernel(URL* url, const uni_char* url_str, BOOL& load, Window *window, HTMLLoadContext *html_ctx, BOOL fire_events);
	// Check one list of rules (it can be used both for the main rules and the extensions)
	static inline BOOL CheckRules(FilterURLList &list, unsigned char *match_array, URL *url, OpString *value, FilterAlgorithm chosen_alg, BOOL &load, HTMLLoadContext *html_ctx);
	/** Find the list associated with the extension;
		@param extension Extension to look for
		@param create if TRUE, in case that the list has not been found, it will be created
		@return the extension list, NULL if it is not present; if create is TRUE, NULL means OOM
	*/
	ExtensionList *FindList(OpGadget *extension, int &pos, BOOL create = FALSE);
	/// Returns the best alorithm to use in this case
	FilterAlgorithm ChooseBestAlgorithm() { if (alg != FILTER_ADAPTIVE) return alg; return (GetCount(TRUE)<=35) ? FILTER_SLOW : FILTER_FAST; }

#ifdef CF_WRITE_URLFILTER
	// Save a single node SaveSingleNode(); an alternative method can ve exposed
	OP_STATUS SaveSingleNode(FilterURLnode *node);
#endif // CF_WRITE_URLFILTER

	#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
		// Add ":80" to an URL filter, if needed. This function is public only for test purposes
		static OP_STATUS AddPortToUrlFilter(OpString *url);
	#endif//CF_BLOCK_INCLUDING_PORT_NUMBER


	/** Accessor for the configuration file */
	class URLFilterAccessor : public IniAccessor
	{
	public:
		URLFilterAccessor();

		/**
		* Load the contents in the file.
		*
		* @param file The file to load stuff from
		* @param map The map to store stuff in
		* @return Status of the operation
		*/
		virtual OP_STATUS LoadL(OpFileDescriptor *file, PrefsMap *map);

		/**
		* Store the contents in the map to a file.
		*
		* @param file The file to store stuff in
		* @param map The map to get stuff from
		* @return Status of the operation
		*/
		virtual OP_STATUS StoreL(OpFileDescriptor *file, const PrefsMap *map);

		inline BOOL ParseLineL(uni_char *key_p, PrefsMap *map);

	private:

		BOOL m_escape_section;
	};

	/// Returns true if the character is a separator
	static inline BOOL IsSeparator(uni_char c);

public:

	/** Default constructor */
	URLFilter();
	/** Destructor */
	~URLFilter();
	//methods
#ifdef FILTER_BYPASS_RULES
	/** return TRUE if the URL should be bypassed by the proxy */
	OP_STATUS CheckBypassURL(const uni_char* url, BOOL& load);
	/** Add a bypass URL */
	OP_STATUS AddFilterL(const uni_char* url);
	/** Remove a bypass URL */
	OP_STATUS RemoveFilter(const uni_char* url);
#endif // FILTER_BYPASS_RULES
#ifdef FILTER_REDIRECT_RULES
	/** return TRUE if the URL should be redirected */
	OP_STATUS CheckRedirectURL(const uni_char* url, BOOL& load);
	/** Add a redirection URL */
	OP_STATUS AddRedirectFilterL(const uni_char* url);
#endif // FILTER_REDIRECT_RULES
	/// Set the listeners
	void SetListener(URLFilterListener *listener){ m_listener = listener; };
	/// Remove the listener, only if it matches
	void RemoveListener( URLFilterListener *listener ) { if(m_listener==listener) m_listener=NULL; }
	/// Remove the listener, always
	void RemoveListener() { m_listener=NULL; }
	/// Return the current lsitener
	URLFilterListener *GetListener() { return m_listener; }
	/// Set the listener for the asynchronous URL filter
	void SetAsyncListener(AsyncURLFilterListener *listener){ m_async_listener = listener; };
	/// Get the listener for the asynchronous URL filter
	AsyncURLFilterListener* GetAsyncListener(){ return  m_async_listener; };

	/** 
		Checks if a URL is allowed or if Opera should block it.
		@param url String representation of the URL to check
		@param load will contain TRUE if the url is allowed. It must be initialized, as
			   the original value is used if case of no match
		@param window optional Window pointer sent to the listeners
		@param html_ctx optional context used to enable finer rules (like third_part) required by ADBlock
		@param fire_events TRUE if the events can be fired
		@return typically in case of error it will return ERR_NO_MEMORY (OOM), ERR_NULL_POINTER (null pointer supplied) or
			    ERR_OUT_OF_RANGE (bad parameter).
		NOTE: This function should be avoided, as internally a URL object will be constructed
	 */
	OP_STATUS CheckURL(const uni_char* url, BOOL& load, Window *window = NULL, HTMLLoadContext *html_ctx = NULL, BOOL fire_events = TRUE);
	/**Checks if a URL is allowed or if Opera should block it.
		@param url URL to check
		@param load will contain TRUE if the URL is allowed. It must be initialized, as
			   the original value is used if case of no match
		@param window optional Window pointer sent to the listeners
		@param html_ctx optional context used to enable finer rules (like third_part) required by ADBlock
		@param fire_events TRUE if the events can be fired
		@return typically in case of error it will return ERR_NO_MEMORY (OOM), ERR_NULL_POINTER (null pointer supplied) or
			    ERR_OUT_OF_RANGE (bad parameter).
		*/
	inline OP_STATUS CheckURL(URL* url, BOOL& load, Window *window = NULL, HTMLLoadContext *html_ctx = NULL, BOOL fire_events = TRUE);

	/**
	 * Asynchronous filter function for checking if a URL is allowed to
	 * be loaded.
	 * @param url   The URL to check
	 * @param msg   The message to be posted when the asyncronous response
	 *              is received.
	 * @param wait  Flag set if an asynchronous call has been made and the
	 *              loading should be halted in wait for the response. If
	 *              false, loading should continue un-interrupted.
	 * @return OP status value.
	 */
	OP_STATUS CheckURLAsync(URL url, OpMessage msg, BOOL& wait);
	/**
	 * Cancel an ongoing asynchronous filter check.
	 * @param url   The URL to cancel
	 * @return OP status value.
	 */
	OP_STATUS CancelCheckURLAsync(URL url);
	/** Add a pattern to the exclude or include list, based on the node Excluded() value
		@param url Node to add
		@param extension Pointer used to identify the extension (NULL means that it is not part of any extensions)
		@param node_acquired set to TRUE if Opera took ownership of the node, which could mean that it has been
			   added to its lists, but also deleted (so it cannot be assumed that it is still working);
			   if it is set to FALSE, the caller must delete the node.
	   MEMORY MANAGEMENT:
			- If the function returns an error, the caller is responsible to delete the node
			- If the function returns OK:
				- if node_acquired is NULL (automatic memory management), this object will manage the life cycle of the node, in particular, it could be deleted immediately.
				- if node_acquired is != NULL (manual memory management):
					- *node_acquired will be FALSE if the caller must free the node itself.
					- *node_acquired will be TRUE if this object will manage the life cycle of the node.
					- one key difference, is that this method will not delete the node immediately if node_acquired != NULL

		*** This method is obviously too complicated to use. AddURLString() is now the preferred way to add URL patterns
	*/
	OP_STATUS	AddURL(FilterURLnode* url, OpGadget *extension, BOOL *node_acquired);
	/** Add a pattern to the exclude or include list
		@param url URL pattern to add
		@param exclude TRUE to add to the exclusion list, FALSE to add to the inclusion list
		@param extension Pointer used to identify the extension (NULL means that it is not part of any extensions)
	*/
	OP_STATUS	AddURLString(const uni_char *url, BOOL exclude, OpGadget *extension);
	/// Set the listener watching an extension; this could actually create the ExtensionList element;
	OP_STATUS   AddExtensionListener(OpGadget *extension, URLFilterExtensionListener *listener);
	/// Remove the listener
	OP_STATUS	RemoveExtensionListener(OpGadget *extension, URLFilterExtensionListener *listener);
	/** Set the blocking mode */
	void		SetBlockMode(URLFilterBlockMode block_mode) { m_block_mode = block_mode; }
	/** Initialization, used for selftests */
	OP_STATUS	InitL(OpString& filter_file);
	/** Delete a pattern from the exclusion or inclusion list list */
	BOOL		DeleteURL(FilterURLnode* url, BOOL exclusion=TRUE, OpGadget *extension=NULL);
	/** Remove all the patterns associated to the given extension; return TRUE if the extension has been found and removed */
	BOOL RemoveExtension(OpGadget *extension);
	/** Remove all the patterns associated to extensions */
	BOOL RemoveAllExtensions();
	/// TRUE if the filter contains at least one exclude rule
	BOOL		HasExcludeRules() { return m_exclude_rules.GetCount() || m_extensions.GetCount(); /* Extensions without rules are removed from this list */ }
	/** Set if the exclusion has priority over inclusion (default), or vice versa*/
	void		SetExclusion(BOOL b) { m_exclude_include=b; }
	/** @return Return the number of nodes in the lists (optionally including extensions) */
	UINT32	GetCount(BOOL count_extension_filters);
	/** @return Return the number of extension lists */
	UINT32	GetCountExtensionLists() { return m_extensions.GetCount(); }
	/** Set the algorithm required for best performances */
	void SetAlgorithm(FilterAlgorithm algorithm) { alg=algorithm; }

#ifdef SELFTEST
	/** Append the statistical variable values to a string; used for debugging */
	void		AppendCountersDescription(OpString8 &str);
	/** Slow version of CheckURL() used for debugging purposes. Does notsupport extensions.
		return TRUE if the URL passes the filtering, FALSE if the URL should be ignored
	*/
	OP_STATUS	CheckURLSlow(const uni_char* url, BOOL& load, Window *window = NULL, HTMLLoadContext *html_ctx = NULL, BOOL fire_events = TRUE);
#endif
#ifdef CF_WRITE_URLFILTER
	// Write the list in a preference file
	OP_STATUS	WriteL();
#endif // CF_WRITE_URLFILTER
	
	// Create the List for the exclusion list; if there is an error, the caller is responsible for deallocating the nodes already added
	OP_STATUS		CreateExcludeList(OpVector<OpString>& list);
	
	// Try to match a path with the given pattern
	static		BOOL MatchUrlPattern(const uni_char *path, const uni_char *pattern);
	
	#ifdef CF_BLOCK_INCLUDING_PORT_NUMBER
		// Compares two filters without considering the HTTP / HTTPS ports (return TRUE if they are equals). Ex: http://opera.com/ == http://opera.com:80/
		// This function is supposed to be used only for testing
		static BOOL PortCompareFilters(char *url1, char *url2);
		// Check if a URL is not changed when ports are not considered
		// This function is supposed to be used only for testing
		static BOOL PortCompareFiltersUnchanged(char *url);
	#endif

#ifdef WEB_TURBO_MODE
	// Sets bypass filters from a whitespace separated list of servers and/or domains and stores
	// the contents of newStr for later comparisons
	void SetWebTurboBypassURLsL(const OpStringC &newStr);
	// Creates wildcard URL string on the form "http://*.example.com/*" provided the urlStr "*.example.com"
	static void GetWildcardURLL(const OpStringC &urlStr, OpString &wildcardStr);
#endif //WEB_TURBO_MODE

#ifdef SYNC_CONTENT_FILTERS
	/** Find a FilterURLnode based on the given GUID. Will search both the exclude and the include list. */
	FilterURLnode*	FindNodeByID(const uni_char* guid);

	/** Delete a FilterURLnode based on the given GUID. Will search both the exclude and the include list. */
	inline BOOL DeleteNodeByID(const uni_char* guid);
#endif // SYNC_CONTENT_FILTERS

	// Implementing the OpSyncDataListener interface - the sync module uses this to send notifications about data changes. Note that setting up URLFilter as listener
	// is done in the platform's sync manager.
#ifdef SYNC_CONTENT_FILTERS
	virtual OP_STATUS SyncDataInitialize(OpSyncDataItem::DataItemType type);
	virtual OP_STATUS SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error);
	virtual OP_STATUS SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty);
	virtual OP_STATUS SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support);
#endif // SYNC_CONTENT_FILTERS

protected:
#ifdef SYNC_CONTENT_FILTERS

	/** Sync this url item.  If sync is disabled for this data type, it will do nothing.  */
	virtual OP_STATUS SyncItem(FilterURLnode* item, OpSyncDataItem::DataItemStatus sync_status, BOOL first_sync = FALSE, BOOL is_dirty = FALSE);

	/** Convert a url filter item to a OpSyncItem suitable for use with sync  */
	virtual OP_STATUS URLFilterItem_to_OpSyncItem(FilterURLnode* item, OpSyncItem*& sync_item, OpSyncDataItem::DataItemStatus sync_status);

	/** Convert a OpSyncItem item to a suitable item for use with url filter  */
	virtual OP_STATUS OpSyncItem_to_URLFilterItem(OpSyncItem* sync_item, FilterURLnode*& item, BOOL enable_update);

	/** Support method to process incoming items */
	OP_STATUS ProcessSyncItem(OpSyncItem* item);

	/** Support method to process added or changed incoming items
		@param item Item to sync
		@param enable_update TRUE if, in case of duplication, the node needs to be update with the new values, FALSE to keep the previous ones
	*/
	OP_STATUS ProcessAddedSyncItem(OpSyncItem* item, BOOL enable_update);

	/** Support method to process deleted incoming items */
	OP_STATUS ProcessDeletedSyncItem(OpSyncItem* item);

	/** Backup the urlfilter.ini file before enabling sync - just in case */
	OP_STATUS BackupFile(OpFile& file);

	/** Generate a GUID for the url filter entry, if needed */
	OP_STATUS GenerateGUIDForFilter(FilterURLnode* item);
#endif // SYNC_CONTENT_FILTERS

private:
#ifdef SYNC_CONTENT_FILTERS
	BOOL m_sync_enabled;					///< Internal flag to set when history syncing is enabled - it's set when the sync module tells us our data type needs to be synchronized
#endif // SYNC_CONTENT_FILTERS
};

/** Compares two Unicode strings
@param src Source string
@param dst Destination string
@return 0 if the strings are equals, -1 if src is smaller, +1 if src is bigger
*/
inline int cf_strcmp (const uni_char * src, const uni_char * dst)
{
	int ret = 0;

	// assumes uni_char is unsigned on the platform (although it makes little difference in practice)
	while((ret = *src - *dst)==0 && *dst)
		++src, ++dst;

	if(ret < 0)
		return  -1;
	if(ret > 0)
		return 1;

	return 0;
}

inline BOOL URLFilter::IsSeparator(uni_char c)
{
	// ^ wildcard matches separator characters.
	// A separator character is anything but a letter, a digit, or one of
	// the following: _ - . %; plus the end of line.

	if( (c>='0' && c<='9') ||
		c=='_' || c=='-' || c == '.' || c == '%' ||
		op_isalpha(c))
		return FALSE;

	return TRUE;
}

inline OP_STATUS URLFilter::CheckURL(URL* url, BOOL& load, Window *window, HTMLLoadContext *html_ctx, BOOL fire_events)
{
	OP_ASSERT(url);

	if(!url)
		return OpStatus::ERR_NULL_POINTER;

	if (
#ifdef SUPPORT_VISUAL_ADBLOCK
		(window && !window->IsContentBlockerEnabled()) ||
#endif // SUPPORT_VISUAL_ADBLOCK
		(m_block_mode == BlockModeOff)
		)
	{
		load = TRUE;
		return OpStatus::OK;
	}

	OpString str;

	RETURN_IF_ERROR(url->GetURLDisplayName(str));
	const uni_char* url_str = str.CStr();

	OP_STATUS ops = CheckURLKernel(url, url_str, load, window, html_ctx, fire_events);

	OP_NEW_DBG("URLFilter::CheckURL(URL ...)", "content_filter.full");
	OP_DBG((UNI_L("URL: %s %s"), url_str, (load) ? UNI_L(" CAN load") : UNI_L(" CANNOT load")));

	return ops;
}

inline BOOL URLFilter::CheckRules(FilterURLList &list, unsigned char *match_array, URL *url, OpString *value, FilterAlgorithm chosen_alg, BOOL &load, HTMLLoadContext *html_ctx)
{
	if(list.m_list.GetCount() && list.FindAlg(match_array, url, value, chosen_alg, html_ctx))
	{
		load=list.m_loaded;

		return TRUE;
	}

	return FALSE;
}

#ifdef SYNC_CONTENT_FILTERS
inline BOOL URLFilter::DeleteNodeByID(const uni_char* guid)
{
	// try the exclude list first, the item could be in either
	if(!m_exclude_rules.DeleteNodeByID(guid))
	{
		return m_include_rules.DeleteNodeByID(guid);
	}
	return TRUE;
}
#endif // SYNC_CONTENT_FILTERS

inline BOOL FilterURLList::FindAlg(unsigned char *match_array, URL *url, OpString* url_str, FilterAlgorithm alg, HTMLLoadContext *html_ctx)
{
	if(alg==FILTER_SLOW)
	{
		BOOL findresult;
		BOOL found;

		OpStatus::Ignore(FindSlow(url, url_str, findresult, found, html_ctx));

		return found;
	}
	else
		return Find(match_array, url, url_str, html_ctx);
}

#endif // URL_FILTER
#endif //_CONTENT_FILTER_H_INCLUDED_
