/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/quick/managers/MailManager.h"
#include "adjunct/m2/opera_glue/m2glue.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

MailManager::~MailManager()
{
	Stop();
}

void MailManager::Stop()
{
	if (m_mailer)
	{
		m_mailer->Stop();
		OP_DELETE(m_mailer);
		m_mailer = 0;
	}
}


OP_STATUS MailManager::Init()
{
	if (MessageEngine::InitM2OnStartup())
	{
		OP_PROFILE_METHOD("Mail::Start completed");

		m_mailer = OP_NEW(MailerGlue, ());
		if (!m_mailer)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpString8> status(OP_NEW(OpString8, ()));
		if (!status.get())
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS ret = m_mailer->Start(*status);
		if (OpStatus::IsError(ret))
		{
			// If initialization failed, alert the user, but allow Opera to start by not returning an error
			Stop();
			g_main_message_handler->PostMessage(MSG_QUICK_MAIL_PROBLEM_INITIALIZING, (MH_PARAM_1)g_application, (MH_PARAM_2)status.release(), 10);
		}
	}
	return OpStatus::OK;
}


#endif // M2_SUPPORT
