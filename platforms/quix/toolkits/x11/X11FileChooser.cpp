/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#include "core/pch.h"

#include "platforms/quix/toolkits/x11/X11FileChooser.h"
#include "platforms/quix/toolkits/x11/X11FileDialog.h"
#include "platforms/unix/base/x11/x11utils.h"



X11ToolkitFileChooser::X11ToolkitFileChooser()
	: m_can_destroy(true)
	, m_request_destroy(false)
{
}


X11ToolkitFileChooser::~X11ToolkitFileChooser()
{
}


void X11ToolkitFileChooser::InitDialog()
{
}


void X11ToolkitFileChooser::SetDialogType(DialogType type)
{
	m_settings.type = type;
}


void X11ToolkitFileChooser::SetCaption(const char* caption)
{
	m_settings.caption.SetFromUTF8(caption);
}


void X11ToolkitFileChooser::SetInitialPath(const char* path)
{
	m_settings.path.SetFromUTF8(path);
	// Do not strip path here. If caller wants us to use spaces we should
}


void X11ToolkitFileChooser::AddFilter(int id, const char* media_type)
{
	X11FileChooserSettings::FilterData* f=0;
	if (!m_settings.filter.Contains(id))
	{
		f = OP_NEW(X11FileChooserSettings::FilterData,());
		if (!f)
			return;
		if( OpStatus::IsError(m_settings.filter.Add(id, f)))
		{
			OP_DELETE(f);
			return;
		}
	}
	else
	{
		RETURN_VOID_IF_ERROR(m_settings.filter.GetData(id, &f));
	}

	f->id = id;
	f->media.SetFromUTF8(media_type);

	// Remove filter information from media type
	int pos = f->media.Find(UNI_L("("));
	if (pos != KNotFound)
	{
		f->media.Delete(pos);
		f->media.Strip();
	}
}

void X11ToolkitFileChooser::AddExtension(int id, const char* extension)
{
	X11FileChooserSettings::FilterData* f=0;
	if (!m_settings.filter.Contains(id))
	{
		f = OP_NEW(X11FileChooserSettings::FilterData,());
		if (!f || OpStatus::IsError(m_settings.filter.Add(id, f)))
		{
			OP_DELETE(f);
			return;
		}
	}
	else
	{
		RETURN_VOID_IF_ERROR(m_settings.filter.GetData(id, &f));
	}

	f->id = id;

	OpString ext;
	RETURN_VOID_IF_ERROR(ext.SetFromUTF8(extension));
	if (f->extension.HasContent())
		RETURN_VOID_IF_ERROR(f->extension.Append(" "));
	f->extension.Append(ext);
}


void X11ToolkitFileChooser::SetDefaultFilter(int id)
{
	m_settings.activefilter = id;
}

	
void X11ToolkitFileChooser::SetFixedExtensions(bool fixed)
{
	m_settings.fixed = fixed;
}
	

void X11ToolkitFileChooser::ShowHiddenFiles(bool show_hidden)
{
	m_settings.show_hidden = show_hidden;
}


void X11ToolkitFileChooser::OpenDialog(X11Types::Window parent, ToolkitFileChooserListener* result_listener)
{
	if (!m_settings.dialog)
	{
		m_settings.dialog = OP_NEW(X11FileDialog,(m_settings));
		if (!m_settings.dialog || OpStatus::IsError(m_settings.dialog->Init(X11Utils::GetDesktopWindowFromX11Window(parent), 0, TRUE)))
		{
			OP_DELETE(m_settings.dialog);
			m_settings.dialog = 0;
		}
		else
		{
			// Dialog will self destruct in this case
			if (result_listener)
			{
				m_can_destroy = false;
				result_listener->OnChoosingDone(this);
				m_can_destroy = true;
				if (m_request_destroy)
				{
					delete this;
					return;
				}
			}
			m_settings.Reset();
		}
	}
}


void X11ToolkitFileChooser::Cancel()
{
	if (m_settings.dialog)
		m_settings.dialog->CloseDialog(TRUE, TRUE, TRUE);
}


void X11ToolkitFileChooser::Destroy()
{
	if (!m_can_destroy)
	{
		m_request_destroy = true;
		return;
	}

	delete this;
}


int X11ToolkitFileChooser::GetFileCount()
{
	return m_settings.selected_files.GetCount();
}

	
const char* X11ToolkitFileChooser::GetFileName(int index)
{
	return m_settings.selected_files.Get(index)->CStr();
}


const char* X11ToolkitFileChooser::GetActiveDirectory()
{
	return m_settings.selected_path.CStr();
}


int X11ToolkitFileChooser::GetSelectedFilter()
{
	return m_settings.activefilter;
}

