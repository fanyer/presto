// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
// Copyright (C) 2002, Morten Stenshorne

// Used in the Opera web browser with permission.

#include "core/pch.h"

#include "audioplayer.h"
#include "audiostream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class AudioPlayerItem
{
public:
	static int audio_id_counter;
	int id;
	AudioStream *s;
	AudioPlayerItem *next;
	int pos;
	int loops_left;
	bool dead;

	AudioPlayerItem(AudioStream *s, int loops);

	int Count();
	~AudioPlayerItem();
};

int AudioPlayerItem::audio_id_counter = 1;

AudioPlayerItem::AudioPlayerItem(AudioStream *s, int loops)
	: s(s)
{
	next = 0;
	pos = 0;
	loops_left = loops;
	dead = false;
	id = audio_id_counter ++;
	if (audio_id_counter < 0) // Ehm... when you have played 2 billion streams, this might happen. :)
		audio_id_counter = 1;
}

AudioPlayerItem::~AudioPlayerItem()
{
	OP_DELETE(s);
}

int AudioPlayerItem::Count()
{
	int count = 0;
	for (AudioPlayerItem *item=this; item; item=item->next)
		count ++;
	return count;
}

///////////////////////////////////////////

AudioPlayer::AudioPlayer()
{
	stop = true;
	mixerrate = 44100; // Don't try to change it. Not implemented properly.
	first = 0;
	autoclose = false;
	fade = false;
	volume = 1000;
	output_level = 1000;
}

AudioPlayer::~AudioPlayer()
{
}

void AudioPlayer::CleanUp()
{
	Lock();
	AudioPlayerItem *item;
	AudioPlayerItem *prev = 0;
	for (item=first; item;)
	{
		AudioPlayerItem *next = item->next;
		if (item->dead)
		{
			if (item == first)
				first = next;
			else if (prev)
				prev->next = next;
			OP_DELETE(item);
		}
		else
		{
			prev = item;
		}
		item = next;
	}
	Unlock();
}

void AudioPlayer::SetAutoCloseDevice(bool autoclose)
{
	Lock();
	this->autoclose = autoclose;
	Unlock();
}

bool AudioPlayer::Play(AudioStream *s, int loops, int *id)
{
	bool result = true;
	AudioPlayerItem *new_item = OP_NEW(AudioPlayerItem, (s, loops));
	if (!new_item)
	{
		if (id)
			*id = -1;
		result = false;
	}
	else
	{
		if (id)
			*id = new_item->id;

		Lock();
	
		AudioPlayerItem *item;
		for (item=first; item && item->next; item=item->next)
			;
		if (!item)
			first = new_item;
		else
			item->next = new_item;

		Unlock();

		stop = false;
		if (!OpenDevice())
		{
			new_item->s = 0;
			new_item->dead = true;
			result = false;
		}
	}
	CleanUp();

	return result;
}

bool AudioPlayer::Stop(int id)
{
	Lock();
	if (!first)
	{
		Unlock();
		return false; // nothing is playing now
	}

	AudioPlayerItem *item;
	for (item=first; item; item=item->next)
	{
		if (item->id == id)
		{
			item->dead = true;
			break;
		}
	}
	bool result = !!item;
	Unlock();
	CleanUp();

	return result;
}

void AudioPlayer::StopMixer()
{
	Lock();
	stop = true;
	for (AudioPlayerItem *item=first; item; item=item->next)
	{
		item->dead = true;
	}
	Unlock();
	CleanUp();
}

void AudioPlayer::Fade(int destination, int duration)
{
	Lock();
	if (destination == volume || destination < 0 || destination > 1000 || duration < 0)
	{
		Unlock();
		return;
	}
	if (duration == 0)
	{
		SetVolume(destination);
		Unlock();
		return;
	}

	fade_delta = abs(destination-volume);
	fade_destination = destination;
	fade_duration = duration;
	fade_counter = 0;
	fade_samples_per_step = ((double)fade_duration * (double)mixerrate) / 
		((double)fade_delta * 1000);
	fade = true;
	Unlock();
}

void AudioPlayer::SetVolume(int vol)
{
	Lock();
	fade = false;
	volume = vol;
	Unlock();
}

bool AudioPlayer::StopInProgress()
{
	Lock();
	bool s = stop;
	Unlock();
	return s;
}

bool AudioPlayer::AutoCloseDevice()
{
	Lock();
	bool ac = autoclose;
	Unlock();
	return ac;
}


namespace
{
	inline int ConvertSample(unsigned char *data, int samplesize)
	{
		if (samplesize == 16)
			return *(short *)data;
		else if (samplesize == 8)
			return (data[0] - 128) << 8;
		else // unsupported
			return 0;
	}

	int AdjustOutputLevel(int sample, int &output_level)
	{
		static int counter = 0;
		int new_sample = sample * output_level / 1000;
		if (new_sample > 32767)
		{
			// Output level too high. Adjust.
			output_level = 32767 * 1000 / sample;
			new_sample = sample * output_level / 1000;
		}
		else if (new_sample < -32768)
		{
			// Output level too high. Adjust.
			output_level = 32768 * 1000 / -sample;
			new_sample = sample * output_level / 1000;
		}
		else
		{
			// Try to increase the output level slightly,
			// to ensure maximum output level at all times.
			if (++counter == 100)
			{
				counter = 0;
				output_level += 10;
				if (output_level > 1000)
					output_level = 1000;
			}
		}
		return new_sample;
	}
}

