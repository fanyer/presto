/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsfile.h"

#include "modules/viewers/viewers.h"
#include "modules/widgets/OpWidgetFactory.h"

#include "PluginPathDialog.h"
#include "PluginSetupDialog.h"

PluginSetupDialog* PluginSetupDialog::m_dialog = 0;

//static
void PluginSetupDialog::Create(DesktopWindow* parent_window)
{
	if( !m_dialog )
	{
		m_dialog = OP_NEW(PluginSetupDialog, ());
		m_dialog->Init(parent_window);
	}
}


PluginSetupDialog::PluginSetupDialog() :
	m_plugin_model(3)
{
}


PluginSetupDialog::~PluginSetupDialog()
{
	m_dialog = 0;
}


void PluginSetupDialog::OnInit()
{
	PopulatePluginList();
	PopulatePluginPath();
}


BOOL PluginSetupDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		SavePlugins();
	}
	else if( action->GetAction() == OpInputAction::ACTION_FIND_PLUGINS )
	{
		g_plugin_viewers->RefreshPluginViewers(FALSE);

		PluginCheckDialog::Create(this);
		PopulatePluginList();
		return TRUE;
	}
	else if( action->GetAction() == OpInputAction::ACTION_SHOW_PLUGIN_PATH_SELECTOR )
	{
		OpEdit *edit = (OpEdit*)GetWidgetByName("Plugin_edit");
		if( edit )
		{
			PluginPathDialog::Create(this, edit);

			g_plugin_viewers->RefreshPluginViewers(FALSE);

			PopulatePluginList();
		}
		return TRUE;
	}

	return Dialog::OnInputAction(action);
}


void PluginSetupDialog::SavePlugins()
{
	OpEdit *edit = (OpEdit*)GetWidgetByName("Plugin_edit");
	if( edit )
	{
		OpString path;
		edit->GetText( path );
		TRAPD(err,g_pcapp->WriteStringL(PrefsCollectionApp::PluginPath, path));
	}

	TRAPD(err, g_viewers->WriteViewersL());
	TRAP(err,g_prefsManager->CommitL());

	// Assuming that plugins are always enabled here.
	g_plugin_viewers->RefreshPluginViewers(FALSE);
}


void PluginSetupDialog::PopulatePluginList()
{
	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Plugins_treeview");
	if (treeview)
	{
		if( !treeview->GetTreeModel() )
		{
			OpString s1, s2, s3;
			TRAPD(rc,g_languageManager->GetStringL(Str::SI_IDLABEL_PLUGINNAME, s1));
			TRAP(rc,g_languageManager->GetStringL(Str::SI_IDLABEL_PLUGINDESC, s2));
			TRAP(rc,g_languageManager->GetStringL(Str::SI_IDLABEL_PLUGINPATH, s3));
			m_plugin_model.SetColumnData(0, s1.CStr());
			m_plugin_model.SetColumnData(1, s2.CStr());
			m_plugin_model.SetColumnData(2, s3.CStr());
			treeview->SetTreeModel(&m_plugin_model);
			treeview->Sort(0,TRUE);
		}
		else
		{
			m_plugin_model.RemoveAll();
		}

		for( unsigned int i=0; i < g_plugin_viewers->GetPluginViewerCount(); i++ )
		{
			PluginViewer* pv = g_plugin_viewers->GetPluginViewer(i);
			if( pv )
			{
				const uni_char* name = pv->GetProductName();
				const uni_char* path = pv->GetPath();
				const uni_char* description = pv->GetDescription();

				// Avoid duplicated entries
				BOOL ok = TRUE;
				for( int j=0; j<m_plugin_model.GetItemCount(); j++ )
				{
					const uni_char* list_string = m_plugin_model.GetItemString(j,0);
					if( name && list_string && uni_strcmp(name,list_string) == 0 )
					{
						list_string = m_plugin_model.GetItemString(j,2);
						if( path && list_string && uni_strcmp(path,list_string) == 0 )
						{
							ok = FALSE;
							break;
						}
					}
				}

				if( ok )
				{
					int pos = m_plugin_model.AddItem(name);
					m_plugin_model.SetItemData(pos, 1, description);
					m_plugin_model.SetItemData(pos, 2, path);
				}
			}
		}

		treeview->Sort(0,TRUE);
		treeview->SetSelectedItem(0);
	}
}


void PluginSetupDialog::PopulatePluginPath()
{
	OpEdit *edit = (OpEdit*)GetWidgetByName("Plugin_edit");
	if( edit )
	{
		OpStringC path = g_pcapp->GetStringPref(PrefsCollectionApp::PluginPath);
		edit->SetText(path.CStr());
		edit->SetReadOnly(TRUE);
	}
}
