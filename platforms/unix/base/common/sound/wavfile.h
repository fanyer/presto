// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#ifndef __WAVFILE_H__
#define __WAVFILE_H__

#include "audiostream.h"

#include <stdio.h>

/**
 * Extension of AudioFile class to support the WAV file format.
 */
class WavFile : public AudioStream
{
private:
	FILE *file;
	int totalsize;
	int data_offs;
	int length;

public:
	WavFile();
	~WavFile();

	bool Open(const char *filename);
	int Length();
	int GetData(int offset, void *buf, int buflen);
};

#endif // __WAVFILE_H__
