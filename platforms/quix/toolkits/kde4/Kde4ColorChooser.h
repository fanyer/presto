/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#ifndef KDE4_COLOR_CHOOSER_H
#define KDE4_COLOR_CHOOSER_H

#include "platforms/quix/toolkits/ToolkitColorChooser.h"


class Kde4ColorChooser : public ToolkitColorChooser
{
public:
	Kde4ColorChooser();

	~Kde4ColorChooser();

	bool Show(X11Types::Window parent, uint32_t initial_color);

	uint32_t GetColor() { return m_color; };

private:
	uint32_t m_color;
};

#endif // KDE4_COLOR_CHOOSER_H
