/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/gtk2/GtkSkinElement.h"

#include <gdk/gdkx.h>
#include <math.h>
#include <string.h>

#define IMAGE_COLUMN_WIDTH 22
#define MENU_LEFT_MARGIN 3

using namespace GtkSkinElements;

GtkSkinElement::~GtkSkinElement()
{
	if (m_all_widgets)
		g_hash_table_destroy(m_all_widgets);

	if (m_widget)
	{
		// Work-around for DSK-348617 - to be reverted some time when this is no longer a major problem ...
#if 0
		gtk_widget_destroy(m_widget);
#else
		if (GTK_IS_TOOLBAR(m_widget))
			gtk_container_remove(GTK_CONTAINER(m_layout), m_widget);
		else
			gtk_widget_destroy(m_widget);
#endif
	}
}

#ifndef GTK3

void GtkSkinElement::Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	gtk_widget_set_direction(m_widget, (state & STATE_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);

	GtkStyle* style = gtk_widget_get_style(m_widget);

	if (IsTopLevel())
	{
		style = gtk_style_attach(style, gtk_widget_get_window(m_widget));
	}
	else
	{
		style = gtk_style_attach(style, gtk_widget_get_parent_window(m_widget));
	}

	GdkRectangle gtk_clip_rect = { clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height };

	if (RespectAlpha())
		DrawWithAlpha(bitmap, width, height, gtk_clip_rect, style, state);
	else
		DrawSolid(bitmap, width, height, gtk_clip_rect, style, state);

	gtk_style_detach(style);
}

void GtkSkinElement::ChangeTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkStyle* style = gtk_widget_get_style(m_widget);

	style = gtk_style_attach(style, GetGdkWindow());

	GdkGCValues values;
	// use foreground context instead of text DSK-298761
	gdk_gc_get_values(style->fg_gc[GetGtkState(state)], &values);

	GdkColor color;
	gdk_colormap_query_color(gdk_gc_get_colormap(style->text_gc[GetGtkState(state)]), values.foreground.pixel, &color);

	red = color.red;
	green = color.green;
	blue = color.blue;
	alpha = 0xff;
}

#endif // !GTK3

bool GtkSkinElement::CreateInternalWidget()
{
	m_widget = CreateWidget();
	if (!m_widget)
		return false;

	if (!IsTopLevel())
	{
		if (!gtk_widget_get_parent(m_widget))
		        gtk_container_add(GTK_CONTAINER(m_layout), m_widget);
	}

	if (!m_all_widgets)
		m_all_widgets = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, 0);

	RealizeSubWidgets(m_widget, m_all_widgets);

	return true;
}

#ifndef GTK3

void GtkSkinElement::DrawWithAlpha(uint32_t* bitmap, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state)
{
	// To determine the alpha channel, we draw once on black and once on white background
#ifdef GTK2_DEPRECATED
	GdkPixbuf* on_black = DrawOnBackground(style->black_gc, width, height, clip_rect, style, state);
	GdkPixbuf* on_white = DrawOnBackground(style->white_gc, width, height, clip_rect, style, state);
#else
	GdkPixbuf* on_black = DrawOnBackground(0, 0, 0, width, height, clip_rect, style, state);
	GdkPixbuf* on_white = DrawOnBackground(1, 1, 1, width, height, clip_rect, style, state);
#endif // GTK2_DEPRECATED

	if (on_black && on_white)
	{
		guchar* black_data = gdk_pixbuf_get_pixels(on_black);
		guchar* white_data = gdk_pixbuf_get_pixels(on_white);

		for (int i = 0; i < width * height; i++)
		{
			bitmap[i] = GetARGB(black_data, white_data);
			black_data += 4;
			white_data += 4;
		}
	}

	g_object_unref(on_black);
	g_object_unref(on_white);
}

void GtkSkinElement::DrawSolid(uint32_t* bitmap, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state)
{
#ifdef GTK2_DEPRECATED
	GdkPixbuf* pixbuf = DrawOnBackground(style->white_gc, width, height, clip_rect, style, state);
#else
	GdkPixbuf* pixbuf = DrawOnBackground(1, 1, 1, width, height, clip_rect, style, state);
#endif // GTK2_DEPRECATED

	if (pixbuf)
	{
		guchar* data = gdk_pixbuf_get_pixels(pixbuf);

		for (int i = 0; i < width * height; i++)
		{
			bitmap[i] = GetARGB(data);
			data += 4;
		}
	}

	g_object_unref(pixbuf);
}

#ifdef GTK2_DEPRECATED
GdkPixbuf* GtkSkinElement::DrawOnBackground(GdkGC* background_gc, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state)
#else
GdkPixbuf* GtkSkinElement::DrawOnBackground(double red, double green, double blue, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state)
#endif // GTK2_DEPRECATED
{
	GdkPixmap *pixmap = 0;
	if (!IsTopLevel())
	{
		pixmap = gdk_pixmap_new(gtk_widget_get_parent_window(m_widget), width, height, -1);
	}
	else
	{
		pixmap = gdk_pixmap_new(gtk_widget_get_window(m_widget), width, height, -1);
	}

	if (!pixmap)
		return 0;

	// Draw background
#ifdef GTK2_DEPRECATED
	gdk_draw_rectangle(pixmap, background_gc, true, 0, 0, width, height);
#else // gtk 2.22 or gtk 2.24
	cairo_t *cr = gdk_cairo_create(pixmap);
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_rectangle(cr, 0, 0, double(width), double(height));
	cairo_fill(cr);
	cairo_destroy(cr);
#endif // GTK2_DEPRECATED

	// Draw content
	GtkDraw(pixmap, width, height, clip_rect, m_widget, style, state);

	// Convert content to pixbuf
	GdkPixbuf *img = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, width, height);
	if (!img)
	{
		g_object_unref(pixmap);
		return 0;
	}
	img = gdk_pixbuf_get_from_drawable(img, pixmap, NULL, clip_rect.x, clip_rect.y, clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height);

	g_object_unref(pixmap);
	return img;
}

