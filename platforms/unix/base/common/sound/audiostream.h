// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2001-2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#ifndef __AUDIOSTREAM_H__
#define __AUDIOSTREAM_H__

/**
 * Base class for an audio stream. Reimplement it to support
 * a specific file format, streaming protocol or whatever.
 */
class AudioStream
{
public:
	enum Endianness
	{
		IGNORE_ENDIAN, ///< Endianness irrelevant
		LITTLE, ///< Little endian (LSB first)
		BIG ///< Big endian (MSB first)
	};

private:
	int pos;

protected:
	int errcode;
	int samplerate;
	int samplesize;
	int channels;
	Endianness endianness; ///< initial value: IGNORE_ENDIAN

public:
	AudioStream();
	virtual ~AudioStream();

	/**
	 * Returns an error (errno value) if an error has occured.
	 * Otherwise 0 is returned.
	 */
	int Error() { return errcode; }

	/**
	 * Returns the number of channels.
	 * 1 indicates mono, 2 indicates stereo.
	 */
	int NumChannels() { return channels; }

	/**
	 * Returns the number of bits per sample for one channel.
	 */
	int SampleSize() {	return samplesize;	}

	/**
	 * Returns number of samples per second.
	 */
	int SampleRate() { return samplerate; }

	/**
	 * Returns the byte order (aka endianness) of the audio stream
	 */
	Endianness GetByteOrder() { return endianness; }

	/**
	 * Returns the length in samples.
	 * @return the number of samples, or -1 if unknown.
	 */
	virtual int Length() = 0;

	/**
	 * Returns the length in milliseconds.
	 */
	int Duration();

	/**
	 * Get sampledata
	 *
	 * @param offset which position (in samples) to start copying from
	 * @param buf the buffer to fill the data into
	 * @param buflen length of the buffer in bytes
	 * @return the numbers of samples copied, or -1 if an error occured
	 */
	virtual int GetData(int offset, void *buf, int buflen) = 0;

	/**
	 * Get sampledata from the current position, and increase the
	 * position counter accordingly.
	 *
	 * @param buf the buffer to fill the data into
	 * @param buflen length of the buffer
	 * @return the numbers of samples copied, or -1 if an error occured
	 */
	int GetData(void *buf, int buflen);

	/**
	 * Reset the position counter.
	 */
	void Rewind();

	/**
	 * Set the position counter. It will be set to 0 if 
	 * @param new value for position counter
	 * @return true if the operation was successful, false
	 * if the value specified was out of range.
	 */
	bool Seek(int p);

	int GetPos() { return pos; }
};

#endif // __AUDIOSTREAM_H__
