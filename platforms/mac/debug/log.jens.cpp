/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

//#define _FSS_LOGGING_		/* Faster and interrupt safe */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "platforms/mac/debug/log.jens.h"
#include "platforms/mac/debug/OnScreen.h"

bool	gLogEnabled = true;
char	gLogBuffer[65536+4096] = {'\0'};


OSErr FSNew(char *filename, FSSpec *fss, short *refNum)
{
	OSErr			err;
	unsigned char	*fname;
	int				l;

	l = strlen(filename);
	fname = (unsigned char *) new char[1 + l];

	memcpy(&fname[1], filename, l);
	fname[0] = l;

	FSMakeFSSpec(0, 0, fname, fss);

	err = FSpOpenDF(fss, fsWrPerm, refNum);
	if(fnfErr == err)
	{
		err = FSpCreate(fss, 'MPS ', 'TEXT', smSystemScript);
		if(noErr == err)
		{
			err = FSpOpenDF(fss, fsWrPerm, refNum);
		}
	}
	delete fname;
	return(err);
}

OSErr FSDumpBin(short refNum, void *start, size_t length)
{
	OSErr	err;
	long	len;

	len = (long) length;
	err = FSWrite(refNum, &len, start);
	return(err);
}

OSErr FSDump(short refNum, void *start, size_t length)
{
	enum
	{
		BYTES_PER_LINE = 16
	};

	OSErr			err;
	char			temp[256], *p, *a;
	unsigned char	c, *s;
	const char		*hex = "0123456789ABCDEF";
	size_t			len1, len2;
	int				i;

	s = (unsigned char *) start;

	len1 = length / BYTES_PER_LINE;
	len2 = length - (len1 * BYTES_PER_LINE);

	err = noErr;
	while(len1-- && noErr == err)
	{
		p = &temp[0];
		a = &temp[(BYTES_PER_LINE / 8) * 17];	// a space for each 8 bytes

		*a++ = ' ';
		*a++ = ' ';

		for(i = BYTES_PER_LINE; i > 0; i--)
		{
			if(0 == (7 & i))
			{
				*p++ = ' ';
			}
			c = *s++;
			if( c < 32)
			{
				*a++ = '.';
			}
			else
			{
				*a++ = c;
			}
			*p++ = hex[c >> 4], *p++ = hex[c & 15];
		}
		*a++ = '\n';
		long len = a - &temp[0];
		err = FSWrite(refNum, &len, &temp[0]);
	}

	if(noErr == err)
	{
		p = &temp[0];
		a = &temp[(BYTES_PER_LINE / 8) * 17];	// a space for each 8 bytes
		memset(&temp[0], ' ', (BYTES_PER_LINE / 8) * 17);
		*a++ = ' ';
		*a++ = ' ';

		for(i = BYTES_PER_LINE; i > 0 && len2--; i--)
		{
			if(0 == (7 & i))
			{
				*p++ = ' ';
			}
			c = *s++;
			if( c < 32)
			{
				*a++ = '.';
			}
			else
			{
				*a++ = c;
			}
			*p++ = hex[c >> 4], *p++ = hex[c & 15];
		}
		*a++ = '\n';
		long len = a - &temp[0];
		err = FSWrite(refNum, &len, &temp[0]);
	}

	return(err);
}

/*
OSErr FSDumpBinToFile(Socket *socket, void *start, size_t length)
{
	FSSpec		fss;
	short		refNum;
	char		filename[32];
	OSErr		err = noErr;
//	long		eof;

#ifdef _DEBUG_CSOCKS_
	if(gLogEnabled)
	{
		sprintf(filename, ": logs:Socket%d.bin", socket->SocketID());
		if(noErr == FSNew(filename, &fss, &refNum))
		{
//			eof = 0;
//			err = GetEOF(refNum, &eof);
			err = SetFPos(refNum, fsFromLEOF, 0);
			FSDumpBin(refNum, start, length);
			err = FSClose(refNum);
		}
	}
#endif
	return(err);
}

OSErr FSDumpToFile(Socket *socket, void *start, size_t length)
{
	FSSpec		fss;
	short		refNum;
	char		filename[32];
	OSErr		err = noErr;

#ifdef _DEBUG_CSOCKS_
	if(gLogEnabled)
	{
		sprintf(filename, ": logs:Socket%d.dump%d", socket->SocketID(), socket->IncCount());
		if(noErr == FSNew(filename, &fss, &refNum))
		{
			FSDump(refNum, start, length);
			err = FSClose(refNum);
		}
	}
#endif
	return(err);
}
*/

char WoFlog(char *format, ...)
{
	static bool	first = true;
	char		s[1024];
	va_list		marker;
	va_start(marker, format);

#if (NOTIFIERDEBUG(1) + 0)
	if(0 == OTAtomicAdd32(0, &gNotifierCalls))
#else
	if(1)			// can't log from within the notifier
#endif
	{
#ifdef _FSS_LOGGING_
		FSSpec		fss;

		if(noErr == FSNew(": logs:csocks.log", &fss, &refNum))
		{
//			err = GetEOF(refNum, &fpos);
			err = SetFPos(refNum, fsFromLEOF, 0);
			if(noErr == err)
			{
				if(first)
				{
					first = false;
					strcpy(s, "\n\n--{ New session }--\n");
					length = strlen(s);
					err = FSWrite(refNum, &length, s);
				}
				if(NULL != format)
				{
					vsprintf(s, format, marker);
					strcat(s, "\n");
					length = strlen(s);
					err = FSWrite(refNum, &length, s);
				}
				err = FSClose(refNum);
			}
		}
#else
		FILE *fp = fopen(": logs:csocks.log", "at");
////	fprintf(fp, "~socket: 0x%08X, provider = 0x%08X\n", this, Provider());
		if(first)
		{
			first = false;
			strcpy(s, "\n\n--{ New session }--\n");
		}
		fputs(gLogBuffer, fp);
		gLogBuffer[0] = '\0';
		if(NULL != format)
		{
			vfprintf(fp, format, marker);
//			vsprintf(s, format, marker);
//			fprintf(fp, "%s\n", s);
			fprintf(fp, "\n");
		}
		fclose(fp);
#endif
	}
	else	// Called from Notifier
	{
		if(first)
		{
			first = false;
			strcpy(gLogBuffer, "\n\n--{ New session }--\n");
		}
		if(NULL != format)
		{
			vsprintf(s, format, marker);
			strcat(s, "\n");
			strcat(gLogBuffer, s);
		}
		strcpy(&gLogBuffer[65536], "\n\n********************\n** LogBuffer full **\n********************\n\n");
	}

	va_end(marker);
	return(0);
}


SInt16	gLockTrackCount = 0;

LockTrack::LockTrack()
{
	oldCount = gLockTrackCount++;
	Put4Pixel(640 + (oldCount << 1), 0, kOnScreenBlue);
}

LockTrack::~LockTrack()
{
	Put4Pixel(640 + (oldCount << 1), 0, kOnScreenWhite);
	--gLockTrackCount;
}

