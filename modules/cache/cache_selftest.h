#ifndef CACHE_SELFTEST_H

#define CACHE_SELFTEST_H

#ifdef SELFTEST

#include "modules/cache/url_cs.h"
#include "modules/cache/simple_stream.h"
#include "modules/cache/multimedia_cache.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/selftest/src/testutils.h"
#include "modules/cache/cache_st_helpers.h"

#ifdef WEBSERVER_SUPPORT
class UniteCacheTester;
#endif

class DebugFreeUnusedResources;
class UPnPDevice;
class CS_MessageHandler;
class CustomLoad;

/// Transfer type expected with the Unite server
enum Transfer {
	/// Full transfer: the whole file
	FULL,
	/// Conditional transfer: only the header
	CONDITIONAL,
	/// NO transfer: loaded from the cache
	NONE };

enum CacheType { TYPE_EMBEDDED, TYPE_CONTAINER, TYPE_PLAIN };

/** Policy for redirects */
enum RedirectPolicy
{
	/** Error if redirect */
	REDIR_NOT_ALLOWED,
	/** Redirect possible. Don't follow it. */
	REDIR_ALLOWED_DONT_FOLLOW,
	/** Redirect possible. Follow it. */
	REDIR_ALLOWED_FOLLOW,
	/** Redirect required. Don;t follow it. */
	REDIR_REQUIRED_DONT_FOLLOW,
	/** Redirect required. Follow it. */
	REDIR_REQUIRED_FOLLOW
};

/// Which method to use for the load call
enum LoadMethod
{
	/// Load with URL::LoadDocument()
	LOAD_LOADDOCUMENT,
	/// Load with URL::Load()
	LOAD_LOAD,
	/// Load with URL::Reload()
	LOAD_RELOAD
};


/// Message object used for the cache selftests
class CS_MessageObject: public MessageObject
{
friend class CS_MessageHandler;
protected:
#ifdef DISK_CACHE_SUPPORT
	/// Check if the URL contains the same data of the file
	OP_STATUS CheckContentDuringLoad(URL_Rep *rep);

	/// File with the original content, used to verify the download (NULL means no verify))
	OpFileDescriptor *expected_content;
#endif // DISK_CACHE_SUPPORT
	/// HTTP response code expected for the file
	UINT32 expected_response_code;
	/// Bytes the object is waiting for (positiong retrieved by LoadDocument)
	OpFileLength waiting_for;
	/// Minimum number of bytes expected to be transfered (-1 means no check)
	int min_exp_trnsf_bytes;
	/// Maximum number of bytes expected to be transfered (-1 means no check)
	int max_exp_trnsf_bytes;
	/// URL that is expected (this is also required to support unique URLs)
	URL *url_expected;
	/// TRUE if we expect an error (for example because content_filter should block the URL)
	BOOL expect_error;
	/// String representation of the URL to download, to avoid a crash
	OpString8 str_url_downloaded;

public:
	CS_MessageObject() {
#ifdef DISK_CACHE_SUPPORT
		expected_content=NULL;
#endif // DISK_CACHE_SUPPORT
		expected_response_code=200;
		waiting_for=0;
		SetExpectedTransferRange(-1, -1);
		url_expected=NULL;
		expect_error = FALSE;
	}

	// Set the range of bytes expected to be transfered
	void SetExpectedTransferRange(int min, int max, BOOL error=FALSE) { min_exp_trnsf_bytes=min; max_exp_trnsf_bytes=max; expect_error=error; }


#ifdef DISK_CACHE_SUPPORT
	/// Set the file with the expected content (and URL)
	void SetExpectedContent(OpFileDescriptor *expected, URL *url) { expected_content=expected; str_url_downloaded.Empty(); url_expected=url; }
	/// Set only the expected URL
	void SetExpectedURL(URL *url) { url_expected=url; }
#endif // DISK_CACHE_SUPPORT
	/// Set the response code expected for the download
	void SetExpectedResponceCode(UINT32 expected_code) { expected_response_code=expected_code; }
	/// Set the URL to download
	void SetURLDownloaded(URL *url) { str_url_downloaded.Set(url->GetAttribute(URL::KName)); }
};

/// Class waiting the download of a single URL
class WaitSingleURL: public CS_MessageObject, OpTimerListener
{
private:
	friend class CustomLoad;
#ifdef WEBSERVER_SUPPORT
	/// Object used to print some statictical informations
	UniteCacheTester *uct;
#endif
	/// Maximum amount of inactivity tolerated
	UINT32 wait_time;
	/// TRUE if some activity has been eprformed
	BOOL activity;
	/// Timer used for the activity check
	OpTimer timer;
	/// Bit field (one bit for each download, eg 0x05 means 1 for first and third download);
	/// For each bit, TRUE if this is part of a real "user download" (that requires a Download_Storage)
	UINT32 download_pattern;
	// Counter used to avoid conflicts on file names, for "downloads"
	UINT32 num_downloads;
	// Number of times that the file has to be fully loaded (meaning MSG_URL_DATA_LOADED with status URL_LOADED) 
	// for the test to be considered completed
	int num_expected_load;
	// Number of times that the file has been fully loaded
	int num_load_executed;
	// Number of times that the header has been loaded
	int num_headers_executed;
	// Decide if it has to be automatically deleted
	BOOL auto_delete;
	// TRUE if it is OK that no bytes has been transfered from the webserver.
	// This is useful for multiple downloads of the same URL, because, depending on the load method, 
	// the content could be retrieved from the cache, generating 0 bytes of transfer).
	// If more than 0 bytes are transfered, then they need to fall in the accepted transfer range
	BOOL accept_no_transfer;
	/// TRUE if this object is allowed to cal ST_passed()
	BOOL enable_st_passed;
	/// To try to simulate different timings, the class can "trigger the download" (calling LoadToFile(), which change
	/// the cache storage to Download_Storage) in different moments:
	/// 0 ==> when the headers have been received
	/// 1 ==> After the first packet has been received
	/// 2 ==> After the second packet has been received
	/// X ==> After the packet number X has been received
	int download_moment;
	/// Current download packet
	int packet_num;

	/// Cleanup required when the selftest is finished (both sucessfully of not), in particular the timer is stopped
	/// and the callbacks are unset.
	/// NOTE that the *this* pointer could be deleted, so after this call the handler MUST return
	void ST_End();

