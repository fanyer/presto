/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined OPCONSOLEVIEW_H && defined CON_UI_GLUE
#define OPCONSOLEVIEW_H

#include "modules/console/opconsoleengine.h"

class OpConsoleFilter;

/**
 * API for the UI collaborator for the OpConsoleView.
 */
class OpConsoleViewHandler
{
public:
	/** Destructor. */
	virtual ~OpConsoleViewHandler() {};

	/**
	 * Post a new incoming console message.
	 *
	 * @param message The message to be displayed.
	 * @return Status of the operation.
	 */
	virtual OP_STATUS PostNewMessage(const OpConsoleEngine::Message *message) = 0;

	/**
	 * Begin posting multiple messages. Should be called before calling
	 * PostNewMessage() more than once in succession. If called, you must
	 * call EndUpdate() when finished.
	 *
	 * The implementation of OpConsoleViewHandler can postpone updating
	 * its processing of the incoming messages until it has received them
	 * all and EndUpdate() has been called.
	 */
	virtual void BeginUpdate() = 0;

	/**
	 * Finish posting multiple messages. Cancels BeginUpdate() and must be
	 * called when the caller is done calling PostNewMessage().
	 */
	virtual void EndUpdate() = 0;

};

/**
 * Core-side of the Opera console dialogue. This class will receive
 * the messages from OpConsoleEngine and post them to the UI console
 * for display. It will also take care of opening the UI console
 * dialogue if that is requested in the preferences.
 */
class OpConsoleView : protected OpConsoleListener
{
public:
	/**
	 * First-phase constructor.
	 */
	OpConsoleView()
		: m_dialog_handler(NULL), m_filter(NULL)
	{}

	/**
	 * Second-phase constructor. The pointers passed are owned by the
	 * caller and will be copied.
	 *
	 * @param handler Dialogue handler.
	 * @param filter Filter to use for this view.
	 */
	OP_STATUS Construct(OpConsoleViewHandler *handler, const OpConsoleFilter *filter);

	virtual ~OpConsoleView();

	/**
	 * Change the associated filter and re-send all messages to the
	 * associated dialogue handler. Call this when the console dialogue
	 * needs to be refilled with messages becase the user has changed
	 * the filter settings.
	 *
	 * @param filter New filter settings to use for this view.
	 * @return Status of the operation.
	 */
	OP_STATUS SendAllComponentMessages(const OpConsoleFilter *filter);

	/**
	 * Get the current filter in use by this view.
	 *
	 * @return Pointer to the filter.
	 */
	inline OpConsoleFilter *GetCurrentFilter()
	{ return m_filter; }

protected:
	// Inherited interfaces
	virtual OP_STATUS NewConsoleMessage(unsigned int id, const OpConsoleEngine::Message *message);

private:
	/** The associated dialouge. */
	OpConsoleViewHandler *m_dialog_handler;

	/** The setting that defines what type of messages to forward to
	  * the OpConsoleViewHandler object.
	  */
	OpConsoleFilter *m_filter;
};

#endif // OPCONSOLEVIEW_H && CON_UI_GLUE
