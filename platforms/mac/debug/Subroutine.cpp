/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

//#define kCheckStart	48600	/* earliest crash out of 6 crashes: 48629, most of them in htm_elm.cpp */
#define kCheckStart	-1		/* always */

#include <stdio.h>
#include <string.h>

#include "platforms/mac/debug/Subroutine.h"
#include "platforms/mac/debug/OnScreen.h"
#include "platforms/mac/debug/StepSpy.jens.h"
#include "platforms/mac/debug/log2.jens.h"

#ifdef _AM_DEBUG
#include "platforms/mac/debug/anders/settings.anders.h"
#endif

#if defined(OPERA_WIDGET_APP)
#define kSubroutineStackXPosition		(-144)
#define kSubroutineLineStackXPosition	(-192)
#else
#define kSubroutineStackXPosition		(-48)
#define kSubroutineLineStackXPosition	(-96)
#endif

#define DO_HEAPCHECK

void PigMode(const char *inWhere, long inID);
void KillBeefs();

Boolean		gCheck = false;
long		Subroutine::sRoutineCount = 0;
long		gSubroutineCount = 0;
#if defined(QUICKLOG)
Log		gQuickLog("Quick.log");
#endif

long	Subroutine::sFlags = 0;
long	Subroutine::sIDindex = 0;
long	Subroutine::sIDstack[kIDstackSize + 1];
long	gYPosition = 0;

enum
{
	kShift = 0x00000001,
	kControl = 0x00000002,
	kAlternate = 0x00000004,
	kCommand = 0x00000008
};

/*inline*/ long Modifiers()
{
	register long	result;
	KeyMap			keys;
	long			modKeys;

	enum
	{
		W_SHIFT = 0x00000001,
		W_ALTERNATE = 0x00000004,
		W_CONTROL = 0x00000008,
		W_COMMAND = 0x00008000
	};

	GetKeys(keys);
#if TARGET_RT_BIG_ENDIAN
	modKeys = keys[1];
#else
	modKeys = keys[1].bigEndianValue;
#endif
	if(modKeys & W_SHIFT)
	{
		result = kShift;
	}
	else
	{
		result = 0;
	}
	if(modKeys & W_ALTERNATE)
	{
		result |= kAlternate;
	}
	if(modKeys & W_CONTROL)
	{
		result |= kControl;
	}
	if(modKeys & W_COMMAND)
	{
		result |= kCommand;
	}
	return(result);
}

/*inline*/ void NewHandleStress(const char *inWhere)
{
	Handle		temp;
	temp = NewHandle(10);
	if(NULL != temp)
	{
		DisposeHandle(temp);
	}
	else
	{
		DebugStr("\pNo more memory");
	}
}

/*inline*/ void NewPtrStress(const char *inWhere)
{
	void *temp;
	temp = op_malloc(10);
	if(NULL != temp)
	{
		op_free(temp);
	}
	else
	{
		DebugStr("\pNo more memory");
	}
}

/*inline*/ void MacsBugHC(const char *inWhere, long inID)
{
#ifdef DO_HEAPCHECK
	static char	firsttime = true;
	static char	dstr[257];

	sprintf(dstr, " ;%s hc; printf \"%s routine %ld\"; g", firsttime ? "hx 'OPRA';" : "", inWhere, inID);
	dstr[0] = strlen(&dstr[1]);
	DebugStr((unsigned char *) dstr);
	firsttime = false;
#endif
}

/*inline*/ void MacsBugHC2(const char *inWhere, long inID)
{
	static char	firsttime = true;
	static char	dstr[257];

	sprintf(dstr, " ;hc all; printf \"%s routine %ld\"; g", inWhere, inID);
	dstr[0] = strlen(&dstr[1]);
	DebugStr((unsigned char *) dstr);
	firsttime = false;
}

static long		gZeroPage[64];
static char		gZeroCheckDstr[256];
static char		gZeroFirstTime = true;

