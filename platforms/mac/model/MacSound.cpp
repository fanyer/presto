/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifndef NO_CARBON

#include "platforms/mac/model/MacSound.h"
#include "platforms/mac/File/FileUtils_Mac.h"

Boolean MacOpSound::isQTinitialized = false;
CGrafPtr MacOpSound::suppressVideoOffscreen = 0;
Head MacOpSound::theSounds;

/**
* This is the "constructor" method. Here we make sure we don't create an excessive number of sound instances.
* Deletion of the sounds is taken care of when the static Terminate method is called (when Opera terminates).
*/
MacOpSound* MacOpSound::GetSound(const uni_char* fullpath)
{
	FSSpec theSoundFSSpec;
	FSRef theSoundFSRef;
	Boolean fileIsValid = false;
	if(OpFileUtils::ConvertUniPathToFSRef(fullpath, theSoundFSRef))
	{
		if(noErr == FSGetCatalogInfo(&theSoundFSRef, kFSCatInfoNone, NULL, NULL, &theSoundFSSpec, NULL))
		{
			fileIsValid = true;
		}
	}

	if(fileIsValid)
	{
		MacOpSound *s = (MacOpSound*)theSounds.First();
		while(s)
		{
			//if(memcmp(&s->m_SoundFile, &theSoundFSSpec, sizeof(FSSpec)) == 0)
			if(	(s->m_SoundFile.vRefNum == theSoundFSSpec.vRefNum) &&
				(s->m_SoundFile.parID == theSoundFSSpec.parID) &&
				(s->m_SoundFile.name[0] == theSoundFSSpec.name[0]) &&
				(strncmp((char*)&s->m_SoundFile.name[1], (char*)&theSoundFSSpec.name[1], theSoundFSSpec.name[0]) == 0)
				)
			{
				return s;
			}

			s = (MacOpSound*)s->Suc();
		}
		return(new MacOpSound(&theSoundFSSpec));
	}

	return NULL;
}

/**
* Private constructor
* This is because we don't want to have multiple instances of MacOpSound that point to the same file.
*/
MacOpSound::MacOpSound(FSSpec *fss)
	: Link()
{
	m_UsedChannel = 0;
	m_StopPlaying = false;
	memcpy(&m_SoundFile, fss, sizeof(FSSpec));

	Into(&theSounds);

	MPCreateSemaphore(1, 1, &m_PlayingStoppedSignalID);	// only do this once

#ifdef _ED_DEBUG
	char cstr[256];
	My_p2c(cstr, m_SoundFile.name);
	fprintf(stderr, "Adding %s to list.\n", cstr);
#endif
}

MacOpSound::~MacOpSound()
{
	// make sure the play thread finishes so we don't leak memory
	if(isRunning)
	{
		m_StopPlaying = true;
		MPWaitOnSemaphore(m_PlayingStoppedSignalID, kDurationForever);
	}

	MPDeleteSemaphore(m_PlayingStoppedSignalID);

	// don't take this sound out of the list before it's done playing
	// or else we'll just hang in an infinite loop.
	Out();

#ifdef _ED_DEBUG
	char cstr[256];
	My_p2c(cstr, m_SoundFile.name);
	fprintf(stderr, "Removed %s from list.\n", cstr);
#endif
}

/**
* Initialize QuickTime and create an offscreen that we use to draw video in.
*/
OP_STATUS MacOpSound::Init()
{
	OSErr err = -1;

	if(!isQTinitialized)
	{
		err = EnterMovies();
		if(err == noErr)
			isQTinitialized = true;
	}

	if(!suppressVideoOffscreen)
	{
		Rect minimalBounds = {0,0,1,1};
		err = NewGWorld(&suppressVideoOffscreen, 32, &minimalBounds, NULL, NULL, useTempMem);
		if(err != noErr)
			suppressVideoOffscreen = 0;
	}

	if(suppressVideoOffscreen && isQTinitialized)
		err = noErr;

	return( err == noErr ? OpStatus::OK : OpStatus::ERR);
}

/**
* Shut down QuickTime and return some memory.
*/
void MacOpSound::Terminate()
{
	if(isQTinitialized)
	{
		ExitMovies();
		isQTinitialized = false;
	}
	if(suppressVideoOffscreen)
	{
		DisposeGWorld(suppressVideoOffscreen);
		suppressVideoOffscreen = 0;
	}

	MacOpSound *s = (MacOpSound*)theSounds.First();
	MacOpSound *next;
	while(s)
	{
#ifdef _ED_DEBUG
		fprintf(stderr, "MacOpSound: Deleting %p.\n", s);
#endif
		next = (MacOpSound*)s->Suc();
		delete s;

		s = next;
	}

}

