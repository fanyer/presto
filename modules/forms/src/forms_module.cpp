/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/forms/forms_module.h"
#include "modules/forms/form_his.h"
#ifdef FORMS_KEYGEN_SUPPORT
# include "modules/forms/src/formsubmitter.h"
#endif // FORMS_KEYGEN_SUPPORT

void FormsModule::InitL(const OperaInitInfo& info)
{
	m_forms_history = OP_NEW_L(FormsHistory, ());
}

void FormsModule::Destroy()
{
	OP_DELETE(m_forms_history);

#ifdef FORMS_KEYGEN_SUPPORT
	FormSubmitter* to_delete = m_submits_in_progress;
	while (to_delete)
	{
		FormSubmitter* next = to_delete->GetNextInList();
		OP_DELETE(to_delete);
		to_delete = next;
	}
#endif // FORMS_KEYGEN_SUPPORT
}

