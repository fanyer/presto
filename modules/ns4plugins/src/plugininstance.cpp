/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/doc/frm_doc.h"
#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/hardcore/component/Messages.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/plugininstance.h"
#include "modules/ns4plugins/src/pluginlibhandler.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/opdata/UniStringUTF8.h"
#include "modules/pi/OpPluginImage.h"


PluginInstance::PluginInstance(NPP npp)
	: m_library(NULL)
	, m_npp(npp)
	, m_remote_instance(NULL)
	, m_plugin_background(NULL)
	, m_plugin_frame(NULL)
{
};

PluginInstance::~PluginInstance()
{
	if (m_remote_instance)
	{
		g_pluginhandler->DelistChannel(m_remote_instance);
		OP_DELETE(m_remote_instance);
	}

	OP_DELETE(m_plugin_background);
	OP_DELETE(m_plugin_frame);
}

OP_STATUS
PluginInstance::ProcessMessage(const OpTypedMessage* message)
{
#ifdef CPUUSAGETRACKING
	Plugin* plugin = g_pluginhandler->FindPlugin(m_npp);
	TRACK_CPU_PER_TAB((plugin && plugin->GetDocument()) ? plugin->GetDocument()->GetWindow()->GetCPUUsageTracker() : NULL);
#endif // CPUUSAGETRACKING

	switch (message->GetType())
	{
		case OpPeerConnectedMessage::Type:
		case OpPluginNewResponseMessage::Type:
		case OpPluginDestroyResponseMessage::Type:
		case OpStatusMessage::Type:
			/* Handled by OpSyncMessenger. */
			break;

		case OpPluginWindowlessHandleEventResponseMessage::Type:
			/* Right click handled asynchronously. */
			break;

		case OpPeerDisconnectedMessage::Type:
			/* Remote instance lost. */
			break;

		case OpPluginGetValueMessage::Type:
			return HandlePluginGetValueMessage(m_npp, OpPluginGetValueMessage::Cast(message));

		case OpPluginSetValueMessage::Type:
			return OnPluginSetValueMessage(OpPluginSetValueMessage::Cast(message));

		case OpPluginEvaluateMessage::Type:
			return OnPluginEvaluateMessage(OpPluginEvaluateMessage::Cast(message));

		case OpPluginGetPropertyMessage::Type:
			return OnPluginGetPropertyMessage(OpPluginGetPropertyMessage::Cast(message));

		case OpPluginSetPropertyMessage::Type:
			return OnPluginSetPropertyMessage(OpPluginSetPropertyMessage::Cast(message));

		case OpPluginHasMethodMessage::Type:
			return OnPluginHasMethodMessage(OpPluginHasMethodMessage::Cast(message));

		case OpPluginCreateObjectMessage::Type:
			return OnPluginCreateObjectMessage(OpPluginCreateObjectMessage::Cast(message));

		case OpPluginInvokeMessage::Type:
			return OnPluginInvokeMessage(OpPluginInvokeMessage::Cast(message));

		case OpPluginInvokeDefaultMessage::Type:
			return OnPluginInvokeDefaultMessage(OpPluginInvokeDefaultMessage::Cast(message));

		case OpPluginHasPropertyMessage::Type:
			return OnPluginHasPropertyMessage(OpPluginHasPropertyMessage::Cast(message));

		case OpPluginRemovePropertyMessage::Type:
			return OnPluginRemovePropertyMessage(OpPluginRemovePropertyMessage::Cast(message));

		case OpPluginObjectEnumerateMessage::Type:
			return OnPluginObjectEnumerateMessage(OpPluginObjectEnumerateMessage::Cast(message));

		case OpPluginObjectConstructMessage::Type:
			return OnPluginObjectConstructMessage(OpPluginObjectConstructMessage::Cast(message));

		case OpPluginGetURLMessage::Type:
			return OnPluginGetURLMessage(OpPluginGetURLMessage::Cast(message));

		case OpPluginGetURLNotifyMessage::Type:
			return OnPluginGetURLNotifyMessage(OpPluginGetURLNotifyMessage::Cast(message));

		case OpPluginPostURLMessage::Type:
			return OnPluginPostURLMessage(OpPluginPostURLMessage::Cast(message));

		case OpPluginPostURLNotifyMessage::Type:
			return OnPluginPostURLNotifyMessage(OpPluginPostURLNotifyMessage::Cast(message));

		case OpPluginDestroyStreamMessage::Type:
			return OnPluginDestroyStreamMessage(OpPluginDestroyStreamMessage::Cast(message));

		case OpPluginUserAgentMessage::Type:
			return OnPluginUserAgentMessage(OpPluginUserAgentMessage::Cast(message));

		case OpPluginInvalidateMessage::Type:
			return OnPluginInvalidateMessage(OpPluginInvalidateMessage::Cast(message));

		case OpPluginStatusTextMessage::Type:
			return OnPluginStatusTextMessage(OpPluginStatusTextMessage::Cast(message));

		case OpPluginGetValueForURLMessage::Type:
			return OnPluginGetValueForURLMessage(OpPluginGetValueForURLMessage::Cast(message));

		case OpPluginSetValueForURLMessage::Type:
			return OnPluginSetValueForURLMessage(OpPluginSetValueForURLMessage::Cast(message));

		case OpPluginPushPopupsEnabledMessage::Type:
			return OnPluginPushPopupsEnabledMessage(OpPluginPushPopupsEnabledMessage::Cast(message));

		case OpPluginPopPopupsEnabledMessage::Type:
			return OnPluginPopPopupsEnabledMessage(OpPluginPopPopupsEnabledMessage::Cast(message));

		case OpPluginGetAuthenticationInfoMessage::Type:
			return OnPluginGetAuthenticationInfoMessage(OpPluginGetAuthenticationInfoMessage::Cast(message));

		default:
			OP_ASSERT(!"PluginInstance received unknown message type!");
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

NPError
PluginInstance::New(NPMIMEType pluginType, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
	/* Look up plug-in and plug-in library. */
	Plugin* plugin = g_pluginhandler->FindPlugin(m_npp);
	RETURN_VALUE_IF_NULL(plugin, NPERR_INVALID_INSTANCE_ERROR);

	m_library = g_pluginhandler->GetPluginLibHandler()->FindPluginLib(plugin->GetName());
	if (!m_library || !m_library->Initialized())
		return NPERR_INVALID_INSTANCE_ERROR;

	/* Create channel to remote instance. */
	OP_ASSERT(!m_remote_instance);
	m_remote_instance = g_opera->CreateChannel(m_library->GetComponent()->GetDestination());
	if (!m_remote_instance ||
		OpStatus::IsError(m_remote_instance->AddMessageListener(this)) ||
		OpStatus::IsError(g_pluginhandler->WhitelistChannel(m_remote_instance)))
		return NPERR_OUT_OF_MEMORY_ERROR;

	/* Build message. */
	OpAutoPtr<OpPluginNewMessage> message(OpPluginNewMessage::Create());
	RETURN_VALUE_IF_NULL(message.get(), NPERR_OUT_OF_MEMORY_ERROR);

	OpChannel* adapter_channel = plugin->GetPluginAdapter() ? plugin->GetPluginAdapter()->GetChannel() : NULL;
	RETURN_VALUE_IF_ERROR(BuildPluginNewMessage(message.get(), pluginType, mode, argc, argn, argv, saved, adapter_channel),
	                      NPERR_OUT_OF_MEMORY_ERROR);

	/* Send message synchronously. */
	PluginSyncMessenger sync(m_remote_instance);
	RETURN_VALUE_IF_ERROR(sync.AcceptResponse(OpPluginNewResponseMessage::Type), NPERR_GENERIC_ERROR);
	OP_STATUS s = sync.SendMessage(message.release());
	m_remote_instance = sync.TakePeer();

	RETURN_VALUE_IF_ERROR(s, NPERR_GENERIC_ERROR);

	return static_cast<OpPluginNewResponseMessage*>(sync.GetResponse())->GetNpError();
}

OP_STATUS
PluginInstance::BuildPluginNewMessage(OpPluginNewMessage* message, NPMIMEType pluginType, uint16 mode, int16 argc,
									  char* argn[], char* argv[], NPSavedData* saved, OpChannel* adapter_channel)
{
	UniString content_type;
	RETURN_IF_ERROR(UniString_UTF8::FromUTF8(content_type, pluginType));

	message->SetContentType(content_type);
	message->SetMode(mode);
	message->SetSaved(reinterpret_cast<UINT64>(saved));

	for (int i = 0; i < argc; i++)
	{
		typedef OpNs4pluginsMessages_MessageSet::PluginNew_Argument Argument;

		OpAutoPtr<Argument> ap (OP_NEW(Argument, ()));
		RETURN_OOM_IF_NULL(ap.get());

		RETURN_IF_ERROR(message->GetArgumentsRef().Add(ap.get()));
		Argument* a = ap.release();

		UniString name;
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(name, argn[i]));
		RETURN_IF_ERROR(a->SetName(name));

		if (argv[i])
		{
			UniString value;
			RETURN_IF_ERROR(UniString_UTF8::FromUTF8(value, argv[i]));
			RETURN_IF_ERROR(a->SetValue(value));
		}
	}

	if (adapter_channel)
	{
		typedef OpNs4pluginsMessages_MessageSet::MessageAddress MessageAddress;
		const OpMessageAddress address = adapter_channel->GetAddress();

		MessageAddress* addr = OP_NEW(MessageAddress, (address.cm, address.co, address.ch));
		RETURN_OOM_IF_NULL(addr);

		message->SetAdapterAddress(addr);
	}

	return OpStatus::OK;
}

NPError
PluginInstance::Destroy(NPSavedData** saved)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	OP_STATUS s = sync.SendMessage(OpPluginDestroyMessage::Create());

	NPError ret = NPERR_GENERIC_ERROR;
	if (OpStatus::IsSuccess(s))
	{
		OpPluginDestroyResponseMessage* response = OpPluginDestroyResponseMessage::Cast(sync.GetResponse());
		ret = response->GetNpError();
		*saved = reinterpret_cast<NPSavedData*>(response->GetSaved());
	}

	return ret;
}

