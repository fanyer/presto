#include "core/pch.h"

#include "modules/ns4plugins/src/staticplugin.h"
#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/plugin.h"

#if defined(NS4P_SUPPORT_PROXY_PLUGIN) || defined(NS4P_COMPONENT_PLUGINS)

OpPluginWindow * ProxyPlugin::GetPluginWindow(NPP instance)
{
	// Get the plugin :
	Plugin * plugin  = g_pluginhandler->FindPlugin(instance);

	// Get the plugin window :
	return plugin ? plugin->GetPluginWindow() : 0;
}

#endif // defined(NS4P_SUPPORT_PROXY_PLUGIN) || defined(NS4P_COMPONENT_PLUGINS)
