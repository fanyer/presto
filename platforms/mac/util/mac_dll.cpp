/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include <string.h>

void* LoadLibrary(char *lpLibFileName)
{
	CFragConnectionID	libraryID;
	Ptr					baseAddr;
	Str255				errMessage;
	OSErr				theErr = noErr;
	Str63				libName;
	short				stringSize;

	stringSize = strlen((const char *)lpLibFileName);
	if (stringSize > 63)
	{
		stringSize = 63;
	}
	libName[0] = stringSize;
	memcpy(&libName[1], (char*) lpLibFileName, stringSize);

	theErr = GetSharedLibrary(libName, kPowerPCCFragArch, kLoadCFrag, &libraryID, &baseAddr, errMessage);
	if (noErr == theErr)
	{
		return((long*) libraryID);
	}
	else if (-2804 == theErr)
	{
		theErr = GetSharedLibrary(libName, kMotorola68KCFragArch, kLoadCFrag, &libraryID, &baseAddr, errMessage);
		if (noErr == theErr)
		{
			return((long*) libraryID);
		}
	}
	return(nil);
}

void* GetProcAddress(void* hModule, char* lpProcName)
{
	Str63				symbolName;
	Ptr					symbolAddr;
	CFragSymbolClass	symbolClass;
	OSErr				theErr = noErr;

	short				stringSize = strlen((const char *)lpProcName);
	if (stringSize > 63)
	{
		stringSize = 63;
	}
	symbolName[0] = stringSize;
	memcpy(&symbolName[1], lpProcName, stringSize);

	theErr = FindSymbol((CFragConnectionID) hModule, symbolName, &symbolAddr, &symbolClass);
	if (theErr == noErr)
	{
		return((long*) symbolAddr);
	}
	return(nil);
}

long FreeLibrary(void* hLibModule)
{
	OSErr	theErr = noErr;
	theErr = CloseConnection((CFragConnectionID *) &hLibModule);
	if (theErr == noErr)
	{
		return(true);
	}
	return(false);
}

