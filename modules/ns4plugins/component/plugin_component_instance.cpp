/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/hardcore/component/Messages.h"
#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/ns4plugins/component/browser_functions.h"
#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/plugin_component_instance.h"
#include "modules/ns4plugins/component/plugin_component_helpers.h"
#include "modules/opdata/UniStringUTF8.h"
#include "modules/pi/OpThreadTools.h"


PluginComponentInstance::PluginComponentInstance(PluginComponent* component)
	: m_component(component)
	, m_channel(NULL)
	, m_is_destroyed(false)
	, m_messages_being_processed(0)
	, m_translator(NULL)
	, m_user_agent(NULL)
	, m_next_timer_id(1)
	, m_use_count(0)
	, m_requests_allowed(true)
{
	m_npp.pdata = m_npp.ndata = NULL;
}

PluginComponentInstance::~PluginComponentInstance()
{
	if (m_component)
		m_component->OnInstanceDestroyed(this);

	OP_DELETE(m_channel);
	OP_DELETE(m_translator);
	BrowserFunctions::NPN_MemFree(const_cast<char*>(m_user_agent));
	m_timers.DeleteAll();
}

OP_STATUS
PluginComponentInstance::Construct(OpMessageAddress plugin_instance, char* content_type, int mode,
								   int argc, char* argn[], char* argv[], NPSavedData* saved,
								   OpMessageAddress* adapter_address)
{
	CountInstanceUse of(this);

	/* Create and connect channel to PluginInstance. */
	m_plugin_instance_address = plugin_instance;
	m_channel = g_component->CreateChannel(m_plugin_instance_address);
	RETURN_OOM_IF_NULL(m_channel);
	RETURN_IF_ERROR(m_channel->AddMessageListener(this));
	RETURN_IF_ERROR(m_channel->Connect());

	m_npp.pdata = NULL;
	m_npp.ndata = this;

	RETURN_IF_ERROR(OpPluginTranslator::Create(&m_translator, m_channel->GetAddress(), adapter_address, GetHandle()));

	int error = NPERR_GENERIC_ERROR;
	if (m_component->GetPluginFunctions()->newp)
		error = m_component->GetPluginFunctions()->newp(content_type, GetHandle(), mode, argc, argn, argv, saved);

	return m_channel->SendMessage(OpPluginNewResponseMessage::Create(error));
}

void
PluginComponentInstance::SetRequestsAllowed(bool requests_allowed)
{
	m_requests_allowed = requests_allowed;

	if (!m_requests_allowed && m_channel->IsDirected())
		m_channel->Undirect();

	if (m_requests_allowed && !m_channel->IsDirected())
		m_channel->Direct(m_plugin_instance_address);
}

NPError
PluginComponentInstance::GetValue(NPPVariable variable, void* value)
{
	CountInstanceUse of(this);

	if (m_is_destroyed || !m_component || !m_component->GetPluginFunctions()->getvalue)
		return NPERR_GENERIC_ERROR;

	RETURN_VALUE_IF_NULL(value, NPERR_INVALID_PARAM);

	return m_component->GetPluginFunctions()->getvalue(GetHandle(), variable, value);
}

int16
PluginComponentInstance::HandleEvent(void* event)
{
	CountInstanceUse of(this);

	if (m_is_destroyed || !m_component || !m_component->GetPluginFunctions()->event)
		return 0;

	return m_component->GetPluginFunctions()->event(GetHandle(), event);
}

NPObject*
PluginComponentInstance::RegisterObject(NPObject* object)
{
	CountInstanceUse of(this);

	if (m_is_destroyed || !GetChannel())
		return NULL;

	OpSyncMessenger sync(GetComponent()->CreateChannel(GetChannel()->GetDestination()));
	if (OpStatus::IsError(sync.SendMessage(OpPluginCreateObjectMessage::Create())))
		return NULL;

	OpPluginCreateObjectResponseMessage* response = OpPluginCreateObjectResponseMessage::Cast(sync.GetResponse());
	if (!response->HasObject())
		return NULL;

	return GetComponent()->Bind(*response->GetObject(), object);
}

