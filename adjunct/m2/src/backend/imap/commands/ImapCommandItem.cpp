/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/imap/commands/ImapCommandItem.h"
#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/backend/imap/imap-flags.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"
#include "adjunct/desktop_util/string/hash.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
 ** Constructor
 **
 ** ImapCommandItem::ImapCommandItem()
 ***********************************************************************************/
ImapCommandItem::ImapCommandItem()
  : m_sent_tag(0)
  , m_sent(FALSE)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** ImapCommandItem::~ImapCommandItem()
 ***********************************************************************************/
ImapCommandItem::~ImapCommandItem()
{
	// Remove from tree if necessary
	Out();
}


/***********************************************************************************
 **
 ** Get next command in queue
 **
 ** ImapCommandItem::Next()
 ***********************************************************************************/
ImapCommandItem* ImapCommandItem::Next() const
{
	if (FirstChild())
		return FirstChild();

	if (Suc())
		return Suc();

	ImapCommandItem* next = Parent();

	while (next && !next->Suc())
		next = next->Parent();

	if (next)
		return next->Suc();

	return NULL;
}


/***********************************************************************************
 ** Make this command depend on other_command. Existing dependencies of
 ** other_command will now depend on this command.
 **
 ** ImapCommandItem::DependsOn()
 ** @param other_command Command that this command should depend on
 ** @param protocol Connection that maintains this item
 ***********************************************************************************/
void ImapCommandItem::DependsOn(ImapCommandItem* other_command, IMAP4& protocol)
{
	OP_ASSERT(other_command);

	Out();

	// Reparent existing dependencies of other_command
	for (ImapCommandItem* child = other_command->FirstChild(); child; child = other_command->FirstChild())
	{
		child->Out();
		child->Into(this);
	}

	// Now insert into other_command
	Into(other_command);

	// Progress updates
	protocol.GetBackend().NewProgress(GetProgressAction(), GetProgressTotalCount());
}


/***********************************************************************************
 ** Insert this command at the end of the specified queue
 **
 ** ImapCommandItem::IntoQueue
 ** @param queue Queue to insert the command into
 ** @param protocol Connection that maintains this item
 ***********************************************************************************/
void ImapCommandItem::IntoQueue(ImapCommandItem* queue, IMAP4& protocol)
{
	OP_ASSERT(queue);

	Into(queue);

	// Progress updates
	protocol.GetBackend().NewProgress(GetProgressAction(), GetProgressTotalCount());
}


/***********************************************************************************
 ** Insert this command at the start of the specified queue
 **
 ** ImapCommandItem::IntoStartOfQueue
 ** @param queue Queue to insert the command into
 ** @param protocol Connection that maintains this item
 ***********************************************************************************/
void ImapCommandItem::IntoStartOfQueue(ImapCommandItem* queue, IMAP4& protocol)
{
	OP_ASSERT(queue);

	Tree::IntoStart(queue);

	// Progress updates
	protocol.GetBackend().NewProgress(GetProgressAction(), GetProgressTotalCount());
}


/***********************************************************************************
 ** Make this command follow other_command (i.e. will be executed after other_command).
 ** There is no dependency relation.
 **
 ** ImapCommandItem::Follow
 ** @param other_command Command that this command should follow
 ** @param protocol Connection that maintains this item
 ***********************************************************************************/
void ImapCommandItem::Follow(ImapCommandItem* other_command, IMAP4& protocol)
{
	OP_ASSERT(other_command);

	Tree::Follow(other_command);

	// Progress updates
	protocol.GetBackend().NewProgress(GetProgressAction(), GetProgressTotalCount());
}


/***********************************************************************************
 **Remove this command and its dependencies from any queue it's in
 **
 ** ImapCommandItem::Out
 ** @param protocol Connection that maintains this item
 ***********************************************************************************/
void ImapCommandItem::Out(IMAP4& protocol)
{
	Out();

	// Progress updates
	protocol.GetBackend().ProgressDone(GetProgressAction());
}


/***********************************************************************************
 ** Sets the string 'command' to the complete IMAP command string this item
 ** represents, to be sent to the IMAP server
 **
 ** ImapCommandItem::GetImapCommand
 ** @param command Output string
 ** @param protocol Communication channel by which this command is going to be sent
 ***********************************************************************************/
OP_STATUS ImapCommandItem::GetImapCommand(OpString8& command, IMAP4& protocol)
{
	if (!IsContinuation())
	{
		// Commands that are not continuations need a tag
		OpString8 tag;

		RETURN_IF_ERROR(protocol.GenerateNewTag(tag));
		RETURN_IF_ERROR(command.AppendFormat("%s ", tag.CStr()));

		m_sent_tag = Hash::String(tag.CStr());
	}

	RETURN_IF_ERROR(AppendCommand(command, protocol));

	return command.Append("\r\n");
}


/***********************************************************************************
 ** Executes all actions necessary when this command has failed
 **
 ** ImapCommandItem::OnFailed
 ** @param protocol Communication channel that received the failure
 ** @param failed_msg Message from the server for failed command
 ***********************************************************************************/
OP_STATUS ImapCommandItem::OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
{
	// Default handling
	OpString error_message;

	g_languageManager->GetString(Str::D_MAIL_IMAP_REQUESTED_ACTION_FAILED, error_message);
	if (failed_msg.HasContent())
	{
		OpString format, failed_msg16;

		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::D_MAIL_IMAP_REQUESTED_ACTION_FAILED_SERVER, format)) &&
			OpStatus::IsSuccess(failed_msg16.Set(failed_msg)))
		{
			OpStatus::Ignore(error_message.Append(UNI_L("\n\n")));
			OpStatus::Ignore(error_message.AppendFormat(format.CStr(), failed_msg16.CStr()));
		}
	}

	protocol.GetBackend().OnError(error_message);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add a dependency to this command. Will return an error if dependency is NULL
 **
 ** ImapCommandItem::AddDependency
 ** @param dependency Dependency to add to this command
 ** @param protocol Connection
 ***********************************************************************************/
OP_STATUS ImapCommandItem::AddDependency(ImapCommandItem* dependency, IMAP4& protocol)
{
	if (!dependency)
		return OpStatus::ERR_NO_MEMORY;

	dependency->DependsOn(this, protocol);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get flags for the state normally needed
 **
 ** ImapCommandItem::GetDefaultFlags
 ***********************************************************************************/
int ImapCommandItem::GetDefaultFlags(BOOL secure, IMAP4& protocol) const
{
	int default_flags = ImapFlags::STATE_CONNECTED | ImapFlags::STATE_RECEIVED_CAPS | ImapFlags::STATE_AUTHENTICATED;

	if (secure)
		default_flags |= ImapFlags::STATE_SECURE;

	if (protocol.GetCapabilities() & ImapFlags::CAP_QRESYNC && 
		(!(protocol.GetCapabilities() & ImapFlags::CAP_COMPRESS_DEFLATE) || (protocol.GetState() & ImapFlags::STATE_COMPRESSED)))
	{
		default_flags |= ImapFlags::STATE_ENABLED_QRESYNC;
	}

	if (protocol.GetCapabilities() & ImapFlags::CAP_COMPRESS_DEFLATE)
		default_flags |= ImapFlags::STATE_COMPRESSED;

	return default_flags;
}


#endif // M2_SUPPORT
