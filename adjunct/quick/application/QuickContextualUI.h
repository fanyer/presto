/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef QUICK_CONTEXTUAL_UI
#define QUICK_CONTEXTUAL_UI

class OpLineParser;

// Test whether a string that represent a UI widget(menu item, dialog widget etc) should be skipped
// @param line The recognized syntax such as "Platform" and "Feature" etc will be consumed when the funtion returns
// @return TRUE indicates this entry should be skipped
// @param first_token The first token that can't be recognized. e.g. for "Feature Feeds, Submenu, ..." it would be "Submenu"
BOOL SkipEntry(OpLineParser& line, OpString& first_token);

#endif // QUICK_CONTEXTUAL_UI
