/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/subclasses/cursor_mac.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "adjunct/embrowser/EmBrowser_main.h"

#define BOOL NSBOOL
#import <AppKit/NSCursor.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSBitmapImageRep.h>
#undef BOOL

extern EmBrowserAppNotification *gApplicationProcs;

MouseCursor *gMouseCursor;

enum {
	kArrowCursor = 0,
	kIBeamCursor = 1,
	kMakeAliasCursor = 2,
	kOperationNotAllowedCursor = 3,
	kBusyButClickableCursor = 4,
	kCopyCursor = 5,
	kClosedHandCursor = 11,
	kOpenHandCursor = 12,
	kPointingHandCursor = 13,
	kCountingUpHandCursor = 14,
	kCountingDownHandCursor = 15,
	kCountingUpAndDownHandCursor = 16,
	kResizeLeftCursor = 17,
	kResizeRightCursor = 18,
	kResizeLeftRightCursor = 19,
	kCrosshairCursor = 20,
	kResizeUpCursor = 21,
	kResizeDownCursor = 22,
	kResizeUpDownCursor = 23,
	kContextualMenuCursor = 24,
	kDisappearingItemCursor = 25,
	kVerticalIBeamCursor = 26,
	kResizeEastCursor = 27,
	kResizeEastWestCursor = 28,
	kResizeNortheastCursor = 29,
	kResizeNortheastSouthwestCursor = 30,
	kResizeNorthCursor = 31,
	kResizeNorthSouthCursor = 32,
	kResizeNorthwestCursor = 33,
	kResizeNorthwestSoutheastCursor = 34,
	kResizeSoutheastCursor = 35,
	kResizeSouthCursor = 36,
	kResizeSouthwestCursor = 37,
	kResizeWestCursor = 38,
	kMoveCursor = 39,
	kHelpCursor = 40,  // Present on >= 10.7.3.
	kCellCursor = 41,  // Present on >= 10.7.3.
	kZoomInCursor = 42,  // Present on >= 10.7.3.
	kZoomOutCursor = 43  // Present on >= 10.7.3.
};

typedef long long OpMacCoreCursorType;

@interface OpMacCoreCursor : NSCursor {

@private
	OpMacCoreCursorType mType;
}

+ (id) cursorWithType: (OpMacCoreCursorType) type;
- (id) initCursorWithType: (OpMacCoreCursorType) type;
- (OpMacCoreCursorType) _coreCursorType;

@end

@implementation OpMacCoreCursor

+ (id) cursorWithType:(OpMacCoreCursorType)type {
	OpMacCoreCursor* cursor = [[OpMacCoreCursor alloc] initCursorWithType:type];
	if ([cursor image])
		return [cursor autorelease];

	[cursor release];
	return nil;
}

- (id) initCursorWithType:(OpMacCoreCursorType)type {
	if ((self = [super init])) {
		mType = type;
	}
	return self;
}

- (OpMacCoreCursorType) _coreCursorType {
	return mType;
}

@end

namespace {

	const int internal_cursors = 17;
	
