/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#ifndef GTK_COLOR_CHOOSER_H
#define GTK_COLOR_CHOOSER_H

#include "platforms/quix/toolkits/ToolkitColorChooser.h"

#include <gtk/gtk.h>

class GtkToolkitColorChooser : public ToolkitColorChooser
{
public:
	GtkToolkitColorChooser();

	virtual ~GtkToolkitColorChooser();

	bool Show(X11Types::Window parent, uint32_t initial_color);

	uint32_t GetColor() { return m_color; };

private:
	GtkWidget* m_dialog;
	GtkWidget* m_color_selector;
	uint32_t m_color;
};

#endif // GTK_COLOR_CHOOSER_H

