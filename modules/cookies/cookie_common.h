/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef COOKIE_COMMON_H
#define COOKIE_COMMON_H

#define COOKIE_BUFFER_SIZE		(COOKIE_HEADER_MAX_LEN + 100)
#define _VERSION_2_COOKIE_FILE_VERSION 1002
#define COOKIES_FILE_VERSION	0x00002001
#define COOKIES_FILE_VERSION_BUGGY_RECVD_PATH	0x00002000
#define COOKIES_FILE_VERSION_OLD	1000
#define COOKIES_FILE_VERSION_BOGUS	1001

#define CookiesFile UNI_L("cookies4.dat")
#define CookiesFileNew UNI_L("cookies4.new")
#define CookiesFileOld UNI_L("cookies4.old")
#define OldCookiesFile UNI_L("cookies.dat")

#ifndef CONTEXT_MAX_TOTAL_COOKIES
# define CONTEXT_MAX_TOTAL_COOKIES 512
#endif

#endif // COOKIE_COMMON_H
