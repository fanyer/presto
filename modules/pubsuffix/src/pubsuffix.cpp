/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined PUBSUFFIX_ENABLED
#include "modules/olddebug/tstdump.h"

#include "modules/pubsuffix/src/pubsuffix.h"

#include "modules/rootstore/auto_update_versions.h"
#include "modules/pubsuffix/pubsuf_auto_update_versions.h"

class SuffixList_item : public Link
{
public:
	OpString name;
	BOOL registry;
	BOOL all_levels_are_TLDs;
	BOOL all;
	unsigned int levels;

	AutoDeleteHead suffix_list;

	SuffixList_item(const uni_char *str, BOOL a_registry):registry(a_registry), all_levels_are_TLDs(FALSE), all(FALSE), levels(0) { name.Set(str); }
	virtual ~SuffixList_item(){ if (InList()) Out(); }
};

#define PUBSUFFIX_VERSION "02"

PublicSuffix_Updater::PublicSuffix_Updater(ServerName *tld, ServerName *checking_domain)
: domain_sn(checking_domain), suffix(tld), all_levels_are_TLDs(FALSE), levels(0)
{
}

PublicSuffix_Updater::~PublicSuffix_Updater()
{
}

OP_STATUS PublicSuffix_Updater::Construct(OpMessage fin_msg)
{
	OpString8 update_string;
	URL url;

	if (suffix == (ServerName *) NULL || domain_sn == (ServerName *) NULL)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(update_string.SetConcat(AUTOUPDATE_SCHEME "://" AUTOUPDATE_SERVER  "/domains/" PUB_SUFFIX_AUTOUPDATE_VERSION "/", suffix->GetName(), ".xml"));

	return Construct(update_string, fin_msg);
}

OP_STATUS PublicSuffix_Updater::Construct(const OpStringC8 &fixed_url, OpMessage fin_msg)
{
	if (suffix == (ServerName *) NULL || domain_sn == (ServerName *) NULL || fixed_url.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	return OpStatus::OK;
}

struct ServerName_item : public Link
{
	ServerName *name;

	ServerName_item(ServerName *nm):name(nm){}
	virtual ~ServerName_item(){name=NULL; if (InList()) Out();}
};

OP_STATUS PublicSuffix_Updater::ParseContent(XMLFragment &parser, AutoDeleteHead &suffix_list)
{
	while (parser.EnterAnyElement())
	{
		do{
			OpStringC name(parser.GetAttribute(UNI_L("name")));

			SuffixList_item *new_item = OP_NEW(SuffixList_item, (name, parser.GetElementName() == UNI_L("registry")));
			if (new_item == NULL)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return OpStatus::ERR_NO_MEMORY;
			}

			OpStringC attr1(parser.GetAttribute(UNI_L("levels")));
			if (attr1.HasContent())
			{
				if (attr1.CompareI("all") ==0)
					new_item->all_levels_are_TLDs = TRUE;
				else
					new_item->levels = (unsigned) uni_atoi(attr1.CStr());
			}

			OpStringC attr2(parser.GetAttribute(UNI_L("all")));
			if (attr2.HasContent() && attr2.Compare("true") ==0)
				new_item->all = TRUE;

			new_item->Into(&suffix_list);

			ParseContent(parser, new_item->suffix_list);

		}while (0);
		parser.LeaveElement();
	}

	return OpStatus::OK;
}

OP_STATUS PublicSuffix_Updater::ParseFile(XMLFragment &parser)
{
#ifdef PUBSUF_DEBUG
	PrintfTofile("pubsuff.txt", "proc spec %s\n", suffix->Name());
#endif

	all_levels_are_TLDs = FALSE;
	levels = 0;
	name.Set(parser.GetAttribute(UNI_L("name")));

	OpStringC attr1(parser.GetAttribute(UNI_L("levels")));
	if (attr1.HasContent())
	{
		if (attr1.CompareI("all") ==0)
			all_levels_are_TLDs = TRUE;
		else
			levels = (unsigned) uni_atoi(attr1.CStr())+1;
	}

	ServerName *tldname = g_url_api->GetServerName(suffix->GetName());
	if (tldname == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}

	tldname->SetDomainType(ServerName::DOMAIN_TLD);

	return ParseContent(parser, suffix_list);
}


