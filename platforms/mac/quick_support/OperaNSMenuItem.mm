/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/quick_support/OperaNSMenuItem.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 


@implementation OperaNSMenuItem

- (id)initWithAction:(OpInputAction*)action
{
	[super init];
	_input_action = action;
	return self;
}

- (OpInputAction *)getAction
{
	return _input_action;
}

@end

