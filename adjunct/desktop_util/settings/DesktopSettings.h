/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DESKTOP_SETTINGS_H
#define DESKTOP_SETTINGS_H

#include "adjunct/desktop_util/settings/SettingsType.h"
#include "modules/util/OpHashTable.h"

/** @brief class for maintaining which settings have changed
  */
class DesktopSettings : private OpINT32Set
{
public:
	
    void Add(SettingsType setting) {OpINT32Set::Add(setting);}
    BOOL IsChanged(SettingsType setting) const {return OpINT32Set::Contains(setting);}
};

#endif // SETTINGS_H
