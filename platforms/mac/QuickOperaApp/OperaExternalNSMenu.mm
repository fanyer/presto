/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/QuickOperaApp/OperaExternalNSMenu.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 

@implementation OperaExternalNSMenu


- (id)initWithTitle:(NSString *)aTitle
{
	self = [super initWithTitle:aTitle];
	_opera_menu_name = nil;
	return self;
}

- (void)setOperaMenuName:(NSString *)operaMenuName
{
	if (_opera_menu_name)
	{
		[_opera_menu_name release];
		_opera_menu_name = nil;
	}
	_opera_menu_name = [[NSString alloc] initWithString:operaMenuName];
}

- (NSString *)operaMenuName
{
	return _opera_menu_name;
}

- (void)dealloc
{
	if (_opera_menu_name)
		[_opera_menu_name release];
	[super dealloc];
}
@end

