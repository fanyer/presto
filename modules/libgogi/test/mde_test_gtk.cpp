/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include <gtk/gtk.h>
#include <stdio.h>

#include "../mde.h"

#define IMAGE_WIDTH	 800
#define IMAGE_HEIGHT 600

void InitTest();
void ShutdownTest();
void PulseTest();

guchar rgbbuf[IMAGE_WIDTH * IMAGE_HEIGHT * 3];
guchar rgbabuf[IMAGE_WIDTH * IMAGE_HEIGHT * 4];

gboolean on_darea_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);

void CopyRgbaToRgb()
{
	int x, y;
	for (y = 0; y < IMAGE_HEIGHT; y++)
    {
		for (x = 0; x < IMAGE_WIDTH; x++)
		{
			rgbbuf[y * IMAGE_WIDTH * 3 + x * 3 + 2] = rgbabuf[y * IMAGE_WIDTH * 4 + x * 4];
			rgbbuf[y * IMAGE_WIDTH * 3 + x * 3 + 1] = rgbabuf[y * IMAGE_WIDTH * 4 + x * 4 + 1];
			rgbbuf[y * IMAGE_WIDTH * 3 + x * 3 + 0] = rgbabuf[y * IMAGE_WIDTH * 4 + x * 4 + 2];
		}
    }
}

class GtkMdeScreen : public MDE_Screen
{
public:
	GtkMdeScreen();
	~GtkMdeScreen() {}

	// == Inherit MDE_View ========================================
	virtual void OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen);

	int GetBitsPerPixel();
	void OutOfMemory();
	MDE_FORMAT GetFormat();
	MDE_BUFFER *LockBuffer();
	void UnlockBuffer(MDE_Region *update_region);

	GtkWidget* m_window;
private:
	MDE_BUFFER screen;
};

GtkMdeScreen::GtkMdeScreen()
{
	m_rect.w = IMAGE_WIDTH;
	m_rect.h = IMAGE_HEIGHT;
	Invalidate(m_rect);
}

void GtkMdeScreen::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	MDE_SetColor(MDE_RGB(69, 81, 120), screen);
	MDE_DrawRectFill(rect, screen);
}

int GtkMdeScreen::GetBitsPerPixel()
{
  return 32;
}

void GtkMdeScreen::OutOfMemory()
{
	printf("out of memory.\n");
}

MDE_FORMAT GtkMdeScreen::GetFormat()
{
  return MDE_FORMAT_BGRA32;
}

MDE_BUFFER *GtkMdeScreen::LockBuffer()
{
	MDE_InitializeBuffer(IMAGE_WIDTH, IMAGE_HEIGHT, MDE_FORMAT_BGRA32, rgbabuf, NULL, &screen);
	return &screen;
}

void GtkMdeScreen::UnlockBuffer(MDE_Region *update_region)
{
	CopyRgbaToRgb();

#define TRIVIAL_REDRAW
#ifdef TRIVIAL_REDRAW
	GdkRectangle r = {0, 0, IMAGE_WIDTH, IMAGE_HEIGHT};
	gdk_window_invalidate_rect(m_window->window, &r, true);
#else
//	GdkRegion* reg = gdk_region_new();
	for(int i = 0; i < update_region->num_rects; i++)
	{
		GdkRectangle r = {update_region->rects[i].x, update_region->rects[i].y, update_region->rects[i].w, update_region->rects[i].h};
		gdk_window_invalidate_rect(m_window->window, &r, true);
		// gdk_region_union_with_rect(reg, &r);
	}
//	gdk_window_invalidate_region(m_window->window, reg, true);
//	gdk_region_destroy(reg);
#endif // TRIVIAL_REDRAW
}

gboolean timeout(gpointer cdata)
{
	((GtkMdeScreen*)cdata)->Validate(true);
	PulseTest();
	return true;
}

static int convert_gtk_mouse_button_to_mde(int gtk_button)
{
	switch (gtk_button)
	{
	case 2: // middle button
		return 3;
	case 3: // right button
		return 2;
	case 1:
	default:
		return 1;
	}
}