NPError
PluginInstance::GetValue(NPPVariable variable, void* ret_value)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return NPERR_INVALID_INSTANCE_ERROR;

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(OpPluginGetValueMessage::Create(variable))))
		return NPERR_GENERIC_ERROR;

	OpPluginGetValueResponseMessage* response = OpPluginGetValueResponseMessage::Cast(sync.GetResponse());
	if (response->GetNpError() != NPERR_NO_ERROR)
		return response->GetNpError();

	if (response->HasObjectValue())
		*reinterpret_cast<NPObject**>(ret_value) = reinterpret_cast<NPObject*>(response->GetObjectValue()->GetObject());
	else if (response->HasIntegerValue())
		*reinterpret_cast<int*>(ret_value) = response->GetIntegerValue();
	else if (response->HasBooleanValue())
		*reinterpret_cast<NPBool*>(ret_value) = response->GetBooleanValue();
	else
	{
		OP_ASSERT(!"PluginInstance::GetValue received unknown value type.");
		return NPERR_GENERIC_ERROR;
	}

	return NPERR_NO_ERROR;
}

/**
 * Helper class to temporarily enter a new scripting context.
 *
 * See PluginHandler::PushScriptingContext().
 */
struct ScriptingContext
{
	ScriptingContext()  { id = g_pluginhandler->PushScriptingContext(); }
	~ScriptingContext() { g_pluginhandler->PopScriptingContext(); }
	int id;
};

