 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2004 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

/** @file WindowsSocketAddress2.cpp
  * 
  * Windows implementation of the Opera socket address.
  *
  * @author Yngve Pettersen
  *
  * @version $Id $
  */


#include "core/pch.h"

#include "WindowsSocketAddress2.h"
#include "WindowsSocket2.h"

#include "modules/url/protocols/comm.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/network/network_type.h"

OP_STATUS
OpSocketAddress::Create(OpSocketAddress** socket_address)
{
	if (!WindowsSocket2::m_manager)
	{
		WindowsSocketManager* manager = new WindowsSocketManager(); // This initializes the libraries.
		if (!manager)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		OP_STATUS s = manager->Init();
		if(OpStatus::IsError(s))
		{
			delete manager;
			return s;
		}
		WindowsSocket2::m_manager = manager; // TODO: Figure out where this is supposed to be deleted.
	}
	(*socket_address) = new WindowsSocketAddress2();

	return (*socket_address) ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

WindowsSocketAddress2::WindowsSocketAddress2() 
{
	memset(&sock_address, 0, sizeof(sock_address));
	sock_address.ss_family = AF_INET;
	address_master.lpSockaddr = (sockaddr *) &sock_address;
	address_master.iSockaddrLength = sizeof(sock_address);

	port_no = 0;
}

WindowsSocketAddress2::~WindowsSocketAddress2() 
{
}

/* input LPSOCKET_ADDRESS */
OP_STATUS WindowsSocketAddress2::SetAddress(const void* aAddress)
{
	LPSOCKET_ADDRESS aSockAddr = (LPSOCKET_ADDRESS) aAddress;
	
	OP_ASSERT(aSockAddr);
	OP_ASSERT(aSockAddr->lpSockaddr);
	OP_ASSERT(aSockAddr->iSockaddrLength);
	OP_ASSERT(aSockAddr->iSockaddrLength < sizeof(sock_address));
	
	if(aSockAddr && aSockAddr->iSockaddrLength <= sizeof(sock_address))
	{
		memcpy(&sock_address, aSockAddr->lpSockaddr, aSockAddr->iSockaddrLength);
		address_master.iSockaddrLength = aSockAddr->iSockaddrLength;
	}

	return OpStatus::OK;
}

/* returns LPSOCKET_ADDRESS */
void* WindowsSocketAddress2::Address()
{
	return &address_master;
}

const int WindowsSocketAddress2::IPv4Address() const
{
	return (/*sock_address.ss_family == AF_INET ? sock_address.AddressIn.sin_addr.s_addr :*/ INADDR_NONE);
}

OP_STATUS WindowsSocketAddress2::ToString(OpString* result)
{
	uni_char *addr_text = (uni_char *) g_memory_manager->GetTempBuf2k();
	DWORD addr_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen())-1;
	
	int res = WSAAddressToString(address_master.lpSockaddr, address_master.iSockaddrLength, NULL, addr_text, &addr_len);
	
	if(res == SOCKET_ERROR)
		addr_len = 0;
	
	addr_text[addr_len] = '\0'; // just to be safe

	if(address_master.lpSockaddr->sa_family == AF_INET6)
	{
		uni_char *colon = uni_strrchr(addr_text, ':');

		// if it's a IPv6 address including a port, the format will be
		// https://[2001:db8::1428:57ab]:443/

		if(colon && colon > addr_text && *(colon-1) == ']')
		{
			*colon='\0'; // Remove the port form the string...
		}
	}
	else
	{
		uni_char *colon = uni_strrchr(addr_text, ':');

		if(colon)
		{
			*colon='\0'; // Remove the port form the string...
		}
	}
	return result->Set(addr_text);	
}

const OpStringC8 WindowsSocketAddress2::ToString8()
{
	char *addr_text = (char *) g_memory_manager->GetTempBuf2k();
	DWORD addr_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen())-1;

	int res = WSAAddressToStringA(address_master.lpSockaddr, address_master.iSockaddrLength, NULL, addr_text, &addr_len);

	if(res == SOCKET_ERROR)
		addr_len = 0;

	addr_text[addr_len] = '\0'; // just to be safe

	if(address_master.lpSockaddr->sa_family == AF_INET6)
	{
		char *colon = op_strrchr(addr_text, ':');

		// if it's a IPv6 address including a port, the format will be
		// https://[2001:db8::1428:57ab]:443/

		if(colon && colon > addr_text && *(colon-1) == ']')
		{
			*colon='\0'; // Remove the port form the string...
		}
	}
	else
	{
		char *colon = op_strrchr(addr_text, ':');

		if(colon)
		{
			*colon='\0'; // Remove the port form the string...
		}
	}

	return addr_text;
}

