/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOPOPSYSTEMINFO_H
#define DESKTOPOPSYSTEMINFO_H

#include "adjunct/desktop_util/prefs/DesktopPrefsTypes.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

#define g_desktop_op_system_info static_cast<DesktopOpSystemInfo*>(g_op_system_info)

class ExternalDownloadManager;

/**
 * DesktopOpSystemInfo extends the OpSystemInfo porting interface
 * with functionality specific for the desktop projects.
 */
class DesktopOpSystemInfo : public OpSystemInfo
{

public:
	/* Public constant*/	
	typedef enum PLATFORM_IMAGE
	{
		PLATFORM_IMAGE_SHIELD = 0,      
		PLATFORM_IMAGE_LAST,      
	} PLATFORM_IMAGE;

	typedef enum PRODUCT_TYPE
	{
		PRODUCT_TYPE_OPERA = 0,      
		PRODUCT_TYPE_OPERA_NEXT,
		PRODUCT_TYPE_OPERA_LABS
	} PRODUCT_TYPE;

	/** Which browser is set as default in the OS. */
	typedef enum DefaultBrowser
	{
		DEFAULT_BROWSER_UNKNOWN,
		DEFAULT_BROWSER_FIREFOX,
		DEFAULT_BROWSER_IE,
		DEFAULT_BROWSER_OPERA
	} DefaultBrowser;

public:

	/**
	 * Return the time limit in milliseconds that distinguishes a double click
	 * from two consecutive mouse clicks.
	 *
	 * @return int Double click time limit in milliseconds
	 */
	virtual int GetDoubleClickInterval() = 0;

	/**
	 * Get info about the content type of a file - name of contenttype and icon for it.
	 *
	 * @param filename                 - name of file (eg. document.pdf)
	 * @param content_type             - contenttype of file
	 * @param content_type_name        - an OpString where the textual description of mimetype of
	 *                                   file should be placed (eg. "PDF document")
	 * @param content_type_bitmap      - reference to an OpBitmap pointer where the icon should be placed
	 * @param content_type_bitmap_size - the required size of the icon
	 * @return OpStatus::OK if filetype info was found
	 */
	virtual OP_STATUS GetFileTypeInfo(const OpStringC& filename,
									  const OpStringC& content_type,
									  OpString & content_type_name,
									  OpBitmap *& content_type_bitmap,
									  UINT32 content_type_bitmap_size  = 16) = 0;

	/**
	 * Get a list of handlers for the given content_type/filename
	 *
	 * @param filename      The filename of the file to be handled
	 * @param content_type  The content type of the file to be handled
	 * @param handlers      The vector of commands to start the handlers
	 * @param handler_names The vector of human friendly names for the handlers
	 * @param handler_icons The vector of bitmaps of icons for the handlers
	 * @param type          The type of URL to be opened (eg URL_HTTP or URL_FILE)
	 *
	 * Note 1:
	 * These vectors have to match, that is:
	 * handlers.Get(i) is the command for the application called
	 * handler_names.Get(i) with the icon handler_icons.Get(i)
	 *
	 * Note 2: The icons should be 16*16 pixels
	 */
	virtual OP_STATUS GetFileHandlers(const OpString& filename,
									  const OpString &content_type,
									  OpVector<OpString>& handlers,
									  OpVector<OpString>& handler_names,
									  OpVector<OpBitmap>& handler_icons,
									  URLType type,
									  UINT32 icon_size  = 16) = 0;

