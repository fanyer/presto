/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "m2glue.h"

#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/glue/connection.h"
#include "adjunct/m2/src/glue/mh.h"
#include "adjunct/m2/src/glue/mime.h"
#include "adjunct/m2/src/glue/util.h"
#include "adjunct/m2/src/util/buffer.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/quick/Application.h"
#include "adjunct/m2_ui/dialogs/AccountSubscriptionDialog.h"
#include "adjunct/m2_ui/dialogs/AccountQuestionDialogs.h"
#include "adjunct/m2_ui/dialogs/ChangeNickDialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/mail/mailformatting.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#include "modules/doc/doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/encodings/charconverter.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/decoders/utf7-decoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/encoders/utf7-encoder.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/idna/idna.h"
#include "modules/layout/box/box.h"
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/pi/OpDLL.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/url/uamanager/ua.h"
#ifdef FORMATS_CAP_MODULE
# include "modules/formats/encoder.h"
#else
# include "modules/url/mime/encoder.h"
#endif
#ifdef MIME_CAP_MODULE
# include "modules/mime/mimedec2.h"
# include "modules/mime/mime_module.h"
#else
# include "modules/url/mime/mimedec2.h"
#endif
#ifdef UPLOAD_CAP_MODULE
# include "modules/upload/upload.h"
#else
# include "modules/url/mime/upload2.h"
#endif
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/url/url2.h"
#include "modules/util/filefun.h"
#include "modules/util/OpTypedObject.h"
#include "modules/util/path.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "modules/util/opfile/unistream.h"

#if defined(MSWIN) || defined(UNIX)
#  include "modules/util/gen_str.h"
#endif

#if defined (MSWIN)
#include "platforms/windows/windows_ui/registry.h"
# include "platforms/windows/network/WindowsSocket2.h"	//For Win32 dynamic run-time linked gethostbyaddr, gethostbyname and gethostname
#elif defined(_UNIX_DESKTOP_) || defined(_MACINTOSH_)
#include <netdb.h> // gethostbyname
#endif

#ifdef WAND_SUPPORT
# include "modules/wand/wandmanager.h"
#endif

#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
#include "modules/libssl/ssl_api.h"
#include "modules/url/url_sn.h" // ServerName
#endif

#include "modules/dochand/win.h"

#ifdef __SUPPORT_OPENPGP__
#ifdef UPLOAD_CAP_MODULE
#include "modules/upload/upload_build.h"
#else
#include "modules/url/mime/upload_build.h"
#endif
#endif

#if !defined(URL_CAP_GLOBAL_OBJECTS_IN_GOPERA)
extern MpSocketFactory* g_socket_factory;
extern MpSocketAddressFactory* g_sockaddr_factory;
extern MpHostResolverFactory* g_resolver_factory;
#endif

class ProtocolComm;
class ServerName;

// ----------------------------------------------------

#ifdef _DEBUG
OpVector<DebugReferences> g_debug_references;
#endif

#ifdef _DEBUG
void AddToReferencesList(DebugReferences::DebugReferencesTypes type, const OpStringC8& source_file, int source_line, const void* object_ptr)
{
	OP_ASSERT(object_ptr);

	DebugReferences* dbg_ref = OP_NEW(DebugReferences, ());
	OP_ASSERT(dbg_ref);
	if (dbg_ref!=NULL)
	{
		dbg_ref->type = type;
		OpStatus::Ignore(dbg_ref->source_file.Set(source_file));
		dbg_ref->source_line = source_line;
		dbg_ref->object_ptr = object_ptr;

		g_debug_references.Add(dbg_ref);
	}
}
#endif //_Debug

#ifdef _DEBUG
void RemoveFromReferencesList(DebugReferences::DebugReferencesTypes type, const void* object_ptr)
{
	if (!object_ptr)
		return;

	UINT32 count = g_debug_references.GetCount();
	DebugReferences* dbg_ref = NULL;
	UINT32 i;
	for (i=0; i<count; i++)
	{
		dbg_ref = g_debug_references.Get(i);
		if (dbg_ref && dbg_ref->type==type && dbg_ref->object_ptr==object_ptr)
			break;
	}
	OP_ASSERT(dbg_ref && i<count); //If dbg_ref==NULL, the given object has not been allocated using Create-functions!
	if (dbg_ref && i<count)
	{
		g_debug_references.Delete(i);
	}
}
#endif //_Debug

// ----------------------------------------------------

MailerGlue::MailerGlue()
:
  m_engine_glue(NULL),
  m_factory(NULL)
{
#ifdef _DEBUG
	g_debug_references.Clear();
#endif
}

MailerGlue::~MailerGlue()
{
    OP_ASSERT(m_factory == NULL); //Should be deleted by calling MailerGlue::Stop()
    OP_ASSERT(m_engine_glue == NULL); //Should be deleted by calling MailerGlue::Stop()

    if (m_engine_glue || m_factory)
        Stop();

#if (1) && defined (_DEBUG)
	UINT32 i;
	DebugReferences* dbg_ref;
	for (i=0; i<g_debug_references.GetCount(); i++)
	{
		dbg_ref = g_debug_references.Get(i);
//		OP_ASSERT(0); //If you get an assert here, please take a note of dbg_ref->type, dbg_ref->source_file and
					  //dbg_ref->source_line, and notify someone on the M2 team.
	}

	g_debug_references.Clear();
#endif
}

OP_STATUS MailerGlue::Start(OpString8& status)
{
	status.Empty();

    if (!(m_factory = OP_NEW(MailerUtilFactory, ())))
    {
        status.Append("Out of memory\n");
        return OpStatus::ERR_NO_MEMORY;
    }

	MessageEngine::CreateInstance();
	if (!g_m2_engine)
		return OpStatus::ERR_NO_MEMORY;

	m_engine_glue = g_m2_engine;

	// set us as listener
#ifdef IRC_SUPPORT
	m_engine_glue->AddChatListener(this);
#endif // IRC_SUPPORT
	m_engine_glue->AddInteractionListener(this);

	// export the factories

	m_engine_glue->SetFactories(m_factory);

	// let's go

	{
		OP_PROFILE_METHOD("MailEngineGlue::Init completed");

		OP_STATUS result = m_engine_glue->Init(status);

		if (OpStatus::IsError(result))
		{
			if (status.IsEmpty())
				status.Set("Engine Init() failed\n");
			return result;
		}
	}
	return OpStatus::OK;
}

OP_STATUS MailerGlue::Stop()
{
	// remove us as listener

    if (m_engine_glue)
    {
#ifdef IRC_SUPPORT
	    m_engine_glue->RemoveChatListener(this);
#endif // IRC_SUPPORT
	    m_engine_glue->RemoveInteractionListener(this);
    }

	MessageEngine::DestroyInstance();

    m_engine_glue = NULL; //Has been deleted when calling delete_ui

    OP_DELETE(m_factory); //Because of OpString and HeapProblems, m_factory should be the deleted last!
    m_factory = NULL;

    return OpStatus::OK;
}

// Debug function, should be removed when M2 gets a real UI

OP_STATUS MailerGlue::MailCommand(URL& url)
{
	return m_engine_glue->MailCommand(url);
}

// Implementing the MessageEngine listener interfaces

#ifdef IRC_SUPPORT
void MailerGlue::OnChatRoomJoined(UINT16 account_id, const ChatInfo& room)
{
	g_application->GoToChat(account_id, room, TRUE, TRUE);
}

void MailerGlue::OnChatMessageReceived(UINT16 account_id,
	EngineTypes::ChatMessageType type, const OpStringC& message,
	const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat)
{
	ChatDesktopWindow* window = ChatDesktopWindow::FindChatRoom(account_id, chat_info.ChatName(), !is_private_chat);

	if (window == NULL)
	{
		if (is_private_chat)
		{
			if (type == EngineTypes::PRIVATE_MESSAGE ||
				type == EngineTypes::PRIVATE_ACTION_MESSAGE)
			{
				BOOL open_in_background = FALSE;

				AccountManager* account_manager = g_m2_engine ? g_m2_engine->GetAccountManager() : 0;
				if (account_manager != 0)
				{
					Account* account = 0;
					account_manager->GetAccountById(account_id, account);

					OP_ASSERT(account != 0);
					open_in_background = account->GetOpenPrivateChatsInBackground();
				}

				g_application->GoToChat(account_id, chat_info, FALSE, FALSE, open_in_background);
			}
			else if (type == EngineTypes::PRIVATE_SELF_MESSAGE)
			{
				// Perhaps open a window in the background if we sent a message
				// ourself, without an existing a chat window for it?
				g_application->GoToChat(account_id, chat_info, FALSE, FALSE, TRUE);
			}
		}
	}
}

