/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_DATASTREAM_DATASTREAM_MODULE_H
#define MODULES_DATASTREAM_DATASTREAM_MODULE_H

enum DataStream_ByteArray_CleansePolicy_values {
	DataStream_Sensitive_Array,
	DataStream_NonSensitive_Array
};

class DatastreamModule : public OperaModule
{
public:
	DataStream_ByteArray_CleansePolicy_values	byte_array_cleanse_policy;

public:
	DatastreamModule() :
		byte_array_cleanse_policy(DataStream_Sensitive_Array)
	{
	}
	virtual void InitL(const OperaInitInfo& info){};

	virtual void Destroy(){};
	virtual ~DatastreamModule(){}
};		

#define g_ByteArray_Cleanse_Policy g_opera->datastream_module.byte_array_cleanse_policy

#define DATASTREAM_MODULE_REQUIRED

#endif // !MODULES_DATASTREAM_DATASTREAM_MODULE_H
