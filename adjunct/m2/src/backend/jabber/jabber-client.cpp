/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef JABBER_SUPPORT

#include "adjunct/m2/src/backend/jabber/jabber-client.h"
#include "adjunct/m2/src/backend/jabber/jabber-backend.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/m2/desktop_util/string/stringutils.h"

// ***************************************************************************
//
//	JabberClient
//
// ***************************************************************************

JabberClient::JabberClient(JabberBackend *jabber_backend)
:	m_jabber_backend(jabber_backend),
	m_has_incoming(false)
{
}


JabberClient::~JabberClient()
{
}


OP_STATUS JabberClient::Init()
{
	BOOL progress_changed = FALSE;
	m_progress.Clear(progress_changed);

	RETURN_IF_ERROR(XMLParserTracker::Init());

	RETURN_IF_ERROR(CreateReplyBuf());
	return OpStatus::OK;
}


OP_STATUS JabberClient::SubscribeToPresence(const OpStringC& jabber_id, BOOL subscribe)
{
	OpString8 jid_buffer;
	jid_buffer.Reserve(jabber_id.UTF8(jid_buffer.CStr()));
	jabber_id.UTF8(jid_buffer.CStr(), jabber_id.UTF8(jid_buffer.CStr()));

	OpString8 buffer;
	buffer.AppendFormat("<presence to='%s' type='%s'/>", Escape(jid_buffer), (subscribe ? "subscribe" : "unsubscribe"));
	return SendToConnection(buffer);
}


OP_STATUS JabberClient::AllowPresenceSubscription(const OpStringC& jabber_id, BOOL allow)
{
	OpString8 jid_buffer;
	jid_buffer.Reserve(jabber_id.UTF8(jid_buffer.CStr()));
	jabber_id.UTF8(jid_buffer.CStr(), jabber_id.UTF8(jid_buffer.CStr()));

	OpString8 buffer;
	buffer.AppendFormat("<presence to='%s' type='%s'/>", Escape(jid_buffer), (allow ? "subscribed" : "unsubscribed"));
	return SendToConnection(buffer);
}


OP_STATUS JabberClient::AnnouncePresence(const OpStringC8& presence, const OpStringC8& status_message)
{
	OpString8 escaped_status_message;
	RETURN_IF_ERROR(escaped_status_message.Set(status_message));

	if (Escape(escaped_status_message) == 0)
		return OpStatus::ERR_NO_MEMORY;

	OpString8 buffer;
	buffer.AppendFormat("<presence><show>%s</show><status>%s</status></presence>",
		presence.CStr(), escaped_status_message.CStr());

	return SendToConnection(buffer);
}


OP_STATUS JabberClient::SendMessage(const OpStringC &message, const OpStringC &chatter, OpStringC jabber_id)
{
	OpString8 message_utf8;
	message_utf8.Reserve(message.UTF8(NULL));
	message.UTF8(message_utf8.CStr(), message.UTF8(message_utf8.CStr()));

	OpString8 jabber_id_utf8;
	jabber_id_utf8.Reserve(jabber_id.UTF8(NULL));
	jabber_id.UTF8(jabber_id_utf8.CStr(), jabber_id.UTF8(jabber_id_utf8.CStr()));

	OpString8 buffer;
	buffer.AppendFormat("<message to='%s' type='chat'><body>%s</body></message>", Escape(jabber_id_utf8), Escape(message_utf8));

	RETURN_IF_ERROR(SendToConnection(buffer));

	// Show this message in chatwindow
	ChatInfo chat_info;
	RETURN_IF_ERROR(chat_info.SetChatName(chatter));

	m_jabber_backend->GetAccountPtr()->OnChatMessageReceived(MessageEngine::PRIVATE_SELF_MESSAGE, message, chat_info, chatter, TRUE);
	return OpStatus::OK;
}