void MailerGlue::OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room)
{
	// Ensure a window exists for this nickname.
	ChatInfo Nicholas(nick);
	OnChatMessageReceived(account_id, EngineTypes::PRIVATE_ACTION_MESSAGE,
		UNI_L(""), Nicholas, nick, TRUE);
}

void MailerGlue::OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender,
	const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id)
{
	// Ensure a window exists for the sender of the file.
	ChatInfo chat_info(sender);
	OnChatMessageReceived(account_id, EngineTypes::PRIVATE_ACTION_MESSAGE,
		UNI_L(""), chat_info, sender, TRUE);
}
#endif // IRC_SUPPORT

void MailerGlue::OnChangeNickRequired(UINT16 account_id, const OpStringC& old_nick)
{
#ifdef IRC_SUPPORT
	ChangeNickDialog* dialog = OP_NEW(ChangeNickDialog, (ChangeNickDialog::CHANGE_NICK_IN_USE,
		account_id, old_nick));

    if (!dialog)
        return;

    dialog->Init(g_application->GetActiveDesktopWindow());
#endif // IRC_SUPPORT
}


void MailerGlue::OnRoomPasswordRequired(UINT16 account_id, const OpStringC& room)
{
#ifdef IRC_SUPPORT
	RoomPasswordDialog* dialog = OP_NEW(RoomPasswordDialog, (account_id, room));

    if (!dialog)
        return;

	DesktopWindow * window = NULL;
	if (room.HasContent())
		window = ChatDesktopWindow::FindChatRoom(account_id, room, TRUE);
	if (!window)
		window = g_application->GetActiveDesktopWindow();
	dialog->Init(window);
#endif // IRC_SUPPORT
}

void MailerGlue::OnYesNoInputRequired(UINT16 account_id, EngineTypes::YesNoQuestionType type, OpString* sender, OpString* param)
{
	switch(type)
	{
		case EngineTypes::DELETE_DOWNLOADED_MESSAGES_ON_SERVER:
		{
			AskServerCleanupController* controller = OP_NEW(AskServerCleanupController, (account_id));
			OpStatus::Ignore(ShowDialog(controller,g_global_ui_context, g_application ? g_application->GetActiveDesktopWindow() : NULL));
			break;
		}

	default:
		OP_ASSERT(0);	//this is wrong
		break;
	}
}

void MailerGlue::OnError(UINT16 account_id, const OpStringC& errormessage, const OpStringC& context, EngineTypes::ErrorSeverity severity)
{
	OpConsoleEngine::Severity console_severity;
	switch(severity)
	{
		case EngineTypes::VERBOSE:
			console_severity = OpConsoleEngine::Verbose;
			break;
		case EngineTypes::INFORMATION:
			console_severity = OpConsoleEngine::Information;
			break;
		case EngineTypes::GENERIC_ERROR:
			console_severity = OpConsoleEngine::Error;
			break;
		case EngineTypes::CRITICAL:
			console_severity = OpConsoleEngine::Critical;
			break;
		case EngineTypes::DONOTSHOW:
		default:
			console_severity = OpConsoleEngine::DoNotShow;
			break;
	}
	if (g_console)
	{
		OpConsoleEngine::Message msg(OpConsoleEngine::M2, console_severity);
		msg.context.Set(context);
		msg.message.Set(errormessage);
		msg.url.Empty();

		TRAPD(rc, g_console->PostMessageL(&msg));
	}
}

// ----------------------------------------------------

class CommGlue
	: public ProtocolComm,      // from Opera
	  public ProtocolConnection, // from m2
	  public Autodeletable,
	  public OpTimerListener
{
public:

	CommGlue();

	~CommGlue();

	// ProtocolConnection (from m2)
	OP_STATUS SetClient(Client* client);
	OP_STATUS Connect(const char* hostname, const char* service, port_t port, BOOL ssl, BOOL V3_handshake = FALSE);

	unsigned int Read(char* buf, size_t buflen);
	OP_STATUS Send(char *buf, size_t buflen);

	OP_STATUS SetTimeout(time_t seconds, BOOL timeout_on_idle);
	void	  RestartTimeoutTimer();
	void	  OnTimeOut(OpTimer* timer);
	OP_STATUS StartTLS();
	virtual void StopConnectionTimer();

	char* AllocBuffer(size_t size);
	void FreeBuffer(char* buf);
	void Release();

private:
	// ProtocolComm (from Opera)
	void RequestMoreData();
	void ProcessReceivedData();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual void SetProgressInformation(ProgressState progress_level, unsigned long progress_info1=0, const uni_char *progress_info2 = NULL);

	UINT TimeoutCheckInterval() const { return m_timeout_secs > 0 ? MAX((m_timeout_secs * 1000) / 5, 1000) : 0; }

	Client* m_client;

	ServerName* m_hostname;
	port_t      m_port;

	time_t		m_timeout_secs;
	BOOL		m_timeout_on_idle;
	OpTimer		m_timeout_timer;
	BOOL		m_ssl;
};

// ----------------------------------------------------

CommGlue::CommGlue()
:	ProtocolComm(g_main_message_handler, NULL, NULL),
	m_client(NULL),
	m_hostname(NULL),
	m_port(0),
	m_timeout_secs(0),
	m_ssl(FALSE)
{
}

CommGlue::~CommGlue()
{
#ifdef _DEBUG
	OP_ASSERT(m_ok_to_delete==TRUE);
#endif
}

void CommGlue::Release()
{
	OP_DELETE(this);
}

OP_STATUS CommGlue::SetClient(Client* client)
{
	m_client = client;
	return OpStatus::OK;
}

unsigned int CommGlue::Read(char* buf, size_t buflen)
{
	return ProtocolComm::ReadData(buf, buflen);
}

OP_STATUS CommGlue::Send(char *buf, size_t buflen)
{
	ProtocolComm::SendData(buf, buflen);

	if (ProtocolComm::Closed())
	{
		if (GetSink() != 0)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), ERR_COMM_CONNECTION_CLOSED);

		mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
	}
	else
	{
		// We have sent something, start timeout timer
		if (buflen)
			RestartTimeoutTimer();
	}

	return OpStatus::OK;
}

OP_STATUS CommGlue::SetTimeout(time_t seconds, BOOL timeout_on_idle)
{
	m_timeout_secs    = seconds;
	m_timeout_on_idle = timeout_on_idle;

	// Make sure we get timeout events
	m_timeout_timer.SetTimerListener(this);

	return OpStatus::OK;
}

void CommGlue::RestartTimeoutTimer()
{
	m_timeout_timer.Stop();

	// Start timer if necessary
	if (m_timeout_secs)
		m_timeout_timer.Start(m_timeout_secs * 1000);
}

void CommGlue::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &m_timeout_timer);

	// Signal client that connection will close
	if (m_client)
		m_client->OnClose(OpStatus::OK);

	// Close connection
	Stop();
}

char* CommGlue::AllocBuffer(size_t size)
{
	return OP_NEWA(char, size);
}

void CommGlue::FreeBuffer(char* buf)
{
	OP_DELETEA(buf);
}

OP_STATUS CommGlue::Connect(const char* hostname, const char* service, port_t port, BOOL ssl, BOOL V3_handshake)
{
	m_hostname = urlManager->GetServerName(hostname, TRUE);
	m_port     = port;
	m_ssl      = ssl;

	SComm* comm = Comm::Create(g_main_message_handler, m_hostname, m_port, FALSE);
	if (comm)
	{
		// M2 must handle the number of open connections independently from
		// normal browser Comm objects. This means that when Comm runs out
		// of connections, mail connections will not be taken down, and that
		// can have negative performance impact on browsing unless M2 makes
		// sure to keep the number of open connections to a minimum.

		((Comm*)comm)->SetManagedConnection();
	}

	if (ssl)
	{
		ProtocolComm *ssl_comm = g_ssl_api->Generate_SSL(g_main_message_handler,  m_hostname, m_port, V3_handshake);
		if (ssl_comm == NULL)
		{
			OP_DELETE(comm);
			return OpStatus::ERR;
		}

		ssl_comm->SetNewSink(comm);
		comm = ssl_comm;
	}

	SetNewSink(comm);
	SetCallbacks(NULL,NULL);

	if (ProtocolComm::Load() != COMM_LOADING)
	{
		return OpStatus::ERR;
	}

	// Restart timer
	RestartTimeoutTimer();

	return OpStatus::OK;
}

void CommGlue::StopConnectionTimer()
{
	m_timeout_timer.Stop();
}

OP_STATUS CommGlue::StartTLS()
{
#ifdef _SSL_SUPPORT_
	return ProtocolComm::InsertTLSConnection(m_hostname, m_port) ? OpStatus::OK : OpStatus::ERR;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // _SSL_SUPPORT_
}

