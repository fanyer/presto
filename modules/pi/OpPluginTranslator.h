/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPPLUGINTRANSLATOR_H__
#define OPPLUGINTRANSLATOR_H__

#if defined(_PLUGIN_SUPPORT_) && defined(NS4P_COMPONENT_PLUGINS)

#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/pi/OpPluginImage.h"

/**
 * An interface used by core to communicate with plug-ins using NPAPI.
 *
 * The primary purpose is to encode data in platform-specific NPAPI structures.
 *
 * Unless otherwise stated, all coordinates referred to in the documentation of
 * this class are rendering coordinates. Scrolling offsets of individual OpViews
 * are not taken into account - they are merely ignored. Additionally, the use
 * of the term "frame" refers to a single graphic in the sequence making up an
 * animation, not an HTML element of the same name.
 *
 * See graph below to understand the terms used for describing coordinates of
 * the rectangles and points used in various methods.
 *
 * ┌────────────────────────────────────────┐
 * │ (browser window)                       │
 * │    ┌──────────┐                        │
 * │    │   Tab 1  │                        │
 * │   ┌┴──────────┴────────────────────────┤
 * │   │ (topmost OpView)                   │
 * │   │                                    │
 * │   │    ┌───────────────────────────────┤
 * │   │    │ (parent OpView - iframe)      │
 * │   │    │                               │
 * │   │    │                               │
 * │   │    │           ┌───────────────┐   │
 * │   │    │           │ (plugin)      │   │
 * │   │    │           │               │   │
 * │   │    │           │               │   │
 * │   │    │           │               │   │
 * │   │    │           │               │   │
 * │   │    │           └───────────────┘   │
 * └───┴────┴───────────────────────────────┘
 *
 */
class OpPluginTranslator
{
public:
	/**
	 * Create an instance of this interface.
	 *
	 * @param[out] translator  The translator object returned.
	 * @param      plugin      Message address of the PluginComponentInstance managing the plug-in instance.
	 *                         Interaction with the plug-in (such as sending events) should be handled
	 *                         by sending messages to this address.
	 * @param      adapter     Message address to listener returned by OpNS4PluginAdapter::GetListener()
	 *                         or NULL if no adapter listener is available. See
	 *                         OpNs4PluginAdapter::GetListener().
	 * @param      npp         Plug-in instance identifier, forwardable to NPN functions; see BrowserFunctions
	 *                         (in modules/ns4plugins/component/browser_functions.h). Additionally, its ndata
	 *                         member is a pointer to the associated PluginComponentInstance objects used to
	 *                         access the plug-in library's NPP functions.
	 */
	static OP_STATUS Create(OpPluginTranslator** translator, const OpMessageAddress& plugin, const OpMessageAddress* adapter, const NPP npp);

	virtual ~OpPluginTranslator() {}

	/**
	 * Finalize the rendered frame for painting in the browser process.
	 *
	 * Platforms that do not require final processing of the OpPluginImage
	 * should return OpStatus::OK without doing anything.
	 *
	 * @param image      Global identifier of the plug-in image that should be finalized.
	 * @param np_window  The plug-in window whose graphical context was used to draw the image.
	 */
	virtual OP_STATUS FinalizeOpPluginImage(OpPluginImageID image, const NPWindow& np_window) = 0;

	/**
	 * Update a platform-specific NPWindow structure.
	 *
	 * @param[out] npwin      NPWindow structure that will be passed to the
	 *                        plug-in's NPP_SetWindow.
	 * @param      rect       Rectangle of plug-in window. Coordinates are
	 *                        relative to the upper left corner of the browser
	 *                        window. Dimensions (width and height) signify
	 *                        the size of the plug-in.
	 * @param      clip_rect  Clip rectangle of plug-in window, relative to the
	 *                        top left corner of \p rect.
	 * @param      type       Type of window. See NPWindowType on Mozilla
	 *                        Developer Network: Drawing and Event Handling.
	 *
	 * @return See OpStatus.
	 */
	virtual OP_STATUS UpdateNPWindow(NPWindow& npwin, const OpRect& rect, const OpRect& clip_rect, NPWindowType type) = 0;

