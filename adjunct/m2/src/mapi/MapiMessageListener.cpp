/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Golczynski (tgolczynski@opera.com)
 */
#include "core/pch.h"
#ifdef M2_MAPI_SUPPORT

#include "adjunct/desktop_pi/DesktopMailClientUtils.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "adjunct/m2/src/generated/g_message_m2_mapimessage.h"
#include "adjunct/m2/src/mapi/MapiMessageListener.h"

#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"

MapiMessageListener::MapiMessageListener()
	: m_initialized(false)
{
	g_component_manager->AddMessageListener(this);
	g_desktop_mail_client_utils->SignalMailClientIsInitialized(TRUE);
}

MapiMessageListener::~MapiMessageListener()
{
	g_desktop_mail_client_utils->SignalMailClientIsInitialized(FALSE);
	g_component_manager->RemoveMessageListener(this);
}

void MapiMessageListener::OperaIsInitialized()
{
	m_initialized = true;

	for (UINT32 i = 0; i < m_new_mails.GetCount(); i++)
	{
		NewMail* mail = m_new_mails.Get(i);
		if (mail->m_window)
		{
			OpStatus::Ignore(mail->m_window->SaveAttachments());
			g_desktop_mail_client_utils->SignalMailClientHandledMessage(mail->m_id);
		}
	}
	m_new_mails.DeleteAll();
}

OP_STATUS MapiMessageListener::ProcessMessage(const OpTypedMessage* message)
{
	if (message->GetType() != OpCreateNewMailMessage::Type)
		return OpStatus::OK;

	const OpCreateNewMailMessage* new_mail = OpCreateNewMailMessage::Cast(message);
	OpString to, cc, bcc, subject, body, file_paths, file_names;

	RETURN_IF_ERROR(subject.SetFromUniString(new_mail->GetSubject()));
	RETURN_IF_ERROR(body.SetFromUniString(new_mail->GetText()));
	RETURN_IF_ERROR(to.SetFromUniString(new_mail->GetTo()));
	RETURN_IF_ERROR(cc.SetFromUniString(new_mail->GetCc()));
	RETURN_IF_ERROR(bcc.SetFromUniString(new_mail->GetBcc()));
	RETURN_IF_ERROR(file_paths.SetFromUniString(new_mail->GetFilePaths()));
	RETURN_IF_ERROR(file_names.SetFromUniString(new_mail->GetFileNames()));

	ComposeDesktopWindow* window = NULL;
	MailTo mail_to;
	RETURN_IF_ERROR(mail_to.Init(to,cc,bcc,subject,body));
	MailCompose::ComposeMail(mail_to, TRUE, FALSE, FALSE, NULL, &window);

	RETURN_VALUE_IF_NULL(window, OpStatus::ERR);

	bool is_waiting = false;
	if (!file_paths.IsEmpty() && OpStatus::IsSuccess(window->AddAttachment(file_paths.CStr(), file_names.CStr())))
	{
		if (m_initialized)
			OpStatus::Ignore(window->SaveAttachments());
		else
		{
			OpAutoPtr<NewMail> ptr(OP_NEW(NewMail, (window, new_mail->GetEventId())));
			if (ptr.get() && OpStatus::IsSuccess(m_new_mails.Add(ptr.get())))
			{
				is_waiting = true;
				ptr.release();
			}
		}
	}

	if (!is_waiting)
		g_desktop_mail_client_utils->SignalMailClientHandledMessage(new_mail->GetEventId());

	if (window->GetParentDesktopWindow())
		window->GetParentDesktopWindow()->Activate();
	return OpStatus::OK;
}

#endif //M2_MAPI_SUPPORT
