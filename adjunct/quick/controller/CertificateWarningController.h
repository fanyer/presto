/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */

#ifndef CERT_WARNING_CONTROLLER_H
#define CERT_WARNING_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/contexts/DelayedOkCancelDialogContext.h"
#include "modules/windowcommander/OpWindowCommander.h"


class CertificateWarningController : public DelayedOkCancelDialogContext
{
public:
	CertificateWarningController(OpSSLListener::SSLCertificateContext* context,
			OpSSLListener::SSLCertificateOption options, bool overlaid);

protected:
	virtual void InitL();
	virtual void OnOk();
	virtual void OnCancel();

private:
	void OnChangeItem(INT32 new_index);

	///< List of strings to display in the details page when selecting a certificate part.
	/**
	 * Extract and parse relevant information from the display context.
	 */
	OP_STATUS		ParseSecurityInformation();
	/**
	 * Function that populates the warnign tab of the certificate
	 * warning dialog based on the display context.
	 */
	OP_STATUS		PopulateWarningPage();
	/**
	 * Function that populates the security tab of the certificate
	 * warning dialog based on the display context member.
	 */
	OP_STATUS		PopulateSecurityPage();
	/**
	 * Function that populates the details tab of the certificate
	 * warning dialog based on the display context member.
	 */
	OP_STATUS		PopulateDetailsPage();

	OpSSLListener::SSLCertificateContext*	m_context;
	OpSSLListener::SSLCertificateOption		m_options;
	const bool m_overlaid;

	// Members holding information about the connection
	SimpleTreeModel							m_warning_chain;
	OpString								m_warning_tab_text;
	OpString								m_domain_name;
	OpString								m_subject_name;
	OpString								m_cert_valid;
	OpString								m_issuer_name;
	OpString								m_encryption;
	OpString								m_cert_comments;
	OpAutoVector<OpString>					m_string_list;
	OpProperty<INT32>                       m_treeview_selected;
	OpProperty<bool>                        m_warning_disable;
	OpProperty<OpString>                    m_warning_details;
};

#endif //CERT_WARNING_CONTROLLER_H
