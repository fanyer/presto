 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2004 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

/** @file WindowsSocketAddress2.h
  *
  * Windows version of the standard interface for Opera socket addresses.
  * 
  * @author Yngve Pettersen
  *
  * @version $Id $
  */


#ifndef __WINDOWSSOCKETADDRESS2_H
#define __WINDOWSSOCKETADDRESS2_H

#include "modules/pi/network/OpSocketAddress.h"
#include "modules/util/opstring.h"

#include <ws2tcpip.h>
#include <WSPiApi.h>  // compatibility layer for systems without getaddrinfo/freeaddrinfo

class WindowsSocketAddress2;


/*
 * Documentation in OprAbstractSocketAddress.h
 */



#ifndef _SS_MAXSIZE
// If the older version's of the Microsoft windows SDK's are used
// Copied from RFC 2553
typedef __int64 int64_t;
typedef unsigned short sa_family_t;
/*
 * Desired design of maximum size and alignment
 */
#define _SS_MAXSIZE    128  /* Implementation specific max size */
#define _SS_ALIGNSIZE  (sizeof (int64_t))
                         /* Implementation specific desired alignment */
/*
 * Definitions used for sockaddr_storage structure paddings design.
 */
#define _SS_PAD1SIZE   (_SS_ALIGNSIZE - sizeof (sa_family_t))
#define _SS_PAD2SIZE   (_SS_MAXSIZE - (sizeof (sa_family_t)+ _SS_PAD1SIZE + _SS_ALIGNSIZE))

typedef struct sockaddr_storage {
    sa_family_t  ss_family;     /* address family */
    /* Following fields are implementation specific */
    char      __ss_pad1[_SS_PAD1SIZE];	/* ARRAY OK 2004-02-04 yngve */
              /* 6 byte pad, this is to make implementation
              /* specific pad up to alignment field that */
              /* follows explicit in the data structure */
    int64_t   __ss_align;     /* field to force desired structure */
               /* storage alignment */
    char      __ss_pad2[_SS_PAD2SIZE];	/* ARRAY OK 2004-02-04 yngve */
              /* 112 byte pad to achieve desired size, */
              /* _SS_MAXSIZE value minus size of ss_family */
              /* __ss_pad1, __ss_align fields is 112 */
} SOCKADDR_STORAGE;


// Copied from RFC 2553
struct addrinfo {
    int     ai_flags;     /* AI_PASSIVE, AI_CANONNAME, AI_NUMERICHOST */
    int     ai_family;    /* PF_xxx */
    int     ai_socktype;  /* SOCK_xxx */
    int     ai_protocol;  /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
    size_t  ai_addrlen;   /* length of ai_addr */
    char   *ai_canonname; /* canonical name for nodename */
    struct sockaddr  *ai_addr; /* binary address */
    struct addrinfo  *ai_next; /* next structure in linked list */
} ;

//Partly copied from RFC2553
typedef int (WINAPI *LPFN_GETADDRINFO)(const char *nodename, const char *servname,
                      const struct addrinfo *hints,
                      struct addrinfo **res);

typedef void (WINAPI *LPFN_FREEADDRINFO)(struct addrinfo *ai);

// End of RFC 2553 related code

#define IN6_IS_ADDR_UNSPECIFIED IN6ADDR_ISANY
#define IN6_IS_ADDR_UNSPECIFIED_PARAM(x) (((sockaddr_in6 *) &x))
#define IN6_ADDR_EQUAL(x,y) (memcpy(x,y, sizeof(in_addr6)) == 0)

#else
#define IN6_IS_ADDR_UNSPECIFIED_PARAM(x) (&((sockaddr_in6 *) &x)->sin6_addr)
#endif

class WindowsSocketAddress2 : public OpSocketAddress
{
protected:
	friend class OpSocketAddress;
	friend class WindowsSocket2;
	friend class WindowsUdpSocket;
	friend class WindowsHostResolver2; // TODO: Should these be friends? Does the host resolver need to intialize winsocaddr2's?
	WindowsSocketAddress2();
	~WindowsSocketAddress2();
public:
	OP_STATUS SetAddress(const void* aAddress);
	void* Address();
	const int IPv4Address() const;
	OP_STATUS ToString(OpString* result);
	OP_STATUS FromString(const uni_char* address_string);
	const OpStringC8 ToString8();
	BOOL IsValid();
	void SetPort(UINT port);
	UINT Port();
	OP_STATUS Copy(OpSocketAddress* socket_address);
	BOOL IsEqual(OpSocketAddress* socket_address);
	BOOL IsHostEqual(OpSocketAddress* socket_address);
	OpSocketAddressNetType GetNetType();
	OpSocketAddress::Family GetAddressFamily() const;

private:
	BOOL IsValidImpl() const;

private:
	SOCKET_ADDRESS address_master;
	SOCKADDR_STORAGE sock_address;
	OpString string_address;

	unsigned int port_no;
};

#endif	//	__WINDOWSSOCKETADDRESS2_H
