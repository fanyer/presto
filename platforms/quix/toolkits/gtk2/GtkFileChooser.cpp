/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/gtk2/GtkFileChooser.h"

#include "platforms/quix/toolkits/gtk2/GtkPorting.h"
#include "platforms/quix/toolkits/gtk2/GtkUtils.h"
#include <gdk/gdkx.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>


static void filter_changed (GObject *object, GParamSpec *spec, gpointer data)
{
	GtkToolkitFileChooser* chooser = NULL;
	chooser = (GtkToolkitFileChooser*)data;
	if (chooser)
		chooser->FilterChanged();
}


GtkToolkitFileChooser::GtkToolkitFileChooser()
	: m_dialog(0)
	, m_open_dialog(false)
	, m_can_destroy(true)
	, m_request_destroy(false)
	, m_selected_filenames(0)
	, m_active_directory(0)
	, m_extensions(0)
{
}

GtkToolkitFileChooser::~GtkToolkitFileChooser()
{
	ResetData();
}

void GtkToolkitFileChooser::ResetData()
{
	if (m_dialog)
	{
		gtk_widget_destroy(m_dialog);
		m_dialog = 0;
	}

	for (GSList* filename = m_selected_filenames; filename; filename = filename->next)
		g_free(filename->data);

	g_slist_free(m_selected_filenames);
	m_selected_filenames = 0;

	g_free(m_active_directory);
	m_active_directory = 0;

 	for (GSList* extension = m_extensions; extension; extension = extension->next)		
		g_string_free((GString *)extension->data, TRUE); 

	g_slist_free(m_extensions);
	m_extensions = 0; 
}

void GtkToolkitFileChooser::InitDialog()
{
	ResetData();

	m_dialog = gtk_file_chooser_dialog_new(0, 0,
										   GTK_FILE_CHOOSER_ACTION_OPEN,
										   GTK_STOCK_CANCEL,
										   GTK_RESPONSE_CANCEL,
										   NULL);

	g_signal_connect(G_OBJECT(m_dialog), "notify::filter", G_CALLBACK(filter_changed), (gpointer)this);
}

void GtkToolkitFileChooser::SetDialogType(DialogType type)
{
	m_action = GTK_FILE_CHOOSER_ACTION_OPEN;
	const gchar* accept_text = 0;

	switch (type)
	{
		case FILE_OPEN_MULTI:
			gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(m_dialog), TRUE);
			/* Fallthrough */
		case FILE_OPEN:
			m_action = GTK_FILE_CHOOSER_ACTION_OPEN;
			accept_text = GTK_STOCK_OPEN;
			break;
		case FILE_SAVE_PROMPT_OVERWRITE:
			gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(m_dialog), TRUE);
			/* Fallthrough */
		case FILE_SAVE:
			m_action = GTK_FILE_CHOOSER_ACTION_SAVE;
			accept_text = GTK_STOCK_SAVE;
			break;
		case DIRECTORY:
			m_action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
			accept_text = GTK_STOCK_OPEN;
			break;
	}

	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(m_dialog), m_action);
	gtk_dialog_add_button(GTK_DIALOG(m_dialog), accept_text, GTK_RESPONSE_ACCEPT);
}

void GtkToolkitFileChooser::SetCaption(const char* caption)
{
	gtk_window_set_title(GTK_WINDOW(m_dialog), caption);
}

void GtkToolkitFileChooser::SetInitialPath(const char* path)
{
	char* converted_path = g_filename_from_utf8(path, -1, 0, 0, 0);
	if (!converted_path)
		return;

	struct stat statinfo;
	bool exists = lstat(converted_path, &statinfo) == 0;

	if (exists)
	{
		if (S_ISDIR(statinfo.st_mode))
		{
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(m_dialog), converted_path);
		}		
		else
		{
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(m_dialog), converted_path);
 			char* name = basename(converted_path);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(m_dialog), name);
		}
	}
	else
	{
		char* name = basename(converted_path);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(m_dialog), name);

		// dirname has to be executed last; on some systems (GNU libc) it modifies its argument
		char* folder = dirname(converted_path);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(m_dialog), folder);
	}

	g_free(converted_path);
}

void GtkToolkitFileChooser::AddFilter(int id, const char* media_type)
{
	GtkFileFilter* filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, media_type);

	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(m_dialog), filter);
}

void GtkToolkitFileChooser::AddExtension(int id, const char* extension)
{
	GtkFileFilter* filter = GetFilterById(id);
	if (!filter)
		return;

 	GString *tmp = static_cast<GString*>(g_slist_nth_data( m_extensions, id ));
	if (!tmp && m_action == GTK_FILE_CHOOSER_ACTION_SAVE)
	{
		GString *extension_g =  g_string_new( (gchar *)extension);
		m_extensions = g_slist_append(m_extensions, extension_g);
	}

	gtk_file_filter_add_pattern(filter, extension);
}

GtkFileFilter* GtkToolkitFileChooser::GetFilterById(int id)
{
	GSList* list = gtk_file_chooser_list_filters(GTK_FILE_CHOOSER(m_dialog));
	GtkFileFilter* filter = GTK_FILE_FILTER(g_slist_nth_data(list, id));
	g_slist_free(list);
	return filter;
}

void GtkToolkitFileChooser::SetDefaultFilter(int id)
{
	GtkFileFilter* filter = GetFilterById(id);
	if (!filter)
		return;

	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(m_dialog), filter);
}

