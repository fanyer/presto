/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/MacTLS.h"

MacTLS MacTLS::s_null_instance;
pthread_key_t MacTLS::s_key;

int MacTLS::Init()
{
	int res = pthread_key_create(&s_key, &DetachThread);
	if (res != 0)
		return res;

	return AttachThread();
}

int MacTLS::AttachThread()
{
	MacTLS* tls = OP_NEW(MacTLS, ());
	RETURN_OOM_IF_NULL(tls);

	return pthread_setspecific(s_key, tls);
}

void MacTLS::Destroy()
{
	DetachThread(Get());
	pthread_key_delete(s_key);
}

void MacTLS::DetachThread(void* data)
{
	pthread_setspecific(s_key, NULL);
	OP_DELETE(static_cast<MacTLS*>(data));
}
