/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPPLUGINWINDOW_H__
#define OPPLUGINWINDOW_H__

#ifdef _PLUGIN_SUPPORT_

#include "modules/pi/OpWindow.h"
#include "modules/pi/OpKeys.h"
#include "modules/hardcore/keys/opkeys.h"

class OpView;
class MDE_Region;

/** @short Listener for OpPluginWindow - used to report input events back to core.
 *
 * An implementor must intercept mouse events to the plug-in, and not
 * send them to the plug-in if the argument of the previous call (if
 * any) to OpPluginWindow::BlockMouseInput() was FALSE. Note that the
 * appropriate method in this listener class must always be called
 * when a mouse event arrives, regardless of BlockMouseInput().
 */
class OpPluginWindowListener
{
public:
	virtual ~OpPluginWindowListener() { }

	/** A mouse button was pressed */
	virtual void OnMouseDown() = 0;

	/** A mouse button was released */
	virtual void OnMouseUp() = 0;

	/** The mouse pointer was moved (within the plug-in window) */
	virtual void OnMouseHover() = 0;

	/** Plugin has crashed. */
	virtual void OnCrash() = 0;
};

/** @short A platform-specific event.
 *
 * This may be a synthetic event created upon request from core, or a real
 * event delivered from the platform / windowing system.
 *
 * In principle, this is a container of an opaque data structure representing a
 * some sort of platform-specific event, which may be returned to core (for the
 * purpose of e.g. passing it on to a plug-in) by calling GetEventData().
 */
class OpPlatformEvent
{
public:
	virtual ~OpPlatformEvent() {}

	/** Get the platform specific event. */
	virtual void* GetEventData() const = 0;
};

/** Event type passed to plug-ins (and applets). */
enum OpPluginEventType
{
	PLUGIN_MOUSE_DOWN_EVENT,
	PLUGIN_MOUSE_UP_EVENT,
	PLUGIN_MOUSE_MOVE_EVENT,
	PLUGIN_MOUSE_ENTER_EVENT,
	PLUGIN_MOUSE_LEAVE_EVENT,
	PLUGIN_MOUSE_WHEELV_EVENT,
	PLUGIN_MOUSE_WHEELH_EVENT,
	PLUGIN_PAINT_EVENT,
	PLUGIN_PRINT_EVENT,
	PLUGIN_KEY_EVENT,
	PLUGIN_FOCUS_EVENT,
	PLUGIN_WINDOWPOSCHANGED_EVENT,
	PLUGIN_OTHER_EVENT
};

enum OpPluginKeyState
{
	PLUGIN_KEY_UP,
	PLUGIN_KEY_DOWN,
	PLUGIN_KEY_PRESSED
};

/** @short Plug-in window/widget and event generator.
 *
 * An object of this type represents a platform window and event generator,
 * which will be used by core to display and interact with plug-ins (and
 * applets).
 *
 * Some plug-ins are windowless, but an object of this type is nevertheless
 * created. The window manipulation methods (show/hide/resize/move) then do
 * nothing (but are also harmless to call). Only the methods that generate
 * events are then useful.
 *
 * For windowed plug-ins, a platform widget/window to represent the plug-in is
 * created as a child of an OpView.
 *
 * Coordinate parameters in methods are typically relative to the OpView passed
 * in Create().
 */
