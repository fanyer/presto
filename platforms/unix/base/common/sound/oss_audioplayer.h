// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#ifndef __OSS_AUDIOPLAYER_H__
#define __OSS_AUDIOPLAYER_H__

#include "audioplayer.h"

#include <pthread.h>

/**
 * OSS implementation of the AudioPlayer. It uses /dev/dsp .
 */
class OssAudioPlayer : public AudioPlayer
{
	friend void *StartMixerThread(void *);

	int fragsize;

private:
	int audiohandle;
 	pthread_t thread;
	pthread_mutex_t mutex;

	static void *StartMixerThread(void *);
	void Run();
	void OutputSound(unsigned char *data, int bytes);

protected:
	bool OpenDevice();

	/**
	 * Close the sound device.
	 */
	void CloseDevice();
public:
	OssAudioPlayer();
	~OssAudioPlayer();

	void Lock();
	void Unlock();
};

extern OssAudioPlayer* g_audio_player;

#endif // __OSS_AUDIOPLAYER_H__
