// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef CHAT_NETWORKS_H
#define CHAT_NETWORKS_H

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

// ***************************************************************************
//
//	ChatNetworks
//
// ***************************************************************************

class ChatNetworkManager
{
public:
	// Construction / destruction.
	ChatNetworkManager() { }
	~ChatNetworkManager();

	OP_STATUS Init();

	// Enums.
	enum NetworkType { IRCNetworkType };

	// Methods.
	BOOL HasNetwork(NetworkType network_type, const OpStringC& network_name) const;
	OP_STATUS AddNetworkAndServers(NetworkType network_type, const OpStringC& input, OpString& network_name);
	OP_STATUS IRCNetworkNames(OpAutoVector<OpString>& network_names) const;

	OP_STATUS ServerList(NetworkType network_type, const OpStringC& network_name, OpString& server_list, INT32 filter_by_port = 0) const;
	OP_STATUS FirstServer(NetworkType network_type, const OpStringC& network_name, OpString& server) const;

private:
	// No copy or assignment.
	ChatNetworkManager(const ChatNetworkManager& other);
	ChatNetworkManager& operator=(const ChatNetworkManager& other);

	// Internal classes.
	class ChatServer
	{
	public:
		// Construction.
		ChatServer() { }
		OP_STATUS Init(const OpStringC& address, const OpStringC& ports);

		OP_STATUS AddPort(INT32 port);
		OP_STATUS AddPortRange(INT32 start_port, INT32 end_port);

		const OpStringC& Address() const { return m_address; }
		BOOL HasPort(INT32 port) const { return m_ports.Find(port) != -1; }

		OP_STATUS GetFirstPort(INT32& port) const;

	private:
		// No copy or assignment.
		ChatServer(const ChatServer& other);
		ChatServer& operator=(const ChatServer& other);

		// Members.
		OpString m_address;
		OpINT32Vector m_ports;
	};

	class ChatNetwork
	{
	public:
		// Construction.
		ChatNetwork() { }
		OP_STATUS Init(const OpStringC& name);

		// Static methods.
		static BOOL IsValidNetworkName(const OpStringC& name);

		// Methods.
		OP_STATUS AddServer(const OpStringC& server_name, const OpStringC& ports = UNI_L("6667"));
		OP_STATUS AddServerFromInput(const OpStringC& input, OpString& server_name);
		const OpStringC& Name() const { return m_name; }

		OP_STATUS ServerList(OpString& server_list, INT32 filter_by_port = 0) const;
		OP_STATUS FirstServer(OpString& server) const;
		INT32 ServerCount() const;

	private:
		// No copy or assignment.
		ChatNetwork(const ChatNetwork& other);
		ChatNetwork& operator=(const ChatNetwork& other);

		// Members.
		OpString m_name;
		OpAutoStringHashTable<ChatServer> m_servers;
	};

	// Methods.
	OP_STATUS AddNetwork(NetworkType network_type, ChatNetwork* network);
	const ChatNetwork* Network(NetworkType network_type, const OpStringC& network_name) const;

	// Members.
	OpAutoStringHashTable<ChatNetwork> m_irc_networks;
};

#endif