	struct CursData {
		unsigned char data[32];
		unsigned char mask[32];
		unsigned char *planes[2];
		char hotspot_x;
		char hotspot_y;
		CursorType operaType;
	};
	static CursData cursors[internal_cursors] = {
		{
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x08, 0x00, // 0b0000100000000000
				0x18, 0x00, // 0b0001100000000000
				0x3F, 0xFC, // 0b0011111111111100
				0x3F, 0xFC, // 0b0011111111111100
				0x18, 0x00, // 0b0001100000000000
				0x08, 0x00, // 0b0000100000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x0C, 0x00, // 0b0000110000000000
				0x1C, 0x00, // 0b0001110000000000
				0x3F, 0xFE, // 0b0011111111111110
				0x7F, 0xFE, // 0b0111111111111110
				0x7F, 0xFE, // 0b0111111111111110
				0x3F, 0xFE, // 0b0011111111111110
				0x1C, 0x00, // 0b0001110000000000
				0x0C, 0x00, // 0b0000110000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 2, 8, CURSOR_W_RESIZE
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x10, // 0b0000000000010000
				0x00, 0x18, // 0b0000000000011000
				0x3F, 0xFC, // 0b0011111111111100
				0x3F, 0xFC, // 0b0011111111111100
				0x00, 0x18, // 0b0000000000011000
				0x00, 0x10, // 0b0000000000010000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x30, // 0b0000000000110000
				0x00, 0x38, // 0b0000000000111000
				0x7F, 0xFC, // 0b0111111111111100
				0x7F, 0xFE, // 0b0111111111111110
				0x7F, 0xFE, // 0b0111111111111110
				0x7F, 0xFC, // 0b0111111111111100
				0x00, 0x38, // 0b0000000000111000
				0x00, 0x30, // 0b0000000000110000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 13, 8, CURSOR_E_RESIZE
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x01, 0x80, // 0b0000000110000000
				0x03, 0xC0, // 0b0000001111000000
				0x07, 0xE0, // 0b0000011111100000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x01, 0x80, // 0b0000000110000000
				0x03, 0xC0, // 0b0000001111000000
				0x07, 0xE0, // 0b0000011111100000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 8, 2, CURSOR_N_RESIZE
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x07, 0xE0, // 0b0000011111100000
				0x03, 0xC0, // 0b0000001111000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x07, 0xE0, // 0b0000011111100000
				0x03, 0xC0, // 0b0000001111000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 8, 13, CURSOR_S_RESIZE
		},
		{
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x3C, 0x00, // 0b0011110000000000
				0x38, 0x00, // 0b0011100000000000
				0x3C, 0x00, // 0b0011110000000000
				0x2E, 0x00, // 0b0010111000000000
				0x07, 0x00, // 0b0000011100000000
				0x03, 0x80, // 0b0000001110000000
				0x01, 0xC0, // 0b0000000111000000
				0x00, 0xE0, // 0b0000000011100000
				0x00, 0x70, // 0b0000000001110000
				0x00, 0x30, // 0b0000000000110000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7F, 0x00, // 0b0111111100000000
				0x7F, 0x80, // 0b0111111110000000
				0x07, 0xC0, // 0b0000011111000000
				0x03, 0xE0, // 0b0000001111100000
				0x01, 0xF0, // 0b0000000111110000
				0x00, 0xF8, // 0b0000000011111000
				0x00, 0x78, // 0b0000000001111000
				0x00, 0x38, // 0b0000000000111000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 2, 2, CURSOR_NW_RESIZE
		},
		{
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x3C, // 0b0000000000111100
				0x00, 0x1C, // 0b0000000000011100
				0x00, 0x3C, // 0b0000000000111100
				0x00, 0x74, // 0b0000000001110100
				0x00, 0xE0, // 0b0000000011100000
				0x01, 0xC0, // 0b0000000111000000
				0x03, 0x80, // 0b0000001110000000
				0x07, 0x00, // 0b0000011100000000
				0x0E, 0x00, // 0b0000111000000000
				0x0C, 0x00, // 0b0000110000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0xFE, // 0b0000000011111110
				0x01, 0xFE, // 0b0000000111111110
				0x03, 0xE0, // 0b0000001111100000
				0x07, 0xC0, // 0b0000011111000000
				0x0F, 0x80, // 0b0000111110000000
				0x1F, 0x00, // 0b0001111100000000
				0x1E, 0x00, // 0b0001111000000000
				0x1C, 0x00, // 0b0001110000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 13, 2, CURSOR_NE_RESIZE
		},
		{
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x30, // 0b0000000000110000
				0x00, 0x70, // 0b0000000001110000
				0x00, 0xE0, // 0b0000000011100000
				0x01, 0xC0, // 0b0000000111000000
				0x03, 0x80, // 0b0000001110000000
				0x07, 0x00, // 0b0000011100000000
				0x2E, 0x00, // 0b0010111000000000
				0x3C, 0x00, // 0b0011110000000000
				0x38, 0x00, // 0b0011100000000000
				0x3C, 0x00, // 0b0011110000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x38, // 0b0000000000111000
				0x00, 0x78, // 0b0000000001111000
				0x00, 0xF8, // 0b0000000011111000
				0x01, 0xF0, // 0b0000000111110000
				0x03, 0xE0, // 0b0000001111100000
				0x07, 0xC0, // 0b0000011111000000
				0x7F, 0x80, // 0b0111111110000000
				0x7F, 0x00, // 0b0111111100000000
				0x7E, 0x00, // 0b0111111000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7E, 0x00, // 0b0111111000000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 2, 13, CURSOR_SW_RESIZE
		},
		{
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x0C, 0x00, // 0b0000110000000000
				0x0E, 0x00, // 0b0000111000000000
				0x07, 0x00, // 0b0000011100000000
				0x03, 0x80, // 0b0000001110000000
				0x01, 0xC0, // 0b0000000111000000
				0x00, 0xE0, // 0b0000000011100000
				0x00, 0x74, // 0b0000000001110100
				0x00, 0x3C, // 0b0000000000111100
				0x00, 0x1C, // 0b0000000000011100
				0x00, 0x3C, // 0b0000000000111100
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x1C, 0x00, // 0b0001110000000000
				0x1E, 0x00, // 0b0001111000000000
				0x1F, 0x00, // 0b0001111100000000
				0x0F, 0x80, // 0b0000111110000000
				0x07, 0xC0, // 0b0000011111000000
				0x03, 0xE0, // 0b0000001111100000
				0x01, 0xFE, // 0b0000000111111110
				0x00, 0xFE, // 0b0000000011111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x7E, // 0b0000000001111110
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 13, 13, CURSOR_SE_RESIZE
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x07, 0xE0, // 0b0000011111100000
				0x0C, 0x30, // 0b0000110000110000
				0x0C, 0x30, // 0b0000110000110000
				0x00, 0x30, // 0b0000000000110000
				0x00, 0x60, // 0b0000000001100000
				0x00, 0xC0, // 0b0000000011000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00, // 0b0000000000000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x07, 0xE0, // 0b0000011111100000
				0x0F, 0xF0, // 0b0000111111110000
				0x1F, 0xF8, // 0b0001111111111000
				0x1E, 0x78, // 0b0001111001111000
				0x1E, 0x78, // 0b0001111001111000
				0x00, 0xF0, // 0b0000000011110000
				0x01, 0xE0, // 0b0000000111100000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x03, 0xC0, // 0b0000001111000000
				0x00, 0x00  // 0b0000000000000000
			}, {NULL, NULL}, 8, 10, CURSOR_HELP
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x01, 0x80, // 0b0000000110000000
				0x03, 0xC0, // 0b0000001111000000
				0x07, 0xE0, // 0b0000011111100000
				0x01, 0x80, // 0b0000000110000000
				0x11, 0x88, // 0b0001000110001000
				0x31, 0x8C, // 0b0011000110001100
				0x7F, 0xFE, // 0b0111111111111110
				0x7F, 0xFE, // 0b0111111111111110
				0x31, 0x8C, // 0b0011000110001100
				0x11, 0x88, // 0b0001000110001000
				0x01, 0x80, // 0b0000000110000000
				0x07, 0xE0, // 0b0000011111100000
				0x03, 0xC0, // 0b0000001111000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00  // 0b0000000000000000
			}, {
				0x01, 0x80, // 0b0000000110000000
				0x03, 0xC0, // 0b0000001111000000
				0x07, 0xE0, // 0b0000011111100000
				0x0F, 0xF0, // 0b0000111111110000
				0x1F, 0xF8, // 0b0001111111111000
				0x3B, 0xDC, // 0b0011101111011100
				0x7F, 0xFE, // 0b0111111111111110
				0xFF, 0xFF, // 0b1111111111111111
				0xFF, 0xFF, // 0b1111111111111111
				0x7F, 0xFE, // 0b0111111111111110
				0x3B, 0xDC, // 0b0011101111011100
				0x1F, 0xF8, // 0b0001111111111000
				0x0F, 0xF0, // 0b0000111111110000
				0x07, 0xE0, // 0b0000011111100000
				0x03, 0xC0, // 0b0000001111000000
				0x01, 0x80  // 0b0000000110000000
			}, {NULL, NULL}, 8, 8, CURSOR_ALL_SCROLL
		}, {
			{
				0x07, 0xE0, // 0b0000011111100000
				0x07, 0xE0, // 0b0000011111100000
				0x07, 0xE0, // 0b0000011111100000
				0x07, 0xE0, // 0b0000011111100000
				0x08, 0x10, // 0b0000100000010000
				0x10, 0x08, // 0b0001000000001000
				0x10, 0x08, // 0b0001000000001000
				0x10, 0x0C, // 0b0001000000001100
				0x13, 0x8C, // 0b0001001110001100
				0x10, 0x88, // 0b0001000010001000
				0x10, 0x88, // 0b0001000010001000
				0x08, 0x10, // 0b0000100000010000
				0x07, 0xE0, // 0b0000011111100000
				0x07, 0xE0, // 0b0000011111100000
				0x07, 0xE0, // 0b0000011111100000
				0x07, 0xE0, // 0b0000011111100000
			}, {
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x1F, 0xF8, // 0b0001111111111000
				0x3F, 0xFC, // 0b0011111111111100
				0x3F, 0xFC, // 0b0011111111111100
				0x3F, 0xFE, // 0b0011111111111110
				0x3F, 0xFE, // 0b0011111111111110
				0x3F, 0xFC, // 0b0011111111111100
				0x3F, 0xFC, // 0b0011111111111100
				0x1F, 0xF8, // 0b0001111111111000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
				0x0F, 0xF0, // 0b0000111111110000
			}, {NULL, NULL}, 8, 8, CURSOR_WAIT
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x40, 0x00, // 0b0100000000000000
				0x60, 0x00, // 0b0110000000000000
				0x70, 0x00, // 0b0111000000000000
				0x78, 0x00, // 0b0111100000000000
				0x7C, 0x00, // 0b0111110000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7F, 0x00, // 0b0111111100000000
				0x7F, 0xBE, // 0b0111111110111110
				0x7C, 0x22, // 0b0111110000100010
				0x6C, 0x14, // 0b0110110000010100
				0x46, 0x08, // 0b0100011000001000
				0x06, 0x14, // 0b0000011000010100
				0x03, 0x22, // 0b0000001100100010
				0x03, 0x3E, // 0b0000001100111110
				0x00, 0x00, // 0b0000000000000000
			}, {
				0xC0, 0x00, // 0b1100000000000000
				0xE0, 0x00, // 0b1110000000000000
				0xF0, 0x00, // 0b1111000000000000
				0xF8, 0x00, // 0b1111100000000000
				0xFC, 0x00, // 0b1111110000000000
				0xFE, 0x00, // 0b1111111000000000
				0xFF, 0x00, // 0b1111111100000000
				0xFF, 0xFF, // 0b1111111111111111
				0xFF, 0xFF, // 0b1111111111111111
				0xFF, 0xFF, // 0b1111111111111111
				0xFE, 0x7E, // 0b1111111001111110
				0xFF, 0x1C, // 0b1111111100011100
				0xCF, 0x3E, // 0b1100111100111110
				0x87, 0xFF, // 0b1000011111111111
				0x07, 0xFF, // 0b0000011111111111
				0x03, 0x7F, // 0b0000001101111111
			}, {NULL, NULL}, 1, 1, CURSOR_PROGRESS
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x3F, 0xFC, // 0b0011111111111100
				0x3F, 0xFC, // 0b0011111111111100
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x01, 0x80, // 0b0000000110000000
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000000000000000
			}, {
				0x00, 0x00, // 0b0000000000000000
				0x00, 0x00, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0111111111111110
				0x3F, 0xFC, // 0b0111111111111110
				0x3F, 0xFC, // 0b0111111111111110
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x01, 0x80, // 0b0000001111000000
				0x00, 0x00, // 0b0000001111000000
				0x00, 0x00, // 0b0000000000000000
			}, {NULL, NULL}, 8, 8, CURSOR_CELL
		}, {
			{
				0xFF, 0xE0, // 0b1111111111100000
				0x80, 0x00, // 0b1000000000000000
				0xBE, 0x00, // 0b1011111000000000
				0xBC, 0x00, // 0b1011110000000000
				0xB8, 0x00, // 0b1011100000000000
				0xB4, 0x01, // 0b1011010000000001
				0xA2, 0x01, // 0b1010001000000001
				0x81, 0x01, // 0b1000000100000001
				0x80, 0x81, // 0b1000000010000001
				0x80, 0x45, // 0b1000000001000101
				0x80, 0x2D, // 0b1000000000101101
				0x00, 0x1D, // 0b0000000000011101
				0x00, 0x3D, // 0b0000000000111101
				0x00, 0x7D, // 0b0000000001111101
				0x00, 0x01, // 0b0000000000000001
				0x07, 0xFF, // 0b0000011111111111
			}, {
				0xFF, 0xE0, // 0b1111111111100000
				0x80, 0x00, // 0b1000000000000000
				0xBE, 0x00, // 0b1011111000000000
				0xBC, 0x00, // 0b1011110000000000
				0xB8, 0x00, // 0b1011100000000000
				0xB4, 0x01, // 0b1011010000000001
				0xA7, 0x01, // 0b1010011100000001
				0x83, 0x81, // 0b1000001110000001
				0x81, 0xC1, // 0b1000000111000001
				0x80, 0xE5, // 0b1000000011100101
				0x80, 0x7D, // 0b1000000001111101
				0x00, 0x3D, // 0b0000000000011101
				0x00, 0x3D, // 0b0000000000111101
				0x00, 0x7D, // 0b0000000001111101
				0x00, 0x01, // 0b0000000000000001
				0x07, 0xFF, // 0b0000011111111111
			}, {NULL, NULL}, 8, 8, CURSOR_NWSE_RESIZE
		}, {
			{
				0x07, 0xFF, // 0b0000011111111111
				0x00, 0x01, // 0b0000000000000001
				0x00, 0x7D, // 0b0000000001111101
				0x00, 0x3D, // 0b0000000000111101
				0x00, 0x1D, // 0b0000000000011101
				0x80, 0x2D, // 0b1000000000101101
				0x80, 0x45, // 0b1000000001000101
				0x80, 0x81, // 0b1000000010000001
				0x81, 0x01, // 0b1000000100000001
				0xA2, 0x01, // 0b1010001000000001
				0xB4, 0x01, // 0b1011010000000001
				0xB8, 0x00, // 0b1011100000000000
				0xBC, 0x00, // 0b1011110000000000
				0xBE, 0x00, // 0b1011111000000000
				0x80, 0x00, // 0b1000000000000000
				0xFF, 0xE0, // 0b1111111111100000
			}, {
				0x07, 0xFF, // 0b0000011111111111
				0x00, 0x01, // 0b0000000000000001
				0x00, 0x7D, // 0b0000000001111101
				0x00, 0x3D, // 0b0000000000111101
				0x00, 0x3D, // 0b0000000000011101
				0x80, 0x7D, // 0b1000000001111101
				0x80, 0xE5, // 0b1000000011100101
				0x81, 0xC1, // 0b1000000111000001
				0x83, 0x81, // 0b1000001110000001
				0xA7, 0x01, // 0b1010011100000001
				0xB4, 0x01, // 0b1011010000000001
				0xB8, 0x00, // 0b1011100000000000
				0xBC, 0x00, // 0b1011110000000000
				0xBE, 0x00, // 0b1011111000000000
				0x80, 0x00, // 0b1000000000000000
				0xFF, 0xE0, // 0b1111111111100000
			}, {NULL, NULL}, 8, 8, CURSOR_NESW_RESIZE
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x40, 0x00, // 0b0100000000000000
				0x60, 0x00, // 0b0110000000000000
				0x70, 0x00, // 0b0111000000000000
				0x78, 0x00, // 0b0111100000000000
				0x7C, 0x00, // 0b0111110000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7F, 0x00, // 0b0111111100000000
				0x7F, 0x80, // 0b0111111110000000
				0x7C, 0x18, // 0b0111110000011000
				0x6C, 0x18, // 0b0110110000011000
				0x46, 0x7E, // 0b0100011001111110
				0x06, 0x7E, // 0b0000011001111110
				0x03, 0x18, // 0b0000001100011000
				0x03, 0x18, // 0b0000001100011000
				0x00, 0x00, // 0b0000000000000000
			}, {
				0xC0, 0x00, // 0b1100000000000000
				0xE0, 0x00, // 0b1110000000000000
				0xF0, 0x00, // 0b1111000000000000
				0xF8, 0x00, // 0b1111100000000000
				0xFC, 0x00, // 0b1111110000000000
				0xFE, 0x00, // 0b1111111000000000
				0xFF, 0x00, // 0b1111111100000000
				0xFF, 0x80, // 0b1111111110000000
				0xFF, 0xFC, // 0b1111111111111100
				0xFF, 0xFC, // 0b1111111111111100
				0xFE, 0x3F, // 0b1111111000111111
				0xFF, 0xFF, // 0b1111111111111111
				0xCF, 0xFF, // 0b1100111111111111
				0x87, 0xFF, // 0b1000011111111111
				0x07, 0xBC, // 0b0000011110111100
				0x03, 0x3C, // 0b0000001100111100
			}, {NULL, NULL}, 1, 1, CURSOR_COPY
		}, {
			{
				0x00, 0x00, // 0b0000000000000000
				0x40, 0x00, // 0b0100000000000000
				0x60, 0x00, // 0b0110000000000000
				0x70, 0x00, // 0b0111000000000000
				0x78, 0x00, // 0b0111100000000000
				0x7C, 0x00, // 0b0111110000000000
				0x7E, 0x00, // 0b0111111000000000
				0x7F, 0x00, // 0b0111111100000000
				0x7F, 0x80, // 0b0111111110000000
				0x7C, 0x3E, // 0b0111110000111110
				0x6C, 0x1E, // 0b0110110000011110
				0x46, 0x1E, // 0b0100011000011110
				0x06, 0x3E, // 0b0000011000111110
				0x03, 0x62, // 0b0000001101100010
				0x03, 0x40, // 0b0000001101000000
				0x00, 0x00, // 0b0000000000000000
			}, {
				0xC0, 0x00, // 0b1100000000000000
				0xE0, 0x00, // 0b1110000000000000
				0xF0, 0x00, // 0b1111000000000000
				0xF8, 0x00, // 0b1111100000000000
				0xFC, 0x00, // 0b1111110000000000
				0xFE, 0x00, // 0b1111111000000000
				0xFF, 0x00, // 0b1111111100000000
				0xFF, 0x80, // 0b1111111110000000
				0xFF, 0xFF, // 0b1111111111111111
				0xFF, 0xFF, // 0b1111111111111111
				0xFE, 0x3F, // 0b1111111000111111
				0xFF, 0x3F, // 0b1111111100111111
				0xCF, 0x7F, // 0b1100111101111111
				0x87, 0xF7, // 0b1000011111110111
				0x07, 0xF2, // 0b0000011111110010
				0x03, 0x60, // 0b0000001101100000
			}, {NULL, NULL}, 1, 1, CURSOR_ALIAS
		}
	};

