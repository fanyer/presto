/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MESSAGE_H
#define MESSAGE_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/header.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "adjunct/m2/src/glue/mime.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/url/url2.h"
#include "adjunct/m2/src/util/autodelete.h"

class OpQuote;
class Account;
class Upload_Base;

// ---------------------------------------------------------------------------

class Multipart : public Link
{
private:
    URL*        m_url;
    OpString8   m_charset;
    OpString    m_suggested_filename;
    BYTE*       m_data;
    int         m_size;
    BOOL        m_is_mailbody;
	URL_InUse	m_url_inuse;
	BOOL		m_is_visible;

public:
    Multipart();
    ~Multipart();

	OP_STATUS   SetData(URL* url, const OpStringC8& charset, const OpStringC& filename, int size, BYTE* data, BOOL is_mailbody=FALSE, BOOL is_visible = TRUE);
	void        DeleteDatabuffer() {OP_DELETEA(m_data); m_data=NULL;}

    URLContentType GetContentType() const {return m_url ? m_url->ContentType() : URL_UNDETERMINED_CONTENT;}
    OP_STATUS   GetContentType(OpString& contenttype) const;
    URL*        GetURL() const {return m_url;}
    OP_STATUS   GetURLFilename(OpString& filename) const;
    OP_STATUS   GetCharset(OpString8& charset) const {return charset.Set(m_charset);}
    OP_STATUS   GetSuggestedFilename(OpString& filename) const {return filename.Set(m_suggested_filename);}
    BYTE*       GetDataPtr() const {return m_data;}
    void        GetDataPtr(BYTE*& dataptr, int& size) const {OP_ASSERT(dataptr); dataptr = m_data; size = m_size;}
    OP_STATUS   GetBodyText(OpString16& body) const;
    int         GetSize() const {return m_size;}
    BOOL        IsMailbody() const {return m_is_mailbody;}
	BOOL		IsVisible() const { return m_is_visible; }
};

// ---------------------------------------------------------------------------

class Message : public Decoder::MessageListener, public MessageLoop::Target, public Autodeletable, public StoreMessage
{
public:
	static const int CopyableFlags = 1 << IS_READ
								   | 1 << IS_SEEN
								   | 1 << IS_OUTGOING
								   | 1 << IS_SENT
								   | 1 << HAS_ATTACHMENT
								   | 1 << HAS_IMAGE_ATTACHMENT
								   | 1 << HAS_AUDIO_ATTACHMENT
								   | 1 << HAS_VIDEO_ATTACHMENT
								   | 1 << HAS_ZIP_ATTACHMENT
								   | 1 << ATTACHMENT_FLAG_COUNT
								   | 1 << HAS_PRIORITY
								   | 1 << HAS_PRIORITY_LOW
								   | 1 << HAS_PRIORITY_MAX
								   ;

    enum TextpartSetting
    {
        PREFER_TEXT_PLAIN=0,
        PREFER_TEXT_HTML=1,
        FORCE_TEXT_PLAIN=2,
		DECIDED_BY_ACCOUNT=3, //Used in UI/preferences
		PREFER_TEXT_PLAIN_BUT_REPLY_WITH_SAME_FORMATTING = 4
    };

	enum MediaType
	{
		TYPE_UNKNOWN			= 0,
		TYPE_APPLICATION,
		TYPE_AUDIO,
		TYPE_IMAGE,
		TYPE_VIDEO
	};

	enum MediaSubtype
	{
		SUBTYPE_UNKNOWN			= 0,
		SUBTYPE_ARCHIVE,
		SUBTYPE_OGG
	};

    Message();
    ~Message();

	void Destroy();

