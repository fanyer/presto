/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_SOUND_UTILS_H
#define DESKTOP_SOUND_UTILS_H

namespace SoundUtils {
	OP_STATUS SoundIt(const OpStringC &sndfile, BOOL force, BOOL async=TRUE);
};

#endif // DESKTOP_SOUND_UTILS_H