OpMacCoreCursorType OperaCursorTypeToCoreCursorType(CursorType ctype) {
	switch (ctype) {
		case CURSOR_URI:              return kArrowCursor;
		case CURSOR_CROSSHAIR:        return kCrosshairCursor;
		case CURSOR_DEFAULT_ARROW:    return kArrowCursor;
		case CURSOR_CUR_POINTER:      return kPointingHandCursor;
		case CURSOR_MOVE:             return kMoveCursor;
		case CURSOR_E_RESIZE:         return kResizeEastCursor;
		case CURSOR_NE_RESIZE:        return kResizeNortheastCursor;
		case CURSOR_NW_RESIZE:        return kResizeNorthwestCursor;
		case CURSOR_N_RESIZE:         return kResizeNorthCursor;
		case CURSOR_SE_RESIZE:        return kResizeSoutheastCursor;
		case CURSOR_SW_RESIZE:        return kResizeSouthwestCursor;
		case CURSOR_S_RESIZE:         return kResizeSouthCursor;
		case CURSOR_W_RESIZE:         return kResizeWestCursor;
		case CURSOR_TEXT:             return kIBeamCursor;
		case CURSOR_WAIT:             return kBusyButClickableCursor;
		case CURSOR_HELP:             return kHelpCursor;
		case CURSOR_ARROW_WAIT:
		case CURSOR_PROGRESS:         return kBusyButClickableCursor;
		case CURSOR_MAGNIFYING_GLASS:
		case CURSOR_ZOOM_IN:          return kZoomInCursor;
		case CURSOR_ZOOM_OUT:         return kZoomOutCursor;
		case CURSOR_CONTEXT_MENU:     return kContextualMenuCursor;
		case CURSOR_CELL:             return kCellCursor;
		case CURSOR_VERTICAL_TEXT:    return kVerticalIBeamCursor;
		case CURSOR_ALIAS:            return kMakeAliasCursor;
		case CURSOR_COPY:             return kCopyCursor;
		case CURSOR_NO_DROP:          return kOperationNotAllowedCursor;
		case CURSOR_DROP_COPY:        return kCopyCursor;
		case CURSOR_DROP_MOVE:        return kClosedHandCursor;
		case CURSOR_DROP_LINK:        return kMakeAliasCursor;
		case CURSOR_NOT_ALLOWED:      return kOperationNotAllowedCursor;
		case CURSOR_COL_RESIZE:       return kResizeLeftRightCursor;
		case CURSOR_VERT_SPLITTER:
		case CURSOR_EW_RESIZE:        return kResizeEastWestCursor;
		case CURSOR_ROW_RESIZE:       return kResizeUpDownCursor;
		case CURSOR_HOR_SPLITTER:
		case CURSOR_NS_RESIZE:        return kResizeNorthSouthCursor;
		case CURSOR_NESW_RESIZE:      return kResizeNortheastSouthwestCursor;
		case CURSOR_NWSE_RESIZE:      return kResizeNorthwestSoutheastCursor;
		case CURSOR_ALL_SCROLL:       return kMoveCursor;
	}
	return kArrowCursor;
}

