/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPMULTIMEDIAPLAYER_H
#define OPMULTIMEDIAPLAYER_H

class OpView;
class OpMultimediaPlayer;

class OpMultimediaListener
{
public:
	virtual ~OpMultimediaListener() {}

	virtual void OnProgress(OpMultimediaPlayer* player, INT32 position, INT32 length) = 0;
};

/** @short Multimedia player singleton - support playback of audio and video.
 *
 * This class can, but doesn't have to, be implemented on the misc.
 * Opera platforms. If it is not implemented, handling of audio and
 * video will be via plugins and external applications only.
 *
 * It is advised that at least support for uncompressed wave audio be
 * implemented.
 *
 * About the identifiers returned from the Play* methods: these are
 * expected to be unique among all currently playing items, audio,
 * video, and MIDI alike.
 */
class OpMultimediaPlayer
{
public:
	/** Create an OpMultimediaPlayer object. */
	static OP_STATUS Create(OpMultimediaPlayer** new_opmultimediaplayer);

	/** Destructor. Stop playback of all audio and video. */
	virtual ~OpMultimediaPlayer() {}

	/** Stop playback of the specified "multimedia item"
	 *
	 * @param id the identifier of the item to stop
	 */
	virtual void Stop(UINT32 id) = 0;

	/** Stop playback of all "multimedia items". */
	virtual void StopAll() = 0;

	virtual OP_STATUS LoadMedia(const uni_char* filename) = 0;
	virtual OP_STATUS PlayMedia() = 0;
	virtual OP_STATUS PauseMedia() = 0;
	virtual OP_STATUS StopMedia() = 0;

	/** Set the multimedia listener. */
	virtual void SetMultimediaListener(OpMultimediaListener* listener) = 0;
};

#endif // OPMULTIMEDIAPLAYER_H
