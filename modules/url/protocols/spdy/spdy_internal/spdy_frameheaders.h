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


#ifndef _SPDY_FRAMEHEADERS_H_
#define _SPDY_FRAMEHEADERS_H_

#include "modules/url/protocols/spdy/spdy_internal/spdy_version.h"

enum SpdyControlType
{
  SCT_SYN_STREAM = 1,
  SCT_SYN_REPLY,
  SCT_RST_STREAM,
  SCT_SETTINGS,
  SCT_NOOP,
  SCT_PING,
  SCT_GOAWAY,
  SCT_HEADERS,
  SCT_WINDOW_UPDATE,
  SCT_CREDENTIAL,
};

enum SpdyControlFlags 
{
  SCF_CONTROL_FLAG_NONE = 0,
  SCF_CONTROL_FLAG_FIN = 1,
  SCF_CONTROL_FLAG_UNIDIRECTIONAL = 2
};

enum SpdyResetStatus
{
	SRS_PROTOCOL_ERROR,
	SRS_INVALID_STREAM,
	SRS_REFUSED_STREAM,
	SRS_UNSUPPORTED_VERSION,
	SRS_CANCEL,
	SRS_INTERNAL_ERROR,
	SRS_FLOW_CONTROL_ERROR
};

class SpdyFrameHeader
{
protected:
	union
	{
		struct
		{
			UINT16 control_and_version;
			UINT16 type;
		} control;
		struct
		{
			UINT32 control_and_stream_id;
		} data;
	};
public:
	BOOL IsControl() const { return op_ntohs(control.control_and_version) & 0x8000; }
	void SetControl(BOOL c) { control.control_and_version = c ? control.control_and_version | op_htons(0x8000) : control.control_and_version & op_htons(0x7fff); }

	static UINT32 GetSize() { return sizeof(UINT32); }
};

class ControlFrameHeader: public SpdyFrameHeader
{
	UINT32 flags_and_length;
public:
	UINT16 GetVersion() const { return op_ntohs(control.control_and_version) & 0x7fff; }
	UINT16 GetType() const { return op_ntohs(control.type); }
	UINT8 GetFlags() const { return (op_ntohl(flags_and_length) & 0xff000000) >> 24; }
	UINT32 GetLength() const { return (op_ntohl(flags_and_length) & 0xffffff); }

	void SetVersion(UINT16 v) { control.control_and_version = op_htons(op_ntohs(control.control_and_version) & 0x8000) | op_htons(v); }
	void SetType(UINT16 t) { control.type = op_htons(t); }
	void SetFlags(UINT8 f) { flags_and_length = op_htonl(op_ntohl(flags_and_length) & 0xffffff | (f << 24)); }
	void SetLength(UINT32 l) { flags_and_length = op_htonl(op_ntohl(flags_and_length) & 0xff000000 | l); }

	static UINT32 GetSize() { return SpdyFrameHeader::GetSize() + sizeof(UINT32); }

	ControlFrameHeader(UINT16 version, UINT16 type, UINT8 flags)
	{
		SetControl(TRUE);
		SetVersion(version);
		SetType(type);
		SetFlags(flags);
		SetLength(0);
	}
};

class SynStreamFrameHeader: public ControlFrameHeader
{
	UINT32 stream_id;
	UINT32 associated_to_stream_id;
	UINT16 priority;
public:
	UINT32 GetStreamId() { return op_ntohl(stream_id); }
	UINT32 GetAssociatedToStreamId() { return op_ntohl(associated_to_stream_id); }
	UINT16 GetPriority(SpdyVersion v) { return op_ntohs(priority) >> (v == SPDY_V2 ? 14 : 13); }
	UINT32 GetHeadersBlockSize() const { return GetLength() - 10; }

	void SetStreamId(UINT32 id) { stream_id = op_htonl(id & 0x7fffffff); }
	void SetAssociatedToStreamId(UINT32 id) { associated_to_stream_id = op_htonl(id & 0x7fffffff); }
	void SetPriority(UINT16 prio, SpdyVersion v) { priority = op_htons(prio << (v == SPDY_V2 ? 14 : 13)); }
	void SetLengthBasedOnHeadersLength(UINT32 headers_length) { SetLength(10 + headers_length); }

	static UINT32 GetSize() { return ControlFrameHeader::GetSize() + sizeof(UINT32) + sizeof(UINT32) + sizeof(UINT16); }

