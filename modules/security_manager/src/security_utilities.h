/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SECURITY_UTILITIES_H
#define SECURITY_UTILITIES_H

#ifdef PUBLIC_DOMAIN_LIST
# include "modules/security_manager/src/publicdomainlist.h"
#endif // PUBLIC_DOMAIN_LIST

typedef	UINT32 IPv4Address;
typedef	UINT8 IPv6Address[16];

/**
 * Representing an IPAddress, IPv4 or IPv6.
 */
class IPAddress
{
public:
	bool is_ipv4;
	union
	{
		IPv4Address ipv4;
		IPv6Address ipv6;
	} u;

	/** Ordered comparison of two IPv6 addresses.
	 * @param a Reference to IPv6Address.
	 * @param b Reference to IPv6Address.
	 * @return -1 if 'a' is a lower address than 'b', +1 if greater, 0 if equal.
	 */
	static int Compare(const IPv6Address &a, const IPv6Address &b);

	/** Parse an IP address. It accepts 2 input formats: %d.%d.%d.%d (IPv4)
	 * and %x:%x:%x:%x:%x:%x:%x:%x (IPv6, with zero component contraction.)
	 * @param addr The input string
	 * @param same_protocol If TRUE, parsed address must match protocol
	 * of the address. Otherwise and if successfully parsed, update protocol
	 * to match that of the parsed address.
	 * @param [out]end_addr If successful, and end_addr is non-NULL, set it
	 * to the buffer position after the parse.
	 * @return OpStatus::OK if successful, OpStatus::ERR if input is
	 * not same protocol as address (if same_protocol is TRUE); OpStatus::ERR
	 * if input is otherwise an syntactically invalid address string.
	 */
	OP_STATUS Parse(const uni_char *addr, BOOL same_protocol = FALSE, const uni_char **end_addr = NULL);

	/** Export IPv4 address to text format, exported as: %d.%d.%d.%d
	 * @param addr The IPv4 address to export
	 * @param[out] text String to place exported address in.
	 * @return OpStatus::OK if successful, OpStatus::ERR for other errors.
	 */
	static OP_STATUS Export(const IPv4Address &addr, OpString &text);

	/** Export IPv6 address to text format, exported as: %x:%x:%x:%x:%x:%x:%x:%x
	 * @param addr The IPv6 address to export.
	 * @param[out] text String to place exported address in.
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS Export(const IPv6Address &addr, OpString &text, BOOL for_user = FALSE);

	/** Export IP address to text format, using the appropriate protocol format.
	 * @param[out] text String to place exported address in.
	 * @param for_user If TRUE, use address-specific rules to shorten and make the
	 * text representation friendlier for the casual reader.
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Export(OpString &text, BOOL for_user = FALSE);
};

/**
 * IPRange represents a protocol range of IP addresses, [addressA, addressB].
 * The range is inclusive of its endpoints, and encompasses any address
 * above addressA and below addressB. Protocols IPv4 and IPv6 are supported.
 */
class IPRange
{
public:
	struct IPv4Range
	{
		IPv4Address from;
		IPv4Address to;
	};

	struct IPv6Range
	{
		IPv6Address from;
		IPv6Address to;
	};

	bool is_ipv4;
	union
	{
		IPv4Range ipv4;
		IPv6Range ipv6;
	} u;

	/**
	 * Make IPRange be a range encompassing the singular 'address', also inheriting
	 * its protocol.
	 * @param address Reference to IPAddress.
	 */
	void FromAddress(const IPAddress &address);

	/**
	 * Set an IPRange with the given bounds and protocol.
	 * @param from_address Reference to the starting IPAddress.
	 * @param to_address Reference to the ending IPAddress.
	 * @return If 'to_address' is below 'from_address', treat this as an
	 * invalid range and return OpStatus::ERR. If the addresses differ in
	 * protocol, also return OpStatus::ERR; otherwise OpStatus::OK.
	 */
	OP_STATUS WithRange(const IPAddress &from_address, const IPAddress &to_address);

	/** Check if an address belongs to the range, but with the address
	 * having to be of same type as the range (IPv4 or IPv6.)
	 * @param address Reference to IPAddress.
	 * @param [out]result comparison result:
	 *	-1 if a is below the range, +1 if above, 0 if within.
	 * @return OpStatus::OK if successful, OpStatus::ERR for other errors.
	 */
	OP_STATUS Compare(const IPAddress &address, int &result) const;

	/** Check if an IPv4 address belongs to the range.
	 * @param address the IPv4Address.
	 * @return the comparison result.
	 *	-1 if a is below the range, +1 if above, 0 if within.
	 */
	int Compare(IPv4Address address) const;

	/** Check if an IPv6 address belongs to the range.
	 * @param address Reference to IPv6Address.
	 * @return the comparison result.
	 *	-1 if a is below the range, +1 if above, 0 if within.
	 */
	int Compare(const IPv6Address &address) const;

