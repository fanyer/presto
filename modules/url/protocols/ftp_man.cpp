/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"
#ifndef NO_FTP_SUPPORT

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/url/url2.h"
#include "modules/util/handy.h"

#include "modules/olddebug/timing.h"

#include "modules/url/url_sn.h"
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
#include "modules/url/url_tools.h"
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/ftp.h"

// ftp_man.cpp

// FTP connection manager

FTP_Server_Manager *FTP_Manager::FindServer(
		ServerName *name,
		unsigned short port_num,
		BOOL create
#ifdef _SSL_SUPPORT_
		//, BOOL sec
#endif
		)
{
	FTP_Server_Manager *item;
	if(!port_num)
		port_num = 21;

	item = (FTP_Server_Manager *) Connection_Manager::FindServer(name,port_num
#ifdef _SSL_SUPPORT_
		//, sec
#endif
		);

	if(item== NULL && create)
	{
		item =  OP_NEW(FTP_Server_Manager, (name,port_num));
		if(item == NULL)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return NULL;
		}
		else
			item->Into(&connections);
	}

	return item;
}


FTP_Server_Manager::FTP_Server_Manager(ServerName *name, unsigned short port_num
#ifdef _SSL_SUPPORT_
										 // , BOOL sec
#endif
										 )
	: Connection_Manager_Element(name, port_num
#ifdef _SSL_SUPPORT_
	, FALSE //, sec
#endif
	)
{
	disable_size = FALSE;
	disable_mode = FALSE;
	disable_mdtm = FALSE;
	disable_full_path = FALSE;
	disable_cwd_when_size= FALSE;
	disable_epsv = FALSE;
	tested_epsv = FALSE;
	restart_tested = FALSE;
	restart_supported = TRUE;
	use_tilde_when_cwd_home = FALSE;
}

FTP_Server_Manager::~FTP_Server_Manager()
{
	// Deleted by their owners, or SComm cleanup
	FTP_Request *req;
	while ((req = (FTP_Request *)requests.First())!= NULL)
	{
		req->master = NULL;
		req->Out();
	}
	g_main_message_handler->UnsetCallBacks(this);
}

CommState FTP_Server_Manager::AddRequest(FTP_Request *req)
{
	if(req == NULL)
		return COMM_REQUEST_FAILED;

	if(req->InList())
		req->Out();
	req->Into(&requests);
	req->SetProgressInformation(REQUEST_QUEUED, 0, servername->UniName());

	FTP_Connection *conn = NULL, *conn2;

	for (conn2 = (FTP_Connection *) connections.First(); conn2; )
	{
		FTP_Connection *conn1 = conn2;
		conn2 = (FTP_Connection *) conn1->Suc();

		if(!conn1->conn)
		{
			OP_DELETE(conn1);
			continue;
		}

#ifdef URL_DISABLE_INTERACTION
		if(((req->GetUserInteractionBlocked() && ! conn1->conn->GetUserInteractionBlocked()) ||
			 (!req->GetUserInteractionBlocked() && conn1->conn->GetUserInteractionBlocked())) )
		{
			continue;
		}
#endif

		if(conn1->conn->Closed() || !conn1->conn->AcceptNewRequests())
		{
			g_main_message_handler->RemoveCallBacks(this, conn1->Id());
			OP_DELETE(conn1);
		}
		else if (conn1->Idle())
		{
			// Anonymous can go to any connection
			// specific user names must match that of the connection
			if(req->username.IsEmpty() || conn1->user_name.Compare(req->username) == 0)
			{
				conn = conn1;
				break;
			}
		}
	}

	OP_ASSERT(!conn || (conn->conn && conn->conn->Idle() && conn->conn->AcceptNewRequests() && !conn->conn->Closed()));

	BOOL new_comm = FALSE;
	unsigned short max_conn = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer);

	if(max_conn > 1 && !disable_epsv)
		max_conn >>= 1; // divide by two if EPSV is or may be supported.

	if (conn == NULL && connections.Cardinal() < max_conn)
	{
		if(servername->GetConnectCount() >= (unsigned int) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer) )
		{
			if(!ftp_Manager->RemoveIdleConnection(TRUE, servername))
				http_Manager->RemoveIdleConnection(TRUE, servername);

		}
		FTP *ftp_conn;
		SComm *comm;
		Comm *comm1;

		comm1 = Comm::Create(g_main_message_handler, servername, port);
		comm = comm1;

		if (comm == NULL)
			return COMM_REQUEST_FAILED;

		comm1->Set_NetType(req->Get_NetType());

		/*
#ifdef _SSL_SUPPORT_
		if(secure)
		{
			ProtocolComm *ssl_comm;

			ssl_comm = Generate_SSL(g_main_message_handler,  servername, port, FALSE, FALSE; TRUE);
			if(ssl_comm == NULL)
			{
				delete comm;
				return COMM_REQUEST_FAILED;
			}
			ssl_comm->SetNewSink(comm);
			comm = ssl_comm;
		}
#endif
		*/
  		ftp_conn = OP_NEW(FTP, (g_main_message_handler));

		if (ftp_conn == NULL)
		{
			SComm::SafeDestruction(comm);
			return COMM_REQUEST_FAILED;
		}

		ftp_conn->SetNewSink(comm);

		ftp_conn->SetManager(this);
		//ftp_conn->SetNewRequest(req, FALSE);

		conn = OP_NEW(FTP_Connection, (ftp_conn));
		if(conn == NULL)
		{
			SComm::SafeDestruction(ftp_conn);
			return COMM_REQUEST_FAILED;
		}

