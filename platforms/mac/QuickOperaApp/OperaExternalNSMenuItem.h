/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OPERAEXTERNALNSMENUITEM_H
#define OPERAEXTERNALNSMENUITEM_H

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 

@interface OperaExternalNSMenuItem : NSMenuItem
{
	EmQuickMenuCommand		_command;
}

- (id)init;
- (id)initWithTitle:(NSString *)aString action:(SEL)aSelector keyEquivalent:(NSString *)charCode;

- (void)setCommand:(EmQuickMenuCommand)command;
- (EmQuickMenuCommand)command;

@end

#endif // OPERAEXTERNALNSMENUITEM_H
