/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#if defined(M2_SUPPORT) && defined(SELFTEST)

#include "adjunct/m2/selftest/overrides/ST_IMAP4.h"

#include "adjunct/m2/src/backend/imap/imap-processor.h"

// Some overridden commands
namespace ImapCommands
{
	class ST_Authenticate : public Authenticate
	{

	};
};


/***********************************************************************************
 ** Create and initialize processor
 **
 ** ST_IMAP4::InitProcessor
 **
 ***********************************************************************************/
OP_STATUS ST_IMAP4::InitProcessor()
{
	// Create and initialize processor
	m_processor = new GenericIncomingProcessor;
	if (!m_processor)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Start authentication
 **
 ** ST_IMAP4::Authenticate
 **
 ***********************************************************************************/
OP_STATUS ST_IMAP4::Authenticate()
{
	return InsertCommand(new ImapCommands::ST_Authenticate, TRUE);
}

#endif // M2_SUPPORT && SELFTEST