/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPSYSTEMINFO_H
#define OPSYSTEMINFO_H

#include "modules/util/adt/opvector.h"

class OpFile;

/** @short Provider of information from the platform about system states, properties and capabilities.
 *
 * TODO (in a future core version): The way strings are passed to this API for
 * modification is a mess; somewhere we use OpString*, other times OpString&,
 * and sometimes simply uni_char* with a max-length. Clean this up.
 */
class OpSystemInfo
{
public:
	/** Create an OpSystemInfo object. */
	static OP_STATUS Create(OpSystemInfo** new_systeminfo);

	virtual ~OpSystemInfo() {}

#ifdef OPSYSTEMINFO_GETPHYSMEM
	/** Get the size of the physical memory (not swap disk etc) in MB.
	 *
	 * This information may be unavailable on some systems, in which
	 * case the platform should return zero. If the platform runs into
	 * OOM it should also return zero.
	 */
	virtual UINT32 GetPhysicalMemorySizeMB() = 0;
#endif // OPSYSTEMINFO_GETPHYSMEM

#ifdef OPSYSTEMINFO_GETAVAILMEM
	/** Get the amount of free physical memory in MB.
	 *
	 * This function should provide the amount of physical memory available,
	 * or unused, in the system, i.e. not claimed by any application - the
	 * value a system task-manager would report as unused physical memory.
	 *
	 * This information may be unavailable on some systems.
	 *
	 * @param result (out) - set to amount of available memory.
	 * @return OpStatus::OK if successful or
	 *		OpStatus::ERR_NOT_SUPPORTED - if platform doesnt support it
	 */
	virtual OP_STATUS GetAvailablePhysicalMemorySizeMB(UINT32* result) = 0;
#endif // OPSYSTEMINFO_GETAVAILMEM

#ifdef HAS_ATVEF_SUPPORT
	enum BackChannelStatus {
		BACKCHANNEL_PERMANENT,
		BACKCHANNEL_CONNECTED,
		BACKCHANNEL_DISCONNECTED,
		BACKCHANNEL_UNAVAILABLE
	};

	/** @return the status of the backchannel. If status cannot be obtained, the backchannel is considered unavailable. */
	virtual BackChannelStatus GetBackChannelStatus() = 0;
#endif // HAS_ATVEF_SUPPORT

	/** Retrieve system proxy settings for the given protocol.
	 *
	 * This is used as the default setting and can be overridden using
	 * the preferences. If your platform does not support system proxy
	 * settings, set enabled to FALSE and clear the string value. This
	 * method should only leave for fatal situations, otherwise it
	 * should just set enabled to FALSE.
	 *
	 * @param protocol String representation of protocol name.
	 * ("http", "https", "ftp", "gopher", "wais" or "socks")
	 * @param enabled (output) Stores a boolean value telling if this
	 * proxy server is enabled or not.
	 * @param proxyserver (output) Stores a string value representing
	 * the address to the proxy server, in the form "host:port".
	 */
	virtual void GetProxySettingsL(const uni_char *protocol, int &enabled, OpString &proxyserver) = 0;

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** Retrieve the default automatic proxy URL for the system.
	 *
	 * This is used as the default setting and can be overridden using
	 * the preferences. If your platform does not support system proxy
	 * settings, simply leave the url string unchanged (but set
	 * *enabled to FALSE). This method should only leave for fatal
	 * situations.
	 *
	 * @param url (output) Pointer to string to put the URL to the
	 * default automatix proxy configuration (PAC) file for the
	 * system.
	 * @param enabled (output) Set to TRUE if the platform supports
	 * system proxy settings, else FALSE.
	 */
	virtual void GetAutoProxyURLL(OpString *url, BOOL *enabled) = 0;
	/* TODO: return status instead of leaving, or use return for
	 * enabled !  Likewise for the exceptions list. */
#endif

