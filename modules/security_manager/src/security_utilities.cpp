/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_gadget_representation.h"
#include "modules/security_manager/src/security_utilities.h"
#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadgetClass.h"
# include "modules/gadgets/OpGadget.h"
#endif
# include "modules/url/url_man.h"
# include "modules/url/protocols/scomm.h"
# include "modules/url/protocols/comm.h"
# include "modules/pi/network/OpSocketAddress.h"

/* IPAddress */

/* static */ int
IPAddress::Compare(const IPv6Address &a, const IPv6Address &b)
{
	for (unsigned i = 0; i < 16; i++)
	{
		if (a[i] > b[i])
			return 1;
		else if (a[i] < b[i])
			return -1;
	}

	return 0;
}

OP_STATUS
IPAddress::Parse(const uni_char *ip_addr_str, BOOL same_protocol, const uni_char **end_addr)
{
	/* Now parse that thing...  This may not even work because we don't
	 * know whether the format of the string is decimal or hex. For IPv4,
	 * We assume decimal because FromString accepts decimal format.
	 *
	 * For IPv6, components are hexadecimal.
	 *
	 * For IPv4: %d.%d.%d.%d
	 * For IPv6: %x:%x:%x:%x:%x:%x:%x:%x
	 */

	unsigned a[8] = {0};
	if (uni_sscanf(ip_addr_str, UNI_L("%d.%d.%d.%d"), &a[0], &a[1], &a[2], &a[3]) == 4)
	{
		if (same_protocol && !is_ipv4 || a[0] > 0xff || a[1] > 0xff || a[2] > 0xff || a[3] > 0xff)
			return OpStatus::ERR;
		is_ipv4 = TRUE;
		u.ipv4 = (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];

		if (end_addr)
		{
			/* Move buffer pointer past parsed address string. */
			unsigned dots_to_see = 3;
			while (dots_to_see > 0)
			{
				if (*ip_addr_str == '.')
					dots_to_see--;
				ip_addr_str++;
			}
			while (*ip_addr_str >= '0' && *ip_addr_str <= '9')
				ip_addr_str++;
		}
	}
	else
	{
		/* For IPv6 text representations, the above syntactic form is the
		 * canonical & expanded version. RFC-5952 defines some
		 * syntatic rules that should be used to shorten addresses
		 * with consecutive zero-valued components to make them more
		 * human friendly. An application must be capable of accepting
		 * those shorthands as input.
		 */
		int run_length_start = -1;
		unsigned fields_read = 0;
		for (unsigned i = 0; i < 8; i++)
		{
			if (*ip_addr_str == ':')
			{
				if (run_length_start >= 0 || ip_addr_str[1] != ':')
					return OpStatus::ERR;

				run_length_start = i;
				ip_addr_str += 2;
			}
			else if (*ip_addr_str == '-' || *ip_addr_str == 0)
				break;
			else if (uni_sscanf(ip_addr_str, UNI_L("%x"), &a[fields_read]) == 1)
			{
				if (a[fields_read] > 0xffff)
					return OpStatus::ERR;
				fields_read++;
				while (*ip_addr_str != ':' && *ip_addr_str != '-' && *ip_addr_str != 0)
					ip_addr_str++;
				if (*ip_addr_str == ':' && ip_addr_str[1] != ':')
					ip_addr_str++;
			}
			else
				return OpStatus::ERR;
		}
		if (same_protocol && is_ipv4)
			return OpStatus::ERR;

		if (run_length_start >= 0)
		{
			unsigned zero_run_length = 8 - fields_read;
			if (fields_read > 0)
			{
				op_memmove(&a[run_length_start + zero_run_length], &a[run_length_start], (fields_read - run_length_start) * sizeof(unsigned));
				for (unsigned i = 0; i < zero_run_length; i++)
					a[run_length_start + i] = 0;
			}
		}

		is_ipv4 = FALSE;
		for (unsigned i = 0; i < 8; i++)
		{
			u.ipv6[2*i] = (a[i] >> 8) & 0xff;
			u.ipv6[2*i + 1] = a[i] & 0xff;
		}
	}
	if (end_addr)
		*end_addr = ip_addr_str;

	return OpStatus::OK;
}

/* static */ OP_STATUS
IPAddress::Export(const IPv4Address &addr, OpString &text)
{
	// %d.%d.%d.%d, 4 times 3 digits + 3 dots + \0
	uni_char buf[4*3 + 3 + 1]; // ARRAY OK 2010-11-10 jborsodi
	unsigned a, b, c, d;
	a = (addr >> 24) & 0xff;
	b = (addr >> 16) & 0xff;
	c = (addr >> 8) & 0xff;
	d = addr & 0xff;
	if (uni_sprintf(buf, UNI_L("%d.%d.%d.%d"), a, b, c, d) < 0)
		return OpStatus::ERR;
	return text.Append(buf);
}

