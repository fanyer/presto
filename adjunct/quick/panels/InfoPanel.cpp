/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "InfoPanel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"
#include "modules/doc/doc.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	InfoPanel
**
***********************************************************************************/

InfoPanel::InfoPanel() : DesktopWindowSpy(TRUE), m_is_cleared(TRUE), m_needs_update(FALSE), m_is_locked(FALSE)
{
	SetSpyInputContext(this, FALSE);
}

void InfoPanel::OnDeleted()
{
	SetSpyInputContext(NULL, FALSE);
	WebPanel::OnDeleted();
}

/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/

void InfoPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_INFO, text);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS InfoPanel::Init()
{
	RETURN_IF_ERROR(WebPanel::Init());
	SetToolbarName("Info Panel Toolbar", "Info Full Toolbar");
	SetName("Info");
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/

void InfoPanel::OnShow(BOOL show)
{
	if (show && m_needs_update)
	{
		UpdateInfo(FALSE);
	}
}

/***********************************************************************************
**
**	UpdateInfo
**
***********************************************************************************/

void InfoPanel::UpdateInfo(BOOL clear)
{
	if (m_is_locked)
		return;

	if (!IsAllVisible() && !clear)
	{
		m_needs_update = TRUE;
		return;
	}

	m_needs_update = FALSE;

	OpBrowserView* browser_view = GetTargetBrowserView();

	clear = clear || !browser_view || !WindowCommanderProxy::HasCurrentDoc(browser_view->GetWindowCommander());

	if (clear)
	{
		if (m_is_cleared)
			return;

		// clear

		URL url = GetBrowserView()->BeginWrite();

#ifdef USE_ABOUT_FRAMEWORK
		InfoPanelEmptyHTMLView htmlview(url, Str::SI_ERR_UNKNOWN_DOCUMENT);
		htmlview.GenerateData();
#else
		url.WriteDocumentData(UNI_L("<html></html>"));
#endif

		GetBrowserView()->EndWrite(url);

		m_is_cleared = TRUE;
	}
	else
	{
		URL url = GetBrowserView()->BeginWrite();

		if (!WindowCommanderProxy::GetPageInfo(browser_view, url))
		{
#ifdef USE_ABOUT_FRAMEWORK
			InfoPanelEmptyHTMLView htmlview(url, Str::SI_IDSTR_ERROR);
			htmlview.GenerateData();
#else
			OpString error;
			g_languageManager->GetString(Str::SI_IDSTR_ERROR, error);

			url.WriteDocumentData(UNI_L("<html>"));
			url.WriteDocumentData(error.CStr());
			url.WriteDocumentData(UNI_L("<html>"));
#endif
		}

		GetBrowserView()->EndWrite(url);

		m_is_cleared = FALSE;
	}
}

#ifdef USE_ABOUT_FRAMEWORK
InfoPanel::InfoPanelEmptyHTMLView::InfoPanelEmptyHTMLView(URL &url, Str::LocaleString text)
	: OpGeneratedDocument(url, HTML5)
	, m_text(text)
{
}

OP_STATUS InfoPanel::InfoPanelEmptyHTMLView::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_INFOPANEL_TITLE, PrefsCollectionFiles::StyleInfoPanelFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_INFOPANEL_TITLE));
#endif
	RETURN_IF_ERROR(OpenBody(Str::LocaleString(m_text)));
	return CloseDocument();
}
#endif


/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL InfoPanel::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_LOCK_PANEL:
				case OpInputAction::ACTION_UNLOCK_PANEL:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_LOCK_PANEL, m_is_locked);
					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_LOCK_PANEL:
			return SetPanelLocked(TRUE);

		case OpInputAction::ACTION_UNLOCK_PANEL:
			return SetPanelLocked(FALSE);

	}
	return FALSE;
}

void InfoPanel::OnTargetDesktopWindowChanged(DesktopWindow* target_window)
{
	if (!(target_window && (target_window->GetType() == WINDOW_TYPE_PANEL ||
	                        target_window->GetType() == WINDOW_TYPE_HOTLIST)))
	{
		UpdateInfo(GetTargetBrowserView() == NULL);
	}
}

/***********************************************************************************
**
**	SetPanelLocked
**
***********************************************************************************/

BOOL InfoPanel::SetPanelLocked(BOOL locked)
{
	if (m_is_locked == locked)
		return FALSE;

	m_is_locked = locked;

	if (!m_is_locked)
		UpdateInfo(FALSE);

	return TRUE;
}
