// ==============================================================================================================
// Counter.h
// ==============================================================================================================
// By Jens Bauer 16/11-2000
// ==============================================================================================================
//  Current owner: Jens Bauer
// ==============================================================================================================
// Class for profiling.

#ifndef _COUNTER_H_
#define _COUNTER_H_

enum	// debug-flags
{
	DEBUG_MAGICS_ON_STACK		= 1 << 0,	// store Magics on stack
	DEBUG_ID_STACK				= 1 << 1,	// store subroutine IDs on local stack (in a buffer)
	DEBUG_ID_ON_STACK			= 1 << 2,	// (unused)
	DEBUG_ZEROCHECK				= 1 << 3,
	DEBUG_NEWCHECK				= 1 << 4,
	DEBUG_HSMEMCHECK			= 1 << 5,	// Test if HighSpeedMemory's blocks are damaged
	DEBUG_QHC					= 1 << 6,	// Quick, but not so picky HeapCheck
	DEBUG_HEAPCHECK				= 1 << 7,	// Test if Opera's internal heap is damaged
	DEBUG_PMHEAPCHECK			= 1 << 8,	// Test if Process Manager's heap is damaged
	DEBUG_NEWPTRSTRESS			= 1 << 9,	// ::DisposePtr(::NewPtr(10));
	DEBUG_NEWHANDLESTRESS		= 1 << 10,
	DEBUG_TEMPHANDLECHECK		= 1 << 11,	//
	DEBUG_KILLBEEFS				= 1 << 12,
	DEBUG_STEPSPY				= 1 << 13,	// check stepspy
	DEBUG_QUICKLOG				= 1 << 14,	// gQuickLog.PutI(id);  -This is not indented.
	DEBUG_LOG					= 1 << 15,	// log subroutine IDs in gOperaLog
	DEBUG_SHOW_SUBROUTINE_ID	= 1 << 16,	// Show subroutine ID directly on screen
	DEBUG_COUNT					= 1 << 17,	// count the # of times a routine has been used
	DEBUG_TIMING				= 1 << 18,	// store timing information for each subroutine

	DEBUG_CUSTOM_FLAG			= 1 << 30,	// use this as you want.
	DEBUG_GLOBAL				= 1 << 31	// use global settings
};

class MacDebugCounter
{
private:
	static MacDebugCounter			*sCounterList;
	MacDebugCounter					*mNext;
	long					mSubroutineID;
	unsigned long			mCount;
	unsigned long			mMilliSecondsUsed;
	unsigned long			mStartTime;
	long					mFlags;
	const char				*mFunctionName;

public:
							MacDebugCounter(long inID);
							MacDebugCounter(long inID, const char *inFunctionName);
							~MacDebugCounter();
	void					Init();
	void					DumpAll();
	inline void				Increment(){ mCount++; }
	inline void				UpdateTime(unsigned long inMilliSeconds){ mMilliSecondsUsed += inMilliSeconds; }
	inline long				Flags(){ return(mFlags); }
	inline void				SetFlags(long inFlags){ mFlags = inFlags; }
	inline unsigned long	GetCount(){ return(mCount); }
	inline unsigned long	GetTimeUsed(){ return(mMilliSecondsUsed); }

};

#endif	// _COUNTER_H_
