/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_INSTALLER_CONTEXT_H
#define GADGET_INSTALLER_CONTEXT_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/util/OpSharedPtr.h"
#include "modules/gadgets/OpGadgetClass.h"


/**
 * The context of a gadget installation.
 *
 * The struct acts as a container for various installation-related data and
 * should not exhibit behavior.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
struct GadgetInstallerContext
{
	GadgetInstallerContext();

	/**
	 * Necessary, because OpString::OpString(const OpString&) is not really a
	 * copy constructor.
	 */
	GadgetInstallerContext(const GadgetInstallerContext& rhs);
	GadgetInstallerContext& operator=(const GadgetInstallerContext& rhs);

	/**
	 * A shortcut to obtain the gadget name from m_gadget_class.
	 *
	 * @return the gadget name as declared by the gadget author
	 */
	OpString GetDeclaredName() const;

	/**
	 * A shortcut to obtain the author name from m_gadget_class.
	 *
	 * @return the gadget author name
	 */
	OpString GetAuthorName() const;

	/**
	 * A shortcut to obtain the description from m_gadget_class.
	 *
	 * @return the gadget description
	 */
	OpString GetDescription() const;

	/**
	 * A shortcut to obtain the path to the icon file.
	 *
	 * @return the path to the icon file relative to the root of the gadget
	 * 		contents, or an empty string if the gadget has no icon
	 */
	OpString GetIconFilePath() const;

	/**
	 * Represents the gadget to be installed.
	 */
	OpSharedPtr<OpGadgetClass> m_gadget_class;

	/**
	 * The gadget display name.  This should be the name that identifies the
	 * gadget installation in the OS from the user's point of view, and is
	 * displayed in shortcuts, etc.  May be different than m_declared_name and
	 * m_normalized_name.
	 */
	OpString m_display_name;

	/**
	 * The normalized gadget display name.  This should be the name that
	 * identifies the gadget installation in the OS internally, and is used to
	 * build path names, registry entries, etc.  May be different than
	 * m_declared_name and m_display_name.
	 */
	OpString m_normalized_name;

	/**
	 * The gadget profile name.  This is the name of the Opera profile directory
	 * (just the directory, not the full path) used by a gadget installation.
	 */
	OpString m_profile_name;

	/**
	 * The path to the installation destination directory.
	 */
	OpString m_dest_dir_path;

	/**
	 * The requested shortcut deployment destinations.  It is a bitfield with
	 * one flag for each of the possible destinations.
	 *
	 * @see DesktopShortcutInfo::Destination
	 */
	INT32 m_shortcuts_bitfield;

	/**
	 * The preferred OS command that starts the gadget once it's installed.
	 */
	OpString m_exec_cmd;

	/**
	 * Do we want to deploy launcher scripts in platform specific places.
	 */
	BOOL m_create_launcher;

	/**
	 * Do we want to install this gadget for all users, globally.
	 */
	BOOL m_install_globally;

private:
	void CopyStrings(const GadgetInstallerContext& rhs);
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_INSTALLER_CONTEXT_H
