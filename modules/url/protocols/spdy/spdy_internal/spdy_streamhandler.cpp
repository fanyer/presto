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


#include "core/pch.h"

#ifdef USE_SPDY

#include "modules/url/protocols/spdy/spdy_internal/spdy_common.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_streamhandler.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_hdrcompr.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_settingscontroller.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_headers.h"
#include "modules/url/protocols/spdy/spdy_protocol.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"

#define DATA_FRAME_MAX_LENGTH 0x1000
#define WINDOW_MIN_SIZE 0x100

SpdyStreamHandler::SpdyStreamHandler(UINT32 streamId, SpdyVersion version, SpdyStreamController *streamController, SpdyCompressionProvider *compressionProvider, 
		UINT32 initialWindowSize, BOOL secureMode, StreamMaster *master): 
	streamState(SS_CREATING),
	priority(streamController->GetPriority()),
	streamId(streamId),
	compressionProvider(compressionProvider),
	streamController(streamController),
	master(master),
	leftToRead(0),
	halfClosedFromClient(FALSE),
	halfClosedFromServer(FALSE),
	receivedSynReply(FALSE),
	requestedReset(FALSE),
	secureMode(secureMode),
	version(version),
	serversWindowSize(initialWindowSize),
	flowControlEnabled(FALSE),
	pendingResetStatus(0)
{
	OP_ASSERT(version == SPDY_V2 || version == SPDY_V3);
	if (version == SPDY_V3)
	{
		flowControlEnabled = TRUE;
		clientsWindowDelta = 0;
	}
}

BOOL SpdyStreamHandler::AcceptsNewFrames() const
{
	return streamState != SS_RESETING && streamState != SS_CLOSED && !requestedReset;
}

void SpdyStreamHandler::SynReplyFrameLoaded(const SynReplyFrameHeader *frameHeader)
{
	OP_ASSERT(AcceptsNewFrames());
	halfClosedFromServer |= frameHeader->GetFlags() & SCF_CONTROL_FLAG_FIN;

	leftToRead = frameHeader->GetHeadersBlockSize(version);
	streamState = SS_PROCESSING_HEADERS;
	receivedSynReply = TRUE;
	MoveToNextState();
}

void SpdyStreamHandler::HeadersFrameLoaded(const HeadersFrameHeader *frameHeader)
{
	OP_ASSERT(AcceptsNewFrames());
	halfClosedFromServer |= frameHeader->GetFlags() & SCF_CONTROL_FLAG_FIN;

	leftToRead = frameHeader->GetHeadersBlockSize(version);
	streamState = SS_PROCESSING_HEADERS;
	MoveToNextState();
}

void SpdyStreamHandler::DataFrameLoaded(const DataFrameHeader *frameHeader)
{
	OP_ASSERT(AcceptsNewFrames());
	if (frameHeader->GetFlags() & SCF_CONTROL_FLAG_FIN)
		halfClosedFromServer = TRUE;

	streamState = SS_PROCESSING_DATA_FRAME;
	leftToRead = frameHeader->GetLength();
	MoveToNextState();
}

void SpdyStreamHandler::ResetFrameLoaded(const ResetStreamFrameHeader *frameHeader)
{
	OP_ASSERT(AcceptsNewFrames());
	streamController->OnResetStream();
	streamState = SS_CLOSED;
	master->NukeStream(this);
}

void SpdyStreamHandler::WindowUpdateFrameLoaded(const WindowUpdateFrameHeader *frameHeader)
{
	serversWindowSize += frameHeader->GetDeltaWindowSize();
	if (!halfClosedFromClient)
		master->OnHasNewFrame(this);
}

