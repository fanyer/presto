/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef CON_UI_GLUE

#include "modules/console/opconsoleengine.h"
#include "modules/console/opconsoleview.h"
#include "modules/console/opconsolefilter.h"

OP_STATUS OpConsoleView::Construct(OpConsoleViewHandler *handler, const OpConsoleFilter *filter)
{
	// Copy the filter
	m_filter = OP_NEW(OpConsoleFilter, ());
	if (!m_filter)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	RETURN_IF_ERROR(m_filter->Duplicate(filter));

	m_dialog_handler = handler;
	g_console->RegisterConsoleListener(this);
	return OpStatus::OK;
}

OpConsoleView::~OpConsoleView()
{
	OP_DELETE(m_filter);
	g_console->UnregisterConsoleListener(this);
}

OP_STATUS OpConsoleView::NewConsoleMessage(unsigned int id, const OpConsoleEngine::Message *message)
{
	if (m_filter->DisplayMessage(id, message))
	{
		// This message should be displayed, forward it to the dialog handler
		return m_dialog_handler->PostNewMessage(message);
	}
	else
	{
		// Just ignore the message for the time being.
		return OpStatus::OK;
	}
}

OP_STATUS OpConsoleView::SendAllComponentMessages(const OpConsoleFilter *filter)
{
	// Remember the new filter
	RETURN_IF_ERROR(m_filter->Duplicate(filter));

	m_dialog_handler->BeginUpdate();

	// Now re-send all messages
	OpConsoleEngine *console = g_console;
	unsigned int highest = console->GetHighestId();
	OP_STATUS rc = OpStatus::OK;
	for (unsigned int i = console->GetLowestId();
	     OpStatus::IsSuccess(rc) && i <= highest; i ++)
	{
		rc = NewConsoleMessage(i, console->GetMessage(i));
	}

	m_dialog_handler->EndUpdate();

	return rc;
}

#endif
