/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "platforms/mac/util/MachOCompatibility.h"

#include "modules/opdata/UniString.h"
#include "modules/hardcore/keys/opkeys.h"

// Last due to YES/NO type redefining.
#include "platforms/mac/util/macutils_mincore.h"

#define BOOL NSBOOL
#import <objc/objc-class.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSCursor.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSHTTPCookie.h>
#undef BOOL

//////////////////////////////////////////////////////////////////////

OperaNSReleasePool::OperaNSReleasePool()
{
	pool = [[NSAutoreleasePool alloc] init];
}

OperaNSReleasePool::~OperaNSReleasePool()
{
	[((NSAutoreleasePool*)pool) release];
}

//////////////////////////////////////////////////////////////////////

void SwizzleClassMethods(Class swizzle_class, SEL origSel, SEL newSel) 
{
	Method origMethod = class_getClassMethod(swizzle_class, origSel);
	Method newMethod = class_getClassMethod(swizzle_class, newSel);
	if (origMethod && newMethod)
		method_exchangeImplementations(origMethod, newMethod);
}

//////////////////////////////////////////////////////////////////////

BOOL SetUniString(UniString& dest, void *source)
{
	if (source)
	{
		NSString *string = (NSString *)source;
		int len = [string length];
		dest.Clear();
		[string getCharacters:(unichar*)dest.GetAppendPtr(len) range:NSMakeRange(0, len)];

		return TRUE;
	}
	
	return FALSE;
}

//////////////////////////////////////////////////////////////////////

void SetNSString(void **dest, const UniString& source)
{
	UniString   local_unistring(source);
	
	// UniString to NSString
	*dest = (void *)[[[NSString alloc] initWithCharacters:(const unichar *)local_unistring.Data(TRUE) length:local_unistring.Length()] autorelease];
}

//////////////////////////////////////////////////////////////////////

void ConvertFSRefToNSString(void **dest, const FSRef *fsref)
{
	CFURLRef url = CFURLCreateFromFSRef(kCFAllocatorDefault, fsref);
	*dest = (void *)[(NSURL *)url path];
	CFRelease(url);
}

//////////////////////////////////////////////////////////////////////

BOOL ConvertFSRefToUniString(UniString& dest, const FSRef *fsref)
{
	NSString *path;

	// Use the existing functions
	ConvertFSRefToNSString((void **)&path, fsref);
	return SetUniString(dest, (void *)path);
}

//////////////////////////////////////////////////////////////////////

int stricmp(const char* s1, const char* s2)
{
	return strcasecmp(s1,s2);
}

//////////////////////////////////////////////////////////////////////

int strnicmp(const char* s1, const char* s2, size_t n)
{
	return strncasecmp(s1,s2,n);
}

//////////////////////////////////////////////////////////////////////

void SetProcessName(const UniString &name)
{
	CFStringRef process_name;
	SetNSString((void **)&process_name, name);
	
	ProcessSerialNumber psn;
	if (::GetCurrentProcess(&psn) != noErr)
		return;

	CFTypeRef asn = LSGetCurrentApplicationASN();
	LSSetApplicationInformationItem(-2, asn, kLSDisplayNameKey, process_name, nil);
}

//////////////////////////////////////////////////////////////////////

// Not ideal placement but it's needed in Opera and the
// Plugin Wrapper
double GetMonotonicMS()
{
	unsigned long long usecs;
	Microseconds((UnsignedWide*)&usecs);
	//	return (((double)usecs) / ((double)1000.0));
	return (double)((usecs) / ((long long)1000)); // temp hack: Do not return fractional millisecs, as this can confuse core.
}

//////////////////////////////////////////////////////////////////////

