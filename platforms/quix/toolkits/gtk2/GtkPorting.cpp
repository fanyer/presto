/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#include "platforms/quix/toolkits/gtk2/GtkPorting.h"

#if defined(GTK2_DEPRECATED) || defined(GTK2_22)

GtkWidget* gtk_combo_box_new_with_entry()
	{ return gtk_combo_box_entry_new(); }

#endif // GTK <= 2.22

#ifdef GTK2_DEPRECATED

GtkStyle* gtk_widget_get_style(GtkWidget *widget)
{
	return widget->style;
}

GtkWidget* gtk_color_selection_dialog_get_color_selection(GtkColorSelectionDialog *colorsel)
{
	return colorsel->colorsel;
}

GdkWindow* gtk_widget_get_window(GtkWidget *widget)
{ 
	return widget->window;
}

void gtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation)
{
	*allocation = widget->allocation;
}

void gtk_widget_set_allocation(GtkWidget *widget, const GtkAllocation *allocation)
{
	widget->allocation = *allocation;
}

#endif // GTK_DEPRECATED

void op_gtk_paint_arrow		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, GtkArrowType arrow_type, gboolean fill, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_paint_arrow(style, surface, state_type, shadow_type, widget, detail, arrow_type, fill, x, y, width, height);
#else
	gtk_paint_arrow(style, surface, state_type, shadow_type, area, widget, detail, arrow_type, fill, x, y, width, height);
#endif
}

void op_gtk_paint_box		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_paint_box(style, surface, state_type, shadow_type, widget, detail, x, y, width, height);
#else
	gtk_paint_box(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height);
#endif
}

void op_gtk_paint_box_gap	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height, GtkPositionType gap_side, gint gap_x, gint gap_width)
{
#ifdef GTK3
	gtk_paint_box_gap(style, surface, state_type, shadow_type, widget, detail, x, y, width, height, gap_side, gap_x, gap_width);
#else
	gtk_paint_box_gap(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height, gap_side, gap_x, gap_width);
#endif
}

void op_gtk_paint_check		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_paint_check(style, surface, state_type, shadow_type, widget, detail, x, y, width, height);
#else
	gtk_paint_check(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height);
#endif
}

void op_gtk_paint_extension	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height, GtkPositionType gap_side)
{
#ifdef GTK3
	gtk_paint_extension(style, surface, state_type, shadow_type, widget, detail, x, y, width, height, gap_side);
#else
	gtk_paint_extension(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height, gap_side);
#endif
}

void op_gtk_paint_flat_box	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_paint_flat_box(style, surface, state_type, shadow_type, widget, detail, x, y, width, height);
#else
	gtk_paint_flat_box(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height);
#endif
}

void op_gtk_paint_focus		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_paint_focus(style, surface, state_type, widget, detail, x, y, width, height);
#else
	gtk_paint_focus(style, surface, state_type, area, widget, detail, x, y, width, height);
#endif
}

void op_gtk_paint_hline		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x1, gint x2, gint y)
{
#ifdef GTK3
	gtk_paint_hline(style, surface, state_type, widget, detail, x1, x2, y);
#else
	gtk_paint_hline(style, surface, state_type, area, widget, detail, x1, x2, y);
#endif
}

void op_gtk_paint_option	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_paint_option(style, surface, state_type, shadow_type, widget, detail, x, y, width, height);
#else
	gtk_paint_option(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height);
#endif
}

void op_gtk_paint_shadow	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height) 
{
#ifdef GTK3
	gtk_paint_shadow(style, surface, state_type, shadow_type, widget, detail, x, y, width, height);
#else
	gtk_paint_shadow(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height);
#endif
}

void op_gtk_paint_slider	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height, GtkOrientation orientation)
{
#ifdef GTK3
	gtk_paint_slider(style, surface, state_type, shadow_type, widget, detail, x, y, width, height, orientation);
#else
	gtk_paint_slider(style, surface, state_type, shadow_type, area, widget, detail, x, y, width, height, orientation);
#endif
}

void op_gtk_paint_vline		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint y1_, gint y2_, gint x)
{
#ifdef GTK3
	gtk_paint_vline(style, surface, state_type, widget, detail, y1_, y2_, x);
#else
	gtk_paint_vline(style, surface, state_type, area, widget, detail, y1_, y2_, x);
#endif
}

void op_gtk_style_apply_default_background (GtkStyle *style, op_gdk_surface *surface, GdkWindow	*window, gboolean set_bg, GtkStateType state_type, const GdkRectangle *area, gint x, gint y, gint width, gint height)
{
#ifdef GTK3
	gtk_style_apply_default_background(style, surface, window, state_type, x, y, width, height);
#else
	gtk_style_apply_default_background(style, surface, set_bg, state_type, area, x, y, width, height);
#endif
}
