/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * author: Patryk Obara
 */
#ifndef DESKTOP_ENTRY_H
#define DESKTOP_ENTRY_H

#ifdef WIDGET_RUNTIME_SUPPORT

struct DesktopShortcutInfo;


class DesktopEntry
{
public:
	DesktopEntry();

	OP_STATUS Init(const DesktopShortcutInfo& shortcut_info);

	/** 
	 * Creates new desktop entry file. Follows freedesktop.org
	 * Desktop Entry Specification:
	 * http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-1.0.html 
	 *
	 * @return OK on success, one of ERRORS otherwise
	 */
	OP_STATUS Deploy();

	OP_STATUS Remove();

private:
	/** 
	 * Sets (absolute) desktop entry file location to path.
	 * Location is constructed using info object according to
	 * freedesktop.org specification and examples:
	 * XDG Base Directory Specification v0.6
	 * http://standards.freedesktop.org/basedir-spec/basedir-spec-0.6.html
	 * Desktop Menu Specification v1.0
	 * http://standards.freedesktop.org/menu-spec/menu-spec-1.0.html 
	 *
	 */
	OP_STATUS PreparePath();

	const DesktopShortcutInfo* m_shortcut_info;
	OpString m_path;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // DESKTOP_ENTRY_H
