/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2008-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPNS4PLUGIN_ADAPTER_H
#define OPNS4PLUGIN_ADAPTER_H

#include "modules/pi/OpPluginWindow.h"

#include "modules/ns4plugins/src/plugincommon.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/ns4plugins/opns4plugin.h"

class VisualDevice;

/** @short Plug-in adapter for a Netscape 4 plug-in instance
 *
 * This class deals with the platform specific part of the Netscape 4 plug-in
 * API. It will not interact directly with the plug-in library; it will however
 * be passed the OpPluginWindow, NPWindow and NPPrint objects associated with
 * the plug-in.
 *
 * In a more perfect world, the Netscape 4 plug-in API would actually be clean
 * and without any platform-specific hacks, and as such there would be no need
 * for this class.
 *
 * An instance of this class is created earlier than the OpPluginWindow (since
 * it may provide information that affects creation of the OpPluginWindow). As
 * soon as the OpPluginWindow has been created, it is passed to this class by
 * calling SetPluginWindow().
 */
class OpNS4PluginAdapter
{
public:
	/** Create an OpNS4PluginAdapter object.
	 *
	 * @param new_object (out) Set to the new object created.
	 * @param component_type (in) Component type of the plugin.
	 */
	static OP_STATUS Create(OpNS4PluginAdapter** new_object, OpComponentType component_type);

	virtual ~OpNS4PluginAdapter() {}

	/** Set the plug-in window.
	 *
	 * Should be called as soon as possible when the OpPluginWindow is created,
	 * and only once.
	 *
	 * @param plugin_window The OpPluginWindow associated with this
	 * plug-in. May not be NULL. It is guaranteed to be valid at least until
	 * this OpNS4PluginAdapter is deleted.
	 */
	virtual void SetPluginWindow(OpPluginWindow* plugin_window) = 0;

	/** Set the visual device.
	 *
	 * Should be called as soon as possible when the OpPluginWindow is created,
	 * and only once. Used to "blitimage" on wingogi and getting view and painter on windows
	 *
	 * @param visual_device The visual device associated with this
	 * plug-in. May not be NULL. It is guaranteed to be valid at least until
	 * this OpNS4PluginAdapter is deleted.
	 */
	virtual void SetVisualDevice(VisualDevice* visual_device) = 0;

	/** @short Get value from browser to be passed to plug-in.
	 *
	 * This method is called from NPN_GetValue(NPP, NPNVariable, void*). The
	 * implementation of this method should set values and return TRUE for as
	 * few variables as possible, as _most_ of this can be dealt with by core
	 * in a cross-platform manner.
	 *
	 * Most notably, the NPNVnetscapeWindow and NPNVSupportsWindowless
	 * variables should probably be implemented on the platform side.
	 *
	 * @param variable Variable to get. Same as the second parameter passed to
	 * NPN_GetValue().
	 * @param value (out) Destination address for the value. Data type depends
	 * entirely on what 'variable' is. Same as the third parameter passed to
	 * NPN_GetValue().
	 * @param result (out) Set to NPERR_NO_ERROR if everything went well. Refer
	 * to the plug-in spec for other possible values.
	 *
	 * @return TRUE if a value was set and core should pass this value to the
	 * plug-in. Set to FALSE if core should set the value on its own (and
	 * disregard the value of 'value' and 'result')
	 */
	virtual BOOL GetValue(NPNVariable variable, void* value, NPError* result) = 0;

	/** @short Get value from browser to be passed to plug-in.
	 *
	 * This method is called from NPN_GetValue(NPP, NPNVariable, void*) if
	 * NPP argument is null. The implementation of this method should set values
	 * and return TRUE for as few variables as possible, as _most_ of this can
	 * be dealt with by core in a cross-platform manner.
	 *
	 * @param variable Variable to get. Same as the second parameter passed to
	 * NPN_GetValue().
	 * @param value (out) Destination address for the value. Data type depends
	 * entirely on what 'variable' is. Same as the third parameter passed to
	 * NPN_GetValue().
	 * @param result (out) Set to NPERR_NO_ERROR if everything went well. Refer
	 * to the plug-in spec for other possible values.
	 *
	 * @return TRUE if a value was set and core should pass this value to the
	 * plug-in. Set to FALSE if core should set the value on its own (and
	 * disregard the value of 'value' and 'result')
	 */
	static BOOL GetValueStatic(NPNVariable variable, void* value, NPError* result);

