/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "platforms/quix/dialogs/FileHandlerDialog.h"
#include "platforms/viewix/FileHandlerManager.h"

FileHandlerDialog* FileHandlerDialog::m_dialog = 0;


/**********************************************************************
 *
 * FileHandlerDialog
 *
 *
 *********************************************************************/

/**
 * Create
 *
 *
 */
//static
void FileHandlerDialog::Create(DesktopWindow* parent_window)
{
	if( !m_dialog )
	{
		m_dialog = OP_NEW(FileHandlerDialog, ());
		m_dialog->Init(parent_window);
	}
}


/**
 * OnInit
 *
 *
 */
void FileHandlerDialog::OnInit()
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
	if (treeview)
	{
		OpString s;
		TRAPD(rc,g_languageManager->GetStringL(Str::S_ASK, s));
		m_handler_model.SetColumnData(0, s.CStr() );
		TRAP(rc,g_languageManager->GetStringL(Str::DI_IDM_FILE_TYPE_GROUP, s));
		m_handler_model.SetColumnData(1, s.CStr() );
		TRAP(rc,g_languageManager->GetStringL(Str::S_PROGRAM, s));
		m_handler_model.SetColumnData(2, s.CStr() );

		treeview->SetCheckableColumn(0);
		treeview->SetTreeModel(&m_handler_model);

		const HandlerElement* item = 0;
		int pos;

		item = &FileHandlerManager::GetManager()->GetDefaultFileHandler();
		pos = m_handler_model.AddItem(UNI_L(""));
		m_handler_model.SetItemData(pos, 1, item->extension.CStr());
		m_handler_model.SetItemData(pos, 2, item->handler.CStr());
		if( item->ask )
			treeview->ToggleItem(pos);

		item = &FileHandlerManager::GetManager()->GetDefaultDirectoryHandler();
		pos = m_handler_model.AddItem(UNI_L(""));
		m_handler_model.SetItemData(pos, 1, item->extension.CStr());
		m_handler_model.SetItemData(pos, 2, item->handler.CStr());
		if( item->ask )
			treeview->ToggleItem(pos);

		const OpVector<HandlerElement>& list = FileHandlerManager::GetManager()->GetEntries();
		for( unsigned int i=0; i<list.GetCount(); i++ )
		{
			item = list.Get(i);
			if( item )
			{
				pos = m_handler_model.AddItem(UNI_L(""));
				m_handler_model.SetItemData(pos, 1, item->extension.CStr());
				m_handler_model.SetItemData(pos, 2, item->handler.CStr());
				if( item->ask )
				{
					treeview->ToggleItem(pos);
				}
			}
		}
	}
}


/**
 * ~FileHandlerDialog
 *
 */

FileHandlerDialog::~FileHandlerDialog()
{
	m_dialog = 0;
}



/**
 * OnSetFocus
 *
 */

void FileHandlerDialog::OnSetFocus()
{
	Dialog::OnSetFocus();

	// Workaround for column width=0 problem in OpTreeView
	// Sometimes (timing issue) the column widths remain zero when
	// we show the dialog. Seems that a layout request is not
	// sent. OnSetFocus() is called after the dialog becomes visible
	// [espen 2004-01-08]

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
	if( treeview )
	{
		treeview->GenerateOnLayout(TRUE);
	}
}


/**
 * OnInputAction
 *
 */
BOOL FileHandlerDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		HandlerElement file_handler, directory_handler;
		OpAutoVector<HandlerElement> list;

		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
		if (treeview)
		{
			file_handler.extension.Set(m_handler_model.GetItemString(0,1));
			file_handler.handler.Set(m_handler_model.GetItemString(0,2));
			file_handler.ask = treeview->IsItemChecked(0);

			directory_handler.extension.Set(m_handler_model.GetItemString(1,1));
			directory_handler.handler.Set(m_handler_model.GetItemString(1,2));
			directory_handler.ask = treeview->IsItemChecked(1);

			for( INT32 i=2; i<m_handler_model.GetItemCount(); i++ )
			{
				HandlerElement *item = OP_NEW(HandlerElement, ());
				if( item )
				{
					item->extension.Set(m_handler_model.GetItemString(i,1));
					item->handler.Set(m_handler_model.GetItemString(i,2));
					item->ask = treeview->IsItemChecked(i);
					list.Add(item);
				}
			}

			TRAPD(err,FileHandlerManager::GetManager()->WriteHandlersL(file_handler, directory_handler, list));
		}
	}
	else if( action->GetAction() == OpInputAction::ACTION_INSERT )
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
		if (treeview)
		{
			FileHandlerEditDialog * dialog = OP_NEW(FileHandlerEditDialog, ( this, -1, TRUE, OpString(), OpString(), TRUE ));
			dialog->Init(this);
		}
	}
	else if( action->GetAction() == OpInputAction::ACTION_CHANGE )
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
		if (treeview)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			if( pos >=0 )
			{
				OpString s1, s2;
				s1.Set(m_handler_model.GetItemString(pos,1));
				s2.Set(m_handler_model.GetItemString(pos,2));
				FileHandlerEditDialog * dialog = OP_NEW(FileHandlerEditDialog, (this, pos, pos>1, s1, s2, treeview->IsItemChecked(pos)));
				dialog->Init(this);
			}
		}
		return TRUE;
	}
	else if( action->GetAction() == OpInputAction::ACTION_DELETE )
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
		if (treeview)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			if (pos != -1 && pos > 1 )
			{
				m_handler_model.Delete(pos);
			}
		}
		return TRUE;
	}

	return Dialog::OnInputAction(action);
}


/**
 * OnItemChanged
 *
 */
BOOL FileHandlerDialog::OnItemChanged( INT32 index, const OpString& extension, const OpString& handler, BOOL ask )
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Handler_treeview");
	if (treeview)
	{
		if( index >= 0 )
		{
			m_handler_model.SetItemData(index, 1, extension.CStr());
			m_handler_model.SetItemData(index, 2, handler.CStr());
			if( ask != treeview->IsItemChecked(index) )
			{
				treeview->ToggleItem(index);
			}
		}
		else
		{
			INT32 pos = m_handler_model.AddItem(UNI_L(""));
			m_handler_model.SetItemData(pos, 1, extension.CStr());
			m_handler_model.SetItemData(pos, 2, handler.CStr());
			if( ask )
			{
				treeview->ToggleItem(pos);
			}
			treeview->SetSelectedItem(pos);
		}

	}

	return TRUE;
}




/**********************************************************************
 *
 * FileHandlerEditDialog
 *
 *
 *********************************************************************/

/**
 * OnInit
 *
 *
 */
void FileHandlerEditDialog::OnInit()
{
	SetWidgetEnabled( "Filetype_edit", m_edit_filetype );
	SetWidgetText( "Filetype_edit", m_extension.CStr() );
	SetWidgetText( "Handler_edit", m_handler.CStr() );
	SetWidgetValue( "Ask_checkbox", m_ask );
	SetWidgetFocus( m_edit_filetype ? "Extension_edit" : "Handler_edit" );
}


/**
 * OnInputAction
 *
 *
 */
BOOL FileHandlerEditDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		if( m_listener )
		{
			GetWidgetText("Filetype_edit", m_extension );
			m_extension.Strip();
			GetWidgetText("Handler_edit", m_handler );
			m_handler.Strip();
			m_ask = GetWidgetValue("Ask_checkbox");

			BOOL ok = m_listener->OnItemChanged( m_index,  m_extension, m_handler, m_ask );
			if( !ok )
			{
				return TRUE;
			}
		}
	}

	return Dialog::OnInputAction(action);
}

