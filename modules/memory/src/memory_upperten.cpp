/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement OpMemUpperTen object
 * 
 * \author Morten Rolland, mortenro@opera.com
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_upperten.h"

OpMemUpperTen::OpMemUpperTen(int size) :
	size(size)
{
	ptr1 = new UINTPTR[size];
	arg2 = new size_t[size];
	arg3 = new int[size];

	if ( ptr1 != 0 && arg2 != 0 && arg3 != 0 )
	{
		for ( int k = 0; k < size; k++ )
		{
			ptr1[k] = 0;
			arg2[k] = 0;
			arg3[k] = 0;
		}

		total_count = 0;
		total_bytes = 0;
		valid = TRUE;
	}
	else
	{
		valid = FALSE;
	}
}

OpMemUpperTen::~OpMemUpperTen(void)
{
	delete[] ptr1;
	delete[] arg2;
	delete[] arg3;
}

void OpMemUpperTen::Add(UINTPTR p, size_t a2, int a3)
{
	if ( !valid )
		return;

	if ( a2 > 0 )
	{
		total_count++;
		total_bytes += a2;
	}

	if ( arg2[size - 1] < a2 )
	{
		// This one qualifies for bottom position, at least
		ptr1[size - 1] = p;
		arg2[size - 1] = a2;
		arg3[size - 1] = a3;

		for ( int k = size - 2; k >= 0; k-- )
		{
			if ( arg2[k+1] > arg2[k] )
			{
				// move up
				p = ptr1[k];
				a2 = arg2[k];
				a3 = arg3[k];

				ptr1[k] = ptr1[k+1];
				arg2[k] = arg2[k+1];
				arg3[k] = arg3[k+1];

				ptr1[k+1] = p;
				arg2[k+1] = a2;
				arg3[k+1] = a3;
				continue;
			}
			break;
		}
	}
}

void OpMemUpperTen::Show(const char* title, const char* format)
{
	if ( !valid )
		return;

	int listed_bytes = 0;
	int listed_count = 0;
	dbg_printf("=========== %s ============\n", title);
	for ( int k = 0; k < size; k++ )
	{
		if ( arg2[k] == 0 )
			break;
		dbg_printf("    ");
		dbg_printf(format, ptr1[k], arg2[k], arg3[k]);
		dbg_printf("\n");
		listed_bytes += arg2[k];
		listed_count++;
	}
	dbg_printf("Listed:          %d bytes in %d entries\n",
			   listed_bytes, listed_count);
	dbg_printf("Not listed:      %d bytes in %d entries\n",
			   total_bytes - listed_bytes, total_count - listed_count);
	dbg_printf("Total:           %d bytes in %d entries\n",
			   total_bytes, total_count);
}

#endif // ENABLE_MEMORY_DEBUGGING
