// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "chat-networks.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

// ***************************************************************************
//
//	ChatNetworkManager
//
// ***************************************************************************

ChatNetworkManager::~ChatNetworkManager()
{
	// Do the deletion manually due to a bug in OpAuto*HashTable (can't auto
	// delete an object that in turn also has an OpAuto*HashTable member).
	while (TRUE)
	{
		StringHashIterator<ChatNetwork> it(m_irc_networks);
		if (!it)
			break;

		ChatNetwork* chat_network = it.GetData();
		OP_ASSERT(chat_network != 0);

		OpStatus::Ignore(m_irc_networks.Remove(chat_network->Name().CStr(), &chat_network));
		m_irc_networks.Delete(chat_network);
	}
}


OP_STATUS ChatNetworkManager::Init()
{
	OP_STATUS err = OpStatus::OK;
	OpString network_name;

	// OperaNet, Europe.
	g_languageManager->GetString(Str::D_IRC_SERVER_OPERANET_EUROPE, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.opera.com"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// Undernet, Europe.
	g_languageManager->GetString(Str::D_IRC_SERVER_UNDERNET_EUROPE, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("eu.undernet.org"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("graz2.at.eu.undernet.org"), UNI_L("6660-6670, 7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("graz.at.eu.undernet.org"), UNI_L("6660-6670,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("elsene.be.eu.undernet.org"), UNI_L("6667-6669, 7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("geneva.ch.eu.undernet.org"), UNI_L("6660-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("zagreb.hr.eu.undernet.org"), UNI_L("6666-6669,9999")));
			OpStatus::Ignore(network->AddServer(UNI_L("amsterdam.nl.eu.undernet.org"), UNI_L("6660-6668")));
			OpStatus::Ignore(network->AddServer(UNI_L("ede.nl.eu.undernet.org"), UNI_L("6666-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("oslo.no.eu.undernet.org"), UNI_L("6666-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("stockholm.se.eu.undernet.org"), UNI_L("6666-6669, 7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("surrey.uk.eu.undernet.org"), UNI_L("6666-6669,7000")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// Undernet, North America.
	g_languageManager->GetString(Str::D_IRC_SERVER_UNDERNET_NORTH_AMERICA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("us.undernet.org"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("mesa.az.us.undernet.org"), UNI_L("6660,6665-6667,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("losangeles.ca.us.undernet.org"), UNI_L("6660-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("miami.fl.us.undernet.org"), UNI_L("6666-6669,7000,8888")));
			OpStatus::Ignore(network->AddServer(UNI_L("fairfax.va.us.undernet.org"), UNI_L("6660,6665-6667,7000")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// IRCnet, Africa.
	// g_languageManager->GetString(Str::D_IRC_SERVER_IRCNET_AFRICA, network_name);
	// if (network_name.HasContent() && OpStatus::IsSuccess(err))
	// {
	// }

	// IRCnet, Asia.
	g_languageManager->GetString(Str::D_IRC_SERVER_IRCNET_ASIA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("ircnet.netvision.net.il"), UNI_L("6661-6668")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.tokyo.wide.ad.jp"), UNI_L("6668")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.seed.net.tw"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// IRCnet, Europe.
	g_languageManager->GetString(Str::D_IRC_SERVER_IRCNET_EUROPE, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("linz.irc.at"), UNI_L("6666-6668")));
			OpStatus::Ignore(network->AddServer(UNI_L("vienna.irc.at"), UNI_L("6666-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.skynet.be"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.datacomm.ch"), UNI_L("6664-6670")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.felk.cvut.cz"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ircnet.dk"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.cs.hut.fi"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ee.auth.gr"), UNI_L("6666-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.elte.hu"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.simnet.is"), UNI_L("6660-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ircnet.is"), UNI_L("6661-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.tin.it"), UNI_L("6665-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.apollo.lv"), UNI_L("6666-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.uunet.nl"), UNI_L("6660-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.xs4all.nl"), UNI_L("6660-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.snt.utwente.nl"), UNI_L("6660-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.sci.kun.nl"), UNI_L("6660-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ifi.uio.no"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.pvv.ntnu.no"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("lublin.irc.pl"), UNI_L("6666-6668")));
			OpStatus::Ignore(network->AddServer(UNI_L("warszawa.irc.pl"), UNI_L("6666-6668")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ru"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ludd.luth.se"), UNI_L("6661-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.swipnet.se"), UNI_L("5190,6660-6670,7002,8000")));
			OpStatus::Ignore(network->AddServer(UNI_L("ircnet.demon.co.uk"), UNI_L("6665-6669")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// EFnet, Asia.
	g_languageManager->GetString(Str::D_IRC_SERVER_EFNET_ASIA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.inter.net.il"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// EFnet, Europe.
	g_languageManager->GetString(Str::D_IRC_SERVER_EFNET_EUROPE, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.eu.efnet.info"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.dkom.at"), UNI_L("6667-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.inet.tele.dk"), UNI_L("6661-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("efnet.cs.hut.fi"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("efnet.xs4all.nl"), UNI_L("6661-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.efnet.nl"), UNI_L("6660-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.homelien.no"), UNI_L("6666,6667,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.daxnet.no"), UNI_L("6666-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.efnet.pl"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.efnet.se"), UNI_L("6665-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("efnet.demon.co.uk"), UNI_L("6665-6669")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// EFnet, North America.
	g_languageManager->GetString(Str::D_IRC_SERVER_EFNET_NORTH_AMERICA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.us.efnet.info"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.ca.efnet.info"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.easynews.com"), UNI_L("6660,6665-6667,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.limelight.us"), UNI_L("6665-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.blessed.net"), UNI_L("6665-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.he.net"), UNI_L("6665-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.prison.net"), UNI_L("6666-6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.wh.verio.net"), UNI_L("6665-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.foxlink.net"), UNI_L("6665-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.mindspring.com"), UNI_L("6660-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.servercentral.net"), UNI_L("6660-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.umich.edu"), UNI_L("6664-6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.umn.edu"), UNI_L("6665-6669")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.choopa.net"), UNI_L("6666-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.nac.net"), UNI_L("6665-6669,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.secsup.org"), UNI_L("6665-6669")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// DALnet, Asia.
	g_languageManager->GetString(Str::D_IRC_SERVER_DALNET_ASIA , network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.dal.net"), UNI_L("6660-6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("mesra.kl.my.dal.net"), UNI_L("6665-6668,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("hotspeed.sg.as.dal.net"), UNI_L("6665-6668,7000")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// DALnet, Europe.
	g_languageManager->GetString(Str::D_IRC_SERVER_DALNET_EUROPE , network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.dal.net"), UNI_L("6660-6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("powertech.no.eu.dal.net"), UNI_L("6665-6668,7000")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// DALnet, North America.
	g_languageManager->GetString(Str::D_IRC_SERVER_DALNET_NORTH_AMERICA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.dal.net"), UNI_L("6660-6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("dragons.ca.us.dal.net"), UNI_L("6665-6668,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("broadway.ny.us.dal.net"), UNI_L("6665-6668,7000")));
			OpStatus::Ignore(network->AddServer(UNI_L("jade.va.us.dal.net"), UNI_L("6665-6668,7000")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// FreeNode, Europe.
	g_languageManager->GetString(Str::D_IRC_SERVER_FREENODE_EUROPE, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.eu.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("saberhagen.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("kornbluth.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("sterling.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("adams.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("tolkien.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("calvino.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("leguin.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("burroughs.freenode.net"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// FreeNode, North America.
	g_languageManager->GetString(Str::D_IRC_SERVER_FREENODE_NORTH_AMERICA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.us.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("niven.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("zelazny.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("gibson.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("sendak.freenode.net"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// FreeNode, Oceania.
	g_languageManager->GetString(Str::D_IRC_SERVER_FREENODE_OCEANIA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.au.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("asimov.freenode.net"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	// FreeNode, South America.
	g_languageManager->GetString(Str::D_IRC_SERVER_FREENODE_SOUTH_AMERICA, network_name);
	if (network_name.HasContent() && OpStatus::IsSuccess(err))
	{
		ChatNetwork *network = OP_NEW(ChatNetwork, ());
		if (network)
		{
			OpStatus::Ignore(network->Init(network_name));
			OpStatus::Ignore(network->AddServer(UNI_L("irc.br.freenode.net"), UNI_L("6667")));
			OpStatus::Ignore(network->AddServer(UNI_L("carneiro.freenode.net"), UNI_L("6667")));

			OpStatus::Ignore(AddNetwork(IRCNetworkType, network));
		}
	}

	return OpStatus::OK;
}


BOOL ChatNetworkManager::HasNetwork(ChatNetworkManager::NetworkType network_type,
	const OpStringC& network_name) const
{
	BOOL has_network = FALSE;

	ChatNetworkManager* non_const_this = (ChatNetworkManager *)(this);
	OP_ASSERT(non_const_this != 0);

	switch (network_type)
	{

		case IRCNetworkType :
		{
			has_network = non_const_this->m_irc_networks.Contains(network_name.CStr());
			break;
		}
		default :
		{
			OP_ASSERT(0);
			break;
		}
	}

	return has_network;
}


OP_STATUS ChatNetworkManager::AddNetworkAndServers(NetworkType network_type,
	const OpStringC& input, OpString& network_name)
{
	// The accepted input format is: [Network name,] Server:Port[,Server:Port], etc.
	StringTokenizer tokenizer(input, UNI_L(","));

	if (!tokenizer.HasMoreTokens())
		return OpStatus::ERR;

	RETURN_IF_ERROR(tokenizer.NextToken(network_name));
	RETURN_IF_ERROR(StringUtils::Strip(network_name));

	ChatNetwork *netw = OP_NEW(ChatNetwork, ());
	if (!netw)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<ChatNetwork> new_network(netw);

	if (ChatNetwork::IsValidNetworkName(network_name))
		RETURN_IF_ERROR(netw->Init(network_name));
	else
	{
		OpString server_and_ports;
		server_and_ports.Set(network_name);
		OpString server_name;

		//try to initialize the network name with the first valid server name we find
		while (OpStatus::IsError(netw->AddServerFromInput(server_and_ports, server_name)))
		{
			if (!tokenizer.HasMoreTokens())
				return OpStatus::ERR;

			RETURN_IF_ERROR(tokenizer.NextToken(server_and_ports));
			RETURN_IF_ERROR(StringUtils::Strip(server_and_ports));
		}

		network_name.Set(server_name);
		netw->Init(server_name);
	}

	// Add the rest of the servers, if any.
	while (tokenizer.HasMoreTokens())
	{
		OpString server_and_ports;
		RETURN_IF_ERROR(tokenizer.NextToken(server_and_ports));
		RETURN_IF_ERROR(StringUtils::Strip(server_and_ports));

		OpString server_name;
		OpStatus::Ignore(netw->AddServerFromInput(server_and_ports, server_name));
	}

	if (netw->ServerCount() == 0)
	{
		// No servers? Perhaps the network name is a server.
		netw->AddServer(netw->Name());
	}

	RETURN_IF_ERROR(AddNetwork(network_type, netw));
	new_network.release();

	return OpStatus::OK;
}


OP_STATUS ChatNetworkManager::IRCNetworkNames(OpAutoVector<OpString>& network_names) const
{
	ChatNetworkManager* non_const_this = const_cast<ChatNetworkManager *>(this);

	for (StringHashIterator<ChatNetwork> it(non_const_this->m_irc_networks); it; it++)
	{
		ChatNetwork* chat_network = it.GetData();
		OP_ASSERT(chat_network != 0);

		OpString *name_copy = OP_NEW(OpString, ());
		if (!name_copy)
			return OpStatus::ERR_NO_MEMORY;

		OpStatus::Ignore(name_copy->Set(chat_network->Name()));
		OpStatus::Ignore(network_names.Add(name_copy));
	}

	return OpStatus::OK;
}


OP_STATUS ChatNetworkManager::ServerList(NetworkType network_type,
	const OpStringC& network_name, OpString& server_list,
	INT32 filter_by_port) const
{
	const ChatNetwork* network = Network(network_type, network_name);
	if (network == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(network->ServerList(server_list, filter_by_port));
	return OpStatus::OK;
}

OP_STATUS ChatNetworkManager::FirstServer(NetworkType network_type, const OpStringC& network_name, OpString& server) const
{
	const ChatNetwork* network = Network(network_type, network_name);
	if (network == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(network->FirstServer(server));
	return OpStatus::OK;
}


OP_STATUS ChatNetworkManager::AddNetwork(NetworkType network_type,
	ChatNetwork* network)
{
	if (network == 0)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(!HasNetwork(network_type, network->Name()));
	switch (network_type)
	{
		case IRCNetworkType :
		{
			RETURN_IF_ERROR(m_irc_networks.Add(network->Name().CStr(), network));
			break;
		}
		default :
		{
			OP_ASSERT(0);
			break;
		}
	}

	return OpStatus::OK;
}


const ChatNetworkManager::ChatNetwork* ChatNetworkManager::Network(NetworkType network_type,
	const OpStringC& network_name) const
{
	ChatNetworkManager* non_const_this = (ChatNetworkManager *)(this);
	OP_ASSERT(non_const_this != 0);

	ChatNetwork* network = 0;

	switch (network_type)
	{
		case IRCNetworkType :
		{
			OpStatus::Ignore(non_const_this->m_irc_networks.GetData(network_name.CStr(), &network));
			break;
		}
		default :
		{
			OP_ASSERT(0);
			break;
		}
	}

	return network;
}


// ***************************************************************************
//
//	ChatNetworkManager::ChatServer
//
// ***************************************************************************

OP_STATUS ChatNetworkManager::ChatServer::Init(const OpStringC& address,
	const OpStringC& ports)
{
	RETURN_IF_ERROR(m_address.Set(address));

	// Parse the ports we were passed. The format supported is a comma
	// separated list of servers, with - allow for giving port ranges.
	StringTokenizer tokenizer(ports, UNI_L(","));
	while (tokenizer.HasMoreTokens())
	{
		OpString token;
		RETURN_IF_ERROR(tokenizer.NextToken(token));
		RETURN_IF_ERROR(StringUtils::Strip(token));

		if (token.IsEmpty())
			continue;

		// Does this token contain any '-'? If so, it's a port range.
		const int delimiter_pos = token.Find("-");
		if (delimiter_pos != KNotFound)
		{
			OpString left_string;
			OpString right_string;

			OpStatus::Ignore(left_string.Set(token.CStr(), delimiter_pos));
			OpStatus::Ignore(right_string.Set(token.CStr() + delimiter_pos + 1));

			OpStatus::Ignore(StringUtils::Strip(left_string));
			OpStatus::Ignore(StringUtils::Strip(right_string));

			const INT32 start_port = StringUtils::NumberFromString(left_string);
			const INT32 end_port = StringUtils::NumberFromString(right_string);

			if (start_port != 0 && end_port != 0)
				OpStatus::Ignore(AddPortRange(start_port, end_port));
		}
		else
		{
			const INT32 port = StringUtils::NumberFromString(token);
			if (port != 0)
				OpStatus::Ignore(AddPort(port));
		}
	}

	return OpStatus::OK;
}


OP_STATUS ChatNetworkManager::ChatServer::AddPort(INT32 port)
{
	RETURN_IF_ERROR(m_ports.Add(port));
	return OpStatus::OK;
}


OP_STATUS ChatNetworkManager::ChatServer::AddPortRange(INT32 start_port,
	INT32 end_port)
{
	OP_ASSERT(start_port <= end_port);
	INT32 current_port = start_port;

	while (current_port <= end_port)
	{
		RETURN_IF_ERROR(AddPort(current_port));
		++current_port;
	}

	return OpStatus::OK;
}

OP_STATUS ChatNetworkManager::ChatServer::GetFirstPort(INT32& port) const
{
	if (m_ports.GetCount() == 0)
		return OpStatus::ERR;
	port = m_ports.Get(0);
	return OpStatus::OK;
}


// ***************************************************************************
//
//	ChatNetworkManager::ChatNetwork
//
// ***************************************************************************

OP_STATUS ChatNetworkManager::ChatNetwork::Init(const OpStringC& name)
{
	OP_ASSERT(IsValidNetworkName(name));

	RETURN_IF_ERROR(m_name.Set(name));
	return OpStatus::OK;
}


BOOL ChatNetworkManager::ChatNetwork::IsValidNetworkName(const OpStringC& name)
{
	BOOL is_valid = TRUE;

	if (name.Find(":") != KNotFound)
		is_valid = FALSE;

	return is_valid;
}


OP_STATUS ChatNetworkManager::ChatNetwork::AddServer(const OpStringC& server_name,
	const OpStringC& ports)
{
	ChatServer *server = OP_NEW(ChatServer, ());
	if (!server)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<ChatServer> new_server(server);

	RETURN_IF_ERROR(server->Init(server_name, ports));
	RETURN_IF_ERROR(m_servers.Add(server->Address().CStr(), server));

	new_server.release();
	return OpStatus::OK;
}


OP_STATUS ChatNetworkManager::ChatNetwork::AddServerFromInput(const OpStringC& input,
	OpString& server_name)
{
	// The input can be on the format server:ports. Ports is optional.
	StringTokenizer tokenizer(input, UNI_L(":"));
	OpString ports;

	RETURN_IF_ERROR(tokenizer.NextToken(server_name));
	OpStatus::Ignore(tokenizer.NextToken(ports));

	RETURN_IF_ERROR(AddServer(server_name, ports.HasContent() ? ports.CStr() : UNI_L("6667")));
	return OpStatus::OK;
}


INT32 ChatNetworkManager::ChatNetwork::ServerCount() const
{
	ChatNetwork* non_const_this = (ChatNetwork *)(this);
	OP_ASSERT(non_const_this != 0);

	return non_const_this->m_servers.GetCount();
}


OP_STATUS ChatNetworkManager::ChatNetwork::ServerList(OpString& server_list,
	INT32 filter_by_port) const
{
	// For now; return a comma separated list without the port numbers, and
	// only those servers that will accept connections on the port given.
	ChatNetwork* non_const_this = const_cast<ChatNetwork *>(this);

	for (StringHashIterator<ChatServer> it(non_const_this->m_servers); it; it++)
	{
		ChatServer* chat_server = it.GetData();
		OP_ASSERT(chat_server != 0);

		if (filter_by_port == 0 || chat_server->HasPort(filter_by_port))
		{
			RETURN_IF_ERROR(server_list.Append(chat_server->Address()));
			RETURN_IF_ERROR(server_list.Append(", "));
		}
	}

	RETURN_IF_ERROR(StringUtils::Strip(server_list, UNI_L(", ")));
	return OpStatus::OK;
}

OP_STATUS ChatNetworkManager::ChatNetwork::FirstServer(OpString& server) const
{
	ChatNetwork* non_const_this = const_cast<ChatNetwork *>(this);
	BOOL found = FALSE;

	for (StringHashIterator<ChatServer> it(non_const_this->m_servers); !found && it; it++)
	{
		ChatServer* chat_server = it.GetData();
		OP_ASSERT(chat_server != 0);

		INT32 port;

		found = OpStatus::IsSuccess(chat_server->GetFirstPort(port));

		if (found)
		{
			RETURN_IF_ERROR(server.Set(chat_server->Address()));
			RETURN_IF_ERROR(server.AppendFormat(UNI_L(":%d"), port));
		}
	}

	if (!found)
		return OpStatus::ERR;
	else
		return OpStatus::OK;


}

#endif // IRC_SUPPORT
