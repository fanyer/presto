#ifndef MacOpSystemInfo_H
#define MacOpSystemInfo_H

#include "modules/pi/OpSystemInfo.h"
#include "modules/hardcore/keys/opkeys.h"

#ifdef DPI_CAP_DESKTOP_SYSTEMINFO
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#endif

#if defined(POSIX_SERIALIZE_FILENAME) && defined(POSIX_OK_FILE)
#include "platforms/posix/posix_system_info.h"
#endif

#ifndef MACGOGI
#include "platforms/mac/model/MacFileHandlerCache.h"
#endif // MACGOGI

class MacOpSystemInfo :
#if defined(POSIX_SERIALIZE_FILENAME) && defined(POSIX_OK_FILE)
	public PosixSystemInfo 
#elif defined(DPI_CAP_DESKTOP_SYSTEMINFO)
	public DesktopOpSystemInfo
#else
	public OpSystemInfo
#endif // DPI_CAP_DESKTOP_SYSTEMINFO
{
public:
	MacOpSystemInfo();
	virtual ~MacOpSystemInfo() {}

#ifndef POSIX_OK_CLOCK
	virtual void   GetWallClock( unsigned long& seconds, unsigned long& milliseconds );
		/**< Returns the current local time as seconds and milliseconds since some epoch. */

	virtual double GetWallClockResolution();
		/**< Returns the resolution of the real-time clock, as a fraction of a second */


	/** Returns the current time in milliseconds since 1970-01-01 00:00 GMT */
	virtual double GetTimeUTC();

	/** Get the time something has been running (expressed in milliseconds).
	"Something" doesn't matter what it is as long as within one Opera session,
	the value returned by this method are never less than the previous value returned.

	This method is eg used by schedulers and the like within Opera that don't want to
	have to deal with sudden time changes (common in mobile devices).

	@return runtime in ms
	*/
	virtual double GetRuntimeMS();

	/** Get the time elapsed since some reference point, but with no fixed guarantees that
	 * the reference point cannot change during an Opera session. Appropriate when you
	 * want to capture length of intervals by comparing the (absolute) difference between
	 * successive calls to GetRuntimeTickMS(), but the absolute precision of the result
	 * for individual calls isn't paramount -- the difference is.
	 *
	 * @return The number of milliseconds since some reference point. No guarantees
	 * that this will be monotonically increasing throughout an Opera session.
	 */
	virtual unsigned int GetRuntimeTickMS();
#endif // POSIX_OK_CLOCK

#ifndef POSIX_OK_TIME_ZONE
	/** Returns the number of seconds west of GMT. Eg for Norway, return -3600 when it's normal time,
		-7200 when it's daylight savings time. */
	virtual long GetTimezone();

	virtual double DaylightSavingsTimeAdjustmentMS(double t);
#endif // POSIX_OK_TIME_ZONE

	/**
	 * Returns the size of the physical memory (not swap disk etc) in MB
	 */
	virtual UINT32 GetPhysicalMemorySizeMB();

	/** Retrieve system proxy settings for the given protocol. This is used as
	  * the default setting and can be overridden using the preferences.
	  *
	  * If your platform do not support system proxy settings, set enabled to
	  * FALSE and clear the string value. This method should only leave for
	  * fatal situations, otherwise it should just set enabled to FALSE.
	  *
	  * @param protocol String representation of protocol name.
	  *                 ("http", "https", "ftp", "gopher" or "wais")
	  * @param enabled (output) Stores a boolean value telling if this proxy
	  *                server is enabled or not.
	  * @param proxyserver (output) Stores a string value representing the
	  *                    address to the proxy server, in the form
	  *                    "host:port".
 	  */
	virtual void GetProxySettingsL(const uni_char *protocol, int &enabled,
	                               OpString &proxyserver);

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** Retrieve the default automatic proxy URL for the system. This is used
	  * as the default setting and can be overridden using the preferences.
	  *
	  * If your platform does not support system proxy settings, simply leave
	  * the string as-is. This method should only leave for fatal situations.
	  *
	  * @param url (output)
	  *   Pointer to string to put the URL to the default automatix proxy
	  *   configuration (PAC) file for the system.
	  * @param enabled (output)
	  *   Stores a boolean value telling if this proxy configuration is
	  *   enabled or not.
	  */
	virtual void GetAutoProxyURLL(OpString *url, BOOL *enabled);
#endif

	/** Retrieve the default proxy exception list for the system. This is used
	  * as the default setting and can be overridden using the preferences.
	  *
	  * If your platform does not support system proxy settings, simply leave
	  * the input as-is. This method should only leave for fatal situations.
	  *
	  * @param exceptions (output)
	  *   Pointer to string to put the list of proxy exceptions
	  *   (comma-delimited) for the system.
	  * @param enabled (output)
	  *   Stores a boolean value telling if this exception list is
	  *   enabled or not.
	  */
	virtual void GetProxyExceptionsL(OpString *exceptions, BOOL *enabled);


#ifdef _PLUGIN_SUPPORT_
	/** Fix-up the plugin path as read from the preferences. This method
	  * receives the plugin path from the preferences and updates it to
	  * include any entries that it must include. If you are staisfied
	  * with the default value, you must still copy it over to the output
	  * string.
	  *
	  * If dfpath is empty (i.e not set in the prefences) a proper default
	  * path should be output.
	  *
	  * The path is separated by the standard path separator for the system,
	  * semi-colon for Windows and colon for the Unix flavours.
	  *
	  * @param dfpath The plugin path read from the preferences.
	  * @param newpath Output: The massaged path to remember.
	  */
	virtual void GetPluginPathL(const OpStringC &dfpath, OpString &newpath);

#if defined(PI_PLUGIN_DETECT) && !defined(NS4P_COMPONENT_PLUGINS)
    OP_STATUS DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener);