unsigned int
PluginComponentInstance::ScheduleTimer(unsigned int interval, bool repeat, void (*func)(NPP npp, uint32_t timerID))
{
	CountInstanceUse of(this);

	if (m_is_destroyed || !m_channel || !m_component)
		return 0;

	if (Timer* t = OP_NEW(Timer, ()))
	{
		t->func = func;
		t->interval = interval;
		t->repeat = repeat;

		if (OpStatus::IsError(m_timers.Add(m_next_timer_id, t)))
		{
			OP_DELETE(t);
			return 0;
		}

		if (OpPluginTimerMessage* message = OpPluginTimerMessage::Create(m_next_timer_id))
		{
			message->SetSrc(m_channel->GetAddress());
			message->SetDst(m_channel->GetAddress());
			message->SetDueTime(message->GetDueTime() + t->interval);

			if (OpStatus::IsSuccess(m_component->SendMessage(message)))
				return m_next_timer_id++;
		}

		UnscheduleTimer(m_next_timer_id);
	}

	return 0;
}

void
PluginComponentInstance::UnscheduleTimer(unsigned int timer_id)
{
	Timer* t;
	if (OpStatus::IsSuccess(m_timers.Remove(timer_id, &t)))
		OP_DELETE(t);
}

void
PluginComponentInstance::PluginThreadAsyncCall(void (*func)(void*), void* data) const
{
	if (!m_channel)
		return;

	OpPluginCallbackMessage* message = OpPluginCallbackMessage::Create();
	if (!message)
		return;

	message->SetFunc(reinterpret_cast<UINT64>(func));
	message->SetData(reinterpret_cast<UINT64>(data));

	message->SetDst(m_channel->GetAddress());
	message->SetSrc(m_channel->GetAddress());

	OpStatus::Ignore(g_thread_tools->SendMessageToMainThread(message));
}

OP_STATUS
PluginComponentInstance::ProcessMessage(const OpTypedMessage* message)
{
	CountInstanceUse of(this);

	OP_STATUS ret = OpStatus::OK;

	switch (message->GetType())
	{
		case OpStatusMessage::Type:
			/* Error returned from an asynchronous fire-and-forget request. */
			break;

		case OpPeerConnectedMessage::Type:
			break;

		case OpPeerDisconnectedMessage::Type:
			if (!m_is_destroyed)
				/* Delete this instance after NPP_Destroy has been called. */
				ret = m_after_destroy_hook.Append(&PluginComponentInstance::Delete);
			else
				/* NPP_Destroy has already been called. Delete this instance at first opportunity. */
				ret = m_unused_hook.Append(&PluginComponentInstance::Delete);
			break;

		case OpPluginDestroyMessage::Type:
			ret = OnDestroyMessage(OpPluginDestroyMessage::Cast(message));
			break;

		case OpPluginGetValueMessage::Type:
			ret = OnGetValueMessage(OpPluginGetValueMessage::Cast(message));
			break;

		case OpPluginNewStreamMessage::Type:
			ret = OnNewStreamMessage(OpPluginNewStreamMessage::Cast(message));
			break;

		case OpPluginDestroyStreamMessage::Type:
			ret = OnDestroyStreamMessage(OpPluginDestroyStreamMessage::Cast(message));
			break;

		case OpPluginWriteReadyMessage::Type:
			ret = OnWriteReadyMessage(OpPluginWriteReadyMessage::Cast(message));
			break;

		case OpPluginWriteMessage::Type:
			ret = OnWriteMessage(OpPluginWriteMessage::Cast(message));
			break;

		case OpPluginStreamAsFileMessage::Type:
			ret = OnStreamAsFileMessage(OpPluginStreamAsFileMessage::Cast(message));
			break;

		case OpPluginNotifyMessage::Type:
			ret = OnNotifyMessage(OpPluginNotifyMessage::Cast(message));
			break;

		case OpPluginWindowMessage::Type:
			ret = OnWindowMessage(OpPluginWindowMessage::Cast(message));
			break;

		case OpPluginWindowlessPaintMessage::Type:
			ret = OnWindowlessPaintMessage(OpPluginWindowlessPaintMessage::Cast(message));
			break;

		case OpPluginWindowlessKeyEventMessage::Type:
			ret = OnWindowlessKeyEventMessage(OpPluginWindowlessKeyEventMessage::Cast(message));
			break;

		case OpPluginWindowlessMouseEventMessage::Type:
			ret = OnWindowlessMouseEventMessage(OpPluginWindowlessMouseEventMessage::Cast(message));
			break;

		case OpPluginTimerMessage::Type:
			ret = OnTimerMessage(OpPluginTimerMessage::Cast(message));
			break;

		case OpPluginCallbackMessage::Type:
			ret = OnCallbackMessage(OpPluginCallbackMessage::Cast(message));
			break;

		case OpPluginPlatformEventMessage::Type:
			ret = OnPlatformEventMessage(OpPluginPlatformEventMessage::Cast(message));
			break;

		case OpPluginFocusEventMessage::Type:
			ret = OnFocusEventMessage(OpPluginFocusEventMessage::Cast(message));
			break;

		default:
			OP_ASSERT(!"PluginComponentInstance received unknown message!");
			return OpStatus::ERR;
	}

	return ret;
}

