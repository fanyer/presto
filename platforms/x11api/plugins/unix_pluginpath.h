/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Espen Sand (espen)
 */

#ifndef _UNIX_PLUGINPATH_H_
#define _UNIX_PLUGINPATH_H_

#if defined(_PLUGIN_SUPPORT_)

#include "modules/util/adt/opvector.h"

class PluginPathElement
{

public:
	/**
	 * Constructor. Initializes the object to an empty item
	 */
	PluginPathElement() : state(0) { }

	/**
	 * Copy constructor
	 */
	PluginPathElement(const PluginPathElement& src)
	{
		if (OpStatus::IsError(path.Set(src.path.CStr())))
			state = 0;
		else
			state = src.state;
	}

	OpString path;
	INT32 state; // 0 = invalid, 1 = activated, 2 = disabled
};


class PluginPathList
{
public:
	/**
	 * Returns pointer to object
	 */
	static PluginPathList* Self();

	/**
	 * Destructor. Removes all allocated resources
	 */
	~PluginPathList() { m_plugin_path_list.DeleteAll(); }

	/**
	 * Reads and prepares the plugin-paths and ini files. This function
	 * should only be used by SystemInfo interface on startup.
	 * 'default_path' and 'new_path' are colon separated collections of paths
	 *
	 * @param default_path  If nothing else is read, these are the only paths we examine
	 * @param new_path      The plugin path that can actually be used.
	 */
	void ReadL(const OpStringC& default_path, OpString& new_path);

#ifdef PREFS_WRITE
	/**
	 * Installs new paths as the valid plug-in paths. The personal ini file
	 * will be written and prefsmanager's colon separated collections of paths
	 * gets updated.
	 *
	 * @param list List og new paths
	 */
	void WriteL(OpVector<PluginPathElement>& list);
#endif //PREFS_WRITE

	/**
	 * Returns the list of currently valid plug-in paths
	 */
	const OpVector<PluginPathElement> &Get() const { return m_plugin_path_list; }

private:
	OpVector<PluginPathElement> m_plugin_path_list;
};

#endif

#endif
