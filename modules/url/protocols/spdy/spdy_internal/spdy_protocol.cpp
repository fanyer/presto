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

#include "modules/url/protocols/spdy/spdy_protocol.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_frameheaders.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_hdrcompr.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_common.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/zlib/zlib.h"

#ifdef SELFTEST
# define CALL_ON_SELFTESTLISTENER(method, params) \
    if (selftestListener)                         \
        selftestListener-> method params;
#else
# define CALL_ON_SELFTESTLISTENER(method, params)
#endif

SpdyFramesHandler::SpdyFramesHandler(SpdyVersion ver, ServerName_Pointer serverName, unsigned short portnumber, BOOL privacyMode, BOOL secureMode, SpdyFramesHandlerListener *l, BOOL serverMode):
	compressionProvider(NULL),
	settingsController(ver, serverName, portnumber, privacyMode),
	pingController(ver, serverMode ? 2 : 1),
	streamReseter(ver),
	activeDataConsumer(NULL),
	nextStreamId(serverMode ? 2 : 1),
	removeStreamsTimerSet(FALSE),
	noMoreStreams(FALSE),
	pendingDataToSend(FALSE),
	listener(l),
#ifdef SELFTEST
	selftestListener(NULL),
#endif // SELFTEST
	version(ver),
	secureMode(secureMode)
{
	OP_ASSERT(version == SPDY_V2 || version == SPDY_V3);
	maxBufLen = (unsigned long)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
}

SpdyFramesHandler::~SpdyFramesHandler()
{
	OP_DELETE(compressionProvider);
	prioritizedFramesProducers.RemoveAll();
	priorityOrderedFramesProducers.RemoveAll();
	streamHandlers.DeleteAll();
	streamsToDestroy.Clear();
}

void SpdyFramesHandler::ConstructL()
{
	settingsController.InitL();
	if (settingsController.HasNextFrame())
		settingsController.Into(&prioritizedFramesProducers);

	pingController.SetListener(listener);
	removeStreamsTimer.SetTimerListener(this);
	AddToProducersList(&pingController);
	
	compressionProvider = OP_NEW_L(SpdyCompressionProvider, (version, secureMode));
	SPDY_LEAVE_IF_ERROR(compressionProvider->Init());
	decompressingConsumer.SetCompressionProvider(compressionProvider);
}

void SpdyFramesHandler::AddToProducersList(SpdyFramesProducer *producer)
{
	if (priorityOrderedFramesProducers.Empty())
		producer->Into(&priorityOrderedFramesProducers);
	else
	{
		UINT8 prio = producer->GetPriority();
		SpdyFramesProducer *it = priorityOrderedFramesProducers.First();
		while (it->Suc() && it->Suc()->GetPriority() >= prio)
			it = it->Suc();
		producer->Follow(it);
	}
}

UINT32 SpdyFramesHandler::CreateStreamL(SpdyStreamController *streamController)
{
	SpdyStreamHandler *handler = OP_NEW_L(SpdyStreamHandler, (nextStreamId, version, streamController, compressionProvider, settingsController.GetInitialWindowSize(), secureMode, this));
	SPDY_LEAVE_IF_ERROR(streamHandlers.Add(nextStreamId, handler));
	nextStreamId += 2;
	handler->Into(&prioritizedFramesProducers);
	
	return handler->GetStreamId();
}

void SpdyFramesHandler::NukeStream(SpdyStreamHandler *streamToDelete)
{
	if (streamToDelete->InList())
		streamToDelete->Out();
	streamToDelete->Link::Into(&streamsToDestroy);
	
	if (!removeStreamsTimerSet)
	{
		removeStreamsTimerSet = TRUE;
		removeStreamsTimer.Start(0);
	}
}

void SpdyFramesHandler::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &removeStreamsTimer);
	removeStreamsTimerSet = FALSE;

	Link *suc;
	for (Link *it = streamsToDestroy.First(); it; it = suc)
	{
		suc = it->Suc();
		
		SpdyStreamHandler *data;
		streamHandlers.Remove(static_cast<SpdyStreamHandler*>(it)->GetStreamId(), &data);

		if (static_cast<SpdyStreamHandler*>(it) != activeDataConsumer)
		{
			it->Out();
			OP_DELETE(it);
		}
	}
}

