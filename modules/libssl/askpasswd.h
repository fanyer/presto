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


#ifndef __ASK_PASSWORD_H__
#define __ASK_PASSWORD_H__

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/libssl/ssl_api.h"

class SSL_Options;
class OpWindow;


class Options_AskPasswordContext : public SSLSecurtityPasswordCallbackImpl
{
private:
	SSL_Options *target;

public:
	Options_AskPasswordContext(PasswordDialogMode md, Str::LocaleString titl, Str::LocaleString msg, SSL_dialog_config &conf, SSL_Options *trgt);
	virtual ~Options_AskPasswordContext();

protected:
	virtual void FinishedDialog(MH_PARAM_2 status);
};

#endif
#endif // __ASK_PASSWORD_H__
