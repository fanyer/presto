/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Niklas Beischer <no@opera.com>, Erik Moller <emoller@opera.com>
**
*/

#ifndef WEBSOCKET_PROTOCOL_H
#define WEBSOCKET_PROTOCOL_H

#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/websocket.h"
#include "modules/auth/auth_man.h"
#include "modules/formats/hdsplit.h"

class WebSocket_Connection;
class WebSocket_Server_Manager;

class WebSocketProtocol : public OpWebSocket, public ProtocolComm, public Link, public AuthenticationInterface
{
public:
	class Frame : public ListElement<Frame>
	{
	public:

		// Various extensions that can be defined in bits 12-14 in FrameControlBits.
		// No extensions are currently implemented.
		enum FrameExtensionId
			{
				NO_EXTENSIONS,
				COMPRESSION = 0x1 << 12 // Not yet implemented. http://tools.ietf.org/html/draft-tyoshino-hybi-websocket-perframe-deflate-06
			};

		// The Operation code is defined in http://tools.ietf.org/html/rfc6455#section-5.2.
		// The code is stored in bits 8-11 in FrameControlBits, hence the 8 bit roll.
		enum FrameOperationCode
		{
			OPCODE_CONTINUATION_FRAME = 0x00,
			OPCODE_TEXT_FRAME         = 0x01 << 8,
			OPCODE_BINARY_FRAME       = 0x02 << 8,
			// 3-7 is reserved for extensions

			// Control frames
			OPCODE_CONNECTION_CLOSE   = 0x08 << 8,
			OPCODE_CONNECTION_PING    = 0x09 << 8,
			OPCODE_CONNECTION_PONG    = 0x0A << 8,
			// B-F is reserved for extensions

			// Special internal code
			INTERNALCODE_INVALID_FRAME = 0xFFFF
		};

		// Bit flags and codes defined in http://tools.ietf.org/html/rfc6455#section-5.2
		// 16 bits in total.
		enum FrameControlBits
		{
			NOT_BITS_SET                   = 0x0000,
			LENGTH_BIT_MASK                = 0x007f,   // bits 0-7 are reserved for message lengths 0-125. Lengths 126 and 127 signals 16 or 64 bit lengths respectively,
													   // which in case the 2 or 8 next bytes are used for storing length.
			MASKED                         = 0x0080,   // bit 7 is for masking. Client will always mask, server will never mask.
			OPERATION_CODE_BIT_MASK        = 0x0F00,   // Bits 8-11 are reserved for operation code, as defined by FrameOperationCode.
			OPERATION_CODE_CONTROL_FRAME   = 0x0800,   // Bit 11 (the most significant bit of the operation code) signals control frame.
			EXTENSIONS_CODE_BIT_MASK       = 0x7000,   // Bits 12-14 (RS1, RS2 and RS3) are reserved for extension codes.
			FIN                            = 0x8000    // bit 15 signals that this frame is finished, and will not be continued in a succesive frame.
		};

