/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPSYSTEMINFO_H
#define WINDOWSOPSYSTEMINFO_H

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_pi/DesktopOpUiInfo.h"
#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"

class WindowsOpSystemInfo: public DesktopOpSystemInfo, public DesktopOpUiInfo
{
public:
	WindowsOpSystemInfo();
	virtual ~WindowsOpSystemInfo();

	virtual UINT32 GetPhysicalMemorySizeMB();

	virtual UINT32 GetVerticalScrollbarWidth();
	virtual UINT32 GetHorizontalScrollbarHeight();
	
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out);

	virtual BOOL RemoveIllegalPathCharacters(OpString& path, BOOL replace);
	virtual BOOL RemoveIllegalFilenameCharacters(OpString& path, BOOL replace);
private:
	BOOL RemoveIllegalCharactersFromPath(OpString& path, BOOL replace, BOOL file);
public:

	virtual UINT32 GetSystemColor(OP_SYSTEM_COLOR color);

	virtual void GetSystemFont(OP_SYSTEM_FONT font, FontAtt &retval);

	virtual void GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default);

    virtual COLORREF    GetUICSSColor(int css_color_value);
    virtual BOOL        GetUICSSFont(int css_font_value, FontAtt &font);

	virtual BOOL		IsMouseRightHanded();

	virtual void GetAutoProxyURLL(OpString *url, BOOL *enabled);
	virtual void GetProxyExceptionsL(OpString *exceptions, BOOL *enabled);

	virtual void GetProxySettingsL(const uni_char *protocol, int &enabled,
	                               OpString &proxyserver);

#ifdef _PLUGIN_SUPPORT_
	virtual void GetPluginPathL(const OpStringC &dfpath, OpString &newpath);
# ifdef PI_PLUGIN_DETECT
	virtual OP_STATUS DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener);
# endif // PI_PLUGIN_DETECT
#endif // _PLUGIN_SUPPORT_

	/** Retrieve letter of first CD-ROM drive in system. If the force CD
	  * option is enabled, only this drive can be accessed.
	  *
	  * @return Letter of CD-ROM, or NUL if none was found.
	  */
	virtual char GetCDLetter();

# ifdef PI_CAP_PATHSEQUAL
	virtual OP_STATUS PathsEqual(const uni_char* p1, const uni_char* p2, BOOL* equal);
#endif // PI_CAP_PATHSEQUAL

# if defined(OPSYSTEMINFO_PATHSEQUAL) || defined(PI_CAP_PATHSEQUAL)
	virtual BOOL PathsEqualL(const uni_char* p1, const uni_char* p2);
#endif //OPSYSTEMINFO_PATHSEQUAL

	virtual BOOL IsInMainThread();

	const char *GetSystemEncodingL();

	const uni_char* GetDefaultTextEditorL();

#ifdef OPSYSTEMINFO_GETFILETYPENAME
	OP_STATUS GetFileTypeName(const uni_char *filename, uni_char *out, size_t out_max_len);
#endif
	OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler);
	OP_STATUS GetFileHandlers(const OpString& filename, const OpString &content_type, OpVector<OpString>& handlers, OpVector<OpString>& handler_names, OpVector<OpBitmap>& handler_icons, URLType type, UINT32 icon_size  = 16);
	OP_STATUS GetFileTypeInfo(const OpStringC& filename, const OpStringC& content_type, OpString & content_type_name,
								OpBitmap *& content_type_bitmap, UINT32 content_type_bitmap_size  = 16);
	OP_STATUS GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler);

	OP_STATUS OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files = TRUE);

	BOOL AllowExecuteDownloadedContent() {return TRUE;}

	// Windows is only using extension, so application is "exe" "bat" "cmd" "com"
	BOOL GetIsExecutable(OpString* filename);

	OP_STATUS GetIllegalFilenameCharacters(OpString* illegalchars);
	OP_STATUS GetNewlineString(OpString* newline_string);

	INT32 GetShiftKeyState();

	INT32 GetCurrentProcessId();
