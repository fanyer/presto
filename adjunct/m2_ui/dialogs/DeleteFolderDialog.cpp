/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"

#ifdef M2_SUPPORT
#include "DeleteFolderDialog.h"

#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/engine.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

/*****************************************************************************
**
**	DeleteFolderDialog::Init
**
*****************************************************************************/

OP_STATUS DeleteFolderDialog::Init(UINT32 id, DesktopWindow* parent_window)
{
	m_id = id;

	OpString title, message, format, name;

	Index *index = g_m2_engine->GetIndexById(m_id);
	if (index)
	{
		index->GetName(name);
	}
	if (name.IsEmpty())
		name.Set("");

	g_languageManager->GetString(Str::S_REMOVE_FOLDER, format);

	title.Reserve(name.Length() + format.Length());
	uni_sprintf(title.CStr(), format.CStr(), name.CStr());

	if (id >= IndexTypes::FIRST_NEWSGROUP && id < IndexTypes::LAST_NEWSGROUP)
	{
		g_languageManager->GetString(Str::S_UNSUBSCRIBE_NEWSGROUP_WARNING, message);
	}
	else if (id >= IndexTypes::FIRST_IMAP && id < IndexTypes::LAST_IMAP)
	{
		g_languageManager->GetString(Str::S_UNSUBSCRIBE_IMAP_WARNING, message);
	}
	else
	{
		g_languageManager->GetString(Str::S_DELETE_FOLDER_WARNING, format);

		message.Reserve(name.Length() + format.Length());
		uni_sprintf(message.CStr(), format.CStr(), name.CStr());
	}
	return SimpleDialog::Init(WINDOW_NAME_DELETE_MAIL_FOLDER, title, message, parent_window);
}

/*****************************************************************************
**
**	DeleteFolderDialog::OnOk
**
*****************************************************************************/

UINT32 DeleteFolderDialog::OnOk()
{
	g_m2_engine->RemoveIndex(m_id);
	return 0;
}
#endif //M2_SUPPORT
