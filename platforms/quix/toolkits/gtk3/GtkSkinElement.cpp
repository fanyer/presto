/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

// keep this include last; include all implementation shared with gtk2
#include "platforms/quix/toolkits/gtk2/GtkSkinElement.cpp"

void GtkSkinElement::Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	gtk_widget_set_direction(m_widget, (state & STATE_RTL) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);

	GtkStyle* style = gtk_widget_get_style(m_widget);

	style = gtk_style_attach(style, GetGdkWindow());

	CairoDraw(bitmap, width, height, style, state);

	gtk_style_detach(style);
}

void GtkSkinElement::CairoDraw(uint32_t* bitmap, int width, int height, GtkStyle* style, int state)
{
	// According to cairo manual stride should be counted before allocating
	// bitmap, but this is oversimplification; stride is convenience value
	// to make it easier to malloc bitmap of correct size.
	// since we use NEWA instead of malloc, we don't need to worry about
	// such things.
	const int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

	// If cairo decided that it needs even more memory, we would rather
	// fail to draw than overflow.
	if (stride < 0 || unsigned(stride) > width * sizeof(uint32_t))
		return;

	cairo_surface_t* surface = cairo_image_surface_create_for_data(
			reinterpret_cast<unsigned char*>(bitmap), CAIRO_FORMAT_ARGB32,
			width, height, stride);

	cairo_status_t status = cairo_surface_status(surface);
	if (status != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy(surface);
		return;
	}

	cairo_t *cr = cairo_create(surface);

	if (RespectAlpha())
	{
		cairo_save(cr);
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		cairo_paint(cr);
		cairo_restore(cr);
	}
	else
	{
		cairo_set_source_rgb(cr, 1.0 , 1.0, 1.0);
		cairo_rectangle(cr, 0, 0, double(width), double(height));
		cairo_fill(cr);
	}

	GdkRectangle dummy;
	GtkDraw(cr, width, height, dummy, m_widget, style, state);

	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

GtkStateFlags GtkSkinElement::GetGtkStateFlags(int state)
{
	if (state & STATE_DISABLED)
		return GTK_STATE_FLAG_INSENSITIVE;
	if (state & STATE_PRESSED)
		return GTK_STATE_FLAG_ACTIVE;
	if (state & STATE_HOVER)
		return GTK_STATE_FLAG_PRELIGHT;
	if (state & STATE_SELECTED)
		return GTK_STATE_FLAG_SELECTED;

	// there are 2 more flags:
	// GTK_STATE_FLAG_INCONSISTENT
	// GTK_STATE_FLAG_FOCUSED

	return GTK_STATE_FLAG_NORMAL;
}

void ScrollbarDirection::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle&, GtkWidget* widget, GtkStyle* style, int state)
{
	gint x = 0, y = 0, width2 = width, height2 = height;

	GtkStyleContext* context = gtk_widget_get_style_context(widget);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_BUTTON);
	gtk_style_context_set_state(context, GetGtkStateFlags(state & ~STATE_PRESSED));

	gint border = 0;
	gfloat arrow_scaling = 0.444; // value taken from Adwaita theme
	gtk_widget_style_get(m_widget,
			"trough-border", &border,
			"arrow-scaling", &arrow_scaling,
			NULL);

	// adjust size and position to simulate border in scrollbar background
	//
	// ChangeDefaultSize does not work for this element :(
	// ChangeDefaultPadding does not work for ScrollbarBackground
	// in result we have additional border between knob (slider)
	// and direction (arrow button) :(
	// see DSK-343917
	x += border;
	y += border;
	width2 -= 2 * border;
	height2 -= 2 * border;

	gtk_render_background(context, pixmap, x, y, width2, height2);
	gtk_render_frame(context, pixmap, x, y, width2, height2);

	gfloat angle;
	switch (GetArrow())
	{
		case GTK_ARROW_RIGHT:
			angle = OP_PI / 2;
			break;
		case GTK_ARROW_DOWN:
			angle = OP_PI;
			break;
		case GTK_ARROW_LEFT:
			angle = 3 * (OP_PI / 2);
			break;
		case GTK_ARROW_UP:
		default:
			angle = 0;
			break;
	}
	gfloat arrow_size = (width2 < height2 ? width2 : height2) * arrow_scaling;
	gfloat arrow_x = x + (width2 - arrow_size) / 2;
	gfloat arrow_y = y + (height2 - arrow_size) / 2;

	gtk_render_arrow(context, pixmap, angle, arrow_x, arrow_y, arrow_size);
}