		enum FrameCloseCodes
		{
			CLOSE_NO_CODE = 0,
			CLOSE_NORMAL = 1000, // Indicates a normal closure, meaning that the purpose for which the connection was established has been fulfilled.
			CLOSE_GOING_AWAY = 1001, // Indicates that an endpoint is "going away", such as a server going down or a browser having navigated away from a page.
			CLOSE_PROTOCOL_ERROR = 1002,  // Indicates that an endpoint is terminating the connection due to a protocol error.
			CLOSE_TERMINATING_CONNECTION = 1003, // Indicates that an endpoint is terminating the connection because it has received a type of data it cannot accept (e.g., an endpoint that understands only text data MAY send this if it receives a binary message).
			CLOSE_RESERVED = 1004, //  Reserved.  The specific meaning might be defined in the future.
			CLOSE_RESERVED_APPLICATION_LAYER_NO_STATUS = 1005, // Reserved value and MUST NOT be set as a status code in aClose control frame by an endpoint.  It is designated for use in applications expecting a status code to indicate that no status code was actually present.
			CLOSE_RESERVED_APPLICATION_LAYER_CLOSED_ABNORMALLY = 1006, // Reserved value and MUST NOT be set as a status code in a Close control frame by an endpoint.  It is designated for use in applications expecting a status code to indicate that the connection was closed abnormally, e.g., without sending or receiving a Close control frame.
			CLOSE_MESSAGE_INCONSISTENT_WITH_FRAME_TYPE = 1007, // Indicates that an endpoint is terminating the connection because it has received data within a message that was not consistent with the type of the message (e.g., non-UTF-8 [RFC3629] data within a text message).
			CLOSE_MESSAGE_VIOLATES_POLICY = 1008, // Indicates that an endpoint is terminating the connection because it has received a message that violates its policy.  This is a generic status code that can be returned when there is no other more suitable status code (e.g., 1003 or 1009) or if there is a need to hide specific details about the policy.
			CLOSE_MESSAGE_TO_BIG = 1009, // Indicates that an endpoint is terminating the connection because it has received a message that is too big for it to process.
			CLOSE_MISSING_EXTENSION_NEGOTATION = 1010, // Indicates that an endpoint (client) is terminating the connection because it has expected the server to negotiate one or more extension, but the server didn't return them in the response message of the WebSocket handshake.  The list of extensions that are needed SHOULD appear in the /reason/ part of the Close frame. Note that this status code is not used by the server, because it can fail the WebSocket handshake instead.
			CLOSE_UNEXPECTED_CONDITION = 1011, // Indicates that a server is terminating the connection because it encountered an unexpected condition that prevented it from fulfilling the request.
			CLOSE_RESERVERD_APPLICATION_LAYER_TLS_ERROR = 1015, // Reserved value and MUST NOT be set as a status code in a Close control frame by an endpoint.  It is designated for use in applications expecting a status code to indicate that the connection was closed due to a failure to perform a TLS handshake (e.g., the server certificate can't be verified).

			CLOSE_START_UNDEFINED_CUSTOM_CODES = 3000, // Status codes in the range 3000-3999 are reserved for use by libraries, frameworks, and applications.  These status codes are registered directly with IANA.  The interpretation of these codes is undefined by this protocol
			CLOSE_START_PRIVATE_CODES = 4000,  // Status codes in the range 4000-4999 are reserved for private use and thus can't be registered.  Such codes can be used by prior agreements between WebSocket applications.  The interpretation of these codes is undefined by this protocol.
			CLOSE_MAX_CLOSE_CODE = 4999 // All codes above this value are illegal.
		};

		FrameExtensionId GetExtensionCode() { return static_cast<FrameExtensionId>(frame_control_bits & EXTENSIONS_CODE_BIT_MASK); }

		UINT16  frame_control_bits; 	// Control bits defined by FrameControlBits and FrameOperationCode.
		FrameOperationCode operation_code;

		INT		      frame_start;                    // Start byte of the frame, -1 means the frame is a continuation from the previous buffer.
		UINT          payload_start;                  // Start byte of the data.
		UINT          payload_end;                    // End byte of the data, a byte index outside the buffer means there will be a continuation.
		BOOL          finished;                       // Frame boundaries found and ready to be sent.
		OpFileLength  data_left_to_receive;           // Total length of the frame.

		BOOL 	spool_to_disk;
		BOOL	delete_file;
		OpFile 	file;

		Frame() : frame_control_bits(0), operation_code(INTERNALCODE_INVALID_FRAME), frame_start(0), payload_start(0), payload_end(0), finished(FALSE), data_left_to_receive(0), spool_to_disk(FALSE), delete_file(FALSE){ }
		~Frame() {
			Out();
			if (delete_file)
			{
				OpStatus::Ignore(file.Close());
				OpStatus::Ignore(file.Delete());
			}
		}
	};


