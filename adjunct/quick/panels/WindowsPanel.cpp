
#include "core/pch.h"

#include "WindowsPanel.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/widgets/OpWindowList.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	WindowsPanel
**
***********************************************************************************/

WindowsPanel::WindowsPanel()
{
}

/***********************************************************************************
**
**	WindowsPanel
**
***********************************************************************************/

OP_STATUS WindowsPanel::Init()
{
	if (!(m_windows_view = OP_NEW(OpWindowList, ())))
		return OpStatus::ERR_NO_MEMORY;

	AddChild(m_windows_view, TRUE);

	SetToolbarName("Windows Panel Toolbar", "Windows Full Toolbar");
	SetName("Windows");
//	m_windows_view->SetShowColumnHeaders(FALSE);
	
	m_windows_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/

void WindowsPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_WINDOWS, text);
}


/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/

void WindowsPanel::OnFullModeChanged(BOOL full_mode)
{
	m_windows_view->SetDetailed(full_mode);
}

/***********************************************************************************
**
**	OnLayoutPanel
**
***********************************************************************************/

void WindowsPanel::OnLayoutPanel(OpRect& rect)
{
	m_windows_view->SetRect(rect);
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void WindowsPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_windows_view->SetFocus(reason);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL WindowsPanel::OnInputAction(OpInputAction* action)
{
	return m_windows_view->OnInputAction(action);
}

