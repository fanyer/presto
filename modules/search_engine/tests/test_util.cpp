/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "modules/pi/system/OpLowLevelFile.h"
#include "modules/pi/OpSystemInfo.h"

#define RETURN_IF_ERROR_CLEANUP( expr ) do { if (OpStatus::IsError(rv = expr)) goto cleanup; } while (0);

OP_STATUS delete_file(const uni_char *name)
{
	OpLowLevelFile *d = NULL;
	OP_STATUS rv;

	RETURN_IF_ERROR_CLEANUP(OpLowLevelFile::Create(&d, name));

	rv = d->Delete();

cleanup:
	OP_DELETE(d);

	return rv;
}

OP_STATUS copy_file(const uni_char *src, const uni_char *dst)
{
	OpLowLevelFile *s = NULL, *d = NULL;
	OP_STATUS rv;

	RETURN_IF_ERROR_CLEANUP(OpLowLevelFile::Create(&s, src));
	RETURN_IF_ERROR_CLEANUP(OpLowLevelFile::Create(&d, dst));

	rv = d->CopyContents(s);

cleanup:
	OP_DELETE(d);
	OP_DELETE(s);

	return rv;
}

void op_msleep(unsigned msec)
{
	double endt = g_op_time_info->GetWallClockMS() + msec;

	while (g_op_time_info->GetWallClockMS() < endt)
		;
}

#undef RETURN_IF_ERROR_CLEANUP

#endif