// System builtin wcstombs is broken.
size_t mac_wcstombs(char * dest, const wchar_t * src, size_t max)
{
	const UInt32* src32 = (UInt32*)src;
	size_t i, len=0;
	UInt32 c;
	char *out_scan = dest;
	for (i=0;(c=src32[i])!=0 && len<max;i++) {
		if (c > 0x10FFFF)
			return (size_t)-1;
		else if (c > 0xFFFF && ((len+4) <= max)) {
			len += 4;
			if (out_scan) {
				*out_scan++ = 0xF0 | (c>>18);
				*out_scan++ = 0x80 | ((c>>12)&0x3f);
				*out_scan++ = 0x80 | ((c>>6)&0x3f);
				*out_scan++ = 0x80 | ((c)&0x3f);
			}
		}
		else if (c > 0x7FF && ((len+3) <= max)) {
			len += 3;
			if (out_scan) {
				*out_scan++ = 0xE0 | (c>>12);
				*out_scan++ = 0x80 | ((c>>6)&0x3f);
				*out_scan++ = 0x80 | ((c)&0x3f);
			}
		}
		else if (c > 0x7F && ((len+2) <= max)) {
			len += 2;
			if (out_scan) {
				*out_scan++ = 0xC0 | (c>>6);
				*out_scan++ = 0x80 | (c&0x3f);
			}
		}
		else {
			len++;
			if (out_scan)
				*out_scan++ = c;
		}
	}
	if (out_scan && (len<max)) {
		*out_scan++ = '\0';
	}
	return len;
}

//////////////////////////////////////////////////////////////////////

char *op_setlocale(int, const char *)
{
	return "";
}

//////////////////////////////////////////////////////////////////////

size_t mac_mbstowcs(wchar_t * dest, const char * src, size_t max)
{
	size_t len=0;
	UInt32* out_scan = (UInt32*)dest, d;
	char c;
	while ((c = *src++) && len<max) {
		if (!(c&0x80)) {					// 7 bits
			len++;
			if (out_scan) {
				*out_scan++ = c;
			}
		} else if (!(c&0x40)) {
			return (size_t)-1; // invalid, 10xx xxxx can only happen as second, third or fourth letter in a multibyte sequence.
		} else if (!(c&0x20)) {				// 11 bits
			len++;
			d = (c&0x1f)<<6;
			if (((c = *src++) & 0xC0) != 0x80)
				return (size_t)-1; // invalid, second byte in a 2-byte sequence needs to be 10xx xxxx
			d |= (c&0x3f);
			if (out_scan)
				*out_scan++ = d;
		} else if (!(c&0x10)) {				// 16 bits
			len++;
			d = (c&0x0f)<<12;
			if (((c = *src++) & 0xC0) != 0x80)
				return (size_t)-1; // invalid, second byte in a 3-byte sequence needs to be 10xx xxxx
			d |= (c&0x3f)<<6;
			if (((c = *src++) & 0xC0) != 0x80)
				return (size_t)-1; // invalid, third  byte in a 3-byte sequence needs to be 10xx xxxx
			d |= (c&0x3f);
			if (out_scan)
				*out_scan++ = d;
		} else if (!(c&0x08)) {				// 20 bits
			len++;
			d = (c&0x03)<<18;
			if (((c = *src++) & 0xC0) != 0x80)
				return (size_t)-1; // invalid, second byte in a 4-byte sequence needs to be 10xx xxxx
			d |= (c&0x3f)<<12;
			if (((c = *src++) & 0xC0) != 0x80)
				return (size_t)-1; // invalid, third  byte in a 4-byte sequence needs to be 10xx xxxx
			d |= (c&0x3f)<<6;
			if (((c = *src++) & 0xC0) != 0x80)
				return (size_t)-1; // invalid, fourth byte in a 4-byte sequence needs to be 10xx xxxx
			d |= (c&0x3f);
			if (out_scan)
				*out_scan++ = d;
		} else {
			return (size_t)-1; // invalid, this is a 5 or 6-byte sequnce, not allowed.
		}
	}
	if (out_scan && (len<max)) {
		*out_scan++ = '\0';
	}
	return len;
}

//////////////////////////////////////////////////////////////////////

