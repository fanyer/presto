/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne, Espen Sand
 */
#include "core/pch.h"

#include "sound/audioplayer.h"
#include "sound/wavfile.h"
#include <stdlib.h>

#include "unix_opmultimediaplayer.h"

void UnixOpMultimediaPlayer::Stop(UINT32 id)
{
	if (g_audio_player)
	{
		g_audio_player->Stop(id);
	}
}

void UnixOpMultimediaPlayer::StopAll()
{
	if (g_audio_player)
	{
		g_audio_player->StopMixer();
	}
}