gint mouse_evt_cb(GtkWidget *widget, GdkEvent  *event, gpointer cdata)
{
	GtkMdeScreen* win = (GtkMdeScreen*)cdata;
	switch(event->type)
	{
	case GDK_MOTION_NOTIFY:
	{
//		printf("GDK_MOTION_NOTIFY\n");
		int x, y;
		gtk_widget_get_pointer(widget, &x, &y);
		win->TrigMouseMove((int)event->motion.x, (int)event->motion.y);
		return TRUE;
	}
	case GDK_BUTTON_PRESS:
		printf("GDK_BUTTON_PRESS\n");
		win->TrigMouseDown((int)event->button.x, (int)event->button.y,
						   convert_gtk_mouse_button_to_mde(event->button.button), 1);
		return TRUE;
	case GDK_BUTTON_RELEASE:
		printf("GDK_BUTTON_RELEASE\n");
		win->TrigMouseUp((int)event->button.x, (int)event->button.y,
						 convert_gtk_mouse_button_to_mde(event->button.button));
		return TRUE;
	default:
		return FALSE;
	}
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *darea;
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  darea = gtk_drawing_area_new ();
  gtk_widget_set_size_request (darea, IMAGE_WIDTH, IMAGE_HEIGHT);
  gtk_container_add (GTK_CONTAINER (window), darea);
  gtk_widget_show_all (window);

  /* Set up the RGB buffer. */
  guchar* pos = rgbabuf;
  for (int y = 0; y < IMAGE_HEIGHT; y++)
  {
      for (int x = 0; x < IMAGE_WIDTH; x++)
	  {
		  *pos++ = 1;
		  *pos++ = 1;
		  *pos++ = 1;
		  *pos++;
	  }
  }

  CopyRgbaToRgb();

  GtkMdeScreen* screen = new GtkMdeScreen();

  screen->m_window = window;

  gtk_signal_connect (GTK_OBJECT (darea), "expose-event",
                      GTK_SIGNAL_FUNC (on_darea_expose), screen);

  g_signal_connect (GTK_OBJECT(window), "destroy", 
					G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_add_events(darea, GDK_ALL_EVENTS_MASK);

  g_signal_connect( GTK_OBJECT(darea), "motion-notify-event",
					G_CALLBACK(mouse_evt_cb), screen);
  
  g_signal_connect( GTK_OBJECT(darea), "button-press-event",
					G_CALLBACK (mouse_evt_cb), screen);

  g_signal_connect( GTK_OBJECT(darea), "button-release-event",
					G_CALLBACK (mouse_evt_cb), screen);

  MDE_Init(screen);
  InitTest();

#ifdef NAIVE_TESTS
  MDE_BUFFER* buffer = screen->LockBuffer();

  printf("Buffer: %p\n", buffer);

  MDE_SetColor(0x00ff00, buffer);
  MDE_RECT rect = MDE_MakeRect(50, 50, 100, 100);
  MDE_DrawRectFill(rect, buffer);

  MDE_SetColor(0x0000ff, buffer);
  MDE_DrawLine(150, 150, 190, 190, buffer);

  MDE_SetColor(0xff0000, buffer);
  MDE_DrawEllipseFill(rect, buffer);
  MDE_DrawRect(rect, buffer);

  buffer->method = MDE_METHOD_COLOR;

  screen->UnlockBuffer(NULL);
  
  CopyRgbaToRgb();
#endif // NAIVE_TESTS

  g_timeout_add(10, &timeout, screen);
  
  gtk_main ();

  

  ShutdownTest();
  MDE_Shutdown();

  return 0;
}

gboolean
on_darea_expose(GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	GdkGC* gc = widget->style->fg_gc[GTK_STATE_NORMAL];
#ifndef TRIVIAL_REDRAW
	gdk_gc_set_clip_origin(gc, event->area.x, event->area.y);
	gdk_gc_set_clip_rectangle(gc, &event->area);
#endif // TRIVIAL_REDRAW
	gdk_draw_rgb_image(widget->window, gc,
					   0, 0, IMAGE_WIDTH, IMAGE_HEIGHT,
					   GDK_RGB_DITHER_MAX, rgbbuf, IMAGE_WIDTH * 3);
	return TRUE;
}
