/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#ifndef GTK_PORTING_H
#define GTK_PORTING_H

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

// Process of deprecating gtk2 api started with 2.22 - preferably we
// would like to compile with 2.24 (since it is last stable version
// of gtk and shares parts of new interface with gtk3), but we need
// to be able to compile also on 2.20 and 2.22 for now.
//
// FIXME Remove everything ifdefed with GTK2_* as soon as our
//       build machines will be updated to gtk 2.24.

#if GTK_MAJOR_VERSION == 3
# define GTK3
#endif

#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION == 24
# define GTK2_24
#endif

#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION == 22
# define GTK2_22
#endif

#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 22
# define GTK2_DEPRECATED
#endif

#ifdef GTK3
# define op_gdk_surface cairo_t
#else
# undef GDK_WINDOW_XDISPLAY
# define GDK_WINDOW_XDISPLAY(win)      (gdk_x11_drawable_get_xdisplay (((GdkWindowObject *)win)->impl))
# define op_gdk_surface GdkPixmap
#endif

#ifdef GTK2_22

GtkWidget* gtk_combo_box_new_with_entry();

#endif

#ifdef GTK2_DEPRECATED

GtkWidget* gtk_combo_box_new_with_entry();

GtkStyle* gtk_widget_get_style(GtkWidget *widget);

GtkWidget* gtk_color_selection_dialog_get_color_selection(GtkColorSelectionDialog *colorsel);

GdkWindow* gtk_widget_get_window(GtkWidget *widget);

void gtk_widget_get_allocation(GtkWidget *widget, GtkAllocation *allocation);

void gtk_widget_set_allocation(GtkWidget *widget, const GtkAllocation *allocation);

#endif

void op_gtk_paint_arrow		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, GtkArrowType arrow_type, gboolean fill, gint x, gint y, gint width, gint height);
void op_gtk_paint_box		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height);
void op_gtk_paint_box_gap	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height, GtkPositionType gap_side, gint gap_x, gint gap_width);
void op_gtk_paint_check		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height);
void op_gtk_paint_extension	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height, GtkPositionType gap_side);
void op_gtk_paint_flat_box	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height);
void op_gtk_paint_focus		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height);
void op_gtk_paint_hline		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x1, gint x2, gint y);
void op_gtk_paint_option	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height);
void op_gtk_paint_shadow	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height); 
void op_gtk_paint_slider	(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, GtkShadowType shadow_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint x, gint y, gint width, gint height, GtkOrientation orientation);
void op_gtk_paint_vline		(GtkStyle *style, op_gdk_surface *surface, GtkStateType state_type, const GdkRectangle *area, GtkWidget *widget, const gchar *detail, gint y1_, gint y2_, gint x);

void op_gtk_style_apply_default_background (GtkStyle *style, op_gdk_surface *surface, GdkWindow *window, gboolean set_bg, GtkStateType state_type, const GdkRectangle *area, gint x, gint y, gint width, gint height);

#endif // GTK_PORTING_H
