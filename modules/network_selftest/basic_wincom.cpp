/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#include "core/pch.h"

#ifdef SELFTEST

#include "modules/selftest/src/testutils.h"
#include "modules/network_selftest/basicwincom.h"

BasicWindowListener::~BasicWindowListener(){fallback=NULL;}

void BasicWindowListener::OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	fallback->OnAuthenticationRequired(commander, callback);
}

void BasicWindowListener::OnUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	fallback->OnUrlChanged(commander, url);
}

void BasicWindowListener::OnStartLoading(OpWindowCommander* commander)
{
	fallback->OnStartLoading(commander);
}

void BasicWindowListener::OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info)
{
	fallback->OnLoadingProgress(commander,info);
}

void BasicWindowListener::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	fallback->OnLoadingFinished(commander,status);
}
void BasicWindowListener::OnStartUploading(OpWindowCommander* commander)
{
	fallback->OnStartUploading(commander);
}
void BasicWindowListener::OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	fallback->OnUploadingFinished(commander,status);
}
BOOL BasicWindowListener::OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url)
{
	return fallback->OnLoadingFailed(commander,msg_id,url);
}

#ifdef EMBROWSER_SUPPORT
void BasicWindowListener::OnUndisplay(OpWindowCommander* commander)
{fallback->OnUndisplay(commander);
}

void BasicWindowListener::OnLoadingCreated(OpWindowCommander* commander)
{fallback->OnLoadingCreated(commander);
}
#endif // EMBROWSER_SUPPORT

void BasicWindowListener::ReportFailure(URL &url, const char *format, ...)
{
	OpString8 tempstring;
	va_list args;
	va_start(args, format);

	if(format == NULL)
		format = "";

	OP_STATUS op_err = url.GetAttribute(URL::KName_Escaped, tempstring);
	if(OpStatus::IsSuccess(op_err))
		op_err = tempstring.Append(" :");

	if(OpStatus::IsSuccess(op_err))
		tempstring.AppendVFormat(format, args);

	if(test_manager)
		test_manager->ReportTheFailure(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);
	else
		ST_failed(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);

	va_end(args);
}

#endif // SELFTEST