OP_STATUS
PluginComponentInstance::Delete()
{
	if (m_use_count > 0)
		return m_unused_hook.Append(&PluginComponentInstance::Delete);

	OP_DELETE(this);
	return OpStatus::OK;
}

OP_STATUS
PluginComponentInstance::OnDestroyMessage(OpPluginDestroyMessage* message)
{
	if (m_use_count > 1)
	{
		/* This instance is used by stack frames above the current ProcessMessage(), and these may
		 * include NPN request handlers. Calling NPP_Destroy in the middle of an NPN request is not
		 * advised (DSK-350137).
		 *
		 * Instead, we remind ourselves to do in the future. (Note that the unused hook cannot be
		 * used for this purpose, since it will trigger right before an NPN request returns to the
		 * plug-in library). */
		return g_component->SendMessage(OpPluginDestroyMessage::Create(message->GetSrc(), m_channel->GetAddress()));
	}

	CountInstanceUse of(this);

	int error = NPERR_INVALID_FUNCTABLE_ERROR;
	NPSavedData* save = NULL;

	if (!m_is_destroyed && m_component->GetPluginFunctions()->destroy)
	{
		error = m_component->GetPluginFunctions()->destroy(GetHandle(), &save);
		GetHandle()->ndata = NULL;
		m_is_destroyed = true;
	}

	OpStatus::Ignore(message->Reply(OpPluginDestroyResponseMessage::Create(error, reinterpret_cast<UINT64>(save))));

	OpStatus::Ignore(RunHook(m_after_destroy_hook));

	return OpStatus::OK;
}

