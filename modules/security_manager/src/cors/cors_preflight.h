/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef CORS_PREFLIGHT_H
#define CORS_PREFLIGHT_H

class OpCrossOrigin_Request;

/** The preflight cache keeps information on prior cross-origin
    preflight responses. A non-simple cross-origin request is
    allowed to go ahead if a valid match is found in this cache
    (=> not requiring its own a preflight request to verify.)

    A cache entry contains a header value or method + an expiry
    time; all with respect to some target resource. The expiry time
    is set based on the max age that the resource/server supplies
    in preflight requests. */
class OpCrossOrigin_PreflightCache
{
public:
	static OP_STATUS Make(OpCrossOrigin_PreflightCache *&cache, unsigned default_max_age, unsigned max_max_age = SECMAN_MAXIMUM_PREFLIGHT_MAX_AGE, unsigned max_size = UINT_MAX);
	/**< Construct a preflight cache instance.

	     @param default_max_age The default max age value to use.
	     @param max_max_age The maximum age value to allow added
	            to the cache.
	     @param max_size The upper limit on cache entries.
	     @return OpStatus::OK on successful creation; OpStatus::ERR_NO_MEMORY
	             on OOM. */

	~OpCrossOrigin_PreflightCache();

	unsigned GetMaximumMaxAge() const { return maximum_max_age; }
    /**< Return upper bound on delta-seconds that a server might
	     provide for age. Responses having higher values will be
	     capped at this maximum. */

	unsigned GetDefaultMaxAge() const { return default_max_age; }
	/**< The default max-age value to use if response lacks one,
	     is ill-formed or invalid. */

	BOOL AllowUpdates(const OpCrossOrigin_Request &request);
	/**< Return TRUE if the preflight cache may add or update the
	     cache with information stemming from the preflight request
	     'request'.

	     Size and other resource constraints may cause the cache
	     to disable such updates. The implementation is free to
	     remove expired cache entries and make room for the
	     updates to 'request' during this call.

	     @param request The (origin, target) cross-origin request.
	     @return TRUE if cache update operations for this request
	             may go ahead. */

	OP_STATUS UpdateMethodCache(OpVector<OpString> &methods, const OpCrossOrigin_Request &request, unsigned max_age);
	/**< Update and extend the method cache with max age data for
	     'methods', stemming from the response of a preflight
	     request 'request'.

	     @param methods The set of methods that the resource allows
	            cross-origin access via.
	     @param request The CORS request the preflight is made on
	            behalf of.
	     @param max_age The effective max-age to give to the resulting
	            cache entry.
	     @return OpStatus::OK on successful update.
	             OpStatus::ERR_NO_MEMORY on OOM. */


	OP_STATUS UpdateHeaderCache(OpVector<OpString> &headers, const OpCrossOrigin_Request &request, unsigned max_age);
	/**< Update and extend the header cache with max age data for
	     'headers', stemming from the response to a preflight
	     request 'request'.

	     @param headers The set of non-simple request headers that
	            the resource allows cross-origin requests to use.
	     @param request The CORS request the preflight is made on
	            behalf of.
	     @param max_age The effective max-age to give to the resulting
	            cache entry.
	     @return OpStatus::OK on successful update.
	             OpStatus::ERR_NO_MEMORY on OOM. */

	BOOL MethodMatch(const OpCrossOrigin_Request &request);
	/**< Check if the request has a method cache match.

	     @param request The CORS request the preflight is made on
	            behalf of.
	     @return TRUE if the request method has a cache match. */

	BOOL HeaderMatch(const OpCrossOrigin_Request &request, const uni_char *header);
	/**< Check if the request has a cache match for a given header
	     name.

	     @param request The CORS request the preflight is made on
	            behalf of.
	     @param header Header name to check for cache match.
	     @return TRUE if the request method has a cache match. */

	void RemoveOriginRequests(const uni_char *origin, const URL &request_url);
	/**< Evict entries that match the given (origin, request_url) pair.
	     Must be done if the cross-origin request fails (from a given origin.)

	     @param origin The origin to purge cache entries with respect to.
	     @param request_url The cross-origin resource to evict.
	     @return TRUE if the request method has a cache match. */

	void InvalidateCache(BOOL expire_all = FALSE);
	/**< Invalidate expired cache entries. If 'expire_all' is
	     TRUE, invalidate unconditionally of expiration time. */

private:
	OpCrossOrigin_PreflightCache(unsigned default_age, unsigned max_age, unsigned max_size)
		: cache_entries_count(0)
		, default_max_age(default_age)
		, maximum_max_age(max_age)
		, max_size(max_size)
	{
	}

	/** The cache entry kept. The organisation of this cache is
	    simple; a time-ordered list of cache entries.

	    The underlying assumption is that there will not be a great
	    number of preflight requests in this cache (most cross-origin
	    access attempts are allowed on the basis of being simple.)

	    [If that assumption is invalid, then we improve.] */
	class CacheEntry
		: public ListElement<CacheEntry>
	{
	public:
		static OP_STATUS Make(CacheEntry *&entry, unsigned max_age, const uni_char *origin, const URL &url, BOOL credentials, BOOL is_method, const uni_char *value);
		/**< Create a new cache entry with the given delta-seconds "maximum age"
		     from current time. The cache entry keeps copies of the required
		     arguments (and control their lifetime), hence caller assumes
		     ownership of the arguments passed. */

		~CacheEntry()
		{
		}

		void UpdateMaxAge(unsigned max_age);
		/**< Update cache entry with new maximum age. */

	private:
		friend class OpCrossOrigin_PreflightCache;

		double expiry_time;
		/**< The expiry time (milliseconds since Epoch) of this cache entry.
		     The entry is invalid past this time (and will be removed.)

		     The implementation is overall in control of the lifetime
		     of cache entries and may remove at some earlier time. */

		OpString origin;
		/**< The source origin. */

		URL url;
		/**< The request URL. */

		OpString url_string;
		/**< The stringified request URL. */

		unsigned max_age;
		/**< Max age seconds provided in the preflight response. */

		BOOL credentials;
		/**< TRUE if this is a credentialled request. */

		BOOL is_method;
		/**< TRUE if this is method cache entry; FALSE if a header (name)
		     cache entry. */

		OpString value;
		/**< The method or header name recorded by this entry. */

		CacheEntry(double expiry, const URL &url, unsigned max_age, BOOL credentials, BOOL is_method)
			: expiry_time(expiry)
			, url(url)
			, max_age(max_age)
			, credentials(credentials)
			, is_method(is_method)
		{
		}
	};

	CacheEntry *FindMethodMatch(const OpCrossOrigin_Request &request, const uni_char *method);
	/**< Returns the matching cache method entry, if one. */

	CacheEntry *FindHeaderMatch(const OpCrossOrigin_Request &request, const uni_char *header);
	/**< Returns the matching cache header entry, if one. */

	List<CacheEntry> cache_entries;

	unsigned cache_entries_count;
	/**< The number of entries in the cache. */

	unsigned default_max_age;
	/**< If the max age in a preflight response is missing
	     or invalid, use this instead. [CORS: 7.1.5.1.Otherwise.9] */

	unsigned maximum_max_age;
	/**< Upper bound on delta-seconds that a server might
	     provide for age. [CORS: 7.1.5.1.Otherwise.9] */

	/* Parameters and configuration will mature with use;
	   just a placeholder to have a max size value.  */

	unsigned max_size;
	/**< Upper limit on cache. */
};

#endif // CORS_PREFLIGHT_H