	class FrameBuffer : public ListElement<FrameBuffer>
	{
	public:
		UINT8*          buffer;         // Buffer where data for raw frames is stored.
		UINT            buffer_size;    // Allocated size of buffer.
		UINT            used;           // Currently used buffer.
		UINT            consumed;       // How much of the used data has been consumed (parsed).
		AutoDeleteList<Frame>  frames;         // List of frames located in the buffer.

		FrameBuffer(UINT8* buf = NULL, UINT len = 0, UINT total_size = 0) : buffer(buf), buffer_size(total_size), used(len), consumed(0) { }

		~FrameBuffer()
		{
			if(InList())
				Out();
			if (buffer)
				OP_DELETEA(buffer);
		}

		// Initialize the buffer.
		OP_STATUS Create();

		// Reset the buffer for reuse.
		void Reset();

		// Check if the buffer is full.
		BOOL IsFull();

		// Check if the last frame of this buffer is continued.
		BOOL IsContinued();

		BOOL IsConsumed();


	};

	class OutBuffer : public ListElement<OutBuffer>
	{
	public:
		virtual ~OutBuffer(){ Out(); }
		Frame::FrameOperationCode GetDataType() { return m_data_type; }
		BOOL HasStartedSending(){ return m_has_started; }
		BOOL IsFinished(){ return m_finished; }
		virtual OpFileLength GetTotalSizeLeft() = 0;

		virtual OP_STATUS StartSendingData() = 0;

		virtual UINT8* GetNextSlice(UINT max_size, UINT &size) = 0;

	protected:
		OutBuffer(Frame::FrameOperationCode  data_type, BOOL finished) : m_data_type(data_type), m_has_started(FALSE), m_finished(finished){}

		Frame::FrameOperationCode m_data_type;
		BOOL m_has_started;
		BOOL m_finished; // True if the message is not fragmented into OPCODE_CONTINUATION_FRAME frames.
	};

	struct Header : public ListElement<Header>
	{
		Header() : name(NULL), value(NULL), nameLen(0), valueLen(0) { }

		const char *name;
		const char *value;
		unsigned int nameLen;
		unsigned int valueLen;
	};

	static OP_STATUS Create(OpWebSocket** socket, OpWebSocketListener* listener, MessageHandler* mh);

	virtual ~WebSocketProtocol();

	/* From OpWebSocket */

	virtual OP_STATUS   Open(const uni_char* url, const URL& origin, const OpVector<OpString>& sub_protocols);
	virtual void        Close(unsigned short status, const uni_char* reason);
	/**< Close the websocket connection with the given status and reason.
	     A status value of 0 means include no status in the Close message,
	     and an empty reason string means no reason provided after the status
	     in the Close message. */

	virtual const uni_char *GetNegotiatedSubProtocol(){ return m_negotiated_sub_protocol.CStr(); }

	virtual void        CloseHard();

	virtual OP_STATUS   SendMessage(const uni_char* data, OpFileLength length, OpFileLength& buffered_amount);
	virtual OP_STATUS   SendMessage(const UINT8* data, OpFileLength length, OpFileLength& buffered_amount);
	virtual OP_STATUS   SendFile(const uni_char* filename, BOOL binary, OpFileLength& buffered_amount);

	virtual OP_STATUS   SendMessageSliceFromQueue();

	virtual UINT        CreateFrameProtocolData(UINT8* frame_protocol_data, UINT64 frame_length, Frame::FrameOperationCode frame_operation_code, BOOL finish);
	virtual void        MaskFrameData(UINT8* frame_data, UINT data_length);

	virtual OpFileLength GetBufferedAmount() { OP_ASSERT(m_buffered_amount >= 0); if (m_buffered_amount >= 0) return m_buffered_amount; else return 0; }
	virtual OpFileLength GetReceivedMessageBytes();
	virtual BOOL        HasMessages();

	virtual OP_STATUS   ReadMessage(uni_char* buffer, OpFileLength buffer_length, OpFileLength& read_len);
	virtual OP_STATUS   ReadMessage(UINT8* buffer, OpFileLength buffer_length, OpFileLength& read_len);
	virtual OP_STATUS   GetMessageFilePath(OpString &full_path);

