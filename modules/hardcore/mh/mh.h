/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_MH_MH_H
#define MODULES_HARDCORE_MH_MH_H

#include "modules/hardcore/mh/mh_enum.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/util/simset.h"
#include "modules/util/twoway.h"
#include "modules/hardcore/mh/messagelisteners.h"
#include "modules/otl/list.h"

class Window;
class DocumentManager;
class URL;
class MessageHandler;
#ifdef CPUUSAGETRACKING
class CPUUsageTracker;
#endif // CPUUSAGETRACKING

class MessageHandlerObserver
	: public Link
{
public:
	virtual void MessageHandlerDestroyed(MessageHandler *mh) = 0;
	/**< Called from MessageHandler::~MessageHandler().  There is no need to
	     unregister the observer when called, this will already have been done.
	     (In fact, an OP_ASSERT will fail in MessageHandler::RemoveObserver() if
	     is called to unregister this observer.) */

protected:
	virtual ~MessageHandlerObserver() { Link::Out(); }
	/**< Never destroyed via this interface. */
};

class MessageHandler : public Link, public TwoWayPointer_Target
{
public:
	/**
	 * Creates a MessageHandler.
	 *
	 * @param window The Window that the MessageHandler belongs to, or NULL
	 *               if the MessageHandler does not belong to a Window.
	 * @param docman The DocumentManager that the MessageHandler belongs to, or NULL
	 *               if the MessageHandler belongs to a Window, or when MessageHandler
	 *				 is the g_main_message_handler.
	 */
	MessageHandler(Window* window, DocumentManager* docman = NULL);

	/**
	 * Destroys the MessageHandler. Clears up its memory usage and all
	 * pending messages.
	 */
	~MessageHandler();

	Window* GetWindow() const { return window; }
	DocumentManager* GetDocumentManager() const { return docman; }

	/**
	 * SetCallBack lets the MessageObject listen to a number of messages with a defined id.
	 *
	 * @param obj The MessageObject which will be called when messages arrive.
	 * @param id The id which must match the id of the sent message to be able to receive it.
	 *           If id is 0 the message will be forwarded even if the id's don't match.
	 * @param messagearray Array of OpMessage elements.
	 * @param messages Number of messages in the array messagearray.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */

	OP_STATUS SetCallBackList(MessageObject* obj, MH_PARAM_1 id, const OpMessage* messagearray, size_t messages);
	OP_STATUS DEPRECATED(SetCallBackList(MessageObject* obj, MH_PARAM_1 id, int unused_parameter_was_priority, const OpMessage* messagearray, size_t messages));

	/**
	 * SetCallBack lets the MessageObject listen to a specific message with a defined id.
	 *
	 * @param obj The MessageObject which will be called when messages arrive.
	 * @param msg Message to listen to.
	 * @param id The id which must match the id of the sent message to be able to receive it.
	 *           If id is 0 the message will be forwarded even if the id's don't match.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */

	/*
		What happens if the same id or msg or both is added. Check implmentation.
	 */

	OP_STATUS SetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id);
	OP_STATUS DEPRECATED(SetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id, int unused_parameter_was_priority));

	/**
	 * @deprecated This function is like RemoveCallBacks(), except it only
	 * removes the first callback.  This is probably not what any one really
	 * means.  Optimizing the case where one knows there is only one listener
	 * should not be terribly important.  Use RemoveCallBacks() instead.
	 */
	void DEPRECATED(RemoveCallBack(MessageObject* obj, MH_PARAM_1 id));

	/**
	 * @deprecated This function has never actually existed (until now).
	 * UnsetCallBack(MessageObject *, OpMessage) should be used instead.
	 */
	void DEPRECATED(RemoveCallBack(MessageObject* obj, OpMessage msg));

	void RemoveCallBacks(MessageObject* obj, MH_PARAM_1 id);

	/**
		@return TRUE if the MessageHandler has a callback for the MessageObject, and the following msg and id.
	 */
	BOOL HasCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id);

	/**
	  What is the difference between UnsetCallBack and RemoveCallBack?
	  They seem to take different parameters, but they are so confusing.
	 */
	void UnsetCallBack(MessageObject* obj, OpMessage msg, MH_PARAM_1 id);
	void UnsetCallBack(MessageObject* obj, OpMessage msg);
	void UnsetCallBacks(MessageObject* obj);

	/**
	 * Post a message to the currently running core component.
	 *
	 * @param msg The message type.
	 * @param par1 Custom message parameter 1.
	 * @param par2 Custom message parameter 2.
	 * @param delay The number of milliseconds from now that this message should be delivered.
	 *
	 * @return TRUE on success, FALSE on OOM.
	 */
	BOOL PostMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay=0);

	BOOL PostDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay);
	void RemoveDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void RemoveDelayedMessages(OpMessage msg, MH_PARAM_1 par1);
	void RemoveFirstDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void RemoveFirstDelayedMessage(OpMessage msg, MH_PARAM_1 par1);

	void HandleErrors();
	void HandleMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void PostOOMCondition( BOOL must_abort );
	void PostOODCondition();

	void AddObserver(MessageHandlerObserver *observer);
	/**< Registers an observer that will be notified when the MessageHandler is
	     destroyed.  The observer is not owned by the MessageHandler, and will
	     be called at most once, and is guaranteed to be called once unless it
	     is destroyed or unregistered before the MessageHandler is destroyed.
	     If the observer is already observing this MessageHandler, nothing
	     happens, and if it is observing another MessageHandler, it will be
	     unregistered from it automatically. */

	void RemoveObserver(MessageHandlerObserver *observer);
	/**< Unregisters an observer previously registered using AddObserver().  An
	     observer will always unregister itself in its destructor, so it is only
	     necessary to unregister it if it should stop observing before it is
	     destroyed. */

