/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Yngve Pettersen
** @author Alexei Khlebnikov
**
*/

#include "core/pch.h"

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER

#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"

#include "modules/dochand/winman.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"
#include "modules/network/src/op_request_impl.h"
#include "modules/url/url_ds.h"


class ListOfMessages : public Link
{
private:
	TwoWayPointer<MessageHandler>	mh;
	OpMessage finished_message;
	MH_PARAM_1 finished_id;

public:

	ListOfMessages(MessageHandler *msg_handler, OpMessage fin_message, MH_PARAM_1 fin_id):
						mh(msg_handler), finished_message(fin_message), finished_id(fin_id) {};
	~ListOfMessages(){if(InList()) Out();}

	void PostMessage(MH_PARAM_2 par2){if(mh.get() != NULL) mh->PostMessage(finished_message,finished_id, par2);} 
};

SSLSecurtityPasswordCallbackImpl::SSLSecurtityPasswordCallbackImpl(PasswordDialogMode md, PasswordRequestReason reason, Str::LocaleString titl, Str::LocaleString msg, SSL_dialog_config& conf)
	: title(titl)
	, message(msg)
	, mode(md)
	, reason(reason)
	, window(conf.window)
	, mh(conf.msg_handler)
	, finished_message(conf.finished_message)
	, finished_id(conf.finished_id)
	, is_finished(FALSE)
, url(conf.url)
{
}

SSLSecurtityPasswordCallbackImpl::~SSLSecurtityPasswordCallbackImpl()
{
	//TODO: connect to Windowcommander, abort dialog
	new_password.Wipe();
	old_password.Wipe();
}

void SSLSecurtityPasswordCallbackImpl::OnSecurityPasswordDone(BOOL ok, const char* old_passwd, const char* new_passwd)
{
	if(ok)
	{
		if (OpStatus::IsError(old_password.Set(old_passwd)) ||
		    OpStatus::IsError(new_password.Set(new_passwd)))
				// Simulate cancellation on OOM.
				ok = FALSE;
	}

	FinishedDialog(ok ? IDOK : IDCANCEL);
}

OP_STATUS SSLSecurtityPasswordCallbackImpl::AddMessage(SSL_dialog_config& config)
{
	ListOfMessages* item = OP_NEW(ListOfMessages, (config.msg_handler, config.finished_message, config.finished_id));

	if(!item)
		return OpStatus::ERR_NO_MEMORY;

	item->Into(&messages_to_post);

	return OpStatus::OK;
}

OP_STATUS SSLSecurtityPasswordCallbackImpl::StartDialog()
{
	// fixme, use a wic if possible, otherwise fallback on windowcommanderManager
	Window* win;
	WindowCommander* wic = NULL;
	for(win = g_windowManager->FirstWindow(); win; win = win->Suc())
	{
		if (win->GetOpWindow() == window)
		{
			wic = win->GetWindowCommander();
			break;
		}
	}


	OpRequestImpl *request = NULL;
	if (url.GetRep()->GetDataStorage())
		request = url.GetRep()->GetDataStorage()->GetOpRequestImpl();

	if (!url.GetAttribute(URL::KUseGenericAuthenticationHandling) && request)
	{
		request->SecurityPasswordRequired(this);
	}
	else
	{
		if (wic)
			wic->GetSSLListener()->OnSecurityPasswordNeeded(wic, this);
		else
			g_windowCommanderManager->GetSSLListener()->OnSecurityPasswordNeeded(NULL, this);
	}

	return OpStatus::OK;
}

void SSLSecurtityPasswordCallbackImpl::FinishedDialog(MH_PARAM_2 status)
{
	(mh.get() != NULL ? mh.get() : g_main_message_handler)->PostMessage(finished_message, finished_id, status);

	ListOfMessages* item = (ListOfMessages *) messages_to_post.First();
	while(item)
	{
		item->PostMessage(status);
		item = (ListOfMessages *) item->Suc();
	}
	is_finished = TRUE;
}

#endif // defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