/**
* This is for playing sounds asynchronously.
*/
OSStatus MacOpSound::run()
{
#define CleanupIfErr(x)		{if(x != noErr) goto cleanup;}
#define CleanupIfNull(x)	{if(!x) { err = -1; goto cleanup; }}

	OSErr 					err = -1;
	short 					theResID = 0;
	short 					theRefNum;
	Movie					theSoundMovie = NULL;
	Boolean					wasChanged;
	Track 					theSoundTrack;
	Media					theSoundMedia;
	SoundDescriptionHandle 	hSoundDescription = 0;
	Handle 					extension = 0;
	long					size;
	AudioCompressionAtomPtr audioAtom = 0;
	CmpSoundHeader 			soundInfo = {0};
	SoundComponentData		theInputFormat;
	SoundComponentData		theOutputFormat;
	SCFillBufferData 		scFillBufferData = {0};
	SoundConverter			mySoundConverter = NULL;
	ScheduledSoundHeader	mySndHeader0;
	ScheduledSoundHeader	mySndHeader1;
	Ptr						pDecomBuffer0 = NULL;
	Ptr						pDecomBuffer1 = NULL;
	Boolean					isSoundDone = false;

	if(isQTinitialized && suppressVideoOffscreen)
	{
		// We don't want video
		SetGWorld(suppressVideoOffscreen, GetGWorldDevice(suppressVideoOffscreen));

		err = OpenMovieFile(&m_SoundFile, &theRefNum, fsRdPerm);
		CleanupIfErr(err);

		err = NewMovieFromFile(&theSoundMovie, theRefNum, &theResID, NULL, newMovieActive, &wasChanged);
		CloseMovieFile(theRefNum);
		CleanupIfErr(err);

		SetMovieGWorld(theSoundMovie, suppressVideoOffscreen, GetGWorldDevice(suppressVideoOffscreen));

		theSoundTrack = GetMovieIndTrackType(theSoundMovie, 1, SoundMediaType, movieTrackMediaType);
		CleanupIfNull(theSoundTrack);

		theSoundMedia = GetTrackMedia(theSoundTrack);
		CleanupIfNull(theSoundMedia);

		hSoundDescription = (SoundDescriptionHandle)NewHandle(0);
		CleanupIfNull(hSoundDescription);
		GetMediaSampleDescription(theSoundMedia, 1, (SampleDescriptionHandle)hSoundDescription);
		err = GetMoviesError();
		CleanupIfErr(err);

		extension = NewHandle(0);
		CleanupIfNull(extension);
		err = GetSoundDescriptionExtension(hSoundDescription, &extension, siDecompressionParams);
		if(err == noErr)
		{
			size = GetHandleSize(extension);
			HLock(extension);
			audioAtom = (AudioCompressionAtomPtr)NewPtr(size);
			err = MemError();
			if(err != noErr)
			{
				HUnlock(extension);
				goto cleanup;
			}
			// copy the atom data to our buffer...
			BlockMoveData(*extension, audioAtom, size);
			HUnlock(extension);
		}
		// don't care if we got an error here...

		// set up our sound header
		soundInfo.format = (*hSoundDescription)->dataFormat;
		soundInfo.numChannels = (*hSoundDescription)->numChannels;
		soundInfo.sampleSize = (*hSoundDescription)->sampleSize;
		soundInfo.sampleRate = (*hSoundDescription)->sampleRate;
		soundInfo.compressionID = (*hSoundDescription)->compressionID;

		theInputFormat.flags = 0;
		theInputFormat.format = soundInfo.format;
		theInputFormat.numChannels = soundInfo.numChannels;
		theInputFormat.sampleSize = soundInfo.sampleSize;
		theInputFormat.sampleRate = soundInfo. sampleRate;
		theInputFormat.sampleCount = 0;
		theInputFormat.buffer = NULL;
		theInputFormat.reserved = 0;

		theOutputFormat.flags = 0;
		theOutputFormat.format = kSoundNotCompressed;
		theOutputFormat.numChannels = theInputFormat.numChannels;
		theOutputFormat.sampleSize = theInputFormat.sampleSize;
		theOutputFormat.sampleRate = theInputFormat.sampleRate;
		theOutputFormat.sampleCount = 0;
		theOutputFormat.buffer = NULL;
		theOutputFormat.reserved = 0;

		// variableCompression means we're going to use the commonFrameSize field and the kExtendedSoundCommonFrameSizeValid flag
		scFillBufferData.isSourceVBR = (soundInfo.compressionID == variableCompression);

		err = SoundConverterOpen(&theInputFormat, &theOutputFormat, &mySoundConverter);
		CleanupIfErr(err);

		SoundConverterSetInfo(mySoundConverter, siClientAcceptsVBR, Ptr(true));
		err = SoundConverterSetInfo(mySoundConverter, siDecompressionParams, audioAtom);
		if (audioAtom)
			DisposePtr((Ptr)audioAtom);
		if (siUnknownInfoType != err)	// this error is ok!
			CleanupIfErr(err);

		// the input buffer has to be large enough so GetMediaSample isn't going to fail, your mileage may vary
		UInt32 inputBytes = ((1000 + (theInputFormat.sampleRate >> 16)) * theInputFormat.numChannels) * 4;
		UInt32 outputBytes = 1024 * 64;	// 64 Kb should do it

		// allocate play buffers
		pDecomBuffer0 = NewPtr(outputBytes);
		CleanupIfErr(MemError());

		pDecomBuffer1 = NewPtr(outputBytes);
		CleanupIfErr(MemError());

		err = SoundConverterBeginConversion(mySoundConverter);
		CleanupIfErr(err);

		// setup first header
		mySndHeader0.u.cmpHeader.samplePtr = pDecomBuffer0;
		mySndHeader0.u.cmpHeader.numChannels = theOutputFormat.numChannels;
		mySndHeader0.u.cmpHeader.sampleRate = theOutputFormat.sampleRate;
		mySndHeader0.u.cmpHeader.loopStart = 0;
		mySndHeader0.u.cmpHeader.loopEnd = 0;
		mySndHeader0.u.cmpHeader.encode = cmpSH;					// compressed sound header encode value
		mySndHeader0.u.cmpHeader.baseFrequency = kMiddleC;
		mySndHeader0.u.cmpHeader.numFrames = 0;						// numFrames will equal outputFrames as we're converting
		// mySndHeader0.u.cmpHeader.AIFFSampleRate;					// this is not used
		mySndHeader0.u.cmpHeader.markerChunk = NULL;
		mySndHeader0.u.cmpHeader.format = theOutputFormat.format;
		mySndHeader0.u.cmpHeader.futureUse2 = 0;
		mySndHeader0.u.cmpHeader.stateVars = NULL;
		mySndHeader0.u.cmpHeader.leftOverSamples = NULL;
		mySndHeader0.u.cmpHeader.compressionID = fixedCompression;	// compression ID for fixed-sized compression, even uncompressed sounds use fixedCompression
		mySndHeader0.u.cmpHeader.packetSize = 0;					// the Sound Manager will figure this out for us
		mySndHeader0.u.cmpHeader.snthID = 0;
		mySndHeader0.u.cmpHeader.sampleSize = theOutputFormat.sampleSize;
		mySndHeader0.u.cmpHeader.sampleArea[0] = 0;					// no samples here because we use samplePtr to point to our buffer instead
		mySndHeader0.flags = kScheduledSoundDoCallBack;
		mySndHeader0.callBackParam1 = 0;							// we don't pass anything to the callback routine
		mySndHeader0.callBackParam2 = 0;

		// setup second header, only the buffer ptr is different
		BlockMoveData(&mySndHeader0, &mySndHeader1, sizeof(mySndHeader0));
		mySndHeader1.u.cmpHeader.samplePtr = pDecomBuffer1;

		// fill in struct that gets passed to SoundConverterFillBufferDataProc via the refcon
		// this includes the ExtendedSoundComponentData record
		scFillBufferData.sourceMedia = theSoundMedia;
		scFillBufferData.getMediaAtThisTime = 0;
		scFillBufferData.sourceDuration = GetMediaDuration(theSoundMedia);
		scFillBufferData.isThereMoreSource = true;
		scFillBufferData.maxBufferSize = inputBytes;
		scFillBufferData.hSource = NewHandle((long)scFillBufferData.maxBufferSize);	// allocate source media buffer
		CleanupIfErr(MemError());
		MoveHHi((Handle)scFillBufferData.hSource);
		HLock((Handle)scFillBufferData.hSource);

		scFillBufferData.compData.desc = theInputFormat;
		scFillBufferData.compData.desc.buffer = (BytePtr)*scFillBufferData.hSource;
		scFillBufferData.compData.desc.flags = kExtendedSoundData;
		scFillBufferData.compData.recordSize = sizeof(ExtendedSoundComponentData);
		scFillBufferData.compData.extendedFlags = kExtendedSoundSampleCountNotValid | kExtendedSoundBufferSizeValid;

		if (scFillBufferData.isSourceVBR)
			scFillBufferData.compData.extendedFlags |= kExtendedSoundCommonFrameSizeValid;

		scFillBufferData.compData.bufferSize = 0;	// filled in during FillBuffer callback

		// create a semaphore used to sync the filling of the play buffers, setup the callback,
		// create the sound channel and play the sound -- we will continue to convert the sound data
		// into a free (non playing) buffer --  because we want to fill up both buffers immediately
		// without waiting for the first buffer to complete, a binary semaphore is created with the maximumValue
		// and initialValue set to 1, when we call MPWaitOnSemaphore if the value of the semaphore is greater
		// than zero, the value is decremented and the function returns immediately which is what we want the
		// very first time through - after that, MPWaitOnSemaphore will block waiting for our callback
		// to fire where we signal the semaphore, this indicates a free buffer and the process can continue
		MPCreateBinarySemaphore(&m_BufferDoneSignalID);
		SndCallBackUPP theSoundCallBackUPP = NewSndCallBackUPP(OpSoundCallBackFunction);
		err = SndNewChannel(&m_UsedChannel, sampledSynth, 0, theSoundCallBackUPP);

		if (err == noErr) {
			SndCommand		  thePlayCmd0,
							  thePlayCmd1;
			SndCommand	 	  *pPlayCmd;
			Ptr 			  pDecomBuffer = NULL;
			CmpSoundHeaderPtr pSndHeader = NULL;

			UInt32 			  outputFrames,
							  actualOutputBytes,
							  outputFlags;
			eBufferNumber     whichBuffer = kFirstBuffer;

			SoundConverterFillBufferDataUPP theFillBufferDataUPP = NewSoundConverterFillBufferDataUPP(SoundConverterFillBufferDataProc);

			thePlayCmd0.cmd = scheduledSoundCmd;
			thePlayCmd0.param1 = 0;						// not used, but clear it out anyway just to be safe
			thePlayCmd0.param2 = (long)&mySndHeader0;

			thePlayCmd1.cmd = scheduledSoundCmd;
			thePlayCmd1.param1 = 0;						// not used, but clear it out anyway just to be safe
			thePlayCmd1.param2 = (long)&mySndHeader1;

			if (noErr == err) {
				while (!isSoundDone && !m_StopPlaying) {
					if (kFirstBuffer == whichBuffer) {
						pPlayCmd = &thePlayCmd0;
						pDecomBuffer = pDecomBuffer0;
						pSndHeader = &mySndHeader0.u.cmpHeader;
						whichBuffer = kSecondBuffer;
					} else {
						pPlayCmd = &thePlayCmd1;
						pDecomBuffer = pDecomBuffer1;
						pSndHeader = &mySndHeader1.u.cmpHeader;
						whichBuffer = kFirstBuffer;
					}

					err = SoundConverterFillBuffer(mySoundConverter,		// a sound converter
												   theFillBufferDataUPP,	// the callback UPP
												   &scFillBufferData,		// refCon passed to FillDataProc
												   pDecomBuffer,			// the decompressed data 'play' buffer
												   outputBytes,				// size of the 'play' buffer
												   &actualOutputBytes,		// number of output bytes
												   &outputFrames,			// number of output frames
												   &outputFlags);			// fillbuffer retured advisor flags
					if (err) break;
					if((outputFlags & kSoundConverterHasLeftOverData) == false) {
						isSoundDone = true;
					}

					pSndHeader->numFrames = outputFrames;

					err = SndDoCommand(m_UsedChannel, pPlayCmd, true);				// play the next buffer

					if(err != noErr)
					{
						break;
					}

					err = MPWaitOnSemaphore(m_BufferDoneSignalID, kDurationForever);	// block waiting for a free buffer

					if(err != noErr)
					{
						break;
					}
				} // while
			}

			SoundConverterEndConversion(mySoundConverter, pDecomBuffer, &outputFrames, &outputBytes);

			if (noErr == err && outputFrames) {
				pSndHeader->numFrames = outputFrames;
				SndDoCommand(m_UsedChannel, pPlayCmd, true);	// play the last buffer.
			}

			if (theFillBufferDataUPP)
				DisposeSoundConverterFillBufferDataUPP(theFillBufferDataUPP);
		}

		if (m_UsedChannel)
		{
			err = SndDisposeChannel(m_UsedChannel, false);		// wait until sounds stops playing before disposing of channel
			m_UsedChannel = 0;
		}

		if (theSoundCallBackUPP)
			DisposeSndCallBackUPP(theSoundCallBackUPP);

		MPDeleteSemaphore(m_BufferDoneSignalID);
	}

cleanup:
	// cleanup
	if (hSoundDescription)
		DisposeHandle((Handle)hSoundDescription);
	if (extension)
		DisposeHandle(extension);
	if (scFillBufferData.hSource)
		DisposeHandle(scFillBufferData.hSource);
	if (mySoundConverter)
		SoundConverterClose(mySoundConverter);
	if (pDecomBuffer0)
		DisposePtr(pDecomBuffer0);
	if (pDecomBuffer1)
		DisposePtr(pDecomBuffer1);
	if (theSoundMovie)
		DisposeMovie(theSoundMovie);

	MPSignalSemaphore(m_PlayingStoppedSignalID);

	terminate();
	return err;
}

