/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/hardcore/mh/messagelisteners.h"

static AutoDeleteHead *
MakeListenerPtrList(OpMessage msg, MH_PARAM_1 id)
{
	AutoDeleteHead *list = OP_NEW(AutoDeleteHead,);
	HC_MessageListener *ml = OP_NEW(HC_MessageListener,(msg, id));
	HC_MessageListenerPtr *ml_ptr = OP_NEW(HC_MessageListenerPtr,(ml));

	if (list && ml && ml_ptr)
	{
		++ml_ptr->locked;
		ml_ptr->Into(list);
		return list;
	}
	else
	{
		OP_DELETE(list);
		OP_DELETE(ml);
		OP_DELETE(ml_ptr);
		return NULL;
	}
}

static void
DeleteListenerPtrList(AutoDeleteHead *list)
{
	if (list && list->First())
		OP_DELETE(static_cast<HC_MessageListenerPtr *>(list->First())->ml);
	OP_DELETE(list);
}

HC_MessageListeners::HC_MessageListeners()
	: listeners_table(this),
	  is_being_called(NULL)
{
}

void
HC_MessageListeners_DeleteAutoDeleteHead(const void *key, void *data)
{
	DeleteListenerPtrList(static_cast<AutoDeleteHead *>(data));
}

/* virtual */
HC_MessageListeners::~HC_MessageListeners()
{
	listeners_table.ForEach(HC_MessageListeners_DeleteAutoDeleteHead);
	listeners_table.RemoveAll();

	while (is_being_called)
	{
		is_being_called->has_been_destroyed = TRUE;
		is_being_called = is_being_called->next;
	}
}

void
HC_MessageListeners::CallListeners(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	/* Keeps track of whether this message handler is destroyed while handling a
	   message.  If it is, we just return as soon as we can. */
	IsBeingCalled is_being_called_local(is_being_called);
	is_being_called = &is_being_called_local;

	void *list0;

	HC_MessageListener ml_exact(msg, par1);
	HC_MessageListenerPtr ml_exact_ptr(&ml_exact);
	if (listeners_table.GetData(&ml_exact_ptr, &list0) == OpStatus::OK)
	{
		AutoDeleteHead *list = static_cast<AutoDeleteHead *>(list0);
		CallListeners(list, msg, par1, par2, &is_being_called_local);
		if (is_being_called_local.has_been_destroyed)
			/* We are no more... */
			return;
		if (list->First() == list->Last())
		{
			OpStatus::Ignore(listeners_table.Remove(&ml_exact_ptr, &list0));
			OP_ASSERT(static_cast<AutoDeleteHead *>(list0) == list);
			DeleteListenerPtrList(list);
		}
	}

	if (par1 != 0)
	{
		HC_MessageListener ml_wild(msg, 0);
		HC_MessageListenerPtr ml_wild_ptr(&ml_wild);
		if (listeners_table.GetData(&ml_wild_ptr, &list0) == OpStatus::OK)
		{
			AutoDeleteHead *list = static_cast<AutoDeleteHead *>(list0);
			CallListeners(list, msg, par1, par2, &is_being_called_local);
			if (is_being_called_local.has_been_destroyed)
				/* We are no more... */
				return;
			if (list->First() == list->Last())
			{
				OpStatus::Ignore(listeners_table.Remove(&ml_wild_ptr, &list0));
				OP_ASSERT(static_cast<AutoDeleteHead *>(list0) == list);
				DeleteListenerPtrList(list);
			}
		}
	}

	is_being_called = is_being_called_local.next;
}

