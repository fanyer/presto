/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include <stdio.h>
#include <string.h>

#include "platforms/mac/debug/StepSpy.jens.h"
#ifdef _JB_DEBUG
#include "platforms/mac/debug/OnScreen.h"
#endif

StepSpy	*gStepSpyListEvenEven = NULL;
StepSpy	*gStepSpyList = NULL;
StepSpy	*gStepSpyListLong = NULL;

StepSpy::StepSpy(StepSpy **theOwner)
{
	next = *theOwner;
	*theOwner = this;
	owner = theOwner;
	if(next)
	{
		next->prev = this;
	}
	prev = NULL;
}

StepSpy::~StepSpy()
{
	if(this == *owner)
	{
		*owner = next;
	}
	if(NULL != prev)
	{
		prev->next = next;
	}
	if(NULL != next)
	{
		next->prev = prev;
	}
}

long	gStepSpyCounter = 0;

void AddStepSpy(void *start, long length, SpyKind spyKind, char *id)
{
	StepSpy	*newSpy;

#ifdef _JB_DEBUG
	DecOnScreen(0, -16, kOnScreenBlue, kOnScreenWhite, ++gStepSpyCounter);
#endif

	if(kStepSpyPointerToVoid == spyKind)
	{
		newSpy = new StepSpy(&gStepSpyListLong);
		newSpy->start = start;
		newSpy->value = *((long *) start);
		newSpy->length = length | 0x80000000;
		newSpy->checksum = *((long *) start);
	}
	else if((kStepSpyHandle == spyKind) || (length & 1) || (((long) start) & 3))
	{
		newSpy = new StepSpy(&gStepSpyList);
		newSpy->start = start;
		newSpy->length = length | ((kStepSpyHandle == spyKind) ? 0x80000000 : 0);
		newSpy->checksum = GetCheckSum(newSpy);
	}
	else if(4 == length)
	{
		newSpy = new StepSpy(&gStepSpyListLong);
		newSpy->start = start;
		newSpy->length = length;
		newSpy->checksum = *((long *) start);
	}
	else	// length is divisible by 2, start is divisible by 2
	{
		newSpy = new StepSpy(&gStepSpyListEvenEven);
		newSpy->start = start;
		newSpy->length = length >> 1;
		newSpy->checksum = GetCheckSum2(newSpy);
	}
	newSpy->id = id;
}

//void AddStepSpy(void *start, long length, short isHandle, char *id)
//{
//	AddStepSpy(start, length, kStepSpyHandle, id);
//}

//void AddStepSpy(void *start, long length, SpyKind spyKind)
//{
//	AddStepSpy(start, length, spyKind, "");
//}

void RemoveStepSpy(void *start)
{
	StepSpy		*travel, *theNext;
	static bool	found;

#ifdef _JB_DEBUG
	DecOnScreen(0, -16, kOnScreenBlue, kOnScreenWhite, --gStepSpyCounter);
#endif

	found = false;

	travel = gStepSpyListEvenEven;
	while(NULL != travel)
	{
		theNext = travel->next;
		if(start == travel->start)
		{
			delete travel;
			found = true;
		}
		travel = theNext;
	}

	travel = gStepSpyList;
	while(NULL != travel)
	{
		theNext = travel->next;
		if(start == travel->start)
		{
			delete travel;
			found = true;
		}
		travel = theNext;
	}

	travel = gStepSpyListLong;
	while(NULL != travel)
	{
		theNext = travel->next;
		if(start == travel->start)
		{
			delete travel;
			found = true;
		}
		travel = theNext;
	}

	if(!found)
	{
		DebugStr("\pStepSpy not found!");
	}
}

bool HasStepSpy(void *start)
{
	StepSpy		*travel, *theNext;
	static bool	found;

	found = false;

	travel = gStepSpyListEvenEven;
	while(NULL != travel && !found)
	{
		theNext = travel->next;
		if(start == travel->start)
		{
			found = true;
		}
		travel = theNext;
	}

	travel = gStepSpyList;
	while(NULL != travel && !found)
	{
		theNext = travel->next;
		if(start == travel->start)
		{
			found = true;
		}
		travel = theNext;
	}

	travel = gStepSpyListLong;
	while(NULL != travel && !found)
	{
		theNext = travel->next;
		if(start == travel->start)
		{
			found = true;
		}
		travel = theNext;
	}
	return found;
}

void EndDebug (void);


void CheckStepSpy(const char *inWhere, long inID)
{
	static char 			dstr[257];
	StepSpy					*travel;
	long					address;
	long					newCheckSum;
//	register unsigned char	*p;

	travel = gStepSpyListEvenEven;		// not for handles, not for odd lengths, not for odd addresses
	while(NULL != travel)
	{
		newCheckSum = GetCheckSum2(travel);
		if(travel->checksum != newCheckSum)
		{
			sprintf(dstr, " OperaStepSpyEven: checksum changed at address 0x%p %s routine %ld, SpyKind=\"%s\"", travel->start, inWhere, inID, travel->id);
			dstr[0] = strlen(&dstr[1]);
			DebugStr((unsigned char *) dstr);
			travel->checksum = newCheckSum;
		}
		travel = travel->next;
	}

	travel = gStepSpyList;
	while(NULL != travel)
	{
		newCheckSum = GetCheckSum(travel);
		if(newCheckSum != travel->checksum)
		{
			address = (long) travel->start;
			if(travel->length < 0)
			{
				address = * (long*)address;
			}
			sprintf(dstr, " OperaStepSpy: checksum changed at address 0x%p %s routine %ld, SpyKind=\"%s\"", (void*)address, inWhere, inID, travel->id);
			dstr[0] = strlen(&dstr[1]);
			DebugStr((unsigned char *) dstr);
			travel->checksum = newCheckSum;
		}
		travel = travel->next;
	}
	travel = gStepSpyListLong;
	while(NULL != travel)
	{
		if(travel->length & 0x80000000)							// PointerToVoid
		{
			if(travel->value != *((long *) travel->start))
			{
				sprintf(dstr, " OperaStepSpyPointerToVoid: pointer changed at address 0x%p %s routine %ld, SpyKind=\"%s\"", travel->start, inWhere, inID, travel->id);
				dstr[0] = strlen(&dstr[1]);
				DebugStr((unsigned char *) dstr);
			}
			(void) (*((char **) travel->start))[0];									// try pointer
			(void) (*((char **) travel->start))[(travel->length & 0x7fffffff) - 1];	// try pointer
		}
		else if((*(long *) travel->start) != travel->checksum)
		{
			sprintf(dstr, " OperaStepSpyLong: checksum changed at address 0x%p %s routine %ld, SpyKind=\"%s\"", travel->start, inWhere, inID, travel->id);
			dstr[0] = strlen(&dstr[1]);
			DebugStr((unsigned char *) dstr);
			travel->checksum = (*(long *) travel->start);
		}
		travel = travel->next;
	}
/*
	travel = gStepSpyListSmall;
	while(NULL != travel)
	{
		p = (unsigned char *) travel->start;
		newCheckSum = (p[0] << 16) | (p[1] << 8) | p[2];
		if((*(long *) travel->start) != travel->checksum)
		{
			sprintf(dstr, " OperaStepSpySmall: checksum changed at address 0x%08X %s routine %d, SpyKind=\"%s\"", travel->start, inWhere, inID, travel->id);
			dstr[0] = strlen(&dstr[1]);
			DebugStr((unsigned char *) dstr);
			travel->checksum = newCheckSum;
		}
		travel = travel->next;
	}
*/

}

//	sp = GetStackPointer();
//	stackSpace = sp - GetApplLimit();
