/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/gtk2/GtkUtils.h"
#include "platforms/quix/toolkits/gtk2/GtkToolkitLibrary.h"
#include "platforms/quix/toolkits/gtk2/GtkPorting.h"
#include <stdlib.h>
#include <string.h>

void GtkUtils::ProcessEvents()
{
	GtkToolkitLibrary::SetCanCallRunSlice(false);

	while (gtk_events_pending())
		gtk_main_iteration();

	GtkToolkitLibrary::SetCanCallRunSlice(true);
}

GtkUtils::String& GtkUtils::String::operator=(const char* data)
{
	delete[] m_data;
	m_data = CopyString(data);
	return *this;
}

char* GtkUtils::String::CopyString(const char* data)
{
	if (!data)
		return 0;

	char* copy = new char[strlen(data) + 1];
	if (copy)
		strcpy(copy, data);
	return copy;
}

GdkColor GtkUtils::ColorrefToGdkColor(const uint32_t color)
{
	uint32_t red, green, blue;
	red	  = 0xff &  color;
	green = 0xff & (color >> 8);
	blue  = 0xff & (color >> 16);
	GdkColor gcolor = { 0,
		red   << 8 | red,
		green << 8 | green,
		blue  << 8 | blue
	};
	return gcolor;
}

uint32_t GtkUtils::GdkColorToColorref(const GdkColor *gcolor)
{
	uint32_t red, green, blue;
	red	  = (0xff00 & gcolor->red) >> 8;
	green = (0xff00 & gcolor->green);
	blue  = (0xff00 & gcolor->blue) << 8;
	return blue|green|red;
}

void GtkUtils::SetResourceName(GtkWidget* widget, const char* name)
{
	if (!widget)
		return;

	char* copy = name ? strdup(name) : 0; // avoid compile error wrt const assignment

	XClassHint hint;
	char opera[6];
	strcpy(opera, "Opera");
	hint.res_class = opera;
	hint.res_name  = copy ? copy : hint.res_class;
	XSetClassHint(GDK_WINDOW_XDISPLAY(gtk_widget_get_window(widget)),
			GDK_WINDOW_XID(gtk_widget_get_window(widget)), &hint);

	free(copy);
}
