/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Implementations specific to windowless plugin instances.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include "common.h"
#include "instance.h"


/**
 * Windowless plugin constructor.
 *
 * @param instance NPP instance.
 */
WindowlessInstance::WindowlessInstance(NPP instance, const char* bgcolor)
	: PluginInstance(instance, bgcolor)
{
	/* Set windowless. */
	g_browser->setvalue(instance, NPPVpluginWindowBool, 0);
}


/**
 * Deliver a platform specific window event.
 *
 * @See https://developer.mozilla.org/en/NPP_HandleEvent.
 */
int16_t WindowlessInstance::HandleEvent(void* event)
{
	CountUse of(this);

#ifdef XP_WIN
	NPEvent* npevent = static_cast<NPEvent*>(event);

	switch (npevent->event)
	{
		case WM_WINDOWPOSCHANGED:
			SetProperty(scriptable_object, GetIdentifier("windowPosChangedCount"), &IntVariant(++windowposchanged_count));
			return 1;

		case WM_PAINT:
			if (NPRect32* rect = reinterpret_cast<NPRect32*>(npevent->lParam))
				return OnPaint(reinterpret_cast<HDC>(npevent->wParam), rect->left, rect->top,
					rect->right - rect->left, rect->bottom - rect->top);

		case WM_MOUSEMOVE:
			return OnMouseMotion(static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), MouseMove);

		case WM_MOUSEWHEEL:
			return OnMouseWheel(GET_WHEEL_DELTA_WPARAM(npevent->wParam), true, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)));

		case WM_MOUSEHWHEEL:
			return OnMouseWheel(GET_WHEEL_DELTA_WPARAM(npevent->wParam), false, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)));

		case WM_LBUTTONDOWN:
			return OnMouseButton(ButtonPress, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), 1);

		case WM_LBUTTONUP:
			return OnMouseButton(ButtonRelease, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), 1);

		case WM_RBUTTONDOWN:
			return OnMouseButton(ButtonPress, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), 2);

		case WM_RBUTTONUP:
			return OnMouseButton(ButtonRelease, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), 2);

		case WM_MBUTTONDOWN:
			return OnMouseButton(ButtonPress, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), 3);

		case WM_MBUTTONUP:
			return OnMouseButton(ButtonRelease, static_cast<int>(LOWORD(npevent->lParam)), static_cast<int>(HIWORD(npevent->lParam)), 3);

		case WM_KEYDOWN:
			return OnKey(KeyPress, static_cast<int>(npevent->wParam), 0, NULL, NULL);

		case WM_KEYUP:
			return OnKey(KeyRelease, static_cast<int>(npevent->wParam), 0, NULL, NULL);
	};
#endif // XP_WIN

#ifdef XP_UNIX
	return HandleXEvent(static_cast<XEvent*>(event));
#endif // XP_UNIX

#ifdef XP_MACOSX
# ifndef NP_NO_CARBON
	if (event_model == NPEventModelCarbon)
		return HandleCarbonEvent(static_cast<EventRecord*>(event));
# endif // !NP_NO_CARBON
	if (event_model == NPEventModelCocoa)
		return HandleCocoaEvent(static_cast<NPCocoaEvent*>(event));
#endif // XP_MACOSX

#ifdef XP_GOGI
	return HandleGOGIEvent(static_cast<GogiPluginEvent*>(event));
#endif

	return 0;
}


/**
 * Return the coordinates of the plugin window's origin
 * relative to the window in which it is embedded.
 *
 * @param[out] x X coordinate of origin.
 * @param[out] y Y coordinate of origin.
 *
 * @return True on success.
 */
bool WindowlessInstance::GetOriginRelativeToWindow(int& x, int& y)
{
	if (!plugin_window)
		return false;

	x = plugin_window->x;
	y = plugin_window->y;

	return true;
}


/**
 * Initialize plug-in instance.
 *
 * Called by NPP_New. Returning error will fail plug-in instantiation.
 *
 * @return True on success.
 */
bool WindowlessInstance::Initialize()
{
	CountUse of(this);

#ifdef XP_MACOSX
	NPBool value = false;

	/* Negotiate drawing model. */
	if (g_browser->getvalue(instance, NPNVsupportsCoreAnimationBool, &value) == NPERR_NO_ERROR && value)
	{
		drawing_model = NPDrawingModelCoreAnimation;
		InitializeCoreAnimationLayer(this);
	}
	else if (g_browser->getvalue(instance, NPNVsupportsCoreGraphicsBool, &value) == NPERR_NO_ERROR && value)
		drawing_model = NPDrawingModelCoreGraphics;
	else
		return false;

	/* Notify browser about chosen model. */
	if (g_browser->setvalue(instance, NPPVpluginDrawingModel, reinterpret_cast<uintptr_t*>(drawing_model)) != NPERR_NO_ERROR)
		return false;

	/* Negotiate event model. */
	if (g_browser->getvalue(instance, NPNVsupportsCocoaBool, &value) == NPERR_NO_ERROR && value)
		event_model = NPEventModelCocoa;
# ifndef NP_NO_CARBON
	else if (g_browser->getvalue(instance, NPNVsupportsCarbonBool, &value) == NPERR_NO_ERROR && value)
		event_model = NPEventModelCocoa;
# endif // !NP_NO_CARBON
	else
		return false;

	/* Notify browser about chosen model. */
	if (g_browser->setvalue(instance, NPPVpluginEventModel, reinterpret_cast<uintptr_t*>(event_model)) != NPERR_NO_ERROR)
		return false;
#endif // XP_MACOSX

	return true;
}