NPError
PluginInstance::SetWindow(NPWindow* window, OpChannel* adapter_channel)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return NPERR_INVALID_INSTANCE_ERROR;

	if (!window)
		return NPERR_INVALID_PARAM;

	OpPluginWindowMessage* message = OpPluginWindowMessage::Create(
		static_cast<UINT64>(reinterpret_cast<uintptr_t>(window->window)),
		window->type);
	if (!message)
		return NPERR_OUT_OF_MEMORY_ERROR;

	ScriptingContext context;
	message->SetContext(context.id);

	message->GetRectRef().SetX(window->x);
	message->GetRectRef().SetY(window->y);
	message->GetRectRef().SetWidth(window->width);
	message->GetRectRef().SetHeight(window->height);

	SetPluginRect(message->GetClipRectRef(), &window->clipRect);

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message, false, 0, context.id)))
		return NPERR_GENERIC_ERROR;

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

NPError
PluginInstance::NewStream(NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return NPERR_INVALID_INSTANCE_ERROR;

	if (!type || !stype || !stream)
		return NPERR_INVALID_PARAM;

	OpPluginNewStreamMessage* message = OpPluginNewStreamMessage::Create();
	if (!message)
		return NPERR_OUT_OF_MEMORY_ERROR;

	UniString tmp;
	if (OpStatus::IsError(UniString_UTF8::FromUTF8(tmp, type)) ||
		OpStatus::IsError(message->SetContentType(tmp)))
	{
		OP_DELETE(message);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	message->SetSeekable(seekable);
	message->SetStreamType(*stype);

	if (OpStatus::IsError(SetPluginStream(message->GetStreamRef(), stream)))
	{
		OP_DELETE(message);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message)))
		return NPERR_GENERIC_ERROR;

	OpPluginNewStreamResponseMessage* response = OpPluginNewStreamResponseMessage::Cast(sync.GetResponse());
	*stype = response->GetStreamType();
	return response->GetNpError();
}

NPError
PluginInstance::DestroyStream(NPStream* stream, NPReason reason)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return NPERR_INVALID_INSTANCE_ERROR;

	if (!stream)
		return NPERR_INVALID_PARAM;

	OpPluginDestroyStreamMessage* message = OpPluginDestroyStreamMessage::Create(reason);
	if (!message)
		return NPERR_OUT_OF_MEMORY_ERROR;

	if (OpStatus::IsError(SetPluginStream(message->GetStreamRef(), stream)))
	{
		OP_DELETE(message);
		return NPERR_OUT_OF_MEMORY_ERROR;
	}

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message)))
		return NPERR_GENERIC_ERROR;

	return OpPluginErrorMessage::Cast(sync.GetResponse())->GetNpError();
}

int32
PluginInstance::WriteReady(NPStream* stream)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return -1;

	if (!stream)
		return -1;

	OpPluginWriteReadyMessage* message = OpPluginWriteReadyMessage::Create();
	if (!message)
		return -1;

	if (OpStatus::IsError(SetPluginStream(message->GetStreamRef(), stream)))
	{
		OP_DELETE(message);
		return -1;
	}

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message)))
		return -1;

	return OpPluginWriteReadyResponseMessage::Cast(sync.GetResponse())->GetBytes();
}

int32
PluginInstance::Write(NPStream* stream, int32 offset, int32 len, void* buffer)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return -1;

	if (!stream || !buffer)
		return -1;

	OpPluginWriteMessage* message = OpPluginWriteMessage::Create(offset);
	if (!message)
		return -1;

	if (OpStatus::IsError(SetPluginStream(message->GetStreamRef(), stream)))
	{
		OP_DELETE(message);
		return -1;
	}

	OpData buffer_data;
	if (OpStatus::IsError(buffer_data.SetConstData(static_cast<char*>(buffer), len)))
	{
		OP_DELETE(message);
		return -1;
	}
	message->SetBuffer(buffer_data);

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(message)))
		return -1;

	return OpPluginWriteResponseMessage::Cast(sync.GetResponse())->GetBytes();
}

void
PluginInstance::StreamAsFile(NPStream* stream, const char* fname)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected() || !stream)
		return;

	/* This functions suppresses various errors like OOM etc because
	 * the NPAPI spec uses "void" as return type for NPP_StreamAsFile.
	 * Ideally, the NPAPI spec should be changed so we can fix this. */

	OpAutoPtr<OpPluginStreamAsFileMessage> message(OpPluginStreamAsFileMessage::Create());
	if (!message.get())
		return;

	if (OpStatus::IsError(SetPluginStream(message->GetStreamRef(), stream)))
		return;

	/* Path can be a NULL pointer. */
	if (fname && OpStatus::IsError(message->SetFname(fname, OpDataUnknownLength)))
		return;

	/* While there is no return value, we still perform a synchronous request in order to
	 * maintain the lock set by STREAMASFILE in PluginHandler::HandleMessage(). */
	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	OpStatus::Ignore(sync.SendMessage(message.release()));
}

OP_STATUS
PluginInstance::HandleWindowlessPaint(OpPainter* painter, const OpRect& plugin_rect, const OpRect& paint_rect, bool transparent, OpWindow* parent_opwindow)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OP_ASSERT(painter || !"PluginInstance::HandleWindowlessPaint: Missing painter");

	// No need to send anything if the paint rectangle is empty
	if (paint_rect.width <= 0 || paint_rect.height <= 0)
		return OpStatus::OK;

	OpPluginWindowlessPaintMessage* message = OpPluginWindowlessPaintMessage::Create();
	RETURN_OOM_IF_NULL(message);

	ScriptingContext context;
	message->SetContext(context.id);

	if (!m_plugin_frame)
		RETURN_IF_ERROR(OpPluginImage::Create(&m_plugin_frame, &m_plugin_background, plugin_rect, parent_opwindow));

	RETURN_IF_ERROR(m_plugin_frame->Update(painter, plugin_rect, transparent));
	message->SetOpPluginImageGlobalIdentifier(m_plugin_frame->GetGlobalIdentifier());

	if (transparent && m_plugin_background)
		message->SetOpPluginBackgroundGlobalIdentifier(m_plugin_background->GetGlobalIdentifier());

	SetPluginRect(&message->GetPaintRectRef(), paint_rect);

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	RETURN_IF_ERROR(sync.SendMessage(message, false, 0, context.id));

	if (OpPluginWindowlessHandleEventResponseMessage::Cast(sync.GetResponse())->GetIsEventHandled())
		RETURN_IF_ERROR(m_plugin_frame->Draw(painter, plugin_rect, paint_rect));

	return OpStatus::OK;
}

