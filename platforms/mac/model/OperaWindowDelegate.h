/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPERA_WINDOW_DELEGATE_H
#define OPERA_WINDOW_DELEGATE_H

#define BOOL NSBOOL
#import <AppKit/NSWindow.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSGeometry.h>
#undef BOOL

class OpWindowListener;
@class NSWindow;

@interface OperaWindowDelegate : NSObject <NSWindowDelegate>
{
	OpWindowListener *windowlistener;
}
- (void)setListener:(OpWindowListener*)listener;
- (OpWindowListener*)getListener;
@end

#endif // OPERA_WINDOW_DELEGATE_H
