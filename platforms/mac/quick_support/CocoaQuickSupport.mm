/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "modules/skin/OpWidgetImage.h"
#include "modules/pi/OpBitmap.h"
#include "modules/display/vis_dev.h"
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/CocoaVegaDefines.h"

#define BOOL NSBOOL
#import <objc/objc-class.h>
#import <AppKit/AppKit.h>
#undef BOOL

#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_5
#include <objc/runtime.h>
#else
#define method_getImplementation(m__)	((m__)->method_imp)
#define method_setImplementation(m__, imp__) ((m__)->method_imp = (imp__))
#define method_getName(m__)		((m__)->method_name)
#endif

#define kMenuIconSize 16

#ifdef VEGA_INTERNAL_FORMAT_RGBA8888
#define PIXEL_SWAP(a) (((a)>>24)|(((a)&0xffffff)<<8))
#else
#define PIXEL_SWAP(a) (((a)>>24)|(((a)>>8)&0xff00)|(((a)&0xff00)<<8)|(((a)&0xff)<<24))
#endif

void *GetImageFromBuffer(const UINT32 *data, int width, int height)
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	NSImage *img = nil;
	
	NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **)NULL pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO 
																colorSpaceName:NSCalibratedRGBColorSpace bitmapFormat:NSAlphaFirstBitmapFormat
																   bytesPerRow:(4*width) bitsPerPixel:32];

	if (rep)
	{
		for (int i = 0; i<width*height; i++)
		{
			UINT32 pixel = data[i];
			pixel = PIXEL_SWAP(pixel);
			((UINT32 *)[rep bitmapData])[i] = pixel;
		}
		
		img = [[NSImage alloc] init];
		[img addRepresentation:rep];
		[rep release];
	}
	
	[pool release];
	return img;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void *GetNSImageFromOpWidgetImage(OpWidgetImage *image)
{
	void *rtn = NULL;
	
	if (image)
	{
		OpBitmap* opbmp;
		if (OpStatus::IsSuccess(OpBitmap::Create(&opbmp, kMenuIconSize, kMenuIconSize, FALSE, TRUE, 0, 0, TRUE)))
		{
			// Clear the bitmap to transparent
			opbmp->SetColor(NULL, TRUE, NULL);
			OpPainter* painter = opbmp->GetPainter();
			if (painter)
			{
				VisualDevice vd;
				vd.painter = painter;
				image->Draw(&vd, OpRect(0,0,kMenuIconSize, kMenuIconSize));
				rtn = GetImageFromBuffer((UINT32*)opbmp->GetPointer(OpBitmap::ACCESS_READONLY), kMenuIconSize, kMenuIconSize);
				opbmp->ReleasePointer(FALSE);
				opbmp->ReleasePainter();
			}
			OP_DELETE(opbmp);
		}
	}
	
	return rtn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void *GetNSImageFromImage(Image *image)
{
	void *rtn = NULL;
	
	if (image)
	{
		OpBitmap* opbmp;
		if (OpStatus::IsSuccess(OpBitmap::Create(&opbmp, image->Width(), image->Height(), FALSE, TRUE, 0, 0, TRUE)))
		{
			// Clear the bitmap to transparent
			opbmp->SetColor(NULL, TRUE, NULL);
			OpPainter* painter = opbmp->GetPainter();
			if (painter)
			{
				VisualDevice vd;
				vd.painter = painter;
				vd.ImageOut(*image, OpRect(0, 0, image->Width(), image->Height()), OpRect(0, 0, image->Width(), image->Height()));
				rtn = GetImageFromBuffer((UINT32*)opbmp->GetPointer(OpBitmap::ACCESS_READONLY), image->Width(), image->Height());
				opbmp->ReleasePointer(FALSE);
				opbmp->ReleasePainter();
			}
			OP_DELETE(opbmp);
		}
	}
	
	return rtn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void *GetNSImageFromOpBitmap(OpBitmap *bitmap)
{
	void *rtn = NULL;

	if (bitmap)
	{
		if (bitmap->GetBpp() == 32)
		{
			rtn = GetImageFromBuffer((UINT32*)bitmap->GetPointer(OpBitmap::ACCESS_READONLY), bitmap->Width(), bitmap->Height());
			bitmap->ReleasePointer(FALSE);
		}
		else
		{
			OpBitmap* opbmp;
			if (OpStatus::IsSuccess(OpBitmap::Create(&opbmp, bitmap->Width(), bitmap->Height(), FALSE, TRUE, 0, 0, TRUE)))
			{
				// Clear the bitmap to transparent
				opbmp->SetColor(NULL, TRUE, NULL);
				OpPainter* painter = opbmp->GetPainter();
				if (painter)
				{
					VisualDevice vd;
					vd.painter = painter;
					painter->DrawBitmap(bitmap, OpPoint(0,0));
					rtn = GetImageFromBuffer((UINT32*)opbmp->GetPointer(OpBitmap::ACCESS_READONLY), bitmap->Width(), bitmap->Height());
					opbmp->ReleasePointer(FALSE);
					opbmp->ReleasePainter();
				}
				OP_DELETE(opbmp);
			}
		}
	}

	return rtn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void *GetNSWindowWithNumber(int num)
{
	NSInteger numberOfWindows = 0, contextID;
	if ([NSApp respondsToSelector:@selector(contextID)])
		contextID = (NSInteger)[NSApp performSelector:@selector(contextID)];
	else
		return nil;

	NSCountWindowsForContext(contextID, &numberOfWindows);

	if(numberOfWindows > 0)
	{
		NSInteger *listOfWindows = new NSInteger[numberOfWindows];
		NSWindowListForContext(contextID, numberOfWindows, listOfWindows);

		NSWindow *win = [NSApp windowWithWindowNumber:listOfWindows[num]];

		delete [] listOfWindows;
		return win;
	}
	return NULL;
}

CFRunLoopRef GetNSCFRunLoop()
{
	// mainRunLoop only available from Mac OS X 10.5
	static Method method = class_getClassMethod([NSRunLoop class], @selector(mainRunLoop));
	if (method && method_getImplementation(method)) {
		NSRunLoop* loop = method_getImplementation(method)(nil, method_getName(method));
		return [loop getCFRunLoop];
	}
	return [[NSRunLoop currentRunLoop] getCFRunLoop];
}

void ShowSpecialCharactersPalette()
{
	[NSApp orderFrontCharacterPalette:nil];
}

#ifdef NO_CARBON
void GetNSGlobalMouse(Point* globalMouse)
{
	NSPoint where = [NSEvent mouseLocation];
	globalMouse->h = where.x;
	globalMouse->v = [[[NSScreen screens] objectAtIndex:0] frame].size.height - where.y;
}

OSStatus NSRestoreApplicationDockTileImage()
{
	[[NSApplication sharedApplication] setApplicationIconImage: nil];
	return noErr;
}

double GetNSCurrentEventTime()
{
	return [NSDate timeIntervalSinceReferenceDate];
}
#endif // NO_CARBON

#if defined(_MAC_DEBUG) && defined(NO_CARBON)
// Needed by OnScreen.cpp
short GetMenuBarHeight()
{
	return [[NSApp mainMenu] menuBarHeight];
}
#endif

AutoReleaser::AutoReleaser()
{
	arp = [[NSAutoreleasePool alloc] init];
}

AutoReleaser::~AutoReleaser()
{
	[((NSAutoreleasePool*) arp) release];
}

static const void * GetBytePointer(void *info)
{
	return((OpBitmap *)info)->GetPointer();
}

static void FreeBitmap(void *info)
{
	// If Opera is already gone it is too late to free these, there might be connections into e.g VEGABackend_HW3D
	if (g_opera)
		delete (OpBitmap *)info;
}

static CGDataProviderDirectCallbacks providerCallbacks = {
	0,
	GetBytePointer,
	NULL,
	NULL,
	FreeBitmap
};

// The OpBitmap passed in is now owned by the returned CGImageRef
CGImageRef CreateCGImageFromOpBitmap(OpBitmap *bm)
{
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	int w = bm->Width();
	int h = bm->Height();
	int bpl = bm->GetBytesPerLine();
	CGDataProviderRef provider = CGDataProviderCreateDirect(bm, h*bpl, &providerCallbacks);
	CGBitmapInfo alpha = kCGBitmapByteOrderVegaInternal;
	CGImageRef image = CGImageCreate(w, h, 8, 32, bpl, colorSpace, (CGImageAlphaInfo)alpha, provider, NULL, 0, kCGRenderingIntentAbsoluteColorimetric);

	CGDataProviderRelease(provider);
	CGColorSpaceRelease(colorSpace);
	return image;
}					
					
void Log(void* exception, const char* where)
{
	NSException* exp = (NSException*)exception;
	OpString op_exp_name, op_exp_reason;
	OpString8 op_exp_name8, op_exp_reason8;
	SetOpString(op_exp_name, (CFStringRef)[exp name]);
	SetOpString(op_exp_reason, (CFStringRef)[exp reason]);
	op_exp_name8.Set(op_exp_name); op_exp_reason8.Set(op_exp_reason);
	OpString8 debug_msg;
	debug_msg.AppendFormat("Exception: \"%s\" reason: \"%s\" caught in: %s\n", op_exp_name8.CStr(), op_exp_reason8.CStr(), where);
	printf("%s", debug_msg.CStr());
}


CGContextRef GetCurrentCGContext() {
	return (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
}
