/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _FL_DEBUG_H_
#define _FL_DEBUG_H_

#ifdef _DATASTREAM_DEBUG_
class DataStream_Debug_Counter : public CleanupItem
{
public:
	DataStream_Debug_Counter();
	~DataStream_Debug_Counter();
};

#define DS_DEBUG_Start() DataStream_Debug_Counter ds_debug_counter;

#define DS_LEAVE(err) \
	do{\
		OP_ASSERT(0);\
		LEAVE(err);\
	}while(0)

#else
#define DS_DEBUG_Start()
#define DS_LEAVE(err) LEAVE(err)
#endif

#define DS_LEAVE_IF_ERROR( expr )                                                \
   do { OP_STATUS LEAVE_IF_ERROR_TMP = expr;                                  \
        if (OpStatus::IsError(LEAVE_IF_ERROR_TMP))							\
		{																		\
		DS_LEAVE(LEAVE_IF_ERROR_TMP);												\
		}																	\
   } while(0)		

#endif // _FL_DEBUG_H_