OP_STATUS
PluginInstance::HandleWindowlessMouseEvent(OpPluginEventType event_type, const OpPoint& point,
                                           int button, ShiftKeyState modifiers, bool* is_event_handled)
{
	OP_ASSERT(is_event_handled);
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpPluginWindowlessMouseEventMessage* msg = OpPluginWindowlessMouseEventMessage::Create();
	RETURN_OOM_IF_NULL(msg);

	msg->SetEventType(event_type);
	SetPluginPoint(&msg->GetPointRef(), point);
	msg->SetButton(button);
	msg->SetModifiers(modifiers);

	/* Send async message on right click to allow plugin to paint when context
	   menu is open and not block the message loop. */
	if (button == MOUSE_BUTTON_2 && (event_type == PLUGIN_MOUSE_DOWN_EVENT || event_type == PLUGIN_MOUSE_UP_EVENT))
	{
		RETURN_IF_ERROR(m_remote_instance->SendMessage(msg));
		*is_event_handled = true;
	}
	else
	{
		PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
		RETURN_IF_ERROR(sync.SendMessage(msg));

		*is_event_handled = !!OpPluginWindowlessHandleEventResponseMessage::Cast(sync.GetResponse())->GetIsEventHandled();
	}

	return OpStatus::OK;
}

OP_STATUS
PluginInstance::HandleWindowlessKeyEvent(const OpKey::Code key, const uni_char *key_value, const OpPluginKeyState key_state, const ShiftKeyState modifiers,
										 UINT64 platform_data_1, UINT64 platform_data_2, INT32* is_event_handled)
{
	OP_ASSERT(is_event_handled);
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpPluginWindowlessKeyEventMessage* msg = OpPluginWindowlessKeyEventMessage::Create(key, key_value?*key_value:0, key_state, modifiers);
	RETURN_OOM_IF_NULL(msg);

	msg->SetPlatformData1(platform_data_1);
	msg->SetPlatformData2(platform_data_2);

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	RETURN_IF_ERROR(sync.SendMessage(msg));

	*is_event_handled = OpPluginWindowlessHandleEventResponseMessage::Cast(sync.GetResponse())->GetIsEventHandled();

	return OpStatus::OK;
}

OP_STATUS
PluginInstance::HandleFocusEvent(bool focus, FOCUS_REASON reason, bool* is_event_handled)
{
	OP_ASSERT(is_event_handled);
	if (!m_remote_instance || !m_remote_instance->IsDirected())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpPluginFocusEventMessage* message = OpPluginFocusEventMessage::Create(focus, reason);
	RETURN_OOM_IF_NULL(message);

	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	RETURN_IF_ERROR(sync.SendMessage(message));

	*is_event_handled = !!OpPluginWindowlessHandleEventResponseMessage::Cast(sync.GetResponse())->GetIsEventHandled();

	return OpStatus::OK;
}

void
PluginInstance::URLNotify(const char* url, NPReason reason, void* notifyData)
{
	if (!m_remote_instance || !m_remote_instance->IsDirected() || !url)
		return;

	OpPluginNotifyMessage* message = OpPluginNotifyMessage::Create();
	if (!message)
		return;

	message->SetReason(reason);
	message->SetNotifyData(reinterpret_cast<UINT64>(notifyData));

	UniString tmp;
	if (OpStatus::IsError(UniString_UTF8::FromUTF8(tmp, url)) ||
		OpStatus::IsError(message->SetURL(tmp)))
	{
		OP_DELETE(message);
		return;
	}

	/* The reason this is done synchronously, is so that runtime behaviour does
	 * not deviate from previous incarnations. There are subtle timing issues. */
	PluginSyncMessenger sync(g_opera->CreateChannel(m_remote_instance->GetDestination()));
	OpStatus::Ignore(sync.SendMessage(message));
}

/* static */ bool
PluginInstance::IsBooleanValue(NPNVariable variable)
{
	return variable == NPNVjavascriptEnabledBool
		|| variable == NPNVasdEnabledBool
		|| variable == NPNVisOfflineBool
		|| variable == NPNVSupportsXEmbedBool
		|| variable == NPNVSupportsWindowless
		|| variable == NPNVprivateModeBool
		|| variable == NPNVsupportsAdvancedKeyHandling
#ifdef XP_MACOSX
		|| variable == NPNVsupportsCoreGraphicsBool
		|| variable == NPNVsupportsOpenGLBool
		|| variable == NPNVsupportsCoreAnimationBool
		|| variable == NPNVsupportsInvalidatingCoreAnimationBool
#ifndef NP_NO_CARBON
		|| variable == NPNVsupportsCarbonBool
#endif // !NP_NO_CARBON
		|| variable == NPNVsupportsCocoaBool
		|| variable == NPNVsupportsUpdatedCocoaTextInputBool
		|| variable == NPNVsupportsCompositingCoreAnimationPluginsBool
#endif // XP_MACOSX
		|| false;
}

/* static */ bool
PluginInstance::IsIntegerValue(NPNVariable variable)
{
#ifdef XP_MACOSX
	return variable == NPNVpluginDrawingModel;
#else
	return false;
#endif // XP_MACOSX
}