#endif // defined(PI_PLUGIN_DETECT) && !defined(NS4P_COMPONENT_PLUGINS)
#endif // _PLUGIN_SUPPORT_

#if 0
#ifdef _APPLET_2_EMBED_
	/** Retrieve the default Java class path for the system.
	  */
	virtual void GetDefaultJavaClassPathL(OpString &target);

	/** Retrieve the default Java policy filename for the system.
	  */
	virtual void GetDefaultJavaPolicyFilenameL(OpString &target);
#endif // _APPLET_2_EMBED_
#endif

#ifdef USE_OP_MAIN_THREAD
	/**
	 * Are we currently running in Opera's main thread?
	 */
	virtual BOOL IsInMainThread();
#endif

#ifdef OPSYSTEMINFO_CPU_FEATURES
	virtual unsigned int GetCPUFeatures();
#endif

	/**
	 * Retrieve the MIME name for the system's default encoding.
	 * The pointer must be valid as long as the OpSystemInfo object
	 * is alive.
	 */
	virtual const char *GetSystemEncodingL();

	/**
	 * Expand system variables from in to out
	 */
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out);

	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, uni_char* out, INT32 out_max_len);

	/**
	 * Get the default text editor for the system
	 */
	virtual const uni_char* GetDefaultTextEditorL();

#ifdef OPSYSTEMINFO_GETFILETYPENAME
	/**
	 * Get a descriptive file type name from a file name. This is used in
	 * directory listings to provide the user with a file type. If the
	 * type of the file is unknown, the implementation should set the the
	 * out parameter to an empty string.
	 *
	 * @param filename Name of the file that we request information from.
	 * @param out String buffer to store the file description in.
	 * @param out_max_len Length of out, in characters.
	 */
	virtual OP_STATUS GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len);
#endif // OPSYSTEMINFO_GETFILETYPENAME

