/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef HARDCORE_MESSAGELISTENERS_H
#define HARDCORE_MESSAGELISTENERS_H

#include "modules/util/OpHashTable.h"

class HC_MessageListener;
class HC_MessageListeners;

/** Collection of all message listeners registered for one MessageObject in one
    MessageHandler. */
class HC_MessageObjectElement
	: private OpHashFunctions
{
public:
	HC_MessageObjectElement(HC_MessageListeners *owner, MessageObject *mo);
	virtual ~HC_MessageObjectElement();
	/**< Virtual because of virtual functions inherited from OpHashFunctions
	     and brain-damaged GCC warnings. */

	void Call(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) { mo->HandleCallback(msg, par1, par2); }

	OP_STATUS AddListener(OpMessage msg, MH_PARAM_1 id, HC_MessageListener *&ml);

	void RemoveListener(HC_MessageListener *ml);
	void RemoveListener(OpMessage msg, MH_PARAM_1 id);
	void RemoveListeners(OpMessage msg);
	void RemoveListeners(MH_PARAM_1 id);

	BOOL HasListener(OpMessage msg, MH_PARAM_1 id);

	MessageObject *GetMessageObject() { return mo; }
	BOOL Empty() { return listeners.Empty(); }


private:
	/* From OpHashFunctions. */
	virtual UINT32 Hash(const void *key);
	virtual BOOL KeysAreEqual(const void *key1, const void *key2);

	void ListenerRemoved(HC_MessageListener *ml);

	HC_MessageListeners *owner;
	MessageObject *mo;

	AutoDeleteHead listeners;
	/**< Contains HC_MessageListener objects. */
	OpHashTable listeners_table;
	/**< Maps OpMessage+MH_PARAM_1 to HC_MessageListener objects from 'listeners'. */
};

/* Swapped order of classes so that the OpAutoPointerHashTable<>'s calls to the
 * above's destructor can actually do the right thing for ADS1.2.
 */

class HC_MessageListeners
	: private OpHashFunctions
{
public:
	HC_MessageListeners();
	virtual ~HC_MessageListeners();
	/**< Virtual because of virtual functions inherited from OpHashFunctions
	     and brain-damaged GCC warnings. */

	void CallListeners(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS AddListeners(MessageObject *mo, const OpMessage *msg, size_t messages, MH_PARAM_1 par1);

	void RemoveListener(MessageObject *mo, OpMessage msg, MH_PARAM_1 id);
	void RemoveListeners(MessageObject *mo, OpMessage msg);
	void RemoveListeners(MessageObject *mo, MH_PARAM_1 id);
	void RemoveListeners(MessageObject *mo);

	BOOL HasListener(MessageObject *mo, OpMessage msg, MH_PARAM_1 par1);

	void ListenerRemoved(HC_MessageListener *ml);

private:
	/* From OpHashFunctions. */
	virtual UINT32 Hash(const void *key);
	virtual BOOL KeysAreEqual(const void *key1, const void *key2);

	HC_MessageObjectElement *FindHC_MessageObjectElement(MessageObject *mo);
	void RemoveHC_MessageObjectElement(HC_MessageObjectElement *moe);

	class IsBeingCalled
	{
	public:
		IsBeingCalled(IsBeingCalled *next)
			: next(next),
			  has_been_destroyed(FALSE)
		{
		}

		IsBeingCalled *next;
		BOOL has_been_destroyed;
	};

	static void CallListeners(AutoDeleteHead *list, OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, IsBeingCalled *is_being_called);

	OpHashTable listeners_table;
	/**< Maps OpMessage+MH_PARAM_1 to AutoDeleteHead objects containing
	     HC_MessageListenerPtr objects.  Every AutoDeleteHead contains a dummy
	     element first, which acts as the hash table key. */

	OpAutoPointerHashTable<MessageObject, HC_MessageObjectElement> messageobjects_table;
	/**< Maps MessageObject pointers to HC_MessageObjectElement objects. */

	IsBeingCalled *is_being_called;
	/**< While this */
};

/** One registered listener. */
class HC_MessageListener
	: public Link
{
public:
	HC_MessageListener(OpMessage msg, MH_PARAM_1 id)
		: msg(msg),
		  id(id)
	{
	}

	HC_MessageListener(HC_MessageObjectElement *moe, OpMessage msg, MH_PARAM_1 id)
		: moe(moe),
		  msg(msg),
		  id(id)
	{
	}

	void Call(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2) { moe->Call(msg, par1, par2); }

	BOOL Match(OpMessage msg) { return this->msg == msg; }
	BOOL Match(MH_PARAM_1 id) { return this->id == id; }

	UINT32 Hash() const { return (static_cast<UINT32>(msg) << 24) ^ static_cast<UINT32>(id); }
	static BOOL Compare(const HC_MessageListener *ml1, const HC_MessageListener *ml2) { return ml1->msg == ml2->msg && ml1->id == ml2->id; }

private:
	HC_MessageObjectElement *moe;
	OpMessage msg;
	MH_PARAM_1 id;
};

class HC_MessageListenerPtr
	: public Link
{
public:
	HC_MessageListenerPtr(HC_MessageListener *ml)
		: ml(ml),
		  locked(0)
	{
	}

	HC_MessageListener *ml;
	unsigned locked;
};

#endif // HARDCORE_MESSAGELISTENERS_H
