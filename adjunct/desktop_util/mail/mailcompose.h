/*
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * George Refseth (rfz)
*/

#ifndef MAILCOMPOSE_H
#define MAILCOMPOSE_H

#include "adjunct/desktop_util/prefs/DesktopPrefsTypes.h"

class MailTo;
class ComposeDesktopWindow;

namespace MailCompose
{
	/** Make an email user agent available to the user
	  * @param id_list list of users as id's
	  */
	void ComposeMail(OpINT32Vector& id_list);

	/** Make an email user agent available to the user
	  * @param mailto object holding the mail as both component and a url mailto string.
	  * @param force_in_opera if true use opera mailer (m2)
	  * @param force_background parameter for use by m2 compose mail or webmail window
	  * @param new_window parameter for use by m2 compose mail or webmail window
	  * @param attachment string contains file url of attachment, only for use by m2 ComposeWindow
	  * @param compose_window pointer to created ComposeWindow
	  */
	void ComposeMail(const MailTo& mailto, BOOL force_in_opera = FALSE, BOOL force_background = FALSE, BOOL new_window = FALSE, const OpStringC* attachment = NULL, ComposeDesktopWindow** compose_window = NULL);

	/** For internal use only, shall only be called by ComposeMail directly or indirectlry (from dialogs invoked by ComposeMail).
	 *  Make an email user agent available to the user
	  * @param mailto object holding the mail as both component and a url mailto string.
	  * @param handler resolved mail handler
	  * @param webmail_id if handler is web mail, webmail_id identifies the web mail choice
	  * @param force_background parameter for use by m2 compose mail or webmail window
	  * @param new_window parameter for use by m2 compose mail or webmail window
	  * @param attachment string contains file url of attachment, only for use by m2 ComposeWindow
	  * @param compose_window pointer to created ComposeWindow
	  */
	void InternalComposeMail(const MailTo& mailto, MAILHANDLER handler, unsigned int webmail_id, BOOL force_background, BOOL new_window, const OpStringC* attachment = NULL, ComposeDesktopWindow** compose_window = NULL);
};

#endif // MAILCOMPOSE_H
