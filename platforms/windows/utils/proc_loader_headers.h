// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2012 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "platforms/windows_common/utils/proc_loader_headers.h"

//Declaration of types used by LIBRARY_CALL entries below.
//Please, keep track of what uses what. There is no particular order here.

//Used by DrawThemeTextEx and SetWindowThemeAttribute
#include <Uxtheme.h>

//Used by NtSetInformationProcess and NtQuery*
#include "ntstatus.h"

#pragma pack(push, 8)
namespace NtDll {

	enum PROCESS_INFORMATION_CLASS {
		ProcessBasicInformation				=  0,
		ProcessDebugPort					=  7,
		ProcessWow64Information				= 26,
		ProcessImageFileName				= 27,
		ProcessExecuteFlags					= 34,
	};

	enum FILE_INFORMATION_CLASS {
		FileDirectoryInformation			= 1,
		FileFullDirectoryInformation		= 2,
		FileBothDirectoryInformation		= 3,
		FileBasicInformation				= 4,
		FileStandardInformation				= 5,
		FileInternalInformation				= 6,
		FileEaInformation					= 7,
		FileAccessInformation				= 8,
		FileNameInformation					= 9,
		FileRenameInformation				= 10,
		FileLinkInformation					= 11,
		FileNamesInformation				= 12,
		FileDispositionInformation			= 13,
		FilePositionInformation				= 14,
		FileFullEaInformation				= 15,
		FileModeInformation					= 16,
		FileAlignmentInformation			= 17,
		FileAllInformation					= 18,
		FileAllocationInformation			= 19,
		FileEndOfFileInformation			= 20,
		FileAlternateNameInformation		= 21,
		FileStreamInformation				= 22,
		FilePipeInformation					= 23,
		FilePipeLocalInformation			= 24,
		FilePipeRemoteInformation			= 25,
		FileMailslotQueryInformation		= 26,
		FileMailslotSetInformation			= 27,
		FileCompressionInformation			= 28,
		FileObjectIdInformation				= 29,
		FileCompletionInformation			= 30,
		FileMoveClusterInformation			= 31,
		FileQuotaInformation				= 32,
		FileReparsePointInformation			= 33,
		FileNetworkOpenInformation			= 34,
		FileAttributeTagInformation			= 35,
		FileTrackingInformation				= 36,
		FileIdBothDirectoryInformation		= 37,
		FileIdFullDirectoryInformation		= 38,
		FileValidDataLengthInformation		= 39,
		FileShortNameInformation			= 40,
		FileMaximumInformatio				= 41
	};

	enum OBJECT_INFORMATION_CLASS {
		ObjectBasicInformation				= 0,
		ObjectNameInformation				= 1,
		ObjectTypeInformation				= 2,
		ObjectAllTypesInformation			= 3,
		ObjectHandleInformation				= 4,
		ObjectSessionInformation			= 5
	};

