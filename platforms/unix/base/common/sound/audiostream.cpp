// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2001-2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#include "core/pch.h"

#include "audiostream.h"

AudioStream::AudioStream()
{
	errcode = 0;
	samplerate = 0;
	samplesize = 0;
	pos = 0;
	channels = 0;
	endianness = IGNORE_ENDIAN;
}

AudioStream::~AudioStream()
{
}

int AudioStream::Duration()
{
	return (int)((double)Length() / (double)SampleRate() * 1000.0);
}

int AudioStream::GetData(void *buf, int buflen)
{
	int bytes = GetData(pos, buf, buflen);
	if (bytes > 0)
		pos += bytes;
	return bytes;
}

void AudioStream::Rewind()
{
	pos = 0;
}

bool AudioStream::Seek(int p)
{
	if (p >= Length() || p < 0)
	{
		pos = 0;
		return false;
	}
	pos = p;
	return true;
}
