/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_util/shortcuts/DesktopShortcutInfo.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


GadgetInstallerContext::GadgetInstallerContext()
	: m_shortcuts_bitfield(DesktopShortcutInfo::SC_DEST_NONE),
	  m_create_launcher(FALSE),
	  m_install_globally(FALSE)
{
}


GadgetInstallerContext::GadgetInstallerContext(
		const GadgetInstallerContext& rhs)
	: m_gadget_class(rhs.m_gadget_class),
	  m_shortcuts_bitfield(rhs.m_shortcuts_bitfield),
	  m_create_launcher(rhs.m_create_launcher),
	  m_install_globally(rhs.m_install_globally)
{
	CopyStrings(rhs);
}


GadgetInstallerContext& GadgetInstallerContext::operator=(
		const GadgetInstallerContext& rhs)
{
	if (&rhs != this)
	{
		m_gadget_class = rhs.m_gadget_class;
	  	m_shortcuts_bitfield = rhs.m_shortcuts_bitfield;
		m_create_launcher = rhs.m_create_launcher;
		m_install_globally = rhs.m_install_globally;
		CopyStrings(rhs);
	}
	return *this;
}


void GadgetInstallerContext::CopyStrings(const GadgetInstallerContext& rhs)
{
	m_display_name.Set(rhs.m_display_name);
	m_normalized_name.Set(rhs.m_normalized_name);
	m_profile_name.Set(rhs.m_profile_name);
	m_dest_dir_path.Set(rhs.m_dest_dir_path);
	m_exec_cmd.Set(rhs.m_exec_cmd);
}


OpString GadgetInstallerContext::GetDeclaredName() const
{
	OpString name;
	if (NULL != m_gadget_class.get())
	{
		const OP_STATUS status = m_gadget_class->GetGadgetName(name);
		OP_ASSERT(OpStatus::IsSuccess(status));

		if (name.IsEmpty() && m_gadget_class->SupportsNamespace(GADGETNS_W3C_1_0))
		{
			const OP_STATUS status = g_languageManager->GetString(Str::S_UNTITLED_WIDGET_NAME, name);
			OP_ASSERT(OpStatus::IsSuccess(status));
		}
	}
	return name;
}


OpString GadgetInstallerContext::GetAuthorName() const
{
	OpString author_name;
	if (NULL != m_gadget_class.get())
	{
		const OP_STATUS status = m_gadget_class->GetGadgetAuthor(author_name);
		OP_ASSERT(OpStatus::IsSuccess(status));
	}
	return author_name;
}


OpString GadgetInstallerContext::GetDescription() const
{
	OpString description;
	if (NULL != m_gadget_class.get())
	{
		const OP_STATUS status = m_gadget_class->GetGadgetDescription(
				description);
		OP_ASSERT(OpStatus::IsSuccess(status));
	}
	return description;
}


OpString GadgetInstallerContext::GetIconFilePath() const
{
	OpString path;
	if (NULL != m_gadget_class.get())
	{
		INT32 unused_width = 0;
		INT32 unused_height = 0;
		const OP_STATUS status = m_gadget_class->GetGadgetIcon(0, path,
				unused_width, unused_height, FALSE);
		OP_ASSERT(path.HasContent() || OpStatus::IsError(status));
	}
	return path;
}

#endif // WIDGET_RUNTIME_SUPPORT