BOOL SpdyFramesHandler::CanCreateNewStream()
{
	if (noMoreStreams)
		return FALSE;
	UINT32 maxConcurrentStreams = settingsController.GetMaxConcurrentStreams();
	UINT32 streamCount = streamHandlers.GetCount();
	return !maxConcurrentStreams || streamCount < maxConcurrentStreams;
}

BOOL SpdyFramesHandler::SendDataL(SComm* sink)
{
	BOOL somethingSent = FALSE;
	SpdyNetworkBuffer buffer(HTTP_TmpBuf, HTTP_TmpBufSize);
	ANCHOR(SpdyNetworkBuffer, buffer); 
	while (!prioritizedFramesProducers.Empty())
	{
		SpdyFramesProducer *producer = prioritizedFramesProducers.First();
		OP_ASSERT(producer->HasNextFrame());
		producer->Out();

		producer->WriteNextFrameL(buffer);
		buffer.SendAndClearL(sink);
		somethingSent = TRUE;
	}

	pendingDataToSend = FALSE;
	if (!priorityOrderedFramesProducers.Empty())
	{
		SpdyFramesProducer *firstProducer = priorityOrderedFramesProducers.First();
		while (firstProducer && buffer.Length() < maxBufLen)
		{
			UINT8 currentPriority = firstProducer->GetPriority();
			BOOL producedSomethingForPriority = FALSE;
			SpdyFramesProducer *it, *suc;

			for (it = firstProducer; it && it->GetPriority() == currentPriority && buffer.Length() < maxBufLen; it = suc)
			{
				suc = it->Suc();

				if (!it->HasNextFrame())
					continue;

				it->WriteNextFrameL(buffer);
				producedSomethingForPriority = TRUE;
			}
			if (!producedSomethingForPriority)
				firstProducer = it;
		}
		pendingDataToSend = firstProducer != NULL;

		if (buffer.Length())
		{
			buffer.SendAndClearL(sink);
			somethingSent = TRUE;
		}
	}

	if (pendingDataToSend)
		listener->OnHasDataToSend();
	return somethingSent;
}

void SpdyFramesHandler::ProcessReceivedDataL(SComm* sink)
{
	unsigned long max_buf_len = (unsigned long)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
	unsigned long buf_len = (max_buf_len < HTTP_TmpBufSize) ? (unsigned long)max_buf_len : HTTP_TmpBufSize;
	
	char* tmp_buf = HTTP_TmpBuf;
	size_t cnt_len;
	cnt_len = sink->ReadData(tmp_buf, buf_len);

	while (cnt_len)
	{
		SPDY_LEAVE_IF_ERROR(buffer.AppendCopyData(tmp_buf, cnt_len));

		while (!buffer.IsEmpty())
		{
			if (activeDataConsumer)
			{
				BOOL stillHungry = activeDataConsumer->ConsumeDataL(buffer);
				if (!stillHungry)
					activeDataConsumer = NULL;
			}
			else
			{
				if (!TryToParseHeader())
					break;
			}
		}

		cnt_len = sink->ReadData(tmp_buf, buf_len);
	}

	if (pendingDataToSend)
		listener->OnHasDataToSend();
}

