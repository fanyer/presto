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

#ifndef _SPDY_STREAMHANDLER_H_
#define _SPDY_STREAMHANDLER_H_

#include "modules/url/protocols/spdy/spdy_internal/spdy_frameheaders.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_dataconsumer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_framesproducer.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_version.h"

class StreamMaster;
class SpdyCompressionProvider;
class SpdyStreamController;
class SpdySettingsController;
struct SpdyHeaders;

/**
 * SpdyStreamHandler is responsible for handling all stream releated frames, sending and receiving headers and data. 
 * Co-operates closely with SpdyStreamController, which provides it with all the required data and is fed with data loaded on the stream.
 */
class SpdyStreamHandler: public SpdyDataConsumer, public SpdyFramesProducer
{
private:
	enum StreamState
	{
		SS_IDLE,
		SS_CREATING, 
		SS_PROCESSING_HEADERS,
		SS_PROCESSING_DATA_FRAME,
		SS_SENDING_DATA,
		SS_SENDING_WINDOW_UPDATE,
		SS_RESETING,
		SS_CLOSED,
	} streamState;

	const UINT8 priority;
	const UINT32 streamId;
	SpdyCompressionProvider *compressionProvider;
	SpdySettingsController *settingsController;
	SpdyStreamController *streamController;
	StreamMaster *master;
	UINT32 leftToRead;
	BOOL halfClosedFromClient;
	BOOL halfClosedFromServer;
	BOOL receivedSynReply;
	BOOL requestedReset;
	BOOL secureMode;
	SpdyVersion version;
	OpData headersBuffer;
	UINT32 serversWindowSize;
	UINT32 clientsWindowDelta;
	BOOL flowControlEnabled;
	UINT32 pendingResetStatus;

private:
	enum HeadersSubsets
	{
		HS_Cookie,
		HS_WhiteListed,
		HS_Other
	};

	HeadersSubsets FindSubsetForHeader(const char *headerName, size_t nameLength) const;
	void AddLengthInNetworkOrder(char *&target, UINT32 length, BOOL simulateOnly) const;
	UINT32 ReadLengthFromNetworkOrder(const char *&source) const;
	void ProcessDecompressedHeadersChunkL(const char *headersChunk, size_t headersChunkSize, BOOL last);
	UINT32 GetHeadersBlock(const SpdyHeaders *headers, char *target, BOOL simulateOnly) const;
	void AddCompressedHeadersL(OpData &target, const SpdyHeaders *headers) const;
	void AddSecurelyCompressedHeadersL(OpData &target, const SpdyHeaders *headers) const;
	void AddCompressedDataL(OpData &target, const char *data, size_t len, int clas, BOOL syncFlush) const;
	UINT32 ProcessHeadersChunkL(const OpData &data);
	UINT32 ProcessDataChunkL(const OpData &data);
	void WriteSynStreamFrameL(SpdyNetworkBuffer &frameData);
	void WriteResetStreamFrameL(SpdyNetworkBuffer &frameData);
	void WriteDataFrameL(SpdyNetworkBuffer &frameData);
	void WriteWindowUpdateFrameL(SpdyNetworkBuffer &frameData);
	void MoveToNextState();

public:
	SpdyStreamHandler(UINT32 streamId, SpdyVersion version, SpdyStreamController *streamController, SpdyCompressionProvider *compressionProvider, 
		UINT32 initialWindowSize, BOOL secureMode, StreamMaster *master);

	void SynReplyFrameLoaded(const SynReplyFrameHeader *frameHeader);
	void DataFrameLoaded(const DataFrameHeader *frameHeader);
	void ResetFrameLoaded(const ResetStreamFrameHeader *frameHeader);
	void HeadersFrameLoaded(const HeadersFrameHeader *frameHeader);
	void WindowUpdateFrameLoaded(const WindowUpdateFrameHeader *frameHeader);
	void Goaway();
	BOOL AcceptsNewFrames() const;
	BOOL ReceivedSynReply() const { return receivedSynReply; }
	UINT32 GetStreamId() const { return streamId; }
	void Reset(UINT32 status);

	virtual BOOL HasNextFrame() const;
	virtual void WriteNextFrameL(SpdyNetworkBuffer &target);
	virtual UINT8 GetPriority() const { return priority; }
	virtual BOOL ConsumeDataL(OpData &data);
};

/**
 * An interface for class that manages the stream handlers.
 * SpdyStreamHandler uses it when it wants to destroy itself or announce that has something to send.
 */
class StreamMaster
{
public:
	virtual void NukeStream(SpdyStreamHandler *streamToDelete) = 0;
	virtual void OnHasNewFrame(SpdyStreamHandler *stream) = 0;
};

#endif // _SPDY_STREAMHANDLER_H_
