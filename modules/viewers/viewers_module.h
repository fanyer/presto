/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef VIEWERS_MODULE_H
#define VIEWERS_MODULE_H

#define VIEWERS_MODULE_REQUIRED

class Viewers;
class PluginViewers;

class ViewersModule : public OperaModule
{
 public:
  ViewersModule(void);
  ~ViewersModule(void);

  virtual void InitL(const OperaInitInfo& info);
  virtual void Destroy(void);

  Viewers* viewer_list;

#ifdef _PLUGIN_SUPPORT_
  PluginViewers* plugin_viewer_list;
#endif
};

#define g_plugin_viewers g_opera->viewers_module.plugin_viewer_list
#define g_viewers g_opera->viewers_module.viewer_list
#define viewers g_opera->viewers_module.viewer_list

#endif // VIEWERS_MODULE_H