	/** Retrieve the default proxy exception list for the system.
	 *
	 * This is used as the default setting and can be overridden using
	 * the preferences. If your platform does not support system proxy
	 * settings, simply leave the input as-is. This method should only
	 * leave for fatal situations.
	 *
	 * @param exceptions (output) Pointer to string to put the list of
	 * proxy exceptions (comma-delimited) for the system.
	 * @param enabled (output) Stores a boolean value telling if this
	 * exception list is enabled or not.
	 */
	virtual void GetProxyExceptionsL(OpString *exceptions, BOOL *enabled) = 0;

#ifdef _PLUGIN_SUPPORT_
	/** Fix up the plug-in path as read from the preferences.
	 *
	 * This method receives the plug-in path from the preferences and
	 * updates it to include any entries that it must include. If you
	 * are satisfied with the default value, you must still copy it
	 * over to the output string.
	 *
	 * If dfpath is empty (i.e not set in the prefences), a proper
	 * default path should be output.
	 *
	 * The path is separated by the standard path separator for the
	 * system, semi-colon for Windows and colon for the Unix flavours.
	 *
	 * @param dfpath The plug-in path read from the preferences.
	 * @param newpath Output: The massaged path to remember.
	 */
	virtual void GetPluginPathL(const OpStringC &dfpath, OpString &newpath) = 0;

# ifdef OPSYSTEMINFO_STATIC_PLUGIN
	/** Get the specification of the proxy plug-in from the platform.
	 *
	 * @param plugin_spec to be filled out by the platform. This type is
	 * defined in ns4plugins/src/staticplugin.h, and is not included here, as
	 * that might damage the namespace too much.
	 * @return ERR_NO_MEMORY on OOM, OK otherwise.
	 */
    virtual OP_STATUS GetStaticPlugin(struct StaticPlugin& plugin_spec) = 0;
# endif // OPSYSTEMINFO_STATIC_PLUGIN
#endif // _PLUGIN_SUPPORT_

# ifdef PI_PLUGIN_DETECT
	/** Detect plug-ins, and add them to the plug-in system.
	 *
	 * Call suitable OpPluginDetectionListener methods to register
	 * each plug-in found. Check the OpPluginDetectionListener
	 * documentation for details.
	 *
	 * @param suggested_plugin_paths The plug-in paths where plug-ins probably
	 * reside. This is a string consisting of one or more directories.
	 * Directories are delimited by the standard path separator for the system
	 * (see GetPluginPathL()). Note that these paths are just advisory. The
	 * implementation is free to ignore it, and it may also find plug-ins in
	 * directories not included in this string.
	 * @param listener The listener to report detected plug-ins to
	 * @return ERR_NO_MEMORY on OOM, OK otherwise.
	 */
	virtual OP_STATUS DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener) = 0;
# endif // PI_PLUGIN_DETECT

#ifdef USE_OP_THREAD_TOOLS
	/** Are we currently running in Opera's main thread? */
	virtual BOOL IsInMainThread() = 0;
#endif // USE_OP_THREAD_TOOLS

#ifdef OPSYSTEMINFO_GETPID
	/** Return the current process ID for this instance of Opera. */
	virtual INT32 GetCurrentProcessId() = 0;
#endif // OPSYSTEMINFO_GETPID

#ifdef OPSYSTEMINFO_GETBINARYPATH
	/** Get the full path to the running binary of this process.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY, or
	 *         OpStatus::ERR for other errors.
	 **/
	virtual OP_STATUS GetBinaryPath(OpString *path) = 0;
#endif // OPSYSTEMINFO_GETBINARYPATH

	/** Retrieve the MIME name for the system's default encoding.
	 *
	 * @return The system's default encoding. The pointer returned
	 * must be valid as long as the OpSystemInfo object is
	 * alive. Returning NULL is not allowed.
	 */
	virtual const char *GetSystemEncodingL() = 0;

#ifdef PI_EXPAND_ENV_VARIABLES
	/** Expand system variables (also known as "environment variables").
	 *
	 * The system environment variables may be handled differently on operating systems.
	 * For example Unix systems often substitute, "${VARIABLE}" or "$VARIABLE", while
	 * Windows systems substitute "%VARIABLE%".
	 * Unix systems also tend to expand "~" with the path to the user's home directory.
	 *
	 * @param in (input) A string potentially containing one or more
	 * system variables (e.g. "$TERM -e $EDITOR /tmp/foo")
	 * @param out (output) points on input to an OpString instance which receives
	 * on output the string with all the system variables resolved
	 * (e.g. "xterm -e vi /tmp/foo"). For a return value other than OpStatus::OK
	 * the contents of the string is undefined.
	 * The caller may reserve memory for the string in advance using Reserve(),
	 * so the implementation should empty any existing data in the the string
	 * by calling out->Delete(0) instead of out->Empty().
	 *
	 * @return OK if expanding succeeded, ERR_NO_MEMORY on OOM, ERR if other
	 * errors occurred, such as parse errors.
	 */
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out) = 0;
#endif // PI_EXPAND_ENV_VARIABLES