void CommGlue::RequestMoreData()
{
	SetDoNotReconnect(TRUE);
    if (m_client)
	    m_client->RequestMoreData();
}

void CommGlue::ProcessReceivedData()
{
	OP_STATUS ret = OpStatus::OK;

	// Data received, stop timer
	RestartTimeoutTimer();

	// Let client process data
	if (m_client)
		ret = m_client->ProcessReceivedData();

	// See if we were disconnected
	if (ret != OpStatus::ERR_NO_SUCH_RESOURCE && ProtocolComm::Closed())
	{
		if (GetSink() != 0)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), ERR_COMM_CONNECTION_CLOSED);

		mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
	}
}

void CommGlue::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_COMM_LOADING_FINISHED:
			g_main_message_handler->RemoveCallBacks(this, par1); // prevents ssl connections to send double MSG_COMM_LOADING_FINISHED
            if (m_client)
			    m_client->OnClose(OpStatus::OK);
			break;
		case MSG_COMM_LOADING_FAILED:
			if (m_client)
			{
				// ERR_COMM_HOST_NOT_FOUND is offline for windows, we don't need to report an error then
				if (par2 == Str::S_MSG_SSL_NonFatalError_Retry)
					m_client->OnRestartRequested();
				else
					m_client->OnClose((par2==ERR_COMM_CONNECTION_CLOSED || par2==ERR_SSL_ERROR_HANDLED || par2==ERR_COMM_HOST_NOT_FOUND) ?
				    				   OpStatus::OK : OpStatus::ERR_NO_ACCESS); // no server contact?
			}
			break;
#ifdef LIBSSL_CAP_HAS_RESTART_MESSAGE
		case MSG_SSL_RESTART_CONNECTION:
			if (m_client)
				m_client->OnRestartRequested();
			break;
#endif // LIBSSL_CAP_HAS_RESTART_MESSAGE
		default:
			OP_ASSERT((int)msg != ERR_COMM_CONNECTION_CLOSED);	//If this is reached, contact julienp@opera.com or arjanl@opera.com
			return;
	}
}

void CommGlue::SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1,
											 const uni_char *progress_info2)
{
	if(progress_level == RESTART_LOADING)
	{
		if(progress_info2)
			*((BOOL *) progress_info2) = m_ssl && (is_connected ? FALSE : TRUE);

		return;
	}
	ProtocolComm::SetProgressInformation(progress_level, progress_info1, progress_info2);
}


// ----------------------------------------------------

class MessageLoopGlue : public MessageLoop, public MessageObject
{
public:
	MessageLoopGlue();
	~MessageLoopGlue();

	void Release();

	OP_STATUS SetTarget(Target* target);

	OP_STATUS Post(OpMessage message, time_t delay = 0);
	OP_STATUS Send(OpMessage message);

	void HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam);
private:
	Target *m_target;
};

MessageLoopGlue::MessageLoopGlue()
{
	m_target = NULL;
}

MessageLoopGlue::~MessageLoopGlue()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void
MessageLoopGlue::Release()
{
	OP_DELETE(this);
}

OP_STATUS MessageLoopGlue::SetTarget(Target* target)
{
	m_target = target;
	return OpStatus::OK;
}

OP_STATUS MessageLoopGlue::Post(OpMessage message, time_t delay)
{
	if (!g_main_message_handler->HasCallBack(this, message, 0))
	{
		g_main_message_handler->SetCallBack(this, message, 0);
	}
//	g_main_message_handler->SendMessage(message,(int)this,(int)m_target);
	g_main_message_handler->PostDelayedMessage(message, (MH_PARAM_1)this, (MH_PARAM_2)m_target, delay);
	return OpStatus::OK;
}

OP_STATUS MessageLoopGlue::Send(OpMessage message)
{
	if (!g_main_message_handler->HasCallBack(this, message, 0))
	{
		g_main_message_handler->SetCallBack(this, message, 0);
	}
	g_main_message_handler->PostMessage(message, (MH_PARAM_1)this, (MH_PARAM_2)m_target); // used to be SendMessage in Core-1
	return OpStatus::OK;
}

void MessageLoopGlue::HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam)
{
	if ((wParam == (MH_PARAM_1)this) && (lParam == (MH_PARAM_2)m_target))
	{
		m_target->Receive(msg);
	}
}

// ***************************************************************************
//
//	BrowserUtilsURLStream
//
// ***************************************************************************

BrowserUtilsURLStream::BrowserUtilsURLStream()
:	m_source_len(0),
	m_has_more(FALSE)
{
}


BrowserUtilsURLStream::~BrowserUtilsURLStream()
{
}


OP_STATUS BrowserUtilsURLStream::Init(URL& url)
{
	OP_ASSERT(m_data_descriptor.get() == 0);

	RETURN_IF_ERROR(MarkURLAsGenerated(url));

	// Fetch the data descriptor that we will read data from.
	m_data_descriptor = url.GetDescriptor(NULL, TRUE);
	if (m_data_descriptor.get() == 0)
		return URL_UNKNOWN_CONTENT;

	m_has_more = TRUE;
	return OpStatus::OK;
}


OP_STATUS BrowserUtilsURLStream::NextChunk(OpString& chunk)
{
	OP_ASSERT(HasMoreChunks());

	m_has_more = FALSE;
	TRAPD(err, m_source_len = m_data_descriptor->RetrieveDataL(m_has_more));
	if (OpStatus::IsError(err))
	{
		m_has_more = FALSE;
		return URL_UNKNOWN_CONTENT;
	}

	if (m_source_len == 0)
	{
		m_has_more = FALSE;
		return URL_UNKNOWN_CONTENT;
	}

	const uni_char *src = (const uni_char *)(m_data_descriptor->GetBuffer());
	if (src == 0)
	{
		m_has_more = FALSE;
		return URL_UNKNOWN_CONTENT;
	}

	chunk.Empty();
	RETURN_IF_ERROR(chunk.Append(src, UNICODE_DOWNSIZE(m_source_len)));

	m_data_descriptor->ConsumeData(m_source_len);
	return OpStatus::OK;
}


OP_STATUS BrowserUtilsURLStream::NextChunk(OpByteBuffer& chunk)
{
	OP_ASSERT(HasMoreChunks());

	m_has_more = FALSE;
	TRAPD(err, m_source_len = m_data_descriptor->RetrieveDataL(m_has_more));
	if (OpStatus::IsError(err))
		return URL_UNKNOWN_CONTENT;

	unsigned char *src = (unsigned char *)m_data_descriptor->GetBuffer();
	if (src == NULL || m_source_len == 0)
		return URL_UNKNOWN_CONTENT;

	chunk.Empty();
	RETURN_IF_ERROR(chunk.Append(src, m_source_len));

	m_data_descriptor->ConsumeData(m_source_len);
	return OpStatus::OK;
}


OP_STATUS BrowserUtilsURLStream::MarkURLAsGenerated(URL& url) const
{
	// Hack to mark the url as generated; avoiding that Opera will use the
	// default encoding selected for web pages. Should be replaced by
	// something better eventually.
	RETURN_IF_ERROR(url.SetAttribute(URL::KIsGenerated, 1));
	return OpStatus::OK;
}

// ----------------------------------------------------

BrowserUtilsGlue::BrowserUtilsGlue()
{
}

BrowserUtilsGlue::~BrowserUtilsGlue()
{
}

const char* BrowserUtilsGlue::GetLocalHost()
{
    return Comm::GetLocalHostName();
}


const char* BrowserUtilsGlue::GetLocalFQDN()
{
	// TODO find out what this does and why
	//This function should be part of PortingInterface MpHostResolver
	const char* hostname = GetLocalHost();
	if (hostname && *hostname)
	{
		struct hostent* host = gethostbyname(hostname);
		if (host && host->h_addr_list && host->h_addr_list[0])
		{
			host = gethostbyaddr(host->h_addr_list[0],host->h_length, host->h_addrtype);
			if (host && host->h_name)
			{
				return host->h_name;
			}
		}
	}
	return NULL;
}


OP_STATUS BrowserUtilsGlue::ResolveUrlName(const OpStringC& szNameIn, OpString& szResolved)
{
    TRAPD(ret, ResolveUrlNameL(szNameIn, szResolved));
    return ret;
}

