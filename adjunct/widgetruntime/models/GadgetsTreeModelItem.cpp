/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/widgetruntime/GadgetInstallerContext.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModelItem.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModelItemImpl.h"
#include "modules/locale/oplanguagemanager.h"

namespace
{
	const UINT32 GADGET_TREE_ICON_SIZE = 32;
}


GadgetsTreeModelItem::GadgetsTreeModelItem(GadgetsTreeModelItemImpl* impl)
	: m_impl(impl)
	, m_locked(FALSE)
{
	OP_ASSERT(NULL != m_impl);
}

GadgetsTreeModelItem::~GadgetsTreeModelItem()
{
	OP_DELETE(m_impl);
	m_impl = NULL;
}


OP_STATUS GadgetsTreeModelItem::GetItemData(
		OpTreeModelItem::ItemData* item_data)
{
	switch (item_data->query_type)
	{
		case COLUMN_QUERY:
			switch (item_data->column_query_data.column)
			{
				case 0:
				{
					item_data->column_bitmap =
							m_impl->GetIcon(GADGET_TREE_ICON_SIZE);
					break;
				}
				case 1:
					// Separator column
					break;
				case 2:
					// .CStr() is a workaround for gcc 4.2, that cannot find the non-const copy constructor
					RETURN_IF_ERROR(
							item_data->column_query_data.column_text->Set(
									m_impl->GetName().CStr()));
					if (m_impl->IsEmphasized())
					{
						item_data->flags |= FLAG_BOLD;
					}
					else
					{
						item_data->flags &= ~FLAG_BOLD;
					}
					break;
			}
			break;

		case ASSOCIATED_TEXT_QUERY:
		{
			item_data->associated_text_query_data.associated_text_indentation = GADGET_TREE_ICON_SIZE+4;
			// .CStr() is a workaround for gcc 4.2, that cannot find the non-const copy constructor
			RETURN_IF_ERROR(item_data->associated_text_query_data.text->Set(m_impl->GetStatus().CStr()));
			break;
		}
	}

	return OpStatus::OK; 
}


void GadgetsTreeModelItem::OnOpen()
{
	m_locked = TRUE;
	m_impl->OnOpen();
	m_locked = FALSE;
}


void GadgetsTreeModelItem::OnDelete()
{
	m_locked = TRUE;

	OpString title;
	OpStatus::Ignore(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_TITLE, title));
	OpString message_format;
	OpStatus::Ignore(g_languageManager->GetString(
				Str::D_WIDGET_UNINSTALLER_CONFIRM, message_format));
	OpString message;
	OpStatus::Ignore(message.AppendFormat(message_format,
				m_impl->GetName().CStr()));
	if (SimpleDialog::DIALOG_RESULT_OK != SimpleDialog::ShowDialog(WINDOW_NAME_WIDGET_UNINSTALL_CONFIRM, 
				g_application->GetActiveDesktopWindow(), message, title,
				SimpleDialog::TYPE_OK_CANCEL, SimpleDialog::IMAGE_WARNING, NULL,
				NULL, 1))
	{
		m_locked = FALSE;
		return;
	}

	if (m_impl->OnDelete())
	{
		Delete();
	}

	m_locked = FALSE;
}


const OpGadgetClass& GadgetsTreeModelItem::GetGadgetClass() const
{
	return m_impl->GetGadgetClass();
}

BOOL GadgetsTreeModelItem::CanBeUninstalled() const
{
	return m_impl->CanBeUninstalled();
}

#endif // WIDGET_RUNTIME_SUPPORT