#endif // !GTK3

void GtkSkinElement::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	// From the Gnome HIG: http://library.gnome.org/devel/hig-book/2.30/design-window.html.en
	// we leave at least 6 pixels between elements
	// Other elements that implement this function should use a multiple of 6
	left = top = right = bottom = 6;
}

GtkStateType GtkSkinElement::GetGtkState(int state)
{
	if (state & STATE_DISABLED)
		return GTK_STATE_INSENSITIVE;
	if (state & STATE_PRESSED)
		return GTK_STATE_ACTIVE;
	if (state & STATE_HOVER)
		return GTK_STATE_PRELIGHT;
	if (state & STATE_SELECTED)
		return GTK_STATE_SELECTED;

	return GTK_STATE_NORMAL;
}

#ifdef DEBUG
void GtkSkinElement::PrintState(int state)
{
	switch (state)
	{
		case GTK_STATE_NORMAL:
			printf("GTK_STATE_NORMAL\n");
			break;
		case GTK_STATE_ACTIVE:
			printf("GTK_STATE_ACTIVE\n");
			break;
		case GTK_STATE_PRELIGHT:
			printf("GTK_STATE_PRELIGHT\n");
			break;
		case GTK_STATE_SELECTED:
			printf("GTK_STATE_SELECTED\n");
			break;
		case GTK_STATE_INSENSITIVE:
			printf("GTK_STATE_INSENSITIVE\n");
			break;
		default:
			printf("Unknown state\n");
	}
}
#endif // DEBUG

void GtkSkinElement::RealizeSubWidgets(GtkWidget* widget, gpointer widget_table)
{
	gtk_widget_realize(widget);

	char* class_path;
	gtk_widget_path (widget, NULL, &class_path, NULL);

	//printf("%s\n", class_path);

	g_hash_table_insert(static_cast<GHashTable*>(widget_table), class_path, widget);

	if (G_TYPE_CHECK_INSTANCE_TYPE ((widget), gtk_container_get_type()))
	{
		gtk_container_forall(GTK_CONTAINER(widget), &RealizeSubWidgets, widget_table);
	}
}

void EmptyElement::Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state)
{
	memset(bitmap, 0, sizeof(*bitmap) * width * height);
}

#ifndef GTK3
void ScrollbarDirection::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	const GtkArrowType arrow = GetArrow();

	// Rounded corners. FixMe: This code segment need to be modified when we support double arrow buttons
	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);

	allocation.x = clip_rect.x;
	allocation.y = clip_rect.y;
	allocation.width = clip_rect.width;
	allocation.height = clip_rect.height;

	if (m_orientation == VERTICAL) 
	{
		allocation.height *= 5;
		if (arrow == GTK_ARROW_DOWN) 
			allocation.y -= 4 * clip_rect.height;
	} 
	else 
	{
		allocation.width *= 5;
		if (arrow == GTK_ARROW_RIGHT) 
			allocation.x -= 4 * clip_rect.width;
	}

	gtk_widget_set_allocation(widget, &allocation);

	GtkShadowType shadow = (state & STATE_PRESSED) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
	const char* style_detail = (m_orientation == VERTICAL) ? "vscrollbar" : "hscrollbar";
	GtkStateType gtk_state = GetGtkState(state & ~STATE_SELECTED); // STATE_SELECTED does not occur in native buttons

	gtk_paint_box(style, pixmap, gtk_state, shadow, &clip_rect, widget, style_detail, 0, 0, width, height);
	gtk_paint_arrow(style, pixmap, gtk_state, GTK_SHADOW_NONE, &clip_rect, widget, style_detail, arrow, FALSE, 4, 4, width-8, height-8);
}
#endif // !GTK3

GtkArrowType ScrollbarDirection::GetArrow()
{
	switch (m_direction)
	{
		case UP:
			return GTK_ARROW_UP;
		case DOWN:
			return GTK_ARROW_DOWN;
		case LEFT:
			return GTK_ARROW_LEFT;
		case RIGHT:
			return GTK_ARROW_RIGHT;
	}

	return GTK_ARROW_NONE;
}

void ScrollbarKnob::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	// Set the correct state and shadow
	GtkShadowType shadow_type = GTK_SHADOW_OUT;
	GtkStateType state_modified = (GetGtkState(state) == GTK_STATE_PRELIGHT || GetGtkState(state) == GTK_STATE_ACTIVE) ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL;

#ifndef GTK3
	gboolean activate_slider = false;
	gtk_widget_style_get(widget, "activate-slider",  &activate_slider, NULL);
    if (activate_slider && GetGtkState(state) == GTK_STATE_ACTIVE) 
	{
        shadow_type = GTK_SHADOW_IN;
        state_modified = GTK_STATE_ACTIVE;
    }
#endif // !GTK3

	gint focus_padding = 1;
	gtk_widget_style_get(m_widget, "focus-line-width", &focus_padding, NULL);
	int x = 0, y = 0;
	GtkOrientation orientation;
	if (m_orientation == VERTICAL)
	{
		orientation = GTK_ORIENTATION_VERTICAL;
		height = MAX(height - focus_padding * 2, 0);
		y = focus_padding;
	}
	else
	{
		orientation = GTK_ORIENTATION_HORIZONTAL;
		width = MAX(width - focus_padding * 2, 0);
		x = focus_padding;
	}