	/**
	 * Starts up an application displaying the folder of the file, and highlighting the file specified.
	 *
	 * @param file_path - the full path to the file
	 * @param treat_folders_as_files - if TRUE and file_path is a path to a directory,
	 *					the parent directory is opened, with the highlighting the folder.
	 *					if FALSE, the path to the directory specified is opened.
	 *
	 * @return OpStatus::OK if successful
	 */
	virtual OP_STATUS OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files = TRUE) = 0;

	/**
	 * @return TRUE if platform allows running downloaded content.
	 */
	virtual BOOL AllowExecuteDownloadedContent() = 0;

	/** Return a string containing all characters illegal for use in a filename.
	 *
	 * NOTE: This method is being considered for removal. Please
	 * refrain from using it from core code.
	 */
	virtual OP_STATUS GetIllegalFilenameCharacters(OpString* illegalchars) = 0;

	/** Cleans a path of illegal characters.
	 *
	 * Any spaces in front and in the end will be removed as well.
	 *
	 * NOTE: This method is being considered for removal. Please
	 * refrain from using it from core code.
	 *
	 * @param path (in/out) Path from which to remove illegal characters.
	 * @param replace Replace illegal characters with a valid
	 * character if TRUE. Otherwise remove the illegal character.
	 * @return TRUE if the returned string is non-empty
	 */
	virtual BOOL RemoveIllegalPathCharacters(OpString& path, BOOL replace) = 0;

	/** Cleans a filename for illegal characters. On most platforms these illegal
	 * characters will include things like path seperator characters for example.
	 *
	 * Any spaces in front and in the end will be removed as well.
	 *
	 * @param path (in/out) filename from which to remove illegal characters.
	 * @param replace Replace illegal characters with a valid
	 * character if TRUE. Otherwise remove the illegal character.
	 * @return TRUE if the returned string is non-empty
	 */
	virtual BOOL RemoveIllegalFilenameCharacters(OpString& path, BOOL replace) = 0;

# ifdef EXTERNAL_APPLICATIONS_SUPPORT
#  ifdef DU_REMOTE_URL_HANDLER
	/** Implement the OpSystemInfo::ExecuteApplication method, and thereby intercept the platform
	 * the platform must implement PlatformExecuteApplication
	 */
	OP_STATUS ExecuteApplication(const uni_char* application,
								 const uni_char* args,
								 BOOL  silent_errors = FALSE);

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
	virtual OP_STATUS PlatformExecuteApplication(const uni_char* application,
												 const uni_char* args,
												 BOOL silent_errors) = 0;

	/** Open given single file using application.
	 * Filename will surrounded with quotation marks before passing as
	 * application argument.
	 *
 	 * @param application Application name
	 * @param filename Argument to the application
	 */
	OP_STATUS OpenFileInApplication(const uni_char* application, const uni_char* filename);

#  endif // DU_REMOTE_URL_HANDLER
# endif // EXTERNAL_APPLICATIONS_SUPPORT

# ifdef EXTERNAL_APPLICATIONS_SUPPORT
	/**
	 * Execute a dedicated application that handles this kind of URL.
	 * Usually it's not an HTTP URL, but a different protocol.
	 * For example, if iTunes is installed, the "itms://" protocol is registered in the system.
	 * Another typical example is "skype://".
	 * Usually the URL is passed as the argument to the protocol handler program.
	 *
	 * Note to implementations :
	 * The URL argument is destroyed right after the call.
	 * If you want to store a pointer, you must increase the reference count or make a copy.
	 *
	 * @param url URL to open
	 * @return error code
	 */
	virtual OP_STATUS OpenURLInExternalApp(const URL& url) = 0;
# endif // EXTERNAL_APPLICATIONS_SUPPORT

	/** Compose an email message with an external client, invoked when clicking mailto
	 *
	 * @param to To header for email to be created
	 * @param cc CC header for email to be created
	 * @param bcc BCC header for email to be created
	 * @param subject Subject header for email to be created
	 * @param message Body of the email message to be created
	 * @param raw_address The link that was clicked that triggered this action
	 * @param mailhandler Which mail handler to use to compose the message
	 */
	virtual void ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler) = 0;

	/**
	 * @return Whether there is a system tray available for Opera
	 */
	virtual BOOL HasSystemTray() = 0;

#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
	/** Implement methods to inquire if opera is set as default 'internet' browser
	 * and if not, then a method to set it as the default browser.
	 * FALSE means either opera is default browser, or we either are not, or it cannot be
	 * determined, and the user shall not be asked
	 * TRUE means either that opera is not the default browser or we don't know, but
	 * the user shall be asked if it shall be set as the default browser anyway */
	virtual BOOL ShallWeTryToSetOperaAsDefaultBrowser() = 0;
	virtual OP_STATUS SetAsDefaultBrowser() = 0;
