/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#ifndef TOOLKIT_COLOR_CHOOSER_H
#define TOOLKIT_COLOR_CHOOSER_H

#include "platforms/utilix/x11types.h"

#include <stdint.h>

class ToolkitColorChooser
{
public:
	virtual ~ToolkitColorChooser() {}

	/** Shows modal color selector dialog.
	  * @param parent X11 window id, that selector dialog
	  *               should be transient for
	  * @param initial_color COROSPEC that dialog should set
	  *               as default color for user choice
	  * @retval TRUE if user accepted new selected color
	  * @ratval FALSE otherwise
	  */
	virtual bool Show(X11Types::Window parent, uint32_t initial_color) = 0;

	/** Get color selected in dialog.
	  * $return COLORSPEC that can be used as selected color
	  */
	virtual uint32_t GetColor() = 0;
};

#endif // TOOLKIT_COLOR_CHOOSER_H
