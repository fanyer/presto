/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/MachOCompatibility.h"
#include "platforms/mac/util/systemcapabilities.h"

OSStatus LoadFrameworkBundle(CFStringRef framework, CFBundleRef *bundlePtr)
{
	OSStatus err;
	FSRef frameworksFolderRef;
	CFURLRef baseURL;
	CFURLRef bundleURL;


	// Set the bundle to nil
	if(bundlePtr == nil)
		return coreFoundationUnknownErr;

	*bundlePtr = nil;

	// Initialize the base and bundle URLs to nil
	baseURL = nil;
	bundleURL = nil;

	// Find the frameworks folder and create a URL for it
	err = FSFindFolder(kOnAppropriateDisk, kFrameworksFolderType, true, &frameworksFolderRef);
	if(err == noErr)
	{
		baseURL = CFURLCreateFromFSRef(kCFAllocatorSystemDefault, &frameworksFolderRef);
		if(!baseURL)
			err = coreFoundationUnknownErr;
	}

	// Append the name of the framework to the URL
	if(err == noErr)
	{
		bundleURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorSystemDefault, baseURL, framework, false);
		if(!bundleURL)
			err = coreFoundationUnknownErr;
	}

	// Create a bundle based on that URL and load the bundle into memory
	// We never unload the bundle, hwich is reasonable in this case because
	// it is assumed that we'll be using the functions linked throughout the
	// application
	if(err == noErr)
	{
		*bundlePtr = CFBundleCreate(kCFAllocatorSystemDefault, bundleURL);
		if(!*bundlePtr)
			err = coreFoundationUnknownErr;
	}

	if(err == noErr)
	{
		if(!CFBundleLoadExecutable(*bundlePtr))
			err = coreFoundationUnknownErr;
	}

	// Clean up
	if(err != noErr && *bundlePtr != nil)
	{
		CFRelease(*bundlePtr);
		*bundlePtr = nil;
	}

	if(bundleURL != nil)
		CFRelease(bundleURL);

	if(baseURL != nil)
		CFRelease(baseURL);

	return err;
}

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
CFB_DECLARE_PTR(HIThemeBeginFocus);
CFB_DECLARE_PTR(HIThemeEndFocus);
#endif

