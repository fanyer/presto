/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Erik Moller <emoller@opera.com>, Niklas Beischer <no@opera.com>
**
*/

#if !defined(_DOM_WEBSOCKET_H_) && defined(WEBSOCKETS_SUPPORT)
#define _DOM_WEBSOCKET_H_

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/domenvironment.h"
#include "modules/url/protocols/websocket_listener.h"
#include "modules/ecmascript_utils/esthread.h"

class OpWebSocket;

// [ Constructor(in DOMString url, in optional DOMString protocols)
// , Constructor(DOMString url, optional DOMString[] protocols)]
class DOM_WebSocket_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_WebSocket_Constructor();

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
	static void AddConstructorL(DOM_Object *object);
};

/*
interface WebSocket : EventTarget {
  readonly attribute DOMString url;

  // ready state
  const unsigned short CONNECTING = 0;
  const unsigned short OPEN = 1;
  const unsigned short CLOSING = 2;
  const unsigned short CLOSED = 3;
  readonly attribute unsigned short readyState;
  readonly attribute unsigned long bufferedAmount;

  // networking
  [TreatNonCallableAsNull]attribute Function? onopen;
  [TreatNonCallableAsNull]attribute Function? onerror;
  [TreatNonCallableAsNull]attribute Function? onclose;
  readonly attribute DOMString extensions;
  readonly attribute DOMString protocol;
  void close([Clamp] optional unsigned short code, optional DOMString reason);

  // messaging
  [TreatNonCallableAsNull]attribute Function? onmessage;
  attribute DOMString binaryType;

  void send(DOMString data);
  void send(ArrayBuffer data);
  void send(Blob data);
};
*/

class DOM_WebSocket
	: public DOM_Object,
	  public DOM_EventTargetOwner,
	  public OpWebSocketListener,
	  public MessageObject
{
public:
	/** Create a WebSocket.
	 *
	 * @return
	 *  OK
	 *  ERR_NO_MEMORY
	 *  ERR_NOT_SUPPORTED  - Internal error, no listener specified.
	.*  ERR_PARSING_FAILED - Invalid url or protocol specified.
	 *  ERR_NO_ACCESS      - Restricted port used in url.
	 *  ERR                - Internal error, failed to create socket.
	 */
	static OP_STATUS Make(DOM_WebSocket *&new_obj, DOM_Runtime *origining_runtime, const uni_char *url, const OpVector<OpString> &sub_protocols);

	static void ConstructWebSocketObjectL(ES_Object *object, DOM_Runtime *runtime);

	static void Init();

	// From DOM_Object
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSOCKET || DOM_Object::IsA(type); }
	virtual void GCTrace();
	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	// From the DOM_EventTargetOwner interface.
	virtual DOM_Object *GetOwnerObject() { return this; }

	// Close the socket without attempting a graceful close and return TRUE if
	// it wasn't already closed.
	BOOL CloseHard();

	// Special GC trace depending on the state of the WebSocket.
	void GCTraceConditional();

	OP_STATUS AddThreadListener(DOM_Runtime *origining_runtime);

	// void close()
	DOM_DECLARE_FUNCTION(close);

	// void send();
	DOM_DECLARE_FUNCTION(send);

	// Defining three ES functions and two ES functions with data.
	enum { FUNCTIONS_ARRAY_SIZE = 4 };
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };

	// ready state constants
	enum ReadyState
	{
		CONNECTING = 0,
		OPEN       = 1,
		CLOSING    = 2,
		CLOSED     = 3
	};

private:

	virtual ~DOM_WebSocket();

	// From OpWebSocketListener
	virtual OP_STATUS OnSocketOpen(OpWebSocket *socket);
	virtual OP_STATUS OnSocketClosing(OpWebSocket *socket);
	virtual void OnSocketClosed(OpWebSocket *socket, BOOL wasClean, unsigned short code, const uni_char *reason);

	virtual OP_STATUS OnSocketMessage(OpWebSocket *socket, BOOL is_binary, BOOL is_file);
	virtual void OnSocketError(OpWebSocket *socket, OP_STATUS error);
	virtual void OnSocketInfo(OpWebSocket *socket, WS_Event evt, const uni_char *str, const uni_char *str2, const uni_char *str3);

	// Methods needed for WebSocket tasks.

