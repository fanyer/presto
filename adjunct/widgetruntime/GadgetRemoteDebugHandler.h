/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_REMOTE_DEBUG_HANDLER_H
#define GADGET_REMOTE_DEBUG_HANDLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/hardcore/timer/optimer.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/prefs/prefsmanager/opprefscollection_default.h"
#include "modules/scope/scope_module.h"
#include "modules/scope/src/scope_manager.h"

class DesktopGadget;
class IPrefsCollectionTools;
class IScopeManager;
class IMessageHandler;
class RegularPrefsCollectionTools;
class RegularLocalScopeManager;
class RegularMessageHandler;

/**
 * The logic behind remote debugging connection handling for standalone gadgets.
 *
 * @author Bazyli Zygan (bazyl)
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetRemoteDebugHandler
	: public MessageObject
{
public:
	/**
	 * Represents the remote debugging connection state.
	 */
	struct State
	{
		State();

		/** Because OpString has no real copying support. */
		State(const State& rhs);
		/** Because OpString has no real copying support. */
		State& operator=(const State& rhs);

		OpString m_ip_address;
		int m_port_no;
		BOOL m_connected;
	};

	/**
	 * Interface for listening on remote debug connection state changes.
	 */
	class Listener
	{
	public:
		virtual ~Listener() {}

		virtual void OnConnectionSuccess() = 0;
		virtual void OnConnectionFailure() = 0;
	};


	/**
	* @param isTestRun - default is zero, which means that regular global
	* object will be used to connection and messages handling for test purposes
	* you can specify objects that will be used instad of singletion objects. 
	*/
	GadgetRemoteDebugHandler(BOOL isTestRun = 0, 
			IPrefsCollectionTools* prefsm = NULL,
			IScopeManager* scpm = NULL,
			IMessageHandler* msgh = NULL);
	~GadgetRemoteDebugHandler();

	/**
	 * Registers a connection state change listener.
	 *
	 * @param listener the listener object
	 * @return status
	 */
	OP_STATUS AddListener(Listener& listener);

	/**
	 * Removes a previously registered connection state change listener.
	 *
	 * @param listener the listener object
	 * @return status
	 */
	OP_STATUS RemoveListener(Listener& listener);

	/**
	 * Retrieves the current connection state.
	 *
	 * @return the current connection state
	 */
	State GetState() const;

	/**
	 * Sets the connection state.  May trigger reconnecting.
	 *
	 * @param rhs the new connection state
	 * @return status
	 */
	OP_STATUS SetState(const State& rhs);


	//
	// MessageObject
	//
	virtual void HandleCallback(
			OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	/**
	*      l_[something] - pointers used for switch global singleton objects for test purposes
	*/

	static IScopeManager*	l_scope_manager;
private:

	
	IPrefsCollectionTools*	l_pctools;
	
	IMessageHandler*		l_message_handler;
	BOOL					m_is_test_run;
	/**
	 * A helper, OpTimer-based remote debugging connection state poller.
	 *
	 * @author Wojciech Dzierzanowski (wdzierzanowski)
	 */
	class ConnectionPoller : public OpTimerListener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener()  {}

			/**
			 * The poller has detected that the remote debugging connection is
			 * closed.
			 */
			virtual void OnConnectionClosed() = 0;

			/**
			 * The poller has reached its poll count limit and will not be
			 * polling anymore.
			 */
			virtual void OnMaxPollCount() = 0;
		};

		ConnectionPoller();

		BOOL IsPolling() const;

		/**
		 * (Re)starts the polling procedure using the specified intervals and
		 * timeout.
		 *
		 * @param interval the polling interval [ms]
		 * @param timeout the time after which the polling is aborted [ms]
		 * @param listener the object that will be notified about
		 * 		polling-related events
		 */
		void Start(INT32 interval, INT32 timeout, Listener& listener);

		// OpTimerListener
		virtual void OnTimeOut(OpTimer* timer);

	private:
		OpTimer m_timer;
		INT32 m_interval;
		INT32 m_max_poll_count;
		INT32 m_poll_count;
		Listener* m_listener;
	};


	friend class Reconnector;
	/**
	 * A ConnectionPoller::Listener that reconnects to the remote debugger as
	 * soon as the previous connection is closed.
	 *
	 * @author Wojciech Dzierzanowski (wdzierzanowski)
	 */
	class Reconnector : public ConnectionPoller::Listener
	{
	public:
		Reconnector();

		void Init(GadgetRemoteDebugHandler& handler,
				const State& state);

		virtual void OnConnectionClosed();
		virtual void OnMaxPollCount();
		
	private:
		void Connect();

		GadgetRemoteDebugHandler* m_handler;
		State m_state;
	};


	OP_STATUS Connect(const State& state);
	OP_STATUS Reconnect(const State& state);
	OP_STATUS Disconnect();

	ConnectionPoller m_reconnecting_poller;
	Reconnector m_reconnector;

	OpListeners<Listener> m_listeners;
};

/** 
* Interface class used to wrap global object for selftest purposes
*/
class IScopeManager
{
public:
	virtual BOOL IsConnected()  = 0;

};

class RegularScopeManager : public IScopeManager
{
	public:
	  virtual BOOL IsConnected()
		{
		   return g_scope_manager->IsConnected();
		}
};

/** 
* Interface class used to wrap global object for selftest purposes
*/
class IPrefsCollectionTools
{
	public:
		virtual const OpStringC GetStringPref(PrefsCollectionTools::stringpref which) const  = 0;
		virtual OP_STATUS WriteStringL(PrefsCollectionTools::stringpref which, const OpStringC &value)  = 0;
		virtual int GetIntegerPref(PrefsCollectionTools::integerpref which) const  = 0;
		virtual OP_STATUS WriteIntegerL(PrefsCollectionTools::integerpref which, int value)  = 0;
};

class RegularPrefsCollectionTools : public IPrefsCollectionTools
{
	public:
		const OpStringC GetStringPref(PrefsCollectionTools::stringpref which) const
		{ 
			return g_pctools->GetStringPref(which);
		}

		OP_STATUS WriteStringL(PrefsCollectionTools::stringpref which, const OpStringC &value)
		{
			return g_pctools->WriteStringL(which,value);
		}

		int GetIntegerPref(PrefsCollectionTools::integerpref which) const
		{
			return g_pctools->GetIntegerPref(which);
		}

		OP_STATUS WriteIntegerL(PrefsCollectionTools::integerpref which, int value)
		{
			return g_pctools->WriteIntegerL(which,value);
		}
};

/** 
* Interface class used to wrap global object for selftest purposes
*/
class IMessageHandler
{
	public:
		virtual void UnsetCallBacks(MessageObject* obj)  = 0;
		virtual OP_STATUS SetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id)  = 0;
		virtual BOOL PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay=0)  = 0;
};

class RegularMessageHandler: public IMessageHandler
{
	public:
		void UnsetCallBacks(MessageObject* obj)
		{
			g_main_message_handler->UnsetCallBacks(obj);			 
		}

		OP_STATUS SetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id)
		{
			return g_main_message_handler->SetCallBack(obj,msg,id);
		}

		BOOL PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay=0)
		{
			return g_main_message_handler->PostMessage(msg,par1,par2,delay);
		}
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_REMOTE_DEBUG_HANDLER_H