NSCursor* LoadFallbackCursor(CursorType type) {

	switch(type) {
		case CURSOR_DEFAULT_ARROW: return [NSCursor arrowCursor];
		case CURSOR_CUR_POINTER:   return [NSCursor pointingHandCursor];
		case CURSOR_MOVE:          return [NSCursor openHandCursor];
		case CURSOR_VERTICAL_TEXT:
		case CURSOR_TEXT:          return [NSCursor IBeamCursor];
		case CURSOR_CROSSHAIR:     return [NSCursor crosshairCursor];
		case CURSOR_VERT_SPLITTER:
		case CURSOR_COL_RESIZE:
		case CURSOR_EW_RESIZE:     return [NSCursor resizeLeftRightCursor];
		case CURSOR_HOR_SPLITTER:
		case CURSOR_ROW_RESIZE:
		case CURSOR_NS_RESIZE:     return [NSCursor resizeUpDownCursor];
		case CURSOR_NO_DROP:
		case CURSOR_NOT_ALLOWED:
			if ([NSCursor respondsToSelector:@selector(operationNotAllowedCursor)])
				return [NSCursor operationNotAllowedCursor];
		case CURSOR_CONTEXT_MENU:
			if ([NSCursor respondsToSelector:@selector(contextualMenuCursor)])
				return [NSCursor contextualMenuCursor];
		case CURSOR_COPY:
			if ([NSCursor respondsToSelector:@selector(dragCopyCursor)])
				return [NSCursor dragCopyCursor];
			break;
		case CURSOR_ZOOM_IN: return [[[NSCursor alloc] initWithImage:[NSImage imageNamed:@"zoom_in.cur"] hotSpot:NSMakePoint(8,8)] autorelease];
		case CURSOR_ZOOM_OUT: return [[[NSCursor alloc] initWithImage:[NSImage imageNamed:@"zoom_out.cur"] hotSpot:NSMakePoint(8,8)] autorelease];
	}
	
	for (int i = 0; i < internal_cursors; ++i) {
		if (type == cursors[i].operaType) {
			NSImage* img = [[NSImage alloc] init];
			cursors[i].planes[0] = cursors[i].data;
			cursors[i].planes[1] = cursors[i].mask;
			NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
									    initWithBitmapDataPlanes: cursors[i].planes
									                  pixelsWide: 16
										              pixelsHigh: 16
									               bitsPerSample: 1
									             samplesPerPixel: 2
														hasAlpha: YES
									                    isPlanar: YES
												  colorSpaceName: NSDeviceBlackColorSpace
												    bitmapFormat: (NSBitmapFormat)0
													 bytesPerRow: 2
									                bitsPerPixel: 1];
			[img addRepresentation:[rep autorelease]];
			return [[[NSCursor alloc] initWithImage:[img autorelease] hotSpot:NSMakePoint(cursors[i].hotspot_x, cursors[i].hotspot_y)] autorelease];
		}
	}
	return nil;
}