#ifdef OPSYSTEMINFO_GETFILETYPENAME
	/** Get a descriptive file type name from a file name.
	 *
	 * This is used in directory listings to provide the user with a
	 * file type. If the type of the file is unknown, the
	 * implementation should set the the out parameter to an empty
	 * string.
	 *
	 * @param filename Name of the file that we request information from.
	 * @param out (output) Pre-allocated buffer which will be set to a file
	 * description. If OpStatus::OK is returned, this will be a NUL-terminated
	 * string. The string may be empty. For any other return values, the
	 * contents of this buffer will be undefined.
	 * @param out_max_len 'out' buffer capacity - maximum number of bytes to
	 * write, including NUL terminator
	 * @return OK, ERR or ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len) = 0;
#endif // OPSYSTEMINFO_GETFILETYPENAME

#ifdef OPSYSTEMINFO_GETPROTHANDLER
	/** Get the default handler for the protocol in the specified URI. */
	virtual OP_STATUS GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler) = 0;
#endif // OPSYSTEMINFO_

#ifdef OPSYSTEMINFO_FILEUTILS
	/** Get the default mimetype/extension handler for a file.
	 *
	 * If content type is not available, this string is empty
	 */
	virtual OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler) = 0;

	/** Is the specified file executable?
	 *
	 * @param filename A file name. This may be a full absolute path, a path
	 * relative to the current working directory, or just a file name. If it is
	 * just a file name, the PATH environment variable will be searched (if
	 * such a thing exists on the system).
	 * @return TRUE if the file is executable, FALSE otherwise
	 */
	virtual BOOL GetIsExecutable(OpString* filename) = 0;
#endif // OPSYSTEMINFO_FILEUTILS

#ifdef OPSYSTEMINFO_SHIFTSTATE
	/** Return the current down state of the modifier keys.
	 *
	 * It is a mask using the SHIFTKEY_NONE, SHIFTKEY_CTRL,
	 * SHIFTKEY_SHIFT, SHIFTKEY_ALT, SHIFTKEY_META and SHIFTKEY_SUPER
	 */
	virtual INT32 GetShiftKeyState() = 0;
#endif // OPSYSTEMINFO_SHIFTSTATE

#ifdef EXTERNAL_APPLICATIONS_SUPPORT
	/** Get the default text editor for the system.
	 *
	 * @return The default text editor, if any, NULL otherwise
	 */
	virtual const uni_char* GetDefaultTextEditorL() = 0;

	/** Execute an external application.
	 *
	 * @param application Application name
	 * @param args Arguments to the application
	 * @param silent_errors If TRUE, the platform should not show any UI to
	 *        the user in the event of errors; caller shall handle all errors
	 *        and a platform dialog would only confuse the user. When FALSE,
	 *        the platform may show a UI on errors but need not.
	 *
	 * @retval OK
	 * @retval ERR_NO_MEMORY
	 * @retval ERR
	 */
	virtual OP_STATUS ExecuteApplication(const uni_char* application,
										 const uni_char* args,
										 BOOL silent_errors = FALSE) = 0;
#endif // EXTERNAL_APPLICATIONS_SUPPORT

#ifdef OPSYSTEMINFO_GETSYSTEMIP
	/** Return one of the local host's IP addresses.
	 *
	 * If there is more than one, which one to return is undefined.
	 *
	 * @param ip The resulting IP address as a string.
	 */
	virtual OP_STATUS GetSystemIp(OpString& ip) = 0;
#endif // OPSYSTEMINFO_GETSYSTEMIP

	/** Retrieve a string describing the platform.
	 *
	 * This is used in the DOM code.
	 *
	 * @return A platform identifier (e.g. "Linux"). May not be NULL.
	 */
	virtual const uni_char *GetPlatformStr() = 0;