/* static */ bool
PluginInstance::IsStringValue(NPNVariable variable)
{
	return variable == NPNVdocumentOrigin;
}

/* static */ bool
PluginInstance::IsObjectValue(NPNVariable variable)
{
	return variable == NPNVWindowNPObject
		|| variable == NPNVPluginElementNPObject
		|| variable == NPNVserviceManager;
}

/* static */ OP_STATUS
PluginInstance::HandlePluginGetValueMessage(NPP npp, OpPluginGetValueMessage* message)
{
	NPError error = NPERR_INVALID_PARAM;
	NPNVariable variable = static_cast<NPNVariable>(message->GetVariable());

	OpPluginGetValueResponseMessage* response = OpPluginGetValueResponseMessage::Create(error);
	RETURN_OOM_IF_NULL(response);

	if (IsBooleanValue(variable))
	{
		BOOL value = FALSE;
		error = NPN_GetValue(npp, variable, &value);
		if (error == NPERR_NO_ERROR)
			response->SetBooleanValue(value);
	}
	else if (IsIntegerValue(variable))
	{
		int value;
		error = NPN_GetValue(npp, variable, &value);
		if (error == NPERR_NO_ERROR)
			response->SetIntegerValue(value);
	}
	else if (IsStringValue(variable))
	{
		NPUTF8* value;
		error = NPN_GetValue(npp, variable, &value);
		if (error == NPERR_NO_ERROR && value)
		{
			UniString u_value;
			RETURN_IF_ERROR(UniString_UTF8::FromUTF8(u_value, value));
			response->SetStringValue(u_value);
			NPN_MemFree(value);
		}
	}
	else if (IsObjectValue(variable))
	{
		NPObject* np_object;
		error = NPN_GetValue(npp, variable, &np_object);

		if (error == NPERR_NO_ERROR)
		{
			if (!np_object)
				error = NPERR_GENERIC_ERROR;
			else
			{
				typedef OpNs4pluginsMessages_MessageSet::PluginObject PluginObject;

				PluginObject* object = OP_NEW(PluginObject, ());
				if (!object)
				{
					OP_DELETE(response);
					return OpStatus::ERR_NO_MEMORY;
				}

				object->SetObject(reinterpret_cast<UINT64>(np_object));
				object->SetReferenceCount(np_object->referenceCount);

				response->SetObjectValue(object);
			}
		}
	}

	response->SetNpError(error);
	return message->Reply(response);
}

