/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef __SYNC_UTIL_H__
#define __SYNC_UTIL_H__

#ifdef SUPPORT_DATA_SYNC

// Copied from m2
template<typename T> class SyncResetter
{
public:
	SyncResetter(T& reset, T reset_to) : m_reset(reset), m_reset_to(reset_to) {}
	~SyncResetter() { m_reset = m_reset_to; }
private:
	T& m_reset;
	T m_reset_to;
};

// Sync Defines
#define SYNC_RETURN_IF_ERROR( expr, x )									\
	do { OP_STATUS SYNC_RETURN_IF_ERROR_TMP = expr;						\
        if (OpStatus::IsError(SYNC_RETURN_IF_ERROR_TMP))				\
		{																\
			OP_DELETE(x);												\
			return SYNC_RETURN_IF_ERROR_TMP;							\
		}																\
	} while(0)

class SyncUtil
{
public:
	static OP_STATUS GenerateStringGUID(OpString& outstr);
	static OP_STATUS CreateRFC3339Date(time_t time, OpString& out_date);
	static OP_STATUS ParseRFC3339Date(time_t& time, const OpStringC& in_date);
	static OP_STATUS SyncDateToString(double date, OpString& outstr);
	static OP_STATUS DateToNormalizedString(double date, OpString& outstr);
#ifdef IMG_BITMAP_TO_BASE64PNG
	DEPRECATED(static OP_STATUS GetPNGAsBase64(OpBitmap* bmp, OpString16& base64str));
#endif // IMG_BITMAP_TO_BASE64PNG
};

#endif // SUPPORT_DATA_SYNC

#endif // __SYNC_UTIL_H__
