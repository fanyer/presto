/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Eirik Byrkjeflot Anonsen (eirik)
 */

#ifndef EVENT_CALLBACK_LIST
#define EVENT_CALLBACK_LIST

typedef union _XEvent XEvent;

class X11EventCallbackList
{
public:
	OP_STATUS Construct();
	~X11EventCallbackList();

	OP_STATUS addCallback(unsigned int win, bool (*cb)(XEvent *));
	void removeCallback(unsigned int win, bool (*cb)(XEvent *));

	/** Pass event to registered callback.
	 *
	 * If there is a callback registered for the X11 window associated with the passed event,
	 * then this callback is called with the event. This function returns true if A) there was
	 * an event handler registered for the X11 window associated with the passed event and
	 * B) this event handler indicated (by itself returning true) that it successfully processed the event.
	 */
	bool callCallbacks(XEvent * ev);

private:

	class CBElm
	{
	public:
		CBElm(unsigned int w, bool(*c)(XEvent*), CBElm * n)
			: win(w), cb(c), next(n) {}
		unsigned int win;
		bool (*cb)(XEvent*);
		CBElm * next;
	};

	CBElm* Find(unsigned int win)
	{
		unsigned int idx = ((uint32_t)win) % m_cb_size;
		CBElm * elm = m_cb[idx];

		while (elm != 0)
		{
			if (elm->win == win)
				return elm;

			elm = elm->next;
		}

		return 0;
	}

	CBElm ** m_cb;
	size_t m_cb_size;
};

#endif // EVENT_CALLBACK_LIST