#ifdef OPSYSTEMINFO_DEVICESTRINGS
	/** Gets the device firmware name and version.
	 *
	 * @param[out] firmware Set to the firmware name and version string.
	 *
	 * @return any relevant value of OpStatus
	 */
	virtual OP_STATUS GetDeviceFirmware(OpString* firmware) = 0;

	/** Gets the device manufacturer.
	 *
	 * @param[out] manufacturer Set to the name of device manufacturer.
	 *
	 * @return any relevant value of OpStatus
	 */
	virtual OP_STATUS GetDeviceManufacturer(OpString* manufacturer) = 0;

	/** Gets the device model.
	 *
	 * @param[out] model Set to the name of device model.
	 *
	 * @return any relevant value of OpStatus
	 */
	virtual OP_STATUS GetDeviceModel(OpString* model) = 0;

#endif // OPSYSTEMINFO_DEVICESTRINGS

#ifdef DEVICE_STOCK_UA_SUPPORT
	/** Gets the device's default browser's default UA string.
	 *
	 * @param[out] ua the device's default browser's default UA string
	 * @return any relevant value of OpStatus
	 */
	virtual OP_STATUS GetDeviceStockUA(OpString8* ua) = 0;
#endif // DEVICE_STOCK_UA_SUPPORT

	/** Convert an OpFileLength to its string representation.
	 *
	 * For the other direction, please see StrToOpFileLength in
	 * modules/util/str.h
	 *
	 * @param length Length to be converted
	 * @param result Resulting string. Must not be NULL.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS OpFileLengthToString(OpFileLength length, OpString8* result) = 0;

#ifdef SYNCHRONOUS_HOST_RESOLVING
	/** Return TRUE if the platform can resolve host names synchronously. */
	virtual BOOL HasSyncLookup() = 0;
#endif // SYNCHRONOUS_HOST_RESOLVING

#ifdef PI_HOST_RESOLVER
	/** Get the address of a DNS server.
	 *
	 * @param dns_address (out) Set to the address of the specified DNS
	 * server. To be freed by the caller. Is NULL if 'number' is out of range.
	 * @param number Which DNS server. Systems may have multiple DNS servers
	 * specified, sorted in order of preference. This parameter specifies which
	 * DNS server to get. 0 means the preferred DNS server, 1 means the
	 * fall-back DNS server for the preferred one, and so on.
	 *
	 * @return OK, ERR or ERR_NO_MEMORY. Note that a 'number' parameter out of
	 * range is not an error in itself.
	 */
	virtual OP_STATUS GetDnsAddress(OpSocketAddress** dns_address, int number = 0) = 0;

	/** Returns a list of DNS suffixes.
	 *
	 * The domain name suffixes should be separated by spaces.
	 * An empty string is a legal result.
	 *
	 * @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS GetDnsSuffixes(OpString* result) = 0;

	enum ResolveHostPolicy
	{
		RESOLVE_EXTERNAL_IGNORE, ///< Ignore static addresses, only use DNS
		RESOLVE_EXTERNAL_FIRST, ///< Check the static addresses before trying DNS
		RESOLVE_EXTERNAL_LAST ///< Check the static addresses after DNS
	};

	/** @return the static host address policy. */
	virtual ResolveHostPolicy GetExternalResolvePolicy() = 0;

	/** Resolve external/static host addresses for a given hostname.
	 *
	 * This will find entries typically found in a hosts file, for instance
	 * /etc/hosts on Unix systems, or
	 * %SystemRoot%\\system32\\drivers\\etc\\hosts on Windows systems.
	 *
	 * Without this, the PI_HOST_RESOLVER cannot resolve special hosts,
	 * like "localhost", since it is a pure DNS resolver.
	 *
	 * The resolver use GetExternalResolvePolicy() to determine when and if
	 * this method is called. It should not be called if
	 * GetExternalResolvePolicy() returns RESOLVE_EXTERNAL_IGNORE.
	 *
	 * @param hostname The hostname that needs to be resolved.
	 *
	 * @return Pointer to list of adresses, or NULL if out of memory. An empty
	 * list means that the hostname could not be resolved. The list must be
	 * deleted by the caller.
	 */
	virtual OpAutoVector<OpSocketAddress> *ResolveExternalHost(const uni_char* hostname) = 0;
