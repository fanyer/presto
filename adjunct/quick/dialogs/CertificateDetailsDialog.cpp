/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "CertificateDetailsDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/dialogs/SecurityInformationDialog.h"
#include "adjunct/quick/dialogs/CertificateInstallWizard.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpListBox.h"

#include "modules/url/url_man.h"

#include "modules/dochand/win.h"

#include "modules/hardcore/mh/messages.h"

/***********************************************************************************
**
**	CertificateDetailsDialog
**
***********************************************************************************/

CertificateDetailsDialog::CertificateDetailsDialog(SSL_Certificate_DisplayContext* p) :
m_certcontext(p)
{
}

/***********************************************************************************
**
**	~CertificateDetailsDialog
**
***********************************************************************************/

CertificateDetailsDialog::~CertificateDetailsDialog()
{
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void CertificateDetailsDialog::OnInit()
{
    BOOL warn, deny;
	OpString title, field;
	int i = 0;
	while (m_certcontext->GetCertificateShortName(title, i++))
	{}
	URL cert_info = m_certcontext->AccessCurrentCertificateInfo(warn, deny, field);

	SetWidgetValue("Allow_connections_to_cert", !deny);
	SetWidgetValue("Warn_before_data_to_cert", warn);
	SetWidgetEnabled("Warn_before_data_to_cert", !deny);

	if (m_certcontext->GetTitle() == Str::SI_PERSONAL_CERT_DLG_TITLE)
	{
		ShowWidget("Allow_connections_to_cert", FALSE);
		ShowWidget("Warn_before_data_to_cert", FALSE);
	}

	OpBrowserView* view = (OpBrowserView*)GetWidgetByName("Cert_browserview");
	if(view && WindowCommanderProxy::HasWindow(view))
	{
		URL ref_url;
		view->GetWindowCommander()->SetScriptingDisabled(TRUE);
		WindowCommanderProxy::OpenURL(view, cert_info, ref_url, FALSE, FALSE, -1);
		view->AddListener(this); // To be able to block context menus
	}
}

/***********************************************************************************
**
**	OnOk
**
***********************************************************************************/

UINT32 CertificateDetailsDialog::OnOk()
{
	if(m_certcontext)
	{
		BOOL warn = GetWidgetValue("Warn_before_data_to_cert");
		BOOL allow = GetWidgetValue("Allow_connections_to_cert");
		m_certcontext->UpdateCurrentCertificate(warn, allow);
	}
	return 0;
}

/***********************************************************************************
**
**	OnPopup
**
***********************************************************************************/

BOOL CertificateDetailsDialog::OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context)
{
	// Disable all context menus for this page
	return TRUE;
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void CertificateDetailsDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget->IsNamed("Allow_connections_to_cert"))
		SetWidgetEnabled("Warn_before_data_to_cert", widget->GetValue());
}

/*********************************************************************************/
// == CertificateInstallDialog ==
/*********************************************************************************/

