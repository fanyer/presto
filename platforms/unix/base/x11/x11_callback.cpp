/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_callback.h"

#include "modules/hardcore/mh/mh.h"

X11CbObject::~X11CbObject()
{
	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS X11CbObject::PostCallback(INTPTR data, unsigned long delay)
{
	const MH_PARAM_1 par1 = reinterpret_cast<MH_PARAM_1>(this);

	if (!g_main_message_handler->HasCallBack(this, MSG_X11_CALLBACK, par1))
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_X11_CALLBACK, par1));

	if (!g_main_message_handler->PostMessage(MSG_X11_CALLBACK, par1, data, delay))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void X11CbObject::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_X11_CALLBACK || par1 != reinterpret_cast<MH_PARAM_1>(this))
		return;

	HandleCallBack(par2);
}