#endif // PI_HOST_RESOLVER

#ifdef SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
	/** List the available drive letters.
	 *
	 * Returns a string listing all the available drive letters in the
	 * system. Each character in the string corresponds to an
	 * available and addressable drive letter. The drive letters
	 * should be sorted alphabetically.
	 *
	 * @param result The list of available drive letters. Must not be NULL.
	 * @return Status of the operation.
	 */
	virtual OP_STATUS GetDriveLetters(OpString *result) = 0;

	/** Get the current drive letter.
	 *
	 * Returns the letter for the currently active drive on the
	 * system, or NULL if there is no such drive.
	 */
	virtual uni_char GetCurrentDriveLetter() = 0;
#endif // SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES

	/** Get a list of prioritized languages.
	 *
	 * Return a comma-separated list of prioritized languages in RFC
	 * 3066/3282 format (language_REGION,language,language_REGION,
	 * etc.). This list should correspond to the user settings for the
	 * operating system. It must not contain quality ("q") values.
	 *
	 * If the language list contains a region-specific language, it
	 * should also include the generic language without the region
	 * specifier, i.e "nb_NO" should be followed by "nb". If there are
	 * several region-specific languages, they should all be listed
	 * before the generic one.
	 *
	 * This information is typically available in the international
	 * settings in the operating system, for example a GNU system
	 * would use the LANGUAGE, LC_ALL, LC_MESSAGES and LANG variables
	 * to construct this value, on MacOS this is the list configured
	 * in the System preferences and in Windows, the language is
	 * configured in the Control Panel.
	 *
	 * The value does not necessarily correspond to the UI language in
	 * Opera.  This value is used as a default value for the
	 * Accept-Language header, which controls the language of web
	 * pages.
	 *
	 * If no such information is available, the implementation should
	 * simply return the empty string.
	 *
	 * Sample return value: "nb_NO,nb,en_US,en_GB,en"
	 */
	virtual OP_STATUS GetUserLanguages(OpString *result) = 0;

	/** Get the country name.
	 *
	 * Return the ISO 3166 name of the user's country. This value should
	 * correspond to the user settings for the operating system.
	 *
	 * The value does not necessarily correspond to the UI language in
	 * Opera or the UI language in the operating system, it is perfectly
	 * reasonable to use an English version in a non-English speaking
	 * country.
	 *
	 * If no such information is available, the implementation should simply
	 * return the empty string (i.e set the first character in result to 0).
	 *
	 * There is no error handling in the form of eg returning an OP_STATUS;
	 * if implementations can run into trouble the necessary information should
	 * be preallocated.
	 *
	 * The output buffer is always three characters long, two letters for
	 * the ISO 3166 country code, and a trailing NUL character.
	 *
	 * Sample return values: "se", "no" or "US"
	 */
	virtual void GetUserCountry(uni_char result[3]) = 0;

#ifdef GUID_GENERATE_SUPPORT
	/** Generate a GUID (Globally Unique Identifier).
	 *
	 * @param guid (output) Set to the GUID generated
	 */
	virtual OP_STATUS GenerateGuid(OpGuid &guid) = 0;
#endif // GUID_GENERATE_SUPPORT

#ifdef OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
	/** Get a preferred pattern to append to a filename to make it unique.
	 *
	 * This is used to resolve filename collisions, when a desired filename is
	 * already in use on the filesystem.
	 *
	 * @return A string pattern to append to filenames to make them
	 * unique. This string must contain '%d', which the API user typically will
	 * replace with a number, and then append the string to the desired
	 * filename, so that it becomes unique. This string pattern is typically
	 * " (%d)" on Windows and "_%d" on UNIX.
	 */
	virtual const uni_char* GetUniqueFileNamePattern() = 0;