    OP_STATUS Init(const UINT16 account_id);
    OP_STATUS Init(IN UINT16 account_id, IN message_gid_t old_message_id, IN MessageTypes::CreateType &type);
	OP_STATUS ReInit(UINT16 account_id);

// ---- Async convert from RFC822 to Url --------------
public:
	class RFC822ToUrlListener
	{
	public:
        virtual ~RFC822ToUrlListener() {}
		virtual void OnUrlCreated(URL* url) = 0;
		virtual void OnUrlDeleted(URL* url) = 0;
		virtual void OnRFC822ToUrlRestarted() = 0;
		virtual void OnRFC822ToUrlDone(OP_STATUS status) = 0;
		virtual void OnMessageBeingDeleted() = 0;
	};

	OP_STATUS AddRFC822ToUrlListener(RFC822ToUrlListener* rfc822_to_url_listener);
	OP_STATUS RemoveRFC822ToUrlListener(RFC822ToUrlListener* rfc822_to_url_listener);
	BOOL	  HasRFC822ToUrlListener() { OpListenersIterator it(m_async_rfc822_to_url_listeners); return m_async_rfc822_to_url_listeners.HasNext(it); }

	// From Target:
	OP_STATUS Receive(OpMessage message);

private:
	OpListeners<RFC822ToUrlListener> m_async_rfc822_to_url_listeners;
	URL*		 m_async_rfc822_to_url_url;
	URL_InUse	 m_async_rfc822_to_url_inuse;
	MessageLoop* m_async_rfc822_to_url_loop;

	int			 m_async_rfc822_to_url_raw_header_size; //Optimization - cache this value
	int			 m_async_rfc822_to_url_raw_body_size;	//Optimization - cache this value
	int			 m_async_rfc822_to_url_position;

	void	  AbortAsyncRFC822ToUrl(BOOL in_destructor=FALSE);
	OP_STATUS RestartAsyncRFC822ToUrl();
	OP_STATUS AsyncRFC822ToUrlBlock();

private:
    UINT16      m_account_id;
    OpString8     m_internetlocation;
    message_gid_t m_myself;
    message_gid_t m_parent;
    message_gid_t m_prev_sibling;
    message_gid_t m_next_sibling;
	message_gid_t m_first_child;
    UINT64      m_flag;
	time_t      m_recv_time;
	MessageTypes::CreateType m_create_type;

public:
    OP_STATUS GetInternetLocation(OpString8& location) const {return location.Set(m_internetlocation);}
	const OpStringC8& GetInternetLocation() const { return m_internetlocation; }
    OP_STATUS SetInternetLocation(const OpStringC8& location) {return m_internetlocation.Set(location);}

	time_t GetRecvTime() const { return m_recv_time; }
	void   SetRecvTime(time_t recv_time) { m_recv_time = recv_time; }

    UINT16    GetAccountId() const {return m_account_id;}
	OP_STATUS SetAccountId(const UINT16 account_id) { return SetAccountId(account_id, FALSE); }
    OP_STATUS SetAccountId(const UINT16 account_id, BOOL update_headers);
    Account*  GetAccountPtr(UINT16 account_id=0) const; //0 => use m_account_id. This pointer should be considered temporary. Don't hold on to it! (The account *may* be deleted without Message being told)

    message_gid_t GetId() const {return m_myself;}
    message_gid_t GetParentId() const {return m_parent;}
    message_gid_t GetPrevSiblingId() const {return m_prev_sibling;}
    message_gid_t GetNextSiblingId() const {return m_next_sibling;}
    message_gid_t GetFirstChildId() const {return m_first_child;}

    void      SetId(const message_gid_t myself) {m_myself=myself;}
    void      SetParentId(const message_gid_t parent) {m_parent=parent;}
    void      SetPrevSiblingId(const message_gid_t sibling) {m_prev_sibling=sibling;}
    void      SetNextSiblingId(const message_gid_t sibling) {m_next_sibling=sibling;}
    void      SetFirstChildId(const message_gid_t child) {m_first_child=child;}

    BOOL      IsFlagSet(Flags flag) const { UINT64 a = (m_flag & (static_cast<UINT64>(1) << flag)); return a ? 1 : 0; }
    void      SetFlag(const Flags flag, const BOOL value) {	m_flag=value?m_flag|(static_cast<UINT64>(1)<<flag):m_flag&~(static_cast<UINT64>(1)<<flag); }