	/**
	 * Create a platform focus event.
	 *
	 * Depending on the configuration of the plug-in instance associated with this
	 * OpPluginTranslator instance, this method may or may not be called.
	 *
	 * The returned platform-specific event is delivered to the plug-in instance's
	 * NPP_HandleEvent() implementation.
	 *
	 * @param[out] event      Platform focus event.
	 * @param      got_focus  True if focus was given and false if it was lost.
	 *
	 * @return OpStatus::OK if event was created successfully, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS CreateFocusEvent(OpPlatformEvent** event, bool got_focus) = 0;

	/**
	 * Create a platform key event sequence.
	 *
	 * Depending on the configuration of the plug-in instance associated with this
	 * OpPluginTranslator instance, this method may or may not be called.
	 *
	 * The returned platform-specific event is delivered to the plug-in instance's
	 * NPP_HandleEvent() implementation.
	 *
	 * @param[out] events     Key-event sequence. Platforms should append key
	 *                        events to it; most should only need to append one.
	 *                        On success, the caller will assume ownership of
	 *                        all returned events and release them with
	 *                        OP_DELETE() after use. If the function returns
	 *                        unsuccessful for any reason, it is the
	 *                        implementation's responsibility to undo any
	 *                        changes made to the list.
	 * @param      key        The virtual key code of the key that was pressed or released.
	 * @param      key_value  The character value of the key, 0 if none.
	 * @param      key_state  State of the key. PLUGIN_KEY_UP, PLUGIN_KEY_DOWN or PLUGIN_KEY_PRESSED
	 * @param      modifiers  Key modifiers held down.
	 * @param      data1      Platform specific data, as returned by OpPluginAdapter::GetKeyEventPlatformData().
	 * @param      data2      Platform specific data, as returned by OpPluginAdapter::GetKeyEventPlatformData().
	 *
	 * @return OpStatus::OK if event sequence was created successfully, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS CreateKeyEventSequence(OtlList<OpPlatformEvent*>& events, OpKey::Code key, uni_char value, OpPluginKeyState key_state, ShiftKeyState modifiers, UINT64 data1, UINT64 data2) = 0;

	/**
	 * Create a platform mouse event based on a core event.
	 *
	 * Depending on the configuration of the plug-in instance associated with this
	 * OpPluginTranslator instance, this method may or may not be called.
	 *
	 * The returned platform-specific event is delivered to the plug-in instance's
	 * NPP_HandleEvent() implementation.
	 *
	 * @param[out] event       Platform mouse event.
	 * @param      event_type  Type of event (one of PLUGIN_MOUSE_DOWN_EVENT,
	 *                         PLUGIN_MOUSE_UP_EVENT, PLUGIN_MOUSE_MOVE_EVENT,
	 *                         PLUGIN_MOUSE_ENTER_EVENT, PLUGIN_MOUSE_LEAVE_EVENT).
	 * @param      point       Mouse pointer position. Relative to the top-left
	 *                         corner of the plug-in widget.
	 * @param      button      Which button was pressed or released
	 * @param      modifiers   Key modifiers held down
	 *
	 * @return See Opstatus.
	 * @retval OpStatus::OK if the event was created successfully.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 * @retval OpStatus::ERR_NO_ACCESS if the plug-in is blocked by an activation overlay.
	 * @retval OpStatus::ERR_NOT_SUPPORTED on unknown event_type.
	 */
	virtual OP_STATUS CreateMouseEvent(OpPlatformEvent** event,
	                                   OpPluginEventType event_type,
	                                   const OpPoint& point,
	                                   int button,
	                                   ShiftKeyState modifiers) = 0;

	/**
	 * Create a platform paint event for windowless plug-ins.
	 *
	 * Depending on the configuration of the plug-in instance associated with this
	 * OpPluginTranslator instance, this method may or may not be called.
	 *
	 * The returned platform-specific event is delivered to the plug-in instance's
	 * NPP_HandleEvent() implementation.
	 *
	 * @param[out] event  Platform paint event.
	 * @param dest        Destination image.
	 * @param bg          Background for destination image. If window mode is
	 *                    opaque, this parameter *must* be zero. On platforms
	 *                    where 32-bit painting is available, bg can safely be
	 *                    ignored.
	 * @param paint_rect  Paint rectangle. Coordinates are relative to the top
	 *                    left corner of the plug-in rectangle. Guaranteed to
	 *                    be non-empty.
	 * @param npwin       Pointer to the NPAPI NPWindow, the platform may choose
	 *                    to modify this struct, and if it does it must also set
	 *                    *npwindow_was_modified = true (doing so will cause
	 *                    core to make an extra call to NPP_SetWindow() using
	 *                    this modified NPWindow before the call to
	 *                    NPP_HandleEvent()).
	 * @param[out] npwindow_was_modified Tells core whether npwin was modified by the platform.
	 * @return See OpStatus; OK if event was created successfully.
	 */
	virtual OP_STATUS CreatePaintEvent(OpPlatformEvent** event, OpPluginImageID dest, OpPluginImageID bg, const OpRect& paint_rect, NPWindow* npwin, bool* npwindow_was_modified) = 0;

	/**
	 * Create a platform window-moved event.
	 *
	 * @note This event exists because on MS-Windows plug-ins rely on
	 *       a platform-specific WM_WINDOWPOSCHANGED event.
	 *       Other platforms can simply return OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param[out] event  Platform window-moved event.
	 *
	 * @return See OpStatus.
	 * @retval OpStatus::OK if event was created successfully,
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if platform does not support this kind of event.
	 */
	virtual OP_STATUS CreateWindowPosChangedEvent(OpPlatformEvent** event) = 0;