#endif // OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN

	/** Check if two (file or directory) paths are equal.
	 *
	 * There are typically several ways of expressing the same file or
	 * directory on a system. This method will check if the two strings
	 * supplied identify the same file or directory. The paths supplied must
	 * exist in the filesystem, or OpStatus::ERR_FILE_NOT_FOUND is
	 * returned. The paths need not be absolute. The two paths are defined as
	 * equal if OpLowLevelFile objects created for each of them would operate
	 * on the same file. This means that UNIX-style symbolic links are resolved
	 * before comparison. The same goes for Windows network shares vs. drive
	 * letters.
	 *
	 * Example for Windows: "C:\FoO//Bar" should match "c:\Dummy\..\foo\BAR".
	 *
	 * @param p1 The path to compare with p2. May not be NULL.
	 * @param p2 The path to compare with p1. May not be NULL.
	 * @param[out] equal Set to TRUE if the paths are equal, FALSE
	 * otherwise. Value is undefined unless OpStatus::OK is returned.
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR_NO_ACCESS if read or
	 * search permission was denied, ERR_FILE_NOT_FOUND if either of the
	 * supplied parameters did not point to an entry in the filesystem, ERR if
	 * either of the supplied parameters contains an invalid path (invalid, as
	 * in e.g. "invalid tokens", but not "path does not exist") and for other
	 * errors.
	 */
	virtual OP_STATUS PathsEqual(const uni_char* p1, const uni_char* p2, BOOL* equal) = 0;

#ifdef OPSYSTEMINFO_GETSUBSCRIBERID
	/** Get the unique subscriber ID if applicable.
	 *
	 * In some cases, for some products, a subscriber ID is to be
	 * supplied in HTTP request headers. The subscriber ID is a NUL
	 * terminated string that uniquely identifies the network
	 * subscriber. This ID is typically used by a proxy server to
	 * determine whether the request should be processed or not.
	 *
	 * @return The subscriber ID, if currently applicable. NULL is
	 * returned if no subscriber ID is to be used.
	 */
	virtual const char* GetSubscriberID() = 0;
#endif // OPSYSTEMINFO_GETSUBSCRIBERID

#ifdef OPSYSTEMINFO_GETDRMVERSION
	/** Get the DRM version supported.
	 *
	 * Retrieves a string containing the version of the currently
	 * supported DRM version on the system. This will be on the form
	 * of MAJOR.MINOR (e.g. "2.0").
	 *
	 * @return string contatining the DRM version supported.
	 */
	virtual const char* GetDRMVersion() = 0;
#endif // OPSYSTEMINFO_GETDRMVERSION

#ifdef OPSYSTEMINFO_GETAPPID
	/** Return a string containing the current application ID for this instance of Opera. */
	virtual const char* GetAppId() = 0;
#endif // OPSYSTEMINFO_GETAPPID

#ifdef SYSTEMINFO_ISLOCALFILE
	/** Check whether a string is a path to local disk file or not.
	 *
	 * Paths to local files look differently, depending on the system. This
	 * method is a way to determine whether some string could be a local file
	 * path or not. Note that this method does not check if the file identified
	 * by such a path exists or not; it only checks whether or not the string
	 * _could_ identify a path to a local file. For example, on a UNIX file
	 * system, the path "/dev/null/there/is/nothing/here" should return TRUE.
	 *
	 * @param str The string to check
	 * @return TRUE if the given string may be used as a path to local file,
	 * FALSE otherwise.
	 */
	virtual BOOL IsLocalFile(const uni_char *str) = 0;
#endif // SYSTEMINFO_ISLOCALFILE

#ifdef OPSYSTEMINFO_GETSOUNDVOLUME
	/** Check current sound volume.
	 *
	 * Provide current volume of the Opera process. Depending on platform
	 * it may either be the system volume setting or per-process setting.
	 *
	 * @return volume in range 0.0 (sound off) to 1.0
	 */
	virtual double GetSoundVolumeL() = 0;
