/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CONTENT_FILTER_CAPABILITIES_H
#define CONTENT_FILTER_CAPABILITIES_H

#define CONTENT_FILTER_MODULE_NAME ?
#define CONTENT_FILTER_MODULE_VERSION 1
#define CONTENT_FILTER_MODULE_FINAL_VERSION 1

#define CF_CAP_CONTENT_FILTER_MODULE

// The CheckURL() methods have an optional Window argument
#define CF_CAP_CHECKURL_HAS_WINDOW

// Method HasExcludeRules() is available
#define CF_CAP_HASEXCLUDERULES

#endif // CONTENT_FILTER_CAPABILITIES_H
