/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Plugin functionality specific to Mac platform.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#include <cassert>
#include <cctype>
#include <cstdio>

#include "common.h"

#import <Foundation/NSString.h>
#import <QuartzCore/CoreAnimation.h>
#import <Cocoa/Cocoa.h>


@interface PluginLayer : CALayer
{
	PluginInstance* _instance;
}
@end

@implementation PluginLayer
{
}
-(void)setInstance:(PluginInstance*)instance
{
	_instance = instance;
}
-(void)drawInContext:(CGContextRef)cgcontext
{
	_instance->OnPaint(cgcontext, 0, 0, [self frame].size.width, [self frame].size.height);
}
@end


const char* PluginInstance::GetString(NPNSString* nsstring)
{
	NSString* str = reinterpret_cast<NSString*>(nsstring);
	return [str UTF8String];
}


void PluginInstance::InitializeCoreAnimationLayer(PluginInstance* instance)
{
	PluginLayer* layer = [[PluginLayer layer] retain];

	ca_layer = layer;

	/* Attach plugin instance to the layer to be able to draw using instance method. */
	[layer setInstance:instance];
}


void PluginInstance::UninitializeCoreAnimationLayer()
{
	[static_cast<PluginLayer*>(ca_layer) release];
}


void PluginInstance::SetCoreAnimationLayer(void* value)
{
	*static_cast<CALayer**>(value) = static_cast<PluginLayer*>(ca_layer);
}


void PluginInstance::UpdateCoreAnimationLayer()
{
	PluginLayer* layer = static_cast<PluginLayer*>(ca_layer);

	/* Disable all implicit animations for the layer. */
	[CATransaction setDisableActions:YES];

	/* Update layer size. */
	layer.bounds = CGRectMake(0, 0, CGFloat(plugin_window->width), CGFloat(plugin_window->height));

	/* Trigger redraw. */
	[layer setNeedsDisplay];
}
