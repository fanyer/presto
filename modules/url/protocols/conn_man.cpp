/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_sn.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/connman.h"

#include "modules/olddebug/tstdump.h"

// conn_man.cpp

// Connection Manager

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
Connection_Manager_Element::Connection_Manager_Element(ServerName *name, unsigned short port_num, BOOL sec)
#else
Connection_Manager_Element::Connection_Manager_Element(ServerName *name, unsigned short port_num)
#endif
{
	servername = name;
	port = port_num;
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
	secure = sec;
#endif
}

Connection_Manager_Element::~Connection_Manager_Element()
{
	servername = NULL;

	// Deleted by their owners, or SComm cleanup
	requests.RemoveAll();
}

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
BOOL Connection_Manager_Element::MatchServer(ServerName *name, unsigned short port_num, BOOL sec)
#else
BOOL Connection_Manager_Element::MatchServer(ServerName *name, unsigned short port_num)
#endif
{
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
	if(!((!secure && !sec) || (secure && sec)))
		return FALSE;
#endif
	if(port != port_num)
		return FALSE;
	
	if(name == (ServerName*)servername)
		return TRUE;

	return FALSE;
}

BOOL Connection_Manager_Element::RemoveIdleConnection(BOOL force)
{
	Connection_Element *connection;

	if(!force && connections.Cardinal() ==1)
		return FALSE;

	connection = (Connection_Element *) connections.First();
	while(connection)
	{
		if((connection->Idle() || !connection->AcceptNewRequests()) && connection->SafeToDelete())
		{
#ifdef _DEBUG_CONN
		PrintfTofile("http1.txt","Deleting Idle connection %d [%lu]\n",connection->Id(), prefsManager->CurrentTime());
#endif
			if(connection->Id())
				g_main_message_handler->RemoveCallBacks(this, connection->Id());
			OP_DELETE(connection);
			return TRUE;
		}
		connection = (Connection_Element *) connection->Suc();
	}

	return FALSE;
}

void Connection_Manager_Element::CloseAllConnections()
{
	Connection_Element *connection, *current_connection;

	connection = (Connection_Element *) connections.First();
	while(connection)
	{
		current_connection = connection;
		connection = (Connection_Element *) connection->Suc();
		if(current_connection->Idle() && current_connection->SafeToDelete())
		{
#ifdef _DEBUG_CONN
		PrintfTofile("http1.txt","Deleting Idle connection %d [%lu]\n",connection->Id(), prefsManager->CurrentTime());
#endif
			OP_DELETE(current_connection);
		}

	}
}

void Connection_Manager_Element::RestartAllConnections()
{
	Connection_Element *connection = (Connection_Element *) connections.First();
	while(connection)
	{
		connection->RestartRequests();
		connection = (Connection_Element *) connection->Suc();
	}
}

Connection_Element *Connection_Manager_Element::GetConnection(int id)
{
	Connection_Element *connection;

	connection = (Connection_Element *) connections.First();
	while(connection)
	{
		if(connection->HasId(id))
			break;
		connection = (Connection_Element *) connection->Suc();
	}

	return connection;
}

/*
void Connection_Manager_Element::HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
}
*/

Connection_Manager_Element *Connection_Manager::FindServer(
				ServerName *name, unsigned short port_num
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
				, BOOL sec
#endif
				)
{
	Connection_Manager_Element *conn = (Connection_Manager_Element *) connections.First();

	while(conn)
	{
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)
		if(conn->MatchServer(name, port_num, sec)) break;
#else
		if(conn->MatchServer(name, port_num)) break;
#endif
		conn = (Connection_Manager_Element *) conn->Suc();
	}

	return conn;
}


BOOL Connection_Manager::RemoveIdleConnection(BOOL force, ServerName *sname)
{
#ifdef _DEBUG_CONN
		PrintfTofile("http1.txt","Trying to remove Idle connections [%lu]\n", prefsManager->CurrentTime());
#endif
	Connection_Manager_Element *conn = (Connection_Manager_Element *) connections.First();

	while(conn)
	{
		if((!sname || conn->HostName() == sname) && conn->RemoveIdleConnection(force)) 
			return TRUE;
		conn = (Connection_Manager_Element *) conn->Suc();
	}

	return FALSE;
}

void Connection_Manager::CloseAllConnections()
{
#ifdef _DEBUG_CONN
		PrintfTofile("http1.txt","Trying to remove Idle connections [%lu]\n", prefsManager->CurrentTime());
#endif
	Connection_Manager_Element *conn = (Connection_Manager_Element *) connections.First();

	while(conn)
	{
		conn->CloseAllConnections();
		conn = (Connection_Manager_Element *) conn->Suc();
	}
}

void Connection_Manager::RestartAllConnections()
{
	Connection_Manager_Element *conn = (Connection_Manager_Element *) connections.First();

	while(conn)
	{
		conn->RestartAllConnections();
		conn = (Connection_Manager_Element *) conn->Suc();
	}
}

void Connection_Manager::ClearIdleConnections()
{
	Connection_Manager_Element *conn;
	Connection_Manager_Element *next_conn = (Connection_Manager_Element *) connections.First();

	while(next_conn)
	{
		conn = next_conn;
		next_conn = (Connection_Manager_Element *) next_conn->Suc();

		if(conn->ClearIdleConnections() && !conn->Preserve())
		{
			conn->Out();
			OP_DELETE(conn);
		}
	}
}

BOOL Connection_Manager_Element ::ClearIdleConnections()
{
	Connection_Element *conn;
	Connection_Element *next_conn = (Connection_Element *) connections.First();

	if (Get_Reference_Count())
		return FALSE;

	while(next_conn)
	{
		conn = next_conn;
		next_conn = (Connection_Element *) next_conn->Suc();

		if(conn->Idle())
		{
			if(conn->Id())
				g_main_message_handler->RemoveCallBacks(this, conn->Id());
			conn->Out();
			OP_DELETE(conn);
		}
	}

	return (requests.Empty() && connections.Empty());
}
