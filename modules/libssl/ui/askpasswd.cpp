/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef LIBSSL_SECURITY_PASSWD

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"

#include "modules/libssl/askpasswd.h"
#include "modules/dochand/winman.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"


Options_AskPasswordContext::Options_AskPasswordContext(PasswordDialogMode md, Str::LocaleString titl, Str::LocaleString msg, SSL_dialog_config &conf, SSL_Options *trgt)
: SSLSecurtityPasswordCallbackImpl(md, OpSSLListener::SSLSecurityPasswordCallback::CertificateUnlock, titl, msg, conf), target(trgt)
{
}

Options_AskPasswordContext::~Options_AskPasswordContext()
{
	target = NULL;
}

void Options_AskPasswordContext::FinishedDialog(MH_PARAM_2 status)
{
	SSL_Options *trgt = (target ? target : g_securityManager);
	if(status == IDOK && trgt)
	{
		switch (GetMode())
		{
			case JustPassword:
				trgt->UseSecurityPassword(GetOldPassword());
				break;

			case NewPassword:
				trgt->UseSecurityPassword(GetNewPassword());
				break;

			default:
				OP_ASSERT(!"Invalid password mode!");
		}
	}
	
	SSLSecurtityPasswordCallbackImpl::FinishedDialog(status);
}

#endif
#endif
