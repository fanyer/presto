/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/


#ifndef _SPDY_PROTOCOL_H_
#define _SPDY_PROTOCOL_H_

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_settingscontroller.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_pingcontroller.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_streamreseter.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_dataconsumer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_streamhandler.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_version.h"
#include "modules/hardcore/timer/optimer.h"

class SynReplyFrameHeader;
class DataFrameHeader;
class SettingsFrameHeader;
class SpdyCompressionProvider;
class SpdyFramesProducer;
class SpdyFramesHandlerListener;
struct SpdyHeaders;

#ifdef SELFTEST
class SpdyProtocolSelftestsListener;
#endif // SELFTEST

/**
 * Stream controler is the listener and the data provider in one. 
 * It's responsible for providing all informations needed for creating the stream and is fed back with the data loaded from the stream.
 */
class SpdyStreamController
{
public:
	virtual ~SpdyStreamController() {}
	/**
	 * Provides the priority of the stream.
	 * The higher number - the higher priority, so the most important requests should have priority 255, the least important - 0.
	 * @return the priority of the stream
	 */
	virtual UINT8 GetPriority() const = 0;

	/**
	 * Returns headers that should be sent on the stream. Caller of this method takes responsibility for destroying the returned structure.
	 * @param version version of the protocol that the headers should be prepared for
	 */
	virtual SpdyHeaders *GetHeadersL(SpdyVersion version) const = 0;
	
	/**
	 * Should return TRUE if the controller wished to send some data over the stream or FALSE in other case
	 */
	virtual BOOL HasDataToSend() const = 0;
	
	/**
	 * Should fill the buffer with next chunk of data that should be sent on the stream.
	 * @param buffer buffer to be filled
	 * @param bufferLength the length of the buffer
	 * @param more is set to TRUE if there is more data to send or FALSE if this was the last chunk of data
	 * @return the number of bytes which the buffer has been filled with
	 */
	virtual size_t GetNextDataChunkL(char *buffer, size_t bufferLength, BOOL &more) = 0;

	/**
	 * Provides the controller with headers that have been received on the stream.
	 */
	virtual void OnHeadersLoadedL(const SpdyHeaders &headers, SpdyVersion version) = 0;
	
	/**
	 * Provides the controller with another chunk of data received on the stream.
	 */
	virtual void OnDataLoadedL(const OpData &data) = 0;
	virtual void OnDataLoadingFinished() = 0;
	virtual void OnResetStream() = 0;
	virtual void OnGoAway() = 0;
};


/** 
 * SpdyFramesHandler is responsible for the SPDY framing layer. It's parsing the incoming data in search of specific headers,
 * feeding the data consumers with data that are destined for them, geting new frames from frame producers and sending them on the wire (SComm).
 */
class SpdyFramesHandler: private StreamMaster, private OpTimerListener
{
private:
	SpdyCompressionProvider *compressionProvider;
	OpINT32HashTable<SpdyStreamHandler> streamHandlers;
	List<SpdyFramesProducer> prioritizedFramesProducers;
	List<SpdyFramesProducer> priorityOrderedFramesProducers;
	Head streamsToDestroy;
	SpdySettingsController settingsController;
	SpdyPingController pingController;
	SpdyDevNullConsumer nullConsumer;
	SpdyDecompressingConsumer decompressingConsumer;
	SpdyStreamReseter streamReseter;
	SpdyDataConsumer *activeDataConsumer;
	OpData buffer;
	UINT32 nextStreamId;
	OpTimer removeStreamsTimer;
	BOOL removeStreamsTimerSet;
	BOOL noMoreStreams;
	BOOL pendingDataToSend;
	SpdyFramesHandlerListener *listener;
#ifdef SELFTEST
	SpdyProtocolSelftestsListener *selftestListener;
#endif // SELFTEST
	const SpdyVersion version;
	BOOL secureMode;
	size_t maxBufLen;

private:
	SpdyStreamHandler *FindStream(UINT32 streamId);
	BOOL TryToParseHeader();
	void AddToProducersList(SpdyFramesProducer *producer);
	virtual void NukeStream(SpdyStreamHandler *streamToDelete);
	virtual void OnHasNewFrame(SpdyStreamHandler *stream);
	virtual void OnTimeOut(OpTimer* timer);
	void CancelStreamsWithHigherId(UINT32 lastGoodId);
	void ResetStream(UINT32 streamId, UINT32 resetStatus);

public:
	SpdyFramesHandler(SpdyVersion ver, ServerName_Pointer serverName, unsigned short port, BOOL privacyMode, BOOL secureMode, SpdyFramesHandlerListener *l, BOOL serverMode = FALSE);
	~SpdyFramesHandler();
	void ConstructL();

	/**
	 * Creates new stream and returns it's ID for further reference.
	 * @param streamController controller of the stream (see SpdyStreamController class).
	 *	FramesHandler does not take over the responsibility for freeing the streamController object
	 * return id for created stream that may be used for further reference (e.g. removing the stream)
	 */
	UINT32 CreateStreamL(SpdyStreamController *streamController);
	BOOL CanCreateNewStream();
	void RemoveStream(UINT32 streamId);

	/**
	 * Returns TRUE if sent something down to the sink or FALSE in other case.
	 */
	BOOL SendDataL(SComm* sink);
	void ProcessReceivedDataL(SComm* sink);

#ifdef SELFTEST
	void SetSelftestListener(SpdyProtocolSelftestsListener *listener) { selftestListener = listener; }
#endif // SELFTEST
};

/**
 * Interface for sending notifications about various protocol events
 */
class SpdyFramesHandlerListener
{
public:
	/**
	 * Notifies the listener that has some data to send, so SendDataL() should be called as soon as possible.
	 */
	virtual void OnHasDataToSend() = 0;

	/**
	 *  Notifies the listener that server didn't respond to PING frame in time determined by refsCollectionNetwork::SpdyPingTimeout
	 */
	virtual void OnPingTimeout() = 0;

	/**
	 *  Notifies the listener that server sent some data dat couldn't be understood by SPDY protocol
	 */
	virtual void OnProtocolError() = 0;

	/**
	 *  Notifies the listener that server sent GOAWAY frame, so some stream can be cancelled and no more streams should be created on that connection.
	 */
	virtual void OnGoAway() = 0;
};

#ifdef SELFTEST
/**
 * An interace for listener used by selftests for testing low level protocol events.
 * Calling each of it's methods notifies the listener about receiving a frame of some kind. 
 * //The medthods should return TRUE when further processing of given frame should be continued, or FALSE when SpdyFramesHandler should ignore the frame.
 */
class SpdyProtocolSelftestsListener
{
public:
	virtual void OnReceivedReset(UINT32 streamId, UINT32 resetStatus) = 0;
	virtual void OnReceivedSynStream(UINT32 streamId) = 0;
	virtual void OnReceivedSynReply(UINT32 streamId) = 0;
	virtual void OnReceivedGoAway() = 0;
};
#endif // SELFTEST

#endif // _SPDY_PROTOCOL_H_