void SpdyStreamHandler::MoveToNextState()
{
	switch (streamState)
	{
	case SS_PROCESSING_HEADERS:
		if (leftToRead == 0)
		{
			streamState = SS_IDLE;
			if (halfClosedFromServer && streamController)
				streamController->OnDataLoadingFinished();
		}
		break;
	case SS_PROCESSING_DATA_FRAME:
		if (leftToRead == 0)
		{
			if (halfClosedFromServer && streamController)
			{
				streamController->OnDataLoadingFinished();
			}
			if (flowControlEnabled && !halfClosedFromServer)
			{
				streamState = SS_SENDING_WINDOW_UPDATE;
				master->OnHasNewFrame(this);
			}
			else
				streamState = SS_IDLE;
		}
		break;
	case SS_SENDING_WINDOW_UPDATE:
		streamState = SS_IDLE;
		break;
	case SS_RESETING:
		streamState = SS_CLOSED;
		master->NukeStream(this);
		break;
	case SS_CREATING:
		if (halfClosedFromClient)
			streamState = SS_IDLE;
		else
		{
			streamState = SS_SENDING_DATA;
			master->OnHasNewFrame(this);
		}
		break;
	case SS_SENDING_DATA:
		if (halfClosedFromClient)
			streamState = SS_IDLE;
		else
			master->OnHasNewFrame(this);
		break;
	}

	if (streamState == SS_IDLE)
	{
		if (halfClosedFromClient && halfClosedFromServer)
		{
			streamState = SS_CLOSED;
			master->NukeStream(this);
		}
		else if (requestedReset)
		{
			streamState = SS_RESETING;
			master->OnHasNewFrame(this);
		}
	}
}

UINT32 SpdyStreamHandler::ReadLengthFromNetworkOrder(const char *&source) const
{
	if (version == SPDY_V2)
	{
		UINT16 len;
		op_memcpy(&len, source, sizeof(UINT16));
		source += sizeof(UINT16);
		return op_ntohs(len);
	}
	else
	{
		UINT32 len;
		op_memcpy(&len, source, sizeof(UINT32));
		source += sizeof(UINT32);
		return op_ntohl(len);
	}
}

void SpdyStreamHandler::ProcessDecompressedHeadersChunkL(const char *headersChunk, size_t headersChunkSize, BOOL last)
{
	if (!last)
	{
		SPDY_LEAVE_IF_ERROR(headersBuffer.AppendCopyData(headersChunk, headersChunkSize));
		return;
	}

	if (!headersBuffer.IsEmpty())
	{
		SPDY_LEAVE_IF_ERROR(headersBuffer.AppendConstData(headersChunk, headersChunkSize));
		SPDY_LEAVE_IF_NULL(headersChunk = headersBuffer.Data());
		headersChunkSize = headersBuffer.Length();
	}

	const char *headersBlockIt = headersChunk, *headersBlockEnd = headersChunk + headersChunkSize;
	ANCHORD(SpdyHeaders, headers);
	UINT32 headersCount = ReadLengthFromNetworkOrder(headersBlockIt);
	headers.InitL(headersCount);
	
	for (UINT32 i = 0; i < headersCount; ++i)
	{
		UINT32 nameLength = ReadLengthFromNetworkOrder(headersBlockIt);
		headers.headersArray[i].name.Init(headersBlockIt, TRUE, nameLength);
		headersBlockIt += nameLength;

		UINT32 valueLength = ReadLengthFromNetworkOrder(headersBlockIt);
		headers.headersArray[i].value.Init(headersBlockIt, TRUE, valueLength);
		headersBlockIt += valueLength;

		if (headersBlockIt > headersBlockEnd)
			SPDY_LEAVE(OpStatus::ERR);
	}

	streamController->OnHeadersLoadedL(headers, version);
	headersBuffer.Clear();
}

