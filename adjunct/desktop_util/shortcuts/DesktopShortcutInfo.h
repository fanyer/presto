/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Remigiusz Bondarowicz (raimon@opera.com)
 */

#ifndef _DESKTOP_SHORTCUT_INFO_H_
#define _DESKTOP_SHORTCUT_INFO_H_

/**
 * Gathers desktop shortcut information.  Different platforms may make use of
 * different fields.
 *
 * "destination" is where the shortcut file is placed on the platform.
 * "target" is the application the shortcut points to.
 */
struct DesktopShortcutInfo
{
	/**
	 * Possible shortcut destinations.  Platform support varies.
	 */
	enum Destination
	{
		/** There is no support for shortcuts on this platform. */
		SC_DEST_NONE = 0,

		/** Desktop, available on Windows. */
		SC_DEST_DESKTOP,

		/** Common desktop, available on Windows. */
		SC_DEST_COMMON_DESKTOP,

		/** Quick launch bar, available on Windows. */
		SC_DEST_QUICK_LAUNCH,

		/** Applications menu, available on Windows, UNIX. */
		SC_DEST_MENU,

		/** Common applications menu, available on Windows. */
		SC_DEST_COMMON_MENU,

		/** Temp, for creating temporary shortcut. */
		SC_DEST_TEMP,

		/** The destination type count */
		SC_DESTINATION_TYPE_COUNT
		
		/* NB: There should be no values below SC_DESTINATION_TYPE_COUNT */

	};

	/** The shortut placement on the platform. */
	Destination m_destination;

	/** The type of the shortcut target (e.g., "Application"). */
	OpString m_type;

	/** The specific name of the shortcut target. */
	OpString m_name;

	/** A generic name of the shortcut target (e.g. "Web Browser"). */
	OpString m_generic_name;

	/** The name of the shortcut file (not the target name). */
	OpString m_file_name;

	/** The absolute path to or the name of the icon associated with the
	    shortcut. */
	OpString m_icon_name;

	/** The icon index in case it's applicable with the value of
	    @p m_icon_file_path. */
	UINT32 m_icon_index;

	/** The path to the working directory of the shortcut target. */
	OpString m_working_dir_path;

	/** A comment for the shortcut. */
	OpString m_comment;

	/** The command that launches the shortcut target. */
	OpString m_command;
};

#endif //_DESKTOP_SHORTCUT_INFO_H_
