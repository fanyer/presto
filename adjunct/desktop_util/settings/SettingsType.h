/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef SETTINGS_TYPE_H
#define SETTINGS_TYPE_H

/** @brief User-changeble settings types
  */
enum SettingsType
{
    SETTINGS_SKIN,
    SETTINGS_SKIN_SCALE,
    SETTINGS_SYSTEM_SKIN,
    SETTINGS_COLOR_SCHEME,
    SETTINGS_SPECIAL_EFFECTS,
    SETTINGS_CUSTOMIZE_BEGIN,
    SETTINGS_CUSTOMIZE_UPDATE,
    SETTINGS_CUSTOMIZE_END_CANCEL,
    SETTINGS_CUSTOMIZE_END_OK,
    SETTINGS_TOOLBAR_SETUP,
    SETTINGS_TOOLBAR_CONTENTS,
	SETTINGS_TOOLBAR_STYLE,
	SETTINGS_TOOLBAR_ALIGNMENT,
    SETTINGS_MENU_SETUP,
    SETTINGS_KEYBOARD_SETUP,
    SETTINGS_MOUSE_SETUP,
	SETTINGS_VOICE_SETUP_obsolete,
    SETTINGS_REGISTERED,
    SETTINGS_DELETE_PRIVATE_DATA,
    SETTINGS_IDENTIFY_AS,
    SETTINGS_UI_FONTS,
    SETTINGS_LANGUAGE,
    SETTINGS_ACCOUNT_SELECTOR,
    SETTINGS_ADVERTISEMENT,
    SETTINGS_MULTIMEDIA,
    SETTINGS_DRAG_BEGIN,
    SETTINGS_DRAG_END,
    SETTINGS_PROXY,
    SETTINGS_DISPLAY,			// display resolution and/or depth has changed
    SETTINGS_SPEED_DIAL,		// speed dial composition has changed
    SETTINGS_SEARCH,
    SETTINGS_PERSONA,			// the active persona has changed
    SETTINGS_EXTENSION_TOOLBAR_CONTENTS
};

#endif // SETTINGS_TYPE_H
