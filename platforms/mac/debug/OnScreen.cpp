/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef _MAC_DEBUG
#include "platforms/mac/debug/OnScreen.h"
#include "platforms/mac/util/MachOCompatibility.h"

#ifdef NO_CARBON
extern short GetMenuBarHeight();
#define GetMBarHeight GetMenuBarHeight
#endif

#ifndef SIXTY_FOUR_BIT

static char		gC1tab[] = {255, 210, 35, 30, 185, 180, 5, 0};
static short	gC2tab[] = {0x000, 0x001f, 0x7c00, 0x7c1f, 0x03e0, 0x03ff, 0x7fe0, 0x7fff};
static long		gC4tab[] = {0x00000000, 0x000000ff, 0x00ff0000, 0x00ff00ff, 0x0000ff00, 0x0000ffff, 0x00ffff00, 0x00ffffff};

static char gCharSet8[] =
{
	0x00,	// 0
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 1
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 2
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 3
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 4
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 5
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 6
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 7
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 8
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 9
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 10
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 11
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 12
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 13
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 14
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 15
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 16
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 17
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 18
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 19
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 20
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 21
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 22
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 23
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 24
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 25
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 26
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 27
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 28
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 29
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 30
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 31
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 32
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x00,	// 33
	0x10,
	0x10,
	0x10,
	0x10,
	0x00,
	0x10,
	0x00,

	0x00,	// 34
	0x24,
	0x24,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x00,	// 35
	0x24,
	0x7E,
	0x24,
	0x24,
	0x7E,
	0x24,
	0x00,

	0x00,	// 36
	0x08,
	0x3E,
	0x28,
	0x3E,
	0x0A,
	0x3E,
	0x08,

	0x00,	// 37
	0x62,
	0x64,
	0x08,
	0x10,
	0x26,
	0x46,
	0x00,

	0x00,	// 38
	0x10,
	0x28,
	0x10,
	0x2A,
	0x44,
	0x3A,
	0x00,

	0x00,	// 39
	0x08,
	0x10,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x00,	// 40
	0x04,
	0x08,
	0x08,
	0x08,
	0x08,
	0x04,
	0x00,

	0x00,	// 41
	0x20,
	0x10,
	0x10,
	0x10,
	0x10,
	0x20,
	0x00,

	0x00,	// 42
	0x00,
	0x14,
	0x08,
	0x3E,
	0x08,
	0x14,
	0x00,

	0x00,	// 43
	0x00,
	0x08,
	0x08,
	0x3E,
	0x08,
	0x08,
	0x00,

	0x00,	// 44
	0x00,
	0x00,
	0x00,
	0x00,
	0x08,
	0x08,
	0x10,

	0x00,	// 45
	0x00,
	0x00,
	0x00,
	0x3E,
	0x00,
	0x00,
	0x00,

	0x00,	// 46
	0x00,
	0x00,
	0x00,
	0x00,
	0x18,
	0x18,
	0x00,

	0x00,	// 47
	0x00,
	0x02,
	0x04,
	0x08,
	0x10,
	0x20,
	0x00,

	0x00,	// 48
	0x3C,
	0x46,
	0x4A,
	0x52,
	0x62,
	0x3C,
	0x00,

	0x00,	// 49
	0x08,
	0x18,
	0x08,
	0x08,
	0x08,
	0x3E,
	0x00,

	0x00,	// 50
	0x3C,
	0x42,
	0x02,
	0x3C,
	0x40,
	0x7E,
	0x00,

	0x00,	// 51
	0x3C,
	0x42,
	0x0C,
	0x02,
	0x42,
	0x3C,
	0x00,

	0x00,	// 52
	0x0C,
	0x14,
	0x24,
	0x44,
	0x7E,
	0x04,
	0x00,

	0x00,	// 53
	0x7E,
	0x40,
	0x7C,
	0x02,
	0x42,
	0x3C,
	0x00,

	0x00,	// 54
	0x3C,
	0x40,
	0x7C,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,	// 55
	0x7E,
	0x02,
	0x04,
	0x08,
	0x10,
	0x10,
	0x00,

	0x00,	// 56
	0x3C,
	0x42,
	0x3C,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,	// 57
	0x3C,
	0x42,
	0x42,
	0x3E,
	0x02,
	0x3C,
	0x00,

	0x00,	// 58
	0x00,
	0x00,
	0x10,
	0x00,
	0x00,
	0x10,
	0x00,

	0x00,	// 59
	0x00,
	0x10,
	0x00,
	0x00,
	0x10,
	0x10,
	0x20,

	0x00,	// 60
	0x00,
	0x04,
	0x08,
	0x10,
	0x08,
	0x04,
	0x00,

	0x00,	// 61
	0x00,
	0x00,
	0x3E,
	0x00,
	0x3E,
	0x00,
	0x00,

	0x00,	// 62
	0x00,
	0x10,
	0x08,
	0x04,
	0x08,
	0x10,
	0x00,

	0x00,	// 63
	0x3C,
	0x42,
	0x04,
	0x08,
	0x00,
	0x08,
	0x00,

	0x00,	// 64
	0x3C,
	0x4A,
	0x56,
	0x5E,
	0x40,
	0x3C,
	0x00,

	0x00,	// 65
	0x3C,
	0x42,
	0x42,
	0x7E,
	0x42,
	0x42,
	0x00,

	0x00,	// 66
	0x7C,
	0x42,
	0x7C,
	0x42,
	0x42,
	0x7C,
	0x00,

	0x00,	// 67
	0x3C,
	0x42,
	0x40,
	0x40,
	0x42,
	0x3C,
	0x00,

	0x00,	// 68
	0x78,
	0x44,
	0x42,
	0x42,
	0x44,
	0x78,
	0x00,

	0x00,	// 69
	0x7E,
	0x40,
	0x7C,
	0x40,
	0x40,
	0x7E,
	0x00,

	0x00,	// 70
	0x7E,
	0x40,
	0x7C,
	0x40,
	0x40,
	0x40,
	0x00,

	0x00,	// 71
	0x3C,
	0x42,
	0x40,
	0x4E,
	0x42,
	0x3C,
	0x00,

	0x00,	// 72
	0x42,
	0x42,
	0x7E,
	0x42,
	0x42,
	0x42,
	0x00,

	0x00,	// 73
	0x3E,
	0x08,
	0x08,
	0x08,
	0x08,
	0x3E,
	0x00,

	0x00,	// 74
	0x02,
	0x02,
	0x02,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,	// 75
	0x44,
	0x48,
	0x70,
	0x48,
	0x44,
	0x42,
	0x00,

	0x00,	// 76
	0x40,
	0x40,
	0x40,
	0x40,
	0x40,
	0x7E,
	0x00,

	0x00,	// 77
	0x42,
	0x66,
	0x5A,
	0x42,
	0x42,
	0x42,
	0x00,

	0x00,	// 78
	0x42,
	0x62,
	0x52,
	0x4A,
	0x46,
	0x42,
	0x00,

	0x00,	// 79
	0x3C,
	0x42,
	0x42,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,	// 80
	0x7C,
	0x42,
	0x42,
	0x7C,
	0x40,
	0x40,
	0x00,

	0x00,	// 81
	0x3C,
	0x42,
	0x42,
	0x52,
	0x4A,
	0x3C,
	0x00,

	0x00,	// 82
	0x7C,
	0x42,
	0x42,
	0x7C,
	0x44,
	0x42,
	0x00,

	0x00,	// 83
	0x3C,
	0x40,
	0x3C,
	0x02,
	0x42,
	0x3C,
	0x00,

	0x00,	// 84
	0xFE,
	0x10,
	0x10,
	0x10,
	0x10,
	0x10,
	0x00,

	0x00,	// 85
	0x42,
	0x42,
	0x42,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,	// 86
	0x42,
	0x42,
	0x42,
	0x42,
	0x24,
	0x18,
	0x00,

	0x00,	// 87
	0x42,
	0x42,
	0x42,
	0x42,
	0x5A,
	0x24,
	0x00,

	0x00,	// 88
	0x42,
	0x24,
	0x18,
	0x18,
	0x24,
	0x42,
	0x00,

	0x00,	// 89
	0x82,
	0x44,
	0x28,
	0x10,
	0x10,
	0x10,
	0x00,

	0x00,	// 90
	0x7E,
	0x04,
	0x08,
	0x10,
	0x20,
	0x7E,
	0x00,

	0x00,	// 91
	0x0E,
	0x08,
	0x08,
	0x08,
	0x08,
	0x0E,
	0x00,

	0x00,	// 92
	0x00,
	0x40,
	0x20,
	0x10,
	0x08,
	0x04,
	0x00,

	0x00,	// 93
	0x70,
	0x10,
	0x10,
	0x10,
	0x10,
	0x70,
	0x00,

	0x00,	// 94
	0x10,
	0x38,
	0x54,
	0x10,
	0x10,
	0x10,
	0x00,

	0x00,	// 95
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0xFF,

	0x00,	// 96
	0x1C,
	0x22,
	0x78,
	0x20,
	0x20,
	0x7E,
	0x00,

	0x00,	// 97
	0x00,
	0x38,
	0x04,
	0x3C,
	0x44,
	0x3C,
	0x00,

	0x00,	// 98
	0x20,
	0x20,
	0x3C,
	0x22,
	0x22,
	0x3C,
	0x00,

	0x00,	// 99
	0x00,
	0x1C,
	0x20,
	0x20,
	0x20,
	0x1C,
	0x00,

	0x00,	// 100
	0x04,
	0x04,
	0x3C,
	0x44,
	0x44,
	0x3C,
	0x00,

	0x00,	// 101
	0x00,
	0x38,
	0x44,
	0x78,
	0x40,
	0x3C,
	0x00,

	0x00,	// 102
	0x0C,
	0x10,
	0x18,
	0x10,
	0x10,
	0x10,
	0x00,

	0x00,	// 103
	0x00,
	0x3C,
	0x44,
	0x44,
	0x3C,
	0x04,
	0x38,

	0x00,	// 104
	0x40,
	0x40,
	0x78,
	0x44,
	0x44,
	0x44,
	0x00,

	0x00,	// 105
	0x10,
	0x00,
	0x30,
	0x10,
	0x10,
	0x38,
	0x00,

	0x00,	// 106
	0x04,
	0x00,
	0x04,
	0x04,
	0x04,
	0x24,
	0x18,

	0x00,	// 107
	0x20,
	0x28,
	0x30,
	0x30,
	0x28,
	0x24,
	0x00,

	0x00,	// 108
	0x10,
	0x10,
	0x10,
	0x10,
	0x10,
	0x0C,
	0x00,

	0x00,	// 109
	0x00,
	0x68,
	0x54,
	0x54,
	0x54,
	0x54,
	0x00,

	0x00,	// 110
	0x00,
	0x78,
	0x44,
	0x44,
	0x44,
	0x44,
	0x00,

	0x00,	// 111
	0x00,
	0x38,
	0x44,
	0x44,
	0x44,
	0x38,
	0x00,

	0x00,	// 112
	0x00,
	0x78,
	0x44,
	0x44,
	0x78,
	0x40,
	0x40,

	0x00,	// 113
	0x00,
	0x3C,
	0x44,
	0x44,
	0x3C,
	0x04,
	0x06,

	0x00,	// 114
	0x00,
	0x1C,
	0x20,
	0x20,
	0x20,
	0x20,
	0x00,

	0x00,	// 115
	0x00,
	0x38,
	0x40,
	0x38,
	0x04,
	0x78,
	0x00,

	0x00,	// 116
	0x10,
	0x38,
	0x10,
	0x10,
	0x10,
	0x0C,
	0x00,

	0x00,	// 117
	0x00,
	0x44,
	0x44,
	0x44,
	0x44,
	0x38,
	0x00,

	0x00,	// 118
	0x00,
	0x44,
	0x44,
	0x28,
	0x28,
	0x10,
	0x00,

	0x00,	// 119
	0x00,
	0x44,
	0x54,
	0x54,
	0x54,
	0x28,
	0x00,

	0x00,	// 120
	0x00,
	0x44,
	0x28,
	0x10,
	0x28,
	0x44,
	0x00,

	0x00,	// 121
	0x00,
	0x44,
	0x44,
	0x44,
	0x3C,
	0x04,
	0x38,

	0x00,	// 122
	0x00,
	0x7C,
	0x08,
	0x10,
	0x20,
	0x7C,
	0x00,

	0x00,	// 123
	0x0E,
	0x08,
	0x30,
	0x08,
	0x08,
	0x0E,
	0x00,

	0x00,	// 124
	0x08,
	0x08,
	0x08,
	0x08,
	0x08,
	0x08,
	0x00,

	0x00,	// 125
	0x70,
	0x10,
	0x0C,
	0x10,
	0x10,
	0x70,
	0x00,

	0x00,	// 126
	0x14,
	0x28,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x3C,	// 127
	0x42,
	0x99,
	0xA1,
	0xA1,
	0x99,
	0x42,
	0x3C,

	0x00,	// 128
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x0F,	// 129
	0x0F,
	0x0F,
	0x0F,
	0x00,
	0x00,
	0x00,
	0x00,

	0xF0,	// 130
	0xF0,
	0xF0,
	0xF0,
	0x00,
	0x00,
	0x00,
	0x00,

	0xFF,	// 131
	0xFF,
	0xFF,
	0xFF,
	0x00,
	0x00,
	0x00,
	0x00,

	0x00,	// 132
	0x00,
	0x00,
	0x00,
	0x0F,
	0x0F,
	0x0F,
	0x0F,

	0x0F,	// 133
	0x0F,
	0x0F,
	0x0F,
	0x0F,
	0x0F,
	0x0F,
	0x0F,

	0xF0,	// 134
	0xF0,
	0xF0,
	0xF0,
	0x0F,
	0x0F,
	0x0F,
	0x0F,

	0xFF,	// 135
	0xFF,
	0xFF,
	0xFF,
	0x0F,
	0x0F,
	0x0F,
	0x0F,

	0x00,	// 136
	0x00,
	0x00,
	0x00,
	0xF0,
	0xF0,
	0xF0,
	0xF0,

	0x0F,	// 137
	0x0F,
	0x0F,
	0x0F,
	0xF0,
	0xF0,
	0xF0,
	0xF0,

	0xF0,	// 138
	0xF0,
	0xF0,
	0xF0,
	0xF0,
	0xF0,
	0xF0,
	0xF0,

	0xFF,	// 139
	0xFF,
	0xFF,
	0xFF,
	0xF0,
	0xF0,
	0xF0,
	0xF0,

	0x00,	// 140
	0x00,
	0x00,
	0x00,
	0xFF,
	0xFF,
	0xFF,
	0xFF,

	0x0F,	// 141
	0x0F,
	0x0F,
	0x0F,
	0xFF,
	0xFF,
	0xFF,
	0xFF,

	0xF0,	// 142
	0xF0,
	0xF0,
	0xF0,
	0xFF,
	0xFF,
	0xFF,
	0xFF,

	0xFF,	// 143
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,


	0
};