#endif // OPSYSTEMINFO_GETSOUNDVOLUME

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	/** Test whether a screen reader is running.
	 *
	 * It's not always clear, so this is more of a "most likely" test; it may be
	 * based on clues from the recent past, divining the best information at our
	 * disposal, poor though that may be.
	 *
	 * @return TRUE if a screen reader is probably available, else FALSE.
	 */
	virtual BOOL IsScreenReaderRunning() = 0;
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef OPSYSTEMINFO_SETRINGTONE

	/** Callback for OpSystemInfo::SetRingtone method */
	class SetRingtoneCallback
	{
	public:
		virtual ~SetRingtoneCallback(){};
		/** called when OpSystemInfo::SetRingtone method finishes
		 *
		 * @param status OpStatus::OK or error code in case of failure
		 * @param file_was_moved - set to true by platform if it performed a file move.
		 */
		virtual void OnFinished(OP_STATUS status, BOOL file_was_moved) = 0;
	};

	/** Sets the ringtone of device to a file at a given path. This file might be a temporary
	 *  one which is indicated by is_temporary_file flag, in which case the implementation SHOULD
	 *  move it to a default system location for ringtones. After the platform implementation of this
	 *  method has finished the operation, it MUST call OpSystemInfo::SetRingtoneCallback::OnFinished().
	 *
	 * @param path Path to the ringtone file.
	 * @param callback A callback object that must be called by the implementation of this
	 *             method when the operation is finished. Thus core is notified about success or failure
	 *             and can release the associated resources.
	 * @param is_temporary_file Flag indicating if the file is temoporatry
	 * @param addressbookitem_id Optional id of the contact of the contact to which a ringtone is set.
	 *             The id is the same as the one used in address book PI - see OpAddressBook.
	 *             If this is NULL it means that it is default ringtone.
	 */
	virtual void SetRingtone(const uni_char* path, SetRingtoneCallback* callback, BOOL is_temporary_file, const uni_char* addressbookitem_id) = 0;

	/** Enumerated types of ringtones */
	enum RingtoneType
	{
		RINGTONE_CALL, //< ringtone used for normal calls
		RINGTONE_MSG   //< ringtone used for text messages
	};

	/** Sets the ringtone volume of device
	 *
	 * @param ringtone_type type of a ringtone to set
	 * @param volume volume of the ringtone. Valid range is 0.0 - 1.0. 0 means off , 1.0 means max volume
	 */
	virtual OP_STATUS SetRingtoneVolume(RingtoneType ringtone_type, double volume) = 0;

	/** Gets the ringtone volume of device
	 *
	 * @param ringtone_type type of a ringtone to set
	 * @param volume set to volume of the ringtone. Valid range is 0.0 - 1.0. 0 means off , 1.0 means max volume
	 * @return OpStatus::OK if the ringtone volume was set successfully. An error status should be returned in case of any other error
	 */
	virtual OP_STATUS GetRingtoneVolume(RingtoneType ringtone_type, double* volume) = 0;

	/** Enables/Disables vibration
	 *
	 * @param vibration TRUE - means enable vibration, FALSE means disable vibration
	 * @return OpStatus::OK if the vibration setting was set successfully. An error
	 *         status should be returned in case of any error. Notably if the
	 *         platform doesn't support vibration OpStatus::ERR_NOT_SUPPORTED should be
	 *         returned.
	 */
	virtual OP_STATUS SetVibrationEnabled(BOOL vibration) = 0;

	/** Checks if the vibration alarm is enabled
	 *
	 * @param vibration should be set to TRUE if vibration alarm is enable.
	 * @return OpStatus::OK if the vibration setting was successfully determined. An error
	 *         status should be returned in case of any error. Notably if the
	 *         platform doesn't support vibration OpStatus::ERR_NOT_SUPPORTED should be
	 *         returned.
	 */
	virtual OP_STATUS GetVibrationEnabled(BOOL *vibration) = 0;

	/** Vibrates the device for a number of miliseconds
	 *
	 * @param miliseconds - number of miliseconds to vibrate
	 * @return OpStatus::OK if the vibration was successfully performed. An error
	 *         status should be returned in case of any error. Notably if the
	 *         platform doesn't support vibration OpStatus::ERR_NOT_SUPPORTED should be
	 *         returned.
	 */
	virtual OP_STATUS Vibrate(UINT32 miliseconds) = 0;
#endif // OPSYSTEMINFO_SETRINGTONE