/* static */ OP_STATUS
IPAddress::Export(const IPv6Address &addr, OpString &text, BOOL for_user)
{
	// %x:%x:%x:%x:%x:%x:%x:%x, 8 times 4 digits + 7 colons + \0
	uni_char buf[8*4 + 7 + 1]; // ARRAY OK 2010-11-11 jborsodi
	unsigned digits[8]; // ARRAY OK 2010-11-10 jborsodi
	for (unsigned i = 0; i < 8; ++i)
	{
		digits[i] = (addr[i*2] << 8) | addr[i*2 + 1];
	}

	if (for_user)
	{
		unsigned run_length = 0;
		unsigned run_length_start = 0;

		unsigned running_length = 0;
		unsigned i = 0;
		for (; i < 8; i++)
			if (digits[i] == 0)
				running_length++;
			else
			{
				if (running_length > 1 && run_length < running_length)
				{
					run_length = running_length;
					run_length_start = i - run_length;
				}
				running_length = 0;
			}
		if (running_length > 1 && run_length < running_length)
		{
			run_length = running_length;
			run_length_start = i - run_length;
		}

		if (run_length > 0)
		{
			buf[0] = 0;
			for (unsigned i = 0; i < 8; i++)
				if (i == run_length_start)
				{
					if (uni_sprintf(buf + uni_strlen(buf), i == 0 ? UNI_L("::") : UNI_L(":")) < 0)
						return OpStatus::ERR;
					i += run_length - 1;
				}
				else
				{
					if (uni_sprintf(buf + uni_strlen(buf), i < 7 ? UNI_L("%x:") : UNI_L("%x"), digits[i]) < 0)
						return OpStatus::ERR;
				}

			return text.Append(buf);
		}
	}

	if (uni_sprintf(buf, UNI_L("%x:%x:%x:%x:%x:%x:%x:%x"), digits[0], digits[1], digits[2], digits[3], digits[4], digits[5], digits[6], digits[7]) < 0)
		return OpStatus::ERR;

	return text.Append(buf);
}

OP_STATUS
IPAddress::Export(OpString &text, BOOL for_user)
{
	if (is_ipv4)
		return Export(u.ipv4, text);
	else
		return Export(u.ipv6, text, for_user);
}

/* IPRange */

OP_STATUS
IPRange::Parse(const uni_char *expr)
{
	IPAddress from_address;
	RETURN_IF_ERROR(from_address.Parse(expr, FALSE, &expr));

	while (uni_isspace(*expr))
		expr++;
	if (*expr == 0)
	{
		FromAddress(from_address);
		return OpStatus::OK;
	}
	else if (*expr++ == '-')
	{
		IPAddress to_address;
		to_address.is_ipv4 = from_address.is_ipv4;
		while (uni_isspace(*expr))
			expr++;
		RETURN_IF_ERROR(to_address.Parse(expr, TRUE));
		return WithRange(from_address, to_address);
	}
	else
		return OpStatus::ERR;
}

int
IPRange::Compare(const IPv4Address a) const
{
	OP_ASSERT(is_ipv4);
	if (a >= u.ipv4.from && a <= u.ipv4.to)
		return 0;
	else if (a < u.ipv4.from)
		return -1;
	else
		return 1;
}

int
IPRange::Compare(const IPv6Address &a) const
{
	OP_ASSERT(!is_ipv4);
	int low_res = IPAddress::Compare(a, u.ipv6.from);
	int high_res = IPAddress::Compare(a, u.ipv6.to);

	if (low_res >= 0 && high_res <= 0)
		return 0;
	else if (low_res < 0)
		return -1;
	else
		return 1;
}

OP_STATUS
IPRange::Compare(const IPAddress &a, int &result) const
{
	if (a.is_ipv4 != is_ipv4)
		return OpStatus::ERR;

	result = is_ipv4 ? Compare(a.u.ipv4) : Compare(a.u.ipv6);
	return OpStatus::OK;
}

