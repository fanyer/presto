
#include "core/pch.h"

#ifndef MENU_BAR_H
#define MENU_BAR_H

#include "oui.h"

#include "adjunct/quick/Application.h"
#include "adjunct/desktop_pi/desktoppopupmenuhandler.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/accessibility/opaccessibleitem.h"
#endif

class PrefsFile;
class OpWidgetPainter;
class VisualDevice;
class OUIMenuManager;

extern OUIMenuManager*			g_windows_menu_manager;
extern HINSTANCE				hInst;

/***********************************************************************************
**
**	OUIMenuManager
**
***********************************************************************************/

#define MAX_MENUBAR_ITEMS	16

class OUIMenuBar;
class OUIMenuBarItem;
class WindowsOpWindow;

class OUIMenuManager : public DesktopPopupMenuHandler
{
	public:
										OUIMenuManager();
		virtual							~OUIMenuManager();

		HWND							GetActiveMenuOwner()	{return m_active_menu_owner;}
		OUIMenuBar*						GetActiveMenuBar()		{return m_active_menu_bar;}
		OUIMenuBarItem*					GetActiveMenuBarItem()	{return m_active_menu_bar_item;}

		void							CreateMenuBar(WindowsOpWindow* op_window, const char* menu_bar_name);
		void							DestroyMenuBar(WindowsOpWindow* op_window = NULL);
		int								ShowMenuBarItemRect(OUIMenuBar* menu_bar, OUIMenuBarItem* menu_bar_item, RECT* rect, HWND owner, BOOL keyboard_activated, BOOL return_command = FALSE);

		// Scope
		void                            OnMenuShown(WPARAM wParam,INT32 lParam);
		void                            OnMenuClosed(WPARAM wParam);

		void                            CancelAltKey();

