/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef MAIL_MANAGER_H
#define MAIL_MANAGER_H

#ifdef M2_SUPPORT

#include "adjunct/quick/managers/DesktopManager.h"

/** @brief Manages the Opera Mail client (M2)
  */
class MailManager : public DesktopManager<MailManager>
{
public:
	MailManager() : m_mailer(0) {}
	~MailManager();
	OP_STATUS Init();

private:
	void Stop();

	class MailerGlue* m_mailer;
};

#endif // M2_SUPPORT

#endif // MAIL_MANAGER_H
