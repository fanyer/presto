#include "adjunct/quick/Application.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/desktop_util/settings/SettingsListener.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

#ifndef _MAC_APPLICATION_LISTENER_H_
#define _MAC_APPLICATION_LISTENER_H_

class MacApplicationListener : public DesktopGlobalApplication, public SettingsListener, public DesktopBookmarkListener
{
public:
	virtual void OnStart();

	virtual void Exit();

	virtual void ExitStarted();

	virtual BOOL OnConfirmExit(BOOL /*exit*/) {return FALSE;}

	virtual void OnHide() {}
	
	virtual void OnShow() {}
	
	virtual void OnStarted();
	
	virtual void OnSettingsChanged(DesktopSettings* settings);

	virtual void OnBookmarkModelLoaded();

	static BOOL s_bookmarks_loaded;
};

class MacApplicationMenuListener : public DesktopPopupMenuHandler
{
public:
	virtual void OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context);
	virtual BOOL OnAddMenuItem(void* menu, BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, BOOL checked, BOOL selected, BOOL disabled, BOOL bold, OpInputAction* input_action);

	virtual void OnAddShellMenuItems(void* menu, const uni_char* path) {}
	virtual void BuildPlatformCustomMenuByName(void* menu, const char* menu_name);

	virtual BOOL ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList &out);
};

#endif // _MAC_APPLICATION_LISTENER_H_