private:
	OP_STATUS HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	OP_STATUS HandleMessageBox(int id);

	Window* window;
#ifdef CPUUSAGETRACKING
	CPUUsageTracker* cpu_usage_tracker;
#endif // CPUUSAGETRACKING
	DocumentManager* docman;
	CoreComponent* component;
	HC_MessageListeners listeners;

	int handle_msg_count; // Keeps track of nesting calls to HandleMessage routine
    					  // This is necessary if a SendMessage is sent from the object's
    					  // HandleMessage routine.

	int msb_box_id;
	AutoDeleteHead msb_box_list;

	Head observers;
};

class MsgHndlListElm
{
	friend class MsgHndlList;
private:

	TwoWayPointer<MessageHandler>	mh;
	BOOL			load_as_inline;
	BOOL			check_modified_silent;
	BOOL			load_silent;

	MsgHndlListElm(MessageHandler* m, BOOL inline_loading, BOOL silent_modified_check, BOOL silent_loading) { mh = m; load_as_inline = inline_loading; check_modified_silent = silent_modified_check, load_silent = silent_loading; };

public:
	MsgHndlListElm() : load_as_inline(FALSE), check_modified_silent(FALSE), load_silent(FALSE) {}

	MessageHandler* GetMessageHandler() { return mh; }
	BOOL			GetLoadAsInline() const { return load_as_inline; };
	BOOL			GetCheckModifiedSilent() const { return check_modified_silent; };
	BOOL			GetLoadSilent() const { return load_silent; }
};

class MsgHndlList
{
public:
	typedef OtlList<MsgHndlListElm*>::Iterator Iterator;
	typedef OtlList<MsgHndlListElm*>::ConstIterator ConstIterator;

	MsgHndlList() {}
	~MsgHndlList() { Clean(); }

	void			Clean();
  	OP_STATUS		Add(MessageHandler* m, BOOL inline_loading, BOOL check_modified_silent, BOOL load_silent, BOOL always_new, BOOL first);
  	void			Remove(MessageHandler* m);
  	BOOL			HasMsgHandler(MessageHandler* m, BOOL inline_loading, BOOL check_modified_silent, BOOL load_silent);
  	MessageHandler*	FirstMsgHandler();

	void			BroadcastMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, int flags) { LocalBroadcastMessage(msg, par1, par2, flags, 0); };
	BOOL			BroadcastDelayedMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, int flags, unsigned long delay) { return LocalBroadcastMessage(msg, par1, par2, flags, delay); }
  	BOOL			IsEmpty() {CleanList(); return mh_list.IsEmpty(); }
	void			SetProgressInformation(ProgressState progress_level, unsigned long progress_info1, const uni_char *progress_info2, URL *url = NULL);

	Iterator        Begin() { return mh_list.Begin(); }
	ConstIterator   Begin() const { return mh_list.Begin(); }
	ConstIterator   End() const { return mh_list.End(); }

private:
	BOOL			LocalBroadcastMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, int flags, unsigned long delay);
	void			CleanList();

	OtlList<MsgHndlListElm*> mh_list;
};

// Utility function available by enabling API_HC_MESSAGE_NAMES
#ifdef MESSAGE_NAMES
const char* GetStringFromMessage(OpMessage);
#endif // MESSAGE_NAMES

#endif // !MODULES_HARDCORE_MH_MH_H