	/**
	 * Handle NPN_GetValue request by plug-in.
	 *
	 * This method will be called first when a plug-in requests a value. The
	 * implementation should return false for any unknown variables, whereupon Core
	 * will handle it if recognized. If this implementation returns true, processing
	 * of this value is assumed to have been completed.
	 *
	 * @param      variable   The variable to get.
	 * @param[out] ret_value  The variable-specific value destination.
	 * @param[out] result     The return value of the NPN_GetValue call when handled.
	 *
	 * @return \c true if Core should do no further processing of this variable.
	 */
	virtual bool GetValue(NPNVariable variable, void* ret_value, NPError* result) = 0;

	/**
	 * Static version of GetValue that may be called without a plug-in instance
	 * in order to get global variables.
	 *
	 * @param      variable   The variable to get.
	 * @param[out] ret_value  The variable-specific value destination.
	 * @param[out] result     The return value of the NPN_GetValue call when handled.
	 *
	 * @return \c true if Core should do no further processing of this variable.
	 */
	static bool GetGlobalValue(NPNVariable variable, void* ret_value, NPError* result);

	/**
	 * Handle NPN_SetValue request by plug-in.
	 *
	 * This method will be called first when a plug-in requests a value be set. The
	 * implementation should return false for any unknown variables, whereupon Core
	 * will handle it if recognized. If this implementation returns true, processing
	 * of this value is assumed to have been completed.
	 *
	 * @param variable The variable to set.
	 * @param value The variable-specific value.
	 * @param[out] result  The return value of the NPN_SetValue call when handled.
	 *
	 * @return \c true if Core should do no further processing of this variable.
	 */
	virtual bool SetValue(NPPVariable variable, void* value, NPError* result) = 0;

	/**
	 * Invalidate plug-in window contents.
	 *
	 * This method will be called when a windowless plug-in instance calls
	 * NPN_InvalidateRect(), which is supposed to trigger a re-paint of the
	 * part of the document containing the plug-in.
	 *
	 * Some combinations of platforms and drawing can accomplish this without the
	 * expensive process that comprises Core invalidation and subsequent repaint,
	 * and may cancel that process using this method. It may also be used for
	 * pre-invalidation hooks.
	 *
	 * @param rect  The area to invalidate, specified in a coordinate system
	 *              whose origin is at the top left of the plug-in rectangle.
	 *
	 * @return \c true if invalidation has been handled by this implementation,
	 *         or \c false if Core should proceed with its ordinary
	 *         invalidation process.
	 */
	virtual bool Invalidate(NPRect* rect) = 0;

	/** @short Convert platform-specific region to NPRect.
	 *
	 * This method is used from NPN_InvalidateRegion to convert platform
	 * specific region to coordinates used for invalidating plugin area.
	 *
	 * Different platforms use different region types.
	 * See https://developer.mozilla.org/en/NPRegion for more details.
	 *
	 * @param      region  Platform-specific region, specified relative to the
	 *                     top left of the plug-in.
	 * @param[out] rect    Output NPRect, relative to the top left of the
	 *                     plug-in, set only on successful conversion.
	 *
	 * @return TRUE if region was converted to coordinates. FALSE otherwise.
	 */
	virtual bool ConvertPlatformRegionToRect(NPRegion region, NPRect &rect) = 0;

	/**
	 * Convert coordinates from one space to another.
	 *
	 * Altough NPN_ConvertPoint is defined as part of the multiplatform NPAPI,
	 * it is actually very Mac-specific and so far only used there. Other
	 * platforms are free to return ERR_NOT_SUPPORTED status.
	 *
	 * @param      src_x      Input x coordinate.
	 * @param      src_y      Input y coordinate.
	 * @param      src_space  Input coordinate space (value from NPCoordinateSpace enum).
	 * @param[out] dst_x      Output x coordinate.
	 * @param[out] dst_y      Output y coordinate.
	 * @param      dst_space  Output coordinate space (value from NPCoordinateSpace enum).
	 *
	 * @retval OpStatus::OK on successful conversion.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if platform does not support conversion.
	 * @return See OpStatus for appropriate responses to other error cases.
	 */
	virtual OP_STATUS ConvertPoint(double src_x, double src_y, int src_space,
	                               double* dst_x, double* dst_y, int dst_space);

	/** Pop up a context menu.
	 *
	 * (Presently only relevant on Mac when using the Cocoa event module. Other
	 * platforms may freely return OpStatus::ERR_NOT_SUPPORTED.)
	 *
	 * @param[in] menu  The menu to pop up.
	 *
	 * @retval OpStatus::OK if the menu was successfully shown.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if the platform does not support this function.
	 * @return See OpStatus for appropriate responses to other error cases.
	 */
	virtual OP_STATUS PopUpContextMenu(NPMenu* menu);
};

inline OP_STATUS OpPluginTranslator::ConvertPoint(double, double, int, double*, double*, int)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

inline OP_STATUS OpPluginTranslator::PopUpContextMenu(NPMenu*)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

#endif // defined(_PLUGIN_SUPPORT_) && defined(NS4P_COMPONENT_PLUGINS)
#endif // OPPLUGINTRANSLATOR_H__