	enum SYSTEM_INFORMATION_CLASS {					// Size:
		SystemBasicInformation				= 0,	// 0x002C
		SystemProcessorInformation,					// 0x000C
		SystemPerformanceInformation,				// 0x0138
		SystemTimeOfDayInformation,					// 0x0020
		SystemPathInformation,						// not implemented
		SystemProcessInformation,					// 0x00F8+ per process
		SystemCallInformation,						// 0x0018 + (n * 0x0004)
		SystemConfigurationInformation,				// 0x0018
		SystemProcessorPerformanceInformation,		// 0x0030 per cpu
		SystemGlobalFlag,							// 0x0004
		SystemCallTimeInformation,
		SystemModuleInformation,					// 0x0004 + (n * 0x011C)
		SystemLockInformation,						// 0x0004 + (n * 0x0024)
		SystemStackTraceInformation,
		SystemPagedPoolInformation,					// checked build only
		SystemNonPagedPoolInformation,				// checked build only
		SystemHandleInformation,					// 0x0004 + (n * 0x0010)
		SystemObjectInformation,					// 0x0038+ + (n * 0x0030+)
		SystemPagefileInformation,					// 0x0018+ per page file
		SystemInstemulInformation,					// 0x0088 
		SystemVdmBopInformation,
		SystemCacheInformation,						// 0x0024
		SystemPoolTagInformation,					// 0x0004 + (n * 0x001C)
		SystemProcessorStatistics,					// 0x0000, or 0x0018 per cpu
		SystemDpcInformation,						// 0x0014
		SystemMemoryUsageInformation1,				// checked build only
		SystemLoadImage,							// 0x0018, set mode only
		SystemUnloadImage,							// 0x0004, set mode only
		SystemTimeAdjustmentInformation,			// 0x000C, 0x0008 writeable
		SystemSummaryMemoryInformation,				// checked build only
		SystemMirrorMemoryInformation,				// checked build only
		SystemPerformanceTraceInformation,			// checked build only
		SystemCrashDumpInformation,					// 0x0004
		SystemExceptionInformation,					// 0x0010
		SystemCrashDumpStateInformation,			// 0x0008
		SystemDebuggerInformation,					// 0x0002
		SystemThreadSwitchInformation,				// 0x0030
		SystemRegistryQuotaInformation,				// 0x000C
		SystemLoadDriver,							// 0x0008, set mode only
		SystemPrioritySeparationInformation,     	// 0x0004, set mode only
		SystemVerifierAddDriverInformation,			// not implemented
		SystemVerifierRemoveDriverInformation,		// not implemented
		SystemProcessorIdleInformation,				// invalid info class
		SystemLegacyDriverInformation,				// invalid info class
		SystemTimeZoneInformation,					// 0x00AC
		SystemLookasideInformation,					// n * 0x0020
		SystemSetTimeSlipEvent,						// set mode only [2]
		SystemCreateSession,						// WTS*, set mode only [2]
		SystemDeleteSession,						// WTS*, set mode only [2]
		SystemSessionInformation,					// invalid info class [2]
		SystemRangeStartInformation,				// 0x0004 [2]
		SystemVerifierInformation,					// 0x0068 [2]
		SystemAddVerifier,							// set mode only [2]
		SystemSessionProcessesInformation,			// WTS[1] [2]
		SystemLoadGdiDriverInSystemSpace,
		SystemNumaProcessorMap,
		SystemPrefetcherInformation,
		SystemExtendedProcessInformation,
		SystemRecommendedSharedDataAlignment,
		SystemComPlusPackage,
		SystemNumaAvailableMemory,
		SystemProcessorPowerInformation,
		SystemEmulationBasicInformation,
		SystemEmulationProcessorInformation,
		SystemExtendedHandleInformation,
		SystemLostDelayedWriteInformation,
		SystemBigPoolInformation,
		SystemSessionPoolTagInformation,
		SystemSessionMappedViewInformation,
		SystemHotpatchInformation,
		SystemObjectSecurityMode,
		SystemWatchdogTimerHandler,
		SystemWatchdogTimerInformation,
		SystemLogicalProcessorInformation,
		SystemWow64SharedInformation,
		SystemRegisterFirmwareTableInformationHandler,
		SystemFirmwareTableInformation,
		SystemModuleInformationEx,
		SystemVerifierTriageInformation,
		SystemSuperfetchInformation,
		SystemMemoryListInformation,
		SystemFileCacheInformationEx,
		MaxSystemInfoClass							// MaxSystemInfoClass should always be the
	};

	// [1] Windows Terminal Server
	// [2] Info classes specific to Windows 2000

} // namepspace NtDll

struct IO_STATUS_BLOCK {
	union {
		NTSTATUS			status;
		PVOID				ptr;
	};
	ULONG_PTR				info;
};

//Used by WlanEnumInterfaces
#define WLAN_MAX_NAME_LENGTH 256

typedef enum _WLAN_INTERFACE_STATE
{
	wlan_interface_state_not_ready,
	wlan_interface_state_connected,
	wlan_interface_state_ad_hoc_network_formed,
	wlan_interface_state_disconnecting,
	wlan_interface_state_disconnected,
	wlan_interface_state_associating,
	wlan_interface_state_discovering,
	wlan_interface_state_authenticating
} WLAN_INTERFACE_STATE, *PWLAN_INTERFACE_STATE;

