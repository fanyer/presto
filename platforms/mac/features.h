/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef  FEATURES_H
#define  FEATURES_H


#include "adjunct/quick/quick-features.h"

/***********************************************************************************
 ** Feature overrides
 ***********************************************************************************/

/* Define license agreement for opera:about */
#undef   FEATURE_3P_GROWL
#define  FEATURE_3P_GROWL                                                  YES

/* Embedded browser. API designed for Macromedia, re-licenced to Adobe. Now used for the
* App-Framework separation, including widgets. */
#undef   FEATURE_EMBROWSER
#define  FEATURE_EMBROWSER                                                 YES



/***********************************************************************************
 ** Quick define overrides
 ***********************************************************************************/

#undef   _BITTORRENT_SUPPORT_ // TODO should be feature

/* We disable IRC_SUPPORT via tweaks, but we need this for the feature scanner */
#undef   FEATURE_IRC
#define  FEATURE_IRC                                                       NO

/* Dump profile information to /klient/ */
extern void ProfileOperaPop();
extern void ProfileOperaPopThenPush(UINT64 unique_id);
extern void ProfileOperaPrepare();
extern void ProfileOperaPush(UINT64 unique_id);

#undef   PROFILE_OPERA_POP
#undef   PROFILE_OPERA_POP_THEN_PUSH
#undef   PROFILE_OPERA_PREPARE
#undef   PROFILE_OPERA_PUSH

#define  PROFILE_OPERA_POP()                                               ProfileOperaPop()
#define  PROFILE_OPERA_POP_THEN_PUSH(x)                                    ProfileOperaPopThenPush(x)
#define  PROFILE_OPERA_PREPARE()                                           ProfileOperaPrepare()
#define  PROFILE_OPERA_PUSH(x)                                             ProfileOperaPush(x)

#undef   SUPPORT_MAIN_BAR
#undef   SUPPORT_NAVIGATION_BAR
#undef   SUPPORT_START_BAR



/***********************************************************************************
 ** Misc mess
 ***********************************************************************************/


/********** TODO Should be Quick feature **********/

/* Mac specific bookmark formats */
#define  SAFARI_BOOKMARKS_SUPPORT
#define  OMNIWEB_BOOKMARKS_SUPPORT

/* Some mac's have the Apple remote now. Enables certain OpKeys in htm_ldoc.cpp */
#define  HAS_REMOTE_CONTROL

// Add to restore fullscreen mode on MacOSx on startup
#define RESTORE_MAC_FULLSCREEN_ON_STARTUP


/********** TODO Should be Mac feature **********/

/* Mac-only defines. These should not be used outside platforms/mac. */
#define  SUPPORT_OSX_SERVICES
#define  _ALLOW_CONTROL_KEY_AND_COMMAND_KEY_IN_SHORTCUTS_



/***********************************************************************************
 ** Further overrides
 ***********************************************************************************/

// DSK-357686
#include "platforms/mac/Resources/buildnum.h"

/* This include MUST be last in the file */
#include "adjunct/quick/custombuild-features.h"

#endif   /* !FEATURES_H */