class OpPluginWindow
{
public:
#ifdef USE_PLUGIN_EVENT_API
	/** Create and set up a new plug-in window (widget).
	 *
	 * @param new_object (out) Set to the new OpPluginWindow created.
	 * @param rect Position and size of the plug-in window. Position
	 * is relative to the parent view
	 * @param scale The Opera zoom factor used. Platforms that trust
	 * their plug-ins to scale to their window size will ignore this
	 * parameter. However, platforms that know that the plug-ins don't
	 * scale to the specified size (as specified by the 'rect'
	 * parameter) on their own, but instead insist on keeping their
	 * intrinsic size (or the size specified by markup or CSS), may
	 * want to use this value to scale all plug-in paint operations,
	 * so that the plug-in content fills the entire plug-in window
	 * exactly. The platform obviously need to be capable of scaling
	 * paint operations for this to work. Example: plug-in size as
	 * specified by the 'rect' parameter has width=640 and
	 * height=480. The 'scale' parameter is 200. This means that the
	 * plug-in really wanted a size of 320x240, but by taking Opera
	 * zoom factor into account, its window size becomes 640x480. If
	 * the plug-in doesn't respect the window size (but insists on
	 * painting the plug-in in 320x200), its paint operations must be
	 * scaled to fit the entire window.
	 * @param parent The parent view of this OpPluginWindow
	 * @param windowless If TRUE, the implementation should not create any
	 * platform window handle for the plug-in, but it might still have to
	 * create some sort of overlay window to intercept input events (for
	 * OpPluginWindowListener).
	 * @param op_window The OpWindow of the document hosting the plug-in.
	 * Note that this is not necessarily the _top level_ window that
	 * contains the plug-in but this window can be used to obtain the
	 * top level window if necessary.
	 */
	static OP_STATUS Create(OpPluginWindow **new_object, const OpRect &rect, int scale, OpView* parent, BOOL windowless, OpWindow* op_window);
#else

#ifndef SUPPRESS_UPEA_REMOVAL_WARNING
# error "All non-USE_PLUGIN_EVENT_API blocks plus all ifdef's will be deleted (making USE_PLUGIN_EVENT_API be 'always on') in core 2.10 and onwards."
#endif // !SUPPRESS_UPEA_REMOVAL_WARNING

	static OP_STATUS Create(class OpPluginWindow **new_object);
#endif //  USE_PLUGIN_EVENT_API

	virtual ~OpPluginWindow() { }

	/** Set the listener for plug-in events.
	 *
	 * Listener methods are to be called when mouse events targeted
	 * for the plug-in window occur. See the OpPluginWindowListener
	 * documentation for more details.
	 *
	 * The listener can be detached using SetListener(NULL).
	 */
	virtual void SetListener(OpPluginWindowListener *listener) = 0;

	/** Make the plug-in window visible. */
	virtual void Show() = 0;

	/** Make the plug-in window hidden. */
	virtual void Hide() = 0;

	/** Set the position of the plug-in window.
	 *
	 * @param x X position, relative to parent OpView.
	 * @param y Y position, relative to parent OpView.
	 */
	virtual void SetPos(int x, int y) = 0;

	/** Set the size of the plug-in window.
	 *
	 * @param w Width
	 * @param h Height
	 */
	virtual void SetSize(int w, int h) = 0;

	/** Return the platform window handle for this plug-in window.
	 *
	 * The returned pointer will be passed to the plug-in library.
	 */
	virtual void *GetHandle() = 0;

	/** Set the state of mouse input blocking.
	 *
	 * Initial state is FALSE (no input blocking). Blocking is used
	 * for e.g. visual content block selection mode, and to avoid
	 * Eolas patent issues.
	 *
	 * @param block If TRUE, the plug-in should not receive any mouse
	 * events. If FALSE, the plug-in should receive and handle events
	 * as normally. Regardless of blocking state, the methods of
	 * OpPluginWindowListener (if one is set) are to be called.
	 */
	virtual void BlockMouseInput(BOOL block) = 0;

