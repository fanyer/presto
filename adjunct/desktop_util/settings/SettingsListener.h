/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef SETTINGS_LISTENER_H
#define SETTINGS_LISTENER_H

class DesktopSettings;

class SettingsListener
{
	public:

		virtual ~SettingsListener();

		virtual void	OnSettingsChanged(DesktopSettings* settings) {}
};

#endif // SETTINGS_LISTENER_H
