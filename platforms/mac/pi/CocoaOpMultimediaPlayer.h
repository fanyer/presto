/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef COCOA_OPMULTIMEDIAPLAYER_H
#define COCOA_OPMULTIMEDIAPLAYER_H

#ifdef NO_CARBON

#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"

#define BOOL NSBOOL
#import <AppKit/NSSound.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#undef BOOL

class CocoaOpMultimediaPlayer : public OpMultimediaPlayer
{
public:
	CocoaOpMultimediaPlayer();
	virtual ~CocoaOpMultimediaPlayer();

	virtual void Stop(UINT32 id);
	virtual void StopAll();

	virtual OP_STATUS LoadMedia(const uni_char* filename);
	virtual OP_STATUS PlayMedia();
	virtual OP_STATUS PauseMedia();
	virtual OP_STATUS StopMedia();

	virtual void SetMultimediaListener(OpMultimediaListener* listener);

	void AudioEnded(id sound);
private:
	NSMutableArray* m_sounds;
	NSObject* m_play_delegate;
};

#endif // NO_CARBON

#endif // COCOA_OPMULTIMEDIAPLAYER_H
