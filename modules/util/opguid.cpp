/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"
#ifdef UTIL_GUID_GENERATE_SUPPORT
#include "modules/util/opguid.h"
#include "modules/pi/OpSystemInfo.h"

#ifndef CRYPTO_API_SUPPORT
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslrand.h"
#else
#include "modules/libcrypto/include/OpRandomGenerator.h"
#endif

#ifndef GUID_GENERATE_SUPPORT
/**
 * Overlap the structure of a UUID as defined per RFC 4122 with the
 * simple array type defined in modules/hardcore/base/op_types.h
 */
struct OpUUIDStruct
{
	UINT32 time_low;		///< The low field of the timestamp
	UINT16 time_mid;		///< The middle field of the timestamp
	UINT16 time_hi_and_version;
							///< The high field of the timestamp
							///< multiplexed with the version number
	UINT8 clock_seq_hi_and_reserved;
							///< The high field of the clock sequence
							///< multiplexed with the variant
	UINT8 clock_seq_low;	///< The low field of the clock sequence
	UINT8 node[6];			///< The spatially unique node identifier
};

OP_STATUS OpGUIDManager::GenerateGuid(OpGuid &guid)
{
	// UUID generator for platforms which do not provide one in their
	// porting interfaces.

	// Use the run-time value as the clock sequence number, and remember
	// it for this session.
	if (!clocksequence)
	{
		clocksequence = g_op_time_info->GetRuntimeMS();
	}
	else
	{
		++ clocksequence;
	}

	// Get the current time as milliseconds since 1970-01-01 midnight UTC
	double milliseconds = g_op_time_info->GetTimeUTC();

	// Adjust to UUID time: 100-nanosecond intervals since since
	// 00:00:00.00, 15 October 1582.
#ifdef HAVE_UINT64
	UINT64 timestamp =
		static_cast<UINT64>(milliseconds) * 10000 + (((UINT64)0x01B21DD2L) << 32) + 0x13814000L;
#else
	UINT32 timestamp_low_unadjusted =
		op_double2int32(milliseconds) * 10000;
	UINT32 timestamp_high =
		static_cast<UINT32>(milliseconds / 429496.72960);

	UINT32 timestamp_low = timestamp_low_unadjusted + 0x13814000;
	if (timestamp_low < timestamp_low_unadjusted)
		++ timestamp_high; // carry
	timestamp_high += 0x01B21DD2;
#endif

#ifdef VALGRIND
	op_valgrind_set_defined(guid, sizeof(OpGuid));
#endif

	// Use the overlain struct UUID so that we can name the fields
	register OpUUIDStruct *uuid_p = reinterpret_cast<OpUUIDStruct *>(&guid);

	// time_low should be set to the 32 least significants bits of the
	// time stamp.
#ifdef HAVE_UINT64
	uuid_p->time_low = op_htonl((UINT32)(timestamp & 0xFFFFFFFFL));
#else
	uuid_p->time_low = op_htonl(timestamp_low);
#endif

	// time_mid should be set to bits 32-47.
#ifdef HAVE_UINT64
	uuid_p->time_mid = op_htons((UINT16)((timestamp >> 32) & 0xFFFF));
#else
	uuid_p->time_mid = op_htons(timestamp_high & 0xFFFF);
#endif

	// time_hi_and_version should receive bits 48-59 of the timestamp,
	// and have the version number (1) in the high four bits.
	uuid_p->time_hi_and_version =
#ifdef HAVE_UINT64
		op_htons((UINT16)(((timestamp >> 48) & 0x0FFF) | (1 << 12)));
#else
		op_htons(((timestamp_high >> 16) & 0x0FFF) | (1 << 12));
#endif

	// clock_seq_low should have the 8 least significant bits of the
	// clock sequence.
	uuid_p->clock_seq_low = static_cast<UINT32>(clocksequence) & 0xFF;

	// clock_seq_hi_and_reserved should have the siz next bits,
	// and bits 6 and 7 set to zero and one.
	uuid_p->clock_seq_hi_and_reserved =
		(static_cast<UINT32>(clocksequence) >> 8) & 0x3F | 0x80;

	// Use a random value instead of the 48-bit MAC address in the node
	// field, with the high-bit set as recommended in RFC 4122.
	SSL_RND(uuid_p->node, sizeof uuid_p->node);
	uuid_p->node[0] |= 0x80;

	return OpStatus::OK;
}
#endif // !GUID_GENERATE_SUPPORT

/* static */
void OpGUIDManager::ToString(const OpGuid &guid, char *buf, size_t buflen)
{
	OP_ASSERT(buflen >= 37 || !"String GUID will be truncated");

	register const UINT8 *guidarray =
		reinterpret_cast<const UINT8 *>(&guid);

	op_snprintf(buf, buflen, 
		"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		guidarray[0], guidarray[1], guidarray[2], guidarray[3],
		guidarray[4], guidarray[5],
		guidarray[6], guidarray[7],
		guidarray[8], guidarray[9],
		guidarray[10], guidarray[11], guidarray[12],
		guidarray[13], guidarray[14], guidarray[15]);
}

#endif // UTIL_GUID_GENERATE_SUPPORT
