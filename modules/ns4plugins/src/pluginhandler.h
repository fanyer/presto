/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef _PLUGINHANDLER_H_INC_
#define _PLUGINHANDLER_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/plugincommon.h"

#include "modules/util/simset.h"

#include "modules/ns4plugins/opns4pluginhandler.h"
#include "modules/hardcore/timer/optimer.h"

class Plugin;
class OpNS4Plugin;
class PluginLibHandler;
class ES_Object;
class ES_Thread;
class PluginRestartObject;
class PluginStream;
class HTML_Elememt;
class FramesDocument;
class OpNPObject;

enum PluginMsgType {	INIT, NEW, SETWINDOW, SETREADYFORSTREAMING, RESIZEMOVE, DESTROY,
						EMBEDSTREAMFAILED, NEWSTREAM, WRITEREADY, WRITE, STREAMASFILE, DESTROYSTREAM, URLNOTIFY };

struct PluginAsyncCall
{
	void (*function)(void*);
	void* userdata;
};

struct PluginTimer : public OpTimer, public ListElement<PluginTimer>
{
	NPP instance;
	uint32_t timerID;
	uint32_t interval;
	BOOL repeat;
	BOOL is_unscheduled;
	BOOL is_running;
	void (*timerFunc)(NPP npp, uint32_t timerID);

	PluginTimer() : timerID(0), interval(0), repeat(FALSE), is_unscheduled(FALSE), is_running(FALSE), timerFunc(0) {}
};

#ifdef NS4P_COMPONENT_PLUGINS
/**
 * Message dispatch filter defining the subset of component channels eligible
 * to receive messages. The filter is limited to one specific component.
 *
 * Plugix implemented its messaging by way of a dedicated pipe to the plug-in
 * wrapper. While waiting for the response to a synchronous message, it
 * handled other messages on this pipe, but did not run the hardcore message
 * handler. A lot of ns4plugins logic depend on the message ordering induced
 * by this behavior.
 *
 * In the new design, all messages go through the hardcore message handler,
 * and in the intermediary synchronous stage of OOPP development, we require
 * this filter to preserve the behavior as it was under Plugix.
 *
 * All plug-in related channels are explicitly whitelisted, and while we're
 * waiting for a synchronous reply from a wrapper, we enable this filter and
 * effectively block all messages that are not plug-in related.
 */
class PluginWhitelist : public OpTypedMessageSelection
{
	/**
	 * Associated component's address. Used to prevent the filter from
	 * affecting messages destined to other components.
	 */
	OpMessageAddress m_component_address;

	/**
	 * Map of addresses that are whitelisted.
	 */
	typedef OtlMap<OpMessageAddress, int> Whitelist;
	Whitelist m_whitelist;

	/**
	 * Limit scripting requests to this context. The value 0 identifies the most general context
	 * and encompasses all scripting requests.
	 */
	unsigned int m_scripting_context;

public:
	PluginWhitelist();

	/**
	 * Check whether an address is whitelisted.
	 *
	 * \param message  The address to check.
	 *
	 * \return \c TRUE if the address is whitelisted. \c FALSE otherwise.
	 */
	virtual BOOL IsMember(const OpTypedMessage* message) const;

	/**
	 * Whitelist an address.
	 *
	 * \param address  The address to whitelist.
	 *
	 * \return See OpStatus.
	 */
	OP_STATUS RegisterAddress(const OpMessageAddress& address);

	/**
	 * Remove an address from the list.
	 *
	 * \param address  The address to remove from the list.
	 *
	 * \return See OpStatus.
	 */
	void DeregisterAddress(const OpMessageAddress& address);

	/**
	 * Restrict the context in which we are accepting script messages.
	 *
	 * Any scripting requests made in a context more general than this will be rejected, while
	 * scripting requests with a context equal or more specific will be allowed. A context of zero
	 * encompasses all script requests. See PluginHandler::PushScriptingContext().
	 */
	unsigned int GetScriptingContext() { return m_scripting_context; }
	void SetScriptingContext(unsigned int context) { m_scripting_context = context; }
};
#endif // NS4P_COMPONENT_PLUGINS

class PluginHandler : public OpNS4PluginHandler, public OpTimerListener
{
private:

    int				plugincount;
    AutoDeleteHead	plugin_list;
	PluginLibHandler* pluginlibhandler;

	typedef struct PluginMsgStruct : public Link {
		PluginMsgType msg_type;
		struct
		{	NPP instance;
			int pluginid;
			int streamid;
			NPWindow* npwin;
		} id;
		int msg_flush_counter;
	} PluginMsgStruct;

	PluginMsgStruct*	last_handled_msg;
	AutoDeleteHead		messages;

	/** Handle the MSG_PLUGIN_CALL messages sent to HandleCallback. */
	OP_STATUS		HandleMessage(OpMessage message_id, MH_PARAM_1 wParam, MH_PARAM_2 lParam);


	/** PluginHandlerRestartObject represents script objects which has been suspended,
		waiting for the HTML_Element's scriptable object to be initiated (applet or plugin).
		Note that only one scripting object can be suspended for the same HTML_Element.
		If the restart_object is NULL, the plugin object was never created, and every attempt to
		suspend the scripting for this html element should fail. */

	typedef struct PluginHandlerRestartObject : public ListElement<PluginHandlerRestartObject> {
		FramesDocument*			frames_doc;
		HTML_Element*			html_element;
		PluginRestartObject*	restart_object_list;
		BOOL					been_blocked_but_plugin_failed;
	} PluginHandlerRestartObject;

	AutoDeleteList<PluginHandlerRestartObject>	restart_object_list;

#ifdef NS4P_QUICKTIME_CRASH_WORKAROUND
	int				m_number_of_active_quicktime_plugins;
#endif // NS4P_QUICKTIME_CRASH_WORKAROUND

# ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	BOOL			m_spoof_ua;
# endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND

	/** List of plugin timers */
	AutoDeleteList<PluginTimer> m_timer_list;

	uint32			m_nextTimerID;

#ifdef NS4P_COMPONENT_PLUGINS
	/** Message filter whitelisting communication with plug-in components. See WhitelistChannel(). */
	PluginWhitelist m_channel_whitelist;

	/** The current scripting context. See PushScriptingContext(). */
	unsigned int m_scripting_context;
#endif // NS4P_COMPONENT_PLUGINS

public:

					PluginHandler();
					virtual ~PluginHandler();

	OP_STATUS		Init();

	/** Handle the MSG_PLUGIN_CALL messages. */
	void			HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam);

	/** Post the MSG_PLUGIN_CALL messages. */
	OP_STATUS		PostPluginMessage(PluginMsgType msgty, NPP inst, int plugin_id, int stream_id = 0, NPWindow* npwin = NULL, int msg_flush_counter = 0, int delay = 0);

	/** Check if the specified message is in the queue of posted MSG_PLUGIN_CALL messages. */
	BOOL			HasPendingPluginMessage(PluginMsgType msgty, NPP inst, int plugin_id, int stream_id = 0, NPWindow* npwin = NULL, int msg_flush_counter = 0);

	/** Initiate the plugin */
#ifdef USE_PLUGIN_EVENT_API
	OpNS4Plugin*	New(const uni_char *plugin_dll, OpComponentType component_type, FramesDocument* frames_doc, OpStringC &mimetype, uint16 mode, HTML_Element* htm_elm = NULL, URL* url = NULL, BOOL embed_url_changed = FALSE);
#else
	OpNS4Plugin*	New(const uni_char *plugin_dll, OpComponentType component_type, FramesDocument* frames_doc, void* plugin_window, OpStringC &mimetype, uint16 mode, int16 argc, const uni_char *argn[], const uni_char *argv[], HTML_Element* htm_elm = NULL, URL* url = NULL, BOOL embed_url_changed = FALSE);
#endif // USE_PLUGIN_EVENT_API

	void			Destroy(OpNS4Plugin* plugin);
	Plugin*			FindPlugin(NPP instance, BOOL return_deleted = FALSE);

	OpNS4Plugin*	FindOpNS4Plugin(OpPluginWindow* oppw, BOOL unused = FALSE);

	PluginLibHandler* GetPluginLibHandler() { return pluginlibhandler; };
	FramesDocument*	GetScriptablePluginDocument(NPP instance, Plugin *&plugin);
	ES_Object*		GetScriptablePluginDOMElement(NPP instance);

