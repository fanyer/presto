/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPNS4PLUGIN_H
#define OPNS4PLUGIN_H

#ifdef _PLUGIN_SUPPORT_

# include "modules/url/url2.h"

class ES_Value;
class ES_Runtime;
class ES_Object;
class HTML_Element;

#ifdef USE_PLUGIN_EVENT_API
class OpPluginWindow;
#endif // USE_PLUGIN_EVENT_API
class Plugin;

#include "modules/dom/domeventtypes.h"

#ifdef USE_PLUGIN_EVENT_API
#include "modules/pi/OpPluginWindow.h"
#include "modules/inputmanager/inputmanager_constants.h"
#endif // USE_PLUGIN_EVENT_API

/** The browser's interface to the Plugin. */

class OpNS4Plugin
{
public:
	virtual ~OpNS4Plugin() {}

#ifdef USE_PLUGIN_EVENT_API
	/** Get the OpPluginWindow associated with this plug-in.
	 *
	 * Will return NULL if the plug-in is not sufficiently initialized.
	 */
	virtual OpPluginWindow* GetPluginWindow() const = 0;

	/** Set listener on the OpPluginWindow when it is created */
	virtual void SetWindowListener(OpPluginWindowListener* listener) = 0;
#endif // USE_PLUGIN_EVENT_API

	/** Is the plug-in windowless?
	 *
	 * @return TRUE if windowless, FALSE if windowed. Return value is undefined
	 * if GetPluginWindow() returns NULL.
	 */
	virtual BOOL IsWindowless() const = 0;

	/** AddPluginStream() initiates and inserts the stream for the plugin itself
	 * into the plugin's stream_list and puts the url object in the url_list.
	 * @param url The URL of the plugin to stream.
	 * @param mime_type The mime type of the stream, or NULL if not available.
	 * @return Normal OOM values
	 */
	virtual OP_STATUS	AddPluginStream(URL& url, const uni_char* mime_type) = 0;

	/** Called to stop loading of all streams */
	virtual void		StopLoadingAllStreams() = 0;

	virtual FramesDocument*	GetDocument() const = 0;

	/** Display the plug-in. Tell it about changes in its window.
	 *
	 * Can be either called during painting, reflowing or at arbitrary time
	 * in case plugin window has not been created yet.
	 *
	 * When called first time and NPP_New() has returned, this will create plugin
	 * window. Otherwise plugin window will be created at soonest opportunity.
	 * First call should be called with 'show' argument set to FALSE to ensure
	 * that a plug-in that e.g. produces sound, but is not within the visible
	 * view, will not start playing the sound even if it's not visible.
	 *
	 * During reflowing some arguments (namely show and is_fixed_positioned)
	 * have hardcoded FALSE value which does not necessarily reflect reality
	 * thus plugin window is not being updated during that phase as that
	 * would cause flickering if visibility would change for example.
	 *
	 * Can also be called from HandleBusyState() in case plugin window has not
	 * been created yet. That is similar to reflowing in a sense that both
	 * show and is_fixed_positioned arguments are FALSE.
	 *
	 * For windowed plug-ins, this will update the position and dimension of
	 * the plug-in window, and show it (unless 'show' is FALSE), and notify the
	 * plug-in library about the new values.
	 *
	 * For windowless plug-ins, this will notify the plug-in library about the
	 * position and dimension, and then paint it (unless 'show' is
	 * FALSE). Whatever the plug-in wants to paint must be painted now, to get
	 * layout box stacking order right.
	 *
	 * In some cases, when the plug-in is busy or not yet initialized, the
	 * plug-in cannot be displayed immediately. If this is the case, Display()
	 * will make sure that a paint request will be issued when the plug-in
	 * becomes ready (which in turn will cause this method to be called again).
	 *
	 * @param plugin_rect Position and size of the plug-in. Coordinates are
	 * relative to the viewport. All values are unscaled ("document" values).
	 * @param paint_rect Area to paint. This is only used for windowless
	 * plug-ins. Coordinates are relative to the viewport. All values are
	 * unscaled ("document" values). This rectangle will always be fully
	 * contained by 'plugin_rect', i.e. so that
	 * paint_rect.IntersectWith(plugin_rect) has no effect.
	 * @param show TRUE if the plug-in is to be shown / painted, FALSE if the
	 * plug-in is only to be notified about its dimensions and whereabouts,
	 * without being shown or painted. Some plug-ins are defined as hidden (in
	 * the HTML markup), typically because the plug-in is only needed for its
	 * ability to produce sound, or to perform other non-visual operations, in
	 * which case this parameter will be FALSE.
	 * @param fixed_position_subtree The first ancestor fixed positioned element
	 * of the plug-in (including the element owning the plugin)
	 * or NULL if no such exists.
	 *
	 * @return OK if successful (this also includes busy or not initialized
	 * plug-ins, since this does not require the caller to take any actions),
	 * ERR_NO_MEMORY if OOM, ERR for other errors.
	 */
#ifdef USE_PLUGIN_EVENT_API
	virtual OP_STATUS	Display(const OpRect& plugin_rect, const OpRect& paint_rect, BOOL show, HTML_Element* fixed_position_subtree) = 0;
#else
	/** SetWindow() tells the plugin about changes in it's window */
	virtual OP_STATUS       SetWindow(int x, int y, unsigned int width, unsigned int height, BOOL show, const OpRect* rect = NULL) = 0;
#endif // USE_PLUGIN_EVENT_API