UINT32 SpdyStreamHandler::ProcessHeadersChunkL(const OpData &data)
{
	size_t readData = 0, dataToRead = MIN(leftToRead, data.Length());

	char *compressedChunkBuff = reinterpret_cast<char*>(g_memory_manager->GetTempBuf());
	size_t compressedChunkBuffSize = g_memory_manager->GetTempBufLen();
	z_stream &decompressor = compressionProvider->GetDecompressionStream();

	while (readData<dataToRead)
	{
		size_t compressedChunkLength = data.CopyInto(compressedChunkBuff, MIN(dataToRead-readData, compressedChunkBuffSize), readData);
		readData += compressedChunkLength;

		decompressor.next_in = reinterpret_cast<Bytef*>(compressedChunkBuff);
		decompressor.avail_in = compressedChunkLength;

		while (decompressor.avail_in > 0)
		{
			char *outputBuffer = static_cast<char*>(g_memory_manager->GetTempBuf2k());
			size_t outputBufferSize = g_memory_manager->GetTempBuf2kLen();
			
			decompressor.next_out = reinterpret_cast<Bytef*>(outputBuffer);
			decompressor.avail_out = outputBufferSize;
			SPDY_LEAVE_IF_ERROR(compressionProvider->Decompress());
			UINT32 decompressedSize = outputBufferSize - decompressor.avail_out;
			if (streamController)
				ProcessDecompressedHeadersChunkL(outputBuffer, decompressedSize, decompressor.avail_in == 0 && readData == dataToRead && dataToRead == leftToRead);
		}
	}

	return readData;
}

UINT32 SpdyStreamHandler::ProcessDataChunkL(const OpData &data)
{
	UINT32 dataToRead = MIN(leftToRead, data.Length());
	OpData readData(data, 0, dataToRead);
	if (streamController)
		streamController->OnDataLoadedL(readData);

	if (flowControlEnabled)
		clientsWindowDelta += dataToRead;
	return dataToRead;
}

BOOL SpdyStreamHandler::ConsumeDataL(OpData &data)
{
	UINT32 readData = 0;
	switch (streamState)
	{
	case SS_PROCESSING_HEADERS:
		readData = ProcessHeadersChunkL(data);
		break;
	case SS_PROCESSING_DATA_FRAME:
		readData = ProcessDataChunkL(data);
		break;
	default:
		OP_ASSERT(!leftToRead || !"Not expecting any data in this state");
	}

	data.Consume(readData);
	leftToRead -= readData;
	MoveToNextState();
	return leftToRead != 0;
}

void SpdyStreamHandler::WriteNextFrameL(SpdyNetworkBuffer &target)
{
	switch (streamState)
	{
	case SS_CREATING:
		WriteSynStreamFrameL(target);
		break;
	case SS_RESETING:
		WriteResetStreamFrameL(target);
		break;
	case SS_SENDING_DATA:
		WriteDataFrameL(target);
		break;
	case SS_SENDING_WINDOW_UPDATE:
		WriteWindowUpdateFrameL(target);
		break;
	}
	
	MoveToNextState();
}

void SpdyStreamHandler::AddLengthInNetworkOrder(char *&target, UINT32 length, BOOL simulateOnly) const
{
	if (version == SPDY_V2)
	{
		if (!simulateOnly)
		{
			UINT16 headersCount = op_htons(length);
			op_memcpy(target, &headersCount, sizeof(UINT16));
		}
		target += sizeof(UINT16);
	}
	else
	{
		if (!simulateOnly)
		{
			UINT32 headersCount = op_htonl(length);
			op_memcpy(target, &headersCount, sizeof(UINT32));
		}
		target += sizeof(UINT32);
	}
}

