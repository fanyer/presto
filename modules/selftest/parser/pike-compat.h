/* -*- Mode: pike; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#if __VERSION__ < 7.4
int q = write(#"
===========================================================================
 Pike 7.4 or newer is recommended (latest stable is 7.6 as of 2004-12-02)
 To get pike, go to http://pike.ida.liu.se
 Even numbered minor version releases are stable versions,
 odd are unstable
===========================================================================

");
#endif

// 'this' instead of 'this_object()' is the norm since 7.4. But did
// not exist before.
#if __VERSION__ < 7.4
# define this this_object()
#endif

// The bool type will be added in 7.8 (actually, newer 7.7, but that's
// sort of hard to detect).
#if __VERSION__ < 7.8
enum bool { false=0, true=1 };
#endif

// Pike 7.2 and early 7.3 compatibilty.
#if constant(System.Timer)
# define TIMER System.Timer
#else
class TIMER
{
	int tt = time();
	float tf = time(tt);

	float get() 
	{
		float res=(time(tt)-tf);
		tt = time();
		tf = time(tt);
		return res;
	}
}
#endif


#ifdef DEBUG
#  define DEBUG_TIME_START() TIMER __t = TIMER();
#  define DEBUG_TIME(X) werror("[%s %3dms]", X,(int)(__t->get()*1000))
#else
#  define DEBUG_TIME_START()
#  define DEBUG_TIME(X)
#endif

