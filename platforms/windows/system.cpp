/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2001-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

int mswin_snprintf( char *buffer, size_t count, const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int length = mswin_vsnprintf(buffer, count, format, arguments);
	va_end(arguments);
	return length;
}

//In case buffer is too small, op_vsnprintf must (according to ANSI C) return how large the buffer needs to be (not including \0)
int mswin_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	int length = -1;
	if (buffer && count>0)
	{
		length = _vsnprintf(buffer, count, format, argptr);
		buffer[count-1] = 0;
		if (length != -1)
			return length;
	}

	//We don't know the size, or it was too small
	size_t temp_count = count>0 ? count*2 : 100;
	while (TRUE)
	{
		char* temp_buffer = new char[temp_count];
		if (!temp_buffer)
			return -2; //Any negative value signals an error

		length = _vsnprintf(temp_buffer, temp_count, format, argptr);
		delete[] temp_buffer;
		
		if (length != -1)
			break;

		temp_count *= 2;
	}

	return length;
}
