// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#include "core/pch.h"

#include "oss_audioplayer.h"

#include "audiostream.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>


OssAudioPlayer* g_audio_player;


OssAudioPlayer::OssAudioPlayer()
{
	pthread_mutex_init(&mutex, 0);
	audiohandle = -1;
	thread = 0;
}

OssAudioPlayer::~OssAudioPlayer()
{
	Lock();
	pthread_t thread = this->thread;
	Unlock();

	if (thread != 0)
	{
		StopMixer();
		pthread_join(thread, 0);
	}
}

bool OssAudioPlayer::OpenDevice()
{
	Lock();

	bool result = true;

	if (audiohandle == -1 && thread == 0)
	{
		if (pthread_create(&thread, 0, StartMixerThread, this) < 0)
			result = false;
	}

	Unlock();
	return result;
}

void OssAudioPlayer::CloseDevice()
{
	Lock();
	if (audiohandle != -1)
	{
		close(audiohandle);
		audiohandle = -1;
	}
	Unlock();
}

void *OssAudioPlayer::StartMixerThread(void *arg)
{
	((OssAudioPlayer *)arg)->Run();
	return 0;
}

void OssAudioPlayer::Run()
{
	Lock();
	audiohandle = open("/dev/dsp", O_WRONLY | O_NONBLOCK, 0);

	unsigned short endian_test = 0x1234;
	AudioStream::Endianness system_endianness = *(unsigned char *)&endian_test == 0x12 ? AudioStream::BIG : AudioStream::LITTLE;

	if (audiohandle != -1)
	{
		int samplesize = 16;
		int dsp_speed = MixerRate();
		int stereo = 1;
//		int prof = APF_NORMAL;
		int format = AFMT_QUERY;
		ioctl(audiohandle, SNDCTL_DSP_SAMPLESIZE, &samplesize);
//		ioctl(audiohandle, SNDCTL_DSP_PROFILE, &prof);
		ioctl(audiohandle, SNDCTL_DSP_STEREO, &stereo);
		ioctl(audiohandle, SNDCTL_DSP_SPEED, &dsp_speed);

		int mask;
		ioctl(audiohandle, SNDCTL_DSP_GETFMTS, &mask);
		if (system_endianness == AudioStream::LITTLE && (mask & AFMT_S16_LE))
		{
			format = AFMT_S16_LE;
		}
		else if (system_endianness == AudioStream::BIG && (mask & AFMT_S16_BE))
		{
			format = AFMT_S16_BE;
		}
		else
		{
			// Not supported
			format = AFMT_QUERY;
		}

		if (format != AFMT_QUERY)
			ioctl(audiohandle, SNDCTL_DSP_SETFMT, &format);

		fcntl(audiohandle, F_SETFL, fcntl(audiohandle, F_GETFL, 0) & ~O_NONBLOCK);
		ioctl(audiohandle, SNDCTL_DSP_GETBLKSIZE, &fragsize);
		if (fragsize > 8192)
		{
			int frag = 0x0000000d;
			ioctl(audiohandle, SNDCTL_DSP_SETFRAGMENT, &frag);			
			ioctl(audiohandle, SNDCTL_DSP_GETBLKSIZE, &fragsize);
		}
	}
	else
	{
		// Device open failed.
		Unlock();
		return;
	}

	int bufsize = fragsize * 2;
	Unlock();
	unsigned char *data = OP_NEWA(unsigned char, bufsize);
	unsigned char *tmpbuf = OP_NEWA(unsigned char, bufsize);
	while (!StopInProgress())
	{
		int bytes = SoundMixer(data, tmpbuf, bufsize);
		if (bytes > 0)
			OutputSound(data, bytes);
		else if (AutoCloseDevice())
			break;
		else
			usleep(20000);
	}
	CloseDevice();

	OP_DELETEA(data);
	OP_DELETEA(tmpbuf);

	Lock();
	thread = 0;
	Unlock();
}

void OssAudioPlayer::Lock()
{
	pthread_mutex_lock(&mutex);
}

void OssAudioPlayer::Unlock()
{
	pthread_mutex_unlock(&mutex);
}

void OssAudioPlayer::OutputSound(unsigned char *data, int bytes)
{
	OpenDevice();
	Lock();
	int handle = audiohandle;
	Unlock();

	audio_buf_info info;
	bool wait;
	do
	{
		// Some synchronization. Make sure that we haven't sent too much
		// data to the device that still is to be played.

		ioctl(handle, SNDCTL_DSP_GETOSPACE, &info);
		int diff = info.fragstotal - info.fragments;
		wait = (diff > 4 && diff*info.fragsize > 20000) && !StopInProgress();
		if (!wait)
			break;
		usleep(5000);
	} while (wait);
	write(handle, data, bytes);
}

