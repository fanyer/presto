// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef _PLUGIN_SETUP_DIALOG_H_
#define _PLUGIN_SETUP_DIALOG_H_

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

/******************************************************************
 *
 * PluginSetupDialog
 *
 *
 ******************************************************************/

class PluginSetupDialog : public Dialog
{
public:
	PluginSetupDialog();
	~PluginSetupDialog();

	static void Create(DesktopWindow* parent_window);
 
	virtual void				OnInit();
	//void 						OnSetFocus();

	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_PLUGIN_PATH;}
	virtual const char*			GetWindowName()			{return "Plugin Setup Dialog";}
	const char*					GetHelpAnchor()			{return "plugins.html";}

private:
	void PopulatePluginList();
	void PopulatePluginPath();
	void SavePlugins();

private:
	static PluginSetupDialog* m_dialog;
	SimpleTreeModel m_plugin_model;
};


#endif
