/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#include "platforms/quix/toolkits/gtk2/GtkColorChooser.h"

#include "platforms/quix/toolkits/gtk2/GtkPorting.h"
#include "platforms/quix/toolkits/gtk2/GtkUtils.h"
#include <gdk/gdkx.h>

GtkToolkitColorChooser::GtkToolkitColorChooser()
	: m_dialog(NULL)
	, m_color_selector(NULL)
	, m_color(0x0)
{
	m_dialog = gtk_color_selection_dialog_new( NULL /* gchar *title */ );

	m_color_selector = gtk_color_selection_dialog_get_color_selection(
			GTK_COLOR_SELECTION_DIALOG(m_dialog));
}


GtkToolkitColorChooser::~GtkToolkitColorChooser()
{
}


bool GtkToolkitColorChooser::Show(X11Types::Window parent, uint32_t initial_color)
{
	GdkColor old_color = GtkUtils::ColorrefToGdkColor(initial_color);
	gtk_color_selection_set_current_color (
			GTK_COLOR_SELECTION(m_color_selector), &old_color);

	// we have to show dialog before running it
	gtk_widget_show(m_dialog);
	// otherwise this will result in nasty crash inside gtk
	XSetTransientForHint(GDK_WINDOW_XDISPLAY(gtk_widget_get_window(m_dialog)),
						 GDK_WINDOW_XID(gtk_widget_get_window(m_dialog)), parent);
	GtkUtils::SetResourceName(m_dialog, "colorselectordialog");

	gint result = gtk_dialog_run(GTK_DIALOG(m_dialog));

	GdkColor new_color;
	gtk_color_selection_get_current_color(
			GTK_COLOR_SELECTION(m_color_selector), &new_color);

	m_color = GtkUtils::GdkColorToColorref(&new_color);

	gtk_widget_destroy (m_dialog);
	GtkUtils::ProcessEvents();

	return (GTK_RESPONSE_OK == result);
}

