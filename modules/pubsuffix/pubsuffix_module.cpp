/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined PUBSUFFIX_ENABLED

#include "modules/url/url_man.h"

#include "modules/pubsuffix/pubsuffix_module.h"
#include "modules/pubsuffix/src/pubsuffix.h"

#include "modules/libssl/updaters.h"
#include "modules/olddebug/tstdump.h"

PubsuffixModule::PubsuffixModule()
{
}

void PubsuffixModule::InitL(const OperaInitInfo& info)
{
	LoadPubSuffixState();
}

void PubsuffixModule::Destroy()
{
	updaters.Clear();
}

struct SuffixString : public Link
{
	OpString8 suffix;

	virtual ~SuffixString(){if (InList()) Out();}
};

struct SuffixOverrideString : public Link
{
	OpString8 suffix;
	OpString8 urlstring;

	virtual  ~SuffixOverrideString(){if (InList()) Out();}
};

BOOL PubsuffixModule::HaveCheckedDomain(const OpStringC8 &tld)
{
	int dot = tld.FindLastOf('.');
	OpStringC8 tld2(tld.CStr() + (dot != KNotFound ? dot+1 : 0));

	SuffixString *item = (SuffixString *) checked_domains.First();
	while (item)
	{
		if (item->suffix.CompareI(tld2)==0)
			return TRUE;

		item = (SuffixString *) item->Suc();
	}
	return FALSE;
}

#define PUB_SUFFIX_XML_FILE_NAME UNI_L("pubsuffix.xml")

OP_STATUS PubsuffixModule::LoadPubSuffixState()
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(PUB_SUFFIX_XML_FILE_NAME, OPFILE_RESOURCES_FOLDER));

	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	updaters.Clear();

	XMLFragment xml;
	OP_STATUS status;

	if (OpStatus::IsSuccess(status = xml.Parse(static_cast<OpFileDescriptor*>(&file))))
	{
		if (xml.EnterElement(UNI_L("tlds")))
		{
			while (xml.HasMoreElements())
			{
				if (xml.EnterElement(UNI_L("tld")))
				{
					const uni_char *name = xml.GetAttribute(UNI_L("name"));
					ServerName *checking_domain = g_url_api->GetServerName(name, TRUE);

					if (checking_domain == NULL)
						return OpStatus::ERR_NULL_POINTER;
#ifdef PUBSUF_DEBUG
					PrintfTofile("pubsuff.txt", "check %s\n", checking_domain->Name());
#endif

					ServerName *tld2, *tld = checking_domain;

					while ((tld2 = tld->GetParentDomain()) != NULL)
						tld = tld2;

					if (tld->Name() == NULL || tld->GetName().FindFirstOf('.') != KNotFound)
						return OpStatus::ERR_NULL_POINTER;

					OpAutoPtr<PublicSuffix_Updater> new_updater(OP_NEW(PublicSuffix_Updater, (tld, checking_domain)));

					if (new_updater.get() == NULL)
					{
#ifdef PUBSUF_DEBUG
						PrintfTofile("pubsuff.txt", "check 3 %s\n", checking_domain->Name());
#endif
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						return OpStatus::ERR_NO_MEMORY;
					}


					RAISE_AND_RETURN_IF_ERROR(new_updater->Construct(MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION));

					OP_STATUS op_err;
					op_err = new_updater->ParseFile(xml);
					RAISE_AND_RETURN_IF_ERROR(op_err);
					new_updater.release()->Into(&updaters);
				}
				xml.LeaveElement();
			}
			xml.LeaveElement();
		}
	}
	else
	{
		file.Close();
		RETURN_IF_MEMORY_ERROR(status);
	}
	return file.Close();
}

OP_STATUS PubsuffixModule::CheckDomain(ServerName *checking_domain)
{
#ifdef PUBSUF_DEBUG
	PrintfTofile("pubsuff.txt", "check 0\n");
#endif
	if (checking_domain == NULL)
		return OpStatus::ERR_NULL_POINTER;
#ifdef PUBSUF_DEBUG
	PrintfTofile("pubsuff.txt", "check %s\n", checking_domain->Name());
#endif

	ServerName *tld= checking_domain;
	ServerName *tld2;
	while ((tld2 =tld->GetParentDomain()) != NULL )
		tld = tld2;

	if (tld->Name() == NULL || tld->GetName().FindFirstOf('.') != KNotFound)
		return OpStatus::ERR_NULL_POINTER;

	PublicSuffix_Updater * update_item2 = (PublicSuffix_Updater *) updaters.First();
	while (update_item2)
	{
		if (update_item2->GetSuffix() == tld)
		{
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "check 2 %s\n", checking_domain->Name());
#endif

			update_item2->ProcessFile();
			if (checking_domain->IsValidDomainType())
				return OpRecStatus::FINISHED;
			OP_ASSERT(!"We should have gotten a result here, is this domain not in our pubsuffix list!?");
			checking_domain->SetDomainType(ServerName::DOMAIN_NORMAL);
			return OpRecStatus::FINISHED;
		}
		update_item2 = (PublicSuffix_Updater *) update_item2->Suc();
	}

	if (tld != NULL)
	{
		ServerName *name = g_url_api->GetFirstServerName();
		while (name)
		{
			ServerName *this_tld = name, *current_parent = NULL;

			while ((current_parent = this_tld->GetParentDomain()) != NULL)
				this_tld = current_parent;

			if (name != tld && this_tld == tld && !name->IsValidDomainType())
			{
				name->SetDomainType(ServerName::DOMAIN_NORMAL);
			}

			name = g_url_api->GetNextServerName();
		}
		tld->SetDomainType(ServerName::DOMAIN_TLD);
	}
	return OpRecStatus::FINISHED;
}

void  PubsuffixModule::SetHaveCheckedDomain(const OpStringC8 &tld)
{
	int dot = tld.FindLastOf('.');
	OpStringC8 tld2(tld.CStr() + (dot != KNotFound ? dot+1 : 0));

	SuffixString *item = (SuffixString *) checked_domains.First();
	while (item)
	{
		if (item->suffix.CompareI(tld2)==0)
			return;

		item = (SuffixString *) item->Suc();
	}

	item = OP_NEW(SuffixString, ());
	if (item) //Ignore OOM
	{
		if ( OpStatus::IsSuccess(item->suffix.Set(tld2)))
			item->Into(&checked_domains);
		else
			OP_DELETE(item);
	}
}

#endif // PUBSUFFIX_ENABLED
