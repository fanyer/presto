/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

ProtobufModule::ProtobufModule()
{
}

void
ProtobufModule::InitL(const OperaInitInfo&)
{
}

void
ProtobufModule::Destroy()
{
}

BOOL
ProtobufModule::FreeCachedData(BOOL toplevel_context)
{
	(void)toplevel_context;
	return FALSE;
}

#endif // PROTOBUF_SUPPORT
