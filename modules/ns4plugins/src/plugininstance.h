/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_SRC_PLUGIN_INSTANCE_H
#define NS4P_SRC_PLUGIN_INSTANCE_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/ns4plugins/src/pluginlib.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"
#include "modules/pi/OpPluginWindow.h"

class OpPluginImage;
class OpPluginBackgroundImage;

/**
 * This is the browser-side representation of a plug-in instance in a remote
 * plug-in component (PluginComponentInstance.)
 *
 * It is instantiated when the browser calls NPP_New on the function table of
 * a PluginLib, and is destroyed when the browser calls NPP_Destroy on same.
 */
class PluginInstance
	: public OpMessageListener
{
public:
	typedef OpNs4pluginsMessages_MessageSet::PluginObject PluginObject;
	typedef OpNs4pluginsMessages_MessageSet::PluginVariant PluginVariant;
	typedef OpNs4pluginsMessages_MessageSet::PluginStream PluginStream;
	typedef OpNs4pluginsMessages_MessageSet::PluginRect PluginRect;
	typedef OpNs4pluginsMessages_MessageSet::PluginPoint PluginPoint;

	/**
	 * Create a new plug-in instance.
	 *
	 * @param npp The plug-in instance identifier defined by the browser.
	 */
	PluginInstance(NPP npp);

	/**
	 * Destroy a plug-in instance.
	 */
	virtual ~PluginInstance();

	/**
	 * Retrieve plug-in library.
	 */
	PluginLib* GetLibrary() { return m_library; }

	/**
	 * Process a message sent to this plug-in instance.
	 *
	 * Handles most NPN function calls.
	 *
	 * @return See OpStatus.
	 */
	OP_STATUS ProcessMessage(const OpTypedMessage* message);

	/**
	 * Synchronous relays for selected NPP functions.
	 *
	 * See NPAPI.
	 *
	 * Functions not listed here are either not supported (e.g. NPP_Print), or have customized
	 * signatures and are listed individually below.
	 */
	NPError New(NPMIMEType pluginType, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
	NPError Destroy(NPSavedData** save);
	NPError GetValue(NPPVariable variable, void* ret_value);
	NPError NewStream(NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
	NPError DestroyStream(NPStream* stream, NPReason reason);
	int32 WriteReady(NPStream* stream);
	int32 Write(NPStream* stream, int32 offset, int32 len, void* buffer);
	void StreamAsFile(NPStream* stream, const char* fname);
	void URLNotify(const char* url, NPReason reason, void* notifyData);

	/**
	 * Customized variant of NPP_SetWindow.
	 *
	 * @param window NPAPI platforms-specific window information.
	 * @param adapter_channel Message channel created by the platform implementation of OpPluginAdapter
	 *                        to carry platform-specific communication between its PluginAdapter (browser)
	 *                        and PluginTranslator (plug-in component). May be NULL if no such channel is
	 *                        needed.
	 *
	 * @return See return values of NPP_SetWindow.
	 */
	NPError SetWindow(NPWindow* window, OpChannel* adapter_channel);

	/**
	 * Handle NPP_HandleEvent focus events.
	 *
	 * @param focus                  Whether we got or lost focus.
	 * @param reason                 Why a focus event was triggered, see FOCUS_REASON.
	 * @param[out] is_event_handled  Whether the event was handled by the plug-in.
	 *
	 * @return See OpStatus for other errors.
	 * @retval OpStatus::ERR_NULL_POINTER when a channel hasn't been established.
	 */
	OP_STATUS HandleFocusEvent(bool focus, FOCUS_REASON reason, bool* is_event_handled);

	/**
	 * Handle NPP_HandleEvent paint events.
	 *
	 * @param painter         Painter used to draw the browser window.
	 * @param plugin_rect     Dimension and position of the plug-in window,
	 *                        relative to the top left corner of the document
	 *                        body.
	 * @param paint_rect      Dimension and position of the rectangle to paint,
	 *                        relative to the top left corner of the plug-in
	 *                        window.
	 * @param transparent     Whether the plug-in window type is transparent
	 *                        (\c true) or opaque (\c false).
	 * @param parent_opwindow The document OpWindow that hosts the plug-in.
	 *
	 * @return See OpStatus for other errors.
	 * @retval OpStatus::ERR_NULL_POINTER when a channel hasn't been established.
	 */
	OP_STATUS HandleWindowlessPaint(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect, bool transparent, OpWindow* parent_opwindow);

	/**
	 * Handle NPP_HandleEvent mouse events.
	 *
	 * @param event_type             Mouse event type, see OpPluginEventType.
	 * @param point                  Point at which the mouse button was clicked.
	 *                               Coordinate origin is at top left corner of the
	 *                               plug-in window.
	 * @param button                 Mouse button that triggered the event.
	 * @param modifiers              Key-modifiers, e.g. ctrl or shift.
	 * @param[out] is_event_handled  Whether the event was handled by the plug-in.
	 *
	 * @return See OpStatus for other errors.
	 * @retval OpStatus::ERR_NULL_POINTER when a channel hasn't been established.
	 */
	OP_STATUS HandleWindowlessMouseEvent(OpPluginEventType event_type, const OpPoint& point, int button,
	                                     ShiftKeyState modifiers, bool* is_event_handled);

	/**
	 * Handle NPP_HandleEvent keyboard events.
	 *
	 * @param key                    The key that triggered the event.
	 * @param key_value              Pointer to the character value of the key that triggered the event (NULL if none).
	 * @param key_state              State of the key, up/down/pressed.
	 * @param modifiers              Key-modifiers, e.g. ctrl or shift.
	 * @param platform_data_1        Platform-specific data for handling key events.
	 * @param platform_data_2        Platform-specific data for handling key events.
	 * @param[out] is_event_handled  Whether the event was handled by the plug-in.
	 *
	 * @return See OpStatus for other errors.
	 * @retval OpStatus::ERR_NULL_POINTER when no channel has been established.
	 */
	OP_STATUS HandleWindowlessKeyEvent(const OpKey::Code key, const uni_char* key_value, const OpPluginKeyState key_state, const ShiftKeyState modifiers,
	                                   UINT64 platform_data_1, UINT64 platform_data_2, INT32* is_event_handled);

	/**
	 * Convert PluginVariant to NPVariant.
	 *
	 * @param[out] out_variant NPVariant allocated by caller.
	 * @param variant Input PluginVariant.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS SetNPVariant(NPVariant* out_variant, const PluginVariant& variant);

	/**
	 * Convert NPvariant to PluginVariant.
	 *
	 * @param[out] out_variant PluginVariant allocated by caller.
	 * @param variant Input NPVariant.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS SetPluginVariant(PluginVariant& out_variant, const NPVariant& variant);

	/**
	 * Convert UniString to NPString.
	 *
	 * @param[out] out_string NPString structure allocated by caller.
	 * @param string Input UniString.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS SetNPString(NPString* out_string, const UniString& string);

	/**
	 * Build PluginResult message.
	 *
	 * @param success Required boolean field value.
	 * @param result Optional PluginVariant field value. Will not be set if 'success' is false.
	 *
	 * @return Pointer to OpPluginResultMessage or NULL on OOM. Returned value must be free'd
	 *         by caller.
	 */
	static OpPluginResultMessage* BuildPluginResultMessage(bool success, const NPVariant* result);

	/**
	 * Build NPVariant array from PluginVariant vector.
	 *
	 * @param arguments Input vector.
	 *
	 * @return Pointer to NPVariant array of equal length to input vector, or NULL on OOM. Caller
	 *         assumes ownership, and must release the array using ReleaseNPVariantArray().
	 */
	static NPVariant* BuildNPVariantArray(const OpProtobufMessageVector<PluginVariant>& arguments);

	/**
	 * Release NPVariant array built by BuildNPVariantArray().
	 *
	 * @param args NPVariant array.
	 * @param count Number of array elements.
	 */
	static void ReleaseNPVariantArray(NPVariant* args, unsigned int count);

	/**
	 * Convert NPStream to PluginStream.
	 *
	 * @param[out] out_stream PluginStream allocated by caller.
	 * @param stream Input NPStream.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS SetPluginStream(PluginStream& out_stream, const NPStream* stream);

	/**
	 * Convert NPRect to PluginRect.
	 *
	 * @param[out] out_rect PluginRect allocated by caller.
	 * @param rect Input NPRect.
	 */
	static void SetPluginRect(PluginRect& out_rect, const NPRect* rect);

	/**
	 * Convert an NPRect to a OpRect.
	 *
	 * @param[out] out_rect Rectangle structure allocated by caller.
	 * @param rect NPRect.
	 */
	static void SetOpRect(OpRect* out_rect, const NPRect& rect);

	/**
	 * Convert an OpRect to a protobuf PluginRect.
	 *
	 * @param[out] out_rect, Rectangle structure allocated by caller.
	 * @param rect OpRect.
	 */
	static void SetPluginRect(PluginRect* out_rect, const OpRect& rect);

	/**
	 * Convert a PluginRect to an NPRect.
	 *
	 * @param[out] out_rect. Rectangle structure allocated by caller.
	 * @param rect PluginRect.
	 */
	static void SetNPRect(NPRect* out_rect, const PluginRect& rect);

	/**
	 * Convert an OpPoint to a protobuf PluginPoint.
	 *
	 * @param[out] out_point, PluginPoint structure allocated by caller.
	 * @param point OpPoint.
	 */
	static void SetPluginPoint(PluginPoint* out_point, const OpPoint& point);

	/**
	 * Convert a protobuf PluginPoint to an OpPoint.
	 */
	static OpPoint OpPointFromPluginPoint(PluginPoint& point);

	/**
	 * Convert an NPObject to a PluginObject.
	 *
	 * @param[out] out_object Reference to object allocated by caller.
	 * @param object Input object.
	 */
	static void SetPluginObject(PluginObject& out_object, const NPObject* object);

	/**
	 * Classify NPNVariable values.
	 */
	static bool IsBooleanValue(NPNVariable variable);
	static bool IsIntegerValue(NPNVariable variable);
	static bool IsStringValue(NPNVariable variable);
	static bool IsObjectValue(NPNVariable variable);

	/**
	 * Handle PluginGetValue message.
	 *
	 * @param npp      Plug-in instance, or NULL if none.
	 * @param message  Message containing the NPNVariable to get.
	 *
	 * @return See OpStatus.
	 */
	static OP_STATUS HandlePluginGetValueMessage(NPP npp, OpPluginGetValueMessage* message);

protected:
	/**
	 * Construct an OpPluginNewMessage object from the given data.
	 *
	 * @param pluginType MIME type of plug-in content.
	 * @param mode NP_EMBED or NP_FULL. See NPP_New.
	 * @param argc Argument count.
	 * @param argn Argument names.
	 * @param argv Argument values.
	 * @param saved Saved library value, see NPP_New and NPP_Destroy.
	 * @param adapter_channel Message channel created by the platform implementation of OpPluginAdapter
	 *                        to carry platform-specific communication between its PluginAdapter (browser)
	 *                        and PluginTranslator (plug-in component). May be NULL if no such channel is
	 *                        needed.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS BuildPluginNewMessage(OpPluginNewMessage* message, NPMIMEType pluginType, uint16 mode, int16 argc,
									char* argn[], char* argv[], NPSavedData* saved, OpChannel* adapter_channel);

	/**
	 * Message handlers. See components/messages.proto for message details.
	 */
	OP_STATUS OnPluginSetValueMessage(OpPluginSetValueMessage* message);
	OP_STATUS OnPluginEvaluateMessage(OpPluginEvaluateMessage* message);
	OP_STATUS OnPluginGetPropertyMessage(OpPluginGetPropertyMessage* message);
	OP_STATUS OnPluginSetPropertyMessage(OpPluginSetPropertyMessage* message);
	OP_STATUS OnPluginHasMethodMessage(OpPluginHasMethodMessage* message);
	OP_STATUS OnPluginCreateObjectMessage(OpPluginCreateObjectMessage* message);
	OP_STATUS OnPluginInvokeMessage(OpPluginInvokeMessage* message);
	OP_STATUS OnPluginInvokeDefaultMessage(OpPluginInvokeDefaultMessage* message);
	OP_STATUS OnPluginHasPropertyMessage(OpPluginHasPropertyMessage* message);
	OP_STATUS OnPluginRemovePropertyMessage(OpPluginRemovePropertyMessage* message);
	OP_STATUS OnPluginObjectEnumerateMessage(OpPluginObjectEnumerateMessage* message);
	OP_STATUS OnPluginObjectConstructMessage(OpPluginObjectConstructMessage* message);
	OP_STATUS OnPluginGetURLMessage(OpPluginGetURLMessage* message);
	OP_STATUS OnPluginGetURLNotifyMessage(OpPluginGetURLNotifyMessage* message);
	OP_STATUS OnPluginPostURLMessage(OpPluginPostURLMessage* message);
	OP_STATUS OnPluginPostURLNotifyMessage(OpPluginPostURLNotifyMessage* message);
	OP_STATUS OnPluginDestroyStreamMessage(OpPluginDestroyStreamMessage* message);
	OP_STATUS OnPluginUserAgentMessage(OpPluginUserAgentMessage* message);
	OP_STATUS OnPluginInvalidateMessage(OpPluginInvalidateMessage* message);
	OP_STATUS OnPluginStatusTextMessage(OpPluginStatusTextMessage* message);
	OP_STATUS OnPluginGetValueForURLMessage(OpPluginGetValueForURLMessage* message);
	OP_STATUS OnPluginSetValueForURLMessage(OpPluginSetValueForURLMessage* message);
	OP_STATUS OnPluginPushPopupsEnabledMessage(OpPluginPushPopupsEnabledMessage* message);
	OP_STATUS OnPluginPopPopupsEnabledMessage(OpPluginPopPopupsEnabledMessage* message);
	OP_STATUS OnPluginGetAuthenticationInfoMessage(OpPluginGetAuthenticationInfoMessage* message);

	/** Plug-in library associated with this plug-in instance. */
	PluginLib* m_library;

	/** NPP handle used as identification when calling NPN functions. */
	NPP m_npp;

	/** Channel to PluginComponentInstance in the plug-in component. */
	OpChannel* m_remote_instance;

	/** An image of the browser rendered background to the object/embed element. Sent to
	 *  the plug-in component to allow 24-bit plug-in renderers to affect existing pixels
	 *  in order to produce transparency. See HandleWindowlessPaint(). */
	OpPluginBackgroundImage* m_plugin_background;

	/** The final plug-in rendering returned by the plug-in component. See HandleWindowlessPaint(). */
	OpPluginImage* m_plugin_frame;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // NS4P_SRC_PLUGIN_INSTANCE_H