typedef struct _WLAN_INTERFACE_INFO
{
    GUID InterfaceGuid;
    WCHAR strInterfaceDescription[WLAN_MAX_NAME_LENGTH];
    WLAN_INTERFACE_STATE isState;
} WLAN_INTERFACE_INFO, *PWLAN_INTERFACE_INFO;

typedef struct _WLAN_INTERFACE_INFO_LIST
{
    DWORD dwNumberOfItems;
    DWORD dwIndex;
    WLAN_INTERFACE_INFO InterfaceInfo[1];
} WLAN_INTERFACE_INFO_LIST, *PWLAN_INTERFACE_INFO_LIST;

//Used by WlanGetNetworkBssList

#define DOT11_SSID_MAX_LENGTH   32      // 32 bytes
typedef struct _DOT11_SSID
{
    ULONG uSSIDLength;
    UCHAR ucSSID[DOT11_SSID_MAX_LENGTH];
} DOT11_SSID, * PDOT11_SSID;

typedef enum _DOT11_BSS_TYPE
{
    dot11_BSS_type_infrastructure = 1,
    dot11_BSS_type_independent = 2,
    dot11_BSS_type_any = 3
} DOT11_BSS_TYPE, * PDOT11_BSS_TYPE;

typedef enum _DOT11_PHY_TYPE 
{
    dot11_phy_type_unknown = 0,
    dot11_phy_type_any = dot11_phy_type_unknown,
    dot11_phy_type_fhss = 1,
    dot11_phy_type_dsss = 2,
    dot11_phy_type_irbaseband = 3,
    dot11_phy_type_ofdm = 4,
    dot11_phy_type_hrdsss = 5,
    dot11_phy_type_erp = 6,
    dot11_phy_type_IHV_start = 0x80000000,
    dot11_phy_type_IHV_end = 0xffffffff
} DOT11_PHY_TYPE, * PDOT11_PHY_TYPE;

#define DOT11_RATE_SET_MAX_LENGTH               126 // 126 bytes

typedef struct _WLAN_RATE_SET
{
    ULONG uRateSetLength;
    USHORT usRateSet[DOT11_RATE_SET_MAX_LENGTH];
} WLAN_RATE_SET, * PWLAN_RATE_SET;

typedef UCHAR DOT11_MAC_ADDRESS[6];

typedef struct _WLAN_BSS_ENTRY 
{
    DOT11_SSID dot11Ssid;
    ULONG uPhyId;
    DOT11_MAC_ADDRESS dot11Bssid;
    DOT11_BSS_TYPE dot11BssType;
    DOT11_PHY_TYPE dot11BssPhyType;
    LONG lRssi;
    ULONG uLinkQuality;
    BOOLEAN bInRegDomain;
    USHORT usBeaconPeriod;
    ULONGLONG ullTimestamp;
    ULONGLONG ullHostTimestamp;
    USHORT usCapabilityInformation;
    ULONG  ulChCenterFrequency;
    WLAN_RATE_SET wlanRateSet;
    // the beginning of the IE blob
    // the offset is w.r.t. the beginning of the entry
    ULONG ulIeOffset;
    // size of the IE blob
    ULONG ulIeSize;
} WLAN_BSS_ENTRY, * PWLAN_BSS_ENTRY;

typedef struct _WLAN_BSS_LIST
{
    // The total size of the data in BYTE
    DWORD dwTotalSize;
    DWORD dwNumberOfItems;
    WLAN_BSS_ENTRY wlanBssEntries[1];
} WLAN_BSS_LIST, *PWLAN_BSS_LIST;

#pragma pack(pop)

//Used by RegisterPowerSettingNotification and UnegisterPowerSettingNotification
#ifndef HPOWERNOTIFY
typedef  PVOID           HPOWERNOTIFY;
typedef  HPOWERNOTIFY   *PHPOWERNOTIFY;
#endif // HPOWERNOTIFY

// Used by GetTouchInputInfo and CloseTouchHandle
#if(WINVER < 0x0601)
/*
 * Gesture defines and functions
 */

/*
 * Gesture information handle
 */
DECLARE_HANDLE(HGESTUREINFO);