OP_STATUS
PluginComponentInstance::OnGetValueMessage(OpPluginGetValueMessage* message)
{
	NPError error = NPERR_INVALID_PARAM;
	NPPVariable variable = static_cast<NPPVariable>(message->GetVariable());

	OpPluginGetValueResponseMessage* response = OpPluginGetValueResponseMessage::Create(error);
	RETURN_OOM_IF_NULL(response);

	/** Boolean value. */
	if (PluginComponentHelpers::IsBooleanValue(variable))
	{
		/* There exist plug-ins that expect the value to be a pointer to an int-sized value. On
		 * Windows, http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/npn_api17.html
		 * indicates that the preferred value is BOOL. We opt for a zeroed int, catering to both
		 * BOOL and NPBool adherents. */
		int value = 0;
		if ((error = GetValue(variable, &value)) == NPERR_NO_ERROR)
			response->SetBooleanValue(!!value);
	}

	/* Integer value. */
	else if (PluginComponentHelpers::IsIntegerValue(variable))
	{
		int value;
		if ((error = GetValue(variable, &value)) == NPERR_NO_ERROR)
			response->SetIntegerValue(value);
	}

	/* Object type value. */
	else if (PluginComponentHelpers::IsObjectValue(variable))
	{
		NPObject* np_object = NULL;
		error = GetValue(variable, &np_object);

		if (error == NPERR_NO_ERROR && np_object == NULL)
			error = NPERR_GENERIC_ERROR;

		if (error == NPERR_NO_ERROR)
		{
			UINT64 browser_ptr = m_component->Lookup(np_object);

			if (!browser_ptr)
			{
				/* The QuakeLive plug-in does not care to go through NPN_CreateObject, but allocates an
				 * NPObject itself and expects us to take it in stride. See CT-1811, and CORE-29702. */
				if (!RegisterObject(np_object) || !(browser_ptr = m_component->Lookup(np_object)))
				{
					BrowserFunctions::NPN_ReleaseObject(np_object);
					error = NPERR_OUT_OF_MEMORY_ERROR;
				}
			}

			if (browser_ptr)
			{
				if (PluginComponent::PluginObject* object = PluginComponentHelpers::ToPluginObject(browser_ptr))
					response->SetObjectValue(object);
				else
					error = NPERR_OUT_OF_MEMORY_ERROR;
			}
		}
	}

	/* Unknown variable. */
	else
		OP_ASSERT(!"PluginInstance requested a variable of unknown class.");

	response->SetNpError(error);
	return message->Reply(response);
}

OP_STATUS
PluginComponentInstance::OnNewStreamMessage(OpPluginNewStreamMessage* message)
{
	NPError error = NPERR_GENERIC_ERROR;
	uint16_t stream_type = message->GetStreamType();

	if (!m_is_destroyed && m_component->GetPluginFunctions()->newstream)
	{
		char* content_type_;
		RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&content_type_, message->GetContentType()));
		OpAutoArray<char> content_type(content_type_);

		NPStream* stream = Bind(message->GetStream());
		RETURN_OOM_IF_NULL(stream);

		error = m_component->GetPluginFunctions()->newstream(GetHandle(), content_type.get(), stream, message->GetSeekable(), &stream_type);

		if (error != NPERR_NO_ERROR)
		{
			Unbind(message->GetStream());
			BrowserFunctions::NPN_MemFree(stream);
		}
	}

	return message->Reply(OpPluginNewStreamResponseMessage::Create(error, stream_type));
}

OP_STATUS
PluginComponentInstance::OnDestroyStreamMessage(OpPluginDestroyStreamMessage* message)
{
	NPError error = NPERR_INVALID_PARAM;

	NPStream* stream = Bind(message->GetStream());
	RETURN_OOM_IF_NULL(stream);

	if (stream)
	{
		if (!m_is_destroyed && m_component->GetPluginFunctions()->destroystream)
			error = m_component->GetPluginFunctions()->destroystream(GetHandle(), stream, message->GetReason());

		Unbind(message->GetStream());
		BrowserFunctions::NPN_MemFree(stream);
	}

	return message->Reply(OpPluginErrorMessage::Create(error));
}

OP_STATUS
PluginComponentInstance::OnWriteReadyMessage(OpPluginWriteReadyMessage* message)
{
	int32_t bytes = 0;

	if (!m_is_destroyed && m_component->GetPluginFunctions()->writeready)
	{
		NPStream* stream = Bind(message->GetStream());
		RETURN_OOM_IF_NULL(stream);

		bytes = m_component->GetPluginFunctions()->writeready(GetHandle(), stream);
	}

	return message->Reply(OpPluginWriteReadyResponseMessage::Create(bytes));
}