static char		gDigits8[] =
{
	0x00,
	0x3C,
	0x46,
	0x4A,
	0x52,
	0x62,
	0x3C,
	0x00,

	0x00,
	0x08,
	0x18,
	0x08,
	0x08,
	0x08,
	0x3E,
	0x00,

	0x00,
	0x3C,
	0x42,
	0x02,
	0x3C,
	0x40,
	0x7E,
	0x00,

	0x00,
	0x3C,
	0x42,
	0x0C,
	0x02,
	0x42,
	0x3C,
	0x00,

	0x00,
	0x0C,
	0x14,
	0x24,
	0x44,
	0x7E,
	0x04,
	0x00,

	0x00,
	0x7E,
	0x40,
	0x7C,
	0x02,
	0x42,
	0x3C,
	0x00,

	0x00,
	0x3C,
	0x40,
	0x7C,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,
	0x7E,
	0x02,
	0x04,
	0x08,
	0x10,
	0x10,
	0x00,

	0x00,
	0x3C,
	0x42,
	0x3C,
	0x42,
	0x42,
	0x3C,
	0x00,

	0x00,
	0x3C,
	0x42,
	0x42,
	0x3E,
	0x02,
	0x3C,
	0x00,

	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,

	0x00,
	0x00,
	0x00,
	0x7E,
	0x00,
	0x00,
	0x00,
	0x00,

	0x00,		// scratch
	0x00,		// scratch
	0x00,		// scratch
	0x00		// scratch
};

