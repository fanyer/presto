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

#ifndef _FL_LIB_H_
#define _FL_LIB_H_

#ifdef _DEBUG
#ifdef YNP_WORK
//#define _DATASTREAM_DEBUG_
#endif
#endif

#include "modules/datastream/fl_base.h" 
#include "modules/datastream/fl_list.h"
#include "modules/datastream/fl_temp.h"
#include "modules/datastream/fl_file.h"
#include "modules/datastream/fl_rec.h"
#include "modules/datastream/fl_grec.h"
#include "modules/datastream/fl_debug.h"

#if defined(DATASTREAM_BYTEARRAY_ENABLED) && defined(DATASTREAM_SEQUENCE_ENABLED)
typedef DataStream_GenericRecord<DataStream_ByteArray> DataStream_GenericRecord_Large;
typedef DataStream_GenericRecord<DataStream_ByteArray_Base> DataStream_GenericRecord_Small;
#endif // DATASTREAM_SEQUENCE_ENABLED, DATASTREAM_BYTEARRAY_ENABLED

#endif