/*
 * Gesture flags - GESTUREINFO.dwFlags
 */
#define GF_BEGIN                        0x00000001
#define GF_INERTIA                      0x00000002
#define GF_END                          0x00000004

/*
 * Gesture IDs
 */
#define GID_BEGIN                       1
#define GID_END                         2
#define GID_ZOOM                        3
#define GID_PAN                         4
#define GID_ROTATE                      5
#define GID_TWOFINGERTAP                6
#define GID_PRESSANDTAP                 7
#define GID_ROLLOVER                    GID_PRESSANDTAP

/*
 * Gesture information structure
 *   - Pass the HGESTUREINFO received in the WM_GESTURE message lParam into the
 *     GetGestureInfo function to retrieve this information.
 *   - If cbExtraArgs is non-zero, pass the HGESTUREINFO received in the WM_GESTURE
 *     message lParam into the GetGestureExtraArgs function to retrieve extended
 *     argument information.
 */
typedef struct tagGESTUREINFO {
    UINT cbSize;                    // size, in bytes, of this structure (including variable length Args field)
    DWORD dwFlags;                  // see GF_* flags
    DWORD dwID;                     // gesture ID, see GID_* defines
    HWND hwndTarget;                // handle to window targeted by this gesture
    POINTS ptsLocation;             // current location of this gesture
    DWORD dwInstanceID;             // internally used
    DWORD dwSequenceID;             // internally used
    ULONGLONG ullArguments;         // arguments for gestures whose arguments fit in 8 BYTES
    UINT cbExtraArgs;               // size, in bytes, of extra arguments, if any, that accompany this gesture
} GESTUREINFO, *PGESTUREINFO;
typedef GESTUREINFO const * PCGESTUREINFO;


/*
 * Gesture argument helpers
 *   - Angle should be a double in the range of -2pi to +2pi
 *   - Argument should be an unsigned 16-bit value
 */
#define GID_ROTATE_ANGLE_TO_ARGUMENT(_arg_)     ((USHORT)((((_arg_) + 2.0 * 3.14159265) / (4.0 * 3.14159265)) * 65535.0))
#define GID_ROTATE_ANGLE_FROM_ARGUMENT(_arg_)   ((((double)(_arg_) / 65535.0) * 4.0 * 3.14159265) - 2.0 * 3.14159265)

/*
 * Gesture configuration structure
 *   - Used in SetGestureConfig and GetGestureConfig
 *   - Note that any setting not included in either GESTURECONFIG.dwWant or
 *     GESTURECONFIG.dwBlock will use the parent window's preferences or
 *     system defaults.
 */
typedef struct tagGESTURECONFIG {
    DWORD dwID;                     // gesture ID
    DWORD dwWant;                   // settings related to gesture ID that are to be turned on
    DWORD dwBlock;                  // settings related to gesture ID that are to be turned off
} GESTURECONFIG, *PGESTURECONFIG;

/*
 * Gesture configuration flags - GESTURECONFIG.dwWant or GESTURECONFIG.dwBlock
 */

/*
 * Common gesture configuration flags - set GESTURECONFIG.dwID to zero
 */
#define GC_ALLGESTURES                              0x00000001

/*
 * Zoom gesture configuration flags - set GESTURECONFIG.dwID to GID_ZOOM
 */
#define GC_ZOOM                                     0x00000001

/*
 * Pan gesture configuration flags - set GESTURECONFIG.dwID to GID_PAN
 */
#define GC_PAN                                      0x00000001
#define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY        0x00000002
#define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY      0x00000004
#define GC_PAN_WITH_GUTTER                          0x00000008
#define GC_PAN_WITH_INERTIA                         0x00000010

/*
 * Rotate gesture configuration flags - set GESTURECONFIG.dwID to GID_ROTATE
 */
#define GC_ROTATE                                   0x00000001

/*
 * Two finger tap gesture configuration flags - set GESTURECONFIG.dwID to GID_TWOFINGERTAP
 */
#define GC_TWOFINGERTAP                             0x00000001

/*
 * PressAndTap gesture configuration flags - set GESTURECONFIG.dwID to GID_PRESSANDTAP
 */