#ifdef NO_CARBON
CFB_DECLARE_DATAPTR(CFStringRef, kTISPropertyUnicodeKeyLayoutData);
# ifndef SIXTY_FOUR_BIT
CFB_DECLARE_PTR(CreateNewWindow);
CFB_DECLARE_PTR(ReleaseWindow);
CFB_DECLARE_PTR(GetWindowPort);
CFB_DECLARE_PTR(SetPortWindowPort);
CFB_DECLARE_PTR(NewEventHandlerUPP);
CFB_DECLARE_PTR(GetApplicationEventTarget);
CFB_DECLARE_PTR(GetWindowEventTarget);
CFB_DECLARE_PTR(InstallEventHandler);
CFB_DECLARE_PTR(RemoveEventHandler);
CFB_DECLARE_PTR(GetEventParameter);
# endif // SIXTY_FOUR_BIT
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
CFB_DECLARE_PTR(HIShapeGetBounds);
#endif
CFB_DECLARE_PTR(SetSystemUIMode);
CFB_DECLARE_PTR(GetSystemUIMode);
CFB_DECLARE_PTR(GetThemeMetric);
CFB_DECLARE_PTR(GetThemeScrollBarArrowStyle);
CFB_DECLARE_PTR(HIThemeHitTestScrollBarArrows);
CFB_DECLARE_PTR(HIThemeHitTestTrack);
CFB_DECLARE_PTR(HIThemeDrawTabPane);
CFB_DECLARE_PTR(HIThemeDrawBackground);
CFB_DECLARE_PTR(HIThemeDrawTab);
CFB_DECLARE_PTR(HIThemeDrawPopupArrow);
CFB_DECLARE_PTR(HIThemeDrawFrame);
CFB_DECLARE_PTR(HIThemeDrawChasingArrows);
CFB_DECLARE_PTR(HIThemeDrawHeader);
CFB_DECLARE_PTR(HIThemeDrawPlacard);
CFB_DECLARE_PTR(HIThemeDrawGroupBox);
CFB_DECLARE_PTR(HIThemeDrawButton);
CFB_DECLARE_PTR(HIThemeDrawTrack);
CFB_DECLARE_PTR(HIThemeDrawFocusRect);
CFB_DECLARE_PTR(HIThemeDrawGenericWell);
CFB_DECLARE_PTR(HIThemeDrawTextBox);
CFB_DECLARE_PTR(HIThemeGetTrackThumbShape);
CFB_DECLARE_PTR(HIThemeGetTrackBounds);
CFB_DECLARE_PTR(HIThemeGetGrowBoxBounds);
CFB_DECLARE_PTR(HIThemeBrushCreateCGColor);
CFB_DECLARE_PTR(TISCopyCurrentKeyboardLayoutInputSource);
CFB_DECLARE_PTR(TISGetInputSourceProperty);
CFB_DECLARE_PTR(KeyTranslate);
CFB_DECLARE_PTR(KeyScript);
CFB_DECLARE_PTR(GetApplicationTextEncoding);
CFB_DECLARE_PTR(LMGetKbdType);
CFB_DECLARE_PTR(KLGetCurrentKeyboardLayout);
CFB_DECLARE_PTR(KLGetKeyboardLayoutProperty);
CFB_DECLARE_PTR(EnableSecureEventInput);
CFB_DECLARE_PTR(DisableSecureEventInput);
CFB_DECLARE_PTR(IsSecureEventInputEnabled);
CFB_DECLARE_PTR(TISCopyCurrentKeyboardInputSource);
CFB_DECLARE_PTR(TISCopyCurrentASCIICapableKeyboardLayoutInputSource);
CFB_DECLARE_PTR(TISSetInputMethodKeyboardLayoutOverride);
CFB_DECLARE_PTR(TISSelectInputSource);
#endif

CFB_DECLARE_PTR(CGContextSetCompositeOperation);
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
#ifndef NP_NO_QUICKDRAW
CFB_DECLARE_PTR(GetRegionBounds);
CFB_DECLARE_PTR(DisposeGWorld);
CFB_DECLARE_PTR(SetGWorld);
CFB_DECLARE_PTR(NewRgn);
CFB_DECLARE_PTR(SetRectRgn);
CFB_DECLARE_PTR(DisposeRgn);
CFB_DECLARE_PTR(UnionRgn);
CFB_DECLARE_PTR(EmptyRgn);
CFB_DECLARE_PTR(SectRgn);
CFB_DECLARE_PTR(EqualRect);
CFB_DECLARE_PTR(EmptyRect);
CFB_DECLARE_PTR(OffsetRect);
CFB_DECLARE_PTR(GetGWorldPixMap);
CFB_DECLARE_PTR(LockPixels);
CFB_DECLARE_PTR(UnlockPixels);
CFB_DECLARE_PTR(CopyBits);
CFB_DECLARE_PTR(NewGWorld);
CFB_DECLARE_PTR(LMGetMainDevice);
CFB_DECLARE_PTR(GetPortBounds);
#endif // !NP_NO_QUICKDRAW
CFB_DECLARE_PTR(CGDisplayBitsPerPixel);
#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7

// --------------------------

