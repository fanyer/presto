#ifndef QuickWidgetManager_H
#define QuickWidgetManager_H

#include "platforms/mac/embrowser/EmBrowserQuick.h"

class QuickWidgetManager
{
public:
	static EmBrowserStatus	CreateQuickMenu(IN const EmBrowserChar * title, IN const char * name, IN EmQuickMenuKind kind, OUT EmQuickMenuRef * menu);
	static EmBrowserStatus	DisposeQuickMenu(IN EmQuickMenuRef menu);
	static EmBrowserStatus	AddQuickMenuItem(IN EmQuickMenuRef menu, IN SInt32 after_item, IN const EmQuickMenuItemInfo *info);
	static EmBrowserStatus	RemoveQuickMenuItem(IN EmQuickMenuRef menu, IN SInt32 item);
	static EmBrowserStatus	InsertQuickMenuInMenuBar(IN EmQuickMenuRef menu, IN EmQuickMenuRef beforeMenu);
	static EmBrowserStatus	RemoveQuickMenuFromMenuBar(IN EmQuickMenuRef menu);

//	static EmBrowserStatus	RefreshMenuBar();
	static EmBrowserStatus	RefreshMenu(IN EmQuickMenuRef menu);
//	static EmBrowserStatus	RefreshCommand(EmQuickMenuCommand command);

	static EmBrowserStatus	RemoveAllQuickMenuItemsFromMenu(IN EmQuickMenuRef menu);

	static void				Notification(IN EmBrowserRef inst, IN EmBrowserAppMessage msg, IN long value);
};

#endif // QuickWidgetManager_H