#ifdef GTK3
	gint border = 0;
	gtk_widget_style_get(m_widget, "trough-border", &border, NULL);
	if (m_orientation == VERTICAL)
	{
		x += border;
		width -= 2 * border;
	}
	else
	{
		y += border;
		height -= 2 * border;
	}
#endif

	op_gtk_paint_slider(style, pixmap, state_modified, shadow_type, &clip_rect, widget, "slider", x, y, width, height, orientation);
}

void ScrollbarBackground::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
    op_gtk_style_apply_default_background(style, pixmap, gtk_widget_get_parent_window(m_widget), TRUE, GTK_STATE_ACTIVE,  &clip_rect, 0, 0, width, height);
    op_gtk_paint_box(style, pixmap, GTK_STATE_ACTIVE, GTK_SHADOW_IN, &clip_rect, widget, "trough", 0, 0, width, height);
    if (GetGtkState(state)==GTK_STATE_SELECTED) 
	{
        op_gtk_paint_focus(style, pixmap, GTK_STATE_ACTIVE, &clip_rect, widget, "trough", 0, 0, width, height); // check
    }
}

void ScrollbarBackground::ChangeDefaultSize(int& width, int& height, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkStyle* style = gtk_widget_get_style(m_widget);
	style = gtk_style_attach (style, gtk_widget_get_parent_window(m_widget));

	gint slider_width = 14;
	gtk_widget_style_get(m_widget, "slider-width", &slider_width, NULL);

#ifdef GTK3
	gint border = 0;
	gtk_widget_style_get(m_widget, "trough-border", &border, NULL);
	slider_width += 2 * border;
#endif

	gtk_style_detach(style);

	if (m_orientation == VERTICAL)
		width = slider_width;
	else
		height = slider_width;
}

void DropdownButton::ChangeDefaultSize(int& width, int& height, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkAllocation geometry = { 0, 0, 200, height };
	gtk_widget_size_allocate(m_widget, &geometry);

	GtkWidget* toggle_button = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBoxEntry.GtkToggleButton"));
	if (!toggle_button)
		return;

	GtkAllocation allocation;
	gtk_widget_get_allocation(toggle_button, &allocation);
	width = allocation.width;
}

void Dropdown::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	// Choose some reasonable values that look very much like GTK itself
	left = 3;
	right = 6;
}

void Dropdown::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkAllocation geometry = { 0, 0, width, height };
	gtk_widget_size_allocate(widget, &geometry);

	// paint button
	GtkWidget* toggle_button = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkToggleButton"));
#ifdef GTK3
	GtkWidget* arrow = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkToggleButton.GtkBox.GtkArrow"));
	GtkWidget* separator = NULL;
#else
	GtkWidget* arrow = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkToggleButton.GtkHBox.GtkArrow"));
	GtkWidget* separator = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkToggleButton.GtkHBox.GtkVSeparator"));
#endif // !GTK3

	if (!toggle_button || !arrow)
	{
		return;
	}

	gtk_widget_set_direction(toggle_button, (state & STATE_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);

	// Paint box for button
	GtkShadowType shadow = (state & STATE_PRESSED) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
	GtkAllocation toggle_button_allocation;
	gtk_widget_get_allocation(toggle_button, &toggle_button_allocation);
	op_gtk_paint_box(gtk_widget_get_style(toggle_button), pixmap,
			GetGtkState(state), shadow, &clip_rect, toggle_button,
			"button", 0, 0, toggle_button_allocation.width, toggle_button_allocation.height);

 	if (state & STATE_FOCUSED)
	{
		gboolean interior_focus = false;
		gint focus_width = 0, focus_pad = 0;
		gint x = 0, y = 0, width2 = width, height2 = height;

		gtk_widget_style_get(toggle_button, "interior-focus",  &interior_focus, "focus-line-width", &focus_width, "focus-padding", &focus_pad,  NULL);

		if (interior_focus)
		{
			const gint xthickness = gtk_widget_get_style(toggle_button)->xthickness;
			const gint ythickness = gtk_widget_get_style(toggle_button)->ythickness;
			x += xthickness + focus_pad;
			y += ythickness + focus_pad;
			width2 -= 2 * (xthickness + focus_pad);
			height2 -= 2 * (ythickness + focus_pad);
		}
		else
		{
			x -= focus_width + focus_pad;
			y -= focus_width + focus_pad;
			width2 += 2 * (focus_width + focus_pad);
			height2 += 2 * (focus_width + focus_pad);
		}
		op_gtk_paint_focus(gtk_widget_get_style(toggle_button), pixmap, GetGtkState(state), &clip_rect,  toggle_button, "button", x, y, width2, height2);
	}

	// Paint arrow
	gfloat scale = 0.7;
	gtk_widget_style_get(arrow, "arrow-scaling", &scale, NULL);

	GtkAllocation arrow_allocation;
	gtk_widget_get_allocation(arrow, &arrow_allocation);
	gint arrow_width = arrow_allocation.width * scale;
	gint arrow_height = arrow_allocation.height * scale;
	gint arrow_x = arrow_allocation.x + (arrow_allocation.width - arrow_width) / 2;
	gint arrow_y = arrow_allocation.y + (arrow_allocation.height - arrow_height) / 2;

	op_gtk_paint_arrow(style, pixmap, GetGtkState(state), GTK_SHADOW_NONE, &clip_rect, arrow, "arrow", GTK_ARROW_DOWN, FALSE, arrow_x, arrow_y, arrow_width, arrow_height);

	if (separator)
	{
		// Paint separator
		GtkAllocation separator_allocation;
		gtk_widget_get_allocation(separator, &separator_allocation);
		op_gtk_paint_vline (style, pixmap, GetGtkState(state), &clip_rect, separator,
				"vseparator",
				separator_allocation.y,
				separator_allocation.y + separator_allocation.height - 1,
				separator_allocation.x + (separator_allocation.width - gtk_widget_get_style(separator)->xthickness) / 2);
	}
}

