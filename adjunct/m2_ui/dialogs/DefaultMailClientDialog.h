/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DEFAULT_MAIL_CLIENT_DIALOG_H
#define DEFAULT_MAIL_CLIENT_DIALOG_H

#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/quick-widget-names.h"
#ifdef M2_MAPI_SUPPORT
class DefaultMailClientDialog : public SimpleDialogController
{
		BOOL		m_do_not_show_again;
	public:
		DefaultMailClientDialog(): SimpleDialogController(TYPE_YES_NO, IMAGE_QUESTION, WINDOW_NAME_DEFAULT_MAIL_CLIENT, Str::D_DEFAULT_MAIL_CLIENT_DIALOG_CONTENT, Str::D_DEFAULT_MAIL_CLIENT_DIALOG_TITLE) {}
		virtual void InitL();
		virtual void OnOk();
};
#endif //M2_MAPI_SUPPORT
#endif