/* private */ void
IPRange::SetRange(const IPAddress &from_address, const IPAddress &to_address)
{
	OP_ASSERT(from_address.is_ipv4 == to_address.is_ipv4);
	is_ipv4 = from_address.is_ipv4;
	if (is_ipv4)
	{
		u.ipv4.from = from_address.u.ipv4;
		u.ipv4.to = to_address.u.ipv4;
	}
	else
	{
		op_memcpy(&u.ipv6.from, from_address.u.ipv6, sizeof(u.ipv6.from));
		op_memcpy(&u.ipv6.to, to_address.u.ipv6, sizeof(u.ipv6.to));
	}
}

void
IPRange::FromAddress(const IPAddress &address)
{
	SetRange(address, address);
}

OP_STATUS
IPRange::WithRange(const IPAddress &from_address, const IPAddress &to_address)
{
	if (from_address.is_ipv4 != to_address.is_ipv4)
		return OpStatus::ERR;

	if (from_address.is_ipv4 && from_address.u.ipv4 > to_address.u.ipv4)
		return OpStatus::ERR;
	else if (!from_address.is_ipv4 && IPAddress::Compare(from_address.u.ipv6, to_address.u.ipv6) > 0)
		return OpStatus::ERR;

	SetRange(from_address, to_address);
	return OpStatus::OK;
}

/* static */ OP_STATUS
IPRange::Export(const IPRange &range, OpString &from, OpString &to, BOOL for_user)
{
	if (range.is_ipv4)
	{
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv4.from, from));
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv4.to, to));
	}
	else
	{
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv6.from, from, for_user));
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv6.to, to, for_user));
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
IPRange::Export(const IPRange &range, OpString &text, BOOL for_user)
{
	if (range.is_ipv4)
	{
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv4.from, text));
		RETURN_IF_ERROR(text.Append(UNI_L("-")));
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv4.to, text));
	}
	else
	{
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv6.from, text, for_user));
		RETURN_IF_ERROR(text.Append(UNI_L("-")));
		RETURN_IF_ERROR(IPAddress::Export(range.u.ipv6.to, text, for_user));
	}

	return OpStatus::OK;
}

/* OpSecurityUtilities */

OpSecurityUtilities::OpSecurityUtilities()
#ifdef PUBLIC_DOMAIN_LIST
	: m_public_domain_list(NULL)
#endif // PUBLIC_DOMAIN_LIST
{
}

OpSecurityUtilities::~OpSecurityUtilities()
{
#ifdef PUBLIC_DOMAIN_LIST
	OP_DELETE(m_public_domain_list);
#endif // PUBLIC_DOMAIN_LIST
}

/* static */ BOOL
OpSecurityUtilities::IsIPAddressOrRange(const uni_char *str)
{
	int dots = 0;
	int colons = 0;
	int dashes = 0;
	int alphas = 0;
	int digits = 0;
	int hexs = 0;
	int colon_group = 0;

	const uni_char *q = NULL;
	for (q = str; q && *q != '\0'; q++)
	{
		if (*q == '.')
			dots++;
		else if (*q == ':')
		{
			colons++;
			if (q > str && q[-1] == ':')
				colon_group++;
		}
		else if (*q == '-')
			dashes++;
		else if (uni_isalpha(*q))
		{
			if ((*q >= 'a' && *q <= 'f') || (*q >= 'A' && *q <= 'F'))
				hexs++;
			alphas++;
		}
		else if (uni_isdigit(*q))
			digits++;
	}

	bool is_ip = false;

	// check for IPv4 or IPv4 range (0.0.0.0 or 0.0.0.0-1.1.1.1)
	if (!alphas && !colons && (dots == 3 || (dots == 6 && dashes == 1)))
	{
		if (dashes == 1)
		{
			if (digits >= 8 && digits <= 24)
				is_ip = true;
		}
		else
		{
			if (digits >= 4 && digits <= 12)
				is_ip = true;
		}
	}
	// check for IPv6 or IPv6 range.
	else if (!dots && colons >= 2 && colons <= 14 && dashes <= 1 && hexs == alphas)
	{
		if (dashes == 0)
			is_ip = colon_group == 0 ? colons == 7 : colons <= 7;
		else
			is_ip = colon_group == 0 ? colons == 14 : true;
	}

	return is_ip;
}

/* static */ OP_STATUS
OpSecurityUtilities::ParsePortRange(const uni_char *expr, unsigned int &low, unsigned int &high)
{
	uni_char *end = NULL;
	long n = uni_strtol(expr, const_cast<uni_char**>(&end), 10, NULL);
	if (n == LONG_MAX || n <= 0)
		return OpStatus::ERR;

	low = static_cast<unsigned int>(n);
	expr = end;
	while (uni_isspace(*expr))
		expr++;
	if (*expr++ == '-')
	{
		while (uni_isspace(*expr))
			expr++;
		long m = uni_strtol(expr, const_cast<uni_char**>(&end), 10, NULL);
		if (m == LONG_MAX || m <= 0 || m < n)
			return OpStatus::ERR;
		high = static_cast<unsigned int>(m);
	}
	else
		high = low;

	return OpStatus::OK;
}

