#ifndef WIDGET_LIB_HANDLER_H
#define WIDGET_LIB_HANDLER_H

#include "platforms/mac/embrowser/EmBrowserQuick.h"

//	Characters to remvove to make the key valid.
#define KEY_STRIPCHARS			"}{[]?:%;+#!)&*ØÆ@,!" //"}{[]?:%;+#!)&*øæåØÆÅ@,!§@"
//	QuickApp Opera 7 embedded license key.
#define	KEY_PRIMARY_NAME		"&?m[);+-e[%kA[Vf%[-[J:[@VY[3%n{-K@!VV+ND@-%*A#J{J%}T%%A-,8At%Jf%"
/** A class that handles all the proc pointers etc. to the widget library.
  * All call to the library goes through here.
  */

class QuickWidgetLibHandler
{
public:
	QuickWidgetLibHandler(int argc, const char** argv);
	~QuickWidgetLibHandler();

	Boolean				Initialized();

	EmBrowserStatus		CallStartupQuickMethod(int argc, const char** argv);
	EmBrowserStatus		CallStartupQuickSecondPhaseMethod(int argc, const char** argv);
	EmBrowserStatus		CallHandleQuickMenuCommandMethod(EmQuickMenuCommand command);
	EmBrowserStatus		CallGetQuickMenuCommandStatusMethod(EmQuickMenuCommand command, EmQuickMenuFlags &flags);
	EmBrowserStatus		CallRebuildQuickMenuMethod(EmQuickMenuRef menu);
	EmBrowserStatus 	CallSharedMenuHitMethod(UInt16 menuID, UInt16 menuItemIndex);
	EmBrowserStatus 	CallUpdateMenuTracking(BOOL tracking);
	EmBrowserStatus 	CallMenuItemHighlighted(CFStringRef item_title, BOOL main_menu, BOOL is_submenu);
	EmBrowserStatus 	CallSetActiveNSMenu(void *nsmenu);
	EmBrowserStatus 	CallOnMenuShown(void *nsmenu, BOOL shown);

	// Helper function to reuse code for extra processes
	static CFBundleRef	LoadOperaFramework(CFBundleRef main_bundle);

private:
	Boolean				InitWidgetLibrary();

	Boolean								success;

	EmBrowserInitLibraryProc			initProc;

	EmBrowserInitParameterBlock			commonParams;
	EmQuickBrowserInitParameterBlock	quickParams;
};

extern QuickWidgetLibHandler *gWidgetLibHandler;

#endif