OP_STATUS
PluginInstance::OnPluginSetValueMessage(OpPluginSetValueMessage* message)
{
	NPPVariable variable = static_cast<NPPVariable>(message->GetVariable());
	void* value = NULL;

	if (message->HasObjectValue())
		*reinterpret_cast<NPObject**>(&value) = reinterpret_cast<NPObject*>(message->GetObjectValue()->GetObject());
	else if (message->HasIntegerValue())
		*reinterpret_cast<int*>(&value) = message->GetIntegerValue();
	else if (message->HasBooleanValue())
		*reinterpret_cast<bool*>(&value) = !!message->GetBooleanValue();
	else
	{
		/* Unknown variable. */
		return OpStatus::ERR;
	}

	NPError error = NPN_SetValue(m_npp, variable, value);

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginInstance::OnPluginEvaluateMessage(OpPluginEvaluateMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPString script;
	RETURN_IF_ERROR(SetNPString(&script, message->GetScript()));

	NPObject* np_object = NULL;
	if (message->HasObject())
		np_object = reinterpret_cast<NPObject*>(message->GetObject()->GetObject());

	NPVariant result;
	bool success = NPN_Evaluate(m_npp, np_object, &script, &result);

	return message->Reply(BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginInstance::OnPluginGetPropertyMessage(OpPluginGetPropertyMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPIdentifier property = reinterpret_cast<NPIdentifier>(message->GetProperty().GetIdentifier());
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	NPVariant result;
	bool success = NPN_GetProperty(m_npp, np_object, property, &result);

	return message->Reply(BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginInstance::OnPluginSetPropertyMessage(OpPluginSetPropertyMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPIdentifier property = reinterpret_cast<NPIdentifier>(message->GetProperty().GetIdentifier());
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	NPVariant value;
	RETURN_IF_ERROR(SetNPVariant(&value, message->GetValue()));

	bool success = NPN_SetProperty(m_npp, np_object, property, &value);

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginInstance::OnPluginHasMethodMessage(OpPluginHasMethodMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPIdentifier method = reinterpret_cast<NPIdentifier>(message->GetMethod().GetIdentifier());
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	bool has_method = NPN_HasMethod(m_npp, np_object, method);

	return message->Reply(OpPluginResultMessage::Create(has_method));
}

OP_STATUS
PluginInstance::OnPluginCreateObjectMessage(OpPluginCreateObjectMessage* message)
{
	OpPluginCreateObjectResponseMessage* response = OpPluginCreateObjectResponseMessage::Create();
	RETURN_OOM_IF_NULL(response);

	if (NPObject* np_object = NPN_CreateObject(m_npp, m_library->GetPluginObjectClass()))
		if (PluginObject* object = OP_NEW(PluginObject, ()))
		{
			object->SetObject(reinterpret_cast<UINT64>(np_object));
			object->SetReferenceCount(np_object->referenceCount);

			response->SetObject(object);
		}

	return message->Reply(response);
}

OP_STATUS
PluginInstance::OnPluginInvokeMessage(OpPluginInvokeMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPIdentifier method = reinterpret_cast<NPIdentifier>(message->GetMethod().GetIdentifier());
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	NPVariant* args = BuildNPVariantArray(message->GetArguments());
	RETURN_OOM_IF_NULL(args);

	NPVariant result;
	bool success = NPN_Invoke(m_npp, np_object, method, args, message->GetArguments().GetCount(), &result);

	ReleaseNPVariantArray(args, message->GetArguments().GetCount());

	return message->Reply(BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginInstance::OnPluginInvokeDefaultMessage(OpPluginInvokeDefaultMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	NPVariant* args = BuildNPVariantArray(message->GetArguments());
	RETURN_OOM_IF_NULL(args);

	NPVariant result;
	bool success = NPN_InvokeDefault(m_npp, np_object, args, message->GetArguments().GetCount(), &result);

	ReleaseNPVariantArray(args, message->GetArguments().GetCount());

	return message->Reply(BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginInstance::OnPluginHasPropertyMessage(OpPluginHasPropertyMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPIdentifier property = reinterpret_cast<NPIdentifier>(message->GetProperty().GetIdentifier());
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	bool success = NPN_HasProperty(m_npp, np_object, property);

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginInstance::OnPluginRemovePropertyMessage(OpPluginRemovePropertyMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPIdentifier property = reinterpret_cast<NPIdentifier>(message->GetProperty().GetIdentifier());
	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	bool success = NPN_RemoveProperty(m_npp, np_object, property);

	return message->Reply(OpPluginResultMessage::Create(success));
}

OP_STATUS
PluginInstance::OnPluginObjectEnumerateMessage(OpPluginObjectEnumerateMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	NPIdentifier* identifiers;
	uint32_t count;
	bool success = NPN_Enumerate(m_npp, np_object, &identifiers, &count);

	OpAutoPtr<OpPluginObjectEnumerateResponseMessage> response(OpPluginObjectEnumerateResponseMessage::Create(success));
	RETURN_OOM_IF_NULL(response.get());

	if (success)
	{
		for (unsigned int i = 0; i < count; i++)
		{
			typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;

			PluginIdentifier* id = OP_NEW(PluginIdentifier, ());
			if (!id)
			{
				NPN_MemFree(identifiers);
				return OpStatus::ERR_NO_MEMORY;
			}

			id->SetIdentifier(reinterpret_cast<UINT64>(identifiers[i]));
			id->SetIsString(NPN_IdentifierIsString(identifiers[i]));

			OP_STATUS s;
			if (OpStatus::IsError(s = response->GetPropertiesRef().Add(id)))
			{
				NPN_MemFree(identifiers);
				return s;
			}
		}

		NPN_MemFree(identifiers);
	}

	return message->Reply(response.release());
}

OP_STATUS
PluginInstance::OnPluginObjectConstructMessage(OpPluginObjectConstructMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	NPObject* np_object = reinterpret_cast<NPObject*>(message->GetObject().GetObject());

	NPVariant* args = BuildNPVariantArray(message->GetArguments());
	RETURN_OOM_IF_NULL(args);

	NPVariant result;
	bool success = NPN_Construct(m_npp, np_object, args, message->GetArguments().GetCount(), &result);

	ReleaseNPVariantArray(args, message->GetArguments().GetCount());

	return message->Reply(BuildPluginResultMessage(success, &result));
}

OP_STATUS
PluginInstance::OnPluginGetURLMessage(OpPluginGetURLMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	const char* url;
	RETURN_IF_ERROR(message->GetURL().CreatePtr(&url, 1));

	const char* window = NULL;
	if (message->HasWindow())
		RETURN_IF_ERROR(message->GetWindow().CreatePtr(&window, 1));

	NPError error = NPN_GetURL(m_npp, url, window);

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginInstance::OnPluginGetURLNotifyMessage(OpPluginGetURLNotifyMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	const char* url;
	RETURN_IF_ERROR(message->GetURL().CreatePtr(&url, 1));

	const char* window = NULL;
	if (message->HasWindow())
		RETURN_IF_ERROR(message->GetWindow().CreatePtr(&window, 1));

	NPError error = NPN_GetURLNotify(m_npp, url, window, reinterpret_cast<void*>(message->GetNotifyData()));

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginInstance::OnPluginPostURLMessage(OpPluginPostURLMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	const char* url;
	RETURN_IF_ERROR(message->GetURL().CreatePtr(&url, 1));

	const char* window = NULL;
	if (message->HasWindow())
		RETURN_IF_ERROR(message->GetWindow().CreatePtr(&window, 1));

	const char* data;
	RETURN_IF_ERROR(message->GetData().CreatePtr(&data));

	NPError error = NPN_PostURL(m_npp, url, window, message->GetData().Length(),
								data, message->GetIsFile());

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginInstance::OnPluginPostURLNotifyMessage(OpPluginPostURLNotifyMessage* message)
{
	PluginChannelWhitelistToggle enabled(false);

	const char* url;
	RETURN_IF_ERROR(message->GetURL().CreatePtr(&url, 1));

	const char* window = NULL;
	if (message->HasWindow())
		RETURN_IF_ERROR(message->GetWindow().CreatePtr(&window, 1));

	const char* data;
	RETURN_IF_ERROR(message->GetData().CreatePtr(&data));

	NPError error = NPN_PostURLNotify(m_npp, url, window, message->GetData().Length(),
									  data, message->GetIsFile(),
									  reinterpret_cast<void*>(message->GetNotifyData()));

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginInstance::OnPluginDestroyStreamMessage(OpPluginDestroyStreamMessage* message)
{
	NPStream* stream = reinterpret_cast<NPStream*>(message->GetStreamRef().GetIdentifier());
	NPReason reason = static_cast<NPReason>(message->GetReason());

	/* This is an asynchronous message, and NPERR_NO_ERROR has already been returned to the plug-in,
	 * so we ignore the return value. See BrowserFunctions::NPN_DestroyStream(). */
	NPN_DestroyStream(m_npp, stream, reason);

	return OpStatus::OK;
}

OP_STATUS
PluginInstance::OnPluginUserAgentMessage(OpPluginUserAgentMessage* message)
{
	const char* user_agent = NPN_UserAgent(m_npp);
	RETURN_OOM_IF_NULL(user_agent);

	OpPluginUserAgentResponseMessage* response = OpPluginUserAgentResponseMessage::Create();
	RETURN_OOM_IF_NULL(response);

	UniString ua;
	if (OpStatus::IsError(UniString_UTF8::FromUTF8(ua, user_agent)) ||
		OpStatus::IsError(response->SetUserAgent(ua)))
	{
		OP_DELETE(response);
		return OpStatus::ERR_NO_MEMORY;
	}

	return message->Reply(response);
}

OP_STATUS
PluginInstance::OnPluginInvalidateMessage(OpPluginInvalidateMessage* message)
{
	NPRect rect;
	SetNPRect(&rect, message->GetRect());

	NPN_InvalidateRect(m_npp, &rect);

	return OpStatus::OK;
}

OP_STATUS
PluginInstance::OnPluginStatusTextMessage(OpPluginStatusTextMessage* message)
{
	char* text;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&text, message->GetText()));

	NPN_Status(m_npp, text);

	OP_DELETEA(text);
	return OpStatus::OK;
}

OP_STATUS
PluginInstance::OnPluginGetValueForURLMessage(OpPluginGetValueForURLMessage* message)
{
	char* url_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&url_, message->GetURL()));
	OpAutoArray<char> url(url_);

	char* value;
	uint32_t len;
	NPError error = NPN_GetValueForURL(m_npp, static_cast<NPNURLVariable>(message->GetVariable()), url.get(), &value, &len);

	OpAutoPtr<OpPluginGetValueForURLResponseMessage> response(OpPluginGetValueForURLResponseMessage::Create(error));
	RETURN_OOM_IF_NULL(response.get());

	if (error == NPERR_NO_ERROR)
	{
		UniString v;
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(v, value, len));
		RETURN_IF_ERROR(response->SetValue(v));

		NPN_MemFree(value);
	}

	return message->Reply(response.release());
}

OP_STATUS
PluginInstance::OnPluginSetValueForURLMessage(OpPluginSetValueForURLMessage* message)
{
	char* url_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&url_, message->GetURL()));
	OpAutoArray<char> url(url_);

	char* value_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&value_, message->GetValue()));
	OpAutoArray<char> value(value_);

	NPError error = NPN_SetValueForURL(m_npp, static_cast<NPNURLVariable>(message->GetVariable()), url.get(), value.get(), message->GetValue().Length());

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginInstance::OnPluginPushPopupsEnabledMessage(OpPluginPushPopupsEnabledMessage* message)
{
	NPN_PushPopupsEnabledState(m_npp, message->GetEnabled());
	return message->Reply(OpPluginErrorMessage::Create(NPERR_NO_ERROR));
}

OP_STATUS
PluginInstance::OnPluginPopPopupsEnabledMessage(OpPluginPopPopupsEnabledMessage* message)
{
	NPN_PopPopupsEnabledState(m_npp);
	return message->Reply(OpPluginErrorMessage::Create(NPERR_NO_ERROR));
}

OP_STATUS
PluginInstance::OnPluginGetAuthenticationInfoMessage(OpPluginGetAuthenticationInfoMessage* message)
{
	char* protocol_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&protocol_, message->GetProtocol()));
	OpAutoArray<char> protocol(protocol_);

	char* host_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&host_, message->GetHost()));
	OpAutoArray<char> host(host_);

	char* scheme_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&scheme_, message->GetScheme()));
	OpAutoArray<char> scheme(scheme_);

	char* realm_;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&realm_, message->GetRealm()));
	OpAutoArray<char> realm(realm_);

	char* username = NULL;
	uint32_t ulen = 0;
	char* password = NULL;
	uint32_t plen = 0;

	NPError error = NPN_GetAuthenticationInfo(m_npp, protocol.get(), host.get(), message->GetPort(),
											  scheme.get(), realm.get(), &username, &ulen, &password, &plen);

	OpAutoPtr<OpPluginGetAuthenticationInfoResponseMessage> response(OpPluginGetAuthenticationInfoResponseMessage::Create(error));
	RETURN_OOM_IF_NULL(response.get());

	if (error == NPERR_NO_ERROR)
	{
		UniString s;
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(s, username, ulen));
		RETURN_IF_ERROR(response->SetUsername(s));
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(s, password, plen));
		RETURN_IF_ERROR(response->SetPassword(s));
	}

	return message->Reply(response.release());
}

