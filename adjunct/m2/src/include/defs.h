// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef DEFS_H
#define DEFS_H

#ifdef M2_SUPPORT

class MessageEngine;

#define CURRENT_M2_ACCOUNT_VERSION 9

#include "adjunct/m2/src/include/enums.h"

#ifdef _DEBUG
# define OP_ASSERT_SUCCESS(ret) OP_ASSERT(OpStatus::IsSuccess(ret))
#else
# define OP_ASSERT_SUCCESS(ret) OpStatus::Ignore(ret)
#endif

typedef UINT32 index_gid_t;
typedef UINT32 message_gid_t;
typedef UINT32 message_index_t;

class Message;

// These macros shouldn't be used - they were used in a transitional period between
// Store2 and Store3. Instead, always use a local variable to refer to a message:
//    Message message;
//    if (OpStatus::IsSuccess(g_m2_engine->GetMessage(message, message_id)))
//    { /* ... do something ... */ }
DEPRECATED(Message* DeprecatedWayToDeclareMessage(Message& message));
inline Message* DeprecatedWayToDeclareMessage(Message& message) { return &message; }
DEPRECATED(Message& DeprecatedWayToGetMessageReference(Message* message));
inline Message& DeprecatedWayToGetMessageReference(Message* message) { return *message; }
# define MESSAGEDEF(variable) \
		Message tmpmsg; \
		Message* variable = DeprecatedWayToDeclareMessage(tmpmsg);
# define MESSAGEPAR(variable) DeprecatedWayToGetMessageReference(variable)
# define MESSAGEADDPAR(variable) DeprecatedWayToGetMessageReference(variable)

#endif //M2_SUPPORT

#endif // DEFS_H