void DropdownEdit::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	// these values are here to work around default padding hardcoded in core,
	// so edit field won't be partly drawn over toggle button
	top = bottom = 2;
	if (state & STATE_RTL)
	{
		left = 7;
		right = 2;
	}
	else
	{
		left = 2;
		right = 7;
	}
}

void DropdownEdit::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkAllocation geometry = { 0, 0, width, height };
	gtk_widget_size_allocate(widget, &geometry);

#ifdef GTK3
	GtkWidget* entry_widget = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkEntry"));
#else
	GtkWidget* entry_widget = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBoxEntry.GtkEntry"));
#endif

	if (!entry_widget)
		return;

	gtk_widget_set_direction(entry_widget, (state & STATE_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);

	// paint background
	GtkAllocation entry_widget_allocation;
	gtk_widget_get_allocation(entry_widget, &entry_widget_allocation);
	op_gtk_paint_flat_box(style, pixmap, GetGtkState(state), GTK_SHADOW_NONE,
			&clip_rect, entry_widget, "entry_bg",
			entry_widget_allocation.x,
			entry_widget_allocation.y,
			entry_widget_allocation.width,
			entry_widget_allocation.height);

	// paint frame
	op_gtk_paint_shadow(gtk_widget_get_style(entry_widget), pixmap,
			GetGtkState(state), GTK_SHADOW_IN, &clip_rect, entry_widget,
			"entry",
			entry_widget_allocation.x,
			entry_widget_allocation.y,
			entry_widget_allocation.width,
			entry_widget_allocation.height);

	// paint button
#ifdef GTK3
	GtkWidget* toggle_button = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkToggleButton"));
	GtkWidget* arrow = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBox.GtkToggleButton.GtkBox.GtkArrow"));
#else
	GtkWidget* toggle_button = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBoxEntry.GtkToggleButton"));
	GtkWidget* arrow = GTK_WIDGET(g_hash_table_lookup(m_all_widgets, "GtkWindow.GtkFixed.GtkComboBoxEntry.GtkToggleButton.GtkHBox.GtkArrow"));
#endif

	if (!toggle_button || !arrow)
		return;

	gtk_widget_set_direction(toggle_button, (state & STATE_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
	GtkShadowType shadow = (state & STATE_PRESSED) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

	// Paint box for button
	GtkAllocation toggle_button_allocation;
	gtk_widget_get_allocation(toggle_button, &toggle_button_allocation);
	op_gtk_paint_box(gtk_widget_get_style(toggle_button), pixmap,
			GetGtkState(state), shadow, &clip_rect, toggle_button, "button",
			toggle_button_allocation.x,
			toggle_button_allocation.y,
			toggle_button_allocation.width,
			toggle_button_allocation.height);

	// Paint arrow
	gfloat scale = 0.7;
	gtk_widget_style_get(arrow, "arrow-scaling", &scale, NULL);

	GtkAllocation arrow_allocation;
	gtk_widget_get_allocation(arrow, &arrow_allocation);
	gint arrow_width = arrow_allocation.width * scale;
	gint arrow_height = arrow_allocation.height * scale;
	gint arrow_x = arrow_allocation.x + (arrow_allocation.width - arrow_width) / 2;
	gint arrow_y = arrow_allocation.y + (arrow_allocation.height - arrow_height) / 2;

	op_gtk_paint_arrow(style, pixmap, GetGtkState(state), GTK_SHADOW_NONE, &clip_rect, arrow, "arrow", GTK_ARROW_DOWN, FALSE, arrow_x, arrow_y, arrow_width, arrow_height);
}

void PushButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkShadowType shadow = (state & STATE_PRESSED) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

	op_gtk_paint_box(style, pixmap, GetGtkState(state), shadow, &clip_rect, widget, "button", 0, 0, width, height);

	if (m_focused)
	{
		gboolean interior_focus = false;
		gint focus_width = 0, focus_pad = 0;
		gint x = 0, y = 0, width2 = width, height2 = height;
		gtk_widget_style_get(widget,
				"interior-focus",  &interior_focus,
				"focus-line-width", &focus_width,
				"focus-padding", &focus_pad,  NULL);
#ifdef GTK3
		GtkBorder border;
		GtkStyleContext* context = gtk_widget_get_style_context(widget);
		gtk_style_context_get_border(context, GTK_STATE_FLAG_FOCUSED, &border);

		if (interior_focus)
        {
			x += border.left + focus_pad;
			y += border.top + focus_pad;
			width2 -= 2 * focus_pad + border.left + border.right;
			height2 -=  2 * focus_pad + border.top + border.bottom;
		}
		else
		{
			x -= focus_width + focus_pad;
			y -= focus_width + focus_pad;
			width2 += 2 * (focus_width + focus_pad);
			height2 += 2 * (focus_width + focus_pad);
		}

		gtk_render_focus(context, pixmap, x, y, width2, height2);
#else
		if (interior_focus)
		{
			const gint xthickness = gtk_widget_get_style(widget)->xthickness;
			const gint ythickness = gtk_widget_get_style(widget)->ythickness;
			x += xthickness + focus_pad;
			y += ythickness + focus_pad;

			width2 -= 2 * (xthickness + focus_pad);
			height2 -= 2 * (ythickness + focus_pad);
		}
		else
		{
			x -= focus_width + focus_pad;
			y -= focus_width + focus_pad;
			width2 += 2 * (focus_width + focus_pad);
			height2 += 2 * (focus_width + focus_pad);
		}

		gtk_paint_focus(style, pixmap, GetGtkState(state), &clip_rect,  widget, "button", x, y, width2, height2);
#endif // !GTK3
	}
}

void PushButton::ChangeDefaultSize(int& width, int& height, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkRequisition req;
	gtk_widget_size_request(m_widget, &req);

	GtkWidget* button_box = gtk_hbutton_box_new();

	gtk_widget_style_get(button_box,
			"child-min-width", &width,
			"child-min-height", &height, NULL);

	if (req.width > width)
		width = req.width;
	if (req.height > height)
		height = req.height;
}

void RadioButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkShadowType shadow = GTK_SHADOW_OUT;
	if (state & (STATE_SELECTED))
		shadow = GTK_SHADOW_IN;

#ifdef GTK3
	// this state mapping looks weird, but it works just as it should as far as I can tell
	// tested on Adwaita and Raleigh themes
	GtkStateType gtk_state = GTK_STATE_NORMAL;
	if (state & STATE_SELECTED)
		gtk_state = GTK_STATE_PRELIGHT;
	if (state & STATE_PRESSED)
		gtk_state = GTK_STATE_SELECTED;
	if (state & STATE_DISABLED)
		gtk_state = GTK_STATE_INSENSITIVE;

	gtk_paint_option(style, pixmap, gtk_state, shadow, widget, "radiobutton", 0, 0, width, height);
#else
	gint spacing;
	gtk_widget_style_get(widget, "indicator-spacing", &spacing, NULL);

	gtk_paint_option(style, pixmap, GetGtkState(state), shadow, &clip_rect, widget, "radiobutton", spacing, spacing, width - spacing * 2, height - spacing * 2);
#endif // !GTK3
}