#ifdef QUICK
	/**
	 * Get the default mimetype/extension handler for a file
	 */
	virtual OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler);

	virtual OP_STATUS GetFileHandlers(const OpString& filename,
									  const OpString &content_type,
									  OpVector<OpString>& handlers,
									  OpVector<OpString>& handler_names,
									  OpVector<OpBitmap>& handler_icons,
									  URLType type,
									  UINT32 icon_size = 16);

	/**
	 * Starts up an application displaying the folder of the file.
	 *
	 * @param file_path - the full path to the file
	 *
	 * @return OpStatus::OK if successful
	 */
	virtual OP_STATUS OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files = TRUE);

	/**
	 * Returns TRUE if platform allow running downloaded content
	 *
	 * @return BOOL
	 */
	virtual BOOL AllowExecuteDownloadedContent();

	virtual OP_STATUS GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler);

	/**
	 * Is the file passed in an executable
	 */
	virtual BOOL GetIsExecutable(OpString* filename);

	/**
	 * Return an OpString containing illegal chars used in a filename for your platform
	 */
	virtual OP_STATUS GetIllegalFilenameCharacters(OpString* illegalchars);

	/**
	 * Cleans a path for illegal characters. Any spaces in front and in the end will
	 * be removed as well.
	 *
	 * @param replace Replace illegal character with a valid character if TRUE. Otherwise
	 *        remove the illegal character.
	 *
	 * @return TRUE if the returned string is non-empty
	 */
	virtual BOOL RemoveIllegalPathCharacters(OpString& path, BOOL replace);

	virtual BOOL RemoveIllegalFilenameCharacters(OpString& path, BOOL replace);

	virtual OpString GetLanguageFolder(const OpStringC &lang_code);

	/**
	 * Return an OpString containing the newline string i textfiles for your platform
	 */
	virtual OP_STATUS GetNewlineString(OpString* newline_string);

	/**
	 * Return the current down state of the modifier keys. It is a mask using the
	 * SHIFTKEY_NONE, SHIFTKEY_CTRL, SHIFTKEY_SHIFT, SHIFTKEY_ALT, SHIFTKEY_META
	 * and SHIFTKEY_SUPER
	 */
	virtual INT32 GetShiftKeyState();
#endif // QUICK

#ifdef EMBROWSER_SUPPORT
	/**
	 * If use_custom is true, use new_color as highlight color. Otherwise return to getting this color from the system.
	 */
	virtual OP_STATUS SetCustomHighlightColor(BOOL use_custom, COLORREF new_color);
#endif //EMBROWSER_SUPPORT

#ifndef NO_EXTERNAL_APPLICATIONS
# ifdef DU_REMOTE_URL_HANDLER
	virtual OP_STATUS PlatformExecuteApplication(const uni_char* application,
						     const uni_char* args,
						     BOOL silent_errors);
# else
	virtual OP_STATUS ExecuteApplication(const uni_char* application,
					     const uni_char* args,
					     BOOL silent_errors);
# endif
#endif // !NO_EXTERNAL_APPLICATIONS

#ifdef EXTERNAL_APPLICATIONS_SUPPORT
	/** @override */
	virtual OP_STATUS OpenURLInExternalApp(const URL& url);
#endif

#if defined(_CHECK_SERIAL_) || defined(M2_SUPPORT) || defined(DPI_CAP_DESKTOP_SYSTEMINFO)
	virtual OP_STATUS GetSystemIp(OpString& ip);
#endif // _CHECK_SERIAL_ || M2_SUPPORT || DPI_CAP_DESKTOP_SYSTEMINFO

	virtual const char *GetOSStr(UA_BaseStringId ua);
	virtual const uni_char *GetPlatformStr();

#ifndef POSIX_OK_NATIVE
	virtual OP_STATUS OpFileLengthToString(OpFileLength length, OpString8* result);
	virtual OP_STATUS StringToOpFileLength(const char* length, OpFileLength* result);
#endif

#ifdef SYNCHRONOUS_HOST_RESOLVING
	virtual BOOL HasSyncLookup();
#endif // SYNCHRONOUS_HOST_RESOLVING

	OP_STATUS GetUserLanguages(OpString *result);

	void GetUserCountry(uni_char result[3]);

#ifdef GUID_GENERATE_SUPPORT
	OP_STATUS GenerateGuid(OpGuid &guid);