	SynStreamFrameHeader(UINT16 version, UINT8 flags, UINT32 stream_id, UINT32 associated_to_stream_id, UINT16 priority, UINT32 headersBlockLength)
		: ControlFrameHeader(version, SCT_SYN_STREAM, flags)
	{
		SetStreamId(stream_id);
		SetAssociatedToStreamId(associated_to_stream_id);
		SetPriority(priority, static_cast<SpdyVersion>(version));
		SetLengthBasedOnHeadersLength(headersBlockLength);
	}
};

class SynReplyFrameHeader: public ControlFrameHeader
{
	UINT32 stream_id;
	UINT16 unused;
public:
	UINT32 GetStreamId() const { return op_ntohl(stream_id) & 0x7fffffff; }
	UINT32 GetHeadersBlockSize(SpdyVersion v) const { return GetLength() - (v == SPDY_V2 ? 6 : 4); }

	void SetStreamId(UINT32 id) { stream_id = op_htonl(id & 0x7fffffff); }
	void SetLengthBasedOnHeadersLength(UINT32 headers_length, SpdyVersion v) { SetLength(4 + (v == SPDY_V2 ? sizeof(UINT16) : 0) + headers_length); }

	static UINT32 GetSize(SpdyVersion v) { return ControlFrameHeader::GetSize() + sizeof(UINT32) + (v == SPDY_V2 ? sizeof(UINT16) : 0); }
	
	SynReplyFrameHeader(UINT16 version, UINT8 flags, UINT32 stream_id, UINT32 headersBlockLength)
		: ControlFrameHeader(version, SCT_SYN_REPLY, flags)
	{
		SetStreamId(stream_id);
		SetLengthBasedOnHeadersLength(headersBlockLength, static_cast<SpdyVersion>(version));
		unused = 0;
	}
};

class ResetStreamFrameHeader: public ControlFrameHeader
{
	UINT32 stream_id;
	UINT32 status_code;
public:
	UINT32 GetStreamId() const { return op_ntohl(stream_id) & 0x7fffffff; }
	SpdyResetStatus GetStatusCode() const { return static_cast<SpdyResetStatus>(op_ntohl(status_code)); }

	void SetStreamId(UINT32 id) { stream_id = op_htonl(id & 0x7fffffff); }
	void SetStatusCode(SpdyResetStatus status) { status_code = op_htonl(static_cast<UINT32>(status)); }

	static UINT32 GetSize() { return ControlFrameHeader::GetSize() + sizeof(UINT32) + sizeof(UINT32); }
	
	ResetStreamFrameHeader(UINT16 version, UINT32 stream_id, SpdyResetStatus status_code)
		: ControlFrameHeader(version, SCT_RST_STREAM, 0)
	{
		SetStreamId(stream_id);
		SetStatusCode(status_code);
		SetLength(8);
	}
};

class SettingsFrameHeader: public ControlFrameHeader
{
	UINT32 number_of_entries;
public:
	UINT32 GetNumberOfEntries() const { return op_ntohl(number_of_entries); }
	void SetNumberOfEntries(UINT32 n) { number_of_entries = op_htonl(n); }

	static UINT32 GetSize() { return ControlFrameHeader::GetSize() + sizeof(UINT32); }

	SettingsFrameHeader(UINT16 version, UINT32 number_of_entr)
		: ControlFrameHeader(version, SCT_SETTINGS, 0)
	{
		SetNumberOfEntries(number_of_entr);
		SetLength(sizeof(UINT32) + 8 * number_of_entr);
	}
};

class NoopFrameHeader: public ControlFrameHeader
{
public:
	static UINT32 GetSize() { return ControlFrameHeader::GetSize(); }

	NoopFrameHeader(UINT16 version)
		: ControlFrameHeader(version, SCT_NOOP, 0)
	{
	}
};

class PingFrameHeader: public ControlFrameHeader
{
	UINT32 id;
public:
	UINT32 GetId() const { return op_ntohl(id); }
	void SetId(UINT32 i) { id = op_htonl(i); }

	static UINT32 GetSize() { return ControlFrameHeader::GetSize() + sizeof(UINT32); }

	PingFrameHeader(UINT16 version, UINT32 id)
		: ControlFrameHeader(version, SCT_PING, 0)
	{
		SetId(id);
		SetLength(4);
	}
};

