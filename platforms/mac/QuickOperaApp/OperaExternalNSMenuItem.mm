/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/QuickOperaApp/OperaExternalNSMenuItem.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 

@implementation OperaExternalNSMenuItem

- (id)init
{
	[super init];
	_command = 0;
	return self;
}

- (id)initWithTitle:(NSString *)aString action:(SEL)aSelector keyEquivalent:(NSString *)charCode
{
	[super initWithTitle:aString action:aSelector keyEquivalent:charCode];
	_command = 0;
	return self;
}

- (void)setCommand:(EmQuickMenuCommand)command
{
	_command = command;
}

- (EmQuickMenuCommand)command
{
	return _command;
}

@end

