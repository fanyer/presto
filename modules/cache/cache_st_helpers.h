#ifndef CACHE_ST_HELPERS_H
#define CACHE_ST_HELPERS_H

#ifdef SELFTEST

#ifdef DISK_CACHE_SUPPORT
/**
 * Helper class that keeps track of loading URLs and files being deleted
 * from the cache.
 */
class WaitURLs: public MessageObject
{
public:
	WaitURLs(OpFileFolder defSizeFolder, OpFileFolder custSizeFolder, MessageHandler *msgHandler);

	// Add a URL of specified size to be loaded into the specified cache context.
	OP_STATUS AddURL(UINT32 size, URL_CONTEXT_ID context);

	// Add a manually created URL to be loaded.
	OP_STATUS AddURL(const URL &test);

	// Clear the list of URLs to test.
	void ClearURLs();

	// Start (re)loading all the test URLs.
	void Reload();

	// Set the number of expected files in the main and custom cache context.
	// This is used when waiting on files to be deleted and is mutually exclusive with the
	// size test.
	void SetExpectedFiles(int main, int custom, BOOL add_main = TRUE);

	// Set the expected size of the cache when deleting files. This is mutually exclusive with
	// the number of files test.
	void SetExpectedSize(int kbLow, int kbHigh, BOOL add_main = TRUE);
	
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	int					urlsRemaining;
	OpAutoVector<URL>	testURLs;
	OpFileFolder		defaultSizeFolder,
						customSizeFolder;
	
	int expectedSizeLow,
		expectedSizeHigh,
		expectedFilesMain,
		expectedFilesCustom,
		lastSize,
		lastDeleteCount, // Number of files in global delete list
		lastFilesMain,
		lastFilesCustom,
		timesWithNoChange,
		check_main_dir;

	MessageHandler *msgHandler;
	UINT32			currId;
	URL				emptyURL;
	URL_LoadPolicy	policy;
};
#endif // DISK_CACHE_SUPPORT

/// Class that contains some useful functions for the cache sefltests
class CacheHelpers
{
	/// Progressive ID used for the URLs
	int id;
public:
	CacheHelpers() { id=1024*1024*1024; /* let's keep some margin... */ }

#ifdef DISK_CACHE_SUPPORT
	/// Return the number of files in the index, -1 if the index is not present
	static int CheckIndex(OpFileFolder folder, BOOL accept_construction_failure=FALSE);
	/// Return the size on disk of the files requested
	static UINT32 ComputeDiskSize(FileName_Store *filenames);
#endif
#if ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)
	/// Return the number of files in the cache directory (<0 means error)
	static int CheckDirectory(OpFileFolder folder, BOOL add_assoc_files=FALSE);
#endif // ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)
	/// Close Windows not in use; this is important to avoid that the URLs are in use
	static void CloseUnusedWindows();
	/** Create fake URLs to put into the cache
		@param context: cache Context ID
		@param minSize: minimum size of the URL
		@param maxSize: maximum size of the URL
		@param num: Number of URLs to create
		@param totalBytes: maximum number of bytes added to the cache
		@param testURLs: List of the URLs created
		@param id: starting id used to create the path of the URLs
		@param plain: TRUE if the cache objects have to be plain (not embedded or in a container)
		@param load_in_the_past: TRUE if the URL has to be loaded in the past
		@param set_inuse: TRUE to increase the usage count of the URL
		@param unique: TRUE to make the URL unique
	*/
	static void CacheBogusURLs(int context, unsigned int minSize, unsigned int maxSize, int num, int totalBytes = 0, OpAutoVector<URL> *testURLs = NULL, int id = 0, BOOL plain = FALSE, BOOL load_in_the_past=FALSE, BOOL set_inuse=FALSE, BOOL unique=FALSE, URLType type=URL_HTTP);
	/// Create ONE fake URL and put it into the cache; the URL cannot be retrieved
	void CacheBogusURL(int context, unsigned int size, int url_id=-1){ CacheBogusURLs(context, size, size, 1, 0, NULL, (url_id<0) ? id++ : url_id, FALSE, FALSE); }
	/// Create ONE fake URL and put it into the cache; the URL can be retrieved.
	URL CacheBogusURLRetrieve(int context, unsigned int size, BOOL load_in_the_past=FALSE, BOOL set_inuse=FALSE, BOOL unique=FALSE, URLType type=URL_HTTP);	/// Create ONE fake URL and put it into the cache
	/** Create one URL with the specified binary content
		@param context: cache Context ID
		@param content: content to put in the URL
		@param size: Size of the content
		@target_url: URL that will be registered; please use something in the domain http://example.tld
		@param mime_type: Mime type to assign to the URL
		@param plain: TRUE if the cache objects have to be plain (not embedded or in a container)
		@param set_inuse: TRUE to increase the usage count of the URL
		@param unique: TRUE to make the URL unique
	*/
	static URL CacheCustomURLBinary(int context, const void *content, int size, const char *target_url=NULL, const char *mime_type="image/png", BOOL plain=FALSE, BOOL set_inuse=FALSE, BOOL unique=FALSE);
	/** Create one URL with the specified textual content
		@param context: cache Context ID
		@param content: content to put in the URL
		@param size: Size of the content
		@target_url: URL that will be registered; please use something in the domain http://example.tld
		@param mime_type: Mime type to assign to the URL
		@param plain: TRUE if the cache objects have to be plain (not embedded or in a container)
		@param set_inuse: TRUE to increase the usage count of the URL
		@param unique: TRUE to make the URL unique
	*/
	static URL CacheCustomURLText(int context, const char *content, const char *target_url=NULL, const char *mime_type="text/html", BOOL plain=FALSE, BOOL set_inuse=FALSE, BOOL unique=FALSE)
	{ return CacheCustomURLBinary(context, content, op_strlen(content), target_url, mime_type, plain, set_inuse, unique); }
};


#endif //SELFTEST

#endif //CACHE_ST_HELPERS_H