// This section is public only to make ADS compile this without errors.
public:
	class DOM_WebSocketThreadListenerLink;

	class DOM_WebSocketThreadListener : public ES_ThreadListener
	{
	public:
		DOM_WebSocketThreadListener(DOM_WebSocket* websocket, DOM_WebSocketThreadListenerLink *l);

		virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

		DOM_WebSocket* webSocket;
		DOM_WebSocketThreadListenerLink *link;
	};

	class DOM_WebSocketThreadListenerLink
		: public ListElement<DOM_WebSocketThreadListenerLink>
	{
	public:
		DOM_WebSocketThreadListenerLink(DOM_WebSocket* websocket, ES_Thread *t);
		virtual ~DOM_WebSocketThreadListenerLink();

		ES_Thread *thread;
		DOM_WebSocketThreadListener listener;
	};

	void ProcessTaskQueue();

	enum TaskType
	{
		CHANGE_READY_STATE = 1,
		SET_COOKIES = 2,
		TRIGGER_EVENT = 4
	};

	enum TaskCondition
	{
		DO_ALWAYS = 0xff,
		DO_WHEN_CONNECTED = (1 << OPEN) | (1 << CLOSING)
	};

	class DOM_WebSocketTask
		: public ListElement<DOM_WebSocketTask>
	{
	public:
		/** The task operations to perform. A disjuntion of TaskType flags. */
		unsigned tasks;

		/** The task operation parameters. Not all fields have a
		    valid interpretation for a particular operation. */
		struct
		{
			char condition;
			/**< For what current ready states does this task apply? */

			DOM_Event* evt;
			/**< The event to fire for this task; only considered by
			     TRIGGER_EVENT. */

			ReadyState readyState;
			/**< The state to switch to; only considered by
			     CHANGE_READY_STATE. */
		} data;

		DOM_WebSocketTask()
			: tasks(SET_COOKIES)
		{
		}

		DOM_WebSocketTask(ReadyState s)
			: tasks(CHANGE_READY_STATE)
		{
			data.readyState = s;
		}

		DOM_WebSocketTask(DOM_Event *e, char cond)
			: tasks(TRIGGER_EVENT)
		{
			data.evt = e;
			data.condition = cond;
		}

		/* A 'trigger event' task may also be combined with a switch
		   in ready state and updating of cookies. */

		/** Have the task switch readyState to 's'. */
		void SetReadyState(ReadyState s)
		{
			OP_ASSERT((tasks & TRIGGER_EVENT) != 0);
			tasks |= CHANGE_READY_STATE;
			data.readyState = s;
		}

		/** Have the task also set cookies. */
		void SetCookies()
		{
			OP_ASSERT((tasks & TRIGGER_EVENT) != 0);
			tasks |= SET_COOKIES;
		}
	};

private:
	friend class DOM_WebSocket_Constructor;

	DOM_WebSocket();

	// Post the different events to listeners.
	OP_STATUS PostOpenEvent(TaskCondition cond);
	OP_STATUS PostErrorEvent(TaskCondition cond);
	OP_STATUS PostCloseEvent(BOOL wasClean, unsigned short code, const uni_char *reason, TaskCondition cond);

	OP_STATUS PostMessageEvent(const uni_char *message_data, unsigned length, const OpString &lastEventId, TaskCondition cond);
	OP_STATUS PostMessageEvent(ES_Value &message_data_value, const OpString &lastEventId, TaskCondition cond);

	OP_STATUS PostReadyStateChange(ReadyState state);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void QueueTask(DOM_WebSocketTask *task);

	// Used for storage of URL passed to script. Not valid until GetName(OP_ATOM_URL) has been called.
	OpString targetURL;

	// This may be read at will, but must never be set explicitly.
	ReadyState readyState;

	// This may be read at will, but must never be set explicitly.
	double bufferedAmount;

	enum BinaryType
	{
		BINARY_BLOB,
		BINARY_ARRAYBUFFER
	};

	// The binary form to expose data as. Default is BINARY_BLOB.
	BinaryType binaryType;

	// Returns constant string representing this socket's binary type.
	const uni_char *GetBinaryTypeString();

	/** Update this socket's binary type.

	    @param new_type The new binary type, either "blob" or "arraybuffer."
	    @return TRUE if successfully updated, FALSE if an unrecognized
	            binary type. */
	BOOL SetBinaryType(const uni_char *new_type);

	OpWebSocket* socket;
	MessageHandler* messageHandler;

	AutoDeleteList<DOM_WebSocketThreadListenerLink> threadListeners;
	AutoDeleteList<DOM_WebSocketTask> tasks;
};

#endif // _DOM_WEBSOCKET_H_ && WEBSOCKETS_SUPPORT