#define GC_PRESSANDTAP                              0x00000001
#define GC_ROLLOVER                                 GC_PRESSANDTAP

#define GESTURECONFIGMAXCOUNT           256             // Maximum number of gestures that can be included
                                                        // in a single call to SetGestureConfig / GetGestureConfig

#endif // (WINVER < 0x0601)

//Used by SHGetStockIconInfo
typedef struct _SHSTOCKICONINFO
{
    DWORD cbSize;
    HICON hIcon;
    int   iSysImageIndex;
    int   iIcon;
    WCHAR szPath[MAX_PATH];
} SHSTOCKICONINFO;

typedef enum SHSTOCKICONID
{
    SIID_DOCNOASSOC = 0,          // document (blank page), no associated program
    SIID_DOCASSOC = 1,            // document with an associated program
    SIID_APPLICATION = 2,         // generic application with no custom icon
    SIID_FOLDER = 3,              // folder (closed)
    SIID_FOLDEROPEN = 4,          // folder (open)
    SIID_DRIVE525 = 5,            // 5.25" floppy disk drive
    SIID_DRIVE35 = 6,             // 3.5" floppy disk drive
    SIID_DRIVEREMOVE = 7,         // removable drive
    SIID_DRIVEFIXED = 8,          // fixed (hard disk) drive
    SIID_DRIVENET = 9,            // network drive
    SIID_DRIVENETDISABLED = 10,   // disconnected network drive
    SIID_DRIVECD = 11,            // CD drive
    SIID_DRIVERAM = 12,           // RAM disk drive
    SIID_WORLD = 13,              // entire network
    SIID_SERVER = 15,             // a computer on the network
    SIID_PRINTER = 16,            // printer
    SIID_MYNETWORK = 17,          // My network places
    SIID_FIND = 22,               // Find
    SIID_HELP = 23,               // Help
    SIID_SHARE = 28,              // overlay for shared items
    SIID_LINK = 29,               // overlay for shortcuts to items
    SIID_SLOWFILE = 30,           // overlay for slow items
    SIID_RECYCLER = 31,           // empty recycle bin
    SIID_RECYCLERFULL = 32,       // full recycle bin
    SIID_MEDIACDAUDIO = 40,       // Audio CD Media
    SIID_LOCK = 47,               // Security lock
    SIID_AUTOLIST = 49,           // AutoList
    SIID_PRINTERNET = 50,         // Network printer
    SIID_SERVERSHARE = 51,        // Server share
    SIID_PRINTERFAX = 52,         // Fax printer
    SIID_PRINTERFAXNET = 53,      // Networked Fax Printer
    SIID_PRINTERFILE = 54,        // Print to File
    SIID_STACK = 55,              // Stack
    SIID_MEDIASVCD = 56,          // SVCD Media
    SIID_STUFFEDFOLDER = 57,      // Folder containing other items
    SIID_DRIVEUNKNOWN = 58,       // Unknown drive
    SIID_DRIVEDVD = 59,           // DVD Drive
    SIID_MEDIADVD = 60,           // DVD Media
    SIID_MEDIADVDRAM = 61,        // DVD-RAM Media
    SIID_MEDIADVDRW = 62,         // DVD-RW Media
    SIID_MEDIADVDR = 63,          // DVD-R Media
    SIID_MEDIADVDROM = 64,        // DVD-ROM Media
    SIID_MEDIACDAUDIOPLUS = 65,   // CD+ (Enhanced CD) Media
    SIID_MEDIACDRW = 66,          // CD-RW Media
    SIID_MEDIACDR = 67,           // CD-R Media
    SIID_MEDIACDBURN = 68,        // Burning CD
    SIID_MEDIABLANKCD = 69,       // Blank CD Media
    SIID_MEDIACDROM = 70,         // CD-ROM Media
    SIID_AUDIOFILES = 71,         // Audio files
    SIID_IMAGEFILES = 72,         // Image files
    SIID_VIDEOFILES = 73,         // Video files
    SIID_MIXEDFILES = 74,         // Mixed files
    SIID_FOLDERBACK = 75,         // Folder back
    SIID_FOLDERFRONT = 76,        // Folder front
    SIID_SHIELD = 77,             // Security shield. Use for UAC prompts only.
    SIID_WARNING = 78,            // Warning
    SIID_INFO = 79,               // Informational
    SIID_ERROR = 80,              // Error
    SIID_KEY = 81,                // Key / Secure
    SIID_SOFTWARE = 82,           // Software
    SIID_RENAME = 83,             // Rename
    SIID_DELETE = 84,             // Delete
    SIID_MEDIAAUDIODVD = 85,      // Audio DVD Media
    SIID_MEDIAMOVIEDVD = 86,      // Movie DVD Media
    SIID_MEDIAENHANCEDCD = 87,    // Enhanced CD Media
    SIID_MEDIAENHANCEDDVD = 88,   // Enhanced DVD Media
    SIID_MEDIAHDDVD = 89,         // HD-DVD Media
    SIID_MEDIABLURAY = 90,        // BluRay Media
    SIID_MEDIAVCD = 91,           // VCD Media
    SIID_MEDIADVDPLUSR = 92,      // DVD+R Media
    SIID_MEDIADVDPLUSRW = 93,     // DVD+RW Media
    SIID_DESKTOPPC = 94,          // desktop computer
    SIID_MOBILEPC = 95,           // mobile computer (laptop/notebook)
    SIID_USERS = 96,              // users
    SIID_MEDIASMARTMEDIA = 97,    // Smart Media
    SIID_MEDIACOMPACTFLASH = 98,  // Compact Flash
    SIID_DEVICECELLPHONE = 99,    // Cell phone
    SIID_DEVICECAMERA = 100,      // Camera
    SIID_DEVICEVIDEOCAMERA = 101, // Video camera
    SIID_DEVICEAUDIOPLAYER = 102, // Audio player
    SIID_NETWORKCONNECT = 103,    // Connect to network
    SIID_INTERNET = 104,          // Internet
    SIID_ZIPFILE = 105,           // ZIP file
    SIID_SETTINGS = 106,          // Settings
    // 107-131 are internal Vista RTM icons
    // 132-159 for SP1 icons
    SIID_DRIVEHDDVD = 132,        // HDDVD Drive (all types)
    SIID_DRIVEBD = 133,           // BluRay Drive (all types)
    SIID_MEDIAHDDVDROM = 134,     // HDDVD-ROM Media
    SIID_MEDIAHDDVDR = 135,       // HDDVD-R Media
    SIID_MEDIAHDDVDRAM = 136,     // HDDVD-RAM Media
    SIID_MEDIABDROM = 137,        // BluRay ROM Media
    SIID_MEDIABDR = 138,          // BluRay R Media
    SIID_MEDIABDRE = 139,         // BluRay RE Media (Rewriable and RAM)
    SIID_CLUSTEREDDRIVE = 140,    // Clustered disk
    // 160+ are for Windows 7 icons
    SIID_MAX_ICONS = 174,
} SHSTOCKICONID;