NSCursor* LoadCursorWithType(CursorType type){
	if (GetOSVersion() >= 0x1070) {
		NSCursor* cursor = [OpMacCoreCursor cursorWithType:OperaCursorTypeToCoreCursorType(type)];
		if (cursor)
			return cursor;
	}
	return LoadFallbackCursor(type);
}

}
	
// ---------------------------------------------------------------------------
MouseCursor::MouseCursor()
{
	memset(m_cursors_table, 0, sizeof(m_cursors_table));

	for (int i = CURSOR_URI; i < CURSOR_NUM_CURSORS; ++i)
		m_cursors_table[i] = [LoadCursorWithType(static_cast<CursorType>(i)) retain];

	m_current_cursor = CURSOR_DEFAULT_ARROW;
}

MouseCursor::~MouseCursor()
{
	for (unsigned int i = 0; i < CURSOR_NUM_CURSORS; ++i)
		[(id)m_cursors_table[i] release];
}

void MouseCursor::SetCursor(CursorType cursor)
{
	OP_ASSERT((cursor < CURSOR_NUM_CURSORS) && (cursor >= CURSOR_URI));
	
	if (m_current_cursor == CURSOR_NONE && m_current_cursor != cursor) [NSCursor unhide];
	
	if ((cursor >= CURSOR_NUM_CURSORS) || (cursor < CURSOR_URI))
		cursor = CURSOR_DEFAULT_ARROW;

	if(gApplicationProcs && gApplicationProcs->setCursor)
	{
		if(gApplicationProcs->setCursor((EmMouseCursor)cursor))
		{
			// ok, the embedder wants to override our cursor
			m_current_cursor = CURSOR_AUTO;
			return;
		}
	}

	if (cursor == CURSOR_ARROW_WAIT)	// loading pages, busy but not locked.
		cursor = CURSOR_DEFAULT_ARROW;

	if (cursor != m_current_cursor)
	{
		m_current_cursor = cursor;
		if (cursor == CURSOR_NONE)
			[NSCursor hide];
		else
			[(NSCursor*)m_cursors_table[m_current_cursor] set];
	}
}

void MouseCursor::ReapplyCursor() {
	[(NSCursor*)m_cursors_table[m_current_cursor] set];
}