BOOL SpdyFramesHandler::TryToParseHeader()
{
	const UINT32 maxHeaderLength = 18;// the longest spdy frame header is 18 bytes long
	char header[maxHeaderLength]; /* ARRAY OK 2012-04-29 kswitalski */
	size_t header_len = MIN(buffer.Length(), maxHeaderLength);
	if (header_len < 4)
		return FALSE;

	buffer.CopyInto(header, header_len);

	if (header_len < SpdyFrameHeader::GetSize())
		return FALSE;
	else
	{
		if (reinterpret_cast<SpdyFrameHeader *>(header)->IsControl())
		{
			if (header_len < ControlFrameHeader::GetSize())
				return FALSE;
			
			ControlFrameHeader *controlFrame = reinterpret_cast<ControlFrameHeader *>(header);
			if (controlFrame->GetVersion() != version)
			{
				listener->OnProtocolError();
				return FALSE;
			}

			switch (controlFrame->GetType())
			{
			case SCT_SYN_STREAM:
				if (header_len >= SynStreamFrameHeader::GetSize())
				{
					// we don't have support for push/hint yet
					SynStreamFrameHeader *synStreamFrame = reinterpret_cast<SynStreamFrameHeader *>(header);
					CALL_ON_SELFTESTLISTENER(OnReceivedSynStream, (synStreamFrame->GetStreamId()));
					if (synStreamFrame->GetStreamId() % 2 == nextStreamId % 2)
					{
						listener->OnProtocolError();
						return FALSE;
					}
					decompressingConsumer.SetAmountOfDataToConsume(synStreamFrame->GetHeadersBlockSize());
					activeDataConsumer = &decompressingConsumer;
					buffer.Consume(SynStreamFrameHeader::GetSize());
					ResetStream(synStreamFrame->GetStreamId(), SRS_REFUSED_STREAM);
					return TRUE;
				}
				break;
			case SCT_SYN_REPLY:
				if (header_len >= SynReplyFrameHeader::GetSize(version))
				{
					SynReplyFrameHeader *synReplyFrame = reinterpret_cast<SynReplyFrameHeader *>(header);
					CALL_ON_SELFTESTLISTENER(OnReceivedSynReply, (synReplyFrame->GetStreamId()));
					SpdyStreamHandler *stream = FindStream(synReplyFrame->GetStreamId());
					
					if (stream && stream->AcceptsNewFrames() && !stream->ReceivedSynReply())
					{
						stream->SynReplyFrameLoaded(synReplyFrame);
						activeDataConsumer = stream;
					}
					else
					{
						if (stream && stream->ReceivedSynReply())
							stream->Reset(SRS_PROTOCOL_ERROR);
						decompressingConsumer.SetAmountOfDataToConsume(synReplyFrame->GetHeadersBlockSize(version));
						activeDataConsumer = &decompressingConsumer;
					}
					
					buffer.Consume(SynReplyFrameHeader::GetSize(version));
					return TRUE;
				}
				break;
			case SCT_SETTINGS:
				if (header_len >= SettingsFrameHeader::GetSize())
				{
					SettingsFrameHeader *settingsFrame = reinterpret_cast<SettingsFrameHeader *>(header);
					settingsController.SettingsFrameLoaded(settingsFrame);
					buffer.Consume(SettingsFrameHeader::GetSize());
					activeDataConsumer = &settingsController;
					return TRUE;
				}
				break;
			case SCT_RST_STREAM:
				if (header_len >= ResetStreamFrameHeader::GetSize())
				{
					ResetStreamFrameHeader *resetFrame = reinterpret_cast<ResetStreamFrameHeader *>(header);
					CALL_ON_SELFTESTLISTENER(OnReceivedReset, (resetFrame->GetStreamId(), resetFrame->GetStatusCode()));
					SpdyStreamHandler *stream = FindStream(resetFrame->GetStreamId());
					if (stream && stream->AcceptsNewFrames())
						stream->ResetFrameLoaded(resetFrame);
					buffer.Consume(ResetStreamFrameHeader::GetSize());
					return TRUE;
				}
				break;
			case SCT_NOOP:
				buffer.Consume(NoopFrameHeader::GetSize());
				return TRUE;
			case SCT_PING:
				if (header_len >= PingFrameHeader::GetSize())
				{
					PingFrameHeader *pingFrame = reinterpret_cast<PingFrameHeader *>(header);
					pingController.PingFrameLoaded(pingFrame);
					buffer.Consume(PingFrameHeader::GetSize());
					return TRUE;
				}
				break;
			case SCT_GOAWAY:
				if (header_len >= GoawayFrameHeader::GetSize(version))
				{
					GoawayFrameHeader *goawayFrame = reinterpret_cast<GoawayFrameHeader *>(header);
					CALL_ON_SELFTESTLISTENER(OnReceivedGoAway, ());
					buffer.Consume(GoawayFrameHeader::GetSize(version));
					noMoreStreams = TRUE;
					listener->OnGoAway();
					CancelStreamsWithHigherId(goawayFrame->GetLastGoodStreamId());
					return TRUE;
				}
				break;
			case SCT_HEADERS:
				if (header_len >= HeadersFrameHeader::GetSize(version))
				{
					HeadersFrameHeader *headersFrame = reinterpret_cast<HeadersFrameHeader *>(header);
					SpdyStreamHandler *stream = FindStream(headersFrame->GetStreamId());
					if (stream && stream->AcceptsNewFrames())
					{
						stream->HeadersFrameLoaded(headersFrame);
						activeDataConsumer = stream;
					}
					else
					{
						decompressingConsumer.SetAmountOfDataToConsume(headersFrame->GetHeadersBlockSize(version));
						activeDataConsumer = &decompressingConsumer;
					}
					buffer.Consume(HeadersFrameHeader::GetSize(version));
					return TRUE;
				}
			case SCT_WINDOW_UPDATE:
				if (header_len >= WindowUpdateFrameHeader::GetSize())
				{
					WindowUpdateFrameHeader *windowUpdateFrame = reinterpret_cast<WindowUpdateFrameHeader *>(header);
					SpdyStreamHandler *stream = FindStream(windowUpdateFrame->GetStreamId());
					if (stream && stream->AcceptsNewFrames())
						stream->WindowUpdateFrameLoaded(windowUpdateFrame);
					buffer.Consume(WindowUpdateFrameHeader::GetSize());
					return TRUE;
				}
			default:
				listener->OnProtocolError();
			}
		}
		else
		{
			if (header_len < DataFrameHeader::GetSize())
				return FALSE;

			DataFrameHeader *dataFrame = reinterpret_cast<DataFrameHeader *>(header);
			SpdyStreamHandler *stream = FindStream(dataFrame->GetStreamId());
			if (stream && stream->AcceptsNewFrames())
			{
				if (!stream->ReceivedSynReply())
				{
					listener->OnProtocolError();
					return FALSE;
				}
				stream->DataFrameLoaded(dataFrame);
				activeDataConsumer = stream;
			}
			else
			{
				if (dataFrame->GetStreamId() >= nextStreamId)
					ResetStream(dataFrame->GetStreamId(), SRS_INVALID_STREAM);
				nullConsumer.SetAmountOfDataToConsume(dataFrame->GetLength());
				activeDataConsumer = &nullConsumer;
			}
			buffer.Consume(DataFrameHeader::GetSize());
			return TRUE;
		}
	}
	return FALSE;
}

