/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#if !defined WIDGETS_CAP_WIC_FILESELECTION || !defined WIC_CAP_FILESELECTION

#include "x11_opfilechooser.h"


OP_STATUS X11OpFileChooser::ShowOpenSelector(SELECTOR_PARENT *parent,
											 const OpString &caption,
											 const OpFileChooserSettings &settings)
{
#ifdef DEBUG
	OpString8 initial;
	RETURN_IF_ERROR(initial.Set(settings.m_initial_file.CStr()));
	char *name = initial.CStr() ? strrchr(initial.CStr(), '/') : 0;
	if (name)
		name ++;
	else
		name = initial.CStr();
	printf("X11OpFileChooser::ShowOpenSelector:\nEnter filename (Press ENTER for \"/tmp/%s\"): ", name);
	char str[500];
	if (!fgets(str, 499, stdin))
		return OpStatus::ERR;
	OpString filename;
	int lastchar = strlen(str) - 1;
	if (str[lastchar] == '\n')
		str[lastchar] = 0;
	if (!*str)
	{
		RETURN_IF_ERROR(filename.Set("/tmp/"));
		RETURN_IF_ERROR(filename.Append(name));
	}
	else
	{
		RETURN_IF_ERROR(filename.Set(str));
	}
	if (settings.m_listener)
		settings.m_listener->OnFileSelected(filename.CStr());
#endif
	return OpStatus::OK;
}

OP_STATUS X11OpFileChooser::ShowSaveSelector(SELECTOR_PARENT *parent,
											 const OpString &caption,
											 const OpFileChooserSettings &settings)
{
#ifdef DEBUG
	OpString8 initial;
	RETURN_IF_ERROR(initial.Set(settings.m_initial_file.CStr()));
	char *name = initial.CStr() ? strrchr(initial.CStr(), '/') : 0;
	if (name)
		name ++;
	else
		name = initial.CStr();
	printf("X11OpFileChooser::ShowOpenSelector:\nEnter filename (Press ENTER for \"/tmp/%s\"): ", name);
	char str[500];
	if (!fgets(str, 499, stdin))
		return OpStatus::ERR;
	OpString filename;
	int lastchar = strlen(str) - 1;
	if (str[lastchar] == '\n')
		str[lastchar] = 0;
	if (!*str)
	{
		RETURN_IF_ERROR(filename.Set("/tmp/"));
		RETURN_IF_ERROR(filename.Append(name));
	}
	else
	{
		RETURN_IF_ERROR(filename.Set(str));
	}
	if (settings.m_listener)
		settings.m_listener->OnFileSelected(filename.CStr());
#endif
	return OpStatus::OK;
}

OP_STATUS X11OpFileChooser::ShowFolderSelector(SELECTOR_PARENT *parent,
											   const OpString &caption,
											   const OpFileChooserSettings &settings)
{
#ifdef DEBUG
	printf("X11OpFileChooser::ShowFolderSelector:\nEnter directory: ");
	char str[500];
	if (!fgets(str, 499, stdin))
		return OpStatus::ERR;
	int lastchar = strlen(str) - 1;
	if (str[lastchar] == '\n')
		str[lastchar] = 0;
	if (*str)
	{
		OpString filename;
		RETURN_IF_ERROR(filename.Set(str));
		if (settings.m_listener)
			settings.m_listener->OnFileSelected(filename.CStr());
	}
#endif
	return OpStatus::OK;
}

#endif // !WIDGETS_CAP_WIC_FILESELECTION || !WIC_CAP_FILESELECTION
