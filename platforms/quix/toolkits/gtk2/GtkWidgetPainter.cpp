/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "platforms/quix/toolkits/gtk2/GtkWidgetPainter.h"
#include <gtk/gtk.h>
#include <stdio.h>


class GtkWidgetPainter::Slider : public ToolkitSlider
{
public:
	Slider() : m_min(0), m_max(100), m_num_tickvalues(0), m_hover_part(NONE), m_horizontal(false), m_hover_knob(false), m_state(0), m_pos(0) {}

	virtual void SetValueAndRange(int value, int min, int max, int num_tick_values);
	virtual void SetOrientation(Orientation orientation);
	virtual void SetState(int state);
	virtual void SetHoverPart(HitPart part);
	virtual void GetKnobRect(int& x, int& y, int& width, int& height);
	virtual void GetTrackPosition(int& start_x, int& start_y, int& stop_x, int& stop_y);
	virtual void Draw(uint32_t* bitmap, int width, int height);

private:
	uint32_t GetARGB(guchar* pixel, uint8_t alpha = 0xff) { return (alpha << 24) | (pixel[0] << 16) | (pixel[1] << 8) | pixel[2]; }
	void ComputeRects(GtkWidget* widget, int width, int height, int focus_margin, gboolean lower_and_upper);

private:
	int m_min;
	int m_max;
	int m_num_tickvalues;
	HitPart m_hover_part;
	bool m_horizontal;
	bool m_hover_knob;
	int m_state;
	float m_pos;
	GdkRectangle m_focus_rect; // Outer most rect that the focus rectangle use
	GdkRectangle m_inner_rect; // All widget elements (except focus rectangle) will be inside this rectagle
	GdkRectangle m_lower_rect; // The area covering values lower than that represented by the knob.
	GdkRectangle m_upper_rect; // The area covering values higher than that represented by the knob.
	GdkRectangle m_knob_rect;  // The area used by the knob
};



ToolkitSlider* GtkWidgetPainter::CreateSlider()
{
	return new GtkWidgetPainter::Slider();
}


void GtkWidgetPainter::Slider::SetValueAndRange(int value, int min, int max, int num_tick_values)
{
	m_min = min;
	m_max = max;
	if (m_min > m_max)
		value = m_min = m_max;
	if (value < m_min)
		value = m_min;
	else if (value > m_max)
		value = m_max;

	m_num_tickvalues = num_tick_values;

	m_pos = m_max == m_min ? 0 : float(value - m_min) / (m_max - m_min);
}

void GtkWidgetPainter::Slider::SetOrientation(Orientation orientation)
{
	m_horizontal = orientation == HORIZONTAL;
}

void GtkWidgetPainter::Slider::SetState(int state)
{
	m_state = state;
}

void GtkWidgetPainter::Slider::SetHoverPart(HitPart part)
{
	m_hover_knob = part == KNOB;
}

void GtkWidgetPainter::Slider::GetKnobRect(int& x, int& y, int& width, int& height)
{
	x = m_knob_rect.x;
	y = m_knob_rect.y;
	width = m_knob_rect.width;
	height = m_knob_rect.height;
}

void GtkWidgetPainter::Slider::GetTrackPosition(int& start_x, int& start_y, int& stop_x, int& stop_y)
{
	if (m_horizontal)
	{
		start_x = m_inner_rect.x;
		start_y = m_inner_rect.y;
		stop_x  = m_inner_rect.width;
		stop_y  = m_inner_rect.y;
	}
	else
	{
		start_x = m_inner_rect.x;
		start_y = m_inner_rect.y + m_inner_rect.height;
		stop_x  = m_inner_rect.x;
		stop_y  = m_inner_rect.y;
	}
}