OP_STATUS JabberClient::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes)
{
	XMLAttributeQN* attr = 0;

	switch (m_progress.GetStatus())
	{
		case Account::CONNECTING:
		{
			if (local_name.CompareI("stream") == 0)
				Authenticate();

			break;
		}
		case Account::AUTHENTICATING:
		{
			if (local_name.CompareI("error") == 0)
				Disconnect();
			else if (local_name.CompareI("iq") == 0)
			{
				attributes.GetData(UNI_L("type"), &attr);
				if (attr->Value().CompareI("result") == 0)
				{
					SetStatus(Account::CONNECTED);
					OpString8 presence;
					OpString8 status_message;
					m_jabber_backend->GetPresenceAndStatusMessage(Account::ONLINE, presence, status_message);
					AnnouncePresence(presence, status_message);
				}
			}

			break;
		}
		case Account::CONNECTED:
		{
			if (local_name.CompareI("message") == 0)
			{
				attributes.GetData(UNI_L("from"), &attr);
				m_incoming_message.Reset();
				m_incoming_message.SetIsReceiving(true);
				m_incoming_message.SetSender(attr->Value());
			}
			else if (m_incoming_message.IsReceiving())
			{
				if (local_name.CompareI("body") == 0)
					m_incoming_message.SetIsAwaitingBody(true);
				else if (local_name.CompareI("error") == 0)
					m_incoming_message.SetHasReceivedError(true);
			}
			else if (local_name.CompareI("presence") == 0)
			{
				attributes.GetData(UNI_L("from"), &attr);
				m_incoming_presence.Reset();
				m_incoming_presence.SetIsReceiving(true);
				m_incoming_presence.SetSender(attr->Value());

				attributes.GetData(UNI_L("type"), &attr);
				if (attr && attr->Value().CompareI("unavailable") == 0)
				{
					OpStringC offline(UNI_L("offline"));
					m_incoming_presence.SetShow(offline);
				}
			}
			else if (m_incoming_presence.IsReceiving())
			{
				if (local_name.CompareI("show") == 0)
					m_incoming_presence.SetIsAwaitingShow(true);
			}

			break;
		}
	}

	return OpStatus::OK;
}


