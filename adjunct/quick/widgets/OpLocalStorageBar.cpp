/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/quick/widgets/OpLocalStorageBar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/wand/wandmanager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "modules/url/url_man.h"

DEFINE_CONSTRUCT(OpLocalStorageBar)

OpLocalStorageBar::OpLocalStorageBar()
	: m_callback(NULL)
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	Clear();
}

OpLocalStorageBar::~OpLocalStorageBar()
{
	HandleRequest(FALSE);
}

void OpLocalStorageBar::Show(OpApplicationCacheListener::InstallAppCacheCallback* callback,
							 ApplicationCacheRequest request,
							 UINTPTR id,
							 const uni_char* url)
{
	if (m_callback && m_callback != callback)
		HandleRequest(FALSE);

	m_id = id;
	m_callback = callback;
	m_request = request;
	m_url.Set(url);
	
	URL tmp = urlManager->GetURL(url);
	if (tmp.GetServerName())
		m_domain.Set(tmp.GetServerName()->UniName());

	OpLabel* label = (OpLabel*)GetWidgetByType(OpTypedObject::WIDGET_TYPE_LABEL, FALSE);

	if (label)
	{
		OpString text;
		g_languageManager->GetString(Str::D_APPLICATION_CACHE_REQUEST, text);
		text.ReplaceAll(UNI_L("%1"), m_domain);
		label->SetText(text.CStr());
	}

	SetAlignmentAnimated(OpBar::ALIGNMENT_OLD_VISIBLE);
	//SetAutoHide();
}

void OpLocalStorageBar::Hide(BOOL focus_page)
{
	HandleRequest(FALSE);
	SetAlignmentAnimated(OpBar::ALIGNMENT_OFF);

	if (focus_page)
		g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
}

void OpLocalStorageBar::Cancel()
{
	Clear();
	Hide();
}

void OpLocalStorageBar::OnAlignmentChanged()
{
	if (GetAlignment() == ALIGNMENT_OFF)
	{
		HandleRequest(FALSE);
	}
}

BOOL OpLocalStorageBar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CANCEL:
			Hide();
			return TRUE;
		case OpInputAction::ACTION_OK:
			{
			TRAPD(err, g_pcui->OverridePrefL(m_domain.CStr(), PrefsCollectionUI::StrategyOnApplicationCache, 1, TRUE));
			HandleRequest(TRUE);
			Hide();
			}
			return TRUE;
		case OpInputAction::ACTION_NEVER:
			{
			TRAPD(err, g_pcui->OverridePrefL(m_domain.CStr(), PrefsCollectionUI::StrategyOnApplicationCache, 2, TRUE));
			Hide();
			}
			return TRUE;
	};
	return OpToolbar::OnInputAction(action);
}

void OpLocalStorageBar::HandleRequest(BOOL accept)
{
	if (m_callback)
	{
		if (m_request == Download)
			m_callback->OnDownloadAppCacheReply(accept);
		else if (m_request == CheckForUpdate)
			m_callback->OnCheckForNewAppCacheVersionReply(accept);
		else if (m_request == UpdateCache)
			m_callback->OnDownloadNewAppCacheVersionReply(accept);
	}

	Clear();
}

void OpLocalStorageBar::Clear()
{
	m_callback = NULL;
	m_id = 0;
	m_url.Empty();
	m_domain.Empty();
}