static long		gDigits5[] =
{
	0x726AB270,
	0x10C1047C,
	0x3813907C,
	0x39119138,
	0x18A49F08,
	0x7D078138,
	0x39079138,
	0x7C108410,
	0x39139138,
	0x3913C138,
	0x00000000,
	0x0007C000
};

/*
static long		gDigits4[] =
{
	0b01101011100111010110000000000000,
	0b00100110001000100111000000000000,
	0b11100001011010001111000000000000,
	0b11100001011000011110000000000000,
	0b00100110101011110010000000000000,
	0b11111000111000011110000000000000,
	0b01101000111010010110000000000000,
	0b11110001001001000100000000000000,
	0b01101001011010010110000000000000,
	0b01101001011100010110000000000000,
	0b00000000000000000000000000000000
};
*/

static long		gDigits3[] =
{
	0x4AAA4000,
	0x4C44E000,
	0xC248E000,
	0xC242C000,
	0x26AE2000,
	0xE8C2C000,
	0x48CA4000,
	0xE2444000,
	0x4A4A4000,
	0x4A624000,
	0x00000000,
	0x00E00000
};



void DecOnScreen(int x, int y, int fgcolor, int bkcolor, long value)
{

	enum
	{
//		kDigits = 10
		kDigits = 6,
		kHeight = 8
//		kHeight = 6			// temporary changed for speeding up alittle
	};

//	static long		oldValue = -1;
	PixMapHandle	mainPixMap;
	register Ptr	baseAddr;
	short			rowBytes;
	register short	gap;
	register char	bits;
	register char	*src;
	register int	v;
	register int	i;
	register long	d;
	register char	spaceflag;
	register long	pixelSizeB;
	register long	*p;
	Boolean			minus;
	register int	ex;
	register int	ey;

//	if(oldValue == value)
//	{
//		return;
//	}
//	oldValue = value;

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	ex = (*mainPixMap)->bounds.right;
	ey = (*mainPixMap)->bounds.bottom;

	if(x < 0)
	{
		x += ex;
	}
	if(y < 0)
	{
		y += ey;
	}
	else
	{
		y += GetMBarHeight();
	}

	if(x >= 0 && x < ex && y >= 0 && y < ey - 6)
	{
		rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

		baseAddr = (y * rowBytes) + (*mainPixMap)->baseAddr;

		x += 8 * kDigits;	// max. 10 digits

		minus = value < 0;
		if(minus)
		{
			value = -value;
		}
		switch((*mainPixMap)->pixelSize)
		{
		  case 8:
			{
				register unsigned char	set = gC1tab[fgcolor];
				register unsigned char	clr = gC1tab[bkcolor];
				pixelSizeB = 1;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= 8;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}
					src = &gDigits8[8 * d];

					p = (long *) ((x * pixelSizeB) + baseAddr);


					for(v = kHeight; v; v--)
					{
						bits = *src++;
						p[0] = ((bits & 0x80 ? set : clr) << 24) | ((bits & 0x40 ? set : clr) << 16) | ((bits & 0x20 ? set : clr) << 8) | (bits & 0x10 ? set : clr);
						p[1] = ((bits & 0x08 ? set : clr) << 24) | ((bits & 0x04 ? set : clr) << 16) | ((bits & 0x02 ? set : clr) << 8) | (bits & 0x01 ? set : clr);
						p += gap;
					}
				}
			}
			break;
		  case 16:
			{
				register unsigned short	set = gC2tab[fgcolor];
				register unsigned short	clr = gC2tab[bkcolor];

				pixelSizeB = 2;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= 8;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}
					src = &gDigits8[8 * d];

					p = (long *) ((x * pixelSizeB) + baseAddr);


					for(v = kHeight; v; v--)
					{
						bits = *src++;
						p[0] = ((bits & 0x80 ? set : clr) << 16) | (bits & 0x40 ? set : clr);
						p[1] = ((bits & 0x20 ? set : clr) << 16) | (bits & 0x10 ? set : clr);
						p[2] = ((bits & 0x08 ? set : clr) << 16) | (bits & 0x04 ? set : clr);
						p[3] = ((bits & 0x02 ? set : clr) << 16) | (bits & 0x01 ? set : clr);
						p += gap;
					}
				}
			}
			break;
		  case 24:
		  case 32:
			{
				register long	set = gC4tab[fgcolor];
				register long	clr = gC4tab[bkcolor];

				pixelSizeB = 4;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= 8;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}
					src = &gDigits8[8 * d];

					p = (long *) ((x * pixelSizeB) + baseAddr);


					for(v = kHeight; v; v--)
					{
						bits = *src++;
						p[0] = bits & 0x80 ? set : clr;
						p[1] = bits & 0x40 ? set : clr;
						p[2] = bits & 0x20 ? set : clr;
						p[3] = bits & 0x10 ? set : clr;
						p[4] = bits & 0x08 ? set : clr;
						p[5] = bits & 0x04 ? set : clr;
						p[6] = bits & 0x02 ? set : clr;
						p[7] = bits & 0x01 ? set : clr;
						p += gap;
					}
				}
			}
			break;
		}
	}
}

