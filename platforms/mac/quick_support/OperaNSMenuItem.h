/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OPERANSMENUITEM_H
#define OPERANSMENUITEM_H

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 

class OpInputAction;

@interface OperaNSMenuItem : NSMenuItem
{
	OpInputAction	*_input_action;
	BOOL			_keyboard_context;
}

- (id)initWithAction:(OpInputAction*)action;
- (OpInputAction *)getAction;

@end

#endif // OPERANSMENUITEM_H
