/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GTK_FILE_CHOOSER_H
#define GTK_FILE_CHOOSER_H

#include "platforms/quix/toolkits/ToolkitFileChooser.h"

#include <gtk/gtk.h>

class GtkToolkitFileChooser : public ToolkitFileChooser
{
public:
	GtkToolkitFileChooser();
	virtual ~GtkToolkitFileChooser();

	// From ToolkitFileChooser
	virtual void InitDialog();

	virtual void SetDialogType(DialogType type);

	virtual void SetCaption(const char* caption);

	virtual void SetInitialPath(const char* path);

	virtual void AddFilter(int id, const char* media_type);

	virtual void AddExtension(int id, const char* extension);

	virtual void SetDefaultFilter(int id);

	virtual void FilterChanged();

	virtual void SetFixedExtensions(bool fixed) {}
	
	virtual void ShowHiddenFiles(bool show_hidden);

	virtual void OpenDialog(X11Types::Window parent, ToolkitFileChooserListener* result_listener);

	virtual void Cancel();

	virtual void Destroy();

	virtual int GetFileCount();

	virtual const char* GetFileName(int index);

	virtual const char* GetActiveDirectory();

	virtual int GetSelectedFilter();

	// Implementation specific functions
	bool VerifySaveFiles();

private:
	enum CustomResponse
	{
		RESPONSE_KILL_DIALOG = 1
	};

	void ResetData();
	GtkFileFilter* GetFilterById(int id);

	GtkWidget* m_dialog;
	bool m_open_dialog;
	bool m_can_destroy;
	bool m_request_destroy;
	GSList* m_selected_filenames;
	gchar* m_active_directory;
	GSList* m_extensions;
	GtkFileChooserAction m_action;
};

#endif // GTK_FILE_CHOOSER_H

