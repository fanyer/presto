/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * \file 
 * \author Owner: Patryk Obara (pobara)
 */
#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef WEBAPP_SUPPORT
#include "WebappWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"


WebappWindow::WebappWindow() :
	m_document_view(NULL)
{}


OP_STATUS WebappWindow::Init(const uni_char *address)
{
	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, NULL, NULL, TRUE, FALSE);
	RETURN_IF_ERROR(status);

	status = OpBrowserView::Construct(&m_document_view, FALSE, NULL);
	RETURN_IF_ERROR(status);

	// m_document_view does not know that its used yet, we have to add it first
	OpWidget* root_widget = GetWidgetContainer()->GetRoot();
	root_widget->AddChild(m_document_view);
	status = m_document_view->init_status;
	RETURN_IF_ERROR(status);

	/// \todo investigaye, how much of this we need
	m_document_view->AddListener(this);
	//m_document_view->SetSupportsNavigation(TRUE);
	//m_document_view->SetSupportsSmallScreen(TRUE);
	//m_document_view->GetWindowCommander()->SetScale(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale));

	OpenURLSetting setting;
	setting.m_address.Set(address);
	setting.m_from_address_field = FALSE;
	setting.m_save_address_to_history = FALSE;
	// setting.m_new_page = NO;
	// setting.m_in_background = NO;
	// setting.m_new_window = NO;

	// why is it -1? what is it? (pobara)
	// URL_CONTEXT_ID context_id = -1;
	// see DocumentDesktopWindow::GoToPage(..) : ~1017
	BOOL result = GetWindowCommander()->OpenURL( setting.m_address.CStr(), 
												 TRUE, (URL_CONTEXT_ID)(-1), FALSE);
	return OpStatus::OK;
}


/// \todo clean up after yourself
WebappWindow::~WebappWindow() {}


// == DesktopWindow hooks ========================
 
/// \todo maybe some optimizations here?

void WebappWindow::OnLayout()
{
	OpRect rect;
	GetBounds(rect);
	m_document_view->SetRect(rect);
}


// == OpPageListener ======================

void WebappWindow::OnPageStartLoading(OpWindowCommander* commander)
{
	Relayout();
}


// == OpInputContext =============================

BOOL WebappWindow::OnInputAction(OpInputAction* action)
{
	/// \todo follow with url handling/filtering starting from here
	printf(">> inputaction: %s\n", OpInputAction::GetStringFromAction(action->GetAction()));
	switch (action->GetAction())
	{
		default: return DesktopWindow::OnInputAction(action);
	}
}

#endif // WEBAPP_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
