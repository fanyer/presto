/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OPERAEXTERNALNSMENU_H
#define OPERAEXTERNALNSMENU_H

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL 

@interface OperaExternalNSMenu : NSMenu
{
	NSString *_opera_menu_name;	// Name of the menu from standard_menu.ini
}

- (id)initWithTitle:(NSString *)aTitle;
- (void)setOperaMenuName:(NSString *)operaMenuName;
- (NSString *)operaMenuName;
- (void)dealloc;

@end

#endif // OPERAEXTERNALNSMENU_H
