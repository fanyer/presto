/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef WAND_SUPPORT

#include "modules/wand/suspendedoperation.h"
#include "modules/wand/wandmanager.h"
#include "modules/doc/frm_doc.h"

/* virtual */
SuspendedWandOperationWithPage::~SuspendedWandOperationWithPage()
{
	if (m_owns_wand_page)
		delete m_wand_page;
}

/* virtual */
SuspendedWandOperationWithLogin::~SuspendedWandOperationWithLogin()
{
#ifdef WAND_EXTERNAL_STORAGE
	if (m_wand_login != &tmp_wand_login)
	{
		delete m_wand_login;
	}
#else
	delete m_wand_login;
#endif // WAND_EXTERNAL_STORAGE
	if (m_callback)
	{
		m_callback->OnPasswordRetrievalFailed();
	}
}

/* virtual */
SuspendedWandOperationWithInfo::~SuspendedWandOperationWithInfo()
{
	delete m_wand_info;
}

void SuspendedWandOperation::ClearReferenceToWindow(Window* window)
{
	if (m_doc && m_doc->GetWindow() == window)
	{
		ClearReferenceToDoc(m_doc);
	}
}

#endif // WAND_SUPPORT