void GtkWidgetPainter::Slider::ComputeRects(GtkWidget* widget, int width, int height, int focus_margin, gboolean lower_and_upper)
{
	int center_x;
	int center_y;

	// Outermost rect 
	m_focus_rect.x = m_focus_rect.y = 0;
	m_focus_rect.width  = width;
	m_focus_rect.height = height;

	// Limit height or width. Some styles draw the slider grove to the full requested
	// height (horizontal) or width (vertical mode). Webpages can set all sorts of sizes
	// so we limit the size if necessary. The focus rectangle is not modified
	int x = 0;
	int y = 0;
	if (m_horizontal)
	{
		if (height > 20)
		{
			y = (height - 20) / 2;
			height = 20;
		}
	}
	else
	{
		if (width > 20)
		{
			x = (width - 20) / 2;
			width = 20;
		}
	}


	// Rect where the control can be drawn
	m_inner_rect.x = x + focus_margin;
	m_inner_rect.y = y + focus_margin;
	m_inner_rect.width = width - 2*focus_margin;
	m_inner_rect.height = height - 2*focus_margin;

	if (m_horizontal)
	{
		const float pos = (m_state & RTL) ? 1.0 - m_pos : m_pos;
		center_x = m_inner_rect.x + pos * m_inner_rect.width;
		if (lower_and_upper)
		{
			GdkRectangle& left_rect = (m_state & RTL) ? m_upper_rect : m_lower_rect;
			GdkRectangle& right_rect = (m_state & RTL) ? m_lower_rect : m_upper_rect;

			// The slider is drawn two times. One rectangle on each side of the knob
			left_rect.x = m_inner_rect.x;
			left_rect.y = m_inner_rect.y;
			left_rect.width  = center_x - m_inner_rect.x;
			left_rect.height = m_inner_rect.height;
			if (left_rect.width > m_inner_rect.width)
				left_rect.width = m_inner_rect.width;

			right_rect.x = left_rect.x + left_rect.width;
			right_rect.y = m_inner_rect.y;
			right_rect.width = m_inner_rect.width - left_rect.width;
			right_rect.height = m_inner_rect.height;
			if (right_rect.width < 0)
				right_rect.width = 0;

			// The reduction of left_rect.width is to prevent it becoming visible
			// on the right side of the knob (observed in ubuntu 11.04 (unity) std style)
			if (!(right_rect.width > 2 && right_rect.height > 2))
				left_rect.width -= 1;
		}
	}
	else
	{
		center_y = m_inner_rect.y + (int)((float)m_inner_rect.height*(1.0-m_pos));
		if (lower_and_upper)
		{
			m_lower_rect.x = m_inner_rect.x;
			m_lower_rect.y = center_y;
			m_lower_rect.width  = m_inner_rect.width;
			m_lower_rect.height = m_inner_rect.y + m_inner_rect.height - m_lower_rect.y;

			m_upper_rect.x = m_inner_rect.x;
			m_upper_rect.y = m_inner_rect.y;
			m_upper_rect.width  = m_inner_rect.width;
			m_upper_rect.height = m_inner_rect.height - m_lower_rect.height;
		}
	}

	// The height of the knob (fall back to default value for clearlooks)
	gint slider_width = -1;
	gtk_widget_style_get(widget, "slider-width", &slider_width, NULL);
	if (slider_width == -1)
		slider_width = 15;

	// The length of the knob (fall back to default value for clearlooks)
	gint slider_length = -1;
	gtk_widget_style_get(widget, "slider-length", &slider_length, NULL);
	if (slider_length == -1)
		slider_length = 23;

	if (m_horizontal)
	{
		m_knob_rect.x = center_x - slider_length/2;
		m_knob_rect.y = y + (height - slider_width + 1)/2; // Adjust down with one pixel to match native look
		m_knob_rect.width = slider_length;
		m_knob_rect.height = slider_width;
	}
	else
	{
		m_knob_rect.x = x + (width - slider_width)/2;
		m_knob_rect.y = center_y - slider_length/2;
		m_knob_rect.width = slider_width;
		m_knob_rect.height = slider_length;
	}

	if (m_knob_rect.x < m_inner_rect.x)
		m_knob_rect.x = m_inner_rect.x;
	if (m_knob_rect.x+m_knob_rect.width > m_inner_rect.x+m_inner_rect.width)
	{
		m_knob_rect.x = m_inner_rect.x+m_inner_rect.width-m_knob_rect.width;
		if (m_knob_rect.x < m_inner_rect.x)
		{
			m_knob_rect.x = m_inner_rect.x;
			if (m_knob_rect.width > m_inner_rect.width)
				m_knob_rect.width = m_inner_rect.width;
		}
	}

	if (m_knob_rect.y < m_inner_rect.y)
		m_knob_rect.y = m_inner_rect.y;
	if (m_knob_rect.y+m_knob_rect.height > m_inner_rect.y+m_inner_rect.height)
	{
		m_knob_rect.y = m_inner_rect.y+m_inner_rect.height-m_knob_rect.height;
		if (m_knob_rect.y < m_inner_rect.y)
		{
			m_knob_rect.y = m_inner_rect.y;
			if (m_knob_rect.height > m_inner_rect.height)
				m_knob_rect.height = m_inner_rect.height;
		}
	}

}