void GtkSkinElement::ChangeTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state)
{
	if (!m_widget && !CreateInternalWidget())
		return;

	GtkWidget* text_widget = GetTextWidget();
	GtkWidget* widget = text_widget ? text_widget : m_widget;

	GdkRGBA color;
	GtkStyleContext* context = gtk_widget_get_style_context(widget);

	if (GetStyleContextClassName())
		gtk_style_context_add_class(context, GetStyleContextClassName());

	gtk_style_context_get_color(context, GetGtkStateFlags(state), &color);

	red   = Round(0xff * color.red);
	green = Round(0xff * color.green);
	blue  = Round(0xff * color.blue);
	alpha = Round(0xff * color.alpha);
}

GtkStateFlags MenuButton::GetGtkStateFlags(int state)
{
	// Fix for text colour.
	//
	// MenuButton is menuitem, but has special state mapping inside gtk3.
	// That's because interaction with menubar items does not map
	// naturally into usual states.
	// :prelight is used for skinning of "selected" menu button
	if (state & (STATE_PRESSED | STATE_SELECTED))
		return GTK_STATE_FLAG_PRELIGHT;
	else
		return GTK_STATE_FLAG_NORMAL;
}

void MenuButton::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle&, GtkWidget* widget, GtkStyle*, int state)
{
	GtkStateFlags gtk_state = GetGtkStateFlags(state);

	if (gtk_state == GTK_STATE_FLAG_PRELIGHT)
	{
		GtkStyleContext* context = gtk_widget_get_style_context(widget);
		gtk_style_context_set_state(context, gtk_state);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_MENUBAR);
		gtk_render_background(context, pixmap, 0, 0, width, height);
		gtk_render_frame(context, pixmap, 0, 0, width, height);
	}
}

void MenuSeparator::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	gboolean wide_separators = false;
	gint separator_height = 0;
	guint horizontal_padding = 0;

	GtkBorder padding;
	GtkStyleContext* context = gtk_widget_get_style_context(widget);

	gtk_widget_style_get(widget,
					  "wide-separators",    &wide_separators,
					  "separator-height",   &separator_height,
					  "horizontal-padding", &horizontal_padding,
					  NULL);

	gtk_style_context_get_padding(context, GTK_STATE_FLAG_NORMAL, &padding);

	if (wide_separators)
	{
		gtk_render_frame(context, pixmap,
				horizontal_padding + padding.left,
				(height - separator_height - padding.top) / 2,
				width - (2 * horizontal_padding) - padding.left - padding.right,
				separator_height);
	}
	else
	{
		gtk_render_line(context, pixmap,
				horizontal_padding + padding.left,
				(height - padding.top) / 2,
				width - horizontal_padding - padding.right - 1,
				(height - padding.top) / 2);
	}
}

void Toolbar::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GdkColor color = style->bg[GTK_STATE_NORMAL];
	gdouble red = color.red / 65535.0;
	gdouble green = color.green / 65535.0;
	gdouble blue = color.blue / 65535.0;

	// GdkRGBA rgba; // Do not work for some themes
	// gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &rgba);

	cairo_set_source_rgb(pixmap, red, green, blue);
	cairo_rectangle(pixmap, 0.0, 0.0, double(width), double(height));
	cairo_fill(pixmap);
}

void PopupMenu::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GdkColor color = style->bg[GTK_STATE_NORMAL];
	gdouble red = color.red / 65535.0;
	gdouble green = color.green / 65535.0;
	gdouble blue = color.blue / 65535.0;

	cairo_set_source_rgb(pixmap, red, green, blue);
	cairo_rectangle(pixmap, 0.0, 0.0, double(width), double(height));
	cairo_fill(pixmap);

	GtkStateFlags gtk_state = GetGtkStateFlags(state);
	GtkStyleContext* context = gtk_widget_get_style_context(widget);
	gtk_style_context_set_state(context, gtk_state);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_MENU);
	gtk_render_background(context, pixmap, 0, 0, width, height);
	gtk_render_frame(context, pixmap, 0, 0, width, height);
}

void Dialog::GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state)
{
	GdkColor color = style->bg[GTK_STATE_NORMAL];
	gdouble red = color.red / 65535.0;
	gdouble green = color.green / 65535.0;
	gdouble blue = color.blue / 65535.0;

	cairo_set_source_rgb(pixmap, red, green, blue);
	cairo_rectangle(pixmap, 0.0, 0.0, double(width), double(height));
	cairo_fill(pixmap);
}
