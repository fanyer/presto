#ifndef EMBROWSER_QUICK_INTERFACE_H
#define EMBROWSER_QUICK_INTERFACE_H

#include "adjunct/embrowser/EmBrowser.h"
#include "platforms/crashlog/gpu_info.h"

class LaunchManager;

enum {
	emBrowser_app_msg_request_quit				= 3000
};
// Non-public EmBrowserAppMessage (the rest are in EmBrowser.h)

typedef struct OpaqueEmQuickMenu *EmQuickMenuRef;

enum {
	emQuickMenuFlagSeperator	= 0x00000001,	// ignore other data
	emQuickMenuFlagDisabled		= 0x00000002,	// Dim menu item
	emQuickMenuFlagHasMark		= 0x00000010,	// check item
	emQuickMenuFlagUseBulletMark= 0x00000020	// check item using a bullet instead of a checkmark (only used with EmQuickMenuFlagHasMark)
};
typedef UInt32 EmQuickMenuFlags;

typedef UInt32 EmQuickMenuCommand;

enum {
	emQuickMenuKindUnknown		= 0,
	emQuickMenuKindFile			= 1,
	emQuickMenuKindEdit			= 2,
	emQuickMenuKindHelp			= 3,
	emQuickMenuKindQuickPrefs	= 4,
	emQuickMenuKindBookmarks	= 5,
	emQuickMenuKindFolder		= 6,
	emQuickMenuKindGadgets      = 7,
	emQuickMenuKindDock         = 8
};
typedef UInt32 EmQuickMenuKind;

enum {
	// Fake hiMenuEquvs (items that may have to be re-arranged)
	emOperaCommandRegister			= FOUR_CHAR_CODE('OpRe'),	// register
	emOperaCommandNewTab			= FOUR_CHAR_CODE('OpTb'),	// New Tab
	emOperaCommandCheckForUpdate	= FOUR_CHAR_CODE('OpCU'),	// Check updates
	emOperaCommandQuickPreferences	= FOUR_CHAR_CODE('OpQP'),	// Quick Preferences
	emOperaCommandServices			= FOUR_CHAR_CODE('OpSe')	// Services
};

typedef struct EmQuickMenuItemInfo
{
	long majorVersion;
	long minorVersion;

	const EmBrowserChar*	title;		// 3	the name of the item, may be null if the emQuickMenuFlagSeperator flag is set.
	EmQuickMenuFlags		flags;		// 4	flags
	EmQuickMenuCommand		command;	// 5	if non-zero, a valid command for HandleQuickMenuCommandProc or GetQuickMenuCommandStatusProc
	EmQuickMenuRef			submenu;	// 6	if non-sero, use this as a submenu.
	UInt32					hiMenuEquv;	// 7	if non-zero, refers to one of the apple standard commands (kHICommandQuit, kHICommandPreferences, ...)
	UInt32					charCode;	// 8	which key to use in the keyboard equiv.
	UInt32					modifers;	// 9	which modifiers to use in the keyboard equiv. (if charCode != 0)
	void	*				icon;		//10
	SInt16					glyph;		//11	a special glyph to display in the menu (like F1, F2, Esc, etc...)
	long reserved[32-11];
} EmQuickMenuItemInfo;

/* App procs */

typedef EmBrowserStatus	(*CreateQuickMenuProc)(IN const EmBrowserChar * title, IN const char * name, IN EmQuickMenuKind kind, OUT EmQuickMenuRef * menu);
typedef EmBrowserStatus	(*DisposeQuickMenuProc)(IN EmQuickMenuRef menu);
typedef EmBrowserStatus	(*AddQuickMenuItemProc)(IN EmQuickMenuRef menu, IN SInt32 after_item, IN const EmQuickMenuItemInfo *info); //EmBrowserChar * title, IN EmQuickMenuFlags flags, IN EmQuickMenuCommand command, IN EmQuickMenuRef submenu);
typedef EmBrowserStatus	(*RemoveQuickMenuItemProc)(IN EmQuickMenuRef menu, IN SInt32 item);
typedef EmBrowserStatus	(*InsertQuickMenuInMenuBarProc)(IN EmQuickMenuRef menu, IN EmQuickMenuRef beforeMenu);
typedef EmBrowserStatus	(*RemoveQuickMenuFromMenuBarProc)(IN EmQuickMenuRef menu);

