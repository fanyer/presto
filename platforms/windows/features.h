/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef  FEATURES_H
#define  FEATURES_H

// This include should be first in the file
#include "adjunct/quick/quick-features.h"


/***********************************************************************************
 ** Feature overrides
 ***********************************************************************************/

#undef   FEATURE_GENERIC_PRINTING
#define  FEATURE_GENERIC_PRINTING                                          NO

#if defined(_DEBUG) && !defined(DESKTOP_STARTER) && !defined(OPERA_MAPI) && !defined(PLUGIN_WRAPPER)
# undef   FEATURE_MEMORY_DEBUGGING
# define  FEATURE_MEMORY_DEBUGGING                                         YES
#endif

#undef   FEATURE_TOUCH_EVENTS
#define  FEATURE_TOUCH_EVENTS                                              YES

/***********************************************************************************
 ** Quick define overrides
 ***********************************************************************************/

extern void ProfileOperaPop();
extern void ProfileOperaPopThenPush(UINT32 unique_id);
extern void ProfileOperaPrepare();
extern void ProfileOperaPush(UINT32 unique_id);

#undef   PROFILE_OPERA_POP
#define  PROFILE_OPERA_POP()                                               ProfileOperaPop()

#undef   PROFILE_OPERA_POP_THEN_PUSH
#define  PROFILE_OPERA_POP_THEN_PUSH(x)                                    ProfileOperaPopThenPush(x)

#undef   PROFILE_OPERA_PREPARE
#define  PROFILE_OPERA_PREPARE()                                           ProfileOperaPrepare()

#undef   PROFILE_OPERA_PUSH
#define  PROFILE_OPERA_PUSH(x)                                             ProfileOperaPush(x)



/***********************************************************************************
 ** Misc mess
 ***********************************************************************************/

/********** TODO Should be Windows feature **********/
#define  _RAS_SUPPORT_
#define  VEGA_AERO_INTEGRATION
#define  VEGA_GDIFONT_SUPPORT


/********** TODO Should be feature or removed **********/
#define  _SUPPORT_WINSOCK2


/********** TODO Should be Quick feature **********/
// Allow multiple menu items to share a common shortcut key
// presing the key will switch highlight among those items.
#define  ALLOW_MENU_SHARE_SHORTCUT
#define  _DDE_SUPPORT_
#define  _INTELLIMOUSE_SUPPORT_
#define  HAS_REMOTE_CONTROL



/***********************************************************************************
 ** Further overrides
 ***********************************************************************************/

// DSK-357686
#include "platforms/windows/windows_ui/res/#buildnr.rci"

// This include MUST be last in the file
#include "adjunct/quick/custombuild-features.h"


#endif   // !FEATURES_H
