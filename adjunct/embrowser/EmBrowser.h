#ifndef EMBROWSER_INTERFACE_H
#define EMBROWSER_INTERFACE_H

#ifndef OUT
#	define OUT
#endif
#ifndef IN
#	define IN
#endif
#ifndef OPT
#	define OPT
#endif
#ifndef IO
#	define IO IN OUT
#endif

#ifdef WIN32
# pragma pack( push, before_embrowser_pack )
# pragma pack(4)
#endif //WIN32

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- */
#define EMBROWSER_MAJOR_VERSION 3
#define EMBROWSER_MINOR_VERSION 0

// -
// ----- TYPEDEFS AND ENUMS -----
// -

/* ---------- */
/* Opera is a full UTF-16 enabled program, the string formats used in Opera are made up of
 * null terminated strings of unsigned short values as specified in the UTF-16 spec.
 * the following types are for utility and naming purposes only. Due to inconsistancy between
 * compilers, wchar_t is not suitable for character management since wchar_t was designed for
 * either double byte or four byte characters as defined per UCS-2 or UCS-4, not for
 * multi-byte representations such as UTF-8 or UTF-16. Therefore Standard C Library functions
 * are not usable on this level either.
 */
typedef unsigned short EmBrowserChar;
typedef EmBrowserChar* EmBrowserString;

typedef struct EmBrowserRect
{
	long left;
	long top;
	long right;
	long bottom;
} EmBrowserRect;

enum
{
	emBrowserNoErr 				=  0,
	emBrowserOutOfMemoryErr 	= -1,
	emBrowserGenericErr 		= -2,
	emBrowserParameterErr 		= -3,
	emBrowserExitingErr 		= -4
};
typedef long EmBrowserStatus;

enum
{
	emBrowserImageDisplayNone		= 0,
	emBrowserImageDisplayAll 		= 1,
	emBrowserImageDisplayLoaded 	= 2
};
typedef long EmBrowserImageDisplayMode;


/* ---------------------------------------------------------------------------- 
 EmBrowser option bits, control various aspects of the widget behavior.
 ---------------------------------------------------------------------------- */
enum
{
	emBrowserOptionUseDefaults					= 0x00000000,
	/* API Version: 1.0 */
	emBrowserOptionSilent						= 0x00000001,
	emBrowserOptionNoContextMenu				= 0x00000002,
	/* API Version: 2.0 */
	emBrowserOptionDisableAppkeys				= 0x00000004,
	emBrowserOptionDisableTooltips				= 0x00000010,
	emBrowserOptionDisableScrollBarDelimiter	= 0x00000020,	// On mac, this will draw white where the scrollbars meet instead of a window header background.

	emBrowserOptionDisableLinkDrags				= 0x00000100,
	emBrowserOptionDisableOtherDrags			= 0x00000200,
	emBrowserOptionDisableAllDrags				= emBrowserOptionDisableLinkDrags | emBrowserOptionDisableOtherDrags,

	emBrowserOptionDisableJavaScript			= 0x00001000,
	emBrowserOptionDisableExternalEmbeds		= 0x00002000,

	emBrowserOptionDisableDownloadDialog		= 0x00004000,	// Ignore download of files Opera can't handle
	emBrowserOptionDisableUrlErrorMessage		= 0x00010000,	// Don't display loading failed (and similar) warning dialogs

	emBrowserOptionDisableJava					= 0x00020000	// Force Java off
};
typedef unsigned long EmBrowserOptions;

enum {
	emBrowserWindowOptionUseDefaults		= 0x00000000,
	emBrowserWindowOptionBackground			= 0x00000001
	/* API Version: 1.0 */

};
typedef unsigned long EmBrowserWindowOptions;

enum
{
	emBrowserURLOptionDefault				= 0x00000000,
	emBrowserURLOptionNoCache				= 0x00000001
	/* API Version: 2.0 (Items 0x00000000 - 0x00000001) */

};
typedef unsigned long EmBrowserURLOptions;

enum
{
	emBrowserInitDefault				= 0x00000000,
	emBrowserInitNoFontEnumeration		= 0x00000001,
	/* API Version: 2.1 */

	emBrowserInitDisableMouseGesures	= 0x00000002,
	emBrowserInitNoScrollbars			= 0x00000004,
	emBrowserInitDisableLinkDrags		= 0x00000010,
	emBrowserInitDisableOtherDrags		= 0x00000020,
	emBrowserInitDisableAllDrags		= emBrowserInitDisableLinkDrags | emBrowserInitDisableOtherDrags,

	emBrowserInitNoContextMenu			= 0x00000100,
	emBrowserInitDisableHotclickMenu	= 0x00000200
	/* API Version: 3.0 */
};
typedef unsigned long EmBrowserInitParams;

/* ---------------------------------------------------------------------------- 
 Search Options
 ---------------------------------------------------------------------------- */
enum {
	emOperaSearchOptionUseDefaults		= 0x00000000,

	emOperaSearchOptionCaseSensitive	= 0x00000001,
	emOperaSearchOptionEntireWord		= 0x00000002,

	emOperaSearchOptionFindNext			= 0x00000010,
	emOperaSearchOptionUpwards			= 0x00000020,	//Only used with find next
	/* API Version: 2.0 (Items 0x00000000 - 0x00000020) */
	emOperaSearchOptionWrapSearch		= 0x00000040,


	emOperaSearchOptionUseCustColor		= 0x00000080
		// if set, the remaining bits will be interpreted as color information,
		// like this: 0xRRGGBBxx
	/* API Version 3.0 */
};
typedef unsigned long EmOperaSearchOptions;

/* ---------------------------------------------------------------------------- 
 Text types to retrieve in EmBrowserGetTextProc.
 ---------------------------------------------------------------------------- */
enum
{
	emBrowserURLText			= 0,
	emBrowserLoadStatusText		= 1,
	emBrowserTitleText			= 2,
	emBrowserMouseContextText	= 3,
	emBrowserCombinedStatusText	= 4  // Load status except when pre-empted by mouse context
};
typedef unsigned long EmBrowserTextOptions;

/* ---------------------------------------------------------------------------- 
 Options to use when generating thumbnail images
 ---------------------------------------------------------------------------- */
enum
{
	emBrowserThumbnailOptionDisableJavaScript			= 0x00001000,
	emBrowserThumbnailOptionDisableExternalEmbeds		= 0x00002000
};
typedef unsigned long EmBrowserThumbnailOptions;