	/** HandleEvent informs a plugin about mouse and keyboard events */
	virtual BOOL		HandleEvent(DOM_EventType event, const OpPoint& point, int button_or_key, ShiftKeyState modifiers) = 0;

#ifdef USE_PLUGIN_EVENT_API
	/** Send a plugin event to the plugin */
	virtual BOOL SendEvent(OpPlatformEvent * event, OpPluginEventType event_type) = 0;

	/**
	 * Send a keyboard input event to the plug-in.
	 * @param key The keycode of the virtual key pressed or released.
	 * @param key_value The character (string in the general case) produced by the key.
	 * @param key_event_data The extra event data for this event. Will be used to
	 *        faithfully reconstruct the key event as a platform key event.
	 * @param key_state  Key up/down/pressed state.
	 * @param location  The location of the key.
	 * @param modifiers  The state of the modifiers keys.
	 * @return \c true if the event was successfully delivered. \c false otherwise.
	 */
	virtual bool DeliverKeyEvent(OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, const OpPluginKeyState key_state, OpKey::Location location, const ShiftKeyState modifiers) = 0;

	/**
	 * Send a focus event to the plug-in.
	 * @param focus   Got/lost focus.
	 * @param reason  Source of the focus event. See FOCUS_REASON.
	 * @return \c true if the event was successfully delivered. \c false otherwise.
	 */
	virtual bool DeliverFocusEvent(bool focus, FOCUS_REASON reason) = 0;
#endif // USE_PLUGIN_EVENT_API

	/** OnMoreDataAvailable() should be called when there are more data available
	 * for the stream of the plugin itself
	 * @return Normal OOM values */
	virtual OP_STATUS   OnMoreDataAvailable() = 0;

	/** Returns an ecmascript object for the NPObject scriptable object for the plugin.
		If not available and the allow_suspend parameter is TRUE, then the the script is
		suspended until the scriptable object should be available. */
	virtual OP_STATUS	GetScriptableObject(ES_Runtime* runtime, BOOL allow_suspend, ES_Value* value) = 0;

	/** Returns the plugin string value for form submission if the plugin is part of a form
	    Returns NULL if no string value available for this plugin.
	    Returned string value should be delete'd */
	virtual OP_STATUS   GetFormsValue(uni_char*& value) = 0;

#if defined(_PLUGIN_NAVIGATION_SUPPORT_)
	/**
	 * Called by the spatial navigation engine for navigation inside a plugin.
	 *
	 * When a plugin is navigated to the spatial navigation engine calls
	 * NavigateInsidePlugin() for all subsequent navigation events until
	 * NavigateInsidePlugin() returns FALSE, in which case the navigation engine
	 * continues navigation beyond the plugin.
	 *
	 * \attention It is up to the plugin implementation to ensure that the
	 * \attention events for the navigation keys reaches the input manager
	 * \attention even if the plugin has focus. Otherwise, it is not possible
	 * \attention to navigate out of the plugin.
	 *
	 * @param direction   The direction to navigate within the plugin (in degrees).
	 *
	 * @return TRUE if navigation was done inside the plugin
	 *         (e.g. the plugin moved focus from one widget to another), FALSE otherwise.
	 */
	virtual BOOL NavigateInsidePlugin(INT32 direction) = 0;
#endif

	/** Marks the plugin as popup enabled */
	virtual void		SetPopupsEnabled(BOOL enabled) = 0;

	/**
	 * Called when the user is scrolling a windowless (transparent) plugin.
	 * NPP_SetWindow() tells the plugin about it's changed x and y positions.
	 */
	virtual OP_STATUS	SetWindowPos(int x, int y) = 0;

	/** Hide the plug-in window. */
	virtual void	HideWindow() = 0;

	/** Returns TRUE if a plugin has returned from the New call. */
	virtual BOOL IsPluginFullyInited() = 0;
};

#endif // _PLUGIN_SUPPORT_
#endif // !OPNS4PLUGIN_H