void Str8OnScreen(int x, int y, int fgcolor, int bkcolor, char *format, ...)
{
	enum
	{
//		kDigits = 10
		kDigits = 6,
		kHeight = 8
//		kHeight = 6			// temporary changed for speeding up alittle
	};
	va_list			marker;
	PixMapHandle	mainPixMap;
	register Ptr	baseAddr;
	short			rowBytes;
	register short	gap;
	register char	bits;
	register char	*src;
	register char	*s;
	register int	h;
	register int	v;
	register int	i;
	register long	c;
	register long	pixelSizeB;
	register long	*p;
	char			buf[513];
	register int	ex;
	register int	ey;

	va_start(marker, format);
	i = vsnprintf(buf, 512, format, marker);
	buf[512] = '\0';

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	ex = (*mainPixMap)->bounds.right;
	ey = (*mainPixMap)->bounds.bottom;

	if(x < 0)
	{
		x += ex;
	}
	if(y < 0)
	{
		y += ey;
	}
	else
	{
		y += GetMBarHeight();
	}
	if ((x < 0) || (y < 0) || ((x + 8) > ex) || ((y + 8) > ey))
		return;

	rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

	baseAddr = (y * rowBytes) + (*mainPixMap)->baseAddr;

	h = (ex - x) / 8;					// max string length
	if(i > h)
	{
		i = h;
	}

//	x += 8 * kDigits;	// max. 10 digits

	s = buf;
	switch((*mainPixMap)->pixelSize)
	{
	  case 8:
		{
			register unsigned char	set = gC1tab[fgcolor];
			register unsigned char	clr = gC1tab[bkcolor];
			pixelSizeB = 1;

			gap = rowBytes / sizeof(long);

			while(i--)
			{
				c = 0xff & *s++;
				p = (long *) ((x * pixelSizeB) + baseAddr);
				src = &gCharSet8[8 * c];

				for(v = kHeight; v; v--)
				{
					bits = *src++;
					p[0] = ((bits & 0x80 ? set : clr) << 24) | ((bits & 0x40 ? set : clr) << 16) | ((bits & 0x20 ? set : clr) << 8) | (bits & 0x10 ? set : clr);
					p[1] = ((bits & 0x08 ? set : clr) << 24) | ((bits & 0x04 ? set : clr) << 16) | ((bits & 0x02 ? set : clr) << 8) | (bits & 0x01 ? set : clr);
					p += gap;
				}
				x += 8;
			}
		}
		break;
	  case 16:
		{
			register unsigned short	set = gC2tab[fgcolor];
			register unsigned short	clr = gC2tab[bkcolor];

			pixelSizeB = 2;

			gap = rowBytes / sizeof(long);

			while(i--)
			{
				c = 0xff & *s++;
				p = (long *) ((x * pixelSizeB) + baseAddr);
				src = &gCharSet8[8 * c];

				for(v = kHeight; v; v--)
				{
					bits = *src++;
					p[0] = ((bits & 0x80 ? set : clr) << 16) | (bits & 0x40 ? set : clr);
					p[1] = ((bits & 0x20 ? set : clr) << 16) | (bits & 0x10 ? set : clr);
					p[2] = ((bits & 0x08 ? set : clr) << 16) | (bits & 0x04 ? set : clr);
					p[3] = ((bits & 0x02 ? set : clr) << 16) | (bits & 0x01 ? set : clr);
					p += gap;
				}
				x += 8;
			}
		}
		break;
	  case 24:
	  case 32:
		{
			register long	set = gC4tab[fgcolor];
			register long	clr = gC4tab[bkcolor];

			pixelSizeB = 4;

			gap = rowBytes / sizeof(long);

			while(i--)
			{
				c = 0xff & *s++;
				p = (long *) ((x * pixelSizeB) + baseAddr);
				src = &gCharSet8[8 * c];

				for(v = kHeight; v; v--)
				{
					bits = *src++;
					p[0] = bits & 0x80 ? set : clr;
					p[1] = bits & 0x40 ? set : clr;
					p[2] = bits & 0x20 ? set : clr;
					p[3] = bits & 0x10 ? set : clr;
					p[4] = bits & 0x08 ? set : clr;
					p[5] = bits & 0x04 ? set : clr;
					p[6] = bits & 0x02 ? set : clr;
					p[7] = bits & 0x01 ? set : clr;
					p += gap;
				}
				x += 8;
			}
		}
		break;
	}
	va_end(marker);
}