OP_STATUS PublicSuffix_Updater::ProcessFile()
{
#ifdef PUBSUF_DEBUG
	PrintfTofile("pubsuff.txt", "proc spec %s\n", suffix->Name());
#endif
	AutoDeleteHead servername_list;

	ServerName *tldname = g_url_api->GetServerName(suffix->GetName());
	if (tldname == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}

	tldname->SetDomainType(ServerName::DOMAIN_TLD);

	ServerName *name = g_url_api->GetFirstServerName();
	
	while (name)
	{
		ServerName *this_tld = name, *current_parent = NULL;

		while ((current_parent = this_tld->GetParentDomain()) != NULL)
			this_tld = current_parent;

		if (name != tldname && this_tld == tldname &&
			(!name->IsValidDomainType()))
		{
			name->SetDomainType(ServerName::DOMAIN_UNKNOWN);
			ServerName_item *new_item = OP_NEW(ServerName_item, (name));
			if (new_item == NULL)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return OpStatus::ERR_NO_MEMORY;
			}

			new_item->Into(&servername_list);
		}

		name = g_url_api->GetNextServerName();
	}

	if (servername_list.Empty())
		return OpStatus::OK;

	ProcessSpecification(servername_list, tldname, 1, (SuffixList_item *) suffix_list.First());

	ServerName_item *item;
	if (all_levels_are_TLDs)
	{
		while ((item = (ServerName_item *) servername_list.First()) != NULL)
		{
			if (item->name)
				item->name->SetDomainType(ServerName::DOMAIN_REGISTRY);

			OP_DELETE(item);
		}
	}
	else if (levels > 1)
	{
		item = (ServerName_item *) servername_list.First();

		while (item != NULL)
		{
			ServerName_item *item1 = item;
			item = (ServerName_item *) item->Suc();
			if (item1->name && item1->name->GetNameComponentCount() <= levels)
			{
				item1->name->SetDomainType(ServerName::DOMAIN_REGISTRY);
				OP_DELETE(item1);
			}
		}
	}

	while ((item = (ServerName_item *) servername_list.First()) != NULL)
	{
		if (item->name)
			item->name->SetDomainType(ServerName::DOMAIN_NORMAL);

		OP_DELETE(item);
	}

	return OpStatus::OK;
}


void PublicSuffix_Updater::ProcessSpecification(Head &servername_list, ServerName *current_suffix, unsigned int level, SuffixList_item *first_item)
{
	OP_ASSERT(current_suffix);

	// We are within the entry for "suffix"; caller will leave the entry.

	Head level_list;

	ServerName_item *item, *next_item = (ServerName_item *) servername_list.First();
	while (next_item)
	{
		item = next_item;
		next_item = (ServerName_item *) next_item->Suc();

		if (item->name != current_suffix && item->name->GetParentDomain() == current_suffix)
		{
			item->Out();

			item->Into(&level_list);
		}
	}

	if (level_list.Empty())
		return; // NOthing to do;

	BOOL all_levels_are_TLDs = FALSE;
	unsigned int levels = 0;
	if (level > 1 && first_item) // Top level handled specially
	{
		all_levels_are_TLDs = first_item->all_levels_are_TLDs;
		levels = first_item->levels;
		if (levels)
			levels += level;
	}

	SuffixList_item *suffix_list_item = first_item; //(SuffixList_item *) suffix_list.First();

	while (suffix_list_item)
	{
		do{
			ServerName *current = NULL;

			OpString attr;
			attr.Set(suffix_list_item->name);

			item = (ServerName_item *) level_list.First();

			while (item != NULL)
			{
				if (item->name && attr.CompareI(item->name->GetNameComponent(level)) == 0)
				{
					current = item->name;
					OP_DELETE(item);
					break;
				}

				item = (ServerName_item *) item->Suc();
			}

			if (current == NULL)
				break;

			if (suffix_list_item->registry)
			{
				current->SetDomainType(ServerName::DOMAIN_REGISTRY);

				ProcessSpecification(servername_list, current, level+1, suffix_list_item ? (SuffixList_item *) suffix_list_item->suffix_list.First() : NULL);
			}
			else if (!suffix_list_item->registry)
			{
				current->SetDomainType(ServerName::DOMAIN_NORMAL);
			}
		}while (0);

		suffix_list_item = (SuffixList_item *) suffix_list_item->Suc();
	}

	suffix_list_item = first_item;
	while (suffix_list_item)
	{
		if (suffix_list_item->all)
		{
			while ((item = (ServerName_item *) level_list.First())!= NULL)
			{
				suffix_list_item = (SuffixList_item *) suffix_list.First();
				if (item->name)
				{
					ServerName *current = item->name;
					current->SetDomainType(ServerName::DOMAIN_REGISTRY);

					ProcessSpecification(servername_list, current, level+1, suffix_list_item ? (SuffixList_item *) suffix_list_item->suffix_list.First() : NULL);
				}
				OP_DELETE(item);
			}
		}

		suffix_list_item = (SuffixList_item *) suffix_list_item->Suc();
	}

	// Move all remaining domains in this hierarchy into the list, so that we can tag them using the default values;
	next_item = (ServerName_item *) servername_list.First();
	while (next_item)
	{
		item = next_item;
		next_item = (ServerName_item *) next_item->Suc();

		if (item->name != current_suffix && item->name->GetCommonDomain(current_suffix) == current_suffix)
		{
			item->Out();

			item->Into(&level_list);
		}
	}

	if (all_levels_are_TLDs)
	{
		while ((item = (ServerName_item *) level_list.First()) != NULL)
		{
			if (item->name)
				item->name->SetDomainType(ServerName::DOMAIN_REGISTRY);

			OP_DELETE(item);
		}
	}
	else if (levels > level)
	{
		item = (ServerName_item *) servername_list.First();

		while (item != NULL)
		{
			ServerName_item *item1 = item;
			item = (ServerName_item *) item->Suc();
			if (item1->name && item1->name->GetNameComponentCount() <= levels)
			{
				item1->name->SetDomainType(ServerName::DOMAIN_REGISTRY);
				OP_DELETE(item1);
			}
		}
	}

	if (level > 1)
	{
		while ((item = (ServerName_item *) level_list.First()) != NULL)
		{
			  if (item->name && item->name->GetNameComponentCount() > level)
				item->name->SetDomainType(ServerName::DOMAIN_NORMAL);

			OP_DELETE(item);
		}
	}

	servername_list.Append(&level_list);
}

#endif