	/** Returns the state of mouse input
	 *
	 * @return TRUE if the plug-in is not receiving mouse events,
	 * otherwise FALSE
	 */
	virtual BOOL IsBlocked() = 0;

#ifdef USE_PLUGIN_EVENT_API
	/** Create a platform mouse event based on a core event.
	 *
	 * With windowless plug-ins the event should be broadcasted to the
	 * OpPluginListener in the platform implementation. This is because with
	 * windowless plug-ins the events will not be detected in the plug-in
	 * window (since there is none), but instead detected in OpView and passed
	 * through core.
	 *
	 * @param event (out) Platform mouse event.
	 * @param event_type Type of event (one of PLUGIN_MOUSE_DOWN_EVENT,
	 * PLUGIN_MOUSE_UP_EVENT, PLUGIN_MOUSE_MOVE_EVENT,
	 * PLUGIN_MOUSE_ENTER_EVENT, PLUGIN_MOUSE_LEAVE_EVENT.
	 * @param point Mouse pointer position. Relative to the upper left corner
	 * of the OpView for windowless plug-ins, and relative to the plug-in
	 * widget itself for windowed plug-ins.
	 * @param button Which button was pressed or released.
	 * @param modifiers Key modifiers held down.
	 *
	 * @return OpStatus::OK if event was created successfully and an error
	 * code otherwise. OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NO_ACCESS if
	 * plugin is blocked by activation overlay, OpStatus::ERR_NOT_SUPPORTED on
	 * unknown type of event.
	 */
	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** event, OpPluginEventType event_type, const OpPoint& point, int button, ShiftKeyState modifiers) = 0;

	/** Create a platform paint event.
	 *
	 * @param event (out) Platform paint event.
	 * @param painter The painter to use. Usually the same painter as the one
	 * associated with the document's OpView, but it will be different if the
	 * document is painted to e.g. an OpBitmap by core.
	 * @param rect Paint rectangle. For windowless plug-ins, the coordinates
	 * are relative to the upper left corner of the OpView. For windowed
	 * plug-ins it is relative to the plug-in window itself.
	 *
	 * @return OpStatus::OK if event was created successfully,
	 * OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR_NOT_SUPPORTED
	 * when paint rect is empty or OpStatus::ERR on other error.
	 */
	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event, class OpPainter* painter, const OpRect& rect) = 0;

	/** Create a platform key event.
	 *
	 * @param key_event (out) Platform key event.
	 * @param key The keycode of the virtual key pressed or released.
	 * @param key_value The character (string in the general case) produced by the key.
	 * @param key_event_data The extra event data for this event. May be used to
	 *        faithfully reconstruct the key event as a platform key event.
	 * @param key_state State of the key: PLUGIN_KEY_UP, PLUGIN_KEY_DOWN or PLUGIN_KEY_PRESSED.
	 * @param location The location of the key.
	 * @param modifiers Key modifiers held down.
	 *
	 * @return OpStatus::OK if event was created successfully,
	 * OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS CreateKeyEvent(OpPlatformEvent** key_event, OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, OpPluginKeyState key_state, OpKey::Location location, ShiftKeyState modifiers) = 0;

	/** Create a platform focus event.
	 *
	 * @param focus_event (out) Platform focus event
	 * @param focus_in TRUE if focus was given and FALSE if it was lost
	 *
	 * @return OpStatus::OK if event was created successfully,
	 * OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** focus_event, BOOL focus_in) = 0;

	/** Create a platform windowposchanged event.
	 *
	 * @param event (out) Platform window position changed event
	 *
	 * @return OpStatus::OK if event was created successfully,
	 * OpStatus::ERR_NO_MEMORY on OOM and OpStatus::ERR_NOT_SUPPORTED
	 * if platform does not support this kind of event.
	 */
	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** event) = 0;

	/** Convert coordinates from one coordinate space to another.
	 *
	 * Altough NPN_ConvertPoint is defined as part of the multiplatform NPAPI,
	 * it is actually very Mac-specific and so far only used there. Other
	 * platforms are free to return ERR_NOT_SUPPORTED status.
	 *
	 * @param source_x Input x coordinate.
	 * @param source_y Input y coordinate.
	 * @param source_space Input coordinate space (value from NPCoordinateSpace enum).
	 * @param dest_x (out) Output x coordinate.
	 * @param dest_y (out) Output y coordinate.
	 * @param dest_space Output coordinate space (value from NPCoordinateSpace enum).
	 *
	 * @retval OpStatus::OK on successful conversion.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if platform does not support conversion.
	 * @return See OpStatus for appropriate responses to other error cases.
	 */
	virtual OP_STATUS ConvertPoint(double source_x, double source_y, int source_space, double* dest_x, double* dest_y, int dest_space);

	/** Pop up a context menu.
	 *
	 * (Presently only relevant on Mac when using the Cocoa event module. Other platforms may freely return OpStatus::ERR_NOT_SUPPORTED.)
	 *
	 * @param menu (in) NPMenu* to the menu to pop up.
	 *
	 * @retval OpStatus::OK if the menu was successfully shown.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if the platform does not support this function.
	 * @return See OpStatus for appropriate responses to other error cases.
	 */
	virtual OP_STATUS PopUpContextMenu(void* menu);

	/** Send an event to the plug-in window.
	 *
	 * Only used for windowed plug-ins.
	 *
	 * @param event The event to handle.
	 *
	 * @return TRUE if the event was handled, FALSE otherwise.
	 */
	virtual BOOL SendEvent(OpPlatformEvent* event) = 0;

	/** Ask platform whether it supports direct painting.
	 * The alternative is to create events and pass them back to core.
	 *
	 * @return TRUE if platform wants to paint directly, FALSE otherwise.
	 * Returning TRUE will result in a call to method "PaintDirectly" instead
	 * of SaveState/CreatePaintEvent/RestoreState combo.
	 */
	virtual BOOL UsesDirectPaint() const = 0;

	/** Paint directly, without having to call back core to handle painting.
	 *
	 * @param rect Update rectangle, relative to parent OpView.
	 *
	 * @return OpStatus::OK if painting performed successfully. Error status
	 * on OOM or other error.
	 */
	virtual OP_STATUS PaintDirectly(const OpRect& rect) = 0;

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW
	/** Must events be delivered via a native window ?
	 *
	 * Windowless plugins may need core to deliver all move/resize/show
	 * events via a native plugin window.  For such plugins, this method
	 * should return TRUE.
	 *
	 * @return TRUE to use the plugin native window, otherwise FALSE.
	 */
	virtual BOOL UsesPluginNativeWindow() const = 0;
