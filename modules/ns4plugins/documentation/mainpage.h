/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/


/** @mainpage ns4plugins module
 *
 * This is the auto-generated API documentation for the ns4plugins module.
 * For more information about the module, see the module's <a
 * href="http://wiki.palace.opera.no/developerwiki/index.php/Modules/ns4plugins">Wiki
 * page</a>.
 *
 * @section Overview
 * 
 * The ns4plugins module implements support for loading Netscape plugins in the
 * browser and scripting of plugin objects from Javascript.
 * It also supports scripting of Netscape4
 * LiveConnect enabled plug-ins on Windows.
 * Platform specific methods still needs to be implemented, so
 * it's not complete.
 *
 * @section api Public API
 *
 * The ns4plugins module API consists of the interfaces and classes described below.
 *
 * @subsection OpNS4PluginHandler
 *
 * A global PluginHandler object for enabling and disabling of plugins and
 * handling of plugin messages sent through the
 * main message handler. The PluginHandler is initialised during
 * startup of the browser. 
 *
 * @subsection OpNS4Plugin
 *
 * A global Plugin object for handling streaming of plugin data, plugin events,
 * plugin specific layout information and scripting of plugins. 
 * The Plugin is initialised when the browser is displaying a site 
 * containing an embedded plugin.
 *
 */