SpdyStreamHandler *SpdyFramesHandler::FindStream(UINT32 streamId)
{
	SpdyStreamHandler *handler;
	if (OpStatus::IsError(streamHandlers.GetData(streamId, &handler)))
		return NULL;
	return handler;
}

void SpdyFramesHandler::RemoveStream(UINT32 streamId)
{
	SpdyStreamHandler *stream = FindStream(streamId);
	if (stream)
		stream->Reset(SRS_CANCEL);
}

void SpdyFramesHandler::ResetStream(UINT32 streamId, UINT32 resetStatus)
{
	streamReseter.ResetStream(streamId, resetStatus);
	if (!streamReseter.InList())
		AddToProducersList(&streamReseter);
}

void SpdyFramesHandler::CancelStreamsWithHigherId(UINT32 lastGoodId)
{
	OpHashIterator *it = streamHandlers.GetIterator();
	OP_STATUS err = OpStatus::OK;
	for (err = it->First(); OpStatus::IsSuccess(err); err = it->Next())
	{
		SpdyStreamHandler* streamHandler = static_cast<SpdyStreamHandler*>(it->GetData());
		if (streamHandler->GetStreamId() > lastGoodId)
			streamHandler->Goaway();
	}
	OP_DELETE(it);
}

void SpdyFramesHandler::OnHasNewFrame(SpdyStreamHandler *stream)
{
	if (!stream->InList())
		AddToProducersList(stream);
	pendingDataToSend = TRUE;
}

#endif // USE_SPDY