	/** @short Set value by plug-in.
	 *
	 * This method is called from NPN_SetValue(NPP, NPPVariable, void*). The
	 * implementation of this method should set values and return TRUE for as
	 * few variables as possible, as _most_ of this can be dealt with by core
	 * in a cross-platform manner.
	 *
	 * @param variable Variable to set. Same as the second parameter passed to
	 * NPN_SetValue().
	 * @param value (in) The value of the specified variable to be set. 
	 * Data type depends entirely on what 'variable' is. Same as the third
	 * parameter passed to NPN_SetValue().
	 * @param result (out) Set to NPERR_NO_ERROR if everything went well. Refer
	 * to the plug-in spec for other possible values.
	 *
	 * @return TRUE if a value was set and core should pass this value to the
	 * plug-in. Set to FALSE if core should set the value on its own (and
	 * disregard the value of 'value' and 'result')
	 */
	virtual BOOL SetValue(NPPVariable variable, void* value, NPError* result) = 0;

	/** @short Convert platform specific region to NPRect.
	 *
	 * This method is used from NPN_InvalidateRegion to convert platform
	 * specific region to coordinates used for invalidating plugin area.
	 *
	 * Different platforms use different region types.
	 * See https://developer.mozilla.org/en/NPRegion for more details.
	 *
	 * @param invalidRegion Platform specific region. The area to invalidate,
	 * specified in a coordinate system that originates at the top left of the plug-in.
	 * @param[in,out] invalidRect NPRect whose coordinates are set on
	 * successful conversion.
	 *
	 * @return TRUE if region was converted to coordinates. FALSE otherwise.
	 */
	virtual BOOL ConvertPlatformRegionToRect(NPRegion invalidRegion, NPRect &invalidRect) = 0;

	/** Prepare an NPWindow for a call to NPP_SetWindow().
	 *
	 * Calling this method before an OpPluginWindow has been set is pointless.
	 *
	 * @param[in,out] npwin NPWindow that will be passed in the
	 * NPP_SetWindow() call.
	 * @param rect Rectangle of plug-in window. Coordinates are relative to the
	 * to the upper left corner of the parent OpView. Dimensions (width and
	 * height) signify the size of the plug-in.
	 * @param paint_rect is the rectangle specifying what part of the plugin relative
	 * to the rendering viewport should be painted. Coordinates are scaled.
	 * @param view_rect is the rectangle of the plugin window (in scaled coordinates)
	 * relative to the parent OpView. The top-left corner of this rectangle has
	 * a different value than rect if the plugin is inside an iframe. The platform
	 * implementation can use this rectangle e.g. if the plugin window is painted
	 * off-screen. It then can blit the plugin's content to this rectangle.
	 * @param view_offset Offset of OpView relative to screen.
	 * @param show Whether to show/hide the plugin window.
	 * @param transparent Indicates whether windowless plugin is transparent or opaque.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors
	 */
	virtual OP_STATUS SetupNPWindow(NPWindow* npwin, const OpRect& rect, const OpRect& paint_rect, const OpRect& view_rect, const OpPoint& view_offset, BOOL show, BOOL transparent) = 0;

	/** Prepare an NPPrint for a call to NPP_Print().
	 *
	 * Calling this method before an OpPluginWindow has been set is pointless.
	 *
	 * @param[in,out] npprint NPPrint that will be passed in the NPP_Print()
	 * call.
	 * @param rect Rectangle of plug-in window. Coordinates are relative to the
	 * to the upper left corner of the parent OpView. Dimensions (width and
	 * height) signify the size of the plug-in.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors
	 */
	virtual OP_STATUS SetupNPPrint(NPPrint* npprint, const OpRect& rect) = 0;