OP_STATUS MacOpSound::Play(BOOL async)
{
	OSErr err = -1;

	if(!async)
	{
		// play the sound synchronously

		// if this sound is already playing, then wait until it's done before repeating it
		m_StopPlaying = true;
		MPWaitOnSemaphore(m_PlayingStoppedSignalID, kDurationForever);
		m_StopPlaying = false;

		start();

		// it's possible that the sound finished in just one run, so don't wait unless we're still running
		if(isRunning)
		{
			MPWaitOnSemaphore(m_PlayingStoppedSignalID, kDurationForever);

			// finished playing, deleted all data, but the thread is still running
			// that's a problem when we're playing the exit sound since we'll delete all sounds right after the Play() call.
			terminate();	// ok, now it's dead and we won't be waiting forever in the destructor.
		}

		err = noErr;
#if 0
		// the following code left is here in case the other alternative turns out to be incompatible with 10.0.
		if(isQTinitialized && suppressVideoOffscreen)
		{
			short 					theResID = 0;
			short 					theRefNum;
			Movie					theSoundMovie;
			Boolean					wasChanged;

			// We don't want video
			SetGWorld(suppressVideoOffscreen, GetGWorldDevice(suppressVideoOffscreen));

			err = OpenMovieFile(&m_SoundFile, &theRefNum, fsRdPerm);
			CleanupIfErr(err);

			err = NewMovieFromFile(&theSoundMovie, theRefNum, &theResID, NULL, newMovieActive, &wasChanged);
			CloseMovieFile(theRefNum);
			CleanupIfErr(err);

			SetMovieGWorld(theSoundMovie, suppressVideoOffscreen, GetGWorldDevice(suppressVideoOffscreen));

			GoToBeginningOfMovie(theSoundMovie);
			StartMovie(theSoundMovie);

			while (!IsMovieDone(theSoundMovie))
			{
				MoviesTask(theSoundMovie, 0);
			}

cleanup:
			if (theSoundMovie)
				DisposeMovie(theSoundMovie);
		}
#endif
	}
	else
	{
		// play the sound asynchronously

		// if this sound is already playing, then wait until it's done before repeating it
		m_StopPlaying = true;
		MPWaitOnSemaphore(m_PlayingStoppedSignalID, kDurationForever);
		m_StopPlaying = false;

		start();
		err = noErr;
	}

	return( err == noErr ? OpStatus::OK : OpStatus::ERR );
}

