/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CERTDETAILSDIALOG_H
#define CERTDETAILSDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#ifdef _SSL_SUPPORT_
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/libssl/ssldlg.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/sslcctx.h"
#endif

#ifdef USE_ABOUT_FRAMEWORK
#include "modules/about/opgenerateddocument.h"
#endif

/***********************************************************************************
**
**	CertificateDetailsDialog
**
***********************************************************************************/

class CertificateDetailsDialog : public Dialog, OpPageListener
{
	SSL_Certificate_DisplayContext*				m_certcontext;
public:
						CertificateDetailsDialog(SSL_Certificate_DisplayContext* p);
						~CertificateDetailsDialog();
	Type				GetType()				{return DIALOG_TYPE_CERTIFICATE_DETAILS;}
	const char*			GetWindowName()			{return "Certificate Details Dialog";}
	const char*			GetHelpAnchor()			{return "certificates.html";}

	void				OnInit();
	UINT32				OnOk();

	virtual BOOL		OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context);

	void OnChange(OpWidget *widget, BOOL changed_by_mouse);
};

/***********************************************************************************
**
**	CertificateInstallDialog
**
***********************************************************************************/

#endif //CERTMANAGERDIALOG_H
