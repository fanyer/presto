
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSPLUGINDETECTOR_H
#define WINDOWSPLUGINDETECTOR_H

#if defined(_PLUGIN_SUPPORT_) && !defined(NS4P_COMPONENT_PLUGINS)

class OpPluginDetectionListener;

class WindowsPluginDetector
{
public:
	WindowsPluginDetector(OpPluginDetectionListener *listener)
		: m_listener(listener)
		, m_acrobat_installed(FALSE)
		, m_svg_installed(FALSE)
		, m_qt_installed(FALSE)
		, m_wmp_installed(FALSE)
		, m_wmp_detection_done(FALSE)
		, m_read_from_registry(FALSE)
	{}
	OP_STATUS ReadPlugins(const OpStringC& suggested_plugin_paths);

	// check if the plugin is for this architecture (win32 or x64)
	BOOL	IsPluginForArchitecture(const OpStringC& plugin_path);

private:

	enum PluginType { ACROBAT, SVG, QT, WMP };

	struct PluginInfo
	{
		OpString product_name;
		OpString mime_type;
		OpString extensions;
		OpString file_open_name;
		OpString description;
		OpString plugin;
		OpString version;
	};


	OP_STATUS CheckReplacePlugin(PluginType type, BOOL& ignore, const OpStringC& checkedFileDescription = NULL);
	OP_STATUS InsertPluginPath(const OpStringC& plugin_path, BOOL* was_different = NULL, const OpStringC& checkedFileDescription = NULL, BOOL is_replacement = FALSE);
	OP_STATUS ReadFromRegistry();
	OP_STATUS ReadEntries(const HKEY hk);
	OP_STATUS InsertPlugin(const OpStringC& productName, const OpStringC& mimeType, const OpStringC& fileExtensions, const OpStringC& fileOpenName, const OpStringC& fileDescription, const OpStringC& plugin, const OpStringC& version);
	OP_STATUS SubmitPlugins();
	BOOL IsVersionNewer (const OpStringC& existing, const OpStringC& added);
	OP_STATUS RemovePlugin(const OpStringC& productName);

	OpPluginDetectionListener *m_listener;
	BOOL m_acrobat_installed;
	BOOL m_svg_installed;
	BOOL m_qt_installed;
	BOOL m_wmp_installed;	// set only when "new" (np-mswmp.dll) plugin is available
	BOOL m_wmp_detection_done;	// tracks if we looked for np-mswmp.dll already
	BOOL m_read_from_registry;
	OpString m_latest_seen_java_name;
	OpString m_latest_seen_java_version;

	OpAutoStringHashTable<PluginInfo> m_plugin_list;
};

#endif // _PLUGIN_SUPPORT_ && !NS4P_COMPONENT_PLUGINS

#endif // !WINDOWSPLUGINDETECTOR_H
