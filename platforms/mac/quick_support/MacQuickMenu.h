#ifndef MacQuickMenu_H
#define MacQuickMenu_H

//#define _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_
#define USE_SKIN_ICONS_IN_MENUS
#define kMenuIconSize 16	// 16x16 pixels

class OpWidgetImage;
class OpInputAction;

class MacQuickMenu
{
public:
	virtual			~MacQuickMenu() {};
	virtual void	AddItem(BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, MacQuickMenu *submenu, BOOL checked, BOOL selected, BOOL disabled, OpInputAction* input_action) = 0;
	virtual Boolean IsInternal() const = 0;
	virtual void	*GetMenu() { return NULL; }		
};

#endif //MacQuickMenu_H