typedef EmBrowserStatus	(*RefreshMenuBarProc)();
typedef EmBrowserStatus	(*RefreshMenuProc)(IN EmQuickMenuRef menu);
typedef EmBrowserStatus	(*RefreshCommandProc)(EmQuickMenuCommand command);

typedef EmBrowserStatus (*RemoveAllQuickMenuItemsProc)(IN EmQuickMenuRef menu);

typedef void			(*InstallSighandlers)();

/* Lib procs */

typedef EmBrowserStatus	(*StartupQuickProc)(int argc, const char** argv);
typedef EmBrowserStatus	(*HandleQuickMenuCommandProc)(EmQuickMenuCommand command);
typedef EmBrowserStatus	(*GetQuickMenuCommandStatusProc)(EmQuickMenuCommand command, EmQuickMenuFlags *flags);
typedef EmBrowserStatus (*RebuildQuickMenuProc)(EmQuickMenuRef menu);
typedef EmBrowserStatus (*SharedMenuHitProc)(UInt16 menuID, UInt16 menuItemIndex);
typedef EmBrowserStatus	(*StartupQuickSecondPhaseProc)(int argc, const char** argv);
typedef EmBrowserStatus	(*UpdateMenuTrackingProc)(BOOL tracking);
typedef EmBrowserStatus	(*MenuItemHighlightedProc)(CFStringRef item_title, BOOL main_menu, BOOL is_submenu);
typedef EmBrowserStatus	(*SetActiveNSMenuProc)(void *nsmenu);
typedef EmBrowserStatus	(*OnMenuShownProc)(void *nsmenu, BOOL shown);


typedef struct EmQuickBrowserAppProcs
{
	long majorVersion;									//  1
	long minorVersion;									//  2

	CreateQuickMenuProc				createMenu;			//  3
	DisposeQuickMenuProc			disposeMenu;		//  4
	AddQuickMenuItemProc			addMenuItem;		//  5
	RemoveQuickMenuItemProc			removeMenuItem;		//  6
	InsertQuickMenuInMenuBarProc	insertMenu;			//  7
	RemoveQuickMenuFromMenuBarProc	removeMenu;			//  8

	RefreshMenuBarProc				refreshMenuBar;		//  9
	RefreshMenuProc					refreshMenu;		// 10
	RefreshCommandProc				refreshCommand;		// 11
	RemoveAllQuickMenuItemsProc		removeMenuItems;	// 12

	InstallSighandlers				installSighandlers;	// 13

	long reserved[32-13];
} EmQuickBrowserAppProcs;

typedef struct EmQuickBrowserLibProcs
{
	long majorVersion;
	long minorVersion;

	StartupQuickProc				startupQuick;				//  3
	HandleQuickMenuCommandProc		handleQuickCommand;			//  4
	GetQuickMenuCommandStatusProc	getQuickCommandStatus;		//  5
	RebuildQuickMenuProc			rebuildQuickMenu;			// 	6
	SharedMenuHitProc				sharedMenuHit;				//	7
	StartupQuickSecondPhaseProc		startupQuickSecondPhase;	//	8
	UpdateMenuTrackingProc			updateMenuTracking;			//	9
	MenuItemHighlightedProc			menuItemHighlighted;		// 10
	SetActiveNSMenuProc				setActiveNSMenu;			// 11
	OnMenuShownProc					onMenuShown;				// 12

	long reserved[32-12];
} EmQuickBrowserLibProcs;

typedef struct EmQuickBrowserInitParameterBlock
{
	long majorVersion;
	long minorVersion;

	char					registrationCode[64];	// IN

	EmQuickBrowserAppProcs	*appProcs;				// IN,  19
	EmQuickBrowserLibProcs	*libProcs;				// OUT, 20

	int 			argc;
	const char** 	argv;
#ifdef AUTO_UPDATE_SUPPORT
	LaunchManager *launch_manager;
#else
	void* unused;
#endif
	GpuInfo		*pGpuInfo;
	CFURLRef	opera_bundle;
	long reserved[32-25];
} EmQuickBrowserInitParameterBlock;

#endif // EMBROWSER_QUICK_INTERFACE_H
