/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "PluginPathDialog.h"

#include "modules/locale/locale-enum.h"
#include "modules/viewers/viewers.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"

#include "platforms/x11api/plugins/unix_pluginpath.h"

PluginPathDialog*     PluginPathDialog::m_dialog         = 0;
PluginRecoveryDialog* PluginRecoveryDialog::m_dialog     = 0;
BOOL                  PluginRecoveryDialog::m_ok_pressed = FALSE;
PluginCheckDialog*    PluginCheckDialog::m_dialog        = 0;

typedef PluginPathElement PluginPathNode;

/***********************************************************************************
**
**
**
***********************************************************************************/
static const OpVector<PluginPathNode>& GetPluginPathNodeList()
{
	return PluginPathList::Self()->Get();
}

/***********************************************************************************
**
**
**
***********************************************************************************/
static void ViewerActionToString( OpString &text, int action )
{
	Str::LocaleString string_id(Str::NOT_A_STRING);

	switch(action)
	{
		case VIEWER_SAVE:
		{
			string_id = Str::DI_IDM_SAVE;
		}
		break;

		case VIEWER_OPERA:
		{
			string_id = Str::DI_IDM_OPERA;
		}
		break;

		case VIEWER_PASS_URL:
		case VIEWER_APPLICATION:
		{
			string_id = Str::DI_IDM_APP;
		}
		break;

		case VIEWER_REG_APPLICATION:
		{
			string_id = Str::DI_IDM_REG_APP;
		}
		break;

		case VIEWER_PLUGIN:
		{
			string_id = Str::DI_IDM_PLUGIN;
		}
		break;

		case VIEWER_ASK_USER:
		default:
		{
			string_id = Str::D_FILETYPE_SHOWDOWNLOADDIAL;
		}
		break;
	}

	TRAPD(rc,g_languageManager->GetStringL(string_id, text));
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginPathDialog::OnFileChoosingDone(DesktopFileChooser* chooser,
										  const DesktopFileChooserResult& result)
{
	m_selected_directory.Wipe();
	OP_ASSERT(result.selected_filter == -1);
	if (result.files.Get(0))
		m_selected_directory.Set(*result.files.Get(0));

	if( m_selected_directory.Length() > 0 )
	{
		OpString candidate;
		candidate.Set(m_selected_directory.CStr());
		if(candidate.Length() > 1 && candidate.CStr()[candidate.Length()-1] == '/' )
			candidate.CStr()[candidate.Length()-1] = 0;

		INT32 pos = 0;
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if (!m_action_change)
		{
			pos = m_path_model.AddItem(UNI_L(""));
			if (treeview)
				treeview->SetCheckboxValue(pos, TRUE);
		}
		else
		{
			if (treeview)
				pos = treeview->GetSelectedItemModelPos();	
		}
		m_path_model.SetItemData(pos, 1, candidate.CStr());
		UpdateFullPath();
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginPathDialog::OnInit()
{
	OpString path;

	if( m_dest_edit )
	{
		m_dest_edit->GetText(path);
	}
	else
	{
		path.Set(g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath));
	}

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
	if (treeview)
	{
		OpString s1, s2;
		TRAPD(rc,g_languageManager->GetStringL(Str::DI_USE_PLUGIN, s1));
		TRAP(rc,g_languageManager->GetStringL(Str::D_PLUGIN_PATH_COMPONENTS, s2));
		m_path_model.SetColumnData(0, s1.CStr() );
		m_path_model.SetColumnData(1, s2.CStr() );

		treeview->SetCheckableColumn(0);
		treeview->SetTreeModel(&m_path_model);
		treeview->SetUserSortable(FALSE);

		m_block_change_detection = TRUE;

		const OpVector<PluginPathNode>& path_list = GetPluginPathNodeList();
		for( UINT32 i=0; i<path_list.GetCount(); i++ )
		{
			const PluginPathNode* item = path_list.Get(i);
			if( item )
			{
				if( item->state != 0 )
				{
					int pos = m_path_model.AddItem(UNI_L(""));
					m_path_model.SetItemData(pos, 1, item->path.CStr());
					if( item->state == 1 )
					{
						treeview->ToggleItem(pos);
					}
				}
			}
		}
		m_block_change_detection = FALSE;

		treeview->SetSelectedItem(0);
	}

	OpEdit *edit = (OpEdit*)GetWidgetByName("Path_edit");
	if( edit )
	{
		edit->SetText(path.CStr());
		edit->SetReadOnly(TRUE);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
PluginPathDialog::~PluginPathDialog()
{
	m_dialog = 0;
	OP_DELETE(m_chooser);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginPathDialog::OnSetFocus()
{
	Dialog::OnSetFocus();

	// Workaround for column width=0 problem in OpTreeView
	// Sometimes (timing issue) the column widths remain zero when
	// we show the dialog. Seems that a layout request is not
	// sent. OnSetFocus() is called after the dialog becomes visible
	// [espen 2004-01-08]

	OpTreeView* path_tree_view = (OpTreeView*)GetWidgetByName("Path_treeview");
	if( path_tree_view )
	{
		path_tree_view->GenerateOnLayout(TRUE);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginPathDialog::OnItemChanged( OpWidget *widget, INT32 pos )
{
	if( !m_block_change_detection )
	{
		UpdateFullPath();
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL PluginPathDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		OpEdit* edit = (OpEdit*)GetWidgetByName("Path_edit");
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if( edit && treeview )
		{
			const OpVector<PluginPathNode>& path_list = GetPluginPathNodeList();
			OpAutoVector<PluginPathNode> items;

			for( INT32 i=0;i<m_path_model.GetItemCount(); i++ )
			{
				PluginPathNode* item = OP_NEW(PluginPathNode, ());
				if( item )
				{
					item->path.Set(m_path_model.GetItemString(i,1));
					item->state = treeview->IsItemChecked(i) ? 1 : 2;
					items.Add(item);
				}
			}

			// We add all non-zero entries from the preference list that are
			// not in the dialog box list

			for( UINT32 i=0; i<path_list.GetCount(); i++ )
			{
				const PluginPathNode* i1 = path_list.Get(i);
				if( i1 && i1->state != 0 )
				{
					BOOL in_list = FALSE;
					for( UINT32 j=0; j<items.GetCount(); j++ )
					{
						PluginPathNode* i2 = items.Get(j);
						if( i2 && i2->path.Compare(i1->path) == 0 )
						{
							in_list = TRUE;
							break;
						}
					}
					if( !in_list )
					{
						PluginPathNode* item = OP_NEW(PluginPathNode, ());
						if( item )
						{
							item->path.Set(i1->path);
							item->state = 0;
							items.Add(item);
						}
					}
				}
			}

			TRAPD(err,PluginPathList::Self()->WriteL(items));

			UpdateFullPath();

			OpString path;
			edit->GetText(path);
			if( m_dest_edit )
			{
				m_dest_edit->SetText(path.CStr());
			}
		}
	}
	else if( action->GetAction() == OpInputAction::ACTION_INSERT )
	{
		m_action_change = FALSE;
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if (treeview)
		{
			if (!m_chooser)
				RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

			m_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
			m_request.initial_filter      = -1;
			m_request.get_selected_filter = FALSE;
			m_request.fixed_filter        = FALSE;

			m_chooser->Execute(GetOpWindow(), this, m_request);
		}
		return TRUE;
	}
	else if( action->GetAction() == OpInputAction::ACTION_CHANGE )
	{
		m_action_change = TRUE;
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if (treeview)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			// pos to update
			if( pos >=0 )
			{
				if (!m_chooser)
					RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

				m_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
				m_request.initial_filter      = -1;
				m_request.get_selected_filter = FALSE;
				m_request.fixed_filter        = FALSE;
				m_request.initial_path.Set(m_path_model.GetItemString(pos,1));
				m_chooser->Execute(GetOpWindow(), this, m_request);
			}
		}

		return TRUE;
	}
	else if( action->GetAction() == OpInputAction::ACTION_DELETE )
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if (treeview)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			if (pos != -1)
			{
				m_path_model.Delete(pos);
				UpdateFullPath();
			}
		}
		return TRUE;
	}
	else if( action->GetAction() == OpInputAction::ACTION_MOVE_ITEM_UP )
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if (treeview)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			if( pos > 0 )
			{
				OpString data;
				data.Set(m_path_model.GetItemString(pos,1));

				m_path_model.Delete(pos);

				INT32 new_pos = m_path_model.InsertBefore(pos-1);
				m_path_model.SetItemData(new_pos, 1, data.CStr());

				treeview->SetCheckboxValue(new_pos, FALSE);

				treeview->SetSelectedItem(pos-1);
				UpdateFullPath();
			}
		}
		return TRUE;
	}
	else if( action->GetAction() == OpInputAction::ACTION_MOVE_ITEM_DOWN )
	{
		OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
		if (treeview)
		{
			INT32 pos = treeview->GetSelectedItemModelPos();
			if( pos != -1 && pos+1 < m_path_model.GetItemCount() )
			{
				OpString data;
				data.Set(m_path_model.GetItemString(pos,1));

				m_path_model.Delete(pos);

				INT32 new_pos = m_path_model.InsertAfter(pos);
				m_path_model.SetItemData(new_pos, 1, data.CStr());

				treeview->SetCheckboxValue(new_pos, FALSE);
				treeview->SetSelectedItem(pos+1);
				UpdateFullPath();
			}
		}
		return TRUE;
	}

	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginPathDialog::UpdateFullPath()
{
	OpString path;

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
	if (treeview)
	{
		for( INT32 i=0;i<m_path_model.GetItemCount(); i++ )
		{
			if( treeview->IsItemChecked(i) )
			{
				if( path.Length() > 0 )
				{
					path.Append(UNI_L(":"));
				}
				path.Append(m_path_model.GetItemString(i,1));
			}
		}
	}

	OpEdit *edit = (OpEdit*)GetWidgetByName("Path_edit");
	if( edit )
	{
		edit->SetText(path.CStr());
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//static
void PluginPathDialog::Create(DesktopWindow* parent_window, OpEdit* dest_edit )
{
	if( !m_dialog )
	{
		m_dialog = OP_NEW(PluginPathDialog, (dest_edit));
		m_dialog->Init(parent_window);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
PluginRecoveryDialog::~PluginRecoveryDialog()
{
	m_dialog = 0;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginRecoveryDialog::OnSetFocus()
{
	Dialog::OnSetFocus();

	// Workaround for column width=0 problem in OpTreeView [espen 2004-09-16]
	OpTreeView* path_tree_view = (OpTreeView*)GetWidgetByName("Path_treeview");
	if( path_tree_view )
	{
		path_tree_view->GenerateOnLayout(TRUE);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginRecoveryDialog::OnInit()
{
	m_ok_pressed = FALSE;


	OpMultilineEdit *edit = (OpMultilineEdit*)GetWidgetByName("Help_label");
	if( edit )
	{
		edit->SetFlatMode();
	}

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
	if (treeview)
	{
		OpStringC path = g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath);

		OpString s1, s2;
		TRAPD(rc,g_languageManager->GetStringL(Str::DI_USE_PLUGIN, s1));
		TRAP(rc,g_languageManager->GetStringL(Str::D_PLUGIN_PATH_COMPONENTS, s2));

		m_path_model.SetColumnData(0, s1.CStr() );
		m_path_model.SetColumnData(1, s2.CStr() );

		treeview->SetCheckableColumn(0);
		treeview->SetTreeModel(&m_path_model);
		treeview->SetUserSortable(FALSE);

		const OpVector<PluginPathNode>& path_list = GetPluginPathNodeList();
		for( UINT32 i=0; i<path_list.GetCount(); i++ )
		{
			PluginPathNode* item = path_list.Get(i);
			if( item )
			{
				int pos = m_path_model.AddItem(UNI_L(""));
				m_path_model.SetItemData(pos, 1, item->path.CStr());
				if( item->state == 1 )
				{
					treeview->ToggleItem(pos);
				}
			}
		}

		treeview->SetSelectedItem(0);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginRecoveryDialog::SaveSetting()
{
	OpAutoVector<PluginPathNode> items;
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Path_treeview");
	if (treeview)
	{
		for( INT32 i=0;i<m_path_model.GetItemCount(); i++ )
		{
			PluginPathNode* item = OP_NEW(PluginPathNode, ());
			if( item )
			{
				item->path.Set(m_path_model.GetItemString(i,1));
				item->state = treeview->IsItemChecked(i) ? 1 : 2;
				items.Add(item);
			}
		}

		TRAPD(err,PluginPathList::Self()->WriteL(items));
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL PluginRecoveryDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		SaveSetting();
		m_ok_pressed = TRUE;
	}
	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//static
BOOL PluginRecoveryDialog::Create(DesktopWindow* parent_window)
{
	if( !m_dialog )
	{
		m_dialog = OP_NEW(PluginRecoveryDialog, ());
		m_dialog->Init(parent_window);
		return PluginRecoveryDialog::m_ok_pressed;
	}
	else
	{
		return FALSE;
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
// static
void PluginRecoveryDialog::TestPluginsLoaded(DesktopWindow* parent_window)
{
	// If the 'ReadingPlugins' flag is true, then this means that an earlier
	// scan crashed Opera. Plug-ins has not been read in InitL() so we ask the
	// user here if plug-ins should be read.

	if(g_pcunix->GetIntegerPref(PrefsCollectionUnix::ReadingPlugins))
	{
		g_pcunix->WriteIntegerL(PrefsCollectionUnix::ReadingPlugins, FALSE);

		if( PluginRecoveryDialog::Create(parent_window) )
		{
			OpStatus::Ignore(g_plugin_viewers->RefreshPluginViewers(TRUE));
		}
		else
		{
			g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::PluginsEnabled, FALSE);
		}
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
PluginCheckDialog::~PluginCheckDialog()
{
	m_dialog = 0;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginCheckDialog::OnSetFocus()
{
	Dialog::OnSetFocus();

	// Workaround for column width=0 problem in OpTreeView [espen 2004-09-16]
	OpTreeView* plugin_tree_view = (OpTreeView*)GetWidgetByName("Plugin_treeview");
	if( plugin_tree_view )
	{
		plugin_tree_view->GenerateOnLayout(TRUE);
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
INT32 PluginCheckDialog::Populate( OpTreeView* treeview, SimpleTreeModel* model )
{
	INT32 num_items = 0;

	TRAPD(err, g_plugin_viewers->RefreshPluginViewers(FALSE));

	if( treeview && model)
	{
		OpString s1, s2, s3, s4;
		TRAPD(rc,g_languageManager->GetStringL(Str::DI_USE_PLUGIN, s1));
		TRAP(rc,g_languageManager->GetStringL(Str::SI_IDLABEL_PLUGINNAME, s2));
		TRAP(rc,g_languageManager->GetStringL(Str::SI_IDSTR_MIME_HEADER, s3));
		TRAP(rc,g_languageManager->GetStringL(Str::D_PLUGIN_CURRENT_ACTION, s4));
		model->SetColumnData(0, s1.CStr() );
		model->SetColumnData(1, s2.CStr() );
		model->SetColumnData(2, s3.CStr() );
		model->SetColumnData(3, s4.CStr() );
		treeview->SetCheckableColumn(0);
		treeview->SetTreeModel(model);
	}

	ChainedHashIterator* viewer_iter;
	Viewer* viewer;

	g_viewers->CreateIterator(viewer_iter);
	while (viewer_iter && (viewer=g_viewers->GetNextViewer(viewer_iter)) != NULL)
	{
		ViewAction action = viewer->GetAction();
		const uni_char* viewer_plugin_path = viewer->GetDefaultPluginViewerPath();
		if (viewer_plugin_path)
		{
			// We have a default plug-in path already
			for(UINT32 j=0; j<viewer->GetPluginViewerCount(); j++)
			{
				PluginViewer* plugin_viewer = viewer->GetPluginViewer(j);
				const uni_char* plugin_viewer_path;
				// Select the plug-in that has the same path as the currently active path
				if (plugin_viewer &&
					(plugin_viewer_path=plugin_viewer->GetPath())!=NULL &&
					uni_stricmp(plugin_viewer_path, viewer_plugin_path)==0 &&
					action!=VIEWER_PLUGIN) // Show it if the viewer action is to not use plug-ins now.
				{
					num_items ++;
					if( treeview && model )
					{
						int pos = model->AddItem(UNI_L(""),NULL,0,-1,viewer);
						OpString tmp;
						tmp.Empty();
						tmp.AppendFormat(UNI_L("%s - %s"), plugin_viewer->GetProductName(), plugin_viewer_path);
						model->SetItemData(pos, 1, tmp.CStr() );
						model->SetItemData(pos, 2, viewer->GetContentTypeString());
						ViewerActionToString( tmp, action );
						model->SetItemData(pos, 3, tmp.CStr());
						treeview->ToggleItem(pos);
					}
					break;
				}
			}
		}
		else if( viewer->GetPluginViewerCount() > 0 )
		{
			// Delayed until SaveSetting()

			if( action != VIEWER_PLUGIN )
			{
				PluginViewer* plugin_viewer = viewer->GetPluginViewer(0);
				if(plugin_viewer)
				{
					num_items ++;
					if( treeview && model )
					{
						int pos = model->AddItem(UNI_L(""),NULL,0,-1,viewer);
						OpString tmp;
						tmp.Empty();
						tmp.AppendFormat(UNI_L("%s - %s"), plugin_viewer->GetProductName(), plugin_viewer->GetPath());
						model->SetItemData(pos, 1, tmp.CStr());
						model->SetItemData(pos, 2, viewer->GetContentTypeString());
						ViewerActionToString( tmp, action );
						model->SetItemData(pos, 3, tmp.CStr());

						treeview->ToggleItem(pos);
					}
				}
			}
		}
		else if( action == VIEWER_PLUGIN )
		{
			// Delayed until SaveSetting()
		}
	}

	OP_DELETE(viewer_iter);

	return num_items;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginCheckDialog::OnInit()
{
	Populate( (OpTreeView*)GetWidgetByName("Plugin_treeview"), &m_plugin_model );
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void PluginCheckDialog::SaveSetting()
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Plugin_treeview");
	if (treeview)
	{
		for(int i=0; i < m_plugin_model.GetItemCount(); i++)
		{
			if(treeview->IsItemChecked(i))
			{
				((Viewer*)m_plugin_model.GetItemUserData(i))->SetAction(VIEWER_PLUGIN, 
																		TRUE); // user-defined
			}
		}
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL PluginCheckDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		SaveSetting();
	}
	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
//static
void PluginCheckDialog::Create(DesktopWindow* parent_window)
{
	if( !m_dialog )
	{
		INT32 new_plugin_count = PluginCheckDialog::Populate(0,0);
		if( new_plugin_count == 0 )
		{
			OpString title, message;
			TRAPD(err, g_languageManager->GetStringL(Str::D_PLUGIN_ASSOCIATION, title));
			TRAP(err, g_languageManager->GetStringL(Str::D_PLUGIN_ASSOCIATION_NO_PLUGINS, message));
			SimpleDialog::ShowDialog(WINDOW_NAME_PLUGIN_CHECK, 0, message.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_INFO );
		}
		else
		{
			m_dialog = OP_NEW(PluginCheckDialog, ());
			m_dialog->Init(parent_window);
		}
	}
}