SpdyStreamHandler::HeadersSubsets SpdyStreamHandler::FindSubsetForHeader(const char *headerName, size_t nameLength) const
{
	if (nameLength == sizeof("cookie") - 1 && op_strncmp(headerName, "cookie", nameLength) == 0)
		return HS_Cookie;
	if (nameLength == sizeof("accept") - 1 && op_strncmp(headerName, "accept", nameLength) == 0 ||
		nameLength == sizeof("accept-charset") - 1 && op_strncmp(headerName, "accept-charset", nameLength) == 0 ||
		nameLength == sizeof("accept-encoding") - 1 && op_strncmp(headerName, "accept-encoding", nameLength) == 0 ||
		nameLength == sizeof("accept-language") - 1 && op_strncmp(headerName, "accept-language", nameLength) == 0 ||
		nameLength == sizeof("host") - 1 && op_strncmp(headerName, "host", nameLength) == 0 ||
		nameLength == sizeof("version") - 1 && op_strncmp(headerName, "version", nameLength) == 0 ||
		nameLength == sizeof("method") - 1 && op_strncmp(headerName, "method", nameLength) == 0 ||
		nameLength == sizeof("scheme") - 1 && op_strncmp(headerName, "scheme", nameLength) == 0 ||
		nameLength == sizeof(":host") - 1 && op_strncmp(headerName, ":host", nameLength) == 0 ||
		nameLength == sizeof(":version") - 1 && op_strncmp(headerName, ":version", nameLength) == 0 ||
		nameLength == sizeof(":method") - 1 && op_strncmp(headerName, ":method", nameLength) == 0 ||
		nameLength == sizeof(":scheme") - 1 && op_strncmp(headerName, ":scheme", nameLength) == 0 ||
		nameLength == sizeof("user-agent") - 1 && op_strncmp(headerName, "user-agent", nameLength) == 0)
		return HS_WhiteListed;
	return HS_Other;
}

UINT32 SpdyStreamHandler::GetHeadersBlock(const SpdyHeaders *headers, char *target, BOOL simulateOnly) const
{
	char *targetIt = target;
	AddLengthInNetworkOrder(targetIt, headers->headersCount, simulateOnly);
	for (UINT32 i = 0; i < headers->headersCount; ++i)
	{
		AddLengthInNetworkOrder(targetIt, headers->headersArray[i].name.length, simulateOnly);
		if (!simulateOnly)
			op_memcpy(targetIt, headers->headersArray[i].name.str, headers->headersArray[i].name.length);
		targetIt += headers->headersArray[i].name.length;
	
		AddLengthInNetworkOrder(targetIt, headers->headersArray[i].value.length, simulateOnly);
		if (!simulateOnly)
			op_memcpy(targetIt, headers->headersArray[i].value.str, headers->headersArray[i].value.length);
		targetIt += headers->headersArray[i].value.length;
	}
	return targetIt - target;
}

void SpdyStreamHandler::AddCompressedHeadersL(OpData &target, const SpdyHeaders *headers) const
{
	UINT32 headersBlockSize = GetHeadersBlock(headers, NULL, TRUE);
	char *headersBlock = OP_NEWA_L(char, headersBlockSize);
	GetHeadersBlock(headers, headersBlock, FALSE);

	char *outputBuffer = static_cast<char*>(g_memory_manager->GetTempBuf2k());
	size_t outputBufferSize = g_memory_manager->GetTempBuf2kLen();

	z_stream &compressor = compressionProvider->GetCompressionStream();
	compressor.next_in = reinterpret_cast<Bytef*>(headersBlock);
	compressor.avail_in = headersBlockSize;
	compressor.clas = Z_CLASS_STANDARD;

	while (compressor.avail_in > 0)
	{
		compressor.next_out = reinterpret_cast<Bytef*>(outputBuffer);
		compressor.avail_out = outputBufferSize;
		SPDY_LEAVE_IF_ERROR(compressionProvider->Compress());
		UINT32 compressedSize = outputBufferSize - compressor.avail_out;
		SPDY_LEAVE_IF_ERROR(target.AppendCopyData(outputBuffer, compressedSize));
	}
	OP_DELETEA(headersBlock);
}

