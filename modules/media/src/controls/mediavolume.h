/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_VOLUME_H
#define MEDIA_VOLUME_H

#ifdef MEDIA_PLAYER_SUPPORT

class VolumeString
{
public:
	/** Generate the string "Media Volume X"
	 *
	 * @param n A volume level 0 <= n <= 100
	 * @return A generated string with lifetime tied to this instance.
	 *         It is only safe to use until the next Get() call.
	 */
	const char* Get(unsigned n)
	{
		OP_ASSERT(n <= 100);
		op_snprintf(str, sizeof(str), "Media Volume %u", n);
		return str;
	}
private:
	// Enough storage for "Media Volume 100\0"
	char str[17]; // ARRAY OK 2012-02-23 philipj
};

#endif // MEDIA_PLAYER_SUPPORT
#endif // MEDIA_VOLUME_H