/* ---------------------------------------------------------------------------- 
 Notification Messages (BrowserControl Messages)  Browser --> Host Application
 Note: Host application can safely ignore many of these.  Will be documented.
 
 Even with the split between browser controls and browser documents, all of the 
 notifications are on a per browser control basis. For messages, one can think 
 of the browser control being notified about all of its sub-documents. An example 
 would be the emBrowser_app_msg_page_busy_changed message. This would fire when 
 all sub documents are loaded.
 ---------------------------------------------------------------------------- */
enum
{
	// REQUIRED:  	Host application must respond to these messages in order
	//				for Opera to behave properly.
	emBrowser_app_msg_edit_commands_changed		= 1000,
	emBrowser_app_msg_request_focus				= 1001,	

	// REQUIRED in future.
	// Please remove all refs to the widget, it is going away.
	emBrowser_app_msg_going_away          		= 1002,
	/* API Version: 1.0 (Items 1000-1002) */

	
	// OPTIONAL
	emBrowser_app_msg_status_text_changed		= 1200,
	emBrowser_app_msg_page_busy_changed			= 1201,
	emBrowser_app_msg_progress_changed			= 1202,
	emBrowser_app_msg_url_changed				= 1203,	
	emBrowser_app_msg_title_changed				= 1204,	
	emBrowser_app_msg_security_changed			= 1205,
	/* API Version: 1.0 (Items 1200-1205) */
    emBrowser_app_msg_page_undisplay			= 1206,
	emBrowser_app_msg_phase_changed  			= 1207,

	emBrowser_app_msg_request_close				= 1300, // JS Window close, f ex.
	emBrowser_app_msg_request_activation		= 1301, // JS
	emBrowser_app_msg_request_raise				= 1302, // JS (move higher in z-order)
	emBrowser_app_msg_request_lower				= 1303, // JS (move lower in z-order)
	emBrowser_app_msg_request_show				= 1304, // JS
	emBrowser_app_msg_request_hide				= 1305, // JS
	/* API Version: 1.0 (Items 1300-1305) */
	
	
	emBrowser_app_msg_content_type_unknown		= 1400,
	emBrowser_app_msg_content_type_unsupported	= 1401,
	emBrowser_app_msg_protocol_unsupported		= 1402
	/* API Version: 1.0 (Items 1400-1402) */
};
typedef long EmBrowserAppMessage;

/* ---------------------------------------------------------------------------- 
 Command Messages (BrowserControl Commands)  Host Application --> Opera
 ---------------------------------------------------------------------------- */
enum
{
	emBrowser_msg_copy_cmd						= 2000,
	emBrowser_msg_paste_cmd						= 2001,
	emBrowser_msg_cut_cmd						= 2002,
	emBrowser_msg_clear_cmd						= 2003,
	emBrowser_msg_select_all_cmd				= 2004,
	emBrowser_msg_undo_cmd						= 2005,
	emBrowser_msg_redo_cmd						= 2006,
	// This is used to find out if currently present modal dialogs 
	// hinder the host application's ability to close the widget.
	emBrowser_msg_quit_cmd						= 2007,
	/* API Version: 1.0 (Items 2000-2007) */
	
	emBrowser_msg_back							= 2100,	
	emBrowser_msg_forward						= 2101,
	emBrowser_msg_reload						= 2102,
	emBrowser_msg_stop_loading					= 2103,
	emBrowser_msg_homepage						= 2104,
	emBrowser_msg_searchpage					= 2105,	// Internet Config searchpage, NOT search in page
	emBrowser_msg_has_document					= 2106,
	emBrowser_msg_clone_window					= 2107,
	/* API Version: 1.0 (Items 2100-2107) */
	
	emBrowser_msg_print							= 2200
	/* API Version: 1.0 (Item 2200) */
};
typedef long EmBrowserMessage;


/* ---------- 
   Structures
   ---------- */
/* Opera uses struct member alignment set to 4, four, bytes throughout all 
 * modules on Win32. For applications passing structures back and forth this 
 * have to be the same. Otherwise data corruption will occur.
 */

/* ---------------------------------------------------------------------------- 
 Browser Instance
 ---------------------------------------------------------------------------- */
typedef struct OpaqueEmBrowser *EmBrowserRef;

/* ---------------------------------------------------------------------------- 
 Browser Document
 ---------------------------------------------------------------------------- */
typedef struct OpaqueEmBrowserDocument *EmDocumentRef;

/* ---------------------------------------------------------------------------- 
 Document cookie
 ---------------------------------------------------------------------------- */
typedef struct OpaqueEmCookieItem *EmCookieRef;

/* ---------------------------------------------------------------------------- 
 History item
 ---------------------------------------------------------------------------- */
typedef struct OpaqueEmHistoryItem *EmHistoryRef;

/* ---------------------------------------------------------------------------- 
 Thumbnail item
 ---------------------------------------------------------------------------- */
typedef struct OpaqueEmThumbnailItem *EmThumbnailRef;

/* ---------------------------------------------------------------------------- 
 Loading Status
 ---------------------------------------------------------------------------- */
enum {
	emLoadStatusLoading		= 0, 	// Busy
	emLoadStatusLoaded		= 1,	// Not Busy. Load succeded.
	emLoadStatusFailed		= 2,	// Not Busy. Load failed.
	emLoadStatusStopped		= 3,	// Not Busy. Load stopped by request from user/host app.
	emLoadStatusEmpty		= 4,	// Not Busy. Nothing loaded (for instance a newly created widget).
	emLoadStatusUnknown		= 5,	// Unknown/other. Don't test on this, test on the values you understand.
	/* API Version: 1.0 (Items 0-5) */

	emLoadStatusUploading	= 16,	// Busy
	emLoadStatusCreated     = 17    // Document ready for JavaScript handling
	/* API Version: 3.0 */
};
typedef long EmLoadStatus;

/* ---------------------------------------------------------------------------- 
 Javascript
 ---------------------------------------------------------------------------- */
/* Notes about memory management:
 * - All strings returned from Opera need to be copied if stored.
 * - All object references need to be protected if stored.
 * - All protected objects must be unprotected at some point.
 */

enum
{
	emJavascriptNull,
	emJavascriptUndefined,
	emJavascriptBoolean,
	emJavascriptNumber,
	emJavascriptString,
	emJavascriptObject
};
typedef unsigned long EmJavascriptType;

typedef struct OpaqueEmJavascriptObject *EmJavascriptObjectRef;

typedef struct EmJavascriptValue
{
	EmJavascriptType type;
	union
	{
		unsigned int boolean;
		double number;
		EmBrowserString string;
		EmJavascriptObjectRef object;
	} value;
} EmJavascriptValue;

