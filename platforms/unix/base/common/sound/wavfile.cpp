// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#include "core/pch.h"

#include "wavfile.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


// #define DEBUG_WAV
#ifdef DEBUG_WAV
inline void debugdump(const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	printf("DEBUG: ");
	vprintf(fmt, l);
	va_end(l);
}
#endif


inline unsigned int read_le32(unsigned char *data)
{
	return data[0] | (data[1] << 8) | 
		(data[2] << 16) | (data[3] << 24);
}

inline unsigned int read_le16(unsigned char *data)
{
	return data[0] | (data[1] << 8);
}


WavFile::WavFile()
{
	totalsize = 0;
	file = 0;
	data_offs = 0;
	length = 0;
}

WavFile::~WavFile()
{
	if (file)
		fclose(file);
}

bool WavFile::Open(const char *filename)
{
	file = fopen(filename, "r");
	if (!file)
	{
#ifdef DEBUG_WAV
		debugdump("Could not open file %s. errno:%d (%s)\n",
				  filename, errno, strerror(errno));
#endif
		errcode = errno;
		return false;
	}
#ifdef DEBUG_WAV
	debugdump("File %s opened\n", filename);
#endif
	unsigned char buffer[128];

	// RIFF chunk
	if (fread(buffer, 1, 12, file) != 12)
	{
#ifdef DEBUG_WAV
		debugdump("fread() failed. errno:%d (%s)\n", filename, errno, strerror(errno));
#endif
		errcode = errno;
		return false;
	}
	if (strncmp((char *)buffer, "RIFF", 4) || 
		strncmp((char *)buffer+8, "WAVE", 4))
	{
#ifdef DEBUG_WAV
		debugdump("Unrecognized header.\n");
#endif
		return false;
	}
	totalsize = read_le32(buffer+4);
#ifdef DEBUG_WAV
	debugdump("Total size: %d\n", totalsize);
#endif


	// FORMAT chunk
	if (fread(buffer, 1, 24, file) != 24)
	{
		errcode = errno;
		return false;
	}
	if (strncmp((char *)buffer, "fmt ", 4))
	{
#ifdef DEBUG_WAV
		debugdump("Unrecognized chunk header\n");
#endif
		return false;
	}
	int chunklength = read_le32(buffer+4);
#ifdef DEBUG_WAV
	debugdump("FORMAT chunk length: %d\n", chunklength);
#endif
	if (chunklength < 16)
		return false;
//	int formattag = read_le16(buffer+8);
	channels = read_le16(buffer+10);
	samplerate = read_le32(buffer+12);
//	int bytes_per_sec = read_le32(buffer+16);
//	int blockalign = read_le16(buffer+20);
	samplesize = read_le16(buffer+22);
	if (samplesize == 16)
		endianness = LITTLE;
	if (chunklength > 16)
		fseek(file, chunklength-16, SEEK_CUR);
#ifdef DEBUG_WAV
	debugdump("Format tag:%x  Channels:%d  Sample rate:%d  Bytes per sec:%d  Block align:%d  Sample size:%d\n", formattag, channels, samplerate, bytes_per_sec, blockalign, samplesize);
#endif

	// DATA chunk
	while (1)
	{
		if (fread(buffer, 1, 8, file) != 8)
		{
#ifdef DEBUG_WAV
			debugdump("Unexpected end of file\n");
#endif
			errcode = errno;
			return false;
		}
		if (!strncmp((char *)buffer, "data", 4))
		{
#ifdef DEBUG_WAV
			debugdump("Found data chunk!\n");
#endif
			break;
		}
#ifdef DEBUG_WAV
		debugdump("Ignored chunk %c%c%c%c\n", buffer[0], buffer[1], buffer[2], buffer[3]);
#endif
		int skip = read_le32(buffer+4);
		fseek(file, skip, SEEK_CUR);
	}
	if (feof(file))
	{
#ifdef DEBUG_WAV
		debugdump("Unexpected end of file\n");
#endif
		return false;
	}
	length = read_le32(buffer+4);
#ifdef DEBUG_WAV
	debugdump("Length of data chunk: %d\n", length);
#endif

	data_offs = ftell(file);

	return true;
}

int WavFile::Length()
{
	return length;
}

int WavFile::GetData(int offset, void *buf, int buflen)
{
	int bps = (samplesize >> 3) * channels;
	if (fseek(file, data_offs+offset*bps, SEEK_SET) == -1)
	{
#ifdef DEBUG_WAV
		debugdump("fseek() failed. errno:%d (%s)\n", errno, strerror(errno));
#endif
		errcode = errno;
		return -1;
	}
	if ((offset + buflen) * bps > length)
	{
		buflen = length/bps - offset;
	}
	int bytes_read = fread(buf, bps, buflen, file);
	if (bytes_read != buflen)
	{
		if (ferror(file))
		{
#ifdef DEBUG_WAV
			debugdump("fread() failed. errno:%d (%s)\n", errno, strerror(errno));
#endif
			errcode = errno;
			return -1;
		}
	}
	return bytes_read;
}