OP_STATUS
HC_MessageListeners::AddListeners(MessageObject *mo, const OpMessage *msg, size_t messages, MH_PARAM_1 id)
{
	/* Our error handling strategy is to either register all listeners, or leave
	   everything the way it was before.  If we fail after having registered
	   some listeners, we unregister those listeners.  If we created the message
	   object element and then fail, we remove it again. */

	HC_MessageObjectElement *moe = FindHC_MessageObjectElement(mo);

	if (!moe)
		if (!(moe = OP_NEW(HC_MessageObjectElement,(this, mo))) || messageobjects_table.Add(mo, moe) == OpStatus::ERR_NO_MEMORY)
		{
			OP_DELETE(moe);
			return OpStatus::ERR_NO_MEMORY;
		}

	BOOL registered_local[32], *registered_allocated, *registered;

	if (messages > 32)
	{
		registered = registered_allocated = OP_NEWA(BOOL, messages);
		if (!registered)
		{
			if (moe->Empty())
				RemoveHC_MessageObjectElement(moe);

			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		registered = registered_local;
		registered_allocated = NULL;
	}

	for (int index = 0; index < static_cast<int>(messages); ++index)
	{
		HC_MessageListener *ml;

		OP_STATUS status = moe->AddListener(msg[index], id, ml);
		registered[index] = status == OpStatus::OK;

		if (registered[index])
		{
			HC_MessageListenerPtr *ml_ptr = OP_NEW(HC_MessageListenerPtr,(ml));

			if (!ml_ptr)
				status = OpStatus::ERR_NO_MEMORY;
			else
			{
				void *list0;

				if (listeners_table.GetData(ml_ptr, &list0) != OpStatus::OK &&
				    (!(list0 = MakeListenerPtrList(msg[index], id)) || listeners_table.Add(static_cast<HC_MessageListenerPtr *>(static_cast<AutoDeleteHead *>(list0)->First()), list0) != OpStatus::OK))
				{
					OP_DELETE(ml_ptr);
					DeleteListenerPtrList(static_cast<AutoDeleteHead *>(list0));
					status = OpStatus::ERR_NO_MEMORY;
				}
				else
					ml_ptr->Into(static_cast<AutoDeleteHead *>(list0));
			}

			if (status == OpStatus::ERR_NO_MEMORY)
				moe->RemoveListener(ml);
		}

		if (status == OpStatus::ERR_NO_MEMORY)
		{
			while (--index >= 0)
				if (registered[index])
					moe->RemoveListener(msg[index], id);

			if (moe->Empty())
				RemoveHC_MessageObjectElement(moe);

			OP_DELETEA(registered_allocated);
			return status;
		}
	}

	OP_DELETEA(registered_allocated);
	return OpStatus::OK;
}

/* Strategy for dealing with listeners removed during a call to CallListeners():
   the HC_MessageListenerPtr object is locked, and then not removed (while the
   HC_MessageListener element isn't locked, and is removed, and the reference
   between them cleared.)  Since HC_MessageListenerPtr object isn't removed, the
   list it is in isn't removed either.

   Removed-but-locked elements and their lists (if empty afterwards) are removed
   by CallListeners(), unless locked multiple times (recursive message loop).

   Since HC_MessageListener objects can be removed at any time HC_MessageObjectElement
   objects can become empty at any time, and can also be removed.

   This all essentially means that the only things CallListeners() can rely upon
   to remain undestroyed during a message listener call is a single
   HC_MessageListenerPtr and the list in which it lives. */

void
HC_MessageListeners::RemoveListener(MessageObject *mo, OpMessage msg, MH_PARAM_1 id)
{
	if (HC_MessageObjectElement *moe = FindHC_MessageObjectElement(mo))
	{
		moe->RemoveListener(msg, id);

		if (moe->Empty())
			RemoveHC_MessageObjectElement(moe);
	}
}

void
HC_MessageListeners::RemoveListeners(MessageObject *mo, OpMessage msg)
{
	if (HC_MessageObjectElement *moe = FindHC_MessageObjectElement(mo))
	{
		moe->RemoveListeners(msg);

		if (moe->Empty())
			RemoveHC_MessageObjectElement(moe);
	}
}

void
HC_MessageListeners::RemoveListeners(MessageObject *mo, MH_PARAM_1 id)
{
	if (HC_MessageObjectElement *moe = FindHC_MessageObjectElement(mo))
	{
		moe->RemoveListeners(id);

		if (moe->Empty())
			RemoveHC_MessageObjectElement(moe);
	}
}

void
HC_MessageListeners::RemoveListeners(MessageObject *mo)
{
	if (HC_MessageObjectElement *moe = FindHC_MessageObjectElement(mo))
		RemoveHC_MessageObjectElement(moe);
}

BOOL
HC_MessageListeners::HasListener(MessageObject *mo, OpMessage msg, MH_PARAM_1 par1)
{
	if (HC_MessageObjectElement *moe = FindHC_MessageObjectElement(mo))
		return moe->HasListener(msg, par1);
	else
		return FALSE;
}

void
HC_MessageListeners::ListenerRemoved(HC_MessageListener *ml)
{
	void *list0;

	HC_MessageListenerPtr ml_ptr_local(ml);
	if (listeners_table.GetData(&ml_ptr_local, &list0) == OpStatus::OK)
	{
		AutoDeleteHead *list = static_cast<AutoDeleteHead *>(list0);
		for (HC_MessageListenerPtr *ml_ptr = static_cast<HC_MessageListenerPtr *>(list->First());
			 ml_ptr;
			 ml_ptr = static_cast<HC_MessageListenerPtr *>(ml_ptr->Suc()))
			if (ml_ptr->ml == ml)
			{
				if (ml_ptr->locked == 0)
				{
					ml_ptr->Out();
					OP_DELETE(ml_ptr);

					if (list->First() == list->Last())
					{
						OpStatus::Ignore(listeners_table.Remove(&ml_ptr_local, &list0));
						OP_ASSERT(static_cast<AutoDeleteHead *>(list0) == list);
						DeleteListenerPtrList(list);
					}
				}
				else
					ml_ptr->ml = NULL;

				break;
			}
	}
}

/* virtual */ UINT32
HC_MessageListeners::Hash(const void *key)
{
	return static_cast<const HC_MessageListenerPtr *>(key)->ml->Hash();
}

/* virtual */ BOOL
HC_MessageListeners::KeysAreEqual(const void *key1, const void *key2)
{
	return HC_MessageListener::Compare(static_cast<const HC_MessageListenerPtr *>(key1)->ml, static_cast<const HC_MessageListenerPtr *>(key2)->ml);
}

HC_MessageObjectElement *
HC_MessageListeners::FindHC_MessageObjectElement(MessageObject *mo)
{
	HC_MessageObjectElement *moe;

	if (messageobjects_table.GetData(mo, &moe) == OpStatus::OK)
		return moe;
	else
		return NULL;
}

void
HC_MessageListeners::RemoveHC_MessageObjectElement(HC_MessageObjectElement *moe)
{
	HC_MessageObjectElement *moe0;

	OpStatus::Ignore(messageobjects_table.Remove(moe->GetMessageObject(), &moe0));
	OP_ASSERT(moe == moe0);

	OP_DELETE(moe);
}

/* static */ void
HC_MessageListeners::CallListeners(AutoDeleteHead *list, OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, IsBeingCalled *is_being_called)
{
	/* The first element in the list is a dummy HC_MessageListenerPtr that acts as
	   the hash table key. */

	/* Iterate the list backwards, so that listeners registered for the same
	   message by a listener isn't called.  This is how is used to be, and
	   perhaps what one assumes (it is definitely what OpTimer assumes.) */
	for (HC_MessageListenerPtr *ptr = static_cast<HC_MessageListenerPtr *>(list->Last()), *next;
	     ptr && ptr->Pred();
	     ptr = next)
	{
		if (ptr->ml)
		{
			++ptr->locked;
			ptr->ml->Call(msg, par1, par2);
			if (is_being_called->has_been_destroyed)
				/* Oups... */
				return;
			--ptr->locked;
		}

		next = static_cast<HC_MessageListenerPtr *>(ptr->Pred());

		if (!ptr->ml && ptr->locked == 0)
		{
			ptr->Out();
			OP_DELETE(ptr);
		}
	}
}

HC_MessageObjectElement::HC_MessageObjectElement(HC_MessageListeners *owner, MessageObject *mo)
	: owner(owner),
	  mo(mo),
	  listeners_table(this)
{
}

HC_MessageObjectElement::~HC_MessageObjectElement()
{
	for (HC_MessageListener *ml = static_cast<HC_MessageListener *>(listeners.First());
	     ml;
	     ml = static_cast<HC_MessageListener *>(ml->Suc()))
		owner->ListenerRemoved(ml);

#ifdef DEBUG_ENABLE_OPASSERT
	mo->m_is_listening = FALSE;
#endif // DEBUG_ENABLE_OPASSERT
}

OP_STATUS
HC_MessageObjectElement::AddListener(OpMessage msg, MH_PARAM_1 id, HC_MessageListener *&ml)
{
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;

	if (!(ml = OP_NEW(HC_MessageListener,(this, msg, id))) || (status = listeners_table.Add(ml, ml)) != OpStatus::OK)
	{
		OP_DELETE(ml);
		return status;
	}

	ml->Into(&listeners);

#ifdef DEBUG_ENABLE_OPASSERT
	mo->m_is_listening = TRUE;
#endif // DEBUG_ENABLE_OPASSERT
	return OpStatus::OK;
}

void
HC_MessageObjectElement::RemoveListener(HC_MessageListener *ml)
{
	void *ml0;
	listeners_table.Remove(ml, &ml0);
	OP_ASSERT(ml == static_cast<HC_MessageListener *>(ml0));

	ml->Out();
	OP_DELETE(ml);

#ifdef DEBUG_ENABLE_OPASSERT
	mo->m_is_listening = !Empty();
#endif // DEBUG_ENABLE_OPASSERT
}

void
HC_MessageObjectElement::RemoveListener(OpMessage msg, MH_PARAM_1 id)
{
	HC_MessageListener ml_local(msg, id);

	void *ml0;
	if (listeners_table.Remove(&ml_local, &ml0) == OpStatus::OK)
	{
		HC_MessageListener *ml = static_cast<HC_MessageListener *>(ml0);
		owner->ListenerRemoved(ml);
		ml->Out();
		OP_DELETE(ml);
	}

#ifdef DEBUG_ENABLE_OPASSERT
	mo->m_is_listening = !Empty();
#endif // DEBUG_ENABLE_OPASSERT
}

void
HC_MessageObjectElement::RemoveListeners(OpMessage msg)
{
	for (HC_MessageListener *ml = static_cast<HC_MessageListener *>(listeners.First()), *next;
	     ml;
	     ml = next)
	{
		next = static_cast<HC_MessageListener *>(ml->Suc());

		if (ml->Match(msg))
			ListenerRemoved(ml);
	}
}

void
HC_MessageObjectElement::RemoveListeners(MH_PARAM_1 id)
{
	for (HC_MessageListener *ml = static_cast<HC_MessageListener *>(listeners.First()), *next;
	     ml;
	     ml = next)
	{
		next = static_cast<HC_MessageListener *>(ml->Suc());

		if (ml->Match(id))
			ListenerRemoved(ml);
	}
}

BOOL
HC_MessageObjectElement::HasListener(OpMessage msg, MH_PARAM_1 id)
{
	HC_MessageListener ml_local(msg, id);
	return listeners_table.Contains(&ml_local);
}

/* virtual */ UINT32
HC_MessageObjectElement::Hash(const void *key)
{
	return static_cast<const HC_MessageListener *>(key)->Hash();
}

/* virtual */ BOOL
HC_MessageObjectElement::KeysAreEqual(const void *key1, const void *key2)
{
	return HC_MessageListener::Compare(static_cast<const HC_MessageListener *>(key1), static_cast<const HC_MessageListener *>(key2));
}

void
HC_MessageObjectElement::ListenerRemoved(HC_MessageListener *ml)
{
	owner->ListenerRemoved(ml);
	RemoveListener(ml);
}
