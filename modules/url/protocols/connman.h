/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _CONNMAN_H_
#define _CONNMAN_H_

#include "modules/hardcore/mh/mh.h"

#include "modules/util/simset.h"
#include "modules/url/url_sn.h"

// Connection Manager

class ServerName;

class Connection_Element : public Link
{
	public:
		virtual ~Connection_Element(){if(InList()) Out();};
		virtual BOOL Idle(){return FALSE;};
		virtual BOOL AcceptNewRequests(){return FALSE;};
		virtual void Stop(){};
		virtual void RestartRequests() {};
		virtual BOOL HasId(unsigned int sid){return FALSE;};
		virtual BOOL SafeToDelete()=0;
		virtual void SetNoNewRequests()=0;
		virtual unsigned int Id(){return 0;};
		//void Out(){Link::Out();};
};


class Connection_Manager_Element : public Link, public MessageObject, public OpReferenceCounter
{
	protected:
		ServerName_Pointer servername;
		unsigned short port;
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		BOOL secure;
#endif
		Head requests;
		AutoDeleteHead connections;

	public:
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		Connection_Manager_Element(ServerName *name, unsigned short port_num, BOOL sec);
#else
		Connection_Manager_Element(ServerName *name, unsigned short port_num);
#endif
		virtual ~Connection_Manager_Element();

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		BOOL MatchServer(ServerName *name, unsigned short port_num, BOOL sec);
#else
		BOOL MatchServer(ServerName *name, unsigned short port_num);
#endif
		BOOL RemoveIdleConnection(BOOL force);
		//virtual void HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		unsigned short Port() const {return port;};
		ServerName *HostName(){return servername;};
		Connection_Element *GetConnection(int id);
		virtual void CloseAllConnections();
		virtual void RestartAllConnections();
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		BOOL IsSecure() const{return secure;};
#else
		BOOL IsSecure() const{return FALSE;};
#endif

		BOOL	ClearIdleConnections();
		virtual BOOL Preserve() = 0;
		friend class HTTP_Manager;
};

class Connection_Manager
{
	protected:
		Head connections;

	public:
		Connection_Manager(){};
		virtual ~Connection_Manager(){connections.Clear();};

		Connection_Manager_Element *FindServer(ServerName *name, unsigned short port_num
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
			, BOOL sec = FALSE
#endif
			);

		BOOL RemoveIdleConnection(BOOL force, ServerName *sname=NULL);
		void CloseAllConnections();
		void RestartAllConnections();

		void ClearIdleConnections();
};

#endif
