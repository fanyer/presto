/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef COOKIE_CAPABILITIES_H
#define COOKIE_CAPABILITIES_H

// This module has the function GetServerName in Cookie_Item_Handler
#define COOKIES_CAP_COOKIE_ITEM_SERVERNAME

// This module has window as parameter to function HandleSingleCookie
#define COOKIES_CAP_COOKIE_SINGLE_COOKIE_WINDOW

// Tells if Cookie_Manager::GetContextFolder exists
#define COOKIES_CAP_COOKIE_MGR_HAS_GETCONTEXTFOLDER

#endif // COOKIE_CAPABILITIES_H