#ifdef URL_DISABLE_INTERACTION
		ftp_conn->SetUserInteractionBlocked(req->GetUserInteractionBlocked());
#endif

		if(OpStatus::IsError(conn->user_name.Set(req->username)) ||
			OpStatus::IsError(ftp_conn->SetCallbacks(this,this)))
		{
			OP_DELETE(conn);
			return COMM_REQUEST_FAILED;
		}

		new_comm = TRUE;
	}

	if(!conn)
		return COMM_LOADING;

	if(conn->InList())
		conn->Out();
	conn->Into(&connections);

	if(!conn->conn->SetNewRequest(req, !new_comm))
		return COMM_REQUEST_FAILED;

	if(!new_comm)
		conn->conn->SetProgressInformation(ACTIVITY_CONNECTION , 0, NULL);

	return new_comm ? conn->conn->Load() : COMM_LOADING;
}

void FTP_Server_Manager::RemoveRequest(FTP_Request *req)
{
	if(req == NULL)
		return;
	if(req->InList())
		req->Out();
	if(req->ftp_conn)
	{
		FTP *ftp = req->ftp_conn;		// Yngve: ftp (req->ftp_conn) pointed to an invalid address here! - I clicked 'STOP loading' and I had a crash here.
		req->ftp_conn = NULL;

		ftp->EndLoading();				// # of crashes so far: 2 (last time: 07/01-01)
		{
			FTP_Connection *conn = (FTP_Connection *) GetConnection(ftp->Id());

			if(conn)
			{
				g_main_message_handler->RemoveCallBacks(this, conn->Id());
				OP_DELETE(conn);
			}
		}
	}

	//req->Stop();
}

FTP_Request *FTP_Server_Manager::GetNewRequest(FTP *conn)
{
	if( conn == NULL)
		return NULL;

	FTP_Request *req= (FTP_Request *) requests.First();
	FTP_Request *first_avail = NULL;
	while(req)
	{
		if(!req->used && !req->ftp_conn)
		{
			if(conn->MatchCWD(req->path))
				break;
			if(!first_avail)
				first_avail = req;
		}
		req = (FTP_Request *) req->Suc();
	}
	if(!req)
		req = first_avail;

	return req;
}

void FTP_Server_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
									__DO_START_TIMING(TIMING_COMM_LOAD);
	switch (msg)
	{
		case MSG_COMM_LOADING_FAILED :
		case MSG_COMM_LOADING_FINISHED :
			{
				g_main_message_handler->RemoveCallBacks(this, par1);
				FTP_Connection *conn = (FTP_Connection *) GetConnection(par1);
				if( conn)
				{
					g_main_message_handler->RemoveCallBacks(this, conn->Id());
					OP_DELETE(conn);
				}
				if(connections.Empty())
				{
					FTP_Request *req= (FTP_Request *) requests.First();
					while(req && (req->used || req->ftp_conn))
					{
						req = (FTP_Request *) req->Suc();
					}
					if(req)
						req->Load();
				}
			}
			break;
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD);
}