OP_STATUS
PluginComponentInstance::OnWriteMessage(OpPluginWriteMessage* message)
{
	int32_t bytes = 0;

	if (!m_is_destroyed && m_component->GetPluginFunctions()->write)
	{
		NPStream* stream = Bind(message->GetStream());
		RETURN_OOM_IF_NULL(stream);

		OpData data = message->GetBuffer();
		const char* buffer = data.Data();

		if (buffer)
			bytes = m_component->GetPluginFunctions()->write(GetHandle(), stream, message->GetOffset(), data.Length(), const_cast<char*>(buffer));
	}

	return message->Reply(OpPluginWriteResponseMessage::Create(bytes));
}

OP_STATUS
PluginComponentInstance::OnStreamAsFileMessage(OpPluginStreamAsFileMessage* message)
{
	if (!m_is_destroyed && m_component->GetPluginFunctions()->asfile)
	{
		NPStream* stream = Bind(message->GetStream());
		RETURN_OOM_IF_NULL(stream);

		const char* fname = NULL;
		if (message->HasFname())
			RETURN_IF_ERROR(message->GetFname().CreatePtr(&fname, 1));

		m_component->GetPluginFunctions()->asfile(GetHandle(), stream, fname);
	}

	/* The return value is constant, but signals that the plug-in function has returned
	 * and that ns4plugins may release its function locks. */
	return message->Reply(OpPluginErrorMessage::Create(NPERR_NO_ERROR));
}

OP_STATUS
PluginComponentInstance::OnNotifyMessage(OpPluginNotifyMessage* message)
{
	if (!m_is_destroyed && m_component->GetPluginFunctions()->urlnotify)
	{
		char* url_;
		RETURN_IF_ERROR(UniString_UTF8::ToUTF8(&url_, message->GetURL()));
		OpAutoArray<char> url(url_);

		m_component->GetPluginFunctions()->urlnotify(GetHandle(), url.get(), message->GetReason(), reinterpret_cast<void*>(message->GetNotifyData()));
	}

	/* The return value is constant, but signals that the plug-in function has returned
	 * and that ns4plugins may release its function locks. */
	return message->Reply(OpPluginErrorMessage::Create(NPERR_NO_ERROR));
}

/**
 * Helper class used to temporarily set a new scripting context.
 *
 * See PluginHandler::PushScriptingContext().
 */
struct SetScriptingContext
{
	SetScriptingContext(PluginComponent* component, int new_context) : component(component) {
		old_context = component->GetScriptingContext();
		component->SetScriptingContext(new_context);
	}
	~SetScriptingContext() { component->SetScriptingContext(old_context); }
	PluginComponent* component;
	int old_context;
};

OP_STATUS
PluginComponentInstance::OnWindowMessage(OpPluginWindowMessage* message)
{
	/* Extract NPWindow data. */
	OpRect rect;
	PluginComponentHelpers::SetOpRect(&rect, message->GetRect());

	OpRect clip_rect;
	PluginComponentHelpers::SetOpRect(&clip_rect, message->GetClipRect());

	/* Update the NPWindow structure and pass it to the plug-in. */
	m_np_window.window = reinterpret_cast<void*>(static_cast<uintptr_t>(message->GetWindow()));

	SetScriptingContext context(m_component, message->GetContext());

	NPError error = SetWindow(rect, clip_rect, static_cast<NPWindowType>(message->GetWindowType()));

	return message->Reply(OpPluginErrorMessage::Create(error));
}