#define SHGSI_ICON              SHGFI_ICON
#define SHGSI_SMALLICON         SHGFI_SMALLICON

//Used by SHOpenWithDialog
#define OAIF_HIDE_REGISTRATION 0x00000020   // hide the "always use this file" checkbox
#define OAIF_URL_PROTOCOL 0x00000040        // the "extension" passed is actually a protocol, and open with should show apps registered as capable of handling that protocol


#if defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)
//Used by SymFromAddr and SymGetLineFromAddr64
#include <Dbghelp.h>
#endif //defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)

//Please, keep this list ordered:
//The libraries appear in alphabetical order and procedures loaded from them appear under the corresponding library, also sorted alphabetically

//advapi32
LIBRARY_CALL(advapi32, BOOL, WINAPI, CreateProcessWithTokenW, (HANDLE hToken, DWORD dwLogonFlags, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInfo), (hToken, dwLogonFlags, lpApplicationName, lpCommandLine, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInfo), FALSE);

#if defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)
//dbghelp
LIBRARY_CALL(dbghelp, BOOL, WINAPI, SymFromAddr, (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol), (hProcess, Address, Displacement, Symbol), FALSE);
LIBRARY_CALL(dbghelp, BOOL, WINAPI, SymGetLineFromAddr64, (HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line), (hProcess, dwAddr, pdwDisplacement, Line), FALSE);
LIBRARY_CALL(dbghelp, BOOL, WINAPI, SymInitialize, (HANDLE hProcess, PCTSTR UserSearchPath, BOOL fInvadeProcess), (hProcess, UserSearchPath, fInvadeProcess), FALSE);
LIBRARY_CALL(dbghelp, DWORD, WINAPI, SymSetOptions, (DWORD SymOptions), (SymOptions), -1);
#endif //defined(MEMTOOLS_ENABLE_CODELOC) && defined(MEMTOOLS_NO_DEFAULT_CODELOC) && defined(MEMORY_LOG_USAGE_PER_ALLOCATION_SITE)

