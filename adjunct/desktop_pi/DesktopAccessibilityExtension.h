/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOP_ACCESSIBILITY_EXTENSION_H
#define DESKTOP_ACCESSIBILITY_EXTENSION_H

#include "modules/pi/OpAccessibilityExtension.h"

class DesktopAccessibilityExtension :
	public OpAccessibilityExtension
{
public:

    static OP_STATUS Create(OpAccessibilityExtension** extension,
							OpAccessibilityExtension* parent,
							OpAccessibilityExtensionListener* listener,
							ElementKind kind);
};

#endif // DESKTOP_ACCESSIBILITY_EXTENSION_H