#ifdef EMBROWSER_SUPPORT
	OP_STATUS SetCustomHighlightColor(BOOL use_custom, COLORREF new_color) { 
		m_use_custom_highlight_color = use_custom; m_custom_highlight_color = new_color; return OpStatus::OK;
	}
#endif //EMBROWSER_SUPPORT

#ifndef NO_EXTERNAL_APPLICATIONS
#ifdef DU_REMOTE_URL_HANDLER
	OP_STATUS PlatformExecuteApplication(const uni_char* application, const uni_char* args, BOOL silent_errors);
#else
	OP_STATUS ExecuteApplication(const uni_char* application, const uni_char* args, BOOL silent_errors);
#endif

#ifdef EXTERNAL_APPLICATIONS_SUPPORT
	/** @override */
	virtual OP_STATUS OpenURLInExternalApp(const URL& url);
#endif

	static BOOL AppIsShellExecutable(const uni_char* app);
	static OP_STATUS ProcessAppParameters(const uni_char* appin, const uni_char* filenamein, OpString& application, OpString& parameters);
	/**
	 * Split command in 2 parts : application and parameters.
	 * @param command shell open command string
	 * @param application executable file path
	 * @param parameters application command line arguments
	 * @return error code
	 */
	static OP_STATUS SplitOpenCommand(const uni_char* command, OpString& application, OpString& parameters);
#endif // !NO_EXTERNAL_APPLICATIONS

	OP_STATUS GetSystemIp(OpString& ip);

	int GetDoubleClickInterval() { return GetDoubleClickTime(); }

	const char *GetOSStr(UA_BaseStringId ua);
	const uni_char *GetPlatformStr();

	OP_STATUS OpFileLengthToString(OpFileLength length, OpString8* result);
	OP_STATUS StringToOpFileLength(const char* length, OpFileLength* result);

#ifdef AUTO_UPDATE_SUPPORT
	/** Name of the operating system as will be passed to the auto update 
	* system. It should be of the form as show on the download page
	* i.e. http://www.opera.com/download/index.dml?custom=yes
	*
	* @param os_string (in/out) holds the name of the OS
	* @return OpStatus::OK if successful
	*/
	OP_STATUS GetOSName(OpString &os_string);

	/** Version of the operating system as will be passed to the auto 
	* update system
	*
	* @param os_version (in/out) holds the version of the OS
	* @return OpStatus::OK if successful
	*/
	OP_STATUS GetOSVersion(OpString &os_version);
#endif // AUTO_UPDATE_SUPPORT

#ifdef SYNCHRONOUS_HOST_RESOLVING
	BOOL HasSyncLookup() { OP_ASSERT(FALSE); return FALSE; /* TODO: Implement me! */ }
#endif // SYNCHRONOUS_HOST_RESOLVING


#ifdef PI_HOST_RESOLVER
	OP_STATUS GetDnsAddress(OpSocketAddress** dns_address) {OP_ASSERT(FALSE); return OpStatus::ERR; /* TODO: Implement me! */ }
#endif // PI_HOST_RESOLVER

#ifdef SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
	OP_STATUS GetDriveLetters(OpString *result);
	uni_char GetCurrentDriveLetter();
#endif // SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES

	BOOL IsFullKeyboardAccessActive();

	UINT32 GetCaretWidth();

	virtual BOOL DefaultButtonOnRight() { return FALSE; }

	OP_STATUS GetUserLanguages(OpString *result);
	static OP_STATUS GetSystemFallbackLanguage(OpString *result);
	void GetUserCountry(uni_char result[3]);

	virtual OP_STATUS GenerateGuid(OpGuid &guid);

#ifdef OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN
	virtual const uni_char* GetUniqueFileNamePattern() {return UNI_L(" (%d)");};
#endif //OPSYSTEM_GET_UNIQUE_FILENAME_PATTERN

	// Windows specific functions (not part of OpSystemInfo interface)

	HFONT	GetMenuFont();
	void	OnSettingsChanged();

	DefaultBrowser GetDefaultBrowser();
	OP_STATUS GetDefaultBrowserBookmarkLocation(DefaultBrowser default_browser, OpString& location);