OP_STATUS BrowserUtilsGlue::GetURL(URL*& url, const uni_char* url_string)
{
    if (!url)
        return OpStatus::ERR_NULL_POINTER;

    *url = g_url_api->GetURL(url_string);

    return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::WriteStartOfMailUrl(URL* url, const Message* message)
{
    if (!url)
        return OpStatus::ERR_NULL_POINTER;

	url->Unload();

	RETURN_IF_ERROR(url->SetAttribute(URL::KMIME_ForceContentType, "text/html"));

	OpString body_comment;
	if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_MESSAGE_BODY_NOT_DOWNLOADED, body_comment)))
	{
		OpStatus::Ignore(SetUrlComment(url, body_comment));
	}

	//Decide TextpartSetting
	Message::TextpartSetting body_mode = (Message::TextpartSetting)g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode);
	if (message->IsFlagSet(Message::PARTIALLY_FETCHED))
	{
		body_mode = Message::PREFER_TEXT_PLAIN;
	}
	else if (body_mode==Message::DECIDED_BY_ACCOUNT && message)
	{
		body_mode = message->GetTextPartSetting();
	}

	RETURN_IF_ERROR(url->SetAttribute(URL::KIgnoreWarnings, TRUE));

	RETURN_IF_ERROR(url->SetAttribute(URL::KPreferPlaintext, body_mode==Message::PREFER_TEXT_PLAIN ||
													  body_mode==Message::FORCE_TEXT_PLAIN));

	return url->SetAttribute(URL::KBigAttachmentIcons, FALSE);
}

OP_STATUS BrowserUtilsGlue::WriteEndOfMailUrl(URL* url, const Message* message)
{
    if (!url)
        return OpStatus::ERR_NULL_POINTER;

	url->WriteDocumentDataFinished();
	url->ForceStatus(URL_LOADED);
	return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::WriteToUrl(URL* url, const char* data, int length) const
{
	if (!url)
		return OpStatus::ERR_NULL_POINTER;

	while (length)
	{
		// making smaller portions to try to optimize speed of MIME decoding (johan 20050825)

		int chunk = length;
		const char* chunk_start = data;
		const int chunk_size = 100*1024;

		if (length > chunk_size)
		{
			chunk = chunk_size;
			length -= chunk_size;
			data += chunk_size;
		}
		else
		{
			length = 0; // break out after write
		}
		url->WriteDocumentData(URL::KNormal, chunk_start, chunk);
	}
	return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::SetUrlComment(URL* url, const OpStringC& comment)
{
	if (!url)
		return OpStatus::ERR_NULL_POINTER;

	return url->SetAttribute(URL::KBodyCommentString, comment);
}

OP_STATUS BrowserUtilsGlue::CreateTimer(OpTimer** timer, OpTimerListener* listener)
{
	OP_ASSERT(!(*timer)); // don't overwrite existing timers

	if (!(*timer = OP_NEW(OpTimer, ())))
		return OpStatus::ERR_NO_MEMORY;

	if(listener)
		(*timer)->SetTimerListener(listener);

	return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::GetUserAgent(OpString8& useragent, UA_BaseStringId override_useragent)
{
	OpString ua;
	ua.AppendFormat(UNI_L("Opera Mail/%s (%s)"), VER_NUM_STR_UNI, g_op_system_info->GetPlatformStr());
	return useragent.Set(ua.CStr());
}

time_t BrowserUtilsGlue::CurrentTime()
{
    return g_timecache->CurrentTime();
}

long BrowserUtilsGlue::CurrentTimezone()
{
	/*
	IMPORTANT: This function returns the local timezone without adjustments for daylight savings time
	*/
	return g_op_time_info->GetTimezone() + (long)(g_op_time_info->DaylightSavingsTimeAdjustmentMS(g_op_time_info->GetTimeUTC())/1000);
}

double BrowserUtilsGlue::GetWallClockMS()
{
	return g_op_time_info->GetWallClockMS();
}

BOOL BrowserUtilsGlue::OfflineMode()
{
	return g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode);
}

BOOL BrowserUtilsGlue::TLSAvailable()
{
	BOOL tls = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableSSL3_1); //TLS1.0
	//tls |= g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTLS1_1); //TLS1.1
	return tls;
}

OP_STATUS BrowserUtilsGlue::ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& address, BOOL& is_mailaddress) const
{
	address.Empty();
	if (!address.Reserve(24+imaa_address.Length()*4)) //This size should defintly be calculated more accurate
		return OpStatus::ERR_NO_MEMORY;

	TRAPD(err, IDNA::ConvertFromIDNA_L(imaa_address.CStr(), const_cast<uni_char*>(address.CStr()), address.Capacity()-1, is_mailaddress));
	if(OpStatus::IsError(err))
		return err;

	return OpStatus::OK;
}


OP_STATUS BrowserUtilsGlue::PathDirFileCombine(OUT OpString& dest, IN const OpStringC& directory, IN const OpStringC& file) const
{
	return OpPathDirFileCombine(dest, directory, file);
}

OP_STATUS BrowserUtilsGlue::PathAddSubdir (OUT OpString& dest, IN const OpStringC& subdir)
{
	return OpPathDirFileCombine(dest, dest, subdir);
}

OP_STATUS BrowserUtilsGlue::GetSelectedText(OUT OpString16& text) const
{
	text.Empty();
	MailDesktopWindow* mail_window = g_application->GetActiveMailDesktopWindow();
	Window* window = (mail_window && mail_window->GetMailView()) ? mail_window->GetMailView()->GetWindow() : NULL;
	DocumentManager* doc_manager = window ? window->DocManager() : NULL;
	FramesDocument* document = doc_manager ? doc_manager->GetCurrentDoc() : NULL;

	if (!document)
		return OpStatus::OK;

	return document->GetSelectedText(text);
}

OP_STATUS BrowserUtilsGlue::FilenamePartFromFullFilename(const OpStringC& full_filename, OpString &filename_part) const
{
	OP_STATUS status = OpStatus::OK;

	OpString name_part;
	OpString extension_part;

	OpString candidate;
	candidate.Set(full_filename);

	TRAP(status, SplitFilenameIntoComponentsL(candidate, 0, &name_part, &extension_part));
	RETURN_IF_ERROR(status);

	filename_part.TakeOver(name_part);
	RETURN_IF_ERROR(filename_part.Append(UNI_L(".")));
	return filename_part.Append(extension_part);
}

OP_STATUS BrowserUtilsGlue::GetContactName(const OpStringC &mail_address, OpString &full_name)
{
	OpString formatted_address;
	MailFormatting::FormatListIdToEmail(mail_address, formatted_address);

	HotlistManager::ItemData item_data;
	if( g_hotlist_manager )
		g_hotlist_manager->GetItemValue_ByEmail( formatted_address, item_data );
	if( !item_data.name.IsEmpty() )
	{
		return full_name.Set( item_data.name );
	}
	else
	{
		return full_name.Set(mail_address);
	}
}

OP_STATUS BrowserUtilsGlue::GetContactImage(const OpStringC &mail_address, const char*& image)
{
	OpString formatted_address;
	MailFormatting::FormatListIdToEmail(mail_address, formatted_address);

	HotlistManager::ItemData item_data;
	if( g_hotlist_manager )
		g_hotlist_manager->GetItemValue_ByEmail( formatted_address, item_data );
	if (item_data.direct_image_pointer)
	{
		image = item_data.direct_image_pointer;
	}
	return OpStatus::OK;
}

INT32 BrowserUtilsGlue::GetContactCount()
{
#if 0
	return g_hotlist_manager->GetContactsModel()->GetItemCount();
#else
	INT32 contact_count = 0;

	HotlistModel* contacts_model = g_hotlist_manager->GetContactsModel();
	if (contacts_model != 0)
	{
		HotlistModelItem* trash_item = contacts_model->GetTrashFolder();
		OP_ASSERT(trash_item != 0);

		for (INT32 index = 0, item_count = contacts_model->GetItemCount();
			index < item_count; ++index)
		{
			HotlistModelItem* item = contacts_model->GetItemByIndex(index);
			OP_ASSERT(item != 0);

			if ((!item->GetIsTrashFolder()) && (!item->IsChildOf(trash_item)))
				++contact_count;
		}
	}

	return contact_count;
#endif
}

OP_STATUS BrowserUtilsGlue::GetContactByIndex(INT32 index, INT32& id, OpString& nick)
{
#if 0
	HotlistModelItem* item = g_hotlist_manager->GetContactsModel()->GetItemByIndex(index);

	if (!item)
	{
		nick.Empty();
		return OpStatus::ERR;
	}

	nick.Set(item->GetShortName().CStr());
#else
	HotlistModel* contacts_model = g_hotlist_manager->GetContactsModel();
	if (contacts_model == 0)
		return OpStatus::ERR;

	HotlistModelItem* trash_item = contacts_model->GetTrashFolder();
	OP_ASSERT(trash_item != 0);

	INT32 real_index = 0;

	for (INT32 count = 0, item_count = contacts_model->GetItemCount();
		count < item_count; ++count)
	{
		HotlistModelItem* item = contacts_model->GetItemByIndex(count);
		OP_ASSERT(item != 0);

		if ((!item->GetIsTrashFolder()) && (!item->IsChildOf(trash_item)))
		{
			if (index == real_index)
			{
				id = item->GetID();
				RETURN_IF_ERROR(nick.Set(item->GetShortName()));
				break;
			}

			++real_index;
		}
	}
#endif
	return OpStatus::OK;
}

