/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/certs/ssl_snlist.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"

OP_STATUS SSL_ServerName_List::SNList_string::AddString(const char *str, int len)
{
	OpAutoPtr<OpString8> new_name(OP_NEW(OpString8, ()));
	if(new_name.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_name->Set(str, len));

	if(new_name->Length() < len)
		return OpStatus::OK; // NUL byte in string. Delete, ignore, and continue with next

	return Add(new_name.release());
}


SSL_ServerName_List::SSL_ServerName_List()
{
	InternalInit();
}

void SSL_ServerName_List::InternalInit()
{
	IPadr_List.ForwardTo(this);
}

void SSL_ServerName_List::Reset()
{
	DNSNames.Clear();
	IPadr_List.Resize(0);
	CommonNames.Clear();
}


BOOL SSL_ServerName_List::MatchName(ServerName *servername)
{
	uint16 i,count;

	if(servername == NULL)
		return FALSE;

	Matched_name.Empty();

	count = DNSNames.GetCount();
	if(count)
	{
		for(i=0;i< count; i++)
		{
			if(MatchNameRegexp(servername, DNSNames.Get(i), TRUE))
			{
				Matched_name.Set(*DNSNames.Get(i));
				return TRUE;
			}
		}
		return FALSE; // If SAN/DNSNames are in use, they are authoritative, and all CNs MUST be in the SAN segment
	}

	// Should check IP address here, but is missing functionality

	count = CommonNames.GetCount();
	for(i=0;i< count; i++)
	{
		if(MatchNameRegexp(servername, CommonNames.Get(i), TRUE))
		{
			Matched_name.Set(*CommonNames.Get(i));
			return TRUE;
		}
	}

	return FALSE;
}

BOOL SSL_ServerName_List::MatchNameRegexp(ServerName *servername, OpStringC8 *compare_name, BOOL reg_exp)
{
	if(!compare_name)
		return FALSE;

	uint16 len = compare_name->Length();
	if(len != 0 && servername && servername->GetNameComponentCount())
	{
		int star_pos = compare_name->FindFirstOf('*');
		if(!reg_exp || star_pos == KNotFound)
		{
			ServerName *compare_server = g_url_api->GetServerName(*compare_name); // Don't need to create a name
			return compare_server == servername;
		}

		OpStringC8 server_name(servername->Name());

		ServerName *parent = servername->GetParentDomain(); // Need to check this first, or the name might not exist
		int dot_pos = compare_name->FindFirstOf('.');
		
		if((parent == NULL && dot_pos != KNotFound) || 
			(dot_pos != KNotFound && star_pos > dot_pos )||
			(parent && parent->GetNameComponentCount()<2) ) // at least two levels in parent domain
			return FALSE; // local name vs. FQDN

		ServerName *compare_parent = (dot_pos != KNotFound ? g_url_api->GetServerName(compare_name->CStr()+dot_pos+1) : NULL);
		if((dot_pos != KNotFound && compare_parent == NULL) || // unknown parent, or invalid name, cannot possibly match the server's parent domain
			parent != compare_parent) // Parent domain mismatch.
			return FALSE; 

		if(star_pos == 0 && dot_pos == 1)
			return TRUE; // matches all hostname components "*.domain"

		if(server_name.CompareI("xn--", STRINGLENGTH("xn--")))
			return FALSE; // Wildcards do not make sense for IDNA hostnames

		OpString8 pattern_start;
		OpString8 pattern_end;

		RETURN_VALUE_IF_ERROR(pattern_start.Set(compare_name->CStr(), star_pos), FALSE); // pre-*
		RETURN_VALUE_IF_ERROR(pattern_end.Set(compare_name->CStr()+ star_pos +1, dot_pos -star_pos -1), FALSE); // post-*

		OpStringC8 server_component(servername->GetNameComponent(servername->GetNameComponentCount()-1)); // the hostname

		int start_len = star_pos;
		int end_len = pattern_end.Length();
		int name_len = server_component.Length();

		if(name_len < start_len + end_len)
			return FALSE; // Too short name;

		if(pattern_start.CompareI(server_component.CStr(), start_len) == 0 &&
			pattern_end.CompareI(server_component.CStr()+name_len-end_len) == 0)
			return TRUE;
	}

	return FALSE;
}

#endif
