/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifndef NO_CARBON
#ifdef MACGOGI
#include "platforms/macgogi/pi/MacOpMultimediaPlayer.h"
#include "platforms/macgogi/util/OpFileUtils.h"
#include "platforms/macgogi/util/utils.h"
#else
#include "platforms/mac/pi/MacOpMultimediaPlayer.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#endif

// #define SYNCHRONOUS_MOVIE_PLAY

#define FILE_CLEANUP

OP_STATUS OpMultimediaPlayer::Create(OpMultimediaPlayer **newObj)
{
	*newObj = new MacOpMultimediaPlayer;
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

MacOpMultimediaPlayer::MacOpMultimediaPlayer()
: movie(NULL)
{
	EnterMovies();
}

MacOpMultimediaPlayer::~MacOpMultimediaPlayer()
{
	ExitMovies();
	if(movie)
	{
		DisposeMovie(movie);
	}
}

void MacOpMultimediaPlayer::Stop(UINT32 id)
{
	if (movie)
	{
		DisposeMovie(movie);
	}
	movie = NULL;
}

void MacOpMultimediaPlayer::StopAll()
{
	if (movie)
	{
		DisposeMovie(movie);
	}
	movie = NULL;
}

OP_STATUS MacOpMultimediaPlayer::LoadMedia(const uni_char* filename)
{
	return OpStatus::ERR;
}

OP_STATUS MacOpMultimediaPlayer::PlayMedia()
{
	return OpStatus::ERR;
}

OP_STATUS MacOpMultimediaPlayer::PauseMedia()
{
	return OpStatus::ERR;
}

OP_STATUS MacOpMultimediaPlayer::StopMedia()
{
	return OpStatus::ERR;
}

void MacOpMultimediaPlayer::SetMultimediaListener(OpMultimediaListener* listener)
{
}
#endif // NO_CARBON