#ifdef _PRINT_SUPPORT_
	void			Print(const VisualDevice* vd, const uni_char* src_url, const int x, const int y, const unsigned int width, const unsigned int height);
#endif // _PRINT_SUPPORT_

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
    OP_STATUS LoadStaticLib();
#endif // NS4P_SUPPORT_PROXY_PLUGIN

	OP_STATUS		HandleFailure();


	void*			FindPluginHandlerRestartObject(FramesDocument* frames_doc, HTML_Element* element);

	// From the OpNS4PluginHandler interface
	virtual OP_STATUS	Suspend(FramesDocument* frames_doc, HTML_Element* element, ES_Runtime* runtime, ES_Value* value, BOOL wait_for_full_plugin_init);
	virtual void		Resume(FramesDocument* frames_doc, HTML_Element* element, BOOL plugin_failed, BOOL include_threads_waiting_for_init);
	virtual BOOL		IsSuspended(FramesDocument* frames_doc, HTML_Element* element);
	virtual BOOL		IsPluginStartupRestartObject(ES_Object* object);
	virtual BOOL		IsSupportedRestartObject(ES_Object* object);

	OP_STATUS		DestroyStream(Plugin* plugin, PluginStream* ps);
	void			DestroyPluginRestartObject(PluginRestartObject* object);

	/** DestroyAllLoadingStreams() destroys all loading streams for all plugins. */
	void			DestroyAllLoadingStreams();

#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	/** Opera spoofing as Firefox.
	 * When NPN_UserAgent() is called without plugin instance, this value from g_pluginhandler is needed.
	 * Ensures backwards compatibility with old windowless Flash plugins (<10).
	 * Newer windowless Flash plugins (>10) doesn't work properly in Opera when spoofing as FF.
	 * @param spoof indicates whether to spoof or not. */
	void			SetFlashSpoofing(BOOL spoof) { m_spoof_ua = spoof; }

	/** Checks if spoofing as Firefox is necessary.
	 * @returns boolean value indicating whether to spoof or not */
	BOOL			SpoofUA() const { return m_spoof_ua; }
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	/** Schedules new plugin timer.
	 *
	 * @param instance  plugin instance
	 * @param interval	interval period in ms
	 * @param repeat	FALSE for one time timers, TRUE for recurring timers
	 * @param timerFunc	function that will be called upon timer timeouts
	 * @return scheduled timer identifier that is greater than 0 or 0 in case of error
	 */
	uint32_t			ScheduleTimer(NPP instance, uint32_t interval, BOOL repeat, void (*timerFunc)(NPP npp, uint32_t timerID));

	/** Terminates plugin timer.
	 *
	 * @param timerID	identifier of timer to unschedule
	 */
	void			UnscheduleTimer(uint32_t timerID);

	/** Terminates all timers of a given plugin instance.
	 *
	 * @param instance	plugin instance
	 */
	void			UnscheduleAllTimers(NPP instance);

	/** OpTimerListener API */
	virtual void	OnTimeOut(OpTimer* timer);

	/**
	 * Fail all plug-in instances of the given library.
	 *
	 * Used when we are disconnected from a remote plug-in component.
	 */
	virtual void    OnLibraryError(const uni_char* library_name, const OpMessageAddress& address);

