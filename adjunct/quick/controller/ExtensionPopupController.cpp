/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 * 
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved. 
 * 
 * This file is part of the Opera web browser.  It may not be distributed 
 * under any circumstances. 
 */ 

#include "core/pch.h"
#include "adjunct/quick/controller/ExtensionPopupController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"


#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/widgets/WidgetContainer.h"

ExtensionPopupController::ExtensionPopupController(OpWidget& parent):
				m_parent(&parent),
				m_popup_browser_view(NULL)		
{}

void ExtensionPopupController::SetContentSize(UINT32 width, UINT32 height)
{
	if (m_popup_browser_view)
	{			
		m_popup_browser_view->SetFixedHeight(height);
		m_popup_browser_view->SetFixedWidth(width);
	}
}

void ExtensionPopupController::InitL()
{
	DesktopWindow* desktop_window = m_parent->GetParentDesktopWindow();	
	if(!(desktop_window && desktop_window->GetWidgetContainer()))
		LEAVE(OpStatus::ERR);

	QuickOverlayDialog* dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Extension Popup", dialog));
	dialog->SetBoundingWidget(*desktop_window->GetWidgetContainer()->GetRoot());

	m_popup_browser_view = dialog->GetWidgetCollection()->GetL<QuickBrowserView>("browser_view");
	
	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*m_parent));
	dialog->SetDialogPlacer(placer);
	dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_NONE);
}

OP_STATUS ExtensionPopupController::LoadDocument(OpWindowCommander* commander, const OpStringC& url)
{
	OP_ASSERT(commander != NULL);
	if (commander == NULL)
		return OpStatus::ERR_NULL_POINTER;

	if (!m_popup_browser_view)
		return OpStatus::ERR;

	OpAutoPtr<OpPage> page(OP_NEW(OpPage, (commander)));
	RETURN_OOM_IF_NULL(page.get());
	RETURN_IF_ERROR(page->Init());
	
	RETURN_IF_ERROR(m_popup_browser_view->GetOpBrowserView()->SetPage(page.release()));	
	
	/// would like to use that line below.. but unfortunately core does not support that
	//  commander->OnUiWindowCreated(m_popup_browser_view->GetOpWidget()->GetOpWindow());
	//  so we need to open url manually : CORE-40839 
	if (commander->OpenURL(url.CStr(), FALSE))
		return OpStatus::OK;
	else 
		return OpStatus::ERR;
}

OP_STATUS ExtensionPopupController::AddPageListener(OpPageListener* listener)
{
	if(!m_popup_browser_view || !m_popup_browser_view->GetOpBrowserView())
		return OpStatus::ERR;

	return m_popup_browser_view->GetOpBrowserView()->AddListener(listener);
}

OP_STATUS ExtensionPopupController::ReloadDocument(OpGadget* extension, const OpStringC& url)
{
	if (!m_popup_browser_view)
		return OpStatus::ERR;

	OpBrowserView* view = m_popup_browser_view->GetOpBrowserView();
	if (view && view->GetWindowCommander()->OpenURL(url,FALSE))
	{
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

BOOL ExtensionPopupController::HandlePopupAction(OpInputAction* action)
{
	OpBrowserView* view = m_popup_browser_view->GetOpBrowserView();
	if (view && view->OnInputAction(action))
	{
		return TRUE;
	}
	else
		return FALSE;
}