	/** Save state before a synchronous call to the plug-in.
	 *
	 * This is an opportunity for the platform to save the state before a
	 * synchronous call is made to the plug-in. The information saved here can
	 * then be restored in RestoreState(), which will be called right after the
	 * synchronous plug-in call. Most platforms will probably do nothing here.
	 *
	 * @param event_type The type of event associated with the imminent call to
	 * the plug-in.
	 */
	virtual void SaveState(OpPluginEventType event_type) = 0;

	/** Restore state after a synchronous call to the plug-in.
	 *
	 * This is an opportunity for the platform to restore the state after a
	 * synchronous call has been made to the plug-in. The information saved in
	 * SaveState() can then be restored in here. Most platforms will probably
	 * do nothing here.
	 *
	 * @param event_type The type of event associated with the call to the
	 * plug-in that just took place.
	 */
	virtual void RestoreState(OpPluginEventType event_type) = 0;

	/** Give platform possibility to modify parameters passed to the plug-in.
	 * This method will be called right before NPP_New for each plugin instance.
	 *
	 * If platform modifies parameters without adding/removing any it should set:
	 *   *new_args8 = NULL; *new_argc=0;
	 *
	 * @param[in] plugin plug-in interface
	 * @param[in,out] args8 array of plugin parameters,
	 * first argc pointers are names and remaining argc are values.
	 * @param[in] argc number of parameters (there are 2*argc strings - names and values)
	 * @param[out] new_args8 new plugin parameters or NULL
	 * @param[out] new_argc number of new parameters (there will be 2*new_argc strings in new_args8) or 0
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors
	 */
	virtual OP_STATUS ModifyParameters(OpNS4Plugin* plugin, char** args8, int argc, char*** new_args8, int* new_argc) { *new_args8 = NULL; *new_argc = 0; return OpStatus::OK; }

#ifdef NS4P_COMPONENT_PLUGINS

	/**
	 * Give platform possibility to block loading of a plug-in based on its filename/path.
	 *
	 * For example, on Linux if the plug-in file is a symlink, the platform can call
	 * realpath() and then run g_pcapp->IsPluginToBeIgnored() on the final filename.
	 * @param[in] plugin_filepath full path to a plug-in
	 * @return true if the plug-in should be ignored, false if the plug-in may load
	 */
	static bool IsBlacklistedFilename(UniString plugin_filepath);

	/** Retrieve a channel for this adapter instance.
	 *
	 * The channel must remain constant for the lifetime of this object, and
	 * should, for OOM reasons, be allocated when constructing this object,
	 * such that this method becomes a simple accessor.
	 *
	 * This channel, if returned, will receive messages from platform code
	 * residing in the plug-in component. This allows typed platform-specific
	 * exchanges to occur between the plug-in component and the browser
	 * component without involving Core.
	 *
	 * The address of the channel will be supplied to OpPluginTranslator
	 * (in the plug-in component) on creation.
	 *
	 * @return Pointer to OpChannel, or NULL if none is available. NULL must not
	 *         indicate OOM. Any potentially failing action should take place as
	 *         part of the construction of this object.
	 */
	virtual OpChannel* GetChannel() { return NULL; }

	/** Inform the platform that the plugin sent an IME Start request.
	 *
	 * Implement this if plugins in your system provide IME feedback.
	 */
	virtual void StartIME() { }

	/** Return \c true if the platforms IME input is active.
	 *
	 * Implement if you are using the StartIME function above.
	 *
	 * @return \c true if the IME is active, otherwise \c false.
	 */
	virtual bool IsIMEActive() { return false; }

	/** Inform the platform that the plugin sent a focus event.
	 *
	 * While the IME is active, your platform may need to be informed that
	 * the focus of the plugin has changed. You can update IME properties here,
	 * if required.
	 *
	 * @param focus \c True when the plugin receives focus, otherwise \c false.
	 * @param reason Reason for the focus.
	 */
	virtual void NotifyFocusEvent(bool focus, FOCUS_REASON reason) { }

#endif // NS4P_COMPONENT_PLUGINS
};

#endif // OPNS4PLUGIN_ADAPTER_H
