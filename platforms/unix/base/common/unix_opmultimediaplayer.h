/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne, Espen Sand
 */
#ifndef UNIX_OPMULTIMEDIAPLAYER_H
#define UNIX_OPMULTIMEDIAPLAYER_H

#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"

# include "platforms/unix/base/common/sound/oss_audioplayer.h"

class UnixOpMultimediaPlayer : public OpMultimediaPlayer
{
public:
	UnixOpMultimediaPlayer() {}
	~UnixOpMultimediaPlayer() { StopAll(); }

	void Stop(UINT32 id);
	void StopAll();

	OP_STATUS LoadMedia(const uni_char* filename) { return OpStatus::ERR; }
	OP_STATUS PlayMedia() { return OpStatus::ERR; }
	OP_STATUS PauseMedia() { return OpStatus::ERR; }
	OP_STATUS StopMedia() { return OpStatus::ERR; }
	void SetMultimediaListener(OpMultimediaListener* listener) {}

};

#endif //UNIX_OPMULTIMEDIAPLAYER_H