/* static */ OP_STATUS
PluginInstance::SetNPVariant(NPVariant* out_variant, const PluginVariant& variant)
{
	if (variant.HasVoidValue())
		out_variant->type = NPVariantType_Void;
	else if (variant.HasNullValue())
		out_variant->type = NPVariantType_Null;
	else if (variant.HasBoolValue())
	{
		out_variant->type = NPVariantType_Bool;
		out_variant->value.boolValue = !!variant.GetBoolValue();
	}
	else if (variant.HasIntValue())
	{
		out_variant->type = NPVariantType_Int32;
		out_variant->value.intValue = variant.GetIntValue();
	}
	else if (variant.HasDoubleValue())
	{
		out_variant->type = NPVariantType_Double;
		out_variant->value.doubleValue = variant.GetDoubleValue();
	}
	else if (variant.HasStringValue())
	{
		out_variant->type = NPVariantType_String;
		RETURN_IF_ERROR(SetNPString(&out_variant->value.stringValue, variant.GetStringValue()));
	}
	else if (variant.HasObjectValue())
	{
		out_variant->type = NPVariantType_Object;
		RETURN_OOM_IF_NULL(out_variant->value.objectValue = reinterpret_cast<NPObject*>(
							   variant.GetObjectValue()->GetObject()));
		NPN_RetainObject(out_variant->value.objectValue);
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
PluginInstance::SetPluginVariant(PluginVariant& out_variant, const NPVariant& variant)
{
	out_variant = PluginVariant();

	switch (variant.type)
	{
		case NPVariantType_Null:
			out_variant.SetNullValue(true);
			break;

		case NPVariantType_Bool:
			out_variant.SetBoolValue(variant.value.boolValue);
			break;

		case NPVariantType_Int32:
			out_variant.SetIntValue(variant.value.intValue);
			break;

		case NPVariantType_Double:
			out_variant.SetDoubleValue(variant.value.doubleValue);
			break;

		case NPVariantType_String:
		{
			UniString s;
			RETURN_IF_ERROR(UniString_UTF8::FromUTF8(s, variant.value.stringValue.UTF8Characters,
													 variant.value.stringValue.UTF8Length));
			out_variant.SetStringValue(s);
			break;
		}

		case NPVariantType_Object:
		{
			PluginObject* object = OP_NEW(PluginObject, ());
			RETURN_OOM_IF_NULL(object);
			object->SetObject(reinterpret_cast<UINT64>(variant.value.objectValue));
			object->SetReferenceCount(variant.value.objectValue->referenceCount);
			out_variant.SetObjectValue(object);
			break;
		}

		case NPVariantType_Void:
		default:
			out_variant.SetVoidValue(true);
			break;
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
PluginInstance::SetNPString(NPString* out_string, const UniString& string)
{
	out_string->UTF8Length = 0;
	out_string->UTF8Characters = NULL;

	char* utf8_string_ = NULL;
	RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&utf8_string_, string));
	OpAutoArray<char> utf8_string(utf8_string_);
	size_t utf8_string_len = op_strlen(utf8_string_);

	/* Make a copy of the string that is allocated by NPN_MemAlloc. This is a requirement of NPStrings. */
	out_string->UTF8Characters = static_cast<NPUTF8*>(NPN_MemAlloc(utf8_string_len + 1));
	if (!out_string->UTF8Characters)
		return OpStatus::ERR_NO_MEMORY;

	out_string->UTF8Length = utf8_string_len;
	op_memcpy(const_cast<NPUTF8*>(out_string->UTF8Characters), utf8_string_, utf8_string_len);
	const_cast<NPUTF8*>(out_string->UTF8Characters)[utf8_string_len] = '\0';

	return OpStatus::OK;
}

/* static */ OpPluginResultMessage*
PluginInstance::BuildPluginResultMessage(bool success, const NPVariant* result)
{
	OpPluginResultMessage* message = OpPluginResultMessage::Create(success);

	if (message && success && result)
	{
		PluginVariant* res = OP_NEW(PluginVariant, ());
		if (!res || OpStatus::IsError(SetPluginVariant(*res, *result)))
		{
			OP_DELETE(message);
			return NULL;
		}

		message->SetResult(res);
	}

	return message;
}

/* static */ NPVariant*
PluginInstance::BuildNPVariantArray(const OpProtobufMessageVector<PluginVariant>& arguments)
{
	NPVariant* args = OP_NEWA(NPVariant, arguments.GetCount());
	if (!args)
		return NULL;

	unsigned int i;
	for (i = 0; i < arguments.GetCount(); i++)
		if (OpStatus::IsError(SetNPVariant(&args[i], *arguments.Get(i))))
			break;

	if (i < arguments.GetCount())
	{
		ReleaseNPVariantArray(args, i);
		return NULL;
	}

	return args;
}

/* static */ void
PluginInstance::ReleaseNPVariantArray(NPVariant* args, unsigned int count)
{
	for (unsigned int i = 0; i < count; i++)
		NPN_ReleaseVariantValue(&args[i]);

	OP_DELETEA(args);
}

/* static */ OP_STATUS
PluginInstance::SetPluginStream(PluginStream& out_stream, const NPStream* stream)
{
	out_stream.SetIdentifier(reinterpret_cast<UINT64>(stream));
	out_stream.SetEnd(stream->end);
	out_stream.SetLastModified(stream->lastmodified);
	out_stream.SetNotifyData(reinterpret_cast<UINT64>(stream->notifyData));

	UniString tmp;

	RETURN_IF_ERROR(UniString_UTF8::FromUTF8(tmp, stream->url));
	RETURN_IF_ERROR(out_stream.SetURL(tmp));

	if (stream->headers)
	{
		RETURN_IF_ERROR(UniString_UTF8::FromUTF8(tmp, stream->headers));
		RETURN_IF_ERROR(out_stream.SetHeaders(tmp));
	}

	return OpStatus::OK;
}

/* static */ void
PluginInstance::SetPluginRect(PluginRect& out_rect, const NPRect* rect)
{
	out_rect.SetX(rect->left);
	out_rect.SetY(rect->top);
	out_rect.SetWidth(rect->right - rect->left);
	out_rect.SetHeight(rect->bottom - rect->top);
}

/* static */ void
PluginInstance::SetPluginRect(PluginRect* out_rect, const OpRect& rect)
{
	out_rect->SetX(rect.x);
	out_rect->SetY(rect.y);
	out_rect->SetWidth(rect.width);
	out_rect->SetHeight(rect.height);
}

/* static */ void
PluginInstance::SetNPRect(NPRect* out_rect, const PluginRect& rect)
{
	out_rect->left = rect.GetX();
	out_rect->top = rect.GetY();
	out_rect->right = rect.GetX() + rect.GetWidth();
	out_rect->bottom = rect.GetY() + rect.GetHeight();
}

/* static */ void
PluginInstance::SetPluginPoint(PluginPoint* out_point, const OpPoint& point)
{
	out_point->SetX(point.x);
	out_point->SetY(point.y);
}

/* static */ OpPoint
PluginInstance::OpPointFromPluginPoint(PluginPoint& point)
{
	return OpPoint(point.GetX(), point.GetY());
}

/* static */ void
PluginInstance::SetPluginObject(PluginObject& out_object, const NPObject* object)
{
	out_object.SetObject(reinterpret_cast<UINT64>(object));
	out_object.SetReferenceCount(object->referenceCount);
}

#endif // NS4P_COMPONENT_PLUGINS