void Dec8OnScreen(int x, int y, int fgcolor, int bkcolor, long value)		// just an alias for DecOnScreen
{
	DecOnScreen(x, y, fgcolor, value, bkcolor);
}

void Dec3OnScreen(int x, int y, int fgcolor, int bkcolor, long value)
{

	enum
	{
//		kDigits = 10
		kDigits = 6,
		kCellWidth = 4,
		kCellHeight = 5,
		kHeight = 5
	};

//	static long		oldValue = -1;
	PixMapHandle	mainPixMap;
	register Ptr	baseAddr;
	short			rowBytes;
	register short	gap;
	register long	bits;
	register int	i;
	register long	d;
	register char	spaceflag;
	register long	pixelSizeB;
	register long	*p;
	register int	ex;
	register int	ey;
	Boolean			minus;

//	if(oldValue == value)
//	{
//		return;
//	}
//	oldValue = value;

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	ex = (*mainPixMap)->bounds.right;
	ey = (*mainPixMap)->bounds.bottom;

	if(x < 0)
	{
		x += ex;
	}
	if(y < 0)
	{
		y += ey;
	}
	else
	{
		y += GetMBarHeight();
	}

	if(x > 0 && x < ex && y > 0 && y < ey - 6)
	{
		rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

		baseAddr = (y * rowBytes) + (*mainPixMap)->baseAddr;

		x += kCellWidth * kDigits;	// max. 10 digits

		minus = value < 0;
		if(minus)
		{
			value = -value;
		}
		switch((*mainPixMap)->pixelSize)
		{
		  case 8:
			{
				register unsigned char	set = gC1tab[fgcolor];
				register unsigned char	clr = gC1tab[bkcolor];
				pixelSizeB = 1;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= kCellWidth;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}
					bits = gDigits3[d];

					p = (long *) ((x * pixelSizeB) + baseAddr);

					p[0] = ((bits & 0x80000000 ? set : clr) << 24) | ((bits & 0x40000000 ? set : clr) << 16) | ((bits & 0x20000000 ? set : clr) << 8) | (bits & 0x10000000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x08000000 ? set : clr) << 24) | ((bits & 0x04000000 ? set : clr) << 16) | ((bits & 0x02000000 ? set : clr) << 8) | (bits & 0x01000000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x00800000 ? set : clr) << 24) | ((bits & 0x00400000 ? set : clr) << 16) | ((bits & 0x00200000 ? set : clr) << 8) | (bits & 0x00100000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x00080000 ? set : clr) << 24) | ((bits & 0x00040000 ? set : clr) << 16) | ((bits & 0x00020000 ? set : clr) << 8) | (bits & 0x00010000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x00008000 ? set : clr) << 24) | ((bits & 0x00004000 ? set : clr) << 16) | ((bits & 0x00002000 ? set : clr) << 8) | (bits & 0x00001000 ? set : clr);
					p[gap] = clr;
				}
			}
			break;
		  case 16:
			{
				register unsigned short	set = gC2tab[fgcolor];
				register unsigned short	clr = gC2tab[bkcolor];

				pixelSizeB = 2;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= kCellWidth;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}

					bits = gDigits3[d];
					p = (long *) ((x * pixelSizeB) + baseAddr);


					p[0] = ((bits & 0x80000000 ? set : clr) << 16) | (bits & 0x40000000 ? set : clr);
					p[1] = ((bits & 0x20000000 ? set : clr) << 16) | (bits & 0x10000000 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x08000000 ? set : clr) << 16) | (bits & 0x04000000 ? set : clr);
					p[1] = ((bits & 0x02000000 ? set : clr) << 16) | (bits & 0x01000000 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x00800000 ? set : clr) << 16) | (bits & 0x00400000 ? set : clr),
					p[1] = ((bits & 0x00200000 ? set : clr) << 16) | (bits & 0x00100000 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x00080000 ? set : clr) << 16) | (bits & 0x00040000 ? set : clr);
					p[1] = ((bits & 0x00020000 ? set : clr) << 16) | (bits & 0x00010000 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x00008000 ? set : clr) << 16) | (bits & 0x00004000 ? set : clr);
					p[1] = ((bits & 0x00002000 ? set : clr) << 16) | (bits & 0x00001000 ? set : clr);

					p[gap+0] = clr;
					p[gap+1] = clr;

				}
			}
			break;
		  case 24:
		  case 32:
			{
				register long	set = gC4tab[fgcolor];
				register long	clr = gC4tab[bkcolor];

				pixelSizeB = 4;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= kCellWidth;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}

					bits = gDigits3[d];
					p = (long *) ((x * pixelSizeB) + baseAddr);


					p[0] = (bits & 0x80000000 ? set : clr);
					p[1] = (bits & 0x40000000 ? set : clr);
					p[2] = (bits & 0x20000000 ? set : clr);
					p[3] = (bits & 0x10000000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x08000000 ? set : clr);
					p[1] = (bits & 0x04000000 ? set : clr);
					p[2] = (bits & 0x02000000 ? set : clr);
					p[3] = (bits & 0x01000000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x00800000 ? set : clr);
					p[1] = (bits & 0x00400000 ? set : clr);
					p[2] = (bits & 0x00200000 ? set : clr);
					p[3] = (bits & 0x00100000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x00080000 ? set : clr);
					p[1] = (bits & 0x00040000 ? set : clr);
					p[2] = (bits & 0x00020000 ? set : clr);
					p[3] = (bits & 0x00010000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x00008000 ? set : clr);
					p[1] = (bits & 0x00004000 ? set : clr);
					p[2] = (bits & 0x00002000 ? set : clr);
					p[3] = (bits & 0x00001000 ? set : clr);

					p[gap+0] = clr;
					p[gap+1] = clr;
					p[gap+2] = clr;
					p[gap+3] = clr;

				}
			}
			break;
		}
	}
}