NPError
PluginComponentInstance::SetWindow(const OpRect& rect, const OpRect& clip_rect, int type /* = 0 */)
{
	NPError error = NPERR_INVALID_INSTANCE_ERROR;

	if (!m_is_destroyed && m_component->GetPluginFunctions()->setwindow)
	{
		if (type == 0)
			type = m_np_window.type;

		RETURN_VALUE_IF_ERROR(m_translator->UpdateNPWindow(m_np_window, rect, clip_rect, static_cast<NPWindowType>(type)), NPERR_GENERIC_ERROR);

		error = m_component->GetPluginFunctions()->setwindow(GetHandle(), &m_np_window);

		/* Send position changed message to windowless plug-ins. */
		if (error == NPERR_NO_ERROR && m_np_window.type == NPWindowTypeDrawable && m_component->GetPluginFunctions()->event)
		{
			OpPlatformEvent* event = NULL;
			RETURN_IF_ERROR(m_translator->CreateWindowPosChangedEvent(&event));

			/* Purposely ignored return value (see CORE-41057). */
			m_component->GetPluginFunctions()->event(GetHandle(), event->GetEventData());

			OP_DELETE(event);
		}
	}

	return error;
}

OP_STATUS
PluginComponentInstance::CreateAndDeliverPaintEvent(OpAutoPtr<OpPlatformEvent>& event, OpPluginImageID plugin_image, OpPluginImageID plugin_background, OpRect paint_rect)
{
	bool platform_modified_npwindow = false;
	OpPlatformEvent* event_ = NULL;
	RETURN_IF_ERROR(m_translator->CreatePaintEvent(&event_, plugin_image, plugin_background, paint_rect, &m_np_window, &platform_modified_npwindow));
	event = OpAutoPtr<OpPlatformEvent>(event_);

	if (platform_modified_npwindow)
	{
		RETURN_VALUE_IF_NULL(m_component->GetPluginFunctions()->setwindow, OpStatus::ERR);

		if (m_component->GetPluginFunctions()->setwindow(GetHandle(), &m_np_window) != NPERR_NO_ERROR)
			return OpStatus::ERR;
	}

	/* We discard the return value for paint events, since at least Flash never returns anything but 0. */
	m_component->GetPluginFunctions()->event(GetHandle(), event->GetEventData());

	return OpStatus::OK;
}

OP_STATUS
PluginComponentInstance::OnWindowlessPaintMessage(OpPluginWindowlessPaintMessage* message)
{
	OP_ASSERT(m_translator && "m_translator should have received a non-NULL value in PluginComponentInstance::OnWindowMessage() which must be successfully called before the first windowless paint event");

	if (m_is_destroyed || !m_component->GetPluginFunctions()->event)
		return OpStatus::ERR;

	SetScriptingContext context(m_component, message->GetContext());

	OpRect paint_rect;
	PluginComponentHelpers::SetOpRect(&paint_rect, message->GetPaintRect());

	OpPluginImageID plugin_image = message->GetOpPluginImageGlobalIdentifier();
	OpPluginImageID plugin_background = message->GetOpPluginBackgroundGlobalIdentifier();
	OpAutoPtr<OpPlatformEvent> event;
	RETURN_IF_ERROR(CreateAndDeliverPaintEvent(event, plugin_image, plugin_background, paint_rect));
	RETURN_IF_ERROR(m_translator->FinalizeOpPluginImage(plugin_image, m_np_window));

	int16 event_handled = static_cast<int16>(true);
	return message->Reply(OpPluginWindowlessHandleEventResponseMessage::Create(event_handled));
}

OP_STATUS
PluginComponentInstance::OnWindowlessMouseEventMessage(OpPluginWindowlessMouseEventMessage* message)
{
	OP_ASSERT(m_translator && "should be non-NULL at this point");

	if (m_is_destroyed || !m_component->GetPluginFunctions()->event)
		return OpStatus::ERR;

	OpPlatformEvent* event = NULL;
	RETURN_IF_ERROR(m_translator->CreateMouseEvent(&event, static_cast<OpPluginEventType>(message->GetEventType()),
												   PluginComponentHelpers::OpPointFromPluginPoint(message->GetPointRef()),
												   message->GetButton(), message->GetModifiers()));
	int16 event_handled = m_component->GetPluginFunctions()->event(GetHandle(), event->GetEventData());
	OP_DELETE(event);

	return message->Reply(OpPluginWindowlessHandleEventResponseMessage::Create(event_handled));
}