	virtual OP_STATUS   GetURL(OpString &url);

	virtual void        SetCookies();

	/* From ProtocolComm */
	virtual void        Stop();

	virtual void        CloseConnection();	// Controlled shutdown
	virtual CommState   ConnectionEstablished() { return COMM_LOADING; }
	virtual void        RequestMoreData();
	virtual void        ProcessReceivedData();

	virtual unsigned int ReadData(char* buf, unsigned blen);
	virtual void         SendData(char* buf, unsigned blen);

	virtual OP_STATUS   SetCallbacks(MessageObject* master, MessageObject* parent);
	virtual void        HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual void        SetProgressInformation(ProgressState progress_level, unsigned long progress_info1=0, const void* progress_info2 = NULL);

	/* Helper functions */
	BOOL GetSecure() { return m_info.secure; }
	void SetConnElement(WebSocket_Connection* connection) { m_connection = connection; }
	void SetState(State state) { m_state = state; }
	void SetUrlLoadStatus(URLStatus status) { OpStatus::Ignore(m_target.SetAttribute(URL::KLoadStatus, status)); }

protected:

	WebSocketProtocol(OpWebSocketListener* listener, MessageHandler* mh)
		: OpWebSocket(listener), ProtocolComm(mh, NULL, NULL), m_master(NULL), m_connection(NULL), m_header(NULL)
		, m_buffered_amount(0), m_total_raw_buffered_amount(0)
		, m_handshake(NULL), m_handshake_len(0), m_targetHost(NULL), m_targetPort(0), m_proxy(NULL), m_proxyPort(0)
		, m_proxy_auth_id(0), m_autoProxyLoadHandler(NULL), m_auth_data(NULL), m_mh_list(NULL), m_current_message_position(0), m_current_disk_frame(NULL)
#ifdef SELFTEST
		, m_selftest_fake_connection(FALSE)
#endif // SELFTEST
	{
		op_memset(&m_info,0,sizeof(m_info));
	}

	// From AuthenticationInterface
	virtual void Authenticate(AuthElm* auth_elm);
	virtual void FailAuthenticate(int mode);
	virtual BOOL CheckSameAuthorization(AuthElm* auth_elm, BOOL proxy);
	virtual authdata_st* GetAuthData();
	virtual MsgHndlList* GetMessageHandlerList();

	// Helper functions
	void        GetPreAuthentication();
	void        BuildHandshakeL();
	void        SendHandshakeL();
	void        SendProxyHandshakeL();
	void        UpdateCookieL(Header_List& headers);
	OP_STATUS   ParseHandshake();
	OP_STATUS   ParseHeaders(char* buf, unsigned int len);
	Header*     GetHeader(const char* name) const;
	OP_STATUS   CreateChallenge(OpString8& challenge);
	OP_STATUS   CheckServerChallengeResponse(const char* challenge_response);
	OP_STATUS   PrepareFrameBuffer();
	OP_STATUS   ParseReceivedData();
	OP_STATUS   PrepareReadData();
	void        CheckBufferStatus(FrameBuffer*& read_buf, Frame* frame);
	OP_STATUS   SendCloseToServer(BOOL callback_clean, unsigned short callback_code, const uni_char* callback_reason, const UINT8* data_to_server, UINT length);
	void        CloseNow(BOOL clean, unsigned short code, const uni_char* reason);
	OP_STATUS   DetermineProxy();
	OP_STATUS   OpenInternal();
	void        DoConnect();
	OP_STATUS   CheckFrame(Frame *frame);

	/**
	 * If a prioritized (low level protocol control frames, like close, ping, etc)
	 * frame is received in the middle of a fragmented binary/text message, the
	 * control  message must be prioritized and handled before the fragmented
	 * message. */
	OP_STATUS 	HandlePrioritizedControlFramesAndCheckForCompleteMessages(BOOL& has_complete_messages);

	/*
	 * Parses and creates a list of the frames that exist in frame_buffer. */
	OP_STATUS 	FindFrames(FrameBuffer* frame_buffer);

