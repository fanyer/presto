/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef BOTTOMLINEINPUT_H
#define BOTTOMLINEINPUT_H

#include "platforms/mac/model/OperaNSView.h"

#define BOOL NSBOOL
#import <AppKit/NSPanel.h>
#import <AppKit/NSTextView.h>
#undef BOOL

struct OpMessageAddress;

@interface BottomLineInput : NSPanel
{
	NSTextView			*_nsTextView;
	BOOL				_visible;
	BOOL				_shouldBeShown;
#ifdef NS4P_COMPONENT_PLUGINS
	OpMessageAddress	_destMessageAddress;
#else // NS4P_COMPONENT_PLUGINS
    void                *_macPluginWindow;
#endif // NS4P_COMPONENT_PLUGINS
	OperaNSView			*_operaNSView;
}

+ (BottomLineInput*)bottomLineInput;

#ifdef NS4P_COMPONENT_PLUGINS
- (void)setDestMessageAddress:(const OpMessageAddress &)addr;
#else // NS4P_COMPONENT_PLUGINS
- (void)setPluginWindow:(void *)macPluginWindow;
#endif // NS4P_COMPONENT_PLUGINS
- (void)sendText:(NSString *)string;

- (void)setMarkedText:(id)aString selectedRange:(NSRange)selRange;

- (void)shouldShow;
- (BOOL)shouldBeShown;

- (void)setOperaNSView:(OperaNSView*)view;

- (NSRange)getMarkedRange;

- (void)show:(BOOL)show;
- (BOOL)isVisible;

@end

#endif // BOTTOMLINEINPUT_H
