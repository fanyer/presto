
#include "core/pch.h"

#include "WebPanel.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "WebPanel.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"

#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

/***********************************************************************************
**
**	WebPanel
**
***********************************************************************************/

WebPanel::WebPanel(const uni_char* url, INT32 id, const uni_char* guid)
{
	m_guid.Set(guid);
	m_url.Set(url);
	m_id = id;
	m_started = FALSE;
}

// ----------------------------------------------------

OP_STATUS WebPanel::Init()
{
	RETURN_IF_ERROR(OpBrowserView::Construct(&m_browser_view));
	AddChild(m_browser_view, TRUE);
	RETURN_IF_ERROR(m_browser_view->init_status);
	SetToolbarName("Web Panel Toolbar", "Web Full Toolbar");

	m_browser_view->GetBorderSkin()->SetImage("Panel Browser Skin", "Browser Skin");

	if( GetType() == PANEL_TYPE_INFO )
	    WindowCommanderProxy::SetType(m_browser_view, WIN_TYPE_INFO);
	else
	    WindowCommanderProxy::SetType(m_browser_view, WIN_TYPE_BRAND_VIEW);

	m_browser_view->SetSupportsNavigation(TRUE);
	m_browser_view->SetSupportsSmallScreen(TRUE);	
	m_browser_view->AddListener(this);

	HotlistModelItem *item = g_hotlist_manager->GetItemByID( m_id );

	if( item && item->GetSmallScreen())
		m_browser_view->GetWindowCommander()->SetLayoutMode(OpWindowCommander::CSSR);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/

void WebPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	HotlistModelItem *item = g_hotlist_manager->GetItemByID( m_id );
	if( item )
	{
		text.Set(item->GetName().CStr());
	}
}

const char* WebPanel::GetPanelImage()
{
	HotlistModelItem *item = g_hotlist_manager->GetItemByID( m_id );
	if( item )
	{
		return item->GetImage();
	}

	return NULL;
}


Image WebPanel::GetPanelIcon()
{	
	HotlistModelItem *item = g_hotlist_manager->GetItemByID( m_id );
	if( item )
	{
		return item->GetIcon();	
	}
	
	return Image();
}

void WebPanel::GetURL(OpString& url)
{
	HotlistModelItem *item = g_hotlist_manager->GetItemByID( m_id );
	if( item )
	{
		url.Set( item->GetUrl().CStr() );
	}
}

void WebPanel::UpdateSettings()
{
	HotlistModelItem *item = g_hotlist_manager->GetItemByID( m_id );
	if( item )
	{
		if( m_url.Compare(item->GetUrl()) != 0 )
		{
			m_url.Set( item->GetUrl().CStr() );
			m_browser_view->GetWindowCommander()->OpenURL(m_url.CStr(), TRUE);
		}
	}

	if( m_selector_button )
	{
		Image img = GetPanelIcon();
		m_selector_button->GetForegroundSkin()->SetBitmapImage( img, FALSE );
	}
}



void WebPanel::OnShow(BOOL show)
{
	if (show && !m_started && m_url.HasContent())
	{
		m_started = TRUE;
		m_browser_view->GetWindowCommander()->OpenURL(m_url.CStr(), TRUE);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL WebPanel::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
		case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
		{
			BOOL enable	= action->GetAction() == OpInputAction::ACTION_ENABLE_HANDHELD_MODE;
			g_hotlist_manager->ShowSmallScreen(m_id, enable);
			break;
		}
		case OpInputAction::ACTION_RELOAD_PANEL:
		{
			m_browser_view->GetWindowCommander()->SetCurrentHistoryPos(0);
			m_browser_view->GetWindowCommander()->Reload();
			return TRUE;
		}
		default:
			break;
	}

	if (m_browser_view->OnInputAction(action))
		return TRUE;

	return FALSE;
}

/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/

void WebPanel::OnFullModeChanged(BOOL full_mode)
{
}

/***********************************************************************************
**
**	OnLayoutPanel
**
***********************************************************************************/

void WebPanel::OnLayoutPanel(OpRect& rect)
{
	m_browser_view->GetOpWindow()->SetMaximumInnerSize(rect.width, rect.height);
	m_browser_view->SetRect(rect);
	// DSK-284110: Sync the visibility of the browser view's window with the visibility of this widget
	m_browser_view->GetOpWindow()->Show(IsAllVisible());
}

// ----------------------------------------------------

void WebPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_browser_view->SetFocus(reason);
	}
}


void WebPanel::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user)
{
	if (!stopped_by_user)
	{
		m_browser_view->SaveDocumentIcon();
	}
}