		void							RemoveMenuItemData(HMENU menu, UINT pos);
		BOOL							OnInitPopupMenu(HWND hwnd, HMENU menu, INTPTR pos_to_init);
		BOOL							OnMenuCommand(INT32 command);
		BOOL							OnMeasureItem(HWND hwnd, MEASUREITEMSTRUCT* measure_item);
		BOOL							OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* draw_item);
		INT32							OnMenuChar(uni_char letter, HMENU menu);
		BOOL							OnMenuButtonUp(POINT pt);

		// Implementing DesktopPopupMenuHandler
		virtual void					OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context);
		virtual BOOL					OnAddMenuItem(void* menu, BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, BOOL checked, BOOL selected, BOOL disabled, BOOL bold, OpInputAction* input_action);
		virtual void					OnAddShellMenuItems(void* menu, const uni_char* path);
		virtual BOOL					OnAddBreakMenuItem();
		virtual void					OnForceShortcutUnderline() { m_force_shortcut_underline = TRUE; }
		virtual BOOL					ListPopupMenuInfo(OpScopeDesktopWindowManager_SI::QuickMenuList &out);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		void							UseScreenReaderMenus(BOOL use) {m_screen_reader_menus = use;}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		static int						GetMenuItemHeight();

		OUIMenuBar*						m_alt_down_bar;
		BOOL							m_opening_menu;

	private:
		class MenuItemData;
		class MenuStackItem;

		// Disable menu animation for OpenGL backend as it cause flickers, restore when we're done
		void							DisableMenuAnimationParameter();
		void							RestoreMenuAnimationParameter();

		void							CreateScreenReaderCompatibleMenuBar(HWND parent, const char* menu_bar_name);

		void							RecreateMenuBars();

		BOOL							SetAltKey(BOOL down);
        BOOL                            SetKeyDown(int key);

		MenuItemData*					GetMenuItemData(HMENU menu, INT32 pos, BOOL by_position);
		void							DestroyMenuItems(HMENU menu);

		void							InitPopupMenuFromName(HMENU menu, const char* menu_name, INTPTR menu_value, INT32 menu_start, UINT32 stack_pos);

		// This function also searches sub menus which ::MenuItemFromPoint doesn't do
		MenuItemData*					GetMenuItemByLocation(HMENU menu, POINT pt);

		void							ShowBookmarkContextMenu(MenuItemData* item);

		static LRESULT CALLBACK			StaticMsgHook(int code, WPARAM wparam, LPARAM lparam);
		LRESULT CALLBACK				MsgHook(int code, WPARAM wparam, LPARAM lparam);
		static LRESULT CALLBACK			StaticKeyboardHook(int code, WPARAM wparam, LPARAM lparam);
		LRESULT CALLBACK				KeyboardHook(int code, WPARAM wparam, LPARAM lparam);

		INT32							GetSelectedMenuItem(HMENU menu);
		OP_STATUS						SetQuickMenuInfo(HMENU menu, HWND menu_owner, const char* menu_name, OpScopeDesktopWindowManager_SI::QuickMenuInfo& info);

		static const int				SUBMENU_ARROW_WIDTH = 16;
		static const int				IMAGE_TEXT_OFFSET = 10;
		static const int				SEPARATOR_HEIGHT = 10;

		/**
		 * Information about an "in-page" pop-up menu, that is, a pop-up menu that is not opened from
		 * the main menu bar.
		 * We need to store them here since the menu listing mechanism only cares about the menus opened
		 * from the main menu bar.
		 */
		HMENU							m_inpage_popup_menu;
		OpString8						m_inpage_popup_menu_name;

		HMENU							m_active_menu;
		HWND							m_active_menu_owner;
		OUIMenuBar*						m_active_menu_bar;
		OUIMenuBarItem*					m_active_menu_bar_item;
		HHOOK							m_hook;
		HHOOK							m_keyboard_hook;
		BOOL							m_activate_first_item;
		BOOL							m_leftmost;
		BOOL							m_rightmost;
		INT								m_popup_counter;
		INT								m_eat_mousemove;
		INT								m_id_counter;

		Head							m_menu_bars;
		BOOL							m_alt_key_down;
		BOOL							m_screen_reader_menus;

		HMENU							m_current_root;
		HMENU							m_popup_root;
		const char*						m_popup_root_name;
		INTPTR							m_popup_root_value;
		HMENU							m_menubar_roots[MAX_MENUBAR_ITEMS];

		OpVector<MenuStackItem>			m_menu_stack;
		OpHashTable						m_item_data_hashtable;

		OpInputContext*					m_menu_input_context;
		BOOL							m_use_keyboard_context;
		OpPoint							m_context_menu_mousepos;
		OpPoint							m_context_menu_pos; // the position where a popup menu will be/is shown. differen with m_context_menu_mousepos when the action is activated by keyboard.
		BOOL							m_menu_break;
		BOOL							m_force_shortcut_underline;
		UINT32							m_max_width;			// used during measurement of the menu, reset to 0 in draw
		UINT32							m_menu_item_count;		// number of items on this particular column (after a break)
		UINT32							m_menu_height;			// height of items on this particular column (after a break)
		BOOL							m_use_flat_menus;					// TRUE if we are using flat menus

		OpRect							m_avoid_rect;

		IContextMenu2					*m_pIContextMenu2;
		XFORM							m_rtl_item_dc_transform;

		BOOL							m_menu_parameters_changed;
		BOOL							m_old_menu_animation;
		BOOL							m_old_menu_selection_fade;

		/**
		 * Get information about the given menu and its parent menus.
		 * Append the information to the out list.
		 *
		 * @param menu The menu we're interested in
		 * @menu_name The name of the menu, we can't retrieve it from the OS
		 * @out The menu list that will be eventually sent to operadriver
		 *
		 * @return OpStatus::OK if everything we OK, or error code otherwise.
		 */
		OP_STATUS GetAllMenuInfo(HMENU menu, const char* menu_name, OpScopeDesktopWindowManager_SI::QuickMenuList &out);
};

/***********************************************************************************
**
**	OUIMenuBar
**
***********************************************************************************/

class OUIMenuBar : public OUIWindow
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				, public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
	public:

										OUIMenuBar(OUIMenuManager* menubar_manager, HWND parent, const char* menu_bar_name);
		virtual							~OUIMenuBar();


		void							RestoreFocus();
		void							SetFocus();
		void							ArrangeMenubar(RECT *rect);

		BOOL                            SetAltKey(BOOL down);
        void                            CancelAltKey();
		BOOL							SetKeyDown(int key);
		void							SetSkin(const char* skin) 
										{
											m_skin = skin;
										}

		SIZE							GetSize(RECT* rect);
						
		virtual LRESULT					DefaultWindowProc
										(
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										);
		
										OUI_DECLARE_ON_MESSAGE(OnPaint);
										OUI_DECLARE_ON_MESSAGE(OnMouseMove);
										OUI_DECLARE_ON_MESSAGE(OnLButtonDown);
										OUI_DECLARE_ON_MESSAGE(OnChar);
										OUI_DECLARE_ON_MESSAGE(OnKeyDown);
										OUI_DECLARE_ON_MESSAGE(OnSetFocus);
										OUI_DECLARE_ON_MESSAGE(OnKillFocus);
										OUI_DECLARE_ON_MESSAGE(OnDrawItem);
										OUI_DECLARE_ON_MESSAGE(OnCommand);
										OUI_DECLARE_ON_MESSAGE(OnCtlColorBtn);
										OUI_DECLARE_ON_MESSAGE(OnLButtonDblClk);								

		void							LayoutItems();

		OUIMenuBarItem*					GetMenuBarItemFromPoint(int x, int y);

		void							SetHoverItem(OUIMenuBarItem* item);

		void							CycleFocus(BOOL forward);

		static LRESULT CALLBACK			StaticSubclassedButtonWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

		static WNDPROC					s_old_button_wndproc;

		void							UpdateMDIButtons(WindowsOpWindow* mdi_child);

		DesktopWindow*					GetActiveDesktopWindow();

		BOOL                            GetMenubarInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out);

		void							SetButtonSkin(OpButton *button);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

		virtual BOOL AccessibilityIsReady() const { return TRUE; }
		virtual OpWindow* AccessibilityGetWindow() const;
		virtual int	GetAccessibleChildrenCount();
		virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
		virtual OP_STATUS AccessibilityGetText(OpString& str);
		virtual OP_STATUS AccessibilityGetDescription(OpString& str);
		virtual OP_STATUS AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut);

		virtual Accessibility::ElementKind AccessibilityGetRole() const;
		virtual Accessibility::State AccessibilityGetState();

		virtual OpAccessibleItem* GetAccessibleParent() {return NULL;}
		virtual OpAccessibleItem* GetAccessibleChild(int);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetNextAccessibleSibling();
		virtual OpAccessibleItem* GetPreviousAccessibleSibling();
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

		virtual OpAccessibleItem* GetLeftAccessibleObject();
		virtual OpAccessibleItem* GetRightAccessibleObject();
		virtual OpAccessibleItem* GetDownAccessibleObject();
		virtual OpAccessibleItem* GetUpAccessibleObject();