class GoawayFrameHeader: public ControlFrameHeader
{
	UINT32 last_good_stream_id;
	UINT32 status;
public:
	UINT32 GetLastGoodStreamId() const { return op_ntohl(last_good_stream_id) & 0x7fffffff; }
	void SetLastGoodStreamId(UINT32 id) { last_good_stream_id = op_htonl(id & 0x7fffffff); }
	UINT32 GetStatusCode() const { return op_ntohl(status); }
	void SetStatusCode(UINT32 stat) { status = op_htonl(stat); }

	static UINT32 GetSize(SpdyVersion v) { return ControlFrameHeader::GetSize() + sizeof(UINT32) + (v == SPDY_V2 ? 0 : sizeof(UINT32)); }

	GoawayFrameHeader(UINT16 version, UINT32 last_good_stream_id, UINT32 status_code)
		: ControlFrameHeader(version, SCT_GOAWAY, 0)
	{
		SetLastGoodStreamId(last_good_stream_id);
		SetStatusCode(status_code);
		SetLength(version == 2 ? 4 : 8);
	}
};

class HeadersFrameHeader: public ControlFrameHeader
{
	UINT32 stream_id;
	UINT16 unused;
public:
	UINT32 GetStreamId() const { return op_ntohl(stream_id) & 0x7fffffff; }
	UINT32 GetHeadersBlockSize(SpdyVersion v) const { return GetLength() - (v == SPDY_V2 ? 6 : 4); }

	void SetStreamId(UINT32 id) { stream_id = op_htonl(id & 0x7fffffff); }

	static UINT32 GetSize(SpdyVersion v) { return ControlFrameHeader::GetSize() + sizeof(UINT32) + (v == SPDY_V2 ? sizeof(UINT16) : 0); }
	
	HeadersFrameHeader(UINT16 version, UINT8 flags, UINT32 stream_id)
		: ControlFrameHeader(version, SCT_HEADERS, flags)
	{
		SetStreamId(stream_id);
		unused = 0;
	}
};

class WindowUpdateFrameHeader: public ControlFrameHeader
{
	UINT32 stream_id;
	UINT32 delta_window_size;
public:
	UINT32 GetStreamId() const { return op_ntohl(stream_id) & 0x7fffffff; }
	UINT32 GetDeltaWindowSize() const { return op_ntohl(delta_window_size) & 0x7fffffff; }

	void SetStreamId(UINT32 id) { stream_id = op_htonl(id & 0x7fffffff); }
	void SetDeltaWindowSize(UINT32 delta) { delta_window_size = op_htonl(delta & 0x7fffffff); }

	static UINT32 GetSize() { return ControlFrameHeader::GetSize() + sizeof(UINT32) + sizeof(UINT32); }

	WindowUpdateFrameHeader(UINT16 version, UINT32 stream_id, UINT32 delta)
		: ControlFrameHeader(version, SCT_WINDOW_UPDATE, 0)
	{
		SetStreamId(stream_id);
		SetDeltaWindowSize(delta);
		SetLength(8);
	}
};

class DataFrameHeader: public SpdyFrameHeader
{
	UINT32 flags_and_length;
public:
	UINT32 GetStreamId() const { return op_ntohl(data.control_and_stream_id) & 0x7fffffff; }
	UINT8 GetFlags() const { return (op_ntohl(flags_and_length) & 0xff000000) >> 24; }
	UINT32 GetLength() const { return (op_ntohl(flags_and_length) & 0xffffff); }

	void SetStreamId(UINT32 id) { data.control_and_stream_id = op_htonl(op_ntohl(data.control_and_stream_id) & 0x80000000) | op_htonl(id); }
	void SetFlags(UINT8 f) { flags_and_length = op_htonl(op_ntohl(flags_and_length) & 0xffffff | (f << 24)); }
	void SetLength(UINT32 l) { flags_and_length = op_htonl(op_ntohl(flags_and_length) & 0xff000000 | l); }

	static UINT32 GetSize() { return SpdyFrameHeader::GetSize() + sizeof(UINT32); }

	DataFrameHeader(UINT32 streamId, UINT8 flags, UINT32 length)
	{
		SetControl(FALSE);
		SetStreamId(streamId);
		SetFlags(flags);
		SetLength(length);
	}
};

#endif // _SPDY_FRAMEHEADERS_H_
