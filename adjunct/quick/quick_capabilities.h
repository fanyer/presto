/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef QUICK_CAPABILITIES_H
#define QUICK_CAPABILITIES_H

// Only define capabilities here!

#define QUICK_CAP_HAS_EXECUTE_PROGRAM
///< Has the ExecuteProgram method in Application

#define QUICK_CAP_HANDLE_EXTERNAL_URL_PROTOCOL
///< Has the HandleExternalURLProtocol method in Application

#define QUICK_CAP_SHOWALTPLUGCONTENT
///< Has the GotoPluginDownloadDialog class

#define QUICK_CAP_USE_STATIC_RESET_DEF_BUTTON
///< Use the new static version of FormObject::ResetDefaultButton()

#define QUICK_CAP_PACKED_COLUMNDATA
///< GetColumnData get one parameter

#define QUICK_CAP_HAS_COMPOSE_MAIL
///< class Application has method ComposeMail

#define QUICK_CAP_HOTLIST_CONTACTDATA
///< class HotListManager has ContactData
 
#define QUICK_CAP_800
///< Collective define for Quick features from 8.00

#define QUICK_CAP_DOWNLOAD_USES_SUBWIN
///< Download dialog can use subwin

#define QUICK_CAP_ASK_PLUGIN_DOWNLOAD
///< Has the AskPluginDownloadDialog class

#define QUICK_CAP_SETUPMANAGER_FILES
///< Support OpSetupManager files as file preferences

#endif // QUICK_CAPABILITIES_H