	BOOL		ReceivedLegalCloseCode(Frame::FrameCloseCodes code);

	OP_STATUS   ParseCloseMessage(const UINT8* server_buffer, int server_buffer_length, Frame::FrameCloseCodes& code, OpString& reason);

	OP_STATUS   SendRawData(const UINT8* data, OpFileLength length, OpFileLength& bytesBuffered, Frame::FrameOperationCode data_type, BOOL finish);

	BOOL		MoreFrames(Frame::FrameOperationCode type);

	OP_STATUS   ReadRawFrame(UINT8* buffer, OpFileLength buffer_length, OpFileLength& read_len, Frame::FrameOperationCode type, BOOL& websocket_frame_finished);

	Frame *GetSpoolNextFrameToDisk(BOOL must_be_finished);

	OP_STATUS   BounceControlFrame(FrameBuffer* buffer, Frame* frame);

	void        HandleError(OP_STATUS status);

	WebSocket_Server_Manager* m_master;
	WebSocket_Connection* m_connection;
	FrameBuffer* 	m_header;

	HeaderList      m_cookie_headers;

	OpFileLength    m_buffered_amount; // Buffered amount of data, excluding framing overhead.
	OpString8       m_websocket_key;

	OpFileLength	m_total_raw_buffered_amount;
	URL             m_target;
	OpString8       m_origin;
	OpString	    m_negotiated_sub_protocol;

	AutoDeleteList<OutBuffer> m_send_buffers;

	AutoDeleteList<FrameBuffer>  m_recv_buffers;

	AutoDeleteList<Header>  m_headers;
	char*           m_handshake;
	UINT            m_handshake_len;
	ServerName*     m_targetHost;
	WORD            m_targetPort;
	ServerName*     m_proxy;
	unsigned short  m_proxyPort;
	OpString8       m_proxy_auth_str;
	unsigned long   m_proxy_auth_id;

	URL_LoadHandler* m_autoProxyLoadHandler;

	authdata_st*    m_auth_data;
	MsgHndlList*    m_mh_list;

	OpINT32Vector   m_negotiated_extensions;

	OpAutoString8HashTable<OpString8> m_sub_protocols;

	UINT8           m_current_mask_data[4];
	UINT			m_current_message_position;

	struct ClosingInfo
	{
		BOOL clean;
		unsigned short status_code;
		OpString reason;

		ClosingInfo() : clean(FALSE), status_code(0){};

	} m_closing_info;

	// This is needed for reporting amount buffered
	// excluding the protocol overhead.
	struct BufferedAmount
	{
		BufferedAmount(OpFileLength total_frame_length, OpFileLength frame_data_length, BOOL message_finished)
		: m_raw_frame_length(total_frame_length)
		, m_over_head(total_frame_length - frame_data_length)
		, m_raw_frame_buffered_amount(total_frame_length)
		, m_frame_data_buffered_amount(frame_data_length)
		, m_message_finished(message_finished)
		{}

		OpFileLength m_raw_frame_length;
		OpFileLength m_over_head;

		OpFileLength m_raw_frame_buffered_amount;
		OpFileLength m_frame_data_buffered_amount;

		BOOL m_message_finished;

	};

	OpAutoVector<BufferedAmount> m_buffered_amount_tracker;

	struct websocket_info_st
	{
		UINT spool_to_disk:1;
		UINT handshake_sent:1;
		UINT handshake_done:1;
		UINT secure:1;
		UINT proxy_handshake_sent:1;
		UINT proxy_connected:1;
		UINT close_signaled:1;
		UINT closing_signaled:1;
		UINT waiting_for_continuation_frame:1;
		UINT doing_proxy_auth:1;

		// If previous data frame was not sent with FIN code, this is the expected next type
		Frame::FrameOperationCode expected_continuation_frame_type;

	} m_info;

	Frame* m_current_disk_frame;

#ifdef SELFTEST
	BOOL m_selftest_fake_connection;
#endif
};

#endif // WEBSOCKET_PROTOCOL_H
