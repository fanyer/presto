/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#if defined(M2_SUPPORT)

#include "adjunct/m2_ui/dialogs/CustomizeHeaderDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/header.h"
#include "adjunct/m2/src/engine/headerdisplay.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"

/***********************************************************************************
**
**	CustomizeHeaderDialog
**
***********************************************************************************/

CustomizeHeaderDialog::CustomizeHeaderDialog()
: m_header_model(1),
  m_tree_view(NULL)
{
}

CustomizeHeaderDialog::~CustomizeHeaderDialog()
{
}

void CustomizeHeaderDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
}

void CustomizeHeaderDialog::OnInit()
{
	m_up_button = (OpButton *)GetWidgetByName("up_button");

	if(m_up_button)
	{
		OpString str;
		g_languageManager->GetString(Str::D_MAILER_HEADER_MOVEUP, str);
		m_up_button->SetText(str.CStr());
		m_up_button->SetEnabled(FALSE);
	}

	m_down_button = (OpButton *)GetWidgetByName("down_button");

	if(m_down_button)
	{
		OpString str;
		g_languageManager->GetString(Str::D_MAILER_HEADER_MOVEDOWN, str);
		m_down_button->SetText(str.CStr());
		m_down_button->SetEnabled(FALSE);
	}

	m_add_button = (OpButton *)GetWidgetByName("add_button");

	if(m_add_button)
	{
		OpString str;
		g_languageManager->GetString(Str::DI_ID_BTN_ADD_LANG, str);
		m_add_button->SetText(str.CStr());
	}

	m_delete_button = (OpButton *)GetWidgetByName("delete_button");

	if(m_delete_button)
	{
		OpString str;
		g_languageManager->GetString(Str::S_DELETE, str);
		m_delete_button->SetText(str.CStr());
		m_delete_button->SetEnabled(FALSE);
	}

	m_tree_view = (OpTreeView*)GetWidgetByName("Header_treeview");

	if (m_tree_view)
	{
		OpString str;
		g_languageManager->GetString(Str::D_MAILER_HEADER_LIST_TITLE, str);
		m_header_model.SetColumnData(0, str.CStr());
		m_tree_view->SetTreeModel(&m_header_model);
		m_tree_view->SetCheckableColumn(0);
		m_tree_view->SetUserSortable(FALSE);
	}


	OpString caption_string;
	g_languageManager->GetString(Str::D_MAILER_CUSTOMIZE_HEADERS, caption_string);
	SetTitle(caption_string.CStr());

	m_display = NULL;
	if(g_m2_engine && g_m2_engine->GetAccountManager())
	{
		m_display = g_m2_engine->GetAccountManager()->GetHeaderDisplay();
	}

	if(m_display)
	{
		INT32 i;
		const HeaderDisplayItem *item;
		for(i=0; (item = m_display->GetItem(i)) != NULL; i++)
		{
			OpString16 str;
			str.Set(item->m_headername);
			BOOL checked = m_display->GetDisplay(item->m_headername);
			if(!Header::IsInternalHeader(item->m_type))
			{
				m_header_model.AddItem(str.CStr(), NULL, 0, -1, (void *)item, 0, OpTypedObject::UNKNOWN_TYPE, checked);
			}
		}
	}
}

UINT32 CustomizeHeaderDialog::OnOk()
{
	if(m_display)
	{
		INT32 i, count;
		for (i = 0, count = m_tree_view->GetItemCount(); i < count; ++i)
		{
			HeaderDisplayItem *item = (HeaderDisplayItem *)m_header_model.GetItemUserData(i);
			if(item)
			{
				m_display->SetDisplay(item->m_headername, m_tree_view->IsItemChecked(i));
			}
		}

		UINT32 j;
		for(j=0; j<m_moves.GetCount(); j++)
		{
			INT32 move = m_moves.Get(j);
			if(move < 0)
			{
				m_display->MoveUp(-move);
			}
			else
			{
				m_display->MoveDown(move);
			}
		}

		m_display->SaveSettings();

	}

	return 0;
}


void CustomizeHeaderDialog::OnCancel()
{
}

void CustomizeHeaderDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if(widget == m_up_button)
	{
		INT32 pos = m_tree_view->GetSelectedItemPos();
		if(pos >= 1)
		{
			if(m_display)
			{
				HeaderDisplayItem *item = (HeaderDisplayItem *)m_header_model.GetItemUserData(pos);

				if(item)
				{
					BOOL checked = m_tree_view->IsItemChecked(pos);

					m_header_model.Delete(pos);

					OpString16 str;
					str.Set(item->m_headername);
					if(!Header::IsInternalHeader(item->m_type))
					{
						m_header_model.InsertBefore(pos-1, str.CStr(), NULL, 0, (void *)item, 0, OpTypedObject::UNKNOWN_TYPE, checked);
					}
					m_moves.Add(-pos);
					m_tree_view->SetSelectedItem(pos-1);
				}
			}

		}
		return;
	}
	if(widget == m_down_button)
	{
		INT32 pos = m_tree_view->GetSelectedItemPos();
		if((pos >= 0) && (pos < m_tree_view->GetItemCount() - 1))
		{
			if(m_display)
			{
				HeaderDisplayItem *item = (HeaderDisplayItem *)m_header_model.GetItemUserData(pos);

				if(item)
				{
					BOOL checked = m_tree_view->IsItemChecked(pos);

					m_header_model.Delete(pos);

					OpString16 str;
					str.Set(item->m_headername);
					if(!Header::IsInternalHeader(item->m_type))
					{
						m_header_model.InsertAfter(pos, str.CStr(), NULL, 0, (void *)item, 0, OpTypedObject::UNKNOWN_TYPE, checked);
					}
					m_moves.Add(pos);
					m_tree_view->SetSelectedItem(pos+1);
				}
			}

		}
		return;
	}

	if(widget == m_add_button)
	{
		AddCustomHeaderDialog *dialog = OP_NEW(AddCustomHeaderDialog, ());
		if(dialog)
		{
			dialog->SetDialogListener(this);
			dialog->Init(this);
		}
		return;
	}

	if(widget == m_delete_button)
	{
		INT32 pos = m_tree_view->GetSelectedItemPos();
		HeaderDisplayItem *item = (HeaderDisplayItem *)m_header_model.GetItemUserData(pos);

		if(item && item->m_type == Header::UNKNOWN)
		{
			m_header_model.Delete(pos);
			item->Out();
		}

		return;
	}

	Dialog::OnClick(widget,id);
}

void CustomizeHeaderDialog::OnOk(Dialog* dialog, UINT32 result)
{
	AddCustomHeaderDialog *customDialog = (AddCustomHeaderDialog *)dialog;
	if(customDialog)
	{
		OpString header_name;
		if(OpStatus::IsSuccess(customDialog->GetHeaderName(header_name)))
		{
			OpString8 header;
			header.Set(header_name.CStr());

			if(m_display && !m_display->FindItem(header) && header_name.HasContent())
			{
				m_display->AddHeader(header);
				const HeaderDisplayItem *item = m_display->FindItem(header);
				OP_ASSERT(item);
				BOOL checked = m_display->GetDisplay(header);
				m_header_model.AddItem(header_name.CStr(), NULL, 0, -1, (void *)item, 0, OpTypedObject::UNKNOWN_TYPE, checked);
			}
		}
	}
}

void CustomizeHeaderDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if(widget == GetWidgetByName("Header_treeview"))
	{
		INT32 pos = m_tree_view->GetSelectedItemPos();
		if(pos >= 0)
		{
			HeaderDisplayItem *item = (HeaderDisplayItem *)m_header_model.GetItemUserData(pos);

			if(item)
			{
				m_up_button->SetEnabled(pos > 0);
				m_down_button->SetEnabled(pos < m_tree_view->GetItemCount() - 1);
				m_delete_button->SetEnabled(item->m_type == Header::UNKNOWN);
			}

		}
	}
}

/***********************************************************************************
**
**	AddCustomHeaderDialog
**
***********************************************************************************/

AddCustomHeaderDialog::AddCustomHeaderDialog()
{
}

AddCustomHeaderDialog::~AddCustomHeaderDialog()
{
}

void AddCustomHeaderDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);

	m_edit = (OpEdit *)GetWidgetByName("header_name");
}

void AddCustomHeaderDialog::OnInit()
{
}

UINT32 AddCustomHeaderDialog::OnOk()
{
	return 0;
}

void AddCustomHeaderDialog::OnCancel()
{
}

#endif // M2_SUPPORT