#ifdef OPSYSTEMINFO_GETPROCESSORUTILIZATIONPERCENT
	/** Gets Current utilization of CPU. In case there are many CPUs it
	 *  gets average CPU utilization
	 *
	 * @param utilization_percent set to CPU utilization.
	 * 1.0 means 100% utilization 0. means 0% utilization
	 * @return OpStatus::OK if the procesor utilization was successfully
	 * determined. An error status should be returned in case of any error.
	 */
	virtual OP_STATUS GetProcessorUtilizationPercent(double* utilization_percent) = 0;
#endif // OPSYSTEMINFO_GETPROCESSORUTILIZATIONPERCENT

#ifdef OPSYSTEMINFO_GETPROCESSTIME
	/** Gets the CPU time spent executing the running process.
	 *
	 * The time includes time spent executing instructions of this process,
	 * plus the time spent executing system calls on behalf of this process.
	 *
	 * The time returned is in milliseconds, but the resolution may be lower
	 * or higher (platform dependent).
	 *
	 * @param [out] time The time spent executing the process. Not NULL.
	 * @return OpStatus::OK if the call was successful and the time was stored
	 *         in the specified location; OpStatus::ERR_NOT_SUPPORTED if the
	 *         function is not supported on this platform;
	 *         OpStatus::ERR_NO_ACCESS if access was denied; or
	 *         OpStatus::ERR_NO_MEMORY if we ran out of memory.
	 */
	virtual OP_STATUS GetProcessTime(double *time) = 0;
#endif // OPSYSTEMINFO_GETPROCESSTIME

#ifdef OPSYSTEMINFO_CPU_ARCHITECTURE
	enum CPUArchitecture
	{
		CPU_ARCHITECTURE_UNKNOWN = 0

#ifdef ARCHITECTURE_ARM
		, CPU_ARCHITECTURE_ARMV5
		, CPU_ARCHITECTURE_ARMV6
		, CPU_ARCHITECTURE_ARMV7
#endif // ARCHITECTURE_ARM
	};

	/** Returns the Architecture of the CPU Opera is run on. The version is
     *  is detected at run-time, to enable the use of more advanced CPU
     *  instructions than could be detected at compile-time (for instance,
     *  assembler optimized routines can use ARMv6 instructions if supported on
     *  a device, even if the binary was compiled for ARMv5).
	 *
	 * @return The CPU architecture identifier. If unknown, the function returns
	 *         CPU_ARCHITECTURE_UNKNOWN.
	 */
	virtual CPUArchitecture GetCPUArchitecture() = 0;
#endif // OPSYSTEMINFO_CPU_ARCHITECTURE

#if defined OPSYSTEMINFO_CPU_FEATURES
	enum CPUFeatures
	{
		CPU_FEATURES_NONE = 0

#if defined ARCHITECTURE_ARM
		, CPU_FEATURES_ARM_VFPV2 = (1 << 0)	///< ARM VFPv2 floating point
		, CPU_FEATURES_ARM_VFPV3 = (1 << 1)	///< ARM VFPv3 floating point
		, CPU_FEATURES_ARM_NEON  = (1 << 2)	///< Support for ARM NEON SIMD instructions
#elif defined ARCHITECTURE_IA32
		, CPU_FEATURES_IA32_SSE2   = (1 << 0)
		, CPU_FEATURES_IA32_SSE3   = (1 << 1)
		, CPU_FEATURES_IA32_SSSE3  = (1 << 2)
		, CPU_FEATURES_IA32_SSE4_1 = (1 << 3)
		, CPU_FEATURES_IA32_SSE4_2 = (1 << 4)
#endif
	};

	/** Returns an bit-field of available CPU features.
	 *
	 * @return A bit-wise OR of the CPUFeature flags of features the platform
	 *         knows to be available on the CPU on which this method is queried.
	 *
	 *         If more than one CPU is available only the features available on
	 *         all CPUs are returned.
	 */
	virtual unsigned int GetCPUFeatures() = 0;
#endif // OPSYSTEMINFO_CPU_FEATURES

	// Removed interfaces
	// virtual const char *GetOSStr(UA_BaseStringId ua) = 0; removed 2011-10-07; see OpUAComponentManager
	// virtual BOOL SupportsVFP()) = 0; removed 2012-03-27; deprecated by OPSYSTEMINFO_CPU_FEATURES
};

#endif // __OPSYSTEMINFO_H
