/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Misc userinterface functions.
*/


#ifndef _USER_H_
#define _USER_H_

#include "adjunct/desktop_pi/desktoppopupmenuhandler.h"

void InitializeWin(HWND hWnd); 

#define ID_TIMER_MARKING 	100
#define USER_WM_CREATE		100
#define ID_TOOLTIP_TIMER	101

BOOL OnCreateMainWindow();

BOOL SetFontDefault(PLOGFONT pLFont);
BOOL ResetStation();

LRESULT ucWMDrawItem(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT ucMeasureItem(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam);
LRESULT ucTimer(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam);
LRESULT ucClose(HWND hWnd, BOOL fAskUser);

class NullAppMenuListener : public DesktopPopupMenuHandler
{

public:
	// Implementing DesktopPopupMenuHandler
	virtual void OnPopupMenu(const char* popup_name, const PopupPlacement& placement, BOOL point_in_page, INTPTR popup_value, BOOL use_keyboard_context) {}
	virtual BOOL OnAddMenuItem(void* menu, BOOL seperator, const uni_char* item_name, OpWidgetImage* image, const uni_char* shortcut, const char* sub_menu_name, INTPTR sub_menu_value, INT32 sub_menu_start, BOOL checked, BOOL selected, BOOL disabled, BOOL bold, OpInputAction* input_action) { return FALSE;}
	virtual void OnAddShellMenuItems(void* menu, const uni_char* path) {}
};

#endif 