#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
	virtual BOOL ShallWeTryToSetOperaAsDefaultBrowser();
	virtual OP_STATUS SetAsDefaultBrowser();
#endif //QUICK_USE_DEFAULT_BROWSER_DIALOG

	void ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler);

	BOOL HasSystemTray() {return TRUE;}
	
	virtual OpString GetLanguageFolder(const OpStringC &lang_code);

	static OP_STATUS	AddToWindowsFirewallException(const OpStringC &exe_path, const OpStringC &entry_name);

	OP_STATUS GetDefaultWindowTitle(OpString& title);

	OP_STATUS GetExternalDownloadManager(OpVector<ExternalDownloadManager>& download_managers);

#ifdef OPSYSTEMINFO_GETBINARYPATH
	OP_STATUS GetBinaryPath(OpString *path);
#endif // OPSYSTEMINFO_GETBINARYPATH

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	void SetScreenReaderRunning() {m_screenreader_running = true;}
	virtual BOOL IsScreenReaderRunning() {return m_screenreader_running;}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	virtual OP_STATUS GetProcessTime(double *time);

	virtual OP_STATUS GetImage(DesktopOpSystemInfo::PLATFORM_IMAGE platform_image_id, Image &image);

	virtual OP_STATUS CancelNestedMessageLoop();

#ifdef OPSYSTEMINFO_CPU_FEATURES
	virtual unsigned int GetCPUFeatures();
#endif //OPSYSTEMINFO_CPU_FEATURES

	virtual bool IsTouchInputAvailable();
	virtual bool IsTouchUIWanted();

	virtual OP_STATUS GetHardwareFingerprint(OpString8& fingerprint);

private:
	OP_STATUS GetFirefoxBookmarkLocation(OpString& location) const;
	OP_STATUS GetIEBookmarkLocation(OpString& location) const;
	OP_STATUS GetOperaBookmarkLocation(OpString& location) const;
	
	void ParseJavaVersionString(const uni_char* version, int& major, int& minor) const;

	/**
	 * Get system commands for handling given content_type/filename.
	 *
	 * @param type URL_HTTP or URL_FILE
	 * @param filename name of file to be handled
	 * @param content_type type of file to be handled
	 * @param handlers gets commands of content_type/filename handlers
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other errors
	 */
	OP_STATUS GetHandlerStrings(URLType type, const OpString& filename, const OpString& content_type, OpVector<OpString>& handlers);

	/**
	 * Get names and icons for handlers returned by GetHandlersCommands.
	 *
	 * @param handlers handlers returned by GetHandlersCommands
	 * @param icon_size size of icons
	 * @param names gets names for handlers
	 * @param icons gets icons for handlers
	 *
	 * @return OK on success, ERR_NO_MEMORY on OOM, ERR on other errors
	 */
	OP_STATUS GetNamesAndIconsForHandlers(const OpVector<OpString>& handlers, UINT32 icon_size, OpVector<OpString>& names, OpVector<OpBitmap>& icons);

	// Data for GetSystemFont
	BOOL m_first;
	short m_height8, m_height10, m_height12;

	// Data for GetCDLetter
	char m_cd_letter;

	DWORD main_thread_id;
	char *m_system_encoding;
#ifdef EMBROWSER_SUPPORT
	COLORREF m_custom_highlight_color;
	BOOL m_use_custom_highlight_color;
#endif // EMBROWSER_SUPPORT
	OpString m_system_text_editor;

	BOOL	m_screenreader_running;
	
	/** Buffer for GetOSStr() */
	char m_os_ver[128]; /* ARRAY OK 2005-06-07 peter */

	HFONT	m_menu_font;

	OpString8 m_hardware_fingerprint;
	BOOL m_hardware_fingerprint_generated;
};

#endif // WINDOWSOPSYSTEMINFO_H