void InitializeCoreGraphics()
{
	static Boolean inited = false;
	if (inited)
		return;

	inited = true;

	CFBundleRef sysBundle;

	// !!!! WARNING !!!! WE CHANGE THE sysBundle PTR HERE!
	LoadFrameworkBundle(CFSTR("Carbon.framework"), &sysBundle);
	if (sysBundle)
	{
#undef CGContextSetCompositeOperation
		CFB_INIT_PRIVATE_FUNCTION(CGContextSetCompositeOperation)

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
		CFB_INIT_FUNCTION(HIThemeBeginFocus);
		CFB_INIT_FUNCTION(HIThemeEndFocus);
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
#ifndef NP_NO_QUICKDRAW
#undef GetRegionBounds
#undef DisposeGWorld
#undef SetGWorld
#undef NewRgn
#undef SetRectRgn
#undef DisposeRgn
#undef UnionRgn
#undef EmptyRgn
#undef SectRgn
#undef EqualRect
#undef OffsetRect
#undef GetGWorldPixMap
#undef LockPixels
#undef UnlockPixels
#undef CopyBits
#undef NewGWorld
#undef LMGetMainDevice
#undef GetPortBounds
		CFB_INIT_PRIVATE_FUNCTION(GetRegionBounds);
		CFB_INIT_PRIVATE_FUNCTION(DisposeGWorld);
		CFB_INIT_PRIVATE_FUNCTION(SetGWorld);
		CFB_INIT_PRIVATE_FUNCTION(NewRgn);
		CFB_INIT_PRIVATE_FUNCTION(SetRectRgn);
		CFB_INIT_PRIVATE_FUNCTION(DisposeRgn);
		CFB_INIT_PRIVATE_FUNCTION(UnionRgn);
		CFB_INIT_PRIVATE_FUNCTION(EmptyRgn);
		CFB_INIT_PRIVATE_FUNCTION(SectRgn);
		CFB_INIT_PRIVATE_FUNCTION(EqualRect);
		CFB_INIT_PRIVATE_FUNCTION(EmptyRect);
		CFB_INIT_PRIVATE_FUNCTION(OffsetRect);
		CFB_INIT_PRIVATE_FUNCTION(GetGWorldPixMap);
		CFB_INIT_PRIVATE_FUNCTION(LockPixels);
		CFB_INIT_PRIVATE_FUNCTION(UnlockPixels);
		CFB_INIT_PRIVATE_FUNCTION(CopyBits);
		CFB_INIT_PRIVATE_FUNCTION(NewGWorld);
		CFB_INIT_PRIVATE_FUNCTION(LMGetMainDevice);
		CFB_INIT_PRIVATE_FUNCTION(GetPortBounds);
#endif // !NP_NO_QUICKDRAW
#undef CGDisplayBitsPerPixel
CFB_INIT_PRIVATE_FUNCTION(CGDisplayBitsPerPixel);
if (!CGDisplayBitsPerPixelPtr)
	CGDisplayBitsPerPixelPtr = CGDisplayBitsPerPixelLocalImpl;

#endif // MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
		
#ifdef NO_CARBON
#ifdef SIXTY_FOUR_BIT
		CFB_INIT_DATAPTR(CFStringRef, kTISPropertyUnicodeKeyLayoutData);
#endif
# ifndef SIXTY_FOUR_BIT
#  ifdef NO_CARBON_HEADERS
		CFB_INIT_FUNCTION(GetWindowPort);
		CFB_INIT_FUNCTION(SetPortWindowPort);
		CFB_INIT_FUNCTION(NewEventHandlerUPP);
		CFB_INIT_FUNCTION(GetApplicationEventTarget);
		CFB_INIT_FUNCTION(GetWindowEventTarget);
		CFB_INIT_FUNCTION(InstallEventHandler);
		CFB_INIT_FUNCTION(RemoveEventHandler);
		CFB_INIT_FUNCTION(GetEventParameter);
#  else // NO_CARBON_HEADERS
#undef CreateNewWindow
#undef ReleaseWindow
#undef GetWindowPort
#undef SetPortWindowPort
#undef NewEventHandlerUPP
#undef GetApplicationEventTarget
#undef GetWindowEventTarget
#undef InstallEventHandler
#undef RemoveEventHandler
#undef GetEventParameter
		CFB_INIT_PRIVATE_FUNCTION(CreateNewWindow);
		CFB_INIT_PRIVATE_FUNCTION(ReleaseWindow);
		CFB_INIT_PRIVATE_FUNCTION(GetWindowPort);
		CFB_INIT_PRIVATE_FUNCTION(SetPortWindowPort);
		CFB_INIT_PRIVATE_FUNCTION(NewEventHandlerUPP);
		CFB_INIT_PRIVATE_FUNCTION(GetApplicationEventTarget);
		CFB_INIT_PRIVATE_FUNCTION(GetWindowEventTarget);
		CFB_INIT_PRIVATE_FUNCTION(InstallEventHandler);
		CFB_INIT_PRIVATE_FUNCTION(RemoveEventHandler);
		CFB_INIT_PRIVATE_FUNCTION(GetEventParameter);
#  endif // NO_CARBON_HEADERS
# endif // SIXTY_FOUR_BIT
# ifdef NO_CARBON_HEADERS
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
		CFB_INIT_FUNCTION(HIShapeGetBounds);
#endif
		CFB_INIT_FUNCTION(SetSystemUIMode);
		CFB_INIT_FUNCTION(GetSystemUIMode);
		CFB_INIT_FUNCTION(GetThemeMetric);
		CFB_INIT_FUNCTION(GetThemeScrollBarArrowStyle);
		CFB_INIT_FUNCTION(HIThemeHitTestScrollBarArrows);
		CFB_INIT_FUNCTION(HIThemeHitTestTrack);
		CFB_INIT_FUNCTION(HIThemeDrawTabPane);
		CFB_INIT_FUNCTION(HIThemeDrawBackground);
		CFB_INIT_FUNCTION(HIThemeDrawTab);
		CFB_INIT_FUNCTION(HIThemeDrawPopupArrow);
		CFB_INIT_FUNCTION(HIThemeDrawFrame);
		CFB_INIT_FUNCTION(HIThemeDrawChasingArrows);
		CFB_INIT_FUNCTION(HIThemeDrawHeader);
		CFB_INIT_FUNCTION(HIThemeDrawPlacard);
		CFB_INIT_FUNCTION(HIThemeDrawGroupBox);
		CFB_INIT_FUNCTION(HIThemeDrawButton);
		CFB_INIT_FUNCTION(HIThemeDrawTrack);
		CFB_INIT_FUNCTION(HIThemeDrawFocusRect);
		CFB_INIT_FUNCTION(HIThemeDrawGenericWell);
		CFB_INIT_FUNCTION(HIThemeDrawTextBox);
		CFB_INIT_FUNCTION(HIThemeGetTrackThumbShape);
		CFB_INIT_FUNCTION(HIThemeGetTrackBounds);
		CFB_INIT_FUNCTION(HIThemeGetGrowBoxBounds);
		CFB_INIT_FUNCTION(HIThemeBrushCreateCGColor);
		CFB_INIT_FUNCTION(TISCopyCurrentKeyboardLayoutInputSource);
		CFB_INIT_FUNCTION(TISGetInputSourceProperty);
		CFB_INIT_FUNCTION(KeyTranslate);
		CFB_INIT_FUNCTION(KeyScript);
		CFB_INIT_FUNCTION(GetApplicationTextEncoding);
		CFB_INIT_FUNCTION(LMGetKbdType);
		CFB_INIT_FUNCTION(KLGetCurrentKeyboardLayout);
		CFB_INIT_FUNCTION(KLGetKeyboardLayoutProperty);
		CFB_INIT_FUNCTION(EnableSecureEventInput);
		CFB_INIT_FUNCTION(DisableSecureEventInput);
		CFB_INIT_FUNCTION(IsSecureEventInputEnabled);
		CFB_INIT_FUNCTION(TISCopyCurrentKeyboardInputSource);
		CFB_INIT_FUNCTION(TISCopyCurrentASCIICapableKeyboardLayoutInputSource);
		CFB_INIT_FUNCTION(TISSetInputMethodKeyboardLayoutOverride);
		CFB_INIT_FUNCTION(TISSelectInputSource);
# else // NO_CARBON_HEADERS
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
#undef HIShapeGetBounds
#endif
#undef SetSystemUIMode
#undef GetSystemUIMode
#undef GetThemeMetric
#undef GetThemeScrollBarArrowStyle
#undef HIThemeHitTestScrollBarArrows
#undef HIThemeHitTestTrack
#undef HIThemeDrawTabPane
#undef HIThemeDrawBackground
#undef HIThemeDrawTab
#undef HIThemeDrawPopupArrow
#undef HIThemeDrawFrame
#undef HIThemeDrawChasingArrows
#undef HIThemeDrawHeader
#undef HIThemeDrawPlacard
#undef HIThemeDrawGroupBox
#undef HIThemeDrawButton
#undef HIThemeDrawTrack
#undef HIThemeDrawFocusRect
#undef HIThemeDrawGenericWell
#undef HIThemeDrawTextBox
#undef HIThemeGetTrackThumbShape
#undef HIThemeGetTrackBounds
#undef HIThemeGetGrowBoxBounds
#undef HIThemeBrushCreateCGColor
#undef TISCopyCurrentKeyboardLayoutInputSource
#undef TISGetInputSourceProperty
#undef KeyTranslate
#undef KeyScript
#undef GetApplicationTextEncoding
#undef LMGetKbdType
#undef KLGetCurrentKeyboardLayout
#undef KLGetKeyboardLayoutProperty
#undef EnableSecureEventInput
#undef DisableSecureEventInput
#undef IsSecureEventInputEnabled
#undef TISCopyCurrentKeyboardInputSource
#undef TISCopyCurrentASCIICapableKeyboardLayoutInputSource
#undef TISSetInputMethodKeyboardLayoutOverride
#undef TISSelectInputSource
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
		CFB_INIT_PRIVATE_FUNCTION(HIShapeGetBounds);
#endif
		CFB_INIT_PRIVATE_FUNCTION(SetSystemUIMode);
		CFB_INIT_PRIVATE_FUNCTION(GetSystemUIMode);
		CFB_INIT_PRIVATE_FUNCTION(GetThemeMetric);
		CFB_INIT_PRIVATE_FUNCTION(GetThemeScrollBarArrowStyle);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeHitTestScrollBarArrows);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeHitTestTrack);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawTabPane);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawBackground);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawTab);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawPopupArrow);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawFrame);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawChasingArrows);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawHeader);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawPlacard);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawGroupBox);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawButton);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawTrack);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawFocusRect);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawGenericWell);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeDrawTextBox);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeGetTrackThumbShape);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeGetTrackBounds);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeGetGrowBoxBounds);
		CFB_INIT_PRIVATE_FUNCTION(HIThemeBrushCreateCGColor);
		CFB_INIT_PRIVATE_FUNCTION(TISCopyCurrentKeyboardLayoutInputSource);
		CFB_INIT_PRIVATE_FUNCTION(TISGetInputSourceProperty);
		CFB_INIT_PRIVATE_FUNCTION(KeyTranslate);
		CFB_INIT_PRIVATE_FUNCTION(KeyScript);
		CFB_INIT_PRIVATE_FUNCTION(GetApplicationTextEncoding);
		CFB_INIT_PRIVATE_FUNCTION(LMGetKbdType);
		CFB_INIT_PRIVATE_FUNCTION(KLGetCurrentKeyboardLayout);
		CFB_INIT_PRIVATE_FUNCTION(KLGetKeyboardLayoutProperty);
		CFB_INIT_PRIVATE_FUNCTION(EnableSecureEventInput);
		CFB_INIT_PRIVATE_FUNCTION(DisableSecureEventInput);
		CFB_INIT_PRIVATE_FUNCTION(IsSecureEventInputEnabled);
		CFB_INIT_PRIVATE_FUNCTION(TISCopyCurrentKeyboardInputSource);
		CFB_INIT_PRIVATE_FUNCTION(TISCopyCurrentASCIICapableKeyboardLayoutInputSource);
		CFB_INIT_PRIVATE_FUNCTION(TISSetInputMethodKeyboardLayoutOverride);
		CFB_INIT_PRIVATE_FUNCTION(TISSelectInputSource);