void CheckBox::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkShadowType shadow = GTK_SHADOW_OUT;
	if (state & STATE_INDETERMINATE)
		shadow = GTK_SHADOW_ETCHED_IN;
	else if (state & STATE_SELECTED)
		shadow = GTK_SHADOW_IN;

#ifdef GTK3
	// this state mapping looks weird, but it works just as it should as far as I can tell
	// tested on Adwaita and Raleigh themes
	GtkStateType gtk_state = GTK_STATE_NORMAL;
	if (state & STATE_SELECTED)
		gtk_state = GTK_STATE_PRELIGHT;
	if (state & STATE_PRESSED)
		gtk_state = GTK_STATE_SELECTED;
	if (state & STATE_DISABLED)
		gtk_state = GTK_STATE_INSENSITIVE;

	gtk_paint_check(style, pixmap, gtk_state, shadow, widget, "checkbutton", 0, 0, width, height);
#else
	gint spacing;
	gtk_widget_style_get(widget, "indicator-spacing", &spacing, NULL);

	gtk_paint_check(style, pixmap, GetGtkState(state), shadow, &clip_rect, widget, "checkbutton", spacing, spacing, width - spacing * 2, height - spacing * 2);
#endif // !GTK3
}

void EditField::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	gboolean interior_focus;
	gint focus_line_width;
	gtk_widget_style_get(widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_line_width, NULL);

	if (state & STATE_DISABLED)
	{
		op_gtk_paint_flat_box(style, pixmap, GTK_STATE_INSENSITIVE, GTK_SHADOW_NONE, &clip_rect, widget, "entry", 0, 0, width, height);
	}

	bool focused = (state & STATE_SELECTED);

	if (focused)
	{
		gtk_widget_grab_focus(widget);
	}

	// This hack stolen from Mozilla/Qt, see https://bugzilla.mozilla.org/show_bug.cgi?id=405421
	g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));

	if (!interior_focus && focused)
	{
		op_gtk_paint_shadow(style, pixmap, GetGtkState(state), GTK_SHADOW_IN, &clip_rect, widget, "focus", focus_line_width, focus_line_width, width - focus_line_width * 2, height - focus_line_width * 2);
		op_gtk_paint_shadow(style, pixmap, GetGtkState(state), GTK_SHADOW_IN, &clip_rect, widget, "GtkEntryShadowIn", 0, 0, width, height);
	}
	else
	{
		op_gtk_paint_shadow(style, pixmap, GetGtkState(state), GTK_SHADOW_IN, &clip_rect, widget, focused ? "focus" : 0, 0, 0, width, height);
	}
}

void MultiLineEditField::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

 	GtkStyle* style = gtk_widget_get_style(m_widget);

	// we don't allow 0 padding, otherwise content may paint over frame
	// (unlike gtk, when frame will be drawn over content)
	int padding = style->xthickness > 0 ? style->xthickness : 1;
	left   = padding;
	top    = padding;
	right  = padding;
	bottom = padding;
}

void Label::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	GtkSkinElement::ChangeDefaultMargin(left, top, right, bottom, state);

	/* We override the right margin so that a distance of 12 pixels is created
	 * between labels and the element next to it, as specified in the gnome HIG
	 */
	right = 12;
}

void Browser::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_box(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_IN, &clip_rect, widget, NULL, 0, 0, width, height);
}

#ifndef GTK3
void Dialog::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_flat_box(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_NONE, &clip_rect, widget, "base", 0, 0, width, height);
}
#endif

void Dialog::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	// 12 pixels padding on all sides according to http://library.gnome.org/devel/hig-book/2.30/windows-alert.html.en#alert-spacing
	left = top = right = bottom = 12;
}

void DialogPage::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	left = top = right = bottom = 0;
}

