/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef _FORMVALUELISTENER_H_
#define _FORMVALUELISTENER_H_

class FramesDocument;
class HTML_Element;
class ES_Thread;

/**
 * Gathers code that is common for form control when being manipulated
 * from DOM, action system and widgets.
 *
 * @author Daniel Bratell
 */
class FormValueListener
{
public:
	/**
	 * Sends appropriate events and does appropriate invalidation
	 * after a value has been changed.
	 *
	 * @param doc The document that has the changed control.
	 *
	 * @param elm The element with the changed value.
	 *
	 * @param send_onchange_event TRUE is onchange event should be
	 * sent. This is typically FALSE for changes in text fields before
	 * the focus has left the field, and false for changes done from
	 * DOM.
	 *
	 * @param has_user_origin TRUE if this is caused by a user
	 * action. This is to determine if the change indicates that the
	 * user has made changes that has to be preserved.
	 *
	 * @param thread The ecmascript thread that caused this change or
	 * NULL if it wasn't triggered by ecmascript. This is needed to
	 * get the order of the events correctly.
	 *
	 */
	static OP_STATUS HandleValueChanged(FramesDocument* doc,
										HTML_Element* elm,
										BOOL send_onchange_event,
										BOOL has_user_origin,
										ES_Thread* thread);

};

#endif // _FORMVALUELISTENER_H_