# endif // NO_CARBON_HEADERS
#endif // NO_CARBON
	}
}

// IOSurface

CFB_DECLARE_DATAPTR(CFStringRef, kIOSurfaceIsGlobal);
CFB_DECLARE_DATAPTR(CFStringRef, kIOSurfaceWidth);
CFB_DECLARE_DATAPTR(CFStringRef, kIOSurfaceHeight);
CFB_DECLARE_DATAPTR(CFStringRef, kIOSurfaceBytesPerElement);


CFB_DECLARE_PTR(IOSurfaceCreate);
CFB_DECLARE_PTR(IOSurfaceLookup);
CFB_DECLARE_PTR(IOSurfaceGetWidth);
CFB_DECLARE_PTR(IOSurfaceGetHeight);
CFB_DECLARE_PTR(IOSurfaceGetID);


CFB_DECLARE_PTR(CGLTexImageIOSurface2D);


void InitializeIOSurface()
{
	static Boolean inited_iosurface = false;
	if (inited_iosurface)
		return;
	
	inited_iosurface = true;

	CFBundleRef sysBundle;
	
	// !!!! WARNING !!!! WE CHANGE THE sysBundle PTR HERE!
	LoadFrameworkBundle(CFSTR("IOSurface.framework"), &sysBundle);
	if (sysBundle)
	{
#undef kIOSurfaceIsGlobal
#undef kIOSurfaceWidth
#undef kIOSurfaceHeight
#undef kIOSurfaceBytesPerElement
		CFB_INIT_PRIVATE_DATAPTR(CFStringRef, kIOSurfaceIsGlobal);
		CFB_INIT_PRIVATE_DATAPTR(CFStringRef, kIOSurfaceWidth);
		CFB_INIT_PRIVATE_DATAPTR(CFStringRef, kIOSurfaceHeight);
		CFB_INIT_PRIVATE_DATAPTR(CFStringRef, kIOSurfaceBytesPerElement);
		
		
#undef IOSurfaceCreate
#undef IOSurfaceLookup
#undef IOSurfaceGetWidth
#undef IOSurfaceGetHeight
#undef IOSurfaceGetID
		CFB_INIT_PRIVATE_FUNCTION(IOSurfaceCreate);
		CFB_INIT_PRIVATE_FUNCTION(IOSurfaceLookup);
		CFB_INIT_PRIVATE_FUNCTION(IOSurfaceGetWidth);
		CFB_INIT_PRIVATE_FUNCTION(IOSurfaceGetHeight);
		CFB_INIT_PRIVATE_FUNCTION(IOSurfaceGetID);
		
	}
	
	LoadFrameworkBundle(CFSTR("OpenGL.framework"), &sysBundle);
	if (sysBundle)
	{
#undef CGLTexImageIOSurface2D
		CFB_INIT_PRIVATE_FUNCTION(CGLTexImageIOSurface2D);
	}
}

