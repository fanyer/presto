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

#include "core/pch.h"

#include "modules/datastream/fl_lib.h"

#ifdef _DATASTREAM_DEBUG_

#include "modules/olddebug/tstdump.h"

const char *DataStream::Debug_ClassName()
{
	return "DataStream";
}

const char *DataStream::Debug_OutputFilename()
{
	return "stream.txt";
}

void DataStream::Debug_Write_All_Data()
{
	DS_DEBUG_Start();

	DS_Debug_Printf1("%s: All Data written\n", Debug_ClassName());
}

void DataStream::DS_Debug_Write(const char *function_name, const char *Description, const unsigned char *source, unsigned int len)
{
	if(DS_debug_level>1)
		PrintfTofile(Debug_OutputFilename(), "%*s", (DS_debug_level-1)*4," ");
	PrintfTofile(Debug_OutputFilename(), "Class %s : Function %s\n", (Debug_ClassName() ? Debug_ClassName() : "?"), 
		(function_name ? function_name : ":"));

	DumpTofile(source, len, (Description ? Description : ""),  Debug_OutputFilename(), DS_debug_level-1);
}

void DataStream::DS_Debug_Printf(const char *format, ...)
{
	va_list marker;
	
	va_start(marker,format);
	if(DS_debug_level>1)
		PrintfTofile(Debug_OutputFilename(), "%*s", (DS_debug_level-1)*4," ");
	
	PrintfTofile(Debug_OutputFilename(), "Class %s : ", (Debug_ClassName() ? Debug_ClassName() : "?"));
	PrintvfTofile(Debug_OutputFilename(), format,marker); 
	va_end(marker);

}

int DataStream::DS_debug_level = 0;

DataStream_Debug_Counter::DataStream_Debug_Counter()
{
	DataStream::DS_debug_level++;
}

DataStream_Debug_Counter::~DataStream_Debug_Counter()
{
	if(DataStream::DS_debug_level>0)
		DataStream::DS_debug_level--;
}
#endif
