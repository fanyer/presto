/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NO_CARBON
#include <pwd.h>

#include "platforms/mac/util/macutils.h"
#include "platforms/mac/pi/CocoaOpMultimediaPlayer.h"

#import <Foundation/NSFileManager.h>

namespace {
#if __LP64__
	int32_t hash_long_to_int(uint64_t l) {
		l = (l) + (l << 18);
		l = l ^ (l >> 31);
		l = l * 21;
		l = l ^ (l >> 11);
		l = l + (l << 6);
		l = l ^ (l >> 22);
		return (int) l;
	}
#else // !__LP64__
#define hash_long_to_int(value__)  (value__)
#endif // !__LP64__
}

@interface OperaNSSound : NSSound {
	int _loop;
	NSString *_path;
}
- (id)initWithContentsOfURL:(NSURL *)url byReference:(BOOL)byRef;
- (id)initWithContentsOfFile:(NSString *)path byReference:(BOOL)byRef;
- (id)initWithContentsOfURL:(NSURL *)url byReference:(BOOL)byRef loop:(int)loop;
- (id)initWithContentsOfFile:(NSString *)path byReference:(BOOL)byRef loop:(int)loop;
- (int)loop;
@end
@implementation OperaNSSound
- (id)initWithContentsOfURL:(NSURL *)url byReference:(BOOL)byRef
{
	self = [super initWithContentsOfURL:url byReference:byRef];
	_loop = 1;
	_path = nil;
	return self;
}
- (id)initWithContentsOfFile:(NSString *)path byReference:(BOOL)byRef
{
	self = [super initWithContentsOfFile:path byReference:byRef];
	_loop = 1;
	_path = path;
	return self;
}
- (id)initWithContentsOfURL:(NSURL *)url byReference:(BOOL)byRef loop:(int)loop
{
	self = [super initWithContentsOfURL:url byReference:byRef];
	_loop = loop>0?loop:1;
	_path = nil;
	return self;
}
- (id)initWithContentsOfFile:(NSString *)path byReference:(BOOL)byRef loop:(int)loop
{
	self = [super initWithContentsOfFile:path byReference:byRef];
	_loop = loop>0?loop:1;
	_path = path;
	return self;
}
- (int)loop
{
	return _loop;
}
- (void)playAgain
{
	_loop--;
	[self play];
}
- (void)dealloc
{
	if (_path != nil)
	{
		[[NSFileManager defaultManager] removeFileAtPath:_path handler:nil];
		[_path release];
	}
	[super dealloc];
}
@end

@interface OperaNSSoundPlayDegate : NSObject {
	CocoaOpMultimediaPlayer* _player;
}
- (id) initWithPlayer:(CocoaOpMultimediaPlayer *)player;
- (void)sound:(OperaNSSound *)sound didFinishPlaying:(BOOL)aBool;
@end
@implementation OperaNSSoundPlayDegate

- (id) initWithPlayer:(CocoaOpMultimediaPlayer *)player
{
	self = [super init];
	_player = player;
	return self;
}

- (void)sound:(OperaNSSound *)sound didFinishPlaying:(BOOL)aBool
{
	if (aBool && [sound loop] > 1)
	{
		[sound playAgain];
	}
	else
		_player->AudioEnded(sound);
}
@end


OP_STATUS OpMultimediaPlayer::Create(OpMultimediaPlayer **newObj)
{
	*newObj = new CocoaOpMultimediaPlayer;
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

CocoaOpMultimediaPlayer::CocoaOpMultimediaPlayer()
	: m_sounds(NULL)
	, m_play_delegate(NULL)
{
	m_sounds = [[NSMutableArray alloc] init];
	m_play_delegate = [[OperaNSSoundPlayDegate alloc] initWithPlayer:this];
}

CocoaOpMultimediaPlayer::~CocoaOpMultimediaPlayer()
{
	StopAll();
}

uni_char *PrepareFileForPlaying(const uni_char* filename, const char* fileext)
{
	static char destnamebeg8[] = "/Users/";
	static char destnameend8[] = "/Library/Caches/operatempXXXX";
	static char cmd_format[] = "ln -f \"%s\" %s";	// cp also worked, but this should be more efficient

	struct passwd *pw = getpwuid(getuid());
	
	char *filename8 = StrToLocaleEncoding(filename);
	if (!filename8)
		return NULL;
	
	char *destname8 = OP_NEWA(char, strlen(destnamebeg8) + strlen(pw->pw_name) + strlen(destnameend8) + strlen(fileext) + 1/*strlen(".")*/ + 1);
	if (!destname8)
	{
		OP_DELETEA(filename8);
		return NULL;
	}
	strcpy(destname8, destnamebeg8);
	strcat(destname8, pw->pw_name);
	strcat(destname8, destnameend8);
	strcat(destname8, ".");
	strcat(destname8, fileext);
	int file = mkstemps(destname8, strlen(fileext) + 1/*strlen(".")*/);
	if (file)
		close(file);
	else
		return NULL;
	
	// Allocate the memory for the command
	char *cmd = OP_NEWA(char, strlen(cmd_format) + strlen(filename8) + strlen(destname8) + 1);
	if (cmd == NULL)
	{
		OP_DELETEA(filename8);
		OP_DELETEA(destname8);
		return NULL;
	}
	
	// Build the command string
	if (sprintf(cmd, cmd_format, filename8, destname8) <= 0)
	{
		OP_DELETEA(cmd);
		OP_DELETEA(filename8);
		OP_DELETEA(destname8);
		return NULL;
	}
	
	// Call the command
	int ret = system(cmd);
	
	OP_DELETEA(cmd);
	OP_DELETEA(filename8);

	// Check the command worked!
	if (ret != 0)
		return NULL;
	
	uni_char *destname = StrFromLocaleEncoding(destname8);

	OP_DELETEA(destname8);

	return destname;
}

void CocoaOpMultimediaPlayer::Stop(UINT32 in_id)
{
	for (unsigned i = 0; i<[m_sounds count]; i++)
	{
		id snd = [m_sounds objectAtIndex:i];
		if (in_id == (UINT32)hash_long_to_int((long)snd))
		{
			[snd stop];
		}
	}
}

void CocoaOpMultimediaPlayer::StopAll()
{
	for (unsigned i = 0; i<[m_sounds count]; i++)
	{
		id snd = [m_sounds objectAtIndex:i];
		[snd stop];
	}
}

OP_STATUS CocoaOpMultimediaPlayer::LoadMedia(const uni_char* filename)
{
	return OpStatus::ERR;
}

OP_STATUS CocoaOpMultimediaPlayer::PlayMedia()
{
	return OpStatus::ERR;
}

OP_STATUS CocoaOpMultimediaPlayer::PauseMedia()
{
	return OpStatus::ERR;
}

OP_STATUS CocoaOpMultimediaPlayer::StopMedia()
{
	return OpStatus::ERR;
}

void CocoaOpMultimediaPlayer::SetMultimediaListener(OpMultimediaListener* listener)
{
}

void CocoaOpMultimediaPlayer::AudioEnded(id sound)
{
	[m_sounds removeObject:sound];
}

#endif // NO_CARBON