void GtkWidgetPainter::Slider::Draw(uint32_t* bitmap, int width, int height)
{
	GtkWidget* widget = m_horizontal ? 
		gtk_hscale_new_with_range((gdouble)m_min, (gdouble)m_max, 1.0) :
		gtk_vscale_new_with_range((gdouble)m_min, (gdouble)m_max, 1.0);
	if (!widget)
		return;

	GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_realize(window);
	GtkWidget* layout = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(window), layout);

	if (!gtk_widget_get_parent(widget))
	{
		gtk_container_add(GTK_CONTAINER(layout), widget);
	}

	gtk_widget_realize(widget);

	GdkPixmap* pixmap = gdk_pixmap_new(gtk_widget_get_parent_window(widget), width, height, -1);
	if (pixmap)
	{
		GtkStyle* style = widget->style;
		GdkRectangle clip_rect = {0, 0, width, height};

		GdkPixbuf* buf = gdk_pixbuf_new_from_data((guchar*)bitmap, GDK_COLORSPACE_RGB, true, 8,  width, height, width*4, NULL, NULL);
		if (buf)
		{
			gdk_pixbuf_render_to_drawable(buf, pixmap, style->black_gc, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NONE, 0, 0);
			g_object_unref(buf);
		}

		// Some skins draw the slider in two steps. Before (lower) and after (upper) the knob
		gboolean lower_and_upper = false;
		gtk_widget_style_get(widget, "trough-side-details", &lower_and_upper, NULL);

		ComputeRects(widget, width, height, 2, lower_and_upper);

		GtkStateType state = GTK_STATE_NORMAL;
		if (!(m_state & ENABLED))
			m_state = GTK_STATE_INSENSITIVE;

		// Draw focus before tick marks so that the tick mark get as visible as possible
		if (m_state & FOCUSED)
			gtk_paint_focus (style, pixmap, state, &clip_rect, widget, "trough", m_focus_rect.x, m_focus_rect.y, m_focus_rect.width, m_focus_rect.height);

		if (m_num_tickvalues > 1)
		{
			int margin = m_inner_rect.x;
			int base = 5;

			int m_tick_spacing;
			int m_tick_x;
			int m_tick_y;
			int m_tick_len;

			m_tick_spacing = ((m_horizontal ? m_inner_rect.width : m_inner_rect.height)) / (m_num_tickvalues-1);
			m_tick_x = m_horizontal ? margin : m_inner_rect.width-base;
			m_tick_y = m_horizontal ? m_inner_rect.height-base : m_inner_rect.height - margin;
			m_tick_len = 5;

			for (int i=0; i<m_num_tickvalues; i++)
			{
				if(m_horizontal)
				{
					gdk_draw_line (pixmap, style->dark_gc[state], m_tick_x, m_tick_y, m_tick_x, m_tick_y+m_tick_len);
					m_tick_x += m_tick_spacing;
				}
				else
				{
					gdk_draw_line (pixmap, style->dark_gc[state], m_tick_x, m_tick_y, m_tick_x+m_tick_len, m_tick_y);
					m_tick_y -= m_tick_spacing;
				}
			}
		}

		// The slider line
		// Some styles give painting artifacts if a box is drawn with width or height less or equal to 2
		if (lower_and_upper)
		{
			bool draw_lower = m_lower_rect.width > 2 && m_lower_rect.height > 2;
			bool draw_upper = m_upper_rect.width > 2 && m_upper_rect.height > 2;

			if (draw_lower)
				gtk_paint_box(style, pixmap, state, GTK_SHADOW_IN, &clip_rect, widget, "trough-lower", m_lower_rect.x, m_lower_rect.y, m_lower_rect.width, m_lower_rect.height);

			if (draw_upper)
				gtk_paint_box(style, pixmap, state, GTK_SHADOW_IN, &clip_rect, widget, "trough-upper", m_upper_rect.x, m_upper_rect.y, m_upper_rect.width, m_upper_rect.height);
		}
		else
		{
			if (m_inner_rect.width > 2 && m_inner_rect.height > 2)
				gtk_paint_box(style, pixmap, state, GTK_SHADOW_IN, &clip_rect, widget, "trough", m_inner_rect.x, m_inner_rect.y, m_inner_rect.width, m_inner_rect.height);
		}

		state = GTK_STATE_NORMAL;
		if (!(m_state & ENABLED))
			state = GTK_STATE_INSENSITIVE;
		else if(m_hover_knob)
			state = GTK_STATE_PRELIGHT;

		// The knob
		gtk_paint_slider(style, pixmap, state, GTK_SHADOW_OUT, &clip_rect, widget, m_horizontal ? "hscale" : "vscale", m_knob_rect.x, m_knob_rect.y, m_knob_rect.width, m_knob_rect.height, m_horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

		//gtk_style_detach(style);

		GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, width, height);
		if (pixbuf)
		{
			pixbuf = gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL, clip_rect.x, clip_rect.y, clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height);
		}
		g_object_unref(pixmap);

		guchar* data = gdk_pixbuf_get_pixels(pixbuf);
		for (int i = 0; i < width * height; i++)
		{
			bitmap[i] = GetARGB(data);
			data += 4;
		}
		g_object_unref(pixbuf);
	}


	gtk_widget_destroy(widget);
	gtk_widget_destroy(window);
}
