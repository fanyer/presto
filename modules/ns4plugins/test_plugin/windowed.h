/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Windowed plugin instance representation.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#ifndef WINDOWED_INSTANCE_H
#define WINDOWED_INSTANCE_H

#include "common.h"


/**
 * Windowed plugin instance.
 */
class WindowedInstance : public PluginInstance
{
public:
	WindowedInstance(NPP instance, const char* bgcolor = 0);
	virtual ~WindowedInstance();

	/* Setup instance. Called during NPP_New. */
	virtual bool Initialize();

	virtual bool IsWindowless() { return false; }
	virtual NPError SetWindow(NPWindow* window);

#ifdef XP_UNIX
	NPError CreateGtkWidgets();
	gboolean HandleGtkEvent(GtkWidget* widget, GdkEvent* event);
#endif // XP_UNIX

#ifdef XP_WIN
	virtual LRESULT WinProc(HWND hwnd, UINT message, WPARAM w, LPARAM l);
#endif // XP_WIN

	virtual void SafeDelete();

protected:
	virtual bool GetOriginRelativeToWindow(int& x, int& y);
	virtual bool paint(NPObject* object, NPIdentifier name, const NPVariant* args, uint32_t argc, NPVariant* result);
	virtual void paint(int x = 0, int y = 0, int w = 0, int h = 0);

	virtual NPError RegisterWindowHandler();
	virtual void UnregisterWindowHandler();

#ifdef XP_UNIX
	virtual int GetEventMask() { return event_mask; }
#endif // XP_UNIX

#ifdef XP_WIN
	virtual HWND GetHWND();
#endif // XP_WIN

#ifdef XP_UNIX
	GtkWidget* container;
	GtkWidget* canvas;
	gulong m_gtk_event_handler_id;

	int event_mask;
#endif // XP_UNIX

#ifdef XP_WIN
	/* Plugin window data prior to our meddling. */
	LONG_PTR old_user_data;
	WNDPROC old_win_proc;
	HWND last_win_hwnd;

	bool left_button_down;
	bool right_button_down;
#endif // XP_WIN
};

#endif // WINDOWED_INSTANCE_H