OP_STATUS WindowsSocketAddress2::FromString(const uni_char* address_string)
{
	OpStringC a_string(address_string);
	OpStringC addressC2(address_string);
	int a_len;
	int addressfamily = AF_INET;
	INT addr_len;

	RETURN_IF_ERROR(string_address.Set(address_string));

	addr_len = address_master.iSockaddrLength = sizeof(sock_address/*buf*/);

	// IPv6 address might have a zone identifier at the end, so 
	// make sure we only compare up to that identifier as it's not always present.
	// Example: fe80::51ec:c5de:ffb0:a8c1%11
	int zone_identifier = addressC2.FindLastOf('%');
	if(zone_identifier != KNotFound)
		a_len = zone_identifier;
	else
		a_len = uni_strlen(address_string);

	if(a_string.HasContent() && a_string.FindFirstOf(':') != KNotFound && 
		addressC2.SpanOf(UNI_L("0123456789ABCDEFabcdef.:")) == a_len)
	{
		addressfamily = AF_INET6;
	}
	if(WSAStringToAddress((uni_char*)address_string, addressfamily, NULL, address_master.lpSockaddr, &addr_len) == SOCKET_ERROR)
	{
#ifdef _DEBUG
		/*int err =*/ WSAGetLastError();
#endif

		memset(&sock_address, 0, sizeof(sock_address));
		sock_address.ss_family = AF_INET;
		address_master.iSockaddrLength = sizeof(struct sockaddr_in);
		return OpStatus::ERR;
	}
	else
	{
		address_master.iSockaddrLength = addr_len;
		return OpStatus::OK;
	}
}

BOOL WindowsSocketAddress2::IsValid()
{
	return IsValidImpl();
}

BOOL WindowsSocketAddress2::IsValidImpl() const
{
	return ((sock_address.ss_family == AF_INET ? ((sockaddr_gen *) &sock_address)->AddressIn.sin_addr.s_addr == 0 : 
		IN6_IS_ADDR_UNSPECIFIED(IN6_IS_ADDR_UNSPECIFIED_PARAM(sock_address))) 
		? FALSE : TRUE);
}

void WindowsSocketAddress2::SetPort(UINT port)
{
	port_no = port;
}

UINT WindowsSocketAddress2::Port()
{
	return port_no;
}

OP_STATUS WindowsSocketAddress2::Copy(OpSocketAddress* socket_address)
{
	sock_address = ((WindowsSocketAddress2*)socket_address)->sock_address;
	address_master.iSockaddrLength = ((WindowsSocketAddress2*)socket_address)->address_master.iSockaddrLength;

	port_no = socket_address->Port();

	return OpStatus::OK;
}

BOOL WindowsSocketAddress2::IsEqual(OpSocketAddress* socket_address)
{
	if (!IsHostEqual(socket_address))
	{
		return FALSE;
	}
	return port_no == (((WindowsSocketAddress2*)socket_address)->port_no);
}

BOOL WindowsSocketAddress2::IsHostEqual(OpSocketAddress* socket_address)
{
	if (address_master.iSockaddrLength != (((WindowsSocketAddress2*)socket_address)->address_master.iSockaddrLength))
		return FALSE;
				
	if (sock_address.ss_family != (((WindowsSocketAddress2*)socket_address)->sock_address.ss_family))
		return FALSE;

	if(sock_address.ss_family == AF_INET)
	{
		return ( ((sockaddr_gen *) (&sock_address))->AddressIn.sin_addr.s_addr == 
			((sockaddr_gen *) &((WindowsSocketAddress2*)socket_address)->sock_address)->AddressIn.sin_addr.s_addr);
	}
	else if(sock_address.ss_family == AF_INET6)
	{
		return IN6_ADDR_EQUAL(&((sockaddr_in6 *) &sock_address)->sin6_addr, &((sockaddr_in6 *) &((WindowsSocketAddress2*)socket_address)->sock_address)->sin6_addr);
	}
	return FALSE;		
}

OpSocketAddressNetType WindowsSocketAddress2::GetNetType()
{
	if (!IsValid())
		return NETTYPE_UNDETERMINED;

	if (sock_address.ss_family == AF_INET)
	{
		UINT8* address = (UINT8 *)&((sockaddr_gen *) (&sock_address))->AddressIn.sin_addr.s_addr;

		return IPv4type(address);
	}
	else if(sock_address.ss_family == AF_INET6)
	{
		UINT8* address = (UINT8 *)&((sockaddr_gen *) (&sock_address))->AddressIn6.sin6_addr.s6_addr;

		return IPv6type(address);
	}

	OP_ASSERT(!"WindowsSocketAddress2::GetNetType(): unhandled address family.");

	return NETTYPE_PUBLIC;
}

OpSocketAddress::Family WindowsSocketAddress2::GetAddressFamily() const
{
	if (!IsValidImpl())
		return UnknownFamily;

	if (sock_address.ss_family == AF_INET)
		return IPv4;
	else if (sock_address.ss_family == AF_INET6)
		return IPv6;
	else
		return UnknownFamily;
}
