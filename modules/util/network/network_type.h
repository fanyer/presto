/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_UTIL_NETWORK_TYPE_H
#define MODULES_UTIL_NETWORK_TYPE_H
# ifdef UTIL_HAVE_NET_TYPE
#include "modules/pi/network/OpSocketAddress.h"
/** @file
 *
 * These two functions are provided as helpers to implementors of
 * OpSocketAddress::GetNetType().  Each takes the relevant network address as a
 * series of bytes in "network order", i.e. more significant bytes are at lower
 * addresses in the UINT8 array.  Since this is the format in which addresses
 * are communicated over the wire by DNS, most platform implementations should
 * find this is an easy form in which to deliver the data.
 *
 * Some systems implement IN6_IS_ADDR_* macros which may yield more efficient
 * implementations on platforms that support them: see implementation of
 * IPv6type for which of them map onto which Opera socket type.
 */

/** IPv4 classifier.
 *
 * @param addr IPv4 address: four bytes, in network order (big-endian);
 * e.g. the .s_addr member of a POSIX struct in_addr.
 * @return See OpSocketAddressNetType.
 */
OpSocketAddressNetType IPv4type(const UINT8 addr[4]);

/** IPv6 classifier.
 *
 * @param addr IPv6 address: sixteen bytes, in network order (big-endian);
 * e.g. the .s6_addr member of a POSIX struct in6_addr.
 * @return See OpSocketAddressNetType.
 */
OpSocketAddressNetType IPv6type(const UINT8 addr[16]);

# endif // UTIL_HAVE_NET_TYPE
#endif // MODULES_UTIL_NETWORK_TYPE_H