BOOL BrowserUtilsGlue::IsContact(const OpStringC &mail_address)
{
	OpString formatted_address;
	MailFormatting::FormatListIdToEmail(mail_address, formatted_address);

	return g_hotlist_manager && g_hotlist_manager->IsContact(formatted_address);
}

OP_STATUS BrowserUtilsGlue::AddContact(const OpStringC& mail_address, const OpStringC& full_name, BOOL is_mailing_list, int parent_folder_id)
{
	OpString notes;
	return AddContact(mail_address, full_name, notes, is_mailing_list, parent_folder_id);
}

OP_STATUS BrowserUtilsGlue::AddContact(const OpStringC& mail_address, const OpStringC& full_name, const OpStringC& notes, BOOL is_mailing_list, int parent_folder_id)
{
	//IMAA-addresses should not be present here, they should have been decoded to the unicode-version
	//(unless the user explicitly wants to add an IMAA-address, for some strange reason)
	OP_ASSERT(mail_address.Find("@xn--")==KNotFound);

	if (IsContact(mail_address))
	{
		return OpStatus::OK;
	}

	OpString formatted_address;
	MailFormatting::FormatListIdToEmail(mail_address, formatted_address);

	HotlistManager::ItemData item_data;
	item_data.name.Set( full_name );
	item_data.mail.Set( formatted_address );
	item_data.description.Set( notes );
	item_data.image.Set( is_mailing_list ? UNI_L("Contact37") : UNI_L("Contact0") );

	if( !is_mailing_list)
		g_hotlist_manager->SetupPictureUrl(item_data);

	g_hotlist_manager->NewContact( item_data, parent_folder_id, 0, FALSE );

	return OpStatus::OK;
}


OP_STATUS BrowserUtilsGlue::PlaySound(OpString& path)
{
	return SoundUtils::SoundIt(path, TRUE);
}


int BrowserUtilsGlue::NumberOfCachedHeaders()
{
	UINT32 megabytes = g_op_system_info->GetPhysicalMemorySizeMB();

	int prefered = megabytes * 10;

	return max(min(prefered, 40000), 100);
}