#endif //ACCESSIBILITY_EXTENSION_SUPPORT


		BOOL							m_alt_key_down;
		Head							m_items;
		int								m_border_type;
		HWND							m_old_focus;
		OUIMenuManager*					m_menu_manager;
		OUIMenuBarItem*					m_hover_item;
		OUIMenuBarItem*					m_focus;
		OUIMenuBarItem*					m_mdi_item;
		const char*						m_skin;

		HWND							m_mdi_minimize;
		HWND							m_mdi_restore;
		HWND							m_mdi_close;

		WindowsOpWindow*				m_parent_opwindow;
};

/***********************************************************************************
**
**	OUIMenuBarItem
**
***********************************************************************************/

class OUIMenuBarItem : public Link
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
				, public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
	public:
								OUIMenuBarItem(OUIMenuBar *menu_bar, uni_char* title, const uni_char* sub_menu_name, INTPTR sub_menu_value, uni_char accel);
								OUIMenuBarItem(OUIMenuBar *menu_bar);
		virtual					~OUIMenuBarItem();


		void					SetPosition(RECT *position) {m_position=*position;};
		void					SetFocus();
		void					SetFlags(int set, int clear = 0);
		void					ClearFlags(int clear) {SetFlags(0, clear);}

		void					Paint(VisualDevice* vd);
		void					Click(BOOL keyboard_activated = FALSE);
		void					Redraw();
		BOOL					IsVisible() {return (!IsRectEmpty(&m_position));};

		void					SetIsMDIItem(BOOL mdi_item) { m_is_mdi_item = mdi_item; }
		BOOL					IsMDIItem() { return m_is_mdi_item; }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

		virtual BOOL AccessibilityIsReady() const { return TRUE; }
		virtual OP_STATUS AccessibilityClicked();
		virtual int	GetAccessibleChildrenCount();
		virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
		virtual OP_STATUS AccessibilityGetText(OpString& str);
		virtual OP_STATUS AccessibilityGetDescription(OpString& str);
		virtual OP_STATUS AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut);

		virtual BOOL AccessibilitySetFocus();

		virtual Accessibility::ElementKind AccessibilityGetRole() const;
		virtual Accessibility::State AccessibilityGetState();

		virtual OpAccessibleItem* GetAccessibleParent() { return m_menu_bar; }
		virtual OpAccessibleItem* GetAccessibleChild(int);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetNextAccessibleSibling();
		virtual OpAccessibleItem* GetPreviousAccessibleSibling();
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();

		virtual OpAccessibleItem* GetLeftAccessibleObject();
		virtual OpAccessibleItem* GetRightAccessibleObject();
		virtual OpAccessibleItem* GetDownAccessibleObject();
		virtual OpAccessibleItem* GetUpAccessibleObject();

#endif //ACCESSIBILITY_EXTENSION_SUPPORT
		void					InternalInit();

public:
		int						m_flags;
		OUIMenuBar*				m_menu_bar;
		RECT					m_position;
		OpString				m_title;
		OpString8				m_sub_menu_name;
		INTPTR					m_sub_menu_value;
		uni_char				m_accel;
		BOOL					m_is_mdi_item;
private:		
		OpButton*				m_button;
};

#endif  // MENU_BAR_H