void Dec5OnScreen(int x, int y, int fgcolor, int bkcolor, long value)
{

	enum
	{
//		kDigits = 10
		kDigits = 6,
		kCellWidth = 6 /* 4 */,
		kCellHeight = 5,
		kHeight = 5
	};

//	static long		oldValue = -1;
	PixMapHandle	mainPixMap;
	register Ptr	baseAddr;
	short			rowBytes;
	register short	gap;
	register long	bits;
	register int	i;
	register long	d;
	register char	spaceflag;
	register long	pixelSizeB;
	register long	*p;
	register int	ex;
	register int	ey;
	Boolean			minus;

//	if(oldValue == value)
//	{
//		return;
//	}
//	oldValue = value;

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	ex = (*mainPixMap)->bounds.right;
	ey = (*mainPixMap)->bounds.bottom;

	if(x < 0)
	{
		x += ex;
		if(x < 0)
		{
			x = 0;
		}
	}
	if(y < 0)
	{
		y += ey;
		if(y < 0)
		{
			y = 0;
		}
	}
	else
	{
		y += GetMBarHeight();
	}
	if(y + 6 > ey)
	{
		y = 0;
	}

	if(x > 0 && x < ex && y > 0 && y < ey)
	{
		rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

		baseAddr = (y * rowBytes) + (*mainPixMap)->baseAddr;

		x += kCellWidth * kDigits;	// max. 10 digits

		minus = value < 0;
		if(minus)
		{
			value = -value;
		}
		switch((*mainPixMap)->pixelSize)
		{
		  case 8:
			{
				register unsigned char	set = gC1tab[fgcolor];
				register unsigned char	clr = gC1tab[bkcolor];
				pixelSizeB = 1;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= kCellWidth;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}
					bits = gDigits5[d];

					p = (long *) ((x * pixelSizeB) + baseAddr);

					p[0] = ((bits & 0x80000000 ? set : clr) << 24) | ((bits & 0x40000000 ? set : clr) << 16) | ((bits & 0x20000000 ? set : clr) << 8) | (bits & 0x10000000 ? set : clr);
					*((short *) &p[1]) = ((bits & 0x08000000 ? set : clr) << 8) | (bits & 0x04000000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x02000000 ? set : clr) << 24) | ((bits & 0x01000000 ? set : clr) << 16) | ((bits & 0x08000000 ? set : clr) << 8) | (bits & 0x04000000 ? set : clr);
					*((short *) &p[1]) = ((bits & 0x02000000 ? set : clr) << 8) | (bits & 0x01000000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x00800000 ? set : clr) << 24) | ((bits & 0x00400000 ? set : clr) << 16) | ((bits & 0x00200000 ? set : clr) << 8) | (bits & 0x00100000 ? set : clr);
					*((short *) &p[1]) = ((bits & 0x00080000 ? set : clr) << 8) | (bits & 0x00040000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x00020000 ? set : clr) << 24) | ((bits & 0x00010000 ? set : clr) << 16) | ((bits & 0x00080000 ? set : clr) << 8) | (bits & 0x00040000 ? set : clr);
					*((short *) &p[1]) = ((bits & 0x00020000 ? set : clr) << 8) | (bits & 0x00010000 ? set : clr);
					p += gap;
					p[0] = ((bits & 0x00008000 ? set : clr) << 24) | ((bits & 0x00004000 ? set : clr) << 16) | ((bits & 0x00002000 ? set : clr) << 8) | (bits & 0x00001000 ? set : clr);
					*((short *) &p[1]) = ((bits & 0x00000800 ? set : clr) << 8) | (bits & 0x00000400 ? set : clr);
					p[gap] = clr;
					*((short *) &p[1]) = clr;
				}
			}
			break;
		  case 16:
			{
				register unsigned short	set = gC2tab[fgcolor];
				register unsigned short	clr = gC2tab[bkcolor];

				pixelSizeB = 2;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= kCellWidth;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}

					bits = gDigits5[d];
					p = (long *) ((x * pixelSizeB) + baseAddr);


					p[0] = ((bits & 0x80000000 ? set : clr) << 16) | (bits & 0x40000000 ? set : clr);
					p[1] = ((bits & 0x20000000 ? set : clr) << 16) | (bits & 0x10000000 ? set : clr);
					p[2] = ((bits & 0x08000000 ? set : clr) << 16) | (bits & 0x04000000 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x02000000 ? set : clr) << 16) | (bits & 0x01000000 ? set : clr);
					p[1] = ((bits & 0x00800000 ? set : clr) << 16) | (bits & 0x00400000 ? set : clr);
					p[2] = ((bits & 0x00200000 ? set : clr) << 16) | (bits & 0x00100000 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x00080000 ? set : clr) << 16) | (bits & 0x00040000 ? set : clr),
					p[1] = ((bits & 0x00020000 ? set : clr) << 16) | (bits & 0x00010000 ? set : clr);
					p[2] = ((bits & 0x00008000 ? set : clr) << 16) | (bits & 0x00004000 ? set : clr),
					p += gap;

					p[0] = ((bits & 0x00002000 ? set : clr) << 16) | (bits & 0x00001000 ? set : clr);
					p[1] = ((bits & 0x00000800 ? set : clr) << 16) | (bits & 0x00000400 ? set : clr);
					p[2] = ((bits & 0x00000200 ? set : clr) << 16) | (bits & 0x00000100 ? set : clr);
					p += gap;

					p[0] = ((bits & 0x00000080 ? set : clr) << 16) | (bits & 0x00000040 ? set : clr);
					p[1] = ((bits & 0x00000020 ? set : clr) << 16) | (bits & 0x00000010 ? set : clr);
					p[2] = ((bits & 0x00000008 ? set : clr) << 16) | (bits & 0x00000004 ? set : clr);

					p[gap+0] = clr;
					p[gap+1] = clr;

				}
			}
			break;
		  case 24:
		  case 32:
			{
				register long	set = gC4tab[fgcolor];
				register long	clr = gC4tab[bkcolor];

				pixelSizeB = 4;

				gap = rowBytes / sizeof(long);

				spaceflag = false;
				for(i = kDigits; i; i--)
				{
					x -= kCellWidth;

					if(spaceflag)
					{
						d = 10;
						if(minus)
						{
							minus = false;
							d = 11;
						}
					}
					else
					{
						d = value % 10;
						value /= 10;
						if(0 == value)
						{
							spaceflag = true;
						}
					}

					bits = gDigits5[d];
					p = (long *) ((x * pixelSizeB) + baseAddr);


					p[0] = (bits & 0x80000000 ? set : clr);
					p[1] = (bits & 0x40000000 ? set : clr);
					p[2] = (bits & 0x20000000 ? set : clr);
					p[3] = (bits & 0x10000000 ? set : clr);
					p[4] = (bits & 0x08000000 ? set : clr);
					p[5] = (bits & 0x04000000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x02000000 ? set : clr);
					p[1] = (bits & 0x01000000 ? set : clr);
					p[2] = (bits & 0x00800000 ? set : clr);
					p[3] = (bits & 0x00400000 ? set : clr);
					p[4] = (bits & 0x00200000 ? set : clr);
					p[5] = (bits & 0x00100000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x00080000 ? set : clr);
					p[1] = (bits & 0x00040000 ? set : clr);
					p[2] = (bits & 0x00020000 ? set : clr);
					p[3] = (bits & 0x00010000 ? set : clr);
					p[4] = (bits & 0x00008000 ? set : clr);
					p[5] = (bits & 0x00004000 ? set : clr);
					p += gap;

					p[0] = (bits & 0x00002000 ? set : clr);
					p[1] = (bits & 0x00001000 ? set : clr);
					p[2] = (bits & 0x00000800 ? set : clr);
					p[3] = (bits & 0x00000400 ? set : clr);
					p[4] = (bits & 0x00000200 ? set : clr);
					p[5] = (bits & 0x00000100 ? set : clr);
					p += gap;

					p[0] = (bits & 0x00000080 ? set : clr);
					p[1] = (bits & 0x00000040 ? set : clr);
					p[2] = (bits & 0x00000020 ? set : clr);
					p[3] = (bits & 0x00000010 ? set : clr);
					p[4] = (bits & 0x00000008 ? set : clr);
					p[5] = (bits & 0x00000004 ? set : clr);

					p[gap+0] = clr;
					p[gap+1] = clr;
					p[gap+2] = clr;
					p[gap+3] = clr;
					p[gap+4] = clr;
					p[gap+5] = clr;

				}
			}
			break;
		}
	}
}