	// Check if LoadToFile() has to be called for the given download; return TRUE if the function must immediately exit (for example because the object has been deleted, calling ST_End() )
	BOOL ManageLoadToFile(URL_Rep *rep, int download_num, BOOL force=FALSE);
	
public:
	#ifdef WEBSERVER_SUPPORT
		WaitSingleURL(UniteCacheTester *tester, BOOL is_download=FALSE) { uct=tester; wait_time=5000; activity=FALSE; timer.SetTimerListener(this); timer.Start(wait_time); num_downloads=0; download_pattern=(is_download)?1:0; SetNumberOfLoading(1); auto_delete=TRUE; accept_no_transfer=FALSE; enable_st_passed=TRUE; download_moment=packet_num=0; }
	#else
		WaitSingleURL() { wait_time=5000; timer.SetTimerListener(this); timer.Start(wait_time); download_pattern=0; num_downloads=0; SetNumberOfLoading(1); auto_delete=TRUE; accept_no_transfer=FALSE; enable_st_passed=TRUE; download_moment=packet_num=0; }
	#endif
	virtual ~WaitSingleURL();

	/// Start the timer
	void StartTimeout() { timer.Stop(); timer.Start(wait_time); }
	/// Stop the timer
	void StopTimeout() { timer.Stop(); }
	/// Override the default wait time for inactivity; stop and start the timeout
	void SetWaitTime(UINT32 wait) { wait_time=wait; StopTimeout(); StartTimeout(); }
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual void OnTimeOut(OpTimer* timer);
	/// Set the number of times that the URL is supposed to be loaded; it also reset the loading state
	/// -1 means that ST_passed() will be called for every load
	void SetNumberOfLoading(int n) { num_expected_load=n; num_load_executed=0; num_headers_executed=0; OP_ASSERT(num_expected_load>0 || num_expected_load==-1); } 
	/// Set the "download storage" status for a given download operation. In case of multiple download executed with the same object,
	/// it can be useful to mark as "download storage" only some of them; this function set the pattern to allow this flexibility
	void SetDownloadStorageStatus(BOOL b, int num_download=0) { UINT32 mask=((UINT32)1)<<num_download; download_pattern&=(~mask); if(b) download_pattern|=mask; }
	/// Reset the loading state, so that new downloads can be performed
	void ResetLoadingState(BOOL st_passed_allowed, int moment) { num_load_executed=0; num_headers_executed=0; enable_st_passed=st_passed_allowed; download_moment=moment; }
	/// Enable this object to call ST_passed()
	void SetEnable_ST_passed(BOOL b) { enable_st_passed=b; }
	/// To try to simulate different timings, the class can "trigger the download" (calling LoadToFile(), which change
	/// the cache storage to Download_Storage) in different moments:
	/// 0 ==> when the headers have been received
	/// 1 ==> After the first packet has been received
	/// 2 ==> After the second packet has been received
	/// X ==> After the packet number X has been received
	/// @param moment Moment chosen to change the storage to Download_Storage 
	void SetDownloadMoment(int moment) { download_moment=moment; }
	/// Get the download storage status for a given download operation
	BOOL GetDownloadStorage(int num_download=0) { UINT32 mask=((UINT32)1)<<num_download; return (download_pattern&mask)>0; }
	/// Set the auto delete mode
	void SetAutoDelete(BOOL b) { auto_delete=b; }
	/// Set if it is acceptable to not transfer bytes from the webserver
	void SetAcceptNoTransfer(BOOL b) { accept_no_transfer=b; }
};

/// Class used to set timeout for asynchronous selftests
class SelfTestTimer: public OpTimerListener
{
private:
	OpTimer timer;

public:
	SelfTestTimer() { timer.SetTimerListener(this); }
	virtual ~SelfTestTimer() { StopTimer(); }
	/// Start the timer, and wait maximum for the specified amount of ms
	void StartTimer(int wait_time) { timer.Stop(); timer.Start(wait_time); }
	// Stop the timer
	void StopTimer() { timer.Stop(); }
	virtual void OnTimeOut(OpTimer* timer) { ST_failed("Test timed out!"); }
};

#ifdef DISK_CACHE_SUPPORT
/// Class used to customize the load
class CustomLoad
{
private:
	WaitSingleURL *wait;		// Custom Message object
	LoadMethod method;			// Method used to load the URL
	// Internal MessageHandler
	CS_MessageHandler *mh;
	
	friend class UniteCacheTester;
	friend class CS_MessageHandler;
	

public:
	CustomLoad(WaitSingleURL *w, LoadMethod lm=LOAD_LOADDOCUMENT) { wait=w; method=lm; mh=NULL; }
	virtual ~CustomLoad();

	/// Create an internal Message handler, that will be used instead of the normal one (enable multiple downloads)
	void CreateMessageHandler();
	// Reset the loading state, to be able to start a new download
	void ResetLoadingState(BOOL st_passed_allowed, int moment) { OP_ASSERT(wait); if(wait) wait->ResetLoadingState(st_passed_allowed, moment); }
	// Calls LoadToFile(); return TRUE if the function must immediately exit (for example because the object has been deleted, calling ST_End() )
	BOOL ManageLoadToFile(URL_Rep *rep, int num_download) { if(wait && wait->GetDownloadStorage(num_download)) return wait->ManageLoadToFile(rep, -1, TRUE); return FALSE; }
};

/// Message handler that with empty URL and default policy
class CS_MessageHandler: public MessageHandler
{
private:
	friend class UniteCacheTester;
	URL empty_url;
	URL_LoadPolicy unconditional_policy; 	// Unconditional reload from the network
	URL_LoadPolicy noreload_policy;			// Reload from the cache
	URL_LoadPolicy normal_policy;   		// Reload if expired
	WaitSingleURL *default_wait;
	/// Minimum number of bytes expected to be transfered (-1 means no check)
	int min_exp_trnsf_bytes;
	/// Maximum number of bytes expected to be transfered (-1 means no check)
	int max_exp_trnsf_bytes;
	/// TRUE if we expect an error (for example because content_filter should block the URL)
	BOOL expect_error;
	
	/// Start to load a URL
	OP_STATUS LoadURL(URL &url, URL_LoadPolicy policy, CustomLoad *cl);

public:
	CS_MessageHandler(Window *window=NULL): MessageHandler(window), unconditional_policy(FALSE, URL_Reload_Full), noreload_policy(FALSE, URL_Reload_None), default_wait(NULL), expect_error(FALSE) { SetExpectedTransferRange(-1, -1); }
	
	void SetDefaultMessageObject(WaitSingleURL *wait) { default_wait=wait; }
	