	UINT32    GetPriority() const { return GetPriority(m_flag); }
	static UINT32 GetPriority(int flags);
	OP_STATUS SetPriority(UINT32 priority) { return SetPriority(priority, true); }

    // for use by Store:
    UINT64	  GetAllFlags() const { return m_flag; }
    void      SetAllFlags(const UINT64 flags) { m_flag = flags; }

	virtual bool IsConfirmedNotSpam() const { return m_not_spam; }
	virtual void SetConfirmedNotSpam(bool is_not_spam) { m_not_spam = is_not_spam; }

	virtual bool IsConfirmedSpam() const { return m_is_spam; }
	virtual void SetConfirmedSpam(bool is_spam) { m_is_spam = is_spam; }

	MessageTypes::CreateType  GetCreateType() const { return m_create_type; }
	void      SetCreateType(const MessageTypes::CreateType create_type) { m_create_type = create_type; }

	int		  GetPreferredStorage() const;

    OP_STATUS Send(BOOL anonymous);

    OP_STATUS GenerateAttributionLine(IN const Message* message, IN const MessageTypes::CreateType type, OUT OpString& attribution_line) const;

// Headers
private:
    Head* m_headerlist;
    char* m_rawheaders;

	OP_STATUS CopyHeaderValue(Header* source_header, Header* destination_header);
    OP_STATUS CopyHeaderValue(const Message* source, Header::HeaderType header_type) {return CopyHeaderValue(source, header_type, header_type);}
    OP_STATUS CopyHeaderValue(const Message* source, const OpStringC8& header_name) {return CopyHeaderValue(source, header_name, header_name);}
    OP_STATUS CopyHeaderValue(const Message* source, Header::HeaderType header_type_from, Header::HeaderType header_type_to);
    OP_STATUS CopyHeaderValue(const Message* source, const OpStringC8& header_name_from, const OpStringC8& header_name_to);

public:
    Header*   GetFirstHeader() const {return m_headerlist?(Header*)(m_headerlist->First()):NULL;}
	Header*   GetHeader(Header::HeaderType header_type) const { return GetHeader(header_type, 0); }
    Header*   GetHeader(Header::HeaderType header_type, const Header* start_search_after) const;
    Header*   GetHeader(const OpStringC8& header_name, const Header* start_search_after=NULL) const;

    const char* GetOriginalRawHeaders() const {return m_rawheaders;}
	int GetOriginalRawHeadersSize() const { return m_rawheaders ? op_strlen(m_rawheaders) : 0; }
    OP_STATUS GetCurrentRawHeaders(OpString8& buffer, BOOL& dropped_illegal_headers, BOOL append_CRLF=TRUE) const;
    OP_STATUS GetHeaderValue(Header::HeaderType header_type, OpString8& value, BOOL do_quote_pair_encode=FALSE) const;
    OP_STATUS GetHeaderValue(const OpStringC8& header_name, OpString8& value, BOOL do_quote_pair_encode=FALSE) const;
	OP_STATUS GetHeaderValue(Header::HeaderType header_type, Header::HeaderValue& value) const { return GetHeaderValue(header_type, value, FALSE); }
	OP_STATUS GetHeaderValue(Header::HeaderType header_type, Header::HeaderValue& value, BOOL do_quote_pair_encode) const;
	OP_STATUS GetHeaderValue(const OpStringC8& header_name, Header::HeaderValue& value, BOOL do_quote_pair_encode=FALSE) const;
    OP_STATUS GetDateHeaderValue(Header::HeaderType header_type, time_t& time_utc) const;
    OP_STATUS GetDateHeaderValue(const OpStringC8& header_name, time_t& time_utc) const;

	/** Returns ERR if none, OK if found one
	 */
	static OP_STATUS GetMailingListHeader(const StoreMessage &message, Header::HeaderValue& value);


	/** Get a 'from' string for display purposes (e.g. in list of messages)
	  */
	const uni_char* GetNiceFrom() const;