void PutPixel(int x, int y, int color)
{
	PixMapHandle	mainPixMap;
	short			rowBytes;
	char			*cp;
	short			*sp;
	long			*lp;

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

	y += GetMBarHeight();

	switch((*mainPixMap)->pixelSize)
	{
	  case 8:
		cp = ((x) + (y * rowBytes) + (*mainPixMap)->baseAddr);
		*cp = gC1tab[color];
		break;
	  case 16:
		sp = (short *) ((x << 1) + (y * rowBytes) + (*mainPixMap)->baseAddr);
		*sp = gC2tab[color];
		break;
	  case 24:
	  case 32:
		lp = (long *) ((x << 2) + (y * rowBytes) + (*mainPixMap)->baseAddr);
		*lp = gC4tab[color];
		break;
	}
}

void Put4Pixel(int x, int y, int color)
{
	PixMapHandle	mainPixMap;
	short			rowBytes;
	char			*cp;
	short			*sp;
	long			*lp;

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

	y += GetMBarHeight();

	switch((*mainPixMap)->pixelSize)
	{
	  case 8:
		cp = ((x) + (y * rowBytes) + (*mainPixMap)->baseAddr);
//		cp[0] = gC1tab[color];
//		cp[1] = gC1tab[color];
		cp[0] = gC1tab[kOnScreenBlack];
		cp[1] = gC1tab[kOnScreenWhite];
		cp = (char *) ((long) cp + rowBytes);
		cp[0] = gC1tab[color];
		cp[1] = gC1tab[color];
		break;
	  case 16:
		sp = (short *) ((x << 1) + (y * rowBytes) + (*mainPixMap)->baseAddr);
//		sp[0] = gC2tab[color];
//		sp[1] = gC2tab[color];
		sp[0] = gC2tab[kOnScreenBlack];
		sp[1] = gC2tab[kOnScreenWhite];
		sp = (short *) ((long) sp + rowBytes);
		sp[0] = gC2tab[color];
		sp[1] = gC2tab[color];
		break;
	  case 24:
	  case 32:
		lp = (long *) ((x << 2) + (y * rowBytes) + (*mainPixMap)->baseAddr);
//		lp[0] = gC4tab[color];
//		lp[1] = gC4tab[color];
		lp[0] = gC4tab[kOnScreenBlack];
		lp[1] = gC4tab[kOnScreenWhite];
		lp = (long *) ((long) lp + rowBytes);
		lp[0] = gC4tab[color];
		lp[1] = gC4tab[color];
		break;
	}
}