//dwmapi
LIBRARY_CALL(dwmapi, BOOL, WINAPI, DwmDefWindowProc, (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *plResult), (hwnd, msg, wParam, lParam, plResult), FALSE);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmExtendFrameIntoClientArea, (HWND hWnd, const MARGINS *pMarInset), (hWnd, pMarInset), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmInvalidateIconicBitmaps, (HWND hwnd), (hwnd), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmSetIconicLivePreviewBitmap, (HWND hwnd, HBITMAP hbmp, POINT *pptClient, DWORD dwSITFlags), (hwnd, hbmp, pptClient, dwSITFlags), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmSetIconicThumbnail, (HWND hwnd, HBITMAP hbmp, DWORD dwSITFlags), (hwnd, hbmp, dwSITFlags), E_NOTIMPL);
LIBRARY_CALL(dwmapi, HRESULT, WINAPI, DwmSetWindowAttribute, (HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute), (hwnd,	dwAttribute, pvAttribute, cbAttribute), E_NOTIMPL);

//kernel32
LIBRARY_CALL(kernel32, int, WINAPI, GetSystemDefaultLocaleName, (LPWSTR lpLocaleName, int cchLocaleName), (lpLocaleName, cchLocaleName), 0);
LIBRARY_CALL(kernel32, BOOL, WINAPI, SetProcessDEPPolicy, (DWORD dwFlags), (dwFlags), FALSE);

//ntdll
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQueryInformationFile, (HANDLE FileHandle, IO_STATUS_BLOCK* IoStatusBlock, PVOID FileInformation, ULONG Length, NtDll::FILE_INFORMATION_CLASS FileInformationClass), (FileHandle, IoStatusBlock, FileInformation, Length, FileInformationClass), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQueryInformationProcess, (HANDLE ProcessHandle, NtDll::PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength), (ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQueryObject, (HANDLE Handle, NtDll::OBJECT_INFORMATION_CLASS ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength), (Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength, ReturnLength), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtQuerySystemInformation, (NtDll::SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength), (SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength), NT_STATUS_NOT_IMPLEMENTED);
LIBRARY_CALL(ntdll, NTSTATUS, WINAPI, NtSetInformationProcess, (HANDLE ProcessHandle, NtDll::PROCESS_INFORMATION_CLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength), (ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength), NT_STATUS_NOT_IMPLEMENTED);

//shell32
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHCreateItemFromParsingName, (PCWSTR pszPath, IBindCtx *pbc, REFIID riid, void **ppv), (pszPath, pbc, riid, ppv), E_NOTIMPL);
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHGetKnownFolderPath, (REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, __out PWSTR *ppszPath), (rfid, dwFlags, hToken, ppszPath), E_NOTIMPL);
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHGetStockIconInfo, (SHSTOCKICONID siid, UINT uFlags, __inout SHSTOCKICONINFO *psii), (siid, uFlags, psii), E_NOTIMPL);
LIBRARY_CALL(shell32, HRESULT, WINAPI, SHOpenWithDialog, (HWND hwndParent, const OPENASINFO *poainfo), (hwndParent, poainfo), E_NOTIMPL);

