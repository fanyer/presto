/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "modules/sync/sync_util.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opguid.h"

/***********************************************************************************
**
**  GenerateStringGUID - Generate a GUID for use with Link IDs
**
***********************************************************************************/
OP_STATUS SyncUtil::GenerateStringGUID(OpString& outstr)
{
	OpGuid guid;
	/* for compatibility reason with Opera Link, the generated UUID MUST be
	 * upper case! */
	const char* hex = "0123456789ABCDEF";	// DO NOT CHANGE, SEE ABOVE!
	UINT8* hash = NULL;
	OpString8 strHash;
	char* pszHash = strHash.Reserve((sizeof(OpGuid) * 2) + 1);
	if(!pszHash)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));
	hash = (UINT8*)&guid;

	for (size_t nByte = 0 ; nByte < sizeof(OpGuid) ; nByte++, hash++)
	{
		*pszHash++ = hex[ *hash >> 4 ];
		*pszHash++ = hex[ *hash & 15 ];
	}
	*pszHash = '\0';

	RETURN_IF_ERROR(outstr.Set(strHash));
	return OpStatus::OK;
}

/***************************************************************************
 **
 ** CreateRFC3339Date
 **
 **************************************************************************/
OP_STATUS SyncUtil::CreateRFC3339Date(time_t time, OpString& out_date)
{
	struct tm* date = op_gmtime(&time);
	if (!date)
		return OpStatus::ERR_OUT_OF_RANGE;

 	OpString s;
	if (!s.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	g_oplocale->op_strftime(s.CStr(), s.Capacity(), UNI_L("%Y-%m-%dT%H:%M:%SZ"), date);
	return out_date.Set(s);
}

/***************************************************************************
 **
 ** ParseRFC3339Date
 **
 **************************************************************************/
OP_STATUS SyncUtil::ParseRFC3339Date(time_t& time, const OpStringC& in_date)
{
	double d = OpDate::ParseRFC3339Date(in_date.CStr());
	if (op_isnan(d))
		return OpStatus::ERR;

	time = static_cast<time_t>(d / 1000);
	return OpStatus::OK;
}

OP_STATUS SyncUtil::SyncDateToString(double date, OpString& outstr)
{
	struct tm _tm;
	OpString s;
	if (!s.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;
	op_memset(&_tm, 0, sizeof(struct tm));

	date = OpDate::LocalTime(date);

	_tm.tm_year = (INT32)OpDate::YearFromTime(date) - 1900;
	_tm.tm_mon  = (INT32)OpDate::MonthFromTime(date);
	_tm.tm_mday = (INT32)OpDate::DateFromTime(date); // 'T'
	_tm.tm_hour = (INT32)OpDate::HourFromTime(date); // ':'
	_tm.tm_min  = (INT32)OpDate::MinFromTime(date);  // ':'
	_tm.tm_sec  = (INT32)OpDate::SecFromTime(date);  // 'Z'
	_tm.tm_yday = (INT32)OpDate::DayWithinYear(date);

	g_oplocale->op_strftime(s.CStr(), s.Capacity(), UNI_L("%Y%m%dT%H%M%S"), &_tm);

	return outstr.Set(s.CStr());
}

/***************************************************************************
 **
 ** DateToNormalizedString
 **
 **************************************************************************/
OP_STATUS SyncUtil::DateToNormalizedString(double date, OpString& outstr)
{
	struct tm _tm;
	OpString s;

	if (!s.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	_tm.tm_hour = (int)OpDate::HourFromTime(date);
	_tm.tm_min  = (int)OpDate::MinFromTime(date);
	_tm.tm_sec  = (int)OpDate::SecFromTime(date);
	_tm.tm_mday = (int)OpDate::DateFromTime(date);
	_tm.tm_year = (int)OpDate::YearFromTime(date) - 1900;
	_tm.tm_mon  = (int)OpDate::MonthFromTime(date);
	_tm.tm_yday = (INT32)OpDate::DayWithinYear(date);

	g_oplocale->op_strftime(s.CStr(), s.Capacity(), UNI_L("%x %H:%M:%S"), &_tm);

	return outstr.Set(s);
}

/***************************************************************************
**
** GetPNGAsBase64
**
** this function is deprecated. please use DumpOpBitmapToPNG in
** modules/img/imagedump.h instead.
**
**************************************************************************/
#ifdef IMG_BITMAP_TO_BASE64PNG
# include "modules/img/imagedump.h"
OP_STATUS SyncUtil::GetPNGAsBase64(OpBitmap* bmp, OpString16& base64str)
{
	TempBuffer buf;
	RETURN_IF_ERROR(GetOpBitmapAsBase64PNG(bmp, &buf));
	return base64str.Set(buf.GetStorage(), buf.Length());
}
#endif // IMG_BITMAP_TO_BASE64PNG

#endif // SUPPORT_DATA_SYNC
