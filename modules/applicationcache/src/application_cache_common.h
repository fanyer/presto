
/*
 * application_cache_common.h
 *
 *  Created on: Nov 16, 2009
 *      Author: hmolland
 */

#ifndef APPLICATION_CACHE_COMMON_H_
#define APPLICATION_CACHE_COMMON_H_

#define MANIFEST_FILENAME UNI_L("manifest.mst")
#define APPLICATION_CACHE_XML_FILE_NAME UNI_L("cache_groups.xml")
#define CACHE_FOLDER_NAME_LENGTH (32)
#define APPLICATION_CACHE_LOAD_TIMEOUT (1*60*1000) /* 1 minute */

#include "modules/url/url2.h"

/* Caches that are not fully restored from disk after a shutdown/startup
 * of opera are stored as UnloadedDiskCache objects. Each cache are
 * read up from the cache_group.xml file and stored in this object until
 * the cache is used. When first used the read ApplicationCacheGroup and
 * ApplicationCache are restored. Only the Most recent complete and non-oboslete
 * cache is stored on disk for each group.
 */


class ApplicationCacheGroup;
class DOM_Environment;

struct UnloadedDiskCache
{
	UnloadedDiskCache()
		: m_cache_disk_size(0)
		, m_cache_disk_quota(0)
	{}

	URL m_manifest;
	OpString m_manifest_url;
	OpAutoVector<OpString> m_master_urls;
	OpString m_location;
	UINT m_cache_disk_size; // how much the cache uses on disk in kilobytes.

	int  m_cache_disk_quota;
};

struct UpdateAlgorithmArguments : public Link
{
	UpdateAlgorithmArguments(DOM_Environment *cache_host, const URL &manifest_url,  const URL &master_url, ApplicationCacheGroup *cache_group)
		: m_cache_host(cache_host)
		, m_cache_group(cache_group)
		, m_schedueled_for_start(FALSE)
		, m_owner(NULL)
	{
		m_manifest_url = manifest_url;
		m_master_url = master_url;
	}

	~UpdateAlgorithmArguments();

	DOM_Environment *m_cache_host;
	URL m_manifest_url;
	URL m_master_url;
	ApplicationCacheGroup *m_cache_group;
	BOOL m_schedueled_for_start;

	ApplicationCacheGroup *m_owner;
};


#endif /* APPLICATION_CACHE_COMMON_H_ */
