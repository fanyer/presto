/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_READYSTATE_LISTENER_H
#define OP_SCOPE_READYSTATE_LISTENER_H

#ifdef SCOPE_SUPPORT

class FramesDocument;

/**
 * An interface for listening to certain 'milestones' of page
 * loading on a FramesDocument.
 */
class OpScopeReadyStateListener
{
public:

	enum ReadyState
	{
		/**
		 * Occurs immediately after the DOM environment is created for a
		 * FramesDocument. It is possible to use the ES_AsyncInterface after
		 * this event has been sent.
		 */
		READY_STATE_DOM_ENVIRONMENT_CREATED,

		/**
		 * This state indicates that the HTML tree is done (for now). The event
		 * happens immediately before the DOMContentLoaded event fires on the
		 * document object. Useful for figuring out when we can inspect the DOM.
		 */
		READY_STATE_BEFORE_DOM_CONTENT_LOADED,

		/**
		 * Occurs after DOMContentLoaded event has been processed. If there was
		 * no DOMContentLoaded registered, occurs immediately after the matching
		 * "before" event.
		 */
		READY_STATE_AFTER_DOM_CONTENT_LOADED,

		/**
		 * Occurs immediately before onload has fired, and after external
		 * resources are loaded.
		 */
		READY_STATE_BEFORE_ONLOAD,

		/**
		 * Occurs after onload event has been processed. If there was no
		 * load event registered, occurs immediately after the matching "before"
		 * event. When this event is observed, it should be safe to take a screenshot.
		 */
		READY_STATE_AFTER_ONLOAD,

		/**
		 * Occurs immediately before onunload has fired.
		 */
		READY_STATE_BEFORE_ONUNLOAD,

		/**
		 * Occurs after onunload is finished.
		 */
		READY_STATE_AFTER_ONUNLOAD
	};

	/**
	 * Call this whenever the ReadyState of a document changes. A message may be
	 * sent to the client about the change.
	 *
	 * @param doc The FramesDocument where the state change happened.
	 * @param state The new ReadyState.
	 */
	static void OnReadyStateChanged(FramesDocument *doc, ReadyState state);
}; // OpScopeReadyStateListener

#endif // SCOPE_SUPPORT

#endif // OP_SCOPE_READYSTATE_LISTENER_H
