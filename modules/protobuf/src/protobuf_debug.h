/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_PROTOBUF_DEBUG_H
#define OP_PROTOBUF_DEBUG_H

#ifdef PROTOBUF_SUPPORT

// Enable the next line to get fine-grained debugging output for the protobuf process
//#define DEBUG_PROTOBUF
// Enable the next line to place the current pid in the debug output, useful for multi-process debugging
//#define DEBUG_PROTOBUF_PROC

#if defined(DEBUG_PROTOBUF) && defined(DEBUG_ENABLE_PRINTF)

#define PROTOBUF_DEBUG_SUPPORT

#if defined(DEBUG_PROTOBUF_PROC) && defined(OPSYSTEMINFO_GETPID)
#define PROTOBUF_DEBUG_PROC() \
	do { \
	dbg_printf("[%d] ", (int) g_op_system_info->GetCurrentProcessId()); \
	} while(0)
#else // DEBUG_PROTOBUF_PROC
#define PROTOBUF_DEBUG_PROC() do { } while(0)
#endif // DEBUG_PROTOBUF_PROC

#define PROTOBUF_DEBUG(prefix, message) \
	do { \
	PROTOBUF_DEBUG_PROC(); \
	dbg_printf("%s: ", prefix); \
	dbg_printf message; \
	dbg_printf("\n"); \
	} while(0)
void PROTOBUF_DUMP(const char* data, size_t bytes);

#else // DEBUG_PROTOBUF

#define PROTOBUF_DEBUG_PROC() do { } while(0)
#define PROTOBUF_DEBUG(prefix, message) do { } while(0)
#define PROTOBUF_DUMP(data, bytes) do { } while(0)

#endif // DEBUG_PROTOBUF

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_DEBUG_H