	// Set the range of bytes expected to be transfered
	void SetExpectedTransferRange(int min, int max, BOOL error=FALSE) { min_exp_trnsf_bytes=min; max_exp_trnsf_bytes=max; expect_error=error; }

	/// Unconditional Reload from the server
	void LoadUnconditional(URL &url, UINT32 expected_response=200, WaitSingleURL *wait=NULL, OpFileDescriptor *expected_content=NULL, CustomLoad *cl=NULL);
	
	/// Load without Reload (so the file will be read from the cache)
	void LoadNoReload(URL &url, UINT32 expected_response=200, WaitSingleURL *wait=NULL, OpFileDescriptor *expected_content=NULL);

	/// Load with policy Normal (so the file usually will be read from the cache)
	void LoadNormal(URL &url, UINT32 expected_response=200, WaitSingleURL *wait=NULL, OpFileDescriptor *expected_content=NULL);

	/// Uses Reload() instead of LoadDocument
	void Reload(URL &url, UINT32 expected_response=200, WaitSingleURL *wait=NULL, OpFileDescriptor *expected_content=NULL);

	/// Returns TRUE if Unite is running or if the url is not using Unite
	static BOOL CheckUniteURL(URL url);
};



#ifdef WEBSERVER_SUPPORT
/** Empty WebServer listener */
class CS_WebserverEventListenerEmpty: public WebserverEventListener
{
public:
	virtual void OnWebserverStopped(WebserverStatus status) { }
	virtual void OnWebserverUPnPPortsClosed(UINT16 port) { }
	virtual void OnWebserverServiceStarted(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE) { }
	virtual void OnWebserverServiceStopped(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE) { }
	virtual void OnWebserverListenLocalStarted(unsigned int port) { }
	virtual void OnWebserverListenLocalStopped() { }
	virtual void OnWebserverListenLocalFailed(WebserverStatus status) { }
	virtual void OnWebserverUploadServiceStatus(UploadServiceStatus status) { }
	virtual void OnProxyConnected() { }
	virtual void OnProxyConnectionFailed(WebserverStatus status, BOOL retry) { }
	virtual void OnProxyConnectionProblem(WebserverStatus status,  BOOL retry) { }
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry) { }
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry, int code) { }
	virtual void OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address) { }
	virtual void OnNewDOMEventListener(const uni_char *service_name, const uni_char *evt, const uni_char *virtual_path = NULL) { }
	virtual void OnPortOpened(UPnPDevice *device, UINT16 internal_port, UINT16 external_port) { }
};

/** Listener that waits for the WebServer to start, and optionally for the proxy to connect */
class CS_WebserverEventListenerStart: public CS_WebserverEventListenerEmpty, OpTimerListener
{
private:
	/// TRUE if we have to wait for the proxy
	BOOL wait_for_proxy;
	/// Timer used for timeout
	OpTimer timer;
	// TRUE if at least a local connection has been established
	BOOL connected_locally;
	// TRUE if the proxy connection has been established
	BOOL connected_proxy;
	// TRUE if at this point hte proxy can disconnect
	BOOL proxy_can_disconnect;

	void passed() { timer.Stop(); ST_passed(); }
	void failed(const char *mex) { timer.Stop(); ST_failed(mex); }
	void failed(const char *mex, int n) { timer.Stop(); ST_failed(mex, n); }

public:
	CS_WebserverEventListenerStart() { wait_for_proxy=FALSE; connected_locally=FALSE; connected_proxy=FALSE; proxy_can_disconnect=FALSE; timer.SetTimerListener(this); timer.Start(30000); }
	virtual ~CS_WebserverEventListenerStart() { timer.Stop(); } 

	/// Set if the listener has to wait for the proxy to connect
	void SetWaitForProxy(BOOL b) { wait_for_proxy=b; }

	virtual void OnWebserverListenLocalStarted(unsigned int port) { timer.Stop(); connected_locally=TRUE; if(wait_for_proxy) { if(!connected_proxy) { timer.Stop(); timer.Start(30000); } } else passed(); }
	virtual void OnWebserverListenLocalFailed(WebserverStatus status) { failed("Unite Webserver cannot start - Status: %d", status); }
	virtual void OnProxyConnected() { connected_proxy=TRUE; if(wait_for_proxy) passed(); else failed("Unexpected proxy connection!"); }
	virtual void OnProxyConnectionFailed(WebserverStatus status, BOOL retry) { failed("Unite Proxy cannot connect - Status: %d", status); }

	virtual void OnTimeOut(OpTimer* timer) { if(connected_locally) { output("*** TIMEOUT: Unite connected only locally."); passed(); } else failed("Not even local connection to Unite"); }

	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry) { if(!proxy_can_disconnect) failed("Proxy disconnected too soon"); }
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry, int code) { if(!proxy_can_disconnect) failed("Proxy disconnected too soon"); }

	// Signal that from this moment the proxy can safely disconnect
	void EnableProxyDisconnect() { proxy_can_disconnect=TRUE; } 
};

class WaitSingleURL;

/// Class used to test the cache using the Unite Webserver
class UniteCacheTester
{
	BOOL ws_started;  // TRUE if the WebServer has been started by this class
	CS_WebserverEventListenerStart lsn_start;
	WebSubServer *new_subserver;
	Window* win;
	TRANSFERS_TYPE last_bytes; // total bytes transferred last time that it was checked
	TRANSFERS_TYPE last_transfer; // bytes transferred between the last two checks
	CS_MessageHandler mh;   // Normal message handler
	OpString8 service_name;
	Window_Type	win_type;
	BOOL webserver_module_initialized;  // TRUE if the WebServer module has been initialized (it means that Unite was disabled via pref)

public:
	~UniteCacheTester() { if(win) win->SetType(win_type); StopWebServer(); }
	UniteCacheTester(Window* window=NULL) { ws_started=FALSE; new_subserver=NULL;  win=window; last_bytes=0; last_transfer=0; SetExpectedTransferRange(-1, -1); win_type=WIN_TYPE_NORMAL; webserver_module_initialized=FALSE; }
	
	// Set the window used by the selftests
	void SetWindow(Window* window) { win=window; if(win) win_type=win->GetType(); }

	// Set the range of bytes expected to be transfered; it also resets the current transfer value
	void SetExpectedTransferRange(int min, int max, BOOL error=FALSE) { GetNewBytesTransferred(); mh.SetExpectedTransferRange(min, max, error); }
	
	// Returns TRUE if Unite is properly configured (user, device, host and password are not empty)
	static BOOL IsUniteProperlyConfigured(OpString8 &user, OpString8 &dev, OpString8 &proxy, OpString8 &pw);