static short gLockupMark[] =
{
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,

	0x0000,
	0x0000,
	0x07E0,
	0x0C30,
	0x0C30,
	0x0C30,
	0x0C30,
	0x3FFC,
	0x3FFC,
	0x3FFC,
	0x3FFC,
	0x3FFC,
	0x3FFC,
	0x0000,
	0x0000,
	0x0000

};

void LockupMark()
{
	PixMapHandle	mainPixMap;
	short			rowBytes;
	register char	*cp;
	register short	*sp;
	register long	*lp;

	register short	*src, data;
	register char	cset, cclr;
	register short	sset, sclr;
	register long	lset, lclr;

	int				x;
	int				y;
	int				color;

	src = &gLockupMark[16];

	mainPixMap = (*LMGetMainDevice())->gdPMap;

	rowBytes = (*mainPixMap)->rowBytes & 0x7fff;

	x = 640;
	y = 1;
	color = kOnScreenRed;
//	y += GetMBarHeight();

	switch((*mainPixMap)->pixelSize)
	{
	  case 8:
		cclr = gC1tab[kOnScreenBlack];
		cset = gC1tab[color];

		cp = ((x) + (y * rowBytes) + (*mainPixMap)->baseAddr);
		for(y = 0; y < 16; y++)
		{
			data = *src++;
			for(x = 0; x < 16; x++)
			{
				if(data & 0x8000)
				{
					*cp++ = cset;
				}
				else
				{
					*cp++ = cclr;
				}
				data = data << 1;
			}
		}
		break;
	  case 16:
		sclr = gC2tab[kOnScreenBlack];
		sset = gC2tab[color];

		sp = (short *) ((x << 1) + (y * rowBytes) + (*mainPixMap)->baseAddr);
		for(y = 0; y < 16; y++)
		{
			data = *src++;
			for(x = 0; x < 16; x++)
			{
				if(data & 0x8000)
				{
					*sp++ = sset;
				}
				else
				{
					*sp++ = sclr;
				}
				data = data << 1;
			}
		}
		break;
	  case 24:
	  case 32:
		lclr = gC4tab[kOnScreenBlack];
		lset = gC4tab[color];
		lp = (long *) ((x << 2) + (y * rowBytes) + (*mainPixMap)->baseAddr);
		for(y = 0; y < 16; y++)
		{
			data = *src++;
			for(x = 0; x < 16; x++)
			{
				if(data & 0x8000)
				{
					*lp++ = lset;
				}
				else
				{
					*lp++ = lclr;
				}
				data = data << 1;
			}
		}
		break;
	}
}

#endif // SIXTY_FOUR_BIT
#endif // MAC_DEBUG
