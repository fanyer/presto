// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __PLUGIN_PATH_DIALOG_H__
#define __PLUGIN_PATH_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/desktop_pi/desktop_file_chooser.h"

#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
class OpEdit;

/***************************************************************
 *
 * PluginPathDialog
 *
 ***************************************************************/

class PluginPathDialog : public Dialog, 
						 public DesktopFileChooserListener
{
public:
	PluginPathDialog(OpEdit* dest_edit) : 
		m_path_model(2), 
		m_dest_edit(dest_edit), 
		m_block_change_detection(FALSE),
		m_chooser(0),
		m_action_change(FALSE) {  }

	~PluginPathDialog();

	static void Create(DesktopWindow* parent_window, OpEdit* dest_edit);
 
	virtual void				OnInit();
	void 						OnSetFocus();

	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_PLUGIN_PATH;}
	virtual const char*			GetWindowName()			{return "Plugin Path Dialog";}

	INT32						GetButtonCount() { return 2; };

	// DesktopFileChooserListener
	void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

	// OpWidgetListener
	void OnItemChanged(OpWidget *widget, INT32 pos);

private:
	void UpdateFullPath();

private:
	static PluginPathDialog* m_dialog;
	SimpleTreeModel m_path_model;	
	OpEdit* m_dest_edit;
	BOOL m_block_change_detection;
	OpString m_selected_directory;
	DesktopFileChooserRequest m_request;
	DesktopFileChooser* m_chooser;
	BOOL m_action_change;
};


/***************************************************************
 *
 * PluginRecoveryDialog
 *
 ***************************************************************/

class PluginRecoveryDialog : public Dialog
{
public:
	PluginRecoveryDialog() : m_path_model(2) {}
	~PluginRecoveryDialog();

	void 						OnSetFocus();

	static BOOL Create(DesktopWindow* parent_window);
	static void TestPluginsLoaded(DesktopWindow* parent_window);
 
	virtual void				OnInit();
	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_PLUGIN_RECOVERY;}
	virtual const char*			GetWindowName()			{return "Plugin Recovery Dialog";}

	INT32						GetButtonCount() { return 2; };

private:
	void SaveSetting();

private:
	static PluginRecoveryDialog* m_dialog;
	static BOOL m_ok_pressed;
	SimpleTreeModel m_path_model;
};


/***************************************************************
 *
 * PluginCheckDialog
 *
 ***************************************************************/

class PluginCheckDialog : public Dialog
{
public:
	PluginCheckDialog() : m_plugin_model(4) {}
	~PluginCheckDialog();

	void 						OnSetFocus();

	static void Create(DesktopWindow* parent_window);
 
	static INT32                Populate( OpTreeView* treeview, SimpleTreeModel* model );
	virtual void				OnInit();
	BOOL						OnInputAction(OpInputAction* action);

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL 				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_PLUGIN_CHECK;}
	virtual const char*			GetWindowName()			{return "Plugin Check Dialog";}

	INT32						GetButtonCount() { return 2; };

private:
	void SaveSetting();

private:
	static PluginCheckDialog* m_dialog;
	SimpleTreeModel m_plugin_model;
};

#endif