	/** Parses an IP range, possibly singular, using the following grammar:
	 *    Range: Address
	 *         | Address '-' Address
	 *
	 * where 'Address' is either an IPv4 address or IPv6. A range's endpoints
	 * must use the same protocol.
	 * @param address Input string.
	 * @return OpStatus::OK if successful, OpStatus::ERR if not
	 * syntactically well-formed.
	 */
	OP_STATUS Parse(const uni_char *address);

	/** Export the bounds of an IP range to text format. Lower and upper bounds exported as,
	 * IPv4: %d.%d.%d.%d
	 * IPv6: %x:%x:%x:%x:%x:%x:%x:%x
	 * @param range Reference to IPRange that contains IPv4 or IPv6 address.
	 * @param[out] from The exported starting address.
	 * @param[out] to The exported ending address.
	 * @param for_user If TRUE, use protocol-specific rules to shorten and make the
	 * text representation friendlier for the casual reader. Only applies to IPv6,
	 * causing a sequence of zero components to be contracted to "::".
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS Export(const IPRange &range, OpString &from, OpString &to, BOOL for_user = FALSE);

	/** Export IP range to text format; addresses formatted as for 'Export(range, from, to)'
	 * but as a single string, lower and upper bounds separated by "-".
	 * @param range Reference to IPRange that contains IPv4 or IPv6 address.
	 * @param[out] text The text representation of the IP range, '-' used
	 * as separator between the 'start' and 'end' addresses.
	 * @param for_user If TRUE, use address-specific rules to shorten and make the
	 * text representation friendlier for the casual reader.
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS Export(const IPRange &range, OpString &text, BOOL for_user = FALSE);

private:
	/** Internal method for filling in the bounds of an IPRange.
	 */
	void SetRange(const IPAddress &from_address, const IPAddress &to_address);
};

/**
 * This class provides helper functions for network address manipulation.
 * Its members are using it for security policy parsing and checking.
 * It supports IPv4 and IPv6 and is one of base classes of OpSecurityManager.
 */
class OpSecurityUtilities
{
#ifdef PUBLIC_DOMAIN_LIST
	PublicDomainDatabase* m_public_domain_list;
#endif // PUBLIC_DOMAIN_LIST

public:

	/** Constructor */
	OpSecurityUtilities();

	/** Destructor */
	~OpSecurityUtilities();

	/** Returns TRUE if supplied string is an IP address or range. */
	static BOOL IsIPAddressOrRange(const uni_char *str);

	/** Parses the non-empty port range: %d-%d, the higher bound must be at least equal to the lower.
	 * @param range Input string.
	 * @param low Pointer to integer value that contains lower port value after parsing.
	 * @param high Pointer to integer value that contains high port value after parsing.
	 * @return OpStatus::OK if successful, OpStatus::ERR if an invalid range or other errors.
	 */
	static OP_STATUS ParsePortRange(const uni_char *range, unsigned int &low, unsigned int &high);

	/** Extracts IP address from socket. Formats supported: %d.%d.%d.%d (IPv4)
	 *	and %x:%x:%x:%x:%x:%x:%x:%x (IPv6, including zero component contraction).
	 * @param sa Pointer to a valid socket address. NULL not allowed.
	 * @param addr Pointer to IPAddress structure that represents input socket address. NULL not allowed.
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM; OpStatus::ERR for other errors.
	 */
	static OP_STATUS ExtractIPAddress(OpSocketAddress *sa, IPAddress &addr);

	/** Resolves URL.
	 * @param url Pointer to url that contains name that must be resolved.
	 * @param[out]addr The resolved IP address.
	 * @param state State of name lookup operation.
	 * @return OpStatus::OK if successful, OpStatus::ERR for other errors.
	 */
	static OP_STATUS ResolveURL(const URL &url, IPAddress &addr, OpSecurityState &state);

	/** Checks if URL may be exported.
	 * @param url Reference to URL that will be checked.
	 */
	static BOOL IsSafeToExport(const URL &url);

	/** Checks is a domain looks like a public domain. Is not
	 * 100% accurate.
	 *
	 * @param[in] domain A null terminated domain name (not NULL), with or without trailing dot.
	 *
	 * @param[out] is_public_domain If the method returns OpStatus::OK then this
	 * is an indication if the domain is a public domain (like co.uk or com)
	 *
	 * @param[out] top_domain_was_known_to_exist If an address to a BOOL is sent in here, that BOOL
	 * will be set to TRUE if the top domain was mentioned in the database, FALSE otherwise.
	 *
	 * @returns OpStatus::OK if the check was done or an error code otherwise.
	 */
	OP_STATUS IsPublicDomain(const uni_char *domain, BOOL &is_public_domain, BOOL *top_domain_was_known_to_exist = NULL);
};

#endif // SECURITY_UTILITIES_H