    OP_STATUS SetRawHeaders(const char* buffer) {return SetRawMessage(buffer);}
	OP_STATUS PrepareRawMessageContent() { return CopyCurrentToOriginalHeaders(); }
	OP_STATUS CopyCurrentToOriginalHeaders(BOOL allow_skipping_illegal_headers = TRUE);
	OP_STATUS SetHeaderValue(Header::HeaderType header_type, const OpStringC8& value) { return SetHeaderValue(header_type, value, FALSE, 0, 0); }
    OP_STATUS SetHeaderValue(Header::HeaderType header_type, const OpStringC8& value, BOOL do_quote_pair_decode, int* parseerror_start=NULL, int* parseerror_end=NULL);
    OP_STATUS SetHeaderValue(const OpStringC8& header_name, const OpStringC8& value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
	OP_STATUS SetHeaderValue(Header::HeaderType header_type, const OpStringC16& value) { return SetHeaderValue(header_type, value, FALSE, 0, 0); }
    OP_STATUS SetHeaderValue(Header::HeaderType header_type, const OpStringC16& value, BOOL do_quote_pair_decode, int* parseerror_start=NULL, int* parseerror_end=NULL);
    OP_STATUS SetHeaderValue(const OpStringC8& header_name, const OpStringC16& value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
    OP_STATUS SetHeaderValue(Header::HeaderType header_type, UINT32 value);
	OP_STATUS SetHeadersFromMimeHeaders(const OpStringC8& headers);
    OP_STATUS SetDateHeaderValue(Header::HeaderType header_type, time_t time_utc=0); // 0 = ::Now
    OP_STATUS SetDateHeaderValue(const OpStringC8& header_name, time_t time_utc=0); // 0 = ::Now

	//Need to handle Resent-headers
	OP_STATUS GetBcc(Header::HeaderValue& bcc, BOOL do_quote_pair_encode=FALSE) {return GetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTBCC : Header::BCC, bcc, do_quote_pair_encode);}
	OP_STATUS GetCc(Header::HeaderValue& cc, BOOL do_quote_pair_encode=FALSE) {return GetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTCC : Header::CC, cc, do_quote_pair_encode);}
	OP_STATUS GetDate(time_t& time_utc) {return GetDateHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTDATE : Header::DATE, time_utc);}
	OP_STATUS GetFrom(Header::HeaderValue& from, BOOL do_quote_pair_encode=FALSE) {return GetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTFROM : Header::FROM, from, do_quote_pair_encode);}
	OP_STATUS GetFromAddress(OpString& address, BOOL do_quote_pair_encode=FALSE);
	virtual OP_STATUS GetMessageId(OpString8& message_id) const;
	OP_STATUS GetMessageId(Header::HeaderValue& message_id, BOOL do_quote_pair_encode=FALSE) const {return GetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTMESSAGEID : Header::MESSAGEID, message_id, do_quote_pair_encode);}
	OP_STATUS GetReplyTo(Header::HeaderValue& reply_to, BOOL do_quote_pair_encode=FALSE) {return GetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTREPLYTO : Header::REPLYTO, reply_to, do_quote_pair_encode);}
	OP_STATUS GetTo(Header::HeaderValue& to, BOOL do_quote_pair_encode=FALSE) {return GetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTTO : Header::TO, to, do_quote_pair_encode);}

	OP_STATUS SetBcc(const OpString& bcc, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL) {return SetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTBCC : Header::BCC, bcc, do_quote_pair_decode, parseerror_start, parseerror_end);}
	OP_STATUS SetCc(const OpString& cc, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL) {return SetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTCC : Header::CC, cc, do_quote_pair_decode, parseerror_start, parseerror_end);}
	OP_STATUS SetDate(time_t time_utc) {return SetDateHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTDATE : Header::DATE, time_utc);}
	OP_STATUS SetFrom(const OpString& from, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL) {return SetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTFROM : Header::FROM, from, do_quote_pair_decode, parseerror_start, parseerror_end);}
	OP_STATUS SetMessageId(const OpString& message_id, BOOL do_quote_pair_decode=FALSE) {return SetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTMESSAGEID : Header::MESSAGEID, message_id, do_quote_pair_decode);}
	OP_STATUS SetReplyTo(const OpString& reply_to, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL) {return SetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTREPLYTO : Header::REPLYTO, reply_to, do_quote_pair_decode, parseerror_start, parseerror_end);}
	OP_STATUS SetTo(const OpString& to, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL) {return SetHeaderValue(IsFlagSet(Message::IS_RESENT) ? Header::RESENTTO : Header::TO, to, do_quote_pair_decode, parseerror_start, parseerror_end);}

    OP_STATUS RemoveDuplicateRecipients(BOOL remove_self=TRUE, const OpStringC& email_to_keep=NULL /*Quick hack. Should eventually be made into a list*/);

    void      ClearHeaderList();
	void	  RemoveXOperaStatusHeader();
	void	  RemoveBccHeader();

	/** Set attachment flags for this message depending on type of parts, checks for known attachment
	  * @param type Main type
	  * @param subtype Subtype
	  * @return Whether this was a known attachment and flags were set
	  */
	BOOL SetAttachmentFlags(MediaType type, MediaSubtype subtype);