void DialogButtonStrip::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	// Leaving 6 pixels on the left for compatibility with OpButtonStrip (will place 6 between buttons)
	left = 6;
	bottom = right = 0;
	// 12 pixels between dialog groups http://library.gnome.org/devel/hig-book/2.30/design-window.html.en
	top = 12;
}

void DialogTabPage::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_box_gap(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect, widget, "notebook", 0, -4, width, height + 4, GTK_POS_TOP, 0, width);
}

void DialogTabPage::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	left = right = top = bottom = 10;
}

void TabButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkStateType gtk_state = GTK_STATE_ACTIVE;
	if (state & STATE_DISABLED)
		gtk_state = GTK_STATE_INSENSITIVE;
	else if (state & (STATE_SELECTED | STATE_PRESSED))
#ifdef GTK3
	// looks like meaning of STATE_NORMAL changed in gtk3
		gtk_state = GTK_STATE_ACTIVE;
	else
#endif
		gtk_state = GTK_STATE_NORMAL;

	gint left_offset, right_offset, vertical_offset, gap_height; 
	left_offset=right_offset=20;
	vertical_offset=4;
	if (state & STATE_TAB_FIRST)
	 	left_offset=0;
	if (style->ythickness < 2)
		gap_height = 2; 
	else
		gap_height = style->ythickness;

	gboolean interior_focus = false;
	gint line_width = 1;
	gtk_widget_style_get(widget,
			"interior-focus", &interior_focus,
			"focus_line-width", &line_width, NULL);

	gint x=0, y=0;
#ifndef GTK3
	y = 1;
#endif
	if (state & STATE_SELECTED)
	{
		op_gtk_style_apply_default_background(style, pixmap, gtk_widget_get_parent_window(m_widget), TRUE, GTK_STATE_NORMAL, &clip_rect, line_width, height - vertical_offset, width-2*line_width, gap_height+5);
		op_gtk_paint_box_gap(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect, widget, "notebook", 0 - left_offset, height - vertical_offset, width + left_offset + right_offset, 3 * height, GTK_POS_TOP, left_offset, width);
		op_gtk_paint_extension(style, pixmap, gtk_state, GTK_SHADOW_OUT, &clip_rect, widget, "tab", x, y, width, height-vertical_offset, GTK_POS_BOTTOM);

#ifndef GTK3
		// Some themes draws a too heigh button border that crosses the TabSeparator (e.g. Epinox ant QtCurve) - simply erase
		if (line_width == 1)
		{
			if (! (state & STATE_TAB_FIRST))
				op_gtk_style_apply_default_background(style, pixmap, gtk_widget_get_parent_window(m_widget), TRUE, gtk_state, &clip_rect, style->xthickness-1, height+line_width-3-(style->ythickness-1), 2*line_width, 2*line_width );
			op_gtk_style_apply_default_background(style, pixmap, gtk_widget_get_parent_window(m_widget), TRUE, gtk_state, &clip_rect, width-style->xthickness-4, height+line_width-3-(style->ythickness-1),  2*line_width+2 , 2*line_width );
		}
#endif
	}
	else
	{
		op_gtk_paint_extension(style, pixmap, gtk_state, GTK_SHADOW_OUT, &clip_rect, widget, "tab", 0, 3, width, height-vertical_offset, GTK_POS_BOTTOM);
	}

	// ToDo: Enable the following code segment when we do support STATE_FOCUSED for tabbuttons
#if 0
	// Draw focus effect
	if (state & STATE_FOCUSED)
	{
		GdkRectangle focusrect = {0, 0, width, height};
		focusrect.x += (style->xthickness + 1);
		focusrect.width -= (style->xthickness * 2 + 2);
		focusrect.y += (style->ythickness + 3);
		focusrect.height -= style->ythickness * 2;
		focusrect.height -= (vertical_offset + 2);
		op_gtk_paint_focus(style, pixmap, GTK_STATE_NORMAL, &clip_rect, widget, "tab",	focusrect.x, focusrect.y, focusrect.width, focusrect.height);
	}
#endif
}

void TabButton::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	// Choose some reasonable values that look very much like GTK itself
	left = right = 6;
#ifdef GTK3
	left = right = 18;
#endif

	top = 9;
	bottom = 4;

	if (state & STATE_SELECTED)
	{
		top -= 1;
		bottom += 5;
	}
}

void TabButton::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	// Make the right overlap because tabs overlap in GTK
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkStyle* style = gtk_widget_get_style(m_widget);
	style = gtk_style_attach (style, gtk_widget_get_parent_window(m_widget));

	gint overlap = 2;
	gtk_widget_style_get(m_widget, "tab-overlap", &overlap, NULL);

	gtk_style_detach(style);

	right = -overlap;

	if (state & STATE_SELECTED)
	{
		top = -2;
		bottom = -4;
	}
}

void TabSeparator::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_box_gap(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect, widget, "notebook", 0, height -3, width, height * 2, GTK_POS_TOP,  10, 0); // this leaves a 1 - 2 pixel gap for many themes
	GdkRectangle clip_rect2 = {clip_rect.x + 5, clip_rect.y , 30, clip_rect.height};
	op_gtk_paint_box_gap(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect2, widget, "notebook", 0, height -3, width, height * 2, GTK_POS_TOP,  width - 20, 0); // and this will fill the gab above
}

void Menu::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_box(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect, widget, "menubar", 0, 0, width, height);
}

GtkWidget* MenuButton::CreateWidget()
{
	// Some themes' styling of menubuttons depend on whether or not they're inside a menubar (ex. rounded corners on menu buttons in Slickness theme)
	m_window = gtk_window_new(GTK_WINDOW_POPUP);
	GtkWidget* menu_bar = gtk_menu_bar_new();
	GtkWidget* item = gtk_menu_item_new_with_label("");
	gtk_container_add(GTK_CONTAINER(m_window), menu_bar);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item);

	return item;
}