int AudioPlayer::SoundMixer(unsigned char *data, unsigned char *tmpbuf, int bufsize)
{
	int *sample_sum = (int *)data;

	Lock();
	int count = first->Count();
	Unlock();

	unsigned short endian_test = 0x1234;
	AudioStream::Endianness endianness = *(unsigned char *)&endian_test == 0x12 ? AudioStream::BIG : AudioStream::LITTLE;

	if (count > 0)
	{
		memset(data, 0, bufsize);
		Lock();
		int active_count = 0;
		for (AudioPlayerItem *item=first; item; item=item->next)
		{
			if (item->dead)
				continue;

			active_count ++;

			AudioStream *s = item->s;
			int channels = s->NumChannels();
			int samplesize = s->SampleSize();
			int samplesize_bytes = samplesize >> 3;
			int rate = s->SampleRate();
			int bps = channels*(samplesize>>3);
			if (bps <= 4)
			{
				int samples = bufsize*rate/8/mixerrate;
				int result = s->GetData(tmpbuf, samples);
				bool finished = false;
				if (result < samples)
				{
					// We didn't get as much data as we asked for.
					// See if we should loop the sound, and fill the
					// buffer if that's the case.
					int zerocount = 0;
					while (result < samples)
					{
						bool more = item->loops_left > 1;
						if (more)
							item->loops_left --;
						if (more || item->loops_left == 0)
						{
							s->Rewind();
							int additional = s->GetData(tmpbuf+result*samplesize_bytes, samples - result);
							if (additional == 0)
							{
								if (zerocount++ > 2)
									// We don't seem to get more data now.
									// Will try again later.
									break;
							}
							else if (additional == -1)
							{
								// An error occured. Finish this sound.
								finished = true;
								break;
							}
							result += additional;
						}
						else
						{
							finished = true;
							break;
						}
					}
				}
				if (result > 0)
				{
					if (samplesize_bytes == 2)
					{
						AudioStream::Endianness e = s->GetByteOrder();
						if (e != AudioStream::IGNORE_ENDIAN &&
							e != endianness)
						{
							int total_bytes = result * bps;
							for (int i=0; i<total_bytes; i+=2)
							{
								*(unsigned short *)(tmpbuf+i) = (tmpbuf[i] | (tmpbuf[i+1] << 8));
							}
						}
					}

					int src_i = 0;
					for (int i=0; i<result; i++, src_i+=bps)
					{
						int dest_i = i*mixerrate/rate*2;
						int dest_numsamples = (i+1)*mixerrate/rate - dest_i / 2;
						for (int j=0; j<channels; j++)
						{
							int sample = ConvertSample(tmpbuf+src_i+j*samplesize_bytes, samplesize);
							for (int k=0; k<dest_numsamples; k++)
								sample_sum[dest_i+k*2+j] += sample;
							if (channels == 1)
							{
								for (int k=0; k<dest_numsamples; k++)
									sample_sum[dest_i+k*2+1] += sample;
							}
							if (i+1 < result && mixerrate > rate)
							{
								// Interpolate
								int nextsample = ConvertSample(tmpbuf+src_i+bps+j*samplesize_bytes, samplesize);
								int diff = nextsample - sample;
								for (int k=1; k<dest_numsamples; k++)
								{
									int add = k * diff / dest_numsamples;
									sample_sum[dest_i+k*2] += add;
									if (channels == 1)
										sample_sum[dest_i+k*2+1] += add;
								}
							}
						}
					}
				}
				if (finished)
				{
					// Finished with this sound
					item->dead = true;
				}
			}
		}
		if (active_count == 0)
		{
			// No sounds were mixed
			Unlock();
			return 0;
		}
		if (fade)
		{
			fade_counter += (double)(bufsize / 8);
			
			while (fade_counter >= fade_samples_per_step)
			{
				if (fade_destination > volume)
					volume ++;
				else
					volume --;
				if (fade_destination == volume)
				{
					fade = false;
					break;
				}
				fade_counter -= fade_samples_per_step;
			}
		}
		int volume = this->volume;
		int output_level = this->output_level;
		Unlock();
		int ii = bufsize/4;
		for (int i=0; i<ii; i+=2)
		{
			int ch1 = AdjustOutputLevel(sample_sum[i], output_level);
			int ch2 = AdjustOutputLevel(sample_sum[i+1], output_level);
			ch1 = ch1 * volume / 1000;
			ch2 = ch2 * volume / 1000;
			*(short *)(data+i*2) = ch1;
			*(short *)(data+i*2+2) = ch2;
		}
		Lock();
		this->output_level = output_level;
		Unlock();
		return bufsize / 2;
	}
	else
	{
		// No sounds to mix.
		return 0;
	}
}

