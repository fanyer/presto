//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "popmodule.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/accountmgr.h"

#include "adjunct/m2/src/backend/pop/pop3-protocol.h"

//********************************************************************

PopBackend::PopBackend(MessageDatabase& database)
  : ProtocolBackend(database)
  , m_protocol(NULL)
  , m_uidl(database)
{
}

//********************************************************************

PopBackend::~PopBackend()
{
    OP_DELETE(m_protocol);
}

//********************************************************************

OP_STATUS PopBackend::Init(Account* account)
{
    if (!account)
        return OpStatus::ERR_NULL_POINTER;

    m_account = account;

	RETURN_IF_ERROR(m_uidl.Init(m_account->GetAccountId()));

    return OpStatus::OK;
}

//********************************************************************

OP_STATUS PopBackend::SettingsChanged(BOOL startup)
{
    if (!m_account)
        return OpStatus::ERR_NULL_POINTER;

	OP_DELETE(m_protocol);
	m_protocol = OP_NEW(POP3, (*this));
	if (m_protocol == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ret;
	if ((ret=m_protocol->Init()) != OpStatus::OK)
		return ret;

	return OpStatus::OK;
}

//********************************************************************

OP_STATUS PopBackend::Connect()
{
	m_scheduled_messages.Clear();
	return m_protocol->Connect();
}

//********************************************************************

OP_STATUS PopBackend::Disconnect()
{
	OpStatus::Ignore(Log("POP OUT: Disconnect", ""));
	return m_protocol->Cancel();
}

//********************************************************************

OP_STATUS PopBackend::FetchMessages(BOOL enable_signalling)
{
	return Connect();
}

//********************************************************************

OP_STATUS PopBackend::FetchMessage(message_index_t idx, BOOL user_request, BOOL force_complete)
{
	// Get UIDL for message
	Message message;
	RETURN_IF_ERROR(GetMessageDatabase().GetMessage(idx, FALSE, message));

	if (message.GetInternetLocation().HasContent())
	{
		// Schedule message for fetching
		RETURN_IF_ERROR(m_scheduled_messages.Insert(idx));

		return m_protocol->FetchMessage(message.GetInternetLocation());
	}
	else
		return OpStatus::OK;
}


BOOL PopBackend::IsScheduledForFetch(message_gid_t id) const
{
	return m_scheduled_messages.Contains(id);
}


/***********************************************************************************
 ** Check if a backend already has received a certain message
 **
 ** PopBackend::HasExternalMessage
 ** @param message The message to check for
 ** @return TRUE if the backend has the message, FALSE if we don't know
 ***********************************************************************************/
BOOL PopBackend::HasExternalMessage(Message& message)
{
	return m_uidl.GetUIDL(message.GetInternetLocation()) != 0;
}


/***********************************************************************************
 ** Do all actions necessary to insert an 'external' message (e.g. from import) into
 ** this backend
 **
 ** PopBackend::InsertExternalMessage
 ** @param message Message to insert
 ***********************************************************************************/
OP_STATUS PopBackend::InsertExternalMessage(Message& message)
{
	if (m_uidl.GetUIDL(message.GetInternetLocation()))
		return OpStatus::ERR;

	return m_uidl.AddUIDL(message.GetInternetLocation(), message.GetId());
}


//********************************************************************


OP_STATUS PopBackend::WriteToOfflineLog(OfflineLog& offline_log)
{
	if (m_protocol)
		return m_protocol->WriteToOfflineLog(offline_log);
	else
		return OpStatus::OK;
}


//********************************************************************

OP_STATUS PopBackend::RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently)
{
	// Nothing to do if messages are not on server
	if (!m_account->GetLeaveOnServer())
		return OpStatus::OK;

	Account* account_ptr = GetAccountPtr();
	if(permanently && account_ptr && account_ptr->GetPermanentDelete())
	{
		for (UINT32 i = 0; i < message_ids.GetCount(); i++)
		{
			Message message;

			if (OpStatus::IsSuccess(GetMessageDatabase().GetMessage(message_ids.Get(i), FALSE, message)))
				RETURN_IF_ERROR(RemoveMessage(message.GetInternetLocation()));
		}
	}
	else
	{
		// if a message is deleted then it's also marked as read == mark as read in UIDL manager
		ReadMessages(message_ids,TRUE);
	}

	return OpStatus::OK;
}

//********************************************************************

OP_STATUS PopBackend::RemoveMessage(const OpStringC8& internet_location)
{
	return m_protocol->RemoveMessage(internet_location);
}

//********************************************************************

OP_STATUS PopBackend::ReadMessages(const OpINT32Vector& message_ids, BOOL read)
{
	for (unsigned i = 0; i < message_ids.GetCount(); i++)
	{
		// Get UIDL for message
		OpString8 uidl;
		RETURN_IF_ERROR(MessageEngine::GetInstance()->GetStore()->GetMessageInternetLocation(message_ids.Get(i), uidl));
		RETURN_IF_ERROR(m_uidl.SetUIDLFlag(uidl,UidlManager::IS_READ,read));
	}

	return OpStatus::OK;
}

//********************************************************************

OP_STATUS PopBackend::EmptyTrash(BOOL done_removing_messages)
{
	if(!done_removing_messages) // do this before messages are removed locally, since we need to find out message ids
	{
		OpINT32Vector message_ids;

		Index* trash = GetMessageDatabase().GetIndex(IndexTypes::TRASH);

		if(trash)
		{
			for (INT32SetIterator it(trash->GetIterator()); it; it++)
			{
				message_gid_t message_id = it.GetData();
				Message message;
				if (OpStatus::IsSuccess(GetMessageDatabase().GetMessage(message_id, FALSE, message)) &&
					message.GetAccountId() == GetAccountPtr()->GetAccountId())
					message_ids.Add(message_id);
			}

			if (message_ids.GetCount() > 0)
				RETURN_IF_ERROR(RemoveMessages(message_ids, TRUE));
		}
	}

	return OpStatus::OK;
}

//********************************************************************

OP_STATUS PopBackend::StopFetchingMessages()
{
	return Disconnect();
}

//********************************************************************

OP_STATUS PopBackend::UpdateStore(int old_version)
{
	return m_uidl.UpdateOldUIDL(GetAccountPtr(), old_version);
}

//********************************************************************
//********************************************************************

#endif //M2_SUPPORT
