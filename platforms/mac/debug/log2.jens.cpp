/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if !TARGET_RT_MAC_MACHO
#include <OpenTptClient.h>
#endif

#include <stdarg.h>

#include "platforms/mac/debug/log2.jens.h"
#include "platforms/mac/debug/OnScreen.h"
#ifdef _JB_DEBUG
#include DEBUG_SETTINGS
#endif

char	*gSpaces = "                                                                                                                                                                                                                                                                \0";
#ifdef SPECIAL_MAC_TEST_VERSION
Log		gContextLog("Context.log");
#endif

SInt32	gNotifierCount = 0;

Log::Log(char *filename)
{
#ifndef _DISABLE_LOGGING
	char	oldname[256];

	enabled = true;
	spacePtr = &gSpaces[256];
	sprintf(name, ": logs:%s", filename);
	sprintf(oldname, "%s.old", name);
	::remove(oldname);
	::rename(name, oldname);
	::remove(name);				// just in case the newest file could not be renamed
	if(Open())
	{
		fprintf(fp, "\n-- New Session -------------------------------------------------------------------------------------\n");
		Close();
	}
	currLog = 0;
	notifierLog[0] = (char*)op_malloc(16384 + 1);
	notifierLog[1] = (char*)op_malloc(16384 + 1);
	if(NULL != notifierLog[0])
	{
		*notifierLog[0] = '\0';
	}
	else
	{
		DebugStr("\pCould not get buffer0 for log-file");
	}
	if(NULL != notifierLog[1])
	{
		*notifierLog[1] = '\0';
	}
	else
	{
		DebugStr("\pCould not get buffer1 for log-file");
	}
#endif
}

Log::~Log()
{
#ifndef _DISABLE_LOGGING
	if(NULL != fp)
	{
		fclose(fp);
	}
	if(NULL != notifierLog[0])
	{
		op_free(notifierLog[0]);
	}
	if(NULL != notifierLog[1])
	{
		op_free(notifierLog[1]);
	}
#endif
}

void Log::PutI(int id)
{
#ifndef _DISABLE_LOGGING
	if(Open())
	{
		fprintf(fp, "%s{%d\n", spacePtr, id);
		Close();
	}
#endif
}

void Log::PutO(int id)
{
#ifndef _DISABLE_LOGGING
	if(Open())
	{
		fprintf(fp, "%s}%d\n", spacePtr, id);
		Close();
	}
#endif
}

void Log::Put(char *format, ...)
{
	va_list				marker;
	char				temp[1024];
	char				*p;
	static OTTimeStamp	startTime;
	UInt32				mS;
	static Boolean		first = true;
	UInt8				log;

	va_start(marker, format);

#ifndef _DISABLE_LOGGING
	if(first)
	{
		first = false;
		OTGetTimeStamp(&startTime);
	}
	mS = OTElapsedMilliseconds(&startTime);

	p = temp;
	sprintf(p, " %2lu:%02lu:%02lu.%03lu ", mS / 3600000, (mS / 60000) % 60, (mS / 1000) % 60, mS % 1000);
	p += OTStrLength(p);
	OTStrCopy(p, spacePtr);
	p += OTStrLength(p);
	vsprintf(p, format, marker);
	p += OTStrLength(p);
	*p++ = '\n';
	*p++ = '\0';

	if(0 == gNotifierCount)
	{
		if(Open())
		{
			log = (~OTAtomicAdd8(1, &currLog)) & 1;			// toggle currLog, get result to log
			p = notifierLog[log];
			if(NULL != p)
			{
				if(*p)										// if we have something in the notifierlog
				{
					fprintf(fp, "%s", p);					// write old log to disk
					*p = '\0';								// reset buffer
				}
			}
			fprintf(fp, "%s", temp);						// write new string to disk
			Close();
		}
	}
	else
	{
//		Dec8OnScreen(0, 0, kOnScreenBlue, timesCalledByNotifier++);
		log = currLog & 1;
		p = notifierLog[log];
		if(NULL != p)
		{
			p[10240] = '\n';
			p[10241] = '{';
			p[10242] = 's';
			p[10243] = 'n';
			p[10244] = 'i';
			p[10245] = 'p';
			p[10246] = '}';
			p[10247] = '\n';
			p[10248] = '\0';		// make sure it does not write way too much
			temp[0] = '*';
			OTStrCat(p, temp);
//			Dec8OnScreen(0, 8, kOnScreenBlue, log);
//			Dec8OnScreen(0, 16, kOnScreenBlue, OTStrLength(p));
		}
//		Dec8OnScreen(0, 24, kOnScreenBlue, (long) p);
	}
#endif
	va_end(marker);
}

int Log::Open()
{
	fp = NULL;
	if(enabled)						// only try opening if log is enabled
	{
		fp = fopen(name, "at");
	}
	if(NULL == fp)					// if we had an error opening file...
	{
		enabled = false;			// ...give up, never try again.
	}
	return(NULL != fp);				// return whether we opened the file or not
}

void Log::Close()
{
	fclose(fp);
	fp = NULL;
}