FTP_Connection::~FTP_Connection()
{
	if(InList())
		Out();
	if(conn)
	{
		//conn->EndLoading();
		conn->Stop();
		SComm::SafeDestruction(conn);
		conn = NULL;
	}
}

BOOL FTP_Connection::Idle()
{
	return conn->Idle();
}

BOOL FTP_Connection::AcceptNewRequests()
{
	return conn->AcceptNewRequests();
}

unsigned int FTP_Connection::Id()
{
	return conn ? conn->Id() : 0;
}

BOOL FTP_Connection::HasId(unsigned int sid)
{
	return (conn && conn->HasId(sid));
}

void FTP_Connection::SetNoNewRequests()
{
	// not necessary to do anything, the determine proxy routine takes care of this.
}

BOOL FTP_Connection::SafeToDelete()
{
	return !conn || conn->SafeToDelete();
}

BOOL FTP::SafeToDelete() const
{
	return ftp_request == NULL && ProtocolComm::SafeToDelete();
}

FTP_Request::FTP_Request(MessageHandler* msg_handler)
		 : ProtocolComm(msg_handler, NULL, NULL)
{
	InternalInit();
}

void FTP_Request::InternalInit()
{
	ftp_conn = NULL;
	master = NULL;
	used = FALSE;

	typecode = 0;

#ifdef FTP_RESUME
	use_resume = FALSE;
	resume_pos = 0;
	using_resume = FALSE;
#endif
#ifdef _ENABLE_AUTHENTICATE
	auth_id = 0;
#endif
	file_size = 0;
}

OP_STATUS FTP_Request::Construct(
			ServerName *server,
			unsigned short port,

			const OpStringC8 &pth,
			const OpStringC8 &usr_name,
			OpStringS8 &passw,
			int r_typecode
			)
{
	master = ftp_Manager->FindServer(server, port, TRUE);
	if(master == NULL)
		return OpStatus::ERR;
	RETURN_IF_ERROR(path.Set(pth));
	RETURN_IF_ERROR(username.Set(usr_name));
	password.OpStringS8::TakeOver(passw);

	typecode = r_typecode;
	return OpStatus::OK;
}

FTP_Request::~FTP_Request()
{
	InternalDestruct();
}

void FTP_Request::InternalDestruct()
{
	if(InList())
		Out();
	if(ftp_conn)
	{
		ftp_conn->Stop();
	}
	password.Wipe();
}

CommState	FTP_Request::Load()
{
	file_size = 0;

	return master->AddRequest(this);
}

void FTP_Request::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
									__DO_START_TIMING(TIMING_COMM_LOAD);

	switch(msg)
	{
		case MSG_COMM_LOADING_FAILED :
			if(master != NULL)
			{
				master->RemoveRequest(this);
				master = NULL;
			}
			// fall-through

		case MSG_FTP_HEADER_LOADED :
		case MSG_COMM_DATA_READY :
			mh->PostMessage(msg, Id(),par2);
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD);
}

void FTP_Request::SetUserNameAndPassword(const OpStringC8 &usr_name, OpStringS8 &passwd)
{
	RAISE_IF_ERROR(username.Set(usr_name));
	password.OpStringS8::TakeOver(passwd);
}

OP_STATUS FTP_Request::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	static const OpMessage messages[] =
    {
        MSG_COMM_DATA_READY,
        MSG_COMM_LOADING_FINISHED,
		MSG_FTP_HEADER_LOADED,
        MSG_COMM_LOADING_FAILED
    };

    RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));

	return ProtocolComm::SetCallbacks(master, this);
}

void FTP_Request::GetDirMsgCopy(OpString8 &message)
{
	message.TakeOver(dir_msg);
}

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void FTP_Request::PauseLoading()
{
	if (ftp_conn)
		ftp_conn->PauseLoading();
	else
		master->RemoveRequest(this);
}

OP_STATUS FTP_Request::ContinueLoading()
{
	if (ftp_conn)
		return ftp_conn->ContinueLoading();
	else
	if (master->AddRequest(this) == COMM_REQUEST_FAILED)
	{
		EndLoading();
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		return OpStatus::ERR;
	}

	return OpStatus::OK;

}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#endif // NO_FTP_SUPPORT