	/// Starts the Unite WebServer (if required)
	void StartWebServer(WebserverListeningMode mode = WEBSERVER_LISTEN_LOCAL);
	/// Stops the Unite WebServer (if started by this object)
	void StopWebServer();	
	void StartService(const uni_char *name, const uni_char *storage_path);	
	/** Share a file
		@param file_path Path of the physical file
		@param str_vpath Virtual path
		@param size (output) size of the file shared
		@param dest_url (output) do a GetURL() for the shared URL (shared based on the last call to StartService())
		@param ctx Context to use
	*/
	OP_STATUS Share(const uni_char *file_path, const char *str_vpath, OpFileLength *size, URL &dest_url, URL_CONTEXT_ID ctx);
	/** Share the same file several times; this is usefull to create a big number of files of the same size
		@param num Number of files to create
		@param file_path Path of the physical file
		@param str_vpath Virtual path (a number will be added at the end, starting from 1)
		@param size (output) size of the file shared
		@param dest_urls (input / output) URLs created; it needs to be an array of num URLs
		@param ctx Context to use
	*/
	//OP_STATUS ShareNum(int num, const uni_char *file_path, const char *str_vpath, OpFileLength *size, URL &dest_urls[], URL_CONTEXT_ID ctx);
	/// Unshare a file
	OP_STATUS UnShare(const char *str_vpath);	
	/// Get an URL for the given virtual path (shared based on the last call to StartService())
	URL GetURL(const char *str_vpath, URL_CONTEXT_ID ctx);
	/// Show the bytes transferred by the WebServer
	void PrintTransferredBytes();
	/// Return the number of bytes transferred after the last call; (UINT32)-1 means data not available
	UINT32 GetNewBytesTransferred();
	/// Return the number of bytes transferred between the last two calls
	UINT32 GetLastTransfer();
	/// Return the total amount of bytes transferred by the Web Server
	UINT32 GetFullBytesTrasferred() { if(!g_webserver) return (UINT32)-1; return (UINT32) g_webserver->GetBytesUploaded(); }

	/// Returns the internal Message Handler
	MessageHandler *GetMessageHandler() { return &mh; }
	/// Returns an object that wait for URL to be downloaded in the usual way
	WaitSingleURL *CreateDefaultMessageObject(BOOL auto_delete) { WaitSingleURL *wait=OP_NEW(WaitSingleURL, (this, FALSE)); if(wait) wait->SetAutoDelete(auto_delete); return wait; }
	/// Returns an object that wait for URL to be downloaded with the logic used for "downloads" (using Download_storage)
	WaitSingleURL *CreateDownloadMessageObject(BOOL auto_delete) { WaitSingleURL *wait=OP_NEW(WaitSingleURL, (this, TRUE)); if(wait) wait->SetAutoDelete(auto_delete); return wait; }
	/** Load unconditionally the given URL and pass the test if the load is successfull and the content (if not NULL) is as expected
		@param url URL to load
		@param expected_content File with the content expected by the download. NULL means no verify.
								WARNING: the object MUST not be used until the download has been completed.
	*/
	void LoadOK(URL &url, OpFileDescriptor *expected_content = NULL, CustomLoad *cl=NULL);
	/** Load without Reload (so the file will be get from the cache) the given URL and pass the test if the load is successful and the content (if not NULL) is as expected
		@param url URL to load
		@param expected_content File with the content expected by the download. NULL means no verify.
								WARNING: the object MUST not be used until the download has been completed.
	*/
	void LoadNoReloadOK(URL &url, OpFileDescriptor *expected_content = NULL, BOOL assume_no_transfer = TRUE);
	/** Load with Normal policy (the file is probably expected to be retrieved from the cache) the given URL and pass the test if the load is successfull and the content (if not NULL) is as expected
			@param url URL to load
			@param expected_content File with the content expected by the download. NULL means no verify.
									WARNING: the object MUST not be used until the download has been completed.
		*/
	void LoadNormalOK(URL &url, OpFileDescriptor *expected_content = NULL);
	/** Load unconditionally the given URL and pass the test if the load is a 404
		@param url URL to load
	*/
	void Load404(URL &url) { LoadStatus(url, 404); }
	/** Load unconditionally the given URL and pass the test if the load is a 404
		@param url URL to load
	*/
	void LoadStatus(URL &url, int http_status_code);
	/** Load unconditionally the given URL and pass the test if there is an error
		@param url URL to load
	*/
	void LoadError(URL &url);
	/** Load without Reload (so the file will be get from the cache) the given URL  and pass the test if the load is a 404
		@param url URL to load
	*/
	void LoadNoReload404(URL &url);

	/////////////// JS API
	/** Create a page of the given size and age; requires the service "generate_page" defined in cache_html.ot
		@param file_size size of the file to download
		@param age, before the URL expires
		@param ctx Context ID
		@param start_download TRUE to start the download (with a LoadOK() call)
	*/
	URL JScreatePage(int file_size, int age, URL_CONTEXT_ID ctx, BOOL start_download=TRUE, BOOL never_flush=FALSE);
	/// Create a page of the given size and age; requires the service "generate_page" defined in cache_html.ot
	/// The address will be the external Unite address of this instance.
	/// However, if Unite is not properly configured, this method will fallback to JScreatePage(), on an attempt to simplify selftests
	URL JScreateExternalPage(int file_size, int age, URL_CONTEXT_ID ctx, BOOL start_download=TRUE);

	/// Reset the steps
	void JSResetSteps(URL_CONTEXT_ID ctx, int step);

	/// Load the auto_step service (normal load) and check the expected transfer
	void JSAutoStepNormal(URL_CONTEXT_ID ctx, int file_size, Transfer exp_transfer);
	/// Load the auto_step service (No Reload) and check the expected transfer
	void JSAutoStepNoReload(URL_CONTEXT_ID ctx, int file_size, Transfer exp_transfer);
	/// Unset the callbacks in the message handler
	void UnsetCallBacks(MessageObject *mo) { mh.UnsetCallBacks(mo); }
};

#endif //WEBSERVER_SUPPORT	

/// Tests for the Multimedia cache
class MMCacheTest
{
private:
	URL_Rep *rep;
	Cache_Storage *cs;
	OpAutoVector<StorageSegment> unsort_seg;	// User to store unsorted segments
	OpAutoVector<StorageSegment> sort_seg;		// User to store sorted segments
	OpAutoVector<StorageSegment> sort_seg_dd;	// User to store unsorted segments retrieved from the URL_DataDescriptor
	
