/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE4_FILE_CHOOSER_H
#define KDE4_FILE_CHOOSER_H

#include "platforms/quix/toolkits/ToolkitFileChooser.h"
#include <QStringList>

class KApplication;
class KFileDialog;

class Kde4FileChooser : public ToolkitFileChooser
{
public:
	Kde4FileChooser(KApplication* application);
	~Kde4FileChooser();

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
	struct Filter
	{
		QString media_type;
		QStringList extensions;
		QString filter_and_media;
	};

	void SetFilters();
	void Reset();

	KFileDialog* m_dialog;
	KApplication* m_application;
	QList<Filter*> m_filters;
	QByteArray m_temp_string;
	QByteArray m_initial_path;
	bool m_can_destroy;
	bool m_request_destroy;
	bool m_ask_before_replace;
};

#endif // KDE4_FILE_CHOOSER_H
