/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include <stdio.h>

#include "platforms/mac/util/Timer.h"
#include "platforms/mac/debug/Counter.h"

MacDebugCounter	*MacDebugCounter::sCounterList = 0;

MacDebugCounter::MacDebugCounter(long inID) :
	mSubroutineID(inID)
{
	Init();
}

MacDebugCounter::MacDebugCounter(long inID, const char *inFunctionName) :
	mSubroutineID(inID)
{
	Init();
	mFunctionName = inFunctionName;
}

void MacDebugCounter::Init()
{
	mFunctionName = 0;
	mCount = 0;
	mMilliSecondsUsed = 0;

	mFlags = DEBUG_GLOBAL;
	mNext = sCounterList;
	sCounterList = this;
}

MacDebugCounter::~MacDebugCounter()
{
	static Boolean	dumpAll = true;

	if(dumpAll)
	{
		dumpAll = false;
		DumpAll();
	}
}

void MacDebugCounter::DumpAll()
{
	FILE		*fp;
	MacDebugCounter		*prev;
	MacDebugCounter		*travel;
	MacDebugCounter		*next;
	Boolean		sorted;
	long		uptime;
	const char	*functionName;

	sorted = false;
	while(!sorted)
	{
		sorted = true;
		prev = 0;
		travel = sCounterList;
		while(travel)
		{
			next = travel->mNext;
			if(next)
			{
				if(next->mCount > travel->mCount)
				{
					travel->mNext = next->mNext;
					next->mNext = travel;
					if(prev)
					{
						prev->mNext = next;
					}
					else
					{
						sCounterList = next;
					}
					sorted = false;
				}
			}
			prev = travel;
			travel = travel->mNext;
		}
	}

	fp = fopen(": logs:timing.html", "wt");
	if(fp)
	{
		uptime = gTimer.Uptime();				// total uptime
		fprintf(fp, "<html>\n"
					"	<head>\n"
					"		<title>Timing</title>\n"
					"		<style type=\"text/css\">\n"
					"			.b	{ background-color:#00e; width:128; text-align:center; }\n"
					"			.r	{ background-color:#e00; width:128; text-align:center; }\n"
					"			.m	{ background-color:#e0e; width:128; text-align:center; }\n"
					"			.g	{ background-color:#0e0; width:128; text-align:center; }\n"
					"			.c	{ background-color:#0ee; width:128; text-align:center; }\n"
					"			.y	{ background-color:#ee0; width:128; text-align:center; }\n"
					"			.w	{ background-color:#eee; width:128; text-align:center; }\n"
					"			.lb	{ background-color:#11f; width:128; text-align:center; }\n"
					"			.lr	{ background-color:#f11; width:128; text-align:center; }\n"
					"			.lm	{ background-color:#f1f; width:128; text-align:center; }\n"
					"			.lg	{ background-color:#1f1; width:128; text-align:center; }\n"
					"			.lc	{ background-color:#1ff; width:128; text-align:center; }\n"
					"			.ly	{ background-color:#ff1; width:128; text-align:center; }\n"
					"			.lw	{ background-color:#fff; width:128; text-align:center; }\n"
					"		</style>\n"
					"	</head>\n"
					"	<body>\n"
					"		<table cols=4 border=1 cellspacing=0 cellpadding=1>\n"
					"			<tr>\n"
					"				<td class=\"b\">routine#</td>\n"
					"				<td class=\"r\">time used</td>\n"
					"				<td class=\"y\">times called</td>\n"
					"				<td class=\"g\">average time used</td>\n"
					"				<td class=\"c\">Function name</td>\n"
					"			</tr>\n"

					"			<tr>\n"
					"				<td class=\"w\">(Opera)</td>\n"
					"				<td class=\"w\">%ld</td>\n"
					"				<td class=\"w\">1</td>\n"
					"				<td class=\"w\">%ld</td>\n"
					"			</tr>\n", uptime, uptime);
		travel = sCounterList;
		while(travel)
		{
			fprintf(fp, "			<tr>\n"
						"				<td class=\"b\">%ld</td>\n", travel->mSubroutineID);
			fprintf(fp, "				<td class=\"r\">%lu</td>\n", travel->mMilliSecondsUsed);
			fprintf(fp, "				<td class=\"y\">%lu</td>\n", travel->mCount);
			if(travel->mCount)
				fprintf(fp, "				<td class=\"g\">%lu</td>\n", travel->mMilliSecondsUsed / travel->mCount);
			else
				fprintf(fp, "				<td class=\"g\"></td>\n");
			functionName = travel->mFunctionName;
			if(0 == functionName)
			{
				functionName = "";
			}
			fprintf(fp, "				<td class=\"c\">%s</td>\n", functionName);
			fprintf(fp, "			</tr>\n");
			travel = travel->mNext;
		}
		fprintf(fp, "		</table>\n"
					"	</body>\n"
					"</html>\n");
		fclose(fp);
	}
}