enum
{
	emJavascriptNoErr			= emBrowserNoErr,
	emJavascriptOutOfMemoryErr	= emBrowserOutOfMemoryErr,
	emJavascriptGenericErr 		= emBrowserGenericErr,
	emJavascriptParameterErr	= emBrowserParameterErr,
	emJavascriptCompilationErr	= emJavascriptParameterErr - 1,
	emJavascriptExecutionErr	= emJavascriptCompilationErr -1
};
typedef long EmJavascriptStatus;

/* ---------------------------------------------------------------------------- 
 Thumbnail
 ---------------------------------------------------------------------------- */
/*  
 * - Returned thumbnail images are native and temporary
 */

enum
{
	emThumbnailNoErr			= emBrowserNoErr,
	emThumbnailOutOfMemoryErr	= emBrowserOutOfMemoryErr,
	emThumbnailGenericErr 		= emBrowserGenericErr
};
typedef long EmThumbnailStatus;

/* ---------------------------------------------------------------------------- 
 Mouse cursor types
 ---------------------------------------------------------------------------- */
enum
{
	emCursorURI					= 0,
	emCursorCrosshair			= 1,
	emCursorDefaultArrow		= 2,
	emCursorCurrentPointer		= 3,
	emCursorMove				= 4,
	emCursorResizeEast			= 5,
	emCursorResizeNorthEast		= 6,
	emCursorResizeNorthWest		= 7,
	emCursorResizeNorth			= 8,
	emCursorResizeSouthEast		= 9,
	emCursorResizeSouthWest		= 10,
	emCursorResizeSouth			= 11,
	emCursorResizeWest			= 12,
	emCursorText				= 13,
	emCursorWait				= 14,
	emCursorHelp				= 15,
	emCursorHorizontalSplitter	= 16,
	emCursorVerticalSplitter	= 17,
	emCursorArrowWait			= 18,
	emCursorProgress			= 19
};
typedef unsigned long EmMouseCursor;

// -
// ----- BROWSER INSTANCE APIs -----
// -

/* ---------------------------------------------------------------------------- 
 Initialization Parameter Methods
 ---------------------------------------------------------------------------- */

// ********* Creation
// ----------------------------------------------------------------------------
// Description:  Creates an instance of the embedded browser.
typedef EmBrowserStatus	(*EmBrowserCreateProc)(IN void* port, IN EmBrowserOptions options, OUT EmBrowserRef* emBrowser);
// ----------------------------------------------------------------------------
// Description:  Destroys an instance of the embedded browser.
typedef EmBrowserStatus	(*EmBrowserDestroyProc)(IN EmBrowserRef);

// ********* Memory Allocation
// ----------------------------------------------------------------------------
// Description:  Opera allocates memory via the host application.
typedef void *	(*AppMallocProc)(unsigned long size);
// ----------------------------------------------------------------------------
// Description:  Opera frees memory via the host application.
typedef void	(*AppFreeProc)(void *ptr);
// ----------------------------------------------------------------------------
// Description:  Opera re-allocates memory via the host application.
typedef void *	(*AppReallocProc)(void *ptr, unsigned long newSize);
// ----------------------------------------------------------------------------

// ********* Shutdown
// ----------------------------------------------------------------------------
// Description:  Releases all global structures, stops network activity and unregisters system callbacks.
typedef EmBrowserStatus	(*EmBrowserShutdown)(void);

/* ---------------------------------------------------------------------------- 
 Notification Methods
 ---------------------------------------------------------------------------- */
// Description:  General purpose Opera notifying the host application.
typedef void (*EmBrowserAppSimpleNotificationProc)(IN EmBrowserRef inst, IN EmBrowserAppMessage msg, IN long value);
// ----------------------------------------------------------------------------
// Description:  Hook for before loading the URL.
typedef long (*EmBrowserAppBeforeNavigationProc)(IN EmBrowserRef inst, IN const EmBrowserString destURL, OUT EmBrowserString newURL);
// ----------------------------------------------------------------------------
// Description:  Ask host to create a new browser. Width & height in bounds is widget size, top & left is window position.
//				 Caller may be NULL. Return non-zero if successful.
typedef long (*EmBrowserAppRequestNewBrowserProc)(OPT IN EmBrowserRef caller, IN EmBrowserOptions browserOptions, IN EmBrowserWindowOptions windowOptions, IN EmBrowserRect *bounds, IN const EmBrowserString destURL, OUT EmBrowserRef* result);

// ----------------------------------------------------------------------------
// Description:  Handle protocols not supported by Opera as news and mail.
//				Optional, if not implemented, the browser will send an AppleEvent to InternetConfig.
typedef EmBrowserStatus (*EmBrowserAppHandleUnknownProtocolProc)(IN EmBrowserRef inst, IN const EmBrowserString url);

// ----------------------------------------------------------------------------
// Description:  Asks the host application to move the window to the given coords. Return non-zero if successful.
typedef long (*EmBrowserAppRequestSetPositionProc)(IN EmBrowserRef widget, IN long x_pos, IN long y_pos);

// ----------------------------------------------------------------------------
// Description:  Asks the host application to resize the widget to the given size. Return non-zero if successful.
typedef long (*EmBrowserAppRequestSetSizeProc)(IN EmBrowserRef widget, IN long width, IN long height);

// ----------------------------------------------------------------------------
// Description:  Hook for before executing Javascript.
typedef long (*EmBrowserAppJavascriptNotificationProc)(IN EmBrowserRef widget, IN EmDocumentRef doc);

// ----------------------------------------------------------------------------
// Description:  Hook for after navigation.
typedef long (*EmBrowserAppAfterNavigationProc)(IN EmBrowserRef widget, IN EmDocumentRef doc);

// ----------------------------------------------------------------------------
// Description:  Hook for information of url which has been redirected
typedef void (*EmBrowserAppRedirNavigationProc)(IN EmBrowserRef inst, IN const EmBrowserString destURL, IN const EmBrowserString redirectURL);

// ----------------------------------------------------------------------------
// Description:  Hook for setting the mouse cursor. Optional, if not implemented the widget will set all cursors.
//				 Return non-zero to override the widget cursor, this allows for overriding only some cursors.
typedef long (*EmBrowserAppRequestSetCursorProc)(IN EmMouseCursor cursorKind);

