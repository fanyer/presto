/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS

#include "platforms/mac/pluginwrapper/PluginWrapperNSCursorSwizzle.h"
#include "platforms/mac/pi/plugins/MacOpPluginTranslator.h"
#include "platforms/mac/util/macutils_mincore.h"

#include "modules/ns4plugins/component/plugin_component.h"
#include "modules/ns4plugins/component/plugin_component_instance.h"

#define BOOL NSBOOL
#import <AppKit/NSCursor.h>
#undef BOOL

//////////////////////////////////////////////////////////////////////

@interface NSCursor (OperaPluginCursor)
+ (void)opera_plugin_cursor_hide;
+ (void)opera_plugin_cursor_unhide;
@end

//////////////////////////////////////////////////////////////////////

@implementation NSCursor (OperaPluginCursor)

+ (void)opera_plugin_cursor_hide 
{
	// Find the currently active MacOpPluginTranslator and send the message via it's channel
	MacOpPluginTranslator *translator = MacOpPluginTranslator::GetActivePluginTranslator();
	if (translator)
		translator->SendBrowserCursorShown(false);
}

+ (void)opera_plugin_cursor_unhide 
{
	// Find the currently active MacOpPluginTranslator and send the message via it's channel
	MacOpPluginTranslator *translator = MacOpPluginTranslator::GetActivePluginTranslator();
	if (translator)
		translator->SendBrowserCursorShown(true);
}

@end

//////////////////////////////////////////////////////////////////////

void HijackNSCursor() 
{
	OperaNSReleasePool pool;

	SwizzleClassMethods([NSCursor class], @selector(hide), @selector(opera_plugin_cursor_hide));
	SwizzleClassMethods([NSCursor class], @selector(unhide), @selector(opera_plugin_cursor_unhide));
}

#endif // NO_CORE_COMPONENTS