OP_STATUS JabberClient::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name)
{
	if (m_incoming_presence.IsReceiving())
	{
		if (local_name.CompareI("show") == 0)
			m_incoming_presence.SetIsAwaitingShow(false);
		else if (local_name.CompareI("presence") == 0)
		{
			if (!m_incoming_presence.HasShow())
			{
				OpStringC show = UNI_L("online");
				m_incoming_presence.SetShow(show);
			}

			m_incoming_presence.SetIsReceiving(false);
			OnPresenceReceived();
		}
	}
	else if (m_incoming_message.IsReceiving())
	{
		if (local_name.CompareI("body") == 0)
			m_incoming_message.SetIsAwaitingBody(false);
		else if (local_name.CompareI("message") == 0)
		{
			m_incoming_message.SetIsReceiving(false);

			if (m_incoming_message.HasReceivedError())
			{
				OpStringC error(UNI_L("An error occured while sending this message"));
				OpString context;
				context.Set("Jabber error:");
				m_jabber_backend->GetAccountPtr()->OnError(m_jabber_backend->GetAccountPtr()->GetAccountId(), error, context);

				m_incoming_message.Reset();
			}
			else if (m_incoming_message.HasBody())
			{
				HotlistModelItem *contact = g_hotlist_manager->GetContactsModel()->GetByNickname(m_incoming_message.GetSender());
				if (!contact || !contact->IsContact()) // this contact does not exist, so create it
				{
					JabberID jid;
					RETURN_IF_ERROR(jid.Init(m_incoming_message.GetSender()));

					contact = g_hotlist_manager->GetContactsModel()->AddItem(OpTypedObject::CONTACT_TYPE);
					if (!contact)
						return OpStatus::ERR_NO_MEMORY;
					else
					{
						contact->SetName(jid.GetUser().CStr());
						contact->SetShortName(jid.GetFullJID().CStr());
						contact->SetIconName(UNI_L("Contact36"));
						g_hotlist_manager->GetContactsModel()->Change(contact->GetIndex(), TRUE);
					}
				}

				ChatInfo chat_info;
				chat_info.SetChatName(contact->GetName());
				m_jabber_backend->GetAccountPtr()->OnChatMessageReceived(MessageEngine::PRIVATE_MESSAGE, m_incoming_message.GetBody(), chat_info, contact->GetName(), TRUE);
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS JabberClient::OnCharacterData(const OpStringC& data)
{
	if (m_incoming_message.IsAwaitingBody())
		RETURN_IF_ERROR(m_incoming_message.AppendBody(data));
	else if (m_incoming_presence.IsAwaitingShow())
		RETURN_IF_ERROR(m_incoming_presence.SetShow(data));

	return OpStatus::OK;
}


void JabberClient::RequestMoreData()
{
	static bool sending = false;
	if (!sending && m_progress.GetStatus() == Account::CONTACTING)
	{
		OpString8 server;
		m_jabber_backend->GetServername(server);

		OpString8 buffer;
		buffer.AppendFormat("<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>",
			Escape(server));

		sending = true;

		if (OpStatus::IsSuccess(SendToConnection(buffer)))
			SetStatus(Account::CONNECTING);
		else
			SetStatus(Account::DISCONNECTED);

		sending = false;
	}
}


OP_STATUS JabberClient::ProcessReceivedData()
{
	// Get the size of the message.
	const int content_loaded = ReadData(m_reply_buf.CStr(), m_reply_buf.Capacity() - 1);
	if (content_loaded == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(Parse(m_reply_buf.CStr(), content_loaded, FALSE));
	return OpStatus::OK;
}


void JabberClient::OnClose(OP_STATUS rc)
{
	OpStatus::Ignore(Parse((uni_char *)(0), 0, TRUE)); // Reset the parser.
	SetStatus(Account::DISCONNECTED);
}


OP_STATUS JabberClient::JabberID::Init(const OpStringC& jabber_id)
{
	const int at_pos = jabber_id.Find("@");
	const int slash_pos = jabber_id.Find("/");
	const int end_pos = jabber_id.Length();

	OpString jab_id_copy;
	RETURN_IF_ERROR(jab_id_copy.Set(jabber_id));

	RETURN_IF_ERROR(m_user.Set(jab_id_copy.SubString(0, at_pos)));
	RETURN_IF_ERROR(m_domain.Set(jab_id_copy.SubString(at_pos + 1, slash_pos - at_pos - 1)));
	RETURN_IF_ERROR(m_resource.Set(jab_id_copy.SubString(slash_pos + 1, end_pos - slash_pos - 1)));
	RETURN_IF_ERROR(m_full_jid.Set(jab_id_copy));
	m_simple_jid.Empty();
	RETURN_IF_ERROR(m_simple_jid.AppendFormat(UNI_L("%s@%s"), m_user.CStr(), m_domain.CStr()));

	return OpStatus::OK;
}


OP_STATUS JabberClient::CreateReplyBuf()
{
	int new_size = 0;
	new_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
	if (new_size <= 1)
		new_size = 1*1024;

	m_reply_buf.Empty();
	if (m_reply_buf.Reserve(new_size) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


OP_STATUS JabberClient::GetProgress(Account::ProgressInfo& progress)
{
	progress = m_progress;
	return OpStatus::OK;
}


void JabberClient::SetStatus(Account::AccountStatus status)
{
	BOOL changed_status;
	m_progress.SetStatus(status, changed_status);

	if (changed_status)
		m_jabber_backend->SignalStatusChanged();

	if (status == Account::DISCONNECTED)
		m_jabber_backend->SetChatStatus(Account::OFFLINE);
}


OP_STATUS JabberClient::SendToConnection(const OpStringC8& buffer)
{
	char *send_buffer = AllocMem(buffer.Length());
	if (send_buffer == 0)
		return OpStatus::ERR_NO_MEMORY;

	memcpy(send_buffer, buffer.CStr(), buffer.Length());

	if (OpStatus::IsError(SendData(send_buffer, buffer.Length())))
		SetStatus(Account::DISCONNECTED);

	return OpStatus::OK;
}


OP_STATUS JabberClient::ReplaceAll(OpString8 &replace_in,
	const OpStringC8& replace_what, const OpStringC8& replace_with)
{
	const int replace_what_length = replace_what.Length();
	const int replace_with_length = replace_with.Length();

	int find_pos = replace_in.Find(replace_what);
	while (find_pos != KNotFound)
	{
		replace_in.Delete(find_pos, replace_what_length);
		RETURN_IF_ERROR(replace_in.Insert(find_pos, replace_with));

		// Conversion just to please StringUtils::Find.
		OpString where;
		OpString what;

		RETURN_IF_ERROR(where.Set(replace_in));
		RETURN_IF_ERROR(what.Set(replace_what));

		find_pos = StringUtils::Find(where, what, find_pos + replace_with_length);
	}

	return OpStatus::OK;
}


const char *JabberClient::Escape(OpString8 &to_escape)
{
    if ((ReplaceAll(to_escape, OpStringC8("&"), OpStringC8("&amp;")) != OpStatus::OK) ||
        (ReplaceAll(to_escape, OpStringC8("<"), OpStringC8("&lt;")) != OpStatus::OK) ||
        (ReplaceAll(to_escape, OpStringC8(">"), OpStringC8("&gt;")) != OpStatus::OK) ||
        (ReplaceAll(to_escape, OpStringC8("'"), OpStringC8("&apos;")) != OpStatus::OK) ||
        (ReplaceAll(to_escape, OpStringC8("\""), OpStringC8("&quot;")) != OpStatus::OK))
    {
        to_escape.Empty();
    }

    return to_escape.CStr();
}


OP_STATUS JabberClient::Connect()
{
	if (m_progress.GetStatus() != Account::DISCONNECTED)
		return OpStatus::ERR;

	OpString8 server;
	ProtocolConnection::port_t port;

	RETURN_IF_ERROR(m_jabber_backend->GetServername(server));
	RETURN_IF_ERROR(m_jabber_backend->GetPort(port));

	RETURN_IF_ERROR(StartLoading((char *)server.CStr(), "JABBER", port, FALSE));

	SetStatus(Account::CONTACTING);
	return OpStatus::OK;
}


OP_STATUS JabberClient::Disconnect()
{
	if (m_progress.GetStatus() == Account::DISCONNECTED)
		return OpStatus::OK;

	RETURN_IF_ERROR(Parse((uni_char *)(0), 0, TRUE)); // Reset the parser.

	OpString8 finalWord;
	finalWord.AppendFormat("<presence type='unavailable'/></stream:stream>");
	return SendToConnection(finalWord);
}


OP_STATUS JabberClient::Authenticate()
{
	SetStatus(Account::AUTHENTICATING);

	OpString8 username;
	m_jabber_backend->GetUsername(username);
	OpString8 password;
	m_jabber_backend->GetPassword(password);

	OpString8 buffer;
	buffer.AppendFormat("<iq type='set'><query xmlns='jabber:iq:auth'><username>%s</username><password>%s</password><resource>%s</resource></query></iq>",
		Escape(username), Escape(password), UNI_L("Opera"));

	return SendToConnection(buffer);
}


void JabberClient::OnPresenceReceived()
{
	Account::ChatStatus presence_status = Account::ONLINE;
	if (m_incoming_presence.HasShow())
	{
		const OpStringC& show = m_incoming_presence.GetShow();

		if (show.CompareI("away") == 0)
			presence_status = Account::AWAY;
		else if (show.CompareI("dnd") == 0)
			presence_status = Account::BUSY;
		else if (show.CompareI("xa") == 0)
			presence_status = Account::APPEAR_OFFLINE;
		else if (show.CompareI("offline") == 0)
			presence_status = Account::OFFLINE;
	}

	OpString empty;
    ChatInfo chat_info;

	m_jabber_backend->GetAccountPtr()->OnChatPropertyChanged(chat_info,
		m_incoming_presence.GetSender(), empty, MessageEngine::CHATTER_PRESENCE,
		empty, presence_status);
}

#endif // JABBER_SUPPORT
