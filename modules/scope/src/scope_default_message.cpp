/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_default_message.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/scope/src/scope_tp_message.h"
#include "modules/scope/src/scope_manager.h"

/*static*/
const OpProtobufMessage *
OpScopeDefaultMessage::GetMessageDescriptor()
{
	return g_scope_manager->GetDefaultMessageDescriptor();
}


#endif // SCOPE_SUPPORT