void SpdyStreamHandler::AddCompressedDataL(OpData &target, const char *data, size_t len, int clas, BOOL syncFlush) const
{
	char *outputBuffer = static_cast<char*>(g_memory_manager->GetTempBuf2k());
	size_t outputBufferSize = g_memory_manager->GetTempBuf2kLen();

	z_stream &compressor = compressionProvider->GetCompressionStream();

	if (compressor.clas == Z_CLASS_STANDARD && clas != Z_CLASS_STANDARD)
	{
		compressor.avail_in = 0;
		compressor.next_out = reinterpret_cast<Bytef*>(outputBuffer);
		compressor.avail_out = outputBufferSize;
		SPDY_LEAVE_IF_ERROR(compressionProvider->Compress(Z_PARTIAL_FLUSH));
		UINT32 compressedSize = outputBufferSize - compressor.avail_out;
		SPDY_LEAVE_IF_ERROR(target.AppendCopyData(outputBuffer, compressedSize));
	}

	compressor.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data));
	compressor.avail_in = len;
	compressor.clas = clas;

	while (compressor.avail_in > 0)
	{
		compressor.next_out = reinterpret_cast<Bytef*>(outputBuffer);
		compressor.avail_out = outputBufferSize;
		SPDY_LEAVE_IF_ERROR(compressionProvider->Compress(syncFlush ? Z_SYNC_FLUSH : (clas == Z_CLASS_STANDARD ? Z_NO_FLUSH : Z_PARTIAL_FLUSH)));
		UINT32 compressedSize = outputBufferSize - compressor.avail_out;
		SPDY_LEAVE_IF_ERROR(target.AppendCopyData(outputBuffer, compressedSize));
	}
}

void SpdyStreamHandler::AddSecurelyCompressedHeadersL(OpData &target, const SpdyHeaders *headers) const
{
	char *it, lengthField[4]; /* ARRAY OK 2012-11-12 kswitalski */
	AddLengthInNetworkOrder(it = lengthField, headers->headersCount, FALSE);
	AddCompressedDataL(target, lengthField, it-lengthField, Z_CLASS_STANDARD, FALSE);

	for (UINT32 i = 0; i < headers->headersCount; ++i)
	{
		AddLengthInNetworkOrder(it = lengthField, headers->headersArray[i].name.length, FALSE);
		AddCompressedDataL(target, lengthField, it-lengthField, Z_CLASS_STANDARD, FALSE);
		AddCompressedDataL(target, headers->headersArray[i].name.str, headers->headersArray[i].name.length, Z_CLASS_STANDARD, FALSE);
		AddLengthInNetworkOrder(it = lengthField, headers->headersArray[i].value.length, FALSE);
		AddCompressedDataL(target, lengthField, it-lengthField, Z_CLASS_STANDARD, FALSE);
		
		HeadersSubsets subset = FindSubsetForHeader(headers->headersArray[i].name.str, headers->headersArray[i].name.length);

		if (subset == HS_Cookie)
		{
			const char *it = headers->headersArray[i].value.str, *itEnd = headers->headersArray[i].value.str + headers->headersArray[i].value.length, *semicolonPtr;
			while (it < itEnd && (semicolonPtr = reinterpret_cast<const char*>(op_memchr(it, ';', itEnd - it))))
			{
				AddCompressedDataL(target, it, semicolonPtr - it + 1, Z_CLASS_COOKIE, i == headers->headersCount - 1 && semicolonPtr == itEnd - 1);
				it = semicolonPtr + 1;
			}
		}
		else if (subset == HS_WhiteListed)
			AddCompressedDataL(target, headers->headersArray[i].value.str, headers->headersArray[i].value.length, Z_CLASS_STANDARD, i == headers->headersCount - 1);
		else
			AddCompressedDataL(target, headers->headersArray[i].value.str, headers->headersArray[i].value.length, Z_CLASS_HUFFMAN_ONLY, i == headers->headersCount - 1);
	}
}

