/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#ifdef _UNIX_DESKTOP_
# include "platforms/unix/base/common/sound/wavfile.h"
# include "platforms/unix/base/common/unix_opmultimediaplayer.h"
#endif // _UNIX_DESKTOP_

OP_STATUS SoundUtils::SoundIt(const OpStringC &sndfile, BOOL force, BOOL async)
{
	OP_STATUS rc = OpStatus::OK;

	if ((!g_pcui->GetIntegerPref(PrefsCollectionUI::SoundsEnabled) && !force) || !IsStr(sndfile.CStr()))
		return rc;

	OpString osPrefsNoSound;
	TRAP_AND_RETURN(rc, g_languageManager->GetStringL(Str::SI_NO_SOUND_STRING, osPrefsNoSound));
	if (sndfile.CompareI(osPrefsNoSound) == 0 || sndfile.Compare("None") == 0)
		return OpStatus::ERR;

#if defined(MSWIN)
	rc = ( sndPlaySound(sndfile.CStr(), (async ? SND_ASYNC : SND_SYNC) | SND_NODEFAULT)==TRUE ? OpStatus::OK : OpStatus::ERR);
#elif defined(_UNIX_DESKTOP_)
	if (g_audio_player)
	{
		char *filename = uni_down_strdup(sndfile.CStr());
		if (!filename)
			return OpStatus::ERR_NO_MEMORY;
		WavFile *wav = OP_NEW(WavFile, ());
		if (!wav)
		{
			rc = OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			if (wav->Open(filename))
			{
				g_audio_player->Play(wav);
			}
			else
			{
				OP_DELETE(wav);
				rc = OpStatus::ERR;
			}
		}
		op_free(filename);
	}
#elif defined(_MACINTOSH_)
	rc = (MacPlaySound(sndfile.CStr(), async) == TRUE) ? OpStatus::OK : OpStatus::ERR;
#endif

	return rc;
}
