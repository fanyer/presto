/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_PROTOBUF_MODULE_H
#define MODULES_PROTOBUF_MODULE_H

#ifdef PROTOBUF_SUPPORT

#include "modules/hardcore/opera/module.h"

class ProtobufModule : public OperaModule
{
public:
	ProtobufModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	virtual BOOL FreeCachedData(BOOL toplevel_context);
};

#define PROTOBUF_MODULE_REQUIRED

#ifdef NO_CORE_COMPONENTS
# ifdef PROTOBUF_ECMASCRIPT_SUPPORT
#  undef PROTOBUF_ECMASCRIPT_SUPPORT
# endif // PROTOBUF_ECMASCRIPT_SUPPORT
# ifdef PROTOBUF_JSON_SUPPORT
#  undef PROTOBUF_JSON_SUPPORT
# endif // PROTOBUF_JSON_SUPPORT
# ifdef PROTOBUF_XML_SUPPORT
#  undef PROTOBUF_XML_SUPPORT
# endif // PROTOBUF_XML_SUPPORT
#endif // NO_CORE_COMPONENTS

#endif // PROTOBUF_SUPPORT

#endif // !MODULES_PROTOBUF_MODULE_H
