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

#ifndef SSL_SECURITY_PASSWORD_CALLBACK_IMPL_H
#define SSL_SECURITY_PASSWORD_CALLBACK_IMPL_H

#if defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/locale/locale-enum.h"

/**
 *  @struct SSL_dialog_config
 *
 *  This structure defines which Window a dialog should use when it opens,
 *	which message handler to use when posting the finished message and
 *	which ID to use when posting the message.
 *
 *	The object is constructed and released by the caller. The information is
 *	copied by the functions called with this object.
 */
struct SSL_dialog_config
{
public:
	/** Window to connect to */
	OpWindow *window;
	/** Message handler to use, caller must register a callback on this
	 *	for the specified message and ID
	 */
	MessageHandler *msg_handler;
	/** Finished message posted when the dialog is finished.
	 *	par1 is the finished id
	 *	par2 is the dialog specific result code
	 */
	OpMessage finished_message;
	/** Id used in posted message */
	MH_PARAM_1 finished_id;

	/** URL call back for retrieving password. */
	URL url;

	/** Default constructor, values have to be assigned to members individually */
	SSL_dialog_config():
			window(NULL),
			msg_handler(NULL),
			finished_message(MSG_NO_MESSAGE),
			finished_id(0)
			{
			}
	/** Constructor which allows you to set each value in one call */
	SSL_dialog_config(OpWindow *wndw,MessageHandler *mh, OpMessage fin_message,	MH_PARAM_1 fin_id, URL &a_url):
			window(wndw),
			msg_handler(mh),
			finished_message(fin_message),
			finished_id(fin_id),
			url(a_url)
			{
			}

};


class SSLSecurtityPasswordCallbackImpl : public OpSSLListener::SSLSecurityPasswordCallback
{
public:
	SSLSecurtityPasswordCallbackImpl(PasswordDialogMode md, PasswordRequestReason reason, Str::LocaleString titl, Str::LocaleString msg, SSL_dialog_config& conf);
	virtual ~SSLSecurtityPasswordCallbackImpl();

public:
	OP_STATUS AddMessage(SSL_dialog_config& conf);
	virtual OP_STATUS StartDialog();

	BOOL IsFinished() const { return is_finished; }
	const char* GetOldPassword() const { return old_password.CStr(); }
	const char* GetNewPassword() const { return new_password.CStr(); }

public:
	/** @name SSLSecurityPasswordCallback methods.
	 *  @{
	 */
	virtual Str::LocaleString  GetTitle()   const { return title;   }
	virtual Str::LocaleString  GetMessage() const { return message; }
	virtual PasswordDialogMode GetMode()    const { return mode;    }
	virtual PasswordRequestReason GetReason() const { return reason;  }
	virtual void OnSecurityPasswordDone(BOOL ok, const char* old_passwd, const char* new_passwd);
	/** @} */

protected:
	virtual void FinishedDialog(MH_PARAM_2 status);

private:
	Str::LocaleString  title;
	Str::LocaleString  message;
	PasswordDialogMode mode;
	PasswordRequestReason reason;

	OpString8 old_password;
	OpString8 new_password;

	OpWindow*                      window;
	TwoWayPointer <MessageHandler> mh;
	OpMessage                      finished_message;
	MH_PARAM_1                     finished_id;
	BOOL                           is_finished;
	URL                            url;

	AutoDeleteHead messages_to_post;
};

#endif // defined _SSL_SUPPORT_ && defined _NATIVE_SSL_SUPPORT_ || defined WIC_USE_SSL_LISTENER
#endif // SSL_SECURITY_PASSWORD_CALLBACK_IMPL_H