OP_STATUS MacOpSound::Stop()
{
	return OpStatus::ERR;
}

MacOpSound* MacOpSound::FindSoundForChannel(SndChannelPtr theChannel)
{
	MacOpSound *s = (MacOpSound*)theSounds.First();

	while(s)
	{
		if(s->m_UsedChannel == theChannel)
			return s;

		s = (MacOpSound*)s->Suc();
	}

	return NULL;
}

pascal void MacOpSound::OpSoundCallBackFunction(SndChannelPtr theChannel, SndCommand *theCmd)
{
	MacOpSound *snd = FindSoundForChannel(theChannel);
	if(snd)
	{
		MPSignalSemaphore(snd->m_BufferDoneSignalID);
	}
}

pascal Boolean MacOpSound::SoundConverterFillBufferDataProc(SoundComponentDataPtr *outData, void *inRefCon)
{
	SCFillBufferDataPtr pFillData = (SCFillBufferDataPtr)inRefCon;

	OSErr err;

	// if after getting the last chunk of data the total time is over the duration, we're done
	if (pFillData->getMediaAtThisTime >= pFillData->sourceDuration) {
		pFillData->isThereMoreSource = false;
		pFillData->compData.desc.buffer = NULL;
		pFillData->compData.desc.sampleCount = 0;
		pFillData->compData.bufferSize = 0;
		pFillData->compData.commonFrameSize = 0;
	}

	if (pFillData->isThereMoreSource) {

		long	  sourceBytesReturned;
		long	  numberOfSamples;
		TimeValue sourceReturnedTime, durationPerSample;

		// in calling GetMediaSample, we'll get a buffer that consists of equal sized frames - the
		// degenerate case is only 1 frame -- for non-self-framed vbr formats (like AAC in QT 6.0)
		// we need to provide some more framing information - either the frameCount, frameSizeArray pair or
		// commonFrameSize field must be valid -- because we always get equal sized frames, we use
		// commonFrameSize and set the kExtendedSoundCommonFrameSizeValid flag -- if there is
		// only 1 frame then (common frame size == media sample size), if there are multiple frames,
		// then (common frame size == media sample size / number of frames).

		err = GetMediaSample(pFillData->sourceMedia,		// specifies the media for this operation
							 pFillData->hSource,			// function returns the sample data into this handle
							 pFillData->maxBufferSize,		// maximum number of bytes of sample data to be returned
							 &sourceBytesReturned,			// the number of bytes of sample data returned
							 pFillData->getMediaAtThisTime,	// starting time of the sample to be retrieved (must be in Media's TimeScale)
							 &sourceReturnedTime,			// indicates the actual time of the returned sample data
							 &durationPerSample,			// duration of each sample in the media
							 NULL,							// sample description corresponding to the returned sample data (NULL to ignore)
							 NULL,							// index value to the sample description that corresponds to the returned sample data (NULL to ignore)
							 0,								// maximum number of samples to be returned (0 to use a value that is appropriate for the media)
							 &numberOfSamples,				// number of samples it actually returned
							 NULL);							// flags that describe the sample (NULL to ignore)

		if ((noErr != err) || (sourceBytesReturned == 0)) {
			pFillData->isThereMoreSource = false;
			pFillData->compData.desc.buffer = NULL;
			pFillData->compData.desc.sampleCount = 0;
			pFillData->compData.bufferSize = 0;
			pFillData->compData.commonFrameSize = 0;

			//if ((err != noErr) && (sourceBytesReturned > 0))
			//	DebugStr("\pGetMediaSample - Failed in FillBufferDataProc");
		}

		pFillData->getMediaAtThisTime = sourceReturnedTime + (durationPerSample * numberOfSamples);

		// we've specified kExtendedSoundSampleCountNotValid and the 'studly' Sound Converter will
		// take care of sampleCount for us, so while this is not required we fill out all the information
		// we have to simply demonstrate how this would be done
		// sampleCount is the number of PCM samples
		pFillData->compData.desc.sampleCount = numberOfSamples * durationPerSample;

		// kExtendedSoundBufferSizeValid was specified - make sure this field is filled in correctly
		pFillData->compData.bufferSize = sourceBytesReturned;

		// for VBR audio we specified the kExtendedSoundCommonFrameSizeValid flag - make sure this field is filled in correctly
		if (pFillData->isSourceVBR)
			pFillData->compData.commonFrameSize = sourceBytesReturned / numberOfSamples;
	}

	// set outData to a properly filled out ExtendedSoundComponentData struct
	*outData = (SoundComponentDataPtr)&pFillData->compData;

	return (pFillData->isThereMoreSource);
}
#endif // NO_CARBON