OP_STATUS
PluginComponentInstance::OnWindowlessKeyEventMessage(OpPluginWindowlessKeyEventMessage* message)
{
	OP_ASSERT(m_translator && "should be non-NULL at this point");

	if (m_is_destroyed || !m_component->GetPluginFunctions()->event)
		return OpStatus::ERR;

	OtlList<OpPlatformEvent*> events;
	RETURN_IF_ERROR(m_translator->CreateKeyEventSequence(
						events, static_cast<OpKey::Code>(message->GetKey()), message->GetKeyValue(), static_cast<OpPluginKeyState>(message->GetKeyState()), message->GetShiftKeyState(),
						message->GetPlatformData1(), message->GetPlatformData2()));

	INT32 event_handled = kNPEventNotHandled;

	/* The single browser key event sent by the browser may be translated into a sequence
	 * of plug-in key events by the translator. We consider the browser event handled if
	 * any of the plug-in events are handled. */
	bool stop_sending = false;
	for (OtlList<OpPlatformEvent*>::ConstRange e = events.ConstAll(); e; ++e)
	{
		if (!stop_sending)
		{
			INT32 this_event_handled = m_component->GetPluginFunctions()->event(GetHandle(), (*e)->GetEventData());
			if (this_event_handled != kNPEventNotHandled)
				event_handled = this_event_handled;

			/* Don't sent any more events if the plug-in starts the IME. */
			if (event_handled == kNPEventStartIME)
				stop_sending = true;
		}

		OP_DELETE(*e);
	}

	return message->Reply(OpPluginWindowlessHandleEventResponseMessage::Create(event_handled));
}

OP_STATUS
PluginComponentInstance::OnTimerMessage(OpPluginTimerMessage* message)
{
	Timer* t;
	if (OpStatus::IsError(m_timers.GetData(message->GetTimerID(), &t)))
		/* We have no information associated with this timer id. Disregard. */
		return OpStatus::OK;

	if (!m_is_destroyed)
		t->func(GetHandle(), message->GetTimerID());

	bool unschedule = true;

	if (!m_is_destroyed && m_channel && t->repeat)
		if (OpPluginTimerMessage* message = OpPluginTimerMessage::Create(m_next_timer_id))
		{
			message->SetSrc(m_channel->GetAddress());
			message->SetDst(m_channel->GetAddress());
			message->SetDueTime(message->GetDueTime() + t->interval);

			if (OpStatus::IsSuccess(g_component->SendMessage(message)))
				unschedule = false;
		}

	if (unschedule)
		UnscheduleTimer(message->GetTimerID());

	return OpStatus::OK;
}

OP_STATUS
PluginComponentInstance::OnCallbackMessage(OpPluginCallbackMessage* message)
{
	void (*func)(void*) = reinterpret_cast<void (*)(void*)>(message->GetFunc());
	void* data = reinterpret_cast<void*>(message->GetData());

	func(data);

	return OpStatus::OK;
}

OP_STATUS
PluginComponentInstance::OnPlatformEventMessage(OpPluginPlatformEventMessage* message)
{
	bool event_handled = false;
	OpPlatformEvent* event = reinterpret_cast<OpPlatformEvent*>(message->GetEvent());

	if (!m_is_destroyed && m_component->GetPluginFunctions()->event)
		event_handled = !!m_component->GetPluginFunctions()->event(GetHandle(), event->GetEventData());

	/* The message was sent by ourselves or platform code on the current thread,
	 * so it's perfectly safe and proper for us to delete it when we're done. */
	OP_DELETE(event);

	return message->Reply(OpPluginWindowlessHandleEventResponseMessage::Create(event_handled));
}

