/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_PLUGIN_COMPONENT_INSTANCE_H
#define NS4P_PLUGIN_COMPONENT_INSTANCE_H

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/otl/map.h"
#include "modules/ns4plugins/src/plugincommon.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/ns4plugins/src/plug-inc/npruntime.h"
#include "modules/ns4plugins/src/generated/g_message_ns4plugins_messages.h"
#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/hooked.h"
#include "modules/util/OpHashTable.h"
#include "modules/pi/OpPluginTranslator.h"


class PluginComponentInstance
	: public OpMessageListener
	, public Hooked<PluginComponentInstance>
{
public:
	typedef OpNs4pluginsMessages_MessageSet::PluginStream PluginStream;

	PluginComponentInstance(PluginComponent* component);
	virtual ~PluginComponentInstance();

	/**
	 * Construct instance and call NPP_New.
	 *
	 * @param plugin_instance The address of the browser-side PluginInstance.
	 * @param content_type The content-type to pass to NPP_New.
	 * @param mode The mode to pass to NPP_New.
	 * @param argc The number of arguments to pass to NPP_New.
	 * @param argn The names of the arguments to pass to NPP_New.
	 * @param argv The values of the arguments to pass to NPP_New.
	 * @param saved The saved value to pass to NPP_New.
	 * @param adapter_channel The address of the browser side OpPluginAdapter's channel, if any.
	 *
	 * @return OpStatus::OK if arguments were OK and NPP_New was invoked.
	 *         Errors from NPP_New are sent across the given channel.
	 */
	OP_STATUS Construct(OpMessageAddress plugin_instance, char* content_type, int mode,
						int argc, char* argn[], char* argv[], NPSavedData* saved,
						OpMessageAddress* adapter_channel);

	/**
	 * Retrieve the component to which this instance belongs.
	 *
	 * @return Pointer to plug-in component, or NULL if the component has been destroyed.
	 */
	PluginComponent* GetComponent() const { return m_component; }

	/**
	 * Retrieve the channel to the browser-side plug-in library.
	 */
	OpChannel* GetChannel() const { return m_channel; }

	/**
	 * Retrieve the translator.
	 */
	OpPluginTranslator* GetTranslator() { return m_translator; }

	/**
	 * Allow or disallow this instance to send requests to the browser.
	 *
	 * The intended use case for this method is blocking instances from causing deadlocks by
	 * entering synchronous calls to the browser at unfortunate times. See DSK-350479.
	 */
	void SetRequestsAllowed(bool requests_allowed);
	bool GetRequestsAllowed() { return m_requests_allowed; }

	/**
	 * Access cached user agent.
	 */
	const char* GetUserAgent() { return m_user_agent; }
	void SetUserAgent(const char* ua) { m_user_agent = ua; }

	/**
	 * Retrieve the plug-in instance identifier.
	 */
	NPP GetHandle() { return &m_npp; }

	/**
	 * Provide the plug-in instance with an updated NPWindow structure.
	 *
	 * This method will call OpPluginTranslator::UpdateNPWindow() before
	 * calling NPP_SetWindow().
	 *
	 * @param rect Rectangle passed to OpPluginTranslator::UpdateNPWindow().
	 * @param clip_rect Clip rectangle passed to OpPluginTranslator::UpdateNPWindow().
	 * @param type Window type. NPWindowTypeWindow, NPWindowTypeDrawable or 0, where
	 *        0 implies no change.
	 *
	 * @return See NPP_SetWindow on MDN.
	 */
	NPError SetWindow(const OpRect& rect, const OpRect& clip_rect, int type = 0);

	/**
	 * Retrieve a value from the plug-in.
	 *
	 * This method is a wrapper around NPP_GetValue(). Platform code should never
	 * access g_plugin_functions directly, but instead utilize access controlling
	 * methods such as this one.
	 *
	 * @param variable The variable whose value to retrieve.
	 * @param[out] value A pointer to a location that, on success, will contain
	 *             the variable-specific value.
	 *
	 * @return NPERR_NO_ERROR on success.
	 */
	NPError GetValue(NPPVariable variable, void* value);

	/**
	 * Send an event synchronously to the plug-in.
	 *
	 * This method is a wrapper around NPP_HandleEvent(). Be aware that many plug-ins
	 * do not like receiving an event while they're already handling another, so
	 * unless you need the return value immediately, you should consider sending a
	 * PluginPlatformEvent message to the local address returned by GetChannel() instead.
	 *
	 * @param event Platform-specific event data.
	 *
	 * @return Integer indicating whether the event was handled (> 0) or not (0). This
	 *         value may not always be trusted, and may also embody other event/platform
	 *         -specific information.
	 */
	int16 HandleEvent(void* event);

	/**
	 * Process incoming message.
	 */
	OP_STATUS ProcessMessage(const OpTypedMessage* message);

	/**
	 * Called by plug-in component when it's destroyed.
	 *
	 * The plug-in component will not destroy instances directly, since they may still
	 * be referenced on the stack, and will take care of their own destruction.
	 */
	void OnComponentDestroyed(PluginComponent* component) { OP_ASSERT(m_component == component); m_component = NULL; }

protected:
	/**
	 * Register local NPObject with browser process.
	 *
	 * @param object Local object to create a browser counterpart for. May be NULL, in
	 *               in which case a new local NPObject will be allocated.
	 *
	 * @return Pointer to local NPObject (allocated or argument), pre-bound in the component
	 *         owning this instance. The object must be unbound and deleted by the caller.
	 */
	NPObject* RegisterObject(NPObject* object);

	/**
	 * Schedule (optionally repeated) call(s) to function.
	 *
	 * @param interval The interval in milliseconds between successive calls.
	 * @param repeat True if the call should be repeated, False if it is to be made once only.
	 * @param func The function to call.
	 *
	 * @return Positive timer ID, or 0 on failure.
	 */
	unsigned int ScheduleTimer(unsigned int interval, bool repeat, void (*func)(NPP npp, uint32_t timerID));

	/**
	 * Remove a scheduled timer.
	 *
	 * @param timer_id ID returned from ScheduleTimer().
	 */
	void UnscheduleTimer(unsigned int timer_id);

	/**
	 * Request that a plug-in function be called on the main plug-in thread.
	 *
	 * @param func Function to call.
	 * @param data Data to supply with the call.
	 */
	void PluginThreadAsyncCall(void (*func)(void*), void* data) const;

	/**
	 * Look up a plug-in stream identifier.
	 *
	 * @param stream NPStream whose identifier we desire.
	 *
	 * @return Identifier or 0 if no such stream exists.
	 */
	UINT64 Lookup(NPStream* stream);

private:
	/**
	 * Bind a PluginStream to a local NPStream.
	 *
	 * If the stream is known, the previous binding will be returned.
	 *
	 * @param stream PluginStream.
	 *
	 * @return Pointer to bound NPStream or NULL on OOM.
	 */
	NPStream* Bind(const PluginStream& stream);

	/**
	 * Unbind PluginStream.
	 *
	 * @param stream Stream to unbind.
	 */
	void Unbind(const PluginStream& stream);

	/**
	 * Message handlers.
	 */
	OP_STATUS OnDestroyMessage(OpPluginDestroyMessage* message);
	OP_STATUS OnGetValueMessage(OpPluginGetValueMessage* message);
	OP_STATUS OnNewStreamMessage(OpPluginNewStreamMessage* message);
	OP_STATUS OnDestroyStreamMessage(OpPluginDestroyStreamMessage* message);
	OP_STATUS OnWriteReadyMessage(OpPluginWriteReadyMessage* message);
	OP_STATUS OnStreamAsFileMessage(OpPluginStreamAsFileMessage* message);
	OP_STATUS OnWriteMessage(OpPluginWriteMessage* message);
	OP_STATUS OnNotifyMessage(OpPluginNotifyMessage* message);
	OP_STATUS OnWindowMessage(OpPluginWindowMessage* message);

	/**
	 * Create and deliver a paint event to the plug-in instance.
	 * @param event              Platform-specific paint event.
	 * @param plugin_image       An image for the plug-in instance to draw on.
	 * @param plugin_background  The rendered area beneath the plug-in instance rectangle.
	 * @param paint_rect         Position and size of the plug-in instance.
	 * @return See OpStatus.
	 */
	OP_STATUS CreateAndDeliverPaintEvent(OpAutoPtr<OpPlatformEvent>& event, OpPluginImageID plugin_image, OpPluginImageID plugin_background, OpRect paint_rect);

	OP_STATUS OnWindowlessPaintMessage(OpPluginWindowlessPaintMessage* message);
	OP_STATUS OnWindowlessMouseEventMessage(OpPluginWindowlessMouseEventMessage* message);
	OP_STATUS OnWindowlessKeyEventMessage(OpPluginWindowlessKeyEventMessage* message);
	OP_STATUS OnTimerMessage(OpPluginTimerMessage* message);
	OP_STATUS OnCallbackMessage(OpPluginCallbackMessage* message);
	OP_STATUS OnPlatformEventMessage(OpPluginPlatformEventMessage* message);
	OP_STATUS OnFocusEventMessage(OpPluginFocusEventMessage* message);

	/** Delete this class. */
	OP_STATUS Delete();

	/** Modify use count. May delete this object. */
	int IncUseCount();
	int DecUseCount();

	/** The plug-in component to which we belong. May be NULL if the component has been destroyed. */
	PluginComponent* m_component;

	/** Channel to PluginLib. */
	OpChannel* m_channel;

	/** Instance object passed to and received from plug-in library. */
	NPP_t m_npp;

	/** True if we have called NPP_Destroy, in which case we need to be careful about plug-in calls. */
	bool m_is_destroyed;

	/** Number of messages currently being processed. Deletion may not occur when non-zero. */
	int m_messages_being_processed;

	/** PluginStream -> NPStream map. See Bind(). */
	typedef OtlMap<UINT64, NPStream*> StreamMap;
	StreamMap m_stream_map;

	/** NPStream -> PluginStream map. See Bind(). */
	typedef OtlMap<NPStream*, UINT64> NpStreamMap;
	NpStreamMap m_np_stream_map;

	/** Platform agent used to convert between platform-specific NPAPI structures
	 *  and cross-platform data. */
	OpPluginTranslator* m_translator;

	/** The window structure sent to the plug-in. After being passed to the plug-in instance
	 *  through NPP_SetWindow(), the plug-in instance will assume that this structure remains alive
	 *  until NPP_SetWindow() is called again with a new value or NPP_Destroy() is called. */
	NPWindow m_np_window;

	/** Cached user agent string. May be NULL. */
	const char* m_user_agent;

	/**
	 * Timers scheduled by plug-in.
	 *
	 * See NPN_ScheduleTimer and NPN_UnscheduleTimer.
	 */
	struct Timer
	{
		void (*func)(NPP npp, uint32_t timerID);
		unsigned int interval;
		bool repeat;
	};

	OpINT32HashTable<Timer> m_timers;
	int m_next_timer_id;

	/** Hook invoked when this instance is no longer active on the stack. */
	Hook<PluginComponentInstance> m_unused_hook;

	/** Hook invoked after calling NPP_Destroy. */
	Hook<PluginComponentInstance> m_after_destroy_hook;

	/** Count of active IPC use. A non-zero value prevents deletion and NPP_Destroy. */
	int m_use_count;

	/** False if we should block any attempt to send requests to the browser. */
	bool m_requests_allowed;

	/** Address of browser-side PluginInstance. */
	OpMessageAddress m_plugin_instance_address;

	friend class BrowserFunctions;
	friend class CountInstanceUse;
};

/**
 * Helper class to maintain instance use count.
 */
class CountInstanceUse
{
public:
	CountInstanceUse(PluginComponentInstance* pci) : m_pci(pci) { if (m_pci) m_pci->IncUseCount(); }
	~CountInstanceUse()                                         { if (m_pci) m_pci->DecUseCount(); }

protected:
	PluginComponentInstance* m_pci;
};

/**
 * Helper class to maintain instance use count.
 */
class CountedPluginComponentInstance
{
public:
	CountedPluginComponentInstance(PluginComponentInstance* pci) : m_pci(pci), m_use(pci) {}
	CountedPluginComponentInstance(const CountedPluginComponentInstance& src) : m_pci(src.get()), m_use(src.get()) {}

	PluginComponentInstance* operator->() const { return m_pci; }
	PluginComponentInstance* get() const { return m_pci; }

protected:
	PluginComponentInstance* m_pci;
	CountInstanceUse m_use;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // NS4P_PLUGIN_COMPONENT_INSTANCE_H