/*inline*/ void ZeroCheck(const char *inWhere, long inID)
{
	register int	i;
	register long	*p;
	register long	*z;
	static long		selfCheck;

	p = &gZeroPage[0];
	z = NULL;

	for(i = 64; i; i--)
	{
		if(*z++ != *p++)
		{
			p--;
			z--;
			if(gZeroFirstTime)
			{
				gZeroFirstTime = false;
				while(i--)
				{
					*p++ = *z++;
				}
				selfCheck = gZeroPage[0];
			}
			else
			{
				if(NULL == z && selfCheck != gZeroPage[0])
				{
					DebugStr("\pInternal error, globals overwritten!!");
				}
				sprintf(gZeroCheckDstr, " Zeropage address 0x%p was changed %s routine %ld", z, inWhere, inID);
				gZeroCheckDstr[0] = strlen(&gZeroCheckDstr[1]);
				DebugStr((unsigned char *) gZeroCheckDstr);
				while(i--)
				{
					*p++ = *z++;
				}
			}
			break;
		}
	}
}


#if defined(DEADBEEFS_ON_STACK_2048) || defined(NULLS_ON_STACK_2048)
#define STACKFILL_COUNT	8
#elif defined(DEADBEEFS_ON_STACK_1024) || defined(NULLS_ON_STACK_1024)
#define STACKFILL_COUNT	4
#elif defined(DEADBEEFS_ON_STACK_512) || defined(NULLS_ON_STACK_512)
#define STACKFILL_COUNT	2
#elif defined(DEADBEEFS_ON_STACK_256) || defined(NULLS_ON_STACK_256)
#define STACKFILL_COUNT	1
#endif

#if TARGET_CPU_PPC && defined(STACKFILL_COUNT)
asm void DeadBeefOnStack()			// the C++ version could never be as effective as this assembly version
{
#if defined(NULLS_ON_STACK_2048) || defined(NULLS_ON_STACK_1024) || defined(NULLS_ON_STACK_512) || defined(NULLS_ON_STACK_256)
#define STACKFILL_VALUE	0
#else
#define STACKFILL_VALUE	0xdeadbeef
#endif
		lis		r5,((STACKFILL_VALUE >> 16) & 0xffff)
		la		r3,-(STACKFILL_COUNT)*256(sp)
		ori		r5,r5,(STACKFILL_VALUE & 0xffff)
		li		r4,STACKFILL_COUNT
		stw		r5,0(r3)
// stall
		stw		r5,4(r3)
// stall
		mtctr	r4
		lfd		f0,0(r3)
// stall
		b		begin
loop:	stfd	f0,0*8(r3)
begin:	stfd	f0,1*8(r3)
		stfd	f0,2*8(r3)
		stfd	f0,3*8(r3)
		stfd	f0,4*8(r3)
		stfd	f0,5*8(r3)
		stfd	f0,6*8(r3)
		stfd	f0,7*8(r3)
		stfd	f0,8*8(r3)
		stfd	f0,9*8(r3)
		stfd	f0,10*8(r3)
		stfd	f0,11*8(r3)
		stfd	f0,12*8(r3)
		stfd	f0,13*8(r3)
		stfd	f0,14*8(r3)
		stfd	f0,15*8(r3)
		stfd	f0,16*8(r3)
		stfd	f0,17*8(r3)
		stfd	f0,18*8(r3)
		stfd	f0,19*8(r3)
		stfd	f0,20*8(r3)
		stfd	f0,21*8(r3)
		stfd	f0,22*8(r3)
		stfd	f0,23*8(r3)
		stfd	f0,24*8(r3)
		stfd	f0,25*8(r3)
		stfd	f0,26*8(r3)
		stfd	f0,27*8(r3)
		stfd	f0,28*8(r3)
		stfd	f0,29*8(r3)
		stfd	f0,30*8(r3)
		stfd	f0,31*8(r3)
		la		r3,256(r3)
		bdnz	loop
}
#else
void DeadBeefOnStack()					// we don't need this on 68k
{
}
#endif