#ifndef GTK3

GtkStateType MenuButton::GetGtkState(int state)
{
	// Fix for text colour.
	if (state & (STATE_PRESSED | STATE_SELECTED))
		return GTK_STATE_PRELIGHT;
	else
		return GTK_STATE_NORMAL;
}

void MenuButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkStateType gtk_state = GetGtkState(state);

	if (gtk_state == GTK_STATE_PRELIGHT)
	{
		GtkShadowType selected_shadow_type;
		gtk_widget_style_get(widget, "selected-shadow-type", &selected_shadow_type, NULL);
		gtk_paint_box(style, pixmap, gtk_state, selected_shadow_type, &clip_rect, widget, "menuitem", 0, 1, width, height-1);
	}
}

#endif // !GTK3

void MenuRightArrow::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	gfloat arrow_scaling = 0.8; // value similar to Raleigh theme

	gtk_widget_style_get(widget, "arrow-scaling", &arrow_scaling, NULL);

	gint scaled_width = width * arrow_scaling;
	gint scaled_height = height * arrow_scaling;

	GtkShadowType shadow;
	if (state & (STATE_HOVER | STATE_PRESSED | STATE_SELECTED))
		shadow = GTK_SHADOW_IN;
	else
		shadow = GTK_SHADOW_OUT;

	op_gtk_paint_arrow(style, pixmap, GetGtkState(state), shadow, &clip_rect, widget, "menuitem", (state & STATE_RTL) ? GTK_ARROW_LEFT : GTK_ARROW_RIGHT, TRUE, (width - scaled_width) / 2, (height - scaled_height) / 2, scaled_width, scaled_height);
}

#ifndef GTK3
void PopupMenu::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	gint x = 0, y = 0, width2 = width, height2 = height;
	op_gtk_paint_box(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect, widget, "base", x, y, width2, height2);
	op_gtk_paint_box(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, &clip_rect, widget, "menu", x, y, width2, height2);
}
#endif

GtkWidget* PopupMenuButton::CreateWidget()
{
      // Some themes' styling of menu items depends on whether or not they're inside a menu
      // (ex. no rounded corners on menu items in Clearlooks, background of checkmarks and bullets in menu items in Ambiance)
      m_menu = gtk_menu_new();
      GtkWidget* item = gtk_check_menu_item_new();
      gtk_menu_shell_append(GTK_MENU_SHELL(m_menu), item);
      return item;
}

void PopupMenuButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	if (state & STATE_DISABLED)
		return;

	if (state & STATE_HOVER)
	{
		op_gtk_paint_box(style, pixmap, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT, &clip_rect, widget, "menuitem", 0, 0, width, height);
    }

	gint indicator_size = 12; // value taken from Adwaita theme
	gtk_widget_style_get(widget, "indicator-size", &indicator_size, NULL);

	int image_adjust_x = MENU_LEFT_MARGIN;
	int image_adjust_y = 0;

	if (indicator_size < IMAGE_COLUMN_WIDTH)
		image_adjust_x += (IMAGE_COLUMN_WIDTH - indicator_size) / 2;
	if (indicator_size < height)
		image_adjust_y += (height - indicator_size) / 2;

	gint new_width = MIN(indicator_size, IMAGE_COLUMN_WIDTH);
	gint new_height = MIN(indicator_size, height);

	if (state & STATE_RTL)
		image_adjust_x = width - image_adjust_x - new_width;

	// Determines 'checked' state
	GtkShadowType shadow = GTK_SHADOW_IN;

	// state in is pressed and evt. hover
	GtkStateType type;
	if (state & STATE_HOVER)
		type = GTK_STATE_PRELIGHT;
	else
		type = GTK_STATE_NORMAL;


	if (state & STATE_PRESSED) // Checkmark
	{
		op_gtk_paint_check(style, pixmap, type, shadow, &clip_rect, widget, "check", image_adjust_x, image_adjust_y, new_width, new_height);
	}
	else if (state & STATE_SELECTED) // Bullet
	{
		op_gtk_paint_option(style, pixmap, type, shadow, &clip_rect, widget, "option", image_adjust_x, image_adjust_y, new_width, new_height);
	}
}

void ListItem::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	if (state & STATE_SELECTED)
	{
		op_gtk_paint_box(style, pixmap, GTK_STATE_PRELIGHT, GTK_SHADOW_OUT, &clip_rect, widget, "menuitem", 0, 0, width, height);
	}
}

void SliderTrack::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_box (style, pixmap, (state & STATE_DISABLED) ? GTK_STATE_INSENSITIVE : GTK_STATE_ACTIVE, GTK_SHADOW_IN, &clip_rect, widget, "trough", 0, 0, width, height);
}

void SliderKnob::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	op_gtk_paint_slider(style, pixmap, GetGtkState(state), GTK_SHADOW_OUT, &clip_rect, widget, "hscale", 0, 0, width, height, m_horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
}

void SliderKnob::ChangeDefaultSize(int& width, int& height, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkStyle* style = gtk_widget_get_style(m_widget);
	style = gtk_style_attach (style, gtk_widget_get_parent_window(m_widget));

	gint value = 14;
	gtk_widget_style_get(m_widget, "slider-width", &value, NULL);
	gtk_style_detach(style);

	if (m_horizontal)
		width = value;
	else
		height = value;
}

void HeaderButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(widget), 1);
	if (!column)
	{
		fprintf(stderr, "Column not found!\n");
		return;
	}

	GtkWidget* header = NULL;
