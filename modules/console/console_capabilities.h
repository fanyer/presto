/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef CONSOLE_CAPABILITIES_H
#define CONSOLE_CAPABILITIES_H

/** OpConsoleEngine::Message contains window id.
  * Added 2005-08 on VCS 1. */
#define CON_CAP_WINDOWID

/** OpConsoleEngine::Message contains time
  * Added 2005-10 on VCS 1. */
#define CON_CAP_TIME

/** OpConsoleEngine has Database section */
#define CON_CAP_DATABASE_SECTION

/** OpConsoleEngine::Source has OperaLink and OperaUnite
  * Added 2009-10 */
#define CON_CAP_LINK_UNITE

#endif // CONSOLE_CAPABILITIES_H
