/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PLUGINEXCEPTIONS_H
#define PLUGINEXCEPTIONS_H

#ifdef _PLUGIN_SUPPORT_

#ifdef NS4P_TRY_CATCH_PLUGIN_CALL
# if defined(PLUGIN_TRY) && defined(PLUGIN_CATCH_CONDITIONAL)
#  define START_PLUGIN_CALL BOOL thought_we_were_executing_plugin_code_already = g_opera->ns4plugins_module.is_executing_plugin_code; g_opera->ns4plugins_module.is_executing_plugin_code = TRUE;
#  define END_PLUGIN_CALL OP_ASSERT(g_opera->ns4plugins_module.is_executing_plugin_code); g_opera->ns4plugins_module.is_executing_plugin_code = thought_we_were_executing_plugin_code_already;
#  define TRY START_PLUGIN_CALL PLUGIN_TRY {
#  define CATCH_PLUGIN_EXCEPTION(x) } PLUGIN_CATCH_CONDITIONAL(g_opera->ns4plugins_module.is_executing_plugin_code) { x } END_PLUGIN_CALL
#  define CATCH_IGNORE } PLUGIN_CATCH_CONDITIONAL(g_opera->ns4plugins_module.is_executing_plugin_code) { } END_PLUGIN_CALL
# elif defined(PLUGIN_TRY) && defined(PLUGIN_CATCH)
#  error "Need updated macros"
#  define TRY PLUGIN_TRY {
#  define CATCH_PLUGIN_EXCEPTION(x) } PLUGIN_CATCH { x }
#  define CATCH_IGNORE } PLUGIN_CATCH { }
# else
#  error "NS4P_TRY_CATCH_PLUGIN_CALL requires PLUGIN_TRY and PLUGIN_CATCH_CONDITIONAL defined by platform"
# endif // PLUGIN_TRY && PLUGIN_CATCH
#else
# define TRY
# define CATCH_PLUGIN_EXCEPTION(x)
# define CATCH_IGNORE
#endif // NS4P_TRY_CATCH_PLUGIN_CALL

#endif // _PLUGIN_SUPPORT_
#endif // !PLUGINEXCEPTIONS_H