//user32
LIBRARY_CALL(user32, BOOL, WINAPI, CloseGestureInfoHandle, (HGESTUREINFO hGestureInfo), (hGestureInfo), FALSE);
LIBRARY_CALL(user32, BOOL, WINAPI, GetGestureInfo, (HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo), (hGestureInfo, pGestureInfo), FALSE);
LIBRARY_CALL(user32, HPOWERNOTIFY, WINAPI, RegisterPowerSettingNotification, (HANDLE hRecipient, LPCGUID PowerSettingGuid, DWORD Flags), (hRecipient, PowerSettingGuid, Flags), NULL);
LIBRARY_CALL(user32, BOOL,  WINAPI, SetGestureConfig, (HWND hwnd, DWORD dwReserved, UINT cIDs, PGESTURECONFIG pGestureConfig, UINT cbSize), (hwnd, dwReserved, cIDs, pGestureConfig, cbSize), FALSE);
LIBRARY_CALL(user32, BOOL, WINAPI, ShutdownBlockReasonCreate, (HWND hWnd, LPCWSTR pwszReason), (hWnd, pwszReason), FALSE);
LIBRARY_CALL(user32, HPOWERNOTIFY, WINAPI, UnregisterPowerSettingNotification, (HPOWERNOTIFY Handle), (Handle), 0);

//uxtheme
LIBRARY_CALL(uxtheme, HRESULT, WINAPI, DrawThemeTextEx, (HTHEME theme, HDC hdc, int part, int state, LPCWSTR str, int len, DWORD dwFlags, LPRECT pRect, const DTTOPTS *pOptions), (theme, hdc, part, state, str, len, dwFlags, pRect, pOptions), E_NOTIMPL);
LIBRARY_CALL(uxtheme, HRESULT, WINAPI, SetWindowThemeAttribute, (HWND hwnd, enum WINDOWTHEMEATTRIBUTETYPE eAttribute, __in_bcount(cbAttribute) PVOID pvAttribute, DWORD cbAttribute), (hwnd, eAttribute, pvAttribute, cbAttribute), E_NOTIMPL);
LIBRARY_CALL(uxtheme, BOOL, WINAPI, BeginPanningFeedback, (HWND hwnd), (hwnd), FALSE);
LIBRARY_CALL(uxtheme, BOOL, WINAPI, EndPanningFeedback, (HWND hwnd, BOOL fAnimateBack), (hwnd, fAnimateBack), FALSE);
LIBRARY_CALL(uxtheme, BOOL, WINAPI, UpdatePanningFeedback, (HWND hwnd, LONG lTotalOverpanOffsetX, LONG lTotalOverpanOffsetY, BOOL fInInertia), (hwnd, lTotalOverpanOffsetX, lTotalOverpanOffsetY, fInInertia), FALSE);

//wlanapi
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanCloseHandle, (HANDLE hClientHandle, PVOID pReserved), (hClientHandle, pReserved), ERROR_NOT_SUPPORTED);
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanEnumInterfaces, (HANDLE hClientHandle, PVOID pReserved, PWLAN_INTERFACE_INFO_LIST *ppInterfaceList), (hClientHandle, pReserved, ppInterfaceList), ERROR_NOT_SUPPORTED);
LIBRARY_CALL(wlanapi, VOID, WINAPI, WlanFreeMemory, (PVOID pMemory), (pMemory), );
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanGetNetworkBssList, (HANDLE hClientHandle, CONST GUID *pInterfaceGuid, CONST PDOT11_SSID pDot11Ssid, DOT11_BSS_TYPE dot11BssType, BOOL bSecurityEnabled, PVOID pReserved, PWLAN_BSS_LIST *ppWlanBssList), (hClientHandle, pInterfaceGuid, pDot11Ssid, dot11BssType, bSecurityEnabled, pReserved, ppWlanBssList), ERROR_NOT_SUPPORTED);
LIBRARY_CALL(wlanapi, DWORD, WINAPI, WlanOpenHandle, (DWORD dwClientVersion, PVOID pReserved, PDWORD pdwNegotiatedVersion, PHANDLE phClientHandle), (dwClientVersion, pReserved, pdwNegotiatedVersion, phClientHandle), ERROR_INVALID_FUNCTION);
