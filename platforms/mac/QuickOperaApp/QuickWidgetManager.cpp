#include "platforms/mac/QuickOperaApp/QuickWidgetManager.h"
#include "platforms/mac/QuickOperaApp/CocoaQuickWidgetMenu.h"
#include "platforms/mac/QuickOperaApp/CocoaApplication.h"

EmBrowserStatus QuickWidgetManager::CreateQuickMenu(IN const EmBrowserChar * title, IN const char * name, IN EmQuickMenuKind kind, OUT EmQuickMenuRef * menu)
{
	*menu = (EmQuickMenuRef) new QuickWidgetMenu((const UniChar *) title, name, kind);
	return (*menu  ? emBrowserNoErr : emBrowserOutOfMemoryErr);
}

EmBrowserStatus QuickWidgetManager::DisposeQuickMenu(IN EmQuickMenuRef menu)
{
	delete (QuickWidgetMenu*) menu;
	return emBrowserNoErr;
}

EmBrowserStatus QuickWidgetManager::AddQuickMenuItem(IN EmQuickMenuRef menu, IN SInt32 after_item, IN const EmQuickMenuItemInfo *info)
{
	((QuickWidgetMenu*) menu)->AddItem(after_item, info);
	return emBrowserNoErr;
}

EmBrowserStatus QuickWidgetManager::RemoveQuickMenuItem(IN EmQuickMenuRef menu, IN SInt32 item)
{
	((QuickWidgetMenu*) menu)->RemoveItem(item);
	return emBrowserNoErr;
}

EmBrowserStatus QuickWidgetManager::RemoveAllQuickMenuItemsFromMenu(IN EmQuickMenuRef menu)
{
	((QuickWidgetMenu*) menu)->RemoveAllItems();
	return emBrowserNoErr;
}

EmBrowserStatus QuickWidgetManager::InsertQuickMenuInMenuBar(IN EmQuickMenuRef menu, IN EmQuickMenuRef beforeMenu)
{
	((QuickWidgetMenu*) menu)->InsertInMenubar((QuickWidgetMenu*) beforeMenu);
	return emBrowserNoErr;
}

EmBrowserStatus QuickWidgetManager::RemoveQuickMenuFromMenuBar(IN EmQuickMenuRef menu)
{
	((QuickWidgetMenu*) menu)->RemoveFromMenubar();
	return emBrowserNoErr;
}

/*
EmBrowserStatus QuickWidgetManager::RefreshMenuBar()
{
	::InvalidateMenuEnabling(NULL);
	return emBrowserNoErr;
}
*/

EmBrowserStatus QuickWidgetManager::RefreshMenu(IN EmQuickMenuRef menu)
{
	((QuickWidgetMenu*) menu)->Refresh();
	return emBrowserNoErr;
}

/*
EmBrowserStatus QuickWidgetManager::RefreshCommand(EmQuickMenuCommand command)
{
	UInt32 matchCount = CountMenuItemsWithCommandID(NULL, command);
	UInt32 index;
	OSStatus err;
	MenuRef menu;
	MenuItemIndex item;
	for (index = 1; index <= matchCount; index++)
	{
		err = GetIndMenuItemWithCommandID(NULL, command, index, &menu, &item);
		if (err == noErr)
		{
			WidgetAppEvents::UpdateQuickMenuItem(menu, item, command);
		}
	}
	return emBrowserNoErr;
}
*/

void QuickWidgetManager::Notification(IN EmBrowserRef inst, IN EmBrowserAppMessage msg, IN long value)
{
	if (msg == emBrowser_app_msg_request_quit)
	{
		ApplicationTerminate();
	}
}
