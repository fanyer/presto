/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FINGERTOUCH_CONFIG_H
#define FINGERTOUCH_CONFIG_H

/** Space from layerwindows edge to where the background begins
	(the skin has some padding for dropshadow and border) */
static const int LAYER_BG_PADDING = 5;

/** Space from layerwindows edge to where content should be positioned. */
static const int LAYER_PADDING = 9;

/** Extra spacing between layers. */
static const int LAYER_SPACING = 0;

#define LAYER_ELEMENT_NAME "Finger Touch Layer Skin"
#define LAYER_ELEMENT_NAME_BRIGHT "Finger Touch Layer Bright Skin"

static const int OVERLAY_HIDE_DELAY = 6000;
static const int HIDE_ANIMATION_LENGTH_MS = 200;
static const int ACTIVATION_ANIMATION_LENGTH_MS = 400;

#endif // FINGERTOUCH_CONFIG_H