	/// Check if a segment is fine
	OP_STATUS CheckSegment(StorageSegment *seg, OpFileLength start, OpFileLength size)
	{
		OP_ASSERT(seg);
		if(!seg)
			return OpStatus::ERR_NULL_POINTER;
		else
		{
			OpString8 file8;

			RETURN_IF_ERROR(file8.Set(rep->GetAttribute(URL::KFileName).CStr()));

			if(seg->content_start!=start)
				ST_failed("Segment start is wrong: %d != %d for %s in %s!", (int)seg->content_start, (int)start, rep->GetAttribute(URL::KName).CStr(), file8.CStr());
			else if(seg->content_length!=size)
				ST_failed("Segment length is wrong: %d != %d for %s in %s!", (int)seg->content_length, (int)size, rep->GetAttribute(URL::KName).CStr(), file8.CStr());
			else
				return OpStatus::OK;
		}
		
		return OpStatus::ERR_OUT_OF_RANGE;
	}

public:
	MMCacheTest(URL_Rep *url_rep)
	{
		rep=url_rep;
		OP_ASSERT(rep);
		cs=(rep && rep->GetDataStorage() && rep->GetDataStorage()->GetCacheStorage())?rep->GetDataStorage()->GetCacheStorage():NULL;
		
		OP_ASSERT(cs);
	}
	
	/// Check a segment in the sorted collection
	OP_STATUS CheckSortedSegment(UINT32 num, OpFileLength start, OpFileLength end)
	{
		OP_STATUS res=OpStatus::OK;
		
		if(!cs)
			res=OpStatus::ERR_NULL_POINTER;
		else if(sort_seg.GetCount()==0)
		{
			res=cs->GetSortedCoverage(sort_seg);

			// Debug code
			URL_DataDescriptor *dd=rep->GetDescriptor(NULL, URL::KNoRedirect);

			dd->GetSortedCoverage(sort_seg_dd);
			OP_ASSERT(sort_seg_dd.GetCount()==sort_seg.GetCount());


			OP_DELETE(dd);
		}
		
		if(OpStatus::IsSuccess(res))
			res=CheckSegment(sort_seg.Get(num), start, end);
		else
			ST_failed("Error during segment check!");
		
		return res;
	}
	
	// Check if the partial coverage is as expected
	void CheckPartialCoverage(OpFileLength position, BOOL multiple_segments, BOOL expected_available, OpFileLength expected_length)
	{
		if(!cs)
		{
			ST_failed("NULL cache storage!");

			return;
		}
		
		BOOL available;
		OpFileLength length;

		cs->GetPartialCoverage(position, available, length, multiple_segments);

		if(expected_available!=available)
			ST_failed("Availability mismatch!");
		else if(expected_length!=length)
			ST_failed("Length mismatch: %d insted of %d!", (int)length, (int)expected_length);
	}

	/// Check a segment in the unsorted collection
	OP_STATUS CheckUnsortedSegment(UINT32 num, OpFileLength start, OpFileLength end)
	{
		OP_STATUS res=OpStatus::OK;
		
		if(!cs)
			res=OpStatus::ERR_NULL_POINTER;
		else if(unsort_seg.GetCount()==0)
			res=cs->GetUnsortedCoverage(unsort_seg);
		
		if(OpStatus::IsSuccess(res))
			res=CheckSegment(unsort_seg.Get(num), start, end);
		else
			ST_failed("Error during segment check!");
		
		return res;
	}
	
	/// Return the number of unsorted segments
	UINT32 GetUnsortedCount()
	{
		if(!cs)
			return 0;
		else if(unsort_seg.GetCount()==0)
			cs->GetUnsortedCoverage(unsort_seg);
			
		return unsort_seg.GetCount();
	}
	
	/// Return the number of sorted segments
	UINT32 GetSortedCount()
	{
		if(!cs)
			return 0;
		else if(sort_seg.GetCount()==0)
		{
			cs->GetSortedCoverage(sort_seg);

			// Debug code
			URL_DataDescriptor *dd=rep->GetDescriptor(NULL, URL::KNoRedirect);

			dd->GetSortedCoverage(sort_seg_dd);
			OP_ASSERT(sort_seg_dd.GetCount()==sort_seg.GetCount());


			OP_DELETE(dd);
		}
			
		return sort_seg.GetCount();
	} 
	
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
	/// Partial download, normal policy
	static void PartialDownload(URL &url, OpFileLength start, OpFileLength end, CS_MessageHandler *mh, OpFileDescriptor *expected = NULL)
	{
		url.SetAttribute(URL::KMultimedia, TRUE);
		url.SetAttribute(URL::KHTTPRangeStart, &start);
		url.SetAttribute(URL::KHTTPRangeEnd, &end);

		mh->LoadUnconditional(url, 206, NULL, expected);
	}
	
	/// Partial download, no Reload
	static void PartialDownloadNoReload(URL &url, OpFileLength start, OpFileLength end, CS_MessageHandler *mh, OpFileDescriptor *expected = NULL)
	{
		url.SetAttribute(URL::KMultimedia, TRUE);
		url.SetAttribute(URL::KHTTPRangeStart, &start);
		url.SetAttribute(URL::KHTTPRangeEnd, &end);
		
		mh->LoadNoReload(url, 206, NULL, expected);
	}
#endif // URL_ENABLE_HTTP_RANGE_SPEC
	
	/// Full download, normal policy
	static void FullDownload(URL &url, CS_MessageHandler *mh)
	{
		OpFileLength start=FILE_LENGTH_NONE;
		OpFileLength end=FILE_LENGTH_NONE;

		url.SetAttribute(URL::KMultimedia, TRUE);
		url.SetAttribute(URL::KHTTPRangeStart, &start);
		url.SetAttribute(URL::KHTTPRangeEnd, &end);

		OP_NEW_DBG("MMCacheTest::FullDownload()", "cache.selftest.mm");
		OP_DBG((UNI_L("Full download\n")));
		
		mh->LoadNormal(url);
	}
	
	/// Full download, no reload
	static void FullDownloadNoReload(URL &url, CS_MessageHandler *mh)
	{
		OpFileLength start=FILE_LENGTH_NONE;
		OpFileLength end=FILE_LENGTH_NONE;

		url.SetAttribute(URL::KMultimedia, TRUE);
		url.SetAttribute(URL::KHTTPRangeStart, &start);
		url.SetAttribute(URL::KHTTPRangeEnd, &end);

		OP_NEW_DBG("MMCacheTest::FullDownload()", "cache.selftest.mm");
		OP_DBG((UNI_L("Full download\n")));
		
		mh->LoadNoReload(url);
	}