CFB_DECLARE_DATAPTR(CFStringRef, _kLSDisplayNameKey);

CFB_DECLARE_PTR(_LSGetCurrentApplicationASN);
CFB_DECLARE_PTR(_LSSetApplicationInformationItem);

void InitializeLaunchServices()
{
	static Boolean inited_launchservices = false;
	if (inited_launchservices)
		return;
	
	inited_launchservices = true;
	
	CFBundleRef sysBundle;
	
	// !!!! WARNING !!!! WE CHANGE THE sysBundle PTR HERE!
	// LaunchServices is buried inside of CoreServices
	LoadFrameworkBundle(CFSTR("CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework"), &sysBundle);
	if (sysBundle)
	{
		CFB_INIT_DATAPTR(CFStringRef, _kLSDisplayNameKey);
		
		CFB_INIT_FUNCTION(_LSGetCurrentApplicationASN);
		CFB_INIT_FUNCTION(_LSSetApplicationInformationItem);
	}
}

size_t CGDisplayBitsPerPixelLocalImpl(CGDirectDisplayID displayId)
{
	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayId);
	size_t depth = 0;
	CFStringRef pixEnc = CGDisplayModeCopyPixelEncoding(mode);
	if (CFStringCompare(pixEnc, CFSTR(IO32BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		depth = 32;
	else if (CFStringCompare(pixEnc, CFSTR(IO16BitDirectPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		depth = 16;
	else if (CFStringCompare(pixEnc, CFSTR(IO8BitIndexedPixels), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
		depth = 8;
	CFRelease(pixEnc);
	CFRelease(mode);
	return depth;
}