Subroutine::Subroutine(long inID, long inStartLine)
{
	__attribute__ ((unused)) static const char	*where = "outside";
	start_line = inStartLine;
#ifdef _HISPEED_SUBROUTINE_	// {
	mID = inID;
	gCheck = true;
	check = true;

	if(check)
	{
		HEAPCHECK(where, inID);
		NEWPTRSTRESS(where);

#ifdef QHEAPCHECK
		QHC(where, inID);
#endif
#ifdef QUICKLOG
		gQuickLog.PutI(inID);
#endif
#ifdef STEPSPY
		CHECKSTEPSPY(where, inID);
#endif
	}
#else	// } _HISPEED_SUBROUTINE_ {
#ifdef _DEBUG_TIMING_
	OTTimeStamp	startTime;
	UInt32		time[20];
#endif
#ifndef QUICK_SUBROUTINE
	mMagic0 = '    ';
	mMagic1 = '###[';
#endif
	mID = inID;
#ifndef QUICK_SUBROUTINE
	mMagic2 = ']###';
	mMagic3 = '    ';
#endif

	check = false;
	show = true;
	log = false;
	check_stepspy = true;

	if(false
	|| (inID >= 800000 && inID <= 800999) // CMacView
	|| (inID >= 810000 && inID <= 819999) // CWidgetView/Window/VirtualWindow
#ifndef _AM_DEBUG
	|| (inID >= 850000 && inID <= 850999) // MacOpView
#endif
	|| (inID >= 860000 && inID <= 869999) // Painting
		)
	{
		show = false;
	}

#ifdef SUBROUTINE_CHECK_ALL
	check = true;
#endif

	if(show)
	{
		gYPosition -= 8;
		DecOnScreen(kSubroutineStackXPosition, gYPosition, kOnScreenBlue, kOnScreenWhite, inID);
#if defined(_AM_DEBUG) || defined(_JH_DEBUG)
		Str8OnScreen(kSubroutineLineStackXPosition, gYPosition, kOnScreenWhite, kOnScreenWhite, "      ");
#endif
	}

	if(log)
	{
	}

	if(check)
	{

/*
		count = gCounterList->Increment(inID);
		if(count < 100)
		{
			if(0 == (kShift & Modifiers()))
			{
				MacsBugHC(where, inID);
			}
		}
*/
		if (check_stepspy)
		{
			CHECKSTEPSPY(where, inID);
		}
		ZEROCHECK(where, inID);
		NEWPTRSTRESS(where);
		NEWHANDLESTRESS(where);
		HEAPCHECK(where, inID);
		NEWCHECK(-1, inID);
		KILLBEEFS
		PIGMODE(-1, inID);
#ifdef CUSTOM_SUBROUTINE_START
		CUSTOM_SUBROUTINE_START
#endif
	}

#endif	// } _HISPEED_SUBROUTINE_
}

Subroutine::~Subroutine()
{
	__attribute__ ((unused)) static const char	*where = "inside";
#ifdef _HISPEED_SUBROUTINE_	// {
	if(check)
	{
		HEAPCHECK(where, id);
		NEWPTRSTRESS(where);
#ifdef QHEAPCHECK
		QHC(where, id);
#endif
#ifdef QUICKLOG
		gQuickLog.PutO(id);
#endif
#ifdef STEPSPY
		CHECKSTEPSPY(where, id);
#endif
	}
#else	// } _HISPEED_SUBROUTINE_ {
	long	thisID;

	thisID = mID;
#ifndef QUICK_SUBROUTINE
	mMagic0 = '    ';
	mMagic1 = '---[';
	mID = -1;
	mMagic2 = ']---';
	mMagic3 = '    ';
#endif

	if(check)
	{
#ifdef CUSTOM_SUBROUTINE_END
		CUSTOM_SUBROUTINE_END
#endif
		PIGMODE(+1, thisID);
		KILLBEEFS
		NEWCHECK(+1, thisID);
		HEAPCHECK(where, thisID);
		NEWHANDLESTRESS(where);
		NEWPTRSTRESS(where);
		ZEROCHECK(where, thisID);
		if (check_stepspy)
		{
			CHECKSTEPSPY(where, thisID);
		}
/*
		count = gCounterList->Increment(thisID);
		if(count < 100)
		{
			if(0 == (kShift & Modifiers()))
			{
				MacsBugHC(where, thisID);
			}
		}
*/
	}

	if(log)
	{
	}
	if(show)
	{
		if(sIDindex >= 0)
		{
			DecOnScreen(kSubroutineStackXPosition, gYPosition, kOnScreenRed, kOnScreenWhite, thisID);
#if defined(_AM_DEBUG) || defined(_JH_DEBUG)
			Str8OnScreen(kSubroutineLineStackXPosition, gYPosition, kOnScreenWhite, kOnScreenWhite, "      ");
#endif
			gYPosition += 8;
		}
	}
#endif	// } _HISPEED_SUBROUTINE_
	gCheck = check;
}

