/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include <stdlib.h>
#include <sys/mman.h>
#include "platforms/mac/MachOGlue/MachOCFMGlue.h"

UInt32 gGlueTemplate[6] = { 0x3D800000, 0x618C0000, 0x800C0000,
							0x804C0004,	0x7C0903A6, 0x4E800420	};

void* CFMFunctionPointerForMachOFunctionPointer(void* inMachProcPtr)
{
    TVector_rec		*vTVector;

    vTVector = (TVector_rec*)malloc(sizeof(TVector_rec));

    if((MemError() == noErr) && (vTVector != NULL))
	{
        vTVector->fProcPtr = (ProcPtr)inMachProcPtr;
        vTVector->fTOC = 0;  // ignored
    }

	return((void *)vTVector);
}

void DisposeCFMFunctionPointer(void* inCfmProcPtr)
{
	if(inCfmProcPtr)
		free(inCfmProcPtr);
}

void* MachOFunctionPointerForCFMFunctionPointer(void* inCfmProcPtr)
{
    UInt32 *vMachProcPtr = (UInt32*)NewPtr(sizeof(gGlueTemplate));

    vMachProcPtr[0] = gGlueTemplate[0] | ((UInt32)inCfmProcPtr >> 16);
    vMachProcPtr[1] = gGlueTemplate[1] | ((UInt32)inCfmProcPtr & 0xFFFF);
    vMachProcPtr[2] = gGlueTemplate[2];
    vMachProcPtr[3] = gGlueTemplate[3];
    vMachProcPtr[4] = gGlueTemplate[4];
    vMachProcPtr[5] = gGlueTemplate[5];
	mprotect(vMachProcPtr, sizeof(gGlueTemplate), PROT_EXEC);

    return(vMachProcPtr);
}

void DisposeMachOFunctionPointer(void *inMachProcPtr)
{
    if(inMachProcPtr)
		DisposePtr((Ptr)inMachProcPtr);
}
