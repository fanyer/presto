/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Alexander Remen (alexr
 */


#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"
#include "adjunct/m2/src/backend/imap/commands/MiscCommands.h"

OP_STATUS ImapCommands::ID::AppendCommand(OpString8& command, IMAP4& protocol) 
{ 
	OpString8 platform;
	RETURN_IF_ERROR(platform.Set(g_op_system_info->GetPlatformStr()));
	OpString8 version;
	RETURN_IF_ERROR(version.Set(VER_NUM_INT_STR_UNI));
	return command.AppendFormat("ID (\"name\" \"Opera Mail\" \"version\" \"%s\" \"vendor\" \"Opera Software ASA\" \"os\" \"%s\")", version.CStr(), platform.CStr());
}

OP_STATUS ImapCommands::EnableQResync::OnSuccessfulComplete(IMAP4& protocol)
{ 
	// if it didn't get enabled, remove the capability, something went wrong!
	if (!(protocol.GetState() & ImapFlags::STATE_ENABLED_QRESYNC)) 
		return OnFailed(protocol, ""); 
	
	return OpStatus::OK;
}

OP_STATUS ImapCommands::Lsub::PrepareToSend(IMAP4& protocol)
{ 
	protocol.ResetSubscribedFolderList(); 
	// if the server supports XLIST or SPECIAL-USE, send a command following the Lsub to get the special folders
	if ((protocol.GetCapabilities() & ImapFlags::CAP_XLIST) || (protocol.GetCapabilities() & ImapFlags::CAP_SPECIAL_USE))
		RETURN_IF_ERROR(AddDependency(OP_NEW(ImapCommands::SpecialUseList, ()), protocol));

	return OpStatus::OK;
}

OP_STATUS ImapCommands::SpecialUseList::AppendCommand(OpString8& command, IMAP4& protocol)
{
	return command.AppendFormat("%s %s%s \"*\"", 
				(protocol.GetCapabilities() & ImapFlags::CAP_XLIST) && !(protocol.GetCapabilities() & ImapFlags::CAP_SPECIAL_USE) ? "XLIST" : "LIST", 
				protocol.GetCapabilities() & ImapFlags::CAP_SPECIAL_USE ? "(SPECIAL-USE) " : "",
				protocol.GetBackend().GetFolderPrefix().CStr());
}
