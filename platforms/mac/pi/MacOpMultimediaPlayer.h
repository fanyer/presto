/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MACOPMULTIMEDIAPLAYER_H
#define MACOPMULTIMEDIAPLAYER_H

#ifndef NO_CARBON

#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"

#define Size MacSize
#define Style MacFontStyle

#include <QuickTime/QuickTime.h>

#undef Size
#undef Style

class MacOpMultimediaPlayer : public OpMultimediaPlayer
{
private:
	Movie			movie;

public:
	MacOpMultimediaPlayer();
	virtual ~MacOpMultimediaPlayer();

	virtual void Stop(UINT32 id);

	virtual void StopAll();

	virtual OP_STATUS LoadMedia(const uni_char* filename);
	virtual OP_STATUS PlayMedia();
	virtual OP_STATUS PauseMedia();
	virtual OP_STATUS StopMedia();

	virtual void SetMultimediaListener(OpMultimediaListener* listener);
};

#endif // NO_CARBON

#endif // MACOPMULTIMEDIAPLAYER_H
