/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Eirik Byrkjeflot Anonsen (eirik)
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/event_callback_list.h"

#include "platforms/utilix/x11_all.h"

const size_t INITIAL_LIST_SIZE = 23;

OP_STATUS X11EventCallbackList::Construct()
{
	m_cb = OP_NEWA(CBElm *, INITIAL_LIST_SIZE);
	RETURN_OOM_IF_NULL(m_cb);
	m_cb_size = INITIAL_LIST_SIZE;
	for (size_t i = 0; i < m_cb_size; ++i)
		m_cb[i] = NULL;

	return OpStatus::OK;
}

X11EventCallbackList::~X11EventCallbackList()
{
	OP_DELETEA(m_cb);
}

OP_STATUS X11EventCallbackList::addCallback(unsigned int win, bool (*cb)(XEvent *))
{
	unsigned int idx = ((uint32_t)win) % m_cb_size;
	CBElm* new_elm = OP_NEW(CBElm, (win, cb, m_cb[idx]));
	RETURN_OOM_IF_NULL(new_elm);
	m_cb[idx] = new_elm;

	return OpStatus::OK;
}

void X11EventCallbackList::removeCallback(unsigned int win, bool (*cb)(XEvent *))
{
	unsigned int idx = ((uint32_t)win) % m_cb_size;
	CBElm dummy(0, 0, m_cb[idx]);
	CBElm * elm = &dummy;
	while (elm->next != 0)
	{
		if (elm->next->win == win && (cb==0 || elm->next->cb == cb))
		{
			CBElm * old = elm->next;
			elm->next = old->next;
			OP_DELETE(old);
		}
		else
			elm = elm->next;
	}
	m_cb[idx] = dummy.next;
}

bool X11EventCallbackList::callCallbacks(XEvent * ev)
{
	CBElm * elm = 0;

	if (ev->type == EnterNotify || ev->type == LeaveNotify)
		elm = Find(ev->xcrossing.subwindow);
	else
		elm = Find(ev->xany.window);

	return elm ? elm->cb(ev) : false;
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
