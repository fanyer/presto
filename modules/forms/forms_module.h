/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_FORMS_FORMS_MODULE_H
#define MODULES_FORMS_FORMS_MODULE_H

#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION
# define FORMS_KEYGEN_SUPPORT
#endif //  defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_

#include "modules/hardcore/opera/module.h"

class FormsHistory;
class FormSubmitter;

/**
 * Class for global data in the forms module.
 */
class FormsModule : public OperaModule
{
public:
	FormsModule()
	{
		m_forms_history = NULL;
#ifdef FORMS_KEYGEN_SUPPORT
		m_submits_in_progress = NULL;
#endif // FORMS_KEYGEN_SUPPORT
	}

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	FormsHistory* m_forms_history;

#ifdef FORMS_KEYGEN_SUPPORT
	FormSubmitter* m_submits_in_progress; // Submits waiting for UI feedback
#endif // FORMS_KEYGEN_SUPPORT
};

#define g_forms_history g_opera->forms_module.m_forms_history

#define FORMS_MODULE_REQUIRED

#endif // !MODULES_FORMS_FORMS_MODULE_H
