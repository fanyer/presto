#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "platforms/mac/quick_support/MacQuickMenu.h"
#include "adjunct/quick/Application.h"
#include "modules/util/adt/opvector.h"

class CocoaInternalQuickMenu : public MacQuickMenu
{
public:
	CocoaInternalQuickMenu(Boolean use_keyboard_context, const char* popup_name, INTPTR menu_value, INT32 start_at_index);
	virtual					~CocoaInternalQuickMenu();
	virtual void			AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action);
	virtual Boolean			IsInternal() const { return true; }
	virtual void			*GetMenu() { return mMenu; }		
	void					Show(const PopupPlacement& placement);
	Boolean					IsKeyboardContext() {return mKeyboardContext;}
	void 					RebuildMenu();
	OpPoint					*GetContextMousePos() { return &m_context_menu_mousepos; }

private:
	void				*mMenu;
	short				mMenuID;
	int					itemCount;
	Boolean				mKeyboardContext;
	Boolean				mIgnoreNextIfSeperator;
	Boolean				mShowIconsInMenu;
	OpString8			mName;
	INTPTR				mMenuValue;
	OpPoint				m_context_menu_mousepos;
};

// Special menu adjusting functions
OP_STATUS LocaliseOperaMenu();
void DisableMenuShortcut(int tag);