OP_STATUS
PluginComponentInstance::OnFocusEventMessage(OpPluginFocusEventMessage* message)
{
	bool event_handled = 0;

	if (!m_is_destroyed && m_component->GetPluginFunctions()->event)
	{
		OpPlatformEvent* event = NULL;
		if (OpStatus::IsSuccess(m_translator->CreateFocusEvent(&event, !!message->GetFocus())))
		{
			event_handled = !!m_component->GetPluginFunctions()->event(GetHandle(), event->GetEventData());
			OP_DELETE(event);
		}
	}

	return message->Reply(OpPluginWindowlessHandleEventResponseMessage::Create(event_handled));
}

NPStream*
PluginComponentInstance::Bind(const PluginStream& stream)
{
	StreamMap::Iterator existing_stream = m_stream_map.Find(stream.GetIdentifier());
	NPStream* np_stream = existing_stream != m_stream_map.End() ? existing_stream->second : NULL;

	if (!np_stream)
		/* No existing entry, create a new one. */
		if (np_stream = static_cast<NPStream*>(BrowserFunctions::NPN_MemAlloc(sizeof(NPStream))))
		{
			op_memset(np_stream, 0, sizeof(*np_stream));
			if (OpStatus::IsError(m_stream_map.Insert(stream.GetIdentifier(), np_stream)) ||
				OpStatus::IsError(m_np_stream_map.Insert(np_stream, stream.GetIdentifier())))
			{
				Unbind(stream);
				BrowserFunctions::NPN_MemFree(np_stream);
				return NULL;
			}

			/* Write notifyData once on creation, and thereafter leave it be, as the plug-in may
			 * use it for its own purposes. In particular, Flash v11.1 will change the value on
			 * NPP_NewStream and expect it to remain changed. */
			np_stream->notifyData = reinterpret_cast<void*>(stream.GetNotifyData());
		}

	/* Update fields to reflect current browser data. */
	if (np_stream)
	{
		if (np_stream->url)
			BrowserFunctions::NPN_MemFree(const_cast<char*>(np_stream->url));

		if (OpStatus::IsError(UniString_UTF8::ToUTF8(const_cast<char**>(&np_stream->url), stream.GetURL())))
			np_stream->url = NULL;

		if (np_stream->headers)
			BrowserFunctions::NPN_MemFree(const_cast<char*>(np_stream->headers));

		if (OpStatus::IsError(UniString_UTF8::ToUTF8(const_cast<char**>(&np_stream->headers), stream.GetHeaders())))
			np_stream->headers = NULL;

		if (!np_stream->url || !np_stream->headers)
		{
			Unbind(stream);
			BrowserFunctions::NPN_MemFree(np_stream);
			return NULL;
		}

		np_stream->end = stream.GetEnd();
		np_stream->lastmodified = stream.GetLastModified();
	}

	return np_stream;
}

void
PluginComponentInstance::Unbind(const PluginStream& stream)
{
	StreamMap::Iterator existing_stream = m_stream_map.Find(stream.GetIdentifier());
	NPStream* np_stream = existing_stream != m_stream_map.End() ? existing_stream->second : NULL;
	if (np_stream)
	{
		BrowserFunctions::NPN_MemFree(const_cast<char*>(np_stream->url));
		np_stream->url = NULL;

		BrowserFunctions::NPN_MemFree(const_cast<char*>(np_stream->headers));
		np_stream->headers = NULL;

		m_np_stream_map.Erase(np_stream);
		m_stream_map.Erase(stream.GetIdentifier());
	}
}

UINT64
PluginComponentInstance::Lookup(NPStream* stream)
{
	NpStreamMap::Iterator id = m_np_stream_map.Find(stream);
	return id != m_np_stream_map.End() ? id->second : 0;
}

int
PluginComponentInstance::IncUseCount()
{
	return ++m_use_count;
}

int
PluginComponentInstance::DecUseCount()
{
	int ret = --m_use_count;

	if (m_use_count == 0)
		OpStatus::Ignore(RunHook(m_unused_hook));

	return ret;
}

#endif // NS4P_COMPONENT_PLUGINS
