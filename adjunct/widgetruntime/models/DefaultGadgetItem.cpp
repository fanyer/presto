/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "adjunct/widgetruntime/models/DefaultGadgetItem.h"
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

DefaultGadgetItem::DefaultGadgetItem(const GadgetInstallerContext& install_info)
	: m_install_info(&install_info)
{
}

DefaultGadgetItem::~DefaultGadgetItem()
{
	if (!m_icon.IsEmpty())
	{
		m_icon.DecVisible(null_image_listener);
	}
}


OpString DefaultGadgetItem::GetName() const
{
	OpString name;
	OpStatus::Ignore(name.Set(m_install_info->m_display_name));
	return name;
}


OpString DefaultGadgetItem::GetStatus() const
{
	// .CStr() is a workaround for gcc 4.2, that cannot find the non-const copy constructor
	const OpStringC& author_name(m_install_info->GetAuthorName().CStr());
	OpString status;
	if (author_name.HasContent())
	{
		OpString format;
		OpStatus::Ignore(g_languageManager->GetString(
					Str::D_WIDGET_INSTALL_BY_AUTHOR, format));
		OpStatus::Ignore(status.AppendFormat(format, author_name.CStr()));
	}
	else
	{
		OpStatus::Ignore(g_languageManager->GetString(
					Str::D_WIDGET_INSTALL_NO_AUTHOR, status));
	}
	return status;
}


Image DefaultGadgetItem::GetIcon(INT32 size) const
{
	if (m_icon.IsEmpty())
	{
		OpGadgetClass* gadget_class = m_install_info->m_gadget_class.get();
		if (gadget_class)
		{
			if (OpStatus::IsSuccess(GadgetUtils::GetGadgetIcon(m_icon, gadget_class, size, size)))
			{
				OP_ASSERT(!m_icon.IsEmpty());
				m_icon.IncVisible(null_image_listener);
			}
		}
	}

	return m_icon;
}


const OpGadgetClass& DefaultGadgetItem::GetGadgetClass() const
{
	return *m_install_info->m_gadget_class;
}


BOOL DefaultGadgetItem::IsEmphasized() const
{
	return TRUE;
}

BOOL DefaultGadgetItem::CanBeUninstalled() const
{
#ifdef _UNIX_DESKTOP_
	return !m_install_info->m_install_globally;
#else
	return TRUE;
#endif // _UNIX_DESKTOP_
}

void DefaultGadgetItem::OnOpen()
{
	if (OpStatus::IsError(PlatformGadgetUtils::ExecuteGadget(*m_install_info)))
	{
		OpString title;
		OpStatus::Ignore(g_languageManager->GetString(
					Str::D_WIDGET_PANEL_LAUNCH_FAIL_TITLE, title));
		
		OpString message_format;
		OpStatus::Ignore(g_languageManager->GetString(
					Str::D_WIDGET_PANEL_LAUNCH_FAIL_MESSAGE, message_format));
		OpString message;
		OpStatus::Ignore(message.AppendFormat(message_format,
					m_install_info->m_display_name.CStr()));
		SimpleDialog::ShowDialog(WINDOW_NAME_PANEL_LAUNCH_FAIL, g_application->GetActiveDesktopWindow(),
				message, title, SimpleDialog::TYPE_OK,
				SimpleDialog::IMAGE_WARNING);
		
	}
}


BOOL DefaultGadgetItem::OnDelete()
{
	OpStatus::Ignore(PlatformGadgetUtils::UninstallGadget(*m_install_info));
	return FALSE;
}

#endif // WIDGET_RUNTIME_SUPPORT