BOOL IsDeadKey(uni_char c)
{
	switch (c)
	{
		case OP_KEY_UP:
		case OP_KEY_DOWN:
		case OP_KEY_LEFT:
		case OP_KEY_RIGHT:
		case OP_KEY_DELETE:
		case OP_KEY_HOME:
		case OP_KEY_END:
		case OP_KEY_PAGEUP:
		case OP_KEY_PAGEDOWN:
		case OP_KEY_ESCAPE:
		case OP_KEY_BACKSPACE:
		case OP_KEY_TAB:
		case OP_KEY_SPACE:
		case OP_KEY_ENTER:
		case OP_KEY_F1:
		case OP_KEY_F2:
		case OP_KEY_F3:
		case OP_KEY_F4:
		case OP_KEY_F5:
		case OP_KEY_F6:
		case OP_KEY_F7:
		case OP_KEY_F8:
		case OP_KEY_F9:
		case OP_KEY_F10:
		case OP_KEY_F11:
		case OP_KEY_F12:
		case OP_KEY_F13:
		case OP_KEY_F14:
		case OP_KEY_F15:
		case OP_KEY_F16:
			return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////

BOOL IsModifierUniKey(uni_char c)
{
	switch (c)
	{
		case OP_KEY_CTRL:
		case OP_KEY_SHIFT:
		case OP_KEY_ALT:
		case OP_KEY_MAC_CTRL:
		case OP_KEY_MAC_CTRL_LEFT:
		case OP_KEY_CONTEXT_MENU: // = OP_KEY_MAC_CTRL_RIGHT
			return TRUE;
	}
	return FALSE;
}


//////////////////////////////////////////////////////////////////////

UniChar OpKey2UniKey(uni_char c)
{
	switch (c)
	{
		case OP_KEY_UP:
			return NSUpArrowFunctionKey;
		case OP_KEY_DOWN:
			return NSDownArrowFunctionKey;
		case OP_KEY_LEFT:
			return NSLeftArrowFunctionKey;
		case OP_KEY_RIGHT:
			return NSRightArrowFunctionKey;
		case OP_KEY_DELETE:
			return NSDeleteFunctionKey;
		case OP_KEY_HOME:
			return NSHomeFunctionKey;
		case OP_KEY_END:
			return NSEndFunctionKey;
		case OP_KEY_PAGEUP:
			return NSPageUpFunctionKey;
		case OP_KEY_PAGEDOWN:
			return NSPageDownFunctionKey;
		case OP_KEY_ESCAPE:
			return 27;
		case OP_KEY_BACKSPACE:
			return 127;
		case OP_KEY_TAB:
			return 9;
		case OP_KEY_SPACE:
			return 32;
		case OP_KEY_ENTER:
			return 13;
		case OP_KEY_F1:
			return NSF1FunctionKey;
		case OP_KEY_F2:
			return NSF2FunctionKey;
		case OP_KEY_F3:
			return NSF3FunctionKey;
		case OP_KEY_F4:
			return NSF4FunctionKey;
		case OP_KEY_F5:
			return NSF5FunctionKey;
		case OP_KEY_F6:
			return NSF6FunctionKey;
		case OP_KEY_F7:
			return NSF7FunctionKey;
		case OP_KEY_F8:
			return NSF8FunctionKey;
		case OP_KEY_F9:
			return NSF9FunctionKey;
		case OP_KEY_F10:
			return NSF10FunctionKey;
		case OP_KEY_F11:
			return NSF11FunctionKey;
		case OP_KEY_F12:
			return NSF12FunctionKey;
		case OP_KEY_F13:
			return NSF13FunctionKey;
		case OP_KEY_F14:
			return NSF14FunctionKey;
		case OP_KEY_F15:
			return NSF15FunctionKey;
		case OP_KEY_F16:
			return NSF16FunctionKey;
		default:;
	}

	return c;
}

//////////////////////////////////////////////////////////////////////