#ifdef NS4P_COMPONENT_PLUGINS
	/**
	 * Register a channel with the plug-in channel whitelist.
	 *
	 * Used to mimic plugix' access control, i.e., allowing browser calls from the
	 * plug-in during a synchronous call into the plug-in, but disallowing any
	 * other message handling in the calling component.
	 *
	 * If a channel is registered N times, it will need to be deregistered
	 * N times before it vanishes from the whitelist.
	 *
	 * @param channel Channel to register and allow communication on.
	 *
	 * @return OpStatus::OK if successfully registered.
	 */
	OP_STATUS      WhitelistChannel(OpChannel* channel);

	/**
	 * Deregister a channel from the plug-in channel whitelist.
	 *
	 * See WhitelistChannel().
	 *
	 * @param channel Channel to forget.
	 */
	void           DelistChannel(OpChannel* channel) { DelistChannel(channel->GetAddress()); }

	/**
	 * Deregister a channel from the plug-in channel whitelist.
	 *
	 * See WhitelistChannel().
	 *
	 * @param channel Channel to forget.
	 */
	void           DelistChannel(const OpMessageAddress& channel_address);

	/**
	 * Retrieve the plug-in channel whitelist.
	 *
	 * See WhitelistChannel().
	 *
	 * @return Message selection that can be passed to OpSyncMessenger.
	 *         The lifetime is equal to that of PluginHandler.
	 */
	OpTypedMessageSelection* GetChannelWhitelist() { return &m_channel_whitelist; }

	/**
	 * Enter a new scripting context.
	 *
	 * When we transmit a SetWindow or Paint message to a plug-in, we are in the midst of a reflow
	 * and must be careful about which scripting operations we allow, as scripts may trigger another
	 * reflow and cause us to crash or otherwise misbehave.
	 *
	 * Contexts are used to classify scripting requests. When we send a window message, we'll first
	 * call PushContext to create a new context, and submit its ID as part of the message. When the
	 * plug-in component receives this request and starts handling it, it will update its global
	 * context to the one we sent just before entering the plug-in, and reset it immediately after
	 * returning. (See PluginComponentInstance::SetWindow.)
	 *
	 * Example:
	 *  1. The current context is 0 (outermost context.)
	 *  2. We call PushContext to enter context 1, and send 1 as part of the PluginWindow message.
	 *  3. The plug-in calls NPN_Evaluate, and since the PluginWindow message hasn't yet been
	 *     dispatched, the PluginEvaluate message is sent with context 0.
	 *  4. The plug-in component receives PluginWindow, updates its context to 1, and enters
	 *     NPP_SetWindow.
	 *  5. The plug-in calls NPN_Evaluate again, which this time results in a PluginEvaluate with
	 *     context 1.
	 *  6. We receive PluginEvaluate with context 0, but block it since we're in a more specific
	 *     context (1 > 0).
	 *  7. We receive PluginEvaluate with context 1, and handle it since we're in equal or less
	 *     specific context (1 >= 1).
	 *
	 * Thus, we will handle only those scripting requests that are an immediate consequence of
	 * calling NPP_SetWindow in the plug-in, and none other.
	 *
	 * @return Context entered (or exited to.)
	 */
	unsigned int PushScriptingContext() { return ++m_scripting_context; }
	unsigned int PopScriptingContext()  { return --m_scripting_context; }
#endif // NS4P_COMPONENT_PLUGINS
};

#ifdef NS4P_COMPONENT_PLUGINS
#include "modules/hardcore/component/OpSyncMessenger.h"

/**
 * Helper class that automatically whitelists the channel used.
 *
 * See OpSyncMessenger.
 */
class PluginSyncMessenger : public OpSyncMessenger
{
public:
	PluginSyncMessenger(OpChannel* peer) : OpSyncMessenger(peer) {}

	/**
	 * Send a message to the recipient.
	 *
	 * Identical to OpSyncMessenger::SendMessage() except the interpretation of allow_nesting.
	 *
	 * @param allow_nesting If false, only plug-in related messages will be allowed. See
	 *        PluginHandler::WhitelistChannel(). If true, no message filter will be applied,
	 *        and any browser action (e.g. reflow, running Carakan threads, &c) is permitted.
	 * @param timeout If a response isn't received within this many msec, the method will return
	 *        OpStatus::ERR_YIELD.
	 * @param scripting_context Only applicable if allow_nesting is false. If set, no scripting
	 *        requests in a more general context will be handled during this call. Requests in the
	 *        same or a more specific context will be handled. See PluginInstance::PushContext().
	 */
	virtual OP_STATUS SendMessage(OpTypedMessage* message, bool allow_nesting = false,
								  unsigned int timeout = 0, int scripting_context = 0);
};

/**
 * Helper class for temporarily toggling the enabled/disabled state of the
 * plug-in channel whitelist.
 *
 * See PluginHandler::WhitelistChannel().
 */
class PluginChannelWhitelistToggle
{
	bool m_whitelist_was_enabled;

public:
	PluginChannelWhitelistToggle(bool enable);
	~PluginChannelWhitelistToggle();
};
#endif // NS4P_COMPONENT_PLUGINS

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGINHANDLER_H_INC_
