// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// line 2	// File: Subroutine.h
// line 3	// For debugging purposes in Opera
// line 4	// Can be torn off easily

#ifndef _SUBROUTINE_H_	// {
#define _SUBROUTINE_H_

#include "platforms/mac/debug/Counter.h"

extern long		gPigsAlive;

void ZeroCheck(long where, long theID);
void PigMode(long where, long theID);		// put in here, to allow everyone to enjoy this debug feature
void CheckPoint(long theID);

asm void DeadBeefOnStack();			// the C++ version could never be as effective as this assembly version
//void DeadBeefsOnStack();

class SockSubroutine
{
private:
	long			mID;

public:

					SockSubroutine(long inID);
					~SockSubroutine();
};

class Subroutine
{
	enum
	{
		kIDstackSize = 2 + 1024
	};

#ifdef _AM_DEBUG
public:
#else
private:
#endif
	static long		sRoutineCount;
	static long		sFlags;
	static long		sIDindex;
	static long		sIDstack[kIDstackSize + 1];

public:

	SInt32			mMagic0;
	SInt32			mMagic1;
	SInt32			mID;
	SInt32			mMagic2;
	SInt32			mMagic3;

	long			mFlags;

	char			show;
	char			check;
	char			log;
	char			check_stepspy;
	long			start_line;

					Subroutine(long inID, MacDebugCounter *inCounter);
					Subroutine(long inID, long inStartLine = 0);
					~Subroutine();
	static long		GetCurrentID();
};

#endif	// } _SUBROUTINE_H_