	/// Full download, unconditional reload
	static void FullDownloadUnconditional(URL &url, CS_MessageHandler *mh)
	{
		OpFileLength start=FILE_LENGTH_NONE;
		OpFileLength end=FILE_LENGTH_NONE;

		url.SetAttribute(URL::KMultimedia, TRUE);
		url.SetAttribute(URL::KHTTPRangeStart, &start);
		url.SetAttribute(URL::KHTTPRangeEnd, &end);
		
		mh->LoadUnconditional(url);
	}

	/// Full download, calling Reload() instead of LoadDocument()
	static void FullDownloadOldReload(URL &url, CS_MessageHandler *mh)
	{
		OpFileLength start=FILE_LENGTH_NONE;
		OpFileLength end=FILE_LENGTH_NONE;

		url.SetAttribute(URL::KMultimedia, TRUE);
		url.SetAttribute(URL::KHTTPRangeStart, &start);
		url.SetAttribute(URL::KHTTPRangeEnd, &end);
		
		mh->Reload(url);
	}

	/// Get the partial coverage (assuming multiple segments)
	static void VerifyPartialCoverage(MultimediaCacheFile *cf, OpFileLength position, BOOL expected_available, OpFileLength expected_length);
	/// Read the content from the cache and checks it against a supplied array of values
	static void ReadAndVerifyContent(MultimediaCacheFile *cf, OpFileLength position, OpFileLength requested_length, OpFileLength expected_length, UINT8 *read_buf, UINT8 *expected_content);
	/// Read the content from the cache and checks it against a supplied array of values
	static void ReadFromFileAndVerifyContent(MultimediaCacheFileDescriptor *cfd, OpFileLength position, OpFileLength requested_length, OpFileLength expected_length, UINT8 *read_buf, UINT8 *expected_content);
};
#endif // DISK_CACHE_SUPPORT

#ifdef DIRECTORY_SEARCH_SUPPORT
/// Some generic cache test
class CacheFileTest
{
private:
	/// Open a file and get the size (>0)
	static OP_STATUS OpenFile(OpFile &file, const uni_char *file_path, OpFileLength &file_size);
public:
	/// Delete a directory
	static OP_STATUS DeleteCacheDir(OpFileFolder folder);
	/// Create an empty cache directory
	static OP_STATUS CreateEmptyCacheDir(const uni_char *name, OpFileFolder &folder);

#ifdef DISK_CACHE_SUPPORT
	/// Open the cache file of a URL, and check the sign
	static OP_STATUS VerifyFileSign(URL &url, const char *sign, OpFileLength &len, BOOL writeCache);
	/// Verify that the content of the file in the cache (url) is the same as expected (file_path)
	static OP_STATUS VerifyFileContent(URL &url, const uni_char *file_path, RedirectPolicy redir_policy=REDIR_NOT_ALLOWED, BOOL temporary_url=FALSE);
	/// Return the size of the cache file, OP_FILE_LENGTH_NONE if error
	static OpFileLength GetCacheFileSize(URL_Rep *rep, BOOL follow_redirect);
#endif // DISK_CACHE_SUPPORT
#ifdef CACHE_FAST_INDEX
	/// Verify that the content of a reader is the same as expected
	static OP_STATUS VerifyFileContent(SimpleStreamReader *reader, const uni_char *file_path);
	/// Verify that the content of a file is the same as expected (provided as a string)
	static OP_STATUS VerifyFileContent(const uni_char *file_path, const OpString8 &expected);
#endif
	// Create a binary file in the temporary directory
	static OP_STATUS CreateTempFile(UINT32 size, BOOL random_content, OpString &file_path, OpFile &file, UINT32 seq=0);
};
#endif // DIRECTORY_SEARCH_SUPPORT

/// Sizes to use for cache testing
enum CacheSize
{
#if CACHE_SMALL_FILES_LIMIT>0
	SIZE_EMBEDDED=CACHE_SMALL_FILES_SIZE-1,
#else
	SIZE_EMBEDDED=0,
#endif

#if CACHE_CONTAINERS_ENTRIES>0
	SIZE_CONTAINERS=CACHE_CONTAINERS_FILE_LIMIT-1,
	SIZE_PLAIN=CACHE_CONTAINERS_FILE_LIMIT*2
#else
	SIZE_CONTAINERS=0,
	SIZE_PLAIN=32768
#endif
};