// ----------------------------------------------------------------------------
// Description:  Hook for allowïng local file URLs on secure pages.
typedef long (*EmBrowserAppAllowLocalFileURLProc)(IN EmBrowserRef widget, IN const EmBrowserString docURL, IN const EmBrowserString localFileURL);

/* ---------------------------------------------------------------------------- 
 Browser Methods
 ---------------------------------------------------------------------------- */

// ********* Screen Port Management
// ----------------------------------------------------------------------------
// Description:  Setting where Opera can draw in the window.
typedef EmBrowserStatus	(*EmBrowserSetPortLocationProc)(IN EmBrowserRef, IN const EmBrowserRect* browserLoc, IN void* window);
// ----------------------------------------------------------------------------
// Description:  Getting where Opera can draw in the window.
typedef EmBrowserStatus	(*EmBrowserGetPortLocationProc)(IN EmBrowserRef, IO EmBrowserRect* browserLoc);


// ********* Screen Port Management
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserSetVisibilityProc)(IN EmBrowserRef, IN long visible);
// ----------------------------------------------------------------------------
typedef long (*EmBrowserGetVisibilityProc)(IN EmBrowserRef);


// ********* Selection
// ----------------------------------------------------------------------------
typedef EmBrowserStatus	(*EmBrowserScrollToSelectionProc)(IN EmBrowserRef inst);


// ********* Basic Navigation
// ----------------------------------------------------------------------------
// Description:  Open URL in browser.  Note:  EmBrowserURLOptions parameter added in API version 2.0
typedef EmBrowserStatus (*EmBrowserOpenURLProc)(IN EmBrowserRef inst, IN const EmBrowserString url, IN EmBrowserURLOptions urlOptions);


// ********* Properties
// ----------------------------------------------------------------------------
// Description:  Get download progress numbers.
// @param currentRemaining The size of the page remaining to be downloaded. -1 if the total size is unknown at the current stage
// @param totalAmount The size of the page being downloaded. -1 if the size is unknown at the current stage
typedef EmBrowserStatus	(*EmBrowserGetDLProgressNumsProc)(IN EmBrowserRef inst, OUT long *currentRemaining, OUT long *totalAmount);
// ----------------------------------------------------------------------------
// Description:  Returns the current load status.
typedef EmLoadStatus	(*EmBrowserGetPageBusyProc)(IN EmBrowserRef inst);
// ----------------------------------------------------------------------------
// Description:  Returns the current security level.
typedef EmBrowserStatus	(*EmBrowserGetSecurityDescProc)(IN EmBrowserRef inst, OUT long* securityOnOff, IN long bufferSize, OUT EmBrowserString securityDescription);
// ----------------------------------------------------------------------------
// Description:  Get various text strings from the browser.
typedef EmBrowserStatus	(*EmBrowserGetTextProc)(IN EmBrowserRef inst, IN EmBrowserTextOptions textToGet,IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);
// ----------------------------------------------------------------------------


// ********* Commands
// ----------------------------------------------------------------------------
// Description:  Query Opera to see if a message is currently available.
typedef long	(*EmBrowserCanHandleMessageProc)(IN EmBrowserRef inst, IN EmBrowserMessage msg);
// ----------------------------------------------------------------------------
// Description:  Send a command message to Opera.
typedef EmBrowserStatus	(*EmBrowserDoHandleMessageProc)(IN EmBrowserRef inst, IN EmBrowserMessage msg);


// ********* Miscellaneous
// ----------------------------------------------------------------------------
// Description:	Force the Widget to draw itself.  Only needed in special circumstances.
//				The widget is self-drawing.
typedef EmBrowserStatus	(*EmBrowserDrawProc)(IN EmBrowserRef inst, IN EmBrowserRect* updateRegion);

// ----------------------------------------------------------------------------
// Description:  Inform the EmBrowser that it has become active, to activate forms
//				controls, scrollbars, etc.
typedef EmBrowserStatus (*EmBrowserSetActiveProc)(IN EmBrowserRef, IN long active);

// ----------------------------------------------------------------------------
// Description:  Inform the EmBrowser that it has received or lost keyboard focus.
typedef EmBrowserStatus (*EmBrowserSetFocusProc)(IN EmBrowserRef inst, IN long focus);

// ----------------------------------------------------------------------------
// Description:  Set zoom factor for EmBrowser, in per cent
typedef EmBrowserStatus (*EmBrowserSetZoomProc)(IN EmBrowserRef inst, IN long zoom);

// ----------------------------------------------------------------------------
// Description:  Set default encoding, will be used if document doesn't supply an encoding.
// Changed SetFallbackEncoding

// ----------------------------------------------------------------------------
// Description:  Set user interface language, will default to english.lng in the directory opera is located
typedef EmBrowserStatus (*EmBrowserSetUILanguage)(IN const EmBrowserString languageFile);

// ----------------------------------------------------------------------------
// Description:  Set the user stylesheet. Pass NULL to stop using custom stylesheet.
typedef EmBrowserStatus (*EmBrowserSetUserStylesheetProc)(IN const EmBrowserString stylesheetFilePath);

// ----- DOCUMENT INSTANCE APIs -----
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserOpenURLinDocProc)(IN EmDocumentRef inst, IN const EmBrowserString url, IN EmBrowserURLOptions urlOptions);
// ----------------------------------------------------------------------------
// Description:   Get the size that will need to be allocated by the host for HTML source.
typedef EmBrowserStatus	(*EmBrowserGetSourceSizeProc)(IN EmDocumentRef inst, OUT long *bufferSize);
// ----------------------------------------------------------------------------
// Description:   Get the HTML source for the page.  Only works after the page is finished loading.
typedef EmBrowserStatus	(*EmBrowserGetSourceProc)(IN EmDocumentRef inst, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/);
// ----------------------------------------------------------------------------
// Description:  Get root document for a given EmBrowserRef
typedef EmBrowserStatus (*EmBrowserGetRootDocProc) (EmBrowserRef IN inst, OUT EmDocumentRef *doc);