void SpdyStreamHandler::WriteSynStreamFrameL(SpdyNetworkBuffer &frameData)
{
	SpdyHeaders *headers = streamController->GetHeadersL(version);
	ANCHOR_PTR(SpdyHeaders, headers);
	ANCHORD(OpData, compressedHeaders);

	if (secureMode)
		AddSecurelyCompressedHeadersL(compressedHeaders, headers);
	else
		AddCompressedHeadersL(compressedHeaders, headers);

	ANCHOR_PTR_RELEASE(headers);
	OP_DELETE(headers);

	halfClosedFromClient = !streamController->HasDataToSend();
	UINT8 prio = version == SPDY_V2 ? 3 - (priority >> 6) : 7 - (priority >> 5);
	SynStreamFrameHeader ssfh(version, halfClosedFromClient ? SCF_CONTROL_FLAG_FIN : 0, streamId, 0, prio, compressedHeaders.Length());
	frameData.AppendCopyDataL(reinterpret_cast<char*>(&ssfh), SynStreamFrameHeader::GetSize());
	frameData.AppendCopyDataL(compressedHeaders);
}

void SpdyStreamHandler::WriteDataFrameL(SpdyNetworkBuffer &frameData)
{
	OP_ASSERT(streamController->HasDataToSend());

	size_t dataFrameMaxLength = DATA_FRAME_MAX_LENGTH;
	if (flowControlEnabled)
		 dataFrameMaxLength = MIN(serversWindowSize, dataFrameMaxLength);

	char *dataChunkBuff = reinterpret_cast<char*>(g_memory_manager->GetTempBuf());
	size_t dataChunkBuffSize = MIN(dataFrameMaxLength, g_memory_manager->GetTempBufLen());

	BOOL more;
	size_t dataChunkSize = streamController->GetNextDataChunkL(dataChunkBuff, dataChunkBuffSize, more);
	if (flowControlEnabled)
		serversWindowSize -= dataChunkSize;
	
	halfClosedFromClient = !more;
	DataFrameHeader dfh(streamId, halfClosedFromClient ? SCF_CONTROL_FLAG_FIN : 0, dataChunkSize);
	frameData.AppendCopyDataL(reinterpret_cast<char*>(&dfh), DataFrameHeader::GetSize());
	frameData.AppendCopyDataL(dataChunkBuff, dataChunkSize);
}

void SpdyStreamHandler::WriteResetStreamFrameL(SpdyNetworkBuffer &frameData)
{
	ResetStreamFrameHeader rsfh(version, streamId, static_cast<SpdyResetStatus>(pendingResetStatus));
	frameData.AppendCopyDataL(reinterpret_cast<char*>(&rsfh), ResetStreamFrameHeader::GetSize());
}

void SpdyStreamHandler::WriteWindowUpdateFrameL(SpdyNetworkBuffer &frameData)
{
	WindowUpdateFrameHeader wufh(version, streamId, clientsWindowDelta);
	frameData.AppendCopyDataL(reinterpret_cast<char*>(&wufh), WindowUpdateFrameHeader::GetSize());
	clientsWindowDelta = 0;
}

BOOL SpdyStreamHandler::HasNextFrame() const
{
	return streamState == SS_CREATING
		|| streamState == SS_SENDING_DATA && (!flowControlEnabled || serversWindowSize >= WINDOW_MIN_SIZE)
		|| streamState == SS_RESETING
		|| streamState == SS_SENDING_WINDOW_UPDATE;
}

void SpdyStreamHandler::Reset(UINT32 resetStatus)
{
	if (streamState == SS_PROCESSING_HEADERS || streamState == SS_PROCESSING_DATA_FRAME)
		requestedReset = TRUE;
	else
	{
		streamState = SS_RESETING;
		master->OnHasNewFrame(this);
	}
	pendingResetStatus = resetStatus;
	if (streamController)
		streamController->OnResetStream();
	streamController = NULL;
}

void SpdyStreamHandler::Goaway()
{
	streamState = SS_CLOSED;
	if (streamController)
	{
		streamController->OnGoAway();
		streamController = NULL;
	}
	master->NukeStream(this);
}

#endif // USE_SPDY