/* static */ OP_STATUS
OpSecurityUtilities::ExtractIPAddress(OpSocketAddress *sa, IPAddress &addr)
{
	OpString ip_addr;

	RETURN_IF_ERROR(sa->ToString(&ip_addr));
	return addr.Parse(ip_addr.CStr());
}

/* static */ OP_STATUS
OpSecurityUtilities::ResolveURL(const URL &url, IPAddress &addr, OpSecurityState &state)
{
	OpSocketAddress* sa = NULL;

	ServerName* server = (ServerName *)url.GetAttribute(URL::KServerName, static_cast<void*>(NULL));

	if (server == NULL)
		return OpStatus::ERR;

	if (server->IsHostResolved())
	{
		state.suspended = FALSE;
		if (state.host_resolving_comm != NULL)
		{
			SComm::SafeDestruction( state.host_resolving_comm );
			state.host_resolving_comm = NULL;
		}

		if ((sa = server->SocketAddress()) == NULL ||
			!sa->IsValid())
			return OpStatus::ERR;

		return ExtractIPAddress(sa, addr);
	}

	if (state.host_resolving_comm == NULL)
	{
		Comm *comm = 0;
		CommState s;

		comm = Comm::Create(g_main_message_handler, server, 80);
		if (!comm)
			return OpStatus::ERR;

		// First, start the lookup.
		s = comm->LookUpName(server);
		if (s == COMM_REQUEST_FINISHED)
		{
			// Got it
			sa = comm->HostName()->SocketAddress();

			SComm::SafeDestruction(comm);

			return ExtractIPAddress(sa, addr);
		}
		else if (s != COMM_LOADING && s != COMM_WAITING_FOR_SYNC_DNS)
		{
			SComm::SafeDestruction(comm);
			return OpStatus::ERR;
		}

		state.suspended = TRUE;
		state.host_resolving_comm = comm;

		return OpStatus::OK;
	}
	else
	{
		SComm::SafeDestruction(state.host_resolving_comm);
		state.host_resolving_comm = NULL;
		state.suspended = FALSE;
		return OpStatus::ERR;
	}
}

/* static */ BOOL
OpSecurityUtilities::IsSafeToExport(const URL &url)
{
	switch (url.Type())
	{
	case URL_HTTP:
	case URL_HTTPS:
	case URL_FTP:
	case URL_FILE:
	case URL_DATA:
		return TRUE;
	default:
		return FALSE;
	}
}

OP_STATUS
OpSecurityUtilities::IsPublicDomain(const uni_char *domain, BOOL &is_public_domain, BOOL *top_domain_was_known_to_exist)
{
	if (top_domain_was_known_to_exist)
		*top_domain_was_known_to_exist = FALSE;

	// Require at least one internal dot in the string
	const uni_char *period_pos = uni_strchr(domain, '.');
	BOOL has_inner_dot = period_pos && *(period_pos+1) != '\0';

#ifdef PUBLIC_DOMAIN_LIST
	if (!has_inner_dot)
	{
		// No need to check against the database since it's incomplete and we will assume
		// that it's a top domain (known or unknown)
		is_public_domain = TRUE;
		return OpStatus::OK;
	}
	if (!m_public_domain_list)
	{
		m_public_domain_list = OP_NEW(PublicDomainDatabase, ());
		if (!m_public_domain_list)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		OpFile pdd_file;
		OP_STATUS status = pdd_file.Construct(UNI_L("public_domains.dat"), OPFILE_INI_FOLDER);
		if (OpStatus::IsSuccess(status))
		{
			status = m_public_domain_list->ReadDatabase(pdd_file);
		}

		if (OpStatus::IsError(status))
		{
			OP_DELETE(m_public_domain_list);
			m_public_domain_list = NULL;
			return status;
		}
	}

	OP_BOOLEAN result = m_public_domain_list->IsPublicDomain(domain, top_domain_was_known_to_exist);
	if (OpStatus::IsError(result))
	{
		return result;
	}

	is_public_domain = (result == OpBoolean::IS_TRUE);
	return OpStatus::OK;
#else
	is_public_domain = !has_inner_dot;
	return OpStatus::OK;
#endif // PUBLIC_DOMAIN_LIST

}