// ----------------------------------------------------------------------------
// Description:  Get URL for a given document.   Note:  textLength parameter added in API version 2.0
typedef EmBrowserStatus (*EmBrowserGetURLProc) (EmDocumentRef IN doc, IN long bufferSize, OUT EmBrowserString docURL/*can be NULL*/,OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:  Get Number of sub-documents for a given document.  Zero means document
//					is not a frameset.
typedef long (*EmBrowserGetNumberOfSubDocumentsProc) (EmDocumentRef IN doc);

// ----------------------------------------------------------------------------
// Description:  Get parent for a given document.  Null means this document is at the top of the structure.
typedef EmBrowserStatus (*EmBrowserGetParentDocumentProc) (EmDocumentRef IN doc, EmDocumentRef  OUT *docOut);

// ----------------------------------------------------------------------------
// Description:  Get EmBrowserRef which a given document is contained in.
typedef EmBrowserStatus (*EmBrowserGetBrowserProc) (EmDocumentRef IN doc, EmBrowserRef OUT *inst );

// ----------------------------------------------------------------------------
// Description:  Get all sub-documents for a given document.  Zero means document
//					is not a frameset.
typedef EmBrowserStatus (*EmBrowserGetSubDocumentsProc) (EmDocumentRef IN doc, long IN numDocs, EmDocumentRef OUT *docArray, EmBrowserRect OUT *locationArray);

// ----- APIs ADDED VERSION 2.0 -----


// ********* Miscellaneous
// ----------------------------------------------------------------------------
// Description:  Search in document
typedef EmBrowserStatus (*EmBrowserSearchInActiveDocumentProc)(IN EmBrowserRef inst, IN const EmBrowserString pattern, EmOperaSearchOptions options);

// ----------------------------------------------------------------------------
// Description:  Get zoom factor for EmBrowser, in per cent
typedef EmBrowserStatus (*EmBrowserGetZoomProc)(IN EmBrowserRef inst, OUT long * zoom);

// ----------------------------------------------------------------------------
// Description:  Set default encoding, will be used if document doesn't supply an encoding.
typedef EmBrowserStatus (*EmBrowserSetFallbackEncodingProc)(IN const EmBrowserString encoding);

// ----------------------------------------------------------------------------
// Description:  Force encoding. This encoding _will_ be used in subsequent loads in the window.
typedef EmBrowserStatus (*EmBrowserForceEncodingProc)(IN EmBrowserRef inst, IN const EmBrowserString encoding);

// ----------------------------------------------------------------------------
// Description:  Enable or disable sounds in browser and page.
typedef EmBrowserStatus (*EmBrowserSetSoundProc)(long IN programSound, long IN pageSound);


// ********* Cookies
// ----------------------------------------------------------------------------
// Description:	Return list of cookies in the document.
typedef EmBrowserStatus	(*EmBrowserGetCookieListProc)(IN const EmBrowserString domain,IN const EmBrowserString path, OUT EmCookieRef* cookieArray);

// ----------------------------------------------------------------------------
// Description:	Return the number of cookies registered for the document
typedef long (*EmBrowserGetCookieCountProc)(IN const EmBrowserString domain,IN const EmBrowserString path);

// ----------------------------------------------------------------------------
// Description:	Get the name of the cookie.
typedef EmBrowserStatus (*EmBrowserGetCookieNameProc)(IN EmCookieRef ref, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get the value of the cookie.
typedef EmBrowserStatus (*EmBrowserGetCookieValueProc)(IN EmCookieRef ref, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get the expiry time of the cookie.
typedef long (*EmBrowserGetCookieExpiryProc)(IN EmCookieRef ref);

// ----------------------------------------------------------------------------
// Description:	Get the domain of the cookie.
typedef EmBrowserStatus (*EmBrowserGetCookieDomainProc)(IN EmCookieRef ref, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get the path of the cookie.
typedef EmBrowserStatus (*EmBrowserGetCookiePathProc)(IN EmCookieRef ref, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get the domain received from the server.
typedef EmBrowserStatus (*EmBrowserGetCookieReceivedDomainProc)(IN EmCookieRef ref, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get the path received from the server.
typedef EmBrowserStatus (*EmBrowserGetCookieReceivedPathProc)(IN EmCookieRef ref, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get the security state of the cookie. Return nonzero if cookie is secure.
typedef long (*EmBrowserGetCookieSecurityProc)(IN EmCookieRef ref);

// ----------------------------------------------------------------------------
// Description:	Set a cookie. Fill in entire request: name, value and attributes.
typedef EmBrowserStatus (*EmBrowserSetCookieRequestProc)(IN const EmBrowserString domain,IN const EmBrowserString path, IN const EmBrowserString);

// ----------------------------------------------------------------------------
// Description:	Set a cookie value. The request: name and attributes are subtracted from existing cookie.
typedef EmBrowserStatus (*EmBrowserSetCookieValueProc)(IN const EmBrowserString domain,IN const EmBrowserString path, IN EmCookieRef ref, IN const EmBrowserString value);

// ----------------------------------------------------------------------------
// Description:	Remove a set of cookies.
// @return emBrowserOutOfMemoryErr if memory error hindered the operation. emBrowserGenericErr on other errors. emBrowserNoErr if the method worked.
typedef EmBrowserStatus (*EmBrowserRemoveCookiesProc)(IN const EmBrowserString domain,IN const EmBrowserString path, IN const EmBrowserString name);



// ********* History
// ----------------------------------------------------------------------------
// Description:	Return the list of items in the current session's history.
typedef EmBrowserStatus	(*EmBrowserGetHistoryListProc)(IN EmBrowserRef inst, OUT EmHistoryRef* historyArray);

// ----------------------------------------------------------------------------
// Description:	Return the number of items in the current session's history.
typedef long (*EmBrowserGetHistorySizeProc)(IN EmBrowserRef inst);

// ----------------------------------------------------------------------------
// Description:	Return the index of the current position in the history list.
typedef long (*EmBrowserGetHistoryIndexProc)(IN EmBrowserRef inst);

// ----------------------------------------------------------------------------
// Description:	Set the index of the current position in the history list.
typedef EmBrowserStatus (*EmBrowserSetHistoryIndexProc)(IN EmBrowserRef inst, IN long index);

// ----------------------------------------------------------------------------
// Description:	Remove an item from the history list.
typedef EmBrowserStatus (*EmBrowserRemoveHistoryItemProc)(IN EmBrowserRef inst, IN long index);

// ----------------------------------------------------------------------------
// Description:	Insert an item in the history list.
typedef EmBrowserStatus (*EmBrowserInsertHistoryItemProc)(IN EmBrowserRef inst, IN long index, IN EmHistoryRef);

// ----------------------------------------------------------------------------
// Description:	Append an item to the history list.
typedef EmBrowserStatus (*EmBrowserAppendHistoryItemProc)(IN EmBrowserRef inst, IN EmHistoryRef);

// ----------------------------------------------------------------------------
// Description:	Get title of the history item.
typedef EmBrowserStatus (*EmBrowserGetHistoryItemTitleProc)(IN EmHistoryRef doc, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Get url of the history item.
typedef EmBrowserStatus (*EmBrowserGetHistoryItemURLProc)(IN EmHistoryRef doc, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:	Remove all items from the history list.
typedef EmBrowserStatus (*EmBrowserRemoveAllHistoryItemsProc)(IN EmBrowserRef inst);


// ********* Javascript
// ----------------------------------------------------------------------------
// Description: Notification of the result of a javascript operation.
typedef void (*EmBrowserJSResultProc)(IN void *clientData, IN EmJavascriptStatus status, IN EmJavascriptValue *value /* can be NULL */);

// ----------------------------------------------------------------------------
// Description: Evaluate a string of JavaScript code in a document.
typedef EmJavascriptStatus (*EmBrowserJSEvaluateCodeProc)(IN EmDocumentRef doc, IN EmBrowserString code,
                                                          IN EmBrowserJSResultProc callback /* can be NULL */, IN void *callbackClientData /* can be NULL */);

// ----------------------------------------------------------------------------
// Description: Read the value of a property of an object.
typedef EmJavascriptStatus (*EmBrowserJSGetPropertyProc)(IN EmDocumentRef doc, IN EmJavascriptObjectRef object, IN EmBrowserString propertyName,
                                                         IN EmBrowserJSResultProc callback /* can be NULL */, IN void *callbackClientData /* can be NULL */);

// ----------------------------------------------------------------------------
// Description: Write a value to a property of an object.
typedef EmJavascriptStatus (*EmBrowserJSSetPropertyProc)(IN EmDocumentRef doc, IN EmJavascriptObjectRef object, IN EmBrowserString propertyName, IN EmJavascriptValue *value,
                                                         IN EmBrowserJSResultProc callback /* can be NULL */, IN void *callbackClientData /* can be NULL */);

// ----------------------------------------------------------------------------
// Description: Call a method on an object.
typedef EmJavascriptStatus (*EmBrowserJSCallMethodProc)(IN EmDocumentRef doc, IN EmJavascriptObjectRef object, IN EmBrowserString methodName,
                                                        IN unsigned argumentsCount, IN EmJavascriptValue *argumentsArray,
                                                        IN EmBrowserJSResultProc callback /* can be NULL */, IN void *callbackClientData /* can be NULL */);

// ----------------------------------------------------------------------------
// Description: Protect a javascript object (increasing its reference count by one).
typedef EmJavascriptStatus (*EmBrowserJSProtectObjectProc)(IN EmJavascriptObjectRef object);

// ----------------------------------------------------------------------------
// Description: Unprotect a javascript object (decreasing its reference count by one).
typedef EmJavascriptStatus (*EmBrowserJSUnprotectObjectProc)(IN EmJavascriptObjectRef object);

// ----------------------------------------------------------------------------
// Description: Hook for function call to functions created with EmBrowserJSCreateMethodProc.
typedef EmJavascriptStatus (*EmBrowserJSMethodCallProc)(IN void *clientData, IN EmDocumentRef doc, IN EmJavascriptObjectRef this_object,
                                                        IN unsigned argumentsCount, IN EmJavascriptValue *argumentsArray,
                                                        OUT EmJavascriptValue *returnValue);

// ----------------------------------------------------------------------------
// Description: Create a method.
typedef EmJavascriptStatus (*EmBrowserJSCreateMethodProc)(IN EmDocumentRef doc, OUT EmJavascriptObjectRef *method,
                                                          IN EmBrowserJSMethodCallProc callback, IN void *callbackClientData /* can be NULL */);


// ********* Document
// ----------------------------------------------------------------------------
// Description:  Set the browser stylesheet.
typedef EmBrowserStatus (*EmBrowserSetBrowserStylesheetProc)(IN const EmBrowserString stylesheetFilePath);

// ----------------------------------------------------------------------------
// Description:  Get number of alternate stylesheets.
typedef long	(*EmBrowserGetAltStylesheetCountProc)(IN EmBrowserRef inst);

// ----------------------------------------------------------------------------
// Description:  Get alternate stylesheet title.
typedef EmBrowserStatus	(*EmBrowserGetAltStylesheetTitleProc)(IN EmBrowserRef inst, IN long index, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);

// ----------------------------------------------------------------------------
// Description:  Get alternate stylesheet title.
typedef EmBrowserStatus	(*EmBrowserEnableAltStylesheetProc)(IN EmBrowserRef inst, IN EmBrowserString title);

// ----------------------------------------------------------------------------
// Description:  Get the width & height of the document (not the view).
typedef EmBrowserStatus	(*EmBrowserGetContentSizeProc)(IN EmBrowserRef inst, IN long* width, IN long* height);

// ----------------------------------------------------------------------------
// Description:  Get the the scroll position of the document
typedef EmBrowserStatus	(*EmBrowserGetScrollPosition)(IN EmBrowserRef inst, IN long* x, IN long* y);

// ----------------------------------------------------------------------------
// Description:  Scroll document to given position.
typedef EmBrowserStatus	(*EmBrowserSetScrollPosition)(IN EmBrowserRef inst, IN long x, IN long y);

// ----------------------------------------------------------------------------
// Description:  Get title of document.
typedef EmBrowserStatus	(*EmBrowserGetDocumentTitleProc)(IN EmDocumentRef doc, IN long bufferSize, OUT EmBrowserString textBuffer /*can be NULL*/, OUT long *textLength /*can be NULL*/);


// ********* Thumbnail
// ----------------------------------------------------------------------------
// Description: Notification of the result of a thumbnail operation.
typedef void (*EmBrowserThumbnailResultProc)(IN void *imageData, IN EmThumbnailStatus status, IN EmThumbnailRef ref);

// ----------------------------------------------------------------------------
// Description: Request creation of a thumbnail of a web page.
typedef EmThumbnailStatus (*EmBrowserThumbnailRequestProc)(IN EmBrowserString url, IN long width, IN long height, IN long scale, IN EmBrowserThumbnailResultProc callback, OUT EmThumbnailRef * ref);
typedef EmThumbnailStatus (*EmBrowserThumbnailRequestWithOptionsProc)(IN EmBrowserString url, IN long width, IN long height, IN long scale, IN EmBrowserThumbnailResultProc callback, IN EmBrowserThumbnailOptions options, OUT EmThumbnailRef * ref);

// ----------------------------------------------------------------------------
// Description: Kill a thumbnail request (when you do not want more updates).
typedef EmThumbnailStatus (*EmBrowserThumbnailKillProc)(IN EmThumbnailRef ref);


// ********* Plugin
// Description: Use plugin path
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserUsePluginPathProc)(IN EmBrowserRef, IN EmBrowserString pluginPath);

// ********* Properties
// ----------------------------------------------------------------------------
// Description:  Get upload progress numbers.
// @param loadedBytes The number of bytes that has been uploaded. 
// @param totalAmount The size of the page being uploaded. -1 if the size is unknown at the current stage
typedef EmBrowserStatus	(*EmBrowserGetULProgressProc)(IN EmBrowserRef inst, OUT long *loadedBytes, OUT long *totalAmount);

// ----------------------------------------------------------------------------
// Description:  Get download progress numbers. Modified interface.
// @param loadedBytes The number of bytes that has been downloaded. 
// @param totalAmount The size of the page being downloaded. -1 if the size is unknown at the current stage
typedef EmBrowserStatus	(*EmBrowserGetDLProgressProc)(IN EmBrowserRef inst, OUT long *loadedBytes, OUT long *totalAmount);

// ********* Image Display
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserSetImageDisplay)(IN EmBrowserRef, IN EmBrowserImageDisplayMode imageMode);

// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserGetImageDisplay)(IN EmBrowserRef, OUT EmBrowserImageDisplayMode* imageMode);

// ********* Visibility of visited links
// Description: Disable or enable the visibility of visited links
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserSetVisLinksProc)(IN long visible);

// ********* Toggle SSR mode
// Description: Enable and disable Small Screen Rendering mode for given EmBrowserRef instance
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserToggleSSRProc)(EmBrowserRef inst);

// ********* Windows
// Description: Get the platform window handle
// ----------------------------------------------------------------------------
typedef void* (*EmBrowserGetPortProc)(IN EmBrowserRef);

// ********* Port control
// Description: Macintosh only! Tells the browser to paint to a GrafPtr instead of to the window.
// Can be used for creating thumbnails etc.
// Will paint to this port until the function is called with NULL as grafPtr.
// Note: It is not recommended to return from the event handler or calling WaitNextEvent with the port set to something other than the window,
// or the Opera widget may have invalid areas that are not properly updated. So the proper way to do this would probably be:
//	browserMethods->useGrafPtr(myEmBrowser, myGrafPort);
//	browserMethods->draw(myEmBrowser, &myRect);
//	browserMethods->useGrafPtr(myEmBrowser, 0);
//
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserUseGrafPtrProc)(IN EmBrowserRef, IN void* grafPtr);

// ********* Toggle Fit To Width mode
// Description: Enable and disable Fit To Width Rendering mode for given EmBrowserRef instance
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserToggleFTWProc)(EmBrowserRef inst);

// ********* Add a component to the user agent string
// Description: Adds a string inside the comments of a user agent string
// Allows a Web server to distinguish the client application from a regular Opera
// ----------------------------------------------------------------------------
typedef EmBrowserStatus (*EmBrowserAddUAComponent)(IN const EmBrowserString component);



/* application calls these methods that apply to each browser instance */
typedef struct EmBrowserMethods 
{
	long majorVersion;												// 1
	long minorVersion;												// 2
	EmBrowserSetPortLocationProc 		setLocation;				// 3
	EmBrowserGetPortLocationProc 		getLocation;				// 4
	EmBrowserDrawProc					draw;						// 5
	EmBrowserScrollToSelectionProc		scrollToSelection;  		// 6
	EmBrowserGetPageBusyProc			pageBusy;					// 7
	EmBrowserGetDLProgressNumsProc 		downloadProgressNums;		// 8
	EmBrowserGetTextProc				getText;					// 9
	EmBrowserGetSourceProc				getSource;					// 10
	EmBrowserGetSourceSizeProc			getSourceSize;				// 11
	EmBrowserCanHandleMessageProc		canHandleMessage;			// 12
	EmBrowserDoHandleMessageProc		handleMessage;				// 13
	EmBrowserOpenURLProc				openURL;					// 14
	EmBrowserOpenURLinDocProc			openURLInDoc;				// 15
	EmBrowserSetFocusProc				setFocus;					// 16
	EmBrowserSetVisibilityProc			setVisible;					// 17
	EmBrowserGetVisibilityProc			getVisible;					// 18
	EmBrowserSetActiveProc				setActive;					// 19
	EmBrowserGetSecurityDescProc		getSecurityDesc;			// 20
	EmBrowserGetRootDocProc				getRootDoc;					// 21
	EmBrowserGetURLProc					getURL;						// 22
	EmBrowserGetNumberOfSubDocumentsProc	getNumberOfSubDocuments;	// 23
	EmBrowserGetParentDocumentProc		getParentDocument;			// 24
	EmBrowserGetBrowserProc				getBrowser;					// 25
	EmBrowserGetSubDocumentsProc		getSubDocuments;			// 26
	/* API Version: 1.0 (Items 1-26) */

	EmBrowserSearchInActiveDocumentProc	searchInActiveDocument;		// 27
	/* API Version: 2.0 (Items 27) */

	EmBrowserGetDocumentTitleProc		getDocumentTitle;			// 28
	EmBrowserSetZoomProc				setZoom;					// 29
	EmBrowserSetFallbackEncodingProc	setFallbackEncoding;		// 30
	EmBrowserSetUserStylesheetProc		setUserStylesheet;			// 31
	/* API Version: 2.1 (Items 27-31) */

	// New functions here...
	/* API Version: 3.0 (Items 32-xx) */

    EmBrowserGetHistoryListProc         getHistoryList;             // 32
    EmBrowserGetHistorySizeProc         getHistorySize;             // 33
    EmBrowserGetHistoryIndexProc        getHistoryIndex;            // 34
    EmBrowserSetHistoryIndexProc        setHistoryIndex;            // 35
    EmBrowserRemoveHistoryItemProc      removeHistoryItem;          // 36
    EmBrowserInsertHistoryItemProc      insertHistoryItem;          // 37
    EmBrowserAppendHistoryItemProc      appendHistoryItem;          // 38
    EmBrowserGetHistoryItemTitleProc    getHistoryItemTitle;        // 39
    EmBrowserGetHistoryItemURLProc      getHistoryItemURL;          // 40

    EmBrowserGetCookieListProc          getCookieList;              // 41
    EmBrowserGetCookieCountProc         getCookieCount;             // 42
    EmBrowserGetCookieNameProc          getCookieName;              // 43
    EmBrowserGetCookieValueProc         getCookieValue;             // 44
    EmBrowserGetCookieExpiryProc        getCookieExpiry;            // 45
    EmBrowserGetCookieDomainProc        getCookieDomain;            // 46
    EmBrowserGetCookiePathProc          getCookiePath;              // 47
    EmBrowserGetCookieSecurityProc      getCookieSecurity;          // 48
    EmBrowserSetCookieRequestProc       setCookieRequest;           // 49
    EmBrowserSetCookieValueProc         setCookieValue;             // 50

	EmBrowserJSEvaluateCodeProc			jsEvaluateCode;				// 51
	EmBrowserJSGetPropertyProc			jsGetProperty;				// 52
	EmBrowserJSSetPropertyProc			jsSetProperty;				// 53
	EmBrowserJSCallMethodProc			jsCallMethod;				// 54
	EmBrowserJSProtectObjectProc		jsProtectObject;			// 55
	EmBrowserJSUnprotectObjectProc		jsUnprotectObject;			// 56
	EmBrowserJSCreateMethodProc			jsCreateMethod;				// 57

   	EmBrowserSetBrowserStylesheetProc	setBrowserStylesheet;		// 58
   	EmBrowserGetAltStylesheetCountProc	getAltStylesheetCount;		// 59
   	EmBrowserGetAltStylesheetTitleProc  getAltStylesheetTitle;    	// 60
   	EmBrowserEnableAltStylesheetProc    enableAltStylesheet;    	// 61

   	EmBrowserThumbnailRequestProc       thumbnailRequest;       	// 62
   	EmBrowserThumbnailKillProc          thumbnailKill;          	// 63

   	EmBrowserSetUILanguage				setUILanguage;				// 64
	EmBrowserGetContentSizeProc			getContentSize;				// 65
	EmBrowserGetScrollPosition			getScrollPos;				// 66
	EmBrowserSetScrollPosition			setScrollPos;				// 67

	EmBrowserUsePluginPathProc			usePluginPath;				// 68

   	EmBrowserGetZoomProc				getZoom;					// 69
	EmBrowserForceEncodingProc	        forceEncoding;              // 70
	EmBrowserSetSoundProc    	        setSound;                   // 71

   	EmBrowserGetULProgressProc 	        uploadProgress;	            // 72
   	EmBrowserGetDLProgressProc 	        downloadProgress;           // 73

    EmBrowserRemoveAllHistoryItemsProc  removeAllHistoryItems;      // 74

	EmBrowserSetImageDisplay			setImageDisplayMode;		// 75
	EmBrowserGetImageDisplay			getImageDisplayMode;		// 76

    EmBrowserRemoveCookiesProc          removeCookies;              // 77
    EmBrowserGetCookieReceivedDomainProc getCookieReceivedDomain;   // 78
    EmBrowserGetCookieReceivedPathProc   getCookieReceivedPath;     // 79

	EmBrowserSetVisLinksProc			setVisLinks;				// 80

	EmBrowserGetPortProc				getWindowhandle;			// 81

	EmBrowserToggleSSRProc				toggleSSRmode;				// 82

	EmBrowserUseGrafPtrProc				useGrafPtr;					// 83

	EmBrowserThumbnailRequestWithOptionsProc	thumbnailRequestWithOptions;	// 84

	EmBrowserToggleFTWProc				toggleFitToWidth;			// 85

	EmBrowserAddUAComponent				addUAComponent;				// 86

    long filler[128 - 86];
} EmBrowserMethods;

/* ---------- */

// ----- App Notification Structure -----

/* application fills these in to be notified */
typedef struct EmBrowserAppNotification 
{
	long majorVersion;												// 1
	long minorVersion;												// 2

	EmBrowserAppSimpleNotificationProc		notification;			// 3
	EmBrowserAppBeforeNavigationProc		beforeNavigation;		// 4
	EmBrowserAppHandleUnknownProtocolProc	handleProtocol;			// 5
	EmBrowserAppRequestNewBrowserProc		newBrowser;				// 6
	EmBrowserAppRequestSetPositionProc		posChange;				// 7
	EmBrowserAppRequestSetSizeProc			sizeChange;				// 8
	/* API Version: 1.0 (Items 1-8) */

	EmBrowserAppJavascriptNotificationProc	javascript;				// 9
	EmBrowserAppAfterNavigationProc			afterNavigation;		// 10
	
	EmBrowserAppRequestSetCursorProc		setCursor;				// 11
	EmBrowserAppRedirNavigationProc			redirNavigation;		// 12

	EmBrowserAppAllowLocalFileURLProc		allowLocalFileURL;		// 13
	long filler[64 - 13];
} EmBrowserAppNotification;

/* ---------- */

// ----- Init Paramter Block -----

typedef struct EmBrowserInitParameterBlock
{
	long majorVersion;								// 1
	long minorVersion;								// 2
	long vendorDataID; 								// IN 3
	long vendorDataMajorVersion; 					// IN 4
	long vendorDataMinorVersion; 					// IN 5
	void *vendorData; 								// IO 6
	AppMallocProc				malloc;				// IN 7
	AppReallocProc				realloc;			// IN 8
	AppFreeProc					free;				// IN 9
	EmBrowserCreateProc			createInstance;		// OUT 10
	EmBrowserDestroyProc		destroyInstance;	// OUT 11
	EmBrowserAppNotification  * notifyCallbacks;	// IN  12
	EmBrowserMethods		  * browserMethods;		// OUT 13, widget creates (to get correct size)
	EmBrowserShutdown			shutdown;			// OUT 14
	void*						libLocation;		// IN 15
	/* API Version: 1.0 (Items 1-15) */
	EmBrowserInitParams			initParams;			// IN 16
	/* API Version: 2.1 (Item 16) */

	void*						writeLocation;		// IN 17

	long filler[64 - 17];

} EmBrowserInitParameterBlock;

// ----- Browser Init -----
// ----------------------------------------------------------------------------
// Description:  Initializes an instance of the library.
typedef EmBrowserStatus (*EmBrowserInitLibraryProc)(EmBrowserInitParameterBlock *initPB);

// ----- APIs ADDED for Windows -----
// Description:  Let Opera process the message, if Opera processes the message return TRUE.
#ifdef WIN32
typedef int	(*EmBrowserProcessOperaMessageProc)(IN MSG* msg);

__declspec(dllexport) int TranslateOperaMessage(MSG* pmsg);

#endif //WIN32
// ----- Browser Method Structure -----

#ifdef __cplusplus
}
#endif

#ifdef WIN32
# pragma pack( pop, before_embrowser_pack )
#endif //WIN32

#endif //!EMBROWSER_INTERFACE_H
