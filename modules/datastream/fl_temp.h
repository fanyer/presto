/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef __FL_TEMP_H__
#define __FL_TEMP_H__

// records stored without Tags and payload length
// Bigendian storage

class DataStream_BaseRecord;
class DataStream_ByteArray;

#include "modules/datastream/fl_ref.h"
#include "modules/datastream/fl_int.h"
#include "modules/datastream/fl_tint.h"
#include "modules/datastream/fl_bytes.h"
#include "modules/datastream/fl_buffer.h"

#ifdef DATASTREAM_UINT_ENABLED
typedef DataStream_Reference<DataStream_UIntBase> DataStream_UIntReference;

#ifdef DATASTREAM_USE_INTEGRALTYPE_TEMPLATE
typedef DataStream_IntegralType<uint8, 1> DataStream_UInt8;
typedef DataStream_IntegralType<uint8, 1> DataStream_Octet;
typedef DataStream_IntegralType<uint16, 2> DataStream_UInt16;
typedef DataStream_IntegralType<uint32, 4> DataStream_UInt32;
#endif // DATASTREAM_USE_INTEGRALTYPE_TEMPLATE

#endif // DATASTREAM_UINT_ENABLED

#endif // __FL_TEMP_H__