/// Class used to test the LRU logic of the cache
class CacheTester
{
private:
	/// Test with 3 URLs that the LRU logic is fine
	/// Return TRUE if it is a pass
	static void TestURLs(Context_Manager *ctx, URL_DataStorage **list, URL url1, URL url2, URL url3, URL url4, URL url5);

public:
	/// @return TRUE if the context manager is frozen
	static BOOL IsContextManagerFrozen(URL_CONTEXT_ID ctx_id) { return urlManager->IsContextManagerFrozen(urlManager->FindContextManager(ctx_id)); }
	/// Get an URL from he test server; this URL will generate a page of the given size, with the given id
	static URL GetURLTestServerSize(URL_CONTEXT_ID ctx, UINT32 size, const char* id=NULL, const char *mime_type=NULL);
	/// Get an URL from he test server (but from the external SSL address); this URL will generate a page of the given size, with the given id
	static URL GetURLExternalTestServerSize(URL_CONTEXT_ID ctx, UINT32 size, const char* id=NULL, const char *mime_type=NULL);
	/// Test with 3 URLs that the LRU logic is fine
	/// Return TRUE if it is a pass
	static void TestURLsRAM(URL_CONTEXT_ID ctx_id);
#ifdef DISK_CACHE_SUPPORT
	/// Test with 3 URLs that the LRU logic is fine
	/// Return TRUE if it is a pass
	static void TestURLsTemp(URL_CONTEXT_ID ctx_id);
	/// Test with 3 URLs that the LRU logic is fine
	/// Return TRUE if it is a pass
	static void TestURLsDisk(URL_CONTEXT_ID ctx_id);
#endif // DISK_CACHE_SUPPORT
	/// Test SkipNextURL_Reps(num) versus num*GetNextURL_Rep()
	static void TestURLsSkip(URL_CONTEXT_ID ctx_id, UINT32 num);
	/// Move the URL creation in the past
	static void MoveCreationInThePast(URL_DataStorage *ds);
	/// Move the time of the last visit in the past
	static void MoveLocalTimeVisitedInThePast(URL_Rep *rep);
	/// Block the task that periodically free the resources, invalidating some selftests
	static void BlockPeriodicFreeUnusedResources();
	/// Restore the task that periodically free the resources, invalidating some selftests
	static void RestorePeriodicFreeUnusedResources();
	// Do the operations necessary to have the file deleted soon
	static void ScheduleForDelete(URL url);
#ifdef DISK_CACHE_SUPPORT
	/// Save the state of the index
	static OP_STATUS WriteIndex(URL_CONTEXT_ID ctx);
#endif // DISK_CACHE_SUPPORT
	// Force the cache size to 0, and delete all the URLs that qualify, then restore the cache size
	static void DeleteEverythingPossible(URL_CONTEXT_ID ctx, BOOL write_index);
	/// Instruct some asserts to be forgiving, because the test is supposed to trigger them; bypass==FALSE show no mercy.  :-)
	static void BypassAsserts(URL url, BOOL bypass=TRUE)
	{
		Cache_Storage *cs=(url.GetRep() && url.GetRep()->GetDataStorage()) ? url.GetRep()->GetDataStorage()->GetCacheStorage(): NULL;

		if(cs)
			cs->bypass_asserts=bypass;
	}
	/// Simulate a memory error during the call of Cache_Storage::StoreData()
	static void SimulateStoreError(Download_Storage *cs, BOOL simulate_error=TRUE) { OP_ASSERT(cs); if(cs) cs->debug_simulate_store_error=simulate_error; if(cs->temp_storage) cs->temp_storage->debug_simulate_store_error=simulate_error; }

#ifdef DISK_CACHE_SUPPORT
	/// Return the size of the cache file
	static OpFileLength GetCacheFileLen(URL url);
	/// Return TRUE if the cache is not synchronised with the index
	static BOOL IsCacheToSync(URL_CONTEXT_ID ctx_id);
	/// Return TRUE if the cache is not synchronised with the index (activity.opr is missing)
	static BOOL IsCacheToSync(OpFileFolder folder);
	/// Deletes activity.opr
	static OP_STATUS ForceCacheSync(URL_CONTEXT_ID ctx_id);
	/// Deletes activity.opr
	static OP_STATUS ForceCacheSync(OpFileFolder folder);
#endif // DISK_CACHE_SUPPORT
	/// Create some empty files in a directory
	static OP_STATUS TouchFiles(OpFileFolder folder, int num_files, BOOL simulate_crash=FALSE, const uni_char *sub_folder=NULL);
	/// Return the number of URL in the url_Store of a specific Content_Manager
	static UINT32 CheckURLsInStore(URL_CONTEXT_ID ctx_id);
#if defined DISK_CACHE_SUPPORT && CACHE_CONTAINERS_ENTRIES>0
	/// Return the number of containers that are marked for delete
	static int CheckMarked(URL_CONTEXT_ID ctx_id);
	/// Save all the files (containers could still be in memory... it is better to also call FlushContainers())
	static OP_STATUS SaveFiles(URL_CONTEXT_ID ctx_id, BOOL flush_containers=TRUE, BOOL disable_always_verify=FALSE);
	/// Flush the containers to disk
	static void FlushContainers(URL_CONTEXT_ID ctx_id, BOOL reset=TRUE);
#endif // defined DISK_CACHE_SUPPORT && CACHE_CONTAINERS_ENTRIES>0
	/// Delete urlManager and recreate it, primarily to test that the index is loaded once, and only once.
	static void ReConstructUrlManager() { OP_DELETE(urlManager);	urlManager=NULL; g_url_module.g_url_api_obj->ConstructL(); OP_ASSERT(urlManager); }
	/// Delete the URL and the URLRep in a brutal way... but for test it makes sense
	static void BrutalDelete(URL *rep, BOOL delete_url, Window *window=NULL);
	/// Set the "Deep debug" in a Context manager
	static void SetDeepDebug(URL_CONTEXT_ID ctx_id, BOOL deep);
	/// Declare the usage by 1
	static void DecUsed(URL *url) { if(url && url->GetRep()) url->GetRep()->DecUsed(1); }
	/// Retrieve a URL pointing to a page of the given size; this page is hosted in the "t" test server, in Opera
	static URL GetPageSizeURL(UINT32 size, char *id, URL_CONTEXT_ID context_id, BOOL unique);
	/// Returns the estimation of the size of the URL object
	static OpFileLength GetURLObjectSize(URL &url) { return url.GetRep()->GetURLObjectSize(); }
	/// Set the size of the disk and of the ram cache; this bypass everything and just sets the limits
	static void SetCacheSize(URL_CONTEXT_ID ctx_id, OpFileLength disk_size, OpFileLength ram_size);
	/// Get the partial coverage (assuming availability from 0 and multiple segments)
	static OpFileLength GetPartialCoverage(URL_Rep *rep, BOOL follow_redirect)
	{
		if(follow_redirect && rep)
		{
			URL url_redir=rep->GetAttribute(URL::KMovedToURL);
			
			if(!url_redir.IsEmpty())
				return GetPartialCoverage(url_redir.GetRep(), TRUE);
		}

		if(!rep || !rep->GetDataStorage() || !rep->GetDataStorage()->GetCacheStorage())
			return 0;

		BOOL available=FALSE;
		OpFileLength length;

		rep->GetDataStorage()->GetCacheStorage()->GetPartialCoverage(0, available, length, TRUE);

		if(!available)
			return 0;

		return length;
	}
	
	/// Set KCachePolicy_AlwaysVerify to FALSE, also to redirected URL;
	/// This is convenient for Unite, as the webserver automatically put a "Cache-Control: no-cache" header...
	static void DisableAlwaysVerify(URL url);
	/// For ALL the URLs, set KCachePolicy_AlwaysVerify to FALSE
	/// This is convenient for Unite, as the webserver automatically put a "Cache-Control: no-cache" header...
	static void DisableAlwaysVerifyForAllURLs(URL_CONTEXT_ID ctx_id);