#ifdef GTK3
	header = gtk_tree_view_column_get_widget(column);
#else
	header = column->button;
#endif // !GTK3

	GtkShadowType shadow = (state & STATE_PRESSED) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

	op_gtk_paint_box(gtk_widget_get_style(header), pixmap, GetGtkState(state), shadow, &clip_rect, header, "button", 0, 0, width, height);
}

GtkWidget* HeaderButton::CreateWidget()
{
	GtkWidget* treeview = gtk_tree_view_new();
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), gtk_tree_view_column_new());
	GtkTreeViewColumn* column = gtk_tree_view_column_new();
#ifdef GTK3
	gtk_tree_view_column_set_widget(column, gtk_label_new(""));
#endif
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), gtk_tree_view_column_new());

	return treeview;
}

#ifndef GTK3
void Toolbar::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	// we draw area bigger by 1px in every direction to work around uninitialized 1px "border"
	op_gtk_paint_flat_box(style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_NONE, &clip_rect, widget, "base", -1, -1, width+2, height+2);
}
#endif

void MainbarButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GtkShadowType shadow = (state & STATE_PRESSED) ? GTK_SHADOW_IN : GTK_SHADOW_OUT;

	if (state & (STATE_HOVER))
		op_gtk_paint_box(style, pixmap, GetGtkState(state), shadow, &clip_rect, widget, "button", 0, 0, width, height);
}

void MainbarButton::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	left   = 4; // values from the unix skin file - should probably use some metrics?
	top    = 3;
	right  = 5;
	bottom = 4;
}

void MainbarButton::ChangeDefaultSpacing(int& spacing, int state)
{
	spacing = 2; // value from the unix skin file
}

GtkWidget* Tooltip::CreateWidget()
{
	GtkWidget* window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_window_set_type_hint (GTK_WINDOW (window),
							  GDK_WINDOW_TYPE_HINT_TOOLTIP);
	gtk_widget_set_name (window, "gtk-tooltip");

	GtkWidget* alignment = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET(alignment));
	gtk_widget_show (alignment);

	GtkWidget* box = gtk_hbox_new (FALSE, 1);
	gtk_container_add (GTK_CONTAINER (alignment), GTK_WIDGET(box));
	gtk_widget_show (box);

	GtkWidget* label = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET(label),
						FALSE, FALSE, 0);
	return window;
}

void Tooltip::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	int x = 0, y = 0;
#ifdef GTK3
	// workaround for broken border around tooltip
	x -= 2;
	y -= 2;
	width += 4;
	height += 4;
#endif
	op_gtk_paint_flat_box (style, pixmap, GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, widget, "tooltip", x, y, width, height);
}

void MenuSeparator::ChangeDefaultSize(int& width, int& height, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

#ifdef GTK3
	GtkWidgetPath* path = gtk_widget_path_new();

	guint pos = gtk_widget_path_append_type(path, GTK_TYPE_MENU);
	gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_MENU);

	pos = gtk_widget_path_append_type(path, GTK_TYPE_SEPARATOR_MENU_ITEM);
	gtk_widget_path_iter_add_class(path, pos, GTK_STYLE_CLASS_SEPARATOR);

	GtkStyleContext* context = gtk_widget_get_style_context(m_widget);
	gtk_style_context_set_path(context, path);
#endif

	GtkStyle* style = gtk_widget_get_style(m_widget);
	style = gtk_style_attach(style, gtk_widget_get_parent_window(m_widget));

	height = 1 + 2 * style->ythickness; 

	// The separator-height property only takes effect if the wide-separators setting is true.
	gboolean wide_separators = false;
	gint separator_height = 0;
	gtk_widget_style_get(m_widget,
			"wide-separators", &wide_separators,
			"separator-height", &separator_height,
			NULL);
	gtk_style_detach(style);

	if (wide_separators)
	{
		height = separator_height + 2 * style->ythickness;
	}
}

#ifndef GTK3

void MenuSeparator::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	gboolean wide_separators = false;
	gint separator_height = 0;
	guint horizontal_padding = 0;

	gtk_widget_style_get (widget,
					  "wide-separators",    &wide_separators,
					  "separator-height",   &separator_height,
					  "horizontal-padding", &horizontal_padding,
					  NULL);

	const gint xthickness = gtk_widget_get_style(widget)->xthickness;
	const gint ythickness = gtk_widget_get_style(widget)->ythickness;
	if (wide_separators)
	{
		gtk_paint_box (style, pixmap,
				GTK_STATE_NORMAL, GTK_SHADOW_ETCHED_OUT,
				&clip_rect, widget, "hseparator",
				clip_rect.x + horizontal_padding + xthickness,
				clip_rect.y + (height - separator_height - ythickness) / 2,
				width - 2 * (horizontal_padding + xthickness),
				separator_height);
	}
	else
	{
		gtk_paint_hline (style, pixmap,
				GTK_STATE_NORMAL,
				&clip_rect, widget, "menuitem", 
				clip_rect.x + horizontal_padding + xthickness,  
				clip_rect.x + width - horizontal_padding - xthickness - 1, 
				clip_rect.y + (height - ythickness) / 2); 
	}
}

#endif // !GTK3

int GtkSkinElement::Round(double num)
{
	double fl = floor(num);
	double ce = ceil(num);
	if (fl == ce)
		return static_cast<int>(num);
	else if (num - fl < ce - num)
		return static_cast<int>(fl);
	else
		return static_cast<int>(ce);
}

#ifdef DEBUG
void RedElement::Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state)
{
	for (int i = 0; i < width * height; i++)
		*bitmap++ = 0xffff0000; // RED
}
#endif // DEBUG

