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

#include "core/pch.h"

#if defined _NATIVE_SSL_SUPPORT_ && defined EXTERNAL_DIGEST_API

#include "modules/libssl/external/ext_digest/external_digest_man.h"

External_Digest_Handler::External_Digest_Handler()
{
	parameter_uid = 0;
	domain_count = 0;
	domains = NULL;
	method_id = g_ext_digest_method_counter;
	g_ext_digest_method_counter += 2;

	g_ext_digest_algorithm_counter = (SSL_HashAlgorithmType) (((int) g_ext_digest_algorithm_counter)+1);
	algorithm_id = g_ext_digest_algorithm_counter;
}

External_Digest_Handler::~External_Digest_Handler()
{
	OP_DELETEA(domains);
}

OP_STATUS External_Digest_Handler::Construct(const char *meth_name, const char **domains_in, int count, int param_uid)
{
	if(!meth_name || !*meth_name)
		return OpExternalDigestStatus::ERR_INVALID_NAME;
		
	if(domains_in == NULL || count == 0)
		return OpExternalDigestStatus::ERR_INVALID_DOMAIN;

	parameter_uid = param_uid;

	RETURN_IF_ERROR(method_name.Set(meth_name));

	domain_count =count;
	domains = OP_NEWA(External_digest_domain_item, count);
	RETURN_OOM_IF_NULL(domains);

	int i;
	for(i = 0; i<count; i++)
	{
		const char *current_domain = domains_in[i];
		
		if(!current_domain || !*current_domain)
			return OpExternalDigestStatus::ERR_INVALID_DOMAIN;

		if(op_strspn(current_domain, "0123456789.") == op_strlen(current_domain) || // IPv4 addresses
			current_domain[0] == ':' || // IPv6 addresses
			current_domain[0] != '.' || // names with no leading dot
			op_strchr(current_domain+1, '.' ) == NULL // names with no internal dot
			) 
		{
			// are always full match
			domains[i].full_match = TRUE;
			if(current_domain[0] == '.')
				current_domain++;
			if(!*current_domain)
				return OpExternalDigestStatus::ERR_INVALID_DOMAIN;
		}
		else
			domains[i].full_match = FALSE;

		RETURN_IF_ERROR(domains[i].domain.Set(current_domain));
		domains[i].len = domains[i].domain.Length();
	}

	return OpExternalDigestStatus::OK;
}

BOOL External_Digest_Handler::MatchDomain(const OpStringC8 &host)
{
	int i;

	if(host.IsEmpty())
		return FALSE;

	for(i=0; i< domain_count; i++)
	{
		if(!domains[i].full_match)
		{
			int host_len = host.Length();
			if(host_len > domains[i].len)
			{
				if(domains[i].domain.CompareI(host.CStr()+(host_len-domains[i].len)) == 0)
					return TRUE;
			}
			if(host.CompareI(domains[i].domain.CStr()+1) == 0)
				return TRUE;
		}
		else if(domains[i].domain.CompareI(host) == 0)
			return TRUE;
	}
	return FALSE;
}

static AutoDeleteHead external_digest_list;

External_Digest_Handler *GetExternalDigestMethod(const OpStringC8 &name, const OpStringC8 &hostname, OP_STATUS &op_err)
{
	op_err = OpExternalDigestStatus::OK;

	External_Digest_Handler *item = (External_Digest_Handler *) external_digest_list.First();
	
	while(item)
	{
		if(item->Name().CompareI(name) == 0)
		{
			if(item->MatchDomain(hostname))
				return item;
			return NULL;
		}

		item = item->Suc();
	}

	op_err = OpExternalDigestStatus::ERR_ALG_NOT_FOUND;
	return NULL;
}

External_Digest_Handler *GetExternalDigestMethod(int method_id)
{
	method_id &= ~0x0001; // Removes sess type;

	External_Digest_Handler *item = (External_Digest_Handler *) external_digest_list.First();
	
	while(item)
	{
		if(item->GetMethodID() == method_id)
		{
			return item;
		}

		item = item->Suc();
	}

	return NULL;
}

SSL_Hash *GetExternalDigestMethod(SSL_HashAlgorithmType digest, OP_STATUS &op_err)
{
	External_Digest_Handler *item = (External_Digest_Handler *) external_digest_list.First();
	op_err = OpStatus::OK;
	
	while(item)
	{
		if(item->GetAlgID() == digest)
		{
			SSL_Hash *new_item = NULL;
			op_err =item->GetDigest(new_item);

			if(OpStatus::IsError(op_err))
			{
				return NULL;
			}
			
			return new_item;
		}

		item = item->Suc();
	}
	op_err = OpStatus::ERR_OUT_OF_RANGE;

	return NULL;
}

OP_STATUS AddExternalDigestMethod(External_Digest_Handler *_spec)
{
	OpStackAutoPtr<External_Digest_Handler> spec(_spec);

	if(!spec.get())
		return OpExternalDigestStatus::ERR;

#if defined EXTERNAL_DIGEST_API_TEST || defined SELFTEST
	if(
#ifdef SELFTEST
		!g_selftest.running &&
#endif
		(spec->Name().CompareI("md5") == 0 ||
		spec->Name().CompareI("sha") == 0))
		return OpExternalDigestStatus::ERR_INVALID_NAME;
#endif
	if(spec->Name().Length() >= 5 &&
		op_stricmp(spec->Name().CStr() + (spec->Name().Length() - 5), "-sess") == 0)
		return OpExternalDigestStatus::ERR_INVALID_NAME;

	External_Digest_Handler *item = (External_Digest_Handler *) external_digest_list.First();
	
	while(item)
	{
		if(item->Name().CompareI(spec->Name()) == 0)
		{
			return OpExternalDigestStatus::ERR_ALGORITHM_EXIST;
		}

		item = item->Suc();
	}

	spec->Into(&external_digest_list);
	spec->Increment_Reference();
	spec.release();
	return OpExternalDigestStatus::OK;
}

void RemoveExternalDigestMethod(const OpStringC8 &name)
{
	if(name.IsEmpty())
		return;

	External_Digest_Handler *item = (External_Digest_Handler *) external_digest_list.First();
	
	while(item)
	{
		if(item->Name().CompareI(name) == 0)
		{
			item->Out();
			if(item->Decrement_Reference() == 0)
				OP_DELETE(item);
			return;
		}

		item = item->Suc();
	}
}

void ShutdownExternalDigest()
{
	External_Digest_Handler *next_item = (External_Digest_Handler *) external_digest_list.First();
	
	while(next_item)
	{
		External_Digest_Handler *item = next_item;
		next_item = item->Suc();

		item->Out();
		if(item->Decrement_Reference() == 0)
			OP_DELETE(item);
	}
}


#endif // EXTERNAL_DIGEST_API 