long Subroutine::GetCurrentID()
{
	return(sIDstack[sIDindex + 2]);
}

void SubroutineLine(int locline)
{
	DecOnScreen(kSubroutineLineStackXPosition, gYPosition, kOnScreenBlack, kOnScreenWhite, locline);
}


/*
struct Pig					// nothing fancy
{
	long	magic[4];
};


#ifndef PIGS
#define PIGS	256
#endif

Pig		*gPigs[PIGS];
long	gPigsAlive = 0;
enum
{
	kPigMagic0 = 'Big ',
	kPigMagic1 = 'Bad ',
	kPigMagic2 = 'Wolf',
	kPigMagic3 = 'YUM!'
};

void PigMode(const char *inWhere, long inID)
{
	static bool		first = true;
	static long		pigTime = 0;
	register long	deadbeef;

	register int	i;
	register Pig	*p;
	int				pigs;
	Str255			dstr;
	long			ticks;

	if(first)
	{
		first = false;
		memset(gPigs, 0, sizeof(gPigs));
	}

#ifdef _FAST_PIGS_						// how fast runs that pig ?
	ticks = TickCount();
	if(pigTime <= ticks)
	{
#ifndef PIGSPEED
#define PIGSPEED 1
#endif
		pigTime = ticks + PIGSPEED;	// only once per tick. (you can change this to be more often if you like)
#endif
		pigs = sizeof(gPigs) / sizeof(gPigs[0]);

		i = pigs;
		gPigsAlive = 0;
		while(i--)
		{
			p = gPigs[i];
			if(NULL != p)
			{
				gPigsAlive++;
				if(kPigMagic0 != p->magic[0] || kPigMagic1 != p->magic[1] || kPigMagic2 != p->magic[2] || kPigMagic3 != p->magic[3])
				{
					sprintf((char *) &dstr[1], "Pigmode: memory at address 0x%08X was overwritten %s routine %d", p, inWhere, inID);
					dstr[0] = strlen((char *) &dstr[1]);
					DebugStr(dstr);
				}
			}
		}

		i = (((unsigned long) WoFRandom()) >> 3) % pigs;	// modulo number of pigs
		p = gPigs[i];								// choose a random pig
		gPigs[i] = (Pig *) NewPtr(sizeof(Pig));		// get a new pig
		if(NULL != gPigs[i])
		{
			gPigs[i]->magic[0] = kPigMagic0;
			gPigs[i]->magic[1] = kPigMagic1;
			gPigs[i]->magic[2] = kPigMagic2;
			gPigs[i]->magic[3] = kPigMagic3;
			if(NULL != p)
			{
				deadbeef = 0xdeadbeef;
				p->magic[0] = deadbeef;				// this is always good to do! :)
				p->magic[1] = deadbeef;
				p->magic[2] = deadbeef;
				p->magic[3] = deadbeef;
				DisposePtr((char *) p);
			}
		}
		else
		{
			gPigs[i] = p;				// set it back, so we don't loose a buffer
		}
		if(1 == (Random() & 255))		// remove all pigs
		{
			i = pigs;
			while(i--)
			{
				if(NULL != gPigs[i])
				{
					DisposePtr((char *) gPigs[i]);
					gPigs[i] = NULL;
				}
			}
		}
#ifdef _FAST_PIGS_						// how fast runs that pig ?
	}
#endif
}

void KillBeefs()
{
	static long		beefTime = 0;
	long			ticks;
	char			*p;
	Str255			dstr;
	long			size;
	long			*lp;
	register long	magic;

	ticks = TickCount();
	if(beefTime <= ticks)
	{
		beefTime = ticks + 1;			// once per tick only
		size = WoFRandom() & 0xffff0;
		p = NewPtr(size);
		if(NULL != p)
		{
			deadbeefset(p, size);		// this routine is optimised.
#if 0
			size = size >> 4;			// divide by 16
			lp = (long *) p;
			magic = 0xdeadbeef;
			while(size--)				// 16 bytes at a time
			{
				*lp++ = magic;
				*lp++ = magic;
				*lp++ = magic;
				*lp++ = magic;
			}
#endif
			DisposePtr(p);
		}
	}
}*/

void CheckPoint(long inID)
{	SUBROUTINE(inID)
}