#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW

#ifdef NS4P_SILVERLIGHT_WORKAROUND
	/** Request windowed plugin window invalidation.
	 *
	 * Silverlight plugin on Windows requires plugin window to be invalidated
	 * by browser.
	 * Triggered only for plugins running in windowed mode on a call to
	 * NPN_InvalidateRect.
	 *
	 * @param rect Update rectangle relative to plugin's own window.
	 */
	virtual void InvalidateWindowed(const OpRect& rect) = 0;
#endif // NS4P_SILVERLIGHT_WORKAROUND

	/** Pass plugin instance to platform.
	 *
	 * Platform in many cases has to call methods of Plugin class directly.
	 * Otherwise it would be very hard to handle some use cases with current API.
	 *
	 * @param plugin Pointer to 'this' plugin instance.
	 */
	virtual void SetPluginObject(Plugin* plugin) = 0;

	/** Detach the window.
	 *
	 * Prepare the window for destruction (but don't destroy it yet). Due to
	 * the asynchronous nature of plug-in code, the parent OpView of the
	 * plug-in window may have to be destroyed before the plug-in window
	 * itself. And, in most windowing systems, it is wrong to delete a parent
	 * before all its children have been deleted. Therefore, a platform
	 * implementation will typically hide the window and reparent it to the
	 * root window (and thus make it a hidden top-level window) here.
	 *
	 * The only useful thing to do with this plug-in window after this method
	 * has been called, is to delete it; however, any method may be safely
	 * called (and typically have no effect).
	 */
	virtual void Detach() = 0;

	/** Check if calling NPP_HandleEvent() with paint event is necessary
	 *
	 * Calling this method before an OpPluginWindow has been set is pointless.
	 * It is platform dependant how painting is done.
	 * One paint event is the normal case.
	 * Wingogi return three values: throttle (0), opaque (1) and transparent (2).
	 *
	 * @return number of paint events to submit to the plugin.
	 */
	virtual unsigned int CheckPaintEvent() = 0;

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW
	/** Return the parent OpView
	 *
	 * Returns the parent view passed in during creation. Used as the attachment 
	 * point for the plugin native window, if used.
	 *
	 * @return OpView of the parent view
	 */
	virtual OpView *GetParentView() = 0;

	/** Pass clip region to the platform
	 *
	 * The plugin native window will receive the SetClipRegion call and we need to
	 * pass this to the platform so it can adjust any clipping rects it may
	 * be using.
	 */
	virtual void SetClipRegion(MDE_Region* rgn) = 0;
#endif // NS4P_USE_PLUGIN_NATIVE_WINDOW
#endif // USE_PLUGIN_EVENT_API
};

#ifdef USE_PLUGIN_EVENT_API
inline OP_STATUS OpPluginWindow::ConvertPoint(double, double, int, double*, double*, int)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

inline OP_STATUS OpPluginWindow::PopUpContextMenu(void*)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}
#endif // USE_PLUGIN_EVENT_API

#endif // _PLUGIN_SUPPORT_

#endif //  OPPLUGINWINDOW_H__
