#ifndef MAC_UP_UI_INFO_H
#define MAC_UP_UI_INFO_H

#include "adjunct/desktop_pi/DesktopOpUiInfo.h"

class MacOpUiInfo :
	public DesktopOpUiInfo
{
public:
	MacOpUiInfo();
	virtual ~MacOpUiInfo() {}

	virtual UINT32 GetVerticalScrollbarWidth();
	virtual UINT32 GetHorizontalScrollbarHeight();

	virtual UINT32 GetSystemColor(OP_SYSTEM_COLOR color);
	/** Retrieve default font for system. This is used as the default setting
	  * and can be overridden using the preferences.
	  * @param font The font to retrieve.
	  * @param outfont FontAtt structure describing the font.
	  */
	virtual void GetSystemFont(OP_SYSTEM_FONT font, FontAtt &outfont);

#if defined(_QUICK_UI_FONT_SUPPORT_)
	virtual void GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default);
#endif

    virtual COLORREF    GetUICSSColor(int css_color_value);
    virtual BOOL        GetUICSSFont(int css_font_value, FontAtt &font);
#ifndef MOUSELESS
    virtual BOOL	IsMouseRightHanded();
#endif

 	/**
	 * When FALSE this will ONLY focus edit fields and listboxes otherwise when TRUE all fields will get focus.
	 *
	 * @return TRUE if full keyboard access is activated
	 */
	virtual BOOL IsFullKeyboardAccessActive();
	/**
	 * If use_custom is true, use new_color as highlight color. Otherwise return to getting this color from the system.
	 */
	virtual OP_STATUS SetCustomHighlightColor(BOOL use_custom, COLORREF new_color);

	virtual BOOL DefaultButtonOnRight() { return TRUE; }

private:
	BOOL m_use_custom_highlight;
	COLORREF m_custom_highlight;
	int m_full_keyboard_access;
	UINT32 GetCaretWidth();
};

#endif // MAC_UP_UI_INFO_H
