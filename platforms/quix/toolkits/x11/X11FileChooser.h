/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#ifndef _X11_FILE_CHOOSER_H_
#define _X11_FILE_CHOOSER_H_

#include "platforms/quix/toolkits/ToolkitFileChooser.h"

class X11FileChooserSettings
{
public:
	struct FilterData
	{
		INT32 id;
		OpString media;
		OpString extension;
	};

public:
	X11FileChooserSettings() { Reset(); }

	void Reset()
	{
		type = ToolkitFileChooser::FILE_OPEN;
		filter.DeleteAll();
		caption.Empty();
		path.Empty();
		selected_files.DeleteAll();
		selected_path.Empty();
		show_hidden = false;
		fixed = false;
		activefilter = 0;
		dialog = 0;
	}

public:	
	ToolkitFileChooser::DialogType type;
	OpINT32HashTable<FilterData> filter;
	OpString caption;
	OpString path;
	OpAutoVector<OpString8> selected_files;
	OpString8 selected_path;
	bool show_hidden;
	bool fixed;
	int activefilter;
	class Dialog* dialog;
};


class X11ToolkitFileChooser : public ToolkitFileChooser
{
public:
	X11ToolkitFileChooser();
	virtual ~X11ToolkitFileChooser();

	// From ToolkitFileChooser
	virtual void InitDialog();

	virtual void SetDialogType(DialogType type);

	virtual void SetCaption(const char* caption);

	virtual void SetInitialPath(const char* path);

	virtual void AddFilter(int id, const char* media_type);

	virtual void AddExtension(int id, const char* extension);

	virtual void SetDefaultFilter(int id);

	virtual void SetFixedExtensions(bool fixed);
	
	virtual void ShowHiddenFiles(bool show_hidden);

	virtual void OpenDialog(X11Types::Window parent, ToolkitFileChooserListener* result_listener);

	virtual void Cancel();

	virtual void Destroy();

	virtual int GetFileCount();

	virtual const char* GetFileName(int index);

	virtual const char* GetActiveDirectory();

	virtual int GetSelectedFilter();

private:
	X11FileChooserSettings m_settings;
	bool m_can_destroy;
	bool m_request_destroy;
};


#endif