void GtkToolkitFileChooser::FilterChanged()
{
	GtkFileFilter* current_filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(m_dialog));

	if (m_action == GTK_FILE_CHOOSER_ACTION_SAVE && current_filter)
	{
		int filter_index = GetSelectedFilter();
		gchar * current_full_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(m_dialog));

		if (current_full_filename)
	    {
			char* current_filename = basename((char *)current_full_filename);

			GString *file_extension; // or use gchar
			file_extension = static_cast<GString*>(g_slist_nth_data(m_extensions, filter_index));
		
			if (file_extension)
			{
				char *extension = (char *)file_extension->str;
  				int extpos = 0;
  				for (size_t i=0; i< strlen(extension); i++)
  					if (extension[i] == '.')
  						extpos = i;

				if (extpos > 0 &&  strlen(extension) > 2 && strstr(extension, "*.") && !strstr(extension, "*.*")) // Check this one
			    {
					GString *current_filename_g = g_string_new(current_filename);
					if (current_filename_g)
					{
						char *ptr = (char *)current_filename;
						int len = strlen(ptr); 
						int pos = len;
						for (int i=0; i<len; i++)					   
							if (ptr[i] == '.')
								pos = i;
						
						g_string_erase(current_filename_g, (gssize)pos, (gssize)(len-pos));
						g_string_append(current_filename_g, (gchar*)extension+extpos);						

						gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(m_dialog), current_filename_g->str);
							
						gchar * current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(m_dialog));
						gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(m_dialog), current_folder);

						g_free(current_folder);
						g_string_free(current_filename_g, TRUE);
					}
				}
			}
			g_free(current_full_filename); // Check this one
		}
	}
}

void GtkToolkitFileChooser::ShowHiddenFiles(bool show_hidden)
{
	gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(m_dialog), show_hidden);
}

void GtkToolkitFileChooser::OpenDialog(X11Types::Window parent, ToolkitFileChooserListener* result_listener)
{
	gtk_widget_show(m_dialog);

	XSetTransientForHint(GDK_WINDOW_XDISPLAY(gtk_widget_get_window(m_dialog)),
						 GDK_WINDOW_XID(gtk_widget_get_window(m_dialog)), parent);

	if (m_action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
		GtkUtils::SetResourceName(m_dialog, "directorychooserdialog");
	else
		GtkUtils::SetResourceName(m_dialog, "filechooserdialog");

	int response_id;
	while(1)
	{
		m_open_dialog = true;
		response_id = gtk_dialog_run(GTK_DIALOG(m_dialog));
		m_open_dialog = false;

		if (response_id == GTK_RESPONSE_ACCEPT)
		{
			m_selected_filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(m_dialog));
			m_active_directory = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(m_dialog));

			if (m_action == GTK_FILE_CHOOSER_ACTION_SAVE)
			{
				// BIG FAT TEST for saving. The file selector (or at least some versions of it)
				// allows users entering an existing directory and accepts this as a valid filename
				// Do not allow that as we have code that can call OpFile::Delete(TRUE) before saving
				// the file. See DSK-318398
				if (!VerifySaveFiles())
				{
					GtkWidget* dialog = m_dialog;
					m_dialog = 0;
					ResetData();
					m_dialog = dialog;
					continue;
				}
			}
		}
		break;
	}

	/* Note that gtk sends GTK_RESPONSE_DELETE_EVENT if the dialog is
	 * closed, including if the user presses the Escape button.
	 */
	if (response_id == GTK_RESPONSE_ACCEPT || response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT)
	{
		m_can_destroy = false;
		result_listener->OnChoosingDone(this);
		m_can_destroy = true;
		if (m_request_destroy)
		{
			delete this; // Calls ResetData() in destructor
			return;
		}
	}

	ResetData();
	GtkUtils::ProcessEvents();

	if (response_id == RESPONSE_KILL_DIALOG)
		delete this;
}

void GtkToolkitFileChooser::Cancel()
{
	if (m_open_dialog)
		gtk_dialog_response(GTK_DIALOG(m_dialog), GTK_RESPONSE_NONE);
}

void GtkToolkitFileChooser::Destroy()
{
	if (!m_can_destroy)
	{
		m_request_destroy = true;
		return;
	}

	if (m_open_dialog)
		gtk_dialog_response(GTK_DIALOG(m_dialog), RESPONSE_KILL_DIALOG);
	else
		delete this;
}

int GtkToolkitFileChooser::GetFileCount()
{
	return m_selected_filenames ? g_slist_length(m_selected_filenames) : 0;
}

const char* GtkToolkitFileChooser::GetFileName(int index)
{
	return (gchar*)g_slist_nth(m_selected_filenames, index)->data;
}

const char* GtkToolkitFileChooser::GetActiveDirectory()
{
	return m_active_directory;
}

int GtkToolkitFileChooser::GetSelectedFilter()
{
	GSList* list = gtk_file_chooser_list_filters(GTK_FILE_CHOOSER(m_dialog));
	GtkFileFilter* selected_filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(m_dialog));

	return g_slist_index(list, selected_filter);
}


// The purpose of this function is test is any of the files in
// the list point to a directory. If so, this function must
// return false. It must also return false on any kind of error
bool GtkToolkitFileChooser::VerifySaveFiles()
{
	for (int i=0; i<GetFileCount(); i++)
	{
		char* filename = g_filename_from_utf8(GetFileName(i), -1, 0, 0, 0);
		if (!filename)
			return false;
		struct stat s;
		int rc = stat(filename, &s);
		if (rc == -1)
		{
			if (errno == ENOENT)
				continue;
			return false;
		}
		if (S_ISDIR(s.st_mode))
			return false;
		g_free(filename);
	}

	return true;
}