#endif // GUID_GENERATE_SUPPORT

#ifdef DPI_CAP_DESKTOP_SYSTEMINFO
	virtual int GetDoubleClickInterval();
	virtual OP_STATUS GetFileTypeInfo(const OpStringC& filename,
									  const OpStringC& content_type,
									  OpString & content_type_name,
									  OpBitmap *& content_type_bitmap,
									  UINT32 content_type_bitmap_size  = 16);
#endif // DPI_CAP_DESKTOP_SYSTEMINFO
#ifdef OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
	/** Return a pattern containing %d for generating unique filenames for
	 * download and ini-files typically " (%d)" on windows, "_%d" on unix
	 */
	virtual const uni_char * GetUniqueFileNamePattern() { return UNI_L("_%d"); }
#endif
#if !defined(POSIX_OK_PATH)
# ifdef PI_CAP_PATHSEQUAL
	OP_STATUS PathsEqual(const uni_char* p1, const uni_char* p2, BOOL* equal);
# endif
#endif
#if defined(POSIX_SERIALIZE_FILENAME) && defined(POSIX_OK_FILE)
	virtual uni_char* SerializeFileName(const uni_char *path);
#endif // POSIX_SERIALIZE_FILENAME

	virtual void ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler);
	virtual BOOL HasSystemTray();
	
#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
	virtual BOOL ShallWeTryToSetOperaAsDefaultBrowser();
	virtual OP_STATUS SetAsDefaultBrowser();
#endif

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/** Tells wether a screen reader currently is running
	 *
	 * @return TRUE if we know a screen reader is running or has been running
	 * FALSE otherwise.
	 */
	virtual BOOL IsScreenReaderRunning() { return m_screen_reader_running; }
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef OPSYSTEMINFO_GETPROCESSTIME
	virtual OP_STATUS GetProcessTime(double *time) { return OpStatus::ERR_NOT_SUPPORTED; }
#endif // OPSYSTEMINFO_GETPROCESSTIME

#ifdef OPSYSTEMINFO_GETBINARYPATH
	virtual OP_STATUS GetBinaryPath(OpString *path);
#endif // OPSYSTEMINFO_GETBINARYPATH


#ifdef PI_POWER_STATUS
	virtual BOOL IsPowerConnected();
	virtual BYTE GetBatteryCharge();
	virtual BOOL IsLowPowerState();
#endif // PI_POWER_STATUS

	//utility:
	void SetEventShiftKeyState(BOOL inEvent, ShiftKeyState state);
	BOOL GetEventShiftKeyState(ShiftKeyState &state);

	void SetScreenReaderRunning(BOOL running) { m_screen_reader_running = running; }
	OP_STATUS GetDefaultWindowTitle(OpString& title) { title.Empty(); return OpStatus::OK; }
	BOOL IsForceTabMode();
	virtual BOOL IsSandboxed();
	virtual BOOL SupportsContentSharing();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	virtual INT32 GetMaximumHiresScaleFactor();
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	virtual OpFileFolder GetDefaultSpeeddialPictureFolder();

private:

#if (defined(OPSYSTEMINFO_PATHSEQUAL) || defined(PI_CAP_PATHSEQUAL)) && !defined(POSIX_OK_PATH)
	const char *ResolvePath(const uni_char *path, size_t stop, char *buffer) const;
#endif
	OpString m_default_language_filename;

#ifndef MACGOGI
	OpAutoStringHashTable<MacFileHandlerCache> m_file_handlers;
#endif // MACGOGI
	BOOL m_use_event_shiftkey_state;
	ShiftKeyState m_event_shiftkey_state;
	BOOL m_screen_reader_running;
};

enum SystemLanguage
{
	kSystemLanguageUnknown,
	kSystemLanguageJapanese,
	kSystemLanguageSimplifiedChinese,
	kSystemLanguageTraditionalChinese,
	kSystemLanguageKorean
};

SystemLanguage GetSystemLanguage();

#endif // MacOpSystemInfo_H