#endif // QUICK_USE_DEFAULT_BROWSER_DIALOG

	/**
	 * Get which browser is set as default in the OS.
	 */
	virtual DefaultBrowser GetDefaultBrowser() { return DEFAULT_BROWSER_UNKNOWN; }

	/**
	 * Get path to bookmarks store of default browser.
	 *
	 * @param default_browser which browser is set as default in the OS
	 * @param location gets path to bookmarks store
	 *
	 * @return
	 *   - OpStatus::OK on success
	 *   - OpStatus::ERR_NOT_SUPPORTED if default_browser is not supported
	 *   - OpStatus::ERR_NO_MEMORY on OOM
	 *   - OpStatus::ERR on other errors
	 */
	virtual OP_STATUS GetDefaultBrowserBookmarkLocation(DefaultBrowser default_browser, OpString& location) { return OpStatus::ERR_NOT_SUPPORTED; }

	/**
	 * Gets the language folder in the format for the system (i.e. Mac: en.lproj, Win/Unix: en)
	 * This is NOT the full path and does not check if the folder exists
	 *
	 * @param  lang_code	Language code for the system (e.g. German = de)
	 * @return folder_name	Build folder name (e.g. de.lproj)
	 */
	virtual OpString GetLanguageFolder(const OpStringC &lang_code) = 0;

	/** Gets the title to use in Opera's top-level browser windows
	  * @param title Title that can be used for Opera windows, eg. "Opera"
	  */
	virtual OP_STATUS GetDefaultWindowTitle(OpString& title) = 0;

	/** Gets available external donwload managers(flashget, IDM etc) to use when downloading files.
	  * Some users prefer to use external tools to accelerate downloading, makes significant sense 
	  * in particular markets, China for one. Platforms could cache the info so that it doesn't need
	  * to search for them everytime. 
	  *
	  * @param download_managers Found available external download managers.  
	  */
	virtual OP_STATUS GetExternalDownloadManager(OpVector<ExternalDownloadManager>& download_managers){return OpStatus::ERR;}

	/** Get the amount of free physical memory in MB. Not used by desktop
	 */
	virtual OP_STATUS GetAvailablePhysicalMemorySizeMB(UINT32* result) { return OpStatus::ERR_NOT_SUPPORTED; }


	/** 
	 * Fetchs an icon.
	 *
	 * @param icon_id (input)	ID of an image/icon.
	 * @param image (output)	Contains image if retrieved successfully.
	 *
	 * @return OK if image is fetched successfully. 
	 */
	virtual OP_STATUS GetImage(PLATFORM_IMAGE icon_id, Image &image) { return OpStatus::ERR_NOT_SUPPORTED; }

	/**
	 * Introduced temporarily as a work around for DSK-337037.
	 * This function should end any Menu created by plugin if it starts a message loop. On
	 * Windows we just call EndMenu indiscriminately to keep it simple. 
	 *
	 * When a plugin is running a nested message loop, any operation that deletes the document
	 * will crash Opera, some of these operations are closing and going back/forward etc.
	 * This should be fixed in core but considering the amount of dupes a quick workaround
	 * in Desktop is worthwhile.
	 *
	 */
	virtual OP_STATUS CancelNestedMessageLoop() { return OpStatus::OK; }


	/**
	 * Get whether the app is in a state where window.open and friends should be forced as tab.
	 * Used in fullscreen and kiosk modes on Mac, where normally all js windows are toplevel windows.
	 */
	virtual BOOL IsForceTabMode() { return FALSE; }

	/**
	 * Get whether the app is in sandbox environment in term of system sandbox (Apple like),
	 * not application sandbox (Chrome like).
	 */
	virtual BOOL IsSandboxed() { return FALSE; }

	/**
	 * Get whether app is capable of content sharing using system api
	 */
	virtual BOOL SupportsContentSharing() { return FALSE; }

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	/**
	 * Return the maximum scale factor in percent that will be used for the pixel scaler when
	 * PIXEL_SCALE_RENDERING_SUPPORT is in use. When multplie screens are in use, it should
	 * return the highest scale factor for all of them.
	 */
	virtual INT32 GetMaximumHiresScaleFactor() { return 100; }
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	/**
	 * Get fingerprint string for the system hardware.
	 * Fingerprints are used by search protection mechanism to prevent
	 * redistribution of search protection data by third-party software.
	 * Implementaion can return empty string if fingerprint can't or shouldn't
	 * be generated.
	 */
	virtual OP_STATUS GetHardwareFingerprint(OpString8& fingerprint)
	{
		fingerprint.Empty();
		return OpStatus::OK;
	}
	
	virtual OpFileFolder GetDefaultSpeeddialPictureFolder() { return OPFILE_PICTURES_FOLDER; }
};

#endif // DESKTOPOPSYSTEMINFO_H
