/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/kde4/Kde4FileChooser.h"

#include "platforms/quix/toolkits/kde4/Kde4Utils.h"

#include <KDE/KFileDialog>
#include <KDE/KApplication>
#include <QEvent>
#include <KDE/KWindowSystem>
#include <QFileInfo>
#include <kdeversion.h>

Kde4FileChooser::Kde4FileChooser(KApplication* application)
	: m_dialog(0)
	, m_application(application)
	, m_can_destroy(true)
	, m_request_destroy(false)
	, m_ask_before_replace(false)
{
}

Kde4FileChooser::~Kde4FileChooser()
{
	Reset();
}

void Kde4FileChooser::Reset()
{
	delete m_dialog;
	m_dialog = 0;
	qDeleteAll(m_filters.begin(), m_filters.end());
	m_filters.clear();
}

void Kde4FileChooser::InitDialog()
{
	m_dialog = new KFileDialog(KUrl(), 0, 0);
}

void Kde4FileChooser::SetDialogType(DialogType type)
{
	KFile::Modes mode = 0;
	KFileDialog::OperationMode opmode = KFileDialog::Other;

	switch (type)
	{
		case FILE_OPEN_MULTI:
			mode |= KFile::Files | KFile::ExistingOnly | KFile::LocalOnly;
			opmode = KFileDialog::Opening;
			break;
		case FILE_OPEN:
			mode |= KFile::File | KFile::ExistingOnly | KFile::LocalOnly;
			opmode = KFileDialog::Opening;
			break;
		case FILE_SAVE_PROMPT_OVERWRITE:  /* fallthrough */
			m_ask_before_replace = true;
		case FILE_SAVE:
			mode |= KFile::File | KFile::LocalOnly;
			opmode = KFileDialog::Saving;
			break;
		case DIRECTORY:
			mode |= KFile::Directory | KFile::ExistingOnly | KFile::LocalOnly;
			opmode = KFileDialog::Opening;
			break;
	}

	if (type == DIRECTORY)
		Kde4Utils::SetResourceName(m_dialog, "directorychooserdialog");
	else
		Kde4Utils::SetResourceName(m_dialog, "filechooserdialog");

	m_dialog->setOperationMode(opmode);
	m_dialog->setMode(mode);
}

void Kde4FileChooser::SetCaption(const char* caption)
{
	m_dialog->setCaption(QString::fromUtf8(caption));
}

void Kde4FileChooser::SetInitialPath(const char* path)
{
	m_initial_path = QByteArray(path);

	QFileInfo info(QString::fromUtf8(path));

	if (info.isDir())
		m_dialog->setUrl(KUrl(info.filePath()));
	else
		m_dialog->setSelection(info.filePath());
}

void Kde4FileChooser::AddFilter(int id, const char* media_type)
{
	Filter* filter = new Filter;
	if (!filter)
		return;

	filter->media_type = QString::fromUtf8(media_type);
	filter->media_type.replace("/", "\\/"); 
	m_filters.insert(id, filter);
}

void Kde4FileChooser::AddExtension(int id, const char* extension)
{
	m_filters.at(id)->extensions.push_back(QString::fromAscii(extension));
}

void Kde4FileChooser::SetDefaultFilter(int id)
{
	// Not applicable in this file chooser
}

void Kde4FileChooser::SetFixedExtensions(bool fixed)
{
	// Not applicable in this file chooser
}

void Kde4FileChooser::ShowHiddenFiles(bool show_hidden)
{
	// Not applicable in this file chooser
}

void Kde4FileChooser::OpenDialog(X11Types::Window parent, ToolkitFileChooserListener* result_listener)
{
	SetFilters();

	while(1)
	{
		int result = Kde4Utils::RunDialog(m_dialog, parent);
		if (result >= 0)
		{
			// User pressed OK or cancel

			if (m_ask_before_replace && GetFileCount() > 0 && !result_listener->OnSaveAsConfirm(this))
			{
				QByteArray initial_path(m_initial_path);
				SetInitialPath(initial_path.data());
				continue;
			}

			m_can_destroy = false;
			result_listener->OnChoosingDone(this);
			m_can_destroy = true;
			if (m_request_destroy)
				delete this; // Calls Reset() in destructor
			else
				Reset();
			break;
		}
		else
		{
			// Dialog was killed (via Destroy())
			delete this;
			break;
		}
	}
}

void Kde4FileChooser::SetFilters()
{
	QString filter_string;

	QListIterator<Filter*> it(m_filters);
	while(it.hasNext())
	{
		Filter* filter = it.next();

		if (!filter_string.isEmpty())
			filter_string.append("\n");
		filter_string.append(filter->extensions.join(QString(" ")));
		filter_string.append("|");
		filter_string.append(filter->media_type);

		filter->filter_and_media = filter->extensions.join(QString(" ")) + "|" + filter->media_type;

	}

	m_dialog->setFilter(filter_string);
}

void Kde4FileChooser::Cancel()
{
	m_dialog->reject();
}

void Kde4FileChooser::Destroy()
{
	if (!m_can_destroy)
	{
		m_request_destroy = true;
		return;
	}

	if (m_dialog)
		m_dialog->done(-1);
	else
		delete this;
}

int Kde4FileChooser::GetFileCount()
{
	if (m_dialog->result() != QDialog::Accepted)
		return 0;

	return m_dialog->selectedFiles().size();
}

const char* Kde4FileChooser::GetFileName(int index)
{
	m_temp_string = m_dialog->selectedFiles().at(index).toUtf8();

	return m_temp_string.data();
}

const char* Kde4FileChooser::GetActiveDirectory()
{
	m_temp_string = m_dialog->baseUrl().path().toUtf8();

	return m_temp_string.data();
}

int Kde4FileChooser::GetSelectedFilter()
{
	QString selected_filter = m_dialog->currentMimeFilter();
	for(int i = 0; i < m_filters.size(); i++)
	{
		if (m_filters.at(i)->filter_and_media == selected_filter)
			return i;
	}
	return 0;
}