OP_STATUS BrowserUtilsGlue::ShowIndex(index_gid_t id, BOOL force_download)
{
    if (!g_application)
        return OpStatus::ERR_NULL_POINTER;

    g_application->GoToMailView(id, NULL, NULL, TRUE, FALSE, force_download);
    return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::SetFromUTF8(OpString* string, const char* aUTF8str, int aLength)
{
	return string->SetFromUTF8(aUTF8str, aLength);
}

extern int ReplaceEscapes(uni_char* txt, BOOL require_termination, BOOL remove_tabs, BOOL treat_nbsp_as_space);

OP_STATUS BrowserUtilsGlue::ReplaceEscapes(OpString& string)
{
	if (string.IsEmpty())
	{
		// ReplaceEscapes returns 0 and nothing to do.
		return OpStatus::OK;
	}

	int num = ::ReplaceEscapes(string.CStr(), TRUE, FALSE, TRUE);

	if (num == 0)
	{
		// OP_ASSERT(0);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::GetTransferItem(OpTransferItem** item,
	const OpStringC& url_string, BOOL* already_created)
{
	return ((TransferManager*)g_transferManager)->GetTransferItem(item, url_string.CStr(), already_created);
}

OP_STATUS BrowserUtilsGlue::StartLoading(URL* url)
{
	if (!url)
		return OpStatus::ERR_NULL_POINTER;

	MessageHandler *oldHandler = url->GetFirstMessageHandler();
	url->ChangeMessageHandler(oldHandler ? oldHandler : g_main_message_handler, g_main_message_handler);

	// if(url->Status(0) == URL_UNLOADED)	//hack to work around failed second download.
	{
		URL tmp;
		url->Reload(g_main_message_handler, tmp, FALSE, FALSE);
	}
	return OpStatus::OK;
}

OP_STATUS BrowserUtilsGlue::ReadDocumentData(URL& url, BrowserUtilsURLStream& url_stream)
{
	RETURN_IF_ERROR(url_stream.Init(url));
	return OpStatus::OK;
}

void BrowserUtilsGlue::DebugStatusText(const uni_char* message)
{
	if (g_application)
	{
		DesktopWindow* dw = g_application->GetActiveDesktopWindow();
		if (dw)
		{
			dw->SetStatusText(message);
		}
	}
}

#ifdef MSWIN
OP_STATUS BrowserUtilsGlue::OpRegReadStrValue(IN HKEY hKeyRoot, IN LPCTSTR szKeyName, IN LPCTSTR szValueName, OUT LPCTSTR szValue, IO DWORD *pdwSize)
{
	return (::OpRegReadStrValue(hKeyRoot, szKeyName, szValueName, szValue, pdwSize) == ERROR_SUCCESS ? OpStatus::OK : OpStatus::ERR);
}

LONG BrowserUtilsGlue::_RegEnumKeyEx(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcbClass, PFILETIME lpftLastWriteTime)
{
	return ::RegEnumKeyEx(hKey, dwIndex, lpName, lpcbName, lpReserved, lpClass, lpcbClass, lpftLastWriteTime);
}

LONG BrowserUtilsGlue::_RegOpenKeyEx(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	return ::RegOpenKeyEx(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LONG BrowserUtilsGlue::_RegQueryValueEx(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	return ::RegQueryValueEx(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LONG BrowserUtilsGlue::_RegCloseKey(HKEY hKey)
{
	return ::RegCloseKey(hKey);
}

DWORD BrowserUtilsGlue::_ExpandEnvironmentStrings(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize)
{
	return ::ExpandEnvironmentStrings(lpSrc, lpDst, nSize);
}
#endif //MSWIN

void BrowserUtilsGlue::SubscribeDialog(UINT16 account_id)
{
	AccountSubscriptionDialog* dialog = OP_NEW(AccountSubscriptionDialog, ());
	if (dialog)
		dialog->Init(account_id, NULL);
}

BOOL BrowserUtilsGlue::MatchRegExp(const OpStringC& regexp, const OpStringC& string)
{
#if defined(_ECMASCRIPT_SUPPORT_) && !defined(ECMASCRIPT_NO_EXPORT_REGEXP)
	{
		if (regexp.IsEmpty() || string.IsEmpty())
		{
			return FALSE;
		}

		// Special case: accept a single * as match anything
		if (regexp.Compare("*") == 0)
		{
			return TRUE;
		}

		int nmatches = 0;
		OpREMatchLoc *locs = NULL;

		OpREFlags flags;
		flags.multi_line = FALSE;
		flags.case_insensitive = TRUE;
		flags.ignore_whitespace = FALSE;

		OpRegExp *re;
		RETURN_VALUE_IF_ERROR(OpRegExp::CreateRegExp(&re, regexp.CStr(), &flags), FALSE);
		OpAutoPtr<OpRegExp> re_holder(re);
		RETURN_VALUE_IF_ERROR(re->Match(string.CStr(),&nmatches,&locs),  FALSE);

		// According to documentation for Match(), the client must delete the
		// returned matches. Thus this should help to avoid a memory leak.
		if (nmatches > 0)
			OP_DELETEA(locs);

		return (nmatches > 0);

		/* example code
		OpRegExp *re;
		OpREMatchLoc loc;
		int nmatches;
		OpREMatchLoc *locs = NULL;

		OpREFlags flags;
		flags.multi_line = FALSE;
		flags.case_insensitive = TRUE;
		flags.ignore_whitespace = FALSE;

		OP_STATUS res = OpRegExp::CreateRegExp(&re, UNI_L("a*(b*)c"), &flags);
		assert( res == OpStatus::OK );

		res = re->Match(UNI_L("xaaBBBBBBCCCCC"),&loc);
		assert( res == OpStatus::OK );
		assert( loc.matchloc == 1 );
		assert( loc.matchlen == 9 );
		res = re->Match(UNI_L("xaaBBBBBBCCCCC"),&nmatches, &locs);
		assert( res == OpStatus::OK );
		assert( nmatches == 2 );
		assert( locs != NULL );
		assert( locs[0].matchloc == 1 );
		assert( locs[0].matchlen == 9 );
		assert( locs[1].matchloc == 3 );
		assert( locs[1].matchlen == 6 );
		delete re;
		*/
	}
#else
	return (regexp.FindI(string) != KNotFound);
#endif // ECMASCRIPT_NO_EXPORT_REGEXP
}

// ----------------------------------------------------

void Decoder::DecodingStopped()
{
//#pragma PRAGMAMSG("FG: Signal all listeners that decoding is stopped")
}

void Decoder::SignalReDecodeMessage()
{
	// TODO: we might have to delete it better than this, for instance with a destroyURL
	if (m_decoder_url)
	{
		m_decoder_url->Unload();
		OP_DELETE(m_decoder_url);
	}
	m_decoder_url = NULL;
//#pragma PRAGMAMSG("FG: Signal all listeners that decoding will restart")
}

OP_STATUS Decoder::DecodeMessage(const char* headers, const char* body, UINT32 message_size, BOOL ignore_content, BOOL body_only, BOOL prefer_plaintext, BOOL ignore_warnings, UINT32 id, MessageListener* listener)
{
	UINT32 header_length = strlen(headers);
	UINT32 body_length = message_size - (header_length + 2);
	BrowserUtils* browser_glue = g_m2_engine->GetGlueFactory()->GetBrowserUtils();
	OP_STATUS ret = OpStatus::OK;
	BOOL found_body = FALSE;

	OP_ASSERT(browser_glue);
	OP_ASSERT(listener);
	if (!listener)
		return OpStatus::ERR_NULL_POINTER;

    // Create mime decoder
	if (!m_decoder_url)
	{
		OpString8 urlstring;

		RETURN_IF_ERROR(urlstring.AppendFormat("operaemail:/reply_%d/%.0f.html", id, g_op_time_info->GetRuntimeMS()));
		m_decoder_url = OP_NEW(URL,(urlManager->GetURL(urlstring.CStr())));
	}
	if (!m_decoder_url)
	{
		listener->OnFinishedDecodingMultiparts(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Set decoder properties
	RETURN_IF_ERROR(m_decoder_url->SetAttribute(URL::KCachePolicy_NoStore, TRUE));
	RETURN_IF_ERROR(m_decoder_url->SetAttribute(URL::KPreferPlaintext, prefer_plaintext));
	RETURN_IF_ERROR(m_decoder_url->SetAttribute(URL::KIgnoreWarnings, ignore_warnings));
	RETURN_IF_ERROR(m_decoder_url->SetAttribute(URL::KBigAttachmentIcons, FALSE));

	if (OpStatus::IsSuccess(ret))
	{
		//Write message to decoder
		if (headers && *headers)
			browser_glue->WriteToUrl(m_decoder_url, headers, header_length);
		browser_glue->WriteToUrl(m_decoder_url, "\r\n", 2);
		if (body && *body)
			browser_glue->WriteToUrl(m_decoder_url, body, body_length);
		m_decoder_url->WriteDocumentDataFinished();

		// Fetch parts
		URL suburl = URL();
		BOOL is_inlined_and_visible_element;
		for (int mimepart_index = 0; OpStatus::IsSuccess(ret) && ((is_inlined_and_visible_element = m_decoder_url->GetAttachment(mimepart_index, suburl)) != 0 || !suburl.IsEmpty()); suburl = URL(), mimepart_index++)
		{
			// for the attachment to be inlined and visible, it has to be a URL_CONTENT_ID element and GetAttachment must return TRUE
			is_inlined_and_visible_element &= suburl.GetAttribute(URL::KType) == URL_CONTENT_ID || g_mime_module.HasContentID(suburl);

			ret = DecodeSuburl(suburl, found_body, ignore_content, body_only, listener, is_inlined_and_visible_element);
		}
	}

	listener->OnFinishedDecodingMultiparts(ret); //Signal that we are done, possibly with errors

	return ret;
}

OP_STATUS Decoder::DecodeSuburl(URL& suburl, BOOL& found_body, BOOL ignore_content, BOOL body_only, MessageListener* listener, BOOL is_visible)
{
	URL* mimepart_url = NULL;
	URL_DataDescriptor* desc;
	URLContentType content_type;
	BOOL is_mailbody, takeover = FALSE;
	BYTE* mimepart_data = NULL;
	int mimepart_length = 0;
	OP_STATUS ret = OpStatus::OK;

	desc = suburl.GetDescriptor(NULL, TRUE, TRUE);
	mimepart_url = g_m2_engine->GetGlueFactory()->CreateURL();
	if (!mimepart_url)
	{
		OP_DELETE(desc);
		return OpStatus::ERR_NO_MEMORY;
	}
	if (desc)
		*mimepart_url = desc->GetURL();

	content_type = mimepart_url->ContentType();
	mimepart_url->GetAttribute(URL::KType);
	is_mailbody = (!found_body && (content_type==URL_TEXT_CONTENT || content_type==URL_HTML_CONTENT));

	if (desc && (!ignore_content || (body_only && is_mailbody)))
	{
		OpFileLength part_length = 0;
		suburl.GetAttribute(URL::KContentLoaded, &part_length, TRUE);

		mimepart_data = OP_NEWA(BYTE, (int)part_length + sizeof(uni_char)); // length of the attachment plus room for a terminator
		BOOL more = TRUE;
		const char *tmp_buffer;

		while (more)
		{
			int tmp_buffer_len = desc->RetrieveData(more);
			tmp_buffer = desc->GetBuffer();

			if (tmp_buffer_len>0)
			{
				UINT32 new_length = mimepart_length + tmp_buffer_len;

				if (new_length > part_length)
				{
					// Doesn't fit, grow buffer
					BYTE* new_mimepart_data = OP_NEWA(BYTE, new_length + sizeof(uni_char));
					if (!new_mimepart_data)
					{
						OP_DELETE(desc);
						OP_DELETEA(mimepart_data);
						g_m2_engine->GetGlueFactory()->DeleteURL(mimepart_url);
						return OpStatus::ERR_NO_MEMORY;
					}
					memcpy(new_mimepart_data, mimepart_data, mimepart_length);
					OP_DELETEA(mimepart_data);
					mimepart_data = new_mimepart_data;
					part_length = new_length;
				}

				// Copy fetched buffer
				memcpy(mimepart_data + mimepart_length, tmp_buffer, tmp_buffer_len);
				mimepart_length += tmp_buffer_len;
			}
			else if (desc->NeedRefresh())
			{
				OP_DELETE(desc);
				OP_DELETEA(mimepart_data);
				g_m2_engine->GetGlueFactory()->DeleteURL(mimepart_url);
				return DecodeSuburl(suburl, found_body, ignore_content, body_only, listener, is_visible);
			}

			desc->ConsumeData(tmp_buffer_len);
		}

		// terminate
		for (size_t i = 0; i < sizeof(uni_char); i++)
			mimepart_data[mimepart_length + i] = 0;
	}

	if (mimepart_length > 0 || ignore_content)
	{
		OpString suggested_filename;
		TRAP(ret, mimepart_url->GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));

		takeover = (OpStatus::IsSuccess(ret) && OpStatus::IsSuccess(ret = listener->OnDecodedMultipart(mimepart_url, suburl.GetAttribute(URL::KMIME_CharSet), suggested_filename, mimepart_length, mimepart_data, is_mailbody, is_visible)));
	}
	if (!takeover)
	{
		g_m2_engine->GetGlueFactory()->DeleteURL(mimepart_url);
		OP_DELETEA(mimepart_data);
	}

	OP_DELETE(desc);
	found_body |= is_mailbody;

	return ret;
}

// ----------------------------------------------------

class MimeUtilsGlue : public MimeUtils
{
public:
	// UTF8toUTF16Converter
#ifdef _DEBUG
	UTF8toUTF16Converter* CreateUTF8toUTF16ConverterDbg(const OpStringC8& source_file, int source_line);
#else
	UTF8toUTF16Converter* CreateUTF8toUTF16Converter();
#endif
	void DeleteUTF8toUTF16Converter(UTF8toUTF16Converter*);

	// UTF16toUTF8Converter
#ifdef _DEBUG
	UTF16toUTF8Converter* CreateUTF16toUTF8ConverterDbg(const OpStringC8& source_file, int source_line);
#else
	UTF16toUTF8Converter* CreateUTF16toUTF8Converter();
#endif
	void DeleteUTF16toUTF8Converter(UTF16toUTF8Converter*);

	// UTF7toUTF16Converter
#ifdef _DEBUG
	UTF7toUTF16Converter* CreateUTF7toUTF16ConverterDbg(UTF7toUTF16Converter::utf7_variant variant, const OpStringC8& source_file, int source_line);
#else
	UTF7toUTF16Converter* CreateUTF7toUTF16Converter(UTF7toUTF16Converter::utf7_variant variant);
#endif
	void DeleteUTF7toUTF16Converter(UTF7toUTF16Converter*);

	// UTF16toUTF7Converter
#ifdef _DEBUG
	UTF16toUTF7Converter* CreateUTF16toUTF7ConverterDbg(UTF16toUTF7Converter::utf7_variant variant, const OpStringC8& source_file, int source_line);
#else
	UTF16toUTF7Converter* CreateUTF16toUTF7Converter(UTF16toUTF7Converter::utf7_variant variant);
#endif
	void DeleteUTF16toUTF7Converter(UTF16toUTF7Converter*);

	OP_STATUS RemoveHeaderEscapes(const char* charset,
                                  const char *&src,
							      OpString16& dst);

#ifndef __SUPPORT_OPENPGP__
	Upload_Base*	CreateUploadElement(Upload_Element_Type typ);
#else
	Upload_Builder_Base*	CreateUploadBuilder();
	void			DeleteUploadBuilder(Upload_Builder_Base* builder) {OP_DELETE(builder);}
#endif
	void            DeleteUploadElement(Upload_Base* element) {OP_DELETE(element);}

	Decoder* CreateDecoder() { return OP_NEW(Decoder, ()); }
	void     DeleteDecoder(Decoder* decoder) { OP_DELETE(decoder); }
};

// ----------------------------------------------------

#ifdef _DEBUG
UTF8toUTF16Converter* MimeUtilsGlue::CreateUTF8toUTF16ConverterDbg(const OpStringC8& source_file, int source_line)
#else
UTF8toUTF16Converter* MimeUtilsGlue::CreateUTF8toUTF16Converter()
#endif
{
	UTF8toUTF16Converter* converter = OP_NEW(UTF8toUTF16Converter, ());
#ifdef _DEBUG
	if (converter)
		AddToReferencesList(DebugReferences::M2REF_UTF8TOUTF16CONVERTER, source_file, source_line, converter);
#endif
	return converter;
}

void MimeUtilsGlue::DeleteUTF8toUTF16Converter(UTF8toUTF16Converter* converter)
{
#ifdef _DEBUG
	if (converter)
		RemoveFromReferencesList(DebugReferences::M2REF_UTF8TOUTF16CONVERTER, converter);
#endif
	OP_DELETE(converter);
}

#ifdef _DEBUG
UTF16toUTF8Converter* MimeUtilsGlue::CreateUTF16toUTF8ConverterDbg(const OpStringC8& source_file, int source_line)
#else
UTF16toUTF8Converter* MimeUtilsGlue::CreateUTF16toUTF8Converter()
#endif
{
	UTF16toUTF8Converter* converter = OP_NEW(UTF16toUTF8Converter, ());
#ifdef _DEBUG
	if (converter)
		AddToReferencesList(DebugReferences::M2REF_UTF16TOUTF8CONVERTER, source_file, source_line, converter);
#endif
	return converter;
}

void MimeUtilsGlue::DeleteUTF16toUTF8Converter(UTF16toUTF8Converter* converter)
{
#ifdef _DEBUG
	if (converter)
		RemoveFromReferencesList(DebugReferences::M2REF_UTF16TOUTF8CONVERTER, converter);
#endif
	OP_DELETE(converter);
}

#ifdef _DEBUG
UTF7toUTF16Converter* MimeUtilsGlue::CreateUTF7toUTF16ConverterDbg(UTF7toUTF16Converter::utf7_variant variant, const OpStringC8& source_file, int source_line)
#else
UTF7toUTF16Converter* MimeUtilsGlue::CreateUTF7toUTF16Converter(UTF7toUTF16Converter::utf7_variant variant)
#endif
{
	UTF7toUTF16Converter* converter = OP_NEW(UTF7toUTF16Converter, (variant));
#ifdef _DEBUG
	if (converter)
		AddToReferencesList(DebugReferences::M2REF_UTF7TOUTF16CONVERTER, source_file, source_line, converter);
#endif
	return converter;
}

void MimeUtilsGlue::DeleteUTF7toUTF16Converter(UTF7toUTF16Converter* converter)
{
#ifdef _DEBUG
	if (converter)
		RemoveFromReferencesList(DebugReferences::M2REF_UTF7TOUTF16CONVERTER, converter);
#endif
	OP_DELETE(converter);
}

#ifdef _DEBUG
UTF16toUTF7Converter* MimeUtilsGlue::CreateUTF16toUTF7ConverterDbg(UTF16toUTF7Converter::utf7_variant variant, const OpStringC8& source_file, int source_line)
#else
UTF16toUTF7Converter* MimeUtilsGlue::CreateUTF16toUTF7Converter(UTF16toUTF7Converter::utf7_variant variant)
#endif
{
	UTF16toUTF7Converter* converter = OP_NEW(UTF16toUTF7Converter, (variant));
#ifdef _DEBUG
	if (converter)
		AddToReferencesList(DebugReferences::M2REF_UTF16TOUTF7CONVERTER, source_file, source_line, converter);
#endif
	return converter;
}

void MimeUtilsGlue::DeleteUTF16toUTF7Converter(UTF16toUTF7Converter* converter)
{
#ifdef _DEBUG
	if (converter)
		RemoveFromReferencesList(DebugReferences::M2REF_UTF16TOUTF7CONVERTER, converter);
#endif
	OP_DELETE(converter);
}

OP_STATUS MimeUtilsGlue::RemoveHeaderEscapes(const char* charset, const char *&source, OpString16& dest)
{
	::RemoveHeaderEscapes(dest, source, strlen(source), FALSE, TRUE, charset);
	return OpStatus::OK;
}

#ifndef __SUPPORT_OPENPGP__
Upload_Base* MimeUtilsGlue::CreateUploadElement(Upload_Element_Type typ)
{
	switch(typ)
	{
	case UPLOAD_BINARY_BUFFER:
		return OP_NEW(Upload_BinaryBuffer, ());
	case UPLOAD_STRING8:
		return OP_NEW(Upload_OpString8, ());
	case UPLOAD_MULTIPART:
		return OP_NEW(Upload_Multipart, ());
	case UPLOAD_URL:
		return OP_NEW(Upload_URL, ());
	case UPLOAD_TEMPORARY_URL:
		return OP_NEW(Upload_TemporaryURL, ());

#ifdef UPLOAD2_OPSTRING16_SUPPORT
	case UPLOAD_STRING16:
		return OP_NEW(Upload_OpString16, ());
#endif

#ifdef UPLOAD2_EXTERNAL_BUFFER_SUPPORT
	case UPLOAD_EXTERNAL_BUFFER:
		return OP_NEW(Upload_ExternalBuffer, ());
#endif

#ifdef UPLOAD2_ENCAPSULATED_SUPPORT
	case UPLOAD_ENCAPSULATED:
		return OP_NEW(Upload_EncapsulatedElement, ());
	case UPLOAD_EXTERNAL_BODY:
		return OP_NEW(Upload_External_Body, ());
#endif
	default:
		return NULL;
	}
}
#else // __SUPPORT_OPENPGP__
Upload_Builder_Base* MimeUtilsGlue::CreateUploadBuilder()
{
	Upload_Builder_Base* builder=NULL;

	TRAPD(op_err, builder = CreateUploadBuilderL());
	OpStatus::Ignore(op_err);

	return builder;
}
#endif

// ----------------------------------------------------

MailerUtilFactory::MailerUtilFactory()
: m_browser_utils_glue(NULL),
  m_mime_utils_glue(NULL)
{
}

MailerUtilFactory::~MailerUtilFactory()
{
    if (m_browser_utils_glue)
        OP_DELETE(m_browser_utils_glue);

    if (m_mime_utils_glue)
        OP_DELETE(m_mime_utils_glue);
}

// ----------------------------------------------------

#ifdef _DEBUG
ProtocolConnection* MailerUtilFactory::CreateCommGlueDbg(const OpStringC8& source_file, int source_line)
#else
ProtocolConnection* MailerUtilFactory::CreateCommGlue()
#endif
{
	ProtocolConnection* connection = OP_NEW(CommGlue, ());
#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_COMMGLUE, source_file, source_line, connection);
#endif
	return connection;
}

// ----------------------------------------------------

void MailerUtilFactory::DeleteCommGlue(ProtocolConnection* p)
{
	if (p)
		p->SetClient(NULL);

#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_COMMGLUE, p);
#endif
	CommGlue* tmp_p = static_cast<CommGlue*>(p);
	autodelete(tmp_p);
}

// ----------------------------------------------------

#ifdef _DEBUG
MessageLoop* MailerUtilFactory::CreateMessageLoopDbg(const OpStringC8& source_file, int source_line)
#else
MessageLoop* MailerUtilFactory::CreateMessageLoop()
#endif
{
	MessageLoop* mess_loop = OP_NEW(MessageLoopGlue, ());
#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_MESSAGELOOP, source_file, source_line, mess_loop);
#endif
	return mess_loop;
}

// ----------------------------------------------------

void MailerUtilFactory::DeleteMessageLoop(MessageLoop* loop)
{
#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_MESSAGELOOP, loop);
#endif
	delete (MessageLoopGlue*) loop;
}

// ----------------------------------------------------

#ifdef _DEBUG
PrefsFile* MailerUtilFactory::CreatePrefsFileDbg(const OpStringC& path, const OpStringC8& source_file, int source_line)
#else
PrefsFile* MailerUtilFactory::CreatePrefsFile(const OpStringC& path)
#endif
{
	OpFile file;
	RETURN_VALUE_IF_ERROR(file.Construct(path.CStr()), NULL);

	PrefsFile* OP_MEMORY_VAR prefs = OP_NEW(PrefsFile, (PREFS_STD));
    if (!prefs)
        return NULL;

	TRAPD(err, prefs->ConstructL(); 
			   prefs->SetFileL(&file));

	if (OpStatus::IsError(err))
    {
        OP_DELETE(prefs);
        return NULL;
    }

#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_PREFSFILE, source_file, source_line, prefs);
#endif
	return prefs;
}

// ----------------------------------------------------

void MailerUtilFactory::DeletePrefsFile(PrefsFile* file)
{
	if (!file)
		return;

#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_PREFSFILE, file);
#endif
	OP_DELETE(file);
}

// ----------------------------------------------------

#ifdef _DEBUG
PrefsSection* MailerUtilFactory::CreatePrefsSectionDbg(PrefsFile* prefsfile, const OpStringC& section, const OpStringC8& source_file, int source_line)
#else
PrefsSection* MailerUtilFactory::CreatePrefsSection(PrefsFile* prefsfile, const OpStringC& section)
#endif
{
    if (!prefsfile)
        return NULL;

    PrefsSection* prefs_section = NULL;
    TRAPD(err, prefs_section = prefsfile->ReadSectionL(section));
    if (OpStatus::IsError(err))
    {
        OP_DELETE(prefs_section);
        return NULL;
    }

#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_PREFSSECTION, source_file, source_line, prefs_section);
#endif
    return prefs_section;
}

// ----------------------------------------------------

void MailerUtilFactory::DeletePrefsSection(PrefsSection* section)
{
#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_PREFSSECTION, section);
#endif
	OP_DELETE(section);
}

// ----------------------------------------------------

#ifdef _DEBUG
OpFile* MailerUtilFactory::CreateOpFileDbg(const OpStringC& path, const OpStringC8& source_file, int source_line)
#else
OpFile* MailerUtilFactory::CreateOpFile(const OpStringC& path)
#endif
{
	OpFile* file = OP_NEW(OpFile, ());
    if (!file)
        return NULL;

	if (OpStatus::IsError(file->Construct(path.CStr(), OPFILE_SERIALIZED_FOLDER)))
    {
        OP_DELETE(file);
        return NULL;
    }

#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_OPFILE, source_file, source_line, file);
#endif
	return file;
}

// ----------------------------------------------------

void MailerUtilFactory::DeleteOpFile(OpFile* file)
{
	if (!file)
		return;

#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_OPFILE, file);
#endif
	OP_DELETE(file);
}

// ----------------------------------------------------

#ifdef _DEBUG
URL* MailerUtilFactory::CreateURLDbg(const OpStringC8& source_file, int source_line)
#else
URL* MailerUtilFactory::CreateURL()
#endif
{
	URL* url = OP_NEW(URL, ());

#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_URL, source_file, source_line, url);
#endif
    return url;
}

// ----------------------------------------------------

void MailerUtilFactory::DeleteURL(URL* url)
{
#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_URL, url);
#endif
	OP_DELETE(url);
}

// ----------------------------------------------------

#ifdef _DEBUG
UnicodeFileInputStream* MailerUtilFactory::CreateUnicodeFileInputStreamDbg(OpFile *inFile, UnicodeFileInputStream::LocalContentType content, const OpStringC8& source_file, int source_line)
#else
UnicodeFileInputStream* MailerUtilFactory::CreateUnicodeFileInputStream(OpFile *inFile, UnicodeFileInputStream::LocalContentType content)
#endif
{
    OP_ASSERT(inFile);
    if (!inFile)
        return NULL;

    UnicodeFileInputStream* file = OP_NEW(UnicodeFileInputStream, ());
    if (!file)
        return NULL;

   if (OpStatus::IsError(file->Construct(inFile, content)))
    {
        OP_DELETE(file);
        return NULL;
    }

#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_INPUTSTREAM, source_file, source_line, file);
#endif
	return file;
}

// ----------------------------------------------------

void MailerUtilFactory::DeleteUnicodeFileInputStream(UnicodeFileInputStream* stream)
{
#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_INPUTSTREAM, stream);
#endif
	OP_DELETE(stream);
}

// ----------------------------------------------------

#ifdef _DEBUG
UnicodeFileOutputStream* MailerUtilFactory::CreateUnicodeFileOutputStreamDbg(const uni_char* filename, const char* encoding, const OpStringC8& source_file, int source_line)
#else
UnicodeFileOutputStream* MailerUtilFactory::CreateUnicodeFileOutputStream(const uni_char* filename, const char* encoding)
#endif
{
    OP_ASSERT(filename && *filename);
    if (!filename || !*filename)
        return NULL;

    UnicodeFileOutputStream* file = OP_NEW(UnicodeFileOutputStream, ());
    if (!file)
        return NULL;

    if (OpStatus::IsError(file->Construct(filename, encoding)))
    {
        OP_DELETE(file);
        return NULL;
    }

#ifdef _DEBUG
	AddToReferencesList(DebugReferences::M2REF_OUTPUTSTREAM, source_file, source_line, file);
#endif
	return file;
}

// ----------------------------------------------------

void MailerUtilFactory::DeleteUnicodeFileOutputStream(UnicodeFileOutputStream* stream)
{
#ifdef _DEBUG
	RemoveFromReferencesList(DebugReferences::M2REF_OUTPUTSTREAM, stream);
#endif
	OP_DELETE(stream);
}

// ----------------------------------------------------

OpSocket* MailerUtilFactory::CreateSocket(OpSocketListener &listener)
{
	OpSocket* socket;
	OP_STATUS status = SocketWrapper::CreateTCPSocket(&socket, &listener, 0);
	if (OpStatus::IsSuccess(status))
	{
		return socket;
	}
	else
	{
		return NULL;
	}
}

void MailerUtilFactory::DestroySocket(OpSocket *socket)
{
	OP_DELETE(socket);
}

OpSocketAddress* MailerUtilFactory::CreateSocketAddress()
{
	OpSocketAddress* socket_address;
	OP_STATUS status = OpSocketAddress::Create(&socket_address);
	if (OpStatus::IsSuccess(status))
	{
		return socket_address;
	}
	else
	{
		return NULL;
	}
}

void MailerUtilFactory::DestroySocketAddress(OpSocketAddress* socket_address)
{
	OP_DELETE(socket_address);
}

OpHostResolver* MailerUtilFactory::CreateHostResolver(OpHostResolverListener &resolver_listener)
{
	OpHostResolver* host_resolver;
	OP_STATUS status = SocketWrapper::CreateHostResolver(&host_resolver, &resolver_listener);
	if (OpStatus::IsSuccess(status))
	{
		return host_resolver;
	}
	else
	{
		return NULL;
	}
}

void MailerUtilFactory::DestroyHostResolver(OpHostResolver* host_resolver)
{
	OP_DELETE(host_resolver);
}

// ----------------------------------------------------

BrowserUtils*
MailerUtilFactory::GetBrowserUtils()
{
    if (!m_browser_utils_glue)
    {
        m_browser_utils_glue = OP_NEW(BrowserUtilsGlue, ());
    }
    return m_browser_utils_glue;
}

MimeUtils*
MailerUtilFactory::GetMimeUtils()
{
    if (!m_mime_utils_glue)
    {
        m_mime_utils_glue = OP_NEW(MimeUtilsGlue, ());
    }
    return m_mime_utils_glue;
}

char*     MailerUtilFactory::OpNewChar(int size) {return OP_NEWA(char, size);}
uni_char* MailerUtilFactory::OpNewUnichar(int size) {return OP_NEWA(uni_char, size);}
void      MailerUtilFactory::OpDeleteArray(void* p) {delete [] static_cast<char *>(p);} //If this does not compile in gcc 3.3, please contact frodegill@opera.com


// ----------------------------------------------------

#endif // M2_SUPPORT

// ----------------------------------------------------
