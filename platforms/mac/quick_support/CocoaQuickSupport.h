/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef _COCOAQUICKSUPPORT_H_
#define _COCOAQUICKSUPPORT_H_

class OpWidgetImage;
class Image;

CGImageRef CreateCGImageFromOpBitmap(OpBitmap *bm);
void *GetImageFromBuffer(const UINT32 *data, int width, int height);
void *GetNSImageFromOpWidgetImage(OpWidgetImage *image);
void *GetNSImageFromImage(Image *image);
void *GetNSImageFromOpBitmap(OpBitmap *bitmap);
void *GetNSWindowWithNumber(int num);
CFRunLoopRef GetNSCFRunLoop();

void ShowSpecialCharactersPalette();

#ifdef NO_CARBON
void GetNSGlobalMouse(Point* globalMouse);
#define GetGlobalMouse GetNSGlobalMouse
OSStatus NSRestoreApplicationDockTileImage();
#define RestoreApplicationDockTileImage NSRestoreApplicationDockTileImage
double GetNSCurrentEventTime(void);
#define GetCurrentEventTime GetNSCurrentEventTime
#endif // NO_CARBON
void Log(void* exception, const char* where);
CGContextRef GetCurrentCGContext();


#if __LP64__ || NS_BUILD_32_LIKE_64
typedef long NSInteger;
typedef unsigned long NSUInteger;
#else
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

class AutoReleaser
{
public:
	AutoReleaser();
	~AutoReleaser();
private:
	void* arp;
};

#endif //_COCOAQUICKSUPPORT_H_
