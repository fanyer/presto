#ifndef _MAC_HOST_QUICK_MENU_H_
#define _MAC_HOST_QUICK_MENU_H_

#include "platforms/mac/quick_support/MacQuickMenu.h"
#include "platforms/mac/embrowser/EmBrowserQuick.h"
#include "modules/util/adt/opvector.h"

class MacHostQuickMenu;

class MacHostQuickMenuBar : public MacQuickMenu
{
public:
	virtual					~MacHostQuickMenuBar();
	virtual void			AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action);
	virtual Boolean			IsInternal() const { return false; }

	OpInputAction *			FindAction(EmQuickMenuCommand command);
	void					ActionExecuted(EmQuickMenuCommand command);

	void					RebuildMenu(EmQuickMenuRef inMenu);
	void					RebuildAllMenus();
	void					RemoveAllMenus();

#ifdef _MAC_DEBUG
	void					PrintAllMenus();
#endif

protected:
	MacHostQuickMenu*		FindMenu(EmQuickMenuRef inMenu);
	MacHostQuickMenu*		FindMenu(UInt16 inMenuID);

private:
	OpAutoVector<MacHostQuickMenu> mSubmenus;
};

class MacHostQuickMenu : public MacQuickMenu
{
	friend class MacHostQuickMenuBar;
public:
							MacHostQuickMenu(const uni_char* menu_name, const char *menu_type, INTPTR menu_value);
	virtual					~MacHostQuickMenu();
	virtual void			AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action);
	virtual Boolean			IsInternal() const { return false; }

protected:
	EmQuickMenuRef			GetEmMenuRef();
	EmQuickMenuKind			GetMenuKind() { return mMenuKind; }
	time_t					GetModTime() { return mModtime; }
	void					SetModTime(time_t t) { mModtime = t; }
	MacHostQuickMenu*		FindMenu(UInt16 inMenuID);
	MacHostQuickMenu*		FindMenu(EmQuickMenuRef inMenu);
	OpInputAction *			GetAction(UInt16 actionID);
	void					UpdateMenu();
	const char*				GetMenuName();

#ifdef _MAC_DEBUG
	void					PrintAllMenus();
#endif

private:
	static UInt16			sMenuCount;
	EmQuickMenuRef			mEmMenuRef;
	OpVector<OpInputAction> mActions;
	OpAutoVector<MacHostQuickMenu> mSubmenus;
	UInt16					mMenuID;
	EmQuickMenuKind			mMenuKind;
	OpString8				mMenuName;
	INTPTR					mMenuValue;
	time_t					mModtime;
};

extern MacHostQuickMenuBar* gMenuBar;

#endif // _MAC_HOST_QUICK_MENU_H_