private:
    OP_STATUS AddHeader(Header::HeaderType header_type, const OpStringC8& value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL); //Will be called from SetHeaderValue when needed
    OP_STATUS AddHeader(const OpStringC8& header_name, const OpStringC8& value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
    OP_STATUS AddHeader(Header::HeaderType header_type, const OpStringC16& value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
    OP_STATUS AddHeader(const OpStringC8& header_name, const OpStringC16& value, BOOL do_quote_pair_decode=FALSE, int* parseerror_start=NULL, int* parseerror_end=NULL);
    OP_STATUS RemoveHeader(Header::HeaderType header_type);
    OP_STATUS RemoveHeader(const OpStringC8& header_name);
	OP_STATUS SetPriority(UINT32 priority, bool add_header);

// Multipart
private:
    enum MimeStatus
    {
        MIME_NOT_DECODED,
        MIME_DECODING_HEADERS,
        MIME_DECODED_HEADERS,
        MIME_DECODING_ALL,
        MIME_DECODED_ALL,
		MIME_DECODING_BODY,
		MIME_DECODED_BODY
    };

    Head*      m_multipart_list;
    MimeStatus m_multipart_status;
    Decoder*   m_mime_decoder;

	// the old message is kept alive to keep it's m_mime_decoder alive
	// the m_mime_decoder contains all the URL's that need to be kept alive
	// if you delete the decoder, you remove the context, and then you can't access any of the inlined images, etc
	Message*   m_old_message;

	OpString   m_quick_attachment_list;

public:
    OP_STATUS ConvertMultipartToAttachment();
    OP_STATUS MimeDecodeMessage(BOOL ignore_content=FALSE, BOOL body_only = FALSE, BOOL prefer_html = FALSE);
	void      PurgeMultipartData(Multipart* item_to_keep = NULL);
	void 	  DeleteMultipartData(Multipart* item_to_delete);
	OP_STATUS OnDecodedMultipart(URL* url, const OpStringC8& charset, const OpStringC& filename, int size, BYTE* data, BOOL is_mailbody, BOOL is_visible);
    void      OnFinishedDecodingMultiparts(OP_STATUS status);
	void	  OnRestartDecoding();

    Head*     GetMultipartListPtr(BOOL ignore_content=FALSE, BOOL prefer_html = FALSE); //This needs to be in a listener when code is async

	OP_STATUS QuickMimeDecode();

// Body
private:
    char*           m_rawbody;
	char*			m_in_place_body;
	size_t			m_in_place_body_offset;
	UINT32        	m_realmessagesize;		// Size of the message on the server
	UINT32			m_localmessagesize;		// Size of the part of the message we have in memory locally (m_rawheaders + m_rawbody)
    OpString8       m_charset;
    TextpartSetting m_textpart_setting;		// whether the user prefers to view this as a HTML message or not
	bool			m_force_charset;
	bool			m_is_HTML;				// whether the user edited message is a HTML message or not
	bool			m_not_spam;				// whether it's flagged as $NotSpam on the IMAP server
	bool			m_is_spam;				// whether it's flagged as $Spam or in the Spam folder on the IMAP server
	UINT32			m_context_id;			// URL context id for inlined attachments

public:
	OP_STATUS   SetRawMessage(const char* buffer, BOOL force_content=FALSE, BOOL strip_body=FALSE /*some IMAP servers send a linefeed body, when only headers are requested*/, char* dont_copy_buffer = NULL, int dont_copy_buffer_length = 0);
	OP_STATUS	GetRawMessage(OpString8& buffer) const { return GetRawMessage(buffer, TRUE, TRUE, FALSE); }
    OP_STATUS   GetRawMessage(OpString8& buffer, BOOL allow_skipping_illegal_headers, BOOL include_bcc, BOOL convert_to_mbox_format = FALSE) const;

	OP_STATUS	IsBodyCharsetCompatible(const OpStringC16& body, int& invalid_count, OpString& invalid_chars);
    OP_STATUS   SetRawBody(const char* buffer, BOOL update_contenttype, char* dont_copy_buffer = NULL);
    OP_STATUS   SetRawBody(const OpStringC16& buffer, BOOL reflow=TRUE, BOOL buffer_is_useredited=FALSE, BOOL force_content=FALSE);
    const char* GetRawBody() const { return m_rawbody; }
	int			GetRawBodySize() const { return m_localmessagesize - GetOriginalRawHeadersSize() - (m_rawheaders ? 2 : 0); }
    OP_STATUS   GetRawBody(OpString16& body) const;
	OP_STATUS	GetBodyItem(Multipart** body_item, BOOL prefer_html = FALSE);
	OP_STATUS	QuickGetBodyText(OpString16& body) const;
	OP_STATUS   GetBodyText(OpString16& text, BOOL unwrap = FALSE, bool* prefer_html = NULL);

	OP_STATUS	SetPreferredRecipients(const MessageTypes::CreateType reply_type, StoreMessage& old_message);

	bool		IsHTML() const {return m_is_HTML;}
	UINT32		GetContextId() const { return m_context_id; }
	void		SetContextId( UINT32 context_id ) { m_context_id = context_id; }
	void		SetHTML(bool is_HTML) {m_is_HTML = is_HTML;}

    long        BodyLength() const {return m_rawbody?strlen(m_rawbody):0;}

    void        SetMessageSize(const UINT32 size) {m_realmessagesize = size;}
    UINT32		GetMessageSize() const {return m_realmessagesize;}
	UINT32		GetLocalMessageSize() const {return m_localmessagesize;}
	void		SetTextDirection(int direction);
	int 		GetTextDirection() const;

	TextpartSetting GetTextPartSetting() const {return m_textpart_setting;}
	OP_STATUS   SetTextPartSetting(TextpartSetting textpart_setting) {m_textpart_setting = textpart_setting; return OpStatus::OK;} //Will probably need to refresh mail-view

    OP_STATUS   SetCharset(const OpStringC8& charset, BOOL force=FALSE); /*Force will overrule the m_force_charset setting. Use with care!*/
	OP_STATUS	UpdateCharsetFromContentType();
    OP_STATUS   GetCharset(OpString8& charset) const {return charset.Set(m_charset);}
	bool		GetForceCharset() const {return m_force_charset;}
	void		SetForceCharset(bool force_charset) {m_force_charset = force_charset;}

    ///An empty charset will use the charset from account
    OP_STATUS   SetContentType(const OpString8& charset);

    OP_STATUS   GenerateInReplyToHeader();

	OpQuote*	CreateQuoteObject(unsigned max_quote_depth = 32) const;
    BOOL        IsFlowed(BOOL& delsp) const;

    OP_STATUS   MimeEncodeAttachments(BOOL remove_bcc, BOOL allow_8bit);
	// Check whether the transfer encoding is 8bit or 7bit. if the parameter rawbody is NULL then we will use m_rawbody to check.
	BOOL		Is7BitBody(char * rawbody = NULL) const;

private:
	size_t      SkipNewline(const char* string) const;
    OP_STATUS   RemoveMimeHeaders();
    OP_STATUS   CreateUploadElement(OUT Upload_Base** element, BOOL remove_bcc/*=TRUE*/, BOOL allow_8bit);
	void		RemoveRawMessage(BOOL remove_body = TRUE, BOOL remove_header = TRUE);

	OP_STATUS	QuoteHTMLBody(Multipart* body_item, OpString& quoted_body);

public:

    class Observer
    {
    public:
        virtual ~Observer() {}
        virtual OP_STATUS Changed(Message* message) = 0;
    };

    OP_STATUS AddObserver    (Observer* observer);
    OP_STATUS RemoveObserver (Observer* observer);

private:
    Message(const Message&);
    Message& operator =(const Message&);

    class Attachment : public Link
    {
    public:
        URL*			m_attachment_url;
		URL_InUse		m_attachment_url_inuse;
        OpString		m_attachment_filename;
        OpString		m_suggested_filename;
		BOOL			m_is_inlined;

    public:
        Attachment();
        ~Attachment();

        OP_STATUS Init(const OpStringC& suggested_filename, const OpStringC& attachment_filename, URL* attachment_url=NULL, BOOL is_inlined = FALSE);
#if !defined __SUPPORT_OPENPGP__
        OP_STATUS CreateUploadElement(Upload_Base** element, const OpStringC8& charset) const;
#else
        OP_STATUS UpdateUploadBuilder(OUT Upload_Builder_Base *builder) const;
#endif

    private:
        Attachment(const Attachment&);
        Attachment& operator =(const Attachment&);

    public:
        bool operator==(const OpStringC& filename) {return m_attachment_filename.Compare(filename)==0;}
    };

private:
    Head* m_attachment_list;

public:
    OP_STATUS AddAttachment(const OpStringC& suggested_filename, const OpStringC& attachment_filename, URL* attachment_url=NULL, BOOL is_inlined = FALSE);
    OP_STATUS AddAttachment(const OpStringC& filename);

    void      RemoveAttachment(const OpStringC& filename);
    void      RemoveAllAttachments();

    int       GetAttachmentCount() const;
    URL*      GetAttachmentURL(int index) const;
    OP_STATUS GetAttachmentFilename(int index, OpString& filename) const;
    OP_STATUS GetAttachmentSuggestedFilename(int index, OpString& filename) const;
	BOOL	  IsInlinedAttachment(int index) const { return GetAttachment(index)->m_is_inlined; }

    Message::Attachment* GetAttachment(const OpStringC& filename) const;
private:
    Message::Attachment* GetAttachment(int index) const;
};

// ---------------------------------------------------------------------------

/*! A class that enables you to create messages of type multipart/alternative
	without too much hassle in the Message class. This class will handle the
	details and return a Message pointer once you are done adding alternative
	parts.
*/

class MultipartAlternativeMessageCreator
{
public:
	// Constructor / destructor.
	MultipartAlternativeMessageCreator();
	~MultipartAlternativeMessageCreator();

	OP_STATUS Init();

	// Methods.
	OP_STATUS AddMultipart(Upload_Base* element);
	OP_STATUS AddMultipart(const OpStringC8& content_type, const OpStringC16& data);

	Message* CreateMessage(UINT16 account_id) const;

private:
	// No copy or assignment.
	MultipartAlternativeMessageCreator(const MultipartAlternativeMessageCreator& other);
	MultipartAlternativeMessageCreator& operator=(const MultipartAlternativeMessageCreator& rhs);

	// Members.
	OpAutoPtr<Upload_Multipart> m_upload_multipart;
};

#endif //M2_SUPPORT

#endif // MESSAGE_H