	static void VerifyType(URL &url, CacheType type, OpFileLength expected_len=FILE_LENGTH_NONE)
	{
		URL_Rep *rep=url.GetRep();
		Cache_Storage *cs=(rep && rep->GetDataStorage() && rep->GetDataStorage()->GetCacheStorage())?rep->GetDataStorage()->GetCacheStorage():NULL;

		if(!cs)
		{
			ST_failed("No storage!");

			return;
		}

		if(expected_len!=FILE_LENGTH_NONE)
		{
			OpFileLength len;

			url.GetAttribute(URL::KContentSize, &len);

			if(len==0)
				url.GetAttribute(URL::KContentLoaded, &len);

			if(len!=expected_len)
			{
				ST_failed("Size is %d instead of %d", (int)len, (int)expected_len);

				return;
			}
		}

		if(type==TYPE_EMBEDDED)
		{
			if(!cs->IsEmbedded())
				ST_failed("URL not embedded!");
			if(cs->GetContainerID())
				ST_failed("Embedded URL in a container!");
		}
		else if(type==TYPE_CONTAINER)
		{
			if(cs->IsEmbedded())
				ST_failed("Container URL embedded!");
			if(!cs->GetContainerID())
				ST_failed("URL not in a container!");
		}
		else
		{
			if(cs->IsEmbedded())
				ST_failed("Plain URL embedded!");
			if(cs->GetContainerID())
				ST_failed("Plain URL in a container!");
		}
	}
	/// Returns TRUE is the URL_Rep is illegal
	static BOOL IsURLRepIllegal(URL_Rep *rep) { return rep->name.illegal_original_url.HasContent(); }

	/// Execute freeze tests, to verify that the main context is freeze in a proper way...
	static void FreezeTests();
};


#ifdef DISK_CACHE_SUPPORT
// Manage Multiple contexts, to simplify some operations
class MultiContext
{
private:
	/// Contexts
	OpINT32Vector ctxs;
	/// Folders associated with the contexts
	OpINT32Vector folders;
	/// Used to perform operations on a context
	OpAutoVector<WaitURLs>waits;
	/// Message handler of each context
	OpAutoVector<MessageHandler>handlers;
	/// Preference: PrefsCollectionNetwork::AlwaysCheckNeverExpireGetQueries
	int pref_never_expire;
	/// Preference: PrefsCollectionNetwork::EmptyCacheOnExit
	int pref_empty_on_exit;
	// Preference: PrefsCollectionNetwork::CheckDocModification
	int pref_doc_mod;
	// Preference: PrefsCollectionNetwork::DocExpiry
	int pref_doc_expiry;
	// Preference: PrefsCollectionNetwork::CacheHTTPS
	int pref_cache_https;
	// Preference: PrefsCollectionNetwork::CheckOtherModification
	int pref_other_mod;
	// Preference: PrefsCollectionNetwork::OtherExpiry
	int pref_other_expiry;
	/// Preference: PrefsCollectionJS::IgnoreUnrequestedPopups
	BOOL pref_ignore_popups;
	// Preference: PrefsCollectionNetwork::AlwaysCheckRedirectChanged
	int pref_cache_redir;
	// Preference: PrefsCollectionNetwork::AlwaysCheckRedirectChangedImages
	int pref_cache_redir_images;
	// Preference: PrefsCollectionNetwork::CacheToDisk
	int pref_enable_disk_cache;
	// Preference: PrefsCollectionWebserver::UseOperaAccount
	int pref_opera_account;
	// Preference: PrefsCollectionWebserver::WebserverEnable
	int pref_webserver_enabled;
	// Preference: PrefsCollectionWebserver::WebserverListenToAllNetworks
	int pref_listen_all;
	// PrefsCollectionNetwork::UseWebTurbo
	int pref_web_turbo;
	// PrefsCollectionJS::DelayedScriptExecution
	int prefs_delayed_scripts;

	/// Message handler for the main cache context, "0"
	MessageHandler mh_main;
	/// Waiter for the main cache context, "0"
	WaitURLs wait_main;

	/// Return the index of the associated context (-1 if not found)
	INT32 GetIndex(URL_CONTEXT_ID ctx) { return ctxs.Find((INT32)ctx); }

public:
	MultiContext(BOOL cache_get_queries=TRUE);
	~MultiContext();

	/** Create a context and add it to the arrays
	 * @param ctx will contain the new context id
	 * @param folder_name name of the folder to be created
	 * @param folder_dest will contain the OpFileFolder of the context
	 * @param index if != NULL, will contain the index in the array of contexts
	 */
	OP_STATUS CreateNewContext(URL_CONTEXT_ID &ctx, const uni_char* folder_name, BOOL delete_directory, OpFileFolder *folder_dest=NULL, int *index=NULL);
	/** Create a context and add it to the arrays
		 * @param ctx will contain the new context id
		 * @param folder_dest the folder of the context
		 * @param index if != NULL, will contain the index in the array of contexts
		 */
	OP_STATUS CreateNewContext(URL_CONTEXT_ID &ctx, OpFileFolder folder_dest, BOOL delete_directory, int *index=NULL);

	/**
		Remove the internal references to the context. Just to be clear, RemoveContext() will not be called, so the selftest itself is responsible for this.
		@param ctx_to_forget Context ID to remove from the list
		@param delete_dir TRUE to delete the associated directory
		@param clean_handlers TRUE to delete the handlers (they will always be removed)
	*/
	BOOL ForgetContext(URL_CONTEXT_ID ctx_to_forget, BOOL delete_dir, BOOL clean_handlers);
	
	/** Add a context to the arrays
		 * @param ctx will contain the new context id
		 * @param folder_dest the folder of the context
		 * @param index if != NULL, will contain the index in the array of contexts
		 */
	OP_STATUS AddContext(Context_Manager *man, int *index=NULL, Window *window=NULL);

	/// Get the context associated with the index. Be careful...
	URL_CONTEXT_ID GetContext(int num) { return (URL_CONTEXT_ID) ctxs.Get(num); }
	/// Get the folder associated with the index. Be careful...
	OpFileFolder GetFolder(int num) { return (OpFileFolder) folders.Get(num); }
	/// Asynchronously wait for the context to reach the expected number of files
	void WaitForFiles(URL_CONTEXT_ID ctx, int num_files, BOOL check_immediately);
	/// Return the number of files in the index
	int CheckIndex(URL_CONTEXT_ID ctx, BOOL save_index=TRUE);

	/// Remove the contexts and delete the folders
	void RemoveFoldersAndContexts();
	/// Set if popups have to be ignore
	void PrefIgnorePopups(BOOL ignore);
	/// Execute EmptyDCache on a context (it also works for the main cache context, "0"), waiting for the files to be deleted. It requires the test to be asynchronous
	void EmptyDCache(URL_CONTEXT_ID ctx, BOOL delete_app_cache = FALSE);
};
#endif // DISK_CACHE_SUPPORT
#endif // SELFTEST

#endif //CACHE_SELFTEST_H
